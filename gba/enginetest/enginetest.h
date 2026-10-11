/* enginetest.h - GBA ENGINE_TEST build (owner: gba). */
#ifndef GBA_ENGINETEST_H
#define GBA_ENGINETEST_H

#include "doomo/map.h"

#define ET_MAP_W 32
#define ET_MAP_H 24
#define ET_LIFT_X 29                    /* 'F' in et_map.c */
#define ET_LIFT_Y 6
#define ET_DOOR_X 5                     /* the house door 'D' */
#define ET_DOOR_Y 8

void et_build_map(DoomoMap *m);
void et_save_test(void);
void et_div_test(void);

#endif
