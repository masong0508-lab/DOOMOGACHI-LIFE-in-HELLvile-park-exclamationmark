"""compose.py - draw a 2D set at a camera position for one variant (previews only).

The PS1 player draws the masters; this is the ART package's preview so Claude
and the creator can judge sets before show.exe can draw them. It follows the
set grammar in SETS.md: planes in order (back, front, top), each plane's props
right after it, parallax `screen = at - cam * par`, px=2/3 planes drawn as
nearest-neighbour blocks, the variant's palette LUT on every plane that is not
`light=fixed`.
"""
import re

import numpy as np

from . import gen
from .setfile import xy, wh, split_list

FRAME_W, FRAME_H, MATTE = 320, 240, 30
ORDER = {"back": 0, "mid": 0, "front": 1, "top": 2}


def _g():
    return gen()


def bake(src):
    return _g().bake(src)


def band_rows(lib, spec, height=240):
    """bands=<id> (library) or bands=sky:<tod> (light.SKY_BANDS)."""
    light = _g().light
    if spec.startswith("sky:"):
        return light.sky_rows(spec[4:], height)
    st = lib.bands.get(spec)
    if st is None:
        raise KeyError("unknown bands '%s'" % spec)
    rows = []
    for tok in st.get("rows", "").split():
        idx, _, n = tok.rpartition(":")
        rows.append((light.parse_index(idx), int(n)))
    out = []
    for c, n in rows:
        out += [c] * n
    stripe = st.get("seam", "stripe") == "stripe"
    if stripe:
        y = 0
        for k in range(1, len(rows)):
            y += rows[k - 1][1]
            a, b = rows[k - 1][0], rows[k][0]
            for j, c in ((y - 3, b), (y - 1, b), (y + 1, a), (y + 3, a)):
                if 0 <= j < len(out):
                    out[j] = c
    return (out + [rows[-1][0]] * height)[:height]


class Variant:
    def __init__(self, st=None):
        a = st.args if st is not None else {}
        self.id = st.id if st is not None else "base"
        self.light = a.get("light", "day")
        self.hide = set(split_list(a.get("hide")))
        self.show = set(split_list(a.get("show")))
        self.states = dict(t.split(":", 1) for t in split_list(a.get("state")))
        self.over = {k: v for k, v in a.items() if "." in k}   # e.g. bands.sky=sky:night
        self.desc = a.get("desc", "")


def variants_of(sd):
    if not sd.variants:
        return [Variant()]
    return [Variant(v) for v in sd.variants]


def plane_hidden(st, var):
    if st.id in var.show:
        return False
    if st.id in var.hide:
        return True
    return st.get("hidden", "no") == "yes"


def _lut_for(st, var):
    light = _g().light
    name = var.over.get("light." + st.id) or (None if st.get("light") == "fixed" else var.light)
    if st.get("light") == "fixed" and ("light." + st.id) not in var.over:
        return None
    return light.lut(name) if name and name != "day" else None


def _put(dst, img, x, y, px, lut):
    a = img.a if hasattr(img, "a") else img
    if lut is not None:
        a = lut[a]
    if px > 1:
        a = np.repeat(np.repeat(a, px, 0), px, 1)
    h, w = a.shape
    x, y = int(round(x)), int(round(y))
    sx0, sy0 = max(0, -x), max(0, -y)
    dx0, dy0 = max(0, x), max(0, y)
    dx1, dy1 = min(dst.shape[1], x + w), min(dst.shape[0], y + h)
    if dx1 <= dx0 or dy1 <= dy0:
        return
    sub = a[sy0:sy0 + dy1 - dy0, sx0:sx0 + dx1 - dx0]
    m = sub != 0
    dst[dy0:dy1, dx0:dx1][m] = sub[m]


def plane_origin(st, img_w, img_h, px):
    """Top-left of a plane in screen pixels (0..319, 0..239) at camera 0."""
    if st.get("at"):
        x, y = xy(st.get("at"))
        return x, y + MATTE
    return (FRAME_W - img_w * px) / 2.0, 0.0


def prop_xy(pr, img, px):
    x, y = xy(pr.get("at"))
    y += MATTE
    w, h = img.w * px, img.h * px
    anchor = pr.get("anchor", "bottom")
    if anchor == "center":
        return x - w / 2.0, y - h / 2.0
    if anchor == "topleft":
        return x, y
    if anchor == "top":
        return x - w / 2.0, y
    return x - w / 2.0, y - h            # bottom (feet)


def par_of(st):
    p = st.get("par", "0")
    a, _, b = p.partition(",")
    return float(a), float(b) if b else float(a)


def compose(lib, sd, var, cam=(0.0, 0.0), strokes=True, props=True, base=1):
    """-> uint8 (240, 320) index frame (open matte). base=0 leaves holes visible."""
    out = np.full((FRAME_H, FRAME_W), base, np.uint8)
    px = sd.px
    planes = sorted(enumerate(sd.planes), key=lambda t: (ORDER.get(t[1].get("order", "back"), 0), t[0]))
    for _, st in planes:
        if plane_hidden(st, var):
            continue
        parx, pary = par_of(st)
        lut = _lut_for(st, var)
        ppx = int(st.get("px", px))
        if st.kw == "strokes":
            if strokes:
                draw_strokes(lib, sd, st, out, cam, parx, pary, lut)
            continue
        if "bands" in st.args:
            spec = var.over.get("bands." + st.id, st.get("bands"))
            rows = band_rows(lib, spec)
            for y in range(FRAME_H):
                yy = int(y + cam[1] * pary)
                out[y, :] = rows[max(0, min(len(rows) - 1, yy))]
            if lut is not None:
                out[:] = lut[out]
            continue
        src = st.get("image") or st.get("tilemap") or st.get("tile")
        if src is None:
            continue
        src = var.over.get("image." + st.id, src)
        img = bake(src)
        if "tile" in st.args:
            tw, th = img.w * ppx, img.h * ppx
            ox = -((cam[0] * parx) % tw)
            oy = -((cam[1] * pary) % th)
            yy = oy
            while yy < FRAME_H:
                xx = ox
                while xx < FRAME_W:
                    _put(out, img, xx, yy, ppx, lut)
                    xx += tw
                yy += th
        else:
            x0, y0 = plane_origin(st, img.w, img.h, ppx)
            _put(out, img, x0 - cam[0] * parx, y0 - cam[1] * pary, ppx, lut)
        if props:
            for pr in sd.props:
                if pr.get("plane") != st.id or plane_hidden(pr, var):
                    continue
                art = pr.get("art")
                state = var.states.get(pr.id)
                if state:
                    art = pr.get("art." + state, art)
                pimg = bake(art)
                plut = None if pr.get("light") == "fixed" else lut
                x, y = prop_xy(pr, pimg, ppx)
                _put(out, pimg, x - cam[0] * parx, y - cam[1] * pary, ppx, plut)
    return out


# ---- strokes (crayon register) ----------------------------------------------
def nib_colours(lib, nib):
    light = _g().light
    name = nib.split(":", 1)[1] if ":" in nib else nib
    st = lib.nibs.get(name)
    if st is None:
        raise KeyError("unknown nib '%s'" % nib)
    ink = light.parse_index(st.get("ink"))
    edge = light.parse_index(st.get("edge", st.get("ink")))
    gap = [int(v) for v in st.get("gap", "0,0").split(",")]
    return ink, edge, gap


def stroke_points(src):
    g = _g()
    m = re.match(r"^(\w+)\((.*)\)$", src)
    if src.startswith("gen:") or (m and m.group(1) in g.GENERATORS):
        return g.bake(src if src.startswith("gen:") else "gen:" + src)
    raise KeyError("unknown stroke path '%s'" % src)


def draw_strokes(lib, sd, st, out, cam, parx, pary, lut, reveal=1.0, frame=0):
    """Polylines with the nib's ink, a 1-px darker edge and its gap pattern."""
    from math import hypot
    for s in st.strokes:
        ink, edge, gap = nib_colours(lib, s.get("nib"))
        w = int(s.get("w", "2"))
        pts = stroke_points(s.get("path"))
        if pts and isinstance(pts[0][0], (list, tuple)):
            polys = pts
        else:
            polys = [pts]
        for poly in polys:
            run = 0.0
            for (xa, ya), (xb, yb) in zip(poly, poly[1:]):
                n = max(1, int(hypot(xb - xa, yb - ya)))
                for k in range(n):
                    t = k / n
                    x = xa + (xb - xa) * t - cam[0] * parx
                    y = ya + (yb - ya) * t - cam[1] * pary + MATTE
                    run += 1
                    if gap[0] and (int(run) % (gap[0] + gap[1])) >= gap[0]:
                        continue
                    xi, yi = int(round(x)), int(round(y))
                    for dy in range(w):
                        for dx in range(w):
                            if 0 <= xi + dx < FRAME_W and 0 <= yi + dy < FRAME_H:
                                c = edge if (dy == w - 1 and w > 1) else ink
                                out[yi + dy, xi + dx] = lut[c] if lut is not None else c
