"""WP10.5: the whole-episode spotting (ep101/cues.shw) resolves on the rundown.

Every op names a series/music.shw cue, lands inside 22:00, the main title's
CRACK is frame 3,000 (01:40), every act card rings the HEE_HEE bell on its
frame 0, every eyecatch pair carries the theme's OUT cut and the IN sting, and
the insert song's 3-04 mute re-enters on bar 51.1 = 3-05 frame 0.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))

from showtk.music import cuelib, spotting  # noqa: E402

TOTAL = 22 * 60 * 30


def _resolved():
    defs = cuelib.load_defs()
    spots, ops = spotting.load("ep101")
    metas = {c: cuelib.load_meta(c) for c in {o.cue for o in ops}}
    rd = spotting.rundown("ep101")
    return defs, rd, spotting.resolve(ops, spots, rd, metas), metas


def test_cues_resolve_inside_programme():
    defs, rd, res, _ = _resolved()
    assert rd[-1][1] + rd[-1][2] == TOTAL
    for o in res:
        assert o.cue in defs, o.where
        assert 0 <= o.frame < TOTAL, o.where
        assert o.smp == o.frame * 1600 or ":bar:" in o.time


def test_landmarks():
    _, rd, res, _ = _resolved()
    beat = {b: f for b, f, _ in rd}
    starts = {(o.cue, o.frame) for o in res if o.op == "start"}
    assert ("theme_main_title", 3000) in starts
    for b in ("e101_1_01", "e101_2_01", "e101_3_01"):
        assert ("heehee_bell", beat[b]) in starts
    assert ("heehee_bell_flat", beat["e101_4_01"]) in starts
    for n in (1, 2, 3):
        assert ("theme_eyecatch_out", beat[f"e101_ec{n}_out"]) in starts
        assert ("heehee_bell_150", beat[f"e101_ec{n}_out"]) in starts
        assert ("eyecatch_in", beat[f"e101_ec{n}_in"]) in starts
    assert ("insert_doom", beat["e101_3_03"]) in starts
    mute = [o for o in res if o.cue == "insert_doom" and o.op == "mute"][0]
    assert mute.frame == beat["e101_3_04"]
    assert mute.frame + int(mute.args[0]) == beat["e101_3_05"]
    # no score in 1-13 (canon holds OMINOUS back until the fog)
    a, b = beat["e101_1_13"], beat["e101_1_14"]
    assert not [o for o in res if o.op == "start" and a <= o.frame < b]
