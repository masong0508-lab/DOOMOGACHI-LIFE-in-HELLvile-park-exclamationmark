# Bone level legibility pass

This map/renderer pass uses **existing art and geometry**: no new sprites, geometry,
textures, framebuffers, or per-frame ray operations.

## Visual guidance

* **Bone Cathedral**: The central carpet aisle brightens (11 to 14) while pews
  darken slightly (10 to 9). Nave stone (10 to 11), doorway (12 to 14),
  narthex (11 to 13), apse steps (12 to 13), and the organ arena
  (11-13 to 12-14) now have distinct brightness zones. Choir-wave trigger
  cells R/Q/V inherit the changed carpet light, preserving their tags.
  Existing stained-glass windows and skull mosaics add ten visual wall
  landmarks along the nave. Their collision, height and floor remain intact.
* **Bone Capital**: Rock paving drops from light 11 to 10, roads/market gore
  paving rises 11 to 14 and gateways rise 12 to 14. Ambient rises 9 to 11.
  Existing skull mosaics and stained glass distinguish twelve wall locations
  near approach roads, market turns and the upper citadel. The GBA's far
  distance fade is capped at 6 instead of 9 on Capital via the existing
  128-entry setup lookup table, with no added pixel or ray work.
* All original map dimensions, placed things, enemy AI, keys, triggers,
  secrets, sky, long view-distance settings and HUD layout are preserved.
  The following conservative GBA pass extends the low-resolution Bone-tier
  textured-floor cutoff from 640 to 768 world units, then still uses the
  texture's average to prevent horizon shimmer. This samples existing
  texture further into the view and **may increase frame time**: a measured
  emulator comparison is required. Actor billboards on Bone maps get a
  minimum light of 8/15 (unless already brighter) to distinguish players,
  enemies and pickups from dark stone without any added draw loops.

## Verification (do not conflate correctness with an FPS claim)

1. `python3 tools/mapc.py --check core/data/maps/bone_cathedral.txt core/data/maps/bone_capital.txt`
2. `make -C core check-gen levelcheck`
3. `make -C gba test-3d-census`, covering first and third person and checking
   >=29 logic TPS independently of drawn FPS.
4. Examine actual ROM screenshots for doorway, aisle/pews, crossing, galleries,
   organ boss, Ossuary Gates, Skull Market, forge, lift and skybridge.
   Inspect damage/low-health frames and both 80x54 Balanced fallback views.
5. Compare drawn FPS, p95/p99 frame time, spikes, navigation/doors/keys/exits.
   Previously documented Bone results were around 16-19 drawn FPS;
   **24 drawn FPS is a target, not a verified result of this pass**.

The checked-in generated map C files must match the authored maps; run
`make -C core check-gen` after subsequent edits.

## GBA fidelity follow-up (current pending validation)

* `gba/src/r3d.c`: keep a little more original floor/aisle/road texture in
  the Balanced 120x80 and 80x54 tiers (far averaging at 768 rather than
  640 world units). This is still substantially less clear than the native
  full-resolution view; do not represent it as native-equivalent.
* Floor and sprite visibility are adjusted without changing geometry,
  collision, map tags, asset count, HUD resolution, view-distance cutoff,
  or the existing FPS safety governor.
* Non-fullbright sprites in the Bone maps retain at least level 8 (of 15)
  lighting to keep active actors visible. Other maps retain their old
  sprite lighting.
* Re-run all four GBA Bone census windows, screenshot walkthroughs, and
  compare the **drawn FPS** and any floor aliasing against the earlier
  640-unit baseline. Revert the 768-unit cutoff if significant FPS or
  shimmering regressions appear; keeping actor visibility is independent.

## Cathedral material correction: avoid a pastel-pink aisle

The original `CARPET` material is reused from the cute-world interiors.
In GBA census screenshots it makes the Cathedral nave a wide, bright-pink
patch when lighted for visibility, overwhelming the old red Gothic stone.
The authored Cathedral now uses its **existing** `GORE_FLOOR` material
on the principal aisle (`c` and choir triggers R/Q/V), the apse stairs
(`a`), the blue-door approach (`U`) and the secret reliquary floor (`r`).
The aisle light is slightly reduced, 14 -> 13; its surrounding
nave tiles and pews remain 11 and 9, respectively, so the route is
still distinguishable without fluorescent color. All heights, stairs,
doors, secret/trigger flags, things and object positions stay unchanged.
Generated map C is updated in the same commit. Because the material is
already in the level and texture cache, this introduces **no new asset**,
render pass, framebuffer or per-frame geometry. This is a *source-code
fidelity adjustment*, not a verified screenshot/FPS success: inspect the
next real GBA ROM capture from the Cathedral start, aisle and apse and
measure its FPS versus the prior screenshots.

## GBA adaptive 96x64 fidelity tier (new, pending CI screenshots)

The older Balanced governor jumped from **120x80** to **80x54**, enlarging
40 distinct ray columns into the 240x160 screen at 3x and causing the
conspicuous blocky skyline/wall texture in GBA emulator screenshots.
The updated Bone-only governor now tries an intermediate **96x64** view
(48 ray columns) before its existing 80x54 emergency fallback. The
96x64 image is enlarged uniformly **2.5x in both axes**, with no warped
aspect ratio or new textures; a direct ARMv4 Mode 4 VRAM routine maps
four input rays to twenty screen pixels in place. No framebuffer,
scratch RAM or IWRAM 3D overlay bytes are added. Existing Smooth,
Max, other Breach levels and the full-resolution HUD are unaffected.

The new middle tier is retained while the render remains within
four GBA vblanks; three consecutive slower frames return to the old
80x54 emergency mode. A cooldown and measured CPU costs prevent rapid
quality toggling; the original 120x80 tier can be restored when time
allows. No claim of 24 FPS or native visual parity is made without
an mGBA run. Verify with `make -C gba test-3d-census`, profile
`r3d_up5x2` cost, check for VRAM overlap and side effects, and compare
equal-tick screenshots using `gba/tools/visual_clarity.py`. The key
regression criteria are clear landmarks, stable movement, correct HUD,
safe VRAM upscaling, and no catastrophic FPS loss.
