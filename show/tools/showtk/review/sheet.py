"""showtk.review.sheet - contact sheets (`show sheet`) and before/after sheets (`show diff`).

    show sheet ep101 co01..co03 [--every 30] [--scale 2] [--out PNG]
    show diff  ep101 co02 --against DIR_OF_REFERENCE_MASTERS [--out PNG]

A sheet is a grid of every event frame (each span's first and last frame, a
type-on's first and last glyph) plus every Nth episode frame, each tile
captioned "shot +frame  f<episode frame>  TC  event".  Tiles are the 320x180
picture at x1 (x2 when the sheet has 24 tiles or fewer, or with --scale 2).
A diff sheet puts reference | current side by side and outlines changed tiles
in red with their changed-pixel count (AT13.2).
"""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from .. import ffmpeg as ff
from ..sidecars import build_dir, tc
from .pictures import Pictures

BG = (24, 22, 30)
FG = (236, 232, 220)
HL = (255, 214, 64)
RED = (235, 40, 50)


def _font(size=11):
    f = ff.font()
    try:
        return ImageFont.truetype(f.replace("Mono", "") if "Mono" in f else f, size)
    except OSError:
        try:
            return ImageFont.truetype(f, size)
        except OSError:
            return ImageFont.load_default()


def event_frames(tl, f0, f1, every=30):
    """{episode frame: [labels]} for a sheet of the range."""
    marks = {}

    def add(f, label):
        if f0 <= f < f1:
            marks.setdefault(int(f), []).append(label)
    for s in tl.shots_in(f0, f1):
        add(s.start, "CUT %s" % s.cut if s.start > 0 else "start")
        add(s.end - 1, "last")
        for e in s.events:
            if not e.visual:
                continue
            g = e.raw.get("glyphs")
            if g:
                add(g[0], "%s first glyph" % e.op)
                add(g[-1], "%s last glyph" % e.op)
            elif e.span:
                add(e.span[0], "%s start" % e.op)
                add(e.span[1], "%s end" % e.op)
            else:
                add(e.f, e.op)
    if every:
        for f in range((f0 + every - 1) // every * every, f1, every):
            marks.setdefault(f, [])
    return dict(sorted(marks.items()))


def _caption(tl, f, labels):
    s = tl.shot_at(f)
    head = "%s +%d  f%d  %s" % (s.id if s else "?", f - s.start if s else 0, f, tc(f))
    return head, ", ".join(labels)


def compose(tiles, scale, cols=None, title=None, borders=None):
    """tiles: [(picture HxWx3, head, sub)] -> PIL image."""
    if not tiles:
        raise ValueError("no frames to put on a sheet")
    h, w = tiles[0][0].shape[:2]
    tw, th = w * scale, h * scale
    cols = cols or max(1, min(len(tiles), (1800 // (tw + 8)) or 1))
    rows = (len(tiles) + cols - 1) // cols
    cap = 30
    top = 26 if title else 4
    sheet = Image.new("RGB", (cols * (tw + 8) + 8, top + rows * (th + cap + 8) + 4), BG)
    d = ImageDraw.Draw(sheet)
    f1, f2 = _font(12), _font(11)
    if title:
        d.text((8, 6), title, fill=HL, font=_font(14))
    for i, (pic, head, sub) in enumerate(tiles):
        r, c = divmod(i, cols)
        x, y = 8 + c * (tw + 8), top + r * (th + cap + 8)
        img = Image.fromarray(np.repeat(np.repeat(pic, scale, 0), scale, 1))
        sheet.paste(img, (x, y))
        if borders and borders[i]:
            d.rectangle([x - 3, y - 3, x + tw + 2, y + th + 2], outline=RED, width=3)
        d.text((x, y + th + 2), head, fill=FG, font=f1)
        d.text((x, y + th + 16), sub[:max(10, tw // 6)], fill=HL, font=f2)
    return sheet


def sheet(tl, rng=None, masters_dir=None, every=30, scale=None, out=None, cols=None):
    f0, f1 = tl.range(rng)
    marks = event_frames(tl, f0, f1, every)
    pics = Pictures(tl, masters_dir).frames(list(marks))
    tiles = []
    for f, labels in marks.items():
        if f in pics:
            head, sub = _caption(tl, f, labels)
            tiles.append((pics[f], head, sub))
    scale = scale or (2 if len(tiles) <= 24 else 1)
    img = compose(tiles, scale, cols, title="%s %s  sheet: %d frames (events + every %d f)" % (
        tl.episode, rng or "all", len(tiles), every))
    out = Path(out) if out else build_dir(tl.episode) / "review" / ("sheet_%s.png" % (
        (rng or "all").replace("..", "-")))
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    return out, [t[1] for t in tiles]


def diff(tl, rng, masters_dir, against_dir, every=15, out=None, scale=1):
    """Before/after: reference (against) | current; changed tiles outlined in red."""
    f0, f1 = tl.range(rng)
    marks = event_frames(tl, f0, f1, every)
    cur = Pictures(tl, masters_dir).frames(list(marks))
    ref = Pictures(tl, against_dir).frames(list(marks))
    tiles, borders, changed = [], [], []
    for f, labels in marks.items():
        if f not in cur or f not in ref:
            continue
        a, b = ref[f], cur[f]
        n = int((np.abs(a.astype(np.int16) - b.astype(np.int16)).max(axis=2) > 0).sum())
        head, sub = _caption(tl, f, labels)
        tiles.append((np.concatenate([a, np.full((a.shape[0], 4, 3), 255, np.uint8), b], 1),
                      head + ("   CHANGED %d px" % n if n else "   same"), "before | after  " + sub))
        borders.append(n > 0)
        if n:
            changed.append((f, n))
    img = compose(tiles, scale, cols=2, borders=borders,
                  title="%s %s  diff: %d of %d sampled frames changed" % (
                      tl.episode, rng, len(changed), len(tiles)))
    out = Path(out) if out else build_dir(tl.episode) / "review" / ("diff_%s.png" % rng.replace("..", "-"))
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    return out, changed
