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

## V1, GBA side (2026-11-06, libmgba harness, `make -C gba profile-invaded`)

First real GBA measurement of this map (the tour's seed and entry; 12 M instructions, 121 video frames).
Settled: 29.8 logic ticks/s, 14.9 drawn fps, work avg 56.8 ms, max 76.5 ms.

| share of the frame | function | note |
|---:|---|---|
| 25.2% | `ow_spr_rows` | the sprite pixel loop (ARM) |
| 8.1% | `ow_sprite` | per-sprite setup and the rows' clipping |
| 4.4% | `owf_sprite_luts` | the composed tables, rebuilt per colormap and frame |
| 2.3% | `draw_sprite` | |
| **~40%** | **sprites in all** | about 23 ms of the 57 |
| 8.8% | `plat_overworld` | ground scan (self time) |
| 5.4% + 1.0% | `ow_tile16`, `tile_src` | ground blocks |
| 0.7% | `lit_cached` | **the tint-flip tile rebuild: at most ~7 ms spread over the window** |
| 2.5% / 1.3% | `res_get_look` / `gba_udivmod` | look lookups; software division |
| ~12% | story / npc logic (`story_update`, `story_draw`, `ownpc_update`, `side_spots`, `story_flag`, `update_resident_ex`, `pn_*`) | |
| 9.0% | outside known symbols | |

Per-frame work (`--worklog`, lines `harness: work TICK MS`): steady frames sit at 50..60 ms
(not 15 fps worth of luck: the average is the floor, not the spikes), and about 5 frames in 190
spike to 67..84 ms (ticks 160, 222, 234, 324, 356 in this run; +17..25 ms each).

What this changes in the plan:
- **The steady state is the problem, not only the spikes.** 20 fps needs every frame under ~50 ms; the
  steady frames are 53..60, so about 8 ms must come off the average work first (sprites are 23 ms of it).
- **V2 (the tint flip) is demoted.** `lit_cached` is 0.7% of the window, and the spikes are 12 ticks
  apart (222, 234), which does not fit the 30/60-tick level flips. It can only explain a few ms of a spike.
- The spikes are still unexplained: a per-frame function profile (every frame's hottest symbols) showed no
  single function in the slow frames, so they are probably spread over several callers or sit in the
  9% outside known symbols. Next: find the symbols outside the table (IWRAM overlays) and log
  `lc.builds` / body-frame renders per frame.
- **Sprite cost is the lever.** Candidates, all exact: cache the composed sprite tables across frames
  (`owf_sprite_luts`, ~0.7 ms a frame, key = colormap, night, tint level and map), and cheaper setup in
  `ow_sprite` (23k cycles a frame for ~30 sprites, about 770 cycles each).

## V4a, done (2026-11-06): the composed sprite tables survive from frame to frame

`owf_sprite_luts` (4.4% of the frame) rebuilt the composed colormap tables of the four colormaps every frame. The tables
depend only on the colormap (constant data), `owf.night` / `owf.tint_level` and the two maps, so `plat_overworld` now keeps an
EWRAM backing copy (`luts_store`, 2,096 B) and loads the frame's working tables (still on the IWRAM stack: the pixel loops read
them for every pixel) from it when night / tint are unchanged, saving them back only after a build (`ow_frame_luts_keep` /
`ow_frame_luts_end`, `OwLuts.dirty`). A first try that made the working tables themselves static (EWRAM) was slower on hahaland
(28.6 -> 26.5 fps: 16-bit bus, 2 wait states on every lookup) and was dropped.

Measured on the libmgba harness, `make -C gba test-speed SPEED_GATES=0`, same script, base ROM vs this ROM:

| scene | work avg before -> after | drawn fps |
|---|---|---|
| invaded | 57.8 -> 54.5 ms (max 79.5 -> 75.4) | 14.9 -> 15.6 |
| hellvile visit | 23.1 -> 21.5 ms | 30.0 -> 30.0 |
| hahaland visit | 33.7 -> 33.7 ms | 28.6 -> 28.8 |
| the other 13 scenes | unchanged | unchanged |

`speedtable.py --parity`: all 16 scenes' `vhash-seq` frames are identical to the base (so this step is exact). `make -C gba test-ow`
passes. EWRAM free: 26,336 -> 24,240 B (-2,096). The invaded map is still 4.5 ms above the 50 ms a 20 fps frame needs on
average, and the worst frames (75 ms) are far above it, so the next steps are `ow_sprite` setup and the spike frames.

## V4b, landed (2026-11-06), NOT yet measured: cheaper `ow_sprite` setup

Source-only change in `gba/src/ow.iwram.c`, `ow_int.h`, `ow_loops.s` (no GBA toolchain was available when it was written, so it is
unbuilt and unmeasured; the first thing to do is the check below):
- `inv` is 65537 when the scale is 1 (nearly every sprite): no software divide.
- the 16-entry `q[][]` fill is one flat loop (`qq[k] = lc[cls[k ^ (xa & 2)]]`, xa is even).
- mirrored, unscaled, opaque sprites (`s == 1 && flip && !trans`) skip the per-column gather (~15 cycles a pixel) and copy each row
  backwards with the new ARM routine `ow_flip_row` (four texels a word read); the edge columns it reads outside the image are the
  ones `head` / `tail` already mask off.
- `ow_spr_rows` is a plain `bl` (IWRAM to IWRAM), `ow_shade` is built `Os`.

CI speed tour after V4b (2026-11-06, `make -C gba test-speed`; EWRAM free still 24,240 B, no scene under its floor):

| scene | V4a -> V4b |
|---|---|
| invaded | work avg 54.5 -> 52.0 ms, drawn 15.6 -> 16.2 fps, logic 29.3 ticks/s, max 75.4 -> 77.1 ms |
| hahaland visit / hellvile visit | 33.7 -> 33.6 / 21.5 -> 21.4 ms (unchanged, as expected: they have no mirrored sprites) |

So V4b is worth about 2.5 ms on invaded: 2 ms above the 50 ms a 20 fps frame needs on average, and the worst frames (77 ms) are not
touched. Parity against V4a is NOT checked yet (no V4a `vhash-seq` was kept); invaded `vhash-seq` after V4b is `5e74560c`, so the next
step compares against it.

Check before the next step (exact step: the invaded `vhash-seq` frames must be identical to V4a):

    make -C gba test-ow && make -C gba test-speed SPEED_GATES=0 && python3 gba/tools/speedtable.py --parity

If parity fails, the likeliest cause is `ow_flip_row`'s edge columns (head / tail masks), then the `q[]` fill. Paste the invaded
work avg / drawn fps here, then go on to the spike frames (log `lc.builds` and body-frame renders per frame) and V5.

## V6, landed (2026-11-07), NOT yet measured on the GBA: the A-prompt search every second tick (invaded only)

Owner sign-off for V6 given in chat. `overworld.c`: on `MAPID_HAHALAND_INVADED`, `find_interactables()` runs on even ticks only. It
searches fresh whenever A is pressed, whenever `near_npc` is past `ow_npc_count`, and on the first ticks of a map, so what a press
of A does is unchanged; only the HUD prompt can trail the player by one tick (33 ms). Other maps are untouched.
`ownpc_update` is NOT thinned: skipping its ticks would change how fast citizens walk, which is a gameplay change, not a time saving.

Expected: about 1.8 ms off one of the two logic ticks of every drawn frame (the measured `find_interactables` cost), so invaded
work avg ~52.0 -> ~50.5 ms. That alone does not reach 20 fps (the median is 53; 94 of 115 frames are over 50 ms).
Host: `make -C core test` 871 / 0 failed, `check-gen`, `story-check` clean.

Check on your CI (the invaded `vhash-seq` may change if a tour frame shows the A prompt one tick late; if it does, that is this step):

    make -C gba test-ow && make -C gba test-speed SPEED_GATES=0 && python3 gba/tools/speedtable.py --parity

Paste the invaded work avg / drawn fps / `vhash-seq` here.

## V6 measured (CI 2026-11-06 tour) and V8, landed (2026-11-07), NOT yet measured on the GBA: the body look hash memo

**V6 saved nothing measurable**: invaded work avg 52.0 -> 51.9 ms, drawn 16.2 -> 16.5 fps, `vhash-seq` still `5e74560c`. Its 1.8 ms came
from the pre-G2 profile; `find_interactables` is not among the hot functions of the current profile. (The CI log said "Nothing to be
done for 'rom'", so check that the V6 commit was in that build.) V6 is harmless but pointless; revert it if you want the map's prompt
exactly per tick.

**V8 (exact).** Host profile of `scene STORY 219` (400 ticks, gprof call graph): `bodyspr_image` is called 7 times a tick (6 from
`ownpc_add_billboards`, 1 from `story_draw`), and each call ran `res_get_look` (40-odd `gene_get`), `look_for_stage` and `look_hash`
just to build the cache key, which is the 3.2% `res_get_look` in the GBA profile. `portrait.c` now memoises that hash per resident
on the raw record: 32 direct-mapped entries of {48 gene bytes, species, hash, id} (56 B each, 1,792 B of EWRAM .bss; a hit needs
the gene bytes and `species` equal, which is everything `look_for_stage` / `look_hash` read, so any look edit recomputes). The deep
run's hash (`look_run_hash`) is still taken every call (the pool is empty or tiny). The image returned is unchanged, so every frame is
pixel-identical. Expected about 1.5 ms off the invaded frame (52 -> ~50.5 ms); `gba_udivmod` (1.3%) is not explained by this and
is still open. Host: `make -C core test` 871 / 0 failed.

Check on your CI (exact step: all `vhash-seq` must equal the table of the V6 run, invaded `5e74560c`), then paste invaded work avg / fps
and EWRAM free (expect about 1,792 B less than 24,240):

    make -C gba test-ow && make -C gba test-speed SPEED_GATES=0 && python3 gba/tools/speedtable.py --parity

## V8 measured (CI 2026-11-07), and V9, landed, NOT yet measured on the GBA: the side-quest spots memo

**V8 worked**: invaded work avg 51.9 -> 50.0 ms, drawn 16.5 -> 17.3 fps, logic 30.0 ticks/s, work max 77.1 -> 69.8 ms, `vhash-seq` still
`5e74560c`; concert 62.3 -> 60.8 ms (`res_get_look` 7.7% -> 4.1%). The only other changed hash is `overworld pause` (`727f6da2` ->
`e9dd90bd`, logic 29.9 -> 30.1 ticks/s): read as frame timing of the pause animation, to be confirmed by `speedtable.py --parity`.

**V9 (exact).** Host call graph of `scene STORY 219` (400 ticks): `side_spots` ran 1.5 times a tick (the A prompt and `story_draw`) and
each call made ~98 `story_flag` calls (`raw_state` for every one of the quests), which is the 1.1% `story_flag` + 1.1% `side_spots`
of the GBA profile. Its answer depends only on the constant quest table, the map and the 2 * NQ side-quest state bits, so
`story_side.c` keeps the last answer with a copy of the bytes holding those bits (a hit is one `memcmp` of `SPOT_BYTES`; a byte shared
with other flags only recomputes it sooner). 60-odd bytes of .bss. Checked on the host with a throw-away build that compared every
memo answer with a fresh scan over the whole suite (871 tests, 160 scripted playthroughs, 19 journeys): no mismatch. Expected about
1 ms off the invaded frame (50.0 -> ~49 ms).

Not done, found in the same profile: `story_draw` builds ~33 prop billboards per drawn frame (`add_bb`, `sprite_frames`, `map_cell_safe`
about 13,000 / 15,000 / 13,000 calls per 400 ticks) although the props are static; caching their list per map would be the next
logic-side step (about 2.9% of the frame is `story_draw` self).

Check on CI (exact step): all `vhash-seq` equal the last table (invaded `5e74560c`), then paste invaded work avg / fps:

    make -C gba test-ow && make -C gba test-speed SPEED_GATES=0 && python3 gba/tools/speedtable.py --parity

## Steps

- **V0** this document, the baseline, `profile-invaded` and its script. (done)
- **V1** (GBA numbers done 2026-11-06, see above; per-frame `lc.builds` still open) measure on the GBA: `make -C gba profile-invaded`, paste the function
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
- **V4a** (done, see above) the sprite tables kept across frames. **V4b** (landed, unmeasured, see above) cheaper `ow_sprite` setup + mirrored rows. **V4** sprite pixels: only if V1 says sprites are still > 20 ms. The tinted
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

## V10 (2026-11-07): frame-time governor, draw-side only, NOT pixel-exact (owner asked for 30 fps)

`gov_pick` in `gba/src/overworld.c`. On a software-path overworld frame with `TINT_RED` (the invaded map only), when the last drawn
frame's work (`gba_work_last`, set in `main.c`) exceeds `GBA_GOV_MS` (30 ms) fewer sprites are DRAWN, nearest to the screen centre first,
until frames fit; with 2 ms to spare they come back (budget -1/8 / +1/8 per frame). Never dropped: off-screen billboards, fullbright
ones (markers, sparkles) and the nearest sprite (the player). Nothing in the game sees it: logic, residents, collisions, saves and the
billboard list are untouched (it only filters the draw order inside the renderer). Scratch is 640 B of static EWRAM, not stack
(IWRAM main stack stays 6,200 B, >= 6,144).

Measured on libmgba (`make -C gba test-speed`): invaded 17.5 -> **29.2 drawn fps**, work avg 49.5 -> 30.7 ms (max 51.5: the chibi
body-render spikes, one frame each), logic 30.1 ticks/s. Every other scene's `vhash-seq` is unchanged; invaded's is now **f87e5596**
(the reference for later steps). Why sprites alone could do it: with every droppable sprite removed the frame is 29.5 ms (ground ~8,
logic ~7, HUD ~1.5), so the near sprites are what is left to budget. A first version (whole 30 px ring always kept, 27 ms target)
only got 25.3 fps: the ring alone cost ~5 ms. `speed_tour.txt` invaded floor raised 12.5 -> 25.
