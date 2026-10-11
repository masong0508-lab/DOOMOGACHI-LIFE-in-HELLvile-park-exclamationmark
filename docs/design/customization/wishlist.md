# DOOMOGACHI LIFE in HELLvile Park: customisation and options wishlist

> The user asked: *"make sure you add a shitton of options, and face/body customization"*.
> Earlier: *"immense customization"*, *"extremely complex customization options"*, *"an immense amount of options for everything and anything"*.
>
> This is a wishlist and design spec, not code. It was written read-only against the repo state on 2026-10-11 (`core/include/doomo/catalog.h`, `resident.h`, `town.h`, `core/src/options.c`, `docs/CREATOR.md`, `docs/GAME_DESIGN.md`, `docs/PORTRAIT.md`, `docs/PS1.md` section 7, `palette.h`, `unlocks.h`, `unlock_ids.h`, `save.h`, `save_ext.h`, `sim.h`, `input.h`, `portrait_int.h`).
> Inspiration: Tomodachi Life, Miitopia, The Sims, Animal Crossing, Saints Row, Black Desert, and furry OC makers. Everything is adapted to plush critter-folk and adorable demons, and kept wholesome and toy-like (ARCHITECTURE.md "Art direction").

---

## 0. How to read this

### Columns in the customisation tables

| Column | Meaning |
|---|---|
| **ID** | `F` face, `H` hair, `E` ears, `B` body, `O` outfit, `A` animation and voice, `T` creator tool |
| **Values** | the choices (count in brackets). `auto` is always value **0** and means "what the renderer does today", so a zeroed extension looks exactly like the current game |
| **Bits** | new per-resident bits. `0` = append-only: new values in an existing byte field, so no RAM cost. **C6** = a 6-bit colour: 0..31 paints, 32..39 island dyes (E7), 63 = auto/none |
| **Tier** | `A` append-only (0 RAM) / `C` core LookX, inline in every resident (E1/E2) / `D` deluxe, stored as sparse override triples only for citizens who change it (E3) / `G` global, not stored per resident |
| **Render** | where it shows up. `P` bust portrait 48x56 (dialogs, lists, HUD face, creator close-up), `F` creator full figure 48x80 (the creator preview), `C` overworld chibi 16x24 and apartment windows, `T` fighter 32x48 (third-person sprite), `M` PS1 low-poly 3D model (`model.c`) |
| **Diff** | `S` data plus a small renderer or logic change (about a day). `M` a new renderer part or layer, or a sim hook (days). `L` cross-module work or a new system (a week or more) |
| **Status** | `NEW`, or `EXTENDS(field)`: an existing `DoomoLook`/`DoomoResident` field, flag, enum or option that this grows, splits or refines |

### Columns in the options tables

`ID`, option, values (`*` = default), `n` = number of values (all options are one byte, and most fit in 2-3 bits when packed), **Scope** `G` = global/system setting or `I` = per-island rule (E4), Status, Diff.

---

## 1. What exists today (the baseline this extends)

| Area | Today |
|---|---|
| `DoomoLook` | **48 bytes, 48 one-byte fields** (resident is 96 B, x1001 records, about 96 KB of the ~110 KB `DoomoTown`) |
| Catalog lists | 24 species, 10 bodies, 12 heads, 24 eye styles, 12 brows, 12 noses, 20 mouths, 16 ears, 8 cheek flags, 32 hair, 8 facial hair, 16 horns, 16 tails, 10 wings, 6 halos, 23 markings (face paint **and** fur patterns share one slot), 24 tops, 12 bottoms, 12 shoes, 24 hats, 20 accessories (glasses, masks, necklaces, capes and backpacks all compete for **one** slot), 8 auras, 8 patterns (**top only**) |
| Sliders | height, build, head size, eye size/spacing/height/tilt, eye count 1-3, brow height, nose size, mouth size, fangs 0-3, horn size |
| Colours | 32 paints x 3 shades + 8 natural skin tones. Colour fields: eye, hair, hair highlight, horn, marking (doubles as fur colour 2), top, bottom, shoes, hat, accessory, pattern. **No** separate colour for brows, nose, lips, sclera, wings, halo, inner ears, paw pads, tail tip, blush or belly (all derived) |
| Renderer materials | `PCtx`: skin, skin2, hair, hairhi, eye, horn, mark, top, bot, shoe, hat, acc, patm (13) |
| Expressions | 11 (`EXPR_*`) |
| Motion | personality "movement" slider only. Species-driven blink, breathe, ear twitch, tail wag. Everyone shares one walk cycle (4 frames) |
| Voice | pitch, speed, quality, intonation, accent (0..31) + 12 voice presets |
| Creator | 9 tabs, 72 species presets, 20 outfit sets, undo/redo 20, randomise all/tab, copy a look, locks with silhouettes, nicknames (NICK chunk, 64 entries) |
| Options | **57 of max 64** (`OPT_COUNT <= 64`, `DOOMO_OPT_BYTES 64`), 7 pages, all stored **per island** in `doomo_town.opt[]` |
| Unlocks | 1024 bits, nearly all allocated by team range. Creator range 256..319 has about 43 ids free, foundation 202..239 spare, action 564..607 spare. `PROG_*` slots 31/32 used |
| Palette | **full**: 1..15 UI, 16..111 paints, 112..135 skins, 136..255 environment ramps |
| Save | slot 0 40 KB, slot 1 88 KB. Extension chunk at most 8 KB each (`DOOMO_SAVE_EXT_MAX`). Residents stored as deltas against roster ROM records |
| Memory | GBA EWRAM about 33 KB **over** budget and PS1 over 2 MB right now. Every per-resident byte costs about 1 KB |

---

## 2. Enablers: how to fit "a shitton" into a GBA

The wishlist adds **310 customisation items** (281 new per-resident fields, plus 29 catalog lists that grow) and **469 options**. Done naively (one byte per field for 1000 residents), that is roughly 280 KB on a machine that is already 33 KB over. These enablers make it fit. They are ordered by leverage.

| # | Enabler | What | Why | Diff |
|---|---|---|---|---|
| **E1** | **Pack `DoomoLook` + the resident tail** | The 48 look fields need **210 bits** today (243 bits with growth headroom: species 6, eye style 6, colours 6 each, and so on), which is **31 B instead of 48**. The non-stat resident tail (voice 5x5 bits, persona 5x4 bits, pronoun, romance, birthday, likes, fear, catchphrase, mood, activity, death) packs from 26 B to 16 B. Keep `stat[8]`, `mood` and `activity` as bytes, because the sim mutates them every tick. Unpack into a wide working `DoomoLook` when copied: the renderer already copies and sanitizes the look, and the creator edits a draft | Frees **27 B per resident**. Either spend 24 of them on Core LookX (E2: 93 B, **no extra RAM**) or shrink the record to 81 B and **save about 15 KB of EWRAM** while still adding a 12 B core (section 9.2) | L |
| **E2** | **Core LookX (192 bits = 24 B, inline)** | The highest-value new axes for **every** citizen (the `C` tier below): pupils, sclera, iris, heterochromia, lashes, lids, shine, brow and nose colour, muzzle, blush, teeth, tongue; fur pattern and third colour, material, fluff, hands, feet, legs, ear size/fold/inner colour, tail count/length/tip, wing colour, 4 proportions; outer layer, legwear, glasses, hat tilt, hood, fit; idle, walk, run, handedness, age look (the layout is in section 9) | Babies, random islands and the roster all use it, and every value 0 is "auto" | M |
| **E3** | **Deluxe overrides (sparse triples)** | Everything in tier `D` (233 fields, 930 bits if stored densely, about 117 B per citizen). Do **not** store it as one dense record. Store a sorted pool of `{u16 resident, u8 field, u8 value}` triples (4 B each), holding only the deluxe fields a citizen changed away from `auto`. Fields wider than 8 bits (O42, O44, O60, F82) are split into per-piece sub-fields. Pool size: GBA 1024 triples (4 KB, about 50 lavishly customised citizens or 200 lightly customised ones), PS1 8192 triples (32 KB). Saved as chunk `LOOK` (4 B x 2048 per 8 KB chunk). When the pool is full, the creator shows "Deluxe wardrobe full" and offers to bake the look down to Core | Most citizens touch only a few deluxe fields, so triples are 3-10x denser than fixed records. Premade citizens can author deluxe values in **ROM** (roster text, `roster_build.py`), so the 1000 premades get rich looks with zero GBA RAM. Precedent: the creator's nickname table (NICK, 64 entries) | M |
| **E4** | **Options v2** | (a) Split **global** settings (audio, display, accessibility, text, controls, creator prefs) from **island rules** (sim, romance, gore, possession, cheats). Global settings go in a small separate save area (GBA flash sector header, or a PS1 memory-card block header). Changing the volume should not need a different save slot. (b) Lift `OPT_COUNT <= 64`: a new `OPTX` save-ext chunk (one byte per option, up to 512 B, or about 200 B nibble-packed). (c) Keep `DoomoOptionInfo` data-driven and extend it with `scope`, `step` and `unlock` fields. (d) The options screen grows sub-pages and a "Reset this page" action | `opt_get/opt_set` stay the API, so the existing 57 keep their ids | M |
| **E5** | **Unlock bank 2** | A second 512-bit bank in a save-ext chunk `UNL2`, plus an extra counters chunk (`PROG` slots are 31/32 full) for counters like makeovers, photos and sprint distance. Or use `UL_ALIAS_MAX` alternate routes for parts that already have an entry | About 80 new cosmetic unlocks (section 10) do not fit the 43 free creator ids | S |
| **E6** | **PS1: portrait as the face texture** | Render the 48x56 bust (or its 32x32 face crop) into a VRAM texture page and map it on `model.c`'s head sphere front. Then **every face option shows on the 3D model for free**, instead of `model.c` re-implementing eyes, blush and mouth | Turns dozens of `M` render gaps into zero work | M |
| **E7** | **Island dyes + palette tricks** | The palette is full. PS1: per-image CLUTs make 8 user-mixed "island dyes" (BGR555 sliders, 16 B per island) easy. GBA: put them in the OBJ palette bank, or borrow one environment ramp while the creator, portraits or apartments are on screen. **Palette cycling** gives rainbow hair, glitter, glowing markings, light-up shoes and pulsing auras for almost no CPU | Unlocks "any colour" and all the animated finishes | L |
| **E8** | **Renderer budget rules** | New parts must fit the per-phase slice budget (the GBA uses about 0.23M cycles per layer group per tick). Each new material costs one `PMat` in `PCtx` (stack), and no palette. Fur patterns and fabric finishes reuse the existing pattern fills and masks in `portrait_gfx.c`. Anything not visible at chibi scale is skipped there (`C` column) | Keeps the UI frame rate | - |
| **E9** | **"0 = auto" rule** | Every new field's value 0 means the current behaviour (species- or style-derived). Old saves, the roster, `resident_generate` and the parity fingerprint tests stay valid. A zeroed LookX record is not stored at all | Backward compatibility and sparse storage | S |
| **E10** | **PS1 overlays** | On the PS1, const data is RAM. Option names (about 400 x 20 B), catalog names and the creator and portrait code should live in an overlay that is loaded only in the creator or options scenes (ties into task "PS1 game disc fits 2 MB (overlays)") | Keeps the PS1 within 2 MB | M |

**Fixed rules (not options, not customisable):**
* Kid and teen romance gating stays hard-wired (`ROSTER_T_KID` / `ROSTER_T_TEEN`). If custom citizens get an **Age look** (B86), it must drive the **same** gate.
* Citizens never bleed in any gore mode (demons only). The body sliders are stylised plush ranges with nothing sexualised. Clothing stays toy-like.

---

## 3. (a) Face customisation

Today: one `eye_style` byte mixes eye shape **and** pupil (`EYE_CAT`, `EYE_HEART`, `EYE_DEMON_SLIT`...). One `marking` mixes makeup **and** fur pattern. Eight cheek on/off flags. The wishlist separates every layer so they combine freely. A legacy value always keeps rendering exactly as today (E9).

### 3.1 Head structure

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| F01 | Head shape | +12: egg, teardrop, pumpkin, cloud, crescent moon, tofu (rounded cube), gumdrop, acorn, peanut, apple, chubby wide-cheek, pancake (24 total) | 0 | A | P F C T M | M | EXTENDS(head_shape) |
| F02 | Head width (squash / stretch) | 0..15, 8 = today | 4 | D | P F T M | S | NEW |
| F03 | Cheek fullness (mochi cheeks) | 0..7 | 3 | D | P F T | S | NEW |
| F04 | Chin shape | auto, round, pointy, square, soft double, cleft, fluffy beard-tuft (7) | 3 | D | P F | S | NEW |
| F05 | Feature height ("baby face": eyes, nose and mouth move together) | 0..15, 8 = today | 4 | D | P F T | S | NEW |
| F06 | Feature scale (all features as a group) | 0..7 | 3 | D | P F | S | NEW |

### 3.2 Eyes: shape, iris, pupil and sclera are now separate

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| F07 | Eye shape (outline) | +16: almond, puppy big-round, upturned (tsurime), downturned (tareme), hooded, soft monolid, villain triangle, crescent, bored half-moon, shocked wide-ring, teardrop, tall oval, robot LED square, "uwu" arc-down, stitched slit, sleepy line (40) | 0 | A | P F T C M | M | EXTENDS(eye_style) |
| F08 | **Pupil shape** | auto (from eye style), round, vertical slit, horizontal slit (goat), wide cat slit, cross, plus, star, 4-point sparkle, heart, spiral, X, ring, diamond, flower, crescent, keyhole, double pupil, **paw print**, skull, triangle, none (void), pinprick, robot rectangle, bullseye, pentagram, hourglass, clover, music note, lightning, heart-swirl, moon + star (32) | 5 | C | P F (M via E6) | M | NEW |
| F09 | Pupil size (dilation) | 0..7 (pinprick shock ↔ huge cute) | 3 | D | P F | S | NEW |
| F10 | Iris style | auto, solid, two-tone gradient, limbal ring, starburst, galaxy (dithered stars), segmented (robot), glossy anime gradient, flat cartoon, spiral, rainbow (palette cycle), cat gradient, glow core, crystal facets, ringed target, void (no iris) (16) | 4 | C | P F | M | NEW |
| F11 | Iris size | 0..7 (sanpaku ↔ all-iris puppy eyes, no sclera) | 3 | D | P F T | S | NEW |
| F12 | Eye colour 2 (lower iris / gradient) | C6 | 6 | D | P F | S | NEW |
| F13 | **Heterochromia** mode | none, full (right eye uses F14), sectoral (half iris), central ring, freckle spot, gradient split (6) | 3 | C | P F T M | S | EXTENDS(eye_color) |
| F14 | Right-eye colour | C6 | 6 | C | P F T M | S | NEW |
| F15 | **Sclera colour** | auto (white), cream, pale pink, pale blue, **black (demon)**, deep red, yellow (goat), void purple, glowing, match skin (frog), mint, bone, gold, gradient dark, star-speckled black, bloodshot cute (16) | 4 | C | P F | S | NEW |
| F16 | Eye shine (highlights) | auto (2, per art direction), one, three sparkly, star, heart, cross glint, square (robot), none ("hypnotised") (8) | 3 | C | P F | S | NEW |
| F17 | Eye glow | off, steady, slow pulse, flicker (4) | 2 | D | P F T C M | M | NEW |
| F18 | Upper lashes | auto (from style), none, short, long, flutter (3 big cartoon), thick winged, spiky goth, single curl, droopy, doll painted, feathery (owl), sparkle-tipped, bottom-heavy, mismatched L/R, cat flick, mascara clump (16) | 4 | C | P F | S | EXTENDS(eye_style: LASHES) |
| F19 | Lower lashes | none, few, full, painted doll (4) | 2 | D | P F | S | NEW |
| F20 | Lash colour | C6 (auto = outline) | 6 | D | P F | S | NEW |
| F21 | Eyelid droop (resting openness) | 0..7 (wide awake ↔ bedroom-sleepy half-lid) | 3 | C | P F T | S | NEW |
| F22 | Eye outline weight | auto, thin, thick, none (glossy dots), coloured (= lash colour) (5) | 3 | D | P F | S | NEW |
| F23 | Eye bags / dark circles | none, light, tired, insomniac goth, plush eye-bag fluff, glitter bags (6) | 3 | D | P F | S | NEW |
| F24 | Eye asymmetry (L/R size) | -3..+3 | 3 | D | P F | S | NEW |
| F25 | Eye count | 1..3 → 1..6 | 0 | A | P F T | M | EXTENDS(eye_count) |
| F26 | Extra-eye placement | auto (forehead), stacked pairs, along the brow, on the horns, on the cheeks, crown ring (6) | 3 | D | P F T | M | EXTENDS(eye_count) |
| F27 | Default gaze | forward, side-eye L, side-eye R, up (dreamy), down (shy), cross-eyed (derp), wall-eyed (7) | 3 | D | P F T | S | NEW |
| F28 | Special eye, per side | none, **button eye** (plush), stitched shut, robot LED, scope eye, glass doll eye, star eye, glowing ember (8) + side L/R/both | 5 | D | P F T M | M | NEW |

### 3.3 Brows

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| F29 | Brow style | +12: fluffy tufts, zigzag, villain angle, sad droop, lightning, feathered (owl), slit cut, heart, stitched, antenna brows, fur ridge, pencil arch (24) | 0 | A | P F | S | EXTENDS(brow_style) |
| F30 | **Brow colour** | C6 (auto = hair colour) | 6 | C | P F | S | NEW |
| F31 | Brow thickness | 0..7 | 3 | C | P F | S | NEW |
| F32 | Brow resting angle | -3..+3 (worried ↔ cross) | 3 | D | P F T | S | NEW |
| F33 | Brow spacing | 0..7 (0 joins into a unibrow) | 3 | D | P F | S | EXTENDS(brow_style: UNIBROW) |
| F34 | Brow length | 0..3 | 2 | D | P F | S | NEW |

### 3.4 Nose, muzzle and whiskers

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| F35 | Nose style | +12: heart, pink triangle (cat), bear oval, wet dog nose, bunny Y, mouse pinhead, mini beak, star, cross-stitched button, bean, robot speaker grille, snail dot (24) | 0 | A | P F T | S | EXTENDS(nose_style) |
| F36 | **Nose colour** | C6 (auto = species pink/dark) | 6 | C | P F T C M | S | NEW |
| F37 | Nose shine | off, on | 1 | D | P F | S | NEW |
| F38 | Nose height | 0..7 | 3 | D | P F | S | NEW |
| F39 | **Muzzle / snout style** | auto (species), none, small round, feline puffs (W), canine short, canine long (fox/collie), bear broad, ovine, murine pointy, owl facial disc, pig snout, duck bill, dragon snout, frog wide, beaked, plush-stitched (16) | 4 | C | P F T M | M | NEW (species-implied today) |
| F40 | Muzzle length | 0..7 | 3 | C | P F T | M | NEW |
| F41 | Muzzle puffiness | 0..3 | 2 | D | P F | S | NEW |
| F42 | Muzzle colour | C6 (auto = skin2 / marking) | 6 | D | P F T C M | S | EXTENDS(marking: FUR_MUZZLE) |
| F43 | Whisker pads | none, dots, freckle dots, big spots (4) | 2 | D | P F | S | NEW |
| F44 | **Whiskers** | auto (species), none, short x2, long x3, curly, droopy elder, drawn-on, glowing (8) | 3 | D | P F T | S | NEW (species-only today) |
| F45 | Whisker length | 0..3 | 2 | D | P F | S | NEW |
| F46 | Whisker colour | C6 (auto = outline) | 6 | D | P F | S | NEW |

### 3.5 Mouth, lips, teeth and tongue

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| F47 | Mouth style | +12: zipper, jack-o'-lantern, kissy "3", nervous squiggle, wide frog grin, tiny curve, vampire smile, buck-tooth bunny smile, hidden in fluff, rabbit/dog "Y" split, crooked smile, sleepy drool-o (32) | 0 | A | P F T | S | EXTENDS(mouth_style) |
| F48 | Mouth width | 0..7 | 3 | D | P F | S | NEW |
| F49 | Mouth height | 0..7 | 3 | D | P F | S | NEW |
| F50 | Resting mouth curve | -3..+3 (frowny ↔ smiley, applied under every expression) | 3 | D | P F T | S | NEW |
| F51 | **Lip colour / lipstick** | C6 (auto = none). Lipstick becomes a colour layer that works with any mouth | 6 | D | P F | S | EXTENDS(mouth_style: LIPSTICK) |
| F52 | Lip style | none (critter default), thin, full, pouty, glossy, ombre, black goth (7) | 3 | D | P F | S | NEW |
| F53 | **Teeth** | auto, none, neat, **buck teeth**, **gap tooth**, **snaggle tooth**, braces, gold tooth, missing tooth, lower tusks, shark rows, needle teeth, candy teeth, molar grin, top+bottom fangs, single big tooth (16) | 4 | C | P F | S | EXTENDS(mouth_style: TEETH, SHARK) |
| F54 | Fang placement | both, left, right, lower, upper + lower (5) | 3 | D | P F | S | EXTENDS(fangs) |
| F55 | Teeth colour | white, ivory, gold, silver grill, black, candy stripe (6) | 3 | D | P F | S | NEW |
| F56 | **Tongue** | auto, **blep** (always a little out), long lick, forked, split tip, panting, studded (7) | 3 | C | P F T | S | EXTENDS(mouth_style: TONGUE) |
| F57 | Tongue colour | pink, red, purple (chow/demon), blue, black, slime green (6) | 3 | D | P F | S | NEW |

### 3.6 Cheeks and skin details

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| F58 | **Blush style** | ovals, circles, anime hatching ///, hearts, stars, spirals (tama-swirl), dots, full flush, patchy, stickers, single line, glow (12) | 4 | C | P F T | S | EXTENDS(cheeks: BLUSH) |
| F59 | **Blush colour** | C6 (auto = rose) | 6 | C | P F T C M | S | NEW |
| F60 | Blush intensity | 0..3 (dither density) | 2 | D | P F | S | NEW |
| F61 | Blush position | high, mid, low, across the nose (4) | 2 | D | P F | S | NEW |
| F62 | Blush size | 0..3 | 2 | D | P F | S | NEW |
| F63 | Freckle pattern | cheeks, nose bridge, cheeks + nose, all-over speckle, shoulders, stars, hearts, single cluster (8) | 3 | D | P F | S | EXTENDS(cheeks: FRECKLES) |
| F64 | Freckle density | 0..3 | 2 | D | P F | S | NEW |
| F65 | Freckle colour | C6 (auto = darker skin) | 6 | D | P F | S | NEW |
| F66 | Beauty mark position | under eye L/R, lip L/R, cheek L/R, chin, forehead (8) + count 1-2 | 4 | D | P F | S | EXTENDS(cheeks: BEAUTY_MARK) |
| F67 | Scar slot 1 | none, cheek slash, through eye L, through eye R, across nose, lip, brow nick, X cross, burn patch, stitched, claw x3, lightning, chin, ear notch, forehead, tiny nick (16) | 4 | D | P F T | M | EXTENDS(cheeks: SCAR; brow_style: SCARRED) |
| F68 | Scar slot 2 | same list | 4 | D | P F T | M | NEW |
| F69 | **Plush seams and stitches** | none, face centre seam, eye-ring seam, mouth-corner stitches, fabric patch, sewn-on button, head zipper, safety pin, stuffing tuft poking out, cross-stitch X, quilt square, heart patch, mended tear, ear seam, chin stitch, cheek darn (16) | 4 | D | P F T M | M | EXTENDS(marking: STITCHES, PATCHES) |
| F70 | Seam / stitch thread colour | C6 (auto = darker skin) | 6 | D | P F T | S | NEW |
| F71 | Bandage / band-aid | cheek plaster, anime X plaster, heart band-aid, nose strip, head wrap, eye wrap, partial mummy wrap, chin plaster (8) | 3 | D | P F T | S | EXTENDS(cheeks: BANDAGE) |
| F72 | Band-aid colour | C6 | 6 | D | P F | S | NEW |
| F73 | **Cheek fluff** (face tufts) | auto (species), none, small, big, sideburn mane, double tuft, curly wool, feathery (8) | 3 | D | P F T C M | M | EXTENDS(species cheek tufts) |
| F74 | Face gems / stickers | none, star under eye, heart under eye, teardrop gem, rhinestone line, forehead gem, moon, sticker dots (8) | 3 | D | P F | S | EXTENDS(cheeks: SPARKLES) |
| F75 | Face-gem colour | C6 | 6 | D | P F | S | NEW |
| F76 | Face age detail | none, baby chub, laugh lines, wrinkly brow, sleepy elder lids (5) | 3 | D | P F | S | NEW |

### 3.7 Makeup

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| F77 | Eyeshadow style | none, lid wash, smoky, wing, cut crease, halo, glitter, rainbow gradient, goth raccoon smudge, under-eye pink (aegyo-sal) (10) | 4 | D | P F | S | NEW |
| F78 | Eyeshadow colour | C6 | 6 | D | P F | S | NEW |
| F79 | **Eyeliner style** (its own layer) | none, tightline, small wing, cat-eye, double wing, goth thick, floating graphic, dot liner, puppy liner, pharaoh (10) | 4 | D | P F | S | EXTENDS(marking: EYELINER) |
| F80 | Eyeliner colour | C6 | 6 | D | P F | S | NEW |
| F81 | Highlighter shine (cheekbones, nose tip) | 0..3 | 2 | D | P F | S | NEW |
| F82 | Nail / claw polish | none, solid, tips, glitter, black goth (5) + colour C6 | 9 | D | F T | S | NEW |

### 3.8 Face paint and face tattoos

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| F83 | **Face paint** (split from `marking`, so it stacks with fur patterns) | none, clown, skull paint, bold stripes, cheek hearts, tears, pentagram, third eye (7 legacy) + war stripes, whisker paint, star over eye, harlequin diamonds, pierrot tear, mime white, half-face skull, flower skull, Hillvile heart badge, Hellvile horns badge, lightning bolt, glitter mask, bandit stripe, cheek numbers, butterfly, zig-zag (24) | 5 | D | P F T M | M | EXTENDS(marking: CLOWN, SKULL_PAINT, TRIBAL, CHEEK_HEARTS, TEARS, PENTAGRAM, THIRD_EYE) |
| F84 | Face paint colour 1 | C6 (shares `marking_color` today) | 6 | D | P F T | S | EXTENDS(marking_color) |
| F85 | Face paint colour 2 | C6 | 6 | D | P F | S | NEW |
| F86 | Face paint finish | faded, half, full, neon glow (4) | 2 | D | P F | S | NEW |
| F87 | Face tattoo | none, tear, heart, star, cross, "666", paw print, rose, cheek spiderweb, chin dots, forehead moon, "XO", tiny bat, crown, bolt, music note (16) | 4 | D | P F | S | EXTENDS(cheeks: TEAR_TATTOO) |
| F88 | Face tattoo colour | C6 | 6 | D | P F | S | NEW |

### 3.9 Piercings

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| F89 | Ear piercings | none, studs, hoops, helix stack, industrial bar, gauges, dangle skull, dangle heart, dangle star, ear cuffs, bell, feather, safety pin, chain-to-ear, crescent (15) | 4 | D | P F T | S | EXTENDS(accessory: EARRINGS) |
| F90 | Ear piercing side | left, right, both (3) | 2 | D | P F | S | NEW |
| F91 | Facial piercings (bitmask) | nose stud, nose ring, septum, brow bar, lip ring, snake bites, labret, dimple studs | 8 | D | P F | S | EXTENDS(accessory: NOSE_RING) |
| F92 | Tongue stud | off, on | 1 | D | P | S | NEW |
| F93 | Horn rings | 0..3 rings | 2 | D | P F T | S | NEW |
| F94 | Piercing metal | silver, gold, rose gold, black, rainbow anodised, glow, bone, candy (8) | 3 | D | P F | S | NEW |

### 3.10 Glasses (their own slot)

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| F95 | **Glasses slot** (frees the accessory slot, so glasses + scarf + mask all work) | none, round, square, oval, cat-eye, heart, star, aviator, wayfarer shades, round shades, 3D red/cyan, monocle, pince-nez, half-moon readers, taped nerd, hypno swirl, welding goggles, aviator goggles, ski goggles, cyber visor, scouter, VR headset, eyepatch L, eyepatch R, shutter shades, skull rims, flame rims, rimless, sport band, bubble goggles, star-rim sparkly, heart sunnies XL (32) | 5 | C | P F T M | M | EXTENDS(accessory: GLASSES, SUNGLASSES, MONOCLE, EYEPATCH) |
| F96 | Frame colour | C6 | 6 | D | P F T | S | NEW |
| F97 | Lens tint | clear, pink, blue, amber, black, mirror, rainbow, swirl, glowing red (9) | 4 | D | P F | S | NEW |
| F98 | Glasses position | on the eyes, low on the nose, pushed up on the forehead, on top of the hat (4) | 2 | D | P F T | S | NEW |

### 3.11 Hair and facial hair

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| H01 | Hair style | +32: wolf cut, shag, side ponytail, high pony + scrunchie, low tails, half-up, fishtail braid, blunt bob + bangs, pixie, undercut, quiff, curtain bangs, twin buns with tails, sheep cloud, owl feather crest, ghost wisp, rat tail, beehive, cornrows, finger waves, bedhead, spiky hero, curly mullet, crown braid long, puff balls, mushroom, sideburns only, princess curls, space buns + streamers, flame-tip mohawk, slime gel, plush yarn hair (64) | 0 | A | P F C T M | M | EXTENDS(hair_style) |
| H02 | **Bangs / fringe** (separate front layer) | auto, none, blunt, side-swept L, side-swept R, curtain, wispy, choppy, eye-covering (emo), heart fringe, M-bangs, finger curl, micro, split, braided fringe, swoop (16) | 4 | D | P F T | M | NEW |
| H03 | **Ahoge / cowlick** (bounces) | none, single strand, heart, double antenna, question mark, coil spring, lightning, flame tip, star tip (9) | 4 | D | P F T C M | S | NEW |
| H04 | Hair volume | 0..3 | 2 | D | P F T M | M | NEW |
| H05 | Hair length | 0..3 (for styles that scale: long, ponytails, twintails, braids) | 2 | D | P F T | M | NEW |
| H06 | Part side / mirror | default, mirrored | 1 | D | P F C T M | S | NEW |
| H07 | **Dye pattern** | streaks (today), tips/ombre, roots, **split dye**, underdye (peekaboo), calico patches, single streak, money piece, 3-step gradient, rainbow (cycling), galaxy, two-tone twintails (12) | 4 | D | P F T C M | M | EXTENDS(hair_highlight) |
| H08 | Hair colour 3 | C6 | 6 | D | P F T | S | NEW |
| H09 | Hair shine | matte, soft, angel ring, sparkle (4) | 2 | D | P F | S | NEW |
| H10 | Hair accessory | none, big bow, small bow, ribbon tails, star clips, heart clips, bone clip, scrunchie, side flower, X pins, headband, hair stick, bone-through-bun, candy clip, skull clip, bell, beads, feather (18) | 5 | D | P F T C | S | NEW |
| H11 | Hair accessory colour | C6 | 6 | D | P F T | S | NEW |
| H12 | Hair accessory side | left, right, both, centre (4) | 2 | D | P F | S | NEW |
| H13 | Facial hair style | +8: soul patch, chinstrap, fu manchu, pencil mustache, walrus, braided beard, chest-fluff beard, goat beard (16) | 0 | A | P F T | S | EXTENDS(facial_hair) |
| H14 | Facial hair colour | C6 (auto = hair) | 6 | D | P F T | S | NEW |

### 3.12 Ears

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| E01 | Ear style | +16: deer, half-lop, cow, pig, goat, hamster, panda, otter, axolotl gills, fennec, lynx, wolf, elephant, leaf, ghost wisp, robot discs (32) | 0 | A | P F T C M | M | EXTENDS(ears) |
| E02 | **Ear size** | 0..7 | 3 | C | P F T C M | S | NEW |
| E03 | Ear angle (splay) | 0..7 (upright ↔ airplane ears) | 3 | D | P F T M | S | NEW |
| E04 | **Ear fold / droop** | none, tip fold, half lop, full lop, left ear down, right ear down, both down (7) | 3 | C | P F T M | M | NEW |
| E05 | **Inner ear colour** | C6 (auto = pink / skin2) | 6 | C | P F T M | S | NEW |
| E06 | Ear tip detail | none, tipped colour, lynx tufts, fluffy inner fur, notched (alley cat), torn, frosted, heart notch (8) | 3 | D | P F T | S | EXTENDS(marking: FUR_TIPPED_EARS) |

---

## 4. (b) Body customisation, including furry features

Today: `body` (10 types), `height` and `build` sliders, one `skin` colour, a second fur colour borrowed from `marking_color`, and species-implied fur tufts, chest fluff, hands and feet. The wishlist adds plush-toy proportions, fur and material, real fur patterns with 3 colours, paw and foot types, digitigrade legs, multi-tails, and adorable demon anatomy.

### 4.1 Proportions (stylised plush ranges, never realistic or sexualised)

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| B01 | Body type | +6: toddler pudge, hourglass, broad-shoulder triangle, bean, mascot (huge head, tiny body), tall elegant (16) | 0 | A | F C T M | M | EXTENDS(body) |
| B02 | **Shoulder width** | 0..7 | 3 | C | F T M C | M | NEW |
| B03 | Torso volume (chest-fluff barrel) | 0..7 | 3 | D | F T M | M | NEW |
| B04 | Waist | 0..7 | 3 | D | F T M | M | NEW |
| B05 | Hips | 0..7 | 3 | D | F T M | M | NEW |
| B06 | **Belly / tummy** | 0..7 (flat ↔ round plush tummy) | 3 | C | F T M C | M | EXTENDS(build) |
| B07 | **Arm length** | 0..7 (T-rex nubs ↔ long) | 3 | C | F T M | M | NEW |
| B08 | Arm thickness | 0..3 | 2 | D | F T M | S | NEW |
| B09 | **Leg length** | 0..7 (stubby ↔ long) | 3 | C | F T M C | M | NEW |
| B10 | Leg thickness | 0..3 | 2 | D | F T M | S | NEW |
| B11 | Neck length | 0..3 | 2 | D | P F T M | S | NEW |
| B12 | Head-to-body ratio (chibi level) | auto, 2-head SD, 2.5, 3, 4-head (5) | 3 | D | F C T M | L | EXTENDS(head_size) |
| B13 | Posture | auto, upright, proud chest-out, slouch, hunched, shy (shoulders in), sassy hip, lean back, limp float (9) | 4 | D | F T M C | M | EXTENDS(body: HUNCHED) |
| B14 | Hand size | 0..3 | 2 | D | F T M | S | NEW |
| B15 | Foot size | 0..3 (max = clown feet) | 2 | D | F T M C | S | NEW |
| B16 | Muscle tone | 0..3 (soft plush ↔ toned lines) | 2 | D | F T | S | EXTENDS(body: BUFF) |
| B17 | Roundness (plush squish) | 0..3 | 2 | D | F T C M | S | NEW |

### 4.2 Fur, skin and material

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| B18 | **Body material** | auto (species), short fur, long shaggy fur, wool curls, velvet plush, patchwork plush, scales, feathers, smooth skin, slime (translucent + bubbles), stone (cracks), bone, porcelain doll, metal plating, glitter, crystal facets, cloud/vapour, gummy candy, shadow, knit/yarn (amigurumi), felt, sequins, bark, chrome, marshmallow, papercraft, stained glass, ice, lava rock, jelly, peach fuzz, terrycloth (32) | 5 | C | P F T C M | L | EXTENDS(skin; species SPF_SQUISHY / MECHANICAL) |
| B19 | **Fluffiness** (outline tuft count) | 0..7 | 3 | C | P F T C M | M | NEW |
| B20 | Fur length | 0..3 | 2 | D | P F T | M | NEW |
| B21 | Material sheen | auto, matte, soft, glossy, wet, metallic, sparkle (7) | 3 | D | P F T M | M | NEW |
| B22 | Translucency | 0..3 (opaque ↔ see-through; PS1 GPU semi-transparency, GBA dither) | 2 | D | P F T M | M | NEW |
| B23 | Natural fur colours | +8 natural fur tones: cream, fawn, ginger, blue-grey, brindle brown, silver, black-brown, apricot. The palette is full, so this needs E7 | 0 | A | P F C T M | L | EXTENDS(skin) |
| B24 | Fur colour 2 (no longer shared with face paint) | C6 | 6 | D | P F T C M | S | EXTENDS(marking_color) |
| B25 | **Fur colour 3** | C6 (calico third, accents, tips) | 6 | C | P F T C M | S | NEW |
| B26 | **Fur pattern** (split from `marking`) | none, muzzle, belly, socks, mask, tabby, calico (legacy) + gloves, mackerel tabby, swirl tabby, tortoiseshell, tuxedo, points (siamese), saddle, brindle, merle, dalmatian, leopard rosettes, tiger, cheetah, piebald, skunk stripe, fox socks + tip + bib, panda, cow, ladybug, zebra, countershade, ombre, heart spots, galaxy speckle, chipmunk stripes, raccoon rings (32) | 5 | C | P F T C M | L | EXTENDS(marking: FUR_*) |
| B27 | Pattern scale / density | 0..3 | 2 | D | P F T | S | NEW |
| B28 | Pattern placement (bitmask) | head, torso, arms, legs, tail | 5 | D | P F T C | M | NEW |
| B29 | Belly / underside colour | C6 (auto = skin2) | 6 | D | F T C M | S | EXTENDS(marking: FUR_BELLY) |
| B30 | **Paw pad colour** | C6 (auto = pink/dark) | 6 | D | F T P | S | NEW |
| B31 | Paw pad shape | **toe beans**, heart pads, star pads, none (4) | 2 | D | F T | S | NEW |
| B32 | Claws | none, short, long, cat-out, painted (= F82) (5) | 3 | D | F T | S | EXTENDS(shoes: CLAWS) |
| B33 | Claw colour | C6 | 6 | D | F T | S | NEW |
| B34 | **Chest fluff / ruff** | auto, none, small tuft, big ruff, lion mane, collar ruff, heart-shaped fluff, shaggy bib (8) | 3 | D | P F T M | M | EXTENDS(species chest_fluff) |
| B35 | Chest fluff colour | C6 (auto = fur 2) | 6 | D | P F T | S | NEW |
| B36 | **Mane / crest** | none, lion, horse crest, punk spines, feather crest, cloud collar, spiky dragon frill, flower mane (8) | 3 | D | P F T M | M | NEW |
| B37 | Grey muzzle (ageing fur) | none, light, senior grey, all silvered (4) | 2 | D | P F | S | NEW |
| B38 | **Glow markings** (emissive) | none, circuit lines, cute sigils, bioluminescent spots, lava cracks, heart chest glow, constellation, stripe trails, eye trails, rune ring, pulse dots, rim glow, neon outline, tail-tip lamp, glowing freckles, belly moon (16) | 4 | D | P F T C M | M | NEW |
| B39 | Glow colour | C6 | 6 | D | P F T C M | S | NEW |
| B40 | Glow pulse | steady, slow, fast, heartbeat (palette cycling) (4) | 2 | D | P F T C | S | NEW |
| B41 | Body tattoo 1 design | none, heart, skull, anchor, flame, star, paw, rose, snake, dagger-heart, web, moon, "DOOMO" banner, band, bat, ghost (16) | 4 | D | F T | M | NEW |
| B42 | Body tattoo 1 placement | L arm, R arm, chest, back, L leg, R leg, belly, neck (8) | 3 | D | F T | S | NEW |
| B43 | Body tattoo 2 (design + placement) | same lists | 7 | D | F T | M | NEW |
| B44 | Tattoo ink colour | C6 | 6 | D | F T | S | NEW |
| B45 | Body scars and bandages | none, arm stitches, patchwork seam, leg patch, bandaged arm, arm sling/cast, tail bandage, chest scar, knee plaster, burn mark (10) | 4 | D | F T M | M | NEW |
| B46 | **Plush construction** | none, side seams, centre seam, patchwork quilt (fur 1/2/3), laundry tag on the hip, button joints (teddy), zipper back, squeaker heart (8) | 3 | D | F T M | M | EXTENDS(species PLUSHIE) |

### 4.3 Hands, feet and legs

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| B47 | **Hand type** | auto, plush nubs/mittens, paws with beans, cartoon 4-finger, raptor claws, talons, hooves, skeletal, robot pincers, slime blobs, tentacle tips, flippers, bat wing-hands, ghost wisps, crab claws, white cartoon gloves (16) | 4 | C | F T M P | M | NEW (species-implied today) |
| B48 | **Foot type** | auto, plush nubs, paws, hooves, talons, webbed, human-ish, skeletal, slime puddle base, ghost tail, tentacle skirt, wheels, pogo spring, roots, clawed, built-in boots (16) | 4 | C | F C T M | M | EXTENDS(shoes: HOOVES, CLAWS: body, not shoes) |
| B49 | **Leg style** | plantigrade, **digitigrade**, stumpy (no knees), bird legs, ghost fade, serpentine (naga-cute) (6) | 3 | C | F C T M | L | EXTENDS(species SPF_FLOATS) |
| B50 | Mobility aid | none, cane, crutch, plush wheelchair, hover chair, floaty ring, walker with tennis balls (7) | 3 | D | F C T M | L | NEW |
| B51 | Prosthetic limb | none, robot arm, robot leg, peg leg, plush-sewn limb, hook, crystal limb (7) + side L/R | 4 | D | F T M | M | NEW |

### 4.4 Tails

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| B52 | Tail style | +16: wolf bushy, squirrel curl, kitsune, beaver, peacock, mermaid fin, dragon spade, scorpion, lion tuft, horse, pig curl, deer flick, axolotl, monkey curl, plush button tail, chain tail (32) | 0 | A | F T C M P | M | EXTENDS(tail) |
| B53 | **Tail count** | auto, 1, 2, 3, 4, 5, 7, **9 (kitsune)** (8) | 3 | C | F T M | M | NEW |
| B54 | **Tail length** | 0..7 | 3 | C | F T M C | S | NEW |
| B55 | Tail fluff | 0..3 | 2 | D | F T M | S | NEW |
| B56 | **Tail tip colour** | C6 | 6 | C | F T C M | S | EXTENDS(tail: FOX, RACCOON_RINGED) |
| B57 | Tail tip style | none, tip, rings, gradient, flame tip, heart tip, spade, puff (8) | 3 | D | F T M | S | NEW |
| B58 | Tail pose / curl | auto, hanging, upright (happy cat), curled (spitz), S-curve, wrapped round the body, question mark, tucked (8) | 3 | D | F T C M | M | NEW |

### 4.5 Wings

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| B59 | Wing style | +10: owl, fairy dragonfly, bee, ghost wisp, broken angel, leaf, ribbon, crystal, candy wrapper, umbrella bat (20) | 0 | A | F T M P | M | EXTENDS(wings) |
| B60 | Wing size | 0..7 | 3 | D | F T M P | S | NEW |
| B61 | **Wing colour 1** (membrane / feathers; derived today) | C6 | 6 | C | F T C M P | S | NEW |
| B62 | Wing colour 2 (bones, edges, eye spots) | C6 | 6 | D | F T M | S | NEW |
| B63 | Wing pattern | none, eye spots, gradient, veins, stars, tattered, sparkle, stripes (8) | 3 | D | F T M | M | NEW |
| B64 | Wing pose | folded, half, spread, droopy, idle flutter (5) | 3 | D | F T M | M | NEW |
| B65 | Wing pairs | 1 pair, 2 pairs (seraph), single wing (3) | 2 | D | F T M | M | NEW |

### 4.6 Horns, antennae and head extras

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| B66 | Horn style | +10: wide ram curl, gazelle, jackalope mini-antlers, crystal, candle horns (lit), branch, bandaged broken, 4 tiny nubs, cyclops single, lightning (26) | 0 | A | P F T C M | M | EXTENDS(horns) |
| B67 | Horn count | auto, 1, 2, 4 (4) | 2 | D | P F T M | M | EXTENDS(horns: MANY) |
| B68 | Horn curl | 0..3 | 2 | D | P F T M | M | NEW |
| B69 | Horn texture | smooth, ridged, cracked, spiral grooves, glossy, glitter, metal-capped, striped (8) | 3 | D | P F | S | NEW |
| B70 | Horn tip colour + gradient | C6 + on/off | 7 | D | P F T M | S | NEW |
| B71 | Horn asymmetry | none, left broken, right broken, one missing, mismatched size (5) | 3 | D | P F T M | S | EXTENDS(horns: BROKEN) |
| B72 | Horn position | auto, forehead, temples, top, swept back, side (ram) (6) | 3 | D | P F T M | M | NEW |
| B73 | **Antennae** (own slot, so horns + antennae both work) | none, bobble, feathered moth, clubbed butterfly, springy bee, robot whip with LED, snail eye-stalks, angler lure glow (8) | 3 | D | P F T C M | S | EXTENDS(horns: ANTENNAE) |
| B74 | Antenna colour | C6 | 6 | D | P F T | S | NEW |
| B75 | Head extra | none, dragon crest, mushroom cap, leaf sprout, candle flame, sewn button, lightbulb, cloud puff, pumpkin lid, eggshell top, hatchling tuft, flower bud, tiny crown spike, snow pile, sticky note, bandage cross (16) | 4 | D | P F T C M | M | NEW |

### 4.7 Halo and aura

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| B76 | Halo style | +6: double ring, heart, square (robot), neon glitch, flower ring, saturn ring (12) | 0 | A | P F T C M | S | EXTENDS(halo) |
| B77 | **Halo colour** (derived today) | C6 | 6 | D | P F T C M | S | NEW |
| B78 | Halo tilt + size | tilt 0..3 + size 0..3 | 4 | D | P F T M | S | NEW |
| B79 | Aura | +12: music notes, petals, snowflakes, leaves, orbiting stars, tiny bats, tiny ghosts, lightning, rainbow, pixel squares, candy, embers (20) | 0 | A | P F T C M | M | EXTENDS(aura) |
| B80 | Aura colour | C6 | 6 | D | P F T C | S | NEW |
| B81 | Aura intensity | 0..3 | 2 | D | P F T C | S | NEW |

### 4.8 Extra anatomy, age and species

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| B82 | Extra arms | none, +1 pair, +2 pairs (3) | 2 | D | F T M | L | NEW |
| B83 | Body eyes (cute-creepy Hellvile) | none, palm eyes, belly eye, tail-tip eye (4) | 2 | D | F T | M | EXTENDS(eye_count) |
| B84 | Buddy head (Cerberus-kin: a small second head on the shoulder) | off, on | 1 | D | P F T M | L | NEW |
| B85 | Belly mouth (cacoblob style) | off, on | 1 | D | F T | M | NEW |
| B86 | **Age look** | auto (roster trait), baby, kid, teen, adult, elder (6). **Drives the kid/teen romance gate** for custom citizens | 3 | C | P F C T M | M | NEW (mirrors ROSTER_T_KID / TEEN / ELDER) |
| B87 | Species | +24: axolotl, deer, panda, red panda, hamster, otter, possum, skunk, hyena, crow, duckling, dragon, goat-demon, cow, piglet, snail, bee, spider (phobia-filter aware), shark, gargoyle, candlekin, pumpkinhead, mushroomkin, cloudkin (48). Each needs base parts, `DoomoSpeciesInfo`, 3 presets and head details | 0 | A | P F C T M | L | EXTENDS(species) |
| B88 | Hybrid second species | none + any species (6 bits) + blend 0..3 (2 bits): mixes default parts and skin, used by mixed-species babies | 8 | D | P F C T M | M | NEW |

---

## 5. (c) Clothing and outfit layering

Today: top, bottom, shoes, hat, **one** accessory, and one pattern (top only), each with one colour. The wishlist uses **16 slots**, drawn inside-out:

`body → socks → legwear → shoes → bottom → inner top → belt → main top → outer layer → gloves → neck → back item / cape → wrist → face mask → glasses → hair accessory → ear / horn / tail / wing accessories → hat → held prop`

Dresses and robes (`TOP_DRESS`, `TOP_ROBE`, `BOTTOM_ROBE`) keep hiding the bottom. Every bottom gets an automatic tail hole, and hats get ear holes (O28).

### 5.1 Slots

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| O01 | Inner top (undershirt) | none, tee, long sleeve, turtleneck, fishnet, striped long sleeve, mesh, collared shirt, tank, frilly blouse, thermal, hoodie-under (12) | 4 | D | P F T M | M | NEW |
| O02 | Inner top colour | C6 | 6 | D | P F T | S | NEW |
| O03 | Main top | +26: sailor uniform, school blazer, cardigan, sweater dress, sports jersey, hawaiian, flannel, varsity, goth corset top, puff-sleeve blouse, apron, chef coat, overall dress, mech pilot suit, wizard robe, cheer uniform, tracksuit, **animal kigurumi onesie**, frog suit, ghost sheet, bee costume, knight tabard, punk vest with patches, poncho, ball gown, festival happi (50) | 0 | A | P F C T M | M | EXTENDS(top) |
| O04 | **Outer layer** | none, open jacket, denim, leather, trench, puffer, cardigan, open hoodie, blazer, vest, haori, lab coat, cloak, cape-coat, fur-trim parka, bolero, poncho, yellow raincoat, varsity, shrug, kimono jacket, bomber, scarf-cape, choir robe, apron, tux jacket, sailor-collar cape, cropped jacket, letterman, track jacket, hunting jacket, rain cape (32) | 5 | C | P F T M C | M | EXTENDS(top: LEATHER_JACKET, LAB_COAT, CLOAK can now layer) |
| O05 | Outer layer colour | C6 | 6 | C | P F T C M | S | NEW |
| O06 | Bottom | +12: bloomers, pleated skirt, jorts, bell-bottoms, harem pants, sweatshorts, hakama, plaid skirt, bubble skirt, cargo shorts, joggers, swim trunks (24) | 0 | A | F T C M | M | EXTENDS(bottom) |
| O07 | **Legwear** | none, tights, fishnets, ripped tights, **striped thigh-highs**, leg warmers, over-knee socks, bandage wraps, knee pads, shin guards, spats, fluffy fake-fur boots (12) | 4 | C | F T C M | M | NEW |
| O08 | Legwear colour | C6 | 6 | C | F T C M | S | NEW |
| O09 | Legwear stripe colour | C6 | 6 | D | F T | S | NEW |
| O10 | Socks | none, ankle, crew, knee, toe socks, frilly ankle, mismatched L/R, paw-print (8) | 3 | D | F T C | S | NEW |
| O11 | Sock colour | C6 | 6 | D | F T C | S | NEW |
| O12 | Shoes | +12: mary janes, high-tops, wellies, ballet flats, geta, moon boots, fluffy boots, cowboy boots, ice skates, flippers, bunny slippers, light-up sneakers (cycling) (24) | 0 | A | F T C M | S | EXTENDS(shoes) |
| O13 | Shoe colour 2 (soles, laces) | C6 | 6 | D | F T | S | NEW |
| O14 | **Gloves** (also tint the first-person weapon hands) | none, mittens, fingerless, opera, boxing, oven mitts, gardening, welding gauntlets, spiked bracers, lace, paw gloves, cartoon white, bandage wraps, rubber, knitted, armour gauntlets (16) | 4 | D | F T M + FP hands | M | NEW |
| O15 | Glove colour | C6 | 6 | D | F T M | S | NEW |
| O16 | Belt | none, simple, studded, **bullet belt**, chain, sash/obi, fanny pack, tool belt, utility pouches, back bow, holster, rope, candy belt, champion belt, skull buckle, wing buckle (16) | 4 | D | F T M | S | NEW |
| O17 | Belt colour | C6 | 6 | D | F T | S | NEW |
| O18 | **Neck item** | none, **bell collar**, spiked collar, lace choker, heart-ring choker, neckerchief, tie, bowtie, scarf, long flutter scarf, cross, skull, heart locket, star, paw locket, fang necklace, pearls, lei, bib, ruff, cowbell, dog tag, pacifier, rosary, chains, ribbon, cravat, medal, headphones round the neck, camera strap, whistle, amulet (32) | 5 | D | P F T M | M | EXTENDS(accessory: BOWTIE, CROSS_NECKLACE, SKULL_NECKLACE, SCARF, ROSARY, CHAINS, PACIFIER) |
| O19 | Neck item colour | C6 | 6 | D | P F T | S | NEW |
| O20 | **Back item** | none, backpack, critter mini-pack, short cape, long cape, fake-wing pack, guitar, quiver, cute sword sheath, jetpack, turtle shell, coffin backpack, strapped teddy, tied balloon, bindle, propane tank, kite, umbrella, surfboard, bow, broom, wind-up key, parasol, shield, banner flag, plush in a baby carrier, scroll case, plushie pile, lantern pole, cello case, briefcase, wing-shaped bag (32) | 5 | D | F T M C | M | EXTENDS(accessory: CAPE, BACKPACK, TEDDY) |
| O21 | Back item colour | C6 | 6 | D | F T M | S | NEW |
| O22 | **Face mask** | none, cloth, plague doctor, hockey, oni, kitsune, gas mask, bandit bandana, domino, luchador, masquerade, skull, bunny mask, welder visor, hannya-cute, ghost veil (16) | 4 | D | P F T M | M | EXTENDS(accessory: MASK, GAS_MASK) |
| O23 | Mask colour | C6 | 6 | D | P F T | S | NEW |
| O24 | Mask position | on the face, pushed to the side of the head, on top of the head (3) | 2 | D | P F T | S | NEW |
| O25 | Hat | +24: sun hat with flower, sailor cap, crossing-guard cap, nurse cap, mortarboard, santa, pumpkin hat, **frog hat**, knit hat with ear holes, animal-ear hood, visor, tricorn, wizard, jester, birthday crown, mushroom hat, ushanka, hard hat, flat cap, deerstalker, halo band, newsboy, straw hat, bonnet (48) | 0 | A | P F T C M | M | EXTENDS(hat) |
| O26 | **Hat tilt** | -2..+2 (5) | 3 | C | P F T M | S | NEW |
| O27 | Hat size | 0..3 | 2 | D | P F T | S | NEW |
| O28 | **Hat and ears** | auto (ears poke through holes), tucked under, flattened (3) | 2 | D | P F T M | M | NEW |
| O29 | Hat colour 2 (band / trim) | C6 | 6 | D | P F T | S | NEW |
| O30 | Ear accessory | none, ear bows, ear ribbons, ear warmers, ear-cuff chain, flower behind ear, pencil behind ear, bell, star pin, heart pin, feather, bandage (12) | 4 | D | P F T C | S | NEW |
| O31 | Ear accessory colour | C6 | 6 | D | P F T | S | NEW |
| O32 | **Tail accessory** | none, bow, ribbon, ring band, bell, striped tail sock, flame, heart charm, scrunchie, lantern, flower, bandage (12) | 4 | D | F T C M | S | NEW |
| O33 | Tail accessory colour | C6 | 6 | D | F T | S | NEW |
| O34 | Horn accessory | none, rubber safety caps, bows, rings, flowers, fairy lights, candy-stripe wrap, chain between horns (8) | 3 | D | P F T M | S | NEW |
| O35 | Horn accessory colour | C6 | 6 | D | P F T | S | NEW |
| O36 | Wing accessory | none, ribbons, charms, wing band, piercings, flowers, bells, fairy lights (8) | 3 | D | F T M | S | NEW |
| O37 | Wrist item | none, bracelet, friendship-bracelet stack, watch, spiked cuffs, **broken shackle** (freed Hellvile prisoner, Ch.4), sweatband, scrunchie, rosary bracelet, smart watch, charm bracelet, bangle stack (12) | 4 | D | F T | S | NEW |
| O38 | Wrist colour | C6 | 6 | D | F T | S | NEW |
| O39 | Held prop (out of combat) | none, umbrella, plush, balloon, lollipop, book, phone, lantern, candle, bubble wand, pitchfork, broom, wand, fan, bouquet, coffee, bubble tea, handheld "DoomoBoy", skateboard, guitar, rubber duck, fishing rod, spatula, teddy, yarn and needles, flag, trumpet, magnifier, cute scythe, sparkler, map (32) | 5 | D | F C M | M | NEW |
| O40 | Prop colour | C6 | 6 | D | F C | S | NEW |
| O41 | Accessory (legacy misc slot) | Stays valid. The creator moves legacy values into the new slots when it opens (saves are not rewritten) | 0 | A | P F T | S | EXTENDS(accessory) |

### 5.2 Per-piece modifiers

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| O42 | **Colour 2 per piece** (trim, cuffs, lining) | C6 each for top, bottom, shoes, outer (the hat uses O29) | 24 | D | P F T C M | M | NEW |
| O43 | Pattern on more pieces (lite) | apply-to bitmask: top, outer, bottom, legwear, hat (shares the one pattern and colour) | 5 | D | P F T C M | M | EXTENDS(pattern: top only today) |
| O44 | Per-piece patterns (full) | 4 extra pieces x (pattern 5 + colour 6) | 44 | D | P F T M | M | EXTENDS(pattern, pattern_color) |
| O45 | Pattern catalogue | +24: gingham, argyle, pinstripe, houndstooth, leopard, zebra, stars, moons, paw prints, bones, candy stripes, checkerboard, tie-dye, galaxy, lace, pentagrams, bats, ghosts, flowers, strawberries, rainbow stripes, hearts-and-crossbones, clouds, polka skulls (32) | 0 | A | P F T C M | S | EXTENDS(pattern) |
| O46 | Chest print / emblem (separate from the all-over pattern) | none, heart, skull, star, paw, flame, "DOOMO" logo, number, made-up band logo ("HELLVILE HOLLERS"), smiley, ghost, planet, cherry, alien, 8-ball, lightning (16) | 4 | D | P F T M | S | NEW |
| O47 | Emblem colour | C6 | 6 | D | P F T | S | NEW |
| O48 | **Sleeve length** | auto, sleeveless, cap, short, elbow, long, **sweater paws** (past the hands), bell sleeves, rolled (9) | 4 | D | P F T M | M | NEW |
| O49 | Neckline / collar | auto, crew, V, turtleneck, off-shoulder, sailor collar, peter pan, frilly, hood down, cowl (10) | 4 | D | P F T M | M | NEW |
| O50 | **Hood up** | off, on (hoodies, cloaks, onesies) | 1 | C | P F T C M | M | NEW |
| O51 | Hood ears | none, cat, bear, bunny, devil horns, frog eyes, dino spikes, fox (8) | 3 | D | P F T C M | S | NEW |
| O52 | Top length | crop, normal, long, tunic (4) | 2 | D | F T M | S | NEW |
| O53 | Tucked in | off, on | 1 | D | F T | S | NEW |
| O54 | Outer layer open / closed | open, closed | 1 | D | F T M | S | NEW |
| O55 | Bottom length | micro, short, knee, midi, ankle, floor (6) | 3 | D | F T C M | M | NEW |
| O56 | Rolled cuffs | off, on | 1 | D | F T | S | NEW |
| O57 | Wear and tear | new, patched, ripped, shredded punk (4) | 2 | D | F T | S | NEW |
| O58 | Fabric finish | auto, cotton, knit rib, denim twill, glossy vinyl, satin, sequins (cycling), fur-trimmed (8) | 3 | D | P F T M | M | NEW |
| O59 | **Fit** | fitted, regular, **oversized** (a big hoodie on a tiny plush), baggy (4) | 2 | C | F T M | M | NEW |

### 5.3 Wardrobe and situational outfits

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| O60 | **Situational outfits** | sleep, swim, formal, battle: each a 5-bit index into 32 outfit sets. The player and deluxe citizens get fully custom outfits instead | 20 | D | P F C T M | L | NEW (TOP_PAJAMAS exists) |
| O61 | Auto-switch rules (bitmask) | pyjamas at night, swimwear at the beach, rain gear when raining, scarf when cold, armour in combat | 5 | D | C F | M | NEW |
| O62 | Outfit set presets | 20 → 60: "Father Fuzz cosplay", "Plush Space Marine", "Vapula merch", "Nana's apron"... (ROM only) | 0 | A | F | S | EXTENDS(creator outfit presets) |
| O63 | Uniform by job | automatic uniform from the custom job (A40) | 0 | A | C F | S | NEW |
| O64 | Dirt and stains | automatic from hygiene (option LS10) | 0 | A | F C | S | EXTENDS(stat[STAT_HYGIENE]) |

---

## 6. (d) Animation, personality of motion, and voice

Today all citizens share one walk cycle. Personality only changes speed and expressions, and species drive blink, breathing, ear twitch and wag. The wishlist gives every citizen a body-language profile. Most items are a small table of pose offsets per style applied to the existing 4-frame cycles. The renderer already poses `arm_pose`, `step` and `legs`.

| ID | Item | Values | Bits | Tier | Render | Diff | Status |
|---|---|---|---|---|---|---|---|
| A01 | **Idle style** | auto (persona movement), bouncy, sway, fidget, stoic, float bob, hop in place, slump, toe tap, tail chase, cat stretch, ear scratch, slime wobble, skeleton rattle, robot twitch, shy rock (16) | 4 | C | P F C T M | M | EXTENDS(persona[MOVEMENT]) |
| A02 | Idle fidget frequency | 0..3 | 2 | D | F C T | S | NEW |
| A03 | **Walk style** | auto, normal, waddle, strut, skip, tiptoe sneak, stomp, prance, shuffle, float, hop, slither, march, sashay, bouncy, moonwalk (unlock) (16) | 4 | C | C F T M | M | EXTENDS(SPF_FLOATS) |
| A04 | **Run style** | auto, sprint, arm flail, speedy lean, **all-fours gallop**, penguin waddle-run, float dash, ball roll, stomping charge, skip-run, tiptoe scurry, hop-run, robot rocket, ghost streak, slime slide, cartwheel (16) | 4 | C | T M C | M | NEW |
| A05 | Stride (cosmetic walk speed; gameplay speed unchanged) | 0..3 | 2 | D | C T M | S | NEW |
| A06 | Sit style | auto, cross-legged, kneel, dangle legs, sprawl, **cat loaf**, perch, puddle (8) | 3 | D | C (windows) F | M | NEW |
| A07 | Sleep pose | auto, curled up with the tail, starfish, belly up with paws up, belly down, sitting nod, upside-down hang (bat), coffin, floating (8) | 3 | D | C (windows) F | M | NEW |
| A08 | Dance style | auto (from liked music), bop, robot, twirl, shuffle, headbang, disco point, conga hop, tail wiggle, ballet spin, breakdance spin, noodle flail, tap, waltz, zombie sway, jump-jump (16) | 4 | D | C F T M | M | EXTENDS(like_music) |
| A09 | **Signature emote** | paw wave, heart hands, peace sign, finger guns, salute, curtsy, bow, metal horns, spin pose, pounce, blow kiss, tail-flick wink, cheek poke, shrug, thumbs up, flex, magical-girl sparkle pose, bunny hop, spooky boo, chef's kiss, paw stamp, wiggle, cartwheel, flop, purr, tip hat, skirt twirl, backflip, bubble blow, ear flop, sleepy yawn, howl (32) | 5 | D | F C T P M | M | NEW |
| A10 | Greeting | auto, wave, bow, hug attempt, high five, **nose boop**, ear wiggle, jump hug, cool nod, shy peek, glomp (11) | 4 | D | C F P | M | NEW |
| A11 | **Laugh** (animation + babble pattern) | auto, giggle behind paws, belly laugh, snort, villain cackle, silent shake, wheeze, squeal, sneaky "nyehehe", tail thump (10) | 4 | D | P F C + audio | M | NEW |
| A12 | Cry | auto, waterfall tears, single tear, ugly sob, quiet sniffle, howl, slime melt (7) | 3 | D | P F | S | EXTENDS(EXPR_SAD) |
| A13 | Angry | auto, stomp, steam puff, **fur poof**, pout with crossed arms, flames, ears flat + hiss, tantrum roll (8) | 3 | D | P F C | M | EXTENDS(EXPR_ANGRY) |
| A14 | Flustered / love | auto, hearts, blush + steam, frantic tail wag, face in paws, float-swoon, ears go red, hide in hood (8) | 3 | D | P F C | S | EXTENDS(EXPR_LOVE) |
| A15 | Victory pose (level end, wave clear, exorcism win) | auto, gun twirl, heart pose, flex, jump, bow, spin, hug own plush (8) | 3 | D | T M | M | NEW |
| A16 | Taunt (shooter button) | none, tongue blep, tail wiggle, raspberry, 2-second dance, wave at demons, pat weapon, yawn (8) | 3 | D | T M | M | NEW |
| A17 | Hurt reaction | auto, flinch, squeak-squash, stagger, stuffing puff (cute gore) (5) | 3 | D | T M | S | EXTENDS(fighter anim 6 "hurt") |
| A18 | Faint / KO | auto, face-plant, spin and fall, cartoon stars, plush deflate, ghost poof, dramatic swoon (7) | 3 | D | T M F | M | NEW |
| A19 | **Handedness** (gun hand; the first-person weapon side can follow it) | right, left | 1 | C | T M C F | M | NEW |
| A20 | Blink style | auto, normal, slow, flutter, double blink, wink habit, side lids (lizard/owl) (7) | 3 | D | P F T | S | NEW |
| A21 | Blink rate | 0..3 | 2 | D | P F | S | NEW |
| A22 | **Ear expressiveness** (flat when sad, perked when happy) | 0..3 | 2 | D | P F T C M | M | NEW |
| A23 | Tail wag style | auto, swish (cat), wag (dog), helicopter, tip twitch, slow sway, still (7) | 3 | D | F T C M | S | NEW |
| A24 | Tail wag amount | 0..3 | 2 | D | F T C | S | NEW |
| A25 | Breathing / bounce | 0..3 | 2 | D | P F T C M | S | EXTENDS(idle 1 px squash) |
| A26 | Hair and fur bounce | 0..3 | 2 | D | P F T M | S | NEW |
| A27 | **Resting expression** | auto (persona), neutral, happy, smug, sleepy, grumpy, shy, deadpan, excited, mischievous, worried, uwu (12) | 4 | D | P F T C | M | EXTENDS(persona[EXPRESSION]) |
| A28 | Expression catalogue (global) | +12: smug, embarrassed >///<, determined, bored, confused, star-eyed excited, crying-laughing, pout, uwu, derp, evil grin, dead inside (23) | 0 | A | P F T | M | EXTENDS(EXPR_*) |
| A29 | Talk style | auto, mouth flap, chomp, muzzle only, hand gestures, whisper lean, loud bounce (7) | 3 | D | P F | S | EXTENDS(EXPR_TALK) |
| A30 | Gaze habit | auto, eye contact, looks away (shy), follows the cursor, stares, lazy drift (6) | 3 | D | P F | S | NEW |
| A31 | Mood-to-motion intensity (sad slump, happy skip) | 0..3 | 2 | D | C F T | S | NEW |
| A32 | Footstep sound | auto (from feet), pitter-pat, **plush squeaker**, clop, squish, clank, rattle, whoosh, heels, tap shoes, thud, bell jingle (12) | 4 | D | audio | S | NEW |
| A33 | Babble waveform | auto (voice quality), square, triangle, saw, noise, "meow", "yip", "chirp", "squeak", "ribbit", "bleep", "boo-oo", kazoo, music box, bubble, growl (16) | 4 | D | audio | M | EXTENDS(voice[VOICE_QUALITY]) |
| A34 | Species voice flavour | none, meow-y, bark-y, chirpy, squeaky, burbly, echo (ghost), bitcrush (robot), demonic double (9) | 4 | D | audio | M | NEW |
| A35 | Singing range + vibrato (Concert Hall) | range 0..3 + vibrato 0..3 | 4 | D | audio | S | EXTENDS(voice) |
| A36 | Quirk sounds | none, sparkle sneezes, bubble hiccups, big yawns, purrs when happy, tummy growls when hungry, squeaks when startled, hums while walking (8) | 3 | D | P F + audio | S | NEW |
| A37 | Idle prop habit (uses O39 in idle: yo-yo, phone, book, lollipop, knitting...) | derived | 0 | A | F C | M | NEW |
| A38 | Motion personality preset (macro, not stored): "Shy bean", "Hyper pup", "Royal", "Goth gremlin", "Sleepy blob", "Tough cookie", "Ghosty", "Robot" | fills A01-A36 | 0 | A | - | S | NEW |
| A39 | Voice loudness | 0..3 | 2 | D | audio | S | EXTENDS(voice) |
| A40 | Job (custom citizens; premades keep `roster_job`) | 64 jobs from the roster job list | 6 | D | uniform, sim | M | NEW |

---

## 7. Creator tools: making the complexity usable

These are not stored per resident. They are what keeps 200+ axes friendly on a D-pad.

| ID | Tool | What | Diff | Status |
|---|---|---|---|---|
| T01 | **Simple / Advanced / Everything** modes | Simple shows today's rows. Advanced adds the `C` tier. Everything shows all of them | S | NEW |
| T02 | **Sub-tabs** | Face → Eyes / Brows / Nose & Muzzle / Mouth / Cheeks / Makeup / Paint & Ink / Piercings / Glasses. Body → Shape / Fur / Pattern / Paws & Feet / Tail / Wings / Horns / Extras. Outfit → 16 slots. Motion. Voice | M | EXTENDS(creator tabs) |
| T03 | Pin rows against the randomiser | A pinned row survives "Randomise all / this tab" | S | NEW |
| T04 | **Themed randomiser** | Cute, Spooky, Goth, Pastel, Punk, Royal, Furry, Demon, Angelic, Chaos, "Like Hellvile", "My favourite colour" | S | EXTENDS(randomise) |
| T05 | Colour harmony assist | complementary / analogous / triad suggestions, plus "match fur / hair / eyes" buttons on every colour row | S | EXTENDS("match fur") |
| T06 | Copy / paste | one colour, one section (eyes, outfit) or a whole look between citizens | S | EXTENDS(copy a look) |
| T07 | Symmetry | Symmetric / Mirror / Independent for ears, eyes, horns, gloves and socks (asymmetry via F24, F28, E04, B71, O10) | M | NEW |
| T08 | **Look Book** | 16 saved looks or outfits per island (chunk `BOOK`, about 1.3 KB) | M | NEW |
| T09 | **Look Password** | A base-32 code of the packed look + LookX (about 400 bits → about 80 characters, CRC-16). Works between the GBA and PS1, because the core is shared. A Mii-QR for cartridges | L | NEW |
| T10 | Salon / mirror | Makeovers from the Resident screen or a "Fluff & Fangs Salon" building, for H$ | M | NEW |
| T11 | Before / after | Hold a button to see the original. Split view on PS1 | S | NEW |
| T12 | Jump / search in pickers | Jump to a category or a first letter | S | NEW |
| T13 | Favourites | Star options and show them first | S | NEW |
| T14 | **Photo mode** | pose (A09), expression, stage, frame, stickers, filter. Album entries store **settings only** (re-rendered from the look), so they cost no image memory | M | NEW |
| T15 | Heredity rules for babies | per-field blend / parent / mutation; calico and heterochromia inheritance jokes; hybrid species (B88) | M | EXTENDS(sim_make_baby) |
| T16 | Species-aware smart defaults | Choosing catfolk suggests slit pupils, whiskers and toe beans, but never forces them | S | EXTENDS(cr_species_base) |
| T17 | Possessed peek with custom sclera / glow | | S | EXTENDS(Possessed peek) |
| T18 | PS1 creator turntable shows the real 3D model | `model.c` in the preview | L | EXTENDS(turntable) |
| T19 | Live test buttons | cycle expressions, emote, dance, laugh, walk, run, sit, sleep | S | NEW |
| T20 | Delta-encoded undo | 20 steps without 20 full copies of the bigger record | M | EXTENDS(undo timeline) |
| T21 | Randomise only colours / only shapes | | S | NEW |
| T22 | One-button "Hellvile-ify" / "Hillvile-ify" restyle | | S | NEW |
| T23 | Deluxe capacity meter | "Deluxe looks: 41/64" on the GBA, with "bake to core" | S | NEW |

---

## 8. (e) Options for everything

There are 57 options today (7 pages, max 64). The wishlist proposes about 31 pages, using sub-pages on the GBA. It needs **E4** (global vs island split, `OPTX` chunk, no 64 cap). Existing options keep their `OPT_*` ids, and rows marked `EXTENDS(OPT_*)` only append values. `*` = default. **G** = global (system) and **I** = island rule.

### 8.1 Game

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| GM01 | Combat difficulty | Plushie, Normal*, Nightmare, Ultra-Hellvile, Custom | 5 | I | EXTENDS(OPT_DIFFICULTY) | M |
| GM02 | Life-sim difficulty | Plushie, Normal*, Nightmare, Ultra-Hellvile, Custom | 5 | I | EXTENDS(OPT_DIFFICULTY) | M |
| GM03 | Pet difficulty | Plushie, Normal*, Nightmare, Ultra-Hellvile | 4 | I | EXTENDS(OPT_DIFFICULTY) | S |
| GM04 | Puzzle difficulty (exorcism, welding, minigames) | Plushie, Normal*, Nightmare, Ultra-Hellvile | 4 | I | EXTENDS(OPT_DIFFICULTY) | S |
| GM05 | Autosave | Off, Every day*, Every hour, On location change, On scene exit | 5 | G | EXTENDS(OPT_AUTOSAVE) | S |
| GM06 | Autosave icon | Off, On* | 2 | G | NEW | S |
| GM07 | Confirm prompts | All*, Important only, None | 3 | G | NEW | S |
| GM08 | Pause sim in menus | Off, On* | 2 | I | NEW | S |
| GM09 | Island map travel | Walk, Instant* | 2 | I | NEW | S |
| GM10 | Tutorial hints | On*, Once, Off | 3 | G | NEW | S |
| GM11 | Unlock toasts | Full*, Compact, Silent (log only) | 3 | G | NEW | S |
| GM12 | Spoiler-free unlock hints | Off*, On | 2 | G | NEW | S |
| GM13 | Menu theme | By district*, Always pastel, Always goth | 3 | G | NEW | S |
| GM14 | Boot into | Title*, Continue last save, Pet screen | 3 | G | NEW | S |
| GM15 | Attract / demo mode | Off, On* | 2 | G | NEW | S |
| GM16 | Hellvile spice (rude words in text) | Clean, Grawlix @#$%*, Spicy ("heck") | 3 | G | NEW | S |
| GM17 | Monster Mode | Off*, ON (monster!), Ask each time | 3 | I | EXTENDS(OPT_MONSTER_MODE) | S |

### 8.2 Story mode

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| ST01 | Story pacing | Relaxed, Normal*, Intense | 3 | I | NEW | M |
| ST02 | Invasion encounters | Story beats only, Low, Normal*, High | 4 | I | NEW | M |
| ST03 | Quest markers | Off, Subtle*, Full arrows | 3 | G | NEW | S |
| ST04 | Seen cutscenes | Ask*, Always skip, Never skip | 3 | G | NEW | S |
| ST05 | Hold to skip dialogue | Off, On* | 2 | G | NEW | S |
| ST06 | Protagonist is called | Name*, Nickname, "you" | 3 | I | NEW | S |
| ST07 | Auto-equip best weapon | Off*, On | 2 | I | NEW | S |
| ST08 | Follower in the overworld | Off*, Best friend, Partner, Pet | 4 | I | NEW | L |
| ST09 | Chapter replay | Locked*, Unlocked after credits | 2 | I | NEW | M |
| ST10 | New Game+ carry-over | Nothing, Weapons, Looks + weapons*, Everything | 4 | I | EXTENDS(MODE_NEW_GAME_PLUS) | M |
| ST11 | Story NPC protection | Protected*, Look editable, Fully editable | 3 | I | EXTENDS(RF_STORY_NPC) | S |

### 8.3 Island rules (free play)

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| IS01 | The Wall | Up*, Gate open, Down | 3 | I | NEW | M |
| IS02 | Moving across the border | Story-gated*, Allowed, Never | 3 | I | NEW | S |
| IS03 | Starting money (new island) | 0, 500*, 5000, 50000 | 4 | I | EXTENDS(DOOMO_START_MONEY) | S |
| IS04 | Income rate | x0.5, x1*, x2, x5 | 4 | I | NEW | S |
| IS05 | Shop prices | x0.5, x1*, x2 | 3 | I | NEW | S |
| IS06 | Rent day | Off, On* | 2 | I | EXTENDS(FPLY rent day) | S |
| IS07 | Travelers visiting | Off, Rare, Normal*, Often | 4 | I | EXTENDS(RF_TRAVELER) | S |
| IS08 | Auto-fill empty homes | Off*, Slow, Fast | 3 | I | NEW | M |
| IS09 | Seasons | Off, Four seasons*, Always spring, Always spooky | 4 | I | NEW | M |
| IS10 | Festivals and holidays | Off, On* | 2 | I | NEW | M |
| IS11 | Day/night visuals | Cycle*, Always day, Always night | 3 | I | NEW | S |
| IS12 | Buildings | Unlock with progress*, All open | 2 | I | NEW | S |
| IS13 | News ticker content | All*, Favourites only, Big events only | 3 | I | EXTENDS(OPT_NEWS_TICKER) | S |
| IS14 | Problem bubbles | All*, Important, Favourites only, Off | 4 | I | NEW | S |
| IS15 | Weather | Off, Normal*, Lots, + Hellvile ash, Candy rain | 5 | I | EXTENDS(OPT_WEATHER) | M |

### 8.4 Life sim

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| LS01 | Sim speed | Pause, Chill, Normal*, Fast, Turbo, x8 | 6 | I | EXTENDS(OPT_SIM_SPEED) | S |
| LS02 | Day length | Real time, 2 h, 1 h*, 45 min, 30 min, 15 min, 10 min | 7 | I | EXTENDS(OPT_DAY_LENGTH) | S |
| LS03 | Offline catch-up (platforms with a clock) | None*, Up to 1 day, Up to 7 days | 3 | I | NEW | M |
| LS04 | Autonomy (citizens solve their own needs) | Low, Normal*, High | 3 | I | NEW | M |
| LS05 | Problem frequency | Rare, Normal*, Often | 3 | I | NEW | S |
| LS06 | Problems expire after | Never*, 1 day, 3 days | 3 | I | NEW | S |
| LS07 | Mood swings | Calm, Normal*, Dramatic | 3 | I | NEW | S |
| LS08 | Personality drift over time | Off*, On | 2 | I | NEW | M |
| LS09 | Build drift from eating | Off*, On | 2 | I | NEW | M |
| LS10 | Hygiene visuals (stink lines, stains) | Off, On* | 2 | I | NEW | S |
| LS11 | Ageing (kids grow up) | Off*, Slow, Normal | 3 | I | NEW | L |
| LS12 | Old-age death | Off*, On | 2 | I | EXTENDS(DEATH_OLD_AGE) | M |
| LS13 | Neglect death | Off*, On | 2 | I | EXTENDS(DEATH_NEGLECT) | S |
| LS14 | Jobs and wages | Off, On* | 2 | I | NEW | M |
| LS15 | Gift pickiness | Easy, Normal*, Picky | 3 | I | NEW | S |
| LS16 | Home visits between citizens | Off, On* | 2 | I | NEW | M |
| LS17 | Sleep schedule | By trait*, Everyone sleeps, Party island (nobody sleeps) | 3 | I | NEW | S |
| LS18 | Dreams | Off, Rare, Normal*, Often | 4 | I | NEW | S |
| LS19 | Thought bubbles | Icon*, Text, Both | 3 | G | NEW | S |
| LS20 | Catchphrase frequency | Rare, Normal*, Often | 3 | I | NEW | S |

### 8.5 Needs (one decay setting per need)

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| ND01 | Hunger decay | Off, Slow, Normal*, Fast, Brutal | 5 | I | NEW (STAT_HUNGER) | S |
| ND02 | Happiness decay | Off, Slow, Normal*, Fast, Brutal | 5 | I | NEW (STAT_HAPPY) | S |
| ND03 | Hygiene decay | Off, Slow, Normal*, Fast, Brutal | 5 | I | NEW (STAT_HYGIENE) | S |
| ND04 | Energy decay | Off, Slow, Normal*, Fast, Brutal | 5 | I | NEW (STAT_ENERGY) | S |
| ND05 | Health decay / sickness | Off, Slow, Normal*, Fast, Brutal | 5 | I | NEW (STAT_HEALTH) | S |
| ND06 | Sanity decay | Off, Slow, Normal*, Fast, Brutal | 5 | I | NEW (STAT_SANITY) | S |
| ND07 | Corruption gain | Off, Slow, Normal*, Fast, Brutal | 5 | I | NEW (STAT_CORRUPTION) | S |
| ND08 | Social decay (loneliness) | Off, Slow, Normal*, Fast, Brutal | 5 | I | NEW (STAT_SOCIAL) | S |
| ND09 | Need floor | 0*, 10 (forgiving) | 2 | I | NEW | S |
| ND10 | Need alert threshold | 10, 25*, 40 | 3 | G | NEW | S |
| ND11 | Fridge auto-feed from inventory | Off*, On | 2 | I | NEW | M |
| ND12 | Need display | Bars*, Hearts, Numbers, Faces | 4 | G | NEW | S |

### 8.6 Romance, family and babies

(Kid and teen romance gating is fixed. It is not an option.)

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| RO01 | Romance | On*, Crushes only, Off | 3 | I | NEW | S |
| RO02 | Crush rate | Rare, Normal*, Often | 3 | I | NEW | S |
| RO03 | Confession luck | Harsh, Normal*, Kind | 3 | I | NEW | S |
| RO04 | Weight of your advice | Low, Normal*, High | 3 | I | NEW | S |
| RO05 | Cross-border romance | On*, After the Wall falls, Off | 3 | I | NEW | S |
| RO06 | Dating → proposal speed | Slow, Normal*, Fast | 3 | I | NEW | S |
| RO07 | Marriage | Off, On* | 2 | I | NEW | S |
| RO08 | Breakups | Off, Rare, Normal* | 3 | I | NEW | S |
| RO09 | Divorce | Off, On* | 2 | I | NEW | S |
| RO10 | Ex drama | Off, On* | 2 | I | EXTENDS(REL_EX) | S |
| RO11 | Jealousy and love triangles | Off*, On | 2 | I | NEW | M |
| RO12 | Rivals and nemeses | Off, Rare, Normal*, Spicy | 4 | I | EXTENDS(REL_RIVAL, REL_NEMESIS) | S |
| RO13 | Arguments | Rare, Normal*, Often | 3 | I | NEW | S |
| RO14 | Friendships fade | Off, Slow, Normal* | 3 | I | NEW | S |
| RO15 | Babies | Off, Rare, Normal*, Often | 4 | I | EXTENDS(sim_make_baby) | S |
| RO16 | Where babies live | Parent's district*, Either, Ask me, Never auto | 4 | I | NEW | M |
| RO17 | Baby kind | By parents*, Always angel, Always hellspawn, Random | 4 | I | NEW | S |
| RO18 | Baby look heredity | Blend*, Mostly parent A, Mostly parent B, Random | 4 | I | NEW | M |
| RO19 | Baby mutation chance | None, Low*, Medium, Wild | 4 | I | NEW | S |
| RO20 | Baby growth | Moves in as a kid*, Grows over days | 2 | I | NEW | L |
| RO21 | Baby naming | Auto, Ask me* | 2 | I | NEW | S |
| RO22 | Adoption at Town Hall | Off*, On | 2 | I | NEW | M |
| RO23 | Premade relationships on a new island | Keep*, Clear, Randomise | 3 | I | NEW | M |
| RO24 | Relationship notifications | All*, Close friends, Off | 3 | G | NEW | S |

### 8.7 Possession, exorcism and death

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| PX01 | Possessions | Off, Rare, Normal*, Frequent, Story only | 5 | I | EXTENDS(OPT_POSSESSION_RATE) | S |
| PX02 | Max possessed at once | 1, 3*, 5, 10, Unlimited | 5 | I | EXTENDS(sim_possession_cap) | S |
| PX03 | Corruption spreads to friends | Off, On*, Viral | 3 | I | NEW | S |
| PX04 | Vape-shop corruption | Off, On* | 2 | I | NEW | S |
| PX05 | Nightmares | Off, Rare, Normal* | 3 | I | NEW | S |
| PX06 | Possessed citizens haunt others | Off, On* | 2 | I | NEW | S |
| PX07 | Exorcism difficulty | Easy, Normal*, Hard, Holy Terror | 4 | I | NEW | S |
| PX08 | Exorcism phases | All 3*, Skip rhythm, Skip aim, Skip true name, True name only | 5 | I | NEW | M |
| PX09 | Failures before a soul is lost | 3*, 5, 9, Never | 4 | I | EXTENDS(DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL) | S |
| PX10 | Lay to Rest | Allowed*, Disabled | 2 | I | NEW | S |
| PX11 | Monster meter consequences | Full*, Light | 2 | I | NEW | S |
| PX12 | Ghost rite | Off, On* | 2 | I | NEW | S |
| PX13 | Ghost temperament | Friendly*, Mischievous, Haunty | 3 | I | NEW | S |
| PX14 | Grief | Short, Normal*, Long | 3 | I | NEW | S |
| PX15 | Nobody dies (masks every death path except Monster Mode) | Off*, On | 2 | I | NEW | S |
| PX16 | Possessed portrait intensity | Cute-creepy, Creepy*, Full horror | 3 | G | NEW | M |
| PX17 | Possessor names | Premade only, Neighbours first*, Anyone in Hellvile | 3 | I | EXTENDS(sim_possessor_of) | S |

### 8.8 Doomogachi pet

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| PT01 | Pet clock | Real time, Game time*, Only while watching | 3 | I | NEW | M |
| PT02 | Pet hunger speed | Slow, Normal*, Fast | 3 | I | NEW | S |
| PT03 | Poop frequency | Low, Normal*, High | 3 | I | NEW | S |
| PT04 | Pet sickness | Off, On* | 2 | I | NEW | S |
| PT05 | Pet death (classic tamagotchi) | Off* (runs away, comes back), On | 2 | I | NEW | S |
| PT06 | Evolution line | Free*, Cute, Angel, Feral, Demon | 5 | I | NEW | S |
| PT07 | Discipline calls | Off, On* | 2 | I | NEW | S |
| PT08 | LCD palette | Classic green*, Pastel, Hellvile red, Greyscale, Backlit blue | 5 | G | NEW | S |
| PT09 | LCD pixel size | Chunky*, Fine | 2 | G | NEW | S |
| PT10 | Pet alerts | Off, Badge only, Toast, Toast + beep* | 4 | G | EXTENDS(OPT_PET_NOTIFICATIONS) | S |
| PT11 | Pet follows you in the overworld | Off*, On | 2 | I | NEW | M |
| PT12 | Pet combat buff | Off, On* | 2 | I | NEW | S |

### 8.9 Gore and horror (finer than Cute/Brutal)

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| GR01 | Gore mode | Cute*, Brutal, Per weapon, Per mode, Cute citizens + brutal demons | 5 | I | EXTENDS(OPT_GORE) | S |
| GR02 | Blood colour | Red*, Green, Rainbow, Confetti, Purple, Black ink, Glitter, Ectoplasm, Candy syrup | 9 | I | EXTENDS(OPT_BLOOD_COLOR) | S |
| GR03 | Confetti | None, Some, Lots*, Absurd | 4 | I | EXTENDS(OPT_CONFETTI_AMOUNT) | S |
| GR04 | Gibs | None, Few, Normal*, Lots | 4 | I | NEW | S |
| GR05 | Gib lifetime | 1 s, 10 s*, Until level end | 3 | G | NEW | S |
| GR06 | Wall splats and stickers | Off, Few*, Many | 3 | G | NEW | M |
| GR07 | Demon corpses | Fade*, Stay | 2 | G | NEW | S |
| GR08 | Demon death style | Per gore*, Balloon pop, Plush deflate, Dust poof, Explode | 5 | I | NEW | M |
| GR09 | Bullet impacts | Auto*, Sparks, Stars, Hearts, Puffs | 5 | I | NEW | S |
| GR10 | Explosions | Auto*, Fire, Confetti, Glitter | 4 | I | NEW | S |
| GR11 | Death sounds | Auto*, Squeak, Splat | 3 | I | NEW | S |
| GR12 | Damage screen effect | Off, Cute heart cracks*, Brutal blood | 3 | G | NEW | S |
| GR13 | Kill hit-stop | Off, Short*, Long | 3 | G | NEW | S |
| GR14 | Horror intensity | Soft, Normal*, Scary | 3 | I | NEW | M |
| GR15 | Jump scares | Off, On* | 2 | I | NEW | S |
| GR16 | Invasion palette rot | Off, Partial, Full* | 3 | I | NEW | S |
| GR17 | Demon designs | By chapter*, Always cute, Always grotesque | 3 | I | EXTENDS(OPT_CHEAT_CUTE_DEMONS) | M |
| GR18 | Boss gore | Same as gore*, Always cute | 2 | I | NEW | S |

### 8.10 Shooter: movement

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| MV01 | Auto-sprint | Off*, On, After 1 s | 3 | G | NEW | S |
| MV02 | Sprint | Hold*, Toggle | 2 | G | NEW | S |
| MV03 | Crouch | Hold, Toggle* | 2 | G | NEW | S |
| MV04 | Dodge roll input | Double-tap + button*, Button only, Double-tap only | 3 | G | NEW | S |
| MV05 | Jump | Button*, Auto-jump ledges | 2 | G | NEW | S |
| MV06 | Movement feel | Snappy (classic), Normal*, Floaty | 3 | G | NEW | S |
| MV07 | Turbo strafe-run | Off*, On | 2 | I | NEW | S |
| MV08 | Auto-run (classic scheme) | Off*, On | 2 | G | NEW | S |
| MV09 | Step-up assist | Off, On* | 2 | G | NEW | S |
| MV10 | Use / interact | Button*, Auto on bump | 2 | G | NEW | S |
| MV11 | Auto-pickup weapons | Off, On* | 2 | G | NEW | S |
| MV12 | Knockback on you | Off, Low, Normal* | 3 | I | NEW | S |

### 8.11 Shooter: first-person camera

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| FP01 | Field of view | 60, 70, 75*, 80, 90, 100, 110 (clamped per platform) | 7 | G | EXTENDS(OPT_FOV) | M |
| FP02 | Weapon bob | Off, Low, Full* | 3 | G | NEW | S |
| FP03 | Weapon position | Right*, Centre (classic), Left, Follow handedness (A19) | 4 | G | NEW | M |
| FP04 | Weapon size | Small, Normal*, Big | 3 | G | NEW | M |
| FP05 | Show weapon | Off, On* | 2 | G | NEW | S |
| FP06 | View kick | Off, Low, Full* | 3 | G | NEW | S |
| FP07 | Muzzle-flash light | Off, On* | 2 | G | NEW | S |
| FP08 | Landing dip | Off, On* | 2 | G | NEW | S |
| FP09 | Look-spring (recentre pitch) | Off*, On | 2 | G | NEW | S |
| FP10 | Pitch | Classic (auto-aim only), Limited, Full* | 3 | G | NEW | M |
| FP11 | Hands tinted by fur / gloves | Off, On* | 2 | G | NEW | S |
| FP12 | Head bob | Off, Low, Full*, Classic strong | 4 | G | EXTENDS(OPT_HEAD_BOB) | S |

### 8.12 Shooter: third-person camera

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| TP01 | Distance | Close, Normal*, Far, Very far | 4 | G | NEW | S |
| TP02 | Height | Low, Normal*, High, Top-down-ish | 4 | G | NEW | S |
| TP03 | Shoulder offset | Centred, Small, Normal*, Wide | 4 | G | NEW | S |
| TP04 | Shoulder | Right*, Left, Auto (swaps near walls) | 3 | G | EXTENDS(OPT_SHOULDER) | M |
| TP05 | Follow smoothing | Off, Low, Normal*, High | 4 | G | NEW | S |
| TP06 | Auto-recentre | Off, Slow*, Fast | 3 | G | NEW | S |
| TP07 | Wall pull-in | Instant, Smooth* | 2 | G | NEW | S |
| TP08 | Your citizen when the camera is close | Visible, Dither*, Hide | 3 | G | NEW | S |
| TP09 | Aiming in third person | Over-shoulder zoom*, Switch to first person | 2 | G | NEW | M |
| TP10 | Reticle | Screen centre*, Projected from the weapon | 2 | G | NEW | M |
| TP11 | Turning | Free look*, Tank, Face movement | 3 | G | NEW | M |
| TP12 | Own nameplate | Off*, On | 2 | G | NEW | S |
| TP13 | Default camera per mode (overworld fights, Hell Breach, Arena) | First / Third each | 2 | G | EXTENDS(OPT_CAMERA_DEFAULT) | S |

### 8.13 Shooter: aim and lock-on

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| AI01 | Horizontal look speed | 1..10 (5*) | 10 | G | EXTENDS(OPT_LOOK_SPEED) | S |
| AI02 | Vertical look speed | 1..10 (5*) | 10 | G | EXTENDS(OPT_LOOK_SPEED) | S |
| AI03 | Invert X | Off*, On | 2 | G | EXTENDS(OPT_INVERT_LOOK) | S |
| AI04 | Invert Y (first person) | Off*, On | 2 | G | EXTENDS(OPT_INVERT_LOOK) | S |
| AI05 | Invert Y (third person) | Off*, On | 2 | G | EXTENDS(OPT_INVERT_LOOK) | S |
| AI06 | Look acceleration | Off, Low*, High | 3 | G | NEW | S |
| AI07 | Aim-down-sights sensitivity | 50%, 75%*, 100% | 3 | G | NEW | S |
| AI08 | Aim slowdown (friction) | Off, Low, Medium*, High | 4 | G | EXTENDS(OPT_AIM_ASSIST) | M |
| AI09 | Aim magnetism (pull) | Off, Low*, Medium, High | 4 | G | EXTENDS(OPT_AIM_ASSIST) | M |
| AI10 | Vertical auto-aim (classic) | Off, On* | 2 | G | EXTENDS(OPT_AIM_ASSIST) | S |
| AI11 | Lock-on | Off, Hold, Toggle* | 3 | G | EXTENDS(OPT_LOCK_ON) | S |
| AI12 | Lock-on priority | Nearest*, Closest to crosshair, Lowest HP, Biggest threat, Bosses first | 5 | G | NEW | M |
| AI13 | Switch lock target | D-pad flick*, Cycle button | 2 | G | NEW | S |
| AI14 | Lock break distance | Short, Normal*, Long | 3 | G | NEW | S |
| AI15 | Auto-fire on target | Off*, On | 2 | G | NEW | S |
| AI16 | Snap to target when aiming | Off*, On | 2 | G | NEW | S |
| AI17 | Weak-spot highlight | Off*, On | 2 | G | NEW | S |

### 8.14 Crosshair

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| XH01 | Shape | the 8 today + chevron, T, brackets, triangle, diamond, flower, moon, fang, horns, cat face, bunny, pentagram, classic plus, ghost, bow, music note | 24 | G | EXTENDS(OPT_CROSSHAIR) | S |
| XH02 | Colour | the 6 today → 32 paints + rainbow cycle + auto-contrast | 34 | G | EXTENDS(OPT_CROSSHAIR_COLOR) | S |
| XH03 | Size | Small, Normal*, Big | 3 | G | NEW | S |
| XH04 | Thickness | 1 px*, 2 px | 2 | G | NEW | S |
| XH05 | Gap | 0..3 (1*) | 4 | G | NEW | S |
| XH06 | Outline | Off, Dark*, Light | 3 | G | NEW | S |
| XH07 | Opacity | 100%*, 75%, 50% | 3 | G | NEW | S |
| XH08 | Dynamic spread | Off, On* | 2 | G | NEW | S |
| XH09 | Centre dot | Off*, On | 2 | G | NEW | S |
| XH10 | Hit-confirm flash | Off, White, Pink*, Red | 4 | G | NEW | S |
| XH11 | Colour change over enemies | Off, On* | 2 | G | NEW | S |
| XH12 | Lock-on reticle | Ring, Heart*, Brackets, Skull, Paw | 5 | G | NEW | S |
| XH13 | Scope overlay | Classic, Cute heart lens*, Minimal | 3 | G | NEW | S |
| XH14 | Crosshair in third person | Always*, Only when aiming, Off | 3 | G | NEW | S |
| XH15 | Crosshair animation | Off, On* | 2 | G | NEW | S |

### 8.15 HUD

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| HD01 | Layout | Cute*, Classic, Minimal, Classic status bar, Off | 5 | G | EXTENDS(OPT_HUD_LAYOUT) | M |
| HD02 | Scale | Small, Normal*, Large (PS1) | 3 | G | NEW | M |
| HD03 | Opacity | 100%*, 75%, 50% | 3 | G | NEW | S |
| HD04 | HUD face (your citizen's portrait) | Off, Small, Big* | 3 | G | EXTENDS(combat_hud face) | S |
| HD05 | Health display | Bar, Hearts*, Number, Face only | 4 | G | NEW | S |
| HD06 | Ammo display | Candy pill*, Number, Bar, Off | 4 | G | NEW | S |
| HD07 | Minimap | Off*, Corner fixed north, Corner rotating | 3 | G | NEW | M |
| HD08 | Objective arrow | Off, On* | 2 | G | NEW | S |
| HD09 | Combo meter | Off, On* | 2 | G | NEW | S |
| HD10 | Score pop-ups | Off, On* | 2 | G | NEW | S |
| HD11 | Damage numbers | Off, Numbers*, Hearts, Stars, Words ("OUCH!") | 5 | G | EXTENDS(OPT_DAMAGE_NUMBERS) | S |
| HD12 | Hit marker | Off, Visual, Visual + sound* | 3 | G | EXTENDS(OPT_HIT_MARKERS) | S |
| HD13 | Damage direction | Off, Arrows*, Edge glow | 3 | G | NEW | S |
| HD14 | Low-health warning | Heartbeat + vignette*, Heartbeat, Vignette, Off | 4 | G | NEW | S |
| HD15 | Pickup messages | Full*, Icons, Off | 3 | G | NEW | S |
| HD16 | Weapon switcher | Strip*, Wheel, Name only | 3 | G | NEW | M |
| HD17 | Keys and items | Off, On* | 2 | G | NEW | S |
| HD18 | Level timer | Off*, On | 2 | G | NEW | S |
| HD19 | Boss bar | Big*, Small, Off | 3 | G | NEW | S |
| HD20 | Kill counter | Off*, On | 2 | G | NEW | S |

### 8.16 Weapons and welding

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| WP01 | Auto-switch on pickup | Off, If better*, Always | 3 | G | NEW | S |
| WP02 | Auto-reload | Off, On* | 2 | G | NEW | S |
| WP03 | Skip empty weapons | Off, On* | 2 | G | NEW | S |
| WP04 | Weapon order | By slot*, By class, Favourites first | 3 | G | EXTENDS(WF_FAVORITE) | S |
| WP05 | Welding minigame | Easy, Normal*, Hard, Auto-weld | 4 | I | NEW | S |
| WP06 | Weapon names | Funny generated*, Plain | 2 | G | NEW | S |
| WP07 | Weapon XP rate | x0.5, x1*, x2 | 3 | I | NEW | S |
| WP08 | Charm dangle | Off, On* | 2 | G | NEW | S |
| WP09 | Default style for new welds | Ask*, Cute, Brutal | 3 | G | NEW | S |
| WP10 | Weapon paint shown in first person | Off, On* | 2 | G | NEW | S |

### 8.17 Audio (per bus)

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| AU01 | Master volume | 0..10 (10*) | 11 | G | NEW | S |
| AU02 | UI sound volume | 0..10 | 11 | G | NEW (split from OPT_SFX_VOL) | S |
| AU03 | Combat sound volume | 0..10 | 11 | G | NEW | S |
| AU04 | Ambience volume | 0..10 | 11 | G | NEW | S |
| AU05 | Pet beep volume | 0..10 | 11 | G | NEW | S |
| AU06 | Footstep volume | 0..10 | 11 | G | NEW | S |
| AU07 | Jingle / fanfare volume | 0..10 | 11 | G | NEW | S |
| AU08 | Heartbeat volume | 0..10 | 11 | G | NEW | S |
| AU09 | Output | Stereo*, Mono, Swapped | 3 | G | NEW | S |
| AU10 | GBA speaker EQ | Speaker*, Headphones | 2 | G | NEW (GBA) | S |
| AU11 | PS1 SPU reverb | Off, Room*, Studio, Hall, Space echo | 5 | G | NEW (PS1) | M |
| AU12 | Reverb amount | 0..3 (1*) | 4 | G | NEW (PS1) | S |
| AU13 | Location music | Auto*, Jukebox playlist, Silence | 3 | G | NEW | M |
| AU14 | Invasion music | Metal*, Cute remix, Keep location theme | 3 | G | NEW | M |
| AU15 | Dynamic combat layers | Off, On* | 2 | G | NEW | M |
| AU16 | Babble style | Off, Bleeps*, Syllables, Mumbles, Kazoo | 5 | G | EXTENDS(OPT_VOICE_BABBLE) | M |
| AU17 | Babble pitch offset | -2..+2 (0*) | 5 | G | NEW | S |
| AU18 | Babble speed | Slow, Normal*, Fast | 3 | G | NEW | S |
| AU19 | Text blips (when babble is off) | Off, Soft*, Classic | 3 | G | NEW | S |
| AU20 | Mute in menus | Off*, On | 2 | G | NEW | S |
| AU21 | GBA mixing rate | 10 kHz, 13 kHz*, 18 kHz | 3 | G | NEW (GBA) | M |
| AU22 | Duck music under dialogue | Off, On* | 2 | G | NEW | S |
| AU23 | Squeaky plush sounds | Off, On* | 2 | G | NEW | S |
| AU24 | Combat music starts | Immediately*, On first shot | 2 | G | NEW | S |

### 8.18 Display, filters, LCD and CRT

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| DS01 | LCD filter | Off, Pet screen*, Everything, Grid only, Ghosting | 5 | G | EXTENDS(OPT_LCD_FILTER) | S |
| DS02 | GBA screen profile (gamma / colour correction) | Original GBA (brighten), SP frontlit, SP backlit*, DS/Micro, Game Boy Player, Emulator | 6 | G | NEW (GBA) | S |
| DS03 | Brightness | -2..+2 (0*) | 5 | G | NEW | S |
| DS04 | Contrast | -2..+2 (0*) | 5 | G | NEW | S |
| DS05 | Saturation | Greyscale, Pastel, Normal*, Vivid | 4 | G | NEW | S |
| DS06 | Gamma | 0..4 (2*) | 5 | G | NEW | S |
| DS07 | Camera filter | the 6 today + Polaroid, Pastel dream, Hellvile red, Cyber green, Noir, Thermal, Comic, Watercolour | 14 | G | EXTENDS(OPT_CAMERA_FILTER) | S |
| DS08 | PS1 dithering | Off, On* | 2 | G | NEW (PS1) | S |
| DS09 | PS1 screen X offset (TV centring) | -8..+8 | 17 | G | NEW (PS1) | S |
| DS10 | PS1 screen Y offset | -8..+8 | 17 | G | NEW (PS1) | S |
| DS11 | Overscan safe area | Off*, Small, Large | 3 | G | NEW (PS1) | S |
| DS12 | PS1 video | NTSC 60*, PAL 50 (+speed fix) | 2 | G | NEW (PS1) | M |
| DS13 | PS1 hi-res menus (480i) | Off*, On | 2 | G | NEW (PS1) | L |
| DS14 | CRT scanlines (PS1) | Off*, Light, Heavy | 3 | G | NEW (PS1) | M |
| DS15 | Frame-rate cap | 30*, 20 (steady on GBA 3D), Uncapped | 3 | G | NEW | M |
| DS16 | 3D draw distance | Near, Normal*, Far | 3 | G | NEW | M |
| DS17 | Fog | Off, On* | 2 | G | NEW | S |
| DS18 | Animated sky | Off, On* | 2 | G | NEW | S |
| DS19 | Crowd density (overworld chibis) | Low, Normal*, High | 3 | G | NEW | M |
| DS20 | Portrait animation | Full*, Blink only, Static | 3 | G | NEW | S |
| DS21 | Window skin | the 12 today + Moss, Bone, Candy Corn, Ocean, Sunset, Cloud Nine, Plaid, Stars | 20 | G | EXTENDS(OPT_WINDOW_SKIN) | S |
| DS22 | Menu background | Animated*, Static, Plain colour | 3 | G | NEW | S |
| DS23 | Particles (sparkles, hearts) | Low, Normal*, High | 3 | G | NEW | S |
| DS24 | Weather visuals | Off, Light, Full* | 3 | G | NEW | S |
| DS25 | Palette-cycle effects (glitter, glow) | Off, On* | 2 | G | NEW | S |
| DS26 | Title theme | the 6 today + Plush Parade, Graveyard Picnic, Festival Night, Doomogachi LCD | 10 | G | EXTENDS(OPT_TITLE_THEME) | S |
| DS27 | Cursor | the 6 today + Bone, Candy, Bat, Ghost, Bell, Fish | 12 | G | EXTENDS(OPT_CURSOR) | S |
| DS28 | Font style | Normal, Bold, Shadowed + Rounded, Pixel serif, Spooky | 6 | G | EXTENDS(OPT_FONT_STYLE) | M |

### 8.19 Colour and colour-blind support

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| CB01 | Colour-blind aid | Off*, Protan, Deutan, Tritan, Achromat, High contrast | 6 | G | EXTENDS(OPT_COLORBLIND) | S |
| CB02 | Method | Remap palette (daltonise)*, Filter | 2 | G | NEW | M |
| CB03 | Strength | 0..3 (2*) | 4 | G | NEW | S |
| CB04 | District colour coding | Pink/purple*, Blue/orange, Add shapes (heart / horn icons) | 3 | G | NEW | S |
| CB05 | Enemy outline highlight | Off*, On | 2 | G | NEW | M |
| CB06 | Pickup glint | Off, On* | 2 | G | NEW | S |
| CB07 | Colour names in pickers | Off, On* | 2 | G | NEW | S |
| CB08 | Possessed icon (not just red eyes) | Off, On* | 2 | G | NEW | S |

### 8.20 Accessibility

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| AC01 | Text size | Normal*, Large (2x dialogue) | 2 | G | NEW | M |
| AC02 | Readable font (heavy-bottom pixel font) | Off*, On | 2 | G | NEW | M |
| AC03 | High-contrast windows | Off*, On | 2 | G | NEW | S |
| AC04 | Holds become toggles | Off*, On | 2 | G | NEW | S |
| AC05 | Button-mash assist | Off*, On | 2 | G | NEW | S |
| AC06 | Rhythm timing window (prayer, concert, cooking) | Normal*, Wide, Very wide, Auto-play | 4 | G | NEW | S |
| AC07 | Shooter game speed | 100%*, 85%, 70%, 50% | 4 | G | NEW | M |
| AC08 | Minigames always easy | Off*, On | 2 | G | NEW | S |
| AC09 | Menu repeat delay | Slow, Normal*, Fast | 3 | G | EXTENDS(btn_repeat 12/3 ticks) | S |
| AC10 | Menu wrap-around | Off, On* | 2 | G | NEW | S |
| AC11 | Remember cursor | Off, On* | 2 | G | NEW | S |
| AC12 | Confirm button | A / Cross*, B / Circle | 2 | G | NEW | S |
| AC13 | Sound radar | Off*, On | 2 | G | NEW | M |
| AC14 | Sound captions ("[growl behind you]") | Off*, On | 2 | G | NEW | M |
| AC15 | Menu audio cues | Off*, On | 2 | G | NEW | S |
| AC16 | Reduce motion (idle animations, UI bounce, bobbing) | Off*, On | 2 | G | EXTENDS(OPT_UI_BOUNCE) | S |
| AC17 | Photosensitive mode (no glitch aura, cycling, strobes) | Off*, On | 2 | G | EXTENDS(OPT_REDUCE_FLASH) | S |
| AC18 | Arachnophobia mode (Cloud Spider → Cloud Bunny, spider parts hidden) | Off*, On | 2 | G | NEW | M |
| AC19 | Phobia filters | Off*, Needles, Clowns, Holes (trypophobia), All | 5 | G | NEW | M |
| AC20 | Vape reskin (Cloud Nine Vapor → Cloud Nine Bubble Tea, clouds → bubbles) | Off*, On | 2 | G | NEW | M |
| AC21 | One-handed controls | Off*, Left hand, Right hand | 3 | G | NEW | M |
| AC22 | Full auto-aim | Off*, On | 2 | G | NEW | S |
| AC23 | No-damage assist (not a cheat) | Off*, On | 2 | I | NEW | S |
| AC24 | Skip combat encounters (story) | Off*, On | 2 | I | NEW | M |
| AC25 | No timers | Off*, On | 2 | G | NEW | M |
| AC26 | Navigation assist | Off*, Highlight, Path line | 3 | G | NEW | M |
| AC27 | Long-press time | Short, Normal*, Long | 3 | G | NEW | S |
| AC28 | Double-tap window | Short, Normal*, Long | 3 | G | NEW | S |
| AC29 | GBA chord window (SELECT+A = X) | 2, 4*, 8 frames | 3 | G | NEW (GBA) | S |
| AC30 | Screen shake | Off, Low, Full*, Classic | 4 | G | EXTENDS(OPT_SCREEN_SHAKE) | S |

### 8.21 Text and dialogue

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| TX01 | Auto-advance | Off*, On | 2 | G | NEW | S |
| TX02 | Auto-advance delay | Short, Normal*, Long | 3 | G | NEW | S |
| TX03 | Skip seen text | Off*, On | 2 | G | NEW | S |
| TX04 | Text log (last 16 lines, about 1 KB) | Off, On* | 2 | G | NEW | M |
| TX05 | Box position | Bottom*, Top, Auto | 3 | G | NEW | S |
| TX06 | Box opacity | Solid*, Translucent | 2 | G | NEW | S |
| TX07 | Speaker portrait | Off, On*, Big | 3 | G | NEW | S |
| TX08 | Name labels | Name*, Nickname, Name + pronouns | 3 | G | EXTENDS(creator nicknames) | S |
| TX09 | Pronouns on profiles | Off, On* | 2 | G | NEW | S |
| TX10 | Name filter for custom names | Off*, On | 2 | G | NEW | S |
| TX11 | Number format | 1,000*, 1.000, 1 000 | 3 | G | NEW | S |
| TX12 | Date format | Day-month*, Month-day | 2 | G | NEW | S |
| TX13 | Overworld chatter | Bubbles*, Thought clouds, Icons only, Off | 4 | G | NEW | S |
| TX14 | Letter spacing | Tight, Normal*, Wide | 3 | G | NEW | S |
| TX15 | Coloured names in text | Off, On* | 2 | G | NEW | S |
| TX16 | Text speed | Typewriter-slow, Slow, Normal*, Fast, Instant | 5 | G | EXTENDS(OPT_TEXT_SPEED) | S |
| TX17 | Subtitles | Off, On*, With speaker name | 3 | G | EXTENDS(OPT_SUBTITLES) | S |

### 8.22 Controls: common

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| CT01 | Scheme | Classic, Modern*, Southpaw, Legacy (strafe on hold), One-hand, Custom | 6 | G | EXTENDS(OPT_CONTROL_SCHEME) | M |
| CT02 | Strafe mode | L/R strafe*, Hold L + D-pad, Auto-strafe | 3 | G | NEW | S |
| CT03 | D-pad turn acceleration | Off, Low*, High | 3 | G | NEW | S |
| CT04 | Look up/down with | L + D-pad*, R + D-pad, Auto (pitch auto-aim) | 3 | G | NEW | S |
| CT05 | Camera toggle | SELECT*, Chord, Disabled | 3 | G | NEW | S |
| CT06 | Quick-action wheel (life sim) | Off*, On | 2 | G | NEW | M |
| CT07 | Soft reset (A+B+START+SELECT) | Off, On* | 2 | G | NEW | S |
| CT08 | Pause | START*, Double-tap START | 2 | G | NEW | S |
| CT09 | Turbo fire | Off*, On | 2 | G | NEW | S |
| CT10 | Button prompts | Auto*, GBA letters, PS1 symbols | 3 | G | NEW | S |

### 8.23 Controls: GBA

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| GB01 | Sleep mode (stop CPU and sound; same combo wakes) | Off, L+R+SELECT*, L+R+START | 3 | G | NEW | M |
| GB02 | X / Y in menus | SELECT chords*, L/R act as X/Y | 2 | G | EXTENDS(DOOMO_INPUT_SYNTH_XY) | S |
| GB03 | Swap L and R | Off*, On | 2 | G | NEW | S |
| GB04 | L shift layer (L + face button = extra actions) | Off*, On | 2 | G | NEW | M |
| GB05 | Game Boy Player rumble | Off*, On (when detected) | 2 | G | NEW | L |
| GB06 | Save reminders (no clock on the cartridge) | Off, On* | 2 | G | NEW | S |

### 8.24 Controls: PS1

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| PS01 | Vibration (DualShock) | Off, Low, High* | 3 | G | NEW | M |
| PS02 | Analog sticks | Off*, Move + look, Swapped | 3 | G | NEW | L |
| PS03 | Stick deadzone | 0..5 (2*) | 6 | G | NEW | S |
| PS04 | Stick curve | Linear, Exponential* | 2 | G | NEW | S |
| PS05 | L2 / R2 | Look up/down*, Lock-on + roll, Weapon swap, Camera | 4 | G | NEW | S |
| PS06 | PS1 Mouse aiming | Off*, Port 1, Port 2 | 3 | G | NEW | L |
| PS07 | Memory card slot | Slot 1*, Slot 2, Ask | 3 | G | NEW | S |
| PS08 | Second-controller assist (player 2 aims holy water, helps in minigames) | Off*, On | 2 | G | NEW | L |
| PS09 | Light gun in the Arena shooting gallery | Off*, On | 2 | G | NEW | L |

### 8.25 Button remapping

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| RM01 | Shooter actions (fire, ADS, jump, crouch, sprint, roll, use, reload, next/prev weapon, camera, lock-on, taunt, map, pause) | any button (4 bits each, about 8 B) | 14 | G | NEW | M |
| RM02 | Overworld actions (interact, run, menu, pet, map, wheel, camera, emote) | any button (about 4 B) | 14 | G | NEW | M |
| RM03 | Menu actions (confirm, back, tab L/R, page, sort) | any button (about 3 B) | 14 | G | NEW | M |
| RM04 | Remap profiles | 3 custom per platform + Reset | 4 | G | NEW | S |
| RM05 | Conflict warnings | Off, On* | 2 | G | NEW | S |

### 8.26 Cheats (each gated by a `UL_CAT_CHEAT` unlock, like today's 12)

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| CH01 | Infinite ammo | Off*, On | 2 | I | NEW | S |
| CH02 | No reload | Off*, On | 2 | I | NEW | S |
| CH03 | Boop of Doom (one-hit kills) | Off*, On | 2 | I | NEW | S |
| CH04 | Rapid fire | Off*, On | 2 | I | NEW | S |
| CH05 | Bullet time on demand | Off*, On | 2 | I | NEW | M |
| CH06 | Moon jump | Off*, On | 2 | I | NEW | S |
| CH07 | Ghost walk (overworld noclip) | Off*, On | 2 | I | NEW | S |
| CH08 | Tiny heads | Off*, On | 2 | I | EXTENDS(OPT_CHEAT_BIG_HEAD) | S |
| CH09 | Citizen size | Pocket, Normal*, Giant | 3 | I | NEW | M |
| CH10 | Furry Island (every citizen drawn as a matching critter species; saves untouched) | Off*, On | 2 | I | NEW | M |
| CH11 | Mirror world | Off*, On | 2 | I | NEW | M |
| CH12 | Party hats on demons | Off*, On | 2 | I | NEW | S |
| CH13 | Hugging demons (pacifist) | Off*, On | 2 | I | NEW | M |
| CH14 | Possession party (cosmetic only) | Off*, On | 2 | I | NEW | S |
| CH15 | No corruption | Off*, On | 2 | I | NEW | S |
| CH16 | Instant pet evolution | Off*, On | 2 | I | NEW | S |
| CH17 | Eternal time of day | Off*, Day, Night | 3 | I | NEW | S |
| CH18 | Eternal festival | Off*, On | 2 | I | NEW | M |
| CH19 | Rainbow palette cycling | Off*, On | 2 | I | NEW | S |
| CH20 | 1-bit mode | Off*, On | 2 | I | NEW | S |
| CH21 | Voice pitch | Chipmunk, Normal*, Deep | 3 | I | NEW | S |
| CH22 | Squeak everything | Off*, On | 2 | I | NEW | S |
| CH23 | Big ears and tails | Off*, On | 2 | I | NEW | S |
| CH24 | Face shuffle (random face parts each day, cosmetic) | Off*, On | 2 | I | NEW | M |
| CH25 | Free shop | Off*, On | 2 | I | EXTENDS(OPT_CHEAT_RICH) | S |
| CH26 | Turbo babies | Off*, On | 2 | I | NEW | S |
| CH27 | Retro '93 palette (brown and grey) | Off*, On | 2 | I | NEW | S |
| CH28 | Plush mode (every material is plush with seams) | Off*, On | 2 | I | EXTENDS(OPT_CHEAT_PAPER_MODE) | S |
| CH29 | uwu-speak dialogue filter | Off*, On | 2 | I | NEW | S |
| CH30 | Barrel party | Off*, On | 2 | I | NEW | M |
| CH31 | Plushie projectiles | Off*, On | 2 | I | NEW | M |
| CH32 | Chunky pixels (pixel-double) | Off*, On | 2 | I | NEW | S |

### 8.27 Modifiers (challenge and fun, unlike cheats they can count toward ranks)

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| MD01 | Damage taken | x0.5, x1*, x2, x4 | 4 | I | NEW | S |
| MD02 | Demon HP | x0.5, x1*, x2 | 3 | I | NEW | S |
| MD03 | Ammo pickups | x0.5, x1*, x2 | 3 | I | NEW | S |
| MD04 | Iron Plush (one life per story save) | Off*, On | 2 | I | NEW | M |
| MD05 | No-HUD challenge | Off*, On | 2 | I | NEW | S |
| MD06 | Melee only | Off*, On | 2 | I | NEW | S |
| MD07 | Weapon style lock | Off*, Cute only, Brutal only | 3 | I | EXTENDS(levels arena "Cute/Brutal-only") | S |
| MD08 | Demon speed | x0.75, x1*, x1.25, x1.5 | 4 | I | NEW | S |
| MD09 | Fast, respawning demons | Off*, On | 2 | I | NEW | M |
| MD10 | Pistol start | Off*, On | 2 | I | NEW | S |
| MD11 | Needs keep ticking in combat | Off*, On | 2 | I | NEW | M |
| MD12 | Pet must survive | Off*, On | 2 | I | NEW | M |
| MD13 | Pacifist tracker (exorcise instead of kill) | Off*, On | 2 | I | NEW | M |
| MD14 | Random weapon each wave | Off*, On | 2 | I | NEW | S |
| MD15 | Mirrored levels | Off*, On | 2 | I | NEW | M |

### 8.28 Per-mode tweaks

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| PM01 | Arena: start wave | 1*, 5, 10 | 3 | I | NEW | S |
| PM02 | Arena: demon mix | All*, Cute only, Grotesque only, Single type | 4 | I | NEW | S |
| PM03 | Arena: wave timer | Off*, On | 2 | I | NEW | S |
| PM04 | Arena: maps | Rotate*, Fixed | 2 | I | NEW | S |
| PM05 | Boss Rush: healing between bosses | Full*, Partial, None | 3 | I | NEW | S |
| PM06 | Horde: shop between waves | Off, On* | 2 | I | NEW | S |
| PM07 | Exorcism: phase order | Fixed*, Random | 2 | I | NEW | S |
| PM08 | Exorcism: holy-water aim assist | Off, On* | 2 | I | NEW | S |
| PM09 | Exorcism: true-name clues | 2, 3*, 4 | 3 | I | NEW | S |
| PM10 | Welding: heat speed | Slow, Normal*, Fast | 3 | I | NEW | S |
| PM11 | Fishing: bite window | Easy, Normal*, Hard | 3 | I | NEW | S |
| PM12 | Fishing: line-tension assist | Off*, On | 2 | I | NEW | S |
| PM13 | Cooking: timer speed | Slow, Normal*, Fast | 3 | I | NEW | S |
| PM14 | Cooking: granny judge strictness | Soft, Normal*, Strict | 3 | I | NEW | S |
| PM15 | Concert: note speed | Slow, Normal*, Fast | 3 | I | NEW | S |
| PM16 | Concert: lyrics | Off, On*, Clean only | 3 | I | NEW | S |
| PM17 | Fun Fair difficulty | Easy, Normal*, Hard | 3 | I | NEW | S |
| PM18 | Tamagotchi-only mode speed | Real time*, x2, x4 | 3 | I | EXTENDS(MODE_TAMA_ONLY) | S |
| PM19 | Dreams | Watch*, Skippable, Off | 3 | I | NEW | S |
| PM20 | Studio metronome | Off, On* | 2 | G | NEW | S |
| PM21 | Studio default tempo | 80, 100, 120*, 140, 160 | 5 | G | NEW | S |
| PM22 | Graveyard flowers wilt | Off, On* | 2 | I | EXTENDS(FPLY graveyard flowers) | S |

### 8.29 Creator preferences

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| CR01 | Creator mode | Simple, Advanced*, Everything | 3 | G | NEW (T01) | M |
| CR02 | Preview stage | Room*, Plain, Hillvile, Hellvile, Sky | 5 | G | NEW | S |
| CR03 | Turntable auto-spin when idle | Off*, On | 2 | G | NEW | S |
| CR04 | Preview cycles expressions | Off, On* | 2 | G | NEW | S |
| CR05 | Locked options | Show silhouettes*, Hide | 2 | G | NEW | S |
| CR06 | Default randomiser theme | Cute*, Spooky, Goth, Pastel, Punk, Royal, Furry, Demon, Angelic, Chaos | 10 | G | NEW (T04) | S |
| CR07 | Confirm discard | Off, On* | 2 | G | NEW | S |
| CR08 | Swatch grid | 8 columns*, 11, 16 | 3 | G | NEW | S |
| CR09 | Show deluxe capacity | Off, On* | 2 | G | NEW (T23) | S |

### 8.30 Photo mode

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| PH01 | Hide HUD in photos | Off, On* | 2 | G | NEW | S |
| PH02 | Frame | None*, Polaroid, Film strip, Hearts, Bones, LCD | 6 | G | NEW | S |
| PH03 | Sticker set | Cute*, Spooky, Both | 3 | G | NEW | S |
| PH04 | Self-timer | Off*, 3 s, 10 s | 3 | G | NEW | S |

### 8.31 System

| ID | Option | Values | n | Scope | Status | Diff |
|---|---|---|---|---|---|---|
| SY01 | Settings scope | Global*, Per island | 2 | G | NEW (E4) | M |
| SY02 | Verify after saving | Off, On* | 2 | G | EXTENDS(save_verify) | S |
| SY03 | Backup save copy (if space) | Off*, On | 2 | G | NEW | M |
| SY04 | Show FPS and RAM (developer) | Off*, On | 2 | G | NEW | S |
| SY05 | "Reset this page" on every options page | action | 1 | G | NEW | S |

---

## 9. Bit budget and the proposed resident layout

### 9.1 Core LookX v1: exactly 192 bits (24 B), every `C`-tier item

| Group | Fields (bits) | Bits |
|---|---|---|
| Eyes | pupil F08 (5), iris F10 (4), heterochromia F13 (3), right-eye colour F14 (6), sclera F15 (4), shine F16 (3), lashes F18 (4), lid droop F21 (3) | 32 |
| Brows, nose, muzzle | brow colour F30 (6), brow thickness F31 (3), nose colour F36 (6), muzzle F39 (4), muzzle length F40 (3) | 22 |
| Mouth, cheeks | teeth F53 (4), tongue F56 (3), blush style F58 (4), blush colour F59 (6) | 17 |
| Ears | size E02 (3), fold E04 (3), inner colour E05 (6) | 12 |
| Proportions | shoulders B02 (3), belly B06 (3), arm length B07 (3), leg length B09 (3) | 12 |
| Fur and material | material B18 (5), fluff B19 (3), fur colour 3 B25 (6), fur pattern B26 (5) | 19 |
| Limbs, tail, wings | hands B47 (4), feet B48 (4), legs B49 (3), tail count B53 (3), tail length B54 (3), tail tip B56 (6), wing colour B61 (6) | 29 |
| Outfit | glasses F95 (5), outer layer O04 (5) + colour O05 (6), legwear O07 (4) + colour O08 (6), hat tilt O26 (3), fit O59 (2), hood up O50 (1) | 32 |
| Identity and motion | idle A01 (4), walk A03 (4), run A04 (4), handedness A19 (1), **age look B86 (3)** | 16 |
| Spare | | 1 |
| **Total** | | **192** |

### 9.2 Resident record options (E1)

| Layout | Look | Core LookX | Name | Stats + hot fields (mood, activity) | Packed tail | Total | x1001 vs today |
|---|---|---|---|---|---|---|---|
| Today | 48 B unpacked | none | 12 | 8 + 2 | 26 | **96 B** | baseline (about 96 KB) |
| **A: same size, rich** | 31 B packed | **24 B** | 12 | 10 | 16 | **93 B** (3 spare) | **±0 KB**, +192 bits per citizen |
| **B: memory relief** | 31 B packed | 12 B (top 96 bits: pupil, sclera, hetero + R colour, lashes, brow colour, nose colour, muzzle, blush colour, fur pattern, fur 3, hands, feet, ear size, tail count, glasses, outer + colour, legwear, walk, age) | 12 | 10 | 16 | **81 B** | **saves about 15 KB of EWRAM** |
| C: naive (no packing) | 48 B | 24 B | 12 | 10 | 26 | 120 B | +24 KB (not viable on the GBA) |

Deluxe overrides (E3) are separate: GBA 4 KB pool, PS1 32 KB. A save-format version bump is needed. `resident_fingerprint` parity tests move to the unpacked view.

### 9.3 Options storage (E4)

* 469 option rows, of which 44 existing `OPT_*` ids are kept and extended, so there are **about 425 new option slots**.
* One byte each = about 470 B. Bit-packed = **105 B** (834 bits).
* 276 are **global**: stored once per cartridge or memory card, not per island. 193 are **island rules**: an `OPTX` chunk next to the existing `opt[64]`.
* On the PS1, the option name and value strings (about 9 KB) belong in the options overlay (E10).

---

## 10. Unlock ideas tied to customisation

Most new options start **unlocked**, as the existing design says ("the creator is huge from minute one"). Signature or silly items are earned. Hooks use existing `PROG_*` counters where possible. "E5" marks a new counter that needs the second bank or counter chunk.

| # | Unlocks | How | Hook |
|---|---|---|---|
| U01 | Heterochromia (F13) | befriend a citizen across the border | PROG_CROSS_BORDER_FRIENDS >= 1 |
| U02 | Black demon sclera (F15) | win 3 exorcisms | PROG_EXORCISMS_WON >= 3 |
| U03 | Heart pupils (F08) | the first marriage on the island | PROG_MARRIAGES >= 1 |
| U04 | Star pupils | release a hit single (alias route, like Sparkle Eyes) | PROG_HIT_SINGLES |
| U05 | Spiral pupils + hypno-swirl glasses | 5 dreams or nightmares | dream journal bits (concert range) |
| U06 | Skull pupils | 100 brutal kills | PROG_DEMONS_KILLED_BRUTAL >= 100 |
| U07 | Paw-print pupils + heart toe beans (B31) | raise the Doomogachi to adult | PROG_PET_EVOLUTIONS >= 4 |
| U08 | Pentagram pupils + pentagram crosshair | reach Hellvile in the story (Ch.4) | story flag |
| U09 | Music-note pupils + headbang dance (A08) | put on 5 concerts | PROG_CONCERTS >= 5 |
| U10 | Clover pupils | win on the Wheel of Fate | Fun Fair hook |
| U11 | Keyhole pupils | find 10 secrets | PROG_SECRETS_FOUND >= 10 |
| U12 | Bullseye pupils + scouter (F95) | 100 headshots | PROG_HEADSHOTS >= 100 |
| U13 | Eye glow (F17) | defeat Lord Vapula | story flag |
| U14 | Galaxy iris + galaxy-speckle fur | live 30 days | PROG_DAYS_LIVED >= 30 |
| U15 | Heart ahoge (H03) | help a confession succeed | life hook |
| U16 | Split dye (H07) | copy one look onto a new citizen ("twins") | creator event |
| U17 | Rainbow dye + rainbow aura | reach the united-park festival | story end |
| U18 | Calico pattern | 3 babies on the island | PROG_BABIES >= 3 |
| U19 | Tuxedo pattern + tie + tux jacket | 3 weddings | PROG_MARRIAGES >= 3 |
| U20 | Merle and brindle | 8 pet evolutions | PROG_PET_EVOLUTIONS >= 8 |
| U21 | Dalmatian | create 101 citizens | PROG_CITIZENS_CREATED >= 101 |
| U22 | Leopard + luchador mask | arena wave 20 | PROG_ARENA_WAVES >= 20 |
| U23 | Tiger stripes | 500 demons | PROG_DEMONS_KILLED >= 500 |
| U24 | Panda pattern | serve 10 meals | PROG_MEALS_SERVED >= 10 |
| U25 | Countershade + flipper feet | catch 25 fish | PROG_FISH_CAUGHT >= 25 |
| U26 | Knit/yarn material + yarn-and-needles prop | give 25 gifts | PROG_GIFTS_GIVEN >= 25 |
| U27 | Porcelain material + crack seams | raise a ghost | PROG_GHOSTS_RAISED >= 1 |
| U28 | Crystal material + crystal horns | find 25 secrets | PROG_SECRETS_FOUND >= 25 |
| U29 | Slime translucency (B22) | 50 Slime Belcher kills | per-demon counter (E5) |
| U30 | Stone (gargoyle) material | 20 Gargoyle kills | per-demon counter (E5) |
| U31 | Glitter material + sequins finish | use Confetti Everything once | cheat hook |
| U32 | Gummy candy material | serve 30 meals | PROG_MEALS_SERVED >= 30 |
| U33 | Cloud/vapour material + cloud tail | clear Cloud Nine Spire | PROG_BREACH_RANKS |
| U34 | Shadow material | use Monster Mode once (a shame unlock with a sad toast) | monster meter |
| U35 | Metal plating + robot prosthetic | weld 25 weapons | PROG_WEAPONS_WELDED >= 25 |
| U36 | Kitsune 9 tails | 9 exorcisms in a row without a failure | exorcism streak (E5) |
| U37 | Lava-crack glow markings | S-rank Hell Breach 2 | PROG_BREACH_RANKS |
| U38 | Extra arms | weld a quality-100 weapon | weld hook |
| U39 | Buddy head | befriend 3 hellhound citizens | life hook |
| U40 | Palm eyes | lose a soul (3 failed exorcisms) | PROG_EXORCISMS_FAILED >= 3 |
| U41 | Belly mouth | serve 100 meals | PROG_MEALS_SERVED >= 100 |
| U42 | Seraph wings (2 pairs) + heart halo | 10 exorcisms | PROG_EXORCISMS_WON >= 10 |
| U43 | All piercings | open the **Pierced Pit** in Hellvile (Ch.4 or Hellvile visits mode) | MODE_HELLVILE_VISITS |
| U44 | All makeup | the **Glam Counter** opens in the Hillvile shop on day 3 | PROG_DAYS_LIVED >= 3 |
| U45 | Tattoo designs | the **Ink & Brimstone** parlour in Hellvile; each design is bought with H$ | UL_CAT_ITEM |
| U46 | Welding goggles | weld your first weapon | PROG_WEAPONS_WELDED >= 1 |
| U47 | 3D glasses | watch the credits parade | story end |
| U48 | Bullet belt | clear Hell Breach 1 | story flag |
| U49 | Broken shackle | redeem 10 Hellvile citizens (Ch.4) | story counter |
| U50 | Yellow raincoat + umbrella prop | live through 5 rainy days | weather counter (E5) |
| U51 | Animal kigurumi onesie | adult pet in the CUTE line | pet form |
| U52 | Animal-ear hood (O51) | create 10 critter-folk citizens | creator counter (E5) |
| U53 | Plague-doctor mask | heal 20 citizens | life counter (E5) |
| U54 | Hockey mask | play 10 arena runs on Brutal gore | arena hook |
| U55 | Kitsune mask | attend a festival | IS10 festival |
| U56 | Coffin backpack + coffin sleep pose | perform the ghost rite | PROG_GHOSTS_RAISED >= 1 |
| U57 | Horn safety caps | 5 Hillvile friends with horns | life hook |
| U58 | Bell collar | pet care score 500 | pet hook |
| U59 | Sweater paws (O48) | 50 friendships | PROG_FRIENDSHIPS >= 50 |
| U60 | Light-up sneakers | any Fun Fair high score | PROG_HS_* |
| U61 | All-fours gallop run (A04) | sprint 10 km | combat counter (E5) |
| U62 | Moonwalk | 10 concerts | PROG_CONCERTS >= 10 |
| U63 | Cat-loaf sit + curled sleep | 7 days of perfect pet care | pet hook |
| U64 | Upside-down sleep | create a batling | creator hook |
| U65 | Helicopter tail wag | friendship level 99 with anyone | resident `level` |
| U66 | Nose-boop greeting | introduce 20 pairs | life counter (E5) |
| U67 | Finger-guns victory pose | S-rank any Hell Breach | PROG_BREACH_RANKS |
| U68 | Demonic double voice (A34) | visit Hellvile 5 times | Hellvile visits hook |
| U69 | Kazoo babble | write a song in the Studio | studio hook |
| U70 | **Island dyes** (E7) | 20 makeovers | creator counter (E5) |
| U71 | +32 deluxe slots on the GBA | the "Wardrobe Wing" of the Grand Library | library hook |
| U72 | **Doomo-Gacha capsule machine** | at the Fun Fair. Spend **candy (never real money)** for a random cosmetic, with duplicate protection and a pity counter | UL_CAT_MISC (minigames) |
| U73 | Crosshairs: cat face / bunny / pentagram | 100 cute kills / 3 babies / story clear | PROG_DEMONS_KILLED_CUTE, PROG_BABIES, story |
| U74 | Camera filters: Polaroid / Noir / Thermal | 20 photos / 10 secrets / arena wave 10 | E5, PROG_SECRETS_FOUND, PROG_ARENA_WAVES |
| U75 | LCD palettes: Hellvile red / backlit blue | DEMON pet line / ANGEL pet line | pet form |
| U76 | Window skins: Bone / Candy Corn / Cloud Nine | 10 graves / 31 days lived / Spire clear | grave_count, PROG_DAYS_LIVED, PROG_BREACH_RANKS |
| U77 | Blood colours: Glitter / Ectoplasm / Candy syrup | Confetti cheat / raise a ghost / collect 50 candy | cheat, PROG_GHOSTS_RAISED, E5 |
| U78 | Cheats CH01-CH32 | one per milestone (S-ranks, 1000 kills, 100 days, all pet forms, 500 citizens...) | UL_CAT_CHEAT |

**New places that sell or unlock looks** (all optional): **Fluff & Fangs Salon** (Hillvile: fur, dye, grooming), **Glam Counter** (makeup), **Pierced Pit** and **Ink & Brimstone** (Hellvile: piercings, tattoos), **Thread & Brim** (tailor: layers, fits, per-piece colours, island dyes), the **Doomo-Gacha** (Fun Fair).

---

## 11. Roadmap

| Wave | Contents | Why first | Size |
|---|---|---|---|
| **0: enablers** | E4 options v2 (global/island split, `OPTX`, no 64 cap). E9 "0 = auto" rule. E5 unlock bank 2 | Nothing new fits without them, and they are small | S-M |
| **1: free variety** | All 29 `A`-tier appends: eye shapes +16, pupils-as-styles, hair +32, ears +16, tails +16, tops +26, hats +24, patterns +24, aura +12, expressions +12. Then about 360 `S` options that are pure data: crosshair, HUD, audio buses, display palette filters, needs decay, romance/gore/possession sliders, cheats | No per-resident RAM, and immediately "a shitton of options" | S-M |
| **2: core looks** | E1 packing + E2 Core LookX (all 48 `C` items): pupils, sclera, heterochromia, lashes, brow and nose colour, muzzle, blush, teeth, tongue, fur pattern + 3rd colour, material, fluff, paws, hooves, digitigrade legs, tail count, wing colour, glasses slot, outer layer, legwear, walk/run/idle, handedness, age look. E6 PS1 face texture | The single biggest visible jump in face and body customisation, at zero net RAM | L |
| **3: deluxe** | E3 override triples: makeup, face paint split, tattoos, piercings, scars, per-piece colours and patterns, 16 clothing slots, sleeves, hoods, motion details (emotes, dances, laughs), voice flavour | The long tail of "extremely complex customisation" | L |
| **4: systems** | Island dyes (E7), situational wardrobes (O60), Look Book + Look Password (T08, T09), photo mode, heredity, ageing, new species +24, extra arms, buddy head, analog/mouse/vibration on the PS1, GBP rumble | Big features with platform work | L |

### 11.1 Top 25 picks by value per bit

1. Pupil shape F08. 2. Sclera colour F15. 3. Heterochromia F13+F14. 4. Fur pattern split from face paint B26 + F83. 5. Fur colour 3 B25. 6. Muzzle style F39. 7. Blush style + colour F58/F59. 8. Teeth (buck, gap, snaggle) F53. 9. Tongue blep F56. 10. Glasses slot F95. 11. Brow colour F30. 12. Nose colour F36. 13. Ear size + fold + inner colour E02/E04/E05. 14. Hands / feet / digitigrade B47-B49. 15. Tail count (kitsune) B53. 16. Body material (plush, scales, slime...) B18. 17. Fluffiness B19. 18. Outer layer O04 + legwear O07 (layering). 19. Hood up with ears O50/O51. 20. Walk / run / idle styles A01-A04. 21. Handedness A19. 22. Wing colour B61. 23. Ahoge H03. 24. Split dye H07. 25. Lashes + lid droop F18/F21.

---

## 12. Totals

| | Count |
|---|---|
| **Customisation items** (sections 3-6) | **310**: face 98 (F), hair 14 (H), ears 6 (E), body 88 (B), outfit 64 (O), animation/voice 40 (A) |
| of which NEW / EXTENDS | 221 NEW / 89 EXTENDS(existing field) |
| by tier | 29 append-only (0 RAM), 48 core (191 bits + 1 spare = 24 B), 233 deluxe (930 bits dense, sparse triples) |
| by difficulty | 193 S, 107 M, 10 L |
| Creator tools (section 7) | 23 |
| **Options** (section 8) | **469** on 31 pages: 396 NEW, 73 EXTENDS (44 existing `OPT_*` ids kept) |
| options by scope | 276 global, 193 island |
| options by difficulty | 360 S, 100 M, 9 L |
| Option storage | about 470 B one byte each, or 105 B bit-packed |
| Unlock ideas (section 10) | 78 + 5 new shops/places |
| Enablers (section 2) | 10 |

*Written read-only against the repo. Nothing in the repo was changed.*
