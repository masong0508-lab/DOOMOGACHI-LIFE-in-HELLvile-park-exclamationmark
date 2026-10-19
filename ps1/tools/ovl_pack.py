#!/usr/bin/env python3
"""ovl_pack.py - write the scene overlays of a linked PS1 ELF to OVL.BIN
(owner: ps1). Stdlib only, deterministic.

    ovl_pack.py game.elf OVL.BIN

The ELF's output sections .ovl_01 .. .ovl_NN (ovl_plan.py's linker
script) all have the slot as their address. OVL.BIN, one 2048-byte
sector per unit:

    sector 0   header, little-endian u32s:
               "DOVL", version 2, N, slot address, biggest group size,
               then per group 1..N: first sector, size in bytes, packed
               size (0 = stored raw)
    sector k   group bytes, every group starts on a sector boundary;
               packed with tools/packart.py's LZ (optimal parse) when that
               saves sectors: overlay.c reads the packed sectors into the
               END of the slot and decodes them in place to its start while
               the drive delivers them (~30% fewer sectors to read at ~150
               sectors/s on a 2x drive). A group is only packed when the
               decoder's output never overtakes its input in the slot.

src/overlay.c reads the header at boot and a group whenever a scene of it
starts.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from packart import lz, unlz  # noqa: E402

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


def symbol(data, want):
    """value of an ELF symbol (the slot's end, from the linker script)"""
    for name, h in sections(data):
        if h[1] == 2:                          # SHT_SYMTAB
            off, size, link, entsize = h[4], h[5], h[6], h[9]
            strh = list(sections(data))[link][1]
            for k in range(0, size, entsize):
                st_name, st_value = struct.unpack_from("<II", data, off + k)
                nm = data[strh[4] + st_name:data.index(b"\0", strh[4] + st_name)]
                if nm == want:
                    return st_value
    return None


def in_place_ok(packed, slot_bytes):
    """overlay.c decodes from the slot's tail to its head: after every
    token the bytes produced must not pass the next unread packed byte"""
    nsec = (len(packed) + SECTOR - 1) // SECTOR
    cstart = slot_bytes - nsec * SECTOR
    if cstart < 0:
        return False
    i = produced = 0
    while i < len(packed):
        c = packed[i]
        if c < 0x80:
            i += 1 + c + 1
            produced += c + 1
        else:
            i += 3
            produced += (c & 0x7F) + 3
        if produced > cstart + i:
            return False
    return True


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
    slot_end = symbol(data, b"__ovl_slot_end")
    slot_bytes = slot_end - slot if slot_end else 0
    hdr = [0x4C564F44, 2, len(groups), slot, biggest]
    out = bytearray()
    sector = 1
    raw_total = 0
    for _, _, body in groups:
        packed = lz(body) if body else b""
        assert unlz(packed, len(body)) == body
        raw_total += len(body)
        if (len(packed) + SECTOR - 1) // SECTOR < (len(body) + SECTOR - 1) // SECTOR and \
                in_place_ok(packed, slot_bytes):
            hdr += [sector, len(body), len(packed)]
            body = packed
        else:
            hdr += [sector, len(body), 0]
        pad = (-len(body)) % SECTOR
        out += body + b"\0" * pad
        sector += (len(body) + pad) // SECTOR
    head = struct.pack("<%dI" % len(hdr), *hdr)
    if len(head) > SECTOR:
        print("ovl_pack: too many groups for one header sector")
        return 1
    with open(argv[2], "wb") as f:
        f.write(head + b"\0" * (SECTOR - len(head)) + out)
    print("ovl_pack: %s: %d groups, %d KB (%d KB unpacked), slot 0x%08x (%d KB)" % (
        argv[2], len(groups), (SECTOR + len(out)) // 1024, raw_total // 1024, slot,
        biggest // 1024))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
