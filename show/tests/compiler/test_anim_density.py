"""ANIM-DENSITY (showtk/lint/anim_density.py): the creator's no-slideshow bar.

1. plan mode on stub shots: element counting, static windows vs declared holds,
   per-line gestures, listener reactions, camera motion, overworld establishing-only.
2. frames mode: runs of more than 12 identical frames are found, shorter ones are not.
3. the real episode compiles under the recorder and every shot gets analysed.
"""
import os
import sys

import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "show", "tools"))

from showtk import parse  # noqa: E402
from showtk.lint import anim_density as AD  # noqa: E402


class _Shot:
    def __init__(self, sid, n, setname="x"):
        self.id, self.len, self.kv = sid, n, {"set": setname}

    def where(self):
        return {"file": "show/ep101/shots/t.shw", "line": 1}


class _Lib:
    def line_timing(self, lid, cast):
        return {"len": 40}


class _SetDef:
    def __init__(self, text):
        self.stmt = parse.parse_text(text)[0]


def stub(n, actors=(), ops=(), setdef=None, kind="2d", setup=""):
    sc = type("SC", (), {})()
    sc.shot = _Shot("t01", n)
    sc.actors = [{"id": a, "who": a, "idle": idle} for a, idle in actors]
    sc.crowd_ix = {}
    sc.setdef = setdef
    sc.set_kind = kind
    sc.lib, sc.cast = _Lib(), None
    txt = "shot t01 len=%d\n%s\nend\n" % (n, setup)
    sc.st = parse.parse_text(txt)[0]
    sc._anim_ops = []
    for f, line in ops:
        st = parse.parse_text(line)[0]
        sc._anim_ops.append(AD.Op(f, st))
    return sc


def rules(diags):
    return sorted(d.rule for d in diags)


def test_static_card_warns_density_hold_and_cam():
    st, d = AD.analyse_shot(stub(90))
    r = rules(d)
    assert "ANIM-DENSITY" in r and "ANIM-HOLD" in r and "ANIM-CAM" in r
    assert st["elements"] == 0


def test_live_shot_is_clean():
    sd = _SetDef("set x kind=2d\n  amb a kind=drift plane=p vx=0.1\n  amb b kind=motes type=dust n=4 area=0,0,9,9\nend\n")
    sc = stub(90, actors=[("sunny", 0), ("nana", 0)], setdef=sd,
              ops=[(0, "cam to scale=1.06 over=90"), (0, "say l1 sunny"), (6, "pose sunny pop_small"),
                   (20, "blink nana")])
    st, d = AD.analyse_shot(sc)
    assert rules(d) == [], [x.format() for x in d]
    assert st["elements"] >= 3 and st["camera"]


def test_hidden_amb_needs_variant_show():
    sd = _SetDef("set x kind=2d\n  variant v show=b\n  amb a kind=drift plane=p vx=0.1\n"
                 "  amb b kind=drift plane=q vx=0.1 hidden=yes\nend\n")
    assert AD.visible_ambs(sd, "") == ["a"]
    assert AD.visible_ambs(sd, "v") == ["a", "b"]


def test_gesture_and_listener():
    sc = stub(60, actors=[("sunny", 0), ("nana", 0)], ops=[(0, "say l1 sunny")], setup="cam drift dx=0.1")
    r = rules(AD.analyse_shot(sc)[1])
    assert "ANIM-GESTURE" in r and "ANIM-LISTEN" in r


def test_deadpan_is_a_declared_hold_not_a_missing_gesture():
    sc = stub(60, actors=[("hattie", 0)], ops=[(0, "still hattie"), (0, "say l1 hattie"), (26, "thaw hattie")],
              setup="cam drift dx=0.1")
    assert "ANIM-GESTURE" not in rules(AD.analyse_shot(sc)[1])


def test_static_window_outside_hold():
    # one actor without idle, a single move at +0..+10: +11..+59 is static
    sc = stub(60, actors=[("a", 3)], ops=[(0, "move a to=10,10 over=10")])
    d = [x for x in AD.analyse_shot(sc)[1] if x.rule == "ANIM-HOLD"]
    assert d and "+11..+59" in d[0].msg


def test_overworld_dialogue_warns():
    sc = stub(90, actors=[("a", 0)], ops=[(0, "say l1 a")], kind="overworld")
    assert "ANIM-OVERWORLD" in rules(AD.analyse_shot(sc)[1])


def test_frame_runs():
    np = pytest.importorskip("numpy")
    a = np.zeros((4, 4, 3), np.uint8)
    b = a + 9
    frames = [a] * 20 + [b] * 5 + [a] * 13 + [b]
    runs = AD.frame_runs(iter(frames))
    assert runs == [(0, 20), (25, 13)]


def test_real_episode_is_analysed():
    stats, diags = AD.run("ep101")
    assert len(stats) > 150
    assert all(d.severity == "warning" for d in diags)
