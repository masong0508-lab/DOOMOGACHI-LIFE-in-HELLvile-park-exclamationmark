#!/usr/bin/env python3
"""memcard_check.py - inspect a PS1 memory card image (.mcd, 128 KB raw).

Lists the files in the directory, decodes each save's title frame
(Shift-JIS title, icon flag, block count) and writes the 16x16 icon
animation frames as one PNG strip (scaled x4) so they can be looked at.

usage: memcard_check.py card.mcd [--icons out.png] [--expect NAME ...]
(NAME may be a glob: BASLUS-DOOMO1*)
Exit code 1 if an --expect'ed file is missing or a DOOMOGACHI save header
is malformed. Stdlib only.
"""
import fnmatch
import struct
import sys
import zlib

FRAME = 128
BLOCK = 8192


def sjis_to_ascii(b):
    out = []
    i = 0
    while i < len(b):
        c = b[i]
        if c == 0:
            break
        if 0x81 <= c <= 0x9f or 0xe0 <= c <= 0xef:
            code = (c << 8) | b[i + 1]
            i += 2
            if 0x8260 <= code <= 0x8279: out.append(chr(ord('A') + code - 0x8260))
            elif 0x8281 <= code <= 0x829a: out.append(chr(ord('a') + code - 0x8281))
            elif 0x824f <= code <= 0x8258: out.append(chr(ord('0') + code - 0x824f))
            elif code == 0x8140: out.append(' ')
            else:
                table = {0x8149: '!', 0x817c: '-', 0x8144: '.', 0x8146: ':', 0x8169: '(',
                         0x816a: ')', 0x8148: '?', 0x8195: '&', 0x815e: '/'}
                out.append(table.get(code, '?'))
        else:
            out.append(chr(c) if 32 <= c < 127 else '?')
            i += 1
    return "".join(out)


def write_png(path, w, h, rgb):
    def ch(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    raw = b"".join(b"\0" + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                ch(b"IDAT", zlib.compress(raw, 6)) + ch(b"IEND", b""))


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    data = open(argv[1], "rb").read()
    icons_out = argv[argv.index("--icons") + 1] if "--icons" in argv else None
    expect = []
    if "--expect" in argv:
        expect = argv[argv.index("--expect") + 1:]
    if data[:2] != b"MC":
        print("memcard_check: not a formatted memory card image")
        return 1
    found, bad, strips = [], 0, []
    for blk in range(1, 16):
        d = data[blk * FRAME:(blk + 1) * FRAME]
        state, size, nxt = struct.unpack("<IIh", d[:10])
        name = d[10:30].split(b"\0")[0].decode("latin-1")
        if state != 0x51:
            continue
        hdr = data[blk * BLOCK:blk * BLOCK + 5 * FRAME]
        ok = hdr[:2] == b"SC"
        title = sjis_to_ascii(hdr[4:0x44]) if ok else "?"
        frames = hdr[2] & 0x0f if ok else 0
        print("block %2d  %-20s %6d bytes (%d blocks)  icon frames %d  title '%s'%s" %
              (blk, name, size, size // BLOCK, frames, title,
               "" if ok else "  <- no SC header"))
        found.append(name)
        if "DOOMO" in name:
            info = struct.unpack("<IIII", hdr[4 * FRAME:4 * FRAME + 16])
            if not ok or info[0] != 0x53504d44 or frames != 3:
                print("  malformed DOOMOGACHI save header")
                bad += 1
            else:
                print("  payload %d bytes, crc %08x" % (info[1], info[2]))
        if ok and icons_out:
            clut = struct.unpack("<16H", hdr[0x60:0x80])
            pal = []
            for c in clut:
                r, g, b = c & 31, (c >> 5) & 31, (c >> 10) & 31
                pal.append(((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2)))
            for f in range(max(1, frames)):
                px = hdr[FRAME * (1 + f):FRAME * (2 + f)]
                strips.append([pal[(px[y * 8 + x // 2] >> ((x & 1) * 4)) & 15]
                               for y in range(16) for x in range(16)])
    if icons_out and strips:
        sc = 4
        w, h = 16 * sc * len(strips) + 4 * (len(strips) - 1), 16 * sc
        rgb = bytearray([40, 40, 48] * w * h)
        for k, icon in enumerate(strips):
            for y in range(16 * sc):
                for x in range(16 * sc):
                    c = icon[(y // sc) * 16 + x // sc]
                    o = (y * w + k * (16 * sc + 4) + x) * 3
                    rgb[o:o + 3] = bytes(c)
        write_png(icons_out, w, h, rgb)
        print("icons -> %s" % icons_out)
    missing = [e for e in expect if not any(fnmatch.fnmatchcase(f, e) for f in found)]
    for m in missing:
        print("memcard_check: missing %s" % m)
    return 1 if (missing or bad) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
