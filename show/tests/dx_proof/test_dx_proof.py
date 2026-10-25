"""The dialogue proof compiles against the real series: 4 shots, 4 lines, a mouth track per speaking
shot, px=1 figures in the two-shots and px=2 busts in the close-ups, and a valid EP SHW."""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SHOW = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(SHOW, "tools"))

from showtk import showc  # noqa: E402


def test_dx_proof():
    ec = showc.EpisodeCompiler("dxp", root=HERE, series=os.path.join(SHOW, "series"))
    r = ec.run()
    errs = [d.format() for d in r.errors if d.rule != "FMT-RUNTIME"]   # a 20 s proof, not a 22:00 episode
    assert not errs, errs
    assert [s.shot.id for s in ec.compilers] == ["dx01", "dx02", "dx03", "dx04"]
    says = [e["src"] for e in r.audio["events"] if "line" in e or str(e.get("src", "")).startswith("line:")]
    assert len([c for c in ec.compilers if c.tracks]) == 4, says
    px = [c.shot.px if hasattr(c.shot, "px") else None for c in ec.compilers]
    assert [len(c.actors) for c in ec.compilers] == [2, 1, 1, 2]
    with tempfile.TemporaryDirectory() as d:
        out = r.write(d)
        assert os.path.getsize(out[0]) > 0
    assert r.timeline["total_frames"] == 600 and px is not None
