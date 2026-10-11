# The citizen creator (`SCENE_CREATOR`)

The heart of the "immense customisation": every citizen of Hellvile Park,
the story protagonist included, is made or made over here. Owner: the
creator module.

| File | What |
|---|---|
| `core/src/creator.c` | the scene: state, input, rows, undo timeline, pickers, randomiser, presets, locks, nicknames, writing into the town |
| `core/src/creator_draw.c` | all drawing: tab strip, preview stage, option list, pickers, START menu, copy picker, turntable, the "moving in!" finale |
| `core/src/creator_data.c` | const tables: tabs and rows, 72 species presets, 20 outfit sets, 12 voice types, personality texts, tab icons, the creator's unlockables |
| `core/src/creator_int.h` | private glue between the three |
| `core/tests/test_creator.c` | headless tests (19) |
| `core/tests/scripts/creator.txt`, `creator_hellvile.txt` | runner walkthroughs (47 screenshots per resolution) |

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
| Personality | movement, speech, expression, attitude, quirk (0-15) with a live panel: personality type name, description, quirk level and how they say hello |
| Profile | name and nickname (on-screen keyboard), pronouns, who they crush on, birthday month and day, favourite food, hated food, hobby, music, favourite colour, fear, catchphrase |
| Home | moves to Hillvile / Hellvile (new citizens only, and in the story only once the wall is down), born in (heritage, `RF_HELLBORN`), favourite (`RF_FAVORITE`), plus a live preview of the apartment block with their window lit up (floor / room of the slot, free homes) - or the protagonist's HaHaland cottage |

Values show their catalog names (`doomo_*_names`). Lists cycle with
LEFT/RIGHT; A opens a picker with every choice. Colour rows show a swatch
and A opens a swatch grid (8 columns; skin adds the 8 natural tones;
hair highlight has "None" in an 11-column grid). Sliders show their value
and their two ends in the detail line ("Short <-> Tall"). Long values
scroll as a marquee on the selected row.

Changing the species brings that species' parts along (skin, head shape,
ears, nose, mouth, fangs, horns, tail, wings, halo, fur marking, eye count -
`cr_species_base` in `creator_data.c`); eyes, hair and outfit stay.

## Controls

| Button | Main list | Pickers | Turntable |
|---|---|---|---|
| UP / DOWN | row | choice | walk on/off (UP), auto-spin (DOWN) |
| LEFT / RIGHT | change value | page (list) / move (swatches) | turn (8 directions) |
| A | open picker / type / toggle / apply preset | choose | tumble |
| B | back; with changes: "Discard?" | cancel (restores the value) | back |
| L / R | tab | page | - |
| START | creator menu | - | back |
| SELECT (tap) | turntable | - | back |
| X / Y | undo / redo (GBA: SELECT+A / SELECT+B) | - | - |

The START menu: **Move in! / All done!**, Randomise all, Randomise this
tab, Undo (n), Redo (n), Copy a look..., Possessed peek, Start over /
Reset changes, Quit.

## The preview

* Full-body `figure_image()` on a little stage (wallpaper, rug, drifting
  hearts or embers), bouncing on every change with sparkles, hearts and
  stars. It blinks, breathes, wags and animates its aura by itself, and
  the expression cycles through the personality's own face.
* On the Head & Face and Hair tabs the camera zooms in: a close-up bust
  (`portrait_render`, re-rendered only when its animation phase changes;
  on 240x160 the head is cropped and shown at 2x).
* SELECT turns the preview into a turntable (`fighter_render`, 8 views,
  walk cycle, a tumble).
* "Possessed peek" shows the citizen possessed (just for fun).
* A locked option shows as a lavender silhouette with "???" and a bubble
  with its unlock hint.
* The look on screen is throttled (a new figure costs about two GBA ticks),
  so holding a direction stays smooth.

## Locks

`unlock_option_ok()` / `unlock_paint_ok()` decide. A locked option can be
browsed (silhouette, "???", hint in the detail line and the bubble, padlock
in pickers) but never chosen: A is refused with a shake. A value the
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

Hellvile citizens get the cute-goth look (`WIN_DARK` windows, a dusky
violet room with embers, the Hellvile Pits finale).

## Costs

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
