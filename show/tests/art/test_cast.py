"""show/tests/art/test_cast.py - the cast as data (ART track).

    python3 -m unittest discover -s show/tests/art -p 'test_cast*.py'   (or pytest)

Checks every look and char in show/series/cast.shw + looks/*.shw, the crowd
K-sets, the kid-look rule (hard limit 3), Grumbleguts' absence from 101, the
acting library, and AT6.1-style model sheets for the Mayor and Nana drawn by
the game's portrait engine (native host, mouth-override hunk (d)).
"""
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "show", "tools"))

from showtk.art.cast import acting, host, lookdata, sheet  # noqa: E402
from showtk.art.cast.catalog import catalog  # noqa: E402

BIBLE = lookdata.Bible()
CAT = catalog()
EP = "ep101"
# every character the pilot's breakdown names on screen or on the mix
REQUIRED = ["sunny", "pet", "mayor", "nana", "pip", "fuzz", "bonejangles", "clinker", "bloom",
            "steamy", "kevin", "hattie", "lulla", "gurgle", "mascotty", "grampy", "rivet",
            "young_giggs", "young_imp", "fluffy", "puff_demon", "big_puff", "hellvile_hand",
            "cloud_lady", "crowned_shadow", "grumbleguts"]
SPEAKERS = ["mayor", "nana", "pip", "fuzz", "bonejangles", "bloom", "steamy", "kevin", "hattie", "lulla"]
MACROS = ["freeze", "pop", "double_take", "deadpan", "glare", "sob", "laugh", "flinch", "speech_pose"]


class TestLooks(unittest.TestCase):
    def test_no_errors(self):
        self.assertEqual(BIBLE.errors, [])

    def test_every_character_is_cast(self):
        for cid in REQUIRED:
            self.assertIn(cid, BIBLE.chars, cid)

    def test_every_char_and_kset_declares_age(self):
        for cid, c in BIBLE.chars.items():
            self.assertIn(c.opts.get("age", (None,))[0], lookdata.AGES, cid)
        for kid, ks in BIBLE.ksets.items():
            self.assertIn(ks.age, lookdata.AGES, kid)

    def test_kids_are_flagged(self):
        kids = {cid for cid, c in BIBLE.chars.items() if c.opts["age"][0] == "child"}
        self.assertEqual(kids, {"pip", "young_giggs", "young_imp"})
        for cid in kids:
            self.assertIn("KID", BIBLE.chars[cid].opts["tags"][0].split(","))
            self.assertEqual(lookdata.kid_problems(BIBLE.chars[cid].look.values), [], cid)

    def test_looks_pack_to_48_bytes_in_range(self):
        order = CAT.look_order
        self.assertEqual(len(order), 48)
        for cid, c in BIBLE.chars.items():
            if c.look is None:
                continue
            b = c.look.pack()
            self.assertEqual(len(b), 48, cid)
            v = dict(zip(order, b))
            self.assertLess(v["species"], CAT.count("SPECIES_"), cid)
            self.assertLess(v["eye_style"], CAT.count("EYE_"), cid)
            self.assertLess(v["mouth_style"], CAT.count("MOUTH_"), cid)
            self.assertIn(v["eye_count"], (1, 2, 3), cid)

    def test_species_fixes(self):
        """the docs' species fixes: Mayor a cherub, Nana a sheepkin, Pip and Sunny bunnykin"""
        sp = lambda cid: CAT.name_of("SPECIES_", BIBLE.chars[cid].look.values["species"])  # noqa: E731
        self.assertEqual(sp("mayor"), "SPECIES_OWLKIN")   # v2: the turkey (creator, Oct 25)
        self.assertEqual(sp("nana"), "SPECIES_SHEEPKIN")
        self.assertEqual(sp("pip"), "SPECIES_BUNNYKIN")
        self.assertEqual(sp("sunny"), "SPECIES_BUNNYKIN")
        self.assertEqual(BIBLE.chars["kevin"].look.values["eye_count"], 3)

    def test_mayor_keeps_his_bow_tie(self):
        self.assertEqual(BIBLE.chars["mayor"].look.values["accessory"], CAT.value("ACC_BOWTIE"))

    def test_grumbleguts_not_seen_in_101(self):
        g = BIBLE.chars["grumbleguts"]
        self.assertEqual(g.opts["first"][0], "ep102")
        self.assertFalse(sheet.seen(g, EP))
        for kid, ks in BIBLE.ksets.items():
            self.assertNotIn(("roster", 729), ks.members, kid)
        for cid, c in BIBLE.chars.items():
            if cid != "grumbleguts":
                self.assertNotEqual(c.opts.get("roster", ("",))[0], "729", cid)

    def test_voice_only_never_seen(self):
        for cid in ("clinker", "gurgle"):
            self.assertFalse(sheet.seen(BIBLE.chars[cid], EP), cid)

    def test_kid_rule_rejects_adult_coding(self):
        v = dict(BIBLE.chars["pip"].look.values)
        v["mouth"] = CAT.value("MOUTH_LIPSTICK")
        v["shoes"] = CAT.value("SHOES_HEELS")
        self.assertEqual(len(lookdata.kid_problems(v)), 2)


class TestKsets(unittest.TestCase):
    def test_k_and_ages(self):
        named = {int(c.opts["roster"][0]) for c in BIBLE.chars.values() if "roster" in c.opts}
        for kid, ks in BIBLE.ksets.items():
            self.assertTrue(1 <= len(ks.members) <= lookdata.KSET_MAX, kid)
            for m in ks.members:
                if m[0] == "roster":
                    self.assertEqual(lookdata.roster_age(m[1]), ks.age, (kid, m))
                    self.assertNotIn(m[1], named, (kid, m))
                    if ks.age == "child":
                        self.assertEqual(lookdata.kid_problems(lookdata.roster()[m[1]][1]), [], (kid, m))

    def test_kset_looks_render(self):
        looks = BIBLE.kset_looks("lulla_kids")
        self.assertEqual(len(looks), 11)
        self.assertTrue(all(len(lb) == 48 for _n, lb in looks))

    def test_generated_members_use_the_game_generator(self):
        a = host.generate([301, 302])
        b = host.generate([301, 302])
        self.assertEqual(a, b)
        self.assertNotEqual(a[0], a[1])


class TestEngineRenders(unittest.TestCase):
    def test_every_seen_look_draws(self):
        cids = [cid for cid in BIBLE.order if sheet.seen(BIBLE.chars[cid], EP)]
        jobs = []
        for cid in cids:
            lb = BIBLE.chars[cid].look.pack()
            jobs += [host.job(lb, "bust"), host.job(lb, "figure"), host.job(lb, "chibi")]
        _pal, imgs = host.render(jobs)
        for i, (w, h, px) in enumerate(imgs):
            opaque = sum(1 for p in px if p)
            self.assertGreater(opaque, w * h // 8, "%s kind %d draws almost nothing" % (cids[i // 3], i % 3))

    def test_mouth_override_reaches_the_image(self):
        """hunk (d): each viseme is its own image for every speaker"""
        for cid in SPEAKERS:
            lk = BIBLE.chars[cid].look
            vis = sheet.visemes(lk.values["species"])
            jobs = [host.job(lk.pack(), "bust", mouth=sheet.mouth_value(vis[v]), anim=4) for v in sheet.VISEMES]
            _pal, imgs = host.render(jobs)
            distinct = len({px for (_w, _h, px) in imgs})
            self.assertGreaterEqual(distinct, 6, "%s: only %d distinct viseme images" % (cid, distinct))


class TestSheets(unittest.TestCase):
    def test_at61_style_sheets_for_mayor_and_nana(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as td:
            for cid in ("mayor", "nana"):
                p = sheet.char_sheet(BIBLE, cid, td, 1)
                with Image.open(p) as im:
                    self.assertGreater(im.width, 11 * 96)
                    self.assertGreater(im.height, 3 * 112)
            p = sheet.lineup(BIBLE, td, 1)
            self.assertTrue(os.path.exists(p))

    def test_sheet_jobs_cover_11_expressions_and_8_visemes(self):
        lb = BIBLE.chars["mayor"].look.pack()
        jobs, labels, _vis = sheet.char_jobs(lb, BIBLE.chars["mayor"].look.values["species"])
        for kind in ("fig", "bust"):
            self.assertEqual(len([lab for lab in labels if lab[0] == kind and lab[1].startswith("EXPR_")]), 11)
        for kind in ("figv", "bustv"):
            self.assertEqual(len([lab for lab in labels if lab[0] == kind]), 8)
        self.assertEqual(CAT.count("EXPR_"), 11)


class TestActing(unittest.TestCase):
    LIB = acting.Library()

    def test_no_errors(self):
        self.assertEqual(self.LIB.errors, [])

    def test_required_macros(self):
        for m in MACROS:
            self.assertIn(m, self.LIB.macros, m)

    def test_freeze_13(self):
        self.assertEqual(self.LIB.expand("freeze", "mayor"), [(0, "freeze mayor"), (13, "thaw mayor")])
        self.assertEqual(self.LIB.expand("freeze", "mayor", n=21)[-1], (21, "thaw mayor"))

    def test_rungs_and_symbols(self):
        ev = dict((op, f) for f, op in self.LIB.expand("look_at", "nana"))
        self.assertEqual(ev["thaw nana"], 8)
        for name in self.LIB.macros:
            for _f, op in self.LIB.expand(name, "a"):
                for tok in op.split():
                    if tok[:4] in ("EXPR", "TWT_", "SHK_"):
                        self.assertIsNotNone(CAT.value(tok), (name, tok))

    def test_no_sound_in_macros(self):
        for name in self.LIB.macros:
            for _f, op in self.LIB.expand(name, "a"):
                self.assertNotIn(op.split()[0], acting.SOUND, (name, op))


if __name__ == "__main__":
    unittest.main()
