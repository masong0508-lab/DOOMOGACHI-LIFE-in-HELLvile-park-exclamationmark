"""show/tests/art/test_parts.py - the show's part sets (Mayor Giggs v2, the
turkey; show/art/parts/show_parts.c).  Run: python3 -m unittest from show/tools
or python3 show/tests/art/test_parts.py."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
from showtk.art.cast import host  # noqa: E402
from showtk.art.cast.catalog import catalog  # noqa: E402
from showtk.art.cast.lookdata import Bible  # noqa: E402

SIG = {"species": "SPECIES_OWLKIN", "mouth_style": "MOUTH_BEAK", "hat": "HAT_TOP_HAT", "tail": "TAIL_FEATHER_FAN",
       "accessory": "ACC_BOWTIE", "facial_hair": "FHAIR_WIZARD"}


def turkey(lb):
    cat = catalog()
    return all(lb[cat.look_order.index(f)] == cat.value(n) for f, n in SIG.items())


class Parts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.b = Bible()
        cls.mayor = cls.b.char_look_bytes("mayor")

    def test_signature_only_mayor(self):
        self.assertTrue(turkey(self.mayor))
        for cid in self.b.chars:
            lb = self.b.char_look_bytes(cid)
            if lb and cid != "mayor":
                self.assertFalse(turkey(lb), cid)

    def test_randomizer_never_makes_a_turkey(self):
        looks = host.generate(range(20000)) + host.generate(range(20000), hellborn=True)
        self.assertFalse([i for i, lb in enumerate(looks) if turkey(lb)])

    def test_all_expressions_and_visemes_differ(self):
        cat = catalog()
        jobs = [host.job(self.mayor, "bust", expr=e, anim=4) for e in range(11)]
        mouths = [-1, "MOUTH_FLAT", "MOUTH_TEETH", "MOUTH_GRIN", "MOUTH_OPEN", "MOUTH_O", "MOUTH_POUT"]
        jobs += [host.job(self.mayor, "bust", mouth=m if m == -1 else cat.value(m), anim=4) for m in mouths]
        jobs.append(host.job(self.mayor, "bust", anim=93))      # blink
        _, recs = host.render(jobs)
        px = [r[2] for r in recs]
        self.assertEqual(len(set(px[:11])), 11, "11 distinct expressions")
        self.assertEqual(len(set(px[11:18])), 7, "distinct beak visemes")
        self.assertNotEqual(px[0], px[18], "blink")

    def test_wobble_frames(self):
        _, recs = host.render([host.job(self.mayor, "figure", anim=4 + 8 * f) for f in range(4)])
        self.assertGreaterEqual(len({r[2] for r in recs}), 3)

    def test_no_white_box_on_bust(self):
        """dx3: the old cherub's wings filled the bust frame; the turkey's
        edge columns stay mostly transparent (cut-out over the set)"""
        _, recs = host.render([host.job(self.mayor, "bust", anim=4)])
        w, h, px = recs[0]
        for x in (0, 1, w - 2, w - 1):
            opaque = sum(1 for y in range(h) if px[y * w + x])
            self.assertLess(opaque, h // 3, "column %d" % x)

    def test_other_looks_untouched_size(self):
        _, recs = host.render([host.job(self.b.char_look_bytes("nana"), "bust", anim=4)])
        self.assertEqual(recs[0][:2], (48, 56))


if __name__ == "__main__":
    unittest.main()
