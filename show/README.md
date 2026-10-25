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

Commands still to come (other packages; spec §14): `compile`, `slug`,
`render`, `voice`, `music`, `sfx`, `mix`, `sheet`, `timing`, `lint`,
`preview`, `deliver`, `standards`, ...

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

## Staging reference

### WP6: actors, crowds, mouths (`show_cast.c`, `show_actor.c`, `show_crowd.c`)

The compiler side is `showtk/actors.py` (`ShotCast`, `parse_actor`, `parse_crowd`);
records are the byte images of `show/include/show/show_actor.h`. A shot's actors
wear the ART lane's looks (`show/series/cast.shw`, `looks/`); slot order is the
declaration order.

```
actor mayor who=mayor kind=figure at=110,150 face=R expr=EXPR_HAPPY idle=on
actor nana  who=nana  kind=bust   at=160,180 px=2
crowd plaza looks=kset:giggle_adults n=150 form=scatter area=0,100,320,60 flip=mix bob=on
end
crowd kids looks=kset:kids n=12 form=lane from=-20,150 spacing=26 speed=2.2 dir=R ripple=0..3 cycle=walk4
end

+0   mayor expr EXPR_SHOCK
+12  mayor headturn 64 2          ; turn 64 (of 256) over 2 frames
+20  mayor move 200,150 over=24 ease=SMOOTH
+20  nana  walk 40 over=30
+30  mayor squash 300 over=6
+40  mayor pop TWT_HOP2
+44  mayor pose wave speed=1.0    ; flipbook; `pose off` returns to the rig
+50  mayor state possessed
+60  mayor freeze 13              ; clock stops: wings/blink hold exactly 13 f
+80  kids  freeze | thaw | pop TWT_HOP2 | face L | speed 0.5 | show off
```

Actor options: `kind=bust|figure|chibi|fighter`, `at=x,y` (feet, show pixels),
`face=L|R|front|back`, `expr=EXPR_*`, `idle=on|still|frozen|none`, `px=1..3`,
`state=normal|possessed|ghost|dead`, `hidden`. Timed ops: `expr`, `blink`, `face`,
`move`, `walk`, `pop`, `squash`, `headturn`, `pose`, `state`, `show on|off`,
`twitch`, `freeze N`/`thaw`, `still`.

Crowd options: `looks=kset:<id>` (K <= 48 looks, reused: 150 citizens draw from
<= 48 images at 0 pool evictions), `n=1..256`, `form=lane|grid|scatter`,
`from=x,y` (lane) or `area=x,y,w,h`, `spacing`, `row`, `speed` (px/f), `dir`,
`ripple=0..R` (per-member clock lag, frames), `cycle=walkN`, `clock=<actor clock>`,
`layer=front`, `flip=mix`, `bob=on`, `special=<member>:<char>,...` (never moves).

Mouths: the AUDIO lane's mouth tracks (spec §9.3/§10.2), one viseme letter per
frame from `XMCEAOUF` (X = rest), mapped through `visemes.shw`; tracks for `expr`
(EXPR_* per frame) and `blink` (`1` shut, `0` open, `.` the clock's own blink)
use the same TRAK records. Crowds never flap.

Tests: `python3 -m pytest show/tests/engine/test_wp6.py` (AT6.1-AT6.5, dx_proof;
one PS1 test build + one emulator run, ~2 min). Sheets in
`<scratch>/show_out/engine/{model_sheets,freeze,crowd_lanes,crowd_klook,blink_patch_f,dx_proof}/`.
