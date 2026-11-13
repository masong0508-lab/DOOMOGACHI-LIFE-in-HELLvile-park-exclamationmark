/* save_ext.c - registry of save extension chunks (see save_ext.h). */
#include "doomo/save_ext.h"

__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_studio = 0;
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_extra = 0;
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_test = 0;
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_freeplay = 0;
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_fishcook = 0;
/* --- S0: story + house chunks (SPEC R14) --- */
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_home = 0;
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_story2 = 0;
/* --- end S0 --- */
/* --- K0: LIFE, PLAY, ENDS, OPT2, PROF (LIFESIM_PLAN R1 + section 6);
 * owners LS1, PA1, E1, U0, MP0 --- */
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_life = 0;
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_playas = 0;
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_endings = 0;
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_opt2 = 0;
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_profiles = 0;
/* --- end K0 --- */
/* --- T0: 'TALK' (docs/TALK.md 6.8); owner T6 --- */
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_talk = 0;
/* --- LV0: 'BRCH' (levels plan 8.5); owner levels_rec.c --- */
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_breach = 0;

/* The registry order (a chunk's position is its bit in save.c's "seen"
 * mask and its place in the budget snapshot; add at the end only). A
 * table of the pointers' addresses, not a switch: LV0 added the 14th slot
 * and this form is smaller than the 13-case switch was. */
static const DoomoSaveExt *const *const slots[SAVE_EXT_SLOTS] = {
    &doomo_save_ext_studio, &doomo_save_ext_extra, &doomo_save_ext_test,
    &doomo_save_ext_freeplay, &doomo_save_ext_fishcook,
    &doomo_save_ext_home, &doomo_save_ext_story2,                       /* S0 */
    &doomo_save_ext_life, &doomo_save_ext_playas, &doomo_save_ext_endings,
    &doomo_save_ext_opt2, &doomo_save_ext_profiles,                     /* K0 */
    &doomo_save_ext_talk,                                               /* T0 */
    &doomo_save_ext_breach                                              /* LV0 */
};

/* The defaults above are weak CONSTANTS: without the volatile read GCC
 * folds their value (0) into this file and never sees a module's strong
 * definition (the registry then always looks empty). i is 0..SAVE_EXT_SLOTS-1. */
static const DoomoSaveExt *slot(int i) {
    return *(const DoomoSaveExt *const volatile *)slots[i];
}

int save_ext_count(void) {
    int i, n = 0;
    for (i = 0; i < SAVE_EXT_SLOTS; i++) if (slot(i)) n++;
    return n;
}

const DoomoSaveExt *save_ext_at(int i) {
    int k;
    for (k = 0; k < SAVE_EXT_SLOTS; k++) {
        const DoomoSaveExt *e = slot(k);
        if (!e) continue;
        if (i-- == 0) return e;
    }
    return 0;
}

void save_ext_reset_all(void) {
    int k;
    for (k = 0; k < SAVE_EXT_SLOTS; k++) {
        const DoomoSaveExt *e = slot(k);
        if (e && e->reset) e->reset();
    }
}
