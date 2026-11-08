#!/usr/bin/env python3
# doomo-generator
"""partcat_build.py - compile the creator's part catalog (CR1).

Reads core/data/parts/catalog/ and writes

    core/gen/part_meta_data.c   names, slots, sets, tags, age, method, state, cost, hides and a
                                sorted name index (creator side: the PS1 links it into the creator
                                overlay with src/part_meta.c)
    core/gen/part_age_data.c    the parts whose age class is not ALL, sorted (resident: the renderer,
                                the generators and babies read it through src/part_age.c)

    python3 tools/partcat_build.py [--out-root DIR]
    python3 tools/partcat_build.py --freeze      append every live part not yet in
                                                 core/tests/golden/part_ids.txt (after a sign-off)
    python3 tools/partcat_build.py --selftest    the seeded rejects below

FILES (one `#` comment per line allowed):

    tags.txt     tag KEY "Name"                          bit = line order, at most 32
    hides.txt    hide KEY                                bit = line order, at most 16
    sets.txt     set KEY "Name" theme=TAG [reward=yes] ["blurb"]   id = line order
    costs.txt    SLOT VALUE COST                         draw cost weights (the unit test measures
                                                         them: build/tests --update-golden
                                                         part_costs_match_the_renderer)
    planned.txt  plan KEY kind=gene|deep target=N method=M name="..."   (tools/part_budget.py)
    <slot>.txt   one slot:
        slot KEY list=FIELD|deep=DF_X name="Name" method=P|S|F|T|C [target=N] [shapes=a,b,..]
        VALUE "Name" set=SET tags=T1,T2 [age=all|teen|adult] [method=M] [shape=S]
                     [hides=H1,H2] [state=live|hidden|planned|test]

    list=FIELD is a gene field of core/data/look/fields.txt (the part list id is its GF_* id) or
    `species` (PL_SPECIES); deep=DF_X a deep field of include/doomo/look_fields.h (PL_DEEP0 + DF_X).

STATES: live = selectable today; hidden = painted, not selectable (the list is not flipped yet);
planned = named, not painted; test = a VM demo value, never selectable.

REJECTED: an unknown set, tag, hide, shape, field, age, method or state; a value that does not fit
the gene field's width (deep fields: 1..127); a name that repeats inside a slot; a part without a set
or a tag; a method=S part without a part script (core/data/parts/<dir>/); a part script without a
catalog line; a golden (shipped) id whose name changed or that is no longer live.
Deterministic, byte-identical output on every machine.
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
CAT = os.path.join(ROOT, "core", "data", "parts", "catalog")
FIELDS = os.path.join(ROOT, "core", "data", "look", "fields.txt")
LOOKF = os.path.join(ROOT, "core", "include", "doomo", "look_fields.h")
PARTS = os.path.join(ROOT, "core", "data", "parts")
GOLDEN = os.path.join(ROOT, "core", "tests", "golden", "part_ids.txt")

PL_SPECIES = 127
PL_DEEP0 = 128
AGES = ["all", "teen", "adult"]
STATES = ["live", "hidden", "planned", "test"]
METHODS = ["P", "S", "F", "T", "C"]
META_FILES = ("tags.txt", "hides.txt", "sets.txt", "costs.txt", "planned.txt")


class CatError(Exception):
    pass


def toks(line):
    """split on blanks, keeping "quoted strings" (without the quotes) as one token"""
    out = []
    for m in re.finditer(r'(\w+)="([^"]*)"|"([^"]*)"|(\S+)', line):
        if m.group(1) is not None:
            out.append(("w", "%s=%s" % (m.group(1), m.group(2))))
        elif m.group(3) is not None:
            out.append(("q", m.group(3)))
        else:
            out.append(("w", m.group(4)))
    return out


def lines_of(path):
    """(line number, text) of every non-empty line, a `#` outside "quotes" starting a comment"""
    for ln, raw in enumerate(open(path, encoding="utf-8"), 1):
        q, cut = False, len(raw)
        for i, ch in enumerate(raw):
            if ch == '"':
                q = not q
            elif ch == "#" and not q:
                cut = i
                break
        line = raw[:cut].strip()
        if line:
            yield ln, line


def kv(tokens, where):
    d = {}
    for kind, t in tokens:
        if kind == "w" and "=" in t:
            k, v = t.split("=", 1)
            if v.startswith('"'):
                v = v.strip('"')
            d[k] = v
        else:
            raise CatError("%s: unexpected %r" % (where, t))
    return d


def gene_fields():
    ids, bits = {}, {}
    n = 0
    for line in open(FIELDS):
        line = line.split("#", 1)[0].split()
        if line:
            ids[line[0]] = n
            bits[line[0]] = int(line[1])
            n += 1
    return ids, bits


def deep_fields():
    text = open(LOOKF).read()
    m = re.search(r"enum\s*\{([^}]*DF_COUNT[^}]*)\}", text)
    names = [x.strip() for x in m.group(1).replace("\n", " ").split(",")]
    ids = {}
    for i, n in enumerate(x for x in names if x):
        if n == "DF_COUNT":
            break
        ids[n] = i
    return ids


def script_entries(gf):
    """{(list id, value): dir} of every part script (tools/parts_build.py sources)."""
    out = {}
    for d in sorted(os.listdir(PARTS)):
        p = os.path.join(PARTS, d)
        if d == "catalog" or not os.path.isdir(p):
            continue
        for f in sorted(os.listdir(p)):
            if not f.endswith(".txt"):
                continue
            for raw in open(os.path.join(p, f)):
                t = raw.split("#", 1)[0].split()
                if len(t) == 3 and t[0] == "list":
                    if t[1].startswith("DF_"):
                        lid = PL_DEEP0 + deep_fields()[t[1]]
                    else:
                        lid = gf[t[1]]
                    out[(lid, int(t[2]))] = d
    return out


def read_catalog():
    gf, gbits = gene_fields()
    df = deep_fields()
    tags, hides, sets = [], [], []
    for ln, line in lines_of(os.path.join(CAT, "tags.txt")):
        t = toks(line)
        if len(t) != 3 or t[0][1] != "tag":
            raise CatError("tags.txt:%d: tag KEY \"Name\"" % ln)
        tags.append((t[1][1], t[2][1]))
    for ln, line in lines_of(os.path.join(CAT, "hides.txt")):
        t = line.split()
        if len(t) != 2 or t[0] != "hide":
            raise CatError("hides.txt:%d: hide KEY" % ln)
        hides.append(t[1])
    if len(tags) > 32 or len(hides) > 16:
        raise CatError("at most 32 tags and 16 hides")
    tagk = [k for k, _ in tags]
    for ln, line in lines_of(os.path.join(CAT, "sets.txt")):
        t = toks(line)
        w = "sets.txt:%d" % ln
        if len(t) < 3 or t[0][1] != "set" or t[2][0] != "q":
            raise CatError("%s: set KEY \"Name\" theme=TAG [reward=yes] [\"blurb\"]" % w)
        rest = t[3:]
        blurb = ""
        if rest and rest[-1][0] == "q":
            blurb = rest[-1][1]
            rest = rest[:-1]
        a = kv(rest, w)
        if a.get("theme") not in tagk:
            raise CatError("%s: unknown theme tag %r" % (w, a.get("theme")))
        sets.append({"key": t[1][1], "name": t[2][1], "theme": tagk.index(a["theme"]),
                     "reward": a.get("reward") == "yes", "blurb": blurb})
    setk = [s["key"] for s in sets]
    if len(set(setk)) != len(setk) or len(sets) > 255:
        raise CatError("sets.txt: a set key repeats (or over 255 sets)")

    slots = []
    for f in sorted(os.listdir(CAT)):
        if not f.endswith(".txt") or f in META_FILES:
            continue
        slot = None
        for ln, line in lines_of(os.path.join(CAT, f)):
            w = "%s:%d" % (f, ln)
            t = toks(line)
            if t[0][1] == "slot":
                if slot is not None:
                    raise CatError("%s: one slot per file" % w)
                a = kv(t[2:], w)
                key = t[1][1]
                if "list" in a:
                    if a["list"] == "species":
                        lid, width = PL_SPECIES, 256
                    elif a["list"] in gf:
                        lid, width = gf[a["list"]], 1 << gbits[a["list"]]
                    else:
                        raise CatError("%s: unknown gene field %r" % (w, a["list"]))
                elif "deep" in a:
                    if a["deep"] not in df:
                        raise CatError("%s: unknown deep field %r" % (w, a["deep"]))
                    lid, width = PL_DEEP0 + df[a["deep"]], 128
                else:
                    raise CatError("%s: slot needs list= or deep=" % w)
                if a.get("method", "P") not in METHODS:
                    raise CatError("%s: unknown method" % w)
                shapes = a["shapes"].split(",") if "shapes" in a else []
                slot = {"key": key, "file": f, "list": lid, "width": width, "name": a.get("name", key),
                        "method": a.get("method", "P"), "target": int(a.get("target", "0")),
                        "shapes": shapes, "parts": []}
                continue
            if slot is None:
                raise CatError("%s: a part before the slot line" % w)
            if t[0][0] != "w" or not t[0][1].isdigit() or len(t) < 2 or t[1][0] != "q":
                raise CatError("%s: VALUE \"Name\" set=.. tags=.." % w)
            v = int(t[0][1])
            a = kv(t[2:], w)
            lim = 127 if slot["list"] >= PL_DEEP0 else slot["width"] - 1
            if a.get("state") == "test":
                lim = 255                               # VM demo values may sit outside the field
            if v > lim or (slot["list"] >= PL_DEEP0 and v < 0):
                raise CatError("%s: value %d does not fit the field (max %d)" % (w, v, lim))
            if "set" not in a or a["set"] not in setk:
                raise CatError("%s: unknown or missing set %r" % (w, a.get("set")))
            tg = a.get("tags", "")
            if not tg:
                raise CatError("%s: a part needs at least one tag" % w)
            mask = 0
            for x in tg.split(","):
                if x not in tagk:
                    raise CatError("%s: unknown tag %r" % (w, x))
                mask |= 1 << tagk.index(x)
            hm = 0
            for x in (a["hides"].split(",") if "hides" in a else []):
                if x not in hides:
                    raise CatError("%s: unknown hide %r" % (w, x))
                hm |= 1 << hides.index(x)
            age = a.get("age", "all")
            state = a.get("state", "live")
            meth = a.get("method", slot["method"])
            if age not in AGES or state not in STATES or meth not in METHODS:
                raise CatError("%s: bad age, state or method" % w)
            shape = 0xFF
            if "shape" in a:
                if a["shape"] not in slot["shapes"]:
                    raise CatError("%s: shape %r is not in the slot's shapes" % (w, a["shape"]))
                shape = slot["shapes"].index(a["shape"])
            for k in a:
                if k not in ("set", "tags", "hides", "age", "state", "method", "shape"):
                    raise CatError("%s: unknown key %r" % (w, k))
            name = t[1][1]
            if not name or len(name) > 40 or any(ord(c) < 32 or ord(c) > 126 for c in name):
                raise CatError("%s: a name is 1..40 printable ASCII characters" % w)
            for p in slot["parts"]:
                if p["value"] == v:
                    raise CatError("%s: value %d repeats" % (w, v))
                if p["name"].lower() == name.lower():
                    raise CatError("%s: name %r repeats in slot %s" % (w, name, slot["key"]))
            slot["parts"].append({"value": v, "name": name, "set": setk.index(a["set"]), "tags": mask,
                                  "hides": hm, "age": AGES.index(age), "state": STATES.index(state),
                                  "method": METHODS.index(meth), "shape": shape})
        if slot is None:
            raise CatError("%s: no slot line" % f)
        slot["parts"].sort(key=lambda p: p["value"])
        slots.append(slot)
    slots.sort(key=lambda s: s["list"])
    lids = [s["list"] for s in slots]
    if len(set(lids)) != len(lids):
        raise CatError("two slot files name the same field")
    keys = [s["key"] for s in slots]
    if len(set(keys)) != len(keys):
        raise CatError("two slots share a key")

    costs = {}
    cp = os.path.join(CAT, "costs.txt")
    if os.path.exists(cp):
        for ln, line in lines_of(cp):
            t = line.split()
            costs[(t[0], int(t[1]))] = min(255, int(t[2]))

    # scripts <-> catalog
    scripts = script_entries(gf)
    byl = {s["list"]: s for s in slots}
    for (lid, v), d in sorted(scripts.items()):
        s = byl.get(lid)
        if not s or not any(p["value"] == v for p in s["parts"]):
            raise CatError("part script %s: list %d value %d has no catalog line" % (d, lid, v))
    for s in slots:
        for p in s["parts"]:
            if METHODS[p["method"]] == "S" and (s["list"], p["value"]) not in scripts:
                raise CatError("%s: %s value %d is method=S but has no part script" % (s["file"], s["key"], p["value"]))
            p["cost"] = costs.get((s["key"], p["value"]), 0)
            p["scripted"] = (s["list"], p["value"]) in scripts
    return {"tags": tags, "hides": hides, "sets": sets, "slots": slots}


def golden_check(cat, path=GOLDEN):
    if not os.path.exists(path):
        return
    by = {s["key"]: s for s in cat["slots"]}
    for ln, line in lines_of(path):
        m = re.match(r'(\S+)\s+(\d+)\s+"(.*)"$', line)
        if not m:
            raise CatError("part_ids.txt:%d: SLOT VALUE \"Name\"" % ln)
        s = by.get(m.group(1))
        p = [x for x in (s["parts"] if s else []) if x["value"] == int(m.group(2))]
        if not p:
            raise CatError("shipped part %s %s \"%s\" was removed (ids are append-only)" % m.groups())
        if p[0]["name"] != m.group(3) or STATES[p[0]["state"]] != "live":
            raise CatError("shipped part %s %s \"%s\" changed name or is no longer live" % m.groups())


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit(cat):
    tags, hides, sets, slots = cat["tags"], cat["hides"], cat["sets"], cat["slots"]
    names = bytearray()
    nofs = {}

    def name_ofs(s):
        if s not in nofs:
            nofs[s] = len(names)
            names.extend(s.encode("ascii") + b"\0")
        return nofs[s]

    parts = []
    srows = []
    for si, s in enumerate(slots):
        first = len(parts)
        for p in s["parts"]:
            p["slot"] = si
            p["nofs"] = name_ofs(p["name"])
            parts.append((s, p))
        live = sum(1 for p in s["parts"] if p["state"] == 0)
        srows.append((s, first, len(parts) - first, live))
    if len(names) >= 65536 or len(parts) >= 65536:
        raise CatError("the catalog is over 64 KB of names or 65535 parts")
    order = sorted(range(len(parts)), key=lambda i: (parts[i][1]["name"].lower(), parts[i][0]["list"], parts[i][1]["value"]))

    L = ["/* part_meta_data.c - GENERATED by tools/partcat_build.py from core/data/parts/catalog/.",
         " * DO NOT EDIT: edit the catalog and run `python3 tools/partcat_build.py`. */",
         '#include "doomo/part_meta.h"', ""]
    L.append("const char doomo_part_names[PART_NAME_BYTES] =")
    chunk = names.decode("ascii").split("\0")[:-1]
    L += ['    %s"\\0"' % cstr(c) for c in chunk] or ['    ""']
    L[-1] += ";"
    L.append("")
    for si, s in enumerate(slots):
        if s["shapes"]:
            L.append("static const char *const shapes_%d[%d] = { %s };" % (si, len(s["shapes"]), ", ".join(cstr(x) for x in s["shapes"])))
    L.append("const PartSlotInfo doomo_part_slots[PART_SLOT_COUNT] = {")
    for si, (s, first, n, live) in enumerate(srows):
        L.append("    { %s, %s, %d, %d, %d, %d, %d, %d, %d, %d, %s }," % (
            cstr(s["key"]), cstr(s["name"]), s["list"], first, n, min(s["width"], 256) - 1 if s["list"] < PL_DEEP0 else 127,
            live, METHODS.index(s["method"]), s["target"], len(s["shapes"]),
            ("shapes_%d" % si) if s["shapes"] else "0"))
    L.append("};")
    L.append("")
    L.append("const PartMeta doomo_part_meta[PART_META_COUNT] = {")
    for s, p in parts:
        fl = p["age"] | (p["state"] << 2) | (p["method"] << 4) | ((1 << 7) if p["scripted"] else 0)
        L.append("    { %d, %d, %d, %d, 0x%02X, 0x%04X, 0x%08Xu, %d, %d, %d }," % (
            s["list"], p["value"], p["set"], p["slot"], fl, p["hides"], p["tags"], p["nofs"],
            p["cost"], p["shape"]))
    L.append("};")
    L.append("")
    L.append("const uint16_t doomo_part_name_order[PART_META_COUNT] = {")
    for i in range(0, len(order), 16):
        L.append("    " + ", ".join(str(x) for x in order[i:i + 16]) + ",")
    L.append("};")
    L.append("")
    L.append("const PartSetInfo doomo_part_sets[PART_SET_COUNT] = {")
    for st in sets:
        L.append("    { %s, %s, %s, %d, %d }," % (cstr(st["key"]), cstr(st["name"]), cstr(st["blurb"]),
                                                  st["theme"], 1 if st["reward"] else 0))
    L.append("};")
    L.append("")
    L.append("const char *const doomo_part_tag_keys[PART_TAG_COUNT] = { %s };" % ", ".join(cstr(k) for k, _ in tags))
    L.append("const char *const doomo_part_tag_names[PART_TAG_COUNT] = { %s };" % ", ".join(cstr(n) for _, n in tags))
    L.append("const char *const doomo_part_hide_keys[PART_HIDE_COUNT] = { %s };" % ", ".join(cstr(h) for h in hides))
    L.append("")
    meta = "\n".join(L)

    hdr = ["/* part_meta_ids.h - GENERATED by tools/partcat_build.py (CR1): table sizes. DO NOT EDIT. */",
           "#ifndef DOOMO_PART_META_IDS_H", "#define DOOMO_PART_META_IDS_H",
           "#define PART_SLOT_COUNT %d" % len(slots), "#define PART_META_COUNT %d" % len(parts),
           "#define PART_SET_COUNT %d" % len(sets), "#define PART_TAG_COUNT %d" % len(tags),
           "#define PART_HIDE_COUNT %d" % len(hides), "#define PART_NAME_BYTES %d" % (len(names) + 1)]
    for si, s in enumerate(slots):
        hdr.append("#define PSLOT_%s %d" % (s["key"].upper(), si))
    for i, st in enumerate(sets):
        hdr.append("#define PSET_%s %d" % (st["key"].upper(), i))
    for i, (k, _) in enumerate(tags):
        hdr.append("#define PTAG_%s (1u << %d)" % (k.upper(), i))
    hdr += ["#endif", ""]

    ages = sorted((s["list"], p["value"], p["age"]) for s, p in parts if p["age"])
    A = ["/* part_age_data.c - GENERATED by tools/partcat_build.py from core/data/parts/catalog/ (CR12).",
         " * The parts whose age class is not ALL, sorted by (list, value): resident on every platform",
         " * (src/part_age.c, the renderer and the generators read it). DO NOT EDIT. */",
         '#include "doomo/part_meta.h"', "",
         "const PartAgeRow doomo_part_age_rows[%d] = {" % max(1, len(ages))]
    A += ["    { %d, %d, %d }," % r for r in ages] or ["    { 0xFFFF, 0, 0 },     /* none yet: a sentinel */"]
    A += ["};", "const uint16_t doomo_part_age_count = %d;" % len(ages), ""]
    return meta, "\n".join(hdr), "\n".join(A)


def freeze(cat):
    have = set()
    if os.path.exists(GOLDEN):
        for ln, line in lines_of(GOLDEN):
            m = re.match(r'(\S+)\s+(\d+)\s', line)
            have.add((m.group(1), int(m.group(2))))
        out = open(GOLDEN).read()
    else:
        out = ("# core/tests/golden/part_ids.txt - every shipped (live) part id: SLOT VALUE \"Name\" (CR1).\n"
               "# Append-only: tools/partcat_build.py and test_part_meta.c reject a removed, renamed or\n"
               "# un-lived line. Add lines with `python3 tools/partcat_build.py --freeze` after a sign-off.\n")
    n = 0
    for s in cat["slots"]:
        for p in s["parts"]:
            if p["state"] == 0 and (s["key"], p["value"]) not in have:
                out += '%s %d "%s"\n' % (s["key"], p["value"], p["name"])
                n += 1
    with open(GOLDEN, "w") as f:
        f.write(out)
    print("partcat_build: froze %d new live part ids" % n)


def selftest():
    import tempfile
    import shutil
    global CAT
    good = CAT
    cases = [
        ("unknown set", 'slot hat list=hat name="Hat" method=P\n0 "None" set=nope tags=plain\n'),
        ("unknown tag", 'slot hat list=hat name="Hat" method=P\n0 "None" set=basics tags=nope\n'),
        ("no tag", 'slot hat list=hat name="Hat" method=P\n0 "None" set=basics\n'),
        ("too wide", 'slot hat list=hat name="Hat" method=P\n64 "Big" set=basics tags=plain\n'),
        ("name repeats", 'slot hat list=hat name="Hat" method=P\n0 "A" set=basics tags=plain\n1 "a" set=basics tags=plain\n'),
        ("bad age", 'slot hat list=hat name="Hat" method=P\n0 "A" set=basics tags=plain age=kid\n'),
        ("script missing", 'slot hat list=hat name="Hat" method=P\n0 "A" set=basics tags=plain method=S\n'),
    ]
    tmp = tempfile.mkdtemp()
    ok = 0
    try:
        for name, text in cases:
            d = os.path.join(tmp, name.replace(" ", "_"))
            os.makedirs(d)
            for f in META_FILES:
                if os.path.exists(os.path.join(good, f)):
                    shutil.copy(os.path.join(good, f), d)
            with open(os.path.join(d, "hat.txt"), "w") as f:
                f.write(text)
            CAT = d
            try:
                read_catalog()
                print("selftest: %s was NOT rejected" % name)
                return 1
            except CatError:
                ok += 1
    finally:
        CAT = good
        shutil.rmtree(tmp)
    # a renamed shipped id
    cat = read_catalog()
    g = os.path.join(tempfile.mkdtemp(), "g.txt")
    open(g, "w").write('hat 1 "Baseball Cap"\n')
    try:
        golden_check(cat, g)
        print("selftest: a renamed shipped id was NOT rejected")
        return 1
    except CatError:
        ok += 1
    print("selftest: %d rejects OK" % ok)
    return 0


def main(argv):
    root = ROOT
    i = 1
    mode = "gen"
    while i < len(argv):
        if argv[i] == "--out-root" and i + 1 < len(argv):
            root = os.path.abspath(argv[i + 1])
            i += 2
        elif argv[i] == "--selftest":
            return selftest()
        elif argv[i] == "--freeze":
            mode = "freeze"
            i += 1
        else:
            print("usage: partcat_build.py [--out-root DIR | --freeze | --selftest]", file=sys.stderr)
            return 2
    try:
        cat = read_catalog()
        if mode == "freeze":
            freeze(cat)
            return 0
        golden_check(cat)
        meta, hdr, age = emit(cat)
    except CatError as e:
        print("partcat_build: error: %s" % e, file=sys.stderr)
        return 1
    gen = os.path.join(root, "core", "gen")
    os.makedirs(gen, exist_ok=True)
    for name, text in (("part_meta_data.c", meta), ("part_meta_ids.h", hdr), ("part_age_data.c", age)):
        with open(os.path.join(gen, name), "w") as f:
            f.write(text)
    n = sum(len(s["parts"]) for s in cat["slots"])
    live = sum(1 for s in cat["slots"] for p in s["parts"] if p["state"] == 0)
    print("partcat_build: %d slots, %d parts (%d live), %d sets -> core/gen/part_meta_data.c, part_age_data.c"
          % (len(cat["slots"]), n, live, len(cat["sets"])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
