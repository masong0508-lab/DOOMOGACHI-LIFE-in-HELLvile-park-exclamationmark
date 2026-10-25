"""Drum kits: layered synthesis + CC0 / free samples, GM note map.

    kit = DrumKit("pop")        # pop | rock | 808 | chip | acoustic
    kick.inst = kit             # any track whose notes use GM drum keys

GM keys: 36 kick, 37 side-stick, 38 snare, 39 clap, 40 snare (rim), 42 closed hat,
44 pedal hat, 46 open hat, 41/43/45/47/48/50 toms, 49/57 crash, 55 splash,
51 ride, 53 ride bell, 54 tambourine, 56 cowbell, 70 shaker, 75 claves.
Articulations: ghost, accent, flam, open, choke (hats are choked automatically).
"""
from __future__ import annotations

import numpy as np

from . import dsp
from .core import SR, db2a, place, rng, stereo
from .instrument import Instrument, vel_gain
from .samplers import oneshot

KICK, SIDE, SNARE, CLAP, RIM, CHH, PHH, OHH = 36, 37, 38, 39, 40, 42, 44, 46
TOMS = {41: 82.0, 43: 98.0, 45: 116.0, 47: 138.0, 48: 164.0, 50: 196.0}
CRASH, CRASH2, SPLASH, RIDE, BELL, TAMB, COWBELL, SHAKER, CLAVES = 49, 57, 55, 51, 53, 54, 56, 70, 75


# ---------------------------------------------------------------------------
# synthesis voices (all return mono)
# ---------------------------------------------------------------------------
def syn_kick(v, r, sr=SR, tune=52.0, punch=1.0, decay=0.38, click=1.0, sweep=4.2, drive=1.6):
    N = int((decay + 0.1) * sr)
    t = np.arange(N) / sr
    f = tune * (1 + sweep * np.exp(-t / 0.018) + 0.6 * np.exp(-t / 0.11))
    body = dsp.sine(f / sr, 0.0) * np.exp(-6.9 * t / decay)
    body = np.tanh(drive * body * (0.8 + 0.4 * v)) / np.tanh(drive)
    nk = int(0.006 * sr)
    clk = np.zeros(N)
    clk[:nk] = r.standard_normal(nk) * np.exp(-np.arange(nk) / (0.0009 * sr))
    clk = dsp.bp(clk, 3500.0, 0.8) * 0.7 + dsp.sine(np.full(N, 1250.0 / sr)) * np.exp(-t / 0.0025) * 0.5
    y = body * punch + clk * click * (0.5 + 0.5 * v)
    return y * np.clip(t / 0.0004, 0, 1)


def syn_snare(v, r, sr=SR, tune=190.0, snappy=1.0, decay=0.2, body=1.0):
    N = int((decay + 0.25) * sr)
    t = np.arange(N) / sr
    fb = tune * (1 + 0.5 * np.exp(-t / 0.01))
    b = (dsp.sine(fb / sr) * 0.8 + dsp.sine(fb * 1.75 / sr, 0.3) * 0.45) * np.exp(-6.9 * t / 0.09)
    nz = r.standard_normal(N)
    nz = dsp.hp(nz, 1500.0, 0.7)
    nz = dsp.lp(nz, 9000.0 + 3000 * v, 0.7) * np.exp(-6.9 * t / decay)
    nz2 = dsp.bp(r.standard_normal(N), 5200.0, 0.8) * np.exp(-6.9 * t / (decay * 0.6))
    y = b * body + (nz * 0.8 + nz2 * 0.5) * snappy
    return y * np.clip(t / 0.0003, 0, 1)


def syn_clap(v, r, sr=SR, decay=0.22, spread=0.0095, tone=1250.0):
    N = int((decay + 0.12) * sr)
    t = np.arange(N) / sr
    env = np.zeros(N)
    for k in range(4):
        t0 = k * spread * (1 + 0.15 * r.standard_normal())
        e = np.where(t >= t0, np.exp(-(t - t0) / 0.0055), 0.0)
        env = np.maximum(env, e * (0.85 if k < 3 else 1.0))
    t3 = 3 * spread
    env += np.where(t >= t3, 0.55 * np.exp(-6.9 * (t - t3) / decay), 0.0)
    nz = r.standard_normal(N)
    y = dsp.bp(nz, tone, 1.4) * 0.7 + dsp.bp(nz, tone * 2.3, 1.2) * 0.45
    return y * env * 2.2


METAL = np.array([205.3, 304.4, 369.6, 522.7, 540.0, 800.0]) * 1.65


def syn_hat(v, r, sr=SR, decay=0.045, open_=False, tone=1.0):
    d = 0.42 if open_ else decay
    N = int((d + 0.06) * sr)
    t = np.arange(N) / sr
    y = np.zeros(N)
    for f in METAL * tone:
        y += dsp.pulse(np.full(N, f / sr), np.full(N, 0.5), r.random())
    y = y * 0.35 + r.standard_normal(N) * 0.5
    y = dsp.bp(y, 10500.0, 0.9)
    y = dsp.hp(y, 7200.0, 0.7)
    env = np.exp(-6.9 * t / d) * np.clip(t / 0.0004, 0, 1)
    return y * env * 1.4


def syn_tom(v, r, sr=SR, f0=110.0, decay=0.5):
    N = int((decay + 0.1) * sr)
    t = np.arange(N) / sr
    f = f0 * (1 + 0.55 * np.exp(-t / 0.05))
    y = dsp.sine(f / sr) * np.exp(-6.9 * t / decay)
    y += dsp.sine(f * 1.6 / sr, 0.2) * 0.25 * np.exp(-6.9 * t / (decay * 0.5))
    nz = dsp.bp(r.standard_normal(N), 3000.0, 0.7) * np.exp(-t / 0.008) * 0.25
    return np.tanh(1.5 * (y + nz)) / np.tanh(1.5)


def syn_tamb(v, r, sr=SR, decay=0.16):
    N = int((decay + 0.05) * sr)
    t = np.arange(N) / sr
    env = np.zeros(N)
    for k in range(5):
        t0 = abs(r.normal(0, 0.006)) + k * 0.004
        env += np.where(t >= t0, np.exp(-(t - t0) / (decay / 3)), 0.0) * (1 - 0.12 * k)
    y = r.standard_normal(N)
    y = dsp.bp(y, 8200.0, 1.6) + 0.6 * dsp.bp(y, 11500.0, 2.0)
    return y * env * 0.7


def syn_shaker(v, r, sr=SR, length=0.09):
    N = int((length + 0.03) * sr)
    t = np.arange(N) / sr
    env = np.clip(t / (length * 0.35), 0, 1) ** 1.5 * np.exp(-np.maximum(t - length * 0.35, 0) / (length * 0.25))
    y = dsp.bp(r.standard_normal(N), 6500.0, 1.0)
    return dsp.hp(y, 4000.0, 0.7) * env * 1.2


def chip_kick(v, r, sr=SR):
    N = int(0.16 * sr)
    t = np.arange(N) / sr
    f = 60 + 500 * np.exp(-t / 0.012)
    vol = dsp.onepole_lp(np.round(15 * np.exp(-t / 0.05)) / 15, np.full(N, 900.0), float(sr))
    y = dsp.pulse(f / sr, np.full(N, 0.5), 0.0) * vol
    return dsp.butter(y, "low", 9000.0, 2, sr) * 0.9


def chip_noise(v, r, sr=SR, rate=9000.0, short=False, decay=0.12):
    N = int((decay + 0.02) * sr)
    t = np.arange(N) / sr
    y = dsp.lfsr_noise(N, rate, float(sr), short, int(r.integers(1, 0x7FFF)))
    vol = dsp.onepole_lp(np.round(15 * np.exp(-t / (decay / 3))) / 15, np.full(N, 900.0), float(sr))
    return dsp.butter(y * vol, "low", 11000.0, 2, sr) * 0.6


# ---------------------------------------------------------------------------
# kits: per piece a list of layers (kind, arg, gain_db)
#   ('syn', fn_name, dB, kwargs) | ('smp', path(s), dB, kwargs)
# ---------------------------------------------------------------------------
KITS = {
    "pop": {
        KICK: [("syn", "kick", 0.0, dict(tune=52.0, decay=0.36, punch=1.0, click=0.8)),
               ("smp", ["sonicpi/drum_heavy_kick"], -5.0, dict(hp=40.0, lp=9000.0))],
        SNARE: [("smp", ["sonicpi/drum_snare_hard"], -1.0, dict(hp=120.0)),
                ("smp", ["teropa/snare", "teropa/snare3"], -6.0, dict(hp=160.0)),
                ("syn", "snare", -7.0, dict(tune=200.0, snappy=1.0, decay=0.22))],
        RIM: [("smp", ["sonicpi/sn_dolf"], -2.0, {}), ("syn", "snare", -9.0, dict(decay=0.14))],
        CLAP: [("syn", "clap", -1.0, {}), ("smp", ["tr808/CP/CP"], -7.0, dict(hp=300.0))],
        SIDE: [("smp", ["tr808/RS/RS"], -4.0, {})],
        CHH: [("smp", ["sonicpi/drum_cymbal_closed"], -3.0, dict(hp=500.0)),
              ("syn", "hat", -12.0, dict(decay=0.04))],
        PHH: [("smp", ["sonicpi/drum_cymbal_pedal"], -4.0, dict(hp=500.0))],
        OHH: [("smp", ["sonicpi/drum_cymbal_open"], -4.0, dict(hp=500.0, fadeout=0.7)),
              ("syn", "hat", -14.0, dict(open_=True))],
        CRASH: [("smp", ["sonicpi/drum_splash_hard"], -3.0, dict(hp=300.0)),
                ("smp", ["teropa/crash"], -5.0, dict(hp=350.0))],
        CRASH2: [("smp", ["sonicpi/drum_cymbal_hard"], -4.0, dict(hp=400.0)),
                 ("smp", ["teropa/crash"], -6.0, dict(hp=350.0, rate=0.94)),
                 ("smp", ["tr808/CY/CY5050"], -14.0, dict(hp=500.0))],
        SPLASH: [("smp", ["sonicpi/drum_splash_soft"], -4.0, dict(hp=300.0))],
        RIDE: [("smp", ["teropa/ride"], -3.0, dict(hp=400.0)),
               ("smp", ["sonicpi/ride_via"], -12.0, dict(hp=600.0, fadeout=1.5))],
        BELL: [("smp", ["sonicpi/ride_tri"], -4.0, dict(hp=400.0, fadeout=1.5))],
        TAMB: [("syn", "tamb", -4.0, {})],
        SHAKER: [("syn", "shaker", -6.0, {})],
        COWBELL: [("smp", ["tr808/CB/CB"], -6.0, {})],
        CLAVES: [("smp", ["tr808/CL/CL"], -6.0, {})],
    },
}
KITS["pop"].update({k: [("smp", [f"sonicpi/drum_tom_{nm}_hard"], -2.0, dict(rate=rt)),
                        ("syn", "tom", -9.0, dict(f0=f0))]
                    for k, nm, rt, f0 in ((41, "lo", 0.85, 82.0), (43, "lo", 1.0, 98.0),
                                          (45, "mid", 0.9, 116.0), (47, "mid", 1.05, 138.0),
                                          (48, "hi", 0.95, 164.0), (50, "hi", 1.1, 196.0))})
KITS["rock"] = dict(KITS["pop"])
KITS["rock"].update({
    KICK: [("syn", "kick", -2.0, dict(tune=55.0, decay=0.3, punch=0.9, click=1.2)),
           ("smp", ["sonicpi/drum_heavy_kick"], -1.0, dict(hp=35.0))],
    SNARE: [("smp", ["sonicpi/drum_snare_hard"], 0.0, dict(hp=100.0)),
            ("syn", "snare", -9.0, dict(tune=180.0, snappy=0.8, decay=0.25))],
})
KITS["808"] = {
    KICK: [("smp", ["tr808/BD/BD5075"], 0.0, {}), ("syn", "kick", -10.0, dict(click=1.5, decay=0.1))],
    SNARE: [("smp", ["tr808/SD/SD5075"], -1.0, {})],
    CLAP: [("smp", ["tr808/CP/CP"], -1.0, {})],
    CHH: [("smp", ["tr808/CH/CH"], -2.0, {})],
    OHH: [("smp", ["tr808/OH/OH50"], -3.0, {})],
    PHH: [("smp", ["tr808/CH/CH"], -6.0, dict(lp=6000.0))],
    CRASH: [("smp", ["tr808/CY/CY5075"], -4.0, {})],
    COWBELL: [("smp", ["tr808/CB/CB"], -3.0, {})],
    CLAVES: [("smp", ["tr808/CL/CL"], -3.0, {})],
    SIDE: [("smp", ["tr808/RS/RS"], -3.0, {})],
    SHAKER: [("smp", ["tr808/MA/MA"], -3.0, {})],
    **{k: [("smp", [f"tr808/{d}/{d}50"], -2.0, dict(rate=rt))]
       for k, d, rt in ((41, "LT", 0.9), (43, "LT", 1.0), (45, "MT", 0.95), (47, "MT", 1.05),
                        (48, "HT", 0.95), (50, "HT", 1.05))},
}
KITS["acoustic"] = {k: [l for l in v if l[0] == "smp"] or v for k, v in KITS["rock"].items()}
KITS["chip"] = {
    KICK: [("syn", "chip_kick", 0.0, {})],
    SNARE: [("syn", "chip_noise", -2.0, dict(rate=12000.0, decay=0.16))],
    CLAP: [("syn", "chip_noise", -3.0, dict(rate=7000.0, decay=0.2))],
    CHH: [("syn", "chip_noise", -8.0, dict(rate=40000.0, short=True, decay=0.04))],
    PHH: [("syn", "chip_noise", -10.0, dict(rate=40000.0, short=True, decay=0.03))],
    OHH: [("syn", "chip_noise", -9.0, dict(rate=30000.0, short=False, decay=0.3))],
    CRASH: [("syn", "chip_noise", -6.0, dict(rate=18000.0, decay=0.9))],
    **{k: [("syn", "tom", -3.0, dict(f0=f0))] for k, f0 in TOMS.items()},
}

SYN = {"kick": syn_kick, "snare": syn_snare, "clap": syn_clap, "hat": syn_hat, "tom": syn_tom,
       "tamb": syn_tamb, "shaker": syn_shaker, "chip_kick": chip_kick, "chip_noise": chip_noise}
HAT_GROUP = {CHH, PHH, OHH}


class DrumKit(Instrument):
    """Layered drum kit. kit = pop | rock | 808 | chip | acoustic.

    params: tune (semitones for all pitched pieces), kick_tune (Hz for the synth
    kick sub, e.g. 51.9 = Ab1), overrides = {gm_key: layers}, vel_range (dB),
    room (0..1 short early-reflection 'room mic' blend per hit).
    """

    name = "drumkit"
    tail = 0.3

    def __init__(self, kit="pop", **params):
        p = dict(vel_range=16.0, kick_tune=None, overrides=None, gain=0.5, bright=1.0, pans=None)
        p.update(params)
        super().__init__(**p)
        self.kit_name = kit
        self.kit = dict(KITS[kit])
        if self.p["overrides"]:
            self.kit.update(self.p["overrides"])

    def hit(self, key, v, r, sr=SR, art=""):
        layers = self.kit.get(key)
        if layers is None:
            layers = self.kit.get(SNARE if key in (37, 40) else CHH)
        out = None
        for (kind, arg, gdb, kw) in layers:
            kw = dict(kw)
            hp = kw.pop("hp", None)
            lp = kw.pop("lp", None)
            rate = kw.pop("rate", 1.0)
            fadeout = kw.pop("fadeout", None)
            if kind == "syn":
                if arg == "kick" and self.p["kick_tune"]:
                    kw["tune"] = self.p["kick_tune"]
                y = stereo(SYN[arg](v, r, sr, **kw))
            else:
                path = arg[int(r.integers(len(arg)))]
                y = oneshot(path, sr)
                # round robin: tiny pitch + level variation
                rr = rate * 2 ** (r.normal(0, 6) / 1200)
                if abs(rr - 1) > 1e-4:
                    idx = np.arange(0, len(y) - 1, rr)
                    y = np.stack([np.interp(idx, np.arange(len(y)), y[:, c]) for c in range(2)], 1)
                y = y * db2a(r.normal(0, 0.4))
                if fadeout:
                    n = len(y)
                    k = int(fadeout * sr)
                    if n > k:
                        y = y[:n].copy()
                        y[k:] *= np.exp(-np.arange(n - k) / (0.15 * sr))[:, None]
            if hp:
                y = dsp.butter(y, "high", hp, 2, sr)
            if lp:
                y = dsp.butter(y, "low", lp, 2, sr)
            # velocity -> brightness on soft hits
            if v < 0.7:
                y = dsp.butter(y, "low", 3500.0 + 14000.0 * v, 1, sr)
            y = y * db2a(gdb)
            if out is None:
                out = y
            else:
                if len(y) > len(out):
                    out, y = y, out
                out = out.copy()
                out[:len(y)] += y
        if out is None:
            return None
        return out * vel_gain(v, self.p["vel_range"]) * self.p["gain"]

    def render(self, track, score, sr=SR):
        total = score.n_samples(sr)
        buf = np.zeros((total, 2))
        notes = track.sorted()
        hat_times = [score.b2n(n.t, sr) for n in notes if n.p in HAT_GROUP]
        for i, n in enumerate(notes):
            r = rng("drum", self.kit_name, n.t, n.p, i)
            v = n.v * (0.45 if n.has("ghost") else 1.0) * (1.08 if n.has("accent") else 1.0)
            v = min(1.0, v)
            y = self.hit(n.p, v, r, sr, n.art)
            if y is None:
                continue
            start = score.b2n(n.t, sr)
            if n.p == OHH or n.has("choke"):
                nxt = [h for h in hat_times if h > start + 10]
                if nxt and nxt[0] - start < len(y):
                    k = nxt[0] - start
                    y = y.copy()
                    fl = min(int(0.012 * sr), len(y) - k)
                    y[k:k + fl] *= np.linspace(1, 0, fl)[:, None]
                    y[k + fl:] = 0
            pans = self.p["pans"] or {}
            if n.p in pans:
                th = (pans[n.p] + 1) * np.pi / 4
                y = y * np.array([np.cos(th), np.sin(th)]) * np.sqrt(2)
            if n.has("flam"):
                g = self.hit(n.p, v * 0.5, r, sr)
                place(buf, g, start - int(0.022 * sr))
            place(buf, y, start)
        return buf
