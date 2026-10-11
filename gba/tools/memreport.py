#!/usr/bin/env python3
"""memreport.py - print the ROM's RAM budget from the linker map
(owner: gba). Stdlib only.

    python3 gba/tools/memreport.py ROM.map [ROM.elf]

Reads the linker-script symbols out of the GNU ld map file and prints
IWRAM (ARM code + data + bss, stack) and EWRAM (.data + .bss + .noinit)
usage, plus the biggest EWRAM / IWRAM objects. Exit status 0 always (the
hard limits are ASSERTs in gba.ld).
"""
import re
import sys

SYMS = ("__iwram_start", "__iwram_end", "__iwram_bss_start", "__iwram_bss_end",
        "__iwram_stack_limit", "__sp_usr", "__data_start", "__data_end",
        "__bss_start", "__bss_end", "__ewram_end", "__rom_end")


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: memreport.py ROM.map")
    text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
    val = {}
    for s in SYMS:
        m = re.search(r"^\s+0x([0-9a-fA-F]+)\s+" + re.escape(s) + r"\s*=", text, re.M)
        if m:
            val[s] = int(m.group(1), 16)
    if "__iwram_start" not in val or "__ewram_end" not in val:
        print("memreport: symbols not found in map")
        return 0
    iw_code = val["__iwram_end"] - val["__iwram_start"]
    iw_bss = val["__iwram_bss_end"] - val["__iwram_bss_start"]
    iw_used = val["__iwram_bss_end"] - 0x03000000
    stack = val.get("__sp_usr", 0x03007D00) - val["__iwram_bss_end"]
    ew_used = val["__ewram_end"] - 0x02000000
    data = val["__data_end"] - val["__data_start"]
    bss = val["__bss_end"] - val["__bss_start"]
    rom = val.get("__rom_end", 0x08000000) - 0x08000000
    print("memreport: ROM   %7d bytes (%.1f KB)" % (rom, rom / 1024.0))
    print("memreport: IWRAM %7d / 32768 used (code+data %d, bss %d); "
          "%d bytes left for the main stack (+672 IRQ stack)" %
          (iw_used, iw_code, iw_bss, stack))
    print("memreport: EWRAM %7d / 262144 used (.data %d, .bss %d, noinit %d); "
          "%d bytes free" % (ew_used, data, bss,
                             val["__ewram_end"] - val["__bss_end"], 262144 - ew_used))
    # biggest objects in .bss / .data (EWRAM) and IWRAM
    objs = []
    for m in re.finditer(r"^ (\.(?:bss|data|iwram|iwram_bss|iwram_data|ewram_bss|noinit)"
                         r"[^\s]*)\s*\n?\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)", text, re.M):
        addr, size = int(m.group(2), 16), int(m.group(3), 16)
        if size >= 2048 and (0x02000000 <= addr < 0x04000000):
            objs.append((size, addr, m.group(1), m.group(4).split("/")[-1]))
    objs.sort(reverse=True)
    for size, addr, sec, obj in objs[:12]:
        print("memreport:   %6d  0x%08x  %-28s %s" % (size, addr, sec[:28], obj))
    return 0


if __name__ == "__main__":
    sys.exit(main())
