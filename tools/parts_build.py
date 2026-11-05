#!/usr/bin/env python3
# doomo-generator
"""parts_build.py - compile the part scripts (customisation v2, P1.M1; SPEC 4.4).

Reads the text files of core/data/parts/<dir>/ (dir = face, head) and writes

    core/gen/parts_face.c, core/gen/parts_head.c    (a PartSet each, include/doomo/parts.h)

    python3 tools/parts_build.py [--out-root DIR]
    python3 tools/parts_build.py --selftest          check the rejects below

SOURCE FORMAT (one `#` comment per line allowed; coordinates are normalised, 256 = one radius,
stored in steps of 2; radii 0..255):

    stamp NAME W H            1bpp stamp, W,H 1..16, then H rows of . and X
    list FIELD VALUE          the script of one style: FIELD is a gene field of fields.txt
    block group=G anchor=head|torso lod=tiny,mid,big|all view=portrait,figure,fighter,chibi|all
      mat FIELD [light|dark]      anchor head|torso    shade NAME      solid [noline,nocast,...]
      decal    mirror on|off    scale N (64 = 1.0)     region none|head|eye_l|eye_r|muzzle|torso
      ellipse X Y RX RY         capsule X0 Y0 X1 Y1 R0 R1      tri X0 Y0 X1 Y1 X2 Y2
      line X0 Y0 X1 Y1 C        dot X Y C              stamp NAME X Y [Q4]
      ifview V,.. / ifyaw 0,1,.. / ifanim blink,breath,twitch,wag   ...ops...   endif
    end

Rejected at build time: an unknown field, name or op, a coordinate outside -256..254, a block over
255 bytes, a stamp bigger than 16x16, and any solid part over 600 px at bust scale (estimated from
its primitives at a nominal bust head of 36x42 px / torso 20x26 px). Integer-only output,
byte-identical on every machine.
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
FIELDS = os.path.join(ROOT, "core", "data", "look", "fields.txt")

OPS = ["END", "ANCHOR", "MAT", "SHADE", "SOLID", "DECAL", "ELLIPSE", "CAPSULE", "TRI", "LINE", "DOT",
       "STAMP", "MIRROR", "SCALE", "REGION", "IFVIEW", "IFYAW", "IFANIM"]
OP = {n: i for i, n in enumerate(OPS)}
SHADES = ["ball", "flat", "vgrad", "gloss", "soft", "hgrad"]
PFLAGS = {"noline": 1, "nocast": 2, "nocont": 4, "fuzz": 8, "darkline": 16}
REGIONS = ["none", "head", "eye_l", "eye_r", "muzzle", "torso"]
VIEWS = ["portrait", "figure", "fighter", "chibi"]
LODS = ["tiny", "mid", "big"]
ANIMS = ["blink", "breath", "twitch", "wag"]
MAX_PART_PX = 600
NOMINAL = {"head": (36, 42), "torso": (20, 26)}      # bust-scale half sizes in px


class PartError(Exception):
    pass


def field_ids():
    ids = {}
    n = 0
    for line in open(FIELDS):
        line = line.split("#", 1)[0].split()
        if line:
            ids[line[0]] = n
            n += 1
    return ids


def coord(tok, where):
    v = int(tok)
    if not -256 <= v <= 254:
        raise PartError("%s: coordinate %d outside -256..254" % (where, v))
    return (v // 2) & 255                              # int8 in steps of two


def radius(tok, where):
    v = int(tok)
    if not 0 <= v <= 255:
        raise PartError("%s: radius %d outside 0..255" % (where, v))
    return v


def mask_of(names, table, where):
    if names == "all":
        return (1 << len(table)) - 1
    m = 0
    for n in names.split(","):
        if n not in table:
            raise PartError("%s: unknown name %r (one of %s)" % (where, n, ", ".join(table)))
        m |= 1 << table.index(n)
    return m


class Area:
    """Pixel-area estimate of the solid part being built (bust scale)."""
    def __init__(self):
        self.px = 0.0
        self.worst = 0.0

    def close(self):
        self.worst = max(self.worst, self.px)
        self.px = 0.0


def compile_block(lines, stamps, fields, where):
    """lines: [(lineno, tokens)] of one block's body. Returns the op bytes."""
    out = bytearray()
    ifs = []                                            # offsets of skip operands to patch
    anchor, mirror, scale = "head", 1, 1.0
    area = Area()

    def sx():
        return NOMINAL[anchor][0] / 256.0 * scale

    def sy():
        return NOMINAL[anchor][1] / 256.0 * scale

    for ln, t in lines:
        w = "%s:%d" % (where, ln)
        k = t[0]
        try:
            if k == "anchor":
                anchor = t[1]
                if anchor not in NOMINAL:
                    raise PartError("%s: anchor must be head or torso" % w)
                out += bytes([OP["ANCHOR"], 0 if anchor == "head" else 1])
            elif k == "mat":
                if t[1] not in fields:
                    raise PartError("%s: unknown field %r" % (w, t[1]))
                tone = 0 if len(t) < 3 else {"light": 1, "dark": 2}[t[2]]
                out += bytes([OP["MAT"], fields[t[1]], tone])
            elif k == "shade":
                out += bytes([OP["SHADE"], SHADES.index(t[1])])
            elif k == "solid":
                fl = 0
                for n in (t[1].split(",") if len(t) > 1 else []):
                    fl |= PFLAGS[n]
                area.close()
                out += bytes([OP["SOLID"], fl])
            elif k == "decal":
                area.close()
                out += bytes([OP["DECAL"]])
            elif k == "mirror":
                mirror = 2 if t[1] == "on" else 1
                out += bytes([OP["MIRROR"], 1 if t[1] == "on" else 0])
            elif k == "scale":
                v = int(t[1])
                if not 1 <= v <= 255:
                    raise PartError("%s: scale outside 1..255" % w)
                scale = v / 64.0
                out += bytes([OP["SCALE"], v])
            elif k == "region":
                out += bytes([OP["REGION"], REGIONS.index(t[1])])
            elif k == "ellipse":
                out += bytes([OP["ELLIPSE"], coord(t[1], w), coord(t[2], w), radius(t[3], w), radius(t[4], w)])
                area.px += 3.14159 * int(t[3]) * sx() * int(t[4]) * sy() * mirror
            elif k == "capsule":
                out += bytes([OP["CAPSULE"]] + [coord(t[i], w) for i in range(1, 5)] +
                             [radius(t[5], w), radius(t[6], w)])
                r = (int(t[5]) + int(t[6])) / 2.0 * sx()
                ln_ = (((int(t[3]) - int(t[1])) * sx()) ** 2 + ((int(t[4]) - int(t[2])) * sy()) ** 2) ** 0.5
                area.px += (3.14159 * r * r + 2 * r * ln_) * mirror
            elif k == "tri":
                v = [int(x) for x in t[1:7]]
                out += bytes([OP["TRI"]] + [coord(x, w) for x in t[1:7]])
                area.px += abs((v[0] * (v[3] - v[5]) + v[2] * (v[5] - v[1]) + v[4] * (v[1] - v[3])) / 2.0) * sx() * sy() * mirror
            elif k == "line":
                out += bytes([OP["LINE"]] + [coord(t[i], w) for i in range(1, 5)] + [int(t[5])])
            elif k == "dot":
                out += bytes([OP["DOT"], coord(t[1], w), coord(t[2], w), int(t[3])])
            elif k == "stamp":
                if t[1] not in stamps:
                    raise PartError("%s: unknown stamp %r" % (w, t[1]))
                q = int(t[4]) if len(t) > 4 else 16
                if not 1 <= q <= 255:
                    raise PartError("%s: stamp scale outside 1..255" % w)
                out += bytes([OP["STAMP"], list(stamps).index(t[1]), coord(t[2], w), coord(t[3], w), q])
            elif k in ("ifview", "ifyaw", "ifanim"):
                table = {"ifview": VIEWS, "ifyaw": None, "ifanim": ANIMS}[k]
                if table is None:
                    m = 0
                    for n in t[1].split(","):
                        if not 0 <= int(n) <= 4:
                            raise PartError("%s: yaw outside 0..4" % w)
                        m |= 1 << int(n)
                else:
                    m = mask_of(t[1], table, w)
                out += bytes([OP[k.upper()], m, 0])
                ifs.append(len(out) - 1)
            elif k == "endif":
                if not ifs:
                    raise PartError("%s: endif without an if" % w)
                at = ifs.pop()
                skip = len(out) - at - 1
                if skip > 255:
                    raise PartError("%s: if body over 255 bytes" % w)
                out[at] = skip
            else:
                raise PartError("%s: unknown op %r" % (w, k))
        except (IndexError, ValueError, KeyError) as e:
            raise PartError("%s: bad operands for %r (%s)" % (w, k, e))
    if ifs:
        raise PartError("%s: if without an endif" % where)
    area.close()
    if area.worst > MAX_PART_PX:
        raise PartError("%s: a solid part is about %d px at bust scale (limit %d)" % (where, area.worst, MAX_PART_PX))
    if len(out) > 255:
        raise PartError("%s: block is %d bytes (limit 255)" % (where, len(out)))
    return bytes(out)


def parse(path, fields):
    """-> (stamps {name: (w, h, bytes)} in file order, entries {(list, value): [block bytes]})."""
    stamps, entries = {}, {}
    cur = None
    block = None                                        # [(group, anchor, lodviews), [(ln, tokens)], where]
    where = os.path.relpath(path, ROOT) if path.startswith(ROOT) else os.path.basename(path)
    rows = None
    for ln, raw in enumerate(open(path), 1):
        line = raw.split("#", 1)[0].strip()
        w = "%s:%d" % (where, ln)
        if rows is not None:
            if not line:
                continue
            name, sw, sh, got = rows
            if len(line) != sw or set(line) - set(".X"):
                raise PartError("%s: stamp row must be %d of . and X" % (w, sw))
            got.append(line)
            if len(got) == sh:
                data = bytearray()
                for r in got:
                    bits = int("".join("1" if ch == "X" else "0" for ch in r), 2) << (-sw % 8)
                    data += bits.to_bytes((sw + 7) // 8, "big")
                stamps[name] = (sw, sh, bytes(data))
                rows = None
            continue
        if not line:
            continue
        t = line.split()
        if t[0] == "stamp" and block is None and len(t) == 4:
            sw, sh = int(t[2]), int(t[3])
            if not (1 <= sw <= 16 and 1 <= sh <= 16):
                raise PartError("%s: stamp is %dx%d (1..16 allowed)" % (w, sw, sh))
            if t[1] in stamps:
                raise PartError("%s: stamp %r defined twice" % (w, t[1]))
            rows = (t[1], sw, sh, [])
        elif t[0] == "list":
            if block is not None:
                raise PartError("%s: list inside a block" % w)
            if t[1] not in fields:
                raise PartError("%s: unknown field %r" % (w, t[1]))
            cur = (fields[t[1]], int(t[2]))
            if cur in entries:
                raise PartError("%s: list %s %s defined twice" % (w, t[1], t[2]))
            entries[cur] = []
        elif t[0] == "block":
            if cur is None or block is not None:
                raise PartError("%s: block outside a list / inside a block" % w)
            kv = dict(x.split("=", 1) for x in t[1:])
            grp = int(kv.get("group", "0"))
            if not 0 <= grp <= 6:
                raise PartError("%s: group outside 0..6" % w)
            if kv.get("anchor", "head") not in NOMINAL:
                raise PartError("%s: anchor must be head or torso" % w)
            lv = mask_of(kv.get("lod", "all"), LODS, w) | (mask_of(kv.get("view", "all"), VIEWS, w) << 3)
            block = [(grp, 0 if kv.get("anchor", "head") == "head" else 1, lv), [], w]
        elif t[0] == "end":
            if block is None:
                raise PartError("%s: end without a block" % w)
            body = list(block[1])
            if block[0][1] == 1:
                body.insert(0, (ln, ["anchor", "torso"]))
            ops = compile_block(body, stamps, fields, block[2])
            g, a, lv = block[0]
            entries[cur].append(bytes([g, a, lv, len(ops)]) + ops)
            block = None
        elif block is not None:
            block[1].append((ln, t))
        else:
            raise PartError("%s: unexpected %r" % (w, t[0]))
    if block is not None or rows is not None:
        raise PartError("%s: unterminated block or stamp" % where)
    return stamps, entries


def emit(dirname, fields):
    d = os.path.join(ROOT, "core", "data", "parts", dirname)
    stamps, entries = {}, {}
    for f in sorted(os.listdir(d)) if os.path.isdir(d) else []:
        if not f.endswith(".txt"):
            continue
        s, e = parse(os.path.join(d, f), fields)
        for k in s:
            if k in stamps:
                raise PartError("%s: stamp %r defined in two files" % (dirname, k))
        stamps.update(s)
        for k in e:
            if k in entries:
                raise PartError("%s: list entry %r defined in two files" % (dirname, k))
        entries.update(e)
    ops = bytearray()
    refs = []
    for key in sorted(entries):
        refs.append((key[0], key[1], len(ops)))
        for b in entries[key]:
            ops += b
        ops += bytes([0xFF, 0, 0, 0])
    if len(ops) >= 65536:
        raise PartError("%s: scripts over 64 KB" % dirname)
    if not ops:
        ops = bytearray([0xFF, 0, 0, 0])
    bits, srefs = bytearray(), []
    for name, (w, h, data) in stamps.items():
        srefs.append((w, h, len(bits)))
        bits += data
    if not bits:
        bits = bytearray([0])

    def arr(b):
        return ",\n".join("    " + ", ".join("0x%02x" % x for x in b[i:i + 16]) for i in range(0, len(b), 16)) + ","

    L = ["/* parts_%s.c - GENERATED by tools/parts_build.py from core/data/parts/%s/." % (dirname, dirname),
         " * DO NOT EDIT: edit the text files and run `python3 tools/parts_build.py`. */",
         '#include "doomo/doomo.h"', '#include "doomo/parts.h"', "",
         "static const PartRef refs[] = {"]
    L += ["    { %d, %d, %d }," % r for r in refs] or ["    { 0, 0, 0 },"]
    L += ["};", "static const uint8_t ops[] = {", arr(ops), "};", "static const StampRef stamps[] = {"]
    L += ["    { %d, %d, %d }," % r for r in srefs] or ["    { 0, 0, 0 },"]
    L += ["};", "static const uint8_t bits[] = {", arr(bits), "};", "",
          "const PartSet doomo_parts_%s = { refs, %d, ops, stamps, %d, bits };" % (dirname, len(refs), len(srefs)), ""]
    return "\n".join(L), len(refs), len(ops)


def selftest():
    import tempfile
    fields = field_ids()
    cases = [
        ("big part", "list eye_style 1\nblock group=2\n mat skin\n solid\n ellipse 0 0 255 255\nend\n"),
        ("bad coordinate", "list eye_style 1\nblock group=2\n ellipse 400 0 5 5\nend\n"),
        ("bad field", "list nope 1\n"),
        ("bad op", "list eye_style 1\nblock group=2\n frob 1\nend\n"),
        ("bad stamp", "stamp s 20 8\n"),
        ("open if", "list eye_style 1\nblock group=2\n ifview chibi\n dot 0 0 2\nend\n"),
    ]
    ok = 0
    for name, text in cases:
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
            f.write(text)
        try:
            parse(f.name, fields)
            print("selftest: %s was NOT rejected" % name)
            return 1
        except PartError:
            ok += 1
        finally:
            os.unlink(f.name)
    print("selftest: %d rejects OK" % ok)
    return 0


def main(argv):
    root = ROOT
    i = 1
    while i < len(argv):
        if argv[i] == "--out-root" and i + 1 < len(argv):
            root = os.path.abspath(argv[i + 1])
            i += 2
        elif argv[i] == "--selftest":
            return selftest()
        else:
            print("usage: parts_build.py [--out-root DIR | --selftest]", file=sys.stderr)
            return 2
    fields = field_ids()
    try:
        for d in ("face", "head"):
            text, n, size = emit(d, fields)
            os.makedirs(os.path.join(root, "core", "gen"), exist_ok=True)
            with open(os.path.join(root, "core", "gen", "parts_%s.c" % d), "w") as f:
                f.write(text)
            print("parts_build: %s: %d scripts, %d B -> core/gen/parts_%s.c" % (d, n, size, d))
    except PartError as e:
        print("parts_build: error: %s" % e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
