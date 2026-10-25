# show/ - DOOMOGACHI LIFE in HELLvile Park!, the show

Episode 101 "Mingle Night" and the episodes after it are made here. Every
frame is drawn by the game's own PS1 engine (`show.exe`, captured from
PCSX-Redux at 320x180 inside the 320x240 display, 30 fps, scaled x6 to
1080p); audio is rendered on the host at 48 kHz, 1,600 samples per frame.

The binding design is `docs/show/SHOW_ENGINE_SPEC.md` v1.1 (read it first:
§0-§4, then the section of the package you work on). Decisions taken while
building are in `show/DECISIONS.md`; what was left out, and why, in
`show/DEFERRED.md`.

**The show never edits a game file.** Nothing outside `show/` changes:
not `core/`, `gba/`, `ps1/`, `tools/`, `trailer/`, `docs/` or the repo root.
`show.exe` is a stand-alone CMake project that reads the game's sources by
path and compiles patched *copies* of two portrait files (spec §4.3).
`show check-game` proves it before every push.

## Quick start

```sh
export PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b
show/tools/show doctor              # is this machine ready? (tools, hashes, disk)
show/tools/show build               # show.exe + show.bin/.cue in show/build/ps1-show/
show/tools/show check-game          # the gate, every commit (milliseconds)
show/tools/show check-game --full   # + the game's EXE/OVL.BIN/TEXT.BIN hash build (~4 min)
```

`show/tools/show` is a two-line shell script that runs `python3 -m showtk`
with `show/tools` on the path; it works from any directory. Every command is
headless, exits non-zero on failure and prints problems in one format
(below).

## Commands

| Command | Does |
|---|---|
| `show doctor [--models] [--boot]` | Python and its libraries; the command modules load; cmake, ninja, the mipsel compiler, mkpsxiso, xvfb-run, ffmpeg (libx264, aac, flac, ffv1); the emulator, SDL3, BIOS, SDK and toolchain hashes against `show/vault.lock`; free disk (WARN < 3 GB, FAIL < 2 GB); show.exe built. `--models` also hashes the TTS/ASR models; `--boot` boots show.exe to the test chart in the emulator |
| `show build [--preset ps1-show\|ps1-show-8mb] [--clean] [-j 2]` | Configures `show/engine` into `show/build/<preset>/` and builds `show.exe`, `show.map` (nm, what the harness reads), `show.ldmap`, `ram.txt` (the §8.7 RAM report) and the disc `show.bin/.cue` |
| `show build --variant selftest\|selftest-game` | The AT1.3 test builds, in `show/build/test-<variant>/` |
| `show check-game [--full] [--base REF] [--rebuild-base]` | The game-unchanged gate (§4.4): every path changed since the merge-base with `origin/claude/nice-allen-gr6czm`, and every uncommitted change, must be under `show/`. `--full` builds the game's `ps1` preset from `git archive` of the merge-base and of HEAD and compares `doomogachi.exe`, `OVL.BIN` and `TEXT.BIN` (never the `.bin` image: mkpsxiso stamps it). The merge-base's hashes are cached per game-tree id in `show/build/check-game/` |
| `show vault pack [--out DIR] [--only emu,sdk]` | Packs the stripped emulator + libSDL3 + openbios.bin + the GPL/zlib licences (`vault-emu`) and `/opt/psn00bsdk` + the mipsel cross toolchain (`vault-sdk`) as reproducible `.tar.xz`, split into 95 MiB `.partNN` pieces, into DIR (default `<scratch>/vault/`), and writes every hash to `show/vault.lock`. The models are pinned in `vault.lock` but not packed this week |
| `show vault verify [--dir DIR]` | AT0.3: the installed tools, the models and the archive parts against `show/vault.lock` |
| `show vault fetch DIR [--root /] [--force]` | Restores from a directory holding the parts: checks every part's hash, unpacks under `--root`, never overwrites a file whose content differs (unless `--force`) and never writes through the scratchpad symlink |

Commands still to come (other packages; spec §14):
`render`, `voice`, `music`, `sfx`, `mix`, `sheet`, `timing`, `lint`,
`preview`, `deliver`, `standards`, ...

## Compiler outputs (WP4, `showc`; other lanes build on these)

`show compile ep101 [--shots co01..co03]` and `show slug ep101` write, under
`show/build/ep101/` (`--out DIR` moves it):

| File | Consumer | What |
|---|---|---|
| `EP101.SHW` (`SLUG.SHW` for `show slug`) | show.exe | The binary, `show/include/show/shw_format.h` (mirrored by `showtk/shw_format.py`, AT4.2) |
| `timeline.json` | every host tool (deliver, review, standards, mix) | Acts, beats, scenes, shots, events, lines, strings, tags in absolute frames, with `file`/`line` provenance |
| `audio_events.json` | voice, music, mix, QC (P4) | Every sound: event index `k`, absolute sample (spec §6.6) |
| `captions.json` | captions, deliver | SDH cues, frame-exact in/out |
| `plan.txt`, `lint.txt` | Claude | Per-shot bytes/badge; every diagnostic in the §13.4 format |

All frames are **episode frames** (frame 0 = the first frame of the cold
open) unless a field says `shot_f` (frames from the shot's frame 0). Samples
are at 48 kHz: `smp = 1600 * frame`. `tc` strings are `HH:MM:SS:FF` at 30 fps.
`end` fields are **exclusive** (`start + len`). Every format carries a
`"format"` version string; additions are add-only within a version.

### `timeline.json` (`"format": "show-timeline/1"`)

```json
{"format": "show-timeline/1", "episode": "ep101", "id": "e101", "title": "Mingle Night",
 "fps": 30, "rate": 48000, "spf": 1600, "total_frames": 39600, "slug": false,
 "inputs_hash": "<sha256 of every input file>",
 "acts":   [{"id": "CO", "title": "Cold open", "start": 0, "len": 3000, "beats": ["e101_co_1", "..."]}],
 "beats":  [{"id": "e101_co_1", "act": "CO", "title": "INSERT. A CRAYON CALENDAR PAGE", "start": 0, "len": 90,
             "tc_in": "00:00:00:00", "elastic": false, "protected": false, "shots": ["co01"],
             "slug_frames": 0, "file": "show/ep101/rundown.shw", "line": 12}],
 "scenes": [{"id": "co_storybook", "shots": ["co01", "co02", "co03"], "start": 0, "end": 390,
             "children": true, "child_sources": ["crowd co03.bunnies (age=child)"], "gore": "CUTE"}],
 "shots":  [{"id": "co01", "index": 0, "beat": "e101_co_1", "scene": "co_storybook", "start": 0, "len": 90,
             "end": 90, "tc_in": "00:00:00:00", "set": "calendar_page", "set_kind": "2d", "px": 2,
             "look": "crayon", "cut": "FADE", "matte": "open", "gore": "CUTE", "text": "on",
             "insert": false, "slug": false, "base": null, "replay": null, "seed": 1,
             "cast": [{"id": "mayor", "slot": 0, "kind": "figure", "age": "adult", "tags": [], "voice": "mayor"}],
             "crowds": [{"id": "bunnies", "n": 6, "age": "child", "kind": "chibi", "species": "SPECIES_BUNNYKIN"}],
             "lines": ["co15_perfect"], "strings": ["s_co02"], "audio_k": [0, 1, 2],
             "waivers": [{"rule": "CANON-HV107", "reason": "...", "file": "...", "line": 28}],
             "asserts": [{"expr": "bandstand.x == 160", "shot_f": 90, "ok": true, "value": 160}],
             "provisional": false, "chunk_bytes": 1234, "badge": "PS1-LEGAL",
             "file": "show/ep101/shots/co.shw", "line": 3,
             "events": [{"f": 0, "shot_f": 0, "end": 8, "op": "fade", "kind": "engine|show|audio|text|mark",
                         "text": "fade in over=R8", "k": null, "file": "...", "line": 5}]}],
 "lines":  [{"id": "co15_perfect", "speaker": "mayor", "text": "Perfect.", "dir": "...", "shot": "co15",
             "start": 1234, "end": 1270, "provisional": true, "tags": ["innuendo"], "to": null, "ref": null,
             "consent": null, "voice": "mayor", "age": "adult", "file": "...", "line": 3}],
 "strings": [{"id": "s_co02", "text": "...", "shot": "co02", "start": 120, "end": 270, "style": "type"}],
 "tags":   [{"on": "shot|line|scene|string|look", "id": "co03", "shot": "co03", "scene": "co_storybook",
             "tags": ["innuendo"], "to": null, "ref": null, "consent": null, "start": 270, "end": 390,
             "file": "...", "line": 30}],
 "diagnostics": [{"rule": "FMT-RUNTIME", "severity": "warning", "text": "<the §13.4 line>"}]}
```

A beat whose shots do not fill its `len` is padded with a **slug card** for
the rest (`slug_frames`; the shot entry has `"slug": true` and id
`slug:<beat>`), so every compile is a full-length episode: `show slug` is a
compile with no shots at all (60 slug cards, 39,600 frames for 101).

### `audio_events.json` (`"format": "show-audio/1"`)

```json
{"format": "show-audio/1", "episode": "ep101", "rate": 48000, "spf": 1600, "total_smp": 63360000,
 "events": [
  {"k": 0, "smp": 0, "src": "FOL_crayon_scratch", "follow": "co01:circle_midsummer", "bus": "FX", "gain_db": -14},
  {"k": 2, "smp": 96000, "src": "cue:mingle_box", "from": "bar:1.1", "bus": "MX", "gain_db": -12},
  {"k": 3, "smp": 144000, "src": "FX_SHWIP", "bus": "FX", "gain_db": -10, "pan": 0.0},
  {"k": 4, "smp": 144000, "src": "walla:kids_far", "op": "on", "bus": "DX", "gain_db": -28, "cc": "[soft crowd babble]"},
  {"k": 6, "smp": 432000, "src": "walla:kids_far", "op": "to", "gain_db": -20, "over_smp": 48000},
  {"k": 9, "smp": 1234000, "src": "line:co15_perfect", "speaker": "mayor", "bus": "DX", "len_smp": 57600,
   "provisional": true}]}
```

`k` is dense, in (sample, source order). Keys appear only when they apply:
`src` is an SFX/foley id from `series/sfx.shw`, `cue:<id>` (music),
`walla:<bed>`, `amb:<id>` or `line:<id>`; `op` is absent for a one-shot or
cue start and is `on|off|to|stop|mute|duck|bend|tapestop` otherwise; `follow`
(`<shot>:<stroke>`), `from` (`bar:B.b` or `crowd:<shot>.<id>,...`),
`gain_db` (sfx/fol: the library level plus the shot's `gain=` trim;
walla/mx/amb: the absolute level), `pan` (-1..1; `center` = 0.0), `over_smp`,
`len_smp`, `cc` (an SDH caption chained on the same line). Every event's
shot, frame and source line are in `timeline.json` (`events[].k`). A timed op
in `ep101/cues.shw` (`@<beat>+N mx ...`) is placed in the shot (or slug card)
that covers its frame and merged with an identical shot op.

### `captions.json` (`"format": "show-captions/1"`)

`{"cues": [{"in": 90, "out": 270, "text": "[soft crowd babble]", "kind": "sdh|line|super", "shot": "co02"}]}`
(frames, `out` exclusive).

### Scenes and standards tags (the syntax the lints read)

| Where | Syntax |
|---|---|
| Shot header | `shot co01 ... scene=co_storybook gore=CUTE\|BRUTAL`; a shot without `scene=` continues the previous shot's scene (a new act starts none: an untagged first shot is scene `<beat>`) |
| Shot body, setup | `scene <id>` (same as `scene=`); `tag shot <tag>[,<tag>...] [to=<char>] [ref=<char>] [consent=<line>]` (whole shot) |
| Shot body, timed | `+N tag shot <tags> [over=+a..+b]` (a frame range); `+N tag line <line id> <tags> ...`; `tag scene <tags>` |
| `lines.shw` | `line <id> <speaker> "<text>" [dir="..."] [tags=innuendo,advance] [to=<char>] [ref=<char>] [consent=<line id>]` |
| `strings.shw` | `string <id> "<text>" [font=] [tags=...]` |
| Ages | `age=adult\|teen\|child` on every `char` (cast.shw), `kset`/crowd and `voice` |

Tags are the spec's closed list (§5.5, §13.5): `innuendo implied offscreen
suggestive advance contact touch yes no stepback gore_truth rear satire
explicit consent_gag`; the compiler refuses any other word (`STD-TAG`), and
records every tag in `timeline.json` `tags[]` with the scene it falls in and
its frame range. The compiler itself only collects and validates them; the
`STD-*` rules live in `showtk/standards/` (WP17) and read `timeline.json`.
`waive <RULE> "<reason>"` refuses `STD-EXPLICIT`, `STD-CONSENT`, `STD-AGE`,
`STD-KIDS` and `STD-HATE`.

## Adding a command (every workflow does it this way)

Create **one new file** `show/tools/showtk/commands/<name>.py`:

```python
"""show my-verb - one line on what it does."""

def _run(args):
    from .. import errors, paths       # import heavy things here, not at module top
    ...
    return 0                           # the exit code

def register(subparsers):
    p = subparsers.add_parser("my-verb", help="one line for `show --help`")
    p.add_argument("episode")
    p.set_defaults(func=_run)
```

`showtk/cli.py` imports every module in `commands/` at start-up (files
starting with `_` are skipped) and calls its `register(subparsers)`; no
shared file is edited. A module that fails to import is reported by name and
the other commands keep working (`show doctor` lists it as FAIL). Until the
CLI is on your branch, run a module directly with `python3 -m`. Keep imports
at the top of a command module cheap: every `show` invocation imports all of
them.

## Diagnostics (spec §13.4)

```
show/ep101/shots/co.shw:88: co15 +153 [AUD-OVERRUN] line co15_perfect (36 f) ends at +189, after the shot's
  tail hold starts (+180, `button`). Move it to +141, use a shorter take, or set retime="take" in takes.toml.
```

`<file>:<line>: <shot> +<frame> [<RULE>] <message>`, each part optional;
warnings say `warning:` before the message; long lines wrap with a two-space
indent. In code: `from showtk.errors import Diag, ShowError` and
`raise ShowError(Diag("RULE", "message", file=..., line=..., shot="co15", frame=153))`;
the CLI prints it and exits 1. `showtk.errors.report(diags)` prints a list
and returns the error count.

## Paths

`showtk/paths.py` holds every location: `paths.REPO`, `SHOW`, `ENGINE`,
`BUILD` (`show/build/`, gitignored), `build_dir(preset)`, `episode_dir(ep)`,
`SCRATCH` (env `SHOW_SCRATCH`; the session scratchpad by default) with
`VAULT_DIR`, `SHOW_LIB`, `SHOW_OUT`, `SHOW_WORK`, `THEME` under it, and the
machine tools `EMU` (env `SHOW_EMU`), `BIOS` (`SHOW_BIOS`), `PSN00BSDK_LIBS`,
`SDL3`, `GAME_HARNESS` (`ps1/harness/run.py`, run read-only). `paths.rel(p)`
gives the repo-relative form for messages.

## The engine (`show/engine/`, spec §4.1)

| File | What |
|---|---|
| `CMakeLists.txt`, `CMakePresets.json` | The stand-alone project. Presets `ps1-show` (retail 2 MB) and `ps1-show-8mb` (the EMU-ONLY RAM hatch; `SHOW_RAM_KB=8192`). Binary dirs `show/build/<preset>/` |
| `core_sources.cmake`, `ps1_sources.cmake` | The curated link closure (§8.7, §15.1): the portrait engine, beat, camfx, tween, ui/font/palette, txt, resident/catalog/rng, **town.c** (`doomo_town`, referenced by beat.c and hcmd.c), **options.c** (camfx/ui call `opt_get`), art accessors, the house and map modules for ROOM/VIEW3D/OVERWORLD (dropped by `--gc-sections` until a stage uses them); all of `ps1/src` except the engine test. Never game.c, scene.c or a scene |
| `patches/portrait.c.patch`, `patches/portrait_head.c.patch` | The portrait hunks (§4.3): (a) `-DPC_BLOCKS=240` (90 KB) with `static_assert(PC_BLOCKS < PC_FREE)` and the sliced job's entry index widened to `int16_t` (240 entries overflow its `int8_t`; DECISIONS.md); (b)/(b') the mouth override folded into the bust/figure and chibi cache keys; (c) the blink override in `phase_of()`; (f) no per-tick render budget; (d) the mouth override in `mouth()`; (g) a phase due now always gets pool blocks (the soft claim could leave the old blink on screen; DECISIONS.md). All `#if DOOMO_SHOW` |
| `apply_patch.py` | Applies a patch to a copy in `show/build/<preset>/gen/`. No fuzz: a hunk whose context changed stops the show build with the patch, the game file and the hunk (AT1.4); the game build is never involved |
| `show_game.c` | `doomo_init`/`doomo_frame` for the show, plus what game.c and scene.c would define: `doomo_frame_count`, `doomo_image_new_uid`, `doomo_initialized`, and the four no-op scene entry points hcmd.c references (`scene_set`, `scene_push`, `scene_top`, `scene_by_name`) |
| `show_chart.c` | The boot test chart: the 320x180 picture, title-safe box, Wall line at x = 160, palette 0-255, a bust (px=2), a figure and a chibi from the portrait engine, an 8 px ruler, matte bars last |
| `show_actor.c` + `show/include/show/show_portrait.h` | `portrait_show_mouth`, `portrait_show_blink` (-1 = the engine's own) and `show_portrait_overrides(mouth, blink)` |
| `show_audio_stub.c` | The core audio API as strong no-ops (the PS1 never plays sound in a show), so no weak stub runs |
| `show_selftest.c` | AT1.3 only (`-DSHOW_SELFTEST=ON`): CRC32 of busts, figures and chibis for 20 roster looks x 11 expressions on the TTY, plus the override checks |
| `ram_report.py`, `iso_show.xml`, `system.cnf` | The elf2exe step + RAM budget, and the disc (`SHOW.EXE`, episodes under `SHOW/` via `-DSHOW_ISO_FILES=EP101.SHW=/abs/path`) |

New player sources: any `show/engine/show_*.c` is compiled automatically
(CMake globs them), so later packages add files without editing the
CMakeLists. Headers go in `show/include/show/`.

**When a game change breaks a patch** (the build stops with
`show: patch ... no longer applies to core/src/portrait.c: hunk N`): copy
the new game file, re-apply the hunk by hand under `#if DOOMO_SHOW`, and
regenerate with `diff -u --label a/core/src/portrait.c --label b/core/src/portrait.c old new > show/engine/patches/portrait.c.patch`.
Then `show build` and `python3 -m pytest show/tests/engine` (AT1.3 must
still say the images are the game's).

### Measured (M0-T4, 2026-10-25, this container)

| What | Value |
|---|---|
| `show.exe` (ps1-show, WP1 skeleton) | 382 KB image + 402 KB .bss; **1,199 KB left** below the stack top; after the two shot-chunk buffers (320 KB) and the rest of the player (120 KB): 759 KB headroom (§8.7 PASS) |
| Portrait pool | 240 x 384 B = 90 KB (`pool` 0x16800 B in show.map) |
| Build | 47 s from clean with `-j2` (89 steps), seconds incremental |
| Boot to the chart, game harness (throttled) | 300 frames in 27 s wall, 26.5 fps shown, 4.0 ms CPU per frame |
| `check-game --full` | two game builds, 131 s each with `-j2`; EXE `44e80cc7`, OVL.BIN `664c8573`, TEXT.BIN `50d371f0` at `d079776` |
| `vault pack` | vault-emu 5.9 MB xz (stripped emulator, SDL3, BIOS, licences) |

## Tests

```sh
python3 -m pytest show/tests/engine -m "not slow"   # seconds
python3 -m pytest show/tests/engine                 # + AT1.3 (two builds, two emulator runs), AT1.4, chart boot
```

## Layout

See spec §4.1 for the whole tree. Owners this week: ENGINE `engine/`,
`include/`, `tools/show`, the showtk core modules, `tests/{engine,compiler,
standards,dx_proof}/`, this README, `vault.lock`, `hooks/`; AUDIO `audio/`,
`showtk/voice|music|sfx`, `mix.py`, `master.py`, `lib/music`, `lib/licences`;
ART `art/`, `showtk/art/`, `series/cast.shw`, `acting.shw`, `looks/`, `sets/`.
Everyone appends to `DECISIONS.md` and `DEFERRED.md`. Binaries, renders,
models and audio are never committed (`show/.gitignore`); they live in the
vault and the scratch dirs.
