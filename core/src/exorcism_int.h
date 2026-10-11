/* exorcism_int.h - private glue between exorcism_scene.c (the ritual) and
 * exorcism_gfx.c (St. Cuddles church, frames, candles, the demon) (owner:
 * life). Not part of the public contract. */
#ifndef DOOMO_EXORCISM_INT_H
#define DOOMO_EXORCISM_INT_H

#include "doomo/doomo.h"

/* "dread" 0..15: 0 = sugary pastel church, 15 = blood-red nightmare. */
void exo_church(int dread, uint32_t t, int floor_y);
/* Father Fuzz standing with his feet at (cx, foot_y). */
void exo_fuzz(int cx, int foot_y, uint32_t t, int expr);
/* The host's framed portrait centred at (cx, cy); shake 0..6 px;
 * `img` may be NULL (empty "?" frame). Returns the frame's outer w/h. */
void exo_host_frame(int cx, int cy, const DoomoImage *img, int scale, int shake,
                    int dread, uint32_t t, int *out_w, int *out_h);
/* A row of `total` candles, `lit` of them burning, centred at (cx, y). */
void exo_candles(int cx, int y, int lit, int total, uint32_t t, int big);
/* The darting vape demon at (x, y) (centre); flash = hit flash ticks. */
void exo_demon(int x, int y, uint32_t t, int flash, int scale);
/* Speech bubble with a tail pointing at (tx, ty). */
void exo_bubble(int x, int y, int w, const char *text, int tx, int ty, int creepy);
/* Lerped colour for this dread level (cached). */
uint8_t exo_col(int which, int dread);
enum { EC_WALL_HI, EC_WALL_LO, EC_STRIPE, EC_STONE_HI, EC_STONE, EC_STONE_LO,
       EC_FLOOR_A, EC_FLOOR_B, EC_CARPET, EC_CARPET_HI, EC_GLASS_PINK,
       EC_GLASS_SKY, EC_GLASS_LEMON, EC_GLASS_MINT, EC_GLASS_LAV, EC_BEAM,
       EC_WOOD_HI, EC_WOOD, EC_WOOD_LO, EC_LEAD, EC_COUNT };

#endif
