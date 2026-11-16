# Resume guide (paused 2026-11-15 ~20:40 UTC: the weekly allowance hit 98%)

Everything unlanded is in this repository. Nothing important lives only in a chat or on a machine.
Each stream's status file (`status/<STREAM>.md`) says what landed and how that stream resumes; the saved
work is in `patches/wip/<STREAM>/` (apply `*.patch` / `*.mbox` with `git am -3` in a fresh worktree off the
tip, then `git apply -3` any `uncommitted.diff`; a README there says more where present).

## How to resume (one step per chat is fine)
1. New machine: `sh tools/dev_setup.sh`; check `make -C core -j8 test` ends "N passed, 0 failed".
2. Read `PARALLEL_WAVES.md` (streams, file ownership, memory shares in section 3, landing rules in
   section 4 incl. 3b / 3c and `LANDING_HOLD`), `OWNER_DECISIONS.md` (latest wins), this file.
3. Start one lead session per stream with its brief (`leads/<STREAM>.md`), or work the list below by hand.
   Land with `tools/agents/wave-runner.js`: gate once, short push loop, stop with `needs-window` after 4
   rejected pushes and open a landing window (`LANDING_HOLD`) instead of looping.
4. Owner rules that bind everything: EVERYTHING AT 30 (no screen done below a steady 30 fps, no frame over
   33 ms), TEXT FITS EVERYWHERE, the hard content rules of CLAUDE.md.

## What is left, per stream (saved work in parentheses)

* **SPEED** (`wip/SPEED/`: `mem2`, `s3-12`, `sa4`)
  - MEM2: grow the PS1 / GBA reserves (targets PS1 +24 KB, EWRAM +12 KB, IWRAM +1 KB); then hand the freed
    bytes out in PARALLEL_WAVES section 3 (reserves now: PS1 about 1.9 KB, GBA EWRAM about 3.8 KB).
  - S3-12: big outlined text made once (chapter card 4 22.7 fps, simmer, Fun Fair title cards).
  - SA4: speed audit 4 (partial: GBA tours profiled) -> `docs/design/SPEED_AUDIT4.md` with an owner per
    window under 30 and the combat baseline; then send each owner its list and "AUDIT4 DONE" to COMBAT.
  - Known windows under 30 on the GBA: exorcism prayer 27.4 and true name; concert band 20.5 and the
    Dreamland stage 25-27.5; kitchen result card 20.8, counter / serving 29.2-29.3, step changes; the Giant
    Plush dream 23.6, Tea Under the Sea 28.5; the Prize Wheel card; credits parade; menus within 3 ms.
* **SPEED-2** (`wip/SPEED-2/`: `S3-TALK`, `S3-7`, `S3-8`, `S3-CREDITS`, `S3-15`): the Talk screen at 30 on the
  GBA (15.5-17.5 fps, a 408 ms hitch), fog and cutscene skies, library lobby / jukebox / level select,
  credits parade, PS1 scene changes without the drive wait; then S3-14 (island hub) and the exorcism
  prayer / true name with the approved trade-offs.
* **SPEED-3** (`wip/SPEED-3/`: `s4-c`, `s4-k-k2`, `s4-w`): the concert band and Dreamland, the kitchen leftovers
  (result card, counter, serving, step changes), the Prize Wheel card; then audit 4's list.
* **TALK** (`wip/TALK/`: `t8a`, `tm3`): the multilevel composer (TM3 and on), replies that carry actions
  (TM2e), the full census, Talk on the PS1 (TKP1-TKP6, PS1 share 12 KB), the Talk screen cleanup; the line
  bank stays frozen.
* **CREATOR** (`wip/CREATOR/`: `fut-cr4-a6`, `a7`, `a8`, `fut-cr-ui`): land CR4-A6 (55 hats and glasses), A7,
  A8 (59 held props) and CR-UI step 1 (they need a landing window); then the personality page CR-PERS
  (Tomodachi sliders + Sims traits + the live Talk preview), the slider-range cheat x1-x4, the rest of the
  art batches toward 1,500+ parts.
* **HOUSEHOLDS** (`wip/HOUSEHOLDS/`: `hh-data`): HH-DATA, HH-CREATOR (the household view in the creator,
  replacing the add-member menu), HH-REL (Create-a-Sim relationship editor), HH-PETS (Boreagotchi families,
  up to 8 each, a town pool of 48), HH-USE. Plan: `docs/design/HOUSEHOLDS_PLAN.md`.
* **DC** (`wip/DC/`: `fut-dc9d.mbox`, `BASES.txt`): DC9 (the full game on the Dreamcast) from DC9D on, the
  DC6D leftover ticks, TD (Talk on the Dreamcast) follow-ups.
* **LEVELS** (`wip/LEVELS/`: `fut-lv6a`, `fut-lv7b`): LV5 (mechanic kit C), LV6 (enemies, AI, bosses), LV7b
  (demon sprites, banks, songs and SFX).
* **LEVELS-2** (`wip/LEVELS-2/`: `lv8`, `lv9`): LV8 (platform level packs), LV9 (combat detail per level).
* **LEVELS-3** (started 20:17, check `wip/LEVELS-3/` and status/LEVELS.md): the 24 new levels: LV12-LV20
  (main game), LV21 (their story hooks), LV22-LV37 (New Game+), LV38 (integration). None is built yet.
* **STUDIO** (`wip/STUDIO/`: `st-play.mbox`; ST-EASY landed 2026-11-15): ST-EASY, ST-PLAY, then ST-TOOLS, ST-ADV, ST-USE
  and all seven helper tools (`docs/design/STUDIO_PLAN.md`).
* **UI** (`wip/UI/`: `ui-fit`, `ui-options`, `RESUME.md`): the text-fit test, the Options rebuild, then every
  menu decluttered at 30 (`docs/design/UI_PLAN.md`).
* **COMBAT** (started 20:00, measuring kit landed; check `wip/COMBAT/` and status/COMBAT.md): the baseline,
  `docs/design/COMBAT_3D_PLAN.md`, then the GBA 3D renderer work after SA4.
* **Show** (queued, not started): the main title redo, `docs/show/INTRO_REDO.md`.

## First thing after the reset (owner, 2026-11-15)
"Make sure all of the concert and music creator screens are a hundred percent full speed 30 frames."
Every studio tab (SONG incl. a made song playing: 28.8 fps on the GBA; STAGE 22.6; NOTES, ORDER, LYRICS,
CHOIR, PERFORM, FILE, the Start card, jam mode and drum pads) and every concert window (the band 20.5, the
Dreamland stage 25-27.5, solos, choirs, setup, the notes show) must hold a steady 30 fps with no frame
over 33 ms on GBA, PS1 and Dreamcast. Owners: STUDIO for the studio tabs (its next package before ST-TOOLS),
SPEED-3 for the concert (its saved `s4-c`). Exact steps first, the approved trade-offs last; measured with
the studio and concert speed2 windows before and after.

## Owner questions still open
The levels plan section 10 (NG+ Chapter 5 and endings, W15 Bonejangles' origin, memorial names default,
the NG+ cold open); "huggle" meaning (INTRO_REDO.md default); the GBA rent default from GBA-500; whether
to remove the concept art (12 MB) and the trailer video (6.8 MB) from the repository.

## Stopped at 100% (2026-11-16 ~00:00 UTC)
STUDIO-30 was in review / landing: saved in patches/wip/STUDIO/studio-30/ (STAGE tab 22.5 -> 30.0, a made song playing 30.0, the take-over-a-song undo, a speed2 window per tab, PS1 +0). Apply with git am -3 and land it first. CONCERT-30 landed partial (band 23.2 fps; what is left is in its status line and findings).

## Studio: exact next steps (written 2026-11-16 ~00:10 UTC, allowance at 100%)
1. **STUDIO-30: LANDED 2026-11-16** (STAGE tab 22.5 -> 30.0, a made song playing 30.0, undo of a take over a
   composed song, a speed2 window per tab, PS1 resident +0).
2. **ST-TOOLS** (not started: no commits): the note grid edits, per-track instrument / envelope / volume / pan,
   swing, chord track, drum pattern editor, then the seven helper tools one by one (chord suggester, melody
   helper, groove picker, lyric syllable fitter, choir auto-harmony, section copier, mix helper).
3. **ST-ADV** (partly built, 2026-11-16: format, player, save, ORDER arrangement, Mix, Expert view, Note grid with Box, Sound / Brightness playback; see `status/STUDIO.md`; left (how to build each: docs/STUDIO.md "ST-ADV: what is left"): pan playback, patterns of 3 and 4 bars, GBA / PS1 / DC builds and sizes): the Advanced switch for full songs (sections, key / tempo changes, automation,
   more tracks, several minutes, lyrics and choir across the song; versioned format, old songs load).
4. **ST-USE**: user songs in the Concert Hall and PERFORM; the studio look decluttered.
5. **Concert to 30**: CONCERT-30 landed partial (band 23.2 fps); what is left with measured costs is in
   `patches/wip/STUDIO/CONCERT-30-findings.md` (instruments as kept pictures ~3 ms, crowd rows as OBJ sprites
   ~7 ms (OBJ VRAM is full: free it first), the approved 15 Hz / flame trades last; Dreamland and Disco stages
   25-27.6; the choir's 35.4 ms peak).
Run each with `tools/agents/wave-runner.js` (stream STUDIO; `"model": "sonnet"` keeps the cost down).
