#!/usr/bin/env python3
"""Build two lossless compact variants of an existing PS2 DVD ISO.

CHD is emulator-ready (PCSX2 2.0+). ISO.ZST is an archive, not directly
bootable. The upstream ISO remains available for native hardware/loaders.

No compressed format is called verified without a byte-exact round trip.
The script requires xorriso, MAME chdman, zstd and Python 3 on the host.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tempfile
from check_ps2_boot import verify_image_boot

SECTOR_SIZE = 2048
REQUIRED = {"SYSTEM.CNF", "DOOMOG.ELF", "TALK_EN.BIN", "AUDSRV.IRX"}


def run(*command):
    print("+", " ".join(str(c) for c in command), flush=True)
    subprocess.run([str(c) for c in command], check=True)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as reader:
        for block in iter(lambda: reader.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def check_iso(iso):
    if not iso.is_file() or iso.stat().st_size < 32 * SECTOR_SIZE:
        raise ValueError("ISO missing or too small to contain an ISO9660 volume")
    if iso.stat().st_size % SECTOR_SIZE:
        raise ValueError("ISO length must be a multiple of 2048 bytes")
    with iso.open("rb") as reader:
        reader.seek(16 * SECTOR_SIZE)
        pvd = reader.read(7)
    if pvd != b"\x01CD001\x01":
        raise ValueError("Not a standard ISO9660 primary volume descriptor")

    listing = subprocess.run(
        ["xorriso", "-indev", str(iso), "-find", "/", "-type", "f"],
        text=True, capture_output=True, check=True
    )
    # xorriso sends its result channel to stderr on some installations
    # and may quote ISO paths (e.g. '/DOOMOG.ELF'). Parse only complete
    # absolute result paths, never diagnostics mentioning filenames.
    names = set()
    for line in (listing.stdout + "\n" + listing.stderr).splitlines():
        entry = line.strip()
        if len(entry) >= 2 and entry[0] == entry[-1] and entry[0] in "'\"":
            entry = entry[1:-1]
        if entry.startswith("/"):
            names.add(PurePosixPath(entry).name.split(";", 1)[0].upper())
    missing = sorted(REQUIRED - names)
    if missing:
        raise ValueError(
            "PS2 boot/data files missing from ISO: " + ", ".join(missing)
            + "; files seen: " + ", ".join(sorted(names))
            + "; xorriso result: " + repr((listing.stdout + listing.stderr)[-350:])
        )
    return sorted(names)


def checked_copy(original, restored, label):
    if not restored.is_file() or original.stat().st_size != restored.stat().st_size:
        raise ValueError(f"{label}: decoded ISO size differs from source")
    source_hash, restored_hash = sha256(original), sha256(restored)
    if source_hash != restored_hash:
        raise ValueError(f"{label}: SHA256 mismatch after decode; refusing release")
    print(f"{label} decoded ISO exactly: {source_hash}")
    return source_hash


def size_row(path, raw_bytes, name, mode):
    nbytes = path.stat().st_size
    return {
        "format": mode,
        "file": name,
        "bytes": nbytes,
        "sha256": sha256(path),
        "saved_bytes": raw_bytes - nbytes,
        "saved_percent": round(100 * (raw_bytes - nbytes) / raw_bytes, 2),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--zstd-level", type=int, default=15)
    args = parser.parse_args()

    if not 1 <= args.zstd_level <= 19:
        parser.error("--zstd-level must be between 1 and 19")
    iso = args.iso.expanduser().resolve(strict=True)
    out = args.out_dir.expanduser().resolve()
    out.mkdir(parents=True, exist_ok=True)
    for command in ("xorriso", "chdman", "zstd"):
        if shutil.which(command) is None:
            raise ValueError(f"Missing required compression program: {command}")

    disc_files = check_iso(iso)
    boot_target = verify_image_boot(iso)
    initial_sha = sha256(iso)
    raw_size = iso.stat().st_size
    name = iso.stem
    chd = out / (name + ".chd")
    archive = out / (iso.name + ".zst")
    for file in (chd, archive):
        if file.exists():
            file.unlink()

    with tempfile.TemporaryDirectory(prefix="ps2-verify-", dir=out) as work:
        restored = Path(work) / "restored.iso"

        # A DVD ISO uses createdvd/extractdvd, not createcd/extractcd.
        # MAME's default DVD codecs prioritize broad CHD compatibility.
        run("chdman", "createdvd", "-i", iso, "-o", chd)
        run("chdman", "verify", "-i", chd)
        run("chdman", "extractdvd", "-i", chd, "-o", restored)
        checked_copy(iso, restored, "CHD")
        restored.unlink()

        run("zstd", f"-{args.zstd_level}", "-T2", "-q", "-f", "-o", archive, iso)
        run("zstd", "-t", "-q", archive)
        run("zstd", "-d", "-q", "-f", "-o", restored, archive)
        checked_copy(iso, restored, "Zstandard archive")

    images = [
        {
            "format": "iso",
            "file": iso.name,
            "bytes": raw_size,
            "sha256": initial_sha,
            "saved_bytes": 0,
            "saved_percent": 0.0,
            "purpose": "baseline image; keep for native PS2/OPL and emulator tests",
        },
        size_row(chd, raw_size, chd.name, "chd"),
        size_row(archive, raw_size, archive.name, "iso.zst"),
    ]
    images[1]["purpose"] = "lossless DVD CHD for compatible emulators such as PCSX2"
    images[2]["purpose"] = "compressed backup/download; decompress before launching"
    manifest = {
        "schema_version": 1,
        "branch_commit": os.environ.get("GITHUB_SHA", "local"),
        "source_iso_sha256": initial_sha,
        "boot2_target": boot_target,
        "iso9660_files": disc_files,
        "roundtrip": "both compressed formats restored byte-identical ISO",
        "images": images,
    }
    (out / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    (out / "SHA256SUMS.txt").write_text(
        "".join(f'{img["sha256"]}  {img["file"]}\n' for img in images),
        encoding="ascii",
    )
    report = [
        "### PS2 lossless compact disc images",
        "",
        "| Format | Size (MiB) | Savings vs ISO | SHA-256 verified |",
        "|:--|--:|--:|:--|",
    ]
    for image in images:
        report.append(
            f'| {image["format"]} | {image["bytes"] / 1048576:.2f} | '
            f'{image["saved_percent"]:+.2f}% | yes |'
        )
    report += [
        "",
        "**CHD and ISO.ZST passed byte-for-byte ISO round trips.** "
        "A successful conversion does not prove emulator gameplay.",
        "Keep the original ISO for hardware/loaders that do not support CHD.",
        "ISO.ZST is a download/archive, not a bootable disc image.",
        "",
    ]
    (out / "SIZE_REPORT.md").write_text("\n".join(report), encoding="utf-8")
    print("\n".join(report))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"Compact PS2 image packaging FAILED: {exc}", file=sys.stderr)
        sys.exit(1)
