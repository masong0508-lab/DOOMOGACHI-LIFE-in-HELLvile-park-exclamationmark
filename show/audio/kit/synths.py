"""Synthesised instruments (band-limited, sample-accurate modulation).

Leads/pads/keys:  Supersaw, SawLead, Pluck, FMEPiano, Bell (celesta / glock /
music box / fm), Chip (pulse 12.5/25/50 %, 4-bit triangle, LFSR noise),
WarmPad, StringPad (Solina-style ensemble), BrassStab, Organ.
Basses:           SubBass, Bass808, Reese, SynthBass, PickBass (finger / slap /
pop / ghost / mute articulations, Karplus-Strong).
Guitar:           DoomGuitar (drop-tuned power chords, palm mutes, double-
tracked, oversampled amp + 4x12 cab simulation).
"""
from __future__ import annotations

import math

import numpy as np
from scipy import signal

from . import dsp
from .core import SR, mtof, place, rng
from .instrument import Instrument, MonoInstrument, adsr_sm, gate_env, vel_gain

JP_OFFS = np.array([-0.11002313, -0.06288439, -0.01952356, 0.0,
                    0.01991221, 0.06216538, 0.10745242]) / 0.11002313


def _env(n_on, n_total, p, sr, **over):
    q = dict(a=p.get("a", 0.005), d=p.get("d", 0.3), s=p.get("s", 0.8), r=p.get("r", 0.2))
    q.update(over)
    return gate_env(n_on, n_total, q["a"], q["d"], q["s"], q["r"], sr)


def _keytrack(semis, amount=0.5, ref=60):
    return 2.0 ** ((np.mean(semis) - ref) / 12.0 * amount)


# ===========================================================================
# leads and pads
# ===========================================================================
class Supersaw(Instrument):
    """JP-8000 style supersaw: 7 band-limited saws, non-linear detune spread."""

    name = "supersaw"
    defaults = dict(voices=7, detune=24.0, side=0.62, width=0.85, cutoff=5200.0,
                    env_amt=5000.0, env_decay=0.35, q=0.9, a=0.004, d=0.6, s=0.75, r=0.35,
                    vib_depth=0.0, vib_rate=5.4, vib_delay=0.35, gain=0.22, keytrack=0.4,
                    hp=0.75)

    def voice(self, n, dur_s, semis, sr, idx):
        p = self.p
        N = len(semis)
        n_on = int(dur_s * sr)
        r = rng("ssaw", n.t, n.p, idx)
        f = mtof(semis)
        L = np.zeros(N)
        R = np.zeros(N)
        nv = int(p["voices"])
        offs = JP_OFFS if nv == 7 else np.linspace(-1, 1, nv)
        for k, off in enumerate(offs):
            fk = f * 2.0 ** (off * p["detune"] / 1200.0)
            s = dsp.saw(fk / sr, r.random())
            g = 1.0 if off == 0 else p["side"]
            pan = off * p["width"]
            th = (pan + 1) * math.pi / 4
            L += s * g * math.cos(th)
            R += s * g * math.sin(th)
        t = np.arange(N) / sr
        kt = _keytrack(semis, p["keytrack"])
        fc = (p["cutoff"] + p["env_amt"] * n.v * np.exp(-t / p["env_decay"])) * kt
        L = dsp.svf(L, fc, p["q"], 0, float(sr))
        R = dsp.svf(R, fc, p["q"], 0, float(sr))
        if p["hp"]:
            hpf = np.full(N, mtof(n.p) * p["hp"])
            L = dsp.svf(L, hpf, 0.6, 2, float(sr))
            R = dsp.svf(R, hpf, 0.6, 2, float(sr))
        env = _env(n_on, N, p, sr) * vel_gain(n.v) * p["gain"]
        return np.stack([L * env, R * env], axis=1)


class SawLead(MonoInstrument):
    """Mono saw/square lead: 3-voice unison + sub, ladder filter, glide, vibrato."""

    name = "saw_lead"
    defaults = dict(glide=0.05, unison=3, detune=9.0, square=0.35, sub=0.25, duty=0.5,
                    cutoff=3200.0, env_amt=4200.0, env_decay=0.25, res=0.25, drive=1.4,
                    a=0.004, d=0.4, s=0.8, r=0.18, vib_depth=22.0, vib_rate=5.6,
                    vib_delay=0.28, vib_fade=0.3, gain=0.42, keytrack=0.5, width=0.25)

    def phrase(self, pitch, gate, trig, vel, sr, idx, notes):
        p = self.p
        N = len(pitch)
        r = rng("lead", idx, notes[0].t)
        f = mtof(pitch)
        L = np.zeros(N)
        R = np.zeros(N)
        u = int(p["unison"])
        for k in range(u):
            off = 0.0 if u == 1 else (k / (u - 1) * 2 - 1)
            fk = f * 2.0 ** (off * p["detune"] / 1200)
            s = dsp.saw(fk / sr, r.random())
            if p["square"] > 0:
                s = s + p["square"] * dsp.pulse(fk / sr, np.full(N, p["duty"]), r.random())
            pan = off * p["width"]
            L += s * (1 - pan) * 0.5
            R += s * (1 + pan) * 0.5
        if p["sub"] > 0:
            sub = dsp.pulse(f / sr / 2, np.full(N, 0.5), 0.0) * p["sub"]
            L += sub * 0.5
            R += sub * 0.5
        # filter envelope retriggered with the amp envelope
        fenv = adsr_sm(gate, trig, 0.002, p["env_decay"], 0.0, 0.2, float(sr), 0.0)
        kt = 2.0 ** ((pitch - 60) / 12.0 * p["keytrack"])
        fc = (p["cutoff"] + p["env_amt"] * fenv * (0.5 + 0.5 * vel)) * kt
        L = dsp.ladder(L * 0.5, fc, p["res"], p["drive"], float(sr))
        R = dsp.ladder(R * 0.5, fc, p["res"], p["drive"], float(sr))
        env = self.env(gate, trig, sr)
        g = 10.0 ** ((vel - 1.0) * 18.0 / 20.0) * p["gain"] * 2
        return np.stack([L * env * g, R * env * g], axis=1)


class Pluck(Instrument):
    """Bright synth pluck (two detuned saws + 25 % pulse, fast filter envelope)."""

    name = "pluck"
    defaults = dict(detune=7.0, cutoff=500.0, env_amt=7000.0, env_decay=0.09, q=1.1,
                    a=0.001, d=0.35, s=0.0, r=0.2, gain=0.33, width=0.6, keytrack=0.6)

    def voice(self, n, dur_s, semis, sr, idx):
        p = self.p
        N = len(semis)
        r = rng("pluck", n.t, n.p)
        f = mtof(semis)
        a = dsp.saw(f * 2 ** (p["detune"] / 1200) / sr, r.random())
        b = dsp.saw(f * 2 ** (-p["detune"] / 1200) / sr, r.random())
        c = 0.5 * dsp.pulse(f / sr, np.full(N, 0.25), r.random())
        t = np.arange(N) / sr
        kt = _keytrack(semis, p["keytrack"])
        fc = (p["cutoff"] + p["env_amt"] * (0.4 + 0.6 * n.v) * np.exp(-t / p["env_decay"])) * kt
        w = p["width"]
        L = dsp.svf(a * (1 + w) / 2 + b * (1 - w) / 2 + c, fc, p["q"], 0, float(sr))
        R = dsp.svf(a * (1 - w) / 2 + b * (1 + w) / 2 + c, fc, p["q"], 0, float(sr))
        env = _env(int(dur_s * sr), N, p, sr) * vel_gain(n.v) * p["gain"]
        return np.stack([L * env, R * env], axis=1)


class FMEPiano(Instrument):
    """DX-style FM electric piano: tine pair (1:14) + body pair (1:1)."""

    name = "fm_epiano"
    tail = 0.6
    defaults = dict(tine=1.0, body=1.0, bark=1.0, r=0.35, gain=0.32, detune=1.5,
                    decay_scale=1.0)

    def voice(self, n, dur_s, semis, sr, idx):
        p = self.p
        N = len(semis)
        n_on = int(dur_s * sr)
        t = np.arange(N) / sr
        f = mtof(semis)
        v = n.v
        ph1 = 2 * np.pi * np.cumsum(f * 2 ** (p["detune"] / 1200)) / sr
        ph2 = 2 * np.pi * np.cumsum(f * 2 ** (-p["detune"] / 1200)) / sr
        # decay times shorter for high notes (like real tines)
        hz = mtof(n.p)
        t60 = float(np.clip(5.5 * (261.0 / hz) ** 0.55, 0.9, 7.0)) * p["decay_scale"]
        # keep the 14:1 tine sidebands below Nyquist on high notes (no aliasing)
        alias_guard = float(np.clip((0.42 * sr - 14.0 * hz) / (6.0 * hz), 0.0, 1.0))
        i_tine = ((0.4 + 2.4 * v) * np.exp(-t / 0.009) + 0.06) * alias_guard
        tine = np.sin(ph1 + i_tine * np.sin(14.0 * ph1))
        i_body = (0.25 + 1.9 * v ** 2 * p["bark"]) * np.exp(-t / (0.35 + 0.6 * (1 - v)))
        body = np.sin(ph2 + i_body * np.sin(ph2))
        amp_t = np.exp(-6.9 * t / (t60 * 0.35))
        amp_b = np.exp(-6.9 * t / t60)
        rel = np.ones(N)
        if n_on < N:
            k = np.arange(N - n_on)
            rel[n_on:] = np.exp(-6.9 * k / (p["r"] * sr))
        att = np.clip(t / 0.0015, 0, 1)
        y_t = tine * amp_t * (0.18 + 0.32 * v) * p["tine"]
        y_b = body * amp_b * p["body"]
        g = vel_gain(v, 22) * p["gain"] * att * rel
        L = (0.62 * y_t + 0.48 * y_b) * g
        R = (0.48 * y_t + 0.62 * y_b) * g
        return np.stack([L, R], axis=1)


BELLS = {
    # (ratio, amp, t60 multiplier)
    "glock": [(1.0, 1.0, 1.0), (2.756, 0.32, 0.35), (5.404, 0.12, 0.15), (8.933, 0.05, 0.08)],
    "celesta": [(1.0, 1.0, 1.0), (2.0, 0.10, 0.5), (3.98, 0.20, 0.18), (9.2, 0.04, 0.06)],
    "musicbox": [(1.0, 1.0, 1.0), (5.43, 0.18, 0.12), (8.9, 0.06, 0.07), (2.0, 0.05, 0.5)],
    "vibes": [(1.0, 1.0, 1.0), (3.94, 0.12, 0.25), (9.9, 0.03, 0.1)],
    "tubular": [(1.0, 0.7, 1.0), (2.76, 1.0, 0.8), (5.40, 0.6, 0.5), (8.93, 0.35, 0.3),
                (13.34, 0.15, 0.2)],
}


class Bell(Instrument):
    """Additive mallet / bell family: kind = glock | celesta | musicbox | vibes | tubular | fm."""

    name = "bell"
    tail = 2.5
    defaults = dict(kind="celesta", t60=2.2, gain=0.3, hammer=0.25, r=0.8, beat=0.6,
                    tremolo=0.0, fm_ratio=3.5, fm_index=2.2)

    def tail_for(self, n):
        return self.p["t60"] + 0.1

    def voice(self, n, dur_s, semis, sr, idx):
        p = self.p
        N = len(semis)
        t = np.arange(N) / sr
        f0 = mtof(semis)
        r = rng("bell", n.t, n.p)
        hz = float(mtof(n.p))
        t60 = p["t60"] * float(np.clip((523.0 / hz) ** 0.35, 0.4, 2.0))
        y = np.zeros(N)
        if p["kind"] == "fm":
            ph = 2 * np.pi * np.cumsum(f0) / sr
            idx_env = p["fm_index"] * (0.5 + 0.7 * n.v) * np.exp(-t / (t60 * 0.25))
            y = np.sin(ph + idx_env * np.sin(p["fm_ratio"] * ph)) * np.exp(-6.9 * t / t60)
        else:
            for k, (ratio, amp, dm) in enumerate(BELLS[p["kind"]]):
                det = 1.0 + r.normal(0, 0.0006) * (k > 0)
                ph = 2 * np.pi * np.cumsum(f0 * ratio * det) / sr + r.random() * 6.28
                if ratio * hz > 0.45 * sr:
                    continue
                part = np.sin(ph) * amp * np.exp(-6.9 * t / (t60 * dm))
                if k == 0 and p["beat"] > 0:  # slow beating pair, like real bars
                    part = part * (1 + 0.04 * p["beat"] * np.sin(2 * np.pi * 1.3 * t))
                y += part
        if p["tremolo"] > 0:
            y *= 1 - p["tremolo"] * 0.5 * (1 + np.sin(2 * np.pi * 5.0 * t))
        # hammer / mallet transient
        if p["hammer"] > 0:
            nh = int(0.006 * sr)
            click = r.standard_normal(nh) * np.exp(-np.arange(nh) / (0.0012 * sr))
            click = dsp.bp(click, min(hz * 4, 9000.0), 1.2)
            y[:nh] += click * p["hammer"] * n.v
        att = np.clip(t / 0.0008, 0, 1)
        n_on = int(dur_s * sr)
        rel = np.ones(N)
        if p["kind"] in ("vibes", "musicbox") and n_on < N:
            rel[n_on:] = np.exp(-6.9 * np.arange(N - n_on) / (p["r"] * sr))
        y = y * att * rel * vel_gain(n.v, 20) * p["gain"]
        return np.stack([y, y], axis=1)


class Chip(Instrument):
    """GB/NES voices: wave = p12 | p25 | p50 | p75 | tri | noise | noise7.

    params: env = (start_vol 0-15, step_dir -1/0/+1, step_period/64 s) Game-Boy
    volume envelope (quantised to 16 levels); arp = semitone offsets cycled at
    arp_rate Hz (note.meta['arp'] overrides); vib in cents; slide in st/s.
    """

    name = "chip"
    defaults = dict(wave="p25", env=(13, -1, 3), arp=None, arp_rate=60.0, vib_depth=0.0,
                    vib_rate=6.0, vib_delay=0.2, gain=0.16, quantise=True, lp=11000.0,
                    noise_rate=None, r=0.03, sweep=0.0)

    def tail_for(self, n):
        return 0.06

    def voice(self, n, dur_s, semis, sr, idx):
        p = self.p
        N = len(semis)
        n_on = int(dur_s * sr)
        t = np.arange(N) / sr
        arp = n.meta.get("arp", p["arp"])
        if arp:
            step = (np.floor(t * p["arp_rate"]).astype(int)) % len(arp)
            semis = semis + np.asarray(arp)[step]
        if p["sweep"]:
            semis = semis + p["sweep"] * t
        f = mtof(semis)
        w = n.meta.get("wave", p["wave"])
        if w.startswith("p"):
            duty = {"p12": 0.125, "p25": 0.25, "p50": 0.5, "p75": 0.75}[w]
            y = dsp.pulse(f / sr, np.full(N, duty), 0.0)
        elif w == "tri":
            y = dsp.tri(f / sr, 0.0, 16)
        else:
            rate = p["noise_rate"] or float(mtof(n.p)) * 8
            y = dsp.lfsr_noise(N, rate, float(sr), w == "noise7", 0x5A5A)
        v0, direction, per = n.meta.get("env", p["env"])
        frames = np.floor(t * 64.0 / max(per, 1)).astype(int) if direction else np.zeros(N, int)
        vol = np.clip(v0 + direction * frames, 0, 15)
        if p["quantise"]:
            vol = np.round(vol * (0.4 + 0.6 * n.v))
        amp = dsp.onepole_lp(vol / 15.0, np.full(N, 900.0), float(sr))  # 4-bit steps, de-clicked
        gate = np.ones(N)
        if n_on < N:
            gate[n_on:] = np.exp(-np.arange(N - n_on) / (p["r"] * sr))
        y = y * amp * gate * p["gain"]
        y = dsp.butter(y, "low", p["lp"], 2, sr)
        y = dsp.butter(y, "high", 30.0, 1, sr)
        return np.stack([y, y], axis=1)


class WarmPad(Instrument):
    """Analogue pad: 3 saws + PWM pulse, ladder low-pass, slow attack, stereo drift."""

    name = "warm_pad"
    tail = 1.6
    defaults = dict(detune=10.0, pwm=0.35, cutoff=1600.0, res=0.15, lfo=0.25, lfo_amt=0.25,
                    a=0.45, d=1.0, s=0.85, r=1.4, gain=0.16, width=0.9, keytrack=0.3)

    def voice(self, n, dur_s, semis, sr, idx):
        p = self.p
        N = len(semis)
        r = rng("pad", n.t, n.p)
        f = mtof(semis)
        t = np.arange(N) / sr
        L = np.zeros(N)
        R = np.zeros(N)
        for k, off in enumerate((-1.0, 0.0, 1.0)):
            drift = 1 + 0.0012 * np.sin(2 * np.pi * (0.13 + 0.07 * k) * t + r.random() * 6)
            s = dsp.saw(f * drift * 2 ** (off * p["detune"] / 1200) / sr, r.random())
            pan = off * p["width"]
            L += s * (1 - pan) * 0.5
            R += s * (1 + pan) * 0.5
        duty = 0.5 + p["pwm"] * 0.45 * np.sin(2 * np.pi * 0.31 * t + r.random() * 6)
        pw = dsp.pulse(f / sr, duty, r.random()) * 0.5
        L += pw * 0.5
        R += pw * 0.5
        kt = _keytrack(semis, p["keytrack"])
        fc = p["cutoff"] * kt * (1 + p["lfo_amt"] * np.sin(2 * np.pi * p["lfo"] * t + r.random()))
        L = dsp.ladder(L * 0.4, fc, p["res"], 1.2, float(sr))
        R = dsp.ladder(R * 0.4, fc, p["res"], 1.2, float(sr))
        env = _env(int(dur_s * sr), N, p, sr) * vel_gain(n.v, 12) * p["gain"] * 2.5
        return np.stack([L * env, R * env], axis=1)


class StringPad(Instrument):
    """String-machine ensemble (Solina-like): saw pairs + triple BBD chorus."""

    name = "string_pad"
    tail = 1.2
    defaults = dict(detune=5.0, cutoff=5200.0, hp=180.0, a=0.22, d=0.5, s=0.9, r=0.9,
                    gain=0.14, ensemble=1.0)

    def voice(self, n, dur_s, semis, sr, idx):
        p = self.p
        N = len(semis)
        r = rng("str", n.t, n.p)
        f = mtof(semis)
        s = dsp.saw(f * 2 ** (p["detune"] / 1200) / sr, r.random()) + \
            dsp.saw(f * 2 ** (-p["detune"] / 1200) / sr, r.random()) + \
            0.4 * dsp.saw(f * 2 / sr, r.random())
        s = dsp.lp(s, p["cutoff"], 0.7)
        s = dsp.hp(s, p["hp"], 0.7)
        env = _env(int(dur_s * sr), N, p, sr) * vel_gain(n.v, 12) * p["gain"]
        return s * env

    def post(self, out, track, score, sr):
        if self.p["ensemble"] <= 0:
            return out
        return ensemble(out[:, 0] * 0.5 + out[:, 1] * 0.5, sr, self.p["ensemble"])


def ensemble(x, sr=SR, depth=1.0):
    """Triple-phase BBD ensemble (0.6 Hz chorus + 6 Hz vibrato), mono in -> stereo out."""
    N = len(x)
    t = np.arange(N) / sr
    outs = []
    for k in range(3):
        ph = 2 * np.pi * k / 3
        d_ms = 7.0 + depth * (2.2 * np.sin(2 * np.pi * 0.62 * t + ph) +
                              0.35 * np.sin(2 * np.pi * 6.1 * t + ph * 1.7))
        outs.append(dsp.mod_delay(x, d_ms * 1e-3 * sr, 0.0, 1.0, 0.0))
    L = 0.55 * x + 0.6 * outs[0] + 0.35 * outs[1] - 0.15 * outs[2]
    R = 0.55 * x + 0.6 * outs[2] + 0.35 * outs[1] - 0.15 * outs[0]
    return np.stack([L, R], axis=1) * 0.8


class BrassStab(Instrument):
    """Synth brass: detuned saw stack, 'bwah' filter sweep, pitch scoop."""

    name = "brass"
    defaults = dict(detune=7.0, cutoff=900.0, env_amt=5200.0, env_a=0.035, env_d=0.25, q=0.9,
                    scoop=-0.35, scoop_t=0.05, a=0.012, d=0.25, s=0.7, r=0.16, gain=0.2,
                    width=0.6)

    def voice(self, n, dur_s, semis, sr, idx):
        p = self.p
        N = len(semis)
        r = rng("brass", n.t, n.p)
        t = np.arange(N) / sr
        semis = semis + p["scoop"] * np.exp(-t / p["scoop_t"])
        f = mtof(semis)
        L = np.zeros(N)
        R = np.zeros(N)
        for k, off in enumerate((-1.0, -0.4, 0.4, 1.0)):
            s = dsp.saw(f * 2 ** (off * p["detune"] / 1200) / sr, r.random())
            pan = off * p["width"]
            L += s * (1 - pan) * 0.5
            R += s * (1 + pan) * 0.5
        rise = np.clip(t / p["env_a"], 0, 1)
        fenv = rise * np.exp(-np.maximum(t - p["env_a"], 0) / p["env_d"]) * 0.7 + 0.3 * rise
        fc = p["cutoff"] + p["env_amt"] * (0.4 + 0.6 * n.v) * fenv
        L = dsp.svf(L, fc, p["q"], 0, float(sr))
        R = dsp.svf(R, fc, p["q"], 0, float(sr))
        env = _env(int(dur_s * sr), N, p, sr) * vel_gain(n.v) * p["gain"]
        return np.stack([L * env, R * env], axis=1)


class Organ(Instrument):
    """Tonewheel organ: drawbars (16' 8' 5 1/3' 4' 2' ...), key click, rotary speaker."""

    name = "organ"
    defaults = dict(drawbars=(8, 8, 6, 0, 0, 0, 0, 0, 3), click=0.15, a=0.004, d=0.1, s=1.0,
                    r=0.06, gain=0.12, rotary=1.0, rotor_hz=6.2)
    RATIOS = (0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0)

    def voice(self, n, dur_s, semis, sr, idx):
        p = self.p
        N = len(semis)
        f = mtof(semis)
        r = rng("org", n.t, n.p)
        ph = 2 * np.pi * np.cumsum(f) / sr
        y = np.zeros(N)
        for ratio, db in zip(self.RATIOS, p["drawbars"]):
            if db <= 0 or ratio * float(mtof(n.p)) > 0.45 * sr:
                continue
            y += (db / 8.0) * np.sin(ratio * ph + r.random() * 6)
        nclick = int(0.004 * sr)
        y[:nclick] += p["click"] * r.standard_normal(nclick) * np.linspace(1, 0, nclick)
        env = _env(int(dur_s * sr), N, p, sr) * vel_gain(n.v, 6) * p["gain"]
        return y * env

    def post(self, out, track, score, sr):
        if self.p["rotary"] <= 0:
            return out
        x = out.mean(axis=1)
        t = np.arange(len(x)) / sr
        lo = dsp.butter(x, "low", 800.0, 2, sr)
        hi = x - lo
        rate = self.p["rotor_hz"]
        dh = (0.6 + 0.35 * np.sin(2 * np.pi * rate * t)) * 1e-3 * sr
        dl = (0.6 + 0.35 * np.sin(2 * np.pi * rate * t + np.pi)) * 1e-3 * sr
        hl = dsp.mod_delay(hi, dh, 0.0, 1.0, 0.0) * (1 + 0.3 * np.sin(2 * np.pi * rate * t))
        hr = dsp.mod_delay(hi, dl, 0.0, 1.0, 0.0) * (1 - 0.3 * np.sin(2 * np.pi * rate * t))
        am = 1 + 0.12 * np.sin(2 * np.pi * rate * 0.85 * t)
        return np.stack([lo * am + hl, lo * am + hr], axis=1)


# ===========================================================================
# basses (mono, mono-compatible: all < 150 Hz content is centred)
# ===========================================================================
class SubBass(MonoInstrument):
    name = "sub_bass"
    defaults = dict(glide=0.04, a=0.003, d=0.3, s=0.9, r=0.06, gain=0.55, harm=0.18, drive=1.3)

    def phrase(self, pitch, gate, trig, vel, sr, idx, notes):
        p = self.p
        f = mtof(pitch)
        y = dsp.sine(f / sr, 0.0)
        y = np.tanh(p["drive"] * (y + p["harm"] * dsp.sine(2 * f / sr, 0.25))) / np.tanh(p["drive"])
        env = self.env(gate, trig, sr)
        return y * env * vel * p["gain"]


class Bass808(MonoInstrument):
    """Tuned 808 boom: sine with pitch-drop transient, long decay, saturation, glides."""

    name = "bass808"
    defaults = dict(glide=0.08, a=0.001, d=1.6, s=0.0, r=0.12, gain=0.5, drop=9.0,
                    drop_t=0.016, drive=2.2, click=0.25)

    def phrase(self, pitch, gate, trig, vel, sr, idx, notes):
        p = self.p
        N = len(pitch)
        # pitch drop at every trigger
        tt = np.zeros(N)
        last = -10 ** 9
        trig_idx = np.flatnonzero(trig)
        k = 0
        for i in range(N):
            if k < len(trig_idx) and i == trig_idx[k]:
                last = i
                k += 1
            tt[i] = (i - last) / sr
        semis = pitch + p["drop"] * np.exp(-tt / p["drop_t"])
        y = dsp.sine(mtof(semis) / sr, 0.0)
        env = self.env(gate, trig, sr)
        y = np.tanh(p["drive"] * y * env) / np.tanh(p["drive"])
        click = p["click"] * np.exp(-tt / 0.003) * dsp.sine(np.full(N, 1800.0 / sr), 0.0)
        return (y + click * env) * vel * p["gain"]


class Reese(MonoInstrument):
    name = "reese"
    defaults = dict(glide=0.05, detune=16.0, cutoff=650.0, lfo=0.18, lfo_amt=0.35, res=0.2,
                    a=0.006, d=0.4, s=0.9, r=0.1, gain=0.42, sub=0.6)

    def phrase(self, pitch, gate, trig, vel, sr, idx, notes):
        p = self.p
        N = len(pitch)
        f = mtof(pitch)
        r = rng("reese", idx)
        a = dsp.saw(f * 2 ** (p["detune"] / 1200) / sr, r.random())
        b = dsp.saw(f * 2 ** (-p["detune"] / 1200) / sr, r.random())
        t = np.arange(N) / sr
        fc = p["cutoff"] * (1 + p["lfo_amt"] * np.sin(2 * np.pi * p["lfo"] * t))
        L = dsp.ladder(0.5 * (a + 0.6 * b), fc, p["res"], 1.8, float(sr))
        R = dsp.ladder(0.5 * (b + 0.6 * a), fc, p["res"], 1.8, float(sr))
        sub = p["sub"] * dsp.sine(f / sr, 0.0)
        env = self.env(gate, trig, sr) * vel * p["gain"]
        return np.stack([(L + sub) * env, (R + sub) * env], axis=1)


class SynthBass(MonoInstrument):
    """Funky analogue synth bass: saw + square, ladder 'pluck' envelope, accent."""

    name = "synth_bass"
    defaults = dict(glide=0.04, square=0.6, cutoff=380.0, env_amt=3200.0, env_decay=0.12,
                    res=0.35, drive=1.8, a=0.002, d=0.35, s=0.75, r=0.05, gain=0.5, sub=0.35)

    def phrase(self, pitch, gate, trig, vel, sr, idx, notes):
        p = self.p
        N = len(pitch)
        f = mtof(pitch)
        r = rng("sbass", idx)
        x = dsp.saw(f / sr, r.random()) + p["square"] * dsp.pulse(f / sr, np.full(N, 0.5), 0.3)
        fenv = adsr_sm(gate, trig, 0.001, p["env_decay"], 0.0, 0.08, float(sr), 0.0)
        fc = p["cutoff"] + p["env_amt"] * fenv * (0.35 + 0.65 * vel ** 2)
        y = dsp.ladder(x * 0.45, fc, p["res"], p["drive"], float(sr))
        y = y + p["sub"] * dsp.sine(f / sr, 0.0) * 0.6
        env = self.env(gate, trig, sr)
        return y * env * vel * p["gain"]


class PickBass(Instrument):
    """Electric bass via Karplus-Strong: art = '' (finger) | slap | pop | mute | ghost.

    The string starts from a plucked-string displacement (triangle with its apex
    at the plucking point), so the fundamental is strong like a real bass DI;
    a short sub layer can reinforce it. Good for J-pop busy lines (octave pops,
    ghost notes, slides).
    """

    name = "pick_bass"
    tail = 0.12
    defaults = dict(t60=2.4, bright=0.55, gain=0.5, r=0.07, slap_bright=0.8, tone=3200.0,
                    sub=0.2, pos=0.18, smile=1.0)

    def voice(self, n, dur_s, semis, sr, idx):
        p = self.p
        r = rng("kbass", n.t, n.p)
        hz = float(mtof(n.p))
        N = len(semis)
        n_on = int(dur_s * sr)
        art = n.art
        bright, t60, pos, tone = p["bright"], p["t60"], p["pos"], p["tone"]
        slap, pop = "slap" in art, "pop" in art
        if slap:
            bright, pos, tone = p["slap_bright"], 0.05, tone * 2.6
        if pop:
            bright, pos, tone = 0.9, 0.035, tone * 3.2
        if "mute" in art:
            t60, bright, tone = 0.25, 0.2, tone * 0.7
        ghost = "ghost" in art
        if ghost:
            t60, bright = 0.05, 0.15
        period = sr / hz
        Np = max(4, int(round(period)))
        x = np.arange(Np) / Np
        # plucked displacement: zero at both ends (no wrap discontinuity); the DC
        # it carries is removed after the string by a DC blocker
        tri = np.where(x < pos, x / pos, (1 - x) / (1 - pos))
        win = np.sin(np.pi * x) ** 2
        noise = dsp.lp(r.uniform(-1, 1, Np), 2000.0 + 6000.0 * bright, 0.7) * (0.1 + 0.25 * bright) * win
        exc = (tri + noise) * (0.6 + 0.4 * n.v)
        decay = 10 ** (-3.0 / (t60 * hz))
        y = dsp.karplus(N, period, decay, 1.0 - bright, exc, 0.0)
        if slap or pop:
            # thumb/finger snap against the frets: short metallic burst, not a tick
            nclk = min(N, int(0.025 * sr))
            env = np.exp(-np.arange(nclk) / ((0.0035 if pop else 0.0025) * sr))
            env *= np.clip(np.arange(nclk) / (0.0003 * sr), 0, 1)
            clk = r.standard_normal(nclk) * env
            clk = dsp.bp(clk, 2400.0 if slap else 3300.0, 1.6) + 0.4 * dsp.bp(clk, 5200.0, 2.0)
            y[:nclk] += clk * (0.45 if pop else 0.3)
        if ghost:
            nclk = int(0.03 * sr)
            y[:nclk] += dsp.bp(r.standard_normal(nclk), 900.0, 0.8) * np.exp(-np.arange(nclk) / (0.004 * sr)) * 0.4
        if p["sub"] > 0 and not ghost:
            t = np.arange(N) / sr
            sub = np.sin(2 * np.pi * np.cumsum(mtof(semis)) / sr) * np.exp(-6.9 * t / t60)
            y += sub * p["sub"] * np.clip(t / 0.004, 0, 1)
        rel = np.ones(N)
        if n_on < N:
            rel[n_on:] = np.exp(-6.9 * np.arange(N - n_on) / (p["r"] * sr))
        y = dsp.butter(y * rel, "low", tone, 2, sr)
        if (slap or pop) and p["smile"] > 0:  # the slap 'smile' EQ: scooped mids, crisp top
            y = y + p["smile"] * 0.9 * dsp.butter(y, "high", 1800.0, 2, sr)
        y = dsp.butter(y, "high", 22.0, 1, sr)  # DC blocker
        g = vel_gain(n.v, 14) * p["gain"] * (0.6 if ghost else 1.0)
        return y * g


# ===========================================================================
# DOOM guitar
# ===========================================================================
def cab_4x12(x, sr=SR, dark=0.0):
    """Closed-back 4x12 cabinet response (EQ approximation of an SM57-on-cone IR)."""
    y = dsp.butter(x, "high", 85.0, 2, sr)
    y = _peak(y, 115.0, 3.0, 1.1, sr)
    y = _peak(y, 420.0, -4.5, 1.0, sr)
    y = _peak(y, 1900.0, 2.5, 1.2, sr)
    y = _peak(y, 3600.0, 1.5, 2.0, sr)
    y = dsp.butter(y, "low", 5200.0 * (1 - 0.3 * dark), 4, sr)
    return y


def _peak(x, f0, gain_db, q, sr=SR):
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * f0 / sr
    alpha = np.sin(w0) / (2 * q)
    b = [1 + alpha * A, -2 * np.cos(w0), 1 - alpha * A]
    a = [1 + alpha / A, -2 * np.cos(w0), 1 - alpha / A]
    return signal.lfilter(np.array(b) / a[0], np.array(a) / a[0], x, axis=0)


def amp_sim(di, sr=SR, gain_db=32.0, tight=True, dark=0.0):
    """High-gain amp: TS-style tight boost -> 2 oversampled clip stages -> cab."""
    x = dsp.butter(di, "high", 160.0 if tight else 90.0, 1, sr)
    x = _peak(x, 720.0, 6.0, 0.8, sr)
    g1 = 10 ** (gain_db / 20)

    def stage(u):
        u = np.tanh(g1 * 0.3 * u + 0.08) - np.tanh(0.08)
        u = _peak_fast(u)
        return np.tanh(3.0 * u) * 0.9

    y = dsp.oversample(stage, x, 4)
    y = cab_4x12(y, sr, dark)
    return y


def _peak_fast(u):
    return u - 0.15 * np.roll(u, 1)  # tiny inter-stage HF tilt (pre-emphasis)


class DoomGuitar(Instrument):
    """Drop-tuned metal rhythm guitar (Hellvile!).

    Track notes are chord roots; params power=True adds the 5th + octave.
    Articulations: mute (palm mute), open (let ring), stac, slide. Two takes are
    rendered with different picking (double-tracking) and panned L/R.
    """

    name = "doom_guitar"
    tail = 0.4
    defaults = dict(power=True, gain_db=34.0, pan=0.82, t60=2.4, mute_t60=0.16, takes=2,
                    level=0.22, dark=0.0, tightness=1.0, pick=0.9)

    def take(self, track, score, sr, take_i):
        p = self.p
        total = score.n_samples(sr)
        di = np.zeros(total)
        for i, n in enumerate(track.sorted()):
            r = rng("gtr", take_i, n.t, n.p)
            start = score.b2n(n.t + r.normal(0, 0.004 / score.spb), sr)
            dur = n.d * score.spb
            pitches = [n.p, n.p + 7, n.p + 12] if p["power"] and "single" not in n.art else [n.p]
            muted = "mute" in n.art
            t60 = p["mute_t60"] if muted else p["t60"]
            N = int((dur + (0.05 if muted else 0.25)) * sr)
            y = np.zeros(N)
            for k, q in enumerate(pitches):
                hz = float(mtof(q)) * 2 ** (r.normal(0, 1.5) / 1200)
                period = sr / hz
                ne = int(min(period * 1.5, 0.012 * sr))
                exc = r.uniform(-1, 1, ne) * p["pick"]
                exc = dsp.lp(exc, 5000.0 if not muted else 1400.0, 0.7)
                s = dsp.karplus(N, period, 10 ** (-3.0 / (t60 * hz)), 0.15 if not muted else 0.6,
                                exc, 0.0)
                off = int((k * 0.0035 + abs(r.normal(0, 0.001))) * sr)  # downstroke strum
                y[off:] += s[:N - off] * (0.9 if k == 0 else 0.75)
            n_on = int(dur * sr)
            if n_on < N:
                y[n_on:] *= np.exp(-np.arange(N - n_on) / (0.012 * sr))
            place(di, y * vel_gain(n.v, 10), max(0, start))
        return di

    def render(self, track, score, sr=SR):
        p = self.p
        takes = []
        for k in range(int(p["takes"])):
            di = self.take(track, score, sr, k)
            wet = amp_sim(di, sr, p["gain_db"], dark=p["dark"])
            # noise gate keyed from the DI (tight palm mutes)
            env = dsp.env_follow(di, dsp.coef(0.5, sr), dsp.coef(40.0, sr))
            gate = np.clip(env / (0.004 + 1e-9), 0, 1) ** 0.5
            takes.append(wet * gate * p["level"])
        if len(takes) == 1:
            return np.stack([takes[0], takes[0]], axis=1)
        pan = p["pan"]
        th1 = (-pan + 1) * np.pi / 4
        th2 = (pan + 1) * np.pi / 4
        L = takes[0] * np.cos(th1) + takes[1] * np.cos(th2)
        R = takes[0] * np.sin(th1) + takes[1] * np.sin(th2)
        return np.stack([L, R], axis=1)


def dsp_place(buf, x, start):
    if start >= len(buf):
        return
    n = min(len(x), len(buf) - start)
    buf[start:start + n] += x[:n]
