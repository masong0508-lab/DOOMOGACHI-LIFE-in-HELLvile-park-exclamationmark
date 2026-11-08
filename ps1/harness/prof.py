#!/usr/bin/env python3
"""prof.py - per-function table of a harness PC profile (prof/profend).

usage: prof.py doomogachi.map OUT/prof_NAME.txt [--callers] [-n 30]
               [--group NAME [--elf doomogachi.elf] [--ld ovl.ld]]

Samples are "pc ra count" lines (one sample per vsync). Addresses inside
the scene overlay slot are ambiguous (every overlay group links there);
without --group the symbol listed is the nearest one of any group ("a/b/c"
when several share the address), so read those with the running scene in
mind.

--group NAME (SP1 T1) names slot addresses by the overlay group that was
loaded while the samples were taken: the scene's group from
ps1/tools/ovl_plan.py GROUPS (creator, combat, island, freeplay, ...; a
house part as home.<part>). The group's output sections (.ovl_NN) come from
the linker script ovl_plan.py wrote (ovl.ld next to the map), each symbol's
section from the ELF's symbol table (doomogachi.elf next to the map, read
here without binutils). A slot address outside that group's symbols is
"?slot" (a sample taken while another group was in). Stdlib only.
"""
import bisect
import os
import re
import struct
import sys


def load_map(path):
    syms = []
    for line in open(path):
        p = line.split()
        if len(p) >= 3 and p[1] in "TtWw":
            try:
                syms.append((int(p[2], 16), p[0]))
            except ValueError:
                pass
    syms.sort()
    return syms


def elf_symbol_sections(path):
    """{(addr, name): section name} for the function symbols of a 32-bit
    little-endian ELF (the PS1 EXE's ELF)."""
    data = open(path, "rb").read()
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise SystemExit("prof.py: %s is not a 32-bit little-endian ELF" % path)
    shoff, = struct.unpack_from("<I", data, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x2E)
    shdrs = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize) for i in range(shnum)]

    def cstr(off):
        return data[off:data.index(b"\0", off)].decode("latin-1")

    names = [cstr(shdrs[shstrndx][4] + h[0]) for h in shdrs]
    out = {}
    for h in shdrs:
        if h[1] != 2:                      # SHT_SYMTAB
            continue
        stroff = shdrs[h[6]][4]            # sh_link: its string table
        for k in range(h[5] // 16):
            st_name, st_value, _size, st_info, _other, st_shndx = struct.unpack_from("<IIIBBH", data, h[4] + k * 16)
            if (st_info & 0xF) != 2 or not st_name or st_shndx >= len(names):   # STT_FUNC
                continue
            out[(st_value, cstr(stroff + st_name))] = names[st_shndx]
    return out


def group_sections(ld_path, group):
    """The output sections (.ovl_NN) of overlay group GROUP in ovl.ld: the
    group's own section, plus its parts (".ovl_NN { /* group.part */")
    when GROUP names the group itself."""
    secs = []
    for line in open(ld_path):
        m = re.search(r"(\.ovl_\d+)\s*(?:__ovl_\w+\s*:\s*AT\s*\([^{]*\))?\s*\{?\s*/\*\s*([\w.]+)\s*\*/", line)
        if m and (m.group(2) == group or m.group(2).startswith(group + ".")):
            secs.append(m.group(1))
    if not secs:
        raise SystemExit("prof.py: no overlay group %r in %s" % (group, ld_path))
    return set(secs)


def name_of(syms, addrs, a, slot=None):
    i = bisect.bisect_right(addrs, a) - 1
    if i < 0:
        return "?"
    if slot and slot["lo"] <= a < slot["hi"]:
        # only the loaded group's symbols name a slot address
        while i >= 0 and syms[i][0] >= slot["lo"] and syms[i] not in slot["mine"]:
            i -= 1
        if i < 0 or syms[i][0] < slot["lo"]:
            return "?slot"
        return syms[i][1]
    # every symbol at that address (overlay groups share the slot)
    base = syms[i][0]
    if a - base > 0x20000:
        return "?%08x" % a
    names = sorted({n for (x, n) in syms if x == base})
    return "/".join(names[:3])


def main():
    argv = sys.argv[1:]

    def opt(name):
        if name in argv:
            k = argv.index(name)
            v = argv[k + 1]
            del argv[k:k + 2]
            return v
        return None

    n = int(opt("-n") or 30)
    group, elf, ld = opt("--group"), opt("--elf"), opt("--ld")
    callers = "--callers" in argv
    args = [a for a in argv if not a.startswith("-")]
    syms = load_map(args[0])
    addrs = [s[0] for s in syms]
    slot = None
    if group:
        stem = os.path.splitext(args[0])[0]
        sec_of = elf_symbol_sections(elf or stem + ".elf")
        secs = group_sections(ld or os.path.join(os.path.dirname(args[0]) or ".", "ovl.ld"), group)
        in_slot = [s for s in syms if sec_of.get(s, "").startswith(".ovl_")]
        if in_slot:
            slot = {"lo": min(s[0] for s in in_slot), "hi": max(s[0] for s in in_slot) + 0x20000,
                    "mine": {s for s in in_slot if sec_of[s] in secs}}
    fn, pair, total = {}, {}, 0
    for line in open(args[1]):
        p = line.split()
        if len(p) != 3:
            continue
        pc, ra, c = int(p[0], 16), int(p[1], 16), int(p[2])
        f = name_of(syms, addrs, pc, slot)
        fn[f] = fn.get(f, 0) + c
        k = (f, name_of(syms, addrs, ra, slot))
        pair[k] = pair.get(k, 0) + c
        total += c
    print("%d samples%s" % (total, " (slot named by group %s)" % group if slot else ""))
    for f, c in sorted(fn.items(), key=lambda x: -x[1])[:n]:
        print("%6.1f%%  %s" % (100.0 * c / total, f))
    if callers:
        print("-- pc function <- ra function")
        for (f, r), c in sorted(pair.items(), key=lambda x: -x[1])[:n]:
            print("%6.1f%%  %s <- %s" % (100.0 * c / total, f, r))


if __name__ == "__main__":
    main()
