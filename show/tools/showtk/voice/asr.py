"""Whisper QA (spec §10.2 step 4): faster-whisper small, int8, 2 threads, through the
vendored show/audio/voice/asr.py. QA only: Whisper never touches the audio.

wer(ref, hyp) is the word error rate (substitutions + deletions + insertions over the
reference words) after normalising case, punctuation, hyphens and numbers. The show's
names go in Whisper's initial prompt (a spelling hint, as a script supervisor would
give), so "Gigglesworth" is scored on how it sounds, not on how Whisper spells it.
"""
from __future__ import annotations

import importlib
import re

import numpy as np

from . import env

NAMES = ("HELLvile, Hellvile, HaHaland, Mayor Giggs, Gigglesworth, Nana Puddin, Pip, Father Fuzz, "
         "Bonejangles, Clinker, Dr. Bloom, Steamy, Kevin, Hattie Haha, Miss Lulla, Mascotty, "
         "Grumbleguts, Duckworth, Quackers, Mingle, sweet pea, Hee. Hee.")

_V = None


def _vasr():
    global _V
    if _V is None:
        env.setup()
        _V = importlib.import_module("asr")   # show/audio/voice/asr.py
    return _V


def transcribe(y, sr=env.SR, prompt=NAMES):
    return _vasr().transcribe(y, sr, prompt=prompt)


def word_times(y, sr=env.SR, prompt=NAMES):
    """-> [(word, t0_s, t1_s)] from faster-whisper's word timestamps."""
    from math import gcd

    from scipy.signal import resample_poly
    x = np.asarray(y, dtype=np.float64)
    if sr != 16000:
        g = gcd(int(sr), 16000)
        x = resample_poly(x, 16000 // g, int(sr) // g)
    x = (x / (np.abs(x).max() + 1e-9) * 0.7).astype(np.float32)
    segs, _ = _vasr().model().transcribe(x, language="en", beam_size=5, vad_filter=False,
                                         condition_on_previous_text=False, initial_prompt=prompt,
                                         word_timestamps=True)
    out = []
    for s in segs:
        for w in (s.words or []):
            out.append((w.word.strip(), float(w.start), float(w.end)))
    return out


_ONES = {"0": "zero", "1": "one", "2": "two", "3": "three", "4": "four", "5": "five", "6": "six",
         "7": "seven", "8": "eight", "9": "nine", "10": "ten"}


def norm_words(t):
    from .g2p import num_words
    t = t.lower().replace("’", "'").replace("-", " ").replace("—", " ")
    t = re.sub(r"\{[^}]*\}", " ", t)
    t = re.sub(r"\([^)]*\)", " ", t)
    out = []
    for w in re.findall(r"[a-z0-9']+", t):
        w = w.strip("'")
        if not w:
            continue
        if w.isdigit():
            out.extend(num_words(w).split())
        else:
            out.append(ABBR.get(w, w))
    return out


ABBR = {"dr": "doctor", "mr": "mister", "mrs": "missus", "st": "saint", "ok": "okay"}


def wer(ref, hyp):
    a, b = norm_words(ref), norm_words(hyp)
    if not a:
        return 0.0 if not b else 1.0
    n, m = len(a), len(b)
    d = list(range(m + 1))
    for i in range(1, n + 1):
        prev, d[0] = d[0], i
        for j in range(1, m + 1):
            cur = d[j]
            d[j] = min(d[j] + 1, d[j - 1] + 1, prev + (a[i - 1] != b[j - 1]))
            prev = cur
    return d[m] / n


def check(y, ref, sr=env.SR):
    hyp = transcribe(y, sr)
    return {"heard": hyp, "wer": round(wer(ref, hyp), 3), "recall": round(recall(ref, hyp), 3),
            "char": round(_vasr().char_sim(ref, hyp), 3)}


def recall(ref, hyp):
    """Share of the reference words heard, in order (insertions do not count against it)."""
    a, b = norm_words(ref), norm_words(hyp)
    if not a:
        return 1.0
    dp = [[0] * (len(b) + 1) for _ in range(len(a) + 1)]
    for i in range(len(a)):
        for j in range(len(b)):
            dp[i + 1][j + 1] = dp[i][j] + 1 if a[i] == b[j] else max(dp[i][j + 1], dp[i + 1][j])
    return dp[-1][-1] / len(a)
