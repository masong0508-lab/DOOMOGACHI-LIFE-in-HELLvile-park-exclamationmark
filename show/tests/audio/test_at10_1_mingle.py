"""AT10.1: MINGLE's first onset lands at sample 96,000 +-48 on the episode clock.

MINGLE (v2, Oct 25) is the creator's pad1.mid phrase (show/lib/music/mingle.py) on
the kit's music box (series/music.shw `mingle_full`); it is
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
    for cid in ("mingle_full",):
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
    start = [e for e in evs if e["src"] == "cue:mingle_full" and "op" not in e][0]
    assert start["smp"] == 96000, start
    on = qa.first_onset(mx)
    print(f"AT10.1 mingle_full: event smp {start['smp']}, measured first onset smp {on} "
          f"(error {on - 96000:+d} smp = {(on - 96000) / 48:.2f} ms)")
    assert abs(on - 96000) <= 48


def test_mingle_is_the_creators_tune():
    """The notes are the creator's MIDI (pad1.mid): C Bb G C ... C Bb Ab Eb at 150 bpm."""
    sys.path.insert(0, os.path.join(HERE, "..", "..", "lib", "music"))
    import mingle as M
    full, cut = M.notes("full"), M.notes("interrupt")
    assert [n for _, _, n in full[:4]] == [60, 58, 55, 60]
    assert [n for _, _, n in full[-4:]] == [60, 58, 56, 51] and full[-1][0] == 21
    assert [(t, n) for t, _, n in cut[-3:]] == [(18, 60), (19.25, 58), (20.75, 56)]
    assert cuelib.load_defs()["mingle_full"].kv["bpm"] == "150"


if __name__ == "__main__":
    test_mingle_is_the_creators_tune()
    test_mingle_first_onset()
    print("AT10.1 PASS")
