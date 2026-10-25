"""ASR intelligibility check (QA only): OpenAI Whisper small (MIT), CTranslate2 int8
via faster-whisper. Transcribes audio (any rate) and scores word recall against the
intended lyric."""
from __future__ import annotations

import os
import re

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
# show/audio vendoring: WHISPER_MODEL points at the vault copy
MODEL = os.environ.get("WHISPER_MODEL") or os.path.abspath(
    os.path.join(HERE, "..", "..", "final_dl", "npm_whisper_small", "m"))
_M = None


def model():
    global _M
    if _M is None:
        from faster_whisper import WhisperModel
        _M = WhisperModel(MODEL, device="cpu", compute_type="int8", cpu_threads=2, num_workers=1)
    return _M


def transcribe(y, sr, lang="en", prompt=None):
    from scipy.signal import resample_poly
    y = np.asarray(y, dtype=np.float64)
    if y.ndim == 2:
        y = y.mean(axis=1)
    if sr != 16000:
        from math import gcd
        g = gcd(int(sr), 16000)
        y = resample_poly(y, 16000 // g, int(sr) // g)
    y = (y / (np.abs(y).max() + 1e-9) * 0.7).astype(np.float32)
    segs, info = model().transcribe(y, language=lang, beam_size=5, vad_filter=False,
                                    condition_on_previous_text=False, initial_prompt=prompt)
    return " ".join(s.text.strip() for s in segs)


def words(t):
    return re.findall(r"[a-z']+", t.lower().replace("-", " "))


def recall(ref, hyp):
    """Fraction of reference words found in order (LCS / len(ref))."""
    a, b = words(ref), words(hyp)
    if not a:
        return 1.0
    dp = [[0] * (len(b) + 1) for _ in range(len(a) + 1)]
    for i in range(len(a)):
        for j in range(len(b)):
            dp[i + 1][j + 1] = dp[i][j] + 1 if a[i] == b[j] else max(dp[i][j + 1], dp[i + 1][j])
    return dp[-1][-1] / len(a)


def char_sim(ref, hyp):
    """Letter-level similarity (spaces and punctuation ignored): 'ha ha land' == 'hahaland'."""
    import difflib
    a = re.sub(r"[^a-z]", "", ref.lower())
    b = re.sub(r"[^a-z]", "", hyp.lower())
    if not a:
        return 1.0
    m = difflib.SequenceMatcher(None, a, b)
    return sum(bl.size for bl in m.get_matching_blocks()) / max(len(a), 1)
