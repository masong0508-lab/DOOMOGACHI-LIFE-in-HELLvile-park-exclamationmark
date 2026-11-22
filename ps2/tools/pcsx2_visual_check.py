#!/usr/bin/env python3
"""Capture visual PCSX2 evidence using an existing locally configured BIOS.

Run on a machine with PCSX2, X11/Xvfb, ImageMagick and a BIOS dumped
from its owner's PS2. No BIOS is included or downloaded. This deliberately
does NOT treat seeing a BIOS or settings UI as a playable game release.
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
from check_play_frames import inspect


def sha256(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("iso", type=Path)
    p.add_argument("--pcsx2", default="pcsx2-qt",
                   help="PCSX2 executable with owner's BIOS already configured")
    p.add_argument("--seconds", type=int, default=45)
    p.add_argument("--out", type=Path, default=Path("ps2/build/pcsx2-smoke"))
    args = p.parse_args()
    if args.seconds < 30:
        p.error("--seconds must be at least 30")
    iso = args.iso.expanduser().resolve()
    if not iso.is_file() or iso.stat().st_size < 65536:
        raise SystemExit("A real PS2 ISO is required")
    if not os.environ.get("DISPLAY"):
        raise SystemExit("DISPLAY is unset (run under X11 or xvfb-run)")
    if not shutil.which(args.pcsx2):
        raise SystemExit(f"PCSX2 executable not installed: {args.pcsx2}")
    for binary in ("import", "convert", "identify"):
        if not shutil.which(binary):
            raise SystemExit(f"Missing ImageMagick command: {binary}")

    out = args.out.expanduser().resolve()
    out.mkdir(parents=True, exist_ok=True)
    log = out / "pcsx2.log"
    command = [args.pcsx2, "-batch", "-fullscreen", "--", str(iso)]
    shots = []
    result = {
        "emulator": "PCSX2 (BIOS configured separately by machine owner)",
        "iso_name": iso.name,
        "iso_sha256": sha256(iso),
        "runtime_seconds_requested": args.seconds,
        "screenshots": [],
        "graphics": {"status": "not_checked"},
        "status": "not_checked",
        "limitations": [
            "PCSX2 must already contain an owner's legitimately dumped BIOS",
            "Visible pixels do not prove that the game rather than BIOS/UI booted",
            "Controls, sound, save/reload, Talk and 3D require a full walkthrough",
        ],
    }
    with log.open("wb") as logfp:
        proc = subprocess.Popen(command, stdout=logfp, stderr=subprocess.STDOUT)
        start = time.monotonic()
        try:
            # Two captures after BIOS/UI initialization. No input is
            # injected: wrong controller mappings must not create false
            # positives. Use manual walkthrough for responsive gameplay.
            for index, when in enumerate((15, args.seconds), 1):
                remaining = start + when - time.monotonic()
                if remaining > 0:
                    time.sleep(remaining)
                if proc.poll() is not None:
                    result["status"] = f"emulator_exited_{proc.returncode}"
                    break
                shot = out / f"frame-{index}.png"
                subprocess.run(
                    ["import", "-window", "root", str(shot)],
                    check=True, timeout=20, stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                )
                if shot.stat().st_size < 1024:
                    result["status"] = "capture_failed"
                    break
                shots.append(shot)
            result["screenshots"] = [x.name for x in shots]
            if len(shots) == 2:
                visual = inspect(shots)
                result["graphics"] = visual
                result["status"] = ("visible_image_needs_gameplay_review"
                                    if visual["status"] == "rendered_pixels_observed"
                                    else "black_display")
        finally:
            (out / "report.json").write_text(
                json.dumps(result, indent=2) + "\n", encoding="utf-8"
            )
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=6)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
    print(json.dumps(result, indent=2))
    return 0 if result["status"] == "visible_image_needs_gameplay_review" else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"PCSX2 visual probe failed: {exc}", file=sys.stderr)
        sys.exit(1)
