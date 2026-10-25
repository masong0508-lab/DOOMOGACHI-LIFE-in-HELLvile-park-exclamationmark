"""AT10.1: MINGLE's first onset lands at sample 96,000 +-48 on the episode clock.

MINGLE comes from the game's own core/src/mml_mingle.c through tools/mmlc.py
(mml_legacy) and the kit's music box (series/music.shw `mingle_box`); it is
spotted in ep101/cues.shw at co01 +60 (00:00:02:00). The onset is measured on
the rendered MX bus, not computed.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))

from showtk.music import cuelib, qa, spotting  # noqa: E402


def _mx_head(seconds=6.0):
    defs = cuelib.load_defs()
    metas = {}
    for cid in ("mingle_box",):
        try:
            metas[cid] = cuelib.load_meta(cid)
        except FileNotFoundError:
            cuelib.build(defs[cid])
            metas[cid] = cuelib.load_meta(cid)
    spots, ops = spotting.load("ep101")
    ops = [o for o in ops if o.cue in metas]
    rd = spotting.rundown("ep101")
    res = spotting.resolve(ops, spots, rd, metas)
    evs = spotting.events(res, spots, metas)
    return evs, spotting.render_mx(evs, metas, int(seconds * 48000), defs)


def test_mingle_first_onset():
    evs, mx = _mx_head()
    start = [e for e in evs if e["src"] == "cue:mingle_box" and "op" not in e][0]
    assert start["smp"] == 96000, start
    on = qa.first_onset(mx)
    print(f"AT10.1 mingle_box: event smp {start['smp']}, measured first onset smp {on} "
          f"(error {on - 96000:+d} smp = {(on - 96000) / 48:.2f} ms)")
    assert abs(on - 96000) <= 48


def test_mingle_is_the_game_tune():
    """The notes are mml_mingle.c's: head C-F-A-F in F major at t96."""
    from showtk.music import mml_legacy as M
    s = M.song("core/src/mml_mingle.c")
    assert M.song_bpm(s) == 96
    head = [r[2] for r in M.melody(s)[:4]]
    assert head == [72, 77, 81, 77]          # C5 F5 A5 F5


if __name__ == "__main__":
    test_mingle_is_the_game_tune()
    test_mingle_first_onset()
    print("AT10.1 PASS")
