# Stream STUDIO: status

Kept by the stream's lead (rules: ../PARALLEL_WAVES.md section 5). Update it with every landing.

- **Last update:** 2026-11-15 (ST-EASY land agent): ST-EASY landed; the STAGE tab on the GBA (22.6 fps) is still under 30 and belongs to the next studio package (commit titles `STUDIO ST-EASY: ...`).
- **Lead:** `session_01Xd63cx71nhiXXY4Jbxr864` (Sonnet, from 2026-11-15)
- **Landed:** ST-0: `docs/design/STUDIO_PLAN.md` (flow, screens, controls, format, memory), defaults in OWNER_DECISIONS.
- **Landed (ST-EASY, round 1):** the Start card, help line + control card, redo, START plays, cursor line, labels, Studio level (docs/STUDIO.md "Easy mode" and its function ledger). Review fix: Make it! / Shuffle undo and redo whole (a 14 byte recipe record in the redo journal; takes replace each other), test `studio_make_it_undoes_and_redoes_whole`. Measured on the rebased tree: core 1508 tests passed, SAN run 1508 passed, check-gen and xcheck (433 files, gcc arm mipsel sh4) OK; GBA build + test OK, `.iwram` 11288 B, `.iwram_bss` 4088 B, `.bss` 210508 B (the package adds only the tail of `BakeSt`, 448 B + 10 B recipe), `profile-studio` song / notes / order / lyrics / choir windows 30.0 fps, `speed2 studio` every window 30 except the STAGE tab (22.6 fps, 36.3 ms work: the known S3-8 miss, 37.8 -> 37.3 ms with this package, not caused by it); PS1 margin 110,608 B below the stack top (resident share 0), game_smoke PASS, speed2 studio all 30; Dreamcast builds, dc-host-test OK, speed2 studio all 30.0 (work max 12.6 ms), the dc audio-test timing check failed on the busy host (no audio code touched).
- **Open (measured gaps):** the made song playing on the SONG tab is 28.8 fps on the GBA (32.7 / 39.0 ms avg / worst); Shuffle or Make it! on a first take over an already composed song (before-image over 768 B) cannot be undone; Advanced shows nothing yet.
- **Next up:** ST-PLAY, ST-TOOLS, ST-ADV, ST-USE (`../leads/STUDIO.md`). ST-ADV: lengths of 32 / 64 bars (the Start card stops at 16 passes = 32 bars), what Advanced shows. ST-PLAY: the plan's PLAY tab will take the START tab's slot in the strip (the labels are in `studio_help.c`).
- **Blockers:** none.
