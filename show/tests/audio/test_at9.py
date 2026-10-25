"""WP9 acceptance tests (spec §16): AT9.1-AT9.4 on the cold open's walla lines and the
dialogue-proof lines. These render with Kokoro and listen with Whisper, so they need the
models (skipped when `show voice` cannot find them) and take a few minutes.

  AT9.1 the walla lines render, lock and reproduce from the store (the vault's local copy)
  AT9.2 the dx_proof bilabial closures show an M within +/-1 f (closures measured from the audio)
  AT9.3 swapping one line TTS -> recording changes only that line's row (take, mouth, times)
  AT9.4 --require human-voice lists every TTS line
"""
import json
import os
import shutil

import numpy as np
import pytest

from showtk.voice import build, env, lock, report, takes

pytestmark = pytest.mark.skipif(not (env.kokoro_models() and env.kokoro_voices() and env.cmudict_path(False)),
                                reason="Kokoro models / cmudict not available")

WALLA = [f"w_kids_0{i}" for i in range(1, 7)]


@pytest.fixture(scope="module")
def ep101():
    build.build("ep101", only=set(WALLA), log=lambda *a: None)
    return lock.read(env.ep_dir("ep101"))


@pytest.fixture(scope="module")
def dx():
    build.build("dx_proof", log=lambda *a: None)
    return lock.read(env.ep_dir("dx_proof"))


# --------------------------------------------------------------------------- AT9.1
def test_at9_1_walla_locked_with_provenance(ep101):
    rows = ep101["lines"]
    for lid in WALLA:
        r = rows[lid]
        assert r["kind"] == "tts" and r["src"].startswith("tts:kokoro@"), lid
        assert r["voice"] == "kids_walla" and r["take"].startswith("store:")
        assert r["onset"] == env.PREROLL and r["dur"] > 0 and len(r["sha256"]) == 64
        assert abs(r["lufs"] - takes.DX_LUFS) < 0.6


def test_at9_1_walla_reproduce(ep101):
    d = env.ep_dir("ep101")
    lines, _ = __import__("showtk.voice.shwio", fromlist=["x"]).read_lines(os.path.join(d, "lines.shw"))
    voices, cast = takes.load_cast()
    for lid in WALLA:
        r = ep101["lines"][lid]
        # from the store (the vault's local mirror): the bytes are the locked samples
        assert takes.pcm_sha(takes.load(env.unrel(r["take"]))) == r["sha256"]
        # and a fresh render from the lock's own recipe gives the same samples
        line = lines[lid]
        plan, params = build.plan_line(line, voices, cast, {}, {}, ep101["lines"])
        assert plan["inputs"] == r["inputs"]
        tk = takes.render_tts(line, params)
        assert takes.pcm_sha(tk.y) == r["sha256"], lid


def test_at9_1_walla_bed_is_deterministic(ep101):
    from showtk.voice import walla
    a = walla.bed("ep101", "kids_far", 180 * env.SPF)
    b = walla.bed("ep101", "kids_far", 180 * env.SPF)
    assert a.shape == (180 * env.SPF, 2) and np.array_equal(a, b)
    assert np.abs(a).max() > 0.05 and np.abs(a).max() <= 0.89


# --------------------------------------------------------------------------- AT9.2
def test_at9_2_dx_proof_closures(dx):
    n = 0
    for lid, r in dx["lines"].items():
        res = report.closures(r)
        n += len(res)
        assert all(ok for _, ok in res), (lid, r["mouth"], res)
        assert len(set(r["mouth"]) - {"X"}) >= 3, (lid, r["mouth"])   # M1-T10: >= 3 visemes a line
        assert r["mouth_from"] == -1
    assert n >= 3     # "Perfect." p; "Habit, sweet pea." b, p


def test_at9_2_dx_proof_heard(dx):
    for lid, r in dx["lines"].items():
        assert isinstance(r["asr"], dict) and r["asr"]["wer"] == 0.0, (lid, r["asr"])


# --------------------------------------------------------------------------- AT9.3 / AT9.4
def _fake_recording(path, text="Stay."):
    """A stand-in for the creator's read: another voice, room tone and noise, 44.1 kHz."""
    import soundfile as sf
    import soxr

    from showtk.voice import g2p, kokoro
    rd = kokoro.render(g2p.G2P()(text), "am_adam", 0.95)
    y = soxr.resample(rd.y.astype(np.float64), 24000, 44100)
    rng = np.random.default_rng(1)
    pad = np.zeros(int(0.4 * 44100))
    y = np.concatenate([pad, 0.5 * y / np.abs(y).max(), pad])
    y = y + rng.normal(0, 0.002, len(y))
    sf.write(path, y, 44100, subtype="PCM_24")


def test_at9_3_swap_one_line_tts_to_recording(dx, tmp_path):
    src = env.ep_dir("dx_proof")
    d = tmp_path / "dx_swap"
    d.mkdir()
    shutil.copy(os.path.join(src, "lines.shw"), d / "lines.shw")
    shutil.copy(os.path.join(src, "voice.lock"), d / "voice.lock")
    before = json.loads((d / "voice.lock").read_text())["lines"]
    (d / "vo").mkdir()
    _fake_recording(str(d / "vo" / "co18_stay_take1.wav"))
    build.build(str(d), log=lambda *a: None)
    after = json.loads((d / "voice.lock").read_text())["lines"]
    changed = sorted(k for k in after if after[k] != before[k])
    assert changed == ["co18_stay"]
    r = after["co18_stay"]
    assert r["kind"] == "rec" and r["src"].startswith("rec:") and not r.get("synthetic")
    assert r["sha256"] != before["co18_stay"]["sha256"] and r["mouth"] and r["align"] in ("whisper", "energy")
    assert abs(r["lufs"] - takes.DX_LUFS) < 0.6
    # AT9.4: the recorded line drops off the human-voice list, every TTS line is on it
    tts_before = {x[0] for x in lock.tts_lines({"lines": before})}
    tts_after = {x[0] for x in lock.tts_lines({"lines": after})}
    assert tts_before == set(before) and tts_after == set(before) - {"co18_stay"}
    out = []
    assert report.report(str(d), require="human-voice", log=out.append) == 1
    listed = {ln.split(": ")[1].split(" ")[0] for ln in out if "[AUD-HUMAN]" in ln}
    assert listed == tts_after
    # and back: takes.toml says tts -> the original row returns exactly
    (d / "takes.toml").write_text('[co18_stay]\ntake = "tts"\n')
    build.build(str(d), log=lambda *a: None)
    back = json.loads((d / "voice.lock").read_text())["lines"]
    assert back["co18_stay"]["sha256"] == before["co18_stay"]["sha256"]
    assert all(back[k] == before[k] for k in before if k != "co18_stay")


def test_at9_4_require_human_voice_lists_every_tts_line(dx):
    out = []
    rc = report.report("dx_proof", require="human-voice", log=out.append)
    listed = [ln for ln in out if "[AUD-HUMAN]" in ln]
    assert rc == 1 and len(listed) == len(dx["lines"])
