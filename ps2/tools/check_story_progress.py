#!/usr/bin/env python3
"""Conservative visual gate for three Play! scripted Story screenshots.

Reject a title screen that merely animates. Compare RGB changes within the
emulator's game viewport (never its moving FPS/status bar). This is evidence
of large scripted scene transitions, NOT physical pad, audio or save testing.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from check_play_frames import game_crop

SAMPLE_W, SAMPLE_H = 176, 111
PIXEL_CHANNEL_DELTA = 24
MIN_CHANGED_FRACTION = 0.35


def _sample(path):
    """Sample the real viewport with ImageMagick; no extra Python packages."""
    data = subprocess.run(
        ["convert", str(path), "-crop", game_crop(path), "+repage",
         "-filter", "point", "-resize", f"{SAMPLE_W}x{SAMPLE_H}!",
         "-depth", "8", "RGB:-"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
    ).stdout
    expected = SAMPLE_W * SAMPLE_H * 3
    if len(data) != expected:
        raise ValueError(f"RGB sample length {len(data)} differs from {expected}")
    return data


def _changed(a, b):
    if len(a) != len(b) or len(a) % 3:
        raise ValueError("Mismatched RGB samples")
    changed = 0
    for i in range(0, len(a), 3):
        if any(abs(a[i+c] - b[i+c]) > PIXEL_CHANNEL_DELTA for c in range(3)):
            changed += 1
    return changed / (len(a) // 3)


def inspect_story(images, min_changed_fraction=MIN_CHANGED_FRACTION):
    if len(images) != 3:
        raise ValueError("Exactly three opening/story screenshots are required")
    if not 0 < min_changed_fraction <= 1:
        raise ValueError("Invalid scene-change threshold")
    if len({str(Path(p).resolve()) for p in images}) != 3:
        raise ValueError("Capture frames must be distinct files")
    before, middle, later = [_sample(p) for p in images]
    values = [_changed(before, middle), _changed(middle, later),
              _changed(before, later)]
    passed = all(f >= min_changed_fraction for f in values[:2])
    return {
        "status": "large_scene_transitions_observed" if passed else "scene_transition_not_proven",
        "minimum_changed_fraction": min_changed_fraction,
        "changed_fraction": {
            "opening_to_middle": round(values[0], 6),
            "middle_to_later": round(values[1], 6),
            "opening_to_later": round(values[2], 6),
        },
        "images": [Path(p).name for p in images],
        "limits": "Synthetic input/scene changes do not verify real controller input, gameplay choices, SPU2 audio, Talk or persistent saves",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("images", nargs=3, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = inspect_story(args.images)
    print("PS2_STORY_PROGRESSION " + json.dumps(result, sort_keys=True), flush=True)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if result["status"] != "large_scene_transitions_observed":
        print("FAIL: Play! screenshots do not establish Story scene progression", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"FAIL: Play! scripted Story visual check: {exc}", file=sys.stderr)
        sys.exit(1)
