"""Synthesis primitives for the show's SFX designs (WP10). 48 kHz float64,
deterministic: every random stream is seeded from the sound's name.
"""
from __future__ import annotations

import zlib

import numpy as np
from scipy import signal

SR = 48000


def rng(*keys):
    return np.random.default_rng(zlib.crc32("|".join(str(k) for k in keys).encode()))


def n_of(sec):
    return max(1, int(round(sec * SR)))


def t_of(n):
    return np.arange(n) / SR


# ---- envelopes ----------------------------------------------------------------
def env_ar(n, a=0.002, r=0.1, curve=4.0):
    """Attack (linear) then exponential release to -60 dB over r seconds."""
    t = t_of(n)
    e = np.minimum(1.0, t / max(a, 1e-5))
    rel = np.exp(-6.9 * np.maximum(0.0, t - a) / max(r, 1e-4))
    return e * rel


def env_lin(n, pts):
    """pts: [(sec, gain), ...] piecewise linear."""
    t = t_of(n)
    return np.interp(t, [p[0] for p in pts], [p[1] for p in pts])


def fade(x, fin=0.001, fout=0.005):
    x = x.copy()
    a, b = n_of(fin), n_of(fout)
    a, b = min(a, len(x)), min(b, len(x))
    if a > 1:
        x[:a] *= np.linspace(0, 1, a) if x.ndim == 1 else np.linspace(0, 1, a)[:, None]
    if b > 1:
        x[-b:] *= np.linspace(1, 0, b) if x.ndim == 1 else np.linspace(1, 0, b)[:, None]
    return x


# ---- sources ------------------------------------------------------------------------
def white(n, r):
    return r.standard_normal(n)


def pink(n, r):
    w = r.standard_normal(n)
    b, a = [0.049922035, -0.095993537, 0.050612699, -0.004408786], [1, -2.494956002, 2.017265875, -0.522189400]
    return signal.lfilter(b, a, w) * 8.0


def brown(n, r):
    x = np.cumsum(r.standard_normal(n))
    x = signal.lfilter([1, -1], [1, -0.999], x)
    return x / (np.abs(x).max() + 1e-9)


def phase(freq):
    """freq: array (Hz) or scalar -> phase in cycles."""
    f = np.asarray(freq, dtype=float)
    return np.cumsum(f) / SR


def sine(freq, n=None):
    if np.isscalar(freq):
        freq = np.full(n, float(freq))
    return np.sin(2 * np.pi * phase(freq))


def sweep(f0, f1, n, curve="exp"):
    x = np.linspace(0, 1, n)
    if curve == "exp":
        return f0 * (f1 / f0) ** x
    return f0 + (f1 - f0) * x


def saw(freq, n=None):
    if np.isscalar(freq):
        freq = np.full(n, float(freq))
    p = phase(freq) % 1.0
    return 2 * p - 1


def square(freq, n=None, duty=0.5):
    if np.isscalar(freq):
        freq = np.full(n, float(freq))
    p = phase(freq) % 1.0
    return np.where(p < duty, 1.0, -1.0)


def tri(freq, n=None):
    if np.isscalar(freq):
        freq = np.full(n, float(freq))
    p = phase(freq) % 1.0
    return 1 - 4 * np.abs(p - 0.5)


def fm(fc, ratio, index, n, idx_env=None):
    ph = 2 * np.pi * phase(np.full(n, float(fc)) if np.isscalar(fc) else fc)
    ix = index if idx_env is None else index * idx_env
    return np.sin(ph + ix * np.sin(ratio * ph))


# ---- filters -------------------------------------------------------------------------
def _sos(kind, f, order=2):
    f = np.clip(f, 10.0, SR * 0.49)
    return signal.butter(order, f, btype=kind, fs=SR, output="sos")


def lp(x, f, order=2):
    return signal.sosfilt(_sos("low", f, order), x, axis=0)


def hp(x, f, order=2):
    return signal.sosfilt(_sos("high", f, order), x, axis=0)


def bp(x, f, q=2.0, order=2):
    lo, hi = f / (1 + 0.5 / q), f * (1 + 0.5 / q)
    sos = signal.butter(order, [max(10, lo), min(SR * 0.49, hi)], btype="band", fs=SR, output="sos")
    return signal.sosfilt(sos, x, axis=0)


def reson(x, freqs, q=8.0, gains=None):
    """A bank of resonant band-passes (bodies: wood, metal, plastic)."""
    out = np.zeros_like(x)
    for i, f in enumerate(freqs):
        g = 1.0 if gains is None else gains[i]
        b, a = signal.iirpeak(min(f, SR * 0.45), q, fs=SR)
        out += g * signal.lfilter(b, a, x)
    return out


def svf_sweep(x, f_curve, q=0.7, mode="lp"):
    """Time-varying 2-pole state-variable filter (Chamberlin), f_curve per sample."""
    y = np.zeros_like(x)
    low = band = 0.0
    damp = 1.0 / q
    fc = np.clip(np.asarray(f_curve, float), 20.0, SR / 6.5)
    F = 2 * np.sin(np.pi * fc / SR)
    for i in range(len(x)):
        f = F[i]
        low += f * band
        high = x[i] - low - damp * band
        band += f * high
        y[i] = low if mode == "lp" else (band if mode == "bp" else high)
    return y


# ---- dynamics / colour -----------------------------------------------------------
def drive(x, amt=2.0):
    return np.tanh(x * amt) / np.tanh(amt)


def crush(x, bits=6):
    q = 2 ** (bits - 1)
    return np.round(x * q) / q


def norm(x, peak_db=-1.0):
    pk = np.abs(x).max()
    return x if pk <= 0 else x / pk * 10 ** (peak_db / 20)


def mix(*parts):
    n = max(len(p) for p in parts)
    out = np.zeros(n) if parts[0].ndim == 1 else np.zeros((n, parts[0].shape[1]))
    for p in parts:
        out[: len(p)] += p
    return out


def at(x, sec, total=None):
    """Delay x by sec (pad); optional total length."""
    pre = np.zeros(n_of(sec)) if sec > 0 else np.zeros(0)
    y = np.concatenate([pre, x])
    if total:
        n = n_of(total)
        y = np.pad(y, (0, max(0, n - len(y))))[:n]
    return y


def place(buf, x, sec, gain=1.0):
    i = n_of(sec) if sec > 0 else 0
    j = min(len(buf), i + len(x))
    if j > i:
        buf[i:j] += x[: j - i] * gain
    return buf


def reverb(x, rt=0.8, mix_=0.3, kind="room"):
    from ..music.kitenv import kit
    ir = kit.reverb.make_ir(kind, rt, 8, SR, 1)
    irm = ir.mean(axis=1) if ir.ndim == 2 else ir
    wet = signal.fftconvolve(x, irm)[: len(x) + int(rt * SR * 0.6)]
    dry = np.pad(x, (0, len(wet) - len(x)))
    return dry * (1 - mix_) + wet * mix_ / (np.abs(irm).sum() ** 0.5 + 1e-9) * 6.0


def pitch_shift_resample(x, ratio):
    """Varispeed by ratio (>1 = higher and shorter)."""
    n = int(len(x) / ratio)
    idx = np.arange(n) * ratio
    i = idx.astype(int)
    f = idx - i
    i2 = np.minimum(i + 1, len(x) - 1)
    return x[i] * (1 - f) + x[i2] * f


def stereo(x, pan=0.0, width=0.0, r=None):
    """Mono -> stereo with constant-power pan; width adds a decorrelated side."""
    th = (pan + 1) * np.pi / 4
    L, R = x * np.cos(th) * np.sqrt(2), x * np.sin(th) * np.sqrt(2)
    if width > 0:
        d = int(0.011 * SR)
        side = np.concatenate([np.zeros(d), x[:-d]]) if len(x) > d else x
        L, R = L + width * side * 0.5, R - width * side * 0.5
    return np.stack([L, R], 1)


# ---- building blocks ------------------------------------------------------------------
def click(n_ms=2.0, r=None, bright=6000.0):
    n = n_of(n_ms / 1000)
    r = r or rng("click")
    return hp(white(n, r), bright * 0.3) * np.exp(-np.arange(n) / (n / 5))


def thump(f0=90.0, f1=40.0, dur=0.25, click_amt=0.4, r=None):
    n = n_of(dur)
    r = r or rng("thump", f0, dur)
    body = sine(sweep(f0, f1, n)) * env_ar(n, 0.001, dur * 0.8)
    c = np.pad(click(3.0, r, 3000), (0, n))[:n] * click_amt
    return body + lp(c, 4000)


def squeak(f0, f1, dur, r=None, vib=8.0, vib_depth=0.03, bright=1.0):
    """A rubber squeak: a buzzy fundamental with vibrato through two formants."""
    n = n_of(dur)
    r = r or rng("squeak", f0, f1, dur)
    t = t_of(n)
    f = sweep(f0, f1, n) * (1 + vib_depth * np.sin(2 * np.pi * vib * t + r.random() * 6))
    src = saw(f) * 0.6 + square(f, duty=0.3) * 0.4
    body = reson(src, [f0 * 2.1, 2600 * bright, 4200 * bright], q=6, gains=[0.6, 1.0, 0.5])
    e = env_lin(n, [(0, 0), (0.012, 1), (dur * 0.7, 0.8), (dur, 0)])
    return body * e


def bell(f, dur=1.2, partials=((1, 1, 1), (2.76, 0.5, 0.6), (5.4, 0.25, 0.4), (8.9, 0.12, 0.25)),
         r=None, strike=0.3):
    n = n_of(dur)
    t = t_of(n)
    y = np.zeros(n)
    for ratio, amp, dm in partials:
        if f * ratio < SR * 0.45:
            y += amp * np.sin(2 * np.pi * f * ratio * t) * np.exp(-6.9 * t / (dur * dm))
    r = r or rng("bell", f)
    c = np.pad(click(1.5, r, f * 2), (0, n))[:n] * strike
    return y + c


def ks(f, dur, decay=0.996, bright=0.5, r=None):
    """Karplus-Strong pluck (strings, wood, twangs)."""
    n = n_of(dur)
    r = r or rng("ks", f)
    p = max(2, int(SR / f))
    buf = r.uniform(-1, 1, p)
    out = np.zeros(n)
    for i in range(n):
        v = buf[i % p]
        out[i] = v
        nxt = buf[(i + 1) % p]
        buf[i % p] = decay * (bright * v + (1 - bright) * 0.5 * (v + nxt))
    return out


def crackle(dur, density=60.0, r=None, bright=3500.0, size=(0.3, 1.0)):
    """Random impulse grains (crunch, crackle, gravel)."""
    n = n_of(dur)
    r = r or rng("crackle", dur, density)
    out = np.zeros(n)
    k = int(density * dur)
    for _ in range(k):
        i = r.integers(0, n)
        g = r.uniform(*size)
        m = int(r.uniform(0.0004, 0.003) * SR)
        burst = r.standard_normal(m) * np.exp(-np.arange(m) / (m / 4)) * g
        out[i:i + m] += burst[: n - i]
    return bp(out, bright, 1.2)


def bubbles(dur, rate=18.0, f_lo=250.0, f_hi=900.0, r=None, rise=1.6):
    """Water bubbles: short sine chirps rising in pitch (Minnaert resonance)."""
    n = n_of(dur)
    r = r or rng("bubbles", dur, rate)
    out = np.zeros(n)
    k = int(rate * dur)
    for _ in range(k):
        i = r.integers(0, n)
        f = r.uniform(f_lo, f_hi)
        m = int(r.uniform(0.01, 0.04) * SR)
        tt = np.arange(m) / SR
        ch = np.sin(2 * np.pi * np.cumsum(f * (1 + rise * tt / tt[-1])) / SR) * np.exp(-tt / (tt[-1] / 3))
        out[i:i + m] += ch[: n - i] * r.uniform(0.3, 1.0)
    return out


def wet(dur, r=None, low=180.0, high=2400.0, squelch=1.0, drops=1.0):
    """A wet impact: squelchy filtered noise + gloop formants + droplets."""
    n = n_of(dur)
    r = r or rng("wet", dur, low, high)
    t = t_of(n)
    base = pink(n, r)
    fc = low + (high - low) * np.exp(-t / (dur * 0.25))
    body = svf_sweep(base, fc * (1 + 0.3 * np.sin(2 * np.pi * 9 * t)), q=3.0, mode="bp") * squelch
    gl = bubbles(dur, 40 * drops, 120, 520, r, rise=-0.6) * 0.5
    e = env_lin(n, [(0, 0), (0.003, 1), (dur * 0.35, 0.45), (dur, 0)])
    return drive((body * 1.5 + gl) * e, 1.5)


def gust(dur, r=None, f0=300.0, f1=1200.0):
    n = n_of(dur)
    r = r or rng("gust", dur)
    t = t_of(n)
    sh = np.sin(np.pi * t / dur) ** 1.5
    f = f0 + (f1 - f0) * sh
    return svf_sweep(pink(n, r), f, q=1.5, mode="bp") * sh
