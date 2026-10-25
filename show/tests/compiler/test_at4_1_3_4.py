"""AT4.1 (spec 6.4/6.6 exact), AT4.3 (file:line errors), AT4.4 (one-shot recompile < 1 s), slug 39,600 f."""
import os
import re
import time

import fixture  # noqa: F401  (sets sys.path)
from showtk import showc, textpack_bridge


def _compile(d, shots="co01..co03"):
    ec = showc.EpisodeCompiler("ep101", root=d, shots=shots)
    return ec, ec.run()


def test_at4_1_worked_example():
    with fixture.Tree() as d:
        ec, r = _compile(d)
        assert not r.errors, [x.format() for x in r.errors]
        sh = {s["id"]: s for s in r.timeline["shots"] if s.get("compiled")}
        assert [(sh[i]["start"], sh[i]["len"], sh[i]["tc_in"]) for i in ("co01", "co02", "co03")] == [
            (0, 90, "00:00:00:00"), (90, 180, "00:00:03:00"), (270, 120, "00:00:09:00")]
        assert (sh["co01"]["set"], sh["co01"]["px"]) == ("calendar_page", 2)
        assert (sh["co02"]["set"], sh["co02"]["px"]) == ("park_storybook", 1)
        assert (sh["co03"]["set"], sh["co03"]["px"]) == ("bridge_storybook", 2)
        want = [
            {"k": 0, "smp": 0, "src": "FOL_crayon_scratch", "follow": "co01:circle_midsummer", "bus": "FX", "gain_db": -14},
            {"k": 1, "smp": 48000, "src": "FOL_crayon_scratch", "follow": "co01:fireworks_scribble", "bus": "FX", "gain_db": -17},
            {"k": 2, "smp": 96000, "src": "cue:mingle_box", "from": "bar:1.1", "bus": "MX", "gain_db": -12},
            {"k": 3, "smp": 144000, "src": "FX_SHWIP", "bus": "FX", "gain_db": -10, "pan": 0.0},
            {"k": 4, "smp": 144000, "src": "walla:kids_far", "op": "on", "bus": "DX", "gain_db": -28, "cc": "[soft crowd babble]"},
            {"k": 5, "smp": 432000, "src": "FOL_paw_patter", "from": "crowd:co03.bunnies,co03.imps", "bus": "FX", "gain_db": -18},
            {"k": 6, "smp": 432000, "src": "walla:kids_far", "op": "to", "gain_db": -20, "over_smp": 48000}]
        assert r.audio["events"] == want
        co01, co02, co03 = ec.compilers
        # co02 iris: over=13 -> iris_in 1 5 (hold 5, circle, radius 0), the v1.1 encoding
        S = textpack_bridge.symbols()
        iris = [e for e in co02.ev_direct if e[1] == S["BOP_IRIS_IN"]]
        assert [(e[0], e[2], e[3]) for e in iris] == [(0, 1, 5)]
        # SUPER glyph 1 at f 120 (+30), glyph 52 at f 222 (+132)
        steps = [o for o in co02.show_ops if o[1] == "SOP_TEXT_STEP"]
        assert len(steps) == 52 and steps[0][0] == 30 and steps[-1][0] == 132
        # camera x = 0 at f 180 (shot +90), exact in Q16
        k = co02.camkeys[0]
        assert k["x_q16"] + 90 * k["vx_q16"] == 0
        assert any("COMP-LOCK" in x.format() and " ok " in x.format() for x in r.diags)
        # co03 SUPER 283-333 (+13..+63)
        sup = [o for o in co03.show_ops if o[1] == "SOP_SUPER"][0]
        assert sup[0] == 13 and sup[2][6] == 63
        # co01 reveals 0-24 and 30-60
        rev = [(o[0], o[2][1], o[2][2]) for o in co01.show_ops if o[1] == "SOP_REVEAL"]
        assert rev == [(0, 0, 24), (30, 30, 60)]


def _errs(text):
    with fixture.Tree(extra={"ep101/shots/co.shw": text}) as d:
        _, r = _compile(d)
        return [x.format() for x in r.errors]


def _co_with(old, new):
    src = open(os.path.join(fixture.SHOW, "ep101", "shots", "co.shw")).read()
    assert old in src
    return src.replace(old, new, 1)


FL = re.compile(r"co\.shw:\d+: ")


def test_at4_3_unknown_symbol():
    errs = _errs(_co_with("FX_SHWIP", "FX_NO_SUCH_SOUND"))
    assert any(FL.search(e) and "FX_NO_SUCH_SOUND" in e for e in errs), errs


def test_at4_3_bad_anchor():
    errs = _errs(_co_with("at=prop:bandstand", "at=prop:nowhere"))
    assert any(FL.search(e) and "nowhere" in e for e in errs), errs


def test_at4_3_cycle():
    errs = _errs(_co_with("  ad \"A crayon circles", "  @lb+2 label la\n  @la+2 label lb\n  ad \"A crayon circles"))
    assert any(FL.search(e) and "cycle" in e.lower() for e in errs), errs


def test_at4_4_one_shot_under_1s():
    with fixture.Tree() as d:
        _compile(d, shots="co02")
        t = time.time()
        _, r = _compile(d, shots="co02")
        dt = time.time() - t
        assert not r.errors and dt < 1.0, dt


def test_slug_39600():
    r = showc.compile_episode("ep101", slug=True)
    assert not r.errors and r.timeline["total_frames"] == 39600 and len(r.timeline["beats"]) == 60
