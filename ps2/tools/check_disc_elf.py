#!/usr/bin/env python3
"""Prove that a stripped PS2 disc ELF loads exactly the debug ELF's bytes.

The PS2's EE loader uses ELF program headers, not DWARF section metadata.
Removing only non-loaded debug sections reduces disc storage without
changing the program or its runtime RAM footprint.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import sys

HEADER = struct.Struct("<HHIIIIIHHHHHH")
PHDR = struct.Struct("<IIIIIIII")
PT_LOAD = 1


def loaded_elf(path):
    raw = Path(path).read_bytes()
    if len(raw) < 52 or raw[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError(f"{path}: not a little-endian ELF32")
    header = HEADER.unpack_from(raw, 16)
    e_entry, phoff, phentsize, phnum = header[3], header[4], header[8], header[9]
    if phentsize != PHDR.size or not phnum or phnum > 128:
        raise ValueError(f"{path}: invalid program-header dimensions")
    if phoff + phnum * phentsize > len(raw):
        raise ValueError(f"{path}: truncated program headers")
    segments = []
    for i in range(phnum):
        p_type, off, vaddr, paddr, size, memsz, flags, align = PHDR.unpack_from(
            raw, phoff + i * phentsize
        )
        if p_type != PT_LOAD:
            continue
        if off + size > len(raw) or size > memsz:
            raise ValueError(f"{path}: invalid executable load segment")
        segments.append((vaddr, paddr, size, memsz, flags, align,
                         hashlib.sha256(raw[off:off + size]).hexdigest()))
    if not segments:
        raise ValueError(f"{path}: no loadable program segments")
    return e_entry, tuple(segments), len(raw)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("debug_elf")
    parser.add_argument("disc_elf")
    args = parser.parse_args()
    a = loaded_elf(args.debug_elf)
    b = loaded_elf(args.disc_elf)
    if a[:2] != b[:2]:
        raise ValueError("STRIP CHANGED PS2 LOADABLE GAME BYTES OR ENTRY POINT")
    if b[2] >= a[2]:
        raise ValueError("Disc ELF is not smaller than its unstripped source")
    saved = a[2] - b[2]
    print(f"PS2 disc ELF validated: {a[2]:,} -> {b[2]:,} bytes"
          f" ({100 * saved / a[2]:.1f}% smaller), unchanged EE load segments")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, struct.error) as exc:
        print(f"FAIL PS2 debug stripping: {exc}", file=sys.stderr)
        sys.exit(1)
