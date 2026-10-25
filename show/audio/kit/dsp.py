"""Numba DSP kernels: band-limited oscillators, filters, delays, envelopes.

Every kernel takes per-sample parameter arrays where modulation matters
(frequency, cutoff, duty), so vibrato, glides, filter sweeps and pitch bends
are sample-accurate.
"""
from __future__ import annotations

import math

import numpy as np
from numba import njit
from scipy import signal

from .core import SR

TWO_PI = 2.0 * math.pi


# --------------------------------------------------------------------------
# oscillators (phase increments `inc` = f / sr, per sample)
# --------------------------------------------------------------------------
@njit(cache=True, fastmath=True)
def _blep(t, dt):
    if dt <= 0.0:
        return 0.0
    if t < dt:
        t = t / dt
        return t + t - t * t - 1.0
    if t > 1.0 - dt:
        t = (t - 1.0) / dt
        return t * t + t + t + 1.0
    return 0.0


@njit(cache=True, fastmath=True)
def saw(inc, phase0=0.0):
    n = inc.shape[0]
    out = np.empty(n)
    ph = phase0 % 1.0
    for i in range(n):
        dt = inc[i]
        out[i] = 2.0 * ph - 1.0 - _blep(ph, dt)
        ph += dt
        if ph >= 1.0:
            ph -= 1.0
    return out


@njit(cache=True, fastmath=True)
def pulse(inc, duty, phase0=0.0):
    """PolyBLEP pulse; duty is a per-sample array (PWM) in (0, 1). DC removed."""
    n = inc.shape[0]
    out = np.empty(n)
    ph = phase0 % 1.0
    for i in range(n):
        dt = inc[i]
        d = duty[i]
        v = 1.0 if ph < d else -1.0
        v += _blep(ph, dt)
        p2 = ph - d
        if p2 < 0.0:
            p2 += 1.0
        v -= _blep(p2, dt)
        out[i] = v - (2.0 * d - 1.0)
        ph += dt
        if ph >= 1.0:
            ph -= 1.0
    return out


@njit(cache=True, fastmath=True)
def tri(inc, phase0=0.0, steps=0):
    """Triangle. steps=16 gives the 4-bit stepped NES/GB wave-channel triangle."""
    n = inc.shape[0]
    out = np.empty(n)
    ph = phase0 % 1.0
    for i in range(n):
        v = 4.0 * ph - 1.0 if ph < 0.5 else 3.0 - 4.0 * ph
        if steps > 0:
            v = (math.floor((v + 1.0) * 0.5 * steps - 1e-9) + 0.5) / steps * 2.0 - 1.0
        out[i] = v
        ph += inc[i]
        if ph >= 1.0:
            ph -= 1.0
    return out


@njit(cache=True, fastmath=True)
def sine(inc, phase0=0.0):
    n = inc.shape[0]
    out = np.empty(n)
    ph = phase0 % 1.0
    for i in range(n):
        out[i] = math.sin(TWO_PI * ph)
        ph += inc[i]
        if ph >= 1.0:
            ph -= 1.0
    return out


@njit(cache=True)
def lfsr_noise(n, rate_hz, sr, short_mode=False, seed=0x7FFF):
    """Game Boy / NES style LFSR noise (15-bit, or 7-bit 'metallic' mode)."""
    out = np.empty(n)
    reg = seed & 0x7FFF
    if reg == 0:
        reg = 1
    acc = 0.0
    step = rate_hz / sr
    for i in range(n):
        acc += step
        while acc >= 1.0:
            acc -= 1.0
            bit = (reg ^ (reg >> 1)) & 1
            reg = (reg >> 1) | (bit << 14)
            if short_mode:
                reg = (reg & ~(1 << 6)) | (bit << 6)
        out[i] = 1.0 if (reg & 1) == 0 else -1.0
    return out


def phase_from_freq(freq_hz, sr=SR, phase0=0.0):
    """Integrated phase (cycles) from a per-sample frequency curve."""
    inc = np.asarray(freq_hz, dtype=float) / sr
    ph = np.cumsum(inc) - inc[0] + phase0
    return ph


# --------------------------------------------------------------------------
# filters
# --------------------------------------------------------------------------
@njit(cache=True, fastmath=True)
def svf(x, fc, q, mode, sr):
    """Zavalishin TPT state-variable filter, per-sample cutoff `fc` (array).

    mode: 0 LP, 1 BP, 2 HP, 3 notch, 4 peak(BP normalised) ; q: resonance (0.5 = no peak)
    """
    n = x.shape[0]
    out = np.empty(n)
    ic1 = 0.0
    ic2 = 0.0
    k = 1.0 / q
    nyq = 0.49 * sr
    for i in range(n):
        f = fc[i]
        if f < 5.0:
            f = 5.0
        if f > nyq:
            f = nyq
        g = math.tan(math.pi * f / sr)
        a1 = 1.0 / (1.0 + g * (g + k))
        a2 = g * a1
        a3 = g * a2
        v3 = x[i] - ic2
        v1 = a1 * ic1 + a2 * v3
        v2 = ic2 + a2 * ic1 + a3 * v3
        ic1 = 2.0 * v1 - ic1
        ic2 = 2.0 * v2 - ic2
        if mode == 0:
            out[i] = v2
        elif mode == 1:
            out[i] = v1 * k
        elif mode == 2:
            out[i] = x[i] - k * v1 - v2
        elif mode == 3:
            out[i] = x[i] - k * v1
        else:
            out[i] = v1
    return out


@njit(cache=True, fastmath=True)
def ladder(x, fc, res, drive, sr):
    """4-pole transistor-ladder style low-pass (tanh stages), per-sample cutoff.

    res 0..1 (self-oscillation near 1); drive >= 1 adds warmth.
    """
    n = x.shape[0]
    out = np.empty(n)
    s0 = 0.0
    s1 = 0.0
    s2 = 0.0
    s3 = 0.0
    nyq = 0.45 * sr
    for i in range(n):
        f = fc[i]
        if f < 10.0:
            f = 10.0
        if f > nyq:
            f = nyq
        g = math.tan(math.pi * f / sr)
        G = g / (1.0 + g)
        k = 4.0 * res
        # zero-delay feedback estimate (one-step)
        S = (G * G * G * s0 + G * G * s1 + G * s2 + s3) * (1.0 - G)
        u = (math.tanh(drive * x[i]) / drive * 1.0 - k * S) / (1.0 + k * G * G * G * G)
        v = (u - s0) * G
        y0 = v + s0
        s0 = y0 + v
        v = (math.tanh(y0) - s1) * G
        y1 = v + s1
        s1 = y1 + v
        v = (y1 - s2) * G
        y2 = v + s2
        s2 = y2 + v
        v = (y2 - s3) * G
        y3 = v + s3
        s3 = y3 + v
        out[i] = y3 * (1.0 + 0.5 * res)
    return out


@njit(cache=True, fastmath=True)
def onepole_lp(x, fc, sr):
    n = x.shape[0]
    out = np.empty(n)
    z = 0.0
    for i in range(n):
        a = 1.0 - math.exp(-TWO_PI * fc[i] / sr)
        z += a * (x[i] - z)
        out[i] = z
    return out


def lp(x, fc, q=0.707, sr=SR):
    return _svf_any(x, fc, q, 0, sr)


def hp(x, fc, q=0.707, sr=SR):
    return _svf_any(x, fc, q, 2, sr)


def bp(x, fc, q=1.0, sr=SR):
    return _svf_any(x, fc, q, 1, sr)


def _svf_any(x, fc, q, mode, sr):
    x = np.asarray(x, dtype=float)
    if x.ndim == 2:
        return np.stack([_svf_any(x[:, c], fc, q, mode, sr) for c in range(x.shape[1])], axis=1)
    fc = np.broadcast_to(np.asarray(fc, dtype=float), x.shape).copy()
    return svf(x, fc, float(q), mode, float(sr))


def lp24(x, fc, q=0.707, sr=SR):
    """Two cascaded SVFs: 24 dB/oct low-pass (Butterworth-ish at q=0.54/1.31)."""
    return lp(lp(x, fc, 0.54, sr), fc, q * 1.31 / 0.707, sr)


def zp_filter(x, kind, fc, order=4, sr=SR):
    """Zero-phase Butterworth (offline). kind: 'low'|'high'|'band'."""
    sos = signal.butter(order, fc, btype=kind, fs=sr, output="sos")
    return signal.sosfiltfilt(sos, x, axis=0)


def butter(x, kind, fc, order=2, sr=SR):
    sos = signal.butter(order, fc, btype=kind, fs=sr, output="sos")
    return signal.sosfilt(sos, x, axis=0)


# --------------------------------------------------------------------------
# delays, modulation
# --------------------------------------------------------------------------
@njit(cache=True, fastmath=True)
def mod_delay(x, delay_samples, mix_dry, mix_wet, fb):
    """Fractional (cubic) modulated delay line; delay per sample (array)."""
    n = x.shape[0]
    size = 1
    mx = 0.0
    for i in range(n):
        if delay_samples[i] > mx:
            mx = delay_samples[i]
    while size < int(mx) + 8:
        size *= 2
    buf = np.zeros(size)
    mask = size - 1
    out = np.empty(n)
    w = 0
    last = 0.0
    for i in range(n):
        buf[w] = x[i] + fb * last
        d = delay_samples[i]
        rp = w - d
        ip = int(math.floor(rp))
        fr = rp - ip
        xm1 = buf[(ip - 1) & mask]
        x0 = buf[ip & mask]
        x1 = buf[(ip + 1) & mask]
        x2 = buf[(ip + 2) & mask]
        c1 = 0.5 * (x1 - xm1)
        c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2
        c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1)
        y = ((c3 * fr + c2) * fr + c1) * fr + x0
        last = y
        out[i] = mix_dry * x[i] + mix_wet * y
        w = (w + 1) & mask
    return out


@njit(cache=True, fastmath=True)
def pingpong(xl, xr, dl, dr, fb, lp_a, hp_a, cross):
    """Stereo ping-pong / dual delay with LP + HP damping inside the loop.

    dl, dr: delay in samples (int); fb: feedback; lp_a/hp_a: one-pole coefs;
    cross: 1.0 = full ping-pong, 0.0 = independent channels.
    """
    n = xl.shape[0]
    size = 1
    while size < max(dl, dr) + 4:
        size *= 2
    bl = np.zeros(size)
    br = np.zeros(size)
    mask = size - 1
    ol = np.empty(n)
    orr = np.empty(n)
    w = 0
    lpl = 0.0
    lpr = 0.0
    hpl = 0.0
    hpr = 0.0
    for i in range(n):
        yl = bl[(w - dl) & mask]
        yr = br[(w - dr) & mask]
        lpl += lp_a * (yl - lpl)
        lpr += lp_a * (yr - lpr)
        hpl += hp_a * (lpl - hpl)
        hpr += hp_a * (lpr - hpr)
        fl = lpl - hpl
        fr = lpr - hpr
        bl[w] = xl[i] + fb * ((1.0 - cross) * fl + cross * fr)
        br[w] = xr[i] + fb * ((1.0 - cross) * fr + cross * fl)
        ol[i] = fl
        orr[i] = fr
        w = (w + 1) & mask
    return ol, orr


@njit(cache=True, fastmath=True)
def karplus(n, period, decay, damp, excite, stretch):
    """Extended Karplus-Strong string.

    period: delay length in samples (float, tuned with an allpass);
    decay: loop gain per period (0.99..0.9999); damp: 0..1 brightness loss;
    excite: excitation (len <= n) ; stretch: 0..0.5 dispersion-ish blend.
    """
    N = int(math.floor(period - 0.5))
    if N < 2:
        N = 2
    frac = period - N - 0.5  # the averaging filter adds 0.5 sample delay
    if frac < 0.0:
        frac += 1.0
        N -= 1
    eta = (1.0 - frac) / (1.0 + frac)  # allpass for the fractional part
    size = 1
    while size < N + 4:
        size *= 2
    buf = np.zeros(size)
    mask = size - 1
    out = np.empty(n)
    w = 0
    prev = 0.0
    ap_x1 = 0.0
    ap_y1 = 0.0
    ne = excite.shape[0]
    a = 0.5 + 0.5 * (1.0 - damp)  # lowpass blend: 1 = no damping
    for i in range(n):
        y = buf[(w - N) & mask]
        # loop lowpass (two-point weighted average)
        lpv = a * y + (1.0 - a) * prev
        prev = y
        # fractional allpass
        apo = eta * lpv + ap_x1 - eta * ap_y1
        ap_x1 = lpv
        ap_y1 = apo
        v = decay * apo
        if i < ne:
            v += excite[i]
        buf[w] = v
        out[i] = v
        w = (w + 1) & mask
    return out


# --------------------------------------------------------------------------
# envelopes and dynamics
# --------------------------------------------------------------------------
def adsr(n_on, a, d, s, r, sr=SR, total=None, attack_curve=2.5):
    """ADSR envelope. n_on = held samples; a/d/r in seconds; s = sustain level.

    decay `d` reaches the sustain level to within -40 dB; release `r` is the
    time to -60 dB. Returns an array of length n_on + release (or `total`).
    """
    na = max(1, int(a * sr))
    nd = max(1, int(d * sr))
    nr = max(1, int(r * sr))
    n = n_on + nr if total is None else int(total)
    i = np.arange(n, dtype=float)
    x = np.clip(i / na, 0.0, 1.0)
    att = (1.0 - np.exp(-attack_curve * x)) / (1.0 - np.exp(-attack_curve))
    dec = s + (1.0 - s) * np.exp(-4.6 * np.maximum(i - na, 0.0) / nd)
    held = np.where(i < na, att, dec)
    k = min(max(n_on, 0), n - 1)
    lvl = held[k]
    rel = lvl * np.exp(-6.9 * np.maximum(i - n_on, 0.0) / nr)
    return np.where(i < n_on, held, rel)


def exp_decay(n, t60, sr=SR):
    return np.exp(-6.9078 * np.arange(n) / (t60 * sr))


@njit(cache=True, fastmath=True)
def env_follow(x, att, rel):
    n = x.shape[0]
    out = np.empty(n)
    e = 0.0
    for i in range(n):
        v = abs(x[i])
        c = att if v > e else rel
        e = c * e + (1.0 - c) * v
        out[i] = e
    return out


@njit(cache=True, fastmath=True)
def smooth_gain(target, att_coef, rel_coef):
    """One-pole smoothing of a gain curve: fast down (attack), slow up (release)."""
    n = target.shape[0]
    out = np.empty(n)
    g = 1.0
    for i in range(n):
        t = target[i]
        if t < g:
            g = att_coef * g + (1.0 - att_coef) * t
        else:
            g = rel_coef * g + (1.0 - rel_coef) * t
        out[i] = g
    return out


def coef(ms, sr=SR):
    return math.exp(-1.0 / max(1e-6, ms * 1e-3 * sr))


# --------------------------------------------------------------------------
# saturation
# --------------------------------------------------------------------------
def oversample(fn, x, factor=4):
    """Run a memoryless nonlinearity at `factor`x the rate (anti-aliased)."""
    up = signal.resample_poly(x, factor, 1, axis=0)
    y = fn(up)
    return signal.resample_poly(y, 1, factor, axis=0)


def tape_sat(x, drive_db=6.0, bias=0.05, mix=1.0, factor=4):
    """Warm asymmetric soft saturation (even + odd harmonics), level-compensated."""
    g = 10 ** (drive_db / 20)

    def f(u):
        y = np.tanh(g * u + bias) - np.tanh(bias)
        return y / np.tanh(g) if g > 1 else y

    y = oversample(f, x, factor)
    return mix * y + (1 - mix) * x


def soft_clip(x, ceiling=1.0, knee=0.5):
    """Cubic soft clipper: transparent below ceiling*knee, smooth to ceiling."""
    t = ceiling * knee
    ax = np.abs(x)
    y = np.where(ax <= t, ax, t + (ceiling - t) * np.tanh((ax - t) / (ceiling - t)))
    return np.sign(x) * y


def white(n, r):
    return r.standard_normal(n)


def pink(n, r):
    """Voss-McCartney-ish pink noise via FFT shaping."""
    w = r.standard_normal(n)
    W = np.fft.rfft(w)
    f = np.arange(len(W))
    f[0] = 1
    W = W / np.sqrt(f)
    y = np.fft.irfft(W, n)
    return y / (np.std(y) + 1e-12)
