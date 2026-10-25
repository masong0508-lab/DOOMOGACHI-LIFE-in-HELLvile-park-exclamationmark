"""Render the same phrases through TimGM6mb, GeneralUser GS, MuseScore General
Lite, FluidR3 Mono and the kit's own instruments; measure them; write an A/B reel.

Outputs: demo/compare_sources.wav (each phrase: TimGM6mb, GeneralUser, MuseScore,
FluidR3, kit), demo/compare_sources.md (metrics table), compare_sources.json.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import numpy as np  # noqa: E402
from scipy import signal  # noqa: E402

import kit  # noqa: E402
from kit import theory  # noqa: E402
from kit.core import mono, note as N, stereo  # noqa: E402

FONTS = ["timgm6mb", "generaluser", "musescore", "fluidr3"]
# phrase name -> (GM program or 'drums', kit preset, phrase type)
PHRASES = [
    ("piano", 0, "piano", "keys"),
    ("electric piano", 4, "fm_epiano", "keys"),
    ("celesta", 8, "celesta", "mallet"),
    ("glockenspiel", 9, "glock_real", "mallet"),
    ("strings", 48, "strings", "pad"),
    ("synth brass", 62, "brass", "stabs"),
    ("saw lead", 81, "saw_lead", "lead"),
    ("square lead", 80, "square_lead", "lead"),
    ("finger bass", 33, "pick_bass", "bass"),
    ("slap bass", 36, "slap_bass", "bass"),
    ("synth bass", 38, "synth_bass", "bass"),
    ("distortion guitar", 30, "doom_guitar", "guitar"),
    ("drums", "drums", "drums_pop", "drums"),
]


def fill(tr, kind):
    if kind == "keys":
        for (t, c) in ((0, "Dbmaj7"), (2, "Eb"), (4, "Cm7"), (6, "Fm7")):
            tr.chord(t, 1.8, theory.voice(c, 64, 4), 0.75)
        tr.seq("Ab5:.5 Bb5:.5 C6:1 Eb6:.5 Db6:.5 C6:1", start=0, v=0.85)
    elif kind == "mallet":
        tr.seq("Eb6:.5 Ab6:.5 C7:.5 Eb7:.5 Db7:.5 C7:.5 Bb6:.5 Ab6:.5", v=0.8)
    elif kind == "pad":
        for (t, c) in ((0, "Dbmaj7"), (4, "Eb")):
            tr.chord(t, 3.9, theory.voice(c, 70, 4), 0.75)
    elif kind == "stabs":
        for t in (0, 1.5, 3, 4.5, 6):
            tr.chord(t, 0.45, theory.voice("Ab", 68, 4), 0.85)
    elif kind == "lead":
        tr.seq("C5:.5 C5:.5 Eb5:.5 C5:.5 Ab5:1 Eb5:1 F5:.5 F5:.5 Eb5:.5 C5:.5 Bb4:1 C5:1", v=0.85)
    elif kind == "bass":
        t = 0
        for nm in "Db2 Db2 Db3 Db2 Eb2 Eb2 Eb3 Eb2 C2 C2 C3 C2 F2 F2 Eb2 C2".split():
            tr.add(t, 0.45, N(nm), 0.85)
            t += 0.5
    elif kind == "guitar":
        for k in range(8):
            tr.add(k * .5, .45, N("Ab2"), .85, "mute")
        tr.add(4, 1.9, N("F2"), .95, "open")
        tr.add(6, 1.9, N("Eb2"), .95, "open")
    elif kind == "drums":
        tr.pattern("x.....x.x.....x.", 36, bars=2, v=0.9)
        tr.pattern("....x.......x...", 38, bars=2, v=0.9)
        tr.pattern("x.x.x.x.x.x.x.x.", 42, bars=2, v=0.6)
        tr.add(0, 2, 49, 0.85)


def metrics(y, sr=kit.SR):
    m = mono(y)
    f, P = signal.welch(m, sr, nperseg=8192)
    c = np.cumsum(P) / np.sum(P)
    f99 = float(f[np.searchsorted(c, 0.99)])
    f999 = float(f[np.searchsorted(c, 0.999)])
    cen = float(np.sum(f * P) / np.sum(P))
    # noise floor: quietest 50 ms window within the active region vs peak
    w = int(0.05 * sr)
    e = np.sqrt(np.convolve(m * m, np.ones(w) / w, mode="valid") + 1e-20)
    act = e > e.max() * 1e-3
    floor = float(20 * np.log10(np.percentile(e[act], 2) / e.max()))
    st = stereo(y)
    l, r = st[:, 0], st[:, 1]
    corr = float(np.sum(l * r) / (np.sqrt(np.sum(l * l) * np.sum(r * r)) + 1e-20))
    # HF content above 10 kHz relative to total
    hf = float(10 * np.log10(np.sum(P[f > 10000]) / np.sum(P) + 1e-20))
    return dict(f99_hz=f99, f999_hz=f999, centroid_hz=cen, quietest_vs_peak_db=floor,
                lr_corr=corr, hf_above_10k_db=hf)


def main():
    rows = []
    reel = []
    for name, prog, preset, kind in PHRASES:
        for src in FONTS + ["kit"]:
            s = kit.Score(bpm=150, key="Ab", bars=2, tail_beats=3)
            if src == "kit":
                inst = kit.make(preset)
            elif prog == "drums":
                inst = kit.make(f"sf2:{src}:drums")
            else:
                inst = kit.make(f"sf2:{src}:0:{prog}")
            tr = s.track(name, inst=inst, drum=(prog == "drums"))
            fill(tr, kind)
            if src == "kit" and preset == "slap_bass":  # the kit plays slap technique via articulations
                for n in tr.notes:
                    n.art = "pop" if n.p >= 48 else "slap"
            y = kit.render_track(tr, s)
            if not np.any(y):
                continue
            L = kit.lufs(y)
            y = y * kit.db2a(-20 - L) if np.isfinite(L) else y
            mt = metrics(y)
            mt.update(phrase=name, source=src)
            rows.append(mt)
            reel.append(y)
            reel.append(np.zeros((int(0.25 * kit.SR), 2)))
            print(f"{name:<18} {src:<12} f99 {mt['f99_hz']:7.0f}  f99.9 {mt['f999_hz']:7.0f}  "
                  f"HF>10k {mt['hf_above_10k_db']:6.1f} dB  corr {mt['lr_corr']:+.2f}", flush=True)
    y = np.concatenate(reel)
    y = y / np.abs(y).max() * 0.7
    kit.write_wav(os.path.join(HERE, "compare_sources.wav"), y)
    with open(os.path.join(HERE, "compare_sources.json"), "w") as fh:
        json.dump(rows, fh, indent=1)
    # markdown summary: per phrase, f99.9 bandwidth and HF content per source
    md = ["| phrase | " + " | ".join(FONTS + ["kit"]) + " |", "|---|" + "---|" * (len(FONTS) + 1)]
    for name, *_ in PHRASES:
        cells = []
        for src in FONTS + ["kit"]:
            r = next((r for r in rows if r["phrase"] == name and r["source"] == src), None)
            cells.append("-" if r is None else f"{r['f999_hz'] / 1000:.1f}k / {r['hf_above_10k_db']:.0f} dB / {r['lr_corr']:+.2f}")
        md.append(f"| {name} | " + " | ".join(cells) + " |")
    with open(os.path.join(HERE, "compare_sources.md"), "w") as fh:
        fh.write("cells: 99.9 % energy bandwidth / energy above 10 kHz / L-R correlation "
                 "(1.00 = mono)\n\n" + "\n".join(md) + "\n")
    print("\n".join(md))


if __name__ == "__main__":
    main()
