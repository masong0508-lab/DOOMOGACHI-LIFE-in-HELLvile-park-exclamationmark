"""props.py - props the sets share: the banner, its drip, the balloon, the ladder,
candles, the egg and its basket, the small lamp, sign boards, the poster.

Readable words on signs are strings (show/ep101/strings.shw, drawn by the player
on a sign prop's meta['text'] box). The banner's paint is art: GIGGLE painted
over MINGLE, with MINGLE showing through, is a picture, not a caption.
"""
import math

import numpy as np

from . import generator, pal
from .canvas import Canvas, Rng, hash2, bayer
from . import kit
from .kit import R, P, D, L
from .crayon import big_letter

# ---------------------------------------------------------------------------
# the banner: GIGGLE (wet pink) painted over MINGLE
# ---------------------------------------------------------------------------
SIZES = {
    # name: (letter w, letter h, gap, margin)
    "ms": (35, 42, 5, 8),          # co12-18: matched to the crayon banner (BANNER in crayon.py)
    "wide": (12, 16, 3, 5),        # on the bandstand eave (figure scale)
    "title": (24, 30, 4, 8),       # MT-7 title card / CR-1 head card
}

BLOOM = {"giggle": 0, "bloom1": 1, "bloom2": 2, "bloom3": 3}


@generator()
def banner(size="ms", state="giggle", word2="", seed=7):
    """The bandstand banner. state: mingle (the old banner), giggle (wet GIGGLE),
    bloom1/2/3 (the N of MINGLE blooming back through the third letter; 3 is red),
    peeled (CR-1: GIGGLE peeling off, MINGLE plain). word2 adds a second painted word
    under it (NIGHT for the title card)."""
    lw, lh, gap, mg = SIZES[size]
    n = 6
    tw = n * lw + (n - 1) * gap
    rows = 2 if word2 else 1
    ch = lh * rows + mg * (rows + 1) + (gap * 2 if rows == 2 else 0)
    cw = tw + mg * 2
    cv = Canvas(cw + 8, ch + 10)
    ox, oy = 4, 4
    # cloth with a scalloped hem and a pink border stripe
    cv.rect(ox, oy, cw, ch, pal.CREAM)
    cv.rect(ox, oy, cw, 2, R("PASTEL_PINK", 5))
    cv.rect(ox, oy + ch - 3, cw, 2, R("PASTEL_PINK", 5))
    for x in range(ox, ox + cw, 8):
        cv.ellipse(x + 4, oy + ch, 4, 3, pal.CREAM)
        cv.px(x + 4, oy + ch + 2, R("PASTEL_PINK", 5))
    # cloth folds
    for x in range(ox + 14, ox + cw - 6, 22):
        cv.rect(x, oy + 3, 1, ch - 6, R("PEACH", 7))
    m = cv.a != 0
    cv.a[cv.outline_mask(m)] = R("PEACH", 3)
    # rope loops
    for x in (ox + 2, ox + cw - 4):
        cv.rect(x, 0, 2, 5, R("WOOD", 3))
    old = "MINGLE"
    new = "GIGGLE"
    bloom = BLOOM.get(state, 0)
    for k in range(n):
        x = ox + mg + k * (lw + gap)
        y = oy + mg
        om = big_letter(old[k], lw, lh)
        nm = big_letter(new[k], lw, lh)
        sub = cv.a[y:y + lh, x:x + lw]
        if state == "mingle":
            sub[om] = P("ROSE", 1)
            continue
        if state == "peeled":
            sub[om] = P("ROSE", 1)
            # flakes of the pink coat still clinging
            rng = Rng(seed + k)
            for _ in range(lw * lh // 30):
                fx, fy = rng.randint(0, lw - 2), rng.randint(0, lh - 2)
                sub[fy:fy + 2, fx:fx + 2] = P("BUBBLEGUM", 2)
            continue
        # the roller band: wet pink over the whole letter cell
        band = sub.copy()
        sub[:, :] = P("BUBBLEGUM", 2)
        for yy in range(0, lh, 5):
            sub[yy, :] = R("PASTEL_PINK", 6)
        # MINGLE shows through the wet paint
        sub[om & ~nm] = R("PASTEL_PINK", 4)
        # the new letter, deep pink with a wet highlight
        sub[nm] = P("BUBBLEGUM", 0)
        hl = nm & ~np.roll(nm, 1, axis=0)
        sub[hl] = P("BUBBLEGUM", 1)
        if k == 2 and bloom:
            # the N blooming back: its strokes push through the pink, bigger each time
            bl = om & ~nm
            rng = Rng(seed + bloom)
            ys, xs = np.nonzero(bl)
            keep = [i for i in range(len(xs)) if rng.rand() < (0.35, 0.7, 1.0)[bloom - 1]]
            col = (P("ROSE", 1), P("ROSE", 0), P("CHERRY", 1))[bloom - 1]
            for i in keep:
                sub[ys[i], xs[i]] = col
            if bloom == 3:
                sub[om & nm] = P("CHERRY", 0)
        # wet gloss dots
        sub[2, 2] = pal.WHITE
        sub[3, 2] = R("PASTEL_PINK", 7)
    if word2:
        y = oy + mg * 2 + lh + gap * 2
        w2 = len(word2) * lw + (len(word2) - 1) * gap
        x0 = ox + (cw - w2) // 2
        for k, chh in enumerate(word2):
            x = x0 + k * (lw + gap)
            sub = cv.a[y:y + lh, x:x + lw]
            sub[big_letter(chh, lw, lh)] = P("GRAPE", 1)
    cv.meta["letters"] = [(ox + mg + k * (lw + gap), oy + mg) for k in range(n)]
    cv.meta["anchor"] = "top"
    return cv


@generator()
def paint_drip(size="ms", color="red", length=12):
    """The drip from the third letter (D16: candy red, an A/B pink version for the animatic)."""
    c = {"red": (P("CHERRY", 1), P("CHERRY", 0), P("CHERRY", 2)),
         "pink": (P("BUBBLEGUM", 1), P("BUBBLEGUM", 0), P("BUBBLEGUM", 2))}[color]
    w = 5 if size == "ms" else 3
    n = int(length)
    cv = Canvas(w + 2, n + 4)
    cx = (w + 2) // 2
    for y in range(n):
        hw = 1 if y < n - 2 else 0
        cv.rect(cx - hw, y, 2 * hw + 1, 1, c[0])
    cv.ellipse(cx + 0.5, n + 0.5, w / 2, 2.2, c[0])
    cv.px(cx - 1, n - 1, c[2])
    cv.a[cv.outline_mask(cv.a != 0) & (np.arange(cv.h)[:, None] > 0)] = c[1]
    cv.meta["anchor"] = "top"
    return cv


@generator()
def cupcake_balloon(face="front", size=1):
    """The Mayor's cupcake balloon tied over the N: 'front' faces the town, 'wall'
    faces the Wall (turned to its right), with its string."""
    s = int(size)
    cv = Canvas(26 * s, 44 * s)
    cx = 13 * s
    # frosting dome
    dome = cv.ellipse_mask(cx, 12 * s, 11 * s, 9 * s)
    cv.shape(dome, P("BUBBLEGUM", 1), outline=P("ROSE", 0), hi=P("BUBBLEGUM", 2))
    cv.ellipse(cx - 3 * s, 8 * s, 3 * s, 2 * s, P("BUBBLEGUM", 2))
    # cherry
    cv.ellipse(cx, 3 * s, 2.5 * s, 2.5 * s, P("CHERRY", 1))
    cv.px(cx - 1, 2 * s, pal.WHITE)
    # wrapper
    wr = cv.poly_mask([(cx - 10 * s, 17 * s), (cx + 10 * s, 17 * s), (cx + 7 * s, 28 * s), (cx - 7 * s, 28 * s)])
    cv.a[wr] = R("CANDY", 5)
    xs, ys = cv.grid()
    cv.a[wr & ((xs - cx) % (4 * s) == 0)] = R("CANDY", 3)
    cv.a[cv.outline_mask(wr) & ~dome] = R("CANDY", 2)
    # face: two dot eyes and a smile, looking the way it faces
    off = 0 if face == "front" else 4 * s
    for ex in (cx - 4 * s + off, cx + 3 * s + off):
        if 0 <= ex < cv.w:
            cv.rect(ex, 11 * s, s, 2 * s, R("STONE", 0))
    if face == "front":
        cv.px(cx - 1, 15 * s, R("STONE", 0))
        cv.px(cx, 15 * s + 1, R("STONE", 0))
        cv.px(cx + 1, 15 * s, R("STONE", 0))
    else:
        cv.px(cx + 5 * s, 15 * s, R("STONE", 0))
        cv.px(cx + 6 * s, 15 * s + 1, R("STONE", 0))
    # knot and string
    cv.px(cx, 29 * s, R("CANDY", 2))
    for y in range(30 * s, 44 * s):
        cv.px(cx + int(round(math.sin(y / (3.0 * s)) * 1.2)), y, R("STONE", 4))
    cv.meta["anchor"] = "bottom"
    return cv


@generator()
def ladder(h=110, seed=1):
    """A painter's step-ladder, side on (the Mayor climbs it in co12-18)."""
    h = int(h)
    cv = Canvas(30, h)
    for k, (xa, xb) in enumerate(((4, 10), (24, 18))):
        cv.line(xa, h - 1, xb, 0, R("WOOD", 4), 3)
        cv.line(xa + 1, h - 1, xb + 1, 0, R("WOOD", 6), 1)
    for y in range(10, h - 4, 14):
        t = y / h
        xa = 10 - 6 * t + 2
        xb = 18 + 6 * t - 2
        cv.rect(int(xa), y, int(xb - xa) + 1, 3, R("WOOD", 5))
        cv.rect(int(xa), y, int(xb - xa) + 1, 1, R("WOOD", 6))
    cv.rect(8, 0, 14, 3, R("WOOD", 3))
    cv.a[cv.outline_mask(cv.a != 0)] = R("WOOD", 1)
    cv.meta["anchor"] = "bottom"
    return cv


def _upscale(cv, k):
    k = int(k)
    if k <= 1:
        return cv
    out = Canvas(cv.w * k, cv.h * k)
    out.a = np.repeat(np.repeat(cv.a, k, 0), k, 1)
    out.meta = dict(cv.meta)
    return out


@generator()
def candle(state="lit", seed=1, size=1):
    return _upscale(_candle(state, seed), size)


def _candle(state="lit", seed=1):
    """A rite candle (10x24): lit, out (a curl of smoke), blue (the last flame as a blue
    bead), lean (its flame leans right, toward the Wall), flash (the 3-f red flash when
    a try fails)."""
    cv = Canvas(12, 26)
    cv.rect(3, 10, 6, 15, pal.CREAM)
    cv.rect(3, 10, 1, 15, pal.WHITE)
    cv.rect(8, 10, 1, 15, R("PEACH", 6))
    cv.rect(2, 24, 8, 2, R("CANDY", 4))
    cv.ellipse(6, 10, 3, 1.2, pal.WHITE)
    cv.px(6, 8, R("STONE", 1))
    cv.px(6, 9, R("STONE", 1))
    if state == "lit":
        cv.ellipse(6, 5, 2.2, 3.6, R("CANDY", 6))
        cv.ellipse(6, 6, 1.2, 2, pal.WHITE)
        cv.px(6, 1, R("CANDY", 5))
    elif state == "lean":
        cv.ellipse(7.5, 5, 2.2, 3.4, R("CANDY", 6))
        cv.ellipse(7.2, 6, 1.2, 2, pal.WHITE)
        cv.px(9, 2, R("CANDY", 5))
    elif state == "blue":
        cv.ellipse(6, 6.5, 1.3, 1.6, P("SKY", 1))
        cv.px(6, 6, P("CYAN", 2))
    elif state == "flash":
        cv.ellipse(6, 5, 3.2, 4.6, P("CHERRY", 1))
        cv.ellipse(6, 6, 1.5, 2.2, P("CHERRY", 2))
    else:
        for k, y in enumerate(range(7, 0, -1)):
            cv.px(6 + int(round(math.sin(y * 1.1))), y, pal.GRAY_L if k % 2 else pal.GRAY)
    cv.a[cv.outline_mask(cv.a != 0) & (np.arange(cv.h)[:, None] > 9)] = R("PEACH", 3)
    cv.meta["anchor"] = "bottom"
    return cv


@generator()
def egg(state="whole", size=1):
    """The egg (16x20 at size 1): whole, wobble (tilted), cracked."""
    s = int(size)
    cv = Canvas(18 * s, 22 * s)
    m = cv.ellipse_mask(9 * s, 12 * s, 7 * s, 9 * s)
    m &= ~(cv.grid()[1] < 6 * s) | cv.ellipse_mask(9 * s, 9 * s, 5.5 * s, 7 * s)
    cv.shape(m, pal.CREAM, outline=R("PEACH", 3), lo=R("PEACH", 6), lo_side="right")
    cv.ellipse(6 * s, 8 * s, 1.5 * s, 2 * s, pal.WHITE)
    for (x, y) in ((11, 14), (8, 17), (12, 9)):
        cv.ellipse(x * s, y * s, 1.2 * s, 1.2 * s, R("PASTEL_PINK", 6))
    if state == "cracked":
        pts = [(3, 12), (6, 10), (8, 13), (11, 10), (13, 13), (15, 11)]
        cv.polyline([(x * s, y * s) for x, y in pts], R("PEACH", 2))
    cv.meta["anchor"] = "bottom"
    return cv


@generator()
def small_lamp(lit=1):
    """Nana's small lamp set beside the egg (12x18)."""
    cv = Canvas(14, 20)
    cv.poly([(2, 9), (12, 9), (10, 2), (4, 2)], P("LEMON", 2) if int(lit) else P("LEMON", 0))
    cv.rect(4, 2, 6, 1, pal.CREAM)
    cv.rect(6, 9, 2, 8, R("WOOD", 3))
    cv.rect(3, 17, 8, 2, R("WOOD", 4))
    if int(lit):
        cv.rect(5, 6, 4, 3, R("CANDY", 7))
    cv.a[cv.outline_mask(cv.a != 0)] = R("WOOD", 1)
    cv.meta["anchor"] = "bottom"
    return cv


@generator()
def gift_tag(turned=0):
    """The tag on the basket: blank face (the words 'for whoever's next' are a string)."""
    cv = Canvas(22, 14)
    cv.poly([(4, 1), (21, 1), (21, 12), (4, 12), (1, 6)], pal.CREAM)
    cv.px(4, 6, R("STONE", 3))
    cv.a[cv.outline_mask(cv.a != 0)] = R("PEACH", 3)
    if int(turned):
        cv.a = cv.a[:, ::-1].copy()
    cv.meta["text"] = (6, 3, 14, 8)
    return cv


@generator()
def sign(w=60, h=24, face="cream", frame="wood", posts=0, style="plank", seed=1):
    """A blank sign board; meta['text'] is the box the player draws the string in."""
    faces = {"cream": pal.CREAM, "pink": R("PASTEL_PINK", 6), "mint": P("MINT", 2),
             "lemon": P("LEMON", 2), "white": pal.WHITE, "lavender": P("LAVENDER", 2),
             "red": P("CHERRY", 1), "grey": R("STONE", 5), "chalk": R("STONE", 1)}
    frames = {"wood": R("WOOD", 3), "pink": P("BUBBLEGUM", 1), "steel": R("STEEL", 3),
              "white": pal.WHITE, "gold": P("HONEY", 1)}
    return kit.sign_board(int(w), int(h), faces.get(face, pal.CREAM), frames.get(frame, R("WOOD", 3)),
                          posts=int(posts), style=style, seed=int(seed))


@generator()
def poster(w=30, h=40, seed=1):
    """A festival poster pinned to a lamp post (its words are a string)."""
    w, h = int(w), int(h)
    cv = Canvas(w, h)
    cv.rect(0, 0, w, h, pal.CREAM)
    cv.rect(0, 0, w, 6, P("BUBBLEGUM", 1))
    kit.heart(cv, w / 2, h - 10, 5, P("BUBBLEGUM", 1))
    for x in (2, w - 3):
        cv.px(x, 1, R("STEEL", 6))
    cv.a[cv.outline_mask(cv.a != 0)] = R("PEACH", 3)
    cv.meta["text"] = (2, 7, w - 4, h - 20)
    return cv


@generator()
def fog_bank(w=200, h=40, seed=1, color="vape"):
    """A bank of the purple fog (dithered wisps), for 1-14's fog over the boards."""
    w, h = int(w), int(h)
    cv = Canvas(w, h)
    rng = Rng(seed)
    b = bayer(w, h)
    for k in range(int(w / 18)):
        cx, cy = rng.uniform(0, w), rng.uniform(h * 0.45, h * 0.8)
        rx, ry = rng.uniform(18, 34), rng.uniform(7, 13)
        m = cv.ellipse_mask(cx, cy, rx, ry)
        cv.a[m & (b < 11)] = R("VAPE", 3) if color == "vape" else R("STONE", 5)
        m2 = cv.ellipse_mask(cx, cy - 2, rx * 0.6, ry * 0.6)
        cv.a[m2 & (b < 7)] = R("VAPE", 5) if color == "vape" else R("STONE", 6)
    cv.meta["anchor"] = "bottom"
    return cv


@generator()
def red_eyes(n=2, seed=1):
    """A pair of red eyes in the dark (emissive: they stay lit at night)."""
    cv = Canvas(10, 4)
    for x in (1, 6):
        cv.rect(x, 1, 3, 2, P("HELLFIRE", 1))
        cv.px(x + 1, 1, P("HELLFIRE", 2))
    return cv


@generator()
def lost_property_lantern(state="lit"):
    """The LOST PROPERTY lantern with Sunny's fallen '1' hanging from it (2-03, PC-1)."""
    cv = Canvas(16, 30)
    lan = kit.lantern_img(P("LEMON", 1), state)
    cv.blit(lan, 3, 0)
    cv.rect(7, 13, 1, 4, R("STONE", 3))
    # the brass '1' and its crayon tag
    cv.rect(6, 17, 3, 9, P("HONEY", 1))
    cv.rect(5, 18, 1, 2, P("HONEY", 1))
    cv.rect(5, 25, 5, 1, P("HONEY", 0))
    cv.rect(10, 18, 6, 4, pal.CREAM)
    cv.px(11, 19, P("CHERRY", 1))
    cv.px(13, 20, P("CHERRY", 1))
    cv.meta["anchor"] = "top"
    return cv


@generator()
def basket(size=1):
    """A small wicker basket with a gingham tea towel (the egg sits in it), 34x20 at size 1."""
    s = int(size)
    cv = Canvas(36 * s, 30 * s)
    bx, by = 18 * s, 26 * s
    body = cv.poly_mask([(bx - 16 * s, by - 12 * s), (bx + 16 * s, by - 12 * s), (bx + 13 * s, by + 2 * s),
                         (bx - 13 * s, by + 2 * s)])
    cv.a[body] = R("WOOD", 5)
    xs, ys = cv.grid()
    cv.a[body & (((xs // (2 * s)) + (ys // (2 * s))) % 2 == 0)] = R("WOOD", 4)
    cv.a[cv.outline_mask(body)] = R("WOOD", 1)
    towel = cv.ellipse_mask(bx, by - 13 * s, 14 * s, 3.5 * s)
    cv.a[towel] = pal.WHITE
    cv.a[towel & (((xs // (2 * s)) + (ys // (2 * s))) % 2 == 0)] = R("PASTEL_PINK", 6)
    for k in range(30):
        a = math.pi + k / 29 * math.pi
        cv.px(bx + math.cos(a) * 15 * s, by - 13 * s + math.sin(a) * 13 * s, R("WOOD", 3))
    cv.meta["anchor"] = "bottom"
    return cv
