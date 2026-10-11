/* ow_int.h - overworld internals shared by overworld.c (THUMB) and
 * ow.iwram.c (ARM pixel loops) (owner: gba). */
#ifndef GBA_OW_INT_H
#define GBA_OW_INT_H

#include "gba_plat.h"

typedef struct OwFrame {
    int ox, oy, ow, oh;            /* viewport (screen pixels)          */
    int scrollx, scrolly;          /* world pixel at the viewport origin */
    int day_level;                 /* colormap level for daylight        */
    int night;                     /* 0..16: night tint dither density   */
    int tint_level;                /* 0..15: view tint dither density    */
    const uint8_t *night_map, *tint_map;
    uint16_t time;
} OwFrame;

extern OwFrame owf;
extern const uint8_t ow_bayer[16];

/* copy src rows (stride sstride) to screen x [x0, x1), rows [y0, y1) */
void ow_blit(int x0, int x1, int y0, int y1, const uint8_t *src, int sstride) IWRAM_CALL;
/* ground rows of one cell row from lit tiles (see ow.iwram.c) */
void ow_ground(const uint8_t *const *tiles, int ntiles, int o, int y0, int y1, int r0) IWRAM_CALL;
void ow_out(uint16_t *dst, const uint8_t *src, int n);   /* ow_loops.s */
void ow_tile16(uint16_t *dst, const uint8_t *src, int rows);   /* ow_loops.s */
/* block front face rows [y0, y1) of h, top row on screen at sy0 */
void ow_front(int px, int sy0, int h, int y0, int y1, const uint8_t *tp, uint8_t avg,
              const uint8_t *cm) IWRAM_CALL;
/* darken screen x [x0, x1), rows [y0, y1) through cm */
void ow_shade(int x0, int x1, int y0, int y1, const uint8_t *cm) IWRAM_CALL;
/* sprite at screen (x0, y0), integer scale s, BB_* flags, lit through cm,
 * then the night / view tint dither, clipped to the viewport */
void ow_sprite(const DoomoImage *img, int x0, int y0, int s, int flags,
               const uint8_t *cm) IWRAM_CALL;

#endif
