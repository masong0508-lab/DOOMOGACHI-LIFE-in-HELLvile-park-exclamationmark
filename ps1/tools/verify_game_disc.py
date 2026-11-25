#!/usr/bin/env python3
"""Verify the actual MODE2/2352 game disc includes Talk and normal resources.

ISO9660 logical sectors are 2048 bytes; mkpsxiso writes them in raw
Mode 2 Form 1 sectors at byte 24. Read only the metadata and files we
verify, not the entire CD image into memory. Engine-test discs do not
use this check. This is a packaging gate, not an emulator playthrough.
"""
import argparse
import hashlib
from pathlib import Path
import struct

RAW = 2352
BLOCK = 2048
DATA_OFFSET = 24

def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]

def sector(disc, lba):
    disc.seek(lba * RAW)
    raw = disc.read(RAW)
    if (len(raw) != RAW or raw[:12] != b"\x00" + b"\xff" * 10 + b"\x00"
            or raw[15] != 2 or raw[18] & 0x20):
        raise ValueError(f"invalid Mode 2 Form 1 sector {lba}")
    return raw[DATA_OFFSET:DATA_OFFSET + BLOCK]

def extent(disc, lba, length):
    if length < 0:
        raise ValueError("negative extent")
    for i in range((length + BLOCK - 1) // BLOCK):
        yield sector(disc, lba + i)[:min(BLOCK, length - i * BLOCK)]

def root_files(disc):
    pvd = sector(disc, 16)
    if pvd[:7] != b"\x01CD001\x01":
        raise ValueError("missing ISO9660 primary volume descriptor")
    root = pvd[156:]
    lba, size = u32(root, 2), u32(root, 10)
    if not 0 < size <= 1024 * 1024:
        raise ValueError("invalid ISO root size")
    data = b"".join(extent(disc, lba, size))
    files = {}
    pos = 0
    while pos < len(data):
        n = data[pos]
        if not n:
            pos = (pos // BLOCK + 1) * BLOCK
            continue
        record = data[pos:pos + n]
        if n < 34 or len(record) != n or 33 + record[32] > n:
            raise ValueError("invalid ISO directory entry")
        if not record[25] & 2:
            name = record[33:33 + record[32]].decode("ascii").split(";")[0].upper()
            if name in files:
                raise ValueError(f"duplicate ISO file {name}")
            files[name] = (u32(record, 2), u32(record, 10))
        pos += n
    return files

def check_file(disc, files, name, source):
    lba, length = files[name]
    if length != source.stat().st_size or length <= 0:
        raise ValueError(f"{name}: size mismatch")
    h = hashlib.sha256()
    with source.open("rb") as src:
        for chunk in extent(disc, lba, length):
            if chunk != src.read(len(chunk)):
                raise ValueError(f"{name}: disc payload mismatch")
            h.update(chunk)
        if src.read(1):
            raise ValueError(f"{name}: extra source bytes")
    print(f"verified {name}: {length} bytes, sha256 {h.hexdigest()}")

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("image", type=Path)
    ap.add_argument("talk", type=Path)
    ap.add_argument("exe", type=Path)
    ap.add_argument("--require-intro", action="store_true")
    args = ap.parse_args()
    required = {"SYSTEM.CNF", "DOOMO.EXE", "OVL.BIN", "SFX.BIN", "SPRITES.BIN",
                "PARTS.BIN", "TEXT.BIN", "TALK_EN.BIN", "README.TXT"}
    if args.require_intro:
        required.add("INTRO.STR")
    with args.image.open("rb") as disc:
        files = root_files(disc)
        missing = sorted(required - files.keys())
        if missing:
            raise ValueError("full PS1 game disc missing: " + ", ".join(missing))
        if any(files[name][1] <= 0 for name in required):
            raise ValueError("empty required game file")
        check_file(disc, files, "DOOMO.EXE", args.exe)
        check_file(disc, files, "TALK_EN.BIN", args.talk)
        if files["TALK_EN.BIN"][1] > 4 * 1024 * 1024:
            raise ValueError("Talk bank exceeds PS1 streaming limit")
    print("Full PS1 game disc: normal resources and integrated Talk verified")

if __name__ == "__main__":
    main()
