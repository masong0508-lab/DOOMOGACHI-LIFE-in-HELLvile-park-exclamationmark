#!/usr/bin/env python3
"""Controlled Play! experiment: did scripted input change the actual scenes?

Two *non-shipping* PS2 game-core ELFs are compared at identical screenshot
offsets: one injects START/A/A, the other injects no input at any time.
Both execute the same initialization, game logic and renderer. Large viewport
differences after input, in addition to intra-run scene changes, provide
stronger evidence than title animation alone. This does NOT prove physical
DualShock handling, audio, Talk, or memory-card saves.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from check_story_progress import _sample, _changed, inspect_story

MIN_CONTROL_DIVERGENCE = 0.35


def inspect_causality(scripted, idle, threshold=MIN_CONTROL_DIVERGENCE):
    if len(scripted) != 3 or len(idle) != 3:
        raise ValueError("Need three aligned screenshots from each run")
    if not 0 < threshold <= 1:
        raise ValueError("Invalid causal comparison threshold")
    if len({str(Path(p).resolve()) for p in scripted + idle}) != 6:
        raise ValueError("All six capture images must be distinct")
    progression = inspect_story(scripted)
    active = [_sample(p) for p in scripted]
    baseline = [_sample(p) for p in idle]
    changes = [_changed(a,b) for a,b in zip(active,baseline)]
    passes = (
        progression["status"] == "large_scene_transitions_observed"
        and changes[1] >= threshold and changes[2] >= threshold
    )
    return {
        "status": "scripted_input_changes_scenes" if passes else "input_causality_not_proven",
        "minimum_control_divergence": threshold,
        "scripted_progression": progression["status"],
        "aligned_frame_difference": {
            "before_input": round(changes[0], 6),
            "after_first_input": round(changes[1], 6),
            "later_story": round(changes[2], 6),
        },
        "notes": "Compared Play! display pixels between two game-core ELFs, differing only in synthetic button input. No real controller, audio, Talk or save testing.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scripted", nargs=3, required=True, type=Path)
    parser.add_argument("--idle", nargs=3, required=True, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = inspect_causality(args.scripted, args.idle)
    print("PS2_STORY_INPUT_CAUSALITY " + json.dumps(result, sort_keys=True), flush=True)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if result["status"] != "scripted_input_changes_scenes":
        print("FAIL: scripted input has not been proven to cause scene changes", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"FAIL: PS2 Story causal-control check: {exc}", file=sys.stderr)
        sys.exit(1)
