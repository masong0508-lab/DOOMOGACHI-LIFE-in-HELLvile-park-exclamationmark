# City system: navigable, physically grounded and capacity-aware

## Implemented now (2026-11-20)

The Free Play illustrated City Hub keeps its established visual art. Both apartment
landmarks provide **Walk the 3D district** (starting on the real pavement outside
their corresponding apartment doors), and **View true street map**. START > City
Atlas works from anywhere on the illustrated Free Play hub. The Atlas samples
the same `DoomoMap.cells` that the 3D renderer and collision engine use. The map
cannot drift from the actual walkable streets. A enters that area's 3D city;
B/START returns to the Free Play hub, and SELECT+B returns directly from 3D.
A 45-tick START+SELECT emergency exit unwinds nested screens to the original
game hub and works through modal UI. No save format changes.

In City Atlas, SELECT opens the **Home Row Directory**. It visualizes 50
permanent lots at a time (five numbered ten-home blocks), over ten chunks in
each district. There are 500 stable address identities per district, with
the same `street_slot`, `street_block` and `street_room` functions as the
original apartment facade; A opens the selected household/room. This is a
functional, scalable **address browser and 2D block preview**, *not yet* a
continuous walkable 3D rendering of all 1,000 separate homes.

## Limits and truthful scope

- **All ports:** 500 permanent address identities Hillvile + 500 Hellvile.
- **GBA:** 250 simultaneously live resident records per district, 500 total.
  Empty addresses remain visible in the directory, but cannot accept another
  resident when the corresponding live district is full.
- **PS1, Dreamcast and native:** up to 500 resident records per district,
  1,000 total.
- **Households:** the proposed 20-member household and 24 resident-household
  records in `docs/design/HOUSEHOLDS_PLAN.md` are a separate *held plan*,
  not a shipped multi-household implementation. Don't represent 500 separate
  family groups as an existing runtime feature.
- **Rendering:** the Atlas keeps one live 64×64 map buffer (the same one the
  engine already uses), merges same-color cell runs, and spawns no residents;
  the Home Row Directory paints at most 50 light facades at a time. No
  per-address saved geometry, extra framebuffer, heap or 1,000-object actor
  array is introduced.

## Next expansion gates

1. Create a shared, tested street-coordinate conversion binding permanent
   addresses to an exact block/lot in 3D and the directory, including
   door/interior connections and a physical return gate.
2. Build one actual 50-lot world chunk from `street_slot`; use the existing
   64×64 `DoomoMap` storage and benchmark CPU/RAM/PS1 overlays.
3. Cross chunk seams with stable world/camera coordinates; make all 20 chunks
   addressable without residency spikes. GBA receives a cheaper visual
   representation with identical addresses if profiling forbids full 3D.
4. Integrate actual households from the held household contract (family roles,
   20-person occupancy, household limits, safely versioned saves) only after
   the unit and RAM budget gates pass.
5. Ensure B/START recovery from every new UI panel and profile both rendered
   FPS and 30 Hz simulation ticks. Add screen-level regression scripts.

## Engine guardrails

Do not allocate a second live map, static 1,000-house geometry table, or extra
framebuffers. Keep 240×160 compatibility, IWRAM stack guard, current save
budgets, and same map IDs. Preserve the GBA 30-drawn-FPS target without claiming
it has been reached before mGBA measurements.
