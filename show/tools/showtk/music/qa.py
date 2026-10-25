"""Listening checks for every cue and SFX (WP10): loudness, true peak,
clipping, onsets on the grid, silence (the brief's "LISTEN-check").

    report(y, barmap=None) -> dict
    first_onset(y) -> sample of the first sound (sample-accurate)
    onsets(y) -> [sample, ...] (spectral flux, refined to the attack)
    grid_check(onsets, barmap, grid=0.25) -> worst / median distance to the
        nearest 16th of the cue's own grid, in ms
"""
from __future__ import annotations

import numpy as np
import pyloudnorm as pyln
from scipy import signal

SR = 48000


def mono(y):
    y = np.asarray(y, dtype=float)
    return y if y.ndim == 1 else y.mean(axis=1)


def true_peak_db(y, os=4):
    y = np.asarray(y, dtype=float)
    if y.ndim == 1:
        y = y[:, None]
    up = signal.resample_poly(y, os, 1, axis=0)
    return float(20 * np.log10(max(np.abs(up).max(), 1e-12)))


def lufs(y):
    y = np.asarray(y, dtype=float)
    if len(y) < int(0.42 * SR):
        y = np.pad(y, ((0, int(0.42 * SR) - len(y)),) + (((0, 0),) if y.ndim == 2 else ()))
    try:
        return float(pyln.Meter(SR).integrated_loudness(y))
    except Exception:
        return float("-inf")


def first_onset(y, rel_db=-40.0, abs_db=-70.0):
    """First sample whose level rises above max(abs_db, peak_of_first_second + rel_db)."""
    m = np.abs(mono(y))
    if not len(m) or m.max() <= 0:
        return None
    head = m[: SR] if len(m) > SR else m
    nz = np.nonzero(m > 10 ** (abs_db / 20))[0]
    if not len(nz):
        return None
    start = nz[0]
    ref = m[start: start + SR].max()
    th = max(10 ** (abs_db / 20), ref * 10 ** (rel_db / 20))
    del head
    return int(np.argmax(m > th))


def onsets(y, hop=128, win=1024, delta=0.08):
    """Onset samples (spectral flux peaks, refined to the energy rise)."""
    x = mono(y)
    if len(x) < win * 2:
        return []
    f, t, Z = signal.stft(x, SR, nperseg=win, noverlap=win - hop, boundary=None, padded=False)
    mag = np.log1p(30 * np.abs(Z))
    flux = np.maximum(0, np.diff(mag, axis=1)).sum(axis=0)
    flux = flux / (flux.max() + 1e-12)
    # adaptive threshold: local median + delta
    k = 9
    med = signal.medfilt(flux, k)
    peaks, _ = signal.find_peaks(flux, height=med + delta, distance=max(1, int(0.05 * SR / hop)))
    out = []
    # 1 ms energy envelope (dB); the onset is the steepest 1 ms rise near the flux peak
    e = np.convolve(x * x, np.ones(48) / 48, mode="same")
    edb = 10 * np.log10(e + 1e-12)
    rise = np.zeros_like(edb)
    rise[:-48] = edb[48:] - edb[:-48]
    for p in peaks:
        c = (p + 1) * hop + win // 2              # centre of the later frame
        a, b = max(0, c - win // 2 - hop), min(len(x) - 48, c + win // 2)
        if b <= a:
            continue
        i = int(np.argmax(rise[a:b]))
        out.append(a + i + 24)
    return sorted(set(out))


def grid_check(ons, barmap, grid=0.25):
    """Distance (ms) of each onset to the nearest grid point (grid in beats)."""
    if not ons or barmap is None:
        return None
    spb = float(barmap.spb_smp)
    step = spb * grid
    d = []
    for s in ons:
        rel = s - barmap.offset_smp
        k = round(rel / step)
        d.append(abs(rel - k * step) / SR * 1000.0)
    d = np.array(d)
    return dict(n=len(d), median_ms=round(float(np.median(d)), 2), p90_ms=round(float(np.percentile(d, 90)), 2),
                worst_ms=round(float(d.max()), 2), within_10ms=round(float(np.mean(d <= 10.0)) * 100, 1))


def silence_runs(y, thresh_db=-60.0, min_s=0.3):
    """[(start_s, len_s)] stretches quieter than thresh (20 ms RMS windows)."""
    x = mono(y)
    w = 960
    n = len(x) // w
    if n == 0:
        return []
    r = np.sqrt(np.mean(x[: n * w].reshape(n, w) ** 2, axis=1) + 1e-20)
    q = 20 * np.log10(r) < thresh_db
    runs, i = [], 0
    while i < n:
        if q[i]:
            j = i
            while j < n and q[j]:
                j += 1
            if (j - i) * w / SR >= min_s:
                runs.append((round(i * w / SR, 2), round((j - i) * w / SR, 2)))
            i = j
        else:
            i += 1
    return runs


def report(y, barmap=None, grid=0.25):
    y = np.asarray(y, dtype=float)
    sp = float(20 * np.log10(max(np.abs(y).max(), 1e-12)))
    clipped = int(np.sum(np.abs(y) >= 0.999))
    ons = onsets(y)
    r = dict(length_s=round(len(y) / SR, 3), lufs=round(lufs(y), 2), true_peak_db=round(true_peak_db(y), 2),
             sample_peak_db=round(sp, 2), clipped=clipped, first_onset_smp=first_onset(y),
             n_onsets=len(ons), silences=silence_runs(y))
    if barmap is not None:
        r["grid"] = grid_check(ons, barmap, grid)
    return r


def spectrogram_png(y, path, title=""):
    """A quick look (log-frequency spectrogram + waveform) for the preview folder."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    x = mono(y)
    fig, ax = plt.subplots(2, 1, figsize=(12, 5), gridspec_kw=dict(height_ratios=[3, 1]))
    f, t, Z = signal.stft(x, SR, nperseg=2048, noverlap=2048 - 512)
    S = 20 * np.log10(np.abs(Z) + 1e-9)
    ax[0].pcolormesh(t, f, S, vmin=S.max() - 90, vmax=S.max(), shading="auto", cmap="magma")
    ax[0].set_yscale("symlog", linthresh=200)
    ax[0].set_ylim(40, 20000)
    ax[0].set_title(title)
    tt = np.arange(len(x)) / SR
    ax[1].plot(tt[::32], x[::32], lw=0.4)
    ax[1].set_xlim(0, tt[-1] if len(tt) else 1)
    ax[1].set_ylim(-1, 1)
    fig.tight_layout()
    fig.savefig(path, dpi=80)
    plt.close(fig)
