#!/usr/bin/env python3
"""slider_shots_check.py - CR-BODY: did every build slider move the creator's preview? (any platform)

    python3 -I tools/slider_shots_check.py SHOT_DIR [--min-px 12] [--expect 11]

SHOT_DIR holds the PNGs of core/tests/scripts/creator_sliders.txt: the native runner (240x160 or 320x240),
the PS1 harness (tools/../ps1/harness/run.py), Dreamcast shots (640x480, or --half). For every slider NN_name:

    NN_name_min  / _min_b    the lowest step, and the same picture a few ticks later (the animation's noise)
    NN_name_max  / _max_b    the highest step, and again
    NN_name_reset            after A: the slider is back at its default

Only the preview pane (the left part of the screen, away from the option list and its slider bars) is
compared, in the screen's own pixels. A slider passes when the lowest and the highest picture differ by more
than --min-px pixels that held still in the repeated shots (the preview's own animation is left out); the
reset must differ from the lowest step too (the slider really went back). The sixteen body_NN shots must each differ
from the one before. Output: one line per slider with its result; exit status 1 when any fails or
fewer than --expect sliders were found (a script that never reached a slider must not pass).
"""
import argparse
import os
import re
import sys

import numpy as np
from PIL import Image

PANE = (0.0, 0.10, 0.27, 0.88)       # x0, y0, x1, y1 as fractions of the screen: the preview pane


def load(path):
    im = Image.open(path).convert("RGB")
    a = np.asarray(im, dtype=np.int16)
    h, w = a.shape[:2]
    x0, y0, x1, y1 = (int(PANE[0] * w), int(PANE[1] * h), int(PANE[2] * w), int(PANE[3] * h))
    return a[y0:y1, x0:x1], (w, h)


def differ(a, b):
    """mask of the pixels that differ by more than a rounding step in any channel (RGB565, scaling)"""
    if a.shape != b.shape:
        return np.ones(a.shape[:2], dtype=bool)
    return np.abs(a - b).max(axis=-1) > 12


def diff(a, b):
    return int(differ(a, b).sum())


def selftest():
    """a slider that moves nothing fails, one that moves the picture passes, animation is left out"""
    import tempfile
    rng = np.random.RandomState(5)
    W, H = 240, 160
    base = np.full((H, W, 3), 200, dtype=np.uint8)

    def write(d, name, arr):
        Image.fromarray(arr).save(os.path.join(d, name + ".png"))

    def shots(d, n, lo, hi, anim):
        a2 = lo.copy()
        b2 = hi.copy()
        if anim:                       # a blink in the repeated shots: pixels inside the pane that move
            a2[40:60, 5:30] = 0
            b2[40:60, 5:30] = 0
        for k, arr in (("min", lo), ("min_b", a2), ("max", hi), ("max_b", b2), ("reset", lo.copy())):
            write(d, "%s_%s" % (n, k), arr)

    with tempfile.TemporaryDirectory() as d:
        moved = base.copy()
        moved[80:120, 10:40] = 50
        shots(d, "01_still", base, base.copy(), True)         # does nothing
        shots(d, "02_moves", base, moved, True)
        shots(d, "03_blink_only", base, base.copy(), False)
        h = base.copy()
        h[40:60, 5:30] = 0
        shots(d, "04_only_a_blink", base, h, True)            # differs only where the preview animates
        for k in range(3):
            write(d, "body_%02d" % k, np.full((H, W, 3), 100 + 40 * (k % 2), dtype=np.uint8))
        r = {}
        for n in ("01_still", "02_moves", "03_blink_only", "04_only_a_blink"):
            lo = load(os.path.join(d, n + "_min.png"))[0]
            hi = load(os.path.join(d, n + "_max.png"))[0]
            lb = load(os.path.join(d, n + "_min_b.png"))[0]
            hb = load(os.path.join(d, n + "_max_b.png"))[0]
            moving = differ(lo, lb) | differ(hi, hb)
            r[n] = int((differ(lo, hi) & ~moving).sum())
        assert r["01_still"] == 0, r
        assert r["02_moves"] >= 12, r
        assert r["03_blink_only"] == 0, r
        assert r["04_only_a_blink"] == 0, r
    print("slider_shots_check: selftest ok")
    return 0


def main():
    if "--selftest" in sys.argv:
        return selftest()
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--min-px", type=int, default=12)
    ap.add_argument("--expect", type=int, default=11)
    args = ap.parse_args()
    files = {f[:-4]: os.path.join(args.dir, f) for f in os.listdir(args.dir) if f.endswith(".png")}
    names = sorted({m.group(1) for k in files for m in [re.match(r"(\d\d_.+)_min$", k)] if m})
    bad = 0
    size = None
    for n in names:
        need = [n + "_min", n + "_min_b", n + "_max", n + "_max_b", n + "_reset"]
        miss = [k for k in need if k not in files]
        if miss:
            print("FAIL  %-18s missing shots %s" % (n, ", ".join(miss)))
            bad += 1
            continue
        pics = {k: load(files[k]) for k in need}
        size = pics[need[0]][1]
        lo, lo_b, hi, hi_b, rs = (pics[k][0] for k in need)
        # the preview breathes, blinks and sparkles: a pixel that moved between a shot and its repeat is
        # animation, not the slider, so only the pixels that held still in BOTH repeated pairs count
        moving = differ(lo, lo_b) | differ(hi, hi_b)
        noise = int(moving.sum())
        d = int((differ(lo, hi) & ~moving).sum())
        back = int((differ(lo, rs) & ~moving).sum())
        need_px = args.min_px
        ok = d >= need_px and back >= 1
        print("%s  %-18s lowest vs highest %5d px (animation %d px left out, needs %d), reset differs from lowest by %d px"
              % ("ok  " if ok else "FAIL", n, d, noise, need_px, back))
        if not ok:
            bad += 1
    if len(names) < args.expect:
        print("FAIL  only %d sliders found, expected %d" % (len(names), args.expect))
        bad += 1
    bodies = sorted(k for k in files if re.match(r"body_\d\d$", k))
    for a, b in zip(bodies, bodies[1:]):
        d = diff(load(files[a])[0], load(files[b])[0])
        if d < args.min_px:
            print("FAIL  %s -> %s: the next body looks the same (%d px)" % (a, b, d))
            bad += 1
    print("sliders: %d checked, %d bodies stepped, %d failed%s"
          % (len(names), len(bodies), bad, "" if size is None else " (screen %dx%d)" % size))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
