#!/usr/bin/env python3
"""Tests of tools/levelcheck.py (owner: levels, LV1).

Needs the reachability tool: MAPREACH=path/to/mapreach (make -C core mapc-check
builds it and sets this). Run: MAPREACH=core/build/mapreach python3 tools/tests/test_levelcheck.py
"""
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(ROOT, "tools", "levelcheck.py")
TESTMAPS = os.path.join(ROOT, "core", "tests", "maps")
MAPREACH = os.environ.get("MAPREACH", os.path.join(ROOT, "core", "build", "mapreach"))

sys.path.insert(0, os.path.join(ROOT, "tools", "tests"))
import test_mapc  # noqa: E402  (its base map text)


def check(*files):
    p = subprocess.run([sys.executable, TOOL, "--mapreach", MAPREACH] + list(files),
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    return p.returncode, p.stdout, p.stderr


def check_text(text, name="lc_test"):
    with tempfile.TemporaryDirectory() as d:
        f = os.path.join(d, name + ".txt")
        with open(f, "w") as fh:
            fh.write(text)
        return check(f)


class TestLevelcheck(unittest.TestCase):
    def test_every_shipped_map_passes(self):
        rc, out, err = check()
        self.assertEqual(rc, 0, out + err)
        self.assertIn("levelcheck: 12 maps, 0 failed", out)

    def test_every_edge_test_map_passes_but_the_soft_lock(self):
        files = [os.path.join(TESTMAPS, f) for f in sorted(os.listdir(TESTMAPS)) if f.endswith(".txt")]
        for f in files:
            rc, out, err = check(f)
            if f.endswith("t2_swap_stuck.txt"):
                self.assertEqual(rc, 1, out)
                self.assertIn("soft lock", out)
            else:
                self.assertEqual(rc, 0, out + err)

    def test_an_unreachable_exit_fails(self):
        # a full column of walls at x=5 cuts the start's room from the exit's
        grid = "grid\n############\n#p...#...x.#\n#....#.....#\n#....#.....#\n#....#.....#\n############\nend\n"
        rc, out, err = check_text(test_mapc.build(grid=grid))
        self.assertEqual(rc, 1, out)
        self.assertIn("no exit can be reached", out)

    def test_an_unreachable_pickup_fails(self):
        # the exit is reachable; the health pickup sits in a sealed pocket
        grid = "grid\n############\n#p.......x.#\n#..........#\n#.####.....#\n#.#h.#.....#\n############\nend\n"
        rc, out, err = check_text(test_mapc.build(grid=grid, legend="cell h like=. thing=HEALTH_SMALL\n"))
        self.assertEqual(rc, 1, out)
        self.assertIn("1 pickups cannot be reached (first: type", out)
        self.assertNotIn("no exit", out)

    def test_a_soft_lock_fails_with_its_cell(self):
        rc, out, err = check(os.path.join(TESTMAPS, "t2_swap_stuck.txt"))
        self.assertEqual(rc, 1)
        self.assertIn("first at 1,1", out)

    def test_a_mapc_refusal_fails_the_map(self):
        rc, out, err = check_text(test_mapc.build(head="dark yes\n"))
        self.assertEqual(rc, 1, out)
        self.assertIn("needs a content note", out)

    def test_a_good_v2_map_prints_its_line(self):
        rc, out, err = check_text(test_mapc.build(head="fog 8\n"))
        self.assertEqual(rc, 0, out + err)
        self.assertIn("v2 12x6", out)

    def test_the_rom_limit(self):
        sys.path.insert(0, os.path.join(ROOT, "tools"))
        import levelcheck
        m = {"v2": False}
        self.assertLess(levelcheck.rom_estimate(m, [0] * 64 * 64, []), levelcheck.ROM_MAX)
        self.assertGreater(levelcheck.rom_estimate(m, [0] * 40000, []), levelcheck.ROM_MAX)

    def test_a_missing_mapreach_is_a_usage_error(self):
        p = subprocess.run([sys.executable, TOOL, "--mapreach", "/nonexistent"], stderr=subprocess.PIPE,
                           stdout=subprocess.PIPE, universal_newlines=True)
        self.assertEqual(p.returncode, 2)


if __name__ == "__main__":
    unittest.main(verbosity=0)
