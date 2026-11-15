#!/usr/bin/env python3
# doomo-generator
"""gba_live.py - which 500 of the 1000 premade citizens the GBA keeps (GBA-500).

Owner, 2026-11-15: "make it five hundred residences for the gba". The GBA keeps
DOOMO_LIVE_RESIDENTS = 500 residents alive at once, 250 per district; the ids stay
0..999 (an id -> slot table, town.h). This tool picks the 500 a new Hellvile Park
Classic island moves in on the GBA, by a fixed rule:

  1. the MUST set, never left out (an old save keeps them too):
     a. roster citizens with story= or flags=STORY_NPC,
     b. the kid cast: traits KID or TEEN, and core/data/story/kid_cast.txt,
     c. every resident a story script names (say, npc, cast, verbs, befriend,
        possess ...: textpack's own compiler resolves them), every side
        mission's giver, every @possessor host / name_res and @armed host,
     d. every ROSTER_ID_* / ID_* cast constant the C code uses
        (core/src, core/include; the constants of roster.h, story_int.h,
        story_ch34.h), and every roster_find("Name") in core/src,
     e. every citizen whose full name is written in a story line or a Talk
        line (core/data/talk), so nobody is talked about who is not there;
  2. their households: a FAMILY, MARRIED or DATING roster edge from a MUST
     citizen brings the other one in (one step, while the district has room);
  3. the rest of each district, lowest ids first, until it has 250.

Writes core/gen/live_set.c: two 1000-bit sets (bit id = byte id >> 3, bit id & 7)
doomo_live_must[] and doomo_live_default[], and the count of each. The tool is
deterministic (make -C core check-gen). core/tests/test_res_live.c checks the rule
against the compiled tables (doomo_story_npc, the side missions, the possessors,
the kid traits) and lists the 500 (`tests --only res_live -v`).

    python3 tools/gba_live.py [--out-root DIR] [--list]
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORE = os.path.join(ROOT, "core")
OUT_REL = os.path.join("core", "gen", "live_set.c")
N_IDS = 1000
DISTRICT = 500
PER_DISTRICT = 250

sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(CORE, "tools"))
import roster_build  # noqa: E402
import textpack      # noqa: E402

STRIP = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)


def story_refs(names):
    """every resident id the story scripts resolve (textpack's compiler), with why"""
    refs = {}
    seen = []
    orig_who = textpack.Compiler.who

    def who(self, w, where):
        v = orig_who(self, w, where)
        seen.append((v, where))
        return v
    textpack.Compiler.who = who
    try:
        comp = textpack.Compiler(textpack.load_symbols())
        files = []
        for d, _, fs in os.walk(os.path.join(CORE, "data", "story")):
            for f in fs:
                if f.endswith(".dsc"):
                    files.append(os.path.join(d, f))
        for f in sorted(files):
            comp.read(f)
        comp.build()
        for r in textpack.mission_rows(comp).values():
            if r.get("kind"):
                seen.append((r["giver"], "side mission S%d" % (100 + r["sid"])))
        for where, a, _pack in comp.possessors:
            for k in ("host", "name_res"):
                v = a.get(k)
                if v and v not in ("script", "practice"):
                    seen.append((comp.who(v, where), where))
        for where, a, _pack in comp.armed:
            if "id" in a:
                seen.append((comp.who(a["id"], where), where))
        texts = []
        for p in comp.packs:
            texts.extend(p.strings)
    finally:
        textpack.Compiler.who = orig_who
    for v, where in seen:
        if isinstance(v, int) and 0 <= v < N_IDS:
            refs.setdefault(v, "story: %s" % os.path.relpath(str(where).split(":")[0], ROOT)
                            if os.path.isabs(str(where).split(":")[0]) else "story: %s" % where)
    return refs, texts


def c_refs(by_name):
    """ROSTER_ID_* / ID_* cast constants and roster_find("Name") in the C code"""
    consts = {}
    for h in (os.path.join(CORE, "include", "doomo", "roster.h"), os.path.join(CORE, "src", "story_int.h"),
              os.path.join(CORE, "src", "story_ch34.h")):
        src = STRIP.sub(" ", open(h).read())
        for m in re.finditer(r"#define[ \t]+((?:ROSTER_)?ID_[A-Z0-9_]+)[ \t]+([A-Z0-9_]+)", src):
            v = m.group(2)
            consts[m.group(1)] = int(v) if v.isdigit() else v
    for _ in range(4):
        for k, v in list(consts.items()):
            if isinstance(v, str) and isinstance(consts.get(v), int):
                consts[k] = consts[v]
    consts = {k: v for k, v in consts.items() if isinstance(v, int) and 0 <= v < N_IDS}
    refs = {}
    files = []
    for d in (os.path.join(CORE, "src"), os.path.join(CORE, "include", "doomo")):
        for f in sorted(os.listdir(d)):
            if f.endswith((".c", ".h")):
                files.append(os.path.join(d, f))
    for path in files:
        src = STRIP.sub(" ", open(path).read())
        rel = os.path.relpath(path, ROOT)
        for m in re.finditer(r"\b((?:ROSTER_)?ID_[A-Z0-9_]+)\b", src):
            if m.group(1) in consts:
                refs.setdefault(consts[m.group(1)], "C: %s %s" % (rel, m.group(1)))
        for m in re.finditer(r'roster_find\(\s*"([^"]+)"\s*\)', src):
            c = by_name.get(m.group(1).lower())
            if c is not None:
                refs.setdefault(c, "C: %s roster_find(\"%s\")" % (rel, m.group(1)))
    return refs


def kid_cast_file(by_name):
    refs = {}
    path = os.path.join(CORE, "data", "story", "kid_cast.txt")
    for line in open(path):
        line = line.split("#", 1)[0].strip()
        if not line or line.startswith("@"):
            continue
        f = line.split("\t")
        if f[0].isdigit():
            refs[int(f[0])] = "kid_cast.txt"
    return refs


def talk_texts():
    out = []
    for d, _, fs in os.walk(os.path.join(CORE, "data", "talk")):
        for f in sorted(fs):
            if f.endswith((".tlk", ".tsv", ".txt")):
                out.append(open(os.path.join(d, f), encoding="utf-8", errors="replace").read())
    return out


def named_in_text(cits, texts):
    """citizens whose full name is written in a line (whole words, case as written)"""
    blob = "\n".join(texts)
    words = set(re.findall(r"[A-Za-z0-9]+", blob))
    refs = {}
    for cid in sorted(cits):
        name = cits[cid].name
        first = re.findall(r"[A-Za-z0-9]+", name)
        if len(name) < 3 or not first or not all(w in words for w in first):
            continue
        if re.search(r"(?<![A-Za-z0-9])" + re.escape(name) + r"(?![A-Za-z0-9])", blob):
            refs[cid] = "named in a line"
    return refs


def pick():
    schema, by_id, edges, story, errors, _warnings = roster_build.load()
    if errors:
        raise SystemExit("gba_live: the roster has errors (python3 tools/roster_build.py --check)")
    by_name = {c.name.lower(): cid for cid, c in by_id.items()}
    why = {}

    def add(refs):
        for k in sorted(refs):
            why.setdefault(k, refs[k])
    roster = {}
    for cid, c in sorted(by_id.items()):
        traits = c.v.get("traits", 0)
        flags = c.v.get("flags", 0)
        if c.v.get("story") is not None or (isinstance(flags, int) and flags & 8):
            roster[cid] = "roster: story cast"
        elif isinstance(traits, int) and traits & (2 | 256):
            roster[cid] = "roster: kid / teen"
    add(roster)
    add(kid_cast_file(by_name))
    srefs, stexts = story_refs(by_name)
    add(srefs)
    add(c_refs(by_name))
    add(named_in_text(by_id, stexts + talk_texts()))
    must = set(why)
    chosen = set(must)
    count = [sum(1 for i in chosen if i // DISTRICT == d) for d in (0, 1)]
    if max(count) > PER_DISTRICT:
        raise SystemExit("gba_live: the must set has %d / %d citizens, more than %d in a district"
                         % (count[0], count[1], PER_DISTRICT))
    # households of the must set (one step), in edge order
    for e in edges:
        if e["kind"] not in ("FAMILY", "MARRIED", "DATING"):
            continue
        a, b = e["a"], e["b"]
        for x, y in ((a, b), (b, a)):
            if x in must and y not in chosen and count[y // DISTRICT] < PER_DISTRICT:
                chosen.add(y)
                count[y // DISTRICT] += 1
                why.setdefault(y, "household of %s" % by_id[x].name)
    # the rest, lowest ids first
    for i in range(N_IDS):
        d = i // DISTRICT
        if i not in chosen and count[d] < PER_DISTRICT:
            chosen.add(i)
            count[d] += 1
            why.setdefault(i, "fill (lowest ids)")
    return by_id, must, chosen, why


def bits(s):
    out = bytearray(N_IDS // 8)
    for i in s:
        out[i >> 3] |= 1 << (i & 7)
    return out


def emit(must, chosen, why, by_id):
    def table(name, s):
        b = bits(s)
        rows = []
        for k in range(0, len(b), 16):
            rows.append("    " + ", ".join("0x%02X" % v for v in b[k:k + 16]) + ",")
        return "const uint8_t %s[DOOMO_MAX_RESIDENTS / 8] = {\n%s\n};\n" % (name, "\n".join(rows))
    hist = {}
    for i in chosen:
        k = why[i].split(":")[0] if i in must else why[i].split(" of ")[0]
        hist[k] = hist.get(k, 0) + 1
    out = []
    out.append("/* GENERATED by tools/gba_live.py - do not edit. */\n")
    out.append("/* The GBA's 500 residents (GBA-500, docs/GBA.md \"500 residents\"): the must set (story,\n"
               " * Talk, side missions, exorcisms, endings, credits, the kid cast; never left out) and the\n"
               " * default set a new island moves in (must + households + the lowest ids, 250 per district).\n")
    out.append(" * must %d (Hillvile %d, Hellvile %d); default %d.\n" % (
        len(must), sum(1 for i in must if i < DISTRICT), sum(1 for i in must if i >= DISTRICT), len(chosen)))
    for k in sorted(hist):
        out.append(" *   %-22s %d\n" % (k, hist[k]))
    out.append(" */\n#include \"doomo/res_live.h\"\n\n")
    out.append("const uint16_t doomo_live_must_count = %du;\n" % len(must))
    out.append(table("doomo_live_must", must))
    out.append("\n")
    out.append(table("doomo_live_default", chosen))
    return "".join(out)


def main(argv):
    out_root = ROOT
    listing = False
    i = 0
    while i < len(argv):
        if argv[i] == "--out-root":
            out_root = os.path.abspath(argv[i + 1])
            i += 2
        elif argv[i] == "--list":
            listing = True
            i += 1
        else:
            print("gba_live: unknown argument %s" % argv[i], file=sys.stderr)
            return 2
    by_id, must, chosen, why = pick()
    if listing:
        for k in sorted(chosen):
            print("%4d  %-11s  %s%s" % (k, by_id[k].name, "MUST  " if k in must else "      ", why[k]))
        return 0
    path = os.path.join(out_root, OUT_REL)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    text = emit(must, chosen, why, by_id)
    old = open(path).read() if os.path.exists(path) else None
    if old != text:
        with open(path, "w") as f:
            f.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
