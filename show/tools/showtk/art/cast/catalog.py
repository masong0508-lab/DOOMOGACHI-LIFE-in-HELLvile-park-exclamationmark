"""showtk.art.cast.catalog - the game's catalogue as Python tables (ART track).

Every enum the show's cast data may name (SPECIES_*, EYE_*, MOUTH_*, EXPR_*,
PAINT_*, SKIN_*, CHEEK_*, RES_*, ...) is read from the game's own headers
with the parser of core/tools/textpack.py (imported read-only), so the show
and the game can never disagree on a value.  `load_symbols()` in textpack
does not read catalog.h or resident.h, so this module adds them.

The DoomoLook field order is read from the struct in resident.h, and the
48-byte size is checked, so a packed look is exactly what the PS1 reads.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
CORE = os.path.join(ROOT, "core")
INC = os.path.join(CORE, "include", "doomo")

sys.path.insert(0, os.path.join(CORE, "tools"))
import textpack as _tp  # noqa: E402  (read-only import of the game's tool)

EXTRA = ["catalog.h", "resident.h", "palette.h", "portrait.h", "pet.h", "sprites.h", "actor.h"]

# The look fields, keyed by the roster text's names (docs/ROSTER.md), which
# the spec's cast.shw excerpt uses (hair=, hat=, accessory=, facial_hair=).
# value: (DoomoLook struct field, kind, enum prefix or range)
FIELDS = {
    "species":         ("species",         "enum",  "SPECIES_"),
    "body":            ("body",            "enum",  "BODY_"),
    "height":          ("height",          "int",   (0, 15)),
    "build":           ("build",           "int",   (0, 15)),
    "skin":            ("skin",            "skin",  None),
    "head":            ("head_shape",      "enum",  "HEAD_"),
    "head_size":       ("head_size",       "int",   (0, 15)),
    "eyes":            ("eye_style",       "enum",  "EYE_"),
    "eye_color":       ("eye_color",       "paint", None),
    "eye_size":        ("eye_size",        "int",   (0, 15)),
    "eye_spacing":     ("eye_spacing",     "int",   (0, 15)),
    "eye_height":      ("eye_height",      "int",   (0, 15)),
    "eye_tilt":        ("eye_tilt",        "int",   (0, 15)),
    "eye_count":       ("eye_count",       "int",   (1, 3)),
    "brows":           ("brow_style",      "enum",  "BROW_"),
    "brow_height":     ("brow_height",     "int",   (0, 15)),
    "nose":            ("nose_style",      "enum",  "NOSE_"),
    "nose_size":       ("nose_size",       "int",   (0, 15)),
    "mouth":           ("mouth_style",     "enum",  "MOUTH_"),
    "mouth_size":      ("mouth_size",      "int",   (0, 15)),
    "fangs":           ("fangs",           "int",   (0, 3)),
    "hair":            ("hair_style",      "enum",  "HAIR_"),
    "hair_color":      ("hair_color",      "paint", None),
    "highlight":       ("hair_highlight",  "paint_or_none", None),
    "facial_hair":     ("facial_hair",     "enum",  "FHAIR_"),
    "ears":            ("ears",            "enum",  "EARS_"),
    "horns":           ("horns",           "enum",  "HORNS_"),
    "horn_color":      ("horn_color",      "paint", None),
    "horn_size":       ("horn_size",       "int",   (0, 15)),
    "tail":            ("tail",            "enum",  "TAIL_"),
    "wings":           ("wings",           "enum",  "WINGS_"),
    "halo":            ("halo",            "enum",  "HALO_"),
    "marking":         ("marking",         "enum",  "MARK_"),
    "marking_color":   ("marking_color",   "paint", None),
    "cheeks":          ("cheeks",          "cheeks", "CHEEK_"),
    "top":             ("top",             "enum",  "TOP_"),
    "top_color":       ("top_color",       "paint", None),
    "bottom":          ("bottom",          "enum",  "BOTTOM_"),
    "bottom_color":    ("bottom_color",    "paint", None),
    "shoes":           ("shoes",           "enum",  "SHOES_"),
    "shoes_color":     ("shoes_color",     "paint", None),
    "hat":             ("hat",             "enum",  "HAT_"),
    "hat_color":       ("hat_color",       "paint", None),
    "accessory":       ("accessory",       "enum",  "ACC_"),
    "accessory_color": ("accessory_color", "paint", None),
    "aura":            ("aura",            "enum",  "AURA_"),
    "pattern":         ("pattern",         "enum",  "PATTERN_"),
    "pattern_color":   ("pattern_color",   "paint", None),
}
# struct-field spellings are accepted too (eye_style=, hair_style=, ...)
ALIASES = {v[0]: k for k, v in FIELDS.items() if v[0] != k}

LOOK_NO_COLOR = 0xFF


class Catalog:
    """Symbol values and name tables for every catalogue list."""

    def __init__(self):
        syms = _tp.load_symbols()
        for h in EXTRA:
            path = os.path.join(INC, h)
            if not os.path.exists(path):
                continue
            src = _tp.strip_comments(open(path).read())
            for m in re.finditer(r"enum\s*\w*\s*\{(.*?)\}", src, flags=re.S):
                cur = -1
                for item in m.group(1).split(","):
                    item = item.strip()
                    if not item:
                        continue
                    if "=" in item:
                        name, val = item.split("=", 1)
                        name = name.strip()
                        v = _tp.c_eval(val, syms)
                        cur = v if v is not None else cur + 1
                    else:
                        name = item
                        cur += 1
                    if re.match(r"^[A-Za-z_]\w*$", name):
                        syms.setdefault(name, cur)
            for m in re.finditer(r"#define[ \t]+([A-Z_][A-Z0-9_]*)[ \t]+([^\n(][^\n]*)", src):
                v = _tp.c_eval(m.group(2), syms)
                if v is not None:
                    syms.setdefault(m.group(1), v)
        self.syms = syms
        self.look_order = self._look_order()
        if len(self.look_order) != 48:
            raise RuntimeError("resident.h: DoomoLook has %d fields, expected 48" % len(self.look_order))
        self.paint_count = syms["PAL_PAINT_COUNT"]

    @staticmethod
    def _look_order():
        src = _tp.strip_comments(open(os.path.join(INC, "resident.h")).read())
        m = re.search(r"typedef struct DoomoLook \{(.*?)\} DoomoLook;", src, flags=re.S)
        return re.findall(r"uint8_t\s+(\w+)\s*;", m.group(1))

    def enum(self, prefix, count=None):
        """{FULL_NAME: value} for one list, without the *_COUNT terminator."""
        cnt = self.syms.get(count or (prefix + "COUNT"))
        out = {}
        for k, v in self.syms.items():
            if k.startswith(prefix) and not k.endswith("_COUNT") and k != prefix + "COUNT":
                if cnt is None or 0 <= v < cnt:
                    out[k] = v
        return out

    def name_of(self, prefix, value):
        for k, v in self.enum(prefix).items():
            if v == value:
                return k
        return "%s?%d" % (prefix, value)

    def count(self, prefix):
        return self.syms[prefix + "COUNT"]

    def paints(self):
        return {k: v for k, v in self.syms.items()
                if k.startswith("PAINT_") and 0 <= v < self.paint_count}

    def skins(self):
        return {k: v for k, v in self.syms.items()
                if k.startswith("SKIN_") and 0 <= v < self.syms["PAL_SKIN_COUNT"]}

    def value(self, name):
        return self.syms.get(name)


_CAT = None


def catalog():
    global _CAT
    if _CAT is None:
        _CAT = Catalog()
    return _CAT
