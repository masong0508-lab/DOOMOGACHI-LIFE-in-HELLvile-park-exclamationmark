"""camera.py - the stage camera: `cam` statements -> ShowCamKey records, and the
camera as a pure function of the shot frame, computed exactly as show_cam.c
does (integer cam units, the same eases, the same divisions), so asserts,
`show timing` and the lints see the pixels the PS1 draws.

Statements (spec 5.5 "Camera"; README "Staging reference: camera"):
  setup   cam at=X,Y [scale=S] [about=X,Y]
          cam drift dx=R [dy=R] [zero=+N]       (rates in px/f; position 0 at +N)
  timed   +T cam to [x=X] [y=Y] [scale=S] over=F [ease=INOUT|IN|OUT|LINEAR]
          +T cam pan dx=R [dy=R]                (a constant rate from +T)
          +T cam pan dx=D [dy=D] over=F [ease=] (a relative move of D px)
          +T cam creep dx=R [dy=R] [ramp=13]    (a slow rate that eases in)
          +T cam whip L|R [over=6] [dist=320]   (a whip pan, speed lines on)
          +T cam crane dy=D over=F [ease=]      (a vertical move: tilts, cranes)
          +T cam stop                           (hold where it is)
Numbers take `doc:` (x5/6, the docs' 384x216 canvas) and decimals; a rate
must be a whole number of 1/3600 px per frame (every rate the docs use is).
"""
from fractions import Fraction

from . import fmt
from .fmt import CAM_CU, Q16, CK_SET, CK_MOVE, CK_VEL, CK_CREEP, CA_X, CA_Y, CA_S, CE, CKF_WHIP


class CamError(ValueError):
    pass


def num(v, doc_ratio=Fraction(5, 6)):
    """'12', '-0.5', 'doc:1.0' -> Fraction (show px)."""
    s = str(v).strip()
    if s.startswith("doc:"):
        return Fraction(s[4:]) * doc_ratio
    return Fraction(s)


def to_cu(v, what="value", exact=False):
    f = num(v) * CAM_CU if not isinstance(v, Fraction) else v * CAM_CU
    if f.denominator != 1:
        if exact:
            raise CamError("%s %s is not a whole number of 1/3600 px" % (what, v))
        return int(round(f))
    return int(f)


def to_q16(v):
    return int(round(num(v) * Q16))


def par_frac(v):
    """'0.05' -> (1, 20); '1' -> (1, 1); '0' -> (0, 1)."""
    f = Fraction(str(v)).limit_denominator(10000)
    if f.denominator > 32767 or abs(f.numerator) > 32767:
        raise CamError("parallax %s is too fine" % v)
    return f.numerator, f.denominator


# ---- the C arithmetic, mirrored --------------------------------------------------
def _cdiv(n, d):                     # C99 '/' (truncates toward zero)
    q = abs(n) // abs(d)
    return q if (n >= 0) == (d >= 0) else -q


def _ease(kind, t):
    one = Q16
    if kind == CE["IN"]:
        return _cdiv(t * t, one)
    if kind == CE["OUT"]:
        return one - _cdiv((one - t) * (one - t), one)
    if kind == CE["INOUT"]:
        return _cdiv(_cdiv(t * t, one) * (3 * one - 2 * t), one)
    if kind == CE["WHIP"]:
        if t < one // 2:
            return _cdiv(_cdiv(4 * t * t, one) * t, one)
        u = 2 * one - 2 * t
        return one - _cdiv(_cdiv(_cdiv(u * u, one) * u, one), 2)
    return t


class Key:
    __slots__ = ("t0", "t1", "kind", "axis", "ease", "flags", "a", "b", "c")

    def __init__(self, t0, t1, kind, axis, a, b=0, c=0, ease=0, flags=0):
        self.t0, self.t1, self.kind, self.axis = int(t0), int(t1), kind, axis
        self.a, self.b, self.c, self.ease, self.flags = int(a), int(b), int(c), ease, flags

    def value(self, t):
        if self.kind == CK_MOVE:
            if t >= self.t1:
                return self.a
            f = (t - self.t0 + 1) * Q16 // (self.t1 - self.t0 + 1)
            return self.b + _cdiv((self.a - self.b) * _ease(self.ease, f), Q16)
        if self.kind == CK_VEL:
            return self.b + self.a * (t - self.c)
        if self.kind == CK_CREEP:
            tau, r = t - self.t0, max(1, self.c)
            if tau <= 0:
                return self.b
            if tau < r:
                return self.b + (self.a * tau * tau) // (2 * r)
            return self.b + (self.a * (2 * tau - r)) // 2
        return self.a

    def pack(self):
        return fmt.camkey(self.t0, self.t1, self.kind, self.axis, self.a, self.b, self.c,
                          self.ease, self.flags)


class Camera:
    """One shot's camera: setup + keys, evaluated like show_cam.c."""

    def __init__(self):
        self.x = self.y = 0
        self.s = Q16
        self.cx = self.cy = 0
        self.keys = []

    # -- evaluation (show_cam_axis) --
    def axis(self, t, axis, keys=None):
        last = None
        for k in sorted(self.keys if keys is None else keys, key=lambda k: k.t0):
            if k.t0 > t:
                break
            if k.axis == axis:
                last = k
        if last is not None:
            return last.value(t)
        return self.x if axis == CA_X else self.y if axis == CA_Y else self.s

    def at(self, t):
        return self.axis(t, CA_X), self.axis(t, CA_Y), self.axis(t, CA_S)

    def _from(self, t0, axis):
        return self.axis(t0 - 1 if t0 > 0 else 0, axis)

    # -- statements --
    def setup_at(self, x=None, y=None, scale=None, about=None):
        if x is not None:
            self.x = to_cu(x)
        if y is not None:
            self.y = to_cu(y)
        if scale is not None:
            self.s = to_q16(scale)
        if about is not None:
            self.cx, self.cy = int(num(about[0])), int(num(about[1]))

    def drift(self, dx=None, dy=None, zero=0):
        for v, axis, base in ((dx, CA_X, self.x), (dy, CA_Y, self.y)):
            if v is None:
                continue
            self.keys.append(Key(0, 0, CK_VEL, axis, to_cu(v, "drift rate", exact=True), base, zero))

    def to(self, t0, over, x=None, y=None, scale=None, ease="INOUT", flags=0):
        t1 = t0 + max(1, int(over)) - 1
        e = CE[ease.upper()]
        for v, axis, conv in ((x, CA_X, to_cu), (y, CA_Y, to_cu), (scale, CA_S, to_q16)):
            if v is None:
                continue
            target = conv(v)
            self.keys.append(Key(t0, t1, CK_MOVE, axis, target, self._from(t0, axis), 0, e, flags))

    def by(self, t0, over, dx=None, dy=None, ease="INOUT", flags=0):
        t1 = t0 + max(1, int(over)) - 1
        e = CE[ease.upper()]
        for v, axis in ((dx, CA_X), (dy, CA_Y)):
            if v is None:
                continue
            start = self._from(t0, axis)
            self.keys.append(Key(t0, t1, CK_MOVE, axis, start + to_cu(v), start, 0, e, flags))

    def pan(self, t0, dx=None, dy=None):
        for v, axis in ((dx, CA_X), (dy, CA_Y)):
            if v is None:
                continue
            c = t0 - 1
            self.keys.append(Key(t0, t0, CK_VEL, axis, to_cu(v, "pan rate", exact=True),
                                 self.axis(c if c >= 0 else 0, axis), c))

    def creep(self, t0, dx=None, dy=None, ramp=13):
        for v, axis in ((dx, CA_X), (dy, CA_Y)):
            if v is None:
                continue
            self.keys.append(Key(t0, t0, CK_CREEP, axis, to_cu(v, "creep rate", exact=True),
                                 self._from(t0, axis), int(ramp)))

    def whip(self, t0, direction, over=6, dist=320):
        d = num(dist) * (1 if direction.upper().startswith("R") else -1)
        self.by(t0, over, dx=d, ease="WHIP", flags=CKF_WHIP)

    def crane(self, t0, dy, over, ease="INOUT"):
        self.by(t0, over, dy=dy, ease=ease)

    def stop(self, t0):
        for axis in (CA_X, CA_Y):
            self.keys.append(Key(t0, t0, CK_SET, axis, self.axis(t0, axis)))

    # -- output --
    def pack(self):
        keys = sorted(self.keys, key=lambda k: k.t0)          # stable: list order on ties
        return fmt.camsetup(self.x, self.y, self.s, self.cx, self.cy), [k.pack() for k in keys]


# ---- the plane transform (show_cam_plane), for asserts and lints ------------------
def plane_origin(cam_x_cu, par, at, shake=0, s_plane=Q16, c=160):
    """Floored show-px x of plane pixel 0 (one axis), as show_cam.c's axis_xf()."""
    pn, pd = par
    base = (at + shake) * CAM_CU - (cam_x_cu * pn) // pd
    if s_plane == Q16:
        return base // CAM_CU
    q16 = c * Q16 + (((base * Q16) // CAM_CU - c * Q16) * s_plane) // Q16
    return q16 // Q16
