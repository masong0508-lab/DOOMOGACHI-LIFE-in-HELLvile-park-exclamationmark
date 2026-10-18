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
    /* (S0, SPEC R14) the chunk's worst-case size() in bytes, for budgets and
     * the joint worst-case test (save_budget.h); 0 = not declared (size()
     * is then its own bound). Never above DOOMO_SAVE_EXT_MAX. */
    uint32_t max_size;
} DoomoSaveExt;

/* Registry: weak NULL defaults in save_ext.c; modules define strong ones. */
extern const DoomoSaveExt *const doomo_save_ext_studio;
extern const DoomoSaveExt *const doomo_save_ext_extra;
/* (freeplay, additive) the free play island's own chunk 'FPLY' (island
 * name, rent day, graveyard flowers; core/src/fp_common.c) */
extern const DoomoSaveExt *const doomo_save_ext_freeplay;
/* (fishcook, additive) Fishdex records, the cooler, recipe grades, today's
 * customers: chunk 'FISH' (core/src/fishcook.c) */
extern const DoomoSaveExt *const doomo_save_ext_fishcook;
/* (foundation) a slot for core/tests only - never define it in a module */
extern const DoomoSaveExt *const doomo_save_ext_test;
/* --- S0: story + house chunks (docs/design/story/SPEC.md R14) --- */
/* (H1) the dream house: chunk 'HOME' v2, worst case HOME_CHUNK_WORST
 * (3,432 B; home.h), core/src/home.c */
extern const DoomoSaveExt *const doomo_save_ext_home;
/* (S2) the story's second chunk 'STRY' v1, exactly 96 B (DoomoStry,
 * story.h), core/src/stry_save.c. Old saves have no STRY: its reset() runs
 * after the town body is decoded (save.c reads the chunks after the town
 * data and resets the missing ones last), so it can derive the episode
 * from doomo_town.story.quest (SPEC C19). */
extern const DoomoSaveExt *const doomo_save_ext_story2;
/* --- end S0 --- */
#define SAVE_EXT_SLOTS 7
int save_ext_count(void);
const DoomoSaveExt *save_ext_at(int i);
void save_ext_reset_all(void);   /* called by town_new() */

#endif
