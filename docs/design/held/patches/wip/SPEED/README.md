# SPEED work in progress (saved 2026-11-13 18:00 UTC, the owner asked to wrap up)

Each folder is one unlanded package: `git format-patch` files (apply in order with `git am`, onto the tip
of claude/nice-allen-gr6czm; expect rebase conflicts in docs and ledgers, keep both sides) and, where present,
`uncommitted.diff`. All were built and reviewed (PS1-RAM, S2-10, S2-9, G2-EX reviews were still running; S3-1 and
S2-4b were in the land step). Resume: restart the round script (round1 PS1-RAM / S2-9 / S2-10, round2 G2-EX / S2-4b,
S3-1 via wave-runner); a build agent continues in `.claude/worktrees/fut-<id>` if the machine still has it, else
apply the patches into a new worktree. Delete a folder when its package lands.
Landed today: PS1-FLAT, S2-11.
