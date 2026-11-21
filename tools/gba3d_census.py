#!/usr/bin/env python3
"""Report the real-game GBA 3D census and its worst frame-time spikes.

Usage: python3 tools/gba3d_census.py gba/build/gba3d_census.log
       [--csv gba/build/gba3d_census.csv] [--require-worklog]
Logic >=29 TPS is gated by the harness. The whole-census 22/30 FPS goals
and Bone Cathedral / Bone Capital 24 FPS interim goal remain report-only.
Every mapped GBA Breach entry must have first- and third-person windows.

With --worklog, the existing harness reports the work of each presented frame;
gaps (vblanks between presentations) are collected by the harness regardless
of --worklog. Neither measurement modifies the game ROM or its rendered pixels.
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
WORK = re.compile(r"^harness: work (\d+) ([0-9.]+)$")
GAPS = re.compile(
    r"^harness: gaps census (\w+) (first|third): "
    r"vblanks-per-frame histogram(.*?) max (\d+) \(([0-9.]+) ms\) at tick (\d+)$"
)
GAP_BIN = re.compile(r"^(\d+)\+?:(\d+)$")

COLUMNS = ("level", "camera", "logic_tps", "drawn_fps", "work_avg_ms",
           "work_max_ms", "ms_to_30_work", "fps_to_22", "fps_to_24", "fps_to_30",
           "work_samples", "work_p50_ms", "work_p90_ms", "work_p95_ms",
           "work_p99_ms", "work_worst_tick", "work_over_33ms",
           "work_over_45ms", "work_over_50ms", "gap_3plus", "gap_4plus",
           "gap_max_vblanks", "gap_worst_tick")


def percentile(samples, pct):
    """Nearest-rank percentile of per-presented-frame work (milliseconds)."""
    return samples[max(0, (len(samples) * pct + 99) // 100 - 1)]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("log", type=Path)
    ap.add_argument("--csv", type=Path)
    ap.add_argument("--require-worklog", action="store_true",
                    help="fail if any scene has no per-frame work or gap data")
    args = ap.parse_args()
    expected = {(slug, cam) for _id, slug, _title in LEVELS + GBA_3D_PILOTS
                for cam in ("first", "third")}
    expected |= {("arena", "first"), ("arena", "third")}
    rows = {}
    window_work = []
    try:
        log = args.log.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print("3D census: cannot read log:", exc, file=sys.stderr)
        return 1
    for line in log.splitlines():
        work = WORK.match(line)
        if work:
            window_work.append((int(work.group(1)), float(work.group(2))))
            continue
        gaps = GAPS.match(line)
        if gaps:
            key = (gaps.group(1), gaps.group(2))
            if key in rows:
                counts = {}
                for token in gaps.group(3).split():
                    found = GAP_BIN.match(token)
                    if found:
                        b, n = map(int, found.groups())
                        counts[b] = counts.get(b, 0) + n
                rows[key].update({
                    "gap_3plus": sum(n for b, n in counts.items() if b >= 3),
                    "gap_4plus": sum(n for b, n in counts.items() if b >= 4),
                    "gap_max_vblanks": int(gaps.group(4)),
                    "gap_worst_tick": int(gaps.group(6)),
                })
            continue
        if not line.startswith("harness: fps "):
            continue
        match = PATTERN.match(line)
        if not match:
            # "fps start" resets the observation window. Never mix work
            # samples from loading, the previous scene, or the other camera.
            window_work.clear()
            continue
        slug, camera, ltps, fps, work, maximum = match.groups()
        key = (slug, camera)
        if key in rows:
            print("3D census: duplicate window:", key, file=sys.stderr)
            return 1
        drawn, avg = float(fps), float(work)
        row = {
            "level": slug, "camera": camera,
            "logic_tps": float(ltps), "drawn_fps": drawn,
            "work_avg_ms": avg, "work_max_ms": float(maximum),
            "ms_to_30_work": round(max(0.0, avg - 1000.0 / 30.0), 2),
            "fps_to_22": round(max(0.0, 22.0 - drawn), 1),
            "fps_to_24": round(max(0.0, 24.0 - drawn), 1),
            "fps_to_30": round(max(0.0, 30.0 - drawn), 1),
        }
        if window_work:
            values = sorted(ms for _tick, ms in window_work)
            worst_tick = max(window_work, key=lambda sample: sample[1])[0]
            row.update({
                "work_samples": len(values),
                "work_p50_ms": percentile(values, 50),
                "work_p90_ms": percentile(values, 90),
                "work_p95_ms": percentile(values, 95),
                "work_p99_ms": percentile(values, 99),
                "work_worst_tick": worst_tick,
                "work_over_33ms": sum(ms > 1000 / 30 for ms in values),
                "work_over_45ms": sum(ms > 1000 / 22 for ms in values),
                "work_over_50ms": sum(ms > 50 for ms in values),
            })
        rows[key] = row
        window_work.clear()
    missing = expected - set(rows)
    unexpected = set(rows) - expected
    if missing or unexpected:
        print("3D census: missing windows:", sorted(missing), file=sys.stderr)
        print("3D census: unexpected windows:", sorted(unexpected), file=sys.stderr)
        return 1
    ordered = sorted(rows.values(), key=lambda r: (r["drawn_fps"], r["level"], r["camera"]))
    if args.require_worklog:
        uninstrumented = [(r["level"], r["camera"]) for r in ordered
                          if not r.get("work_samples") or "gap_max_vblanks" not in r]
        if uninstrumented:
            print("3D census: missing worklog/gaps:", uninstrumented, file=sys.stderr)
            return 1
    n22 = sum(r["drawn_fps"] >= 22 for r in ordered)
    n30 = sum(r["drawn_fps"] >= 29.5 for r in ordered)
    print("Mapped GBA Breach levels: %d (legacy %d, pilots %d); plus arena; camera windows: %d" %
          (len(LEVELS) + len(GBA_3D_PILOTS), len(LEVELS), len(GBA_3D_PILOTS), len(ordered)))
    print("Interim >=22 drawn FPS: %d/%d; final ~30 drawn FPS (>=29.5): %d/%d" %
          (n22, len(ordered), n30, len(ordered)))
    bone = [r for r in ordered if r["level"] in ("cathedral", "bone_capital")]
    bone24 = sum(r["drawn_fps"] >= 24 for r in bone)
    print("Bone Cathedral + Bone Capital interim >=24 drawn FPS: %d/%d (both cameras; report-only)" %
          (bone24, len(bone)))
    for r in sorted(bone, key=lambda r: (r["level"], r["camera"])):
        print("Bone 24 target: %-12s %-5s %.1f drawn FPS; %.1f FPS short; %.1f ms avg work; %.1f ms P99 work" %
              (r["level"], r["camera"], r["drawn_fps"], r["fps_to_24"],
               r["work_avg_ms"], r.get("work_p99_ms", 0.0)))
    print("| Level | Camera | Drawn FPS | Logic TPS | Work avg/max ms | Work ms to 30 | FPS shortfall to 30 |")
    print("|---|---|---:|---:|---:|---:|---:|")
    for r in ordered:
        print("| %s | %s | %.1f | %.1f | %.1f / %.1f | %.1f | %.1f |" %
              (r["level"], r["camera"], r["drawn_fps"], r["logic_tps"],
               r["work_avg_ms"], r["work_max_ms"], r["ms_to_30_work"], r["fps_to_30"]))
    print("All-pass status: %s" % ("TARGET REACHED" if n30 == len(ordered) else "NOT YET"))
    print("Scope: every GBA-listed map including pilots, plus arena; unbuilt no-map levels excluded.")
    print("Other scripted 3D encounters and alternate detail modes retain separate tests.")
    if all("work_samples" in r and "gap_max_vblanks" in r for r in ordered):
        print("### GBA 3D worst-frame diagnostics (work ms are per presented frame)")
        print("| Level | Camera | P95 / P99 work ms | >33 / >45 / >50 ms | 3+ / 4+ vblank gaps | Worst work tick | Worst gap tick |")
        print("|---|---|---:|---:|---:|---:|---:|")
        for r in sorted(ordered, key=lambda x: (-x["work_p99_ms"], -x["gap_4plus"])):
            print("| %s | %s | %.1f / %.1f | %d / %d / %d | %d / %d | %d | %d |" %
                  (r["level"], r["camera"], r["work_p95_ms"], r["work_p99_ms"],
                   r["work_over_33ms"], r["work_over_45ms"], r["work_over_50ms"],
                   r["gap_3plus"], r["gap_4plus"], r["work_worst_tick"],
                   r["gap_worst_tick"]))
    if args.csv:
        args.csv.parent.mkdir(parents=True, exist_ok=True)
        with args.csv.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=COLUMNS)
            writer.writeheader()
            writer.writerows(ordered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
