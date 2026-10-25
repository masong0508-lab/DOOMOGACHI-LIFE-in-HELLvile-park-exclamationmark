"""show/art/gen - the show's plate, prop and card generators (owner: ART).

The generators ARE the source: no hand-painted binary goes into git. A set
file names a generator call, `gen:<name>(k=v,...)`, and the compiler's bake
phase (showc phase 5) calls bake() and caches the indexed result.

    from <this package> import bake, GENERATORS
    cv = bake("gen:bandstand_tiles(side=north)")     # -> Canvas (8bpp indices)
    pts = bake("gen:scribble_bursts(cx=194,cy=96,n=5,r=28..52,seed=12)")  # path

Contract (also in show/series/sets/SETS.md):
  * image generators return a Canvas: `.a` is a uint8 (h, w) array of master
    palette indices, 0 = transparent; `.meta` may carry 'tiles' (TILEMAP),
    'text' boxes, 'anchor', 'frames' (flipbooks) and 'notes';
  * path generators return a list of (x, y) points in show pixels;
  * lut generators return a uint8[256] index remap;
  * every generator is pure: same arguments, same pixels (seeded Rng only).
"""
import importlib
import re

GENERATORS = {}
_MODULES = ["crayon", "kit", "props", "cards", "sets_park", "sets_fair", "sets_town",
            "sets_misc", "amb", "motion", "props2"]
_loaded = False


def generator(name=None, kind="image"):
    def deco(fn):
        GENERATORS[name or fn.__name__] = (fn, kind)
        return fn
    return deco


def _load():
    global _loaded
    if _loaded:
        return
    _loaded = True
    for m in _MODULES:
        try:
            importlib.import_module("." + m, __name__)
        except ModuleNotFoundError as e:
            if e.name and e.name.endswith("." + m):
                continue
            raise


def _value(v):
    v = v.strip()
    if re.fullmatch(r"-?\d+", v):
        return int(v)
    if re.fullmatch(r"-?\d*\.\d+", v):
        return float(v)
    m = re.fullmatch(r"(-?\d+)\.\.(-?\d+)", v)
    if m:
        return (int(m.group(1)), int(m.group(2)))
    if v.startswith('"') and v.endswith('"'):
        return v[1:-1]
    return v


def parse_src(src):
    """'gen:name(a=1,b=x)' -> ('name', {'a': 1, 'b': 'x'})."""
    s = src.strip()
    if s.startswith("gen:"):
        s = s[4:]
    m = re.fullmatch(r"([A-Za-z_][A-Za-z0-9_]*)(?:\((.*)\))?", s)
    if not m:
        raise ValueError("bad generator call '%s'" % src)
    name, args = m.group(1), m.group(2) or ""
    kw = {}
    depth, cur = 0, ""
    parts = []
    for ch in args:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur)
    for p in parts:
        k, _, v = p.partition("=")
        kw[k.strip()] = _value(v)
    return name, kw


_CACHE = {}


def bake(src):
    """Run (or fetch from the in-process cache) a generator call."""
    _load()
    key = src.strip()
    if key in _CACHE:
        return _CACHE[key]
    name, kw = parse_src(key)
    if name not in GENERATORS:
        raise KeyError("unknown generator '%s' (in %s)" % (name, src))
    fn, kind = GENERATORS[name]
    out = fn(**kw)
    _CACHE[key] = out
    return out


def kind_of(src):
    _load()
    name, _ = parse_src(src)
    return GENERATORS[name][1]


def names():
    _load()
    return sorted(GENERATORS)
