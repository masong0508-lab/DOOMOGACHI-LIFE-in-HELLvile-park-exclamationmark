# Stream LEVELS: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.
What goes in: last update, lead session, landed (package, commit, key numbers, PS1 margin and
delta, GBA sizes), in flight (package and stage), next up, blockers, needs from other streams.

- **Last update:** 2026-11-13: LV1 built on branch fut-lv1 (map format v2, levelcheck), awaiting review and landing
- **Lead:** session_01Xd63cvXV9YcGJn2CBo4zvt (machine: every toolchain and emulator installed: GBA, PS1 SDK + PCSX-Redux, KallistiOS + Flycast)
- **Landed:** the plan and the owner answers are in docs/design/levels/LEVELS_EXPANSION_PLAN.md; no code yet.
- **In flight:** round 1: LV0 (level registry, records, `BRCH` chunk, select pages) and LV10 (level test harness) building; LV1 (map format v2, `tools/levelcheck.py`, `core/tools/map_bfs.h`) built and in review (its patch file is deleted in the same commit; LV0's `fut-lv0.diff` stays in `docs/design/held/patches/wip/LEVELS/` until it lands).
- **Next up:** round 2: LV7 (art and audio kit), LV2 (script VM, after LV1), LV11 (content notes, after LV0); then LV6, LV3, LV4, LV9, LV5, LV8 (GBA side, tool and host tests first)
- **Blockers:** none
- **Needs from other streams:** SPEED: PS1-RAM must land before LV8's PS1 packs add any resident byte. DC: DC6 before the Dreamcast level reader and runs. CREATOR: nothing yet.
