/* save_ext.c - registry of save extension chunks (see save_ext.h). */
#include "doomo/save_ext.h"

__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_studio = 0;
__attribute__((weak)) const DoomoSaveExt *const doomo_save_ext_extra = 0;

static const DoomoSaveExt *slot(int i) {
    switch (i) {
    case 0: return doomo_save_ext_studio;
    case 1: return doomo_save_ext_extra;
    default: return 0;
    }
}

int save_ext_count(void) {
    int i, n = 0;
    for (i = 0; i < 2; i++) if (slot(i)) n++;
    return n;
}

const DoomoSaveExt *save_ext_at(int i) {
    int k;
    for (k = 0; k < 2; k++) {
        const DoomoSaveExt *e = slot(k);
        if (!e) continue;
        if (i-- == 0) return e;
    }
    return 0;
}

void save_ext_reset_all(void) {
    int i, n = save_ext_count();
    for (i = 0; i < n; i++) {
        const DoomoSaveExt *e = save_ext_at(i);
        if (e && e->reset) e->reset();
    }
}
