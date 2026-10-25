"""preview.py - animated previews of sets with their ambient layers and cast cycles.

    python3 -m showtk.art.motion.preview fun_fair:night bandstand:night ... [--out DIR]
        [--frames 180] [--pan 0.3] [--cast mayor:walk,nana:idle] [--gif] [--strip]

Writes <set>_<variant>.mp4 (x3, 30 fps, H.264 CRF 14, nearest) and, with
--strip, a PNG of 8 frames 6 f apart so a reader can see the motion. The
open matte is cropped to the 320x180 picture.
"""
import argparse
import os
import subprocess
import sys

import numpy as np
from PIL import Image

from ..setkit.setfile import Library
from ..setkit import compose as C
from .render import SetAnim

OUT = os.path.join(os.environ.get("SHOW_OUT") or
                   "/tmp/claude-0/-home-user-DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/"
                   "ea792815-11f0-5f9f-bdfe-5997cdfc4994/scratchpad/show_out", "art", "motion")


def palette():
    return C._g().pal.RGB


def find_variant(sd, vid):
    for v in C.variants_of(sd):
        if vid is None or v.id == vid:
            return v
    raise KeyError("set %s has no variant %s" % (sd.id, vid))


def render_frames(lib, spec, frames=180, pan=0.0, cam0=None, actors=(), cache=None):
    sid, _, vid = spec.partition(":")
    sd = lib.sets[sid]
    var = find_variant(sd, vid or None)
    sa = SetAnim(lib, sd, var, crowd_cache=cache)
    if cam0 is None:
        cam0 = (0.0, 0.0)
        for fr in sd.frames:
            cam0 = tuple(float(v) for v in fr.get("at").split(","))
            break
    out = []
    for t in range(frames):
        cam = (cam0[0] + pan * (t - frames / 2), cam0[1])
        out.append(sa.frame(t, cam=cam, actors=actors)[30:210])
    return out


def write_mp4(frames, path, scale=3):
    rgb = palette()
    h, w = frames[0].shape
    p = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24",
                          "-s", "%dx%d" % (w, h), "-r", "30", "-i", "-",
                          "-vf", "scale=%d:%d:flags=neighbor" % (w * scale, h * scale),
                          "-c:v", "libx264", "-preset", "veryfast", "-crf", "14", "-pix_fmt", "yuv420p",
                          "-threads", "2", path], stdin=subprocess.PIPE)
    for f in frames:
        p.stdin.write(rgb[f].astype(np.uint8).tobytes())
    p.stdin.close()
    if p.wait():
        raise RuntimeError("ffmpeg failed for " + path)


def write_gif(frames, path, scale=2, step=2):
    rgb = palette()
    ims = [Image.fromarray(rgb[f].astype(np.uint8)).resize((f.shape[1] * scale, f.shape[0] * scale),
                                                          Image.NEAREST) for f in frames[::step]]
    ims[0].save(path, save_all=True, append_images=ims[1:], duration=int(1000 * step / 30), loop=0)


def write_strip(frames, path, picks=None, scale=2):
    rgb = palette()
    picks = picks or list(range(0, min(len(frames), 48), 6))
    h, w = frames[0].shape
    cols = 2
    rows = (len(picks) + cols - 1) // cols
    sheet = np.full((rows * (h * scale + 4), cols * (w * scale + 4), 3), 24, np.uint8)
    for k, t in enumerate(picks):
        im = np.repeat(np.repeat(rgb[frames[t]], scale, 0), scale, 1)
        r, c = divmod(k, cols)
        sheet[r * (h * scale + 4):r * (h * scale + 4) + h * scale, c * (w * scale + 4):c * (w * scale + 4) + w * scale] = im
    Image.fromarray(sheet).save(path)


def motion_stats(frames):
    """moving pixels per frame pair, and the longest pixel-static run (frames)."""
    diffs = [int((a != b).sum()) for a, b in zip(frames, frames[1:])]
    run = best = 0
    for d in diffs:
        run = run + 1 if d == 0 else 0
        best = max(best, run)
    return (min(diffs) if diffs else 0, int(np.mean(diffs)) if diffs else 0, best)


def main(argv=None):
    ap = argparse.ArgumentParser(prog="showtk.art.motion.preview")
    ap.add_argument("sets", nargs="*")
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--frames", type=int, default=180)
    ap.add_argument("--pan", type=float, default=0.0)
    ap.add_argument("--gif", action="store_true")
    ap.add_argument("--strip", action="store_true")
    a = ap.parse_args(argv)
    os.makedirs(a.out, exist_ok=True)
    lib = Library().load_dir()
    cache = {}
    for spec in a.sets:
        fr = render_frames(lib, spec, a.frames, a.pan, cache=cache)
        name = spec.replace(":", "_")
        write_mp4(fr, os.path.join(a.out, name + ".mp4"))
        if a.gif:
            write_gif(fr, os.path.join(a.out, name + ".gif"))
        if a.strip:
            write_strip(fr, os.path.join(a.out, name + "_strip.png"))
        mn, avg, run = motion_stats(fr)
        print("%-28s moving px/frame min %d avg %d, longest static run %d f" % (spec, mn, avg, run))


if __name__ == "__main__":
    sys.exit(main())
