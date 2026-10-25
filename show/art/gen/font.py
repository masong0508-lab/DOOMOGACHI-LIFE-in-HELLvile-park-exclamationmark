"""font.py - the game's own 5x8 font (core/src/font.c, read-only) for baked lettering.

Only art that is lettering by nature is baked (act cards, the banner paint,
stencils, the TV-MA card). Signage that the episode reads (sign text, SUPERs)
lives in strings and is drawn by the player on the sign props.
"""
import os
import re

import numpy as np

from . import pal
from .canvas import Rng

FIRST = 0x20


def _load():
    src = open(os.path.join(pal.ROOT, "core", "src", "font.c")).read()
    body = src[src.index("doomo_font[FONT_LAST - FONT_FIRST + 1][5] = {"):]
    rows = re.findall(r"FONT_G\(([^)]*)\)", body)
    glyphs = []
    for r in rows:
        ids = [t.strip() for t in r.split(",")]
        g = np.zeros((8, 5), bool)
        for y, ident in enumerate(ids[:8]):
            for x, ch in enumerate(ident[:5]):
                g[y, x] = ch == "X"
        glyphs.append(g)
    return glyphs


GLYPHS = _load()


def glyph(ch):
    o = ord(ch) - FIRST
    if 0 <= o < len(GLYPHS):
        return GLYPHS[o]
    return GLYPHS[ord("?") - FIRST]


def text_width(s, scale=1, spacing=1):
    return len(s) * (5 + spacing) * scale - spacing * scale if s else 0


def draw_text(cv, x, y, s, c, scale=1, spacing=1, shadow=None, outline=None, bold=False):
    """Draw `s` with the game font at an integer scale."""
    def put(xx, yy, col):
        for ch in s:
            g = glyph(ch)
            ys, xs = np.nonzero(g)
            for gy, gx in zip(ys, xs):
                cv.rect(xx + gx * scale, yy + gy * scale, scale + (1 if bold else 0), scale, col)
            xx += (5 + spacing) * scale

    if outline is not None:
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, 1), (-1, 1), (1, -1)):
            put(x + dx, y + dy, outline)
    if shadow is not None:
        put(x + 1, y + 1, shadow)
    put(x, y, c)


def draw_hand(cv, x, y, s, c, scale=2, seed=1, slant=1, wobble=1, spacing=1):
    """A handwriting look from the game font: per-glyph baseline wobble and a slant.

    Each glyph is drawn at `scale`, sheared right by `slant` px over its height,
    and nudged up or down by up to `wobble` px (seeded, so it never changes).
    """
    rng = Rng(seed)
    xx = x
    for ch in s:
        g = glyph(ch)
        dy = rng.randint(-wobble, wobble) if wobble else 0
        hgt = 8 * scale
        ys, xs = np.nonzero(g)
        for gy, gx in zip(ys, xs):
            for sy in range(scale):
                py = gy * scale + sy
                shear = int(round(slant * (hgt - 1 - py) / max(1, hgt - 1)))
                cv.rect(xx + gx * scale + shear, y + py + dy, scale, 1, c)
        xx += (5 + spacing) * scale + (1 if ch == " " else 0)
    return xx
