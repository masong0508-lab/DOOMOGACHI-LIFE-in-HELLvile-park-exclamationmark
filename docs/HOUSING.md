# The dream house (HOUSING)

The house is one scene (`SCENE_HOME`) over one save chunk (`HOME`). This file
is the implementer's map; the design is `docs/design/story/housing_final.md`
(HF) as amended by `docs/design/story/SPEC.md` (sections 2 and 12 win).
Sections 1-5 belong to the house core (H1), 6-9 to the scene, the art,
visits and HomeMart (H4).

| File | Owner | What |
|---|---|---|
| `core/include/doomo/home.h`, `furn.h`, `house_api.h` | H1 (add-only) | the contract: homes, rooms, stats, grades, phases, the `SCENE_HOME` arg, the chunk sizes, the catalog record, themes / blueprints / layouts |
| `core/src/home.c` | H1 | the state, the `HOME` v2 chunk, room templates, moving in, grants, the cache, photos, the Ch2 haunting |
| `core/src/house_api.c` | H1 | the **resident** getters the story uses (reads and writes `HOME` `.bss` only, SPEC C6) |
| `core/src/home_edit.c` | H1 | catalog access, placement rules, the undo ring, half tiles, recolours, themes, multi-select, the clipboard, blueprints, decorate-for |
| `core/src/home_score.c` | H1 | Comfort / Spook / Style-Harmony / Light / Barricade, the feng-shui rules, the stamps |
| `core/src/home_defend.c` | H1 | Hold the Door |
| `core/src/unlocks_home.c` | H1 | the 20 house trophies (564..583) |
| `core/data/furn/catalog.txt`, `core/data/home/themes.txt`, `blueprints.txt` | H1 | stats, the 10 themes, the 12 ROM blueprints |
| `core/data/furn/art/*.txt`, `core/tools/furngen.c`, `core/gen/furn_*.c` | H4 | the art and the generator |
| `core/src/home_draw.c`, `home_visit.c`, `home_mart.c`, `room_ref.c` | H4 | the scene, visits, HomeMart, the reference renderer |
| `core/data/home/layouts_<pkg>.txt` | the content packages | decorate-for targets (SPEC R19) |

## 1. Rooms and the chunk

Nine room records: the cottage's six rooms (Living Room 6x6 with the hearth,
Bedroom 6x6, Kitchen 8x6, Hell-Cellar 8x8 at light level 6, Garden 8x6 with
no walls, Roof Annex 6x6), Towers 7B, the Hell Pits flat and the post-game
Park Plot (10x8, open). Each room has fixed slots from `home_room_defs[]`:
the door (wall A), up to two windows and the Living Room's hearth. A room
record is 10 B plus 6 B per object (at most 40).

`HOME` v2 (SPEC R9, R10, C9) worst case **3,432 B**: header 8, 9 rooms,
`owned` nibbles 192, `seen_new` 48, 32 gifts, 1000 visited bits, the haunt
16 B, **two saved blueprints** (room records), **16 photo recipes** (8 B
each), wants, the visitor log, decorate-for progress, 4 pending prop gags
and the 16 B cache that `house_api.c` serves. `size()` is exactly what
`write()` emits; before anyone moves in the chunk is 10 B (R13). Object
word 0: furniture id (9 bits), rotation (2), on-surface (1), the half-tile
sub-position (2: +1/2 tile x, +1/2 tile y), 2 reserved.

On the GBA the `HOME` `.bss` stays out of the build until the memory pass
frees EWRAM (SPEC C9: post-G0 >= 12,352 B free; `HOME_PERSIST`). The PS1
and native builds keep it (`HomeState` is 3.4 KB of `.bss`).

## 2. The editor rules

`home_fit()` / `home_fit_sub()` decide; the scene only animates. Floor
pieces may not overlap; rugs live under everything; smalls stack on a
surface at one of four half-tile sub-positions; wall pieces only on wall A
(y = 0) or B (x = 0), never overlapping another in both span and height;
the Garden takes fence pieces only on its edges. Kevin's rule: nothing
stands on the two tiles in front of the hearth and nothing hangs over its
mouth. The door's own tile stays clear. Windows swap into window slots.
Ch3's Shelter pieces: a **welded plate** snaps over a window slot that has
a window (and no plate yet); the **toaster turret** stands on a window
slot's floor tile. `FF_FIXED` decals (the wallpaper eye) are placed only by
the house. Remove-only mode (Interlude III) refuses every placement.

The undo ring (32 steps of 8 B, in the scene scratch, never saved):
`HU_PLACE`, `HU_REMOVE`, `HU_MOVE` (also recolours), `HU_WALL` (the room
header), `HU_TMASK_A/B` (a theme's recolour masks) and `HU_GROUP`, which
closes a multi-entry step so it undoes as one (a group move or rotation, a
paste, a delete to storage, a theme). Switching rooms clears the history,
and so does applying a blueprint (a fresh start).

Multi-select takes the non-fixed pieces inside a rubber band plus the
smalls on them (at most 8). A group moves by an offset (wall pieces slide
along their wall), rotates a quarter turn about the band's centre, goes to
storage, or is copied to the clipboard and pasted where only the spares you
own are placed.

## 3. Themes, blueprints, photos, decorate-for

**Themes** (`themes.txt`, 10): wallpaper and floor patterns with their
paints, the two furniture paints, a ceiling tint and an accent. Applying a
theme recolours only the *free* paint slots (those still on the catalog
default), so a piece the player painted keeps its colour.

**Blueprints** (`blueprints.txt`, 12 in ROM: one per theme, "Nana's
Starter" and "Shelter"; plus two slots saved in `HOME`). Applying one puts
the room's own pieces in storage, paints the walls and places every piece
you own (floor pieces, then wall pieces, then smalls); the rest come back
as ghosts (`HomePlan.ghost[]`, status MISSING or BLOCKED), which the scene
draws as grey outlines with a "Buy at HomeMart" hint. A complete one grants
the "full blueprint" trophy. Every ROM blueprint applies in its own room
with nothing blocked (`home_blueprints_apply`).

**Photos** are 8-byte recipes, never pixels: place (a citizen 0..999 or
1000 + room), view, zoom, camera filter, daylight, three stickers (kind,
x 0..15, y 0..7), the layout's CRC-16 (`home_room_hash`) so the gallery can
say "(the room has changed since)", and a valid bit. 16 slots; the 10th
photo is a trophy.

**Decorate-for** layouts come from the content packages
(`layouts_<pkg>.txt`, ids 1..15). `home_layout_progress()` matches named
pieces (same facing; a wall piece's facing is its wall) or `any ROLE`
pieces within the tolerance and stores `layout << 4 | progress` (0..8) for
`house_api.c`'s `home_decorate_for()`.

## 4. The score

`home_score_room()` (integer, recomputed on layout changes only): Comfort,
Spook, Style (Harmony from Ch4), Light and Barricade (Ch3), with the named
feng-shui rules of HF sec.4.2 reported as `HRULE_*` bits for the Score
panel. Stamps: COSY (Comfort 40, a bed, 6 pieces), LIT (lamps cover 60%;
candles count double), HELD (Barricade 100 and a turret), HOME (8 seats,
both styles, Harmony 60 in Ch4, the Annex furnished) and ETERNAL.
`home_refresh_cache()` re-scores every open room and grants the trophies.

## 5. Story hooks: the haunting and Hold the Door

**The Ch2 haunting** (HF sec.10.2, SF Ep 2.3). The story advances the
night with `home_haunt_step(night)` (resident); the home group applies it
when the house opens (`home_haunt_apply`), once per night:

1. the Picnic Crate is one tile to the left in the morning;
2. the lamp is off, and one heart of the wallpaper is an eye (a fixed
   decal on wall A; it never leaves, it blinks until LIT);
3. two pieces move toward the cellar hatch, and Vex's pentagram rug is
   under the bed, though nobody put it there.

Reaching LIT ends the moves; the eye stays an eye and stops blinking.
`home_haunt_3am()` is the live move the scene plays at 03:00.

**Hold the Door** (`home_defend.c`; HF sec.10.3 retimed by SPEC R11). The
story's `home DEFEND b` picks a wave table: 1 = Ep 3.2 (three imps under
the red sky), 2 = the Ep 3.7 siege (6 imps; 10 imps and a brute at a
window; 14 imps and 2 brutes, the chimney beat, then the 2x PANIC clock),
3 = a Ch3 Night Raid (scrap, the STRY counter), 4 = the post-game Arena.
A bare `HOME_ARG_STORY(HSM_DEFEND)` asks the story (Ep 3.2 done or not).

* Demons walk lanes (the door, each window slot). Imps hit the door
  (1 HP / 16 f, two fit in the frame) until their nerve is spent; a brute
  squashes against a plate every 24 f, a plate takes 10 hits, then the
  glass breaks with the brute stuck in the frame.
* **Door HP = the room's Barricade** (scored in Ch3 terms).
* The player: A repairs the door (+10), re-winds the turret (20 shots, one
  every 12 f at the nearest demon in its window's arc), comforts a guest
  (a calm tick absorbs one scare) or puts a fallen plate back.
* Guests (Pip, Miss Lulla, Nana, Kevin, the Mayor at the window as a
  decoy): a broken door lets imps in and the guests in the room lose
  "safe"; guests near a breached window flee to the Bedroom and stay safe;
  the decoy works once. HELD needs 4 safe; with no input, a scripted
  wave 3 loses exactly the Mayor's (`home_defend_waves`).
* Wave 3 plays the rule of three ("the door is going to give", 15 f
  apart, 1 / 3 / 5 px; the third holds) and the chimney beat.
* Dawn writes the result into `HOME` (the cache the story reads through
  `home_defend_result()`), the best wave and the plates owned.

Every frame count lives in the timing table at the top of each file.

## 6-9. The scene, visits, HomeMart, the renderers

(H4; see below once H4 M2 lands.)
