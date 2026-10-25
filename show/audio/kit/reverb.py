"""Synthetic impulse responses (plate / hall / room / chamber / spring / cab) and
convolution helpers.  IRs are generated, not downloaded: frequency-dependent
decay (per octave band RT60), early reflections, diffusion build-up and
decorrelated stereo tails, reproducible from a seed.
"""
from __future__ import annotations

import functools
import os

import numpy as np
from scipy import signal

from .core import CACHE, SR, rng, write_wav

PRESETS = {
    #           rt60  predelay  build  early  hf_damp lf_mult  width
    "plate":   (1.7,   8.0,     0.002, 0.0,   0.55,   0.9,     1.0),
    "hall":    (2.4,   22.0,    0.03,  0.6,   0.35,   1.2,     1.0),
    "room":    (0.55,  3.0,     0.006, 0.9,   0.5,    1.0,     0.8),
    "chamber": (1.2,   10.0,    0.012, 0.7,   0.45,   1.0,     0.9),
    "ambience": (0.35, 1.0,     0.002, 1.0,   0.5,    0.9,     1.0),
    "spring":  (1.6,   2.0,     0.0,   0.0,   0.7,    0.8,     0.6),
}
BANDS = (63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000)


def make_ir(kind="plate", rt60=None, predelay_ms=None, sr=SR, seed=0, hf_damp=None,
            width=None, length=None):
    """Return a stereo IR (n, 2), unit energy."""
    p = list(PRESETS[kind])
    if rt60 is not None:
        p[0] = rt60
    if predelay_ms is not None:
        p[1] = predelay_ms
    if hf_damp is not None:
        p[4] = hf_damp
    if width is not None:
        p[6] = width
    rt, pre, build, early, hfd, lfm, wid = p
    return _make_ir_cached(kind, rt, pre, build, early, hfd, lfm, wid, sr, seed, length)


@functools.lru_cache(maxsize=32)
def _make_ir_cached(kind, rt, pre, build, early, hfd, lfm, wid, sr, seed, length):
    r = rng("ir", kind, rt, seed)
    n = int((length or rt * 1.6 + 0.1) * sr)
    t = np.arange(n) / sr
    tails = []
    for ch in range(2):
        noise = r.standard_normal(n)
        acc = np.zeros(n)
        edges = [0] + [np.sqrt(BANDS[i] * BANDS[i + 1]) for i in range(len(BANDS) - 1)] + [sr / 2 - 1]
        for bi, fc in enumerate(BANDS):
            lo, hi = edges[bi], min(edges[bi + 1], sr / 2 - 100)
            if lo <= 0:
                sos = signal.butter(4, hi, "low", fs=sr, output="sos")
            elif hi >= sr / 2 - 200:
                sos = signal.butter(4, lo, "high", fs=sr, output="sos")
            else:
                sos = signal.butter(2, (lo, hi), "band", fs=sr, output="sos")
            band = signal.sosfiltfilt(sos, noise)
            # band RT60: highs decay faster (air + absorption), lows slightly longer
            octs = np.log2(fc / 1000.0)
            rtb = rt * (lfm if fc < 300 else 1.0) * (1.0 - hfd * np.clip(octs / 4.0, 0, 1))
            rtb = max(rtb, 0.08)
            acc += band * np.exp(-6.9078 * t / rtb)
        if kind == "spring":
            # dispersive 'boing': chirped allpass cascade smears transients
            for _ in range(12):
                a = 0.6
                acc = signal.lfilter([a, 0, 0, 1], [1, 0, 0, a], acc)
            acc *= 1 + 0.3 * np.sin(2 * np.pi * 33 * t)
        # diffusion build-up
        if build > 0:
            acc *= 1 - np.exp(-t / build)
        tails.append(acc)
    L, R = tails
    M = 0.5 * (L + R)
    S = 0.5 * (L - R) * wid
    L, R = M + S, M - S
    ir = np.stack([L, R], axis=1)
    # early reflections (sparse taps, first ~80 ms)
    if early > 0:
        k = 14
        times = np.sort(r.uniform(0.004, 0.08 if kind != "room" else 0.035, k))
        for i, tt in enumerate(times):
            idx = int(tt * sr)
            g = early * 0.9 * (1 - i / k) * r.uniform(0.6, 1.0)
            ir[idx, 0] += g * r.choice([-1, 1]) * 4
            ir[min(n - 1, idx + int(r.uniform(0, 0.002) * sr)), 1] += g * r.choice([-1, 1]) * 4
        ir[:, :] = signal.sosfilt(signal.butter(1, 9000, "low", fs=sr, output="sos"), ir, axis=0)
    pre_n = int(pre * 1e-3 * sr)
    ir = np.concatenate([np.zeros((pre_n, 2)), ir])
    # fade the very end
    f = int(0.05 * sr)
    ir[-f:] *= np.linspace(1, 0, f)[:, None]
    ir /= np.sqrt(np.sum(ir ** 2) / 2)
    ir.setflags(write=False)
    return ir


def convolve(x, ir, sr=SR, mono_in=True):
    """Convolve (n,2)/(n,) audio with a stereo IR; returns len(x)+len(ir)-1 samples."""
    x = np.asarray(x, dtype=float)
    if x.ndim == 2 and mono_in:
        x = x.mean(axis=1)
    if x.ndim == 1:
        return np.stack([signal.oaconvolve(x, ir[:, c]) for c in range(2)], axis=1)
    return np.stack([signal.oaconvolve(x[:, c], ir[:, c]) for c in range(2)], axis=1)


def export_ir(kind, path=None, **kw):
    ir = make_ir(kind, **kw)
    path = path or os.path.join(CACHE, f"ir_{kind}.wav")
    write_wav(path, ir / np.abs(ir).max() * 0.9, kw.get("sr", SR))
    return path
