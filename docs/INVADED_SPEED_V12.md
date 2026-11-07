# Invaded speed V12: HUD panels under a toast, and the star's bob (2026-11-07)

Addendum to `docs/INVADED_SPEED.md` (its own file so it applies on any tree).

## What was wrong
On the invaded map the "Walk into a nest to fight!" toast (92 ticks) sits over both HUD pills. A panel drawn over by a later
call is spoiled (`gba/src/panel.c`), so the objective pill and the clock were captured, spoiled again, drawn uncaptured for
8 frames, then retried: each capture is about +4 ms, which is the 9-10 tick bump. After the toast, the objective star's bob
(`ui_wave(t, 40, 1)` is in the panel key) changed the key four times every 40 ticks and recaptured the pill each time.

## Changes (core only, no GBA file touched)
1. `ui_toast_over(x, y, w, h)` (`ui.c`, `ui.h`): 1 when the toast that `ui_toast_draw()` draws later in the same frame can touch
   the rect (its panel rect plus a 6 px margin; generous on purpose, a false 1 costs a capture, never a pixel). The toast's
   x is now computed by one helper (`toast_x`) used by both.
2. `draw_objective` and `draw_clock` (`overworld_fx.c`) skip `plat_panel_begin` while the toast is over them and draw the pill
   as before.
3. The objective star is no longer in the panel: its strip (pill coordinates x 4..12, y 1..11, the 9 x 9 star mask at bob -1..1)
   is the panel's hole. `objective_star()` redraws it on every frame, after `plat_panel_end`, in the pill's own order: the
   flat fill, the gloss row (y 2), the star. The bob is out of the key.

## Checked (host, `make -C core -j8 test`: 872 passed, 0 failed)
- New test `panel_invaded_toast_and_star_bob_are_pixel_identical` (map 219, two toasts, 240x160 and 320x240): panel cache on
  vs off identical on every frame, no panel rule broken. Panels on the 240x160 run: 1200 begins / 238 kept / 962 drawn /
  788 spoiled before, 414 begins / 234 kept / 180 drawn / 5 spoiled after.
- Frame hashes of all panel scenarios (visit, language switch, narration, portrait, island, invaded; both resolutions; cache off),
  4,040 frames, are identical between the tree before this patch and after it.
- These are host panel counts, not GBA timings.

## Not checked (no ARM toolchain or libmgba in the session)
- GBA frame times. Expected: the 9-10 tick bumps gone, about -4 ms on the frames that had them.
- That the star redraw's 2D calls stay inside the hole on the GBA's `pn_route` (if one reached outside, the objective panel would
  be spoiled on every frame and never kept: slower, still the same pixels). Look at `make -C gba profile-invaded`: `pn_span`
  should appear on most frames.
- Gate: `make -C gba test-speed` must keep invaded `vhash-seq` f87e5596 and the other scenes' hashes.
