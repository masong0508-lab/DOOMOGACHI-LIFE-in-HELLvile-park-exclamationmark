"""sheet.py - contact sheets: every variant of a set at 320x180 and at x3.

Layout: a title bar; a strip of the 1x frames (16:9 crop, rows 30-209) of every
variant and framing; then each one again at x3 (nearest neighbour), two per
row, each captioned. PIC-PAL is checked on every frame drawn here.
"""
import numpy as np

from . import gen
from .compose import compose, variants_of, MATTE
from .setfile import xy

BG = (24, 22, 32)
FG = (255, 248, 228)
DIM = (150, 146, 170)


def _text(img, x, y, s, col, scale=1):
    font = gen().font
    for ch in s:
        g = font.glyph(ch)
        ys, xs = np.nonzero(g)
        for gy, gx in zip(ys, xs):
            img[y + gy * scale:y + gy * scale + scale, x + gx * scale:x + gx * scale + scale] = col
        x += 6 * scale


def frames_of(sd):
    """(label, cam) for the set's named framings; camera 0 first."""
    out = [("cam 0,0", (0.0, 0.0))]
    for fr in sd.frames:
        if fr.get("at"):
            out.append((fr.id, xy(fr.get("at"))))
    return out


def render_frames(lib, sd, variants=None, frames=None):
    pal = gen().pal
    lut555 = pal.rgb555_lut()
    tiles = []
    vs = variants_of(sd)
    if variants:
        vs = [v for v in vs if v.id in variants]
    fr = frames_of(sd)
    if frames is not None:
        fr = fr[:frames]
    for v in vs:
        for label, cam in (fr if v is vs[0] else fr[:1]):
            idx = compose(lib, sd, v, cam)
            rgb = pal.RGB[idx]
            crop = rgb[MATTE:MATTE + 180]
            # PIC-PAL: every pixel's BGR555 maps back to a palette entry
            r5 = (crop[..., 0] >> 3).astype(np.int32)
            g5 = (crop[..., 1] >> 3).astype(np.int32)
            b5 = (crop[..., 2] >> 3).astype(np.int32)
            off = int((lut555[r5 | (g5 << 5) | (b5 << 10)] < 0).sum())
            tiles.append(("%s / %s" % (v.id, label) if label != "cam 0,0" else v.id, crop, off, idx))
    return tiles


def contact_sheet(lib, sd, path, cols1=4, cols3=2):
    from PIL import Image
    tiles = render_frames(lib, sd)
    n = len(tiles)
    pad = 8
    head = 34
    rows1 = (n + cols1 - 1) // cols1
    rows3 = (n + cols3 - 1) // cols3
    w1 = cols1 * (320 + pad) + pad
    w3 = cols3 * (960 + pad) + pad
    W = max(w1, w3)
    H = head + rows1 * (180 + 16 + pad) + pad + rows3 * (540 + 20 + pad) + pad
    img = np.zeros((H, W, 3), np.uint8)
    img[:] = BG
    title = "%s  kind=%s px=%d  (%s)" % (sd.id, sd.kind, sd.px, sd.args.get("desc", "")[:90])
    _text(img, pad, 8, title, FG, 2)
    offs = sum(t[2] for t in tiles)
    _text(img, pad, 26, "PIC-PAL off-palette pixels: %d  |  variants: %s" % (
        offs, ", ".join(v.id for v in variants_of(sd))), DIM, 1)
    y = head
    for k, (label, crop, off, _) in enumerate(tiles):
        cx = pad + (k % cols1) * (320 + pad)
        cy = y + (k // cols1) * (180 + 16 + pad)
        img[cy:cy + 180, cx:cx + 320] = crop
        _text(img, cx, cy + 183, label[:52], FG)
    y = head + rows1 * (180 + 16 + pad) + pad
    for k, (label, crop, off, _) in enumerate(tiles):
        cx = pad + (k % cols3) * (960 + pad)
        cy = y + (k // cols3) * (540 + 20 + pad)
        big = np.repeat(np.repeat(crop, 3, 0), 3, 1)
        img[cy:cy + 540, cx:cx + 960] = big
        _text(img, cx, cy + 544, ("x3  " + label)[:150], FG, 2)
    Image.fromarray(img).save(path)
    return tiles


def overview(lib, sets, path, cols=4):
    """SETS_OVERVIEW.png: the first variant of every set at 320x180, captioned."""
    from PIL import Image
    tiles = []
    for sd in sets:
        if sd.kind != "2d":
            continue
        t = render_frames(lib, sd, frames=1)
        if t:
            label, crop, off, _ = t[0]
            tiles.append((sd.id + "  [" + ",".join(v.id for v in variants_of(sd)) + "]", crop, off))
    pad = 8
    n = len(tiles)
    rows = (n + cols - 1) // cols
    W = cols * (320 + pad) + pad
    H = 30 + rows * (180 + 16 + pad) + pad
    img = np.zeros((H, W, 3), np.uint8)
    img[:] = BG
    _text(img, pad, 8, "Mingle Night (ep101) - every 2D set, first variant, 320x180", FG, 2)
    for k, (label, crop, off) in enumerate(tiles):
        cx = pad + (k % cols) * (320 + pad)
        cy = 30 + (k // cols) * (180 + 16 + pad)
        img[cy:cy + 180, cx:cx + 320] = crop
        _text(img, cx, cy + 183, label[:52], FG)
    Image.fromarray(img).save(path)
    return tiles
