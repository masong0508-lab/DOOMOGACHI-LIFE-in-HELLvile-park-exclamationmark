# Stream COMBAT: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.

- **Last update:** 2026-11-15 (COMBAT lead): the lead session is running; the measuring kit is in, the GBA baseline is measured, the PS1 and Dreamcast baselines wait for their toolchains (dev_setup is running).
- **Lead:** `session_01Xd63cvheL1r2v43wuUsHm6` (Sonnet), started 2026-11-15. No renderer or combat code is changed until the coordinator sends "AUDIT4 DONE".
- **Landed:** `tools/combat_kit.py` (the measuring kit; not game code).
- **In flight:** the PS1 / Dreamcast baselines, then `docs/design/COMBAT_3D_PLAN.md`.
- **Next up:** the measuring kit and baseline on all three ports, `docs/design/COMBAT_3D_PLAN.md` (renderer spec, measured GBA options, owner choices), then packages (GBA first).
- **Blockers:** speed audit 4 (needs every SPEED package on origin); PS1-RAM / PS1-FLAT for any PS1 resident bytes.
- **Needs from other streams:** DC: DC6B / DC6C (billboards, weapons, effects, HUD, detail option) are theirs; LEVELS: the map format v2 and the hooks they add to combat*.c / actor.c.
- **PS1 resident used of share (8 KB):** 0 B.  **GBA EWRAM used of share (8 KB):** 0 B.
- **Baseline, GBA (libmgba, tip dc04212, `tools/combat_kit.py run gba`, Balanced), 2026-11-15:**
  Vape Cellar fixed fight (`push COMBAT 3`): first person 30.0 logic / **18.7 drawn fps** / 44.2 ms work (max 93.8); third person 29.9 / 18.9 / 41.6 (max 116). Smooth and Max detail the same within 1 fps.
  The seven Hell Breach levels with the autopilot (fights, real enemy counts): drawn fps 3.8 to 17.1 and **logic only 8 to 30 ticks/s** (cloud_nine 8.2, factory 11-12, graveyard 10.7, cathedral 10.8 at the start); worst frames 0.3 to 1.3 s.
  Profile of those windows: the **game logic is the biggest cost, not the renderer**: `bfs_to_targets` (enemy path-finding) 15-34% of the cycles, then `map_floor_z`, `map_cell_safe`, `map_lift_range`, `actors_tick`, `fx_dist`. In the fixed fight the renderer is the cost: `r3d_flat_loop` 9.3%, `r3d_column_asm` 6.2%, `vid_blit` 5.6%, `r3d_up2` 5.1%, `plat_view3d` 4.4%, `ba_face` 4.0%, `combat_bb_sort` 3.4%.
