# Stream STUDIO: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.

- **Last update:** 2026-11-15 (ST-EASY build agent): ST-EASY is on branch `fut-st-easy`, ready for review and landing.
- **Lead:** `session_01Xd63cx71nhiXXY4Jbxr864` (Sonnet, from 2026-11-15)
- **Landed:** ST-0: `docs/design/STUDIO_PLAN.md` (flow, screens, controls, format, memory), defaults in OWNER_DECISIONS.
- **PAUSED 2026-11-15 20:40 UTC (coordinator: weekly allowance):** workflow stopped, check-ins deleted. ST-EASY (10 commits) and ST-PLAY (6) are built and reviewed but NOT landed: `docs/design/held/patches/wip/STUDIO/st-easy.mbox`, `st-play.mbox` (git am onto the tip; fix conflicts, run all gates, then land). To resume: apply both, run `make -C core -j8 test`, `make -C gba -j4 && make -C gba test`, the studio profile, then start round 2 (ST-TOOLS, ST-ADV) with `tools/agents/wave-runner.js`; the owner's TEXT FITS EVERYWHERE rule and the 8 KB GBA EWRAM share apply. KOS (Dreamcast) may need `JOBS=2 sh tools/dev_setup.sh kos flycast`.
- **In flight (before the pause):** machine setup (PS1 and Dreamcast toolchains building), ST-EASY.
- **Next up:** ST-EASY (in flight), ST-PLAY, ST-TOOLS, ST-ADV, ST-USE (`../leads/STUDIO.md`).
- **In flight:** ST-EASY (built, on `fut-st-easy`: the Start card, help line + control card, redo, START plays, cursor line, labels, Studio level; docs/STUDIO.md "Easy mode" and its function ledger). Another round package edits the same studio files: rebase conflicts are in `studio.c` / `studio_edit.c` / `studio_file.c` (small hunks; the new code is in `studio_start.c`, `studio_play.c`, `studio_help.c`).
- **Next up:** ST-PLAY, ST-TOOLS, ST-ADV, ST-USE (`../leads/STUDIO.md`). ST-ADV: lengths of 32 / 64 bars (the Start card stops at 16 passes = 32 bars), what Advanced shows. ST-PLAY: the plan's PLAY tab will take the START tab's slot in the strip (the labels are in `studio_help.c`).
- **Blockers:** none.
