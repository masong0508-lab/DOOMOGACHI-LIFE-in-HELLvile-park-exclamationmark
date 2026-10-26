"""props2.py - the props and prop flipbooks the shot lists ask ART for (owner: ART).

Every item here closes an ART line of a TODO at the top of
show/ep101/shotlists/{co_mt,a1,a2,a3,a4}.md (SETS.md sec.7 maps them). Pixel
register, family-dark outlines, master-palette indices; flipbooks carry
meta['frames'] / meta['holds'] like amb.py's.

Demons only take the gore (the truth frames), cartoon-gruesome, in the
game's own red ramps; nothing here is ever drawn near a child.
"""
import math

import numpy as np

from . import generator, pal, kit
from .canvas import Canvas, Rng, bayer
from .amb import flipbook

R, P, D = pal.ramp, pal.paint, kit.D
WHITE, BLACK = pal.WHITE, pal.BLACK


def _fb(frames, holds, loop=1, mode="replace"):
    fb = flipbook(frames, holds)
    fb.meta.update(loop=int(loop), mode=mode, anchor="bottom")
    return fb


# ---------------------------------------------------------------------------
# demons (the game's Puff Demons, the show's own drawing for the 2D sets)
# ---------------------------------------------------------------------------
def _puff(s=1, squash=1.0, look="front", face="happy", tint="VAPE_PURPLE"):
    w, h = int(28 * s), int(30 * s)
    cv = Canvas(w + 4, h + 6)
    cx, base = (w + 4) / 2.0, h + 4
    ry = 10 * s * squash
    rx = 12 * s / max(0.6, squash) ** 0.5
    body = np.zeros((cv.h, cv.w), bool)
    for dx, dy, r in ((-6, 2, 7), (6, 2, 7), (0, -3, 9), (-3, -8, 6), (4, -7, 6)):
        body |= cv.ellipse_mask(cx + dx * s * rx / (12 * s), base - ry - 2 * s + dy * s * squash, r * s * rx / (12 * s),
                                r * s * squash)
    cv.shape(body, P(tint, 1), outline=P("GRAPE", 0), hi=P(tint, 2))
    top = int(np.nonzero(body.any(axis=1))[0].min())
    for side in (-1, 1):                                      # two nub horns
        hx = cx + side * 5 * s
        cv.poly([(hx - 2 * s, top + 3 * s), (hx + 2 * s, top + 3 * s), (hx + side * 1 * s, top - 3 * s)], P("CHERRY", 1))
    ey = base - ry - 3 * s
    for side in (-1, 1):
        ex = cx + side * 4 * s
        if face == "x":
            cv.line(ex - s, ey - s, ex + s, ey + s, BLACK)
            cv.line(ex - s, ey + s, ex + s, ey - s, BLACK)
        else:
            cv.ellipse(ex, ey, 1.6 * s, 2.2 * s, BLACK)
            cv.px(ex - 0.5 * s, ey - 1 * s, WHITE)
    if face == "happy":
        cv.line(cx - 2 * s, ey + 4 * s, cx, ey + 5 * s, BLACK)
        cv.line(cx, ey + 5 * s, cx + 2 * s, ey + 4 * s, BLACK)
    elif face in ("shock", "x"):
        cv.ellipse(cx, ey + 5 * s, 1.5 * s, 1.5 * s, BLACK)
    cv.ellipse(cx - 7 * s, ey + 3 * s, 1.5 * s, 1 * s, P("ROSE", 2))
    cv.ellipse(cx + 7 * s, ey + 3 * s, 1.5 * s, 1 * s, P("ROSE", 2))
    return cv


@generator(kind="flipbook")
def puff_demon(state="idle", size=1):
    """state=idle (bob + squash, 4 x 6 f) | flinch | hearts (CUTE burst) | mist (BRUTAL burst:
    red mist and gibs, demons only) | float (drift bob)"""
    s = int(size)
    if state in ("idle", "float"):
        return _fb([_puff(s, q) for q in (1.0, 0.94, 1.0, 1.05)], 6)
    if state == "flinch":
        return _fb([_puff(s, 0.8, face="shock"), _puff(s, 1.1, face="shock"), _puff(s, 0.96), _puff(s)], [2, 2, 3, 6], 0)
    frames = [_puff(s, 0.85, face="shock"), _puff(s, 1.2, face="x")]
    rng = Rng(7 * s)
    w, h = frames[0].w + 24 * s, frames[0].h + 20 * s
    for k in range(4):
        cv = Canvas(w, h)
        cx, cy = w / 2, h - 16 * s
        r = (6 + k * 6) * s
        for j in range(14):
            a = 2 * math.pi * j / 14 + rng.uniform(-0.2, 0.2)
            x, y = cx + r * math.cos(a), cy + r * 0.8 * math.sin(a) + k * 2 * s
            if state == "hearts":
                kit.heart(cv, x, y, (2 - k * 0.3) * s + 1, P("BUBBLEGUM", 2) if j % 2 else P("ROSE", 2))
            else:
                c = R("BLOOD", 5 - min(k, 3)) if j % 3 else P("VAPE_PURPLE", 0)
                cv.ellipse(x, y, (2.2 - k * 0.4) * s + 0.5, (1.6 - k * 0.3) * s + 0.5, c)
                if j % 4 == 0 and k < 3:
                    cv.px(x, y + 2 * s + k * 2, R("BLOOD", 3))
        if state == "mist":
            b = bayer(w, h)
            m = cv.ellipse_mask(cx, cy, r * 1.1, r * 0.8) & (b < max(1, 8 - k * 2)) & (cv.a == 0)
            cv.a[m] = R("BLOOD", 6)
        frames.append(cv)
    return _fb(frames, [2, 2, 3, 3, 4, 6], 0)


@generator()
def big_puff(state="idle"):
    """the Big Puff (3-02, 3-05): the demons stacked into one, 3x."""
    return puff_demon(state=state, size=3)


@generator(kind="flipbook")
def puff_breath(seed=1):
    """4-02: the breath puff slips out between two ribs and drifts, squeezing (6 x 4 f)"""
    out = []
    for k in range(6):
        sq = 1.0 + 0.25 * math.sin(2 * math.pi * k / 6)
        cv = Canvas(30, 22)
        m = cv.ellipse_mask(15, 12, 9 / sq, 6 * sq) | cv.ellipse_mask(9, 13, 5 / sq, 4 * sq) | cv.ellipse_mask(21, 13, 5 / sq, 4 * sq)
        cv.shape(m, P("VAPE_PURPLE", 2), outline=P("GRAPE", 1), hi=P("LAVENDER", 2))
        b = bayer(30, 22)
        cv.a[cv.ellipse_mask(15 - 8 - k, 13, 6, 3) & (b < 5) & (cv.a == 0)] = P("LAVENDER", 2)
        out.append(cv)
    return _fb(out, 4)


# ---------------------------------------------------------------------------
# the fair's spooks
# ---------------------------------------------------------------------------
@generator()
def cardboard_ghost(face="front", seed=1):
    """2-03 / a2_17: a cardboard ghost on a stick; face=wall is it turned edge-on toward the Wall"""
    cv = Canvas(26, 40)
    if face == "wall":
        cv.rect(11, 2, 4, 26, R("WOOD", 6))
        cv.vline(11, 2, 27, R("WOOD", 3))
        cv.rect(12, 28, 2, 12, R("WOOD", 2))
        cv.px(14, 10, BLACK)
        return cv
    m = cv.ellipse_mask(13, 12, 11, 10) | cv.poly_mask([(2, 12), (24, 12), (24, 28), (2, 28)])
    xs, ys = cv.grid()
    m &= ~((ys > 24) & (((xs // 4) % 2) == 0) & (ys > 25))
    cv.shape(m, WHITE, outline=R("STONE", 3), lo=R("STONE", 6))
    cv.ellipse(9, 12, 1.5, 2.5, BLACK)
    cv.ellipse(17, 12, 1.5, 2.5, BLACK)
    cv.ellipse(13, 18, 2, 2.5, BLACK)
    cv.rect(12, 28, 2, 12, R("WOOD", 2))
    return cv


@generator(kind="flipbook")
def cardboard_ghost_turn(seed=1):
    """the turn to the Wall: front, 3/4 (squeezed), edge-on (fwip on each)"""
    f = cardboard_ghost("front", seed)
    mid = Canvas(f.w, f.h)
    sub = f.a[:, ::2]
    mid.blit(sub, (f.w - sub.shape[1]) // 2, 0)
    return _fb([f, mid, cardboard_ghost("wall", seed)], [2, 2, 8], 0)


@generator(kind="flipbook")
def bedsheet_ghost(seed=1):
    """the bedsheet ghosts of the Not Scary House: the hem ripples (4 x 5 f)"""
    out = []
    for k in range(4):
        cv = Canvas(30, 40)
        m = cv.ellipse_mask(15, 13, 11, 11)
        xs, ys = cv.grid()
        hem = 34 + np.round(2 * np.sin(xs / 3.0 + k * math.pi / 2))
        m |= (xs >= 4) & (xs <= 26) & (ys >= 13) & (ys <= hem)
        cv.shape(m, WHITE, outline=R("STONE", 3), lo=R("STONE", 6))
        cv.ellipse(11, 14, 2, 2.5, BLACK)
        cv.ellipse(19, 14, 2, 2.5, BLACK)
        out.append(cv)
    return _fb(out, 5)


# ---------------------------------------------------------------------------
# set dressing
# ---------------------------------------------------------------------------
@generator(kind="flipbook")
def door(kind="plain", seed=1):
    """a cottage-style door; kind=knock: the knock wobble (shear 1 px, 3 knocks)"""
    def one(sh):
        cv = Canvas(34, 56)
        kit.planks(cv, 2, 2, 30, 52, seed, base=R("WOOD", 4), vertical=True, pw=6)
        cv.frame(2, 2, 30, 52, R("WOOD", 1))
        cv.ellipse(26, 30, 1.5, 1.5, pal.GOLD)
        if sh:
            a = cv.a.copy()
            for y in range(cv.h):
                d = sh if y < cv.h // 2 else 0
                cv.a[y] = np.roll(a[y], d)
        return cv
    if kind == "knock":
        return _fb([one(0), one(1), one(0), one(1), one(0), one(1), one(0)], [3, 2, 6, 2, 6, 2, 8], 0)
    return _fb([one(0)], [1])


@generator(kind="flipbook")
def knock_wobble(seed=1):
    return door(kind="knock", seed=seed)


@generator()
def plinth(seed=1):
    cv = Canvas(30, 34)
    cv.shape(cv.poly_mask([(3, 4), (27, 4), (25, 33), (5, 33)]), R("STONE", 6), outline=R("STONE", 2), hi=R("STONE", 7))
    cv.rect(1, 1, 28, 4, R("STONE", 5))
    cv.frame(1, 1, 28, 4, R("STONE", 2))
    return cv


@generator()
def ammo_box(seed=1):
    cv = Canvas(24, 16)
    cv.rect(0, 3, 24, 13, P("FOREST", 1))
    cv.frame(0, 3, 24, 13, P("FOREST", 0))
    cv.rect(0, 0, 24, 4, P("FOREST", 2))
    cv.rect(9, 7, 6, 4, P("LEMON", 1))
    return cv


@generator()
def recipe_tin(state="shut", seed=1):
    """Nana's recipe tin (insert); state=open shows the cards"""
    cv = Canvas(64, 44)
    cv.shape(cv.poly_mask([(2, 14), (62, 14), (60, 43), (4, 43)]), P("ROSE", 1), outline=P("ROSE", 0), hi=P("ROSE", 2))
    for x in range(6, 60, 8):
        cv.ellipse(x, 28, 1.5, 1.5, WHITE)
    if state == "open":
        for k in range(4):
            cv.rect(8 + k * 12, 2 + k, 18, 14, pal.CREAM)
            cv.frame(8 + k * 12, 2 + k, 18, 14, R("PEACH", 3))
            cv.hline(10 + k * 12, 22 + k * 12, 7 + k, R("STONE", 4))
    else:
        cv.rect(1, 8, 62, 7, P("ROSE", 2))
        cv.frame(1, 8, 62, 7, P("ROSE", 0))
    cv.meta["text"] = (10, 30, 44, 8)
    return cv


@generator()
def keepsakes(seed=1):
    """the tin's keepsakes: a dried flower, a ribbon, a photo, a tiny shoe"""
    cv = Canvas(80, 30)
    cv.rect(2, 4, 22, 18, pal.CREAM); cv.frame(2, 4, 22, 18, R("STONE", 4)); cv.rect(5, 7, 16, 11, R("SKY", 6))
    cv.line(30, 24, 44, 8, P("FOREST", 1)); kit.heart(cv, 44, 8, 3, P("ROSE", 2))
    cv.line(50, 26, 62, 14, P("BUBBLEGUM", 2)); cv.line(62, 14, 70, 24, P("BUBBLEGUM", 2))
    cv.ellipse(72, 24, 5, 3, P("PEACH", 2)); cv.frame(68, 22, 8, 4, P("PEACH", 0))
    return cv


@generator()
def signup_sheet(rows=6, seed=1):
    """Fuzz's sign-up sheet (insert): ruled lines; names are strings in the text box"""
    cv = Canvas(120, 150)
    cv.rect(0, 0, 120, 150, WHITE)
    cv.frame(0, 0, 120, 150, R("STONE", 4))
    cv.rect(0, 0, 120, 18, P("LAVENDER", 2))
    for k in range(int(rows)):
        cv.hline(10, 110, 34 + k * 18, R("SKY", 5))
    cv.meta["text"] = (8, 4, 104, 12)
    return cv


@generator()
def free_hugs_booth(seed=1):
    """1-14: Fuzz's FREE HUGS booth (the words are a string on the board)"""
    cv = Canvas(84, 92)
    kit.booth(cv, 42, 91, 76, 60, [P("LAVENDER", 2), WHITE], counter=P("CARAMEL", 1), seed=seed)
    cv.rect(10, 4, 64, 16, pal.CREAM)
    cv.frame(10, 4, 64, 16, P("GRAPE", 0))
    kit.heart(cv, 16, 12, 3, P("ROSE", 2))
    kit.heart(cv, 68, 12, 3, P("ROSE", 2))
    cv.meta["text"] = (20, 6, 44, 12)
    return cv


@generator()
def spotlight(w=70, h=150, color="cream"):
    """a dithered cone of stage light (light=fixed plane/prop; Hattie's mic)"""
    w, h = int(w), int(h)
    cv = Canvas(w, h)
    c = {"cream": R("CANDY", 7), "white": WHITE, "pink": R("PASTEL_PINK", 7)}.get(color, R("CANDY", 7))
    b = bayer(w, h)
    xs, ys = cv.grid()
    half = 4 + (w / 2 - 4) * ys / h
    m = np.abs(xs - w / 2) < half
    lvl = 3 + (ys * 3 // h)
    cv.a[m & (b < lvl)] = c
    cv.a[cv.ellipse_mask(w / 2, h - 5, w / 2 - 2, 5) & (b < 9)] = c
    return cv


@generator()
def teacup(color="BUBBLEGUM", red=0):
    """a fair teacup ride car (2-03); red=1 the red cup state of a2_16"""
    col = P("CHERRY", 1) if int(red) else P(str(color).upper(), 1)
    cv = Canvas(46, 30)
    m = cv.ellipse_mask(22, 6, 20, 5) | cv.poly_mask([(2, 6), (42, 6), (36, 26), (8, 26)])
    cv.shape(m, col, outline=D(col, 2), hi=kit.L(col, 1))
    cv.ellipse(22, 6, 17, 3, D(col, 2))
    hm = cv.ellipse_mask(42, 14, 4, 5) & ~cv.ellipse_mask(42, 14, 2, 3)
    cv.a[hm] = D(col, 1)
    cv.rect(10, 26, 26, 3, R("STONE", 5))
    return cv


@generator()
def hay_bale(seed=1):
    cv = Canvas(34, 20)
    cv.shape(cv.poly_mask([(1, 3), (33, 3), (33, 19), (1, 19)]), P("HONEY", 1), outline=P("CARAMEL", 0), hi=P("LEMON", 1))
    rng = Rng(seed)
    for _ in range(40):
        x, y = rng.randint(2, 31), rng.randint(4, 18)
        cv.px(x, y, P("CARAMEL", 1) if rng.chance(0.5) else P("LEMON", 2))
    for x in (10, 24):
        cv.vline(x, 3, 19, P("CARAMEL", 0))
    return cv


@generator()
def cake_table(seed=1):
    """2-01: the cake table with a gingham cloth and a three-tier cake"""
    cv = Canvas(90, 60)
    cv.rect(2, 30, 86, 20, WHITE)
    xs, ys = cv.grid()
    g = (cv.a == WHITE) & ((((xs // 4) + (ys // 4)) % 2) == 0)
    cv.a[g] = P("BUBBLEGUM", 2)
    cv.frame(2, 30, 86, 20, R("STONE", 4))
    cv.rect(8, 50, 3, 10, R("WOOD", 2)); cv.rect(79, 50, 3, 10, R("WOOD", 2))
    for k, (w, y) in enumerate(((40, 22), (28, 14), (16, 7))):
        cv.rect(45 - w // 2, y, w, 8, pal.CREAM if k % 2 == 0 else P("BUBBLEGUM", 2))
        cv.frame(45 - w // 2, y, w, 8, R("PEACH", 3))
    cv.rect(44, 2, 2, 5, P("LEMON", 2))
    cv.px(44, 1, pal.GOLD)
    return cv


@generator(kind="flipbook")
def underline_wobble(w=60, seed=4):
    """a2_01: the second underline wobbles, overshoots, stops (3 drawings)"""
    out = []
    for k, amp in enumerate((2.0, -1.2, 0.0)):
        cv = Canvas(int(w) + 4, 10)
        pts = [(2 + x, 5 + amp * math.sin(x / 6.0) + (0.5 if x % 9 == 0 else 0)) for x in range(int(w))]
        cv.polyline(pts, P("COBALT", 0))
        out.append(cv)
    return _fb(out, [4, 4, 30], 0)


@generator()
def clipboard_cu(seed=1, ox=60, oy=40):
    """clipboard close-up inserts (a1_09a, a1_10c, a1_12c): the act-card board at x2"""
    from .cards import clipboard
    src = clipboard(seed=seed)
    c = src.crop(int(ox), int(oy), 160, 120).scaled(2)
    return c


@generator()
def muffin_xray(seed=1, key="horned"):
    """a1_09e insert: the suspicious muffin on the light box: a pastel x-ray, a
    tiny horned puff curled inside (a demon, so the joke can land later); key= names
    the shape inside (only "horned" is drawn)"""
    cv = Canvas(320, 240)
    cv.rect(0, 0, 320, 240, P("NAVY", 0))
    xs, ys = cv.grid()
    cv.a[((xs // 16 + ys // 16) % 2 == 0)] = P("NAVY", 1)
    top = cv.ellipse_mask(160, 104, 64, 40)
    cup = cv.poly_mask([(104, 112), (216, 112), (200, 190), (120, 190)])
    cv.a[top | cup] = P("COBALT", 0)
    cv.a[cv.outline_mask(top | cup)] = P("CYAN", 2)
    for x in range(112, 210, 10):
        cv.line(x, 116, x + (160 - x) * 0.12, 186, P("CYAN", 1))
    inner = _puff(2, 0.9)
    m = inner.a != 0
    inner.a[m] = P("CYAN", 2)
    inner.a[m & ~np.roll(m, 1, 0)] = WHITE                     # the rim light on top edges
    cv.blit(inner, 160 - inner.w // 2, 130 - inner.h // 2)
    return cv


@generator()
def mascotty_tiny(h=8):
    """Mascotty 8 px tall (the eyecatch IN cards): the engine's chibi, every
    third pixel (a nearest shrink keeps the palette)"""
    from .motion import render
    a = render("mascotty", "chibi", 0, 0, a=0, b=0)
    k = max(1, a.shape[0] // int(h))
    sub = a[::k, ::k]
    cv = Canvas(sub.shape[1], sub.shape[0])
    cv.a = sub.copy()
    return cv


@generator()
def nana_paw(scale=3):
    """co20: Nana's paw in the window, the engine's own paw pixels at x3"""
    from .motion import render, Parts
    a = render("nana", "figure", 0, 0)
    p = Parts(a)
    ys, xs = np.nonzero(p.rpaw)
    sub = np.where(p.rpaw, a, 0)[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    s = int(scale)
    cv = Canvas(sub.shape[1] * s, sub.shape[0] * s)
    cv.a = np.repeat(np.repeat(sub, s, 0), s, 1).astype(np.uint8)
    return cv


# ---------------------------------------------------------------------------
# the pet
# ---------------------------------------------------------------------------
def _blob(nubs=0, eyes=1):
    cv = Canvas(22, 20)
    m = cv.ellipse_mask(11, 12, 9, 7)
    cv.shape(m, P("BUBBLEGUM", 2), outline=P("ROSE", 0), hi=WHITE)
    if eyes:
        for x in (8, 14):
            cv.ellipse(x, 11, 1.6, 2.2, BLACK)
            cv.px(x - 0.5, 10, WHITE)
    for k in range(int(nubs)):
        for side in (-1, 1):
            hx = 11 + side * 4
            cv.poly([(hx - 1, 6), (hx + 1, 6), (hx + side * 0.5, 6 - k)], P("CHERRY", 1))
    return cv


@generator(kind="flipbook")
def pet_hatch(seed=1):
    """a2_23: the egg on the string: whole, wobble, crack, shell halves fall, two glossy eyes"""
    from .props import egg
    e = egg(state="whole", size=1)
    c = egg(state="cracked", size=1)
    frames = [e, c]
    for k in range(3):
        cv = Canvas(30, 26)
        blob = _blob(0, eyes=1 if k >= 1 else 0)
        cv.blit(blob, 4, 26 - blob.h)
        top = c.crop(0, 0, c.w, 12)
        cv.blit(top, 6 - k * 3, 2 + k * 4)
        bot = c.crop(0, 12, c.w, c.h - 12)
        cv.blit(bot, 6 + k * 3, 14 + k * 3)
        frames.append(cv)
    frames.append(_blob(0, 1))
    return _fb(frames, [8, 6, 3, 3, 4, 20], 0)


@generator(kind="flipbook")
def horn_nubs(seed=1):
    """T-2: by morning, two horn nubs (pet:12 Impkin): they push up in three steps"""
    return _fb([_blob(0), _blob(1), _blob(2), _blob(3)], [10, 6, 6, 20], 0)


# ---------------------------------------------------------------------------
# fireworks and paper
# ---------------------------------------------------------------------------
@generator(kind="flipbook")
def rocket_climb(seed=1):
    """4-08: the rocket climbs thin and quiet: flame flicker + spark trail (4 x 2 f)"""
    out = []
    for k in range(4):
        cv = Canvas(9, 30)
        cv.rect(3, 2, 3, 10, P("BUBBLEGUM", 2))
        cv.poly([(2, 3), (7, 3), (4.5, -1)], P("ROSE", 1))
        cv.vline(4, 12, 20, R("WOOD", 3))
        cv.ellipse(4.5, 14 + (k % 2), 1.5, 2 + (k % 2), pal.GOLD)
        cv.px(4, 14, WHITE)
        for j in range(4):
            cv.px(4 + ((j + k) % 3) - 1, 18 + j * 3 + (k % 2), pal.GOLD if j % 2 else R("CANDY", 7))
        out.append(cv)
    return _fb(out, 2)


def _bloom(color, heart, n=10, size=60):
    out = []
    for k in range(n):
        cv = Canvas(size, size)
        cx = cy = size / 2
        t = (k + 1) / float(n)
        r = (size / 2 - 3) * min(1.0, t * 1.6)
        for j in range(28):
            a = 2 * math.pi * j / 28
            if heart:
                hx = 16 * math.sin(a) ** 3
                hy = -(13 * math.cos(a) - 5 * math.cos(2 * a) - 2 * math.cos(3 * a) - math.cos(4 * a))
                x, y = cx + hx * r / 17.0, cy + hy * r / 17.0 + t * 4
            else:
                x, y = cx + r * math.cos(a), cy + r * math.sin(a) + t * 5
            if k < n - 2 or j % 2:
                cv.px(x, y, color[0] if k < n * 0.6 else color[1])
                if k < 3:
                    cv.px(x, y + 1, WHITE)
        out.append(cv)
    return out


@generator(kind="flipbook")
def heart_bloom(seed=1):
    """4-08: the rocket opens as a PINK HEART (luminance ramps over 8 f via holds)"""
    return _fb(_bloom((P("BUBBLEGUM", 2), P("ROSE", 1)), True), [1, 1, 1, 1, 1, 1, 2, 4, 8, 12], 0)


@generator(kind="flipbook")
def purple_bloom(seed=1):
    """4-08: the answer from the south: a small round purple bloom"""
    return _fb(_bloom((P("VAPE_PURPLE", 2), P("GRAPE", 1)), False, size=36), [1, 1, 1, 1, 2, 2, 3, 4, 8, 12], 0)


def _sheet(w, h, sy=1.0, sx=1.0, fold=0):
    W, H = int(w * sx), max(1, int(h * abs(sy)))
    cv = Canvas(int(w), int(h))
    x0, y0 = (int(w) - W) // 2, int(h) - H
    cv.rect(x0, y0, W, H, WHITE)
    cv.frame(x0, y0, W, H, R("STONE", 4))
    for k in range(3, H - 2, 5):
        cv.hline(x0 + 3, x0 + W - 4, y0 + k, R("SKY", 5) if sy > 0 else R("STONE", 6))
    if fold:
        cv.vline(x0 + W // 2, y0, y0 + H - 1, R("STONE", 5))
    return cv


@generator(kind="flipbook")
def sheet_flip(w=40, h=52):
    """a4s20: the sheet clipped behind flips up (4 drawings)"""
    return _fb([_sheet(w, h, sy) for sy in (0.3, 0.7, -0.6, 1.0)], [2, 2, 2, 10], 0)


@generator(kind="flipbook")
def fold4(w=40, h=52):
    """the sheet folds in half, then in quarters"""
    return _fb([_sheet(w, h), _sheet(w, h, sx=0.7, fold=1), _sheet(w, h, sx=0.5, fold=1),
                _sheet(w, h, sx=0.5, sy=0.5, fold=1)], [4, 3, 3, 10], 0)


@generator(kind="flipbook")
def stamp(seed=1):
    """the rubber stamp comes down (squash on contact), lifts off the mark"""
    def one(y, sq, mark):
        cv = Canvas(26, 40)
        if mark:
            cv.rect(4, 34, 18, 5, P("CHERRY", 1))
        cv.rect(10, y, 6, 12, R("WOOD", 4))
        cv.ellipse(13, y, 5, 3, R("WOOD", 5))
        hh = int(6 * sq)
        cv.rect(4, y + 12, 18, hh, P("CHERRY", 0))
        return cv
    return _fb([one(4, 1.0, 0), one(16, 1.0, 0), one(22, 0.7, 0), one(14, 1.0, 1), one(2, 1.0, 1)], [3, 1, 3, 2, 8], 0)


@generator(kind="flipbook")
def crate_lid(seed=2):
    """a4s29: the MINGLE crate's lid creaks open (first time in eleven years)"""
    from .sets_park import mingle_crate
    box = mingle_crate(seed=seed, w=52, h=20)
    out = []
    for k, ang in enumerate((0, 15, 40, 75)):
        cv = Canvas(64, 52)
        cv.blit(box, 6, 32)
        a = math.radians(ang)
        hx, hy = 6, 32
        L = 52
        x1, y1 = hx + L * math.cos(a), hy - L * math.sin(a)
        for t in range(4):
            cv.line(hx, hy - t, x1, y1 - t, R("WOOD", 5 if t else 2))
        out.append(cv)
    return _fb(out, [6, 4, 4, 12], 0)


@generator(kind="flipbook")
def key_turn(seed=1):
    """4-03: the key turns in the padlock (4 drawings, a quarter turn)"""
    out = []
    for k in range(4):
        cv = Canvas(16, 16)
        a = math.pi / 2 * k / 3
        cx, cy = 8, 8
        dx, dy = math.cos(a) * 6, math.sin(a) * 6
        cv.line(cx - dx, cy - dy, cx + dx, cy + dy, pal.GOLD, w=2)
        cv.ellipse(cx - dx, cy - dy, 2.5, 2.5, pal.GOLD)
        cv.px(cx - dx, cy - dy, D(pal.GOLD, 2))
        out.append(cv)
    return _fb(out, [3, 3, 3, 10], 0)


@generator(kind="flipbook")
def hand_slip(seed=70):
    """co07: the loop slips off the cherub's fingers (hold, slide 1, slide 2, open)"""
    from .crayon import crayon_hand
    h = crayon_hand(pose="hold", seed=seed)
    o = crayon_hand(pose="open", seed=seed)
    mid = h.copy()
    diff = (h.a != o.a) & (h.a != 0)
    ys, xs = np.nonzero(diff)
    mid.a[diff] = 0
    mid.a[np.clip(ys + 2, 0, h.h - 1), xs] = h.a[ys, xs]
    return _fb([h, mid, o], [8, 3, 30], 0)


# ---------------------------------------------------------------------------
# the turkey (the creator: "And make him a turkey too")
# ---------------------------------------------------------------------------
def _roast(s=1):
    cv = Canvas(int(40 * s), int(28 * s))
    plate = cv.ellipse_mask(20 * s, 24 * s, 19 * s, 3.5 * s)
    cv.shape(plate, WHITE, outline=R("STONE", 4))
    body = cv.ellipse_mask(20 * s, 15 * s, 13 * s, 9 * s)
    cv.shape(body, P("CARAMEL", 1), outline=P("CHOCOLATE", 0), hi=P("HONEY", 2), lo=P("CARAMEL", 0))
    for side in (-1, 1):                                       # drumsticks
        lx = 20 * s + side * 12 * s
        leg = cv.ellipse_mask(lx, 10 * s, 4 * s, 5.5 * s)
        cv.shape(leg, P("CARAMEL", 2), outline=P("CHOCOLATE", 0))
        cv.rect(lx - 1 * s + side * 2 * s, 3 * s, 2 * s, 3 * s, WHITE)
        cv.ellipse(lx + side * 2 * s, 3 * s, 1.5 * s, 1.2 * s, WHITE)
    cv.ellipse(15 * s, 12 * s, 2 * s, 1.2 * s, P("LEMON", 2))   # the glaze shine
    for x in (8, 32):                                         # parsley
        cv.ellipse(x * s, 21 * s, 2 * s, 1.5 * s, P("LIME", 1))
    return cv


@generator(kind="flipbook")
def turkey(kind="roast", size=1):
    """kind=roast: a glazed roast turkey on its platter, steam curling up (8 x 3 f)
    -- it sits on Nana's bake-tent table or in a cast member's paws (prop OVER);
    kind=live: a round pastel festival turkey (a critter, not food) that bobs,
    fans its tail and gobbles (6 drawings); kind=walk: its 4-drawing strut"""
    s = int(size)
    if kind == "roast":
        from .amb import amb_steam
        st = amb_steam(w=10 * s, h=18 * s)
        out = []
        for f in st.meta["frames"]:
            base = _roast(s)
            cv = Canvas(base.w, base.h + f.h - 6 * s)
            cv.blit(base, 0, cv.h - base.h)
            cv.blit(f, base.w // 2 - f.w // 2 - 2 * s, 0)
            cv.blit(f, base.w // 2 - f.w // 2 + 6 * s, 2 * s, flip=True)
            out.append(cv)
        return _fb(out, 3)

    def live(fan=0, gob=0, bob=0, step=-1):
        cv = Canvas(int(34 * s), int(34 * s))
        cx, base = 17 * s, 33 * s - bob * s
        cols = [P("TANGERINE", 1), P("LEMON", 1), P("BUBBLEGUM", 2), P("MINT", 2), P("LAVENDER", 2)]
        for k in range(7):                                    # the fan tail
            a = math.pi * (0.15 + 0.7 * k / 6.0)
            r = (12 + fan * 2) * s
            fx, fy = cx + 2 * s - r * math.cos(a), base - 12 * s - r * math.sin(a)
            cv.ellipse(fx, fy, 3.5 * s, 4.5 * s, cols[k % 5])
            cv.px(fx, fy - 2 * s, WHITE)
        body = cv.ellipse_mask(cx, base - 10 * s, 9 * s, 8 * s)
        cv.shape(body, P("CHOCOLATE", 2), outline=P("CHOCOLATE", 0), hi=P("CARAMEL", 2))
        hx, hy = cx - 7 * s, base - 19 * s
        cv.ellipse(hx, hy, 4 * s, 4 * s, P("PEACH", 2))
        cv.ellipse(hx - 1 * s, hy - 1 * s, 1 * s, 1.2 * s, BLACK)
        cv.poly([(hx - 4 * s, hy), (hx - 7 * s, hy + 1 * s), (hx - 4 * s, hy + 2 * s)], P("LEMON", 1))
        cv.ellipse(hx - 4 * s, hy + (4 + gob * 2) * s, 1.2 * s, (1.8 + gob) * s, P("CHERRY", 1))  # the wattle
        cv.ellipse(hx + 2 * s, hy + 1 * s, 1.2 * s, 0.8 * s, P("ROSE", 2))
        for side, lift in ((-1, step == 0), (1, step == 1)):
            fx = cx + side * 3 * s
            cv.vline(int(fx), int(base - 3 * s - (2 * s if lift else 0)), int(base - (2 * s if lift else 0)), P("TANGERINE", 0))
            cv.hline(int(fx - 1 * s), int(fx + 1 * s), int(base - (2 * s if lift else 0)), P("TANGERINE", 0))
        return cv
    if kind == "walk":
        return _fb([live(0, 0, 0, 0), live(0, 0, 1, -1), live(0, 0, 0, 1), live(0, 0, 1, -1)], 4)
    return _fb([live(0, 0, 0), live(1, 0, 1), live(2, 0, 0), live(2, 1, 0), live(2, 2, 1), live(1, 0, 0)],
               [10, 4, 6, 3, 6, 8])
