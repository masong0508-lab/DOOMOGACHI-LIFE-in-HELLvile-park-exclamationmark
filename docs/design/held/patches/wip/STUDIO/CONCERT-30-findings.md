# CONCERT-30 findings (running file, 2026-11-15)

Machine: libmgba harness, `gba/harness/scripts/concert_profile.txt`, `--worklog`, tip 2c948ea5 + SPEED-3's s4-c.mbox.
Drawn fps / work avg ms / max ms.

| window | baseline (tip + s4-c) | after CLITE_LEAD |
|---|---|---|
| solo (Sugar Bowl) | 30.0 / 29.1 / 32.3 | 30.0 / 27.0 / 30.2 |
| band | 20.8 / 38.1 / 41.8 | 23.2 / 36.0 / 39.5 |
| choir 12 | 29.9 / 30.3 / 34.7 | 29.9 / 30.3 / 35.4 (unchanged: the lead is not drawn by this path) |
| setup | 30.0 / 7.5 / 9.0 | 30.0 / 7.5 / 9.0 |

## What each change bought
- s4-c.mbox applied (silhouette loops, exact): in the baseline above.
- Experiment, not kept: holding the lead's figure to one still pose (`portrait_hold(1)`) saves only 1.3 ms
  (38.1 -> 36.8): the "cache thrash" of her phases is a small part of the band's cost.
- CLITE_LEAD (exact): the lead's figure and shadow as one baked image (49x81 in the choir's mini buffers, made again
  when her image uid changes): -2.1 ms on the band and the solo. Native has a soft shadow (colormap) unlike the GBA's
  black one, so under CLITE_LEAD the fallback draws the GBA's shadow spelled out (silhouette + image) and the test
  compares baked against that.

## Left (band needs another ~3-6 ms, max 39.5 -> <33)
Stage 10-12.5 ms, crowd 9 ms (back silhouettes 3.6, front chibis 3.4), band members 6.4 ms (instruments as kept
pictures -3 ms), karaoke + HUD 2 ms. The Dreamland stage is 3-5 ms over the others: not profiled yet.

## Gates (2026-11-15, on 33f8a583 + guard commit)
core test 1534/0; SAN=1 1534/0 (4 old reports, none in the new code); check-gen ok; xcheck 434 files ok; make -C gba test ok;
gba sizes: .bss 210,508 B (EWRAM unchanged), .iwram 11,392 B (+104 B from s4-c), overlays unchanged; dc-host-test ok;
PS1: 110,352 B below the stack top with the boot FMV (the lead bake is compiled out there), perf_concert 29.88, game_smoke PASS.
Not run: make -C dc (KOS build, the change is compiled out on DC), test-speed2 concert groups, a frame-hash proof on the ROM
(two ROMs of different speed do not share ticks; the proof is the unit test against the two-blit reference).

## Re-measure and instrument experiments (2026-11-18, on ea6384c6; nothing changed in the code)

Purpose: find out what the band's remaining ~8 ms actually is before writing the next step. Documentation only: no
source file was changed, every experiment below was a scratch build that was thrown away.

### How to reproduce on a plain Ubuntu box (no devkitPro, no apt libmgba)
* `apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi cmake libpng-dev zlib1g-dev` (`libmgba-dev` is not in
  Ubuntu noble's archive).
* libmgba 0.10.3 from source (`git clone --branch 0.10.3 https://github.com/mgba-emu/mgba`), cmake with
  `-DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_GL=OFF -DBUILD_GLES2=OFF -DBUILD_GLES3=OFF -DBUILD_LIBRETRO=OFF
  -DBUILD_TEST=OFF -DBUILD_EXAMPLE=OFF -DBUILD_PYTHON=OFF -DUSE_LIBZIP=OFF -DUSE_MINIZIP=OFF -DUSE_FFMPEG=OFF
  -DUSE_LUA=OFF -DUSE_EDIT=OFF -DUSE_SQLITE3=OFF -DUSE_ELF=OFF -DUSE_DEBUGGERS=OFF -DCMAKE_BUILD_TYPE=Release`
  (libzip's cmake config wants `/usr/bin/zipcmp`, so it is switched off), `make mgba`, install the library and
  headers under /usr/local, `ldconfig`.
* `make -C gba rom harness`, then `gba/build/harness gba/build/doomogachi.gba --script
  gba/harness/scripts/concert_profile.txt --shots DIR --no-fps-gates --quiet --worklog`. A cold full build is about
  15 minutes on one core; a rebuild after editing `concert_gfx.c` about one minute.
* Gotchas: a build started from a tool call that times out is killed with it (start it with `setsid nohup`; the
  objects already built are kept). `concert_profile.txt` stops being useful after the band window for this
  purpose, so cut it after `fps concert band` for quick runs (the band's `scene CONCERT -6013` does not depend on the
  solo window before it).

### Baseline on this tree (libmgba 0.10.3; drawn fps / work avg / max ms)
| window | this run | CONCERT-30 table above |
|---|---|---|
| solo | 29.5 / 29.7 / 38.8 | 30.0 / 27.0 / 30.2 |
| band | 18.8 / 41.0 / 58.5 (second run 41.4 / 56.2) | 23.2 / 36.0 / 39.5 |
| choir 12 | 30.0 / 30.4 / 34.7 | 29.9 / 30.3 / 35.4 |
| setup | 30.0 / 7.5 / 9.0 | 30.0 / 7.5 / 9.0 |
The choir and setup match the earlier table to 0.1 ms, so the emulator itself is comparable; the solo and the band are
3-5 ms slower than recorded. Either the tree gained work since those numbers (the Option C / studio work landed
after them) or the libmgba build differs from the CI one. Compare runs from the same machine only. Logic stays at
30.0 ticks/s in every window; the band's loss is in drawn frames (every tick over 33.4 ms costs a video frame pair).

### Where the band's time sits (profile of 6,000,000 instructions, band window, 8.7 % outside known symbols)
vid_blit 10.4 %, face.constprop.0 7.5 %, plat_stage_restore 6.5 %, vid_rect_narrow 5.1 %, vid_checker_ui 4.1 %,
bodyspr_image 2.8 %, plat_keep_rows 2.4 %, draw_show_part 2.1 %, scan_row 1.8 %, gba_udivmod 1.7 %, finish_marked
1.7 %, vid_rect 1.7 %, out_rect 1.7 %, plat_line_m4 1.6 %, then a tail under 1.5 % each. It is flat: no function
is worth 8 ms. The UI-group loops show under other names at the same overlay slot addresses (section 3 of GBA.md), so
`vid_rect_narrow`, `vid_checker_ui` and friends are shared by the stage, the crowd and the instruments.

### Experiment: take the band's instruments out (scratch build, `if (0)` before the four calls in
### `concert_draw_performers`; everything else unchanged)
| variant | band drawn fps | work avg | max |
|---|---|---|---|
| as is (full script) | 18.8 | 41.0 | 58.5 |
| drum kit, keyboard, guitar and bass all off (full script) | 27.4 | 33.6 | 51.5 |
| drum kit off (band-only script) | 19.2 | 39.5 | 57.8 |
| keyboard off (band-only script) | 18.5 | 41.1 | 57.1 |
| guitar and bass off (band-only script) | 20.2 | 38.5 | 57.8 |
Reading it:
* All four instruments together are about 7.4 ms of the band's tick, more than the 3 ms the earlier note guessed.
* The keyboard costs nothing measurable (it is a few rects); the guitars are the largest single item (about 2.5 ms),
  the drum kit about 1.5 ms. The three single-item rows add up to about 4 ms, not 7.4: they come from the short
  script and I did not take a band-only baseline, so use them for the order (guitars > drums > keyboard), not for
  the sums. Re-run all five on the band-only script before relying on a number.
* The worst tick stays at 51-58 ms in all five runs. Removing the instruments moves the average, not the spikes:
  the 33.3 ms target needs the spikes found too (the lead's portrait job phases, `bodyspr_image` / `face.constprop.0`,
  and the half-rate stage rows' second half are the first suspects; the earlier table's `max` was 39.5 ms).

### What the instruments are, and what would fix them (not built)
* `drum_kit`, `guitar` (twice: guitar and bass), `keyboard` in `core/src/concert_gfx.c` draw every tick with
  `plat_rect`, `plat_line`, `ui_fill_ellipse`, `ui_fill_circle` and `ui_heart_flat`. The guitar body is three
  filled shapes in two colours plus two small rects; the drum kit is two cymbal rects, two stand lines, two toms
  (one colour), two nested circles and a heart. `ui_fill_ellipse` already draws one `out_rect` per distinct
  half-width (`nested_bands`), so the cost is the number of calls, not a divide.
* Painter's order inside each instrument matters (cymbals and stands first, then the toms and the drum, then the
  heart; on the guitar the neck and head first, then the body shapes, then the two rects). A baked picture of only
  the filled shapes, blitted at the right point in that order, keeps every pixel.
* Cheapest exact route to try first: the dream scene's `dream_ellipses` / `dream_circles` (declared in
  `concert_int.h`; same pixels as `ui_fill_ellipse` / `ui_fill_circle`, one `plat_hshapes` ARM call per up to 16
  shapes on the GBA). The drum's two toms are one colour and batch into one call; the rest are single calls.
* Second route, if that is not enough: a kept picture per instrument state. The body of the guitars does not depend
  on `strum` (only the neck and head do), so one small image per colour covers both states; the drum's toms and drum
  are static, only the cymbal offsets and the heart's size change. The band has free scratch for this: `v->minis`
  (16 x 392 B of ChoirMini) is only used by a choir, and the lead's bake takes 49 x 81 + 8 = 3,977 B of it, so about
  2.3 KB is unused in solo and band mode. `ConcertState` is 7,324 B of its 12 KB.
* Either route must keep `core/tests/test_concert_gfx.c` green: 576 stage frames against
  `core/tests/golden/concert_frames.txt`.

### Already in, so not a lever
The 15 Hz LED wall / lamps / beams (`CLITE_HALF`, `keep_half`) and the six-flame wall (`CLITE_FLAMES`) are the owner's
approved trades and are already on for the GBA band. The kept rows live in the OBJ VRAM (16,320 B, `OBJV_KEEP`),
which is why crowd rows as OBJ sprites needs that space freed first.

### Order I would take (each step measured on the band-only script, same machine, before and after)
1. Take a band-only baseline; re-run the five rows above.
2. Guitars and drums through `dream_ellipses` / `dream_circles`; golden frames; measure.
3. If still over: kept pictures for the guitar bodies and the drum's toms and drum, in the unused part of `minis`.
4. Find the spikes (the 51-58 ms ticks) with `--worklog`; the average will be near 33 ms after step 2-3, the max is
   what the drawn fps shows.
5. Crowd rows as OBJ sprites (about 7 ms) only if 2-4 do not get there, because it needs the OBJ VRAM budget redone.

## CI check on f3fcfb1 (2026-11-19, documentation only)
Concert solo in the CI speed tour: 29.7 fps, work avg 29.2 / max 43.7 ms; closed curtain 29.2 fps (worst tick 76.2 ms).
The band is not in the speed tour, so the band numbers above are still the latest. The whole CI table, the Music
Studio tabs, the Hell Breach table and a PS1 margin drop to 76,640 B are in
`../SPEED/CI-f3fcfb1-speed-report-2026-11-19.md`.
