#!/usr/bin/env python3
# doomo-generator
"""Bake the GBA Bone materials and sky into indexed ROM pixels."""
import argparse
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parent.parent

# The larger motifs keep eyes, mortar and window leading visible at 48 rays.
SKULL = (
    '   hhhhhhh   ',
    '  hhhhhhhhm  ',
    ' hhhhhhhmmms ',
    ' hhhhmmmmmsd ',
    ' hdddmmdddsd ',
    ' mdddmmdddsd ',
    ' mdddmsdddsd ',
    ' mmmmmddsssd ',
    '  mmmssssdd  ',
    '   smmssdd   ',
    '   smdmdsd   ',
    '   smmmmdd   ',
    '    ddddd    ',
)
SKULL_COLORS = {'h': ('BONE', 6), 'm': ('BONE', 5), 's': ('BONE', 3), 'd': ('WOOD', 1)}


def bone_wall(x, y):
    course = y // 16
    u, v = (x + 8 * (course & 1)) % 16, y % 16
    if v == 0 or u == 0:
        return 'WOOD', 1
    if v == 1 or u == 1:
        return 'BONE', 3
    if u >= 2 and v >= 2 and u < 15 and v < 15:
        ink = SKULL[v - 2][u - 2]
        if ink != ' ':
            return SKULL_COLORS[ink]
    return 'WOOD', 4 if v < 9 else 3


def carved_mosaic(x, y):
    border = min(x, 31 - x, y, 31 - y)
    if border < 4:
        return 'BONE', (1, 5, 4, 1)[border]
    u, v = (x - 3) // 2, (y - 3) // 2
    if 0 <= u < 13 and 0 <= v < 13:
        ink = SKULL[v][u]
        if ink != ' ':
            return ('BONE', 7) if ink == 'h' else SKULL_COLORS[ink]
    return 'WOOD', 2 if y < 20 else 3


def stained_glass(x, y):
    u, v = min(x, 31 - x), min(y, 31 - y)
    border = min(u, v)
    if border < 3:
        return 'BONE', (1, 4, 2)[border]
    # The rosette has thick common lead lines and a small amber centre.
    d = abs(2 * x - 31) + abs(2 * y - 31)
    if d <= 10:
        return 'CANDY', 6 if y < 16 else 5
    if d <= 14 or u == 15 or v == 15:
        return 'WOOD', 1
    if d <= 28:
        return ('VAPE', 6) if (x < 16) == (y < 16) else ('SKY', 5)
    return 'VAPE', 4 if (x + y) % 32 < 16 else 5


def brick(x, y):
    u, v = (x + 8 * (y // 8)) % 16, y % 8
    if u == 0 or v == 0:
        return 'BLOOD', 1
    return 'BLOOD', 4 if v <= 2 else 3 if v < 7 else 2


def red_paving(x, y):
    u, v = (x + 8 * (y // 8)) % 16, y % 8
    if v == 0 or u == 0:
        return 'BLOOD', 2
    if v == 1 or u == 1:
        return 'BLOOD', 4
    return 'BLOOD', 3 + ((x // 7 + y // 5) % 5 == 0)


def coffered_vault(x, y):
    # One recessed panel per tile. Long ribs survive oblique views,
    # where the small skull relief used to turn into a checkerboard.
    u, v = min(x, 31 - x), min(y, 31 - y)
    rim = min(u, v, (u + v - 3) // 2)
    if rim < 2:
        return 'WOOD', 3
    if rim < 4:
        return 'WOOD', 5
    if rim < 6:
        return 'WOOD', 2
    return 'WOOD', 3 if y < 19 else 2


def bone_slab(x, y):
    # The horizontal side of the same stone has broad joints and a
    # worn face. The vertical side keeps the original carved reliefs.
    u, v = (x + 8 * (y // 16)) % 32, y % 16
    if u < 2 or v < 2:
        return 'WOOD', 2
    if u < 4 or v < 4:
        return 'WOOD', 5
    return 'WOOD', 4 if v < 13 and u < 29 else 3


def chapel_flagstone(x, y):
    # Terracotta keeps the red route visible beside the neutral steps.
    u, v = (x + 16 * (y // 16)) % 32, y % 16
    if u < 2 or v < 2:
        return 'BLOOD', 2
    if u < 4 or v < 4:
        return 'BLOOD', 5
    return 'BLOOD', 4 if v < 13 and u < 29 else 3


def distant_stone_palette():
    source = (ROOT / 'core/src/palette.c').read_text()
    result = {}
    for ramp in ('WOOD', 'BONE'):
        match = re.search(r'^ +/\* ' + ramp + r'\b[^\n]*\*/\n([^/]+)', source, re.MULTILINE)
        colors = re.findall(r'C\(\s*(\d+),\s*(\d+),\s*(\d+)\)', match[1] if match else '')
        if len(colors) != 8:
            raise ValueError(f'Expected eight colors in the {ramp} ramp')
        result.update(((ramp, index), tuple(map(int, rgb))) for index, rgb in enumerate(colors))
    return result


def distant_stone(tile, x, y, palette):
    # One sample covers four world texels in either direction. Average
    # in RGB and quantize back to a color from the two stone ramps.
    samples = [palette[tile(2 * x + dx, 2 * y + dy)] for dy in range(2) for dx in range(2)]
    total = tuple(sum(channel) for channel in zip(*samples))
    return min(palette, key=lambda swatch: sum((4 * c - target) ** 2
                                              for c, target in zip(palette[swatch], total)))


def sky(x, y):
    bank = abs((x + 9) % 64 - 32) // 5
    ridge = 40 - abs((x + 3) % 37 - 18) // 4
    near_ridge = 44 - abs((x + 15) % 32 - 16) // 5
    if y >= near_ridge:
        return 'STONE', 0
    if y >= ridge:
        return 'STONE', 2 if y == ridge else 1
    # A panoramic sky texel takes up more screen width than height.
    # Draw a narrow ellipse in the atlas so the eclipse reads as round.
    sun = (5 * (x - 96)) ** 2 + (y - 25) ** 2
    if sun < 28:
        return 'VAPE', 0
    if sun < 45:
        return 'BLOOD', 4
    if y >= 31 + bank:
        return 'PEACH', 1
    if y >= 24 + bank:
        return 'BLOOD', 2
    if y > 17 - bank:
        return 'VAPE', 2
    return 'VAPE', 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out-root', type=Path, default=ROOT)
    args = parser.parse_args()
    tiles = (bone_wall, carved_mosaic, stained_glass, brick, red_paving,
             coffered_vault, bone_slab, chapel_flagstone)
    lines = ['/* Generated by tools/gen_gba_bone_materials.py. */',
             '#define BONE_P(r, s) PAL_RAMP(RAMP_##r, s), PAL_RAMP(RAMP_##r, s)',
             '#define BONE_ROW(...) __VA_ARGS__, __VA_ARGS__,',
             '#define BONE_MIP_ROW(...) BONE_ROW(__VA_ARGS__) BONE_ROW(__VA_ARGS__)']
    for tile in tiles:
        lines.append(f'ALIGN4 static const uint8_t {tile.__name__}_pixels[DOOMO_TEX_SIZE_PX] = {{')
        for y in range(32):
            row = ','.join(f'BONE_P({r},{s})' for r, s in (tile(x, y) for x in range(32)))
            lines.append(f'    BONE_ROW({row})')
        lines.append('};')
    palette = distant_stone_palette()
    for tile in (bone_wall, carved_mosaic):
        lines.append(f'ALIGN4 static const uint8_t {tile.__name__}_far[DOOMO_TEX_SIZE_PX] = {{')
        for y in range(16):
            row = ','.join(f'BONE_P({r},{s}), BONE_P({r},{s})' for r, s in
                            (distant_stone(tile, x, y, palette) for x in range(16)))
            lines.append(f'    BONE_MIP_ROW({row})')
        lines.append('};')
    lines.append('ALIGN4 static const uint8_t bone_sky_pixels[256 * 96] = {')
    for y in range(48):
        row = [f'BONE_P({r},{s})' for r, s in (sky(x, y) for x in range(128))]
        chunks = [','.join(row[i:i + 16]) for i in range(0, len(row), 16)]
        lines.extend(['    BONE_ROW(' + ',\n             '.join(chunks) + ')'])
    lines += ['};', '#undef BONE_MIP_ROW', '#undef BONE_ROW', '#undef BONE_P']
    out = args.out_root / 'gba/src/bone_materials.inc'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
