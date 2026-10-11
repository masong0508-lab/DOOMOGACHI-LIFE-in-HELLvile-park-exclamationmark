# Roadmap: designed, not yet built

Everything in the game today is listed in README.md. This folder holds the
design work for the next wave, written by the design passes on 2026-10-11
before implementation started. Work was paused by the weekly session limit.
Start here when resuming.

## 1. Face / body customization + options ("a shitton of options")
- `customization/current_map.md`: every DoomoLook field, consumer and budget.
- `customization/wishlist.md`: 120+ customization items, 150+ options.
- `customization/design_packed.md` (keep residents at 96 bytes with a
  bit-packed look codec) and `design_grow.md` (grow the record and pay for
  it elsewhere). The judge/synthesis step never ran: pick one (packed is the
  lower-risk default) and write the work packages listed at the end of each.

## 2. Deeper story, 80+ side missions, dream house, comic timing
- `story/story_map.md`: the current four-chapter story beat by beat.
- `story/comic_timing.md`: the trailer's timing turned into a reusable
  tween/beat API (springs, squash & stretch, smash cuts, holds, scratches).
- `story/housing_tech.md`: dream-house tech. GBA cheap isometric, PS1 2.5D.
- `story/bible_horror.md` (complete) and `bible_heart_partial.md`: the two
  competing story bibles. Merge: heart arcs + horror-comedy escalation.
- `story/GRUMBLEGUTS_AND_DIRECTOR_NOTES.md` + `refs/`: director's notes that
  MUST be honoured: Gatekeeper Grumbleguts' talking belly, big-butt devil
  design (references in refs/), hell's-biggest-flirt personality with its
  hard boundaries, the cutscene gags, and the post-game quest "The Feast of
  Rebuilding" with its permanent-death branch.

## 3. Dynamic chiptune music with record scratches
- `music/engine_map.md`: the current sequencer and both backends.
- `music/direction.md`: leitmotifs, full cue sheet, non-repetition
  techniques. Requirements: chiptune identity, push GBA (4 PSG + 2 mixed
  DirectSound channels) and PS1 (24 SPU voices + reverb) to their limits,
  simulated record scratches as a real DSP on both consoles.

## 4. Rendering speed
The user measured a very slow PS1 title screen in DuckStation. A speed pass
targets 60 fps 2D on PS1 / 30 on GBA; see docs/PS1.md and docs/GBA.md for the
numbers that landed, and `INTEGRATION_TODO.md` for the open items (PS1 black
skies, third-person camera clamp, GBA harness scene commands).
