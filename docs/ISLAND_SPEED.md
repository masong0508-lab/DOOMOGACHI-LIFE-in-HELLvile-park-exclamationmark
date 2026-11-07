# Island hub speed on the GBA (sub-roadmap, G8)

Goal: lift the free-play island hub (`SCENE_ISLAND`) and its town hall menu from ~7 drawn fps toward 30, without changing a pixel
where a step says "exact". Same shape as `docs/PAUSE_SPEED.md` and `docs/INVADED_SPEED.md`: each step lands alone and keeps
`make test` and `make -C gba test-speed` green. This file is the V0-equivalent: baseline, where the time goes, and the first step.

## Baseline (libmgba harness, `make -C gba profile-all`, 2026-11-06)

| scene | logic ticks/s | drawn fps | work avg / max ms | to lose for 30 fps |
|---|---:|---:|---|---:|
| island hub | 12.6 | 7.6 | 96.8 / 106.7 | ~67 ms |
| island town hall menu | 11.0 | 6.5 | 108.4 / 128.2 | ~78 ms |

Hottest functions, island hub (8 M instructions, 80.6 video frames): `fp_island_draw_map` 10.6%, `vid_rect_narrow` 9.4%, `put` 9.1%,
`vm_span` 5.6%, `vid_blit` 5.4%, `gba_udivmod` 4.3%, `vm_flush` 3.9%, `vid_rect` 3.5%, `__plat_rect_from_thumb` 2.9%, `use_geo` 2.4%,
`town_count` 2.3%, `plat_rect_m4` 1.8%, `out_rect` 1.8%, `ellipse_draw` 1.7%, `plat_rect` 1.5%, `plat_clear` 1.4%.
Town hall menu: `put` 10.7%, `fp_island_draw_map` 9.7%, `vid_shade_words` 9.4% (the menu's dim), `vid_blit` 5.4%, `vm_span` 4.9%,
`gba_udivmod` 3.6%, `vm_flush` 3.4%.

Reading the hub: the rect pipeline (`fp_island_draw_map` + `put` + `vid_rect_narrow` + `vid_rect` + `__plat_rect_from_thumb` +
`plat_rect_m4` + `plat_rect` + `vid_rect_full` + `out_rect`) is about 40% of the frame, ~39 ms. Of that, `fp_island_draw_map` and `put`
together (the phase functions are inlined into the first) are ~20%, ~19 ms, which is the geometry math and the call overhead, not
pixel fill (`vid_rect_narrow` and friends are the fill). The island is drawn from scratch every frame; only the building sprites are
cached today (`bc_build`, `fp_island_bcache`). `gba_udivmod` (4.3%) is software division, probably inside that geometry.

## What the frame is made of (native runner, `NEW_ISLAND` to `ISLAND`, 78 frames, 2026-11-06)

Throw-away counters in `plat_rect` (not in the tree). They count `plat_rect` calls only: circles, ellipses, checkers and images are not in
these numbers. The hash is over the (x, y, w, h, colour) stream a phase emits.

| phase | rect calls / frame | pixels / frame | changes frame to frame | distinct streams in 78 frames |
|---|---:|---:|---:|---:|
| sea | 8 | 31,216 | 66 / 77 | 67 |
| land | 534 | 27,606 | 26 / 77 | 20 |
| patches | 136 | 5,637 | 3 / 77 | 4 |
| roads | 125 | 5,785 | **0 / 77** | **1** |
| decos | 271 | 3,577 | 21 / 77 | 17 |
| walkers | 48 | 204 | 39 / 77 | 40 |
| buildings | 214 | 3,817 | 77 / 77 | 78 |
| weather | 40 | 120 | 77 / 77 | 78 |
| total | ~1,376 | ~78k | | |

Facts that follow:
- **Roads never change, patches almost never, land changes in a slow cycle (20 states)**: together 795 of ~1,376 rect calls (58%) and 39k
  px come from phases whose output repeats. Sea (few calls, a big fill) and walkers / buildings / weather change nearly every frame.
- A **retained full backdrop** does not fit: 240x160 at 8 bpp is 38 KB and EWRAM free is 24,240 B; both Mode 4 pages are in use
  (`docs/PAUSE_SPEED.md` P1 reached the same wall). A real full-screen retained layer needs the tile backend (Mode 0, 4 bpp tiles, up to
  1,200 distinct tiles = 38 KB of the 64 KB BG VRAM), which is a different renderer and the reason G8 is called a tile backend.
- A **recorded rect list** does fit: 795 rects at 6 bytes (x, y as int16, w, h as uint8, colour) is ~4.8 KB of EWRAM.

## Steps (each lands alone; "exact" = the island's `vhash-seq` frames identical)

- **I0** this document, the baseline and the phase table. (done)
- **I1** (exact) *record and replay the repeating phases.* Key each of roads / patches / land by what actually drives it (find the
  inputs first: `fp_t()`, `fp_island_tint()`, the geometry, locations; roads has none that change in the 78 frames), record the phase's
  `put` / `plat_rect` calls into a ~5 KB EWRAM list when the key changes, and replay the list otherwise: the same calls in the same
  order, so the same pixels, minus the geometry math (`fp_island_draw_map` self time, `put`, part of `gba_udivmod`). Expected: a few
  ms to ~10 ms of the ~19 ms geometry share, measured per phase before it is claimed. Needs a host test that live and replayed streams
  hash equal for every tick of a long run (the hash code above is the model).
- **I2** (exact) the town hall menu: `fp_island_occluder` already skips screen draws wholly under the menu window; check that the replay
  honours `hidden()` per rect, and that `vid_shade_words` (9.4%, the dim outside the window) is drawn once or on the retained-dim idea of
  `docs/PAUSE_SPEED.md` P2.
- **I3** (exact) `vm_span` / `vm_flush` / `vid_blit` (together ~15%): find what `vm_*` is in the island frame (it is not in the rect
  path) before choosing; `town_count` (2.3%) and `use_geo` (2.4%) look like per-frame recomputation of something that changes per tick.
- **I4** (big, exact if the palette fits) the tile backend for the static layers: land + roads + patches (+ sea if its animation can be
  a palette cycle) as BG tiles, sprites and walkers over them as today. Decide after I1..I3 say how much is left; do not start it first.
- **I5** (a DECISIONS call, not planned) lower animation rates (sea, decos) so more frames repeat.

## Acceptance

`make test`, `make -C gba test` (parity), `make -C gba test-speed` with the island `vhash-seq` frames identical to the base for I1..I3,
`make -C gba profile-all` before / after, EWRAM free no lower than 24,240 B less what the step documents. Target for this sub-roadmap:
island hub and town hall at 30 fps with 30.0 logic ticks/s; I1..I3 alone will not get there (about 67 ms must come off): they are the
exact, low-risk part, and I4 is the structural one.

## I1 landed, and where the hub's 81.5 ms goes now (libmgba harness, 2026-11-07)

I1 (land recorded once and replayed, exact) is in: hub 94.7 -> 81.5 ms, 9.7 fps. Measured by skipping one phase at a time
(temporary compile-time mask, not in the tree), work ms per frame:

| phase | ms | | phase | ms |
|---|---:|---|---|---:|
| land (replayed) | 12.5 | | sea | 3.2 |
| buildings | 17.7 | | walkers | 2.1 |
| decos | 10.4 | | weather | 2.6 |
| patches | 7.8 | | roads | 4.9 |
| **everything outside the map** | **21.3** | | whole map skipped | 30.1 fps |

So the map costs ~60 ms and must fall to ~10 ms for 30 fps. Only a retained backdrop can do that; replaying lists in Mode 4 cannot.

## I4 design (tile backend), not built yet
- Mode 0, one BG (8bpp, map in the last screenblock). Base tiles 0..599 = the static backdrop (all static ops of sea, land, patches,
  roads, deco and building statics, shadows), written only when the key (map size, tint, locations, selection) changes.
  Pool tiles 600..991 hold the frame's dirty composites, double-buffered (2 x 196); the 600-entry map is rewritten in the vblank.
- Per frame, walk the ordered op stream. Dynamic ops (glints, foam, shimmer, animated deco parts, walkers, building animation,
  weather, the bobbing selected building) draw into tiles initialised from the base tile. A static op is replayed only if its box
  meets a tile already dirty (z-order stays exact); all other static ops are skipped.
- Known trap: shade ops (building shadows) read what is beneath, so replaying one over a base that already holds it darkens twice.
  Needs an unshaded copy of the shadow tiles, or the shade applied to the dynamic item's colour instead.
- Mode 0 tile data overlaps both Mode 4 pages, so leaving tile mode (menus, fades) destroys the backdrop: a rebake on return.
- G5's canvas (canvas.c) is overworld-specific (policy, backoff, billboard OBJs); the island path is better kept separate.
- Verify with the harness: vhash-seq against the Mode 4 ROM, and fps; the island-only tour is wait/press A x3/wait 200/fps start.

## I4 measurement: how much of the hub is animated (libmgba harness, 2026-11-07)

Tiles (8x8, of 600) the animated content touches in one hub frame, counted with a throw-away marker in `fp_island_gfx.c`
(not in the tree). The static "soft seams" checker rows are static and are not counted.

| source | tiles / frame |
|---|---:|
| animated deco parts (decos phase) | ~107 |
| building animation + the selected building | ~68 |
| weather (default option, this seed has some) | ~50 |
| foam + river shimmer | ~45 |
| walkers | ~26 |
| sea glints | ~3 |
| **union of all** | **~245** (~175 without decos) |

Consequences for the I4 design above:
- ~245 of 600 tiles (41%) are rebuilt every frame. The 2 x 196 double-buffered pool does not fit; one pool of 392 does, so the
  map flip needs another way to avoid tearing (map rewrite in the vblank with the pool tiles in place only if the copy fits).
- Expect the map to cost ~25-30 ms, not ~10 ms: about 20 fps with weather and animated decos. 30 fps needs fewer dirty tiles.
- Cheapest levers (a DECISIONS call, step I5): hold the deco sway still (-~107), no weather on clear days (-~50),
  slower foam (-~45). With all three the count is ~125.
- Seams were first counted as foam (~170 tiles). They are static checkers: put them in the backdrop.

## I2 landed: patches and roads recorded and replayed (exact)

Same mechanism as the land (I1): the roads (no time input) and the clover / plum patches (their ember glow cycles every 8 ticks, so
the key carries `(t / 8) % 12`) are recorded as rects once per key and replayed in order. The patches' ellipses are captured through
`ur_tap` in `ui_raster.c` (one pointer test in `out_rect`); a shape that is drawn another way (a cached sprite on native) marks the
list unrecordable and it is drawn live. Hub 81.5 -> 76.3 ms (9.7 -> 10.1 fps, libmgba). The island hub and town hall menu frames
are identical tick for tick against the I1 ROM (30 + 30 ticks); `make -C core test` 878 passed, 0 failed.
Cost: 3.9 KB more EWRAM (two lists of 320 and 160 8-byte entries; EWRAM free 12.5 -> 8.6 KB).
Not yet: `make -C gba test` / `test-speed` with the floors (run in CI), the decos (107 animated tiles; see I4 measurement).

## I3 landed: ARM rect-run and the deco variant cache (exact)

Hub 76.3 -> 70.2 ms (10.1 -> 10.7 fps, libmgba); island hub frames identical tick for tick (seq 4f32b3aa).
- `plat_rect_run(list, n)` (video_loops.s, ~15 instructions, IWRAM headroom is now very small): draws n recorded rects through
  `plat_rect_m4` in one call. `lr_run` uses it for runs of plain rects when `gba_nodraw == 0` and no occluder is up (weak
  symbol, so other platforms replay rect by rect as before).
- Lists are now recorded with the menu occluder off, so a list recorded while the town hall menu is open is complete.
- `C()` (circles) is recordable like `E()`.
- Decos: one rect list per (kind, sway state, tint), recorded once at a fixed spot and replayed shifted to each deco (17 variants,
  15-20 rects each; pool of 400 rects = 3.2 KB EWRAM). A single list keyed on every deco's phase was rebuilt nearly every frame.
- Town hall menu frames differ from the I2 ROM only because the faster ROM draws ticks the old one skipped (one screenshot
  differed in 14 sea pixels, animation timing); not yet checked with `make -C gba test-speed` parity.
Next (I3b): quantized animation clock for building animation / walkers / weather with recorded lists (governor, pixels change),
clear only outside the sea box, cache `town_count`.

## I3b (governor): animated phases recorded per step, sky clear outside the sea box (NOT pixel-exact vs the old ROM)

- Sea glints, walkers and weather are plain rects: each is recorded once per animation step (`ANIM_Q` = 6 ticks, clock `g_q`) and
  replayed on the draws inside the step (the GBA draws every 3rd tick). These phases now move in 6-tick steps, so the island hub
  `vhash-seq` (de7d5caf) and town hall menu (b780a9d1) WILL change; expect new values in CI, then re-pin.
- `fp_island_clear_outside()` replaces the full `plat_clear`: the sea paints the whole map box, only the strips above and below
  need the sky colour. Same pixels.
- Bug fixed on the way (`ui_raster.c`): a cached shape replayed (or drawn as an image) while `ur_tap` was set bypassed the tap and
  painted straight to the screen. On the first island frame in a process this left a stray tree at the deco recording spot
  (120,110) and made `panel_island_hud_and_ticker_are_pixel_identical` fail when run alone. Replays now go through the tap.
- New test `core/tests/test_island_replay.c`: replayed lists equal live lists, by day and by night, with weather on.
- Unit suite: 883 passed, 0 failed. Not measured on the GBA (no ARM toolchain in this session).

## I8: the hub holds its frame under a steady toast (exact except the toast's sparkle)

The speed tour's slow window (first ~300 ticks of the hub) is the auto-rent and "NEW UNLOCK" toasts: `ui_toast_pending()` forced a
full ~64 ms redraw every drawn tick. A toast holds still for 75 ticks between its slide-in and slide-out, so it is part of the frame:
- `ui_toast_steady()` (ui.c): nonzero from the end of the slide-in to the start of the slide-out (a line wider than its pill: after
  its scroll ends); the value changes when the queue replaces the line.
- The hub records it when it draws a full frame (`I.hub_toast`). The held frame (both Mode 4 pages) then carries the toast, and
  while the same steady toast is up the hub keeps holding and calls `ui_toast_hold()`, so `ui_toast_draw` only advances the timer.
  Slide-in, slide-out, a new line, a scroll step or any other overlay ends the hold and the frame is drawn in full as before.
- The toast's sparkle steps with the hub (every HUB_HOLD ticks) instead of every 2 ticks. Nothing else changes.
- Test: `island_hub_holds_under_a_steady_toast` (test_island_replay.c): against a run with the hold off every full-drawn tick is
  pixel-identical, the toast timer matches tick for tick, held ticks repeat the picture above the news strip, holds happen.
- Not measured here (no ARM toolchain in the session): the CI speed tour should show the toast phases at ~30 ticks/s.

## I9: HUB_HOLD 6 -> 8

CI #551 (toast hold + shorter particles): hub 28.7 logic ticks/s, 19.4 drawn fps, 29.2 ms work avg, 78.4 ms max. What is left is the
periodic full redraw (~64 ms) every HUB_HOLD ticks. HUB_HOLD 8 makes one full frame in 8 ticks instead of 6 (3.75 hub animation
steps/s instead of 5). Rough model (not a measurement): 29.2 -> ~27.5 ms avg. Only a CI speed report says whether the ticks reach 30.
Revert or raise it by changing the one number in core/src/fp_island.c.
