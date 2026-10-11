# DOOMOGACHI customisation, design B: fat look, thin RAM

> The user asked: *"make sure you add a shitton of options, and face/body customization"*. Earlier asks:
> *"immense customization"*, *"extremely complex customization options"*, *"an immense amount of options for everything and anything"*.
> The task covers both platforms: the GBA and the PS1 each get their own memory plan, third-person plan, save plan
> and options below. The PS1 is not treated as a port afterthought.
>
> **Architecture angle B.** Grow the stored citizen record far past 96 bytes so that every field is one plain byte, and
> pay for it by moving RAM out of the places that hold redundant data. The repo was only read. Every number in this
> document is measured, or is derived by a script, against commit `43af439` ("Docs: GBA/PS1 memory tables after the
> overlay + EWRAM pass"); the four commits that followed up to HEAD `ae10664` were re-checked and change the EWRAM
> baseline by 48 bytes (2.1). The scripts live in `scratchpad/custom/growB/` and are listed in the appendix.
>
> The relayed user request that triggered this run asks for a cheeky cartoon devil in the Red-Guy-from-*Cow-and-Chicken*
> mould, with Hellvile's dungeon-punk gear. That is a character brief, not a systems brief, but section 1.10 shows
> that this design builds him from stock fields with no extra storage, inside the art direction's toy-like rule.

---

## 0. Summary

**The idea in one sentence.** A citizen's *logical* record grows from 96 B to **432 B**: a 48 B "hot" record plus a
**384 B `DoomoLook`** in which every wishlist field is its own byte (353 fields and 31 reserved bytes). Only the hot
half is stored in RAM for all 1,001 records. The look is never stored for citizens whose look can be recomputed (the
ROM roster or the seeded generator). Looks the player made or changed are stored as packed deltas in one 33.5 KB heap,
and those same bytes are what the save writes. A 12-line (GBA) or 16-line (PS1) cache pages decoded looks in on demand.

| | today | design B | change |
|---|---:|---:|---|
| Logical record per citizen | 96 B | 432 B (48 hot + 384 look) | 4.5x the room. 300 new look fields (1,645 new drawable options), with no tiers and no "deluxe" second class |
| RAM per **untouched** citizen | 96 B | 48 B + 3 B bookkeeping | Looks of premade and random citizens are recomputed, not stored |
| RAM per citizen with a 5-field makeover | 96 B | 51 B + an 11 B blob | |
| RAM per fully hand-made citizen | 96 B | 51 B + a 31.5 B blob (average with legacy detail only), 64 B with 20 new fields set | |
| **GBA EWRAM free** | **7,680 B** | **12,256 B** | **+4,576 B** after everything below, including the heap, the cache and the bigger options arrays |
| GBA IWRAM | 6,460 B left for the 6 KB stack | unchanged | Rule: no new IWRAM code. New painters are THUMB in ROM and call the existing `P_FAST` loops |
| GBA ROM | 2.43 MB | about 2.6 MB | Part scripts and code are in ROM. The packed roster saves 31 KB of ROM |
| **PS1 RAM below the stack top** | **142,528 B (139.2 KB)** | **131,745 B (128.7 KB)** | **-10,783 B**. That leaves 66 KB above the 64 KB stack reserve and still meets the integration target of 128 KB headroom. The PS1 pays for about 112 KB of new code and art (145 KB before the part scripts are LZ-packed) by packing the roster, BPE-coding the bios and moving SFX PCM into SPU RAM |
| Save slot 1 worst case | 58.6 KB measured (1000 random custom) | at most 89,606 B proven bound (limit 90,112) | The heap size is set by this bound (section 2.5) |
| Options | 57 (8 wired to nothing) | **373 listed**: 311 new, 62 extended, and all 8 dead options wired. Global settings are no longer per island | Every option names its consumer and its test probe (section 6) |

**Where the RAM comes from** (bytes, both platforms unless marked):

1. Hot/cold split of `doomo_town.res[1000]` + `player`: 96,096 B becomes 48,048 B, which frees **48,048 B**. The cold
   store spends 42,064 B of that (GBA) or 43,632 B (PS1): 34,304 B of heap, 2,036 B of directory, 1,020 B of
   versions and the decode cache.
2. PS1 only, roster packed. `doomo_roster_premade` is 96,000 B of RAM on the PS1. Packed per citizen with the v2 widths
   it is 62,938 B plus 2,000 B of offsets, measured on the real `core/gen/roster_data.c`. That saves **31,062 B**.
3. PS1 only, roster bios and jobs coded with byte-pair encoding (BPE). That keeps random access per string. They go
   from 52,978 B to 29,228 B including the pair tables (measured), which saves **23,350 B** after the decoder.
4. PS1 only, `doomo_sfx_pcm_data` (47,793 B of RAM). It is pre-encoded to ADPCM as `SFX.BIN` on the disc and uploaded
   to SPU RAM at boot. The core only reaches it through `audio_sfx_pcm()`. This saves **43,413 B** after a 2,880 B
   jukebox wave table and the loader.
5. PS1 only, part scripts (the new art) are stored LZ-packed per block, which saves about 45 KB against raw.

**In.** All 300 per-citizen fields of the wishlist's C and D tiers, and all 21 of its append-only list growths (306
new values in existing lists). A colour-tone system multiplies every colour field by 3 (96 paints), and every new
field sets an "auto" value. 22 of the 23 creator tools are in, as are 373 options with button remapping. The palette
filters cover brightness, contrast, gamma, saturation, colour-blind aids, camera filters and the LCD filter. The PS1
third person shows every face, hair and hat feature in 3D through a head impostor.

**Cut**, with reasons in 1.9 and 6.6: the 8 extra natural fur tones (the palette is full; tones replace them), island
dyes (the GBA has one 256-colour palette for Mode 4), the real 3D model in the PS1 creator turntable, fully custom
situational outfits (a preset index is kept), and 98 wishlist options. Those options need new hardware drivers or
systems, or they were merged into other options.

---

## 1. Data layout

### 1.1 Why not just store a fatter record for every citizen

| Layout of every citizen in RAM | bytes per citizen | x1001 | change vs 96 B | GBA EWRAM after (7,680 free today) |
|---|---:|---:|---:|---:|
| today | 96 | 96,096 | 0 | 7,680 |
| fixed 128 B (look 80 B) | 128 | 128,128 | +32,032 | **-24,352: overflow** |
| fixed 192 B (C + D tiers bit-packed) | 192 | 192,192 | +96,096 | **-88,416: overflow** |
| fixed 432 B (everything one byte per field) | 432 | 432,432 | +336,336 | impossible (EWRAM is 256 KB) |
| **B: 48 B hot + paged cold store** | 48 + 3 + blob | 48,048 + 37,360 + cache | **-4,576 net** | **12,256** |

Nothing else in EWRAM can be cut to pay for even the 128 B layout. The 32 KB live map, 18 KB portrait pool, 17 KB
`gba_big` and 12 KB scene scratch are all in use (`docs/GBA.md` RAM table). The only large redundancy is the
residents themselves. On a premade island, 1000 x 48 B of looks are byte-for-byte copies of ROM records. On a random
island they are a pure function of `(seed, id)`. Design B stops storing those copies. With that space freed, the
*logical* record can be as fat as we like, because a fat record exists only as a few working copies (the cache lines,
the creator draft, the portrait job).

### 1.2 Three forms of a citizen

| Form | Size | Where | Who uses it |
|---|---:|---|---|
| `DoomoResident` (hot) | 48 B | `doomo_town.res[1000]`, `doomo_town.player`: always in RAM | the sim (every tick), UI lists, names, news, saves |
| `DoomoLook` (fat look, unpacked) | 384 B | decode cache lines; creator draft; portrait job; locals of births and loaders | renderers, creator, babies, Look Password |
| blob (packed delta) | 0..196 B, variable | `res_heap` (RAM), the save body, the Look Book, passwords | `resstore.c` only |

`DoomoCitizen = { DoomoResident r; DoomoLook l; }` (432 B) is the full working form that the creator, births,
`town_new` and the loaders pass around.

### 1.3 The hot record

```c
/* resident.h v2 */
typedef struct DoomoResident {          /* "hot": in RAM for every citizen, 48 B */
    char    name[DOOMO_NAME_LEN];                        /*  0..11 */
    uint8_t persona[PERS_COUNT];                         /* 12..16 each 0..15  */
    uint8_t stat[STAT_COUNT];                            /* 17..24 each 0..100 */
    uint8_t status, flags, pronoun, romance;             /* 25..28 RES_*, RF_* */
    uint8_t bday_month, bday_day;                        /* 29..30 */
    uint8_t like_food, like_hobby, like_music, like_color; /* 31..34 */
    uint8_t hate_food, fear, catchphrase;                /* 35..37 */
    uint8_t exorcism, level, mood, activity, death_cause;/* 38..42 */
    uint8_t job;          /* 43  wishlist A40: custom job 0..63, 0xFF = roster job / none */
    /* mirrors of the cold look: written ONLY by res_commit(), town_new() and the loaders */
    uint8_t species;      /* 44  look.species: sim need rates, almanac, exorcism, lyrics       */
    uint8_t skin;         /* 45  look.skin: placeholder ball (fp_common), first-person paws    */
    uint8_t soul;         /* 46  holy bonus 0..25: halo / ACC_ROSARY / ACC_CROSS / neck item   */
    uint8_t xflags;       /* 47  RFX_* below                                                   */
} DoomoResident;
DOOMO_STATIC_ASSERT(sizeof(DoomoResident) == 48, hot_is_48_bytes);

enum { RFX_AGE_MASK     = 7,    /* look.age_look mirror: AGE_AUTO, BABY, KID, TEEN, ADULT, ELDER */
       RFX_BLOB         = 8,    /* the cold store holds a blob for this id                    */
       RFX_PREMADE_SELF = 16,   /* the slot still holds its premade identity (roster traits apply) */
       RFX_LEFTY        = 32 }; /* look.lefty mirror: combat weapon side                      */

typedef struct DoomoCitizen { DoomoResident r; DoomoLook l; } DoomoCitizen;
DOOMO_STATIC_ASSERT(sizeof(DoomoCitizen) == 432, citizen_is_432_bytes);
```

Why these mirrors? The per-tick sim reads only four things from the look: species
(`sim.c:430` `species_pct`, `sim_care.c:172,308`, `exorcism.c:78`), halo and accessory (`sim_soul.c:125-127`), skin
(`fp_common.c:709` placeholder ball, `combat_hud.c:304` paws), and age for the romance gate. Mirroring those 4 bytes
means the sim never decodes a look. `sim_traits()` (`sim.c:117`) no longer runs `strncmp` against `roster_name()`. It
tests `RFX_PREMADE_SELF`, and for custom citizens it maps `RFX_AGE` to `ROSTER_T_KID`, `ROSTER_T_TEEN` and
`ROSTER_T_ELDER`. **The new Age look field therefore drives the same kid/teen romance gate** that is hard-wired today.

The 246 `->name` uses in 61 files keep working because the name stays hot. The 119 `->look` uses in 27 files stop
compiling, because `look` is no longer a member. The compiler therefore lists every site that has to move to
`res_look(id)` or to `DoomoCitizen`.

### 1.4 The fat look (`DoomoLook`, 384 B)

Every field is one `uint8_t`, so the creator's generic row model, presets, randomise, copy and tests keep working one
byte at a time. The first 48 bytes keep today's offsets and meaning, so existing painters compile unchanged. Voice
moves in at 48..52 because it is presentation, not sim state. Each section below is also one section of the packed
blob.

```c
typedef struct DoomoLook {           /* the fat working look: every field one byte */
    /* ---- legacy ---- */
    uint8_t species, body, height, build, skin, head_shape;            /* 0..5 */
    uint8_t head_size, eye_style, eye_color, eye_size, eye_spacing;    /* 6..10 */
    uint8_t eye_height, eye_tilt, eye_count, brow_style, brow_height;  /* 11..15 */
    uint8_t nose_style, nose_size, mouth_style, mouth_size, fangs;     /* 16..20 */
    uint8_t hair_style, hair_color, hair_highlight, facial_hair;       /* 21..24 */
    uint8_t ears, horns, horn_color, horn_size, tail, wings, halo;     /* 25..31 */
    uint8_t marking, marking_color, cheeks, top, top_color, bottom;    /* 32..37 */
    uint8_t bottom_color, shoes, shoes_color, hat, hat_color;          /* 38..42 */
    uint8_t accessory, accessory_color, aura, pattern, pattern_color;  /* 43..47 */
    /* voice (moved here from DoomoResident) */
    uint8_t voice[VOICE_COUNT];                                 /* 48..52 */
    /* ---- face ---- */
    uint8_t head_width, cheek_fullness, chin, feature_height;          /* 53..56 */
    uint8_t feature_scale, face_age;                                   /* 57..58 */
    /* ---- eyes ---- */
    uint8_t pupil, pupil_size, iris, iris_size, eye_color2, hetero;    /* 59..64 */
    uint8_t eye_color_r, sclera, shine, eye_glow, lashes, lashes_low;  /* 65..70 */
    uint8_t lash_color, lid_droop, eye_outline, eye_bags, eye_asym;    /* 71..75 */
    uint8_t extra_eye_place, gaze, special_eye_l, special_eye_r;       /* 76..79 */
    /* ---- brows ---- */
    uint8_t brow_color, brow_thick, brow_angle, brow_spacing;          /* 80..83 */
    uint8_t brow_length;                                               /* 84..84 */
    /* ---- nose ---- */
    uint8_t nose_color, nose_shine, nose_height, muzzle, muzzle_len;   /* 85..89 */
    uint8_t muzzle_puff, muzzle_color, whisker_pads, whiskers;         /* 90..93 */
    uint8_t whisker_len, whisker_color;                                /* 94..95 */
    /* ---- mouth ---- */
    uint8_t mouth_width, mouth_height, mouth_curve, lip_color;         /* 96..99 */
    uint8_t lip_style, teeth, fang_place, teeth_color, tongue;         /* 100..104 */
    uint8_t tongue_color;                                              /* 105..105 */
    /* ---- cheeks ---- */
    uint8_t blush, blush_color, blush_amt, blush_pos, blush_size;      /* 106..110 */
    uint8_t freckles, freckle_amt, freckle_color, beauty_mark, scar1;  /* 111..115 */
    uint8_t scar2, seams, seam_color, bandage, bandage_color;          /* 116..120 */
    uint8_t cheek_fluff, face_gem, face_gem_color;                     /* 121..123 */
    /* ---- makeup ---- */
    uint8_t eyeshadow, eyeshadow_color, eyeliner, eyeliner_color;      /* 124..127 */
    uint8_t highlighter, polish, polish_color, face_paint;             /* 128..131 */
    uint8_t paint_color1, paint_color2, paint_finish, face_tattoo;     /* 132..135 */
    uint8_t face_tattoo_color;                                         /* 136..136 */
    /* ---- pierce ---- */
    uint8_t ear_pierce, ear_pierce_side, face_pierce, tongue_stud;     /* 137..140 */
    uint8_t horn_rings, pierce_metal;                                  /* 141..142 */
    /* ---- glasses ---- */
    uint8_t glasses, frame_color, lens_tint, glasses_pos;              /* 143..146 */
    /* ---- hair ---- */
    uint8_t bangs, ahoge, hair_volume, hair_length, hair_mirror;       /* 147..151 */
    uint8_t dye_pattern, hair_color3, hair_shine, hair_acc;            /* 152..155 */
    uint8_t hair_acc_color, hair_acc_side, facial_hair_color;          /* 156..158 */
    /* ---- ears ---- */
    uint8_t ear_size, ear_angle, ear_fold, inner_ear_color, ear_tip;   /* 159..163 */
    /* ---- shape ---- */
    uint8_t shoulders, torso, waist, hips, belly, arm_len, arm_thick;  /* 164..170 */
    uint8_t leg_len, leg_thick, neck_len, head_ratio, posture;         /* 171..175 */
    uint8_t hand_size, foot_size, muscle, roundness;                   /* 176..179 */
    /* ---- fur ---- */
    uint8_t material, fluff, fur_len, sheen, translucency;             /* 180..184 */
    uint8_t fur_color2, fur_color3, fur_pattern, fur_pat_scale;        /* 185..188 */
    uint8_t fur_pat_place, belly_color, pad_color, pad_shape, claws;   /* 189..193 */
    uint8_t claw_color, chest_fluff, chest_fluff_color, mane;          /* 194..197 */
    uint8_t grey_muzzle, glow_mark, glow_color, glow_pulse, tattoo1;   /* 198..202 */
    uint8_t tattoo1_place, tattoo2, tattoo2_place, tattoo_ink;         /* 203..206 */
    uint8_t body_scar, plush_build;                                    /* 207..208 */
    /* ---- limbs ---- */
    uint8_t hands, feet, legs, mobility, prosthetic;                   /* 209..213 */
    /* ---- tail ---- */
    uint8_t tail_count, tail_len, tail_fluff, tail_tip_color;          /* 214..217 */
    uint8_t tail_tip, tail_pose;                                       /* 218..219 */
    /* ---- wings ---- */
    uint8_t wing_size, wing_color, wing_color2, wing_pattern;          /* 220..223 */
    uint8_t wing_pose, wing_pairs;                                     /* 224..225 */
    /* ---- horns ---- */
    uint8_t horn_count, horn_curl, horn_texture, horn_tip_color;       /* 226..229 */
    uint8_t horn_gradient, horn_asym, horn_pos, antennae;              /* 230..233 */
    uint8_t antenna_color, head_extra, halo_color, halo_tilt;          /* 234..237 */
    uint8_t halo_size, aura_color, aura_amt;                           /* 238..240 */
    /* ---- extra ---- */
    uint8_t extra_arms, body_eyes, buddy_head, belly_mouth, age_look;  /* 241..245 */
    uint8_t hybrid, hybrid_blend;                                      /* 246..247 */
    /* ---- outfit ---- */
    uint8_t inner_top, inner_top_color, outer, outer_color, legwear;   /* 248..252 */
    uint8_t legwear_color, legwear_color2, socks, sock_color;          /* 253..256 */
    uint8_t shoes_color2, gloves, glove_color, belt, belt_color;       /* 257..261 */
    uint8_t neck, neck_color, back, back_color, mask, mask_color;      /* 262..267 */
    uint8_t mask_pos, hat_tilt, hat_size, hat_ears, hat_color2;        /* 268..272 */
    uint8_t ear_acc, ear_acc_color, tail_acc, tail_acc_color;          /* 273..276 */
    uint8_t horn_acc, horn_acc_color, wing_acc, wrist, wrist_color;    /* 277..281 */
    uint8_t prop, prop_color;                                          /* 282..283 */
    /* ---- mods ---- */
    uint8_t top_color2, bottom_color2, shoes_trim, outer_color2;       /* 284..287 */
    uint8_t pattern_on, outer_pattern, outer_pattern_color;            /* 288..290 */
    uint8_t bottom_pattern, bottom_pattern_color, legwear_pattern;     /* 291..293 */
    uint8_t legwear_pattern_color, hat_pattern, hat_pattern_color;     /* 294..296 */
    uint8_t emblem, emblem_color, sleeves, neckline, hood_up;          /* 297..301 */
    uint8_t hood_ears, top_len, tucked, outer_closed, bottom_len;      /* 302..306 */
    uint8_t cuffs, wear, fabric, fit;                                  /* 307..310 */
    /* ---- wardrobe ---- */
    uint8_t outfit_sleep, outfit_swim, outfit_formal, outfit_battle;   /* 311..314 */
    uint8_t auto_switch;                                               /* 315..315 */
    /* ---- motion ---- */
    uint8_t idle, fidget, walk, run, stride, sit, sleep_pose, dance;   /* 316..323 */
    uint8_t emote, greeting, laugh, cry, angry, love, victory, taunt;  /* 324..331 */
    uint8_t hurt, faint, lefty, blink, blink_rate, ear_expr, wag;      /* 332..338 */
    uint8_t wag_amt, breath, bounce, rest_expr, talk, gaze_habit;      /* 339..344 */
    uint8_t mood_motion;                                               /* 345..345 */
    /* ---- voicex ---- */
    uint8_t footstep, babble, voice_flavor, sing_range, vibrato;       /* 346..350 */
    uint8_t quirk_sound, loudness;                                     /* 351..352 */
    uint8_t reserved[31];                                        /* 353..383: 0, growth room */
} DoomoLook;
DOOMO_STATIC_ASSERT(sizeof(DoomoLook) == 384, look_is_384_bytes);
```

### 1.5 The field registry and value kinds

One table drives sanitize, blob encode/decode, the save, the creator rows, randomise, copy/paste, symmetry links and
the tests. Adding a field takes one struct line, one registry line and a painter. Reserved bytes 353..383 let later
versions add 31 fields without changing the struct size, and the blob's table version (1.7) keeps old blobs readable.

```c
typedef struct LookField {
    uint16_t off;     /* offsetof(DoomoLook, x)                                       */
    uint8_t  count;   /* number of values (E, S) or flag bits (F)                     */
    uint8_t  kind;    /* LK_ENUM, LK_SLIDER, LK_COLOR, LK_PAINT, LK_SKIN, LK_FLAGS, LK_VOICE */
    uint8_t  sect;    /* blob section 0..24 (legacy, voice, 23 new sections)          */
    uint8_t  parent;  /* index of the field whose 0 makes this one irrelevant, 0xFF none */
    uint8_t  wave;    /* 0..4: shown by the creator and kept by sanitize once LOOK_WAVE >= wave */
    uint8_t  flags;   /* LF_ANIM (affects animation phases), LF_PAIR (L/R twin), LF_SIMPLE /
                         LF_ADVANCED (creator mode), LF_SPECIES_BASE, LF_SHAPE / LF_COLOUR */
    uint8_t  views;   /* LV_BUST | LV_FIGURE | LV_CHIBI | LV_FIGHTER | LV_MODEL: which renders it changes */
    uint8_t  prio;    /* order in which "Simplify" drops it (5.7): wave-3 detail first   */
} LookField;          /* 10 B x 353 = 3,530 B const (ROM on GBA, resident on PS1) */
extern const LookField doomo_look_fields[LF_COUNT];
DOOMO_STATIC_ASSERT(LF_COUNT == 353, look_field_count);   /* + a generated offsetof check per field */
```

**Value kinds and blob widths.** A blob's `tones` header bit decides between the two widths shown for colours.

| kind | values | blob bits (tones off / on) | notes |
|---|---|---|---|
| `LK_ENUM` | 0..count-1 (new fields: 0 = auto/none) | ceil(log2(count)) | |
| `LK_SLIDER` | 0..count-1 | ceil(log2(count)) | new sliders are stored XOR the step that matches today's drawing (`head_width`: today = 8, stored as 0), so a zero byte always draws as today. The creator shows the real step |
| `LK_COLOR` (new colour fields) | 0 = auto (derived exactly as today), 1..96 = paint (v-1)%32 in tone (v-1)/32 | 6 / 7 | tone 0 normal, 1 pastel, 2 deep |
| `LK_PAINT` (the 11 legacy colour fields) | 0..95 = paint + 32 x tone, 0xFF = none (highlight) | 5 / 7 (highlight 6 / 7) | v1 values 0..31 keep their meaning |
| `LK_SKIN` | 0..7 natural, 8..103 = paint + tone | 6 / 7 | |
| `LK_FLAGS` | bit mask | count | `cheeks`, `face_pierce`, `fur_pat_place`, `pattern_on`, `auto_switch` |
| `LK_VOICE` | 0..31 | 5 | |

**Colour tones** give 96 colours without touching the full palette. `pm_paint()` (`portrait_gfx.c:86`) already builds
a 5-level material from a paint's 3 shades through `pm_from3()`. Pastel is `pm_from3(base, light, lighter(light))`,
and deep is `pm_from3(darker(dark), dark, base)`. This needs no palette entries, so the GBA's single Mode 4 palette
is fine. A generated table `doomo_tone_ok[96]` hides the tones that collapse onto another colour, and the creator's
swatch grid shows 32 columns x 3 rows. The 83 raw `PAL_PAINT(field, s)` reads in the painters move to `pm_paint()` or
`look_paint()`, which decode tones (grep-able).

### 1.6 Field list with bit widths

The legacy 48 keep their offsets. Their v2 counts after the append-only growths are below. The v2 blob width caps
each list at 2^width values.

| field | v1 count, width | v2 count | v2 blob width | wish id |
|---|---|---:|---:|---|
| species | 24, 6 | 48 | 6 | B87 (+24 species, wave 4) |
| body | 10, 5 | 16 | 4 | B01 |
| head_shape | 12, 5 | 24 | 5 | F01 |
| eye_style | 24, 6 | 40 | 6 | F07 |
| eye_count | 3, 2 | 6 (1..6) | 3 | F25 |
| brow_style / nose_style | 12, 5 | 24 | 5 | F29 / F35 |
| mouth_style | 20, 6 | 32 | 5 | F47 |
| hair_style | 32, 6 | 64 | 6 | H01 |
| facial_hair | 8, 4 | 16 | 4 | H13 |
| ears | 16, 5 | 32 | 5 | E01 |
| horns | 16, 5 | 26 | 5 | B66 |
| tail | 16, 5 | 32 | 5 | B52 |
| wings | 10, 5 | 20 | 5 | B59 |
| halo | 6, 4 | 12 | 4 | B76 |
| top | 24, 6 | 50 | 6 | O03 |
| bottom / shoes | 12, 5 | 24 | 5 | O06 / O12 |
| hat | 24, 6 | 48 | 6 | O25 |
| accessory | 20, 6 | 20 (legacy misc slot) | 5 | O41 |
| aura | 8, 4 | 20 | **5** (was capped at 16) | B79 |
| pattern | 8, 4 | 32 | **5** (was capped at 16) | O45 |
| skin | 40, 6 | 104 | 6 / 7 | tones |
| 11 legacy colours | 32, 5 | 96 (+none) | 5 / 7 | tones |
| sliders height, build, head_size, eye_size/spacing/height/tilt, brow_height, nose_size, mouth_size, horn_size | 16, 4 | 16 | 4 | |
| fangs | 4, 2 | 4 | 2 | |
| marking | 23, 6 | 23 | 5 | face paint and fur pattern now have their own fields |
| cheeks | 8 flags | 8 flags | 8 | |

The 300 new fields follow, one table per blob section. "off" is the byte offset in `DoomoLook`. In "values", `C`
fields are "auto + 32 paints x 3 tones". "bits" is the blob width (`6/7` = tones off/on). "wave" is when the field
ships (1.9, 8). "shows on" is a 5-letter code, one position per view, with `-` where the field is not drawn:
**B** bust, **F** creator figure, **C** chibi, **T** fighter (GBA third person and the creator turntable), and in the
last position **M** for PS1 3D model code or **I** for the PS1 head impostor (automatic, 4.4).

**face** (6 fields, 20 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 53 | `head_width` | F02 | 0..15 | 4 | 2 | BF-TI |
| 54 | `cheek_fullness` | F03 | 0..7 | 3 | 2 | BF-TI |
| 55 | `chin` | F04 | 7: auto, round, pointy, square, soft double, cleft, fluffy beard-tuft (7) | 3 | 3 | BF--I |
| 56 | `feature_height` | F05 | 0..15 | 4 | 2 | BF-TI |
| 57 | `feature_scale` | F06 | 0..7 | 3 | 3 | BF--I |
| 58 | `face_age` | F76 | 5: none, baby chub, laugh lines, wrinkly brow, sleepy elder lids (5) | 3 | 3 | BF--I |

**eyes** (21 fields, 78 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 59 | `pupil` | F08 | 32: auto (from eye style), round, vertical slit, horizontal slit (goat)... | 5 | 2 | BF--I |
| 60 | `pupil_size` | F09 | 0..7 | 3 | 2 | BF--I |
| 61 | `iris` | F10 | 16: auto, solid, two-tone gradient, limbal ring, starburst, galaxy (dit... | 4 | 2 | BF--I |
| 62 | `iris_size` | F11 | 0..7 | 3 | 3 | BF-TI |
| 63 | `eye_color2` | F12 | auto + 32 paints x 3 tones | 6/7 | 3 | BF--I |
| 64 | `hetero` | F13 | 6: none, full (right eye uses F14), sectoral (half iris), central ring... | 3 | 2 | BF-TI |
| 65 | `eye_color_r` | F14 | auto + 32 paints x 3 tones (if hetero) | 6/7 | 2 | BF-TI |
| 66 | `sclera` | F15 | 16: auto (white), cream, pale pink, pale blue, black (demon), deep red,... | 4 | 2 | BF--I |
| 67 | `shine` | F16 | 8: auto (2, per art direction), one, three sparkly, star, heart, cross... | 3 | 2 | BF--I |
| 68 | `eye_glow` | F17 | 4: off, steady, slow pulse, flicker (4) | 2 | 3 | BFCTI |
| 69 | `lashes` | F18 | 16: auto (from style), none, short, long, flutter (3 big cartoon), thic... | 4 | 2 | BF--I |
| 70 | `lashes_low` | F19 | 4: none, few, full, painted doll (4) | 2 | 3 | BF--I |
| 71 | `lash_color` | F20 | auto + 32 paints x 3 tones | 6/7 | 3 | BF--I |
| 72 | `lid_droop` | F21 | 0..7 | 3 | 2 | BF-TI |
| 73 | `eye_outline` | F22 | 5: auto, thin, thick, none (glossy dots), coloured (= lash colour) (5) | 3 | 3 | BF--I |
| 74 | `eye_bags` | F23 | 6: none, light, tired, insomniac goth, plush eye-bag fluff, glitter ba... | 3 | 3 | BF--I |
| 75 | `eye_asym` | F24 | 7: -3..+3 | 3 | 3 | BF--I |
| 76 | `extra_eye_place` | F26 | 6: auto (forehead), stacked pairs, along the brow, on the horns, on th... | 3 | 3 | BF-TI |
| 77 | `gaze` | F27 | 7: forward, side-eye L, side-eye R, up (dreamy), down (shy), cross-eye... | 3 | 3 | BF-TI |
| 78 | `special_eye_l` | F28 | 8: none, button eye (plush), stitched shut, robot LED, scope eye, glas... | 3 | 3 | BF-TI |
| 79 | `special_eye_r` | F28 | 8: none, button eye (plush), stitched shut, robot LED, scope eye, glas... | 3 | 3 | BF-TI |

**brows** (5 fields, 18 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 80 | `brow_color` | F30 | auto + 32 paints x 3 tones | 6/7 | 2 | BF--I |
| 81 | `brow_thick` | F31 | 0..7 | 3 | 2 | BF--I |
| 82 | `brow_angle` | F32 | 7: -3..+3 (worried ↔ cross) | 3 | 3 | BF-TI |
| 83 | `brow_spacing` | F33 | 0..7 | 3 | 3 | BF--I |
| 84 | `brow_length` | F34 | 0..3 | 2 | 3 | BF--I |

**nose** (11 fields, 41 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 85 | `nose_color` | F36 | auto + 32 paints x 3 tones | 6/7 | 2 | BFCTI |
| 86 | `nose_shine` | F37 | 2: off, on | 1 | 3 | BF--I |
| 87 | `nose_height` | F38 | 0..7 | 3 | 3 | BF--I |
| 88 | `muzzle` | F39 | 16: auto (species), none, small round, feline puffs (W), canine short, ... | 4 | 2 | BF-TI |
| 89 | `muzzle_len` | F40 | 0..7 | 3 | 2 | BF-TI |
| 90 | `muzzle_puff` | F41 | 0..3 | 2 | 3 | BF--I |
| 91 | `muzzle_color` | F42 | auto + 32 paints x 3 tones | 6/7 | 3 | BFCTI |
| 92 | `whisker_pads` | F43 | 4: none, dots, freckle dots, big spots (4) | 2 | 3 | BF--I |
| 93 | `whiskers` | F44 | 8: auto (species), none, short x2, long x3, curly, droopy elder, drawn... | 3 | 3 | BF-TI |
| 94 | `whisker_len` | F45 | 0..3 (if whiskers) | 2 | 3 | BF--I |
| 95 | `whisker_color` | F46 | auto + 32 paints x 3 tones (if whiskers) | 6/7 | 3 | BF--I |

**mouth** (10 fields, 35 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 96 | `mouth_width` | F48 | 0..7 | 3 | 3 | BF--I |
| 97 | `mouth_height` | F49 | 0..7 | 3 | 3 | BF--I |
| 98 | `mouth_curve` | F50 | 7: -3..+3 (frowny ↔ smiley, applied under every expression) | 3 | 3 | BF-TI |
| 99 | `lip_color` | F51 | auto + 32 paints x 3 tones | 6/7 | 3 | BF--I |
| 100 | `lip_style` | F52 | 7: none (critter default), thin, full, pouty, glossy, ombre, black got... | 3 | 3 | BF--I |
| 101 | `teeth` | F53 | 16: auto, none, neat, buck teeth, gap tooth, snaggle tooth, braces, gol... | 4 | 2 | BF--I |
| 102 | `fang_place` | F54 | 5: both, left, right, lower, upper + lower (5) | 3 | 3 | BF--I |
| 103 | `teeth_color` | F55 | 6: white, ivory, gold, silver grill, black, candy stripe (6) | 3 | 3 | BF--I |
| 104 | `tongue` | F56 | 7: auto, blep (always a little out), long lick, forked, split tip, pan... | 3 | 2 | BF-TI |
| 105 | `tongue_color` | F57 | 6: pink, red, purple (chow/demon), blue, black, slime green (6) | 3 | 3 | BF--I |

**cheeks** (18 fields, 77 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 106 | `blush` | F58 | 12: ovals, circles, anime hatching ///, hearts, stars, spirals (tama-sw... | 4 | 2 | BF-TI |
| 107 | `blush_color` | F59 | auto + 32 paints x 3 tones | 6/7 | 2 | BFCTI |
| 108 | `blush_amt` | F60 | 0..3 | 2 | 3 | BF--I |
| 109 | `blush_pos` | F61 | 4: high, mid, low, across the nose (4) | 2 | 3 | BF--I |
| 110 | `blush_size` | F62 | 0..3 | 2 | 3 | BF--I |
| 111 | `freckles` | F63 | 8: cheeks, nose bridge, cheeks + nose, all-over speckle, shoulders, st... | 3 | 3 | BF--I |
| 112 | `freckle_amt` | F64 | 0..3 | 2 | 3 | BF--I |
| 113 | `freckle_color` | F65 | auto + 32 paints x 3 tones | 6/7 | 3 | BF--I |
| 114 | `beauty_mark` | F66 | 17: under eye L/R, lip L/R, cheek L/R, chin, forehead (8) + count 1-2 | 5 | 3 | BF--I |
| 115 | `scar1` | F67 | 16: none, cheek slash, through eye L, through eye R, across nose, lip, ... | 4 | 3 | BF-TI |
| 116 | `scar2` | F68 | 16: same list | 4 | 3 | BF-TI |
| 117 | `seams` | F69 | 16: none, face centre seam, eye-ring seam, mouth-corner stitches, fabri... | 4 | 3 | BF-TI |
| 118 | `seam_color` | F70 | auto + 32 paints x 3 tones (if seams) | 6/7 | 3 | BF-TI |
| 119 | `bandage` | F71 | 9: cheek plaster, anime X plaster, heart band-aid, nose strip, head wr... | 4 | 3 | BF-TI |
| 120 | `bandage_color` | F72 | auto + 32 paints x 3 tones (if bandage) | 6/7 | 3 | BF--I |
| 121 | `cheek_fluff` | F73 | 8: auto (species), none, small, big, sideburn mane, double tuft, curly... | 3 | 3 | BFCTI |
| 122 | `face_gem` | F74 | 8: none, star under eye, heart under eye, teardrop gem, rhinestone lin... | 3 | 3 | BF--I |
| 123 | `face_gem_color` | F75 | auto + 32 paints x 3 tones (if face_gem) | 6/7 | 3 | BF--I |

**makeup** (13 fields, 66 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 124 | `eyeshadow` | F77 | 10: none, lid wash, smoky, wing, cut crease, halo, glitter, rainbow gra... | 4 | 3 | BF--I |
| 125 | `eyeshadow_color` | F78 | auto + 32 paints x 3 tones (if eyeshadow) | 6/7 | 3 | BF--I |
| 126 | `eyeliner` | F79 | 10: none, tightline, small wing, cat-eye, double wing, goth thick, floa... | 4 | 3 | BF--I |
| 127 | `eyeliner_color` | F80 | auto + 32 paints x 3 tones (if eyeliner) | 6/7 | 3 | BF--I |
| 128 | `highlighter` | F81 | 0..3 | 2 | 3 | BF--I |
| 129 | `polish` | F82 | 5: none, solid, tips, glitter, black goth (5) + colour C6 | 3 | 3 | -F-T- |
| 130 | `polish_color` | F82 | auto + 32 paints x 3 tones (if polish) | 6/7 | 3 | -F-T- |
| 131 | `face_paint` | F83 | 24: none, clown, skull paint, bold stripes, cheek hearts, tears, pentag... | 5 | 3 | BF-TI |
| 132 | `paint_color1` | F84 | auto + 32 paints x 3 tones (if face_paint) | 6/7 | 3 | BF-TI |
| 133 | `paint_color2` | F85 | auto + 32 paints x 3 tones (if face_paint) | 6/7 | 3 | BF--I |
| 134 | `paint_finish` | F86 | 4: faded, half, full, neon glow (4) (if face_paint) | 2 | 3 | BF--I |
| 135 | `face_tattoo` | F87 | 16: none, tear, heart, star, cross, "666", paw print, rose, cheek spide... | 4 | 3 | BF--I |
| 136 | `face_tattoo_color` | F88 | auto + 32 paints x 3 tones (if face_tattoo) | 6/7 | 3 | BF--I |

**pierce** (6 fields, 20 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 137 | `ear_pierce` | F89 | 15: none, studs, hoops, helix stack, industrial bar, gauges, dangle sku... | 4 | 3 | BF-TI |
| 138 | `ear_pierce_side` | F90 | 3: left, right, both (3) (if ear_pierce) | 2 | 3 | BF--I |
| 139 | `face_pierce` | F91 | 8-bit mask | 8 | 3 | BF--I |
| 140 | `tongue_stud` | F92 | 2: off, on | 1 | 3 | B---I |
| 141 | `horn_rings` | F93 | 0..3 | 2 | 3 | BF-TI |
| 142 | `pierce_metal` | F94 | 8: silver, gold, rose gold, black, rainbow anodised, glow, bone, candy... | 3 | 3 | BF--I |

**glasses** (4 fields, 18 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 143 | `glasses` | F95 | 32: none, round, square, oval, cat-eye, heart, star, aviator, wayfarer ... | 5 | 2 | BF-TI |
| 144 | `frame_color` | F96 | auto + 32 paints x 3 tones (if glasses) | 6/7 | 3 | BF-TI |
| 145 | `lens_tint` | F97 | 9: clear, pink, blue, amber, black, mirror, rainbow, swirl, glowing re... (if glasses) | 4 | 3 | BF--I |
| 146 | `glasses_pos` | F98 | 4: on the eyes, low on the nose, pushed up on the forehead, on top of ... (if glasses) | 2 | 3 | BF-TI |

**hair** (12 fields, 47 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 147 | `bangs` | H02 | 16: auto, none, blunt, side-swept L, side-swept R, curtain, wispy, chop... | 4 | 3 | BF-TI |
| 148 | `ahoge` | H03 | 9: none, single strand, heart, double antenna, question mark, coil spr... | 4 | 3 | BFCTI |
| 149 | `hair_volume` | H04 | 0..3 | 2 | 3 | BF-TI |
| 150 | `hair_length` | H05 | 0..3 | 2 | 3 | BF-TI |
| 151 | `hair_mirror` | H06 | 2: default, mirrored | 1 | 3 | BFCTI |
| 152 | `dye_pattern` | H07 | 12: streaks (today), tips/ombre, roots, split dye, underdye (peekaboo),... | 4 | 3 | BFCTI |
| 153 | `hair_color3` | H08 | auto + 32 paints x 3 tones | 6/7 | 3 | BF-TI |
| 154 | `hair_shine` | H09 | 4: matte, soft, angel ring, sparkle (4) | 2 | 3 | BF--I |
| 155 | `hair_acc` | H10 | 18: none, big bow, small bow, ribbon tails, star clips, heart clips, bo... | 5 | 3 | BFCTI |
| 156 | `hair_acc_color` | H11 | auto + 32 paints x 3 tones (if hair_acc) | 6/7 | 3 | BF-TI |
| 157 | `hair_acc_side` | H12 | 4: left, right, both, centre (4) (if hair_acc) | 2 | 3 | BF--I |
| 158 | `facial_hair_color` | H14 | auto + 32 paints x 3 tones | 6/7 | 3 | BF-TI |

**ears** (5 fields, 19 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 159 | `ear_size` | E02 | 0..7 | 3 | 2 | BFCTI |
| 160 | `ear_angle` | E03 | 0..7 | 3 | 3 | BF-TI |
| 161 | `ear_fold` | E04 | 7: none, tip fold, half lop, full lop, left ear down, right ear down, ... | 3 | 2 | BF-TI |
| 162 | `inner_ear_color` | E05 | auto + 32 paints x 3 tones | 6/7 | 2 | BF-TI |
| 163 | `ear_tip` | E06 | 8: none, tipped colour, lynx tufts, fluffy inner fur, notched (alley c... | 3 | 3 | BF-TI |

**shape** (16 fields, 42 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 164 | `shoulders` | B02 | 0..7 | 3 | 2 | -FCTM |
| 165 | `torso` | B03 | 0..7 | 3 | 3 | -F-TM |
| 166 | `waist` | B04 | 0..7 | 3 | 3 | -F-TM |
| 167 | `hips` | B05 | 0..7 | 3 | 3 | -F-TM |
| 168 | `belly` | B06 | 0..7 | 3 | 2 | -FCTM |
| 169 | `arm_len` | B07 | 0..7 | 3 | 2 | -F-TM |
| 170 | `arm_thick` | B08 | 0..3 | 2 | 3 | -F-TM |
| 171 | `leg_len` | B09 | 0..7 | 3 | 2 | -FCTM |
| 172 | `leg_thick` | B10 | 0..3 | 2 | 3 | -F-TM |
| 173 | `neck_len` | B11 | 0..3 | 2 | 3 | BF-TM |
| 174 | `head_ratio` | B12 | 5: auto, 2-head SD, 2.5, 3, 4-head (5) | 3 | 3 | -FCTM |
| 175 | `posture` | B13 | 9: auto, upright, proud chest-out, slouch, hunched, shy (shoulders in)... | 4 | 3 | -FCTM |
| 176 | `hand_size` | B14 | 0..3 | 2 | 3 | -F-TM |
| 177 | `foot_size` | B15 | 0..3 | 2 | 3 | -FCTM |
| 178 | `muscle` | B16 | 0..3 | 2 | 3 | -F-T- |
| 179 | `roundness` | B17 | 0..3 | 2 | 3 | -FCTM |

**fur** (29 fields, 123 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 180 | `material` | B18 | 32: auto (species), short fur, long shaggy fur, wool curls, velvet plus... | 5 | 2 | BFCTM |
| 181 | `fluff` | B19 | 0..7 | 3 | 2 | BFCTM |
| 182 | `fur_len` | B20 | 0..3 | 2 | 3 | BF-T- |
| 183 | `sheen` | B21 | 7: auto, matte, soft, glossy, wet, metallic, sparkle (7) | 3 | 3 | BF-TM |
| 184 | `translucency` | B22 | 0..3 | 2 | 3 | BF-TM |
| 185 | `fur_color2` | B24 | auto + 32 paints x 3 tones | 6/7 | 3 | BFCTM |
| 186 | `fur_color3` | B25 | auto + 32 paints x 3 tones | 6/7 | 2 | BFCTM |
| 187 | `fur_pattern` | B26 | 32: none, muzzle, belly, socks, mask, tabby, calico (legacy) + gloves, ... | 5 | 2 | BFCTM |
| 188 | `fur_pat_scale` | B27 | 0..3 (if fur_pattern) | 2 | 3 | BF-T- |
| 189 | `fur_pat_place` | B28 | 5-bit mask (if fur_pattern) | 5 | 3 | BFCT- |
| 190 | `belly_color` | B29 | auto + 32 paints x 3 tones | 6/7 | 3 | -FCTM |
| 191 | `pad_color` | B30 | auto + 32 paints x 3 tones | 6/7 | 3 | BF-T- |
| 192 | `pad_shape` | B31 | 4: toe beans, heart pads, star pads, none (4) | 2 | 3 | -F-T- |
| 193 | `claws` | B32 | 5: none, short, long, cat-out, painted (= F82) (5) | 3 | 3 | -F-T- |
| 194 | `claw_color` | B33 | auto + 32 paints x 3 tones (if claws) | 6/7 | 3 | -F-T- |
| 195 | `chest_fluff` | B34 | 8: auto, none, small tuft, big ruff, lion mane, collar ruff, heart-sha... | 3 | 3 | BF-TM |
| 196 | `chest_fluff_color` | B35 | auto + 32 paints x 3 tones (if chest_fluff) | 6/7 | 3 | BF-T- |
| 197 | `mane` | B36 | 8: none, lion, horse crest, punk spines, feather crest, cloud collar, ... | 3 | 3 | BF-TM |
| 198 | `grey_muzzle` | B37 | 4: none, light, senior grey, all silvered (4) | 2 | 3 | BF--- |
| 199 | `glow_mark` | B38 | 16: none, circuit lines, cute sigils, bioluminescent spots, lava cracks... | 4 | 3 | BFCTM |
| 200 | `glow_color` | B39 | auto + 32 paints x 3 tones (if glow_mark) | 6/7 | 3 | BFCTM |
| 201 | `glow_pulse` | B40 | 4: steady, slow, fast, heartbeat (palette cycling) (4) (if glow_mark) | 2 | 3 | BFCT- |
| 202 | `tattoo1` | B41 | 16: none, heart, skull, anchor, flame, star, paw, rose, snake, dagger-h... | 4 | 3 | -F-T- |
| 203 | `tattoo1_place` | B42 | 8: L arm, R arm, chest, back, L leg, R leg, belly, neck (8) (if tattoo1) | 3 | 3 | -F-T- |
| 204 | `tattoo2` | B43 | 16: same lists | 4 | 3 | -F-T- |
| 205 | `tattoo2_place` | B43 | 8: same lists (if tattoo2) | 3 | 3 | -F-T- |
| 206 | `tattoo_ink` | B44 | auto + 32 paints x 3 tones | 6/7 | 3 | -F-T- |
| 207 | `body_scar` | B45 | 10: none, arm stitches, patchwork seam, leg patch, bandaged arm, arm sl... | 4 | 3 | -F-TM |
| 208 | `plush_build` | B46 | 8: none, side seams, centre seam, patchwork quilt (fur 1/2/3), laundry... | 3 | 3 | -F-TM |

**limbs** (5 fields, 18 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 209 | `hands` | B47 | 16: auto, plush nubs/mittens, paws with beans, cartoon 4-finger, raptor... | 4 | 2 | BF-TM |
| 210 | `feet` | B48 | 16: auto, plush nubs, paws, hooves, talons, webbed, human-ish, skeletal... | 4 | 2 | -FCTM |
| 211 | `legs` | B49 | 6: plantigrade, digitigrade, stumpy (no knees), bird legs, ghost fade,... | 3 | 2 | -FCTM |
| 212 | `mobility` | B50 | 7: none, cane, crutch, plush wheelchair, hover chair, floaty ring, wal... | 3 | 4 | -FCTM |
| 213 | `prosthetic` | B51 | 13: none, robot arm, robot leg, peg leg, plush-sewn limb, hook, crystal... | 4 | 4 | -F-TM |

**tail** (6 fields, 21 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 214 | `tail_count` | B53 | 8: auto, 1, 2, 3, 4, 5, 7, 9 (kitsune) (8) | 3 | 2 | -F-TM |
| 215 | `tail_len` | B54 | 0..7 | 3 | 2 | -FCTM |
| 216 | `tail_fluff` | B55 | 0..3 | 2 | 3 | -F-TM |
| 217 | `tail_tip_color` | B56 | auto + 32 paints x 3 tones | 6/7 | 2 | -FCTM |
| 218 | `tail_tip` | B57 | 8: none, tip, rings, gradient, flame tip, heart tip, spade, puff (8) | 3 | 3 | -F-TM |
| 219 | `tail_pose` | B58 | 8: auto, hanging, upright (happy cat), curled (spitz), S-curve, wrappe... | 3 | 3 | -FCTM |

**wings** (6 fields, 25 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 220 | `wing_size` | B60 | 0..7 | 3 | 3 | BF-TM |
| 221 | `wing_color` | B61 | auto + 32 paints x 3 tones | 6/7 | 2 | BFCTM |
| 222 | `wing_color2` | B62 | auto + 32 paints x 3 tones | 6/7 | 3 | -F-TM |
| 223 | `wing_pattern` | B63 | 8: none, eye spots, gradient, veins, stars, tattered, sparkle, stripes... | 3 | 3 | -F-TM |
| 224 | `wing_pose` | B64 | 5: folded, half, spread, droopy, idle flutter (5) | 3 | 3 | -F-TM |
| 225 | `wing_pairs` | B65 | 3: 1 pair, 2 pairs (seraph), single wing (3) | 2 | 3 | -F-TM |

**horns** (15 fields, 55 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 226 | `horn_count` | B67 | 4: auto, 1, 2, 4 (4) | 2 | 3 | BF-TI |
| 227 | `horn_curl` | B68 | 0..3 | 2 | 3 | BF-TI |
| 228 | `horn_texture` | B69 | 8: smooth, ridged, cracked, spiral grooves, glossy, glitter, metal-cap... | 3 | 3 | BF--I |
| 229 | `horn_tip_color` | B70 | auto + 32 paints x 3 tones | 6/7 | 3 | BF-TI |
| 230 | `horn_gradient` | B70 | 2: C6 + on/off (if horn_tip_color) | 1 | 3 | BF-TI |
| 231 | `horn_asym` | B71 | 5: none, left broken, right broken, one missing, mismatched size (5) | 3 | 3 | BF-TI |
| 232 | `horn_pos` | B72 | 6: auto, forehead, temples, top, swept back, side (ram) (6) | 3 | 3 | BF-TI |
| 233 | `antennae` | B73 | 8: none, bobble, feathered moth, clubbed butterfly, springy bee, robot... | 3 | 3 | BFCTI |
| 234 | `antenna_color` | B74 | auto + 32 paints x 3 tones (if antennae) | 6/7 | 3 | BF-TI |
| 235 | `head_extra` | B75 | 16: none, dragon crest, mushroom cap, leaf sprout, candle flame, sewn b... | 4 | 3 | BFCTI |
| 236 | `halo_color` | B77 | auto + 32 paints x 3 tones | 6/7 | 3 | BFCTI |
| 237 | `halo_tilt` | B78 | 0..3 | 2 | 3 | BF-TI |
| 238 | `halo_size` | B78 | 0..3 | 2 | 3 | BF-TI |
| 239 | `aura_color` | B80 | auto + 32 paints x 3 tones | 6/7 | 3 | BFCTI |
| 240 | `aura_amt` | B81 | 0..3 | 2 | 3 | BFCTI |

**extra** (7 fields, 17 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 241 | `extra_arms` | B82 | 3: none, +1 pair, +2 pairs (3) | 2 | 4 | -F-TM |
| 242 | `body_eyes` | B83 | 4: none, palm eyes, belly eye, tail-tip eye (4) | 2 | 4 | -F-T- |
| 243 | `buddy_head` | B84 | 2: off, on | 1 | 4 | BF-TM |
| 244 | `belly_mouth` | B85 | 2: off, on | 1 | 4 | -F-T- |
| 245 | `age_look` | B86 | 6: auto (roster trait), baby, kid, teen, adult, elder (6). Drives the ... | 3 | 2 | BFCTM |
| 246 | `hybrid` | B88 | 49: none + any species (6 bits) + blend 0..3 (2 bits): mixes default pa... | 6 | 4 | BFCTM |
| 247 | `hybrid_blend` | B88 | 0..3 (if hybrid) | 2 | 4 | BFCTM |

**outfit** (36 fields, 189 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 248 | `inner_top` | O01 | 12: none, tee, long sleeve, turtleneck, fishnet, striped long sleeve, m... | 4 | 3 | BF-TM |
| 249 | `inner_top_color` | O02 | auto + 32 paints x 3 tones (if inner_top) | 6/7 | 3 | BF-T- |
| 250 | `outer` | O04 | 32: none, open jacket, denim, leather, trench, puffer, cardigan, open h... | 5 | 2 | BFCTM |
| 251 | `outer_color` | O05 | auto + 32 paints x 3 tones (if outer) | 6/7 | 2 | BFCTM |
| 252 | `legwear` | O07 | 12: none, tights, fishnets, ripped tights, striped thigh-highs, leg war... | 4 | 2 | -FCTM |
| 253 | `legwear_color` | O08 | auto + 32 paints x 3 tones (if legwear) | 6/7 | 2 | -FCTM |
| 254 | `legwear_color2` | O09 | auto + 32 paints x 3 tones (if legwear) | 6/7 | 3 | -F-T- |
| 255 | `socks` | O10 | 8: none, ankle, crew, knee, toe socks, frilly ankle, mismatched L/R, p... | 3 | 3 | -FCT- |
| 256 | `sock_color` | O11 | auto + 32 paints x 3 tones (if socks) | 6/7 | 3 | -FCT- |
| 257 | `shoes_color2` | O13 | auto + 32 paints x 3 tones | 6/7 | 3 | -F-T- |
| 258 | `gloves` | O14 | 16: none, mittens, fingerless, opera, boxing, oven mitts, gardening, we... | 4 | 3 | -F-TM |
| 259 | `glove_color` | O15 | auto + 32 paints x 3 tones (if gloves) | 6/7 | 3 | -F-TM |
| 260 | `belt` | O16 | 16: none, simple, studded, bullet belt, chain, sash/obi, fanny pack, to... | 4 | 3 | -F-TM |
| 261 | `belt_color` | O17 | auto + 32 paints x 3 tones (if belt) | 6/7 | 3 | -F-T- |
| 262 | `neck` | O18 | 32: none, bell collar, spiked collar, lace choker, heart-ring choker, n... | 5 | 3 | BF-TM |
| 263 | `neck_color` | O19 | auto + 32 paints x 3 tones (if neck) | 6/7 | 3 | BF-T- |
| 264 | `back` | O20 | 32: none, backpack, critter mini-pack, short cape, long cape, fake-wing... | 5 | 3 | -FCTM |
| 265 | `back_color` | O21 | auto + 32 paints x 3 tones (if back) | 6/7 | 3 | -F-TM |
| 266 | `mask` | O22 | 16: none, cloth, plague doctor, hockey, oni, kitsune, gas mask, bandit ... | 4 | 3 | BF-TM |
| 267 | `mask_color` | O23 | auto + 32 paints x 3 tones (if mask) | 6/7 | 3 | BF-T- |
| 268 | `mask_pos` | O24 | 3: on the face, pushed to the side of the head, on top of the head (3) (if mask) | 2 | 3 | BF-T- |
| 269 | `hat_tilt` | O26 | 5: -2..+2 (5) | 3 | 2 | BF-TM |
| 270 | `hat_size` | O27 | 0..3 | 2 | 3 | BF-T- |
| 271 | `hat_ears` | O28 | 3: auto (ears poke through holes), tucked under, flattened (3) | 2 | 3 | BF-TM |
| 272 | `hat_color2` | O29 | auto + 32 paints x 3 tones | 6/7 | 3 | BF-T- |
| 273 | `ear_acc` | O30 | 12: none, ear bows, ear ribbons, ear warmers, ear-cuff chain, flower be... | 4 | 3 | BFCT- |
| 274 | `ear_acc_color` | O31 | auto + 32 paints x 3 tones (if ear_acc) | 6/7 | 3 | BF-T- |
| 275 | `tail_acc` | O32 | 12: none, bow, ribbon, ring band, bell, striped tail sock, flame, heart... | 4 | 3 | -FCTM |
| 276 | `tail_acc_color` | O33 | auto + 32 paints x 3 tones (if tail_acc) | 6/7 | 3 | -F-T- |
| 277 | `horn_acc` | O34 | 8: none, rubber safety caps, bows, rings, flowers, fairy lights, candy... | 3 | 3 | BF-TM |
| 278 | `horn_acc_color` | O35 | auto + 32 paints x 3 tones (if horn_acc) | 6/7 | 3 | BF-T- |
| 279 | `wing_acc` | O36 | 8: none, ribbons, charms, wing band, piercings, flowers, bells, fairy ... | 3 | 3 | -F-TM |
| 280 | `wrist` | O37 | 12: none, bracelet, friendship-bracelet stack, watch, spiked cuffs, bro... | 4 | 3 | -F-T- |
| 281 | `wrist_color` | O38 | auto + 32 paints x 3 tones (if wrist) | 6/7 | 3 | -F-T- |
| 282 | `prop` | O39 | 32: none, umbrella, plush, balloon, lollipop, book, phone, lantern, can... | 5 | 3 | -FC-M |
| 283 | `prop_color` | O40 | auto + 32 paints x 3 tones (if prop) | 6/7 | 3 | -FC-- |

**mods** (27 fields, 119 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 284 | `top_color2` | O42 | auto + 32 paints x 3 tones | 6/7 | 3 | BFCTM |
| 285 | `bottom_color2` | O42 | auto + 32 paints x 3 tones | 6/7 | 3 | BFCTM |
| 286 | `shoes_trim` | O42 | auto + 32 paints x 3 tones | 6/7 | 3 | BFCTM |
| 287 | `outer_color2` | O42 | auto + 32 paints x 3 tones | 6/7 | 3 | BFCTM |
| 288 | `pattern_on` | O43 | 5-bit mask | 5 | 3 | BFCTM |
| 289 | `outer_pattern` | O44 | 32: 4 extra pieces x (pattern 5 + colour 6) | 5 | 3 | BF-TM |
| 290 | `outer_pattern_color` | O44 | auto + 32 paints x 3 tones (if outer_pattern) | 6/7 | 3 | BF-TM |
| 291 | `bottom_pattern` | O44 | 32: 4 extra pieces x (pattern 5 + colour 6) | 5 | 3 | BF-TM |
| 292 | `bottom_pattern_color` | O44 | auto + 32 paints x 3 tones (if bottom_pattern) | 6/7 | 3 | BF-TM |
| 293 | `legwear_pattern` | O44 | 32: 4 extra pieces x (pattern 5 + colour 6) | 5 | 3 | BF-TM |
| 294 | `legwear_pattern_color` | O44 | auto + 32 paints x 3 tones (if legwear_pattern) | 6/7 | 3 | BF-TM |
| 295 | `hat_pattern` | O44 | 32: 4 extra pieces x (pattern 5 + colour 6) | 5 | 3 | BF-TM |
| 296 | `hat_pattern_color` | O44 | auto + 32 paints x 3 tones (if hat_pattern) | 6/7 | 3 | BF-TM |
| 297 | `emblem` | O46 | 16: none, heart, skull, star, paw, flame, "DOOMO" logo, number, made-up... | 4 | 3 | BF-TM |
| 298 | `emblem_color` | O47 | auto + 32 paints x 3 tones (if emblem) | 6/7 | 3 | BF-T- |
| 299 | `sleeves` | O48 | 9: auto, sleeveless, cap, short, elbow, long, sweater paws (past the h... | 4 | 3 | BF-TM |
| 300 | `neckline` | O49 | 10: auto, crew, V, turtleneck, off-shoulder, sailor collar, peter pan, ... | 4 | 3 | BF-TM |
| 301 | `hood_up` | O50 | 2: off, on (hoodies, cloaks, onesies) | 1 | 2 | BFCTM |
| 302 | `hood_ears` | O51 | 8: none, cat, bear, bunny, devil horns, frog eyes, dino spikes, fox (8) | 3 | 3 | BFCTM |
| 303 | `top_len` | O52 | 4: crop, normal, long, tunic (4) | 2 | 3 | -F-TM |
| 304 | `tucked` | O53 | 2: off, on | 1 | 3 | -F-T- |
| 305 | `outer_closed` | O54 | 2: open, closed (if outer) | 1 | 3 | -F-TM |
| 306 | `bottom_len` | O55 | 6: micro, short, knee, midi, ankle, floor (6) | 3 | 3 | -FCTM |
| 307 | `cuffs` | O56 | 2: off, on | 1 | 3 | -F-T- |
| 308 | `wear` | O57 | 4: new, patched, ripped, shredded punk (4) | 2 | 3 | -F-T- |
| 309 | `fabric` | O58 | 8: auto, cotton, knit rib, denim twill, glossy vinyl, satin, sequins (... | 3 | 3 | BF-TM |
| 310 | `fit` | O59 | 4: fitted, regular, oversized (a big hoodie on a tiny plush), baggy (4) | 2 | 2 | -F-TM |

**wardrobe** (5 fields, 29 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 311 | `outfit_sleep` | O60 | 33: sleep, swim, formal, battle: each a 5-bit index into 32 outfit sets... | 6 | 4 | BFCTM |
| 312 | `outfit_swim` | O60 | 33: sleep, swim, formal, battle: each a 5-bit index into 32 outfit sets... | 6 | 4 | BFCTM |
| 313 | `outfit_formal` | O60 | 33: sleep, swim, formal, battle: each a 5-bit index into 32 outfit sets... | 6 | 4 | BFCTM |
| 314 | `outfit_battle` | O60 | 33: sleep, swim, formal, battle: each a 5-bit index into 32 outfit sets... | 6 | 4 | BFCTM |
| 315 | `auto_switch` | O61 | 5-bit mask | 5 | 4 | -FC-- |

**motion** (30 fields, 89 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 316 | `idle` | A01 | 16: auto (persona movement), bouncy, sway, fidget, stoic, float bob, ho... | 4 | 2 | BFCTM |
| 317 | `fidget` | A02 | 0..3 | 2 | 3 | -FCT- |
| 318 | `walk` | A03 | 16: auto, normal, waddle, strut, skip, tiptoe sneak, stomp, prance, shu... | 4 | 2 | -FCTM |
| 319 | `run` | A04 | 16: auto, sprint, arm flail, speedy lean, all-fours gallop, penguin wad... | 4 | 2 | --CTM |
| 320 | `stride` | A05 | 0..3 | 2 | 3 | --CTM |
| 321 | `sit` | A06 | 8: auto, cross-legged, kneel, dangle legs, sprawl, cat loaf, perch, pu... | 3 | 3 | -FC-- |
| 322 | `sleep_pose` | A07 | 8: auto, curled up with the tail, starfish, belly up with paws up, bel... | 3 | 3 | -FC-- |
| 323 | `dance` | A08 | 16: auto (from liked music), bop, robot, twirl, shuffle, headbang, disc... | 4 | 3 | -FCTM |
| 324 | `emote` | A09 | 32: paw wave, heart hands, peace sign, finger guns, salute, curtsy, bow... | 5 | 3 | BFCTM |
| 325 | `greeting` | A10 | 11: auto, wave, bow, hug attempt, high five, nose boop, ear wiggle, jum... | 4 | 3 | BFC-- |
| 326 | `laugh` | A11 | 10: auto, giggle behind paws, belly laugh, snort, villain cackle, silen... | 4 | 3 | audio |
| 327 | `cry` | A12 | 7: auto, waterfall tears, single tear, ugly sob, quiet sniffle, howl, ... | 3 | 3 | BF--- |
| 328 | `angry` | A13 | 8: auto, stomp, steam puff, fur poof, pout with crossed arms, flames, ... | 3 | 3 | BFC-- |
| 329 | `love` | A14 | 8: auto, hearts, blush + steam, frantic tail wag, face in paws, float-... | 3 | 3 | BFC-- |
| 330 | `victory` | A15 | 8: auto, gun twirl, heart pose, flex, jump, bow, spin, hug own plush (8) | 3 | 3 | ---TM |
| 331 | `taunt` | A16 | 8: none, tongue blep, tail wiggle, raspberry, 2-second dance, wave at ... | 3 | 3 | ---TM |
| 332 | `hurt` | A17 | 5: auto, flinch, squeak-squash, stagger, stuffing puff (cute gore) (5) | 3 | 3 | ---TM |
| 333 | `faint` | A18 | 7: auto, face-plant, spin and fall, cartoon stars, plush deflate, ghos... | 3 | 3 | -F-TM |
| 334 | `lefty` | A19 | 2: right, left | 1 | 2 | -FCTM |
| 335 | `blink` | A20 | 7: auto, normal, slow, flutter, double blink, wink habit, side lids (l... | 3 | 3 | BF-T- |
| 336 | `blink_rate` | A21 | 0..3 | 2 | 3 | BF--- |
| 337 | `ear_expr` | A22 | 0..3 | 2 | 3 | BFCTM |
| 338 | `wag` | A23 | 7: auto, swish (cat), wag (dog), helicopter, tip twitch, slow sway, st... | 3 | 3 | -FCTM |
| 339 | `wag_amt` | A24 | 0..3 | 2 | 3 | -FCT- |
| 340 | `breath` | A25 | 0..3 | 2 | 3 | BFCTM |
| 341 | `bounce` | A26 | 0..3 | 2 | 3 | BF-TM |
| 342 | `rest_expr` | A27 | 12: auto (persona), neutral, happy, smug, sleepy, grumpy, shy, deadpan,... | 4 | 3 | BFCT- |
| 343 | `talk` | A29 | 7: auto, mouth flap, chomp, muzzle only, hand gestures, whisper lean, ... | 3 | 3 | BF--- |
| 344 | `gaze_habit` | A30 | 6: auto, eye contact, looks away (shy), follows the cursor, stares, la... | 3 | 3 | BF--- |
| 345 | `mood_motion` | A31 | 0..3 | 2 | 3 | -FCT- |

**voicex** (7 fields, 21 bits max)

| off | field | wish | values | bits | wave | shows on |
|---:|---|---|---|---:|---:|---|
| 346 | `footstep` | A32 | 12: auto (from feet), pitter-pat, plush squeaker, clop, squish, clank, ... | 4 | 3 | audio |
| 347 | `babble` | A33 | 16: auto (voice quality), square, triangle, saw, noise, "meow", "yip", ... | 4 | 3 | audio |
| 348 | `voice_flavor` | A34 | 9: none, meow-y, bark-y, chirpy, squeaky, burbly, echo (ghost), bitcru... | 4 | 3 | audio |
| 349 | `sing_range` | A35 | 0..3 | 2 | 3 | audio |
| 350 | `vibrato` | A35 | 0..3 | 2 | 3 | audio |
| 351 | `quirk_sound` | A36 | 8: none, sparkle sneezes, bubble hiccups, big yawns, purrs when happy,... | 3 | 3 | audio |
| 352 | `loudness` | A39 | 0..3 | 2 | 3 | audio |

**Totals.** 353 fields (48 legacy, 5 voice, 300 new) use 353 of 384 bytes. The densest possible blob of a citizen is
1,465 bits. The real worst blob is 1,553 bits = **195 B**, counting headers and masks. Of the new fields, 168 are
enums (1,711 values, 1,339 of them drawn), 63 are sliders, 65 are colour fields and 4 are flag sets.

### 1.7 The packed blob (heap, save, Look Book and Look Password share it)

Bits are LSB-first, using the same `wbits()`/`rbits()` as `save.c`, and the blob is padded to a whole byte.

| bits | field | meaning |
|---:|---|---|
| 4 | `tver` | field-table version (1). Sections only ever grow at their end, so a decoder knows each section's field count for every older `tver` |
| 2 | `base` | `BK_SLOT` (the slot's natural base: roster[id] on premade islands, `gen(seed,id)` on random ones, `resident_default` on empty ones) / `BK_ROSTER` / `BK_SEED` / `BK_PRESET` |
| 0 / 10 / 33 / 7 | base param | SLOT: none. ROSTER: id. SEED: 32-bit seed + hellborn bit. PRESET: species preset 0..71 (applied to `resident_default`) |
| 1 | `fat_auto` | ignore the base's new fields (they start at 0 = auto). Set for v1-migrated custom citizens and hand-made ones |
| 1 | `tones` | colour fields use 7-bit widths |
| 1 (+1) | legacy present (+ mode) | full: all 48 fields at width, *skipping* children whose parent is 0 (`horn_color` and `horn_size` when `horns == 0`, `hat_color`, `pattern_color`, `accessory_color`, `marking_color`). Delta: 48-bit mask + values |
| 1 (+1) | voice present (+ mode) | full 25 bits, or a 5-bit mask + values |
| 1 (+1) | fat present (+ mode) | section mode: 23-bit section mask, then per section a full/delta bit and either all fields (with parent skipping) or an n-bit field mask + values. Sparse mode: 9-bit count, then (9-bit field index + value) per field |

The encoder tries every candidate base (`BK_SLOT`, the base the creator tracked, `BK_PRESET` of the species) with
every mode and keeps the smallest. Decoding builds the base, applies the delta, then runs `look_sanitize()` (registry
ranges plus the wave gate), so a CRC-valid but hostile blob can never yield an out-of-range byte.

**Measured sizes.** These come from `sim.py`, which runs the real 1000 roster looks through these widths, with
uniformly random new fields.

| citizen | blob | per 33.5 KB heap |
|---|---:|---:|
| untouched premade / random / seeded citizen | 0 B | unlimited |
| "Randomise all" (`BK_SEED`), no edits | 5 B | 6,860 |
| premade with a 5-field makeover | 11 B | 3,118 |
| seed + 10 new fields tweaked | 22.6 B | 1,517 |
| fully hand-made, legacy detail only | **31.5 B avg** (34 B max) | **1,089** |
| hand-made + 5 / 10 / 20 / 40 new fields | 40.6 / 48.5 / 64.4 / 87.1 B | 844 / 707 / 532 / 393 |
| premade makeover (5 legacy) + 20 new fields | 44.3 B | 774 |
| hand-made with all 300 new fields set | 177.5 B | 193 |
| absolute worst (adversarial, every field at max width) | 195 B | 175 |

Premade and random citizens are rich *by default*. The roster authors new fields for every premade citizen (3.4), and
the generator draws new fields for every random citizen (3.5). Those cost no heap at all.

### 1.8 The cold store (`resstore.h`, `resstore.c`)

```c
#define RES_HEAP_BYTES  34304u          /* section 2.5: forced into [34,034; 34,810] */
#define RES_IDS         (DOOMO_MAX_RESIDENTS + 1 + RES_BOOK)  /* 1000 + player + 16 Look Book */
#define RES_BOOK        16
#if defined(DOOMO_PLATFORM_GBA)
#define RES_CACHE 12
#else
#define RES_CACHE 16
#endif
static uint8_t  res_heap[RES_HEAP_BYTES];     /* blobs, contiguous, sorted by id      */
static uint16_t res_dir[RES_IDS + 1];         /* byte offset of id's blob; len = dir[id+1]-dir[id] */
uint8_t         res_ver[RES_IDS + 3];         /* bumped on every change of id's look  */
typedef struct ResLine { int16_t id; uint8_t ver, pin; uint32_t stamp; DoomoLook l; } ResLine;
static ResLine  res_cache[RES_CACHE];         /* 392 B each                           */

const DoomoLook *res_look(int id);            /* decoded + sanitized, from the cache  */
uint8_t  res_look_ver(int id);                /* no decode: for cache keys            */
void     res_pin(int id), res_unpin(int id);  /* player, creator subject, render job  */
void     res_get(int id, DoomoCitizen *out);  /* hot + look copy                      */
int      res_commit(int id, const DoomoCitizen *c, const ResBase *tracked); /* RES_OK / RES_E_FULL */
int      res_cost(int id, const DoomoCitizen *c, const ResBase *tracked);   /* bytes it would take */
void     res_clear(int id);                   /* slot emptied: drop the blob          */
uint32_t res_heap_used(void);                 /* the creator's wardrobe meter         */
int      res_base_look(int id, DoomoLook *out);/* base only (premade, random, default) */
```

- **Decode a miss.** First build the base. A premade base unpacks the packed roster record (about 4k GBA cycles) and
  runs `gen_fat()` with the roster's fat seed plus its authored overrides. A random or seed base runs
  `resident_generate()`, whose legacy draws are unchanged, followed by `gen_fat()`. A preset base runs
  `resident_default()` plus the preset. Then the blob is applied and the look sanitized. Native timings on this host
  are `resident_generate` 0.84 µs, portrait 76 µs and chibi 18.3 µs. The GBA bench measures portraits at 763k
  cycles and chibis at 250k, which gives 10-14k GBA cycles per native µs, so a decode is about **30-40k GBA cycles**:
  4-5% of a bust render and at most 7% of a 30 Hz tick. Only cache misses pay it.
- **Hits are free.** The portrait cache keys on `(rid, res_ver[rid])` instead of hashing the 48-byte look (4.7), and
  stores the look's animation mask in the cache entry. A cached portrait, chibi or figure is therefore found without
  decoding anything, as fast as today.
- **Commit.** Encode into a 196 B scratch. If the result equals "no blob", drop the blob. Otherwise `memmove` the
  heap's tail. That is at most 34 KB, about 0.1M GBA cycles with DMA3, and happens only on creator commit, birth or
  load. Then fix `res_dir[id+1..]`, write the hot mirrors, bump `res_ver[id]`, drop the cache line and call
  `portrait_invalidate(id)`. If the blob does not fit, the call returns `RES_E_FULL` and changes nothing (5.7).
- **Pointer rule.** A `res_look()` pointer stays valid until `RES_CACHE - pins - 1` further misses. In practice:
  hold at most 4 at once and never across a call that may decode (a render, a birth, a save). Native and test builds
  poison evicted lines with `0xA5`, and `res_look_check()` asserts. A test decodes `RES_CACHE + 1` ids round-robin
  and checks that stale use is caught.
- **Only one write path.** `res_commit()` is the only writer of blobs and mirrors. The test `mirrors_match` decodes
  every citizen after 10^4 random commits, births and loads and compares the mirrors.

### 1.9 In vs cut

| | count | items |
|---|---:|---|
| **IN: new per-citizen fields** | 300 | every C and D wishlist item (sections 3-6). The multi-part items are split into one byte per piece: F28 (special eye L/R), F82 (polish + colour), B43 (tattoo 2 + placement), B70 (tip colour + gradient), B78 (halo tilt + size), B88 (hybrid + blend), A35 (range + vibrato), O42 (4 trim colours), O44 (4 pieces x pattern + colour), O60 (4 outfit slots). A40 job goes into the hot record |
| **IN: append-only list growth** | 21 lists, +306 values | F01, F07, F25, F29, F35, F47, H01, H13, E01, B01, B52, B59, B66, B76, B79, B87, O03, O06, O12, O25, O45 (O41: the legacy accessory slot stays as it is) |
| **IN: derived / no storage** | 6 | A28 expression catalogue (+12 EXPR_*), A37 idle prop habit, A38 motion presets, O62 outfit presets 20 to 60, O63 job uniforms, O64 dirt from hygiene |
| **IN: creator tools** | 22 | T01-T17 and T19-T23. Section 5 |
| **IN: options** | 373 | 311 new + 62 extended or newly wired. Section 6 |
| CUT: B23 +8 natural fur tones | | The palette is full ("fixed - art, textures and saves depend on it"). Tones give every colour field 96 colours for free instead |
| CUT: E7 island dyes | | The GBA draws everything into one 8bpp Mode 4 framebuffer with one 256-entry palette and no hardware sprites, so there is no per-image palette to put dyes in. The PS1 could, but a PS1-only look breaks Look Password parity. `LK_COLOR` values 97..127 stay reserved |
| CUT: T18 real 3D model in the PS1 creator turntable | | The creator is a 2D overlay scene with no 3D viewport. The turntable keeps `fighter_render` (8 views). The 3D model shows the fat look in-game through 4.4 |
| CUT: fully custom situational outfits (part of O60) | | 4 full extra outfits per citizen would cost up to 4 x 30 fields of heap each. We keep 4 indices into 32 outfit sets, and the player has the Look Book (16 full looks) |
| DEFERRED (field stored now, painter later) | 13 fields + 24 species | wave 4: species +24, extra arms, body eyes, buddy head, belly mouth, mobility aid, prosthetic, hybrid species + blend, 4 situational outfits, auto-switch. They are in the struct, the blob and the save from day one, so they ship without a format change. Sanitize zeroes them until `LOOK_WAVE >= 4` |

### 1.10 Worked example: the relayed brief's cheeky Hellvile devil

The relayed user request asks for a "big butt devil" who keeps showing it off, Red-Guy style, with a slightly
dungeon-punk edge "because this is hell". Every part of that is a stock value of the fat look, so a premade citizen
of this kind costs **0 B of RAM** (authored in the roster text, 3.4) and a player-made one costs one blob of about
**26 B** (23 non-auto fields, 1.7). Reference images for "base his designs from these characters" belong to the
character brief; nothing here depends on them.

| what the brief wants | field (wish id) | value | where it shows |
|---|---|---|---|
| a devil | `species` | `SPECIES_IMP`; `horns` = `HORNS_DEVIL_TALL`, `tail` = `TAIL_DEVIL`, `skin` = red paint, `material` (B18) = smooth skin, `sheen` (B21) = glossy | P F C T M |
| cartoon-nude like the Red Guy | `top` = `TOP_NONE`, `bottom` = `BOTTOM_NONE`, `shoes` = `SHOES_BARE` | the painters already draw a bare plush body for these values (`portrait_body.c`, `model.c:306-308`) | F C T M |
| the big butt | `hips` (B05) = 7, `belly` (B06) = 2, `roundness` (B17) = 3, `muscle` (B16) = 0, `posture` (B13) = sassy hip, `tail_pose` (B58) = S-curve | `pb_layout()` widens the hip capsule and shifts the pelvis; the fighter's back views (turntable directions 3..5) and the PS1 model's hip box show the silhouette | F C T M |
| "annoyingly showing it off" | `emote` (A09) = wiggle, `taunt` (A16) = tail wiggle, `victory` (A15) = spin, `idle_style` (A01) = sway, `greeting` (A10) = cool nod | the pose table for *wiggle* is a turn-away-and-wiggle (4 frames, back view, tail swish). The *tail wiggle* taunt is the same pose on the taunt button in the shooter. No new values, so no new bits | F C T M |
| the face | `eye_style` = villain triangle, `pupil` (F08) = vertical slit, `sclera` (F15) = yellow (goat), `brow_angle` (F32) = +3, `mouth_curve` (F50) = +3, `teeth` (F53) = molar grin, `tongue` (F56) = blep, `laugh` (A11) = villain cackle | P F T (M through the head impostor, 4.4) |
| "slightly bdsm-y" | `neck_item` (O18) = spiked collar, `wrist` (O37) = spiked cuffs + `wrist_color` black, `belt` (O16) = chain, `legwear` (O07) = fishnets, optional `wrist` = broken shackle (unlock U49) | Hellvile's punk/dungeon kit from the wishlist as it stands: collar, chains, studs, shackles. These are the cartoon-villain register and all of them are already catalogued | P F T M |

What this does **not** change: the body sliders stay the stylised plush ranges of section 1.4 (hips 0..7 moves the hip
capsule by at most 3 px on the 48x80 figure), bare bodies render as smooth or furred plush exactly as `TOP_NONE` does
today, and the fixed rules of the wishlist (toy-like clothing, nothing sexualised, citizens never bleed) stand. The
Red Guy is a cartoon-network gag; that is the register this example lives in. A "Hellvile-ify" restyle (T22) and a
species preset ("Imp 3: Cheeky") can ship this look as one button in the creator (5.5).

---

## 2. Memory math

### 2.1 Starting point (measured, HEAD `43af439`)

- **GBA** (`docs/GBA.md`, `tools/memreport.py`). EWRAM 254,464 / 262,144 used, so **7,680 B free**. The biggest
  users are `doomo_town` 109,524, `doomo_map` 32,780, the portrait pool 18,432, `gba_big` 17,408 and the scene
  scratch 12,288. IWRAM 25,540 / 32,768, leaving 6,460 B for the 6 KB stack (high-water mark 1,744 B). ROM 2,428,644
  B.
- **PS1** (`scratchpad/ps1all3/disc/doomogachi.map`, after the overlay pass). `_end = 0x801dd330`. With the stack top
  at `0x801FFFF0` that is **142,528 B (139.2 KB) below the stack top**, of which 65,536 B is the stack reserve
  `elf2exe.py` enforces. Resident const data includes `doomo_roster_premade` 96,000, `doomo_roster_bio_text` 44,922,
  `doomo_sfx_pcm_data` 47,793, `doomo_roster_edges` 9,240 and `doomo_roster_job_text` 8,056. BSS includes
  `doomo_town` 109,524. The portrait code is 141,884 B of resident `.text` (`portrait_*.c`).
- **Re-check at HEAD `ae10664`** (4 commits after `43af439`: fishing/cooking, 5 Hell Breach levels and arena modes,
  profiling hooks, native draw-call counters). `docs/GBA.md` and `docs/PS1.md` are unchanged. The only EWRAM growth
  is the harness mailbox (`gba.ld`: 16 to 64 B, `GbaMailbox` gained `work_sum/work_n/work_max` and `name[20]`),
  **+48 B**, plus 4 pointer/byte statics (`ks`, `fs`, `cook_music`, `fish_music`, about 12 B). The fishing/cooking
  scenes keep their state and particle pool in the scene scratch (`FCOK` tag, `fishcook_int.h`), not in `.bss`. So the
  baseline is **7,620 B free** and every "after" figure below is 60 B lower than printed; none of the gates in 2.7
  moves. On the PS1, `fish_*`, `cook_*`, `levels*` and `arena*` all belong to overlay groups (`ovl_plan.py`:
  `fishcook`, `combat`), so their new code lands in the 108 KB overlay slot, not in resident RAM.
- **The task's premise is out of date, and that does not matter.** The brief assumed the GBA was about 33 KB over,
  with the CI pass landing somewhere between 0 and 16 KB free. The pass has since landed at 7,680 B free. Design B's
  data changes are a fixed **+4,576 B** of free EWRAM whatever the starting point: 0 KB free would become 4.5 KB, and
  16 KB free would become 20.5 KB. No GBA code or const goes into EWRAM.

### 2.2 Data (BSS) changes, both platforms

| line | GBA EWRAM | PS1 RAM |
|---|---:|---:|
| `doomo_town.res[1000]` + `player`: 96 B to 48 B hot | -48,048 | -48,048 |
| `doomo_town.opt`: 64 to 256 B (island rules, 6.1) | +192 | +192 |
| `doomo_town.unlocks2`: second unlock bank, 512 bits (wishlist E5) | +64 | +64 |
| `doomo_town.progress`: 32 to 48 counters (makeovers, photos, rainy days, ...) | +64 | +64 |
| `doomo_settings`: global options + 3 remap tables (header-saved, 6.1) | +512 | +512 |
| `res_heap` (2.5) | +34,304 | +34,304 |
| `res_dir`: uint16 x (1001 + 16 + 1) | +2,036 | +2,036 |
| `res_ver`: uint8 x 1020 | +1,020 | +1,020 |
| decode cache: GBA 12 x 392 B, PS1 16 x 392 B | +4,704 | +6,272 |
| portrait `Job`: `DoomoLook` 48 to 384 B, `PCtx` +40 `PMat` (6 B each) | +576 | +576 |
| creator `CrState` 6,560 to about 8,500 B | 0 (scene scratch, 12,288 B, 5.9) | 0 |
| palette filter (6.3) | 0 (writes palette RAM directly) | 0 (CLUT upload from the stack) |
| **data subtotal** | **-4,576** | **-3,008** |

### 2.3 GBA ROM and IWRAM

- **IWRAM: +0 B (rule).** The part-script interpreter, decoder, encoder and every new painter are THUMB in ROM. They
  draw through the existing `P_FAST` IWRAM loops (`sh_span`, `capsule_plain`, `scan_transitions`). `memreport.py`'s
  IWRAM line is a CI gate: any growth fails the build. The `animated()` locals (`PCtx` +240 B, `DoomoLook` +336 B)
  grow the IWRAM *stack* by 576 B, taking the high-water mark from 1,744 to about 2,320 B of the 6,460 B available.
- **ROM: about +190 KB.** That is about 60 KB of new code, 92 KB of part scripts and pose tables, 30 KB of names and
  creator tables, and 3.5 KB of the registry, offset by -31 KB from the packed roster and -24 KB from BPE text. The
  ROM goes from 2.43 to about 2.6 MB, still in the 4 MB cart.

**GBA EWRAM after: 7,680 + 4,576 = 12,256 B free.**

### 2.4 PS1 code and const changes

On the PS1 everything is RAM, so the new code and art must be paid for.

| line | PS1 RAM | how / why |
|---|---:|---|
| packed roster records | **-31,062** | measured: 62,938 B + 2,000 B offsets vs 96,000 B (v2 widths, parent skipping, about 2 authored overrides per citizen) |
| roster bios + jobs as BPE | **-23,350** | measured: bios 44,922 to 23,981 (+368 B, 184 pair rules), jobs 8,056 to 4,477 (+402 B), 400 B decoder. `roster_bio(id)` and `roster_job(id)` expand one string into a static ring of 4 |
| SFX PCM to SPU RAM | **-43,413** | `SFX.BIN` holds ADPCM encoded at build time by the existing `adpcm_encode`; it is read once at boot and uploaded to SPU `0x02000...`. `doomo_sfx_pcm_data` becomes a `--defsym` virtual address, as the packed art already does. A 4-bit wave table (36 SFX x 80 columns) feeds the jukebox's `build_wave()`. This also frees SPU RAM for the reverb option AU11 |
| `resstore.c` + registry + `gen_fat` code and weights | +15,360 | |
| part-script interpreter | +6,144 | |
| part scripts + pose tables | +56,320 | 92 KB raw, stored as LZ blocks (about 47 KB) + an 8 KB decoded-part LRU (4.2) |
| painter changes (materials, sliders, occlusion, new layers) | +12,288 | |
| option consumers, palette filter, remap, settings code | +13,312 | option names, help and value strings live in the free-play overlay group (6.1) |
| `model.c` fat body + head impostor | +8,192 | |
| **code/const subtotal** | **+13,791** | |
| **PS1 total (data + code)** | **+10,783** | |

**PS1 after: 142,528 - 10,783 = 131,745 B (128.7 KB) below the stack top**, which is 66,209 B above the 64 KB stack
reserve.

Overlay groups (`ps1/tools/ovl_plan.py`): the creator group grows from 50 KB to about 95 KB (new UI code, row tables,
presets and the 1,645 new value names). The free-play screens group grows from 70 KB to about 88 KB (option strings
and help). Both stay under the 108 KB overlay slot, so the slot itself does not grow. `ovl_plan.py` keeps a section
resident if two groups reach it. The combat pause menu therefore gets its own copy of the names of its 7 in-place
options, and nothing in `doomo_option_meta[]` (resident) points at strings.

**Levers if a wave comes in over budget** (in order): smaller PS1 decode cache 16 to 12 (-1.5 KB); pack the roster
edges as sorted deltas (-3.7 KB); BPE the existing catalog name tables (about -3 KB); defer wave-4 content (parts are
data, so cutting content is cheap); pose tables of scene-only animations into their overlay groups.

### 2.5 Why the heap is exactly 34,304 B

The heap size is not a free choice. Two hard limits pin it.

**Ceiling: the save slot.** Slot 1 is 88 KB and both platforms honour it (the GBA flash is full: slot 0 is 40 KB at
`0x00000`, slot 1 is 88 KB at `0x0A000`). Today `test_save.c:155` guarantees the worst case fits. The v2 worst case
is computed from field widths, not measured:

| part | worst case | how |
|---|---:|---|
| header (48 B + 512 B settings, uncompressed) | 560 | 3.1 |
| town fixed data (scalars, items, weapons, pet, story, opt[256], unlocks 192 B, progress 48 x 4, graves) | 1,527 | `write_town()` |
| edges | 9,985 | 2048 x 39 bits (the raw edge form) |
| residents' hot records + blob framing | 35,035 | 1001 x (2 tag + 254 field + 24 framing bits) |
| extension chunks | 7,494 | SONG 5,352 + NICK 906 + FPLY 270 + FISH, with headers (rounded up) |
| heap | H | |
| LZ worst-case expansion | +0.79 % | one control byte per 128 literals |

`(560 + 1,527 + 9,985 + 35,035 + 7,494 + H) x 128/127 <= 90,112` gives **H <= 34,810**.

**Floor: lossless v1 migration.** A v1 citizen whose legacy look and voice differ from its base becomes a legacy-only
blob of at most 34 B (269 bits). The adversarial v1 island has 1001 of them, so **H >= 34,034**.

So `RES_HEAP_BYTES = 34,304` (33.5 KB). The worst-case save is then 89,606 B, 506 B under the limit. `test_save`
asserts this bound from the field registry and each extension chunk's declared maximum. A new `max_size` member of
`DoomoSaveExt` means that any later chunk that grows forces this check to be redone.

The story slot (40 KB) gets an extra cap: `res_commit()` refuses to grow the heap beyond 8 KB in story mode. Story
islands are premade, and the only things edited there are the protagonist and the odd makeover.

### 2.6 What fits

| island | heap use |
|---|---|
| Premade island (story or free play), untouched | 0 B. All 1000 premade citizens show their authored new fields |
| Random island, untouched | 0 B. All 1000 random citizens show generated new fields |
| Premade island, 300 makeovers of 5 legacy + 20 new fields | 13.3 KB (39 % of the heap) |
| Empty island, 1000 hand-made citizens with legacy detail | 31.5 KB (fits, so no regression from today) |
| Empty island, 532 hand-made citizens with 20 new fields each | 33.5 KB |
| Empty island, 1000 "Randomise all" citizens with 10 tweaks each | 22.6 KB |

### 2.7 Budgets and CI gates per wave

| gate | where | limit |
|---|---|---|
| GBA EWRAM free | `gba/tools/memreport.py` (exists) | >= 12,000 B after wave 3. Each wave may not reduce free EWRAM below the previous wave minus 1 KB |
| GBA IWRAM | `memreport.py` + the linker `ASSERT` | +0 B vs today |
| PS1 below stack | `ps1/tools/elf2exe.py` (exists, 64 KB) | add a second threshold: >= 128 KB (131,072 B) |
| PS1 overlay groups | `ovl_plan.py` report | creator <= 100 KB, free-play screens <= 100 KB |
| save worst case | `test_save` | <= 90,112 B, computed |
| render cost | `test_portrait` + the GBA bench (4.8) | pixel visits < 12,000, part ids <= 240, GBA worst bust <= 1.4M cycles |

---

## 3. Save versioning, migration, roster and determinism

### 3.1 Header v2

`DOOMO_SAVE_VERSION 2`. The header grows from 48 B to 560 B: the 48 B of today (with `version = 2`) followed by a
512 B `DoomoSettings` block. The block is uncompressed and protected by its own CRC-16. The save's CRC-32 now covers
`[16,48)` and `[560,total)`, combined with the existing `crc32_combine()` (`save.c:156-192`). A settings-only rewrite
(3.7) therefore never invalidates the island. At boot both slots' headers are read without decoding the LZ body, and
the settings with the higher `seq` win. `save_info_from()` (`save.c:1144`)
changes its strict `version == 1` test to `version == 1 || version == 2`. A v1 header has no settings block, so the
defaults are used, and when no v2 settings exist anywhere yet, the global-scope values found in the v1 `opt[64]` are
adopted.

### 3.2 Body v2 (inside the same LZ + CRC)

The order is today's `write_town()` order with five changes:

1. `opt`: `u16 count` + count raw bytes (256 now; append-only from here on).
2. `unlocks`: 128 B (bank 1) + `u16 count` + bank 2 bytes (64 B now).
3. `progress`: `u8 count` + count x u32 (48 now).
4. The player and every resident: the **hot** record (schema below) + the cold part. The cold part is
   `u1 has_blob`, and if set `walign()`, a `varint len` and `len` raw blob bytes copied straight out of `res_heap`.
5. The 2-bit run tags stay (empty run / same as base / delta / full), but "same as base" now means the hot record
   equals the base hot record **and** `has_blob == 0`.

**Hot v2 schema** (`rfields_v2[]`, 254 bits max). name (4-bit length + 7-bit chars), persona 5 x 4, stat 8 x 7,
status 3, flags 8, pronoun 3, romance 3, bday_month 4, bday_day 5, like_food 6, like_hobby 5, like_music 4,
like_color 5, hate_food 6, fear 5, catchphrase 7, exorcism 8, level 7, mood 4, activity 4, death_cause 3, job 7.
Mirrors (`species`, `skin`, `soul`, `xflags`) are **not** saved, which keeps the bound in 2.5. A load rebuilds them
with `res_mirrors()` and never runs a full decode:

- citizens without a blob take the base's mirrors. A premade base unpacks the roster record (about 4k cycles). A
  random base reuses the legacy generator run that the hot delta base needs anyway, plus the `outfit`, `extra` and
  `motion` substreams for neck item, age and handedness (3.5).
- citizens with a blob read the 7 source fields straight out of the blob during the structural validation pass that
  the load already does (2-5k cycles), and fall back to the base for fields the blob does not carry.

### 3.3 Reading a v1 save (lossless)

1. `save_v1.c` is a frozen copy of today's `rfields[]` (85 fields, `save.c:513-545`), the v1 delta reader and
   `base_of_v1()`. `base_of_v1(PREMADE)` = the **legacy view** of the packed roster record, which a golden test proves
   is byte-identical to today's `roster_data.c`. `base_of_v1(RANDOM)` = today's `resident_generate*()` output. The
   legacy draw sequence is not touched (3.5), so the existing `fingerprints.txt` lines prove it.
2. Each v1 citizen is decoded into a temporary 96 B v1 record. The hot fields are copied. The mirrors come from its
   legacy look (`species`, `skin`, `soul` from halo + accessory, `age = AUTO`, `RFX_PREMADE_SELF` if the base is the
   roster and the name still matches).
3. If the legacy look and voice equal the base's, no blob is written. The citizen inherits the base's new fields
   (authored for premades, generated for random citizens) and gains the new richness automatically. Otherwise the
   citizen gets a `BK_SLOT` blob with `fat_auto = 1` and only the legacy/voice delta, so **anything the player touched
   looks exactly as it did in v1**. Accessories in the legacy slot keep rendering in that slot (O41). The creator
   offers "move glasses / scarf / mask into their own slot" the next time it opens.
4. The bound in 2.5 guarantees the heap holds even an adversarial v1 island (34,034 <= 34,304).
5. The next save writes v2. The v1 image on flash or memory card is untouched until then.

v2 also never needs migration code for future look fields. New fields take reserved bytes and are appended to a
section, and the blob's `tver` tells the decoder how many fields that section had when the blob was written.

### 3.4 The premade roster v2 (deterministic, ROM on GBA, RAM on PS1)

- `tools/roster_build.py` reads the same `core/data/roster/*.txt` plus new optional keys (one per new field, e.g.
  `pupil=SLIT`, `glasses=ROUND`, `outer=CARDIGAN`). It emits `core/gen/roster_data.c` as a packed bitstream:
  `doomo_roster_pack[]` (62,938 B), `doomo_roster_pack_ofs[1000]` (u16), the BPE bios, the BPE jobs and their pair
  tables. Traits stay `u16[1000]`.
- **Premade new fields are generated plus authored.** `fat = gen_fat(roster_fat_seed(id), legacy look, district,
  persona)`, then the record's explicit keys override. `roster_fat_seed(id)` is FNV-1a of the name XOR the id, so it
  is stable when a citizen's text moves within a file. The generator conditions on species, district and persona,
  so premades stay coherent: catfolk get slit pupils and whiskers more often, Hellvile folk get black sclera, chokers
  and spikes, elders get grey muzzles. This costs about 4 B per citizen instead of about 18 B for fully authored
  fields, which is what keeps the PS1 roster at 63 KB.
- `--assign-coverage` is a one-time deterministic pass. It finds every new enum value that no premade uses (the
  strict test `test_roster.c:214` extends to all 1,645 new values). It then writes an override for it into the
  most-fitting citizen's text, by a species/district/persona score with the citizen's id as tie-break. Humans then
  tune the text.
- `make -C core gen` / `check-gen` keep `roster_data.c` byte-identical to the generator's output, as today.
- **Compatibility golden.** `core/tests/golden/roster_v1.txt` holds the FNV-1a hash of every premade's 96 B legacy view,
  computed *today* from the current `roster_data.c` before the switch. `test_roster_v1_compat` decodes all 1000 packed
  records and must match each hash. This is what makes v1 delta saves decode correctly.

### 3.5 Generator (random islands, babies, creator randomise)

- `gen_look()` and `gen_rest()` (`resident.c:309-639`) keep their exact draw sequence. **No legacy output changes**,
  so every existing line of `core/tests/golden/fingerprints.txt` stays valid untouched.
- `gen_fat(DoomoLook *l, uint32_t seed, const GenCtx *ctx)` fills the 300 new fields from **independent per-section
  substreams**: `DoomoRng` seeded with `fnv(seed ^ 0xFA7F1E1D, section)`. So:
  - a section can be generated on its own (the save loader computes the `soul` mirror for random bases from the
    `outfit` substream's neck item alone, about 200 cycles, without running all of `gen_fat`);
  - reordering sections never changes another section's output;
  - each section first draws "use this section" (for example 35 % for makeup, 60 % for outfit layers, 100 % for
    body proportions), then its fields from weighted tables keyed by species class and district. Hellborn citizens and
    the themed randomisers (5.4) swap in their own weight overlays.
- New golden lines: `fingerprints_v2.txt` has `citizen_fingerprint()` (FNV-1a over all 432 bytes of `DoomoCitizen`)
  for the same 96 seeds, generated once with `--update-golden`.

### 3.6 Determinism and fingerprint parity on native, GBA and PS1

| check | where | what |
|---|---|---|
| legacy generator unchanged | `test_resident.c:84-130` (exists) | the 96 v1 fingerprints, byte for byte |
| fat generator | new `fingerprints_v2.txt` | 96 seeds x 432 B |
| roster compatibility | new `roster_v1.txt` | 1000 legacy views |
| blob codec | new `test_resstore.c` | `decode(encode(x)) == sanitize(x)` for 20,000 random citizens, every base kind, both modes; encoder determinism (same bytes twice); hostile blobs (random bytes, truncated lengths) never crash and always sanitize |
| **on device** | GBA `hcmd.c` command `fpgold` and the PS1 harness command `fpgold` (new) | Runs the 96 v1 + 96 v2 fingerprints, 64 encode/decode round trips and 64 premade decodes on the real CPU. It prints one combined FNV, and the harness scripts compare it with the native value. This closes the gap noted in the current map: today no fingerprint is checked on device |
| cross-compile | `core/tools/xcheck.sh` (exists) | gcc, arm-none-eabi and mipsel at `-Werror` |
| portrait parity | GBA bench hashes (exist) | new "kitchen sink" set (every field non-zero) |

### 3.7 Settings persistence without saving the island

Global settings live in the header (3.1). When the options screen closes with changed global values:

- **GBA:** `save_flash.c` reads the slot's first 4 KB sector into `gba_big` (17 KB, claimed through `gba_big_claim()`),
  patches the header, erases the sector and programs it again. This takes about 60-100 ms behind the existing "Saving"
  banner and touches no other sector.
- **PS1:** `memcard.c` rewrites the frames that hold the header (5 x 128 B) of the current or newest save file.
- **No save on the medium yet:** the settings stay in RAM until the first save.
- Every island save also writes the current settings into its header, so the rewrite above is only needed when the
  settings change without an island save. Its power-cut exposure is the same as an ordinary save: one sector, the
  "Saving" banner, and detection by CRC.

---

## 4. Rendering plan

### 4.1 Principles

1. **New parts are data, not code.** Today `portrait_*.c` is 141,884 B of resident MIPS code on the PS1 for 349
   catalog parts plus sliders, species details and the framework, which is roughly 290 B of code per added part.
   Written as C `case`s, about 1,645 new drawable values would cost about 470 KB of PS1 RAM. Instead parts are
   **part scripts** (4.2), which average 56 B.
2. **Decals for detail.** Make-up, paint, tattoos, freckles, scars, seams, piercings and gems are drawn with
   `pc_decal()`, which keeps the part ids. They never touch the 253 part-id ceiling (`portrait_gfx.c:136-147`) and add
   no outline cost.
3. **Materials, not new colours.** `PCtx` grows from 13 to 53 `PMat` (+240 B). Each is resolved once in
   `pctx_init()` from a colour field, or derived exactly as today when the field is 0 (auto).
4. **Occlusion first.** Layers that cover others mark the covered region, and the covered part skips its spans. An
   outer coat skips most of the top, legwear skips leg skin, gloves skip hands, a hood skips back hair, and a mask
   skips the mouth and nose. Kitchen-sink citizens therefore do not cost the sum of their layers.
5. **Level of detail per view.** Every part-script op carries a minimum LOD (`LOD_TINY` chibi, `LOD_MID` fighter,
   `LOD_BIG` bust and figure). Micro-detail is skipped on chibis automatically.
6. **Animation from the registry.** `LF_ANIM` fields (eye glow, glow pulse, ahoge, tail wag style, aura colour cycling,
   palette-cycle finishes, ...) feed `look_anim_mask()`. That replaces the hand-written `phase_of()` condition
   (`portrait.c` phase list) and the creator's `anim_phase()` list (`creator_draw.c:312`). Nothing can freeze because
   someone forgot to add it.

### 4.2 Part scripts

`core/art/parts/*.txt` is compiled by `tools/partc.py`, a deterministic `# doomo-generator`, into
`core/gen/part_data.c`:

```
part pupil.star  space=eye lod=mid
  mat  eye_dark
  star 0 0 r=pupil*5/8                 ; uses the pupil_size slider
  mat  shine
  dot  -2 -3 if shine==auto
part glasses.heart space=face lod=mid mirror
  mat frame
  heart eyeL.x eyeL.y r=eyeR*11/8 ring=1
  line  eyeL.x+eyeR eyeL.y  bridge.x bridge.y
```

- **Ops** (8 B each): `E` ellipse, `C` capsule, `T` triangle, `Q` quad, `B` bezier ribbon, `L` line, `P` pixel, `STAR`,
  `HEART`, `TILE` (pattern fill of the current region), plus `MAT`, `DECAL`/`PART`, `MIRROR`, `IF field op value`,
  `IFYAW front/side/back`, `LOD`, `ANCHOR`, `SCALE slider`, `FX phase`.
- **Spaces and anchors.** Head space uses the existing `hE/hC/hT/hQ/hB/hL/hP` (`portrait_int.h`), which already
  project through `PHead` yaw. Anchors: eye L/R, brow, nose tip, mouth, cheek L/R, ear base L/R, horn base, head top,
  chin. Body space uses `pb_*` coordinates with the anchors neck, chest, belly, hip, hand L/R, foot L/R, tail root,
  back, and wrist L/R.
- **Size.** About 92 KB raw for everything in waves 1-4 (estimate by group: face micro-parts 12.5 KB, hair 9 KB, ears
  and horns 4.5 KB, body and fur 5 KB, limbs, tails and wings 8.7 KB, outfit 33 KB, species 9.6 KB, halo and aura
  1.1 KB, poses 5.9 KB). On the GBA they are read straight from ROM. The PS1 build packs them in 2 KB LZ blocks (the
  `packart.c` decoder) behind `plat_part_ptr()`. The weak default returns the ROM pointer, and the PS1 override keeps
  an 8 KB LRU of decoded blocks. A render touches 30-60 parts, and a block miss costs about 2k PS1 cycles.
- `partc.py` also estimates every part's pixel area at each LOD. It fails if one part exceeds 600 px at bust scale or
  if a whole list's worst case would break the 12,000-visit bound.

### 4.3 Per-feature plan

The layer groups are `draw_step()` 0..5 (`portrait.c:135-171`): 0 back (aura back, wings, tail, long hair, back ears),
1 body, 2 head + face, 3 neck, front hair and front ears, 4 hat, overlays, horns and face accessories, 5 `finish()`.

| group (fields) | painter / hook | layer | bust | figure | GBA fighter | chibi | PS1 3D | cost |
|---|---|---|---|---|---|---|---|---|
| head structure: head_width, cheek_fullness, chin, feature_height/scale, face_age | `phead_set` (rx/ry), `ph_head_base`, `ph_eye_geom` offsets | 2 | yes | yes | yes | width only | impostor | geometry only |
| eyes: pupil, iris, sizes, eye_color2, hetero + right colour, sclera, shine, glow, lashes, lid droop, outline, bags, asymmetry, extra-eye placement, gaze, special eye L/R | `one_eye()` split into stages sclera, iris, pupil, shine, lid, lashes; each stage takes a part script for its value | 2 | all | all | iris size, hetero colours, lid droop, glow, gaze, extra eyes, special eyes | sclera colour + glow | impostor | eyes are about 12x8 px on the bust; +4-9 % on the face |
| brows: colour, thickness, angle, spacing, length | `brows()` params | 2 | yes | yes | colour, angle | - | impostor | ~0 |
| nose and muzzle: colour, shine, height, muzzle style, length, puff, colour, whisker pads, whiskers | new `muzzle()` before `nose()`; whiskers in group 3 (outline-free decals) | 2-3 | yes | yes | muzzle, nose colour | nose colour dot | impostor | muzzle +1 part |
| mouth: width, height, curve, lips, teeth, fang placement, teeth colour, tongue, tongue colour | `mouth()` params + teeth and tongue scripts; curve offsets every `EXPR_*` | 2 | yes | yes | tongue blep, curve | - | impostor | decals |
| cheeks and skin: blush style, colour, amount, position, size; freckles; beauty mark; 2 scars; seams; bandage; cheek fluff; face gems | `markings()` becomes decal passes; cheek fluff on the head outline (fur tufts) | 2 | yes | yes | blush, scars, fluff | blush colour | impostor | decals |
| make-up, face paint, face tattoo | decal passes around the eye geometry and over the face | 2 | yes | yes | paint (big shapes) | - | impostor | decals |
| piercings and horn rings | decals anchored to ear, nose, brow, lip and horns; `pierce_metal` picks a ramp | 3-4 | yes | yes | ear only | - | impostor | decals |
| glasses: style, frame colour, lens tint, position | own slot in `ph_face_acc()` (frees the accessory slot) | 4 | yes | yes | yes | 2 px band | impostor | 1-2 parts |
| hair: +32 styles, bangs, ahoge, volume, length, mirror, dye pattern, colour 3, shine, accessories | `hair_back_shapes()` / `hair_front_shapes()` scripts; bangs are their own front layer; dye pattern is a gradient mask over hair parts; ahoge has a bounce phase | 0 / 3 | yes | yes | yes | silhouette + colour | impostor | hair is the biggest region; +32 styles at today's cost |
| ears: +16 styles, size, angle, fold, inner colour, tip detail | `one_ear()` params + scripts; `ph_top_extent()` reads size | 0 / 3 | yes | yes | yes | size, fold | impostor + `model.c` ear boxes scaled | ~0 |
| horns, antennae, head extra, halo colour, tilt and size, aura colour and amount | `one_horn()` count/curl/asymmetry; antennae + head extra scripts; `ph_halo()`; `pf_aura_*` colour | 0 / 4 / 5 | yes | yes | yes | halo, aura colour | impostor | horns x2 at most (count 4 = 2 small pairs) |
| body proportions: shoulders, torso, waist, hips, belly, arm length/thickness, leg length/thickness, neck, head ratio, posture, hand/foot size, muscle, roundness | `pb_layout()` (`bw`, `bh`, `legs`, arm anchors), `body_mod[]` | 1 | neck only | yes | yes | shoulders, belly, legs | `model.c` box/blob sizes | geometry only |
| fur and material: material (32), fluff, fur length, sheen, translucency, fur colour 2/3, fur pattern + scale + placement, belly, pads, claws, chest fluff, mane, grey muzzle, glow marks, tattoos, scars, plush construction | `pctx_init()` picks a shade mode per material (fur = tufts on the outline, scales/knit/felt/sequin = 8x8 `TILE` like `p_pattern_tiles`, slime = translucent dither, metal = sheen band, glass = rim light); fur pattern = per-region mask tiles; glow = palette-cycle FX op | 1-2 | yes | yes | yes | base + 1 pattern colour + glow | `model.c` colours; slime/ghost semi-transparent `POLY_GT`; fluff pyramids | tiles are span fills: no extra visits |
| hands, feet, legs: hand type, foot type, digitigrade/bird/serpentine legs | `legs()`, `foot()`, arm painters + scripts per type x view | 1 | - | yes | yes | feet, legs | `model.c`: digitigrade = 2-segment leg with reversed knee | 1-3 parts per limb |
| tail: count, length, fluff, tip colour/style, pose | `pb_draw_tail()` loops `tail_count` with fanned angles (9 = kitsune fan) | 0 | - | yes | yes | count, length | `model.c` tail boxes x count, tip box | count x tail cost; capped by pixel budget (9 tails thinner) |
| wings: size, colours, pattern, pose, pairs | `wing_one()` params | 0 | - | yes | yes | colour | `model.c` panels sized and coloured | x2 for seraph |
| outfit layers: inner top, outer, legwear, socks, gloves, belt, neck, back, mask (+position), hat tilt/size/ears, ear/tail/horn/wing accessories, wrist, prop | new `pb_outer()`, `pb_legwear()`, `pb_gloves()`, `pb_belt()`, `pb_back()`; `neck_acc()` gets a slot; `ph_hat()` with a rotated anchor for tilt; hat-and-ears modes in `ph_top_extent()` | 0 / 1 / 3 / 4 | upper | yes | yes | outer colour, hood, legwear colour, prop | `model.c`: coat panels, leg bands, glove colour, backpack/cape quad | occlusion keeps it bounded |
| per-piece modifiers: trim colours, patterns on outer/bottom/legwear/hat, emblem, sleeves, neckline, hood up + hood ears, lengths, tucked, open/closed, cuffs, wear, fabric, fit | `top_details()`, `bottoms()`, `collar()` params; patterns use the existing `TILE` path per piece; fit scales the clothing silhouette over the body | 1 | upper | yes | yes | hood | `model.c` sizes | ~0 |
| age look | head ratio, eye size and scale presets per age (baby/kid big heads, elder lids); never body shaping | all | yes | yes | yes | yes | `model.c` scale | ~0 |
| motion: idle, walk, run, stride, sit, sleep, dance, emote, greeting, laugh, cry, angry, love, victory, taunt, hurt, faint, lefty, blink, ear expressiveness, wag, breath, bounce, resting expression, talk, gaze, mood motion | pose tables (4 frames x 8 B per style) applied in `pb_layout()` through `arm_pose`, `step` and bob; chibi walk frames per walk style (`bodyspr_render` frame); `fighter_render` anims; `phase_of()` for blink/wag/breath | - | expressions | yes | yes | walk, idle | `model.c` swing amplitude/phase tables per walk/run style; lefty mirrors the gun arm | ~0 |
| voice: babble waveform, flavour, footsteps, quirks, singing range, vibrato, loudness | `audio.c` `voice_pitch()` / `voice_tick()` / `audio_voice_char()`; footsteps in `overworld.c` `player_update()` and `combat.c` `player_tick()` | - | - | - | - | - | - | audio |

### 4.4 Third person

- **GBA.** The third person is already a `fighter_image()` billboard at scale 300 (`combat.c:1650-1668`), which is
  fully procedural. Every field marked T above shows with no extra code. `TP08` adds dithering or hiding when the
  camera is close.
- **PS1** (`ps1/src/model.c:248-422`; it reads about 15 look fields today):
  1. **Head impostor.** A new image kind `CK_HEAD` (32x32, 3 cache blocks) is rendered by `headspr_render(look,
     yaw8)`, which is the bust's head and face painters at fighter LOD with yaw = the player's facing relative to the
     camera. `model_player_draw()` stops drawing the blob head, face boxes, hair blobs, ears, horns and halo. Instead
     it emits one `POLY_FT4` billboard at the head position, 2x scale, from the VRAM image cache (`vram.c`). **Every
     head, face, hair, hat, mask, glasses, horn, halo and ear option then shows in 3D, with no per-feature 3D code.**
     The current and the next yaw stay cached, about 5 ms per view on the R3000 (`PC_STEPS = 3`, so sliced), redrawn
     only when the yaw bucket changes. While it renders, the old blob head is the fallback.
  2. **Body fields in `model.c`.** Proportions scale boxes and blobs (`shoulders`, `belly`, `arm_len`, `leg_len`,
     `height`, `build`, which `model.c` ignores today). `legs` adds a digitigrade 2-segment leg. `hands`/`feet` choose
     paw blobs, hooves or talons. `tail_count`, `tail_len` and the tip colour add a fan of boxes. Wings use style,
     colour, size and pose. `outer` adds coat panels with gouraud, `legwear` and `gloves` add colour bands, `back` adds
     a backpack box or a cape quad, and the prop is shown out of combat. `material` picks slime or ghost
     semi-transparency and a metal highlight, and `fluff` adds outline pyramids. `walk`/`run` swap the swing
     tables, and `lefty` mirrors the gun arm.
  3. Other citizens on the PS1 are chibi billboards (`bodyspr_image`), so every C-marked field shows.

### 4.5 Chibi rules (16x24)

A whitelist renders at `LOD_TINY`: species silhouette, body/height/build, shoulders, belly, leg length, skin + one fur
pattern colour, hair silhouette + colour, ears size and fold, horns, halo, tail count and length, wing colour, outer
layer colour, hood, legwear colour, top/bottom/shoes colours, hat + tilt, glasses band, prop, aura colour, glow,
blush colour, walk style. Nothing else is drawn, so chibi cost stays at today's 250k average and at most 360k worst.

### 4.6 Creator preview

- Figure (48x80) on every body/outfit/motion section; the bust zoom on Face, Eyes, Hair and Makeup (exists;
  `creator_draw.c:328`); new: **lower-body zoom** on Paws & feet / Tail / Legwear sections (figure cropped and shown
  at 2x on 240x160, as the bust is).
- Turntable (`fighter_render`, 8 views) unchanged; new **test buttons** (T19) on the Motion tab play walk, run, idle,
  dance, emote, laugh, sit, sleep on the turntable.
- `preview_update()` throttle (1 render per 5 ticks while a direction is held) stays; the figure re-renders only when
  a field whose registry `views` include `LV_FIGURE` changed, so scrolling colour rows on the Face tab only
  costs bust slices.
- Possessed peek (T17) uses the new sclera and glow.

### 4.7 Cache keys

- `CacheEnt.look` (today a 48 B FNV) becomes `ver | amask << 8 | epoch << 16`. `ver` is `res_ver[rid]`, `amask` is the
  look's animation mask stored when the entry was rendered, and `epoch` is a render epoch bumped by option changes
  that alter every render (cheats CH00a-d, CH08-CH10, CH14, CH23, CH24, CH28; DS20; DS25; PX16; LS10). `animated()` first
  finds *any* entry for `(kind, rid, ver)`, takes its `amask` to compute the phase key and then looks up the exact
  phase. It decodes only on a real miss.
- `fp_common.c:629-690` memo and `look_hash()` (`fp_common.c:656`) key on `(rid, ver)`.
- `figure_image(const DoomoLook *, ...)` keeps hashing for callers without an id; the creator passes its draft
  through a new `figure_image_v(look, cr.draft_ver, ...)` to skip hashing 384 B per call; callers with an id
  (almanac, cutscenes, concert, exorcism, credits) move to `figure_image_id(rid, ...)`.

### 4.8 Performance budget (GBA UI >= 20 fps)

| item | today | budget | enforced by |
|---|---|---|---|
| bust render | 763k avg / 1.20M worst | <= 850k / 1.40M | GBA bench, 3 sets: random v1, random v2, kitchen sink |
| figure | 915k / 1.38M | <= 1.0M / 1.6M | same |
| chibi | 250k / 338k | <= 270k / 360k | same |
| fighter | 454k / 655k | <= 500k / 750k | same |
| one layer group (sliced re-render) | 0.1-0.2M | <= 0.23M | bench per group |
| decode on miss | - | <= 50k | bench |
| cache hit | a few k | unchanged (no decode, no hash) | bench |
| pixel visits per render | 4.1k avg, 8.7k worst, bound 12k | bound stays 12k | `test_portrait.c:420-437` over random v2 + kitchen sink |
| part ids per image | <= 253 hard ceiling | <= 240 | new test |
| UI fps | >= 20 | >= 20 | harness: creator scroll script, island with 40 chibis, 3D overworld |

At most one brand-new bust per 3 ticks keeps the GBA at or above 20 fps. Today's mechanisms already ensure that: the
free-play budget of one new render per frame for chibis, the creator throttle, and sliced re-renders for phases.

---

## 5. Creator UI plan

### 5.1 Information architecture: 10 tabs, 51 sections, about 385 rows

At most 10 tabs fit the strip (`creator_draw.c:27-29`). Today's Heart (persona) and Home merge into Profile, and Eyes,
Makeup and Motion become tabs.

| tab | sections (rows) |
|---|---|
| **Body** | Species & age (species, preset, age look, hybrid + blend: 5) - Proportions (body, height, build, shoulders, torso, waist, hips, belly, neck, head ratio: 10) - Limbs & posture (arm length/thickness, leg length/thickness, hand/foot size, muscle, roundness, posture: 9) - Fur & skin (skin, material, fluff, fur length, sheen, translucency, fur colour 2/3, belly colour, grey muzzle, "match fur": 11) - Pattern (marking, marking colour, fur pattern, scale, placement: 5) - Paws & feet (hands, feet, legs, pad colour/shape, claws + colour, polish + colour: 9) - Extras (chest fluff + colour, mane, plush construction, glow mark/colour/pulse, 2 tattoos + placements, ink, body scar, extra arms, body eyes, buddy head, belly mouth, mobility aid, prosthetic: 18) |
| **Face** | Head (head shape, size, width, cheek fullness, chin, feature height/scale, age detail: 8) - Nose & muzzle (13) - Mouth (13) - Cheeks (8 cheek flags, blush x5, freckles x3, beauty mark, cheek fluff, gem + colour: 20) - Scars & stitches (6) - Ears (6) |
| **Eyes** | Shape (style, count, extra-eye placement, size, spacing, height, tilt, asymmetry, lid droop, special eye L/R: 11) - Colour (eye colour, colour 2, heterochromia, right colour, sclera, glow: 6) - Pupil & iris (6) - Lashes & lids (5) - Brows (7) |
| **Hair** | Style (6) - Colour (6) - Accessories (3) - Facial hair (2) |
| **Makeup** | Makeup (eyeshadow x2, eyeliner x2, highlighter: 5) - Face paint (4) - Tattoo (2) - Piercings (8 flags + 5: 13) |
| **Spooky** | Horns (10) - Antennae & extras (3) - Tail (7) - Wings (7) - Halo & aura (7) |
| **Outfit** | Presets & fit (preset, fit, fabric, wear: 4) - Top (14) - Outer layer (5) - Bottoms & legs (11) - Shoes & gloves (6) - Hat, mask & glasses (13) - Accessories (16) - Wardrobe (5) |
| **Motion** | Moves (idle, fidget, walk, run, stride, mood motion, handedness: 7) - Rest (2) - Face habits (rest expression, laugh, cry, angry, love, talk, gaze, blink, blink rate: 9) - Body language (5) - Show-off (dance, emote, greeting, victory, taunt, hurt, faint: 7) - Presets + test buttons (2) |
| **Voice** | Voice (preset, 5 axes, test: 7) - Sounds (babble, flavour, loudness, footsteps, quirks, singing range, vibrato: 7) |
| **Profile** | Name (name, nickname, pronouns, crushes: 4) - Heart (5 persona sliders + panel: 5) - Likes (birthday x2, food, hated food, hobby, music, colour, fear, catchphrase, job: 10) - Home (district, born in, favourite: 3) |

**Creator modes** (T01, option CR01): **Simple** shows today's 105 rows. **Advanced** (the default) adds the 52 wave-2
"core" fields, for about 160 rows. **Everything** shows all of them. The registry flags `LF_SIMPLE` / `LF_ADVANCED`
decide, so the modes cost no extra tables. Changing mode never changes a value: hidden fields keep theirs, and a chip
says "+12 hidden details".

### 5.2 Screen at 240x160 (GBA)

```
 +--------------------------------------------------------------------------------+
 | [Bod][Fac][EYE][Hai][Mkp][Spk][Out][Mot][Voi][Pro]                  21 px cells |  tab strip (exists)
 +-------------------------+------------------------------------------------------+
 |                         |  < Shape . Colour . PUPIL & IRIS . Lashes . Brows >  |  section chips (new)
 |      bust 2x zoom       |  > Pupil          [star  *]                  <  >    |
 |     (or figure /        |    Pupil size     [====|---]                         |
 |      lower-body zoom)   |    Iris           [galaxy]          pinned: *       |  8 rows visible
 |                         |    Iris size      [==|-----]                         |
 |      86 x 122           |    Shine          [heart]                            |
 |                         |    Gaze           [forward]                          |
 |                         |    ...                                               |
 +-------------------------+------------------------------------------------------+
 | "Star pupils: sparkly!"  Wardrobe 61% (+9 B)        START menu  SEL turntable   |  detail line + meter (new)
 +--------------------------------------------------------------------------------+
```

At 320x240 (PS1), 12 rows are visible, the preview is 120 px wide at 2x, and the chip row shows every section name in
full.

### 5.3 Controls

| button | main list | chip row | pickers | turntable |
|---|---|---|---|---|
| UP / DOWN | row; UP on the first row goes to the chip row | DOWN back into the rows | choice | walk / spin (exists) |
| LEFT / RIGHT | change value | previous / next section | page / move | turn |
| A | picker / toggle / type; **hold A** (AC27 long-press) = pin or unpin the row | - | choose | tumble |
| B | back (Discard?) | back | cancel | back |
| L / R | tab (exists) | tab | page | - |
| **SELECT + LEFT/RIGHT** (GBA) or **L2 / R2** (PS1) | previous / next section from anywhere | | | |
| **L + R held** | before/after: shows `orig` (T11); split view on PS1 | | | |
| X / Y (GBA SELECT+A / SELECT+B) | undo / redo (exists) | | | |
| START | menu (below) | | | |
| SELECT tap | turntable (exists) | | | |

Tab switches act when L or R is *released* alone, so holding L + R together never changes the tab.

**START menu:** Move in! / All done!, **Find...**, Randomise all..., **Randomise section**, **Randomise colours only**,
**Randomise shapes only**, **Theme randomiser** (T04), Undo (n), Redo (n), **Copy section**, **Paste section**,
**Copy colour**, **Paste colour**, Copy a look..., **Look Book...**, **Look Password...**, **Mode: Simple / Advanced
/ Everything**, **Hellvile-ify / Hillvile-ify** (T22), Possessed peek, Start over, Quit.

The worst case to reach any row is 5 tab presses + 4 section presses + 10 row presses = 19 presses. With **Find**
(T12) it is START, a letter jump with L/R by first letter, then A, which reaches any of the 385 rows in at most about
8 presses. Find lists every row and section alphabetically, plus a "Recently changed" group (the last 8 rows edited)
and "Favourites" (T13, starred rows, kept in the global settings: 48 B bitmap).

### 5.4 Randomise, pins, themes

- **Section / colours only / shapes only** (T21): the registry's `LF_COLOUR`/`LF_SHAPE` flags select fields; values
  come from a fresh `gen_fat()` / `gen_look()` roll for that section's substream (so a section re-roll matches what
  the generator would produce). `fix_locks()` (exists) still replaces locked values.
- **Pins** (T03): a 384-bit pin mask in `CrState`; pinned fields survive every randomise.
- **Themes** (T04, option CR06): Cute, Spooky, Goth, Pastel, Punk, Royal, Furry, Demon, Angelic, Chaos, "Like
  Hellvile", "My favourite colour" = weight overlays on the generator tables (about 0.5 KB each, creator overlay on
  PS1).
- **Hellvile-ify / Hillvile-ify** (T22) = the hellborn / cute weight overlay applied to the outfit, colour and face
  sections only (species and name kept).

### 5.5 Copy, paste, symmetry, presets, Look Book, Look Password

- **Copy/paste** (T06): the clipboard is a `DoomoLook` + a 384-bit field mask in `CrState` (432 B). It holds one colour,
  one section or the whole look, and survives tab and citizen switches inside one creator session. "Copy a look..."
  (exists) also takes any citizen's whole look through `res_get()`.
- **Symmetry** (T07): fields with `LF_PAIR` (special eye L/R, tattoo slots, socks "mismatched", ear fold L/R values,
  horn asymmetry, eye asymmetry) show a link toggle. When linked, editing one side sets the twin.
- **Presets.** The format is `(uint16 field, uint8 value)` triples ending at 0xFFFF, up to 32 per preset. That covers
  72 species presets extended with species-smart new fields (T16: catfolk suggest slit pupils, whiskers and toe beans,
  without forcing them), 60 outfit sets (O62), 12 voice types, 8 motion personalities (A38: Shy bean, Hyper pup,
  Royal, Goth gremlin, Sleepy blob, Tough cookie, Ghosty, Robot) and 10 look themes. That is about 170 presets x 60 B
  = 10 KB, held in ROM on the GBA and in the creator overlay on the PS1. `cr_species_base` (`creator_data.c:201-210`)
  gains the species-defining new fields (muzzle, whiskers, hands, feet, legs, material, cheek fluff, inner ear colour).
- **Look Book** (T08): 16 named slots stored *in the heap* as ids 1001..1016, so they are counted by the 2.5 bound and
  saved for free. Each slot holds a whole look or only the outfit sections.
- **Look Password** (T09): the blob re-encoded with a portable base (`BK_PRESET`/`BK_SEED`/`BK_ROSTER`, never `BK_SLOT`).
  It is written as base-32 with a 4-character CRC-16 and is 48-144 characters typically (312 at most). Because the
  codec is the shared core, a code from a GBA cart works on a PS1 disc and vice versa.

### 5.6 Undo (T20)

Snapshots are delta-encoded with the blob encoder against `orig` into a 2,048 B byte ring (32 B to 90 B per step), so
the timeline holds 22-64 steps, up from 19 today. Undo keys use the field index instead of `cr.tab*64+row`
(`creator.c:1382`), which removes the 64-rows-per-tab limit.

### 5.7 Wardrobe meter and a full heap

The footer always shows `Wardrobe NN% (+x B)`, which is `res_heap_used()` plus the draft's `res_cost()`. Above 90 %
it turns pink. If a commit returns `RES_E_FULL`, a dialog offers:

1. **Simplify.** It drops the lowest-priority non-auto new fields, in a registry order that starts with wave-3 detail,
   until the citizen fits, and shows the preview first.
2. **Start from a preset or seed.** It re-bases the draft so the delta shrinks, which is often enough.
3. **Cancel.**

Births never fail for lack of heap: a baby falls back to a 6 B `BK_SEED` blob without inherited fields. The Look Book
shows its own share of the heap.

### 5.8 Unlocks

`unlocks2` (512 bits, wishlist E5) and 16 new `PROG_*` counters (makeovers, photos, rainy days, heals, sprint distance,
introductions, Slime Belcher kills, Gargoyle kills, exorcism streak, candy collected, festivals attended, critter-folk
created, batlings created, hellhound friends, confessions helped, Wheel of Fate wins) make room for all 78 unlock ideas U01-U78 of the wishlist
without touching the full 1024-bit bank or the 32 full `PROG` slots. Grandfathering (`cr.orig`) and silhouettes work
unchanged.

### 5.9 `CrState` budget (scene scratch, 12,288 B)

| part | bytes |
|---|---:|
| today's state minus its 22 `DoomoResident` copies and `shown` | 4,400 |
| draft + orig `DoomoCitizen` | 864 |
| shown `DoomoLook` | 384 |
| clipboard (look + mask) | 432 |
| undo ring + index | 2,112 |
| pin mask | 48 |
| sections (row/top x 51), mode, Find state, recent, meter cache | 260 |
| **total** | **8,500** (3,788 B spare; `creator.c:22` static assert) |

---

## 6. Options expansion

### 6.1 Storage

- **One id space, two homes.** `OPT_*` stays the API (`opt_get()`/`opt_set()`, `options.c:120-134`). Existing ids keep
  their numbers. Each option has resident metadata `{count, def, scope, slot}` (4 B, about 0.9 KB for all of them).
  - **Global** (`G`: audio, display, controls, accessibility, text, creator preferences) is stored in
    `doomo_settings.v[384]`, saved in the slot header (3.1, 3.7), so changing the volume no longer depends on which
    island is loaded.
  - **Island rules** (`I`: sim, romance, gore, possession, cheats, modifiers) are stored in `doomo_town.opt[256]`,
    saved with the island.
- **Capacity.** 373 options are listed here, 203 G and 170 I. Counting the 57 existing ids on top, at most 236 G
  slots (384 available) and 194 I slots (256 available) are used. The `OPT_COUNT <= 64` assert becomes `<= 640`.
- **Strings off the resident path.** Names, value names and help texts (`opt_help[]`, about 15 KB) live in
  `options_text.c`. Only `fp_options.c` reaches it, so it sits in the free-play overlay group on the PS1 and in ROM on
  the GBA.
- `DoomoSettings`: `{ u32 seq; u8 v[384]; u8 remap[3][14]; u8 fav[48]; u8 pad[32]; u16 crc; }` is exactly 512 B.

### 6.2 Options UI (`fp_options.c`)

- The screen has 23 pages in 6 categories: Game & story; Life (Island, Life sim, Needs, Romance, Possession, Pet);
  Shooter (Gore, Movement, Camera, Aim, Crosshair & HUD, Weapons); Sound & screen (Audio, Display, Colour &
  accessibility, Text); Controls; Extras (Cheats & modifiers, Modes & minigames, Creator & photo, System).
- Navigation: the category list sits on the left on the PS1, or in a popup on the GBA. L/R change the page and
  UP/DOWN the row. At most 24 rows fit per page (`ROWS_MAX`). The largest page, Cheats & modifiers, has 49 entries and
  is split into Cheats and Modifiers.
- **Find option**: START opens a letter jump over all option names, the same widget as the creator's Find.
- **Presets**: "Classic DOOM", "Cozy island", "Max accessibility" and "Story first" set groups of options at once.
  "Reset page" (exists) and "Reset all" are also there.
- **Help line** for every option, required by a test. Locked values show "Next: ??? (how)", as `fp_options.c:81-86`
  already does.

### 6.3 The palette filter (wires 12 options with one mechanism)

Everything on both platforms is drawn through the one 256-entry master palette: GBA Mode 4 indices, PS1 CLUTs, and
PS1 `TILE` colours through `pal_rgb()`. A new `palette_display(uint16_t out[256])` in the core applies, in integer
maths:

1. brightness, contrast and gamma (DS03, DS04, DS06): a 32-entry LUT per channel;
2. saturation (DS05);
3. a 3x3 Q8 colour matrix for colour-blind aids (CB01 protan, deutan, tritan, achromat, high contrast, with strength
   from CB03) and for camera filters (DS07: sepia, nightvision, storybook, game pal 4-shade, VHS, plus 8 new);
4. LCD mode (DS01 "Everything"), the rainbow cycle (CH19), 1-bit mode (CH20), the retro '93 palette (CH27) and
   invasion rot (GR16).

`plat_palette_changed()` is a new platform hook:

- **GBA:** `video.c` writes `out` straight into palette RAM at `0x05000000`, plus the DS02 screen-profile gamma. No
  EWRAM is used.
- **PS1:** `gfx.c` `palette_tables()` rebuilds the master, silhouette and font CLUTs from `out`, computed on the stack.
- **Native:** copies the result.

The colour maps used for shading are index tables (`doomo_colormap`), so filtering never disturbs shading. This makes
three of the eight dead options real: colour-blind aid, camera filter and LCD filter. The fourth dead non-cheat
option, subtitles, is wired in `ui_dialog.c`.

### 6.4 Controls remapping

Remapping is a **permutation per input context**. `input_update()` (`core/src/input.c`) maps the 14 physical bits to
logical bits through `doomo_settings.remap[ctx]` before edge detection. On the GBA this happens *after* the SELECT+A/B
X/Y synthesis.

- Scenes set the context with `input_set_context(IC_MENU | IC_WORLD | IC_SHOOTER)`.
- A permutation can never leave an action unreachable or bind two buttons to it, so no conflict detection is needed
  (RM05 is cut).
- The remap screen shows the 14 logical buttons. Picking one and pressing a physical button swaps the two entries.
- Storage is 42 B. The default is the identity, and RM01-RM03 each reset their context.
- AC12 (confirm on B/Circle) is a preset permutation of the menu context. CT01 schemes (Southpaw, One-hand, Legacy)
  are preset permutations of the shooter context, plus `read_controls()` flags.

### 6.5 The list: 373 options

`*` marks the default. Scope `G` means global and `I` means island. Status `NEW` means a new id, `EXT(X)` means it
grows or splits existing option X, and "today unwired" means the option exists but nothing consumes it yet. "Wires
into" names the consumer at file/function level (`core/src` unless prefixed `gba`/`ps1`). "Test probe" is how
`test_options_effect` proves the option does something (6.7).

#### Game (15)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| GM01 | Combat difficulty | Plushie, Normal*, Nightmare, Ultra-Hellvile, Custom | I | EXT(OPT_DIFFICULTY) | actor.c difficulty_skip/actor_damage, combat.c combat_hurt_player | CMB |
| GM02 | Life-sim difficulty | Plushie, Normal*, Nightmare, Ultra-Hellvile | I | NEW (split of OPT_DIFFICULTY) | sim_soul.c rate_factor, sim.c apply_needs | SIM |
| GM03 | Pet difficulty | Plushie, Normal*, Nightmare, Ultra-Hellvile | I | NEW | pet.c pet_tick_minute, needs_call | PET |
| GM04 | Puzzle difficulty (exorcism, welding, minigames) | Plushie, Normal*, Nightmare, Ultra-Hellvile | I | NEW | exorcism_scene.c judge, weld_game.c step_score, mg.c start_round | MG |
| GM05 | Autosave | Off, Every day*, Every hour, On location change, On scene exit | G | EXT(OPT_AUTOSAVE) | fp_common.c fp_live, overworld.c ow_sync_to_save | SAVE |
| GM06 | Autosave icon | Off, On* | G | NEW | fp_common.c fp_hud | UI |
| GM07 | Confirm prompts | All*, Important only, None | G | NEW | ui_modal.c ui_confirm (auto-yes for non-destructive) | UI |
| GM08 | Pause sim in menus | Off, On* | I | NEW | sim.c sim_tick (skip while a modal/menu scene is on top) | SIM |
| GM10 | Tutorial hints | On*, Once, Off | G | NEW | ui.c ui_hint | UI |
| GM11 | Unlock toasts | Full*, Compact, Silent (log only) | G | NEW | ui.c ui_toast (unlock path) | UI |
| GM12 | Spoiler-free unlock hints | Off*, On | G | NEW | unlocks.c hint text, creator cr_lock_hint | UI |
| GM13 | Menu theme | By district*, Always pastel, Always goth | G | NEW | ui_draw.c ui_style / ui_backdrop | UI |
| GM14 | Boot into | Title*, Continue last save, Pet screen | G | NEW | fp_title.c (boot path), game.c doomo_init | UI |
| GM16 | Hellvile spice (rude words) | Clean*, Grawlix @#$%, Spicy ("heck") | G | NEW | ui_dialog.c copy_str filter, sim_events.c sim_news_push | TXT |
| GM17 | Monster Mode | Off*, ON (monster!), Ask each time | I | EXT(OPT_MONSTER_MODE) | sim_soul.c possession kill path, levels.c | SIM |

#### Story (8)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| ST01 | Story pacing | Relaxed, Normal*, Intense | I | NEW | story_script.c script_ambient_talked / nest timers | STORY |
| ST02 | Invasion encounters | Story beats only, Low, Normal*, High | I | NEW | story_script.c script_nest_encounter | STORY |
| ST03 | Quest markers | Off, Subtle*, Full arrows | G | NEW | overworld.c story_draw + story.c story_target | UI |
| ST04 | Seen cutscenes | Ask*, Always skip, Never skip | G | NEW | cutscene.c cs_update (skip input), story.c story_push_cutscene | STORY |
| ST05 | Hold to skip dialogue | Off, On* | G | NEW | ui_dialog.c dlg_update | TXT |
| ST06 | Protagonist is called | Name*, Nickname, "you" | I | NEW | story.c story_player_name | TXT |
| ST07 | Auto-equip best weapon | Off*, On | I | NEW | story_script.c ensure_weapon, combat.c combat_select_weapon | CMB |
| ST11 | Story NPC protection | Protected*, Look editable, Fully editable | I | EXT(RF_STORY_NPC) | creator.c (scene arg checks), fp_resident.c | UI |

#### Island rules (13)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| IS01 | The Wall | Up*, Gate open, Down | I | NEW | fp_island.c peek_wall, sim_social.c cross_border_pct | SIM |
| IS02 | Moving across the border | Story-gated*, Allowed, Never | I | NEW | creator.c RK_DISTRICT row, town_alloc | UI |
| IS03 | Starting money (new island) | 0, 500*, 5000, 50000 | I | EXT(DOOMO_START_MONEY) | town.c town_new | SIM |
| IS04 | Income rate | x0.5, x1*, x2, x5 | I | NEW | sim_events.c reward, fp_island.c collect_rent | SIM |
| IS05 | Shop prices | x0.5, x1*, x2 | I | NEW | fp_shop.c (price calc) | UI |
| IS06 | Rent day | Off, On* | I | EXT(FPLY rent day) | fp_island.c collect_rent | SIM |
| IS07 | Travelers visiting | Off, Rare, Normal*, Often | I | EXT(RF_TRAVELER) | sim.c day_rollover (traveler spawn) | SIM |
| IS09 | Seasons | Off, Four seasons*, Always spring, Always spooky | I | NEW | fp_island_gfx.c fp_island_tint/draw_deco, sim.c day_rollover | UI |
| IS11 | Day/night visuals | Cycle*, Always day, Always night | I | NEW | fp_island_gfx.c fp_island_tint, golden_hour | UI |
| IS12 | Buildings | Unlock with progress*, All open | I | NEW | fp_island.c add_activities / fp_common.c fp_activity_visible | UI |
| IS13 | News ticker content | All*, Favourites only, Big events only, Off | I | EXT(OPT_NEWS_TICKER) | fp_common.c fp_ticker, sim_events.c sim_news_push | UI |
| IS14 | Problem bubbles | All*, Important, Favourites only, Off | I | NEW | fp_island.c draw_bubbles | UI |
| IS15 | Weather | Off, Normal*, Lots, Hellvile ash, Candy rain | I | EXT(OPT_WEATHER) | fp_island_gfx.c fp_island_weather/draw_weather | UI |

#### Life sim (16)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| LS01 | Sim speed | Pause, Chill, Normal*, Fast, Turbo, x8 | I | EXT(OPT_SIM_SPEED) | sim.c speed_q | SIM |
| LS02 | Day length | Real time, 2 h, 1 h*, 45 min, 30 min, 15 min, 10 min | I | EXT(OPT_DAY_LENGTH) | sim.c ticks_per_minute_q | SIM |
| LS04 | Autonomy | Low, Normal*, High | I | NEW | sim.c decide_activity (need-driven weight) | SIM |
| LS05 | Problem frequency | Rare, Normal*, Often | I | NEW | sim_events.c sim_check_problems | SIM |
| LS06 | Problems expire after | Never*, 1 day, 3 days | I | NEW | sim_events.c sim_problems_minute | SIM |
| LS07 | Mood swings | Calm, Normal*, Dramatic | I | NEW | sim.c sim_mood_of / tick_transient | SIM |
| LS08 | Personality drift | Off*, On | I | NEW | sim.c day_rollover (persona +-1 toward friends, hot record) | SIM |
| LS10 | Hygiene visuals (stink lines, stains) | Off, On* | I | NEW | fp_common.c fp_chibi overlay, portrait.c pf_status | POR |
| LS12 | Old-age death | Off*, On | I | EXT(DEATH_OLD_AGE) | sim_soul.c sim_soul_daily | SIM |
| LS13 | Neglect death | Off*, On | I | EXT(DEATH_NEGLECT) | sim.c update_resident | SIM |
| LS14 | Jobs and wages | Off, On* | I | NEW | sim.c job_venue / decide_activity, sim_events.c reward | SIM |
| LS15 | Gift pickiness | Easy, Normal*, Picky | I | NEW | sim_care.c gift_taste | SIM |
| LS17 | Sleep schedule | By trait*, Everyone sleeps, Party island | I | NEW | sim.c decide_activity (sleep_at/wake_at) | SIM |
| LS18 | Dreams | Off, Rare, Normal*, Often | I | NEW | dream.c dream_plan | SIM |
| LS19 | Thought bubbles | Icon*, Text, Both | G | NEW | fp_island.c draw_bubbles | UI |
| LS20 | Catchphrase frequency | Rare, Normal*, Often | I | NEW | sim_events.c sim_news_fluff, overworld_npc chatter | SIM |

#### Needs (11)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| ND01 | Hunger decay | Off, Slow, Normal*, Fast, Brutal | I | NEW (STAT_HUNGER) | sim.c apply_needs (per-stat rate x 0/0.5/1/1.5/2.5) | SIM |
| ND02 | Happiness decay | Off, Slow, Normal*, Fast, Brutal | I | NEW (STAT_HAPPY) | sim.c apply_needs (per-stat rate x 0/0.5/1/1.5/2.5) | SIM |
| ND03 | Hygiene decay | Off, Slow, Normal*, Fast, Brutal | I | NEW (STAT_HYGIENE) | sim.c apply_needs (per-stat rate x 0/0.5/1/1.5/2.5) | SIM |
| ND04 | Energy decay | Off, Slow, Normal*, Fast, Brutal | I | NEW (STAT_ENERGY) | sim.c apply_needs (per-stat rate x 0/0.5/1/1.5/2.5) | SIM |
| ND05 | Health decay | Off, Slow, Normal*, Fast, Brutal | I | NEW (STAT_HEALTH) | sim.c apply_needs (per-stat rate x 0/0.5/1/1.5/2.5) | SIM |
| ND06 | Sanity decay | Off, Slow, Normal*, Fast, Brutal | I | NEW (STAT_SANITY) | sim.c apply_needs (per-stat rate x 0/0.5/1/1.5/2.5) | SIM |
| ND07 | Corruption gain decay | Off, Slow, Normal*, Fast, Brutal | I | NEW (STAT_CORRUPTION) | sim_soul.c sim_corrupt_natural | SIM |
| ND08 | Loneliness decay | Off, Slow, Normal*, Fast, Brutal | I | NEW (STAT_SOCIAL) | sim.c apply_needs (per-stat rate x 0/0.5/1/1.5/2.5) | SIM |
| ND09 | Need floor | 0*, 10 (forgiving) | I | NEW | sim.c sim_stat_add | SIM |
| ND10 | Need alert threshold | 10, 25*, 40 | G | NEW | sim_events.c sim_check_problems | SIM |
| ND12 | Need display | Bars*, Hearts, Numbers, Faces | G | NEW | fp_common.c fp_meter | UI |

#### Romance & family (19)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| RO01 | Romance | On*, Crushes only, Off | I | NEW | sim.c sim_romance_ok, sim_social.c romance_step | SIM |
| RO02 | Crush rate | Rare, Normal*, Often | I | NEW | sim_social.c romance_step | SIM |
| RO03 | Confession luck | Harsh, Normal*, Kind | I | NEW | sim_social.c sim_confess | SIM |
| RO04 | Weight of your advice | Low, Normal*, High | I | NEW | sim_social.c sim_advise_confess/propose | SIM |
| RO05 | Cross-border romance | On*, After the Wall falls, Off | I | NEW | sim_social.c cross_border_pct | SIM |
| RO06 | Dating to proposal speed | Slow, Normal*, Fast | I | NEW | sim_social.c romance_step | SIM |
| RO07 | Marriage | Off, On* | I | NEW | sim_social.c sim_propose | SIM |
| RO08 | Breakups | Off, Rare, Normal* | I | NEW | sim_social.c sim_breakup / romance_step | SIM |
| RO10 | Ex drama | Off, On* | I | EXT(REL_EX) | sim_social.c visit_edge | SIM |
| RO12 | Rivals and nemeses | Off, Rare, Normal*, Spicy | I | EXT(REL_RIVAL) | sim_social.c visit_edge | SIM |
| RO13 | Arguments | Rare, Normal*, Often | I | NEW | sim_social.c visit_edge (argue roll) | SIM |
| RO14 | Friendships fade | Off, Slow, Normal* | I | NEW | sim_social.c sim_edges_step | SIM |
| RO15 | Babies | Off, Rare, Normal*, Often | I | EXT(sim_make_baby) | sim_social.c sim_make_baby | SIM |
| RO17 | Baby kind | By parents*, Always angel, Always hellspawn, Random | I | NEW | sim_social.c sim_make_baby (resident_generate vs resident_generate_hellborn) | SIM |
| RO18 | Baby look heredity | Blend*, Mostly parent A, Mostly parent B, Random | I | NEW | sim_social.c sim_make_baby (inherit table, fat fields included) | POR |
| RO19 | Baby mutation chance | None, Low*, Medium, Wild | I | NEW | sim_social.c sim_make_baby (fat mutation draws) | POR |
| RO21 | Baby naming | Auto, Ask me* | I | NEW | sim_social.c sim_make_baby -> ui_text_entry on next island visit | UI |
| RO23 | Premade relationships on a new island | Keep*, Clear, Randomise | I | NEW | town.c town_new (edge copy) | SIM |
| RO24 | Relationship notifications | All*, Close friends, Off | G | NEW | sim_events.c sim_news_push filter | UI |

#### Possession & death (14)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| PX01 | Possessions | Off, Rare, Normal*, Frequent, Story only | I | EXT(OPT_POSSESSION_RATE) | sim_soul.c possession_allowed/sim_possess | SIM |
| PX02 | Max possessed at once | 1, 3*, 5, 10, Unlimited | I | NEW | sim_soul.c sim_possession_cap | SIM |
| PX03 | Corruption spreads | Off, On*, Viral | I | NEW | sim_soul.c sim_corrupt (friend spread) | SIM |
| PX05 | Nightmares | Off, Rare, Normal* | I | NEW | dream.c dream_plan (creep_of) | SIM |
| PX07 | Exorcism difficulty | Easy, Normal*, Hard, Holy Terror | I | NEW | exorcism.c exorcism_odds, exorcism_scene.c judge | MG |
| PX08 | Exorcism phases | All 3*, Skip rhythm, Skip aim, Skip true name, True name only | I | NEW | exorcism_scene.c start_ritual | MG |
| PX09 | Failures before a soul is lost | 3*, 5, 9, Never | I | EXT(DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL) | exorcism.c exorcism_fails/exorcism_tries_left | SIM |
| PX10 | Lay to Rest | Allowed*, Disabled | I | NEW | sim_soul.c sim_can_lay_to_rest | SIM |
| PX12 | Ghost rite | Off, On* | I | NEW | fp_common.c fp_ghost_rite_blocker | UI |
| PX13 | Ghost temperament | Friendly*, Mischievous, Haunty | I | NEW | sim_soul.c sim_raise_ghost, sim_events.c (haunt events) | SIM |
| PX14 | Grief | Short, Normal*, Long | I | NEW | sim_soul.c grieve | SIM |
| PX15 | Nobody dies (except Monster Mode) | Off*, On | I | NEW | sim.c update_resident, sim_soul.c sim_soul_daily | SIM |
| PX16 | Possessed portrait intensity | Cute-creepy, Creepy*, Full horror | G | NEW | portrait.c pctx_init (creepy level), portrait_fx.c pf_status | POR |
| PX17 | Possessor names | Premade only, Neighbours first*, Anyone in Hellvile | I | EXT(sim_possessor_of) | sim_soul.c sim_possessor_of | SIM |

#### Pet (10)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| PT02 | Pet hunger speed | Slow, Normal*, Fast | I | NEW | pet.c pet_tick_minute | PET |
| PT03 | Poop frequency | Low, Normal*, High | I | NEW | pet.c pet_tick_minute | PET |
| PT04 | Pet sickness | Off, On* | I | NEW | pet.c pet_tick_minute | PET |
| PT05 | Pet death | Off* (runs away, comes back), On | I | NEW | pet.c pass_away | PET |
| PT06 | Evolution line | Free*, Cute, Angel, Feral, Demon | I | NEW | pet.c evolve | PET |
| PT07 | Discipline calls | Off, On* | I | NEW | pet.c needs_call | PET |
| PT08 | LCD palette | Classic green*, Pastel, Hellvile red, Greyscale, Backlit blue | G | NEW | pet_scene.c colours | UI |
| PT09 | LCD pixel size | Chunky*, Fine | G | NEW | pet_scene.c lcd_rect/lcd_sprite | UI |
| PT10 | Pet alerts | Off, Badge only, Toast, Toast + beep* | G | EXT(OPT_PET_NOTIFICATIONS) | pet_scene.c / fp_common.c fp_hud | UI |
| PT12 | Pet combat buff | Off, On* | I | NEW | pet.c pet_combat_buff | CMB |

#### Gore & horror (14)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| GR01 | Gore mode | Cute*, Brutal, Per weapon, Per mode, Cute citizens + brutal demons | I | EXT(OPT_GORE) | actor.c combat_gore_style | CMB |
| GR02 | Blood colour | Red*, Green, Rainbow, Confetti, Purple, Black ink, Glitter, Ectoplasm, Candy syrup | I | EXT(OPT_BLOOD_COLOR) | actor.c actor_gore, combat.c | CMB |
| GR03 | Confetti | None, Some, Lots*, Absurd | I | EXT(OPT_CONFETTI_AMOUNT) | actor.c actor_gib, mg_fx.c | CMB |
| GR04 | Gibs | None, Few, Normal*, Lots | I | NEW | actor.c actor_gib | CMB |
| GR05 | Gib lifetime | 1 s, 10 s*, Until level end | G | NEW | actor.c gib_tick | CMB |
| GR07 | Demon corpses | Fade*, Stay | G | NEW | actor.c effect_tick | CMB |
| GR08 | Demon death style | Per gore*, Balloon pop, Plush deflate, Dust poof, Explode | I | NEW | actor.c actor_gore | CMB |
| GR09 | Bullet impacts | Auto*, Sparks, Stars, Hearts, Puffs | I | NEW | actor.c actor_fx | CMB |
| GR11 | Death sounds | Auto*, Squeak, Splat | I | NEW | actor.c actor_event -> audio_sfx | AUD |
| GR12 | Damage screen effect | Off, Cute heart cracks*, Brutal blood | G | NEW | combat_hud.c combat_draw_hud | HUD |
| GR13 | Kill hit-stop | Off, Short*, Long | G | NEW | combat.c combat_on_kill (freeze ticks) | CMB |
| GR15 | Jump scares | Off, On* | I | NEW | actor.c demon_wake (stinger), dream.c start_vignette | CMB |
| GR16 | Invasion palette rot | Off, Partial, Full* | I | NEW | overworld.c restore_style / palette filter | PAL |
| GR17 | Demon designs | By chapter*, Always cute, Always grotesque | I | EXT(OPT_CHEAT_CUTE_DEMONS) | actor.c actor_demon_frame | CMB |

#### Shooter: movement (10)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| MV01 | Auto-sprint | Off*, On, After 1 s | G | NEW | combat.c read_controls | INP |
| MV02 | Sprint | Hold*, Toggle | G | NEW | combat.c read_controls | INP |
| MV03 | Crouch | Hold, Toggle* | G | NEW | combat.c read_controls | INP |
| MV04 | Dodge roll input | Double-tap + button*, Button only, Double-tap only | G | NEW | combat.c read_controls | INP |
| MV06 | Movement feel | Snappy, Normal*, Floaty | G | NEW | combat.c player_tick (accel/friction table) | CMB |
| MV08 | Auto-run (classic scheme) | Off*, On | G | NEW | combat.c read_controls | INP |
| MV09 | Step-up assist | Off, On* | G | NEW | combat.c try_move | CMB |
| MV10 | Use / interact | Button*, Auto on bump | G | NEW | combat.c bump_doors | CMB |
| MV11 | Auto-pickup weapons | Off, On* | G | NEW | combat.c pickups_tick | CMB |
| MV12 | Knockback on you | Off, Low, Normal* | I | NEW | combat.c combat_hurt_player | CMB |

#### Shooter: camera (18)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| FP01 | Field of view | 60, 70, 75*, 80, 90, 100, 110 (clamped per platform) | G | EXT(OPT_FOV) | combat_hud.c focal_px, gba r3d.c, ps1 view3d.c | PLAT |
| FP02 | Weapon bob | Off, Low, Full* | G | NEW | combat_hud.c weapon_view | HUD |
| FP03 | Weapon position | Right*, Centre, Left, Follow handedness | G | NEW | combat_hud.c weapon_view (reads look.lefty for the player) | HUD |
| FP04 | Weapon size | Small, Normal*, Big | G | NEW | combat_hud.c weapon_view | HUD |
| FP05 | Show weapon | Off, On* | G | NEW | combat_hud.c weapon_view | HUD |
| FP06 | View kick | Off, Low, Full* | G | NEW | combat.c fire_weapon (recoil to view) | CMB |
| FP07 | Muzzle-flash light | Off, On* | G | NEW | combat_hud.c muzzle_flash | HUD |
| FP09 | Look-spring (recentre pitch) | Off*, On | G | NEW | combat.c player_view_dir | CMB |
| FP11 | Hands tinted by fur / gloves | Off, On* | G | NEW | combat_hud.c draw_paw (skin + glove PMat) | HUD |
| FP12 | Head bob | Off, Low, Full*, Classic strong | G | EXT(OPT_HEAD_BOB) | combat.c combat_build_view | CMB |
| TP01 | 3rd-person distance | Close, Normal*, Far, Very far | G | NEW | combat.c update_camera | CMB |
| TP02 | 3rd-person height | Low, Normal*, High, Top-down-ish | G | NEW | combat.c update_camera | CMB |
| TP03 | Shoulder offset | Centred, Small, Normal*, Wide | G | NEW | combat.c update_camera | CMB |
| TP04 | Shoulder | Right*, Left, Auto (swaps near walls) | G | EXT(OPT_SHOULDER) | combat.c update_camera | CMB |
| TP05 | Follow smoothing | Off, Low, Normal*, High | G | NEW | combat.c update_camera | CMB |
| TP06 | Auto-recentre | Off, Slow*, Fast | G | NEW | combat.c update_camera | CMB |
| TP08 | Your citizen when the camera is close | Visible, Dither*, Hide | G | NEW | combat.c combat_build_view (GBA billboard), ps1 model.c model_player_draw | PLAT |
| TP11 | Turning | Free look*, Tank, Face movement | G | NEW | combat.c read_controls | INP |

#### Aim & lock-on (13)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| AI01 | Horizontal look speed | 1..10 (5*) | G | EXT(OPT_LOOK_SPEED) | combat.c read_controls | INP |
| AI02 | Vertical look speed | 1..10 (5*) | G | EXT(OPT_LOOK_SPEED) | combat.c read_controls | INP |
| AI03 | Invert X | Off*, On | G | EXT(OPT_INVERT_LOOK) | combat.c read_controls | INP |
| AI05 | Invert Y (third person) | Off*, On | G | EXT(OPT_INVERT_LOOK) | combat.c read_controls | INP |
| AI06 | Look acceleration | Off, Low*, High | G | NEW | combat.c read_controls | INP |
| AI08 | Aim slowdown (friction) | Off, Low, Medium*, High | G | EXT(OPT_AIM_ASSIST) | combat.c aim_solution | CMB |
| AI09 | Aim magnetism (pull) | Off, Low*, Medium, High | G | EXT(OPT_AIM_ASSIST) | combat.c aim_solution | CMB |
| AI10 | Vertical auto-aim (classic) | Off, On* | G | EXT(OPT_AIM_ASSIST) | combat.c aim_solution | CMB |
| AI11 | Lock-on | Off, Hold, Toggle* | G | EXT(OPT_LOCK_ON) | combat.c update_lock | CMB |
| AI12 | Lock-on priority | Nearest*, Closest to crosshair, Lowest HP, Biggest threat, Bosses first | G | NEW | combat.c find_target | CMB |
| AI14 | Lock break distance | Short, Normal*, Long | G | NEW | combat.c update_lock | CMB |
| AI15 | Auto-fire on target | Off*, On | G | NEW | combat.c weapon_tick | CMB |
| AI17 | Weak-spot highlight | Off*, On | G | NEW | combat_hud.c lock_reticle | HUD |

#### Crosshair & HUD (22)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| XH01 | Crosshair shape | Dot*, Cross, Circle, Heart, Star, Skull, Paw, None + 16 more (24) | G | EXT(OPT_CROSSHAIR) | combat_hud.c crosshair | HUD |
| XH02 | Crosshair colour | 6 today -> 32 paints + rainbow + auto-contrast (34) | G | EXT(OPT_CROSSHAIR_COLOR) | combat_hud.c crosshair_color | HUD |
| XH03 | Crosshair size | Small, Normal*, Big | G | NEW | combat_hud.c crosshair | HUD |
| XH05 | Crosshair gap | 0..3 (1*) | G | NEW | combat_hud.c crosshair | HUD |
| XH06 | Crosshair outline | Off, Dark*, Light | G | NEW | combat_hud.c crosshair | HUD |
| XH08 | Dynamic spread | Off, On* | G | NEW | combat_hud.c crosshair | HUD |
| XH10 | Hit-confirm flash | Off, White, Pink*, Red | G | NEW | combat_hud.c crosshair | HUD |
| XH12 | Lock-on reticle | Ring, Heart*, Brackets, Skull, Paw | G | NEW | combat_hud.c lock_reticle | HUD |
| XH13 | Scope overlay | Classic, Cute heart lens*, Minimal | G | NEW | combat_hud.c scope_overlay | HUD |
| XH14 | Crosshair in third person | Always*, Only when aiming, Off | G | NEW | combat_hud.c combat_draw_hud | HUD |
| HD01 | HUD layout | Cute*, Classic, Minimal, Classic status bar, Off | G | EXT(OPT_HUD_LAYOUT) | combat_hud.c combat_draw_hud/classic_bar | HUD |
| HD04 | HUD face | Off, Small, Big* | G | NEW | combat_hud.c face_badge | HUD |
| HD05 | Health display | Bar, Hearts*, Number, Face only | G | NEW | combat_hud.c health_panel | HUD |
| HD06 | Ammo display | Candy pill*, Number, Bar, Off | G | NEW | combat_hud.c ammo_panel | HUD |
| HD11 | Damage numbers | Off, Numbers*, Hearts, Stars, Words | G | EXT(OPT_DAMAGE_NUMBERS) | combat_hud.c damage_numbers | HUD |
| HD12 | Hit marker | Off, Visual, Visual + sound* | G | EXT(OPT_HIT_MARKERS) | combat_hud.c, audio_sfx | HUD |
| HD13 | Damage direction | Off, Arrows*, Edge glow | G | NEW | combat_hud.c damage_direction | HUD |
| HD14 | Low-health warning | Heartbeat + vignette*, Heartbeat, Vignette, Off | G | NEW | combat_hud.c health_panel + audio_sfx | HUD |
| HD15 | Pickup messages | Full*, Icons, Off | G | NEW | combat_hud.c toast | HUD |
| HD18 | Level timer | Off*, On | G | NEW | combat_hud.c top_right | HUD |
| HD19 | Boss bar | Big*, Small, Off | G | NEW | combat_hud.c boss_bar | HUD |
| HD20 | Kill counter | Off*, On | G | NEW | combat_hud.c top_left | HUD |

#### Weapons (6)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| WP01 | Auto-switch on pickup | Off, If better*, Always | G | NEW | combat.c pickups_tick -> combat_select_weapon | CMB |
| WP02 | Auto-reload | Off, On* | G | NEW | combat.c weapon_tick -> start_reload | CMB |
| WP03 | Skip empty weapons | Off, On* | G | NEW | combat.c combat_next_weapon | CMB |
| WP05 | Welding minigame | Easy, Normal*, Hard, Auto-weld | I | NEW | weld_game.c step_score / wg_update | MG |
| WP06 | Weapon names | Funny generated*, Plain | G | NEW | weld.c weld_name | UI |
| WP07 | Weapon XP rate | x0.5, x1*, x2 | I | NEW | combat.c combat_on_kill (xp) | CMB |

#### Audio (18)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| AU01 | Master volume | 0..10 (10* master; 8* others) | G | NEW | audio.c audio_sfx (per-SFX bus table) / audio_music | AUD |
| AU02 | UI sound volume | 0..10 (10* master; 8* others) | G | NEW (bus split of OPT_SFX_VOL) | audio.c audio_sfx (per-SFX bus table) / audio_music | AUD |
| AU03 | Combat sound volume | 0..10 (10* master; 8* others) | G | NEW (bus split of OPT_SFX_VOL) | audio.c audio_sfx (per-SFX bus table) / audio_music | AUD |
| AU04 | Ambience volume | 0..10 (10* master; 8* others) | G | NEW (bus split of OPT_SFX_VOL) | audio.c audio_sfx (per-SFX bus table) / audio_music | AUD |
| AU05 | Pet beep volume | 0..10 (10* master; 8* others) | G | NEW (bus split of OPT_SFX_VOL) | audio.c audio_sfx (per-SFX bus table) / audio_music | AUD |
| AU06 | Footstep volume | 0..10 (10* master; 8* others) | G | NEW (bus split of OPT_SFX_VOL) | audio.c audio_sfx (per-SFX bus table) / audio_music | AUD |
| AU07 | Jingle / fanfare volume | 0..10 (10* master; 8* others) | G | NEW (bus split of OPT_SFX_VOL) | audio.c audio_sfx (per-SFX bus table) / audio_music | AUD |
| AU08 | Heartbeat volume | 0..10 (10* master; 8* others) | G | NEW (bus split of OPT_SFX_VOL) | audio.c audio_sfx (per-SFX bus table) / audio_music | AUD |
| AU09 | Output | Stereo*, Mono, Swapped | G | NEW | audio.c send/chan pan -> plat_tone/plat_sample | AUD |
| AU11 | PS1 SPU reverb | Off, Room*, Studio, Hall, Space echo | G | NEW (PS1) | ps1 spu.c audio_hw_init (reverb area; needs SFX move, section 2) | PLAT |
| AU13 | Location music | Auto*, Jukebox playlist, Silence | G | NEW | audio.c audio_music, overworld.c ow_music_for_map | AUD |
| AU14 | Invasion music | Metal*, Cute remix, Keep location theme | G | NEW | overworld.c ow_music_for_map | AUD |
| AU16 | Babble style | Off, Bleeps*, Syllables, Mumbles, Kazoo | G | EXT(OPT_VOICE_BABBLE) | audio.c audio_voice_char / voice_tick | AUD |
| AU17 | Babble pitch offset | -2..+2 (0*) | G | NEW | audio.c voice_pitch | AUD |
| AU18 | Babble speed | Slow, Normal*, Fast | G | NEW | audio.c voice_tick | AUD |
| AU19 | Text blips (babble off) | Off, Soft*, Classic | G | NEW | ui_dialog.c dlg_update -> audio_sfx | AUD |
| AU22 | Duck music under dialogue | Off, On* | G | NEW | audio.c audio_tick (music gain while a dialogue is open) | AUD |
| AU23 | Squeaky plush sounds | Off, On* | G | NEW | audio.c footstep/hurt SFX choice by look.material | AUD |

#### Display & filters (22)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| DS01 | LCD filter | Off, Pet screen*, Everything, Grid only, Ghosting | G | EXT(OPT_LCD_FILTER) - today unwired | pet_scene.c lcd_* (grid), palette filter (Everything) | PAL |
| DS02 | GBA screen profile | Original GBA, SP frontlit, SP backlit*, DS/Micro, Game Boy Player, Emulator | G | NEW (GBA) | gba video.c video_load_palette (gamma LUT) | PLAT |
| DS03 | Brightness | -2..+2 (0*) | G | NEW | palette filter: new core palette_display() -> new hook plat_palette_changed() | PAL |
| DS04 | Contrast | -2..+2 (0*) | G | NEW | palette filter | PAL |
| DS05 | Saturation | Greyscale, Pastel, Normal*, Vivid | G | NEW | palette filter | PAL |
| DS06 | Gamma | 0..4 (2*) | G | NEW | palette filter | PAL |
| DS07 | Camera filter | None*, Storybook, VHS, Game Pal, Sepia, Nightvision + 8 new (14) | G | EXT(OPT_CAMERA_FILTER) - today unwired | palette filter (colour matrix per filter); VHS adds scanline rows in ui_draw.c | PAL |
| DS08 | PS1 dithering | Off, On* | G | NEW (PS1) | ps1 gfx.c gfx_init (draw.dtd) | PLAT |
| DS09 | PS1 screen X offset | -8..+8 (0*) | G | NEW (PS1) | ps1 gfx.c gfx_init (DISPENV screen.x) | PLAT |
| DS10 | PS1 screen Y offset | -8..+8 (0*) | G | NEW (PS1) | ps1 gfx.c gfx_init (DISPENV screen.y) | PLAT |
| DS15 | Frame-rate cap | 30*, 20 (steady) | G | NEW | gba main.c plat_end_frame / ps1 main.c plat_end_frame | PLAT |
| DS16 | 3D draw distance | Near, Normal*, Far | G | NEW | gba r3d.c / ps1 view3d.c plat_view3d (cell radius) | PLAT |
| DS17 | Fog | Off, On* | G | NEW | gba r3d.c colormap depth / ps1 view3d.c sprite_light | PLAT |
| DS19 | Crowd density | Low, Normal*, High | G | NEW | fp_island_gfx.c draw_walkers, overworld.c build_billboards | UI |
| DS20 | Portrait animation | Full*, Blink only, Static | G | NEW | portrait.c phase_of (masks phases) -> fewer sliced re-renders | POR |
| DS21 | Window skin | 12 today + 8 new (20) | G | EXT(OPT_WINDOW_SKIN) | ui_draw.c ui_style | UI |
| DS22 | Menu background | Animated*, Static, Plain colour | G | NEW | ui_draw.c ui_backdrop | UI |
| DS23 | Particles | Low, Normal*, High | G | NEW | fp_common.c fp_fx_burst, actor.c actor_fx | UI |
| DS25 | Palette-cycle effects (glitter, glow) | Off, On* | G | NEW | portrait.c phase_of (fx phases), part scripts with FX op | POR |
| DS26 | Title theme | 6 today + 4 new (10) | G | EXT(OPT_TITLE_THEME) | fp_title.c | UI |
| DS27 | Cursor | 6 today + 6 new (12) | G | EXT(OPT_CURSOR) | ui.c ui_cursor_glyph | UI |
| DS28 | Font style | Normal*, Bold, Shadowed, Rounded, Pixel serif, Spooky | G | EXT(OPT_FONT_STYLE) | ui.c ui_text (glyph table select) | UI |

#### Colour & accessibility (28)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| CB01 | Colour-blind aid | Off*, Protan, Deutan, Tritan, Achromat, High contrast | G | EXT(OPT_COLORBLIND) - today unwired | palette filter (daltonise matrix on the 256-entry palette) | PAL |
| CB03 | Colour-blind strength | 0..3 (2*) | G | NEW | palette filter | PAL |
| CB04 | District colour coding | Pink/purple*, Blue/orange, Add shapes | G | NEW | fp_common.c fp_status_color, ui_draw.c | UI |
| CB05 | Enemy outline highlight | Off*, On | G | NEW | actor.c actor_image (outline pass) / combat view | CMB |
| CB07 | Colour names in pickers | Off, On* | G | NEW | ui.c ui_paint_picker_draw, creator_draw.c swatch grid | UI |
| CB08 | Possessed icon | Off, On* | G | NEW | fp_common.c fp_status_icon | UI |
| AC01 | Text size | Normal*, Large (2x dialogue) | G | NEW | ui_dialog.c dlg_draw (ui_text_big) | TXT |
| AC03 | High-contrast windows | Off*, On | G | NEW | ui_draw.c ui_style | UI |
| AC04 | Holds become toggles | Off*, On | G | NEW | input.c input_update (latched bits for hold actions) | INP |
| AC05 | Button-mash assist | Off*, On | G | NEW | mg.c mg_pressed (repeat counts as presses) | MG |
| AC06 | Rhythm timing window | Normal*, Wide, Very wide, Auto-play | G | NEW | exorcism_scene.c judge, concert_scene.c show_update, cook_scene.c | MG |
| AC07 | Shooter game speed | 100%*, 85%, 70%, 50% | G | NEW | combat.c combat_tick (sub-tick scaling of demon/projectile speed) | CMB |
| AC08 | Minigames always easy | Off*, On | G | NEW | mg.c start_round, mg_lose_life | MG |
| AC09 | Menu repeat delay | Slow, Normal*, Fast | G | EXT(btn_repeat 12/3) | input.c btn_repeat | INP |
| AC10 | Menu wrap-around | Off, On* | G | NEW | ui.c ui_list_update | UI |
| AC11 | Remember cursor | Off, On* | G | NEW | ui.c ui_list_update (per-list memo) | UI |
| AC12 | Confirm button | A / Cross*, B / Circle | G | NEW | input.c input_update (menu context swaps A/B) | INP |
| AC14 | Sound captions ("[growl behind you]") | Off*, On | G | NEW | combat_hud.c toast from actor_event SFX | HUD |
| AC16 | Reduce motion | Off*, On | G | EXT(OPT_UI_BOUNCE) | ui_draw.c ui_pop_scale, portrait.c phase_of (breath/bounce), overworld.c view_shake | UI |
| AC17 | Photosensitive mode | Off*, On | G | EXT(OPT_REDUCE_FLASH) | combat_hud.c, concert_gfx.c, portrait phase_of (no glow pulse/cycling) | UI |
| AC18 | Arachnophobia mode | Off*, On | G | NEW | actor.c actor_demon_frame (Cloud Spider -> Cloud Bunny), creator hides spider parts | CMB |
| AC20 | Vape reskin (Cloud Nine Bubble Tea) | Off*, On | G | NEW | fp_island_gfx.c b_shop sign, story_script.c lines via text table | TXT |
| AC22 | Full auto-aim | Off*, On | G | NEW | combat.c aim_solution | CMB |
| AC23 | No-damage assist (not a cheat) | Off*, On | I | NEW | combat.c combat_hurt_player | CMB |
| AC24 | Skip combat encounters (story) | Off*, On | I | NEW | story.c story_push_combat (auto-win) | STORY |
| AC25 | No timers | Off*, On | G | NEW | mg.c, cook_scene.c heat_tick, arena_modes.c fight_tick | MG |
| AC27 | Long-press time | Short, Normal*, Long | G | NEW | input.c btn_held_ticks consumers via a new input_long() | INP |
| AC30 | Screen shake | Off, Low, Full*, Classic | G | EXT(OPT_SCREEN_SHAKE) | combat.c combat_shake, overworld.c view_shake | CMB |

#### Text & dialogue (12)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| TX01 | Auto-advance | Off*, On | G | NEW | ui_dialog.c dlg_update | TXT |
| TX02 | Auto-advance delay | Short, Normal*, Long | G | NEW | ui_dialog.c dlg_update | TXT |
| TX04 | Text log (last 16 lines) | Off, On* | G | NEW | ui_dialog.c enqueue -> ring in scene scratch | TXT |
| TX05 | Box position | Bottom*, Top, Auto | G | NEW | ui_dialog.c dlg_box | TXT |
| TX06 | Box opacity | Solid*, Translucent | G | NEW | ui_dialog.c dlg_draw (plat_shade_rect) | TXT |
| TX07 | Speaker portrait | Off, On*, Big | G | NEW | ui_dialog.c has_portrait/dlg_draw | TXT |
| TX08 | Name labels | Name*, Nickname, Name + pronouns | G | EXT(creator nicknames) | ui_dialog.c dlg_draw, fp_common.c | TXT |
| TX11 | Number format | 1,000*, 1.000, 1 000 | G | NEW | ui_fmt.c doomo_snprintf (new %M money format) | TXT |
| TX12 | Date format | Day-month*, Month-day | G | NEW | town.c town_time_str / fp_common.c | TXT |
| TX13 | Overworld chatter | Bubbles*, Thought clouds, Icons only, Off | G | NEW | overworld_npc.c chatter, fp_island.c draw_bubbles | UI |
| TX16 | Text speed | Typewriter-slow, Slow, Normal*, Fast, Instant | G | EXT(OPT_TEXT_SPEED) | ui_dialog.c dlg_update | TXT |
| TX17 | Subtitles | Off, On*, With speaker name | G | EXT(OPT_SUBTITLES) - today unwired | ui_dialog.c (babble-only lines get a caption line), cutscene.c caption | TXT |

#### Controls (18)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| CT01 | Scheme | Classic, Modern*, Southpaw, Legacy, One-hand, Custom | G | EXT(OPT_CONTROL_SCHEME) | combat.c read_controls + input remap preset | INP |
| CT02 | Strafe mode | L/R strafe*, Hold L + D-pad, Auto-strafe | G | NEW | combat.c read_controls | INP |
| CT03 | D-pad turn acceleration | Off, Low*, High | G | NEW | combat.c read_controls | INP |
| CT05 | Camera toggle | SELECT*, Chord, Disabled | G | NEW | combat.c combat_toggle_camera | INP |
| CT07 | Soft reset (A+B+START+SELECT) | Off, On* | G | NEW | game.c doomo_frame (combo -> SCENE_TITLE) | INP |
| CT09 | Turbo fire | Off*, On | G | NEW | combat.c weapon_tick | CMB |
| CT10 | Button prompts | Auto*, GBA letters, PS1 symbols | G | NEW | ui.c ui_button (glyph set) | UI |
| RM01 | Remap: shooter | permutation of 14 buttons (Reset*) | G | NEW | input.c input_update (context IC_SHOOTER) | INP |
| RM02 | Remap: overworld / island | permutation of 14 buttons (Reset*) | G | NEW | input.c input_update (context IC_WORLD) | INP |
| RM03 | Remap: menus | permutation of 14 buttons (Reset*) | G | NEW | input.c input_update (context IC_MENU) | INP |
| GB01 | GBA sleep combo | Off, L+R+SELECT*, L+R+START | G | NEW (GBA) | gba main.c read_pad -> sleep (SOUNDCNT off, Stop SWI, keypad IRQ wake) | PLAT |
| GB02 | X / Y in menus | SELECT chords*, L/R act as X/Y | G | EXT(DOOMO_INPUT_SYNTH_XY) | input.c input_update | INP |
| GB03 | Swap L and R | Off*, On | G | NEW (GBA) | gba main.c read_pad | PLAT |
| PS02 | Analog sticks | Off*, Move + look, Swapped | G | NEW (PS1) | ps1 pad.c pad_analog -> new hook plat_analog() -> combat.c read_controls | PLAT |
| PS03 | Stick deadzone | 0..5 (2*) | G | NEW (PS1) | ps1 pad.c pad_read (threshold 24..88) | PLAT |
| PS04 | Stick curve | Linear, Exponential* | G | NEW (PS1) | combat.c read_controls (analog path) | PLAT |
| PS05 | L2 / R2 | Look up/down*, Lock-on + roll, Weapon swap, Camera | G | NEW (PS1) | combat.c read_controls | INP |
| PS07 | Memory card slot | Slot 1*, Slot 2, Ask | G | NEW (PS1) | ps1 memcard.c (bu00 / bu10) | PLAT |

#### Cheats & modifiers (49)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| CH00a | Big-head mode | Off*, On | I | OPT_CHEAT_BIG_HEAD - today unwired | portrait.c pctx_init (head_size + 4, render epoch) | POR |
| CH00b | Brutal residents | Off*, On | I | OPT_CHEAT_BRUTAL_RESIDENTS - today unwired | portrait.c pctx_init (render-time Hellvile-ify: T22 restyle) | POR |
| CH00c | Paper mode | Off*, On | I | OPT_CHEAT_PAPER_MODE - today unwired | portrait_gfx.c pc_part (flat shading, thin outline) | POR |
| CH00d | Everyone is a ghost | Off*, On | I | OPT_CHEAT_ALL_GHOSTS - today unwired | portrait.c pctx_init (ghost = 1), pf_recolor_ghost | POR |
| CH01 | Infinite ammo | Off*, On | I | NEW | combat.c weapon_tick | CMB |
| CH02 | No reload | Off*, On | I | NEW | combat.c start_reload | CMB |
| CH03 | Boop of Doom (one-hit kills) | Off*, On | I | NEW | actor.c actor_damage | CMB |
| CH04 | Rapid fire | Off*, On | I | NEW | combat.c weapon_tick | CMB |
| CH05 | Bullet time on demand | Off*, On | I | NEW | combat.c combat_tick (actors/projectiles at half rate while R2 / SELECT+R held) | CMB |
| CH06 | Moon jump | Off*, On | I | NEW | combat.c player_tick | CMB |
| CH07 | Ghost walk (overworld noclip) | Off*, On | I | NEW | overworld.c can_stand | UI |
| CH08 | Tiny heads | Off*, On | I | NEW | portrait.c pctx_init | POR |
| CH09 | Citizen size | Pocket, Normal*, Giant | I | NEW | portrait.c setup_figure + fp_common.c fp_chibi scale | POR |
| CH10 | Furry Island (render every citizen as a critter species) | Off*, On | I | NEW | portrait.c pctx_init (species override) | POR |
| CH11 | Mirror world | Off*, On | I | NEW | GBA BG2 affine PA=-1; PS1 gfx2d.c x mirror + r3d.c mirrored projection | PLAT |
| CH12 | Party hats on demons | Off*, On | I | NEW | actor.c actor_image overlay | CMB |
| CH13 | Hugging demons (pacifist) | Off*, On | I | NEW | actor.c actor_damage -> befriend | CMB |
| CH14 | Possession party (cosmetic) | Off*, On | I | NEW | portrait.c pctx_init (creepy look on everyone, status unchanged) | POR |
| CH15 | No corruption | Off*, On | I | NEW | sim_soul.c sim_corrupt_natural | SIM |
| CH16 | Instant pet evolution | Off*, On | I | NEW | pet.c evolve | PET |
| CH17 | Eternal time of day | Off*, Day, Night | I | NEW | sim.c minute_step | SIM |
| CH19 | Rainbow palette cycling | Off*, On | I | NEW | palette filter (hue rotate per tick) | PAL |
| CH20 | 1-bit mode | Off*, On | I | NEW | palette filter (threshold) | PAL |
| CH21 | Voice pitch | Chipmunk, Normal*, Deep | I | NEW | audio.c voice_pitch | AUD |
| CH22 | Squeak everything | Off*, On | I | NEW | audio.c audio_sfx (every SFX -> squeak sample, pitch kept) | AUD |
| CH23 | Big ears and tails | Off*, On | I | NEW | portrait.c pctx_init (ear/tail size +3) | POR |
| CH24 | Face shuffle (daily, cosmetic) | Off*, On | I | NEW | portrait.c pctx_init (seeded field swap by day) | POR |
| CH25 | Free shop | Off*, On | I | NEW | fp_shop.c price | UI |
| CH26 | Turbo babies | Off*, On | I | NEW | sim_social.c sim_make_baby (chance x10) | SIM |
| CH27 | Retro 93 palette | Off*, On | I | NEW | palette filter (brown/grey matrix) | PAL |
| CH28 | Plush mode (every material plush) | Off*, On | I | NEW | portrait.c pctx_init (material override) | POR |
| CH29 | uwu-speak filter | Off*, On | I | NEW | ui_dialog.c copy_str | TXT |
| CH30 | Barrel party | Off*, On | I | NEW | combat.c combat_start (extra barrels via actor_spawn_thing) | CMB |
| CH31 | Plushie projectiles | Off*, On | I | NEW | actor.c actor_projectile (sprite swap) | CMB |
| MD01 | Damage taken | x0.5, x1*, x2, x4 | I | NEW | combat.c combat_hurt_player | CMB |
| MD02 | Demon HP | x0.5, x1*, x2 | I | NEW | actor.c actor_spawn_demon | CMB |
| MD03 | Ammo pickups | x0.5, x1*, x2 | I | NEW | combat.c pickups_tick | CMB |
| MD04 | Iron Plush (one life) | Off*, On | I | NEW | combat.c player_dead_tick | CMB |
| MD05 | No-HUD challenge | Off*, On | I | NEW | combat_hud.c combat_draw_hud (hidden) + levels.c rank bonus | CMB |
| MD06 | Melee only | Off*, On | I | NEW | levels_play.c enforce_challenge | CMB |
| MD07 | Weapon style lock | Off*, Cute only, Brutal only | I | NEW | levels_play.c enforce_challenge | CMB |
| MD08 | Demon speed | x0.75, x1*, x1.25, x1.5 | I | NEW | actor.c demon_speed | CMB |
| MD09 | Fast, respawning demons | Off*, On | I | NEW | actor.c demon_speed + arena_modes.c spawn_demon (respawn timer) | CMB |
| MD10 | Pistol start | Off*, On | I | NEW | levels_play.c setup_challenge | CMB |
| MD11 | Needs keep ticking in combat | Off*, On | I | NEW | sim.c sim_tick (runs while SCENE_COMBAT is on top) | SIM |
| MD12 | Pet must survive | Off*, On | I | NEW | pet.c pass_away -> levels_play.c dead_draw | CMB |
| MD13 | Pacifist tracker | Off*, On | I | NEW | combat.c combat_on_kill | CMB |
| MD14 | Random weapon each wave | Off*, On | I | NEW | arena_modes.c horde_start_wave | CMB |
| MD15 | Mirrored levels | Off*, On | I | NEW | map.c map_load (cells mirrored on load) | CMB |

#### Modes & minigames (22)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| PM01 | Arena: start wave | 1*, 5, 10 | I | NEW | arena_modes.c am_start | CMB |
| PM02 | Arena: demon mix | All*, Cute only, Grotesque only, Single type | I | NEW | arena_modes.c spawn_demon | CMB |
| PM03 | Arena: wave timer | Off*, On | I | NEW | arena_modes.c fight_tick | CMB |
| PM04 | Arena: maps | Rotate*, Fixed | I | NEW | arena_modes.c am_start | CMB |
| PM05 | Boss Rush: healing between bosses | Full*, Partial, None | I | NEW | arena_modes.c rush_next | CMB |
| PM06 | Horde: shop between waves | Off, On* | I | NEW | arena_modes.c shop_update | CMB |
| PM07 | Exorcism: phase order | Fixed*, Random | I | NEW | exorcism_scene.c start_ritual | MG |
| PM08 | Exorcism: holy-water aim assist | Off, On* | I | NEW | exorcism_scene.c update_water | MG |
| PM09 | Exorcism: true-name clues | 2, 3*, 4 | I | NEW | exorcism.c exorcism_clue | MG |
| PM10 | Welding: heat speed | Slow, Normal*, Fast | I | NEW | weld_game.c wg_update | MG |
| PM11 | Fishing: bite window | Easy, Normal*, Hard | I | NEW | fish_scene.c roll_bite | MG |
| PM12 | Fishing: line-tension assist | Off*, On | I | NEW | fish_scene.c reel_tick | MG |
| PM13 | Cooking: timer speed | Slow, Normal*, Fast | I | NEW | cook_scene.c chop_tick/stir_tick/heat_tick | MG |
| PM14 | Cooking: judge strictness | Soft, Normal*, Strict | I | NEW | cook_scene.c to_result | MG |
| PM15 | Concert: note speed | Slow, Normal*, Fast | I | NEW | concert_scene.c show_update | MG |
| PM16 | Concert: lyrics | Off, On*, Clean only | I | NEW | concert_scene.c draw_karaoke | UI |
| PM17 | Fun Fair difficulty | Easy, Normal*, Hard | I | NEW | mg.c start_round | MG |
| PM18 | Tamagotchi-only mode speed | Real time*, x2, x4 | I | NEW | pet.c pet_tick_minute (MODE_TAMA_ONLY) | PET |
| PM19 | Dreams | Watch*, Skippable, Off | I | NEW | dream.c dream_update | UI |
| PM20 | Studio metronome | Off, On* | G | NEW | studio.c playback tick | AUD |
| PM21 | Studio default tempo | 80, 100, 120*, 140, 160 | G | NEW | studio.c new-song defaults | AUD |
| PM22 | Graveyard flowers wilt | Off, On* | I | NEW | fp_common.c fp_flowers | UI |

#### Creator & photo (13)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| CR01 | Creator mode | Simple, Advanced*, Everything | G | NEW | creator.c row filter (field tier) | UI |
| CR02 | Preview stage | Room*, Plain, Hillvile, Hellvile, Sky | G | NEW | creator_draw.c stage_colors | UI |
| CR03 | Turntable auto-spin | Off*, On | G | NEW | creator.c (CRP_TURN idle) | UI |
| CR04 | Preview cycles expressions | Off, On* | G | NEW | creator.c cr_preview_expr | UI |
| CR05 | Locked options | Show silhouettes*, Hide | G | NEW | creator.c cr_value_locked / picker list | UI |
| CR06 | Default randomiser theme | Cute*, Spooky, Goth, Pastel, Punk, Royal, Furry, Demon, Angelic, Chaos | G | NEW | creator.c randomise -> gen theme weights | POR |
| CR07 | Confirm discard | Off, On* | G | NEW | creator.c (B with changes) | UI |
| CR08 | Swatch grid | 8 columns*, 11, 16 | G | NEW | creator_draw.c swatch grid (cr_color_cols) | UI |
| CR09 | Show wardrobe meter | Off, On* | G | NEW | creator_draw.c footer | UI |
| CR10 | Photo: hide HUD | Off, On* | G | NEW | photo scene (new, fp group) | UI |
| CR11 | Photo: frame | None*, Polaroid, Film strip, Hearts, Bones, LCD | G | NEW | photo scene | UI |
| CR12 | Photo: sticker set | Cute*, Spooky, Both | G | NEW | photo scene | UI |
| CR13 | Photo: self-timer | Off*, 3 s, 10 s | G | NEW | photo scene | UI |

#### System (2)

| ID | Option | Values (`*` default) | Scope | Status | Wires into | Test probe |
|---|---|---|---|---|---|---|
| SY02 | Verify after saving | Off, On* | G | EXT(save_verify) | save.c save_game -> save_verify | SAVE |
| SY04 | Show FPS and RAM | Off*, On | G | NEW | game.c doomo_frame overlay (plat mailbox fps) | UI |

### 6.6 Not in this plan (100 of the 469 wishlist options)

| reason | options |
|---|---|
| needs new hardware drivers or timing work | PS01 DualShock vibration (the BIOS pad driver used by `ps1/src/pad.c` cannot send motor commands; needs a new SIO driver), PS06 PS1 mouse, PS08 second controller, PS09 light gun, GB05 Game Boy Player rumble, AU10 GBA speaker EQ and AU21 mixing rate (both change the IWRAM mixer, which has 0 B of IWRAM budget), DS11/DS12/DS13/DS14 PS1 overscan, PAL 50, 480i menus and CRT scanlines (VRAM layout and frame timing) |
| needs a new system (later wave) | ST08 overworld follower, ST09 chapter replay, ST10 New Game+ carry-over, PT11 pet follower, LS11 ageing and RO20 baby growth (the `age_look` field already exists, so no format change will be needed), LS03 offline catch-up (neither platform has a clock), RO16 where babies live, RO22 adoption, IS08 auto-fill homes, IS10 festivals, HD07 minimap, HD16 weapon wheel, AC02 second font, AC13 sound radar, AC26 navigation assist, TX03 skip seen text (needs seen-bits), CH18 eternal festival (needs IS10 festivals) |
| merged into a listed option | AI04 (into AI03/AI05), XH04/XH07/XH09/XH11/XH15 (into XH01/XH03/XH06), TP13 (OPT_CAMERA_DEFAULT stays), GB06 (into GM05), RM04/RM05 (one permutation per context; no conflicts possible), SY01 (scope is fixed per option), SY05 (every page has Reset), AU12 (into AU11), CB02 (palette method only) |
| cheap, low value: next-wave backlog | GM09, GM15, LS09, LS16, ND11, RO09, RO11, PX04, PX06, PX11, PT01, GR06, GR10, GR14, GR18, MV05, MV07, FP08, FP10, TP07, TP09, TP10, TP12, AI07, AI13, AI16, HD02, HD03, HD08, HD09, HD10, HD17, WP04, WP08, WP09, WP10, AU15, AU20, AU24, DS18, DS24, CB06, AC15, AC19, AC21, AC28, AC29, TX09, TX10, TX14, TX15, CT04, CT06, CT08, GB04, CH32 chunky pixels, SY03 (the backup save copy has no flash space) |

### 6.7 "An option must actually do something": `test_options_effect`

Every row of `doomo_option_meta[]` carries a probe kind. For each option, the test runs a fixed scenario at the
default value and at every other value, and requires a **different hash** for each (or for each unlocked value):

| probe | scenario | hash of |
|---|---|---|
| SIM | 3 in-game days on a seeded premade island | all hot records + edges + news |
| CMB | 300 ticks of a scripted combat (bot input) | player, actors, camera state |
| HUD | one HUD frame in a fixed combat state | screen pixels |
| UI | the scene's first 30 frames | screen pixels |
| AUD | 120 ticks of a fixed scene | the `plat_tone`/`plat_sample` command log |
| PAL | - | `palette_display()` output |
| POR | 8 citizens, bust + figure | image pixels |
| INP | a scripted button sequence | logical button log |
| TXT | a fixed dialogue | dialogue state per tick + text buffer |
| MG / PET / STORY / SAVE | scene-specific scripted runs | scene state |
| PLAT | GBA/PS1 harness scripts | screenshot or TTY hash per value |

An option that fails its probe fails CI. The same test checks that every option has a help text, a valid default and
a scope, and that `I`-scope options never change hashes of global-only probes.

---

## 7. Risks and how to test

| # | risk | likelihood / impact | mitigation | test |
|---|---|---|---|---|
| 1 | Stale `res_look()` pointer used after eviction | M / H (wrong renders, rare) | 1.8 pointer rule, pins, poison in test builds, `res_look_check()` asserts | round-robin decode test; SAN=1 run of all runner scripts |
| 2 | Heap too small for someone's Empty Island | M / M | meter, Simplify, re-base, Look Book; 1000 legacy-detail hand-made citizens fit | `test_resstore` capacity table (2.6) as assertions |
| 3 | A later save chunk breaks the 88 KB guarantee | M / H | `DoomoSaveExt.max_size`; bound computed, not measured | `test_save` worst-case bound |
| 4 | v1 saves decode differently after the roster switch | L / H | `roster_v1.txt` golden; frozen `save_v1.c` | **v1 save corpus**: before landing wave 0, write story, premade-with-edits, random-with-edits, empty-1000-hand-made and `town_fill` worst-case saves with today's build into `core/tests/data/saves_v1/`; v2 must load each and produce the same legacy view per citizen |
| 5 | Fat generator differs between native, GBA and PS1 | L / H (random islands and saves would differ) | integer-only RNG substreams, xcheck | on-device `fpgold` (3.6) in both harnesses |
| 6 | Render cost or part-id blow-up with kitchen-sink looks | M / M | occlusion, decals, LOD, `partc.py` area check | pixel-visit bound, part-id <= 240, GBA bench gates (4.8) |
| 7 | PS1 RAM estimate for new code/art is wrong by +-40 % | M / M | measured reclaims land first (wave 0: -101 KB before spending); per-wave elf2exe threshold; levers list (2.4) | `elf2exe.py` >= 128 KB threshold in CI |
| 8 | Creator overlay group grows past the 108 KB slot | L / M | names and presets are data; split the group (creator UI vs creator tables) if needed | `ovl_plan.py` group report gate (<= 100 KB) |
| 9 | Mirrors (species, skin, soul, age) drift from the look | L / M | single write path `res_commit()` | `mirrors_match` property test |
| 10 | Kid/teen safety regressions with Age look | L / H | Age look drives the existing gate; sanitize refuses romance != NONE for baby/kid; body sliders are plush ranges only | `test_sim`: custom kid + any adult, 10^4 days, no crush/dating edges |
| 11 | Load time on GBA grows | M / L | blobs validated structurally (2-5k cycles each); no full decode at load; mirrors rebuilt from blob fields and cheap bases | harness load timer: <= +0.3 s for the worst island |
| 12 | Content load (1,645 new options need art) | H / M | part scripts are small text; waves; deferred fields render nothing until their wave (sanitize zeroes them) | `test_portrait` distinctness per enabled list (`MAXOPT` 40 -> 64) |
| 13 | Option explosion confuses players | M / L | categories, Find, presets, defaults unchanged, help on every row | UI runner scripts at both resolutions |
| 14 | Landing while the CI memory pass edits EWRAM | H / M | wave 0 is RAM-positive on both platforms before any feature spends; land wave 0 alone, re-measure, then spend | memreport and elf2exe gates per PR |
| 15 | Settings header rewrite wears flash or corrupts a slot | L / H | rewrite only on options exit with changes; CRC-16 on settings; erase/program the header sector only; on CRC failure fall back to defaults | GBA harness `test-save` extended: settings round trip, power-cut simulation (abort between erase and program) |

**Test inventory (new or changed):** `test_resstore.c` (codec, heap ops vs a reference model for 10^5 random ops, cache
and pointer rules, capacity), `test_save.c` (v2 round trip, v1 corpus, worst-case bound, hostile blobs), `test_roster.c`
(compat golden, every new value used), `test_resident.c` (v1 and v2 goldens, mirrors), `test_creator.c` (a row for
every enabled registry field, modes, Find, pins, undo ring, meter), `test_portrait.c` (distinctness per new list, cost
bounds, part ids), `test_options_effect.c` (6.7), `test_input_remap.c` (permutation invariants), and harness scripts
`fpgold`, `creator_scroll_fps`, `island40_fps`, `ps1_impostor`.

---

## 8. Implementation plan

| wave | contents | RAM effect | files |
|---|---|---|---|
| **0: enablers** (RAM-positive) | hot/cold split, `resstore`, registry, v2 save + frozen v1 reader, packed roster + BPE, PS1 SFX to SPU, settings header, options storage split, `res_look` call-site migration (119 sites) | GBA +4.6 KB EWRAM free. PS1: -100.8 KB of data and const freed, about +15 KB of `resstore` code, so about +86 KB of headroom before wave 1 spends | `resident.h`, `resstore.[ch]` (new), `look_fields.c` (new), `town.[ch]`, `save.c`, `save_v1.c` (new), `roster.h`, `tools/roster_build.py`, `options.c`, `settings.c` (new), `sim.c` (`sim_traits`), `sim_soul.c`, `sim_care.c`, `sim_social.c`, `portrait.c` (keys), `fp_common.c`, `ps1/src/spu.c`, `ps1/tools/` (SFX.BIN), `core/src/audio.c` |
| **1: free variety** | 21 list appends, tones, palette filter (12 options), 8 dead options wired, about 200 pure-data options, remapping, part-script interpreter + `partc.py` | ROM/PS1 code only | `portrait_*.c`, `tools/partc.py`, `core/art/parts/`, `palette_filter.c` (new), `input.c`, `fp_options.c`, `options_text.c` (new), `gba/src/video.c`, `ps1/src/gfx.c` |
| **2: core looks** | the 52 wave-2 fields, PS1 head impostor + `model.c` body, creator v2 (tabs, sections, modes, Find, meter, delta undo) | as budgeted | `portrait_*.c`, `ps1/src/model.c`, `creator*.c` |
| **3: deluxe** | the 235 wave-3 fields, Look Book, Look Password, themes, remaining options | as budgeted | same + `creator_data.c` |
| **4: systems** | species +24, extra anatomy, mobility, prosthetics, hybrids, situational outfits + auto-switch, photo mode | content only (format already there) | painters, `sim_social.c` heredity, photo scene (free-play group) |

Each wave lands with its gates green: memreport, elf2exe, ovl_plan, the save bound, the GBA bench and the fps
harness scripts.

---

## Appendix: scripts behind the numbers (`scratchpad/custom/growB/`)

| script | produces |
|---|---|
| `parse.py` | wishlist items (310) to `items.json` |
| `fields.py`, `gen_struct.py`, `gen_tables.py` | the field registry, the `DoomoLook` struct and the per-section tables (1.4, 1.6) |
| `sim.py` | blob sizes and heap capacity from the real roster looks (1.7, 2.6) |
| `roster_pack.py` | packed roster size (62,938 B) from `core/gen/roster_data.c` |
| `../grow/measure.py`, `../grow/bpe.py` | roster v1-width packing and BPE/LZ sizes of bios and jobs |
| `budget.py` | the GBA/PS1 tables and the save bound (2.2-2.5) |
| `options.py` | the 373 options (6.5) |
| `probe.c`, `diffs.c` (native, built from repo sources into `growB/obj`) | struct sizes, generator and render timings, preset-vs-roster field differences (36.5 of 53 fields differ on average) |
