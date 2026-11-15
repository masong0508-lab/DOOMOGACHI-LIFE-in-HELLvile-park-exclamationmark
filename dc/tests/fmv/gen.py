#!/usr/bin/env python3
"""gen.py OUTDIR - the boot FMV host test's clip (DC9C): synthetic frames through
dc/tools/mkfmv.py (no ffmpeg needed), then for each frame the planes an ideal
float IDCT gives (expected.bin: Y, Cb, Cr per frame) and the source RGB
(source.bin), which test_dmv.c checks dmv.c against."""
import os
import sys

import numpy as np

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, "..", "..", "tools"))
import mkfmv  # noqa: E402

W, H = 64, 48
rng = np.random.RandomState(7)


def frames():
    yy, xx = np.mgrid[0:H, 0:W]
    f = []
    g = np.stack([xx * 4, yy * 5, (xx + yy) * 2], -1)                 # a smooth gradient
    f.append(np.clip(g, 0, 255).astype(np.uint8))
    bars = np.zeros((H, W, 3), np.uint8)                              # colour bars, soft edges
    cols = [(230, 40, 40), (40, 230, 40), (40, 40, 230), (230, 230, 40)]
    for i in range(W):
        bars[:, i] = cols[(i // 16) % 4]
    f.append(bars)
    f.append(np.zeros((H, W, 3), np.uint8))                           # black: every block a lone DC
    st = np.zeros((H, W, 3), np.uint8)                                # hard stripes: big levels (escape)
    st[:, ::2] = 255
    f.append(st)
    n = np.clip(120 + rng.randn(H, W, 3) * 18, 0, 255).astype(np.uint8)   # noise: long token runs
    f.append(n)
    return f


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    fr = frames()
    audio = (np.sin(np.arange(1470 * len(fr)) * 0.05) * 8000).astype("<i2")
    data = mkfmv.encode(fr, audio, fps=15, rate=22050, quality=2.0)
    open(os.path.join(out, "clip.dmv"), "wb").write(data)
    dqs = mkfmv.tables(2.0)
    exp = bytearray()
    for im in fr:
        y, cb, cr = mkfmv.rgb_to_planes(im)
        for p, dq in ((y, dqs[0]), (cb, dqs[1]), (cr, dqs[1])):
            lv = mkfmv.clamp_levels(mkfmv.quantise(p, dq))
            exp += mkfmv.reconstruct(lv, dq).tobytes()
    open(os.path.join(out, "expected.bin"), "wb").write(bytes(exp))
    open(os.path.join(out, "source.bin"), "wb").write(b"".join(im.tobytes() for im in fr))
    open(os.path.join(out, "audio.bin"), "wb").write(audio.tobytes())
    print("gen: %d frames %dx%d, %d bytes" % (len(fr), W, H, len(data)))


main()
