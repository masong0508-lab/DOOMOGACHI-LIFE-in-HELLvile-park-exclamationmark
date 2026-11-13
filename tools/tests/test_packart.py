#!/usr/bin/env python3
"""test_packart.py - the PS1 art pack leaves the LV7 art out (no new resident byte).

core/gen/art_data.c carries the old textures / skies first and the LV7 ones last behind
`#ifndef DOOMO_PLATFORM_PS1`. ps1/tools/packart.py must pack the old frames only, and the
file it writes must compile as the PS1 build sees it (TEX_COUNT / SKY_COUNT are the old
counts there, textures.h). Needs a host gcc; no PS1 SDK."""
import os
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import mapc  # noqa: E402

ART = os.path.join(ROOT, "core", "gen", "art_data.c")
PACK = os.path.join(ROOT, "ps1", "tools", "packart.py")


class PackArt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = subprocess.run([sys.executable, PACK, ART, cls.tmp.name], capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_runs(self):
        self.assertEqual(self.out.returncode, 0, self.out.stderr)

    def test_old_frames_only(self):
        with open(os.path.join(self.tmp.name, "packed_art.c")) as f:
            txt = f.read()
        tex = int(re.search(r"ps1_pack_tex_frames = (\d+)", txt).group(1))
        sky = int(re.search(r"ps1_pack_sky_frames = (\d+)", txt).group(1))
        # the old art: 60 texture frames and 5 sky frames (one per old id for the skies)
        self.assertEqual(tex, 60)
        self.assertEqual(sky, mapc.ENUMS["SKY_OLD_COUNT"])
        with open(ART) as f:
            full = f.read()
        self.assertIn("#ifndef DOOMO_PLATFORM_PS1", full)
        # the full file holds more frames than the PS1 packs (the LV7 ones sit behind the guard)
        self.assertGreater(len(re.findall(r"frame \d+ \*/ \{", full)), tex + sky)

    def test_ps1_view_compiles_with_old_counts(self):
        cc = os.environ.get("CC", "gcc")
        src = os.path.join(self.tmp.name, "art_data_ps1.c")
        r = subprocess.run([cc, "-std=c99", "-fsyntax-only", "-Wall", "-Werror", "-DDOOMO_PLATFORM_PS1=1",
                            "-I" + os.path.join(ROOT, "core", "include"), src], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        # the host view of the same file still has them
        r = subprocess.run([cc, "-std=c99", "-fsyntax-only", "-Wall", "-Werror",
                            "-I" + os.path.join(ROOT, "core", "include"), ART], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)


if __name__ == "__main__":
    unittest.main()
