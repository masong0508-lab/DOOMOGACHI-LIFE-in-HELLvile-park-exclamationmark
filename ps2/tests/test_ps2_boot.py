"""Regression tests for the PS2 ELF boot path on the actual disc image."""
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from check_ps2_boot import EXPECTED, read_root_file, validate_boot_text


class Ps2BootPathTests(unittest.TestCase):
    def test_real_source_matches_valid_ps2_boot2_path(self):
        self.assertEqual(
            validate_boot_text((ROOT / "SYSTEM.CNF").read_bytes()),
            EXPECTED
        )

    def test_invalid_double_backslash_rejected(self):
        double = "cdrom0:" + chr(92) * 2 + "DOOMOG.ELF;1"
        with self.assertRaisesRegex(ValueError, "exactly one backslash"):
            validate_boot_text(("BOOT2 = " + double + "\n").encode("ascii"))

    def test_wrong_executable_and_duplicate_keys(self):
        for bad in (
            b"BOOT2 = cdrom0:\\BAD.ELF;1\n",
            b"VER = 1.00\n",
            b"BOOT2=cdrom0:\\DOOMOG.ELF;1\nBOOT2=cdrom0:\\DOOMOG.ELF;1\n",
            b"BOOT2=\xff\n",
        ):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                validate_boot_text(bad)

    def test_reject_non_iso(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "bad.iso"
            path.write_bytes(bytes(64 * 2048))
            with self.assertRaisesRegex(ValueError, "ISO9660"):
                read_root_file(path, "SYSTEM.CNF")


if __name__ == "__main__":
    unittest.main()
