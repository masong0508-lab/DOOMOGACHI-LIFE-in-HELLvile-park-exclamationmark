"""amb.py - art for the ambient animation layers of the sets (owner: ART).

The quality bar: every set lives. The `amb` statements of series/sets/*.shw
(grammar in SETS.md sec.6) animate the plates with cheap PS1 means: a drifting
plane, a row-sheared plane in the wind, a palette cycle, flickering lamps,
swaying props, flipbooks, motes and background walkers. This module draws the
art some of them need; every image is palette-exact and seamless where it
wraps.

  amb_clouds(w,h,n,seed,tone)        a horizontally tileable cloud strip (drift)
  amb_fog(w,h,seed,c,density)        a tileable fog band (drift, order=front)
  amb_haze(w,h,seed)                 a far dust/heat haze strip for the Hellvile side
  amb_water(w,h,seed,kind)           river/pond/fountain-basin water in the CYCLE ramp
  amb_jet(frames=6,h)                fountain jet flipbook (drops arc and fall)
  amb_flag(color,w,h)                a pennant on a pole tip, 4 wave frames
  amb_wheel(r,spokes,seed)           a ferris wheel, 8 rotation frames
  amb_carousel(w)                    carousel canopy + horses bobbing, 6 frames
  amb_steam(w,h)                     a steam/smoke curl, 8 frames
  amb_flame(w,h,color)               a torch/candle flame, 4 frames
  amb_sparkle()                      a 7x7 twinkle, 4 frames (signs, the banner)

A flipbook generator returns its first frame as the Canvas and the whole
cycle in meta['frames'] (Canvases of one size) and meta['holds'] (frames each
drawing is held at 30 fps).
"""
import math

import numpy as np

from . import generator, pal, kit
from .canvas import Canvas, Rng, bayer

R, P = pal.ramp, pal.paint
D = kit.D

# The water CYCLE ramp: four indices that only the water overlays use, so a
# per-plane palette rotation (amb kind=pcycle) shimmers the water and nothing
# else. SKY.3..SKY.6 (blue-greens stay put in every LUT family).
CYCLE_WATER = [R("SKY", 3), R("SKY", 4), R("SKY", 5), R("SKY", 6)]


def flipbook(frames, holds):
    if isinstance(holds, int):
        holds = [holds] * len(frames)
    w = max(f.w for f in frames)
    h = max(f.h for f in frames)
    out = []
    for f in frames:
        if (f.w, f.h) != (w, h):
            c = Canvas(w, h)
            c.blit(f, (w - f.w) // 2, h - f.h)
            f = c
        out.append(f)
    first = out[0].copy()
    first.meta["frames"] = out
    first.meta["holds"] = [int(x) for x in holds]
    return first


# ---------------------------------------------------------------------------
@generator()
def amb_clouds(w=512, h=64, n=5, seed=1, tone="day"):
    """Puffy clouds in a strip that wraps at w (drawn twice across the seam)."""
    w, h = int(w), int(h)
    cv = Canvas(w, h)
    rng = Rng(seed)
    fill, shade = {"day": (pal.WHITE, R("SKY", 7)), "warm": (R("CANDY", 7), R("PASTEL_PINK", 6)),
                   "grey": (R("STONE", 7), R("STONE", 6)),
                   "night": (R("STONE", 4), R("STONE", 3)), "dawn": (R("PASTEL_PINK", 7), R("PEACH", 6)),
                   "crayon": (R("SKY", 6), R("SKY", 5))}.get(tone, (pal.WHITE, R("SKY", 7)))
    step = w / max(1, int(n))
    xs, ys = cv.grid()
    for k in range(int(n)):
        cw = rng.randint(44, 84)
        x = k * step + rng.uniform(0, max(1, step - cw))
        base = rng.randint(h // 2 + 4, h - 3)
        m = np.zeros((h, w), bool)
        npuff = 3 + cw // 22
        for p in range(npuff):
            t = (p + 0.5) / npuff
            r = cw * (0.16 + 0.16 * math.sin(math.pi * t)) + rng.uniform(-2, 2)
            cx = x + t * cw
            cy = base - r * 0.55 - rng.uniform(0, 3)
            for ox in (0, -w, w):
                m |= ((xs - cx - ox) ** 2 / (r * r) + (ys - cy) ** 2 / (r * r * 0.7)) <= 1.0
        m &= ys <= base
        cv.a[m] = fill
        # shaded underside: the lowest third of each column
        sh = m & ~np.roll(m, -3, axis=0)
        sh |= m & (ys > base - 3)
        cv.a[sh] = shade
    cv.a[cv.outline_mask(cv.a != 0) & (cv.a == 0)] = 0
    return cv


@generator()
def amb_fog(w=512, h=40, seed=1, c="STONE.6", density=9):
    """A soft fog band: a dithered mask with a lumpy top, tileable at w."""
    from .light import parse_index
    w, h = int(w), int(h)
    col = parse_index(c)
    cv = Canvas(w, h)
    rng = Rng(seed)
    ph = [rng.uniform(0, 6.28) for _ in range(3)]
    xs = np.arange(w)
    top = h * 0.45 + sum(h * 0.18 / (k + 1) * np.sin(xs * 2 * math.pi * (k + 1) / w + ph[k])
                         for k in range(3))
    ys = np.arange(h)[:, None]
    depth = (ys - top[None, :]) / np.maximum(1, h - top[None, :])
    lvl = np.clip(depth * 16 + int(density) - 8, 0, 15)
    b = bayer(w, h)
    cv.a[(ys >= top[None, :]) & (b < lvl)] = col
    return cv


@generator()
def amb_haze(w=512, h=48, seed=1):
    """Hellvile's far smoke over the Wall: dark wisps, tileable."""
    w, h = int(w), int(h)
    cv = Canvas(w, h)
    rng = Rng(seed)
    b = bayer(w, h)
    xs = np.arange(w)
    for k in range(4):
        ph = rng.uniform(0, 6.28)
        y = h * (0.3 + 0.15 * k) + 5 * np.sin(xs * 2 * math.pi * (k + 2) / w + ph)
        for x in range(w):
            y0 = int(y[x])
            for yy in range(max(0, y0 - 2), min(h, y0 + 3)):
                if b[yy, x] < 7 - abs(yy - y0) * 2:
                    cv.a[yy, x] = R("STONE", 1 + (k & 1))
    return cv


@generator()
def amb_water(w=352, h=24, seed=1, kind="river"):
    """Ripple lines in the CYCLE_WATER ramp over transparency (sits on the
    painted water). kind=river: long streaks; pond/basin: short arcs."""
    w, h = int(w), int(h)
    cv = Canvas(w, h)
    rng = Rng(seed)
    n = (w * h) // (40 if kind == "river" else 26)
    for i in range(n):
        x, y = rng.randint(0, w - 1), rng.randint(0, h - 1)
        ln = rng.randint(4, 12) if kind == "river" else rng.randint(2, 5)
        c = CYCLE_WATER[(x // 3 + i) % 4]
        for k in range(ln):
            cv.px((x + k) % w, y, c)
        if kind != "river":
            cv.px(x - 1, y + 1, c)
            cv.px((x + ln) % w, y + 1, c)
    return cv


@generator(kind="flipbook")
def amb_jet(frames=6, h=26, w=21, seed=1):
    """A fountain jet: a column and two arcs of drops that move down their arcs."""
    frames, h, w = int(frames), int(h), int(w)
    out = []
    cx = w // 2
    for f in range(frames):
        cv = Canvas(w, h)
        for y in range(4, h - 2):
            cv.px(cx, y, R("SKY", 6) if (y + f) % 3 else pal.WHITE)
        for side in (-1, 1):
            for k in range(7):
                t = ((k + f / frames) / 7.0)
                x = cx + side * t * (w // 2 - 1)
                y = 4 + (h - 6) * (t * t) - 3 * t
                cv.px(round(x), round(y), pal.WHITE if k % 2 else R("SKY", 7))
        cv.px(cx, 3 - (f % 2), pal.WHITE)
        out.append(cv)
    return flipbook(out, 3)


@generator(kind="flipbook")
def amb_flag(color="BUBBLEGUM", w=12, h=8):
    """A pennant on a pole tip; the wave travels down its length (4 frames)."""
    c = P(color.upper(), 1)
    w, h = int(w), int(h)
    out = []
    for f in range(4):
        cv = Canvas(w + 2, h + 4)
        cv.vline(0, 0, h + 3, R("WOOD", 2))
        for x in range(w):
            k = x / w
            off = round(1.5 * math.sin(2 * math.pi * (k - f / 4.0)) * k)
            half = (h / 2) * (1 - k * 0.85)
            y0, y1 = h / 2 - half + off + 1, h / 2 + half + off + 1
            for y in range(int(round(y0)), int(round(y1)) + 1):
                cv.px(1 + x, y, c if y > (y0 + y1) / 2 - 0.5 else P(color.upper(), 2))
        out.append(cv)
    return flipbook(out, 4)


@generator(kind="flipbook")
def amb_wheel(r=34, spokes=8, seed=1, frames=8):
    """A pastel ferris wheel: the rim turns, the gondolas hang plumb."""
    r, spokes, frames = int(r), int(spokes), int(frames)
    size = 2 * r + 12
    cols = [P("BUBBLEGUM", 1), P("LEMON", 1), P("MINT", 1), P("LAVENDER", 1)]
    out = []
    for f in range(frames):
        cv = Canvas(size, size + 18)
        cx = cy = size // 2
        # legs
        cv.line(cx, cy, cx - r * 0.7, size + 16, R("STEEL", 3))
        cv.line(cx, cy, cx + r * 0.7, size + 16, R("STEEL", 3))
        a0 = 2 * math.pi * f / (frames * spokes)
        for k in range(spokes):
            a = a0 + 2 * math.pi * k / spokes
            cv.line(cx, cy, cx + r * math.cos(a), cy + r * math.sin(a), R("STEEL", 5))
        for k in range(120):
            a = 2 * math.pi * k / 120
            cv.px(round(cx + r * math.cos(a)), round(cy + r * math.sin(a)), R("STEEL", 6))
        for k in range(spokes):
            a = a0 + 2 * math.pi * k / spokes
            gx, gy = round(cx + r * math.cos(a)), round(cy + r * math.sin(a))
            cv.rect(gx - 3, gy + 1, 7, 5, cols[k % 4])
            cv.hline(gx - 3, gx + 3, gy + 1, D(cols[k % 4], 1))
            cv.px(gx, gy + 3, R("CANDY", 7))
        cv.rect(cx - 1, cy - 1, 3, 3, pal.GOLD)
        out.append(cv)
    return flipbook(out, 4)


@generator(kind="flipbook")
def amb_carousel(w=64, frames=6):
    """A carousel: a striped canopy, poles, horses bobbing on alternate phases."""
    w, frames = int(w), int(frames)
    h = int(w * 0.8)
    cols = [P("BUBBLEGUM", 2), P("LEMON", 2), P("MINT", 2), P("LAVENDER", 2)]
    out = []
    for f in range(frames):
        cv = Canvas(w, h)
        for x in range(w):
            top = int(abs(x - w / 2) * 0.35)
            stripe = cols[((x + f * 2) // 6) % 4]
            cv.vline(x, top, 14, stripe)
        cv.hline(0, w - 1, 15, R("CANDY", 7))
        npole = 5
        for k in range(npole):
            x = int((k + 0.5) * w / npole)
            cv.vline(x, 15, h - 6, R("STONE", 6))
            bob = round(2 * math.sin(2 * math.pi * (f / frames + k / 2.0)))
            hy = h // 2 + bob
            cv.rect(x - 4, hy, 9, 5, pal.WHITE)
            cv.rect(x + 3, hy - 3, 3, 4, pal.WHITE)
            cv.px(x - 4, hy + 5, R("STONE", 5))
            cv.px(x + 3, hy + 5, R("STONE", 5))
            cv.px(x + 4, hy - 2, cols[k % 4])
        cv.rect(0, h - 6, w, 6, R("WOOD", 3))
        cv.hline(0, w - 1, h - 6, R("WOOD", 5))
        out.append(cv)
    return flipbook(out, 4)


@generator(kind="flipbook")
def amb_steam(w=12, h=22, frames=8, c="WHITE"):
    """A rising curl of steam (pies, the turkey, chimneys): dithered puffs."""
    from .light import parse_index
    col = parse_index(c) if c != "WHITE" else pal.WHITE
    w, h, frames = int(w), int(h), int(frames)
    out = []
    b = bayer(w, h)
    for f in range(frames):
        cv = Canvas(w, h)
        for k in range(4):
            t = ((k + f / frames) / 4.0)
            y = h - 3 - t * (h - 5)
            x = w / 2 + 2.5 * math.sin(2 * math.pi * (t * 1.3 + 0.2))
            r = 1.2 + t * 2.3
            m = cv.ellipse_mask(x, y, r, r * 0.8)
            lvl = int(13 - t * 11)
            cv.a[m & (b < lvl)] = col
        out.append(cv)
    return flipbook(out, 3)


@generator(kind="flipbook")
def amb_flame(w=5, h=8, color="gold"):
    w, h = int(w), int(h)
    core = {"gold": pal.WHITE, "blue": P("SKY", 2), "purple": P("VAPE_PURPLE", 2)}.get(color, pal.WHITE)
    edge = {"gold": pal.GOLD, "blue": P("COBALT", 1), "purple": P("VAPE_PURPLE", 1)}.get(color, pal.GOLD)
    out = []
    for f, (lean, tall) in enumerate([(0, 0), (1, -1), (0, 1), (-1, 0)]):
        cv = Canvas(w + 2, h + 2)
        cx = (w + 2) / 2 - 0.5
        pts = [(cx - w / 2, h + 1), (cx + w / 2, h + 1), (cx + lean, 1 - tall)]
        cv.poly(pts, edge)
        cv.ellipse(cx, h - 1, w / 2 - 0.5, 2, edge)
        cv.ellipse(cx + lean * 0.5, h - 1, 1, 1.6, core)
        out.append(cv)
    return flipbook(out, [3, 2, 4, 3])


@generator(kind="flipbook")
def amb_sparkle(c="WHITE"):
    from .light import parse_index
    col = pal.WHITE if c == "WHITE" else parse_index(c)
    out = []
    for r in (0, 1, 3, 1):
        cv = Canvas(7, 7)
        if r:
            for k in range(-r, r + 1):
                cv.px(3 + k, 3, col)
                cv.px(3, 3 + k, col)
        cv.px(3, 3, col)
        out.append(cv)
    return flipbook(out, [6, 2, 3, 2])
