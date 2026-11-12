# BOREagotchi LIFE! — in HELLvile PARK... (repo guide for Claude)

A C99 game core (`core/`) with three console ports: GBA (`gba/`), PS1 (`ps1/`, PSn00bSDK),
Dreamcast (`dc/`, KallistiOS). The pet is "Boreagotchi". Code identifiers keep the old
`doomo`/`DOOMO` names; do not rename them.

## What to work on
Start with `docs/design/HELD_OFF_ROADMAP.md` (the order, status, rules) and
`docs/design/held/STEP_CARDS.md` (small self-contained tasks, one per session).
Owner decisions: `docs/design/held/OWNER_DECISIONS.md` (latest entry wins).

## Hard rules (never relax)
- PG-21 adult comedy register. No sexual content or genital nudity. Anything suggestive
  is adults-only; minors are never in suggestive content.
- Gore only on adults and demons; cute filter around kids; never show harm to a child.
- No real-world hate. Self-harm only with care, never a mechanic or a joke.
- Never weaken `mature_adult_ok` or the kid-cast list. Talk: adults-only romance both
  ways, refusal/deflection with minors, no teen-teen romance; a hug toward a child stays a
  plain hug the child may decline.
- Every screen targets a steady 30 fps on GBA, PS1, Dreamcast (combat excepted).
- PS1: the EXE built with the boot FMV must end >= 72 KB below the stack top
  (`docs/PS1.md`, "RAM fix 2026-11-08"). GBA EWRAM/IWRAM are nearly full: check
  `make -C gba sizes` after any GBA-side change.
- Never force-push. Commit small, test before each commit.

## Commands (run from the repo root)
- Core tests (fast, always): `make -C core -j8 test` — must end "N passed, 0 failed".
- Generated files unchanged: `make -C core check-gen`.
- Talk data: `make -C core talk talk-check`; conversation census: `make -C core talk-census`.
- GBA: `make -C gba -j8` then `make -C gba test` (creator: `make -C gba test-creator`).
- Dreamcast: `make -C dc -j8` (needs `/opt/toolchains/dc`), 2D tests `make -C dc/tests/gfx2d test`.
- PS1: see `docs/PS1.md` (CMake build, harness `ps1/harness/run.py`).
Fresh machine (a new cloud container): `sh tools/dev_setup.sh` installs every toolchain and emulator
(GBA, PS1 SDK + PCSX-Redux, Dreamcast KallistiOS + Flycast; about an hour, stages can be run one by one).
If a toolchain is missing on your machine, run only the core tests and say so in the commit.

## Style
Plain C99, no allocations in hot paths, fixed-size tables. Match the surrounding code.
Docs are plain English, short sentences. Every change updates `ROADMAP.md` when it
finishes a step.
