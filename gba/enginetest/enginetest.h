/* enginetest.h - GBA ENGINE_TEST build (owner: gba). */
#ifndef GBA_ENGINETEST_H
#define GBA_ENGINETEST_H

#include "doomo/map.h"

#define ET_MAP_W 32
#define ET_MAP_H 24

void et_build_map(DoomoMap *m);
void et_save_test(void);
void et_div_test(void);

#endif
