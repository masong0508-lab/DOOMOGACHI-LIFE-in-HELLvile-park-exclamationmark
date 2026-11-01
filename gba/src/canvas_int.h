/* canvas_int.h - the tile-mode overworld (G5): Mode 0 world BG + overlay
 * canvas (owner: gba; docs/GBA.md section 6b).
 *
 * A frame whose first drawing call is plat_overworld() can be shown in
 * Mode 0 instead of Mode 4:
 *   BG2  the world: 16x16 cell composites (a cell's ground or block plus
 *        the lifted top of the cell below, built from the lit tiles of
 *        overworld.c, so they are the Mode 4 pixels) in a VRAM cache, a
 *        32x32 map rewritten from a shadow in every vblank of a flip
 *   BG0  the canvas: every 2D call after the world (billboards, HUD,
 *        dialogue, particles, toasts) in 8bpp tiles, 0 = transparent
 * VRAM (BG, charblock 0 for both BGs):
 *   0x0000 SB 0   canvas map            0x0800 SB 1   world map
 *   0x1000        36 cell composites (4 tiles each)
 *   0x3400        the canvas pool: 392 tiles (tile 0 of it is all zero)
 *   0x9600        (the raycaster's colormap gap: never touched)
 *   0xA000        the dense build canvas: 600 tiles, screen tile t at
 *                 0xA000 + 64 t (page 1's bytes; tiles 384.. in OBJ VRAM)
 * Frames:
 *   CV_DENSE   the screen shows a tile frame: build the canvas dense in
 *              page 1's area; the flip copies the used tiles into the
 *              pool (vblank DMA) - or, when they do not fit or the frame
 *              zooms, converts page 1 in place into a Mode 4 page
 *   CV_SPARSE  the screen shows Mode 4 page 1: tiles go straight into the
 *              pool; overflow drops the frame (the screen keeps page 1)
 *   CV_OFF     Mode 4 into gba_back, as before G5
 */
#ifndef GBA_CANVAS_INT_H
#define GBA_CANVAS_INT_H

#include "gba_plat.h"

enum { CV_OFF, CV_DENSE, CV_SPARSE };
enum { DISP_M4_P0, DISP_M4_P1, DISP_T };

#define CV_VRAM         0x06000000u
#define CV_SB_CANVAS    0                       /* screenblock numbers   */
#define CV_SB_WORLD     1
#define CV_COMP_OFS     0x1000u
#define CV_COMP_N       36                      /* world cell composites */
#define CV_POOL_OFS     (CV_COMP_OFS + CV_COMP_N * 256u)   /* 0x3400   */
#define CV_POOL_END     0x9600u
#define CV_POOL_N       ((CV_POOL_END - CV_POOL_OFS) / 64u) /* 392      */
#define CV_ZERO_TILE    (CV_POOL_OFS / 64u)     /* pool tile 0: all zero */
#define CV_COMP_TILE(s) (CV_COMP_OFS / 64u + 4u * (uint32_t)(s))
#define CV_DENSE_OFS    0xA000u
#define CV_TW           30                      /* screen tiles          */
#define CV_TH           20
#define CV_TILES        (CV_TW * CV_TH)
#define CV_USED_WORDS   ((CV_TILES + 31) / 32)
/* the world map shadow: 32 x 21 entries (31 x 21 are visible) */
#define CV_WMAP_W       32
#define CV_WMAP_H       21

#define CV_GRID_W 17                    /* visible cells + 1 */
#define CV_GRID_H 13
/* T metadata: the lower half of the overworld's arena region (gba_ow);
 * the lit-tile cache keeps its upper half while tiles are on (overworld.c
 * cache_fit). Rebuilt whenever ow_invalidate() runs. */
typedef struct CvComp {
    uint32_t k[6];                 /* key (see ow_tiles.c)            */
    uint32_t last;                 /* cv.frame it was last used in     */
    int8_t   next;                 /* hash chain                       */
    uint8_t  live;                 /* in a chain                       */
    uint8_t  h;                    /* its hash (chain head)            */
    uint8_t  pad;
} CvComp;
typedef struct CvMeta {
    uint16_t wmap[CV_WMAP_H * CV_WMAP_W];   /* BG2 map shadow           */
    uint16_t cmap[CV_TH * 32];              /* BG0 map shadow, rows 0..19 */
    CvComp   comp[CV_COMP_N];
    int8_t   hash[64];                      /* key hash -> first slot, -1 */
    /* dense: what the vblank copies: n runs of tiles, dense tile src ->
     * pool tile dst, len tiles each (identical tiles share a pool tile) */
    uint16_t run_src[CV_POOL_N], run_dst[CV_POOL_N], run_n[CV_POOL_N];
    uint16_t nruns;
    uint32_t dedup[256];                    /* tile hash -> dense tile | pool tile << 10 */
    /* the frame's world (ow_tiles.c): the lifts of the raised cells over
     * the visible cells plus one row below (0 = ground), for the
     * billboards' occlusion; cell (gx0 + i, gy0 + j) at lift[j][i] */
    int16_t  gx0, gy0;
    uint8_t  gw, gh, pad2[2];
    uint8_t  lift[CV_GRID_H][CV_GRID_W];
    /* animated textures: this frame's pixels per texture, and an epoch
     * that changes when any of them changes frame */
    const uint8_t *anim_px[64];
    uint32_t anim_mask[2];
    uint32_t anim_epoch;
    /* the last world pass: its signature and the slots it used */
    uint32_t wsig[2], wused[2], wvalid;
} CvMeta;
#define cv_meta ((CvMeta *)(void *)gba_ow)
/* it lives in the lower half of the lit-tile cache (overworld.c) */
#define CV_META_MAX 8576u

typedef struct CvState {
    uint8_t  mode;                 /* CV_* of the frame being built    */
    uint8_t  disp;                 /* DISP_* shown now                 */
    uint8_t  last_big;             /* the last drawn frame had one     */
    uint8_t  meta_ok;              /* cv_meta valid (composite keys)   */
    uint8_t  overflow;             /* sparse pool full: frame dropped  */
    uint8_t  measure;              /* Mode 4 overworld frame: estimate */
    uint8_t  big;                  /* this frame: a big op or a zoom   */
    uint8_t  ow_now, ow_last;      /* plat_overworld this / last frame */
    uint8_t  pending_ref;          /* measure the reference estimate   */
    uint8_t  prefer_m4;            /* the last tile frame cost more    */
    uint16_t est_last;             /* last frame's Mode 4 estimate     */
    int16_t  sx, sy;               /* world pixel at the screen origin */
    uint16_t pool_next;            /* sparse: next free pool tile      */
    uint16_t used_n;               /* dense: tiles used                */
    uint16_t backoff;              /* frames before the next entry     */
    uint16_t backoff_len;
    uint16_t ref;                  /* estimate the entry must undercut */
    uint16_t ok_run;               /* tile frames shown in a row       */
    uint32_t frame;                /* drawn frames (composite LRU)     */
    uint32_t tint_state;
    uint32_t used[CV_USED_WORDS];  /* dense: tiles touched this frame  */
    uint32_t est[CV_TH];           /* Mode 4: tiles a 2D call covered  */
    /* stats for the harness log (tiles mode, conversions, drops) */
    uint32_t n_tiles, n_conv, n_drop, n_m4;
} CvState;
extern CvState cv;

/* ---- canvas.c (THUMB): policy, flip, conversion ------------------------ */
void cv_frame_begin(void);                /* plat_begin_frame          */
void cv_pre_flip(void);                   /* before the vblank wait     */
int  cv_vblank(void);                     /* in the vblank: 1 = tiles shown */
int  cv_vblank_lines(void);               /* the scanlines cv_vblank needs */
void cv_invalidate(void);                 /* ow_invalidate: meta lost   */
/* plat_overworld: the frame kind (CV_*) for a full-screen view with a map;
 * CV_OFF also arms the Mode 4 estimate when the frame was eligible */
int  cv_ow_begin(int eligible);
void cv_ow_failed(void);                  /* world build failed: Mode 4 */
/* a 2D call covering [x0,x1) x [y0,y1) (clipped) is about to draw:
 * converts / drops / measures. Returns 1 to draw into the canvas. */
int  cv_op(int x0, int y0, int x1, int y1);
void cv_note(int x0, int y0, int x1, int y1);      /* Mode 4 estimate   */
void cv_to_m4(void);                      /* dense -> Mode 4 page 1 now */
void cv_drop(void);                       /* sparse: drop this frame    */
void cv_need_m4(void);                    /* the frame must be Mode 4   */
void cv_scale(void);                      /* plat_post_scale this frame */
void cv_shown_m4(int page);               /* video_flip: Mode 4 shown   */
/* the slow path of every 2D call (gba_nodraw != 0), with the call's
 * clipped, non-empty rect: 0 = draw nothing (a skipped tick, a dropped
 * frame), 1 = draw in Mode 4 into gba_back, 2 = draw into the canvas */
int  vid_route(int x0, int y0, int x1, int y1) __attribute__((long_call));
static inline int vid_route_fast(int x0, int y0, int x1, int y1)
{
    return gba_nodraw ? vid_route(x0, y0, x1, y1) : 1;
}
/* the pixel the screen shows at (x, y) of the frame being built (a tile
 * frame: canvas, else world; Mode 4: gba_back) */
uint32_t cv_read_px(int x, int y);
void gba_drop_frame(void);                /* main.c: no flip this tick  */
/* the screen rect a string covers, grown by pad, clipped */
void cv_text_box(int x, int y, const char *s, int pad, int *x0, int *y0, int *x1, int *y1);

/* ---- ow_tiles.c (THUMB): the world of a tile frame ------------------------ */
/* the composites and the world map shadow for the view in owf (cv.sx/sy
 * set from its scroll); 0 = not possible this frame (the slots the screen
 * shows leave too few for the new view, or the keys were lost) */
int  ow_tiles_world(const DoomoMap *m);
/* the billboards in Mode 4 order (after raised row ty0..ty1, by their
 * feet), into the canvas, each minus the raised cells drawn after it */
void ow_tiles_sprites(const DoomoOverworldView *v, int ty0, int ty1);

/* ---- canvas_2d.c (ARM, the IWRAM 2D group): pixel loops ------------------ */
#include "iwram_ovl.h"
#define CV_ARM IWRAM_2D_CODE
/* the rare paths stay ARM but in ROM (conversion, checker, line pixels,
 * rect shading): the 2D group must fit the slot beside the 6 KB stack */
#define CV_ROM __attribute__((target("arm"), long_call, noinline))
void cv_fill(int x0, int y0, int x1, int y1, uint32_t color) CV_ARM;
void cv_shade(int x0, int y0, int x1, int y1, const uint8_t *cm) CV_ROM;
void cv_checker(int x0, int y0, int x1, int y1, int par, uint32_t color) CV_ROM;
void cv_pixel(int x, int y, uint32_t color) CV_ROM;
/* glyph rows g[0..rows) (bit c = column c, at most 16 columns) at (x, y) */
void cv_glyph(const uint16_t *g, int rows, int x, int y, uint32_t color) CV_ARM;
/* text into the canvas (video.c plat_text / plat_text_outline) */
void cv_text(int x, int y, const char *s, uint32_t color, int dil) CV_ARM;
void cv_blit(int x, int y, const DoomoImage *img, int scale, int flags, uint32_t color) CV_ARM;
/* an overworld billboard: ow.iwram.c ow_sprite's pixels, minus the parts
 * of the raised cells Mode 4 draws after it: those of the frame's lift
 * grid (cv_meta) in map rows > row (row < -32768: none) */
void cv_sprite(const DoomoImage *img, int x0, int y0, int s, int flags, const uint8_t *cm,
               int row) CV_ARM;
/* ow_shade over the canvas (a shadow blob row), minus the occluders */
void cv_shade_occ(int x0, int x1, int y, const uint8_t *cm, int row) CV_ARM;
#define CV_ROW_NONE 0x7FFF                /* cv_sprite: drawn after all rows */
/* the world's screen row y (240 px) into dst (halfword aligned) */
void cv_world_row(uint8_t *line, int y) CV_ROM;
void cv_convert(void) CV_ROM;             /* canvas.c cv_to_m4's loop  */
uint32_t cv_plan(void) CV_ARM;            /* dense: pool tiles + runs  */
uint32_t cv_plan_dedup(void) CV_ROM;      /* (more tiles than the pool) */

#endif
