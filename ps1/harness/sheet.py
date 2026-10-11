#!/usr/bin/env python3
"""sheet.py - glue harness screenshots into one contact sheet PNG.

usage: sheet.py OUT.png IN1.png IN2.png ... [--cols N]
Stdlib only (reads/writes the 8-bit RGB PNGs that run.py produces).
"""
import struct
import sys
import zlib


def read_png(path):
    d = open(path, "rb").read()
    pos, w, h, idat = 8, 0, 0, b""
    while pos < len(d):
        n, = struct.unpack(">I", d[pos:pos + 4])
        t, c = d[pos + 4:pos + 8], d[pos + 8:pos + 8 + n]
        pos += 12 + n
        if t == b"IHDR":
            w, h = struct.unpack(">II", c[:8])
        elif t == b"IDAT":
            idat += c
    raw = zlib.decompress(idat)
    st = w * 3
    rows = []
    for y in range(h):
        if raw[y * (st + 1)] != 0:
            raise SystemExit("%s: only unfiltered PNGs (run.py output) supported" % path)
        rows.append(bytearray(raw[y * (st + 1) + 1:(y + 1) * (st + 1)]))
    return w, h, rows


def write_png(path, w, h, rows):
    raw = b"".join(b"\0" + bytes(r) for r in rows)

    def ch(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                ch(b"IDAT", zlib.compress(raw, 6)) + ch(b"IEND", b""))


def main(argv):
    cols = 3
    if "--cols" in argv:
        i = argv.index("--cols")
        cols = int(argv[i + 1])
        del argv[i:i + 2]
    if len(argv) < 3:
        print(__doc__)
        return 2
    out, files = argv[1], argv[2:]
    imgs = [read_png(f) for f in files]
    W = max(i[0] for i in imgs)
    H = max(i[1] for i in imgs)
    nrows = (len(imgs) + cols - 1) // cols
    sheet = [bytearray(W * cols * 3) for _ in range(H * nrows)]
    for k, (w, h, rows) in enumerate(imgs):
        cx, cy = (k % cols) * W, (k // cols) * H
        for y in range(h):
            sheet[cy + y][cx * 3:cx * 3 + w * 3] = rows[y]
    write_png(out, W * cols, H * nrows, sheet)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
