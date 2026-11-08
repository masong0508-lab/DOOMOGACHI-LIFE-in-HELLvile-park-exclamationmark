#!/usr/bin/env python3
"""compare_house.py - DC8: Flycast shots of the house scripts against the
host build of the Dreamcast layers and against the native runner (owner:
dreamcast).

    python3 -I compare_house.py DCDIR HOSTDIR NATDIR [--gate 99] [--close-gate 99.9]

DCDIR holds dcrun's shots (NAME.png, 640x480), HOSTDIR the host runner's
pictures of the same ticks (NAME_dc.png: dc/src/room.c and the 2D layer
through the software PVR, 640x480), NATDIR the native runner's (NAME.png,
320x240: room_ref.c, the reference). Every pair is compared in RGB565, the
console frame buffer's precision:

  Flycast vs host  GATED: both draw the same prims; exact >= --gate % and
                   within 24/255 >= --close-gate % (the PVR's and the
                   software blend round GLASS / ADD pixels a level apart)
  Flycast vs native  REPORTED, not gated: every logical pixel's 2x2 block
                   against the native pixel; the expected differences are
                   the Dreamcast's own (translucent ghosts and residents,
                   the selected pieces' glow, squashes as one quad, the
                   title's DC label)

A Flycast shot without a host twin fails (MISSING). Prints one line per
shot and a summary; exit 1 on a fail.
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image


def rgb565(path):
    a = np.asarray(Image.open(path).convert('RGB')).astype(int)
    return np.stack([a[..., 0] >> 3, a[..., 1] >> 2, a[..., 2] >> 3], axis=2) * np.array([8, 4, 8])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dcdir')
    ap.add_argument('hostdir')
    ap.add_argument('natdir')
    ap.add_argument('--gate', type=float, default=99.0)
    ap.add_argument('--close-gate', type=float, default=99.9)
    a = ap.parse_args()
    names = sorted(f[:-4] for f in os.listdir(a.dcdir) if f.endswith('.png'))
    fails = 0
    worst = (101.0, '')
    nat_sum, nat_n, nat_exact_shots = 0.0, 0, 0
    for n in names:
        d = rgb565(os.path.join(a.dcdir, n + '.png'))
        hp = os.path.join(a.hostdir, n + '_dc.png')
        if not os.path.exists(hp):
            print('MISSING %s (no host shot)' % n)
            fails += 1
            continue
        h = rgb565(hp)
        if d.shape != h.shape:
            print('FAIL %s: size %s vs %s' % (n, d.shape, h.shape))
            fails += 1
            continue
        e = np.abs(d - h).max(axis=2)
        exact = 100.0 * float((e == 0).mean())
        close = 100.0 * float((e <= 24).mean())
        tag = 'ok' if exact >= a.gate and close >= a.close_gate else 'FAIL'
        if tag == 'FAIL':
            fails += 1
        if exact < worst[0]:
            worst = (exact, n)
        natinfo = ''
        np_ = os.path.join(a.natdir, n + '.png')
        if os.path.exists(np_):
            nat = rgb565(np_)
            if nat.shape[0] * 2 == d.shape[0] and nat.shape[1] * 2 == d.shape[1]:
                eq = np.ones(nat.shape[:2], dtype=bool)
                for dy in (0, 1):
                    for dx in (0, 1):
                        eq &= (d[dy::2, dx::2] == nat).all(axis=2)
                ne = 100.0 * float(eq.mean())
                nat_sum += ne
                nat_n += 1
                nat_exact_shots += ne == 100.0
                natinfo = '  native %7.3f%%' % ne
        print('%-4s %-28s host exact %7.3f%% close %7.3f%%%s' % (tag, n, exact, close, natinfo))
    print('compare_house: %d shots, %d failed; worst host exact %.3f%% (%s); against native: mean %.3f%%, '
          '%d of %d identical' % (len(names), fails, worst[0] if names else 100.0, worst[1],
                                  nat_sum / nat_n if nat_n else 0.0, nat_exact_shots, nat_n))
    return 1 if fails or not names else 0


if __name__ == '__main__':
    sys.exit(main())
