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
    DF_COUNT
};

#define DFF_CHIBI 1u     /* resolves for chibis (and is part of the chibi cache key) */
#define DFF_ANIM  2u     /* the field animates: it asks for effect frames            */

typedef struct { uint8_t v[DF_COUNT]; } DoomoDeep;       /* resolved choices, 0 = as today */

extern const uint8_t doomo_df_flags[DF_COUNT];
extern const DoomoDeep doomo_deep_zero;

#define LV_ALL   0u
#define LV_CHIBI 1u

void look_resolve(const DoomoResident *r, int id, DoomoDeep *out);
void look_resolve_mask(const DoomoResident *r, int id, uint32_t lv, DoomoDeep *out);

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
