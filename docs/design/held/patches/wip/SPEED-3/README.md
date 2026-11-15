# SPEED-3 wip patches (PAUSE 2026-11-15 ~20:55 UTC)

Lead: session_01Xd63cvDzgwv9nxsL8r6tWV. Nothing of SPEED-3 is on origin. Apply with `git am` on the tip
(rebase conflicts expected in ROADMAP.md, docs/GBA.md, status/SPEED.md, OWNER_DECISIONS.md: keep both sides' rows,
renumber the ledger row). Re-run all gates after the rebase (each was gated on an older tip).

| file | what | state |
|---|---|---|
| `s4-k-k2.mbox` | S4-K (4 commits: Granny's pose cut, the grade stamp in four layers, tests, docs, ledger row 64) plus S4-K2 (3 commits, the result card with G2-EX's clipped redraw, docs) | S4-K gated and reviewed (exact; result card only 20.8 -> 21.5); S4-K2 built, review and land unfinished |
| `s4-k2-uncommitted.diff` | uncommitted docs edits in the S4-K2 worktree (docs/GBA.md, status/SPEED.md) | apply on top of the mbox |
| `s4-w.mbox` | S4-W, the Prize Wheel result card held (7 commits) | built, reviewed, gated on 86b91c3; rebased once; spin 30.0, result 30.5, card 30.1; the curse wedges stay 23.7 fps (13 frames over 33 ms) |
| `s4-c.mbox` | S4-C, the concert band and the Dreamland stage (3 commits) | built; review and land unfinished; read its GBA.md section for the numbers |

Next: land S4-W first (smallest risk), then S4-K/K2, then S4-C; then the curse wedges and whatever speed audit 4 lists.
