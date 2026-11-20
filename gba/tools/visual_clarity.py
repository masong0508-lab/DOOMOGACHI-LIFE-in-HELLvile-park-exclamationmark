#!/usr/bin/env python3
"""Measure uniform 4x4 blocks in GBA census PNG screenshots (report-only).

No dependencies outside the Python standard library, no ROM runtime costs.
A high ratio is consistent with quarter-resolution 60x40 image magnification,
but this metric alone cannot establish artistic or geometric fidelity.
"""
import argparse
from pathlib import Path
import struct
import sys
import zlib

PNG_MAGIC = b"\x89PNG\r\n\x1a\n"


def read_rgb(path):
    blob = path.read_bytes()
    if not blob.startswith(PNG_MAGIC):
        raise ValueError("not a PNG")
    offset, chunks, dims = 8, [], None
    while offset + 12 <= len(blob):
        size = struct.unpack_from(">I", blob, offset)[0]
        tag = blob[offset + 4:offset + 8]
        data = blob[offset + 8:offset + 8 + size]
        if len(data) != size:
            raise ValueError("truncated PNG")
        offset += size + 12
        if tag == b"IHDR":
            w, h, depth, kind, cm, fm, interlace = struct.unpack(">IIBBBBB", data)
            if depth != 8 or kind not in (2, 6) or interlace:
                raise ValueError("expected 8-bit RGB or RGBA non-interlaced PNG")
            dims = (w, h, 3 if kind == 2 else 4)
        elif tag == b"IDAT":
            chunks.append(data)
        elif tag == b"IEND":
            break
    if not dims:
        raise ValueError("no IHDR")
    w, h, bpp = dims
    stride = w * bpp
    payload = zlib.decompress(b"".join(chunks))
    if len(payload) != h * (stride + 1):
        raise ValueError("unexpected decompressed length")
    rows, previous, pos = [], bytearray(stride), 0
    for _ in range(h):
        filt = payload[pos]
        row = bytearray(payload[pos + 1:pos + stride + 1])
        pos += stride + 1
        if filt not in range(5):
            raise ValueError("unsupported PNG filter")
        for i in range(stride):
            left = row[i - bpp] if i >= bpp else 0
            up = previous[i]
            upper_left = previous[i - bpp] if i >= bpp else 0
            if filt == 1:
                row[i] = (row[i] + left) & 255
            elif filt == 2:
                row[i] = (row[i] + up) & 255
            elif filt == 3:
                row[i] = (row[i] + (left + up) // 2) & 255
            elif filt == 4:
                predictor = left + up - upper_left
                a, b, c = (abs(predictor - left), abs(predictor - up),
                           abs(predictor - upper_left))
                choice = left if a <= b and a <= c else up if b <= c else upper_left
                row[i] = (row[i] + choice) & 255
        rows.append(row)
        previous = row
    return w, h, bpp, rows


def count_uniform_blocks(path):
    w, h, bpp, rows = read_rgb(path)
    if (w, h) != (240, 160):
        raise ValueError("expected native 240x160 screenshot")
    total = uniform = 0
    # Exclude the full-resolution bottom HUD for a fair 3D view comparison.
    for y in range(0, 108, 4):
        for x in range(0, 240, 4):
            color = rows[y][x * bpp:x * bpp + 3]
            total += 1
            if all(rows[y + dy][(x + dx) * bpp:(x + dx) * bpp + 3] == color
                   for dy in range(4) for dx in range(4)):
                uniform += 1
    return uniform, total


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shots", type=Path, help="directory containing census_*.png")
    parser.add_argument("--out", type=Path, help="optional Markdown report file")
    args = parser.parse_args()
    images = sorted(args.shots.glob("census_*.png"))
    if not images:
        parser.error("no census screenshots to inspect")
    out = [
        "### GBA screenshot blockiness census (report only)", "",
        "Fraction of uniform 4x4 blocks in top 108 rows; lower usually means finer detail.",
        "Compare identical scenes and ticks. This is not an artistic quality score.", "",
        "| Scene | Uniform blocks | Ratio |", "|---|---:|---:|",
    ]
    for p in images:
        n, total = count_uniform_blocks(p)
        out.append(f"| {p.stem.removeprefix('census_')} | {n}/{total} | {n / total:.1%} |")
    result = "\n".join(out) + "\n"
    print(result, end="")
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(result, encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
