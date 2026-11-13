# Stream LEVELS: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.
What goes in: last update, lead session, landed (package, commit, key numbers, PS1 margin and
delta, GBA sizes), in flight (package and stage), next up, blockers, needs from other streams.

- **Last update:** 2026-11-13 00:10 UTC: **PAUSED** (coordinator's PAUSE, owner near the hourly usage limit)
- **Lead:** session_01Xd63cvXV9YcGJn2CBo4zvt
- **Landed:** the plan and the owner answers are in docs/design/levels/LEVELS_EXPANSION_PLAN.md; no code yet.
- **In flight (stopped, saved):** round 1 (workflow run wf_fa408d5c-e74, stopped about 20 minutes in, all three in the build step). Unlanded, uncommitted work saved as diffs against base de02ef4 in `docs/design/held/patches/wip/LEVELS/`: `fut-lv0.diff` (LV0: registry, levels2.c, levels_rec.c, save-ext changes; unfinished, untested), `fut-lv1.diff` (LV1: mapc v2, map.h, levels_mapx.c, BFS tool; unfinished, untested). LV10 had no file changes yet.
- **On RESUME:** apply each diff in a fresh worktree `fut-lv0` / `fut-lv1` from de02ef4 (`git apply --index`), then run round 1 again with tools/agents/wave-runner.js (the build agents continue from an existing worktree); the round's arguments are in the lead session. Then round 2: LV7, LV2 (after LV1), LV11 (after LV0); then LV6, LV3, LV4, LV9, LV5, LV8. Re-arm the 40-minute check-in. A rebuilt machine needs `sh tools/dev_setup.sh` again.
- **Blockers:** none
- **Needs from other streams:** SPEED: PS1-RAM must land before LV8's PS1 packs add any resident byte. DC: DC6 before the Dreamcast level reader and runs. CREATOR: nothing yet.
