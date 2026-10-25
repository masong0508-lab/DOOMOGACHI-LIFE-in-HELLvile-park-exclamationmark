#!/usr/bin/env python3
"""ram_report.py - convert show.elf with the game's elf2exe.py and check RAM.

    ram_report.py --elf2exe ps1/tools/elf2exe.py [--ram-kb 2048] show.elf show.exe ram.txt

elf2exe.py (read-only, the game's) writes the PS-X EXE and refuses an image
that leaves no room for the stack in 2 MB. This adds the show's budget
from spec 8.7 on top: what the skeleton leaves below the stack top must
still hold the two shot-chunk buffers (320 KB) and the rest of the player
(<= 120 KB), with the 64 KB stack reserve elf2exe already keeps. The
numbers go to ram.txt (AT1.2, M0-T4) and to stdout.
"""
import argparse
import re
import subprocess
import sys

CHUNK_BUFFERS_KB = 320      # spec 8.7: play one, prefetch the next
PLAYER_KB = 120             # spec 8.7: show player (e), the upper end


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--elf2exe", required=True)
    ap.add_argument("--ram-kb", type=int, default=2048)
    ap.add_argument("elf")
    ap.add_argument("exe")
    ap.add_argument("report")
    a = ap.parse_args()
    p = subprocess.run([sys.executable, a.elf2exe, a.elf, a.exe],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    out = p.stdout.strip()
    print(out)
    if p.returncode != 0:
        print("show: elf2exe refused %s (spec 8.7)" % a.elf, file=sys.stderr)
        return p.returncode
    m = re.search(r"size (\d+) KB \(\+bss (\d+) KB, ends 0x([0-9a-f]+), (\d+) KB left", out)
    if not m:
        print("show: cannot read the elf2exe report", file=sys.stderr)
        return 1
    size_kb, bss_kb, end, left_kb = int(m.group(1)), int(m.group(2)), int(m.group(3), 16), int(m.group(4))
    need = CHUNK_BUFFERS_KB + PLAYER_KB
    ok = left_kb >= need
    lines = [
        "show RAM report (spec 8.7; retail %d KB%s)" % (2048, "" if a.ram_kb == 2048 else
                                                       ", built for the %d KB hatch" % a.ram_kb),
        "image      %5d KB  (code + data)" % size_kb,
        "bss        %5d KB" % bss_kb,
        "ends       0x%08x" % end,
        "left       %5d KB  below the stack top (64 KB stack reserve kept by elf2exe)" % left_kb,
        "needs      %5d KB  (two shot-chunk buffers %d + rest of the player %d)" % (
            need, CHUNK_BUFFERS_KB, PLAYER_KB),
        "headroom   %5d KB  after those" % (left_kb - need),
        "verdict    %s" % ("PASS" if ok else "FAIL: over the spec 8.7 budget"),
    ]
    open(a.report, "w").write("\n".join(lines) + "\n")
    print("\n".join(lines))
    if not ok and a.ram_kb <= 2048:
        print("show: %s leaves %d KB, the player needs %d KB (spec 8.7)" % (a.exe, left_kb, need),
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
