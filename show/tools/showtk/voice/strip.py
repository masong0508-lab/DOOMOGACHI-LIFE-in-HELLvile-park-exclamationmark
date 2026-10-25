"""A picture of a locked line: waveform and spectrogram on the frame grid, the mouth track
letter by letter (shifted by mouth_from, so a letter sits over the frame it is shown on),
word spans, and the measured bilabial closures (green = an M within 1 f, red = none).

    python3 -m showtk.voice strip <ep> <line> [--out x.png]    (also: strip_lines() for a sheet)
"""
from __future__ import annotations

import numpy as np

from . import env, lock, mouth as mouthm, takes


def strip_lines(ep, ids, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    d = env.ep_dir(ep)
    rows = lock.read(d)["lines"]
    ids = [i for i in ids if i in rows and rows[i].get("take")]
    fig, axes = plt.subplots(len(ids), 1, figsize=(14, 2.3 * len(ids)), squeeze=False)
    for ax, lid in zip(axes[:, 0], ids):
        r = rows[lid]
        y = takes.load(env.unrel(r["take"]))
        on = r["onset"]
        t = (np.arange(len(y)) - on) / env.SPF        # in frames, 0 = the say frame
        nf = len(r["mouth"]) + r["mouth_from"] + 1
        from scipy.signal import spectrogram
        fq, ts, S = spectrogram(y, fs=env.SR, nperseg=512, noverlap=448)
        keep = fq <= 8000
        Sdb = 10 * np.log10(S[keep] + 1e-12)
        ax.imshow(Sdb, origin="lower", aspect="auto", cmap="magma", vmin=Sdb.max() - 80, vmax=Sdb.max(),
                  extent=((ts[0] * env.SR - on) / env.SPF, (ts[-1] * env.SR - on) / env.SPF, 0, 8000))
        ax2 = ax.twinx()
        ax2.plot(t, y, color="#7fd1ff", lw=0.4, alpha=0.8)
        ax2.set_ylim(-1.2, 1.2)
        ax2.set_yticks([])
        ax.set_ylim(0, 8000)
        for i, c in enumerate(r["mouth"]):
            f = i + r["mouth_from"]
            col = {"M": "#ff5050", "X": "#888888"}.get(c, "white")
            ax.text(f + 0.5, 7300, c, color=col, ha="center", va="center", fontsize=8, family="monospace",
                    fontweight="bold")
        for w, a, b in r["words"]:
            ax.axvspan(a / env.SPF, b / env.SPF, ymin=0.0, ymax=0.06, color="#ffd060", alpha=0.6)
            ax.text(a / env.SPF, 300, w, color="black", fontsize=7, va="bottom")
        if r.get("mouth"):
            ons = mouthm.closure_onsets(y[on:], [tuple(p) for p in r["phones"]])
            for (f, ok), tt in zip(mouthm.check_closures(r["mouth"], r["mouth_from"], ons), ons):
                ax.axvline(tt / env.SPF, color="#30ff60" if ok else "#ff2020", lw=1.2)
        for f in range(int(np.floor(t[0])), nf + 1):
            ax.axvline(f, color="white", lw=0.2, alpha=0.25)
        ax.set_xlim(min(-2, t[0]), max(nf, t[-1]))
        a = r.get("asr")
        heard = a.get("heard") if isinstance(a, dict) else a
        ax.set_title(f"{lid} [{r['kind']} {r['voice']}] {r['text']!r}  ->  heard {heard!r}   "
                     f"{r['frames']} f, {r.get('lufs')} LUFS", fontsize=9, loc="left")
        ax.set_ylabel("Hz")
    axes[-1, 0].set_xlabel("frames from the say frame (30 fps)")
    fig.tight_layout()
    fig.savefig(out, dpi=90)
    plt.close(fig)
    return out
