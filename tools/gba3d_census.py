#!/usr/bin/env python3
"""Report the GBA 3D census without claiming the unfinished FPS target is met.

Usage: python3 tools/gba3d_census.py gba/build/gba3d_census.log
       [--csv gba/build/gba3d_census.csv]
The test harness enforces logic >=29; FPS 22/30 remain report-only until achieved.
Every mapped GBA Breach entry in tools/breach_scripts.py must have two camera windows.
"""
import argparse
import csv
import re
import sys
from pathlib import Path
from breach_scripts import LEVELS, GBA_3D_PILOTS

PATTERN = re.compile(
    r"^harness: fps census (\w+) (first|third): "
    r"([0-9.]+) logic ticks/s, ([0-9.]+) drawn frames/s, "
    r"work avg ([0-9.]+) ms max ([0-9.]+) ms"
)
COLUMNS = ("level", "camera", "logic_tps", "drawn_fps", "work_avg_ms",
           "work_max_ms", "ms_to_30_work", "fps_to_22", "fps_to_30")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("log", type=Path)
    ap.add_argument("--csv", type=Path)
    args = ap.parse_args()
    expected = {(slug, cam) for _id, slug, _title in LEVELS + GBA_3D_PILOTS
                for cam in ("first", "third")}
    expected |= {("arena", "first"), ("arena", "third")}
    rows = {}
    try:
        log = args.log.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print("3D census: cannot read log:", exc, file=sys.stderr)
        return 1
    for line in log.splitlines():
        match = PATTERN.match(line)
        if not match:
            continue
        slug, camera, ltps, fps, work, maximum = match.groups()
        key = (slug, camera)
        if key in rows:
            print("3D census: duplicate window:", key, file=sys.stderr)
            return 1
        drawn = float(fps)
        avg = float(work)
        rows[key] = {
            "level": slug, "camera": camera,
            "logic_tps": float(ltps), "drawn_fps": drawn,
            "work_avg_ms": avg, "work_max_ms": float(maximum),
            "ms_to_30_work": round(max(0.0, avg - 1000.0 / 30.0), 2),
            "fps_to_22": round(max(0.0, 22.0 - drawn), 1),
            "fps_to_30": round(max(0.0, 30.0 - drawn), 1),
        }
    missing = expected - set(rows)
    unexpected = set(rows) - expected
    if missing or unexpected:
        print("3D census: missing windows:", sorted(missing), file=sys.stderr)
        print("3D census: unexpected windows:", sorted(unexpected), file=sys.stderr)
        return 1
    ordered = sorted(rows.values(), key=lambda r: (r["drawn_fps"], r["level"], r["camera"]))
    n22 = sum(r["drawn_fps"] >= 22 for r in ordered)
    n30 = sum(r["drawn_fps"] >= 29.5 for r in ordered)
    print("Mapped GBA Breach levels: %d (legacy %d, pilots %d); plus arena; camera windows: %d" %
          (len(LEVELS) + len(GBA_3D_PILOTS), len(LEVELS), len(GBA_3D_PILOTS), len(ordered)))
    print("Interim >=22 drawn FPS: %d/%d; final ~30 drawn FPS (>=29.5): %d/%d" %
          (n22, len(ordered), n30, len(ordered)))
    print("| Level | Camera | Drawn FPS | Logic TPS | Work avg/max ms | Work ms to 30 | FPS shortfall to 30 |")
    print("|---|---|---:|---:|---:|---:|---:|")
    for r in ordered:
        print("| %s | %s | %.1f | %.1f | %.1f / %.1f | %.1f | %.1f |" %
              (r["level"], r["camera"], r["drawn_fps"], r["logic_tps"],
               r["work_avg_ms"], r["work_max_ms"], r["ms_to_30_work"], r["fps_to_30"]))
    print("All-pass status: %s" % ("TARGET REACHED" if n30 == len(ordered) else "NOT YET"))
    print("Scope: every GBA-listed map including pilots, plus arena; unbuilt no-map levels excluded.")
    print("Other scripted 3D encounters and alternate detail modes retain separate tests.")
    if args.csv:
        args.csv.parent.mkdir(parents=True, exist_ok=True)
        with args.csv.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=COLUMNS)
            writer.writeheader()
            writer.writerows(ordered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
