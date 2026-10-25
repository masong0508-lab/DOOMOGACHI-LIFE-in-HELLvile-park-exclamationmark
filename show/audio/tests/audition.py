"""Audition presets: render a phrase per preset -> wav + spectrogram png + band summary.
usage: python tests/audition.py preset [preset ...]   (outputs in tests/out/)"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import numpy as np
import kit
from kit import qa
from kit.core import note as N

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
os.makedirs(OUT, exist_ok=True)

def phrase(key, tr):
    if key.startswith("drums"):
        tr.pattern("x...x...x...x...", 36, bars=2, v=0.95)
        tr.pattern("....x.......x...", 38, bars=2, v=0.9)
        tr.pattern("..x...x...x...x.", 42, bars=2, v=0.6)
        tr.add(8, 2, 49, 0.9)
        return
    if "bass" in key or key in ("reese",):
        seq = "Ab1:.5 Ab1:.5 Ab2:.25 Ab1:.25 Ab1:.5 Eb2:.5 Eb2:.5 F2:.5 G2:.5 Db2:1 C2:1 Bb1:1 Eb2:1"
        arts = ["", "", "pop", "ghost", "", "slap", "", "", "", "slap", "", "", ""]
        t = 0
        for tok, a in zip(seq.split(), arts):
            nm, d = tok.split(":")
            tr.add(t, float(d) * 0.9, N(nm), 0.85, a if key == "slap_bass" else "")
            t += float(d)
        return
    if key == "doom_guitar":
        for i in range(8):
            tr.add(i * 0.5, 0.45, N("Ab2"), 0.85, "mute")
        tr.add(4, 2, N("F2"), 0.95, "open"); tr.add(6, 2, N("Db2"), 0.95, "open")
        return
    tr.seq("Ab4:.5 C5:.5 Eb5:.5 Ab5:.5 G5:1 Eb5:1 F5:.5 Ab5:.5 C6:1 Bb5:2", v=0.85)
    for p in ("Ab3", "C4", "Eb4"):
        tr.add(8, 3, N(p), 0.8)

for key in sys.argv[1:]:
    s = kit.Score(bpm=150, bars=3, tail_beats=4)
    inst = kit.make(key)
    tr = s.track("t", inst=inst, drum=key.startswith("drums"))
    phrase(key, tr)
    y = inst.render(tr, s)
    y = y / (np.abs(y).max() + 1e-9) * 0.7
    kit.write_wav(os.path.join(OUT, key + ".wav"), y)
    qa.spectrogram_png(y, os.path.join(OUT, key + ".png"), title=key)
    lv = qa.stem_levels({key: y})[key]
    print(f"{key:<14} sub {lv['sub']:6.1f} low {lv['low']:6.1f} mid {lv['mid']:6.1f} high {lv['high']:6.1f} air {lv['air']:6.1f}")
