#!/usr/bin/env python3
# doomo-generator
"""sfxgen.py - procedural sound effects for every SFX_* (owner: story).

Synthesises signed 8-bit mono PCM for every SFX_* in
core/include/doomo/audio.h and writes const tables to

    core/gen/sfx_data.c   (doomo_sfx_pcm_data, doomo_sfx_info)

so the samples live in ROM on the GBA and cost no RAM. Two families:
CUTE (cursor blips, sparkles, squeaks, bubbles, hearts, the egg, the bell)
and BRUTAL (shotgun, chaingun, rockets, chainsaw, explosions, splats,
demon roars). Total size is kept under AUDIO_SFX_BUDGET (48 KB).

    python3 tools/sfxgen.py [--out-root DIR] [--check] [--wav DIR]

--out-root DIR  write DIR/core/gen/sfx_data.c (default: the repository)
--check         synthesise and report sizes only
--wav DIR       also write DIR/<name>.wav files to listen to

INTEGER MATH ONLY (no floats, no math module): the output must be
byte-identical on every machine (make -C core check-gen).
"""
import os
import re
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
AUDIO_H = os.path.join(ROOT, "core", "include", "doomo", "audio.h")
BUDGET = 48 * 1024

HI = 10512   # the GBA mixer rate: no resampling at all on hardware
MID = 8000
LO = 6000    # crunchy lo-fi for the brutal booms

ONE = 32767


# ---------------------------------------------------------------------------
# deterministic integer helpers
class Rng(object):
    """xorshift32, same as the core's rng.c core step."""

    def __init__(self, seed):
        self.s = (seed * 2654435761 + 12345) & 0xFFFFFFFF or 1

    def next(self):
        s = self.s
        s ^= (s << 13) & 0xFFFFFFFF
        s ^= s >> 17
        s ^= (s << 5) & 0xFFFFFFFF
        self.s = s
        return s

    def range(self, n):
        return self.next() % n


def _sin_table():
    """Quarter-free full sine table, 1024 entries, Bhaskara I in integers."""
    tab = []
    P = 512  # half period in table steps
    for i in range(1024):
        x = i % P
        num = 16 * x * (P - x)
        den = 5 * P * P - 4 * x * (P - x)
        v = (num * ONE) // den
        tab.append(v if i < P else -v)
    return tab


SIN = _sin_table()


def _iroot(n, k):
    """floor(n ** (1/k)) for big ints."""
    lo, hi = 0, 1
    while hi ** k <= n:
        hi *= 2
    while lo < hi - 1:
        mid = (lo + hi) // 2
        if mid ** k <= n:
            lo = mid
        else:
            hi = mid
    return lo


# 2^(i/192) for i in 0..191 (1/16 semitone steps), Q24
Q = 1 << 24
SEMI16 = [_iroot((2 ** i) * (Q ** 192), 192) for i in range(192)]


def ratio_q24(steps16):
    """Pitch ratio for a signed number of 1/16-semitone steps, Q24."""
    octv, rem = divmod(steps16, 192)
    r = SEMI16[rem]
    if octv >= 0:
        return r << octv
    return r >> (-octv)


def hz256(semitone_from_a4):
    """Frequency (Hz * 256) of a note given in semitones from A4 (440)."""
    return (440 * 256 * ratio_q24(semitone_from_a4 * 16)) >> 24


def note(name):
    """'C5' / 'F#4' / 'Bb3' -> Hz*256"""
    m = re.match(r"^([A-Ga-g])([#b]?)(-?\d)$", name)
    base = {"C": -9, "D": -7, "E": -5, "F": -4, "G": -2, "A": 0, "B": 2}[m.group(1).upper()]
    if m.group(2) == "#":
        base += 1
    elif m.group(2) == "b":
        base -= 1
    return hz256(base + (int(m.group(3)) - 4) * 12)


# ---------------------------------------------------------------------------
# oscillators: phase is a 32-bit accumulator, outputs -32767..32767
def osc(wave, ph, duty=0x80000000):
    ph &= 0xFFFFFFFF
    if wave == "sine":
        return SIN[ph >> 22]
    if wave == "square":
        return ONE if ph < duty else -ONE
    if wave == "saw":
        return (ph >> 16) - 32768
    if wave == "tri":
        v = ph >> 16  # 0..65535
        return v * 2 - 32767 if v < 32768 else (65535 - v) * 2 - 32767
    raise ValueError(wave)


def inc_for(freq256, rate):
    return ((freq256 << 24) // rate) & 0xFFFFFFFF


def tone(rate, ms, f0, f1=None, wave="square", duty=0x80000000, vol=100,
         attack=2, curve=None, vib=0, vib_hz=6, sweep="exp", start_ms=0,
         decay=None, total_ms=None, tremolo=0, trem_hz=8):
    """Render one tone. f0/f1 Hz*256; vib in 1/16 semitones; vol 0..100.
    decay: per-ms exponential decay factor in 1/65536 (None = linear fade
    over the last 40%). Returns a list of ints aligned at start_ms within
    total_ms (zero padded)."""
    n = rate * ms // 1000
    if f1 is None:
        f1 = f0
    out = []
    ph = 0
    vph = 0
    tph = 0
    vinc = inc_for(vib_hz * 256, rate)
    tinc = inc_for(trem_hz * 256, rate)
    att = max(1, rate * attack // 1000)
    env = 65536
    dmul = None
    if decay is not None:
        # per-sample multiplier: decay is per ms, convert roughly
        per_sample = 1000 * 65536 // rate
        dmul = 65536 - ((65536 - decay) * per_sample // 65536)
    # exponential sweep: interpolate in pitch space
    if f0 > 0 and f1 > 0 and sweep == "exp":
        # find steps between f0 and f1 in 1/16 semitones (integer search)
        steps = 0
        if f1 != f0:
            sign = 1 if f1 > f0 else -1
            while True:
                nf = (f0 * ratio_q24((steps + sign) * 1)) >> 24
                if (sign > 0 and nf > f1) or (sign < 0 and nf < f1):
                    break
                steps += sign
                if abs(steps) > 192 * 8:
                    break
    for i in range(n):
        if f0 > 0 and f1 > 0 and sweep == "exp" and f1 != f0:
            s16 = steps * i // max(1, n - 1)
            f = (f0 * ratio_q24(s16)) >> 24
        else:
            f = f0 + (f1 - f0) * i // max(1, n - 1)
        if vib:
            v = SIN[vph >> 22] * vib // ONE
            f = (f * ratio_q24(v)) >> 24
            vph = (vph + vinc) & 0xFFFFFFFF
        ph = (ph + inc_for(max(f, 1), rate)) & 0xFFFFFFFF
        s = osc(wave, ph, duty)
        # envelope
        a = 65536 if i >= att else i * 65536 // att
        if dmul is not None:
            if i >= att:
                env = env * dmul >> 16
            e = a * env >> 16
        else:
            tail = n * 6 // 10
            e = a if i < tail else a * (n - i) // max(1, n - tail)
        if curve:
            e = e * curve(i, n) >> 16
        if tremolo:
            t = (SIN[tph >> 22] + ONE) // 2  # 0..32767
            e = e * (100 - tremolo * (ONE - t) // ONE) // 100
            tph = (tph + tinc) & 0xFFFFFFFF
        out.append(s * e // 65536 * vol // 100)
    return place(out, rate, start_ms, total_ms)


def place(samples, rate, start_ms, total_ms):
    pre = [0] * (rate * start_ms // 1000)
    out = pre + samples
    if total_ms is not None:
        n = rate * total_ms // 1000
        out = (out + [0] * n)[:n]
    return out


def noise(rate, ms, seed, vol=100, hold_hz=None, hold_hz1=None, lp0=256, lp1=None,
          attack=1, decay=None, start_ms=0, total_ms=None, hp=0):
    """Noise burst: sample & hold at hold_hz (Hz, sweeping to hold_hz1),
    one-pole low-pass sweeping lp0->lp1 (coef 1..256, 256 = open)."""
    n = rate * ms // 1000
    rng = Rng(seed)
    out = []
    held = 0
    hacc = 0
    y = 0
    yh = 0
    att = max(1, rate * attack // 1000)
    env = 65536
    dmul = None
    if decay is not None:
        per_sample = 1000 * 65536 // rate
        dmul = 65536 - ((65536 - decay) * per_sample // 65536)
    if lp1 is None:
        lp1 = lp0
    for i in range(n):
        hz = hold_hz if hold_hz1 is None or hold_hz is None else hold_hz + (hold_hz1 - hold_hz) * i // max(1, n - 1)
        if hz is None:
            held = (rng.next() & 0xFFFF) - 32768
        else:
            hacc += hz
            while hacc >= rate:
                hacc -= rate
                held = (rng.next() & 0xFFFF) - 32768
        k = lp0 + (lp1 - lp0) * i // max(1, n - 1)
        y += (held - y) * k // 256
        s = y
        if hp:
            yh += (s - yh) * hp // 256
            s = s - yh
        a = 65536 if i >= att else i * 65536 // att
        if dmul is not None:
            if i >= att:
                env = env * dmul >> 16
            e = a * env >> 16
        else:
            tail = n * 5 // 10
            e = a if i < tail else a * (n - i) // max(1, n - tail)
        out.append(s * e // 65536 * vol // 100)
    return place(out, rate, start_ms, total_ms)


def mix(*tracks):
    n = max(len(t) for t in tracks)
    out = [0] * n
    for t in tracks:
        for i, v in enumerate(t):
            out[i] += v
    return out


def gain(t, pct):
    return [v * pct // 100 for v in t]


def crush(t, bits):
    step = 1 << (16 - bits)
    return [(v // step) * step for v in t]


def lowpass(t, k):
    y = 0
    out = []
    for v in t:
        y += (v - y) * k // 256
        out.append(y)
    return out


def drive(t, amount):
    """soft-ish saturation: amount 100 = clean, 300 = crunchy."""
    out = []
    for v in t:
        x = v * amount // 100
        if x > 32767:
            x = 32767 - (32767 * 32767) // (x + 1) // 8
        elif x < -32767:
            x = -32767 + (32767 * 32767) // (-x + 1) // 8
        out.append(x)
    return out


def to_pcm8(t, peak=124):
    """Normalise to the peak and convert to signed 8-bit."""
    m = max(1, max(abs(v) for v in t))
    out = []
    for v in t:
        s = v * peak // m
        out.append(max(-127, min(127, s)))
    # trim trailing near-silence
    while len(out) > 8 and abs(out[-1]) <= 3:
        out.pop()
    return out


def fade_out(t, ms, rate):
    n = len(t)
    k = min(n, rate * ms // 1000)
    out = list(t)
    for i in range(k):
        out[n - k + i] = out[n - k + i] * (k - i) // k
    return out


# ---------------------------------------------------------------------------
# the recipes
def arp_notes(rate, names, step_ms, wave="square", duty=0x40000000, vol=100, decay=64000,
              tail_ms=120, vib=0):
    total = step_ms * len(names) + tail_ms
    parts = []
    for k, nm in enumerate(names):
        parts.append(tone(rate, step_ms + tail_ms, note(nm), wave=wave, duty=duty, vol=vol,
                          decay=decay, start_ms=k * step_ms, total_ms=total, vib=vib))
    return mix(*parts)


def sfx_cursor():
    r = HI
    return r, mix(tone(r, 38, note("E6"), note("A6"), wave="square", duty=0x40000000, decay=60000),
                  tone(r, 38, note("E7"), wave="sine", vol=30, decay=58000))


def sfx_select():
    r = MID
    return r, mix(arp_notes(r, ["C6", "G6"], 34, duty=0x40000000, tail_ms=50),
                  tone(r, 110, note("C7"), wave="sine", vol=35, decay=62000, start_ms=34, total_ms=118))


def sfx_cancel():
    r = MID
    return r, arp_notes(r, ["G5", "C5"], 40, duty=0x20000000, tail_ms=50)


def sfx_error():
    r = LO
    a = tone(r, 70, note("D3"), wave="square", duty=0x60000000, decay=63000, total_ms=180)
    b = tone(r, 90, note("C#3"), wave="square", duty=0x60000000, decay=63000, start_ms=85, total_ms=180)
    return r, drive(mix(a, b), 140)


def sfx_blip():
    r = HI
    return r, tone(r, 24, note("A6"), note("D7"), wave="tri", decay=55000)


def sfx_eat():
    r = LO
    parts = []
    for k in range(3):
        st = k * 75
        parts.append(noise(r, 45, 11 + k, hold_hz=2400, lp0=120, lp1=40, decay=60000, start_ms=st, total_ms=240))
        parts.append(tone(r, 55, note("E4") + k * 900, note("A3"), wave="square", duty=0x50000000,
                          vol=60, decay=61000, start_ms=st, total_ms=240))
    parts.append(tone(r, 60, note("C6"), note("G6"), wave="sine", vol=50, decay=62000, start_ms=200, total_ms=270))
    return r, mix(*parts)


def sfx_happy():
    r = MID
    a = arp_notes(r, ["C5", "E5", "G5", "C6"], 45, duty=0x40000000, vol=80, decay=63500, tail_ms=90)
    b = arp_notes(r, ["E6", "G6", "C7"], 45, wave="sine", vol=40, decay=63000, tail_ms=90)
    b = place(b, r, 45, None)
    return r, mix(a, b)


def sfx_sad():
    r = LO
    # a cute little "womp womp"
    a = tone(r, 170, note("G4"), note("F#4"), wave="square", duty=0x40000000, vib=6, vib_hz=7,
             decay=64800, total_ms=520)
    b = tone(r, 300, note("F4"), note("D4"), wave="square", duty=0x40000000, vib=10, vib_hz=6,
             decay=65200, start_ms=190, total_ms=520)
    return r, lowpass(mix(a, b), 150)


def sfx_love():
    r = LO
    parts = []
    for k, (nm, st) in enumerate([("A5", 0), ("C#6", 70), ("E6", 140), ("A6", 210)]):
        parts.append(tone(r, 200, note(nm), wave="sine", vol=70, decay=64000, start_ms=st, total_ms=420, vib=3))
    # heartbeat bumps
    parts.append(tone(r, 60, note("A2"), note("E2"), wave="sine", vol=90, decay=60000, total_ms=420))
    parts.append(tone(r, 60, note("A2"), note("E2"), wave="sine", vol=70, decay=60000, start_ms=110, total_ms=420))
    return r, mix(*parts)


def sfx_sparkle():
    r = MID
    rng = Rng(7)
    parts = []
    names = ["E7", "B6", "G#7", "C#7", "E7", "B7", "F#7"]
    for k, nm in enumerate(names):
        parts.append(tone(r, 120, note(nm), wave="sine", vol=60 - k * 4, decay=60500,
                          start_ms=k * 26 + rng.range(8), total_ms=300))
    return r, mix(*parts)


def sfx_splash():
    r = LO
    a = noise(r, 260, 21, hold_hz=None, lp0=200, lp1=30, decay=64000)
    bub = []
    for k in range(4):
        bub.append(tone(r, 40, note("C5") + k * 2000, note("G5") + k * 3000, wave="sine", vol=40,
                        decay=60000, start_ms=60 + k * 45, total_ms=260))
    return r, mix(a, *bub)


def sfx_poop():
    r = LO
    a = tone(r, 160, note("G3"), note("C2"), wave="sine", vol=100, decay=64000, total_ms=230)
    b = noise(r, 70, 5, hold_hz=900, lp0=60, lp1=20, vol=60, decay=61000, start_ms=10, total_ms=230)
    c = tone(r, 60, note("E5"), note("C5"), wave="tri", vol=30, decay=61000, start_ms=150, total_ms=230)
    return r, mix(a, b, c)


def sfx_egg_crack():
    r = LO
    parts = []
    for k, st in enumerate([0, 40, 70, 120]):
        parts.append(noise(r, 25, 31 + k, hold_hz=None, lp0=256, lp1=120, decay=55000, start_ms=st,
                           total_ms=170, hp=40))
    parts.append(tone(r, 50, note("A5"), note("E6"), wave="tri", vol=35, decay=60000, start_ms=120, total_ms=170))
    return r, mix(*parts)


def sfx_evolve():
    r = LO
    a = tone(r, 380, note("C4"), note("C6"), wave="square", duty=0x40000000, vol=60, tremolo=60,
             trem_hz=14, total_ms=620)
    chord = [tone(r, 260, note(nm), wave="sine", vol=55, decay=64800, start_ms=360, total_ms=620)
             for nm in ("C6", "E6", "G6", "C7")]
    spark = tone(r, 200, note("G6"), wave="sine", vol=25, decay=63000, start_ms=400, total_ms=620, vib=8, vib_hz=12)
    return r, mix(a, spark, *chord)


def sfx_pistol():
    r = MID
    a = noise(r, 110, 41, hold_hz=None, lp0=256, lp1=40, decay=62000)
    b = tone(r, 70, note("A3"), note("A1"), wave="square", duty=0x30000000, vol=80, decay=61000, total_ms=110)
    return r, drive(mix(a, b), 180)


def sfx_shotgun():
    r = LO
    a = noise(r, 330, 43, hold_hz=None, lp0=256, lp1=18, decay=64300, attack=0)
    b = tone(r, 160, note("F2"), note("A0"), wave="square", duty=0x60000000, vol=100, decay=63500, total_ms=330)
    c = noise(r, 60, 44, hold_hz=None, lp0=60, vol=40, decay=62000, start_ms=260, total_ms=330)  # pump click
    return r, drive(mix(a, b, c), 220)


def sfx_chaingun():
    r = MID
    a = noise(r, 70, 47, hold_hz=None, lp0=230, lp1=50, decay=60000, attack=0)
    b = tone(r, 45, note("D3"), note("D2"), wave="square", duty=0x40000000, vol=70, decay=60000, total_ms=70)
    return r, drive(mix(a, b), 200)


def sfx_rocket():
    r = LO
    a = noise(r, 380, 51, hold_hz=900, hold_hz1=2600, lp0=40, lp1=160, decay=64800, attack=30)
    b = tone(r, 300, note("C2"), note("G2"), wave="saw", vol=50, decay=64700, total_ms=380)
    c = noise(r, 70, 52, hold_hz=None, lp0=256, lp1=60, vol=70, decay=60000, total_ms=380)
    return r, drive(mix(a, b, c), 160)


def sfx_plasma():
    r = MID
    a = tone(r, 140, note("C7"), note("C4"), wave="square", duty=0x20000000, vol=70, decay=63000)
    b = tone(r, 140, note("G6"), note("G3"), wave="saw", vol=50, decay=63000, vib=24, vib_hz=40)
    return r, mix(a, b)


def sfx_bubble():
    r = MID
    a = tone(r, 90, note("C5"), note("C6"), wave="sine", vol=100, decay=62000, total_ms=130)
    b = tone(r, 30, note("G6"), wave="sine", vol=40, decay=60000, start_ms=85, total_ms=130)
    return r, mix(a, b)


def sfx_squeak():
    r = MID
    a = tone(r, 60, note("A5"), note("E6"), wave="tri", vol=100, decay=64500, vib=10, vib_hz=30, total_ms=130)
    b = tone(r, 60, note("E6"), note("B5"), wave="tri", vol=90, decay=63000, vib=10, vib_hz=30,
             start_ms=60, total_ms=130)
    return r, mix(a, b)


def sfx_chainsaw():
    r = LO
    a = tone(r, 360, note("A1"), note("C2"), wave="saw", vol=90, tremolo=70, trem_hz=24, total_ms=360)
    b = tone(r, 360, note("A2") + 300, note("D3"), wave="square", duty=0x30000000, vol=50, tremolo=50,
             trem_hz=31, total_ms=360)
    c = noise(r, 360, 61, hold_hz=1800, lp0=90, vol=40)
    return r, drive(mix(a, b, c), 260)


def sfx_explode():
    r = LO
    a = noise(r, 620, 71, hold_hz=None, lp0=256, lp1=10, decay=64900, attack=0)
    b = tone(r, 450, note("D2"), note("A0"), wave="sine", vol=100, decay=64700, total_ms=620)
    c = noise(r, 400, 72, hold_hz=400, lp0=120, vol=40, decay=64500, start_ms=80, total_ms=620)
    return r, drive(mix(a, b, c), 240)


def sfx_splat():
    r = LO
    a = noise(r, 190, 81, hold_hz=1200, hold_hz1=300, lp0=180, lp1=30, decay=63500, attack=0)
    b = tone(r, 120, note("E3"), note("E1"), wave="sine", vol=80, decay=62500, total_ms=190)
    return r, drive(mix(a, b), 190)


def sfx_confetti():
    r = MID
    pop = noise(r, 40, 91, hold_hz=None, lp0=256, lp1=90, decay=58000, attack=0)
    parts = [pop]
    names = ["C7", "E7", "G7", "D7", "A7", "F#7", "B6"]
    for k, nm in enumerate(names):
        parts.append(tone(r, 90, note(nm), wave="sine", vol=35, decay=61000,
                          start_ms=30 + k * 30, total_ms=320))
    parts.append(tone(r, 50, note("C5"), note("C6"), wave="tri", vol=45, decay=60000, total_ms=320))
    return r, mix(*parts)


def sfx_demon_roar():
    r = LO
    a = tone(r, 520, note("D2"), note("A1"), wave="saw", vol=90, vib=18, vib_hz=9, attack=60, decay=64900)
    b = tone(r, 520, note("D3") + 200, note("G2"), wave="square", duty=0x28000000, vol=50, vib=24, vib_hz=13,
             attack=60, decay=64900)
    c = noise(r, 520, 101, hold_hz=1500, lp0=70, vol=60, attack=80, decay=64900)
    return r, drive(lowpass(mix(a, b, c), 170), 260)


def sfx_demon_pain():
    r = LO
    a = tone(r, 200, note("A2"), note("E3"), wave="saw", vol=90, vib=30, vib_hz=17, decay=63800)
    b = noise(r, 200, 111, hold_hz=2000, lp0=90, vol=50, decay=63000)
    return r, drive(mix(a, b), 240)


def sfx_demon_die():
    r = LO
    a = tone(r, 420, note("E3"), note("E1"), wave="saw", vol=90, vib=20, vib_hz=11, decay=64700)
    b = noise(r, 420, 121, hold_hz=1600, hold_hz1=300, lp0=110, lp1=30, vol=60, decay=64600)
    gurgle = tone(r, 180, note("C2"), note("G1"), wave="square", duty=0x50000000, vol=40, tremolo=90,
                  trem_hz=21, start_ms=220, total_ms=420)
    return r, drive(mix(a, b, gurgle), 230)


def sfx_player_hurt():
    r = MID
    a = tone(r, 150, note("D4"), note("A2"), wave="square", duty=0x40000000, vol=80, decay=63000)
    b = noise(r, 60, 131, hold_hz=None, lp0=150, lp1=40, vol=50, decay=60000, total_ms=150)
    return r, drive(mix(a, b), 160)


def sfx_pickup():
    r = MID
    a = tone(r, 50, note("B5"), wave="square", duty=0x40000000, vol=80, total_ms=150)
    b = tone(r, 100, note("E6"), wave="square", duty=0x40000000, vol=80, decay=63000, start_ms=50, total_ms=150)
    return r, mix(a, b)


def sfx_door():
    r = LO
    a = noise(r, 300, 141, hold_hz=500, lp0=50, lp1=30, vol=80, attack=40)
    b = tone(r, 300, note("A1"), note("C2"), wave="saw", vol=30, total_ms=380)
    clunk = mix(noise(r, 60, 142, hold_hz=None, lp0=120, lp1=20, decay=60000, start_ms=300, total_ms=380),
                tone(r, 60, note("E2"), note("B1"), wave="sine", vol=90, decay=60000, start_ms=300, total_ms=380))
    return r, drive(mix(a, b, clunk), 170)


def sfx_weld():
    r = LO
    rng = Rng(151)
    parts = [noise(r, 320, 152, hold_hz=None, lp0=256, hp=30, vol=45)]
    for k in range(9):
        parts.append(noise(r, 12, 153 + k, hold_hz=None, lp0=256, vol=90, decay=50000,
                           start_ms=rng.range(300), total_ms=330, hp=60))
    parts.append(tone(r, 320, note("A#2"), wave="square", duty=0x20000000, vol=25, tremolo=60,
                      trem_hz=30, total_ms=330))
    return r, mix(*parts)


def sfx_holy():
    r = LO
    parts = []
    for k, nm in enumerate(["C5", "E5", "G5", "C6", "E6"]):
        parts.append(tone(r, 440, note(nm), wave="sine", vol=55, attack=90 + k * 20, vib=4, vib_hz=5,
                          decay=64900, start_ms=k * 25, total_ms=520))
        parts.append(tone(r, 440, note(nm) + note(nm) // 200, wave="sine", vol=30, attack=120,
                          decay=64900, start_ms=k * 25, total_ms=520))
    parts.append(tone(r, 300, note("G6"), wave="sine", vol=12, vib=12, vib_hz=11, start_ms=200, total_ms=520))
    return r, mix(*parts)


def sfx_possess():
    r = LO
    a = tone(r, 560, note("C2"), note("F#1"), wave="saw", vol=70, attack=300, vib=40, vib_hz=5)
    b = tone(r, 560, note("C#2"), note("G1"), wave="saw", vol=60, attack=300, vib=40, vib_hz=6)
    c = noise(r, 560, 161, hold_hz=300, hold_hz1=3000, lp0=40, lp1=200, vol=40, attack=400)
    return r, drive(crush(mix(a, b, c), 6), 200)


def sfx_bell():
    r = LO
    parts = []
    base = note("G4")
    # inharmonic church-bell partials (hum, prime, tierce, quint, nominal)
    for ratio16, vol, dec in ((-192, 50, 65300), (0, 100, 65150), (58, 45, 65000),
                              (112, 40, 64900), (192, 60, 64800), (192 + 96, 25, 64500)):
        f = (base * ratio_q24(ratio16)) >> 24
        parts.append(tone(r, 560, f, wave="sine", vol=vol, attack=2, decay=dec))
    clang = noise(r, 30, 171, hold_hz=None, lp0=200, vol=30, decay=55000, total_ms=560)
    return r, mix(clang, *parts)


# Length caps (ms): the tails are faded out so everything fits the budget.
CAP_MS = {
    "SAD": 400, "EVOLVE": 480, "DEMON_ROAR": 420, "POSSESS": 440, "BELL": 460, "HOLY": 430,
    "WELD": 250, "DOOR": 280, "CHAINSAW": 260, "DEMON_DIE": 330, "ROCKET": 310, "LOVE": 300,
    "CONFETTI": 220, "PICKUP": 125, "EXPLODE": 400, "SPARKLE": 190, "EAT": 230,
    "SHOTGUN": 280, "DEMON_PAIN": 150, "SPLASH": 200, "EGG_CRACK": 140, "ERROR": 160,
    "HAPPY": 220, "CANCEL": 110, "SELECT": 105, "SQUEAK": 110, "BUBBLE": 105,
}

RECIPES = {
    "CURSOR": sfx_cursor, "SELECT": sfx_select, "CANCEL": sfx_cancel, "ERROR": sfx_error,
    "BLIP": sfx_blip, "EAT": sfx_eat, "HAPPY": sfx_happy, "SAD": sfx_sad, "LOVE": sfx_love,
    "SPARKLE": sfx_sparkle, "SPLASH": sfx_splash, "POOP": sfx_poop, "EGG_CRACK": sfx_egg_crack,
    "EVOLVE": sfx_evolve, "PISTOL": sfx_pistol, "SHOTGUN": sfx_shotgun, "CHAINGUN": sfx_chaingun,
    "ROCKET": sfx_rocket, "PLASMA": sfx_plasma, "BUBBLE": sfx_bubble, "SQUEAK": sfx_squeak,
    "CHAINSAW": sfx_chainsaw, "EXPLODE": sfx_explode, "SPLAT": sfx_splat, "CONFETTI": sfx_confetti,
    "DEMON_ROAR": sfx_demon_roar, "DEMON_PAIN": sfx_demon_pain, "DEMON_DIE": sfx_demon_die,
    "PLAYER_HURT": sfx_player_hurt, "PICKUP": sfx_pickup, "DOOR": sfx_door, "WELD": sfx_weld,
    "HOLY": sfx_holy, "POSSESS": sfx_possess, "BELL": sfx_bell,
}


# ---------------------------------------------------------------------------
def read_sfx_names():
    with open(AUDIO_H, "r", encoding="ascii") as f:
        text = f.read()
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    m = re.search(r"enum\s*\{\s*(SFX_NONE.*?)\}", text, flags=re.S)
    names = [s.strip() for s in m.group(1).split(",") if s.strip()]
    if names[-1] != "SFX_COUNT":
        raise SystemExit("sfxgen: SFX enum must end with SFX_COUNT")
    return [n[4:] for n in names[:-1]]


def build():
    names = read_sfx_names()
    blobs = []
    for nm in names:
        if nm == "NONE":
            blobs.append((nm, HI, []))
            continue
        if nm not in RECIPES:
            raise SystemExit("sfxgen: no recipe for SFX_%s" % nm)
        rate, data = RECIPES[nm]()
        if nm in CAP_MS and len(data) > rate * CAP_MS[nm] // 1000:
            data = fade_out(data[:rate * CAP_MS[nm] // 1000], 60, rate)
        pcm = to_pcm8(fade_out(data, 6, rate))
        blobs.append((nm, rate, pcm))
    return blobs


def emit(blobs):
    total = sum(len(b[2]) for b in blobs)
    out = []
    out.append("/* GENERATED by tools/sfxgen.py - do not edit.\n")
    out.append(" * %d sound effects, %d bytes of signed 8-bit PCM (budget %d). */\n" % (
        len(blobs) - 1, total, BUDGET))
    out.append('#include "doomo/audio.h"\n\n')
    out.append("const uint32_t doomo_sfx_pcm_total = %du;\n\n" % total)
    out.append("const int8_t doomo_sfx_pcm_data[%d] = {\n" % max(1, total))
    flat = []
    for b in blobs:
        flat.extend(b[2])
    if not flat:
        flat = [0]
    for i in range(0, len(flat), 20):
        out.append("    " + ",".join("%d" % v for v in flat[i:i + 20]) + ",\n")
    out.append("};\n\n")
    out.append("/* {offset, length, rate} */\n")
    out.append("const DoomoSfxInfo doomo_sfx_info[SFX_COUNT] = {\n")
    off = 0
    for nm, rate, pcm in blobs:
        out.append("    { %du, %du, %du },   /* SFX_%s */\n" % (off, len(pcm), rate, nm))
        off += len(pcm)
    out.append("};\n")
    return "".join(out), total


def write_wav(path, rate, pcm):
    data = bytes((v + 128) & 255 for v in pcm)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate, 1, 8))
        f.write(b"data" + struct.pack("<I", len(data)) + data)


def main(argv):
    out_root = ROOT
    check = False
    wav = None
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == "--out-root" and i + 1 < len(argv):
            out_root = os.path.abspath(argv[i + 1])
            i += 2
            continue
        if a == "--wav" and i + 1 < len(argv):
            wav = argv[i + 1]
            i += 2
            continue
        if a == "--check":
            check = True
        elif a in ("-h", "--help"):
            print(__doc__)
            return 0
        i += 1
    blobs = build()
    text, total = emit(blobs)
    if check:
        for nm, rate, pcm in blobs:
            print("%-12s %5d bytes @ %5d Hz  %4d ms" % (nm, len(pcm), rate, len(pcm) * 1000 // rate))
        print("total %d / %d bytes" % (total, BUDGET))
    if total > BUDGET:
        sys.stderr.write("sfxgen: %d bytes > budget %d\n" % (total, BUDGET))
        return 1
    if wav:
        os.makedirs(wav, exist_ok=True)
        for nm, rate, pcm in blobs:
            if pcm:
                write_wav(os.path.join(wav, nm.lower() + ".wav"), rate, pcm)
    if check:
        return 0
    dst = os.path.join(out_root, "core", "gen", "sfx_data.c")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, "w", encoding="ascii", newline="\n") as f:
        f.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
