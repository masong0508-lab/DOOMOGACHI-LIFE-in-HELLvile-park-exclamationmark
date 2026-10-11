#!/usr/bin/env python3
"""elf2exe.py - convert a PSn00bSDK ELF into a PlayStation PS-X EXE.

Why not the SDK's elf2x? With the apt `mipsel-linux-gnu` toolchain the
linker always emits a PT_GNU_STACK program header at vaddr 0, and elf2x
takes the lowest p_vaddr of *every* program header as the load address,
which yields a 2 GB "executable". This converter only looks at PT_LOAD
segments with a non-zero file size, pads to 2 KB sectors, fills in the BSS
fields and refuses images that do not fit in the console's 2 MB of RAM.
Segments whose load address differs from their run address are the scene
overlays (ovl_plan.py / ovl_pack.py): they are left out of the EXE, but
the RAM slot they run in counts towards the 2 MB.

usage: elf2exe.py input.elf output.exe [--region NA|EU|JP]
Stdlib only, deterministic output.
"""
import struct
import sys

PT_LOAD = 1


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    src, dst = argv[1], argv[2]
    region = "NA"
    if "--region" in argv:
        region = argv[argv.index("--region") + 1].upper()
    data = open(src, "rb").read()
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        print("elf2exe: not a 32-bit little-endian ELF: %s" % src)
        return 1
    (e_entry,) = struct.unpack_from("<I", data, 24)
    (e_phoff,) = struct.unpack_from("<I", data, 28)
    (e_shoff,) = struct.unpack_from("<I", data, 32)
    e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx = struct.unpack_from(
        "<HHHHH", data, 42)
    segs = []
    bss_lo, bss_hi = None, None
    ovl_hi = None
    for i in range(e_phnum):
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = \
            struct.unpack_from("<IIIIIIII", data, e_phoff + i * e_phentsize)
        if p_type != PT_LOAD or p_memsz == 0:
            continue
        if p_paddr != p_vaddr:
            # a scene overlay (ovl_plan.py): its bytes go to OVL.BIN
            # (ovl_pack.py), only the RAM slot it runs in counts here
            ovl_hi = max(ovl_hi or 0, p_vaddr + p_memsz)
            continue
        if p_filesz:
            segs.append((p_vaddr, data[p_offset:p_offset + p_filesz]))
        if p_memsz > p_filesz:
            lo, hi = p_vaddr + p_filesz, p_vaddr + p_memsz
            bss_lo = lo if bss_lo is None else min(bss_lo, lo)
            bss_hi = hi if bss_hi is None else max(bss_hi, hi)
    if not segs:
        print("elf2exe: no loadable segments")
        return 1
    lo = min(a for a, _ in segs)
    hi = max(a + len(b) for a, b in segs)
    size = (hi - lo + 2047) & ~2047
    image = bytearray(size)
    for a, b in segs:
        image[a - lo:a - lo + len(b)] = b
    # find _gp from the symbol table (not needed by the BIOS, informative)
    gp = 0
    sym_end = 0
    try:
        for i in range(e_shnum):
            sh = struct.unpack_from("<IIIIIIIIII", data, e_shoff + i * e_shentsize)
            if sh[1] == 2:  # SHT_SYMTAB
                strtab = struct.unpack_from("<IIIIIIIIII", data, e_shoff + sh[6] * e_shentsize)
                for j in range(sh[5] // 16):
                    st_name, st_value = struct.unpack_from("<II", data, sh[4] + j * 16)
                    s = data[strtab[4] + st_name:data.index(b"\0", strtab[4] + st_name)]
                    if s == b"_gp":
                        gp = st_value
                    elif s == b"_end":
                        sym_end = st_value
    except Exception:
        gp = 0
    end = max(bss_hi if bss_hi is not None else hi, ovl_hi or 0, sym_end) - 0x80000000
    if end > 0x200000 - 0x10000:
        print("elf2exe: image + bss end at 0x%x, leaves no room for the stack "
              "in 2 MB RAM" % (end + 0x80000000))
        return 1
    hdr = bytearray(2048)
    hdr[0:8] = b"PS-X EXE"
    struct.pack_into("<IIIIIIIIII", hdr, 0x10,
                     e_entry, gp, lo, size, 0, 0,
                     bss_lo or 0, (bss_hi - bss_lo) if bss_lo is not None else 0,
                     0x801FFFF0, 0)
    marker = {
        "NA": b"Sony Computer Entertainment Inc. for North America area",
        "EU": b"Sony Computer Entertainment Inc. for Europe area",
        "JP": b"Sony Computer Entertainment Inc. for Japan area",
    }[region]
    hdr[0x4C:0x4C + len(marker)] = marker
    with open(dst, "wb") as f:
        f.write(hdr)
        f.write(image)
    print("elf2exe: %s load 0x%08x size %d KB (+bss %d KB, ends 0x%08x, %d KB left "
          "below the stack top), entry 0x%08x" % (
              dst, lo, size // 1024,
              ((bss_hi - bss_lo) if bss_lo is not None else 0) // 1024,
              end + 0x80000000, (0x1FFFF0 - end) // 1024, e_entry))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
