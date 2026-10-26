"""kit.py - pixel-art building blocks shared by every pixel set (owner: ART).

Rules that keep backgrounds from fighting the characters:
  * background shapes take a family-dark outline (two DARKER steps of their
    own fill), never the black outline the game's sprites wear;
  * far planes are lighter and greyer than mid planes; the brightest, most
    saturated colours are kept for props and characters;
  * every surface is drawn in DAY colours: time of day is a CLUT swap
    (light.py), so lamps, windows and lantern cores use the emissive indices
    (CANDY.7, GOLD, WHITE) that the night tint keeps lit.

All coordinates are plate pixels. Plates at px=1 are 240 rows tall (open
matte): show y = plate y - 30. Y() converts.
"""
import math

import numpy as np

from . import pal
from .canvas import Canvas, Rng, hash2, bayer

R = pal.ramp
P = pal.paint
D = lambda c, n=2: _darker(c, n)   # noqa: E731


def _darker(c, n=1):
    for _ in range(n):
        c = int(pal.DARKER[c])
    return c


def _lighter(c, n=1):
    for _ in range(n):
        c = int(pal.LIGHTER[c])
    return c


L = _lighter
MATTE = 30


def Y(y):
    """show y -> plate y (px=1, 240-row plates)."""
    return int(y) + MATTE


def plate(w, h=240):
    return Canvas(w, h)


# ---------------------------------------------------------------------------
# skies and far scenery
# ---------------------------------------------------------------------------
def sky_fill(cv, rows):
    """Paint a BANDS row list into a canvas (for previews and baked skies)."""
    for y in range(min(cv.h, len(rows))):
        cv.a[y, :] = rows[y]


def stars(cv, n, seed, y0=0, y1=120, cols=None):
    rng = Rng(seed)
    cols = cols or [pal.WHITE, R("SKY", 7), R("CANDY", 7), R("SKY", 6)]
    for _ in range(n):
        x, y = rng.randint(0, cv.w - 1), rng.randint(y0, y1)
        c = rng.choice(cols)
        cv.px(x, y, c)
        if rng.chance(0.12):
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                cv.px(x + dx, y + dy, R("SKY", 5))


def cloud(cv, x, y, w, seed, fill=None, shade=None, hi=None):
    """A puffy cumulus: overlapping ellipses, flat-ish bottom, shaded underside."""
    fill = fill if fill is not None else pal.WHITE
    shade = shade if shade is not None else R("SKY", 7)
    rng = Rng(seed)
    m = np.zeros((cv.h, cv.w), bool)
    n = max(3, w // 14)
    for k in range(n):
        cx = x + (k + 0.5) * w / n + rng.uniform(-3, 3)
        r = rng.uniform(w / n * 0.65, w / n * 1.05)
        cy = y - r * 0.35 + rng.uniform(-2, 2)
        m |= cv.ellipse_mask(cx, cy, r, r * 0.8)
    base = y + 2
    m[int(base):, :] = False
    cv.a[m] = fill
    # underside shade: lower third of the mask
    ys = np.nonzero(m)[0]
    if len(ys):
        cut = int(ys.max() - (ys.max() - ys.min()) * 0.28)
        sm = m.copy()
        sm[:cut, :] = False
        cv.a[sm] = shade
    if hi is not None:
        cv.a[cv.inner_edge(m, "top")] = hi
    return m


def hills(cv, base_y, amp, period, fill, seed, top=None, phase=0.0, lumps=3, bottom=None, x0=0):
    """Rolling hills from `base_y` (crest line) down to the plate bottom.  x0: the canvas
    column where the hills' own x=0 sits (a padded plate keeps its centre identical)."""
    rng = Rng(seed)
    ph = [rng.uniform(0, 6.28) for _ in range(lumps)]
    xs = np.arange(cv.w)
    crest = np.full(cv.w, float(base_y))
    for k in range(lumps):
        crest -= amp / (k + 1) * (0.5 + 0.5 * np.sin((xs - x0) / (period / (k + 1)) * 6.283 + ph[k] + phase))
    bottom = cv.h if bottom is None else bottom
    m = np.zeros((cv.h, cv.w), bool)
    for x in xs:
        y0 = int(round(crest[x]))
        m[max(0, y0):bottom, x] = True
    cv.a[m] = fill
    if top is not None:
        cv.a[cv.inner_edge(m, "top")] = top
    return m, crest


def tree_round(cv, x, base, r, leaf=None, trunk=None, seed=1, outline=True):
    """A fluffy lollipop tree (HaHaland style)."""
    leaf = leaf if leaf is not None else R("GRASS", 5)
    trunk = trunk if trunk is not None else R("WOOD", 3)
    th = int(r * 0.9)
    cv.rect(x - 1, base - th, 3, th, trunk)
    cv.px(x + 1, base - th, D(trunk, 1))
    rng = Rng(seed)
    cy = base - th - r * 0.75
    m = cv.ellipse_mask(x + 0.5, cy, r, r * 0.9)
    for k in range(4):
        a = k * 1.57 + rng.uniform(0, 1)
        m |= cv.ellipse_mask(x + 0.5 + math.cos(a) * r * 0.55, cy + math.sin(a) * r * 0.45,
                             r * 0.6, r * 0.55)
    cv.a[m] = leaf
    # shade lower-right, light upper-left
    xs, ys = cv.grid()
    sh = m & ((xs - x) * 0.6 + (ys - cy) > r * 0.25)
    cv.a[sh] = D(leaf, 1)
    hl = m & ((xs - x) * 0.7 + (ys - cy) < -r * 0.55)
    cv.a[hl] = L(leaf, 1)
    if outline:
        cv.a[cv.outline_mask(m)] = D(leaf, 3)
    return m


def tree_pine(cv, x, base, h, leaf=None, seed=1):
    leaf = leaf if leaf is not None else R("GRASS", 3)
    w = h * 0.45
    for k in range(3):
        y0 = base - h + k * h * 0.28
        pts = [(x, y0), (x + w * (0.55 + k * 0.2), y0 + h * 0.42), (x - w * (0.55 + k * 0.2), y0 + h * 0.42)]
        cv.poly(pts, leaf if k % 2 == 0 else D(leaf, 1))
    cv.rect(x - 1, base - h * 0.15, 3, int(h * 0.15) + 1, R("WOOD", 2))


def bush(cv, x, base, w, h, fill=None, seed=1, flowers=None):
    fill = fill if fill is not None else R("GRASS", 4)
    rng = Rng(seed)
    m = np.zeros((cv.h, cv.w), bool)
    n = max(2, int(w // 8))
    for k in range(n):
        cx = x + (k + 0.5) * w / n
        m |= cv.ellipse_mask(cx, base - h * 0.55 + rng.uniform(-1.5, 1.5), w / n * 0.8, h * 0.6)
    m[base:, :] = False
    cv.a[m] = fill
    cv.a[cv.inner_edge(m, "top")] = L(fill, 1)
    cv.a[cv.outline_mask(m) & (cv.a != 0)] = D(fill, 2)
    if flowers:
        ys, xs = np.nonzero(m)
        for k in range(len(xs) // 25):
            i = rng.randint(0, len(xs) - 1)
            cv.px(xs[i], ys[i], rng.choice(flowers))
    return m


# ---------------------------------------------------------------------------
# ground surfaces
# ---------------------------------------------------------------------------
def grass(cv, x, y, w, h, seed, ramp_lo=3, ramp_hi=6, flowers=True, tufts=True, period=None):
    """A grass field: banded GRASS ramp (darker near the top = farther), tufts, flowers.

    period=N draws one N-px-wide strip and repeats it across (tile-friendly: a TILEMAP
    dedupes the repeats)."""
    if period:
        strip = Canvas(int(period), cv.h)
        grass(strip, 0, y, int(period), h, seed, ramp_lo, ramp_hi, flowers, tufts, None)
        for xx in range(x - (x % int(period)), x + w, int(period)):
            src = strip.a[y:y + h]
            x0, x1 = max(xx, x), min(xx + int(period), x + w)
            if x1 > x0:
                cv.a[y:y + h, x0:x1] = src[:, x0 - xx:x1 - xx]
        return
    cv.vgrad(x, y, w, h, [R("GRASS", s) for s in range(ramp_hi, ramp_lo - 1, -1)][::-1][::-1])
    rng = Rng(seed)
    if tufts:
        for _ in range(w * h // 40):
            tx, ty = rng.randint(x, x + w - 1), rng.randint(y + 1, y + h - 1)
            c = cv.get(tx, ty)
            if c:
                cv.px(tx, ty, D(c, 1))
                cv.px(tx, ty - 1, D(c, 1) if rng.chance(0.5) else c)
                if rng.chance(0.3):
                    cv.px(tx + 1, ty, L(c, 1))
    if flowers:
        fl = [P("BUBBLEGUM", 2), P("LEMON", 2), pal.WHITE, P("LAVENDER", 2), P("ROSE", 2)]
        for _ in range(w * h // 260):
            fx, fy = rng.randint(x, x + w - 1), rng.randint(y + 2, y + h - 1)
            c = rng.choice(fl)
            cv.px(fx, fy, c)
            if fy > y + h * 0.5 and rng.chance(0.5):
                cv.px(fx + 1, fy, c)


def cobbles(cv, x, y, w, h, seed, base=None, mortar=None, rows_px=(5, 9)):
    """Rounded cobbles in perspective rows (smaller near the top)."""
    base = base if base is not None else R("STONE", 5)
    mortar = mortar if mortar is not None else R("STONE", 3)
    rng = Rng(seed)
    cv.rect(x, y, w, h, mortar)
    yy = y
    row = 0
    while yy < y + h:
        t = (yy - y) / max(1, h)
        rh = int(rows_px[0] + (rows_px[1] - rows_px[0]) * t)
        cw = int(rh * 1.6)
        xx = x - (row % 2) * cw // 2
        while xx < x + w:
            sw = cw + rng.randint(-1, 1)
            c = rng.choice([base, base, L(base, 1), D(base, 1)])
            cv.rect(xx + 1, yy + 1, sw - 2, rh - 2, c)
            cv.rect(xx + 2, yy, sw - 4, 1, c)
            cv.px(xx + 2, yy + 1, L(c, 1))
            cv.rect(xx + 2, yy + rh - 2, sw - 4, 1, D(c, 1))
            xx += sw
        yy += rh
        row += 1
    cv.rect(x, y + h, w, 0, mortar)


def planks(cv, x, y, w, h, seed, base=None, vertical=False, pw=6, gap=None):
    """Wooden boards with grain flecks and nail dots."""
    base = base if base is not None else R("WOOD", 4)
    gap = gap if gap is not None else D(base, 2)
    rng = Rng(seed)
    if vertical:
        xx = x
        while xx < x + w:
            c = rng.choice([base, base, L(base, 1), D(base, 1)])
            cv.rect(xx, y, pw - 1, h, c)
            cv.rect(xx + pw - 1, y, 1, h, gap)
            cv.rect(xx, y, 1, h, L(c, 1))
            for _ in range(h // 10):
                cv.px(xx + rng.randint(1, pw - 3), rng.randint(y, y + h - 1), D(c, 1))
            cv.px(xx + pw // 2, y + 2, D(c, 2))
            cv.px(xx + pw // 2, y + h - 3, D(c, 2))
            xx += pw
    else:
        yy = y
        while yy < y + h:
            c = rng.choice([base, base, L(base, 1), D(base, 1)])
            cv.rect(x, yy, w, pw - 1, c)
            cv.rect(x, yy + pw - 1, w, 1, gap)
            cv.rect(x, yy, w, 1, L(c, 1))
            seam = rng.randint(x, x + 40)
            while seam < x + w:
                cv.rect(seam, yy, 1, pw - 1, gap)
                seam += rng.randint(40, 70)
            for _ in range(w // 12):
                cv.px(rng.randint(x, x + w - 1), rng.randint(yy + 1, yy + pw - 2), D(c, 1))
            yy += pw


def bricks(cv, x, y, w, h, seed, base=None, mortar=None, bw=12, bh=6, chips=True, period=0):
    """Bricks; period=N (in bricks) repeats the colour pattern every N bricks across, so a
    TILEMAP of a long wall dedupes to a handful of tiles."""
    base = base if base is not None else R("STONE", 4)
    mortar = mortar if mortar is not None else R("STONE", 2)
    if period:
        rows = (h + bh - 1) // bh
        cv.rect(x, y, w, h, mortar)
        for row in range(rows):
            yy = y + row * bh
            off = bw // 2 if row % 2 else 0
            xx = x - off
            col = 0
            while xx < x + w:
                hsh = hash2(col % int(period), row, seed)
                c = (base, base, base, D(base, 1), L(base, 1))[hsh % 5]
                cv.rect(xx + 1, yy + 1, bw - 1, bh - 1, c)
                cv.rect(xx + 1, yy + 1, bw - 1, 1, L(c, 1))
                if chips and hsh % 4 == 0:
                    cv.px(xx + 2 + (hsh >> 8) % max(1, bw - 4), yy + 2 + (hsh >> 16) % max(1, bh - 3), D(c, 1))
                xx += bw
                col += 1
        return
    rng = Rng(seed)
    cv.rect(x, y, w, h, mortar)
    row = 0
    yy = y
    while yy < y + h:
        xx = x - (bw // 2 if row % 2 else 0)
        while xx < x + w:
            c = rng.choice([base, base, base, D(base, 1), L(base, 1)])
            cv.rect(xx + 1, yy + 1, bw - 1, bh - 1, c)
            cv.rect(xx + 1, yy + 1, bw - 1, 1, L(c, 1))
            if chips and rng.chance(0.25):
                cv.px(xx + rng.randint(2, bw - 2), yy + rng.randint(2, bh - 2), D(c, 1))
            xx += bw
        yy += bh
        row += 1


# ---------------------------------------------------------------------------
# festival dressing
# ---------------------------------------------------------------------------
def catenary(x0, y0, x1, y1, sag, n=None):
    n = n or max(2, int(abs(x1 - x0)))
    pts = []
    for k in range(n + 1):
        t = k / n
        x = x0 + (x1 - x0) * t
        y = y0 + (y1 - y0) * t + sag * 4 * t * (1 - t)
        pts.append((x, y))
    return pts


def draw_string(cv, pts, c):
    for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
        cv.line(xa, ya, xb, yb, c)


def bunting(cv, x0, y0, x1, y1, sag, colors, every=10, size=6, string=None, seed=1):
    pts = catenary(x0, y0, x1, y1, sag)
    draw_string(cv, pts, string if string is not None else R("WOOD", 2))
    k = 0
    span = abs(x1 - x0)
    for i in range(every // 2, int(span), every):
        px, py = pts[min(i, len(pts) - 1)]
        c = colors[k % len(colors)]
        tri = [(px - size // 2, py), (px + size // 2 + 1, py), (px + 0.5, py + size)]
        cv.poly(tri, c)
        cv.px(px - size // 2, py, D(c, 1))
        k += 1
    return pts


LANTERN_COLS = [P("BUBBLEGUM", 1), P("LEMON", 1), P("MINT", 1), P("LAVENDER", 1), P("PEACH", 1)]


def lantern_img(color=None, state="lit", size=1):
    """A round paper lantern sprite (8x11 at size 1): cap, ribs, glowing core.

    states: lit (warm emissive core), out (dark, no glow), purple (glows purple
    from inside, the EC1 IN card), lean (flame leans right = south), blue
    (a blue bead), red (the possessed glow)."""
    color = color if color is not None else LANTERN_COLS[0]
    w, h = 8 * size, 11 * size
    cv = Canvas(w + 2, h + 2)
    cx, cy = (w + 2) / 2, 1 + 2 * size + (h - 3 * size) / 2
    body = cv.ellipse_mask(cx, cy, w / 2, (h - 3 * size) / 2)
    if state == "out":
        fill = D(color, 3)
        core = None
    elif state == "purple":
        fill = P("VAPE_PURPLE", 1)
        core = P("VAPE_PURPLE", 2)
    elif state == "red":
        fill = D(color, 2)
        core = P("CHERRY", 1)
    else:
        fill = color
        core = R("CANDY", 7)
    cv.a[body] = fill
    if core is not None:
        xs, ys = cv.grid()
        off = 1 if state == "lean" else 0
        cm = cv.ellipse_mask(cx + off * size, cy + 0.5, w / 4 + 0.3, (h - 3 * size) / 4 + 0.5)
        cv.a[cm & body] = core
        if state not in ("blue",):
            cv.a[cv.ellipse_mask(cx + off * size, cy + 0.5, 1.0 * size, 1.2 * size) & body] = pal.WHITE \
                if state == "lit" or state == "lean" else core
    # ribs
    for dx in (-2 * size, 2 * size):
        for y in range(int(cy - (h - 3 * size) / 2) + 1, int(cy + (h - 3 * size) / 2)):
            if body[y, int(cx + dx)]:
                cv.a[y, int(cx + dx)] = D(cv.a[y, int(cx + dx)], 1)
    # caps and hook
    cap = R("WOOD", 2) if state != "purple" else R("STONE", 2)
    cv.rect(cx - 2 * size, 1 + size, 4 * size, size, cap)
    cv.rect(cx - 2 * size, h - size + 1, 4 * size, size, cap)
    cv.px(cx - 0.5, 0, R("WOOD", 2))
    cv.px(cx - 0.5, 1, R("WOOD", 2))
    cv.a[cv.outline_mask(cv.a != 0) & (np.arange(cv.h)[:, None] > 1)] = D(fill, 3) if state != "out" \
        else R("STONE", 1)
    cv.meta["anchor"] = "top"
    return cv


def glow_pool(cv, cx, cy, rx, ry, c=None, levels=(10, 5, 2)):
    """A dithered warm light pool (drawn on its own fixed-light plane)."""
    c = c if c is not None else R("CANDY", 6)
    b = bayer(cv.w, cv.h)
    for k, lvl in enumerate(levels):
        f = 1.0 - k * 0.3
        m = cv.ellipse_mask(cx, cy, rx * (0.5 + k * 0.35) / f * f, ry * (0.5 + k * 0.35))
        cv.a[m & (b < lvl) & (cv.a == 0)] = c


def awning(cv, x, y, w, h, colors, scallop=True, stripe=6):
    """A striped awning with a scalloped hem."""
    for xx in range(x, x + w):
        k = ((xx - x) // stripe) % len(colors)
        c = colors[k]
        cv.rect(xx, y, 1, h, c)
    cv.rect(x, y, w, 1, L(colors[0], 1))
    if scallop:
        for xx in range(x, x + w, stripe):
            k = ((xx - x) // stripe) % len(colors)
            c = colors[k]
            cv.ellipse(xx + stripe / 2, y + h, stripe / 2, 2.2, c)
            cv.px(xx + stripe // 2, y + h + 2, D(c, 1))
    m = np.zeros((cv.h, cv.w), bool)
    m[y:y + h + 3, x:x + w] = cv.a[y:y + h + 3, x:x + w] != 0
    cv.a[cv.outline_mask(m) & (np.arange(cv.h)[:, None] >= y)] = D(colors[0], 3)


def booth(cv, x, base, w, h, colors, counter=None, post=None, roof=12, seed=1, back=None):
    """A fair booth: posts, a counter, a dark interior and a striped awning on top."""
    counter = counter if counter is not None else R("WOOD", 4)
    post = post if post is not None else pal.WHITE
    back = back if back is not None else R("PEACH", 2)
    top = base - h
    cv.rect(x + 2, top + roof, w - 4, h - roof, back)
    cv.dither_rect(x + 2, top + roof, w - 4, 4, back, D(back, 1), 8)
    # counter
    ch = max(10, h // 3)
    planks(cv, x, base - ch, w, ch, seed, base=counter, vertical=True, pw=5)
    cv.rect(x - 1, base - ch - 2, w + 2, 3, L(counter, 1))
    cv.rect(x - 1, base - ch - 2, w + 2, 1, L(counter, 2))
    # posts
    for px_ in (x, x + w - 3):
        cv.rect(px_, top + roof, 3, h - roof, post)
        cv.rect(px_ + 2, top + roof, 1, h - roof, D(post, 1))
    awning(cv, x - 3, top, w + 6, roof, colors)
    return (x + 4, top + roof + 2, w - 8, h - roof - ch - 4)   # interior box


def tent(cv, cx, base, w, h, colors, door=True, flag=None, stripe=None):
    """A peaked circus tent with a scalloped valance and an open door."""
    stripe = stripe or max(5, w // 9)
    wall_h = int(h * 0.48)
    roof_h = h - wall_h
    top = base - h
    x0 = cx - w // 2
    # walls: vertical stripes
    for xx in range(x0, x0 + w):
        c = colors[((xx - x0) // stripe) % len(colors)]
        cv.rect(xx, base - wall_h, 1, wall_h, c)
    # roof: stripes converge on the peak
    m = cv.poly_mask([(cx, top), (x0 + w + 3, base - wall_h + 1), (x0 - 3, base - wall_h + 1)])
    xs, ys = cv.grid()
    with np.errstate(divide="ignore", invalid="ignore"):
        t = np.where(ys > top, (xs - cx) / np.maximum(1, (ys - top)) * roof_h, 0)
    k = (np.floor(t / stripe) % len(colors)).astype(int)
    for i, c in enumerate(colors):
        cv.a[m & (k == i)] = c
    # valance scallops
    for xx in range(x0 - 3, x0 + w + 3, stripe):
        c = colors[((xx - x0) // stripe) % len(colors)]
        cv.ellipse(xx + stripe / 2, base - wall_h + 1, stripe / 2 + 0.3, 2.4, c)
    if door:
        dw = max(8, w // 4)
        dm = cv.poly_mask([(cx - dw // 2, base), (cx, base - wall_h * 0.85), (cx + dw // 2, base)])
        cv.a[dm] = D(colors[0], 3)
        cv.a[cv.inner_edge(dm, "left")] = D(colors[0], 2)
    whole = m.copy()
    whole[base - wall_h:base, x0:x0 + w] = True
    cv.a[cv.outline_mask(whole)] = D(colors[0], 3)
    # pole + pennant
    cv.vline(cx, top - 7, top, R("WOOD", 2))
    fc = flag if flag is not None else P("CHERRY", 1)
    cv.poly([(cx + 1, top - 7), (cx + 7, top - 5), (cx + 1, top - 3)], fc)
    return (cx - w // 2, base - wall_h, w, wall_h)


def sign_board(w, h, face=None, frame=None, posts=0, style="plank", seed=1):
    """A blank sign board; returns (canvas, text box) - the text is a string, drawn by the player."""
    face = face if face is not None else pal.CREAM
    frame = frame if frame is not None else R("WOOD", 3)
    ph = posts
    cv = Canvas(w, h + ph)
    if ph:
        for px_ in (w // 5, w - w // 5 - 2):
            cv.rect(px_, h - 2, 3, ph + 2, R("WOOD", 3))
            cv.rect(px_ + 2, h - 2, 1, ph + 2, R("WOOD", 2))
    cv.rect(0, 0, w, h, frame)
    cv.rect(0, 0, w, 1, L(frame, 1))
    if style == "plank":
        planks(cv, 2, 2, w - 4, h - 4, seed, base=face, pw=max(4, (h - 4) // 3))
    else:
        cv.rect(2, 2, w - 4, h - 4, face)
        cv.rect(2, 2, w - 4, 1, L(face, 1))
    cv.a[cv.outline_mask(cv.a != 0)] = D(frame, 2)
    cv2 = Canvas(w + 2, h + ph + 2)
    cv2.blit(cv, 1, 1)
    cv2.a[cv2.outline_mask(cv2.a != 0)] = D(frame, 3)
    cv2.meta["text"] = (4, 4, w - 6, h - 6)
    return cv2


def lamp_post(cv, x, base, h, lit=True, head=None):
    pole = R("STEEL", 2)
    cv.rect(x - 1, base - h, 2, h, pole)
    cv.px(x - 1, base - h, R("STEEL", 4))
    cv.rect(x - 2, base - 3, 4, 3, pole)
    head = head if head is not None else (R("CANDY", 7) if lit else R("STONE", 4))
    cv.rect(x - 3, base - h - 6, 6, 6, head)
    cv.rect(x - 3, base - h - 7, 6, 1, pole)
    cv.rect(x - 2, base - h - 8, 4, 1, pole)
    cv.frame(x - 4, base - h - 7, 8, 8, D(pole, 1))


def picket_fence(cv, x0, x1, base, h, c=None):
    c = c if c is not None else pal.WHITE
    cv.rect(x0, base - h + 3, x1 - x0, 2, D(c, 1))
    cv.rect(x0, base - 4, x1 - x0, 2, D(c, 1))
    for x in range(x0, x1, 5):
        cv.rect(x, base - h + 1, 3, h - 1, c)
        cv.px(x + 1, base - h, c)
        cv.rect(x + 2, base - h + 1, 1, h - 1, D(c, 1))


def window(cv, x, y, w, h, frame=None, glass=None, lit=False, cross=True, curtain=None):
    frame = frame if frame is not None else pal.WHITE
    glass = glass if glass is not None else (R("CANDY", 7) if lit else R("SKY", 5))
    cv.rect(x, y, w, h, frame)
    cv.rect(x + 1, y + 1, w - 2, h - 2, glass)
    if not lit:
        cv.px(x + 2, y + 2, R("SKY", 7))
        cv.px(x + 3, y + 2, R("SKY", 7))
        cv.px(x + 2, y + 3, R("SKY", 7))
    if cross:
        cv.rect(x + w // 2, y + 1, 1, h - 2, frame)
        cv.rect(x + 1, y + h // 2, w - 2, 1, frame)
    if curtain is not None:
        cv.rect(x + 1, y + 1, 2, h - 2, curtain)
        cv.rect(x + w - 3, y + 1, 2, h - 2, curtain)
    cv.frame(x - 1, y - 1, w + 2, h + 2, D(frame, 2))


def cottage(cv, x, base, w, h, wall=None, roof=None, door=None, seed=1, lit=False, chimney=True):
    """A plush HaHaland cottage: round-shouldered walls, scalloped roof, heart window."""
    wall = wall if wall is not None else P("PEACH", 2)
    roof = roof if roof is not None else P("BUBBLEGUM", 1)
    door = door if door is not None else P("ROSE", 1)
    wh = int(h * 0.58)
    cv.rect(x, base - wh, w, wh, wall)
    cv.rect(x, base - wh, 1, wh, L(wall, 1))
    cv.rect(x + w - 2, base - wh, 2, wh, D(wall, 1))
    # roof: triangle with scallop rows
    rt = base - h
    rm = cv.poly_mask([(x + w / 2, rt), (x + w + 4, base - wh + 2), (x - 4, base - wh + 2)])
    cv.a[rm] = roof
    xs, ys = cv.grid()
    sc = rm & (((ys - rt) % 4) == 3) & ((xs // 2 + (ys // 4)) % 2 == 0)
    cv.a[sc] = D(roof, 1)
    cv.a[cv.inner_edge(rm, "top")] = L(roof, 1)
    cv.a[cv.outline_mask(rm)] = D(roof, 3)
    if chimney:
        cx = x + int(w * 0.72)
        cv.rect(cx, rt + 4, 5, (base - wh) - rt - 6, R("PASTEL_PINK", 4))
        cv.rect(cx - 1, rt + 3, 7, 2, R("PASTEL_PINK", 5))
    # door
    dw, dh = max(7, w // 5), int(wh * 0.62)
    dx = x + w // 2 - dw // 2 - (w // 6 if w > 40 else 0)
    cv.rect(dx, base - dh, dw, dh, door)
    cv.ellipse(dx + dw / 2, base - dh, dw / 2, 3, door)
    cv.px(dx + dw - 3, base - dh // 2, R("CANDY", 6))
    cv.frame(dx - 1, base - dh - 2, dw + 2, dh + 2, D(door, 2))
    # windows
    window(cv, x + w - w // 3 - 2, base - wh + 6, max(7, w // 5), max(7, w // 5), lit=lit,
           curtain=P("BUBBLEGUM", 2))
    whole = np.zeros((cv.h, cv.w), bool)
    whole[base - wh:base, x:x + w] = True
    cv.a[cv.outline_mask(whole) & ~rm & (ys < base)] = D(wall, 3)


def heart(cv, cx, cy, r, c, outline=None):
    """A pixel heart centred at (cx, cy), radius r."""
    m = cv.ellipse_mask(cx - r * 0.5, cy - r * 0.2, r * 0.55, r * 0.55)
    m |= cv.ellipse_mask(cx + r * 0.5, cy - r * 0.2, r * 0.55, r * 0.55)
    m |= cv.poly_mask([(cx - r * 1.02, cy), (cx + r * 1.02, cy), (cx, cy + r * 1.0)])
    cv.a[m] = c
    if outline is not None:
        cv.a[cv.outline_mask(m)] = outline
    return m


def shadow_ellipse(cv, cx, cy, rx, ry, lvl=8):
    """A dithered contact shadow: darkens what is already there (stays on palette)."""
    m = cv.ellipse_mask(cx, cy, rx, ry) & (cv.a != 0)
    b = bayer(cv.w, cv.h)
    mm = m & (b < lvl)
    cv.a[mm] = pal.DARKER[cv.a[mm]]


def vignette_dark(cv, x0, x1, lvl_max=10):
    """Darken towards the left/right plate edges with an ordered dither."""
    b = bayer(cv.w, cv.h)
    for x in range(cv.w):
        d = 0
        if x < x0:
            d = (x0 - x) / max(1, x0)
        elif x > x1:
            d = (x - x1) / max(1, cv.w - x1)
        lvl = int(lvl_max * d)
        if lvl:
            col = cv.a[:, x]
            m = (b[:, x] < lvl) & (col != 0)
            col[m] = pal.DARKER[col[m]]
