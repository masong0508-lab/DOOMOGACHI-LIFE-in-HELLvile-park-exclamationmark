/* art.h - procedural art toolkit used by the generators in core/art/.
 *
 * These files are compiled NATIVELY into core/tools/artgen (never into the
 * GBA/PS1 builds). Everything is integer-only and seeded, so the output
 * (core/gen/art_data.c) is bit-identical on every machine and compiler:
 * no libm, no floats, no time, no rand().
 *
 * Coordinate conventions
 *   textures: 64x64, wrap in x and y (seamless tiling)
 *   skies:    256x96, wrap in x
 *   sprites:  drawn on a scratch canvas around an ANCHOR at (0,0) = the
 *             feet / ground point, y grows DOWN (so heads have y < 0).
 *             Pixel (x,y) covers [x,x+1); x = 0 is the centre LINE, so a
 *             shape centred on x = 0 is exactly mirror-symmetric.
 */
#ifndef DOOMO_ART_H
#define DOOMO_ART_H

#include <stdint.h>
#include <string.h>
#include "doomo/palette.h"
#include "doomo/textures.h"
#include "doomo/sprites.h"

/* ======================================================================
 * integer math
 * ==================================================================== */
#define Q12 4096                    /* 1.0 in Q12                        */
#define ART_TURN 4096               /* angle units per full turn         */
int  art_sin(int a);                /* Q12, any int angle                */
int  art_cos(int a);
int  art_atan2(int y, int x);       /* 0..4095, 0 = +x, 1024 = +y        */
uint32_t art_isqrt(uint32_t v);
int  art_clamp(int v, int lo, int hi);
int  art_wrap(int v, int m);        /* positive modulo                   */
int  art_abs(int v);
int  art_smooth(int t);             /* smoothstep, Q12 -> Q12            */
int  art_lerp(int a, int b, int t); /* t in Q12                          */
int  art_dist(int dx, int dy);      /* euclidean length (integer)        */

/* ======================================================================
 * hashing & noise (all periodic noise tiles perfectly)
 * ==================================================================== */
uint32_t art_hash(uint32_t a, uint32_t b, uint32_t c);
/* tiny deterministic generator */
typedef struct { uint32_t s; } ArtRng;
void     art_seed(ArtRng *r, uint32_t seed);
uint32_t art_next(ArtRng *r);
int      art_rnd(ArtRng *r, int n);             /* 0..n-1             */
int      art_range(ArtRng *r, int lo, int hi);  /* lo..hi inclusive   */
/* value noise 0..255, lattice every `cell` px, repeats every `period` px
 * (period must be a multiple of cell). */
int  art_noise(int x, int y, int cell, int period, uint32_t seed);
/* x/y periods may differ (skies) */
int  art_noise2(int x, int y, int cell, int px, int py, uint32_t seed);
/* fractal sum of `oct` octaves starting at `cell`, halving each time */
int  art_fbm(int x, int y, int cell, int oct, int period, uint32_t seed);
int  art_fbm2(int x, int y, int cell, int oct, int px, int py, uint32_t seed);
/* periodic Worley / cellular noise over a grid x grid lattice that spans
 * `period` px. d1,d2: distance to nearest / 2nd nearest feature in 1/16 px.
 * id: hash of the nearest cell. (fx,fy): nearest feature point (px,
 * unwrapped relative to the query). */
typedef struct { int d1, d2; uint32_t id; int fx, fy; } ArtCell;
void art_worley(int x, int y, int grid, int period, uint32_t seed,
                ArtCell *out);
/* jitter: 0 = regular grid, 16 = fully random feature points */
void art_worley_j(int x, int y, int grid, int period, uint32_t seed,
                  int jitter, ArtCell *out);
int  art_bayer(int x, int y);       /* 4x4 ordered dither 0..15          */

/* ======================================================================
 * palette helpers (art_palette.c)
 * ==================================================================== */
void    art_palette_setup(void);    /* call once before any generator    */
void    art_rgb(uint8_t idx, int *r, int *g, int *b);
int     art_luma(uint8_t idx);
uint8_t art_darker(uint8_t idx, int steps);
uint8_t art_lighter(uint8_t idx, int steps);
uint8_t art_outline_of(uint8_t idx); /* dark, hue-matched outline colour  */
uint8_t art_nearest(int r, int g, int b);
/* generated tables (also written to art_data.c) */
extern uint8_t art_colormap[DOOMO_LIGHT_LEVELS][256];
extern uint8_t art_tintmap[TINT_COUNT][256];
extern uint8_t art_darker_tab[256];
extern uint8_t art_lighter_tab[256];

#define RP(ramp, s) PAL_RAMP(ramp, art_clamp((s), 0, 7))
#define PP(paint, s) PAL_PAINT(paint, art_clamp((s), 0, 2))

/* Value v (0..255) onto shades lo..hi of a ramp. dither != 0 applies a 4x4
 * ordered dither at (x, y) for smooth gradients. */
uint8_t art_ramp_v(int ramp, int lo, int hi, int v, int x, int y, int dither);

/* A material: up to 12 palette indices from dark to light. */
typedef struct { uint8_t n; uint8_t c[12]; } ArtMat;
ArtMat  art_mat_ramp(int ramp, int lo, int hi);
/* paint: extended with its family's darker/lighter steps -> 5 shades */
ArtMat  art_mat_paint(int paint);
ArtMat  art_mat_skin(int skin);
ArtMat  art_mat_of(int n, const uint8_t *c);
ArtMat  art_mat2(uint8_t a, uint8_t b);
ArtMat  art_mat3(uint8_t a, uint8_t b, uint8_t c);
ArtMat  art_mat4(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
ArtMat  art_mat5(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e);
uint8_t art_mat_pick(const ArtMat *m, int v, int x, int y, int dither);

/* ======================================================================
 * images (textures, skies, scratch)
 * ==================================================================== */
typedef struct {
    int w, h;
    int wrapx, wrapy;               /* coordinates wrap (seamless)       */
    uint8_t *p;
} ArtImg;

void    img_init(ArtImg *im, uint8_t *buf, int w, int h, int wrapx, int wrapy);
uint8_t img_get(const ArtImg *im, int x, int y);
void    img_put(ArtImg *im, int x, int y, uint8_t c);
void    img_fill(ArtImg *im, uint8_t c);
void    img_rect(ArtImg *im, int x, int y, int w, int h, uint8_t c);
void    img_hline(ArtImg *im, int x, int y, int w, uint8_t c);
void    img_vline(ArtImg *im, int x, int y, int h, uint8_t c);
void    img_line(ArtImg *im, int x0, int y0, int x1, int y1, uint8_t c);
void    img_disc(ArtImg *im, int cx, int cy, int r, uint8_t c);
void    img_ellipse(ArtImg *im, int cx, int cy, int rx, int ry, uint8_t c);
void    img_ring(ArtImg *im, int cx, int cy, int r0, int r1, uint8_t c);
/* shift a pixel along its family: >0 lighter, <0 darker */
void    img_shift(ArtImg *im, int x, int y, int steps);
/* shaded ball: material picked by sphere lighting from the top-left */
void    img_ball(ArtImg *im, int cx, int cy, int rx, int ry, const ArtMat *m,
                 int dither);
/* polygon fill (points in px, wraps) */
void    img_poly(ArtImg *im, const int *xy, int n, uint8_t c);
/* draw a heart of size s (width ~2s) centred at cx,cy */
void    img_heart(ArtImg *im, int cx, int cy, int s, uint8_t c);
/* crisp bitmap heart, size 0 (5x5), 1 (7x6), 2 (9x8), centred */
void    img_heart_px(ArtImg *im, int cx, int cy, int size, uint8_t fill,
                     uint8_t dark, uint8_t hi);
/* 5-pointed star outline/filled, radius r */
void    img_star(ArtImg *im, int cx, int cy, int r, int filled, uint8_t c);
/* tiny 5x7 block letters (A-Z, 0-9, a few symbols), scale s */
void    img_text(ArtImg *im, int x, int y, const char *s, int scale, uint8_t c);
int     img_text_w(const char *s, int scale);
/* blit image b onto a at (x,y), index 0 = transparent */
void    img_blit(ArtImg *a, const ArtImg *b, int x, int y);
/* average colour (perceptual nearest of the mean RGB) */
uint8_t img_avg(const ArtImg *im);

/* ======================================================================
 * texture helpers (tex_util.c)
 * ==================================================================== */
/* brick / tile layout. Rows of height bh, bricks of width bw, odd rows
 * shifted by `stagger` px. lx,ly: position inside the brick (0-based,
 * the mortar is the first `mortar` px of each row/column). */
typedef struct { uint32_t id; int row, col, lx, ly, w, h, mortar; } TxCell;
void tx_brick(int x, int y, int bw, int bh, int stagger, int mortar, TxCell *c);
/* soft bevel: +1 on the lit top/left edge, -1 on bottom/right (width e) */
int  tx_bevel(int lx, int ly, int w, int h, int e);
/* phase helpers for seamless 4-frame loops: angle for frame f of n */
int  tx_phase(int frame, int nframes);
/* draw a ball shaded from the top-left with a ramp range, sharp bands */
void tx_ball(ArtImg *im, int cx, int cy, int rx, int ry, int ramp, int lo, int hi);
/* drop a soft 1-px shadow (one shade darker) right/below everything that
 * is NOT colour-family `keep` - used for little objects on a surface */
void tx_shadow_px(ArtImg *im, int x, int y);
/* cute 5-petal flower (petal paint, centre colour) */
void tx_flower(ArtImg *im, int x, int y, uint8_t petal, uint8_t petal_d, uint8_t centre);
/* scatter n jittered points over a w x h wrap area (grid based, no
 * clumping); returns count written */
int  tx_scatter(int n, int w, int h, uint32_t seed, int *xs, int *ys);

/* ======================================================================
 * sprite canvas (art_spr.c)
 * ==================================================================== */
#define SC_W 224                    /* scratch canvas                    */
#define SC_H 192
#define SC_AX 112                   /* anchor position inside it         */
#define SC_AY 176

/* primitive style flags */
enum {
    SH_SPHERE   = 0,                /* default: ball lighting            */
    SH_FLAT     = 1,                /* single colour (mat middle)        */
    SH_CYL      = 2,                /* lit across its short axis         */
    SH_VGRAD    = 3,                /* top light -> bottom dark          */
    SH_MASK     = 7,
    SH_DITHER   = 8,                /* ordered dither between bands      */
    SH_NOCONTOUR= 16,               /* no inner contour against parts    */
    SH_SPEC     = 32,               /* white specular glint              */
    SH_SAMEPART = 64,               /* merge into the previous part      */
    SH_BEHIND   = 128,              /* only paint where canvas is empty  */
    SH_ONTOP    = 256,              /* only paint where canvas is filled */
    SH_GLOW     = 512               /* emissive: never darkened at all   */
};

void spr_begin(void);               /* clear canvas, reset transform     */
/* transform stack (applied to all primitive coordinates) */
void spr_push(void);
void spr_pop(void);
void spr_translate(int dx, int dy);       /* pixels                      */
void spr_translate_q(int dx16, int dy16); /* 1/16 pixels                 */
void spr_rotate(int ang);                 /* 4096 per turn, about origin */
void spr_scale(int sx, int sy);           /* Q12                         */
/* lighting direction (default top-left): Q12 vector, need not be unit */
void spr_light(int lx, int ly, int lz);
void spr_ambient(int amb);                /* 0..255 (default 70)         */
void spr_rim(int amount);                 /* rim light 0..255 (def. 60)  */

/* primitives; coordinates in px relative to the current transform */
void spr_ellipse(int cx, int cy, int rx, int ry, const ArtMat *m, int fl);
/* ellipse rotated by `ang` around its own centre */
void spr_ellipse_r(int cx, int cy, int rx, int ry, int ang,
                   const ArtMat *m, int fl);
/* tapered capsule from (x0,y0) radius r0 to (x1,y1) radius r1 */
void spr_capsule(int x0, int y0, int r0, int x1, int y1, int r1,
                 const ArtMat *m, int fl);
/* polygon; lighting from its bounding ellipse */
void spr_poly(const int *xy, int n, const ArtMat *m, int fl);
void spr_tri(int x0, int y0, int x1, int y1, int x2, int y2,
             const ArtMat *m, int fl);
void spr_rect(int x, int y, int w, int h, const ArtMat *m, int fl);
/* curved horn / claw / tail: a capsule chain along a quadratic bezier */
void spr_bezier(int x0, int y0, int x1, int y1, int x2, int y2,
                int r0, int r2, const ArtMat *m, int fl);
/* sub-pixel variants: every coordinate / radius in 1/16 px */
void spr_ellipse_q(int cx16, int cy16, int rx16, int ry16, const ArtMat *m,
                   int fl);
void spr_capsule_q(int x0, int y0, int r0, int x1, int y1, int r1,
                   const ArtMat *m, int fl);
void spr_poly_q(const int *xy16, int n, const ArtMat *m, int fl);
/* fluffy ellipse: the rim is broken into `tufts` pointed fur tufts of
 * depth `depth` (Q12 of the radius) between angles a0..a1 (0..4096, 0 = +x,
 * 1024 = down); outside that arc the edge stays smooth. phase shifts the
 * tufts (Q12 of one tuft) for animation. */
void spr_fluffy(int cx, int cy, int rx, int ry, int tufts, int depth,
                int phase, int a0, int a1, const ArtMat *m, int fl);
/* single pixels / 1px lines (no transform scaling of thickness) */
void spr_px(int x, int y, uint8_t c);
void spr_px_raw(int cx, int cy, uint8_t c);  /* canvas coordinates, no xf */
void spr_line(int x0, int y0, int x1, int y1, uint8_t c);
void spr_heart(int cx, int cy, int s, uint8_t c);
void spr_star(int cx, int cy, int r, uint8_t c);
void spr_twinkle(int cx, int cy, int r, uint8_t core, uint8_t arm);
/* read back (canvas coords relative to anchor, no transform) */
uint8_t spr_get(int x, int y);
void    spr_set(int x, int y, uint8_t c);

/* post passes */
void spr_contours(void);            /* inner part-edge darkening         */
void spr_outline(void);             /* 1px dark selective outline        */
void spr_outline_col(uint8_t c);    /* 1px outline in a fixed colour     */
void spr_erode(uint32_t seed, int amount); /* remove 0..256/256 of pixels*/
void spr_clean(void);               /* drop orphan single pixels         */
void spr_recolor(const uint8_t *map);      /* idx -> map[idx]            */
void spr_brighten(int steps);
void spr_flip(void);                /* mirror horizontally about x=0     */

/* finish: trim to the tight box, horizontally symmetric about the anchor
 * so every frame of a sprite shares the same centre column.
 *   ANCHOR_GROUND  bottom row = the anchor line (feet), centred in x
 *   ANCHOR_CENTER  box symmetric about the anchor point in x AND y (fx,
 *                  projectiles: the image centre is the object centre)
 *   ANCHOR_TIGHT   plain tight box                                       */
enum { ANCHOR_GROUND, ANCHOR_TIGHT, ANCHOR_CENTER };
typedef struct { int w, h; const uint8_t *pix; } ArtFrame;
void spr_finish(ArtFrame *out, int anchor);

/* frame pixel pool (artgen.c) */
uint8_t *art_alloc(int bytes);

/* ======================================================================
 * reusable sprite parts (art_parts.c)
 * ==================================================================== */
/* big glossy cute eye: coloured iris (dark->light 3 shades), pupil,
 * two highlights. look: pupil offset in px. */
void part_eye_cute(int cx, int cy, int rx, int ry, int paint, int lookx,
                   int looky);
/* glowing demon eye: sclera glow material, slit pupil when slit != 0 */
void part_eye_glow(int cx, int cy, int rx, int ry, const ArtMat *glow,
                   int slit, uint8_t pupil);
/* closed eyes / expressions drawn with 1px lines */
void part_eye_x(int cx, int cy, int r, uint8_t c);
void part_eye_shut(int cx, int cy, int r, uint8_t c);   /* ^ shape  */
void part_eye_squeeze(int cx, int cy, int r, uint8_t c);/* > < shape */
/* tooth / fang pointing down (dir 1) or up (dir -1) */
void part_fang(int x, int y, int w, int h, int dir, uint8_t c);
/* soft radial glow: rings of the material from bright core to dark edge,
 * dithered at band edges; drawn BEHIND what is already there. */
void part_glow(int cx, int cy, int r, const ArtMat *m);
/* curved horn from base (x,y) with given direction and size */
void part_horn(int x, int y, int dx, int dy, int bend, int r, const ArtMat *m);
/* a clawed little hand / paw (r = palm radius) */
void part_paw(int x, int y, int r, const ArtMat *m, int claws, uint8_t claw);
/* blush oval (dithered pink) */
void part_blush(int cx, int cy, int w);
/* a little puff of cloud made of 3-5 shaded balls */
void part_cloud(int cx, int cy, int r, const ArtMat *m, uint32_t seed, int fl);

/* ======================================================================
 * generator registry (each generator file exports a table)
 * ==================================================================== */
/* animation modes (DOOMO_ANIM_* in textures.h) are stored per texture */
typedef void (*ArtTexFn)(ArtImg *out, int frame, int nframes);
typedef struct {
    int id;
    const char *name;
    int frames;         /* 1, 2 or 4 */
    int speed;          /* ticks per frame (see DOOMO_ANIM_*) */
    int mode;           /* DOOMO_ANIM_* */
    ArtTexFn fn;
} ArtTexDef;

typedef void (*ArtSkyFn)(ArtImg *out, int frame, int nframes);
typedef struct {
    int id;
    const char *name;
    int frames;
    int speed;
    int mode;
    ArtSkyFn fn;
} ArtSkyDef;

typedef void (*ArtSprFn)(int frame);   /* draws on the canvas; artgen
                                          then calls spr_finish          */
typedef struct {
    int id;
    const char *name;
    int frames;
    int anchor;         /* ANCHOR_* */
    int flags;          /* SPRF_* (sprites.h) */
    ArtSprFn fn;
} ArtSprDef;

extern const ArtTexDef art_tex_cute[];
extern const int       art_tex_cute_n;
extern const ArtTexDef art_tex_hell[];
extern const int       art_tex_hell_n;
extern const ArtSkyDef art_skies[];
extern const int       art_skies_n;
extern const ArtSprDef art_spr_demons[];
extern const int       art_spr_demons_n;
extern const ArtSprDef art_spr_fx[];
extern const int       art_spr_fx_n;
extern const ArtSprDef art_spr_items[];
extern const int       art_spr_items_n;

#endif
