# CONCERT-30: trial of the band instruments through dream_ellipses / dream_circles (2026-11-18, NOT KEPT)

Result: **no gain, not landed.** Recorded so nobody repeats it. Context: `CONCERT-30-findings.md` (the
"Re-measure and instrument experiments" section, if that patch is in) names this as the first thing to try.

## What was tried
`guitar()` (the body's three shapes) and `drum_kit()` (two toms, the bass drum's two circles) in
`core/src/concert_gfx.c` drawn through `dream_ellipses` / `dream_circles` (concert_int.h; the same pixels as
`ui_fill_ellipse` / `ui_fill_circle`, one `plat_hshapes` ARM call per batch on the GBA) instead of
`ui_fill_ellipse` / `ui_fill_circle`. The guitar's two body-colour shapes went in one call (a circle is an ellipse with
rx == ry); the drum's two toms in one call; the dark and the white drum circles one call each, in order. The heart,
cymbals, stands, neck and keyboard were left alone. The diff is at the end of this file.

## Measured (libmgba 0.10.3, `gba/build/harness`, band-only script = `concert_profile.txt` cut after `fps concert band`,
## same machine, same script, ROM rebuilt each time; deterministic: two baseline runs were identical)
| ROM | band drawn fps | work avg | work max |
|---|---|---|---|
| unchanged tree (two runs) | 18.8 | 41.0 ms | 58.5 ms |
| with the change | 18.5 | 40.8 ms | 57.2 ms |
That is 0.2 ms on the average, inside what a frame-pair rounding can move; the drawn fps went down a little. The
instruments' ~7.4 ms (see the findings file) is not in the ellipse and circle calls.

## Exactness: not established
The band's `vhash-seq` differs between the two ROMs (`ca126a68` vs `f95ff95e`). That alone proves nothing: the two
ROMs draw a different set of video frames (18.8 vs 18.5 fps drawn), and the hash covers the drawn frames (see "Not run:
a frame-hash proof on the ROM" above). `core/tests/test_concert_gfx.c` (576 golden frames) was NOT run on the change.
If this route is ever taken up again, run that test first.

## Why I think it did nothing (a reading, not measured)
`ui_fill_ellipse` already draws one `out_rect` per distinct half-width (`nested_bands`), so a small ellipse is 2-5
rect calls; `dream_ellipses` has to build a half-width table per size (`ui_ellipse_dx`) and fill a `DoomoHShape`
before its one ARM call, which costs about what the saved calls do at 2-6 shapes. Batching pays in the dreams, where
dozens of one-size circles share one table; the band draws a handful of different sizes once each.
So the cost is elsewhere in the instruments: the many `plat_rect` / `plat_line` calls (cymbals, stands, neck, head,
keyboard keys), `ui_heart_flat`, or `ui_darker` / palette lookups. Not yet measured; per-call cycle marks around each
instrument would say.

## What this leaves
* Drop this route. Next: take the instruments' cost apart by call type (a temporary `gba_cycles()` mark around
  `drum_kit`, `guitar`, `keyboard` and, inside them, the ellipses vs the lines vs the rects), or go straight to the
  kept picture (one blit per instrument, the lines and rects baked in too, drawn in painter's order).
* The 51-58 ms spikes are untouched by any instrument change (they were still there with all four instruments
  removed), so they need their own look with `--worklog`.

## The tried diff (for the record)
```diff
diff --git a/core/src/concert_gfx.c b/core/src/concert_gfx.c
index a9ad732..9a50dea 100644
--- a/core/src/concert_gfx.c
+++ b/core/src/concert_gfx.c
@@ -1159,9 +1159,17 @@ static void guitar(int x, int y, uint8_t body, int strum, int bass)
     plat_line(x + 3, y - 1, x + 3 + len, y - 6 - strum, neck);
     plat_line(x + 3, y, x + 3 + len, y - 5 - strum, neck);
     plat_rect(x + 2 + len, y - 8 - strum, 4, 3, PAL_RAMP(RAMP_WOOD, 1));
-    ui_fill_ellipse(x - 3, y + 1, 6, 5, d);
-    ui_fill_ellipse(x - 3, y, 5, 4, body);
-    ui_fill_circle(x + 2, y - 1, 3, body);
+    /* (CONCERT-30) the body's three shapes through dream_ellipses (the same pixels as ui_fill_ellipse / ui_fill_circle;
+     * on the GBA one ARM call per colour): the dark one first, then the two of the body colour (a circle is an
+     * ellipse with rx == ry; one colour, so their order does not matter) */
+    {
+        int16_t dark[4], lit[8];
+        dark[0] = (int16_t)(x - 3); dark[1] = (int16_t)(y + 1); dark[2] = 6; dark[3] = 5;
+        lit[0] = (int16_t)(x - 3); lit[1] = (int16_t)y; lit[2] = 5; lit[3] = 4;
+        lit[4] = (int16_t)(x + 2); lit[5] = (int16_t)(y - 1); lit[6] = 3; lit[7] = 3;
+        dream_ellipses(dark, 1, d);
+        dream_ellipses(lit, 2, body);
+    }
     plat_rect(x - 4, y - 1, 3, 3, d);
     plat_rect(x - 6, y - 2, 2, 1, ui_lighter(body));
 }
@@ -1177,11 +1185,19 @@ static void drum_kit(int x, int feet, int scale, const StageView *v, uint8_t c)
     plat_rect(x + 4 * scale, feet - 17 * scale + (v->snare > 2), 8 * scale, 2, PAL_PAINT(PAINT_HONEY, 2));
     plat_line(x + 8 * scale, feet - 17 * scale, x + 8 * scale, feet, PAL_RAMP(RAMP_STEEL, 4));
     /* toms */
-    ui_fill_ellipse(x - 6 * scale, feet - 13 * scale, 4 * scale, 2 * scale, d);
-    ui_fill_ellipse(x + 6 * scale, feet - 13 * scale, 4 * scale, 2 * scale, d);
-    /* bass drum with a heart */
-    ui_fill_circle(x, by, r, d);
-    ui_fill_circle(x, by, r - 2, PAL_PAINT(PAINT_WHITE, 2));
+    {   /* (CONCERT-30) both toms in one dream_ellipses call (one colour) */
+        int16_t tom[8];
+        tom[0] = (int16_t)(x - 6 * scale); tom[1] = (int16_t)(feet - 13 * scale); tom[2] = (int16_t)(4 * scale); tom[3] = (int16_t)(2 * scale);
+        tom[4] = (int16_t)(x + 6 * scale); tom[5] = (int16_t)(feet - 13 * scale); tom[6] = (int16_t)(4 * scale); tom[7] = (int16_t)(2 * scale);
+        dream_ellipses(tom, 2, d);
+    }
+    /* bass drum with a heart (the circles in order: the dark one, then the white one over it) */
+    {
+        int16_t c0[2];
+        c0[0] = (int16_t)x; c0[1] = (int16_t)by;
+        dream_circles(c0, 1, r, d);
+        dream_circles(c0, 1, r - 2, PAL_PAINT(PAINT_WHITE, 2));
+    }
     ui_heart_flat(x, by + 1, 2 + scale + (v->kick > 2), c);
 }
 
```
