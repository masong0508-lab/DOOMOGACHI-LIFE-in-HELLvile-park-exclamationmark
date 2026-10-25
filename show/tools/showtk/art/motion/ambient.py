"""ambient.py - the REFERENCE model of the sets' ambient layers (ART track).

`amb` statements in series/sets/*.shw (grammar: SETS.md sec.6) make every set
move without a single hand-keyed frame. This module is the exact per-frame
math, written in integers so the PS1 module is a line-for-line port (the C
twin is show/art/engine/amb_math.h; tests/art/test_motion.py checks they
agree). Everything is a pure function of (statement, shot frame t, seed):
shots stay pure (spec 8.2) and a seek renders the same pixels.

Kinds
  drift   plane=<p> vx=<px/f> [vy=]            the plane slides by itself (wraps)
  wave    plane=<p> amp=<px> period=<f> len=<rows> [strip=2] [rows=y0..y1]
                                               rows shear in the wind (strings, bunting,
                                               flags, trees); strips of `strip` rows
  pcycle  plane=<p> idx=<I>,<I>,... every=<f>  rotate those CLUT entries (water, glow)
  lamps   plane=<p> | props=<ids> [hold=a..b] [dim=<pct>] [bloom=<pct>] [seed=]
                                               every lantern core / bulb in the plate (found
                                               at bake: WHITE inside CANDY.7) flickers on its
                                               own clock
  sway    props=<ids> amp=<px> period=<f> [lag=<f>] [dy=<px>]
                                               hanging props swing, each `lag` frames behind
  flip    art=<flipbook src> plane=<p> at=x,y [anchor=] [speed=<q>] [phase=<f>] [light=fixed]
  motes   type=firefly|sparkle|ember|dust|petal|snow|bubble|leaf n=<N> area=x,y,w,h
          [plane=<p> | order=front] [seed=] [speed=<q>]
  crowd   form=walk|idle kset=<kset> n=<N> y=<y> x=<x0..x1> [speed=<px/f>] [seed=]
          [plane=<p>] [step=<f>]               background chibis (CRWD lanes / scatter)
Common: `hidden=yes` (a variant's show= turns it on), and variants' hide=/show=
take amb ids like plane ids.
"""
import math

SIN = [int(round(256 * math.sin(2 * math.pi * i / 256))) for i in range(256)]


def sin8(i):
    return SIN[i & 255]


def cos8(i):
    return SIN[(i + 64) & 255]


def hash3(a, b, c):
    """show_l2.h's show_hash3, bit for bit."""
    M = 0xFFFFFFFF
    h = ((a * 0x9E3779B1) & M) ^ (((b + 0x7F4A7C15) * 0x85EBCA77) & M) ^ (((c + 0x165667B1) * 0xC2B2AE3D) & M)
    h &= M
    h ^= h >> 15
    h = (h * 0x2C1B3C6D) & M
    h ^= h >> 12
    h = (h * 0x297A2D39) & M
    h ^= h >> 15
    return h


def q8(v):
    return int(round(float(v) * 256))


def rng2(s):
    a, _, b = str(s).partition("..")
    return int(a), int(b or a)


def wave_dx(amp, period, length, y, t):
    """Row y's x offset at frame t (px)."""
    i = (t * 256 // max(1, period)) + (y * 256 // max(1, length))
    return (amp * sin8(i) + 128) >> 8


def sway_dx(amp, period, t, k, lag):
    p = max(1, period)
    i = (t * 256) // p - (k * lag * 256) // p
    return (amp * sin8(i) + 128) >> 8


def drift_off(v_q8, t):
    return (v_q8 * t) >> 8


def flip_frame(holds, t, speed_q8=256, phase=0):
    """Index of the flipbook drawing at frame t (loops)."""
    total = sum(holds)
    tt = (((t + phase) * speed_q8) >> 8) % total
    for i, h in enumerate(holds):
        if tt < h:
            return i
        tt -= h
    return 0


def lamp_state(seed, i, t, hold=(4, 12), dim=22, bloom=12):
    """0 normal, 1 dim, 2 bloom for lamp i at t. Each lamp has its own hold."""
    span = hold[1] - hold[0] + 1
    h0 = hash3(seed, i, 0xA11)
    hl = hold[0] + h0 % span
    h = hash3(seed, i, (t + h0 % 97) // hl) % 100
    if h < dim:
        return 1
    if h >= 100 - bloom:
        return 2
    return 0


def pcycle_step(t, every, n):
    return (t // max(1, every)) % n


# ---- motes -------------------------------------------------------------------
MOTE_TYPES = ("firefly", "sparkle", "ember", "dust", "petal", "snow", "bubble", "leaf")


def mote(typ, seed, i, t, area, speed_q8=256):
    """-> (x, y, size, colour role) or None (not drawn this frame).
    size: 0 = 1 px, 1 = plus sign, 2 = 2x2, 3 = sparkle cross r3.
    colour role: 'core' | 'hi' | 'dim'; the caller maps roles to indices."""
    x0, y0, w, h = area
    h1, h2, h3 = hash3(seed, i, 1), hash3(seed, i, 2), hash3(seed, i, 3)
    ts = (t * speed_q8) >> 8
    if typ == "firefly":
        bx, by = x0 + h1 % w, y0 + h2 % h
        rx, ry = 6 + h3 % 10, 3 + (h3 >> 8) % 6
        fx, fy = 1 + (h1 >> 12) % 3, 2 + (h2 >> 12) % 3
        x = bx + ((rx * sin8(ts * fx // 2 + (h3 >> 4))) >> 8)
        y = by + ((ry * cos8(ts * fy // 3 + (h2 >> 4))) >> 8)
        cyc = 70 + h3 % 60
        ph = (ts + h1) % cyc
        if ph < 10:
            return None
        if ph < 16 or ph > cyc - 8:
            return (x, y, 0, "dim")
        return (x, y, 1 if ph % 20 < 10 else 0, "core")
    if typ == "sparkle":
        x, y = x0 + h1 % w, y0 + h2 % h
        cyc = 40 + h3 % 50
        ph = (ts + h3) % cyc
        if ph > 9:
            return None
        return (x, y, (0, 1, 3, 3, 1, 1, 0, 0, 0, 0)[ph], "hi" if ph < 6 else "dim")
    if typ in ("ember", "bubble"):
        vy = 64 + h3 % 96                      # Q8 px/f upward
        life = h * 256 // vy + 1
        ph = (ts + h1) % life
        y = y0 + h - ((ph * vy) >> 8)
        x = x0 + h2 % w + ((3 * sin8(ph * 4 + h3)) >> 8)
        if typ == "ember":
            role = "core" if ph < life // 2 else ("hi" if ph < life * 3 // 4 else "dim")
            return (x, y, 0, role)
        return (x, y, 1 if (h3 & 3) == 0 else 0, "hi")
    # falling / drifting kinds
    vx = {"dust": 40, "petal": 70, "snow": 20, "leaf": 90}[typ] + h3 % 48
    vy = {"dust": 10, "petal": 60, "snow": 70, "leaf": 80}[typ] + (h3 >> 6) % 40
    x = x0 + ((h1 % (w * 256) + ts * vx) >> 8) % w
    y = y0 + ((h2 % (h * 256) + ts * vy) >> 8) % h
    x += (2 * sin8(ts * 3 + h3)) >> 8
    size = 2 if typ in ("petal", "leaf") and (h1 & 1) else 0
    return (x, y, size, "core" if (h2 & 3) else "hi")


MOTE_COLOURS = {
    # role -> master index name (light.parse_index)
    "firefly": {"core": "RAMP_CANDY.7", "hi": "PAL_WHITE", "dim": "PAINT_LEMON.0"},
    "sparkle": {"core": "PAL_WHITE", "hi": "PAL_WHITE", "dim": "RAMP_SKY.7"},
    "ember": {"core": "PAINT_HELLFIRE.2", "hi": "PAINT_TANGERINE.1", "dim": "PAINT_RUST.0"},
    "dust": {"core": "RAMP_STONE.6", "hi": "RAMP_STONE.7", "dim": "RAMP_STONE.5"},
    "petal": {"core": "PAINT_BUBBLEGUM.2", "hi": "PAINT_ROSE.2", "dim": "PAINT_BUBBLEGUM.1"},
    "snow": {"core": "PAL_WHITE", "hi": "PAL_WHITE", "dim": "RAMP_SKY.7"},
    "bubble": {"core": "RAMP_SKY.7", "hi": "PAL_WHITE", "dim": "RAMP_SKY.6"},
    "leaf": {"core": "PAINT_CHARTREUSE.1", "hi": "PAINT_HONEY.1", "dim": "PAINT_FOREST.1"},
}


# ---- crowds ---------------------------------------------------------------------
def walker(i, t, seed, x0, x1, y, speed_q8, step=4, jitter=6, n=8):
    """Member i of a walking lane: (x, y, dir, frame). Odd members walk the
    other way (dir 1 = left, 2 = right); they wrap off one edge to the other
    with a margin, so nobody pops in frame."""
    h1, h2 = hash3(seed, i, 7), hash3(seed, i, 8)
    span = (x1 - x0) + 40
    v = speed_q8 + (h2 % 64) - 32
    left = (i & 1) == 1
    slot = span * 256 // max(1, n)
    pos = (i * slot + h1 % slot + t * v) >> 8
    pos %= span
    x = (x1 + 20 - pos) if left else (x0 - 20 + pos)
    yy = y + (h2 >> 8) % (jitter + 1) - jitter // 2
    frame = ((t + h1 % 16) // max(1, step)) % 4
    return x, yy, 1 if left else 2, frame


def idler(i, t, seed, x0, x1, y, depth=10):
    """Member i of an idle cluster: (x, y, dir, frame, bob). Faces change now
    and then (a glance), bob 1 px on its own breath."""
    h1, h2, h3 = hash3(seed, i, 11), hash3(seed, i, 12), hash3(seed, i, 13)
    x = x0 + h1 % max(1, x1 - x0)
    yy = y + h2 % (depth + 1)
    look_hold = 60 + h3 % 120
    k = (t + h3) // look_hold
    d = (0, 0, 1, 2, 0, 3)[hash3(seed, i, k) % 6] if (h3 & 1) else (0, 1, 0, 2)[hash3(seed, i, k) % 4]
    bob = 1 if ((t + h2) // (20 + h1 % 12)) & 1 else 0
    return x, yy, d, 0, bob
