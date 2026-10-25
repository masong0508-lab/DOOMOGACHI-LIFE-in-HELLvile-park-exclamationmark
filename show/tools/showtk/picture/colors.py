"""colors.py - palette names for staging statements (color=, outline=, nib=):

    WHITE  BLACK  GRAY_L ... UI_CREAM ...    the fixed entries (PAL_*)
    PAINT_ROSE:1   ROSE:1                    paint, shade 0..2 (PAL_PAINT)
    RAMP_CANDY:6   CANDY:6                   ramp, shade 0..7 (PAL_RAMP)
    crayon:<ink>                             an ART crayon ink, when ART's kit is there
    42                                       a raw index

Read from core/include/doomo/palette.h, so the names are the game's.
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
PALETTE_H = os.path.join(REPO, "core", "include", "doomo", "palette.h")
PAINT_BASE, RAMP_BASE = 16, 136

_FIXED = _PAINTS = _RAMPS = None


def _enum_after(text, marker):
    i = text.index(marker)
    body = text[text.rindex("{", 0, i) + 1:text.index("}", i)]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return [t.split("=")[0].strip() for t in body.split(",") if t.strip()]


def _load():
    global _FIXED, _PAINTS, _RAMPS
    if _FIXED is not None:
        return
    text = open(PALETTE_H).read()
    fixed = _enum_after(text, "PAL_CLEAR = 0")
    _FIXED = {n[4:]: i for i, n in enumerate(fixed)}
    _PAINTS = {n[6:]: i for i, n in enumerate(_enum_after(text, "PAINT_WHITE, PAINT_SILVER"))}
    _RAMPS = {n[5:]: i for i, n in enumerate(_enum_after(text, "RAMP_STONE, RAMP_WOOD"))}


class ColorError(ValueError):
    pass


def index(name):
    """A palette name or number -> 0..255."""
    _load()
    s = str(name).strip()
    if re.fullmatch(r"\d+", s):
        v = int(s)
        if not 0 <= v <= 255:
            raise ColorError("palette index %s is out of 0..255" % s)
        return v
    if s.lower() in ("none", "off", "0"):
        return 0
    if s.startswith("crayon:"):
        return _crayon(s[7:])
    s = s[4:] if s.startswith("PAL_") else s
    base, _, shade = s.partition(":")
    if base in _FIXED and not shade:
        return _FIXED[base]
    b = base[6:] if base.startswith("PAINT_") else base
    if b in _PAINTS and not base.startswith("RAMP_"):
        k = int(shade or 1)
        if not 0 <= k <= 2:
            raise ColorError("paint shade %d is out of 0..2" % k)
        return PAINT_BASE + _PAINTS[b] * 3 + k
    b = base[5:] if base.startswith("RAMP_") else base
    if b in _RAMPS:
        k = int(shade or 4)
        if not 0 <= k <= 7:
            raise ColorError("ramp shade %d is out of 0..7" % k)
        return RAMP_BASE + _RAMPS[b] * 8 + k
    raise ColorError("unknown colour '%s' (WHITE, PAINT_ROSE:1, RAMP_CANDY:6, crayon:<ink>, or 0..255)" % name)


def _crayon(ink):
    """ART's crayon inks (show/art/gen/crayon.py), when that package is on the path."""
    try:
        from . import bake
        cr = bake.art_module("crayon")
    except Exception as e:                       # pragma: no cover - ART not landed
        raise ColorError("crayon:%s needs ART's show/art/gen (%s)" % (ink, e))
    name = ink[7:].lower() if ink.upper().startswith("CRAYON_") else ink.lower()
    try:
        return int(cr.ink(name))
    except Exception:
        try:
            return int(cr.ink(ink))
        except Exception as e:
            raise ColorError("unknown crayon ink '%s' (%s)" % (ink, e))
