#!/usr/bin/env python3
"""Conservative visual evidence gate for a Play! capture, not a game QA pass.

ImageMagick computes the proportion of visibly lit pixels in a region
fully inside Play!'s render viewport; excludes menu and status bars.
An emulator process that ran 35 seconds with black frames is NOT a
successful graphical boot. Black screens fail while retaining PNG logs.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys


def game_crop(path):
    dimensions = subprocess.check_output(
        ["identify", "-format", "%w %h", str(path)], text=True).split()
    if len(dimensions) != 2:
        raise ValueError("Cannot determine screenshot dimensions")
    width, height = map(int, dimensions)
    if width < 1024 or height < 600:
        raise ValueError(f"Unexpected screenshot dimensions: {width}x{height}")
    # The ROI lies wholly inside the Play! viewport, not its menus.
    return f"{int(width * 0.6875)}x{int(height * 0.77083)}+{int(width * 0.03125)}+{int(height * 0.03472)}"


def visible_fraction(path):
    output = subprocess.check_output(
        ["convert", str(path), "-crop", game_crop(path), "+repage",
         "-colorspace", "gray", "-threshold", "3%",
         "-format", "%[fx:mean]", "info:"],
        text=True, stderr=subprocess.DEVNULL,
    )
    fraction = float(output)
    if not 0.0 <= fraction <= 1.0:
        raise ValueError("Invalid image brightness measurement")
    return fraction


def viewport_signature(path):
    # Check actual cropped RGB pixels rather than full-window PNG bytes:
    # the emulator's status-bar FPS could change while GS output freezes.
    return subprocess.check_output(
        ["convert", str(path), "-crop", game_crop(path), "+repage",
         "-format", "%#", "info:"],
        text=True, stderr=subprocess.DEVNULL,
    ).strip()


def inspect(images, min_visible_fraction=0.002):
    results = [{"image": str(p.name),
                "visible_fraction": visible_fraction(p),
                "game_pixels_sha": viewport_signature(p)}
               for p in images]
    # All screenshots must show visible content. A changing emulator
    # status bar does not count as a changing PS2 frame.
    verdict = all(x["visible_fraction"] >= min_visible_fraction for x in results)
    changed = len({x["game_pixels_sha"] for x in results}) > 1
    return {
        "status": "rendered_pixels_observed" if verdict else "black_viewport",
        "minimum_fraction": min_visible_fraction,
        "viewport_changed": changed,
        "frames": results,
        "limits": "Pixels alone do not prove gameplay, control response or scene progression",
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("images", type=Path, nargs="+")
    p.add_argument("--output", type=Path)
    args = p.parse_args()
    if len(args.images) < 2:
        p.error("Provide at least two independent screenshot captures")
    result = inspect(args.images)
    print(json.dumps(result, indent=2))
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + "\n",
                               encoding="utf-8")
    if result["status"] != "rendered_pixels_observed":
        print("FAIL: emulator loaded an ELF but did not render a visible game.",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"FAIL: Play! screenshot inspection: {exc}", file=sys.stderr)
        sys.exit(1)
