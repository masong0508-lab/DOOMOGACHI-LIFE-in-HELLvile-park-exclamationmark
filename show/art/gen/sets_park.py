"""sets_park.py - Hellvile Park's north half: the bandstand, the Wall and the sealed gate.

The bandstand is the only set drawn in both looks (crayon in the storybook,
pixel art here). The pixel bandstand is a TILEMAP plane (16x16 tiles, deduped)
at figure scale (a 48x80 figure on the deck has its feet at show y 124):

  plate x   0..160   the fair's edge: Miss Lulla's craft tent, Fuzz's FREE HUGS booth
  plate x 160..330   the north half: live, painted, lanterns, the GIGGLE banner
  plate x 330..500   the south half: boarded up, faded; the MINGLE crate under the deck
  plate x 512..800   the Wall (its face towards HaHaland) and the sealed gate (x 640)

Everything is drawn in day colours; dawn, dusk, night, dark and vape are CLUT
variants (light.py). Lamps and lantern cores use emissive indices.
"""
import math

import numpy as np

from . import generator, pal
from .canvas import Canvas, Rng, hash2, bayer
from . import kit
from .kit import R, P, D, L, Y
from .tilemap import to_tilemap

W = 800
CX = 330                   # bandstand centre (plate x); frame 'wide' is cam -70
DECK = Y(124)              # deck top (plate y)
GROUND = Y(150)            # where the deck skirt meets the ground
EAVE = Y(10)               # eave line (bottom of the roof)
DECK_X0, DECK_X1 = CX - 170, CX + 170
POSTS = [CX - 160 + k * 64 for k in range(6)]
SPLIT = CX                 # north half | south half
WALL_X = 512
GATE_X = 640
STEPS = (CX - 86, CX - 26)  # the front steps, north half


def _ground(cv, x0, x1, seed):
    """Lawn in front of the bandstand with a worn path to the steps."""
    kit.grass(cv, x0, GROUND, x1 - x0, cv.h - GROUND, seed, ramp_lo=3, ramp_hi=5, period=64)
    # worn dirt patch in front of the steps
    m = cv.ellipse_mask((STEPS[0] + STEPS[1]) / 2, GROUND + 22, 44, 16) & (np.arange(cv.h)[:, None] >= GROUND)
    b = bayer(cv.w, cv.h)
    cv.a[m & (b < 11)] = R("PEACH", 5)
    cv.a[m & (b >= 11)] = R("PEACH", 4)
    cv.rect(x0, GROUND, x1 - x0, 1, R("GRASS", 2))


def _roof(cv, x0, x1, faded_from=None, seed=1):
    """The pavilion roof band above the eave: pink scallop shingles, a valance."""
    top = 0
    for y in range(top, EAVE):
        row = (y - top) // 4
        for x in range(x0, x1):
            faded = faded_from is not None and x >= faded_from
            base = R("PASTEL_PINK", 4) if not faded else R("STONE", 4)
            k = ((x - x0 + (row % 2) * 4) // 8)
            yy = (y - top) % 4
            c = base
            if yy == 3:
                c = D(base, 1)
            elif yy == 0 and (x - x0 + (row % 2) * 4) % 8 in (2, 3, 4, 5):
                c = L(base, 1)
            if faded and hash2(x // 8, row, seed) % 7 == 0:
                c = R("STONE", 2)          # a missing shingle
            cv.a[y, x] = c
    # valance: scallops under the eave
    for x in range(x0, x1, 10):
        faded = faded_from is not None and x >= faded_from
        k = (x - x0) // 10
        c = (pal.CREAM if k % 2 == 0 else R("PASTEL_PINK", 5)) if not faded else (
            R("STONE", 5) if k % 2 == 0 else R("STONE", 4))
        cv.rect(x, EAVE, 10, 5, c)
        cv.ellipse(x + 5, EAVE + 5, 5, 3.5, c)
        cv.px(x + 5, EAVE + 8, D(c, 2))
    cv.rect(x0, EAVE - 2, x1 - x0, 2, R("PASTEL_PINK", 2) if faded_from is None else R("PASTEL_PINK", 2))
    if faded_from is not None:
        cv.rect(faded_from, EAVE - 2, x1 - faded_from, 2, R("STONE", 2))
    m = np.zeros((cv.h, cv.w), bool)
    m[EAVE:EAVE + 10, x0:x1] = cv.a[EAVE:EAVE + 10, x0:x1] != 0
    cv.a[cv.outline_mask(m) & (np.arange(cv.h)[:, None] > EAVE + 2)] = R("PASTEL_PINK", 1)


def _post(cv, x, faded=False, seed=1):
    """A candy-cane post (north) or a grey peeled post (south)."""
    top, bot = EAVE + 6, DECK
    if not faded:
        for y in range(top, bot):
            for dx in range(6):
                s = ((y + dx * 2) // 4) % 3
                c = pal.WHITE if s else R("PASTEL_PINK", 5)
                if dx == 5:
                    c = D(c, 1)
                if dx == 0:
                    c = L(c, 1) if c != pal.WHITE else pal.WHITE
                cv.a[y, x - 3 + dx] = c
        cv.rect(x - 5, top, 10, 3, pal.CREAM)
        cv.rect(x - 5, bot - 3, 10, 3, pal.CREAM)
    else:
        for y in range(top, bot):
            for dx in range(6):
                c = R("STONE", 4) if dx < 4 else R("STONE", 3)
                if hash2(x + dx, y // 3, seed) % 11 == 0:
                    c = R("PASTEL_PINK", 4)        # a fleck of the old paint
                cv.a[y, x - 3 + dx] = c
        cv.rect(x - 5, top, 10, 3, R("STONE", 5))
        cv.rect(x - 5, bot - 3, 10, 3, R("STONE", 5))
    cv.frame(x - 5, top, 10, bot - top, R("STONE", 1) if faded else R("PASTEL_PINK", 2))
    cv.rect(x - 4, top + 3, 1, bot - top - 6, R("STONE", 1) if faded else R("PASTEL_PINK", 2))
    cv.rect(x + 3, top + 3, 1, bot - top - 6, R("STONE", 1) if faded else R("PASTEL_PINK", 2))


def _railing(cv, x0, x1, gaps=()):
    top = DECK - 18
    cv.rect(x0, top, x1 - x0, 3, pal.WHITE)
    cv.rect(x0, top + 3, x1 - x0, 1, R("STONE", 5))
    cv.rect(x0, DECK - 3, x1 - x0, 2, pal.CREAM)
    for x in range(x0 + 2, x1 - 2, 6):
        if any(a <= x <= b for a, b in gaps):
            continue
        cv.rect(x, top + 4, 3, 14, pal.WHITE)
        cv.px(x + 2, top + 4, R("STONE", 5))
        cv.rect(x + 2, top + 5, 1, 13, R("STONE", 6))
    for a, b in gaps:
        cv.rect(a, top, b - a, 4, 0)
        cv.rect(a, DECK - 3, b - a, 2, 0)


def _deck(cv, seed):
    # deck top surface (a thin strip of boards seen from slightly above)
    kit.planks(cv, DECK_X0, DECK - 1, DECK_X1 - DECK_X0, 5, seed, base=R("WOOD", 5), pw=5)
    cv.rect(DECK_X0, DECK + 4, DECK_X1 - DECK_X0, 2, R("WOOD", 3))
    # skirt: lattice on the north half, dark under the boarded south half
    y0, y1 = DECK + 6, GROUND
    cv.rect(DECK_X0, y0, SPLIT - DECK_X0, y1 - y0, R("WOOD", 1))
    for y in range(y0, y1):
        for x in range(DECK_X0, SPLIT):
            if (x + y) % 8 in (0, 1) or (x - y) % 8 in (0, 1):
                cv.a[y, x] = pal.CREAM if (x // 4 + y // 4) % 2 else R("PEACH", 6)
    cv.rect(DECK_X0, y0, SPLIT - DECK_X0, 2, pal.WHITE)
    # south half: the dark hollow under the deck (the crate prop sits in it; boards cover it)
    cv.rect(SPLIT, y0, DECK_X1 - SPLIT, y1 - y0, R("STONE", 0))
    cv.dither_rect(SPLIT, y0, DECK_X1 - SPLIT, 4, R("STONE", 1), R("STONE", 0), 8)
    cv.rect(SPLIT, y0, DECK_X1 - SPLIT, 2, R("STONE", 4))
    # skirt end posts
    for x in (DECK_X0, SPLIT - 2, DECK_X1 - 4):
        cv.rect(x, DECK, 4, GROUND - DECK, R("WOOD", 3) if x < SPLIT else R("STONE", 3))
    cv.frame(DECK_X0 - 1, DECK - 2, DECK_X1 - DECK_X0 + 2, GROUND - DECK + 2, R("WOOD", 1))


def _steps(cv, seed):
    x0, x1 = STEPS
    n = 3
    hgt = (GROUND - DECK) / n
    for k in range(n):
        y = int(DECK + 2 + k * hgt)
        grow = (k + 1) * 4
        kit.planks(cv, x0 - grow, y, x1 - x0 + 2 * grow, int(hgt), seed + k, base=R("WOOD", 5 - (k % 2)), pw=4)
        cv.rect(x0 - grow, y, x1 - x0 + 2 * grow, 1, R("WOOD", 6))
        cv.rect(x0 - grow, y + int(hgt) - 1, x1 - x0 + 2 * grow, 1, R("WOOD", 2))
        cv.rect(x0 - grow, y, 1, int(hgt), R("WOOD", 2))
        cv.rect(x1 + grow - 1, y, 1, int(hgt), R("WOOD", 2))


def _boards(cv, x0, x1, y0, y1, seed, shade=0):
    """Weathered planks nailed across an opening, with two cross braces; a few gaps show
    the dark inside, one plank hangs loose, an old festival poster is half torn off."""
    rng = Rng(seed)
    cv.rect(x0, y0, x1 - x0, y1 - y0, R("STONE", 0))
    y = y0
    while y < y1:
        hgt = rng.randint(7, 10)
        c = rng.choice([R("WOOD", 3), R("WOOD", 2), R("STONE", 4), R("WOOD", 3)])
        if shade:
            c = D(c, shade)
        tilt = rng.randint(-2, 2)
        for x in range(x0, x1):
            dy = int(round(tilt * (x - x0) / max(1, x1 - x0)))
            cv.rect(x, y + dy, 1, hgt - 1, c)
            cv.px(x, y + dy, L(c, 1))
            cv.px(x, y + dy + hgt - 1, D(c, 2))
        for k in range(3):
            fx = rng.randint(x0 + 4, x1 - 6)
            cv.px(fx, y + rng.randint(2, hgt - 3), D(c, 1))
        for nx in (x0 + 3, x1 - 4):
            cv.px(nx, y + hgt // 2, R("STONE", 6))
        if rng.chance(0.35):
            px_ = rng.randint(x0 + 6, x1 - 10)
            cv.rect(px_, y + 2, rng.randint(2, 5), 2, R("PASTEL_PINK", 4))     # old paint
        y += hgt + (2 if rng.chance(0.3) else 0)                              # a gap: the dark inside
    # braces
    for (ax, ay, bx, by) in ((x0, y0 + 4, x1, y1 - 4), (x0, y1 - 4, x1, y0 + 4)):
        cv.line(ax, ay, bx, by, R("WOOD", 2), 3)
        cv.line(ax, ay - 1, bx, by - 1, R("WOOD", 4), 1)
    if x1 - x0 > 80 and y1 - y0 > 60:
        # a torn poster (pink, its corner flapping) and a hand-lettered KEEP OFF board (a string)
        px_, py_ = x0 + (x1 - x0) // 3, y0 + (y1 - y0) // 3
        cv.poly([(px_, py_), (px_ + 22, py_ - 2), (px_ + 24, py_ + 26), (px_ + 4, py_ + 30)], R("PASTEL_PINK", 5))
        cv.poly([(px_ + 12, py_ + 30), (px_ + 24, py_ + 26), (px_ + 20, py_ + 36)], R("PASTEL_PINK", 3))
        kit.heart(cv, px_ + 12, py_ + 12, 5, P("BUBBLEGUM", 0))
        cv.px(px_ + 2, py_ + 2, R("STEEL", 6))
        cv.px(px_ + 21, py_, R("STEEL", 6))
        # one plank hanging loose by a nail
        lx, ly = x1 - (x1 - x0) // 4, y1 - 22
        cv.line(lx - 20, ly - 6, lx + 14, ly + 12, R("WOOD", 3), 5)
        cv.line(lx - 20, ly - 8, lx + 14, ly + 10, R("WOOD", 4), 1)
        cv.px(lx - 18, ly - 6, R("STEEL", 6))


def _wall(cv, x0, x1, seed):
    """The Wall's face: big grey blocks, darker towards the base, its end pilaster at x0."""
    kit.bricks(cv, x0, 0, x1 - x0, GROUND, seed, base=R("STONE", 3), mortar=R("STONE", 1), bw=20, bh=10,
               period=4)
    b = bayer(cv.w, cv.h)
    for y in range(GROUND - 40, GROUND):
        lvl = int((y - (GROUND - 40)) / 40 * 9)
        row = cv.a[y, x0:x1]
        m = b[y, x0:x1] < lvl
        row[m] = pal.DARKER[row[m]]
    # end pilaster (the Wall's thickness, catching the light)
    cv.rect(x0, 0, 12, GROUND, R("STONE", 4))
    cv.rect(x0, 0, 2, GROUND, R("STONE", 5))
    cv.rect(x0 + 10, 0, 2, GROUND, R("STONE", 1))
    for y in range(4, GROUND, 14):
        cv.rect(x0, y, 12, 1, R("STONE", 2))
    # cracks, moss and a weedy foot
    rng = Rng(seed + 5)
    for _ in range(6):
        cx, cy = rng.randint(x0 + 20, x1 - 10), rng.randint(20, GROUND - 30)
        pts = [(cx, cy)]
        for k in range(rng.randint(3, 6)):
            cx += rng.randint(-3, 3)
            cy += rng.randint(2, 5)
            pts.append((cx, cy))
        cv.polyline(pts, R("STONE", 0))
    for x in range(x0, x1, 3):
        hh = 2 + hash2(x, 7, seed) % 5
        cv.vline(x, GROUND - hh, GROUND - 1, R("GRASS", 3 + (x % 2)))
    cv.rect(x0, GROUND - 1, x1 - x0, 1, R("STONE", 0))


def gate_art(cv, gx, seed, base=None, scale=1):
    """The sealed gate: a stone arch, a riveted iron door, the small barred grille
    (shutter shut). The seven padlocks and the KEEP OUT sign are props."""
    base = GROUND if base is None else base
    s = scale
    aw, ah = 70 * s, 116 * s
    x0 = gx - aw // 2
    top = base - ah
    # arch stones
    am = cv.poly_mask([(x0 - 8 * s, base), (x0 - 8 * s, top + 26 * s), (gx, top - 10 * s),
                       (x0 + aw + 8 * s, top + 26 * s), (x0 + aw + 8 * s, base)])
    cv.a[am] = R("STONE", 5)
    xs, ys = cv.grid()
    joints = am & ((((ys - top) // (12 * s)) % 1 == 0) & (((xs - x0) * 3 + (ys - top) * 2) % (22 * s) < 1))
    cv.a[joints] = R("STONE", 3)
    cv.a[cv.outline_mask(am)] = R("STONE", 1)
    # door
    dm = cv.poly_mask([(x0, base), (x0, top + 26 * s), (gx, top), (x0 + aw, top + 26 * s), (x0 + aw, base)])
    cv.a[dm] = R("STEEL", 2)
    for x in range(x0, x0 + aw, 10 * s):
        cv.a[dm & (xs == x)] = R("STEEL", 1)
        cv.a[dm & (xs == x + 1)] = R("STEEL", 3)
    for y in (top + 34 * s, base - 30 * s):
        cv.rect(x0, y, aw, 5 * s, R("STEEL", 3))
        cv.rect(x0, y, aw, 1, R("STEEL", 4))
        for x in range(x0 + 3 * s, x0 + aw - 2, 7 * s):
            cv.rect(x, y + 2 * s, s, s, R("STEEL", 6))
    # the grille at head height, shutter shut
    gw, gh = 22 * s, 16 * s
    gy = top + 44 * s
    cv.rect(gx - gw // 2 - 2 * s, gy - 2 * s, gw + 4 * s, gh + 4 * s, R("STEEL", 1))
    cv.rect(gx - gw // 2, gy, gw, gh, R("STEEL", 3))
    for y in range(gy + 2 * s, gy + gh, 3 * s):
        cv.rect(gx - gw // 2, y, gw, s, R("STEEL", 2))
    for x in range(gx - gw // 2 + 3 * s, gx + gw // 2, 5 * s):
        cv.rect(x, gy - 1 * s, 2 * s, gh + 2 * s, R("STEEL", 1))
        cv.rect(x, gy - 1 * s, s, gh + 2 * s, R("STEEL", 4))
    cv.a[cv.outline_mask(dm) & ~am] = R("STEEL", 0)
    cv.rect(x0, base - 1, aw, 1, R("STEEL", 0))
    return dict(grille=(gx, gy + gh // 2), padlocks=(gx, top + 70 * s), top=top, x0=x0, w=aw)


def _fair_edge(cv, seed):
    """Left of the bandstand: Miss Lulla's craft tent and Fuzz's FREE HUGS booth."""
    kit.tent(cv, 56, GROUND, 92, 96, [P("LAVENDER", 2), pal.CREAM], flag=P("LAVENDER", 1))
    box = kit.booth(cv, 112, GROUND, 52, 66, [P("PEACH", 1), pal.CREAM], seed=seed + 2,
                    back=R("PEACH", 3))
    return box


@generator()
def bandstand_tiles(seed=124, torn=0):
    """The pixel bandstand (TILEMAP, 800x240): fair edge, north half, south half, Wall, gate."""
    cv = Canvas(W, 240)
    _ground(cv, 0, W, seed)
    # the Wall and the gate first (the bandstand's south end overlaps the pilaster)
    _wall(cv, WALL_X, W, seed + 1)
    gate_art(cv, GATE_X, seed + 2)
    # fair edge on the left
    _fair_edge(cv, seed + 3)
    # roof band (north half painted, south half faded)
    _roof(cv, DECK_X0 - 18, DECK_X1 + 10, faded_from=SPLIT, seed=seed + 4)
    # back of the pavilion: open (transparent) between the posts in the north half;
    # boarded up in the south half
    _boards(cv, SPLIT + 4, DECK_X1 - 2, EAVE + 10, DECK - 2, seed + 5)
    _deck(cv, seed + 6)
    _railing(cv, DECK_X0 + 4, SPLIT, gaps=[(STEPS[0] - 2, STEPS[1] + 2)])
    for k, x in enumerate(POSTS):
        _post(cv, x, faded=x > SPLIT, seed=seed + 10 + k)
    _steps(cv, seed + 7)
    # the dividing line: a post at the split, half painted, half bare
    _post(cv, SPLIT, faded=True, seed=seed + 20)
    for y in range(EAVE + 6, DECK):
        for dx in range(-3, 0):
            s_ = ((y + (dx + 3) * 2) // 4) % 3
            cv.a[y, SPLIT + dx] = pal.WHITE if s_ else R("PASTEL_PINK", 5)
    # lantern strings: tent pole -> eave, and along the north eave (lanterns are props)
    spots = []
    for (x0, y0, x1, y1, sag, every) in ((56, GROUND - 104, DECK_X0 - 16, EAVE + 8, 14, 18),
                                          (DECK_X0 - 16, EAVE + 8, SPLIT, EAVE + 8, 8, 20)):
        pts = kit.catenary(x0, y0, x1, y1, sag)
        kit.draw_string(cv, pts, R("WOOD", 2))
        for i in range(every // 2, len(pts) - 4, every):
            spots.append((int(pts[i][0]), int(pts[i][1])))
    cv.meta["lanterns"] = [(x - (W - 320) // 2, y - 30) for x, y in spots]
    cv.meta["notes"] = "deck top show y 124; ground show y 150; split x %d; gate x %d (plate)" % (SPLIT, GATE_X)
    return to_tilemap(cv)


def as_tilemap(cv):
    """Sparse planes (light pools, haze) pack best as tilemaps: empty tiles dedupe to one."""
    w16 = (cv.w + 15) // 16 * 16
    h16 = (cv.h + 15) // 16 * 16
    if (w16, h16) != (cv.w, cv.h):
        c2 = Canvas(w16, h16)
        c2.blit(cv, 0, 0, key=None)
        c2.meta = cv.meta
        cv = c2
    return to_tilemap(cv)


@generator()
def bandstand_far(seed=125, w=560):
    """The far plane (par 0.5): the fair beyond (tent tops, a ferris wheel), trees,
    the town hill and its clock tower; the Hellvile glow over the Wall on the right."""
    cv = Canvas(w, 240)
    base = Y(118)
    kit.hills(cv, base - 16, 14, 260, R("GRASS", 4), seed, top=R("GRASS", 5), bottom=240)
    # town hill + clock tower (left)
    m, crest = kit.hills(cv, base - 34, 20, 400, R("GRASS", 3), seed + 1, top=R("GRASS", 4), lumps=2, bottom=base - 4)
    tx = 70
    cv.rect(tx - 6, base - 82, 12, 52, R("PEACH", 6))
    cv.rect(tx - 6, base - 82, 2, 52, R("PEACH", 7))
    cv.poly([(tx - 9, base - 82), (tx + 9, base - 82), (tx, base - 98)], P("BUBBLEGUM", 1))
    cv.ellipse(tx, base - 70, 4.5, 4.5, pal.CREAM)
    cv.px(tx, base - 70, R("STONE", 1))
    cv.line(tx, base - 70, tx, base - 73, R("STONE", 1))
    cv.line(tx, base - 70, tx + 2, base - 70, R("STONE", 1))
    # ferris wheel
    fx, fy, fr = 190, base - 66, 34
    for k in range(12):
        a = k / 12 * 6.283
        cv.line(fx, fy, fx + math.cos(a) * fr, fy + math.sin(a) * fr, R("STONE", 6))
    for k in range(24 * 4):
        a = k / 96 * 6.283
        cv.px(fx + math.cos(a) * fr, fy + math.sin(a) * fr, R("PASTEL_PINK", 6))
    for k in range(8):
        a = k / 8 * 6.283 + 0.2
        x, y = fx + math.cos(a) * fr, fy + math.sin(a) * fr
        cv.rect(x - 2, y, 5, 4, [P("BUBBLEGUM", 2), P("LEMON", 2), P("MINT", 2), P("LAVENDER", 2)][k % 4])
    cv.line(fx, fy, fx - 16, base, R("STONE", 5))
    cv.line(fx, fy, fx + 16, base, R("STONE", 5))
    # distant tent tops
    for k, (x, c) in enumerate(((280, P("MINT", 2)), (330, P("LEMON", 2)), (120, P("BUBBLEGUM", 2)))):
        kit.tent(cv, x, base + 4, 44, 40, [c, pal.CREAM], door=False, flag=c)
    # trees
    for k in range(10):
        x = 10 + k * 56 + hash2(k, 2, seed) % 20
        kit.tree_round(cv, x, base + 2, 10 + hash2(k, 3, seed) % 5, leaf=R("GRASS", 4), seed=seed + k)
    # the Wall's far run on the right (lower, hazy) - Hellvile is beyond it
    cv.rect(430, base - 72, w - 430, 76, R("STONE", 4))
    kit.bricks(cv, 430, base - 72, w - 430, 76, seed + 9, base=R("STONE", 4), mortar=R("STONE", 3), bw=14, bh=7,
               chips=False)
    cv.rect(430, base - 74, w - 430, 2, R("STONE", 5))
    return cv


@generator()
def hellvile_glow(seed=126, w=560):
    """Above the Wall on the far right: the purple haze of the other side (fixed light,
    so it stays the same colour at every time of day). Dithered, palette-exact."""
    cv = Canvas(w, 240)
    b = bayer(w, 240)
    for y in range(0, Y(50)):
        for x in range(430, w):
            t = (x - 430) / (w - 430)
            v = (1 - y / Y(50)) * 0.6 + t * 0.4
            lvl = int(v * 9)
            if b[y, x] < lvl:
                cv.a[y, x] = R("VAPE", 2) if b[y, x] < lvl // 2 else R("VAPE", 1)
    return cv


@generator()
def bandstand_glow(seed=127, w=W):
    """Lantern light pools on the deck and the lawn (fixed light: shown at dusk and night)."""
    cv = Canvas(w, 240)
    for x in range(DECK_X0 + 20, SPLIT, 40):
        kit.glow_pool(cv, x, DECK + 2, 22, 4, R("CANDY", 6), levels=(8, 4))
    for x in range(DECK_X0 + 10, SPLIT + 10, 56):
        kit.glow_pool(cv, x, GROUND + 18, 30, 8, R("CANDY", 4), levels=(4, 2))
    kit.glow_pool(cv, 56, GROUND + 6, 40, 8, R("CANDY", 4), levels=(4, 2))
    kit.glow_pool(cv, 138, GROUND + 8, 30, 8, R("CANDY", 4), levels=(4, 2))
    return as_tilemap(cv)


# ---------------------------------------------------------------------------
# bandstand props
# ---------------------------------------------------------------------------
@generator()
def lantern(color="pink", state="lit", size=1):
    cols = {"pink": P("BUBBLEGUM", 1), "lemon": P("LEMON", 1), "mint": P("MINT", 1),
            "lavender": P("LAVENDER", 1), "peach": P("PEACH", 1)}
    return kit.lantern_img(cols.get(color, P("BUBBLEGUM", 1)), state, int(size))


@generator()
def skirt_board(w=24, h=26, seed=1):
    """One of the boards nailed over the hollow under the south half (2-03 tears them off)."""
    cv = Canvas(int(w), int(h))
    rng = Rng(seed)
    c = rng.choice([R("WOOD", 3), R("STONE", 4), R("WOOD", 2)])
    cv.rect(0, 0, int(w), int(h), c)
    cv.rect(0, 0, int(w), 1, L(c, 1))
    cv.rect(0, 0, 1, int(h), L(c, 1))
    cv.rect(int(w) - 1, 0, 1, int(h), D(c, 2))
    for y in range(3, int(h) - 2, 5):
        cv.rect(2, y, int(w) - 4, 1, D(c, 1))
    for (nx, ny) in ((2, 2), (int(w) - 3, 2), (2, int(h) - 3), (int(w) - 3, int(h) - 3)):
        cv.px(nx, ny, R("STONE", 6))
    cv.meta["anchor"] = "topleft"
    return cv


@generator()
def mingle_crate(seed=2, w=52, h=20):
    """The crate under the boards: stencilled MINGLE, DO NOT OPEN (the stencil is a string;
    the crate carries a blank stencil panel)."""
    w, h = int(w), int(h)
    cv = Canvas(w, h)
    kit.planks(cv, 0, 0, w, h, seed, base=R("WOOD", 4), vertical=False, pw=5)
    cv.frame(0, 0, w, h, R("WOOD", 1))
    cv.rect(0, 0, 4, h, R("WOOD", 3))
    cv.rect(w - 4, 0, 4, h, R("WOOD", 3))
    cv.rect(7, 4, w - 14, h - 8, R("WOOD", 5))
    cv.frame(7, 4, w - 14, h - 8, R("WOOD", 3))
    cv.meta["text"] = (8, 5, w - 16, h - 10)
    return cv


@generator()
def sky_stars(seed=3, w=128, h=80, n=14):
    """Stars for a night sky: a small GRAIN-style tile (tile= plane) repeated over the BANDS
    sky; the planes in front cover the repeats below the horizon. Emissive colours."""
    cv = Canvas(int(w), int(h))
    kit.stars(cv, int(n), int(seed), 0, int(h) - 1, cols=[pal.WHITE, R("CANDY", 7), pal.WHITE, R("SKY", 7)])
    return cv


@generator()
def padlock(seed=1, state="shut"):
    """One of the seven padlocks (9x12); 'jump' is the 2-f hop after the BOING."""
    cv = Canvas(9, 13)
    cv.rect(2, 0, 5, 1, R("STEEL", 5))
    cv.rect(1, 1, 1, 4, R("STEEL", 5))
    cv.rect(7, 1, 1, 4, R("STEEL", 4))
    cv.rect(0, 5, 9, 7, P("HONEY", 1))
    cv.rect(0, 5, 9, 1, P("HONEY", 2))
    cv.rect(8, 6, 1, 6, P("HONEY", 0))
    cv.rect(0, 11, 9, 1, P("HONEY", 0))
    cv.px(4, 7, R("STONE", 0))
    cv.px(4, 8, R("STONE", 0))
    cv.px(4, 9, R("STONE", 1))
    return cv


@generator()
def keep_out_sign(seed=3):
    """The sign on the gate (KEEP OUT. FOREVER. is a string): a riveted metal plate."""
    cv = Canvas(58, 18)
    cv.rect(0, 0, 58, 18, P("CHERRY", 0))
    cv.rect(1, 1, 56, 16, P("CHERRY", 1))
    cv.rect(1, 1, 56, 1, P("CHERRY", 2))
    for (x, y) in ((2, 2), (55, 2), (2, 15), (55, 15)):
        cv.px(x, y, R("STEEL", 6))
    cv.meta["text"] = (3, 3, 52, 12)
    return cv


# ---------------------------------------------------------------------------
# CO-5: the banner MS (co12-co18), matched to the crayon banner of co10/co11
# ---------------------------------------------------------------------------
@generator()
def bandstand_ms(seed=128):
    """The north half close: the roof band and valance at the top, two candy posts at the
    frame edges, the deck rail along the bottom; the bays are open (the far plane shows).
    The banner (size=ms) hangs at show y 50 so its letters sit on the crayon banner's
    baseline (show y 104) and x positions."""
    cv = Canvas(352, 240)
    top_valance = Y(4)
    for y in range(0, top_valance):
        row = y // 5
        for x in range(352):
            c = R("PASTEL_PINK", 4)
            yy = y % 5
            if yy == 4:
                c = R("PASTEL_PINK", 3)
            elif yy == 0 and (x + (row % 2) * 5) % 10 in (3, 4, 5, 6):
                c = R("PASTEL_PINK", 5)
            cv.a[y, x] = c
    cv.rect(0, top_valance - 3, 352, 3, R("PASTEL_PINK", 2))
    for x in range(0, 352, 14):
        c = pal.CREAM if (x // 14) % 2 == 0 else R("PASTEL_PINK", 5)
        cv.rect(x, top_valance, 14, 7, c)
        cv.ellipse(x + 7, top_valance + 7, 7, 4.5, c)
        cv.px(x + 7, top_valance + 11, D(c, 2))
    # posts at the frame edges (show x ~2 and ~318)
    for px_ in (18, 334):
        for y in range(top_valance + 10, Y(176)):
            for dx in range(12):
                s_ = ((y + dx * 2) // 6) % 3
                c = pal.WHITE if s_ else R("PASTEL_PINK", 5)
                if dx >= 10:
                    c = D(c, 1)
                cv.a[y, px_ - 6 + dx] = c
        cv.frame(px_ - 7, top_valance + 10, 14, Y(176) - top_valance - 10, R("PASTEL_PINK", 2))
    # rail and deck at the bottom
    ry = Y(150)
    cv.rect(0, ry, 352, 5, pal.WHITE)
    cv.rect(0, ry + 5, 352, 1, R("STONE", 5))
    for x in range(4, 352, 10):
        cv.rect(x, ry + 6, 5, 22, pal.WHITE)
        cv.rect(x + 4, ry + 6, 1, 22, R("STONE", 6))
    kit.planks(cv, 0, ry + 28, 352, 240 - ry - 28, seed, base=R("WOOD", 5), pw=6)
    return cv


@generator()
def bandstand_cu(side="north", seed=129):
    """Close-up backings at px=2 (160x120) for bust shots at the bandstand: north (a candy
    post, lanterns, the edge of the banner), boards (the boarded south half), steps (the
    step treads and the rail), wall (the Wall's blocks beside the gate)."""
    cv = Canvas(160, 120)
    if side == "north":
        # out-of-focus lanterns over the far fair (big soft dots)
        for k in range(7):
            x, y = 12 + k * 22 + (k % 2) * 6, 18 + (k % 3) * 8
            c = kit.LANTERN_COLS[k % 5]
            cv.ellipse(x, y, 6, 7, c)
            cv.ellipse(x, y + 1, 3, 3.5, R("CANDY", 7))
        cv.rect(0, 0, 160, 6, R("PASTEL_PINK", 4))
        for x in range(0, 160, 10):
            cv.ellipse(x + 5, 6, 5, 3, pal.CREAM if (x // 10) % 2 == 0 else R("PASTEL_PINK", 5))
        for y in range(6, 120):
            for dx in range(14):
                s_ = ((y + dx * 2) // 6) % 3
                cv.a[y, 126 + dx] = pal.WHITE if s_ else R("PASTEL_PINK", 5)
        cv.frame(125, 6, 16, 114, R("PASTEL_PINK", 2))
        cv.rect(0, 92, 160, 4, pal.WHITE)
        for x in range(2, 124, 8):
            cv.rect(x, 96, 4, 24, pal.WHITE)
    elif side == "boards":
        _boards(cv, 0, 160, 0, 120, seed, shade=0)
        cv.rect(70, 0, 10, 120, R("STONE", 4))
        cv.rect(78, 0, 2, 120, R("STONE", 2))
    elif side == "steps":
        kit.planks(cv, 0, 0, 160, 60, seed, base=R("WOOD", 2), pw=8)
        for k in range(3):
            y = 60 + k * 20
            kit.planks(cv, 0, y, 160, 20, seed + k, base=R("WOOD", 5 - (k % 2)), pw=5)
            cv.rect(0, y, 160, 1, R("WOOD", 6))
            cv.rect(0, y + 19, 160, 1, R("WOOD", 1))
        cv.rect(0, 40, 160, 4, pal.WHITE)
        for x in range(4, 160, 10):
            cv.rect(x, 44, 4, 16, pal.WHITE)
    else:
        kit.bricks(cv, 0, 0, 160, 120, seed, base=R("STONE", 3), mortar=R("STONE", 1), bw=24, bh=12)
        cv.rect(120, 0, 40, 120, R("STEEL", 2))
        for y in range(4, 120, 9):
            cv.px(124, y, R("STEEL", 6))
        cv.rect(120, 0, 2, 120, R("STEEL", 0))
    return cv


# ---------------------------------------------------------------------------
# MT-4, 4-03, PC-1: the sealed gate in the Wall (figure scale, the Wall's top in frame)
# ---------------------------------------------------------------------------
@generator()
def gate_wall(seed=130):
    """The Wall face with the sealed gate centred (show x 160), the Wall's coping at show
    y 22 so the sky shows above it (the lost-property lantern drifts over in PC-1); the
    boarded corner of the bandstand peeks in at the left edge. The door is a prop."""
    cv = Canvas(352, 240)
    wtop = Y(22)
    kit.grass(cv, 0, GROUND, 352, 240 - GROUND, seed, ramp_lo=3, ramp_hi=5, period=64)
    kit.bricks(cv, 0, wtop, 352, GROUND - wtop, seed, base=R("STONE", 3), mortar=R("STONE", 1), bw=20, bh=10,
               period=4)
    b = bayer(352, 240)
    for y in range(GROUND - 40, GROUND):
        lvl = int((y - (GROUND - 40)) / 40 * 9)
        row = cv.a[y]
        m = b[y] < lvl
        row[m] = pal.DARKER[row[m]]
    cv.rect(0, wtop - 6, 352, 6, R("STONE", 5))
    cv.rect(0, wtop - 6, 352, 1, R("STONE", 6))
    cv.rect(0, wtop - 1, 352, 1, R("STONE", 1))
    for x in range(0, 352, 18):
        cv.rect(x + 2, wtop - 10, 10, 4, R("STONE", 4))
        cv.rect(x + 2, wtop - 10, 10, 1, R("STONE", 5))
    info = gate_art(cv, 176, seed, base=GROUND, scale=1)
    # cut the door out: the door is a prop (it bulges in PC-1); leave the dark doorway
    x0, w, top = info["x0"], info["w"], info["top"]
    dm = cv.poly_mask([(x0, GROUND), (x0, top + 26), (176, top), (x0 + w, top + 26), (x0 + w, GROUND)])
    cv.a[dm] = R("STONE", 0)
    # the bandstand's boarded corner at the far left
    _boards(cv, 0, 26, Y(10), DECK - 2, seed + 3)
    cv.rect(0, DECK - 2, 30, GROUND - DECK + 2, R("STONE", 1))
    cv.rect(26, Y(4), 6, DECK - Y(4), R("STONE", 4))
    for x in range(0, 352, 3):
        hh = 2 + hash2(x, 7, seed) % 5
        cv.vline(x, GROUND - hh, GROUND - 1, R("GRASS", 3 + (x % 2)))
    cv.meta["door"] = (x0 - 88, top - 30)
    return cv


@generator()
def gate_door(state="shut", seed=131):
    """The gate's iron door (prop, 70x116): shut, or bulge (PC-1: pushed outward once, BOING;
    drawn 6 px wider at its middle). The grille's shutter stays shut in 101."""
    cv0 = Canvas(90, 132)
    info = gate_art(cv0, 45, seed, base=126, scale=1)
    # keep only the door (inside the arch)
    x0, w, top = info["x0"], info["w"], info["top"]
    dm = cv0.poly_mask([(x0, 126), (x0, top + 26), (45, top), (x0 + w, top + 26), (x0 + w, 126)])
    cv = Canvas(w + 8, 126 - top + 2)
    sub = np.where(dm, cv0.a, 0)[top:127, x0 - 4:x0 + w + 4]
    cv.a[:sub.shape[0], :sub.shape[1]] = sub
    if state == "bulge":
        h, ww = cv.a.shape
        out = np.zeros_like(cv.a)
        for y in range(h):
            t = math.sin(math.pi * y / max(1, h - 1))
            k = 1 + 0.09 * t
            xs = ((np.arange(ww) - ww / 2) / k + ww / 2).astype(int)
            ok = (xs >= 0) & (xs < ww)
            out[y, ok] = cv.a[y, xs[ok]]
        cv.a = out
    cv.meta["anchor"] = "bottom"
    return cv
