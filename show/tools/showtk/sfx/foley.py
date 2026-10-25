"""The two data-driven foley synths (SHOW_ENGINE_SPEC.md sec.10.5; WP10).

    crayon(speed, seed)            FOL_crayon_scratch follow=<stroke>
    paws(contacts, n_frames, seed) FOL_paw_patter from=crowd:<ids>

Both read the picture's own data, frame by frame, so the sound is the
drawing's: the mix (WP11) passes them what showc puts in timeline.json.

crayon: `speed` is the stroke's reveal speed per frame (arc length revealed in
that frame, any unit; 0 = the crayon is lifted). Filtered noise (wax on paper
grain) whose level follows speed^0.7 and whose brightness and grain rate rise
with it; a waxy squeak resonance appears on fast strokes. A frame of zero speed
is silence, so a stroke that stops mid-word stops the sound on that frame.
`crayon_span(frames)` is the M1 fallback (a burst over a reveal span, eased).

paws: `contacts` is [(frame, pan -1..1, size 0..1), ...], one per walk-cycle
contact of each citizen (computed by showc from the crowd: lane, speed, cycle,
ripple). Each is a small soft pad tick, its pitch and level by size, panned by
x. Output is stereo.
"""
from __future__ import annotations

import numpy as np

from .synth import SR, bp, hp, lp, n_of, pink, reson, rng, svf_sweep, white

SPF = SR // 30


def crayon(speed, seed=0):
    sp = np.asarray(speed, dtype=float)
    if not len(sp) or sp.max() <= 0:
        return np.zeros(len(sp) * SPF)
    v = sp / sp.max()
    n = len(v) * SPF
    r = rng("crayon", seed)
    # per-sample control, eased across frame edges (no zipper)
    k = np.repeat(v, SPF)
    k = np.convolve(k, np.ones(160) / 160, mode="same")
    level = np.power(np.clip(k, 0, 1), 0.7)
    grain = white(n, r) * (0.55 + 0.45 * np.sign(np.sin(2 * np.pi * np.cumsum(60 + 90 * k) / SR)))
    body = svf_sweep(grain, 1800 + 2600 * k, q=1.4, mode="bp")
    hiss = hp(white(n, r), 4500) * 0.35
    wax = reson(pink(n, r), [2350, 3900], q=14) * 0.25 * np.clip(k - 0.6, 0, 1) * 2.5
    y = (body + hiss * k + wax) * level
    gate = (np.repeat(v, SPF) > 0).astype(float)
    gate = np.convolve(gate, np.ones(96) / 96, mode="same")
    return y * gate


def crayon_span(frames: int, seed=0):
    """A stroke revealed at a steady pace over `frames` (the M1 fallback)."""
    t = np.linspace(0, 1, frames)
    speed = np.sin(np.pi * np.clip(t * 1.05, 0, 1)) ** 0.5
    return crayon(speed, seed)


def paws(contacts, n_frames: int, seed=0):
    n = n_frames * SPF
    L = np.zeros(n + SR)
    R = np.zeros(n + SR)
    r = rng("paws", seed)
    for k, (fr, pan, size) in enumerate(contacts):
        m = n_of(0.035)
        t = np.arange(m) / SR
        f = 900 + 900 * (1 - size) + r.uniform(-120, 120)
        tick = lp(white(m, r), 2400) * np.exp(-t / 0.006) + 0.5 * np.sin(2 * np.pi * f * 0.4 * t) * np.exp(-t / 0.01)
        tick = bp(tick, f, 0.8) * (0.4 + 0.6 * size) * r.uniform(0.7, 1.0)
        th = (np.clip(pan, -1, 1) + 1) * np.pi / 4
        i = int(fr * SPF + r.integers(0, 200))
        if i < 0 or i >= n:
            continue
        L[i:i + m] += tick * np.cos(th)
        R[i:i + m] += tick * np.sin(th)
    return np.stack([L[:n], R[:n]], 1)


def crowd_contacts(n_walkers=12, n_frames=120, cycle=4, fps_step=8, lane_dir=None, seed=0):
    """A stand-in for showc's crowd data (the library preview): walkers crossing a
    lane both ways, one contact per step, ripple 0-3 f."""
    r = rng("crowd", seed)
    out = []
    for w in range(n_walkers):
        ripple = r.integers(0, 4)
        d = 1 if (lane_dir or (w % 2)) else -1
        x0 = r.uniform(-1.4, -0.6) if d > 0 else r.uniform(0.6, 1.4)
        size = r.uniform(0.3, 0.8)
        for fr in range(ripple, n_frames, fps_step):
            x = x0 + d * fr / n_frames * 2.2
            if -1.1 < x < 1.1:
                out.append((fr, x, size))
    return sorted(out)
