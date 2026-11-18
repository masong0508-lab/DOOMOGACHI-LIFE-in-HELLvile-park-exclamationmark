# Title screen fps, and the 2026-11-17 GBA CI breakage (notes + plan)

Docs only: nothing in this file changes code. It records (1) why the GBA CI job failed on 2026-11-17 and
what fixed it, (2) what the first green run (#869) measured, (3) what the breach MISS rows mean, and (4)
the plan for taking the title screen to a steady 30 fps. Numbers are from CI's libmgba harness; anything
marked **estimate** has not been measured.

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

## 4. Title screen: where it stands

| window | drawn fps | work avg / max ms | note |
|---|---:|---|---|
| idle | 28.6 | 28.3 / 66.8 | speed tour; `profile-title` says 28.6 / 28.1 / 66.9 |
| menu open (after START) | 26.2 | 33.3 / 81.8 | the only title window under 30: avg is over the 33.3 ms budget |
| first settle | 27.5 | 24.8 / 219.6 | the load; leave it |

Profile, idle window (share of cycles): `vid_rect_narrow` 11.2%, `vid_blit` 10.8%, `vid_text_dil` 6.6% +
`vid_glyph_dilate` 6.5% (outlined text, 13.1% together), `vid_rect_full` 6.2%, `title_draw` 2.7%,
`vid_text` 2.4%, `res_get_look` 2.4%, `gba_udivmod` 2.1%; 19.5% is outside known symbols. Menu open adds
`vid_text` 5.7% and `vid_rect` 4.7%.

History (`docs/GBA.md`, "Speed pass 2 / 3"): the title went 13-14 fps (55-57 ms) to 28.6 through the
hills cache, one-pass outline text, the blit fast path, the logo letters as cached images, the figures as
hardware OBJs and the SP2 figure prefetch. What is left is the redraw of everything else each tick.

### What the code does every tick (`core/src/fp_title.c`, `title_draw`)

`draw_sky`, `draw_ground`, `draw_figures`, `draw_logo`, `draw_menu`, the footer, `fp_fx_draw`:

1. **Static outlined text redrawn each tick.** The footer ("(c) HaHaland", the platform name) goes through
   `ui_text_outline` every frame; so do "in" and "vile PARK..." (when `logo_snap` does not serve them) and
   the four HELL letters (every frame at the small scale). Outline text is the expensive path (the
   dilate pass).
2. **The background is about 160 small rects plus 3-4 ellipse groups per tick:** four sky bands and
   checker lines, ~60 hill rects (heights already cached), 7 houses x 3 and 5 spires x 4 rects, the meadow,
   16 flowers x 3 rects, then the sun / clouds. Only the clouds, sun bob, spire window blink and
   sparkles move. This is the likely home of the `vid_rect_*` time. **estimate:** 4-7 ms of the 28.
3. **The worst ticks (66.8 / 81.8 ms) are not explained by the report.** Candidates: the parade turn
   (every 240 ticks: two `roster_find` and a new figure render), a figure's expression change (a 45-77 ms
   render, sliced), the HELL glitch burst (`T.glitch_t`), the menu pill pop-in. Unmeasured.

### Plan (each step keeps pixels: the title's `vhash-seq` in the speed tour is `a67f87f6`)

1. **Measure.** `make -C gba profile-title` gets the studio's tables: the slowest ticks with a hint for
   each (tick, work ms, what was happening), and per-stage time (sky, ground, figures, logo, menu,
   footer). Reports only, no gates. This names the 66.8 / 81.8 ms ticks.
2. **Cache the static outlined text** with the existing `logo_snap` / `logo_extra` mechanism: the footer
   and the "in" / "vile PARK..." line. Small, pixel-identical. **estimate:** a few ms of the 13%.
3. **Cache the static background.** EWRAM is the limit (about 26 KB free against a 38.4 KB Mode 4 page,
   `docs/GBA.md`), so a cached band or a merge of the static runs, not a full page. Biggest and riskiest
   step; sized from step 1's numbers. The moving parts (clouds, sun, window blink, sparkles) stay live.
4. **Smooth the spikes** that step 1 finds: spread the parade turn and the menu pill pop-in over ticks.

Done when: idle and menu-open drawn fps are 30.0 with no tick over 33.3 ms in the window, `vhash-seq`
unchanged, and EWRAM / IWRAM not worse than the `make -C gba sizes` figures today.

### How to re-measure

    make -C gba profile-title      # build/title_profile.log (idle, then menu open)
    make -C gba test-speed         # the tour; the title row and its vhash-seq
    # in CI: the run's Summary tab, "GBA speed report"; logs are the gba-speed-report artifact
