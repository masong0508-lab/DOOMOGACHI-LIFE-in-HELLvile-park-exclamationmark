#!/usr/bin/env python3
"""Identify GBA 3D image resolution from pixel-aligned emulator screenshots.

Report only: no ROM runtime costs or third-party dependencies. Uniform 4x4
blocks are an approximate blockiness indicator; horizontal colour-transition
alignment also distinguishes the Bone-only 96x64 and 80x54 modes. Neither
metric alone certifies artistic quality, view stability or actual drawn FPS.
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


# Source view has two identical pixels per ray column. The upscalers
# produce aligned 2, 4, 5, 6 or 8 pixel ray runs in the final 240x160
# screenshot. Study colour-change *boundaries*, not average colour or
# texture uniformity; repeated wall patterns can fool block counts.
# Only the central game view is sampled (not the unscaled HUD at the bottom).
_RAY_TIERS = (
    (8, "60x40 quarter"),
    (6, "80x54 emergency"),
    (5, "96x64 middle"),
    (4, "120x80 half"),
    (2, "240x160 full"),
)


def ray_tier(path):
    """Return (likely tier, alignment, transition count), not an FPS gate."""
    w, h, bpp, rows = read_rgb(path)
    if (w, h) != (240, 160):
        raise ValueError("expected native 240x160 screenshot")
    aligned = {width: 0 for width, _ in _RAY_TIERS}
    changes = 0
    # Avoid HUD, scene caption, ammo widget, and the screen's left margin.
    for y in range(32, 100, 2):
        row = rows[y]
        for x in range(61, 225):
            at = x * bpp
            if row[at:at + 3] == row[at - bpp:at - bpp + 3]:
                continue
            changes += 1
            for width in aligned:
                if x % width == 0:
                    aligned[width] += 1
    if changes < 64:
        return "indeterminate", 0.0, changes
    # Larger multiples also align at smaller divisors: inspect widest
    # qualifying ray pitch first so an 8-pixel fallback is not mistaken
    # for a 4-pixel image.
    for width, name in _RAY_TIERS:
        ratio = aligned[width] / changes
        if ratio >= 0.82:
            return name, ratio, changes
    return "mixed/unclassified", max(aligned.values()) / changes, changes


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
        "Ray-tier inference uses horizontally aligned colour transitions in the",
        "center of the 3D viewport. It is a heuristic, not a runtime flag.",
        "",
        "| Scene | Uniform 4x4 blocks | Ratio | Likely source tier | Edge alignment |",
        "|---|---:|---:|---|---:|",
    ]
    for p in images:
        n, total = count_uniform_blocks(p)
        tier, alignment, changes = ray_tier(p)
        out.append(
            f"| {p.stem.removeprefix('census_')} | {n}/{total} | "
            f"{n / total:.1%} | {tier} | {alignment:.1%} ({changes} edges) |"
        )
    result = "\n".join(out) + "\n"
    print(result, end="")
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(result, encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
