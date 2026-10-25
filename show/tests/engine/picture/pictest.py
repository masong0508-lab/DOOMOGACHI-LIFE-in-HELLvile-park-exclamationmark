"""pictest.py - build and run the lane-2 test driver (host and PS1), read its
P-log, and make contact sheets. Used by test_picture.py and by hand:

    python3 show/tests/engine/picture/pictest.py host     build the host driver
    python3 show/tests/engine/picture/pictest.py ps1      build the PS1 driver for a blob
"""
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SHOW = HERE.parents[2]
REPO = SHOW.parent
sys.path.insert(0, str(SHOW / "tools"))

from showtk import paths                                      # noqa: E402

WORK = paths.SHOW_WORK / "lane2"
OUT = paths.SHOW_OUT / "engine"
ENGINE_SRCS = ["show_cam.c", "show_fx.c", "show_strokes.c", "show_tilemap.c", "show_text.c",
               "show_l2.c", "show_font_child.c"]
CORE_SRCS = ["camfx.c", "tween.c", "palette.c", "font.c"]


def build_host():
    exe = WORK / "host" / "pic_host"
    exe.parent.mkdir(parents=True, exist_ok=True)
    srcs = [SHOW / "engine" / s for s in ENGINE_SRCS] + [REPO / "core" / "src" / s for s in CORE_SRCS] + \
           [HERE / "pic_driver.c", HERE / "pic_host.c"]
    newest = max(p.stat().st_mtime for p in srcs + list((SHOW / "include" / "show").glob("*.h")))
    if exe.exists() and exe.stat().st_mtime > newest:
        return exe
    cmd = ["gcc", "-O1", "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-DSHOW_L2_HOST",
           "-I", str(SHOW / "include"), "-I", str(REPO / "core" / "include")] + \
          [str(s) for s in srcs] + ["-o", str(exe)]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if p.returncode:
        raise RuntimeError("host driver build failed:\n" + p.stdout)
    return exe


LOG = re.compile(r"PIC t=(\d+) (.*)")


def parse_log(text):
    rows = []
    for line in text.splitlines():
        m = LOG.search(line)
        if not m:
            continue
        r = {"t": int(m.group(1))}
        for kv in m.group(2).split():
            k, _, v = kv.partition("=")
            r[k] = v
        rows.append(r)
    return rows


def run_host(blob, name, frames=None, draw=True):
    """-> (log rows, out dir). frames: list of frames to write as PPM, or 'all'."""
    exe = build_host()
    d = WORK / "runs" / name
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)
    bp = d / "shot.pic"
    bp.write_bytes(blob)
    cmd = [str(exe), str(bp), str(d)]
    if not draw:
        cmd.append("--no-draw")
    elif frames:
        cmd += ["--png-frames", "all" if frames == "all" else ",".join(str(f) for f in frames)]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if p.returncode:
        raise RuntimeError("pic_host failed:\n" + p.stdout[-2000:])
    (d / "plog.txt").write_text(p.stdout)
    return parse_log(p.stdout), d


def read_ppm(path):
    import numpy as np
    data = Path(path).read_bytes()
    parts = data.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    return np.frombuffer(parts[3], np.uint8).reshape(h, w, 3).copy()


def frame_stamp(rgb):
    """The 16-bit frame number the driver stamps in screen rows 0-3."""
    t = 0
    for b in range(16):
        t = (t << 1) | (1 if rgb[1, b * 4 + 1].mean() > 128 else 0)
    return t


def sheet(images, labels, out, cols=4, scale=2, crop=(0, 30, 320, 180)):
    """A contact sheet: picture crops (320x180) at x scale, labelled."""
    from PIL import Image, ImageDraw
    x0, y0, w, h = crop
    rows = -(-len(images) // cols)
    tile_w, tile_h = w * scale, h * scale + 14
    im = Image.new("RGB", (cols * tile_w + (cols + 1) * 4, rows * tile_h + (rows + 1) * 4), (40, 40, 48))
    d = ImageDraw.Draw(im)
    for i, (a, lab) in enumerate(zip(images, labels)):
        r, c = divmod(i, cols)
        tile = Image.fromarray(a[y0:y0 + h, x0:x0 + w]).resize((w * scale, h * scale), Image.NEAREST)
        ox, oy = 4 + c * (tile_w + 4), 4 + r * (tile_h + 4)
        im.paste(tile, (ox, oy + 14))
        d.text((ox + 2, oy + 1), lab, fill=(255, 255, 210))
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    im.save(out)
    return out


if __name__ == "__main__":
    if sys.argv[1:] == ["host"]:
        print(build_host())
