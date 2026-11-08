#!/usr/bin/env python3
"""sheet.py - DC7: a contact sheet of overworld shots (owner: dreamcast).

    python3 -I sheet.py OUT.png DIR... [--names a,b,c] [--cols 2]

Each DIR is one script's results of run_flycast.sh (DIR/dc/NAME.png from
Flycast, DIR/nat/NAME.png from the native runner). Every shot (or the
named ones) becomes a row pair: the Dreamcast's 640x480 picture beside the
native top-down reference at 2x, captioned with the script and the shot.
"""
import argparse
import os
import sys

from PIL import Image, ImageDraw


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('dirs', nargs='+')
    ap.add_argument('--names', default='')
    ap.add_argument('--max', type=int, default=24)
    a = ap.parse_args()
    want = set(x for x in a.names.split(',') if x)
    tiles = []
    for d in a.dirs:
        dcd, natd = os.path.join(d, 'dc'), os.path.join(d, 'nat')
        for f in sorted(os.listdir(dcd)):
            n = f[:-4]
            if not f.endswith('.png') or (want and n not in want):
                continue
            tiles.append((os.path.basename(d.rstrip('/')), n, os.path.join(dcd, f), os.path.join(natd, f)))
    tiles = tiles[:a.max]
    if not tiles:
        print('sheet: no shots')
        return 1
    w, h, cap = 640, 480, 18
    sheet = Image.new('RGB', (2 * w + 12, len(tiles) * (h + cap + 6)), (24, 24, 32))
    dr = ImageDraw.Draw(sheet)
    for i, (s, n, dp, npth) in enumerate(tiles):
        y = i * (h + cap + 6)
        dr.text((4, y + 3), '%s / %s   (left: Dreamcast on Flycast, right: native top-down reference)' % (s, n),
                fill=(230, 230, 230))
        sheet.paste(Image.open(dp).convert('RGB').resize((w, h)), (0, y + cap))
        if os.path.exists(npth):
            sheet.paste(Image.open(npth).convert('RGB').resize((w, h), Image.NEAREST), (w + 12, y + cap))
    sheet.save(a.out)
    print('sheet: %d shots -> %s' % (len(tiles), a.out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
