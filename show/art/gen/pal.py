"""pal.py - the game's 256-colour master palette, read (never edited) from the repo.

Every show plate is an 8-bit index image into this palette (index 0 is
transparent), exactly like the game's own art. The colours are parsed from
core/src/palette.c and decoded the way palette_rgb() decodes them
(BGR555 -> 8-bit, r = r5 << 3 | r5 >> 2), so an index here is the exact RGB
the PS1 shows. The game's generated helper tables (darker/lighter steps, the
TINT_* maps) are parsed from core/gen/art_data.c.
"""
import os
import re

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))


def repo_root():
    d = HERE
    while d != os.path.dirname(d):
        if os.path.isfile(os.path.join(d, "core", "src", "palette.c")):
            return d
        d = os.path.dirname(d)
    raise RuntimeError("repo root (core/src/palette.c) not found above " + HERE)


ROOT = repo_root()

# ---- fixed layout (core/include/doomo/palette.h) -------------------------
CLEAR, BLACK, WHITE, GRAY_L, GRAY, GRAY_D = 0, 1, 2, 3, 4, 5
UI_PINK, UI_PINK_D, UI_MINT, UI_MINT_D, CREAM = 6, 7, 8, 9, 10
UI_RED, UI_RED_D, GOLD, UI_PURPLE, LCD = 11, 12, 13, 14, 15

PAINTS = ["WHITE", "SILVER", "CHARCOAL", "JET", "CHERRY", "BLOOD", "ROSE",
          "BUBBLEGUM", "PEACH", "TANGERINE", "RUST", "HONEY", "LEMON",
          "CHARTREUSE", "LIME", "MINT", "FOREST", "TEAL", "CYAN", "SKY",
          "COBALT", "NAVY", "LAVENDER", "GRAPE", "VAPE_PURPLE", "MAGENTA",
          "CHOCOLATE", "CARAMEL", "BONE", "SLIME", "HELLFIRE", "ECTOPLASM"]
SKINS = ["PORCELAIN", "FAIR", "PEACH", "TAN", "OLIVE", "BROWN", "UMBER", "EBONY"]
RAMPS = ["STONE", "WOOD", "GRASS", "PASTEL_PINK", "SKY", "VAPE", "CANDY",
         "MINT", "PEACH", "BLOOD", "LAVA", "FLESH", "BONE", "SICK", "STEEL"]


def paint(name, shade=1):
    """PAL_PAINT(p, shade): shade 0 dark, 1 base, 2 light."""
    return 16 + PAINTS.index(name.upper()) * 3 + shade


def skin(name, shade=1):
    return 112 + SKINS.index(name.upper()) * 3 + shade


def ramp(name, shade):
    """PAL_RAMP(r, shade): shade 0 darkest .. 7 brightest."""
    shade = max(0, min(7, int(shade)))
    return 136 + RAMPS.index(name.upper()) * 8 + shade


def ramp_list(name, lo=0, hi=7):
    return [ramp(name, s) for s in range(lo, hi + 1)]


def _parse_palette():
    src = open(os.path.join(ROOT, "core", "src", "palette.c")).read()
    body = src[src.index("doomo_palette[256] = {"):]
    body = body[:body.index("};")]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    vals = []
    for m in re.finditer(r"\b(C3?)\s*\(([^)]*)\)", body):
        nums = [int(t) for t in m.group(2).split(",")]
        for k in range(0, len(nums), 3):
            vals.append(tuple(nums[k:k + 3]))
    if len(vals) != 256:
        raise RuntimeError("palette.c: expected 256 colours, parsed %d" % len(vals))
    bgr = np.zeros(256, np.uint16)
    rgb = np.zeros((256, 3), np.uint8)
    for i, (r, g, b) in enumerate(vals):
        r5, g5, b5 = (r >> 3) & 31, (g >> 3) & 31, (b >> 3) & 31
        bgr[i] = r5 | (g5 << 5) | (b5 << 10)
        rgb[i] = ((r5 << 3) | (r5 >> 2), (g5 << 3) | (g5 >> 2), (b5 << 3) | (b5 >> 2))
    return rgb, bgr


RGB, BGR555 = _parse_palette()
LUMA = ((RGB[:, 0].astype(int) * 77 + RGB[:, 1].astype(int) * 150 +
         RGB[:, 2].astype(int) * 29) >> 8)


def _parse_table(name, rows):
    src = open(os.path.join(ROOT, "core", "gen", "art_data.c")).read()
    i = src.index(name)
    body = src[src.index("{", i):]
    depth, end = 0, 0
    for k, ch in enumerate(body):
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                end = k
                break
    nums = [int(t) for t in re.findall(r"\d+", body[:end])]
    a = np.array(nums, np.uint8)
    return a.reshape(rows, 256) if rows > 1 else a


DARKER = _parse_table("doomo_art_darker[256]", 1)
LIGHTER = _parse_table("doomo_art_lighter[256]", 1)
TINTMAP = _parse_table("doomo_art_tintmap[TINT_COUNT][256]", 6)
COLORMAP = _parse_table("doomo_art_colormap[DOOMO_LIGHT_LEVELS][256]", 16)
TINT_NONE, TINT_RED, TINT_GOLD, TINT_GREEN, TINT_PURPLE, TINT_NIGHT = range(6)

# ---- families (mirrors core/art/art_palette.c art_palette_setup) ---------
FAMILY = np.full(256, -1, np.int32)
_fam = 0
for _lst in ([BLACK, GRAY_D, GRAY, GRAY_L, WHITE], [UI_PINK_D, UI_PINK],
             [UI_MINT_D, UI_MINT], [UI_RED_D, UI_RED], [CREAM], [GOLD],
             [UI_PURPLE], [LCD]):
    for _i in _lst:
        FAMILY[_i] = _fam
    _fam += 1
for _k in range(32):
    FAMILY[16 + _k * 3:16 + _k * 3 + 3] = _fam
    _fam += 1
for _k in range(8):
    FAMILY[112 + _k * 3:112 + _k * 3 + 3] = _fam
    _fam += 1
for _k in range(15):
    FAMILY[136 + _k * 8:136 + _k * 8 + 8] = _fam
    _fam += 1


def dist(r, g, b, idx=None):
    """'redmean' distance (palette.c palette_distance) to every entry (or idx)."""
    P = RGB.astype(np.int64) if idx is None else RGB[idx].astype(np.int64)
    rm = (P[..., 0] + r) >> 1
    dr, dg, db = P[..., 0] - r, P[..., 1] - g, P[..., 2] - b
    return (((512 + rm) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rm) * db * db) >> 8)


def nearest(r, g, b):
    d = dist(int(r), int(g), int(b))
    d[0] = 1 << 60
    return int(np.argmin(d))


def nearest_fam(r, g, b, ref, slack_pct=0):
    """nearest_fam() from art_palette.c: stay in ref's family when it is close."""
    r, g, b = [int(max(0, min(255, v))) for v in (r, g, b)]
    if r + g + b < 12:
        return BLACK
    d = dist(r, g, b)
    d[0] = 1 << 60
    gi = int(np.argmin(d))
    fam = np.where(FAMILY == FAMILY[ref])[0]
    fam = fam[fam > 0]
    if len(fam):
        fi = int(fam[np.argmin(d[fam])])
        if d[fi] <= d[gi] + d[gi] * slack_pct // 100 + 120:
            return fi
    return gi


def rgb555_lut():
    """32768-entry map BGR555 -> palette index, or -1 when off-palette (PIC-PAL)."""
    lut = np.full(32768, -1, np.int32)
    for i in range(255, 0, -1):          # lowest index wins on duplicates
        lut[int(BGR555[i])] = i
    return lut


def png_palette():
    """Flat 768-entry RGB list for an indexed PNG."""
    return [int(v) for v in RGB.reshape(-1)]
