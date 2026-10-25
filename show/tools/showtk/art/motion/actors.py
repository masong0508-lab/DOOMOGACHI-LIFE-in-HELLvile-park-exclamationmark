"""actors.py - cast flipbooks on a set, for previews and sheets (ART track).

    a = PreviewActor("mayor", script=[(0, "walk", x0, x1, 60), (60, "pop_small"), ...], y=160)
    SetAnim(...).frame(t, actors=[a])

A tiny stand-in for the player's actor pose channel (show_actor.h SHOP_POSE +
SHOP_WALK): a pose plays its flipbook (loops, or a one-shot that hands back to
its `next`), a walk moves the anchor linearly, `face` mirrors. The flipbooks
come from series/acting.shw through the same generators the compiler bakes.

    python3 -m showtk.art.motion.actors sheet [--out DIR]    # every flipbook, sheets
"""
import os
import sys

import numpy as np
from PIL import Image

from ..setkit import compose as C
from ..cast.acting import Library as Acting
from . import ambient as A

_ACT = []


def acting():
    if not _ACT:
        _ACT.append(Acting())
    return _ACT[0]


def flip_frames(fl):
    fb = C.bake(fl["src"])
    return fb.meta.get("frames", [fb]), fb.meta.get("holds", [1]), fb.meta.get("pad", 0)


class PreviewActor:
    """script: [(t, op, args...)] with ops
         ("pose", kind)                     play a flipbook from t
         ("walk", to_x, over)               move the anchor to to_x over frames
         ("face", "L"|"R")"""

    def __init__(self, char, x, y, script, face="R", px=1):
        self.char, self.x0, self.y, self.px = char, float(x), int(y), px
        self.script = sorted(script, key=lambda s: s[0])
        self.face0 = face

    def state(self, t):
        x, face = self.x0, self.face0
        pose, pose_t = "idle", 0
        walk = None
        for ev in self.script:
            if ev[0] > t:
                break
            op = ev[1]
            if op == "pose":
                pose, pose_t = ev[2], ev[0]
            elif op == "face":
                face = ev[2]
            elif op == "walk":
                if walk is not None:
                    x = walk[1] + (walk[2] - walk[1]) * min(1.0, (ev[0] - walk[0]) / float(walk[3]))
                walk = (ev[0], x, float(ev[2]), int(ev[3]))
        if walk is not None:
            k = min(1.0, (t - walk[0]) / float(max(1, walk[3])))
            x = walk[1] + (walk[2] - walk[1]) * k
        # resolve one-shots that ran out
        lib = acting()
        while True:
            fl = lib.resolve_pose(self.char, pose)
            if fl is None:
                return x, face, None, 0
            frames, holds, pad = flip_frames(fl)
            if fl["loop"] or (t - pose_t) < sum(holds):
                break
            pose_t += sum(holds)
            pose = fl["next"]
        k = A.flip_frame(holds, t - pose_t) if fl["loop"] else self._oneshot(holds, t - pose_t)
        return x, face, frames[k], pad

    @staticmethod
    def _oneshot(holds, tt):
        for i, h in enumerate(holds):
            if tt < h:
                return i
            tt -= h
        return len(holds) - 1

    def draw(self, out, t, cam):
        x, face, img, pad = self.state(t)
        if img is None:
            return
        a = img.a if hasattr(img, "a") else img
        if face == "L":
            a = a[:, ::-1]
        h, w = a.shape
        X = int(round(x - w / 2.0 - cam[0]))
        Y = int(round(self.y + 30 - h + pad - cam[1]))
        C._put(out, a, X, Y, self.px, None)


# ---- the flipbook sheets -------------------------------------------------------
def sheet(out_dir):
    lib = acting()
    rgb = C._g().pal.RGB
    groups = {}
    for name, fl in sorted(lib.flips.items()):
        key = fl["char"] or "props"
        groups.setdefault(key, []).append((name, fl))
    paths = []
    for key, items in sorted(groups.items()):
        rows = []
        for name, fl in items:
            frames, holds, _pad = flip_frames(fl)
            H = max(f.h for f in frames)
            W = sum(f.w + 2 for f in frames)
            row = np.full((H + 12, max(W, 160), 3), 40, np.uint8)
            x = 0
            for f in frames:
                im = rgb[f.a].astype(np.uint8)
                im[f.a == 0] = (72, 70, 92)
                row[12:12 + f.h, x:x + f.w] = im
                x += f.w + 2
            rows.append((name, holds, fl, row))
        Wm = max(r[3].shape[1] for r in rows)
        Hs = sum(r[3].shape[0] for r in rows)
        sh = np.full((Hs, Wm, 3), 24, np.uint8)
        y = 0
        for _n, _h, _f, r in rows:
            sh[y:y + r.shape[0], :r.shape[1]] = r
            y += r.shape[0]
        im = Image.fromarray(sh).resize((Wm * 2, Hs * 2), Image.NEAREST)
        from PIL import ImageDraw
        d = ImageDraw.Draw(im)
        y = 0
        for n, holds, fl, r in rows:
            d.text((4, y * 2), "%s  holds=%s  %s%s" % (n, ",".join(map(str, holds)), fl["mode"],
                                                       "" if fl["loop"] else " one-shot -> " + fl["next"]),
                   fill=(240, 236, 220))
            y += r.shape[0]
        p = os.path.join(out_dir, "flipbooks_%s.png" % key)
        im.save(p)
        paths.append(p)
    return paths


def main(argv=None):
    argv = argv if argv is not None else sys.argv[1:]
    out = argv[argv.index("--out") + 1] if "--out" in argv else os.path.join(
        os.environ.get("SHOW_OUT") or "/tmp/claude-0/-home-user-DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/"
        "ea792815-11f0-5f9f-bdfe-5997cdfc4994/scratchpad/show_out", "art", "motion")
    os.makedirs(out, exist_ok=True)
    if argv and argv[0] == "sheet":
        for p in sheet(out):
            print(p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
