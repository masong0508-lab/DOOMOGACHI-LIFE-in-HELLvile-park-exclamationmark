#!/usr/bin/env python3
"""compare.py - compare Dreamcast screenshots with the native runner (DC3;
owner: dreamcast).

    python3 -I dc/harness/compare.py DC_SHOTS NATIVE_SHOTS [--diff DIR]
                                     [--gate 99] [--close-gate 99.9]
                                     [--known FILE --script NAME]

DC_SHOTS holds the 640x480 PNGs dcrun wrote for a script (`shot NAME`),
NATIVE_SHOTS the 320x240 PNGs of `core/build/runner --w 320 --h 240` on
the same script. Each pair (same file name) is compared logical pixel by
logical pixel: a Dreamcast shot is downscaled by taking the top-left pixel
of every 2x2 block (nearest), and both sides are reduced to RGB565, the
precision of the console's frame buffer. A pixel is
    exact  when it is equal, and
    close  when every channel is within 16/255 (the shade fits, blends).
A pair fails when exact < --gate % or close < --close-gate %. Shots that
are 320x240 already (dcrun --half) are compared as they are. With --diff,
a picture per pair is written: grey = exact, yellow = close, red = not.
With --known (dc/harness/known_diffs.tsv), the differences listed there
for --script NAME are expected, each with its reason: "mask X Y W H" (a
320x240 rectangle that may differ: a platform label, a Dreamcast-only
option row) or "gate PCT" (a lower exact gate until a later package draws
the rest, e.g. the 3D view). Such a pair prints "known" and the reason.
Exit status 1 when a pair fails or a native shot has no Dreamcast twin.
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image


def as565(im):
    """H x W x 3 array of the RGB565 components (r5, g6, b5) as ints."""
    a = np.asarray(im.convert("RGB"), dtype=np.int32)
    return np.stack([a[..., 0] >> 3, a[..., 1] >> 2, a[..., 2] >> 3], axis=-1), a


def load_known(path, script):
    """{shot: [(kind, args, reason)]} for one script from a known-diffs TSV."""
    known = {}
    with open(path) as f:
        for line in f:
            if not line.strip() or line.startswith("#"):
                continue
            cols = line.rstrip("\n").split("\t")
            if len(cols) != 4:
                raise ValueError("%s: want 4 tab-separated columns: %r" % (path, line))
            sc, shot, what, reason = cols
            if sc != script:
                continue
            w = what.split()
            if w[0] == "mask" and len(w) == 5:
                known.setdefault(shot, []).append(("mask", [int(v) for v in w[1:]], reason))
            elif w[0] == "gate" and len(w) == 2:
                known.setdefault(shot, []).append(("gate", float(w[1]), reason))
            else:
                raise ValueError("%s: bad entry %r" % (path, what))
    return known


def compare(dc_path, nat_path, diff_path, masks=()):
    nat, nat_rgb = as565(Image.open(nat_path))
    dc, _ = as565(Image.open(dc_path))
    h, w = nat.shape[:2]
    if dc.shape[:2] == (h * 2, w * 2):
        dc = dc[::2, ::2]               # the top-left pixel of each 2x2 block
    if dc.shape[:2] != (h, w):
        raise ValueError("%s is %dx%d, want %dx%d or twice that" % (dc_path, dc.shape[1], dc.shape[0], w, h))
    d = np.abs(dc - nat)
    exact = np.all(d == 0, axis=-1)
    close = (d[..., 0] * 8 <= 16) & (d[..., 1] * 4 <= 16) & (d[..., 2] * 8 <= 16)
    for x, y, mw, mh in masks:          # known differences count as equal
        exact[y:y + mh, x:x + mw] = True
        close[y:y + mh, x:x + mw] = True
    if diff_path:
        g = (nat_rgb.sum(axis=-1) // 6).astype(np.uint8)
        out = np.zeros((h, w, 3), dtype=np.uint8)
        out[..., 0] = np.where(exact, g, 255)
        out[..., 1] = np.where(exact, g, np.where(close, 220, 0))
        out[..., 2] = np.where(exact, g, 0)
        Image.fromarray(out, "RGB").save(diff_path)
    n = float(h * w)
    return 100.0 * exact.sum() / n, 100.0 * close.sum() / n


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dc")
    ap.add_argument("native")
    ap.add_argument("--diff")
    ap.add_argument("--gate", type=float, default=99.0)
    ap.add_argument("--close-gate", type=float, default=99.9)
    ap.add_argument("--known")
    ap.add_argument("--script")
    a = ap.parse_args()
    known = load_known(a.known, a.script or "") if a.known else {}
    if a.diff:
        os.makedirs(a.diff, exist_ok=True)
    fail = 0
    names = sorted(f for f in os.listdir(a.native) if f.endswith(".png"))
    for name in names:
        dc_path = os.path.join(a.dc, name)
        if not os.path.exists(dc_path):
            print("MISSING %s (no Dreamcast shot)" % name)
            fail = 1
            continue
        k = known.get(name, [])
        masks = [args for kind, args, _ in k if kind == "mask"]
        gates = [args for kind, args, _ in k if kind == "gate"]
        e, c = compare(dc_path, os.path.join(a.native, name),
                       os.path.join(a.diff, name) if a.diff else None, masks)
        if gates:
            ok = e >= min(gates)
        else:
            ok = e >= a.gate and c >= a.close_gate
        fail |= not ok
        tag = ("known" if k else "ok   ") if ok else "FAIL "
        why = ("  [%s]" % "; ".join(r for _, _, r in k)) if k else ""
        print("%s %s exact %.3f%% close %.3f%%%s" % (tag, name, e, c, why))
    if not names:
        print("no native shots in %s" % a.native)
        fail = 1
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
