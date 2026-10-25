"""show/tests/art/test_sets.py - every set of the pilot (ART track).

    python3 -m unittest discover -s show/tests/art -p 'test_sets*.py'   (or pytest)

Checks that every set file reads, every generator bakes, every 2D set covers the
16:9 picture at camera 0 and at every named framing (shaken +-8 px), every frame of
every variant is 100 % on the master palette (PIC-PAL, AT5.3 style), the texel
budget fits the set pool, the tilemaps round-trip, the time-of-day LUTs are index
remaps, the 3D sets name real game symbols, and every location of the pilot's
breakdown has a set.
"""
import os
import sys
import unittest

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "show", "tools"))

from showtk.art.setkit import gen  # noqa: E402
from showtk.art.setkit import check  # noqa: E402
from showtk.art.setkit.setfile import Library  # noqa: E402
from showtk.art.setkit.compose import compose, variants_of, MATTE  # noqa: E402

LIB = Library().load_dir()
G = gen()

# every location the pilot's scenes name (docs/show/PILOT_BEATSHEET.md, BREAKDOWN.md),
# mapped to the set that serves it
LOCATIONS = {
    "CO-1 calendar page": "calendar_page",
    "CO-2 storybook park, one park": "park_storybook",
    "CO-2 the bridge (co03)": "bridge_storybook",
    "CO-2 the sky, the cherub, the hand (co04/06/07)": "sky_storybook",
    "CO-2 the soup table (co05)": "soup_table_storybook",
    "CO-2 the river (co08)": "river_storybook",
    "CO-3 the Wall through the bandstand (co09)": "park_storybook",
    "CO-4 the storybook banner (co10/11)": "banner_storybook",
    "CO-5 bandstand at dawn, the banner": "bandstand_banner",
    "CO-5 the fountain faces (co14)": "fountain_faces",
    "CO-5 the crayon insert (co17)": "sky_storybook",
    "CO-6 fountain plaza": "fountain_plaza",
    "CO-6 Nana's bakery window": "bakery_window",
    "CO-6 the egg": "egg_basket",
    "MT-1 HaHaland from above": "hahaland_crane",
    "MT-3 pan to the Wall": "hahaland_wall_pan",
    "MT-3 the split": "wall_split",
    "MT-4 the gate": "sealed_gate",
    "MT-5 the corridor": "title_corridor",
    "1-01 Candy Hills Road": "candy_hills_road",
    "1-02 town sign": "town_sign",
    "1-03 Giggling Fountain, park gate": "fountain_plaza",
    "1-04 Hook-a-Duck": "hook_a_duck",
    "1-05 face painting": "face_painting",
    "1-06 Hattie's tent": "hattie_tent",
    "1-07 Lulla's craft tent": "lulla_tent",
    "1-08 bandstand (north half)": "bandstand",
    "1-09 bake tent": "bake_tent",
    "1-10 lantern string": "fun_fair",
    "1-11 edge of the crowd": "fun_fair",
    "1-12 bandstand": "bandstand",
    "1-13 the Giggle at 21:00": "giggle_wide",
    "1-13 insert: beyond the Wall": "pipe_yard",
    "1-14 bandstand / the boards": "bandstand",
    "1-14 insert: the kids' tent": "kids_tent_memory",
    "2-03 fun fair": "fun_fair",
    "2-05 bandstand steps": "bandstand",
    "2-08 Whack-an-Imp booth": "whack_an_imp",
    "3-01 fun fair, cute mode": "fun_fair",
    "3-02 bake tent": "bake_tent",
    "3-03 the Absolutely Not Scary House (FP)": "scary_house_fp",
    "3-05 bake tent (FP)": "bake_tent_fp",
    "4-03 the sealed gate": "sealed_gate",
    "4-06 the MINGLE crate": "bandstand",
    "4-07 joke graveyard path": "joke_graveyard",
    "4-08 launch hill": "launch_hill",
    "4-08 the split": "wall_split",
    "T-1 Pink Door Cottage": "cottage",
    "CR-1 end credits stills": "credits_storybook",
    "PC-1 the sealed gate": "sealed_gate",
}


class TestSetFiles(unittest.TestCase):
    def test_every_location_has_a_set(self):
        missing = sorted({loc for loc, sid in LOCATIONS.items() if sid not in LIB.sets})
        self.assertEqual(missing, [])

    def test_check_has_no_errors(self):
        rep = check.check_all(LIB)
        self.assertEqual(rep.errors, [], "\n".join(rep.errors))

    def test_variants_name_real_lights(self):
        for sd in LIB.sets.values():
            for v in sd.variants:
                if sd.kind == "2d":
                    G.light.lut(v.get("light", "day"))


class TestPalette(unittest.TestCase):
    def test_palette_is_the_games(self):
        # palette.c's first entries, decoded the way palette_rgb() does
        self.assertEqual(tuple(G.pal.RGB[2]), (255, 255, 255))
        self.assertEqual(tuple(G.pal.RGB[6]), (255, 189, 214))   # PAL_UI_PINK (BGR555 rounding)
        self.assertEqual(len(G.pal.RGB), 256)

    def test_luts_are_index_remaps(self):
        for name in G.light.NAMES:
            t = G.light.lut(name)
            self.assertEqual(t.dtype, np.uint8)
            self.assertEqual(len(t), 256)
            self.assertEqual(int(t[0]), 0)
            self.assertTrue((t[1:] >= 1).all(), name)

    def test_night_keeps_emissives(self):
        t = G.light.lut("night")
        for i in G.light.EMISSIVE:
            self.assertEqual(int(t[i]), i)

    def test_game_night_is_the_games_table(self):
        self.assertTrue((G.light.lut("game_night") == G.pal.TINTMAP[G.pal.TINT_NIGHT]).all())


class TestPicPal(unittest.TestCase):
    """AT5.3 style: every composed 2D frame is 100 % on-palette."""

    def test_every_variant_on_palette(self):
        lut = G.pal.rgb555_lut()
        total = 0
        for sd in LIB.sets.values():
            if sd.kind != "2d":
                continue
            for v in variants_of(sd):
                idx = compose(LIB, sd, v)[MATTE:MATTE + 180]
                rgb = G.pal.RGB[idx].astype(np.int32)
                key = (rgb[..., 0] >> 3) | ((rgb[..., 1] >> 3) << 5) | ((rgb[..., 2] >> 3) << 10)
                off = int((lut[key] < 0).sum())
                self.assertEqual(off, 0, "%s/%s" % (sd.id, v.id))
                total += idx.size
        self.assertGreater(total, 320 * 180 * 30)


class TestTilemaps(unittest.TestCase):
    def test_tilemaps_round_trip_and_fit(self):
        from show_art_gen.tilemap import from_tilemap
        n = 0
        for sd in LIB.sets.values():
            for st in sd.planes:
                src = st.get("tilemap")
                if not src:
                    continue
                cv = G.bake(src)
                a = from_tilemap(cv.meta["tiles"], cv.meta["map"])
                self.assertTrue((a == cv.a).all(), src)
                self.assertLessEqual(len(cv.meta["tiles"]), 5 * 256, src)
                n += 1
        self.assertGreaterEqual(n, 1)

    def test_the_bandstand_is_a_tilemap(self):
        st = LIB.sets["bandstand"].plane("stage")
        self.assertIn("tilemap", st.args)


class TestGenerators(unittest.TestCase):
    def test_generators_are_pure(self):
        for src in ("gen:bandstand_tiles(seed=124)", "gen:storybook_park(river=1,bridge=1,seed=22)",
                    "gen:banner(size=ms,state=bloom3)"):
            a = G.bake(src).a.copy()
            G._CACHE.pop(src, None)
            b = G.bake(src).a
            self.assertTrue((a == b).all(), src)

    def test_banner_states(self):
        for st in ("mingle", "giggle", "bloom1", "bloom2", "bloom3", "peeled"):
            cv = G.bake("gen:banner(size=ms,state=%s)" % st)
            self.assertGreater(cv.w, 200)
        red = G.pal.paint("CHERRY", 1)
        self.assertTrue((G.bake("gen:banner(size=ms,state=bloom3)").a == red).any())
        self.assertFalse((G.bake("gen:banner(size=ms,state=giggle)").a == red).any())
        # the cold open keeps BLOOD out (D16): the drip is candy red, never the BLOOD ramp
        blood = set(G.pal.ramp_list("BLOOD")) | {G.pal.paint("BLOOD", s) for s in range(3)}
        a = G.bake("gen:paint_drip(size=ms,color=red)").a
        self.assertFalse(any(int(v) in blood for v in np.unique(a)))


if __name__ == "__main__":
    unittest.main()
