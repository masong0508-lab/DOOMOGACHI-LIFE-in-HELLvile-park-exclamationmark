# Invaded speed V13: static prop data kept in the prop (2026-11-07)

Addendum to `docs/INVADED_SPEED.md` (its own file so it applies on any tree).

## What changed (core only)
`build_billboards()` (`overworld.c`) walked every prop each drawn frame and, for each one on screen, ran a `switch` on its type,
two cell divisions and a `sprite_frames()` call. None of that changes while the props live, so `ownpc_load_props()`
(`overworld_npc.c`) now stores it in the prop (`OwProp`: `cx`, `cy`, `spr`, `fl`, `nfr`; `prop_art()` holds the type -> sprite /
flags table that used to be the `switch`). `t / 5` is taken once per frame instead of once per prop.

Kept live on purpose (exactness): `ow_floor_h()` (a lift can move a cell's floor), the animation frame `(t / 5 + i) % frames`,
`sprite_image()`, and the cull test. The billboards come out in the same order with the same fields.
RAM: `OwProp` grows 6 -> 8 bytes (`spr`, and `art` = frame count in bits 0-5 + flag code in bits 6-7); the cell is `x / DOOMO_CELL_UNITS`.
Scratch 6,472 -> 6,984 B.

## Checked (host, `make -C core -j8 test`: 873 passed with the temporary dump test, 872 without)
- 135 runs (every debug beat by day and night, plus the three visit maps), 150 frames each with the player walking: all 2,115
  logged frame hashes and the 135 run totals are identical to the tree before this patch (1,605 distinct hashes, so the frames do
  change).

## Not checked
- GBA time. This is a few tenths of a millisecond at most (about 30 on-screen props on the invaded map); it will not move the
  worst frames. The dips left over 33.3 ms (36.1 / 37.6 in the last CI run) are not from this loop: to find them, run
  `make -C gba profile-invaded` with `--worklog` and read which functions the over-budget ticks spend their time in.
- Gate: `make -C gba test-speed` must keep invaded `vhash-seq` f87e5596 and every other scene's hash.

## V13b fix: the first V13 broke the GBA tile cache (2026-11-07)
The first V13 made `OwProp` 14 bytes, so `OwScratch` went 6,472 -> about 8,520 B. On the GBA the overworld keeps its full lit-tile
cache and the tile frame only while `gba_big_fits(GBA_BIG_OW)`, i.e. `scene_scratch_used() <= GBA_ARENA_BB - GBA_BIG_OW`
(26,992 - 2,400 - 17,152 = **7,440 B**). Over that, every overworld map ran on half the cache and without the tile frame: other
pixels (vhash-seq moved on every overworld scene), other timings, and `lit_cached` / `draw_tile` / `map_cell_safe` high in the
profile. The host tests could not see it (no arena). `OwProp` is now 8 bytes and `core/src/overworld_int.h` asserts
`sizeof(OwScratch) <= 7440` and `sizeof(OwProp) == 8`, so this fails at build time instead of in CI.
Expected on CI: invaded `vhash-seq` back to f87e5596 and every other scene's hash back to its V12 value.
