# Bone level legibility

## Current GBA validation

The native Cathedral reference calls for a red approach, skull masonry,
stained glass framing a dark central entrance, and readable goal, health
and ammo text. The Cathedral facade now places its existing stained glass
one tile closer to each side of the arch and lights the red approach
separately from the outer stone. The matched GBA approach captures show
the windows framing the arch later into the walk. The same map tiles
keep their original solidity and height. Capital's navigation cues are
its lit market roads, gates and contrasting stone.

The headless mGBA census passed the existing Bone frame-rate and logic
floors in both cameras. The 96×64 scaler engine test checked every
pixel, including the boundary between 2- and 3-row repeats and the VRAM
guard after the final row. These checks cover the sampled views. Use the
approach, nave, choir, organ, market, lift and skybridge checkpoints
below for a full visual review. The 24 and 30 drawn FPS targets remain
open. See the [measured camera results](GBA.md#current-gba-performance-status).

## Earlier passes

These notes record successive iterations and the test status at the time.

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

## Screenshot ray-resolution audit (report-only)

The GBA census now classifies likely 3D scaling modes from **real PNG
pixels**, using aligned horizontal colour-change boundaries in the center
of the game view. The signatures are 4 pixels per source ray for 120x80,
5 for the new 96x64 middle tier, 6 for 80x54 emergency, and 8 for
60x40 quarter detail. A 240x160 full-resolution ray has a 2-pixel
signature. This does not rely on a claim about how the governor *should*
behave. The reporter also retains the uniform 4x4 block statistic.

Comparing equal-tick archived captures from `3a27b6be` and `528be3d5`
shows nearly all their Bone images have the **six-pixel boundary
signature**, meaning the older actual views were overwhelmingly the
80x54 fallback. In the `528be3d5` screenshots the percentage of
edge transitions on six-pixel boundaries is 88.2% (Cathedral first),
86.6% (Cathedral third), 99.4% (Capital first) and 99.5% (Capital
third) when sampled in the same central viewport region. These
numbers characterize captured pixel spacing, **not** final quality.

Once a new 96x64 build produces screenshots, its Bone census should
report a five-pixel ray signature where the middle tier was reached.
If six-pixel signature still dominates, inspect frame work, cooldown
and the governor; do not claim the new tier fixed blur without
evidence. Keep FPS, ROM integrity and exact-tick scenery inspection
as independent acceptance checks.

## GBA-only compact Bone edition: playable geometry budget

The original Cathedral (48×56) and Capital (64×64) **remain full-size** on
PS1, PC, Dreamcast and other ports. For the GBA only, `map_load_def()`
clips decorative *solid* buttresses, mast, tower/spire roofs above 40 height
steps (320 world units). This lowers **28 Cathedral and 24 Capital solid
cells**: existing heights 46/56/80/112 now stop at 40. Open cells,
reachable floors, stairs, the blue-key path, keys, pickups, boss triggers,
doors, lifts, exits, map widths/heights and actors are unchanged. The
maximum walkable platform is below 40 steps; the change cannot make a
new climbable roof within the 24-world-unit step-up limit. No extra
texture, frame buffer, RAM, or ongoing CPU cost is introduced.

The GBA renderer also limits ray/view horizon in these two maps:
* **Cathedral**: Balanced and Smooth up to 18 cells; 80×54 emergency
  fallback up to 16; Max detail up to 26.
* **Capital**: Balanced and Smooth up to 20 cells; 80×54 emergency
  fallback up to 16; Max detail up to 26.

This is a **deliberate GBA-specific visual simplification**. It can reduce
distant panorama, and clipping distant sprites is consistent with the
ray horizon. Nearby landmark geometry and all navigation remain intact.
It does not change simulation or the maps used by other ports.

### Acceptance test

Build `make -C gba rom`; run `make -C gba test-3d-census` and compare
the same camera/movement windows against the earlier 15–16 Cathedral
and 18–19 Capital drawn FPS baseline. Keep ≈30 logic TPS and test the
blue-key lift, secret rooms, choir waves, organ boss and exits.
Inspect first-/third-person GBA screenshots **from this commit**, including
the Cathedral entrance and interior aisle and Capital gate, market,
lift and skybridge. If the shortened horizon hides important threats or
landmarks, raise that area's limit; do not silently remove encounters.
No FPS or ROM success claim until GBA CI and emulator runs complete.

## Critical GBA assembler correction

The first CI build after adding the 96x64 ARM Mode-4 scaler failed to
assemble `stmdbne r5!, {r8-r12}` in `gba/src/video_loops.s` (GNU as,
ARM7TDMI; `bad instruction`). This is a **build blocker**, not a
measured FPS result. The optional third source-row copy now uses a
normal conditional branch around unconditional `stmdb`, avoiding this
assembler-incompatible mnemonic while preserving 2/3 vertical row
repetition. The replacement was separately accepted by an ARMv4T
assembler before pushing. GBA CI still must build and run the ROM;
validate that the compiled scaler produces correct pixels, no memory
corruption, and retains the 96x64 readability benefit.

## GBA Cathedral sightline pass: favor crisp mid resolution

After the `c15733bc` mGBA census, the Cathedral remained in the
**80×54 emergency** presentation at **15.5 FPS first person and
16.3 FPS third person**, while the Capital reached 96×64 at 17.3/17.4
FPS. The reference image demands recognizable nearby skull stonework,
Gothic windows, towers and a clear central opening rather than a noisy
long-range pattern. In GBA **Balanced and Smooth only**, the Cathedral
ray horizon now caps at **14 cells** (formerly 18); in its emergency
80×54 state, it caps at **12 cells** (formerly 16). Capital retains its
20/16 limits and manual Max retains its existing 26-cell panorama.
The actual authored map and collision/AI ranges are unchanged.

**Test gate:** compare first/third Cathedral drawn FPS, average render
work and actual screenshot ray tier with the `c15733bc` baseline;
look especially for transitions between outside facade, narthex, nave,
windows, crossing and organ gate. Improved numbers are *not* assumed:
if a formerly visible threat, exit or prominent landmark is hidden,
restore that sightline or add a more selective occlusion rule. This
changes GBA view rendering only; PC, PS1 and Dreamcast retain full
artist-authored geometry, textures and distance.
