# tools/agents: how the work streams are run

`wave-runner.js` is the Claude Code workflow script that builds this game package by package:
for every package one agent builds it in its own git worktree (`.claude/worktrees/fut-<id>`,
branch `fut-<id>`), a second agent reviews the branch independently, and a third verifies the
findings, rebases, runs every gate and pushes to `claude/nice-allen-gr6czm`.

It is kept here because a cloud machine (and with it a session's own copy) is rebuilt after a
pause, and because several sessions use it (docs/design/held/PARALLEL_WAVES.md).

To run a round, call the Workflow tool with `scriptPath` = this file and args:

```json
{ "wave": "name of the wave or round",
  "stream": "SPEED | TALK | DC | CREATOR | LEVELS (names docs/design/held/status/<STREAM>.md)",
  "context": "what every agent of this round must know (owner rules, where the plan is)",
  "attribution": "the lines your own session ends commit messages with (Co-Authored-By..., Claude-Session...)",
  "evidence": "/absolute/dir for logs and screenshots (your scratchpad; not in the repo)",
  "machine": "optional: what is installed on this machine right now, if not everything is",
  "packages": [ { "id": "S2-9", "title": "...", "brief": "goal, measurements, files, limits, how done is measured",
                  "after": ["PS1-RAM"] } ] }
```

`after` makes a package's landing wait for other packages of the same round. Three packages per
round is what a 4-core machine carries; a second round can run beside it when agents are mostly
waiting. A good brief names the goal as a number, the files, the plan section, what must not
change, and how "done" is measured.

Without the Workflow tool the same three steps can be run by hand with the Agent tool: the
prompts are the `buildPrompt`, `reviewPrompt` and `landPrompt` functions of the script.

A fresh machine needs `sh tools/dev_setup.sh` first (toolchains and emulators, about an hour).
The plans the briefs are written from: `docs/design/HELD_OFF_ROADMAP.md`, `docs/design/held/`,
`docs/design/SPEED_AUDIT3.md`, `docs/design/talk/`, `docs/design/levels/`.
