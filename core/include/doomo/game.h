/* game.h - entry points the platform calls. */
#ifndef DOOMO_GAME_H
#define DOOMO_GAME_H

#include "types.h"

void doomo_init(void);                 /* once, after hardware init        */
void doomo_frame(uint32_t buttons);    /* one 30 Hz tick: update + draw    */
uint32_t doomo_frame_count(void);

#endif
