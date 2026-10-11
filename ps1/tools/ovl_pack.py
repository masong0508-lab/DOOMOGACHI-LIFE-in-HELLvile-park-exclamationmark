#!/usr/bin/env python3
"""ovl_pack.py - write the scene overlays of a linked PS1 ELF to OVL.BIN
(owner: ps1). Stdlib only, deterministic.

    ovl_pack.py game.elf OVL.BIN

The ELF's output sections .ovl_01 .. .ovl_NN (ovl_plan.py's linker
script) all have the slot as their address. OVL.BIN, one 2048-byte
sector per unit:

    sector 0   header, little-endian u32s:
               "DOVL", version 1, N, slot address, biggest group size,
               then per group 1..N: first sector, size in bytes
    sector k   group bytes, every group starts on a sector boundary

src/overlay.c reads the header at boot and a group whenever a scene of it
starts.
"""
import struct
import sys

SECTOR = 2048


def sections(data):
    (e_shoff,) = struct.unpack_from("<I", data, 32)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", data, 46)
    hdrs = [struct.unpack_from("<IIIIIIIIII", data, e_shoff + i * e_shentsize)
            for i in range(e_shnum)]
    strtab = hdrs[e_shstrndx]
    for h in hdrs:
        name = data[strtab[4] + h[0]:data.index(b"\0", strtab[4] + h[0])].decode()
        yield name, h


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    data = open(argv[1], "rb").read()
    groups = []
    for name, h in sections(data):
        if name.startswith(".ovl_"):
            sh_type, sh_addr, sh_offset, sh_size = h[1], h[3], h[4], h[5]
            body = data[sh_offset:sh_offset + sh_size] if sh_type != 8 else b""
            groups.append((int(name[5:]), sh_addr, body))
    groups.sort()
    if [g[0] for g in groups] != list(range(1, len(groups) + 1)):
        print("ovl_pack: overlay sections are not .ovl_01 .. .ovl_%02d" % len(groups))
        return 1
    slots = {g[1] for g in groups}
    if len(slots) != 1:
        print("ovl_pack: overlays are not all linked at the same address")
        return 1
    slot = slots.pop() if groups else 0
    biggest = max((len(g[2]) for g in groups), default=0)
    hdr = [0x4C564F44, 1, len(groups), slot, biggest]
    out = bytearray()
    sector = 1
    for _, _, body in groups:
        hdr += [sector, len(body)]
        pad = (-len(body)) % SECTOR
        out += body + b"\0" * pad
        sector += (len(body) + pad) // SECTOR
    head = struct.pack("<%dI" % len(hdr), *hdr)
    if len(head) > SECTOR:
        print("ovl_pack: too many groups for one header sector")
        return 1
    with open(argv[2], "wb") as f:
        f.write(head + b"\0" * (SECTOR - len(head)) + out)
    print("ovl_pack: %s: %d groups, %d KB, slot 0x%08x (%d KB)" % (
        argv[2], len(groups), (SECTOR + len(out)) // 1024, slot, biggest // 1024))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
