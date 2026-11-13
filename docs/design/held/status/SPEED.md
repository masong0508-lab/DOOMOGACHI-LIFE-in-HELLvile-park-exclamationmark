# Stream SPEED: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.
What goes in: last update, lead session, landed (package, commit, key numbers, PS1 margin and
delta, GBA sizes), in flight (package and stage), next up, blockers, needs from other streams.

- **Last update:** 2026-11-13 14:50 UTC (coordinator)
- **Lead:** the coordinator session (session_01RLcGbHBa2kju7ynbaJLHT6)
- **Landed:** landed before the restart: S2-1 to S2-8, S2-12, S2-13, ST1 (analog sticks). PS1 margin 65,952 B on e89f329.
- **In flight:** eight packages are built and independently reviewed; none has landed yet. Each lander must rebase onto a branch that moves every ten minutes and re-run the gates, and seven landers share four cores (CPU pressure 82%). Order: PS1-FLAT first (it frees PS1 resident room: margin 68,384 -> 72,928 B on its tip), then PS1-RAM and S2-10; S2-9, S2-11, S2-4b, G2-EX and S3-1 run at nice +10 until those are in.
- **Next up:** after the landings above: round 3 (S2-4c kitchen, S2-4d dreams, G2-CO concert), then S3-12, S3-7, S3-8, S3-13, S3-14, S3-15. Nothing new starts until two of the eight have landed and the load is under 6.
- **Blockers:** the PS1 margin is 68,512 B on the tip (hard failure at 65,536 B): every stream's landing eats into it, so PS1-FLAT and PS1-RAM are the critical path. CPU on this machine is the limit.
- **Needs from other streams:** none
