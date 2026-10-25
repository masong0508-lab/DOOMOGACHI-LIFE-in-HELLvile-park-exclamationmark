"""Kokoro-82M v1.0 (Apache-2.0) runner: tokenizer, voices, speech and singing.

    kv = KokoroVoice()
    y, info = kv.speak("dˈum tˈæstɪk", "af_heart")          # normal speech
    y = kv.sing(tokens, durations, f0_curve, "af_heart")       # forced timing + pitch

Units: one duration frame = 600 samples at 24 kHz (25 ms); the F0 curve has two
values per duration frame (12.5 ms). Inference is capped at 2 threads.
"""
from __future__ import annotations

import os

import numpy as np
import onnxruntime as ort

HERE = os.path.dirname(os.path.abspath(__file__))
# show/audio vendoring: KOKORO_MODELS / KOKORO_VOICES point at the vault copies
MODELS = os.environ.get("KOKORO_MODELS") or os.path.abspath(os.path.join(HERE, "..", "work", "models"))
VOICES = os.environ.get("KOKORO_VOICES") or os.path.abspath(
    os.path.join(HERE, "..", "..", "final_dl", "npm_kokorojs", "x", "package", "voices"))
SR_K = 24000
HOP = 600          # samples per duration frame
F0_PER_FRAME = 2

# Kokoro v1.0 vocabulary (config.json "vocab"): the v0.19 symbol table with slots
# 17-23 re-used for ̃ ʣ ʥ ʦ ʨ ᵝ ꭧ, 'A' moved to 24 and 'ᵊ' at 42. Only the
# symbols v1.0 actually uses are kept.
_V019 = ("$;:,.!?¡¿—…\"«»“” ")
VOCAB = {";": 1, ":": 2, ",": 3, ".": 4, "!": 5, "?": 6, "—": 9, "…": 10, "\"": 11, "(": 12, ")": 13,
         "“": 14, "”": 15, " ": 16, "̃": 17, "ʣ": 18, "ʥ": 19, "ʦ": 20, "ʨ": 21, "ᵝ": 22, "ꭧ": 23,
         "A": 24, "I": 25, "O": 31, "Q": 33, "S": 35, "T": 36, "W": 39, "Y": 41, "ᵊ": 42,
         "a": 43, "b": 44, "c": 45, "d": 46, "e": 47, "f": 48, "h": 50, "i": 51, "j": 52, "k": 53,
         "l": 54, "m": 55, "n": 56, "o": 57, "p": 58, "q": 59, "r": 60, "s": 61, "t": 62, "u": 63,
         "v": 64, "w": 65, "x": 66, "y": 67, "z": 68, "ɑ": 69, "ɐ": 70, "ɒ": 71, "æ": 72, "β": 75,
         "ɔ": 76, "ɕ": 77, "ç": 78, "ɖ": 80, "ð": 81, "ʤ": 82, "ə": 83, "ɚ": 85, "ɛ": 86, "ɜ": 87,
         "ɟ": 90, "ɡ": 92, "ɥ": 99, "ɨ": 101, "ɪ": 102, "ʝ": 103, "ɯ": 110, "ɰ": 111, "ŋ": 112,
         "ɳ": 113, "ɲ": 114, "ɴ": 115, "ø": 116, "ɸ": 118, "θ": 119, "œ": 120, "ɹ": 123, "ɾ": 125,
         "ɻ": 126, "ʁ": 128, "ɽ": 129, "ʂ": 130, "ʃ": 131, "ʈ": 132, "ʧ": 133, "ʊ": 135, "ʋ": 136,
         "ʌ": 138, "ɣ": 139, "ɤ": 140, "χ": 142, "ʎ": 143, "ʒ": 147, "ʔ": 148, "ˈ": 156, "ˌ": 157,
         "ː": 158, "ʰ": 162, "ʲ": 164, "↓": 169, "→": 171, "↗": 172, "↘": 173, "ᵻ": 177}

UNVOICED = set("ptkfθsʃhʧçɸx") | {"ʦ", "ʨ", "ɕ"}
VOWELS = set("aeiouæɑɐɒɔəɚɛɜɪʊʌɨɯøœᵊᵻAIOQWY")
MARKS = set("ˈˌː ,.!?;:—…")


def tokenize(phon: str):
    ids = []
    for ch in phon:
        if ch not in VOCAB:
            raise ValueError(f"symbol {ch!r} (U+{ord(ch):04X}) not in the Kokoro vocab: {phon!r}")
        ids.append(VOCAB[ch])
    return ids


def _session(name):
    so = ort.SessionOptions()
    so.intra_op_num_threads = 2
    so.inter_op_num_threads = 1
    so.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    return ort.InferenceSession(os.path.join(MODELS, name), so, providers=["CPUExecutionProvider"])


class KokoroVoice:
    def __init__(self):
        self._an = None
        self._sing = None
        self._voices = {}

    @property
    def an(self):
        if self._an is None:
            self._an = _session("kokoro_analysis.onnx")
        return self._an

    @property
    def sing_sess(self):
        if self._sing is None:
            self._sing = _session("kokoro_sing.onnx")
        return self._sing

    def style(self, voice, n_tokens):
        """voice: a name, or a dict {name: weight} (a blended voice)."""
        if isinstance(voice, dict):
            tot = sum(voice.values())
            return sum(self.style(k, n_tokens) * (w / tot) for k, w in voice.items()).astype(np.float32)
        if voice not in self._voices:
            v = np.fromfile(os.path.join(VOICES, f"{voice}.bin"), dtype=np.float32).reshape(-1, 256)
            self._voices[voice] = v
        v = self._voices[voice]
        return v[min(max(n_tokens, 0), len(v) - 1)][None, :].astype(np.float32)

    def speak(self, phon, voice="af_heart", speed=1.0):
        ids = tokenize(phon)
        inp = np.array([[0] + ids + [0]], dtype=np.int64)
        out = self.an.run(None, {"input_ids": inp, "style": self.style(voice, len(ids)),
                                 "speed": np.array([speed], dtype=np.float32)})
        names = [o.name for o in self.an.get_outputs()]
        d = dict(zip(names, out))
        return d["waveform"][0], dict(dur=d["dur_pred"][0], f0=d["f0_pred"][0], n=d["n_pred"][0], ids=inp[0])

    def sing(self, phon_or_ids, durations, f0, voice="af_heart", n_fn=None, return_n=False):
        """durations: frames per token INCLUDING the leading/trailing pad token;
        f0: Hz per half-frame (len = 2 * sum(durations)).
        n_fn(n_pred) -> n_in edits the model's own energy curve (e.g. hold a vowel);
        None keeps the prediction."""
        ids = phon_or_ids if not isinstance(phon_or_ids, str) else tokenize(phon_or_ids)
        inp = np.array([[0] + list(ids) + [0]], dtype=np.int64)
        dur = np.asarray(durations, dtype=np.int64)[None, :]
        assert dur.shape[1] == inp.shape[1], (dur.shape, inp.shape)
        f0 = np.asarray(f0, dtype=np.float32)[None, :]
        assert f0.shape[1] == 2 * int(dur.sum()), (f0.shape, dur.sum())
        feed = {"input_ids": inp, "style": self.style(voice, len(ids)),
                "speed": np.array([1.0], dtype=np.float32), "dur_in": dur, "f0_in": f0,
                "n_in": np.zeros_like(f0)}
        # pass 1 reads the model's own energy for these durations (n_in does not feed it)
        n_pred = self.sing_sess.run(["n_pred"], feed)[0]
        n_in = n_pred if n_fn is None else np.asarray(n_fn(n_pred[0].copy()), dtype=np.float32)[None, :]
        feed["n_in"] = n_in.astype(np.float32)
        y = self.sing_sess.run(["waveform"], feed)[0][0]
        return (y, n_pred[0], n_in[0]) if return_n else y
