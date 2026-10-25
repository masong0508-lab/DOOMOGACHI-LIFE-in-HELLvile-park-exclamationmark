"""Alignment of recorded takes (spec §9.3 "Recorded takes"):
  1. faster-whisper word times (word_timestamps=True);
  2. inside each word, phones spread by the TTS render's own duration ratios for the
     same text (the line's own voice, unshaped);
  3. fallback when Whisper finds nothing usable: the energy envelope mapped to C/E/A
     pseudo-phones, so the mouth still flaps with the sound.
MFCC-DTW against the TTS render is deferred (spec §1.2).
"""
from __future__ import annotations

import re

import numpy as np

from . import asr, env, kokoro

SIL = set("^$ ,.!?;:—…")


def _norm(w):
    return re.sub(r"[^a-z0-9']", "", w.lower())


def tts_word_phones(text, params):
    """-> [(word_text, [(ch, frac0, frac1)])]: each word's phones as fractions of the word."""
    from .takes import g2p, phones_from_render
    utt = g2p()(text)
    rd = kokoro.render(utt, params.get("kokoro", "af_heart"), params.get("speed", 1.0))
    ph = phones_from_render(rd)
    out = []
    for (wt, a, b) in [(w, t0 * 2, t1 * 2) for w, t0, t1 in rd.words]:
        span = max(1, b - a)
        inside = [(c, (max(t0, a) - a) / span, (min(t1, b) - a) / span) for c, t0, t1 in ph
                  if c not in SIL and t1 > a and t0 < b]
        out.append((wt, inside))
    return out


def align_recording(y, line, params):
    """-> (phones, words, how) in samples relative to y's start."""
    say = params.get("say", line.text)
    try:
        ww = asr.word_times(y, env.SR)
    except Exception:  # noqa: BLE001 (QA model missing: fall back, never block a take)
        ww = []
    ref = tts_word_phones(say, params)
    if ww:
        phones, words = [], []
        # match reference words to Whisper words in order (LCS on normalised text)
        a = [_norm(w) for w, _ in ref]
        b = [_norm(w[0]) for w in ww]
        pairs = _lcs_pairs(a, b)
        if len(pairs) >= max(1, len(a) // 2):
            pmap = dict(pairs)
            # unmatched reference words take the gap between matched neighbours
            spans = []
            for i in range(len(a)):
                if i in pmap:
                    w = ww[pmap[i]]
                    spans.append((w[1], w[2]))
                else:
                    spans.append(None)
            _fill_spans(spans, len(y) / env.SR)
            for (wt, inside), (s0, s1) in zip(ref, spans):
                t0, t1 = int(s0 * env.SR), int(s1 * env.SR)
                words.append((wt, t0, t1))
                for c, f0, f1 in inside:
                    phones.append((c, t0 + int(f0 * (t1 - t0)), t0 + int(f1 * (t1 - t0))))
                phones.append((" ", t1, t1))
            return phones, words, "whisper"
    return energy_phones(y), [], "energy"


def _lcs_pairs(a, b):
    n, m = len(a), len(b)
    dp = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(n):
        for j in range(m):
            dp[i + 1][j + 1] = dp[i][j] + 1 if a[i] == b[j] else max(dp[i][j + 1], dp[i + 1][j])
    i, j, out = n, m, []
    while i and j:
        if a[i - 1] == b[j - 1]:
            out.append((i - 1, j - 1))
            i, j = i - 1, j - 1
        elif dp[i - 1][j] >= dp[i][j - 1]:
            i -= 1
        else:
            j -= 1
    return out[::-1]


def _fill_spans(spans, total):
    n = len(spans)
    for i in range(n):
        if spans[i] is None:
            lo = next((spans[k][1] for k in range(i - 1, -1, -1) if spans[k]), 0.0)
            hi = next((spans[k][0] for k in range(i + 1, n) if spans[k]), total)
            j = i
            while j < n and spans[j] is None:
                j += 1
            cnt = j - i
            step = (hi - lo) / max(1, cnt)
            for q in range(cnt):
                spans[i + q] = (lo + q * step, lo + (q + 1) * step)


def energy_phones(y, hop=None):
    """Loudness -> pseudo-phones: loud = 'a' (A), medium = 'e' (E), soft = 't' (C)."""
    hop = hop or env.SPF // 2
    y = np.asarray(y, np.float64)
    n = len(y) // hop
    if n == 0:
        return []
    e = np.sqrt((y[: n * hop].reshape(n, hop) ** 2).mean(axis=1) + 1e-20)
    db = 20 * np.log10(e / e.max())
    out = []
    for i, d in enumerate(db):
        c = " " if d < -38 else ("a" if d > -8 else ("e" if d > -18 else "t"))
        out.append((c, i * hop, (i + 1) * hop))
    return out
