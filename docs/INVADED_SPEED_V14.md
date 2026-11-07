# V14: the frame-time governor on every overworld map (2026-11-07) - NOT pixel-exact (owner asked for 30 fps)

Addendum to `docs/INVADED_SPEED.md` (V10 governor). `gov_pick` (`gba/src/overworld.c`) now also runs on the overworld maps
that are not red-hazed, because the story map with dialogue (36.5 ms, 24 fps) and hahaland (33.7 ms, 28.4 fps) are over 30 ms
of work a frame and their sprites are the only part that can be budgeted (`ow_spr_rows` 13% / 35% of the frame).

- Target for those maps: `GBA_GOV_MS_N` = 29 ms (sprites come back below 27). The invaded map keeps `GBA_GOV_MS` = 26.
- Guard: off the red map the governor starts only after `GOV_SUSTAIN` (3) drawn frames in a row over the target, so a single
  spike (pop-in, a body render) never drops a sprite, and a map that fits (park 26 ms max 28.5, hellvile 22 ms) is not touched.
- Same rules as V10: nearest sprite, fullbright sprites and off-screen billboards are never dropped; logic never sees it.
- The invaded map is unchanged: same constants, no wait (`vhash-seq` must stay f87e5596).
- New statics only (3 words + 1 byte in EWRAM); no new stack.

## Gate for the next CI run
- invaded `vhash-seq` f87e5596 (unchanged); park, hellvile, map walk, overworld pause, title and every non-overworld scene keep
  their hashes (if one of them moved, the guard triggered there: raise `GBA_GOV_MS_N` or `GOV_SUSTAIN`).
- story map + dialogue and hahaland change hash on purpose. Expected: both near 30 drawn fps, work avg ~28-30 ms.
- Not checked (no ARM toolchain in the session). If the story map is still under ~28 fps, its sprites hit the `GOV_MIN` floor and the rest
  of the frame (ground blocks ~30 ms of the old 53) is what is left: next lever is `draw_tile` / `ow_tile16`.
