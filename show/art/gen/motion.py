"""motion.py - the cast's motion flipbooks, built from the game engine's own parts.

Every drawing starts as a render of the character by the game's portrait
engine (core/src/portrait*.c through showtk.art.cast.host: figure 48x80,
bust 48x56, chibi 16x24, fighter 32x48). This module only cuts those renders
into parts (head/hat band, torso, paws, feet) and moves the parts: the
classic cut-out animation of a puppet, never a repaint. The show player
draws a flipbook as an actor POSE (show_actor.h FLIP, SHOP_POSE): REPLACE
mode for body motion, OVER mode for overlays (Kevin's third eye, the ribs).

    gen:cycle(char=mayor,kind=walk)        a loop; kinds below
    gen:pose(char=mayor,name=pat)          a named one-shot (the shot lists' TODOs)

Cycle kinds (holds at 30 fps; the anchor is the feet's bottom-centre):
    idle      engine breath + blink (anim 0/32, a blink), 4 drawings, 72 f
    walk      8 drawings x 3 f = a 24 f two-step waddle: contact squash, passing
              bob, feet lift, paws swing, a 1 px lean, the hat/ears lag 2 f
    run       6 x 2 f: 2 px bob, 3 px lift, 2 px lean, stretched passing pose
    tiptoe    8 x 5 f: walk at half stride, stretched 4 % tall (Pip)
    back_step 8 x 3 f: the walk played backwards with no lean
Pop and take variants (one-shots; play with `pose A <name>` then back to idle):
    pop       anticipation 0.88x1.10 -> stretch 1.18 tall -> overshoot 0.94 -> 1.03 -> settle
    pop_small 0.94 -> 1.06 -> 0.98 -> settle
    take_big  anticipation squash 3 f, SHOCK stretch 1.25 tall, hat pops 6 px
              and lands 2 f late, overshoot, settle (13 drawings)
    react_small  EXPR_SHOCK, 0.95 squash, ears/hat flop 2 px, settle
    laugh_full   EXPR_HAPPY, 3 squash-bounce hops with a body shake, hat lag
    hit       squash 0.82, hat jumps 4 px and lands 3 f late, ears flop, settle
    smear_l / smear_r  whip smears: 2 drawings stretched 1.6x along the move,
              the trailing edge broken into speed streaks (the cam whip's cut)
    exit_run  anticipation lean back, then the run's first stride smeared
"""
import math
import os
import sys

import numpy as np

from . import generator, pal
from .canvas import Canvas
from .amb import flipbook

R, P = pal.ramp, pal.paint

_TOOLS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
_CACHE = {}
_BIBLE = []


def _bible():
    if not _BIBLE:
        if _TOOLS not in sys.path:
            sys.path.insert(0, _TOOLS)
        from showtk.art.cast.lookdata import Bible
        _BIBLE.append(Bible())
    return _BIBLE[0]


def render(char, kind="figure", expr=0, anim=0, a=0, b=0, status=1, mouth=-1):
    """the engine's own drawing of a cast member -> uint8 (h, w) indices"""
    key = (char, kind, expr, anim, a, b, status, mouth)
    if key in _CACHE:
        return _CACHE[key]
    from showtk.art.cast import host
    lb = _bible().char_look_bytes(char)
    if lb is None:
        raise KeyError("char %s has no look (sprite cast?)" % char)
    _pal, out = host.render([host.job(lb, kind, status=status, expr=expr, mouth=mouth, anim=anim, a=a, b=b)])
    w, h, px = out[0]
    arr = np.frombuffer(px, np.uint8).reshape(h, w).copy()
    _CACHE[key] = arr
    return arr


def expr_id(name):
    names = ["NEUTRAL", "HAPPY", "SAD", "ANGRY", "LOVE", "SHOCK", "SLEEPY", "SICK", "POSSESSED", "DEAD", "TALK"]
    n = str(name).upper().replace("EXPR_", "")
    return names.index(n) if n in names else int(name)


# ---------------------------------------------------------------------------
# parts of a figure
# ---------------------------------------------------------------------------
class Parts:
    """Masks of a figure render: top (hat/ears/hair band), feet, left/right paws."""

    def __init__(self, a):
        self.a = a
        h, w = a.shape
        ys, xs = np.nonzero(a)
        self.x0, self.x1, self.y0, self.y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
        bh = self.y1 - self.y0
        self.cx = (self.x0 + self.x1) / 2.0
        yy, xx = np.mgrid[0:h, 0:w]
        op = a != 0
        self.top = op & (yy < self.y0 + int(bh * 0.26))
        self.feet = op & (yy >= self.y1 - max(5, int(bh * 0.13)))
        self.lfoot = self.feet & (xx < self.cx)
        self.rfoot = self.feet & (xx >= self.cx)
        band = op & (yy >= self.y0 + int(bh * 0.58)) & (yy < self.y1 - max(5, int(bh * 0.13)))
        bw = self.x1 - self.x0
        self.lpaw = band & (xx < self.x0 + int(bw * 0.24))
        self.rpaw = band & (xx >= self.x1 - int(bw * 0.24))


def _move(dst, src, mask, dx, dy):
    """paste src[mask] into dst shifted by (dx, dy)"""
    ys, xs = np.nonzero(mask)
    h, w = dst.shape
    ny, nx = ys + dy, xs + dx
    ok = (ny >= 0) & (ny < h) & (nx >= 0) & (nx < w)
    dst[ny[ok], nx[ok]] = src[ys[ok], xs[ok]]


def _scale_about(a, sx, sy, ax, ay, out_shape, ox, oy):
    """nearest-neighbour scale of a about (ax, ay) (bottom-centre), pasted so the
    anchor lands at (ox, oy) of a canvas of out_shape"""
    H, W = out_shape
    out = np.zeros(out_shape, np.uint8)
    yy, xx = np.mgrid[0:H, 0:W]
    sxs = np.floor((xx - ox) / sx + ax + 0.5).astype(int)
    sys_ = np.floor((yy - oy) / sy + ay + 0.5).astype(int)
    ok = (sxs >= 0) & (sxs < a.shape[1]) & (sys_ >= 0) & (sys_ < a.shape[0])
    out[ok] = a[sys_[ok], sxs[ok]]
    return out


def _shear(a, k, y_pivot):
    """lean in two stepped bands (pixel art leans by blocks, never a staircase):
    the upper half of the body shifts k px, the middle third k//2 (rounded away
    from 0), the feet stay planted"""
    out = np.zeros_like(a)
    ys = np.nonzero(a.any(axis=1))[0]
    top = ys.min() if len(ys) else 0
    span = max(1, y_pivot - top)
    half = k // 2 if k >= 0 else -((-k) // 2)
    if half == 0:
        half = 1 if k > 0 else (-1 if k < 0 else 0)
    for y in range(a.shape[0]):
        f = (y - top) / float(span)
        d = k if f < 0.5 else (half if f < 0.8 else 0)
        if d > 0:
            out[y, d:] = a[y, :a.shape[1] - d]
        elif d < 0:
            out[y, :d] = a[y, -d:]
        else:
            out[y] = a[y]
    return out


PAD = 16          # every cycle drawing is the figure on a canvas PAD px wider each side


def compose(a, parts=None, bob=0, sx=1.0, sy=1.0, lift_l=0, lift_r=0, paw_l=0, paw_r=0, lean=0,
            top_dy=0, top_dx=0, pawx_l=0, pawx_r=0, pad=PAD):
    """one drawing: the figure with its parts moved, then squashed about the feet"""
    p = parts or Parts(a)
    base = a.copy()
    # cut the moving parts out, paste them back moved
    moving = p.top | p.lfoot | p.rfoot | p.lpaw | p.rpaw
    body = np.where(moving, 0, base).astype(np.uint8)
    # keep the torso under moved paws/feet filled: paws/feet move by <= 3 px, the hole
    # is covered by the part itself or is background, which reads as the gap of a step
    _move(body, base, p.top, top_dx, top_dy)
    _move(body, base, p.lpaw, pawx_l, paw_l)
    _move(body, base, p.rpaw, pawx_r, paw_r)
    _move(body, base, p.lfoot, 0, -lift_l)
    _move(body, base, p.rfoot, 0, -lift_r)
    if lean:
        body = _shear(body, lean, p.y1 - 4)
    H, W = a.shape[0] + pad * 2 + 16, a.shape[1] + pad * 2      # 16 px headroom: stretches, hat pops
    ax, ay = p.cx, p.y1 - 1
    return _scale_about(body, sx, sy, ax, ay, (H, W), W / 2.0, H - pad - 1 + bob)


def canv(arr):
    c = Canvas(arr.shape[1], arr.shape[0])
    c.a = arr.astype(np.uint8)
    return c


def _fb(arrs, holds, mode="replace", loop=1):
    fb = flipbook([canv(x) for x in arrs], holds)
    # every drawing's feet sit `pad` px above its bottom edge (flipbook() bottom-aligns)
    fb.meta.update(mode=mode, loop=int(loop), anchor="bottom", pad=PAD)
    return fb


# ---------------------------------------------------------------------------
# cycles
# ---------------------------------------------------------------------------
def _walk(a, stride=2, bob=1, lean=1, swing=1, frames=8, squash=0.96, stretch=1.0):
    p = Parts(a)
    out = []
    lift = [0, 1, stride, 1, 0, 0, 0, 0]
    for k in range(frames):
        kk = k * 8 // frames
        ll = lift[kk]
        rl = lift[(kk + 4) % 8]
        contact = kk in (0, 4)
        passing = kk in (2, 6)
        dy = -bob if passing else 0
        sy = squash if contact else stretch
        sx = 1.0 + (1.0 - squash) * 0.6 if contact else 1.0
        sw = swing if kk < 4 else -swing
        lag_dy = [0, 1, 0, -1, 0, 1, 0, -1][(kk - 2) % 8] if bob else 0
        out.append(compose(a, p, bob=dy, sx=sx, sy=sy, lift_l=ll, lift_r=rl, paw_l=-sw, paw_r=sw,
                           lean=lean, top_dy=lag_dy))
    return out


@generator(kind="flipbook")
def cycle(char="mayor", kind="idle", expr="NEUTRAL", status=1):
    e = expr_id(expr)
    st = int(status)
    if kind == "idle":
        fr = [render(char, "figure", e, an, status=st) for an in (0, 32, 0, 95)]
        p = Parts(fr[0])
        arrs = [compose(x, p) for x in fr]
        return _fb(arrs, [18, 18, 33, 3])
    a = render(char, "figure", e, 0, status=st)
    if kind == "walk":
        return _fb(_walk(a), 3)
    if kind == "run":
        arrs = _walk(a, stride=3, bob=2, lean=2, swing=2, frames=6, squash=0.92, stretch=1.04)
        return _fb(arrs, 2)
    if kind == "tiptoe":
        return _fb(_walk(a, stride=1, bob=1, lean=0, swing=1, squash=1.0, stretch=1.04), 5)
    if kind == "back_step":
        return _fb(_walk(a, lean=-1)[::-1], 3)
    p = Parts(a)
    if kind == "pop":
        keys = [(1.10, 0.88, 0), (0.86, 1.18, -3), (1.04, 0.94, 0), (0.98, 1.03, 0), (1.0, 1.0, 0)]
        return _fb([compose(a, p, sx=sx, sy=sy, bob=b, top_dy=(-2 if sy > 1.1 else 0)) for sx, sy, b in keys],
                   [3, 3, 3, 3, 6], loop=0)
    if kind == "pop_small":
        keys = [(1.04, 0.94), (0.97, 1.06), (1.01, 0.98), (1.0, 1.0)]
        return _fb([compose(a, p, sx=sx, sy=sy) for sx, sy in keys], [2, 3, 3, 4], loop=0)
    if kind in ("take_big", "react_small", "hit", "laugh_full"):
        return _fb(*_oneshot(char, kind, st), loop=0)
    if kind in ("smear_l", "smear_r"):
        d = -1 if kind == "smear_l" else 1
        return _fb([smear(compose(a, p, lean=2 * d), d, 1.6), smear(compose(a, p, lean=d), d, 1.25)], [1, 1], loop=0)
    if kind == "exit_run":
        run = _walk(a, stride=3, bob=2, lean=2, swing=2, frames=6, squash=0.92, stretch=1.04)
        arrs = [compose(a, p, lean=-2, sx=1.06, sy=0.92), compose(a, p, lean=-2, sx=1.08, sy=0.9),
                smear(run[1], 1, 1.5), run[2], run[3]]
        return _fb(arrs, [3, 4, 1, 2, 2], loop=0)
    raise KeyError("cycle kind %s" % kind)


def _oneshot(char, kind, st):
    if kind == "take_big":
        a0 = render(char, "figure", 0, 0, status=st)
        a1 = render(char, "figure", expr_id("SHOCK"), 0, status=st)
        p0, p1 = Parts(a0), Parts(a1)
        seq = [(a0, p0, dict(sx=1.06, sy=0.92)), (a0, p0, dict(sx=1.1, sy=0.86)),
               (a1, p1, dict(sx=0.84, sy=1.25, bob=-4, top_dy=-6)), (a1, p1, dict(sx=0.88, sy=1.18, bob=-3, top_dy=-6)),
               (a1, p1, dict(sx=0.96, sy=1.06, bob=-1, top_dy=-4)), (a1, p1, dict(sx=1.06, sy=0.93, top_dy=-1)),
               (a1, p1, dict(sx=1.08, sy=0.92, top_dy=1)), (a1, p1, dict(sx=0.98, sy=1.03)),
               (a1, p1, dict(sx=1.01, sy=0.99)), (a1, p1, {})]
        return [compose(a, p, pad=PAD, **kw) for a, p, kw in seq], [3, 3, 2, 2, 2, 2, 2, 3, 3, 8]
    if kind == "react_small":
        a1 = render(char, "figure", expr_id("SHOCK"), 0, status=st)
        p = Parts(a1)
        seq = [dict(sx=1.03, sy=0.95, top_dy=1), dict(sx=1.0, sy=1.0, top_dy=2), dict(sy=1.02, top_dy=-1), {}]
        return [compose(a1, p, **kw) for kw in seq], [2, 3, 3, 6]
    if kind == "hit":
        a1 = render(char, "figure", expr_id("SHOCK"), 0, status=st)
        p = Parts(a1)
        seq = [dict(sx=1.16, sy=0.82, top_dy=-4), dict(sx=1.1, sy=0.88, top_dy=-4), dict(sx=0.96, sy=1.05, top_dy=-2),
               dict(sx=1.03, sy=0.97, top_dy=1), dict(top_dy=2), dict(top_dy=0)]
        return [compose(a1, p, **kw) for kw in seq], [2, 2, 2, 2, 3, 6]
    if kind == "laugh_full":
        a1 = render(char, "figure", expr_id("HAPPY"), 0, status=st)
        a2 = render(char, "figure", expr_id("HAPPY"), 32, status=st)
        p = Parts(a1)
        arrs, holds = [], []
        for hop in range(3):
            src = a1 if hop % 2 == 0 else a2
            arrs += [compose(src, p, sx=1.08, sy=0.9, top_dx=1), compose(src, p, sx=0.95, sy=1.07, bob=-3, top_dy=-1),
                     compose(src, p, sx=1.0, sy=1.0, bob=-2, top_dx=-1, top_dy=1)]
            holds += [2, 3, 3]
        arrs.append(compose(a1, p))
        holds.append(6)
        return arrs, holds


def smear(a, d, k):
    """stretch along x by k away from the move, with broken streaks on the trailing side"""
    h, w = a.shape
    ys, xs = np.nonzero(a)
    cx = (xs.min() + xs.max()) / 2.0
    out = np.zeros_like(a)
    for y in range(h):
        row = a[y]
        nz = np.nonzero(row)[0]
        if len(nz) == 0:
            continue
        for x in range(w):
            # trailing side stretched: source x maps back toward the centre
            if (x - cx) * d < 0:
                sxp = cx + (x - cx) / k
            else:
                sxp = x
            si = int(round(sxp))
            if 0 <= si < w and row[si]:
                out[y, x] = row[si]
        if y % 3 == 1:          # streak gaps on the trailing edge
            tail = nz.min() if d > 0 else nz.max()
            for t in range(3, 9):
                xx = int(tail - d * t)
                if 0 <= xx < w and t % 2:
                    out[y, xx] = 0
    return out


# ---------------------------------------------------------------------------
# named poses (the ART items of the shot lists' TODOs)
# ---------------------------------------------------------------------------
def _figure(char, expr="NEUTRAL", anim=0, status=1):
    a = render(char, "figure", expr_id(expr), anim, status=status)
    return a, Parts(a)


def _raise_paw(a, p, side, dy, dx=0):
    """paw lifted dy px: the paw part moves, a short sleeve (the paw's top
    row colour) fills the gap so the arm reads as raised"""
    out = a.copy()
    m = p.rpaw if side == "r" else p.lpaw
    out[m] = 0
    ys, xs = np.nonzero(m)
    if len(ys) == 0:
        return out
    sleeve = a[ys.min(), xs[np.argmin(ys)]]
    for k in range(0, abs(dy) + 1):
        for x in np.unique(xs):
            yy = ys.min() - k if dy > 0 else ys.max() + k
            if 0 <= yy < a.shape[0] and 0 <= x + dx * k // max(1, abs(dy)) < a.shape[1]:
                if out[yy, x + dx * k // max(1, abs(dy))] == 0:
                    out[yy, x + dx * k // max(1, abs(dy))] = sleeve
    _move(out, a, m, dx, -dy)
    return out


def _pad(a):
    return compose(a, Parts(a))


@generator(kind="flipbook")
def pose(char="mayor", name="pat", expr="HAPPY"):
    n = name
    if n == "pat":                                  # co15: pats the banner/ladder, pleased
        a, p = _figure(char, expr)
        fr = [_raise_paw(a, p, "r", 10, -2), _raise_paw(a, p, "r", 12, -2), _raise_paw(a, p, "r", 10, -2),
              _raise_paw(a, p, "r", 12, -2), _raise_paw(a, p, "r", 4)]
        return _fb([_pad(x) for x in fr], [5, 4, 4, 4, 6], loop=0)
    if n == "tie_balloon":                          # co15: both paws up, tying a knot
        a, p = _figure(char, "NEUTRAL")
        fr = []
        for k in range(6):
            x = _raise_paw(a, p, "l", 14 + (k & 1) * 2, 2)
            x = _raise_paw(x, Parts(x), "r", 14 + ((k + 1) & 1) * 2, -2)
            fr.append(compose(x, Parts(x), sy=1.03))
        return _fb(fr, 4)
    if n == "roll":                                 # co15: the roll off the ladder (a plush tumble)
        a, p = _figure(char, "SHOCK")
        pd = _pad(a)
        fr = [np.rot90(pd, k) for k in (0, 1, 2, 3)]
        H = max(f.shape[0] for f in fr)
        W = max(f.shape[1] for f in fr)
        out = []
        for f in fr:
            c = np.zeros((H, W), np.uint8)
            c[H - f.shape[0]:, (W - f.shape[1]) // 2:(W - f.shape[1]) // 2 + f.shape[1]] = f
            out.append(c)
        out.append(compose(a, p, sx=1.12, sy=0.86, pad=(H - a.shape[0]) // 2))
        return _fb(out, [3, 3, 3, 3, 6], loop=0)
    if n == "reach_turn":                           # co18: reaches for the balloon, then turns to it
        a, p = _figure(char, "NEUTRAL")
        r1 = _raise_paw(a, p, "r", 16, -1)
        q = Parts(r1)
        fr = [compose(r1, q, sy=1.04), compose(r1, q, sy=1.08, bob=-1), compose(r1, q, sx=0.7, sy=1.06),
              compose(r1[:, ::-1].copy(), Parts(r1[:, ::-1].copy()), sx=0.7, sy=1.04),
              compose(r1[:, ::-1].copy(), Parts(r1[:, ::-1].copy()))]
        return _fb(fr, [4, 6, 2, 2, 8], loop=0)
    if n == "climb_down":                           # co18: down the ladder, paw over paw
        a, p = _figure(char, "NEUTRAL")
        fr = []
        for k in range(4):
            x = _raise_paw(a, p, "l" if k % 2 == 0 else "r", 12)
            fr.append(compose(x, Parts(x), lift_l=3 if k % 2 else 0, lift_r=0 if k % 2 else 3, bob=k % 2))
        return _fb(fr, 5)
    if n == "paw_tuck":                             # co20 (nana): the paw tucks the towel round the egg
        a, p = _figure(char, "LOVE")
        fr = [_raise_paw(a, p, "r", 4, -3), _raise_paw(a, p, "r", 6, -6), _raise_paw(a, p, "r", 5, -8),
              _raise_paw(a, p, "r", 6, -8), _raise_paw(a, p, "r", 3, -4)]
        return _fb([_pad(x) for x in fr], [6, 6, 8, 6, 8], loop=0)
    if n == "wave_quarter":                         # Mascotty: a wave at quarter speed (12 f per drawing)
        a, p = _figure(char, "NEUTRAL")
        fr = [_raise_paw(a, p, "r", 16, 2), _raise_paw(a, p, "r", 18, 4), _raise_paw(a, p, "r", 16, 2),
              _raise_paw(a, p, "r", 18, 0)]
        return _fb([_pad(x) for x in fr], 12)
    if n == "hug":                                  # Fuzz: arms in, the squeeze (the one hugged squashes)
        a, p = _figure(char, "LOVE")
        fr = [compose(a, p, pawx_l=3, pawx_r=-3, paw_l=-3, paw_r=-3), compose(a, p, pawx_l=5, pawx_r=-5, paw_l=-4,
              paw_r=-4, sx=1.04, sy=0.97), compose(a, p, pawx_l=5, pawx_r=-5, paw_l=-4, paw_r=-4, sx=1.02, sy=0.99)]
        return _fb(fr, [4, 10, 8])
    if n == "mallet_play":                          # Bonejangles plays his ribs (a mallet in the right paw)
        a, p = _figure(char, "HAPPY", status=1)
        fr = []
        for k, (dy, dx) in enumerate([(8, -6), (4, -8), (8, -10), (4, -8)]):
            x = _raise_paw(a, p, "r", dy, dx)
            c = canv(_pad(x))
            mx, my = c.w // 2 + dx - 2, c.h - PAD - 34 - dy
            c.line(mx + 10, my - 6, mx, my, R("WOOD", 3))
            c.ellipse(mx, my, 2, 2, P("CHERRY", 1))
            fr.append(c.a)
        return _fb(fr, [3, 3, 3, 3])
    if n in ("tiptoe", "steps"):                    # Pip: tiptoe in / three steps back
        return cycle(char=char, kind="tiptoe" if n == "tiptoe" else "back_step")
    if n == "third_blink":                          # Kevin: only the third eye blinks (OVER the bust)
        return third_blink(char)
    if n == "ribs_purple":                          # Bonejangles possessed: the ribs glow purple (OVER)
        return ribs_purple(char)
    if n == "from_behind":                          # the young cherub from behind (co04): the engine's back view
        fr = [render(char, "fighter", 0, 0, a=k, b=0xFF) for k in (0, 1, 2, 3)]
        return _fb([_pad(x) for x in fr], [6, 6, 6, 6])
    if n == "bob21":                                # Fluffy: a 21 f bob on her string
        a, p = _figure(char, "LOVE")
        fr = [compose(a, p, bob=-int(round(2 * math.sin(2 * math.pi * k / 7))), sy=1.0 + 0.03 * math.cos(2 * math.pi * k / 7))
              for k in range(7)]
        return _fb(fr, 3)
    if n == "tub":                                  # Steamy in the duck tub
        return steamy_tub(char)
    if n == "sheet_up":                             # Fuzz raises the sign-up sheet
        a, p = _figure(char, "HAPPY")
        fr = [_raise_paw(_raise_paw(a, p, "l", k, 2), p, "r", k, -2) for k in (4, 8, 12)]
        return _fb([_pad(x) for x in fr], [3, 3, 6], loop=0)
    raise KeyError("pose %s" % n)


def third_blink(char="kevin"):
    """Kevin's third eye blinks alone: the bust's forehead strip from the
    engine's blink frame (anim 95) over the open-eyed bust, OVER mode."""
    a0 = render(char, "bust", 0, 0)
    a1 = render(char, "bust", 0, 95)
    diff = (a0 != a1)
    ys, xs = np.nonzero(diff)
    over0 = np.zeros_like(a0)
    over1 = np.zeros_like(a0)
    if len(ys):
        ytop = ys.min()
        # the third eye is the topmost changed blob: keep changed pixels within 6 rows of the top
        m = diff & (np.arange(a0.shape[0])[:, None] <= ytop + 6)
        over1[m] = a1[m]
        over0[m] = a0[m]
    fb = _fb([over0, over1, over1, over0], [60, 2, 2, 30])
    fb.meta["mode"] = "over"
    fb.meta["kind"] = "bust"
    return fb


def ribs_purple(char="bonejangles"):
    """The possessed ribs glow purple from inside: the torso's light bone pixels
    pulse VAPE_PURPLE (OVER the figure; 4 drawings, 36 f)."""
    a = render(char, "figure", 0, 0, status=1)
    p = Parts(a)
    h = a.shape[0]
    yy = np.arange(h)[:, None]
    torso = (a != 0) & (yy > p.y0 + (p.y1 - p.y0) * 0.45) & (yy < p.y1 - 10)
    rgb = pal.RGB[a]
    light = rgb.sum(axis=2) > 520
    m = torso & light
    out = []
    for lvl in (P("VAPE_PURPLE", 1), P("VAPE_PURPLE", 2), P("LAVENDER", 2), P("VAPE_PURPLE", 2)):
        o = np.zeros_like(a)
        o[m] = lvl
        out.append(compose(o, p))
    fb = _fb(out, [9, 9, 9, 9])
    fb.meta["mode"] = "over"
    return fb


def steamy_tub(char="steamy"):
    a, p = _figure(char, "HAPPY")
    fr = []
    for k in range(4):
        c = canv(compose(a, p, bob=(k % 2)))
        tub_y = c.h - PAD - 22
        c.ellipse(c.w / 2, tub_y + 10, 28, 12, P("SKY", 2))
        c.rect(c.w / 2 - 28, tub_y + 10, 57, PAD + 12, P("BUBBLEGUM", 1))
        c.hline(int(c.w / 2 - 28), int(c.w / 2 + 28), tub_y + 10, P("BUBBLEGUM", 2))
        for j in range(5):
            bx = c.w / 2 - 20 + j * 10 + (k + j) % 3
            c.ellipse(bx, tub_y + 8 - ((k + j) % 2), 3, 2, pal.WHITE)
        fr.append(c.a)
    return _fb(fr, 6)
