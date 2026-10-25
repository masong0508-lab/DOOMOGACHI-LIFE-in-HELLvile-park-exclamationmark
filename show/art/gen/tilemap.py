"""tilemap.py - cut a plate into 16x16 tiles for a TILEMAP plane (spec 8.3).

The plate is drawn whole (so it can be previewed and diffed as a picture);
to_tilemap() dedupes identical tiles and records the map. The player packs
the unique tiles into 8bpp pages (256 tiles per page) and draws the map.

meta['tiles'] = list of (16,16) uint8 arrays (tile 0 is the first tile seen)
meta['map']   = (rows, cols) uint16 array of tile indices
"""
import numpy as np

T = 16


def to_tilemap(cv):
    h, w = cv.h, cv.w
    if h % T or w % T:
        raise ValueError("tilemap plates must be multiples of 16 (got %dx%d)" % (w, h))
    rows, cols = h // T, w // T
    seen = {}
    tiles = []
    m = np.zeros((rows, cols), np.uint16)
    for r in range(rows):
        for c in range(cols):
            t = cv.a[r * T:(r + 1) * T, c * T:(c + 1) * T]
            key = t.tobytes()
            if key not in seen:
                seen[key] = len(tiles)
                tiles.append(t.copy())
            m[r, c] = seen[key]
    cv.meta["tiles"] = tiles
    cv.meta["map"] = m
    return cv


def from_tilemap(tiles, m):
    rows, cols = m.shape
    a = np.zeros((rows * T, cols * T), np.uint8)
    for r in range(rows):
        for c in range(cols):
            a[r * T:(r + 1) * T, c * T:(c + 1) * T] = tiles[int(m[r, c])]
    return a
