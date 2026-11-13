# Resume guide (written 2026-11-13 18:10 UTC, everything paused; allowance resets 2026-11-15 04:00 UTC)

Everything unlanded is in this repository. Nothing important lives only in a chat or on a machine.

## Where the work is
| What | Where |
|---|---|
| Landed work | the branch claude/nice-allen-gr6czm (git log) |
| Unlanded SPEED packages (PS1-RAM, S2-10, S2-9, S2-4b, G2-EX, S3-1, S2-4c) | `held/patches/wip/SPEED/<pkg>/` (git format-patch + uncommitted.diff) and its README |
| Unlanded TALK (T8A), DC (DC6B, DC6C, DC9A), CREATOR (CR5, CR-BODY, A4, A5, A1b), LEVELS (LV7a, LV11, LV3, LV4) | `held/patches/wip/<STREAM>/` and `held/status/<STREAM>.md` ("how to resume") |
| Round scripts and arguments of the SPEED stream | `tools/agents/resume/` (round1.js PS1-RAM/S2-9/S2-10, round2.js S2-11/S2-4b/G2-EX, round3.js kitchen/dreams/concert+choir with the approved trade-offs, round4.js PS1-FLAT; args*.json; leads/*.txt) |
| Owner decisions (latest wins) | `held/OWNER_DECISIONS.md` |
| Streams, ownership, memory budgets, landing rules | `held/PARALLEL_WAVES.md`; lead briefs `held/leads/*.md` (COMBAT not started) |
| Speed plan | `../SPEED_AUDIT3.md` (misses, packages S3-1..S3-15, section 6 trade-offs) |
Not saved (regenerable): before/after screenshots and logs in the old scratchpad, build directories, old worktrees.

## Step by step (one step per chat is fine)
1. `sh tools/dev_setup.sh` on a fresh machine; run `make -C core -j8 test` (must say "N passed, 0 failed").
2. Land PS1-RAM first: `git worktree add -b fut-ps1-ram .claude/worktrees/fut-ps1-ram origin/claude/nice-allen-gr6czm`, `git am held/patches/wip/SPEED/ps1-ram/*.patch` (docs and ledger conflicts: keep both sides), apply `uncommitted.diff` if present, run all gates (list in `tools/agents/wave-runner.js`), push fast-forward only, delete its patch folder. PS1 margin must end >= 72 KB.
3. Same for S2-10, S2-9, G2-EX, S2-4b, S3-1, S2-4c (S2-4c is only 2 files of work). One at a time keeps the CPU free.
4. Then round 3 (kitchen, dreams, concert + the choir-scale experiment), the later trade-off packages, S3-12/7/8/13/14/15.
5. Streams: for each of TALK, DC, CREATOR, LEVELS follow "how to resume" in its status file.
6. When every SPEED package is on origin: speed audit 4 (all scenes, all ports, combat with profiles -> `docs/design/SPEED_AUDIT4.md`), then start the COMBAT stream with `held/leads/COMBAT.md`.
7. New owner requests waiting: creator body types, sliders and the x1-x4 slider range cheat (OWNER_DECISIONS.md, CREATOR BODY AND SLIDERS).
