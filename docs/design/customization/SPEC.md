# DOOMOGACHI customisation v2: the final spec

**Status:** final, for implementation. It supersedes `design_packed.md`, `design_grow.md` and `design_hybrid.md`.

**Baseline:** HEAD `8cc7910` (2026-10-18, clean tree). Numbers marked *measured* come from its build outputs, which were read and never written. P0's first commit installs this file as `docs/design/customization/SPEC.md`.

**Files:** `/tmp/claude-0/-home-user-DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/ea792815-11f0-5f9f-bdfe-5997cdfc4994/scratchpad/custom/SPEC.md` (this file). The normative appendices are in the same directory:
- `SPEC_appendix_fields.md`: all 304 deep fields with their full value lists;
- `SPEC_appendix_options.md`: all 458 options with their full wiring notes.

## 0. Decision and summary

The judges split:
- Judge 1: grow 7.7, hybrid 7.0, packed 6.0.
- Judge 2: hybrid 7.9, grow 7.6, packed 6.6.
- Mean: grow 7.65, hybrid 7.45.

**The base is hybrid's data model:** a fixed 76 B record, a 384-bit gene, 14 inline core fields, and a sparse deep layer resolved own > style > ROM > derived.

**Why.** Every fatal flaw the judges found in hybrid is a ledger flaw: PS1 net-negative, a static save bound that does not fit, the wrong fps diagnosis, and a gene that is exactly full. Grafting grow's measured moves fixes each of them. Grow's flaws are architectural: decode-cache pointer lifetimes, the widest wave 0, a header flash rewrite on every options exit, a partly open age gap, and a 328 B `HOME` margin.

| from | taken |
|---|---|
| **hybrid** (base) | the 76 B record and gene; 14 core fields; the 8 KB look pool and `LOOK` chunk; 4-layer resolution with Classic, Varied and Authored; derivation rules; the roster seed search; Look Book styles; **an age gate for every citizen** (it fixes today's gap where adults can target in-game babies and custom kids, `sim.c:117-231`); safe GBA flash trailer migration and a settings journal; option sinks; `palfx`; 10 tabs and 43 sections; Classic pixel parity |
| **grow** | the measured PS1 reclaims (packed roster, bios and edges on disc, Huffman jobs, SFX to SPU); the offline job-venue table; the **drift-proof save bound**; the creator fps fix (lock bitmaps, dirty regions, view-masked preview, harness gate); `test_options_effect`; colour **tones**; the PS1 **head impostor** |
| **packed** | **linked colours**; `check_options.py`; remapping as a bijective permutation per input context; the "0 = today" pixel golden |
| **judge fixes** | trim `like_hobby`, `like_music` and `catchphrase` to their minimum widths (3 spare gene bits); re-baseline everything at today's HEAD; a static per-chunk save bound; harness fps gates |

| | today (*measured*) | this spec |
|---|---|---|
| look fields per citizen | 48 | **62 core + 304 deep = 366** (+ 21 identity), all renderable, on every citizen |
| new choices | - | **+300 catalog values**, **~2,000 non-colour deep choices**, **96 colours + 15 links** on each of 68 deep colour fields |
| options | 58 (8 dead) | **458**, every one wired to a named consumer |
| `DoomoResident` | 96 B | **76 B** |
| GBA EWRAM free | 7,104 B | base **+12,176 B** (19,280 B at the 8cc7910 base), or base +8,744 B after `HOME` (the house's data; gate 12,352, floor 8,192). Corrected in CF-4/CF-15 |
| GBA IWRAM / ROM | 6,320 B stack / 3,076,700 B | +0 B / **+180 KB** (C21 share 190 KB) |
| PS1 below the stack top | 110,624 B | base **+6,900 B** (117,524 B at the 8cc7910 base; peaks at about 224 KB after the reclaims; C5 gate 73,728). Corrected in CF-15 |
| save slot 1 (90,112 B) | measured 59,148 B | a **statically proven** bound the sim cannot move, once SONG/FPLY/FISH declare `max_size` (CF-9); every v1 island migrates within **86,635 B** (CF-10) |
| creator | 9 tabs, 105 rows, 13.4 fps on GBA | **10 tabs, 43 sections, 392 rows, ≥ 20 fps on GBA** |

**Baselines** (*measured* at `8cc7910`):

- **GBA top EWRAM users:** `doomo_town` 109,524, `doomo_map` 32,780, the portrait pool 18,432, `gba_big` 17,408, scene scratch 12,288, `sim_s` 7,084.
- **PS1** (`_end = 0x801E4FD0`, slot 116 KB) resident objects that this spec moves: `roster_premade` 96,000, bios 44,931 + 2,000, jobs 8,056, edges 9,258, SFX PCM 47,793, nicks 1,024.
- **Saves:** slot 0 holds 40,960 B; `DOOMO_SAVE_EXT_MAX` is 8,192; only `HOME` (3,432) and `STRY` (96) declare `max_size`; `SONG`, `FPLY`, `FISH` and `NICK` declare 0. A v1 record is 509 bits at most: look 232, identity 100, hot 96, name ≤ 81.
- **Moving baseline:** a CI memory pass (task #77, which owns `gba.ld`, `bigbuf.c`, `gba_plat.h`, `memreport.py` and `scene_scratch.c` until G0) is in flight, and the live tree is over budget on both consoles. Every figure here is a delta. **Every gate in this spec is relative to the measurement of the tree a package merges into (CF-1).** P0 starts only after G0, and captures its goldens on the post-G0 tree, not on `8cc7910`.

## 1. Data layout (exact C)

### 1.1 Layers and resolution

| layer | where | RAM |
|---|---|---|
| L0 base | ROM roster record (premade), `resident_generate*()` (random), `resident_default()` (empty): the save delta base | 0 |
| L1 core | `DoomoResident` for all 1001: hot sim bytes, name, 384-bit gene | 76 B each |
| L2 own deep | the citizen's run in `doomo_look_pool`, saved as chunk `LOOK` | 4 + 2k B |
| L3 shared deep | Look Book style runs (ids 1001..1016); the premade ROM deep table | pool / ROM |
| L4 derived | `look_derive()`: a pure function of (core look, species, persona, district, `look_seed`, `auto_style`) | 0 |

A deep field resolves own > style > ROM > derived. A stored entry whose low 7 bits are 0 is a tombstone: it forces the derived value over a style or ROM value. An explicit choice 0 ("as today") is stored as 1 (CF-21). One pass fills the 304 B `DoomoDeep` that painters read.

### 1.2 `resident.h` v2

```c
#define DOOMO_GENE_BYTES 48
typedef struct DoomoResident {          /* all uint8_t/char: no padding, no endianness */
    uint8_t species;                    /*  0 hot copy (22 readers)            */
    uint8_t stat[STAT_COUNT];           /*  1 0..100                           */
    uint8_t status, flags, exorcism, level, mood, activity, death_cause; /* 9..15 */
    char    name[DOOMO_NAME_LEN];       /* 16 11 chars + NUL                   */
    uint8_t gene[DOOMO_GENE_BYTES];     /* 28 384 bits LSB-first (1.3)         */
} DoomoResident;                        /* 76 */
DOOMO_STATIC_ASSERT(sizeof(DoomoResident) == 76, resident_is_76_bytes);
DOOMO_STATIC_ASSERT(MOOD_COUNT <= 16 && ACT_COUNT <= 16 && DEATH_COUNT <= 8, hot_widths);
/* (CF-2) DoomoResident has 1-byte alignment and doomo_town.res[] can sit at any even
 * offset. gene_get/gene_set/look_key read the gene BYTE-WISE (words are assembled from
 * bytes, as portrait.c:506-517 look_hash does today). A uint32_t load from gene[] is a
 * wrong value on ARM7TDMI (rotated) and an address-error exception on the R3000.
 * (CF-3) hair_highlight keeps today's sentinel: the gene stores 32 for LOOK_NO_COLOR
 * (0xFF), and res_look() maps it back, as save.c field_get/field_set do in v1. */

typedef struct DoomoLook {              /* wide working look, never stored: 64 B */
    uint8_t species, body, height, build, skin, head_shape, head_size, eye_style,
            eye_color, eye_size, eye_spacing, eye_height, eye_tilt, eye_count,
            brow_style, brow_height, nose_style, nose_size, mouth_style, mouth_size,
            fangs, hair_style, hair_color, hair_highlight, facial_hair, ears, horns,
            horn_color, horn_size, tail, wings, halo, marking, marking_color, cheeks,
            top, top_color, bottom, bottom_color, shoes, shoes_color, hat, hat_color,
            accessory, accessory_color, aura, pattern, pattern_color;   /* 0..47 v1 order */
    uint16_t look_seed;                 /* 48 0..2047                          */
    uint8_t  auto_style, age, pupil, sclera, fur_pattern, fur_color3, glasses,
             outer, legwear, blush_style, ear_fold, hood_up, handedness, pad; /* 50..63 */
} DoomoLook;
DOOMO_STATIC_ASSERT(sizeof(DoomoLook) == 64 && offsetof(DoomoLook, pattern_color) == 47, look_v2);

typedef struct DoomoCitizen {           /* 112 B unpacked citizen */
    DoomoLook look; char name[DOOMO_NAME_LEN];
    uint8_t voice[VOICE_COUNT], persona[PERS_COUNT], stat[STAT_COUNT];
    uint8_t status, flags, pronoun, romance, bday_month, bday_day, like_food, like_hobby,
            like_music, like_color, hate_food, fear, catchphrase, exorcism, level, mood,
            activity, death_cause;
} DoomoCitizen;
enum { AUTO_CLASSIC, AUTO_VARIED, AUTO_AUTHORED };
enum { AGE_AUTO, AGE_BABY, AGE_KID, AGE_TEEN, AGE_ADULT, AGE_ELDER };

void     res_unpack(const DoomoResident *r, DoomoCitizen *out);
void     res_pack(const DoomoCitizen *c, DoomoResident *out);   /* sanitizes */
void     res_look(const DoomoResident *r, DoomoLook *out);      /* ~1.6k GBA cycles */
uint32_t gene_get(const uint8_t *g, int gf);                    /* ~25 cycles */
void     gene_set(uint8_t *g, int gf, uint32_t v);              /* clamps */
#define RES_PERSONA(r,i) gene_get((r)->gene, GF_PERSONA0 + (i))
#define RES_VOICE(r,i)   gene_get((r)->gene, GF_VOICE0 + (i))
#define RES_AGE(r)       gene_get((r)->gene, GF_AGE)
#define RES_LOOK(r,lf)   gene_get((r)->gene, doomo_look_gf[lf])   /* lf != LF_SPECIES: species is the hot byte */
/* ... one RES_* macro per removed member (pronoun, romance, bday_*, like_*, hate_food, fear, catchphrase) */
void res_to_v1(const DoomoCitizen *c, uint8_t out96[96]);       /* v1 bridge: saves, goldens */
void res_from_v1(const uint8_t in96[96], DoomoCitizen *out);
uint32_t resident_fingerprint(const DoomoResident *r);          /* all 76 B */
uint32_t citizen_fingerprint_v1(const DoomoResident *r);        /* over res_to_v1(): frozen */
/* resident_generate/_hellborn/_randomize_look/_sanitize/_default keep their signatures */
```

Removed members make every unconverted reader a compile error on all three toolchains. Re-measured at `8cc7910` (CF-5): about 217 `.look.<field>` occurrences in 20 files, about 84 whole-`look` uses (`&r->look`, struct copies) in 23 files, 29 `look.species` sites (→ `r->species`), and the identity members in 37 files, over 49 files in all (10 of them tests, plus `ps1/src/model.c` and `ps1/src/etest.c`). Writers (generator, babies, creator, save reader, `story.c:147-158`, `exorcism_scene.c:162-177`, `town.c:496-498`, `ui_gallery.c:54`) build a `DoomoCitizen` and call `res_pack()` once.

**Whole-look call sites (CF-6).** `figure_image(const DoomoLook *, …)`, `fp_host_look()`, `fp_figure()`, `draw_paw()`, `model_set_look()` and `look_hash()` take a `DoomoLook` pointer from `&r->look`. A decoded temporary has no id, so it would silently lose the citizen's own and style deep layers. Each resident call site therefore moves to an id-based call (`figure_image_res(rid, status, expr)`, `fp_host_id()`, `model_set_resident(rid)`). `figure_image_v(look, deep, ver)` is kept for anonymous looks only (the draft, the exorcism dummy, stubs). The sites are `cutscene.c:427`, `overworld_fx.c:623`, `concert_gfx.c:809`, `dream_gfx.c:819`, `lib_common.c:936`, `lib_almanac.c:546`, `creator_draw.c:1169`, `fp_common.c:247-253,702-733`, `fp_title.c:458`, `fp_island.c:864`, `fp_shop.c:289`, `exorcism_gfx.c:268-270`, `combat_hud.c:404-407`, `ps1/src/model.c:38-43` and `ps1/src/etest.c:547`.

**Roster API call sites (CF-7).** `roster_premade()` (9 callers) cannot return a pointer to a 58 B packed record. The Phase A repack converts its callers to `roster_premade_record(id, &tmp)`. `roster_bio()` and `roster_job()` keep their `const char *` signatures (13 callers in 9 teams' files) as wrappers over `roster_bio_get()`/`roster_job_get()`. Each returns a pointer into a 2-deep static ring per kind (2 × 61 + 2 × 21 B = 168 B .bss on both consoles; CF-15). On PS1 a bio may need a CD read, so the screens that show bios call `roster_text_prefetch()` on entry. That covers `fp_common.c`, `lib_almanac.c`, `exorcism.c` and S2's `%B` code in `story_talk.c`, the last by an S2 hunk.

### 1.3 The gene (381 bits used, 3 reserved)

- Each field is listed as `name@bit:width`, LSB-first. `(v1 n)` marks a width that changed from today's `rfields[]`. Every v1 value fits its v2 width.
- **List growth.** The list appends in 4.2 fit without a layout change.
- **Trims.** `like_hobby` drops from 6 to 5 bits, `like_music` from 5 to 4, and `catchphrase` from 7 to 6.
- **`gene_spare`** must be 0. A v2.1 field may claim it without a format change.

```
body@0:4(v1 5)  height@4:4  build@8:4  skin@12:6  head_shape@18:5  head_size@23:4
eye_style@27:6  eye_color@33:5  eye_size@38:4  eye_spacing@42:4  eye_height@46:4  eye_tilt@50:4
eye_count@54:3(v1 2)  brow_style@57:5  brow_height@62:4  nose_style@66:5  nose_size@71:4
mouth_style@75:5(v1 6)  mouth_size@80:4  fangs@84:2  hair_style@86:6  hair_color@92:5
hair_highlight@97:6  facial_hair@103:4  ears@107:5  horns@112:5  horn_color@117:5
horn_size@122:4  tail@126:5  wings@131:5  halo@136:4  marking@140:5(v1 6)  marking_color@145:5
cheeks@150:8  top@158:6  top_color@164:5  bottom@169:5  bottom_color@174:5  shoes@179:5
shoes_color@184:5  hat@189:6  hat_color@195:5  accessory@200:5(v1 6)  accessory_color@205:5
aura@210:5(v1 4)  pattern@215:5(v1 4)  pattern_color@220:5  voice[0..4]@225:25
persona[0..4]@250:20  pronoun@270:3  romance@273:3  bday_month@276:4  bday_day@280:5
like_food@285:6  like_hobby@291:5(v1 6)  like_music@296:4(v1 5)  like_color@300:5
hate_food@305:6  fear@311:5  catchphrase@316:6(v1 7)  look_seed@322:11  auto_style@333:2
age@335:3  pupil@338:5  sclera@343:4  fur_pattern@347:5  fur_color3@352:6  glasses@358:5
outer@363:5  legwear@368:4  blush_style@372:4  ear_fold@376:3  hood_up@379:1  handedness@380:1
gene_spare@381:3
```

Totals: legacy look 225, identity 97, new core 59, spare 3, for 384. The species byte is kept hot.

```c
/* look_fields.h (P0). Generated from core/data/look/fields.txt by tools/look_build.py
 * ("# doomo-generator") into core/gen/look_ids.h + core/gen/look_fields.c. */
typedef struct DoomoGeneField { uint16_t pos, max; uint8_t bits; } DoomoGeneField; /* x83, 6 B padded:
                                     max is u16 because look_seed's max is 2047 (CF-8) */
enum { LF_DEEP0 = 64, LF_COUNT = LF_DEEP0 + DF_COUNT };  /* 0..47 legacy, 48..61 core, 62..63 rsvd,
                                                            64..367 deep, 368..511 free */
typedef struct DoomoDeepInfo {          /* 6 B x 304 */
    uint8_t count;                      /* 2..112 choices; choice 0 draws exactly as today */
    uint8_t def;                        /* Classic choice (0 everywhere: wishlist E9)    */
    uint8_t rule, arg;                  /* DR_* (1.6) and its argument                    */
    uint8_t flags;                      /* DFF_COLOR STYLE INHERIT SYM_L SYM_R CHIBI ANIM SLIDER
                                           (all 8 bits used: a 9th flag needs a 7 B record) */
    uint8_t views;                      /* LV_BUST FIGURE CHIBI FIGHTER MODEL             */
} DoomoDeepInfo;
typedef struct { uint8_t v[DF_COUNT]; } DoomoDeep;          /* 304 B resolved choices */
DOOMO_STATIC_ASSERT(DF_COUNT == 304, deep_count);
/* generated asserts per catalog count replace save.c:57-88 (HOBBY_COUNT <= 32, ...) */
```

### 1.4 Colours: 32 paints × 3 tones, plus 15 links

Each of the 68 deep colour fields has 112 choices:

```c
#define DC_AUTO      0          /* today's derivation in pctx_init()          */
#define DC_PAINT(p)  (1 + (p))  /* 1..32                                      */
#define DC_PASTEL(p) (33 + (p)) /* 33..64  pm_from3() lifted toward white     */
#define DC_DEEP(p)   (65 + (p)) /* 65..96  pm_from3() pushed toward shadow    */
#define DC_LINK(s)   (97 + (s)) /* 97..111 follow slot s                      */
enum { CS_FUR1, CS_FUR2, CS_FUR3, CS_HAIR, CS_HILITE, CS_EYES, CS_EYE_R, CS_HORNS, CS_TOP,
       CS_BOTTOM, CS_SHOES, CS_HAT, CS_ACC, CS_OUTER, CS_LEGWEAR, CS_COUNT };   /* 15 */
```

Tones are new shade triples over existing palette entries (the palette stays fixed, `palette.h:8-16`); `doomo_tone_ok[96]` hides tones that collapse onto another paint. Links resolve one level; a cycle resolves to auto. The 11 legacy colours get tones through 11 `tone_*` deep fields, edited in the colour picker. B23 (natural fur tones) and E7 (island dyes) stay cut: the palette is full.

### 1.5 `look_pool.h`

```c
#define LOOK_POOL_BYTES 8064u            /* + 8 B header = LOOK chunk <= 8,072 B */
#define LOOK_ID_BOOK0 1001u              /* Look Book styles 1001..1016           */
#define LOOK_ID_DRAFT 1023u              /* creator scratch, never saved          */
#define LOOK_STORY_CAP 4096u             /* pool bytes usable in story mode       */
enum { RUN_DEEP, RUN_NICK, RUN_NAME };
typedef struct DoomoLookPool {
    uint16_t used, runs, gen, version;   /* gen bumps on every change             */
    uint8_t  has[128];                   /* bit per id 0..1023                    */
    uint8_t  buf[LOOK_POOL_BYTES];       /* runs sorted by (id, kind):
       u16 (id:10 | kind:2 | rsvd:4) | u16 n | n x u16 (lf << 7 | choice+1) sorted by lf;
       low 7 bits 0 = tombstone. n is u16: a full copy can need 304 entries (CF-11) */
} DoomoLookPool;                         /* 8,200 B */
DOOMO_STATIC_ASSERT(sizeof(DoomoLookPool) == 8200 && LOOK_POOL_BYTES + 8 <= DOOMO_SAVE_EXT_MAX, pool);
int  look_pool_find(int id, int kind, const uint8_t **run, int *n); /* has[] bit test first */
int  look_pool_put(int id, int kind, const void *d, int n);  /* 0 = no room; invalidates portraits */
void look_pool_drop(int id);             /* town_free(), moves */
int  look_pool_free_bytes(void);
int  look_pool_import_nick(const uint8_t *nick, uint32_t len);   /* v1 'NICK' */
void look_inherit(DoomoCitizen *baby, int a, int b, int mode);   /* 1.8 */
/* doomo_save_ext_extra becomes chunk 'LOOK' v1, max_size 8,072. NICK is import-only, inside
 * save_v1.c's own chunk loop: the pool is reset BEFORE the chunks are read, and LOOK counts
 * as seen once a NICK was imported. Otherwise read_exts() would reset() the missing LOOK
 * chunk last (save.c:997-1001, the loop at :1000) and wipe the imported nicknames (CF-13). */
```

A lookup is one `has[]` bit test, then a hop over run headers (~6 cycles each, ≤ ~500 runs). A write is one `memmove` of ≤ 8 KB (~9k cycles), only on commits, births, makeovers, copies and Look Book saves. Nicknames become `RUN_NICK` runs; `town_free()` drops a citizen's runs, so reused slots start clean (replaces `creator.c:25-34`).

### 1.6 `look_derive.h`

```c
enum { DR_DEF, DR_SPC, DR_CHN, DR_HAR, DR_PER, DR_JIT, DR_MAP, DR_COL, DR_AGE };
uint8_t  look_derive(const DeriveCtx *c, int df);        /* 20-60 cycles; integer only */
void     look_resolve(const DoomoResident *r, int id, DoomoDeep *out);
void     look_resolve_mask(const DoomoResident *r, int id, uint32_t lv, DoomoDeep *out);
void     look_resolve_draft(const DoomoCitizen *c, const uint8_t *run, int n, DoomoDeep *out);
uint32_t look_key(int id);   /* FNV-1a over the gene bytes, species, district, the bytes of
                                the citizen's own run and of its style run (if any), and
                                doomo_render_epoch: ~200 + 6/byte cycles. It is content-
                                addressed (CF-14): no global pool.gen, so one birth no longer
                                misses every citizen with a run, and a 16-bit gen wrap can
                                never revive a stale image */
/* h = doomo_hash32(look_seed*2654435761u ^ df*0x9E3779B1u ^ 0x5EEDC0DEu) */
```

| rule | n | Varied behaviour |
|---|--:|---|
| DEF | 113 | always `def` |
| SPC | 47 | species profile: a main choice plus an alternative at `alt_pct` (catfolk: slit 70 %, round 30 %). Table `DoomoSpeciesLook` 48×12×4 B |
| CHN | 46 | seeded chance with separate Hillvile and Hellvile odds, drawn from a weighted list (scars 2/10, ear piercings 8/25, eyeliner 6/18, heterochromia 3/5) |
| HAR:x | 22 | colour harmony of x: complementary, analogous or triad |
| PER:a | 22 | thresholds on persona axis a (walk from movement, laugh from speech, …) |
| JIT | 19 | `def` ± 1 |
| MAP:x | 19 | table from a core or identity field (legacy accessory → neck/back/mask; hobby → held prop). Counts per Appendix A (CF-29) |
| COL:x | 11 | follows colour x (brows = hair) |
| AGE | 4 | from `age` (baby chub, grey muzzle on elders) |

Classic returns `def` for every field; Authored reads the ROM run, then falls back to Varied.

**Derive tables are frozen once a field is live (CF-25).** Every Varied citizen in every save is a function of these tables. Changing a live row would restyle saved citizens without the player touching them. Rows of live fields are therefore append-only: a new field may add a rule, but an existing row never changes, and `derive_golden.txt` fails on any change to a live row. The only exception is a reviewed `DERIVE_V` bump, recorded in the `LOOK` chunk version, with the old row kept.

### 1.7 Other contract headers (P0 writes them in M0, before anyone else starts)

```c
/* settings.h */
enum { IC_MENU, IC_WORLD, IC_SHOOTER, IC_COUNT };
typedef struct DoomoSettings {           /* 416 B; copy in every save header + GBA journal */
    uint32_t gen; uint16_t len, crc16;   /* newest valid gen wins */
    uint8_t  v[288];                     /* one byte per global option (269 used)           */
    uint8_t  remap[3][IC_COUNT][7];      /* 3 profiles x 3 contexts x 14 nibbles            */
    uint8_t  profile;                    /* 0 = preset from OPT_CONTROL_SCHEME              */
    uint8_t  fav[16][3];                 /* creator favourites (lf, choice)                 */
    uint8_t  seen[8];                    /* tutorial + seen-cutscene bits                   */
} DoomoSettings;
DOOMO_STATIC_ASSERT(sizeof(DoomoSettings) == 416, settings_size);
int settings_load_best(void); int settings_commit(void);
void input_context(int ic);   /* (CF-19) declared here by P0 in M0, so P4 and the scenes
                                 can call it before P5 owns input.c; weak stub until P5.M1 */

/* opt_ids.h: core/gen/, by tools/opt_build.py from core/data/options/options.txt.
 * Ids 0..57 keep today's values; new ids append from 58 in table order; the first
 * "EXTENDS(OPT_X)" row keeps id X, later rows that split X get new ids. */
typedef struct DoomoOptMeta { uint16_t store; uint8_t count, def, page, flags; } DoomoOptMeta;
/* (CF-44) renamed: town.h already defines DoomoOptionInfo {name, count, def, values, page},
 * which fp_options.c, fp_title.c, ui_gallery.c and combat_scene.c read. doomo_opt_meta[] is
 * resident; the legacy name/value table moves to options_text.c behind opt_name()/opt_value_name(). */
/* (CF-8) store is u16: the global store has 269 used slots of 288, beyond a u8's 255. 6 B x 458 */
/* flags: OF_GLOBAL OF_GBA OF_PS1 OF_CHEAT OF_RESET_EXEMPT OF_LIVE OF_PENDING */

/* opt_rules.h: sink structs. P0 lands them with today's constants and converts the
 * sim/combat constants to reads in Phase A; P5's opt_rules.c rebuilds them on opt_set(). */
typedef struct SimRules    { uint8_t decay[STAT_COUNT], floor, problem_pct, autonomy, crush_pct,
    confess_pct, baby_pct, possess_pct, possess_cap, corrupt_mode, income_q4, price_q4,
    ageing, nobody_dies, diff; } SimRules;
/* CombatRules, XhStyle, HudStyle, FxRules, DlgStyle, InputTiming, PetRules: same pattern, one
 * uint8_t per knob listed in the section 5 sink table; defaults = today's constants. */

/* roster.h v2 */
typedef struct DoomoPremadeRec { uint8_t gene[48], species, stat7[7], flags, mood_act; } DoomoPremadeRec; /* 58 B */
void roster_premade_record(int id, DoomoResident *out);
int  roster_bio_get(int id, char *buf, int cap);   int roster_job_get(int id, char *buf, int cap);
int  roster_job_venue(int id);                     void roster_text_prefetch(int id);
int  roster_deep_run(int id, const uint16_t **entries);
const uint8_t *roster_text_pack(int k, uint32_t *len);   /* weak; PS1 override in rostxt.c */

/* portrait_int.h additions (P1 implements the core, P2 the body) */
#define DL_STEPS 7     /* 0 back, 1 body, 2a head+eyes, 2b face decals, 3 front hair/ears, 4 hat/horns/acc, 5 finish */
enum { LOD_TINY = 1, LOD_MID = 2, LOD_BIG = 4 };
/* PCtx gains: const DoomoDeep *d; uint8_t lod, view; PMemo memo[16]; */
const PMat *pm_of(PCtx *c, int lf);       /* tones, links, auto */
void pc_stamp(PCtx *c, const uint8_t *m1bpp, int w, int h, int x, int y, int q4, const PMat *m);
void pc_region(PCtx *c, int region);      /* decal clip: head, eye L/R, muzzle, torso ... */
int  ps_run(PCtx *c, int list, int value, int anchor);   /* part-script VM */
/* parts.h: PartHdr {u8 group, anchor, lod_views, len} + 4..8 B ops: ANCHOR ELLIPSE CAPSULE
 * POLY ARC LINE DOT STAMP MIRROR END MAT SHADE PART DECAL IF IFVIEW IFYAW IFANIM PAT SCALE FX */

/* motion.h (P3) */
typedef struct MotionPose { int8_t dx, dy, lean, arm_l, arm_r, leg_l, leg_r, head, tail, ears; } MotionPose;
void motion_pose(int kind, int style, int phase, MotionPose *out);

/* save.h v2 */
#define DOOMO_SAVE_VERSION 2
#define DOOMO_SAVE_SLOT0_MAX (36u * 1024u)   /* 4 KB goes to the GBA settings journal; I9's
                                                SAVE_SLOT_STORY_CAP (save_budget.h) must follow
                                                (I9 hunk, CF-17) */
enum { SAVE_ERR_V1 = -20 };
uint32_t save_bound(int slot);  int save_bound_fits(int slot, int32_t delta);
uint32_t save_citizen_bound_bits(int id, const DoomoCitizen *c);
```

P0 also adds weak stubs for every new function in `core/src/stubs.c`, so the tree links at every step.

### 1.8 Babies, age, copies, job venues

- **Babies.** `look_inherit()` builds the baby's look:
  - **Core fields** come from parent A or B by `pick8()` (or by the mode of option RO18): species, skin, head, eyes, nose, hair colours, ears, horns, tail, wings, halo, marking, `pupil`, `sclera`, `fur_pattern`, `fur_color3` and `ear_fold`.
  - **Forced values:** the existing baby rules, plus `age = AGE_BABY`, Varied mode, and `seed = (a*31 + b + baby_seed) & 2047`.
  - **Deep fields:** only the explicit `DFF_INHERIT` values are copied, at most 8 entries (20 B). Nothing explicit is inherited when the pool is ≥ 75 % full.
- **Age gate** (fixed, not an option):
  - `sim_traits()` adds the KID bit for baby and kid, and the TEEN bit for teen, **for every citizen**.
  - `AGE_AUTO` means the roster traits for an unedited premade, and adult otherwise.
  - `sim_romance_ok()` already checks KID and teen parity on both sides.
  - The creator offers romance only for teens and older.
  - The v1 migration sets `AGE_BABY` on the baby signature (`romance NONE && BODY_TINY && TOP_PAJAMAS`). **(CF-16)** Every other migrated custom, edited or player citizen with `body == BODY_TINY` gets `AGE_KID`. That is the safe default: v1 had no age for hand-made kids, so without it they would migrate as adults and keep today's gap. The player can raise the age in the creator.
  - `gen_core2()` sets `age = AGE_KID` when `body == BODY_TINY`, and adult or elder from the persona otherwise. This gives random islands the gate too (`test_romance_age_gate` covers "random-island kids").
  - `sim_traits()` today returns 0 for every non-premade island (`sim.c:122`). v2 applies the age bits **before** that early return.
- **Ageing (LS11)** needs no extra counter. On `bday_day` (Normal) or on `bday_month`/`bday_day` (Slow), a citizen steps one age (baby → kid → teen → adult) and its portraits are invalidated. The step writes `age`, which is a sim-driven change to a gene field. **(CF-12)** `age` is therefore serialised as a **hot** field (the 16th slot of schema groups 0-1, 3 bits at full width). That keeps 3.2's "the sim cannot move `save_bound`" true.
- **Copy look** copies the 62 core fields and materialises the source's resolved deep values that differ from the target's derivation. The target becomes Varied, and the copy goes through the budget (3.2).
- **The sim never decodes a look.** It reads `species`, `RES_AGE()` and `RES_LOOK(halo / accessory)` (the holy bonus).
- **Job venues.** `roster_job_venue(id)` covers unedited premades. Everyone else uses deep `job` (DF 290) through `doomo_job_venue_of[64]`, and only during work hours. Babies and kids never get a venue (today `sim_social.c:280` sets 0xFF for babies). `sim_s.job[1000]` is deleted.
  - The test build keeps the old scanner, and `test_roster_job_venue` checks that it agrees with the table.
  - **(CF-20) Warm-up parity.** Today `job_venue()` (`sim.c:297-307`) returns "no venue" for an uncached citizen once `sim_s.job_budget` is spent, so venues appear over the first ticks after each load. An instant table changes `decide_activity()` and every sim golden. M0 keeps the budget semantics with a 125 B `job_known[1000/8]` bitset (net −875 B, not −1,000). M0b (below) may drop it when it re-baselines the sim goldens.

## 2. Memory math

### 2.1 GBA EWRAM

| line | Δ B |
|---|--:|
| `res[1000]` + player, 96 → 76 B | −20,020 |
| creator `nicks[64]` + flag, folded into the pool | −1,025 |
| `sim_s.job[1000]` removed (venue table in ROM), less the 125 B warm-up bitset (CF-20) | −875 |
| `doomo_look_pool` | +8,200 |
| `doomo_settings` | +416 |
| `town.opt` 64 → 192 B, `unlocks2[64]`, progress 32 → 48 counters | +256 |
| portrait `Job`: look 48 → 64, `DoomoDeep` 304, `PCtx` +176 | +496 |
| `roster_bio()`/`roster_job()` result rings (CF-7) | +168 |
| sink structs (`SimRules` … `PetRules`, 1.7), resident `.bss` (estimate) | +200 |
| save bound sums | +8 |
| **net** | **−12,176** |

- **Free EWRAM.** base + 12,176: at the `8cc7910` base, 7,104 + 12,176 = **19,280 B**. After `HOME` (3,432 B) it is base + 8,744 (**15,848 B**), and about 15.3 KB once S1, S3 and H2 land. These figures move with G0 (CF-1).
- **Not counted above** (each under 100 B): `DoomoLook` 48 → 64 in `portrait.c`'s static `def` copies and in `exorcism_scene.c`'s `X.dummy`; on PS1 also `model.c`'s `s_default_look`.
- **Outside EWRAM.**
  - The creator state lives in the 12,288 B scene scratch (6.6).
  - Part scripts, tables and strings are in ROM.
  - `palfx` writes palette RAM at vblank.
- **IWRAM: +0 B static** (a CI gate). New code is THUMB in ROM and draws through the existing `P_FAST` loops. The stack grows about +700 B in the render path: from the 1,744 B high-water mark to about 2,450 B, out of 6,320 B.
- **ROM: +180 KB** in total, against C21's 190 KB share.

  | package | ROM |
  |---|--:|
  | P0 | −33 KB (packed roster −26.6, Huffman texts −21.6) |
  | P1 | +64 KB (face and head parts, 54 KB raw) |
  | P2 | +46 KB (body and outfit parts, 38 KB raw) |
  | P3 | +5 KB |
  | P4 | +58 KB (code, names, presets) |
  | P5 | +30 KB |
  | P6 | +10 KB |

  If it runs over, the lever is 6 B part-script ops (−23 KB).

### 2.2 PS1 resident ledger (code and const are RAM)

The base is 110,624 B.

| pkg | line | Δ B |
|--|--|--:|
| P0 | res 96 -> 76 B | -20,020 |
| P0 | CrNick into the pool | -1,025 |
| P0 | roster 96,000 -> 58 B records + names 11,430 | -26,570 |
| P0 | bios Huffman, resident (~46.9 KB -> 28,177) | -18,756 |
| P0 | jobs Huffman (8,056 -> 5,058 + decoder) | -2,260 |
| P0 | sim_s.job -> venue table (rodata) + 125 B warm-up bitset (CF-20) | +125 |
| P0 | doomo_look_pool | +8,200 |
| P0 | settings, opt, unlocks2, progress | +672 |
| P0 | portrait Job | +496 |
| P0 | roster bio/job result rings (CF-7) | +168 |
| P0 | bound sums | +8 |
| P0 | code: registry, codec, resolver, derive | +9,216 |
| P0 | code: pool, LOOK, NICK import | +2,560 |
| P0 | code: save v2 + bound (v1 reader in freeplay overlay) | +3,072 |
| P0 | const: registry (83 × 6 B gene fields + 304 × 6 B deep info; CF-8) | +2,322 |
| P0 | const: option metadata (458 × 6 B; CF-8) | +2,748 |
| P0 | const: initial derive tables | +1,024 |
| **P0** | subtotal | **-38,020** |
| P1 | interpreter, stamps, 8 KB part LRU | +14,336 |
| P1 | face/head painters | +7,168 |
| P1 | face/head parts, LZ (54 KB raw) | +27,648 |
| **P1** | subtotal | **+49,152** |
| P2 | body/fur/garment painters | +5,120 |
| P2 | body/outfit parts, LZ (38 KB raw) | +19,456 |
| **P2** | subtotal | **+24,576** |
| P3 | bio packs -> ROSTXT.BIN (+4,096 buffers, +664 reader) | -23,417 |
| P3 | edges -> EDGES.BIN | -9,058 |
| P3 | SFX PCM -> SPU RAM (+2,880 wave table, +1,500 loader) | -43,413 |
| P3 | model.c recipes + head impostor | +8,192 |
| P3 | motion tables + fighter LOD | +2,048 |
| **P3** | subtotal | **-65,648** |
| P4 | creator: overlay group only | 0 |
| **P4** | subtotal | **+0** |
| P5 | sinks, palfx, buses, remap (text in overlay) | +13,312 |
| **P5** | subtotal | **+13,312** |
| P6 | roster deep layer | +4,096 |
| P6 | full derive tables | +2,560 |
| P6 | appended catalog names | +3,072 |
| **P6** | subtotal | **+9,728** |
| | **net** | **-6,900** |

- **End state.** base + 6,900. At the `8cc7910` base that is **117,524 B** below the stack top: 43,796 B above C5 and 51,988 B above `elf2exe`'s floor (`0x200000 − 0x10000`, `elf2exe.py:86`).
- **The story's own C5 lines** (≈ +4 to +11 KB net, plus the `HOME` .bss of 3.4 KB) come out of the same pool, and I9 owns the joint total. Expect about 103-110 KB at the end with both, still above C5.
- **Merge order** (enforced in 7.2). P0 alone leaves base + 38,020; P0 + P3.M1 leaves base + 113,908. **P1.M2 and P2.M2 merge only after P3.M1.**
- **Generated files are resident on PS1 unless routed (CF-18).** `ps1/CMakeLists.txt:40` globs `core/gen/*.c` into the EXE, and `ovl_plan.group_of()` returns resident for anything outside `/core/src/` (`ovl_plan.py:116-129`). So `core/gen/creator_rows.c`, any generated option or creator text, and the raw `core/gen/parts_*.c` (92 KB) would all link resident. This ledger assumes three hunks:
  - an H3 hunk in `group_of()` that maps `/core/gen/creator_rows` to `creator`;
  - an S2 hunk in `ps1/CMakeLists.txt` that excludes the raw `parts_*.c` and links the LZ blocks instead (the `packart.py` pattern at `CMakeLists.txt:73-88`);
  - the same S2 hunk drops `sfx_pcm.c`.
- **Task #79** (≥ 128 KB below the stack top) has three levers: portrait cache 48 → 36 blocks (−4,608 B), Huffman catalog names (≈ −5 KB) and 6 B ops (≈ −12 KB). Together they reach about 140 KB, and P3 decides at its M3 whether to pull them.

**Overlay groups** (`ovl_plan.py`):
All four are hunks **requested from H3**, which keeps `GROUPS` (CF-18).
- **creator:** about 50 KB now (`ovl_plan --report`), ≤ 104,448 B after. The regex becomes `creator(_\w+)?$`, plus `group_of()` routing for `/core/gen/creator_rows`. Requested by P4.
- **options:** a new group `("options", r"fp_options$|options_text$")`, ≤ 51,200 B. Requested by P5. `GROUPS` is first-match, and `fp_options` matches `freeplay` today (`ovl_plan.py:56`), so the same hunk removes `options` from the freeplay regex.
- **freeplay:** gains `save_v1` (+3 KB; the regex must list it, or it stays resident) and loses `fp_options`.
- **Option names outside the options group.** `fp_title.c:487` (freeplay) and `ui_gallery.c:141,248` (gallery) read `doomo_option_info[].values/name`. If those strings move into the options overlay, the reads cross groups: garbage on PS1, or `ovl_plan` pins the whole text resident. Both files keep local copies, as the combat pause menu does, through freeplay and gallery hunks (CF-26).
- **combat:** C4 allows net growth ≤ +1,936 B for the **whole group**, and S3's `combat_story.c` spends from the same slack. 150 options are consumed in combat-group files (121 of them in wave 1), so +1,936 B cannot hold their HUD, crosshair, camera and input code. **(CF-24)** P5 must deliver a combat-group ledger before P5.M1, with offsets:
  - crosshair, HUD and damage-number styles as compact data tables read by one existing function;
  - C4's option (a), moving `levels_select$` out of the group;
  - if still over, C4's option (b), an I9 decision on a larger slot paid from C5.

  The rules themselves live in the resident `opt_rules.c`. The custom share of the slack is ≤ +1,000 B unless I9 decides otherwise.

## 3. Saves, migration, determinism

### 3.1 Format v2

**Header (480 B).** Bytes 0..47 keep their v1 meaning, with `version = 2`. New fields:

| offset | field |
|---:|---|
| 48 | `u32 settings_gen` |
| 52 | `u16 settings_len` |
| 54 | `u16 look_used` (save-list meter) |
| 56 | `u16 flags` (`SVF_SETTINGS_PER_ISLAND`, `SVF_GBA_JOURNAL`) |
| 58 | 6 reserved bytes |
| 64 | `DoomoSettings` (uncompressed, inside the CRC) |

**Body.** Same LZ and CRC as v1, in this order:

1. `opt[192]`
2. `unlocks[128]` + `unlocks2[64]`
3. `progress[48]`
4. residents
5. chunks: `LOOK` v1 in the `extra` slot; `NICK` is import-only

**Resident schema v2.** It is generated from the registry: 99 fields in 13 groups of 8 (5 slots spare).

| groups | contents | bits |
|---|---|---|
| 0-1 | the 16 sim-mutable fields: stat ×8 (7 bits each), status 3, flags 8, exorcism 8, level 7, mood 4, activity 4, death 3, **age 3** (CF-12) | 96 |
| 2-12 | name (4 + 7×len), species 6, and the other 81 gene fields at gene width | - |

- **Tags** are unchanged: empty run, equal to base, delta (13-bit group mask, then 8-bit field masks), and full.
- **Full** records are `u1 core2_present` + every non-core2 field + core2 if present. Core2 is now 56 bits, because `age` is hot. A migrated custom citizen therefore costs 508 bits (v1: 511).
- **Empty islands** delta against `resident_default()` with the district. This is new in v2: v1 rejects tags 1 and 2 on empty islands (`save.c:906,915`), and `save_v1.c` keeps that rule.
- **Write cost.** `write_town()` calls `resident_sanitize()` per citizen, on both streaming passes. `res_pack()` already sanitises at every commit, so v2's writer skips the decode/re-encode sanitize and only clamps hot fields. Otherwise a GBA save gains about 0.4 s (CF-30).

**On load,** `look_pool_sanitize()`:

1. drops runs with id > 1016 or kind > 2, and out-of-range entries;
2. re-sorts the runs and rebuilds `has[]`;
3. invalidates all portraits.

A bad chunk is reset, and its citizens fall back to core + derived.

### 3.2 Slot 1: a static bound the sim cannot move

```
save_bound(1) = 480 + ceil((TOWN_FIXED_MAX + 9,984 EDGES_MAX + ceil(R_bits/8)
                + sum(10 + max_or_ext(c)) over chunks c other than HOME and LOOK
                + 10 + home.size() + 10 + look.size()) * 129/128)
max_or_ext(c) = c->max_size ? c->max_size : DOOMO_SAVE_EXT_MAX       /* (CF-9) */
TOWN_FIXED_MAX = 1,459 at the v2 sizes (CF-10), computed by a counting pass over
                 write_town()'s fixed part and asserted in test_save, never typed in
R_bits = sum over ids of cb(id);   cb(empty) = 18
cb(id) = min(2 + 1 + 96 + name_bits + 6 + 322 + (core2 differs ? 56 : 0),      /* full  */
             2 + 13 + 8*g(id) + bits(id) + 16 + 96)                           /* delta */
```

`g(id)` and `bits(id)` count the non-hot groups and the field bits that differ from the base. `R_bits` is accumulated inside `read_town()`'s applying pass, where each base is already at hand. A separate pass would cost about 1 s on the GBA for a random island, because every base is a `resident_generate()` call.

**Why this is a proof.** The bound fixes every part the sim can touch at its maximum:
- hot fields at full width (including `age`, which LS11 ageing changes; CF-12);
- edges, items and graves at their maxima;
- every chunk at `max_size`, except `HOME` and `LOOK`, which are budgeted themselves. A chunk that declares no `max_size` counts as `DOOMO_SAVE_EXT_MAX` (CF-9). Today that is `SONG`, `FPLY` and `FISH`.

So the sim cannot change `save_bound`, and the file never exceeds it (LZ worst case 129/128). `save.c` keeps `R_bits` as a running sum, updated by commits and recomputed on load.

**Growth paths.** Each of these calls `save_bound_fits(1, Δ)`. It requires `save_bound + ⌈Δ×129/128⌉ ≤ cap(1) − 2,048`, where `cap(1)` is the platform's real slot capacity, not a constant: 90,112 on a 128 KB flash or a memory card, but about 24 KB on the 64 KB MX29L512/MN63F805 chips that `save_flash.c:54-55` accepts (CF-27). The paths are:
- creator commit, makeover and copy;
- baby, IS08 auto-fill;
- Look Book save, pool put;
- `HOME` placement, gift and photo;
- **any other write of a record into a slot**, such as a move across the border (IS02) or a new runtime citizen source. All of them go through one `town_put(id, const DoomoCitizen *)`. `test_save` greps that no other code assigns `doomo_town.res[...]` (CF-28).

**When it refuses:**
- the creator offers Smart pack, Keep core only or Back;
- a birth waits a day ("the stork is stuck in traffic");
- IS08 stops;
- the house editor shows the shared meter.

**I9 hunk** (`save_budget.c`): `save_budget_left(1) = cap(1) − 2,048 − save_bound(1)`. Slot 0 keeps the R13 formula, with `SAVE_SLOT_STORY_CAP` lowered to 36,864 (CF-17).

**Capacity.** Recomputed in `scratchpad/custom/critique/budget_fix.py` with `TOWN_FIXED_MAX` 1,459, hot 96 bits, and the real chunk maxima: `SONG` 5,408 (`song_book.c` `encode()` worst case), `FPLY` 270 (`FPX_SIZE`), `FISH` ≤ 182 (`FCX_SIZE`), `STRY` 96. With those, Σcb + `LOOK` + `HOME` has **69,446 B** of room. **This holds only once the SONG, FPLY and FISH owners declare those `max_size` values (R14).** P0.M1's proof depends on those three hunks. Until they land, `max_or_ext()` counts each undeclared chunk as 8,192 B and the room drops to about 50.7 KB (CF-9).

| island | bound | result |
|---|---|---|
| premade, fully lived in | 34,109 B | 54 KB left for makeovers |
| typical hand-made citizen | 481 bits each | 1,120 fit with an empty house, 1,063 with a full one: **all 1000 always fit** |
| every field at maximum (adversarial) | 70.9 B each | 965 fit |

**Tests.** `test_save` asserts:
- bound ≥ encoded size over 10^4 random islands;
- the bound is unchanged after 30 simulated days;
- I9's `save_joint_worst_case`.

### 3.3 v1 saves: lossless and proven to fit

`save_v1.c` freezes today's `rfields[]` (`save.c:513-545`), the body order and `base_of`. The v1 base is `res_to_v1(v2 base)`.

| v1 data | v2 result |
|---|---|
| town scalars, items, weapons, pet, story, graves, edges | copied |
| `opt[64]` | island rules go to `town.opt`. Globals go to `doomo_settings` only if `settings_gen == 0`, so a v1 save never overwrites v2 settings |
| `unlocks`, `progress` | copied; bank 2 and the new counters start at 0 |
| resident whose look equals the base | new core taken from the v2 base: premades become Authored, random citizens Varied |
| resident that was edited, empty island, or the player | `look_seed = 0`, Classic, new core 0: **pixel-identical**; except `age`, which is `AGE_KID` for `BODY_TINY` (CF-16) |
| `NICK` | `look_pool_import_nick()` (only entries whose name hash still matches), run by `save_v1.c`'s own chunk loop after a pool reset, never by `read_exts()` (CF-13) |
| `SONG`, `FPLY`, `FISH`, `HOME`, `STRY` | unchanged |

- **Worst case.** The worst v1 island has 1001 fully custom citizens, 2,048 edges and every chunk at its real maximum (CF-10; `HOME` counted at 3,432 even though v1 has no house). It migrates within **86,635 B**, with `save_bound` the same. If a migrated island's bound ever exceeds `cap(1) − 2,048` (a 64 KB flash, or undeclared chunks), it still loads and saves as today, because `save_game()` checks the real size. Only the growth paths refuse until the bound drops.
- **PS1 placement.** `save_v1.c` lives in the freeplay overlay (C6; the freeplay regex must list `save_v1`, CF-18). Resident `save_load()` returns `SAVE_ERR_V1`. Every caller that can meet a v1 file then calls `save_v1_load()` from freeplay-group code: `fp_title.c` (continue and load) and `fp_saveload.c`, by freeplay hunks (CF-26). The harness `coresave` scripts load v2 only.
- **Rewrite.** The next save writes v2. A v1 file is never rewritten behind the player's back.

### 3.4 Global settings persistence

1. **Every save header carries a copy.** The newest `gen` across both slots wins.
2. **GBA journal.**
   - Slot 0 shrinks to 36 KB (`SLOT0_LEN 0x09000`, trailer at `0x08FF4`).
   - Sector `0x09000-0x09FFF` becomes an append-only journal of 448 B records ("DMST", gen, len, CRC-32, block), 9 records per erase.
   - **Migration**, run once if the v1 trailer at `0x09FF4` is valid and the story save is ≤ 32 KB:
     1. Erase `0x08000`.
     2. Program an identical trailer at `0x08FF4` and verify it.
     3. Only then erase `0x09000`.

     No data byte moves. A power cut at any step leaves at least one valid trailer. Saves larger than 32 KB keep the v1 layout.
   - **Story slot (dynamic R13 budget):** 13,504 + 3,432 + 96 + 4,096 + 480 = 21,608 B ≤ 36,864 B. Today's clean "too big, old save kept" path stays.
   - **The journal fills.** Every tenth commit erases the sector (about 25-100 ms). A power cut during that erase loses only the journal: the save headers still hold the settings as of the last save. This is documented, not hidden.
3. **PS1 (CF-17).** There is **no** extra settings frame. `DoomoSettings` is 416 B and a frame is 128 B, so "`HDR_FRAMES` 5 → 6" could not hold it, and moving the payload one frame later would break v1 files. The settings are part of the v2 header (payload bytes 64..479), which lands in block 0 that `memcard.c` reads whole anyway. `HDR_FRAMES` stays 5, and v1 files load unchanged. There is no SY06 option either (the SY category has five). On PS1, leaving the options screen keeps the change in RAM and marks it dirty; it is written with the next save (manual or autosave). A memory card write is whole 8 KB blocks, about 1 s each (`memcard.c:17,272`), far too slow for every options exit.

### 3.5 Roster v2 and determinism

**Roster tool.** `tools/roster_build.py` v2 emits:

| output | what | goes to |
|---|---|---|
| `doomo_roster_pack[1000]` | 58 B records, plus the name pool and offsets | all builds |
| `doomo_roster_job_venue[1000]` | job venues | all builds |
| jobs | Huffman-coded | all builds |
| bios | 32 canonical-Huffman packs (73 symbols, max code length 15, 146 B table) | `roster_text.c` on GBA and native, `ROSTXT.BIN` on PS1 |
| edges | | `EDGES.BIN` |
| `doomo_roster_deep[]` | ≤ 4,096 B | all builds |

It checks that every premade is ALIVE with level, exorcism and death all 0.

**Frozen goldens,** captured from the post-G0 merge base before P0.M0 (not from `8cc7910`; CF-1):

| golden | covers | rule |
|---|---|---|
| `roster_v1.txt` | FNV of each premade's 96 B v1 view | frozen |
| `fingerprints.txt` | 96 seeds over `res_to_v1()` | never changes |
| `portrait_classic_golden.txt` | 1000 busts, 1000 chibis, 96 figures, 96 fighters | byte-identical under Classic |

**Generator.** `resident_generate()` runs `gen_v1()` (today's draws, order unchanged), then `gen_core2()` on `rng(seed ^ 0xC0DE5EED)`, then `res_pack()`.

**New goldens:**

- `fingerprints_v2.txt`: the 76 B record plus `look_resolve()`, for 96 seeds × 2 generators.
- `derive_golden.txt`: 48 species × 16 seeds × 2 districts. P6 owns its updates.

**Seed search** (P6). `core/tools/look_seed_search.c` is built from the same `look_derive.c` as the game.

- For each premade it keeps the smallest seed in 0..2047 that maximises the number of authored deep keys matching their derived values. Only the leftovers become ROM entries.
- Without authored keys, the seed is `fnv1a(name) & 2047`.
- `check-gen` proves the output is reproducible.

**Sim goldens (CF-20).** The age gate and the job-venue table change sim behaviour on purpose: adults can no longer crush on babies, and kids get kid hours. So the "byte-identical" rule cannot cover sim-derived goldens in the same commit. M0 is split:
- **M0a** is the pure refactor (record, codec, repack, roster v2, the warm-up bitset). Every golden stays byte-identical, sim goldens included.
- **M0b** is the age gate. It re-baselines exactly the defaults golden, `test_sim`'s fingerprints and any story fingerprint that runs the sim, with a reviewed diff showing that only age-gated events changed. S2 and I9 are notified per story-SPEC C18.

The defaults golden hashes a canonical v1 view: `res_to_v1()` of every resident plus today's 64 option bytes and v1 town fields. A raw FNV over `DoomoTown` changes with any layout change (`opt[192]`, `res` at 76 B) and could never stay identical.

**Sinks after load (CF-22).** `opt_rules.c` also rebuilds the sinks in `town_new()`, after `save_deserialize()`, in `opt_defaults()` and after a settings load. Otherwise the sim could tick under the previous island's rules until the next scene entry.

**Parity.** All code is integer-only over `uint8_t`. Gene access is byte-wise (CF-2). `xcheck.sh` builds all three toolchains at `-Werror`. On hardware, `lookfp <n>` (H2/H3 `hcmd.c` hunks, plus the native runner) prints the v2 goldens, and CI diffs the GBA and PS1 output against native.

## 4. Feature list: face, body and furry, clothing layers, motion and personality

**View codes:**

| code | view |
|---|---|
| P | bust 48×56 |
| F | figure 48×80 |
| C | chibi 16×24 (crowds of 40) |
| T | fighter 32×48: the GBA third person and the creator turntable |
| M | PS1 3D model |

Every head, face, hair, hat, horn and ear field also appears on the PS1 model through the head impostor (4.6).

**Art.** Everything follows the "Art direction" section of ARCHITECTURE.md: premium, cute, lively plush critter-folk. Each new list ships a contact sheet at 1×, 2× and chibi scale (`core/tools/portrait_sheet.c`) and passes an art review before it goes live.

### 4.1 The 14 new core fields (inline, never lost to a full pool)

| field | bits | values | wish | render (group) | views |
|---|--:|---|---|---|---|
| `look_seed` | 11 | 0..2047 | - | drives every Varied value; "Reroll details" | all |
| `auto_style` | 2 | Classic, Varied, Authored | E9 | resolution mode | all |
| `age` | 3 | auto, baby, kid, teen, adult, elder | B86 | head ratio and eye/lid presets per age, never body shaping; **romance gate** | P F C T M |
| `pupil` | 5 | auto + 31: round, slit, goat bar, cross, star, heart, paw, skull, diamond, spiral, flower, moon, ring, keyhole, X… | F08 | 8×8 stamps per eye (2a) | P F T M |
| `sclera` | 4 | auto, cream, black demon, goat yellow, void, glowing, red, mint, starry, inverted… (16) | F15 | sclera stage of `one_eye()` (2a) | P F T M |
| `fur_pattern` | 5 | none, the 7 legacy fur marks, tuxedo, calico, tortie, tiger, dalmatian, merle, socks, points, saddle, belly patch… (32) | B26 | region-masked fills in fur 2/3 (1-2a) | P F C T M |
| `fur_color3` | 6 | auto + 32 paints (+ `tone_fur3`) | B25 | `CS_FUR3` material | P F C T M |
| `glasses` | 5 | none + 31: round, cat-eye, heart, aviator, 3D, monocle, goggles, visor, eyepatch L/R, star… | F95 | scripts on `PA_HEAD_FRONT` (4) | P F C T M |
| `outer` | 5 | none + 31: open jacket, puffer, haori, lab coat, raincoat, cape-coat, cardigan, leather, varsity… | O04 | second torso shell with open front and lapels (1) | P F C T M |
| `legwear` | 4 | none + 15: tights, fishnets, striped thigh-highs, leg warmers, garters, bandages… | O07 | painted before the bottoms (1) | F C T M |
| `blush_style` | 4 | ovals, circles, ///, hearts, stars, spirals, swirls, sparkles… (16) | F58 | `MIRROR` scripts (2b) | P F C T M |
| `ear_fold` | 3 | none, tip, half lop, full lop, L down, R down, both down, crumpled | E04 | `one_ear()` hinge (0/3) | P F C T M |
| `hood_up` | 1 | off, on | O50 | head shell + hood-ear script (3/4) | P F C T M |
| `handedness` | 1 | right, left | A19 | mirrored arm and weapon side (FP3 too) | F C T M |

### 4.2 Append-only catalog growth (+300 values, 0 RAM)

P0 appends every enum value and name in M0, with the live counts still at v1. Each owner flips its lists live once the painters exist, a premade wears each value (P6) and the art review passes.

| list | today → v2 | w | owner | list | today → v2 | w | owner |
|---|---|--:|---|---|---|--:|---|
| F01 head shapes | 12 → 24 | 5 | P1 | B52 tails | 16 → 32 | 5 | P2 |
| F07 eye styles | 24 → 40 | 6 | P1 | B59 wings | 10 → 20 | 5 | P2 |
| F25 eye count | 1-3 → 1-6 | 3 | P1 | B66 horns | 16 → 26 | 5 | P1 |
| F29 brows | 12 → 24 | 5 | P1 | B76 halos | 6 → 12 | 4 | P1 |
| F35 noses | 12 → 24 | 5 | P1 | B79 auras | 8 → 20 | 5 | P1 |
| F47 mouths | 20 → 32 | 5 | P1 | B87 species (wave 4) | 24 → 48 | 6 | P2+P6 |
| H01 hair | 32 → 64 | 6 | P1 | O03 tops | 24 → 50 | 6 | P2 |
| H13 facial hair | 8 → 16 | 4 | P1 | O06 bottoms | 12 → 24 | 5 | P2 |
| E01 ears | 16 → 32 | 5 | P1 | O12 shoes | 12 → 24 | 5 | P2 |
| B01 bodies | 10 → 16 | 4 | P2 | O25 hats | 24 → 48 | 6 | P1 |
| O45 fabric patterns | 8 → 32 | 5 | P2 | A28 expressions | 11 → 23 | - | P1 |

- **Presets and accessories.**
  - O62 outfit presets grow from 20 to 60, and 32 of them double as situational sets.
  - O41, the legacy accessory slot, stays. The creator offers to move its glasses, mask, neck or back item into the new dedicated slot.
- **Derived at 0 storage:** O63 job uniforms, O64 dirt from hygiene, A37 idle prop habit, A38 motion personalities.

### 4.3 The 304 deep fields

The table below lists every field, grouped by wishlist section. **Appendix A** (`SPEC_appendix_fields.md`, normative) gives each field's choices `n`, its Varied rule, its views (P F C T M), its chibi flag and its full value list.

- **Colour fields:** 68 of them, with 112 choices each (1.4).
- **Sliders** keep today's position as choice 0.
- **Owners:** P1 = face and head-top, P2 = body and outfit, P3 = motion, P5 = voice and sounds.

| wish § | section | DF | n | owner | waves | fields |
|--|--|--|--:|--|--|--|
| 3.1 | Head structure | 0..4 | 5 | P1 | w3:5 | head_width, cheek_fullness, chin, feature_height, feature_scale |
| 3.2 | Eyes: shape, iris, pupil… | 5..23 | 19 | P1 | w2:6 w3:13 | pupil_size, iris, iris_size, eye_color2, hetero, eye_color_r, eye_shine, eye_glow, lashes, lashes_low, lash_color, lid_droop, eye_line, eye_bags, eye_asym, extra_eye_place, gaze, special_eye, special_eye_side |
| 3.3 | Brows | 24..28 | 5 | P1 | w2:2 w3:3 | brow_color, brow_thick, brow_angle, brow_spacing, brow_length |
| 3.4 | Nose, muzzle and whiskers | 29..39 | 11 | P1 | w2:3 w3:8 | nose_color, nose_shine, nose_height, muzzle, muzzle_len, muzzle_puff, muzzle_color, whisker_pads, whiskers, whisker_len, whisker_color |
| 3.5 | Mouth, lips, teeth and to… | 40..49 | 10 | P1 | w2:2 w3:8 | mouth_w, mouth_y, mouth_curve, lip_color, lip_style, teeth, fang_place, teeth_color, tongue, tongue_color |
| 3.6 | Cheeks and skin details | 50..68 | 19 | P1 | w2:1 w3:18 | blush_color, blush_amt, blush_pos, blush_size, freckle_pat, freckle_amt, freckle_color, mole_pos, mole_count, scar1, scar2, seams, seam_color, bandage, bandage_color, cheek_fluff, face_gem, face_gem_color, face_age |
| 3.7 | Makeup | 69..75 | 7 | P1 | w3:7 | eyeshadow, eyeshadow_color, eyeliner, eyeliner_color, highlight, nails, nail_color |
| 3.8 | Face paint and face tatto… | 76..81 | 6 | P1 | w3:6 | face_paint, paint_color1, paint_color2, paint_finish, face_tattoo, face_tattoo_color |
| 3.9 | Piercings | 82..88 | 7 | P1 | w3:7 | ear_pierce, ear_pierce_side, face_pierce_a, face_pierce_b, tongue_stud, horn_rings, pierce_metal |
| 3.10 | Glasses (their own slot) | 89..91 | 3 | P1 | w3:3 | frame_color, lens_tint, glasses_pos |
| 3.11 | Hair and facial hair | 92..103 | 12 | P1 | w3:12 | bangs, ahoge, hair_volume, hair_length, hair_mirror, dye, hair_color3, hair_shine, hair_acc, hair_acc_color, hair_acc_side, beard_color |
| 3.12 | Ears | 104..107 | 4 | P1 | w2:2 w3:2 | ear_size, ear_angle, ear_inner_color, ear_tip |
| 4.1 | Proportions (stylised plu… | 108..123 | 16 | P2 | w2:4 w3:12 | shoulders, torso, waist, hips, belly, arm_len, arm_thick, leg_len, leg_thick, neck_len, head_ratio, posture, hand_size, foot_size, muscle, roundness |
| 4.2 | Fur, skin and material | 124..150 | 27 | P2 | w2:2 w3:25 | material, fluff, fur_len, sheen, translucency, fur_color2, pattern_scale, pattern_where, belly_color, pad_color, pad_shape, claws, claw_color, chest_fluff, chest_fluff_color, mane, grey_muzzle, glow, glow_color, glow_pulse, tattoo1, tattoo1_where, tattoo2, tattoo2_where, ink_color, body_scar, plush_build |
| 4.3 | Hands, feet and legs | 151..156 | 6 | P2 | w2:3 w4:3 | hands, feet, legs, mobility, prosthetic, prosthetic_side |
| 4.4 | Tails | 157..162 | 6 | P2 | w2:3 w3:3 | tail_count, tail_len, tail_fluff, tail_tip_color, tail_tip, tail_pose |
| 4.5 | Wings | 163..168 | 6 | P2 | w2:1 w3:5 | wing_size, wing_color, wing_color2, wing_pattern, wing_pose, wing_pairs |
| 4.6 | Horns, antennae and head… | 169..178 | 10 | P1 | w3:10 | horn_count, horn_curl, horn_tex, horn_tip_color, horn_gradient, horn_asym, horn_pos, antennae, antenna_color, head_extra |
| 4.7 | Halo and aura | 179..183 | 5 | P1 | w3:5 | halo_color, halo_tilt, halo_size, aura_color, aura_amt |
| 4.8 | Extra anatomy, age and sp… | 184..188 | 5 | P2 | w4:5 | extra_arms, body_eyes, belly_mouth, hybrid, hybrid_blend |
| 5.1 | Slots | 189..222 | 34 | P2 | w2:3 w3:31 | inner_top, inner_color, outer_color, legwear_color, legwear_stripe, socks, sock_color, shoe_color2, gloves, glove_color, belt, belt_color, neck, neck_color, back, back_color, mask, mask_color, mask_pos, hat_tilt, hat_size, hat_ears, hat_color2, ear_acc, ear_acc_color, tail_acc, tail_acc_color, horn_acc, horn_acc_color, wing_acc, wrist, wrist_color, prop, prop_color |
| 5.2 | Per-piece modifiers | 223..248 | 26 | P2 | w2:1 w3:25 | top_color2, bottom_color2, shoes_color2, outer_color2, pattern_on, pat_outer, pat_outer_color, pat_bottom, pat_bottom_color, pat_legwear, pat_legwear_color, pat_hat, pat_hat_color, emblem, emblem_color, sleeves, neckline, hood_ears, top_len, tucked, outer_open, bottom_len, cuffs, wear, fabric, fit |
| 5.3 | Wardrobe and situational… | 249..253 | 5 | P2 | w4:5 | outfit_sleep, outfit_swim, outfit_formal, outfit_battle, outfit_rules |
| 6 | Motion and personality | 254..282 | 29 | P3 | w2:3 w3:18 w4:8 | idle, fidget, walk, run, stride, sit, sleep, dance, emote, greet, laugh, cry, angry, fluster, victory, taunt, hurt, ko, blink, blink_rate, ear_expr, wag, wag_amt, breath, bounce, rest_expr, talk, gaze_habit, mood_motion |
| 6 | Voice, sounds and job | 283..290 | 8 | P5 | w3:7 w4:1 | steps, babble, voice_flavour, sing_range, vibrato, quirk, loudness, job |
| T08 | Look Book style | 291..291 | 1 | P2 | w4:1 | style |
| T05 | Tones of legacy colours | 292..302 | 11 | P1 | w2:11 | tone_skin, tone_hair, tone_eyes, tone_marking, tone_horn, tone_top, tone_bottom, tone_shoes, tone_hat, tone_fur3, tone_acc |
| - | reserved | 303 | 1 | - | - | deep_spare |

**Totals:**

- 304 fields: 291 fields from 268 distinct wishlist items (14 items split into 2 or 3 fields, e.g. F28, B43, O42), `style`, 11 tone fields and 1 spare (CF-29).
- By wave: 47 in wave 2, 233 in wave 3, 23 in wave 4.
- 91 of them are in the chibi mask.
- Stored densely they would be about 147 B per citizen (147 KB). The sparse layer costs **0 B** for every untouched citizen.

**Cut:**

- **B23** (natural fur tones): the palette is full, and the tones replace them.
- **B84** (buddy head): it doubles the head cost and passes the 253 part-id ceiling. B75 head extras cover the gag.
- **E7** (island dyes): the palette is full. Rainbow and glow finishes are 4-phase re-renders of existing paints, shown on busts and figures only. DS25 and AC17 freeze them.

**Motion and personality** (deep fields 254-290):

| group | fields |
|---|---|
| body language (18) | idle, fidget, walk, run, stride, sit, sleep pose, dance, emote, greeting, laugh, cry, angry, flustered, victory, taunt, hurt, faint |
| face and tail motion (11) | blink style and rate, ear expressiveness, wag style and amount, breathing, bounce, resting expression, talk style, gaze habit, mood motion |
| voice (7) | footsteps, babble wave, species flavour, singing range, vibrato, quirk sound, loudness. These sit on top of the 5 voice sliders |

- **How motion works.** Pose-offset tables (`motion.h`, P3) run on the existing 4-frame cycles, so there is no new frame art.
- **Derived defaults.** PER rules derive defaults from the persona, so a shy citizen walks shyly with nothing stored.
- **Presets.** 8 motion personalities: Shy bean, Hyper pup, Royal, Goth gremlin, Sleepy blob, Tough cookie, Ghosty, Robot.
- **Test buttons** (T19) play walk, run, idle, dance, emote, laugh, sit, sleep and voice.

### 4.4 Rendering pipeline (P1 owns `portrait.c`)

1. **Resolve once per job.**
   - On a cache miss, each image accessor calls `look_resolve_mask(r, id, LV_kind, &job.D)`.
   - `pctx_init()` stores the look and the deep set; painters read `c->d->v[DF_x]`.
2. **Materials.** `pm_of(c, lf)` builds tone and link materials on demand, with a 16-entry memo. Choice 0 keeps today's derivation (`portrait.c:55-101`).
3. **`DL_STEPS` goes from 6 to 7.**
   - Group 2 splits into 2a (head base, muzzle, eyes) and 2b (brows, nose, whiskers, mouth, decals, makeup, piercings).
   - No slice grows. `PC_AHEAD` goes from 8 to 9.
4. **Part scripts.**
   - Pipeline: `core/data/parts/<dir>/*.txt` → `tools/parts_build.py` → `core/gen/parts_<dir>.c`, run by the VM in `portrait_script.c`.
   - Ops are 4-8 B. Anchors come from `PHead` and the body layout, so sliders move parts, and today's primitives keep outlines identical.
   - Stamps are 1bpp, 8×8 to 16×16. Decals take no part ids.
   - The GBA reads scripts raw from ROM. The PS1 stores 2 KB LZ blocks behind an 8 KB LRU.
   - Any part over 600 px at bust scale is rejected at build time.
5. **Per-op LOD:** chibi, fighter, and bust/figure.
6. **Occlusion first:** coats, legwear, gloves, hoods and masks skip the spans they cover.
7. **Animation flags.** `DFF_ANIM` builds `look_anim_mask()`, which replaces the hand-written lists in `phase_of()` and `anim_phase()`.

| family | owner | hook | group | chibi | PS1 model |
|---|---|---|--:|---|---|
| head structure, face age | P1 | `phead_set`, `ph_head_base`, `ph_eye_geom` | 2a | shape | impostor |
| eyes: pupil, iris, sclera, hetero, shine, glow, lashes, lids, liner, bags, asymmetry, gaze, special and extra eyes | P1 | `one_eye()` split into stages, each taking a script or stamp | 2a | glow 1 px | impostor |
| brows, nose, muzzle, whiskers, mouth, lips, teeth, tongue | P1 | `brows()` / `nose()` / `mouth()` params, plus a new `muzzle()` | 2a/2b | muzzle colour | impostor |
| blush, freckles, moles, scars, seams, bandages, gems, makeup, face paint, tattoos, piercings | P1 | decal passes (`markings()` → passes) | 2b | blush 1 px | impostor |
| glasses, hair (+32, bangs, ahoge, dye, accessories), ears, horns, antennae, head extras, halo, aura | P1 | `ph_face_acc()`, `hair_*_shapes()`, `one_ear()`, `one_horn()`, `ph_halo()`, `pf_aura_*` | 0/3/4/5 | silhouette, colours | impostor |
| proportions, posture | P2 | `pb_layout()` Q4 maths, `body_mod[]` | 1 | build, legs | box scales |
| material, fluff, sheen, translucency, fur colours and pattern, belly, pads, claws, mane, glow marks, body tattoos | P2 | `pctx_init()` shade modes (tufts, 8×8 tiles, slime dither, metal band), region masks | 1/2a/5 | 2 colours, tint | vertex colours, semi-trans |
| hands, feet, legs, mobility aid, prosthetic | P2 | `legs()`, `foot()`, arm painters + scripts | 1 | feet, legs | digitigrade bend, hoof/talon ends |
| tails (≤ 9), wings, extra anatomy | P2 | `pb_draw_tail()` angle fan, `wing_one()` | 0/1 | ≤ 3 tails, colour | fan of blobs, wing quads |
| outfit slots, colour 2, trims, patterns, emblem, sleeves, neckline, hood, lengths, tuck, cuffs, wear, fabric, fit, situational outfits | P2 | `pb_outer()`, `pb_legwear()`, `pb_gloves()`, `pb_belt()`, `pb_back()`, `p_pattern_tiles` per piece | 1/3/4 | outer, legwear, socks, hood, back, prop | piece colours, coat box, back quad |
| motion | P3 | `motion_pose()` in `pb_layout()`, `bodyspr_render`, `fighter_render`, `model.c` | anim | walk/run/sit/sleep | pose tables |
| voice and sounds | P5 | babble (S1 hunk), `audio_fx.c`, footsteps from the feet field | - | - | - |

**The chibi mask.**

- Only the 91 `DFF_CHIBI` fields plus the 62 core fields resolve for chibis. The chibi key hashes the gene **plus the run entries whose field is `DFF_CHIBI`** (own and style runs, filtered while hashing). A gene-only key would leave a chibi stale when a chibi-visible deep field changes, for example `outer_color` or `socks` (CF-45). A lipstick change still re-renders 0 chibis, because `portrait_invalidate(id)` clears an image only when its key changes.
- Chibi worst case: ≤ 360k cycles (338k today).

### 4.5 Cache keys and performance

**Cache keys.**

- **Resident images** key on `(kind, rid, status, expr, phase, look_key(rid))`. That is about 200 cycles plus 6 per run byte (typical runs are 20-60 B). It reads the same bytes today's `look_hash` reads, plus the citizen's own and style runs (content-addressed, CF-14). No pointer lifetime, and no per-citizen version array.
- **The draft** uses `figure_image_v(look, deep, cr.draft_ver)`.
- **Invalidation.** `portrait_invalidate(id)` runs on every pool write that touches `id`, and on commit, birth and load. A style change invalidates all its wearers; the key covers the style run, so a missed call can only cost a stale-free re-render. In test builds, every cache hit recomputes the key and asserts that it matches.

**GBA budget** (16.78 MHz; a tick is 561,792 cycles):

| image | today avg / worst | v2 typical | maxed gate | slice gate |
|---|---|--:|--:|--:|
| bust | 763k / 1.20M | ≈ 790k | ≤ 1.45M | ≤ 300k |
| figure | 915k / 1.38M | ≈ 950k | ≤ 1.65M | ≤ 300k |
| chibi | 250k / 338k | ≈ 255k | ≤ 360k | unsliced |
| fighter | 454k / 655k | ≈ 470k | ≤ 720k | as today |
| `look_resolve` (all / chibi) | - | 12k / 3k | ≤ 15k | - |

- **Pixel visits:** < 14,000 (12,000 today).
- **Part ids:** ≤ 240.
- **Scenes this spec does not own** lose ≤ 0.5 fps to portrait work.

### 4.6 Third person

- **GBA (P3).** The fighter billboard is procedural (`render_fighter_ctx()`, `portrait.c:306`, and `fighter_image()`/`fighter_render()`, `portrait.c:840-929`; CF-23), so every T field already shows. The held weapon is `portrait_gun.c`, which P3 now owns.
  - P0 moves `render_fighter_ctx` into `portrait_fighter.c`.
  - P3 adds fighter LOD, `motion_pose()` run styles (gallop, float, roll, hop) and the left-handed weapon mirror.
  - P3 implements `fighter_image_ex(…, FI_DITHER | FI_HIDE)` in its own `portrait_fighter.c`, declared by P0 in M0. P5 only calls it for TP08 (CF-23).
- **PS1 head impostor (P3).** This shows every head option in 3D with no per-feature 3D code.
  - `CK_HEAD` is a 32×32 image (3 pool blocks) from `headspr_render(L, D, yaw8)`: the bust's head painters at fighter LOD.
  - `model_player_draw()` drops the blob head, face boxes, hair, ears, horns and halo, and draws one `POLY_FT4` at 2× from `vram_image()` instead.
  - Two yaw buckets are cached (~5 ms per view, sliced). The blob head is the fallback while one renders.
- **PS1 body recipes (`model.c`, ~8 KB).**
  - Geometry: limb box scales, digitigrade bend, hoof, talon and paw ends, tail fan, wings.
  - Colour: fur 2/3 vertex colours, coat box, legwear and sock bands, back quad, a prop outside combat.
  - Materials: slime and ghost semi-transparency, metal highlight, fluff pyramids.
  - Motion: swing tables, and the gun arm mirrored for left-handers.
  - Budget: ≤ 120 primitives and ≤ 400 triangles, with a merged LOD beyond 6 cells (≤ 1.5 ms).
  - Other PS1 citizens are chibi billboards.

## 5. Options: 458, each wired

### 5.1 Storage and ids

- **API.** `opt_get()` and `opt_set()` stay as they are (`options.c:120-134`). The 58 existing ids keep their values, and new ids append from 58 (1.7).
- **Asserts.** `OPT_COUNT <= 64` becomes two store asserts: global ≤ 288, island ≤ 192.

| scope | count | covers | stored in | saved |
|---|---:|---|---|---|
| **G** global | 269 | audio, display, controls, accessibility, text, HUD, camera, creator | `doomo_settings.v[288]` | every save header + the GBA journal (3.4) |
| **I** island | 189 | sim, romance, souls, pet, gore, modifiers, cheats, per-mode tweaks | `doomo_town.opt[192]` | the town body |

- **SY01 "Settings follow each island"** copies the global block into the island save.
- **Text off the resident path.** Names, value names and help text live in `options_text.c`, in the PS1 options overlay. The combat pause menu keeps its own copy of its 7 option names.

### 5.2 Sinks: one mechanism per family

`opt_rules.c` (resident, P5) rebuilds the 1.7 structs on each `opt_set()` and on scene entry. P0 lands the structs in Phase A with today's constants as defaults. The defaults golden (7 sim days, then a town FNV) must not change.

| sink | read by |
|---|---|
| `sim_rules` | `sim.c` `apply_needs()` `decide_activity()` `day_rollover()`, `sim_social.c` `romance_step()` `sim_confess()` `sim_make_baby()`, `sim_soul.c` `rate_factor()` `sim_possession_cap()` `grieve()`, `sim_care.c`, `sim_events.c` |
| `combat_rules` | `combat.c` `read_controls()` `ease_turn()` `find_target()` `combat_hurt_player()` `combat_on_kill()`, `actor.c` `actor_spawn_demon()` |
| `hud_style`, `xh_style` | `combat_hud.c` `crosshair()` `crosshair_color()` `lock_reticle()` `scope_overlay()` `health_panel()` `ammo_panel()` `damage_numbers()` `weapon_view()` `boss_bar()` |
| `fx_rules` | `actor.c` `actor_gore()` `actor_gib()`, `mg_fx.c`, `audio_fx.c` |
| **`palfx`** | `palfx_build()`: a 3×3 Q8 colour matrix (daltonise, sepia, noir, thermal), brightness/contrast/saturation/gamma, 1-bit. **(CF-31)** `plat_pal_lerp(pal_id, level16)` only lerps toward four fixed palettes (`platform.h:205-207`), so palfx needs a new `plat_pal_base(const uint16_t rgb555[256])` hook in S1's `gba/src/postfx.c` and `ps1/src/postfx.c` (S1 hunks). On PS1 a CLUT reaches only CLUT-textured prims, not the r3d world or the gouraud `model.c`, so 3D coverage needs a platform hunk in `r3d.c`/`view3d.c`; until then the PS1 3D values are `OF_PENDING`. LCD grid and ghosting are not palette operations and go through `postfx.c` (S1). **12 options, 3 of them dead today** |
| `audio_bus_gain()` | `audio_bus.c`, called from `audio.c` `send()` (audio owner hunk), `ps1/src/spu.c` |
| `input_timing` + remap | `input.c` `input_update()`, `btn_repeat()` (fixed at 12/3 ticks today), `btn_held_ticks()` |
| `dlg_style` | `ui_dialog.c` `dlg_update()` `dlg_box()` `babble()` `copy_str()` (S1 hunks) |
| `pet_rules` | `pet.c` `pet_tick_minute()` `evolve()` `pass_away()` (S3 hunk) |
| render cheats | `look_resolve()` view override + `doomo_render_epoch` |

**The 8 dead options get consumers:**

| option | consumer |
|---|---|
| LCD filter, colour-blind aid, camera filter | `palfx` |
| subtitles | S1 hunk |
| big head | `look_resolve` |
| brutal residents | `fx_rules` |
| paper mode | material override |
| all ghosts | ghost recolour in the render epoch |

**Every option must do something:**

- **`tools/check_options.py`** (CI) fails any `OPT_*` id that has no `opt_get()`, sink or rules-table read outside `options*.c`, `opt_rules.c` and `fp_options.c`. A mention in an unlock **table** (`UL_CAT_OPTION` rows) is not a read. A read in `unlocks.c` code does count: GM11's toast is in `unlock_grant()`, and the earlier blanket `unlocks*.c` exclusion would have failed it in CI (CF-26). Ids that wait on another team's hunk are listed in `core/data/options/pending_hunks.txt` and flagged `OF_PENDING`, which hides them in the UI.
- **`test_options_effect`** runs every option at every unlocked value and requires a **different probe hash** each time. The probes are SIM (3 seeded days), CMB (300 bot ticks), HUD/UI pixels, AUD log, PAL, POR (8 busts and figures), INP, TXT, MG, PET, STORY, SAVE and PLAT. It also checks that every option has help text, a valid default and a scope, and that island options never move a global probe.

### 5.3 The options screen and remapping

**Screen** (`fp_options.c`): 10 categories and 31 pages, ≤ 24 rows per page.

| category | pages |
|---|---|
| Game | Game · Story · System |
| Island | Island rules · Life sim · Needs |
| Hearts | Romance · Possession and death |
| Pet & modes | Pet · Per-mode tweaks · Photo |
| Shooter | Movement · FP camera · TP camera · Aim · Weapons |
| HUD | Crosshair · HUD |
| Gore & challenge | Gore · Modifiers · Cheats ×2 |
| Audio | ×2 |
| Display | ×2 · Colour · Text |
| Access & controls | Accessibility ×2 · Controls · GBA/PS1 · Remap · Creator |

- **Navigation:** L/R changes page; SELECT+L/R (L2/R2) changes category; tapping SELECT jumps by letter.
- Rows for the other console are hidden.
- Locked values show "Next: ??? (how)".
- **Presets:** Comfort, Classic DOOM, Cozy life, Hardcore, Max accessibility, Default. Monster Mode is exempt.
- **Every page** has help text and ends with "Reset this page".
- **Leaving the screen** with global changes calls `settings_commit()`. On GBA that is a journal append (no erase on 9 of 10 commits, ~10 ms). On PS1 it marks the settings dirty; they are written with the next save (CF-17).

**Remapping** is a bijective permutation per context:

- Scenes call `input_context(IC_MENU|IC_WORLD|IC_SHOOTER)`.
- `input_update()` maps the 14 physical bits through `remap[profile][ctx]` before edge detection. On the GBA this happens after the SELECT+A/B → X/Y synthesis.
- Scenes keep testing `BTN_*`. Assigning a taken button swaps the two, so conflicts cannot exist.
- **Presets:** Classic, Modern, Southpaw, Legacy, One-hand L/R, and PS1 L2/R2 variants.
- **Hard-wired, ahead of the permutation:** START+SELECT for 3 s restores the preset, and soft reset always works. Confirm and back may swap (AC12) but never unbind.

### 5.4 The list

**458 options are IN.**

- **284** of them have their consumer in customisation-owned code.
- **174** need one marked hunk from another team (summary at the end of this section).
- **Appendix B** (`SPEC_appendix_options.md`, normative) lists all 458 with category, values and default, scope, wave and exact consumer.

The **160 headline rows** below cover every page, all 44 extended legacy ids, and up to 5 options per page.

**11 options are cut:** ST08 (the follower), LS03, RO20 (merged into LS11), PT11, AU21, DS12, DS13, GB05, PS06, PS08 and PS09. P5 records the reasons in `core/data/options/cut.txt`.

**Legend:**

- **S:** G = global, I = island.
- **W:** wave.
- **(x):** extends an existing `OPT_*` id; Appendix B names which one. 44 legacy ids gain values, and their legacy bytes keep their meaning; 14 stay unchanged.
- **(h):** the consumer is in another team's file; the option stays `OF_PENDING` until that hunk lands.

Categories (ID prefix, options in the category): GM Game (17); ST Story mode (10); IS Island rules (15); LS Life sim (19); ND Needs (12); RO Romance, family and babies (23); PX Possession, exorcism and death (17); PT Doomogachi pet (11); GR Gore and horror (18); MV Shooter: movement (12); FP Shooter: first-person camera (12); TP Shooter: third-person camera (13); AI Shooter: aim and lock-on (17); XH Crosshair (15); HD HUD (20); WP Weapons and welding (10); AU Audio (23); DS Display, filters, LCD and CRT (26); CB Colour and colour-blind support (8); AC Accessibility (30); TX Text and dialogue (17); CT Controls: common (10); GB Controls: GBA (5); PS Controls: PS1 (6); RM Button remapping (5); CH Cheats (32); MD Modifiers (15); PM Per-mode tweaks (22); CR Creator preferences (9); PH Photo mode (4); SY System (5).

| ID | option | values (*default) | S | W | wires into |
|--|--|--|--|--|--|
| GM01 | Combat difficulty (x) | Plushie, Normal*, Nightma… | I | 1 | combat.c combat_hurt_player() |
| GM02 | Life-sim difficulty (x) | Plushie, Normal*, Nightma… | I | 1 | sim.c apply_needs() |
| GM03 | Pet difficulty (x) | Plushie, Normal*, Nightma… | I | 1 | pet.c pet_tick_minute() (h) |
| GM04 | Puzzle difficulty (exorci… | Plushie, Normal*, Nightma… | I | 1 | exorcism.c exorcism_chance() (h) |
| GM05 | Autosave (x) | Off, Every day*, Every ho… | G | 1 | fp_common.c (h) |
| GM17 | Monster Mode (x) | Off*, ON (monster!), Ask… | I | 1 | sim_soul.c |
| ST01 | Story pacing | Relaxed, Normal*, Intense | I | 2 | story.c story_pacing_scale() (h) |
| ST02 | Invasion encounters | Story beats only, Low, No… | I | 2 | story.c story_encounter_rate() (h) |
| ST03 | Quest markers | Off, Subtle*, Full arrows | G | 2 | overworld.c (h) |
| ST04 | Seen cutscenes | Ask*, Always skip, Never… | G | 2 | cutscene.c (h) |
| ST05 | Hold to skip dialogue | Off, On* | G | 1 | ui_dialog.c dlg_update() (h) |
| IS01 | The Wall | Up*, Gate open, Down | I | 3 | fp_island.c peek_wall() (h) |
| IS02 | Moving across the border | Story-gated*, Allowed, Ne… | I | 1 | town.c town_alloc() |
| IS03 | Starting money (new islan… | 0, 500*, 5000, 50000 | I | 1 | town.c town_new() |
| IS13 | News ticker content (x) | All*, Favourites only, Bi… | I | 1 | fp_common.c (h) |
| IS15 | Weather (x) | Off, Normal*, Lots, + Hel… | I | 1 | fp_island_gfx.c (h) |
| LS01 | Sim speed (x) | Pause, Chill, Normal*, Fa… | I | 1 | sim.c speed_q() |
| LS02 | Day length (x) | Real time, 2 h, 1 h*, 45… | I | 1 | sim.c ticks_per_minute_q() |
| LS04 | Autonomy (citizens solve… | Low, Normal*, High | I | 2 | sim.c decide_activity() |
| LS05 | Problem frequency | Rare, Normal*, Often | I | 1 | sim_events.c |
| LS06 | Problems expire after | Never*, 1 day, 3 days | I | 1 | sim_events.c day_rollover() |
| ND01 | Hunger decay | Off, Slow, Normal*, Fast… | I | 1 | sim.c apply_needs() |
| ND02 | Happiness decay | Off, Slow, Normal*, Fast… | I | 1 | sim.c apply_needs() |
| ND03 | Hygiene decay | Off, Slow, Normal*, Fast… | I | 1 | sim.c apply_needs() |
| ND04 | Energy decay | Off, Slow, Normal*, Fast… | I | 1 | sim.c apply_needs() |
| ND05 | Health decay / sickness | Off, Slow, Normal*, Fast… | I | 1 | sim.c apply_needs() |
| RO01 | Romance | On*, Crushes only, Off | I | 1 | sim.c sim_romance_ok() |
| RO02 | Crush rate | Rare, Normal*, Often | I | 1 | sim_social.c romance_step() |
| RO03 | Confession luck | Harsh, Normal*, Kind | I | 1 | sim_social.c sim_confess() |
| RO04 | Weight of your advice | Low, Normal*, High | I | 1 | sim_social.c |
| RO05 | Cross-border romance | On*, After the Wall falls… | I | 1 | sim_social.c cross_border_pct() |
| PX01 | Possessions (x) | Off, Rare, Normal*, Frequ… | I | 1 | sim_soul.c rate_factor() |
| PX02 | Max possessed at once | 1, 3*, 5, 10, Unlimited | I | 1 | sim_soul.c sim_possession_cap() |
| PX03 | Corruption spreads to fri… | Off, On*, Viral | I | 1 | sim_soul.c sim_corrupt() |
| PX04 | Vape-shop corruption | Off, On* | I | 1 | sim.c story_vape_open() |
| PX05 | Nightmares | Off, Rare, Normal* | I | 1 | dream.c dream_plan() (h) |
| PT01 | Pet clock | Real time, Game time*, On… | I | 1 | pet.c pet_tick_minute() (h) |
| PT02 | Pet hunger speed | Slow, Normal*, Fast | I | 1 | pet.c (h) |
| PT03 | Poop frequency | Low, Normal*, High | I | 1 | pet.c (h) |
| PT04 | Pet sickness | Off, On* | I | 1 | pet.c (h) |
| PT10 | Pet alerts (x) | Off, Badge only, Toast, T… | G | 1 | pet_scene.c (h) |
| GR01 | Gore mode (x) | Cute*, Brutal, Per weapon… | I | 1 | actor.c |
| GR02 | Blood colour (x) | Red*, Green, Rainbow, Con… | I | 1 | actor.c |
| GR03 | Confetti (x) | None, Some, Lots*, Absurd | I | 1 | actor.c |
| GR04 | Gibs | None, Few, Normal*, Lots | I | 1 | actor.c actor_gib() |
| GR17 | Demon designs (x) | By chapter*, Always cute… | I | 1 | actor.c |
| MV01 | Auto-sprint | Off*, On, After 1 s | G | 1 | combat.c read_controls() |
| MV02 | Sprint | Hold*, Toggle | G | 1 | combat.c read_controls() |
| MV03 | Crouch | Hold, Toggle* | G | 1 | combat.c read_controls() |
| MV04 | Dodge roll input | Double-tap + button*, But… | G | 1 | combat.c read_controls() |
| MV05 | Jump | Button*, Auto-jump ledges | G | 1 | combat.c |
| FP01 | Field of view (x) | 60, 70, 75*, 80, 90, 100… | G | 1 | combat_hud.c focal_px() |
| FP02 | Weapon bob | Off, Low, Full* | G | 1 | combat_hud.c weapon_view() |
| FP03 | Weapon position | Right*, Centre (classic)… | G | 2 | combat_hud.c weapon_view() |
| FP04 | Weapon size | Small, Normal*, Big | G | 2 | combat_hud.c weapon_view() |
| FP12 | Head bob (x) | Off, Low, Full*, Classic… | G | 1 | combat.c |
| TP01 | Distance | Close, Normal*, Far, Very… | G | 1 | combat.c |
| TP02 | Height | Low, Normal*, High, Top-d… | G | 1 | combat.c |
| TP03 | Shoulder offset | Centred, Small, Normal*… | G | 1 | combat.c |
| TP04 | Shoulder (x) | Right*, Left, Auto (swaps… | G | 2 | combat.c |
| TP13 | Default camera per mode… | First / Third each | G | 1 | combat.c combat_start_at() |
| AI01 | Horizontal look speed (x) | 1..10 (5*) | G | 1 | combat.c ease_turn() |
| AI02 | Vertical look speed (x) | 1..10 (5*) | G | 1 | combat.c |
| AI03 | Invert X (x) | Off*, On | G | 1 | combat.c |
| AI04 | Invert Y (first person)… | Off*, On | G | 1 | combat.c read_controls() |
| AI05 | Invert Y (third person)… | Off*, On | G | 1 | combat.c |
| AI08 | Aim slowdown (friction)… | Off, Low, Medium*, High | G | 1 | combat.c find_target() |
| AI09 | Aim magnetism (pull) (x) | Off, Low*, Medium, High | G | 1 | combat.c find_target() |
| AI10 | Vertical auto-aim (classi… | Off, On* | G | 1 | combat.c |
| AI11 | Lock-on (x) | Off, Hold, Toggle* | G | 1 | combat.c |
| XH01 | Shape (x) | the 8 today + chevron, T… | G | 1 | combat_hud.c crosshair() |
| XH02 | Colour (x) | the 6 today → 32 paints +… | G | 1 | combat_hud.c crosshair_color() |
| XH03 | Size | Small, Normal*, Big | G | 1 | combat_hud.c crosshair() |
| XH04 | Thickness | 1 px*, 2 px | G | 1 | combat_hud.c crosshair() |
| XH05 | Gap | 0..3 (1*) | G | 1 | combat_hud.c crosshair() |
| HD01 | Layout (x) | Cute*, Classic, Minimal… | G | 1 | combat_hud.c |
| HD02 | Scale | Small, Normal*, Large (PS… | G | 2 | combat_hud.c |
| HD03 | Opacity | 100%*, 75%, 50% | G | 1 | combat_hud.c panel() |
| HD11 | Damage numbers (x) | Off, Numbers*, Hearts, St… | G | 1 | combat_hud.c damage_numbers() |
| HD12 | Hit marker (x) | Off, Visual, Visual + sou… | G | 1 | combat_hud.c |
| WP01 | Auto-switch on pickup | Off, If better*, Always | G | 1 | combat.c combat_select_weapon() |
| WP02 | Auto-reload | Off, On* | G | 1 | combat.c |
| WP03 | Skip empty weapons | Off, On* | G | 1 | combat.c combat_next_weapon() |
| WP04 | Weapon order | By slot*, By class, Favou… | G | 1 | combat.c combat_next_weapon() |
| WP05 | Welding minigame | Easy, Normal*, Hard, Auto… | I | 1 | weld_game.c step_score() (h) |
| AU01 | Master volume | 0..10 (10*) | G | 1 | audio.c send() (h) |
| AU02 | UI sound volume | 0..10 | G | 1 | audio_bus.c audio_bus_gain() |
| AU03 | Combat sound volume | 0..10 | G | 1 | audio_bus.c audio_bus_gain() |
| AU04 | Ambience volume | 0..10 | G | 1 | audio_bus.c audio_bus_gain() |
| AU16 | Babble style (x) | Off, Bleeps*, Syllables… | G | 1 | ui_dialog.c babble() (h) |
| DS01 | LCD filter (x) | Off, Pet screen*, Everyth… | G | 1 | palfx.c palfx_build() |
| DS07 | Camera filter (x) | the 6 today + Polaroid, P… | G | 1 | palfx.c palfx_build() |
| DS21 | Window skin (x) | the 12 today + Moss, Bone… | G | 1 | ui_draw.c ui_style() (h) |
| DS26 | Title theme (x) | the 6 today + Plush Parad… | G | 1 | fp_title.c (h) |
| DS27 | Cursor (x) | the 6 today + Bone, Candy… | G | 1 | ui.c (h) |
| DS28 | Font style (x) | Normal, Bold, Shadowed +… | G | 2 | font.c (h) |
| CB01 | Colour-blind aid (x) | Off*, Protan, Deutan, Tri… | G | 1 | palfx.c palfx_build() |
| CB02 | Method | Remap palette (daltonise)… | G | 1 | palfx.c palfx_build() |
| CB03 | Strength | 0..3 (2*) | G | 1 | palfx.c palfx_build() |
| CB04 | District colour coding | Pink/purple*, Blue/orange… | G | 1 | ui.c district badges (h) |
| CB05 | Enemy outline highlight | Off*, On | G | 2 | combat.c |
| AC01 | Text size | Normal*, Large (2x dialog… | G | 2 | ui.c big_glyphs() (h: foundation UI; `big_glyphs` is `ui.c:325`, not `ui_dialog.c`, CF-26) |
| AC02 | Readable font (heavy-bott… | Off*, On | G | 2 | font.c (h) |
| AC16 | Reduce motion (idle anima… | Off*, On | G | 1 | tween.c (h) |
| AC17 | Photosensitive mode (no g… | Off*, On | G | 1 | look_derive.c look_resolve() |
| AC30 | Screen shake (x) | Off, Low, Full*, Classic | G | 1 | camfx.c shake_pct() (h) |
| TX01 | Auto-advance | Off*, On | G | 1 | ui_dialog.c dlg_update() (h) |
| TX02 | Auto-advance delay | Short, Normal*, Long | G | 1 | ui_dialog.c (h) |
| TX03 | Skip seen text | Off*, On | G | 1 | ui_dialog.c (h) |
| TX16 | Text speed (x) | Typewriter-slow, Slow, No… | G | 1 | ui_dialog.c (h) |
| TX17 | Subtitles (x) | Off, On*, With speaker na… | G | 1 | cutscene.c jump-scare beats (h) |
| CT01 | Scheme (x) | Classic, Modern*, Southpa… | G | 1 | combat.c read_controls() |
| CT02 | Strafe mode | L/R strafe*, Hold L + D-p… | G | 1 | combat.c |
| CT03 | D-pad turn acceleration | Off, Low*, High | G | 1 | combat.c ease_turn() |
| CT04 | Look up/down with | L + D-pad*, R + D-pad, Au… | G | 1 | combat.c |
| CT05 | Camera toggle | SELECT*, Chord, Disabled | G | 1 | combat.c combat_toggle_camera() |
| GB01 | Sleep mode (stop CPU and… | Off, L+R+SELECT*, L+R+STA… | G | 2 | main.c (h) |
| GB02 | X / Y in menus | SELECT chords*, L/R act a… | G | 1 | input.c |
| GB03 | Swap L and R | Off*, On | G | 1 | main.c (h) |
| GB04 | L shift layer (L + face b… | Off*, On | G | 2 | combat.c |
| GB06 | Save reminders (no clock… | Off, On* | G | 1 | fp/overworld reminder toast e… (h) |
| PS01 | Vibration (DualShock) | Off, Low, High* | G | 2 | pad.c |
| PS02 | Analog sticks | Off*, Move + look, Swapped | G | 2 | pad.c input_set_analog() |
| PS03 | Stick deadzone | 0..5 (2*) | G | 1 | input.c input_analog() |
| PS04 | Stick curve | Linear, Exponential* | G | 1 | combat.c stick_curve() |
| PS05 | L2 / R2 | Look up/down*, Lock-on +… | G | 1 | PS1 L2/R2 action table (h) |
| RM01 | Shooter actions (fire, AD… | any button (4 bits each… | G | 2 | combat.c read_controls() |
| RM02 | Overworld actions (intera… | any button (about 4 B) | G | 2 | overworld.c press_a() (h) |
| RM03 | Menu actions (confirm, ba… | any button (about 3 B) | G | 2 | input.c |
| RM04 | Remap profiles | 3 custom per platform + R… | G | 2 | input.c remap tables |
| RM05 | Conflict warnings | Off, On* | G | 2 | options remap screen conflict… (h) |
| CH01 | Infinite ammo | Off*, On | I | 1 | combat.c |
| CH02 | No reload | Off*, On | I | 1 | combat.c |
| CH08 | Tiny heads (x) | Off*, On | I | 1 | portrait.c |
| CH25 | Free shop (x) | Off*, On | I | 1 | fp_shop.c (h) |
| CH28 | Plush mode (every materia… | Off*, On | I | 1 | look_derive.c look_resolve() |
| MD01 | Damage taken | x0.5, x1*, x2, x4 | I | 1 | combat.c read_controls() |
| MD02 | Demon HP | x0.5, x1*, x2 | I | 1 | actor.c |
| MD03 | Ammo pickups | x0.5, x1*, x2 | I | 1 | combat.c |
| MD04 | Iron Plush (one life per… | Off*, On | I | 3 | story save: death deletes the… (h) |
| MD05 | No-HUD challenge | Off*, On | I | 1 | combat_hud.c |
| PM01 | Arena: start wave | 1*, 5, 10 | I | 1 | arena_modes.c am_start() (h) |
| PM02 | Arena: demon mix | All*, Cute only, Grotesqu… | I | 1 | arena_modes.c pick_type() (h) |
| PM03 | Arena: wave timer | Off*, On | I | 1 | arena_modes.c (h) |
| PM04 | Arena: maps | Rotate*, Fixed | I | 1 | arena_modes.c (h) |
| PM05 | Boss Rush: healing betwee… | Full*, Partial, None | I | 1 | arena_modes.c rush_next() (h) |
| CR01 | Creator mode | Simple, Advanced*, Everyt… | G | 2 | creator.c |
| CR02 | Preview stage | Room*, Plain, Hillvile, H… | G | 2 | creator_draw.c |
| CR03 | Turntable auto-spin when… | Off*, On | G | 2 | creator_draw.c |
| CR04 | Preview cycles expressions | Off, On* | G | 2 | creator_draw.c anim_phase() |
| CR05 | Locked options | Show silhouettes*, Hide | G | 2 | creator.c |
| PH01 | Hide HUD in photos | Off, On* | G | 4 | photo.c (h) |
| PH02 | Frame | None*, Polaroid, Film str… | G | 4 | photo.c (h) |
| PH03 | Sticker set | Cute*, Spooky, Both | G | 4 | photo.c (h) |
| PH04 | Self-timer | Off*, 3 s, 10 s | G | 4 | photo.c (h) |
| SY01 | Settings scope | Global*, Per island | G | 1 | options.c opt_scope_mode() |
| SY02 | Verify after saving | Off, On* | G | 1 | save.c |
| SY03 | Backup save copy (if spac… | Off*, On | G | 3 | memcard.c backup copy |
| SY04 | Show FPS and RAM (develop… | Off*, On | G | 1 | game.c dev overlay (h) |
| SY05 | "Reset this page" on ever… | action | G | 1 | fp_options.c |

**Hunk-dependent options, by owning team:**

- **S1** (32): GM16, ST03, ST04, ST05, ST06, GR15, GR16, AU16, AU17, AU18, AU19, AU22, DS19, AC16, AC30, TX01, TX02, TX03, TX04, TX05, TX06, TX07, TX08, TX13, TX15, TX16, TX17, GB01, GB03, RM02, CH07, CH29
- **freeplay** (24): GM05, GM06, GM09, GM12, GM15, IS01, IS04, IS05, IS06, IS09, IS12, IS13, IS14, IS15, LS19, ND10, ND12, RO22, DS24, DS26, TX09, CT06, CH25, PM22
- **S3** (23): GM03, GM04, PX07, PX08, PX09, PT01, PT02, PT03, PT04, PT05, PT06, PT07, PT08, PT09, PT10, PT12, AC06, CH16, MD12, PM07, PM08, PM09, PM18
- **foundation UI** (22): GM07, GM10, GM13, GM14, AU20, DS21, DS22, DS27, DS28, CB04, AC01, AC02, AC03, AC10, AC15, TX11, TX12, TX14, CT07, CT08, CT10, SY04
- **other (consumer named in the appendix)** (19): RO24, DS11, CB08, AC11, AC19, AC20, AC21, AC25, AC26, TX10, GB06, PS05, RM05, CH09, CH11, CH32, MD04, MD11, MD15
- **levels / action / minigames** (18): WP05, WP06, WP07, WP09, AC05, AC08, MD07, MD09, MD10, MD14, PM01, PM02, PM03, PM04, PM05, PM06, PM10, PM17
- **GBA / PS1 platform** (12): DS08, DS09, DS10, DS14, DS15, DS16, DS17, DS18, PH01, PH02, PH03, PH04
- **audio** (9): GR11, AU01, AU09, AU10, AU13, AU15, AU23, CH21, CH22
- **cook / concert / fish / studio teams** (8): PM11, PM12, PM13, PM14, PM15, PM16, PM20, PM21
- **S2** (7): ST01, ST02, ST09, LS18, PX05, AC24, PM19
- **Partly pending (CF-26).** These 5 are not in the 174, but Appendix B also names another team's file for them. Each ships with its owned consumer; the extra part stays `OF_PENDING` per value until its hunk lands:
  - GM17 (`levels.c`, levels);
  - IS11 (`fp_island_gfx.c`, freeplay);
  - GR01 (`levels_play.c` and `mg_fx.c`, levels and minigames);
  - GR03 (`mg_fx.c`, minigames);
  - FP01 (`r3d.c`, GBA/PS1 platform).
- **Consumers in other custom packages (CF-26).** 28 options are read in files that P5 does not own. The owning package implements the read; P5 owns only the option's definition, rules and probe. They are:
  - P0: ST10, IS02, IS03, IS11, RO23, PS07, CH17, SY02 (`town.c`, `save.c`, `memcard.c`, `look_derive.c`);
  - P1: LS10, PX16, DS20, CH08, CH14 (`portrait*.c`);
  - P3: AU01, AU09, AU11, AU12 (`spu.c`);
  - P4: ST11 and CR01-CR09 (`creator*.c`);
  - P6: GM11 (`unlocks.c` toast).
  - The render cheats and AC17/CH28 in `look_resolve()` are P0's.

## 6. Creator UX (240×160 GBA, 320×240 PS1)

### 6.1 Structure: 10 tabs, 43 sections, 392 rows

Each section has at most 24 rows, and each row edits one `LF_*` field. Tone fields live in the colour picker (6.4).

| tab | sections (rows) |
|---|---|
| **Body** (31) | Species & presets (5) · Shape (19: body, height, build, shoulders, torso, waist, hips, belly, arm/leg length + thickness, neck, head ratio, posture, hand/foot size, muscle, roundness) · Limbs & aids (6) · Details (auto style, Reroll details) |
| **Fur** (32) | Skin & material (6) · Colours & pattern (8) · Paws & fluff (8) · Glow, ink & scars (10) |
| **Face** (61) | Head (7) · Nose & muzzle (13) · Mouth (13) · Cheeks & freckles (18) · Marks & bits (10) |
| **Eyes** (39) | Shape (8) · Colour & pupils (13) · Lids & lashes (7) · Brows (7) · Glasses (4) |
| **Hair** (22) | Hair (11) · Hair bits (5) · Ears (6) |
| **Style** (20) | Makeup (7) · Face paint & tattoo (6) · Piercings (7) |
| **Spooky** (37) | Horns & extras (13) · Tail (7) · Wings (7) · Halo & aura (7) · Extra anatomy (3) |
| **Outfit** (81) | Tops & layers (19) · Bottoms, legs & feet (13) · Accessories (15) · Head, ear & tail pieces (13) · Prints & fabric (12) · Wardrobe (9) |
| **Motion** (46) | Moves (8) · Emotes (11) · Face & tail motion (11) · Voice (15) · Job (1) |
| **Heart** (23) | Personality (5 + type) · Profile (name, nickname, pronoun, romance, birthday, likes, hate, fear, catchphrase) · Home (district, home) |

**Modes** (CR01). Changing mode never changes a value; a chip says "+N hidden details".

| mode | rows | what it adds |
|---|---:|---|
| Simple | ~100 | today's rows + 12 core + Reroll details; one list per tab |
| Advanced (default) | ~200 | + the 47 wave-2 deep fields + the most-used D fields |
| Everything | 392 | everything |

```c
typedef struct CrRow {          /* v2: an LF id, not an offsetof; no 64-rows-per-tab limit */
    uint8_t  kind;              /* RK_* + RK_DEEP RK_DEEP_COLOR RK_DEEP_SLIDER RK_LINK RK_SEED RK_STYLE */
    uint8_t  level;             /* CRL_SIMPLE / CRL_ADV / CRL_ALL */
    uint16_t lf;                /* CR_NOFIELD = 0xFFFF */
    uint8_t  count, base, aux, flags;   /* CRF_PINNABLE CRF_SYM_PAIR CRF_FACE CRF_LEGS */
    const char *label; const char *const *names;   /* creator_names.c (creator overlay) */
} CrRow;
typedef struct CrSection { const char *name; const CrRow *rows; uint8_t nrows, icon; } CrSection;
typedef struct CrTab { const char *name, *shortname; const CrSection *secs; uint8_t nsecs, paint; } CrTab;
```

Rows are generated: `core/data/creator/sections.txt` → `tools/creator_build.py` → `core/gen/creator_rows.c`. The build fails unless every live registry field has exactly one row and every section has at most 24. The tone fields (DF 292..302) and `deep_spare` (DF 303) are exempt: they have no rows, because tones are edited in the colour picker (CF-29).

### 6.2 Screens

**GBA, 240×160:**

```
y   0 [Bd][Fu][Fa][Ey][Ha][St][Sp][Ou][Mo][He]  ADV         tab strip, 21 px cells
y  12 < Lids & lashes | Brows | Glasses >                    section pills
y  23 +-------------+-------------------------------------+
      | preview     | Upper lashes    < Spiky goth >  *   |  9 rows x 11 px
      | 96 x 100    | Lower lashes      Auto (Few)        |  "Auto (X)" always shows
      | figure 1x / | Lash colour   [##] Auto (= hair)    |  what auto resolves to
      | bust 2x /   | Eyelid droop  ====|--- 3            |
      | legs 2x     | ...                                 |
      |             | Wardrobe ##### 41%   Save ### 48%   |  meters (pink > 90 %)
y 124 +-------------+-------------------------------------+
      value strip: 8 swatches / 6 icons / slider ticks
y 148 A pick  B back  L/R section  SEL jump  START menu
```

**PS1, 320×240:**

- 29 px tab cells and full section names.
- A 128×176 preview: figure at 2×, bust at 3×, or legs at 2×.
- 16 rows of 12 px, a 2-line help footer, and a value strip of 12 swatches or 8 icons.
- Wave 4 adds the 3D turntable (T18) through `plat_model_preview()` with the impostor.

### 6.3 Controls

| action | GBA | PS1 |
|---|---|---|
| row ↑↓, value ←→ (repeat; locked values skipped) | D-pad | D-pad |
| picker / pin (hold 0.5 s) | A | × |
| back | B | ○ |
| previous / next section (crosses into the next tab) | L / R | L1 / R1 |
| previous / next tab | SELECT+L / SELECT+R | L2 / R2 |
| jump palette / compare with original (hold ≥ 10 ticks) | tap / hold SELECT | SELECT |
| undo / redo | X (SELECT+A) / Y (SELECT+B) | △ / □ |
| menu | START | START |

All of these go through the remap menu context.

### 6.4 Pickers, finding, randomising, copying

- **Choice grid.** 6×2 on the GBA and 8×3 on the PS1, with cached chibi or bust crops (one rendered per tick, kept in a portrait-pool LRU). The first entry is "Auto (= X)". Y stars a favourite (16 stars, `DoomoSettings.fav`), and starred values sort first.
- **Colour picker.**
  - Layout: Auto; then a **links** strip of labelled chips ("Hair (Pink)"); then 32 paints.
  - **L/R switches the tone: Normal, Pastel, Deep.**
  - On legacy colour rows, the links strip is hidden and the tone sets the matching `tone_*` field.
  - "Unlink" copies the linked paint into the row. CB04 adds colour names.
- **Jump** (tap SELECT). A grid of all 43 sections.
  - R switches to **letter mode**: every row, across all tabs, whose label starts with the letter.
  - It also lists Recently changed and Favourites.
  - Any row takes at most 8 presses this way, or at most 19 without it.
- **Randomise.** Scope: section, tab or all.
  - **Themes:** Cute, Spooky, Goth, Pastel, Punk, Royal, Furry, Demon, Angelic, Chaos, Like Hellvile, My favourite colour.
  - **Filters:** only colours, or only shapes.
  - **Reroll details** draws a new `look_seed` and costs 0 pool bytes.
  - **Hellvile-ify / Hillvile-ify.**
  - `fix_locks()` runs after every randomise, and pins hold.
- **Copy / paste.** A 64-pair clipboard holds a row, section, tab or whole look. Paste skips locked values and reports them.
  - **Copy from citizen** (whole, face, body, fur, outfit, motion) materialises the resolved values.
  - Saving as a Look Book style, and wearing one, come in wave 4.
- **Symmetry.** An `RK_LINK` row drives both sides from one value. It covers eye colour and right eye, eye asymmetry, special eye side, ear fold, ear piercing side, horn asymmetry, hair accessory side, beauty mark side, socks, prosthetic side and tattoo placement.
- **Presets.**
  - 144 species presets (3 × 48). Today `CR_PRESET_KV` is 40 **bytes** of u8 `(field, value)` pairs, 20 pairs. LF ids reach 367, so a preset becomes a variable-length list of `{u16 lf, u8 value}` triples, at most 64 per preset. Fixed 64-slot arrays for 296 presets would be about 57 KB in the creator group (CF-32). Changing species also resets the species-defining core fields and sets every SPC deep field to Auto.
  - 24 furry kits, 60 outfits, 16 makeup, 12 face paint, 20 voice, 8 motion, 12 themes.
  - All of this lives in the creator group.

### 6.5 Undo, meters, commit

- **Undo.** The 20 full snapshots (2,112 B) become a 4,096 B ring of delta records `{u16 lf, u8 old, u8 new}`.
  - A normal step costs 4-12 B; "Randomise all" costs at most 1.2 KB.
  - The footer shows "Undo: N steps". (`CRU_UNDO_LOTS` is unlock 273, "Undo 30 times in one sitting" (`creator.c:622`), not a depth setting. It keeps counting `n_undo`; the ring has no fixed step limit, CF-33.)
  - Undo keys use `lf`, replacing `tab*64+row` (`creator.c:1382`).
- **Meters.** Wardrobe (pool use) and Save (`save_budget_meter(1)`, shared with the house editor).
- **Commit:**
  1. Sanitize and pack the core.
  2. Build the run: entries that differ from what auto would give, plus tombstones over ROM or style values.
  3. Δ = core bound change + run size change.
  4. Check `save_bound_fits()` and pool free space.
  5. Write, `portrait_invalidate`, `progress_add(PROG_MAKEOVERS)`.
- **If the check says no:**
  - **Smart pack** tries all 2,048 seeds, keeps the one that turns the most explicit values into derived ones, and usually frees 30-50 % (about 0.1 s on the GBA for 20 values).
  - **Keep core only** shows a preview first.
  - **Back.**
- **Pool full** never blocks a core-only commit. The last 512 B of the budget are reserved for births and loads.

### 6.6 `CrState` (12,288 B scene scratch) and frame rate

**`CrState` totals ≈ 10,830 B:**

| part | bytes |
|---|--:|
| draft + original (citizen and deep set) | 832 |
| undo log | 4,096 |
| preview pixels | 2,688 |
| shown look + deep | 368 |
| tab icons | 1,330 |
| clipboard | 192 |
| lock bitmaps (24 × u64, indexed by base value: the paint index for colour rows, CF-34) | 192 |
| cursors, pins, jump list, dirty masks | ~800 |
| sparks and strings | ~330 |

**Frame rate.** Today's 13.4 fps is spent in `unlock_find` per field. The fixes:

1. **Lock bitmaps:** one `uint64_t` per visible row, recomputed on section entry, on scroll, and when `unlock_grant()` bumps `unlock_epoch`. Grandfathered values are folded in.
2. **Dirty regions:** tabs, pills, rows, preview and footer. With Mode 4 each region redraws for 2 frames after a change.
3. **View-masked, sliced previews.**
4. **Gate:** `creator_scroll.txt` (GBA and PS1 harness) holds DOWN through every section, then picks and cycles values. `tools/fps_gate.py` requires **≥ 20 fps on the GBA and ≥ 28 on the PS1**.

**Unlocks** (P6):

- `DoomoUnlock.list` is `uint8_t` (`unlocks.h:45`) and cannot hold LF ids up to 367. **(CF-35)** New part unlocks therefore use `list = CLIST_LF`, a new enum value, with `index = lf << 7 | value`, which fits `index`'s u16 (9 + 7 bits). The struct is unchanged; a 22-entry table maps the old `CLIST_*` values.
- **Bank 1:** 81 signature cosmetics (U01-U77) at creator ids 269 and 278..319, plus the custom reserve 202..239.
- **Bank 2:** ids 1024..1535 (`unlocks2`) hold the rest, plus 32 cheats and gated option values.
- **New `PROG_*` counters (16)** (`UL_PROG_SLOTS` 32 → 48 in `unlock_ids.h` and the assert at `unlocks.c:214`: an RFC to I9, the maintainer, CF-35): makeovers, photos, rainy days, sprint metres, slime kills, gargoyle kills, exorcism streak, heals, critters made, introductions, candy, looks copied, styles saved, salon visits, piercings, tattoos.
- Premade, generated and baby citizens ignore locks.

## 7. Work packages: parallel agents, one shared tree

### 7.1 Rules

1. **One owner per file per phase.**
   - Phase A is P0.M0 alone (serial).
   - Phase B is everyone else, in parallel.
   - Ownership changes hands only at that boundary.
2. **Other teams' files** are never edited directly: S1, S2, S3, H2 and H3 (SPEC C10), plus freeplay, library, audio, foundation and I9. The owning package asks for one marked hunk (`/* --- custom: <id> --- */ … /* --- end custom --- */`), in C10 order. Until it lands, the dependent feature is `OF_PENDING` or not live.
   - **Hand-overs (CF-36).** This spec takes ownership of files whose headers name another team. Each needs a written hand-over from that owner, recorded by I9 before Phase A, as story-SPEC C10 did for `save.c`, `combat*.c` and `actor.c`:
     - foundation: `resident.c`, `town.c`, `save.c`, `options.c`, `input.c`, `catalog.c`, `unlocks.c` (P6's parts only);
     - life: `sim*.c`;
     - action: `combat*.c`, `actor.c` (already given by C10);
     - roster: `roster.c`, `tools/roster_build.py`;
     - portrait: `portrait*.c`;
     - freeplay: `fp_options.c`;
     - library: `lib_almanac.c`;
     - gba and ps1 platform: `save_flash.c`, `memcard.c`, `pad.c`.
     
     A hand-over refused means that file is edited only by hunk. S3 keeps its existing authored-possessor hook in `sim_soul.c` and gets hunks for anything more.
   - **The Phase A repack (CF-37)** is the one sanctioned exception. It is purely mechanical (removed members → `RES_*`/`res_look()`/id-based calls), produced by `tools/repack_sites.py`, and it touches about 49 files that other active teams own (S1, S2, S3, freeplay, library, studio, concert, fishcook, ps1). It runs in a **declared freeze window** that I9 announces. The script is idempotent and re-runnable, so an in-flight branch rebases by re-running it, not by hand-merging.
   - **No cross-package edits inside custom either (CF-38).** Where one custom package needs code in another's file, the owner adds a call to a weak-stubbed hook that the requester implements in its own file:
     - P1 adds `pf_material_init(c)` to `pctx_init()` (`portrait.c:55`), and P2 implements it in `portrait_fur.c`;
     - P1 (`bodyspr_render`, `portrait.c:293`) and P2 (`pb_layout`, `portrait_body.c:51`) call P3's `motion_pose()`;
     - P1 exports `ph_head_only(c, lod)` from `portrait_head.c`/`portrait_hair.c` for P3's `headspr_render()`;
     - P5's `combat_hud.c` `weapon_view()` reads `handedness` for the first-person mirror;
     - P0's `save.c`, `memcard.c` and `look_derive.c` implement SY02, SY03, AC17, CH28 and the render cheats;
     - P6's `unlock_grant()` implements GM11.
3. **Live flags.**
   - P0 registers every appended value and deep field in M0, sanitized to 0 and hidden.
   - Each owner flips its own fields live in `core/data/look/live_p<N>.txt`, once the painters and the art review pass.
   - `tools/look_build.py` merges the files.
4. **Every merge passes:**
   - (a) `make -C core test` (unit tests + all runner scripts at 240×160 and 320×240);
   - (b) `make -C core check-gen`;
   - (c) `sh core/tools/xcheck.sh` (gcc, arm-none-eabi and mipsel, all at `-Werror`);
   - (d) `make -C gba && make -C gba test` must pass `memreport.py`:
     - EWRAM (relative to the merge base's measured free figure B, CF-1): ≥ B + 19,500 after P0.M0 and ≥ B + 12,000 after M1; no later package lowers it; never below 12,352 B absolute before `HOME` lands or 8,192 B after. I9 adds these gate lines to `memreport.py` after G0 (it is the memory pass's file until then);
     - IWRAM: 25,680 B (+0); stack ≤ 3,072 B in the creator, roster and combat scripts;
     - ROM: within C21's 190 KB share;
   - (e) `cmake --preset ps1 && cmake --build --preset ps1` must pass `elf2exe` (≥ 73,728 B below the stack top, and ≥ the merge base − the package's ledger line in 7.2) and `ovl_plan.py --report` (creator ≤ 104,448 B, options ≤ 51,200 B, combat per the P5 combat ledger, CF-24);
   - (f) the package's harness scripts on mGBA and PCSX-Redux, **with screenshots at both resolutions reviewed by eye**;
   - (g) fps (`tools/fps_gate.py`): creator ≥ 20 fps (GBA) and ≥ 28 (PS1); no scene below its G0 figure − 0.5;
   - (h) a `SAN=1` unit run;
   - (i) the docs;
   - (j) `check_options.py`.

### 7.2 Merge order and budget lines

```
0  G0: the memory pass (#77) merges; I9 re-measures; hand-overs recorded (CF-1, CF-36)
G  P0 captures goldens on the post-G0 tree (commit goldens only)
A  P0.M0a contract, codec, repack (freeze window), roster v2   -> everyone rebases
   P0.M0b sim age gate + venue table, reviewed sim-golden re-baseline (CF-20)
B  P3.M1 PS1 reclaims          before P1.M2 and P2.M2
   P0.M1 saves, pool, derive   before P4.M2, P5.M2, P6.M2
   P1.M1 interpreter, pipeline before P2.M2
   rest in any order, gated by wave (section 8); I9 re-measures after each merge
```

| package | GBA EWRAM | GBA ROM | PS1 resident |
|---|--:|--:|--:|
| P0 | −12,176 | −33 KB | −38,020 |
| P1 | 0 | +64 KB | +49,152 |
| P2 | 0 | +46 KB | +24,576 |
| P3 | 0 | +5 KB | −65,648 |
| P4 | 0 | +58 KB | 0 (overlay group ≤ 104,448) |
| P5 | 0 (its sink structs are in P0's line) | +30 KB | +13,312 (combat per CF-24) |
| P6 | 0 | +10 KB | +9,728 |
| **total** | **−12,176** | **+180 KB** | **−6,900** |

### P0: contract, codec, saves, generator (first)

**Writes first** (M0). Everyone builds against these:

- **Headers:** every 1.2-1.7 header: `resident.h`, `look_fields.h`, `look_pool.h`, `look_derive.h`, `settings.h`, `opt_rules.h`, `parts.h`, `motion.h`, `roster.h`, `save.h`, `town.h`, `portrait.h`, and the additions to `portrait_int.h`.
- **Generated:**
  - `core/gen/look_ids.h` and `look_fields.c`, built from `core/data/look/fields.txt`, the single source of truth;
  - `core/gen/opt_ids.h`, built from `core/data/options/options.txt` (all 458 ids).
- **Catalog:** every appended enum value and name in `catalog.h`/`.c`.
- **Stubs:** weak stubs in `stubs.c` for all P1-P6 functions.

**Owns.**

- **Phase A**, in addition to the files above:
  - `resident.c`, `look_pack.c`, `roster.c`, `town.c`;
  - `sim.c`, `sim_social.c`, `sim_soul.c`, `sim_int.h`;
  - `portrait.c` (fighter split, resolve plumbing) and `portrait_fighter.c` (the moved code);
  - `creator.c` (compile fix only) and `save.c`;
  - `tools/{gen_look,gen_opt,roster_build,repack_sites}.py`. **(CF-39)** `core/Makefile` runs `tools/gen_*.py` first and every other generator alphabetically (`Makefile:17-23`), so `creator_build`, `derive_build` and `motion_build` would otherwise run before the look registry exists in `check-gen`'s temp root;
  - `core/gen/roster_{data,text}.c`;
  - the tests that the repack and roster v2 break: `test_roster.c` (24 roster API uses), `test_town.c`, `test_freeplay.c`, `test_portrait.c`, `test_creator.c`, `test_sim.c`, `test_save.c`, `test_concert.c`, `test_fishcook.c`, `test_studio.c` (CF-37);
  - the scripted repack of every reader of a removed member or a changed roster API (CF-6, CF-7), run in I9's freeze window (CF-37).
- **Phase B:**
  - `resident.c`, `look_pack.c`, `look_pool.c`, `look_derive.c` (the engine only; P6 owns its tables);
  - `settings.c`, `save.c`, `save_v1.c`, `town.c`, `roster.c`;
  - `gba/src/save_flash.c` and `ps1/src/memcard.c`;
  - `tools/gen_look.py` and `fields.txt` (new fields on request). **(CF-40)** `options.txt` and `tools/gen_opt.py` pass to **P5** in Phase B: `OF_PENDING` and `OF_LIVE` flip as hunks land, and P5 must not need a P0 edit for each one;
  - the tests `test_{resident,save,look,pool,settings,flash_migrate}.c`;
  - the goldens (`fingerprints.txt` frozen, `fingerprints_v2.txt`, `roster_v1.txt`, `portrait_classic_golden.txt`) and `core/tests/data/saves_v1/`.

**Hunks requested:**
- I9: the `save_budget.c` slot-1 term.
- H2, H3 and foundation: `lookfp` in `hcmd.c` and in the native runner.
- The SONG, FPLY and FISH owners: `max_size` declarations (R14): 5,408, 270 and `FCX_SIZE` (≤ 182). **These gate P0.M1's bound proof (CF-9).**
- I9: `SAVE_SLOT_STORY_CAP` 40,960 → 36,864 in `save_budget.h`; `UL_PROG_SLOTS` 32 → 48 (RFC); the memreport/elf2exe gate lines after G0; the bank-2 id range in `test_integration.c` (CF-17, CF-35).
- Freeplay: `fp_title.c` and `fp_saveload.c` call `save_v1_load()` on `SAVE_ERR_V1`; `fp_saveload.c` shows the `look_used` meter (CF-26).
- H3: `save_v1` in the freeplay regex of `ovl_plan.py` (CF-18).
- PS1 platform: `ps1/src/coreref.c:34,128` calls `roster_premade()`. It is converted by the Phase A repack, or keeps a weak local copy (CF-7).

**Milestones.**

- **G** (on the post-G0 tree, CF-1):
  - `roster_v1.txt` and `portrait_classic_golden.txt`;
  - six v1 save fixtures: story mid-Ch2, premade untouched, premade + 50 edits, random lived-in, empty island + 1000 hand-made, and the worst case;
  - the defaults golden;
  - all runner screenshots.
- **M0:**
  - the record and the gene codec;
  - the repack, scripted and reviewed, in I9's freeze window: about 217 look-field and 84 whole-look occurrences, plus the identity members, over 49 files (CF-5). Whole-look sites move to id-based calls (CF-6) and `roster_premade()` callers to `roster_premade_record()` (CF-7);
  - the v1 bridges (the disk format stays v1 in M0);
  - roster v2: 58 B records, name pool, Huffman texts, venue table, `ROSTXT.BIN` and `EDGES.BIN`;
  - removal of `sim_s.job`;
  - the age gate for every citizen;
  - `look_inherit()` for core fields;
  - the sink structs, with the constants converted;
  - the fighter split;
  - the Classic resolve plumbing.
- **M1:**
  - save v2, with `save_v1.c` in the freeplay overlay on PS1;
  - `save_bound`;
  - the pool, the `LOOK` chunk and the NICK import. `CrNick`, `nick_ext` and the strong `doomo_save_ext_extra` live in `creator.c` (`creator.c:25-34,166-170`), which is P4's in Phase B. So P0.M1 lands `look_nick_get/set()` and the LOOK ext behind a weak `doomo_save_ext_extra`, and **P4.M1 removes `CrNick` in the same merge window**. Two strong definitions would not link (CF-41);
  - `look_derive` and `look_key`;
  - the settings: header copy, GBA journal with trailer migration, PS1 frame;
  - `opt[192]`, `unlocks2` and `progress[48]`.
- **M2:**
  - fuzzing: 10^4 random v1 streams, each either rejected or loaded, never a crash;
  - flash power-cut injection;
  - on-device `lookfp` diffs;
  - docs.

**Accept.**

- **M0:**
  - every G golden byte-identical: fingerprints, the v1 roster view, Classic portraits, the defaults, and all screenshots (pixel diff 0);
  - `sizeof == 76`;
  - EWRAM ≥ B + 19,500; PS1 ≥ B + 30,000 (relative to the merge base B; CF-1);
  - `test_roster_job_venue` passes;
  - `test_romance_age_gate`: over 365 simulated days, babies, custom kids, aged teens and random-island kids never crush, date, confess or propose.
- **M1:**
  - every v1 fixture loads to its v1-view FNV, saves as v2 and reloads identically;
  - `save_bound` ≥ the encoded size on 10^4 islands, and does not change over 30 sim days;
  - adversarial migration ≤ 86,635 B (CF-10), with the R14 `max_size` hunks landed;
  - `test_pool`;
  - Classic resolves to `def` for all 304 deep fields;
  - Varied gives identical results on native, GBA and PS1;
  - settings: newest gen wins, v1 never overwrites v2, and power cuts are safe;
  - EWRAM ≥ B + 12,000 (CF-1).

### P1: face and head renderer, pipeline core

**Owns:**
- `core/src/portrait{,_gfx,_head,_hair,_fx,_lut,_script}.c` and `portrait_int.h`, `portrait.h`;
- `tools/parts_build.py`, `core/data/parts/{face,head}/`, `core/gen/parts_{face,head}.c`;
- `test_portrait.c`, `golden/portrait_v2_*.txt`, `core/tools/portrait_sheet.c`;
- `docs/PORTRAIT.md`, `live_p1.txt`.

**Hunk requested:** the free-play memo key at `fp_common.c:656` (freeplay), and the `look_hash()` calls at `fp_common.c:702,729` → `look_key(rid)`.

**Hooks P1 adds for other custom packages (CF-38):** `pf_material_init()` in `pctx_init()` for P2, the `motion_pose()` call in `bodyspr_render()` for P3, and an exported `ph_head_only(c, lod)` for P3's head impostor.

**Milestones.**
- **M1** (early; it unblocks P2):
  - the interpreter, stamps and `parts_build.py`;
  - on PS1, LZ blocks with an 8 KB LRU;
  - 7 layer groups;
  - `pm_of` (tones, links) and `pc_region`;
  - per-job resolve;
  - `look_key` plus the test-build check on hits;
  - LOD and `look_anim_mask()`.
- **M2:**
  - wave-2 face fields: pupil, sclera, blush, glasses, ear fold, the head part of the hood, and the face C-tier deep fields;
  - list appends F01, F07, F25, F29, F35, F47, H01, H13, E01, B66, B76, B79 and O25.
- **M3:**
  - the remaining face, head and makeup deep fields;
  - A28 expressions;
  - aura and halo detail.

**Accept:**
- the Classic golden stays byte-identical;
- every live P1 value renders distinctly (`MAXOPT` 128);
- maxed fixture: < 14,000 pixel visits and ≤ 240 part ids;
- each slice ≤ 300k cycles and a bust ≤ 1.45M (mGBA bench over random v1, random v2 and kitchen-sink sets);
- a non-chibi change leaves the chibi hash unchanged;
- contact sheets reviewed;
- IWRAM +0; PS1 line holds;
- `portrait_gallery.txt` and `creator.txt` reviewed at both resolutions.

### P2: body, furry and clothing renderer (portrait, figure, chibi)

**Owns:**
- `core/src/portrait_body.c`, `portrait_wear.c` (garment layers) and `portrait_fur.c` (materials, fur patterns, region masks);
- `core/data/parts/{body,outfit}/` and `core/gen/parts_{body,outfit}.c`;
- `test_portrait_body.c`, `docs/PORTRAIT_BODY.md`, `live_p2.txt`.

**Milestones.**
- **M1** (C painters; it can start before P1.M1):
  - proportions and posture;
  - material modes;
  - fur pattern and fur 3, belly and pads;
  - hands, feet and legs;
  - the outer layer and legwear.
- **M2** (after P1.M1 and P3.M1):
  - tail fan and wings;
  - outfit slots, colour 2, trims, patterns;
  - socks, gloves, belt, back and neck pieces;
  - appends B01, B52, B59, O03, O06, O12 and O45;
  - the chibi mask.
- **M3:** the wave-3 tail of fur, limb, modifier and accessory fields.
- **M4:**
  - extra anatomy, mobility aids, prosthetics;
  - situational outfits;
  - +24 species bodies (with P6).

**Accept:**
- P1's gates, run in `test_portrait_body.c`;
- chibi ≤ 360k cycles, and a lipstick change re-renders 0 chibis;
- `island40` (40 chibis) ≥ the G0 figure − 0.5 fps;
- a maxed outfit costs ≤ 1.3× a plain one;
- contact sheets reviewed;
- PS1 line holds.

### P3: third person, motion, PS1 RAM moves

**Owns:**
- `core/src/portrait_fighter.c` and `motion.c`;
- `core/data/motion/`, `tools/motion_build.py`, `core/gen/motion_data.c`;
- `ps1/src/{model,spu,rostxt}.c`. **(CF-42)** `ps1/src/prims.c` belongs to H3 (`prims.c:1-2`; story SPEC §6 and row 229); P3 only calls it. There is no `cdread.c`: P3 reuses S2's CD reader in `txtload.c` (`CdRead`, `txtload.c:47`), through an S2 hunk that exports it;
- `core/src/portrait_gun.c` (**CF-23**: it was in no package, and the left-handed weapon mirror lives there);
- `tools/sfxgen.py` (C10 gives SFX→SPU to custom) and `core/gen/sfx_{info,pcm}.c`;
- `test_motion.c`;
- the PS1 harness scripts `model_looks`, `thirdperson_looks`, `rostxt`, `sfx_boot` and `impostor`, and the GBA script `tp_looks`;
- `live_p3.txt`.

**Hunks requested:**
- S2 (C3): `ps1/CMakeLists.txt` and `iso.xml`. Drop `roster_text.c` and `sfx_pcm.c` from the EXE, and add `ROSTXT.BIN`, `EDGES.BIN` and `SFX.BIN`. **(CF-43)** Today `sfxgen.py` emits one file, `core/gen/sfx_data.c`, which holds both `doomo_sfx_info` and `doomo_sfx_pcm_data`. P3 first splits it into `sfx_info.c` and `sfx_pcm.c`, deleting `sfx_data.c`. The same S2 hunk links P1/P2's LZ part blocks and excludes the raw `parts_*.c` (CF-18).
- S1/story (`audio.c` owner): `audio_sfx_pcm()` (`audio.c:587-598`) returns NULL on PS1 when the PCM lives in SPU RAM. Without this hunk it indexes a dropped array: a link error, or out-of-bounds reads in `lib_jukebox.c:87,309` (CF-43).
- PS1 platform: `ps1/src/coreref.c:166-169` `core_sfx_pcm()` follows the same rule.
- Library: `lib_jukebox.c` `build_wave()` reads the 2,880 B wave table.
- PS1 platform: `vram.c`, only if needed.

**Milestones.**
- **M1:**
  - SFX: `SFX.BIN` loaded into SPU RAM at boot (eager ADPCM); on PS1, `audio_sfx_pcm()` returns NULL after boot;
  - `rostxt.c` overrides `roster_text_pack()`, with two 2 KB sector buffers and a prefetch on screen entry;
  - `EDGES.BIN` is read once by `town_new()`.
- **M2:**
  - pose tables (idle, walk, run, sit, sleep, dance, emotes, laughs, blinks);
  - fighter LOD, run styles, and the mirror for left-handers;
  - the head impostor;
  - the `model.c` body recipes.
- **M3:**
  - motion fields go live;
  - decision on the #79 levers.

**Accept.**
- **M1:**
  - PS1 ≥ B + 100,000 below the stack top with P0.M0 (relative, CF-1);
  - `sfx_boot` plays all 34 SFX;
  - `rostxt`: 50 random profiles and almanac pages, each ≤ 0.25 s, and **0 CD reads** in a full sim day;
  - `test_audio` green.
- **M2:**
  - `model_looks` (8 kitchen-sink looks × 5 views) and `impostor` show every head option in 3D;
  - ≤ 120 primitives, inside the 30 fps frame;
  - GBA `tp_looks` matches native;
  - `test_motion`: every style changes the pose hash, deterministically.

### P4: creator UI

**Owns:**
- `core/src/creator{,_data,_draw,_names,_presets,_themes,_book}.c` and `creator_int.h`;
- `core/data/creator/sections.txt`, `tools/creator_build.py`, `core/gen/creator_rows.c`;
- `test_creator.c` and `core/tests/scripts/creator*.txt`;
- `{gba,ps1}/harness/scripts/creator_scroll.txt` and `tools/fps_gate.py` (shared by all);
- `docs/CREATOR.md`.

**Hunks requested:**
- H3: the creator regex in `ovl_plan.py`, and `group_of()` routing for `/core/gen/creator_rows` (CF-18).
- H2: `creator_scroll.txt` in a `gba/Makefile` test target; `gba/Makefile` lists its scripts explicitly (story-SPEC C2).
- Freeplay or S1: a salon location for "salon makeovers" (M4).

**Coordinated with P0.M1:** P4.M1 removes `CrNick`, `nick_ext` and the strong `doomo_save_ext_extra` from `creator.c` (CF-41).

**Milestones.**
- **M1:**
  - `CrRow` v2 and the row generator;
  - tabs, sections and modes;
  - pickers, including the colour picker with tones and links;
  - **lock bitmaps, dirty regions and the view-masked preview**;
  - the delta undo;
  - the sliced preview and the lower-body zoom.
- **M2** (needs P0.M1):
  - deep rows labelled "Auto (= X)";
  - meters, Smart pack, Keep core only;
  - Jump, letter mode, favourites, pins;
  - themes, randomise scopes, copy/paste, symmetry;
  - all presets.
- **M3:**
  - every live field reachable;
  - Hellvile-ify;
  - Possessed peek.
- **M4:**
  - the Look Book;
  - Look Password: a base-32 delta against the species preset plus CRC-16, valid on both consoles;
  - salon makeovers.

**Accept:**
- **`test_creator`:**
  - exactly one row per live field, and ≤ 24 rows per section;
  - every row reachable by Jump;
  - randomising a section touches only that section, and pins hold;
  - undo replays exactly;
  - the lock bitmap equals `cr_value_locked()`;
  - presets apply through LF ids;
  - a refused commit leaves the town untouched.
- **`creator_scroll` ≥ 20 fps on GBA and ≥ 28 on PS1.**
- `CrState` ≤ 12,288 B and the creator group ≤ 104,448 B.
- Screens reviewed at both resolutions.

### P5: options, sinks, controls

**Owns:**
- `core/src/{options,opt_rules,palfx,options_text,audio_bus,fp_options,input}.c` and `input.h`;
- `sim{,_care,_events,_social,_soul}.c` and `sim_int.h`;
- `combat{,_hud,_scene}.c`, `combat_int.h` and `actor.c`;
- `ps1/src/pad.c`;
- `core/data/options/{cut,pending_hunks}.txt` and `tools/check_options.py`;
- `test_{options,options_effect,input_remap,sim,combat}.c`;
- `docs/OPTIONS.md`, `live_p5.txt` (voice and sound fields).

**Hunks requested,** grouped as in 5.4 (C10 order):

| team | files |
|---|---|
| S1 | `ui_dialog`, `cutscene`, `overworld*`, `gba/src/main.c` `read_pad` |
| S2 | `story*`, `dream` |
| S3 | `exorcism*`, `pet*` |
| audio | `audio.c` `send()` |
| freeplay | `fp_island`, `fp_shop`, `fp_common`, `fp_title`, `fp_saveload` |
| foundation | `ui`, `ui_draw` |
| platform | `r3d`, `view3d` |
| levels / action | `levels*`, `arena_modes`, `mg*` |
| their own teams (wave 3) | `cook`, `concert`, `fish`, `studio`, `weld_game` |
| S1 (more, CF-26) | `audio_fx.c` (`fx_rules`, footsteps), `tween.c` (AC16), `camfx.c` (AC30), `gba/src/postfx.c` and `ps1/src/postfx.c` (palfx base palette, LCD grid, ghosting; CF-31) |
| minigames | `mg_fx.c` (`fx_rules`: GR01, GR03) |
| gallery | `ui_gallery.c` (keeps local option names, CF-18) |
| P0 | `town.c`, and the reads in `save.c`, `memcard.c` and `look_derive.c` (CF-38) |
| P1, P3, P4, P6 | the consumer reads listed under "Consumers in other custom packages" (5.4) |

**Milestones.**
- **M1:**
  - the options screen: 31 pages, presets, help;
  - `opt_rules.c`, `palfx`, the audio buses, `input_timing`;
  - remapping: the permutation, its screen, and the restore combo;
  - `check_options.py` and `test_options_effect`;
  - the 8 dead options.
- **M2:**
  - all 284 options whose consumers are in owned code;
  - the hunk options, as their hunks land.
- **M3:**
  - waves 2 and 3;
  - the voice fields go live.

**Accept:**
- `check_options.py` clean;
- `test_options_effect`: every non-pending option changes its probe for every value;
- the defaults golden is unchanged;
- `test_input_remap`: every profile is a bijection, confirm and back are always reachable, and restore works from every scene;
- the combat group within the P5 combat ledger (custom share ≤ +1,000 B of C4's +1,936 B unless I9 decides otherwise; CF-24), and the options group ≤ 51,200 B;
- `options_tour.txt` at both resolutions;
- no GBA scene below its G0 figure − 0.5 fps.

### P6: premade variety, derivation tables, unlocks, almanac

**Owns:**
- `core/data/roster/*.txt`, `tools/roster_build.py`, `core/gen/roster_{data,text}.c`;
- `core/data/derive/`, `tools/derive_build.py`, `core/gen/derive_tables.c`, `core/tools/look_seed_search.c`;
- `catalog.h` and `catalog.c`;
- `unlocks_custom.c`, and in `unlocks.c`: bank 2, LF-id lists, `unlock_lf_mask()`;
- `lib_almanac.c` (fashion pages; coordinate the S5 hook). This is a library-team file: ownership needs the library team's hand-over (CF-36), or the fashion pages go in a new `lib_almanac_fashion.c` that matches the library group regex, with one registry hunk;
- `test_{roster,derive,unlocks_custom}.c` and `golden/derive_golden.txt`;
- `docs/ROSTER.md`, `live_p6.txt`.

**Hunks requested:**
- `core/Makefile` (foundation): one rule for the seed search, which must run before `roster_build.py` in `make gen`/`check-gen`, as `furngen` does (`Makefile:119-140`).
- `unlock_ids.h`: `UL_IDS_CUSTOM2_FIRST/LAST` 1024..1535.

**Milestones.**
- **M1:**
  - derive tables: 48 species looks, 46 chance lists with Hillvile/Hellvile odds, persona maps, harmonies;
  - the seed search;
  - `--assign-coverage`.
- **M2:**
  - authored deep keys for the story cast and about 100 fashion icons;
  - reviewed premade restyles, listed in `core/data/roster/restyled.txt`;
  - unlocks U01-U77 and the 16 new counters.
- **M3:**
  - the almanac fashion pages;
  - +24 species, with P2.

**Accept:**
- `test_roster`:
  - v1 view golden, or the reviewed restyle list;
  - every live appended and core value is worn;
  - the deep layer ≤ 4,096 B;
  - the seed search is reproducible.
- `test_derive`:
  - the derive golden passes;
  - Classic resolves to `def`;
  - ≥ 60 % of catfolk get slit-ish pupils;
  - Hellvile black sclera at ≥ the table odds.
- `test_integration`: bank-2 ranges.
- `library_almanac.txt` reviewed at both resolutions.
- PS1 line holds.

## 8. Waves and risks

| wave | content | packages | gate |
|---|---|---|---|
| 0 enablers | G0, P0.M0a-M1, P3.M1 | P0, P3 | goldens identical (sim goldens re-baselined once at M0b); GBA ≥ B + 12,000; PS1 ≥ B + 110,000 |
| 1 options and free variety | 355 options, `palfx`, remap, 28 list appends, interpreter, tones | P5, P1.M1, P2.M1 | `check_options`, distinctness |
| 2 core looks and derived detail | 14 core painters, 47 deep fields, derive tables, seed search, impostor and model, creator v2, 90 options | P1-P3.M2, P4.M1-2, P6.M1 | creator fps, derive golden, impostor on hardware |
| 3 the deep long tail | 233 D-tier fields in batches: Face/Eyes/Style → Fur/Body → Spooky → Outfit → Motion | P1-P3.M3, P5.M3, P6.M2-3 | per batch: distinctness, pixel visits, art review |
| 4 systems | Look Book, situational outfits, Look Password, ageing, extra anatomy, mobility, prosthetics, hybrids, +24 species, PS1 3D turntable, photo mode | P2.M4, P4.M4, P6.M3 | full suite and playthroughs |

| risk | mitigation and test |
|---|---|
| the repack breaks a reader (335 sites) | removed members fail to compile on 3 toolchains; G goldens must stay byte-identical |
| v1 saves or Classic looks change | frozen `save_v1.c`; v1 base = `res_to_v1(v2 base)`; fixtures, fuzz, `portrait_classic_golden` |
| a save overflows slot 1 | static, sim-invariant `save_bound` checked on every growth path; `test_save`, `save_joint_worst_case` |
| the PS1 ledger runs out | reclaims first (P3.M1 before P1.M2/P2.M2); per-package lines; #79 levers; C5 checked on every merge |
| render cost or fps | 7 groups, chibi mask, decals without ids, occlusion, sliced preview, lock bitmaps; bench, `creator_scroll`, `island40` |
| the pool fills, or derivation drifts | core fields never need the pool; Smart pack; meters. One `look_derive.c` for consoles and tools, plus the derive golden |
| kid or teen safety | `age` is core and gates both sides for every citizen; `test_romance_age_gate` |
| a flash migration loses a save | no data byte moves; trailer copied before erase; `test_flash_migrate` with power cuts |
| options that do nothing, or a remap lockout | sinks, `check_options.py`, `OF_PENDING`, effect probes; permutation + 3 s restore |
| programmer art, or hunks that stall | contact sheets and art review before a field goes live; `OF_PENDING` and live flags keep packages unblocked |

*Calculators: `scratchpad/custom/final/{budget,ledgers,ledger2,gen2,gen3,assemble2}.py`. Source data: `scratchpad/custom/hyb/{model.json,wish.json,opt_wiring.txt}`. Critique checks: `scratchpad/custom/critique/{gene_check,fields_check,opt_check,budget_fix}.py` and `counts.c`. Read-only against HEAD `8cc7910`.*

## 9. Critique fixes

An adversarial review against the real tree at `8cc7910` (`core/`, `gba/`, `ps1/`, the build maps in `gba/build` and `ps1/build`, and `docs/design/story/SPEC.md`). Each item gives the problem, the evidence, and the fix. **Where the body and this section disagree, this section wins.** The body was edited in place, and its fixes carry the same `CF-n` tags.

### 9.1 Correctness and determinism

- **CF-2. Unaligned gene reads.**
  - Problem: "FNV-1a over gene words" and "~25-cycle `gene_get`" invite `uint32_t` loads from `gene[]`. `DoomoResident` has 1-byte alignment, and `res[]` follows `edges[]` (6 B records) in `DoomoTown`.
  - Consequence: a word load from an odd or 2-aligned address rotates silently on ARM7TDMI, so GBA and native diverge. On the R3000 it raises an address-error exception.
  - Fix: all gene access is byte-wise, assembling words from bytes as `portrait.c:506-517` does.
- **CF-3. The `hair_highlight` sentinel was missing.**
  - Problem: `LOOK_NO_COLOR` is 0xFF (`resident.h:15`), and premades use it (`roster_data.c` rows 1-11 store 255). It does not fit `hair_highlight@97:6`.
  - Fix: the gene stores 32, and `res_look()`/`res_pack()` map it, exactly as v1's `field_get`/`field_set` do (`save.c:554-565`).
- **CF-12. Ageing moved `save_bound`.**
  - Problem: LS11 ageing writes `age`, a non-hot gene field, from the sim. That makes `cb(id)` grow without any growth-path check, so "the sim cannot move `save_bound`" was false.
  - Fix: `age` is serialised as the 16th hot field (groups 0-1). Hot is now 96 bits; core2 is 56.
  - This also corrects the spec's "16 sim-mutable fields": there were 15. The schema has 99 fields, not 100.
- **CF-13. The NICK import was wiped by the LOOK reset.**
  - Problem: `read_exts()` resets every registered chunk the save lacks, after all chunks are read (`save.c:997-1001`). A v1 save has no `LOOK`, so `LOOK`'s reset() would run after `look_pool_import_nick()` and erase the nicknames.
  - Fix: `save_v1.c` runs its own chunk loop: pool reset, then the NICK import, and `LOOK` counts as seen.
- **CF-14. A global `pool.gen` in the cache keys.**
  - Problem: every pool write (a birth, say) would miss the image of every citizen who has a run, and a wrapped 16-bit gen could revive a stale image.
  - Fix: keys are content-addressed, hashing the bytes of the own and style runs.
- **CF-45. The chibi key hashed only the gene.**
  - Problem: 91 chibi-visible deep fields live in the pool, not the gene, so a change to one of them left a stale chibi.
  - Fix: the chibi key also hashes the `DFF_CHIBI` run entries.
- **CF-20. Sim goldens could not stay byte-identical.**
  - Problem 1: the age gate changes sim behaviour on purpose (crushes on babies, kid hours).
  - Problem 2: the instant venue table bypasses `job_venue()`'s per-tick budget (`sim.c:297-307`).
  - Problem 3: a raw FNV over `DoomoTown` changes with the layout.
  - Fix: M0a is the refactor with every golden identical, using a 125 B warm-up bitset. M0b is the age gate, with a reviewed re-baseline of the sim and story goldens and S2/I9 notified. The defaults golden hashes a canonical v1 view.
- **CF-22. Stale sinks after a load.**
  - Problem: the sinks were rebuilt only on `opt_set()` and scene entry, so the sim could tick under the previous island's rules.
  - Fix: they are also rebuilt in `town_new()`, after `save_deserialize()`, in `opt_defaults()` and after a settings load.
- **CF-25. Derive-table drift.**
  - Problem: every Varied citizen in every save is derived from the tables, so tuning them restyles saved citizens.
  - Fix: rows of live fields are append-only; `DERIVE_V` covers the rare reviewed change.
- **CF-21. Tombstone wording.**
  - Problem: 1.1 said "choice 0 is a tombstone", while 1.5 stores `choice + 1`.
  - Fix: low bits 0 are the tombstone; an explicit choice 0 is stored as 1.
- **CF-16. Kid safety on migration and on random islands.**
  - Problem: v1 had no age, so every hand-made kid migrated as an adult. `AGE_AUTO` on random islands was always adult. `sim_traits()` returns 0 early for non-premade islands (`sim.c:122`).
  - Fix: migrated `BODY_TINY` citizens get `AGE_KID`. `gen_core2()` sets kid for `BODY_TINY`. The age bits are applied before the early return.

### 9.2 Saves and migration

- **CF-9. The bound was not a proof yet.**
  - Problem: `SONG`, `FPLY`, `FISH` and `NICK` declare `max_size` 0 (`song_book.c:265`, `fp_common.c:323`, `fishcook.c:119`, `creator.c:168`). The formula summed `max_size`, so it would have undercounted them as 0 B.
  - Fix: `max_or_ext()` counts an undeclared chunk as `DOOMO_SAVE_EXT_MAX`. The three R14 hunks are a hard dependency of P0.M1, with the values from the code: `SONG` 5,408, `FPLY` 270, `FISH` ≤ 182. Without them the room drops from 69.4 KB to 50.7 KB.
  - The baseline line said only `HOME` declares `max_size`: `STRY` declares 96 too.
- **CF-10. The fixed part was undercounted.**
  - Problem: `TOWN_FIXED_MAX` 1,436 is 23 B short. Recounted from `write_town()` with the v2 sizes, it is 1,459 B: magic and scalars 22, items 129, weapons 384, pet 24, story 62, `opt` 192, unlocks 192, progress 192, graves 258, edge count 2, end 2.
  - Fix: the value is computed by a counting pass and asserted in `test_save`, never typed in. With the real chunk maxima, the worst v1 migration is **86,635 B** (was 87,204) and the capacity table is restated (`critique/budget_fix.py`).
- **CF-17. PS1 settings, SY06 and the slot-0 cap.**
  - Problem: the PS1 "settings frame" cannot hold 416 B in a 128 B frame, and moving the payload one frame would break v1 files. SY06 does not exist (the SY category has 5 options). A "one-frame" card write is really an 8 KB block of about 1 s. `SAVE_SLOT_STORY_CAP` (`save_budget.h:36`) still says 40,960.
  - Fix: settings live only in the v2 header on PS1, written with the next save, and `HDR_FRAMES` stays 5. An I9 hunk sets the story cap to 36,864.
- **CF-27. The slot capacity is not a constant.**
  - Problem: `save_flash.c:54-55` accepts 64 KB chips, where slot 1 is about 24 KB.
  - Fix: the growth check uses the platform's real capacity.
- **CF-28. One write path.**
  - Problem: any new runtime source of citizens, such as IS02 moves, would bypass the bound.
  - Fix: every record write goes through `town_put()`, and `test_save` greps that no other code assigns `doomo_town.res[]`.
- **CF-30. Save time.**
  - Problem: a decode/re-encode `resident_sanitize()` per citizen per streaming pass adds about 0.4 s to a GBA save.
  - Fix: `res_pack()` sanitises at commit, so the writer only clamps hot fields.

### 9.3 Memory

- **CF-1. Absolute gates on a moving baseline.**
  - Problem: the task states that the live tree is about 33 KB over its GBA EWRAM budget and over 2 MB on PS1. Task #77 also owns `gba.ld`, `bigbuf.c`, `gba_plat.h`, `memreport.py` and `scene_scratch.c` until G0. So `8cc7910`'s 7,104 B and 110,624 B are only a snapshot. The memory pass may also take the same levers itself, such as `doomo_town` and the portrait pool, which would double-count P0's −20 KB.
  - Fix: every gate is relative to the merge base, P0 starts after G0, and the goldens are captured post-G0. I9 confirms which levers #77 already spent.
- **CF-4 / CF-15. Ledgers.**
  - GBA EWRAM: the warm-up bitset (+125), the roster text rings (+168) and the sink structs (+200, which the spec had not counted) give a net of **−12,176** (was −12,669). At the `8cc7910` base that is 19,280 B free, or 15,848 B after `HOME`.
  - PS1: the same lines plus the wider metadata (+458 for options, +166 for gene fields) give a net of **−6,900** (was −7,817). That is 117,524 B at the base, or about 103-110 KB once the story's own C5 lines land.
- **CF-8. Field widths.**
  - `DoomoGeneField.max` was u8, but `look_seed`'s max is 2047.
  - `DoomoOptionInfo.store` was u8, but there are 269 global slots.
  - Both are now u16, and the ledgers include the cost.
- **CF-11. Pool run length.**
  - Problem: a u8 `n` caps a run at 255 entries, but a full "copy look" can need 304.
  - Fix: the header is `u16 id/kind` + `u16 n`, still 4 B.
- **CF-18. Generated files on PS1.**
  - Problem: they link resident by default (`CMakeLists.txt:40`, `ovl_plan.py:116-129`). `GROUPS` is first-match, and `fp_options` matches `freeplay` today (`ovl_plan.py:56`). `save_v1` matches no group, so it would be resident. The raw `parts_*.c` (92 KB) would link resident.
  - Fix: H3 hunks for `group_of()` and the regexes, and an S2 CMake hunk for the LZ parts.
- **CF-24. The combat group.**
  - Problem: C4's +1,936 B is the whole group's slack, shared with S3's `combat_story.c`. 150 options are consumed in combat-group files.
  - Fix: P5 delivers a combat ledger with offsets, the custom share is ≤ +1,000 B, and I9 decides anything beyond that.
- **CF-32. Creator presets.**
  - Problem: `CR_PRESET_KV` is 40 **bytes** of u8 pairs (`creator_int.h:62,65`, 20 pairs), not 40 pairs. LF ids need u16.
  - Fix: variable-length `{u16, u8}` lists, which keep the creator group cap.

### 9.4 API, wiring and consumers

- **CF-5. Repack counts.**
  - Problem: "216 identity, 119 `.look.`, 22 species" do not match the tree.
  - Fix: re-measured as about 217 look-field and 84 whole-look occurrences, plus identity members in 37 files, over 49 files.
- **CF-6. Whole-look APIs.**
  - Problem: `figure_image(&r->look)`, `fp_host_look()`, `model_set_look()` and `look_hash(&r->look)` would decode to an id-less look and silently drop the own and style deep layers.
  - Fix: the resident call sites, all listed in 1.2, go id-based.
- **CF-7. The roster API.**
  - Problem: `roster_premade()` returns a pointer into today's 96 B array. `roster_bio()` and `roster_job()` have 13 callers in 9 teams' files, and the spec never mentioned them.
  - Fix: `roster_premade()` callers are converted in the repack. The text functions keep their signatures as wrappers over a 2-deep ring (168 B), with prefetch on PS1.
- **CF-19. `input_context()`.**
  - Problem: it had no declaration that P0 writes in M0 (`input.h` is P5's).
  - Fix: it is declared in `settings.h`.
- **CF-23. Third person.**
  - Problem: `render_fighter_ctx()` is in `portrait.c:306`, not `combat.c:1650-1668`. `portrait_gun.c`, the held weapon and the home of the left-handed mirror, was in no package.
  - Fix: P3 owns `portrait_gun.c` and implements `fighter_image_ex(FI_DITHER)`; P5 only calls it.
- **CF-26. Option wiring.**
  - Five options name another team's file but were not pending: GM17, IS11, GR01, GR03 and FP01. They are now partly pending.
  - AC01's `big_glyphs()` is in `ui.c:325`, which is foundation UI, not S1's `ui_dialog.c`. The lists now read S1 32, foundation UI 22.
  - `check_options.py`'s blanket `unlocks*.c` exclusion would fail GM11, whose only read is in `unlock_grant()`. It now ignores unlock tables, not code.
  - 28 options are read in other custom packages' files. Those packages implement the reads.
  - `fp_title.c:487` and `ui_gallery.c:141,248` read option names that would move into the options overlay. They get local copies.
  - `fp_title.c` and `fp_saveload.c` need the `save_v1_load()` and meter hunks.
  - The `fx_rules` consumers `mg_fx.c` (minigames) and `audio_fx.c` (S1), and S1's `tween.c`, `camfx.c` and `postfx.c`, were missing from P5's hunk table.
- **CF-31. palfx.**
  - Problem: `plat_pal_lerp()` only lerps to four fixed palettes (`platform.h:205-207`; it is S1's `postfx.c` on both consoles). The PS1 CLUT path does not reach the r3d world or the gouraud models. LCD grid and ghosting are not palette operations.
  - Fix: a `plat_pal_base()` hook (S1 hunks), PS1 3D coverage behind a platform hunk (`OF_PENDING` until then), and the grid and ghosting through `postfx.c`.
- **CF-33. `CRU_UNDO_LOTS`.** It is unlock 273, "Undo 30 times" (`creator.c:622`), not an undo-depth setting. The sentence was corrected.
- **CF-34. Lock bitmaps.** A `u64` cannot cover the 112 choices of a colour row. Bits are indexed by base value, the paint for colour rows, since tones and links share the paint's lock.
- **CF-35. Unlocks.**
  - Problem: `DoomoUnlock.list` is u8 (`unlocks.h:45`) and cannot hold LF ids. `UL_PROG_SLOTS` is 32, and `unlocks.c:214` asserts it. Bank-2 ids ≥ 1024 contradict the "0..1023" contract and `test_integration`'s range check.
  - Fix: `list = CLIST_LF` with `index = lf << 7 | value`, plus I9 RFCs for `UL_PROG_SLOTS` 48 and the bank-2 range.
- **CF-43. SFX.**
  - Problem: there is no `sfx_pcm.c`. `sfxgen.py` emits `core/gen/sfx_data.c`, holding both tables. `audio_sfx_pcm()` (`audio.c:587-598`, owned by story) indexes `doomo_sfx_pcm_data`, so dropping it breaks the link or makes `lib_jukebox.c:87,309` read out of bounds. `ps1/src/coreref.c:166` wraps it.
  - Fix: P3 splits the file, and audio.c and coreref.c hunks are added.
- **CF-44. Name clash.** `town.h` already defines `DoomoOptionInfo` (name, count, def, values, page). The new metadata struct is `DoomoOptMeta`.
- **CF-29. Counts.**
  - Rule counts: PER is 22 and MAP 19 (Appendix A), not 21 and 20.
  - "304 = 268 wishlist items + style + 11 tones + spare" only sums to 281. It is 291 fields from 268 distinct items.
  - The 11 tone fields are flagged chibi but list no C view in Appendix A. Treat the flag as authoritative.
  - The one-row-per-field build check must exempt DF 292..303.
  - The 8 `DFF_*` flags fill the byte.

### 9.5 Ownership and process

- **CF-36. Hand-overs.** P0, P5 and P6 own files whose headers name foundation, life, roster, portrait, freeplay, library, gba or ps1, while rule 2 forbade direct edits to foundation, freeplay and library files. Hand-overs are now recorded by I9; a refused hand-over means hunks only.
- **CF-37. The Phase A repack.**
  - Problem: it edits about 49 files owned by teams that are active right now (S2c.4, S3.2-S3.5, freeplay #56, perf #16-18).
  - Fix: a mechanical, idempotent script in an I9 freeze window. Phase A also had to own the test files the repack and roster v2 break, and it now lists them.
- **CF-38. Cross-package edits inside custom.**
  - P2's material modes needed P1's `pctx_init()` (`portrait.c:55`).
  - P3's motion needed P1's `bodyspr_render()` and P2's `pb_layout()`.
  - P3's impostor needed P1's static head painters.
  - P3's first-person mirror needed P5's `weapon_view()`.
  - P5's options needed P0's `save.c`, `memcard.c` and `look_derive.c`, P1's `portrait.c` and P6's `unlocks.c`.
  - Fix: owner-added weak hooks, with the requester implementing in its own file.
- **CF-39. Generator order.** `make gen`/`check-gen` runs `tools/gen_*.py` first and the rest alphabetically, so `creator_build`, `derive_build` and `motion_build` would run before `look_build`. The registry generators are renamed `gen_look.py` and `gen_opt.py`. The C seed search gets a rule before `roster_build.py`, like `furngen`.
- **CF-40. `options.txt` ownership.** P5 owns `options.txt` and `gen_opt.py` in Phase B, so `OF_PENDING`/`OF_LIVE` flips need no P0 edits.
- **CF-41. A double strong definition.** `CrNick`, `nick_ext` and the strong `doomo_save_ext_extra` are in `creator.c` (P4's in Phase B). P0.M1's LOOK ext and P4.M1's removal land in one window.
- **CF-42. `prims.c` and `cdread.c`.** `ps1/src/prims.c` is H3's (`prims.c:1-2`) and was listed under P3. `cdread.c` would duplicate S2's `txtload.c` CD reader.
- **Also added as hunks:**
  - H2: `gba/Makefile` targets for `creator_scroll.txt` and `tp_looks.txt` (the scripts are listed explicitly; story-SPEC C2);
  - a salon location for "salon makeovers";
  - `plat_model_preview()` (P4's T18): P0 declares it in `platform.h` as a weak stub in M0, P3 implements it in `model.c`.

### 9.6 Checked and correct

These held against the tree and need no change:
- the gene layout is contiguous `0..384` (`critique/gene_check.py`), and every v1 value and every 4.2 list fits its width (`critique/counts.c`: `SPECIES` 24, `MARK` 23, `ACC` 20, `HOBBY` 32, `MUSIC` 16, `CATCHPHRASE` 64; `MOOD` 14, `ACT` 13, `DEATH` 6);
- the sizes: `DoomoResident` 76, `DoomoLook` 64, `DoomoCitizen` 112, `DoomoSettings` 416, the 4-B pool run header;
- the GBA figures from `gba/build/doomogachi.map`: EWRAM 7,104 free, IWRAM 25,680 / 6,320 stack, ROM 3,076,700, and the top users;
- the PS1 figures from `ps1/build/doomogachi.map`: `_end` 0x801E4FD0 (110,624 B below `0x801FFFF0`); `roster_premade` 96,000, bios 44,931, jobs 8,056, edges 9,258, SFX 47,793;
- the flash layout (`SLOT0_LEN 0x0A000`, trailer at `0x09FF4`) and the power-cut safety of the trailer move;
- the creator unlock ids 269 and 278..319 are free; the 14 buttons fit `remap[3][3][7]`;
- the sim reads only species, halo and accessory from looks;
- Appendix A: 304 fields, 68 colour, 91 chibi, waves 47/233/23;
- Appendix B: 458 options (269 G / 189 I; waves 355/90/9/4); category totals; the hunk list sums 174;
- the 2.2 arithmetic, as written, before the corrections above.

