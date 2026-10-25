"""Instrument base classes: polyphonic (one voice per note) and monophonic legato.

An instrument turns a Track into a stereo stem:  stem = inst.render(track, score)
Subclasses implement voice() (poly) or phrase() (mono).
"""
from __future__ import annotations

import math

import numpy as np
from numba import njit

from .core import SR, mtof, place, rng, stereo


def vel_gain(v, range_db=18.0):
    """Velocity 0..1 -> linear gain (v=1 -> 0 dB, v=0 -> -range_db)."""
    return 10.0 ** ((float(v) - 1.0) * range_db / 20.0)


@njit(cache=True, fastmath=True)
def adsr_sm(gate, trig, a, d, s, r, sr, legato_floor):
    """Gate-driven ADSR state machine (sample accurate, click-free retriggers).

    gate: 1 while held; trig: 1 at (re)trigger samples. The attack is a
    raised-cosine ramp from the current level (no slope jump = no tick on soft
    sounds); decay/release are exponential (d: to sustain within -40 dB,
    r: to -60 dB).
    """
    n = gate.shape[0]
    out = np.empty(n)
    env = 0.0
    stage = 0
    ax = 0.0
    e0 = 0.0
    ai = 1.0 / max(1.0, a * sr)
    dc = math.exp(-4.6 / max(1.0, d * sr))
    rc = math.exp(-6.9 / max(1.0, r * sr))
    for i in range(n):
        if trig[i] > 0.5 or (gate[i] > 0.5 and (stage == 0 or stage == 3)):
            stage = 1
            ax = 0.0
            e0 = env
        if gate[i] < 0.5 and (stage == 1 or stage == 2):
            stage = 3
        if stage == 1:
            ax += ai
            if ax >= 1.0:
                ax = 1.0
                stage = 2
            env = e0 + (1.0 - e0) * (0.5 - 0.5 * math.cos(math.pi * ax))
        elif stage == 2:
            env = s + (env - s) * dc
        elif stage == 3:
            env *= rc
        out[i] = env
    return out


def gate_env(n_on, n_total, a, d, s, r, sr=SR):
    gate = np.zeros(n_total)
    gate[:max(1, n_on)] = 1.0
    trig = np.zeros(n_total)
    trig[0] = 1.0
    return adsr_sm(gate, trig, a, d, s, r, sr, 0.0)


class Instrument:
    """Polyphonic instrument: each note rendered as an independent voice."""

    name = "instrument"
    defaults: dict = {}
    tail = 0.5            # seconds of release rendered after note-off
    mono = False

    def __init__(self, **params):
        self.p = dict(self.defaults)
        self.p.update(params)

    def __repr__(self):
        return f"<{type(self).__name__} {self.p}>"

    # ---- pitch helpers ---------------------------------------------------
    def pitch_curve(self, n, n_total, spb, sr, vib=True):
        """Semitone curve for one note: bend envelope + delayed vibrato."""
        t = np.arange(n_total) / sr
        semis = np.full(n_total, float(n.p))
        if n.bend:
            bt = np.array([b[0] for b in n.bend]) * spb
            bv = np.array([b[1] for b in n.bend], dtype=float)
            semis += np.interp(t, bt, bv)
        depth = self.p.get("vib_depth", 0.0)  # cents
        if vib and depth > 0:
            dl = self.p.get("vib_delay", 0.3)
            fade = self.p.get("vib_fade", 0.25)
            rate = self.p.get("vib_rate", 5.5)
            ramp = np.clip((t - dl) / max(fade, 1e-3), 0, 1)
            r = rng("vib", n.t, n.p)
            ph = r.random() * 2 * np.pi
            drift = 1.0 + 0.04 * np.sin(2 * np.pi * 0.7 * t + ph)
            semis += depth / 100.0 * ramp * np.sin(2 * np.pi * rate * t * drift + ph)
        return semis

    # ---- rendering -------------------------------------------------------
    def voice(self, n, dur_s, semis, sr, idx):
        raise NotImplementedError

    def render(self, track, score, sr=SR):
        total = score.n_samples(sr)
        out = np.zeros((total, 2))
        for i, n in enumerate(track.sorted()):
            start = score.b2n(n.t, sr)
            dur_s = max(0.005, n.d * score.spb)
            n_total = int((dur_s + self.tail_for(n)) * sr)
            semis = self.pitch_curve(n, n_total, score.spb, sr)
            y = self.voice(n, dur_s, semis, sr, i)
            if y is None:
                continue
            place(out, stereo(y), start)
        return self.post(out, track, score, sr)

    def tail_for(self, n):
        return self.p.get("r", self.tail) + 0.05

    def post(self, out, track, score, sr):
        return out


class MonoInstrument(Instrument):
    """Monophonic legato voice: glides, legato (no retrigger) and pitch bends.

    Notes that overlap / touch with 'legato' or 'slide' flags are joined into a
    phrase and rendered with one continuous oscillator.
    """

    mono = True
    defaults = {"glide": 0.06}

    def phrases(self, notes, spb):
        notes = sorted(notes, key=lambda n: n.t)
        out, cur = [], []
        for n in notes:
            if cur:
                if self.joined(cur[-1], n, spb):
                    cur.append(n)
                    continue
                out.append(cur)
            cur = [n]
        if cur:
            out.append(cur)
        return out

    @staticmethod
    def joined(prev, n, spb):
        flag = n.has("legato") or n.has("slide") or n.glide > 0
        gap_s = (n.t - prev.end) * spb
        if flag and gap_s < 0.02:
            return True
        return -gap_s > 0.03  # a real overlap = legato playing

    def render(self, track, score, sr=SR):
        total = score.n_samples(sr)
        out = np.zeros((total, 2))
        spb = score.spb
        for ph_i, ph in enumerate(self.phrases(track.notes, spb)):
            s0 = score.b2n(ph[0].t, sr)
            s_end = score.b2n(max(n.end for n in ph), sr)
            tail = int(self.p.get("r", 0.2) * sr) + int(0.05 * sr)
            N = s_end - s0 + tail
            pitch = np.zeros(N)
            gate = np.zeros(N)
            trig = np.zeros(N)
            vel = np.zeros(N)
            note_start = np.zeros(N)  # seconds since the current note began
            prev_p = None
            for k, n in enumerate(ph):
                a = score.b2n(n.t, sr) - s0
                b = score.b2n(ph[k + 1].t, sr) - s0 if k + 1 < len(ph) else N
                e = score.b2n(n.end, sr) - s0
                seg = np.arange(b - a)
                p = np.full(b - a, float(n.p))
                g_time = n.glide if n.glide > 0 else (self.p.get("glide", 0.06) / spb
                                                        if n.has("slide") else 0.0)
                if prev_p is not None and g_time > 0:
                    gs = max(1, int(g_time * spb * sr))
                    x = np.clip(seg / gs, 0, 1)
                    x = 1 - (1 - x) ** 2
                    p = prev_p + (n.p - prev_p) * x
                if n.bend:
                    bt = np.array([q[0] for q in n.bend]) * spb * sr
                    bv = np.array([q[1] for q in n.bend], dtype=float)
                    p = p + np.interp(seg, bt, bv)
                pitch[a:b] = p
                gate[a:min(e, N)] = 1.0
                vel[a:b] = n.v
                note_start[a:b] = seg / sr
                if k == 0 or n.has("accent"):
                    trig[a] = 1.0
                prev_p = float(n.p)
            # delayed vibrato per note
            depth = self.p.get("vib_depth", 0.0)
            if depth > 0:
                dl = self.p.get("vib_delay", 0.3)
                fade = self.p.get("vib_fade", 0.25)
                rate = self.p.get("vib_rate", 5.5)
                ramp = np.clip((note_start - dl) / max(fade, 1e-3), 0, 1)
                t = np.arange(N) / sr
                pitch = pitch + depth / 100.0 * ramp * np.sin(2 * np.pi * rate * t)
            y = self.phrase(pitch, gate, trig, vel, sr, ph_i, ph)
            if y is not None:
                place(out, stereo(y), s0)
        return self.post(out, track, score, sr)

    def phrase(self, pitch, gate, trig, vel, sr, idx, notes):
        raise NotImplementedError

    def env(self, gate, trig, sr, a=None, d=None, s=None, r=None):
        p = self.p
        return adsr_sm(gate, trig, p.get("a", 0.005) if a is None else a,
                       p.get("d", 0.3) if d is None else d,
                       p.get("s", 0.8) if s is None else s,
                       p.get("r", 0.15) if r is None else r, float(sr), 0.0)


def freq(semis):
    return mtof(semis)
