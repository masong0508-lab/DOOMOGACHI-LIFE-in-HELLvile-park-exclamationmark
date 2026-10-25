# show/audio: the vendored audio and voice kit (spec §10.7)

Text sources only. Samples, models, renders and caches are **not** in git: they live in
the vault (`show/vault.lock`) and, on this container, in the scratchpad they came from.

| Here | Vendored from (scratchpad, 2026-10-25) | What |
|---|---|---|
| `kit/*.py`, `KIT.md` | `theme/kit/kit/`, `theme/kit/KIT.md` | The production kit: score, instruments, samplers, mixer, master, QA (48 kHz float64) |
| `demo/*.py`, `tests/*.py` | `theme/kit/demo/`, `theme/kit/tests/` | The kit's own demos and smoke tests (AT0.4 renders `demo/groove16.py`) |
| `voice/kokoro_voice.py` | `theme/final/src/` | Kokoro-82M v1.0 runner: tokenizer, voices, `speak()`, `sing()` |
| `voice/kokoro_patch.py` | `theme/final/src/` | Writes `kokoro_analysis.onnx` (exposes `dur_pred`/`f0_pred`/`n_pred`) and `kokoro_sing.onnx` from the fp32 model |
| `voice/singer.py` | `theme/final/src/` | Kokoro + WORLD singer (hums, sung lines, the library "Hee. Hee.") |
| `voice/asr.py` | `theme/final/src/` | Whisper small (int8, faster-whisper) read-back, `recall()`, `char_sim()` |

## Local edits (the only differences from the sources)

Each one makes a path overridable, so the code runs from git with the binaries elsewhere.
With no variable set, every default is the original one.

| File | Variable | Default |
|---|---|---|
| `kit/core.py` | `KIT_ASSETS` (the 168 MB sample set), `KIT_CACHE` | `show/audio/assets`; the cache moved from `show/audio/.cache` to `show/build/cache/kit` so nothing is written into the repo |
| `voice/kokoro_voice.py` | `KOKORO_MODELS` (the patched `kokoro_analysis.onnx` + `kokoro_sing.onnx`), `KOKORO_VOICES` (the 54 `*.bin` style files) | the theme's own layout |
| `voice/kokoro_patch.py` | `KOKORO_MODELS` (output), `KOKORO_SRC` (`kokoro-fp32.onnx`) | the theme's own layout |
| `voice/asr.py` | `WHISPER_MODEL` (the CTranslate2 int8 model dir) | the theme's own layout |

`show/tools/showtk/voice/env.py` finds the models (environment, then `show/build/models/`,
then the scratchpad's `theme/`) and sets these variables before it imports anything here.

## Source hashes (sha256, first 16 hex, before the edits above)

```
0031a02aaefd73ec KIT.md                 957de863fcc71227 demo/compare_sources.py
100678aedea3f75d kit/__init__.py        b8d70c3618d6f77a demo/groove16.py
3fead6c0a6b4cffe kit/core.py            7e77a0cea9d32f37 demo/showcase.py
eb8e348b8326c676 kit/drums.py           33f9fd49167eea41 tests/audition.py
1673e973001717ca kit/dsp.py             d06f933875a82db5 tests/contact.py
7a67c8fde98ea45d kit/fx.py              61dc549439ac2929 tests/smoke.py
d9415284ebe24605 kit/instrument.py      b5f18198f43db0ec voice/asr.py
04045024c2df4ca2 kit/master.py          8143b92735e64809 voice/kokoro_patch.py
b9bf8c2664dd1caa kit/mixer.py           b1e8d52e3d76b98b voice/kokoro_voice.py
bd404c3573098d2c kit/qa.py              65144bd98a752d52 voice/singer.py
dceed3dbd7d093df kit/render.py
3f81b998c101f39a kit/reverb.py
c4bf73ab276fbc94 kit/samplers.py
7d5848672441ff32 kit/score.py
fb80a75d38d1a477 kit/synths.py
142f32c8e17712a8 kit/theory.py
```

## Binaries this code needs (vault, `vault-models` / `vault-audio`)

| Item | Scratchpad path today | Size |
|---|---|---|
| Kokoro fp32 ONNX | `theme/final_dl/npm_kokoro_fp32/kokoro-fp32.onnx` | 326 MB |
| Patched analysis / sing models (or regenerate with `voice/kokoro_patch.py`) | `theme/final/work/models/` | 2 x 326 MB |
| 54 Kokoro voices | `theme/final_dl/npm_kokorojs/x/package/voices/` | 28 MB |
| Whisper small int8 | `theme/final_dl/npm_whisper_small/m/` | 248 MB |
| CMU Pronouncing Dictionary data (`cmudict.dict`, BSD-style; from the PyPI `cmudict` 1.1.3 wheel, read as data, never imported) | `show_lib/g2p/cmudict.dict` | 3.6 MB |
| Kit samples (CC0 / CC BY, see `show/lib/licences/`) | `theme/kit/assets/` | 168 MB |

Licences: `show/lib/licences/licences.toml`.
