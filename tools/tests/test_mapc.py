#!/usr/bin/env python3
"""Tests of tools/mapc.py, map format v2 (owner: levels, LV1).

A test per v2 statement (what it stores) and per refusal (the sentence mapc
gives), and the rule that a map without v2 statements compiles exactly as
before. Run: python3 tools/tests/test_mapc.py   (make -C core mapc-check)
"""
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MAPC = os.path.join(ROOT, "tools", "mapc.py")

HEAD = """format 2
title "Mapc Test"
kind dungeon
sky PASTEL_DAY
music HAHALAND_DAY
ambient 12
"""

LEGEND = """cell # wall=PASTEL_BRICK
cell . floor=WOOD_FLOOR ceil=CLOUD_PUFF ch=14
cell p like=. thing=PLAYER_START
cell x like=. flags=exit
cell B wall=CANDY_STRIPE
cell D floor=TILE_FLOOR ceil=DOOR_CUTE fh=-6 ch=4 flags=door
cell o like=. light=9
cell s like=. thing=CANDY
"""

# 12 x 6; (1,1) is the start; row 3 col 6 is a wall nothing ever opens
GRID = """grid
############
#p.......x.#
#..........#
#.....#....#
#..........#
############
end
"""


def build(extra="", head="", legend="", grid=GRID):
    return HEAD + head + LEGEND + legend + grid + extra


def run(text, *args):
    """(exit status, stderr, generated C text or None) for a map text."""
    with tempfile.TemporaryDirectory() as d:
        f = os.path.join(d, "mapc_test.txt")
        with open(f, "w") as fh:
            fh.write(text)
        p = subprocess.run([sys.executable, MAPC, "--out-root", d] + list(args) + [f],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        gen = os.path.join(d, "core", "gen", "map_mapc_test.c")
        c = None
        if os.path.exists(gen):
            with open(gen) as fh:
                c = fh.read()
        return p.returncode, p.stderr, c


class V2Test(unittest.TestCase):
    def ok(self, text):
        rc, err, c = run(text)
        self.assertEqual(rc, 0, err)
        return c

    def refuse(self, text, words):
        rc, err, c = run(text)
        self.assertEqual(rc, 1, "mapc accepted a map it should refuse (%s)" % words)
        self.assertIn(words, err)
        self.assertIsNone(c)


class TestV1Unchanged(V2Test):
    def test_a_map_without_v2_statements_has_no_ext(self):
        text = HEAD.replace("format 2\n", "") + LEGEND + GRID
        c = self.ok(text)
        self.assertNotIn("DoomoMapExt", c)
        self.assertNotIn("map_ext_", c)

    def test_a_v2_map_has_an_ext_pointing_at_its_def(self):
        c = self.ok(build(head="fog 8\n"))
        self.assertIn("const DoomoMapExt map_ext_mapc_test", c)
        self.assertIn("&map_def_mapc_test", c)

    def test_the_format_line_alone_makes_a_v2_map(self):
        self.assertIn("map_ext_mapc_test", self.ok(build()))

    def test_format_must_be_2(self):
        self.refuse(HEAD.replace("format 2", "format 3") + LEGEND + GRID, "format 2")

    def test_the_shipped_maps_compile_without_ext(self):
        d = os.path.join(ROOT, "core", "data", "maps")
        for f in sorted(os.listdir(d)):
            if f.endswith(".txt"):
                with open(os.path.join(d, f)) as fh:
                    rc, err, c = run(fh.read())
                # the name is taken from the file text, the test file name only matters for the output path
                self.assertEqual(rc, 0, "%s: %s" % (f, err))


class TestHeaderKeys(V2Test):
    def test_numbers_are_stored(self):
        c = self.ok(build(head="fog 9\nview 14\ngravity 80\nnote 7\ndetail far,fx\nhero 2 5\ngba no\n"))
        self.assertIn("MAPX_HERO", c)
        for want in ("9", "14", "80", "7"):
            self.assertIn(want, c)

    def test_dark_with_note(self):
        c = self.ok(build(head="dark yes\nnote 3\n"))
        self.assertIn("MAPX_DARK", c)

    def test_dark_without_note_refused(self):
        self.refuse(build(head="dark yes\n"), "needs a content note")

    def test_fog_range(self):
        self.refuse(build(head="fog 0\n"), "fog")
        self.refuse(build(head="fog 41\n"), "fog")
        self.refuse(build(head="fog\n"), "fog N")

    def test_view_range(self):
        self.refuse(build(head="view 3\n"), "view")
        self.refuse(build(head="view 41\n"), "view")

    def test_gravity_range(self):
        self.refuse(build(head="gravity 9\n"), "gravity")
        self.refuse(build(head="gravity 251\n"), "gravity")

    def test_note_range(self):
        self.refuse(build(head="note 0\n"), "note")
        self.refuse(build(head="note x\n"), "note")

    def test_detail(self):
        self.ok(build(head="detail far,fx,props\n"))
        self.refuse(build(head="detail far,fx,props,sky\n"), "detail a,b,c")
        self.refuse(build(head="detail bogus\n"), "unknown detail")
        self.refuse(build(head="detail far,far\n"), "twice")

    def test_hero(self):
        self.refuse(build(head="hero 1\n"), "hero LOOK LOADOUT")
        self.refuse(build(head="hero 1 300\n"), "out of range")

    def test_a_key_twice_is_refused(self):
        self.refuse(build(head="fog 8\nfog 9\n"), "given twice")
        self.refuse(build(head="note 1\nnote 2\n"), "given twice")

    def test_yes_no(self):
        self.refuse(build(head="gba maybe\n"), "gba yes|no")


class TestGbaBudgets(V2Test):
    GOOD = "view 14\ngba yes\n"

    def test_a_small_roofed_map_passes(self):
        self.ok(build(head=self.GOOD))

    def test_needs_a_view_distance(self):
        self.refuse(build(head="gba yes\n"), "no 'view N'")

    def test_view_distance_range(self):
        self.refuse(build(head="view 20\ngba yes\n"), "view 20")
        self.refuse(build(head="view 8\ngba yes\n"), "view 8")

    def test_map_size(self):
        row = "#" * 42
        grid = "grid\n" + row + "\n" + "#p" + "." * 38 + "x#\n" + row + "\nend\n"
        self.refuse(build(head=self.GOOD, grid=grid), "the GBA takes at most")

    def test_roofed_share(self):
        sky = "cell . floor=WOOD_FLOOR ceil=sky ch=14\n"
        text = (HEAD + "view 14\ngba yes\n" + LEGEND.replace("cell . floor=WOOD_FLOOR ceil=CLOUD_PUFF ch=14\n", sky) + GRID)
        self.refuse(text, "roofed")

    def test_thing_count(self):
        rows = "".join("thing %d 2 CANDY\n" % x for x in range(1, 11)) * 13
        self.refuse(build(head=self.GOOD, extra=rows), "things, the GBA takes at most")

    def test_a_non_gba_map_is_not_held_to_it(self):
        self.ok(build(head="gba no\n", grid=GRID))


class TestZones(V2Test):
    def test_stored(self):
        c = self.ok(build(extra="zone 1 1 3 3 fog arg=6 tag=30\nzone 5 1 8 3 dark arg=4\n"))
        self.assertIn("/* fog */", c)
        self.assertIn("/* dark */", c)
        self.assertIn("zones_mapc_test[2]", c)

    def test_every_kind_is_known(self):
        for k in ("fog", "tide", "noise", "face_a", "face_b", "ward", "chase", "safe", "web"):
            self.ok(build(extra="zone 1 1 2 2 %s arg=1\n" % k))

    def test_outside_the_map(self):
        self.refuse(build(extra="zone 1 1 12 3 fog\n"), "outside the 12x6 map")
        self.refuse(build(extra="zone 1 1 3 6 fog\n"), "outside the 12x6 map")

    def test_corner_order(self):
        self.refuse(build(extra="zone 5 1 2 3 fog\n"), "top left corner")

    def test_unknown_kind_and_key(self):
        self.refuse(build(extra="zone 1 1 2 2 bogus\n"), "unknown zone kind")
        self.refuse(build(extra="zone 1 1 2 2 fog color=3\n"), "unknown zone key")

    def test_too_short(self):
        self.refuse(build(extra="zone 1 1 2 2\n"), "zone X0 Y0 X1 Y1 KIND")

    def test_same_kind_may_not_overlap(self):
        self.refuse(build(extra="zone 1 1 3 3 fog\nzone 3 3 5 5 fog\n"), "overlaps the fog zone")

    def test_other_kinds_may_overlap(self):
        self.ok(build(extra="zone 1 1 3 3 fog\nzone 3 3 5 5 dark arg=2\n"))

    def test_exclusive_kinds_may_not_overlap(self):
        self.refuse(build(extra="zone 1 1 3 3 face_a\nzone 3 3 5 5 face_b\n"), "may not overlap")
        self.refuse(build(extra="zone 1 1 3 3 safe\nzone 3 3 5 5 chase\n"), "may not overlap")

    def test_gravity_and_bounce_need_args(self):
        self.refuse(build(extra="zone 1 1 3 3 gravity\n"), "gravity needs arg")
        self.refuse(build(extra="zone 1 1 3 3 bounce\n"), "bounce needs arg")
        self.refuse(build(extra="zone 1 1 3 3 bounce arg=128\n"), "bounce needs arg")
        self.ok(build(extra="zone 1 1 3 3 gravity arg=50\nzone 5 1 6 1 bounce arg=10\n"))

    def test_at_most_32(self):
        zones = "".join("zone %d %d %d %d noise arg=1\n" % (x, y, x, y) for y in range(1, 5) for x in range(1, 11))
        self.assertGreater(zones.count("\n"), 32)
        self.refuse(build(extra=zones), "zones, at most 32")
        ok = "".join("zone %d 1 %d 1 noise arg=1\n" % (x, x) for x in range(1, 11)) + \
             "".join("zone %d 2 %d 2 noise arg=1\n" % (x, x) for x in range(1, 11)) + \
             "".join("zone %d 4 %d 4 noise arg=1\n" % (x, x) for x in range(1, 11))
        self.assertEqual(ok.count("\n"), 30)
        self.ok(build(extra=ok + "zone 1 3 1 3 noise arg=1\nzone 2 3 2 3 noise arg=1\n"))


class TestPaths(V2Test):
    def test_stored(self):
        c = self.ok(build(extra="path 7 kind=route 1,1 3,1 3,3\npath 9 kind=tube 1,3 8,3\n"))
        self.assertIn("paths_mapc_test[2]", c)
        self.assertIn("pathpts_mapc_test[10]", c)

    def test_every_kind(self):
        for k in ("route", "zip", "ride", "raft", "tube", "arc", "chase"):
            self.ok(build(extra="path 1 kind=%s 1,1 3,1\n" % k))

    def test_default_kind_is_route(self):
        c = self.ok(build(extra="path 1 1,1 3,1\n"))
        self.assertIn("/* route */", c)

    def test_outside_the_map(self):
        self.refuse(build(extra="path 1 1,1 12,1\n"), "outside the 12x6 map")

    def test_point_in_a_wall_that_never_opens(self):
        self.refuse(build(extra="path 1 1,1 6,3\n"), "wall that never opens")

    def test_point_in_a_swapped_wall_is_fine(self):
        self.ok(build(extra="path 1 1,1 6,3\nswap w event=5\n\n\n\n......o\nend\n"))

    def test_count_limits(self):
        self.refuse(build(extra="path 1 1,1\n"), "needs 2..32")
        pts = " ".join("%d,%d" % (1 + i % 10, (1, 2, 4)[i // 10 % 3]) for i in range(33))
        self.refuse(build(extra="path 1 %s\n" % pts), "needs 2..32")
        pts = " ".join("%d,%d" % (1 + i % 10, (1, 2, 4)[i // 10 % 3]) for i in range(32))
        self.ok(build(extra="path 1 %s\n" % pts))

    def test_id_twice(self):
        self.refuse(build(extra="path 1 1,1 2,1\npath 1 3,1 4,1\n"), "id used twice")

    def test_bad_kind_key_point(self):
        self.refuse(build(extra="path 1 kind=bogus 1,1 2,1\n"), "unknown path kind")
        self.refuse(build(extra="path 1 speed=2 1,1 2,1\n"), "unknown path key")
        self.refuse(build(extra="path 1 1,1 2\n"), "is not x,y")

    def test_at_most_8(self):
        paths = "".join("path %d 1,1 2,1\n" % i for i in range(9))
        self.refuse(build(extra=paths), "paths, at most 8")
        self.ok(build(extra="".join("path %d 1,1 2,1\n" % i for i in range(8))))


class TestSwaps(V2Test):
    SW = "swap w event=5\n\n\n\n......o\nend\n"      # opens the wall at (6,3)

    def test_stored(self):
        c = self.ok(build(extra=self.SW))
        self.assertIn("swapcells_mapc_test[1]", c)
        self.assertIn("MSWAP_ONCE", c)

    def test_modes(self):
        self.assertIn("MSWAP_SCRIPT", self.ok(build(extra="swap w\n\n\n\n......o\nend\n")))
        self.assertIn("MSWAP_TOGGLE", self.ok(build(extra="swap w event=5 toggle\n\n\n\n......o\nend\n")))

    def test_toggle_needs_an_event(self):
        self.refuse(build(extra="swap w toggle\n\n\n\n......o\nend\n"), "toggle needs event")

    def test_bad_name_and_option(self):
        self.refuse(build(extra="swap W1\n.\nend\n"), "swap NAME")
        self.refuse(build(extra="swap w speed=3\n.\nend\n"), "unknown swap option")

    def test_name_twice(self):
        self.refuse(build(extra=self.SW + "swap w event=6\n\n\n\n......o\nend\n"), "name used twice")

    def test_a_block_without_end(self):
        self.refuse(build(extra="swap w event=5\n......o\n"), "has no 'end'")

    def test_an_unknown_legend_character(self):
        self.refuse(build(extra="swap w event=5\n\n\n\n......Q\nend\n"), "no cell legend")

    def test_no_overrides(self):
        self.refuse(build(extra="swap w event=5\n.\n.\nend\n"), "no cell overrides")

    def test_the_override_must_differ(self):
        self.refuse(build(extra="swap w event=5\n\n\n\n......#\nend\n"), "equals the cell it replaces")

    def test_more_rows_than_the_map(self):
        self.refuse(build(extra="swap w event=5\n" + ".\n" * 7 + "end\n"), "rows, the grid only 6")

    def test_row_too_wide(self):
        self.refuse(build(extra="swap w event=5\n.............\nend\n"), "too wide")

    def test_doors_and_lifts_may_not_be_touched(self):
        self.refuse(build(extra="swap w event=5\n\n\n\n......D\nend\n"), "door or a lift")
        self.refuse(build(extra="swap w event=5\n\n\n\n.D\nend\n"), "door or a lift")

    def test_a_cell_in_two_sets(self):
        self.refuse(build(extra=self.SW + "swap v event=6\n\n\n\n......B\nend\n"), "already in swap w")

    def test_things_may_not_be_swapped_in(self):
        self.refuse(build(extra="swap w event=5\n\n\n\n......s\nend\n"), "places a thing")

    def test_walls_in_the_player_start(self):
        self.refuse(build(extra="swap w event=5\n\n.B\nend\n"), "walls in the PLAYER_START")

    def test_opening_the_border(self):
        self.refuse(build(extra="swap w event=5\no\nend\n"), "opens a cell on the map's border")

    def test_a_wall_nothing_opens(self):
        # (0,0) is a corner: a wall whose neighbours are all walls, swapped to another wall
        self.refuse(build(extra="swap w event=5\nB\nend\n"), "never opens")
        # (6,3) is a wall with open neighbours: swapping its texture is fine
        self.ok(build(extra="swap w event=5\n\n\n\n......B\nend\n"))

    def test_at_most_16_sets(self):
        sets = "".join("swap s%d event=5\n\n\n\n%so\nend\n" % (i, "." * (i % 10 + 1)) for i in range(17))
        self.refuse(build(extra=sets), "swap sets, at most 16")

    def test_at_most_256_cells(self):
        rows = "\n".join([" " + "o" * 62] * 5)     # 310 cells in the interior rows 2..6 of the wide map
        self.refuse(build(extra="swap a event=5\n\n\n" + rows + "\nend\n", grid=self._wide()), "at most 256")
        rows = "\n".join([" " + "o" * 62] * 4)     # 248 cells
        self.ok(build(extra="swap a event=5\n\n\n" + rows + "\nend\n", grid=self._wide()))

    @staticmethod
    def _wide():
        return "grid\n" + "#" * 64 + "\n" + "#p" + "." * 60 + "x#\n" + ("#" + "." * 62 + "#\n") * 6 + "#" * 64 + "\nend\n"


class TestTexbank(V2Test):
    def test_stored(self):
        c = self.ok(build(head="texbank 0 lv_brick\ntexbank 3 lv_floor\n"))
        self.assertIn("lv_brick", c)
        self.assertIn("lv_floor", c)
        self.assertIn("texbank_mapc_test[4]", c)

    def test_a_cell_can_use_a_bank_texture(self):
        text = build(head="texbank 0 lv_brick\n", legend="cell Q wall=lv_brick\n", grid=GRID.replace("#.....#", "#.....Q"))
        self.ok(text)

    def test_slot_range(self):
        self.refuse(build(head="texbank 8 lv_a\n"), "texbank slot")

    def test_slot_and_name_twice(self):
        self.refuse(build(head="texbank 1 lv_a\ntexbank 1 lv_b\n"), "slot 1 given twice")
        self.refuse(build(head="texbank 1 lv_a\ntexbank 2 lv_a\n"), "given twice")

    def test_name_rules(self):
        self.refuse(build(head="texbank 1 a-b\n"), "lowercase identifier")
        self.refuse(build(head="texbank 1 1ab\n"), "lowercase identifier")
        self.refuse(build(head="texbank 1 averyveryverylongname\n"), "at most 11")
        self.refuse(build(head="texbank 1 tile_floor\n"), "already a shared texture")

    def test_needs_both_words(self):
        self.refuse(build(head="texbank 1\n"), "texbank N TEXNAME")


class TestThingFlags2(V2Test):
    def test_stored(self):
        c = self.ok(build(extra="thing 2 2 IMPLING tflags2=sleeper,splitter\nthing 3 2 CANDY tflags2=collect\n"))
        self.assertIn("tflags2_mapc_test", c)

    def test_every_flag(self):
        for f in ("escort", "collect", "sleeper", "splitter", "frozen", "usable"):
            self.ok(build(extra="thing 2 2 IMPLING tflags2=%s\n" % f))

    def test_unknown_flag(self):
        self.refuse(build(extra="thing 2 2 IMPLING tflags2=flying\n"), "tflags2")

    def test_the_legend_can_carry_it(self):
        self.ok(build(legend="cell c like=. thing=CANDY tflags2=collect\n"))


class TestScripts(V2Test):
    def test_on_is_not_compiled_yet(self):
        self.refuse(build(extra="on kill 3 do open 4\n"), "script VM")


class TestExport(V2Test):
    def test_export_is_written_and_has_the_tables(self):
        with tempfile.TemporaryDirectory() as d:
            f = os.path.join(d, "mapc_test.txt")
            with open(f, "w") as fh:
                fh.write(build(extra="zone 1 1 3 3 fog arg=6\npath 4 kind=tube 1,1 5,1\n"))
            p = subprocess.run([sys.executable, MAPC, "--check", "--export", d, f], stderr=subprocess.PIPE,
                               universal_newlines=True)
            self.assertEqual(p.returncode, 0, p.stderr)
            with open(os.path.join(d, "mapc_test.mapx")) as fh:
                text = fh.read()
            self.assertIn("zones 1\n1 1 3 3 ", text)
            self.assertIn("paths 1\n4 ", text)
            self.assertFalse(os.path.exists(os.path.join(d, "core")))     # --check wrote no C


if __name__ == "__main__":
    unittest.main(verbosity=0)
