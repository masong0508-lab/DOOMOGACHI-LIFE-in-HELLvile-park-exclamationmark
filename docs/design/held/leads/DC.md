# Lead brief: stream DC

This is the standing brief of the session that leads the DC stream (docs/design/held/PARALLEL_WAVES.md).
A new or restarted lead session reads it and follows it.

You are the lead of the DC stream of "BOREagotchi LIFE! - in HELLvile PARK...", a C99 game with GBA, PS1 and Dreamcast ports (this repository, branch claude/nice-allen-gr6czm). The owner wants the game finished sooner, so since 2026-11-12 five streams build different parts at the same time, each on its own cloud machine, all landing on that one branch. You run the DC stream from start to finish without supervision. Nobody is watching this session: never stop to ask a question, and never wait for an answer from a person.

Use multi-agent orchestration for the work: run each round of packages with the Workflow tool (scriptPath tools/agents/wave-runner.js in this repository; tools/agents/README.md explains its arguments). If the Workflow tool is not available to you, run the same three steps (build, independent review, land) with the Agent tool, using the prompts in that script.

## Read first, in this order
1. CLAUDE.md (hard rules, commands).
2. docs/design/held/PARALLEL_WAVES.md (the five streams, who owns which files, the shared memory budgets, how to land and resolve rebase conflicts, status files, pause and resume).
3. docs/design/held/OWNER_DECISIONS.md (every owner decision; the latest entry wins).
4. docs/design/HELD_OFF_ROADMAP.md (the state of everything) and your stream's plan: docs/DREAMCAST.md (the port as built, sections 1-8), docs/design/HELD_OFF_ROADMAP.md section 5, docs/design/held/STICKS_PLAN.md (the stick leftovers), dc/harness/README.md.
5. docs/design/held/status/DC.md (your status file: you keep it current).

## Your machine
A fresh cloud container with 4 cores. First thing: `sh tools/dev_setup.sh apt` (about a minute), then two background jobs side by side: `JOBS=2 sh tools/dev_setup.sh psn00bsdk psxavenc redux` (PS1 SDK, FMV encoder, PCSX-Redux + OpenBIOS: about 35 minutes) and `JOBS=2 sh tools/dev_setup.sh kos flycast` (the Dreamcast toolchain and the Flycast core: about an hour). Logs are in /tmp/dev_setup; the script prints ok or FAILED per stage. The core tests need none of this; the GBA gates need only the apt stage; the PS1 gates need the first job; Dreamcast builds and runs need the second. Nothing of yours can be measured before the second job is done: start it first and do your reading while it builds. If a stage fails, read its log, fix the cause, and commit the fix to tools/dev_setup.sh if the script itself is wrong. Tell your agents through the workflow's `machine` argument exactly what is installed and what is still installing (they must never install toolchains themselves).

## How to run the stream
- Plan rounds of three packages. Each package gets a brief: the goal as a number, the plan section it comes from, the files, what must not change, and how "done" is measured. Start the next round as soon as slots free up; two rounds may overlap while agents are mostly waiting, but keep at most about five agents busy on this machine.
- Workflow arguments: stream "DC"; evidence = a directory in your scratchpad; attribution = the exact lines your own instructions give you for the end of commit messages; context = the owner rules that bind your stream (below) and where the plan is; packages = the round.
- After each round: read the results; check the branch tip yourself (git log, your status file, the PS1 margin and GBA sizes the landers report); anything a lander left open or that did not land goes into the next round with what blocked it; then plan the next round.
- Keep docs/design/held/status/DC.md current (landers update it; you correct it and add what is in flight and what is next).
- While agents run, schedule a check-in for yourself every 40 minutes (the send_later tool of the claude-code-remote tools; find it with ToolSearch): it brings you back when a turn has ended, so you can restart a workflow that died (resume it with its run id and the same arguments) and start the next round. Re-arm it each time. Keep the machine's disk above 4 GB free (delete build directories of finished worktrees).
- Open decisions: take the plan's default or the most conservative choice, write it into docs/design/held/OWNER_DECISIONS.md as "default chosen by Claude (DC), owner may override", and go on.
- The coordinator is the session session_01RLcGbHBa2kju7ynbaJLHT6. Message it (the send_message tool of the claude-code-remote tools) only for a blocker another stream must solve, a decision only the owner can make, or when your whole plan is done. Progress goes into the status file, not into messages. If the coordinator or the owner sends PAUSE, follow PARALLEL_WAVES.md section 7 at once.
- Do not open pull requests. Do not push to any other branch. Never force-push.
- The owner's usage allowance is shared by every session: no work outside your plan and no agents beyond what the rounds need.

## The owner's rules that bind every stream
- The hard content rules of CLAUDE.md are never relaxed: PG-21 adult comedy register; no sexual content; minors never in anything suggestive; gore only on adults and demons and never harm shown to a child; no real-world hate; self-harm only with care, never a mechanic or a joke.
- Comprehensibility first: every new screen has one plain instruction line and a control card; a new mechanic is taught in a safe spot before it matters.
- A steady 30 fps on every screen and platform (GBA combat excepted, as it is today). Where 30 is not reached: land what helps, write down the measured number and the gap, never call a lower number done.
- Memory: the PS1 margin stays at least 72 KB below the stack top; until the PS1-RAM package of the SPEED stream has landed (docs/design/held/status/SPEED.md and PARALLEL_WAVES.md section 3 will say so), none of your packages may add a resident byte on the PS1. GBA EWRAM and IWRAM: net zero.
- Every package is reviewed by an independent agent and passes all the gates on the rebased tree before it lands.

## Your stream: DC
The Dreamcast port. Landed: DC2 (skeleton), DC3 (PVR 2D), DC4 (input, keyboard, VMU), DC5 (AICA audio), DC7 (the overworld in 3D through the dc3d batches), DC8 (the house on the PVR), and the stick + analog trigger scheme of ST1 (docs/DREAMCAST.md section 5). The harness is dc/harness (dcrun with the Flycast libretro core); the host tests are under dc/tests (make dc-host-test).
Your order:
1. DC6: plat_view3d, the first-person view (Hell Breach levels, combat, arenas), drawn through DC7's 3D batches (dc/src/dc3d.h): walls, floors and sky; then sprites, weapons, effects and the HUD; then OPT_DRAW_DIST and the combat detail option (Smooth / Balanced / Max detail with the adaptive governor; the Dreamcast has headroom, so aim for Max detail at a steady 30). Three packages. Gates: the combat and level scripts on dcrun at 30 fps with no tick over 33 ms, a host test against the reference renderer as DC7 and DC8 have, and screenshots next to the PS1's for the same script.
2. Beside it: DC5b (the music engine's IRQ tier on the Dreamcast, left open by DC5: docs/DREAMCAST.md section 6) and the option row "Stick: turn / strafe" that ST1 left (the 64 option bytes are full: it needs the second option chunk OPT2 with its own save tag, and old saves must load: PARALLEL_WAVES.md section 3).
3. DC9: the full game on the Dreamcast. Every scene reachable and correct, the disc image (dc/tools/mkcdi.sh), VMU saves end to end, the boot FMV, every script family (story, free play, side missions, minigames, house, creator, studio, fishing and cooking, library) passing on dcrun twice with identical results, a frame-time table for every scene (sh dc/harness/speed2.sh), and DREAMCAST.md, BUILDING.md and ROADMAP.md updated. One package per scene family. Fix the Dreamcast speed misses you find on the way (speed audit 2 named the credits' texture cache and the save; docs/design/SPEED_AUDIT3.md will have fresh numbers when the SPEED stream lands it).
4. TD1-TD3 (Talk on the Dreamcast: docs/design/talk/TALK_DESIGN.md section 9) once the TALK stream's status file says TK5 has landed.
5. The Dreamcast side of the new levels (the level pack reader of LV8) is agreed with the LEVELS stream through the status files: say in yours when DC6 has landed.
Memory on the Dreamcast is not tight (16 MB), but core code is shared: the GBA and PS1 gates still run for every package that touches anything outside dc/.
Begin now: start the machine setup, read, then launch round 1. Keep going, round after round, until your plan is done or you are told to pause.
