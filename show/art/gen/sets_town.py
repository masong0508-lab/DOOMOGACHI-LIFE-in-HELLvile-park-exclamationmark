"""sets_town.py - HaHaland beyond the fair: Candy Hills Road, the town sign, the
fountain plaza and Nana's bakery window, the launch hill and the joke graveyard,
the Wall seen edge-on for the split screen, and the two inserts beyond the Wall.

Figure scale unless noted (feet at show y ~150-170). Day colours; time of day is
a palette variant.
"""
import math

import numpy as np

from . import generator, pal
from .canvas import Canvas, Rng, hash2, bayer
from . import kit
from .kit import R, P, D, L, Y
from . import font as gfont


# ---------------------------------------------------------------------------
# 1-01 Candy Hills Road (sunset) and the bus
# ---------------------------------------------------------------------------
@generator()
def candy_hills(seed=300, w=480):
    """The far plane: rolling candy hills (pink, mint, lemon), lollipop trees, a striped
    candy-cane fence along a ridge, HaHaland's roofs peeping over the last hill."""
    w = int(w)
    cv = Canvas(w, 240)
    base = Y(116)
    kit.hills(cv, base - 44, 18, 320, R("PASTEL_PINK", 5), seed, top=R("PASTEL_PINK", 6), bottom=240)
    # town roofs over the far hill
    for k in range(9):
        x = 40 + k * 46 + hash2(k, 1, seed) % 16
        y = base - 52 + hash2(k, 2, seed) % 6
        cv.rect(x, y, 16, 10, R("PEACH", 7))
        cv.poly([(x - 2, y), (x + 18, y), (x + 8, y - 8)], [P("BUBBLEGUM", 2), P("SKY", 2), P("LEMON", 2)][k % 3])
    kit.hills(cv, base - 26, 14, 260, P("MINT", 2), seed + 1, top=R("MINT", 7), bottom=240)
    kit.hills(cv, base - 8, 10, 200, P("LEMON", 2), seed + 2, top=R("CANDY", 7), bottom=240)
    for k in range(12):
        x = 12 + k * 40 + hash2(k, 3, seed) % 14
        y = base - 4 + hash2(k, 4, seed) % 8
        cv.rect(x, y - 14, 1, 14, pal.WHITE)
        c = [P("BUBBLEGUM", 1), P("MINT", 1), P("TANGERINE", 1)][k % 3]
        m = cv.ellipse_mask(x + 0.5, y - 17, 5, 5)
        cv.a[m] = c
        xs, ys = cv.grid()
        cv.a[m & (((xs - x) + (ys - y)) % 4 == 0)] = pal.WHITE
        cv.a[cv.outline_mask(m)] = D(c, 2)
    # candy-cane fence along the mint ridge
    for x in range(0, w, 7):
        y = base - 20 + int(4 * math.sin(x / 40.0 + seed))
        cv.rect(x, y - 8, 2, 8, pal.WHITE)
        cv.px(x, y - 6, P("CHERRY", 1))
        cv.px(x + 1, y - 3, P("CHERRY", 1))
    return cv


@generator()
def candy_road(seed=301, w=640):
    """The road plane: a lemon-cream road with a dashed pink centre line, grassy verges with
    sugar-flowers, a bus stop post at x 160 (show x at cam 0 of the stop: 0)."""
    w = int(w)
    cv = Canvas(w, 240)
    top = Y(132)
    kit.grass(cv, 0, Y(120), w, top - Y(120), seed, ramp_lo=4, ramp_hi=6)
    cv.rect(0, top, w, 30, R("CANDY", 6))
    cv.rect(0, top, w, 1, R("CANDY", 7))
    cv.rect(0, top + 1, w, 1, R("CANDY", 4))
    for x in range(0, w, 24):
        cv.rect(x + 4, top + 15, 12, 2, P("BUBBLEGUM", 1))
    cv.rect(0, top + 29, w, 1, R("CANDY", 3))
    kit.grass(cv, 0, top + 30, w, 240 - top - 30, seed + 1, ramp_lo=3, ramp_hi=5)
    # bus stop post with its (blank) sign: a lollipop disc
    bx = w // 2
    cv.rect(bx - 1, top - 44, 3, 46, R("STEEL", 4))
    disc = cv.ellipse_mask(bx + 0.5, top - 50, 8, 8)
    cv.shape(disc, P("BUBBLEGUM", 1), outline=P("ROSE", 0), hi=pal.WHITE)
    cv.rect(bx - 4, top - 51, 9, 2, pal.WHITE)
    return cv


@generator()
def pink_bus(state="drive", seed=1):
    """The pink bus, side-on (120x62): drive (door shut), stop (door open, step out)."""
    cv = Canvas(124, 64)
    body = cv.poly_mask([(4, 52), (4, 14), (12, 6), (116, 6), (120, 12), (120, 52)])
    cv.shape(body, P("BUBBLEGUM", 1), outline=P("ROSE", 0), hi=P("BUBBLEGUM", 2))
    cv.rect(5, 38, 115, 4, pal.WHITE)
    cv.rect(5, 42, 115, 2, P("BUBBLEGUM", 0))
    for k in range(6):
        x = 16 + k * 16
        kit.window(cv, x, 14, 12, 12, frame=pal.WHITE, glass=R("SKY", 6), cross=False)
    # windscreen and door at the front (right: it drives right)
    cv.poly([(108, 12), (118, 12), (119, 30), (108, 30)], R("SKY", 6))
    if state == "stop":
        cv.rect(96, 14, 10, 36, R("STONE", 1))
        cv.rect(94, 50, 14, 3, R("STEEL", 4))
    else:
        cv.rect(96, 14, 10, 36, R("SKY", 5))
        cv.rect(100, 14, 1, 36, pal.WHITE)
    for x in (24, 98):
        w_ = cv.ellipse_mask(x, 54, 9, 9)
        cv.shape(w_, R("STEEL", 1), outline=R("STEEL", 0))
        cv.ellipse(x, 54, 4, 4, R("STEEL", 5))
        cv.px(x, 54, R("STEEL", 2))
    cv.rect(116, 40, 4, 4, R("CANDY", 7))      # headlight
    kit.heart(cv, 60, 46, 3, pal.WHITE)
    cv.meta["anchor"] = "bottom"
    return cv


# ---------------------------------------------------------------------------
# 1-02 the town sign
# ---------------------------------------------------------------------------
@generator()
def town_sign_board(seed=310):
    """HaHaland's welcome sign: a big scalloped board on two candy posts. The words are
    strings; the population number is made of digit tiles (props), and under it a patch of
    fresh paint hides an old 1000 (that ghost is art: it is paint, not a caption)."""
    cv = Canvas(176, 112)
    # posts
    for x in (24, 146):
        for y in range(40, 112):
            cv.rect(x, y, 6, 1, pal.WHITE if (y // 4) % 2 else P("CHERRY", 1))
        cv.frame(x - 1, 40, 8, 72, P("CHERRY", 0))
    # board with a scalloped top
    bm = np.zeros((112, 176), bool)
    bm[14:84, 4:172] = True
    for x in range(4, 172, 14):
        bm |= cv.ellipse_mask(x + 7, 14, 7, 6) & (cv.grid()[1] < 16)
    cv.a[bm] = R("PEACH", 7)
    cv.a[cv.inner_edge(bm, "top")] = pal.WHITE
    cv.rect(10, 20, 156, 58, pal.CREAM)
    cv.frame(10, 20, 156, 58, R("PEACH", 4))
    cv.a[cv.outline_mask(bm)] = R("WOOD", 2)
    # hearts in the corners
    kit.heart(cv, 20, 30, 4, P("BUBBLEGUM", 1))
    kit.heart(cv, 156, 30, 4, P("BUBBLEGUM", 1))
    # the paint patch over the old number (lighter cream), with the 1000 showing through
    px0, py0 = 92, 50
    cv.rect(px0 - 4, py0 - 3, 46, 15, R("PEACH", 7))
    ghost = Canvas(46, 12)
    gfont.draw_text(ghost, 2, 1, "1000", R("PEACH", 5), scale=1, spacing=4)
    cv.blit(ghost, px0 - 2, py0 - 1)
    cv.meta["text"] = {"title": (24, 24, 128, 18), "line": (16, 50, 70, 10), "number": (92, 50, 40, 10),
                       "tail": (16, 64, 144, 10), "pencil": (100, 86, 70, 10)}
    cv.meta["anchor"] = "bottom"
    return cv


@generator()
def digit(ch="1", seed=1):
    """A painted digit tile for the population number (8x12): it can fall off."""
    cv = Canvas(8, 12)
    cv.rect(0, 0, 8, 12, P("BUBBLEGUM", 2))
    cv.frame(0, 0, 8, 12, P("ROSE", 0))
    gfont.draw_text(cv, 2, 2, str(ch), P("ROSE", 0))
    cv.meta["anchor"] = "topleft"
    return cv


@generator()
def sign_verge(seed=311, w=352):
    """The verge by the road where the sign stands: grass running downhill to the right
    toward the lanterns of the Giggle (a few lantern dots far below)."""
    w = int(w)
    cv = Canvas(w, 240)
    m = np.zeros((240, w), bool)
    for x in range(w):
        top = Y(124) + int((x / w) * 26 + 3 * math.sin(x / 31.0))
        m[top:, x] = True
    cv.a[m] = R("GRASS", 4)
    kit.grass(cv, 0, Y(124), w, 240 - Y(124), seed, ramp_lo=3, ramp_hi=5)
    cv.a[~m] = 0
    cv.a[cv.inner_edge(m, "top")] = R("GRASS", 6)
    # the road edge at the bottom left
    for x in range(0, w // 2):
        y = Y(168) + x // 12
        cv.rect(x, y, 1, 240 - y, R("CANDY", 6))
        cv.px(x, y, R("CANDY", 7))
    # far lantern dots downhill (the Giggle)
    for k in range(10):
        x = w - 120 + k * 11
        y = Y(118) + k * 2 + int(2 * math.sin(k))
        cv.px(x, y, R("CANDY", 7))
        cv.px(x, y + 1, kit.LANTERN_COLS[k % 5])
    return cv


# ---------------------------------------------------------------------------
# CO-6 / 1-03 the fountain plaza and the park gate
# ---------------------------------------------------------------------------
def fountain(cv, cx, base, scale=1.0, seed=1):
    """The Giggling Fountain: a round basin, a pedestal, two laughing stone faces
    back to back, water arcing from both mouths."""
    s = scale
    bw, bh = 70 * s, 14 * s
    basin = cv.ellipse_mask(cx, base - bh / 2, bw / 2, bh / 2 + 2)
    cv.shape(basin, R("STONE", 6), outline=R("STONE", 2), hi=R("STONE", 7))
    cv.rect(cx - bw / 2, base - bh / 2, bw, bh / 2, R("STONE", 5))
    water = cv.ellipse_mask(cx, base - bh / 2 - 1, bw / 2 - 4, bh / 2 - 3)
    cv.a[water] = R("SKY", 5)
    xs, ys = cv.grid()
    cv.a[water & ((xs + ys * 3) % 9 == 0)] = R("SKY", 7)
    # pedestal
    cv.rect(cx - 5 * s, base - bh - 26 * s, 10 * s, 26 * s, R("STONE", 5))
    cv.rect(cx - 5 * s, base - bh - 26 * s, 2 * s, 26 * s, R("STONE", 6))
    # two faces (round heads) back to back, mouths open laughing
    for side in (-1, 1):
        hx = cx + side * 9 * s
        hy = base - bh - 36 * s
        head = cv.ellipse_mask(hx, hy, 11 * s, 10 * s)
        cv.shape(head, R("STONE", 6), outline=R("STONE", 2), hi=R("STONE", 7))
        ex = hx + side * 4 * s
        cv.rect(ex - 2 * s, hy - 4 * s, 3 * s, 1 * s + 1, R("STONE", 2))     # squeezed laughing eyes
        mouth = cv.ellipse_mask(hx + side * 6 * s, hy + 3 * s, 3.5 * s, 3 * s)
        cv.a[mouth & head] = R("STONE", 1)
        # water arc from the mouth
        for k in range(int(24 * s)):
            t = k / (24 * s)
            x = hx + side * (8 * s + t * 26 * s)
            y = hy + 3 * s - math.sin(t * 3.1) * 10 * s + t * 22 * s
            cv.px(x, y, R("SKY", 6) if k % 3 else pal.WHITE)
            cv.px(x, y + 1, R("SKY", 5))
    return basin


@generator()
def plaza_far(seed=320, w=480):
    """The plaza's back: pastel shopfronts (Nana's bakery among them), and far behind
    them the bandstand's roof and the Wall."""
    w = int(w)
    cv = Canvas(w, 240)
    base = Y(116)
    # the Wall far behind (right) and the bandstand's peak
    cv.rect(w - 150, base - 92, 150, 60, R("STONE", 4))
    kit.bricks(cv, w - 150, base - 92, 150, 60, seed, base=R("STONE", 4), mortar=R("STONE", 3), bw=10, bh=5,
               chips=False)
    cv.poly([(w - 190, base - 76), (w - 166, base - 96), (w - 142, base - 76)], R("PASTEL_PINK", 4))
    cv.rect(w - 186, base - 76, 40, 24, R("PASTEL_PINK", 6))
    # shopfronts
    x = 0
    k = 0
    cols = [(P("PEACH", 2), P("BUBBLEGUM", 1)), (P("MINT", 2), P("SKY", 1)), (P("LEMON", 2), P("TANGERINE", 1)),
            (P("LAVENDER", 2), P("GRAPE", 1)), (R("PASTEL_PINK", 6), P("CHERRY", 1))]
    while x < w - 120:
        sw = 56 + hash2(k, 7, seed) % 20
        sh = 56 + hash2(k, 8, seed) % 18
        wall, roof = cols[k % len(cols)]
        cv.rect(x, base - sh, sw, sh, wall)
        cv.rect(x, base - sh, sw, 2, L(wall, 1))
        cv.poly([(x - 3, base - sh), (x + sw + 3, base - sh), (x + sw // 2, base - sh - 14)], roof)
        kit.awning(cv, x + 4, base - 30, sw - 8, 6, [roof, pal.WHITE], stripe=5)
        kit.window(cv, x + 8, base - sh + 10, 12, 12, lit=False)
        kit.window(cv, x + sw - 20, base - sh + 10, 12, 12, lit=False)
        cv.rect(x + sw // 2 - 5, base - 20, 10, 20, D(roof, 1))
        cv.frame(x, base - sh, sw, sh, D(wall, 3))
        x += sw + 6
        k += 1
    kit.hills(cv, base + 2, 2, 400, R("STONE", 5), seed, bottom=240)
    return cv


@generator()
def plaza(seed=321, w=560):
    """The plaza: pink cobbles, lamp posts with lantern strings between them, benches,
    flower tubs, the Giggling Fountain in the foreground centre, the park gate arch at the
    right (1-03: Sunny follows the lanterns through it)."""
    w = int(w)
    cv = Canvas(w, 240)
    top = Y(112)
    kit.cobbles(cv, 0, top, w, 240 - top, seed, base=R("PASTEL_PINK", 6), mortar=R("PASTEL_PINK", 4),
                rows_px=(4, 9))
    cv.rect(0, top, w, 2, R("PASTEL_PINK", 3))
    # lamp posts
    lamps = [40, 150, 410, 520]
    for x in lamps:
        kit.lamp_post(cv, x, top + 18, 56)
    # park gate arch on the right
    gx = 470
    for side in (-1, 1):
        x = gx + side * 34
        cv.rect(x - 4, top - 62, 9, 80, R("WOOD", 4))
        cv.rect(x - 4, top - 62, 2, 80, R("WOOD", 5))
        cv.frame(x - 5, top - 63, 11, 82, R("WOOD", 1))
    arch = cv.ellipse_mask(gx, top - 56, 42, 16) & ~cv.ellipse_mask(gx, top - 52, 32, 10)
    arch &= cv.grid()[1] < top - 52
    cv.shape(arch, R("WOOD", 4), outline=R("WOOD", 1), hi=R("WOOD", 5))
    # flower tubs and benches
    for x in (88, 236, 330):
        cv.rect(x, top + 6, 20, 10, R("WOOD", 3))
        kit.bush(cv, x + 10, top + 7, 22, 10, fill=R("GRASS", 5), seed=x,
                 flowers=[P("BUBBLEGUM", 2), P("LEMON", 2), pal.WHITE])
    # the fountain, front and centre
    fountain(cv, w // 2 - 30, Y(166), 1.0, seed)
    cv.meta["lamps"] = [(x - (w - 320) // 2, top + 18 - 64 - 30) for x in lamps]
    return cv


@generator()
def fountain_faces(seed=330):
    """co14 insert at px=2 (160x120): the two laughing stone faces close up, water bubbling."""
    cv = Canvas(160, 120)
    cv.rect(0, 0, 160, 120, R("SKY", 6))
    cv.vgrad(0, 0, 160, 120, [R("SKY", 6), R("PASTEL_PINK", 7), R("PEACH", 7)])
    for k, (hx, side) in enumerate(((54, -1), (106, 1))):
        hy = 66
        head = cv.ellipse_mask(hx, hy, 30, 28)
        cv.shape(head, R("STONE", 6), outline=R("STONE", 1), hi=R("STONE", 7))
        xs, ys = cv.grid()
        cv.a[head & ((xs - hx) * side > 14)] = R("STONE", 5)
        # laughing eyes (upturned arcs)
        for ex in (hx - 10, hx + 10):
            for t in range(-4, 5):
                cv.px(ex + t, hy - 10 - (4 - abs(t)) // 2, R("STONE", 1))
                cv.px(ex + t, hy - 9 - (4 - abs(t)) // 2, R("STONE", 2))
        # cheeks
        cv.ellipse(hx - 16, hy + 2, 4, 3, R("STONE", 7))
        cv.ellipse(hx + 16, hy + 2, 4, 3, R("STONE", 7))
        # open laughing mouth with water bubbling out
        mouth = cv.ellipse_mask(hx, hy + 12, 12, 8) & (ys > hy + 7)
        cv.a[mouth] = R("STONE", 1)
        cv.a[cv.ellipse_mask(hx, hy + 16, 6, 3) & mouth] = P("ROSE", 0)
        for k2 in range(5):
            bx, by = hx - 8 + k2 * 4, hy + 20 + (k2 % 2) * 3
            cv.ellipse(bx, by, 2, 2, R("SKY", 6))
            cv.px(bx - 1, by - 1, pal.WHITE)
        cv.rect(hx - 30, 104, 60, 16, R("STONE", 5))
    # the basin water at the bottom
    cv.rect(0, 108, 160, 12, R("SKY", 5))
    for x in range(0, 160, 6):
        cv.px(x + (hash2(x, 1, seed) % 4), 110 + hash2(x, 2, seed) % 8, pal.WHITE)
    return cv


# ---------------------------------------------------------------------------
# CO-6 Nana's bakery window (co20) and the egg (co21)
# ---------------------------------------------------------------------------
@generator()
def bakery_window(seed=340):
    """co20 (MS): Nana's bakery window from outside: a peach shopfront with a striped
    awning, a big window with a display shelf of two of everything (two loaves, two buns,
    two cakes); the basket, egg, lamp and tag are props; Nana's paw is an actor."""
    cv = Canvas(336, 240)
    cv.rect(0, 0, 336, 240, P("PEACH", 2))
    for y in range(0, 240, 10):
        cv.rect(0, y, 336, 1, P("PEACH", 1))
        for x in range((y // 10) % 2 * 10, 336, 20):
            cv.rect(x, y, 1, 10, P("PEACH", 1))
    kit.awning(cv, 0, Y(-6), 336, 22, [P("BUBBLEGUM", 1), pal.WHITE], stripe=12)
    # window
    wx0, wy0, wx1, wy1 = 34, Y(30), 302, Y(150)
    cv.rect(wx0 - 6, wy0 - 6, wx1 - wx0 + 12, wy1 - wy0 + 12, pal.WHITE)
    cv.rect(wx0 - 6, wy0 - 6, wx1 - wx0 + 12, 2, R("STONE", 6))
    cv.rect(wx0, wy0, wx1 - wx0, wy1 - wy0, R("PEACH", 6))
    cv.vgrad(wx0, wy0, wx1 - wx0, wy1 - wy0, [R("CANDY", 7), R("PEACH", 7), R("PEACH", 6)])
    # inside: a back shelf with jars, warm light
    cv.rect(wx0, wy0 + 30, wx1 - wx0, 3, R("WOOD", 4))
    for k, x in enumerate(range(wx0 + 10, wx1 - 10, 22)):
        c = [P("BUBBLEGUM", 2), P("LEMON", 2), P("MINT", 2)][k % 3]
        cv.rect(x, wy0 + 18, 10, 12, pal.WHITE)
        cv.rect(x + 1, wy0 + 22, 8, 8, c)
        cv.rect(x + 2, wy0 + 16, 6, 2, P("CARAMEL", 1))
    # display shelf: two loaves, two buns, two cakes
    sy = wy1 - 20
    cv.rect(wx0, sy, wx1 - wx0, 4, R("WOOD", 5))
    cv.rect(wx0, sy + 4, wx1 - wx0, 2, R("WOOD", 2))
    items = []
    x = wx0 + 14
    for kind in ("loaf", "loaf", "bun", "bun", "cake", "cake"):
        if kind == "loaf":
            m = cv.ellipse_mask(x + 12, sy - 6, 13, 7) & (cv.grid()[1] < sy)
            cv.shape(m, P("CARAMEL", 1), outline=P("CHOCOLATE", 1), hi=P("CARAMEL", 2))
            for k in range(3):
                cv.line(x + 6 + k * 6, sy - 10, x + 9 + k * 6, sy - 6, P("CARAMEL", 0))
            x += 30
        elif kind == "bun":
            m = cv.ellipse_mask(x + 8, sy - 5, 8, 5) & (cv.grid()[1] < sy)
            cv.shape(m, R("CANDY", 5), outline=P("CHOCOLATE", 1), hi=R("CANDY", 7))
            cv.px(x + 6, sy - 8, pal.WHITE)
            cv.px(x + 10, sy - 7, pal.WHITE)
            x += 22
        else:
            cv.rect(x, sy - 14, 22, 14, pal.WHITE)
            cv.rect(x, sy - 9, 22, 4, P("BUBBLEGUM", 2))
            cv.rect(x, sy - 16, 22, 2, P("BUBBLEGUM", 1))
            cv.px(x + 11, sy - 18, P("CHERRY", 1))
            cv.frame(x, sy - 16, 22, 16, R("PASTEL_PINK", 3))
            x += 30
    # glass glints and the frame bars
    for k in range(3):
        cv.line(wx0 + 8 + k * 5, wy0 + 50, wx0 + 30 + k * 5, wy0 + 4, pal.WHITE)
    cv.rect((wx0 + wx1) // 2 - 1, wy0, 3, wy1 - wy0, pal.WHITE)
    cv.frame(wx0 - 7, wy0 - 7, wx1 - wx0 + 14, wy1 - wy0 + 14, R("STONE", 4))
    # sill and planter box
    cv.rect(wx0 - 10, wy1 + 6, wx1 - wx0 + 20, 6, pal.WHITE)
    cv.rect(wx0 - 10, wy1 + 12, wx1 - wx0 + 20, 2, R("STONE", 5))
    cv.rect(wx0, wy1 + 14, wx1 - wx0, 14, R("WOOD", 3))
    kit.bush(cv, (wx0 + wx1) // 2, wy1 + 16, wx1 - wx0, 12, fill=R("GRASS", 5), seed=seed,
             flowers=[P("BUBBLEGUM", 2), pal.WHITE, P("LEMON", 2)])
    cv.meta["sill_y"] = wy1 - 20 - 30
    return cv


@generator()
def egg_basket(seed=350):
    """co21 at px=2 (160x120): the basket close up in the window's warm light, a tea towel
    tucked in; the egg (px=2 prop), the lamp and the tag sit on it."""
    cv = Canvas(160, 120)
    cv.vgrad(0, 0, 160, 120, [R("CANDY", 7), R("PEACH", 7), R("PEACH", 6), R("PEACH", 5)])
    # window frame edges at the sides, soft
    cv.rect(0, 0, 8, 120, pal.WHITE)
    cv.rect(152, 0, 8, 120, pal.WHITE)
    # the shelf
    cv.rect(0, 96, 160, 24, R("WOOD", 5))
    cv.rect(0, 96, 160, 2, R("WOOD", 6))
    kit.planks(cv, 0, 98, 160, 22, seed, base=R("WOOD", 5), pw=7)
    # basket
    bx, by = 80, 92
    body = cv.poly_mask([(bx - 36, by - 24), (bx + 36, by - 24), (bx + 30, by + 4), (bx - 30, by + 4)])
    cv.a[body] = R("WOOD", 5)
    xs, ys = cv.grid()
    cv.a[body & (((xs // 3) + (ys // 3)) % 2 == 0)] = R("WOOD", 4)
    cv.a[cv.outline_mask(body)] = R("WOOD", 1)
    rim = cv.ellipse_mask(bx, by - 24, 37, 5)
    cv.shape(rim, R("WOOD", 6), outline=R("WOOD", 1))
    # tea towel (gingham) tucked round where the egg sits
    towel = cv.ellipse_mask(bx, by - 26, 30, 7) | cv.poly_mask([(bx - 34, by - 26), (bx - 44, by - 6), (bx - 28, by - 14)])
    cv.a[towel] = pal.WHITE
    cv.a[towel & (((xs // 3) + (ys // 3)) % 2 == 0)] = R("PASTEL_PINK", 6)
    cv.a[cv.outline_mask(towel)] = R("PASTEL_PINK", 3)
    # handle
    for k in range(60):
        a = math.pi + k / 59 * math.pi
        cv.px(bx + math.cos(a) * 34, by - 26 + math.sin(a) * 30, R("WOOD", 3))
        cv.px(bx + math.cos(a) * 33, by - 26 + math.sin(a) * 29, R("WOOD", 5))
    return cv


# ---------------------------------------------------------------------------
# 4-07 the joke graveyard path and 4-08 the launch hill
# ---------------------------------------------------------------------------
def gravestone(cv, x, base, w, h, style, seed):
    c = R("STONE", 5)
    if style == "round":
        m = cv.poly_mask([(x, base), (x, base - h + w // 2), (x + w, base - h + w // 2), (x + w, base)])
        m |= cv.ellipse_mask(x + w / 2, base - h + w / 2, w / 2, w / 2)
    elif style == "cross":
        m = np.zeros((cv.h, cv.w), bool)
        m[base - h:base, x + w // 2 - 2:x + w // 2 + 2] = True
        m[base - h + 4:base - h + 8, x:x + w] = True
    else:
        m = np.zeros((cv.h, cv.w), bool)
        m[base - h:base, x:x + w] = True
    cv.shape(m, c, outline=R("STONE", 2), hi=R("STONE", 6))
    rng = Rng(seed)
    if rng.chance(0.5):
        cv.rect(x + 2, base - h // 2, w - 4, 1, R("STONE", 3))
    return m


@generator()
def joke_graveyard(seed=360, w=480):
    """4-07: the path up to the launch hill through the joke graveyard: rubber-chicken
    headstones, a whoopee-cushion urn, gravestones with blank faces (the epitaphs are
    strings; the child-sized FLUFFY stone and its cupcake are props)."""
    w = int(w)
    cv = Canvas(w, 240)
    # ART review: 4-07's board note keeps the sky out of frame (camera low on the path), so
    # a dark yew wall and a row of lollipop trees fill everything above the fence; only a
    # few stars peek through the top rows. Lantern glints keep it from reading as a void.
    rng0 = Rng(seed + 50)
    yew = np.zeros((240, w), bool)
    for x in range(w):
        yew[max(0, 6 + int(4 * math.sin(x / 11.0 + seed)) + rng0.randint(0, 2)):Y(110), x] = True
    cv.a[yew] = R("GRASS", 2)
    cv.a[yew & (bayer(w, 240) < 4)] = R("GRASS", 1)
    cv.a[cv.inner_edge(yew, "top")] = R("GRASS", 3)
    for k, x in enumerate(range(10, w, 38)):
        kit.tree_round(cv, x + rng0.randint(-6, 6), Y(96), 14 + rng0.randint(0, 6), leaf=R("GRASS", 3),
                       seed=seed + 60 + k)
    for k in range(0, w, 46):
        kit.glow_pool(cv, k + 20, Y(54), 10, 6, c=P("LEMON", 2), levels=(6, 3, 1))
        cv.px(k + 20, Y(54), kit.LANTERN_COLS[(k // 46) % 5])
    m, crest = kit.hills(cv, Y(96), 10, 300, R("GRASS", 4), seed, top=R("GRASS", 5), lumps=2, bottom=240)
    kit.grass(cv, 0, Y(110), w, 240 - Y(110), seed + 1, ramp_lo=3, ramp_hi=5, flowers=False)
    # the path, rising to the right
    pm = np.zeros((240, w), bool)
    for x in range(w):
        y0 = Y(150) - int(x / w * 30)
        pm[y0:y0 + 18, x] = True
    cv.a[pm] = R("PEACH", 5)
    cv.a[pm & (bayer(w, 240) < 3)] = R("PEACH", 4)
    cv.a[cv.inner_edge(pm, "top")] = R("PEACH", 6)
    # stones along both sides of the path
    rng = Rng(seed)
    x = 8
    k = 0
    while x < w - 30:
        base = Y(146) - int(x / w * 30) - 2
        style = ["round", "flat", "cross", "round"][k % 4]
        gw, gh = rng.randint(14, 20), rng.randint(18, 28)
        gravestone(cv, x, base, gw, gh, style, seed + k)
        if k % 3 == 1:
            # a rubber chicken draped over a stone
            cv.line(x + 2, base - gh + 2, x + gw - 2, base - gh - 2, P("LEMON", 1), 2)
            cv.px(x + gw - 1, base - gh - 3, P("TANGERINE", 1))
        x += gw + rng.randint(18, 34)
        k += 1
    # a picket fence along the back
    kit.picket_fence(cv, 0, w, Y(112), 12, pal.WHITE)
    return cv


@generator()
def small_stone(seed=361):
    """The child-sized stone (FLUFFY THE CLOUD. DRIFTED. is a string in a child's hand)."""
    cv = Canvas(22, 22)
    m = cv.ellipse_mask(11, 9, 10, 9) | cv.poly_mask([(1, 9), (21, 9), (21, 22), (1, 22)])
    cv.shape(m, R("STONE", 6), outline=R("STONE", 2), hi=R("STONE", 7))
    cv.meta["text"] = (3, 5, 16, 12)
    cv.meta["anchor"] = "bottom"
    return cv


@generator()
def cupcake(seed=1):
    cv = Canvas(10, 10)
    cv.poly([(1, 5), (9, 5), (8, 9), (2, 9)], R("CANDY", 5))
    cv.ellipse(5, 4, 4.5, 3.2, P("BUBBLEGUM", 2))
    cv.px(5, 0, P("CHERRY", 1))
    cv.px(5, 1, P("CHERRY", 1))
    cv.a[cv.outline_mask(cv.a != 0)] = P("ROSE", 0)
    cv.meta["anchor"] = "bottom"
    return cv


@generator()
def launch_hill(seed=370, w=400):
    """4-08: the top of the launch hill looking south. Below lies the whole park, dark, with
    the Wall straight down the frame's centre line (plate x = w/2 = show x 160 at cam 0):
    HaHaland's lanterns on the left, Hellvile's rooftops on the right. The hill crest is
    the foreground; the iron launch stand is a prop. The sky (top third) stays open for
    the firework."""
    w = int(w)
    cv = Canvas(w, 240)
    hz = Y(52)            # the far horizon
    near = Y(124)         # where the crest hides the valley
    cx = w // 2
    rng = Rng(seed)
    # left: HaHaland, dark lawns in bands that recede, lanterns, tiny tents and the bandstand
    for k, (y, c) in enumerate(((hz, R("GRASS", 2)), (hz + 18, R("GRASS", 3)), (hz + 40, R("GRASS", 3)))):
        kit.hills(cv, y, 3 + k, 160, c, seed + k, top=L(c, 1), bottom=near + 6)
    cv.a[:, cx:] = 0
    for k in range(40):
        x = rng.randint(4, cx - 10)
        y = rng.randint(hz + 6, near)
        cv.px(x, y, kit.LANTERN_COLS[k % 5])
    for tx, c in ((40, P("BUBBLEGUM", 1)), (70, P("LAVENDER", 1)), (104, P("MINT", 1))):
        cv.poly([(tx - 5, hz + 30), (tx + 5, hz + 30), (tx, hz + 22)], c)
    bx = cx - 52
    cv.poly([(bx - 9, hz + 24), (bx + 9, hz + 24), (bx, hz + 16)], P("CHERRY", 1))   # bandstand roof
    cv.rect(bx - 7, hz + 24, 14, 5, pal.WHITE)
    # right: Hellvile, crooked rooftops on purple ground with ember windows
    cv.rect(cx, hz, w - cx, near + 6 - hz, R("VAPE", 1))
    x = cx + 6
    while x < w:
        bw = rng.randint(10, 22)
        bh = rng.randint(10, 26)
        base = hz + 10 + rng.randint(0, 50)
        c = R("VAPE", 0)
        cv.rect(x, base - bh, bw, bh, c)
        cv.poly([(x - 2, base - bh), (x + bw + 2, base - bh), (x + bw // 2, base - bh - 9)], c)
        if rng.chance(0.6):
            cv.rect(x + 3, base - bh + 4, 2, 2, P("HELLFIRE", 1))
        x += bw + rng.randint(2, 10)
    # the Wall: straight down the centre line, narrow at the horizon, wider near
    wall = cv.poly_mask([(cx - 2, hz - 6), (cx + 2, hz - 6), (cx + 9, near + 6), (cx - 9, near + 6)])
    tmp = Canvas(w, 240)
    kit.bricks(tmp, 0, 0, w, 240, seed + 1, base=R("STONE", 3), mortar=R("STONE", 1), bw=6, bh=3,
               chips=False)
    cv.a[wall] = tmp.a[wall]
    cv.a[cv.outline_mask(wall)] = R("STONE", 1)
    cv.rect(cx - 2, hz - 7, 4, 1, R("STONE", 5))
    # the hill crest in front
    m = np.zeros((240, w), bool)
    for xx in range(w):
        top = near - int(14 * math.cos((xx / w - 0.5) * 2.6))
        m[top:, xx] = True
    g = Canvas(w, 240)
    kit.grass(g, 0, near - 20, w, 240 - near + 20, seed + 3, ramp_lo=3, ramp_hi=5)
    cv.a[m] = g.a[m]
    cv.a[cv.inner_edge(m, "top")] = R("GRASS", 6)
    return cv


@generator()
def launch_stand(seed=371):
    """The old iron launch stand on the crest (24x30): rusted A-frame and a rail pointing
    south, over the Wall."""
    cv = Canvas(24, 30)
    iron, rust = R("STONE", 2), R("WOOD", 3)
    cv.line(4, 29, 12, 6, iron, 2)
    cv.line(20, 29, 12, 6, iron, 2)
    cv.line(7, 20, 17, 20, iron)
    cv.line(12, 4, 12, 26, R("STONE", 4), 2)       # the rail
    for (x, y) in ((6, 25), (17, 23), (10, 13), (14, 16)):
        cv.px(x, y, rust)
    cv.rect(10, 2, 5, 3, R("STONE", 3))
    cv.meta["anchor"] = "bottom"
    return cv


# ---------------------------------------------------------------------------
# the Wall edge-on: the split screen (MT-3, MT-6, 4-08)
# ---------------------------------------------------------------------------
@generator()
def wall_split(seed=380, side="both"):
    """Both sides at once, the Wall's cross-section on x = 152..168 (the brick-wipe line,
    x = 160). Left: HaHaland, pastel lawn and cottages; right: Hellvile, crooked goth roofs
    under a purple sky, silhouettes only (no faces). 320x240, par 0."""
    cv = Canvas(320, 240)
    base = Y(150)
    # left: HaHaland sky is a BANDS plane; draw its ground and cottages
    kit.hills(cv, base - 30, 8, 200, R("GRASS", 5), seed, top=R("GRASS", 6), bottom=240)
    cv.a[:, 152:] = 0
    kit.cottage(cv, 24, base - 18, 46, 52, seed=seed)
    kit.cottage(cv, 92, base - 22, 40, 44, wall=P("MINT", 2), roof=P("SKY", 1), seed=seed + 1)
    kit.tree_round(cv, 76, base - 14, 10, seed=seed + 2)
    kit.grass(cv, 0, base, 152, 240 - base, seed + 3, ramp_lo=3, ramp_hi=5)
    kit.picket_fence(cv, 0, 150, base + 4, 10)
    # right: Hellvile (sky is part of this plate so the left BANDS sky never shows there)
    cv.rect(168, 0, 152, base, R("VAPE", 1))
    for y in range(0, base, 1):
        t = y / base
        c = R("VAPE", 0) if t < 0.3 else (R("VAPE", 1) if t < 0.65 else R("VAPE", 2))
        cv.a[y, 168:] = c
    for y in (Y(30), Y(70)):
        cv.dither_rect(168, y - 2, 152, 4, cv.a[y - 3, 200], cv.a[y + 3, 200], 8)
    rng = Rng(seed + 9)
    x = 172
    while x < 320:
        bw = rng.randint(16, 30)
        bh = rng.randint(40, 80)
        c = R("SKY", 0)
        cv.rect(x, base - bh, bw, bh, c)
        cv.poly([(x - 3, base - bh), (x + bw + 3, base - bh), (x + bw // 2 + rng.randint(-4, 4), base - bh - 18)], c)
        if rng.chance(0.6):
            cv.rect(x + 5, base - bh + 10, 4, 5, P("HELLFIRE", 1))
        x += bw + rng.randint(2, 8)
    cv.rect(168, base, 152, 240 - base, R("STONE", 1))
    for xx in range(170, 320, 9):
        cv.rect(xx, base - 12, 2, 12, R("STONE", 0))
        cv.px(xx, base - 13, R("STONE", 0))
    cv.rect(168, base - 9, 152, 2, R("STONE", 0))
    # the Wall: a tall cross-section of brick down the centre
    kit.bricks(cv, 152, 0, 16, 240, seed + 5, base=R("STONE", 4), mortar=R("STONE", 1), bw=8, bh=6, chips=False)
    cv.rect(152, 0, 1, 240, R("STONE", 6))
    cv.rect(167, 0, 1, 240, R("STONE", 0))
    return cv


@generator()
def hellvile_table(seed=381):
    """4-08 split: on the Hellvile half, a table set with two bowls and a fuse (the sooty
    hand is an actor). Bottom-anchored prop, 60x40."""
    cv = Canvas(60, 40)
    cv.rect(4, 22, 52, 5, R("WOOD", 2))
    cv.rect(4, 22, 52, 1, R("WOOD", 3))
    for x in (8, 50):
        cv.rect(x, 27, 3, 13, R("WOOD", 1))
    for k, x in enumerate((18, 38)):
        m = cv.ellipse_mask(x, 19, 7, 4) & (cv.grid()[1] > 17)
        cv.shape(m, R("BONE", 5), outline=R("BONE", 1))
        cv.ellipse(x, 18, 6, 1.5, R("BLOOD", 3) if False else P("TANGERINE", 0))
    # the little firework on its stand, fuse trailing
    cv.rect(52, 6, 4, 16, P("GRAPE", 1))
    cv.poly([(51, 6), (57, 6), (54, 1)], P("VAPE_PURPLE", 2))
    cv.line(54, 22, 58, 30, R("STONE", 4))
    cv.a[cv.outline_mask(cv.a != 0)] = R("STONE", 0)
    cv.meta["anchor"] = "bottom"
    return cv


# ---------------------------------------------------------------------------
# the inserts: beyond the Wall (1-13) and the kids' tent memory (1-14)
# ---------------------------------------------------------------------------
@generator()
def pipe_yard(seed=390):
    """1-13 INSERT (4 f): beyond the Wall, a big pipe heaves a purple sigh. No faces.
    The Wall's back on the left, a fat riveted pipe elbowing out of the ground."""
    cv = Canvas(320, 240)
    cv.rect(0, 0, 320, 240, R("VAPE", 0))
    for y in range(0, 240):
        if y > 120:
            cv.a[y, :] = R("STONE", 1) if y > 170 else R("VAPE", 1)
    kit.bricks(cv, 0, 0, 70, 200, seed, base=R("STONE", 2), mortar=R("STONE", 0), bw=16, bh=8)
    # the pipe: rises from the ground, bends toward camera-left
    px0 = 170
    cv.rect(px0, 110, 46, 100, R("STEEL", 3))
    cv.rect(px0, 110, 6, 100, R("STEEL", 4))
    cv.rect(px0 + 40, 110, 6, 100, R("STEEL", 2))
    elbow = cv.ellipse_mask(px0 + 23, 110, 23, 23)
    cv.shape(elbow, R("STEEL", 3), outline=R("STEEL", 0), hi=R("STEEL", 5))
    cv.rect(px0 - 70, 87, 93, 46, R("STEEL", 3))
    cv.rect(px0 - 70, 87, 93, 6, R("STEEL", 4))
    cv.rect(px0 - 70, 127, 93, 6, R("STEEL", 2))
    for x in (px0 - 70, px0 - 30, px0 + 4):
        cv.rect(x, 85, 6, 50, R("STEEL", 2))
        for y in range(89, 133, 7):
            cv.px(x + 3, y, R("STEEL", 6))
    cv.ellipse(px0 - 72, 110, 8, 24, R("STEEL", 1))      # the open mouth of the pipe
    cv.ellipse(px0 - 72, 110, 5, 19, R("VAPE", 0))
    cv.a[cv.outline_mask(cv.a == R("STEEL", 3))] = R("STEEL", 0)
    return cv


@generator()
def sigh_puff(seed=391):
    """The purple sigh from the pipe (a prop, 80x56)."""
    cv = Canvas(80, 56)
    b = bayer(80, 56)
    rng = Rng(seed)
    for k in range(9):
        cx, cy, r = rng.uniform(14, 66), rng.uniform(14, 42), rng.uniform(8, 16)
        m = cv.ellipse_mask(cx, cy, r, r * 0.8)
        cv.a[m & (b < 12)] = R("VAPE", 4)
        cv.a[cv.ellipse_mask(cx - 2, cy - 2, r * 0.5, r * 0.4) & (b < 9)] = R("VAPE", 6)
    return cv


@generator()
def kids_tent_old(seed=392):
    """1-14 INSERT (4 f, sepia): the kids' tent eleven years ago tonight: a striped tent
    inside, little chairs, drawings; the cloud-shaped lady and the crowned silhouette are
    cast (cloud_lady, crowned_shadow)."""
    cv = Canvas(320, 240)
    for x in range(320):
        cv.rect(x, 0, 1, Y(140), P("BUBBLEGUM", 2) if (x // 20) % 2 == 0 else pal.CREAM)
    roof = cv.poly_mask([(0, 0), (320, 0), (320, Y(14)), (160, Y(-20)), (0, Y(14))])
    cv.a[roof] = P("BUBBLEGUM", 1)
    kit.planks(cv, 0, Y(140), 320, 240 - Y(140), seed, base=R("WOOD", 4), pw=8)
    for k, x in enumerate((40, 90, 230, 280)):
        cv.rect(x, Y(130), 18, 4, [P("SKY", 1), P("LEMON", 1), P("MINT", 1), P("BUBBLEGUM", 1)][k])
        for xx in (x + 1, x + 15):
            cv.rect(xx, Y(134), 2, 12, R("WOOD", 3))
        cv.rect(x + 1, Y(118), 2, 12, R("WOOD", 3))
    pts = kit.catenary(10, Y(30), 310, Y(30), 12)
    kit.draw_string(cv, pts, R("STONE", 3))
    for i in range(14, len(pts), 32):
        x, y = pts[i]
        cv.rect(x - 8, y + 1, 16, 18, pal.WHITE)
        cv.frame(x - 8, y + 1, 16, 18, R("STONE", 5))
        cv.ellipse(x, y + 10, 4, 3, [P("SKY", 1), P("LIME", 1), P("TANGERINE", 1)][(i // 32) % 3])
    return cv
