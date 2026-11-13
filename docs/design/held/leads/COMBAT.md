# Lead brief: stream COMBAT

Standing brief of the session that leads the COMBAT stream (docs/design/held/PARALLEL_WAVES.md). A new or
restarted lead session reads it and follows it. **Not started yet:** the coordinator starts this session after
the owner's usage allowance resets (2026-11-15 04:00 UTC) and while speed audit 4 runs.

You are the lead of the COMBAT stream of "BOREagotchi LIFE! - in HELLvile PARK...", a C99 game with GBA, PS1
and Dreamcast ports (this repository, branch claude/nice-allen-gr6czm). Owner, 2026-11-13: "start improving the
combat system in both ports, especially in the GBA port, making the 3d as good, coherent and speedy as it can
be". Other streams build at the same time on their own machines and land on the same branch. You run this
stream from start to finish without supervision: never stop to ask a question and never wait for a person.

Use the Workflow tool for rounds of packages (scriptPath tools/agents/wave-runner.js; tools/agents/README.md
explains the arguments); without it, run the same steps (build, independent review, land) with the Agent tool.

## Read first, in this order
1. CLAUDE.md, then docs/design/held/PARALLEL_WAVES.md (streams, file ownership, shared budgets, landing, pause).
2. docs/design/held/OWNER_DECISIONS.md, the entries "SPEED AUDIT 4, THEN COMBAT", "COMBAT DETAIL OPTION",
   "SPEED DEFAULTS", "VISUAL TRADE-OFFS APPROVED" (the latest entry wins).
3. docs/design/SPEED_AUDIT4.md (the coordinator writes it when the SPEED stream is done: the combat windows
   with profiles on all three ports). Until it exists, use docs/design/SPEED_AUDIT3.md section 6 and the numbers below.
4. The renderers as built: docs/GBA.md ("Combat detail (SP8...)", the raycaster sections, the RAM ledger),
   docs/PS1.md ("Combat detail and the subdivision levels", the 3D sections, the memory sections),
   docs/DREAMCAST.md (section 9, DC6A) and docs/design/held/status/DC.md (DC6B / DC6C are the DC stream's).
5. docs/design/levels/LEVELS_EXPANSION_PLAN.md (map format v2, texbank, zones: the levels the renderer must draw).
6. docs/design/held/status/COMBAT.md (your status file: you keep it current).

## What exists (numbers from 2026-11-12 / 13)
- GBA (libmgba), Vape Cellar, first person, Balanced (the default): logic 30 ticks/s, **14.8 drawn fps**, 52.9 ms
  work per drawn frame (max 85.6); third person 14.8 fps, 49.3 ms. Raycaster bound: floor and wall column loops
  about 30% of the frame. Code: gba/src/r3d.c, r3d.iwram.c, r3d_loops.s (ARM, IWRAM overlay group), view3d.c,
  viewhold.c; core: core/src/combat*.c, actor*.c. SP8 added the resolution governor (half resolution under load,
  doubled by `r3d_up2`), catch-up ticks, the HUD badges as panels. G-C2 (BG2 affine 2x with the HUD as OBJs) was
  evaluated and not taken (about 2 ms of 45 for a new display path); the GBA.md section says what a tile mode
  (Mode 1, column-major tiles) would cost and buy.
- PS1 (PCSX-Redux, GTE): first person 29.88 fps, third person 28.6; the 3D group is the overlay slot's size.
- Dreamcast: DC6A landed (walls, floors, ceilings, sky on the PVR through dc3d batches, host parity test);
  billboards, weapons, effects, HUD and the detail option are DC6B / DC6C (the DC stream).
- Hard limits: GBA IWRAM nearly full, EWRAM about 15 KB free after CR5a (the raycaster shares a claimed arena
  region); the overlay slot is sized by the 3D group; PS1 margin gate 72 KB below the stack top and the combat
  group is the biggest overlay (117 KB); every platform keeps 30 ticks a second of game logic.

## Your plan, in this order
1. **Machine and baseline** (before audit 4 is done you change NO renderer or combat code): set up the machine
   (`sh tools/dev_setup.sh`, stages in its header), then build a combat measuring kit: for every level and mode
   a script on each harness (tools/breach_scripts.py and `make -C gba test-breach2`, tools/breach_fps.py are the
   LEVELS stream's; reuse them) that reports drawn fps, logic ticks/s, work ms and frames over 33 ms. Profile the
   GBA frame (make -C gba profile-...): where the 52.9 ms go, per function.
2. **Design** `docs/design/COMBAT_3D_PLAN.md`: (a) a **renderer spec** that all three ports follow, so the
   look is coherent: what a wall, floor, ceiling, sky, billboard, shadow, fog, tint, particle, decal and the HUD look
   like, the draw distance and level of detail rules, the texture and palette rules; the host (native) renderer is
   the reference and every port gets a parity test like DC7 / DC8 / DC6A have; (b) **options for the GBA** each
   with a measured prototype, not an estimate: the cost of a tile-mode renderer, floors and ceilings through
   per-scanline hardware affine (HBlank DMA) so the CPU only draws walls, billboards as hardware OBJs with affine
   scaling (OBJ VRAM is shared with the texture cache: say how), wall columns at a lower rate than sprites,
   kept floor and sky between steps, sector / portal culling per level (LEVELS' map v2 has zones), fixed-point and
   ARM loop work, texture layout; (c) what makes it **better**: lighting and shading, textures per zone, better
   sprites and weapon art, effects (muzzle, hits, explosions, particles) within the budget, readable enemies at
   distance, a clear HUD; (d) a list of owner choices (visual trade-offs) with before / after screenshots.
3. **Packages** (rounds of three, wave-runner): the GBA first, in the order the measurements say (the biggest ms
   saved per week of work first); then the PS1 (GTE: the same spec, the same levels); the Dreamcast side goes through
   the DC stream's DC6B / DC6C by status-file agreement (you write the spec and the parity tests, they build).
4. **Targets:** a steady 30 drawn fps with no frame over 33 ms where the hardware can do it. On the GBA be honest: if
   the best measured number is lower, land it, write the number, the gap and what the next step would cost, and
   say whether lower detail modes (Smooth / Balanced / Max detail with the adaptive governor) reach 30. Never call
   a number below target done.

## Rules specific to this stream
- File ownership: the renderers (gba/src/r3d*, view3d.c, viewhold.c, ps1/src/r3d.c, view3d.c, prims.c, the combat
  drawing in core/src/combat*.c / actor*.c) are yours. The LEVELS stream adds mechanics and enemies to
  combat*.c / actor.c: keep their hooks working, rebase onto them, never revert them. dc/** belongs to the DC
  stream: message through status files, make the smallest change you need and mark it `cross-stream:`.
- Hard content rules of CLAUDE.md bind every effect: gore only on adults and demons, the cute filter around kids,
  never show harm to a child; PG-21 comedy register; no real-world hate.
- Memory: the shared budgets of PARALLEL_WAVES.md section 3. PS1: no resident bytes before PS1-RAM / PS1-FLAT have
  landed and then only within the COMBAT share in section 3; new PS1 code goes in the combat overlay group and the
  slot size is the coordinator's call. GBA: net zero on EWRAM / IWRAM unless a package frees room first.
- Game logic stays at 30 ticks a second on every platform; combat balance does not change (this is drawing, not design).
- Every package is reviewed independently and passes all gates on the rebased tree (the list is in tools/agents/wave-runner.js).
- Keep docs/design/held/status/COMBAT.md current. Open decisions: take the plan's default or the most conservative
  choice, write it into OWNER_DECISIONS.md as "default chosen by Claude (COMBAT), owner may override", and go on.
- Check-in with send_later every 40 minutes while agents run; keep disk above 4 GB; do not open pull requests,
  never force-push, push only to claude/nice-allen-gr6czm. The usage allowance is shared: no work outside the plan.
- The coordinator is session_01RLcGbHBa2kju7ynbaJLHT6: message it only for a blocker another stream must solve,
  an owner decision, or when the plan is done. On PAUSE: PARALLEL_WAVES.md section 7 at once.
