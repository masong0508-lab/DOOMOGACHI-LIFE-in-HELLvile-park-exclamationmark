"""Smoke test: every preset renders a short phrase; finite, non-silent, peak sane, tuned."""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import numpy as np
import kit
from kit import qa

only = sys.argv[1:]
s = kit.Score(bpm=120, bars=2, tail_beats=4)
res = []
for key in kit.PRESETS:
    if only and key not in only:
        continue
    t0 = time.time()
    inst = kit.make(key)
    sc = kit.Score(bpm=120, bars=2, tail_beats=4)
    tr = sc.track("t", inst=inst, drum=key.startswith("drums"))
    if key.startswith("drums"):
        tr.pattern("x...x...x...x...", 36, bars=1)
        tr.pattern("..x...x...x...x.", 42, step=0.25, bars=1)
        tr.pattern("....x.......x...", 38, bars=1)
        tr.add(4, 1, 49, 0.9)
    elif key == "fx":
        tr.add(0, 2, 60, 0.8, fx="riser"); tr.add(2, 1, 36, 0.8, fx="impact"); tr.add(4, 1, 60, 0.8, fx="scratch", pattern="chirp"); tr.add(6, 0.5, 60, 0.9, fx="hey")
    else:
        lo = 36 if "bass" in key or key in ("reese", "doom_guitar", "contrabass", "celli", "timpani") else 60
        for i, p in enumerate([0, 4, 7, 12]):
            tr.add(i, 0.9, lo + p, 0.8)
        tr.add(4, 2, lo, 0.9)
    y = inst.render(tr, sc)
    ok = np.isfinite(y).all()
    pk = float(np.abs(y).max())
    rms = float(np.sqrt(np.mean(y ** 2)))
    tun = None
    if not key.startswith(("drums", "fx", "chip_noise", "doom")):
        try:
            lo = 36 if "bass" in key or key in ("reese", "contrabass", "celli", "timpani") else 60
            r = qa.tuning_audio(kit.make(key), None, pitches=(lo, lo + 4, lo + 7, lo + 12, lo + 16))
            tun = r["max_abs_cents"]
        except Exception as e:
            tun = "err " + str(e)[:40]
    res.append((key, ok, pk, rms, tun, time.time() - t0))
    print(f"{key:<16} finite={ok} peak={pk:6.3f} rms={20*np.log10(rms+1e-12):6.1f} dB  tuning_max_cents={tun}  {time.time()-t0:5.2f}s", flush=True)
