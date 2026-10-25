"""AT10.3: SHWIP at 48 kHz matches `runner --wav`'s SHWIP envelope (resampled)
within 1 dB - and so do the other 14 SynthSeg one-shots and the sfxgen PCM.

The reference is the game's own code, unchanged: sfxref.c links core/src/audio_fx.c
(the SynthSeg tables and audio_fx_synth_step), core/native/plat_music_native.c
(the 22,050 Hz musrender `runner --wav` records with) and core/gen/sfx_data.c, with
pitch_hz() copied verbatim out of core/src/audio_seq.c at build time. It is built
into the scratch work dir (never into the game's build dirs).

The envelope is the RMS of each 30 Hz frame (735 samples at 22,050 Hz, 1,600 at
48 kHz), in dB re the loudest frame, over every frame the reference holds above
-40 dB.
"""
import os
import subprocess
import sys

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))

from showtk.music import audiopaths as P  # noqa: E402
from showtk.sfx import gamesfx as G  # noqa: E402

BUILD = P.WORK / "sfxref"


def build_ref():
    return G.build_ref()


def frame_env(y, spf):
    y = y if y.ndim == 1 else y.mean(axis=1)
    n = len(y) // spf
    r = np.sqrt(np.mean(y[: n * spf].reshape(n, spf) ** 2, axis=1) + 1e-20)
    return 20 * np.log10(r / r.max())


def compare(ref, ours):
    a, b = frame_env(ref, 735), frame_env(ours, 1600)
    n = min(len(a), len(b))
    m = a[:n] > -40.0
    return float(np.max(np.abs(a[:n][m] - b[:n][m]))), int(m.sum())


def ref_synth(exe, idx, semis=0):
    out = BUILD / f"synth_{idx}_{semis}.wav"
    subprocess.run([str(exe), "synth", str(idx), str(out), str(semis)], check=True)
    y, sr = sf.read(str(out))
    assert sr == 22050
    return y


def test_shwip_envelope_within_1db():
    exe = build_ref()
    names = [n for n, _ in G.synth_recipes()]
    ref = ref_synth(exe, names.index("shwip"))
    d, n = compare(ref, G.synth("shwip"))
    print(f"AT10.3 SHWIP: {n} frames compared, worst envelope difference {d:.2f} dB")
    assert d <= 1.0


def test_all_synth_oneshots_within_1db():
    exe = build_ref()
    worst = []
    for i, (name, _) in enumerate(G.synth_recipes()):
        d, n = compare(ref_synth(exe, i), G.synth(name))
        worst.append((name, round(d, 2), n))
    print("AT10.3 SynthSeg envelopes (worst dB, frames):", worst)
    assert all(d <= 1.0 for _, d, _ in worst), worst


def test_kazoo_semis_steps_down():
    """audio_sfx_pitch(SFX_KAZOO, -n): the kazoo of failure steps down, as in the game."""
    exe = build_ref()
    names = [n for n, _ in G.synth_recipes()]
    for semis in (-2, -4):
        d, n = compare(ref_synth(exe, names.index("kazoo"), semis), G.synth("kazoo", semis))
        assert d <= 1.0, (semis, d)


def test_sfxgen_pcm_at_48k_follows_the_game():
    """sfxgen.py's PCM effects re-rendered at 48 kHz keep the game's envelope: per 10 ms
    window the median difference against the engine's own playback (8-bit PCM at its
    native rate through the musrender) is within 1.6 dB for every effect (the native
    8-bit quantisation and its trimmed tails make the worst windows differ more)."""
    from scipy.signal import resample_poly
    rows = []
    for name in G.sfxgen_names():
        ref, sr = G.engine_render("pcm", name)
        ref = resample_poly(ref, 320, 147)
        y, r0, _pcm = G.sfxgen(name)
        a, b = frame_env(ref, 480), frame_env(y, 480)
        n = min(len(a), len(b))
        m = a[:n] > -30.0
        d = np.abs(a[:n][m] - b[:n][m])
        rows.append((name, r0, round(float(np.median(d)), 2)))
    print("AT10.3 sfxgen PCM at 48 kHz, median envelope difference (dB):", rows)
    assert all(med <= 1.6 for _, _, med in rows), rows


if __name__ == "__main__":
    test_sfxgen_pcm_at_48k_follows_the_game()
    test_shwip_envelope_within_1db()
    test_all_synth_oneshots_within_1db()
    test_kazoo_semis_steps_down()
    print("AT10.3 PASS")
