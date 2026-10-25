"""crayon.py - the storybook crayon register (cold open shots 1-11, 17; end credits).

A child's crayon on paper, all palette-exact:
  * fills are waxy hatching: the ink runs in diagonal lanes and leaves paper
    showing through (more at the edges, where a child's crayon skips);
  * outlines are 2-px wobbly crayon lines with a gap pattern;
  * the paper is cream (PAL_UI_CREAM), its grain a separate GRAIN tile at a
    fixed density (never alpha); the live strokes (circles, scribbles) are
    STROKES planes drawn by the GPU, so they can reveal and boil;
  * no faces on the young cherub or the young imp (held for F4).

The reference is the game's own crayon prologue (core/src/cutscene_ch12.c
draw_p0()): red-roofed bandstand, cupcake tower, cauldron, lanterns, bunting.
"""
import math

import numpy as np

from . import generator, pal
from .canvas import Canvas, Rng, hash2, bayer
from . import font as gfont

P = pal.paint
R = pal.ramp
PAPER = pal.CREAM
PAPER_D = R("PEACH", 7)          # a warm shade of paper (fibres, folds)
PAPER_DD = R("BONE", 6)

# crayon inks: (ink, dark edge, light)
INK = {
    "red": (P("CHERRY", 1), P("CHERRY", 0), P("CHERRY", 2)),
    "pink": (P("BUBBLEGUM", 1), P("BUBBLEGUM", 0), P("BUBBLEGUM", 2)),
    "rose": (P("ROSE", 1), P("ROSE", 0), P("ROSE", 2)),
    "yellow": (P("LEMON", 1), P("LEMON", 0), P("LEMON", 2)),
    "orange": (P("TANGERINE", 1), P("TANGERINE", 0), P("TANGERINE", 2)),
    "green": (P("LIME", 1), P("LIME", 0), P("LIME", 2)),
    "dkgreen": (P("FOREST", 2), P("FOREST", 1), P("LIME", 1)),
    "sky": (P("SKY", 1), P("SKY", 0), P("SKY", 2)),
    "blue": (P("COBALT", 1), P("COBALT", 0), P("COBALT", 2)),
    "navy": (P("NAVY", 1), P("NAVY", 0), P("NAVY", 2)),
    "purple": (P("GRAPE", 1), P("GRAPE", 0), P("GRAPE", 2)),
    "lavender": (P("LAVENDER", 1), P("LAVENDER", 0), P("LAVENDER", 2)),
    "brown": (P("CHOCOLATE", 2), P("CHOCOLATE", 1), P("CARAMEL", 1)),
    "tan": (P("CARAMEL", 1), P("CARAMEL", 0), P("CARAMEL", 2)),
    "grey": (pal.GRAY, pal.GRAY_D, pal.GRAY_L),
    "black": (P("CHARCOAL", 1), P("JET", 1), P("CHARCOAL", 2)),
    "white": (pal.WHITE, pal.GRAY_L, pal.WHITE),
    "mint": (P("MINT", 1), P("MINT", 0), P("MINT", 2)),
    "teal": (P("TEAL", 1), P("TEAL", 0), P("TEAL", 2)),
    "sooty": (P("CHARCOAL", 2), P("CHARCOAL", 1), P("SILVER", 0)),
}


def ink(name, k=0):
    return INK[name][k]


# ---------------------------------------------------------------------------
# crayon primitives
# ---------------------------------------------------------------------------
_NOISE_CACHE = {}


def lane_noise(w, h, seed, angle=1, lane=2):
    key = (w, h, seed, angle, lane)
    if key not in _NOISE_CACHE:
        cv = Canvas(w, h)
        xs, ys = cv.grid()
        u = (xs + ys) if angle >= 0 else (xs - ys)
        v = (ys - xs) if angle >= 0 else (ys + xs)
        lane_id = (u // lane).astype(np.int64)
        seg = (v // 7).astype(np.int64)
        a = (lane_id * 374761393 + seg * 668265263 + seed * 2246822519) & 0xFFFFFFFF
        a = ((a ^ (a >> 13)) * 1274126177) & 0xFFFFFFFF
        a = (a ^ (a >> 16)) & 0xFFFF
        b = (xs.astype(np.int64) * 2654435761 + ys.astype(np.int64) * 40503 + seed * 97) & 0xFFFFFFFF
        b = ((b ^ (b >> 15)) * 2246822519) & 0xFFFFFFFF
        b = (b >> 8) & 0xFF
        _NOISE_CACHE[key] = a / 65535.0 * 0.82 + b / 255.0 * 0.18
    return _NOISE_CACHE[key]


def edge_distance(mask, maxd=4):
    """Chebyshev distance of mask pixels to the mask edge, capped at maxd."""
    d = np.zeros(mask.shape, np.int32)
    cur = mask.copy()
    for k in range(1, maxd + 1):
        er = cur.copy()
        er[1:, :] &= cur[:-1, :]
        er[:-1, :] &= cur[1:, :]
        er[:, 1:] &= cur[:, :-1]
        er[:, :-1] &= cur[:, 1:]
        d[cur & ~er] = k
        cur = er
    d[cur] = maxd + 1
    return d


def fill(cv, mask, name, seed=1, coverage=0.88, paper=None, angle=1, shade=None, edge_skip=True):
    """Waxy crayon fill: ink in diagonal lanes; inside the shape the lanes leave the
    ink's light shade (wax over wax), and only at the very edge does the paper show
    (a child's crayon skips at the lines). paper=0 keeps the edge gaps transparent."""
    c_ink, c_dark, c_light = INK[name]
    paper = PAPER if paper is None else paper
    n = lane_noise(cv.w, cv.h, seed, angle)
    thr = np.full(mask.shape, coverage)
    d = edge_distance(mask, 3) if edge_skip else np.full(mask.shape, 9)
    if edge_skip:
        thr = thr - np.where(d == 1, 0.30, np.where(d == 2, 0.08, 0.0))
    on = mask & (n < thr)
    gap = mask & ~on
    cv.a[on] = c_ink
    inner_gap = gap & (d > 1)
    edge_gap = gap & (d <= 1)
    cv.a[inner_gap] = c_light
    if paper:
        cv.a[edge_gap] = paper
    else:
        cv.a[edge_gap] = c_light
    if shade is not None:
        # a heavier pass of the dark tone over part of the shape
        cv.a[on & shade & (n < coverage * 0.55)] = c_dark
    return on


def scribble_fill(cv, mask, name, seed=1, coverage=0.75, paper=None):
    """Two crossing hatch passes: denser, more scribbled (night skies, shadows)."""
    fill(cv, mask, name, seed, coverage * 0.85, angle=1, paper=paper)
    n2 = lane_noise(cv.w, cv.h, seed + 5, -1)
    cv.a[mask & (n2 < coverage * 0.6)] = INK[name][0]


def wobble_pts(pts, seed, amp=0.8, step=3):
    rng = Rng(seed)
    out = []
    for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
        n = max(1, int(math.hypot(xb - xa, yb - ya) / step))
        for k in range(n):
            t = k / n
            out.append((xa + (xb - xa) * t + rng.uniform(-amp, amp),
                        ya + (yb - ya) * t + rng.uniform(-amp, amp)))
    out.append(pts[-1])
    return out


def line(cv, pts, name, w=2, seed=1, gap=(9, 1), wobble=0.7, dark=True):
    """A crayon line: wobbly, `w` px, with a skip every gap[0] px."""
    c = INK[name][1] if dark else INK[name][0]
    pts = wobble_pts(pts, seed, wobble) if wobble else pts
    run = 0
    rng = Rng(seed + 3)
    for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
        n = max(1, int(round(math.hypot(xb - xa, yb - ya))))
        for k in range(n):
            t = k / n
            x, y = xa + (xb - xa) * t, ya + (yb - ya) * t
            run += 1
            if gap and gap[0] and (run % (gap[0] + gap[1])) >= gap[0]:
                continue
            xi, yi = int(round(x)), int(round(y))
            for dy in range(w):
                for dx in range(w):
                    if w > 1 and dx == w - 1 and dy == w - 1 and rng.chance(0.4):
                        continue
                    cv.px(xi + dx - (w - 1) // 2, yi + dy - (w - 1) // 2, c)


def outline(cv, mask, name, w=1, seed=1, gap=(11, 1)):
    """Trace a crayon line around a mask (on its outer edge)."""
    o = cv.outline_mask(mask)
    c = INK[name][1]
    ys, xs = np.nonzero(o)
    rng = Rng(seed)
    for x, y in zip(xs, ys):
        if gap and (hash2(int(x) // 2, int(y) // 2, seed) % (gap[0] + gap[1])) >= gap[0]:
            continue
        cv.a[y, x] = c
        if w > 1 and rng.chance(0.5):
            cv.px(x + rng.randint(-1, 1), y + rng.randint(-1, 1), c)


def blob(cv, cx, cy, rx, ry, seed, wob=0.12, n=24):
    """A hand-drawn closed shape (wobbly ellipse) as a mask."""
    rng = Rng(seed)
    pts = []
    for k in range(n):
        a = k / n * 6.283
        f = 1 + rng.uniform(-wob, wob)
        pts.append((cx + math.cos(a) * rx * f, cy + math.sin(a) * ry * f))
    return cv.poly_mask(pts)


def star(cv, cx, cy, r, name, seed=1, w=1):
    pts = []
    for k in range(6):
        a = -math.pi / 2 + k * 4 * math.pi / 5
        pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    line(cv, pts, name, w=w, seed=seed, gap=None, wobble=0.3, dark=False)


def paper_base(w, h, seed=1, fold=False):
    cv = Canvas(w, h, PAPER)
    rng = Rng(seed)
    for _ in range(w * h // 90):
        x, y = rng.randint(0, w - 1), rng.randint(0, h - 1)
        cv.px(x, y, PAPER_D)
        if rng.chance(0.3):
            cv.px(x + 1, y - 1, PAPER_D)
    return cv


# ---------------------------------------------------------------------------
# generators: paper, grain, calendar
# ---------------------------------------------------------------------------
@generator()
def paper_sheet(seed=1, w=160, h=120):
    """Plain cream paper (a full plate, so the crayon has something to sit on)."""
    return paper_base(w, h, seed)


@generator()
def paper_grain(density=4, seed=2, size=64, ink="fibre"):
    """The GRAIN tile: transparent except paper flecks at a fixed density (% of texels).

    Flecks are single texels and short fibres in random directions, in the paper's
    warm shade: invisible on the cream, a paper tooth over dark crayon.
    """
    cv = Canvas(size, size)
    c = PAPER_D if ink == "fibre" else pal.WHITE
    target = size * size * int(density) // 100
    rng = Rng(seed)
    count = 0
    guard = 0
    dirs = ((1, 0), (0, 1), (1, 1), (1, -1))
    while count < target and guard < target * 20:
        guard += 1
        x, y = rng.randint(0, size - 1), rng.randint(0, size - 1)
        n = 1 if rng.chance(0.7) else 2
        dx, dy = rng.choice(dirs)
        for k in range(n):
            xx, yy = (x + k * dx) % size, (y + k * dy) % size
            if cv.a[yy, xx] == 0:
                cv.a[yy, xx] = c
                count += 1
    return cv


MICRO = {
    "0": ["111", "101", "101", "101", "111"], "1": ["010", "110", "010", "010", "111"],
    "2": ["111", "001", "111", "100", "111"], "3": ["111", "001", "011", "001", "111"],
    "4": ["101", "101", "111", "001", "001"], "5": ["111", "100", "111", "001", "111"],
    "6": ["111", "100", "111", "101", "111"], "7": ["111", "001", "010", "010", "010"],
    "8": ["111", "101", "111", "101", "111"], "9": ["111", "101", "111", "001", "111"],
    "M": ["10001", "11011", "10101", "10001", "10001"], "I": ["111", "010", "010", "010", "111"],
    "D": ["110", "101", "101", "101", "110"], "S": ["111", "100", "111", "001", "111"],
    "U": ["101", "101", "101", "101", "111"], "E": ["111", "100", "110", "100", "111"],
    "R": ["110", "101", "110", "101", "101"], "J": ["001", "001", "001", "101", "111"],
    "N": ["1001", "1101", "1011", "1001", "1001"], "T": ["111", "010", "010", "010", "010"],
    "W": ["10001", "10001", "10101", "11011", "10001"], "F": ["111", "100", "110", "100", "100"],
    "A": ["010", "101", "111", "101", "101"], "O": ["111", "101", "101", "101", "111"],
    "H": ["101", "101", "111", "101", "101"], " ": ["0", "0", "0", "0", "0"],
}


def micro_text(cv, x, y, s, c):
    """A tiny printed 3x5 face (calendar print)."""
    for ch in s:
        g = MICRO.get(ch, MICRO[" "])
        for gy, row in enumerate(g):
            for gx, b in enumerate(row):
                if b == "1":
                    cv.px(x + gx, y + gy, c)
        x += len(g[0]) + 1
    return x


def micro_width(s):
    return sum(len(MICRO.get(ch, MICRO[" "])[0]) + 1 for ch in s) - 1


@generator()
def calendar_page(month="JUN", mark="MIDSUMMER", day=21, seed=3):
    """A printed wall-calendar page at px=2 (160x120): header, 7x5 grid, MIDSUMMER cell.

    meta: 'mark' = the MIDSUMMER cell centre in show pixels (for the circle stroke).
    """
    cv = paper_base(160, 120, seed)
    ink_c = R("STONE", 3)
    head = P("CHERRY", 1)
    # binding holes and the header
    for x in (12, 148):
        cv.ellipse(x, 20, 2, 2, PAPER_DD)
        cv.px(x, 20, R("STONE", 4))
    name = {"JUN": "JUNE"}.get(month, month)
    big = Canvas(60, 10)
    gfont.draw_text(big, 0, 0, name, head)
    cv.blit(big, 80 - gfont.text_width(name) // 2, 19)
    # grid: 7 columns x 5 rows, cells 20x16
    gx, gy, cw, ch = 10, 30, 20, 14
    for r in range(6):
        cv.hline(gx, gx + 7 * cw, gy + r * ch if r < 5 else gy + 5 * ch, ink_c)
    for c in range(8):
        cv.vline(gx + c * cw, gy, gy + 5 * ch, ink_c)
    start = 4                      # the 1st falls on a Thursday: the 21st sits mid-grid
    for d in range(1, 31):
        idx = d - 1 + start
        col, row = idx % 7, idx // 7
        if row > 4:
            continue
        micro_text(cv, gx + col * cw + 2, gy + row * ch + 2, str(d), ink_c)
    idx = day - 1 + start
    col, row = idx % 7, idx // 7
    cx = gx + col * cw + cw // 2
    cy = gy + row * ch + ch // 2 + 2
    # the printed note for the day, overflowing its cell like real calendars do
    wtxt = micro_width(mark)
    micro_text(cv, cx - wtxt // 2, cy + 1, mark, P("GRAPE", 1))
    cv.meta["mark"] = (cx * 2, cy * 2 + 2 - 30)
    return cv


# ---------------------------------------------------------------------------
# skies, the park, the river, the bridge
# ---------------------------------------------------------------------------
def night_sky(cv, y0, y1, seed, stars_n=40, hills_y=None):
    m = np.zeros((cv.h, cv.w), bool)
    m[y0:y1, :] = True
    scribble_fill(cv, m, "navy", seed, 0.86, paper=P("COBALT", 0))
    # a lighter band near the horizon
    hz = np.zeros_like(m)
    hz[max(y0, y1 - 40):y1, :] = True
    fill(cv, hz & (lane_noise(cv.w, cv.h, seed + 9) < 0.35), "blue", seed + 2, 0.9, paper=0,
         edge_skip=False)
    rng = Rng(seed + 1)
    for _ in range(stars_n):
        x, y = rng.randint(4, cv.w - 5), rng.randint(y0 + 4, max(y0 + 5, y1 - 50))
        if rng.chance(0.3):
            star(cv, x, y, rng.randint(2, 4), "yellow", seed=x * 7 + y)
        else:
            cv.px(x, y, ink("yellow"))
            cv.px(x + 1, y, ink("yellow", 2))


@generator()
def crayon_sky(night=1, stars=40, seed=21, w=352, h=240, moon=1):
    cv = paper_base(w, h, seed)
    if night:
        night_sky(cv, 0, h, seed, int(stars))
        if moon:
            mm = blob(cv, w * 0.82, 52, 13, 13, seed + 4, 0.06)
            fill(cv, mm, "yellow", seed + 4, 0.9)
            bite = blob(cv, w * 0.82 + 7, 47, 11, 11, seed + 5, 0.05)
            cv.a[mm & bite] = ink("navy")
            outline(cv, mm & ~bite, "orange", seed=seed)
    else:
        m = np.ones((h, w), bool)
        fill(cv, m, "sky", seed, 0.55)
    return cv


def bunting_crayon(cv, x0, y0, x1, y1, sag, seed, cols=("pink", "yellow", "mint", "lavender")):
    pts = []
    n = int(abs(x1 - x0))
    for k in range(n + 1):
        t = k / n
        pts.append((x0 + (x1 - x0) * t, y0 + (y1 - y0) * t + sag * 4 * t * (1 - t)))
    line(cv, pts[::3] + [pts[-1]], "brown", w=1, seed=seed, gap=None, wobble=0.3)
    i = 0
    for k in range(6, n - 4, 11):
        x, y = pts[k]
        tri = cv.poly_mask([(x - 4, y), (x + 4, y), (x, y + 8)])
        fill(cv, tri, cols[i % len(cols)], seed + k, 0.9, edge_skip=False)
        i += 1
    return pts


def lanterns_crayon(cv, pts, seed, every=16, cols=("pink", "yellow", "mint", "orange")):
    for i, k in enumerate(range(every // 2, len(pts) - 3, every)):
        x, y = pts[k]
        m = blob(cv, x, y + 5, 3.4, 4.2, seed + k, 0.05, 12)
        fill(cv, m, cols[i % len(cols)], seed + k, 0.95, edge_skip=False)
        cv.px(x, y + 5, pal.WHITE)
        cv.px(x, y + 1, ink("brown"))


@generator()
def lantern_strings(seed=23, w=400, h=240, y=36):
    """The near plane: swags of lanterns and bunting across the top of the frame."""
    cv = Canvas(w, h)
    rng = Rng(seed)
    x = -10
    k = 0
    while x < w:
        span = rng.randint(90, 130)
        yy = y + rng.randint(-6, 6)
        pts = []
        sag = rng.randint(14, 22)
        n = span
        for j in range(n + 1):
            t = j / n
            pts.append((x + span * t, yy - 14 + sag * 4 * t * (1 - t)))
        line(cv, pts[::4] + [pts[-1]], "brown", w=1, seed=seed + k, gap=None, wobble=0.3)
        lanterns_crayon(cv, pts, seed + k * 13, every=18)
        x += span
        k += 1
    # grass tufts along the bottom edge, near and dark
    m = np.zeros((h, w), bool)
    base = h - 30
    for xx in range(w):
        top = base + int(3 * math.sin(xx * 0.11 + seed) + 2 * math.sin(xx * 0.37))
        m[top:, xx] = True
    fill(cv, m, "dkgreen", seed + 40, 0.8)
    for xx in range(2, w, 5):
        hh = 3 + (hash2(xx, 0, seed) % 5)
        line(cv, [(xx, base + 2), (xx + 1, base - hh)], "green", w=1, seed=xx, gap=None, wobble=0)
    return cv


def bandstand(cv, cx, base, scale=1.0, seed=24, lanterns=12, drained=False):
    """The round bandstand (crayon): red cone roof, heart on top, white posts, wood deck."""
    s = scale
    roof_w, roof_h = 92 * s, 40 * s
    post_h = 40 * s
    deck_h = 9 * s
    deck_w = 104 * s
    top = base - deck_h - post_h - roof_h
    roof_col = "pink" if drained else "red"
    # deck
    dm = blob(cv, cx, base - deck_h / 2, deck_w / 2, deck_h / 2 + 1, seed, 0.03, 30)
    fill(cv, dm, "brown", seed, 0.85)
    outline(cv, dm, "brown", seed=seed)
    for k in range(-2, 3):
        x = cx + k * deck_w / 5.5
        line(cv, [(x, base - deck_h), (x + 1, base)], "brown", w=1, seed=seed + k, gap=None)
    # posts
    for k in range(-3, 4):
        x = cx + k * (roof_w / 7.2)
        pm = np.zeros((cv.h, cv.w), bool)
        pm[int(base - deck_h - post_h):int(base - deck_h), int(x - 2 * s):int(x + 2 * s)] = True
        fill(cv, pm, "white", seed + k, 0.95, paper=0, edge_skip=False)
        line(cv, [(x - 2 * s, base - deck_h - post_h), (x - 2 * s, base - deck_h)], "grey", w=1,
             seed=seed + k * 3, gap=(6, 2))
    # railing
    line(cv, [(cx - deck_w / 2 + 6, base - deck_h - 12 * s), (cx + deck_w / 2 - 6, base - deck_h - 12 * s)],
         "white", w=2, seed=seed + 50, gap=None, dark=False)
    # roof: a cone with a scalloped hem
    rm = cv.poly_mask([(cx, top), (cx + roof_w / 2 + 6, top + roof_h), (cx - roof_w / 2 - 6, top + roof_h)])
    for k in range(9):
        x = cx - roof_w / 2 - 2 + k * (roof_w + 4) / 8
        rm |= blob(cv, x, top + roof_h, 6 * s, 4 * s, seed + k, 0.05, 10)
    fill(cv, rm, roof_col, seed + 60, 0.88, shade=None)
    outline(cv, rm, roof_col, seed=seed)
    # roof stripes
    for k in range(-3, 4):
        line(cv, [(cx, top + 2), (cx + k * roof_w / 7, top + roof_h)], "white" if not drained else "grey",
             w=1, seed=seed + 70 + k, gap=(5, 3), dark=False)
    # heart on the top
    hm = blob(cv, cx - 2.5 * s, top - 5 * s, 3 * s, 3 * s, seed + 80, 0.03, 10)
    hm |= blob(cv, cx + 2.5 * s, top - 5 * s, 3 * s, 3 * s, seed + 81, 0.03, 10)
    hm |= cv.poly_mask([(cx - 5.5 * s, top - 4 * s), (cx + 5.5 * s, top - 4 * s), (cx, top + 2 * s)])
    fill(cv, hm, "pink" if not drained else "grey", seed + 82, 0.95, edge_skip=False)
    # lanterns along the hem
    if lanterns:
        for k in range(int(lanterns)):
            t = (k + 0.5) / lanterns
            x = cx - roof_w / 2 + roof_w * t
            y = top + roof_h + 5 * s + 2 * math.sin(t * 3.14) * s
            m = blob(cv, x, y + 3 * s, 2.6 * s, 3.2 * s, seed + 90 + k, 0.05, 10)
            colname = ("pink", "yellow", "mint", "orange")[k % 4]
            if drained:
                colname = ("grey", "lavender")[k % 2]
            fill(cv, m, colname, seed + 90 + k, 0.95, edge_skip=False)
            line(cv, [(x, top + roof_h + 2 * s), (x, y)], "brown", w=1, seed=k, gap=None, wobble=0)
    return rm


@generator()
def bandstand_round(lanterns=12, seed=24, drained=0):
    """The bandstand as a prop (centre anchor): 128x112."""
    cv = Canvas(128, 112)
    bandstand(cv, 64, 108, 1.0, seed, int(lanterns), bool(int(drained)))
    return cv


@generator()
def storybook_park(river=1, bridge=1, seed=22, w=368, h=240, drained=0):
    """The one park (co02): hills, a river running across the middle distance, the
    bridge, grass. The bandstand and tables are props on this plane."""
    cv = Canvas(w, h)
    dr = bool(int(drained))
    rng = Rng(seed)
    # far hills
    far = np.zeros((h, w), bool)
    for x in range(w):
        top = 128 + int(8 * math.sin(x / 47.0 + seed) + 5 * math.sin(x / 19.0))
        far[top:, x] = True
    fill(cv, far, "teal" if not dr else "grey", seed, 0.62, paper=0)
    outline(cv, far, "teal" if not dr else "grey", seed=seed)
    # near meadow
    near = np.zeros((h, w), bool)
    for x in range(w):
        top = 150 + int(4 * math.sin(x / 31.0 + 2) + 2 * math.sin(x / 11.0))
        near[top:, x] = True
    fill(cv, near, "green" if not dr else "mint", seed + 1, 0.82)
    outline(cv, near, "dkgreen", seed=seed + 1)
    if int(river):
        rv = np.zeros((h, w), bool)
        for x in range(w):
            yc = 186 + int(3 * math.sin(x / 40.0))
            rv[yc - 9:yc + 9, x] = True
        fill(cv, rv, "sky" if not dr else "lavender", seed + 2, 0.85)
        for _ in range(26):
            x, y = rng.randint(0, w - 12), rng.randint(180, 192)
            line(cv, [(x, y), (x + 4, y - 1), (x + 8, y)], "white", w=1, seed=x, gap=None, dark=False)
        outline(cv, rv, "blue", seed=seed + 2)
        if int(bridge):
            # a little footbridge crossing the river towards us (wider at the bottom)
            bx = w // 2 + 126
            ytop, ybot = 170, 202
            bm = cv.poly_mask([(bx - 9, ytop), (bx + 9, ytop), (bx + 17, ybot), (bx - 17, ybot)])
            fill(cv, bm, "tan", seed + 3, 0.92)
            for y in range(ytop + 3, ybot, 4):
                t = (y - ytop) / (ybot - ytop)
                hw = 9 + 8 * t
                line(cv, [(bx - hw, y), (bx + hw, y)], "brown", w=1, seed=seed + y, gap=(6, 1), wobble=0.2)
            for side in (-1, 1):
                rail = [(bx + side * 10, ytop - 6), (bx + side * 19, ybot - 6)]
                line(cv, rail, "brown", w=2, seed=seed + 5 + side, gap=None)
                for k in range(4):
                    t = k / 3
                    x = bx + side * (10 + 9 * t)
                    y = ytop + (ybot - ytop) * t
                    line(cv, [(x, y - 6), (x, y)], "brown", w=1, seed=k + side, gap=None, wobble=0)
            outline(cv, bm, "brown", seed=seed + 3)
    # flowers and tufts
    fl = ("pink", "yellow", "white", "lavender") if not dr else ("white", "lavender", "grey")
    for _ in range(70):
        x, y = rng.randint(2, w - 3), rng.randint(160, h - 4)
        if 177 < y < 196:
            continue
        cv.px(x, y, ink(rng.choice(fl), 0))
        cv.px(x + 1, y, ink(rng.choice(fl), 0))
    return cv


@generator()
def table(cupcake_tower=0, cauldron=0, bowls=0, cupcakes=0, seed=25, drained=0):
    """A picnic table (crayon) with what is on it. Bottom-anchored prop, 56x48."""
    cv = Canvas(56, 48)
    dr = bool(int(drained))
    top = 30
    tm = cv.poly_mask([(4, top), (52, top), (54, top + 5), (2, top + 5)])
    fill(cv, tm, "brown", seed, 0.9)
    outline(cv, tm, "brown", seed=seed)
    for x in (8, 46):
        line(cv, [(x, top + 5), (x - 2, 47)], "brown", w=2, seed=seed + x, gap=None)
    if int(cupcake_tower):
        for k, (w_, y_) in enumerate(((36, top - 5), (28, top - 11), (20, top - 17), (12, top - 23))):
            m = blob(cv, 28, y_, w_ / 2, 3.2, seed + k, 0.05, 14)
            fill(cv, m, "pink" if not dr else "lavender", seed + k, 0.92, edge_skip=False)
            outline(cv, m, "rose" if not dr else "grey", seed=seed + k)
        cm = blob(cv, 28, top - 29, 3, 3, seed + 9, 0.05, 10)
        fill(cv, cm, "red" if not dr else "grey", seed + 9, 1.0, edge_skip=False)
    if int(cupcakes):
        for k in range(int(cupcakes)):
            x = 28 + (k - (int(cupcakes) - 1) / 2) * 12
            cup = cv.poly_mask([(x - 4, top - 6), (x + 4, top - 6), (x + 3, top), (x - 3, top)])
            fill(cv, cup, "tan", seed + 20 + k, 0.95, edge_skip=False)
            ic = blob(cv, x, top - 8, 4.5, 3, seed + 21 + k, 0.08, 12)
            fill(cv, ic, "pink" if not dr else "lavender", seed + 22 + k, 0.95, edge_skip=False)
            cv.px(x, top - 11, ink("red"))
    if int(cauldron):
        km = blob(cv, 28, top - 9, 13, 10, seed + 30, 0.04, 20)
        km[: top - 14, :] = False
        fill(cv, km, "black", seed + 30, 0.92, paper=0)
        rim = blob(cv, 28, top - 14, 14, 3, seed + 31, 0.03, 16)
        fill(cv, rim, "green" if not dr else "mint", seed + 31, 0.95, edge_skip=False)
        for k in range(3):
            x = 22 + k * 6
            line(cv, [(x, top - 18), (x + 2, top - 24), (x - 1, top - 30)], "grey", w=1, seed=x,
                 gap=(3, 2), dark=False)
    if int(bowls):
        for k in range(int(bowls)):
            x = 28 + (k - (int(bowls) - 1) / 2) * 16
            bm = blob(cv, x, top - 3, 6, 3.5, seed + 40 + k, 0.04, 14)
            bm[: top - 4, :] = False
            fill(cv, bm, "orange" if not dr else "grey", seed + 40 + k, 0.95, edge_skip=False)
            line(cv, [(x - 6, top - 4), (x + 6, top - 4)], "brown", w=1, seed=x, gap=None)
    return cv


@generator()
def crayon_wall(seed=40, w=32, h=240, drained=1):
    """The Wall as its own plane (co09): grey crayon bricks, straight down x = 160."""
    cv = Canvas(w, h)
    m = np.zeros((h, w), bool)
    m[:, 2:w - 2] = True
    fill(cv, m, "grey", seed, 0.9)
    for y in range(0, h, 8):
        line(cv, [(2, y), (w - 3, y)], "black", w=1, seed=seed + y, gap=(5, 2))
        off = 0 if (y // 8) % 2 == 0 else 7
        for x in range(2 + off, w - 2, 14):
            line(cv, [(x, y), (x, y + 8)], "black", w=1, seed=seed + y + x, gap=None, wobble=0.2)
    outline(cv, m, "black", w=2, seed=seed)
    return cv


@generator()
def crayon_bridge(side=1, seed=33, w=160, h=120):
    """co03 at px=2: the bridge side-on, the deck at plate y 74 (show y 118)."""
    cv = Canvas(w, h)
    deck = 74
    # far bank and trees
    bank = np.zeros((h, w), bool)
    for x in range(w):
        bank[int(56 + 3 * math.sin(x / 17.0)):, x] = True
    fill(cv, bank, "green", seed, 0.8)
    for k in range(5):
        x = 12 + k * 34 + (hash2(k, 1, seed) % 9)
        tm = blob(cv, x, 44, 9, 10, seed + k, 0.1, 16)
        fill(cv, tm, "dkgreen", seed + k, 0.85)
        line(cv, [(x, 54), (x, 60)], "brown", w=2, seed=k, gap=None)
    # arch
    am = cv.poly_mask([(0, deck), (w, deck), (w, deck + 6), (0, deck + 6)])
    xs, ys = cv.grid()
    arch = (ys >= deck + 6) & (ys < deck + 30) & (((xs - w / 2) / (w / 2 - 6)) ** 2 + ((ys - (deck + 34)) / 28.0) ** 2 > 1)
    am |= arch & (xs > 6) & (xs < w - 6)
    fill(cv, am, "tan", seed + 2, 0.9)
    outline(cv, am, "brown", seed=seed + 2)
    for x in range(4, w, 9):
        line(cv, [(x, deck), (x, deck + 6)], "brown", w=1, seed=x, gap=None, wobble=0.2)
    return cv


@generator()
def crayon_bridge_rail(seed=34, w=160, h=120):
    """The rail in front of the kids (order=front)."""
    cv = Canvas(w, h)
    deck = 74
    line(cv, [(0, deck - 12), (w, deck - 12)], "brown", w=2, seed=seed, gap=None)
    line(cv, [(0, deck - 5), (w, deck - 5)], "brown", w=1, seed=seed + 1, gap=None)
    for x in range(3, w, 12):
        line(cv, [(x, deck - 14), (x, deck)], "brown", w=2, seed=seed + x, gap=None, wobble=0.2)
    return cv


@generator()
def crayon_river(seed=32, w=160, h=120):
    """The water under the bridge (px=2), boils on twos in the set."""
    cv = Canvas(w, h)
    m = np.zeros((h, w), bool)
    m[80:, :] = True
    fill(cv, m, "sky", seed, 0.85)
    rng = Rng(seed)
    for _ in range(40):
        x, y = rng.randint(0, w - 10), rng.randint(84, h - 3)
        line(cv, [(x, y), (x + 3, y - 1), (x + 7, y)], "white", w=1, seed=x + y, gap=None, dark=False)
    line(cv, [(0, 80), (w, 80)], "blue", w=1, seed=seed, gap=(7, 2))
    return cv


@generator()
def river_storybook(seed=50, w=456, h=240):
    """co08: the river running south (right) under the night sky; the cloud drifts over it."""
    cv = Canvas(w, h)
    near = np.zeros((h, w), bool)
    for x in range(w):
        near[int(150 + 5 * math.sin(x / 29.0)):, x] = True
    fill(cv, near, "dkgreen", seed, 0.8)
    rv = np.zeros((h, w), bool)
    for x in range(w):
        yc = 176 + int(5 * math.sin(x / 50.0 + 1))
        hw = 14 + int(x / w * 6)
        rv[yc - hw:yc + hw, x] = True
    fill(cv, rv, "blue", seed + 1, 0.85)
    rng = Rng(seed)
    for _ in range(60):
        x, y = rng.randint(0, w - 10), rng.randint(165, 195)
        if rv[y, x]:
            line(cv, [(x, y), (x + 4, y - 1), (x + 8, y)], "sky", w=1, seed=x, gap=None, dark=False)
    outline(cv, rv, "navy", seed=seed + 1)
    # reeds and the far bank's trees, dark against the sky
    for k in range(14):
        x = 10 + k * 33 + (hash2(k, 3, seed) % 12)
        tm = blob(cv, x, 140, 11, 13, seed + k, 0.12, 16)
        fill(cv, tm, "teal", seed + 10 + k, 0.8)
        line(cv, [(x, 150), (x, 158)], "brown", w=2, seed=k, gap=None)
    return cv


# ---------------------------------------------------------------------------
# the banner (co10-co11), matched to the pixel banner of co12
# ---------------------------------------------------------------------------
BANNER = dict(cx=160, top=56, h=58, letters=6, lw=35, lh=42, gap=5, base=104)


def banner_letters_xy():
    b = BANNER
    total = b["letters"] * b["lw"] + (b["letters"] - 1) * b["gap"]
    x0 = b["cx"] - total // 2
    return [(x0 + k * (b["lw"] + b["gap"]), b["base"]) for k in range(b["letters"])]


def big_letter(ch, w, h):
    """A glyph of the game font stretched to w x h as a boolean mask."""
    g = gfont.glyph(ch)[:7]
    ys = (np.arange(h) * 7 // h).clip(0, 6)
    xs = (np.arange(w) * 5 // w).clip(0, 4)
    return g[ys[:, None], xs[None, :]]


@generator()
def storybook_banner(word="MINGLE", seed=60, w=320, h=240):
    """The banner cloth with its word in crayon (a plane: the GIGGLE layer reveals over it)."""
    cv = Canvas(w, h)
    b = BANNER
    y0 = b["top"] + 30
    cloth = blob(cv, w / 2, y0 + b["h"] / 2, 138, b["h"] / 2 + 2, seed, 0.02, 40)
    if word == "MINGLE":
        fill(cv, cloth, "white", seed, 0.95, paper=PAPER)
        outline(cv, cloth, "grey", seed=seed)
        # the ropes to the eaves
        line(cv, [(w / 2 - 136, y0 + 4), (w / 2 - 150, y0 - 30)], "brown", w=1, seed=seed + 1, gap=None)
        line(cv, [(w / 2 + 136, y0 + 4), (w / 2 + 150, y0 - 30)], "brown", w=1, seed=seed + 2, gap=None)
    col = "rose" if word == "MINGLE" else "pink"
    for k, (x, base) in enumerate(banner_letters_xy()):
        ch = word[k] if k < len(word) else " "
        lh = b["lh"]
        m = np.zeros((h, w), bool)
        lm = big_letter(ch, b["lw"], lh)
        m[base + 30 - lh:base + 30, x:x + b["lw"]] = lm
        if word == "MINGLE":
            fill(cv, m, col, seed + k, 0.9, edge_skip=True)
            outline(cv, m, col, seed=seed + k)
        else:
            # wet roller paint: a broad pink band behind the new letter, letter in deep pink
            band = np.zeros((h, w), bool)
            band[base + 30 - lh - 3:base + 33, x - 3:x + b["lw"] + 3] = True
            band &= cloth
            fill(cv, band, "pink", seed + 20 + k, 0.97, paper=P("BUBBLEGUM", 2), edge_skip=False)
            fill(cv, m, "rose", seed + 30 + k, 0.95, paper=0, edge_skip=False)
    return cv


@generator()
def storybook_bandstand_north(seed=61, w=352, h=240):
    """co10/co11 backing: the north half of the bandstand close up (eave, posts, lanterns)."""
    cv = Canvas(w, h)
    night_sky(cv, 0, h, seed, 18)
    # eave
    eave = np.zeros((h, w), bool)
    eave[30:58, :] = True
    fill(cv, eave, "red", seed, 0.9)
    for x in range(0, w, 16):
        sc = blob(cv, x + 8, 58, 8, 5, seed + x, 0.05, 12)
        fill(cv, sc, "red", seed + x, 0.92, edge_skip=False)
        outline(cv, sc, "red", seed=seed + x)
    # posts
    for x in (20, 120, 232, 332):
        pm = np.zeros((h, w), bool)
        pm[60:220, x - 5:x + 5] = True
        fill(cv, pm, "white", seed + x, 0.92)
        outline(cv, pm, "grey", seed=seed + x)
    # deck rail and floor
    fl = np.zeros((h, w), bool)
    fl[196:, :] = True
    fill(cv, fl, "brown", seed + 5, 0.88)
    line(cv, [(0, 176), (w, 176)], "white", w=3, seed=seed + 6, gap=None, dark=False)
    for x in range(8, w, 14):
        line(cv, [(x, 176), (x, 196)], "white", w=2, seed=seed + x, gap=None, dark=False)
    for k in range(10):
        x = 10 + k * 36
        m = blob(cv, x, 72, 4, 5, seed + 70 + k, 0.05, 12)
        fill(cv, m, ("pink", "yellow", "mint", "orange")[k % 4], seed + k, 0.95, edge_skip=False)
        line(cv, [(x, 58), (x, 67)], "brown", w=1, seed=k, gap=None)
    return cv


# ---------------------------------------------------------------------------
# inserts: the hand and the string (co07/co17), the soup table (co05)
# ---------------------------------------------------------------------------
@generator()
def crayon_hand(pose="hold", seed=70):
    """The young cherub's small hand from the side (no face anywhere): 'hold' with the
    string loop round two fingers, 'open' with the loop sliding off. 96x80, bottom anchor."""
    cv = Canvas(96, 80)
    palm = blob(cv, 46, 58, 17, 13, seed, 0.05, 24)
    arm = cv.poly_mask([(30, 66), (52, 62), (60, 80), (26, 80)])
    sleeve = cv.poly_mask([(24, 72), (62, 70), (64, 80), (22, 80)])
    fingers = np.zeros_like(palm)
    if pose == "hold":
        for k in range(3):
            fingers |= blob(cv, 52 + k * 5, 44 - k, 3.2, 8, seed + k, 0.05, 12)
        fingers |= blob(cv, 36, 50, 7, 3.4, seed + 9, 0.05, 12)
    else:
        for k in range(3):
            fingers |= blob(cv, 54 + k * 6, 38 - k * 2, 3.0, 10, seed + k, 0.05, 12)
        fingers |= blob(cv, 34, 44, 8, 3.4, seed + 9, 0.05, 12)
    hand = palm | fingers | arm
    fill(cv, hand, "orange", seed, 0.6, paper=P("PEACH", 2))
    outline(cv, hand, "orange", seed=seed)
    fill(cv, sleeve, "sky", seed + 5, 0.9)
    outline(cv, sleeve, "blue", seed=seed + 5)
    if pose == "hold":
        line(cv, [(58, 40), (64, 30), (70, 16), (74, 0)], "black", w=1, seed=seed + 20, gap=None)
        loop = [(52, 42), (56, 38), (62, 39), (64, 44), (60, 48), (54, 47), (52, 42)]
        line(cv, loop, "black", w=1, seed=seed + 21, gap=None, wobble=0.2)
    else:
        line(cv, [(72, 26), (80, 14), (86, 0)], "black", w=1, seed=seed + 22, gap=None)
        loop = [(68, 30), (73, 25), (79, 27), (80, 33), (75, 36), (70, 35), (68, 30)]
        line(cv, loop, "black", w=1, seed=seed + 23, gap=None, wobble=0.2)
    return cv


@generator()
def crayon_soup_table(seed=80, w=160, h=120):
    """co05 top-down at px=2: wooden table boards, the cauldron's rim at the edge."""
    cv = paper_base(w, h, seed)
    for k in range(7):
        y = 6 + k * 16
        m = np.zeros((h, w), bool)
        m[y:y + 15, 4:w - 4] = True
        fill(cv, m, "tan" if k % 2 else "brown", seed + k, 0.86)
        line(cv, [(4, y + 15), (w - 4, y + 15)], "brown", w=1, seed=seed + k, gap=(8, 2))
    rim = blob(cv, w - 18, 20, 30, 26, seed + 30, 0.03, 30)
    fill(cv, rim, "black", seed + 30, 0.9)
    soup = blob(cv, w - 18, 20, 24, 20, seed + 31, 0.04, 24)
    fill(cv, soup, "green", seed + 31, 0.92)
    return cv


@generator()
def crayon_bowl(seed=81, full=1):
    """A soup bowl from above (24x20), for co05's two-bowl set-down."""
    cv = Canvas(24, 20)
    m = blob(cv, 12, 10, 10.5, 8.5, seed, 0.04, 20)
    fill(cv, m, "orange", seed, 0.95, edge_skip=False)
    outline(cv, m, "orange", seed=seed)
    if int(full):
        s = blob(cv, 12, 10, 7, 5.5, seed + 1, 0.05, 16)
        fill(cv, s, "green", seed + 1, 0.95, edge_skip=False)
        cv.px(10, 9, ink("white"))
        cv.px(14, 11, ink("white"))
    return cv


# ---------------------------------------------------------------------------
# path generators for STROKES planes (show pixels)
# ---------------------------------------------------------------------------
@generator(kind="path")
def ellipse(cx=160, cy=90, rx=40, ry=20, turns=1.15, wobble=1, seed=11, n=0):
    """A hand-drawn circling stroke that overshoots its start (turns > 1)."""
    rng = Rng(seed)
    n = int(n) or int(max(24, (rx + ry) * 1.6))
    total = int(n * float(turns))
    pts = []
    a0 = -2.3
    for k in range(total + 1):
        a = a0 + k / n * 6.283
        drift = k / max(1, total) * 2.5
        r1 = rx + rng.uniform(-wobble, wobble) + drift
        r2 = ry + rng.uniform(-wobble, wobble) * 0.6 + drift * 0.4
        pts.append((cx + math.cos(a) * r1, cy + math.sin(a) * r2))
    return pts


@generator(kind="path")
def scribble_bursts(cx=160, cy=90, n=5, r=(28, 52), seed=12, rays=7):
    """Firework scribbles round a point, outside the lines: n bursts of rays."""
    rng = Rng(seed)
    polys = []
    lo, hi = (r if isinstance(r, tuple) else (int(r), int(r)))
    for k in range(int(n)):
        a = rng.uniform(0, 6.283)
        d = rng.uniform(lo, hi)
        bx, by = cx + math.cos(a) * d * 1.6, cy + math.sin(a) * d * 0.9
        for j in range(int(rays)):
            aa = j / rays * 6.283 + rng.uniform(-0.2, 0.2)
            ln = rng.uniform(5, 10)
            polys.append([(bx + math.cos(aa) * 2, by + math.sin(aa) * 2),
                          (bx + math.cos(aa) * ln, by + math.sin(aa) * ln)])
    return polys


@generator(kind="path")
def poly(pts=""):
    v = [float(t) for t in str(pts).split(";") if t] if ";" in str(pts) else []
    return [(v[i], v[i + 1]) for i in range(0, len(v) - 1, 2)]


# ---------------------------------------------------------------------------
# end credits: storybook stills of the night (px=2, 160x120 each)
# ---------------------------------------------------------------------------
@generator()
def credits_still(n=1, seed=90):
    """CR-1: crayon stills, one per 4 s. 1 banner, 2 bubbles, 3 the Big Puff bursting
    into hearts, 4 three candles, 5 a pink heart over the Wall, 6 a purple firework."""
    n = int(n)
    cv = paper_base(160, 120, seed + n)
    sky = np.zeros((120, 160), bool)
    sky[:84, :] = True
    ground = ~sky
    if n in (1, 2, 3):
        fill(cv, sky, "navy", seed + n, 0.7)
    else:
        scribble_fill(cv, sky, "navy", seed + n, 0.8)
    fill(cv, ground, "dkgreen", seed + 50 + n, 0.75)
    rng = Rng(seed + n)
    for _ in range(14):
        cv.px(rng.randint(2, 157), rng.randint(2, 60), ink("yellow"))
    if n == 1:
        cl = blob(cv, 80, 44, 62, 16, seed, 0.03, 30)
        fill(cv, cl, "white", seed, 0.95, paper=PAPER)
        outline(cv, cl, "grey", seed=seed)
        for k, ch in enumerate("GIGGLE"):
            m = np.zeros((120, 160), bool)
            m[34:54, 28 + k * 18:28 + k * 18 + 14] = big_letter(ch, 14, 20)
            fill(cv, m, "pink", seed + k, 0.95, paper=0, edge_skip=False)
        line(cv, [(70, 56), (70, 66)], "red", w=2, seed=seed, gap=None)
        for k in range(6):
            lm = blob(cv, 22 + k * 23, 72, 3, 4, seed + k, 0.05, 10)
            fill(cv, lm, ("pink", "yellow", "mint")[k % 3], seed + k, 0.95, edge_skip=False)
    elif n == 2:
        for k in range(7):
            x, y, r = 24 + k * 19 + rng.randint(-4, 4), 24 + rng.randint(0, 50), rng.randint(6, 12)
            m = blob(cv, x, y, r, r, seed + k, 0.04, 20)
            inner = blob(cv, x, y, r - 2, r - 2, seed + k, 0.04, 20)
            fill(cv, m & ~inner, "sky", seed + k, 0.95, edge_skip=False)
            cv.px(x - r // 2, y - r // 2, pal.WHITE)
            cv.px(x - r // 2 + 1, y - r // 2, pal.WHITE)
        wand = [(40, 110), (60, 90)]
        line(cv, wand, "yellow", w=2, seed=seed, gap=None)
        star(cv, 62, 88, 5, "pink", seed=seed)
    elif n == 3:
        m = blob(cv, 80, 52, 34, 26, seed, 0.12, 30)
        fill(cv, m, "purple", seed, 0.6)
        outline(cv, m, "purple", seed=seed)
        for k in range(12):
            a = k / 12 * 6.283
            x, y = 80 + math.cos(a) * 46, 52 + math.sin(a) * 34
            hm = blob(cv, x - 2, y, 2.5, 2.5, seed + k, 0.03, 8) | blob(cv, x + 2, y, 2.5, 2.5, seed + k, 0.03, 8)
            hm |= cv.poly_mask([(x - 4.5, y + 1), (x + 4.5, y + 1), (x, y + 6)])
            fill(cv, hm, "pink", seed + k, 1.0, paper=0, edge_skip=False)
        tent = cv.poly_mask([(40, 112), (80, 76), (120, 112)])
        fill(cv, tent, "pink", seed + 30, 0.8)
    elif n == 4:
        rail = np.zeros((120, 160), bool)
        rail[78:84, 10:150] = True
        fill(cv, rail, "white", seed, 0.95)
        for k, x in enumerate((48, 80, 112)):
            cm = np.zeros((120, 160), bool)
            cm[56:78, x - 4:x + 4] = True
            fill(cv, cm, "white", seed + k, 0.95, edge_skip=False)
            outline(cv, cm, "grey", seed=seed + k)
            if k == 2:
                fl = blob(cv, x, 50, 3, 5, seed + k, 0.05, 10)
                fill(cv, fl, "yellow", seed + k, 1.0, paper=0, edge_skip=False)
                cv.px(x, 49, ink("orange"))
            else:
                line(cv, [(x, 54), (x + 1, 46), (x - 1, 40)], "grey", w=1, seed=x, gap=(2, 2), dark=False)
    elif n == 5:
        wm = np.zeros((120, 160), bool)
        wm[30:, 72:88] = True
        fill(cv, wm, "grey", seed, 0.9)
        for y in range(30, 120, 6):
            line(cv, [(72, y), (88, y)], "black", w=1, seed=y, gap=(4, 2))
        hm = blob(cv, 73, 22, 9, 9, seed, 0.04, 16) | blob(cv, 87, 22, 9, 9, seed + 1, 0.04, 16)
        hm |= cv.poly_mask([(63, 25), (97, 25), (80, 44)])
        fill(cv, hm, "pink", seed + 2, 0.9)
        outline(cv, hm, "rose", seed=seed + 2)
    else:
        wm = np.zeros((120, 160), bool)
        wm[46:, 70:86] = True
        fill(cv, wm, "grey", seed, 0.9)
        for k in range(10):
            a = k / 10 * 6.283
            line(cv, [(118 + math.cos(a) * 4, 34 + math.sin(a) * 4),
                      (118 + math.cos(a) * 16, 34 + math.sin(a) * 16)], "purple", w=2, seed=k, gap=None,
                 dark=False)
        line(cv, [(118, 112), (117, 80), (118, 52)], "purple", w=1, seed=seed, gap=(3, 2), dark=False)
    return cv
