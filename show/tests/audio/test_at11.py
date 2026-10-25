"""WP11 mix and master (SHOW_ENGINE_SPEC.md sec.10.6, sec.16 AT11.1-AT11.3).

AT11.1  the `web` master of co01-co03 is -16 LUFS +/-0.5 and <= -1 dBTP;
AT11.2  P4: every event k fired on its scheduled frame (a real render's plog.tsv when
        show/build/ep101/render has one for these shots; the checker itself on fixtures);
AT11.3  re-mixing gives the same PCM hash.
Plus: ducking under dialogue, AUD-LIC, the audio captions in captions.json.
"""
import json
import os
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest
import soundfile as sf

from showtk import mix as M
from showtk import master

SHOW = Path(__file__).resolve().parents[2]
TOOLS = SHOW / "tools"
SPF = 1600


@pytest.fixture(scope="module")
def co_build(tmp_path_factory):
    d = tmp_path_factory.mktemp("co")
    r = subprocess.run([sys.executable, "-m", "showtk", "compile", "ep101", "--shots", "co01..co03",
                        "--out", str(d)], cwd=TOOLS, capture_output=True, text=True)
    if not (d / "audio_events.json").exists():
        pytest.fail("compile failed: " + r.stdout[-2000:] + r.stderr[-2000:])
    return d


@pytest.fixture(scope="module")
def co_mix(co_build):
    return M.mix("ep101", build=co_build, upto="co03", verbose=False)


def test_at11_1_web_master(co_mix, co_build):
    m = co_mix["master"]
    assert m["ok"], m
    y, sr = sf.read(str(co_build / "audio" / "mix_web.wav"))
    assert sr == 48000 and y.shape == (390 * SPF, 2)
    assert abs(master.lufs(y) - (-16.0)) <= 0.5
    assert master.true_peak(y) <= -1.0 if hasattr(master, "true_peak") else master.KM.true_peak(y) <= -1.0
    assert co_mix["qa"]["clipped"] == 0 and co_mix["qa"]["clicks"] == 0


def test_at11_1_events_on_their_samples(co_mix, co_build):
    """MINGLE's first onset at sample 96,000 (AT10.1 survives the mix); SHWIP and the walla
    bed land on frame 90; every event in range was placed."""
    assert co_mix["events_in_mix"] == co_mix["events_total"] == 7
    mx, _ = sf.read(str(co_build / "audio" / "stem_MX.flac"))
    assert abs(int(np.argmax(np.abs(mx).max(1) > 1e-3)) - 96000) <= 48
    fx, _ = sf.read(str(co_build / "audio" / "stem_FX.flac"))
    a = np.abs(fx).max(1)
    assert a[143000:144000].max() < 1e-4 < a[144000:144100].max()


def test_at11_2_p4_checker():
    evs = [{"k": 0, "smp": 0}, {"k": 1, "smp": 48000}, {"k": 2, "smp": 96000}]
    assert M.p4_check(evs, {0: [0], 30: [1], 60: [2], 61: []})["ok"]
    r = M.p4_check(evs, {0: [0], 31: [1], 60: [], 61: [2, 9]})
    assert not r["ok"] and [x["k"] for x in r["late"]] == [1, 2] and r["extra"][0]["k"] == 9


def test_at11_2_p4_render(co_build):
    plog = SHOW / "build" / "ep101" / "render" / "plog.tsv"
    if not plog.exists():
        pytest.skip("no render of co01-co03 (run `show render build/ep101/EP101.SHW --shots co01..co03`)")
    pl = M.read_plog(plog)
    if max(pl) < 389:
        pytest.skip("the render does not cover co01-co03")
    ev = json.loads((co_build / "audio_events.json").read_text())["events"]
    r = M.p4_check(ev, {f: k for f, k in pl.items() if f < 390}, (0, 390))
    assert r["ok"] and r["checked"] == 7, r


def test_at11_3_remix_same_hash(co_mix, co_build, tmp_path):
    again = M.mix("ep101", build=co_build, upto="co03", out=tmp_path, merge_captions=False, verbose=False)
    assert again["sha256_pcm"] == co_mix["sha256_pcm"]
    y, _ = sf.read(str(tmp_path / "mix_web.wav"))
    assert M.pcm_hash(y) == co_mix["sha256_pcm"]


def test_aud_lic(co_mix):
    assert co_mix["aud_lic"]["ok"], co_mix["aud_lic"]
    lic = M.licences(["kit:TimGM6mb.sf2", "kit:salamander/A0v8.flac", "nowhere:x"])
    assert lic["kit:TimGM6mb.sf2"] is None and lic["nowhere:x"] is None
    assert "Salamander" in lic["kit:salamander/A0v8.flac"][1]
    assert M.licence_files_ok() == []


def test_audio_captions(co_mix, co_build):
    cues = json.loads((co_build / "captions.json").read_text())["cues"]
    sdh = {(c["text"], c["in"]) for c in cues if c["kind"] == "sdh"}
    assert ("[music]", 60) in sdh
    assert ("[crayon scratching]", 0) in sdh
    assert ("[soft crowd babble]", 90) in sdh
    assert sum(1 for c in cues if c["text"] == "[soft crowd babble]") == 1
    # captions.py (ENGINE) reads them back as SDH cues
    from showtk import captions
    assert all(c.get("kind") == "sdh" for c in cues if c.get("src") == "mix")
    assert captions.clean("[music]") == "[music]"


def test_ducking_under_dialogue(tmp_path):
    """A cue under a dialogue line drops 7 dB while the line speaks and comes back after."""
    lock = SHOW / "tests" / "audio" / "dx_proof" / "voice.lock"
    row = json.loads(lock.read_text())["lines"]["co15_perfect"]
    from showtk.voice import env as venv
    if not os.path.exists(venv.unrel(row["take"])):
        pytest.skip("dx_proof takes not in the local store")
    n_f = 300
    ev = {"format": "show-audio/1", "events": [
        {"k": 0, "smp": 0, "src": "cue:mingle_full", "from": "bar:1.1", "bus": "MX", "gain_db": -12},
        {"k": 1, "smp": 120 * SPF, "src": "line:co15_perfect", "speaker": "mayor", "bus": "DX",
         "len_smp": row["len"]}]}
    tl = {"episode": "dxt", "acts": [], "shots": [{"id": "t1", "start": 0, "end": n_f, "len": n_f, "cast": []}]}
    (tmp_path / "audio_events.json").write_text(json.dumps(ev))
    (tmp_path / "timeline.json").write_text(json.dumps(tl))
    r1 = M.mix("dxt", build=tmp_path, locks=[lock], out=tmp_path / "a", merge_captions=False, verbose=False)
    mx1, _ = sf.read(str(tmp_path / "a" / "stem_MX.flac"))
    ev["events"] = ev["events"][:1]
    (tmp_path / "audio_events.json").write_text(json.dumps(ev))
    r0 = M.mix("dxt", build=tmp_path, locks=[lock], out=tmp_path / "b", merge_captions=False, verbose=False)
    mx0, _ = sf.read(str(tmp_path / "b" / "stem_MX.flac"))
    k = 10 ** ((r0["master"]["gain_db"] - r1["master"]["gain_db"]) / 20)

    def lvl(x, a, b):
        return 20 * np.log10(np.sqrt(np.mean(x[a:b] ** 2)) + 1e-12)
    on = 120 * SPF + int(row["onset"])
    mid = slice(on + 4800, on + int(row["dur"]) - 4800)
    assert abs(lvl(mx1 * k, mid.start, mid.stop) - lvl(mx0, mid.start, mid.stop) + 7.0) < 0.5
    assert abs(lvl(mx1 * k, 0, 100 * SPF) - lvl(mx0, 0, 100 * SPF)) < 0.05
    assert abs(lvl(mx1 * k, 280 * SPF, 300 * SPF) - lvl(mx0, 280 * SPF, 300 * SPF)) < 0.1
