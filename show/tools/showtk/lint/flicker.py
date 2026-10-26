"""lint/flicker.py - ANIM-FLICKER: actors that vanish or flash for 1-2 frames.

A flicker is a region that changes on frame t and changes BACK on frame t+k
(k = 1 or 2), so the frames either side of the span agree there while the
span itself differs: a character dropped for a frame (eviction, a late
upload, a missing pose frame) and drawn again. Per span:

    M = changed(f[t-1], f[t]) & changed(f[t+k-1], f[t+k]) & ~changed(f[t-1], f[t+k])

and the largest 8-connected blob of M (after a 1 px close) is the hit. Small
blobs (blinks, lip flap, a twinkle) stay under --min-area; whole-canvas
changes (flash fx, a cut inside a master) are reported as `flash`, not
flicker. Masters are read from the render index (render_index.json) of
`show render`, one shot each.

    cd show/tools && python3 -m showtk.lint.flicker ../build/ep101/render [--shots a,b] [--json OUT]
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

import numpy as np
from scipy import ndimage

THRESH = 40          # per-channel difference that counts as "changed"
MIN_AREA = 300       # px of 320x240: a px=1 chibi is ~250, a px=2 one ~1000
FLASH_FRAC = 0.6     # a blob over this share of the canvas is a flash/cut


def read_frames(path):
    p = subprocess.run(["ffmpeg", "-v", "error", "-i", str(path), "-f", "rawvideo",
                        "-pix_fmt", "rgb24", "-"], capture_output=True, check=True)
    a = np.frombuffer(p.stdout, np.uint8)
    return a.reshape(-1, 240, 320, 3)


def _changed(a, b):
    return (np.abs(a.astype(np.int16) - b.astype(np.int16)) > THRESH).any(axis=2)


def scan(fr, min_area=MIN_AREA):
    """[(frame, k, area, bbox, kind)] for one shot's frames"""
    hits = []
    n = len(fr)
    ch = [_changed(fr[i], fr[i + 1]) for i in range(n - 1)]      # ch[i]: i -> i+1
    st = np.ones((3, 3), bool)
    for t in range(1, n - 1):
        for k in (1, 2):
            if t + k >= n:
                break
            m = ch[t - 1] & ch[t + k - 1] & ~_changed(fr[t - 1], fr[t + k])
            if k == 2:
                m &= ~ch[t]                    # a 2-frame drop holds its wrong pixels
            if m.sum() < min_area:
                continue
            m = ndimage.binary_closing(m, st)
            lab, nl = ndimage.label(m, st)
            if not nl:
                continue
            sizes = ndimage.sum(m, lab, range(1, nl + 1))
            j = int(np.argmax(sizes))
            area = int(sizes[j])
            if area < min_area:
                continue
            ys, xs = np.nonzero(lab == j + 1)
            bbox = [int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())]
            w, h = bbox[2] - bbox[0] + 1, bbox[3] - bbox[1] + 1
            if area > FLASH_FRAC * 320 * 180:
                kind = "flash"
            elif w >= 80 and h * 5 < w * 2:
                kind = "ambient"               # a wide thin band: lights, bunting, ground fx
            else:
                kind = "flicker"               # actor-shaped: the defect
                sel = lab == j + 1
                span = fr[t][sel].astype(np.int16)
                if span.std(axis=0).max() < 6 and fr[t - 1][sel].std(axis=0).max() > 12:
                    kind = "hole"              # a plate tile missing: flat clear colour
            hits.append({"frame": t, "k": k, "area": area, "bbox": bbox, "kind": kind})
            break                              # one report per start frame
    _cycles(hits)
    return hits


def _iou(a, b):
    ix = min(a[2], b[2]) - max(a[0], b[0]) + 1
    iy = min(a[3], b[3]) - max(a[1], b[1]) + 1
    if ix <= 0 or iy <= 0:
        return 0.0
    i = ix * iy
    ar = lambda r: (r[2] - r[0] + 1) * (r[3] - r[1] + 1)
    return i / float(ar(a) + ar(b) - i)


def _cycles(hits):
    """A-B-A-B pose/flipbook cycles (walk, wave, a 2-frame boil) revert every
    frame too; a real drop is isolated. A hit with 2+ overlapping hits within
    +-4 frames is a cycle, not a flicker."""
    fl = [h for h in hits if h["kind"] == "flicker"]
    cyc = [sum(1 for o in fl if o is not h and abs(o["frame"] - h["frame"]) <= 4
               and _iou(o["bbox"], h["bbox"]) > 0.2) >= 2 for h in fl]
    for h, c in zip(fl, cyc):
        if c:
            h["kind"] = "cycle"


def main(argv=None):
    ap = argparse.ArgumentParser(prog="showtk.lint.flicker")
    ap.add_argument("render_dir")
    ap.add_argument("--shots", default="")
    ap.add_argument("--min-area", type=int, default=MIN_AREA)
    ap.add_argument("--json")
    a = ap.parse_args(argv)
    idx = json.loads((Path(a.render_dir) / "render_index.json").read_text())["shots"]
    want = set(s for s in a.shots.split(",") if s)
    out, total, amb_total, holes = {}, 0, [0], [0]
    for sid, d in idx.items():
        if want and sid not in want:
            continue
        mkv = Path(d) / "master.mkv"
        if not mkv.exists():
            continue
        hits = scan(read_frames(mkv), a.min_area)
        out[sid] = hits
        nf = sum(h["k"] for h in hits if h["kind"] == "flicker")
        total += nf
        amb_total[0] += sum(h["k"] for h in hits if h["kind"] == "ambient")
        holes[0] += sum(h["k"] for h in hits if h["kind"] == "hole")
        if nf:
            print(f"ANIM-FLICKER {sid}: {nf} frame(s) " +
                  " ".join([f"+{h['frame']}x{h['k']}:{h['area']}px{h['kind'][0]}{h['bbox']}" for h in hits if h["kind"] == "flicker"][:8]))
        sys.stdout.flush()
    print(f"ANIM-FLICKER total: {total} actor flicker frame(s) over {len(out)} shot(s); {amb_total[0]} ambient-band frame(s) (not actors); {holes[0]} plate-hole frame(s)")
    if a.json:
        Path(a.json).write_text(json.dumps(out, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
