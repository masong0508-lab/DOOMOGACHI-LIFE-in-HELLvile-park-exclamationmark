"""WP13 review tools: AT13.1 (timing reports every event's picture change on its frame),
the sheet, frame overlay, diff, status and notes, on the synthetic co01..co03."""
import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from showtk import ffmpeg as ff, sidecars  # noqa: E402
from showtk.review import frame, notes, sheet, status, synthetic, timing  # noqa: E402

pytestmark = pytest.mark.skipif(not ff.have_ffmpeg(), reason="ffmpeg not installed")


@pytest.fixture(scope="module")
def syn(tmp_path_factory):
    root = tmp_path_factory.mktemp("wp13")
    out = synthetic.make(root)
    old = {k: os.environ.get(k) for k in ("SHOW_BUILD", "SHOW_EPISODES")}
    os.environ["SHOW_BUILD"] = str(root / "build")
    os.environ["SHOW_EPISODES"] = str(root / "eps")
    yield out
    for k, v in old.items():
        if v is None:
            os.environ.pop(k, None)
        else:
            os.environ[k] = v


def test_at13_1_timing_every_event_on_its_frame(syn):
    tl = sidecars.load_timeline("ep101")
    txt, res = timing.report(tl, "co01..co03", masters_dir=syn["masters"],
                             mix=syn["build"] / "audio" / "mix_web.wav")
    measured = [r for r in res if r["status"] != "info"]
    assert len(measured) == 13, txt
    assert all(r["status"] == "ok" for r in measured), txt
    glyph = [r for r in res if r.get("glyphs")][0]
    assert glyph["glyphs"] == (52, 52)


def test_timing_catches_a_late_event(syn):
    tl = sidecars.load_timeline("ep101")
    tl.shot("co03").events[-1].f += 2          # scripted 2 f later than drawn
    tl.shot("co03").events[-1].t += 2
    _, res = timing.report(tl, "co03", masters_dir=syn["masters"])
    sup = [r for r in res if r["op"] == "super"][0]
    assert sup["status"] == "OFF" and sup["measured"] == 283


def test_sheet_frame_diff_status_notes(syn, tmp_path):
    tl = sidecars.load_timeline("ep101")
    p, tiles = sheet.sheet(tl, "co01..co03", syn["masters"])
    assert p.exists() and len(tiles) >= 15
    p, st = frame.frame(tl, "co02", 6, syn["masters"])
    assert p.exists() and st["frame"] == 96
    d, changed = sheet.diff(tl, "co02", syn["masters"], syn["masters"])
    assert d.exists() and changed == []
    txt, s = status.status(tl, syn["masters"])
    assert s["rendered"] == 3 and "final" in txt
    ident, _ = notes.add("ep101", "too slow", shot="co02")
    notes.done("ep101", ident, "fixed")
    assert "[x] %s co02: too slow" % ident in notes.listing("ep101")
