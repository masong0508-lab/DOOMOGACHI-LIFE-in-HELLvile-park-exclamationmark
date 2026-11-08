/* exorcism_int.h - private glue between exorcism_scene.c (the ritual) and
 * exorcism_gfx.c (St. Cuddles church, frames, candles, the demon) (owner:
 * life). Not part of the public contract. */
#ifndef DOOMO_EXORCISM_INT_H
#define DOOMO_EXORCISM_INT_H

#include "doomo/doomo.h"
#include "doomo/backdrop.h"

/* "dread" 0..15: 0 = sugary pastel church, 15 = blood-red nightmare.
 * exo_church draws it all: the backdrop for (dread, t), then every live part. */
void exo_church(int dread, uint32_t t, int floor_y);
/* SP7 (docs/EXORCISM_SPEED.md): the church as a kept backdrop (backdrop.h) plus live parts.
 * exo_church_key: the backdrop key at (dread, t); exo_church_backdrop(key) paints it over the
 * whole screen; exo_church_keys lists every key (the GBA bakes them into the ROM). The live
 * parts go over it in any order: the rose window's heart, the swaying lower curtains and the
 * seven dust motes. exo_church_part gives a part's box (x, y, w, h; w = 0: nothing drawn) and
 * the key of its pixels; exo_church_part_draw draws it. */
uint32_t exo_church_key(int dread, uint32_t t);
void exo_church_backdrop(uint32_t key);
int  exo_church_keys(uint32_t *out, int cap);
enum { EXO_PART_HEART, EXO_PART_CURTAIN_L, EXO_PART_CURTAIN_R, EXO_PART_MOTE0, EXO_MOTES = 7,
       EXO_PARTS = EXO_PART_MOTE0 + EXO_MOTES };
int  exo_church_part(int part, int dread, uint32_t t, int16_t box[4], uint32_t *key);
void exo_church_part_draw(int part, int dread, uint32_t t);
extern const struct DoomoBackdrop exo_church_family;
/* SP7: on a fill-bound screen (GBA, native) a shape drawn between exo_hole() and exo_hole_off()
 * leaves out the hole [x0, x1) x [y0, y1), and exo_shade_hole() shades its rect less the hole:
 * only for a hole that something drawn later is sure to paint over (the same pixels, fewer
 * writes). Elsewhere (PS1, Dreamcast: per-primitive cost) they draw the whole shape. */
void exo_hole(int x0, int y0, int x1, int y1);
void exo_hole_off(void);
void exo_shade_hole(int x, int y, int w, int h, int level, int hx0, int hy0, int hx1, int hy1);
/* Father Fuzz standing with his feet at (cx, foot_y); _image is his figure (a portrait
 * cache image: fetch it once per frame), _draw draws it. */
void exo_fuzz(int cx, int foot_y, uint32_t t, int expr);
const DoomoImage *exo_fuzz_image(int expr);
void exo_fuzz_draw(int cx, int foot_y, uint32_t t, const DoomoImage *img);
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
