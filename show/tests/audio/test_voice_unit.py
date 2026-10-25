"""Fast tests for showtk.voice (no models): the .shw readers, G2P, mouth rules, WER, lock IO."""
import os

import numpy as np
import pytest

from showtk.voice import env, g2p, lock, mouth, shwio, takes
from showtk.voice import asr as vasr


def test_tokenize_quotes_and_kv():
    t = shwio.tokenize('line a1 nana "It\'s mostly \\"Hee. Hee.\\"" dir="her own read" take=lib:x/y # c')
    assert t[0] == "line" and t[1] == "a1" and t[2] == "nana"
    assert t[3] == ("str", 'It\'s mostly "Hee. Hee."')
    assert ("kv", "dir", "her own read") in t and ("kv", "take", "lib:x/y") in t
    assert len(t) == 6


def test_ep101_lines_and_beds_parse():
    lines, beds = shwio.read_lines(os.path.join(env.SHOW, "ep101", "lines.shw"))
    assert len(lines) >= 100
    assert beds["kids_far"].lines == [f"w_kids_0{i}" for i in range(1, 7)]
    assert lines["co13_heehee"].lib_take == "lib:mayor/heehee_v1"
    assert lines["a2_01_bone_01"].attrs.get("mode") == "possessed"


def test_every_line_has_a_voice_with_an_age():
    voices, cast = takes.load_cast()
    for v in voices.values():
        assert v.age in ("adult", "teen", "child"), v.id
    for ep in ("ep101", "dx_proof"):
        lines, _ = shwio.read_lines(os.path.join(env.ep_dir(ep), "lines.shw"))
        for ln in lines.values():
            vid, v = takes.voice_for(ln, voices, cast)
            assert v.age, (ln.id, vid)
    # the kids are children; the fog's child echo is never Pip's voice
    assert voices["kids_walla"].age == "child" and voices["pip"].age == "child"
    assert "af_sky" not in voices["fog_child"].attrs.get("kokoro", "")


@pytest.fixture(scope="module")
def G():
    if not env.cmudict_path(fetch=False):
        pytest.skip("cmudict data not available")
    return g2p.G2P()


@pytest.mark.parametrize("word,phon", [("HELLvile", "hˈɛlvˌIl"), ("Grumbleguts", "ɡɹˈʌmbəlɡˌʌts"),
                                       ("Mascotty", "mæskˈɑɾi"), ("Gigglesworth", "ɡˈɪɡəlzwˌɜɹθ"),
                                       ("Bonejangles", "bˈOnʤˌæŋɡəlz"), ("HaHaland", "hˈɑhəlˌænd")])
def test_lexicon_names(G, word, phon):
    assert G(word).segments[0].words[0].phon == phon


def test_no_oov_in_ep101(G):
    lines, _ = shwio.read_lines(os.path.join(env.SHOW, "ep101", "lines.shw"))
    oov = {lid: G(ln.attrs.get("say", ln.text)).oov for lid, ln in lines.items()}
    assert not {k: v for k, v in oov.items() if v}


def test_pause_codes_split_segments(G):
    u = G("They all drew the same friend.{..} I don't know who he is.")
    assert len(u.segments) == 2 and u.segments[0].pause_after == 8
    assert G("{d}Needs death.").flags == {"deadpan"}
    assert G("Mm! A-plus!").phon.startswith("ˈʌm! ˈA plˈʌs")


def test_mouth_rules_lead_hold_and_closures():
    spf = env.SPF
    # "m a b a" : m 0-2 f, a 2-8 f, b 8-8.5 f (a short closure), a 8.5-14 f, silence after
    ph = [("m", 0, 2 * spf), ("a", 2 * spf, 8 * spf), ("b", 8 * spf, int(8.5 * spf)), ("a", int(8.5 * spf), 14 * spf)]
    y = np.zeros(16 * spf, np.float32)
    y[: 14 * spf] = 0.3 * np.sin(np.arange(14 * spf) * 0.05)
    m, mf = mouth.track(ph, y, mouth.DEFAULT_RULES)
    assert mf == -1
    assert m[0] == "M"                                   # the first m shows one frame early (lead)
    i_b = 8 - mf                                         # sound frame 8 -> track index 9
    assert "M" in m[i_b - 2:i_b + 1]                     # the short b still gets its M
    runs = mouth._runs(list(m))
    for v, a, b in runs[1:-1]:
        assert b - a >= 2 or v == "M", (m, v, a, b)       # holds >= 2 f except M
    assert m.endswith("X")


def test_mouth_gap_closing():
    s = list("AAXXAA")
    assert "".join(mouth._close_gaps(s, 3)) == "AAAAAA"   # a 2 f gap stays open
    s = list("AAXXXAA")
    assert "".join(mouth._close_gaps(s, 3)) == "AAXXXAA"  # a 3 f gap closes


def test_check_closures():
    m = "XMMAAAMAAX"
    res = mouth.check_closures(m, -1, [0, 6 * env.SPF + 10, 3 * env.SPF])
    assert [ok for _, ok in res] == [True, True, False]


def test_wer():
    assert vasr.wer("Two of everything, dear.", "two of everything dear") == 0.0
    assert vasr.wer("Stay.", "Say.") == 1.0
    assert vasr.wer("I saw them 4 times", "i saw them four times") == 0.0


def test_quantize_round_trip(tmp_path):
    import soundfile as sf
    y = (0.5 * np.sin(np.arange(4800) / 7.0)).astype(np.float32)
    q = takes.quantize(y)
    p = tmp_path / "x.flac"
    sf.write(p, (q.astype(np.int64) << 8).astype(np.int32), 48000, subtype="PCM_24", format="FLAC")
    assert takes.pcm_sha(takes.load(str(p))) == takes.pcm_sha(y)


def test_lock_write_read(tmp_path):
    lk = {"episode": "t", "sr": 48000, "lines": {"b": {"kind": "tts", "mouth": "XMX", "words": [["a", 0, 1]]},
                                                  "a": {"kind": "rec", "src": "rec:00"}}}
    lock.write(str(tmp_path), lk, order=["b", "a"])
    back = lock.read(str(tmp_path))
    assert list(back["lines"]) == ["b", "a"] and back["lines"]["b"]["words"] == [["a", 0, 1]]
    assert [x[0] for x in lock.tts_lines(back)] == ["b"]


def test_babble_keeps_rhythm():
    b = takes.babble("...needs salt.", 3)
    assert b.endswith(".") and len(b.split()) == 2
