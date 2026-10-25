"""showtk.review.pictures - frames out of the per-shot FFV1 masters, the palette LUT,
and the per-frame change signal the timing report measures."""
from functools import lru_cache
from pathlib import Path

import numpy as np

from .. import ffmpeg as ff
from ..edl import Masters
from ..sidecars import REPO


@lru_cache(maxsize=1)
def palette():
    """doomo_palette (core/src/palette.c, read-only) -> 256x3 uint8, (v<<3)|(v>>2)."""
    import re
    src = (REPO / "core" / "src" / "palette.c").read_text()
    i = src.index("doomo_palette[256] = {")
    body = re.sub(r"/\*.*?\*/", "", src[i:src.index("};", i)], flags=re.S)
    vals = []
    for _, a in re.findall(r"\b(C3|C)\s*\(([^()]*)\)", body):
        v = [int(x, 0) for x in a.split(",")]
        for k in range(0, len(v), 3):
            vals.append(tuple(((c >> 3) << 3) | ((c >> 3) >> 2) for c in v[k:k + 3]))
    return np.array(vals, np.uint8)


@lru_cache(maxsize=1)
def on_palette_lut():
    """32,768-entry BGR555 -> on-palette bool (the capture's LUT, sec.2.3)."""
    pal = palette().astype(np.int32) >> 3
    lut = np.zeros(1 << 15, bool)
    lut[pal[:, 0] | (pal[:, 1] << 5) | (pal[:, 2] << 10)] = True
    return lut


def off_palette_mask(rgb):
    v = rgb.astype(np.int32) >> 3
    return ~on_palette_lut()[v[..., 0] | (v[..., 1] << 5) | (v[..., 2] << 10)]


class Pictures:
    def __init__(self, tl, masters_dir=None):
        self.tl = tl
        self.masters = Masters(tl, masters_dir)
        c = tl.canvas
        self.size = (c["w"], c["full_h"])
        self.y0, self.h = c["y_off"], c["h"]
        self._shot_cache = {}

    def has(self, sid):
        return self.masters.path(sid) is not None

    def shot_frames(self, sid, full=False):
        """Every frame of a shot -> N x H x W x 3 (picture area unless full)."""
        if sid not in self._shot_cache:
            p = self.masters.path(sid)
            if p is None:
                raise FileNotFoundError("no master for %s (run `show render %s %s`)" % (
                    sid, self.tl.episode, sid))
            self._shot_cache = {sid: ff.decode_all(p, self.size)}     # one shot at a time
        arr = self._shot_cache[sid]
        return arr if full else arr[:, self.y0:self.y0 + self.h]

    def frames(self, ep_frames, full=False):
        """Episode frames -> {f: picture}."""
        out, by = {}, {}
        for f in ep_frames:
            s = self.tl.shot_at(f)
            if s is not None:
                by.setdefault(s.id, []).append(f)
        for sid, fs in by.items():
            s = self.tl.shot(sid)
            p = self.masters.path(sid)
            if p is None:
                continue
            got = ff.decode_frames(p, [f - s.start for f in fs], self.size)
            for f in fs:
                img = got[f - s.start]
                out[f] = img if full else img[self.y0:self.y0 + self.h]
        return out


def change_signal(frames, prev=None):
    """Changed-pixel count between consecutive frames: d[0] against `prev` (the frame
    before, or all pixels when there is none: a cut from nothing)."""
    a = frames.astype(np.int16)
    d = np.zeros(len(frames), np.int64)
    if len(frames) > 1:
        d[1:] = (np.abs(np.diff(a, axis=0)).max(axis=3) > 0).sum(axis=(1, 2))
    if prev is not None:
        d[0] = int((np.abs(a[0] - prev.astype(np.int16)).max(axis=2) > 0).sum())
    else:
        d[0] = frames.shape[1] * frames.shape[2]
    return d


def baseline(d, half=7):
    """The running median of the change signal: continuous motion (drifts, walks)."""
    n = len(d)
    b = np.zeros(n)
    for i in range(n):
        lo, hi = max(0, i - half), min(n, i + half + 1)
        b[i] = np.median(d[lo:hi])
    return b


def save_png(arr, path):
    from PIL import Image
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(arr).save(path)
    return path
