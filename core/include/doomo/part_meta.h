/* part_meta.h - the creator's part catalog (CR1) and the age gate for parts (CR12).
 *
 * Source: core/data/parts/catalog/ (one text file per slot, plus sets, tags, hides, costs and the
 * plan's targets), compiled by tools/partcat_build.py into core/gen/part_meta_data.c (names, slots,
 * sets, tags, method, state, cost weight, a sorted name index) and core/gen/part_age_data.c (the
 * parts whose age class is not ALL). docs/CREATOR.md "The part catalog" has the format.
 *
 * A PART is one value of one list: (list, value). The LIST id is
 *   - a gene field's GF_* id (gene.h) for the look's own fields (hat, hair, pupil, glasses ...),
 *   - PL_SPECIES for the species,
 *   - PL_DEEP0 + DF_x for a deep field (look_fields.h; provisional shim ids until P0 lands).
 * The same ids name part scripts (parts.h) and, later, CLIST_LF unlocks.
 *
 * Memory: everything here is const (ROM). The PS1 links part_meta.c / part_meta_data.c into the
 * creator overlay (only the creator and tools read names); part_age.c / part_age_data.c stay
 * resident because the renderer and the generators call part_age_ok(). */
#ifndef DOOMO_PART_META_H
#define DOOMO_PART_META_H

#include "types.h"
#include "../../gen/part_meta_ids.h"

#define PL_SPECIES 127          /* list id of the species ("hot byte", not a gene field)    */
#define PL_DEEP0   128          /* list id of deep field 0                                   */
#define PL_IS_DEEP(l) ((l) >= PL_DEEP0)

enum { PAGE_ALL, PAGE_TEEN, PAGE_ADULT };                 /* age class: who may wear it         */
enum { PSTATE_LIVE, PSTATE_HIDDEN, PSTATE_PLANNED, PSTATE_TEST };
enum { PMETH_P, PMETH_S, PMETH_F, PMETH_T, PMETH_C };     /* painter, script, family, tool data,
                                                             code variant                       */

/* PartMeta.flags */
#define PMF_AGE(f)     ((f) & 3)
#define PMF_STATE(f)   (((f) >> 2) & 3)
#define PMF_METHOD(f)  (((f) >> 4) & 7)
#define PMF_SCRIPTED   0x80    /* drawn by a part script (core/data/parts/<dir>/)              */

typedef struct PartMeta {
    uint16_t list;      /* list id (above)                                                     */
    uint8_t  value;     /* the choice                                                          */
    uint8_t  set;       /* doomo_part_sets[] index: every part is in exactly one set           */
    uint8_t  slot;      /* doomo_part_slots[] index                                            */
    uint8_t  flags;     /* PMF_*                                                               */
    uint16_t hides;     /* bit i = doomo_part_hide_keys[i] ("hair", "mouth" ...)                */
    uint32_t tags;      /* bit i = doomo_part_tag_keys[i] (PTAG_* in part_meta_ids.h)           */
    uint16_t name;      /* offset into doomo_part_names                                        */
    uint8_t  cost;      /* draw cost weight: ~64 pixel visits per unit over the slot's value 0  */
    uint8_t  shape;     /* index into the slot's shapes (the grid's shape chips), 0xFF none     */
} PartMeta;

typedef struct PartSlotInfo {
    const char *key;    /* "hat"                                                               */
    const char *name;   /* "Hat"                                                               */
    uint16_t list;      /* list id                                                             */
    uint16_t first;     /* first doomo_part_meta[] row (rows are sorted by slot, then value)    */
    uint16_t count;     /* rows                                                                 */
    uint8_t  maxv;      /* the largest value the field can hold (gene width; deep fields 127)   */
    uint8_t  live;      /* rows that are PSTATE_LIVE                                            */
    uint8_t  method;    /* default PMETH_*                                                      */
    uint16_t target;    /* new parts the creator plan budgets for this slot (tools/part_budget) */
    uint8_t  nshapes;
    const char *const *shapes;
    uint8_t  gate;      /* CR3: the shape gate's minimum bust difference for its scripted parts (px) */
} PartSlotInfo;

typedef struct PartSetInfo {
    const char *key, *name, *blurb;
    uint8_t theme;      /* tag bit of the set's theme                                          */
    uint8_t reward;     /* 1: the set's new parts open through an unlock (CR11)                 */
} PartSetInfo;

typedef struct PartAgeRow { uint16_t list; uint8_t value, age; } PartAgeRow;

extern const char doomo_part_names[PART_NAME_BYTES];
extern const PartSlotInfo doomo_part_slots[PART_SLOT_COUNT];
extern const PartMeta doomo_part_meta[PART_META_COUNT];
extern const uint16_t doomo_part_name_order[PART_META_COUNT];   /* rows sorted by name (A-Z) */
extern const PartSetInfo doomo_part_sets[PART_SET_COUNT];
extern const char *const doomo_part_tag_keys[PART_TAG_COUNT];
extern const char *const doomo_part_tag_names[PART_TAG_COUNT];
extern const char *const doomo_part_hide_keys[PART_HIDE_COUNT];
extern const PartAgeRow doomo_part_age_rows[];
extern const uint16_t doomo_part_age_count;
extern const uint8_t doomo_part_live[PART_GENE_FIELDS];          /* part_age_data.c (resident) */

/* ---- the catalog (part_meta.c; creator side) ---------------------------------------------- */
const PartMeta *part_meta_find(int list, int value);          /* NULL if not catalogued        */
const PartSlotInfo *part_slot_of_list(int list);              /* NULL if no slot has the list  */
const char *part_meta_name(const PartMeta *m);                /* "" for NULL                   */
const char *part_name_of(int list, int value);                /* "" if not catalogued          */
const PartSetInfo *part_set_info(int set);                    /* NULL if out of range          */
int  part_set_size(int set, int live_only);                   /* parts in the set              */
/* Rows of slot `slot` whose name or set name contains `letters` (case-insensitive), A-Z order,
 * at most `max` written to out[] (row indices); returns how many matched in all. letters "" or
 * NULL matches every row. live_only skips hidden / planned / test rows. */
int  part_search(int slot, const char *letters, int live_only, uint16_t *out, int max);
/* 1 if part m's name or its set's name contains `letters` (case-insensitive; "" matches all). */
int  part_matches(const PartMeta *m, const char *letters);

/* ---- live counts (CR3; part_age.c, resident) ------------------------------------------------- */
/* How many choices of gene list `gf` (a GF_* id) are live: its values 0..N-1 are selectable, saved
 * and kept by resident_sanitize. 0 if the field is not a catalogued part list. The same on every
 * platform (a save never depends on the build that wrote it). */
int  part_live_count(int gf);
/* ... of which this build can DRAW: the leading values up to the list's first part script. The
 * PS1 builds the renderer without the part-script VM until CR10 (DOOMO_PARTS_VM=0, docs/PORTRAIT.md),
 * and every scripted value is appended after the painted ones, so there the creator offers only the
 * painted values. Elsewhere it equals part_live_count(). */
int  part_live_drawable(int gf);

/* ---- the age gate (part_age.c; resident) ---------------------------------------------------- */
/* PAGE_* of (list, value): PAGE_ALL unless the catalog says teen or adult. */
int  part_age_class(int list, int value);
/* 1 if a citizen at life stage `stage` (LIFE_*) may wear (list, value): TEEN+ parts need a teen,
 * an adult or an elder; ADULT parts need an adult or an elder. */
int  part_age_ok(int list, int value, int stage);
/* The look to DRAW at `stage` with every gene part above the stage's age class swapped for its
 * slot's default (value 0). Returns how many fields were swapped. Pure. */
struct DoomoLook;
int  part_age_filter_look(struct DoomoLook *l, int stage);
/* The same for a resident record (look fields in the gene; the life stage is the resident's own
 * unless stage >= 0). Returns how many fields were swapped. */
struct DoomoResident;
int  part_age_filter_resident(struct DoomoResident *r, int stage);

#if defined(DOOMO_PART_AGE_TEST)
/* Host tests only: pretend (list, value) has age class `age` (PAGE_*); age < 0 clears. No adult
 * or teen part exists yet, so the gate is tested through this. */
void part_age_test_set(int list, int value, int age);
#endif

#endif
