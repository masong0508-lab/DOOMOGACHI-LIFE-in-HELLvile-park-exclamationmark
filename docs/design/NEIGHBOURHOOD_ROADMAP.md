# Neighbourhood roadmap: the walkable street of 1000 houses

Owner: freeplay / levels. Written 2 Nov 2026 after "pick a house, make a family" landed
(`fp_apartments.c`). Status: **plan only, nothing built.**

## The goal in one paragraph
A new free play area beside the island hub: **Home Row**, a fully walkable 3D street of
houses, one lot per resident slot (500 Hillvile, 500 Hellvile). You walk up to a lot, and
it behaves like the apartment room does today: empty lot -> "This house is empty" menu
(Make a family here / Create one person), occupied lot -> that resident's door. The world is
**streamed** in chunks, so it can be 1000 lots long without ever loading 1000 lots. When the
1000-person limit is reached, the surplus lot models vanish and the street is **truncated** to
the length the limit allows, and it grows back as you delete people.

## Interpretation (decided by the owner, 2 Nov 2026)
The street has one lot per slot. "Limit reached" = all 1000 slots are used. Then no empty lots
remain, the "free house" models are gone, and the street's far end is capped with a wall and
gate. Delete a person and their slot becomes an empty lot again at its fixed address, and the
street lengthens to include it.
**Decided: the street is the occupied lots plus a few free lots** (`FREE_LOTS`, default 10, up
to 1000 total), never shorter than one chunk. It does not shrink to occupied lots only.

## Hard facts this plan is built on
| Fact | Where | Consequence |
|---|---|---|
| A map is at most 64 x 64 cells, 8 B a cell = **32 KB** | `map.h`, `DoomoMap` | one chunk = one map; the street cannot be one map |
| `ACTOR_MAX` is 128 | `actor.h` | houses are mostly wall cells; only near ones are billboards |
| 1000 residents, ids 0..499 Hillvile, 500..999 Hellvile | `types.h` | lot address = slot id, no saved layout |
| GBA has ~13 KB EWRAM free | `docs/GBA.md` | GBA gets a lite street (2D strip or a 32 x 32 window); full 3D is native / PS1 / DC first |
| Door cells tagged `STORY_TAG_DOOR + place` open places | `story.h` | lots reuse the door tag, no new interaction system |
| Saves are a fixed budget (`save_budget.h`) | `save_budget.h` | **nothing new is saved**; the street is derived from the town table |

## Design rules
1. **Derived, never saved.** The street layout is a pure function of (slot id, slot status).
   No new save chunk. Same save loads the same street.
2. **Address is permanent.** Slot id -> chunk, lot, side of the street. Deleting a person never
   moves anyone else's house.
3. **Streaming is invisible.** Crossing a chunk seam must not hitch, pop or reset the camera.
4. **Minors and mature rules are untouched.** Houses only show a door; interiors stay in the
   existing scenes (`SCENE_HOME`, resident profile).
5. **Everything runs headless.** The runner can walk the whole street in a script (the test
   suite never needs a screen).

## World layout
- Two **districts**, 500 lots each, joined at the hub gate: Hillvile (pastel, sun) and Hellvile
  (red, lava glow). Matches `RES_DISTRICT()`.
- A chunk is **50 lots** (25 per side of a boulevard), 64 x 40 cells used: a lot is a 5 x 4 cell
  footprint plus a 2-cell front path. 10 chunks per district, 20 total.
- Lots in 10-house blocks of 5 + 5, so address 10 = "floor" and room 1..10 in the apartment view
  map to the same block number. One address everywhere.
- Hub gate -> plaza -> boulevard. Side streets are not needed: 1000 lots stay on one avenue
  per district, which makes streaming a one-axis problem.

## Streaming model
- **Window:** current chunk + the next one in the direction of travel (+ the previous one kept
  for the first second after a seam). Two chunks resident, 64 x 64 cells each, native/PS1/DC.
- **Seam:** a "street cell" strip duplicated at the end of both chunks. On crossing the midpoint
  the runtime swaps `doomo_map` to the next chunk and shifts the player and camera by one
  chunk length in world units. Pure integer shift, no fade.
- **Chunk builder:** `neighbourhood_build(chunk, out_map)` in a new `core/src/street.c`. Reads
  the 50 slot statuses and fills cells + things. No file loading, so no I/O stalls.
- **Level of detail:** lots within 2 chunk-widths are billboards/props (door, window glow,
  mailbox with a name); beyond that a lot is just a coloured wall block. Distant lots keep
  their lit window if someone is home (`act == ACT_HOME`).
- **Budget per chunk:** <= 50 doors (cells), <= 60 things (props + name signs). Stays far under
  `ACTOR_MAX` with the player, pets and NPCs.

## Truncation rule (the limit)
```
extent = last occupied lot + 1 in the district      (0 if nobody lives there)
lots   = min(500, extent + STREET_FREE_LOTS)        STREET_FREE_LOTS = 10
chunks = max(1, ceil(lots / 50))
```
Lot addresses are permanent, so the street must reach the *last occupied* lot (the extent),
not just count people; empty lots in the middle are free lots too. New people take the first
free slot (`town_alloc`), so in practice the street stays compact.
- At 500 of 500 in a district there are no free lots: free-house models are not built, the last chunk is
  cut at the last used lot and ends in a gate wall ("Home Row is full").
- When `used < cap`, the end of the street shows `FREE_LOTS` empty lots, then the end wall.
- Deleting a resident rebuilds the affected chunk on the next visit (chunk cache is invalidated
  by a `town_revision` counter, the same trick the portrait cache uses).

## Phases

### M0 - Contract and tests first (S) - DONE 2 Nov 2026
- `street.h`: `street_chunk_of(slot)`, `street_lot_of(slot)`, `street_length(district)`,
  `street_build(district, chunk, DoomoMap *)`, `street_slot_at(district, cell_x, cell_y)`.
- Tests: address round trip for all 1000 slots; length for used = 0, 1, 499, 500, 999, 1000;
  deleting a person never changes another slot's address.
- Exit: `make -C core test` green with the new unit tests and no map code yet.
- Landed: `core/include/doomo/street.h`, `core/src/street.c`, `core/tests/test_street.c` (6 tests).
  `street_extent/lots/chunks/free_lots/is_full` read the live town; the `_for` forms are pure.

### M1 - One walkable chunk (M)
- `MAPID_HOME_ROW`, a single chunk built by `street_build`, walkable in 3D with the existing
  first-person overworld (`doomo_maps`, raycaster, `story_trigger` door tags).
- Empty lot door -> the "This house is empty" menu from `fp_apartments.c`; occupied -> resident
  profile (`fp_open_resident`). Factor the menu out of the apartments file so both share it.
- Runner script: walk to lot 7, make a person, see the lot change to occupied.
- Exit: both screen sizes, no stubs, screenshots reviewed.

### M2 - Streaming across a district (L)
- Two-chunk window, seam shift, chunk cache, LOD. 10 chunks of Hillvile.
- Test: a script walks the whole district at both screen sizes; a unit test checks the world
  coordinate stays continuous across all 9 seams (no jump larger than one step).
- Profile on native + PS1 build: frame time at the seam <= 1.2 x the average.
- Exit: no hitch at any seam, memory = 2 chunks.

### M3 - Both districts and the truncation rule (M)
- Hellvile district, hub gate, the cap and `FREE_LOTS` rule above, "Home Row is full" gate.
- Tests: fill all 1000 slots in a unit test -> street length matches the formula and no free
  lot exists; delete one -> exactly one lot is free and the gate moves; delete 500 -> length
  halves. Journey: reach the cap, walk to the end wall, delete someone, walk on.
- Exit: the "occupied + a few free lots" rule verified at every size.

### M4 - Hub integration and travel (M)
- A door "Home Row" beside the hub (new place in `story.h`, free play only, hidden in a story
  island until `STORY_CH_DONE`). Return gate, and a fast-travel list: jump to floor / room
  (reuse the apartments START jump list), or "my house" for the avatar's home.
- Walk-in doors: the pet and the family follow you for a short distance (cosmetic, no sim cost).

### M5 - Life on the street (L)
- Residents seen on the street use the sim's real activity (`act`, `ACT_HOME`, away). Only
  the ~10 nearest are actors; the rest are the glowing windows.
- Mailboxes show the family name; a house with an exorcism / problem shows the same icon the
  apartments use (`sim_problem_for`). Night lighting by `town_is_night()`.
- Free will passes keep running unchanged; the street only reads state.

### M6 - Platforms (L)
- Native / PS1 / DC: full 3D as above. Check the PS1 GTE budget with the existing harness.
- GBA: **lite street**: a 2D scrolling strip (the apartments' facade, one block per screen) with
  the same address rules, because EWRAM has ~13 KB free and one 3D chunk is 32 KB. Decide
  after M2's real numbers; a 32 x 32 window (8 KB) is the fallback if it fits.

### M7 - Polish (M)
- House variety by slot seed (roof, colour, porch), the custom creator's colours on the
  owner's house, holiday lights, a street map with the avatar dot, a "find a person" jump.
- Spanish text batch for the new strings. Tone check on any new lines.

## Order and sizing
M0 -> M1 -> M2 -> M3 are the critical path (S, M, L, M). M4 and M5 can run in parallel after M3.
M6 starts with the numbers from M2. M7 last.

## Risks
| Risk | Mitigation |
|---|---|
| Seam hitch on PS1 / DC | build the next chunk over several frames (stage by rows), swap at the seam |
| Chunk rebuild cost when a person is deleted | rebuild only the chunk that holds the slot, lazily |
| Address mismatch with the apartments view | one shared `street_*` address function; test both |
| 1000 lots tempts a saved layout | rule 1: no saved bytes; `test_save_budget` must stay unchanged |
| GBA cannot fit 3D | lite street (M6), same data, different renderer |
| "Disappearing houses" feels like data loss | the gate sign says why; deleted/free lots always return |

## Definition of done
- A script walks every one of the 1000 lots at both screen sizes without a hitch or stub.
- At 1000 people no free-house model exists and the street ends at the last lot; one deletion
  gives exactly one free lot back.
- Save size is unchanged; a load re-creates the same street.
- Works in free play from day one; story islands unlock it after the story.
