# Comic timing engine

The game borrows the trailer's elastic, frame-exact comedy (`trailer/`) through
one small engine that every scene shares: cutscenes, dialogue, the overworld,
the house, combat captions. Package S1 of `docs/design/story/SPEC.md` built it;
`docs/design/story/comic_timing.md` (CT) is the timing bible it implements.

**The one rule:** timing lives in data that hangs off named beats, never in
frame constants scattered through update code. The trailer's `music.c` had to
repeat every scene's frame numbers (`FR(n)`); the game does not. Re-time a
beat and its sound, shake and caption move with it.

## The pieces

| Module | Header | What it does | Where it lives on the PS1 |
|---|---|---|---|
| `tween.c`, `tween_tables.c` | `tween.h` | curves, the trailer's keyframe tables (`TW_*`, CT Appendix A), squash, springs | resident |
| `beat.c` | `beat.h` | the beat scheduler: events hang off labels, A moves anchors only | resident |
| `camfx.c` | `camfx.h` | shake profiles (CT Appendix C), punch, zoom, flash, fade, iris, palette lerps | resident |
| `audio_fx.c` | `audio.h` | mute, duck, tape-stop, resume on the beat, dual music, the synth comedy kit | resident |
| `ui_dialog.c` | `ui.h` | dialogue beat codes, punctuation holds, glyph hop, portraits, the name pill | resident |
| `cutscene.c` | `scene.h` | cutscene stages on beat scripts; the timing showcase | `cutscene` group |
| `overworld*.c` | `overworld_int.h` | NPC acts, the camera field, the border scratch, the gag spawner, wrongness | `story` group |
| `gba/src/postfx.c`, `obj.c`; `ps1/src/postfx.c`, `xform.c` | `platform.h` | hardware post-FX and squash | platform |

Resident code never calls overlay code (SPEC C6): `beat_tick()` hands the
host kinds (`SAY`, `CAPTION`, `SHOT`, `NPC`, `CAM`, `IRIS`, ...) back as
`BT_EXT`, and the scene that runs the script handles them in its own code.

## Beats

A `BeatScript` is a ROM array of 8-byte `BeatEv {at, anchor, op, a, b}`,
sorted by `(anchor, at)` (`beat_script_valid`). An event fires `at` frames
after its anchor label fired. Labels fire on a schedule (`BOP_LABEL`), after
an unskippable hold (`BOP_HOLD`), or on A (`BOP_WAIT_A`, whose `b` is
`BEAT_WAIT_A_B(min, auto)`: A is accepted after `min` frames and the label
fires by itself after `auto` frames). `beat_age(label)` drives every entrance
in draw code: `tw_tab(&TW_SLAM_STAMP, beat_age(L_FAIL1))`.

Reserved labels: `L_PUNCH` (30) marks the punch; `L_BUTTON` (31), 12-25 frames
later, is the after-beat, or the script carries `BOP_NOBUTTON`. A never
shortens a hold after `L_PUNCH`. START skips the whole script and applies only
its story flags.

The ladder (CT sec.2): gaps sit on `RUNG_R2, R3, R5, R8, R13, R21, R26` (+-1).

## Cutscenes (stages)

Each `CUT_*` is a stage (the vignette's drawing code, which reads the current
shot and `beat_age`) plus a beat script: `SHOT` switches the shot, `CAPTION`
the caption, and every sound, shake, flash and iris is a row. The Wall's
impact is an `L_PUNCH` that is never undercut; the eruption gets an R13
silence before its FWOOMP; Vapula's deflation tape-stops the victory theme.
`CUT_FEAST_POP` keeps `story_feast.c`'s art and exact panel lengths.

`SCENE_CUTSCENE` args 256-258 are the **timing showcase**: the trailer's s04
pet turn (a 4-frame hell insert, back with horns: "beep." "...it's fine."),
the s12 census (three slams, then the cork) and the s14 handheld DROP and
deadpan. `core/tests/scripts/timing_showcase.txt` shoots their key frames for
comparison with `make -C trailer` frames (`build/trailer --png s04 134 x.png`).

## Dialogue

`ui_say(id, text)` and `ui_say_ex(id, EXPR_*, DL_* flags, text)`. Codes are
parsed when the line is queued and stripped from the 200-character entry:

| Code | Effect |
|---|---|
| `{p N}` `{.}` `{..}` `{...}` | pause N / R5 / R8 / R13 frames |
| `{d}` | deadpan for the rest of the line: it appears whole, no hop, no babble, the portrait holds, the music ducks to 0.5 |
| `{!}` | a sting: `SHK_POW`, a "!" over the speaker, a 1-frame flash |
| `{big}word{/}` | the word pops whole (`TW_POP_BUBBLE`) |
| `{w}` | the rest of the line waves |
| `{r}` | record scratch: tape-stop 5, 14 frames of silence, back on the beat |
| `{cut N}` | a 4-frame smash flash; the host draws insert N (`ui_dialog_insert`) |
| `{punch}` | a punchline: A cannot rush it; it ends on an unskippable R13 |

Punctuation holds: `,` 3, `.` 6, `!` 4 (the box hops), `?` 5 (the portrait
tilts), `...` 5/5/5 and R8 before a line break. Story lines also take `/`
(line break) and `//` (page break). Flags: `DL_DEADPAN`, `DL_PUNCH`,
`DL_PILL` (the name pill, which pops in, is drawn only on these lines),
`DL_SHOUT` (the portrait stretches 0.92/1.10, then jellies), `DL_HEART`
(sincere; the pet may "beep."), `DL_GURGLE` (Grumbleguts' belly speaks from
its own little slot, with the `VOICE_GURGLE` babble). Possessed speakers
twitch. Every new line starts with a talk bob. Legacy `ui_say` lines keep
the name plate.

## Audio

`audio_music_mute(n)` gates the music for exactly n ticks while the sequencer
keeps running, so it re-enters on the grid. `audio_music_duck(gain, n)`,
`audio_tapestop(n)` (-15 semitones over n frames, then silence until a new
song or `audio_music_resume_on_beat()`, which waits for the next quarter
note), `audio_music_dual(a, b)` (Ep 4.6). Sound effects are never muted.

The comedy kit is 15 synth one-shots (`SFX_SCRATCH .. SFX_CREAK`, ids from
`SFX_SYNTH_FIRST`): segment tables of at most 12 frames played on core tone
channels 2 (tones) and 3 (noise), borrowed from the music like the voice
blip borrows channel 1. They cost no PCM (the PCM budget is full, SPEC C11).
`audio_sfx_pitch(sfx, semis)` steps the kazoo down.

## The overworld

* **NPC acts** (`ownpc_act(id, NACT_*, arg)`, `BOP_NPC`, the VM's `OP_NPC`):
  `WALK_TO`, `HOP`, `TURN`, `FACE`, `EMOTE`, `FLEE` (the `2a + 0.35a^2` run,
  staggered by `id % 7`, sweating), `FREEZE`/`THAW`/`PANIC`/`STILL`/`TWITCH`,
  `HEAD_TURN` (the creak), `RIPPLE` (neighbours bang "!" 3 frames apart) and
  `DOUBLE_TAKE`. Eight script slots live in the overworld's scene scratch.
* **The double-take**: an ambient citizen you pass within 2 cells may stop
  R3, turn, bang "!" with a tik. Once per citizen per visit.
* **The camera field** (`ow_cam(CAM_*, arg)`, `BOP_CAM`): `PAN_TO` (sit, then
  a 12-frame smoothstep move), `FOLLOW`, `BRAKE` (`TW_BRAKE` overshoot +
  `SHK_BRAKE`), `CREEP` (dread), `ZOOM` (a snap under a flash), `RESET`.
* **The border record scratch**: walking from Hillvile into Hellvile
  tape-stops the music, holds 14 frames of silence, brakes the camera and
  brings the Hellvile theme in on the downbeat.
* **The gag spawner** (`overworld_spawn.c`): a weighted ROM table
  `{gag, weight, cooldown_min, chapter_mask, night_only}`; one gag per
  20-40 s of walking, none for 5-15 s after a big beat (`ow_spawn_quiet`),
  none during a beat script or a conversation. The starer (stand still R21
  and they wave), the crowned cloud, the pet's "beep." on a sincere line,
  the Days Since Last Fire board flipping 7, -, 0 by Rivet's, and
  Grumbleguts backing into frame in Hellvile.
* **Wrongness** (W1-W16) is the same mechanism with "noticed" bits
  (`SF_WRONG(w)`): each row fires once, `delay` frames late, with a sound, a
  one-frame palette lerp, a deadpan caption or a VM script. The rows are
  story content (`doomo_ow_wrong_rows`).
* `ow_scratch_ext()`: 512 bytes for scripted overworld scenes (SPEC R17).

## Safety and options (CT sec.9)

`OPT_SCREEN_SHAKE`, `OPT_REDUCE_FLASH` and `OPT_UI_BOUNCE` scale amplitudes
only, never timing: the same frames are marked at every setting (tests:
`test_camfx`, `test_dialog_codes`). Full-screen flashes stay under 3 Hz.

## Tests

`test_tween.c` (the trailer curves), `test_beat.c` (scheduler, cutscene
scripts, the showcase's punch-to-button gaps), `test_camfx.c`,
`test_audio_fx.c`, `test_dialog_codes.c`, `test_ow_script.c`, and the runner
script `timing_showcase.txt` (both resolutions).
