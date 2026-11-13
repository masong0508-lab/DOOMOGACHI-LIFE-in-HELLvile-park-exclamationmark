# Parallel streams: five machines, one branch

Owner, 2026-11-12: "have more agents running faster, so you can complete this damn thing sooner".
Until then the waves ran one at a time on one 4-core machine. That machine is the limit: a package's
gates (core tests, sanitizers, GBA, PS1 and Dreamcast builds and emulator runs) cost about an hour of
CPU. So the waves now run **side by side, each on its own cloud machine**, and all of them land on
`claude/nice-allen-gr6czm`.

Every stream lead and every agent reads this file. The hard rules of `CLAUDE.md`, the owner decisions
(`OWNER_DECISIONS.md`, latest entry wins) and the plans (`../HELD_OFF_ROADMAP.md`) are unchanged.

## 1. The streams

| Stream | Runs on | Scope | Plan |
|---|---|---|---|
| **SPEED** | the coordinator's machine: `session_01RLcGbHBa2kju7ynbaJLHT6` | the rest of speed wave 2: PS1-RAM, S2-9, S2-10, S2-11, the rest of S2-4 (fair, kitchen, dreams, cutscenes, library, studio STAGE), the exorcism / concert / pet gaps, the S3 packages of speed audit 3. Also watches CI on the tip and repairs it | `SPEED_AUDIT2.md`, `../SPEED_AUDIT3.md`, HELD_OFF_ROADMAP 2 |
| **TALK** | `session_01Xd63czJHmFDCnjA8mf9GoY` (Sonnet; the first lead `session_01Xd63cxr7ssfTR3pnDRhesw` stopped at the Fable limit) | TK4 (T6 drama manager, outcomes, the Talk save chunk; T5b the English content to about 6,000 alternatives; the census targets), TK5 (T8 house Talk mode, T9 pointer), TKP (PS1: TP0-TP4), later T10 | `../talk/TALK_DESIGN.md`, `docs/TALK.md`, HELD_OFF_ROADMAP 4 |
| **DC** | `session_01Xd63cx9wuhKv8Rqec7pX14` (Sonnet; first lead `session_01Xd63czSpihgJ8Rcfy2jCub`) | DC6 (the first-person 3D view on the PVR, combat detail option), DC5b (music IRQ tier), the stick option row left by ST1, DC9 (the full game on the Dreamcast), Dreamcast speed misses, and TD1-TD3 (Talk on the Dreamcast) once TK5 has landed | `docs/DREAMCAST.md`, HELD_OFF_ROADMAP 5, `STICKS_PLAN.md` |
| **CREATOR** | `session_01Xd63cvvRYHgPW3b8GJby1n` (Sonnet; first lead `session_01Xd63cwsuWNoECo4JeUnWy3`) | CR3 (the saved patch `patches/fut-crc.mbox`), CR4 art batches until at least 1,500 new parts are live, CR5a, CR5, CR8, CR10, CR11, CR13, CR14, the creator's held-input speed gap (GBA about 22 fps), the six creator sets tied to the new levels | `CREATOR_PLAN.md`, `docs/CREATOR.md`, HELD_OFF_ROADMAP 6 |
| **LEVELS** | `session_01Xd63cz7PBDFwYoagK8oCML` (Sonnet; first lead `session_01Xd63cvXV9YcGJn2CBo4zvt`) | LV0-LV11 (foundations), LV12-LV21 (9 main-game levels), LV22-LV37 (New Game+), LV38 (integration) | `../levels/LEVELS_EXPANSION_PLAN.md`, HELD_OFF_ROADMAP 7 |
| **COMBAT** | not started: a session starts after the owner's usage allowance resets (2026-11-15 04:00 UTC); brief `leads/COMBAT.md` | the combat 3D view on all three ports, the GBA first: good (what is drawn), coherent (one renderer spec, parity tests) and fast (a steady 30 where the hardware can). Owner 2026-11-13. Changes no renderer or combat code until speed audit 4 is done (the coordinator says "AUDIT4 DONE") | `leads/COMBAT.md`, `status/COMBAT.md`, `../SPEED_AUDIT4.md` |

The sessions were started on 2026-11-12 23:47 UTC; each lead's standing brief is `leads/<STREAM>.md`.
If a lead session is lost, the coordinator starts a new one with the same brief and edits this table.

More machines may be added to a stream when its packages no longer depend on each other (for
example the level-content packages after LV0-LV11). The coordinator decides and edits this table.

## 2. Who owns which files

A stream changes its own area freely. Outside it: the smallest change that works, and the commit
message says `cross-stream:` and why.

| Stream | Its area |
|---|---|
| SPEED | drawing and frame-time work in the scenes it is fixing (`core/src/mg*.c`, `cook*.c`, `dream*.c`, `cutscene*.c`, `lib_*.c`, `studio*.c`, `weld*.c`, `workshop.c`, `exorcism*.c`, `concert*.c`, `pet*.c`, save streaming in `save.c` / `town.c`), `gba/src`, `ps1/src` (not Talk, not level packs), the memory lists of `ps1/CMakeLists.txt`, `tools/speed2.py`, the `speed2_*` scripts |
| TALK | `core/src/talk_*.c`, `home_talk.c`, `core/data/talk/**`, `core/tools/talkc*.py`, `talk_*.py`, the Talk tests and scripts, `ps1/src/talkdisc.c`, `docs/TALK.md` |
| DC | `dc/**`, `docs/DREAMCAST.md` |
| CREATOR | `core/src/creator*.c`, `portrait*.c`, `part*.c`, `core/data/parts/**`, `tools/part*.py`, `tools/parts_build.py`, the generated part files, `docs/CREATOR.md`, `docs/PORTRAIT.md` |
| LEVELS | `core/src/levels*.c`, `lv_*.c`, `map.c`, `maps.c`, `core/data/maps/**`, `tools/mapc.py`, new level tools, the mechanics and enemies it adds to `combat*.c` / `actor.c` (the hot path must keep its fps), the PS1 level packs, `docs/LEVELS.md` |
| COMBAT | the 3D renderers (`gba/src/r3d*`, `view3d.c`, `viewhold.c`, `ps1/src/r3d.c`, `view3d.c`, `prims.c`) and the combat drawing in `core/src/combat*.c` / `actor*.c`; LEVELS keeps adding mechanics and enemies there (hooks stay, rebase onto them); `dc/**` stays the DC stream's (`cross-stream:` and a status-file agreement) |

Shared files that every stream touches (keep changes small, expect to rebase): `ROADMAP.md`,
`docs/GBA.md`, `docs/PS1.md`, `core/Makefile` and the source lists, the registries in
`core/include/doomo/*.h` (scene ids, save tags, unlock ids, options), `ps1/CMakeLists.txt`,
`ps1/tools/ovl_plan.py`, `gba/Makefile`, `.github/workflows/ci.yml`, `CLAUDE.md`,
`OWNER_DECISIONS.md`, the language packs.

## 3. Shared budgets

* **PS1 resident memory.** The EXE with the boot FMV must end at least **72 KB below the stack top**
  on origin at all times (`ps1/tools/elf2exe.py` prints the margin). On 2026-11-12 it is 65,952 B:
  **until the PS1-RAM package (SPEED) has landed, no other package may add a single resident byte on
  the PS1** (new PS1 code goes into an overlay group, an `-Os` file that replaces more than it adds, or
  data on the disc). After it lands the coordinator writes the freed room and each stream's share
  here. Planned shares: TALK 4 KB, LEVELS 8 KB, CREATOR 4 KB, SPEED 2 KB, the rest stays in reserve.
  Every landing reports its PS1 delta in bytes. PS1-FLAT (houses draw no 3D model, `model.c` in the
  combat overlay group) took it from 65,952 B to 70,480 B on 2026-11-13 (+4.5 KB; on the tip at landing, 68,384 B to
  72,928 B).
* **PS1 overlay slot.** An overlay group must fit the slot (docs/PS1.md). Growing the slot costs
  resident memory: ask the coordinator first. The slot is 131,072 B. The combat group is 128,188 B
  since PS1-FLAT (2,884 B of room; about 200 B on PS1-RAM's tree): LEVELS adds to `combat*.c`,
  `actor.c` and `levels*.c`, and growth beyond that room grows the slot by 2 KB.
* **GBA EWRAM / IWRAM.** About 2 KB of EWRAM free and IWRAM nearly full (`make -C gba sizes`). Net
  zero for every package unless its plan frees room first (CR5a frees EWRAM for the creator).
  Scene data lives in the scene scratch arena, not in new statics.
* **GBA ROM.** 32 MB cartridge, about 5 MB used: not a limit now. Say what a package adds.
* **Options.** The 64 option bytes are full. The first stream that needs a new option builds the
  second option chunk (OPT2, its own save tag, old saves load); the others rebase onto it.
* **Ids and tags** (unlock ids, save tags, scene ids, news ids, map ids, level ids, sound and song
  ids): take the next free one when you rebase. If a rebase shows a clash, renumber **yours**. An id
  that is already on origin never changes: saves depend on it.

## 4. Landing on the shared branch

1. `git fetch origin claude/nice-allen-gr6czm`, rebase your package onto it.
2. Conflicts: in docs and ledgers keep both sides' rows (renumber yours). A generated file
   (`core/gen/*`, anything `make -C core check-gen` covers) is never merged by hand: take either
   side, run its generator again, check with `check-gen`. In source files understand both changes
   and keep both behaviours.
3. Run **all** gates on the rebased tree (the list is in `tools/agents/wave-runner.js`), then
   `git push origin HEAD:claude/nice-allen-gr6czm`. Fast-forward only. Rejected: fetch, rebase,
   re-run what the incoming commits could affect, push again. Never force-push.
4. Never revert another stream's commit. If a clean checkout of origin fails a gate because of
   someone else's landing: tell the coordinator, and fix it forward yourself only when the fix is
   small and plainly right (its own commit, message starting `tip repair:`).
5. Every package still gets an independent review before it lands (the workflow does this).

CI cancels a run when a newer push arrives, so with many landings a green CI run is rare. The gates
a lander runs on the rebased tree are the real check. The coordinator watches the tip.

## 5. Status files

Each stream keeps `status/<STREAM>.md` (next to this file) up to date, in the same push as a landing
or in a small commit of its own:

* last update (UTC), the session that leads the stream;
* landed: package, commit, the key numbers (fps, part count, census, PS1 margin and its delta, GBA sizes);
* in flight: package and stage (building, in review, landing);
* next up; blockers; what the stream needs from another stream.

This is how the coordinator and the owner see progress. Do not message progress: write it there.
Message the coordinator session only for a blocker another stream must solve, a decision only the
owner can make, or when the stream's whole plan is done.

## 6. Running a stream

* Machine: `sh tools/dev_setup.sh` (all stages, about an hour in the background; `apt psn00bsdk
  psxavenc redux` are enough for the GBA and PS1 gates; `kos flycast` for Dreamcast builds and runs).
* Rounds: three packages per round with `tools/agents/wave-runner.js` (build, independent review,
  land), next round as soon as slots free up. Keep at most about five agents busy on a 4-core machine.
* A scheduled check-in every 40 minutes while agents run, to restart whatever died.
* Nobody watches a stream's session: never stop to ask a question. Take the plan's default, write it
  into `OWNER_DECISIONS.md` as "default chosen by Claude, owner may override", and go on.
* No work outside the plan. The owner's usage allowance is shared by every session.

## 7. Pause and resume

On a message that says **PAUSE** (from the coordinator or the owner): start nothing new; stop the
running workflows and agents; save every worktree's unlanded work as patch files under
`patches/wip/<stream>/` (commits as `git format-patch`, uncommitted changes as a diff), commit and
push that; cancel your scheduled check-ins; update the status file; stop. **RESUME**: read the
status file and the patches and carry on.

## 8. What waits for what

* Everything that adds PS1 resident bytes waits for **PS1-RAM** (SPEED).
* **TKP** (Talk on the PS1) waits for PS1-RAM and TK5.
* **TD1-TD3** (Talk on the Dreamcast, DC stream) wait for TK5.
* **LV8** (level packs on the PS1) waits for PS1-RAM; the Dreamcast side of the levels waits for **DC6**.
* **LV9** (combat detail option per level) builds on the option the speed wave landed (SP8).
* The six creator sets for the levels (CREATOR) follow the list in the levels plan; the LEVELS lead
  says in its status file when a level needs its set.
* A new option row (the Dreamcast stick row is the first known) creates OPT2: whoever lands first.
* **COMBAT** waits for speed audit 4 (every SPEED package on origin) before it changes renderer or combat code; its PS1 resident bytes wait for PS1-RAM / PS1-FLAT and the share in section 3; its Dreamcast side is built by the DC stream (DC6B / DC6C) from COMBAT's spec.
