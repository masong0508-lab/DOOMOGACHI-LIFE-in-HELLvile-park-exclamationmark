# DOOMOGACHI customization + options: technical map of the current system

Snapshot: working tree at HEAD `e705a4d` plus the CI fix pass's uncommitted edits
(`core/src/portrait_gfx.c`, `core/src/portrait_int.h`, `gba/gba.ld`, `gba/src/gba_hw.h`,
`gba/src/irq.iwram.c`). I did not modify the repo. Sizes marked "probe" come from a
throwaway compile in `scratchpad/custom/probe/` against the live headers.

---------------------------------------------------------------------------------------
## 0. Ten facts to know before extending anything

1. **Every byte added to `DoomoResident` or `DoomoLook` costs about 1 KB of GBA EWRAM and
   about 2 KB of PS1 RAM.** `doomo_town.res[1000]` plus `player` add 1,001 B of .bss. The
   const roster adds 1,000 B, which sits in ROM on the GBA but in RAM on the PS1. The
   creator's scratch adds 22 B (`draft`, `orig` and `hist[20]`). Right now the GBA game
   link is **33,768 B over** in EWRAM, and IWRAM also eats into the 6 KB stack
   (`scratchpad/mem/gba1.log`).
2. `DoomoLook` takes 48 B (384 bits) of RAM but holds only **210 bits** of information
   (232 bits as saved). That leaves 174 spare bits inside the existing bytes. Using them
   would cost no RAM, but every byte-oriented consumer would have to mask them (see 9.3).
3. **The save format has no migration path.** `save_info_from()` rejects any header whose
   version is not `DOOMO_SAVE_VERSION` (=1) (`core/src/save.c:1144`). Any change to the
   resident bit schema (`rfields[]`, `save.c:513-545`) or to an enum width makes every
   existing save "Not a save file". Extension chunks are the only versioned,
   forward-compatible path (`save_ext.h`).
4. Paint colours are capped at 32 (`PAL_PAINT_COUNT`) by the fixed 256-entry master palette
   layout (`core/include/doomo/palette.h:8-16`: "fixed - art, textures and saves depend on
   it") and by the 5-bit save width of every `*_color` field. Adding colours is a palette
   and save redesign, not a table edit.
5. Sliders are 0..15 everywhere and saved in exactly 4 bits. More resolution needs a save
   version bump.
6. A new option in an existing catalog list needs: the enum + name, a portrait drawing that
   makes it visually distinct (the test enforces it), at least one of the 1000 premade
   citizens wearing it (the test enforces it), a roster regen, and a check against the
   save-width static assert. The creator itself needs nothing.
7. A new look field needs: `resident.h` + sanitize + save `rfields` (version bump) + a
   creator row (the test requires one row per look byte) + the roster generator + the
   portrait renderer + regenerated golden fingerprints. Section 9 has the full checklist.
8. Options are a 64-byte array saved inside the town. 57 are used, so **7 slots are free**
   (`DOOMO_STATIC_ASSERT(OPT_COUNT <= 64)`, `town.h:121`). **8 of the 57 options are
   wired to nothing** (7.4).
9. Unlock bits (1024) are fully allocated to teams. The creator's range 256..319 has
   **43 free ids** (269, 278..319). All 32 `PROG_*` slots are used, so a new
   auto-unlock counter needs code or a save_ext chunk.
10. Determinism: `resident_generate*()` output is pinned by
    `core/tests/golden/fingerprints.txt` (96 seeds). It is also the **delta base of every
    Random-Island save**. Changing the draw order or adding a draw silently changes saved
    random citizens and requires `--update-golden`.

---------------------------------------------------------------------------------------
## 1. Data model

### 1.1 `DoomoLook` (`core/include/doomo/resident.h:17-66`, 48 B, static assert `:124`)

Every field is a `uint8_t`. The `resident.h` field line equals 18 + offset. "save" is the
bit width in `core/src/save.c:513-541`. "min" is ceil(log2(count)) at today's counts. Each
range is enforced by `resident_sanitize()` (`core/src/resident.c:778-869`), and the
renderer applies its own clamp (`core/src/portrait.c:18-46`).

| off | field | RAM range / meaning | count | save | min | creator row (tab) |
|---:|---|---|---:|---:|---:|---|
| 0 | species | SPECIES_* | 24 | 6 | 5 | ENUM CLIST_SPECIES (Body); a change resets the species-base fields |
| 1 | body | BODY_* | 10 | 5 | 4 | ENUM (Body) |
| 2 | height | 0..15 | 16 | 4 | 4 | SLIDER (Body) |
| 3 | build | 0..15 skinny..chonky | 16 | 4 | 4 | SLIDER (Body) |
| 4 | skin | 0..7 PAL_SKIN, 8..39 = paint+8 | 40 | 6 | 6 | RK_SKIN (Body), labelled Fur/Goo/Plating... by species |
| 5 | head_shape | HEAD_* | 12 | 5 | 4 | ENUM (Face) |
| 6 | head_size | 0..15 | 16 | 4 | 4 | SLIDER (Face) |
| 7 | eye_style | EYE_* | 24 | 6 | 5 | ENUM (Face) |
| 8 | eye_color | paint 0..31 | 32 | 5 | 5 | PAINT (Face) |
| 9 | eye_size | 0..15 | 16 | 4 | 4 | SLIDER |
| 10 | eye_spacing | 0..15 | 16 | 4 | 4 | SLIDER |
| 11 | eye_height | 0..15 | 16 | 4 | 4 | SLIDER |
| 12 | eye_tilt | 0..15, 8 = level | 16 | 4 | 4 | SLIDER |
| 13 | eye_count | 1..3 | 3 | 2 | 2 | ENUM base 1 |
| 14 | brow_style | BROW_* | 12 | 5 | 4 | ENUM |
| 15 | brow_height | 0..15 | 16 | 4 | 4 | SLIDER |
| 16 | nose_style | NOSE_* | 12 | 5 | 4 | ENUM |
| 17 | nose_size | 0..15 | 16 | 4 | 4 | SLIDER |
| 18 | mouth_style | MOUTH_* | 20 | 6 | 5 | ENUM |
| 19 | mouth_size | 0..15 | 16 | 4 | 4 | SLIDER |
| 20 | fangs | 0..3 | 4 | 2 | 2 | ENUM (no clist) |
| 21 | hair_style | HAIR_* | 32 | 6 | 5 | ENUM (Hair) |
| 22 | hair_color | paint | 32 | 5 | 5 | PAINT (Hair) |
| 23 | hair_highlight | paint or 0xFF = none (saved as 32) | 33 | 6 | 6 | RK_PAINT_NONE (Hair) |
| 24 | facial_hair | FHAIR_* | 8 | 4 | 3 | ENUM (Hair) |
| 25 | ears | EARS_* | 16 | 5 | 4 | ENUM (Face) |
| 26 | horns | HORNS_* | 16 | 5 | 4 | ENUM (Spooky) |
| 27 | horn_color | paint | 32 | 5 | 5 | PAINT |
| 28 | horn_size | 0..15 | 16 | 4 | 4 | SLIDER |
| 29 | tail | TAIL_* | 16 | 5 | 4 | ENUM (Spooky) |
| 30 | wings | WINGS_* | 10 | 5 | 4 | ENUM |
| 31 | halo | HALO_* | 6 | 4 | 3 | ENUM |
| 32 | marking | MARK_* | 23 | 6 | 5 | ENUM |
| 33 | marking_color | paint (the fur 2nd colour for MARK_FUR_*) | 32 | 5 | 5 | PAINT |
| 34 | cheeks | 8 CHEEK_* bit flags (**all 8 used**) | 256 | 8 | 8 | 8 x RK_FLAG (Face) |
| 35 | top | TOP_* | 24 | 6 | 5 | ENUM (Outfit) |
| 36 | top_color | paint | 32 | 5 | 5 | PAINT |
| 37 | bottom | BOTTOM_* | 12 | 5 | 4 | ENUM |
| 38 | bottom_color | paint | 32 | 5 | 5 | PAINT |
| 39 | shoes | SHOES_* | 12 | 5 | 4 | ENUM |
| 40 | shoes_color | paint | 32 | 5 | 5 | PAINT |
| 41 | hat | HAT_* | 24 | 6 | 5 | ENUM |
| 42 | hat_color | paint | 32 | 5 | 5 | PAINT |
| 43 | accessory | ACC_* (one slot only) | 20 | 6 | 5 | ENUM |
| 44 | accessory_color | paint | 32 | 5 | 5 | PAINT |
| 45 | aura | AURA_* | 8 | 4 | 3 | ENUM (Spooky) |
| 46 | pattern | PATTERN_* (on the top only) | 8 | 4 | 3 | ENUM "Print" (Outfit) |
| 47 | pattern_color | paint | 32 | 5 | 5 | PAINT |

The totals are 232 save bits and 210 minimum bits against 384 RAM bits.

Structural limits of the current shape:
- **One accessory slot, one hat, one marking, one pattern, and the pattern only on the
  top.** Bottoms and shoes have no pattern. The kilt hard-codes plaid
  (`portrait_body.c:502,583`).
- **No per-part colours** for ears, tail, wings, nose, mouth, brows, fangs, inner ear,
  muzzle or paws. They derive from skin, hair, horn or marking colours in `pctx_init()`
  (`portrait.c:55-101`).
- **No left/right asymmetry, no per-eye colour, no eye-shine or pupil choice, no lash or
  brow colour, no secondary hair colour beyond `hair_highlight`.**
- **Cheek flags are full.** A new face decal needs a new byte or a reuse of
  `marking`.

### 1.2 The rest of `DoomoResident` (`resident.h:98-122`, 96 B, static assert `:125`)

The trailing `/* NN */` comments in `resident.h` give *exclusive end* offsets, not start
offsets. The probe shows the start offsets: name 48..59 (11 chars + NUL, `DOOMO_NAME_LEN`
12, `types.h:24`), voice[5] 60..64 (0..31, 5 b), persona[5] 65..69 (0..15, 4 b), stat[8]
70..77 (0..100, 7 b), status 78 (RES_*, 3 b), flags 79 (RF_* 8 bits, all used:
`resident.h:77-85`), pronoun 80 (5 values, 3 b), romance 81 (5, 3 b), bday_month 82,
bday_day 83 (1..28), like_food 84 (48), like_hobby 85 (32), like_music 86 (16),
like_color 87 (paint), hate_food 88, fear 89 (24), catchphrase 90 (64, 7 b), exorcism 91
(two packed nibbles: the precedent for packing), level 92 (0..99), mood 93 (MOOD_ 14),
activity 94 (ACT_ 13), death_cause 95 (DEATH_ 6).

Nicknames do not live in `DoomoResident`. They sit in a 64-entry side table saved as the
`'NICK'` v1 extension chunk (`core/src/creator.c:25-171`).

### 1.3 Catalog lists (`core/include/doomo/catalog.h`; names in `core/src/catalog.c:12-305`)

"gated" counts the distinct `UL_CAT_PART` unlock entries for that list. The tables live
in `unlocks.c` (86 part entries), `unlocks_life.c` (24), `creator_data.c` (21),
`unlocks_freeplay.c` (18), `unlocks_concert.c` (17), `unlocks_minigames.c` (8),
`unlocks_levels.c` (3) and `unlocks_story.c` (1). "save cap" is the static assert in
`save.c:57-88`.

| list | catalog.h | COUNT | save cap | gated | open at start |
|---|---|---:|---:|---:|---:|
| species | 14-23 | 24 | 64 (`:57`) | 3 | 21 |
| body | 26-28 | 10 | 32 | 3 | 7 |
| head | 30-32 | 12 | 32 | 5 | 7 |
| eye | 35-39 | 24 | 64 | 14 | 10 |
| brow | 41-43 | 12 | 32 | 4 | 8 |
| nose | 45-47 | 12 | 32 | 4 | 8 |
| mouth | 49-53 | 20 | 64 | 7 | 13 |
| ears | 55-58 | 16 | 32 | 3 | 13 |
| cheeks (bits) | 61-63 | 8 | 8 bits, full | - | 8 |
| hair | 66-72 | 32 | 64 | 17 | 15 |
| facial hair | 74-76 | 8 | **16** | 4 | 4 |
| horns | 79-82 | 16 | 32 | 8 | 8 |
| tail | 84-87 | 16 | 32 | 5 | 11 |
| wings | 89-91 | 10 | 32 | 5 | 5 |
| halo | 93-94 | 6 | **16** | 4 | 2 |
| marks | 96-103 | 23 | 64 | 9 | 14 |
| tops | 106-110 | 24 | 64 | 13 | 11 |
| bottoms | 112-115 | 12 | 32 | 6 | 6 |
| shoes | 117-119 | 12 | 32 | 7 | 5 |
| hats | 121-126 | 24 | 64 | **22** | 2 |
| accessories | 128-132 | 20 | 64 | 16 | 4 |
| aura | 134-135 | 8 | **16** | 7 | 1 |
| patterns | 137-139 | 8 | **16** | 5 | 3 |
| paints | palette.h:49-58 | 32 | 5 bits, **full** | 6 (`unlocks.c:152-155`, `unlocks_life.c:76-77`) | 26 |
| skins (natural) | palette.h:61-62 | 8 | in skin's 6 bits | 0 | 8 |
| pronoun / romance | 142-146 | 5 / 5 | 8 / 8 | - | - |

That makes 349 discrete part choices in 22 lists, 171 of them gated (~49%). The
`unlocks.h:10` comment ("most options start unlocked") is no longer true for hats,
accessories, auras and patterns.

Non-look identity lists: FOOD 48, HOBBY 32, MUSIC 16, FEAR 24, CATCHPHRASE 64, first
names 256, last names 128 (`catalog.h:164-170`). Persona has 5 axes x 16 levels and
yields 16 derived types (`persona_type`, `resident.c:915`). Voice has 5 axes x 32 levels.

Species behaviour table: `doomo_species_info[]` (`catalog.c:329-355`). It holds the
default skin, the hair/horn/tail/wing chances for the generator, base corruption, holy
affinity and SPF_* flags (FLOATS, NO_SKIN_SHADE, UNDEAD, SQUISHY, MECHANICAL, HOLY).

---------------------------------------------------------------------------------------
## 2. Every consumer of `DoomoLook` fields

Found with a per-field grep for `[.>]field` over core/, gba/src, ps1/src, tools/ and
trailer/. False positives (`->top` UI rects, drum `->hat`, fish `->body`, actor
`->height`) were removed.

### 2.1 Core definition, validation and generation
- **`core/src/resident.c`** reads and writes every field.
  - `gen_look()` `:309-639` writes all 48. Its weighted tables are at `:36-301`.
  - The species-specific fur and skin palettes are at `:62-138`. The cute and hell outfit
    colour schemes are at `:142-174`.
  - `resident_sanitize()` `:778-869` clamps every field. Cheeks are left alone because
    every bit pattern is valid.
  - `resident_default()` `:871-913` builds the "Mochi" catfolk template.
  - `look_paint()` `:923` and `look_skin()` `:931` map colours to palette indices.
  - `resident_fingerprint()` `:939` runs FNV-1a over all 96 bytes.
  - `resident_randomize_look()` `:753` (seed ^ 0x100C0000).
  - `resident_generate_hellborn()` `:744` (seed ^ 0x48E11B0D).
- **`core/src/catalog.c`**: name tables plus `doomo_species_info`.
- **`core/src/town.c:103-116`**:
  - `town_premade_record()` builds the roster record (or a generated stand-in).
  - `town_random_record()` builds the seeded generated record. These are the save delta
    bases.
- **`core/src/save.c:513-545`** holds the bit schema for every field (section 4).
- **`core/src/stubs.c:337-467`** has the weak fallback renderers. They read skin,
  eye_color and top_color (bring-up only).

### 2.2 Renderers
- **`core/src/portrait.c`**:
  - `sanitize()` `:18-46` (all fields).
  - `pctx_init()` `:55-101` turns every colour field plus skin into materials. When the
    hair colour equals the fur colour it darkens the hair.
  - `setup_portrait()` `:192` uses head_size and species, and reads hats and ears through
    `ph_top_extent`.
  - `setup_figure()` `:231` and `render_body_ctx()` `:271` use head_size.
  - `render_fighter_ctx()` `:306` uses head_size, top and pattern (roll ball).
  - The cache keys use `look_hash()` `:500` over all 48 bytes.
- **`core/src/portrait_head.c`** reads species, skin, head_shape, the eye_* fields,
  brow_*, nose_*, mouth_*, fangs, marking(+color), cheeks, facial_hair, hair_style,
  ears and eye_count:
  - `head_shape()` `:194`
  - species switches `:76,107,271,372,509`
  - `one_eye()` `:813`
  - `brows()` `:1126`
  - `nose()` `:1209`
  - `mouth()` `:1362`
  - `markings()` `:1657`
  - `facial_hair()` `:1856`
  - `ph_face()` `:1965`
- **`core/src/portrait_hair.c`** reads hair_style, hair_color, hair_highlight, ears,
  horns, horn_size, horn_color, hat(+color), accessory(+color) for face accessories,
  halo, marking and species:
  - `hair_back_shapes()` `:93`
  - `hair_front_shapes()` `:223`
  - `one_ear()` `:560`
  - `one_horn()` `:745`
  - `ph_hat()` `:886`
  - `ph_face_acc()` `:1239`
  - `ph_top_extent()` `:1391` (ears/hat heights)
  - `ph_halo()` `:1419`
- **`core/src/portrait_body.c`** reads body, height, build, top, top_color, pattern(+color),
  bottom(+color), shoes(+color), tail, wings, accessory(+color) for neck and back, marking
  for fur belly and socks, species and skin:
  - `body_mod[BODY_COUNT][6]` `:30`
  - `pb_layout()` `:51`
  - top tables `:232-280`
  - `foot()` `:299`
  - `legs()` `:464`
  - `bottoms()` `:567`
  - `top_details()` `:669`
  - `collar()` `:899`
  - `pb_draw_tail()` `:1224`
  - `wing_one()` `:1393`
  - `neck_acc()` `:1527`
- **`core/src/portrait_fx.c`** reads aura, species, skin and eye_count:
  `pf_aura_back()` `:60`, `pf_aura_front()` `:148`.
- **`core/src/portrait_gfx.c:979`** holds `p_pattern_tiles[PATTERN_COUNT][64]` (8x8 tile
  per pattern).
- **`ps1/src/model.c`** (the third-person low-poly model, **a subset**) reads:
  - skin, top_color, bottom_color, shoes_color, hair_color and eye_color `:296-303`
  - top==NONE, bottom==NONE, shoes==BARE `:306-308`
  - tail (long vs puff) `:344`
  - wings (angel, butterfly or other) `:354`
  - cheeks != 0 `:371`
  - hair_style (bald or twintails/pigtails) `:377`
  - ears (6 groups) `:386-409`
  - horns (NUBS or DEVIL_TALL height) and horn_color `:411`
  - halo != 0 `:418`

  Its default look is at `:184-200`. It **ignores** species shape, body, height, build,
  head shape and size, the eye params except colour, brows, nose, mouth, fangs, facial
  hair, highlight, horn size, marking, hat, accessory, aura and pattern.
  `ps1/src/coreref.c:125-137` wraps look_paint and look_skin.
- **`ps1/src/etest.c:547`**: `model_set_look()` with roster looks (engine test).

### 2.3 Image call sites
These sites only pass the resident id or look to the portrait API, so new fields need no
code there.
- `portrait_image`: ui.c:733, combat_hud.c:151, mg_draw.c:561, mg_memory.c:240,
  studio.c:431, studio_words.c:533, concert_scene.c:774, cook_scene.c:1040,
  dream_gfx.c:686/759/1038, fp_resident.c:1007, exorcism_scene.c:111, fp_common.c:730
- `bodyspr_image` (chibi): overworld.c:769, overworld_npc.c:385, fp_common.c:703,
  fish_gfx.c:803, story_questlog.c:210, cutscene.c:481-482, credits.c:130/168,
  concert_scene.c:744/951/1046, concert_gfx.c:586, mg_memory.c:265, mg_gallery.c:269,
  studio.c:462, dream_gfx.c:765
- `figure_image`: creator_draw.c:532/1169, lib_almanac.c:546, lib_common.c:936,
  cutscene.c:265, overworld_fx.c:622, concert_gfx.c:809, dream_gfx.c:819,
  exorcism_gfx.c:273, exorcism_scene.c:109, fp_common.c:743
- `fighter_image`: combat.c:1662 (the player only)

### 2.4 Gameplay and UI logic that reads look fields directly
- `core/src/fp_common.c`:
  - `fp_host_look()` `:247` (roster look).
  - `look_hash()` `:656`, also over all 48 B.
  - The chibi, portrait and figure placeholder ball uses `look_skin`
    (`:709,733,746`).
- `core/src/combat_hud.c:304-317`: the first-person paws use skin shades.
- `core/src/sim_social.c:226-253`: babies inherit species, skin, head_shape, eye_style,
  eye_color, nose, hair_color, highlight, ears, horns, horn_color, tail, wings, halo,
  marking and marking_color from a parent, plus cheeks. They are forced to
  BODY_TINY/small/big-headed, bald or messy hair, a 50% pacifier, TOP_PAJAMAS and no hat.
  Everything else comes from `resident_generate*()` called with a fresh seed.
- `core/src/sim_soul.c:125-127`:
  - Holy affinity comes from species.
  - +10 for a halo other than NONE or BROKEN.
  - +15 for ACC_ROSARY or ACC_CROSS_NECKLACE.

  `:162` takes base corruption from species. **These are the only look fields with
  gameplay effects besides species.**
- Species only:
  - `core/src/sim.c:430` (need rates)
  - `sim_care.c:172,308`
  - `exorcism.c:78`
  - `concert_lyrics.c:118`
  - `lib_almanac.c:173/393/430` (sort and label)
  - `fp_resident.c:811`
  - `creator_draw.c:1045`
- `core/src/story.c:144-148`: the default protagonist is generated and forced to bunnykin
  ears, tail and no horns or wings.
- `core/src/exorcism_scene.c:157-169`: the practice dummy look is built in code.
- `core/src/creator.c` and `creator_draw.c` (section 6).

### 2.5 Tools
- `tools/roster_build.py`:
  - Enum parsing from the headers `:152-216`.
  - Field kinds `:208-248`.
  - `KEY_LINES` (the canonical record order and --strict list) `:250-268`.
  - `defaults()` plus per-species coherence `:356-440`.
  - `LOOK_ORDER` `:579-584` sets the **positional** emit order and must match the struct.
  - The duplicate-look check `:719-725`.
  - `emit()` writes positional initialisers (`core/gen/roster_data.c`) `:923-951`.
- `core/data/roster/*.txt` holds the 1000 records as `key=value` lines.
- `core/tools/portrait_sheet.c`: the contact-sheet tool. It sets brow, mouth, hair,
  horns, aura and the colours.
- `trailer/` does **not** use `DoomoLook`. It has its own `TkChibi` (`trailer/tk.h:83`,
  float host-only renderer `trailer/tk_chibi.c`).

### 2.6 Tests
- `core/tests/test_creator.c`:
  - `:68` requires a creator row for every look byte.
  - `:103` requires ENUM row counts to match the catalog.
  - `:149` checks presets.
  - `:188` checks the unlock range.
- `core/tests/test_portrait.c`:
  - `:54-64` lists the catalog lists.
  - `:69-93` requires every option to render distinctly. **`MAXOPT 40` at `:67` silently
    truncates the check for lists over 40.**
  - `:95+` checks options on other citizens.
  - `:420-437` sets the render cost bound at **< 12,000 pixel visits**.
- `core/tests/test_resident.c`:
  - `:30` generates 20,000 seeds.
  - `:67` checks determinism.
  - `:84-130` checks the golden fingerprints.
  - `:142` checks sanitize.
  - `:220` checks catalog tables.
- `core/tests/test_roster.c`:
  - `:125` requires unique looks.
  - `:214-260` requires **every catalog option, every paint (as top, hair and eye colour)
    and all 8 cheek bits to be used by some premade citizen**. Its `seen[24][64]` limits
    each list to 64 options.
- `core/tests/test_save.c:155` checks that the worst case (1000 random custom) stays
  within slot 1 and over slot 0.
- `core/tests/test_integration.c:155` checks that part unlocks reference valid options.

---------------------------------------------------------------------------------------
## 3. How looks are rendered

Everything is **procedural**: there is no stored per-part art (`docs/PORTRAIT.md:3-6`).
The code is integer-only, writes 8bpp indices into the master palette, and draws parts
back-to-front with a per-pixel part-id buffer, inner contours and a selective outline
(`portrait_int.h:1-20`). Coordinates are Q4 sub-pixels, so every slider step changes
pixels.

| image | size | entry | GBA cost avg / worst (PORTRAIT.md:163-169) | used by |
|---|---|---|---|---|
| bust | 48x56 | `portrait_render` / `portrait_image` | 763k / 1.20M cycles | dialogs, lists, HUD |
| figure | 48x80 | `figure_render` / `figure_image` | 915k / 1.38M | creator, almanac, cutscenes, title |
| chibi | 16x24 | `bodyspr_render` / `bodyspr_image` | 250k / 338k | overworld, island, concerts |
| fighter | 32x48 | `fighter_render` / `fighter_image` | 454k / 655k | third-person shooter (GBA) and creator turntable |

A 30 Hz logic tick is 561,792 cycles, so **a bust is more than a whole tick** on the
GBA.

- **Layer order** is `draw_step()` (`portrait.c:135-171`). It runs `DL_STEPS`=6 groups:
  0. aura back, weapon back, wings, tail and long hair, back ears
  1. body (legs, torso, outfit, arms)
  2. head base + face
  3. neck/collar, front hair, forehead marks, front ears
  4. hat, species overlays, horns, face accessories, front paws, weapon front
  5. `finish()`: lines, halo, status FX, expression FX, front aura, ghost or dead recolour
     (`portrait.c:114-131`)

  A new feature has to be placed in one of these groups.
- **Cache**: one pool of 48 x 384 B = **18,432 B** (`portrait.c:391-429`). A portrait
  takes 7 blocks, a figure 10, a fighter 4 and a chibi 1 (static asserts). Entries are
  keyed by resident id + `look_hash` + status/expr/phase. On the GBA, re-renders that
  only change the animation phase are sliced one layer group per tick
  (`PC_STEPS`=1 GBA, 3 PS1, 6 native; `:404-410`). `portrait_invalidate(id)` (`:851`)
  must be called after editing a look; `creator.c:1097` and `save.c:1202,1205` do it.
- **Canvas**: `PCanvas pcv` is about 4.5 KB (probe: 4,496 B). The CI pass is editing it
  right now. Part ids are `uint8`, so `pc_part()` saturates at 253 parts per image
  (`portrait_gfx.c:136-147`) and contours merge beyond that. That is a hidden ceiling for
  heavily layered new features.
- **Free-play render budget**: `fp_common.c:629-690` keeps a 40-entry memo. On the GBA
  it allows 1 new render credit per frame (cap 4); elsewhere 6 per frame (cap 12).
  Otherwise it shows a placeholder ball in skin colour.
- **Creator preview** (`creator_draw.c`):
  - `figure_image(&cr.shown)` `:532` for the full body.
  - `bust_image()` `:328` renders `portrait_render` into the creator's own 2,688 B buffer.
    On 240x160 it is cropped and shown at 2x.
  - `turn_image()` `:354` is the 8-direction turntable via `fighter_render`.
  - `anim_phase()` `:312` lists which look values animate (aura, sparkle cheeks,
    star/flame halo, flame hair). A new animated part must be added there or it freezes
    in the bust.
  - `preview_update()` (`creator.c:758-779`) throttles to about 1 render per 5 ticks
    while a direction is held.
- **Third person**:
  - **GBA**: a `fighter_image` billboard at scale 300 (`combat.c:1650-1668`), fully
    procedural, so every look field shows. The integration notes measure it at 9-10 ms
    in heavy scenes.
  - **PS1**: `ps1/src/view3d.c:438-440` replaces the player billboard with
    `model_player_draw()` (`ps1/src/model.c:248-422`), built from boxes, blobs and
    pyramids with gouraud shading. **New look features are invisible in the PS1 third
    person unless `model.c` gets code** (2.2 lists what it reads).
- **First person**: only the paw colour (`combat_hud.c:304`).

**Where a new feature needs code:**
- a new enum value: a `case` in the part painter for that list (2.2 has the functions)
- a new colour field: a `PMat` in `PCtx` (`portrait_int.h:185-186`) plus `pctx_init`
- a new slider: the geometry maths in the painter
- a new list: a new painter function plus a layer slot in `draw_step`, the PS1
  `model.c` (optional), `anim_phase` if it animates, and `ph_top_extent()` if it
  sticks up above the head

Pattern tiles are the only table art (8x8 x 8 patterns, `portrait_gfx.c:979`).

---------------------------------------------------------------------------------------
## 4. Save format (`core/include/doomo/save.h`, `core/src/save.c`)

- **Header** (48 B, uncompressed, `save.c:3-10`): magic "DMOO", u16 version
  (`DOOMO_SAVE_VERSION 1`, `save.h:19`), header size, total, CRC-32 of [16,total), raw
  stream size, mode/chapter/pop/weekday, residents, minute, day, money, player name.
- **Body**: an LZ77 stream (4 KB window, `save.c:194-436`) around a LSB-first bit
  stream. It holds, in order (`write_town()` `:724-813`):
  - town scalars
  - inventory
  - raw weapons
  - pet
  - story
  - **`opt[64]` raw** (`:741`)
  - **`unlocks[128]` raw**
  - `progress[32]` u32
  - graves
  - edges (sorted delta varints)
  - the **player as a full record**
  - the 1000 residents, each with a 2-bit tag: 0 = empty run, 1 = run identical to base,
    2 = **delta vs base** (11-bit group mask + 8-bit field masks + changed fields),
    3 = full record (`:774-812`)
- **Resident schema** `rfields[]` (`:513-541`): 85 fields of fixed width, guarded by the
  static assert `:545` (`== 48+1+5+5+8+18`). The name is 4-bit length + 7-bit chars.
  `hair_highlight` 0xFF maps to 32 (`:554-565`). Costs:
  - a fully custom citizen is about 57-61 B
  - an untouched premade citizen is a fraction of a bit
  - the GBA doc gives the worst case (1000 random custom) as 58.6 KB (`docs/GBA.md:471`)
- **Delta base** (`base_of()` `:672-679`):
  - a premade island deltas against the roster record
  - a Random Island deltas against `town_random_record(seed,id)`, which is
    `resident_generate*()`
  - an Empty Island always writes full records

  So **changing `roster_data.c` or the generator changes how existing saves decode.**
  Untouched citizens adopt the new base, and delta citizens get new values in every field
  they did not change.
- **Slot limits** (`save.h:11-12`): slot 0 (story) is 40 KB, slot 1 (free play) is 88 KB.
  An oversized save fails cleanly ("Island too big").
- **CRC**: IEEE CRC-32 (`:94-153`). Streaming saves combine the header and body CRCs with
  `crc32_combine` (`:156-192`).
- **Load**: magic, version and CRC are checked, then a dry-run decode, then the real
  decode (`:1192-1209`). After that every resident goes through `resident_sanitize`, every
  portrait is invalidated, and `rel_normalize` runs. **The version check is a strict
  equality (`:1144`) with no migration code**, so a v2 reader would have to keep a v1
  `rfields` table and pick by header version.
- **Extension chunks** (`save_ext.h`, `save_ext.c`):
  - Format: `[tag u32][version u16][len u32][bytes]` after the town data, inside the
    same LZ + CRC (`save.c:936-1003`).
  - Unknown tags are skipped. A registered chunk missing from the save gets `reset()`.
    A chunk whose `read()` fails is reset and does not fail the load. Each chunk has its
    own version, so this is the **only migration-friendly mechanism**.
  - Max 8,192 B per chunk (`DOOMO_SAVE_EXT_MAX`, staged in the scratch).
  - The registry has **5 hard-coded weak slots** (`save_ext.c:4-24`): studio 'SONG',
    extra = creator 'NICK', test, freeplay 'FPLY', fishcook 'FISH'. A new chunk needs a
    new weak pointer, a `case` and a bump of the loop bound 5 (`save_ext.c:28,34`).
- **Options and unlocks are per save**: `read_town` memsets the town and reads `opt` and
  `unlocks` from the slot (`save.c:828,857-858`). `town_new()` keeps them
  (`town.c:470-481`). So loading the story slot vs the free-play slot switches the whole
  option set and unlock set. There is no global settings store.

---------------------------------------------------------------------------------------
## 5. Memory

- **Where the 1000 residents live**: `DoomoTown doomo_town` (`town.h:148-180`) is
  109,524 B (probe). `res[]` sits at offset 13,424 (96,000 B), `player` at 109,424 and
  `opt` at 558.
  - **GBA**: it is .bss in **EWRAM** (256 KB).
  - **PS1**: it is .bss in the 2 MB main RAM.
  - The 1000 premade records are `const doomo_roster_premade[1000]` (96,000 B of records;
    164 KB with bios, jobs, traits and edges, `docs/PS1.md:204`). They are **ROM on the
    GBA but RAM on the PS1**.
- **GBA budget table** (`docs/GBA.md:136-160`, stale vs the live tree): EWRAM
  246,536 / 262,144 with the top users
  - doomo_town 109,524
  - doomo_map 32,780
  - portrait pool 18,432
  - gba_big 17,408
  - weld view_pix 12,288
  - sim 7,084
  - actors 6,144
  - portrait pcv 4,480
  - ...

  `docs/GBA.md:177-184` notes that the post-perf modules (creator, studio, story
  overworld, free play, library, minigames, concert) add about 43 KB of .bss.
- **What the CI-fix pass is fighting**:
  - The live tree's game ROM fails to link with "region `ewram' overflowed by 33768
    bytes" plus "IWRAM overflow: code/data eat into the 6 KB main stack"
    (`scratchpad/mem/gba1.log`).
  - `scratchpad/integration_todo.md` lists the .bss contributors (creator 6.3K, fp_*
    ~10.3K, song_book 5.4K, overworld 4.2K+3.3K+3.0K, lib_common 2.1K, mg 2.1K, studio
    1.9K, ...) and the fix direction: move per-scene state into `scene_scratch()`
    (12 KB, `scene.h:41`) and shrink caches. The portrait canvas files are currently
    being edited.
  - On the PS1, `docs/PS1.md:153-205` shows the game disc 181 KB under the limit before
    the newest modules. The integration TODO notes the engine-test disc overflows 2 MB.
    The fix in progress is an **overlay split** (`scratchpad/ovl_try.py`, not committed).
    Its groups put `creator`, `creator_draw` and `creator_data` in one CD overlay. Creator
    tables (presets, rows, icons) would then cost overlay space, not resident RAM. The
    exception is `doomo_unlocks_creator`, which `unlocks.c` reaches, so it stays
    resident.
- **The creator's RAM**: `CrState` is 6,560 B (probe). It lives in the 12,288 B scene
  scratch, not .bss (`creator_int.h:190-197`; assert `creator.c:22`). It holds
  22 x DoomoResident (2,112 B), pix 2,688, icons 1,089 and the rest. **About 5.7 KB of
  scratch is free**, so creator-side buffers are cheap, but each byte of `DoomoResident`
  costs 22 B here. The nickname table is static .bss (64 x 16 B).
- **Cost model for designers** (per resident byte, ignoring save bits):

| change | GBA EWRAM | GBA ROM | PS1 RAM | save (worst case) |
|---|---:|---:|---:|---:|
| +1 B in DoomoLook or DoomoResident | +1,001 B (+1 B portrait Job copy) | +1,000 B roster | ~+2,001 B | +N bits x 1000 citizens / 8 (pre-LZ) |
| +1 enum value in a list | 0 | name string + painter code + roster text | same, in RAM | 0, if within the width cap |
| +1 game option (spare slot) | 0 | strings | strings | 0 (the 64 B array is always saved) |
| +1 creator preset or row | 0 | ~40 B or 12 B | overlay (when it lands) | 0 |

---------------------------------------------------------------------------------------
## 6. Creator UI architecture

Files: `core/src/creator.c` (logic, 1,762 lines), `creator_data.c` (const tables),
`creator_draw.c` (drawing), `creator_int.h` (glue). The docs are `docs/CREATOR.md`
(Extending at `:198-207`).

- **Scene args** (`creator.c:1667+`): -1 = new Hillvile citizen, -2 = new Hellvile
  citizen, -3 = protagonist (`DOOMO_PLAYER_ID`), 0..999 = edit or fill that slot.
  Callers:
  - `fp_island.c:531,533,705`
  - `fp_apartments.c:122`
  - `fp_resident.c:484`
  - `overworld.c:291`
- **Tabs** (`creator_int.h:14-16`, `creator_data.c:183-193`): Body 8 rows, Face 30,
  Hair 7, Spooky 13, Outfit 16, Voice 8, Heart (persona) 5, Profile 15, Home 3. That is
  105 rows, 86 of them selectable. The tab strip fits **at most 10 tabs** at either
  resolution without layout changes (`creator_draw.c:27-29`: 21 or 29 px cells). Each
  tab needs an 11x11 icon (`creator_data.c:665`).
- **The row model** (`creator_int.h:22-49`): `CrRow { kind, field, count, base, clist,
  aux, label, names }`. `field` is a **uint8 `offsetof(DoomoResident, …)`**, so every
  editable thing must be one addressable byte. `CR_NOFIELD`=0xFF, which means the struct
  must stay ≤ 255 B. Kinds:
  - HEADER
  - ENUM (`base + index`, `names[]`, `clist` for locks)
  - SLIDER (0..count-1)
  - PAINT (0..31)
  - PAINT_NONE (0 = none)
  - SKIN (0..39)
  - FLAG (bit `aux` of a look byte, used for cheeks)
  - RFLAG (bit of `flags`)
  - NAME
  - NICK
  - ACTION (CA_SPECIES_PRESET, CA_OUTFIT_PRESET, CA_VOICE_PRESET, CA_TEST_VOICE,
    CA_MATCH_FUR)
  - DISTRICT

  There is **no "bitfield" or "masked sub-byte" kind yet.**
- **How a field becomes a row**: add one macro line to the tab's array
  (`creator_data.c:48-178`):
  - `ENUM_L(label, field, COUNT, names, CLIST_x)`
  - `SLIDE_L(label, field, 16, n_lowhigh)`
  - `PAINT_L(label, field)`
  - `CHEEK(label, bit)`

  The generic code then handles value get and set (`cr_row_value` `creator.c:391`,
  `cr_row_set` `:416`) and lock checks. It also covers names and the picker (`:524`),
  undo (`commit` `:593`, 20-snapshot ring), randomise-tab (`:860`, which copies the byte
  from a fresh `resident_generate*` roll), fix_locks (`:785`), copy-a-look
  (`copy_editable` `:820`, which copies the whole `look`) and the tests.

  Limits:
  - **rows per tab < 64**, because of the undo key `cr.tab*64+row` (`:1382`)
  - `nrows` is uint8
- **Presets** (`creator_int.h:62-77`): `(field,value)` byte pairs ending at CR_NOFIELD,
  at most 19 pairs (`CR_PRESET_KV 40`).
  - 72 species presets, exactly 3 per species in species order (`creator_data.c:212-502`)
  - 20 outfit presets (`:511-573`)
  - 12 voice presets (`:581-595`)

  `cr_species_base` (`:201-210`) lists the 14 fields reset when species changes (skin,
  head_shape, ears, nose, mouth, fangs, horns, horn_color, tail, wings, halo, marking,
  marking_color, eye_count). After the reset it applies the species' first preset to
  those fields (`species_change()` `creator.c:322`). New species-defining fields belong
  in that list.
- **Commit** (`write_town()` `creator.c:1055-1104`):
  - `resident_sanitize` runs first.
  - A new citizen gets the record copied whole. An edited one gets `copy_editable()`, so
    the look, name, voice, persona and profile fields change while stats stay.
  - It then calls `portrait_invalidate`, sets the nickname, adds `PROG_CITIZENS_CREATED`
    and grants the creator secrets.

### 6.1 Unlock gating of parts (`core/include/doomo/unlocks.h`, `unlock_ids.h`, `core/src/unlocks.c`)
- `DoomoUnlock { id(0..1023), cat, list, index, name, how, prog, need }`
  (`unlocks.h:42-54`).
  - Creator parts use `UL_CAT_PART`, `list = CLIST_*` (`unlocks.h:32-40`, 22 look lists +
    9 weapon lists = 31) and `index` = the enum value.
  - Paints use `UL_CAT_PAINT`.
  - Options use `UL_CAT_SKIN` (list = OPT id, index = value) and `UL_CAT_CHEAT`
    (index = OPT id).
- **An option with no entry is open.** `unlock_option_ok()` (`unlocks.c:363`) and
  `unlock_paint_ok()` (`:368`) decide. Up to 16 alternate routes can gate the same part
  (`UL_ALIAS_MAX`, `unlock_ids.h:91`); owning any of them opens it.
- Auto-grant happens when `progress_get(prog) >= need`. With `need == 0` only code can
  grant (`unlock_grant()`). **PROG slots: 32 of 32 used** (`unlocks.h:114-128`; the
  `unlock_ids.h:93-99` comment still says 31).
- **Creator behaviour** (`cr_value_locked()` `creator.c:447-473`):
  - Locked values show as silhouettes with the `how` hint.
  - Locked values are **grandfathered** when the edited citizen already had them
    (it compares against `cr.orig`).
  - Locked presets are blocked.
  - The randomiser runs `fix_locks()`.
  - **Generated and premade citizens ignore locks.**
- **Id ranges** (`unlock_ids.h:12-61`):
  - creator 256..319 is used for 256..268 and 270..277 (`creator_data.c:790-823`;
    `CRU_*` at `creator_int.h:94-96`), so **43 ids are free**
  - foundation 202..239 is spare
  - action 564..607 is spare

  `core/tests/test_integration.c:77-182` enforces uniqueness and ranges.

---------------------------------------------------------------------------------------
## 7. Options system

- **Storage**: `uint8_t opt[DOOMO_OPT_BYTES=64]` in `DoomoTown` (`town.h:130,163`). It
  is saved raw, so it is per save slot (section 4). Use `opt_get()`, which falls back to
  the default when a value is out of range, and `opt_set()`, which clamps
  (`core/src/options.c:120-134`). `opt_defaults()` (`:136`) is called once at boot
  (`game.c:41`).
- **Schema**: an append-only enum (`town.h:89-120`), `OPT_COUNT` = **57**, static assert
  ≤ 64 (`:121`). `DoomoOptionInfo { name, count(uint8), def, values[], page }`
  (`town.h:131-138`) has one row per option in `doomo_option_info[]`
  (`options.c:60-118`). Value-name arrays are at `options.c:12-55`.
- **Pages** (`OPTPAGE_*`, `town.h:123-124`; names `options.c:7-10`):

| page | count |
|---|---:|
| Game | 7 |
| Shooter | 15 |
| Life Sim | 7 |
| Audio | 4 |
| Display | 7 |
| Accessibility | 5 |
| Cheats | 12 |
| **total** | **57** |

  The UI allows `ROWS_MAX 24` per page (`fp_options.c:13`).
- **The UI** (`SCENE_OPTIONS`, `core/src/fp_options.c`) is fully generated from the
  table:
  - one tab per page (L/R), LEFT/RIGHT/A to change a value, and a last row "Reset page
    to defaults" (Monster Mode excluded)
  - per-option help text `opt_help[OPT_COUNT]` (`:15-73`; a NULL entry shows "")
  - volume bars for the three volume options (`:233-238`)
  - Monster Mode needs "I AM A MONSTER" typed in plus a confirm (`:104-149`)
  - locked values are skipped when cycling through `value_ok()` (`:81-86`: UL_CAT_CHEAT
    for OPT_CHEAT_*, UL_CAT_SKIN otherwise), and the help line shows "Next: ??? (how)"

  It opens from the title (`fp_title.c:179`), the island (`fp_island.c:556`) and the
  story pause (`overworld.c:601`). The shooter's pause menu exposes 7 options in place
  (`combat_scene.c:57-79`: shoulder, controls, invert, aim, crosshair, HUD, gore).
- **The full list**: GORE, MONSTER_MODE, DIFFICULTY, MUSIC_VOL, SFX_VOL, VOICE_VOL,
  VOICE_BABBLE, TEXT_SPEED, SIM_SPEED, DAY_LENGTH, AUTOSAVE, CAMERA_DEFAULT,
  CONTROL_SCHEME, INVERT_LOOK, LOOK_SPEED, AIM_ASSIST, LOCK_ON, CROSSHAIR,
  CROSSHAIR_COLOR, FOV, HEAD_BOB, SCREEN_SHAKE, DAMAGE_NUMBERS, HIT_MARKERS, HUD_LAYOUT,
  SHOULDER, BLOOD_COLOR, CONFETTI_AMOUNT, DEMON_DENSITY, POSSESSION_RATE, EVENT_RATE,
  WEATHER, LCD_FILTER, WINDOW_SKIN, FONT_STYLE, SUBTITLES, COLORBLIND, REDUCE_FLASH,
  PET_NOTIFICATIONS, NEWS_TICKER, CLOCK_24H, TITLE_THEME, CURSOR, CAMERA_FILTER,
  UI_BOUNCE, then 12 cheats: BIG_HEAD, CONFETTI, INFINITE_CANDY, CUTE_DEMONS,
  BRUTAL_RESIDENTS, LOW_GRAV, PAPER_MODE, GOD_MODE, RICH, DISCO, TINY_DEMONS,
  ALL_GHOSTS.

### 7.4 Options that do nothing today
Each of these is referenced only by the options tables and the unlock tables. A grep of
core/src, gba/src and ps1/src finds no consumer:
- `OPT_LCD_FILTER`
- `OPT_SUBTITLES`
- `OPT_COLORBLIND`
- `OPT_CAMERA_FILTER` (5 unlockable values in `unlocks.c:183-187`, no effect)
- `OPT_CHEAT_BIG_HEAD`
- `OPT_CHEAT_BRUTAL_RESIDENTS`
- `OPT_CHEAT_PAPER_MODE`
- `OPT_CHEAT_ALL_GHOSTS`

These are the cheapest "more options" wins because the storage, UI and unlocks already
exist. BIG_HEAD and ALL_GHOSTS would hook into `portrait.c` (head_size or status at
`pctx_init`/setup). Note that cached images key on the look hash, so a global render cheat
must also go into the cache key or call `portrait_cache_reset()`.

### 7.5 Controls and remapping
**There is no remapping.**
- `core/include/doomo/input.h` defines 14 fixed `BTN_*` bits. On the GBA, X and Y are
  synthesized as SELECT+A and SELECT+B (`input.h:4-6,21`).
- The platform maps are hard-coded: `gba/src/main.c:135-146` and `ps1/src/pad.c:36-55`
  (the PS1 analog stick acts as a d-pad at a ±48 threshold).
- Auto-repeat is fixed at a 12-tick delay and every 3 ticks after (`core/src/input.c:14-15`).
- Per-scene bindings are hard-coded. The only control options are CONTROL_SCHEME
  (classic/modern), INVERT_LOOK, LOOK_SPEED (10), AIM_ASSIST (4), LOCK_ON (hold/toggle)
  and SHOULDER, all read in `combat.c:518-552`.
- A remap feature would need a binding table (one byte per action) in a spare option
  slot or a save_ext chunk, plus a translation step in `input_update()`.

---------------------------------------------------------------------------------------
## 8. Determinism and parity constraints

- Core rule (`ARCHITECTURE.md:41-43`): anything seeded uses a local `DoomoRng`
  (`core/src/rng.c`, 16x16 multiply ranges at `:34-49`), with no floats and identical
  results on native, GBA and PS1.
- **The golden file** is `core/tests/golden/fingerprints.txt`. It holds 96 seeds (0..63
  and 0x9E3779B9 x 1..32) with FNV-1a over all 96 bytes of `resident_generate(seed)` and
  `resident_generate_hellborn(seed)`, checked by `test_resident.c:84-130`. You regenerate
  it with `$(BUILD)/tests --update-golden fingerprint_golden_file`. **Any** change to
  `gen_look`/`gen_rest` invalidates it: a new field, a new weighted entry, a reordered or
  extra `rng_*` call, or a change to `doomo_species_info` chances.
- The cross-platform guarantee rests on identical code compiled by `core/tools/xcheck.sh`
  (gcc, arm-none-eabi and mipsel at -Werror). I found no on-device fingerprint check in
  gba/, ps1/ or .github/. The GBA harness only checks save round-trips by town CRC
  (`gba/src/hcmd.c:64-82`, `gba/harness/scripts/coresave1.txt`). Portrait renders are
  documented as bit-identical between GBA and native (`docs/PORTRAIT.md:191`).
- Generator changes propagate to:
  - Random-Island saves (delta base)
  - babies (`sim_social.c:223-224`)
  - creator "randomise" (`creator.c:839-893`)
  - `resident_randomize_look`
  - the roster fallback when the roster is not linked (`town.c:107-108`)
  - the GBA harness `town_fill` worst case (`gba/src/hcmd.c:19-52`)
- Roster changes change premade-island saves and require `make -C core gen`. CI runs
  `check-gen` (`core/Makefile:135`), so the committed `core/gen/roster_data.c` must
  match.
- Order of draws: `gen_look` draws the scheme, then species, then fur/skin, and so on, in
  a fixed sequence (`resident.c:309-639`). **Append new draws at the very end of
  `gen_rest` or behind a separate rng** (for example a second `DoomoRng` seeded from
  `seed ^ K`) if the existing citizens' looks must stay stable. The golden file changes
  either way, because the fingerprint covers all bytes.

---------------------------------------------------------------------------------------
## 9. Extension checklists (derived from the above)

### 9.1 Add an option to an existing list (e.g. a new hat)
1. `catalog.h`: append before `*_COUNT`, never reorder. Add the name in `catalog.c`.
2. Check the save cap (`save.c:57-88`). For example HALO, AURA, PATTERN and FHAIR max out
   at 16 and BODY at 32. Exceeding a cap means a save version bump.
3. Painter: add a `case` in the list's function (2.2). Use `ph_top_extent()` if it is
   tall, `anim_phase()` in creator_draw if it animates, and the cost bound of 12k visits.
4. Optional: generator weights (`resident.c:197-301`). This changes the golden file.
5. Roster: at least one premade citizen must use it (`test_roster.c:214`). Edit
   `core/data/roster/*.txt`, then run `make -C core gen`.
6. Optional: an unlock entry (creator ids 269, 278..319).
7. Optional: PS1 `model.c` support.
8. If the list now exceeds 40 entries, raise `MAXOPT` in `test_portrait.c:67`.

### 9.2 Add a new look field, the expensive way (new byte)
1. `resident.h` (DoomoLook) shifts every later offset, the static asserts (48/96), and
   `DoomoResident` to 97 B or more. That costs about +1 KB GBA EWRAM and +2 KB PS1 RAM per
   byte, and EWRAM is already over.
2. `resident_sanitize` and `portrait.c:sanitize`.
3. `save.c` `rfields[]`, the static assert `:545`, and `DOOMO_SAVE_VERSION` (+ a v1
   reader if old saves must load).
4. `creator_data.c`: a row (required by `test_creator.c:68`). Possibly
   `cr_species_base` and presets.
5. `tools/roster_build.py`:
   - `Schema.fields`
   - `KEY_LINES`
   - `LOOK_ORDER` (positional, must match the struct)
   - `defaults()`

   Then regen `roster_data.c`.
6. Generator (`gen_look`) and regenerate the golden fingerprints.
7. Painter + `PCtx` material, the PS1 `model.c`, and the baby inheritance
   (`sim_social.c`).
8. Tests:
   - add it to the `test_portrait.c` lists if it is an enum
   - `test_roster.c` usage
   - the `test_save.c` worst-case size against slot 1 (88 KB)

### 9.3 Add a new look field, the zero-RAM way (bits inside existing bytes)
Spare bits per byte today:
- eye_count (6), fangs (6), halo (5), aura (5), pattern (5), facial_hair (5)
- body, head_shape, brow_style, nose_style, ears, horns, tail, wings, bottom and shoes
  (4 each)
- skin (2)
- each paint field (3)

Every byte-oriented consumer would need masking:
- a new `RK_*` kind with shift/mask (rows address whole bytes)
- `resident_sanitize` (`fix_max`/`fix_enum` would wipe the high bits)
- the save `RField` (shift/mask + version bump)
- `roster_build.py` packing in `emit()`
- `portrait.c:sanitize` and every painter that reads the byte
- `sim_social` inheritance
- `look_hash` is fine because it covers whole bytes

`exorcism` (two nibbles) is the in-struct precedent.

### 9.4 Add a game option
1. Append the `OPT_*` before `OPT_COUNT` (7 free slots left).
2. Add a row in `doomo_option_info[]` (same order), the value names, and `opt_help[]` text
   in `fp_options.c`.
3. Optional: gate values with `UL_CAT_SKIN` (list = OPT id, index = value) or
   `UL_CAT_CHEAT`.
4. Wire a consumer with `opt_get()`.

Past 64 options you need a save version bump (raw 64 B array) or a save_ext chunk with its
own UI hook.

### 9.5 Add a creator tab
1. `CR_TAB_COUNT` enum, `cr_tabs[]`, an icon in `cr_icons`. Only one more tab fits the
   strip.
2. `CrState.row/top/icon_pix` grow automatically, and the scratch has room.
3. Rows still must be one byte each.
