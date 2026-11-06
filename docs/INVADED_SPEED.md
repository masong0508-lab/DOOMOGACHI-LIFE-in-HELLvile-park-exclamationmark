# Invaded Hahaland speed (sub-roadmap)

Goal: lift the invaded map (`MAPID_HAHALAND_INVADED`, chapter 3) from 15 drawn
fps to 20 on the GBA, keep its 30 logic ticks/s, and keep every frame
pixel-identical wherever a step says "exact". Same shape as
`docs/PAUSE_SPEED.md`: each step lands alone and keeps `make test` and
`make -C gba test-speed` green.

## Baseline (libmgba harness, `make -C gba test-speed`, 2026-11-06, CI)

| scene | logic ticks/s | drawn fps | work avg / max ms |
|---|---:|---:|---|
| invaded | 29.8 | 14.9 | 57.8 / 79.5 |
| hahaland visit (same tour, for scale) | 30.0 | 28.6 | 33.7 / 37.7 |

Floor in `speed_tour.txt`: 12.5. The tour's `vhash-seq 30 invaded` frames are
the exactness reference.

## Why it sits at 15, and what 20 needs

Drawn fps is 60 / (vblanks a frame takes), so the steps are 30, 20, 15, 12:
- 15 fps = a frame of 50..66 ms. The average (57.8) is inside it, but the
  worst frames (79.5) fall into the 12 fps bucket and skip a logic tick, which
  is why logic reads 29.8, not 30.0.
- **20 fps needs the work under ~50 ms on every frame**: about 8 ms off the
  average and about 30 ms off the worst frame. Anything less moves no fps, only
  the margin.
- 30 fps (work under ~33 ms) is out of reach on this map: 47 billboards alone
  are about a quarter of the frame and the ground is 15 ms.

## Where the time goes

Last measured breakdown (G0 profiling pass, 2026-11-01, before G2), then G2's
effect (`docs/GBA.md` section 6):

| part | before G2 | after G2 | note |
|---|---:|---:|---|
| sprites (`ow_sprite`) | 77.2 ms | 24.6 | 47 tinted sprites, 45,284 px, 23 distinct images (20.8 KB as padded 8bpp, max 46x49) |
| ground | 15.2 | ~8 (est.) | |
| HUD | 4.3 | | |
| toast over the HUD | 2.7 | | the toast is why the HUD panels cannot be retained here |
| logic | 6.9 | | `ownpc` 2.5, `find_interactables` 1.8 |
| periodic spike | | ~20 ms every ~18 drawn frames | blamed on `build_billboards()` at the time; see V1 host side: the tint-level flip is the stronger suspect |

These are old, partial and not summed to the current 57.8 ms: step V1
measures the current split before anything is changed.

Facts that bound the options:
- The tint level on this map takes two values (`2 + (wave + 2) / 3`, wave in
  -2..2 gives 2 or 3), so a lit-and-tinted copy of an image needs two
  variants, not eight.
- The dither phase is the screen pixel (x & 3, y & 3), so a moving billboard
  changes its pixels. That, not the size, is what stops a per-image cache and
  what stops OBJ (tile-mode) billboards (`docs/GBA.md` section 6: "invaded
  cannot be tiled exactly: 15 of 47 blocks spill").
- The core keeps some state per drawn frame (sparkles, the portal animation),
  so parity checks hash from the scene's first tick (the tour's windows do).
- EWRAM free is 26,336 B and the IWRAM stack is 6,260 B (>= 6,144): about 20 KB
  of new RAM at most, and the overlay slots are full, so any cache lives in
  EWRAM, not IWRAM.

## V1, host side (2026-11-06): what the core hands the renderer

Measured with the native runner on `scene STORY 219` (seed 1, 200 ticks) and
`STORY 10` (hahaland) using throw-away counters (not in the tree). These are
the core's own numbers, so they hold for the GBA; they are not GBA timings.

| | invaded | hahaland |
|---|---:|---:|
| billboards submitted / on screen | 47 / 30 | 24 / 18 |
| on-screen billboard box area (px) | ~22,300 | ~17,300 |
| visible flipped / translucent / scaled | 0..2 (<= 3,400 px) / 0 / 0 | |
| body-sprite requests per tick | 7 | |
| colormaps used by sprites in a frame | 4 (levels 12..15) | 1 |
| `plat_rect` / text / image / shade calls per tick | 98 / 6 / 2 / 9 (~420 px shaded) | 86 / 5 / 1 / 0 |
| view tint level | 2 for 60 ticks, 3 for 30, repeating (90-tick wave) | 0 |

What that rules in and out:
- **Culling (old V3) is a no-op.** `ow_sprite` returns before any work when the
  billboard is off screen, so the 17 hidden ones cost only their setup.
- **A flipped / translucent / scaled row loop is not worth it.** Almost every
  visible sprite is on the fast `ow_spr_rows` path; flipped ones are at most 3,400
  px of ~22,300.
- **The sprite LUT cache does not thrash in the tour.** Four colormaps need four
  builds a frame, and the tint's two pixel classes leave 8 / 2 = 4 ways, so all
  four stay. (At night under the tint there are 4 classes, 2 ways and the same four
  colormaps: that case WOULD thrash, and the tour does not cover it.)
- **2D calls are not the gap to hahaland**: 98 vs 86 rects, ~420 shaded px.
- **The periodic ~20 ms spike has two candidates**, and the first is new:
  1. *Tint-level change.* `plat_overworld` puts `owf.tint_level` into
     `lc.tint_state`, and `lit_cached()` puts that state in every tile's key.
     The invaded pulse (`overworld.c`, `lvl = 2 + (ui_wave(..., 90, 2) + 2) / 3`)
     flips the level 2 <-> 3 twice per 90 ticks, so every lit ground tile and
     block face on screen is rebuilt (each with the per-pixel `post()` dither) on
     the frame after a flip: about 15-30 drawn frames apart, which fits the
     "~every 18 drawn frames" spike better than anything else.
  2. *A new chibi body frame.* `bodyspr_image()` renders a missing (dir, frame)
     synchronously (`render_body_ctx`): about one miss per 30 ticks natively,
     < 1 ms on average, but a single 16-20 ms frame.

Not known until the GBA is profiled: how much of the 57.8 ms is the ground (and
its tint), sprites, logic and HUD. The 24 ms gap to hahaland (33.7 ms) is NOT
the draw-call count or the sprite structure, so look at the world tint, the logic
(6.9 ms measured before G2 against 3.1 on hahaland) and the toast.

## Steps

- **V0** this document, the baseline, `profile-invaded` and its script. (done)
- **V1** measure on the GBA: `make -C gba profile-invaded`, paste the function
  split here; add local `gba_cycles()` marks (not committed) around the ground,
  sprites, HUD, toast, logic and `build_billboards()`, and log `lc.builds` (the lit
  tile rebuilds) per drawn frame, and whether the slow frames are the ones right
  after a tint-level change. Host side done (above).
- **V2** the tint flip (exact if done in the renderer; the pulse stays). Make the
  tile cache survive a 2 <-> 3 flip: key the tiles on the tint state, as now, but
  keep both states warm (a second tile set, or build the other level's tiles one
  or two per tick ahead of the flip, as the pulse is periodic and known).
  Expected: removes the ~20-70 ms spike frames (max 79.5 -> about 55) and puts
  logic back at 30.0. If V1 shows the rebuild is not the spike, fall back to the
  chibi miss below.
- **V2b** (only if V1 says so) the chibi miss: render the next walk frame for a
  walking citizen one tick early, in `ownpc_add_billboards`, so the miss lands on
  a lighter frame. Exact; < 1 ms on average, helps only the worst frame.
- **V3** (dropped: `ow_sprite` already rejects hidden billboards).
- **V4** sprite pixels: only if V1 says sprites are still > 20 ms. The tinted
  path is already one table lookup per pixel and the ARM row loop covers ~95% of
  the pixels, so the remaining gains are small (a cheaper LUT build for the four
  colormaps, ~1 ms) unless V1 surprises us.
- **V5** toast / HUD (exact): draw the toast alone into the retained HUD page,
  as the pause does for its window. 2-4 ms while a toast is up; check how long
  it stays on this map first.
- **V6** logic (not exact in time, needs sign-off): `find_interactables` (1.8 ms)
  and part of `ownpc` (2.5 ms) every second tick. Skip if V2..V5 reach 20.
- **V7** (a DECISIONS call, not planned) sprite-anchored dither for OBJ
  billboards. Changes pixels; the other maps' tile frames lost to Mode 4.

## Acceptance

`make test`, `make -C gba test` (parity), `make -C gba test-speed` with the
invaded `vhash-seq` frames identical to the base for V2..V5 (V6 and V7
regenerate them and say so), `make -C gba profile-invaded` before / after,
EWRAM free no lower than 26,336 B less what the step documents. Target:
invaded drawn fps >= 20 with 30.0 logic ticks/s; then raise the floor in
`speed_tour.txt` from 12.5 to about 17 (85% of 20).

## PS1

The PS1 docs have no invaded row; `perf_*.txt` has none either. If the PS1
invaded map is also slow, it needs its own V1 on the PS1 harness; its renderer
(`plat_overworld` in `ps1/src`) shares nothing with the steps above except the
core's `build_billboards` (V2, V3).
