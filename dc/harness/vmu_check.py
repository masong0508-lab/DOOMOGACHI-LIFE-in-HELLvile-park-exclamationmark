#!/usr/bin/env python3
"""vmu_check.py - check a VMU flash image (DC4; owner: dreamcast).

A VMU is 256 blocks of 512 bytes; Flycast keeps each one as a 128 KB file
(dcrun: RUN/system/dc/vmu_save_A1.bin). This reads it the way the
Dreamcast BIOS does, independently of the game's own C code (dc/src/vms.c):
the root block (255), the FAT, the directory, every file's block chain, and
each data file's VMS header (descriptions, application id, icon frames,
eyecatch, payload length, CRC-16 of the whole file).

    vmu_check.py IMAGE [--expect NAME]... [--app-id ID] [--json]
    vmu_check.py IMAGE --fill BLOCKS     add a filler file (another game's
                                         save) of BLOCKS blocks; makes the
                                         image first when IMAGE is missing
    vmu_check.py IMAGE --format          write a blank formatted VMU
    vmu_check.py IMAGE --icon NAME OUT.png   the file's icon frames and
                                         eyecatch, 4x, side by side

Checks (exit 1 when one fails): the root's magic, a FAT whose chains stay
inside the user blocks and match each file's size, no block in two files,
every data file's VMS header and CRC, our files' application id
(BOREAGOTCHI) and icon count (3 for saves, 1 for the config record), and
each --expect file present. Blocks allocated in the FAT but in no file
(left by a card pulled out mid-write) are reported, not failed.
"""
import argparse
import json
import os
import struct
import sys
import zlib

BLOCK = 512
APP_ID = "BOREAGOTCHI"
FAT_FREE, FAT_END = 0xFFFC, 0xFFFA


def crc16(data, crc=0):
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def blank_image():
    img = bytearray(256 * BLOCK)
    root = 255 * BLOCK
    img[root:root + 16] = b"\x55" * 16
    img[root + 0x10:root + 0x15] = b"\x01\xff\xff\xff\xff"
    img[root + 0x30:root + 0x38] = bytes([0x20, 0x26, 0x01, 0x01, 0, 0, 0, 0x03])
    img[root + 0x40] = 0xFF
    img[root + 0x44] = 0xFF
    struct.pack_into("<8H", img, root + 0x46, 254, 1, 253, 13, 0, 200, 31, 0)
    struct.pack_into("<H", img, root + 0x56, 128)
    fat = [FAT_FREE] * 256
    for b in range(242, 254):
        fat[b] = b - 1
    fat[241] = fat[254] = fat[255] = FAT_END
    struct.pack_into("<256H", img, 254 * BLOCK, *fat)
    return img


class Vmu:
    def __init__(self, img):
        self.img = img
        root = img[255 * BLOCK:256 * BLOCK]
        self.ok_magic = root[:16] == b"\x55" * 16
        (self.fat_loc, self.fat_size, self.dir_loc, self.dir_size, self.icon_shape,
         self.user) = struct.unpack_from("<6H", root, 0x46)
        self.fat = list(struct.unpack_from("<256H", img, self.fat_loc * BLOCK)) if self.ok_magic else []
        self.entries = []
        if not self.ok_magic:
            return
        for b in range(self.dir_size):
            blk = img[(self.dir_loc - b) * BLOCK:(self.dir_loc - b + 1) * BLOCK]
            for e in range(16):
                ent = blk[e * 32:e * 32 + 32]
                if ent[0]:
                    self.entries.append((b * 16 + e, ent))

    def block(self, b):
        return self.img[b * BLOCK:(b + 1) * BLOCK]

    def chain(self, first, size):
        out, b = [], first
        for n in range(size):
            if b >= self.user:
                return out, "chain leaves the user blocks at %d" % b
            out.append(b)
            nxt = self.fat[b]
            if n + 1 < size and nxt in (FAT_FREE, FAT_END):
                return out, "chain ends after %d of %d blocks" % (n + 1, size)
            if n + 1 == size and nxt != FAT_END:
                return out, "chain longer than the file's %d blocks" % size
            b = nxt
        return out, None


def parse_vms(data):
    if len(data) < 128:
        return None, "shorter than a VMS header"
    desc_s = data[0:16].decode("latin-1").rstrip(" \0")
    desc_l = data[16:48].decode("latin-1").rstrip(" \0")
    app = data[48:64].split(b"\0")[0].decode("latin-1").rstrip()
    icons, speed, ec, crc, dlen = struct.unpack_from("<4HI", data, 0x40)
    ec_size = {0: 0, 1: 72 * 56 * 2, 2: 512 + 72 * 56, 3: 32 + 72 * 56 // 2}.get(ec)
    if ec_size is None:
        return None, "unknown eyecatch type %d" % ec
    off = 128 + 512 * icons + ec_size
    total = off + dlen
    info = dict(desc_short=desc_s, desc_long=desc_l, app_id=app, icons=icons, anim_speed=speed,
                eyecatch=ec, data_off=off, data_len=dlen, total=total, crc=crc)
    if total > len(data):
        return info, "header says %d bytes, the file has %d" % (total, len(data))
    calc = crc16(bytes(data[:0x46]) + b"\0\0" + bytes(data[0x48:total]))
    if calc != crc:
        return info, "CRC %04x, the header says %04x" % (calc, crc)
    return info, None


def check(img, expect, app_id):
    v = Vmu(img)
    problems, files = [], []
    if not v.ok_magic:
        return files, ["not a formatted VMU (root block magic)"], 0, 0
    if v.fat_size != 1 or v.user > 256:
        problems.append("odd root: FAT size %d, user blocks %d" % (v.fat_size, v.user))
    owner = {}
    for idx, ent in v.entries:
        ftype, copy, first = struct.unpack_from("<BBH", ent, 0)
        name = ent[4:16].split(b"\0")[0].decode("latin-1").rstrip()
        size, hdroff = struct.unpack_from("<HH", ent, 0x18)
        f = dict(name=name, type="data" if ftype == 0x33 else "game" if ftype == 0xCC else hex(ftype),
                 first=first, blocks=size, entry=idx, copyable=copy == 0)
        blocks, err = v.chain(first, size)
        if err:
            problems.append("%s: %s" % (name, err))
        for b in blocks:
            if b in owner:
                problems.append("%s: block %d is also in %s" % (name, b, owner[b]))
            owner[b] = name
        if not err and ftype == 0x33:
            data = b"".join(v.block(b) for b in blocks)
            info, verr = parse_vms(data[hdroff * BLOCK:])
            if info:
                f.update(info)
            if verr:
                problems.append("%s: %s" % (name, verr))
            elif info["app_id"] == app_id:
                want_icons = 1 if name.startswith("DOOMOCFG") else 3
                if info["icons"] != want_icons:
                    problems.append("%s: %d icon frames, wanted %d" % (name, info["icons"], want_icons))
                if (info["total"] + BLOCK - 1) // BLOCK != size:
                    problems.append("%s: %d blocks for %d bytes" % (name, size, info["total"]))
                if name.endswith(".NEW"):
                    problems.append("%s: a save's temporary file was left behind" % name)
        files.append(f)
    names = {f["name"] for f in files}
    for e in expect:
        if e not in names:
            problems.append("%s: not on the VMU" % e)
    free = sum(1 for b in range(v.user) if v.fat[b] == FAT_FREE)
    leaked = sum(1 for b in range(v.user) if v.fat[b] != FAT_FREE and b not in owner)
    return files, problems, free, leaked


def fill(img, blocks):
    """a filler data file (VMS header, payload of 0xEE) from the top down"""
    v = Vmu(img)
    free = [b for b in range(v.user - 1, -1, -1) if v.fat[b] == FAT_FREE]
    if blocks > len(free):
        sys.exit("vmu_check: only %d free blocks" % len(free))
    hdr = bytearray(128 + 512)
    hdr[0:16] = b"FILLER".ljust(16)
    hdr[16:48] = b"vmu_check.py --fill".ljust(32)
    hdr[48:54] = b"FILLER"
    dlen = blocks * BLOCK - len(hdr)
    struct.pack_into("<4HI", hdr, 0x40, 1, 0, 0, 0, dlen)
    struct.pack_into("<H", hdr, 0x60, 0xF000)
    data = bytes(hdr) + b"\xee" * dlen
    struct.pack_into("<H", hdr, 0x46, crc16(data))
    data = bytes(hdr) + b"\xee" * dlen
    use = free[:blocks]
    for i, b in enumerate(use):
        img[b * BLOCK:(b + 1) * BLOCK] = data[i * BLOCK:(i + 1) * BLOCK]
        v.fat[b] = use[i + 1] if i + 1 < len(use) else FAT_END
    struct.pack_into("<256H", img, v.fat_loc * BLOCK, *v.fat)
    for b in range(v.dir_size):
        base = (v.dir_loc - b) * BLOCK
        for e in range(16):
            o = base + e * 32
            if img[o] == 0:
                ent = bytearray(32)
                struct.pack_into("<BBH", ent, 0, 0x33, 0, use[0])
                ent[4:16] = b"FILLER.BIN".ljust(12, b"\0")
                struct.pack_into("<HH", ent, 0x18, blocks, 0)
                img[o:o + 32] = ent
                return
    sys.exit("vmu_check: the directory is full")


def write_png(path, rgb, w, h):
    raw = b"".join(b"\0" + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def icon_png(img, name, out, zoom=4):
    v = Vmu(img)
    for _, ent in v.entries:
        if ent[4:16].split(b"\0")[0].decode("latin-1").rstrip() != name:
            continue
        first, = struct.unpack_from("<H", ent, 2)
        size, = struct.unpack_from("<H", ent, 0x18)
        blocks, err = v.chain(first, size)
        if err:
            sys.exit("vmu_check: %s: %s" % (name, err))
        data = b"".join(v.block(b) for b in blocks)
        info, _ = parse_vms(data)
        pal = struct.unpack_from("<16H", data, 0x60)
        pics = []
        for k in range(info["icons"]):
            bits = data[128 + k * 512:128 + (k + 1) * 512]
            pics.append((32, 32, bits, pal))
        if info["eyecatch"] == 3:
            o = 128 + 512 * info["icons"]
            pics.append((72, 56, data[o + 32:o + 32 + 2016], struct.unpack_from("<16H", data, o)))
        W = sum(p[0] + 2 for p in pics) * zoom
        H = 56 * zoom
        rgb = bytearray([40]) * (W * H * 3)
        x0 = 0
        for (w, h, bits, pl) in pics:
            for y in range(h):
                for x in range(w):
                    byte = bits[y * (w // 2) + x // 2]
                    c = pl[(byte & 15) if x & 1 else byte >> 4]
                    if c >> 12:
                        col = (((c >> 8) & 15) * 17, ((c >> 4) & 15) * 17, (c & 15) * 17)
                        for j in range(zoom):
                            for i in range(zoom):
                                p = ((y * zoom + j) * W + (x0 + x) * zoom + i) * 3
                                rgb[p:p + 3] = bytes(col)
            x0 += w + 2
        write_png(out, rgb, W, H)
        return
    sys.exit("vmu_check: no file %s" % name)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("image")
    ap.add_argument("--expect", action="append", default=[])
    ap.add_argument("--app-id", default=APP_ID)
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--fill", type=int)
    ap.add_argument("--format", action="store_true")
    ap.add_argument("--icon", nargs=2, metavar=("NAME", "OUT"))
    a = ap.parse_args()
    if a.format or (a.fill is not None and not os.path.exists(a.image)):
        os.makedirs(os.path.dirname(os.path.abspath(a.image)), exist_ok=True)
        with open(a.image, "wb") as f:
            f.write(blank_image())
    with open(a.image, "rb") as f:
        img = bytearray(f.read())
    if len(img) != 256 * BLOCK:
        sys.exit("vmu_check: %s is %d bytes, a VMU is %d" % (a.image, len(img), 256 * BLOCK))
    if a.fill is not None:
        fill(img, a.fill)
        with open(a.image, "wb") as f:
            f.write(img)
    if a.icon:
        icon_png(img, a.icon[0], a.icon[1])
    files, problems, free, leaked = check(img, a.expect, a.app_id)
    if a.json:
        print(json.dumps(dict(files=files, free=free, leaked=leaked, problems=problems), indent=1))
    else:
        for f in files:
            line = "%-12s %-4s %3d blocks @%3d" % (f["name"], f["type"], f["blocks"], f["first"])
            if "app_id" in f:
                line += "  %-11s %d icon%s ec %d  %6d B payload  CRC %04x  %s" % (
                    f["app_id"], f["icons"], "" if f["icons"] == 1 else "s", f["eyecatch"],
                    f["data_len"], f["crc"], f["desc_long"])
            print(line)
        print("vmu_check: %d files, %d blocks free, %d leaked, %d problem%s" % (
            len(files), free, leaked, len(problems), "" if len(problems) == 1 else "s"))
        for p in problems:
            print("  PROBLEM " + p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
