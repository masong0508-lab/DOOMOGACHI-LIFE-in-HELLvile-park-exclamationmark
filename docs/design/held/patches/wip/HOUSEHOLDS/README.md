# HOUSEHOLDS paused work (PAUSE 2026-11-15, allowance 98%)

`hh-data/`: HH-DATA, built, NOT reviewed, NOT gated, not landed. One commit (`git am -3 *.patch` in a fresh
worktree on origin) plus `uncommitted.diff` (docs: CREATOR.md, GAME_DESIGN.md, PLAYAS.md; `git apply -3`).
It adds household.h/.c, the HOUS chunk, cap 20 everywhere. Unknown: gate results, GBA/PS1 byte deltas.
Resume: continue the build agent from this patch (run all gates, memory report), then review, land;
delete this folder in the landing commit. Next: HH-CREATOR, HH-REL, HH-USE, HH-PETS (plan: docs/design/HOUSEHOLDS_PLAN.md).
