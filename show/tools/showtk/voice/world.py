"""WORLD (pyworld) shaping for acting (spec §10.2 step 3): pitch, formant, a flattened
F0 (possessed, deadpan), a defined intonation contour (Steamy's lilt), the {w} wave and a
doubled voice. Timing is never changed here, so token and word times stay valid.

shape(y, sr, Shape(...)) -> y. With every field neutral it returns the input unchanged
(bit-exact), so voices that need no shaping keep Kokoro's own waveform.
"""
from __future__ import annotations

import dataclasses as dc

import numpy as np

FP = 5.0  # ms


@dc.dataclass
class Shape:
    pitch: float = 0.0        # semitones
    formant: float = 0.0      # semitones of spectral-envelope warp (+ = smaller / younger)
    flatten: float = 0.0      # 0 = natural F0, 1 = monotone at the line's median
    contour: str = ""         # "", "lilt" (Steamy), "fall" (a verdict)
    contour_amt: float = 1.0
    wavy: float = 0.0         # depth of the {w} wave, semitones
    double: float = 0.0       # gain of a detuned, delayed second voice (0 = off)
    breath: float = 0.0       # extra aperiodicity 0..1 (whisper, purr)
    seed: int = 0

    def neutral(self):
        return (self.pitch == 0 and self.formant == 0 and self.flatten == 0 and not self.contour
                and self.wavy == 0 and self.double == 0 and self.breath == 0)

    def tag(self):
        parts = []
        if self.pitch:
            parts.append(f"pitch={self.pitch:+g}st")
        if self.formant:
            parts.append(f"formant={self.formant:+g}st")
        if self.flatten:
            parts.append(f"flatten_f0={self.flatten:g}")
        if self.contour:
            parts.append(f"contour={self.contour}:{self.contour_amt:g}")
        if self.wavy:
            parts.append(f"wavy={self.wavy:g}")
        if self.double:
            parts.append(f"double={self.double:g}")
        if self.breath:
            parts.append(f"breath={self.breath:g}")
        return ",".join(parts)


def _warp_env(sp, ratio):
    """Scale the spectral envelope's frequency axis by `ratio` (>1 moves formants up)."""
    if ratio == 1.0:
        return sp
    nb = sp.shape[1]
    src = np.arange(nb) / ratio
    src = np.clip(src, 0, nb - 1)
    i0 = np.floor(src).astype(int)
    i1 = np.minimum(i0 + 1, nb - 1)
    fr = src - i0
    out = sp[:, i0] * (1 - fr) + sp[:, i1] * fr
    return np.maximum(out, 1e-16)


def _contour(n, voiced, kind, amt):
    """A per-frame semitone curve over the voiced span of the line."""
    c = np.zeros(n)
    idx = np.nonzero(voiced)[0]
    if not len(idx):
        return c
    a, b = idx[0], idx[-1] + 1
    x = np.linspace(0.0, 1.0, b - a)
    if kind == "lilt":
        # up on the name, down on the period: two rises and a settled fall (a fixed tune)
        c[a:b] = amt * (2.5 * np.sin(np.pi * np.clip(x / 0.55, 0, 1)) - 3.0 * np.clip((x - 0.7) / 0.3, 0, 1) ** 1.5
                        + 1.0 * np.sin(2 * np.pi * x * 2.0) * (1 - x))
    elif kind == "fall":
        c[a:b] = -amt * 2.0 * x ** 2
    return c


def _gate(x, sr, floor_db=-60.0, knee_db=12.0):
    hop = max(1, sr // 100)
    n = len(x)
    m = (n + hop - 1) // hop
    xp = np.concatenate([x, np.zeros(m * hop - n)])
    e = np.sqrt((xp.reshape(m, hop) ** 2).mean(axis=1) + 1e-20)
    db = 20 * np.log10(e / (e.max() + 1e-20))
    g = np.clip((db - floor_db) / knee_db, 0.0, 1.0)
    g = np.maximum(g, np.concatenate([[0], g[:-1]]))      # open a frame early, close a frame late
    g = np.maximum(g, np.concatenate([g[1:], [0]]))
    return np.interp(np.arange(n), np.arange(m) * hop + hop / 2, g)


def shape(y, sr, s: Shape):
    if s.neutral():
        return y
    import pyworld as pw
    x = np.asarray(y, dtype=np.float64)
    pad = int(0.05 * sr)
    x = np.concatenate([np.zeros(pad), x, np.zeros(pad)])
    f0, t = pw.harvest(x, sr, f0_floor=55.0, f0_ceil=900.0, frame_period=FP)
    sp = pw.cheaptrick(x, f0, t, sr)
    ap = pw.d4c(x, f0, t, sr)
    voiced = f0 > 0
    lf = np.where(voiced, np.log2(np.maximum(f0, 1e-3)), 0.0)
    if voiced.any():
        med = np.median(lf[voiced])
        if s.flatten:
            lf = np.where(voiced, med + (lf - med) * (1.0 - s.flatten), 0.0)
        semis = np.full(len(f0), s.pitch)
        if s.contour:
            semis = semis + _contour(len(f0), voiced, s.contour, s.contour_amt)
        if s.wavy:
            tt = np.arange(len(f0)) * FP / 1000.0
            rng = np.random.default_rng(s.seed)
            ph = rng.uniform(0, 2 * np.pi)
            semis = semis + s.wavy * (0.6 * np.sin(2 * np.pi * 1.3 * tt + ph) + 0.4 * np.sin(2 * np.pi * 5.2 * tt))
        lf = np.where(voiced, lf + semis / 12.0, 0.0)
        f0n = np.where(voiced, 2.0 ** lf, 0.0)
    else:
        f0n = f0
    if s.formant:
        sp = _warp_env(sp, 2.0 ** (s.formant / 12.0))
    if s.breath:
        ap = np.clip(ap + s.breath * (1 - ap), 0, 1)
    f0n, sp, ap = (np.ascontiguousarray(a, dtype=np.float64) for a in (f0n, sp, ap))
    out = pw.synthesize(f0n, sp, ap, sr, FP)
    if s.double:
        d = int(0.018 * sr)
        f2 = np.where(voiced, f0n * 2 ** (0.12 / 12), 0.0)
        o2 = pw.synthesize(np.ascontiguousarray(f2), sp, ap, sr, FP)
        o2 = np.concatenate([np.zeros(d), o2])[: len(out)]
        out = (out + s.double * o2) / (1 + 0.5 * s.double)
    out = out[pad: pad + len(y)]
    if len(out) < len(y):
        out = np.concatenate([out, np.zeros(len(y) - len(out))])
    # silence stays silence: WORLD's aperiodic resynthesis leaves a noise floor in the gaps,
    # so the output is gated by the input's own 10 ms envelope (-60 dB re peak = closed)
    out = out * _gate(np.asarray(y, np.float64), sr)
    # keep the input's level (WORLD resynthesis drifts by a dB or two)
    rin = np.sqrt(np.mean(np.asarray(y, np.float64) ** 2) + 1e-20)
    rout = np.sqrt(np.mean(out ** 2) + 1e-20)
    if s.wavy:
        tt = np.arange(len(out)) / sr
        out = out * (1.0 - 0.25 * s.wavy * (0.5 + 0.5 * np.sin(2 * np.pi * 0.9 * tt)))
    return (out * (rin / rout)).astype(np.float32)


def warp(y, sr, src_marks, dst_marks, out_len):
    """Piecewise-linear time warp keeping pitch: the sample positions src_marks[i] of y land
    on dst_marks[i] of the output (both monotonic). Used to lay a synced twin (Clinker)
    word for word on the line it doubles."""
    import pyworld as pw
    x = np.asarray(y, dtype=np.float64)
    f0, t = pw.harvest(x, sr, f0_floor=55.0, f0_ceil=900.0, frame_period=FP)
    sp = pw.cheaptrick(x, f0, t, sr)
    ap = pw.d4c(x, f0, t, sr)
    n = len(f0)
    m = max(2, int(np.ceil(out_len / sr * 1000.0 / FP)) + 1)
    tau = np.arange(m) * FP / 1000.0 * sr                      # output sample positions
    src = np.interp(tau, np.asarray(dst_marks, float), np.asarray(src_marks, float))
    i0 = np.clip(np.round(src / sr * 1000.0 / FP).astype(int), 0, n - 1)
    out = pw.synthesize(np.ascontiguousarray(f0[i0]), np.ascontiguousarray(sp[i0]),
                        np.ascontiguousarray(ap[i0]), sr, FP)
    out = out[:out_len] if len(out) >= out_len else np.concatenate([out, np.zeros(out_len - len(out))])
    return out.astype(np.float32)


def stretch(y, sr, ratio):
    """Time-stretch by `ratio` (>1 longer) keeping pitch: WORLD analysis, resampled
    frame tracks, resynthesis. Used for retime=fit within +/-15 % (spec §10.2 step 6)."""
    if abs(ratio - 1.0) < 1e-6:
        return y
    import pyworld as pw
    x = np.asarray(y, dtype=np.float64)
    f0, t = pw.harvest(x, sr, f0_floor=55.0, f0_ceil=900.0, frame_period=FP)
    sp = pw.cheaptrick(x, f0, t, sr)
    ap = pw.d4c(x, f0, t, sr)
    n = len(f0)
    m = max(2, int(round(n * ratio)))
    src = np.linspace(0, n - 1, m)
    i0 = np.round(src).astype(int)
    f0n, spn, apn = f0[i0], sp[i0], ap[i0]
    out = pw.synthesize(np.ascontiguousarray(f0n), np.ascontiguousarray(spn), np.ascontiguousarray(apn), sr, FP)
    want = int(round(len(y) * ratio))
    out = out[:want] if len(out) >= want else np.concatenate([out, np.zeros(want - len(out))])
    return out.astype(np.float32)
