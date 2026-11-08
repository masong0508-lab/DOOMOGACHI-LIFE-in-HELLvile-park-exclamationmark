# The citizen creator (`SCENE_CREATOR`)

The heart of the "immense customisation": every citizen of Hellvile Park,
the story protagonist included, is made or made over here. Owner: the
creator module.

| File | What |
|---|---|
| `core/src/creator.c` | the scene: state, input, rows, undo timeline, pickers, randomiser, presets, locks, nicknames, writing into the town |
| `core/src/creator_draw.c` | all drawing: tab strip, preview stage, option list, pickers, START menu, copy picker, turntable, the "moving in!" finale |
| `core/src/creator_data.c` | const tables: tabs and rows, 78 species presets (Turkey and Animatronic are the show canon's), 20 outfit sets, 12 voice types, personality texts, tab icons, the creator's unlockables |
| `core/src/creator_grid.c` | the picture grid for part rows (CR6): filters, search, try-on, locked parts |
| `core/src/creator_int.h` | private glue between them |
| `core/include/doomo/part_icons.h`, `core/src/part_icons.c`, `core/tools/part_icons.c`, `core/gen/part_icons_*.c` | the prerendered part icons (CR7), below |
| `core/tests/test_creator.c`, `test_part_icons.c`, `test_panel.c` (creator case) | headless tests (33 + 3 + 1) |
| `core/data/parts/catalog/`, `core/src/part_meta.c`, `core/src/part_age.c` | the part catalog (CR1) and the age gate for parts (CR12), below |
| `core/tests/scripts/creator.txt`, `creator_hellvile.txt`, `creator_grid.txt` | runner walkthroughs (65 screenshots per resolution) |
| `gba/harness/scripts/creator_scroll.txt` (`make -C gba test-creator`), `creator_profile.txt` | GBA speed gates and profile (CR9) |

## Opening it

```c
scene_push(&SCENE_CREATOR, arg);
```

| `arg` | Meaning |
|---|---|
| `>= 0` | edit resident `arg` in place (a makeover). If the slot is empty, create a new citizen right into that slot |
| `-1` | new Hillvile (good side) citizen; the first free slot of the district |
| `-2` | new Hellvile (bad side) citizen, hellborn |
| `-3` | the story protagonist, `town_res(DOOMO_PLAYER_ID)` |

When the player finishes or cancels, the creator pops itself (or, if it is
the only scene, goes to `SCENE_TITLE`). Afterwards:

```c
int  creator_last_result(void);       /* id written, DOOMO_PLAYER_ID, or -1 = cancelled */
const char *creator_nickname(int id); /* "" if none */
```

(both declared in `scenes.h`).

## The tabs

L / R switch tabs (icons in the strip, the name in the list header). Every
row edits exactly one byte of a draft `DoomoResident`, so *every* field of
`DoomoLook` and every identity field of `DoomoResident` is editable (the
test `creator_rows_cover_every_field` checks it). Section captions
("~ Eyes ~") group long tabs.

| Tab | Rows |
|---|---|
| Species & Body | species, species preset, body, height, build, skin / fur / goo / plating (8 natural tones + 32 paints) |
| Head & Face | head shape, head size; eye style, colour, size, spacing, height, tilt, count (1-3); brows, brow height; nose, nose size, mouth, mouth size, fangs; ears; the 8 cheek flags (blush, freckles, scar, bandage, beauty mark, tear tattoo, dimples, sparkles) |
| Hair | style, colour, highlight (or none), "match fur", facial hair |
| Supernatural | horns, horn colour, horn size; tail, wings, halo; marking, marking colour; aura |
| Outfit | outfit preset; top, top colour, print, print colour; bottom + colour; shoes + colour; hat + colour; accessory + colour |
| Voice | voice preset; pitch, speed, quality, intonation, accent (0-31); "Test it!" plays the catchphrase as voice babble in a speech bubble |
| Personality | movement, speech, expression, attitude, quirk (0-15) with a live panel: personality type name, description, quirk level and how they say hello; Age (Adult, Baby, Toddler, Child, Teen, Elder: the saved life stage, see ROSTER.md. Never randomised; the story hero is locked to Adult/Elder, and a premade citizen who keeps their roster name cannot be made older than canon) |
| Profile | name and nickname (on-screen keyboard), pronouns, who they crush on, birthday month and day, favourite food, hated food, hobby, music, favourite colour, fear, catchphrase |
| Home | moves to Hillvile / Hellvile (new citizens only, and in the story only once the wall is down), born in (heritage, `RF_HELLBORN`), favourite (`RF_FAVORITE`), plus a live preview of the apartment block with their window lit up (floor / room of the slot, free homes) - or the protagonist's HaHaland cottage |

The list header names the tab and the section of the selected row ("Face > Eyes"; on 240x160
the section alone when both do not fit); SELECT + UP / DOWN jumps to the next / previous section
(the button bar says so every few seconds). Every row has a help line saying what it is for and how to
change it (`cr_row_help`): the button bar shows it as its tip on 240x160, and a second footer line
shows it on 320x240. The START menu's **Rows** entry (LEFT / RIGHT or A) switches between *every
detail* (the default, every row) and *just the basics*, which hides the fine-tuning rows
(`cr_row_level` 1: the proportion sliders, the cheek extras but blush and freckles, the hair
highlight, Match fur, eye count, fangs, the voice's quality / intonation / accent, the marking and
print colours). Every part row stays in the basics.

Values show their catalog names (`doomo_*_names`); a part row's footer names its themed set too
("Top Hat 4/24 - Mayor Giggs' Tea Party"; on 240x160 only when it fits). Lists cycle with
LEFT/RIGHT; A on a **part row** opens the picture grid (below), A on any other list opens a picker
with every choice. Colour rows show a swatch
and A opens a swatch grid (8 columns; skin adds the 8 natural tones;
hair highlight has "None" in an 11-column grid). Sliders show their value
and their two ends in the detail line ("Short <-> Tall"). Long values
scroll as a marquee on the selected row.

Changing the species brings that species' parts along (skin, head shape,
ears, nose, mouth, fangs, horns, tail, wings, halo, fur marking, eye count -
`cr_species_base` in `creator_data.c`); eyes, hair and outfit stay.

## Controls

| Button | Main list | Picture grid | Pickers | Turntable |
|---|---|---|---|---|
| UP / DOWN | row | cursor (a row of pictures) | choice | walk on/off (UP), auto-spin (DOWN) |
| LEFT / RIGHT | change value | cursor (tries the part on) | page (list) / move (swatches) | turn (8 directions) |
| A | open grid / picker / type / toggle / apply preset | wear it (refused, with a shake, when locked) | choose | tumble |
| B | back; with changes: "Discard?" | back (puts the old part back) | cancel (restores the value) | back |
| L / R | tab | filter chip | page | - |
| START | creator menu | find by letters | - | back |
| SELECT (tap) | turntable | show all (clears the chip and the search) | - | back |
| SELECT + UP / DOWN | jump to the first row of the previous / next section of the tab | - | - | - |
| X / Y | undo / redo (GBA: SELECT+A / SELECT+B) | - | - | - |

The START menu lists only what applies, in five groups with a thin rule
between them: **Move in! / All done!** | Randomise (one entry: LEFT / RIGHT
switches *all* / *this tab*; the Home tab only has *all*) | Undo (n), Redo (n)
(each only while there is something to undo / redo) | Copy a look...,
Possessed peek | Start over / Reset changes (only once something changed),
Quit. Nothing is shown greyed out. `cr.menu_sel` holds a `CM_*` id, and
`cr_menu_list()` / `cr_menu_index()` give the visible order for tests.

## The preview

* Full-body `figure_image()` on a little stage (wallpaper, rug, drifting
  hearts or embers), bouncing on every change with sparkles, hearts and
  stars. It blinks, breathes, wags and animates its aura by itself, and
  the expression cycles through the personality's own face.
* The camera follows the row (`cr_camera`, CR6): on the Head & Face and Hair tabs and on the hat,
  horn and halo rows (and their colour / size rows) a close-up bust (`portrait_render`, re-rendered
  only when its animation phase changes; on 240x160 the head is cropped and shown at 2x); on the
  shoes and bottom rows the legs (the lower half of the figure at 2x, 3x on 320x240); on the tail
  and wings rows the citizen turns round ("Back view", the fighter's back view).
* SELECT turns the preview into a turntable (`fighter_render`, 8 views,
  walk cycle, a tumble).
* "Possessed peek" shows the citizen possessed (just for fun).
* A locked part is worn by the citizen in lavender: only the part itself (the pixels where the
  look wearing it differs from the draft without it; for a part that reshapes the head, only the
  colours the draft does not use), with a bubble with its unlock hint (at most 3 lines, 4 on 320x240,
  inside the pane; at the top on the legs camera, clear of the shoes; the footer always has the whole
  hint). Other locked options (paints, presets) still show as a lavender silhouette.
* The look on screen is throttled (a new figure costs about two GBA ticks; on the GBA it is drawn in
  slices while the last one stays up, "Speed" below),
  so holding a direction stays smooth.

## Locks

`unlock_option_ok()` / `unlock_paint_ok()` decide. A locked option can be
browsed (the part in lavender on the citizen, its name and the whole hint in the
footer: a marquee on 240x160, two lines on 320x240; a padlock in pickers and on
the grid's lavender icon) but never chosen: A is refused with a shake. A value the
citizen already had when the creator opened is grandfathered (a premade
citizen keeps their Goat Eyes). Presets containing a locked option show a
padlock and can't be applied. The randomiser and "copy a look" replace any
locked value with an allowed one.

The creator's own unlockables (ids 256..319, `doomo_unlocks_creator`):

| Unlock | How |
|---|---|
| Sparkle Eyes, Witch Hat, Star Freckles, Monocle, Unicorn Horn, Platform Shoes, Fez, Goat Eyes, Skull Cap, Lab Coat, Heart Head, Fly Wings, Pompadour | create 1, 3, 5, 8, 12, 15, 18, 22, 30, 40, 50, 75, 100 citizens |
| Spiral Horns | move a new citizen into Hellvile |
| Mutton Chops | give a citizen a makeover |
| Cowboy Hat | create yourself for the story |
| Bushy Brows | undo 30 times in one sitting |
| Nose Ring | test 10 voices in one sitting |
| Scales | randomise 25 times in one sitting |
| Bowl Cut | copy the look of a premade citizen |
| Lizard Tail | peek at a possessed preview |

## Tools

* **Undo / redo**: a 20-snapshot timeline (19 undo steps). Holding a
  direction on one row merges into one step.
* **Randomise all**: `resident_generate()` (cute by default), or
  `resident_generate_hellborn()` for Hellvile citizens; keeps a typed name,
  the heritage and the slot.
* **Randomise this tab**: only that tab's fields (the Body tab keeps the
  species coherent).
* **Copy a look**: from any citizen in the town or from any of the 1000
  premade citizens (SELECT / L / R switch the source); the preview shows
  the look before you copy it.
* **Presets**: 3 per species (72), 20 outfit sets, 12 voice types; LEFT /
  RIGHT previews a preset on the figure, A applies it.
* **Name ideas**: LEFT/RIGHT on the name rolls a cute (or spooky) name;
  on the nickname it rolls "Mo-chan", "Lil Mochi", "MoMo"...
* **Reset / Start over**, **Quit** (asks before discarding).

## Finishing

1. The name must be non-empty. If the player never typed or picked one
   (or the protagonist is still the placeholder "You"), the keyboard opens
   first; cancelling it returns to the Profile tab.
2. `resident_sanitize()` on the record.
3. New citizen: `town_alloc(district)` (or the given empty slot), status
   ALIVE, `RF_CUSTOM`, `progress_add(PROG_CITIZENS_CREATED, 1)`. If the
   district is full, a toast says so and nothing is written.
   Edit / protagonist: only the identity fields are copied (look, name,
   voice, personality, profile, heritage and favourite flags) - stats,
   status, relationships, the slot and everything else stay. `RF_CUSTOM`
   (and `RF_PLAYER` for the protagonist).
4. `portrait_invalidate(id)`, the nickname is stored.
5. The finale: the citizen hops over to Hillvile Towers / Hellvile Pits
   (or their HaHaland cottage) with confetti, or takes a bow on a curtain
   stage after a makeover. A (or a few seconds) returns.

Cancelling leaves the town byte-for-byte unchanged
(`creator_cancel_leaves_the_town_unchanged`).

## The age gate for parts (CR12)

Every part has an age class in the catalog (below): **all**, **teen** (TEEN+) or **adult**. Built-in
content is non-explicit; "adult" is for things like a wine glass prop (owner default), never anything
sexual, no tobacco, vape or drug props, no gore. While the draft's Age row is a minor stage:

* a part above it is **not offered**: LEFT / RIGHT skip it, the picker neither lists nor counts it
  (`cr_value_hidden()`, `cr_visible_count()`). There is no "18+" badge: for adults such parts look
  like any other part;
* changing the Age row to a younger stage swaps every such part for its slot's default and says so in
  a toast ("Traffic Cone is for grown-ups - swapped for None"); randomise and copy-a-look sweep the
  same way quietly; `write_town()` filters the record once more before it is stored;
* the renderer enforces it too: `look_for_stage()` (portrait_age.c) drops the part, so a premade, a
  copied look, an inherited baby look or a hand-edited save never shows one on a minor;
  `resident_generate*()` and `sim_make_baby()` filter what they roll or inherit.

TEEN+ needs a teen, an adult or an elder; ADULT needs an adult or an elder. A slot's value 0 is always
**all** (the build refuses anything else), because the gate swaps to it. Today no part is tagged teen or
adult, so nothing changes for any citizen; the gate is in before the first such part goes live (the
art batches tag them in the catalog). Tests: `test_creator.c` (`creator_hides_parts_above_the_drafts_age`,
`creator_age_change_swaps_grown_up_parts_and_says_so`) and `test_part_meta.c` (4,000 random looks at
every minor stage never render an adult part), through the host-only override `part_age_test_set()`.

## The part catalog (CR1)

`core/data/parts/catalog/` names every part the creator can show, one text file per slot, compiled by
`tools/partcat_build.py` (a `make gen` generator, checked by `make check-gen`) into
`core/gen/part_meta_data.c` / `part_meta_ids.h` (names, sets, tags, age, method, state, cost weight,
shape chip, a sorted name index) and `core/gen/part_age_data.c` (only the parts that are not *all*:
resident on every platform). API: `core/include/doomo/part_meta.h` (`part_meta_find`, `part_name_of`,
`part_search` by letters in the name or the set name, `part_set_info`, `part_age_ok`, the filters).
On the PS1 `part_meta.c` and its data link into the creator overlay (`ps1/tools/ovl_plan.py`).

```
slot hat list=hat name="Hat" shapes=caps,soft,tall,wide,crowns,ears,helmets,hoods method=P target=40 new=F
0   "None"           set=basics tags=plain
3   "Top Hat"        set=teaparty tags=formal,showcanon shape=tall
24  "Gumdrop Hat"    set=candy tags=candy,cute shape=soft method=F state=hidden
```

* **A part** is one value of one list. The list is a gene field (`list=hat`: its `GF_*` id), the species
  (`PL_SPECIES`) or a deep field (`deep=DF_LASH`: `PL_DEEP0 + DF_x`, provisional shim ids until P0).
* **Keys:** `set=` (one of `sets.txt`: every part is in exactly one of 25 themed sets, e.g. Hellvile Pits
  Punk, HaHaland Candy Co., Bonejangles' Party Pals, Indie Art Jokes, Mayor Giggs' Tea Party; a third of
  the themed sets are `reward=yes`), `tags=` (`tags.txt`, 31 theme tags), `age=all|teen|adult`,
  `method=P|S|F|T|C` (painter code, script, family, tool data, code variant), `shape=` (the slot's shape
  chips), `hides=` (`hides.txt`: what it covers, for "Hidden by" chips), `state=live|hidden|planned|test`.
* **States:** *live* is selectable today; *hidden* is drawn but not selectable until its list is flipped
  live (the 6 B76 halos, the 45 wave-2 choices, the CR2 family batch); *planned* is named, not drawn;
  *test* is a VM demo value.
* **Ids are append-only.** `core/tests/golden/part_ids.txt` freezes every live part (364 today);
  `partcat_build.py --freeze` appends new ones after a sign-off. The build and `test_part_meta.c` refuse
  a removed, renamed or un-lived shipped id.
* **Targets:** `target=` / `new=` in a slot line and `planned.txt` hold the creator plan's 1,611 new
  parts (524 in the gene, 1,087 deep) per slot, for `tools/part_budget.py`.
* **Costs:** `costs.txt` is MEASURED by the unit test `part_costs_match_the_renderer` (units of 64 pixel
  visits over the slot's value 0, bust + figure + chibi); refresh with `build/tests --update-golden
  part_costs_match_the_renderer` then `python3 tools/partcat_build.py`. The creator's coming Busy meter
  (CR9) sums them.
* Rejected at build time: unknown set / tag / hide / shape / field, a value past the field's width, a
  repeated name in a slot, a part without a set or tag, `method=S` without a script, a script without a
  catalog line or on a list no painter draws scripts for, and a value 0 that is not `age=all`.

Today: 35 slots, 433 parts (364 live, 66 hidden, 3 VM demos), 25 sets. `python3 tools/part_budget.py`
prints the per-slot table.

## The picture grid (CR6)

A on a part row (every row whose list is in the part catalog: species, body, head, eyes, brows,
nose, mouth, ears, hair, facial hair, horns, tail, wings, halo, marking, aura, top, print, bottom,
shoes, hat, accessory) opens `creator_grid.c` over the list window:

* a grid of the row's choices as pictures (the part icons below): 6 x 3 cells of 21 px on 240x160,
  7 x 5 of 24 px on 320x240, scrolling by rows; the citizen's original part has a gold star, a
  locked one shows its picture as it is on a lavender cell with a padlock (you can see what it is);
* the cursor tries each part on at once (the preview's camera follows the slot); A keeps it (one
  undo step), B puts the old one back;
* **filter chips**, stepped with L / R: All, Unlocked (when the slot has locked parts), the slot's
  shape chips (hats: Caps, Soft, Tall, Wide, Crowns, Ears ...) and its themed sets (Mayor Giggs'
  Tea Party, Hellvile Pits Punk ...); the header counts "3/8 of 24";
* **search**: START opens the keyboard; a few letters keep the parts whose name or set name holds
  them (`part_matches`); the letters show next to the chip; SELECT shows everything again;
* the footer names the part and its set, or for a locked one its name and how to unlock it (the
  whole hint: a marquee when it is longer than the line, on both screens);
* parts above the draft's age class are not listed at all (CR12);
* every choice's lock bit is worked out once when the grid opens (`cr.g_lock`);
* a chip is one byte (kind in the top 3 bits, shape or set index in the low 5) and the slot's sets
  are gathered in a 32-bit mask, so the catalog may have at most 32 themed sets: `creator_grid.c`
  stops the build past that (`PART_SET_COUNT <= 32`). CR4's ~120 sets need a wider chip first.

## Part icons (CR7)

Every live part of the grid's slots (351 today) has a 20x20 icon made at build time from the real
renderer by `core/tools/part_icons.c` (`make -C core gen`; `make check-gen` regenerates and
compares; `make -C core part-icons-sheet` writes the contact sheet `build/sheets/part_icons.png`):

* bust slots (head parts, hats, hair, horns, halo, accessory, species) are framed from the
  portrait, body slots (body, tops, prints, bottoms, shoes, tails, wings, markings, aura) from the
  full figure; each slot uses one window for all its icons (the pixels its choices change, squared),
  box-sampled to 20x20 keeping the most common part colour of each box (the darkest on a tie);
* in slots whose first choice is None / Bald / Barefoot / Solid the part is shown alone on a flat
  grey silhouette of the mannequin (moved with parts that move the citizen, e.g. platform soles;
  hats and horns keep only colours the mannequin does not use), and choice 0 gets a red slash; the
  other slots (species, eyes, head shapes, mouths, ears, body types) show the part in colour on a
  bald mannequin;
* at most 15 colours an icon. `core/gen/part_icons_data.c` holds the raw icons (140 KB of ROM,
  GBA and native blit them from ROM); `core/gen/part_icons_rle.c` the same packed at 4 bits a pixel
  (57 KB, `part_icon_unpack`): the PS1 links only these, in the creator overlay group
  (`DOOMO_ICONS_RLE`), and unpacks one icon into `cr.icon_buf` before `plat_image` uploads it to
  the VRAM image cache (each icon has its own uid). Tests (`test_part_icons.c`): one icon per live
  part, packed == raw, at most 15 colours, stable uids.

When CR3 flips more lists live and CR4 adds parts, `make -C core gen` draws their icons too (the
PS1's packed icons grow with them: past the creator overlay's room they move to the disc, CR10).

## Speed (CR9)

Measured with `make -C gba profile-creator` (the CR0 script) on the GBA (libmgba, drawn fps /
average work of a drawn tick; a 30 Hz tick is 33.4 ms):

| | before (CRA) | first CR9 pass | now |
|---|---|---|---|
| idle (Species & Body tab) | 12.7 / 57.2 ms | 27.2 / 32.0 ms | **30.0 / 15.6 ms** |
| rows scrolled (Outfit) | 12.9 / 56.8 ms | 18.3 / 46.2 ms | **26.2 / 27.7 ms** |
| hat grid scrolled | 7.6 / 95.4 ms | 17.1 / 49.5 ms | **21.1 / 39.3 ms** |
| a value held (Outfit, Top) | 8.6 / 85.0 ms | 11.3 / 64.7 ms | **20.5 / 40.2 ms** |

`make -C gba test-creator` (`creator_scroll.txt`, expect-fps floors at about 85-90% of today) measures
idle 30.0, the species grid at rest 29.9, rows 26.8, a value held 19.8 and the species grid scrolled
16.5. The owner's target is a steady 30 everywhere: idle and the grid at rest reach it; the gap is a
held direction, where each step redraws the list window (~10 ms on the GBA) and each new look is a
whole figure or bust (~0.9M cycles) drawn in slices. Where each change applies:

* **every platform** (the same on native, the GBA, the PS1 and the Dreamcast): the list cursor, the
  selected row's arrows, the slider heart and the toggle hold still (no bob); the tab strip is drawn
  last; the bust comes from the portrait cache (`portrait_image_look`); the backdrop is not drawn
  under the pane;
* **the GBA and the PS1** (`CR_SLICE`): a new look or camera is drawn in slices while the last one
  stays up and lands with its hop, and the camera waits for the cursor to rest;
* **the GBA only** (`CR_CALM`, a listed speed step): the still stage (no blink or breathing, still
  wallpaper, motes and backdrop), the retained preview pane and button bar, `ui_glint_hold`,
  `plat_panel_solid` and `portrait_pump`. Native and the Dreamcast animate the preview as before.

What landed:

* **a new look in slices** (`creator_draw.c` `pane_view`): the pane shows `cr.disp` (look, the look
  without a browsed locked part, camera) while the portrait cache draws the wanted one, `cr.pend`,
  in slices (`portrait_hold(2)`, one layer group a tick, more at the end of a frame with room:
  `portrait_pump`), then it lands with a hop and a sparkle; a look is started once it has rested two
  ticks, so a held direction streams the names past and the figure catches up when it rests; a view
  the cache cannot start is drawn at once after 24 ticks. The bust comes from the cache too
  (`portrait_image_look`), so a new expression is sliced as well (it was a whole ~45 ms render);
* **the camera waits** for the cursor to rest 6 ticks on a row before it zooms (scrolling through
  the Outfit rows draws no bust / legs view for every row passed);
* **retained panels everywhere**: the preview pane and the button bar are panels too (the tab strip,
  the list and the grid already were); the stage stands still (no drifting wallpaper hearts, no
  motes, a still backdrop), windows have no sliding glint (`ui_glint_hold`), and every creator panel
  is put back as its whole rect (`plat_panel_solid`, platform.h: one copy a row instead of one per
  rect of its body);
* **the backdrop is not drawn under the pane** (`ui_backdrop_holes`, two holes; every platform, the
  same pixels);
* **the locked-part overlay** (`lock_compose`) skips the pixels that agree a word at a time and keeps
  the draft picture's colours per picture (~17 ms on the GBA before);
* the first pass (still there): the lock memo, the tab / list / grid panels, the still pose.

`core/tests/test_panel.c` plays the creator with the GBA's way switched on (`cr_calm`, a fake portrait
clock at one layer group a tick): every frame is pixel-identical with the panel cache on and off;
`test_creator.c` checks that a new look lands in slices with the old one up meanwhile, and that the
camera waits.

**PS1** (`ps1/harness/scripts/creator_speed.txt`, the same moves, run by the disc's harness target with
expect-fps floors; PCSX-Redux): idle 30.1, rows 30.0, a value held 29.9, the species grid scrolled
24.7 (22.0 before the PS1 took the sliced looks: `CR_SLICE`, the part of `CR_CALM` the PS1 uses; its
preview still animates and it has no panels). The gap on the PS1 is the species grid held, where
every step is a whole new body. Memory: the packed icons (58 KB) and the part catalog
(`part_meta_data`, 14 KB) are generated files, which `ovl_plan.py` used to keep resident; they join
the creator overlay group, and the creator group now loads low over the borrowed `.bss` like the
house (`BORROW_GROUPS`; nothing of the creator draws a 3D view, syncs the world or touches the
card), so its 150 KB need 99,828 B of slot and the disc ends 11,824 B above the 64 KB stack reserve
(docs/PS1.md).

## Nicknames

`DoomoResident` has no room for a nickname, so the creator keeps a small
table (64 entries, 1 KB) saved as the `NICK` save-extension chunk through
the `doomo_save_ext_extra` slot of `save_ext.h`. Each entry remembers a
hash of the citizen's name, so a slot that is freed and reused never
inherits a stale nickname.

## Layout

`cr_layout()` computes everything from `plat_screen_w()/h()`:

| | 240x160 (GBA) | 320x240 (PS1) |
|---|---|---|
| tab strip | 9 x 21 px cells + L/R caps | 9 x 29 px cells |
| preview pane | 86 x 122, figure 1x, bust 2x (cropped), turntable 2x | 120 x 195, figure 2x, bust 2x, turntable 3x |
| list | 145 x 122, 8 rows of 11 px | 189 x 195, 13 rows of 12 px |

The START menu (`cr_menu_rect`) lists up to 9 entries (Undo, Redo and Reset with the Rows entry). On
240x160 that is too tall with the usual 4 px between the groups, so then the gaps are 2 px and the
bottom margin 10 px: the "Creator menu" pill stays on the screen and the window stays above the
button bar (`creator_menu_fits_with_every_entry_listed`).

Hellvile citizens get the cute-goth look (`WIN_DARK` windows, a dusky
violet room with embers, the Hellvile Pits finale).

## Costs

**Baselines for the creator expansion (CR0, measured 2026-11-08):**

| | before (`4851147`) | after CR0-CR2 + CR12 |
|---|---|---|
| GBA ROM | 4,299,852 B (`b080562`: 4,301,668 B) | 4,312,888 B (on `b080562`: 4,314,712 B; +13,044: catalog names and meta, the VM macro ops and hooks, 15 family parts) |
| GBA EWRAM free | 3,556 B (`b080562`: 3,540 B) | the same (0 B: everything new is `const`; the tools-only shape mode has no variable on the GBA) |
| GBA IWRAM stack | 6,356 B | 6,356 B |
| `sizeof(CrState)` (GBA) | 5,992 B of the 12,288 B scene scratch | unchanged (the picker's age filter keeps no state) |
| slot 1 (free play) bound | joint worst 68,612 B of 90,112 B (2,048 B margin): 19,452 B left, so the planned `LOOK` 8,072 B + `CRPF` 256 B fit | unchanged |
| PS1 resident (game disc) | ends `0x801ec580`: 14,976 B above the 64 KB stack reserve | ends `0x801ecce0`: 13,088 B above (+1,888 B; the part-script VM is off on the PS1 until CR10, docs/PORTRAIT.md); on `b080562` + CRA ends `0x801ecd10` (13,040 B above) |
| creator speed, GBA (`make -C gba profile-creator`, new) | idle 12.7 fps / 57.2 ms; a value held 8.5 / 83.6 ms; rows scrolled 13.0 / 56.1 ms; picker scrolled 8.0 / 93.9 ms | 12.7 / 57.2; 8.5 / 84.0; 12.9 / 57.0; 8.0 / 93.7 (`unlock_find` is still 13 % of the picker: CR9) |
| creator speed, PS1 (`ps1/harness/scripts/perf_creator.txt`) | - | 30.0 fps |

`tools/part_budget.py` turns the catalog into per-slot costs: kind (gene: 0 B per citizen; deep: 2 B per
citizen wearing one), width and headroom, live / hidden / target counts, script bytes in the tree, and
the projected GBA ROM and PS1 bytes for the plan's target (about +480 KB GBA ROM and +146 KB PS1
scripts and meta for all 1,611, none of it resident on the PS1).

RAM: about 6.3 KB of `.bss` (the draft, the original, the undo timeline,
one 48x56 render buffer shared by the bust and the turntable, the nickname
table, 9 cached tab icons). Everything else is `const` (ROM). Per tick the
logic is tiny; drawing is a normal UI screen plus at most one figure,
bust or fighter render when the look, the expression or the animation
phase changes (the figure re-renders are throttled while a direction is
held; on the GBA the bust's effect frames change every 16 ticks).

## Extending

* **A new catalog option** (e.g. a new hat): nothing to do - rows use the
  `*_COUNT` and the name tables.
* **A new row**: add it to the tab's array in `creator_data.c` (`field` =
  `offsetof(DoomoResident, ...)`). It is then covered by undo, randomise
  this tab, the tests and locks.
* **A preset**: append (field, value) pairs ending with `CR_NOFIELD`;
  species presets stay 3 per species, in species order.
* **An unlockable**: add it to `doomo_unlocks_creator` (ids 256..319).

## Tests

`make -C core test` runs `test_creator.c` (creating a citizen through the
real scene, Hellvile and district switching, a full district, editing a
premade citizen in place, cancelling, the protagonist, undo/redo, locks and
grandfathering, the randomiser vs locks, species parts, copying a premade
look, nicknames and their save chunk, drawing every phase at both sizes)
and both runner scripts at 240x160 and 320x240.
