"""light.py - time of day as palette variants (256-entry index remaps), never redraws.

A set variant re-colours a plate by swapping its CLUT: the player uploads
CLUT[i] = doomo_palette[lut[i]] for the plate, so the pixels never change and
every colour on screen is still a master-palette colour (PIC-PAL stays 100 %).

  day     identity
  dawn    soft peach-pink morning (the cold open's bandstand, the plaza, the tag)
  sunset  warm rose-gold (18:00: the road, the town sign, the park gate)
  dusk    lavender blue hour (the Giggle crawl, the welcome at the bandstand)
  night   moonlit blue with the emissive colours kept lit (the NIGHT tint's rule,
          as the treatment's colour script asks), a little lighter than the game's
          own TINT_NIGHT so pastel still reads at 320x180
  game_night  the game's own TINT_NIGHT table (core/gen/art_data.c), verbatim
  dark    night with the lanterns out (1-14's act out and Act Two's darkest beats)
  vape    21:00: the VAPE ramp floods in from the south as the lanterns die
  sepia   the kids'-tent memory insert (1-14)
  drained the crayon storybook with the pink drained out (co09)

Skies are BANDS planes (flat palette steps drawn as TILEs), one band list per
time of day; see SKY_BANDS and show/series/sets/skies.shw.
"""
import numpy as np

from . import pal

IDENT = np.arange(256, dtype=np.uint8)

# emissive indices (core/art/art_palette.c emissive()): kept lit at night
EMISSIVE = set([pal.GOLD, pal.LCD, pal.WHITE, pal.ramp("CANDY", 7)] +
               [pal.ramp("LAVA", s) for s in range(4, 8)] +
               [pal.paint("HELLFIRE", s) for s in range(3)] +
               [pal.paint("ECTOPLASM", s) for s in (1, 2)])


def _xform(fn, keep_emissive=False, slack=10):
    lut = IDENT.copy()
    for i in range(1, 256):
        if keep_emissive and i in EMISSIVE:
            continue
        r, g, b = [int(v) for v in pal.RGB[i]]
        r2, g2, b2 = fn(r, g, b)
        lut[i] = pal.nearest_fam(r2, g2, b2, i, slack)
    lut[0] = 0
    return lut


def _luma(r, g, b):
    return (r * 77 + g * 150 + b * 29) >> 8


def _sunset(r, g, b):
    return r * 0.94 + 30, g * 0.76 + 10, b * 0.68 + 24


def _dawn(r, g, b):
    l = _luma(r, g, b)
    r, g, b = (r * 3 + l) / 4, (g * 3 + l) / 4, (b * 3 + l) / 4
    return r * 0.90 + 30, g * 0.86 + 18, b * 0.84 + 28


def _dusk(r, g, b):
    return r * 0.86 + 18, g * 0.74 + 8, b * 0.86 + 32


def _night(r, g, b):
    l = _luma(r, g, b)
    r, g, b = (r * 3 + l) / 4, (g * 3 + l) / 4, (b * 3 + l) / 4
    return r * 0.58 + 12, g * 0.62 + 16, b * 0.76 + 40


def _dark(r, g, b):
    l = _luma(r, g, b)
    r, g, b = (r + l) / 2, (g + l) / 2, (b + l) / 2
    return r * 0.22 + 4, g * 0.26 + 6, b * 0.42 + 22


def _vape(r, g, b):
    l = _luma(r, g, b)
    r, g, b = (r + l) / 2, (g + l) / 2, (b + l) / 2
    return r * 0.34 + 40, g * 0.24 + 12, b * 0.44 + 60


def _sepia(r, g, b):
    l = _luma(r, g, b)
    return l * 0.95 + 26, l * 0.80 + 14, l * 0.58 + 6


def _drained(r, g, b):
    l = _luma(r, g, b)
    # pinks and reds lose their colour first; everything goes a little greyer
    if r > g + 30 and r > b:
        return (r + 3 * l) / 4, (g + 3 * l) / 4, (b + 3 * l) / 4 + 6
    return (r * 2 + l) / 3, (g * 2 + l) / 3, (b * 2 + l) / 3 + 4


_CACHE = {}


def lut(name):
    """The 256-byte index remap for a named time of day / register."""
    name = (name or "day").lower()
    if name in _CACHE:
        return _CACHE[name]
    if name in ("day", "none", "identity"):
        t = IDENT.copy()
    elif name == "night":
        t = _xform(_night, keep_emissive=True, slack=35)
    elif name == "game_night":
        t = pal.TINTMAP[pal.TINT_NIGHT].copy()
    elif name == "sunset":
        t = _xform(_sunset, slack=30)
    elif name == "dawn":
        t = _xform(_dawn, slack=20)
    elif name == "dusk":
        t = _xform(_dusk, keep_emissive=True, slack=40)
    elif name == "dark":
        t = _xform(_dark)
    elif name == "vape":
        t = _xform(_vape)
    elif name == "sepia":
        t = _xform(_sepia, slack=0)
    elif name == "drained":
        t = _xform(_drained)
    else:
        raise KeyError("unknown light '%s'" % name)
    t[0] = 0
    _CACHE[name] = t
    return t


NAMES = ["day", "dawn", "sunset", "dusk", "night", "game_night", "dark", "vape", "sepia", "drained"]


# ---- skies (BANDS planes) ---------------------------------------------------
# Each entry is a list of (palette index, rows), top to bottom, for a 240-row
# open-matte frame (rows 30-209 are the 16:9 picture). The player draws them as
# flat TILE bands; `stripe` rows alternate the two neighbours one row at a time
# (TILEs again, so still no Gouraud and no dither hunk).
def _r(name, s):
    return pal.ramp(name, s)


SKY_BANDS = {
    "day": [(_r("SKY", 4), 40), (_r("SKY", 5), 50), (_r("SKY", 6), 60), (_r("SKY", 7), 90)],
    "dawn": [(_r("SKY", 5), 34), (_r("SKY", 6), 30), (_r("VAPE", 7), 26), (_r("PASTEL_PINK", 7), 26),
             (_r("PASTEL_PINK", 6), 24), (_r("PEACH", 7), 30), (_r("CANDY", 7), 70)],
    "sunset": [(_r("VAPE", 4), 30), (_r("VAPE", 5), 26), (_r("PASTEL_PINK", 5), 28),
               (_r("PASTEL_PINK", 6), 24), (_r("PEACH", 6), 24), (_r("CANDY", 6), 28),
               (_r("CANDY", 7), 80)],
    "dusk": [(_r("SKY", 1), 30), (_r("SKY", 2), 30), (_r("VAPE", 3), 30), (_r("VAPE", 4), 30),
             (_r("PASTEL_PINK", 4), 24), (_r("PEACH", 5), 96)],
    "night": [(_r("SKY", 0), 60), (_r("SKY", 1), 50), (_r("SKY", 2), 46), (_r("SKY", 3), 84)],
    "dark": [(_r("SKY", 0), 110), (_r("SKY", 1), 130)],
    "vape": [(_r("SKY", 0), 40), (_r("VAPE", 0), 40), (_r("VAPE", 1), 50), (_r("VAPE", 2), 110)],
}


def sky_rows(name, height=240, stripe=True):
    """Expand a band list to one palette index per row (with 1-row stripe seams)."""
    bands = SKY_BANDS[name]
    rows = []
    for k, (c, n) in enumerate(bands):
        rows += [c] * n
    rows = (rows + [bands[-1][0]] * height)[:height]
    if stripe:
        y = 0
        for k in range(1, len(bands)):
            y += bands[k - 1][1]
            a, b = bands[k - 1][0], bands[k][0]
            for j, c in ((y - 3, b), (y - 1, b), (y + 1, a), (y + 3, a)):
                if 0 <= j < height:
                    rows[j] = c
    return rows


def bands_text(name):
    """The band list in .shw notation (index names the compiler resolves)."""
    out = []
    for c, n in SKY_BANDS[name]:
        out.append("%s:%d" % (index_name(c), n))
    return " ".join(out)


def index_name(i):
    """A palette index as a symbol expression: PAL_*, PAINT_X.s or RAMP_X.s."""
    i = int(i)
    fixed = ["PAL_CLEAR", "PAL_BLACK", "PAL_WHITE", "PAL_GRAY_L", "PAL_GRAY", "PAL_GRAY_D",
             "PAL_UI_PINK", "PAL_UI_PINK_D", "PAL_UI_MINT", "PAL_UI_MINT_D", "PAL_UI_CREAM",
             "PAL_UI_RED", "PAL_UI_RED_D", "PAL_UI_GOLD", "PAL_UI_PURPLE", "PAL_UI_LCD"]
    if i < 16:
        return fixed[i]
    if i < 112:
        return "PAINT_%s.%d" % (pal.PAINTS[(i - 16) // 3], (i - 16) % 3)
    if i < 136:
        return "SKIN_%s.%d" % (pal.SKINS[(i - 112) // 3], (i - 112) % 3)
    return "RAMP_%s.%d" % (pal.RAMPS[(i - 136) // 8], (i - 136) % 8)


def parse_index(tok):
    """Inverse of index_name (also accepts a plain number)."""
    tok = tok.strip()
    if tok.isdigit():
        return int(tok)
    fixed = {"PAL_CLEAR": 0, "PAL_BLACK": 1, "PAL_WHITE": 2, "PAL_GRAY_L": 3, "PAL_GRAY": 4,
             "PAL_GRAY_D": 5, "PAL_UI_PINK": 6, "PAL_UI_PINK_D": 7, "PAL_UI_MINT": 8,
             "PAL_UI_MINT_D": 9, "PAL_UI_CREAM": 10, "PAL_UI_RED": 11, "PAL_UI_RED_D": 12,
             "PAL_UI_GOLD": 13, "PAL_UI_PURPLE": 14, "PAL_UI_LCD": 15}
    if tok in fixed:
        return fixed[tok]
    name, _, shade = tok.partition(".")
    kind, _, nm = name.partition("_")
    if kind == "PAINT":
        return pal.paint(nm, int(shade))
    if kind == "SKIN":
        return pal.skin(nm, int(shade))
    if kind == "RAMP":
        return pal.ramp(nm, int(shade))
    raise KeyError("bad palette index '%s'" % tok)
