# COMBAT 3D plan (stream COMBAT)

Owner, 2026-11-13: "start improving the combat system in both ports, especially in the GBA port, making the 3d as
good, coherent and speedy as it can be". Owner, 2026-11-15: "everything must be thirty frames per second".
Written 2026-11-15 by the COMBAT lead (`session_01Xd63cvheL1r2v43wuUsHm6`) while speed audit 4 runs. **No renderer or
combat code has been changed**; this file is the baseline, the renderer spec, the options and the package order.
Status and numbers as they land: `held/status/COMBAT.md`.

## 1. Baseline (measured on tip `dc04212`, 2026-11-15)

Kit: `tools/combat_kit.py` (`python3 tools/combat_kit.py run gba OUT [--modes 0,1,2] [--levels a,b] [--profile N]`;
`gen` writes the same scripts for the PS1 and the Dreamcast harnesses; `table OUT` prints every log). Windows:
the fixed Vape Cellar fight (`push COMBAT 3`, first and third person), and for each of the seven Hell Breach levels
three windows (start room, middle, late) played two ways: **autopilot** (`LVF_DEMO`) and **walk** (a player-like script:
forward held, a turn and a shot every few ticks). All windows are 130 ticks after a settle; tables are
`tools/speed2.py`'s (logic ticks/s, drawn fps, work ms, frames by vblanks, frames over 33 / 50 ms).

### GBA (libmgba 0.10, game ROM, Balanced, `sync on`)

| window | logic t/s | drawn fps | work avg / max ms | frames over 33 ms of 130 ticks |
|---|---:|---:|---|---:|
| Vape Cellar fight, first person | 30.0 | **18.7** | 44.2 / 93.8 | 81 |
| Vape Cellar fight, third person | 29.9 | **18.9** | 41.6 / 116.0 | 82 |
| (Smooth and Max detail: within 0.5 fps of Balanced in both views) | | | | |
| walk, seven levels, 21 windows | 22.4 - 30.1 | **9.0 - 17.6**, median about 14.7 | 43 - 75 | 77 - 128 |
| autopilot, seven levels | **8 - 30** | 3.8 - 17.1 | 41 - 132 | 49 - 76 |

Per level (walk, drawn fps start / middle / late): vape_cellar 15.6 / 15.3 / 14.7, cloud_nine 14.2 / 15.7 / 14.0,
sewers 14.8 / 17.6 / 14.6, factory 13.8 / 15.7 / 12.0, graveyard 11.7 / 15.3 / 9.9, cathedral 10.4 / 14.4 / 9.0,
underbelly 14.4 / 14.9 / 15.6. (The "start" windows start with the level's 0.4 s load frames; ignore their max.)
**Nothing is at 30. The best window is 18.7 fps, the worst 9.0.** The GBA must about double its drawn rate.

### What the profiles say (`--profile N`, harness `profile`, % of CPU cycles; 1 % is about 0.5 ms of a drawn frame)

1. **The autopilot's planner is not the game.** In the autopilot windows `bfs_to_targets` (`core/src/combat_bot.c`, a
   test driver) takes 15 - 34 % of the cycles and drops the *logic* to 8 - 12 ticks/s on four levels. A player never
   pays it. The Dreamcast stream found the same (its "bot plan" ticks of 22 ms). The kit reports the walk windows for
   the renderer and keeps the autopilot windows only as the LEVELS stream's script. The bot should plan in slices
   (LEVELS / COMBAT package G-B1, cheap).
2. **The renderer, fixed fight (first person):** `r3d_flat_loop` (floor and ceiling) 9.3 %, `r3d_column_asm` (walls)
   6.2 %, `vid_blit` (the weapon) 5.6 %, `r3d_up2` (doubling the half-resolution view) 5.1 %, `plat_view3d` set-up 4.4 %,
   `ba_face` (the HUD face) 4.0 %, `combat_bb_sort` 3.4 %, `r3d_columns` 2.4 %, `fx_dist` 2.0 %, `ba_flat` 1.9 %,
   billboards (`r3d_draw_sprite`, `r3d_sprite_loop`) 2.7 %. 13 % is outside known symbols (libgcc, IRQ, the colour
   conversion).
3. **The enemies' logic in a real level is as big as the renderer.** In the walk windows of crowded levels
   `actors_tick`, `demon_try_move`, `fx_dist`, `cw_trace`, `cw_fits`, `map_cell_safe`, `map_floor_z`,
   `__aeabi_ldivmod` and `atan_oct` add up to 20 - 33 % (graveyard 33 %, cathedral 25 %, cloud nine 19 %) against about
   12 % for the wall / floor / sprite loops. **A faster renderer alone cannot reach 30 in these levels.** (The LEVELS
   stream owns what the enemies do; the hot-path cost is shared: package G-L1 below.)
4. The HUD (`ba_face`, `vid_blit`, `vid_rect*`, `out_rect`) is another 12 - 15 %: the face portrait and the weapon
   blit are the biggest single items after the floor.

### PS1 and Dreamcast

PS1 build on the tip: EXE 110,608 B below the stack top (gate 73,728). PCSX-Redux is still being built on this machine;
the PS1 and Dreamcast baselines (`combat_kit.py run ps1|dc`) are added to `held/status/COMBAT.md` when they exist.
Known numbers (docs/PS1.md SP8, 2026-11-12): PS1 fight first person 30.00 fps, third person 29.4; DC (status DC.md):
`COMBAT 0 1 2 4 6` 0 ticks over 33.3 ms at every mode (worst 28.6 ms), the seven breach scripts 6 - 7 ticks over
(scene entry ticks and bot plan ticks). So the **console work is the GBA**; the PS1 and Dreamcast work is the spec and
the parity.

## 2. The renderer spec (all three ports)

One look, whatever the hardware. The reference is the host renderer `core/native/raycast.c` (semantics also written
in docs/GBA.md section 5). A port may draw it with polygons (PS1 GTE, Dreamcast PVR) or columns (GBA), but a screen
shot of the same map, position and angle must show the same things. Each rule has a number so a test can check it.

| thing | rule (the reference's value; a port may differ only where the table says) |
|---|---|
| **Wall** | textured, one texture column per screen column (GBA: per 2 px), `texture_pixels(id, time)` animated, v anchored to world z so steps and blocks line up. Y-side faces one light level darker; step faces one darker. |
| **Floor / ceiling** | textured from the cell's texture, per-cell heights, perspective-correct on the GBA (exact u,v every 4th row), affine-subdivided on PS1 / DC (SP8 levels). Beyond 1400 units the texture's average colour (no shimmer). |
| **Sky** | `sky_pixels(sky, time)` panned with the view angle, 3/4 of the view height above the horizon, sheared with pitch. A cell under the sky with no top is 256 units tall. |
| **Light** | per-cell light (0 = ambient) minus **one of 16 levels per 128 units** of distance, minimum 1; the same 16-row `doomo_colormap` on every port (the Dreamcast may tint instead of table lookups but the 16 steps must be visible). |
| **Fog / draw distance** | `R3D_MAXDIST` 40 cells GBA, 48 reference; beyond it sky or dark fog, never a pop: the last 4 cells fade to the fog colour. Options draw distance and Combat detail move it by the same table on every port (Smooth 20 cells, Balanced 32, Max 40 on a console). |
| **Tint** | the view tint (`tint`, `tint_level`): level / 16 of the pixels in a 4 x 4 ordered dither toward `doomo_tintmap[tint]`; flashes and haze only. |
| **Billboard** | feet anchored, `scale`, `z` lift, `BB_FLIPX`, `BB_FULLBRIGHT`, `BB_TRANSLUCENT` (checker, alternating per frame), light by cell and distance, clipped by walls, steps and lintels, nearest billboard under 4 units skipped. Enemies keep a readable silhouette at 16 cells (see section 4: the outline rule). |
| **Shadow** | `BB_SHADOW`: a darkened ellipse on the floor under the feet, radius from the sprite's width. Smooth keeps only the player's. |
| **Particle / decal** | particles are 1 - 3 px billboards (fullbright); decals (bullet holes, scorch, blood on adults and demons only) are floor and wall quads with at most 8 live per view; the drawing is skipped, never the logic. |
| **HUD** | the 2D layer over the view: face badge, health box, ammo pill, weapon, crosshair. Panels (G7) keep what did not change. HUD is never part of the 3D budget but is part of the frame (12 - 15 % on the GBA today). |
| **Palette / textures** | one 256-colour palette per level, the shared ramp for the 16 light levels, textures 64 x 64 8-bit; zone textures (map format v2 `texbank`, `LEVELS_EXPANSION_PLAN.md`) swap whole texture sets per zone and never change the palette inside a view. |
| **Resolution** | GBA: 120 x 80 columns doubled (Smooth always, Balanced / Max by the governor); PS1 and DC: the viewport at full resolution. A GBA view at half resolution must be the same picture as the reference at 120 x 80 (the 2 x 2 blocks test). |

**Parity tests (every port, the DC7 / DC8 / DC6A way).** `core/tests/` gets a golden-scene table (`combat3d_golden.h`:
for each of 14 views, a map, a position, an angle, a pitch, a tint and a sprite set) that the host renderer draws. Each
port draws the same views and compares: the GBA exact (the raycaster is the reference's algorithm; any difference is a
bug or a documented speed rule: the half-resolution mode and the kept floor), the PS1 and DC by the DC6A metric (share of pixels within 40 / 255,
gate 80 %, and the walls' edge positions within 2 px). The DC gate already exists (`make -C dc v3d-test`, 85.6 %); the PS1
gets `ps1/harness/scripts/view3d_parity.txt` through its `--shot` and a host compare; the GBA gets
`gba/harness/scripts/view3d_golden.txt` using `shot` and `tools/combat_parity.py`. Package G-S1 builds the table and the compare.

## 3. The GBA: options, measured what, and what is still to measure

The frame to beat: 33.3 ms. Today (fixed fight) 44 ms avg of work and 93 ms worst; in real levels 43 - 75 ms avg. So
the target is about **-15 ms avg in the fight, -25 ms in the worst levels, and no frame over 33 ms**. Items below are
ordered by ms saved per week of work. Savings are *derived from the profile above* (a percentage of cycles times
about 0.5 ms), not yet measured by a prototype: the prototype is the first step of each package, and a package that
does not measure its saving in the harness does not land. No renderer file may be touched until "AUDIT4 DONE".

| # | option | what it replaces | derived saving | cost / risk | memory |
|---|---|---|---|---|---|
| G-L1 | **Game logic budget in combat**: slice the bot's BFS (test driver), cache `fx_dist` (one integer distance used for the sort, the cull and the AI), replace `__aeabi_ldivmod` and `atan_oct` in the enemy aim by tables, make `demon_try_move` / `cw_trace` / `cw_fits` check the 3x3 cells around the actor first | the actors' and map's per-tick cost | **5 - 10 ms** in crowded levels (graveyard 33 %, cathedral 25 % of cycles) | touches `actor.c` / `combat.c` (LEVELS hooks: rebase, keep them); same results (hash-sequence compare) | none |
| G-R1 | **Kept floor, ceiling and sky between steps** (the dream's rule, "at 15 Hz kept between steps"): a floor row only depends on the camera; when it moves less than a threshold the previous floor rows are reused for one frame | `r3d_flat_loop` + `ba_flat` 11 % | about **3 - 5 ms** on alternate frames, so avg 2 - 3 ms; visible only as a small swim of the floor texture when walking | a speed trade of the VISUAL TRADE-OFFS list: needs before / after shots; the EWRAM for the kept rows is the cost (not available: see memory) | needs a half-view buffer: 8 KB EWRAM share, exactly the share |
| G-R2 | **Rows of the floor cast at half the horizontal rate** (u,v exact every 8th row, every 4th column pair, the rest linear) | `r3d_flat_loop` | **2 - 3 ms**; texture shimmer risk close to the camera | exact path stays for rows nearer than 3 cells | none |
| G-R3 | **Wall columns at 60 columns (4 px) when moving fast, 120 when still**; sprites always at 120 | `r3d_column_asm` + `r3d_columns`, plus a smaller `r3d_up2` | **3 - 4 ms** while turning; no cost standing | resolution flicker while turning: judged by the owner | none |
| G-H1 | **HUD cost**: the face badge keeps its last picture for 16 drawn frames (no portrait render in the frame), the weapon blit becomes a 3-colour-run blit from a pre-converted sprite, the crosshair / kill count only when changed | `ba_face`, `vid_blit`, `vid_rect*` 12 - 15 % | **3 - 5 ms** | exact steps first, visuals unchanged | a pre-converted weapon strip in ROM |
| G-S2 | **Billboards as hardware OBJs** (affine scaled, 32 x 32 or 64 x 64): enemies and pickups beyond the first row of columns drawn as OBJ sprites by distance; the CPU skips their loops. Walls must clip them: OBJ priority below BG0 layer cannot clip per column, so the wall column pass must leave a mask: **not possible in Mode 4** (one BG layer, OBJ can only be above or below all of it) | `r3d_sprite_loop`, `r3d_draw_sprite`, `combat_bb_sort` | **1 - 3 ms**, but loses per-column clipping behind walls (enemies seen through a door frame) | only valid in a tile mode where walls are BG0 and the near wall strip can be a BG1 mask; otherwise enemies pop in front of walls: **rejected for Mode 4** | OBJ VRAM is the 16 KB texture cache: sprites would take 8 KB of it |
| G-M1 | **Mode 1 hybrid (the tile mode)**: BG2 affine layer holds the floor / ceiling (per-scanline matrix through HBlank DMA: 160 lines x 6 registers, about 1 - 2 % of the bus cycles, no CPU), BG0 holds the wall columns in a **column-major tile layout** (tile id = tx * 20 + ty so a wall column is a constant 8-byte stride: the loop cost per pixel equals Mode 4), sprites remain CPU. | `r3d_flat_loop`, `ba_flat`, the sky, the floor half of `r3d_up2` | floor and ceiling cost **0 ms of CPU** (11 % = 5 - 6 ms) and the 2x of the half-resolution view is free (BG scale: another 2.6 ms) = **about 8 ms** | a new display path: Mode 1, the HUD through the canvas or OBJs, the colormap's VRAM gaps move, the texture cache moves; the floor texture is one 256 x 256 tile map per zone (8-bit tiles, 256 tiles, 64 x 64 texture cells: floors cannot vary per cell, only per zone: **a visual trade-off**); double buffering uses two tile sets of 150 tiles at half resolution (9.6 KB each) | VRAM only; no EWRAM. G-C2 evaluated the HUD-as-OBJ cost; this package carries it |
| G-M2 | **Sector / portal culling per level** (map format v2 zones): the column pass starts from the zone's visible set | `r3d_columns` DDA walk | **1 - 2 ms** in the big levels (cathedral, graveyard) | needs the zone table of LV0; cheap | a 64 B visible table per zone |
| G-A1 | **ARM loop work**: unroll the sprite loop by 8, fold the light table into the wall loop's add, keep `plat_view3d`'s set-up in IWRAM (4.4 %) | `r3d_sprite_loop`, `plat_view3d` | **1 - 2 ms** | IWRAM is full: net zero only if a larger loop leaves | none |

**What is measured and what is not.** The percentages in the table of section 1 are measured on the tip. The savings
above are derived, not measured. Before any of G-R*, G-H1, G-M1 is built, a prototype in a new `gba/proto/` directory (a
test ROM that draws one frame the old way and the new way and prints the cycles) measures it in the harness; the
harness number replaces the "derived" column in this table and in the status file. **G-M1 is the only option that
changes the architecture; its prototype (an HBlank-DMA floor with the column-major wall layer, 120 x 80, one zone) is
the first thing to build after AUDIT4 DONE.** If it measures below about 6 ms saved, the rest of the list (G-L1, G-H1, G-R2, G-R3, G-M2) is the plan.

**Likely outcome (honest).** G-L1 + G-H1 + G-R2 + G-R3 + G-A1 are exact or near-exact steps and add up to about
**12 - 20 ms** in the fixed fight (44 -> 24 - 32 ms: 30 fps reachable there) and **8 - 15 ms** in the crowded levels (55 -> 40 - 47: **not**
30 fps without G-M1 and G-L1 together). Smooth (always half resolution, fewer effects) is the mode that gets
nearest. If after those steps a level is at 24 - 28 fps the plan says so, with the gap and the cost of the next step (G-M1).

## 4. What makes it better (the look, within the budget)

* **Light and shade:** the 16 colormap rows already give distance light. Add a per-zone *ambient colour ramp* (the
  `doomo_colormap` row for a zone is chosen at level load, not per frame): cold blue in the Cloud Nine, warm orange in
  the cathedral. Cost: one table per zone in ROM, no frame time.
* **Textures per zone:** LEVELS' `texbank` (map format v2) already picks the set; the spec asks for four distinct wall
  textures and one distinct floor per zone, and animated ones (lava, vapour) at 4 frames.
* **Enemies readable at distance:** an enemy at 8 - 16 cells is a few pixels. Rule: a 1-pixel dark outline (the sprite's
  silhouette in the darkest ramp colour) on every enemy sprite, drawn by the same sprite loop at no extra pass (a
  pre-built outlined sprite in the sprite sheet: art + 1 ROM copy), and a two-frame idle.
* **Weapon and effects:** the weapon's recoil and muzzle flash (3 frames) stay 2D; hits are 3 - 5 particle billboards
  (fullbright) on adults and demons only; no gore on kids (the rule of `CLAUDE.md`).
* **HUD:** the existing panels stay; the face is the only animated HUD item and is limited (G-H1). A clear crosshair that
  changes colour when an enemy is under it is one rect (already a hit-scan result: free).
* **Third person:** the player's chibi model (PS1) / sprite (GBA) is the same character; no new art.

## 5. Owner choices (visual trade-offs; each needs before / after shots when built)

Defaults chosen by Claude (COMBAT), owner may override. Before shots are the baseline GBA shots in
`docs/design/combat3d/baseline_gba_*.png` (taken by the kit: the fixed fight in both views and four walk windows of the slowest levels); the after shots are added by each package.

1. **Kept floor and sky between steps (G-R1)**: the floor texture updates on alternate frames while the player moves
   slowly; default **on in Smooth and Balanced**, off in Max detail.
2. **Floor texture per zone instead of per cell (G-M1)**: only if G-M1 is built; default **build G-M1 only if its
   prototype saves 6 ms or more**; otherwise per-cell floors stay.
3. **Wall columns at 60 while turning (G-R3)**: default **on in Smooth only**.
4. **Floor detail beyond 3 cells at half rate (G-R2)**: default **on in all modes** (the texture beyond 3 cells is
   already shimmer-limited).
5. **Enemy outline (section 4)**: default **on**.
6. **Draw distance by mode (section 2)**: Smooth 20 cells, Balanced 32, Max 40: default as listed.

## 6. Packages, in order

Round 1 (after AUDIT4 DONE; the three run side by side): **G-L1** (logic budget; `tools/combat_kit.py` walk windows +
`vhash-seq` equal before and after), **G-H1** (HUD cost), **G-P1** (the G-M1 prototype in `gba/proto/`, measured).
Round 2: the options of section 3 that round 1 measured to pay (G-R2, G-R3, G-A1, G-M2, then G-M1 if its prototype said
yes), and **G-S1** (the golden-scene table, the compare tool and the GBA parity script). Round 3: the PS1: the same spec
and parity (`ps1/src/r3d.c`, `view3d.c`; SP8's subdivision levels and the adaptive governor are in place; resident
bytes only inside the COMBAT share of 8 KB, new code in the combat overlay group, the slot is the coordinator's call);
the Dreamcast through the DC stream's status file: the spec, the golden table and the parity gate are theirs to build against.
Every package: core tests, check-gen, `make -C gba -j8 && make -C gba test` and `test-3d`, `make -C gba sizes`
(EWRAM and IWRAM net zero), the PS1 build and its margin, `combat_kit.py` before and after in the commit.

**Targets.** A steady 30 drawn fps with no frame over 33 ms in every combat window of the kit, on all three ports, in
Smooth; Balanced and Max detail with the governor. On the GBA the plan above is the best expected; if the measured end
number is lower it is landed with the gap and the cost of the next step, and nothing below 30 is called done.
