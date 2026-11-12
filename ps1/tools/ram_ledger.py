#!/usr/bin/env python3
"""ram_ledger.py - where the PS1 game's 2 MB of RAM go (owner: ps1).
Stdlib only, deterministic.

    ram_ledger.py build/doomogachi.ldmap [--ld build/ovl.ld]
                  [--readelf READELF --elf build/doomogachi.elf]
                  [--objs N] [--syms N] [--md] [--gate BYTES]

Reads the linker map of the game disc (ld -Map: every build writes
build/doomogachi.ldmap) and prints the resident ledger of docs/PS1.md
"RAM headroom 2026-11-12":

  * the layout: resident .text / .rodata / .data / .bss, the borrowed
    .bss, the overlay slot, and what is left below the stack top;
  * the N biggest resident objects with .text, .rodata, string literals,
    .data and .bss apart. The linker merges the string literals of all
    resident objects into one block: an object's "strings" are its share
    before the merge, the block's real size is in the layout;
  * the overlay groups, named from ovl.ld with --ld (not resident: one
    group at a time is read from OVL.BIN into the slot, so only the
    biggest sets the slot's size);
  * with --readelf and --elf: the N biggest resident symbols, each with
    its kind and object.

--md prints GitHub markdown (the CI job summary). --gate BYTES adds a
WARNING line when less than BYTES are left below the stack top (the
exit code stays 0: tools/elf2exe.py is what fails a build).
"""
import argparse
import bisect
import collections
import re
import subprocess
import sys

STACK_TOP = 0x801FFFF0
RESIDENT = (".text", ".rodata", ".ctors", ".dtors", ".data", ".sdata", ".sbss", ".bss", ".borrow")
KIND = {".text": "text", ".rodata": "rodata", ".ctors": "data", ".dtors": "data",
        ".data": "data", ".sdata": "data", ".sbss": "bss", ".bss": "bss", ".borrow": "borrow"}


def obj_name(path):
    """the object as the docs name it: core/src/ui, ps1/src/gfx, sdk/libpsxgpu(image.c)"""
    m = re.search(r"/(lib\w+?)(?:_exe_gprel)?\.a\((.+?)\.o(?:bj)?\)$", path)
    if m:
        return "sdk/%s(%s)" % (m.group(1), m.group(2))
    p = path
    for ext in (".c.obj", ".s.obj", ".obj", ".o"):
        if p.endswith(ext):
            p = p[: -len(ext)]
            break
    k = p.find("/core/")
    if k >= 0:
        return p[k + 1:]
    k = p.find("doomo_objs.dir/")
    if k >= 0:
        rest = p[k + len("doomo_objs.dir/"):]
        return ("ps1/" + rest) if rest.startswith("src/") else ("build/" + rest)
    return p.rsplit("/", 1)[-1]


def parse_map(path):
    """-> (outs, syms). outs: output section -> [addr, size, [(input section, addr, size, object)]];
    syms: the layout symbols the map prints (_end, __ovl_slot, ...)"""
    outs = collections.OrderedDict()
    syms = {}
    cur = pend = None
    started = False
    rx_out = re.compile(r"^(\.[\w.]+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)")
    rx_in1 = re.compile(r"^ (\.[^\s]+|COMMON)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$")
    rx_in0 = re.compile(r"^ (\.[^\s]+|COMMON)\s*$")
    rx_in2 = re.compile(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$")
    rx_sym = re.compile(r"^\s+0x([0-9a-f]+)\s+(\w+) = ")
    with open(path, errors="replace") as f:
        lines = f.read().split("\n")
    for line in lines:
        if not started:
            started = line.startswith("Linker script and memory map")
            continue
        m = rx_out.match(line)
        if m:
            cur = m.group(1)
            outs[cur] = [int(m.group(2), 16), int(m.group(3), 16), []]
            pend = None
            continue
        if line and not line.startswith(" "):
            cur = pend = None           # the next statement of the script, or the map's tail
            continue
        m = rx_sym.match(line)
        if m:
            syms[m.group(2)] = int(m.group(1), 16)
            continue
        if cur is None:
            continue
        m = rx_in1.match(line)
        if m:
            outs[cur][2].append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4).strip()))
            pend = None
            continue
        m = rx_in0.match(line)
        if m:
            pend = m.group(1)           # a long section name: its address is on the next line
            continue
        m = rx_in2.match(line)
        if m and pend:
            outs[cur][2].append((pend, int(m.group(1), 16), int(m.group(2), 16), m.group(3).strip()))
        pend = None
    return outs, syms


def is_str(name):
    return ".str1." in name


def group_names(ld_path):
    """.ovl_NN -> group name, from the comments ovl_plan.py writes into ovl.ld"""
    names = {}
    try:
        with open(ld_path) as f:
            for line in f:
                m = re.match(r"\s*(\.ovl_\d+)\s.*/\*\s*(\S+)\s*\*/", line)
                if m:
                    names[m.group(1)] = m.group(2)
    except OSError:
        pass
    return names


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("map")
    ap.add_argument("--ld")
    ap.add_argument("--readelf")
    ap.add_argument("--elf")
    ap.add_argument("--objs", type=int, default=40)
    ap.add_argument("--syms", type=int, default=40)
    ap.add_argument("--md", action="store_true")
    ap.add_argument("--gate", type=int, default=0)
    a = ap.parse_args(argv[1:])

    outs, syms = parse_map(a.map)
    if ".text" not in outs:
        print("ram_ledger: no .text in %s (not a linker map?)" % a.map)
        return 2
    ovl = [(n, o[0], o[1]) for n, o in outs.items() if n.startswith(".ovl_")]
    end = syms.get("_end") or max(o[0] + o[1] for n, o in outs.items() if n in RESIDENT or n.startswith(".ovl_"))
    slot = syms.get("__ovl_slot", 0)
    slot_end = syms.get("__ovl_slot_end", end)
    left = STACK_TOP - end
    size = {n: outs[n][1] for n in RESIDENT if n in outs}
    # the merged string block: the map prints it at its first input, with the block's real size
    merged, merged_at = 0, None
    for i, (name, _, sz, _) in enumerate(outs.get(".rodata", [0, 0, []])[2]):
        if is_str(name) and sz > merged:
            merged, merged_at = sz, i

    def row(*cells):
        if a.md:
            print("| " + " | ".join(str(c) for c in cells) + " |")
        else:
            print("  " + "  ".join(str(c) for c in cells))

    def head(title, cols):
        print()
        if a.md:
            print("### " + title)
            print()
            print("| " + " | ".join(cols) + " |")
            print("|" + "|".join("---" if i == 0 else "---:" for i in range(len(cols))) + "|")
        else:
            print(title + "  (" + ", ".join(cols) + ")")

    head("PS1 game disc: the 2 MB", ["what", "bytes"])
    row("resident .text (code)", size.get(".text", 0))
    row("resident .rodata (const data; %d B of it the merged string literals)" % merged, size.get(".rodata", 0))
    row(".data + .sdata + ctors / dtors", sum(size.get(n, 0) for n in (".ctors", ".dtors", ".data", ".sdata")))
    row(".sbss + .bss", size.get(".sbss", 0) + size.get(".bss", 0))
    row(".borrow (the .bss the house and creator groups load over)", size.get(".borrow", 0))
    if slot:
        row("overlay slot (after the .bss; the biggest group sets it)", slot_end - slot)
    row("the EXE ends at", "0x%08x" % end)
    row("left below the stack top (0x801FFFF0)", left)

    # --- resident objects ---
    per = collections.defaultdict(collections.Counter)
    for n in RESIDENT:
        for i, (name, _, sz, obj) in enumerate(outs.get(n, [0, 0, []])[2]):
            if name == "*fill*":
                continue
            k = KIND[n]
            if n == ".rodata" and is_str(name):
                k = "str"
                if i == merged_at:
                    continue            # the block's size, not this object's share
            per[obj_name(obj)][k] += sz
    rows = sorted(per.items(), key=lambda kv: (-sum(kv[1].values()), kv[0]))
    cols = ["object", "total", ".text", ".rodata", "strings", ".data", ".bss", "borrowed .bss"]
    head("The %d biggest resident objects" % min(a.objs, len(rows)), cols)
    for obj, c in rows[: a.objs]:
        row(obj, sum(c.values()), c["text"], c["rodata"], c["str"], c["data"], c["bss"], c["borrow"])
    if rows[a.objs:]:
        c = collections.Counter()
        for _, x in rows[a.objs:]:
            c.update(x)
        row("(%d more)" % len(rows[a.objs:]), sum(c.values()), c["text"], c["rodata"], c["str"], c["data"],
            c["bss"], c["borrow"])

    # --- overlay groups ---
    if ovl:
        names = group_names(a.ld) if a.ld else {}
        head("Overlay groups (one at a time in the slot)", ["group", "bytes", "loads at"])
        for n, addr, sz in sorted(ovl, key=lambda x: (-x[2], x[0])):
            row(names.get(n, n), sz, "0x%08x%s" % (addr, "" if addr >= slot or not slot else " (over .borrow)"))

    # --- resident symbols ---
    if a.readelf and a.elf:
        out = subprocess.run([a.readelf, "-S", "-s", "-W", a.elf], check=True, capture_output=True, text=True).stdout
        secname = {}
        symrows = []
        for line in out.splitlines():
            m = re.match(r"\s*\[\s*(\d+)\]\s+(\S+)\s+\S+\s+[0-9a-f]+", line)
            if m:
                secname[int(m.group(1))] = m.group(2)
                continue
            # (readelf prints a size over 99999 in hex)
            m = re.match(r"\s*\d+:\s+([0-9a-f]+)\s+(\d+|0x[0-9a-f]+)\s+(\w+)\s+\w+\s+\w+\s+(\d+)\s+(\S+)", line)
            if m and m.group(3) in ("OBJECT", "FUNC") and int(m.group(2), 0):
                sec = secname.get(int(m.group(4)))
                if sec in RESIDENT:
                    symrows.append((int(m.group(2), 0), m.group(5), int(m.group(1), 16), sec))
        spans = {}
        for n in RESIDENT:
            s = sorted((addr, addr + sz, obj_name(obj)) for name, addr, sz, obj in outs.get(n, [0, 0, []])[2] if sz)
            spans[n] = (s, [x[0] for x in s])
        head("The %d biggest resident symbols" % a.syms, ["symbol", "bytes", "kind", "object"])
        for sz, name, addr, sec in sorted(symrows, key=lambda r: (-r[0], r[1]))[: a.syms]:
            s, starts = spans[sec]
            i = bisect.bisect_right(starts, addr) - 1
            obj = s[i][2] if i >= 0 and s[i][0] <= addr < s[i][1] else "?"
            row(name, sz, KIND[sec], obj)

    if a.gate and left < a.gate:
        print()
        print("ram_ledger: WARNING: %d B left below the stack top, %d B under the %d B gate" % (
            left, a.gate - left, a.gate))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
