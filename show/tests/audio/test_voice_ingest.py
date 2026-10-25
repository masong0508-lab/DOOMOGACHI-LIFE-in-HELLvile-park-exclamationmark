"""showtk.voice.ingest: mouth-track rules (spec sec.9.3) on a synthetic take.

    cd show/tools && python3 -m pytest ../tests/audio/test_voice_ingest.py -q
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
from showtk.voice import ingest  # noqa: E402


def synth(words, sr=48000):
    """a tone burst per word, silence between: 'asr' words at known times"""
    y = np.zeros(int(sr * (0.6 + 0.45 * len(words))))
    asr, t = [], 0.3
    for w in words:
        n = int(0.32 * sr)
        s = int(t * sr)
        y[s:s + n] = 0.2 * np.sin(np.arange(n) * 2 * np.pi * 220 / sr)
        asr.append((w, t, t + 0.32))
        t += 0.45
    return y, asr


def test_bilabials_and_variety():
    text = "Page one! Manners! We'll keep you."
    y, asr = synth(ingest.words_of(text))
    tr, ph, _ = ingest.align(y, text, asr)
    assert len(tr) == -(-len(y) // 1600)
    chk = ingest.check_track(tr, ph)
    assert chk["bilabials"] == 3          # P(age), M(anners), P (keep)
    assert chk["ok"], (tr, chk)
    assert set(tr) <= set("XMCEAOUF")


def test_holds_and_gaps():
    text = "My speech. I've given it four hundred times."
    y, asr = synth(ingest.words_of(text))
    tr, ph, _ = ingest.align(y, text, asr)
    import re
    # no 1-2 frame X holes between shapes (gaps close only when >= 3 f)
    assert not re.search(r"[^X]X{1,2}[^X]", tr), tr
    # non-M shapes hold >= 2 f except right before an M
    for m in re.finditer(r"([CEAOUF])\1*", tr):
        if m.end() - m.start() == 1:
            assert m.end() < len(tr) and tr[m.end()] in "MX", tr


def test_unmatched_words_interpolated():
    text = "Welcome to HaHaland, where"
    y, asr = synth(["welcome", "to", "ha", "ha", "land", "boy"])
    tr, ph, wt = ingest.align(y, text, asr)
    assert all(b > a for a, b in wt)
    assert all(wt[i][0] <= wt[i + 1][0] for i in range(len(wt) - 1))
    assert ingest.check_track(tr, ph)["ok"]


def test_short_shape_does_not_flood():
    """a 1-frame shape steals one frame from the next shape, never a cascade"""
    ph = [("a", "AH", 0.10, 0.12), ("b", "IY", 0.12, 0.50), ("c", "UW", 0.50, 0.90)]
    tr = ingest.mouth_track(ph, 40)
    assert "E" in tr and "U" in tr and tr.count("A") >= 2
