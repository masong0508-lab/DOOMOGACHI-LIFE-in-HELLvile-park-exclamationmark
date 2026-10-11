/* save_ext.h - optional extension chunks appended to save files
 * (owner: foundation; registry pattern like activities/unlocks).
 *
 * Modules that need to persist extra data (e.g. the Music Studio's song
 * book) register a DoomoSaveExt. save.c writes every registered chunk after
 * the core town data as [tag u32][version u16][len u32][bytes], inside the
 * same compression + CRC. On load, known tags are handed to read(); unknown
 * tags are skipped (forward compatible); registered chunks that are missing
 * from an old save get reset(). Chunks count toward the slot size limits.
 */
#ifndef DOOMO_SAVE_EXT_H
#define DOOMO_SAVE_EXT_H

#include "types.h"

#define SAVE_EXT_TAG(a, b, c, d) \
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

typedef struct DoomoSaveExt {
    uint32_t tag;                                   /* SAVE_EXT_TAG('S','O','N','G') */
    uint16_t version;
    uint32_t (*size)(void);                         /* exact bytes write() emits */
    uint32_t (*write)(uint8_t *dst, uint32_t cap);  /* returns bytes written */
    int      (*read)(const uint8_t *src, uint32_t len, uint16_t version); /* 1 ok */
    void     (*reset)(void);                        /* defaults (new town / missing) */
} DoomoSaveExt;

/* Registry: weak NULL defaults in save_ext.c; modules define strong ones. */
extern const DoomoSaveExt *const doomo_save_ext_studio;
extern const DoomoSaveExt *const doomo_save_ext_extra;
/* (foundation) a slot for core/tests only - never define it in a module */
extern const DoomoSaveExt *const doomo_save_ext_test;
int save_ext_count(void);
const DoomoSaveExt *save_ext_at(int i);
void save_ext_reset_all(void);   /* called by town_new() */

#endif
