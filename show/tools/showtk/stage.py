"""stage.py - sets, planes, props, strokes, paths and marks (spec 5.1, 6.1, 7.1 phase 4).

series/sets/*.shw (ART track):

    set <id> kind=2d|view3d|overworld|room px=1|2|3 [matte=open|black] [look=pixel|crayon|...]
      plane <id> image=<src> | tile=<src> | bands=<src> [size=WxH] [par=0.05] [order=back|front|top] [boil=N] [at=x,y]
      strokes <id> [par=] [boil=]
        stroke <id> nib=crayon:<NAME>|<palette index> w=2 path=<path> [outside=on]
      end
      prop <id> art=<src> plane=<plane> at=x,y [anchor=center] [face=front|L|R|wall]
      mark <id> <x>                 a named x (the Wall line)
      path <id> y=<y> from=<x> to=<x>   a lane for crowds and walks
    end

Paths a stroke may follow: ellipse(cx,cy,rx,ry[,turns=,wobble=,seed=]),
line(x0,y0,x1,y1), poly(x,y,x,y,...); anything else (gen:...) is asked of a
registered generator (register_path), else the stroke gets no points and a
warning (the bake lane fills it in later).

Plates (`image=gen:...`, `tile=gen:...`) are baked by a registered baker
(register_baker): fn(src, w, h, px, seed) -> {'w','h','data' (8bpp bytes),
'clut' [BGR555...]} or None. Until a baker exists the plane carries tex=NONE
and the player draws its slug fill.
"""
import math
import random
import re
from fractions import Fraction

from . import docunits, textpack_bridge
from .errors import Diag

SET_KINDS = {"slug": 0, "2d": 1, "view3d": 2, "overworld": 3, "room": 4}
PLANE_KINDS = {"bands": 0, "image": 1, "tilemap": 2, "strokes": 3, "grain": 4, "view3d": 5,
               "overworld": 6, "room": 7}
ORDERS = {"back": 0, "front": 1, "top": 2}
FACES = {"front": 0, "L": 1, "R": 2, "wall": 3}

_BAKERS = []
_PATHS = []


def register_baker(fn):
    """fn(src, w, h, px, seed) -> dict | None (first non-None wins)"""
    _BAKERS.append(fn)


def register_path(fn):
    """fn(src) -> [(x, y)] | None"""
    _PATHS.append(fn)


def _load_plugins():
    """the ART/lane-2 bake modules register themselves on import, if present"""
    import importlib
    for name in ("showtk.art.bake", "showtk.art.gen", "showtk.bake"):
        try:
            importlib.import_module(name)
        except ImportError:
            pass
        except Exception:       # a broken plug-in must not stop a compile
            pass


_plugins_loaded = False


def bake(src, w, h, px, seed=0):
    global _plugins_loaded
    if not _plugins_loaded:
        _plugins_loaded = True
        _load_plugins()
    for fn in _BAKERS:
        r = fn(src, w, h, px, seed)
        if r:
            return r
    return None


class SetDef:
    def __init__(self, st):
        self.id = st.words[0] if st.words else st.fail("SET", "`set <id> kind=...`")
        self.stmt = st
        self.kind = st.get("kind", "2d")
        if self.kind not in SET_KINDS:
            st.fail("SET", "set %s: kind=%s (one of %s)" % (self.id, self.kind, ", ".join(SET_KINDS)))
        self.px = int(st.get("px", "1"))
        self.matte = st.get("matte", "open")
        self.look = st.get("look", "pixel")
        self.planes, self.props, self.marks, self.paths, self.strokes = [], [], {}, {}, []
        self.variants = {}
        self.frames = {}
        for c in st.children:
            if c.kw == "plane":
                self.planes.append(c)
            elif c.kw == "strokes":
                self.planes.append(c)
                for s in c.children:
                    if s.kw == "stroke":
                        self.strokes.append((c, s))
            elif c.kw == "prop":
                self.props.append(c)
            elif c.kw == "mark":
                if len(c.pos) < 2:
                    c.fail("SET", "`mark <id> <x>[,<y>]`")
                xy = str(c.pos[1]).split(",")
                self.marks[c.pos[0]] = tuple(docunits.num(v, c.where()) for v in xy)
            elif c.kw == "frame":
                if not c.pos:
                    c.fail("SET", "`frame <id> at=x,y [scale=s]`")
                self.frames[c.pos[0]] = c
            elif c.kw == "path":
                if not c.pos:
                    c.fail("SET", "`path <id> y= from= to=`")
                self.paths[c.pos[0]] = c
            elif c.kw == "variant":
                if c.pos:
                    self.variants[c.pos[0]] = c
            # other statements belong to other tools (lights, locks, notes): ignored here


def load_sets(stmts_by_file):
    sets, diags = {}, []
    for stmts in stmts_by_file:
        for st in stmts:
            if st.kw != "set":
                continue
            try:
                sd = SetDef(st)
            except Exception as e:     # ShowError with a diag
                diags += getattr(e, "diags", [Diag("SET", str(e), **st.where())])
                continue
            if sd.id in sets:
                diags.append(st.diag("SET", "set %s is already defined at %s:%d" % (
                    sd.id, sets[sd.id].stmt.file, sets[sd.id].stmt.line)))
                continue
            sets[sd.id] = sd
    return sets, diags


def _args(inner):
    pos, kw = [], {}
    for part in [p.strip() for p in inner.split(",") if p.strip()]:
        if "=" in part:
            k, v = part.split("=", 1)
            kw[k.strip()] = v.strip()
        else:
            pos.append(part)
    return pos, kw


def path_points(src, where):
    """a stroke path -> ([(x, y)], diag or None)"""
    m = re.fullmatch(r"(\w+)\((.*)\)", src.strip())
    if m and m.group(1) in ("ellipse", "line", "poly"):
        pos, kw = _args(m.group(2))
        nums = [float(docunits.num(p, where)) for p in pos]
        if m.group(1) == "ellipse":
            if len(nums) < 4:
                return [], Diag("STROKE", "ellipse(cx,cy,rx,ry[,turns=,wobble=,seed=])", **where)
            cx, cy, rx, ry = nums[:4]
            turns = float(kw.get("turns", 1))
            wob = float(kw.get("wobble", 0))
            rng = random.Random(int(kw.get("seed", 0)))
            n = max(8, int(math.ceil(2 * math.pi * max(rx, ry) * turns / 3)))   # a point every ~3 px
            pts = []
            for i in range(n + 1):
                a = 2 * math.pi * turns * i / n - math.pi / 2
                dx = rng.uniform(-wob, wob) if wob else 0
                dy = rng.uniform(-wob, wob) if wob else 0
                pts.append((int(round(cx + rx * math.cos(a) + dx)), int(round(cy + ry * math.sin(a) + dy))))
            return pts, None
        if len(nums) < 4 or len(nums) % 2:
            return [], Diag("STROKE", "%s needs x,y pairs" % m.group(1), **where)
        return [(int(round(nums[i])), int(round(nums[i + 1]))) for i in range(0, len(nums), 2)], None
    for fn in _PATHS:
        r = fn(src)
        if r:
            return [(int(x), int(y)) for x, y in r], None
    return [], Diag("STROKE", "path %s: no generator for it yet (the stroke has no points until the "
                              "art lane registers one)" % src, severity="warning", **where)


def nib_index(nib, where):
    """nib=crayon:CRAYON_RED | PAINT_* | <0..255> -> (palette index, diag or None)"""
    v = nib.split(":", 1)[1] if ":" in nib else nib
    if re.fullmatch(r"\d+", v):
        return int(v), None
    S = textpack_bridge.symbols()
    if v in S:
        return S[v] & 0xFF, None
    return 0, Diag("STROKE", "nib %s: no palette entry called %s yet (index 0 until the art lane's palette "
                             "names it)" % (nib, v), severity="warning", **where)


def size_of(st, setdef):
    s = st.get("size")
    if s:
        m = re.fullmatch(r"(\d+)x(\d+)", s)
        if not m:
            st.fail("SET", "size=%s: WxH" % s)
        return int(m.group(1)), int(m.group(2))
    return docunits.CANVAS[0] // setdef.px, 240 // setdef.px


def par_q16(v, where):
    """par=0.05 or par=<x>,<y> (the x factor drives the plane; y is in the timeline)"""
    v = str(v).split(",")[0]
    return int(math.floor(Fraction(docunits.num(v, where)) * 65536 + Fraction(1, 2)))
