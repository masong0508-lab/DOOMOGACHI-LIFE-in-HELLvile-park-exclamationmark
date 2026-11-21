#!/usr/bin/env python3
"""Regression for the compact image PACKAGER, using a synthetic PS2-like ISO.

This checks compression and SHA256 consistency. It does not test game
execution or claim that the synthetic placeholder ELF can boot a PS2.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PACKAGER = ROOT / "ps2" / "tools" / "compact_iso.py"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sample_iso(tmp, omit=None, boot_path=r"cdrom0:\DOOMOG.ELF;1"):
    payload = tmp / "payload"
    payload.mkdir()
    files = {
        "SYSTEM.CNF": ("BOOT2 = " + boot_path + "\nVER = 1.00\nVMODE = NTSC\n").encode("ascii"),
        "DOOMOG.ELF": b"synthetic-elf-not-executable-" * 512,
        "TALK_EN.BIN": b"TALK" + bytes(range(256)) * 256,
        "AUDSRV.IRX": b"synthetic-irx-not-executable-" * 256,
    }
    for name, data in files.items():
        if name != omit:
            (payload / name).write_bytes(data)
    image = tmp / "tiny_ps2.iso"
    subprocess.run(
        ["xorriso", "-as", "mkisofs", "-iso-level", "1",
         "-V", "DOOMOTEST", "-o", str(image), str(payload)],
        check=True, capture_output=True, text=True,
    )
    return image


class CompactImageTests(unittest.TestCase):
    def test_lossless_chd_and_archive(self):
        with tempfile.TemporaryDirectory() as work:
            tmp = Path(work)
            image = sample_iso(tmp)
            original_hash = digest(image)
            out = tmp / "compact"
            result = subprocess.run(
                [sys.executable, str(PACKAGER), "--iso", str(image),
                 "--out-dir", str(out), "--zstd-level", "7"],
                text=True, capture_output=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(original_hash, digest(image))
            report = json.loads((out / "manifest.json").read_text())
            self.assertEqual(report["source_iso_sha256"], original_hash)
            self.assertEqual(report["schema_version"], 1)
            self.assertEqual(report["boot2_target"], r"CDROM0:\DOOMOG.ELF;1")
            self.assertEqual(len(report["images"]), 3)
            self.assertIn("TALK_EN.BIN", report["iso9660_files"])
            self.assertIn("AUDSRV.IRX", report["iso9660_files"])
            for image_entry in report["images"]:
                self.assertTrue(len(image_entry["sha256"]) == 64)
                if image_entry["format"] != "iso":
                    target = out / image_entry["file"]
                    self.assertTrue(target.is_file())
                    self.assertEqual(image_entry["sha256"], digest(target))
                    self.assertGreater(target.stat().st_size, 0)
                    self.assertEqual(image_entry["bytes"], target.stat().st_size)
            text = (out / "SIZE_REPORT.md").read_text()
            self.assertIn("byte-for-byte ISO round trips", text)
            checksums = (out / "SHA256SUMS.txt").read_text()
            self.assertIn(original_hash, checksums)

    def test_reject_double_slash_boot_path(self):
        with tempfile.TemporaryDirectory() as work:
            tmp = Path(work)
            bad_path = r"cdrom0:\\" + r"\DOOMOG.ELF;1"
            iso = sample_iso(tmp, boot_path=bad_path)
            out = tmp / "compact"
            result = subprocess.run(
                [sys.executable, str(PACKAGER), "--iso", str(iso),
                 "--out-dir", str(out)], text=True, capture_output=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("BOOT2", result.stderr)
            self.assertFalse(list(out.glob("*.chd")))
            self.assertFalse(list(out.glob("*.iso.zst")))

    def test_reject_missing_disc_assets(self):
        with tempfile.TemporaryDirectory() as work:
            tmp = Path(work)
            iso = sample_iso(tmp, omit="TALK_EN.BIN")
            out = tmp / "compact"
            result = subprocess.run(
                [sys.executable, str(PACKAGER), "--iso", str(iso),
                 "--out-dir", str(out)],
                text=True, capture_output=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("TALK_EN.BIN", result.stderr)
            self.assertFalse(list(out.glob("*.chd")))

    def test_reject_invalid_iso_and_level(self):
        with tempfile.TemporaryDirectory() as work:
            tmp = Path(work)
            bad = tmp / "bad.iso"
            bad.write_bytes(bytes(64 * 2048))
            out = tmp / "compact"
            result = subprocess.run(
                [sys.executable, str(PACKAGER), "--iso", str(bad),
                 "--out-dir", str(out)], text=True, capture_output=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("ISO9660", result.stderr)
            result = subprocess.run(
                [sys.executable, str(PACKAGER), "--iso", str(bad),
                 "--out-dir", str(out), "--zstd-level", "30"],
                text=True, capture_output=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("zstd-level", result.stderr)


if __name__ == "__main__":
    unittest.main()
