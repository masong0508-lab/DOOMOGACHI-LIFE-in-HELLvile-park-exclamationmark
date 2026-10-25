"""Kokoro-82M through the vendored show/audio/voice/kokoro_voice.py (spec §10.2 step 2).

render(utt, voice) speaks each segment of an Utterance and returns a Render: the 24 kHz
waveform plus every token's time, read from the model's own duration prediction
(dur_pred; one unit = 600 smp at 24 kHz = 25 ms = 0.75 f). Segments are joined with the
exact pause the text asked for ({..} = R8). Deterministic on this container (2 threads).
"""
from __future__ import annotations

import dataclasses as dc

import numpy as np

from . import env

# Kokoro's audio runs ahead of its token grid: fricative bursts and bilabial closures
# sit 55-75 ms before the token starts (measured here on speak() output, and by the theme's
# singer.py); every token time below is corrected by this much.
OFFSET_K = -1560          # samples at 24 kHz (-65 ms)
HOP = 600

_KV = None


def kv():
    global _KV
    if _KV is None:
        env.setup()
        from kokoro_voice import KokoroVoice  # noqa: E402  (show/audio/voice)
        _KV = KokoroVoice()
    return _KV


def vocab():
    env.setup()
    from kokoro_voice import VOCAB
    return VOCAB


def parse_blend(spec):
    """'am_puck:0.6+am_michael:0.4' -> {'am_puck': 0.6, 'am_michael': 0.4}; 'bf_emma' -> 'bf_emma'."""
    if isinstance(spec, dict):
        return spec
    parts = [p for p in spec.split("+") if p]
    if len(parts) == 1 and ":" not in parts[0]:
        return parts[0]
    out = {}
    for p in parts:
        n, _, w = p.partition(":")
        out[n] = float(w or 1.0)
    return out


def blend_str(v):
    if isinstance(v, str):
        return v
    return "+".join(f"{k}:{w:g}" for k, w in v.items())


@dc.dataclass
class Tok:
    ch: str
    t0: int          # samples at 24 kHz, from the start of the render (offset-corrected)
    t1: int
    word: int = -1   # index into Render.words, -1 for spaces / punctuation / pads


@dc.dataclass
class Render:
    y: np.ndarray                 # float32, 24 kHz mono
    toks: list                    # [Tok]
    words: list                   # [(text, t0, t1)] at 24 kHz
    phon: str
    f0: list = dc.field(default_factory=list)   # per segment: model F0 (Hz per 12.5 ms)


def _strip_vocab(phon):
    V = vocab()
    bad = [c for c in phon if c not in V]
    if bad:
        raise ValueError(f"phonemes not in the Kokoro vocab: {''.join(sorted(set(bad)))!r} in {phon!r}")
    return phon


def speak_segment(phon, voice, speed):
    """-> (y24 float32, token chars incl. pads, token starts/ends in samples (raw grid), f0)."""
    _strip_vocab(phon)
    y, info = kv().speak(phon, parse_blend(voice), float(speed))
    dur = info["dur"].astype(np.int64)
    chars = ["^"] + list(phon) + ["$"]
    st = np.concatenate([[0], np.cumsum(dur)]) * HOP
    return np.asarray(y, dtype=np.float32), chars, st, info["f0"]


def render(utt, voice, speed=1.0, pause_frames_extra=0):
    """Speak an Utterance. Each segment is trimmed at its acoustic edges, and segments are
    joined by exactly their pause (frames) of silence."""
    out_y, toks, words, f0s = [], [], [], []
    pos = 0
    nseg = len(utt.segments)
    for si, seg in enumerate(utt.segments):
        y, chars, st, f0 = speak_segment(seg.phon, voice, speed)
        f0s.append(np.asarray(f0, dtype=np.float32))
        # map token -> word
        wmap = {}
        for wi, w in enumerate(seg.words):
            for t in range(w.tok0, w.tok1):
                wmap[t + 1] = wi  # +1: the leading pad token
        a, b = 0, len(y)
        if nseg > 1:
            a, b = _edges(y)
            if si == 0:
                a = 0
            if si == nseg - 1:
                b = len(y)
        seg_y = y[a:b]
        base_w = len(words)
        for j, ch in enumerate(chars):
            t0 = int(st[j]) + OFFSET_K - a + pos
            t1 = int(st[j + 1]) + OFFSET_K - a + pos
            toks.append(Tok(ch, t0, t1, base_w + wmap[j] if j in wmap else -1))
        for wi, w in enumerate(seg.words):
            ts = [t for t in toks if t.word == base_w + wi]
            words.append((w.text, ts[0].t0, ts[-1].t1))
        out_y.append(seg_y)
        pos += len(seg_y)
        if si < nseg - 1:
            gap = (seg.pause_after + pause_frames_extra) * env.SPF_K
            out_y.append(np.zeros(gap, dtype=np.float32))
            pos += gap
    return Render(np.concatenate(out_y) if out_y else np.zeros(1, np.float32), toks, words, utt.phon, f0s)


def _edges(y, thr_db=-45.0, hop=120):
    """First / last sample of sound (envelope above thr_db relative to the peak)."""
    n = len(y)
    if n < 2 * hop:
        return 0, n
    fr = y[: n // hop * hop].reshape(-1, hop)
    e = np.sqrt((fr.astype(np.float64) ** 2).mean(axis=1) + 1e-20)
    db = 20 * np.log10(e / (e.max() + 1e-20))
    idx = np.nonzero(db > thr_db)[0]
    if not len(idx):
        return 0, n
    return max(0, idx[0] * hop - hop), min(n, (idx[-1] + 2) * hop)
