# Stream CREATOR: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.
What goes in: last update, lead session, landed (package, commit, key numbers, PS1 margin and
delta, GBA sizes), in flight (package and stage), next up, blockers, needs from other streams.

- **Last update:** 2026-11-13 04:55 UTC (lead) - resumed after the pause
- **Lead:** session_01Xd63cwsuWNoECo4JeUnWy3 (GBA and PS1 tools installed; Dreamcast toolchain installing again after a container restart)
- **Landed:** landed before the restart: CRA (CR0, CR1, CR2, CR12), CRB (CR6, CR7, CR9). 433 parts catalogued, 351 live with icons.
- **CR5a (landed 2026-11-13):** GBA EWRAM free 1,788 -> 15,444 B (island hub lists moved into the arena top, 13,720 B); island hub frames identical (vhash-seq 94cd2df9, fps 30.0); IWRAM unchanged; PS1 object sizes identical (the full PS1 EXE margin: see the commit message); docs/GBA.md ledger row 41. The Town Hall menu speed-tour hash moved (portrait phase only; a log line, not a gate).
- **In flight:** round 1 restarted 04:52 UTC, all three building (they continue from the paused worktrees; the wip patches under `../patches/wip/CREATOR/` are deleted as each package lands): CR3 (the saved patch fut-crc.mbox, gene lists go live); G2-CR (GBA creator held-input speed gap, 20.5-26.2 fps -> 30; lands after CR3); CR5a (GBA EWRAM room for the LOOK pool)
- **Next up:** round 2 as soon as CR3 has landed: CR4 art batches A1 face and eyes (283), A2 hair (148), A3 spooky (134); then A4, A5, CR5, the later batches; CR8, CR11, CR13, CR14; CR10 after PS1-RAM; the six level sets when LEVELS asks or last
- **Blockers:** CR3's PS1 build is 288 B over the 64 KB stack-reserve check (+704 B resident vs 4c0f369, which has 416 B slack) with the PS1 gating already in (DOOMO_GENE_EXT 0); needs about 300-700 B resident back, or the SPEED PS1-RAM package, before CR3 can land
- **Needs from other streams:** SPEED: PS1-RAM must land before CR10 and before any creator package may add PS1 resident bytes
