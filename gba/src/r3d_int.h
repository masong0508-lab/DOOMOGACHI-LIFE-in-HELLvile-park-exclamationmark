/* r3d_int.h - internals of the GBA height raycaster (owner: gba).
 *
 * Pipeline of plat_view3d() (see r3d.c for the overview):
 *   r3d.c        THUMB  camera setup, colormaps, texture cache, sprites
 *   r3d.iwram.c  ARM    column pass (DDA, step faces, walls, sky),
 *                       visplanes -> horizontal spans, sprite columns
 */
#ifndef GBA_R3D_INT_H
#define GBA_R3D_INT_H

#include "gba_plat.h"
#include "doomo/map.h"
#include "doomo/platform.h"

#define R3D_MAXCOLS   120          /* 2-pixel-wide columns              */
#define R3D_MAXROWS   160
#define R3D_MAXCP     14           /* sprite clip checkpoints / column  */
#define R3D_KROWS     288          /* |row - horizon| range of rowK      */
#define R3D_FADE_N    128          /* fade buckets of 32 world units     */
#define R3D_MAXTEX    64
#define R3D_MAXDIST   40           /* max ray length in cells            */

/* Window checkpoint: after the ray passed distance d8 (1/8 world units),
 * the still-open rows of the column were [top, bot). Sprites at depth d
 * are clipped by the last checkpoint with d8 <= d. */
typedef struct R3DClip {
    uint16_t d8;
    uint8_t  top, bot;
} R3DClip;

typedef struct R3DFrame {
    /* camera (16.16 world units unless noted) */
    int32_t ex, ey, ez;
    int32_t ez4;                   /* eye z in 1/16 units                */
    int32_t fx, fy, rx, ry;        /* forward / right unit vectors       */
    int32_t rxpp, rypp;            /* rx/proj, ry/proj in 0.32           */
    int32_t hor;                   /* horizon row (viewport relative)    */
    int32_t hor12;                 /* hor << 12                          */
    int32_t proj;                  /* focal length in pixels             */
    uint32_t invproj;              /* 2^32 / proj                        */
    int32_t vw, vh, ncol;
    int32_t halfw;                 /* vw / 2                             */
    uint16_t *dst;                 /* viewport (0,0) in the back page    */
    /* map */
    const DoomoMap *map;
    const DoomoCell *cells;
    int32_t mw, mh;
    uint32_t maxd;                 /* max DDA distance (16.16 cells)     */
    int32_t maxtop4;               /* tallest floor/ceiling/top, 1/16 u. */
    /* textures resolved for this frame (lazily) */
    uint16_t time;
    uint8_t  sky_id;
    uint8_t  pad0;
    const uint8_t *sky;            /* 256 x 96                           */
    int32_t skyang;
    /* colormaps: cm = plain (IWRAM), cmt = tinted (EWRAM) or 0 */
    const uint8_t *cm;
    const uint8_t *cmt;
    uint32_t tint_thresh;          /* columns/rows with dither < thresh tinted */
    uint32_t frame;
    /* stats */
    uint16_t nplanes, nspans;
    uint16_t cp_over, plane_over;
    uint32_t dbg_steps, dbg_facepx, dbg_skypx, dbg_flatpx;
} R3DFrame;

extern R3DFrame r3f;
extern uint8_t  r3d_cmap[16 * 256];        /* IWRAM */
extern int32_t  r3d_rowk[R3D_KROWS];       /* proj / (m + 0.5), 16.16 */
extern uint8_t  r3d_fade[R3D_FADE_N];
extern int32_t  r3d_colt[R3D_MAXCOLS];     /* camera-plane t per column, 16.16 */
extern int16_t  r3d_colang[R3D_MAXCOLS];   /* angle offset per column */
extern uint8_t  r3d_dither[R3D_MAXCOLS];   /* 0..15 ordered dither by column */
extern const uint8_t *r3d_texc[R3D_MAXTEX]; /* per-frame texture cache (IWRAM) */
extern uint8_t r3d_avgc[R3D_MAXTEX];        /* texture average colours (IWRAM) */

extern R3DClip  r3d_clips[R3D_MAXCOLS][R3D_MAXCP];
extern uint8_t  r3d_nclips[R3D_MAXCOLS];

extern const int32_t  gba_sin16[5120];
extern const uint32_t gba_recip8[32768];

/* texture lookup with per-frame cache (r3d.c, THUMB). Never NULL. */
const uint8_t *r3d_texture(int id) ROM_CALL;
uint8_t r3d_texavg(int id) ROM_CALL;
/* door / lift aware heights for flagged cells, world units (r3d.c) */
int r3d_cell_floor(int cx, int cy, const DoomoCell *c) ROM_CALL;
int r3d_cell_ceil(int cx, int cy, const DoomoCell *c) ROM_CALL;

/* hand-written ARM pixel loops (r3d_loops.s, IWRAM; call from IWRAM only) */
void r3d_face_loop(uint16_t *p, int n, const uint8_t *texcol, int32_t v,
                   int32_t dv, const uint8_t *cm);
void r3d_sky_loop(uint16_t *p, int n, const uint8_t *col, int v2, const uint8_t *cm);
void r3d_span_loop(uint16_t *dst, int n, uint32_t pos, uint32_t step,
                   const uint8_t *tex, const uint8_t *cm);
void r3d_fill_loop(uint16_t *p, int n, uint32_t c2, int stride_bytes);

/* floor / ceiling column cast (layout fixed by r3d_loops.s) */
typedef struct R3DFlat {
    uint16_t *p;
    int32_t  n;
    const int32_t *kp;             /* &r3d_rowk[m] of the first row       */
    int32_t  kdir;                 /* +4 floors, -4 ceilings              */
    uint32_t u0, v0;               /* eye x / y, 26 fractional bits       */
    int32_t  A, B;                 /* rdx * hd, rdy * hd, 10 frac bits    */
    const uint8_t *tex;
    const uint8_t *cm;             /* 16 x 256 colormap (maybe tinted)    */
    int32_t  L, hd;                /* sector light, |eye - plane| units   */
    const uint8_t *fade;
    uint32_t far;                  /* distance for average colour, 24.8   */
    uint32_t avg;                  /* texture's average texel             */
} R3DFlat;
void r3d_flat_loop(R3DFlat *f);

/* DDA state for r3d_walk() (layout fixed by r3d_loops.s) */
typedef struct R3DWalk {
    uint32_t sdx, sdy, ddx, ddy;   /* 16.16 cells                         */
    int32_t  cx, cy;               /* steps left before leaving the map   */
    int32_t  idx8, ix8, iy8;       /* byte offsets: current cell, x, y    */
    const uint32_t *cells;
    uint32_t c0, c1;               /* raw words of the current region     */
    uint32_t maxd;
    uint32_t d, w0, w1;            /* out: boundary distance, next cell   */
    int32_t  side;                 /* out: 0 = x step, 1 = y step         */
} R3DWalk;
int r3d_walk(R3DWalk *w);          /* 0 = cell differs, 1 = left map, 2 = far */

/* roof height (x 8 units) of each open roofed cell seen from the sky
 * (max explicit top of its solid neighbours), 127 = none (r3d.c) */
extern int8_t r3d_roof[DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H];

/* ARM / IWRAM entry points (r3d.iwram.c) */
void r3d_columns(void) IWRAM_CALL;

typedef struct R3DSprite {
    const DoomoImage *img;
    int32_t  depth16;              /* view depth, 16.16                  */
    int32_t  x0_12;                /* screen x of the left edge, 20.12   */
    int32_t  ytop12;               /* screen y of the top edge, 20.12    */
    uint32_t step16;               /* texels per screen pixel, 16.16     */
    int32_t  w12, h12;             /* screen size in pixels, 20.12       */
    uint8_t  level, flags, trans, pad;
    int32_t  shadow_y12;           /* floor y under the feet (shadow)     */
    int32_t  shadow_hh;            /* shadow ellipse half height, pixels  */
} R3DSprite;
void r3d_draw_sprite(const R3DSprite *s) IWRAM_CALL;

static inline int32_t r3d_k8(uint32_t d16)
{
    /* pixels per world unit at distance d16, 24.8 */
    uint32_t i = d16 >> 13;
    if (i > 32767) i = 32767;
    return (int32_t)(((uint32_t)r3f.proj * gba_recip8[i]) >> 15);
}

#endif
