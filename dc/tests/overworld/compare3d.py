#!/usr/bin/env python3
"""compare3d.py - DC7: Flycast shots against the host software PVR (owner:
dreamcast).

    python3 -I compare3d.py DCDIR HOSTDIR [--report HOSTDIR/report.txt]

DCDIR holds dcrun's shots (NAME.png, 640x480); HOSTDIR the host runner's
(NAME_dc.png, the Dreamcast layers' frame through dc/tests/gfx2d/swpvr.c,
640x480) and its DC3_REPORT, whose `v3d N` says how many 3D views (DC7
markers) a shot's frame had. Both are compared in RGB565, the frame
buffer's precision. Every shot with a 3D view must have at least 97% of
its pixels within 24/255 on every channel (both sides draw the same
polygons; the PVR's and the software rasteriser's edge rules, Gouraud and
colour rounding differ by a few levels and a pixel along edges). Shots
without a view are listed, not gated (the 2D is DC3's ui-compare; a combat
view is DC6's). Prints one line per shot and a summary; exit 1 on a fail.
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image

GATE = 97.0
TOL = 24


def views(report):
    out = {}
    if not report or not os.path.exists(report):
        return out
    for line in open(report):
        w = line.split()
        if len(w) > 2 and w[0] == 'shot' and 'v3d' in w:
            name = os.path.basename(w[1])
            if name.endswith('.png'):
                name = name[:-4]
            out[name] = int(w[w.index('v3d') + 1])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dcdir')
    ap.add_argument('hostdir')
    ap.add_argument('--report', default=None)
    a = ap.parse_args()
    v = views(a.report or os.path.join(a.hostdir, 'report.txt'))
    names = sorted(f[:-4] for f in os.listdir(a.dcdir) if f.endswith('.png'))
    gated = fails = 0
    worst = (101.0, '')
    for n in names:
        hp = os.path.join(a.hostdir, n + '_dc.png')
        if not os.path.exists(hp):
            print('MISSING %s (no host shot)' % n)
            continue
        d = np.asarray(Image.open(os.path.join(a.dcdir, n + '.png')).convert('RGB')).astype(int)
        h = np.asarray(Image.open(hp).convert('RGB')).astype(int)
        if d.shape != h.shape:
            print('FAIL %s: size %s vs %s' % (n, d.shape, h.shape))
            fails += 1
            continue
        # both in the RGB565 of the frame buffer (the PNGs carry 8 bits)
        d = np.stack([d[..., 0] >> 3, d[..., 1] >> 2, d[..., 2] >> 3], axis=2) * np.array([8, 4, 8])
        h = np.stack([h[..., 0] >> 3, h[..., 1] >> 2, h[..., 2] >> 3], axis=2) * np.array([8, 4, 8])
        e = np.abs(d - h).max(axis=2)
        close = 100.0 * float((e <= TOL).mean())
        exact = 100.0 * float((e == 0).mean())
        is3d = v.get(n, 0) > 0
        tag = 'ok' if close >= GATE else 'FAIL'
        if not is3d:
            tag = 'nogate'
        else:
            gated += 1
            if close < worst[0]:
                worst = (close, n)
            if tag == 'FAIL':
                fails += 1
        print('%-6s %-28s close %7.3f%% exact %7.3f%% (3D views %d)' % (tag, n, close, exact, v.get(n, 0)))
    print('compare3d: %d shots with a 3D view gated, %d failed; worst %.3f%% (%s)' %
          (gated, fails, worst[0] if gated else 100.0, worst[1]))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
