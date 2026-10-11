# DOOMOGACHI customisation v2: the packed-gene design (angle A)

> Ask: *"make sure you add a shitton of options, and face/body customization"* (earlier: "immense
> customization", "extremely complex customization options", "an immense amount of options for
> everything and anything"), and *"still work on the PS one too"*.
>
> Angle A: **`DoomoResident` stays exactly 96 bytes in RAM.** The stored look becomes a bit-packed
> 69-byte **gene** that holds the old look, the old identity tail and 57 new look fields. A wide
> unpacked working struct exists only on the stack and in the creator. A sparse **deluxe tier** adds
> 238 more fields. For premade and generated citizens it costs no RAM, because its values are
> recomputed from a seed. Only player edits are stored, in a small heap.
>
> This is a design written read-only against HEAD `43af439` (2026-10-11). It came after the overlay
> and EWRAM pass: GBA EWRAM has 7.5 KB free and PS1 has 139 KB below the stack top
> (`docs/GBA.md` §3, `docs/PS1.md` §3). Every number marked *calc* comes from
> `scratchpad/custom/gen/schema.py` or `options.py`. Every number marked *measured* comes from the
> repo's docs, logs or linker map. Nothing in the repo was changed.

---

## 0. Summary

| | today | this design |
|---|---|---|
| `DoomoResident` in RAM | 96 B (48 B look + 48 B rest) | **96 B** (69 B gene + 27 B plain hot fields), static-asserted |
| per-citizen look/identity bits stored | 232 + 100 save bits in 69 B of bytes | **552 bits**, all used: legacy look 233 (with append headroom), **new look 211**, identity 108 |
| new per-citizen customisation fields | - | **57 in-record (R tier)** + **238 deluxe (D tier)** + 3 new identity fields |
| catalog values (existing lists) | 349 | **~647** (+298 appended inside the widened fields, A tier) |
| distinct choices offered | ~350 parts + sliders | ~647 catalog parts + 856 R-tier values + **~3,680 deluxe values** *calc* |
| options | 57 (8 wired to nothing) | **285 rows on 23 pages**: 237 new, 30 extended, 8 dead ones wired, 10 kept |
| button remapping | none | 3 custom profiles x 3 input contexts + presets (one-hand, southpaw) |
| GBA EWRAM net | 7.5 KB free | **-20 B net** (reclaims pay for everything) |
| PS1 RAM net | 139 KB below stack top | **about +24.5 KB more headroom** (≈163 KB) with the CD roster (§2.3), or about 91 KB without it. Both stay above the 64 KB floor |
| save format | v1, strict equality, no migration | **v2 + a kept v1 reader**. The gene is saved as raw bytes, so future fields in spare bits need no format change |
| worst-case save (1000 random custom + 2048 edges) | 59,148 B measured | **≈86,648 B** *calc* (3.4 KB under the 88 KB slot 1) |
| PS1 third person | `model.c` reads 12 of 48 fields | the portrait-rendered **face texture** shows every face option. `model.c` geometry covers body, fur, tail, wings, outfit layers, gait and handedness |

How it fits:

1. **Bit packing.** The 48 look bytes and the 21 identity-tail bytes (552 bits) carry only 310
   bits of information (210 + 100). Packing frees 242 bits: 23 widen catalog lists, 8 hold new
   identity fields, and **211 hold new look fields**. The record stays 96 B.
2. **Deluxe base from a seed.** A deluxe value is *heap override* if present, else
   `gen_deluxe(seed, slot, look)` for citizens that came with the island, else 0 (auto). So all 1000
   premade and random citizens wear makeup, piercings, scars, tattoos, layered outfits, emotes and
   so on with **zero RAM**. Only player edits cost heap bytes.
3. **Linked colours.** New coloured parts store a 4-bit reference into a 16-slot citizen palette
   (fur 1-3, hair, highlight, both eyes, horns, outfit colours, 2 accents) instead of a 5-6 bit
   paint. Brows can follow the hair, blush can follow accent A, and so on. This roughly halves the
   cost of colour.
4. **Reclaimed RAM.**
   - The sim's per-resident job cache `sim_s.job[1000]` becomes a 4-bit gene field.
   - The creator's fixed 64-entry nickname table is folded into the new heap.
   - On PS1, the roster records move to the CD, and bios and job texts are BPE-compressed.
5. **Part scripts.** About 300 appended catalog parts and most deluxe decals are drawn by a
   tiny bytecode VM, not as compiled C. On PS1, code is RAM: about 45 B per part instead of about
   250 B of MIPS code.

---

## 1. Data layout

### 1.1 `DoomoResident` v2 (exactly 96 B)

```c
/* resident.h (v2) */
#define DOOMO_GENE_BYTES 69
#define DOOMO_GENE_BITS  (DOOMO_GENE_BYTES * 8)          /* 552 */

typedef struct DoomoResident {
    uint8_t  gene[DOOMO_GENE_BYTES]; /*  0..68  packed look + identity (gene.h)   */
    char     name[DOOMO_NAME_LEN];   /* 69..80  unchanged: 11 chars + NUL         */
    uint8_t  stat[STAT_COUNT];       /* 81..88  0..100, mutated every sim tick    */
    uint8_t  status;                 /* 89      RES_*                             */
    uint8_t  flags;                  /* 90      RF_*  (all 8 bits used)           */
    uint8_t  level;                  /* 91      friendship with the player 0..99  */
    uint8_t  mood;                   /* 92      MOOD_*                            */
    uint8_t  activity;               /* 93      ACT_*                             */
    uint8_t  exorcism;               /* 94      hi nibble tries, lo nibble fails  */
    uint8_t  death_cause;            /* 95      DEATH_*                           */
} DoomoResident;
DOOMO_STATIC_ASSERT(sizeof(DoomoResident) == 96, resident_is_96_bytes);
DOOMO_STATIC_ASSERT(offsetof(DoomoResident, name) == DOOMO_GENE_BYTES, gene_first);
```

**Plain or packed.** The 27 plain bytes are the fields that are hot or written often:

| field | references (grep) |
|---|---:|
| `status` | 379 |
| `flags` | 383 |
| `stat` | 170 |
| `name` | 318 |
| `activity` | 49 |
| `mood` | 33 |
| `level` | 89 |

These sites keep compiling unchanged. Everything that is written rarely and read through a
pointer moves into the gene:

| moved into the gene | references (grep) |
|---|---:|
| the 48 look fields | 136 direct sites outside the renderer, in 17 files |
| `persona` | 55 |
| `like_*` and `hate_food` | about 136 |
| `fear` | 38 |
| `voice` | 19 |
| `catchphrase` | 18 |
| `pronoun` | 10 |
| `romance` | 10 |
| `bday_*` | 18 |

These are upper bounds that include false positives. The plain members are **removed**, not
renamed, so every site that is missed is a compile error, not a silent bug.

The gene is a byte array with no endianness and no padding. Its bit layout is LSB-first, the
same as the save bit stream.

### 1.2 The codec (`gene.h`, `gene.c`, about 2 KB code)

The schema has a single source of truth, `core/data/schema/gene.def`. It is an X-macro list with
**explicit, frozen bit positions**, because the gene is saved as raw bytes (§3) and its positions
are the format:

```c
/*  GF(NAME,            pos, bits, max, def, xoff,                      kind)      */
GF(SPECIES,              0,   6,  SPECIES_COUNT-1, 0, LX(species),       GK_ENUM)
...
GF(PUPIL,              233,   5,  31, 0,  LX(pupil),                     GK_ENUM)
GF(BROW_CREF,          266,   4,  15, 0,  LX(brow_cref),                 GK_CREF)
...
GF(PERSONA0,           469,   4,  15, 7,  IX(persona[0]),                GK_IDENT)
...
GF(BASE_LOOK,          551,   1,   1, 0,  IX(base_look),                 GK_IDENT)
```

`tools/schema_build.py` reads the same file and writes `core/gen/gene_layout.c`, the const field
table. `tools/roster_build.py` and `tools/parts_build.py` parse it too, and CI `check-gen` checks
the result. There is one field table, which the C code, the roster tool, the save codec and the
tests all use.

```c
typedef struct DoomoGeneField {     /* 8 B, const (ROM / resident on PS1)        */
    uint16_t pos;                   /* first bit, LSB-first in gene[]             */
    uint8_t  bits;                  /* 1..8: a field never straddles more than 2 B */
    uint8_t  max;                   /* largest valid value                        */
    uint8_t  def;                   /* sanitize replaces out-of-range values      */
    uint8_t  xoff;                  /* offsetof() in DoomoLook or DoomoIdent      */
    uint8_t  kind;                  /* GK_ENUM GK_SLIDER GK_PAINT GK_CREF GK_SKIN
                                       GK_FLAGS GK_IDENT GK_RESERVED              */
    uint8_t  flags;                 /* GFF_LOOK GFF_SPECIES_BASE GFF_INHERIT ...   */
} DoomoGeneField;
extern const DoomoGeneField doomo_gene_fields[GF_COUNT];   /* GF_COUNT = 130 */

static inline int gene_get(const uint8_t *g, int f) {
    const DoomoGeneField *d = &doomo_gene_fields[f];
    unsigned p = d->pos, v = g[p >> 3];
    if ((p & 7) + d->bits > 8) v |= (unsigned)g[(p >> 3) + 1] << 8;
    return (int)((v >> (p & 7)) & ((1u << d->bits) - 1));
}
void gene_set(uint8_t *g, int f, int v);       /* value > max -> def                   */
int  gene_sanitize(uint8_t *g);                /* every field + reserved bits -> 0     */
void gene_unpack_look(const uint8_t *g, DoomoLook *l);   /* 106 fields, about 3k cycles GBA */
void gene_pack_look(uint8_t *g, const DoomoLook *l);
void gene_unpack_ident(const uint8_t *g, DoomoIdent *id);
void gene_pack_ident(uint8_t *g, const DoomoIdent *id);
DOOMO_STATIC_ASSERT(GENE_LAST_BIT + 1 == DOOMO_GENE_BITS, gene_is_full);
```

`test_gene.c` checks the table itself:

- no overlaps and no gaps (a coverage bitmap),
- `bits <= 8`,
- `max < 1 << bits`,
- the `def` of every enum is valid,
- the bits of every catalog `*_COUNT` fit (this replaces the `sv_*` asserts in `save.c:57-88`).

**Hot read cost.** `gene_get` is a table load, 1-2 byte loads, a shift and a mask. That is about
15-20 cycles in ARM or THUMB on the GBA. The sim updates `SIM_BATCH` = 32 residents per tick and
reads about 10 identity fields each, which is about 6-8k cycles per tick, or 1.4% of a tick.
`sim_compat()` (`sim.c:198`) unpacks both citizens' identity once per pair, not per field.

### 1.3 Full gene bit table (69 B = 552 bits, *calc*)

Legend:

- **Bold** rows are new. The other rows are today's fields, sometimes with wider headroom.
- "values" shows the planned range. Every new field uses value 0 for *auto*, which means today's
  rendering.
- `voice0..4` and `persona0..4` are 5 fields each (5 and 4 bits).
- Shows-in codes:
  - P = bust portrait 48x56
  - F = creator figure 48x80
  - C = chibi 16x24
  - T = fighter 32x48 (the GBA third person)
  - M = PS1 `model.c` geometry
  - M* = on the PS1 model through the face texture (§4.5)

Totals: legacy look **233** bits (210 needed today, so 23 bits of headroom for the A-tier
appends). New look **211** (57 fields + 2 reserved bits). Identity **108** (100 today + 8 new).
The look section is bits 0..443.

| # | bits | pos | field | values | wishlist | shows in |
|---:|---:|---:|---|---|---|---|
| 0 | 6 | 0 | `species` | 0..47 (24 today, +24 B87) | B87 | P F C T M |
| 1 | 4 | 6 | `body` | 0..15 (10 today, +6 B01) | B01 | F C T M |
| 2 | 4 | 10 | `height` | 0..15 | - | F C T M |
| 3 | 4 | 14 | `build` | 0..15 | - | F C T M |
| 4 | 6 | 18 | `skin` | 0..47 (8 natural + 32 paint; +8 natural fur B23 reserved) | B23 | P F C T M |
| 5 | 5 | 24 | `head_shape` | 0..23 (12 today, +12 F01) | F01 | P F C T M |
| 6 | 4 | 29 | `head_size` | 0..15 | - | P F C T M |
| 7 | 6 | 33 | `eye_style` | 0..39 (24 today, +16 F07) | F07 | P F C T M* |
| 8 | 5 | 39 | `eye_color` | paint 0..31 | - | P F C T M* |
| 9 | 4 | 44 | `eye_size` | 0..15 | - | P F T M* |
| 10 | 4 | 48 | `eye_spacing` | 0..15 | - | P F T M* |
| 11 | 4 | 52 | `eye_height` | 0..15 | - | P F T M* |
| 12 | 4 | 56 | `eye_tilt` | 0..15 | - | P F T M* |
| 13 | 3 | 60 | `eye_count` | 1..6 (1..3 today, F25) | F25 | P F T M* |
| 14 | 5 | 63 | `brow_style` | 0..23 (12 today, +12 F29) | F29 | P F M* |
| 15 | 4 | 68 | `brow_height` | 0..15 | - | P F M* |
| 16 | 5 | 72 | `nose_style` | 0..23 (12 today, +12 F35) | F35 | P F T M* |
| 17 | 4 | 77 | `nose_size` | 0..15 | - | P F T M* |
| 18 | 6 | 81 | `mouth_style` | 0..31 (20 today, +12 F47) | F47 | P F T M* |
| 19 | 4 | 87 | `mouth_size` | 0..15 | - | P F T M* |
| 20 | 2 | 91 | `fangs` | 0..3 | - | P F T M* |
| 21 | 6 | 93 | `hair_style` | 0..63 (32 today, +32 H01) | H01 | P F C T M |
| 22 | 5 | 99 | `hair_color` | paint | - | P F C T M |
| 23 | 6 | 104 | `hair_highlight` | paint 0..31, 32 = none | - | P F C T M |
| 24 | 4 | 110 | `facial_hair` | 0..15 (8 today, +8 H13) | H13 | P F T M* |
| 25 | 5 | 114 | `ears` | 0..31 (16 today, +16 E01) | E01 | P F C T M |
| 26 | 5 | 119 | `horns` | 0..25 (16 today, +10 B66) | B66 | P F C T M |
| 27 | 5 | 124 | `horn_color` | paint | - | P F C T M |
| 28 | 4 | 129 | `horn_size` | 0..15 | - | P F T M |
| 29 | 5 | 133 | `tail` | 0..31 (16 today, +16 B52) | B52 | F C T M |
| 30 | 5 | 138 | `wings` | 0..19 (10 today, +10 B59) | B59 | F C T M |
| 31 | 4 | 143 | `halo` | 0..11 (6 today, +6 B76) | B76 | P F C T M |
| 32 | 6 | 147 | `marking` | 0..39 face paint / legacy fur (23 today, +17 F83) | F83 | P F C T M* |
| 33 | 5 | 153 | `marking_color` | paint (fur colour 2 + face paint colour) | - | P F C T M |
| 34 | 8 | 158 | `cheeks` | 8 CHEEK_* flags | - | P F T M* |
| 35 | 6 | 166 | `top` | 0..49 (24 today, +26 O03) | O03 | P F C T M |
| 36 | 5 | 172 | `top_color` | paint | - | P F C T M |
| 37 | 5 | 177 | `bottom` | 0..23 (12 today, +12 O06) | O06 | F C T M |
| 38 | 5 | 182 | `bottom_color` | paint | - | F C T M |
| 39 | 5 | 187 | `shoes` | 0..23 (12 today, +12 O12) | O12 | F C T M |
| 40 | 5 | 192 | `shoes_color` | paint | - | F C T M |
| 41 | 6 | 197 | `hat` | 0..47 (24 today, +24 O25) | O25 | P F C T M |
| 42 | 5 | 203 | `hat_color` | paint | - | P F C T M |
| 43 | 5 | 208 | `accessory` | 0..19 legacy misc slot (O41) | O41 | P F T |
| 44 | 5 | 213 | `accessory_color` | paint | - | P F T |
| 45 | 5 | 218 | `aura` | 0..19 (8 today, +12 B79) | B79 | P F C T |
| 46 | 5 | 223 | `pattern` | 0..31 (8 today, +24 O45) | O45 | P F C T M |
| 47 | 5 | 228 | `pattern_color` | paint | - | P F C T M |
| 48 | 5 | 233 | **`pupil`** | auto + 31 shapes (round, slit, goat, cross, star, heart, paw, skull, ...) | F08 | P F M* |
| 49 | 4 | 238 | **`iris`** | auto + 15 styles (two-tone, ring, galaxy, glossy anime, crystal, ...) | F10 | P F |
| 50 | 3 | 242 | **`hetero`** | none, full, sectoral, central ring, freckle, split | F13 | P F T M* |
| 51 | 5 | 245 | **`eye_color_r`** | paint (right eye; also palette slot 7) | F14 | P F T M* |
| 52 | 4 | 250 | **`sclera`** | auto + 15 (cream, black demon, goat yellow, void, glowing, ...) | F15 | P F M* |
| 53 | 3 | 254 | **`shine`** | auto, one, three, star, heart, cross, square, none | F16 | P F |
| 54 | 4 | 257 | **`lashes`** | auto + 15 (none, flutter, winged, spiky goth, doll, ...) | F18 | P F M* |
| 55 | 3 | 261 | **`lids`** | lid droop 0..7 (wide awake .. sleepy) | F21 | P F T |
| 56 | 2 | 264 | **`pupil_size`** | auto, small, big, huge | F09 | P F |
| 57 | 4 | 266 | **`brow_cref`** | colour ref (auto = hair) | F30 | P F |
| 58 | 2 | 270 | **`brow_thick`** | thin .. bushy | F31 | P F |
| 59 | 4 | 272 | **`nose_cref`** | colour ref (auto = species) | F36 | P F T C M* |
| 60 | 4 | 276 | **`muzzle`** | auto + 15 (feline puffs, canine long, bear, owl disc, pig, duck bill, ...) | F39 | P F T M |
| 61 | 2 | 280 | **`muzzle_len`** | 0..3 | F40 | P F T |
| 62 | 4 | 282 | **`teeth`** | auto + 15 (buck, gap, snaggle, braces, gold, tusks, shark rows, ...) | F53 | P F |
| 63 | 3 | 286 | **`tongue`** | auto, blep, long lick, forked, split, panting, studded | F56 | P F T |
| 64 | 4 | 289 | **`blush`** | ovals, circles, ///, hearts, stars, spirals, ... (12) | F58 | P F T M* |
| 65 | 4 | 293 | **`blush_cref`** | colour ref (auto = rose) | F59 | P F T C M* |
| 66 | 3 | 297 | **`chin`** | auto, round, pointy, square, soft, cleft, beard-tuft | F04 | P F |
| 67 | 3 | 300 | **`ear_size`** | 0..7 | E02 | P F T C M |
| 68 | 3 | 303 | **`ear_fold`** | none, tip, half lop, full lop, L down, R down, both down | E04 | P F T M |
| 69 | 4 | 306 | **`ear_cref`** | inner-ear colour ref | E05 | P F T M |
| 70 | 3 | 310 | **`shoulders`** | 0..7 | B02 | F T M C |
| 71 | 3 | 313 | **`belly`** | 0..7 flat .. round plush tummy | B06 | F T M C |
| 72 | 3 | 316 | **`arm_len`** | 0..7 T-rex nubs .. long | B07 | F T M |
| 73 | 3 | 319 | **`leg_len`** | 0..7 stubby .. long | B09 | F T M C |
| 74 | 4 | 322 | **`posture`** | auto, upright, proud, slouch, hunched, shy, sassy, lean back, float | B13 | F T M C |
| 75 | 5 | 326 | **`material`** | auto + 31 (short fur, shaggy, wool, velvet, scales, feathers, slime, stone, knit, ...) | B18 | P F T C M |
| 76 | 3 | 331 | **`fluff`** | 0..7 outline tufts | B19 | P F T C M |
| 77 | 5 | 334 | **`fur3`** | paint (fur colour 3; palette slot 3) | B25 | P F T C M |
| 78 | 5 | 339 | **`fur_pattern`** | none + 31 (legacy 7 FUR_* + tuxedo, calico, tiger, dalmatian, merle, ...) | B26 | P F T C M |
| 79 | 4 | 344 | **`hands`** | auto + 15 (mittens, beans, 4-finger, talons, hooves, pincers, ...) | B47 | F T M P |
| 80 | 4 | 348 | **`feet`** | auto + 15 (paws, hooves, talons, webbed, slime base, wheels, ...) | B48 | F C T M |
| 81 | 3 | 352 | **`legs`** | plantigrade, digitigrade, stumpy, bird, ghost fade, serpentine | B49 | F C T M |
| 82 | 3 | 355 | **`tail_count`** | auto, 1, 2, 3, 4, 5, 7, 9 | B53 | F T M |
| 83 | 3 | 358 | **`tail_len`** | 0..7 | B54 | F T M C |
| 84 | 4 | 361 | **`tail_cref`** | tail-tip colour ref | B56 | F T C M |
| 85 | 4 | 365 | **`wing_cref`** | wing colour ref | B61 | F T C M P |
| 86 | 5 | 369 | **`glasses`** | none + 31 (round, cat-eye, heart, aviator, 3D, monocle, goggles, visor, eyepatch L/R, ...) | F95 | P F T M* |
| 87 | 5 | 374 | **`outer`** | none + 31 (open jacket, puffer, haori, lab coat, raincoat, cape-coat, ...) | O04 | P F T M C |
| 88 | 5 | 379 | **`outer_color`** | paint | O05 | P F T C M |
| 89 | 4 | 384 | **`legwear`** | none + 11 (tights, fishnets, striped thigh-highs, leg warmers, ...) | O07 | F T C M |
| 90 | 4 | 388 | **`legwear_cref`** | colour ref | O08 | F T C M |
| 91 | 5 | 392 | **`neck`** | none + 31 (bell collar, choker, scarf, tie, locket, rosary, ...) | O18 | P F T M |
| 92 | 5 | 397 | **`back`** | none + 31 (backpack, cape, guitar, turtle shell, coffin pack, ...) | O20 | F T M C |
| 93 | 3 | 402 | **`hat_tilt`** | -2..+2 (5) | O26 | P F T M |
| 94 | 2 | 405 | **`fit`** | fitted, regular, oversized, baggy | O59 | F T M |
| 95 | 1 | 407 | **`hood_up`** | off, on | O50 | P F T C M |
| 96 | 3 | 408 | **`hood_ears`** | none, cat, bear, bunny, devil, frog, dino, fox | O51 | P F T C M |
| 97 | 4 | 411 | **`ahoge`** | none + 8 (strand, heart, antenna, ?, spring, bolt, flame, star) | H03 | P F T C M |
| 98 | 4 | 415 | **`dye`** | streaks, tips, roots, split, underdye, calico, ... (12) | H07 | P F T C M |
| 99 | 4 | 419 | **`idle`** | auto + 15 | A01 | P F C T M |
| 100 | 4 | 423 | **`walk`** | auto + 15 (waddle, strut, skip, sneak, stomp, float, hop, ...) | A03 | C F T M |
| 101 | 4 | 427 | **`run`** | auto + 15 (sprint, flail, all-fours gallop, ball roll, ...) | A04 | T M C |
| 102 | 1 | 431 | **`left_handed`** | right, left | A19 | T M C F + FP weapon side |
| 103 | 5 | 432 | **`accent_a`** | paint (palette slot 14) | T05 | - |
| 104 | 5 | 437 | **`accent_b`** | paint (palette slot 15) | T05 | - |
| 105 | 2 | 442 | **`look_spare`** | reserved, must be 0 | - | - |
| 106 | 25 | 444 | `voice0..4` | 5 x 0..31 | - | - |
| 107 | 20 | 469 | `persona0..4` | 5 x 0..15 | - | - |
| 108 | 3 | 489 | `pronoun` | 0..4 | - | - |
| 109 | 3 | 492 | `romance` | 0..4 | - | - |
| 110 | 4 | 495 | `bday_month` | 0..11 | - | - |
| 111 | 5 | 499 | `bday_day` | 1..28 | - | - |
| 112 | 6 | 504 | `like_food` | 0..47 | - | - |
| 113 | 6 | 510 | `like_hobby` | 0..31 | - | - |
| 114 | 5 | 516 | `like_music` | 0..15 | - | - |
| 115 | 5 | 521 | `like_color` | paint | - | - |
| 116 | 6 | 526 | `hate_food` | 0..47 | - | - |
| 117 | 5 | 532 | `fear` | 0..23 | - | - |
| 118 | 7 | 537 | `catchphrase` | 0..63 | - | - |
| 119 | 3 | 544 | `age_look` | auto(roster), baby, kid, teen, adult, elder | B86 | drives the kid/teen romance gate for custom citizens |
| 120 | 4 | 547 | `workplace` | 0 none, 1..13 = ACT_* venue + 1 | A40 | replaces sim_s.job[1000] |
| 121 | 1 | 551 | `base_look` | 1 = deluxe base comes from this slot's premade/generated citizen | - | - |


### 1.4 Linked colours: the 16-slot citizen palette

The master palette is full and fixed (`palette.h:8-16`), so new colours are **references**, never
new palette entries. A `GK_CREF` field (4 bits) names a slot. The renderer resolves it to one of
16 `PMat` materials that it builds once in `pctx_init()`. The named materials in `PCtx`
(`portrait_int.h:185`) become `PMat pal[16]`, with macros that keep the old names (`c->hair`
becomes `c->pal[CS_HAIR]`).

| ref | slot | source |
|---:|---|---|
| 0 | auto | part default: species pink nose, rose blush, brows follow hair, and so on |
| 1 | fur 1 | `skin` (natural tone or paint) |
| 2 | fur 2 | `marking_color` |
| 3 | fur 3 | **`fur3`** (new paint) |
| 4 | hair | `hair_color` (with today's "darker than fur" rule) |
| 5 | highlight | `hair_highlight` (falls back to the hair colour) |
| 6 | eyes | `eye_color` |
| 7 | right eye | **`eye_color_r`** |
| 8 | horns | `horn_color` |
| 9 | top | `top_color` |
| 10 | bottom | `bottom_color` |
| 11 | shoes | `shoes_color` |
| 12 | hat | `hat_color` |
| 13 | accessory | `accessory_color` |
| 14 | accent A | **`accent_a`** |
| 15 | accent B | **`accent_b`** |

Deluxe colour fields (D tier, a byte each, so bits are not scarce there) take
**0 auto, 1..15 slot, 16..47 paint 0..31, 48..55 reserved for island dyes**.

In the creator, the user never sees slot numbers. The colour picker shows *Auto*, then a
"your colours" strip with labelled chips, then the 32 paints. When a paint is picked for an
R-tier row, the allocator does this:

1. It reuses a slot that already holds that paint, preferring accents, then fur, then hair.
2. Otherwise it puts the paint into a free accent. An accent is free when no other row refers to
   it.
3. If both accents are in use, it asks: "Replace Accent A (blush, tail tip) / B (brows) /
   Cancel".

A linked colour then follows its source. "Brows = Hair" stays true when the hair is re-dyed. Each
row offers a "Match fur / hair / eyes" quick button (wishlist T05).

### 1.5 Deluxe tier (D): 238 fields, zero RAM for citizens the island came with

```
deluxe value(citizen c, field d) =
    XTRA heap override for (c, d), if one exists                      // player edits only
    else gen_deluxe(island_dlx_seed ^ c, district(c), look(c))[d]     // if gene.base_look == 1
    else 0 (auto)                                                     // player-made citizens
```

- **`gen_deluxe()`** (`resident_dlx.c`, about 3 KB) draws from its own `DoomoRng` stream with
  weighted tables per field. It reads the in-record look: species, district, age_look, persona
  bits and outfit scheme. That keeps things coherent: whiskers mostly on critter-folk, goth liner
  mostly in Hellvile, no makeup on kids, plague masks in the clinic crowd, and so on. About 10-20
  of the 238 fields end up non-zero for a typical citizen. It costs about 8k cycles on the GBA
  and runs only when a portrait is not cached. It is pinned by `fingerprints_dlx.txt` (§3.5).
- **The seed:**
  - On premade islands it is a constant (`ROSTER_DLX_SEED`), so Mayor Giggs looks the same on
    every premade island.
  - On random and empty islands it is the island seed. Islands migrated from a v1 random island
    use `look_gen == 1` (§3.3), which turns the base off.
- **Authored overrides for premades.** `doomo_roster_dlx[]` is a sorted list of
  `{u16 rid, u8 df, u8 value}`, written in the roster text as `dlx.lip_color=cherry`. It is used
  for the story cast and about 100 fashion icons, and is capped at 512 entries (2 KB).
- **`base_look` (1 identity bit).**
  - It is 1 for every citizen `town_new()` creates on premade and random islands, and for babies.
  - It is 0 for citizens the creator makes from scratch. Their "auto" stays auto, so they never
    pay heap bytes for generated extras they did not choose.
  - When a premade is edited, the creator diffs the chosen deluxe values against
    `gen_deluxe(seed, slot, new look)` and stores only the differences.

**The XTRA heap** replaces the creator's fixed nickname table. It holds deluxe overrides,
nicknames and (in wave 4) Look Book entries.

```c
/* xtra.h: one packed byte heap, records sorted by (rid, type), saved as chunk 'XTRA' v1 */
#if DOOMO_PLATFORM_GBA
#define DOOMO_XTRA_BYTES 1600       /* 1,024 B NICK table + 1,000 B sim job cache paid for it */
#else
#define DOOMO_XTRA_BYTES 4096       /* PS1 and native */
#endif
/* record:
 *   u8 rid_lo; u8 rid_hi:2 | type:2 | flags:4; u8 len; payload[len]
 *   type 0 DELUXE   payload: u8 rev, then (u8 df, u8 value) pairs sorted by df  (<= 64 pairs)
 *   type 1 NICK     payload: u16 name_hash (as CrNick.hash today), 1..11 chars, no NUL
 *   type 2 BOOK     payload: u8 slot, 56 B look-section gene bytes, deluxe pairs (wave 4)
 */
typedef struct DoomoXtra { uint16_t used; uint8_t heap[DOOMO_XTRA_BYTES]; } DoomoXtra;
int  xtra_dlx(int rid, uint8_t df_out[], uint8_t val_out[]);       /* returns the number of pairs */
int  xtra_set_dlx(int rid, const uint8_t *df, const uint8_t *val, int n); /* 0 = heap full     */
void xtra_drop(int rid);                    /* town_free() and slot reuse                     */
uint32_t xtra_hash(int rid);                /* part of the portrait cache key                 */
```

Capacity at a 3 B header + 1 B rev + 2 B per pair (28 B for a citizen with 12 deluxe edits):

| | GBA (1,600 B) | PS1 (4,096 B) |
|---|---:|---:|
| players' citizens with 12 deluxe edits each | 57 | 146 |
| ... if all 64 nicknames are also set (about 11 B each, 704 B) | 32 | 121 |

The creator shows the fill level ("Wardrobe 37%", option `CR_CAPACITY`). A commit that would
overflow offers three choices: **Bake** (keep only the deluxe fields that differ from what the
generator would give, if `base_look` can be set to 1), **Simplify** (drop chosen deluxe extras;
the in-record look stays), or Cancel. A lookup is a linear scan of at most 1.6 KB or 4 KB: about
1-4k cycles, and only when a portrait is not cached.

Deluxe field list (DF ids in order, *calc*: 238 fields, 53 of them colours, about 3,680 value
choices). It covers every D-tier wishlist item except those cut in §1.8:

| DF ids | group | field:values (`col` = deluxe colour: auto, 15 linked colours, 32 paints) |
|---|---|---|
| 0-4 | Head & face shape | `head_width`:16, `cheek_full`:8, `feature_y`:16, `feature_scale`:8, `face_age`:5 |
| 5-15 | Eyes | `iris_size`:8, `eye_color2`:col, `eye_glow`:4, `lower_lashes`:4, `lash_color`:col, `eye_outline`:5, `eye_bags`:6, `eye_asym`:7, `extra_eye_place`:6, `gaze`:7, `special_eye`:24 |
| 16-26 | Brows, nose, muzzle, whiskers | `brow_angle`:7, `brow_spacing`:8, `brow_len`:4, `nose_shine`:2, `nose_y`:8, `muzzle_puff`:4, `muzzle_color`:col, `whisker_pads`:4, `whiskers`:8, `whisker_len`:4, `whisker_color`:col |
| 27-34 | Mouth | `mouth_w`:8, `mouth_y`:8, `mouth_curve`:7, `lip_color`:col, `lip_style`:7, `fang_place`:5, `teeth_color`:6, `tongue_color`:6 |
| 35-50 | Cheeks & skin details | `blush_amt`:4, `blush_pos`:4, `blush_size`:4, `freckles`:8, `freckle_amt`:4, `freckle_color`:col, `beauty_mark`:16, `scar1`:16, `scar2`:16, `seams`:16, `seam_color`:col, `bandage`:8, `bandage_color`:col, `cheek_fluff`:8, `face_gem`:8, `gem_color`:col |
| 51-62 | Makeup, face paint, face ink | `eyeshadow`:10, `shadow_color`:col, `eyeliner`:10, `liner_color`:col, `highlighter`:4, `nails`:5, `nail_color`:col, `paint_color2`:col, `paint_finish`:4, `paint_color1`:col, `face_tattoo`:16, `tattoo_color`:col |
| 63-68 | Piercings | `ear_pierce`:15, `pierce_side`:3, `face_pierce`:256, `tongue_stud`:2, `horn_rings`:4, `pierce_metal`:8 |
| 69-71 | Glasses details | `frame_color`:col, `lens_tint`:9, `glasses_pos`:4 |
| 72-81 | Hair | `bangs`:16, `hair_volume`:4, `hair_len`:4, `hair_mirror`:2, `hair_color3`:col, `hair_shine`:4, `hair_acc`:18, `hair_acc_color`:col, `hair_acc_side`:4, `fhair_color`:col |
| 82-83 | Ears | `ear_angle`:8, `ear_tip`:8 |
| 84-94 | Proportions | `torso`:8, `waist`:8, `hips`:8, `arm_thick`:4, `leg_thick`:4, `neck_len`:4, `chibi_ratio`:5, `hand_size`:4, `foot_size`:4, `muscle`:4, `squish`:4 |
| 95-120 | Fur, skin & body art | `fur_len`:4, `sheen`:7, `translucency`:4, `pattern_scale`:4, `pattern_place`:32, `belly_color`:col, `pad_color`:col, `pad_shape`:4, `claws`:5, `claw_color`:col, `chest_fluff`:8, `chest_color`:col, `mane`:8, `grey_muzzle`:4, `glow_marks`:16, `glow_color`:col, `glow_pulse`:4, `tattoo1`:16, `tattoo1_at`:8, `tattoo2`:16, `tattoo2_at`:8, `ink_color`:col, `body_scars`:10, `plush_build`:8, `mobility`:7, `prosthetic`:14 |
| 121-141 | Tail, wings, horns, head extras, halo, aura | `tail_fluff`:4, `tail_tip`:8, `tail_pose`:8, `wing_size`:8, `wing_color2`:col, `wing_pattern`:8, `wing_pose`:5, `wing_pairs`:3, `horn_count`:4, `horn_curl`:4, `horn_texture`:8, `horn_tip`:col, `horn_asym`:5, `horn_pos`:6, `antennae`:8, `antenna_color`:col, `head_extra`:16, `halo_color`:col, `halo_shape`:16, `aura_color`:col, `aura_amt`:4 |
| 142-147 | Spooky anatomy (unlock-gated) | `extra_arms`:3, `body_eyes`:4, `buddy_head`:2, `belly_mouth`:2, `hybrid`:49, `hybrid_mix`:4 |
| 148-204 | Outfit layers, pieces & wardrobe | `inner_top`:12, `inner_color`:col, `stripe_color`:col, `socks`:8, `sock_color`:col, `shoe_color2`:col, `gloves`:16, `glove_color`:col, `belt`:16, `belt_color`:col, `neck_color`:col, `back_color`:col, `mask`:16, `mask_color`:col, `mask_pos`:3, `hat_size`:4, `hat_ears`:3, `hat_color2`:col, `ear_acc`:12, `ear_acc_color`:col, `tail_acc`:12, `tail_acc_color`:col, `horn_acc`:8, `horn_acc_color`:col, `wing_acc`:8, `wrist`:12, `wrist_color`:col, `prop`:32, `prop_color`:col, `top_trim`:col, `bottom_trim`:col, `outer_trim`:col, `pattern_on`:32, `outer_pattern`:32, `outer_pat_color`:col, `bottom_pattern`:32, `bottom_pat_color`:col, `legwear_pattern`:32, `legwear_pat_color`:col, `hat_pattern`:32, `hat_pat_color`:col, `emblem`:16, `emblem_color`:col, `sleeves`:9, `neckline`:10, `top_len`:4, `tucked`:2, `outer_open`:2, `bottom_len`:6, `cuffs`:2, `wear`:4, `fabric`:8, `outfit_sleep`:32, `outfit_swim`:32, `outfit_formal`:32, `outfit_battle`:32, `outfit_auto`:32 |
| 205-237 | Motion & voice | `fidget`:4, `stride`:4, `sit`:8, `sleep_pose`:9, `dance`:16, `emote`:32, `greeting`:11, `laugh`:10, `cry`:7, `angry`:8, `flustered`:8, `victory`:8, `taunt`:8, `hurt`:5, `faint`:7, `blink_style`:7, `blink_rate`:4, `ear_expr`:4, `wag_style`:7, `wag_amt`:4, `breathing`:4, `bounce`:4, `rest_expr`:12, `talk_style`:7, `gaze_habit`:6, `mood_motion`:4, `footstep`:12, `babble_wave`:16, `voice_flavour`:9, `sing_range`:4, `vibrato`:4, `quirk_sound`:8, `loudness`:4 |


### 1.6 Working structs (never stored per citizen)

```c
/* DoomoLook is now the WIDE, UNPACKED working look (renderers, creator, babies).
 * Byte per field, the 48 legacy fields keep their names and order, so the
 * renderer source keeps reading l->eye_style. hair_highlight keeps LOOK_NO_COLOR. */
typedef struct DoomoLookDlx {               /* generated from core/data/schema/deluxe.def */
#define DF(name, n, ...) uint8_t name;
#include "doomo/deluxe.def"                 /* 238 fields: l->d.lip_style ...            */
#undef DF
} DoomoLookDlx;
typedef struct DoomoLook {
    uint8_t species, body, height /* ... the 48 legacy fields, unchanged ... */;
    uint8_t pupil, iris, hetero, eye_color_r /* ... the 57 new R fields ... */;
    uint8_t age_look;                       /* copied from identity: renders kid/elder   */
    DoomoLookDlx d;                         /* resolved deluxe values                     */
} DoomoLook;
DOOMO_STATIC_ASSERT(sizeof(DoomoLook) == 48 + 57 + 1 + DF_COUNT, look_is_344);   /* 344 B */

typedef struct DoomoIdent {                 /* 26 B */
    uint8_t voice[VOICE_COUNT], persona[PERS_COUNT];
    uint8_t pronoun, romance, bday_month, bday_day, like_food, like_hobby, like_music,
            like_color, hate_food, fear, catchphrase, age_look, workplace, base_look;
} DoomoIdent;

/* resident.h accessors */
void res_look(int id, DoomoLook *out);          /* gene + deluxe resolve + cheat views */
void res_look_of(const DoomoResident *r, int id, DoomoLook *out);   /* same, explicit record */
int  res_store_look(int id, const DoomoLook *l);/* pack + heap diff; 0 = wardrobe full    */
uint32_t res_look_key(int id);                  /* FNV(gene) ^ xtra_hash(id) ^ view epoch */
#define RES_GET(r, gf)      gene_get((r)->gene, (gf))
#define RES_SET(r, gf, v)   gene_set((r)->gene, (gf), (v))
#define RES_PERSONA(r, i)   RES_GET(r, GF_PERSONA0 + (i))
#define RES_VOICE(r, i)     RES_GET(r, GF_VOICE0 + (i))
#define RES_LIKE_FOOD(r)    RES_GET(r, GF_LIKE_FOOD)      /* ... one per old member */
```

Migrating the call sites is mechanical, and the compiler checks it:

- `r->persona[i]` becomes `RES_PERSONA(r, i)`.
- `r->like_food` becomes `RES_LIKE_FOOD(r)`.
- Writes become `RES_SET`.
- `portrait_render(&r->look, ...)` becomes `res_look(id, &L); portrait_render(&L, ...)`.

The 61 look edits in `sim_social.c`'s baby code turn into a heredity table (§4.7). A local
`DoomoLook` is 344 B on the stack. The GBA main-stack high-water mark is 1,744 B *measured* of
the 6 KB reserve, so about 2.1 KB is expected afterwards.

### 1.7 Appended catalog values (A tier, within the widened bits, 0 RAM)

| list | today | after | gene bits | | list | today | after | gene bits |
|---|---:|---:|---:|---|---|---:|---:|---:|
| head shape | 12 | 24 | 5 | | ears | 16 | 32 | 5 |
| eye style | 24 | 40 | 6 | | horns | 16 | 26 | 5 |
| eye count | 1-3 | 1-6 | 3 | | tail | 16 | 32 | 5 |
| brows | 12 | 24 | 5 | | wings | 10 | 20 | 5 |
| noses | 12 | 24 | 5 | | halo | 6 | 12 | 4 |
| mouths | 20 | 32 | 6 | | marking (face paint) | 23 | 40 | 6 |
| hair | 32 | 64 | 6 | | tops | 24 | 50 | 6 |
| facial hair | 8 | 16 | 4 | | bottoms / shoes | 12 / 12 | 24 / 24 | 5 / 5 |
| body | 10 | 16 | 4 | | hats | 24 | 48 | 6 |
| aura | 8 | 20 | 5 | | patterns | 8 | 32 | 5 |
| species | 24 | 48 (wave 4) | 6 | | skin | 40 | 48 reserved | 6 |

That is **+298 values**, all drawn by part scripts (§4.2). The names follow wishlist rows F01,
F07, F25, F29, F35, F47, H01, H13, E01, B01, B52, B59, B66, B76, B79, F83, O03, O06, O12, O25 and
O45.

The marking split: the 7 `MARK_FUR_*` values (`marking` 16..22) move to the new `fur_pattern`,
where they become 1..7. `marking` becomes face paint only, so face paint and fur pattern stack. The legacy
values stay valid in `marking` and render exactly as today (§3.2), and migration moves them
(§3.3).

### 1.8 What is IN, what is cut, and why

**IN (per citizen):**

- **57 R-tier fields** (the bold rows of §1.3). These cover the whole wishlist C tier and all of
  its Top 25 picks.
- **The 3 new identity fields:**
  - age look (B86), which drives the romance gate
  - workplace (A40, a reduced version)
  - `base_look`
- **238 deluxe fields** (§1.5).
- **+298 catalog values.**
- **Heredity for babies.**
- **Unlock hooks** for about 80 cosmetics (§5.8).

**IN (tools):** everything in §5.

| cut or deferred | wishlist | why |
|---|---|---|
| 8 natural fur tones | B23 | The fixed palette is full. Skin keeps 6 bits, so the storage is ready |
| island dyes, palette cycling for dyes | E7 | Same reason. Deluxe colours 48..55 are reserved, and glow/glitter use palette rotation of existing ramps only (`PALCYCLE`) |
| +24 species | B87 | The bits are reserved (6-bit species), but each species is about 1 week of art, presets and head details. Wave 4 |
| a 64-job catalog | A40 | Replaced by a 4-bit `workplace` (the sim venue). Premade job titles stay authored text |
| motion as real per-frame animation sets | A01-A39 | Implemented as pose-offset tables on the existing 4-frame cycles (§4.4), not new frames |
| head-to-body "chibi level" | B12 | In the deluxe list, but wave 4: it changes `pb_layout()` for every view |
| buddy head, extra arms, belly mouth | B82-B85 | In the deluxe list, wave 4, behind unlocks: L-size renderer work |
| about 180 of the wishlist's 469 options | §8 | Kept to options that wire into code that exists today (§6.4). Dropped: Game Boy Player rumble, PS1 mouse, light gun, 2nd-pad assist, PAL 50 Hz, 480i menus, CRT scanlines, GBA mix-rate, most per-mode tweaks and modifiers. Each needs a new subsystem first |
| a dense 117 B "LookX" per citizen | wishlist E3 | Replaced by base-from-seed plus a heap (§1.5): dense would be 876 bits x 1001, about 107 KB *calc* |
| `DoomoResident` shrink to 81 B | wishlist E1 option B | This design keeps 96 B (angle A) and spends the freed bits on features |

---

## 2. Memory math

### 2.1 Starting point (*measured*, HEAD `43af439`)

- **GBA game ROM.** EWRAM 254,464 / 262,144: **7,680 B free**. IWRAM 6,460 B left for the 6 KB
  stack. ROM 2,428,644 B of a 32 MB cart.
- **PS1 game disc.** EXE 1,224 KB + BSS 511 KB + overlay slot 108 KB, ending at `0x801dd310`.
  That is **139 KB below the stack top**, and `elf2exe.py` requires at least 64 KB.
- The integration team's targets (pending tasks) are **at least 24 KB** EWRAM free on the GBA and
  **at least 128 KB** of PS1 headroom.
- This design must not consume either platform's headroom.

### 2.2 GBA EWRAM (bytes)

| change | delta | where |
|---|---:|---|
| `sim_s.job[DOOMO_MAX_RESIDENTS]` removed: the venue now comes from the gene `workplace` (4 bits). `roster_build.py` runs the job-word scan of `job_venue_scan()` offline | **-1,000** | `sim_int.h:42` |
| `static CrNick nicks[64]` (16 B each) removed: nicknames become XTRA records | **-1,024** | `creator.c:34` |
| `DoomoXtra` heap | +1,602 | new `xtra.c` |
| portrait `Job`: `DoomoLook L` (48 B) becomes a packed snapshot (`gene[69]` + rid + up to 64 resolved deluxe pairs = 200 B). Each slice unpacks it into a stack `DoomoLook` (about 4k cycles a slice) | +152 | `portrait.c:466` |
| `PCtx`: 13 named `PMat` become `pal[16]`, plus material, fluff and LOD ints (inside `Job`) | +34 | `portrait_int.h:177` |
| `DoomoSettings` global option block (80 B packed + u16 generation + u16 crc) | +84 | `options.c` |
| `DoomoTown`: `optx[32]` packed island rules + `uint8_t look_gen` (4-byte aligned) | +36 | `town.h:148` |
| `PRG2` extra progress counters (8 x u32: makeovers, photos, sprint metres, heals, introductions, rainy days, critters made, slime kills) | +32 | `unlocks.c` |
| `UNL2` second unlock bank (512 bits), in case the 43 free creator ids and the foundation spare ids do not cover the about 80 new unlocks | +64 | `unlocks.c` |
| **net** | **-20** | |

Unchanged:

- `doomo_town.res[]` (96,000 B) and `player`.
- The 18,432 B portrait pool and the canvas.
- IWRAM: the part VM stays THUMB code in ROM, and only the pixel loops are in IWRAM, as today.
- The creator state: `CrState` lives in the 12 KB scene scratch and goes from 6,560 B to about
  8,710 B (§5.9).

GBA ROM grows by about +150 KB (scripts, painters, tables, strings, roster v2), to about 2.6 MB.

### 2.3 PS1 main RAM (bytes; resident code and const data are RAM on the PS1)

Reclaims:

| reclaim | delta | how |
|---|---:|---|
| **roster records on the CD** (recommended): `doomo_roster_premade[1000]` (96,000 B) becomes `ROSTER.BIN` on the disc, 1000 x 78 B `DoomoPremade`. RAM keeps the name pool (9,430 B *calc*, average name 8.43 chars + NUL), `u16` name offsets (2,000 B) and a 2-sector CD cache (4,096 B) | **-80,474** | `ps1/src/roster_cd.c` overrides `roster_premade_record()`, like `packart.c` overrides `map_def_cells()` |
| bios text BPE (44,924 B becomes 23,981 B + a 368 B pair table) *measured with `custom/grow/bpe.py`* | **-20,575** | `roster_bio()` decodes into a 2 x 64 B ring |
| job text BPE (8,055 B becomes 4,477 B + 402 B) | **-3,176** | `roster_job()` the same way |
| `sim_s.job[1000]` | **-1,000** | as on the GBA |
| `CrNick nicks[64]` | **-1,024** | as on the GBA |
| **total reclaim** | **-106,249** | |

Costs:

| cost | delta | how |
|---|---:|---|
| XTRA heap (4 KB on PS1) | +4,098 | |
| `Job`/`PCtx`, settings, `optx`, `PRG2`, `UNL2` | +402 | as on the GBA |
| gene, deluxe and options schema tables (130 x 8 B + 238 x 4 B + 285 x 6 B) | +3,700 | const, resident |
| codec, deluxe resolver, XTRA heap code | +5,000 | MIPS `-Os` estimate |
| `gen_deluxe()` tables and code | +3,000 | |
| part-script VM | +3,000 | |
| part scripts (about 300 catalog parts + about 180 deluxe decals, about 45 B each) | +22,000 | `core/gen/parts_data.c` |
| R-tier C painters: materials, iris shading, fur-pattern masks, proportions, hands/feet/legs families, tail fan, garment layers, hood | +16,000 | estimate: about 250 B a part today, from the linker map (`portrait_*.c` = 126 KB for about 350 parts plus the framework) |
| `model.c` growth + the face-texture path | +7,000 | `ps1/src/model.c` |
| v1 save reader (kept for migration) | +3,000 | `save_v1.c` |
| A-tier catalog names (resident: the creator and the almanac both use them) | +4,000 | |
| options runtime, settings store, input remap | +3,000 | |
| option consumers in resident modules (sim, ui, audio, portrait, input). The rest sit in scene overlays | +5,500 | |
| authored premade deluxe overrides (max 512 entries) | +2,048 | |
| **total cost** | **+81,748** | |

| result | headroom below the stack top |
|---|---:|
| **net with the CD roster** | **-24.5 KB, so about 163 KB** (meets the 128 KB target with 35 KB to spare for cost overruns) |
| net without the CD roster (roster compacted in RAM to 78 B records + name pool: -6,570 B instead of -80,474 B) | **+48 KB, so about 91 KB**: no overflow (27 KB above the 64 KB floor), but below the 128 KB target |

Overlay groups (`ovl_plan.py`, slot = largest group, 108 KB today):

| group | contents | today | after |
|---|---|---:|---:|
| creator | creator code + rows (about 430 x 16 B) + presets + R and D value names (about 2,600 strings, stored as one NUL pool with `u16` offsets and BPE-packed) | 50 KB | about 96 KB |
| options (new, split from "free play screens", 70 KB) | option names, value names, help texts for 285 options + the remap screen | - | about 40 KB |

Both stay under the 108 KB slot, so the slot does not grow. A CI check fails the build if any
group grows past the slot.

**The CD roster in detail.** Callers and their access patterns:

| caller | access |
|---|---|
| save and load `base_of()`, `town_new(PREMADE)` | sequential, through `roster_iter()`. That is 39 sectors, about 0.26 s at 2x, behind the existing "Saving..." banner |
| `roster_premade_record()` from the creator's copy-from-premade, `fp_shop` host look, Father Fuzz's exorcism look, the title parade, Quillby in the library | random, 1 sector (about 100 ms); the cache keeps the last 2 |

The title parade prefetches its fixed ids at boot. `roster_name()` and `roster_find()` stay
in RAM. The GBA keeps the same `DoomoPremade` table in ROM, so there is one data format and two
storage backends. The precedent is `map_def_cells()` with packed maps (`docs/PS1.md` §3).

### 2.4 Per-citizen cost model after this design

| change | GBA EWRAM | PS1 RAM | save (worst case) |
|---|---:|---:|---:|
| new value appended to a list inside its width | 0 | about 45 B script + name | 0 |
| new R field (uses 2 reserved bits, or a reorganisation into v3) | 0 | painter code | +bits x 1000 / 8 |
| new deluxe field (DF id up to 255) | 0 | painter or script + name in the overlay | 0 unless edited (2 B per edited citizen) |
| new option | packed bits in the 80 B / 32 B blocks | schema 6 B + strings in the overlay | 0 (blocks are fixed size) |

---

## 3. Saves, migration, roster and determinism

### 3.1 Save v2 format

- `DOOMO_SAVE_VERSION 2`. The loader accepts **1 or 2** (`save.c:1144` becomes a range check).
  Version 1 goes to `read_town_v1()`.
- **Resident schema v2** (`rfields_v2[]`, the same `RField {off, bits}` machinery). It has 85 units:

  | units | contents |
  |---|---|
  | 69 | gene bytes, 8 bits each |
  | 1 | the name (4-bit length + 7-bit chars, as today) |
  | 8 | `stat` (7 bits each) |
  | 7 | `status` 3, `flags` 8, `level` 7, `mood` 5, `activity` 5, `exorcism` 8, `death_cause` 4 |

  That is `NGROUPS` = 11, as in v1, so `delta_masks()`, `write_delta()`, `read_delta()` and the
  tags 0..3 stay as they are.
- **The gene is saved as raw bytes.** Spare and reserved bits round-trip. A future field placed in
  reserved bits, or a new value inside a width, needs no new save version. Only a change of a
  field's meaning does. The load path runs `gene_sanitize()` (reserved bits back to 0) and the hot
  clamps of `resident_sanitize()`.
- **Delta bases:**

  | island | base |
  |---|---|
  | premade | `roster_premade_record()` |
  | random | `town_random_record(seed, id, look_gen)` |
  | empty | `resident_default()`, so creator-made citizens that resemble the Mochi template delta-encode too. The writer still picks the cheaper of full and delta (tags 2 and 3) |
- **Town section additions:** `look_gen` (u8) after `population`; `optx[32]` raw after `opt[64]`.
- **Extension chunks:**

  | chunk | status | contents |
  |---|---|---|
  | `'XTRA'` v1 | new, replaces `'NICK'` | up to 4,096 B |
  | `'PRG2'` v1 | new | 32 B |
  | `'UNL2'` v1 | new | 64 B |
  | `'NICK'` v1 | **read only**, never written | its 64 entries convert into XTRA nickname records |

  `save_ext.c` has a hard-coded 5-slot registry. The loop bound goes 5 → 8.
- **Size:**

  | case | v1 | v2 |
  |---|---:|---:|
  | full record (all fields except the name) | 428 bits | 648 bits *calc* (+220 bits) |
  | worst case: 1000 random custom + 2048 edges | 59,148 B *measured* | **≈ 86,648 B** *calc* (slot 1 = 90,112 B, so 3.4 KB margin) |
  | story slot, all premade lived in | 13,504 B *measured* | same order: stat-only deltas are unchanged, the player record grows 27 B |

- **Room check at commit.** `save_measure()` runs the existing counting pass ("pass 1 encodes into
  nothing") without writing. That is about 0.3 s on the GBA (estimated), hidden behind the move-in
  animation. The creator calls it on commit, and so does the XTRA heap when it
  grows. If the island would not fit its slot, the commit offers Simplify instead of letting a
  later save fail. The island menu shows a "Scrapbook space" meter.
- **Global settings get their own store**, so they no longer switch with the save slot:
  - GBA: slot 0 shrinks 40 → **36 KB** (the measured story size is 13.5 KB), and flash
    `0x09000-0x09FFF` becomes an append-only settings log. Records are 96 B (magic, u16
    generation, 80 B packed, CRC-16). 42 records fit before the sector is erased, and the newest
    valid record wins.
  - PS1: a 1-block card file `BASLUS-DOOMOS`.
  - If the store is missing or corrupt, the game boots with defaults. On the first v2 boot it
    adopts the global-scope legacy values from the loaded save's `opt[]` (§6.2).
  - On 64 KB GBA flash chips, settings live in RAM only.

### 3.2 Frozen legacy fields: what the v2 code must reproduce exactly

1. **Legacy look and identity values are frozen, except for a reviewed restyle list.**
   - The v2 roster record of every premade, projected back to v1 fields by
     `resident_to_v1(&v2, &v1)`, must equal today's record byte for byte. Today's `marking` comes
     back from `fur_pattern` when `marking == NONE` and `fur_pattern` is 1..7, and
     `hair_highlight` 32 maps back to 0xFF.
   - CI checks this against `core/tests/golden/roster_v1_fp.txt`: 1000 FNV-1a values captured
     from HEAD before wave 2.
   - The exceptions are listed in `core/data/roster/restyled.txt`: the premades that are
     deliberately restyled into appended catalog values, because `test_roster` needs every value
     worn by somebody. At most 300 ids, each diff reviewed. Their untouched v1 citizens pick up the
     restyle as a content patch (§3.3).
2. **The generator's legacy draws are frozen.** `resident_generate*(seed, look_gen)` runs today's
   `gen_look()` and `gen_rest()` draw for draw.
   - With `look_gen = 1` it stops there: new fields stay auto. That is the base for migrated
     random islands.
   - With `look_gen = 2` it continues on a **second** `DoomoRng` seeded with `seed ^ 0x6C6F6F6B`
     ("look"). That stream fills the R fields, sets `age_look` and `workplace`, and may restyle a
     legacy field into an appended value (for example hair 32..63 at about 30%).
   - `fingerprint_v1(resident_to_v1(gen(seed, 1)))` must still match **today's**
     `core/tests/golden/fingerprints.txt` (96 seeds), and that file is never regenerated. A new
     `fingerprints_v2.txt` pins the full 96 B v2 record of `gen(seed, 2)`.
3. **Pixel parity.** For all 1000 premades and 20,000 generated seeds, a render with every new
   field forced to 0 (auto) and deluxe off must hash the same as today's renderer.
   - The golden `portrait_golden_v1.txt` is captured before wave 2: bust, chibi frame 0, figure
     and fighter view 4, with FNV of the pixels.
   - This is what makes "0 = today's behaviour" (wishlist E9) a tested contract, not a hope.

### 3.3 Migrating v1 saves (deterministic, the same on every platform)

`read_town_v1()` decodes with the kept `rfields_v1[]` (today's table, moved to `save_v1.c`) into
`DoomoResidentV1` records (today's 96 B layout, defined only in `save_v1.c`). The v1 delta base is
`resident_to_v1(base_v2)`. Bases only change values, never the bit stream, so parsing is exact.
Then `resident_from_v1(&v1, &base_v2, tag, &out)`:

| v1 record | new R fields | `base_look` | legacy fields |
|---|---|---|---|
| tag 1 (identical to base: an untouched premade or random citizen) | from the v2 base | 1 | from the v2 base |
| tag 2 delta that touched **no** look field (stat or identity changes only) | from the v2 base | 1 | v1 values |
| tag 2 delta that touched a look field, or tag 3 full | **0** (auto) | 0 | v1 values, with the marking split applied |

Rule for each case:

- Untouched premades get their v2 makeover: new R fields plus generated deluxe extras.
- Anything the player designed renders **identically** to v1.

Other migration steps:

- **Random islands** load with `look_gen = 1`. Their base is today's generator with new fields and
  deluxe off, so migrated random islands look exactly as before. New random islands use
  `look_gen = 2`.
- **Options:** `opt[64]` is read as before. If the settings store is uninitialised, global-scope
  values are copied into it once. `optx` resets to defaults.
- **Nicknames:** the `'NICK'` chunk becomes XTRA records.
- **Workplace:** for premades that pass today's `sim_traits()` rule (premade island, not
  `RF_CUSTOM`, name unchanged) it comes from the v2 roster base, where `roster_build.py` set it
  offline. Everyone else gets 0. The sim behaves exactly as today, because the cache it replaces
  gave 0xFF (none) in all the other cases.
- **Age look:** 0 (auto, meaning "use roster traits"), and set to `baby` for v1 citizens with
  `romance == NONE && body == BODY_TINY && top == TOP_PAJAMAS` (today's baby signature,
  `sim_social.c:247-257`).
- After a successful v1 load, the next save writes v2. The loader shows "Your island was updated
  to the new wardrobe!" once. A v1 save is never overwritten until the v2 save has been written
  and verified (`save_verify`, option `SAVE_VERIFY`).
- **Fixtures:** before wave 2, CI captures six v1 saves (story lived in, premade untouched, 500
  custom + 500 premade, random lived in, empty, worst case) together with the FNV of the decoded
  town. These go into `core/tests/fixtures/`.

### 3.4 Roster v2 (`tools/roster_build.py`)

- **Input:** the text records stay. New keys are the R-field names (`pupil=heart`,
  `sclera=black`), `dlx.<field>=` for authored overrides, and `workplace=` (optional). By default
  it is derived from the job title with today's word table, now in Python.
- **Output:**
  - `DoomoPremade doomo_roster_premade[1000]` (78 B: `gene[69]`, `stat7[7]` packed 7-bit stats,
    `flags`, `mood | activity << 4`). Today's roster has status ALIVE, level 0, exorcism 0 and
    death 0 for all 1000, which is checked by the tool.
  - the name pool and offsets
  - `doomo_roster_dlx[]`
  - `ROSTER.BIN` for the PS1 disc
- **`LOOK_ORDER` and positional emit go away.** The tool packs with the parsed `gene.def`, so
  record order can never drift from the struct again.
- **New-value coverage** (`test_roster.c:214`): every value of every R enum is used by at least
  one premade. Every deluxe value is used by a premade's *resolved* deluxe (generator + authored)
  or by a creator preset. `seen[24][64]` becomes `seen[GF_COUNT + DF_COUNT][256]` in a bitset
  (11 KB, test-only).

### 3.5 Determinism and parity on hardware

- The codec, `gen_deluxe()`, part scripts and colour resolution are integer-only and depend only
  on byte order inside `uint8_t` arrays, so they give the same result on gcc, arm-none-eabi and
  mipsel (`xcheck.sh`).
- **New: on-device fingerprints.** A harness command `fp` is added to `gba/src/hcmd.c` and
  `ps1/src/hcmd.c`. It prints the v2 fingerprints of the 96 seeds, the FNV of all 1000 premade
  genes, and the resolved-deluxe FNV of the premades. Both harness scripts diff the output
  against `fingerprints_v2.txt`. This closes the gap noted in the current map (no on-device
  fingerprint check).
- Portrait bit-identity between GBA and native already holds (`docs/PORTRAIT.md:191`). The bench
  hashes are extended with 16 "everything on" looks.

---

## 4. Rendering

### 4.1 Pipeline changes (`portrait*.c`)

1. **Unpack once per render.** The entry points keep their signatures (`const DoomoLook *`, which
   is now the wide struct). The cached accessors (`portrait_image(rid)`, `bodyspr_image`,
   `fighter_image`) compute `res_look_key(rid)` for the lookup:
   - FNV-1a over the 69 B gene: about 550 cycles, against 48 B today
   - `xtra_hash(rid)`
   - the global *view epoch*, which changes when a render cheat changes

   `res_look()` (gene unpack about 3k cycles + deluxe resolve about 1-12k) runs only on a miss.
   `figure_image(const DoomoLook *)` hashes the wide struct word by word (about 1k cycles).
2. **`pctx_init()`** builds `pal[16]` (§1.4) plus the material state (sheen, translucency,
   fluff count) and the LOD flags.
3. **`DL_STEPS` 6 → 7.** The face group "2: head base + face" splits into:
   - 2a: head base, eyes (sclera, iris, pupil, shine, lashes, lids)
   - 2b: brows, nose and muzzle, mouth and teeth and tongue, cheeks, makeup, face paint, ink,
     scars

   Every new face layer lands in 2a or 2b, so a single slice stays at or under 0.25M cycles on
   the GBA. `PC_AHEAD` 8 → 9 keeps the look-ahead longer than a 7-slice job.
4. **LOD** (`LOD_TINY` chibi, `LOD_MID` fighter, `LOD_BIG` bust and figure). Every painter and
   every part script carries a mask. The chibi skips anything under 1 px: pupils, iris, lashes,
   makeup, piercings, teeth, freckles, tattoos, whiskers. It keeps colours, silhouette parts,
   ears, tails, wings, hats, glasses (as a 1 px band), outer layer and legwear colour, ahoge,
   hood and the gait.
5. **Part-id budget.** `pc_part()` saturates at 253 parts per image (`portrait_gfx.c:136-147`).
   Decals (makeup, ink, piercings, freckles, patterns, glow lines) paint **inside their parent's
   part id** (`pc_part_inherit()`), so they add no outline parts. A test renders 64
   "everything on" looks and asserts at most 200 parts.
6. **Render cheats** (`OPT_CHEAT_BIG_HEAD` with Tiny, `OPT_CHEAT_PAPER_MODE` Paper/Plush,
   `OPT_CHEAT_ALL_GHOSTS`, `CIT_SIZE`, `FURRY_ISLAND`, `BIG_EARS`, `FACE_SHUFFLE`) are applied in
   `res_look()` as a *view override* after resolving. They bump the view epoch, which goes into
   every cache key. The saves are never touched.

### 4.2 Part scripts (`core/data/parts/*.txt` → `tools/parts_build.py` → `core/gen/parts_data.c`)

Every appended catalog value and most deluxe decals are bytecode, not C, because on the PS1
compiled code costs RAM. About 250 B of MIPS per part today (*measured*: `portrait_body.c`
37,028 B, `portrait_head.c` 36,260 B and `portrait_hair.c` 27,944 B for about 350 parts) becomes
about 45 B of script.

```c
typedef struct PartScript {      /* 4 B header + ops, in one const blob             */
    uint8_t layer;               /* DL group 0..6 and front/back                     */
    uint8_t anchor;              /* PA_HEAD_FRONT PA_HEAD_SIL PA_HEAD_TOP PA_EYE_L/R
                                    PA_MOUTH PA_NECK PA_CHEST PA_BACK PA_HIP PA_HAND_L/R
                                    PA_FOOT_L/R PA_TAIL_ROOT PA_WING_ROOT PA_EAR_L/R   */
    uint8_t lod;                 /* LOD mask + view mask (P/F/C/T)                   */
    uint8_t len;
} PartScript;
/* ops (int8 coordinates in anchor space, -128..127 = the anchor's half extent;
 * PA_HEAD_* go through ph_proj() so they wrap around the head as yaw changes):
 *   MAT  ref         material: colour ref / the row's own colour / line / white
 *   PART flags       new part id (PF_FUZZ / PF_SHADE / PF_NOLINE) or INHERIT
 *   ELL  x y rx ry   CAP x0 y0 x1 y1 r   TRI x0 y0 x1 y1 x2 y2   POLY n (x y)*
 *   ARC  x y r a0 a1 w    LINE x0 y0 x1 y1    DOT x y
 *   MIRROR ... END   replay the block mirrored (L/R symmetry, one-sided values skip it)
 *   IFVIEW mask / IFYAW lo hi / IFANIM phase / IFFLAG gf value   conditional blocks
 *   PAT tile         pattern fill on (p_pattern_tiles), PAT 0 off
 *   SCALE gf         scale the block by a slider field (ear_size, wing_size, hat_size ...) */
void pv_run(PCtx *c, int list, int value);   /* about 3 KB VM; dispatch about 30 cycles an op */
```

The VM calls the existing shape primitives (`pc_ellipse`, capsules, triangles, outline marking),
so results are pixel-identical across platforms and scripts can be tested like C painters. The
distinctness test (`test_portrait.c:69-93`) runs over scripts too. Its `MAXOPT 40` becomes 64,
which fixes today's silent truncation. The VM loop stays THUMB in ROM on the GBA: it is not the
pixel loop, so no IWRAM is needed.

### 4.3 Feature rendering matrix (R tier; deluxe groups follow the same painters)

| feature (gene fields) | painter / where | group | chibi | GBA fighter | PS1 model |
|---|---|---|---|---|---|
| pupil, pupil_size, iris, eye_color_r, hetero, sclera, shine, lids, lashes | `one_eye()` (`portrait_head.c:813`) split into sclera, iris, pupil (32 pupil shapes as scripts on `PA_EYE_*`), shine, lid and lash passes. Hetero uses `eye_color_r` for the right eye, or a sectoral/ring mask. Iris styles are C shading modes (2-tone ramp, ring, galaxy dither, glossy band) | 2a | colour of both eyes only | yes (`LOD_MID`: pupil shape, sclera, hetero) | face texture |
| brow_cref, brow_thick (+ deluxe angle, spacing, length) | `brows()` (`:1126`): colour from the palette ref, stroke width table | 2b | - | width only | face texture |
| nose_cref, muzzle, muzzle_len (+ deluxe whiskers, pads, puff) | `nose()` (`:1209`). Muzzles are scripts on `PA_HEAD_FRONT` filled in fur 2 or ref; length scales the script | 2b | muzzle colour patch | yes | face texture + a muzzle blob (3 lengths) |
| teeth, tongue (+ lips, fang placement, colours) | `mouth()` (`:1362`). Teeth and tongue are scripts drawn inside the mouth mask; the blep shows with a closed mouth | 2b | - | blep only | face texture |
| blush, blush_cref, chin | cheeks pass in `ph_face()` (`:1965`). 12 blush styles are scripts (`MIRROR`); the chin is a `head_shape()` modifier | 2a/2b | blush 1 px | yes | face texture |
| ear_size, ear_fold, ear_cref | `one_ear()` (`portrait_hair.c:560`): size scales, fold bends the tip (up to 2 segments), inner fill from ref. `ph_top_extent()` uses the scaled height | 0/3 | yes | yes | ear prims scaled, folded ears as a hinged box, inner colour box |
| shoulders, belly, arm_len, leg_len, posture | `pb_layout()` (`portrait_body.c:51`) + `body_mod[][]`: 5 more Q4 terms. Posture is a torso lean/offset table | 1 | leg length + belly | yes | box and blob sizes, pivots, lean |
| material, fluff | `pctx_init()` material table: shading ramp (velvet, scales, chrome, stone), translucency (dither on GBA, the ghost recolour path generalised), fluff = count of `PF_FUZZ` tufts on silhouette parts (`finish()` outline pass) | all | tint + translucency dither | yes | shading mode, semi-transparent prims (`ABR` 50/50) for slime, ghost, ice and crystal, a tuft pyramid ring for fluff |
| fur3, fur_pattern | `markings()` (`:1657`) and `portrait_body.c` fur belly and socks. 32 patterns = tile masks (`portrait_gfx.c` tiles, 64 B each, 2 KB) in fur 2 / fur 3 colours, clipped to head, torso, arms, legs and tail | 1-2b | mask, socks, belly only | yes | 2-tone body parts (belly, socks, mask boxes) + up to 3 stripe bands or 4 spot blobs |
| hands, feet, legs | `pb_draw_front()`, `foot()` (`:299`), `legs()` (`:464`): C families (paw, mitten, hoof, talon, pincer, blob, tentacle, wheel). Digitigrade = a 2-segment leg with a backward knee | 1/4 | feet only | yes | per type: dark box hoof, 3 talon pyramids, flat slime blob, discs; digitigrade = 2 boxes a leg with the knee pivot |
| tail_count, tail_len, tail_cref | `pb_draw_tail()` (`:1224`) called N times along an angle fan (kitsune: 9 at ±60°), length scales, tip in ref colour | 0 | up to 3 | yes | N thin boxes in a fan (merged into one fan blob beyond LOD distance), tip box |
| wing_cref (+ size, colour 2, pattern, pose, pairs) | `wing_one()` (`:1393`): membrane/feather fill from the ref | 0 | colour | yes | wing panels in the ref colour (pose = panel angle) |
| glasses (32) | scripts on `PA_HEAD_FRONT` in group 4 (face accessories), replacing the 4 accessory values | 4 | 1 px band | yes | face texture |
| outer, outer_color, legwear, legwear_cref, fit, hood_up, hood_ears | garment family painters in `portrait_body.c` (`top_details()`, `collar()`, `bottoms()`): outer = a second torso shell with open front and lapels. Legwear paints legs before the bottoms. Fit scales the garment silhouette. The hood is a head shell + ear script | 1/3/4 | colours + hood | yes | outer = a larger torso blob + 2 front boxes; legwear colour on the legs; hood blob over the hair cap |
| neck, back | scripts on `PA_NECK` / `PA_BACK` (the old `neck_acc()` values move here) | 0/3 | back silhouette | yes | small box or blob recipes (scarf, backpack, cape panel, shell) |
| hat_tilt, ahoge, dye | `ph_hat()` (`portrait_hair.c:886`) rotates the hat anchor (±2 steps). Ahoge = 8 scripts on `PA_HEAD_TOP`, bouncing with `anim`. Dye = a mask over the hair shapes in `hair_front/back_shapes()` (tips, roots, split, underdye, calico) | 0/3/4 | ahoge, split dye | yes | hair cap 2-tone (split, tips), ahoge pyramid |
| idle, walk, run, left_handed | pose tables (§4.4) | - | gait | run anims 0..3, gun hand mirrored | gait parameter table, mirrored gun arm |
| deluxe makeup, paint, ink, scars, seams, piercings, gems | scripts, `INHERIT` part ids | 2b | - | some (`LOD_MID`) | face texture |
| deluxe garments, accessories, props | scripts + garment families | 1/3/4 | silhouettes | yes | recipes for gloves, belt, mask (face texture), hat size |

### 4.4 Motion without new art

The renderer already poses `arm_pose`, `step`, `legs` and head bob. Each motion style is a row of
small offsets:

```c
typedef struct GaitStyle { int8_t stride, bob, sway, lean, arm, hop, kind; } GaitStyle;
/* kind: biped / quadruped / float / roll / slither */
extern const GaitStyle doomo_walk_styles[16], doomo_run_styles[16];
typedef struct IdleStyle { uint8_t period, amp, kind; } IdleStyle;  /* bounce/sway/fidget ... */
extern const IdleStyle doomo_idle_styles[16];
```

| consumer | what it uses |
|---|---|
| `render_body_ctx()` (`portrait.c:271`) | `steps[]`, from the walk style |
| `render_fighter_ctx()` (`:306`) | the run style. An "all-fours gallop" leans the torso and swings 4 limbs |
| `pctx_init()` | the idle period/amp of `breath`/`twitch`/`wag` |
| `model.c` (PS1) | the same tables |
| GBA fighter | the cached run frames 0..3 |

Emotes, laughs and dances (deluxe) are short sequences of `arm_pose` + expression + hop offsets,
played by the overworld and the creator's live-test buttons. They render through the normal
cache key (`expr` + phase).

### 4.5 PS1 third person: face texture + a richer `model.c`

`model.c` today reads 12 of 48 fields (`ps1/src/model.c:296-418`). After this design it gets two
upgrades.

**1. Face texture (wishlist E6).**

- **The render.** A new view kind `VK_FACE` draws 32x36: no body and no head base (transparent
  index 0). It covers only groups 2a/2b plus face accessories: eyes, brows, nose and muzzle,
  mouth, blush, makeup, paint, ink, piercings, glasses and mask. It gets its own cached accessor
  `face_image(rid, expr)`: 3 pool blocks, 1,152 B, PS1 only. It is keyed like the portrait, so
  blinks and talk flaps re-render through the normal sliced job.
- **Upload.** `vram_image()` uploads it into the image cache (8bpp, master CLUT, index 0
  transparent).
- **Drawing.** `model_player_draw()` draws it as one `POLY_FT4` on the head blob's front, lit
  with the head's vertex colour, with OT bias -2. It replaces the 8 eye, glint, blush and mouth
  boxes (`model.c:362-372`).
- **Result:** every face option, including all deluxe face fields, appears on the 3D model for
  one textured quad. Cost: one 3-block cache entry and about 0.1 ms of GPU time.

**2. Geometry from the gene.** Each feature is a recipe table, not code per value:

| look data | model |
|---|---|
| `height`, `build`, `head_size`, `shoulders`, `belly`, `arm_len`, `leg_len`, `posture` | box and blob extents and pivots, lean angle |
| `material` | shade function: matte, glossy (higher lit contrast), metallic, glitter (a per-frame random lift on 2 faces), translucent (semi-transparent prims, `ABR` 0), stone/bone (desaturate) |
| `fluff` | 0..8 tuft pyramids on cheeks, head and chest |
| `fur_pattern` | mask, belly, socks, points: coloured sub-boxes. Stripes: up to 3 dark bands on the torso and tail. Spots: 4 blobs |
| `hands`, `feet`, `legs` | 16 + 16 + 6 recipes (§4.3) |
| tail (style, count, length, tip) | 32 styles grouped into 6 recipes (long, puff, fan, spade, curl, fin), times count, with the tip box |
| wings | 20 styles in 5 recipes, colour ref |
| horns | 26 styles in 7 recipes (2-segment curl) |
| hat (48) | 12 recipes: cap, beanie dome, top hat, cone, crown ring, helmet, ear pair, headphones, band, puff, veil, brim |
| `outer`, `legwear`, `neck`, `back`, hood | §4.3 |
| `left_handed` | the gun arm and gun on the left (`box(-U(10), ...)`) |
| `walk`/`run` | `GaitStyle` (stride, bob, lean, quadruped gallop, float, roll) |

**Budget.** At most 120 primitives (about 45 today), at most about 400 triangles after back-face
culling. Past 6 map cells it falls back to a merged LOD: no tufts or stripes, tails as one fan.
That is under 1.5 ms of GTE+GPU per frame, inside the 30 fps PS1 frame (`docs/PS1.md` §12).
`model_set_look()` (the engine test) takes the wide `DoomoLook`.

**3. The PS1 creator turntable (T18, wave 4).** A platform hook
`plat_model_preview(const DoomoLook*, x, y, w, h, yaw, anim)` (a weak no-op elsewhere) lets the
PS1 creator draw the real 3D model in the turntable panel through its own small OT. On the GBA
the turntable stays `fighter_render` (8 directions), which is already fully procedural.

### 4.6 GBA third person and chibis

- **Third person:** the fighter billboard (`combat.c:1650-1668`) is procedural, so every R and
  deluxe field appears automatically. The cost budget is §4.9.
- **Overworld and island chibis:** `bodyspr_image` at `LOD_TINY` (1 block each).
- **Free-play render memo:** keyed by `res_look_key()` instead of `look_hash(&r->look)`
  (`fp_common.c:656`).

### 4.7 Babies (`sim_make_baby()`, `sim_social.c:223-279`)

The 20 hand-written `pick8()` lines become a table walk over GF and DF ids with heredity
classes:

| class | rule |
|---|---|
| `H_PARENT` | species, skin, fur 2/3, fur_pattern, eye colours and hetero, ears, tail, wings, horns, halo, muzzle, material: from mum or dad, weighted by option `BABY_LOOKS` |
| `H_MIX` | sliders: the average ± 1 |
| `H_FORCED` | today's baby rules: BODY_TINY, big head, pyjamas, no hat, pacifier at 50%, `age_look = baby`, `workplace = 0` |
| `H_RANDOM` | everything else, from the baby's generated record |

Calico, tortoiseshell and heterochromia get a small "mutation" chance (`BABY_MUTATION`).
`base_look = 1`, so babies' deluxe extras are generated and cost no heap.

**Age safety (fixed rule, not an option).** `sim_romance_ok()` (`sim.c:217`) refuses romance for
either partner when `age_of(id)` is baby or kid, and requires teen on both sides or on neither.
`age_of()` is the roster trait for unedited premades (today's `sim_traits()`), else the gene
`age_look`. A creator-made kid or teen is therefore gated exactly like a premade one. Today
custom citizens have no age at all.

### 4.8 Creator preview

| preview | change |
|---|---|
| bust (`bust_image()`, `creator_draw.c:328`), figure (`:532`), turntable (`:354`) | unchanged |
| `anim_phase()` (`:312`) | learns the new animated values: ahoge bounce, glow pulse, palette-cycled materials, eye glow, aura colours |
| `preview_update()` (`creator.c:758-779`) | while a value is being **scrubbed** (a direction held), renders at `LOD_SCRUB` (no fluff tufts, glow, decals or patterns: about 60% of the cost) through the sliced job, 2 groups a tick. Full quality follows 10 ticks after input stops |
| "Before/After" (T11) | swaps `cr.shown` with `cr.orig` while a button is held (the turntable keeps the after view) |

### 4.9 Performance budget (GBA cycles at 16.78 MHz; a tick is 561,792)

| render | today *measured* | budget after | enforced by |
|---|---:|---:|---|
| bust average / worst | 763k / 1.20M | **at most 860k / 1.35M** | mGBA bench in `docs/PORTRAIT.md` §Performance, 16 random looks + 16 "everything on" looks |
| one sliced group (`PC_STEPS` = 1 on GBA) | at most 0.23M | **at most 0.25M** | `test_portrait` slice-cost check with the fake clock |
| figure | 915k / 1.38M | at most 1.03M / 1.50M | bench |
| chibi | 250k / 338k | at most 275k / 370k | bench |
| fighter | 454k / 655k | at most 500k / 720k | bench |
| gene unpack + deluxe resolve | - | at most 15k (only on a miss) | unit timing in the bench |
| cache lookup key | FNV over 48 B | FNV over 69 B + heap hash (about 1k) | - |
| pixel visits per render | at most 12k (test) | **at most 13.5k** | `test_portrait.c:420-437` (raised once, with bench evidence) |
| sim per tick (identity accessors) | - | at most 2% of a tick | `sim` ops counter (`sim_s.ops`) in `test_sim` |

**GBA UI at least 20 fps** comes from four things:

- Animating portraits stay sliced at 1 group a tick.
- New content (a new look) is still drawn at once, as today: at most 1.35M worst, about 2.4
  ticks, once per change.
- The free-play memo still allows 1 render credit a frame.
- Scrubbing in the creator uses `LOD_SCRUB` + slices.

The harness script `gba/harness/scripts/creator_fps.txt` scrubs 30 rows across every tab and
asserts fps × 10 ≥ 200 in the mailbox. A twin script covers the island with 40 visible chibis.

**PS1:** `PC_STEPS` 3. Everything above is about 2x faster on the R3000 at -O2/-Os. The model
budget is in §4.5.

**Cache memory** is unchanged: 18,432 B pool, 48 entries. Only the PS1 adds the 3-block face
image.

---

## 5. Creator UI: about 430 rows on a 240x160 D-pad screen

Today there are 9 tabs with 105 rows. On the GBA, 8 rows are visible at a time (`creator_draw.c`
layout: 86 px preview, 145 px list, 11 px rows). The answer is not longer lists but **tabs →
collapsible sections → rows**, plus modes, jump and search.

### 5.1 Structure

The strip fits at most 10 tabs at 21 px cells, so there are 10 tabs. The bracketed number is the
section count; section names below.

| tab | sections |
|---|---|
| **Body** [5] | Species & presets · Shape (body, height, build, posture, age look) · Proportions (shoulders, belly, arms, legs, torso, waist, hips, neck, muscle, squish, chibi ratio) · Hands & feet (hands, feet, legs, hand/foot size, claws, pads) · Extras (mobility, prosthetic, extra arms, buddy head, belly mouth, body eyes, hybrid) |
| **Fur** [4] | Material & colours (material, fluff, fur 1/2/3, fur length, sheen, translucency) · Pattern (fur pattern, scale, placement, belly colour, grey muzzle, chest fluff, mane) · Glow · Ink & scars (tattoos, body scars, plush construction) |
| **Face** [7] | Head (shape, size, width, chin, cheek fullness, feature height/scale, face age) · Brows · Nose & muzzle (+ whiskers) · Mouth & teeth (+ tongue, fangs, lips) · Cheeks (blush, freckles, beauty mark, cheek fluff, gems, the 8 cheek flags) · Scars & patches · Makeup |
| **Eyes** [5] | Shape (style, size, spacing, height, tilt, count, asymmetry) · Iris & pupil (colours, hetero, iris, pupil, sizes) · Whites & shine (sclera, shine, glow, bags) · Lashes & lids · Gaze & specials (gaze, special eye, extra-eye placement) |
| **Hair** [4] | Style (style, bangs, length, volume, mirror, ahoge) · Colour (colour, highlight, dye pattern, colour 3, shine) · Facial hair · Hair accessory |
| **Critter** [6] | Ears · Tail · Wings · Horns & antennae · Halo & aura · Head extra |
| **Outfit** [8] | Outfit sets · Top (+ trim, print, emblem, sleeves, neckline, length, tuck, fit) · Layers (inner top, outer + colour/trim/open/print, hood, hood ears) · Bottom (+ trim, length, cuffs, print) · Legs & feet (legwear + stripe + print, socks, shoes, shoe colour 2) · Hat (+ colour 2, tilt, size, ears, print) · Finish (fabric, wear) · Wardrobe (sleep/swim/formal/battle sets, auto-switch) |
| **Bling** [7] | Glasses · Masks & legacy accessory · Neck & back · Hands (gloves, wrist, nails, held prop) · Tiny accessories (ear, tail, horn, wing) · Piercings · Face paint & ink |
| **Moves** [4] | Body language (idle, fidget, walk, stride, run, sit, sleep) · Emotes (signature, greeting, laugh, cry, angry, flustered, victory, taunt, hurt, faint, dance) · Face motion (blink, resting expression, talk style, gaze habit, ear and tail expressiveness, breathing, bounce, mood motion) · Voice (5 sliders, presets, babble wave, flavour, singing range, vibrato, loudness, quirk sounds, footsteps, Test) |
| **You** [4] | Heart (persona 5 + type text) · Profile (name, nickname, pronoun, romance, birthday, likes, hate, fear, catchphrase, workplace, handedness) · Home (district, story-NPC flags as today) · Colours (accent A/B, palette overview, "match" buttons) |

Each tab holds about 25-60 rows. The undo key `cr.tab*64+row` (`creator.c:1382`) goes away with
the undo log (§5.6), so the "under 64 rows a tab" limit disappears. `nrows` stays `uint8_t`.

### 5.2 Rows

```c
typedef struct CrRow {
    uint8_t kind;      /* RK_HEADER (now a section), RK_GENE_ENUM, RK_GENE_SLIDER, RK_PAINT,
                          RK_CREF (linked colour), RK_SKIN, RK_FLAG, RK_RFLAG, RK_DLX_ENUM,
                          RK_DLX_SLIDER, RK_DLX_COLOR, RK_DLX_BITS (piercing / placement masks),
                          RK_NAME, RK_NICK, RK_ACTION, RK_DISTRICT                          */
    uint8_t id;        /* GF_* or DF_* by kind (each < 256): no offsetof, so no 255 B limit */
    uint8_t count, base, clist, aux;
    uint8_t tier;      /* CT_SIMPLE / CT_ADVANCED / CT_EVERYTHING (mode filter)             */
    uint8_t sym;       /* SYM_NONE or the index of the sibling L/R row (§5.5)               */
    const char *label; const char *const *names;   /* names live in the creator overlay */
} CrRow;               /* 16 B: about 430 rows = 6.9 KB, ROM (GBA) / creator overlay (PS1) */
```

`test_creator.c:68` ("a row for every look byte") becomes "**exactly one row for every GF field
except reserved fields and `base_look`, and for every DF field**". `:103` ("ENUM row counts match
the catalog") extends to GF max+1 and to DF counts.

### 5.3 Navigation (GBA buttons; PS1 in brackets)

| input | action |
|---|---|
| L / R (L1 / R1) | previous / next tab, as today |
| UP / DOWN | move. Only the open section's rows show: a tab is about 6 headers + about 12 rows, so the list always fits in 1-2 screens |
| A on a section header | open it (the previous one closes). LEFT/RIGHT on a header jumps to the previous/next header |
| LEFT / RIGHT on a row | change the value, as today, with auto-repeat |
| A on a row | the picker: a list, or for shape lists a **thumbnail grid** of cached chibi/bust crops (rendered 1 a tick, LRU in the portrait pool) |
| B | close the picker, or close the section, or step back up |
| SELECT (tap) | **Jump** (§5.4) |
| X = SELECT+A (Square) | randomise **this row** (respects locks) |
| Y = SELECT+B (Triangle) | **pin** this row (pinned rows survive every randomiser, T03) |
| L+R (L2/R2) | cycle preview: figure → bust → turntable. Face and Eyes open on the bust automatically |
| START | menu (§5.4) |

### 5.4 Finding things and changing many things at once

- **Modes** (T01, option `CR_MODE`, also in START):

  | mode | shows |
  |---|---|
  | Simple | today's 86 rows + the 12 most popular new ones: pupil, blush style, glasses, outer layer, ear size, tail count, fur pattern, fur 3, material, walk, ahoge, dye |
  | Advanced | default. All R tier |
  | Everything | + all 238 deluxe rows |

  Headers show a count: "Makeup (0/9 set)".
- **Jump** (T12), from SELECT: a list of all ~60 sections across tabs. Typing letters with the
  existing name-entry keyboard filters by row label as you type ("bl" → Blush style, Blush colour,
  Blink style, Belt). A goes straight to the tab, section and row.
- **START menu:**
  - Done
  - Randomise ▸ (everything / this tab / **this section** / colours only / shapes only, T21)
    × **theme** (T04: Cute, Spooky, Goth, Pastel, Punk, Royal, Furry, Demon, Angelic, Chaos,
    "Like Hellvile", "My favourite colour" = the identity `like_color`)
  - Hellvile-ify / Hillvile-ify (T22)
  - Undo, Redo
  - Copy ▸ (this section / colours / whole look) and Paste
  - Copy from citizen ▸ (with a section filter)
  - Pin all in section, Unpin all
  - Symmetry
  - Mode
  - Before/After
  - Test ▸ (emote, dance, laugh, walk, run, sit, sleep, voice; T19)
  - Spooky peek (T17: uses the custom sclera and glow)
  - Reset section
  - Quit
- **Themed randomiser.** `resident_randomize_look_theme(r, seed, theme, pins)` reuses
  `gen_look()` and `gen_deluxe()` with per-theme weight overrides (`resident.c` tables). It never
  produces a locked value (`fix_locks()`).
- **Presets** (ROM / overlay):

  | preset family | count |
  |---|---:|
  | species (unchanged, 3 per species) | 72 |
  | outfit sets (O62) | 20 → 60 |
  | face sets (eyes + brows + mouth + blush) | 24 |
  | makeup looks | 16 |
  | fur-pattern looks (2 per critter species) | 16 |
  | motion personalities (A38: "Shy bean", "Hyper pup", "Royal", "Goth gremlin", "Sleepy blob", "Tough cookie", "Ghosty", "Robot") | 8 |
  | voice presets | 12 |

  A preset `kv` gains a namespace byte: `0xFE` switches to DF ids. `CR_PRESET_KV` 40 → 64.
- **Species-aware smart defaults** (T16). Changing species resets `cr_species_base` as today,
  which grows to include the new species-defining fields: muzzle, hands, feet, legs, material,
  ear size, tail count, whisker style. It then **suggests**, never forces, slit pupils and toe
  beans for catfolk, hooves for goat demons, and so on, through the first species preset.

### 5.5 Symmetry and asymmetry (T07)

L/R variation lives in values, not in duplicated fields:

| field | one-sided values |
|---|---|
| ear fold | L down, R down |
| special eye | per side |
| horn asymmetry | yes |
| piercing side | yes |
| socks | mismatched |
| hetero | split |

`CR_SYMMETRY` = Symmetric hides one-sided values in pickers and keeps paired rows (L/R prosthetic
and similar) in sync through `CrRow.sym`. Independent shows everything.

### 5.6 Undo / redo (T20)

The `hist[20]` ring of full residents (1,920 B) becomes a **3 KB change log** (a ring, oldest
steps dropped first):

- Entries are `{u8 kind, u8 id, u8 old, u8 new}` plus step markers.
- "Bulk" steps (randomise, preset, paste, copy look) store one snapshot: the gene plus the deluxe
  pairs that differ from the base. That is at most 69 + 128 B, typically about 100 B.
- That gives at least 15 bulk steps in the worst case (typically 25+), or about 750 single edits,
  against 19 steps today.
- The old-value check against `cr.orig` (lock grandfathering, `creator.c:447-473`) is unchanged.

### 5.7 Commit (`write_town()`, `creator.c:1055-1104`)

1. Sanitize the gene. Pack the draft's R fields into the gene.
2. Diff the draft's deluxe values against `gen_deluxe(seed, slot, new look)` (if `base_look`) or
   against 0, then write the XTRA record.
3. If the heap is full, offer Bake / Simplify / Cancel (§1.5).
4. Call `save_measure()`. If the island no longer fits its slot, offer Simplify (§3.1).
5. Call `portrait_invalidate()`, set the nickname (now an XTRA record), add `PROG_CITIZENS_CREATED`,
   and grant the secrets, as today.

### 5.8 Unlocks for the new options

Most rows start open ("the creator is huge from minute one"). About 80 signature values are
earned. They follow the wishlist §10 list (U01-U78) and use the hooks it names:

- heterochromia: befriend someone across the border
- black sclera: 3 exorcisms
- heart pupils: the first marriage
- kitsune 9 tails: 9 exorcisms in a row
- seraph wings: 10 exorcisms
- piercings: the Pierced Pit in Hellvile
- makeup: the Glam Counter on day 3
- tattoos: Ink & Brimstone, paid in H$
- … and so on

Mechanics:

| need | solution |
|---|---|
| unlock ids | the creator range has 43 free (269, 278..319). The rest go into `UNL2` (§2.2), or reuse alias routes (`UL_ALIAS_MAX` 16) |
| new progress counters (makeovers, photos, sprint metres, heals, introductions, rainy days, critters created, slime kills) | the 8 `PRG2` counters |
| new lists | `CLIST_*` grows by one id per R/D field with gated values (`CLIST_COUNT` 31 → about 75). `UL_CAT_PART` entries carry the GF/DF id in `list` |

Locked values show as silhouettes with the "how" hint, unless `SPOILER_HINTS`. They are
grandfathered for citizens who already have them. **Premade, generated and baby citizens ignore
locks**, as today.

### 5.9 Creator RAM (scene scratch, 12,288 B)

| `CrState` field | today | after |
|---|---:|---:|
| `draft`, `orig` (`DoomoResident`) | 192 | 192 |
| `draft_dlx`, `orig_dlx` (resolved deluxe, 238 B each) | - | 476 |
| `hist[20]` → undo log | 1,920 | 3,072 |
| `shown` (`DoomoLook`) | 48 | 344 |
| pins (430 rows as bits), open sections, search buffer, clipboard (48 pairs) | - | 54 + 10 + 16 + 98 |
| everything else (pix 2,688, icons 1,089, ...) | about 4,400 | about 4,450 |
| **total** | **6,560** | **≈8,710** (3.5 KB still free) |

---

## 6. Options: 285 rows on 23 pages, all wired

### 6.1 Rules

1. **Every option has a consumer.** `tools/check_options.py` (CI) greps core/src, gba/src and
   ps1/src for each `OPT_*` id outside `options.c`, `fp_options.c` and the unlock tables. An id
   with no reader fails the build. That would have caught today's 8 dead options, which are
   wired in this design (status **FIX** below).
2. **Existing ids keep their numbers and values.** **EXT** options only *append* values. The
   legacy byte means what it meant (for example `OPT_HIT_MARKERS` 1 = "Visual + sound", which
   was "On").
3. **`opt_get()` / `opt_set()` stay the API.** They return the value or the default, and clamp.

### 6.2 Storage (global vs island, wishlist E4)

```c
typedef struct DoomoOptionInfo {     /* resident schema: 6 B a row, 285 rows = 1.7 KB */
    uint16_t pos;                    /* bit position in its store; 0xFFFF = legacy opt[] byte */
    uint8_t  count, def;
    uint8_t  bits;                   /* 1..6 (crosshair colour has 34 values = 6 bits)  */
    uint8_t  page_scope;             /* page (0..22) | scope << 7                       */
} DoomoOptionInfo;
/* text (name, value names, help) lives in a separate table that only SCENE_OPTIONS
 * (its own PS1 overlay group) and the combat pause menu (its 7-entry copy) touch. */
typedef struct DoomoSettings {       /* global: per cartridge / memory card              */
    uint8_t  bits[80];               /* 363 option bits (46 B) + 3 remap profiles (33 B) */
    uint16_t gen, crc;
} DoomoSettings;                     /* 84 B .bss                                        */
/* island: doomo_town.opt[64] (legacy bytes, as today) + doomo_town.optx[32] (195 bits) */
```

- **Scope G (180 options, 363 bits *calc*)** = system settings: audio, display, controls,
  accessibility, text, HUD, crosshair, creator preferences. They live in `DoomoSettings`, are
  stored in the settings sector or file (§3.1), and do **not** change when another island is
  loaded.
- **Scope I (105 options, 195 bits)** = island rules: difficulty, sim, romance, souls, pet,
  gore, cheats. Legacy island options keep their `opt[]` byte. New ones go in `optx`. Both are
  saved with the island, as today.
- **`SETTINGS_SCOPE` = "Each island"** copies the global block into the island's `optx` on save
  and back on load, for players who want per-save settings.
- **Migration:** on the first v2 boot with an empty store, the global-scope values of the loaded
  save's `opt[]` seed the store. The legacy G bytes in `opt[]` are written but ignored after that.
- **UI** (`fp_options.c`):
  - 23 pages. L/R cycles the pages, and SELECT opens a page list (the page names do not fit a
    tab strip on 240 px).
  - `ROWS_MAX 24` per page still holds: the largest page is Access with 24 rows.
  - The help line for each option is shown (`opt_help[]`).
  - Each page ends with "Reset this page".
  - Locked values are skipped through `value_ok()`, as today.

### 6.3 Button remapping (wishlist RM01-RM05)

```c
/* input.c: contexts the scenes set in enter() */
enum { IC_MENU, IC_OVERWORLD, IC_SHOOTER, IC_COUNT };
void input_context(int ic);
/* a profile maps each logical BTN_* bit to a physical one, 4 bits each:
 * shooter 14 x 4 = 56 bits, overworld 8 x 4 = 32 bits, menu: confirm/back swap only (1 bit)
 * = 89 bits ~ 11 B; 3 custom profiles = 33 B in DoomoSettings.bits[]                       */
```

- **Where it applies.** `input_update(raw)` (`input.c:22`) applies the active context's
  permutation after the GBA X/Y synthesis. Every scene keeps testing `BTN_*` as today, so
  remapping needs **no changes in scene code**.
- **Presets** (const): Classic, Modern, Southpaw, One-hand left/right (`ONE_HANDED`), PS1 L2/R2
  variants (`PS1_L2R2`).
- **The remap screen** says "press the new button for *Fire*". Assigning a button that is taken
  **swaps** the two actions, so a mapping always stays a bijection and conflicts cannot exist.
- **Lockout escape:** holding START + SELECT for 2 s resets the current context to the preset.
- **The combat pause menu** gains "Controls…" (the remap screen) next to its 7 in-place options
  (`combat_scene.c:57-79`).
- **PS1 analog** (`PS1_ANALOG`). `pad_analog()` (`ps1/src/pad.c:21`) already reads the sticks.
  A new `plat_analog(int *lx, int *ly, int *rx, int *ry)` (a weak 0 elsewhere) lets
  `read_controls()` (`combat.c`) use the right stick for analog look and the left for movement.
  `PS1_DEADZONE` replaces the fixed ±48 threshold. Without it, the stick still maps to the D-pad
  as today.
- **GBA:**
  - `CHORD_WINDOW` lets SELECT precede A or B by N ticks for X/Y.
  - `GBA_SWAP_LR` and `GBA_SLEEP` live in `gba/src/main.c` `read_pad()`: L+R+SELECT enters stop
    mode with a keypad IRQ wake, and audio is paused.

### 6.4 The list (*calc*: 285 rows; 237 NEW, 30 EXT, 8 FIX, 10 KEEP)

The other 9 existing options (demon density and 8 cheats) are unchanged. Values are separated by
`·`, and `*` marks the default. "n/bits" is the value count and its packed width. Scope is
G (global) or I (island).


**Game** (15)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `OPT_DIFFICULTY` | Combat difficulty | Plushie · Normal* · Nightmare · Ultra-Hellvile | 4/2 | I | combat.c refresh_stats(), actor.c difficulty_skip() (unchanged meaning: combat only from v2) | EXT |
| `LIFE_DIFF` | Life-sim difficulty | Plushie · Normal* · Nightmare · Ultra-Hellvile | 4/2 | I | sim_soul.c rate_factor() (today reads OPT_DIFFICULTY), sim.c apply_needs() decay scale | NEW |
| `PET_DIFF` | Pet difficulty | Plushie · Normal* · Nightmare · Ultra-Hellvile | 4/2 | I | pet.c stage_minutes(), needs_call() | NEW |
| `PUZZLE_DIFF` | Puzzle difficulty (exorcism, welding, minigames) | Plushie · Normal* · Nightmare · Ultra-Hellvile | 4/2 | I | exorcism.c exorcism_odds(); weld game timing; mg.c difficulty hook (today exorcism reads OPT_DIFFICULTY) | NEW |
| `OPT_AUTOSAVE` | Autosave | Off · Every day* · Every hour · On location change · On scene exit | 5/3 | G | fp_common.c autosave tick; scene.c leave hook for value 4 | EXT |
| `AUTOSAVE_ICON` | Autosave icon | Off · On* | 2/1 | G | ui.c ui_toast() 'Saving...' badge | NEW |
| `CONFIRMS` | Confirm prompts | All* · Important only · None | 3/2 | G | ui_dialog.c ui_choice() short-circuit for flagged low-risk confirms | NEW |
| `TUTORIAL_HINTS` | Tutorial hints | On* · Once · Off | 3/2 | G | ui.c ui_hint() gate + seen bitset (save_ext 'PRG2') | NEW |
| `UNLOCK_TOASTS` | Unlock toasts | Full* · Compact · Log only | 3/2 | G | unlocks.c unlock_grant() -> ui_toast() style | NEW |
| `SPOILER_HINTS` | Hide unlock hints | Off* · On | 2/1 | G | creator silhouettes 'how' text, fp_options value_ok() 'Next: ???' line | NEW |
| `MENU_THEME` | Menu theme | By district* · Always pastel · Always goth | 3/2 | G | creator_draw.c cr_is_hellish(), ui.c ui_window() skin pick | NEW |
| `BOOT_INTO` | Boot into | Title* · Continue last save · Pet screen | 3/2 | G | fp_title.c enter() | NEW |
| `SPICE` | Hellvile spice (rude words) | Clean · Grawlix* · Spicy | 3/2 | G | ui_dialog.c copy_str() word filter table | NEW |
| `OPT_MONSTER_MODE` | Monster Mode | Off* · ON (monster!) · Ask each time | 3/2 | I | sim_soul.c, levels.c, fp_options.c typed confirm (value 2 asks per kill) | EXT |
| `OPT_TEXT_SPEED` | Text speed | Slow · Normal* · Fast · Instant · Typewriter | 5/3 | G | ui_dialog.c dlg_update() | EXT |

**Story** (6)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `QUEST_MARKERS` | Quest markers | Off · Subtle* · Full arrows | 3/2 | G | overworld.c marker draw, story_questlog.c | NEW |
| `SKIP_SEEN` | Seen cutscenes | Ask* · Always skip · Never skip | 3/2 | G | cutscene.c start (seen bit in story flags) | NEW |
| `HOLD_SKIP` | Hold to skip dialogue | Off · On* | 2/1 | G | ui_dialog.c advance() on btn_held_ticks(BTN_B) >= 20 | NEW |
| `PROTAG_NAME` | Protagonist is called | Name* · Nickname · "you" | 3/2 | I | story_talk.c %P expansion | NEW |
| `STORY_NPC_EDIT` | Story NPC protection | Protected* · Look editable · Fully editable | 3/2 | I | creator.c scene enter: RF_STORY_NPC gate | NEW |
| `NGP_CARRY` | New Game+ carry-over | Nothing · Weapons · Looks + weapons* · Everything | 4/2 | I | story.c new-game-plus setup | NEW |

**Island** (10)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `STARTING_MONEY` | Starting money | 0 · 500* · 5000 · 50000 | 4/2 | I | town.c town_new() (DOOMO_START_MONEY) | NEW |
| `INCOME` | Income rate | x0.5 · x1* · x2 · x5 | 4/2 | I | sim_events.c reward(), fp_shop.c sale payout | NEW |
| `PRICES` | Shop prices | x0.5 · x1* · x2 | 3/2 | I | fp_shop.c price display + charge | NEW |
| `TRAVELERS` | Travelers visiting | Off · Rare · Normal* · Often | 4/2 | I | freeplay traveler spawn (RF_TRAVELER) | NEW |
| `SEASONS` | Seasons | Off · Four seasons* · Always spring · Always spooky | 4/2 | I | fp_island_gfx.c palette tint + weather table | NEW |
| `DAYNIGHT` | Day/night visuals | Cycle* · Always day · Always night | 3/2 | I | town.c town_is_night() cache in sim_s.night | NEW |
| `OPT_NEWS_TICKER` | News ticker | Off · All* · Favourites only · Big events only | 4/2 | I | fp_common.c ticker, sim_events.c sim_news_push() priority | EXT |
| `PROBLEM_BUBBLES` | Problem bubbles | All* · Important · Favourites · Off | 4/2 | I | fp_island.c bubble draw filter | NEW |
| `OPT_WEATHER` | Weather | Off · Normal* · Lots · Hellvile ash · Candy rain | 5/3 | I | fp_island_gfx.c weather particles | EXT |
| `OPT_EVENT_RATE` | Town events | Calm · Normal* · Chaotic | 3/2 | I | sim.c event roll (unchanged) | KEEP |

**Life sim** (15)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `OPT_SIM_SPEED` | Sim speed | Chill · Normal* · Fast · Turbo · x8 · Pause | 6/3 | I | sim.c speed_q() | EXT |
| `OPT_DAY_LENGTH` | Day length | Real time · 2 h · 1 h* · 30 min · 15 min · 45 min · 10 min | 7/3 | I | sim.c ticks_per_minute_q() | EXT |
| `AUTONOMY` | Autonomy | Low · Normal* · High | 3/2 | I | sim.c decide_activity() weight for need-solving venues | NEW |
| `PROBLEM_RATE` | Problem frequency | Rare · Normal* · Often | 3/2 | I | sim_events.c sim_check_problems() roll | NEW |
| `PROBLEM_EXPIRE` | Problems expire after | Never* · 1 day · 3 days | 3/2 | I | sim_events.c sim_problems_minute() | NEW |
| `MOOD_SWINGS` | Mood swings | Calm · Normal* · Dramatic | 3/2 | I | sim.c sim_mood_of() thresholds | NEW |
| `HYGIENE_FX` | Hygiene visuals | Off · On* | 2/1 | I | portrait fx: stink lines from STAT_HYGIENE (pf_status) | NEW |
| `AGEING` | Ageing (kids grow up) | Off* · Slow · Normal | 3/2 | I | sim.c day_rollover(): age_look step + portrait_invalidate | NEW |
| `OLD_AGE` | Old-age death | Off* · On | 2/1 | I | sim_soul.c sim_soul_daily() DEATH_OLD_AGE path | NEW |
| `NEGLECT_DEATH` | Neglect death | Off* · On | 2/1 | I | sim.c apply_needs() DEATH_NEGLECT path | NEW |
| `JOBS` | Jobs | Off · On* | 2/1 | I | sim.c decide_activity() via gene workplace | NEW |
| `SLEEP_SCHED` | Sleep schedule | By trait* · Everyone sleeps · Party island | 3/2 | I | sim.c decide_activity() ACT_SLEEPING branch | NEW |
| `DREAMS` | Dreams | Off · Rare · Normal* · Often | 4/2 | I | dream.c trigger roll | NEW |
| `THOUGHTS` | Thought bubbles | Icon* · Text · Both | 3/2 | G | fp_island.c / overworld_npc.c bubble style | NEW |
| `CATCHPHRASE_RATE` | Catchphrase frequency | Rare · Normal* · Often | 3/2 | I | sim_care.c line picker | NEW |

**Needs** (11)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `DECAY_HUNGER` | Hunger decay | Off · Slow · Normal* · Fast · Brutal | 5/3 | I | sim.c apply_needs() per-stat rate x {0,1/2,1,3/2,2} | NEW |
| `DECAY_HAPPY` | Happiness decay | Off · Slow · Normal* · Fast · Brutal | 5/3 | I | sim.c apply_needs() | NEW |
| `DECAY_HYGIENE` | Hygiene decay | Off · Slow · Normal* · Fast · Brutal | 5/3 | I | sim.c apply_needs() | NEW |
| `DECAY_ENERGY` | Energy decay | Off · Slow · Normal* · Fast · Brutal | 5/3 | I | sim.c apply_needs() | NEW |
| `DECAY_HEALTH` | Health decay | Off · Slow · Normal* · Fast · Brutal | 5/3 | I | sim.c apply_needs() | NEW |
| `DECAY_SANITY` | Sanity decay | Off · Slow · Normal* · Fast · Brutal | 5/3 | I | sim.c apply_needs() sanity block | NEW |
| `DECAY_CORRUPTION` | Corruption gain | Off · Slow · Normal* · Fast · Brutal | 5/3 | I | sim_soul.c sim_corrupt_natural() | NEW |
| `DECAY_SOCIAL` | Loneliness | Off · Slow · Normal* · Fast · Brutal | 5/3 | I | sim.c apply_needs() | NEW |
| `NEED_FLOOR` | Need floor | 0* · 10 | 2/1 | I | sim.c sim_stat_add() clamp | NEW |
| `NEED_ALERT` | Need alert threshold | 10 · 25* · 40 | 3/2 | G | fp_island.c / fp_resident.c warning icons | NEW |
| `NEED_DISPLAY` | Need display | Bars* · Hearts · Numbers · Faces | 4/2 | G | fp_resident.c stat panel, ui.c ui_bar() | NEW |

**Romance** (18)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `ROMANCE` | Romance | On* · Crushes only · Off | 3/2 | I | sim_social.c romance_step(), sim_confess() | NEW |
| `CRUSH_RATE` | Crush rate | Rare · Normal* · Often | 3/2 | I | sim_social.c romance_step() | NEW |
| `CONFESS_LUCK` | Confession luck | Harsh · Normal* · Kind | 3/2 | I | sim_social.c sim_confess() odds | NEW |
| `ADVICE_WEIGHT` | Weight of your advice | Low · Normal* · High | 3/2 | I | sim_social.c sim_advise_*() | NEW |
| `CROSS_BORDER` | Cross-border romance | On* · After the Wall falls · Off | 3/2 | I | sim_social.c cross_border_pct() | NEW |
| `PROPOSAL_SPEED` | Dating to proposal | Slow · Normal* · Fast | 3/2 | I | sim_social.c sim_propose() threshold | NEW |
| `MARRIAGE` | Marriage | Off · On* | 2/1 | I | sim_social.c sim_propose() | NEW |
| `BREAKUPS` | Breakups | Off · Rare · Normal* | 3/2 | I | sim_social.c sim_breakup() roll | NEW |
| `JEALOUSY` | Love triangles | Off* · On | 2/1 | I | sim_social.c visit_edge() crush conflicts | NEW |
| `RIVALS` | Rivals and nemeses | Off · Rare · Normal* · Spicy | 4/2 | I | sim_social.c visit_edge() REL_RIVAL/NEMESIS | NEW |
| `ARGUMENTS` | Arguments | Rare · Normal* · Often | 3/2 | I | sim_social.c visit_edge() argue roll (sim_social.c:602) | NEW |
| `FRIEND_FADE` | Friendships fade | Off · Slow · Normal* | 3/2 | I | sim_social.c sim_edges_step() decay | NEW |
| `BABIES` | Babies | Off · Rare · Normal* · Often | 4/2 | I | sim_social.c sim_make_baby() roll | NEW |
| `BABY_KIND` | Baby kind | By parents* · Always angel · Always hellspawn · Random | 4/2 | I | sim_social.c sim_make_baby() generator pick | NEW |
| `BABY_LOOKS` | Baby look heredity | Blend* · Mostly parent A · Mostly parent B · Random | 4/2 | I | sim_social.c heredity table (§4.7) | NEW |
| `BABY_MUTATION` | Baby mutation chance | None · Low* · Medium · Wild | 4/2 | I | sim_social.c heredity: per-field reroll chance | NEW |
| `BABY_NAMING` | Baby naming | Auto · Ask me* | 2/1 | I | sim_social.c -> ui name entry | NEW |
| `REL_NOTIFY` | Relationship news | All* · Close friends · Off | 3/2 | G | sim_events.c sim_news_push() filter | NEW |

**Souls** (12)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `OPT_POSSESSION_RATE` | Possessions | Off · Rare · Normal* · Frequent · Story only | 5/3 | I | sim_soul.c possession_allowed() | EXT |
| `POSSESS_CAP` | Max possessed at once | 1 · 3* · 5 · 10 · Unlimited | 5/3 | I | sim_soul.c sim_possession_cap() | NEW |
| `CORRUPT_SPREAD` | Corruption spreads | Off · On* · Viral | 3/2 | I | sim_soul.c sim_corrupt() | NEW |
| `VAPE_CORRUPT` | Vape-shop corruption | Off · On* | 2/1 | I | sim.c story_vape_open() venue effect | NEW |
| `EXO_DIFF` | Exorcism difficulty | Easy · Normal* · Hard · Holy Terror | 4/2 | I | exorcism.c exorcism_odds_with() | NEW |
| `EXO_FAILS` | Failures before a soul is lost | 3* · 5 · 9 · Never | 4/2 | I | exorcism.c exorcism_fails() vs DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL | NEW |
| `EXO_CLUES` | True-name clues | 2 · 3* · 4 | 3/2 | I | exorcism.c exorcism_clue() count | NEW |
| `LAY_TO_REST` | Lay to Rest | Allowed* · Disabled | 2/1 | I | sim_soul.c sim_can_lay_to_rest() | NEW |
| `GHOST_RITE` | Ghost rite | Off · On* | 2/1 | I | sim_soul.c sim_raise_ghost() | NEW |
| `GRIEF` | Grief | Short · Normal* · Long | 3/2 | I | sim_soul.c grieve() | NEW |
| `NOBODY_DIES` | Nobody dies | Off* · On | 2/1 | I | sim_soul.c / sim.c every DEATH_* path except DEATH_MONSTER | NEW |
| `POSSESSED_LOOK` | Possessed portrait intensity | Cute-creepy · Creepy* · Full horror | 3/2 | G | portrait.c pctx_init() creepy level, portrait_fx.c pf_status() | NEW |

**Pet** (9)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `PET_CLOCK` | Pet clock | Real time · Game time* · Only while watching | 3/2 | I | pet.c pet_tick_minute() caller | NEW |
| `PET_POOP` | Poop frequency | Low · Normal* · High | 3/2 | I | pet.c pet_tick_minute() | NEW |
| `PET_SICK` | Pet sickness | Off · On* | 2/1 | I | pet.c pet_tick_minute() sick roll | NEW |
| `PET_DEATH` | Pet death | Off* (runs away, comes back) · On | 2/1 | I | pet.c pass_away() | NEW |
| `PET_LINE` | Evolution line | Free* · Cute · Angel · Feral · Demon | 5/3 | I | pet.c evolve() | NEW |
| `PET_DISCIPLINE` | Discipline calls | Off · On* | 2/1 | I | pet.c needs_call() | NEW |
| `PET_LCD` | LCD palette | Classic green* · Pastel · Hellvile red · Greyscale · Backlit blue | 5/3 | G | pet_gfx.c LCD colour table | NEW |
| `OPT_PET_NOTIFICATIONS` | Pet alerts | Off · On (toast + beep)* · Badge only · Toast only | 4/2 | G | pet_scene.c / fp_common.c alert | EXT |
| `PET_BUFF` | Pet combat buff | Off · On* | 2/1 | I | pet.c pet_combat_buff() | NEW |

**Gore** (12)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `OPT_GORE` | Gore | Cute* · Brutal · Per weapon · Per mode · Cute citizens, brutal demons | 5/3 | I | actor.c combat_gore_style() | EXT |
| `OPT_BLOOD_COLOR` | Blood colour | Red* · Green · Rainbow · Confetti · Purple · Black ink · Glitter · Ectoplasm · Candy syrup | 9/4 | I | actor.c actor_gore() colour table | EXT |
| `OPT_CONFETTI_AMOUNT` | Confetti | Some · Lots* · Absurd · None | 4/2 | I | actor.c / mg_fx.c particle count | EXT |
| `GIBS` | Gibs | None · Few · Normal* · Lots | 4/2 | I | actor.c actor_gib() count | NEW |
| `GIB_LIFE` | Gib lifetime | 1 s · 10 s* · Until level end | 3/2 | G | actor.c gib_tick() | NEW |
| `CORPSES` | Demon corpses | Fade* · Stay | 2/1 | G | actor.c effect_tick() corpse timer | NEW |
| `DEATH_STYLE` | Demon death style | Per gore* · Balloon pop · Plush deflate · Dust poof · Explode | 5/3 | I | actor.c actor_gore() variant | NEW |
| `IMPACTS` | Bullet impacts | Auto* · Sparks · Stars · Hearts · Puffs | 5/3 | I | combat.c fire_hitscan() impact fx | NEW |
| `DMG_SCREEN` | Damage screen effect | Off · Cute heart cracks* · Brutal blood | 3/2 | G | combat_hud.c damage_direction() / health_panel() | NEW |
| `HITSTOP` | Kill hit-stop | Off · Short* · Long | 3/2 | G | combat.c combat_on_kill() | NEW |
| `JUMP_SCARES` | Jump scares | Off · On* | 2/1 | I | levels / story scare triggers | NEW |
| `BOSS_GORE` | Boss gore | Same as gore* · Always cute | 2/1 | I | actor.c combat_gore_style() for bosses | NEW |

**Movement** (7)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `SPRINT` | Sprint | Hold* · Toggle · Auto | 3/2 | G | combat.c read_controls() | NEW |
| `CROUCH` | Crouch | Hold · Toggle* | 2/1 | G | combat.c read_controls() | NEW |
| `ROLL_INPUT` | Dodge roll | Double-tap + button* · Button only · Double-tap only | 3/2 | G | combat.c read_controls() | NEW |
| `MOVE_FEEL` | Movement feel | Snappy · Normal* · Floaty | 3/2 | G | combat.c player_tick() accel/friction table | NEW |
| `AUTO_PICKUP` | Auto-pickup weapons | Off · On* | 2/1 | G | combat.c pickups_tick() | NEW |
| `USE_MODE` | Use / interact | Button* · Auto on bump | 2/1 | G | combat.c bump_doors() | NEW |
| `KNOCKBACK` | Knockback on you | Off · Low · Normal* | 3/2 | I | combat.c combat_hurt_player() | NEW |

**Camera** (17)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `OPT_CAMERA_DEFAULT` | Default camera | First person* · Third person | 2/1 | G | combat.c combat_start() | KEEP |
| `OPT_FOV` | Field of view | Narrow 70 · Normal 75* · Wide 90 · 60 · 80 · 100 · 110 | 7/3 | G | combat_hud.c focal_px(), combat_build_view() | EXT |
| `WEAPON_BOB` | Weapon bob | Off · Low · Full* | 3/2 | G | combat_hud.c weapon_view() | NEW |
| `WEAPON_SIDE` | Weapon position | Right* · Centre · Left · Follow handedness | 4/2 | G | combat_hud.c weapon_view() x offset; gene left_handed | NEW |
| `WEAPON_SIZE` | Weapon size | Small · Normal* · Big | 3/2 | G | combat_hud.c weapon_view() scale | NEW |
| `VIEW_KICK` | View kick | Off · Low · Full* | 3/2 | G | combat.c fire_weapon() pitch kick | NEW |
| `OPT_HEAD_BOB` | Head bob | Off · Low · Full* · Classic strong | 4/2 | G | combat.c update_camera() | EXT |
| `PITCH_MODE` | Pitch | Auto-aim only · Limited · Full* | 3/2 | G | combat.c update_camera() pitch clamp | NEW |
| `FP_HANDS` | Hands tinted by fur / gloves | Off · On* | 2/1 | G | combat_hud.c draw_paw() (skin shades or glove colour) | NEW |
| `TP_DIST` | 3rd-person distance | Close · Normal* · Far · Very far | 4/2 | G | combat.c update_camera() | NEW |
| `TP_HEIGHT` | 3rd-person height | Low · Normal* · High · Top-down-ish | 4/2 | G | combat.c update_camera() | NEW |
| `TP_OFFSET` | Shoulder offset | Centred · Small · Normal* · Wide | 4/2 | G | combat.c update_camera() | NEW |
| `OPT_SHOULDER` | Shoulder | Right* · Left · Auto (swaps near walls) | 3/2 | G | combat.c update_camera(), combat_scene.c | EXT |
| `TP_SMOOTH` | Camera smoothing | Off · Low · Normal* · High | 4/2 | G | combat.c update_camera() lerp | NEW |
| `TP_RECENTRE` | Auto-recentre | Off · Slow* · Fast | 3/2 | G | combat.c update_camera() | NEW |
| `TP_SELF` | Your citizen when the camera is close | Visible · Dither* · Hide | 3/2 | G | combat.c billboard flags; ps1 model.c dist2 cut | NEW |
| `TP_TURN` | Turning | Free look* · Tank · Face movement | 3/2 | G | combat.c read_controls() | NEW |

**Aim** (12)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `OPT_LOOK_SPEED` | Horizontal look speed | 1..10 (5*) | 10/4 | G | combat.c read_controls() | KEEP |
| `LOOK_SPEED_Y` | Vertical look speed | 1..10 (5*) | 10/4 | G | combat.c read_controls() | NEW |
| `OPT_INVERT_LOOK` | Invert Y | Off* · On | 2/1 | G | combat.c read_controls() | KEEP |
| `INVERT_X` | Invert X | Off* · On | 2/1 | G | combat.c read_controls() | NEW |
| `LOOK_ACCEL` | Look acceleration | Off · Low* · High | 3/2 | G | combat.c read_controls() hold ramp | NEW |
| `ADS_SENS` | Aim-down-sights sensitivity | 50% · 75%* · 100% | 3/2 | G | combat.c read_controls() | NEW |
| `OPT_AIM_ASSIST` | Aim slowdown | Off · Low · Medium* · High | 4/2 | G | combat.c aim_solution() | KEEP |
| `AIM_MAGNET` | Aim magnetism | Off · Low* · Medium · High | 4/2 | G | combat.c aim_solution() pull | NEW |
| `OPT_LOCK_ON` | Lock-on | Hold · Toggle* · Off | 3/2 | G | combat.c update_lock() | EXT |
| `LOCK_PRIORITY` | Lock-on priority | Nearest* · Closest to crosshair · Lowest HP · Biggest threat · Bosses first | 5/3 | G | combat.c find_target() score | NEW |
| `LOCK_BREAK` | Lock break distance | Short · Normal* · Long | 3/2 | G | combat.c update_lock() | NEW |
| `WEAKSPOT` | Weak-spot highlight | Off* · On | 2/1 | G | combat_hud.c lock_reticle() | NEW |

**Crosshair** (13)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `OPT_CROSSHAIR` | Shape | 8 today + chevron, T, brackets, triangle, diamond, flower, moon, fang, horns, cat face, bunny, pentagram, plus, ghost, bow, note (24) | 24/5 | G | combat_hud.c crosshair() | EXT |
| `OPT_CROSSHAIR_COLOR` | Colour | 6 today + 26 paints + rainbow + auto-contrast (34) | 34/6 | G | combat_hud.c crosshair_color() | EXT |
| `XH_SIZE` | Size | Small · Normal* · Big | 3/2 | G | combat_hud.c crosshair() | NEW |
| `XH_THICK` | Thickness | 1 px* · 2 px | 2/1 | G | combat_hud.c crosshair() | NEW |
| `XH_GAP` | Gap | 0 · 1* · 2 · 3 | 4/2 | G | combat_hud.c crosshair() | NEW |
| `XH_OUTLINE` | Outline | Off · Dark* · Light | 3/2 | G | combat_hud.c crosshair() | NEW |
| `XH_DOT` | Centre dot | Off* · On | 2/1 | G | combat_hud.c crosshair() | NEW |
| `XH_SPREAD` | Dynamic spread | Off · On* | 2/1 | G | combat_hud.c crosshair() from weapon spread | NEW |
| `XH_HITFLASH` | Hit-confirm flash | Off · White · Pink* · Red | 4/2 | G | combat_hud.c crosshair_color() | NEW |
| `XH_ENEMY` | Colour change over enemies | Off · On* | 2/1 | G | combat_hud.c crosshair_color() | NEW |
| `XH_LOCK` | Lock-on reticle | Ring · Heart* · Brackets · Skull · Paw | 5/3 | G | combat_hud.c lock_reticle() | NEW |
| `XH_SCOPE` | Scope overlay | Classic · Cute heart lens* · Minimal | 3/2 | G | combat_hud.c scope_overlay() | NEW |
| `XH_THIRD` | Crosshair in third person | Always* · Only when aiming · Off | 3/2 | G | combat_hud.c combat_draw_hud() | NEW |

**HUD** (12)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `OPT_HUD_LAYOUT` | Layout | Cute* · Classic · Minimal · Classic status bar · Off | 5/3 | G | combat_hud.c combat_draw_hud(), classic_bar() | EXT |
| `HUD_FACE` | HUD face | Off · Small · Big* | 3/2 | G | combat_hud.c face_badge() | NEW |
| `HUD_HEALTH` | Health display | Bar · Hearts* · Number · Face only | 4/2 | G | combat_hud.c health_panel() | NEW |
| `HUD_AMMO` | Ammo display | Candy pill* · Number · Bar · Off | 4/2 | G | combat_hud.c ammo_panel() | NEW |
| `OPT_DAMAGE_NUMBERS` | Damage numbers | Off · Numbers* · Hearts · Stars · Words | 5/3 | G | combat_hud.c damage_numbers() | EXT |
| `OPT_HIT_MARKERS` | Hit markers | Off · Visual + sound* · Visual only | 3/2 | G | combat_hud.c + audio_sfx() | EXT |
| `DMG_DIRECTION` | Damage direction | Off · Arrows* · Edge glow | 3/2 | G | combat_hud.c damage_direction() | NEW |
| `LOW_HP` | Low-health warning | Heartbeat + vignette* · Heartbeat · Vignette · Off | 4/2 | G | combat_hud.c health_panel(), audio_sfx() | NEW |
| `PICKUP_MSG` | Pickup messages | Full* · Icons · Off | 3/2 | G | combat.c combat_toast() | NEW |
| `BOSS_BAR` | Boss bar | Big* · Small · Off | 3/2 | G | combat_hud.c boss_bar() | NEW |
| `KILL_COUNTER` | Kill counter | Off* · On | 2/1 | G | combat_hud.c top_right() | NEW |
| `LEVEL_TIMER` | Level timer | Off* · On | 2/1 | G | combat_hud.c top_left() | NEW |

**Weapons** (6)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `AUTO_SWITCH` | Auto-switch on pickup | Off · If better* · Always | 3/2 | G | combat.c pickups_tick() -> combat_select_weapon() | NEW |
| `AUTO_RELOAD` | Auto-reload | Off · On* | 2/1 | G | combat.c weapon_tick() -> start_reload() | NEW |
| `SKIP_EMPTY` | Skip empty weapons | Off · On* | 2/1 | G | combat.c combat_next_weapon() | NEW |
| `WELD_GAME` | Welding minigame | Easy · Normal* · Hard · Auto-weld | 4/2 | I | weld game scene timing | NEW |
| `WEAPON_NAMES` | Weapon names | Funny* · Plain | 2/1 | G | weld.c weld_name() | NEW |
| `CHARM_DANGLE` | Charm dangle | Off · On* | 2/1 | G | combat_hud.c weapon_view() | NEW |

**Audio** (15)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `MASTER_VOL` | Master volume | 0..10 (10*) | 11/4 | G | audio.c mixer gain (multiplies music/sfx/voice) | NEW |
| `OPT_MUSIC_VOL` | Music volume | 0..10 (7*) | 11/4 | G | audio.c, song_player.c | KEEP |
| `OPT_SFX_VOL` | Sound volume | 0..10 (8*) | 11/4 | G | audio.c audio_sfx() | KEEP |
| `OPT_VOICE_VOL` | Voice volume | 0..10 (8*) | 11/4 | G | audio.c voice_tick() | KEEP |
| `UI_VOL` | UI sound volume | 0..10 (8*) | 11/4 | G | audio.c audio_sfx() for SFX_UI_* ids | NEW |
| `FOOTSTEP_VOL` | Footstep volume | 0..10 (6*) | 11/4 | G | audio.c audio_sfx() for footstep ids (deluxe footstep sounds) | NEW |
| `STEREO` | Output | Stereo* · Mono · Swapped | 3/2 | G | gba/src/audio.c, ps1/src/spu.c channel pan | NEW |
| `PS1_REVERB` | Reverb (PS1) | Off · Room* · Studio · Hall · Space echo | 5/3 | G | ps1/src/spu.c reverb work area 0x7e000 (unused today) | NEW |
| `INVASION_MUSIC` | Invasion music | Metal* · Cute remix · Keep location theme | 3/2 | G | audio.c audio_music() song pick | NEW |
| `OPT_VOICE_BABBLE` | Babble style | Off · Bleeps* · Syllables · Mumbles · Kazoo | 5/3 | G | audio.c audio_voice_char() | EXT |
| `BABBLE_PITCH` | Babble pitch offset | -2 · -1 · 0* · +1 · +2 | 5/3 | G | audio.c voice_pitch() | NEW |
| `BABBLE_SPEED` | Babble speed | Slow · Normal* · Fast | 3/2 | G | audio.c voice_tick() | NEW |
| `TEXT_BLIPS` | Text blips (babble off) | Off · Soft* · Classic | 3/2 | G | ui_dialog.c dlg_update() -> audio_sfx() | NEW |
| `DUCK_MUSIC` | Duck music under dialogue | Off · On* | 2/1 | G | audio.c music gain while dlg active | NEW |
| `SQUEAKS` | Squeaky plush sounds | Off · On* | 2/1 | G | audio.c footstep/hurt variants (plush material) | NEW |

**Display** (20)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `OPT_LCD_FILTER` | LCD filter | Off · Pet screen* · Everything · Grid only · Ghosting | 5/3 | G | pet_gfx.c LCD grid; value 2+ full-screen grid pass in plat_end_frame() (FIXED: wired today to nothing) | FIX |
| `GBA_SCREEN` | GBA screen profile | Original (brighten) · SP frontlit · SP backlit* · DS / Micro · Game Boy Player · Emulator | 6/3 | G | gba/src/video.c video_load_palette() gamma LUT | NEW |
| `BRIGHTNESS` | Brightness | -2 · -1 · 0* · +1 · +2 | 5/3 | G | gba video_load_palette(); ps1 gfx.c palette_tables() | NEW |
| `SATURATION` | Saturation | Greyscale · Pastel · Normal* · Vivid | 4/2 | G | same palette LUT | NEW |
| `OPT_CAMERA_FILTER` | Camera filter | None* · Storybook · VHS · Game Pal · Sepia · Nightvision · Polaroid · Noir · Thermal | 9/4 | G | palette LUT per scene (FIXED: wired today to nothing) | FIX |
| `PS1_DITHER` | Dithering (PS1) | Off · On* | 2/1 | G | ps1/src/gfx.c draw mode dither bit | NEW |
| `PS1_SCREEN_X` | Screen X offset (PS1) | -8..+8 (0*) | 17/5 | G | ps1/src/gfx.c gfx_init() display area | NEW |
| `PS1_SCREEN_Y` | Screen Y offset (PS1) | -8..+8 (0*) | 17/5 | G | ps1/src/gfx.c gfx_init() display area | NEW |
| `DRAW_DIST` | 3D draw distance | Near · Normal* · Far | 3/2 | G | ps1/src/view3d.c cast(maxcells); gba r3d.c column depth | NEW |
| `FOG` | Fog | Off · On* | 2/1 | G | r3d fog table | NEW |
| `CROWD` | Crowd density | Low · Normal* · High | 3/2 | G | overworld_npc.c spawn cap | NEW |
| `PORTRAIT_ANIM` | Portrait animation | Full* · Blink only · Static | 3/2 | G | portrait.c phase_of() (Static = phase 0, saves CPU) | NEW |
| `OPT_WINDOW_SKIN` | Window skin | 12 today + Moss, Bone, Candy Corn, Ocean, Sunset, Cloud Nine, Plaid, Stars (20) | 20/5 | G | ui.c ui_window() | EXT |
| `PARTICLES` | Particles | Low · Normal* · High | 3/2 | G | creator sparks, mg_fx.c, concert_gfx.c counts | NEW |
| `PALCYCLE` | Palette-cycle effects | Off · On* | 2/1 | G | portrait glow / glitter materials (§4.3) | NEW |
| `OPT_TITLE_THEME` | Title theme | 6 today + Plush Parade, Graveyard Picnic, Festival Night, Doomogachi LCD | 10/4 | G | fp_title.c | EXT |
| `OPT_CURSOR` | Cursor | 6 today + Bone, Candy, Bat, Ghost, Bell, Fish | 12/4 | G | ui.c ui_cursor_glyph() | EXT |
| `OPT_FONT_STYLE` | Font style | Normal* · Bold · Shadowed · Rounded · Spooky | 5/3 | G | ui.c text_run() | EXT |
| `OPT_CLOCK_24H` | Clock | 12-hour* · 24-hour | 2/1 | G | town.c clock text | KEEP |
| `OPT_UI_BOUNCE` | UI bounce | Off · Subtle · Boingy* | 3/2 | G | ui.c | KEEP |

**Access** (24)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `OPT_COLORBLIND` | Colour-blind aid | Off* · Protan · Deutan · Tritan · Achromat · High contrast | 6/3 | G | palette LUT (daltonise remap) in video_load_palette()/palette_tables() (FIXED: wired today to nothing) | FIX |
| `CB_STRENGTH` | Colour-blind strength | Low · Medium* · High | 3/2 | G | same LUT | NEW |
| `CB_DISTRICT` | District colour coding | Pink/purple* · Blue/orange · Add shapes | 3/2 | G | ui.c district badges, fp_island_gfx.c | NEW |
| `CB_ENEMY_OUTLINE` | Enemy outline highlight | Off* · On | 2/1 | G | r3d billboard outline flag | NEW |
| `CB_COLOR_NAMES` | Colour names in pickers | Off · On* | 2/1 | G | creator_draw.c swatch picker caption (doomo_paint_names) | NEW |
| `CB_POSSESSED_ICON` | Possessed icon | Off · On* | 2/1 | G | fp_island.c / ui.c ui_portrait() badge | NEW |
| `OPT_SUBTITLES` | Subtitles | Off · On* · With speaker name | 3/2 | G | ui_dialog.c dlg_draw() speaker tag; concert lyric captions (FIXED: wired today to nothing) | FIX |
| `TEXT_SIZE` | Text size | Normal* · Large dialogue | 2/1 | G | ui_dialog.c dlg_box() with ui_text_big() | NEW |
| `HOLD_TOGGLE` | Holds become toggles | Off* · On | 2/1 | G | input.c: btn_down() latch for flagged actions | NEW |
| `MASH_ASSIST` | Button-mash assist | Off* · On | 2/1 | G | mg.c mash counters (hold counts as mash) | NEW |
| `RHYTHM_WINDOW` | Rhythm timing window | Normal* · Wide · Very wide · Auto-play | 4/2 | G | exorcism prayer, concert, cooking judge windows | NEW |
| `SHOOTER_SPEED` | Shooter game speed | 100%* · 85% · 70% · 50% | 4/2 | G | combat.c combat_tick() dt scale | NEW |
| `MENU_REPEAT` | Menu repeat delay | Slow · Normal* · Fast | 3/2 | G | input.c REPEAT_DELAY / REPEAT_RATE | NEW |
| `MENU_WRAP` | Menu wrap-around | Off · On* | 2/1 | G | ui.c ui_list_update() | NEW |
| `CONFIRM_BTN` | Confirm button | A / Cross* · B / Circle | 2/1 | G | input remap: menu context swap (§6.3) | NEW |
| `REDUCE_MOTION` | Reduce motion | Off* · On | 2/1 | G | portrait idle/bounce amplitude, ui.c bounce, overworld bob | NEW |
| `OPT_REDUCE_FLASH` | Photosensitive mode | Off* · On | 2/1 | G | existing consumers + palette-cycle off + glitch aura frozen | EXT |
| `OPT_SCREEN_SHAKE` | Screen shake | Off · Low · Full* · Classic | 4/2 | G | combat.c combat_shake(), mg.c, dream_gfx.c | EXT |
| `ARACHNO` | Arachnophobia mode | Off* · On | 2/1 | G | catalog redirect: spider parts -> bunny; actor sprite swap | NEW |
| `VAPE_RESKIN` | Vape reskin (bubble tea) | Off* · On | 2/1 | G | text substitution table + venue sprite | NEW |
| `ONE_HANDED` | One-handed controls | Off* · Left hand · Right hand | 3/2 | G | input remap preset (§6.3) | NEW |
| `NO_DMG_ASSIST` | No-damage assist | Off* · On | 2/1 | I | combat.c combat_hurt_player() | NEW |
| `SKIP_COMBAT` | Skip story combat | Off* · On | 2/1 | I | story.c encounter start -> auto-win | NEW |
| `CHORD_WINDOW` | GBA chord window (SELECT+A = X) | 2 · 4* · 8 frames | 3/2 | G | input.c synth X/Y: SELECT may precede A by N ticks | NEW |

**Text** (8)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `AUTO_ADVANCE` | Auto-advance | Off* · Short · Normal · Long | 4/2 | G | ui_dialog.c dlg_update() | NEW |
| `TEXT_LOG` | Text log (16 lines) | Off · On* | 2/1 | G | ui_dialog.c ring of last lines in scene scratch | NEW |
| `BOX_POS` | Box position | Bottom* · Top · Auto | 3/2 | G | ui_dialog.c dlg_box() | NEW |
| `SPEAKER_PORTRAIT` | Speaker portrait | Off · On* · Big | 3/2 | G | ui_dialog.c has_portrait() | NEW |
| `NAME_LABELS` | Name labels | Name* · Nickname · Name + pronouns | 3/2 | G | ui_dialog.c speaker tag; creator nickname table | NEW |
| `NUMBER_FMT` | Number format | 1,000* · 1.000 · 1 000 | 3/2 | G | ui.c ui_textf() %n | NEW |
| `DATE_FMT` | Date format | Day-month* · Month-day | 2/1 | G | sim.c sim_date() | NEW |
| `CHATTER` | Overworld chatter | Bubbles* · Thought clouds · Icons only · Off | 4/2 | G | overworld_npc.c | NEW |

**Controls** (14)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `OPT_CONTROL_SCHEME` | Scheme | Classic · Modern* · Southpaw · One-hand · Custom | 5/3 | G | combat.c read_controls(); Custom = remap table | EXT |
| `STRAFE_MODE` | Strafe | L/R strafe* · Hold L + D-pad · Auto-strafe | 3/2 | G | combat.c read_controls() | NEW |
| `TURN_ACCEL` | D-pad turn acceleration | Off · Low* · High | 3/2 | G | combat.c read_controls() | NEW |
| `SOFT_RESET` | Soft reset (A+B+START+SELECT) | Off · On* | 2/1 | G | game.c frame hook -> scene_switch(SCENE_TITLE) | NEW |
| `TURBO_FIRE` | Turbo fire | Off* · On | 2/1 | G | combat.c read_controls() | NEW |
| `PROMPTS` | Button prompts | Auto* · GBA letters · PS1 symbols | 3/2 | G | ui.c button_cap() | NEW |
| `GBA_SLEEP` | Sleep mode (GBA) | Off · L+R+SELECT* | 2/1 | G | gba/src/main.c: stop mode via keypad IRQ | NEW |
| `GBA_SWAP_LR` | Swap L and R (GBA) | Off* · On | 2/1 | G | gba/src/main.c read_pad() | NEW |
| `PS1_ANALOG` | Analog sticks (PS1) | Off* · Move + look · Swapped | 3/2 | G | ps1/src/pad.c pad_analog() -> new plat analog API -> combat.c read_controls() | NEW |
| `PS1_DEADZONE` | Stick deadzone (PS1) | 16 · 32 · 48* · 64 · 80 | 5/3 | G | ps1/src/pad.c threshold (48 today) | NEW |
| `PS1_L2R2` | L2 / R2 (PS1) | Look up/down* · Lock-on + roll · Weapon swap · Camera | 4/2 | G | remap shooter context | NEW |
| `PS1_VIBRATION` | Vibration (DualShock) | Off · Low · High* | 3/2 | G | ps1/src/pad.c actuator bytes from combat_shake() | NEW |
| `PS1_CARD_SLOT` | Memory card slot (PS1) | Slot 1* · Slot 2 | 2/1 | G | ps1/src/memcard.c bu00 / bu10 | NEW |
| `REMAP_PROFILE` | Button layout profile | Default* · Custom 1 · Custom 2 · Custom 3 | 4/2 | G | input.c remap tables (§6.3) | NEW |

**Cheats** (17)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `OPT_CHEAT_BIG_HEAD` | Head size | Normal* · Big · Tiny | 3/2 | I | portrait.c setup_*(): head_size bias + cache key (FIXED: wired today to nothing) | FIX |
| `OPT_CHEAT_BRUTAL_RESIDENTS` | Brutal residents | Off* · On | 2/1 | I | sim_social.c argue/rival rolls x2 + persona speech bias in sim_care.c tone (FIXED) | FIX |
| `OPT_CHEAT_PAPER_MODE` | Paper / plush mode | Off* · Paper · Plush | 3/2 | I | portrait.c pctx_init(): material override (flat paper / plush seams) + cache key (FIXED) | FIX |
| `OPT_CHEAT_ALL_GHOSTS` | Everyone's a ghost | Off* · On | 2/1 | I | portrait.c render status override RES_GHOST + cache key (FIXED) | FIX |
| `INF_AMMO` | Infinite ammo | Off* · On | 2/1 | I | combat.c weapon_has_ammo() | NEW |
| `ONE_HIT` | Boop of Doom (one-hit kills) | Off* · On | 2/1 | I | actor.c actor_damage() | NEW |
| `MOON_JUMP` | Moon jump | Off* · On | 2/1 | I | combat.c player_tick() jump | NEW |
| `CIT_SIZE` | Citizen size | Pocket · Normal* · Giant | 3/2 | I | portrait.c pb_layout() scale (chibi + fighter) | NEW |
| `FURRY_ISLAND` | Furry Island | Off* · On | 2/1 | I | res_look() view override: species -> nearest critter-folk (render only, saves untouched) | NEW |
| `PARTY_HATS` | Party hats on demons | Off* · On | 2/1 | I | actor.c actor_image() overlay | NEW |
| `NO_CORRUPTION` | No corruption | Off* · On | 2/1 | I | sim_soul.c sim_corrupt() | NEW |
| `RAINBOW` | Rainbow palette cycling | Off* · On | 2/1 | I | palette LUT rotation in plat_end_frame() | NEW |
| `ONE_BIT` | 1-bit mode | Off* · On | 2/1 | I | palette LUT threshold | NEW |
| `SQUEAK_ALL` | Squeak everything | Off* · On | 2/1 | I | audio.c audio_sfx() substitution | NEW |
| `BIG_EARS` | Big ears and tails | Off* · On | 2/1 | I | res_look() view override: ear_size/tail_len max | NEW |
| `FACE_SHUFFLE` | Face shuffle (daily, cosmetic) | Off* · On | 2/1 | I | res_look() view override seeded by day | NEW |
| `UWU` | uwu-speak filter | Off* · On | 2/1 | I | ui_dialog.c copy_str() filter | NEW |

**Creator** (9)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `CR_MODE` | Creator mode | Simple · Advanced* · Everything | 3/2 | G | creator.c row filter (§5.4) | NEW |
| `CR_STAGE` | Preview stage | Room* · Plain · Hillvile · Hellvile · Sky | 5/3 | G | creator_draw.c stage_colors() | NEW |
| `CR_SPIN` | Turntable auto-spin | Off* · On | 2/1 | G | creator.c preview | NEW |
| `CR_EXPR` | Preview cycles expressions | Off · On* | 2/1 | G | creator.c expr_i timer | NEW |
| `CR_LOCKED` | Locked options | Show silhouettes* · Hide | 2/1 | G | creator.c cr_value_locked() picker filter | NEW |
| `CR_THEME` | Default randomiser theme | Cute* · Spooky · Goth · Pastel · Punk · Royal · Furry · Demon · Angelic · Chaos | 10/4 | G | creator.c randomise (§5.4) | NEW |
| `CR_SWATCH` | Swatch grid | 8 columns* · 16 columns | 2/1 | G | creator_draw.c swatch grid | NEW |
| `CR_CAPACITY` | Show wardrobe capacity | Off · On* | 2/1 | G | creator_draw.c header meter (§1.5) | NEW |
| `CR_SYMMETRY` | Symmetry | Symmetric* · Independent | 2/1 | G | creator.c L/R value filtering (§5.5) | NEW |

**System** (3)

| id | option | values (`*` = default) | n/bits | scope | wired into | status |
|---|---|---|---|---|---|---|
| `SETTINGS_SCOPE` | Settings follow | This cartridge / card* · Each island | 2/1 | G | options.c store selection (§6.2) | NEW |
| `SAVE_VERIFY` | Verify after saving | Off · On* | 2/1 | G | save.c save_game() read-back | NEW |
| `DEV_STATS` | Show FPS and RAM | Off* · On | 2/1 | G | game.c overlay (gba mailbox fps, ps1 hblank timer) | NEW |


---

## 7. Risks and how to test them

| # | risk | likelihood / impact | mitigation | test |
|---|---|---|---|---|
| R1 | **The refactor is wide:** about 136 look-field sites and about 300 identity-field sites move to accessors | high / medium | Remove the plain members so the compiler finds every site. Wave 2a is a pure refactor with no new features, and must reproduce today's fingerprints and pixels exactly | `fingerprints.txt` (unchanged) + `portrait_golden_v1.txt` + every existing test green before any feature lands |
| R2 | **Save migration loses or garbles citizens** | medium / severe | The v1 reader is kept forever. A v1 save is not overwritten until the v2 save verifies. Migration is a pure function of (v1 bytes, ROM) | 6 v1 fixtures (§3.3) loaded into the expected town FNV, then saved as v2, reloaded, equal. Fuzz: 10k random v1 bit streams must reject or load and sanitise, never crash (ASan/UBSan, `SAN=1`) |
| R3 | **Determinism drift** between native, GBA and PS1 (a new RNG stream, the codec, scripts) | low / severe (Random-Island saves are deltas against the generator) | Integer-only, byte arrays, no struct punning; a separate RNG stream for new fields | legacy-projection fingerprints vs today's golden; `fingerprints_v2.txt` + `fingerprints_dlx.txt`; the new harness `fp` command on mGBA and PCSX-Redux diffed against native in CI (§3.5) |
| R4 | **PS1 RAM:** code growth beats the estimate | medium / high | CD roster (-80 KB), BPE texts, part scripts instead of C. Per-subsystem size caps in CI (`ps1/tools/memreport` budget: portrait + parts + gene at most 160 KB) | `elf2exe` headroom check (at least 128 KB) per PR; `ovl_plan.py` group sizes at most the slot |
| R5 | **CD roster:** latency or failure | medium / medium | Sequential `roster_iter()` for save, load and town_new; a 2-sector cache; the title prefetch; the GBA/native path unchanged; option to fall back to the RAM roster at build time (`-DDOOMO_ROSTER_IN_RAM`) | harness: save/load timings (at most +0.4 s), the creator copy-from-premade picker at 10 entries a second |
| R6 | **GBA EWRAM:** another team spends the reclaimed bytes, or the CI pass shrinks something this design relies on | medium / medium | The design is self-funding (-20 B net) and touches nothing the CI pass is shrinking. Land it after the CI pass | `memreport.py` EWRAM free at least the value before the PR |
| R7 | **Render cost creep** makes UI fps drop under 20 on the GBA | medium / high | `DL_STEPS` 7, slice cap 0.25M, `LOD_SCRUB`, decals inside parent part ids | mGBA bench thresholds (§4.9); harness `creator_fps.txt` and an island script with 40 chibis, both at least 20 fps |
| R8 | **Part-id saturation** (253) with heavily layered looks | low / visual | `pc_part_inherit()` for decals | 64 "everything on" looks, at most 200 parts |
| R9 | **The worst-case save** grows to about 86.6 KB, and maxed chunks can exceed slot 1 | medium / medium (clean failure) | `save_measure()` at commit, Simplify, the "Scrapbook space" meter, XTRA capped at 4 KB | `test_save`: 1000 random custom + 2048 edges at most 88 KB − 2 KB margin; a new "+ full XTRA + full song book" case must fail cleanly with "too big" and keep the old save, like today's slot-0 case |
| R10 | **The heap fills up** on the GBA on custom-heavy empty islands | high on those islands / low | Base-from-seed means most citizens need 0 B; Bake/Simplify; the capacity meter; every R-tier feature still works with a full heap | `test_creator`: fill the heap, then the commit offers Simplify, the in-record look is kept, and the pixel diff covers only deluxe parts |
| R11 | **Too many choices.** The UX drowns | medium / high | Simple/Advanced/Everything, sections, jump/search, presets, themed randomiser | the GBA navigation test: every row reachable in at most 12 presses from the tab start (scripted harness walk); playtest checklist |
| R12 | **Age and romance safety:** custom kids or teens slip through | low / severe | `age_of()` in `sim_romance_ok()` for every citizen; babies forced to baby; not an option | `test_sim`: every pairing of (premade or custom) x (baby, kid, teen, adult, elder) |
| R13 | **Linked colours confuse players** ("I changed the hair and the brows changed") | medium / low | Chips say "Hair (Pink)"; "Unlink" in the picker copies the paint into a free accent | creator unit test of the allocator: reuse, free, replace, unlink |
| R14 | **Cheat view overrides** leave stale cached portraits | low / visual | View epoch in every cache key, plus `portrait_cache_reset()` | toggle each render cheat, then every cached key differs |
| R15 | **Options regress existing behaviour** | low / medium | Append-only values; the defaults equal today's behaviour | a "defaults" golden: a native island run for 7 sim days with default options gives the same town FNV before and after the options change |
| R16 | **Conflicting edits with the active CI pass** (`portrait_gfx.c`, `portrait_int.h` are being edited now) | high / low | Start after the CI pass lands; wave 2a rebases | - |

**Test inventory added:**

- `test_gene.c`: layout coverage, get/set round trips at min/max/random, neighbour isolation,
  sanitize.
- `test_xtra.c`: insert, update, delete, full, NICK migration.
- `test_save.c` additions: v1 fixtures, migration, worst case + margin, chunk round trips,
  `look_gen`.
- `test_resident.c` additions: legacy projection, v2 and deluxe goldens, themed randomiser
  respects pins and locks.
- `test_portrait.c` additions:
  - `MAXOPT` 64
  - distinctness of every new value (R and D, scripts included)
  - pixel parity with new fields at auto
  - slice cost
  - part count
  - LOD: chibis unchanged by sub-pixel fields
- `test_creator.c`: one row per field; navigation reachability; undo log; heap full; colour
  allocator; symmetry filter.
- `test_options.c`: schema bits fit 80 B / 32 B; append-only (legacy default values unchanged);
  remap is a bijection; lockout reset; migration of `opt[]` into the store.
- `test_sim.c`: age gate, workplace from the gene equals the old `job_venue_scan()` for all 1000
  premades (built offline), baby heredity distribution.
- Harness:
  - GBA: `fp`, `creator_fps`, `island_fps`, `coresave2` (v1 → v2 across two runs)
  - PS1: `fp`, the CD-roster save/load timing, a model screenshot set (8 "everything on" looks ×
    5 views)
- CI:
  - `check-gen` for `gene_layout.c`, `parts_data.c`, `roster_data.c` and `ROSTER.BIN`
  - `check_options.py`
  - memory budgets on both platforms

---

## 8. Rollout (each wave ships green on GBA, PS1 and native)

| wave | contents | exit criteria |
|---|---|---|
| **0 (prep)** | Capture the goldens from HEAD: `roster_v1_fp.txt`, `portrait_golden_v1.txt`, v1 save fixtures. Add the harness `fp` commands. Add `check_options.py` and wire the 8 dead options (FIX). PS1: BPE bios and jobs (-23.7 KB) | all tests green, PS1 headroom +23 KB |
| **1 (options v2)** | `DoomoSettings` + settings store (GBA settings sector, PS1 settings file), `optx`, the 237 NEW / 30 EXT options with their consumers, the remap contexts and screen, the SCENE_OPTIONS overlay group | `check_options.py` clean; defaults golden unchanged |
| **2a (codec, no features)** | `gene.def`, the codec, `DoomoResident` v2, accessors, the wide `DoomoLook`, `DoomoPremade` + roster tool v2, save v2 + the v1 reader, XTRA (with the NICK migration), the `sim_s.job` removal. PS1: the CD roster | fingerprints and pixels identical to wave 0; migration fixtures pass; EWRAM -20 B; PS1 headroom about +80 KB |
| **2b (R tier)** | the 57 R fields: painters, palette refs, part VM, `DL_STEPS` 7, `LOD_SCRUB`, generator stream 2, roster authoring of the new fields, creator sections, modes, jump, pins, colour allocator, undo log, PS1 face texture + `model.c` recipes, the age gate | bench budgets met; creator ≥ 20 fps; new goldens |
| **3 (deluxe + catalog)** | the 238 D fields (`gen_deluxe`, scripts), +298 catalog values, presets, themed randomiser, heredity, unlocks (U01-U78), Bake/Simplify, `save_measure()` | coverage tests; heap tests; PS1 headroom ≥ 128 KB |
| **4 (systems)** | Look Book (XTRA type 2) and Look Password, the PS1 3D creator turntable, chibi ratio, extra arms / buddy head / belly mouth, +24 species, photo mode | per-feature |

**Checklist for each future addition** (replaces map §9):

| addition | needs |
|---|---|
| a value in a list | name + part script (+ an unlock) + premade or preset coverage. No RAM, no save change |
| a deluxe field | a `deluxe.def` line + a painter or script + a row + a `gen_deluxe` weight. No RAM, no save change |
| an R field | the 2 reserved bits (no save change), or a gene layout v3 with a save version bump |
| an option | a schema row + strings + a consumer (CI-enforced) |

*Written read-only against the repo. Calculators: `scratchpad/custom/gen/schema.py` and
`options.py` (outputs in `gen/out/`).*
