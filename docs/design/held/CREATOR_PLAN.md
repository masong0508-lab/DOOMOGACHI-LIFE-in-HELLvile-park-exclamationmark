# Creator expansion plan: 1,600 new parts and a creator that can browse them

Owner ask: "in the creator screen, add at least 1.5k new parts and accessories, and improve the creator screen furthermore".
This is a plan only; no game code was changed. Tree: `origin/claude/nice-allen-gr6czm` at `4851147`, read in a scratch worktree
(`.claude/worktrees/scout-creator`). It builds on `docs/design/customization/SPEC.md` (P0-P6), `P0_M0A_STATUS.md`, ROADMAP
"Refresh 2026-11-06" and "Customization P1.M2 status", `docs/CREATOR.md` and `docs/PORTRAIT.md`. Where this plan and SPEC
differ, this plan says so and why.

---

## 0. In short

* **Today the creator offers about 351 part choices** across 22 lists (plus 8 cheek flags, 4 fang sizes, 3 eye counts, 78
  species presets, 20 outfit sets, 12 voice presets). **About 45 more exist as painters but cannot be chosen** (13 wave-2 deep
  fields and 6 halos), and 12 more list appends (169 values) are planned in SPEC but not painted.
* **Target: 1,611 new parts, 1,566 of them brand new** (the other 45 are the painted-but-hidden ones made selectable). None is
  a palette swap: colour stays in the colour picker, and a "shape hash" test rejects any part that differs from another only
  in colour.
* **524 of them cost nothing in the save and nothing in RAM**: they are list appends that fit today's gene widths (hat 6 bits
  = 64 values, top 6, hair 6, brows 5 ...) and the 11 new core gene fields that already have bits (pupil, sclera, glasses,
  outer, legwear, blush style, fur pattern, ear fold ...). They can ship before any save-format work.
* **1,087 are "deep" parts** (neck, back, mask, props, face paint, hair accessories, pins ..., plus page-2 of hats/tops/hair)
  stored as 2-byte entries in the SPEC's sparse `LOOK` pool. That needs P0.M1 (saves v2 + pool), and on the GBA it needs
  EWRAM the tree does not have today (measured **3,556 B free**; the pool is 8,200 B). Plan: reclaim ~12.5 KB of island-hub
  record lists into the arena, or ship a 4 KB GBA pool.
* **Budgets:** GBA ROM is no issue (4.30 MB used of 32 MB; this costs ~0.6 MB with icons and translations). GBA EWRAM and PS1
  RAM are the hard limits. PS1 is about 9.8 KB above its stack reserve, so PS1 part data cannot be resident: it goes to SPU RAM
  (about 400 KB unused) or the disc, behind a per-render part arena, and names/tags/icons live in the creator overlay or on disc.
* **Creator UX:** a visual icon-grid picker with section pills, theme/tag filters, search by letters, favourites, recents,
  "new" badges, set headers with "wear the whole set", slot-aware camera (bust for hats, legs for shoes, back view for
  backpieces), randomise by theme, saved outfits, hold-to-compare, delta undo, and a speed pass (lock bitmaps, prerendered
  icons, debounced figure renders) so the GBA scrolls at 20+ fps instead of 13.4.
* **Work packages CR0-CR14** (with CR5a): 2 XL, 4 L, 5 M, 5 S. First playable slice (CR1-CR3 + CR6/CR7 core): the 524 gene parts in
  a new grid picker, with no save change.

---

## 1. What was looked at, and measured

* Read: the docs above, `core/src/creator{,_data,_draw}.c`, `creator_int.h`, `catalog.h`, `resident.h`, `gene.h`,
  `core/data/look/fields.txt`, `tools/look_build.py`, `tools/parts_build.py`, `parts.h`, `part_lru.c`, `look_fields.h`,
  `look_pool.{h,c}`, `portrait_*.c` (structure), `ps1/tools/ovl_plan.py`, `docs/PS1.md` memory sections, `docs/GBA.md`.
* Built the native runner and ran `creator.txt` and `creator_hellvile.txt` at 240x160 and 320x240 (all four runs pass). Shots and
  3x/2x contact sheets of them: `scratchpad/week2/creator/{240x160,320x240}/...`, `creator/g_a.png`, `g_b.png`, `g_c.png`,
  `p_a.png`. Built `portrait_sheet` and rendered today's hats (`creator/sheets/opt_hat.png`).
* Built the GBA ROM here (`make -C gba rom sizes`, arm-none-eabi-gcc, at `4851147`):
  **ROM 4,299,852 B** (of 32 MB), **IWRAM 6,356 B left for the stack**, **EWRAM 258,588 / 262,144 used, 3,556 B free**
  (`doomo_town` 89,504; arena 26,992; map 32,780; portrait pool 18,432; island hub lists `lr_land_r` 5,128 + `lr_patch_r`
  2,568 + `lr_road_r` 1,288 + `dv_pool` 3,208 + `dv_tmp` 320; creator `nicks` 1,024; look-pool shim 384).
  `sizeof(CrState)` on ARM = **5,992 B** of the 12,288 B scene scratch.
* PS1 was not built (no PSn00bSDK here). Numbers come from `docs/PS1.md` and the log: `76b86bf` says the disc was 2,368 B over
  the 64 KB stack reserve and returned ~12.2 KB, so it is **about 9.8 KB above the reserve** now (confirm on CI). Creator overlay
  group ~55.7 KB in a 118,784 B slot. SPU RAM: 0x02000-0x7dfff (~496 KB) is the SFX area; the SFX use a fraction of it.

---

## 2. Inventory

### 2.1 Selectable today (live counts, `catalog.h`)

| Tab | Category | Count | Gene width (values) |
|---|---|--:|---|
| Species & Body | species | 26 | hot byte |
| | species presets | 78 (3 x 26) | - |
| | body types | 10 | 4 b (16) |
| | height, build | sliders 16 | 4 b |
| | skin / fur / goo / plating | 8 natural + 32 paints (colour) | 6 b |
| Head & Face | head shapes | 12 | 5 b (32) |
| | eye styles | 24 | 6 b (64) |
| | eye count | 1-3 | 3 b (8) |
| | brows | 12 | 5 b |
| | noses | 12 | 5 b |
| | mouths | 20 | 5 b |
| | fangs | 4 | 2 b |
| | ears | 16 | 5 b |
| | cheek flags | 8 on/off | 8 b |
| Hair | hair styles | 32 | 6 b (64) |
| | facial hair | 8 | 4 b (16) |
| | highlight | none + 32 (colour) | 6 b |
| Supernatural | horns | 16 | 5 b |
| | tails | 16 | 5 b |
| | wings | 10 | 5 b |
| | halos | 6 | 4 b (16) |
| | markings (incl. 7 fur marks) | 23 | 5 b |
| | auras | 8 | 5 b |
| Outfit | tops | 24 | 6 b (64) |
| | prints (patterns) | 8 | 5 b |
| | bottoms | 12 | 5 b |
| | shoes | 12 | 5 b |
| | hats | 24 | 6 b (64) |
| | accessories (one mixed slot: glasses, masks, neck, back, bits) | 20 | 5 b (32) |
| | outfit presets | 20 | - |
| Voice / Personality / Profile / Home | sliders, presets, texts | not parts | - |

**Part choices today: 351 list values** (+8 cheek flags, 4 fang sizes, 3 eye counts). The creator has 9 tabs and 105 rows.

### 2.2 Exists but not selectable

| What | Values | State |
|---|--:|---|
| Halos B76 (Double Ring, Heart, Square, Neon Glitch, Flower Ring, Saturn) | 6 | painted, `HALO_ALL_COUNT` 12, live count still 6 |
| Wave-2 deep painters (shim `DF_*`): pupil 4, sclera 2, blush2 4, glasses2 4, ear fold 7, hood head 3, lash 3, lid 3, liner 3, bags 3, freckle 3, mole 3, scar2 3 | 45 | painted, tested "distinct", read from a 384 B in-RAM shim pool that nothing writes; no creator row, no save field |
| `DF_MAKEUP`, `DF_FACEPAINT`, `DF_AURA2` | 0 | ids only (M3) |
| The 12 pending M2 list appends: F01 heads +12, F07 eyes +16, F25 eye count to 6, F29 brows +12, F35 noses +12, F47 mouths +12, H01 hair +32, H13 facial hair +8, E01 ears +16, B66 horns +10, B79 auras +12, O25 hats +24 | 169 | planned, not painted |
| P2 list appends (B01 bodies +6, B52 tails +16, B59 wings +10, O03 tops +26, O06 bottoms +12, O12 shoes +12, O45 prints +24) | 106 | planned, not painted |
| New core gene fields with bits but no row and no painter on the gene: `pupil`, `sclera`, `fur_pattern`, `fur_color3`, `glasses`, `outer`, `legwear`, `blush_style`, `ear_fold`, `hood_up`, `handedness` | - | bits exist in every saved gene (all 0) |
| SPEC deep slots (neck, back, mask, gloves, belt, ear/tail/horn/wing accessories, wrist, prop, hair accessory, socks, face paint, tattoos, piercings, makeup, emblem ...) | ~2,000 choices on paper | none painted; the real 304-field registry (P0) is not in the tree |

### 2.3 What the code allows

* **Gene:** 384 bits, 381 used, 3 spare (`fields.txt`). Widths never change once shipped. Every list above can grow to its
  width with **no save change**: the gene already stores the bits. 
* **Part scripts:** `PartRef { u8 list, u8 value, u16 ofs }`, `PartSet.nst` is u8 (≤ 255 stamps per set), raw offsets u16 (a set
  ≤ 64 KB). Ops are 4-8 B, coordinates normalised to the head/torso anchor, stamps 1bpp ≤ 16x16, per-block LOD and view gates.
  GBA reads them raw from ROM; PS1 reads LZ blocks through a 4-slot 8 KB LRU (`part_lru.c`).
* **Creator rows:** `CrRow.field` is a u8 gene field id, so deep fields (LF 64..511) need the SPEC's `CrRow` v2 (u16 `lf`).
* **Unlocks:** `DoomoUnlock.list` u8 + `index` u16; deep parts use SPEC's `CLIST_LF` (`index = lf << 7 | value`).

---

## 3. The 1,611 new parts

### 3.1 Targets per category

"Closest live today" is the nearest thing a player can pick now (e.g. the 3 glasses inside the mixed accessory slot). Methods:
**P** = new painter code in C (big structural things), **S** = hand-authored part scripts (`core/data/parts/<slot>/*.txt`),
**F** = parameterised family (one shape grammar, curated combinations with clearly different silhouettes, 4-8 B of table each),
**T** = tool-generated pixel data (1bpp/2bpp stamps and 8x8 fabric tiles from `tools/stampgen.py`, placed by a short script),
**C** = code-only variants of an existing painter (counts, curls, ring spacing).

| Group | Category | Closest live today | New | in the gene (0 B) | deep (pool) | Method | Stored in |
|---|---|--:|--:|--:|--:|---|---|
| Body | Body types | 10 | **6** | 6 | 0 | P | gene `body` (4 b, 16) |
| Head & face | Head shapes | 12 | **12** | 12 | 0 | S | gene `head_shape` (5 b) |
| Head & face | Eye styles | 24 | **24** | 24 | 0 | S | gene `eye_style` (6 b, -> 48) |
| Head & face | Pupils | 0 | **31** (4 painted) | 31 | 0 | T | gene `pupil` (5 b) |
| Head & face | Sclera | 0 | **15** (2 painted) | 15 | 0 | F | gene `sclera` (4 b) |
| Head & face | Extra eyes (4-6) | 3 | **3** | 3 | 0 | C | gene `eye_count` (3 b) |
| Head & face | Lashes, lids, liner | 0 | **40** (9 painted) | 0 | 40 | F | DF lashes / lid / eye_line |
| Head & face | Brows | 12 | **20** | 20 | 0 | S | gene `brow_style` (5 b, -> 32) |
| Head & face | Noses | 12 | **12** | 12 | 0 | S | gene `nose_style` |
| Head & face | Muzzles & whiskers | 0 | **24** | 0 | 24 | F | DF muzzle / whiskers |
| Head & face | Mouths | 20 | **12** | 12 | 0 | S | gene `mouth_style` (-> 32) |
| Head & face | Teeth & tongues | 4 | **12** | 0 | 12 | F | DF teeth / tongue |
| Head & face | Ears | 16 | **16** | 16 | 0 | S | gene `ears` (-> 32) |
| Head & face | Ear folds | 0 | **7** (7 painted) | 7 | 0 | C | gene `ear_fold` (3 b) |
| Head & face | Blush styles | 1 | **15** (4 painted) | 15 | 0 | T | gene `blush_style` (4 b) |
| Head & face | Freckles, moles, scars, plasters | 4 | **40** (9 painted) | 0 | 40 | T | DF freckle_pat / mole / scar / bandage |
| Head & face | Face paint | 0 | **52** | 0 | 52 | T | DF face_paint |
| Head & face | Face tattoos & gems | 0 | **36** | 0 | 36 | T | DF face_tattoo / face_gem |
| Head & face | Makeup shapes (shadow, lips, highlight) | 0 | **36** | 0 | 36 | F | DF eyeshadow / lip_style / highlight |
| Hair | Hair styles | 32 | **56** | 32 | 24 | S | gene `hair_style` (-> 64) + DF hair_x |
| Hair | Bangs & ahoge | 0 | **24** | 0 | 24 | S | DF bangs / ahoge |
| Hair | Hair accessories | 0 | **52** | 0 | 52 | T | DF hair_acc |
| Hair | Facial hair | 8 | **16** | 8 | 8 | S | gene `facial_hair` (-> 16) + DF beard_x |
| Spooky | Horns | 16 | **16** | 10 | 6 | S | gene `horns` (-> 26) + DF horn_x |
| Spooky | Horn & antenna bits | 0 | **32** | 0 | 32 | T | DF horn_acc / antennae / head_extra |
| Spooky | Tails | 16 | **16** | 16 | 0 | P | gene `tail` (-> 32) |
| Spooky | Tail accessories | 0 | **16** | 0 | 16 | T | DF tail_acc |
| Spooky | Wings | 10 | **14** | 14 | 0 | P | gene `wings` (-> 24) |
| Spooky | Wing accessories | 0 | **8** | 0 | 8 | T | DF wing_acc |
| Spooky | Halos | 6 | **16** (6 painted) | 10 | 6 | T | gene `halo` (4 b, -> 16) + DF halo_x |
| Spooky | Auras | 8 | **16** | 16 | 0 | C | gene `aura` (-> 24) |
| Fur & skin | Fur patterns | 7 | **24** | 24 | 0 | T | gene `fur_pattern` (5 b) |
| Fur & skin | Body tattoos & marks | 23 | **24** | 0 | 24 | T | DF tattoo1 / tattoo2 |
| Outfit | Tops | 24 | **64** | 40 | 24 | F | gene `top` (-> 64) + DF top_x |
| Outfit | Outer layers | 0 | **48** | 31 | 17 | F | gene `outer` (5 b) + DF outer_x |
| Outfit | Bottoms | 12 | **36** | 20 | 16 | F | gene `bottom` (-> 32) + DF bottom_x |
| Outfit | Legwear | 0 | **24** | 15 | 9 | F | gene `legwear` (4 b) + DF legwear_x |
| Outfit | Socks | 0 | **16** | 0 | 16 | F | DF socks |
| Outfit | Shoes | 12 | **36** | 20 | 16 | F | gene `shoes` (-> 32) + DF shoes_x |
| Outfit | Prints & fabric tiles | 8 | **56** | 24 | 32 | T | gene `pattern` (-> 32) + DF fabric |
| Outfit | Pins, badges & emblems | 0 | **72** | 0 | 72 | T | DF emblem |
| Accessories | Hats & headwear | 24 | **128** | 40 | 88 | F+S | gene `hat` (-> 64) + DF hat_x |
| Accessories | Glasses | 3 | **40** (4 painted) | 31 | 9 | F | gene `glasses` (5 b) + DF glasses_x |
| Accessories | Masks | 2 | **36** | 0 | 36 | S | DF mask |
| Accessories | Neckwear | 4 | **52** | 0 | 52 | F | DF neck |
| Accessories | Backpieces | 3 | **52** | 0 | 52 | S | DF back |
| Accessories | Gloves & wristwear | 1 | **36** | 0 | 36 | F | DF gloves / wrist |
| Accessories | Belts & waist | 0 | **24** | 0 | 24 | F | DF belt |
| Accessories | Ear jewellery | 1 | **28** | 0 | 28 | T | DF ear_acc |
| Accessories | Held props | 1 | **104** | 0 | 104 | S+T | DF prop |
| Accessories | Face piercings | 1 | **16** | 0 | 16 | T | DF face_pierce |
| | **Total** | | **1,611** | **524** | **1,087** | | |

By group: Body 6, Head & face 407, Hair 148, Spooky 134, Fur & skin 48, Outfit 352, Accessories 516.
By method: families 591 (incl. 88 of the 128 hats), tool data 518, scripts 336 (incl. 40 hats), props as scripts + stamps 104, new painters 36, code variants 26.
Brand new (excluding the 45 painted-but-hidden): **1,566**. Every gene number fits its width (checked: hat/top/hair 64, brows,
mouths, ears, bottoms, shoes, tails, prints 32, halo 16, facial hair 16, body 16).

**Page-2 fields (`hat_x`, `top_x`, `outer_x`, `bottom_x`, `legwear_x`, `shoes_x`, `glasses_x`, `hair_x`, `beard_x`, `horn_x`,
`halo_x`)** are an addition to SPEC 4.3: 11 deep fields in the free LF range 368..511. Value 0 = "use the gene value"; v > 0 =
catalog value (gene max + v). Every deep field here stays ≤ 127 choices, so the SPEC's 2-byte run entry (`lf << 7 | choice+1`)
holds them unchanged.

### 3.2 Families: how 591 parts stay distinct and cheap

A family is one shape grammar with a few structural axes; we hand-pick combinations that read as different objects at bust
size, never "the same shape in another colour". New VM macro ops (`tools/parts_build.py` + `portrait_script.c`) carry the
grammar so the PS1 does not need per-family C:

| Family | Axes (examples) | Curated | Examples |
|---|---|--:|---|
| Hats | crown (dome, cone, cylinder, flat, slouch, peaked, beret disc) x brim (none, cap bill, small, wide, floppy, cocked) x top (pompom, spike, stalk, ears, candle, propeller) x band (plain, feather, bow, buckle) | 88 | stovepipe with playing cards, slouch with bat ears, gumdrop dome with sprinkles stamp |
| Tops / outer | silhouette (fitted, boxy, cropped, long, robe) x neckline (crew, v, collar, hood, ruff, turtle) x sleeves (none, cap, short, long, puffed, bell) x front (closed, zip, buttons, open, lapels) | 112 | varsity jacket, hanbok-style jeogori, choir robe with ruff, Bonejangles staff polo |
| Bottoms / legwear / socks / shoes | length x cut x hem / pattern band x toe shape x sole | 112 | bloomers, cargo shorts, tabi, platform creepers, striped thigh-highs |
| Neck / belts / gloves | form (band, scarf, collar, chain, tie) x ornament (bell, bow, tag, spikes, gem) | 112 | spiked collar with heart tag, cowbell collar, cravat |
| Glasses / sclera / lashes / makeup | frame shape x bridge x lens / lid line x lash count | 131 | heart frames, pince-nez, swim goggles, winged liner |
| Muzzles / teeth | muzzle length x puff x nose pad / tooth layout | 36 | snout, beak-muzzle, buck teeth, snaggle fang |

Gate for a family member: its **shape hash** (render with every material forced to one ramp, outlines on) must differ from every
other member of the slot by ≥ 24 px at bust scale (≥ 6 px for chibi-visible slots at chibi scale), else it is cut or merged.

### 3.3 Themed sets (the tone)

Every part belongs to one **set** (≈ 120 sets) and carries **theme tags** (32-bit mask). Sets drive "wear the whole set",
randomise-by-theme, unlock rewards and the "new" badges. Built-in content is non-explicit; parody stays affectionate.

| Set | Tone | Sample pieces |
|---|---|---|
| Hellvile Pits Punk | the bad side, cute-goth | studded collar with heart tag, safety-pin ear cuff, flame-trim jacket, devil-tail bow, "HELL YEAH" pin |
| HaHaland Candy Co. | the good side, sugar | gumdrop hat, lollipop prop, sprinkle print, candy-cane horn caps, frosting drip bangs |
| Bonejangles' Party Pals | Walten Files parody (retro 80s mascot restaurant) | staff visor, "Hi! I'm ___" name tag pin, birthday party hat, sewn-button eyes, felt bunny-ear headband, VHS tape prop, faded staff polo, friendly animatronic face plate (mask), "Employee of the Month" tie. Cute-uncanny, never gory |
| Indie Art Jokes | artist in-jokes | "WIP" sticker, placeholder-cube hat, missing-texture print (magenta/black check), tablet pen and "Commissions OPEN" sign props, art-block thought cloud aura, crunchy-JPEG dither print, lasso-tool ahoge, colour-picker eyedropper, unfinished-sketch hat drawn as pencil lines, "ref sheet" clipboard |
| Graveyard Shift | the graveyard map | gravedigger cap, lantern, little shovel, tombstone backpack, ghost plush |
| Bone Cathedral Choir | the cathedral, concert | choir ruff, candle crown, songbook prop, bell earrings |
| Cloud Nine | angelic | laurel, toga top, harp prop, cloud slippers, extra halos |
| Plushie Factory | stitched | button eyes (pupil), seam scars, "DO NOT REMOVE" tag earring, stuffing-tuft bangs |
| Sugar Rush Sewers | rainy-day | rain slicker, rubber boots, rat plush, umbrella |
| Mayor Giggs' Tea Party | show canon, Mad Hatter turkeys | card-band top hats, teacup prop, cravats, pocket watch |
| Hell Breach Gear | the shooter | cute visors, ammo-belt sash (toy darts), knee pads, sticker-bomb helmet |
| Concert & Studio | music modes | headphones, mic prop, glowsticks, guitar backpiece, sequin top |
| Fish & Cook | fishing / cooking | bucket hat with lures, fishing rod, chef toques, oven mitts, apron |
| Fun Fair | minigames | prize ribbon pin, foam finger, cotton candy, whack-a-mole mallet |
| Cozy Home | the house | nightcap, sleep mask, blanket cape, bunny slippers, mug prop |
| Little Ones | babies to children | bib, bonnet, onesie, rattle, crayon, school backpack, floaties |
| Elders | elders | cardigan, reading glasses on a chain, cane, shawl, knitting prop |
| Seasons | spooky / winter / spring / summer | pumpkin pail, earmuffs, flower crown, beach hat |
| Exorcist Kit | exorcism mode | garlic necklace, holy-water squirt bottle, cute stole |
| Royal Court | royal | crowns, sceptre, ermine cape, monocle chain |

### 3.4 Content and age rules (built into the data, enforced in code)

* Each part has an **age class**: `ALL`, `TEEN+` (e.g. heavy face piercings, band-tee slogans), `ADULT` (e.g. fishnets and
  garters, evening gown with open back, wine glass / beer stein / cocktail props). No tobacco, vape or drug props, nothing
  sexual, no gore. "Mature" in this game is tone (Hellvile jokes), not explicit content; `OPT_MATURE` is not needed for any
  built-in part.
* The creator hides `ADULT` parts while the Age row is a minor stage (and `TEEN+` for baby/toddler/child). Changing the Age row
  to a younger stage swaps such parts for the slot's default and says so ("Fishnets aren't for kids - swapped for plain tights").
* The renderer enforces it too: `look_for_stage()` drops any part whose age class is above the stage, so a premade, a copied
  look, an inherited baby look (`look_inherit`) or a hand-edited save can never show one on a minor. The age class sits in the
  part's script header (one bit pair in `PartHdr.group`'s spare bits or a new header byte), so the check costs no extra RAM.
* Generators: `resident_generate*()` never rolls new parts on Classic islands (determinism); Varied/Authored roll them with the
  same age filter. `sim_romance_ok` is untouched.

### 3.5 Review process (every batch)

1. **Contact sheets** (extend `core/tools/portrait_sheet.c` with `--slot NAME [--set NAME]`), each at GBA and PS1 framing:
   * GBA size: bust 48x56 at 1x and the creator's 2x crop, figure 48x80 at 1x, chibi 16x24 at 1x and 3x, fighter back view;
   * PS1 size: figure at 2x, bust at 2x and 3x (the PS1 preview pane 120x195);
   * on 4 reference citizens (catfolk, bald human, hellborn imp, animatronic) and one row per life stage (toddler, child, teen,
     adult, elder);
   * a **shape-only sheet** (flat material) that makes accidental palette swaps obvious;
   * an **interplay sheet**: each hat x {long hair, horns, hood up, glasses, mask, big ears}; each backpiece x {wings, cape, tail};
   * the **icon sheet** (the 20x20 creator icons).
2. **Automated gates** (`make -C core unit`): every live value renders distinct from every other value of its slot (raise
   `MAXOPT` 40 -> 160 in `test_portrait.c`); shape-hash distance ≥ threshold; chibi hash unchanged by a non-chibi part; part
   ≤ 600 px at bust scale (existing build check); per-part cost weight ≤ its slot cap; age class present; name unique; every
   part in exactly one set.
3. **Runner walk-through** `core/tests/scripts/creator_parts_<group>.txt`: opens the grid on every section at both
   resolutions and shoots every page (≈ 90 pages per resolution).
4. **Art review sign-off**: a line per batch in `core/data/parts/review.txt` (batch, sheets reviewed, date, reviewer). A slot's
   live count flips only with a sign-off line (replaces "a premade must wear every value": that rule is kept for the Authored
   roster layer only, so Classic islands stay byte-identical).

---

## 4. Budgets

### 4.1 Save format

* **`DoomoResident` stays 76 B; the gene layout does not change.** The 524 gene parts use values that already fit the shipped
  widths; only the live counts (`*_COUNT`, sanitize, creator rows) grow. 0 extra bits per resident.
* **Deep parts:** 2 B per non-default entry in the citizen's `RUN_DEEP` run (+4 B run header) in the `LOOK` save chunk
  (SPEC 1.5, max 8,072 B, slot 1). A citizen wearing 10 deep parts costs 24 B; the pool holds ~336 such citizens (8 KB) or
  ~170 with a 4 KB GBA pool. Premade citizens' deep parts come from the ROM Authored layer and cost 0 B. The commit-time check,
  Wardrobe meter and Smart pack of SPEC 6.5 apply.
* **Creator prefs:** a small `CRPF` save-ext chunk (≤ 256 B): 32 favourite part ids (u16), 16 recents, 48 per-slot "seen"
  high-water marks (parts are appended in release order, so "new" = id above the mark), the last theme filter. Saved outfits use
  Look Book style runs (ids 1001..1016) in the same pool, so no extra chunk.
* **Unlocks:** gene lists keep their `CLIST_*`; deep parts use `CLIST_LF`. Unlocks are per **set** (≈ 120 ids in bank 2,
  1024..1535), not per part.
* **Slot-1 bound:** measured worst island 60,244 B (LS0 note) + `LOOK` 8,072 + `CRPF` 256 = 68,572 B of the 90,112 B cap;
  re-measure with `save_budget` in CR0 because SONG/FPLY/FISH grew since.
* **Old saves:** a save without `LOOK`/`CRPF` loads with every deep part 0 = exactly today's look; gene values are all below the
  old counts. v1 saves migrate as today. Forward: an older build reading a new save clamps new gene values in
  `resident_sanitize` (it would show the slot's last old value) and skips the unknown chunks; acceptable, document it.

### 4.2 Per platform

| | GBA (measured here) | PS1 (from docs / log) | Dreamcast |
|---|---|---|---|
| Free today | ROM 4.30 of 32 MB (~27.7 MB free); EWRAM **3,556 B**; IWRAM stack 6,356 B; scene scratch 6,296 B spare | ~**9.8 KB** above the 64 KB stack reserve; creator overlay ~55.7 KB of a 118.8 KB slot; SPU RAM ~400 KB unused | 16 MB; renders nothing yet |
| Part scripts / stamps / tiles (~1,611 parts: S ~120 B, F ~8 B + shared templates, T 48-80 B, all LODs) | ~100-150 KB ROM, read raw | ~55-80 KB LZ: **not resident**. Uploaded to SPU RAM at boot (like the SFX) and DMA'd into a per-render part arena on a miss; fallback `PARTS.BIN` read per scene | linked |
| Macro ops + family templates + age filter in the VM | ~12 KB ROM | **+12 KB resident** | linked |
| Part arena / LRU | 0 | replace the 4-slot LRU with 2 slots + a 4 KB per-render arena (each worn entry decoded once per render): **+0 to +4 KB** | - |
| Names, tags, sets, age, cost weights (~1,611 x ~20 B) | ~32 KB ROM | creator overlay (only `catalog.c` and the creator read part names); if the group passes the slot, names go to `PNAMES.BIN` per slot | linked |
| Icons 20x20 4bpp | ~320 KB ROM (0 render while browsing) | `ICONS.BIN` on disc, one slot's page read on section entry and uploaded to the VRAM image cache | linked |
| Creator UI (grid, filters, search, outfits, delta undo) | ~30 KB ROM; `CrState` grows ~5.0 KB inside the 12 KB scratch (fits) | creator overlay +30 KB (55.7 -> ~86 KB + names ~32 KB = ~118 KB: at the slot edge, so names on disc) | - |
| Translations (es first, 7 more later) | ~25 KB ROM per language | disc (`lang` packs) | - |
| `LOOK` pool | **+8,200 B EWRAM: does not fit** (CR5a) | +8,200 B resident | - |
| `CRPF` prefs | +256 B EWRAM | +256 B | - |
| **Verdict** | **ROM: trivial (~0.6 MB of 27.7 MB free). EWRAM: the 524 gene parts fit with 0 B; the deep parts need ~8.5 KB that only a reclaim provides** | **Tight: needs ~+20 KB resident (VM ops, pool, prefs, arena) against ~9.8 KB free. Needs SPEC's P3 reclaims first (bios to disc -23 KB, edges -9 KB) and part data in SPU RAM** | **no constraint** |

**GBA EWRAM reclaim options (CR5a), pick one or two:**
1. The island hub's recorded rect lists (`lr_land_r`, `lr_patch_r`, `lr_road_r`, `dv_pool`, `dv_tmp`: 12,512 B) live only while
   the 2D hub runs; place them in the arena's 3D/overworld cache region the hub does not use (re-recorded on hub entry). +12.5 KB.
2. A 4,096 B GBA `LOOK` pool (`LOOK_POOL_BYTES_GBA`) with the Wardrobe meter; ~170 fully kitted citizens.
3. Fold the creator's 1,024 B `nicks` into the pool (SPEC already plans it): +1 KB.

### 4.3 Draw cost (GBA)

* Today (PORTRAIT.md): bust 763k avg / 1.20M worst cycles, figure 915k / 1.38M, chibi 250k / 338k, fighter 454k / 655k; a 30 Hz
  tick is 561,792 cycles. SPEC gates: bust ≤ 1.45M maxed, figure ≤ 1.65M, chibi ≤ 360k, < 14,000 pixel visits, ≤ 240 part ids.
* **A cost weight per part**, measured by the build tool (pixel visits at bust, figure and chibi LOD on the bench citizen) and
  stored with the meta. The creator sums the weights of the draft and shows a "Busy" meter; past the cap it refuses with a
  friendly line ("Too much bling to draw! Take something off."). This turns the maxed gate into a guarantee instead of a hope:
  no citizen anyone can make exceeds 1.45M on a bust.
* **Chibi LOD:** only silhouette slots draw on chibis (hat, hair, ears, horns, wings, tail, back, outer, big prop as 1-3 px);
  everything else has no `tiny` block, so it costs 0 cycles in crowds and does not touch the chibi hash (no re-render of 40
  chibis when a face paint changes).
* **Occlusion first:** hood, mask, coat and gloves skip what they cover (SPEC 4.4); the hat family clips hair tops.
* **Portrait cache:** image sizes do not change, the 18 KB pool and its keys stay; the key gains the deep run bytes as in
  `look_key()`. More variety means the same number of misses (one look = one image).
* **Creator on GBA:** today 13.4 fps / 54.5 ms, bound by `unlock_find` per field (GBA.md). With 1,600 items this gets worse unless
  locks become bitsets (CR9). Icons are prerendered in ROM, so scrolling a grid draws 18 small blits (~30k cycles) and renders no
  portrait; the figure re-renders only after the cursor rests 3 ticks (one figure ≈ 2 ticks today).

### 4.4 PS1 RAM headroom: the path

1. SPEC P3 reclaims (bios -> `ROSTXT.BIN` ≈ -23 KB, edges -> `EDGES.BIN` ≈ -9 KB) land first; together with today's ~9.8 KB that
   gives ~42 KB.
2. Spend: VM macro ops +12 KB, pool +8.2 KB, prefs 0.25 KB, arena 0-4 KB = ~21-25 KB. Leaves ~17-21 KB.
3. Part data never resident: SPU RAM store (DMA ~2 KB per miss, guarded against the audio driver's own uploads), or `PARTS.BIN`.
4. Everything creator-only (names, tags, icons, grid code) in the creator overlay or on disc.
5. Gate: `elf2exe` ≥ 64 KB below the stack top on every merge; creator group ≤ the slot (`ovl_plan --report`).

---

## 5. The creator screen

### 5.1 What the screenshots show (240x160 and 320x240)

1. **The picker is a text list**: 7 rows on the GBA, 13 on the PS1, no picture of the item. With 152 hats that is 22 GBA pages of
   names; nobody will find "Slouch Bat" that way. (`07_eye_picker`)
2. **The figure is tiny on the Outfit tab on the GBA** (1x, the hat is ~10 px tall); the camera zooms only on Face and Hair.
   Hats, glasses, masks, earrings and shoes need a zoom. (`15_outfit_browse`, `05_hell_outfit`)
3. **A locked choice hides the whole citizen** behind a lavender "???" blob; you cannot see whether the locked hat suits them.
   (`10_locked_peek`, `15_outfit_browse`)
4. **Hints truncate** in the detail line ("See 8 different dr..", "Defeat 40 demons b.."), and the toast covers the tab strip.
5. **One mixed Accessory slot** (glasses, masks, scarves, backpacks): you cannot wear glasses and a scarf.
6. **Rows per tab are long and flat**: Outfit scrolls through 13 rows; Head & Face 25; no sections to jump to.
7. **No favourites, recents, search, filters, saved outfits or set grouping**; randomise is all / this tab only.
8. **Speed**: 13.4 fps on the GBA (unlock checks per row per frame), before any new content.
9. Good and worth keeping: the stage, bounce and sparkles; marquee names; the START menu that lists only what applies; undo/redo;
   copy a look; preset preview on LEFT/RIGHT; the Hellvile cute-goth skin.

### 5.2 Upgrades

**Structure**
* Tabs become **Body, Face, Eyes, Hair, Spooky, Outfit, Accessories, Voice, Heart, Home** (SPEC's 10 tabs, with Accessories split
  out of Outfit) and each tab gets **section pills** under the strip ("Hats | Glasses | Masks | Neck | Back | Hands | Props |
  Jewellery"). L/R = section (crossing into the next tab), SELECT+L/R = tab (SPEC 6.3).
* **Simple / Advanced / Everything** modes (SPEC CR01) hide the deep detail rows from new players.

**Browsing 1,600 items: the grid picker** (A on a part row)
* GBA: a 6 x 3 grid of 22 px cells beside the preview; PS1: 8 x 4 grid of 24 px cells, a two-line help footer.
* Each cell is a **prerendered icon** (the part alone on a neutral mannequin, made by the build from the real renderer); badges:
  **NEW** dot, **star** (favourite), **padlock**, **set colour stripe**. There is no "18+" badge: adult-only parts are simply
  not listed while the citizen is a minor, and look like any other part for adults.
* **Header chips**: All, Favourites, Recent, New, Owned, and a **Theme** chip (LEFT/RIGHT cycles Hellvile, HaHaland, Party Pals,
  Indie Art, Graveyard, Cozy, Royal, Punk, Spooky, Sporty, Little Ones ...). Shape chips per slot (hats: Caps, Tall, Wide, Ears,
  Crowns, Hoods).
* **Set headers** inside the grid: "Bonejangles' Party Pals 3/8 - A: wear the set".
* **Search**: START in the grid opens the on-screen keyboard; 1-3 letters filter the slot by name or set (substring, ROM names,
  ~0.2M cycles per keystroke). Letter jump: hold SELECT + LEFT/RIGHT to step A-Z.
* **Paging**: d-pad moves cells, the view scrolls by rows; L/R = page; held direction accelerates (1, 1, 2, 4 cells per step after
  8/16/24 ticks); the cursor position is remembered per slot for the session.
* **Live try-on**: moving the cursor puts the item on the draft (as LEFT/RIGHT preview does today); B restores, A keeps. The
  figure re-renders after the cursor rests 3 ticks (GBA) / 1 tick (PS1); meanwhile the icon is shown big in the preview corner.
* **Slot-aware camera**: head slots zoom to the bust (2x GBA, 3x PS1), feet slots to the legs, back/tail/wing slots turn the
  figure to the back view, props show the hand side.
* **Locked items** are drawn on the citizen in lavender (silhouette of the item only), with the hint in the footer (two lines on
  PS1, marquee on GBA). The rest of the citizen stays visible.
* **Conflicts are shown, not silent**: "Hidden by: Hood" chip when a hat hides under a hood, "Glasses go over the mask" etc.
  (a `hides` mask per part in the meta).

**Making looks faster**
* **Favourites** (Y / triangle stars; 32 kept), **Recently used** (16), **"new" badges** that clear when seen.
* **Randomise by theme**: the START menu's Randomise entry gets a scope (section, tab, outfit only, all) and a theme; pins
  (hold A 0.5 s on a row) keep a slot fixed; locks and age rules always apply.
* **Wear the whole set** from a set header; **outfit presets** grow from 20 to 60 (SPEC O62), many drawn from the sets.
* **Saved outfits**: "Save outfit / Load outfit" in the START menu: 16 Look Book styles shared by the island, each with a name and
  a mini preview; "Load" applies only outfit slots.
* **Compare**: hold SELECT ≥ 10 ticks shows the original look; tap SELECT stays the turntable.
* **Undo**: SPEC's 4 KB delta ring (`{u16 lf, u8 old, u8 new}`), "Undo: N steps" in the footer; randomising a set costs one step.
* **Copy a look** keeps working and copies deep parts too; "copy outfit only" and "copy face only" scopes.

**Clarity**
* Footer help line per item: `Slouch Bat Hat  ·  Hellvile Pits Punk 2/6  ·  NEW`, and for a locked one the full hint
  (two lines on PS1, marquee on GBA). Button hints change with context (grid: "A wear  B back  Y star  L/R page  START search").
* Section captions and counts ("Hats 41/152"), a "Busy" meter (draw cost) and a "Wardrobe" meter (pool use) in the footer.
* Toasts move below the tab strip.

**Speed (GBA ≥ 20 fps scrolling, PS1 ≥ 28)**
* Lock bitset for the open slot (1 bit per part, built on section entry and when `unlock_epoch` changes) replaces per-frame
  `unlock_find`. Age/visibility filter computed once into an index list (≤ 152 u16 = 304 B).
* Dirty regions: only the grid cells and footer redraw on a move; preview redraws only when its image uid changes.
* Icons blitted from ROM (GBA) / VRAM (PS1); no portrait render while scrolling; figure debounced.
* `creator_scroll.txt` harness script + `tools/fps_gate.py` (SPEC P4) in CI for both consoles.

### 5.3 Screen sketch (GBA 240x160)

```
y  0 [Bd][Fa][Ey][Ha][Sp][Ou][Ac][Vo][He][Ho]           tab strip
y 12 < Hats | Glasses | Masks | Neck | Back | Props >   section pills
y 23 +- preview 86x112 -+ [All][*][New][Recent] Theme: Punk >
     | bust 2x on hats  | [ic][ic][ic][ic][ic][ic]
     | legs 2x on shoes | [ic][ic][ic][ic][ic][ic]     6 x 3 icons, 22 px
     | back view for    | [ic][ic][ic][ic][ic][ic]
     | back pieces      | Hats 41/152          Busy ##-- 
y136 Slouch Bat Hat · Hellvile Pits Punk 2/6 · NEW       help line (marquee)
y148 A wear  B back  Y star  L/R page  START search      button hints
```

---

## 6. Work packages

Sizes: S ≤ 2 days, M ≤ 1 week, L ≤ 2 weeks, XL > 2 weeks (split into batches). "SPEC" = docs/design/customization/SPEC.md.

| ID | Size | What | Main files | Depends on |
|---|---|---|---|---|
| **CR0** | S | Baselines and budget tool: GBA (done here: ROM 4,299,852, EWRAM 3,556 free, IWRAM stack 6,356, `CrState` 5,992), PS1 from the CI map, slot-1 bound, creator fps on both consoles; `tools/part_budget.py` that prints per-slot ROM / PS1 / pool cost from the catalog | `tools/part_budget.py`, `docs/CREATOR.md` (costs) | - |
| **CR1** | L | **Part catalog**: `core/data/parts/catalog/<slot>.txt` (one line per part: id, name, set, tags, age class, unlock set, method, hides), `tools/partcat_build.py` -> `core/gen/part_meta.c` (names, set table, tag masks, age, cost weight, sorted name index); `part_meta.h` API; `check-gen` | new: `core/include/doomo/part_meta.h`, `tools/partcat_build.py`, `core/data/parts/catalog/`; `core/Makefile` gen | CR0 |
| **CR2** | L | **Renderer for many parts**: per-slot part sets (`parts_<slot>.c`, u16 list id), VM macro ops (BRIM, CROWN, BAND, STRAP, BOW, RING, CHAIN, FRILL, POUCH) and family templates, age filter in `look_for_stage()`, chibi LOD rules, cost-weight measurement in the build, shape-hash helper; PS1: part arena (decode each worn entry once per render) and `DOOMO_PARTS_SPU` source | `tools/parts_build.py`, `core/src/portrait_script.c`, `portrait_age.c`, `portrait_int.h`, `parts.h`, `part_lru.c` | CR1, P1.M1 (landed) |
| **CR3** | M | **Flip the gene lists live** (no save change): the 12 pending M2 appends + P2 appends + the 11 core gene fields (pupil, sclera, glasses, outer, legwear, blush style, fur pattern, ear fold, hood up ...) get creator rows, sanitize counts, randomiser (Varied only), presets; re-home the 45 shim painters onto the gene fields | `catalog.h/.c`, `resident.c`, `creator_data.c`, `portrait_head.c`, `portrait_hair.c`, `portrait_body.c`, `test_portrait.c` | CR2; art batches A1-A3 for the values |
| **CR4** | XL | **Art batches** (each its own patch, sheets + sign-off): A1 face & eyes 283; A2 hair 148; A3 spooky 134; A4 fur & body 54; A5 outfit 224; A6 hats & glasses 168; A7 masks, neck, back, gloves, belts, jewellery, piercings 244; A8 props 104; A9 face paint, tattoos, makeup, prints, pins 252 (tool data). Sum 1,611 | `core/data/parts/<slot>/*.txt`, `tools/stampgen.py`, `core/data/parts/stamps/`, `core/data/parts/review.txt` | CR1, CR2; A5-A9 deep slots need CR5 to be saved |
| **CR5** | L | **Deep parts saved**: the P0.M1 slice this needs (real `LOOK` pool, `LOOK` chunk, resolve own > style > ROM > derived, `look_key`), the 11 page-2 fields in LF 368..511, `CrRow` v2 (u16 lf), commit check + Wardrobe meter | `look_pool.c`, `look_fields.h` (P0's), `save.c`, `creator.c`, `creator_int.h` | P0.M1 (or do that slice here), CR5a |
| **CR5a** | M | **GBA EWRAM room**: island hub record lists into the arena region the hub does not use (+12.5 KB), creator nicks into the pool (+1 KB); or a 4 KB GBA pool | `core/src/fp_island_gfx.c`, `gba/src/bigbuf.c`, `gba/include/gba_plat.h` | CR0 |
| **CR6** | XL | **Creator browser**: section pills, grid picker, filters and theme chip, search, favourites, recents, new badges, set headers + wear set, slot-aware camera, locked-item overlay, conflict chips, help lines, Simple/Advanced/Everything modes | `creator.c`, `creator_draw.c`, `creator_data.c`, `creator_int.h`, new `creator_grid.c`, `ps1/tools/ovl_plan.py` (regex `creator(_\w+)?$`) | CR1; CR7 for icons |
| **CR7** | M | **Icons**: host tool renders every part on a mannequin into 20x20 4bpp icons -> `core/gen/part_icons.c` (GBA/host) and `ICONS.BIN` (PS1 disc, per-slot pages uploaded to the VRAM image cache), icon contact sheet | new `core/tools/part_icons.c`, `ps1/CMakeLists.txt`, `ps1/iso.xml` | CR2 |
| **CR8** | M | **Outfits and presets**: Save / Load outfit (Look Book styles 1001..1016), 60 outfit presets, randomise by theme and scope with pins, copy scopes, compare (hold SELECT), delta undo ring | `creator.c`, `creator_presets.c`, `creator_themes.c` | CR5, CR6 |
| **CR9** | M | **Speed**: lock and visibility bitsets, dirty regions, debounced figure, cost meter ("Busy"), `creator_scroll.txt` on GBA and PS1 harnesses + `tools/fps_gate.py` (≥ 20 / ≥ 28 fps) | `creator*.c`, `gba/harness/scripts/`, `ps1/harness/scripts/`, `tools/fps_gate.py`, `gba/Makefile` | CR6 |
| **CR10** | L | **PS1 memory**: SPU-RAM part store (upload at boot after the SFX, DMA on miss, audio-driver lock) or `PARTS.BIN`; names/meta in the creator overlay or `PNAMES.BIN`; take the P3 reclaims (bios, edges) if not landed | `ps1/src/` (spu/part loader), `core/src/part_lru.c`, `ps1/CMakeLists.txt`, `ps1/tools/ovl_plan.py` | CR2, SPEC P3.M1 |
| **CR11** | S | **Unlocks and sets**: set-level unlocks in bank 2 (≈ 120), hints, rewards from minigames / story / Fun Fair, "new" after unlock, almanac "Wardrobe" page | `unlocks_*.c`, `unlock_ids.h`, `lib_*.c` | CR1 |
| **CR12** | S | **Age gate for parts**: age classes in the catalog, creator hiding and swap-on-age-change, render filter, `look_inherit` filter, generator filter | `portrait_age.c`, `creator.c`, `resident.c`, `family.c` | CR1, CR2 |
| **CR13** | S | **Prefs chunk** `CRPF` (favourites, recents, seen marks, last filter), save-ext registration, old-save load test | `creator.c`, `save_ext.h` users, `save.c` | CR6 |
| **CR14** | S | **Docs and text**: `docs/CREATOR.md`, `docs/PORTRAIT.md`, part names through the lang packs (es first), ROADMAP lines | docs, `core/lang/` | each package |

### 6.1 Test plan per package

| ID | Unit tests (`make -C core unit`) | Runner / harness | Sheets |
|---|---|---|---|
| CR1 | names unique, every part has set + age + tags, counts ≤ gene width / 127 per deep field, ids append-only against a frozen list (`part_ids_golden.txt`) | - | - |
| CR2 | Classic golden byte-identical; every part distinct (raised `MAXOPT`); shape-hash distance; chibi hash unchanged by non-chibi parts; age filter (a minor never renders an ADULT part, 4,000 random looks); LZ/SPU arena == raw (host `DOOMO_PARTS_LZ_TEST`); maxed look ≤ pixel-visit bound | `portrait_gallery.txt` | `opt_<slot>`, shape-only |
| CR3 | `creator_rows_cover_every_field` (now with the new rows), sanitize keeps old saves, Classic premade fingerprints equal, randomiser never rolls new values on Classic | `creator.txt`, `creator_hellvile.txt` (both sizes) | per list |
| CR4 | per batch: distinct, shape-hash, cost cap, age class | `creator_parts_<group>.txt` | full set per batch, interplay, ages, icons |
| CR5 | pool put/find/drop, `LOOK` round trip, v1/v2 save without `LOOK` loads identical, refused commit leaves the town unchanged, cache hit check with runs | `test-coresave` (GBA), memcard (PS1) | - |
| CR5a | island hub frames identical before/after (`test_island_replay`, `vhash-seq`) | `make -C gba profile-all` (no fps loss) | - |
| CR6 | grid index math, filters (theme, new, fav, recent), search, locked overlay, wear-set, camera per slot, every part reachable in ≤ N presses (`expect-presses`) | `creator_grid.txt`, a journey with a budget row | screens at both sizes |
| CR7 | icons deterministic (`check-gen`), one icon per live part | - | icon sheet |
| CR8 | save/load outfit round trip, randomise scope touches only its slots, pins hold, delta undo replays exactly | `creator_outfits.txt` | - |
| CR9 | lock bitset == `cr_value_locked()` for every part | `creator_scroll.txt` ≥ 20 fps GBA, ≥ 28 PS1 | - |
| CR10 | - | PS1 harness `impostor`/`creator` scripts, `elf2exe` ≥ 64 KB, overlay group ≤ slot | - |
| CR11-13 | set unlock grants, hint text, `CRPF` round trip and absent chunk | `creator.txt` | - |

### 6.2 Per-platform notes

* **GBA:** everything data goes to ROM (plenty); keep new hot loops out of IWRAM (+0 B gate). Icons in ROM avoid any render while
  scrolling. Only the `LOOK` pool and prefs need EWRAM (CR5a first). Measure every art batch with the harness bench (bust,
  figure, chibi over kitchen-sink looks), not on the host.
* **PS1:** resident RAM is the limit: no part data, names or icons resident; VM ops only. Creator group must stay ≤ the slot
  (names to disc if needed). SPU-RAM store needs a lock with the audio driver's uploads (both use SPU DMA). Head impostor (P3)
  shows head parts on the 3D model; other new parts reach the PS1 3D model only through SPEC's body recipes (outer, back quad,
  prop), the rest are visible in 2D views.
* **Dreamcast:** draws nothing yet; compile the data, run the unit suite. No budget concern.
* **Native:** host builds keep `DOOMO_HITCHECK` and `DOOMO_PARTS_LZ_TEST`, so cache and LZ paths are tested on every run.

### 6.3 Order

1. **Slice 1 (no save change, ~524 parts):** CR0 -> CR1 -> CR2 -> CR7 -> CR6 (grid, filters, favourites in session only) -> CR3
   with art batches A1-A4 and the gene part of A5/A6 -> CR9. Ship.
2. **Slice 2 (deep parts, ~1,087):** CR5a -> CR5 (with P0.M1) -> CR10 (PS1) -> A5-A9 deep batches -> CR13 -> CR8 -> CR11 -> CR12
   (age gate lands with the first ADULT-tagged part, so in practice with A5) -> CR14.
3. The age gate (CR12) must be in before any `TEEN+` / `ADULT` part goes live.

---

## 7. Risks and owner questions

**Risks**
* **GBA EWRAM**: 3,556 B free is less than the docs' last figure (20.5 KB in STUDIO.md); the island hub's I-series lists took it.
  Without CR5a, slice 2 cannot save on the GBA.
* **PS1 resident RAM**: depends on SPEC P3 reclaims that are not in the tree; SPU-RAM storage is new code on real hardware
  (DMA timing, audio contention) and must be tested on a console or a cycle-accurate emulator.
* **Art throughput**: 1,611 parts is the bulk of the work. Families and tool data are ~70 % of it; the review gate (sheets +
  sign-off) is what keeps them from becoming samey.
* **Distinctness at 48x56**: a bust hat has ~30x14 px; some ideas will fail the shape gate and must be cut, not recoloured.
* **Gene-width ceiling**: lists that reach 64/32 are full for good; later growth goes to page-2 deep fields (pool cost).
* **SPEC overlap**: CR6/CR8/CR9 implement parts of SPEC P4 (creator v2); CR5 is a slice of P0.M1. Coordinate ownership so the
  same files are not edited by two packages at once.

**Questions for the owner**
1. OK to count the 45 already-painted hidden choices toward the 1,500 (total 1,611; 1,566 without them)?
2. Party Pals (the Walten Files parody set): how close may it get (name tags, VHS, mascot face plates) before it reads as a copy?
3. Adult-only props: is "wine glass / beer stein / cocktail" fine for adults, or keep all props all-ages?
4. GBA: reclaim the island lists (keeps the 8 KB pool and every feature) or accept a 4 KB pool (fewer kitted citizens)?
5. Unlocks: most parts open from the start with ~20 % of sets as rewards, or more gated?

> OWNER CORRECTION 2026-11-08: the creator speed target is 30 fps on GBA and PS1 (not 20 / 28). See DECISIONS.md "FPS TARGET".
