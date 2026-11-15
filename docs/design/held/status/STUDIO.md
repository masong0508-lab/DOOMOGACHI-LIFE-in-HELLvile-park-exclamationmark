# Stream STUDIO: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.

- **Last update:** 2026-11-15 (ST-EASY land agent): ST-EASY landed (commit titles `STUDIO ST-EASY: ...`).
- **Lead:** `session_01Xd63cx71nhiXXY4Jbxr864` (Sonnet, from 2026-11-15)
- **Landed:** ST-0: `docs/design/STUDIO_PLAN.md` (flow, screens, controls, format, memory), defaults in OWNER_DECISIONS.
- **Landed (ST-EASY, round 1):** the Start card, help line + control card, redo, START plays, cursor line, labels, Studio level (docs/STUDIO.md "Easy mode" and its function ledger). Review fix: Make it! / Shuffle undo and redo whole (a 14 byte recipe record in the redo journal; takes replace each other), test `studio_make_it_undoes_and_redoes_whole`. Measured at landing: core 1478 tests, SAN, check-gen, xcheck 431 files OK; GBA `.bss` / `.iwram` unchanged by the package (state is the tail of `BakeSt`, 448 B + 10 B of recipe), `profile-studio` windows all 30.0 fps and not slower than before; PS1 margin 110,608 B below the stack top (resident share 0), game_smoke / studio / studio_easy PASS, speed2 studio 30 fps; Dreamcast builds, speed2 studio 30.0 ticks/s, boot PASS.
- **Open (measured gaps):** the made song playing on the SONG tab is 28.8 fps on the GBA (32.7 / 39.0 ms avg / worst); Shuffle or Make it! on a first take over an already composed song (before-image over 768 B) cannot be undone; Advanced shows nothing yet.
- **Next up:** ST-PLAY, ST-TOOLS, ST-ADV, ST-USE (`../leads/STUDIO.md`). ST-ADV: lengths of 32 / 64 bars (the Start card stops at 16 passes = 32 bars), what Advanced shows. ST-PLAY: the plan's PLAY tab will take the START tab's slot in the strip (the labels are in `studio_help.c`).
- **Blockers:** none.
