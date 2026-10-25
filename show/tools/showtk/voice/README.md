# showtk.voice: dialogue for the show (WP9; spec §9.3, §10.2-§10.3)

Run from `show/tools` as `python3 -m showtk.voice ...` (or `show voice ...` once the ENGINE's
`cli.py` loads `showtk/commands/voice.py`).

| Command | Does |
|---|---|
| `show voice ep101` | Renders (TTS) or ingests (recordings) every line whose inputs changed, aligns it, makes its mouth track, runs Whisper, writes `show/ep101/voice.lock` |
| `show voice ep101 --line co15_perfect --force` | One line, rebuilt even if unchanged |
| `show voice dx_proof` | The dialogue proof's four lines (`show/tests/audio/dx_proof/`) |
| `show voice verify ep101` | AT9.1: every take is in the store with its locked hash, and every TTS take re-renders to the same samples |
| `show voice report ep101 --require human-voice` | AT9.4: lists every line still synthetic (exit 1 if any) |
| `show voice lint ep101` | AUD-ASR, G2P-OOV, STD-AGE (voices), VOX-MOUTH (an M within 1 f of every measured bilabial closure), AUD-HEEHEE |
| `show voice mouth ep101 co15_perfect` | The track on its frame ruler, word frames, closure check |
| `show voice strip ep101 <line...> --out x.png` | Spectrogram + waveform + mouth letters + closures, one row per line |
| `show voice audition mayor "…Hee. Hee. Perfect."` | The role's voice and 5 neighbours as MP3s with Whisper read-backs |
| `show voice casting` | 3 numbered options per main role + `CASTING.md` (scratchpad `show_out/voices/`) |
| `show voice ingest take.wav --line co15_perfect` | Copies a recording to `show/ep101/vo/<id>_take<n>.wav` and re-locks that line |
| `show voice bed ep101 kids_far --frames 300 --out bed.wav` | A walla bed built from its locked lines |
| `show voice lib make [mayor/heehee_v1 ...]` / `lib list` | Library takes (`lib:` sources) |
| `show voice import-lines ep101 <script.md ...>` | `lines.shw` from the writers' ```shw blocks |
| `show voice g2p "Welcome to HaHaland"` | The phonemes Kokoro gets |

## Files

| File | Owner | What |
|---|---|---|
| `show/series/voices.shw` | creator (Claude edits) | Every voice: Kokoro voice or blend, speed, WORLD shaping, age; `audition` options; `sample` reads |
| `show/series/lexicon.shw` | Claude | Names and coinages in Kokoro phonemes (HELLvile, Grumbleguts, Mascotty, ...) |
| `show/series/visemes.shw` | Claude | Mouth rules (lead 1, hold 2, gap 3, gate -38 dB) and viseme -> `MOUTH_*` per look family |
| `show/ep101/lines.shw` | creator + Claude | `line <id> <speaker> "<text>" [dir=] [mode=possessed] [say=] [take=lib:..] [sync= late=] [asr=skip] [mouth=]`, `walla <bed> lines=a..b density= pan= seed=` |
| `show/ep101/takes.toml` | creator | `[id] fav = n` / `take = "tts" \| "rec:n" \| "rec:<path>" \| "lib:<name>"`, `retime = "fit" \| "take"` |
| `show/ep101/vo/` | creator | Drop folder for recordings (gitignored): `<id>_take<n>.wav`, `<id>_s<n>.wav` |
| `show/ep101/voice.lock` | generated, committed | See below |
| `show/lib/takes/index.json` | generated, committed | Library takes: hash, how made, synthetic or not |

## voice.lock (format `show-voice-lock/1`), the contract with the compiler and the mixer

JSON, one row per line id in `lines.shw` order; all times are samples at 48 kHz.

- `take` (`store:takes/<voice>/<ep>/<id>_<kind>_<sha12>.flac`, under `$SHOW_LIB`, default the
  scratchpad's `show_lib/`), `sha256` of its 24-bit samples, `kind` (`tts | rec | scratch | lib |
  pending`), `src` provenance (`tts:kokoro@<fp32 sha16>+<analysis sha8>:<voice>,speed=..,pitch=..`
  or `rec:<file sha16>` or `lib:<name>@<sha12>`), `synthetic: true` on every TTS-made take.
- `onset` = 1440 (the 30 ms pre-roll: the first sound is take sample 1440), `len` (take
  samples), `dur` (first to last sound), `frames` = ceil(dur / 1600).
- `words` / `phones`: `[text, start, end]` relative to the onset (stress marks folded into
  their vowel). `@<line>.w<k>` = say frame + words[k-1][1] // 1600.
- `mouth` (one viseme letter per frame) and `mouth_from` (-1): letter i is shown on frame
  `say + mouth_from + i`, so the mouth leads the sound by one frame.
- `asr` (`heard`, `wer`, `recall`, `char`) or `"skip"`; `lufs` (-23 dialogue level);
  `sync` (`line`, `late` in samples) on Clinker's twins: lay the take `late` samples after the
  target line's onset.

**Placement.** `say <line>` on frame F puts take sample `onset` on episode sample 1600·F: the
take file starts 1440 samples earlier. The mixer reads takes with `showtk.voice.takes.load()`
(float32, 48 kHz mono). Walla beds: `showtk.voice.walla.bed(ep, bed, n_samples)` -> (n, 2).

## Where binaries come from (`env.py`)

Models: `$KOKORO_MODELS`/`$KOKORO_VOICES`/`$WHISPER_MODEL`/`$SHOW_MODELS`, then
`show/build/models/`, then the scratchpad's `theme/`. CMU dictionary data: `$SHOW_CMUDICT`, then
`$SHOW_LIB/g2p/cmudict.dict`, else extracted from the pinned PyPI wheel (`cmudict==1.1.3`, sha256
checked; read as data, never imported). Takes: `$SHOW_LIB` (the vault's local mirror).
