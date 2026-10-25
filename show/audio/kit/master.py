"""Mastering: tonal EQ, glue compressor, oversampled soft clipper, true-peak
limiter, loudness targeting (-14 LUFS integrated, <= -1.0 dBTP) and exports
(WAV 24-bit, FLAC 24-bit, MP3 320 kbps, with MP3 true-peak re-check).
"""
from __future__ import annotations

import os
import subprocess
import tempfile

import numpy as np
import pyloudnorm as pyln
import soundfile as sf
from numba import njit
from scipy import signal

from . import dsp
from .core import SR, a2db, db2a, stereo
from .mixer import compress, eq, mono_below, width


def true_peak(x, sr=SR, os_factor=4):
    """Inter-sample (true) peak in dBTP via 4x polyphase oversampling (BS.1770-4)."""
    up = signal.resample_poly(stereo(x), os_factor, 1, axis=0)
    return float(a2db(np.max(np.abs(up))))


def lufs(x, sr=SR):
    meter = pyln.Meter(sr)
    return float(meter.integrated_loudness(stereo(x)))


@njit(cache=True)
def _sliding_min(x, w):
    """min over x[i : i+w] (forward window), O(n) monotonic deque."""
    n = x.shape[0]
    out = np.empty(n)
    dq = np.empty(n + w + 1, dtype=np.int64)
    head = 0
    tail = 0
    j = 0
    for i in range(n):
        while j < n and j < i + w:
            while tail > head and x[dq[tail - 1]] >= x[j]:
                tail -= 1
            dq[tail] = j
            tail += 1
            j += 1
        while dq[head] < i:
            head += 1
        out[i] = x[dq[head]]
    return out


@njit(cache=True)
def _release(g, rel_c):
    n = g.shape[0]
    out = np.empty(n)
    y = 1.0
    for i in range(n):
        t = g[i]
        if t < y:
            y = t
        else:
            y = rel_c * y + (1.0 - rel_c) * t
        out[i] = y
    return out


def limiter(x, ceiling_db=-1.0, lookahead_ms=1.5, release_ms=80.0, sr=SR, os_factor=4,
            return_gain=False):
    """Look-ahead true-peak limiter (oversampled detection, smooth gain ramps)."""
    x = stereo(x)
    up = signal.resample_poly(x, os_factor, 1, axis=0)
    n = len(x)
    pk = np.abs(up[: n * os_factor]).reshape(n, os_factor, 2).max(axis=(1, 2))
    ceil = db2a(ceiling_db)
    need = np.minimum(1.0, ceil / np.maximum(pk, 1e-9))
    L = max(1, int(lookahead_ms * 1e-3 * sr))
    # forward-looking hold over [i, i+2L], then a causal L-sample box ramp:
    # every peak p is covered (sm[p] <= need[p]) and the attack is a smooth
    # ramp that starts up to 2L samples early. No audio delay is needed.
    held = _sliding_min(need, 2 * L + 1)
    k = np.ones(L) / L
    sm = np.convolve(np.concatenate([np.ones(L - 1), held]), k, mode="valid")
    g = _release(sm, dsp.coef(release_ms, sr))
    return (x * g[:, None], g) if return_gain else x * g[:, None]


def soft_clipper(x, threshold_db=-2.0, ceiling_db=-0.3, sr=SR, os_factor=4):
    """Oversampled soft clipper: rounds off transient peaks before the limiter."""
    c = db2a(ceiling_db)
    knee = db2a(threshold_db) / c

    def f(u):
        return dsp.soft_clip(u, c, knee)

    return dsp.oversample(f, stereo(x), os_factor)


DEFAULT_CHAIN = dict(
    eq=[("hp", 24.0, 2), ("lowshelf", 90.0, 0.5), ("peak", 320.0, -0.8, 0.8),
        ("peak", 3200.0, 0.4, 0.7), ("highshelf", 11000.0, 1.0)],
    glue=dict(thresh=-21.0, ratio=2.0, attack=30.0, release=160.0, knee=6.0, detector="rms",
              sc_hp=90.0),
    clip=dict(threshold_db=-3.0, ceiling_db=-0.6),
    limit=dict(ceiling_db=-1.3, lookahead_ms=1.5, release_ms=90.0),
    mono_bass=120.0,
    width=1.0,
)


def master(x, target_lufs=-14.0, tp_max=-1.0, chain=None, sr=SR, verbose=False):
    """Master to `target_lufs` integrated with true peak <= tp_max. Returns (y, report)."""
    ch = dict(DEFAULT_CHAIN)
    ch.update(chain or {})
    x = stereo(x)
    if ch.get("mono_bass"):
        x = mono_below(x, ch["mono_bass"], sr)
    if ch.get("eq"):
        x = eq(x, ch["eq"], sr)
    # glue compression at a sensible level: normalise the input first
    l0 = lufs(x, sr)
    x = x * db2a(-18.0 - l0)
    gr_info = None
    if ch.get("glue"):
        x, gr = compress(x, sr=sr, return_gr=True, **ch["glue"])
        gr_info = float(np.percentile(-gr, 95))
    if ch.get("width", 1.0) != 1.0:
        x = width(x, ch["width"])
    gain = target_lufs - lufs(x, sr)
    y = x
    ceiling = ch["limit"]["ceiling_db"]
    for it in range(8):
        z = x * db2a(gain)
        if ch.get("clip"):
            z = soft_clipper(z, sr=sr, **ch["clip"])
        lim = dict(ch["limit"])
        lim["ceiling_db"] = ceiling
        y, lg = limiter(z, sr=sr, return_gain=True, **lim)
        L = lufs(y, sr)
        tp = true_peak(y, sr)
        if verbose:
            print(f"  master iter {it}: gain {gain:+.2f} dB -> {L:.2f} LUFS, TP {tp:.2f} dBTP")
        if tp > tp_max - 0.05:
            ceiling -= (tp - (tp_max - 0.1))
            continue
        if abs(L - target_lufs) < 0.05:
            break
        gain += (target_lufs - L)
    # final safety: TPDF dither at 24-bit is negligible but correct
    lgr = -20 * np.log10(np.maximum(lg, 1e-9))
    rep = dict(lufs=lufs(y, sr), true_peak_dbtp=true_peak(y, sr), glue_gr_p95_db=gr_info,
               limiter_gr_max_db=float(lgr.max()), limiter_gr_p99_db=float(np.percentile(lgr, 99)),
               pre_gain_db=gain, limiter_ceiling_db=ceiling)
    return y, rep


def export(y, base, sr=SR, mp3=True, flac=True, tp_max=-1.0):
    """Write base.wav (24-bit), base.flac, base.mp3 (320k); verify MP3 true peak."""
    os.makedirs(os.path.dirname(os.path.abspath(base)), exist_ok=True)
    r = np.random.default_rng(0)
    lsb = 1.0 / (2 ** 23)
    d = (r.random(y.shape) - r.random(y.shape)) * lsb
    yd = np.clip(y + d, -1, 1)
    out = {"wav": base + ".wav"}
    sf.write(base + ".wav", yd, sr, subtype="PCM_24")
    if flac:
        sf.write(base + ".flac", yd, sr, subtype="PCM_24", format="FLAC")
        out["flac"] = base + ".flac"
    if mp3:
        vol = 0.0
        for _ in range(4):
            subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", base + ".wav", "-af",
                            f"volume={vol}dB", "-c:a", "libmp3lame", "-b:a", "320k",
                            base + ".mp3"], check=True)
            dec, _ = _decode(base + ".mp3")
            tp = true_peak(dec, sr)
            if tp <= tp_max:
                break
            vol -= (tp - tp_max) + 0.05
        out["mp3"] = base + ".mp3"
        out["mp3_true_peak_dbtp"] = tp
        out["mp3_trim_db"] = vol
    return out


def _decode(path):
    from .core import CACHE
    os.makedirs(CACHE, exist_ok=True)
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False, dir=CACHE) as tf:
        tmp = tf.name
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", path, "-c:a", "pcm_f32le", tmp], check=True)
    x, sr = sf.read(tmp, always_2d=True)
    os.unlink(tmp)
    return x, sr
