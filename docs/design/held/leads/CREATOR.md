# Lead brief: stream CREATOR

This is the standing brief of the session that leads the CREATOR stream (docs/design/held/PARALLEL_WAVES.md).
A new or restarted lead session reads it and follows it.

You are the lead of the CREATOR stream of "BOREagotchi LIFE! - in HELLvile PARK...", a C99 game with GBA, PS1 and Dreamcast ports (this repository, branch claude/nice-allen-gr6czm). The owner wants the game finished sooner, so since 2026-11-12 five streams build different parts at the same time, each on its own cloud machine, all landing on that one branch. You run the CREATOR stream from start to finish without supervision. Nobody is watching this session: never stop to ask a question, and never wait for an answer from a person.

Use multi-agent orchestration for the work: run each round of packages with the Workflow tool (scriptPath tools/agents/wave-runner.js in this repository; tools/agents/README.md explains its arguments). If the Workflow tool is not available to you, run the same three steps (build, independent review, land) with the Agent tool, using the prompts in that script.

## Read first, in this order
1. CLAUDE.md (hard rules, commands).
2. docs/design/held/PARALLEL_WAVES.md (the five streams, who owns which files, the shared memory budgets, how to land and resolve rebase conflicts, status files, pause and resume).
3. docs/design/held/OWNER_DECISIONS.md (every owner decision; the latest entry wins).
4. docs/design/HELD_OFF_ROADMAP.md (the state of everything) and your stream's plan: docs/design/held/CREATOR_PLAN.md (the plan: the package table CR0-CR14 and the art batches), docs/CREATOR.md and docs/PORTRAIT.md (what is built), core/data/parts/catalog/ (the part catalog).
5. docs/design/held/status/CREATOR.md (your status file: you keep it current).

## Your machine
A fresh cloud container with 4 cores. First thing: `sh tools/dev_setup.sh apt` (about a minute), then two background jobs side by side: `JOBS=2 sh tools/dev_setup.sh psn00bsdk psxavenc redux` (PS1 SDK, FMV encoder, PCSX-Redux + OpenBIOS: about 35 minutes) and `JOBS=2 sh tools/dev_setup.sh kos flycast` (the Dreamcast toolchain and the Flycast core: about an hour). Logs are in /tmp/dev_setup; the script prints ok or FAILED per stage. The core tests need none of this; the GBA gates need only the apt stage; the PS1 gates need the first job; Dreamcast builds and runs need the second. Your work needs the GBA and PS1 gates: launch round 1 as soon as the apt stage is done; the landers need the PS1 job finished. If a stage fails, read its log, fix the cause, and commit the fix to tools/dev_setup.sh if the script itself is wrong. Tell your agents through the workflow's `machine` argument exactly what is installed and what is still installing (they must never install toolchains themselves).

## How to run the stream
- Plan rounds of three packages. Each package gets a brief: the goal as a number, the plan section it comes from, the files, what must not change, and how "done" is measured. Start the next round as soon as slots free up; two rounds may overlap while agents are mostly waiting, but keep at most about five agents busy on this machine.
- Workflow arguments: stream "CREATOR"; evidence = a directory in your scratchpad; attribution = the exact lines your own instructions give you for the end of commit messages; context = the owner rules that bind your stream (below) and where the plan is; packages = the round.
- After each round: read the results; check the branch tip yourself (git log, your status file, the PS1 margin and GBA sizes the landers report); anything a lander left open or that did not land goes into the next round with what blocked it; then plan the next round.
- Keep docs/design/held/status/CREATOR.md current (landers update it; you correct it and add what is in flight and what is next).
- While agents run, schedule a check-in for yourself every 40 minutes (the send_later tool of the claude-code-remote tools; find it with ToolSearch): it brings you back when a turn has ended, so you can restart a workflow that died (resume it with its run id and the same arguments) and start the next round. Re-arm it each time. Keep the machine's disk above 4 GB free (delete build directories of finished worktrees).
- Open decisions: take the plan's default or the most conservative choice, write it into docs/design/held/OWNER_DECISIONS.md as "default chosen by Claude (CREATOR), owner may override", and go on.
- The coordinator is the session session_01RLcGbHBa2kju7ynbaJLHT6. Message it (the send_message tool of the claude-code-remote tools) only for a blocker another stream must solve, a decision only the owner can make, or when your whole plan is done. Progress goes into the status file, not into messages. If the coordinator or the owner sends PAUSE, follow PARALLEL_WAVES.md section 7 at once.
- Do not open pull requests. Do not push to any other branch. Never force-push.
- The owner's usage allowance is shared by every session: no work outside your plan and no agents beyond what the rounds need.

## The owner's rules that bind every stream
- The hard content rules of CLAUDE.md are never relaxed: PG-21 adult comedy register; no sexual content; minors never in anything suggestive; gore only on adults and demons and never harm shown to a child; no real-world hate; self-harm only with care, never a mechanic or a joke.
- Comprehensibility first: every new screen has one plain instruction line and a control card; a new mechanic is taught in a safe spot before it matters.
- A steady 30 fps on every screen and platform (GBA combat excepted, as it is today). Where 30 is not reached: land what helps, write down the measured number and the gap, never call a lower number done.
- Memory: the PS1 margin stays at least 72 KB below the stack top; until the PS1-RAM package of the SPEED stream has landed (docs/design/held/status/SPEED.md and PARALLEL_WAVES.md section 3 will say so), none of your packages may add a resident byte on the PS1. GBA EWRAM and IWRAM: net zero.
- Every package is reviewed by an independent agent and passes all the gates on the rebased tree before it lands.

## Your stream: CREATOR
The character creator. The owner wants at least 1,500 NEW parts and accessories and a better creator screen, at 30 fps. Landed: CRA (CR0 budgets and baselines, CR1 the part catalog, CR2 the many-part renderer, CR12 the age gate) and CRB (CR6 the browser grid, CR7 the part icons, CR9 speed). The CREATOR DEFAULTS in OWNER_DECISIONS bind you: the 1,500 counts brand-new parts only (the plan has 1,566 new plus 45 already-drawn extras); the Walten Files parody set is an original send-up of the style and its tropes, with no copied characters, names or logos; mild adult-only props (a wine glass) are tagged adult and never appear on minors; no tobacco, vape or drug props; about a third of the themed sets are locked behind unlocks.
Your order:
1. CR3: the saved patch docs/design/held/patches/fut-crc.mbox (git am -3; it conflicts in core/src/portrait.c, portrait_body.c and portrait_head.c with speed work that landed after it: keep the speed work's retained and sliced drawing, and add CR3's new gene fields and their reads). Delete the mbox in the landing commit.
2. CR4, the art batches, each its own package of 100-200 parts with the contact sheets, the shape gate and review.txt the plan describes: A1 face and eyes (283), A2 hair (148), A3 spooky (134), A4 fur and body (54), A5 outfit (224), then the plan's remaining batches, until at least 1,500 new parts are live. Batches touch different catalog files; generated files are regenerated on every rebase, never merged by hand. Every part carries its age class. You and the reviewer look at the contact sheets with your own eyes: no nudity, nothing suggestive on a minor, no real-world hate symbols, no tobacco, vape or drug props.
3. Beside the art: the creator's held-input speed gap (GBA about 22 fps with a direction held, target 30: docs/GBA.md on CR9, and the speed audits have the profiles); CR5a (GBA EWRAM room for looks: the island hub's record lists into the arena region the hub does not use, or a 4 KB GBA-only look pool, and only if the island hub stays at 30 fps); CR5 (deep parts saved: the LOOK pool and chunk); CR10 (PS1 memory for parts: an SPU-RAM part store or PARTS.BIN on the disc; waits for PS1-RAM); CR8 (outfits and presets); CR11 (set unlocks); CR13 (the prefs chunk); CR14 (docs, and part names through the language packs).
4. The six creator sets tied to the new levels (docs/design/levels/LEVELS_EXPANSION_PLAN.md and its owner answers name them): when the LEVELS status file asks for one, or last.
Old saves must load after every package.
Begin now: start the machine setup, read, then launch round 1. Keep going, round after round, until your plan is done or you are told to pause.
