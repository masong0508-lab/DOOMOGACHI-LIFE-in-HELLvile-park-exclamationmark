/* look_fields.h - PROVISIONAL deep-look fields (customisation v2, P1.M1 shim).
 *
 * Stand-in for P0's contract (SPEC 1.2) so P1 can build and test its half: a handful of face
 * fields, the resolved set (DoomoDeep), and look_resolve*() over the in-RAM run pool
 * (src/look_pool.c). NOTHING here is saved or loaded; no save byte changes. When P0 lands, delete
 * this header and src/look_pool.c and take P0's; P1 code only uses DF_* ids, DoomoDeep.v[],
 * DFF_CHIBI / DFF_ANIM, look_resolve_mask and `c->d->v[DF_x]`.
 *
 * A deep RUN (RUN_DEEP) is a list of (field, value) byte pairs; the value's low 7 bits are the
 * choice. A choice of 0 is "as today": it draws nothing new. */
#ifndef DOOMO_LOOK_FIELDS_H
#define DOOMO_LOOK_FIELDS_H

#include "types.h"
#include "resident.h"

enum {
    DF_PUPIL, DF_SCLERA, DF_BLUSH2, DF_GLASSES2, DF_EAR_FOLD, DF_HOOD_HEAD, DF_LASH, DF_LID,
    DF_LINER, DF_BAGS, DF_FRECKLE, DF_MOLE, DF_SCAR2, DF_MAKEUP, DF_FACEPAINT, DF_AURA2,
    /* CR3 (CRC): the gene's new core fields that have no DoomoLook byte ride in the resolved set
     * too (look_gene_deep below). The wave-2 painters above already read DF_PUPIL, DF_SCLERA,
     * DF_BLUSH2, DF_GLASSES2, DF_EAR_FOLD and DF_HOOD_HEAD; these five are new. */
    DF_FUR_PATTERN, DF_FUR_COLOR3, DF_OUTER, DF_LEGWEAR, DF_HANDED,
    DF_COUNT
};

#define DFF_CHIBI 1u     /* resolves for chibis (and is part of the chibi cache key) */
#define DFF_ANIM  2u     /* the field animates: it asks for effect frames            */

typedef struct DoomoDeep { uint8_t v[DF_COUNT]; } DoomoDeep;   /* resolved choices, 0 = as today */

extern const uint8_t doomo_df_flags[DF_COUNT];
extern const DoomoDeep doomo_deep_zero;

/* DOOMO_GENE_EXT: the gene's new look fields (look_gene.c) are read, drawn and saved. On on every
 * platform since CR3 landed (the PS1 too: its painted values only, no part VM; the creator offers
 * what the build can draw, part_live_drawable). DOOMO_GENE_EXT=0 (a -D) compiles the whole
 * feature out of the renderer, the resident code and the save (the _d calls become the plain ones,
 * the look_gene_* calls vanish): kept as a switch for a platform that runs out of RAM; no build
 * uses it today and no test covers it. */
#ifndef DOOMO_GENE_EXT
#define DOOMO_GENE_EXT 1
#endif

#define LV_ALL   0u
#define LV_CHIBI 1u

/* The resolved set of resident `id`: the gene's own new core fields of `r` (look_gene_deep; none
 * when r is NULL), then the run pool's entries for `id` on top. */
void look_resolve(const DoomoResident *r, int id, DoomoDeep *out);
void look_resolve_mask(const DoomoResident *r, int id, uint32_t lv, DoomoDeep *out);

/* CR3 (CRC): the gene-backed choices of a record, no save change: pupil -> DF_PUPIL, sclera ->
 * DF_SCLERA, blush_style -> DF_BLUSH2, glasses -> DF_GLASSES2, ear_fold -> DF_EAR_FOLD, hood_up ->
 * DF_HOOD_HEAD (1 = "up", the wave-2 picture 2), fur_pattern, fur_color3, outer, legwear,
 * handedness -> their DF_ ids. Every other entry of `out` is left alone. With lv == LV_CHIBI only the
 * chibi-visible fields (DFF_CHIBI). A part above the record's life stage (part_age_ok) reads 0, as
 * look_for_stage() does for the look's own parts. */
#if DOOMO_GENE_EXT
void look_gene_deep(const DoomoResident *r, uint32_t lv, DoomoDeep *out);
/* The same into a record (the creator writes its choices back through the gene): only the gene
 * fields look_gene_deep reads. */
void look_gene_deep_put(DoomoResident *r, const DoomoDeep *d);
#else
#define look_gene_deep(r, lv, out) ((void)(r), (void)(lv), (void)(out))      /* nothing to read ... */
#define look_gene_deep_put(r, d) ((void)(r), (void)(d))                       /* ... or write (no code) */
#define look_gene_ext_size() 0u
#define look_gene_ext_write(dst, cap) ((void)(dst), (void)(cap), 0u)
#define look_gene_ext_read(src, len) ((void)(src), (len) == 0 || (len) >= 2)
#endif
/* FNV-1a of a resolved set, 0 when every choice is 0 (so a Classic look keeps its cache key). With
 * chibi != 0 only the DFF_CHIBI fields count. */
uint32_t deep_hash(const DoomoDeep *d, int chibi);
/* CR3: the town's gene bits the v1 save stream does not carry (the new fields, the high bit of aura,
 * print and eye count), as a trailer of the creator's 'NICK' chunk (format in look_gene.c).
 * size 0 = no record has any. read: 1 ok, 0 malformed (what could be read is applied). */
/* Put value v of part list `list` (part_meta.h: a GF_* id, PL_SPECIES or PL_DEEP0 + DF_x) on a look
 * you hold: a DoomoLook byte, or the resolved set for the gene-only and deep fields. 0 if the list
 * is neither. For tests, tools and contact sheets. */
int look_part_set(DoomoLook *l, DoomoDeep *d, int list, int v);
#if DOOMO_GENE_EXT
uint32_t look_gene_ext_size(void);
uint32_t look_gene_ext_write(uint8_t *dst, uint32_t cap);
int      look_gene_ext_read(const uint8_t *src, uint32_t len);
#endif

/* FNV-1a of resident `id`'s deep run, 0 when it has none or nothing in it counts. With chibi != 0
 * only DFF_CHIBI entries are hashed. Folded into the portrait cache's look hash. */
uint32_t look_run_hash(int id, int chibi);

/* One portrait bust drawn with the deep set `d` (portrait.h's portrait_render has none): for tests
 * and contact sheets. PORTRAIT_W x PORTRAIT_H bytes. */
void portrait_render_deep(const DoomoLook *l, const DoomoDeep *d, int status, int expr, int anim,
                          uint8_t *out);

/* LAM_FX if any DFF_ANIM field of `d` is set. */
uint32_t deep_anim_mask(const DoomoDeep *d);

#endif
