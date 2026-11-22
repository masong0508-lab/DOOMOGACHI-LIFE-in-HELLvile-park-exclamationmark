#!/usr/bin/env python3
"""Check that input changes Story progress in aligned Play! captures.

The default compares the two diagnostic ELFs with and without synthetic
buttons. With --controller, require two captures of the same gameplay ISO
with and without Qt keyboard input entering Play!'s virtual controller.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
from check_story_progress import _sample, _changed, inspect_story

MIN_CONTROL_DIVERGENCE = 0.35


def check_controller_reports(active, idle):
    def load_report(shots):
        report = json.loads((shots[0].parent / "report.json").read_text(encoding="utf-8"))
        if report.get("boot_media_type") != "disc" or report.get("pngs") != [p.name for p in shots]:
            raise ValueError("Controller captures must come from a single disc run")
        if any(p.parent != shots[0].parent for p in shots):
            raise ValueError("Controller frames must share their capture directory")
        return report

    driven, control = load_report(active), load_report(idle)
    sha = driven.get("boot_image_sha256", "")
    if not isinstance(sha, str) or not re.fullmatch(r"[0-9a-f]{64}", sha) or sha != control.get("boot_image_sha256"):
        raise ValueError("Controller and idle runs must boot the same ISO")
    offsets = driven.get("capture_offsets_seconds")
    if (not isinstance(offsets, list) or len(offsets) != 3
            or any(type(t) is not int for t in offsets) or offsets != control.get("capture_offsets_seconds")):
        raise ValueError("Controller and idle runs must capture at the same times")
    keys = driven.get("x11_key_events", [])
    if (control.get("x11_key_events") != [] or not isinstance(keys, list)
            or any(not isinstance(e, dict) or type(e.get("seconds")) is not int for e in keys)
            or [e.get("key") for e in keys] != ["Return", "z", "z"]
            or not offsets[0] < keys[0]["seconds"] < keys[1]["seconds"] < keys[2]["seconds"] < offsets[1]):
        raise ValueError("Expected START/CROSS/CROSS between the first two frames and no keys in the idle run")
    return sha


def inspect_causality(scripted, idle, threshold=MIN_CONTROL_DIVERGENCE, controller=False):
    if len(scripted) != 3 or len(idle) != 3:
        raise ValueError("Need three aligned screenshots from each run")
    if not 0 < threshold <= 1:
        raise ValueError("Invalid causal comparison threshold")
    if len({str(Path(p).resolve()) for p in scripted + idle}) != 6:
        raise ValueError("All six capture images must be distinct")
    sha = check_controller_reports(scripted, idle) if controller else None
    active = [_sample(p) for p in scripted]
    baseline = [_sample(p) for p in idle]
    changes = [_changed(a, b) for a, b in zip(active, baseline)]
    if controller:
        # Both post-input captures can land in the same Story cutscene.
        # Require departure from the opening and divergence from idle at
        # both capture times so title animation and autoplay cannot pass.
        departed = all(_changed(active[0], frame) >= threshold for frame in active[1:])
        progression = "left_opening" if departed else "opening_departure_not_proven"
    else:
        progression = inspect_story(scripted)["status"]
        departed = progression == "large_scene_transitions_observed"
    passes = (
        departed
        and changes[0] < threshold and changes[1] >= threshold and changes[2] >= threshold
    )
    success = "controller_input_changes_scenes" if controller else "scripted_input_changes_scenes"
    return {
        "status": success if passes else "input_causality_not_proven",
        "input_source": "play_qt_virtual_pad" if controller else "diagnostic_elf",
        "iso_sha256": sha,
        "minimum_control_divergence": threshold,
        "scripted_progression": progression,
        "aligned_frame_difference": {
            "before_input": round(changes[0], 6),
            "after_first_input": round(changes[1], 6),
            "later_story": round(changes[2], 6),
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scripted", nargs=3, required=True, type=Path)
    parser.add_argument("--idle", nargs=3, required=True, type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--controller", action="store_true", help="Validate two captures of the same ISO")
    args = parser.parse_args()
    result = inspect_causality(args.scripted, args.idle, controller=args.controller)
    print("PS2_STORY_INPUT_CAUSALITY " + json.dumps(result, sort_keys=True), flush=True)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if result["status"] == "input_causality_not_proven":
        print("FAIL: input has not been proven to cause scene changes", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"FAIL: PS2 Story causal-control check: {exc}", file=sys.stderr)
        sys.exit(1)
