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
| `core/tests/scripts/home_*.txt`, `test_home_visit.c`, `test_furngen.c`, `test_home_render.c` | H4 | the runner scripts and tests (section 9) |
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

## 6. The scene (`SCENE_HOME`, `home_draw.c`)

One scene, five modes over one working room (`HomeEdit` and its undo ring):

| mode | buttons |
|---|---|
| Build | D-pad the cursor; A picks up (a tap, on release) or places; hold A on a piece (12 f) to recolour, L / R step paint A / B; L / R turn a held piece (the squash-flip) or the view (10 f of brake, then the turn); B puts a held piece back, else leaves; SELECT taps undo, a held SELECT (8 f) draws the rubber band |
| Live | walk; A uses what is in front: a door goes to the next open room (the iris), lamps switch, beds sleep (the heartfelt fade), seats sit, anything else is examined; on a visit A next to the host asks what they wish for |
| Photo | L / R turn, UP / DOWN the daylight, SELECT the filter, B a sticker (3 at most), A snaps the 8-byte recipe, START is done |
| Defend | Hold the Door: D-pad the cursor, A on the door / turret / a guest / a window slot |
| View | a photo re-rendered from its recipe (the Library's photo wing): L / R the next photo, A / B back |

The START menu: Catalog, Live / Build mode, Score, then (Build) Walls,
Themes, Blueprints, then Photo, Rooms (own home), Leave. A visit's menu is
Give a gift, Photo, Leave (no gift in the sealed room or a vacant flat).

**Arguments** (`home.h`): `HOME_ARG_OWN(k)` (-1 = the cottage),
`HOME_ARG_VISIT(id)`, `HOME_ARG_VISIT_AS(id, kind)` (a visit shown as
possessed / ghost / sealed / away whatever the citizen's state: previews
and runner scripts), `HOME_ARG_STORY(mode) | param << 8` for the story
modes: TUTORIAL (Ep 1.2's checklist: the Picnic Crate, the Egg Nest, a
lamp), GOAL (param = a stamp), HAUNT_NIGHT (param = the night: the
morning's reveal, lights out, the 4-frame hell insert, "...it's fine.",
the 03:00 freeze and the tik), DEFEND (`HOME_ARG_DEFEND(table)`,
`HOME_ARG_DEFEND_AT(table, wave)`), DECORATE_FOR (param = layout id),
OPEN_HOUSE (three friends hop in 8 frames apart and rate the room 15 apart)
and PHOTO (param = the slot). A met goal slams its stamp (or "DONE!") and
hands back after `HT_GOAL_HOLD`.

**Animation.** Every frame count is in the timing table `ht[]` at the top
of the file (SPEC risk 6): the heart iris, the 2-frame anticipation and
lift, the DROP (t^2 fall, the s14 impact, corner dust), the neighbour
ripple (one `TW_JELLY` spring sampled with delays), the nope, the
squash-flip, the view brake, the paint splat and the paint-roller runner
(the covered cut for walls and themes; an iris when nobody is home), the
group DROP stagger, the reverse DROP of an undo, the blueprint montage
(gaps 10 9 8 8 6, then 4, the BLUEPRINT stamp), the expansion rumble
(12 f of crumbling, 8 of silence, then the new floor diagonal by
diagonal), the Score counter and SLAM stamp, the shutter, the reactions
(the double take: look 8, SHOCK 16 with jitter, stare at the lens 8; the
dawning "!", hearts, the bang, the sweat drop, the flee), the pet hopping
between the lamp, the bed's plushie and the nest, idle breathing, and the
open house's arrivals.

**State.** The scene scratch (`'HOME'`, `HomeScn` about 2.9 KB on 32-bit;
the house's budget is 4 KB, SPEC C9); its tail is the view's
`plat_cache`. A scene pushed over the house (HomeMart, the bench, the
Library) takes the scratch; the house re-enters its room from the
remembered arg the next frame it updates or draws, and the undo history
is gone. `SCENE_HOME` is a singleton: never push a house over a house.
Every enter path starts with all effect clocks idle (`clear_fx`). Nothing
is saved except through `home.c` (commit on leave, on room switches and
after story results).

**HUD.** 12-pixel bars top and bottom: the room's name (a visit: "Nana
Puddin, Towers 1-2", the facade's floor-room numbering), the clock, the
purse and the four stat dials; the bottom bar says what the buttons do,
with a shorter line wherever the full one would not fit (`hint()`). The
catalog panel lays out from the bottom up: the price line, the wrapped
description, the sprite preview in what is left (2x when it fits, the
320x240 panel; on the PS1, which links no baked sprites, a maquette of
the piece's prims as shaded iso blocks, `room_ref_maquette()`). Photo and View modes letterbox the bars in black. The
lane strip of Hold the Door counts its lanes from `HomeDefend.lanes` (a
bit per lane), shows the door's HP, the wave, and the turret's rounds
only when there is a turret; at dawn the result card stands alone.

## 7. Visits (`home_visit.c`)

`home_visit_gen(id)` builds a citizen's room from the seed and their
record, never saved (0 save bytes for 1000 homes): the district's set
(Hillvile pastel / Hellvile goth; a Hellborn in Hillvile gets a mix), the
wall paint from `like_color` and its floor partner, 1-2 hobby pieces, a
snack, a band poster in the genre's colour, never a fear trigger (The
Dark always gets a lamp; sometimes the protective piece), the layout from
the personality (tidy along the walls, chaotic, shy with the bed in the
far corner, show-off with the biggest piece in the middle), a framed
candid per married / best-friend edge (2 at most), the tomato board for a
nemesis, then the player's gifts, which override their tiles. Variants:
possessed (bleeding hearts, red, everything facing the wall), ghost (dust
sheets, the night tint), sealed (the departed's room: candles, flowers,
their hobby piece on the bed, no music, no bubbles), away (a note on the
door, the lamp off), vacant (the realtor's sign). Work is counted in
`HomeVisit.ops` (worst room under 60k, `home_visit_time`); the 1000 ids on
two seeds hash to `core/tests/golden/home_visit.txt`
(`home_visit_determinism`, regenerate with `tests --update-golden
home_visit_determinism`).

**Gifts, wants, reactions.** A gift from storage is placed in the
citizen's room (`HOME.gifts`: u16 resident 10 | x 3 | y 3, u16 furn 9 |
rot 2 | paint 5; 32 slots) and leaves storage. Wants are three of 16,
recomputed each week from `(day / 7) ^ id` (The Dark wants light; a fear
of cute things or hugs never wants them). A gift that meets a want raises
friendship by 8; one that meets all three, once a week, earns a thank-you
piece (their hobby's first). The reaction picks one of nine (flee for a
fear trigger, ew for their hated food's snack, the bang for their hobby,
hearts for their colour, sit, nod, double take, dawning, deadpan) with a
polite or blunt line by `PERS_SPEECH`, one per 90 frames. The visitor log
keeps the last four (HomeMart's Wants tab, the neighbours' doors);
50 / 500 / 1000 homes visited are trophies.

**Ways in.** The care screen's "Their home" (`fp_resident.c`), HomeMart's
"Visit a neighbour", a visit's photo in the Library. On the apartment
facade A still opens the care screen (the free-play runner scripts and
players rely on it); "Their home" is one press from there.

## 8. HomeMart and its doors (`home_mart.c`)

`SCENE_HOME_MART` (scratch `'HMRT'`, about 600 B) by its arg:

| arg | what |
|---|---|
| 0 | HomeMart: Today (12 seeded pieces a day, one at -30%, locked rows as "???"), Season, Last Season (3x), Wants (what the last four visitors wished for), Sell (40%) |
| 1 | the Hellvile Black Market: the hell / goth stock; open at night, by day once Chapter 4 is done or in free play |
| 2 | the Workshop's furniture bench (SELECT in `SCENE_WORKSHOP`): 24 recipes of scrap, gears, hellsteel, stuffing, glitter and horns; the weld bar grades S / A / B / C (S: a second copy, A: one material back) |
| 3 | Town Hall's "Expand": the next cottage room for H$ (400, 600, 500, 900, 1200; the story opens its own); the house then plays the expansion rumble |
| 4 | "Visit a neighbour": the visitor log, the best friends, a random door |

Stock and the deal come from `story_rng(HOME_CTX_MART)` (daily, C18). Rows
are cut to fit before the price ("Stained-Glass Wi."); the preview panel
lays out from the bottom (the count, who wants it, or the bench's
materials one per line), then the description, then the sprite. The
house's activities (`doomo_activities_home`: My Dream House, Visit a
Neighbour, HomeMart, the Black Market, Expand the House, Hold the Door)
live in `unlocks_home.c`, a resident file. The raw unlock bits (which
tutorials ran, the Black Market found) are set without a toast.

## 9. The renderers and the platforms

* **Native** (`room_ref.c`): the reference renderer (HF sec.11.5): the
  same `DoomoRoomView` as an iso bitmap from `furngen`'s sprites;
  `test_home_render.c` holds its parity golden (`room_ref.txt`).
* **GBA** (`gba/src/room.c`, H2): the static iso bitmap and OBJ actors.
* **PS1** (`ps1/src/room.c`, H3): a GTE diorama with the backdrop and the
  clipped passes (docs/PS1.md, section 7). H4 adds the resting player:
  the chibi joins the static layer while it stands still.
* **The PS1 overlay group.** Everything of the house is one group,
  `home`, linked over the 3D views' and the arena bot's tables below the
  slot (`ovl_plan.py` BORROW, 45,572 B). Its mode-only code is in PARTS
  that share one range after the common code: `visit`
  (`home_visit.c`), `defend` (`home_defend.c`), `mart` (`home_mart.c`,
  a scene of its own) and `roomtest` (the renderer's harness layouts).
  The common code asks for a part (`plat_code_part(PLAT_PART_*)`, scene.h;
  a no-op elsewhere) before it calls into it, and `home_draw.c` keeps the
  mode's part in `HomeScn.part` so update and draw ask again after
  another group ran. The furniture names and descriptions are packed with
  a canonical Huffman code on the PS1 (`furngen`, `doomo_furn_text()`;
  other platforms read the plain strings).
* **Runner scripts** (`core/tests/scripts/home_*.txt`, both resolutions):
  `home_tutorial`, `home_editor`, `home_rooms`, `home_visit`,
  `home_haunt`, `home_defend`, `home_mart`, `home_gallery`. The runner's
  `home-room R` (move in and open a room), `clock H M` and `money N` set
  the scene up; the PS1 harness plays `clock` and `money` too
  (`home-room` is native-only: it would pull `home.c` resident).
  `ps1/harness/scripts/home.txt` and `gba/harness/scripts/home.txt` hold
  the fps gates.
