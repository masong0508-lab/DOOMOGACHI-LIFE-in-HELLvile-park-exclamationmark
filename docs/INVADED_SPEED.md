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
| `build_billboards()` spike | 4 | ~20 every ~18 drawn frames | a sprite-frame rebuild; the 79.5 ms worst frame |

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

## Steps

- **V0** this document, the baseline, `profile-invaded` and its script. (done)
- **V1** measure. `make -C gba profile-invaded`; copy the function split into
  this file (same as the pause baseline). Add `gba_cycles()` marks locally (not
  committed) around ground, sprites, HUD, toast, logic and `build_billboards()`
  to get ms per part, and the count of billboards that are on screen versus
  submitted. No behaviour change. **Everything below is ordered by what V1
  finds; the estimates are guesses until then.**
- **V2** the 79.5 ms worst frame (exact). Find what the ~20 ms
  `build_billboards()` spike rebuilds every ~18 drawn frames (a sprite-frame
  decode / `sprite_image()` path) and spread it (warm the next frame's images
  one per tick) or cache it. Expected: max 79.5 -> about 60, logic back to
  30.0. Does not move fps alone; it is the precondition for a stable 20.
- **V3** skip what is never seen (exact). Count submitted vs visible
  billboards (the build window is +-640 x -700..+520 world units, larger than
  the 240x160 screen): cull in `build_billboards` the sprites whose box is
  fully off screen and cannot cast into it. Only if V1 shows a real gap; if
  `ow_sprite` already clips them cheaply this is a no-op.
- **V4** sprite pixels (exact, the big one). Two routes, V1 picks:
  1. a per-image, per-light-level lit + tinted texel cache in EWRAM, with the
     dither applied on blit (only the colormap lookup is cached, the dither
     stays at the screen pixel: pixel-identical). Budget: the 23 images are
     20.8 KB padded 8bpp, times 2 levels is too big, so cache only the hot
     ones (LRU, 12 KB).
  2. a cheaper tinted row loop in `ow_spr_rows` (`ow_loops.s`): the tinted
     path was the slow path in every profile (hellvile: "all on the tinted
     slow path"). A tint-aware ARM row loop is exact and needs no RAM.
  Expected 3-8 ms.
- **V5** toast / HUD (exact). The toast makes the HUD redraw every frame. Draw
  the toast alone into the retained HUD page, as the pause now does for its
  window, instead of redoing the HUD. Expected about 2-4 ms, only while a toast
  is up; check how long the toast actually stays on this map first.
- **V6** logic (not exact in time, needs sign-off). `find_interactables` (1.8
  ms) and part of `ownpc` (2.5 ms) every second tick instead of every tick.
  Gameplay-visible by up to one tick, so it needs an OK and a regenerated
  `vhash-seq`; skip if V2..V5 already reach 20.
- **V7** (needs a DECISIONS call, not planned) sprite-anchored dither so
  invaded can use OBJ billboards. It changes pixels and the hashes, and on the
  other maps tile frames lost to Mode 4 (hahaland 35.6 against 34.7 ms), so it
  is a last resort.

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
