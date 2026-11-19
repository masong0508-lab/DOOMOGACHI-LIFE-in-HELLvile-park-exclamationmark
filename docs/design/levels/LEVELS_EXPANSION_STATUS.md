# Hell Breach expansion status — first playable-map integration

This status note records the first two map integrations from the 2026-11-08
`LEVELS_EXPANSION_PLAN.md`. It does not replace that plan or mark the full
level packages complete.

## Integrated in the shared host map/level registry

| Level | Source map | Script/text | Main mechanic represented in map data | Current state |
|---|---|---|---|---|
| Hell Breach 8 — Steam Room Panic | `core/data/maps/steam_room_panic.txt` | `core/data/levels/steam_room_panic.txt` | steam/fog zones, valve tags, coolable pools, bounce jets and swap sets | registered as `MAPID_STEAM_ROOM_PANIC`; level table now points at it |
| Hell Breach 9 — The Undercroft | `core/data/maps/the_undercroft.txt` | `core/data/levels/the_undercroft.txt` | dark-room lighting, candle zones, secrets and boss encounter script | registered as `MAPID_THE_UNDERCROFT`; level table now points at it |

Both maps were moved from `core/data/maps_pending/` into the active
`core/data/maps/` directory. Their generated C files are in `core/gen/` and
their v2 extension tables are registered in `core/src/levels_mapx.c`.

## Checks run

- `tools/mapc.py --verbose` compiled both active map sources.
- `tools/leveltext.py` passed for all six Hell Breach text files.
- `tools/levelcheck.py --mapreach core/build/mapreach` passed both maps:
  Steam Room Panic 40x34 / 12,292 B / 789 reachability states; The Undercroft
  34x34 / 9,671 B / 597 reachability states.
- The changed C translation units (`levels2.c`, `maps.c`, `levels_mapx.c`) and
  generated map translation units were reached during the host unit build with
  `-Wall -Wextra -Werror` and compiled without a reported diagnostic before
  the overall build hit the execution time limit.

## Still open — do not treat as finished

- The complete `make -C core test` run did not finish here: the broader
  story/Talk checks exceeded the execution window. The subsequent unit build
  also exceeded the execution window while compiling unrelated later modules.
- No full game runner/autopilot playthrough or real hardware timing test was
  completed in this pass.
- The PS1 build deliberately excludes the expanded level table while its
  resident-memory / disc-pack work remains open. This patch therefore does not
  claim these levels are selectable in a PS1 retail image. The PS1 BREACH.BIN
  streaming/package work and a PCSX-Redux run remain required.
- The plan's boss-specific art/behaviour, reward hooks, news/Talk integration,
  and the 30 fps target still need their own level-package work and platform
  verification.

## Next candidates already designed

`Fright Night at the Fun Fair`, `The Glum Pipes`, `The Wall`,
`Boneyard Buffet & Bandstand`, and `The Department of Contracts` remain in
`core/data/maps_pending/`. Their text files and plan entries are present. The
next low-risk continuation is to run map reachability/budget checks on each,
then integrate the ones whose map mechanics are supported by the current
runtime. `The Wall` and `Fright Night` have additional face/ride traversal
requirements and should not be activated by registry edits alone.
