# Resume notes (paused 2026-10-18 23:15 UTC)

Everything finished is committed on `claude/nice-allen-gr6czm`, and CI is green through `7fccf54` (run 102).
This folder holds the work that was in progress when the session paused. Nothing here is built into the game.

## Unfinished work, saved as patches (unverified)

The patches apply on top of `7fccf54`, with `git apply docs/wip/<name>.patch`.

| Patch | Package | State |
|---|---|---|
| `house_h3_ps1_room_and_h4m2_visits_mart.patch` | H4.M2 rest: visits (`home_visit.c`), the mart (`home_mart.c`), animations and hooks. H3 (the PS1 house renderer) has landed: skip the patch's `ps1/src/room.c` hunk (`git apply --exclude=ps1/src/room.c`) | mid-work; not yet through the core, GBA and PS1 checks |
| `music_mu1t_m15_mu4d_banks.patch` | MU1T M1.5 (musgen, guard, turnarounds, GEN graphs), MU6 T3, MU4d banks | mid-work |
| `gba_speed_island_wip.patch` | GBA speed pass, stopped mid-work: island hub static layers / row stream (`fp_island*.c`, `ui_raster.c`, `gba/src/video.c`, `bigbuf.c`) | mid-work; apply onto the current branch and re-verify |
| `music_mu1_m4.patch` | MU1 M4 (matrix, stingers, comic bridge, DECK vinyl, GEN bags, chapters, beds), MU6 T8/T9 | mid-work |

Each patch's agent was told to verify everything before pushing. Re-run the full verification after applying a patch: `make -C core test`, SAN=1, xcheck, check-gen, the GBA builds and tests, and clean PS1 builds.

## What is left, in order

1. **Story and dream-house build** (`story_house_build_workflow.js`), still to do:
   - finish H4.M2 (the patch above; H3 has landed);
   - S4: side-mission batch A (54 missions, plus re-timing the 49 existing quests);
   - S5: batch B (55 missions);
   - the Feast cutscene port;
   - I9: integration.
2. **Music core** (`music_core_workflow.js`), still to do:
   - finish M1.5/MU4d and MU1 M4 (the patches above);
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
