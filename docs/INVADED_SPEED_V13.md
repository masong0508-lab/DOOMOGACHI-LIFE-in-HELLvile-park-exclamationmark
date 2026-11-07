# Invaded speed V13: static prop data kept in the prop (2026-11-07)

Addendum to `docs/INVADED_SPEED.md` (its own file so it applies on any tree).

## What changed (core only)
`build_billboards()` (`overworld.c`) walked every prop each drawn frame and, for each one on screen, ran a `switch` on its type,
two cell divisions and a `sprite_frames()` call. None of that changes while the props live, so `ownpc_load_props()`
(`overworld_npc.c`) now stores it in the prop (`OwProp`: `cx`, `cy`, `spr`, `fl`, `nfr`; `prop_art()` holds the type -> sprite /
flags table that used to be the `switch`). `t / 5` is taken once per frame instead of once per prop.

Kept live on purpose (exactness): `ow_floor_h()` (a lift can move a cell's floor), the animation frame `(t / 5 + i) % frames`,
`sprite_image()`, and the cull test. The billboards come out in the same order with the same fields.
RAM: `OwProp` grows 6 -> 14 bytes; the 256-prop table is in the scene scratch (6,472 of 12,288 bytes used before, about 8.5 KB
after), so EWRAM is unchanged.

## Checked (host, `make -C core -j8 test`: 873 passed with the temporary dump test, 872 without)
- 135 runs (every debug beat by day and night, plus the three visit maps), 150 frames each with the player walking: all 2,115
  logged frame hashes and the 135 run totals are identical to the tree before this patch (1,605 distinct hashes, so the frames do
  change).

## Not checked
- GBA time. This is a few tenths of a millisecond at most (about 30 on-screen props on the invaded map); it will not move the
  worst frames. The dips left over 33.3 ms (36.1 / 37.6 in the last CI run) are not from this loop: to find them, run
  `make -C gba profile-invaded` with `--worklog` and read which functions the over-budget ticks spend their time in.
- Gate: `make -C gba test-speed` must keep invaded `vhash-seq` f87e5596 and every other scene's hash.
