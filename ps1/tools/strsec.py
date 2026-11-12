#!/usr/bin/env python3
"""strsec.py - every string literal of a file-scope table in a section of
its own (owner: ps1). A compiler launcher: CMake runs

    strsec.py CC ARGS... -o X.obj -c X.c

for every C file of the PS1 build (ps1/CMakeLists.txt, DOOMO_STRING_SECTIONS).
Stdlib only, deterministic.

Why. With -fdata-sections GCC gives every function's string literals a
section of their own (.rodata.<function>.str1.4), but all the literals a
file's TABLES point to share one, .rodata.str1.4. The overlay planner
(ovl_plan.py) places whole sections: one resident pointer into that
section (a scene's name, two tab labels in .sdata) kept every string of
the file's tables in resident RAM, 40 KB of them on 2026-11-12 (docs/PS1.md
"RAM headroom 2026-11-12"). Here each of those literals gets its own
section, .rodata.str1.4.LC<n>, so the planner moves exactly the strings a
group alone reads into that group, and --gc-sections drops the ones
nothing reads.

How. The file is compiled to assembly (the same command with -S), the
assembly is rewritten (split_strings below: only the `.section
.rodata.str1.N` blocks change, each `$LC<n>:` label opens its own
mergeable section), and the result is assembled with the command's
machine flags. Without the rewrite the two steps give the same object,
byte for byte, as the one-step compile: STRSEC_VERIFY=1 checks exactly
that on every file (and fails the compile when it does not hold). The
linker still merges equal strings inside an output section, as before.

Anything that is not a plain `CC ... -c one.c -o obj` is run unchanged.
"""
import os
import re
import subprocess
import sys

RX_GENERIC = re.compile(r'^\s*\.section\s+(\.rodata\.str1\.\d+)\s*(,.*)?$')
RX_SECTION = re.compile(r'^\s*\.(section|text|data|bss|rdata|sdata|sbss|lcomm|comm)\b')
RX_UNKNOWN = re.compile(r'^\s*\.(previous|pushsection|popsection|subsection)\b')
RX_LABEL = re.compile(r'^([.$]LC\d+):\s*$')
RX_ALIGN = re.compile(r'^\s*\.(align|p2align|balign)\b')
RX_BLANK = re.compile(r'^\s*(#.*)?$')


def split_strings(text):
    """the assembly with every labelled literal of a generic string section
    (.rodata.str1.N) in a section of its own, named after its label. Returns
    (new text, literals moved). A file that uses the section stack after its
    first string section (GCC only does at the very top: `.section
    .mdebug.abi32` / `.previous`), or has data in a string section before any
    label, is returned unchanged."""
    out = []
    moved = 0
    cur = None          # the generic string section we are in
    flags = {}          # section name -> its flags, from the directive that carried them
    held = []           # the generic .section line and .align lines not yet written
    labelled = False    # a label was seen since the last .section line
    for line in text.split("\n"):
        if RX_UNKNOWN.match(line):
            if flags:
                return text, 0
            out.append(line)
            continue
        m = RX_GENERIC.match(line)
        if m:
            cur = m.group(1)
            if m.group(2):
                flags[cur] = m.group(2)
            if cur not in flags:
                return text, 0          # re-entered before it was ever declared
            held = [line]
            labelled = False
            continue
        if cur is None:
            out.append(line)
            continue
        if RX_SECTION.match(line):
            if not labelled:
                out.extend(held)        # an empty visit: keep it as it was
            cur, held = None, []
            out.append(line)
            continue
        if RX_ALIGN.match(line):
            held.append(line)
            continue
        m = RX_LABEL.match(line)
        if m:
            out.append("\t.section\t%s.%s%s" % (cur, m.group(1).lstrip(".$"), flags[cur]))
            out.extend(l for l in held if RX_ALIGN.match(l))
            out.append(line)
            held = []
            labelled = True
            moved += 1
            continue
        if not labelled and not RX_BLANK.match(line):
            return text, 0              # data before a label: not GCC's layout
        out.extend(held)                # (an .align inside a literal: keep its place)
        held = []
        out.append(line)
    if cur is not None and not labelled:
        out.extend(held)
    return "\n".join(out), moved


def assembler_args(args):
    """the flags of a compile command that the assembler step needs (the
    target, the ABI, -G, -Wa,...); everything else was used up by the -S step"""
    keep = []
    for a in args:
        if a.startswith(("-m", "-G", "-Wa,", "-EL", "-EB")) or a in ("-fno-pic", "-fpic", "-fPIC", "-fno-PIC"):
            keep.append(a)
    return keep


def parse(argv):
    """(cc, args, index of -c, index of the source, index of the object path)
    or None when this is not a plain one-file C compile"""
    if len(argv) < 2:
        return None
    cc, args = argv[0], argv[1:]
    if args.count("-c") != 1 or "-o" not in args or "-S" in args or "-E" in args:
        return None
    srcs = [i for i, a in enumerate(args) if a.endswith(".c") and not a.startswith("-")
            and (i == 0 or args[i - 1] not in ("-o", "-MT", "-MF", "-MQ", "-include", "-I"))]
    if len(srcs) != 1:
        return None
    oi = args.index("-o") + 1
    if oi >= len(args):
        return None
    return cc, args, args.index("-c"), srcs[0], oi


def run(cmd):
    return subprocess.run(cmd).returncode


def main(argv):
    p = parse(argv[1:])
    if p is None:
        if len(argv) < 2:
            print(__doc__)
            return 2
        return run(argv[1:])
    cc, args, ci, si, oi = p
    obj = args[oi]
    asm = obj + ".s"
    step1 = list(args)
    step1[ci] = "-S"
    step1[oi] = asm
    rc = run([cc] + step1)
    if rc:
        return rc
    text = open(asm).read()
    if os.environ.get("STRSEC_VERIFY"):
        # the two steps without the rewrite must give the one-step object
        plain = [a for i, a in enumerate(args) if not (a in ("-MD", "-MMD") or
                 (i and args[i - 1] in ("-MT", "-MF", "-MQ")) or a in ("-MT", "-MF", "-MQ"))]
        plain[plain.index("-o") + 1] = obj + ".direct"
        if run([cc] + plain) or run([cc] + assembler_args(args) + ["-c", asm, "-o", obj + ".twostep"]):
            return 1
        same = open(obj + ".direct", "rb").read() == open(obj + ".twostep", "rb").read()
        os.remove(obj + ".direct")
        os.remove(obj + ".twostep")
        if not same:
            print("strsec: %s: compile + assemble differs from the one-step compile" % args[si])
            return 1
    new, moved = split_strings(text)
    if moved:
        with open(asm, "w") as f:
            f.write(new)
    rc = run([cc] + assembler_args(args) + ["-c", asm, "-o", obj])
    if rc == 0:
        os.remove(asm)
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
