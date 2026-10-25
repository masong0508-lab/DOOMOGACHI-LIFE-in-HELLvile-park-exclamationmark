"""bake.py - showc phase 5 for the picture (spec 7.1 step 5, 9.4): run a
generator call once, keep its indexed result.

    image("gen:storybook_park(river=1,seed=22)") -> (uint8 array (h, w), meta)
    path("gen:ellipse(cx=160,cy=134,rx=48,ry=13,seed=11)") -> [[(x, y), ...], ...]

`gen:` calls go to ART's generator registry (show/art/gen, `bake()`); the
results are cached in memory and on disk as indexed PNGs under
show/build/cache/bake/<sha256 of the call and the generator sources>.png, so a
warm bake is a file read. `shape:` calls are the few path shapes the engine
itself provides (tests, quick staging):

    shape:line(x0,y0,x1,y1)  shape:poly(x;y;x;y...)  shape:circle(cx,cy,r[,turns])
    shape:ellipse(cx,cy,rx,ry[,turns])                (no wobble: ART's ellipse has it)
"""
import hashlib
import importlib
import math
import os
import re
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SHOW = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
CACHE = os.path.join(SHOW, "build", "cache", "bake")
_MEM = {}
_PAL = None


class BakeError(ValueError):
    pass


def art_gen():
    """ART's generator package (show/art/gen), imported as `art.gen`."""
    if SHOW not in sys.path:
        sys.path.insert(0, SHOW)
    try:
        return importlib.import_module("art.gen")
    except ModuleNotFoundError as e:
        raise BakeError("ART's generators (show/art/gen) are not on this branch yet: %s" % e)


def art_module(name):
    art_gen()
    return importlib.import_module("art.gen." + name)


def _sources_hash():
    d = os.path.join(SHOW, "art", "gen")
    h = hashlib.sha256()
    if os.path.isdir(d):
        for f in sorted(os.listdir(d)):
            if f.endswith(".py"):
                with open(os.path.join(d, f), "rb") as fh:
                    h.update(f.encode() + fh.read())
    return h.hexdigest()[:16]


def _key(src):
    return hashlib.sha256((src.strip() + "|" + _sources_hash()).encode()).hexdigest()[:24]


def _palette_rgb():
    """The master palette as 256 RGB triples (for the PNG's PLTE only)."""
    from . import colors
    global _PAL
    if _PAL is not None:
        return _PAL
    text = open(os.path.join(colors.REPO, "core", "src", "palette.c")).read()
    body = text[text.index("doomo_palette[256] ="):]
    body = re.sub(r"/\*.*?\*/", "", body[body.index("{") + 1:body.index("};")], flags=re.S)
    rgb = []
    for m in re.finditer(r"\bC(3?)\s*\(([^)]*)\)", body):         # C(r,g,b) / C3(9 values)
        v = [int(x, 0) for x in m.group(2).split(",")]
        for i in range(0, len(v), 3):
            for c in v[i:i + 3]:
                c5 = (c >> 3) & 31
                rgb.append((c5 << 3) | (c5 >> 2))
    if len(rgb) != 768:
        raise BakeError("parsed %d palette entries from core/src/palette.c, not 256" % (len(rgb) // 3))
    _PAL = rgb
    return rgb


def save_indexed_png(path, a):
    from PIL import Image
    im = Image.fromarray(np.asarray(a, np.uint8), "P")
    im.putpalette(_palette_rgb())
    os.makedirs(os.path.dirname(path), exist_ok=True)
    im.save(path, optimize=False)


def load_indexed_png(path):
    from PIL import Image
    im = Image.open(path)
    if im.mode != "P":
        raise BakeError("%s is not an indexed PNG" % path)
    return np.array(im, np.uint8)


def image(src):
    """A generator call -> (indexed array, meta). meta survives only in memory."""
    src = src.strip()
    if src in _MEM:
        return _MEM[src]
    key = _key(src)
    disk = os.path.join(CACHE, key + ".png")
    gen = art_gen()
    if os.path.exists(disk) and gen.kind_of(src) == "image":
        a = load_indexed_png(disk)
        meta = {}
        try:                                   # tiles/text boxes need the real run
            cv = gen.bake(src)
            meta = getattr(cv, "meta", {})
        except Exception:
            pass
        _MEM[src] = (a, meta)
        return _MEM[src]
    cv = gen.bake(src)
    a = np.asarray(cv.a, np.uint8)
    save_indexed_png(disk, a)
    _MEM[src] = (a, getattr(cv, "meta", {}))
    return _MEM[src]


# ---- paths ------------------------------------------------------------------------
def _args(s):
    return [float(v) for v in re.split(r"[;,]", s) if v.strip()]


def _shape(call):
    m = re.fullmatch(r"(\w+)\((.*)\)", call.strip())
    if not m:
        raise BakeError("bad shape call '%s'" % call)
    name, a = m.group(1), _args(m.group(2))
    if name == "line" and len(a) == 4:
        return [[(a[0], a[1]), (a[2], a[3])]]
    if name == "poly" and len(a) >= 4 and len(a) % 2 == 0:
        return [[(a[i], a[i + 1]) for i in range(0, len(a), 2)]]
    if name in ("circle", "ellipse"):
        if name == "circle":
            cx, cy, rx = a[0], a[1], a[2]
            ry, turns = rx, (a[3] if len(a) > 3 else 1.0)
        else:
            cx, cy, rx, ry = a[:4]
            turns = a[4] if len(a) > 4 else 1.0
        n = max(16, int((rx + ry) * 1.6))
        tot = int(n * turns)
        return [[(cx + math.cos(-2.3 + k / n * 6.283) * rx, cy + math.sin(-2.3 + k / n * 6.283) * ry)
                 for k in range(tot + 1)]]
    raise BakeError("unknown shape '%s' (line, poly, circle, ellipse)" % name)


def path(src):
    """A path call -> a list of polylines of (x, y) show px."""
    src = src.strip()
    if src.startswith("shape:"):
        return _shape(src[6:])
    gen = art_gen()
    if gen.kind_of(src) != "path":
        raise BakeError("%s is not a path generator" % src)
    out = gen.bake(src)
    if out and isinstance(out[0][0], (int, float)):
        return [list(out)]                      # one polyline
    return [list(p) for p in out]               # several
