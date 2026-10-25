"""Walla beds (spec §6.2, §10.5): `walla kids_far lines=w_kids_01..w_kids_06 density=1.4/s
pan=spread seed=7` in lines.shw becomes a stereo bed built from those lines' locked takes.

bed(ep, bed_id, n_samples) -> float32 (n, 2) at 48 kHz. Deterministic: the event times
are a Poisson stream of `density` events a second drawn from `seed`; each event plays the
next line of a seeded shuffle (every line before any repeats), at a random gain of
0..-6 dB, panned across +/-0.8 for pan=spread (or a fixed pan=x). The bed's own level is
0 dB peak-safe; the mixer applies the `walla <bed> on|to gain=` automation from
audio_events.json on top. Sample 0 of the bed = the `walla ... on` event's sample.
"""
from __future__ import annotations

import os

import numpy as np

from . import env, lock, shwio, takes


def _density(v):
    v = str(v).replace("/s", "")
    return float(v)


def bed(ep, bed_id, n, lead_in=0):
    d = env.ep_dir(ep)
    lines, beds = shwio.read_lines(os.path.join(d, "lines.shw"))
    if bed_id not in beds:
        raise KeyError(f"{ep}: no walla bed {bed_id} in lines.shw")
    b = beds[bed_id]
    rows = lock.read(d)["lines"]
    clips = []
    for lid in b.lines:
        r = rows.get(lid)
        if not r or not r.get("take"):
            raise KeyError(f"walla {bed_id}: line {lid} is not locked (run `show voice {ep}`)")
        clips.append(takes.load(env.unrel(r["take"])))
    rng = np.random.default_rng(int(b.attrs.get("seed", 0)))
    dens = _density(b.attrs.get("density", "1.0/s"))
    pan_mode = b.attrs.get("pan", "spread")
    out = np.zeros((n + env.SR * 4, 2), np.float32)
    t = -lead_in - rng.exponential(1.0 / dens) * env.SR * 0.5   # the bed is already going at sample 0
    order = []
    while t < n:
        if not order:
            order = list(rng.permutation(len(clips)))
        k = order.pop(0)
        g = 10 ** (-rng.uniform(0, 6) / 20)
        pan = rng.uniform(-0.8, 0.8) if pan_mode == "spread" else float(pan_mode)
        y = clips[k] * g
        lft, rgt = np.cos((pan + 1) * np.pi / 4), np.sin((pan + 1) * np.pi / 4)
        a = int(t)
        s0 = max(0, -a)
        if s0 < len(y):
            seg = y[s0:][: len(out) - (a + s0)]
            out[a + s0:a + s0 + len(seg), 0] += seg * lft
            out[a + s0:a + s0 + len(seg), 1] += seg * rgt
        t += rng.exponential(1.0 / dens) * env.SR
    out = out[:n]
    pk = np.abs(out).max()
    if pk > 0.89:
        out *= 0.89 / pk
    return out


def write_bed(ep, bed_id, frames, path):
    import soundfile as sf
    y = bed(ep, bed_id, frames * env.SPF)
    sf.write(path, y, env.SR, subtype="PCM_24")
    print(f"{path}: {frames} f, {y.shape[0]} smp, peak {20 * np.log10(np.abs(y).max() + 1e-9):.1f} dBFS")
    return path
