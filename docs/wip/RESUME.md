# Resume notes (paused 2026-10-18 23:15 UTC)

Everything finished is committed on `claude/nice-allen-gr6czm`, and CI is green through `7fccf54` (run 102).
This folder holds the work that was in progress when the session paused. Nothing here is built into the game.

## Unfinished work, saved as patches (unverified)

The patches apply on top of `7fccf54`, with `git apply docs/wip/<name>.patch`.

| Patch | Package | State |
|---|---|---|
| ~~`music_mu1t_m15_mu4d_banks.patch`~~ | MU1T M1.5 (musgen, guard, turnarounds, GEN graphs), MU6 T3, MU4d banks | **done** (2026-10-25): applied, finished (T3 + T18 in `test_music_rep.c`, the shared-bank T4 check, docs/AUDIO.md), pushed; the patch is deleted |
| `gba_speed_island_wip.patch` | GBA speed pass, stopped mid-work: island hub static layers / row stream (`fp_island*.c`, `ui_raster.c`, `gba/src/video.c`, `bigbuf.c`) | mid-work; apply onto the current branch and re-verify |
| `music_mu1_m4.patch` | MU1 M4 (matrix, stingers, comic bridge, DECK vinyl, GEN bags, chapters, beds), MU6 T8/T9 | mid-work |

Each patch's agent was told to verify everything before pushing. Re-run the full verification after applying a patch: `make -C core test`, SAN=1, xcheck, check-gen, the GBA builds and tests, and clean PS1 builds.

## What is left, in order

1. **Story and dream-house build** (`story_house_build_workflow.js`), still to do:
   - ~~finish H4.M2~~ **done** (2026-10-25): visits (`home_visit.c`, golden
     `core/tests/golden/home_visit.txt`), HomeMart / the Black Market / the
     bench / Expand / the doors (`home_mart.c`), the hooks (care screen,
     workshop bench, the Library's photo wing), the eight `home_*.txt`
     runner scripts (the runner's `home-room`, `clock`, `money`), the PS1
     home group's PARTS (`ovl_plan.py`: visit, defend, mart, roomtest;
     `plat_code_part()`), docs/HOUSING.md sections 6-9. Left open: the
     apartment facade's A still opens the care screen (HF sec.3.1 wanted
     the house; the free-play scripts rely on it);
   - ~~S4: side-mission batch A~~ **done** (2026-10-25): the 54 missions in
     `core/data/story/side_a/`, the 49 old quests re-timed (`quests49/`),
     givers on the maps, `story_mg_a.c` (six games), `SONG_WALL_LULLABY`,
     `layouts_a.txt`, `test_side_a.c`, runner `side_sample_a.txt`. For S5:
     grafts (S196+) are exempt from one-part-per-chapter (story_vm.h
     `STORY_SIDE2_GRAFT_FIRST`); S4 used 14 `{d}` (the game is at 66 of 80);
     the runner has `side`, `talk`, `cell`, `skip-to`, `skip-to-scene`,
     `skip-talk` for side samples (docs/STORY_ENGINE.md); `story_mg_b.c`
     loads with the PS1 `storymg` group by itself (`story_mg_find` is in
     `story_mg.c`, `ovl_plan.py` `LOCAL_REGISTRIES`): read the registry only
     from there;
   - S5: batch B (55 missions);
   - the Feast cutscene port;
   - I9: integration.
2. **Music core** (`music_core_workflow.js`), still to do:
   - ~~finish M1.5/MU4d~~ **done** (2026-10-25): the shared banks
     (`core/music/common/*.dms`), `tools/musgen` (fingerprint on gcc and
     clang), the GEN slots of TITLE / HAHALAND / INVASION / HOME, T3/T18
     (`test_music_rep.c`, `music_rep_lib.c`, `music_rep_proto.c`: the
     prototype passes, the four known loops fail); the bank is 14,428 B;
   - finish MU1 M4 (the patch above);
   - MU4a/b/c: the wave-1 cues;
   - MU5b: Studio, Concert and DJ.
3. **Finish and optimize** (`finish_workflow.js`), all of it still to do:
   - G0 RAM pass (#77-#80): GBA EWRAM from about 6.6 KB to ≥ 24 KB free; GBA stack ≥ 6 KB; PS1 ≥ 128 KB free below the stack; the story overlay group under its cap;
   - customization P0-P6 (`docs/design/customization/SPEC.md`);
   - music on the consoles: the GBA/PS1 backends, the game hookups, TAPE scratches and the remaining cues (`docs/design/music/SPEC.md`);
   - the speed pass:
     - GBA: island hub ~7.5 fps; dialogue, pet, exorcism and concert ~10 fps; home_defend_w3 12.6 fps;
     - PS1: concert/dialogue ~22, island ~24;
   - final integration with CI green.

These three are scripts for the Claude Code Workflow tool. Every package in them has agents verify and push their own work. Run (3) only after (1) is finished: its P0 step rewrites about 49 shared files.
