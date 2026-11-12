# tools/agents: how the work waves are run

`wave-runner.js` is the Claude Code workflow script that builds this game package by package:
for every package one agent builds it in its own git worktree (`.claude/worktrees/fut-<id>`,
branch `fut-<id>`), a second agent reviews the branch independently, and a third verifies the
findings, rebases, runs every gate and pushes to `claude/nice-allen-gr6czm`.

It is kept here because the cloud machine (and with it the session's own copy) is rebuilt after a
pause. To run a round, pass the script to the Workflow tool with args:

```json
{ "wave": "name of the wave",
  "context": "what every agent of this wave must know (owner rules, where the plan is)",
  "packages": [ { "id": "S2-9", "title": "...", "brief": "goal, measurements, files, limits",
                  "after": ["PS1-RAM"] } ] }
```

`after` makes a package's landing wait for other packages of the same round. Three packages per
round is what a 4-core machine carries. The plans the briefs are written from:
`docs/design/HELD_OFF_ROADMAP.md`, `docs/design/held/`, `docs/design/SPEED_AUDIT3.md`.
A fresh machine needs `sh tools/dev_setup.sh` first (toolchains and emulators).
