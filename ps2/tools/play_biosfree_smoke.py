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
from check_play_frames import inspect


# Play!'s Qt frontend binds Return=START and Z=CROSS by default when
# no user pad bindings exist. These X11 events enter the UNMODIFIED
# game ISO through Play!'s real virtual PADMAN; never confuse them with
# direct PS2_SCRIPTED_STORY_PROBE input in the test-only ELF.
X11_TEST_KEYS = frozenset(("Return", "z", "x", "Up", "Down", "Left", "Right"))
X11_KEY_HOLD_SECONDS = 0.18


def parse_x11_key_events(raw, duration):
    events = []
    previous = -1
    for item in raw:
        key, sep, seconds = item.partition("@")
        if not sep or key not in X11_TEST_KEYS or not seconds.isdecimal():
            raise ValueError("Invalid --x11-key; expected Return@6 or z@8")
        when = int(seconds)
        if when <= 0 or when >= duration or when <= previous:
            raise ValueError("X11 keys must be strictly increasing, after start and before capture end")
        events.append((when, key))
        previous = when
    return events


def send_x11_key(key):
    """Deliver a Qt keyboard event to Play!'s visible main window, not the root."""
    result = subprocess.run(["xdotool", "search", "--onlyvisible", "--name", "Play"],
                            text=True, capture_output=True, timeout=8)
    windows = result.stdout.split() if result.returncode == 0 else []
    if not windows:
        raise RuntimeError("No visible Play! Qt window for X11 keyboard input")
    last_error = ""
    for wid in reversed(windows):
        try:
            subprocess.run(["xdotool", "windowfocus", "--sync", wid],
                           capture_output=True, check=True, timeout=8)
            # Keep the button down across several 30 Hz input polls.
            try:
                subprocess.run(["xdotool", "keydown", "--clearmodifiers", key],
                               capture_output=True, check=True, timeout=8)
                time.sleep(X11_KEY_HOLD_SECONDS)
            finally:
                subprocess.run(["xdotool", "keyup", key],
                               capture_output=True, check=True, timeout=8)
            return wid
        except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
            last_error = str(exc)
    raise RuntimeError("Could not focus/send X11 key to Play!: " + last_error)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("iso", type=Path, help="Game ISO or diagnostic EE ELF")
    p.add_argument("--launch", choices=("disc", "elf"), default="disc",
                   help="Use --elf to isolate emulator/GS bring-up from ISO files")
    p.add_argument("--out", type=Path, default=Path("ps2/build/play-smoke"))
    p.add_argument("--seconds", type=int, default=35)
    p.add_argument("--software-gl", action="store_true",
                   help="Force Mesa llvmpipe within Flatpak for headless Xvfb")
    p.add_argument("--require-motion", action="store_true",
                   help="For GS animation diagnostics, require changed viewport pixels")
    p.add_argument("--early-capture", type=int, default=0, metavar="SECOND",
                   help="Optional extra early PNG before the middle and final captures")
    p.add_argument("--middle-capture", type=int, default=12, metavar="SECOND",
                   help="Time for the middle capture (default: 12 seconds)")
    p.add_argument("--x11-key", action="append", default=[], metavar="KEY@SECOND",
                   help="EXPERIMENTAL Play! Qt keyboard injection (Return=START, z=CROSS); repeat for more keys")
    args = p.parse_args()
    try:
        key_events = parse_x11_key_events(args.x11_key, args.seconds)
    except ValueError as exc:
        p.error(str(exc))
    iso = args.iso.resolve()
    if not iso.is_file() or iso.stat().st_size < 4096:
        raise SystemExit("Missing or unusually small PS2 boot image")
    if args.launch == "elf" and iso.open("rb").read(4) != b"\x7fELF":
        raise SystemExit("--launch elf requires a native PS2 EE executable")
    if args.seconds < 15:
        raise SystemExit("At least 15 seconds needed for a meaningful boot probe")
    if not 0 <= args.early_capture < args.middle_capture < args.seconds:
        p.error("Require 0 <= early capture < middle capture < final capture")
    if not os.environ.get("DISPLAY"):
        raise SystemExit("DISPLAY is unset: run with xvfb-run (or an X11 desktop)")
    for binary in (("flatpak", "import", "xdotool") if key_events else ("flatpak", "import")):
        if not shutil.which(binary):
            raise SystemExit(f"Missing {binary}")
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    logs = out / "emulator.log"
    shots = []
    flatpak_flags = []
    if args.software_gl:
        # The hosted runner's Xvfb session may expose no hardware DRI
        # device. Force a real llvmpipe OpenGL context rather than silently
        # producing a black application surface.
        flatpak_flags = [
            "--env=LIBGL_ALWAYS_SOFTWARE=1",
            "--env=GALLIUM_DRIVER=llvmpipe",
            "--env=MESA_LOADER_DRIVER_OVERRIDE=llvmpipe",
        ]
    cmd = [
        "flatpak", "run", "--user", *flatpak_flags,
        f"--filesystem={iso.parent}:ro", "org.purei.Play",
        "--disc" if args.launch == "disc" else "--elf", str(iso),
    ]
    with logs.open("wb") as logfile:
        proc = subprocess.Popen(cmd, stdout=logfile, stderr=subprocess.STDOUT)
        started = time.monotonic()
        try:
            offsets = (args.middle_capture, args.seconds)
            if args.early_capture:
                offsets = (args.early_capture,) + offsets
            # Combine X11 key deliveries and screenshots on the SAME
            # monotonic clock. No input is injected into the game code.
            events = [(when, 0, "screenshot", i) for i, when in enumerate(offsets, 1)]
            events += [(when, 1, "x11_key", key) for when, key in key_events]
            for offset, _, kind, value in sorted(events):
                delay = started + offset - time.monotonic()
                if delay > 0:
                    time.sleep(delay)
                if proc.poll() is not None:
                    print(f"Play! exited early ({proc.returncode}); see {logs}", file=sys.stderr)
                    return 1
                if kind == "x11_key":
                    try:
                        wid = send_x11_key(value)
                    except (RuntimeError, OSError, subprocess.TimeoutExpired) as exc:
                        print(f"FAIL: X11 keyboard injection: {exc}", file=sys.stderr)
                        return 1
                    print("PLAY_X11_KEY " + json.dumps({
                        "at_seconds": offset, "key": value, "target_window": wid,
                        "proof": "X11 event sent; PS2 pad response not yet verified",
                    }), flush=True)
                    continue
                index = value
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
            visual = inspect(shots)
            evidence = {
                "emulator": "Play! built-in HLE BIOS",
                "ee_console_telemetry": telemetry,
                "visual_capture": visual,
                "boot_media_type": args.launch,
                "forced_mesa_software_gl": args.software_gl,
                "boot_image": iso.name,
                "boot_image_sha256": hashlib.sha256(iso.read_bytes()).hexdigest(),
                "seconds_observed": args.seconds,
                "capture_offsets_seconds": list(offsets),
                "x11_key_events": [{"seconds": t, "key": k} for t, k in key_events],
                "pngs": [s.name for s in shots],
                "result": f"emulator process and {len(shots)} screenshots captured",
                "limitations": [
                    "Not a proof that the game rendered rather than the emulator UI",
                    ("X11 keyboard events sent to Play! Qt; PADMAN response requires independent scene comparison" if key_events
                     else "No controller input, Talk, SPU2, 3D or save/reload playthrough verified"),
                ],
            }
            (out / "report.json").write_text(
                json.dumps(evidence, indent=2) + "\n", encoding="utf-8"
            )
            # Surface the actual frame ratios and EE evidence in CI logs.
            # Previously they were visible only inside the artifact ZIP,
            # making a green GS-only probe easy to overinterpret.
            print("PLAY_FRAME_CHECK " + json.dumps({
                "status": visual["status"],
                "viewport_changed": visual["viewport_changed"],
                "frames": visual["frames"],
            }, sort_keys=True), flush=True)
            print("PS2_EE_TELEMETRY " + json.dumps({
                "status": telemetry["status"],
                "count": telemetry["count"],
                "frame_hash_changed": telemetry["frame_hash_changed"],
            }, sort_keys=True), flush=True)
            print(f"Play! process survived {args.seconds} seconds; screenshots: {shots}")
            if visual["status"] == "black_viewport":
                print("FAIL: screenshots show an effectively black game viewport.",
                      file=sys.stderr)
                return 1
            if args.require_motion and not visual["viewport_changed"]:
                print("FAIL: GS-only probe has visible but frozen viewport pixels.",
                      file=sys.stderr)
                return 1
            if telemetry["status"] in (
                "invalid_upload_count", "no_tick_progress", "non_monotonic"
            ):
                print("FAIL: EE console reported invalid or stalled simulation ticks.")
                return 1
            print(f"PASS: visible game viewport in {len(shots)} frames; gameplay functionality NOT verified.")
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
