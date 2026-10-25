"""cards.py - the pilot's cards: the act cards (the Mayor's clipboard), the TV-MA card,
the main-title logo (drawn the way the game's title screen draws it), the pilot title
card, the end-credits head card, the eyecatch cards and the Dippinn Productions card.

Cards are ordinary 2D sets; their lines are props so the player can pop them in on
the beat (lines 8 f apart, the deadpan whole, SUPERs on top are strings).
"""
import math
import os

import numpy as np

from . import generator, pal
from .canvas import Canvas, Rng, hash2, bayer
from . import kit
from .kit import R, P, D, L, Y
from . import font as gfont

ADV = 6


def big_text(cv, x, y, s, scale, color, outline):
    """ui_text_big() from core/src/ui.c: a shadow, an outline ring, the fill, and the
    lighter upper part of each stroke (the game's two-tone candy text)."""
    def glyphs(x0, y0, c, top_only=False):
        cx = x0
        for ch in s:
            g = gfont.glyph(ch)
            ys, xs = np.nonzero(g)
            for gy, gx in zip(ys, xs):
                if top_only:
                    if gy < 3:
                        cv.rect(cx + gx * scale, y0 + gy * scale, scale, max(scale // 2, 1), c)
                else:
                    cv.rect(cx + gx * scale, y0 + gy * scale, scale, scale, c)
            cx += ADV * scale
    glyphs(x + 1, y + 3, D(outline, 1))
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dx or dy:
                glyphs(x + dx, y + dy, outline)
    glyphs(x, y + 1, outline)
    glyphs(x, y, color)
    glyphs(x, y, L(color, 1), True)


def big_width(s, scale):
    return len(s) * ADV * scale - scale + 2


# ---------------------------------------------------------------------------
# the backdrop every card shares: pastel with polka dots (the game's chapter cards)
# ---------------------------------------------------------------------------
@generator()
def card_backdrop(color="pink", seed=1, w=320, h=240):
    cv = Canvas(int(w), int(h))
    base = {"pink": R("PASTEL_PINK", 6), "cream": pal.CREAM, "lavender": R("VAPE", 7),
            "mint": R("MINT", 7), "night": R("SKY", 1)}[color]
    dot = {"pink": R("PASTEL_PINK", 7), "cream": R("PEACH", 7), "lavender": R("VAPE", 6),
           "mint": R("MINT", 6), "night": R("SKY", 2)}[color]
    cv.rect(0, 0, cv.w, cv.h, base)
    for y in range(4, cv.h, 12):
        for x in range((y // 12) % 2 * 6 + 2, cv.w, 12):
            cv.rect(x, y, 2, 2, dot)
    return cv


# ---------------------------------------------------------------------------
# the act cards: the Mayor's festival clipboard
# ---------------------------------------------------------------------------
CLIP = dict(x=56, y=Y(4), w=208, h=168)            # the board (plate px)
PAPER = dict(x=66, y=Y(18), w=188, h=150)
LINE0 = Y(36)                                       # first handwritten line (plate y)
LINE_DY = 22


@generator()
def clipboard(seed=1):
    """The clipboard on its card: a pink board, the silver clip, a cream sheet with faint
    rules, a cupcake doodle in the margin. Lines are props (hand_line)."""
    cv = card_backdrop("pink", seed)
    b = CLIP
    m = np.zeros((cv.h, cv.w), bool)
    m[b["y"]:b["y"] + b["h"], b["x"]:b["x"] + b["w"]] = True
    cv.shape(m, P("BUBBLEGUM", 1), outline=P("ROSE", 0), hi=P("BUBBLEGUM", 2))
    for k in range(4):
        cv.px(b["x"] + 3, b["y"] + 3 + k, P("BUBBLEGUM", 2))
    # shadow of the board on the backdrop
    cv.rect(b["x"] + 4, b["y"] + b["h"] + 1, b["w"], 3, R("PASTEL_PINK", 4))
    cv.rect(b["x"] + b["w"] + 1, b["y"] + 4, 3, b["h"], R("PASTEL_PINK", 4))
    p = PAPER
    cv.rect(p["x"], p["y"], p["w"], p["h"], pal.CREAM)
    cv.rect(p["x"] + p["w"] - 2, p["y"] + 2, 2, p["h"] - 2, R("PEACH", 6))
    for y in range(LINE0 + 16, p["y"] + p["h"] - 4, LINE_DY):
        cv.rect(p["x"] + 6, y, p["w"] - 12, 1, R("SKY", 7))
    cv.rect(p["x"] + 20, p["y"] + 4, 1, p["h"] - 8, R("PASTEL_PINK", 6))
    # the clip
    cx = b["x"] + b["w"] // 2
    cv.rect(cx - 26, b["y"] - 4, 52, 16, R("STEEL", 5))
    cv.rect(cx - 26, b["y"] - 4, 52, 2, R("STEEL", 6))
    cv.rect(cx - 26, b["y"] + 10, 52, 2, R("STEEL", 3))
    cv.ellipse(cx, b["y"] - 6, 8, 5, R("STEEL", 4))
    cv.ellipse(cx, b["y"] - 6, 4, 2.5, R("PASTEL_PINK", 6))
    cv.frame(cx - 27, b["y"] - 5, 54, 18, R("STEEL", 1))
    # a cupcake doodle in the margin (pen)
    dx, dy = p["x"] + p["w"] - 26, p["y"] + p["h"] - 26
    ink = R("SKY", 3)
    cv.polyline([(dx, dy + 10), (dx + 14, dy + 10), (dx + 12, dy + 18), (dx + 2, dy + 18), (dx, dy + 10)], ink)
    cv.polyline([(dx - 1, dy + 10), (dx + 2, dy + 4), (dx + 7, dy + 2), (dx + 12, dy + 4), (dx + 15, dy + 10)], ink)
    cv.px(dx + 7, dy, P("CHERRY", 1))
    return cv


def hand_text(cv, x, y, s, c, seed=1, slant=1):
    """The Mayor's handwriting: the game font one px wide per stroke, two tall, slanted,
    each letter nudged up or down a pixel (seeded)."""
    rng = Rng(seed)
    xx = x
    for ch in s:
        g = gfont.glyph(ch)
        dy = rng.randint(-1, 1)
        ys, xs = np.nonzero(g)
        for gy, gx in zip(ys, xs):
            for sy in range(2):
                py = gy * 2 + sy
                shear = int(round(slant * (15 - py) / 15.0))
                cv.px(xx + gx + shear, y + py + dy, c)
        xx += 6 if ch != " " else 4
    return xx


def hand_width(s):
    return sum(6 if ch != " " else 4 for ch in s) + 1


@generator()
def hand_line(text="18:00 Welcome!", mark="", seed=1, ink="pen"):
    """One handwritten clipboard line as a prop (pops in on its beat).
    mark: tick (a tick after it), underline2 (underlined twice, nervously), strike (struck
    through), blank (a long blank rule after the time)."""
    c = {"pen": R("SKY", 2), "red": P("CHERRY", 1)}[ink]
    text = str(text).replace("_", " ")
    w = max(hand_width(text) + (96 if mark == "blank" else 24), 40)
    cv = Canvas(w, 22)
    end = hand_text(cv, 2, 2, text, c, seed)
    if mark == "tick":
        cv.polyline([(end + 4, 10), (end + 7, 14), (end + 15, 2)], P("LIME", 0))
        cv.polyline([(end + 5, 10), (end + 8, 14), (end + 16, 2)], P("LIME", 0))
    elif mark == "underline2":
        rng = Rng(seed + 5)
        for k, yy in enumerate((19, 21)):
            pts = [(2 + i * 6, yy + rng.randint(-1, 0)) for i in range((end - 2) // 6 + 1)]
            cv.polyline(pts, P("CHERRY", 1))
    elif mark == "strike":
        cv.line(hand_width(text.split(" ")[0]) + 4, 10, end, 8, P("CHERRY", 1))
        cv.line(hand_width(text.split(" ")[0]) + 4, 11, end, 9, P("CHERRY", 1))
    elif mark == "blank":
        for k in range(8):
            cv.rect(end + 2 + k * 10, 15 + (k % 2), 8, 1, c)
    cv.meta["anchor"] = "topleft"
    return cv


@generator()
def hand_word(text="weather", seed=7):
    """A word written over a struck one (Act Three: 'weather' over 'Bedtime!')."""
    text = str(text)
    cv = Canvas(hand_width(text) + 6, 20)
    hand_text(cv, 2, 2, text, P("CHERRY", 1), seed, slant=2)
    cv.meta["anchor"] = "topleft"
    return cv


@generator()
def mirror_ghost(seed=9):
    """Act Four: faint mirror-writing showing through from the sheets clipped behind."""
    cv = Canvas(180, 70)
    rng = Rng(seed)
    lines = ["You never asked", "where it went.", "  - G"]
    for k, s in enumerate(lines):
        tmp = Canvas(180, 20)
        hand_text(tmp, 4, 2, s, R("SKY", 6), seed + k)
        cv.blit(tmp.a[:, ::-1], 0, k * 20)
    cv.meta["anchor"] = "topleft"
    return cv


# ---------------------------------------------------------------------------
# the TV-MA card (head of the episode)
# ---------------------------------------------------------------------------
@generator()
def tvma_card(descriptors="V", seed=1):
    """TV-MA (V): the rating box, plain and adult (no cute art on it: release rule 7)."""
    cv = Canvas(320, 240)
    cv.rect(0, 0, 320, 240, R("SKY", 0))
    bx, by, bw, bh = 104, Y(26), 112, 128
    m = np.zeros((240, 320), bool)
    m[by:by + bh, bx:bx + bw] = True
    for (cx, cy) in ((bx, by), (bx + bw - 1, by), (bx, by + bh - 1), (bx + bw - 1, by + bh - 1)):
        for dy in range(-2, 3):
            for dx in range(-2, 3):
                if abs(dx) + abs(dy) >= 3:
                    pass
    cv.a[m] = pal.WHITE
    inner = np.zeros_like(m)
    inner[by + 4:by + bh - 4, bx + 4:bx + bw - 4] = True
    cv.a[inner] = R("SKY", 0)
    cv.rect(bx + 4, by + 44, bw - 8, 3, pal.WHITE)
    big_text(cv, bx + (bw - big_width("TV", 4)) // 2 + 1, by + 10, "TV", 4, pal.WHITE, R("STONE", 2))
    big_text(cv, bx + (bw - big_width("MA", 4)) // 2 + 1, by + 54, "MA", 4, pal.WHITE, R("STONE", 2))
    cv.rect(bx + 4, by + 94, bw - 8, 3, pal.WHITE)
    big_text(cv, bx + (bw - big_width(str(descriptors), 2)) // 2 + 1, by + 104, str(descriptors), 2,
             pal.WHITE, R("STONE", 2))
    return cv


# ---------------------------------------------------------------------------
# the logo (MT-1 letters, MT-6 end pose), drawn the way core/src/fp_title.c draws it
# ---------------------------------------------------------------------------
LOGO_COLS = ["ROSE", "TANGERINE", "LEMON", "MINT", "SKY", "LAVENDER", "BUBBLEGUM", "ROSE", "LEMON", "MINT"]


@generator()
def logo(part="all", scale=3, seed=1):
    """DOOMOGACHI (ten candy letters) + the LIFE pill + 'in HELLvile Park!', as the game's
    title screen draws them (ui_text_big at scale 3). part=all|letters|life|tagline."""
    part = {"doomogachi": "letters", "park": "tagline"}.get(part, part)   # mt02's letter-drop names
    sc = int(scale)
    lw = 10 * ADV * sc
    cv = Canvas(lw + 16, 8 * sc + 50)
    x0, y0 = 8, 4
    if part in ("all", "letters"):
        for i, ch in enumerate("DOOMOGACHI"):
            big_text(cv, x0 + i * ADV * sc, y0, ch, sc, P(LOGO_COLS[i], 2), P(LOGO_COLS[i], 0))
    if part in ("all", "life"):
        lx, ly = x0 + lw - 70, y0 + 8 * sc + 1
        pill = cv.ellipse_mask(lx + 9, ly + 9, 9, 9) | cv.ellipse_mask(lx + 57, ly + 9, 9, 9)
        pill[ly:ly + 18, lx + 9:lx + 57] = True
        cv.shape(pill, P("ROSE", 1), outline=P("ROSE", 0), hi=P("ROSE", 2))
        big_text(cv, lx + 12, ly + 2, "LIFE", 2, pal.WHITE, P("ROSE", 0))
        kit.heart(cv, lx + 60, ly + 6, 3, pal.GOLD)
    if part in ("all", "tagline"):
        ly = y0 + 8 * sc + 22
        tw = len("in HELLvile Park!") * 6
        lx = (cv.w - tw) // 2 - 20
        gfont.draw_text(cv, lx, ly, "in", pal.WHITE, outline=P("GRAPE", 0))
        gfont.draw_text(cv, lx + 18, ly, "HELL", P("CHERRY", 1), outline=R("BLOOD", 0))
        gfont.draw_text(cv, lx + 18 + 24 - 2, ly, "vile Park!", pal.WHITE, outline=P("GRAPE", 0))
        cv.meta["drip_x"] = lx + 18
    cv.meta["anchor"] = "top"
    return cv


@generator()
def roller(seed=1, wet="pink"):
    """The Mayor's paint roller (it crosses the banner in CO-4 and the logo in MT-6)."""
    cv = Canvas(30, 44)
    c = P("BUBBLEGUM", 1) if wet == "pink" else pal.WHITE
    cv.rect(2, 2, 26, 12, c)
    cv.rect(2, 2, 26, 2, P("BUBBLEGUM", 2))
    cv.rect(2, 12, 26, 2, P("BUBBLEGUM", 0))
    cv.rect(26, 6, 3, 3, R("STEEL", 4))
    cv.line(28, 8, 28, 20, R("STEEL", 4))
    cv.line(28, 20, 16, 24, R("STEEL", 4))
    cv.rect(14, 24, 4, 18, R("WOOD", 4))
    cv.rect(14, 24, 1, 18, R("WOOD", 5))
    cv.a[cv.outline_mask(cv.a != 0)] = R("STONE", 1)
    cv.meta["anchor"] = "top"
    return cv


# ---------------------------------------------------------------------------
# eyecatch cards: a lantern printed with the logo, the town clock
# ---------------------------------------------------------------------------
@generator()
def clock_face(time="2100", seed=1):
    """The town clock's face (34x34): hands at the time of the night (hhmm)."""
    cv = Canvas(36, 36)
    m = cv.ellipse_mask(18, 18, 16, 16)
    cv.shape(m, pal.CREAM, outline=R("WOOD", 1), hi=pal.WHITE)
    ring = cv.ellipse_mask(18, 18, 16, 16) & ~cv.ellipse_mask(18, 18, 14, 14)
    cv.a[ring] = P("BUBBLEGUM", 1)
    for k in range(12):
        a = k / 12 * 6.283
        cv.px(18 + math.sin(a) * 11, 18 - math.cos(a) * 11, R("WOOD", 2))
    t = str(time)
    hh, mm = int(t[:2]) % 12, int(t[2:])
    am = mm / 60 * 6.283
    ah = (hh + mm / 60) / 12 * 6.283
    cv.line(18, 18, 18 + math.sin(am) * 11, 18 - math.cos(am) * 11, R("STONE", 1))
    cv.line(18, 18, 18 + math.sin(ah) * 7, 18 - math.cos(ah) * 7, R("STONE", 0), 2)
    cv.px(18, 18, P("CHERRY", 1))
    cv.meta["anchor"] = "center"
    return cv


@generator()
def logo_lantern(state="lit", seed=1):
    """EC1: a big round paper lantern printed with the logo's heart and its name
    (state=purple: it glows purple from the inside, like a ribcage)."""
    cv = Canvas(84, 104)
    cx, cy = 42, 56
    body = cv.ellipse_mask(cx, cy, 38, 40)
    fill = P("BUBBLEGUM", 2) if state == "lit" else P("VAPE_PURPLE", 1)
    core = R("CANDY", 7) if state == "lit" else P("VAPE_PURPLE", 2)
    cv.shape(body, fill, outline=P("ROSE", 0) if state == "lit" else P("GRAPE", 0))
    cv.a[cv.ellipse_mask(cx, cy + 2, 24, 26) & body] = core if state == "lit" else P("VAPE_PURPLE", 2)
    cv.a[cv.ellipse_mask(cx, cy + 4, 12, 14) & body] = pal.WHITE if state == "lit" else P("LAVENDER", 2)
    for dx in (-26, -13, 13, 26):
        for y in range(cy - 38, cy + 39):
            if body[y, cx + dx]:
                cv.a[y, cx + dx] = D(cv.a[y, cx + dx], 1)
    if state == "purple":
        # ribs: the light behind the paper looks like a ribcage
        for k in range(4):
            y = cy - 12 + k * 9
            cv.a[(np.abs(np.arange(104)[:, None] - y) <= 1) & body & cv.ellipse_mask(cx, cy, 20, 30)] = P("GRAPE", 0)
    kit.heart(cv, cx, cy - 8, 8, P("CHERRY", 1) if state == "lit" else P("GRAPE", 1),
              outline=P("CHERRY", 0) if state == "lit" else P("GRAPE", 0))
    cv.rect(cx - 16, 10, 32, 6, R("WOOD", 3))
    cv.rect(cx - 16, 96, 32, 6, R("WOOD", 3))
    cv.rect(cx - 1, 0, 2, 10, R("WOOD", 2))
    cv.meta["anchor"] = "top"
    return cv


@generator()
def eggshell(half="left", seed=1, size=1):
    """EC2: an eggshell half hanging on the lantern string (24x18 at size 1)."""
    from .props import _upscale
    return _upscale(_eggshell(half), size)


def _eggshell(half="left"):
    cv = Canvas(26, 20)
    m = cv.ellipse_mask(13, 6, 11, 13) & (cv.grid()[1] > 6)
    cv.shape(m, pal.CREAM, outline=R("PEACH", 3), lo=R("PEACH", 6), lo_side="right" if half == "left" else "left")
    for x in range(2, 25, 4):
        cv.px(x, 7 + (x // 4) % 2, R("PEACH", 3))
    cv.ellipse(13, 7, 10, 2, R("PEACH", 6))
    cv.rect(12, 0, 2, 7, R("WOOD", 2))
    cv.meta["anchor"] = "top"
    return cv


@generator()
def eyecatch_back(seed=1, banner=1):
    """The eyecatch backdrop: night over the Giggle, the banner across the back,
    a lantern string, the bandstand rail along the bottom (for the EC3 candles)."""
    cv = Canvas(352, 240)
    for y in range(240):
        cv.a[y, :] = R("SKY", 1) if y < 90 else (R("SKY", 2) if y < 150 else R("SKY", 3))
    for y in (90, 150):
        cv.dither_rect(0, y - 2, 352, 4, cv.a[y - 3, 0], cv.a[y + 3, 0], 8)
    kit.stars(cv, 50, seed, 0, 140)
    kit.hills(cv, Y(126), 8, 300, R("GRASS", 2), seed, top=R("GRASS", 3), bottom=240)
    for k in range(6):
        kit.tent(cv, 20 + k * 64, Y(132), 40, 36, [kit.LANTERN_COLS[k % 5], pal.CREAM], door=False,
                 flag=kit.LANTERN_COLS[(k + 2) % 5])
    if int(banner):
        bn = gfont  # noqa
        from .props import banner as _banner
        b = _banner(size="wide", state="giggle")
        cv.blit(b, 176 - b.w // 2, Y(64))
    pts = kit.catenary(0, Y(12), 352, Y(12), 26)
    kit.draw_string(cv, pts, R("WOOD", 2))
    # the bandstand rail along the bottom
    ry = Y(150)
    cv.rect(0, ry, 352, 4, pal.WHITE)
    for x in range(4, 352, 9):
        cv.rect(x, ry + 4, 4, 20, pal.WHITE)
        cv.rect(x + 3, ry + 4, 1, 20, R("STONE", 6))
    kit.planks(cv, 0, ry + 24, 352, 240 - ry - 24, seed, base=R("WOOD", 4), pw=6)
    return cv


# ---------------------------------------------------------------------------
# the Dippinn Productions card (PC-1's last frames), from docs/media (read-only)
# ---------------------------------------------------------------------------
@generator()
def dippinn_card(seed=1):
    """docs/media/dippinn_productions.png quantised to the master palette (nearest by the
    game's own redmean distance), centred on black at 320x240. The source is the repo's
    own logo file; the quantiser is the generator (no binary is committed)."""
    from PIL import Image
    src = Image.open(os.path.join(pal.ROOT, "docs", "media", "dippinn_productions.png")).convert("RGB")
    w, h = src.size
    w2, h2 = 300, max(1, int(h * 300 / w))
    src = src.resize((w2, h2), Image.NEAREST)
    a = np.asarray(src).astype(np.int64)
    cv = Canvas(320, 240)
    cv.rect(0, 0, 320, 240, pal.BLACK)
    cache = {}
    sub = np.zeros((h2, w2), np.uint8)
    for y in range(h2):
        for x in range(w2):
            r, g, b = (int(v) for v in a[y, x])
            if r + g + b < 40:
                sub[y, x] = pal.BLACK
                continue
            key = (r >> 2, g >> 2, b >> 2)
            if key not in cache:
                cache[key] = pal.nearest(r, g, b)
            sub[y, x] = cache[key]
    cv.blit(sub, (320 - w2) // 2, (240 - h2) // 2, key=None)
    return cv
