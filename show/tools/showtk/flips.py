"""flips.py - `pose <actor> <name>` -> the FLIP records the player draws (show_actor.h).

The flipbooks are ART's (series/acting.shw `cycles` / `flip`, baked by
show/art/gen/motion.py from the engine's own renders).  This module resolves a
pose name for an actor's character, bakes it once (disk cache under
show/build/flipcache/, keyed by the src and motion.py's text), crops every
drawing to the union bounding box (kept centred on the anchor so dx = 0),
drops repeated drawings into longer holds, and returns pack_flip() arguments.

    resolve(char, name) -> dict(src, mode, loop, next) | None   (None: back to idle)
    load(char, name)    -> dict(w, h, frames, holds, mode, loop, dx, dy, hold_end)

Pose names "idle", "off", "none", "rest" and "next" return the actor to its
engine image (live mouth, blinks, breathing) instead of a baked idle loop.
"""
import hashlib
import os
import pickle
import sys

from . import paths

IDLE = ("idle", "off", "none", "rest", "next")
_LIB = []
_MEM = {}
SHOW = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


class FlipError(Exception):
    pass


def lib():
    if not _LIB:
        from .art.cast.acting import Library
        _LIB.append(Library())
    return _LIB[0]


def resolve(char, name):
    if name in IDLE:
        return None
    f = lib().flips.get(name) or lib().flips.get("%s_%s" % (char, name))
    if f is None:
        raise FlipError("pose %s: no flip %s or %s_%s in series/acting.shw" % (name, name, char, name))
    return f


def _src_hash(src):
    h = hashlib.sha1(src.encode())
    for fn in ("motion.py", "amb.py"):
        p = os.path.join(SHOW, "art", "gen", fn)
        try:
            with open(p, "rb") as fh:
                h.update(fh.read())
        except OSError:
            pass
    return h.hexdigest()[:20]


def _bake(src):
    import numpy as np
    cdir = os.path.join(SHOW, "build", "flipcache")
    cfile = os.path.join(cdir, _src_hash(src) + ".pkl")
    if os.path.exists(cfile):
        try:
            with open(cfile, "rb") as fh:
                return pickle.load(fh)
        except Exception:                   # noqa: BLE001 - a bad cache file is rebaked
            pass
    if SHOW not in sys.path:
        sys.path.insert(0, SHOW)
    from art.gen import bake
    fb = bake(src)
    frames = [np.asarray(f.a, np.uint8) for f in fb.meta.get("frames", [fb])]
    holds = list(fb.meta.get("holds", [1] * len(frames)))
    pad = int(fb.meta.get("pad", 0))
    out = (frames, holds, pad)
    os.makedirs(cdir, exist_ok=True)
    with open(cfile + ".tmp", "wb") as fh:
        pickle.dump(out, fh)
    os.replace(cfile + ".tmp", cfile)
    return out


def load(char, name):
    """the cropped flipbook for `pose <actor of char> <name>` (cached per process)"""
    import numpy as np
    f = resolve(char, name)
    if f is None:
        return None
    key = f["src"]
    if key in _MEM:
        r = dict(_MEM[key])
    else:
        frames, holds, pad = _bake(f["src"])
        H, W = frames[0].shape
        st = np.stack(frames)
        ys, xs = np.nonzero(st.any(axis=0))
        if not len(ys):
            raise FlipError("flip %s is empty" % name)
        cx = W // 2
        half = max(cx - int(xs.min()), int(xs.max()) + 1 - cx, 1)
        x0, x1 = max(0, cx - half), min(W, cx + half)
        floor = H - pad                      # the feet line of every drawing
        y0, y1 = int(ys.min()), max(int(ys.max()) + 1, min(floor, H))
        crop = [np.ascontiguousarray(a[y0:y1, x0:x1]) for a in frames]
        # repeated drawings become one longer hold
        fr, hd = [], []
        for a, h in zip(crop, holds):
            if fr and np.array_equal(fr[-1], a) and hd[-1] + h <= 255:
                hd[-1] += h
            else:
                fr.append(a)
                hd.append(max(1, min(255, int(h))))
        r = dict(w=x1 - x0, h=y1 - y0, frames=[a.tobytes() for a in fr], holds=hd,
                 dx=((x0 + x1) // 2 - cx), dy=y1 - floor)
        _MEM[key] = dict(r)
    r["mode"] = f.get("mode", "replace")
    r["loop"] = bool(int(f.get("loop", 1)))
    r["hold_end"] = (f.get("next") or "idle") in ("hold", "stay", "last")
    return r


def size(r):
    return 12 + ((len(r["frames"]) + 3) & ~3) + ((r["w"] * r["h"] * len(r["frames"]) + 3) & ~3)


_ = paths  # (imported for its side effects on sys.path in some entry points)
