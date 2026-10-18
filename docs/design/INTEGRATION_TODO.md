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
- Authored possessor slots are not saved: after a reload, exorcism_authored_row() guesses a possessed story host's row, and natural sim possessions (sim_possession_cap from Ch2; sim_corrupt_natural doesn't skip RF_STORY_NPC) of Sprinklepop/Hattie/Grampy/Lady Smog are misread as authored (forced fail, SF_*_LOST). Fix in S2: VM re-arms POSSESS on resume or STRY v2 saves the slots; then drop the guess and test possessor_survives_a_reload_in_story_mode.
