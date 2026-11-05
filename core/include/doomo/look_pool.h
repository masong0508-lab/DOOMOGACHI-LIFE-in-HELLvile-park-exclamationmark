/* look_pool.h - contract for the deep-look pool (customisation v2, SPEC 1.5).
 * P0.M0a lands the layout only: nothing allocates or saves a pool yet, and
 * the functions are weak stubs until M1 (the real look_pool_find/put/drop, the LOOK chunk). */
#ifndef DOOMO_LOOK_POOL_H
#define DOOMO_LOOK_POOL_H

#include "types.h"
#include "save.h"

#define LOOK_POOL_BYTES 8064u            /* + 8 B header = LOOK chunk <= 8,072 B */
#define LOOK_ID_BOOK0   1001u            /* Look Book styles 1001..1016          */
#define LOOK_ID_DRAFT   1023u            /* creator scratch, never saved         */
#define LOOK_STORY_CAP  4096u            /* pool bytes usable in story mode      */
enum { RUN_DEEP, RUN_NICK, RUN_NAME };

typedef struct DoomoLookPool {
    uint16_t used, runs, gen, version;   /* gen bumps on every change            */
    uint8_t  has[128];                   /* bit per id 0..1023                   */
    uint8_t  buf[LOOK_POOL_BYTES];       /* runs sorted by (id, kind)            */
} DoomoLookPool;
DOOMO_STATIC_ASSERT(sizeof(DoomoLookPool) == 8200, look_pool_is_8200_bytes);
DOOMO_STATIC_ASSERT(LOOK_POOL_BYTES + 8 <= DOOMO_SAVE_EXT_MAX, look_pool_fits_one_chunk);

/* The pool API (weak stubs in stubs.c until M1 allocates a pool). A run is the bytes of one kind
 * (RUN_*) for one id. These shapes are provisional: M1 may refine them before anyone calls them. */
const uint8_t *look_pool_find(int id, int kind, int *len);        /* NULL if there is no such run */
int  look_pool_put(int id, int kind, const uint8_t *data, int len); /* 1 stored, 0 pool full       */
void look_pool_drop(int id);                                      /* every run of one id          */

#endif
