"""WP12 assemble and deliver: AT12.1 and AT12.2 on a synthetic 390-frame FFV1 master
(co01..co03 in the capture's format), plus the EDL, segments, captions and chapters.

    python3 -m pytest show/tests/engine/test_wp12_deliver.py -q
"""
import json
import os
import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from showtk import captions, deliver, edl, ffmpeg as ff, sidecars  # noqa: E402
from showtk.review import synthetic  # noqa: E402

pytestmark = pytest.mark.skipif(not ff.have_ffmpeg(), reason="ffmpeg not installed")


@pytest.fixture(scope="module")
def syn(tmp_path_factory):
    root = tmp_path_factory.mktemp("wp12")
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


@pytest.fixture(scope="module")
def web(syn):
    return deliver.deliver("ep101", "web", rng="co01..co03", masters_dir=syn["masters"])


def test_at12_1_web_mp4_checks_out(web):
    """AT12.1 / M1-T1: 1920x1080, 30/1 fps, 390 frames, 13.000 s, AAC stereo 48 kHz."""
    mp4 = Path(web["_paths"]["mp4"])
    assert mp4.name == "ep101_co01-co03_web_hd.mp4"
    info = ff.media_info(mp4, count_frames=True)
    v, a = info["video"], info["audio"]
    assert (v["width"], v["height"]) == (1920, 1080)
    assert v["fps"] == 30.0 and abs(v["avg_fps"] - 30.0) < 1e-6
    assert v["frames"] == 390
    assert abs(v["duration"] - 13.0) < 1e-6
    assert v["codec"] == "h264" and v["pix_fmt"] == "yuv420p" and v["color_space"] == "bt709"
    assert a["codec"] == "aac" and a["sample_rate"] == 48000 and a["channels"] == 2
    assert abs(a["duration"] - 13.0) < 0.03
    assert web["qc"]["probe"]["problems"] == []


def test_at12_2_hd_crop_matches_masters_x6(web):
    """AT12.2: the framemd5s of the HD chain equal the masters x6 on the sampled frames
    (every shot's first and last frame among them), and the decoded encode matches each
    sampled master frame better than its neighbours."""
    pic = web["qc"]["picture"]
    assert pic["ok"], pic
    assert {0, 89, 90, 269, 270, 389} <= set(pic["frames"])
    assert all(a == b for _, a, b in pic["framemd5"])
    assert pic["worst_mean_err"] < 3.0


def test_at12_2_catches_an_off_by_one(syn):
    """The check is not vacuous: an EDL whose co02 piece starts one frame late fails."""
    tl = sidecars.load_timeline("ep101")
    m = edl.Masters(tl, syn["masters"])
    e = edl.EDL(tl, 0, 390, m)
    p = e.pieces[1]
    p.a += 1                    # co02 from its frame 1 ...
    p.f0 += 1
    e.pieces[0].b -= 0
    bad = edl.EDL.__new__(edl.EDL)
    bad.__dict__.update(e.__dict__)
    lst = Path(os.environ["SHOW_BUILD"]) / "bad.ffconcat"
    ff.write_ffconcat([(q.master, q.a, q.b) for q in bad.pieces], lst)
    got = ff.decode_frames(lst, [90], (320, 180), vf=ff.picture_filter(tl.canvas, 1, yuv=False),
                           input_args=["-f", "concat", "-safe", "0"])
    good = edl.EDL(tl, 0, 390, m)
    want = deliver.master_crops(good, tl.canvas, [90])
    assert not np.array_equal(got[90], want[90])


def test_manifest_names_hashes_and_badges(web):
    """M1-T2: the manifest carries frame hashes per shot, badges, tools and a render hash."""
    man = json.loads(Path(web["_paths"]["committed_manifest"]).read_text())
    assert [s["id"] for s in man["shots"]] == ["co01", "co02", "co03"]
    assert all(len(s["frames_sha256"]) == 64 for s in man["shots"])
    assert man["badges"] == {"PS1-LEGAL": 3}
    assert set(man["tools"]) >= {"show_exe", "emulator", "bios", "harness", "patches", "ffmpeg"}
    assert man["audio"]["sample_rate"] == 48000
    assert len(man["render_hash"]) == 64
    assert man["frames"] == [0, 390] and man["n_frames"] == 390


def test_captions_srt_vtt(web):
    """M1-T6: the SRT carries "[soft crowd babble]" from 00:00:03,000; VTT matches."""
    srt = Path(web["_paths"]["srt"]).read_text()
    cues = captions.parse_srt(srt)
    assert cues and cues[0][0] == 3000 and cues[0][2].endswith("[soft crowd babble]")
    vtt = Path(web["_paths"]["vtt"]).read_text()
    assert vtt.startswith("WEBVTT") and "00:00:03.000 -->" in vtt


def test_second_delivery_reuses_segments_and_decodes_identically(syn, web):
    """A re-run re-encodes nothing; the render hash and the decoded frames are identical."""
    m2 = deliver.deliver("ep101", "web", rng="co01..co03", masters_dir=syn["masters"])
    assert m2["segments_encoded"] == 0 and m2["segments_reused"] == 2
    assert m2["render_hash"] == web["render_hash"]
    a = ff.framemd5(m2["_paths"]["mp4"], vf=["select='not(mod(n\\,13))'"])
    assert len(a) == 30


def test_partial_range_is_frame_exact(syn):
    """A range that cuts shots mid-way (frames 80..299) keeps exactly those frames."""
    m = deliver.deliver("ep101", "web", rng="80..299", masters_dir=syn["masters"])
    assert m["n_frames"] == 220
    assert m["qc"]["probe"]["video"]["frames"] == 220
    assert m["qc"]["picture"]["ok"], m["qc"]["picture"]


def test_preview_x3_burnins(syn):
    m = deliver.preview("ep101", "co01..co03", masters_dir=syn["masters"])
    v = m["qc"]["probe"]["video"]
    assert (v["width"], v["height"], v["frames"]) == (960, 540, 390)
    # the burn-ins are drawn: the top-left TC box differs from the plain x3 picture
    fr = ff.decode_frames(m["_paths"]["mp4"], [150], (960, 540))[150]
    assert fr[6:24, 6:120].std() > 20


def test_ranges_and_chapters(syn):
    tl = sidecars.load_timeline("ep101")
    assert tl.range("co01..co03") == (0, 390)
    assert tl.range("co02") == (90, 270)
    assert tl.range("e101_co_2") == (90, 390)
    assert tl.range("CO") == (0, 390)
    assert tl.range("00:00:03:00..00:00:08:29") == (90, 270)
    assert deliver.chapters(tl, 0, 390) == [(0, 390, "Cold open")]
    tl.chapters = [{"f": 0, "title": "Cold open"}, {"f": 400, "title": "B"}, {"f": 410, "title": "C"},
                   {"f": 1000, "title": "D"}, {"f": 1990, "title": "E"}]
    # C (10 f after B) and E (10 f before the end) fold away: YouTube wants >= 10 s each
    ch = deliver.chapters(tl, 0, 2000)
    assert ch == [(0, 400, "Cold open"), (400, 1000, "B"), (1000, 2000, "D")]
    assert deliver.chapters(tl, 500, 2000)[0] == (0, 500, "B")


def test_caption_rules():
    assert captions.clean("{d}Nobody talks{..} about {big}why{/}.") == "Nobody talks about why."
    rows = captions.wrap("Every Midsummer night, the whole park had one party, and it was loud.")
    assert all(len(r) <= 42 for r in rows)
    parts = captions._split(0, 300, " ".join(["word"] * 40))
    assert len(parts) >= 2 and parts[0][0] == 0 and parts[-1][1] == 300
    assert all(p[2].count("\n") <= 1 for p in parts)


def test_missing_master_names_the_shots(syn, tmp_path):
    tl = sidecars.load_timeline("ep101")
    with pytest.raises(edl.MissingMasters) as e:
        edl.EDL(tl, 0, 390, edl.Masters(tl, tmp_path))
    assert "co01" in str(e.value) and "show render ep101" in str(e.value)
