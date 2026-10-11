/* game.h - entry points the platform calls. */
#ifndef DOOMO_GAME_H
#define DOOMO_GAME_H

#include "types.h"

void doomo_init(void);                 /* once, after hardware init        */
void doomo_frame(uint32_t buttons);    /* one 30 Hz tick: update + draw    */
uint32_t doomo_frame_count(void);

/* ---- foundation internals ------------------------------------------------ */
/* Builds doomo_colormap / doomo_tintmap at runtime from doomo_palette when
 * the generated art tables are missing (stubs.c). Slow on hardware; only
 * used before the art module has landed. */
void doomo_palette_fallback(void);
/* 1 once doomo_init() has run. */
int  doomo_initialized(void);

#endif
