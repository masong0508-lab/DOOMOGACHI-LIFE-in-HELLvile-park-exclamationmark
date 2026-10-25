"""canvas.py - an indexed (8bpp) pixel canvas with palette-exact primitives.

Everything here writes palette indices; nothing blends. Gradients are palette
bands with an ordered-dither seam (the game's qtone() rule), never a mix, so
every plate is 100 % on-palette by construction (PIC-PAL).
"""
import math
import zlib
import struct

import numpy as np

from . import pal

BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.int32)


def bayer(w, h, ox=0, oy=0):
    ys = (np.arange(h) + oy) & 3
    xs = (np.arange(w) + ox) & 3
    return BAYER4[ys[:, None], xs[None, :]]


class Rng:
    """Tiny deterministic LCG (no global random state, same on every machine)."""

    def __init__(self, seed):
        self.s = (int(seed) * 2654435761 + 0x9E3779B9) & 0xFFFFFFFF

    def u32(self):
        self.s = (self.s * 1664525 + 1013904223) & 0xFFFFFFFF
        return self.s

    def rand(self):
        return self.u32() / 4294967296.0

    def randint(self, a, b):
        return a + self.u32() % (b - a + 1)

    def uniform(self, a, b):
        return a + (b - a) * self.rand()

    def choice(self, seq):
        return seq[self.u32() % len(seq)]

    def chance(self, p):
        return self.rand() < p


def hash2(x, y, seed=0):
    h = (x * 374761393 + y * 668265263 + seed * 2246822519) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return (h ^ (h >> 16)) & 0xFFFFFFFF


class Canvas:
    def __init__(self, w, h, fill=0):
        self.w, self.h = int(w), int(h)
        self.a = np.full((self.h, self.w), fill, np.uint8)
        self.meta = {}

    # ---- basics -----------------------------------------------------------
    def copy(self):
        c = Canvas(self.w, self.h)
        c.a = self.a.copy()
        c.meta = dict(self.meta)
        return c

    def px(self, x, y, c):
        x, y = int(x), int(y)
        if 0 <= x < self.w and 0 <= y < self.h:
            self.a[y, x] = c

    def get(self, x, y):
        x, y = int(x), int(y)
        if 0 <= x < self.w and 0 <= y < self.h:
            return int(self.a[y, x])
        return 0

    def _clip(self, x, y, w, h):
        x0, y0 = max(0, int(x)), max(0, int(y))
        x1, y1 = min(self.w, int(x) + int(w)), min(self.h, int(y) + int(h))
        return x0, y0, x1, y1

    def rect(self, x, y, w, h, c):
        x0, y0, x1, y1 = self._clip(x, y, w, h)
        if x1 > x0 and y1 > y0:
            self.a[y0:y1, x0:x1] = c

    def frame(self, x, y, w, h, c, t=1):
        self.rect(x, y, w, t, c)
        self.rect(x, y + h - t, w, t, c)
        self.rect(x, y, t, h, c)
        self.rect(x + w - t, y, t, h, c)

    def hline(self, x0, x1, y, c):
        if x1 < x0:
            x0, x1 = x1, x0
        self.rect(x0, y, x1 - x0 + 1, 1, c)

    def vline(self, x, y0, y1, c):
        if y1 < y0:
            y0, y1 = y1, y0
        self.rect(x, y0, 1, y1 - y0 + 1, c)

    def line(self, x0, y0, x1, y1, c, w=1):
        x0, y0, x1, y1 = int(round(x0)), int(round(y0)), int(round(x1)), int(round(y1))
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        err = dx + dy
        while True:
            if w <= 1:
                self.px(x0, y0, c)
            else:
                self.rect(x0 - (w - 1) // 2, y0 - (w - 1) // 2, w, w, c)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def polyline(self, pts, c, w=1, closed=False):
        n = len(pts)
        for i in range(n - (0 if closed else 1)):
            a, b = pts[i], pts[(i + 1) % n]
            self.line(a[0], a[1], b[0], b[1], c, w)

    # ---- masks ------------------------------------------------------------
    def grid(self):
        ys, xs = np.mgrid[0:self.h, 0:self.w]
        return xs, ys

    def fill_mask(self, mask, c):
        self.a[mask] = c

    def ellipse_mask(self, cx, cy, rx, ry):
        xs, ys = self.grid()
        rx, ry = max(rx, 0.5), max(ry, 0.5)
        return ((xs + 0.5 - cx) / rx) ** 2 + ((ys + 0.5 - cy) / ry) ** 2 <= 1.0

    def ellipse(self, cx, cy, rx, ry, c):
        self.a[self.ellipse_mask(cx, cy, rx, ry)] = c

    def poly_mask(self, pts):
        """Even-odd scanline fill mask of a polygon (pixel centres)."""
        m = np.zeros((self.h, self.w), bool)
        pts = [(float(x), float(y)) for x, y in pts]
        n = len(pts)
        ys = [p[1] for p in pts]
        y0, y1 = max(0, int(math.floor(min(ys)))), min(self.h - 1, int(math.ceil(max(ys))))
        for y in range(y0, y1 + 1):
            yc = y + 0.5
            xs = []
            for i in range(n):
                xa, ya = pts[i]
                xb, yb = pts[(i + 1) % n]
                if (ya <= yc < yb) or (yb <= yc < ya):
                    xs.append(xa + (yc - ya) * (xb - xa) / (yb - ya))
            xs.sort()
            for k in range(0, len(xs) - 1, 2):
                a = max(0, int(math.ceil(xs[k] - 0.5)))
                b = min(self.w - 1, int(math.floor(xs[k + 1] - 0.5)))
                if b >= a:
                    m[y, a:b + 1] = True
        return m

    def poly(self, pts, c):
        self.a[self.poly_mask(pts)] = c

    def outline_mask(self, mask, diag=False):
        """Pixels just outside `mask` (4- or 8-connected)."""
        o = np.zeros_like(mask)
        o[1:, :] |= mask[:-1, :]
        o[:-1, :] |= mask[1:, :]
        o[:, 1:] |= mask[:, :-1]
        o[:, :-1] |= mask[:, 1:]
        if diag:
            o[1:, 1:] |= mask[:-1, :-1]
            o[:-1, :-1] |= mask[1:, 1:]
            o[1:, :-1] |= mask[:-1, 1:]
            o[:-1, 1:] |= mask[1:, :-1]
        return o & ~mask

    def inner_edge(self, mask, side="top"):
        """Pixels of `mask` whose neighbour on `side` is outside the mask."""
        e = np.zeros_like(mask)
        if side == "top":
            e[1:, :] = mask[1:, :] & ~mask[:-1, :]
            e[0, :] = mask[0, :]
        elif side == "bottom":
            e[:-1, :] = mask[:-1, :] & ~mask[1:, :]
            e[-1, :] = mask[-1, :]
        elif side == "left":
            e[:, 1:] = mask[:, 1:] & ~mask[:, :-1]
            e[:, 0] = mask[:, 0]
        elif side == "right":
            e[:, :-1] = mask[:, :-1] & ~mask[:, 1:]
            e[:, -1] = mask[:, -1]
        return e

    def shape(self, mask, fill, outline=None, hi=None, lo=None, hi_side="top", lo_side="bottom"):
        """Fill a mask, then (optionally) a 1-px highlight edge, shade edge and outline."""
        self.a[mask] = fill
        if hi is not None:
            self.a[self.inner_edge(mask, hi_side)] = hi
        if lo is not None:
            self.a[self.inner_edge(mask, lo_side)] = lo
        if outline is not None:
            self.a[self.outline_mask(mask)] = outline

    def auto_outline(self, c, diag=False):
        m = self.a != 0
        self.a[self.outline_mask(m, diag)] = c

    # ---- dithers and gradients ---------------------------------------------
    def dither_rect(self, x, y, w, h, c1, c2, level):
        """Ordered dither: `level`/16 of the pixels take c2, the rest c1."""
        x0, y0, x1, y1 = self._clip(x, y, w, h)
        if x1 <= x0 or y1 <= y0:
            return
        b = bayer(x1 - x0, y1 - y0, x0, y0)
        self.a[y0:y1, x0:x1] = np.where(b < level, c2, c1).astype(np.uint8)

    def dither_mask(self, mask, c, level, ox=0, oy=0):
        b = bayer(self.w, self.h, ox, oy)
        self.a[mask & (b < level)] = c

    def vgrad(self, x, y, w, h, ramp, seam=0.24, top_first=True):
        """Banded vertical gradient through `ramp` (indices), dithered seams only."""
        x0, y0, x1, y1 = self._clip(x, y, w, h)
        n = len(ramp)
        for yy in range(y0, y1):
            t = (yy - y) / max(1, h - 1)
            L = t * (n - 1) if top_first else (1 - t) * (n - 1)
            a = int(L)
            fr = L - a
            if a >= n - 1:
                self.a[yy, x0:x1] = ramp[-1]
                continue
            lo_c, hi_c = ramp[a], ramp[a + 1]
            lo_t, hi_t = 0.5 - seam / 2, 0.5 + seam / 2
            if fr < lo_t:
                self.a[yy, x0:x1] = lo_c
            elif fr > hi_t:
                self.a[yy, x0:x1] = hi_c
            else:
                lvl = int((fr - lo_t) / seam * 16)
                b = BAYER4[yy & 3, (np.arange(x0, x1)) & 3]
                self.a[yy, x0:x1] = np.where(b < lvl, hi_c, lo_c)

    def bands(self, y, rows, ramp_rows, x=0, w=None):
        """Explicit bands: ramp_rows = [(index, height), ...] with 2-row dither seams."""
        w = self.w if w is None else w
        yy = y
        for k, (c, hgt) in enumerate(ramp_rows):
            self.rect(x, yy, w, hgt, c)
            if k > 0:
                prev = ramp_rows[k - 1][0]
                self.dither_rect(x, yy, w, 1, c, prev, 6)
                self.dither_rect(x, yy - 1, w, 1, prev, c, 6)
            yy += hgt

    # ---- composition --------------------------------------------------------
    def blit(self, src, x, y, flip=False, key=0, lut=None):
        s = src.a if isinstance(src, Canvas) else src
        if flip:
            s = s[:, ::-1]
        h, w = s.shape
        x, y = int(x), int(y)
        sx0, sy0 = max(0, -x), max(0, -y)
        dx0, dy0 = max(0, x), max(0, y)
        dx1, dy1 = min(self.w, x + w), min(self.h, y + h)
        if dx1 <= dx0 or dy1 <= dy0:
            return
        sub = s[sy0:sy0 + (dy1 - dy0), sx0:sx0 + (dx1 - dx0)]
        if lut is not None:
            sub = lut[sub]
        if key is None:
            self.a[dy0:dy1, dx0:dx1] = sub
        else:
            m = sub != key
            self.a[dy0:dy1, dx0:dx1][m] = sub[m]

    def remap(self, lut, mask=None):
        if mask is None:
            self.a = lut[self.a]
        else:
            self.a[mask] = lut[self.a[mask]]

    def darken(self, mask, steps=1):
        for _ in range(steps):
            self.a[mask] = pal.DARKER[self.a[mask]]

    def lighten(self, mask, steps=1):
        for _ in range(steps):
            self.a[mask] = pal.LIGHTER[self.a[mask]]

    def crop(self, x, y, w, h):
        c = Canvas(w, h)
        c.blit(self.a, -x, -y, key=None)
        return c

    def scaled(self, k):
        c = Canvas(self.w * k, self.h * k)
        c.a = np.repeat(np.repeat(self.a, k, 0), k, 1)
        return c

    def bbox(self):
        ys, xs = np.nonzero(self.a)
        if len(xs) == 0:
            return (0, 0, 0, 0)
        return (int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1)

    # ---- output -------------------------------------------------------------
    def rgb(self, transparent=(0, 0, 0)):
        out = pal.RGB[self.a].copy()
        out[self.a == 0] = transparent
        return out

    def save_png(self, path):
        write_indexed_png(path, self.a)


def write_indexed_png(path, a):
    """An 8-bit indexed PNG with the master palette; index 0 transparent (tRNS)."""
    h, w = a.shape
    raw = b"".join(b"\x00" + a[y].tobytes() for y in range(h))

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0))
    png += chunk(b"PLTE", bytes(pal.png_palette()))
    png += chunk(b"tRNS", b"\x00")
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)
