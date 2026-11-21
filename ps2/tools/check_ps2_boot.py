#!/usr/bin/env python3
"""Validate that a PS2 ISO actually boots its bundled DOOMOG.ELF.

Read SYSTEM.CNF *inside the ISO9660 root directory*, not just the source
tree. This catches a real production bug where BOOT2 used two literal
backslashes (cdrom0:\\DOOMOG.ELF;1) instead of one. The game compiled
and the ISO passed CHD/zstd round trips but the disc's boot path was bad.

Only Python's standard library is required; this also runs in the
minimal ps2dev container without external ISO extraction tools.
"""
import argparse
from pathlib import Path
import sys

SECTOR = 2048
EXPECTED = r"CDROM0:\DOOMOG.ELF;1"


def read_root_file(iso, filename):
    iso = Path(iso)
    with iso.open("rb") as stream:
        stream.seek(16 * SECTOR)
        descriptor = stream.read(SECTOR)
        if len(descriptor) != SECTOR or descriptor[:7] != b"\x01CD001\x01":
            raise ValueError("Not a valid ISO9660 primary volume descriptor")
        root = descriptor[156:]
        if len(root) < 34 or root[0] < 34 or (root[25] & 2) == 0:
            raise ValueError("Invalid ISO9660 root directory record")
        lba = int.from_bytes(root[2:6], "little")
        size = int.from_bytes(root[10:14], "little")
        if not lba or not 0 < size <= 1024 * 1024:
            raise ValueError("Invalid ISO9660 root directory extent")
        stream.seek(lba * SECTOR)
        directory = stream.read(size)
        if len(directory) != size:
            raise ValueError("Truncated ISO9660 root directory")
        i = 0
        while i < len(directory):
            n = directory[i]
            if n == 0:
                i = (i // SECTOR + 1) * SECTOR
                continue
            if n < 34 or i + n > len(directory):
                raise ValueError("Invalid ISO9660 directory record")
            record = directory[i:i+n]
            count = record[32]
            if count < 1 or 33 + count > n:
                raise ValueError("Invalid ISO9660 filename length")
            try:
                entry = record[33:33+count].decode("ascii")
            except UnicodeDecodeError as exc:
                raise ValueError("Non-ASCII ISO9660 filename") from exc
            if entry.split(";", 1)[0].upper() == filename.upper():
                if record[25] & 2:
                    raise ValueError("SYSTEM.CNF is a directory, not a file")
                file_lba = int.from_bytes(record[2:6], "little")
                length = int.from_bytes(record[10:14], "little")
                if not file_lba or length < 12 or length > 4096:
                    raise ValueError("Invalid SYSTEM.CNF extent or length")
                stream.seek(file_lba * SECTOR)
                payload = stream.read(length)
                if len(payload) != length:
                    raise ValueError("Truncated SYSTEM.CNF inside ISO")
                return payload
            i += n
    raise ValueError("SYSTEM.CNF missing from ISO root directory")


def validate_boot_text(data):
    try:
        lines = data.decode("ascii").splitlines()
    except UnicodeDecodeError as exc:
        raise ValueError("SYSTEM.CNF is not ASCII") from exc
    paths = []
    for line in lines:
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        if key.strip().upper() == "BOOT2":
            paths.append(value.strip().upper())
    if len(paths) != 1:
        raise ValueError("SYSTEM.CNF must contain exactly one BOOT2 entry")
    if paths[0] != EXPECTED:
        raise ValueError(
            f"Incorrect BOOT2 path {paths[0]!r}; expected {EXPECTED!r}"
            " (exactly one backslash after cdrom0:)"
        )
    return EXPECTED


def verify_image_boot(iso, source=None):
    disc = read_root_file(iso, "SYSTEM.CNF")
    value = validate_boot_text(disc)
    if source is not None and disc != Path(source).read_bytes():
        raise ValueError("Disc SYSTEM.CNF differs from the checked-in source")
    print(f"PS2 ISO boot path verified: BOOT2 = {value}")
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", required=True, type=Path)
    parser.add_argument("--source", type=Path, help="Optional source SYSTEM.CNF equality check")
    args = parser.parse_args()
    try:
        verify_image_boot(args.iso, args.source)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"PS2 BOOT2 VALIDATION FAILED: {exc}\n")


if __name__ == "__main__":
    main()
