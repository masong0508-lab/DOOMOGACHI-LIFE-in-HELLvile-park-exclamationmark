# Invaded speed: V4b measured, spike frames, V5, steady split (2026-11-06)

Addendum to `docs/INVADED_SPEED.md` (kept as its own file so it applies on any tree).

## V4b measured, and the spike frames explained (2026-11-06, libmgba harness, ARM toolchain installed in the session)

**V4b is confirmed.** `make -C gba test-ow` passes and `make -C gba test-speed SPEED_GATES=0` reproduces the CI numbers exactly: invaded
work avg 52.0 ms, max 77.1, 29.3 logic ticks/s, 16.2 drawn fps, `vhash-seq` 5e74560c; EWRAM free 24,240 B. (Parity against V4a is still
unchecked: no V4a hash was kept. 5e74560c is now the reference for V5.)

**The spikes are body-frame renders, not the tint flip.** Throw-away counters (not in the tree) logged, per drawn frame, `lc.builds` and the
number of `bodyspr_image()` cache misses (read from the harness, `--worklog`). In ticks 150..373 every one of the five spike frames
(161, 221, 235, 323, 357; 63..76 ms) has exactly one body render and zero `lc.builds`, and no other frame has a body render. So candidate 2
(chibi miss) is the cause and candidate 1 (tint flip) is cleared. One render is the whole +17..25 ms.

**Why the cache keeps missing after warm-up** (native runner, `scene STORY 219`, 400 ticks, a log line in `bodyspr_image`): 7 misses after the
first tick, all frame 0, all a citizen turning to a facing it has not shown yet (ticks 32, 49, 118, 179, 192, 281, 314; the gaps match the
GBA's 60 / 14 / 88 / 34). The set is finite (citizen x 4 facings) but the map has 28 citizens in `ow_npcs` at spawn (script cast, side cast,
ambient), about 6 of them ever on screen, and the body cache is 48 shared blocks.

**Tried and dropped: warm all facings at map entry** (`ownpc_spawn_all`, exact in pixels). 28 x 4 = 112 blocks do not fit 48, so a cap
(<= 6 citizens) is needed and it skips this map; no map benefits. Reverted, nothing in the tree.

**What the spikes are worth: very little for 20 fps.** Steady frames: median 53.1 ms, p90 59.1, and 94 of 115 frames are over the 50 ms a
20 fps frame needs; the five spikes move the average by about 0.2 ms (52.9 without them). Removing them entirely would take logic from 29.3
to 30.0 and the max from 77 to about 62, but drawn fps would stay at ~16. **So V2 / V2b are not on the road to 20 fps.** What is: the
*steady* frame, 3..10 ms lower on every frame (53 -> under 50 at the median and p90 59 -> under 50).

Options that remain, in order of expected value per risk:
1. **V5 toast / HUD** (exact, 2-4 ms while a toast is up): first log how long a toast stays on this map; if it is up for a small share of the
   window it will not move the median.
2. **Steady-state profile of a typical (non-spike) frame**: `ow_spr_rows` 25% + `ow_sprite` 8% are still ~33% (about 17 ms). The ground
   (`plat_overworld` 8.8% + `ow_tile16` 5.4%) and the ~12% logic block (`story_update`, `ownpc_update`, `side_spots`, `pn_*`) are the next
   biggest; `gba_udivmod` / `res_get_look` (3.8%) are cheap to look at.
3. **V6 logic every second tick** (needs sign-off, not exact in time): about 4 ms by the numbers above; it is the only item that alone covers
   most of the gap.
4. If the spikes are wanted gone anyway (logic 30.0, max ~62): slice the chibi render over ticks with a stand-in frame (not pixel-exact for 2-3
   ticks, so a DECISIONS call), reusing the portrait job system (`job_start` / `job_pump`, `draw_group`), which `bodyspr_image` does not use today.

## V5 closed, and the steady frame after V4b (2026-11-06, `make -C gba profile-invaded`)

**V5 (toast / HUD) is moot on this map.** The only toast the invaded map shows is the one-shot "Walk into a nest to fight!" hint
(`overworld.c`, `SF_INVASION_HINT`, set once, `ui_toast` shows it for 9 + 75 + 8 = 92 ticks). The profile and the tour measure from tick ~240,
after it is gone, so there is nothing for V5 to save in steady state.

Function split of the steady window on the V4b ROM (12 M instructions, 116 video frames; invaded 52.6 ms avg, 16.0 fps):

| share | function | about (ms / frame) | note |
|---:|---|---:|---|
| 29.3% | `ow_spr_rows` | 15 | ~45k px, ~5.5 cycles a pixel; already pair writes + transparent-run skip + 4 texels a word |
| 2.8% + 2.6% + 2.6% | `ow_flip_row`, `draw_sprite`, `ow_sprite` | 4 | V4b's mirrored path is ~1.5 ms of it |
| 10.4% + 6.4% + 1.2% + 0.9% | `plat_overworld`, `ow_tile16`, `tile_src`, `lit_cached` | ~10 | ground |
| ~12% | story / npc logic: `res_get_look` 2.8, `story_draw` 2.5, `story_update` 2.2, `ownpc_update` 2.0, `pn_*` 2.0, `story_flag` 1.1, `side_spots` 1.1, `update_resident_ex` 1.0, `gba_udivmod` 1.3 | ~6 | two logic ticks run per drawn frame (30 ticks/s, 16 fps) |
| 7.1% | outside known symbols | ~4 | |

Reading it: the sprite pixel loop has no cheap exact slack left (read in `ow_loops.s`), so the exact levers together are small: the ground
(~10 ms) can only shrink if tiles are not rebuilt or re-blitted, and the logic (~6 ms for two ticks) is the only block that does not depend
on pixels. Halving all logic would be ~3 ms: it takes the median from 53 to ~50 and leaves p90 (59) above it. **20 fps on this map is
therefore not reachable by exact steps alone**; it needs ~3 ms of logic (V6, sign-off) plus ~3..6 ms of something that changes pixels or
work done (far billboards at lower detail, fewer billboards, a cheaper ground in the far rows), or the target is moved.
