#!/usr/bin/env python3
"""check_isa.py - fail if a linked PS1 ELF contains instructions the R3000A
(MIPS I) cannot execute.

Why: the Debian/Ubuntu mipsel-linux-gnu toolchain's libgcc.a is built for
MIPS32R2, so a stray libgcc helper (e.g. __divdi3 using CLZ) links fine and
then crashes on the console with a Reserved Instruction exception. Our own
objects use -march=r3000; this check catches anything pulled in from
elsewhere. ps1/src/libgcc_r3000.c provides R3000 versions of the helpers.

usage: check_isa.py OBJDUMP file.elf
"""
import re
import subprocess
import sys

FORBIDDEN = {
    # MIPS32 / MIPS II+ integer ops
    "clz", "clo", "mul", "madd", "maddu", "msub", "msubu", "movn", "movz",
    "seb", "seh", "wsbh", "ext", "ins", "rotr", "rotrv", "sync", "ll", "sc",
    "teq", "tne", "tge", "tgeu", "tlt", "tltu", "teqi", "tnei", "tgei", "tgeiu",
    "tlti", "tltiu", "beql", "bnel", "blezl", "bgtzl", "bltzl", "bgezl",
    "bltzall", "bgezall", "pref", "synci", "rdhwr", "di", "ei", "eret",
    "deret", "wait", "ssnop", "ehb", "jalr.hb", "jr.hb",
    # FPU (the PS1 has none)
    "lwc1", "swc1", "ldc1", "sdc1", "mfc1", "mtc1", "cfc1", "ctc1",
    "add.s", "add.d", "mul.s", "mul.d", "div.s", "div.d", "cvt.s.w", "cvt.d.w",
}


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    objdump, elf = argv[1], argv[2]
    out = subprocess.run([objdump, "-d", "--no-show-raw-insn", elf],
                         capture_output=True, text=True, check=True).stdout
    func = "?"
    bad = {}
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <([^>]+)>:", line)
        if m:
            func = m.group(1)
            continue
        m = re.match(r"^\s*([0-9a-f]+):\s+(\S+)", line)
        if m and m.group(2) in FORBIDDEN:
            bad.setdefault(func, []).append("%s %s" % (m.group(1), m.group(2)))
    if bad:
        print("check_isa: %s contains non-R3000 instructions:" % elf)
        for f, ins in sorted(bad.items()):
            print("  %-32s %s%s" % (f, ", ".join(ins[:4]), " ..." if len(ins) > 4 else ""))
        return 1
    print("check_isa: %s is R3000 (MIPS I) clean" % elf)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
