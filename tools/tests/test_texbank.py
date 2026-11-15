#!/usr/bin/env python3
"""test_texbank.py - LV7a: tools/texbank.py (the per-level texture bank generator).

Checks the blob against the format in core/include/doomo/texbank.h, determinism, that the committed
core/gen/tb_sample.c is the generator's output, every refusal, and that the sample compiles as the
PS1 sees it (to nothing). Needs a host gcc for the last one only."""
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import texbank  # noqa: E402

SAMPLE = os.path.join(texbank.BANK_DIR, "sample.bank")
OK = "bank t\ntex a fill STONE.3\n"


def err(text):
    try:
        texbank.build(text)
    except texbank.BankError as e:
        return str(e)
    return None


class Format(unittest.TestCase):
    def test_sample_layout(self):
        blob = texbank.build_file(SAMPLE)
        self.assertEqual(blob[:4], b"LVTB")
        self.assertEqual(blob[4], 1)
        self.assertEqual(blob[5], 3)
        self.assertEqual(int.from_bytes(blob[8:12], "little"), len(blob))
        self.assertEqual(len(blob), 12 + 3 * 16 + 4 * 4096)
        self.assertEqual(blob[12:21], b"vat_brick")
        self.assertEqual(blob[12 + 9:12 + 12], b"\0\0\0")
        self.assertEqual(list(blob[12 + 12:12 + 15]), [1, 1, 0])
        self.assertEqual(list(blob[28 + 12:28 + 15]), [2, 12, 1])      # ink_stripe: 2 frames, speed 12, pingpong
        self.assertTrue(all(blob[12 + 16 * i + 15] != 0 for i in range(3)))

    def test_deterministic_and_committed(self):
        self.assertEqual(texbank.build_file(SAMPLE), texbank.build_file(SAMPLE))
        with open(os.path.join(ROOT, "core", "gen", "tb_sample.c")) as f:
            self.assertEqual(f.read(), texbank.c_source("sample", texbank.build_file(SAMPLE)))

    def test_textures_tile(self):
        blob = texbank.build_file(SAMPLE)
        px = blob[60:60 + 4096]
        # a seamless tile: wrapping the picture one pixel changes few neighbours' relation, so check the
        # bricks repeat with their 64 px period by construction and use more than one colour
        self.assertGreater(len(set(px)), 2)
        # stripes: frame 1 is frame 0 slid by half a period
        f0 = blob[60 + 4096:60 + 8192]
        f1 = blob[60 + 8192:60 + 12288]
        self.assertNotEqual(f0, f1)
        self.assertEqual(f0[0], f1[8])      # N = 8: shift f * 2N / frames = 8

    def test_colour_zero_is_clear_and_refused(self):
        self.assertIsNotNone(err("bank t\ntex a fill PAL.0\n"))
        self.assertIsNone(err(OK))

    def test_colours(self):
        self.assertEqual(texbank.colour("STONE.0"), 136)
        self.assertEqual(texbank.colour("STEEL.7"), 136 + 14 * 8 + 7)
        self.assertEqual(texbank.colour("PAL.5"), 5)

    def test_pack_ready_bin(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "s.bin")
            self.assertEqual(texbank.main(["--bin", "sample", out]), 0)
            with open(out, "rb") as f:
                self.assertEqual(f.read(), texbank.build_file(SAMPLE))


class Refusals(unittest.TestCase):
    def test_each(self):
        cases = [
            ("", "empty bank"),
            ("tex a fill STONE.3\n", "`bank NAME` comes first"),
            ("bank T\ntex a fill STONE.3\n", "lowercase"),
            ("bank t\nbank u\n", "once"),
            ("bank t\ntex Aa fill STONE.3\n", "slot name"),
            ("bank t\ntex abcdefghijkl fill STONE.3\n", "at most 11"),
            ("bank t\ntex a fill STONE.3\ntex a fill STONE.4\n", "twice"),
            ("bank t\ntex a fill NOPE.3\n", "no ramp"),
            ("bank t\ntex a fill STONE.8\n", "shade 0..7"),
            ("bank t\ntex a fill\n", "fill C"),
            ("bank t\ntex a blob STONE.3\n", "no kind"),
            ("bank t\ntex a checker STONE.1 STONE.2 7\n", "does not divide"),
            ("bank t\ntex a stripes STONE.1 STONE.2 24\n", "outside"),
            ("bank t\ntex a stripes STONE.1 STONE.2 32\n", "outside"),
            ("bank t\ntex a frames=5 noise STONE.1 STONE.2 1\n", "frames 5 outside"),
            ("bank t\ntex a frames=2 fill STONE.3\n", "only stripes and noise"),
            ("bank t\ntex a mode=flash fill STONE.3\n", "loop or pingpong"),
            ("bank t\ntex a speed=0 fill STONE.3\n", "speed 0 outside"),
            ("bank t\ntex a size=3 fill STONE.3\n", "option"),
            ("bank t\nwall a\n", "unknown statement"),
            ("bank t\n" + "".join("tex s%d fill STONE.3\n" % i for i in range(9)), "at most 8"),
        ]
        for text, want in cases:
            e = err(text)
            self.assertIsNotNone(e, "accepted: %r" % text)
            self.assertIn(want, e, "%r -> %s" % (text, e))

    def test_eight_slots_of_four_frames_is_the_cap_and_fits(self):
        blob = texbank.build("bank t\n" + "".join("tex s%d frames=4 noise STONE.1 STONE.2 %d\n" % (i, i) for i in range(8)))[1]
        self.assertEqual(len(blob), 12 + 8 * 16 + 32 * 4096)

    def test_name_must_match_the_file(self):
        with self.assertRaises(texbank.BankError):
            texbank.build(OK, "other")

    def test_line_numbers(self):
        self.assertIn("line 3", err("bank t\n\ntex a nope\n"))


class Ps1Sees(unittest.TestCase):
    def test_compiles_to_nothing_on_the_ps1_and_the_gba(self):
        src = os.path.join(ROOT, "core", "gen", "tb_sample.c")
        for plat in ("DOOMO_PLATFORM_PS1", "DOOMO_PLATFORM_GBA"):
            with tempfile.TemporaryDirectory() as tmp:
                o = os.path.join(tmp, "x.o")
                subprocess.run(["gcc", "-std=c99", "-Wall", "-Werror", "-D%s=1" % plat, "-I" + os.path.join(ROOT, "core", "include"),
                                "-c", src, "-o", o], check=True)
                out = subprocess.run(["size", "-A", o], capture_output=True, text=True).stdout
                rodata = [int(l.split()[1]) for l in out.splitlines() if l.startswith((".rodata", ".data", ".bss"))]
                self.assertEqual(sum(rodata), 0, plat + ": " + out)


if __name__ == "__main__":
    unittest.main()
