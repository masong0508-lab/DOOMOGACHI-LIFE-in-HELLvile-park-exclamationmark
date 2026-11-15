#!/usr/bin/env python3
"""cardgone.py - S2-10: the busy card is up during a save and gone after it (gba/harness/cardgone.sh).

usage: cardgone.py MID.png AFTER.png ...
The GBA's "Saving..." / "Loading..." card (loadfade.c: CARD_W x CARD_H, centered) has a one-colour border; the
island hub's picture never has a 144 x 30 frame of one colour there. MID (a shot in the middle of the save) must show
it, every AFTER shot (one per tick after the save) must not. Exit 0: as it should be; 1: a card left in a picture
after the save; 3: no card in MID (the save ran in a tick that builds the next frame, which gets the fade only: the
caller tries another tick). Stdlib only.
"""
import struct
import sys
import zlib

SCREEN_W, SCREEN_H, CARD_W, CARD_H = 240, 160, 144, 30
X0, Y0 = (SCREEN_W - CARD_W) // 2, (SCREEN_H - CARD_H) // 2


def read_png(path):
    d = open(path, "rb").read()
    pos, w, h, ct, idat = 8, 0, 0, 2, b""
    while pos < len(d):
        n, = struct.unpack(">I", d[pos:pos + 4])
        t, c = d[pos + 4:pos + 8], d[pos + 8:pos + 8 + n]
        pos += 12 + n
        if t == b"IHDR":
            w, h, _, ct = struct.unpack(">IIBB", c[:10])
        elif t == b"IDAT":
            idat += c
    bpp = 4 if ct == 6 else 3
    raw, st, rows, prev, p = zlib.decompress(idat), w * bpp, [], bytearray(w * bpp), 0
    for _ in range(h):
        f, line = raw[p], bytearray(raw[p + 1:p + 1 + st])
        p += 1 + st
        for x in range(st):
            a = line[x - bpp] if x >= bpp else 0
            b, c = prev[x], (prev[x - bpp] if x >= bpp else 0)
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        rows.append(bytes(line))
        prev = line
    return [[tuple(r[x * bpp:x * bpp + 3]) for x in range(w)] for r in rows]


def card_border(path):
    """1 when the card's outline is in the picture: its four edges are one colour"""
    px = read_png(path)
    edge = [px[Y0][x] for x in range(X0, X0 + CARD_W)] + [px[Y0 + CARD_H - 1][x] for x in range(X0, X0 + CARD_W)]
    edge += [px[y][X0] for y in range(Y0, Y0 + CARD_H)] + [px[y][X0 + CARD_W - 1] for y in range(Y0, Y0 + CARD_H)]
    return len(set(edge)) == 1


def main(argv):
    if len(argv) < 2:
        print("usage: cardgone.py MID.png AFTER.png ...")
        return 2
    if not card_border(argv[0]):
        print("cardgone: no card in %s (the save ran on a building tick)" % argv[0])
        return 3
    bad = [p for p in argv[1:] if card_border(p)]
    for p in bad:
        print("cardgone: the busy card is still in %s" % p)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
