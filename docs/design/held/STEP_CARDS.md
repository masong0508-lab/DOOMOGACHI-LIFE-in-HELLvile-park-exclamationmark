# Step cards: the held-off work in small, self-contained tasks

Written so that **one card = one session** on a normal model (e.g. Claude Sonnet) with no
memory of earlier sessions. Each card says what to read, what to change, how to check it
and when it is done. Do them in order inside a wave; waves go in the order of
`HELD_OFF_ROADMAP.md` section 0 (speed -> sticks -> Talk -> Dreamcast -> creator -> levels).

**How to use a card:** start a new session in this repo and paste:

> Read CLAUDE.md, then do step card `<ID>` from `docs/design/held/STEP_CARDS.md`.
> Work only on that card. Run its checks. Commit when they pass. Then tick the card's
> box in STEP_CARDS.md in the same commit and stop.

Rules for every card: the hard rules in `CLAUDE.md`; never change files the card does not
name unless a check forces it (then say why in the commit message); if a check fails and
you cannot fix it within the card, commit nothing, and write what failed under the card's
"Notes" line so the next session can pick it up.

Checks used below:
- **CORE** = `make -C core -j8 test` ends "0 failed" and `make -C core check-gen` passes.
- **TALK** = CORE + `make -C core talk talk-check`.
- **GBA** = `make -C gba -j8 && make -C gba test` (skip and say so if devkitARM is missing).
- **DC** = `make -C dc -j8 && make -C dc/tests/gfx2d test` (skip and say so if KOS is missing).
- **PS1** = the build in `docs/PS1.md`, the margin still >= 72 KB below the stack top.

---------------------------------------------------------------------------

## A. Finish what was mid-flight

- [ ] **A1 - Creator CR3 patch.** Read `HELD_OFF_ROADMAP.md` section 6.
  `git am -3 docs/design/held/patches/fut-crc.mbox`. It stops on conflicts in
  `core/src/portrait.c`, `portrait_body.c`, `portrait_head.c`. In each conflict keep the
  current tree's drawing code (retained panels / sliced drawing from the speed work) and add
  only CR3's new gene fields and their reads. `git add` the files, `git am --continue`.
  Checks: CORE, GBA (`make -C gba test-creator` too). Notes:
- [ ] **A2 - Speed wave 2 leftovers.** Open `docs/design/held/SPEED_AUDIT2.md`, find the
  first of S2-4, S2-5, S2-9, S2-10, S2-11 that `ROADMAP.md` does not list as done. Do only
  that one package's GBA part (the audit gives files and the measured cause). Measure with
  `make -C gba test-speed2` before and after; write both numbers in `docs/GBA.md` under a
  new "Speed wave 2: S2-x" heading. Checks: CORE, GBA. Repeat this card per package. Notes:
- [ ] **A3 - Sticks leftovers.** Read `docs/design/held/STICKS_PLAN.md` and `git log
  --oneline | grep -i stick` to see what landed. Do the first unfinished numbered item only.
  Checks: CORE, plus the platform it touches. Notes:

## B. Talk (house conversation) - content first, no emulator needed

Read first, every time: `docs/TALK.md` (what exists) and the safety rules in `CLAUDE.md`.
Content lives in `core/data/talk/grammar/en/*.tlk` and `core/data/talk/voice/en/*.tlk`;
syntax is documented at the top of `core/tools/talkc_gen.py`; look at
`grammar/en/core_warm.tlk` for the style (`@rule name guard: ... act: ...` then quoted lines).

- [ ] **B1 - 150 lines for warm acts.** Add new alternatives to `core_warm.tlk` (compliment,
  comfort, encouragement, jokes, offers). Short, funny, in the PG-21 register; kid speakers
  (`guard: kid_speaker`) stay kid-safe. Checks: TALK, then `make -C core talk-census` and
  note the "Surface" number in the commit message. Notes:
- [ ] **B2 - 150 lines for cool/rude acts** (`core_cool.tlk`): teasing, refusals, arguments,
  walk-outs. No slurs, no real-world hate, swears only when they earn the joke. Checks as B1.
- [ ] **B3 - 150 lines for questions/answers** (`core_ask.tlk`, `core_tell.tlk`). Checks as B1.
- [ ] **B4 - 100 lines for romance** (`core_romance.tlk`): adults only; every rule must keep
  the existing adult guards (copy them from the neighbouring rules). Never add a rule that
  can fire with a minor speaker or listener. Checks as B1 + `test_talk_safety` in CORE.
- [ ] **B5 - cast voices** (`voice/en/cast.tlk`, `tics.tlk`): 20 lines per main cast member
  in their voice. Checks as B1.
- [ ] **B6..B20 - repeat B1-B5 rounds** until the census reaches the targets in
  `docs/design/talk/TALK_DESIGN.md` section 3.7 (Surface >= 1,000,000, Skeleton >= 100,000,
  Meaning >= 5,000, quality failures <= 1%). About 6,000 alternatives in total.
- [ ] **B21 - drama manager skeleton (code).** `TALK_DESIGN.md` section 5 and the T6 row of
  section 9: create `core/src/talk_dm.c` with the beat state and turn-taking only, plus
  `core/tests/test_talk_dm.c` (a scripted 3-person session). Checks: TALK.
- [ ] **B22 - outcomes and save** (`talk_outcome.c`, `talk_save.c`, T6 row). Checks: TALK.
- [ ] **B23 - house Talk mode on native** (T8 row; `core/src/home_talk.c`). Big: split it
  into "enter/leave Talk", "bubbles", "quick replies", "composer" sub-cards as you go.
- [ ] **B24.. - PS1 Talk** (TP0-TP3 rows). Needs the PS1 toolchain and emulator. If it will
  not fit in 72 KB, the owner allows dropping Talk on PS1.

## C. Dreamcast

Read first: `docs/DREAMCAST.md` (sections 7 and 8 show how the overworld and house were done).
- [ ] **C1 - DC6 plan.** Write `docs/design/dc6_plan.md`: how `plat_view3d` will draw through
  the `dc/src/dc3d.h` batches, copying DC7's approach. No code. Check: none.
- [ ] **C2 - DC6 walls and floors.** Implement only the wall/floor pass in a new
  `dc/src/view3d.c`. Checks: DC, CORE.
- [ ] **C3 - DC6 sprites and HUD**, **C4 - DC6 detail option + 30 fps** (each its own card).
- [ ] **C5.. - DC9 full game:** one card per scene group (story, free play, minigames,
  creator), each "make it reachable and run its harness script on `dcrun`".

## D. Creator (1,500+ new parts)

Read first: `docs/design/held/CREATOR_PLAN.md` (sections on CR4 art batches) and
`docs/CREATOR.md`; copy the line format of the existing files in `core/data/parts/catalog/`.
- [ ] **D1.. - one art batch per card**, smallest first: add 20-40 parts of one batch (A1
  face & eyes, A2 hair, A3 spooky, A4 fur & body, A5 outfit, ...) as catalog lines + part
  scripts, regenerate (`make -C core` regenerates; then `check-gen` must show only your
  files changed - commit the regenerated files). Respect the age gate tags (adult-only
  props never on minors; no tobacco/vape/drug props). Checks: CORE, GBA test-creator.
  Note the new live part count in the commit message.

## E. New levels (24)

Read first: `docs/design/levels/LEVELS_EXPANSION_PLAN.md` (section 0 and the package table
at the end) and `core/data/maps/README.md`.
- [ ] **E1 - LV0 level registry** (the LV0 row). Checks: CORE.
- [ ] **E2 - LV1 map format v2**, split per feature (zones; paths; swap sets; texbank).
- [ ] **E3 - LV2 level script VM.**
- [ ] **E4.. - one level per card** in the plan's order (LV12 onwards), each: map `.txt` via
  `tools/mapc.py`, level table entry, its script, a runner test. Dark levels get their
  content note (LV11) first.

---------------------------------------------------------------------------

Tips for a smaller model:
- Read only the files the card names, plus whatever a failing check points at.
- Prefer copying an existing, similar piece (another rule, another level, another part)
  and changing it, over writing from scratch.
- Keep each commit under ~400 changed lines (content cards excepted).
- If a card turns out bigger than one session, split it: write the new sub-cards into this
  file and do only the first.
