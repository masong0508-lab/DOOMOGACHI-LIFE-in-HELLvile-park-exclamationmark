"""Sample-based instruments: multisample sampler, Salamander piano, VSCO-2 CE
orchestra, SoundFont (sf2/sf3 via tinysoundfont) and one-shot loading.
"""
from __future__ import annotations

import functools
import json
import math
import os
import re

import numpy as np
from numba import njit

from .core import ASSETS, CACHE, SR, load, mtof, note as note_num, stereo, trim_silence
from .instrument import Instrument, vel_gain


@njit(cache=True, fastmath=True)
def _play(src, ratio, n_out, loop_s, loop_e, start_pos):
    """Cubic-interpolated variable-rate playback with optional loop [loop_s, loop_e)."""
    n = src.shape[0]
    out = np.zeros(n_out)
    pos = start_pos
    L = loop_e - loop_s
    for i in range(n_out):
        if loop_e > 0 and pos >= loop_e:
            pos -= L
        ip = int(pos)
        if ip + 2 >= n:
            break
        fr = pos - ip
        xm1 = src[ip - 1] if ip >= 1 else src[0]
        x0 = src[ip]
        x1 = src[ip + 1]
        x2 = src[ip + 2]
        c1 = 0.5 * (x1 - xm1)
        c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2
        c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1)
        out[i] = ((c3 * fr + c2) * fr + c1) * fr + x0
        pos += ratio[i]
    return out


def _xfade_loop(x, ls, le, xf):
    """Make a seamless loop: blend the region before le with the region before ls."""
    xf = min(xf, ls, le - ls)
    if xf < 16:
        return x
    y = x.copy()
    w = np.sin(np.linspace(0, np.pi / 2, xf)) ** 2
    if y.ndim == 2:
        w = w[:, None]
    y[le - xf:le] = x[le - xf:le] * (1 - w) + x[ls - xf:ls] * w
    return y


_TUNE_CACHE = None


def _tune_cache():
    global _TUNE_CACHE
    if _TUNE_CACHE is None:
        p = os.path.join(CACHE, "sample_tuning.json")
        try:
            with open(p) as fh:
                _TUNE_CACHE = json.load(fh)
        except Exception:
            _TUNE_CACHE = {}
    return _TUNE_CACHE


def _save_tune_cache():
    os.makedirs(CACHE, exist_ok=True)
    with open(os.path.join(CACHE, "sample_tuning.json"), "w") as fh:
        json.dump(_TUNE_CACHE, fh, indent=0, sort_keys=True)


def measure_cents(x, sr, root, t0=0.12, t1=1.6, frame=0.12):
    """Median pitch deviation (cents vs 12-TET A=440) of a sample's sustain."""
    from .qa import _f0_peak
    m = x.mean(axis=1) if x.ndim == 2 else x
    a, b = int(t0 * sr), min(len(m), int(t1 * sr))
    f_exp = float(mtof(root))
    cs, ws = [], []
    frame = max(frame, 20.0 / f_exp)
    hop = int(frame * sr / 2)
    nf = int(frame * sr)
    for i in range(a, max(a + 1, b - nf), hop):
        seg = m[i:i + nf]
        if len(seg) < nf:
            break
        e = float(np.sqrt(np.mean(seg ** 2)))
        if e < 1e-4:
            continue
        f = _f0_peak(seg, sr, f_exp, 0.9)
        if f:
            cs.append(1200 * np.log2(f / f_exp))
            ws.append(e)
    if not cs:
        return None
    o = np.argsort(cs)
    cw = np.cumsum(np.array(ws)[o])
    return float(np.array(cs)[o][np.searchsorted(cw, cw[-1] / 2)])  # weighted median


class Zone:
    def __init__(self, path, root, vel_hi=1.0, tune=0.0, loop=None, offset=0.0, gain=1.0,
                 lo=0, hi=127, trim=True, autotune=False):
        """tune = the sample's own deviation in cents (it is subtracted on playback).
        autotune=True measures it from the audio (cached in .cache/sample_tuning.json)."""
        self.path, self.root, self.vel_hi, self.tune = path, root, vel_hi, tune
        self.loop, self.offset, self.gain, self.lo, self.hi, self.trim = loop, offset, gain, lo, hi, trim
        self.autotune = autotune
        self._data = None

    def data(self, sr):
        if self._data is None:
            x = stereo(load(self.path, sr))
            a = int(self.offset * sr)
            x = x[a:]
            if self.trim and not self.loop:
                x = trim_silence(x, -50.0, int(0.0008 * sr))
            fi = min(len(x), int(0.0006 * sr))  # 0.6 ms fade-in: never start on a step
            x = x.copy()
            x[:fi] *= (np.sin(np.linspace(0, np.pi / 2, fi)) ** 2)[:, None]
            if self.loop:
                ls, le = int((self.loop[0] - self.offset) * sr), int((self.loop[1] - self.offset) * sr)
                le = min(le, len(x) - 4)
                x = _xfade_loop(x, ls, le, int(0.12 * sr))
                self._loop = (ls, le)
            else:
                self._loop = (0, 0)
            self._data = x
            if self.autotune:
                key = f"{os.path.relpath(self.path, ASSETS)}@{self.root}@{sr}"
                tc = _tune_cache()
                if key not in tc:
                    c = measure_cents(x, sr, self.root, t0=0.12 if self.loop else 0.1,
                                      t1=(self.loop[1] - self.offset) if self.loop else 1.6,
                                      frame=0.12 if self.loop else 0.1)
                    tc[key] = c if c is not None else self.tune
                    _save_tune_cache()
                if tc[key] is not None and abs(tc[key]) < 80:
                    self.tune = tc[key]
        return self._data


class Sampler(Instrument):
    """Generic multisample player (velocity layers, pitch bends, loops, release)."""

    name = "sampler"
    tail = 0.4
    defaults = dict(r=0.35, gain=1.0, vel_range=18.0, max_len=12.0, pan=0.0, spread=0.0)

    def __init__(self, zones=None, **params):
        super().__init__(**params)
        self.zones = zones or []

    def pick(self, p, v):
        cands = [z for z in self.zones if z.lo <= p <= z.hi] or self.zones
        best_d = min(abs(z.root - p) for z in cands)
        near = [z for z in cands if abs(z.root - p) == best_d]
        layer = sorted(near, key=lambda z: z.vel_hi)
        for z in layer:
            if v <= z.vel_hi + 1e-9:
                return z
        return layer[-1]

    def voice(self, n, dur_s, semis, sr, idx):
        z = self.pick(n.p, n.v)
        x = z.data(sr)
        ls, le = z._loop
        damp = self.p.get("damp", True)
        n_on = int(dur_s * sr)
        rel_n = int(self.p["r"] * sr * 1.2)
        ratio = 2.0 ** ((semis - z.root - z.tune / 100.0) / 12.0)
        if le > 0:
            n_out = n_on + rel_n
        else:
            n_out = min(int((len(x) - 4) / max(float(ratio.mean()), 0.05)),
                        int(self.p["max_len"] * sr))
            if damp:
                n_out = min(n_out, n_on + rel_n)
        if len(ratio) < n_out:
            ratio = np.concatenate([ratio, np.full(n_out - len(ratio), ratio[-1])])
        r = np.ascontiguousarray(ratio[:n_out])
        out = np.stack([_play(np.ascontiguousarray(x[:, c]), r, n_out, ls, le, 0.0)
                        for c in range(2)], axis=1)
        if n_on < n_out and (damp or le > 0):
            k = np.arange(n_out - n_on)
            out[n_on:] *= np.exp(-6.9 * k / (self.p["r"] * sr))[:, None]
        g = vel_gain(n.v, self.p["vel_range"]) * z.gain * self.p["gain"]
        pan, spread = self.p.get("pan", 0.0), self.p.get("spread", 0.0)
        if pan or spread:  # constant-power placement (+ pitch spread across the stage)
            pp = float(np.clip(pan + spread * (n.p - 64) / 24.0, -1, 1))
            th = (pp + 1) * math.pi / 4
            m = out.mean(axis=1)
            out = np.stack([m * math.cos(th), m * math.sin(th)], axis=1) * math.sqrt(2)
        return out * g

    def tail_for(self, n):
        return self.p["r"] * 1.2 + 0.05


_NAME_RE = re.compile(r"^([A-G](?:#|s|b)?)(-?\d)")


def _parse_name(s):
    m = _NAME_RE.match(s)
    if not m:
        return None
    nm = m.group(1).replace("s", "#")
    return note_num(nm + m.group(2))


class SalamanderPiano(Sampler):
    """Salamander Grand Piano V3 (Alexander Holm, CC BY 3.0): 6 velocity layers."""

    name = "piano"
    tail = 1.5
    LAYERS = (3, 6, 8, 11, 14, 16)

    def __init__(self, **params):
        p = dict(r=0.45, gain=0.9, vel_range=6.0, max_len=10.0, damp=True)
        p.update(params)
        zones = []
        d = os.path.join(ASSETS, "salamander")
        for f in sorted(os.listdir(d)):
            m = re.match(r"^([A-G]#?)(\d)v(\d+)\.mp3$", f)
            if not m:
                continue
            root = note_num(m.group(1) + m.group(2))
            lay = int(m.group(3))
            vi = self.LAYERS.index(lay) if lay in self.LAYERS else 0
            vel_hi = [0.3, 0.45, 0.6, 0.75, 0.9, 1.0][vi]
            # louder layers are louder: compensate so velocity -> level is smooth
            zones.append(Zone(os.path.join(d, f), root, vel_hi, gain=1.0))
        super().__init__(zones, **p)


@functools.lru_cache(maxsize=1)
def _vsco_manifest():
    with open(os.path.join(ASSETS, "vsco2", "manifest.json")) as fh:
        return json.load(fh)["instruments"]


class VSCO(Sampler):
    """VSCO-2 Community Edition (CC0) via tiny-orchestra: violins, violas, celli,
    basses, violinsPizz, celliPizz, harp, flute, oboe, clarinet, bassoon, horn,
    trumpet, trombone, tuba, glockenspiel, marimba, timpani, ..."""

    name = "vsco"

    def __init__(self, inst="violins", **params):
        man = _vsco_manifest()[inst]
        p = dict(r=man.get("release", 0.35), gain=1.0 * man.get("gain", 1.0), vel_range=12.0,
                 max_len=8.0, damp=man.get("sustain", False) or man.get("damp", True))
        p.update(params)
        zones = []
        for s in man["samples"]:
            if "midi" not in s:
                continue
            loop = (s["loopStart"], s["loopEnd"]) if "loopStart" in s else None
            zones.append(Zone(os.path.join(ASSETS, "vsco2", s["file"]), s["midi"],
                              s.get("vel", 1.0), tune=s.get("tune", 0.0), loop=loop,
                              offset=s.get("offset", 0.0), autotune=True))
        super().__init__(zones, **p)
        self.inst = inst
        self.sustain = man.get("sustain", False)
        if self.sustain:
            self.p["a"] = self.p.get("a", 0.04)

    def voice(self, n, dur_s, semis, sr, idx):
        y = super().voice(n, dur_s, semis, sr, idx)
        if self.sustain and self.p.get("a", 0) > 0:
            na = int(self.p["a"] * sr)
            y[:na] *= np.linspace(0, 1, min(na, len(y)))[:, None] ** 1.5
        return y


# ---------------------------------------------------------------------------
# SoundFonts (GeneralUser GS, MuseScore General Lite, FluidR3 Mono, TimGM6mb)
# ---------------------------------------------------------------------------
SOUNDFONTS = {
    "generaluser": "soundfonts/GeneralUser_GS_1.471.sf2",
    "musescore": "soundfonts/MuseScore_General_Lite.sf3",
    "fluidr3": "soundfonts/FluidR3Mono_GM.sf3",
    "timgm6mb": None,  # shipped inside pretty_midi (comparison only)
}


def sf_path(key):
    if key == "timgm6mb":
        import pretty_midi
        return os.path.join(os.path.dirname(pretty_midi.__file__), "TimGM6mb.sf2")
    p = SOUNDFONTS.get(key, key)
    return p if os.path.isabs(p) else os.path.join(ASSETS, p)


class SF2(Instrument):
    """Render a track through a SoundFont preset (tinysoundfont), block-accurate."""

    name = "sf2"

    def __init__(self, font="generaluser", program=0, bank=0, drums=False, gain_db=0.0,
                 **params):
        super().__init__(**params)
        self.font, self.program, self.bank, self.drums, self.gain_db = font, program, bank, drums, gain_db

    def render(self, track, score, sr=SR):
        import tinysoundfont as tsf
        syn = tsf.Synth(samplerate=sr, gain=self.gain_db)
        sid = syn.sfload(sf_path(self.font))
        ch = 9 if self.drums else 0
        syn.program_select(ch, sid, 128 if self.drums else self.bank, self.program,
                           is_drums=self.drums)
        total = score.n_samples(sr)
        ev = []
        for n in track.notes:
            a = score.b2n(n.t, sr)
            b = max(a + 1, score.b2n(n.end, sr))
            ev.append((a, 1, n.p, int(np.clip(n.v * 127, 1, 127))))
            ev.append((b, 0, n.p, 0))
        ev.sort()
        out = np.zeros((total, 2), dtype=np.float32)
        pos = 0
        for (s, kind, p, v) in ev + [(total, -1, 0, 0)]:
            s = min(s, total)
            if s > pos:
                blk = np.frombuffer(syn.generate(s - pos), dtype=np.float32).reshape(-1, 2)
                out[pos:s] = blk
                pos = s
            if kind == 1:
                syn.noteon(ch, p, v)
            elif kind == 0:
                syn.noteoff(ch, p)
        return out.astype(np.float64) * self.p.get("gain", 1.0)


# ---------------------------------------------------------------------------
# one-shots
# ---------------------------------------------------------------------------
@functools.lru_cache(maxsize=512)
def _oneshot(path, sr, trim):
    x = stereo(load(path, sr))
    if trim:
        x = trim_silence(x, -45.0, int(0.0003 * sr))
    x = x / (np.abs(x).max() + 1e-12)  # peak-normalised: kits set the balance in dB
    x.setflags(write=False)
    return x


def oneshot(name, sr=SR, trim=True):
    """Load a one-shot by short name: 'sonicpi/drum_snare_hard', 'tr808/BD/BD5010',
    'teropa/kick', 'vsco2/cymbal/crash1' (extension optional)."""
    base = os.path.join(ASSETS, name)
    if not os.path.splitext(base)[1]:
        for ext in (".flac", ".wav", ".WAV", ".mp3", ".aiff"):
            if os.path.exists(base + ext):
                base = base + ext
                break
    return np.array(_oneshot(base, sr, trim))


def list_assets(sub=""):
    d = os.path.join(ASSETS, sub)
    out = []
    for root, _, files in os.walk(d):
        for f in files:
            if f.lower().endswith((".wav", ".flac", ".mp3", ".aiff", ".sf2", ".sf3")):
                out.append(os.path.relpath(os.path.join(root, f), ASSETS))
    return sorted(out)
