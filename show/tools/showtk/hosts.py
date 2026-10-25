"""hosts.py - the stage-kind hosts' compiler half (owner: show, ENGINE lane 3; spec 8.3).

The game's own renderers as show stage kinds: OVERWORLD (plat_overworld on the
game's overworld maps), ROOM (the 2.5D house renderer) and VIEW3D (first person
on the game's 3D maps, with cam3d rails, monsters, gore by the cute/brutal
filter and the HUD). This module turns a host set block plus a shot's `world`
statements into the binary record show/engine/show_host.c plays
(show/include/show/show_hosts.h is the format; this file mirrors it, and
show/tests/engine/hosts/test_hosts.py checks the two agree).

The plug-in contract for the compiler (show/README.md "Plug-in API"):

    hosts.HOST_KINDS                 {"overworld", "room", "view3d"}
    hosts.is_host_statement(text)    True for `world ...` and `cam3d ...` lines
    hosts.pack_plane(set_block, shot_lines, shot_len, cast=None, seed=0,
                     gore="CUTE", kids=False, npc_rid=600)  -> bytes (the plane payload)

set_block and shot_lines are lists of Line(file, line, text): the set's lines
(from `set ...` to `end`) and the shot's statements with their time prefix
already resolved to "+N" frames (or left as +N / +Ns / +R8, which this module
reads too). cast maps a cast id (who=sunny) to its doomo_town.res[] slot.

Run on its own (no compiler needed):

    cd show/tools && python3 -m showtk.hosts check FILE.shw...    lint the host shots
    cd show/tools && python3 -m showtk.hosts dump FILE.shw        print each record
    cd show/tools && python3 -m showtk.hosts cgen -o out.c FILE.shw...   the test table
"""
import argparse
import math
import os
import re
import struct
import sys
from collections import namedtuple

HERE = os.path.dirname(os.path.abspath(__file__))
SHOW = os.path.normpath(os.path.join(HERE, "..", ".."))
REPO = os.path.dirname(SHOW)
INC = os.path.join(REPO, "core", "include", "doomo")
HOSTS_H = os.path.join(SHOW, "include", "show", "show_hosts.h")

Line = namedtuple("Line", "file line text")

HOST_KINDS = {"overworld", "room", "view3d"}
CELL = 64                       # world units per map cell (map.h DOOMO_CELL_UNITS)
FIX = 65536
EYE = 40                        # DOOMO_EYE_HEIGHT


class HostError(Exception):
    """file:line: shot +frame [RULE] message (spec 13.4)"""

    def __init__(self, rule, msg, where=None, shot=None, frame=None):
        self.rule, self.msg, self.where, self.shot, self.frame = rule, msg, where, shot, frame
        super().__init__(self.format())

    def format(self):
        head = ""
        if self.where is not None:
            head = "%s:%d: " % (self.where.file, self.where.line)
        if self.shot:
            head += self.shot + (" +%d " % self.frame if self.frame is not None else " ")
        return "%s[%s] %s" % (head, self.rule, self.msg)


# ---- symbols: the game's own enums (like textpack.load_symbols) ------------------
_SYMS = None
_HEADERS = ["sprites.h", "map.h", "camfx.h", "palette.h", "portrait.h", "catalog.h", "town.h",
            "actor.h"]


def _strip(src):
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    return re.sub(r"//[^\n]*", " ", src)


def _eval(expr, syms):
    expr = re.sub(r"\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]+\b", r"\1", expr.strip())
    names = re.findall(r"[A-Za-z_]\w*", re.sub(r"\b0[xX][0-9a-fA-F]+\b", "0", expr))
    env = {}
    for n in names:
        if n not in syms:
            return None
        env[n] = syms[n]
    if re.search(r"[^\w\s\+\-\*/\(\)<>\|&x]", expr):
        return None
    try:
        return int(eval(expr.replace("/", "//"), {"__builtins__": {}}, env))
    except Exception:
        return None


def _scan(path, syms):
    if not os.path.exists(path):
        return
    src = _strip(open(path, encoding="latin-1").read())
    for m in re.finditer(r"#define[ \t]+([A-Z_][A-Z0-9_]*)[ \t]+([^\n(][^\n]*)", src):
        v = _eval(m.group(2), syms)
        if v is not None:
            syms[m.group(1)] = v
    for m in re.finditer(r"enum\s*\w*\s*\{(.*?)\}", src, flags=re.S):
        cur = -1
        for item in m.group(1).split(","):
            item = item.strip()
            if not item:
                continue
            if "=" in item:
                name, val = item.split("=", 1)
                name = name.strip()
                v = _eval(val, syms)
                cur = v if v is not None else cur + 1
            else:
                name = item
                cur += 1
            if re.match(r"^[A-Za-z_]\w*$", name):
                syms[name] = cur


def symbols():
    global _SYMS
    if _SYMS is None:
        s = {}
        for h in _HEADERS:
            _scan(os.path.join(INC, h), s)
        _scan(os.path.join(REPO, "core", "gen", "furn_ids.h"), s)
        _scan(HOSTS_H, s)
        _SYMS = s
    return _SYMS


def sym(name, prefix=None, where=None):
    s = symbols()
    if isinstance(name, int):
        return name
    n = name.strip()
    if re.match(r"^-?\d+$", n):
        return int(n)
    if prefix and not n.startswith(prefix) and (prefix + n.upper()) in s:
        return s[prefix + n.upper()]
    if n in s:
        return s[n]
    if n.upper() in s:
        return s[n.upper()]
    raise HostError("SHW-SYM", "unknown symbol %s%s" % (n, " (want %s*)" % prefix if prefix else ""), where)


# the format's constants, read from the C header (they cannot disagree)
def C(name):
    return symbols()[name]


# ---- record layout (show_hosts.h) ---------------------------------------------------
FMT = {
    "Hdr":   "<IHBBIHBBI",          # 20
    "Sec":   "<HHI",                # 8
    "World": "<BBBBhhhhBBBBBBBBBBBBBB2x",   # 28
    "Key":   "<HBBiiihh",           # 20
    "Actor": "<BBHHHBBHB3x",        # 16
    "Event": "<HBBhhi",             # 12
    "Furn":  "<HBBBBBBbbbB",        # 12
    "Npc":   "<BBHIhhhhBBBB",       # 20
    "Hud":   "<12B" + "%dH" % 0,    # filled below
}
SIZES = {"Hdr": 20, "Sec": 8, "World": 28, "Key": 20, "Actor": 16, "Event": 12, "Furn": 12, "Npc": 20}


def _hud_fmt():
    n = C("SWPN_COUNT")
    return "<12B%dH%dHHHHH" % (n, n)


def check_layout():
    """the Python mirror's sizes (the C side is checked by test_hosts.py)"""
    for k, n in SIZES.items():
        got = struct.calcsize(FMT[k])
        if got != n:
            raise AssertionError("%s: %d bytes, want %d" % (k, got, n))
    return {k: struct.calcsize(FMT[k]) for k in SIZES} | {"Hud": struct.calcsize(_hud_fmt())}


# ---- the statement language -----------------------------------------------------------
_RUNG = {"R2": 2, "R3": 3, "R5": 5, "R8": 8, "R13": 13, "R21": 21, "R26": 26}


def parse_time(tok, where=None):
    """+N | +Ns | +1.5s | +R8 -> frame (30 fps)"""
    t = tok.strip()
    if not t.startswith("+"):
        raise HostError("SHW-TIME", "a time must start with + (got %s)" % tok, where)
    t = t[1:]
    if t in _RUNG:
        return _RUNG[t]
    m = re.match(r"^(\d+(?:\.\d+)?)s$", t)
    if m:
        f = float(m.group(1)) * 30
        if abs(f - round(f)) > 1e-6:
            raise HostError("SHW-TIME", "%s is not a whole frame count at 30 fps" % tok, where)
        return int(round(f))
    if re.match(r"^\d+$", t):
        return int(t)
    raise HostError("SHW-TIME", "cannot read the time %s" % tok, where)


def split_words(text):
    """words; "quoted strings" stay whole; # starts a comment"""
    out, i, n = [], 0, len(text)
    while i < n:
        while i < n and text[i].isspace():
            i += 1
        if i >= n or text[i] == "#":
            break
        j = i
        buf = []
        while j < n and not text[j].isspace():
            if text[j] == '"':
                k = text.index('"', j + 1) if '"' in text[j + 1:] else n
                buf.append(text[j + 1:k])
                j = k + 1
            else:
                buf.append(text[j])
                j += 1
        out.append("".join(buf))
        i = j
    return out


def kv(words):
    """k=v words -> dict; bare words -> list"""
    d, bare = {}, []
    for w in words:
        if "=" in w:
            k, v = w.split("=", 1)
            d[k.lower()] = v
        else:
            bare.append(w)
    return d, bare


def nums(s, n=None, where=None):
    try:
        v = [float(x) for x in s.split(",")]
    except ValueError:
        raise HostError("SHW-ARG", "want numbers, got %s" % s, where)
    if n and len(v) not in (n if isinstance(n, tuple) else (n,)):
        raise HostError("SHW-ARG", "want %s numbers, got %s" % (n, s), where)
    return v


def yaw_of(s, where=None):
    """compass or degrees (0 = east, 90 = south: the map's y grows south) -> 0..4095"""
    c = {"E": 0, "SE": 45, "S": 90, "SW": 135, "W": 180, "NW": 225, "N": 270, "NE": 315}
    u = s.strip().upper()
    if u in c:
        deg = c[u]
    else:
        try:
            deg = float(u.rstrip("D"))
        except ValueError:
            raise HostError("SHW-ARG", "a yaw is N/E/S/W or degrees, got %s" % s, where)
    return int(round(deg * 4096 / 360.0)) % 4096


EASES = {"HOLD": 0, "LINEAR": 1, "IN": 2, "OUT": 3, "INOUT": 4}
DIRS = {"D": 0, "DOWN": 0, "L": 1, "LEFT": 1, "R": 2, "RIGHT": 2, "U": 3, "UP": 3, "AUTO": 255}
WEAPONS = {"none": 0, "pump": 1, "saw": 2, "wand": 3}
FACES = {"idle": 0, "left": 1, "right": 2, "grin": 3, "wince": 4, "glare": 5}
NPC_MODES = {"wander": 0, "stand": 1, "lane_e": 2, "lane_w": 3, "lane_n": 4, "lane_s": 5}
ROOM_KINDS = {"walls": 0, "garden": 1, "open": 2}
KINDS = {"chibi": 1, "figure": 2, "sprite": 3, "monster": 4}
MAPS = {"hahaland": 0, "hahaland_invaded": 1, "hellvile": 2, "vape_cellar": 3, "cloud_nine_spire": 4,
        "arena": 5, "park_united": 6, "sugar_rush_sewers": 7, "plushie_factory": 8,
        "graveyard_shift": 9, "bone_cathedral": 10, "hellvile_underbelly": 11}
OW_MAPS = {"hahaland", "hahaland_invaded", "hellvile", "park_united"}


class Plane:
    """one host plane: what the record holds, before packing"""

    def __init__(self, kind):
        self.kind = kind
        self.world = dict(map=0, daylight=15, tint=0, tint_level=0, vx=0, vy=30, vw=320, vh=180,
                          flags=0, fov=1, view_dist=0, room_w=0, room_d=0, wallpaper=0, floor=0,
                          wall_paint=0, floor_paint=0, ceiling=0, room_kind=0, weather=0)
        self.paths = {}         # VIEW3D rails: name -> dict(points, eye, look)
        self.furn = []
        self.cam = []           # (f, ease, flags, a, b, c, d, e)
        self.actors = []        # dicts
        self.npcs = []
        self.events = []        # (f, op, target, a, b, c)
        self.hud = None
        self.strings = bytearray()
        self.str_index = {}
        self.gore = 0
        self.kids = False
        self.seed = 0
        self.len = 0
        self.eye_keys = False

    def string(self, s):
        if s in self.str_index:
            return self.str_index[s]
        off = len(self.strings)
        self.strings += s.encode("latin-1") + b"\0"
        self.str_index[s] = off
        return off


# ---- the set block ------------------------------------------------------------------------
def parse_set(lines):
    """`set <name> kind=overworld|room|view3d k=v ...` ... `end` -> (name, Plane)"""
    if not lines:
        raise HostError("SET-KIND", "empty set block")
    head = split_words(lines[0].text)
    if not head or head[0] != "set" or len(head) < 2:
        raise HostError("SET-KIND", "a set block starts with `set <name> kind=...`", lines[0])
    d, _ = kv(head[2:])
    kind = d.get("kind", "").lower()
    if kind not in HOST_KINDS:
        raise HostError("SET-KIND", "kind=%s is not a host kind (%s)" % (kind, ", ".join(sorted(HOST_KINDS))), lines[0])
    p = Plane(kind)
    w = p.world
    if kind in ("overworld", "view3d"):
        m = d.get("map", "")
        if m not in MAPS:
            raise HostError("SET-MAP", "map=%s is not one of the game's maps (%s)" % (m, ", ".join(sorted(MAPS))),
                            lines[0])
        if kind == "overworld" and m not in OW_MAPS:
            raise HostError("SET-MAP", "%s is a 3D map: use kind=view3d (overworld maps: %s)"
                            % (m, ", ".join(sorted(OW_MAPS))), lines[0])
        if kind == "view3d" and m in OW_MAPS:
            raise HostError("SET-MAP", "%s is an overworld map: use kind=overworld" % m, lines[0])
        w["map"] = MAPS[m]
    if "daylight" in d:
        w["daylight"] = max(0, min(15, int(d["daylight"])))
    if "tint" in d:
        w["tint"] = 0 if d["tint"].lower() == "none" else sym(d["tint"], "TINT_", lines[0])
        w["tint_level"] = int(d.get("tint_level", "6"))
    if "viewport" in d:
        vx, vy, vw, vh = [int(x) for x in nums(d["viewport"], 4, lines[0])]
        w.update(vx=vx, vy=vy, vw=vw, vh=vh)
    if d.get("shadows", "on") == "off":
        w["flags"] |= C("SWF_NO_SHADOWS")
    if d.get("doors", "") == "open":
        w["flags"] |= C("SWF_DOORS_OPEN")
    if "fov" in d:
        w["fov"] = max(0, min(2, int(d["fov"])))
    if "view" in d:
        w["view_dist"] = max(11, min(18, int(d["view"])))
    if kind == "room":
        r = d.get("room", "6x6").lower().split("x")
        w["room_w"], w["room_d"] = int(r[0]), int(r[1])
        if not (1 <= w["room_w"] <= 16 and 1 <= w["room_d"] <= 16):
            raise HostError("SET-ROOM", "room=%s: 1..16 tiles a side" % d.get("room"), lines[0])
        w["room_kind"] = ROOM_KINDS.get(d.get("walls", "walls"), 0)
        w["wallpaper"] = int(d.get("wallpaper", "0"))
        w["floor"] = int(d.get("floor", "0"))
        w["wall_paint"] = sym(d.get("wall_paint", "PAINT_BUBBLEGUM"), "PAINT_", lines[0])
        w["floor_paint"] = sym(d.get("floor_paint", "PAINT_CARAMEL"), "PAINT_", lines[0])
        w["daylight"] = max(0, min(15, int(d.get("daylight", "12"))))
    for ln in lines[1:]:
        ws = split_words(ln.text)
        if not ws:
            continue
        if ws[0] == "end":
            break
        e, bare = kv(ws[1:])
        if ws[0] == "furn" and kind == "room":
            if not bare:
                raise HostError("SET-FURN", "furn needs a FURN_* name", ln)
            fid = sym(bare[0], "FURN_", ln)
            x, y = [int(v) for v in nums(e.get("at", "0,0"), 2, ln)]
            rot = int(e.get("rot", "0")) & 3
            anim = int(e.get("anim", "0")) & 7
            pa = pb = 0xFF
            if "paint" in e:
                ps = e["paint"].split(",")
                pa = sym(ps[0], "PAINT_", ln)
                if len(ps) > 1:
                    pb = sym(ps[1], "PAINT_", ln)
            sq = [int(v) for v in nums(e.get("squash", "0,0"), 2, ln)]
            fl = C("RO_FULLBRIGHT") if e.get("lit") == "on" else 0
            if not (0 <= x < w["room_w"] and 0 <= y < w["room_d"]):
                raise HostError("SET-FURN", "%s at %d,%d is outside the %dx%d room" % (bare[0], x, y,
                                w["room_w"], w["room_d"]), ln)
            p.furn.append((fid, x, y, int(e.get("z", "0")), rot | (anim << 2), pa, pb, sq[0], sq[1], 0, fl))
        elif ws[0] == "path" and kind == "view3d":
            if not bare:
                raise HostError("SET-PATH", "path needs a name and points", ln)
            pts = []
            for b in bare[1:]:
                v = nums(b, (2, 3), ln)
                pts.append(v)
            if len(pts) < 2:
                raise HostError("SET-PATH", "path %s needs two points or more" % bare[0], ln)
            p.paths[bare[0]] = dict(points=pts, eye=float(e.get("eye", EYE)), look=e.get("look", "ahead"),
                                    pitch=int(e.get("pitch", "0")), where=ln)
        elif ws[0] in ("mark", "note", "waive", "prop", "plane"):
            pass                # other packages' set lines: not the host's
        else:
            raise HostError("SET-LINE", "`%s` is not a %s set statement" % (ws[0], kind), ln)
    return head[1], p


# ---- rails: cam3d paths sampled into dense keys --------------------------------------------
def _catmull(p0, p1, p2, p3, u):
    u2, u3 = u * u, u * u * u
    return [0.5 * ((2 * b) + (-a + c) * u + (2 * a - 5 * b + 4 * c - d) * u2 + (-a + 3 * b - 3 * c + d) * u3)
            for a, b, c, d in zip(p0, p1, p2, p3)]


def rail_samples(path, n=256):
    """points along the path (cells), roughly evenly spaced by arc length"""
    pts = [list(v[:2]) for v in path["points"]]
    if len(pts) == 2:
        dense = [[pts[0][0] + (pts[1][0] - pts[0][0]) * i / n, pts[0][1] + (pts[1][1] - pts[0][1]) * i / n]
                 for i in range(n + 1)]
    else:
        ext = [pts[0]] + pts + [pts[-1]]
        dense = []
        seg = len(pts) - 1
        per = max(8, n // seg)
        for s in range(seg):
            for i in range(per):
                dense.append(_catmull(ext[s], ext[s + 1], ext[s + 2], ext[s + 3], i / per))
        dense.append(pts[-1])
    acc = [0.0]
    for a, b in zip(dense, dense[1:]):
        acc.append(acc[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    return dense, acc


def _at_len(dense, acc, s):
    s = max(0.0, min(acc[-1], s))
    lo, hi = 0, len(acc) - 1
    while lo + 1 < hi:
        mid = (lo + hi) // 2
        if acc[mid] <= s:
            lo = mid
        else:
            hi = mid
    span = acc[hi] - acc[lo]
    u = 0.0 if span <= 0 else (s - acc[lo]) / span
    return [dense[lo][0] + (dense[hi][0] - dense[lo][0]) * u, dense[lo][1] + (dense[hi][1] - dense[lo][1]) * u]


def _ease(name, u):
    if name == "IN":
        return u * u
    if name == "OUT":
        return 1 - (1 - u) * (1 - u)
    if name == "INOUT":
        return u * u * (3 - 2 * u)
    return u


def rail_keys(path, f0, f1, ease="LINEAR", step=2, yaw=None, pitch=None):
    """camera keys (f, x, y, eye, yaw) every `step` frames from f0 to f1"""
    dense, acc = rail_samples(path)
    total = acc[-1]
    keys = []
    look = path.get("look", "ahead")
    fixed = None if (look == "ahead" and yaw is None) else yaw_of(yaw if yaw is not None else look)
    prev_yaw = None
    frames = list(range(f0, f1, step)) + [f1]
    for f in frames:
        u = 0.0 if f1 == f0 else (f - f0) / (f1 - f0)
        s = _ease(ease, u) * total
        x, y = _at_len(dense, acc, s)
        if fixed is None:
            ax, ay = _at_len(dense, acc, s - 0.25)
            bx, by = _at_len(dense, acc, s + 0.25)
            if (bx - ax) ** 2 + (by - ay) ** 2 < 1e-9:
                bx, by = dense[-1] if s < total else dense[1]
                ax, ay = (x, y) if s < total else dense[0]
            a = math.atan2(by - ay, bx - ax) * 4096 / (2 * math.pi)
            if prev_yaw is not None:        # unwrapped, so the keys never spin the long way
                while a - prev_yaw > 2048:
                    a -= 4096
                while prev_yaw - a > 2048:
                    a += 4096
            prev_yaw = a
            yw = int(round(a))
        else:
            yw = fixed
        keys.append((f, x, y, path.get("eye", EYE), yw, path.get("pitch", 0) if pitch is None else pitch))
    return keys


# ---- shot statements ------------------------------------------------------------------------
def is_host_statement(text):
    ws = split_words(text)
    if ws and ws[0].startswith("+"):
        ws = ws[1:]
    return bool(ws) and ws[0] in ("world", "cam3d")


def _cell(v):
    return int(round(v * CELL * FIX))


def _where_actor(p, name, where):
    for a in p.actors:
        if a["name"] == name:
            return a
    raise HostError("SHW-ACTOR", "no world actor `%s` in this shot (declare it with `world actor %s ...`)"
                    % (name, name), where)


def _resolve_who(who, cast, where):
    if who.startswith("res:"):
        return int(who[4:])
    if cast and who in cast:
        return int(cast[who])
    if re.match(r"^\d+$", who):
        return int(who)
    raise HostError("SHW-CAST", "who=%s: not in the cast table (use who=res:N for a fixed slot)" % who, where)


def apply_statement(p, ln, cast, shot_len, shot_id="shot"):
    ws = split_words(ln.text)
    if not ws:
        return
    t = None
    if ws[0].startswith("+"):
        t = parse_time(ws[0], ln)
        ws = ws[1:]
    if not ws:
        return
    if ws[0] == "cam3d":
        ws = ["world", "rail"] + ws[1:]
    if ws[0] != "world" or len(ws) < 2:
        return
    op, rest = ws[1].lower(), ws[2:]
    d, bare = kv(rest)
    f = 0 if t is None else t
    if f > shot_len:
        raise HostError("TIME-SHOT", "+%d is after the shot's end (len=%d)" % (f, shot_len), ln, shot_id, f)
    room = p.kind == "room"
    v3d = p.kind == "view3d"

    def pos(s, z=False):
        v = nums(s, (2, 3) if z else 2, ln)
        if room:
            return int(round(v[0] * FIX)), int(round(v[1] * FIX)), (int(v[2]) if len(v) > 2 else 0)
        return _cell(v[0]), _cell(v[1]), (int(round(v[2] * FIX)) if len(v) > 2 else None)

    if op == "cam":
        to = bare and bare[0] == "to"
        if to and t is None:
            raise HostError("SHW-TIME", "`world cam to` needs a time (+N world cam to ...)", ln)
        over = int(d.get("over", "1")) if to else 0
        ease = EASES[d.get("ease", "INOUT" if to else "LINEAR").upper()]
        if p.kind == "overworld":
            at = d.get("at") or (bare[1] if to and len(bare) > 1 else None)
            if not at:
                raise HostError("SHW-ARG", "world cam needs at=x,y (cells)", ln)
            x, y, _ = pos(at)
            key = (f + over if to else f, ease, 0, x, y, 0, 0, 0)
        elif room:
            yaw = yaw_of(d["yaw"], ln) if "yaw" in d else None
            last = p.cam[-1] if p.cam else (0, 1, 0, 0, 0, 256, 0, 0)
            yaw = last[3] if yaw is None else yaw
            if p.cam:                         # the short way round
                while yaw - last[3] > 2048:
                    yaw -= 4096
                while last[3] - yaw > 2048:
                    yaw += 4096
            zoom = int(d.get("zoom", str(last[4])))
            punch = int(d.get("punch", str(last[5] or 256)))
            pan = [int(v) for v in nums(d["pan"], 2, ln)] if "pan" in d else [last[6], last[7]]
            key = (f + over if to else f, ease, 0, yaw, zoom, punch, pan[0], pan[1])
        else:
            at = d.get("at") or (bare[1] if to and len(bare) > 1 else None)
            if not at:
                raise HostError("SHW-ARG", "world cam needs at=x,y[,z] (cells; z = eye height, default %d)" % EYE, ln)
            v = nums(at, (2, 3), ln)
            eye = v[2] if len(v) > 2 else float(d.get("eye", EYE))
            yaw = yaw_of(d.get("yaw", "N"), ln)
            pitch = int(d.get("pitch", "0"))
            p.eye_keys = True
            key = (f + over if to else f, ease, 2, _cell(v[0]), _cell(v[1]), int(round(eye * FIX)), yaw, pitch)
        if to and p.cam:
            # the move starts from where the camera is at +f
            prev = p.cam[-1]
            if prev[0] < f:
                p.cam.append((f, 0, prev[2]) + prev[3:])
        p.cam.append(key)
        return
    if op == "rail":
        if not v3d:
            raise HostError("SHW-KIND", "cam3d rails are VIEW3D only", ln)
        name = d.get("rail")
        if name not in p.paths:
            raise HostError("SHW-PATH", "cam3d rail=%s: no such path in the set (%s)" % (name, ", ".join(p.paths)), ln)
        f1 = parse_time(d["to"], ln) if "to" in d else shot_len
        if f1 <= f:
            raise HostError("SHW-TIME", "cam3d rail=%s to=+%d is not after +%d" % (name, f1, f), ln)
        p.eye_keys = True
        for k in rail_keys(p.paths[name], f, f1, d.get("ease", "LINEAR").upper(), int(d.get("step", "2")),
                           d.get("yaw"), int(d["pitch"]) if "pitch" in d else None):
            fk, x, y, eye, yw, pitch = k
            p.cam.append((fk, 1, 2, _cell(x), _cell(y), int(round(eye * FIX)), yw, pitch))
        return
    if op == "actor":
        if not bare:
            raise HostError("SHW-ARG", "world actor needs an id", ln)
        kind = d.get("kind", "chibi").lower()
        if kind not in KINDS:
            raise HostError("SHW-ARG", "kind=%s: chibi, figure, sprite or monster" % kind, ln)
        if kind in ("sprite", "monster"):
            ref = sym(d.get("spr", "SPR_PUFF_DEMON"), "SPR_", ln)
        else:
            ref = _resolve_who(d.get("who", ""), cast, ln)
        x, y, z = pos(d.get("at", "0,0"), z=True)
        flags = 0
        for b in bare[1:]:
            flags |= {"kid": C("SAF_KID"), "shadow": C("SAF_SHADOW"), "lit": C("SAF_FULLBRIGHT"),
                      "ghost": C("SAF_TRANSLUCENT"), "hidden": C("SAF_HIDDEN"), "flip": C("SAF_FLIPX")}.get(b, 0)
        if d.get("age") == "child":
            flags |= C("SAF_KID")
        if kind in ("chibi", "figure") and "noshadow" not in bare:
            flags |= C("SAF_SHADOW")
        face = DIRS[d.get("face", "AUTO").upper()]
        a = dict(name=bare[0], id=len(p.actors), kind=KINDS[kind], ref=ref, flags=flags,
                 expr=sym(d.get("expr", "EXPR_NEUTRAL"), "EXPR_", ln), scale=int(float(d.get("scale", "0")) * 256)
                 if "." in d.get("scale", "") else int(d.get("scale", "0")),
                 hp=int(d.get("hp", "0")), keys=[(0, 1, 0, x, y, (z or 0) // FIX if not room else z, face, 0)],
                 where=ln)
        if len(p.actors) >= 48:
            raise HostError("PLAN-ACTORS", "more than 48 world actors in one shot", ln)
        p.actors.append(a)
        return
    if op in ("walk", "run", "hop", "move"):
        if t is None:
            raise HostError("SHW-TIME", "world %s needs a time" % op, ln)
        a = _where_actor(p, bare[0] if bare else "", ln)
        x, y, z = pos(d.get("to", "0,0"), z=True)
        over = int(d.get("over", "30"))
        mode = {"walk": 1, "run": 2, "hop": 3, "move": 0}[op]
        face = DIRS[d.get("face", "AUTO").upper()]
        last = a["keys"][-1]
        if last[0] < f:
            a["keys"].append((f, 0, 0, last[3], last[4], last[5], face, mode))
        else:
            a["keys"][-1] = last[:6] + (face, mode)
        zz = last[5] if z is None else (z // FIX if not room else z)
        # it arrives standing, still facing the way it walked (auto) or face=
        a["keys"].append((f + over, EASES[d.get("ease", "LINEAR").upper()], 0, x, y, zz, face, 0))
        return
    if op == "face":
        a = _where_actor(p, bare[0] if bare else "", ln)
        last = a["keys"][-1]
        dr = DIRS[(bare[1] if len(bare) > 1 else "D").upper()]
        if last[0] < f:
            a["keys"].append((f, 0, 0, last[3], last[4], last[5], dr, last[7]))
        else:
            a["keys"][-1] = last[:6] + (dr, last[7])
        return
    target = 0xFF
    if op in ("show", "hide", "expr", "pain", "attack", "kill", "fall"):
        target = _where_actor(p, bare[0] if bare else "", ln)["id"]
        if t is None and op not in ("hide", "show"):
            raise HostError("SHW-TIME", "world %s needs a time" % op, ln)
    ev = lambda o, a=0, b=0, c=0: p.events.append((f, C(o), target, a, b, c))  # noqa: E731
    if op == "show":
        ev("SEV_SHOW")
    elif op == "hide":
        ev("SEV_HIDE")
    elif op == "expr":
        ev("SEV_EXPR", sym(bare[1] if len(bare) > 1 else "EXPR_NEUTRAL", "EXPR_", ln))
    elif op == "pain":
        ev("SEV_PAIN", int(d.get("frames", "6")))
    elif op == "attack":
        ev("SEV_ATTACK", int(d.get("frames", "10")))
    elif op == "kill":
        ev("SEV_KILL", 1 if "overkill" in bare[1:] else 0, 1 if "lens" in bare[1:] else 0)
    elif op == "fall":
        ev("SEV_FALL")
    elif op == "fire":
        ev("SEV_FIRE", int(d.get("flash", "3")), int(d.get("shells", "1")))
    elif op == "hitstop":
        ev("SEV_HITSTOP", int(bare[0]) if bare else int(d.get("frames", "2")))
    elif op == "hurt":
        ev("SEV_HURT")
    elif op == "shake":
        ev("SEV_SHAKE", sym(bare[0] if bare else "SHK_TAP", "SHK_", ln))
    elif op == "lens":
        lvl = int(bare[0]) if bare else 0
        ev("SEV_LENS", max(0, min(15, lvl)), int(d.get("over", "8")),
           sym(d["tint"], "TINT_", ln) if "tint" in d else 0)
    elif op == "weapon":
        ev("SEV_WEAPON", WEAPONS[(bare[0] if bare else "pump").lower()])
    elif op == "gore":
        g = (bare[0] if bare else "CUTE").upper()
        if g not in ("CUTE", "BRUTAL"):
            raise HostError("SHW-ARG", "gore CUTE or BRUTAL", ln)
        if t is None:
            p.gore = 1 if g == "BRUTAL" else 0
        else:
            ev("SEV_GORE", 1 if g == "BRUTAL" else 0)
    elif op == "npc":
        if not bare:
            raise HostError("SHW-ARG", "world npc needs an id", ln)
        n = int(d.get("n", "8"))
        box = d.get("at", "0,0..4,4").split("..")
        x0, y0 = nums(box[0], 2, ln)
        x1, y1 = nums(box[1], 2, ln) if len(box) > 1 else (x0 + 4, y0 + 4)
        mode = NPC_MODES.get(d.get("mode", "wander"), 0)
        fl = C("SAF_KID") if ("kid" in bare[1:] or d.get("age") == "child") else 0
        p.npcs.append(dict(name=bare[0], n=n, flags=fl, rid=int(d["rid"]) if "rid" in d else None,
                           seed=int(d.get("seed", "1"), 0), box=(x0, y0, x1, y1),
                           speed=float(d.get("speed", "0.75")), species=(sym(d["species"], "SPECIES_", ln) + 1)
                           if "species" in d else 0, mode=mode, looks=int(d.get("looks", str(min(n, 6)))), where=ln))
    elif op == "hud":
        _hud(p, f, t is not None, bare, d, ln)
    else:
        raise HostError("SHW-OP", "`world %s` is not a host statement (see show/README.md, Staging reference)" % op,
                        ln)


def _hud(p, f, timed, bare, d, ln):
    if p.kind != "view3d":
        raise HostError("SHW-KIND", "the HUD is VIEW3D only", ln)
    if p.hud is None:
        p.hud = dict(on=0, hearts=5, hearts_max=5, shells=8, shells_max=8, face=0, weapon=1, plate=0,
                     sticker=0, mode=0, horn=0, flags=0,
                     cute=["", "PARTY PUMP", "PURRING SAW", "BUBBLE WAND"],
                     brutal=["", "SLAUGHTER", "MEATCLEAVER", "BUBBLE WAND"],
                     tiny="", sticker1="HELLVILE PARK BOARD", sticker2="OF DECENCY (approved)")
    h = p.hud
    sets = []
    for b in bare:
        b = b.lower()
        if b in ("on", "off", "slam"):
            sets.append(("SHUD_ON", {"off": 0, "on": 1, "slam": 2}[b]))
        elif b == "horn":
            sets.append(("SHUD_HORN", 1))
    for k, v in d.items():
        if k in ("hearts", "shells"):
            parts = v.split("/")
            sets.append(("SHUD_HEARTS" if k == "hearts" else "SHUD_SHELLS", int(parts[0])))
            if len(parts) > 1 and not timed:
                h[k + "_max"] = int(parts[1])
        elif k == "face":
            sets.append(("SHUD_FACE", FACES[v.lower()]))
        elif k == "plate":
            sets.append(("SHUD_PLATE", 1 if v.lower() == "brutal" else 0))
        elif k == "sticker":
            sets.append(("SHUD_STICKER", {"off": 0, "on": 1, "slam": 1, "blow": 2, "blown": 2}[v.lower()]))
        elif k == "mode":
            sets.append(("SHUD_MODE", 1 if v.lower() == "on" else 0))
        elif k == "weapon":
            if timed:
                p.events.append((f, C("SEV_WEAPON"), 0xFF, WEAPONS[v.lower()], 0, 0))
            else:
                h["weapon"] = WEAPONS[v.lower()]
        elif k == "tiny":
            h["tiny"] = v
        elif k in ("cute", "brutal"):
            h[k][1] = v
        elif k in ("saw_cute", "saw_brutal"):
            h[k.split("_")[1]][2] = v
        elif k == "sticker_text":
            a, _, b = v.partition("|")
            h["sticker1"], h["sticker2"] = a, b
        else:
            raise HostError("SHW-ARG", "world hud: unknown option %s" % k, ln)
    for field, val in sets:
        if timed:
            p.events.append((f, C("SEV_HUD"), 0xFF, C(field), val, 0))
        else:
            h[{"SHUD_ON": "on", "SHUD_HEARTS": "hearts", "SHUD_SHELLS": "shells", "SHUD_FACE": "face",
               "SHUD_PLATE": "plate", "SHUD_STICKER": "sticker", "SHUD_MODE": "mode", "SHUD_HORN": "horn"}[field]] = val


# ---- packing ------------------------------------------------------------------------------------
def _sec(tag, count, payload):
    while len(payload) % 4:
        payload += b"\0"
    return struct.pack(FMT["Sec"], tag, count, len(payload)) + payload


def pack(p, npc_rid=600):
    check_layout()
    secs = []
    w = p.world
    secs.append(_sec(C("SEC_WORLD"), 1, struct.pack(
        FMT["World"], w["map"], w["daylight"], w["tint"], w["tint_level"], w["vx"], w["vy"], w["vw"], w["vh"],
        w["flags"], w["fov"], w["view_dist"], 0, w["room_w"], w["room_d"], w["wallpaper"], w["floor"],
        w["wall_paint"], w["floor_paint"], w["ceiling"], w["room_kind"], w["weather"], w.get("tmpl", 0))))
    if p.cam:
        cam = sorted(p.cam, key=lambda k: k[0])
        # one key per frame: a later statement on the same frame wins
        dedup = {}
        for k in cam:
            dedup[k[0]] = k
        cam = [dedup[f] for f in sorted(dedup)]
        secs.append(_sec(C("SEC_CAMKEY"), len(cam), b"".join(struct.pack(FMT["Key"], *k) for k in cam)))
    if p.actors:
        path = []
        acts = b""
        for a in p.actors:
            keys = sorted(a["keys"], key=lambda k: k[0])
            p0 = len(path)
            path += keys
            acts += struct.pack(FMT["Actor"], a["id"], a["kind"], a["ref"], p0, len(keys), a["flags"], a["expr"],
                                a["scale"], a["hp"])
        secs.append(_sec(C("SEC_ACTOR"), len(p.actors), acts))
        secs.append(_sec(C("SEC_PATH"), len(path), b"".join(struct.pack(FMT["Key"], *k) for k in path)))
    if p.events:
        ev = sorted(p.events, key=lambda e: e[0])          # stable: script order within a frame
        secs.append(_sec(C("SEC_EVENT"), len(ev), b"".join(struct.pack(FMT["Event"], *e) for e in ev)))
    if p.furn:
        secs.append(_sec(C("SEC_FURN"), len(p.furn), b"".join(struct.pack(FMT["Furn"], *f) for f in p.furn)))
    if p.npcs:
        out = b""
        rid = npc_rid
        for n in p.npcs:
            base = n["rid"] if n["rid"] is not None else rid
            rid = base + n["n"]
            x0, y0, x1, y1 = [int(round(v * CELL)) for v in n["box"]]
            out += struct.pack(FMT["Npc"], n["n"], n["flags"], base, n["seed"] & 0xFFFFFFFF, x0, y0, x1, y1,
                               max(1, min(255, int(round(n["speed"] * 16)))), n["species"], n["mode"],
                               max(0, min(n["n"], n["looks"])))
        secs.append(_sec(C("SEC_NPC"), len(p.npcs), out))
    if p.hud is not None:
        h = p.hud
        cute = [p.string(s) if s else 0xFFFF for s in h["cute"]]
        brut = [p.string(s) if s else 0xFFFF for s in h["brutal"]]
        tiny = p.string(h["tiny"]) if h["tiny"] else 0xFFFF
        s1 = p.string(h["sticker1"])
        s2 = p.string(h["sticker2"])
        secs.append(_sec(C("SEC_HUD"), 1, struct.pack(
            _hud_fmt(), h["on"], h["hearts"], h["hearts_max"], h["shells"], h["shells_max"], h["face"],
            h["weapon"], h["plate"], h["sticker"], h["mode"], h["horn"], h["flags"], *cute, *brut, tiny, s1, s2, 0)))
    if p.strings:
        secs.append(_sec(C("SEC_STR"), 1, bytes(p.strings)))
    body = b"".join(secs)
    total = struct.calcsize(FMT["Hdr"]) + len(body)
    flags = C("SHF_KIDS") if p.kids else 0
    hdr = struct.pack(FMT["Hdr"], C("SHOST_MAGIC"), C("SHOST_ABI"),
                      {"overworld": C("SHOST_OVERWORLD"), "room": C("SHOST_ROOM"), "view3d": C("SHOST_VIEW3D")}[p.kind],
                      len(secs), total, p.len, p.gore, flags, p.seed & 0xFFFFFFFF)
    return hdr + body


def lint(p, shot_id="shot", where=None):
    """the host's own lints (spec 13.5): STD-KIDS (a child in a BRUTAL shot is an error, the
    filter stays on anyway), STD-FPS bookkeeping, PLAN-* counts"""
    out = []
    has_kid = p.kids or any(a["flags"] & C("SAF_KID") for a in p.actors) or \
        any(n["flags"] & C("SAF_KID") for n in p.npcs)
    brutal = p.gore == 1 or any(e[1] == C("SEV_GORE") and e[3] == 1 for e in p.events)
    if has_kid and brutal:
        out.append(HostError("STD-KIDS", "a child is in this shot and it asks for gore=BRUTAL: the filter is "
                             "forced on in every frame (hard limit 3); stage the gore in a scene with no child",
                             where, shot_id))
    nb = len(p.actors) + sum(n["n"] for n in p.npcs)
    if nb > 160:
        out.append(HostError("PLAN-BB", "%d billboards: the host draws at most 160" % nb, where, shot_id))
    if sum(n["n"] for n in p.npcs) > 64:
        out.append(HostError("PLAN-NPC", "more than 64 NPC citizens in one shot", where, shot_id))
    return out


def pack_plane(set_block, shot_lines, shot_len, cast=None, seed=0, gore="CUTE", kids=False, npc_rid=600,
               shot_id="shot", allow_std=False):
    """the plug-in entry point: set block + shot statements -> record bytes. A hard-limit
    lint (STD-*) raises HostError; allow_std packs anyway (only the standards fixtures)"""
    _, p = parse_set(set_block)
    p.len = shot_len
    p.seed = seed
    p.gore = 1 if str(gore).upper() == "BRUTAL" else 0
    p.kids = bool(kids)
    for ln in shot_lines:
        if is_host_statement(ln.text):
            apply_statement(p, ln, cast or {}, shot_len, shot_id)
    errs = [e for e in lint(p, shot_id, shot_lines[0] if shot_lines else None) if e.rule.startswith("STD-")]
    if errs and not allow_std:
        raise errs[0]
    return pack(p, npc_rid)


# ---- a stand-alone host script (the tests, and staging before the compiler) -------------------------
def read_file(path):
    """`set` blocks and `shot <id> len=N set=<name> [gore=] [seed=] [kids]` blocks -> shots"""
    lines = [Line(path, i + 1, t) for i, t in enumerate(open(path, encoding="utf-8").read().splitlines())]
    sets, shots, i = {}, [], 0
    while i < len(lines):
        ws = split_words(lines[i].text)
        if not ws:
            i += 1
            continue
        if ws[0] in ("set", "shot"):
            j = i + 1
            while j < len(lines) and split_words(lines[j].text)[:1] != ["end"]:
                j += 1
            block = lines[i:j + 1]
            if ws[0] == "set":
                d, _ = kv(ws[2:])
                sets[ws[1]] = block
            else:
                shots.append(block)
            i = j + 1
            continue
        i += 1
    out = []
    for block in shots:
        ws = split_words(block[0].text)
        d, bare = kv(ws[2:])
        sname = d.get("set", "")
        if sname not in sets:
            raise HostError("SHW-SET", "shot %s: set=%s is not defined in this file" % (ws[1], sname), block[0])
        out.append(dict(id=ws[1], len=int(d.get("len", "90")), set=sname, set_block=sets[sname],
                        lines=block[1:-1], gore=d.get("gore", "CUTE"), seed=int(d.get("seed", "0"), 0),
                        kids="kids" in bare, fixture="fixture" in bare, cast={}, where=block[0]))
    return out


def compile_file(path, npc_rid=600):
    recs = []
    for s in read_file(path):
        rec = pack_plane(s["set_block"], s["lines"], s["len"], s["cast"], s["seed"], s["gore"], s["kids"],
                         npc_rid, s["id"], allow_std=s["fixture"])
        recs.append((s, rec))
    return recs


def cgen(paths, out):
    rows, body = [], []
    for path in paths:
        for s, rec in compile_file(path):
            name = re.sub(r"\W", "_", s["id"])
            words = struct.unpack("<%dI" % (len(rec) // 4), rec)
            body.append("static const uint32_t rec_%s[%d] = {\n%s\n};" % (
                name, len(words), ",\n".join("    " + ", ".join("0x%08xu" % w for w in words[i:i + 6])
                                             for i in range(0, len(words), 6))))
            rows.append('    { "%s", %d, rec_%s, %d },' % (s["id"], s["len"], name, len(rec)))
    src = ["/* generated by show/tools/showtk/hosts.py cgen: the hosts' test shots. Do not edit. */",
           "#include <stdint.h>",
           "typedef struct HostScene { const char *name; int len; const uint32_t *rec; uint32_t bytes; } HostScene;",
           ""] + body + ["", "const HostScene host_scenes[] = {"] + rows + ["    { 0, 0, 0, 0 }", "};",
                                                                             "const int host_scene_count = %d;" % len(rows), ""]
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    open(out, "w").write("\n".join(src))
    return len(rows)


def dump(rec):
    """a record as text (tests, debugging)"""
    out = []
    h = struct.unpack_from(FMT["Hdr"], rec, 0)
    out.append("hdr magic=%08x abi=%d kind=%d nsec=%d bytes=%d len=%d gore=%d flags=%d seed=%d" % h)
    off = SIZES["Hdr"]
    names = {v: k for k, v in symbols().items() if k.startswith("SEC_")}
    for _ in range(h[3]):
        tag, count, n = struct.unpack_from(FMT["Sec"], rec, off)
        out.append("  %s x%d (%d B)" % (names.get(tag, tag), count, n))
        off += SIZES["Sec"] + n
    return "\n".join(out)


def main(argv=None):
    ap = argparse.ArgumentParser(prog="showtk.hosts", description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("cgen")
    c.add_argument("-o", "--out", required=True)
    c.add_argument("files", nargs="+")
    d = sub.add_parser("dump")
    d.add_argument("files", nargs="+")
    k = sub.add_parser("check")
    k.add_argument("files", nargs="+")
    a = ap.parse_args(argv)
    try:
        if a.cmd == "cgen":
            n = cgen(a.files, a.out)
            print("hosts: %d shot(s) -> %s" % (n, a.out))
        elif a.cmd == "dump":
            for f in a.files:
                for s, rec in compile_file(f):
                    print("%s (%s, %d f): %d B" % (s["id"], s["set"], s["len"], len(rec)))
                    print(dump(rec))
        else:
            bad = 0
            for f in a.files:
                for s in read_file(f):
                    _, p = parse_set(s["set_block"])
                    p.len = s["len"]
                    p.gore = 1 if s["gore"].upper() == "BRUTAL" else 0
                    p.kids = s["kids"]
                    for ln in s["lines"]:
                        apply_statement(p, ln, {}, s["len"], s["id"])
                    for e in lint(p, s["id"], s["where"]):
                        print(e.format())
                        bad += e.rule.startswith("STD-")
            return 1 if bad else 0
    except HostError as e:
        print(e.format(), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
