"""Mixer: channel strips, sends/returns, group buses, sidechain, automation.

    mx = Mixer(score)
    mx.channel("kick", stem, bus="drums", eq=[("hp", 30), ("peak", 60, 2, 1.0)],
               comp=dict(thresh=-14, ratio=4, attack=8, release=80), sat=3.0)
    mx.channel("bass", stem, bus="music", sidechain=dict(source="kick", depth=6),
               mono_below=150, sends={"plate": -30})
    mx.channel("lead", stem, bus="lead", sends={"plate": -14, "delay": -18},
               automation={"gain_db": [(0, -6), (16, 0)], "lpf": [(0, 800), (16, 18000)]})
    mx.reverb("plate", kind="plate", rt60=1.6, hp=280, lp=9000)
    mx.delay("delay", beats=0.75, feedback=0.32, hp=350, lp=6000)
    mx.bus("drums", comp=dict(...), parallel=dict(thresh=-28, ratio=10, mix_db=-7))
    pre_master = mx.render()          # (n, 2) ;  mx.stems: processed per-channel audio

EQ specs: ("hp", f[, order]) ("lp", f[, order]) ("peak", f, gain_db, q)
          ("lowshelf", f, gain_db) ("highshelf", f, gain_db) ("notch", f, q)
Insert chain: any pedalboard plugin or callable(audio, sr) -> audio.
"""
from __future__ import annotations

import dataclasses as dc
import math

import numpy as np
from numba import njit
from scipy import signal

from . import dsp
from .core import SR, db2a, mono, stereo
from .reverb import convolve, make_ir


# ---------------------------------------------------------------------------
# EQ (RBJ cookbook biquads)
# ---------------------------------------------------------------------------
def _biquad(kind, f0, gain_db=0.0, q=0.707, sr=SR):
    A = 10 ** (gain_db / 40)
    w0 = 2 * math.pi * f0 / sr
    c, s = math.cos(w0), math.sin(w0)
    alpha = s / (2 * q)
    if kind == "peak":
        b = [1 + alpha * A, -2 * c, 1 - alpha * A]
        a = [1 + alpha / A, -2 * c, 1 - alpha / A]
    elif kind == "lowshelf":
        sq = 2 * math.sqrt(A) * alpha
        b = [A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c),
             A * ((A + 1) - (A - 1) * c - sq)]
        a = [(A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq]
    elif kind == "highshelf":
        sq = 2 * math.sqrt(A) * alpha
        b = [A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c),
             A * ((A + 1) + (A - 1) * c - sq)]
        a = [(A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq]
    elif kind == "notch":
        b = [1, -2 * c, 1]
        a = [1 + alpha, -2 * c, 1 - alpha]
    else:
        raise ValueError(kind)
    b = np.array(b) / a[0]
    a = np.array(a) / a[0]
    return np.concatenate([b, a])[None, :]


def eq(x, specs, sr=SR):
    """Apply a list of EQ specs (see module doc)."""
    y = np.asarray(x, dtype=float)
    for sp in specs or []:
        kind = sp[0]
        if kind in ("hp", "lp"):
            f = sp[1]
            order = sp[2] if len(sp) > 2 else 2
            sos = signal.butter(order, f, "high" if kind == "hp" else "low", fs=sr, output="sos")
        elif kind == "peak":
            sos = _biquad("peak", sp[1], sp[2], sp[3] if len(sp) > 3 else 1.0, sr)
        elif kind in ("lowshelf", "highshelf"):
            sos = _biquad(kind, sp[1], sp[2], sp[3] if len(sp) > 3 else 0.707, sr)
        elif kind == "notch":
            sos = _biquad("notch", sp[1], 0, sp[2] if len(sp) > 2 else 4.0, sr)
        else:
            raise ValueError(f"bad eq spec {sp}")
        y = signal.sosfilt(sos, y, axis=0)
    return y


# ---------------------------------------------------------------------------
# dynamics
# ---------------------------------------------------------------------------
@njit(cache=True, fastmath=True)
def _gain_computer(det_db, thresh, ratio, knee, att_c, rel_c):
    n = det_db.shape[0]
    gr = np.empty(n)
    g = 0.0
    for i in range(n):
        x = det_db[i]
        over = x - thresh
        if knee > 0 and 2 * abs(over) <= knee:
            tgt = (1.0 / ratio - 1.0) * (over + knee / 2) ** 2 / (2 * knee)
        elif over > 0:
            tgt = (1.0 / ratio - 1.0) * over
        else:
            tgt = 0.0
        # tgt <= 0 dB ; smooth: attack when going down
        if tgt < g:
            g = att_c * g + (1 - att_c) * tgt
        else:
            g = rel_c * g + (1 - rel_c) * tgt
        gr[i] = g
    return gr


def compress(x, thresh=-18.0, ratio=3.0, attack=10.0, release=120.0, knee=6.0, makeup=0.0,
             detector="rms", sidechain=None, sc_hp=None, sr=SR, return_gr=False, mix=1.0,
             auto_makeup=False):
    """Stereo-linked feed-forward compressor (soft knee; peak or RMS detection).

    sidechain: optional key signal; sc_hp: high-pass the detector (bus glue: 90 Hz).
    """
    x = stereo(x)
    key = stereo(sidechain) if sidechain is not None else x
    k = np.max(np.abs(key), axis=1) if detector == "peak" else np.sqrt(np.mean(key ** 2, axis=1))
    if sc_hp:
        k = np.abs(dsp.butter(mono(key), "high", sc_hp, 2, sr))
    if detector == "rms":
        w = max(1, int(0.004 * sr))
        k = np.sqrt(np.convolve(k ** 2, np.ones(w) / w, mode="same"))
    det_db = 20 * np.log10(np.maximum(k, 1e-9))
    gr = _gain_computer(det_db, thresh, ratio, knee, dsp.coef(attack, sr), dsp.coef(release, sr))
    mk = makeup
    if auto_makeup:
        mk = -0.5 * (1 - 1 / ratio) * thresh * 0.25
    y = x * db2a(gr + mk)[:, None]
    y = mix * y + (1 - mix) * x
    return (y, gr) if return_gr else y


def duck_curve(times_s, n, depth_db=6.0, attack_ms=4.0, hold_ms=10.0, release_ms=140.0,
               sr=SR, shape=2.0):
    """Kickstart-style sidechain gain curve from trigger times (seconds)."""
    g = np.ones(n)
    a = max(1, int(attack_ms * 1e-3 * sr))
    h = int(hold_ms * 1e-3 * sr)
    rl = max(1, int(release_ms * 1e-3 * sr))
    floor = db2a(-depth_db)
    shape_curve = np.concatenate([
        1 - (1 - floor) * np.linspace(0, 1, a) ** 0.5,
        np.full(h, floor),
        floor + (1 - floor) * (np.linspace(0, 1, rl) ** (1 / shape)) ** 1.0,
    ])
    L = len(shape_curve)
    for t in times_s:
        s = int(t * sr) - a // 2
        if s >= n:
            continue
        lo = max(0, s)
        seg = shape_curve[lo - s:min(L, n - s)]
        g[lo:lo + len(seg)] = np.minimum(g[lo:lo + len(seg)], seg)
    return g


# ---------------------------------------------------------------------------
# stereo
# ---------------------------------------------------------------------------
def width(x, w=1.0):
    x = stereo(x)
    M = 0.5 * (x[:, 0] + x[:, 1])
    S = 0.5 * (x[:, 0] - x[:, 1]) * w
    return np.stack([M + S, M - S], axis=1)


def mono_below(x, f=150.0, sr=SR):
    """Phase-coherent mono low end: zero-phase split, sum lows to mono."""
    x = stereo(x)
    sos = signal.butter(4, f, "low", fs=sr, output="sos")
    low = signal.sosfiltfilt(sos, x, axis=0)
    high = x - low
    lm = low.mean(axis=1, keepdims=True)
    return high + lm


def pan(x, p=0.0):
    """Stereo balance pan with constant power (-1..1)."""
    x = stereo(x)
    th = (p + 1) * math.pi / 4
    gl, gr = math.cos(th) * math.sqrt(2), math.sin(th) * math.sqrt(2)
    if p == 0:
        return x
    out = np.stack([x[:, 0] * gl, x[:, 1] * gr], axis=1)
    return out


def autopan(x, spb, rate_beats=1.0, depth=0.5, sr=SR, phase=0.0):
    x = stereo(x)
    t = np.arange(len(x)) / sr
    lfo = np.sin(2 * np.pi * t / (rate_beats * spb) + phase) * depth
    th = (lfo + 1) * math.pi / 4
    return np.stack([x[:, 0] * np.cos(th) * math.sqrt(2), x[:, 1] * np.sin(th) * math.sqrt(2)], 1)


def haas(x, ms=12.0, sr=SR):
    x = stereo(x)
    d = int(ms * 1e-3 * sr)
    R = np.concatenate([np.zeros(d), x[:-d, 1]]) if d > 0 else x[:, 1]
    return np.stack([x[:, 0], R], axis=1)


# ---------------------------------------------------------------------------
# automation
# ---------------------------------------------------------------------------
def automation_curve(points, n, spb, sr=SR, log=False, default=0.0):
    """[(beat, value), ...] -> per-sample curve (linear, or in log space)."""
    if not points:
        return np.full(n, default)
    bt = np.array([p[0] for p in points]) * spb * sr
    v = np.array([p[1] for p in points], dtype=float)
    idx = np.arange(n)
    if log:
        return np.exp(np.interp(idx, bt, np.log(v)))
    return np.interp(idx, bt, v)


# ---------------------------------------------------------------------------
# mixer
# ---------------------------------------------------------------------------
@dc.dataclass
class Channel:
    name: str
    audio: np.ndarray
    bus: str = "music"
    gain_db: float = 0.0
    eq: list = None
    comp: dict = None
    sat: float = 0.0              # tape saturation drive (dB), 0 = off
    chain: list = None            # pedalboard plugins / callables
    post_eq: list = None
    pan: float = 0.0
    width: float = 1.0
    mono_below: float = None
    sends: dict = None            # {return_name: dB}
    sidechain: dict = None        # {source, depth, attack, hold, release} or {key: array}
    automation: dict = None       # gain_db / lpf / hpf / pan / send:<name>
    autopan: dict = None          # {rate_beats, depth}
    mute: bool = False


class Mixer:
    def __init__(self, score, sr=SR):
        self.score = score
        self.sr = sr
        self.channels: dict[str, Channel] = {}
        self.returns = {}
        self.buses = {}
        self.stems = {}
        self.bus_out = {}
        self.trigger_times = {}

    # ---- setup -----------------------------------------------------------
    def channel(self, name, audio, **kw):
        """Add a channel strip. audio=None (e.g. an empty track) is skipped."""
        if audio is None:
            return None
        ch = Channel(name, stereo(audio), **kw)
        self.channels[name] = ch
        return ch

    def triggers(self, name, beats):
        """Register sidechain trigger times (beats) under a key name (e.g. 'kick')."""
        self.trigger_times[name] = [b * self.score.spb for b in beats]

    def reverb(self, name, kind="plate", rt60=None, predelay_ms=None, hp=250.0, lp=10000.0,
               gain_db=0.0, width=1.0, duck=None, seed=0):
        self.returns[name] = dict(type="reverb", kind=kind, rt60=rt60, predelay_ms=predelay_ms,
                                  hp=hp, lp=lp, gain_db=gain_db, width=width, duck=duck, seed=seed)

    def delay(self, name, beats=0.75, beats_r=None, feedback=0.3, hp=300.0, lp=6000.0,
              gain_db=0.0, cross=1.0, duck=None, verb=None):
        self.returns[name] = dict(type="delay", beats=beats, beats_r=beats_r or beats,
                                  feedback=feedback, hp=hp, lp=lp, gain_db=gain_db, cross=cross,
                                  duck=duck, verb=verb)

    def bus(self, name, eq=None, comp=None, parallel=None, sat=0.0, width=1.0, gain_db=0.0,
            chain=None, sidechain=None, automation=None, mono_below=None):
        self.buses[name] = dict(eq=eq, comp=comp, parallel=parallel, sat=sat, width=width,
                                gain_db=gain_db, chain=chain, sidechain=sidechain,
                                automation=automation, mono_below=mono_below)

    # ---- processing ------------------------------------------------------
    def _run_chain(self, x, chain):
        for plug in chain or []:
            if callable(plug) and not hasattr(plug, "process"):
                x = plug(x, self.sr)
            else:
                y = plug(np.ascontiguousarray(x.T.astype(np.float32)), self.sr)
                x = np.asarray(y, dtype=float).T
        return x

    def _sidechain_gain(self, sc, n):
        if sc is None:
            return None
        if "key" in sc:
            env = dsp.env_follow(mono(sc["key"])[:n], dsp.coef(1.0, self.sr), dsp.coef(sc.get("release", 120), self.sr))
            env = env / (env.max() + 1e-9)
            return db2a(-sc.get("depth", 6.0) * env)
        times = self.trigger_times.get(sc.get("source", "kick"), [])
        return duck_curve(times, n, sc.get("depth", 6.0), sc.get("attack", 4.0),
                          sc.get("hold", 10.0), sc.get("release", 140.0), self.sr,
                          sc.get("shape", 2.0))

    def process_channel(self, ch: Channel):
        sr, spb = self.sr, self.score.spb
        x = ch.audio * db2a(ch.gain_db)
        n = len(x)
        if ch.eq:
            x = eq(x, ch.eq, sr)
        if ch.comp:
            x = compress(x, sr=sr, **ch.comp)
        if ch.sat:
            x = dsp.tape_sat(x, ch.sat, 0.03)
        if ch.chain:
            x = self._run_chain(x, ch.chain)
        if ch.post_eq:
            x = eq(x, ch.post_eq, sr)
        x = self._automate(x, ch.automation)
        if ch.width != 1.0:
            x = width(x, ch.width)
        if ch.autopan:
            x = autopan(x, spb, ch.autopan.get("rate_beats", 1.0), ch.autopan.get("depth", 0.5), sr)
        if ch.pan:
            x = pan(x, ch.pan)
        if ch.mono_below:
            x = mono_below(x, ch.mono_below, sr)
        g = self._sidechain_gain(ch.sidechain, n)
        if g is not None:
            x = x * g[:, None]
        return x

    def _automate(self, x, au):
        """gain_db / lpf / hpf automation lanes: [(beat, value), ...]."""
        if not au:
            return x
        sr, spb, n = self.sr, self.score.spb, len(x)
        if "hpf" in au:
            fc = automation_curve(au["hpf"], n, spb, sr, log=True)
            x = np.stack([dsp.svf(np.ascontiguousarray(x[:, c]), fc, 0.707, 2, float(sr)) for c in range(2)], 1)
        if "lpf" in au:
            fc = automation_curve(au["lpf"], n, spb, sr, log=True)
            x = np.stack([dsp.svf(np.ascontiguousarray(x[:, c]), fc, 0.8, 0, float(sr)) for c in range(2)], 1)
        if "gain_db" in au:
            x = x * db2a(automation_curve(au["gain_db"], n, spb, sr))[:, None]
        return x

    def render(self):
        sr, spb = self.sr, self.score.spb
        n = max(len(c.audio) for c in self.channels.values())
        bus_sum = {}
        send_sum = {k: np.zeros((n, 2)) for k in self.returns}
        for name, ch in self.channels.items():
            if ch.mute:
                continue
            x = self.process_channel(ch)
            x = np.pad(x, ((0, n - len(x)), (0, 0)))
            self.stems[name] = x
            bus_sum.setdefault(ch.bus, np.zeros((n, 2)))
            bus_sum[ch.bus] += x
            for rname, db in (ch.sends or {}).items():
                if rname not in send_sum:
                    continue
                gs = db2a(db)
                au = (ch.automation or {}).get("send:" + rname)
                if au:
                    gs = db2a(automation_curve(au, n, spb, sr))[:, None]
                send_sum[rname] += x * gs
        # returns
        for rname, rp in self.returns.items():
            s = send_sum[rname]
            if not np.any(s):
                continue
            if rp["type"] == "reverb":
                ir = make_ir(rp["kind"], rp["rt60"], rp["predelay_ms"], sr, rp["seed"],
                             width=rp["width"])
                pre = eq(s, [("hp", rp["hp"]), ("lp", rp["lp"])], sr)
                wet = convolve(pre, ir, sr)[:n]
            else:
                dl = int(rp["beats"] * spb * sr)
                dr = int(rp["beats_r"] * spb * sr)
                pre = eq(s, [("hp", rp["hp"]), ("lp", rp["lp"])], sr)
                lp_a = 1 - math.exp(-2 * math.pi * rp["lp"] / sr)
                hp_a = 1 - math.exp(-2 * math.pi * rp["hp"] / sr)
                wl, wr = dsp.pingpong(np.ascontiguousarray(pre[:, 0]), np.ascontiguousarray(pre[:, 1]),
                                      dl, dr, rp["feedback"], lp_a, hp_a, rp["cross"])
                wet = np.stack([wl, wr], axis=1)
                if rp.get("verb"):
                    wet = wet + convolve(wet, make_ir("plate", 1.4, sr=sr), sr)[:n] * db2a(rp["verb"])
            g = self._sidechain_gain(rp.get("duck"), n)
            if g is not None:
                wet = wet * g[:, None]
            wet = wet * db2a(rp["gain_db"])
            self.stems["ret:" + rname] = wet
            bus_sum.setdefault("returns", np.zeros((n, 2)))
            bus_sum["returns"] += wet
        # buses
        out = np.zeros((n, 2))
        for bname, x in bus_sum.items():
            bp = self.buses.get(bname)
            if bp:
                if bp["eq"]:
                    x = eq(x, bp["eq"], sr)
                if bp["comp"]:
                    x = compress(x, sr=sr, **bp["comp"])
                if bp["parallel"]:
                    pp = dict(bp["parallel"])
                    mix_db = pp.pop("mix_db", -8.0)
                    pp.setdefault("attack", 1.0)
                    pp.setdefault("release", 60.0)
                    pp.setdefault("detector", "peak")
                    sq = compress(x, sr=sr, **pp)
                    sq = dsp.tape_sat(sq, 4.0, 0.0)
                    x = x + sq * db2a(mix_db)
                if bp["sat"]:
                    x = dsp.tape_sat(x, bp["sat"], 0.02)
                if bp["chain"]:
                    x = self._run_chain(x, bp["chain"])
                if bp["width"] != 1.0:
                    x = width(x, bp["width"])
                if bp.get("mono_below"):
                    x = mono_below(x, bp["mono_below"], sr)
                x = self._automate(x, bp.get("automation"))
                g = self._sidechain_gain(bp.get("sidechain"), n)
                if g is not None:
                    x = x * g[:, None]
                x = x * db2a(bp["gain_db"])
            self.bus_out[bname] = x
            out += x
        return out
