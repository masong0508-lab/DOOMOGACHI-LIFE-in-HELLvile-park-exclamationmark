# Exorcism speed (sub-roadmap, SP7 = G-E0 / G-E1 of the week-2 speed audit)

Goal (owner, 2026-11-08): a steady 30 fps on every screen. `SCENE_EXORCISM` (Father Fuzz's ritual at the Church of St.
Cuddles: `core/src/exorcism_scene.c`, `exorcism_gfx.c`) was the slowest screen on the GBA in the week-2 audit (8.0 drawn
fps, 14.1 logic ticks/s: the game ran at half speed) and missed 30 on the PS1 (23.8 fps). This file is E0 (the baseline
and where the time goes), E1 (the exact fixes), E2 (the kept backdrop and the retained stage) and what is still missing.

Measure with:

```
make -C gba profile-exorcism        # gba/harness/scripts/exorcism_profile.txt: every phase, fps + work + profile + vhash
python3 ps1/harness/run.py --exe ps1/build/doomogachi.exe --cue ps1/build/doomogachi.cue \
    --script ps1/harness/scripts/perf_exorcism_phases.txt --out OUT        # the same phases on the PS1
core/build/tests stage_             # the retained stage against the full redraw, frame for frame (native)
make -C gba test-exorcism           # (G2-EX) gba/harness/scripts/exorcism_fps.txt: every phase with an fps floor
```

Both scripts walk the practice ritual on the plush dummy (`scene EXORCISM -1`) like `core/tests/scripts/life_exorcism.txt`:
the intro under its dialogue, the tools menu, the prayer rhythm, the holy water (reticle held at the left edge, so the phase
ends when the flasks are thrown), the true name, the verdict and the result. Frame parity between two GBA ROMs: build both
with `make -C gba rom PSLICE_DET=1`, run `gba/harness/scripts/exorcism_parity.txt` (every tick of the ritual hashed) on
each and compare with `gba/tools/speedtable.py --compare` (the script's header has the commands).

## E0: baseline (2026-11-08, tree 7729ada)

GBA (libmgba 0.10.2 harness, `make -C gba profile-exorcism`), drawn fps / logic ticks/s / CPU work of a drawn tick:

| phase | drawn fps | logic t/s | work avg / max ms |
|---|---:|---:|---|
| intro (dialogue up) | 7.9 | 14.0 | 100.5 / 121.2 |
| tools menu | 7.9 | 14.0 | 100.2 / 118.8 |
| prayer rhythm | 7.4 | 13.0 | 106.1 / 129.4 |
| holy water | 8.2 | 14.3 | 97.0 / 115.9 |
| true name | 7.2 | 12.1 | 106.8 / 126.5 |
| verdict | 7.9 | 13.5 | 103.5 / 126.4 |
| result | 8.2 | 14.2 | 95.9 / 114.8 |

PS1 (PCSX-Redux, `perf_exorcism_phases.txt`; one logic tick per presented frame, so fps is also the game speed): intro
23.8, tools 26.9, prayer 22.6, holy water 24.7, true name 26.9, verdict 29.3, result 29.7.

Where the GBA time went (harness `profile`, tools phase): `vid_rect_narrow` 10.5%, `plat_line_m4` 9.0%, `curtain` 8.1%,
`gba_udivmod` 7.7% + `__aeabi_idiv` 4.3% + `__divsi3` 1.3%, `vid_rect` 4.7%, `vid_blit` 3.7%, `exo_col` 2.8%, then the
portrait renders (`pc_finish_lines`, `capsule_plain`, `scan_transitions`, `sh_span`). Split by experiment (temporary ROMs,
not in the tree):

| what was taken out | tools work ms | |
|---|---:|---|
| nothing | 100.2 | |
| the church (one full-screen rect instead) | 32.3 | the church costs ~68 ms of rects, lines and circles |
| everything the scene draws (one rect) | 2.4 | logic, audio, input: the floor |
| + the host's frame back | +14 (water, scale 2: +19) | the biggest foreground item |
| + the tools window back | +11 | |
| + the prayer lane back | +10 | |
| + Father Fuzz / the demon / the candles back | +4 / +2.7 / +1.2 | |

So 30 fps needed both: the church nearly free, and the foreground cheaper or drawn less often. The PS1 profile pointed at
the same church code (`rect_slow` 11.9%, the scene's own overlay code 10.6%: the curtain loops with their divides).

## E1: the exact fixes (landed, frames identical)

- The curtain asked `drape_w()` for every row of every fold: two divides each, ~2,400 calls of the GBA's software divider a
  frame. The widths are a band table now (`drape_bands`: a band k = y * 16 / h shares one width), and a fold's runs are
  walked band by band, not row by row. The carpet's half width is stepped per row instead of divided.
- GBA frames identical to the base ROM (the `vhash-seq` of all six windows); native `life_exorcism` shots identical at
  240x160 and 320x240; the band table checked against the old formula for every w <= 60, h <= 480.
- GBA: tools 100.2 -> 85.8 ms, prayer 106.1 -> 94.7, water 97.0 -> 81.3. PS1: intro 23.8 -> 28.0 fps, prayer 22.6 ->
  26.4, tools 29.8, water 29.0, name 29.8, verdict / result 30.0.

## E2: the kept backdrop and the retained stage (landed, frames identical)

**The church as a backdrop baked into the ROM.** Everything of the church that does not move every few ticks is a function
of a small key (`exo_church_key`: the dread level 0..15, the drips' step at dread >= 9, the failing beams at dread >= 13: 59
keys). `exo_church_backdrop(key)` paints it; the moving parts are drawn over it (`exo_church_part`): the heart / eye in the
rose window, the swaying lower halves of the curtains (`curtain(..., CUR_LOWER)`), the seven dust motes. Each of them lies
where nothing the church draws later in the old order reaches (above the beams, below the valance and the vignette, beside
the altar and the pews), except a mote on the altar: a mote leaves out the pixels the altar's outline covers (the outline's
rects captured once with `ur_tap`), which is what the altar painted over before. So backdrop + parts = the old church, pixel
for pixel.

The GBA build bakes every key: `make -C gba` builds the native runner from the core in `gba/build/native` (host gcc) and
runs `runner --w 240 --h 160 --bake-backdrops build/gen/backdrops.bin` (`core/native/stage_native.c`: the core's own
drawing code, so the pictures cannot drift from it); `gba/src/backdrop_blob.s` links the file (2,266,084 B; the cartridge
is 32 MB). `plat_stage_begin` (`plat_ext.h`, `gba/src/stage.c`) answers from it, and `plat_stage_restore` copies a box of
the picture back with DMA: whole rows 16 at a time in one 32-bit DMA (~4 ms for the screen), partial rows from IWRAM ARM
code (`stage_rows`, in the IWRAM overlay's room group, which no stage scene uses).

**The retained stage** (`core/include/doomo/backdrop.h`, `core/src/backdrop.c`). `exo_draw` runs its layers twice: pass 1
declares every layer's box and key (and fetches the portrait images, once, in the old order), pass 2 draws the layers the
stage asks for. Per frame buffer (the GBA's two Mode 4 pages) the stage keeps the boxes and keys of the last frame drawn
into it; a frame drawn into a buffer that holds one of them puts the backdrop back on the old and new boxes of the layers
that changed and on the box of what was drawn over that frame after the stage (the dialogue, toasts, hints: the platform
tracks it, `GBA_NODRAW_TRACK` in `vid_route`, and a kept panel adds its rect), closes over the layers lying on those boxes,
and draws exactly those, in order. The dirty boxes are merged (a moving layer's old and new box) and cut into row bands,
so every pixel is put back once; when more than 85% of the screen is dirty it puts back the whole screen (whole rows are
the fastest copy). The records live in the exorcism's scene scratch (a cache: re-bound when another scene took it).
`core/tests/test_stage.c` plays the whole ritual (1,400 frames, 240x160 and 320x240) with the native platform emulating
the GBA's two pages, stage on against stage off: every frame identical; 310 of 1,400 frames (231 at 320x240) put back
the whole screen.

**Fewer pixels on a fill-bound screen** (GBA and native; PS1 and Dreamcast pay per primitive and draw the shapes whole):
the host's frame and the prayer ribbon are layers of shapes that paint over one another (aura, drop shadow, three round
rects, the inner panel; shadow, two pills). Each shape now leaves out the hole the next one is sure to cover (`ur_tap`:
every rect of a shape, clipped to the hole), and the drop shadows shade only the band the frame does not paint over. The
water phase's 2x frame lost about 90k of 120k pixel writes. The true-name window's clue lines and names are worked out
once per host and clue count (they were roster lookups on every frame: ~6 ms).

All of E2 is exact: the GBA's `vhash-seq` of every phase equals the base ROM's, and the native shots are identical.

## Results (2026-11-08)

GBA (libmgba, `make -C gba profile-exorcism`; logic ticks/s now 30 in every phase: the ritual runs at full speed, the
drawn frame rate is what is left):

| phase | base fps | now fps | base work ms | now work ms |
|---|---:|---:|---:|---:|
| intro (dialogue up) | 7.9 | **17.8** | 100.5 | 46.3 |
| tools menu | 7.9 | **20.4** | 100.2 | 41.3 |
| prayer rhythm | 7.4 | **15.4** | 106.1 | 52.0 |
| holy water | 8.2 | **21.1** | 97.0 | 40.4 |
| true name | 7.2 | **17.5** | 106.8 | 47.1 |
| verdict | 7.9 | **16.4** | 103.5 | 53.3 |
| result | 8.2 | **17.3** | 95.9 | 45.5 |

Logic ticks/s: 12.1-14.3 before, 29.9-30.8 now (the ritual's timing, the prayer's beat included, is right again). Max
work of a drawn tick 61-73 ms (was 115-129). `make -C gba test-speed` gained an exorcism row (the tools menu, last in the
tour, floor 15 fps).

PS1 (PCSX-Redux; the PS1 has no kept backdrop yet, so it gets E1 and the name cache):

| phase | base fps | now fps |
|---|---:|---:|
| intro (dialogue up) | 23.8 | **28.7** |
| tools menu | 26.9 | **30.0** |
| prayer rhythm | 22.6 | **27.1** |
| holy water | 24.7 | **29.3** |
| true name | 26.9 | **30.0** |
| verdict | 29.3 | **30.0** |
| result | 29.7 | **30.0** |

(PCSX-Redux charges about twice the cycles of a console for CPU-bound code, docs/PS1.md section 12: the 27-29 fps phases
are CPU-bound here and probably hold 30 on hardware; the last `perf:` line, in the result phase, reads cpu 19.8 ms a
frame, was 27.3 ms.)

## What was still missing for 30 on the GBA after SP7 (the gap, measured; G2-EX below closed most of it)

**GBA: 15-21 fps against 30.** What a drawn tick still pays (profiles of the final ROM, `make -C gba
profile-exorcism`):

- **the portraits' animation** (`pc_finish_lines`, `capsule_plain`, `scan_transitions`, `sh_span`, `hf_row`, ...: 12-16%,
  6-8 ms a tick): the possessed host and Father Fuzz are animated figures (blink, breath, twitch, four aura frames every 16
  ticks, the talking mouth), the 18 KB cache cannot hold their phase combinations, so a sliced re-render runs on almost
  every tick. Not this package's code (portrait.c, X1 / P-D1 of the audit); the levers are pixel-changing: a stand-in
  image while a phase renders (X1), or a slower aura cadence for the possessed host (DECISIONS: half-rate effects).
- **the figure blits** (`vid_blit` 7-12%): the host (48x80, 96x160 in the holy water), Fuzz (48x80), the demon, the
  dialogue portrait.
- **putting the backdrop back** (`plat_stage_restore` + `stage_rows` + `restore_dirty`, 8-15%): ROM is a 16-bit bus; a
  partial row costs its DMA set-up and edges.
- **big layers redrawn for small changes**: the stage can only put back the backdrop, so a layer that lies on a changed
  box is redrawn whole. The tools window (11 ms) changes with its cursor's bob and its glint every 2-3 ticks; the host
  frame is redrawn whenever the demon peeking over its edge bobs, and in the prayer and later phases whenever its aura
  pulses; the prayer lane (~8 ms) changes every tick by design (the prompts move).

Next steps, in the order they would pay:
1. **Clip-to-box redraw** on the GBA (a clip rect honoured by `vid_route`'s slow path, the stage drawing a layer under a
   changed one only inside the dirty box): the host frame under a bobbing demon would cost the demon's box, not the frame.
2. **Retained panels for the tools and true-name windows** (G7 `plat_panel_begin`: the body is copied from the front page
   while the cursor row is redrawn), as the dialogue box already is.
3. **Owner's call, pixel-changing (DECISIONS "SPEED DEFAULTS")**: the decorative animations (aura pulse, cursor bob,
   demon bob, window glint) at 15 Hz on the GBA; X1's stand-in portrait while a phase renders.
4. The demon as a hardware sprite over the bitmap where nothing is drawn over it (the bubbles and the dialogue are).

**PS1: 27.1-30 in PCSX-Redux.** The PS1 has no kept backdrop (the church would need a 320x240 16-bit VRAM area, which the
image cache uses; the stage's restore would be a VRAM-to-VRAM copy): E1 and the name cache brought every phase to 27-30.
What is left in the intro (28.7) is the dialogue portrait's renders, in the prayer (27.1) the lane's prims; both are
CPU-bound in Redux.

## RAM, ROM

- GBA EWRAM: 3,348 B free (was 3,540 before CRA's last count, 3,380 with the backdrop alone): `stage.c` 34 B, the altar's
  outline (100 B), `backdrop.c` 16 B. The stage's records and the name cache (~1.5 KB) live in the scene scratch.
- GBA IWRAM: `stage_rows` (~180 B) in the overlay's room group (the slot is sized by the 3D group: no new IWRAM).
- GBA ROM: +2.27 MB of baked backdrops (59 keys x 38,400 B), a few KB of code.
- PS1: resident +256 B (image +112, bss +144: the stage's state; `_end` 0x801ece90 -> 0x801ecf90, 76 KB left below
  the stack top); `backdrop.c` joined the exorcism's overlay group (`ps1/tools/ovl_plan.py`, with `exo_church_backdrop` /
  `exo_church_keys` as its modal callbacks), which grew 34 -> 46 KB (the split church, the layer code; the slot is 128 KB).
  No backdrop on the disc. The PS1's seven phase shots are identical to the base disc's; `game_smoke` and the core's
  `life_exorcism` pass on the harness with no `ovl: ERROR`.

## G2-EX (speed wave 2, built 2026-11-13, landed 2026-11-15): clipped redraw, held dims, the aura as a list, two held poses

Owner target: a steady 30 fps, logic 30, no frame over 33 ms, in every phase of the ritual. The work started from
audit 3 (`docs/design/SPEED_AUDIT3.md`, S3-11) and a fresh profile of each phase. The profile now names the
layer: the harness build marks which layer is drawing (a temporary `exo_mark` byte that a local copy of the
harness reads; not in the tree), so the cost of every layer and of the stage itself is a number per phase.

**What the profile said (SP7 tree, ms of a drawn GBA frame):** the stage's own work (restoring the backdrop, its
bookkeeping) 10.8 in the intro, 12.7 in the prayer; the host frame 4.6-9.5 (its aura alone 4.5-8.4 where the
dread is over 3); the portraits' re-renders 2.6-9 (the two animated figures need more than the 18 KB cache
holds); Fuzz, the demon, the candles and the curtains 1-3 each; the lane 6-7; the verdict's whole-screen dim
15.8; the result screen's single whole-screen layer 15+. The reason the stage kept so little: a layer that did
not change but lay on a changed box was put back and drawn whole, and that widened the dirty set, which
dragged in the next layer, and so on. A dialogue box over the bottom rows redrew Fuzz, the candles, the curtains
and the host on every frame; the demon's bob redrew the host frame. In the intro 10 layers of 14 were drawn a
frame, one of them a changed one in three.

### Exact steps (frames identical to the base ROM)

1. **Clipped redraw** (`core/include/doomo/backdrop.h`, `core/src/backdrop.c`). A layer declared with
   `stage_layer_clip` (it draws only with `plat_rect`, `plat_shade_rect`, `plat_text`, `plat_text_outline` and
   `plat_image` at scale 1: the UI shapes and text go through those) that did not change but lies on the
   dirty set is no longer put back whole. The dirty set is cut once into disjoint pieces (`build_pieces`: a band
   per row range, adjacent equal spans joined; it replaces `restore_dirty`, which cut it twice), and the layer
   is drawn through the pieces that lie in its box, clipped to each (`stage_clip_n`, `stage_clip_get`,
   `plat_clip`). It does not widen the dirty set, so nothing is dragged in. Pieces are disjoint, so a layer's
   shading lands once on every pixel. Layers with text, lines or scaled images stay as before (a changed box is
   still drawn whole). The exorcism's clippable layers: the host frame (scale 1 with a picture), the candles,
   Fuzz, the demon (scale 1), the faith meter, every church part (heart, curtains, motes) and, on the verdict and
   result screens, the dim, the heart with its sparkles, the percentage, the caption, the title, the line and the
   figures strip.
   GBA: `gba_nodraw` mode 7 (`GBA_NODRAW_CLIP`); `plat_rect` jumps to `stage_clip_rect` (ARM, the overlay's room
   group, which `plat_clip` loads); `plat_shade_rect`, `plat_image` (a cropped image per piece: whole rows as one
   call, else one call per row) and `plat_text` / `plat_text_outline` (`clip_text`: the glyph pixels inside the box)
   cut themselves. Native: `plat_clip` is honoured by the same calls, and counts every call it cannot honour
   (lines, checkers, clears, scaled or flipped images) so a clipped layer that uses one is a test failure.
2. **Idle lane parts have no box.** The lane's feedback pop (full width, over the host, Fuzz and the curtains) and
   the count-in declared a box on every tick while drawing nothing most of the time; they declare an empty one.
3. **Held dims** (the verdict and the result). The verdict's whole-screen dim (15.8 ms) is a clippable layer whose
   key never changes: when something under it changes only that part is dimmed again, the rest of the screen keeps
   the dim it had. The heart and sparkles, the percentage and the caption are layers of their own, in the order the
   one layer drew them. The result screen is the same: the cloud and confetti (a cast out, still one layer that
   moves everywhere), the demon (a failure), the title, the line, the figures strip, the hint and the failure's
   slow fade (a clippable layer; its first level is 0, which is black, and that is kept).
4. **The aura as a kept list.** The host's aura is two ellipses with the frame's hole left out: 5 ms of shape
   code and hole splitting each time the pulse changes, every third tick. The 40-76 rects it draws are kept, one
   list per pulse (-2..2), and drawn again at the frame's place (`plat_rect_run` on the GBA's plain Mode 4 path,
   `plat_rect` otherwise; the list moves with the frame). The lists live in the scene scratch (+3.9 KB there). A
   frame with rows off the screen, and the PS1 / Dreamcast (no holes, they draw the shapes whole), draw it as before.

**Proof of exactness.** (a) Native: `core/tests/test_stage.c` and `test_stage_clip.c` play the whole ritual (240x160
and 320x240; a failure to the end of the result's fade and a cast out, found by seed) with the stage on and off:
every frame identical, and at least a quarter of the frames use a clipped layer with no call the clip cannot
honour; the clip primitives are checked against the unclipped call (rects, shades, images with and without a shadow
or a silhouette, text and outlined text, 400 random cases); the aura's list is checked against the shape code; the
stage's decisions (a clipped layer lying on a dirty box does not widen it, an unclipped one does, the pieces are
disjoint and cover the dirty part) have their own tests. (b) GBA: every frame of the whole ritual (1,382 frames,
`gba/harness/scripts/exorcism_parity.txt`: `vhash-seq` on every tick) is identical to the base ROM's, with the two
pixel-changing steps below turned off (`EXO_HOLD_STILL` 0). The portraits' slice budget depends on how fast the frame
ran, so two ROMs of different speed animate the faces differently; both ROMs are built with `make -C gba rom
PSLICE_DET=1` (`gba/src/pslice.c`: every slice at once, none in idle time; the game ROM's code is unchanged).
Checked when it was built (base `9877a45e`) and again on the landing tree (base `7cda6362`, which has S2-4b's stamps and
S2-4c's kitchen in the same stage): 1,382 of 1,382 frames identical, and the 220 hashed frames of `exorcism_fps.txt`
(both rituals, the cast out's result included) too. (c) PS1: the seven phase shots of `perf_exorcism_phases.txt` are
byte-identical to the base disc's (the PS1 does not take the pixel-changing steps).

### Pixel-changing steps (SPEED DEFAULTS; GBA, Dreamcast and native only; before / after shots below)

Both are under `EXO_HOLD_STILL` (`core/src/exorcism_int.h`): 1 on the GBA, the Dreamcast and native (the GBA's baker
and the tests), **0 on the PS1**, which reads 30.00 fps in all seven phases with the drips and the poses moving (the
owner's SPEED DEFAULTS allow a pixel change only where the exact steps cannot reach 30; a review finding of the landing).
The Dreamcast keeps them: without them its true name, verdict and result windows show 50 ms picture gaps (texture
uploads: its kept church, DC9B, bakes one texture per church key, and every new pose of a figure is an upload); with
them those windows are a steady 30 and the run's uploads fall 1,166 -> 164 (docs/DREAMCAST.md).
Before / after at the same tick (`make PSLICE_DET=1` ROMs of the tip and of G2-EX, `exorcism_fps.txt` with a
`vhash-seq 1` before each `shot`, so both shots show the frame of the same tick; the third column marks the changed
pixels): the landing's evidence `G2-EX/land/before_after_same_tick.png` (session scratchpad, not in the tree). The
changes are the drips on the wall (dread 9 and up: true name, verdict, result) and the host's and Fuzz's faces.

- **The wall's drips hold a length.** The church backdrop's key carried the drips' step (dread >= 9: their length
  cycled every 8 ticks over 5 steps). Each change put a new picture under the stage and redrew the whole screen on
  two frames in a row (one per GBA page): 39-60 ms frames every 8 ticks from the true name on. They stay as the
  first step of the old cycle (2 px wide marks, 3-7 px long, on the wall's stripes). The key is the dread level and
  the failing beams: the GBA bakes 19 church pictures, not 59 (the ROM is 1.53 MB smaller). The Dreamcast's drips hold
  too; the PS1's key still carries the step (`EXO_DRIP_STEPS`).
- **The possessed host and Father Fuzz hold one pose** (`portrait_hold(1)`, as the combat HUD's portrait and the
  title's figures do): no blink, breath, aura-frame or mouth frames; a new expression is a stand-in for a few ticks
  while it renders. Their re-renders were 6-8 ms of a GBA frame in the prayer, water, name, verdict and result.
  The aura around the frame, the shake, the bobs and the candles still move. The Dreamcast's figures hold too; the
  PS1's do not.

### What was tried and not kept (the numbers are why)

- The true-name window and its four names as clippable layers (and split, with the cursor on its own): slower
  (name 43.4 -> 44-46 ms). The layer's cost is its generation, not its pixels (text, checker bands, round rects:
  ~11 ms of calls whichever pixels they reach), and a clipped draw repeats the calls for every piece (the seven
  dust motes make several small pieces). Clipping pays for layers that are cheap to generate and big to fill.
- Drawing a clipped layer through at most two pieces, and a backdrop "diff" (only the drips' boxes dirty when the
  key moves): both made the frames after a key change slower than the full redraw they replaced (60 ms against
  45); the drips' hold removed the cause.
- A clippable speech bubble (worse in the name phase), the aura and the demons' bobs at 15 Hz, the dust motes at
  15 Hz, the verdict's counter at 15 Hz: no change in the measured windows, so not applied.

### Results (2026-11-15, libmgba, on the landing tree: the tip `cf5e9f27` against the tip + G2-EX)

`gba/harness/scripts/exorcism_fps.txt` (the profile script's windows without the `profile` lines, then the same ritual
with seed 2, which ends in a cast out) run with `harness --worklog --no-fps-gates` on both ROMs, table by
`python3 tools/speed2.py table`:

| phase | tip fps | G2-EX fps | tip work avg / max ms | G2-EX work avg / max ms | frames over 33 / over 50 ms (tip -> G2-EX) |
|---|---:|---:|---|---|---|
| intro (dialogue) | 23.7 | **29.8** | 36.5 / 44.4 | 28.2 / 37.8 | 39 / 0 -> 1 / 0 |
| tools menu | 27.0 | **29.7** | 34.3 / 39.4 | 28.7 / 37.4 | 18 / 0 -> 2 / 0 |
| prayer rhythm | 19.5 | 27.5 | 46.1 / 57.1 | 33.1 / 42.0 | 78 / 6 -> 19 / 0 |
| holy water | 23.6 | **30.0** | 36.7 / 47.8 | 22.0 / 35.3 | 42 / 6 -> 0 / 0 |
| true name | 20.0 | 24.0 | 42.1 / 46.4 | 36.6 / 38.5 | 40 / 0 -> 26 / 0 |
| verdict | 19.3 | **30.0** | 45.9 / 55.9 | 26.6 / 35.0 | 37 / 5 -> 0 / 0 |
| result (a failure) | 19.6 | **29.7** | 43.6 / 47.3 | 31.2 / 41.5 | 58 / 7 -> 3 / 0 |
| result (a cast out, seed 2) | 19.7 | 19.8 | 43.0 / 46.9 | 42.5 / 44.8 | 57 / 1 -> 58 / 0 |

Logic is 30 ticks a second in every phase. Holy water and the verdict hold a steady 30 with no frame over 33 ms. The
intro, tools and failure result have one to three single 50 ms frames (a portrait slice or a state change). No frame in
a window is over 50 ms any more. The seed-2 ritual's other windows read the same as seed 1's (intro 30.0, tools 30.0,
prayer 27.9, water 29.9, name 23.5); on the tip before LV11 / LV7a (`7cda6362`) G2-EX read the same numbers to the
0.1 ms. Built on `9877a45e` the same windows read 29.8 / 29.7 / 27.9 / 30.0 / 24.0 / 30.0 /
29.7 (base 23.0 / 27.8 / 19.0 / 23.4 / 20.0 / 19.5 / 19.2). The exact steps alone (before the two held poses and the drips)
measured: intro 28.4, tools 29.0, prayer 37.2, water 27.8, name 43.1, verdict 34.8, result 35.3 ms of work.
`make -C gba test-exorcism` holds these numbers: floors intro 27, tools 28, prayer 25, water 27, name 22, verdict 27,
result 27, cast out 17; it fails on the tip before G2-EX. The speed tour's exorcism floor is 28 (was 15).

**The stage's other users** (the kitchen, S2-4c, and the fair, S2-4b, both landed after G2-EX was built): `speed2`
`fishcook` and `fair` on the same two ROMs. The fair is unchanged (every window the same fps, work within 0.1 ms). The
kitchen gets faster: the dirty set is now put back as fewer, taller pieces (`build_pieces` joins a band to the piece
above it), so its cooking windows do 0.8-2.4 ms less work, the result card 20.8 -> 22.0 fps (41.1 -> 37.9 ms), the
counter 29.2 -> 29.5, serving 29.3 -> 29.8. The native stage on / off tests of both pass on the landing tree.

### What is still missing, and what it would take

- **Prayer, 27.5 fps (33.1 ms; 19 of 111 frames at 50 ms).** The lane is redrawn on every tick by design (the
  prompts move): 6 ms, plus the stage's work 4 ms and the aura's rebuild on pulse changes. 30 would take the lane
  split into a kept ribbon and a few moving prompts, each with its own boxes (the stage keeps one box per layer, so
  eight prompts would need eight layers or layers with several boxes).
- **True name, 24.0 fps (36.6 ms, steady).** The clue window and the four names are one layer of text and shapes
  (13 ms to draw); the host's aura, the demon's bob, the cursor and the seven motes touch its box on nearly every
  tick, and clipping it costs more than it saves (above). 30 would take the window and the buttons as retained
  panels in the stage (the G7 body copy from the front page, which the stage does not use yet), or text drawn
  cheaply enough that a redraw of the pieces costs less than the whole.
- **A cast out's result, 19.8 fps (42.5 ms; was 19.7).** The cloud and the confetti are one layer that moves over the
  whole screen, so every tick is a full redraw, as before G2-EX. It would take the confetti pieces as separate small
  boxes (more layers than the stage's 24) or the confetti drawn over a kept frame of the rest; confetti at 15 Hz would
  not help on the GBA (each page compares with the picture two ticks old: docs/GBA.md "Speed wave 2: S2-4c").
- **The phase changes.** Every layer's key carries the state, so the first frames of a new phase redraw everything on
  both GBA pages, plus the new expression's portrait: ticks outside the measured windows (`--worklog`, seed 1 / seed 2;
  tip -> G2-EX): true name -> verdict 88.0 + 76.0 -> **85.9 + 67.6 ms**; verdict -> result 93.6 + 69.0 -> **91.1 + 66.7
  ms** (a failure), 151.0 + 72.7 -> **155.2 + 68.8 ms** (a cast out: Fuzz's and the host's happy pictures render); leaving
  the result 103.2 -> **100.9 ms**. A window that starts on such a frame reads lower (seed 2's verdict window holds one:
  29.0 fps with one 67 ms frame; with seed 3, 20.8). Better than the tip: a second ritual's first frame (96.0 -> 49.5 ms) and a frame
  after the holy water (81.2 -> 26.5 ms); the first ritual's first frame is the tip's (138.1 -> 138.5 ms; it was 181 ms
  before the landing fixed the held pose's first fetch). Not profiled further here; a key that
  does not carry the state for layers that look the same in both phases, and the next expression rendered ahead
  (`portrait_prefetch`), are the likely levers.
- Single 50 ms frames in the intro (1), tools (2) and the failure's result (3): portrait slices and the dialogue.

### RAM, ROM (GBA; measured on the landing tree, `make -C gba sizes` and `gba/tools/memreport.py`)

- EWRAM `.bss` 254,820 -> 254,828 B (**+8 B**; 6,428 -> 6,420 B free): the aura's pointer (4 B) and the clip box's fifth
  halfword (`stage_box[5]`: the old `trk[4]` plus the saved `gba_nodraw`), with padding. The aura's five lists (3,924 B)
  and the stage's clip mask and pieces live in the exorcism's scene scratch: its claim grows 1,672 -> 6,000 B of the
  12 KB block (stage memory 1,240 -> 1,644 B, the name cache 432 B, the aura 3,924 B). The kitchen's claim grows by the
  same 404 B of stage memory (its static assert allows 2,048).
- IWRAM resident 11,272 -> 11,288 B (**+16 B**, `plat_rect`'s mode-7 dispatch in `video_loops.s`); the main stack's room
  6,288 -> 6,272 B (gate 6,144 B; 5,124 B beside the flash routines while saving). The overlay's room group 9,800 ->
  9,952 B (+152 B, `stage_clip_rect`; 400 B of the 10,352 B slot left beside it).
- ROM 9,929,752 -> 8,399,980 B (-1.53 MB: the drips' hold, 19 church pictures baked instead of 59), `.text` +6,696 B.
- Net zero is not met: +8 B EWRAM and +16 B IWRAM (GBA ledger row "G2-EX"); tried for the landing and not found without
  moving the aura's pointer behind a call on the hot path (the bytes land in padding elsewhere).
- PS1: see `docs/PS1.md` "Speed tour 2" (G2-EX): 30.00 fps in all seven phases before and after, the shots identical,
  resident +0 B (129,504 B below the stack top with the boot FMV). The aura's list is not built there (no holes).
- Dreamcast: see `docs/DREAMCAST.md` (G2-EX).

