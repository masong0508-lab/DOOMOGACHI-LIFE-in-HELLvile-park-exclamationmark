"""show/tests/art/test_motion.py - the ART motion pass: ambient layers and cast flipbooks.

    python3 -m unittest discover -s show/tests/art -p 'test_motion*.py'

* the C twin (show/art/engine/amb_math.h) equals the Python reference value for value;
* every 2D set has >= 2 parallax depth planes and >= 1 ambient layer; every
  amb statement names a known kind and a real plane/prop;
* every set fits the 5-page set pool with its ambient layers;
* every flipbook in series/acting.shw bakes, is on-palette, and every macro checks;
* every 2D set moves: no 12-frame pixel-static window over 60 frames.
"""
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SHOW = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(SHOW, "tools"))

from showtk.art.setkit.setfile import Library, split_list  # noqa: E402
from showtk.art.setkit import compose as C  # noqa: E402
from showtk.art.motion import ambient as A  # noqa: E402
from showtk.art.motion import budget as B  # noqa: E402
from showtk.art.motion.render import SetAnim  # noqa: E402
from showtk.art.cast.acting import Library as Acting  # noqa: E402

LIB = Library().load_dir()
KINDS = {"drift", "wave", "pcycle", "lamps", "sway", "flip", "motes", "crowd"}

DRIVER = r'''
#include <stdio.h>
#include "amb_math.h"
int main(void){
  int t,i; AmbMote m; AmbWalker w;
  for(t=0;t<400;t+=7){
    printf("%d %d %d %d\n", amb_wave_dx(2,110,90,t%240,t), amb_sway_dx(1,96,t,3,11),
           amb_drift(31,t), amb_lamp_state(5,t%17,t,4,14,18,14));
    for(i=0;i<8;i++){ int ty;
      for(ty=0;ty<8;ty++){ if(amb_mote(ty,9,i,t,-160,60,640,80,256,&m)) printf("m %d %d %d %d\n",m.x,m.y,m.size,m.role); else printf("m -\n"); }
      amb_walker(i,t,4,-160,480,146,115,5,6,10,&w); printf("w %d %d %d %d\n",w.x,w.y,w.dir,w.frame);
    }
  }
  return 0;
}
'''
TYPES = ["firefly", "sparkle", "ember", "dust", "petal", "snow", "bubble", "leaf"]
ROLES = {"core": 0, "hi": 1, "dim": 2}


def py_lines():
    out = []
    for t in range(0, 400, 7):
        out.append("%d %d %d %d" % (A.wave_dx(2, 110, 90, t % 240, t), A.sway_dx(1, 96, t, 3, 11),
                                    A.drift_off(31, t), A.lamp_state(5, t % 17, t, (4, 14), 18, 14)))
        for i in range(8):
            for ty in TYPES:
                m = A.mote(ty, 9, i, t, (-160, 60, 640, 80), 256)
                out.append("m -" if m is None else "m %d %d %d %d" % (m[0], m[1], m[2], ROLES[m[3]]))
            x, y, d, f = A.walker(i, t, 4, -160, 480, 146, 115, 5, 6, 10)
            out.append("w %d %d %d %d" % (x, y, d, f))
    return out


class TestTwin(unittest.TestCase):
    def test_c_equals_python(self):
        with tempfile.TemporaryDirectory() as td:
            src = os.path.join(td, "d.c")
            open(src, "w").write(DRIVER)
            exe = os.path.join(td, "d")
            subprocess.check_call(["gcc", "-std=c99", "-O1", "-I", os.path.join(SHOW, "art", "engine"), src, "-o", exe])
            c = subprocess.check_output([exe], text=True).split("\n")[:-1]
        p = py_lines()
        self.assertEqual(len(c), len(p))
        bad = [(k, a, b) for k, (a, b) in enumerate(zip(c, p)) if a != b]
        self.assertEqual(bad[:5], [])


class TestSets(unittest.TestCase):
    def test_depth_and_ambient(self):
        for sid, sd in LIB.sets.items():
            self.assertTrue(sd.ambs, "%s has no ambient layer" % sid)
            for a in sd.ambs:
                self.assertIn(a.get("kind"), KINDS, "%s amb %s" % (sid, a.id))
                pl = a.get("plane")
                if pl:
                    self.assertTrue(any(p.id == pl for p in sd.planes), "%s amb %s plane %s" % (sid, a.id, pl))
                for pid in split_list(a.get("props")):
                    self.assertTrue(any(p.id == pid for p in sd.props), "%s amb %s prop %s" % (sid, a.id, pid))
            if sd.kind != "2d":
                continue
            pars = {C.par_of(p)[0] for p in sd.planes if p.get("order") != "top"}
            pars |= {float(a.get("par")) for a in sd.ambs if a.get("par")}
            self.assertGreaterEqual(len(pars), 2, "%s: one depth plane only" % sid)

    def test_budget(self):
        for sid, kind, base, tex, prims, chibi, n in B.table(LIB):
            self.assertLessEqual((base + tex) / B.PAGE, B.POOL, sid)

    def test_alive(self):
        for sid, sd in sorted(LIB.sets.items()):
            if sd.kind != "2d":
                continue
            v = C.variants_of(sd)[0]
            sa = SetAnim(LIB, sd, v, crowd_cache={})
            prev, run, worst = None, 0, 0
            for t in range(60):
                f = sa.frame(t)[30:210]
                if prev is not None:
                    run = run + 1 if (f == prev).all() else 0
                    worst = max(worst, run)
                prev = f
            self.assertLess(worst, 12, "%s:%s holds still for %d frames" % (sid, v.id, worst))


class TestFlips(unittest.TestCase):
    def test_all_bake(self):
        lib = Acting()
        self.assertEqual(lib.errors, [])
        self.assertGreaterEqual(len(lib.flips), 200)
        for name, fl in lib.flips.items():
            fb = C.bake(fl["src"])
            frames, holds = fb.meta.get("frames"), fb.meta.get("holds")
            self.assertTrue(frames and len(frames) == len(holds), name)
            self.assertTrue(all(h > 0 for h in holds), name)
            self.assertEqual(len({(f.w, f.h) for f in frames}), 1, name)
            self.assertTrue(any(f.a.any() for f in frames), name)

    def test_macros_use_cycles(self):
        lib = Acting()
        for m in ("enter_walk", "exit_run", "take_big", "react_small", "laugh_full", "hit_react", "walk_to"):
            self.assertIn(m, lib.macros)
            ops = " ".join(o for _f, o in lib.expand(m, "mayor"))
            self.assertIn("pose mayor", ops)
            kind = ops.split("pose mayor ")[1].split()[0]
            self.assertIsNotNone(lib.resolve_pose("mayor", kind), (m, kind))


if __name__ == "__main__":
    unittest.main()
