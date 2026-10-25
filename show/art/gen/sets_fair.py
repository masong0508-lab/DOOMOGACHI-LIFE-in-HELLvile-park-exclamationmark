"""sets_fair.py - the Midsummer Giggle: a pastel fun fair under lantern strings.

Figure scale (feet on the fairground at show y ~150-170; a stall-holder behind a
counter shows from the counter top at show y 118 up). Every stall set shares
the fair's far plane (fair_far, par 0.5) and its ground (fair_ground) and puts
its own stall on a 352-px plane at par 1.0 (room for a +-16 px drift).
"""
import math

import numpy as np

from . import generator, pal
from .canvas import Canvas, Rng, hash2, bayer
from . import kit
from .kit import R, P, D, L, Y

GROUND = Y(146)             # the fairground's far edge (stall bases sit here)
COUNTER = Y(118)            # counter tops


@generator()
def fair_far(seed=200, w=480, clock=0):
    """The fair beyond: tent tops, a ferris wheel, the helter-skelter, trees, bunting,
    and (clock=1) the town clock tower on its hill."""
    cv = Canvas(int(w), 240)
    w = int(w)
    base = Y(112)
    kit.hills(cv, base - 6, 10, 220, R("GRASS", 4), seed, top=R("GRASS", 5), bottom=240)
    for k in range(9):
        x = 14 + k * 54 + hash2(k, 4, seed) % 18
        kit.tree_round(cv, x, base + 4, 9 + hash2(k, 5, seed) % 5, leaf=R("GRASS", 4), seed=seed + k)
    # ferris wheel
    fx, fy, fr = int(w * 0.72), base - 58, 30
    for k in range(12):
        a = k / 12 * 6.283
        cv.line(fx, fy, fx + math.cos(a) * fr, fy + math.sin(a) * fr, R("STONE", 6))
    for k in range(96):
        a = k / 96 * 6.283
        cv.px(fx + math.cos(a) * fr, fy + math.sin(a) * fr, R("PASTEL_PINK", 6))
    for k in range(8):
        a = k / 8 * 6.283 + 0.2
        x, y = fx + math.cos(a) * fr, fy + math.sin(a) * fr
        cv.rect(x - 2, y, 5, 4, [P("BUBBLEGUM", 2), P("LEMON", 2), P("MINT", 2), P("LAVENDER", 2)][k % 4])
    cv.line(fx, fy, fx - 14, base + 4, R("STONE", 5))
    cv.line(fx, fy, fx + 14, base + 4, R("STONE", 5))
    # helter-skelter
    hx = int(w * 0.30)
    cv.rect(hx - 8, base - 70, 16, 74, pal.CREAM)
    for y in range(base - 70, base + 4, 8):
        cv.line(hx - 8, y + 6, hx + 8, y, P("BUBBLEGUM", 1), 2)
    cv.poly([(hx - 11, base - 70), (hx + 11, base - 70), (hx, base - 88)], P("MINT", 1))
    cv.vline(hx, base - 95, base - 88, R("WOOD", 2))
    cv.poly([(hx + 1, base - 95), (hx + 6, base - 93), (hx + 1, base - 91)], P("CHERRY", 1))
    # distant tent tops
    for k, (x, c) in enumerate(((int(w * 0.12), P("LEMON", 2)), (int(w * 0.52), P("MINT", 2)),
                                (int(w * 0.90), P("BUBBLEGUM", 2)))):
        kit.tent(cv, x, base + 6, 46, 42, [c, pal.CREAM], door=False, flag=c)
    if int(clock):
        tx = int(w * 0.5)
        cv.rect(tx - 7, base - 96, 14, 60, R("PEACH", 6))
        cv.rect(tx - 7, base - 96, 2, 60, R("PEACH", 7))
        cv.poly([(tx - 10, base - 96), (tx + 10, base - 96), (tx, base - 114)], P("BUBBLEGUM", 1))
        cv.ellipse(tx, base - 82, 5.5, 5.5, pal.CREAM)
        cv.a[cv.outline_mask(cv.ellipse_mask(tx, base - 82, 5.5, 5.5))] = R("STONE", 2)
    return cv


@generator()
def fair_ground(seed=201, w=352, path=1):
    """The fairground: a trodden peach path through the grass, lantern-light flecks."""
    cv = Canvas(int(w), 240)
    w = int(w)
    kit.grass(cv, 0, GROUND, w, 240 - GROUND, seed, ramp_lo=3, ramp_hi=5)
    if int(path):
        top = GROUND + 10
        m = np.zeros((240, w), bool)
        for x in range(w):
            y0 = top + int(2 * math.sin(x / 37.0 + seed))
            m[y0:y0 + 36 + int(3 * math.sin(x / 23.0)), x] = True
        b = bayer(w, 240)
        cv.a[m] = R("PEACH", 5)
        cv.a[m & (b < 4)] = R("PEACH", 4)
        cv.a[cv.inner_edge(m, "top")] = R("PEACH", 6)
        cv.a[cv.inner_edge(m, "bottom")] = R("PEACH", 3)
        rng = Rng(seed)
        for _ in range(w // 6):
            x, y = rng.randint(0, w - 1), rng.randint(top + 2, top + 34)
            if m[y, x]:
                cv.px(x, y, R("PEACH", 3) if rng.chance(0.5) else R("PEACH", 6))
        # confetti on the path
        for _ in range(w // 10):
            x, y = rng.randint(0, w - 1), rng.randint(top + 2, top + 34)
            if m[y, x]:
                cv.px(x, y, rng.choice([P("BUBBLEGUM", 2), P("LEMON", 2), P("MINT", 2), P("LAVENDER", 2)]))
    cv.rect(0, GROUND, w, 1, R("GRASS", 3))
    return cv


@generator()
def fair_lanterns(seed=202, w=400, y=14, lit=1):
    """The near plane: swags of lantern string and bunting across the top of frame
    (lanterns baked here; the per-lantern props live in sets that need them to change)."""
    cv = Canvas(int(w), 240)
    w = int(w)
    rng = Rng(seed)
    x = -20
    k = 0
    while x < w:
        span = rng.randint(110, 150)
        y0 = Y(y) + rng.randint(-4, 4)
        pts = kit.catenary(x, y0, x + span, y0 + rng.randint(-4, 4), rng.randint(12, 20))
        kit.draw_string(cv, pts, R("WOOD", 2))
        for i in range(10, len(pts) - 6, 22):
            px, py = pts[i]
            lan = kit.lantern_img(kit.LANTERN_COLS[(k + i // 22) % 5], "lit" if int(lit) else "out")
            cv.blit(lan, px - lan.w // 2, py)
        x += span
        k += 1
    kit.bunting(cv, -10, Y(y) - 10, w + 10, Y(y) - 8, 10, [P("BUBBLEGUM", 2), P("LEMON", 2), P("MINT", 2),
                                                         P("LAVENDER", 2)], every=11, size=6)
    return cv


def _stall_canvas(w=352):
    return Canvas(w, 240)


@generator()
def stall_hook_a_duck(seed=210):
    """1-04: Steamy's Hook-a-Duck: a round paddling pool on a booth, the ducks are props."""
    cv = _stall_canvas()
    box = kit.booth(cv, 96, GROUND + 2, 160, 104, [P("SKY", 2), pal.WHITE], seed=seed,
                    back=R("SKY", 4), counter=R("WOOD", 5))
    # the pool in front of the counter: a round tub with water
    px0, pw = 70, 212
    py = COUNTER + 4
    tub = cv.ellipse_mask(px0 + pw / 2, py + 14, pw / 2, 16)
    cv.shape(tub, P("BUBBLEGUM", 2), outline=P("ROSE", 0), hi=pal.WHITE)
    water = cv.ellipse_mask(px0 + pw / 2, py + 10, pw / 2 - 6, 9)
    cv.a[water] = R("SKY", 5)
    xs, ys = cv.grid()
    cv.a[water & (((xs // 3 + ys) % 7) == 0)] = R("SKY", 7)
    cv.a[cv.inner_edge(water, "top")] = R("SKY", 4)
    # prize shelf in the back: plushies as soft shapes
    for k in range(6):
        x = box[0] + 10 + k * 24
        kit.heart(cv, x + 6, box[1] + 14, 5, [P("BUBBLEGUM", 2), P("LEMON", 2), P("MINT", 2)][k % 3], outline=R("STONE", 3))
    cv.rect(box[0], box[1] + 22, box[2], 2, R("WOOD", 3))
    # hook rods leaning on the post
    for k in range(3):
        cv.line(92 + k * 3, GROUND - 4, 86 + k * 5, COUNTER - 52, R("WOOD", 4))
        cv.px(86 + k * 5, COUNTER - 53, R("STEEL", 5))
    cv.meta["sign"] = (176, Y(14))
    return cv


@generator()
def duck(face="front", seed=1):
    """A rubber duck (12x11); face=wall turns it to look right (toward the Wall)."""
    cv = Canvas(13, 12)
    body = cv.ellipse_mask(6, 8, 6, 3.6)
    head = cv.ellipse_mask(4 if face == "front" else 8, 4, 3.2, 3.2)
    cv.a[body | head] = P("LEMON", 1)
    cv.a[cv.inner_edge(body | head, "top")] = P("LEMON", 2)
    bx = 1 if face == "front" else 11
    cv.rect(bx, 4, 2, 2, P("TANGERINE", 1))
    cv.px(4 if face == "front" else 8, 3, R("STONE", 0))
    cv.a[cv.outline_mask(body | head)] = P("HONEY", 0)
    return cv


@generator()
def stall_face_painting(seed=220):
    """1-05: Kevin's face painting: an easel with face designs, a stool, paint pots,
    a mirror; the ANY FACE (within reason) sign is a string on the sign prop."""
    cv = _stall_canvas()
    box = kit.booth(cv, 80, GROUND + 2, 190, 108, [P("LEMON", 2), pal.WHITE], seed=seed,
                    back=R("CANDY", 6), counter=R("WOOD", 5))
    # design board in the back: a grid of painted faces (circles with marks)
    gx, gy = box[0] + 8, box[1] + 4
    cv.rect(gx - 2, gy - 2, 112, 34, R("WOOD", 3))
    cv.rect(gx, gy, 108, 30, pal.CREAM)
    cols = [P("BUBBLEGUM", 1), P("SKY", 1), P("LIME", 1), P("TANGERINE", 1), P("LAVENDER", 1), P("CHERRY", 1)]
    for k in range(6):
        cx, cy = gx + 9 + k * 18, gy + 15
        cv.ellipse(cx, cy, 6.5, 7.5, R("PEACH", 6))
        cv.a[cv.outline_mask(cv.ellipse_mask(cx, cy, 6.5, 7.5))] = R("PEACH", 3)
        cv.rect(cx - 4, cy - 3, 3, 2, cols[k])
        cv.rect(cx + 1, cy - 3, 3, 2, cols[k])
        if k == 2:
            cv.rect(cx - 1, cy - 6, 2, 2, cols[k])       # three eyes: a design for Kevin's crowd
        cv.rect(cx - 2, cy + 3, 4, 1, cols[(k + 2) % 6])
    # paint pots on the counter
    for k in range(5):
        x = box[0] + 128 + k * 9
        cv.rect(x, COUNTER - 8, 7, 8, cols[k])
        cv.rect(x, COUNTER - 8, 7, 1, L(cols[k], 1))
        cv.rect(x + 1, COUNTER - 10, 1, 3, R("WOOD", 4))
    # a mirror on a stand at the left
    m = cv.ellipse_mask(56, COUNTER - 30, 12, 16)
    cv.shape(m, R("SKY", 6), outline=P("HONEY", 0))
    cv.a[m & (cv.grid()[0] - 52 + (cv.grid()[1] - (COUNTER - 30)) // 2 == 0)] = pal.WHITE
    cv.rect(55, COUNTER - 14, 3, GROUND - COUNTER + 16, R("WOOD", 3))
    # stool
    cv.ellipse(222, GROUND - 22, 14, 4, P("BUBBLEGUM", 1))
    for x in (212, 230):
        cv.rect(x, GROUND - 20, 3, 22, R("WOOD", 3))
    return cv


@generator()
def stall_hattie_tent(seed=230):
    """1-06: Hattie Haha's tent from inside the flap: a crate stage, a striped back wall,
    a mic stand, rows of the tent crowd's benches in front (the crowd are actors)."""
    cv = _stall_canvas()
    # tent interior back wall: big stripes
    for x in range(0, 352):
        c = P("BUBBLEGUM", 2) if (x // 22) % 2 == 0 else pal.CREAM
        cv.rect(x, Y(-30), 1, GROUND - Y(-30), c)
    # the roof canvas sloping in from the top corners
    m = cv.poly_mask([(0, 0), (352, 0), (352, Y(30)), (176, Y(-4)), (0, Y(30))])
    cv.a[m] = P("BUBBLEGUM", 1)
    xs, ys = cv.grid()
    cv.a[m & (((xs - 176) // 22) % 2 == 0)] = R("PASTEL_PINK", 5)
    cv.a[cv.outline_mask(m) & (ys > 0)] = P("ROSE", 0)
    # scallop valance
    for x in range(0, 352, 16):
        cv.ellipse(x + 8, Y(28) - abs(x + 8 - 176) * 0.19, 8, 4, P("BUBBLEGUM", 1))
    # string lights across
    pts = kit.catenary(0, Y(40), 352, Y(40), 16)
    kit.draw_string(cv, pts, R("WOOD", 2))
    for i in range(8, len(pts), 16):
        cv.ellipse(pts[i][0], pts[i][1] + 3, 2, 2, R("CANDY", 7))
    # the crate stage (Hattie stands on it) and a spotlight pool on the back wall
    kit.glow_pool(cv, 176, Y(80), 46, 40, R("CANDY", 7), levels=(8, 4, 2))
    cv.rect(140, GROUND - 26, 72, 28, R("WOOD", 4))
    kit.planks(cv, 140, GROUND - 26, 72, 28, seed, base=R("WOOD", 4), pw=7)
    cv.frame(140, GROUND - 26, 72, 28, R("WOOD", 1))
    # mic stand
    cv.rect(212, GROUND - 70, 2, 70, R("STEEL", 3))
    cv.ellipse(213, GROUND - 72, 3, 3, R("STEEL", 2))
    cv.rect(208, GROUND - 2, 10, 2, R("STEEL", 2))
    # the floor (boards) and benches at the front edge
    kit.planks(cv, 0, GROUND, 352, 240 - GROUND, seed + 1, base=R("WOOD", 3), pw=8)
    for y in (Y(166), Y(186)):
        cv.rect(0, y, 352, 5, R("WOOD", 5))
        cv.rect(0, y, 352, 1, R("WOOD", 6))
        cv.rect(0, y + 5, 352, 2, R("WOOD", 1))
    return cv


@generator()
def lulla_tent(seed=240, drawings=11):
    """1-07: Miss Lulla's craft tent, inside: a string of children's drawings (every one the
    same crowned purple cloud, never with a string), craft tables, and the flap open on
    the right onto the night fair and the Wall (the Cloud Nine Vapor COMING SOON sign is a
    prop there). 480 wide: the drawings pan at constant speed."""
    w = 480
    cv = Canvas(w, 240)
    for x in range(w):
        c = P("LAVENDER", 2) if (x // 24) % 2 == 0 else pal.CREAM
        cv.rect(x, 0, 1, GROUND, c)
    kit.planks(cv, 0, GROUND, w, 240 - GROUND, seed, base=R("WOOD", 4), pw=8)
    # the flap opening (transparent: the fair-at-dusk far plane shows through)
    fx0, fx1 = 382, 452
    m = cv.poly_mask([(fx0, GROUND), (fx0 + 8, Y(30)), (fx1 - 10, Y(26)), (fx1, GROUND)])
    cv.a[m] = 0
    cv.a[cv.outline_mask(m) & (cv.grid()[1] < GROUND)] = P("GRAPE", 0)
    cv.poly([(fx0 - 6, GROUND), (fx0 + 6, Y(30)), (fx0 + 12, Y(36)), (fx0 + 4, GROUND)], P("LAVENDER", 1))
    # the string of drawings
    n = int(drawings)
    pts = kit.catenary(10, Y(34), 370, Y(34), 10)
    kit.draw_string(cv, pts, R("STONE", 3))
    for k in range(n):
        i = 14 + k * 31
        px, py = pts[min(i, len(pts) - 1)]
        dx0, dy0 = int(px) - 11, int(py) + 1
        cv.rect(dx0, dy0, 22, 26, pal.WHITE)
        cv.frame(dx0, dy0, 22, 26, R("STONE", 5))
        cv.rect(int(px) - 1, int(py) - 1, 3, 3, P("CHERRY", 1) if k % 2 else P("SKY", 1))     # pegs
        # a crowned purple cloud in a child's hand (slightly different each time)
        cx, cy = dx0 + 11 + (k % 3) - 1, dy0 + 15 + (k % 2)
        cl = cv.ellipse_mask(cx - 4, cy, 4, 3.5) | cv.ellipse_mask(cx + 4, cy, 4, 3.5) | cv.ellipse_mask(cx, cy - 3, 5, 4)
        cv.a[cl] = P("GRAPE", 1) if k % 3 else P("VAPE_PURPLE", 1)
        cv.px(cx - 2, cy - 1, pal.WHITE)
        cv.px(cx + 2, cy - 1, pal.WHITE)
        crown = [(cx - 4, cy - 7), (cx - 4, cy - 10), (cx - 2, cy - 8), (cx, cy - 11), (cx + 2, cy - 8),
                 (cx + 4, cy - 10), (cx + 4, cy - 7)]
        cv.poly(crown, P("LEMON", 1))
    # craft tables with paper, glue and crayons
    for (x0, x1) in ((30, 170), (200, 340)):
        cv.rect(x0, COUNTER + 6, x1 - x0, 5, R("WOOD", 5))
        cv.rect(x0, COUNTER + 11, x1 - x0, 2, R("WOOD", 2))
        for x in (x0 + 4, x1 - 7):
            cv.rect(x, COUNTER + 13, 3, GROUND - COUNTER - 13, R("WOOD", 3))
        rng = Rng(seed + x0)
        for k in range(6):
            x = rng.randint(x0 + 6, x1 - 16)
            cv.rect(x, COUNTER + 3, 12, 3, pal.WHITE)
            cv.rect(x + 13, COUNTER + 4, 4, 2, rng.choice([P("CHERRY", 1), P("SKY", 1), P("LIME", 1), P("LEMON", 1)]))
    return cv


@generator()
def bake_tent(seed=250, puff=0):
    """1-09 / 3-02: the bake tent from the front: a big peach-and-cream striped marquee,
    its front open on a lit counter of bakes; Nana's stall has two of everything (the pies
    and muffins are props). The BAKE TENT sign (BAKE TEN after 3-05) is a string on the
    sign prop over the entrance."""
    cv = _stall_canvas()
    cx = 176
    eave = Y(40)
    top = Y(-10)
    x0, x1 = 26, 326
    # the marquee roof: a ridge tent seen from the front (two slopes up to the ridge)
    roof = cv.poly_mask([(x0 - 14, eave), (cx - 60, top), (cx + 60, top), (x1 + 14, eave)])
    xs, ys = cv.grid()
    stripe = (((xs - cx) * 64 // np.maximum(8, ys - top + 40)) // 6) % 2 == 0
    cv.a[roof] = pal.CREAM
    cv.a[roof & stripe] = P("PEACH", 1)
    cv.a[cv.inner_edge(roof, "top")] = P("PEACH", 2)
    cv.a[cv.outline_mask(roof)] = P("PEACH", 0)
    # pennants on the ridge
    for x in range(cx - 56, cx + 60, 14):
        cv.poly([(x, top), (x + 8, top), (x + 4, top - 7)], [P("BUBBLEGUM", 1), P("LEMON", 1), P("MINT", 1)][(x // 14) % 3])
    # valance
    for x in range(x0 - 14, x1 + 14, 12):
        c = P("PEACH", 1) if ((x - x0) // 12) % 2 == 0 else pal.CREAM
        cv.rect(x, eave, 12, 6, c)
        cv.ellipse(x + 6, eave + 6, 6, 4, c)
        cv.px(x + 6, eave + 9, P("PEACH", 0))
    # the inside: warm and lit (emissive cream at the back so it stays lit at night)
    cv.rect(x0, eave + 6, x1 - x0, COUNTER - eave - 6, R("PEACH", 6))
    cv.rect(x0, eave + 6, x1 - x0, 3, R("PEACH", 4))
    for x in (x0, x1 - 6):
        cv.rect(x, eave, 6, GROUND - eave, pal.WHITE)
        cv.rect(x + 5 if x == x0 else x, eave, 1, GROUND - eave, R("STONE", 5))
    # strings of bulbs inside
    pts = kit.catenary(x0 + 6, eave + 12, x1 - 6, eave + 12, 8)
    kit.draw_string(cv, pts, R("WOOD", 2))
    for i in range(6, len(pts), 14):
        cv.px(pts[i][0], pts[i][1] + 1, R("CANDY", 7))
        cv.px(pts[i][0], pts[i][1] + 2, R("CANDY", 7))
    # shelves of bakes on the back wall
    for row, y in enumerate((eave + 18, eave + 40)):
        cv.rect(x0 + 10, y + 12, x1 - x0 - 20, 3, R("WOOD", 4))
        cv.rect(x0 + 10, y + 15, x1 - x0 - 20, 1, R("WOOD", 2))
        rng = Rng(seed + row)
        for x in range(x0 + 16, x1 - 24, 18):
            kind = rng.randint(0, 2)
            if kind == 0:
                cv.ellipse(x + 6, y + 9, 7, 4, P("CARAMEL", 1))
                cv.rect(x - 1, y + 9, 14, 3, P("CARAMEL", 0))
            elif kind == 1:
                cv.rect(x + 1, y + 5, 10, 7, P("BUBBLEGUM", 2))
                cv.rect(x + 1, y + 3, 10, 2, pal.WHITE)
                cv.px(x + 6, y + 2, P("CHERRY", 1))
            else:
                cv.ellipse(x + 6, y + 8, 5, 4, R("CANDY", 5))
                cv.ellipse(x + 6, y + 6, 4, 2, R("CANDY", 7))
    # the long counter with a gingham cloth
    cy0, cy1 = COUNTER, GROUND + 2
    cv.rect(x0 - 6, cy0, x1 - x0 + 12, cy1 - cy0, pal.WHITE)
    for y in range(cy0 + 2, cy1, 4):
        for x in range(x0 - 6, x1 + 6, 4):
            if (x // 4 + y // 4) % 2 == 0:
                cv.rect(x, y, 4, 4, R("PASTEL_PINK", 6))
    cv.rect(x0 - 6, cy0, x1 - x0 + 12, 2, pal.WHITE)
    cv.rect(x0 - 6, cy1, x1 - x0 + 12, 1, R("PASTEL_PINK", 3))
    for x in range(x0 - 6, x1 + 6, 8):
        cv.ellipse(x + 4, cy1 + 1, 4, 2, pal.WHITE)
    # guy ropes and pegs at the sides
    for (ax, bx) in ((x0 - 14, 0), (x1 + 14, 351)):
        cv.line(ax, eave + 2, bx, GROUND + 10, R("WOOD", 2))
    return cv


@generator()
def pie(kind="pie", card=1, seed=1):
    """A pie (24x14) with its name card (blank: the names are strings)."""
    cv = Canvas(28, 22)
    cv.ellipse(12, 12, 11, 4, R("STONE", 6))
    cv.rect(2, 7, 20, 5, P("CARAMEL", 1))
    cv.ellipse(12, 7, 10, 3.2, P("CARAMEL", 2))
    for x in range(4, 21, 4):
        cv.line(x, 5, x + 2, 9, P("CARAMEL", 0))
    if int(card):
        cv.rect(20, 14, 8, 6, pal.WHITE)
        cv.frame(20, 14, 8, 6, R("STONE", 4))
    cv.a[cv.outline_mask(cv.a != 0)] = P("CARAMEL", 0)
    cv.meta["anchor"] = "bottom"
    cv.meta["text"] = (21, 15, 6, 4)
    return cv


@generator()
def muffin(seed=1):
    cv = Canvas(10, 10)
    cv.poly([(1, 5), (9, 5), (8, 9), (2, 9)], R("CANDY", 4))
    cv.ellipse(5, 4.5, 4.5, 3.4, P("CHOCOLATE", 2))
    cv.px(3, 3, P("CARAMEL", 2))
    cv.a[cv.outline_mask(cv.a != 0)] = P("CHOCOLATE", 0)
    cv.meta["anchor"] = "bottom"
    return cv


@generator()
def fun_fair(seed=260, w=640):
    """1-10, 1-11, 2-03, 2-04, 3-01: the fun fair midway: the teacups ride, the Absolutely
    Not Scary House (its NOTHING INSIDE sign is a string), a low lantern string Pip can
    reach (1-10), candy stalls. Wide (640) for side-view runs and pans."""
    w = int(w)
    cv = Canvas(w, 240)
    # Absolutely Not Scary House: a pastel funhouse facade with a clown-ish cloud mouth door
    hx = 470
    cv.rect(hx - 70, Y(10), 140, GROUND - Y(10), R("VAPE", 6))
    for y in range(Y(10), GROUND, 10):
        cv.rect(hx - 70, y, 140, 1, R("VAPE", 5))
    cv.poly([(hx - 78, Y(12)), (hx, Y(-22)), (hx + 78, Y(12))], P("LAVENDER", 1))
    for k in range(-3, 4):
        cv.line(hx, Y(-22), hx + k * 26, Y(12), P("LAVENDER", 2))
    door = cv.ellipse_mask(hx, GROUND - 22, 24, 24) & (cv.grid()[1] < GROUND)
    cv.shape(door, R("VAPE", 1), outline=P("GRAPE", 0))
    for k in range(-2, 3):
        cv.poly([(hx + k * 9 - 4, GROUND - 44), (hx + k * 9 + 4, GROUND - 44), (hx + k * 9, GROUND - 36)], pal.WHITE)
    # cardboard ghosts on the facade (they turn to face the Wall in 2-03: props there)
    for k, gx in enumerate((hx - 50, hx + 50)):
        g = cv.ellipse_mask(gx, Y(46), 9, 10) | (cv.ellipse_mask(gx, Y(50), 9, 10) & (cv.grid()[1] < Y(60)))
        cv.shape(g, pal.WHITE, outline=R("STONE", 4))
        cv.rect(gx - 4, Y(42), 2, 3, R("STONE", 1))
        cv.rect(gx + 2, Y(42), 2, 3, R("STONE", 1))
    cv.a[cv.outline_mask(cv.a != 0) & (cv.grid()[1] < GROUND)] = P("GRAPE", 0)
    # the teacups ride: a round platform with three giant cups
    tx = 170
    plat = cv.ellipse_mask(tx, GROUND - 4, 110, 14)
    cv.shape(plat, R("PASTEL_PINK", 5), outline=R("PASTEL_PINK", 2), hi=R("PASTEL_PINK", 6))
    for k, (cxo, c) in enumerate(((-64, P("MINT", 1)), (0, P("BUBBLEGUM", 1)), (64, P("LEMON", 1)))):
        cx = tx + cxo
        cup = cv.poly_mask([(cx - 26, GROUND - 46), (cx + 26, GROUND - 46), (cx + 18, GROUND - 14), (cx - 18, GROUND - 14)])
        cup |= cv.ellipse_mask(cx, GROUND - 16, 18, 5)
        cv.shape(cup, c, outline=D(c, 2), hi=L(c, 1), lo=D(c, 1), lo_side="right")
        rim = cv.ellipse_mask(cx, GROUND - 46, 26, 5)
        cv.shape(rim, L(c, 1), outline=D(c, 2))
        inner = cv.ellipse_mask(cx, GROUND - 46, 22, 3)
        cv.a[inner] = D(c, 2)
        h = cv.ellipse_mask(cx + 28, GROUND - 32, 6, 8) & ~cv.ellipse_mask(cx + 28, GROUND - 32, 3, 5)
        cv.a[h] = c
        for dx in (-12, 0, 12):
            cv.px(cx + dx, GROUND - 30, pal.WHITE)
    # a candy-floss cart at the far left
    kit.booth(cv, 18, GROUND + 2, 58, 70, [P("MINT", 2), pal.WHITE], seed=seed, back=R("MINT", 5))
    for k in range(3):
        cv.ellipse(30 + k * 14, COUNTER - 6, 6, 6, [P("BUBBLEGUM", 2), P("LAVENDER", 2), P("SKY", 2)][k])
        cv.rect(30 + k * 14, COUNTER, 1, 6, pal.WHITE)
    # the low lantern string (Pip can reach a lantern: the LOST PROPERTY prop hangs here)
    pts = kit.catenary(300, Y(60), 400, Y(48), 26)
    kit.draw_string(cv, pts, R("WOOD", 2))
    cv.rect(296, Y(56), 4, GROUND - Y(56), R("WOOD", 3))
    cv.rect(400, Y(44), 4, GROUND - Y(44), R("WOOD", 3))
    kit.grass(cv, 0, GROUND, w, 240 - GROUND, seed + 3, ramp_lo=3, ramp_hi=5)
    m = np.zeros((240, w), bool)
    m[GROUND + 12:GROUND + 46, :] = True
    cv.a[m] = R("PEACH", 5)
    cv.a[m & (bayer(w, 240) < 3)] = R("PEACH", 4)
    cv.rect(0, GROUND + 12, w, 1, R("PEACH", 6))
    cv.rect(0, GROUND + 45, w, 1, R("PEACH", 3))
    cv.meta["lantern_spots"] = [(int(pts[i][0]) - (w - 320) // 2, int(pts[i][1]) - 30) for i in range(12, 100, 22)]
    return cv


@generator()
def whack_booth(seed=270):
    """2-08: the Whack-an-Imp booth (holes in the counter; the plastic imps are props) and
    Rivet's welded prize case on the right (the three prizes and their tags are props)."""
    cv = _stall_canvas()
    box = kit.booth(cv, 20, GROUND + 2, 170, 104, [P("CHERRY", 2), pal.WHITE], seed=seed,
                    back=R("PEACH", 3), counter=R("WOOD", 4))
    # five holes in the counter top
    for k in range(5):
        x = 44 + k * 30
        cv.ellipse(x, COUNTER - 1, 10, 3, R("STONE", 0))
        cv.ellipse(x, COUNTER - 2, 10, 2, R("STONE", 1))
    # mallet hanging on the post
    cv.rect(184, COUNTER - 44, 3, 30, R("WOOD", 4))
    cv.rect(178, COUNTER - 50, 16, 9, P("CHERRY", 1))
    cv.frame(178, COUNTER - 50, 16, 9, P("CHERRY", 0))
    # Rivet's prize case: riveted steel frame, glass front, velvet bed
    x0, y0, x1, y1 = 214, Y(48), 338, GROUND + 2
    cv.rect(x0, y0, x1 - x0, y1 - y0, R("STEEL", 3))
    cv.rect(x0 + 5, y0 + 5, x1 - x0 - 10, y1 - y0 - 30, P("CHERRY", 0))
    cv.dither_rect(x0 + 5, y0 + 5, x1 - x0 - 10, 8, P("CHERRY", 0), P("BLOOD", 1), 6)
    for x in range(x0 + 2, x1, 8):
        for y in (y0 + 2, y1 - 3):
            cv.px(x, y, R("STEEL", 6))
    for y in range(y0 + 2, y1, 8):
        for x in (x0 + 2, x1 - 3):
            cv.px(x, y, R("STEEL", 6))
    # glass glint
    for k in range(3):
        cv.line(x0 + 12 + k * 4, y0 + 40, x0 + 30 + k * 4, y0 + 10, R("SKY", 7))
    cv.rect(x0 + 5, y1 - 25, x1 - x0 - 10, 20, R("STEEL", 2))
    cv.rect(x0 + 5, y1 - 25, x1 - x0 - 10, 1, R("STEEL", 4))
    cv.frame(x0, y0, x1 - x0, y1 - y0, R("STEEL", 0))
    cv.meta["case"] = (x0, y0, x1, y1)
    return cv


@generator()
def plastic_imp(state="up", seed=1):
    """A whack-an-imp target (14x16): up (grinning), shut (pulled its lid down)."""
    cv = Canvas(16, 18)
    if state == "up":
        body = cv.ellipse_mask(8, 9, 6, 7)
        cv.shape(body, P("CHERRY", 1), outline=P("CHERRY", 0), hi=P("CHERRY", 2))
        cv.poly([(3, 4), (5, 0), (6, 4)], P("CHERRY", 1))
        cv.poly([(10, 4), (11, 0), (13, 4)], P("CHERRY", 1))
        cv.px(6, 8, pal.WHITE)
        cv.px(10, 8, pal.WHITE)
        cv.rect(6, 12, 5, 1, R("STONE", 0))
    else:
        lid = cv.ellipse_mask(8, 15, 7, 3)
        cv.shape(lid, R("STEEL", 3), outline=R("STEEL", 0), hi=R("STEEL", 5))
    cv.meta["anchor"] = "bottom"
    return cv


@generator()
def prize(kind="wand"):
    """Rivet's prizes on velvet: wand (the Bubble Wand), pump (the Pump Shotgun: a
    bath-house bellows, a drainpipe, a bike bell), saw (the welded saw). Each carries a
    blank name tag: PARTY PUMP over SLAUGHTER, PURRING SAW over MEATCLEAVER are strings."""
    if kind == "wand":
        cv = Canvas(30, 30)
        cv.line(4, 26, 20, 10, R("CANDY", 5), 2)
        ring = cv.ellipse_mask(23, 7, 6, 6) & ~cv.ellipse_mask(23, 7, 4, 4)
        cv.a[ring] = P("BUBBLEGUM", 1)
        cv.px(21, 5, pal.WHITE)
    elif kind == "pump":
        cv = Canvas(44, 22)
        cv.rect(2, 9, 30, 5, R("STEEL", 4))          # drainpipe barrel
        cv.rect(2, 9, 30, 1, R("STEEL", 6))
        cv.poly([(26, 14), (40, 10), (42, 18), (28, 20)], P("CARAMEL", 1))   # bellows
        for x in range(30, 40, 3):
            cv.line(x, 12, x + 1, 19, P("CARAMEL", 0))
        cv.ellipse(10, 7, 3, 2, R("CANDY", 6))        # the bike bell
        cv.px(10, 6, pal.WHITE)
    else:
        cv = Canvas(46, 22)
        cv.rect(2, 8, 28, 8, R("STEEL", 5))
        for x in range(2, 30, 3):
            cv.poly([(x, 8), (x + 3, 8), (x + 1.5, 5)], R("STEEL", 6))
        cv.rect(28, 6, 14, 12, P("CHERRY", 1))         # welded motor housing
        for x in range(30, 42, 4):
            cv.px(x, 8, R("STEEL", 6))
        cv.rect(40, 16, 4, 5, R("STEEL", 2))
    cv.a[cv.outline_mask(cv.a != 0)] = R("STEEL", 0)
    return cv


@generator()
def name_tag(w=26, h=9):
    cv = Canvas(int(w), int(h))
    cv.rect(0, 0, int(w), int(h), pal.CREAM)
    cv.frame(0, 0, int(w), int(h), R("PEACH", 3))
    cv.px(1, int(h) // 2, R("STONE", 3))
    cv.meta["text"] = (3, 1, int(w) - 4, int(h) - 2)
    return cv


@generator()
def giggle_wide(seed=280, w=400):
    """1-13 (21:00), EC wides: the whole Giggle at chibi scale: tents, stalls, the teacups,
    the bandstand with its boarded half, the Wall across the back right, the town clock
    on its hill, and strings of lanterns over everything."""
    w = int(w)
    cv = Canvas(w, 240)
    base = Y(120)
    # town hill with the clock tower (left back)
    kit.hills(cv, base - 30, 18, 300, R("GRASS", 4), seed, top=R("GRASS", 5), lumps=2, bottom=base + 2)
    tx = 64
    cv.rect(tx - 6, base - 86, 12, 56, R("PEACH", 6))
    cv.rect(tx - 6, base - 86, 2, 56, R("PEACH", 7))
    cv.poly([(tx - 9, base - 86), (tx + 9, base - 86), (tx, base - 102)], P("BUBBLEGUM", 1))
    face = cv.ellipse_mask(tx, base - 72, 5, 5)
    cv.shape(face, pal.CREAM, outline=R("STONE", 2))
    cv.line(tx, base - 72, tx, base - 76, R("STONE", 1))        # 21:00: the hour hand at 9
    cv.line(tx, base - 72, tx - 3, base - 72, R("STONE", 1))
    # the Wall across the back right, Hellvile's dark beyond
    cv.rect(268, base - 64, w - 268, 66, R("STONE", 3))
    kit.bricks(cv, 268, base - 64, w - 268, 66, seed + 1, base=R("STONE", 3), mortar=R("STONE", 1), bw=12, bh=6)
    cv.rect(268, base - 66, w - 268, 2, R("STONE", 4))
    cv.rect(268, base - 64, 4, 66, R("STONE", 5))
    # the ground
    kit.grass(cv, 0, base, w, 240 - base, seed + 2, ramp_lo=3, ramp_hi=5)
    # the bandstand, small (north half lit, south half boarded) against the Wall
    bx = 236
    cv.rect(bx - 30, base - 8, 60, 8, R("WOOD", 4))
    for k in range(5):
        x = bx - 26 + k * 13
        cv.rect(x, base - 32, 2, 24, pal.WHITE if x < bx else R("STONE", 4))
    cv.rect(bx, base - 30, 28, 22, R("WOOD", 2))
    for y in range(base - 30, base - 8, 4):
        cv.rect(bx, y, 28, 1, R("WOOD", 1))
    cv.poly([(bx - 34, base - 32), (bx, base - 50), (bx + 34, base - 32)], R("PASTEL_PINK", 4))
    cv.poly([(bx, base - 50), (bx + 34, base - 32), (bx, base - 32)], R("STONE", 4))
    # tents and stalls across the middle
    for (x, c, tw, th) in ((120, P("LAVENDER", 2), 44, 42), (170, P("SKY", 2), 34, 30),
                           (30, P("LEMON", 2), 40, 36), (310, P("BUBBLEGUM", 2), 40, 40)):
        if x < 268 or x > 300:
            kit.tent(cv, x, base + 18, tw, th, [c, pal.CREAM], flag=c)
    # lantern strings over the fair
    for (x0, y0, x1, y1, sag) in ((0, Y(20), 140, Y(30), 18), (140, Y(30), 300, Y(18), 20),
                                   (60, Y(56), 200, Y(64), 12)):
        pts = kit.catenary(x0, y0, x1, y1, sag)
        kit.draw_string(cv, pts, R("WOOD", 2))
        for i in range(6, len(pts) - 4, 12):
            cv.ellipse(pts[i][0], pts[i][1] + 3, 2, 2.5, kit.LANTERN_COLS[i // 12 % 5])
            cv.px(pts[i][0], pts[i][1] + 3, R("CANDY", 7))
    return cv
