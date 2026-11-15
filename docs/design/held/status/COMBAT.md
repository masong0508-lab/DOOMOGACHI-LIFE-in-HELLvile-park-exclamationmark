# Stream COMBAT: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.

- **Last update:** 2026-11-15 (COMBAT lead): the lead session is running; the measuring kit is in, the GBA baseline is measured, the PS1 and Dreamcast baselines wait for their toolchains (dev_setup is running).
- **Lead:** `session_01Xd63cvheL1r2v43wuUsHm6` (Sonnet), started 2026-11-15. No renderer or combat code is changed until the coordinator sends "AUDIT4 DONE".
- **Landed:** `tools/combat_kit.py` (the measuring kit with autopilot and walk windows; not game code); `docs/design/COMBAT_3D_PLAN.md` (baseline, renderer spec, GBA options with the derived savings, owner choices, package order).
- **In flight:** the PS1 / Dreamcast baselines (PCSX-Redux is still compiling on this machine); then the plan's round 1 after "AUDIT4 DONE".
- **Next up:** the measuring kit and baseline on all three ports, `docs/design/COMBAT_3D_PLAN.md` (renderer spec, measured GBA options, owner choices), then packages (GBA first).
- **Blockers:** speed audit 4 (needs every SPEED package on origin); PS1-RAM / PS1-FLAT for any PS1 resident bytes.
- **Needs from other streams:** DC: DC6B / DC6C (billboards, weapons, effects, HUD, detail option) are theirs; LEVELS: the map format v2 and the hooks they add to combat*.c / actor.c.
- **PS1 resident used of share (8 KB):** 0 B.  **GBA EWRAM used of share (8 KB):** 0 B.
- **Baseline, GBA (libmgba, tip dc04212, `tools/combat_kit.py run gba`, Balanced), 2026-11-15:**
  Vape Cellar fixed fight (`push COMBAT 3`): first person 30.0 logic / **18.7 drawn fps** / 44.2 ms work (max 93.8); third person 29.9 / 18.9 / 41.6 (max 116). Smooth and Max detail the same within 1 fps.
  The seven Hell Breach levels with the autopilot (fights, real enemy counts): drawn fps 3.8 to 17.1 and **logic only 8 to 30 ticks/s** (cloud_nine 8.2, factory 11-12, graveyard 10.7, cathedral 10.8 at the start); worst frames 0.3 to 1.3 s.
  Profile of those windows: the **game logic is the biggest cost, not the renderer**: `bfs_to_targets` (enemy path-finding) 15-34% of the cycles, then `map_floor_z`, `map_cell_safe`, `map_lift_range`, `actors_tick`, `fx_dist`. In the fixed fight the renderer is the cost: `r3d_flat_loop` 9.3%, `r3d_column_asm` 6.2%, `vid_blit` 5.6%, `r3d_up2` 5.1%, `plat_view3d` 4.4%, `ba_face` 4.0%, `combat_bb_sort` 3.4%.
- **Walk windows (player-like, the renderer baseline), GBA Balanced:** 9.0 to 17.6 drawn fps over the seven levels (median about 14.7), work 43-75 ms. In crowded levels the enemies' logic (`actors_tick`, `demon_try_move`, `fx_dist`, `cw_trace`) is 19-33 % of the cycles against about 12 % for the wall / floor / sprite loops: a faster renderer alone does not reach 30 there (plan G-L1, shared with LEVELS). The autopilot windows measure the bot's BFS planner (`combat_bot.c`), a test driver's cost.
- **Next up (after AUDIT4 DONE):** round 1: G-L1 (logic budget), G-H1 (HUD cost), G-P1 (the Mode 1 hybrid prototype, measured), see the plan section 6.
- **PAUSED 2026-11-15 20:50 UTC (coordinator PAUSE, weekly allowance).** Nothing unlanded: no patches needed (`docs/design/held/patches/wip/COMBAT/` is empty on purpose). Check-ins cancelled. **To resume:** (1) wait for "AUDIT4 DONE" before any renderer or combat code; (2) run `sh tools/dev_setup.sh apt psn00bsdk psxavenc redux kos flycast` (the redux build was killed unfinished; PS1 EXE already builds: `cd ps1; PSN00BSDK_LIBS=/opt/psn00bsdk/lib/libpsn00b PATH=$PATH:/opt/psn00bsdk/bin cmake --preset ps1 && cmake --build --preset ps1 -j3`); (3) `python3 tools/combat_kit.py run ps1 /tmp/ckps1 --modes 0 --no-levels`, same for `dc`, and add the PS1 and Dreamcast baselines to the plan section 1; (4) start round 1 of the plan section 6 (G-L1, G-H1, G-P1).
