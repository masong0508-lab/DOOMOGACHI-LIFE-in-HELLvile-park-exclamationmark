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
```

Both scripts walk the practice ritual on the plush dummy (`scene EXORCISM -1`) like `core/tests/scripts/life_exorcism.txt`:
the intro under its dialogue, the tools menu, the prayer rhythm, the holy water (reticle held at the left edge, so the phase
ends when the flasks are thrown), the true name, the verdict and the result. Frame parity between two GBA ROMs: the same
script without its `profile` lines (they run instructions, so they move later ticks) and compare the `vhash-seq` lines.

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

## What is still missing for 30 on the GBA (the gap, measured)

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
