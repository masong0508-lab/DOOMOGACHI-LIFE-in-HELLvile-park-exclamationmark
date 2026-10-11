#!/usr/bin/env python3
# doomo-generator
"""mapc.py - the ASCII map compiler (owner: action).

Compiles every core/data/maps/*.txt into one C file per map:

    core/data/maps/vape_cellar.txt  ->  core/gen/map_vape_cellar.c
                                        (const DoomoMapDef map_def_vape_cellar)

core/src/maps.c references the map_def_<name> symbols weakly, so a map
links in as soon as its generated file exists. The format and the legend
are documented in core/data/maps/README.md.

    python3 tools/mapc.py [--out-root DIR] [--check] [--verbose] [FILE.txt ...]

--out-root DIR  write DIR/core/gen/map_<name>.c (default: the repository)
--check         parse + validate only, write nothing
FILE.txt        compile only these files (default: every core/data/maps/*.txt)

Enum values (TEX_*, SKY_*, SONG_*, THING_*, CELL_*, THF_*, MAP_*) are read
from the headers in core/include/doomo/, so the compiler never goes out of
date when an enum grows. Output is deterministic (stdlib only).
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
INC = os.path.join(ROOT, "core", "include", "doomo")
MAPS_DIR = os.path.join(ROOT, "core", "data", "maps")

MAX_W = 64
MAX_H = 64


class MapError(Exception):
    pass


# ---------------------------------------------------------------------------
# C enum reader
def _strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def read_enums(*headers):
    """Return {NAME: value} for every enumerator in the given headers."""
    values = {}
    for h in headers:
        with open(os.path.join(INC, h), "r", encoding="ascii") as f:
            text = _strip_comments(f.read())
        for body in re.findall(r"enum\s*\w*\s*\{(.*?)\}", text, flags=re.S):
            cur = -1
            for item in body.split(","):
                item = item.strip()
                if not item:
                    continue
                if "=" in item:
                    name, expr = [s.strip() for s in item.split("=", 1)]
                    cur = _eval_expr(expr, values)
                else:
                    name = item
                    cur += 1
                if re.match(r"^[A-Za-z_]\w*$", name):
                    values[name] = cur
    return values


def _eval_expr(expr, values):
    expr = expr.strip()
    toks = re.findall(r"0x[0-9A-Fa-f]+|\d+|[A-Za-z_]\w*|<<|>>|[-+|()~*]", expr)
    out = []
    for t in toks:
        if re.match(r"^[A-Za-z_]\w*$", t):
            if t not in values:
                raise MapError("unknown enum symbol %s in %s" % (t, expr))
            out.append(str(values[t]))
        elif t.startswith("0x"):
            out.append(str(int(t, 16)))
        else:
            out.append(t)
    return int(eval(" ".join(out), {"__builtins__": {}}, {}))  # trusted headers


ENUMS = read_enums("textures.h", "audio.h", "actor.h", "map.h")


def enum_lookup(prefix, name, what):
    """TEX + 'pastel_brick' -> TEX_PASTEL_BRICK value; numbers pass through."""
    n = name.strip()
    if re.match(r"^-?\d+$", n):
        return int(n)
    key = n.upper()
    if not key.startswith(prefix + "_"):
        key = prefix + "_" + key
    if key not in ENUMS:
        raise MapError("unknown %s '%s'" % (what, name))
    return ENUMS[key]


def enum_names(prefix, stop):
    return sorted(k for k in ENUMS if k.startswith(prefix + "_") and k != stop)


TEX_COUNT = ENUMS["TEX_COUNT"]
THING_COUNT = ENUMS["THING_COUNT"]
SKY_COUNT = ENUMS["SKY_COUNT"]
SONG_COUNT = ENUMS["SONG_COUNT"]
TEX_SKY = 255
CELL_FLAGS = {
    "door": ENUMS["CELL_DOOR"], "lift": ENUMS["CELL_LIFT"],
    "lava": ENUMS["CELL_LAVA"], "water": ENUMS["CELL_WATER"],
    "secret": ENUMS["CELL_SECRET"], "exit": ENUMS["CELL_EXIT"],
    "portal": ENUMS["CELL_PORTAL"], "trigger": ENUMS["CELL_TRIGGER"],
}
THING_FLAGS = {
    "ambush": ENUMS["THF_AMBUSH"], "not_easy": ENUMS["THF_NOT_EASY"],
    "hard": ENUMS["THF_HARD"], "dormant": ENUMS["THF_DORMANT"],
    "boss": ENUMS["THF_BOSS"], "kill_event": ENUMS["THF_KILL_EVENT"],
    "secret": ENUMS["THF_SECRET"],
}
KINDS = {"overworld": "MAP_OVERWORLD", "dungeon": "MAP_DUNGEON", "arena": "MAP_ARENA"}
DEMONS = set(range(ENUMS["THING_IMPLING"], ENUMS["THING_LORD_VAPULA"] + 1))


# ---------------------------------------------------------------------------
# parsing helpers
def tokenize(line):
    """Split on whitespace, keeping "quoted strings" together."""
    out, cur, q = [], "", False
    for ch in line:
        if ch == '"':
            q = not q
            continue
        if ch.isspace() and not q:
            if cur:
                out.append(cur)
                cur = ""
            continue
        cur += ch
    if q:
        raise MapError("unterminated string")
    if cur:
        out.append(cur)
    return out


def parse_kv(tokens):
    kv = []
    for t in tokens:
        if "=" not in t:
            raise MapError("expected key=value, got '%s'" % t)
        k, v = t.split("=", 1)
        kv.append((k.strip().lower(), v.strip()))
    return kv


def parse_int(v, lo, hi, what):
    try:
        n = int(v, 0)
    except ValueError:
        raise MapError("%s: '%s' is not a number" % (what, v))
    if n < lo or n > hi:
        raise MapError("%s %d out of range %d..%d" % (what, n, lo, hi))
    return n


def parse_tex(v, allow_sky=False):
    if v.lower() in ("0", "none", "-"):
        return 0
    if allow_sky and v.lower() == "sky":
        return TEX_SKY
    n = enum_lookup("TEX", v, "texture")
    if n < 0 or (n >= TEX_COUNT and not (allow_sky and n == TEX_SKY)):
        raise MapError("texture %s out of range" % v)
    return n


def parse_flags(v, table, what):
    bits = 0
    for f in v.split(","):
        f = f.strip().lower()
        if not f or f == "none":
            continue
        if f not in table:
            raise MapError("unknown %s flag '%s' (have: %s)" % (what, f, ", ".join(sorted(table))))
        bits |= table[f]
    return bits


def overlay_value(ch):
    """base-36 height digits: 0-9 a-z = 0..35, A-Z = -1..-26."""
    if ch.isdigit():
        return int(ch)
    if "a" <= ch <= "z":
        return 10 + ord(ch) - ord("a")
    if "A" <= ch <= "Z":
        return -(1 + ord(ch) - ord("A"))
    raise MapError("bad overlay digit '%s'" % ch)


class Thing(object):
    __slots__ = ("x", "y", "type", "angle", "arg", "flags", "line")

    def __init__(self, x, y, typ, angle=0, arg=0, flags=0, line=0):
        self.x, self.y, self.type = x, y, typ
        self.angle, self.arg, self.flags, self.line = angle, arg, flags, line


def thing_from_kv(kv, x, y, line):
    typ, angle, arg, flags = None, 0, 0, 0
    for k, v in kv:
        if k in ("thing", "type"):
            typ = enum_lookup("THING", v, "thing")
            if typ <= 0 or typ >= THING_COUNT:
                raise MapError("thing %s out of range" % v)
        elif k == "angle":
            deg = parse_int(v, -360, 720, "angle")
            angle = ((deg % 360) * 256 + 180) // 360 & 255
        elif k == "arg":
            arg = parse_int(v, 0, 255, "arg")
        elif k == "tflags":
            flags = parse_flags(v, THING_FLAGS, "thing")
        else:
            raise MapError("unknown thing key '%s'" % k)
    if typ is None:
        raise MapError("thing without a type")
    return Thing(x, y, typ, angle, arg, flags, line)


# ---------------------------------------------------------------------------
def parse_map(path):
    name = os.path.splitext(os.path.basename(path))[0]
    m = {
        "name": name, "title": name.replace("_", " ").title(), "kind": "dungeon",
        "sky": 0, "music": 0, "ambient": 12, "legend": {}, "things_legend": {},
        "grid": None, "overlays": {}, "things": [], "path": path,
    }
    with open(path, "r", encoding="ascii") as f:
        lines = f.read().split("\n")
    i = 0
    block = None
    rows = []
    while i < len(lines):
        raw = lines[i].rstrip("\r")
        lineno = i + 1
        i += 1
        try:
            if block is not None:
                if raw.strip() == "end":
                    if block == "grid":
                        m["grid"] = rows
                    else:
                        m["overlays"][block] = rows
                    block, rows = None, []
                else:
                    rows.append(raw)
                continue
            s = raw.strip()
            if not s or s.startswith("#"):
                continue
            toks = tokenize(s)
            kw = toks[0].lower()
            # trailing comment: a token starting with '#' (after the legend
            # character of a cell line, which may itself be '#')
            first = 2 if kw == "cell" else 1
            for ci in range(first, len(toks)):
                if toks[ci].startswith("#"):
                    toks = toks[:ci]
                    break
            if kw == "map":
                if len(toks) != 2 or not re.match(r"^[a-z_][a-z0-9_]*$", toks[1]):
                    raise MapError("map NAME (lowercase C identifier)")
                m["name"] = toks[1]
            elif kw == "title":
                m["title"] = " ".join(toks[1:])
                if len(m["title"]) > 31 or any(ord(c) > 126 for c in m["title"]):
                    raise MapError("title must be <= 31 ASCII chars")
            elif kw == "kind":
                if len(toks) != 2 or toks[1].lower() not in KINDS:
                    raise MapError("kind overworld|dungeon|arena")
                m["kind"] = toks[1].lower()
            elif kw == "sky":
                m["sky"] = enum_lookup("SKY", toks[1], "sky")
                if not 0 <= m["sky"] < SKY_COUNT:
                    raise MapError("sky out of range")
            elif kw == "music":
                m["music"] = enum_lookup("SONG", toks[1], "song")
                if not 0 <= m["music"] < SONG_COUNT:
                    raise MapError("song out of range")
            elif kw == "ambient":
                m["ambient"] = parse_int(toks[1], 0, 15, "ambient")
            elif kw == "cell":
                if len(toks) < 2 or len(toks[1]) != 1:
                    raise MapError("cell C key=value ...  (C is one character)")
                ch = toks[1]
                if ch in m["legend"]:
                    raise MapError("cell '%s' defined twice" % ch)
                cell = {"wall": 0, "floor": 0, "ceil": 0, "fh": 0, "ch": 16,
                        "light": 0, "flags": 0, "tag": 0}
                thing_kv = []
                for k, v in parse_kv(toks[2:]):
                    if k == "like":
                        if v not in m["legend"]:
                            raise MapError("like=%s: not defined yet" % v)
                        cell = dict(m["legend"][v])
                        if v in m["things_legend"] and not thing_kv:
                            pass  # things are not inherited
                    elif k == "wall":
                        cell["wall"] = parse_tex(v)
                    elif k == "floor":
                        cell["floor"] = parse_tex(v)
                    elif k == "ceil":
                        cell["ceil"] = parse_tex(v, allow_sky=True)
                    elif k == "fh":
                        cell["fh"] = parse_int(v, -128, 127, "fh")
                    elif k == "ch":
                        cell["ch"] = parse_int(v, -128, 127, "ch")
                    elif k == "light":
                        cell["light"] = parse_int(v, 0, 15, "light")
                    elif k == "tag":
                        cell["tag"] = parse_int(v, -128, 255, "tag") & 255
                    elif k == "flags":
                        cell["flags"] = parse_flags(v, CELL_FLAGS, "cell")
                    elif k in ("thing", "angle", "arg", "tflags"):
                        thing_kv.append((k, v))
                    else:
                        raise MapError("unknown cell key '%s'" % k)
                m["legend"][ch] = cell
                if thing_kv:
                    m["things_legend"][ch] = thing_kv
            elif kw in ("grid", "floorh", "ceilh", "light"):
                if len(toks) != 1:
                    raise MapError("'%s' starts a block that ends with 'end'" % kw)
                block, rows = kw, []
            elif kw == "thing":
                if len(toks) < 4:
                    raise MapError("thing X Y TYPE [angle=..] [arg=..] [tflags=..]")
                x = parse_int(toks[1], 0, MAX_W - 1, "thing x")
                y = parse_int(toks[2], 0, MAX_H - 1, "thing y")
                kv = [("thing", toks[3])] + parse_kv(toks[4:])
                m["things"].append(thing_from_kv(kv, x, y, lineno))
            else:
                raise MapError("unknown keyword '%s'" % toks[0])
        except MapError as e:
            raise MapError("%s:%d: %s" % (path, lineno, e))
    if block is not None:
        raise MapError("%s: block '%s' has no 'end'" % (path, block))
    if not m["grid"]:
        raise MapError("%s: no grid" % path)
    return m


def build(m):
    path = m["path"]
    grid = m["grid"]
    h = len(grid)
    w = max(len(r) for r in grid)
    if w > MAX_W or h > MAX_H:
        raise MapError("%s: map is %dx%d, max %dx%d" % (path, w, h, MAX_W, MAX_H))
    for y, r in enumerate(grid):
        if len(r) != w:
            raise MapError("%s: grid row %d is %d wide, expected %d" % (path, y, len(r), w))
    cells = []
    things = []
    for y in range(h):
        for x in range(w):
            ch = grid[y][x]
            if ch not in m["legend"]:
                raise MapError("%s: grid (%d,%d): no cell legend for '%s'" % (path, x, y, ch))
            cells.append(dict(m["legend"][ch]))
            if ch in m["things_legend"]:
                things.append(thing_from_kv(m["things_legend"][ch], x, y, 0))
    for name, rows in sorted(m["overlays"].items()):
        if len(rows) > h:
            raise MapError("%s: overlay %s has %d rows, the grid only %d" % (path, name, len(rows), h))
        for y, r in enumerate(rows):
            if len(r) > w:
                raise MapError("%s: overlay %s row %d too wide" % (path, name, y))
            for x, ch in enumerate(r):
                if ch in " .":
                    continue
                c = cells[y * w + x]
                if name == "floorh":
                    c["fh"] = overlay_value(ch)
                elif name == "ceilh":
                    c["ch"] = overlay_value(ch)
                elif name == "light":
                    v = overlay_value(ch)
                    if not 0 <= v <= 15:
                        raise MapError("%s: light overlay (%d,%d) out of range" % (path, x, y))
                    c["light"] = v
    things.extend(m["things"])
    validate(m, w, h, cells, things)
    return w, h, cells, things


def validate(m, w, h, cells, things):
    path = m["path"]
    starts = [t for t in things if t.type == ENUMS["THING_PLAYER_START"]]
    if m["kind"] != "overworld" and len(starts) != 1:
        raise MapError("%s: needs exactly one PLAYER_START (has %d)" % (path, len(starts)))
    if len(starts) > 1:
        raise MapError("%s: more than one PLAYER_START" % path)
    for idx, c in enumerate(cells):
        x, y = idx % w, idx // w
        where = "%s: cell (%d,%d)" % (path, x, y)
        for k in ("wall", "floor"):
            if c[k] >= TEX_COUNT:
                raise MapError("%s: %s texture %d out of range" % (where, k, c[k]))
        if c["ceil"] >= TEX_COUNT and c["ceil"] != TEX_SKY:
            raise MapError("%s: ceil texture %d out of range" % (where, c["ceil"]))
        if c["wall"]:
            continue
        if c["floor"] == 0:
            raise MapError("%s: open cell without a floor texture" % where)
        if c["ceil"] == 0:
            raise MapError("%s: open cell without a ceiling texture (use ceil=sky)" % where)
        if c["ch"] <= c["fh"] and c["ceil"] != TEX_SKY:
            raise MapError("%s: ceiling %d not above floor %d" % (where, c["ch"], c["fh"]))
        if c["flags"] & CELL_FLAGS["door"]:
            if c["ceil"] == TEX_SKY:
                raise MapError("%s: a door needs a door texture as ceil=" % where)
            if c["flags"] & CELL_FLAGS["lift"]:
                raise MapError("%s: a cell cannot be both door and lift" % where)
        if c["flags"] & CELL_FLAGS["lift"]:
            top = c["tag"] - 256 if c["tag"] > 127 else c["tag"]
            if top == c["fh"]:
                raise MapError("%s: lift tag (other height) equals its floor" % where)
            if c["ceil"] != TEX_SKY and max(top, c["fh"]) * 8 + 56 > c["ch"] * 8:
                raise MapError("%s: lift travels into its ceiling" % where)
    for t in things:
        if t.x >= w or t.y >= h:
            raise MapError("%s: thing at (%d,%d) outside the map" % (path, t.x, t.y))
        c = cells[t.y * w + t.x]
        if c["wall"]:
            raise MapError("%s: thing %d at (%d,%d) is inside a wall" % (path, t.type, t.x, t.y))
    if len(things) > 255:
        raise MapError("%s: too many things (%d > 255)" % (path, len(things)))


def emit(m, w, h, cells, things):
    name = m["name"]
    rel = os.path.relpath(m["path"], ROOT).replace(os.sep, "/")
    out = []
    out.append("/* GENERATED by tools/mapc.py from %s - do not edit.\n" % rel)
    out.append(" * \"%s\": %dx%d cells, %d things. */\n" % (m["title"], w, h, len(things)))
    out.append('#include "doomo/map.h"\n\n')
    out.append("/* {wall, floor_tex, ceil_tex, floor_h, ceil_h, light, flags, tag} */\n")
    out.append("static const DoomoCell cells_%s[%d] = {\n" % (name, w * h))
    for y in range(h):
        row = []
        for x in range(w):
            c = cells[y * w + x]
            row.append("{%d,%d,%d,%d,%d,%d,%d,%d}" % (
                c["wall"], c["floor"], c["ceil"], c["fh"], c["ch"], c["light"],
                c["flags"], c["tag"]))
        out.append("    /* %2d */ %s,\n" % (y, ",".join(row)))
    out.append("};\n\n")
    if things:
        out.append("/* {x, y, type, angle, arg, flags} */\n")
        out.append("static const DoomoThing things_%s[%d] = {\n" % (name, len(things)))
        for t in things:
            out.append("    {%d, %d, %d, %d, %d, %d},\n" % (t.x, t.y, t.type, t.angle, t.arg, t.flags))
        out.append("};\n\n")
    out.append("const DoomoMapDef map_def_%s = {\n" % name)
    out.append('    "%s", %d, %d, %s, %d, %d, %d,\n' % (
        m["title"].replace("\\", "\\\\").replace('"', '\\"'), w, h, KINDS[m["kind"]],
        m["sky"], m["music"], m["ambient"]))
    out.append("    cells_%s, %s, %d\n" % (name, ("things_" + name) if things else "0", len(things)))
    out.append("};\n")
    return "".join(out)


def compile_file(path, out_root, check, verbose):
    m = parse_map(path)
    w, h, cells, things = build(m)
    text = emit(m, w, h, cells, things)
    demons = sum(1 for t in things if t.type in DEMONS)
    if verbose:
        print("mapc: %s -> map_%s.c (%dx%d, %d things, %d demons)" % (
            os.path.relpath(path, ROOT), m["name"], w, h, len(things), demons))
    if not check:
        dst_dir = os.path.join(out_root, "core", "gen")
        os.makedirs(dst_dir, exist_ok=True)
        dst = os.path.join(dst_dir, "map_%s.c" % m["name"])
        with open(dst, "w", encoding="ascii", newline="\n") as f:
            f.write(text)
    return m["name"]


def main(argv):
    out_root = ROOT
    check = verbose = False
    files = []
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--out-root" and i + 1 < len(argv):
            out_root = os.path.abspath(argv[i + 1])
            i += 2
            continue
        if a == "--check":
            check = True
        elif a in ("-v", "--verbose"):
            verbose = True
        elif a in ("-h", "--help"):
            print(__doc__)
            return 0
        else:
            files.append(os.path.abspath(a))
        i += 1
    if not files:
        if not os.path.isdir(MAPS_DIR):
            return 0
        files = sorted(os.path.join(MAPS_DIR, f) for f in os.listdir(MAPS_DIR) if f.endswith(".txt"))
    names = set()
    try:
        for f in files:
            n = compile_file(f, out_root, check, verbose)
            if n in names:
                raise MapError("%s: map name '%s' used twice" % (f, n))
            names.add(n)
    except MapError as e:
        sys.stderr.write("mapc: error: %s\n" % e)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
