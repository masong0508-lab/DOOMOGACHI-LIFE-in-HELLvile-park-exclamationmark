# Integration TODO (collected by orchestrator)
- PS1 RAM: engine-test disc overflows 2 MB (CI run 38126404025: image+bss end 0x801f660c). Game disc still fits but will grow with story/audio. Options: LZ-compress art_data/roster in ROM-equivalent and decompress on demand, or load big const tables from CD, shrink PS1 caches, split engine-test assets.
- Core tests failing in WIP tree: test_render raycaster_room_views ("r3d_east left holes"), test_weld x5 (action agent mid-work) - recheck after core workflow finishes.
- Core UI perf (perf agent in progress) + GBA EWRAM budget (perf agent).
- GBA heavy 3D scenes 17-19 fps (perf agent part C).
- PS1 harness not in CI (needs PCSX-Redux build) - optional cached job.
- README status table must be updated when modules land.
- Node 20 deprecation warnings on actions v4 (harmless for now; consider bumping to v5 when available).
- Concert <-> Studio integration: SCENE_CONCERT should use DoomoSong + song_player and offer 'Edit in Studio' (see docs/STUDIO.md integration note from the studio agent).
- GBA EWRAM overflowed by 48 B at CI run 38131709096 (before perf's streaming-save fix landed). Every expansion adds .bss; re-check memreport after all modules land.
## From perf pass (done 12:0x)
- GBA EWRAM over by ~28 KB in live tree (new modules +43 KB .bss: creator 6.3K, fp_* ~10.3K, song_book 5.4K (persistent, keep), overworld 4.2K+fx 3.3K+npc 3.0K, lib_common 2.1K, mg 2.1K, studio 1.9K, creator_draw 1.3K, concert_scene 1.1K). Fix: move transient per-scene buffers to scene_scratch() (12 KB shared, scene.h), shrink caches. GBA IWRAM also over: story's gba ow.iwram.o (+1984 B) + ow_loops (+248 B).
- Unlock id collisions between team tables (concert vs creator parts; freeplay vs minigames cat 9 list 70) -> 3 core test failures. Need a central id allocation table + test that ids are unique across all tables.
- PS1: measure RAM with ALL modules (perf measured 181 KB headroom on a tree without the newest modules). Pet/exorcism scenes 19-27 fps on PS1.
- UBSan left-shift-of-negative warnings in actor.c, combat.c, combat_hud.c, portrait_gfx.c.
- GBA heavy 3D scenes 19.2-19.9 fps (target 20): needs algorithmic change (3rd-person sprite cost 9-10 ms, floor pass).
- Weak registries must read via volatile pointer (GCC folding bug fixed in activities.c/save_ext.c) - check any new registries (studio added slot 5).
## From fishcook/levels (17:0x)
- Combat TPS camera: clamped to floor+6 on top of raised platform when backing into it -> huge texels (combat.c camera clamp).
- GBA harness lacks scene/push commands: levels/fishing/cooking scripts can't run on GBA ROM.
- Autopilot finds 0 secrets (S rank only unit-tested).
- Story must link the 3 story levels into level select.
- Gallery minigame ~2100 plat_rect calls/frame -> measure on hardware (perf agent).
- PS1 black skies -> sent to perf agent.

## From S3.M1 (2026-10-18)
- PS1 game disc overflows 2 MB on a clean build since S1.M4 (740ec45): 0x801f28d0 at 007706e, 0x801f3280 with S3.M1 (~12.9 KB over). ~20 KB is core/src/grumbleguts_gfx.c kept resident because the `fig_fn` function pointer escapes; exorcism.c adds ~1.8 KB resident (exo_rows_default 624 B could move out).
- ~~Authored possessor slots are not saved~~ FIXED in S2: story.c story_rearm_possessors() clears and re-arms the slots on every resume from the .dsc `@armed` windows (legacy_ch12/34.dsc: Steamy, Hattie, Grampy, Lady Smog); exorcism_authored_row() no longer guesses, so a natural possession of a story host keeps the sim's pick. Tests: possessor_survives_a_reload_in_story_mode, story_armed_rows_match_the_possessor_table, story_save_round_trip_mid_story. S3: new authored possessions in ch3/ch4 packs need an `@armed` line each.
- (PS1 overflow above: fixed by 4ba4e96, ovl_plan LOCAL_CALLBACKS.)

## From S2 engine (2026-10-18)
- PS1 story overlay group is 99 KB, over C4's 75 KB cap (it now holds the plot code that the escaping strings used to pin resident; resident headroom went 75 -> 108 KB). It stays under the 116 KB slot and C5's slot-14 KB tail rule. The minigame shells are a new `storymg` group (6 KB; an S2 hunk in H3's GROUPS list, ovl_plan.py). Levers: Ch1-2 content replaces story_script.c's legacy glue (~12 KB) and story_talk.c chatter (~11 KB); credits$ as its own group was tried: only 3 KB leaves, most of credits.c is shared.
- GBA EWRAM: S2's .bss is ~1 KB (8,160 -> 7,104 B free; the SPEC's 176 B plan did not count C5's 256 B stash, the resident HUD objective (a PS1 RAM win) or the verb / minigame state). The >= 8,192 B gate was already missed at the baseline.
- PS1 text window is 20 KB resident (episode 12 + side 8 KB) until H3 adds ovl_tail() (C5): then the side slot moves into the overlay tail (-8 KB).
- GBA / PS1 harness commands `seed`, `expect-flag`, `expect-text`, `story-fp` (C2/R20) are H2/H3's; the native runner has them (story_engine.txt).
- Content gates (story_length_budget, silence_budget, NOBUTTON rations, "Ten years I" == 4) are implemented in test_story_lint.c and bite per chapter once episode packs with minutes land; today only the legacy port exists (lint-exempt).
- core/data/story/test/vmtest.dsc (the VM test fixtures, 688 B pack) is linked into every build like any pack; nothing in the game starts it.
- Escort/find/photo/deliver verbs exist (story_verbs.c) but S100+ givers are not placed on the maps yet (no coordinates in @mission): S4/S5 add a giver spot (or the cast table) when the first missions land.
