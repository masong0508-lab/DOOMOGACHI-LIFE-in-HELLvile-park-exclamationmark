#!/usr/bin/env python3
"""run.py - run the PS1 build headless in PCSX-Redux with scripted input.

Takes a script in the SAME format as the core native runner
(core/build/runner --script FILE). Times are GAME FRAMES (30 Hz logic
ticks), counted with the game's own frame counter, so a script drives the
PS1 build through exactly the same game states as the native runner even
when a scene renders below 30 fps:

    # comment
    wait 30            run 30 game frames
    press A [B ..]     hold for 1 frame, then 1 frame released
    hold RIGHT 20      hold for 20 frames, then release
    down A / up A      press and keep holding / release one button
    release [A]        release one / every button
    shot name          screenshot -> OUT/name.png
    scene NAME [ARG]   scene_set()   } run inside the game between two
    push NAME [ARG]    scene_push()  } frames through the g_hcmd mailbox
    expect-scene NAME  fail unless the top scene is NAME   (src/hcmd.c),
    save N / load N / expect-save N  save_game() / load_game() / save_exists()
    type "text"        feed the open text entry, 1 char/frame (\n = OK)
    log TEXT           print TEXT on the game's TTY
    no-stubs           fail if a core "STUB:" was logged before this line
  PS1 extensions:
    vram name          full 1024x512 VRAM dump -> OUT/name_vram.png
    fps name / fpsend name   measure the presented frame rate
    expect-log TEXT    fail unless TEXT appears in the TTY log

Buttons: A B X Y L R L2 R2 START SELECT UP DOWN LEFT RIGHT
(PS1: Cross Circle Square Triangle L1 R1 L2 R2 Start Select D-pad).

usage:
  run.py --exe build/doomogachi.exe --script S --out DIR [options]
  run.py --cue build/doomogachi.cue --script S --out DIR   (boot the disc)
options:
  --map FILE        symbol map (default: <exe dir>/doomogachi.map) for the
                    g_frame heartbeat used for hang detection and fps
  --card FILE       memory card image for slot 1 (default: fresh OUT/card1.mcd)
  --keep-card       do not delete the card image before the run
  --timeout SEC     wall-clock limit for the emulator (default 300)
  --hang VSYNCS     vsyncs without a new frame = hang (default 900)
  --expect TEXT     fail unless TEXT appears in the TTY log (repeatable)
  --assert-no-stubs fail if any core stub ran (like the native runner)
  --emu PATH        pcsx-redux binary (default /opt/ps1emu/pcsx-redux)
  --bios PATH       BIOS image (default /opt/ps1emu/openbios.bin)

Writes OUT/tty.log (everything the game printed), OUT/*.png and
OUT/harness_results.txt. Exit code 0 = pass, 1 = failure (hang, crash,
missing expected output). Stdlib only.
"""
import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
BUTTONS = {
    "A": "CROSS", "B": "CIRCLE", "X": "SQUARE", "Y": "TRIANGLE",
    "L": "L1", "R": "R1", "L1": "L1", "R1": "R1", "L2": "L2", "R2": "R2",
    "START": "START", "SELECT": "SELECT",
    "UP": "UP", "DOWN": "DOWN", "LEFT": "LEFT", "RIGHT": "RIGHT",
    "CROSS": "CROSS", "CIRCLE": "CIRCLE", "SQUARE": "SQUARE", "TRIANGLE": "TRIANGLE",
}
VSYNC_PER_TICK = 2


def write_png(path, w, h, rgb):
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    raw = bytearray()
    stride = w * 3
    for y in range(h):
        raw.append(0)
        raw += rgb[y * stride:(y + 1) * stride]
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 6)))
        f.write(chunk(b"IEND", b""))


def bgr555_to_rgb(data, w, h):
    out = bytearray(w * h * 3)
    n = min(len(data) // 2, w * h)
    for i in range(n):
        v = data[2 * i] | (data[2 * i + 1] << 8)
        r, g, b = v & 31, (v >> 5) & 31, (v >> 10) & 31
        out[3 * i] = (r << 3) | (r >> 2)
        out[3 * i + 1] = (g << 3) | (g >> 2)
        out[3 * i + 2] = (b << 3) | (b >> 2)
    return out


def convert_shot(raw_path, txt_path, png_path):
    data = open(raw_path, "rb").read()
    w, h = [int(x) for x in open(txt_path).read().split()[:2]]
    if w <= 0 or h <= 0:
        return False
    if len(data) >= w * h * 3 and len(data) < w * h * 2 * 1.4:
        rgb = bytearray(data[:w * h * 3])          # 24-bit display mode
    else:
        rgb = bgr555_to_rgb(data, w, h)
    write_png(png_path, w, h, rgb)
    return True


def convert_vram(path, png_path):
    data = open(path, "rb").read()
    write_png(png_path, 1024, 512, bgr555_to_rgb(data, 1024, 512))


def split_words(line):
    """the native runner's tokenizer: words, "quoted strings" with \\n \\b
    \\t escapes, '#' at the start of a word starts a comment"""
    words, i, n = [], 0, len(line)
    while i < n:
        while i < n and line[i].isspace():
            i += 1
        if i >= n or line[i] == "#":
            break
        if line[i] == '"':
            i += 1
            w = []
            while i < n and line[i] != '"':
                if line[i] == "\\" and i + 1 < n:
                    c = line[i + 1]
                    w.append({"n": "\n", "b": "\b", "t": "\t"}.get(c, c))
                    i += 2
                else:
                    w.append(line[i])
                    i += 1
            i += 1
            words.append("".join(w))
            continue
        j = i
        while j < n and not line[j].isspace():
            j += 1
        words.append(line[i:j])
        i = j
    return words


def hcmd(name, iarg=0, text=""):
    return "hcmd %s %d %s" % (name, iarg, text.encode("latin-1").hex())


def compile_script(lines, boot_frames=2):
    """native-runner script -> low-level harness commands. The script starts
    once the game's main loop is running (the PS1 boot shows a splash while
    the BIOS reads the memory cards), so timings match the native runner.
    Returns (commands, warnings, expected TTY texts)."""
    cmds, warnings, expect = ["waitframe %d" % boot_frames], [], []
    for ln, line in enumerate(lines, 1):
        parts = split_words(line)
        if not parts:
            continue
        op = parts[0].lower()
        arg = parts[1] if len(parts) > 1 else ""

        def btn(i=1):
            name = parts[i].upper() if i < len(parts) else ""
            if name not in BUTTONS:
                raise SystemExit("line %d: unknown button %s" % (ln, name))
            return BUTTONS[name]

        def num(i, default):
            return int(parts[i]) if i < len(parts) else default
        if op == "wait":
            cmds.append("waitf %d" % num(1, 1))
        elif op == "press":
            # native: press BTN [BTN..] = hold for 1 frame, then 1 frame up
            names = [btn(i) for i in range(1, len(parts))]
            cmds += ["down " + b for b in names]
            cmds.append("waitf 1")
            cmds += ["up " + b for b in names]
            cmds.append("waitf 1")
        elif op == "hold":
            cmds += ["down " + btn(), "waitf %d" % num(2, 1), "up " + btn()]
        elif op == "down":
            cmds.append("down " + btn())
        elif op == "up":
            cmds.append("up " + btn())
        elif op == "release":
            if len(parts) > 1:
                cmds.append("up " + btn())
            else:
                cmds += ["up " + b for b in sorted(set(BUTTONS.values()))]
        elif op == "shot":
            cmds.append("shot " + (arg or "shot"))
        elif op in ("vram", "fps", "fpsend"):
            cmds.append("%s %s" % (op, arg))
        elif op in ("scene", "push"):
            cmds.append(hcmd(op, num(2, 0), arg))
        elif op == "expect-scene":
            cmds.append(hcmd(op, 0, arg))
        elif op in ("save", "load", "expect-save"):
            cmds.append(hcmd(op, num(1, 1)))
        elif op == "type":
            if len(arg) > 63:
                raise SystemExit("line %d: type text longer than 63 chars" % ln)
            cmds.append(hcmd(op, 0, arg))
        elif op == "log":
            cmds.append(hcmd(op, 0, " ".join(parts[1:])[:63]))
        elif op == "no-stubs":
            cmds.append("mark no-stubs line %d" % ln)
        elif op == "expect-log":
            expect.append(" ".join(parts[1:]))
        else:
            raise SystemExit("line %d: unknown command %s" % (ln, op))
    cmds.append("waitf 1")
    cmds.append("quit")
    return cmds, warnings, expect


def find_symbol(map_path, name):
    try:
        for line in open(map_path):
            p = line.split()
            if len(p) >= 3 and p[0] == name:
                return int(p[2], 16)
    except OSError:
        pass
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe")
    ap.add_argument("--cue")
    ap.add_argument("--script", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--map")
    ap.add_argument("--card")
    ap.add_argument("--keep-card", action="store_true")
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--hang", type=int, default=900)
    ap.add_argument("--expect", action="append", default=[])
    ap.add_argument("--assert-no-stubs", action="store_true")
    ap.add_argument("--emu", default="/opt/ps1emu/pcsx-redux")
    ap.add_argument("--bios", default="/opt/ps1emu/openbios.bin")
    a = ap.parse_args()
    if not a.exe and not a.cue:
        ap.error("need --exe or --cue")

    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        if f.endswith((".raw", ".png", ".vram", ".txt", ".log")):
            os.remove(os.path.join(out, f))
    cmds, warnings, script_expect = compile_script(open(a.script).read().splitlines())
    for w in warnings:
        print("run.py: warning:", w)
    cmd_path = os.path.join(out, "harness_cmds.lst")
    with open(cmd_path, "w") as f:
        f.write("\n".join(cmds) + "\n")

    image = os.path.abspath(a.exe or a.cue)
    map_path = a.map or os.path.join(os.path.dirname(image), "doomogachi.map")
    frame_addr = find_symbol(map_path, "g_loop_frames") or find_symbol(map_path, "g_frame")
    hcmd_addr = find_symbol(map_path, "g_hcmd")
    card1 = os.path.abspath(a.card or os.path.join(out, "card1.mcd"))
    card2 = os.path.join(out, "card2.mcd")
    if not a.keep_card and os.path.exists(card1) and not a.card:
        os.remove(card1)

    env = dict(os.environ)
    env.update(HARNESS_CMDS=cmd_path, HARNESS_OUT=out, HARNESS_FRAME="%x" % frame_addr,
               HARNESS_HCMD="%x" % hcmd_addr, HARNESS_HANG=str(a.hang))
    argv = ["xvfb-run", "-a", a.emu, "-cli", "-stdout", "-run", "-bios", a.bios,
            "-memcard1", card1, "-memcard2", card2]
    argv += ["-exe", image] if a.exe else ["-iso", image]
    argv += ["-dofile", os.path.join(HERE, "harness.lua")]
    tty_path = os.path.join(out, "tty.log")
    status = "ok"
    with open(tty_path, "wb") as tty:
        try:
            p = subprocess.run(argv, stdout=tty, stderr=subprocess.STDOUT, env=env,
                               cwd=out, timeout=a.timeout)
            rc = p.returncode
        except subprocess.TimeoutExpired:
            rc, status = -1, "timeout"
    tty_text = open(tty_path, "rb").read().decode("latin-1")

    shots = 0
    for f in sorted(os.listdir(out)):
        if f.endswith(".raw"):
            base = f[:-4]
            txt = os.path.join(out, base + ".txt")
            if os.path.exists(txt) and convert_shot(os.path.join(out, f), txt,
                                                    os.path.join(out, base + ".png")):
                shots += 1
                os.remove(os.path.join(out, f))
                os.remove(txt)
        elif f.endswith(".vram"):
            convert_vram(os.path.join(out, f), os.path.join(out, f[:-5] + "_vram.png"))
            os.remove(os.path.join(out, f))

    fails = []
    if status == "timeout":
        fails.append("emulator timed out after %d s" % a.timeout)
    if "HARNESS: HANG" in tty_text:
        fails.append("game hang detected (heartbeat stopped)")
    if "HARNESS: END" not in tty_text:
        fails.append("harness did not finish (emulator crash?) rc=%s" % rc)
    for pat in ("Unrecoverable error", "*** PANIC", "Segmentation fault",
                "Exception", "assert"):
        if pat in tty_text:
            fails.append("TTY contains '%s'" % pat)
    for e in a.expect + script_expect:
        if e not in tty_text:
            fails.append("expected TTY text not found: %s" % e)
    for m in re.findall(r"HARNESS: FAIL: (.*)", tty_text):
        fails.append(m.strip())
    stubs = sorted(set(re.findall(r"STUB: (\S+)", tty_text)))
    for m in re.finditer(r"HARNESS: mark no-stubs (line \d+)", tty_text):
        early = sorted(set(re.findall(r"STUB: (\S+)", tty_text[:m.start()])))
        if early:
            fails.append("no-stubs at %s: stubs ran: %s" % (m.group(1), ", ".join(early)))
    if a.assert_no_stubs and stubs:
        fails.append("stubs ran (--assert-no-stubs): " + ", ".join(stubs))
    fps = re.findall(r"HARNESS: fps (\S+) ([0-9.]+)", tty_text)
    perf = re.findall(r"perf: .*", tty_text)

    print("run.py: %d screenshot(s) in %s" % (shots, out))
    for name, v in fps:
        print("run.py: fps %-16s %s" % (name, v))
    if perf:
        print("run.py: last " + perf[-1])
    if stubs:
        print("run.py: core weak stubs hit: " + ", ".join(stubs))
    for f in fails:
        print("run.py: FAIL: " + f)
    print("run.py: %s" % ("PASS" if not fails else "FAIL"))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
