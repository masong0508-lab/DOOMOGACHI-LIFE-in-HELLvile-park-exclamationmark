#!/usr/bin/env python3
"""Run the experimental game ISO in BIOS-free Play! under Xvfb.

The process staying alive and rendering screenshots establishes a real
emulator smoke attempt, NOT a complete gameplay/playability result.
The two PNGs plus stdout are preserved for manual review in Actions.
This script requires Play! and ImageMagick's import command.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from probe_log import summarize


def main():
    p = argparse.ArgumentParser()
    p.add_argument("iso", type=Path)
    p.add_argument("--out", type=Path, default=Path("ps2/build/play-smoke"))
    p.add_argument("--seconds", type=int, default=35)
    args = p.parse_args()
    iso = args.iso.resolve()
    if not iso.is_file() or iso.stat().st_size < 4096:
        raise SystemExit("Missing or unusually small PS2 gameplay ISO")
    if args.seconds < 15:
        raise SystemExit("At least 15 seconds needed for a meaningful boot probe")
    if not os.environ.get("DISPLAY"):
        raise SystemExit("DISPLAY is unset: run with xvfb-run (or an X11 desktop)")
    for binary in ("flatpak", "import"):
        if not shutil.which(binary):
            raise SystemExit(f"Missing {binary}")
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    logs = out / "emulator.log"
    shots = []
    cmd = [
        "flatpak", "run", "--user",
        f"--filesystem={iso.parent}:ro", "org.purei.Play",
        "--disc", str(iso),
    ]
    with logs.open("wb") as logfile:
        proc = subprocess.Popen(cmd, stdout=logfile, stderr=subprocess.STDOUT)
        started = time.monotonic()
        try:
            for index, offset in enumerate((12, args.seconds), 1):
                delay = started + offset - time.monotonic()
                if delay > 0:
                    time.sleep(delay)
                if proc.poll() is not None:
                    print(f"Play! exited early ({proc.returncode}); see {logs}", file=sys.stderr)
                    return 1
                png = out / f"boot-{index}.png"
                shot = subprocess.run(
                    ["import", "-window", "root", str(png)],
                    capture_output=True, text=True, timeout=20,
                )
                if shot.returncode != 0 or not png.is_file() or png.stat().st_size < 1024:
                    print(f"Screenshot {index} failed: {shot.stderr}", file=sys.stderr)
                    return 1
                shots.append(png)
            logfile.flush()
            telemetry = summarize(logs.read_text(encoding="utf-8", errors="replace"))
            evidence = {
                "emulator": "Play! built-in HLE BIOS",
                "ee_console_telemetry": telemetry,
                "iso_name": iso.name,
                "iso_sha256": hashlib.sha256(iso.read_bytes()).hexdigest(),
                "seconds_observed": args.seconds,
                "pngs": [s.name for s in shots],
                "result": "emulator process and two screenshots captured",
                "limitations": [
                    "Not a proof that the game rendered rather than the emulator UI",
                    "No controller, Talk, SPU2, 3D or save/reload playthrough verified",
                ],
            }
            (out / "report.json").write_text(
                json.dumps(evidence, indent=2) + "\n", encoding="utf-8"
            )
            print(f"Play! process survived {args.seconds} seconds; screenshots: {shots}")
            if telemetry["status"] in (
                "invalid_upload_count", "no_tick_progress", "non_monotonic"
            ):
                print("FAIL: EE console reported invalid or stalled simulation ticks.")
                return 1
            print("PASS: emulator process/screenshot smoke ONLY, not gameplay fidelity.")
            return 0
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=6)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()


if __name__ == "__main__":
    sys.exit(main())
