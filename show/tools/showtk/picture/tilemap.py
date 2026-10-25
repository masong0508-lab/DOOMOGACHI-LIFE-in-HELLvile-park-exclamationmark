"""tilemap.py - a plate -> a TILEMAP payload (show_tilemap.h): its unique 16x16
tiles packed 49 to a 128x128 atlas page with 1-texel repeated gutters, and the
map. A tile of all index 0 (transparent) becomes an empty cell.

    plane <name> tilemap=gen:<plate>(...) [par=P] [at=X,Y]     (set file)

The bake accepts a plate whose width or height is not a multiple of 16 by
padding it with transparent pixels on the right and bottom.
"""
import numpy as np

from . import fmt
from .fmt import TM_TILE as T, TM_ATLAS, TM_CELL, TM_PER_ROW, TM_PER_ATLAS, TM_EMPTY

MAX_ATLAS = 12


class TilemapError(ValueError):
    pass


def split(a):
    """-> (tiles: list of (16,16) uint8, map: (rows, cols) uint16)."""
    a = np.asarray(a, np.uint8)
    h, w = a.shape
    H, W = -(-h // T) * T, -(-w // T) * T
    if (H, W) != (h, w):
        b = np.zeros((H, W), np.uint8)
        b[:h, :w] = a
        a = b
    rows, cols = H // T, W // T
    seen, tiles = {}, []
    m = np.full((rows, cols), TM_EMPTY, np.uint16)
    for r in range(rows):
        for c in range(cols):
            t = a[r * T:(r + 1) * T, c * T:(c + 1) * T]
            if not t.any():
                continue
            k = t.tobytes()
            if k not in seen:
                seen[k] = len(tiles)
                tiles.append(t.copy())
            m[r, c] = seen[k]
    return tiles, m


def atlases(tiles):
    n = -(-len(tiles) // TM_PER_ATLAS)
    if n > MAX_ATLAS:
        raise TilemapError("%d unique tiles need %d atlas pages; a plane holds %d (%d tiles)"
                           % (len(tiles), n, MAX_ATLAS, MAX_ATLAS * TM_PER_ATLAS))
    out = np.zeros((max(n, 0), TM_ATLAS, TM_ATLAS), np.uint8)
    for k, t in enumerate(tiles):
        a, c = divmod(k, TM_PER_ATLAS)
        u, v = TM_CELL * (c % TM_PER_ROW), TM_CELL * (c // TM_PER_ROW)
        cell = np.pad(t, 1, mode="edge")          # the gutter repeats the edge
        out[a, v:v + TM_CELL, u:u + TM_CELL] = cell
    return out


def compile_plane(a, plane_id=1):
    tiles, m = split(a)
    at = atlases(tiles)
    rows, cols = m.shape
    hdr = fmt.TILEMAP.pack(cols, rows, len(tiles), at.shape[0], T, plane_id & 0xFFFFFFFF, 0, 0)
    body = hdr + at.tobytes() + m.astype("<u2").tobytes()
    return fmt.pad4(body), {"tiles": len(tiles), "atlases": at.shape[0], "cols": cols, "rows": rows,
                            "bytes": len(body)}


def unpack(payload):
    """payload -> plate array (the inverse, for tests)."""
    cols, rows, ntiles, natlas, tile, pid, flags, pad = fmt.TILEMAP.unpack_from(payload, 0)
    o = fmt.TILEMAP.size
    at = np.frombuffer(payload, np.uint8, natlas * TM_ATLAS * TM_ATLAS, o).reshape(natlas, TM_ATLAS, TM_ATLAS)
    o += natlas * TM_ATLAS * TM_ATLAS
    m = np.frombuffer(payload, "<u2", rows * cols, o).reshape(rows, cols)
    out = np.zeros((rows * T, cols * T), np.uint8)
    for r in range(rows):
        for c in range(cols):
            k = int(m[r, c])
            if k == TM_EMPTY:
                continue
            a, cc = divmod(k, TM_PER_ATLAS)
            u, v = 1 + TM_CELL * (cc % TM_PER_ROW), 1 + TM_CELL * (cc // TM_PER_ROW)
            out[r * T:(r + 1) * T, c * T:(c + 1) * T] = at[a, v:v + T, u:u + T]
    return out
