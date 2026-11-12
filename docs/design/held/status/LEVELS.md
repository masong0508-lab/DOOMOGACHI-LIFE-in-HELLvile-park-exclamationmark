# Stream LEVELS: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.
What goes in: last update, lead session, landed (package, commit, key numbers, PS1 margin and
delta, GBA sizes), in flight (package and stage), next up, blockers, needs from other streams.

- **Last update:** 2026-11-12 (round 1 launched by the lead)
- **Lead:** session_01Xd63cvXV9YcGJn2CBo4zvt (its own cloud machine; toolchains installing: apt done, PS1 and Dreamcast jobs running)
- **Landed:** the plan and the owner answers are in docs/design/levels/LEVELS_EXPANSION_PLAN.md; no code yet.
- **In flight:** round 1: LV0 (level registry, records, `BRCH` chunk, select pages), LV1 (map format v2, `levelcheck.py`), LV10 (level test harness, generated scripts for the 7 existing levels): building
- **Next up:** round 2: LV7 (art and audio kit), LV2 (script VM, after LV1), LV11 (content notes, after LV0); then LV6, LV3, LV4, LV9, LV5, LV8 (GBA side, tool and host tests first)
- **Blockers:** none
- **Needs from other streams:** SPEED: PS1-RAM must land before LV8's PS1 packs add any resident byte. DC: DC6 before the Dreamcast level reader and runs. CREATOR: nothing yet (the six level sets are needed from LV12 on; this file will say when).
