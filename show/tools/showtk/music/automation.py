"""Mute, duck, bend, tape-stop: music automation over a continuous cue render
(SHOW_ENGINE_SPEC.md sec.5.5 `mx† <cue> start|stop|mute F|duck <g> F|bend <a>-><b>
over=F`, `tapestop† F`; sec.10.4; the game's own rules in core/src/audio_fx.c).

The cue is always rendered as one continuous take. Automation never stops the
"sequencer": a mute is bus gain over the render, so the music comes back on
its own grid exactly where a game cue would (audio_fx.c: "the sequencer keeps
running underneath a mute"). A tape-stop slides the pitch down 15 semitones
over F frames (audio_fx.c `audio_tapestop`) by varispeed, then holds the
music muted until `resume` (on the next beat of the cue's grid), `start` of a
new cue, or a timed mute ends. A bend is pitch automation: score cues are
re-rendered with the bend in their notes (cuelib), file cues are bent by
varispeed.

Events use the audio_events.json shape (sec.6.6), one dict per op, absolute
episode samples:

  {"smp": S, "src": "cue:X", "from": "bar:1.1", "gain_db": -12}       start
  {"smp": S, "src": "cue:X", "op": "stop", "over_smp": 0}               stop (fade)
  {"smp": S, "src": "cue:X", "op": "mute", "over_smp": 46400}           mute 29 f
  {"smp": S, "src": "cue:X", "op": "duck", "gain_db": -6, "over_smp": N} duck N
  {"smp": S, "src": "cue:X", "op": "to", "gain_db": -18, "over_smp": N}  level ramp
  {"smp": S, "src": "cue:X", "op": "bend", "from_st": 0, "to_st": -2, "over_smp": N}
  {"smp": S, "src": "cue:X", "op": "tapestop", "over_smp": 8000}        5 f
  {"smp": S, "src": "cue:X", "op": "resume"}                            next beat

A duck's level is 0.4-0.75 (lint: -8 to -2.5 dB) for 5-20 f by the house rules.
"""
from __future__ import annotations

import numpy as np

SR = 48000
SPF = 1600
DECLICK = 96            # 2 ms ramps on every hard gain change
TAPE_SEMIS = 15.0       # audio_fx.c: 15 semitones down over the tape-stop


def _ramp_to(env, i0, i1, g0, g1):
    """Linear ramp of env[i0:i1] from g0 to g1 (clipped to the buffer)."""
    n = len(env)
    i0c, i1c = max(0, i0), min(n, i1)
    if i1c <= i0c:
        return
    x = (np.arange(i0c, i1c) - i0) / max(1, i1 - i0)
    env[i0c:i1c] = g0 + (g1 - g0) * x


def gain_envelope(n: int, ops: list, base_db: float = 0.0, t0: int = 0) -> np.ndarray:
    """Linear gain per sample for one cue instance of n samples starting at
    episode sample t0. ops: event dicts (absolute samples)."""
    env = np.full(n, 10 ** (base_db / 20.0))
    level = env[0]
    for e in sorted(ops, key=lambda e: e["smp"]):
        op = e.get("op")
        s = int(e["smp"]) - t0
        if op == "to":
            g1 = 10 ** (float(e["gain_db"]) / 20.0)
            ov = int(e.get("over_smp", DECLICK))
            _ramp_to(env, s, s + ov, level, g1)
            if s + ov < n:
                env[max(0, s + ov):] = g1
            level = g1
    out = env.copy()
    for e in sorted(ops, key=lambda e: e["smp"]):
        op = e.get("op")
        s = int(e["smp"]) - t0
        if op == "mute":
            ov = int(e["over_smp"])
            m = np.ones(n)
            _ramp_to(m, s - DECLICK, s, 1.0, 0.0)
            m[max(0, s):max(0, min(n, s + ov))] = 0.0
            _ramp_to(m, s + ov, s + ov + DECLICK, 0.0, 1.0)
            out *= m
        elif op == "duck":
            g = 10 ** (float(e["gain_db"]) / 20.0)
            ov = int(e["over_smp"])
            att, rel = 240, 1920                     # 5 ms in, 40 ms out
            m = np.ones(n)
            _ramp_to(m, s - att, s, 1.0, g)
            m[max(0, s):max(0, min(n, s + ov))] = g
            _ramp_to(m, s + ov, s + ov + rel, g, 1.0)
            out *= m
        elif op == "stop":
            ov = max(DECLICK, int(e.get("over_smp", 0)))
            m = np.ones(n)
            _ramp_to(m, s, s + ov, 1.0, 0.0)
            m[max(0, s + ov):] = 0.0
            out *= m
    return out


def varispeed(x: np.ndarray, rate: np.ndarray) -> np.ndarray:
    """Read x at a time-varying speed (rate per output sample, 1 = normal), with
    linear interpolation (enough for a tape-stop's falling pitch)."""
    pos = np.concatenate([[0.0], np.cumsum(rate[:-1])])
    pos = np.clip(pos, 0, len(x) - 1.001)
    i = pos.astype(int)
    f = (pos - i)[:, None]
    return x[i] * (1 - f) + x[np.minimum(i + 1, len(x) - 1)] * f


def tapestop_curve(n: int, semis: float = TAPE_SEMIS) -> np.ndarray:
    """Playback rate over a tape-stop of n samples: pitch falls linearly in
    semitones (audio_fx.c interpolates semi_down_q12 in 1/16 semitones)."""
    st = np.linspace(0.0, semis, n, endpoint=False)
    return 2.0 ** (-st / 12.0)


def render_instance(audio: np.ndarray, length: int, t0: int, offset: int, ops: list,
                    base_db: float = 0.0, beat_smp: float | None = None) -> np.ndarray:
    """One cue instance on the episode clock: `length` samples from episode
    sample t0, reading the cue from `offset`. Applies stop / mute / duck / to /
    tapestop / resume. Returns (length, 2)."""
    a = np.asarray(audio, dtype=float)
    if a.ndim == 1:
        a = np.stack([a, a], 1)
    seg = np.zeros((length, 2))
    take = a[offset: offset + length]
    seg[: len(take)] = take
    env = gain_envelope(length, ops, base_db, t0)
    # tape-stops: replace the window with the varispeed read, then hold silence
    hold = np.ones(length)
    for e in sorted(ops, key=lambda e: e["smp"]):
        if e.get("op") != "tapestop":
            continue
        s = int(e["smp"]) - t0
        n = int(e["over_smp"])
        if s < 0 or s >= length:
            continue
        rate = tapestop_curve(n)
        src = a[offset + s: offset + s + int(np.ceil(rate.sum())) + 2]
        if len(src) > 2:
            w = varispeed(src, rate)
            w *= np.linspace(1.0, 0.0, len(w))[:, None] ** 0.5   # the level sags with the motor
            e_ = min(length, s + len(w))
            seg[s:e_] = w[: e_ - s]
        # held mute after the stop, until a resume / a later start
        res = [r for r in ops if r.get("op") == "resume" and int(r["smp"]) - t0 >= s + n]
        r_at = length
        if res:
            r0 = int(min(res, key=lambda r: r["smp"])["smp"]) - t0
            if beat_smp:
                # the next beat of the cue's own grid (audio_fx.c: the next quarter)
                pos = offset + r0
                k = int(np.ceil(pos / beat_smp - 1e-9))
                r0 = int(round(k * beat_smp)) - offset
            r_at = r0
        hold[s + n: r_at] = 0.0
        if r_at < length:
            _ramp_to(hold, r_at, r_at + DECLICK, 0.0, 1.0)
    return seg * (env * hold)[:, None]


def bend_curve(ops: list, t0: int, length: int) -> np.ndarray | None:
    """Semitone offset per sample from the bend ops (holds the last value)."""
    bends = [e for e in ops if e.get("op") == "bend"]
    if not bends:
        return None
    c = np.zeros(length)
    for e in sorted(bends, key=lambda e: e["smp"]):
        s = int(e["smp"]) - t0
        n = int(e.get("over_smp", 1))
        a, b = float(e.get("from_st", 0.0)), float(e["to_st"])
        _ramp_to(c, s, s + n, a, b)
        if s + n < length:
            c[max(0, s + n):] = b
    return c
