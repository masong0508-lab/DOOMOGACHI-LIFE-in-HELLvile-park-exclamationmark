/* ow_int.h - overworld internals shared by overworld.c (THUMB) and
 * ow.iwram.c (ARM pixel loops) (owner: gba). */
#ifndef GBA_OW_INT_H
#define GBA_OW_INT_H

#include "gba_plat.h"

/* (G2) A sprite's pixel goes through the colormap, then the night tint
 * where ow_bayer < night, then the view tint where ow_bayer < tint_level:
 * one of four classes per 4x4 screen position (bit 0 night, bit 1 tint).
 * ow_sprite composes the classes a frame uses into 256-byte tables, one
 * lookup per pixel, cached per colormap for the frame in plat_overworld's
 * stack frame (OW_LUT_TABLES tables, 2 KB, shared out among the classes:
 * 8 colormaps by day, 4 at night or under a view tint, 2 under both).
 * Outside plat_overworld owf.luts is NULL and ow_sprite uses the colormap
 * as it is: the caller (the house, room.c) sets night = tint_level = 0. */
#define OW_LUT_TABLES 8
typedef struct OwLuts {
    uint32_t tab[OW_LUT_TABLES][64];       /* way w, class slot s: [w * nc + s] (words: built 4 at a time) */
    const uint8_t *key[OW_LUT_TABLES];     /* the colormap a way holds */
    uint8_t ways, victim;
    /* (V4) what the cached tables were built for (ow_frame_luts_keep): the frame's night / tint */
    const uint8_t *s_nm, *s_tm;
    int8_t s_night, s_tint;
    uint8_t s_ok, dirty;           /* dirty: a table was built since the frame's start */
} OwLuts;

typedef struct OwFrame {
    int ox, oy, ow, oh;            /* viewport (screen pixels)          */
    int scrollx, scrolly;          /* world pixel at the viewport origin */
    int day_level;                 /* colormap level for daylight        */
    int night;                     /* 0..16: night tint dither density   */
    int tint_level;                /* 0..15: view tint dither density    */
    const uint8_t *night_map, *tint_map;
    uint16_t time;
    /* (G2) the classes: cls[(y & 3) * 4 + (x & 3)] of a screen pixel, the
     * table slot of each class present (nc of them), the table cache */
    uint8_t cls[16], slot[4], nc, pad;
    OwLuts *luts;
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
/* (G2) a sprite's rows for ow_spr_rows (ow_loops.s; the offsets are
 * .equ'd there and asserted in ow.iwram.c) */
typedef struct OwSprJob {
    uint16_t *dst;                 /* +0  the first row's first pixel pair */
    const uint8_t *src;            /* +4  its first texel                  */
    int32_t n;                     /* +8  pixels per row, even, > 0        */
    int32_t rows;                  /* +12 > 0                              */
    int32_t stride;                /* +16 texels per image row             */
    uint32_t phase;                /* +20 the first row's y & 3            */
    const uint8_t *q[4][4];        /* +24 [y & 3][k & 3]: pixel k's table  */
    uint32_t head;                 /* +88 0xFF: drop each row's first texel */
    uint32_t tail;                 /* +92 the mask of each row's last quad */
} OwSprJob;
/* (V4b) only ow.iwram.c (ARM, IWRAM) calls ow_spr_rows and ow_flip_row, IWRAM to IWRAM: a plain bl
 * (4 B a call), not the long_call sequence the ROM callers need (16 B a call with its literal) */
void ow_spr_rows(OwSprJob *j);
/* (V4b) d[k] = s[-k], k < n (n even, > 0), d word aligned, s any alignment: a mirrored row (ow_loops.s) */
void ow_flip_row(uint8_t *d, const uint8_t *s, int n);
/* (G2) set up owf's pixel classes from owf.night / owf.tint_level and
 * point owf.luts at L, emptied (overworld.c; plat_overworld per frame,
 * owtest.c per case) */
void ow_frame_luts(OwLuts *L);
/* (V4) the same, but the tables built for the same night / tint (levels and maps) as the last
 * frame stay valid (the colormaps are constant, so nothing else changes them): L (the frame's
 * working copy, in IWRAM: the pixel loops read it every pixel) is loaded from the backing copy
 * `store` (EWRAM, slow to read) when its signature matches, else emptied; ow_frame_luts_end(L, store)
 * saves L back when a table was built */
void ow_frame_luts_keep(OwLuts *L, OwLuts *store);
void ow_frame_luts_end(OwLuts *L, OwLuts *store);
/* (G2) lc[c]: colormap cm composed for each class c the frame uses
 * (overworld.c, THUMB; cached per frame in owf.luts) */
void owf_sprite_luts(const uint8_t *cm, const uint8_t **lc) IWRAM_CALL;
/* sprite at screen (x0, y0), integer scale s, BB_* flags, lit through cm,
 * then the night / view tint dither, clipped to the viewport */
void ow_sprite(const DoomoImage *img, int x0, int y0, int s, int flags,
               const uint8_t *cm) IWRAM_CALL;

/* overworld.c, for the tile mode (ow_tiles.c, G5): the lit 16x16 tile
 * (front 0) or 16 x front block face of lit_cached(), the texture frame
 * of tex this frame (tex normalised like lit_cached does), a cell's light
 * level and the night / tint state of the frame */
const uint8_t *ow_lit(int tex, int lvl, int front);
const uint8_t *ow_tex_frame(int *tex);
int  ow_cell_level(const DoomoCell *c);
uint32_t ow_tint_state(void);
const DoomoCell *ow_cell(const DoomoMap *m, int tx, int ty);
#define OW_BLOCK_H 10

#endif
