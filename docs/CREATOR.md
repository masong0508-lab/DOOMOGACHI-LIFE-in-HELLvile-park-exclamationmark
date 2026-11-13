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
  under the list window, nor under the pane but on the PS1 (its one-hole backdrop code keeps its
  resident RAM at the 72 KB plan gate; the same pixels);
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
* **the backdrop is not drawn under the pane** (`ui_backdrop_holes`, two holes: the list window and
  the pane; the PS1 takes the first only; the same pixels);
* **the locked-part overlay** (`lock_compose`) skips the pixels that agree a word at a time and keeps
  the draft picture's colours per picture (~17 ms on the GBA before);
* the first pass (still there): the lock memo, the tab / list / grid panels, the still pose.

`core/tests/test_panel.c` plays the creator with the GBA's way switched on (`cr_calm`, a fake portrait
clock at one layer group a tick): every frame is pixel-identical with the panel cache on and off;
`test_creator.c` checks that a new look lands in slices with the old one up meanwhile, and that the
camera waits.

**PS1** (`ps1/harness/scripts/creator_speed.txt`, the same moves, run by the disc's harness target with
expect-fps floors; PCSX-Redux): idle 30.1, rows 30.0, a value held 29.9, the species grid scrolled
24.5 (22.0 before the PS1 took the sliced looks: `CR_SLICE`, the part of `CR_CALM` the PS1 uses; its
preview still animates and it has no panels; 24.7 while the PS1 still cut the pane out of the
backdrop too). The gap on the PS1 is the species grid held, where
every step is a whole new body. Memory: the packed icons (58 KB) and the part catalog
(`part_meta_data`, 14 KB) are generated files, which `ovl_plan.py` used to keep resident; they join
the creator overlay group, and the creator group now loads low over the borrowed `.bss` like the
house (`BORROW_GROUPS`; nothing of the creator draws a 3D view, syncs the world or touches the
card), so its 150 KB need about 100 KB of slot. On `696ffa6` the disc ends 79,568 B below the stack
top: 14,032 B above the 64 KB reserve and 5,840 B above the 72 KB plan gate (CRB's resident cost:
80 B; docs/PS1.md has how it was brought down from 832 B).

### S3-3: the held frame and panels drawn first (2026-11-13)

The speed audit 3's package S3-3 (the old G2-CR). Two GBA pieces and one PS1 piece, all in
`creator_draw.c`, `creator_grid.c` and `part_icons.c`.

* **The held frame** (the saved G2-CR work, GBA only: `CR_HOLD` = panels and `CR_CALM`). On the still
  stage nothing but four regions ever changes: the tab strip, the list window (or the part grid in its
  place), the preview pane and the button bar. After one full frame (`cr_still`: nothing is moving,
  `hd_strip`: the button bar's rows have a clean copy, `plat_strip`) the platform keeps the picture in
  both pages (`ui_modal_hold`), and a tick draws only the regions whose key changed, on two drawn ticks
  (one for each page); a tick with nothing new draws nothing. A skipped (catch-up) tick no longer
  drops the bar's clean copy (`cr_draw` set `hd_strip` to 0 on every skipped tick, so under any load the
  held frame never began).
* **Panels drawn first** (`cr_region_pre`, the recipe of S2-6). A region whose key the platform cannot
  keep (it was not begun with that key on the last drawn tick: `pk_key`, `pk_dn`) is drawn before its
  panel, on the fast path, and then taken as one solid panel (`cr_region_pre_end`: begin, solid, end with
  nothing between them). Drawn inside a panel every 2D call is routed and checked (`pn_route`,
  `plat_rect_slow`, `vid_route`: 10-30 ms more on a capture frame). The tab strip, the list, the grid and
  the pane do it; the button bar does not, because `ui_hint`'s own pill is a retained panel too and a
  top-level one is not put back from the page the creator's panel keeps (the bar vanished: the
  panel test caught it). `plat_ui_loops()` runs before the pane's frame, so the figure's 2x and 3x
  scaled images take the UI group's loop (S2-5).
* **PS1: the grid's unpacked icons stay** (`part_icon_image_kept`, `grid_icon`). `part_icon_unpack` was
  30% of the hat grid's try-on frames: every cell was unpacked again on every draw. Each cell's icon now
  stays in the tail of the overlay slot (`plat_scene_scratch`, 48 cells x 400 B; the creator group
  needs about 100 KB of the 131 KB slot; no resident RAM, no new `ovl_plan.py` entry: the call returns
  NULL when the room is not there and the old unpack runs).

Measured on the GBA (libmgba, `sync on`, seed 1; `make -C gba test-creator` and `make -C gba
test-speed2 SPEED2=creator`; drawn fps, frames of 50 ms or more in brackets):

| window | before (`4c0f369`) | after |
|---|---|---|
| idle | 30.0 | 30.0 |
| grid scrolled (species) | 17.2 | **26.8** |
| rows (Outfit, DOWN held) | 29.4 | **30.0** |
| a value held (Outfit, Top) | 22.2 | **29.4** |
| tab change to head, hair, supernatural, outfit: frames over 33 ms | one of 50 and one of 67 | two of 50 (none longer) |
| tab change to voice, home / personality, profile | 2 / 1-2 over 33 ms, one of 67 on voice | 1 / 0 over 33 ms, none over 50 |
| hat grid (bust camera) | 27.2 | **29.8** (none over 33 ms) |
| hat grid try-on move | 15.2 | **26.9** (7 frames of 50 ms, none longer) |
| hat grid filter chip | 21.7 | **30.5** |
| colour swatches / swatch move | 30.0 / 26.0 | 30.0 / 26.0 |

PS1 (PCSX-Redux, `creator_speed.txt` and `speed2_creator`): grid scrolled 29.39 -> **30.00**, hat grid
try-on 28.57 -> 29.03 (2:27 3:1 4:1 against 2:26 3:2 4:1: one frame less; the target 30 is not reached),
idle, rows and a value held unchanged (30.13, 30.13, 29.88). The disc ends at the same address as before
(0x801efe60, 64 KB below the stack top with the boot FMV): no resident byte.

**Re-measured on the landed tree (S3-3 on top of CR3, `0df7284`; the numbers to trust).** CR3's longer lists and
new rows moved the baseline, so the table above (measured on `4c0f369`) is the first build's. On `0df7284` itself
and with S3-3 (same commands; `make -C gba test-creator`, `test-speed2 SPEED2=creator`), drawn fps:

| window | `0df7284` | with S3-3 |
|---|---|---|
| grid scrolled (species) | 17.1 | **27.7** |
| rows (Outfit, DOWN held) | 27.3 | **29.9** |
| a value held (Outfit, Top) | 17.9 | **27.9** |
| idle (work per tick) | 30.0 (16.0 ms) | 30.0 (3.9 ms) |
| hat grid (bust camera) | 29.0 | 29.0 (no gain; target 30 not reached) |
| hat grid try-on move | 21.3 | **23.4** (target 22 met; 10 frames of 50 ms or more, was 14) |
| hat grid filter chip | 26.3 | 26.4 (no gain here; the 30.5 of the first table was before CR3) |

Tab changes: on `0df7284` every tab change had a frame of 50 to 84 ms and six had a frame of 67 ms or more (worst
84 ms); with S3-3 none has a frame over 50 ms. Frames of 50 ms per change: home 0, voice 1, profile 1, hair,
supernatural, outfit and personality 2, head and face 3. The audit's "at most one 50 ms frame" holds on three tabs of
eight. GBA sizes: EWRAM 15,444 B free (as CR3), IWRAM resident 11,256 B, main stack 6,308 B, all
unchanged. PS1 (PCSX-Redux, boot FMV disc): `_end` 0x801ef4e0, 68,368 B below the stack top, identical to CR3's
landing (+0 B); `creator_speed.txt`: idle 30.13, grid scrolled 29.39, rows 30.13, value 29.88; `speed2 creator`:
every window 30.0 or more, the hat grid try-on 30.00 (2:28 3:1) and the grid cursor move 30.00. The PS1 was not
re-measured on a `0df7284` disc, so the PS1 gain is the first build's (try-on 28.57 -> 29.03 on `4c0f369`).
The raised GBA floors that hold on this tree: hat grid 28, try-on 22, filter chip 25 (`speed2_creator.txt`).
One fix at landing: `part_icon_image_kept` took the raw icon index into the packed set; CR3 made the two sets
differ on the host, so the host test failed (it now uses the packed index; the PS1 has only that set).

**Same pixels** (`vhash-seq` over the tab tour and the scroll script, `HARNESS_VHASH_SHOTS` frames, against
`4c0f369`). 18 of the 20 settled shots (every tab, the grids at rest, the swatches, the START menu, the species grid
scrolled) are byte-identical. The other two (the try-on 30 ticks after a move, a value held mid-step) differ in 17 and
122 pixels of the preview pane: its landing sparkle is at another phase. Frames in between may land a tick or two
later than before (the held frame leaves the pane's pending picture to the idle time after the frame, `pane_view`, so
the preview's hop and sparkle begin later while a direction is held): 102 of 2,130 frames of the tab tour and 221 of
1,026 of the scroll script differ, every one inside the pane (x 7-84, y 37-138), plus the first three ticks of the
entry fade, which is timed by the clock. `core/tests/test_panel.c` plays the creator and the species grid with the
panel cache on and off (identical) and counts the captures that drew nothing inside (`nat_panel_stats.empty`: at
least half; without `cr_region_pre` none).

What is left: four tab changes (head, hair, supernatural, outfit) still have two frames of 50 ms, the first two
ticks after the press (the audit's target was one; a try that held the pane's new picture back for those two ticks
changed nothing, so it is the list's new page and its first-time text, not the picture), the hat grid's try-on moves
have seven 50 ms frames a window, and the species grid held is 26.8, not 30 (each step is a whole new figure:
S3-2's portrait work). The lock overlay (`lock_compose`) is composed once
per picture (it was already memoised by the picture's key since CR9); making it once per page would need
the pictures of a whole page, which the scratch does not hold.

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

## CR3: the gene lists live, the gene's new fields (2026-11-13)

* Every gene list now offers its live catalog count (`part_live_count`), not only the old `*_COUNT`
  enums; `resident_sanitize` keeps every live choice and the classic generators keep rolling the v1
  values, so Classic towns, premade fingerprints and old saves are unchanged.
* The gene's eleven new fields (pupils, eye whites, fur pattern, spot colour, glasses, outer layer,
  legwear, blush style, ear fold, hood up, handedness) have creator rows, help lines and reach every
  renderer path through the resolved set (`look_gene_deep`; the cache key hashes it, 0 for a Classic
  look). Rows of a list with no live value yet stay hidden (Outer layer, Legwear, Fur pattern until art
  batches A1-A3).
* Saves are still the v1 wire image: the NICK chunk carries the new bits after the nicknames
  (`look_gene_ext_*`, version 1 kept so an older build reads the nicknames).
* Speed work kept: the deep set is threaded through `job_begin` / `job_start` / stand-ins / the
  prefetch; the sliced and retained drawing is unchanged.
* Runner scripts `creator.txt` and `creator_grid.txt` step over the new rows (Hood, Pupils, Eye whites,
  Ear fold, Blush style); the eye picker step uses the Unlocked chip so both screen sizes reach the
  same row.
* PS1 (2026-11-13, measured on `ee686c7` + CR3): the new fields are live there too, as far as the PS1
  can draw them. It has no part VM yet (`DOOMO_PARTS_VM=0`, until CR10), so the painted values show
  (pupils, eye whites, blush styles, ear fold, hood, handedness) and the creator offers only those
  (`part_live_drawable`); the scripted ones (glasses, outer layer, legwear, fur pattern) wait for CR10
  and their rows stay hidden. `DOOMO_GENE_EXT=0` (a `-D`) still compiles the whole feature out
  (`look_fields.h`); no build uses it and no test covers it.
* PS1 memory: the first build grew the EXE and failed the 64 KB stack reserve. The cause was not the
  new code: `creator_data.c` held its whole merged string pool (about 6 KB of row labels, help lines
  and names) resident, because resident code reads two of its tables (`doomo_unlocks_creator`,
  `doomo_life_stage_names`, now in `unlocks_creator.c`) and its 8-byte `n_*` pointer tables sat in
  `.sdata` (always resident; `creator_data.c` is built `-G0` now). The pool now goes with the creator
  overlay group. EXE `_end` `0x801ef4e0` against `0x801efe60` on `ee686c7`: 68,368 B below the stack
  top against 65,936 B, so CR3 leaves the PS1 2,432 B better than it found it. Still below the 72 KB
  plan gate (SPEED PS1-RAM). Harness: `game_smoke`, `creator.txt`, `creator_grid.txt`,
  `creator_speed.txt` PASS; `speed2` creator equals the base table.
* The part live counts (`doomo_part_live`, `part_live_count`, `part_live_drawable`) moved to the
  catalog files (`part_meta_data.c` / `part_meta.c`, creator overlay on the PS1).

## CR4 batch A2: hair (2026-11-13)

* **40 new parts live:** 32 hair styles (gene `hair_style` 32..63: Pixie, Shag, Space Buns, Top Knot, Braid Pair,
  Curtains, Beehive, Bouffant, Rattail, Fauxhawk, Undercut, Bubble Pony, Side Pony, Half Up, Victory Rolls, Pageboy,
  Flip, Mop Top, Cloud Puff, Cat Tufts, Icicle Spikes, Waterfall, Pom Tufts, Bedhead, Lampshade, Spiral Curls,
  Crescent Sweep, Mini Mohawk Row, Fringe Curtain, Wisp Ponytail, Long Droop, Star Spikes) and 8 beards (gene
  `facial_hair` 8..15: Chin Puff, Chinstrap, Van Dyke, Horseshoe, Braided Beard, Pencil Stache, Sea Captain, Cloud
  Beard). The lists are full (64 and 16 of their 6 and 4 bits). Saves, premades and Classic islands are untouched:
  the gene already had the bits, and the classic generators still roll the v1 values.
* **Authoring:** hand-written part scripts in `core/data/parts/hair/hair_b.txt` and `beard/beard_b.txt` (templates for
  the cap and the back-view nape, `scale 128`, coordinates in head radii, hair in `hair_color` on one ramp so the
  shape alone tells the styles apart). A hair style is a back block (group 0: what hangs behind the head) and a front
  block (group 4: cap, bangs, top pieces, and from behind the whole head); beards are group 3, drawn after the mouth
  so none covers it. Chibis get the same blocks. 108 more parts of the plan's hair rows (hair page 2 24, bangs 12,
  ahoge 12, hair accessories 52, facial hair page 2 8) are deep fields: they need new `DF_` ids and painter hooks
  in C and are only selectable after CR5 saves deep parts, so they are left for the next package (see
  `status/CREATOR.md`).
* **Age:** all 40 are age class ALL. Facial hair is dropped by `look_for_stage` for every minor, so no beard shows on a
  baby, toddler, child or teen (tested for all eight); no hair is suggestive.
* **Gates:** shape gate (>= 24 px of the bust against every other value of the slot, >= 6 px on a chibi), 600 px solid
  cap, cost weights measured (`costs.txt`, the dearest new part costs 31, the dearest classic one 89), names unique,
  one set each, ids frozen in `golden/part_ids.txt`, review line `A2` in `review.txt`.
* **Review sheets** (`portrait_sheet`): `--slot hair` and `--slot hair --shape`, `--slot beard` (a dark beard on a bald
  citizen) and `--shape`, and the new `--interplay` sheet: every hat of the catalog over Beehive, Bouffant, Pom Tufts,
  Waterfall, Space Buns and tall horns (`interplay_hats_N.png`).
* **Runner walk-through:** `core/tests/scripts/creator_parts_hair.txt` (both resolutions): the Hair tab, the Hair
  grid paged, a style found by letters and worn, the Facial hair grid, a beard worn.
* **Tests:** `test_cr4_a2.c` (lists full and every new part catalogued under the cost cap; hair seen from behind;
  no minor wears a new beard; every hat over every new style renders the same twice).
* **PS1:** the part VM is still off there (CR10), so the creator's Hair and Facial hair rows keep offering the painted
  values (`part_live_drawable`); the new parts are data in the creator overlay only (names, set, tags: no resident
  byte; the PS1 margin is unchanged, see `docs/PS1.md`).

## CR4 batch A3: spooky (2026-11-13)

* **60 new parts live** (the plan's 134 are these 60, the 6 painted halos live since CR3, and 68 deep ones left for CR5): 10 horns (gene `horns` 16..25: Candy Cane
  Hooks, Stair Horns, Trident Prongs, Rhino Nub, Bone Fork, Flame Tips, Leaf Sprouts, Lollipop Swirls, Crescent
  Moons, Imp Tips), 16 tails (`tail` 16..31: Squirrel Plume, Pig Curl, Scorpion, Spade Tail, Zigzag Tail, Pompom
  Rope, Fish Fin, Dino Plates, Ghost Wisp, Bow Tail, Looped Tail, Chain Tail, Skunk Plume, Candy Cane Tail, Beaver
  Paddle, Twin Tails), 14 wings (`wings` 10..23: Cherub Mini, Moth, Raven, Dragonfly, Paper Wings, Balloon Wings,
  Leaf Wings, Jet Fins, Cloud Puffs, Heart Wings, Stained Glass, Crystal Shards, Ribbon Streamers, Parasol Wings),
  4 halos (`halo` 12..15: Vinyl Record, Candle Crown, Cloud Wisp, Orbit Ring) and 16 auras (`aura` 8..23: Orbit
  Dots, Rain Cloud, Music Notes, Snowflakes, Star Ring, Sleepy Zzz, Spirit Wisps, Petals, Fireflies, Circling
  Bats, Falling Leaves, Question Marks, Sweat Drops, Floating Eyes, Confetti, Embers). The lists are now 26, 32,
  24, 16 and 24 long. No save change: the gene had the bits; aura values 16..23 use the fifth bit that the creator's
  NICK trailer carries (`look_gene.c`; the v1 stream stops at 15). The six painted B76 halos were live since CR3.
* **Left for the next spooky package (68 of the batch's 134):** horn page 2 (6) and horn / antenna bits (32), tail
  accessories (16), wing accessories (8), halo page 2 (6) are deep fields. They need new `DF_` ids with painter hooks
  in C and a creator row that takes a deep field, so they are only selectable after CR5 (saved deep parts).
* **Authoring:** hand-written part scripts in `core/data/parts/spooky/{horns,tail,wings,halo,aura}_b.txt` (one script
  set, 5.9 KB raw), no new painter code: the CR2 hooks already run a script for a tail, wings, halo and aura. Horns, halos
  and auras are head anchored (`scale 128`, half head radii). **Tails and wings are torso anchored and drawn at
  `scale 255`**: the figure's torso is only 10 x 6 px, so a tail has to be authored about four times its torso units, and
  each is a template drawn from the side of the hips for the front views (`ifyaw 0,1,2`) and from the middle of the hem
  for the back views (3, 4); a second block with its own scale and offset draws the bust. Auras draw two frames
  (`ifanim breath` against its absence) and never on a chibi (`view=portrait,figure,fighter`).
* **`parts_build.py`:** a torso block that never draws on the bust (`view=` without `portrait`) is now estimated against
  the figure's 10 x 6 px torso instead of the bust's 18 x 15 (`NOMINAL_TORSO_BODY`), so the 600 px guard measures a tail
  at the size it is drawn. Nothing else changed.
* **Age:** all 60 are age class ALL; none is gore, a body part or suggestive (a chain tail is rings, a candle crown is cartoon
  candles, the floating eyes are cartoon). The first draft's pair of lightning-bolt horns was replaced by stair steps so
  no pair of bolts can read as a real-world symbol.
* **Gates:** shape gate (>= 24 px of the bust against every other value of the slot, >= 6 px on a chibi where the part
  draws there), cost weights measured (`costs.txt`, the dearest new part costs 24 against 50 for the dearest classic
  one), names unique, one set each, frozen ids, review line `A3`, part icons regenerated.
* **Review sheets** (`portrait_sheet --slot KEY [--shape]` for each of the five slots, and the new
  `--slot tail|wings --views [--from N] [--shape]`: the figure at 3x and the fighter's five views from behind to the
  front, so a tail and a wing are seen from every side).
* **Runner walk-through:** `core/tests/scripts/creator_parts_spooky.txt` (both resolutions): the Supernatural tab, each
  of the five rows opened as a grid, a new part found by letters and worn (Stair Horns, Squirrel Plume, Moth, Vinyl
  Record, Confetti), the preview showing it.
* **Tests:** `test_cr4_a3.c` (the lists full and the 60 parts catalogued and inside the cost cap; tails and wings show on
  the figure and from behind; every aura has two frames; every stage keeps them and a child may wear all; they survive a
  save and sanitize; the old values still differ from the new). The PS1 reads none of the scripts yet (the part VM waits for
  CR10): its creator keeps offering the painted values.

## CR4 A1a: head shapes, eyes, pupils, eye whites, brows (2026-11-13)

The first art batch of the part plan (CREATOR_PLAN 3.1, "Head & face" rows), drawn as part scripts
(`core/data/parts/head/a1a.txt`, `core/data/parts/face/a1a_{eyes,pupil,brows}.txt`) and live:

| slot | values | new | live now | gate (shape, bust px) |
|---|---|--:|--:|--:|
| head shape | 12..23 | 12 | 24 | 24 |
| eye style | 24..47 | 24 | 48 | 20 |
| pupil | 5..31 | 27 | 32 | 2 |
| eye whites (sclera) | 3..15 | 13 | 16 | 4 |
| brows | 12..31 | 20 | 32 | 4 |

**96 new parts live** (plan A1a: about 145). Left for the next package, with the reason:
* lashes, lids and liner (31 new, deep fields `DF_LASH`, `DF_LID`, `DF_LINER`): a deep field is not saved
  or chosen yet (CR5); they cannot go live before it.
* extra eyes 4-6 (3): `eye_count` is 1..3 in the look, the creator, the generators and `resident_sanitize`; a
  gene change belongs with the save work, not an art batch.
* the pupil gate is weak: at the creator's default eye size a pupil is 1-3 px, so the 27 shapes differ by
  only 2-14 px at 1x (they read at the creator's 2x crop and with a big eye). Ideas for the next pass:
  a pupil-size slider tied to the eye, or a bigger default iris.

**What changed besides data.**
* `mat ink` and `mat white` (script materials, `PS_MAT_INK` / `PS_MAT_WHITE`): fixed jet black and white
  that follow no colour gene; the pupils, glints and eye whites need them (the dark tone of an eye
  colour is too close to its iris). They live in the render's 16-slot material memo: no RAM of their own.
* `tools/parts_build.py`: a `solid same` shape (a head silhouette drawn into the head's own part) is held
  to 2,200 px, not the 600 px of a new part (a nominal head is about 900 px); selftest covers it.
* The shape gate (`part_scripts_pass_the_shape_gate`, `part_scripts_are_drawn_by_their_slot_hooks_and_distinct`,
  `part_scripts_show_on_other_citizens_too`) now reads each slot's `gate=` from the catalog (it was a
  constant 24 px and a stale binary hid that), tests the gene-only lists pupil and sclera through the deep
  set, compares pupils and whites in colour on a big eye (the one-ramp shape mode erases an ink on an iris),
  and applies the 6 px chibi rule only to slots with a 24 px gate (an eye is 1-2 px on a chibi). A small
  face mark may vanish on one of the three test citizens (a covered or tiny face) but not on all three.
* `make -C core portrait-sheet` builds the contact-sheet tool; its slot sheets stage heads on a bald
  citizen and eyes / pupils / whites / brows on a cobalt-eyed one with the face band at 2x.
* `core/tests/scripts/creator_parts_face_a.txt` (both screen sizes): opens the Shape, Eyes, Pupils, Eye
  whites and Brows grids, walks onto the new parts, wears one of each. `creator.txt` kept its Brows step
  (a 32-part grid lands six rows down on a locked brow, so the grid now keeps None as before).
* Sets: new parts sit in the existing themed sets (no new set); reward sets (Royal Court, Plushie ...) keep
  their new parts behind the set unlock, so about a third of the new face parts show as locked.

Review: sheets in the stream's evidence (`slot_head`, `slot_eyes`, `slot_pupil`, `slot_sclera`,
`slot_brows`, each colour and `_shape`) were looked at; the sign-off is the `A1a` lines in
`core/data/parts/review.txt`. All 96 are age class ALL (plain shapes: no gore, no suggestive reading, no props).
PS1: scripted values draw as the slot's default until CR10 (no part VM yet), so the PS1 creator does not
offer them (`part_live_drawable`); the new parts add no resident byte there.

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
