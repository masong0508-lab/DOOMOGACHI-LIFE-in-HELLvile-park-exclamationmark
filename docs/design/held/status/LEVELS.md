# Stream LEVELS: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.
What goes in: last update, lead session, landed (package, commit, key numbers, PS1 margin and
delta, GBA sizes), in flight (package and stage), next up, blockers, needs from other streams.

- **Last update:** 2026-11-13 04:55 UTC: resumed (coordinator's RESUME)
- **Lead:** session_01Xd63cvXV9YcGJn2CBo4zvt (machine: every toolchain and emulator installed: GBA, PS1 SDK + PCSX-Redux, KallistiOS + Flycast)
- **Landed:** the plan and the owner answers are in docs/design/levels/LEVELS_EXPANSION_PLAN.md; no code yet.
- **In flight:** round 1 restarted: LV0 (level registry, records, `BRCH` chunk, select pages), LV1 (map format v2, `levelcheck.py`), LV10 (level test harness): building. LV0 and LV1 continue from their worktrees (the same work is in `docs/design/held/patches/wip/LEVELS/`; each lander deletes its diff when it lands).
- **Next up:** round 2: LV7 (art and audio kit), LV2 (script VM, after LV1), LV11 (content notes, after LV0); then LV6, LV3, LV4, LV9, LV5, LV8 (GBA side, tool and host tests first)
- **Blockers:** none
- **Needs from other streams:** SPEED: PS1-RAM must land before LV8's PS1 packs add any resident byte. DC: DC6 before the Dreamcast level reader and runs. CREATOR: nothing yet.
