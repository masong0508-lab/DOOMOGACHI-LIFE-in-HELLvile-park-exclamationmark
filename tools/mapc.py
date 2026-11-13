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
--export DIR    also write DIR/<name>.mapx: the compiled map as plain numbers
                (the input of core/tools/mapreach.c; tools/levelcheck.py)
FILE.txt        compile only these files (default: every core/data/maps/*.txt)

Format v2 (zones, paths, swap sets, texbank, tflags2, level header keys) is
emitted as a `DoomoMapExt map_ext_<name>` next to the map definition. A map
that uses none of the v2 statements is a v1 map: its generated file is
byte for byte what it was before v2 existed.

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
    "secret": ENUMS["THF_SECRET"], "heart": ENUMS["THF_MACHINE_HEART"],
}
KINDS = {"overworld": "MAP_OVERWORLD", "dungeon": "MAP_DUNGEON", "arena": "MAP_ARENA"}
DEMONS = set(range(ENUMS["THING_IMPLING"], ENUMS["THING_LORD_VAPULA"] + 1))

# ---- format v2 (map.h "levels additions") ----
ZONE_MAX = 32
PATH_MAX = 8
PATH_POINTS = 32
SWAP_MAX = 16
SWAP_CELLS_MAX = 256
TEXBANK_MAX = 8
TEXBANK_FIRST = 56
TEXBANK_NAME = 12
SCRIPT_MSG = "'on EVENT do ACTION' needs the script VM (LV2): mapc does not compile level scripts yet"


def _enum_table(prefix, skip=()):
    out = {}
    for k, v in ENUMS.items():
        if k.startswith(prefix) and k not in skip:
            out[k[len(prefix):].lower()] = v
    return out


ZONE_KINDS = _enum_table("ZONE_", ("ZONE_NONE", "ZONE_COUNT"))
PATH_KINDS = _enum_table("MPATH_", ("MPATH_COUNT",))
DETAILS = _enum_table("MDET_", ("MDET_NONE", "MDET_COUNT"))
THING_FLAGS2 = _enum_table("THF2_")
# Zone pairs that may not share a cell. Two zones of the SAME kind may
# never overlap (the cell's arg would be ambiguous).
ZONE_EXCLUSIVE = [("face_a", "face_b"), ("safe", "chase")]
# 7.2 of the levels plan: the GBA budgets a `gba yes` map must keep
GBA_MAX_W = 40
GBA_MAX_H = 40
GBA_ROOFED_PCT = 75
GBA_SKY_SPAN = 8
GBA_VIEW = (12, 16)
GBA_THINGS = 120
GBA_ROOM_TEX = 4            # dominant textures per room
GBA_ROOM_TEX_PCT = 75       # ... must cover this share of the room's surfaces
GBA_ROOM_MIN = 16           # rooms smaller than this are not judged


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


def parse_tex(v, allow_sky=False, bank=None):
    if v.lower() in ("0", "none", "-"):
        return 0
    if allow_sky and v.lower() == "sky":
        return TEX_SKY
    if bank and v.lower() in bank:          # a level-local texture (texbank)
        return TEXBANK_FIRST + bank[v.lower()]
    n = enum_lookup("TEX", v, "texture")
    if bank and TEXBANK_FIRST <= n < TEXBANK_FIRST + TEXBANK_MAX and (n - TEXBANK_FIRST) in bank.values():
        return n
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
    __slots__ = ("x", "y", "type", "angle", "arg", "flags", "line", "flags2")

    def __init__(self, x, y, typ, angle=0, arg=0, flags=0, line=0, flags2=0):
        self.x, self.y, self.type = x, y, typ
        self.angle, self.arg, self.flags, self.line = angle, arg, flags, line
        self.flags2 = flags2


def thing_from_kv(kv, x, y, line):
    typ, angle, arg, flags, flags2 = None, 0, 0, 0, 0
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
        elif k == "tflags2":
            flags2 = parse_flags(v, THING_FLAGS2, "thing (tflags2)")
        else:
            raise MapError("unknown thing key '%s'" % k)
    if typ is None:
        raise MapError("thing without a type")
    return Thing(x, y, typ, angle, arg, flags, line, flags2)


def parse_yes_no(toks, kw):
    if len(toks) != 2 or toks[1].lower() not in ("yes", "no"):
        raise MapError("%s yes|no" % kw)
    return toks[1].lower() == "yes"


# ---------------------------------------------------------------------------
# format v2 statements
V2_KEYWORDS = ("format", "fog", "view", "gravity", "dark", "note", "gba", "detail", "hero",
               "zone", "path", "swap", "texbank", "on")


def parse_v2(m, kw, toks, lineno):
    """One v2 statement. Returns True if it opens a `swap` block."""
    hdr = m["hdr"]
    if kw == "on":
        raise MapError(SCRIPT_MSG)
    if kw != "zone" and kw != "path" and kw != "swap" and kw != "texbank":
        if kw in hdr:
            raise MapError("'%s' given twice" % kw)
    if kw == "format":
        if len(toks) != 2 or toks[1] != "2":
            raise MapError("format 2")
        hdr["format"] = 2
    elif kw == "fog":
        if len(toks) != 2:
            raise MapError("fog N (fog distance in cells, 1..40)")
        hdr["fog"] = parse_int(toks[1], 1, 40, "fog")
    elif kw == "view":
        if len(toks) != 2:
            raise MapError("view N (view distance in cells, 4..40)")
        hdr["view"] = parse_int(toks[1], 4, 40, "view")
    elif kw == "gravity":
        if len(toks) != 2:
            raise MapError("gravity N (percent of normal, 10..250)")
        hdr["gravity"] = parse_int(toks[1], 10, 250, "gravity")
    elif kw == "dark":
        hdr["dark"] = parse_yes_no(toks, "dark")
    elif kw == "gba":
        hdr["gba"] = parse_yes_no(toks, "gba")
    elif kw == "note":
        if len(toks) != 2:
            raise MapError("note TEXTID (the content note's number, 1..65535)")
        hdr["note"] = parse_int(toks[1], 1, 65535, "note")
    elif kw == "detail":
        names = [n.strip().lower() for n in " ".join(toks[1:]).replace(" ", "").split(",") if n.strip()]
        if not 1 <= len(names) <= 3:
            raise MapError("detail a,b,c (1 to 3 of: %s)" % ", ".join(sorted(DETAILS)))
        for n in names:
            if n not in DETAILS:
                raise MapError("unknown detail '%s' (have: %s)" % (n, ", ".join(sorted(DETAILS))))
        if len(set(names)) != len(names):
            raise MapError("detail names something twice")
        hdr["detail"] = [DETAILS[n] for n in names]
    elif kw == "hero":
        if len(toks) != 3:
            raise MapError("hero LOOK LOADOUT (two numbers 0..255)")
        hdr["hero"] = (parse_int(toks[1], 0, 255, "hero look"), parse_int(toks[2], 0, 255, "hero loadout"))
    elif kw == "zone":
        if len(toks) < 6:
            raise MapError("zone X0 Y0 X1 Y1 KIND [arg=N] [tag=N]")
        x0 = parse_int(toks[1], 0, 255, "zone x0")
        y0 = parse_int(toks[2], 0, 255, "zone y0")
        x1 = parse_int(toks[3], 0, 255, "zone x1")
        y1 = parse_int(toks[4], 0, 255, "zone y1")
        kind = toks[5].lower()
        if kind not in ZONE_KINDS:
            raise MapError("unknown zone kind '%s' (have: %s)" % (toks[5], ", ".join(sorted(ZONE_KINDS))))
        z = {"x0": x0, "y0": y0, "x1": x1, "y1": y1, "kind": kind, "arg": 0, "tag": 0, "line": lineno}
        for k, v in parse_kv(toks[6:]):
            if k in ("arg", "tag"):
                z[k] = parse_int(v, 0, 255, "zone " + k)
            else:
                raise MapError("unknown zone key '%s'" % k)
        m["zones"].append(z)
    elif kw == "path":
        if len(toks) < 2:
            raise MapError("path ID [kind=KIND] x,y x,y ...")
        p = {"id": parse_int(toks[1], 0, 255, "path id"), "kind": "route", "pts": [], "line": lineno}
        for t in toks[2:]:
            if "=" in t:
                k, v = t.split("=", 1)
                if k.lower() != "kind":
                    raise MapError("unknown path key '%s'" % k)
                if v.lower() not in PATH_KINDS:
                    raise MapError("unknown path kind '%s' (have: %s)" % (v, ", ".join(sorted(PATH_KINDS))))
                p["kind"] = v.lower()
            else:
                xy = t.split(",")
                if len(xy) != 2:
                    raise MapError("path point '%s' is not x,y" % t)
                p["pts"].append((parse_int(xy[0], 0, 255, "path x"), parse_int(xy[1], 0, 255, "path y")))
        m["paths"].append(p)
    elif kw == "swap":
        if len(toks) < 2 or not re.match(r"^[a-z_][a-z0-9_]*$", toks[1]):
            raise MapError("swap NAME [event=N] [toggle]   (NAME is a lowercase identifier)")
        sw = {"name": toks[1], "event": 0, "mode": "script", "rows": [], "line": lineno}
        for t in toks[2:]:
            if t.lower() == "toggle":
                sw["mode"] = "toggle"
            elif t.lower().startswith("event="):
                sw["event"] = parse_int(t.split("=", 1)[1], 1, 255, "swap event")
                if sw["mode"] == "script":
                    sw["mode"] = "once"
            else:
                raise MapError("unknown swap option '%s'" % t)
        if sw["mode"] == "toggle" and not sw["event"]:
            raise MapError("swap %s: toggle needs event=N" % sw["name"])
        m["swaps"].append(sw)
        return True
    elif kw == "texbank":
        if len(toks) != 3:
            raise MapError("texbank N TEXNAME")
        n = parse_int(toks[1], 0, TEXBANK_MAX - 1, "texbank slot")
        name = toks[2].lower()
        if not re.match(r"^[a-z_][a-z0-9_]*$", name) or len(name) >= TEXBANK_NAME:
            raise MapError("texbank name '%s': a lowercase identifier of at most %d characters" % (
                toks[2], TEXBANK_NAME - 1))
        if n in m["texbank"].values():
            raise MapError("texbank slot %d given twice" % n)
        if name in m["texbank"]:
            raise MapError("texbank name '%s' given twice" % name)
        if "TEX_" + name.upper() in ENUMS:
            raise MapError("texbank name '%s' is already a shared texture" % name)
        if TEX_COUNT > TEXBANK_FIRST:
            raise MapError("the shared textures reach into the texbank ids %d..63" % TEXBANK_FIRST)
        m["texbank"][name] = n
    return False


def parse_map(path):
    name = os.path.splitext(os.path.basename(path))[0]
    m = {
        "name": name, "title": name.replace("_", " ").title(), "kind": "dungeon",
        "sky": 0, "music": 0, "ambient": 12, "legend": {}, "things_legend": {},
        "grid": None, "overlays": {}, "things": [], "path": path,
        # format v2
        "v2": False, "hdr": {}, "zones": [], "paths": [], "swaps": [], "texbank": {},
        "texbank_names": {},
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
                    elif block == "swap":
                        m["swaps"][-1]["rows"] = rows
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
                        cell["wall"] = parse_tex(v, bank=m["texbank"])
                    elif k == "floor":
                        cell["floor"] = parse_tex(v, bank=m["texbank"])
                    elif k == "ceil":
                        cell["ceil"] = parse_tex(v, allow_sky=True, bank=m["texbank"])
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
                    elif k in ("thing", "angle", "arg", "tflags", "tflags2"):
                        if k == "tflags2":
                            m["v2"] = True
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
                if any(k == "tflags2" for k, _ in kv):
                    m["v2"] = True
                m["things"].append(thing_from_kv(kv, x, y, lineno))
            elif kw in V2_KEYWORDS:
                m["v2"] = True
                if parse_v2(m, kw, toks, lineno):
                    block, rows = "swap", []
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
    if m["v2"]:
        m["ext"] = build_v2(m, w, h, cells, things)
    return w, h, cells, things


def tex_ok(m, n, allow_sky=False):
    if allow_sky and n == TEX_SKY:
        return True
    if n < TEX_COUNT:
        return True
    return TEXBANK_FIRST <= n < TEXBANK_FIRST + TEXBANK_MAX and (n - TEXBANK_FIRST) in m["texbank"].values()


def check_cell(m, where, c):
    """The rules one cell must keep (the grid's cells and swapped cells)."""
    for k in ("wall", "floor"):
        if not tex_ok(m, c[k]):
            raise MapError("%s: %s texture %d out of range" % (where, k, c[k]))
    if not tex_ok(m, c["ceil"], allow_sky=True):
        raise MapError("%s: ceil texture %d out of range" % (where, c["ceil"]))
    if c["wall"]:
        return
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


def validate(m, w, h, cells, things):
    path = m["path"]
    starts = [t for t in things if t.type == ENUMS["THING_PLAYER_START"]]
    if m["kind"] != "overworld" and len(starts) != 1:
        raise MapError("%s: needs exactly one PLAYER_START (has %d)" % (path, len(starts)))
    if len(starts) > 1:
        raise MapError("%s: more than one PLAYER_START" % path)
    for idx, c in enumerate(cells):
        check_cell(m, "%s: cell (%d,%d)" % (path, idx % w, idx // w), c)
    for t in things:
        if t.x >= w or t.y >= h:
            raise MapError("%s: thing at (%d,%d) outside the map" % (path, t.x, t.y))
        c = cells[t.y * w + t.x]
        if c["wall"]:
            raise MapError("%s: thing %d at (%d,%d) is inside a wall" % (path, t.type, t.x, t.y))
    if len(things) > 255:
        raise MapError("%s: too many things (%d > 255)" % (path, len(things)))


# ---------------------------------------------------------------------------
# format v2: validation and the side tables
CELL_KEYS = ("wall", "floor", "ceil", "fh", "ch", "light", "flags", "tag")
MOVER_BITS = CELL_FLAGS["door"] | CELL_FLAGS["lift"]


def cell_bytes(c):
    return [c["wall"] & 255, c["floor"] & 255, c["ceil"] & 255, c["fh"] & 255, c["ch"] & 255,
            c["light"] & 255, c["flags"] & 255, c["tag"] & 255]


def rects_overlap(a, b):
    return a["x0"] <= b["x1"] and b["x0"] <= a["x1"] and a["y0"] <= b["y1"] and b["y0"] <= a["y1"]


def rooms_of(w, h, cells):
    """Connected groups of open cells; a door cell is a room boundary (it
    belongs to no room). Returns a list of lists of cell indices."""
    seen = [False] * (w * h)
    rooms = []
    for start in range(w * h):
        c = cells[start]
        if seen[start] or c["wall"] or (c["flags"] & CELL_FLAGS["door"]):
            continue
        room, stack = [], [start]
        seen[start] = True
        while stack:
            i = stack.pop()
            room.append(i)
            x, y = i % w, i // w
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h:
                    ni = ny * w + nx
                    n = cells[ni]
                    if not seen[ni] and not n["wall"] and not (n["flags"] & CELL_FLAGS["door"]):
                        seen[ni] = True
                        stack.append(ni)
        rooms.append(room)
    return rooms


def gba_rules(m, w, h, cells, things):
    """Section 7.2 of the levels plan, as far as map data can show it.
    Returns a list of plain sentences, empty if the map keeps the budgets.
    Approximations (docs/LEVELS.md "Format v2"): the sky rule measures the
    bounding box of each connected patch of open-sky cells; a room is a
    connected group of open cells bounded by walls and doors, and its
    texture rule counts floor, ceiling and bordering wall faces."""
    out = []
    hdr = m["hdr"]
    if w > GBA_MAX_W or h > GBA_MAX_H:
        out.append("is %dx%d, the GBA takes at most %dx%d" % (w, h, GBA_MAX_W, GBA_MAX_H))
    opened = [c for c in cells if not c["wall"]]
    roofed = sum(1 for c in opened if c["ceil"] != TEX_SKY)
    if opened and roofed * 100 < GBA_ROOFED_PCT * len(opened):
        out.append("only %d%% of its open cells are roofed, the GBA needs %d%%" % (
            roofed * 100 // len(opened), GBA_ROOFED_PCT))
    # open sky: connected patches of sky cells
    seen = [False] * (w * h)
    for start in range(w * h):
        c = cells[start]
        if seen[start] or c["wall"] or c["ceil"] != TEX_SKY:
            continue
        stack, xs, ys = [start], [], []
        seen[start] = True
        while stack:
            i = stack.pop()
            x, y = i % w, i // w
            xs.append(x)
            ys.append(y)
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h:
                    ni = ny * w + nx
                    if not seen[ni] and not cells[ni]["wall"] and cells[ni]["ceil"] == TEX_SKY:
                        seen[ni] = True
                        stack.append(ni)
        sw, sh = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
        if sw > GBA_SKY_SPAN or sh > GBA_SKY_SPAN:
            out.append("has a %dx%d patch of open sky at (%d,%d), the GBA takes at most %dx%d" % (
                sw, sh, min(xs), min(ys), GBA_SKY_SPAN, GBA_SKY_SPAN))
    if "view" not in hdr:
        out.append("has no 'view N': a GBA level sets its view distance (%d..%d cells)" % GBA_VIEW)
    elif not GBA_VIEW[0] <= hdr["view"] <= GBA_VIEW[1]:
        out.append("has view %d, the GBA needs %d..%d" % (hdr["view"], GBA_VIEW[0], GBA_VIEW[1]))
    if len(things) > GBA_THINGS:
        out.append("has %d things, the GBA takes at most %d" % (len(things), GBA_THINGS))
    for room in rooms_of(w, h, cells):
        if len(room) < GBA_ROOM_MIN:
            continue
        uses = {}
        for i in room:
            c = cells[i]
            x, y = i % w, i // w
            uses[c["floor"]] = uses.get(c["floor"], 0) + 1
            if c["ceil"] != TEX_SKY:
                uses[c["ceil"]] = uses.get(c["ceil"], 0) + 1
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h and cells[ny * w + nx]["wall"]:
                    t = cells[ny * w + nx]["wall"]
                    uses[t] = uses.get(t, 0) + 1
        counts = sorted(uses.values(), reverse=True)
        total = sum(counts)
        top = sum(counts[:GBA_ROOM_TEX])
        if top * 100 < GBA_ROOM_TEX_PCT * total:
            out.append("the room at (%d,%d) uses %d textures and its %d most used cover only %d%% "
                       "(the GBA's texture cache holds %d: they must cover %d%%)" % (
                           room[0] % w, room[0] // w, len(counts), GBA_ROOM_TEX, top * 100 // total,
                           GBA_ROOM_TEX, GBA_ROOM_TEX_PCT))
    return out


def build_v2(m, w, h, cells, things):
    """Validate the v2 statements and return the side tables."""
    path = m["path"]
    hdr = m["hdr"]
    start = None
    for t in things:
        if t.type == ENUMS["THING_PLAYER_START"]:
            start = t.y * w + t.x

    # ---- swap sets (first: paths and the wall rule need the swapped cells)
    if len(m["swaps"]) > SWAP_MAX:
        raise MapError("%s: %d swap sets, at most %d" % (path, len(m["swaps"]), SWAP_MAX))
    names = set()
    owner = {}             # cell index -> set name
    swapped = {}           # cell index -> swapped cell
    swaps = []
    total = 0
    for sw in m["swaps"]:
        where = "%s:%d: swap %s" % (path, sw["line"], sw["name"])
        if sw["name"] in names:
            raise MapError("%s: name used twice" % where)
        names.add(sw["name"])
        rows = sw["rows"]
        if len(rows) > h:
            raise MapError("%s has %d rows, the grid only %d" % (where, len(rows), h))
        over = []
        for y, r in enumerate(rows):
            if len(r) > w:
                raise MapError("%s row %d too wide" % (where, y))
            for x, ch in enumerate(r):
                if ch in " .":
                    continue
                if ch not in m["legend"]:
                    raise MapError("%s (%d,%d): no cell legend for '%s'" % (where, x, y, ch))
                if ch in m["things_legend"]:
                    raise MapError("%s (%d,%d): legend '%s' places a thing; a swap changes cells only" % (
                        where, x, y, ch))
                i = y * w + x
                base, new = cells[i], dict(m["legend"][ch])
                at = "%s (%d,%d)" % (where, x, y)
                if cell_bytes(base) == cell_bytes(new):
                    raise MapError("%s: the override equals the cell it replaces" % at)
                if (base["flags"] | new["flags"]) & MOVER_BITS:
                    raise MapError("%s: a swap may not touch a door or a lift" % at)
                if i in owner:
                    raise MapError("%s: the cell is already in swap %s" % (at, owner[i]))
                check_cell(m, at, new)
                if new["wall"] and i == start:
                    raise MapError("%s: walls in the PLAYER_START" % at)
                if not new["wall"] and m["kind"] != "overworld" and (x in (0, w - 1) or y in (0, h - 1)):
                    raise MapError("%s: opens a cell on the map's border" % at)
                owner[i] = sw["name"]
                swapped[i] = new
                over.append((i, [a ^ b for a, b in zip(cell_bytes(base), cell_bytes(new))]))
        if not over:
            raise MapError("%s: no cell overrides" % where)
        total += len(over)
        swaps.append({"name": sw["name"], "mode": sw["mode"], "event": sw["event"], "cells": over})
    if total > SWAP_CELLS_MAX:
        raise MapError("%s: %d swap cell overrides in all, at most %d" % (path, total, SWAP_CELLS_MAX))

    def ever_open(i):
        return not cells[i]["wall"] or (i in swapped and not swapped[i]["wall"])

    # a wall that stays a wall and that no open cell ever touches is never
    # seen: an override on it is a mistake ("walls the level never opens")
    for i in sorted(swapped):
        if not (cells[i]["wall"] and swapped[i]["wall"]):
            continue
        x, y = i % w, i // w
        seen = False
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < w and 0 <= ny < h and ever_open(ny * w + nx):
                seen = True
        if not seen:
            raise MapError("%s: swap %s (%d,%d): the override is on a wall the level never opens "
                           "(it stays a wall and no open cell ever touches it)" % (path, owner[i], x, y))

    # ---- zones
    if len(m["zones"]) > ZONE_MAX:
        raise MapError("%s: %d zones, at most %d" % (path, len(m["zones"]), ZONE_MAX))
    excl = set()
    for a, b in ZONE_EXCLUSIVE:
        excl.add((a, b))
        excl.add((b, a))
    for zi, z in enumerate(m["zones"]):
        where = "%s:%d: zone" % (path, z["line"])
        if z["x0"] > z["x1"] or z["y0"] > z["y1"]:
            raise MapError("%s: X0 Y0 must be the top left corner, X1 Y1 the bottom right" % where)
        if z["x1"] >= w or z["y1"] >= h:
            raise MapError("%s (%d,%d)-(%d,%d) is outside the %dx%d map" % (
                where, z["x0"], z["y0"], z["x1"], z["y1"], w, h))
        if z["kind"] == "gravity" and not 1 <= z["arg"] <= 250:
            raise MapError("%s: gravity needs arg=1..250 (percent of normal)" % where)
        if z["kind"] == "bounce" and not 1 <= z["arg"] <= 127:
            raise MapError("%s: bounce needs arg=1..127 (rise in height steps)" % where)
        for o in m["zones"][:zi]:
            if not rects_overlap(z, o):
                continue
            if o["kind"] == z["kind"]:
                raise MapError("%s overlaps the %s zone of line %d (two zones of one kind may not overlap)" % (
                    where, o["kind"], o["line"]))
            if (o["kind"], z["kind"]) in excl:
                raise MapError("%s: a %s zone may not overlap the %s zone of line %d" % (
                    where, z["kind"], o["kind"], o["line"]))

    # ---- paths
    if len(m["paths"]) > PATH_MAX:
        raise MapError("%s: %d paths, at most %d" % (path, len(m["paths"]), PATH_MAX))
    ids = set()
    for p in m["paths"]:
        where = "%s:%d: path %d" % (path, p["line"], p["id"])
        if p["id"] in ids:
            raise MapError("%s: id used twice" % where)
        ids.add(p["id"])
        if not 2 <= len(p["pts"]) <= PATH_POINTS:
            raise MapError("%s has %d points, needs 2..%d" % (where, len(p["pts"]), PATH_POINTS))
        for x, y in p["pts"]:
            if x >= w or y >= h:
                raise MapError("%s: point %d,%d is outside the %dx%d map" % (where, x, y, w, h))
            if not ever_open(y * w + x):
                raise MapError("%s: point %d,%d is inside a wall that never opens" % (where, x, y))

    # ---- header
    if hdr.get("dark") and "note" not in hdr:
        raise MapError("%s: a 'dark yes' map needs a content note ('note TEXTID')" % path)
    if hdr.get("gba"):
        broke = gba_rules(m, w, h, cells, things)
        if broke:
            raise MapError("%s: 'gba yes' but the map %s" % (path, "; ".join(broke)))

    bank = [None] * TEXBANK_MAX
    for name, n in m["texbank"].items():
        bank[n] = name
    nbank = max([n for n in m["texbank"].values()] + [-1]) + 1
    return {"swaps": swaps, "bank": bank[:nbank]}


def emit_v2(m, things):
    """The side tables and `map_ext_<name>` (appended after the definition)."""
    name = m["name"]
    hdr = m["hdr"]
    ext = m["ext"]
    out = ["\n/* ---- format v2: the side tables (map.h DoomoMapExt) ---- */\n"]
    zones, paths, swaps, bank = m["zones"], m["paths"], ext["swaps"], ext["bank"]
    if zones:
        out.append("/* {x0, y0, x1, y1, kind, arg, tag} */\n")
        out.append("static const DoomoZone zones_%s[%d] = {\n" % (name, len(zones)))
        for z in zones:
            out.append("    {%d, %d, %d, %d, %d, %d, %d},   /* %s */\n" % (
                z["x0"], z["y0"], z["x1"], z["y1"], ZONE_KINDS[z["kind"]], z["arg"], z["tag"], z["kind"]))
        out.append("};\n\n")
    if paths:
        npts = sum(len(p["pts"]) for p in paths)
        out.append("static const uint8_t pathpts_%s[%d] = {\n" % (name, npts * 2))
        for p in paths:
            out.append("    %s,   /* path %d */\n" % (", ".join("%d,%d" % xy for xy in p["pts"]), p["id"]))
        out.append("};\n")
        out.append("/* {id, kind, count, reserved, first} */\n")
        out.append("static const DoomoPath paths_%s[%d] = {\n" % (name, len(paths)))
        first = 0
        for p in paths:
            out.append("    {%d, %d, %d, 0, %d},   /* %s */\n" % (
                p["id"], PATH_KINDS[p["kind"]], len(p["pts"]), first, p["kind"]))
            first += len(p["pts"])
        out.append("};\n\n")
    if swaps:
        ncells = sum(len(sw["cells"]) for sw in swaps)
        out.append("/* {cell, base XOR swapped} */\n")
        out.append("static const DoomoSwapCell swapcells_%s[%d] = {\n" % (name, ncells))
        for sw in swaps:
            out.append("    /* %s */\n" % sw["name"])
            for i, x in sw["cells"]:
                out.append("    {%d, {%s}},\n" % (i, ",".join(str(v) for v in x)))
        out.append("};\n")
        out.append("/* {first, count, mode, event} */\n")
        out.append("static const DoomoSwapSet swaps_%s[%d] = {\n" % (name, len(swaps)))
        first = 0
        for sw in swaps:
            out.append("    {%d, %d, %s, %d},   /* %s */\n" % (
                first, len(sw["cells"]), "MSWAP_" + sw["mode"].upper(), sw["event"], sw["name"]))
            first += len(sw["cells"])
        out.append("};\n\n")
    if bank:
        out.append("static const char texbank_%s[%d][DOOMO_TEXBANK_NAME] = {\n" % (name, len(bank)))
        for i, b in enumerate(bank):
            out.append('    "%s",   /* texture id %d */\n' % (b or "", TEXBANK_FIRST + i))
        out.append("};\n\n")
    has_f2 = any(t.flags2 for t in things)
    if has_f2:
        out.append("static const uint8_t tflags2_%s[%d] = {\n" % (name, len(things)))
        for i in range(0, len(things), 16):
            out.append("    %s,\n" % ",".join(str(t.flags2) for t in things[i:i + 16]))
        out.append("};\n\n")
    flags = []
    if hdr.get("dark"):
        flags.append("MAPX_DARK")
    if hdr.get("gba"):
        flags.append("MAPX_GBA")
    if "hero" in hdr:
        flags.append("MAPX_HERO")
    det = (hdr.get("detail", []) + [0, 0, 0])[:3]
    hero = hdr.get("hero", (0, 0))
    out.append("const DoomoMapExt map_ext_%s = {\n" % name)
    out.append("    &map_def_%s,\n" % name)
    out.append("    %s, %d, %d, %d, %d,   /* flags, fog, view, gravity, note */\n" % (
        " | ".join(flags) if flags else "0", hdr.get("fog", 0), hdr.get("view", 0),
        hdr.get("gravity", 0), hdr.get("note", 0)))
    out.append("    {%d, %d, %d}, %d, %d,   /* detail, hero look, hero loadout */\n" % (
        det[0], det[1], det[2], hero[0], hero[1]))
    out.append("    %d, %d, %d, %d,   /* zones, paths, swap sets, texbank */\n" % (
        len(zones), len(paths), len(swaps), len(bank)))
    out.append("    %s, %s, %s,\n" % (
        ("zones_" + name) if zones else "0", ("paths_" + name) if paths else "0",
        ("pathpts_" + name) if paths else "0"))
    out.append("    %s, %s,\n" % (
        ("swaps_" + name) if swaps else "0", ("swapcells_" + name) if swaps else "0"))
    out.append("    %s, %s\n" % (
        ("texbank_" + name) if bank else "0", ("tflags2_" + name) if has_f2 else "0"))
    out.append("};\n")
    return "".join(out)


def export_text(m, w, h, cells, things):
    """The compiled map as plain numbers: what core/tools/mapreach.c reads
    (the reachability search of core/tools/map_bfs.h, shared with
    core/tests/test_maps.c). One format for v1 and v2 maps."""
    out = ["doomomap 2\n", "name %s\n" % m["name"]]
    out.append("size %d %d %d\n" % (w, h, ("overworld", "dungeon", "arena").index(m["kind"])))
    for y in range(h):
        row = []
        for x in range(w):
            row.extend(cell_bytes(cells[y * w + x]))
        out.append(" ".join(str(v) for v in row) + "\n")
    out.append("things %d\n" % len(things))
    for t in things:
        out.append("%d %d %d %d %d %d %d\n" % (t.x, t.y, t.type, t.angle, t.arg, t.flags, t.flags2))
    if not m["v2"]:
        out.append("ext 0\n")
        return "".join(out)
    hdr, ext = m["hdr"], m["ext"]
    flags = (1 if hdr.get("dark") else 0) | (2 if hdr.get("gba") else 0) | (4 if "hero" in hdr else 0)
    out.append("ext 1\n")
    out.append("hdr %d %d %d %d %d\n" % (flags, hdr.get("fog", 0), hdr.get("view", 0),
                                         hdr.get("gravity", 0), hdr.get("note", 0)))
    out.append("zones %d\n" % len(m["zones"]))
    for z in m["zones"]:
        out.append("%d %d %d %d %d %d %d\n" % (z["x0"], z["y0"], z["x1"], z["y1"],
                                               ZONE_KINDS[z["kind"]], z["arg"], z["tag"]))
    out.append("paths %d\n" % len(m["paths"]))
    for p in m["paths"]:
        out.append("%d %d %d %s\n" % (p["id"], PATH_KINDS[p["kind"]], len(p["pts"]),
                                      " ".join("%d %d" % xy for xy in p["pts"])))
    out.append("swaps %d\n" % len(ext["swaps"]))
    for sw in ext["swaps"]:
        out.append("%d %d %d\n" % (ENUMS["MSWAP_" + sw["mode"].upper()], sw["event"], len(sw["cells"])))
        for i, x in sw["cells"]:
            out.append("%d %s\n" % (i, " ".join(str(v) for v in x)))
    return "".join(out)


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
    if m["v2"]:
        out.append(emit_v2(m, things))
    return "".join(out)


def compile_file(path, out_root, check, verbose, export=None):
    m = parse_map(path)
    w, h, cells, things = build(m)
    text = emit(m, w, h, cells, things)
    if export:
        os.makedirs(export, exist_ok=True)
        with open(os.path.join(export, "%s.mapx" % m["name"]), "w", encoding="ascii", newline="\n") as f:
            f.write(export_text(m, w, h, cells, things))
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
    export = None
    files = []
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--out-root" and i + 1 < len(argv):
            out_root = os.path.abspath(argv[i + 1])
            i += 2
            continue
        if a == "--export" and i + 1 < len(argv):
            export = os.path.abspath(argv[i + 1])
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
            n = compile_file(f, out_root, check, verbose, export)
            if n in names:
                raise MapError("%s: map name '%s' used twice" % (f, n))
            names.add(n)
    except MapError as e:
        sys.stderr.write("mapc: error: %s\n" % e)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
