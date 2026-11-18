# AI Project Status Snapshot — BOREagotchi LIFE! in HELLvile PARK...

**Snapshot date:** 2026-11-18
**Basis:** the supplied source archive and its documentation; no Git history was included.
**Purpose:** a practical orientation for a new developer/AI session. This is a snapshot, not a replacement for the detailed roadmap or stream notes.


## How to read this document

- **Documented complete** means the repository's own status/roadmap says the work is complete. It is not a claim that this audit independently verified every feature on console hardware.
- **Host-tested** means a test or build is described as running in the native environment.
- **Platform-verified** means a build, emulator, or hardware run is explicitly documented. Hardware verification is called out separately.
- **Open** means the documentation says work remains, is partial, needs a decision, or is not verified.
- Dates matter: the main `docs/STATUS.md` snapshot is dated 2026-11-06, while `ROADMAP.md` and `docs/design/held/RESUME.md` contain later entries through 2026-11-17. Prefer those later notes for recent package status, but verify against the actual current tree before resuming any task.

## Project at a glance

The archive contains approximately 2,851 files across 251 directories. It is a cross-platform game project with a shared C core, generated data and content, tools, automated tests, and platform-specific GBA, PlayStation, and Dreamcast work. It also contains show/pilot material alongside the game.


## Documented complete or substantially implemented

These areas are described as implemented in `README.md`, `docs/STATUS.md`, and/or the later `ROADMAP.md` notes. Their completion level varies by platform and should not be inferred from the list alone.


### Shared/native core and game systems

- Shared C game core, scene/UI/input systems, settings/options, unlocks, town state, and CRC-checked saves.
- Citizen roster and relationship data; art, portrait, chibi, sprite, texture, and creator rendering pipelines.
- Free Play/island and town features, citizen creator, apartments/house systems, care and life-simulation systems, family/household systems, pets/Boreagotchi, loss/memorial, play-as/new-life-related work, and New Game+ foundations.
- Hell Breach/shooter, combat/arena, welding/weapon customization, exorcism, story chapters and epilogue, side quests, and ending routes.
- Extra activities documented as present include Fun Fair minigames, Concert Hall, Music Studio, Library, dreams, fishing, cooking, and content-pack infrastructure.
- Automated test infrastructure: native unit tests, scripted journeys/playthroughs, generated-data checks, story/content checks, and platform harnesses.

### Platform status (documentation claims)


| Platform | Documented state | Main caution |
|---|---|---|
| Native runner | Broadest feature coverage; roadmap says 69 scenes and describes a complete tour of game systems. | Native coverage is not proof of console parity. |
| Game Boy Advance | ROM builds and mGBA harness/profile work are documented; some scenes have been optimized. | Multiple heavy scenes remain slow; real hardware controls and performance checks are outstanding. |
| PlayStation (PS1) | Disc build and overlay/RAM work are documented; later notes record successful CI builds and selected harness checks. | Re-run the PS1 harness and verify memory/performance on the actual current tree; real hardware is not established by the archive. |
| Dreamcast | 2D, controls, sound, overworld, house, partial first-person work, full-game scripting/disc/VMU work are documented at varying stages. | It is not yet a complete, verified playable port; later notes still list DC6/DC9/Talk follow-up work and owner approval for Flycast. |


## Open work — recommended priority order

This order is a practical starting point synthesized from `ROADMAP.md`, `docs/design/held/RESUME.md`, `docs/design/HELD_OFF_ROADMAP.md`, stream status files, and `docs/design/INTEGRATION_TODO.md`. It is not a new owner-approved priority list.


### P0 — establish the true build/repository baseline

1. Recover or confirm the exact Git commit and branch the archive came from. The archive has no `.git` history.
2. Compare the current checkout with the status notes before applying any patch; several notes explicitly say the ZIP may omit owner-applied changes or unlanded work.
3. Run the setup and the core verification suite on a suitable environment. This audit attempted `make -C core test`, but the command did **not** finish: it was terminated at the 300-second limit during the `story-check` stage. The output included large `stale`/`missing` Spanish translation reports (the checker labels the run as a rewrite phase and says stale/missing are allowed); downstream unit/script/journey tests were not reached in that invocation. Do not treat this as a clean test run or a confirmed test failure.
4. Inventory and recover WIP patches that the resume documents refer to. The supplied archive does **not** contain a top-level `patches/` or `patches/wip/` directory, even though `docs/design/held/RESUME.md` refers to saved stream patches there. Treat those items as unavailable until recovered from the original working tree or another backup.

### P1 — console verification and performance

- GBA: validate controls on hardware (`docs/HARDWARE_CHECK.md`); re-measure RAM and speed on the ROM. The documented heavy scenes include the island hub, Town Hall, combat, pet, concert, title, and invaded map. The 30-fps goal is not achieved across all scenes.
- PS1: re-run the harness and check the current RAM/overlay budgets, config/save behavior, family UI, and recent module changes; obtain emulator/hardware evidence rather than relying on older notes.
- Dreamcast: complete remaining first-person/full-game port stages, build/run the relevant tests, and obtain Flycast approval/verification. Documentation is explicit that a full verified playable port is not yet established.
- Speed work: follow the current profiling evidence, not old estimates. `docs/design/held/RESUME.md` identifies known sub-30 GBA windows and unfinished SPEED/SPEED-2/SPEED-3 tasks; 30 fps is the standing target in those notes.

### P2 — life simulation and multiplayer

- New Life from the title screen and in a fresh world (the current character-switcher flow is not the same feature).
- Co-op hero content: story lines, quests, abilities, final names/looks for the named heroes; some entries are documented as placeholders.
- Local simultaneous multiplayer and helping in another world (MP1 remainder).
- Story-house unlock definition (owner decision required).
- Household/pet/character polish: uplifted pet look, baby outfits/body, elder stoop, graveyard presentation.
- Hand-written disaster scripts and owner tone review.

### P3 — Music Studio and Concert completion

- ST-TOOLS / remaining editing features and seven helper tools, as tracked in `docs/design/held/RESUME.md` and `docs/STUDIO.md`.
- ST-ADV: finish remaining playback/notes details and verify GBA, PS1, and Dreamcast builds/sizes. Some Advanced-format, ORDER, Mix, and Expert features are documented as landed; other items remain incomplete or device-unverified.
- ST-USE: user songs in Concert Hall/PERFORM and presentation cleanup.
- Concert performance: `CONCERT-30` is documented as partial; remaining band/Dreamland/choir windows and performance trade-offs are tracked in WIP findings/status notes. Confirm those WIP files exist before relying on them.

### P4 — creator/customization expansion

- Finish the remaining customization P0/P1/M2/M3/P2-P6 steps described in `docs/design/customization/`.
- Deep portrait fields and painters are not all selectable in the live creator/save/Look Book flow; documentation explicitly calls some implemented painters **not playable**.
- Complete remaining part-list appends, art review, creator integration, and the planned expansion toward 1,500+ parts.
- Confirm the current gene/roster repack status before repeating work; roadmap notes mention owner-applied patches that may not be in the supplied ZIP.

### P5 — Talk, story, levels, and localization

- Talk system follow-ups, including multilevel composition/action replies and PS1/Dreamcast support/cleanup, while respecting the frozen line-bank and content rules.
- Story/script packages still listed as open: W9-W12, W15 (approved but unwritten in the resume notes), W16; story expansion and Feast scene remain open.
- Level expansion: plan describes 24 new Hell Breach levels (main game plus New Game+ arc) with platform-specific scope; packages are not complete in the resume snapshot.
- Localization: re-translation pass for rewritten text; Portuguese-Brazilian, French, German, Italian, Dutch, and Polish text are listed as untranslated; Chinese/Japanese need a new font path.

### P6 — integrations and housekeeping

- Integrate Concert and Studio song systems (`SCENE_CONCERT` / `DoomoSong` / song player / Edit in Studio) as noted in integration TODOs.
- Resolve generated-data/check and story-lint findings against the current tree; do not blindly regenerate or rewrite language files without understanding the checker mode.
- Refresh the README's player-facing status: it currently presents a broad feature-complete console build, while later README text also describes game builds booting to a developer scene launcher with placeholder scenes. The status needs a precise distinction between native-runner coverage, console build coverage, and verified console gameplay.
- Decide repository asset policy for large concept-art and trailer assets (owner decision listed in resume notes).
- Review old branches/patch names and stale status records once Git history is recovered.


## Owner decisions / approval needed

- Define the story-house unlock.
- Tone-review the hand-written disaster scripts.
- Approve/verify the Dreamcast emulator workflow (Flycast) as described in the resume.
- Confirm hardware/emulator testing availability for GBA and PS1.
- Resolve remaining story/NG+ ending decisions listed in the resume's owner questions.
- Clarify the “huggle” wording in the intro-redo notes and the GBA rent default, if those decisions are still current.
- Decide whether large concept art and trailer media should remain in the repository.


## Source documents to read first

1. `docs/design/held/RESUME.md` — latest session resume and stream-by-stream open work (dated 2026-11-15/16).
2. `ROADMAP.md` — main project roadmap; its later appended entries extend beyond the header refresh date.
3. `docs/design/HELD_OFF_ROADMAP.md` and `docs/design/held/OWNER_DECISIONS.md` — held work, owner decisions, and restart rules.
4. `docs/design/held/status/*.md` — stream status, when present and current.
5. `docs/STATUS.md` — older status snapshot (2026-11-06); useful context but not the newest source.
6. `docs/NEXT_SESSION_PROMPT.md`, `CLAUDE.md`, and `AI_PATCH_PROTOCOL.md` (if created) — contributor rules and session handoff.
7. `docs/design/INTEGRATION_TODO.md` — older integration risks; verify each against later work before acting.


## Next session checklist

- [ ] Confirm the archive's branch/commit from the original Git checkout.
- [ ] Recover missing WIP patch folders before attempting work described as saved.
- [ ] Read `CLAUDE.md` and `docs/NEXT_SESSION_PROMPT.md` for owner rules.
- [ ] Reconcile this snapshot with the latest stream statuses and actual code.
- [ ] Run the core suite to completion; separately run generated-data, story, SAN, xcheck, GBA, PS1, and Dreamcast gates that are available.
- [ ] Pick **one** bounded task, record its baseline, make a patch, and update this status file when the patch lands.


## Audit limitations

- No Git metadata was present in the supplied ZIP, so commit identity, branch state, untracked changes, and patch applicability cannot be established.
- This audit read the main status/roadmap/resume documentation and inspected repository structure. It did not independently execute every platform build, emulator, hardware test, gameplay path, or content check.
- `make -C core test` was attempted on 2026-11-18 and timed out after 300 seconds during `story-check`; the output reported stale/missing Spanish strings, but that checker identified itself as a rewrite phase where stale/missing are allowed. No later test stages ran in that invocation.
- Dates and counts in older documents can conflict. Confirm any status before changing or marking work complete.
