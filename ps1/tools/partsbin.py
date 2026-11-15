#!/usr/bin/env python3
"""partsbin.py - write PARTS.BIN, the PS1 disc's part scripts (owner: ps1, CR10).

    partsbin.py core/gen PARTS.BIN

core/gen/parts_<dir>.c (tools/parts_build.py) hold every part script as a raw byte array `ops[]`; the PS1 EXE
does not link them (DOOMO_PARTS_SPU, parts.h). src/partstore.c reads this file at boot and uploads the
scripts to SPU RAM, and the part VM reads one entry at a time back into a per-render arena.

CR10 also puts the packed part icons there (core/gen/part_icons_rle.c `doomo_part_icon_rle[]`, the bytes the
creator's grid unpacks: not linked on the PS1), as one more record after the sets.

Layout (2,048-byte sectors):
  sector 0   "PRTS", u16 nsets, u16 0, u32 total data bytes, u32 0 (16 bytes), then per set (in the order
             of doomo_part_scripts[], core/gen/parts_index.c): u32 offset of its scripts in the data, u32
             length, then the same pair for the icons; the rest zero
  sector 1.. the data: each set's raw scripts, then the packed icons, every one starting on a 64-byte boundary,
             the whole data padded to whole sectors
Stdlib only, deterministic.
"""
import os
import re
import struct
import sys

SECTOR = 2048
ALIGN = 64


def main(argv):
    if len(argv) != 3:
        sys.stderr.write(__doc__)
        return 2
    gen, out = argv[1], argv[2]
    index = open(os.path.join(gen, "parts_index.c"), encoding="utf-8").read()
    m = re.search(r"doomo_part_scripts\[\d+\]\s*=\s*\{(.*?)\};", index, re.S)
    dirs = re.findall(r"&doomo_parts_(\w+)", m.group(1)) if m else []
    if not dirs:
        sys.stderr.write("partsbin: no sets in parts_index.c\n")
        return 1
    if 16 + 8 * (len(dirs) + 1) > SECTOR:
        sys.stderr.write("partsbin: too many sets for one header sector\n")
        return 1
    data = bytearray()
    table = []
    for d in dirs:
        text = open(os.path.join(gen, "parts_%s.c" % d), encoding="utf-8").read()
        a = re.search(r"static const uint8_t ops\[\]\s*=\s*\{(.*?)\};", text, re.S)
        if not a:
            sys.stderr.write("partsbin: no ops[] in parts_%s.c\n" % d)
            return 1
        raw = bytes(int(v, 16) for v in re.findall(r"0x[0-9a-fA-F]{2}", a.group(1)))
        data += bytes(-len(data) % ALIGN)
        table.append((len(data), len(raw)))
        data += raw
    icons = open(os.path.join(gen, "part_icons_rle.c"), encoding="utf-8").read()
    m = re.search(r"doomo_part_icon_rle\[\d+\]\s*=\s*\{(.*?)\};", icons, re.S)
    if not m:
        sys.stderr.write("partsbin: no doomo_part_icon_rle[] in part_icons_rle.c\n")
        return 1
    ic = bytes(int(v) for v in re.findall(r"\d+", m.group(1)))
    ofs = re.search(r"doomo_part_icon_rle_ofs\[\d+\]\s*=\s*\{(.*?)\};", icons, re.S)
    last = [int(v) for v in re.findall(r"\d+", ofs.group(1))][-1]
    if last != len(ic):
        sys.stderr.write("partsbin: icon bytes %d, offsets say %d\n" % (len(ic), last))
        return 1
    data += bytes(-len(data) % ALIGN)
    table.append((len(data), len(ic)))
    data += ic
    data += bytes(-len(data) % SECTOR)
    head = b"PRTS" + struct.pack("<HHII", len(dirs), 0, len(data), 0)    # 16 bytes: the sets follow
    for ofs, n in table:
        head += struct.pack("<II", ofs, n)
    head += bytes(SECTOR - len(head))
    with open(out, "wb") as f:
        f.write(head + data)
    print("partsbin: %s: %d sets, %d bytes of scripts, %d bytes of packed icons, %d sectors" %
          (out, len(dirs), sum(n for _, n in table[:-1]), table[-1][1], 1 + len(data) // SECTOR))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
