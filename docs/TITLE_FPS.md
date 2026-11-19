# Title screen fps, and the 2026-11-18 GBA profiling results (notes + plan)

Docs only: nothing in this file changes code. It records the GBA CI repair, the latest speed-tour and
title-profile measurements, what the breach MISS rows mean, and the next steps toward a steady 30 fps
title screen. Numbers below are from the 2026-11-18 CI libmgba run; profiler percentages are shares of
sampled cycles, not milliseconds.

## 1. The GBA build failure (run on `503b2ab`, "GBA audio: precomputed wave tables", #868)

`make -C gba` stopped on `gba/src/audio.c:56`:

    error: 'wave_tables' defined but not used [-Werror=unused-const-variable=]

* **Cause.** `gba/tools/wave_tables.py` now generates `wave_plain[][][]` and `wave_pluck[][][]` into
  `audio.c`, and `wave_load()` is four word writes from those. The old hand-written
  `static const uint8_t wave_tables[WAVE_COUNT][16]` lost its last reader. The GBA build is `-Werror`
  (`GBA_CFLAGS` in `gba/Makefile`), so an unused `static const` fails the compile.
* **Why every GBA step went red.** `audio.o` is a dependency of the ROM, so the ROM, the mGBA tests, the
  speed tour and all six profile jobs failed on the same line. The speed report's "(no scene is under its
  floor)" was printed because `speed_tour.log` did not exist, not because the scenes passed.
* **Fix** (`doomo-gba-audio-unused-wave-tables-nov17a`, run #869): delete the dead array. The shapes
  still live in `wave_tables.py` (`TABLES`), which is the source of the generated blocks;
  `wave_tables.py --check` passes. No generated block, table or register write changed.
* **Rule to remember.** When a table is replaced by a generated one, delete the old one in the same
  patch. The ARM build is the only place this shows up: the host gcc check of `audio.c` does not warn.

## 2. First green run (#869): what the numbers say

All eight GBA test groups passed (lang, ow, home, story, coresave, game, save, et); `doomogachi-gba`
was uploaded. PS1 is unchanged (EXE ends at 0x801ecde0, 78,352 B below the stack top).

Speed tour (drawn fps / work avg ms): 14 of 18 scenes are at 30 or within a frame of it. Below 30:
combat first person 16.7 / 47.1, combat third person 18.9 / 42.9, island town hall menu 27.0 / 31.3,
title 28.6 / 28.3, concert closed curtain 29.2, pet 29.8 / 30.0 (no headroom). Against the 2026-11-06
report: invaded 14.8 -> 29.9, island hub and town hall menu (6-7 fps) -> 30.0 and 27.0, overworld pause
13.6 -> 29.9.

Music Studio: every tab holds 30 fps. Sung PERFORM median 25.4 ms, worst 30.9, none over 33.4. LYRICS is
the heaviest tab (31.2 avg / 33.0 max) and still draws at 30. The combat scenes' cost is the 3D view
(`r3d_flat_loop`, `r3d_column_asm`, `r3d_up2`, `plat_view3d`): about 12-15 ms to cut for 30 fps.

## 3. The breach MISS rows

`make -C gba test-breach2` plays the seven Hell Breach levels with the autopilot and prints three fps
windows per level. In run #869 every window says MISS, at 3.7-16.4 fps (best: vape_cellar late, 16.4 /
41.4 ms; worst: cloud_nine, 3.7-4.8 fps, 97-138 ms avg, 1,423 ms worst frame; most windows also have 13-16
frames of 16+ vblanks, i.e. stalls, not steady slowness).

* **It is not a CI failure.** The step runs with `--no-fps-gates` unless `BREACH2_GATES=1`
  (`gba/Makefile`, the LV10 comment). MISS means "below this window's floor"; the floors are 85% of the
  first measurement (ROADMAP, LV10, 2026-11-13), and that first measurement was already "4-17 drawn fps".
* **It is not the wave tables.** The patch only touches `wave_load()`, which got cheaper.
* **It does matter on a real GBA.** 4-6 fps is a slideshow and a 1.4 s frame is a freeze. It is the same
  3D view cost as the combat scenes, so it will be fixed by the same work. Open: find what causes the
  16+ vblank frames (start with cloud_nine); cut the combat view's 12-15 ms.

## 4. Title screen: 2026-11-18 profiling update

| window | drawn fps | work avg / max ms | note |
|---|---:|---:|---|
| speed-tour title | 28.7 | 27.6 / 72.2 | `vhash-seq` `35b5e9bb` |
| title idle | 28.2 | 27.7 / 80.4 | `profile-title`; 130 ticks |
| title menu open | 26.8 | 30.3 / 62.5 | `profile-title`; 130 ticks |
| first settle | 27.5 | 23.9 / 217.1 | startup/load window; keep separate from steady-state goals |

The speed tour reports no scene below its configured floor. The title nevertheless remains below 30
drawn fps in the speed-tour and dedicated title windows. The menu-open window is the weakest of the
two steady-state title windows. Its 30.3 ms average is below the nominal 33.3 ms frame budget, but its
drawn rate is still only 26.8 fps; the maximum tick is 62.5 ms.

The idle title profile (12,000,000 sampled instructions) reports these largest known-symbol costs:
`vid_rect_narrow` 11.3%, `vid_blit` 10.7%, `vid_text_dil` 6.3%, `vid_rect_full` 6.3%,
`finish_marked` 3.1%, `sh_span` 2.6%, `vid_text` 2.4%, `res_get_look` 2.4%, and `title_draw` 2.4%.
About 16.7% of sampled cycles are outside known symbols.

The menu-open profile reports `vid_blit` 10.4%, `vid_rect_narrow` 10.1%, `vid_text_dil` 6.5%,
`vid_rect_full` 6.1%, `vid_text` 5.7%, `vid_rect` 4.6%, and `title_draw` 2.5%.
This points to broad renderer work (rectangles, blits, and text), rather than a single dominant
title-level function. These symbol percentages alone do not identify which title stage or event causes
the long ticks.

History (`docs/GBA.md`, "Speed pass 2 / 3"): the title rose from 13-14 fps (55-57 ms) to about 28.6
through the hills cache, one-pass outline text, the blit fast path, cached logo letters, hardware-OBJ
figures, and SP2 figure prefetch. The current source adds further sky-coordinate caching and hill-run
merging, but the latest title profile still shows renderer costs and unresolved long ticks.

### What the code already does (`core/src/fp_title.c`, `title_draw`)

`draw_sky`, `draw_ground`, `draw_figures`, `draw_logo`, `draw_menu`, the footer, and `fp_fx_draw`
contribute to the title frame. The current source already includes several optimizations:

1. **Static text caching is already present.** Footer labels are captured in the scene scratch
   allocation, and the logo has the existing `logo_snap` / `logo_extra` path. Do not repeat the old
   footer/logo caching proposal unless a fresh profile identifies a remaining uncached text path.
2. **Sky and hill work are already optimized.** Sky-particle coordinates are cached, and adjacent
   hill rectangles are merged into runs. Do not reapply the earlier sky-cache or hill-run patches.
3. **Other drawing work remains.** Houses, spires, flowers, sky bands, checker lines, moving scenery,
   and menu elements still issue renderer operations. The sampled costs for `vid_rect_*`, `vid_blit`,
   and text routines make these reasonable areas to investigate, but they do not prove which specific
   title stage is responsible.
4. **The long ticks are not yet explained.** The dedicated profile reports maxima of 80.4 ms idle and
   62.5 ms with the menu open. Parade turns, figure-expression changes, HELL glitch effects, and menu
   animation are possible causes only; the summary output does not establish any of them.

### Next steps

1. **Read the slow-tick records before changing code.** `make -C gba profile-title` now runs the harness
   with `--worklog`: after each profile window it lists the ticks over 33.3 ms (`slow frame tick N work
   MS`, at most 40) and the functions that cost extra in them (`slow+ ... ms/frame`). The CI summary
   prints these lines under "Title screen", and `title_spikes.log` in the `gba-speed-report` artifact
   holds the same lines (`title_profile.log` has the full output). `HARNESS_SLOW_MS=45` narrows the
   list to the worst ticks. Do not infer causes from the FPS table alone.
2. **Compare idle with menu-open measurements.** Identify which recorded stage or operation grows
   after START. If the detailed log does not provide stage timings, add narrowly scoped profiling
   counters first rather than guessing.
3. **Target measured renderer work.** Investigate the dominant `vid_rect_narrow`, `vid_rect_full`,
   `vid_rect`, `vid_blit`, and text paths in the context of the title call sites. Prefer eliminating
   redundant drawing or combining truly adjacent static operations. Preserve clipping, layering,
   transparency, and exact pixels.
4. **Explain the outlier ticks.** Use the slow-tick records to confirm or reject the parade-turn,
   figure-expression, glitch-effect, and menu-animation hypotheses. Optimize only the event the trace
   identifies.
5. **Change one thing per patch and remeasure.** Run `make -C gba profile-title` and
   `make -C gba test-speed` after each change. Keep the speed-tour title `vhash-seq`
   (`35b5e9bb`) unchanged unless an intentional visual change is approved. Run
   `make -C gba sizes` before adding memory-heavy caches.

Done when: title idle and menu-open windows both draw at 30.0 fps, steady-state work stays within
33.3 ms per tick, the approved title `vhash-seq` is unchanged, and EWRAM / IWRAM remain within the
current size budget. Treat startup settle spikes separately from steady-state performance.

### How to re-measure

    make -C gba profile-title      # build/title_profile.log: idle and menu-open windows
    make -C gba test-speed         # speed tour; check the title row and vhash-seq
    make -C gba sizes              # confirm EWRAM / IWRAM headroom
    # CI: the run's Summary tab, or the gba-speed-report artifact: title_profile.log, title_spikes.log

## 5. Menu-open spike (2026-11-19, CI run on f3fcfb1): cause and first change

Slow ticks with the menu open: 4 of 55, worst 58.1 ms (58.1, 44.2, 45.4, 35.6 ms, one tick apart). The extra time is
`capsule_plain`, `finish_marked`, `sh_span`, `scan_transitions`: a portrait / figure render, not menu drawing. The idle
window has none.

* **Reading.** The figures change expression 45 ticks apart and the cache has one job slot, so a render starts after the
  one before it ends. With the menu open a frame takes ~25-29 ms and leaves ~4 ms of idle time; slices over that wait
  for `PATIENCE` ticks (`gba/src/pslice.c`). A render asked 75 ticks ahead was not finished when the change came, the
  stand-in's deadline (`PC_STANDIN_HOLD`, 4 ticks) forced the rest, and four ticks in a row ran 8-25 ms over.
  This is from reading the code and the log; it was NOT measured (no ARM toolchain on the writing machine).
* **Change.** `prefetch_figures` asks 88 ticks ahead instead of 75 (`PREFETCH_AHEAD`, `core/src/fp_title.c`). Pictures are
  the same; only when they are drawn. No resident bytes.
* **Check on CI.** "Title screen" in the run summary: `slow frames` in the menu-open window should be 0 of ~55. If
  the same four ticks still show, the cause is the second figure's slot wait: next lever is `PATIENCE` for
  title jobs, or one expression change at a time while the menu is open. `vhash-seq` must stay `8d097471`.
