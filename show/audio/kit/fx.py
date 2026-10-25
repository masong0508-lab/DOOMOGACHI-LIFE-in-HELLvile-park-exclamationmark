"""Transition FX, turntable scratches, vinyl noise and formant 'Hey!' shouts.

FX track notes carry meta['fx']:
    riser | downlifter | impact | revcymbal | whoosh | scratch | hey | ahh | vinyl | sample
Risers and reverse cymbals END at note end (they lead into the downbeat);
everything else starts at the note start.  Scratch patterns:
    baby | chirp | transformer | flare | scribble | tear | stab  (meta['pattern'])
"""
from __future__ import annotations

import numpy as np
from numba import njit

from . import dsp
from .core import SR, mtof, place, rng, stereo
from .instrument import Instrument
from .samplers import _play, oneshot


# ---------------------------------------------------------------------------
# transitions
# ---------------------------------------------------------------------------
def riser(dur, sr=SR, seed=0, f0=300.0, f1=11000.0, pitched=None, tail=1.0, noise=1.0):
    """Filtered-noise riser (+ optional pitched supersaw sweep). Peak at `dur`."""
    r = rng("riser", seed)
    N = int((dur + tail) * sr)
    n = int(dur * sr)
    t = np.arange(N) / sr
    x = np.clip(t / dur, 0, 1)
    fc = f0 * (f1 / f0) ** (x ** 1.6)
    L = dsp.svf(r.standard_normal(N), fc, 2.2, 1, float(sr))
    R = dsp.svf(r.standard_normal(N), fc * 1.03, 2.2, 1, float(sr))
    amp = x ** 2.2
    amp[n:] = np.exp(-np.arange(N - n) / (0.03 * sr))
    y = np.stack([L * amp, R * amp], axis=1) * noise
    if pitched is not None:
        semis = pitched + 12 * x ** 1.3
        s = np.zeros(N)
        for k, d in enumerate((-12, -4, 0, 5, 13)):
            s += dsp.saw(mtof(semis + d / 100) / sr, r.random())
        s = dsp.svf(s, 400 + 9000 * x ** 2, 0.8, 0, float(sr)) * amp * 0.12
        y += np.stack([s, s], axis=1)
    return y * 0.5


def downlifter(dur, sr=SR, seed=0, f0=9000.0, f1=200.0):
    r = rng("down", seed)
    N = int(dur * sr)
    t = np.arange(N) / sr
    x = t / dur
    fc = f0 * (f1 / f0) ** (x ** 0.7)
    L = dsp.svf(r.standard_normal(N), fc, 1.8, 1, float(sr))
    R = dsp.svf(r.standard_normal(N), fc * 0.97, 1.8, 1, float(sr))
    amp = (1 - x) ** 1.8 * np.clip(t / 0.005, 0, 1)
    sub = dsp.sine(mtof(48 - 18 * x) / sr) * (1 - x) ** 2 * 0.5
    return np.stack([L * amp + sub, R * amp + sub], axis=1) * 0.45


def impact(sr=SR, seed=0, tune=36.0, length=2.4, boom=True):
    """Cinematic hit: tuned sub boom + noise crack + layered CC0 boom."""
    r = rng("impact", seed)
    N = int(length * sr)
    t = np.arange(N) / sr
    f = mtof(tune) * (1 + 1.5 * np.exp(-t / 0.04))
    sub = np.tanh(2.0 * dsp.sine(f / sr) * np.exp(-6.9 * t / (length * 0.8))) * 0.8
    crack = r.standard_normal(N) * np.exp(-t / 0.06)
    crack = dsp.svf(crack, 9000 * np.exp(-t / 0.15) + 300, 0.7, 0, float(sr)) * 0.6
    y = stereo(sub + crack)
    if boom:
        b = oneshot("sonicpi/misc_cineboom", sr)[:N]
        y[:len(b)] += b * 0.5
    fo = int(0.6 * sr)
    y[-fo:] *= np.cos(np.linspace(0, np.pi / 2, fo))[:, None] ** 2  # no truncation click
    return y * 0.7


def reverse_cymbal(dur, sr=SR, seed=0, sample="vsco2/cymbal/crash1"):
    """Reverse crash swelling into the downbeat (ends exactly at `dur`)."""
    x = dsp.butter(oneshot(sample, sr), "high", 300.0, 2, sr)
    from .reverb import convolve, make_ir
    wet = convolve(x, make_ir("hall", 2.0, sr=sr, seed=seed), sr)
    y = x * 0.8 + wet[:len(x)] * 0.5 if len(wet) >= len(x) else x
    y = np.concatenate([y, wet[len(x):] * 0.5]) if len(wet) > len(x) else y
    y = y[::-1]
    n = int(dur * sr)
    if len(y) >= n:
        y = y[-n:]
    else:
        y = np.concatenate([np.zeros((n - len(y), 2)), y])
    fadein = np.clip(np.arange(n) / (0.3 * n), 0, 1)[:, None] ** 2
    return y * fadein * 0.9


def whoosh(sr=SR, seed=0):
    return oneshot("sonicpi/perc_swoosh", sr) * 0.8


# ---------------------------------------------------------------------------
# vinyl
# ---------------------------------------------------------------------------
def crackle(n, sr=SR, seed=0, density=26.0, pops=0.6, level=0.05):
    """Vinyl surface noise: Poisson clicks (sparse, varied), rare pops, faint hiss."""
    r = rng("crackle", seed)
    y = np.zeros(n)
    k = r.poisson(density * n / sr)
    pos = r.integers(0, n, k)
    amp = r.exponential(1.0, k) * r.choice([-1, 1], k)
    y[pos] += amp
    big = r.integers(0, n, max(1, int(pops * n / sr)))
    y[big] += r.choice([-1, 1], len(big)) * r.uniform(3, 6, len(big))
    y = dsp.butter(y, "high", 900.0, 2, sr)
    y = dsp.lp(y, 7000.0, 0.7)
    hiss = dsp.butter(r.standard_normal(n), "high", 3000.0, 2, sr) * 0.04
    out = (y * 0.25 + hiss) * level
    if seed < 10 ** 6:  # independent (decorrelated) right channel: no comb filter in mono
        other = crackle(n, sr, seed + 10 ** 6, density, pops, level)[:, 0]
        return np.stack([out, 0.6 * out + 0.4 * other], axis=1)
    return np.stack([out, out], axis=1)


# ---------------------------------------------------------------------------
# turntable scratch
# ---------------------------------------------------------------------------
def _stroke(n, frm, to, shape="cos"):
    x = np.arange(n) / max(n - 1, 1)
    e = 0.5 - 0.5 * np.cos(np.pi * x) if shape == "cos" else x
    return frm + (to - frm) * e


def scratch_motion(pattern, beats, spb, sr=SR, length=0.22, seed=0):
    """Hand motion -> (record position [s], crossfader gain) per sample."""
    r = rng("scr", pattern, seed)
    N = int(beats * spb * sr)
    pos = np.zeros(N)
    fader = np.zeros(N)
    step = 0.25 if pattern in ("baby", "chirp", "flare", "tear", "stab") else 0.125
    ns = max(1, int(step * spb * sr))
    i, k = 0, 0
    while i < N:
        m = min(ns, N - i)
        L = length * (0.85 + 0.3 * r.random())
        fwd = (k % 2 == 0)
        a, b = (0.0, L) if fwd else (L, 0.0)
        if pattern == "tear":
            h = m // 2
            mid = a + (b - a) * 0.55
            pos[i:i + h] = _stroke(h, a, mid)
            pos[i + h:i + m] = _stroke(m - h, mid, b)
            fader[i:i + m] = 1.0
        elif pattern == "scribble":
            pos[i:i + m] = 0.05 + 0.025 * np.sin(2 * np.pi * 17.0 * np.arange(m) / sr)
            fader[i:i + m] = 1.0
        elif pattern == "transformer":
            sl = int(4 * ns)
            idx = np.arange(i, min(N, i + m))
            pos[idx] = _stroke(sl, 0.0, L, "lin")[(idx - (i // sl) * sl) % sl] if sl > 0 else 0
            fader[i:i + m] = 1.0 if (k % 2 == 0) else 0.0
        else:
            pos[i:i + m] = _stroke(m, a, b)
            if pattern == "baby":
                fader[i:i + m] = 1.0
            elif pattern == "chirp":
                # fader closes as the record reaches the end of the forward push
                fader[i:i + m] = np.where(np.arange(m) < 0.6 * m, 1.0, 0.0) if fwd else \
                    np.where(np.arange(m) > 0.45 * m, 1.0, 0.0)
            elif pattern == "flare":
                f = np.ones(m)
                c = int(0.5 * m)
                f[c:c + int(0.12 * m)] = 0.0
                fader[i:i + m] = f
            elif pattern == "stab":
                fader[i:i + m] = np.where(np.arange(m) < 0.4 * m, 1.0, 0.0) if fwd else 0.0
        i += m
        k += 1
    # crossfader cut ramps ~0.7 ms (hamster-style click-free cuts)
    kn = max(1, int(0.0012 * sr))
    fader = np.convolve(fader, np.ones(kn) / kn, mode="same")
    return pos, fader


def scratch(src, pattern="baby", beats=1.0, spb=0.4, sr=SR, cue=0.0, length=0.22, seed=0,
            noise=1.0):
    """Turntable scratch of `src` (mono/stereo array) following a hand pattern.

    The platter position is integrated from hand velocity; output level and
    brightness follow the stylus speed; the crossfader cuts; groove noise rides
    the motion; a little static crackle sits underneath.
    """
    x = stereo(src)
    pos, fader = scratch_motion(pattern, beats, spb, sr, length, seed)
    vel = np.gradient(pos) * sr  # record speed relative to normal (1 = 33 1/3 forward)
    rd = (cue + pos) * sr
    rd = np.clip(rd, 1, len(x) - 4)
    ratio = np.diff(rd, prepend=rd[0])
    # _play integrates a ratio; reconstruct with start position
    out = np.stack([_play(np.ascontiguousarray(x[:, c]), ratio, len(rd), 0, 0, float(rd[0]))
                    for c in range(2)], axis=1)
    sp = np.abs(vel)
    gain = np.clip(sp, 0, 1.6) ** 0.6
    out = out * gain[:, None] * fader[:, None]
    # stylus-speed dependent tone: slow = dull
    fc = 600 + 9000 * np.clip(sp, 0, 1.2)
    out = np.stack([dsp.svf(out[:, c], fc, 0.7, 0, float(sr)) for c in range(2)], axis=1)
    r = rng("groove", seed)
    gn = dsp.butter(r.standard_normal(len(rd)), "band", (1500.0, 7000.0), 2, sr) * 0.015
    gn = gn * np.clip(sp, 0, 2) * fader * noise
    cr = crackle(len(rd), sr, seed, level=0.06 * noise)
    return out + gn[:, None] + cr


# ---------------------------------------------------------------------------
# formant voices: gang "Hey!" / "Ahh"
# ---------------------------------------------------------------------------
VOWELS = {  # F1, F2, F3, F4 (adult male, shouted F1 raised) and bandwidths
    "a": ((850, 1220, 2600, 3500), (90, 110, 160, 220)),
    "e": ((560, 1900, 2550, 3500), (70, 100, 150, 220)),
    "i": ((380, 2150, 2850, 3600), (60, 100, 160, 220)),
    "o": ((560, 900, 2500, 3400), (80, 90, 150, 220)),
    "u": ((380, 850, 2300, 3300), (60, 80, 150, 220)),
}


@njit(cache=True)
def _glottal(f0, sr, tilt, jitter_seed):
    """LF-like glottal pulse train: integrated asymmetric pulse, per-sample f0."""
    n = f0.shape[0]
    out = np.empty(n)
    ph = 0.0
    np.random.seed(jitter_seed)
    jit = 1.0
    for i in range(n):
        ph += f0[i] * jit / sr
        if ph >= 1.0:
            ph -= 1.0
            jit = 1.0 + 0.012 * np.random.randn()
        oq = 0.55
        if ph < oq:
            x = ph / oq
            v = 0.5 - 0.5 * np.cos(np.pi * x)
            v = v * (1.0 - x * tilt)
        else:
            x = (ph - oq) / (1 - oq)
            v = (1.0 - tilt) * max(0.0, 1.0 - x * 6.0)
        out[i] = v
    return out


def formant_voice(vowels, dur, f0_start, f0_end, sr=SR, seed=0, tract=1.0, breath=0.25,
                  aspirate=0.06, shout=1.0):
    """One voice: aspirated onset ('h'), vowel glide (e.g. 'e'->'i' = 'hey'), pitch fall."""
    r = rng("vox", seed)
    N = int(dur * sr)
    t = np.arange(N) / sr
    x = t / dur
    f0 = f0_start * (f0_end / f0_start) ** (x ** 0.8)
    f0 *= 1 + 0.01 * np.sin(2 * np.pi * 5.5 * t + r.random() * 6)
    src = _glottal(f0, float(sr), 0.4 / shout, int(r.integers(1, 2 ** 31 - 1)))
    src = np.diff(src, prepend=0.0) * 30.0  # glottal flow derivative
    src = src + breath * r.standard_normal(N) * 0.3
    # aspiration (the 'h') then voicing
    voiced = np.clip((t - aspirate * 0.6) / 0.025, 0, 1)
    asp = r.standard_normal(N) * (1 - voiced) * 0.9
    exc = src * voiced + asp
    seq = list(vowels)
    y = np.zeros(N)
    for fi in range(4):
        if len(seq) == 1:
            fc = np.full(N, VOWELS[seq[0]][0][fi] * tract)
        else:
            a = VOWELS[seq[0]][0][fi]
            b = VOWELS[seq[-1]][0][fi]
            fc = (a + (b - a) * np.clip((x - 0.35) / 0.5, 0, 1)) * tract
        if fi == 0:
            fc = fc * (1 + 0.15 * (shout - 1))
        bw = VOWELS[seq[0]][1][fi]
        q = fc / bw
        amp = (1.0, 0.75, 0.45, 0.25)[fi]
        y += dsp.svf(exc, fc, float(np.mean(q)), 4, float(sr)) * amp
    env = np.clip(t / 0.012, 0, 1) * np.clip((dur - t) / 0.06, 0, 1) ** 1.5
    return y * env


def gang_shout(word="hey", voices=10, sr=SR, seed=0, dur=0.34, pitch=0.0):
    """Gang shout from `voices` formant voices (male+female mix), stereo spread."""
    r = rng("gang", word, seed)
    vow = {"hey": "ei", "ho": "o", "ahh": "a", "oi": "oi", "yeah": "ea", "hai": "ai"}.get(word, "e")
    N = int((dur + 0.08) * sr)
    out = np.zeros((N, 2))
    for k in range(voices):
        female = k % 3 == 2
        f0 = (r.uniform(150, 200) if not female else r.uniform(260, 330)) * 2 ** (pitch / 12)
        tract = r.uniform(0.94, 1.06) * (1.16 if female else 1.0)
        d = dur * r.uniform(0.85, 1.1)
        v = formant_voice(vow, d, f0 * 1.12, f0 * 0.8, sr, seed * 100 + k, tract,
                          breath=0.35, aspirate=0.05 if word in ("hey", "ho", "hai") else 0.0,
                          shout=1.6)
        off = int(abs(r.normal(0, 0.012)) * sr)
        pan = r.uniform(-0.8, 0.8)
        th = (pan + 1) * np.pi / 4
        seg = v[:N - off]
        out[off:off + len(seg), 0] += seg * np.cos(th)
        out[off:off + len(seg), 1] += seg * np.sin(th)
    out = dsp.butter(out, "high", 140.0, 2, sr)
    out = dsp.tape_sat(out / (np.abs(out).max() + 1e-9), 8.0, 0.0)
    return out * 0.5


# ---------------------------------------------------------------------------
# FX instrument
# ---------------------------------------------------------------------------
class FX(Instrument):
    name = "fx"

    def render(self, track, score, sr=SR):
        total = score.n_samples(sr)
        out = np.zeros((total, 2))
        for i, n in enumerate(track.sorted()):
            kind = n.meta.get("fx", "impact")
            dur = n.d * score.spb
            start = score.b2n(n.t, sr)
            g = n.v
            seed = int(n.meta.get("seed", i))
            if kind == "riser":
                y = riser(dur, sr, seed, pitched=n.meta.get("pitched"))
            elif kind == "downlifter":
                y = downlifter(dur, sr, seed)
            elif kind == "impact":
                y = impact(sr, seed, tune=n.p if n.p < 60 else 36)
            elif kind == "revcymbal":
                y = reverse_cymbal(dur, sr, seed)
            elif kind == "whoosh":
                y = whoosh(sr, seed)
            elif kind == "vinyl":
                y = crackle(int(dur * sr), sr, seed, level=0.08)
            elif kind in ("hey", "ho", "ahh", "yeah", "oi", "hai"):
                y = gang_shout(kind, int(n.meta.get("voices", 10)), sr, seed,
                               dur=n.meta.get("len", 0.34), pitch=n.meta.get("pitch", 0.0))
            elif kind == "scratch":
                src = n.meta.get("src", "ahh")
                if isinstance(src, str):
                    if src in ("ahh", "hey"):
                        src_audio = gang_shout(src, 6, sr, seed=99, dur=0.5)
                    else:
                        src_audio = oneshot(src, sr)
                else:
                    src_audio = src
                y = scratch(src_audio, n.meta.get("pattern", "baby"), n.d, score.spb, sr,
                            cue=n.meta.get("cue", 0.0), length=n.meta.get("length", 0.22),
                            seed=seed)
            elif kind == "sample":
                y = oneshot(n.meta["path"], sr)
            else:
                continue
            y = stereo(y) * g
            if kind in ("riser", "revcymbal"):
                end = score.b2n(n.end, sr)
                n_lead = int(dur * sr)
                place(out, y, end - n_lead)
            else:
                place(out, y, start)
        return out
