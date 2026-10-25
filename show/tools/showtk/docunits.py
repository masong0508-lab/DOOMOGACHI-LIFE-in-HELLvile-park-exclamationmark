"""docunits.py - numbers in the script (spec 5.2).

* `doc:N` is a number in the docs' 384x216 units: x5/6 on the default
  320x180 canvas, x1 on the wide one (deferred). Exact (Fraction).
* Frames: `N`, `R8` (a ladder rung), `1.5s` (must be a whole frame count),
  `48000smp` (must be a whole frame count).
* Gains: `-12dB` or `-12`.
"""
import re
from fractions import Fraction

from .errors import Diag, ShowError

FPS = 30
RATE = 48000
SPF = RATE // FPS                  # 1600 samples per frame
CANVAS = (320, 180)
DOC_CANVAS = (384, 216)
DOC_SCALE = Fraction(CANVAS[0], DOC_CANVAS[0])     # 5/6
RUNGS = {"R1": 1, "R2": 2, "R3": 3, "R5": 5, "R8": 8, "R13": 13, "R21": 21, "R26": 26}


def num(s, where=None, scale=DOC_SCALE):
    """'12', '-3.5', 'doc:1.0' -> Fraction (show px)"""
    s = str(s).strip()
    doc = s.startswith("doc:")
    if doc:
        s = s[4:]
    try:
        v = Fraction(s)
    except (ValueError, ZeroDivisionError):
        raise ShowError(Diag("UNITS", "not a number: %r" % s, **(where or {})))
    return v * scale if doc else v


def doc2px(v):
    return num(v) * DOC_SCALE if not str(v).startswith("doc:") else num(v)


def px2doc(v):
    return Fraction(v) / DOC_SCALE


def frames(s, where=None):
    """a duration: 13, R8, 1.5s, 48000smp -> int frames"""
    s = str(s).strip()
    w = where or {}
    if s.upper() in RUNGS:
        return RUNGS[s.upper()]
    m = re.fullmatch(r"(-?\d+(?:\.\d+)?)s", s)
    if m:
        v = Fraction(m.group(1)) * FPS
        if v.denominator != 1:
            raise ShowError(Diag("UNITS", "%s is not a whole number of frames (%s f)" % (s, float(v)), **w))
        return int(v)
    m = re.fullmatch(r"(-?\d+)smp", s)
    if m:
        v = Fraction(int(m.group(1)), SPF)
        if v.denominator != 1:
            raise ShowError(Diag("UNITS", "%s is not a whole number of frames" % s, **w))
        return int(v)
    if re.fullmatch(r"-?\d+", s):
        return int(s)
    raise ShowError(Diag("UNITS", "not a frame count: %r (N, R8, 1.5s or Nsmp)" % s, **w))


def gain_db(s, where=None):
    s = str(s).strip()
    m = re.fullmatch(r"([+-]?\d+(?:\.\d+)?)(dB|db)?", s)
    if not m:
        raise ShowError(Diag("UNITS", "not a gain: %r (e.g. -12dB)" % s, **(where or {})))
    v = float(m.group(1))
    return int(v) if v == int(v) else v


def tc(frame, fps=FPS):
    """frame -> HH:MM:SS:FF"""
    f = int(frame)
    s, ff = divmod(f, fps)
    m, ss = divmod(s, 60)
    h, mm = divmod(m, 60)
    return "%02d:%02d:%02d:%02d" % (h, mm, ss, ff)


def parse_tc(s, where=None):
    m = re.fullmatch(r"(\d+):(\d+):(\d+):(\d+)", s)
    if not m:
        raise ShowError(Diag("UNITS", "not a timecode HH:MM:SS:FF: %r" % s, **(where or {})))
    h, mi, se, fr = map(int, m.groups())
    if fr >= FPS:
        raise ShowError(Diag("UNITS", "timecode frame %d >= %d" % (fr, FPS), **(where or {})))
    return ((h * 60 + mi) * 60 + se) * FPS + fr


def q16(v):
    """Fraction -> nearest Q16 integer"""
    return int(round(Fraction(v) * 65536))
