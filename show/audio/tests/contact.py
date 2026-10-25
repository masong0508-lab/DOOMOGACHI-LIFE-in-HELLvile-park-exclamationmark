"""Contact sheet of spectrograms + 1/3-oct curves for audition wavs: python tests/contact.py out.png a b c ..."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import numpy as np, soundfile as sf
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy import signal
from kit import qa
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
names = sys.argv[2:]
cols = 4
rows = (len(names) + cols - 1) // cols
fig, axs = plt.subplots(rows * 2, cols, figsize=(5 * cols, 4.2 * rows), gridspec_kw=dict(height_ratios=[2.2, 1] * rows))
axs = np.atleast_2d(axs)
for i, nm in enumerate(names):
    x, sr = sf.read(os.path.join(OUT, nm + ".wav"), always_2d=True)
    m = x.mean(1)
    r, c = divmod(i, cols)
    ax = axs[2 * r, c]
    f, t, Z = signal.stft(m, sr, nperseg=2048, noverlap=2048 - 256)
    S = 20 * np.log10(np.abs(Z) + 1e-9); S -= S.max()
    sel = (f > 25)
    ax.pcolormesh(t, f[sel], S[sel], vmin=-80, vmax=0, cmap="magma", shading="auto")
    ax.set_yscale("log"); ax.set_ylim(25, 20000); ax.set_title(nm, fontsize=10)
    ax2 = axs[2 * r + 1, c]
    lv = qa.third_octave(x, sr)
    ax2.semilogx(qa.THIRD_OCT, lv, "o-", ms=2)
    ax2.set_ylim(-40, 25); ax2.grid(alpha=.3)
for j in range(len(names), rows * cols):
    r, c = divmod(j, cols); axs[2 * r, c].axis("off"); axs[2 * r + 1, c].axis("off")
fig.tight_layout(); fig.savefig(sys.argv[1], dpi=70)
print(sys.argv[1])
