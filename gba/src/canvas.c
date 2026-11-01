/* canvas.c - the tile-mode overworld's frame policy, flip and the 2D
 * entry points while a tile frame is built (owner: gba; G5; layout and
 * frame kinds in canvas_int.h, docs/GBA.md section 6b).
 *
 * While a tile frame is built, gba_nodraw is GBA_NODRAW_CANVAS (2): every
 * 2D call of platform.h sees a nonzero gba_nodraw on its fast path, as on
 * a skipped tick, and takes its slow path (vid_route below, or straight to
 * cv_rect_fast for plat_rect) into the canvas loops of canvas_2d.c, so a
 * Mode 4 frame pays nothing for the tile mode. Anything that must draw into a bitmap
 * page (plat_view3d, plat_room, a zoom, a call too big for the pool)
 * first turns the frame into Mode 4 (cv_need_m4): a dense frame converts
 * page 1 in place; a sparse frame is dropped.
 */
#include "canvas_int.h"
#include "video_int.h"
#include "ow_int.h"
#include "postfx.h"
#include "doomo/palette.h"

IWRAM_BSS CvState cv;

/* an op this big (pixels) is drawn in Mode 4: shading it through the
 * canvas reads the world per pixel, and it would fill most of the pool */
#define CV_BIG_AREA  (SCREEN_W * SCREEN_H / 2)
/* entry needs this many free pool tiles above the estimate */
#define CV_ENTRY_SLACK 16
/* A canvas tile costs more than the same pixels in Mode 4 (tile
 * addressing, read-modify-writes, the vblank copy), and the world saves a
 * fixed cost. G5 measured the break-even at about 250 canvas tiles
 * (hahaland 321 tiles: 48.0 ms tiled, 43.8 ms Mode 4; hellvile ~200:
 * 35.0 / 38.6). G2 made Mode 4 much cheaper (ground, blocks and above all
 * the billboards, which the canvas still draws per pixel): hellvile is
 * now 35.0 ms tiled against 24.1 ms in Mode 4, park and the map walk lose
 * too, so only a nearly empty canvas (a HUD's worth) is worth a tile
 * frame until G6 moves the billboards to OBJs. A frame above this goes
 * to Mode 4; a Mode 4 estimate above it does not enter and is looked at
 * again only every CV_REMEASURE_FAR frames (measuring costs). Exact
 * either way. G6 moved the billboards to OBJs (ow_obj.c), but measured
 * on G2's Mode 4 a tile frame still loses with a full canvas (hahaland
 * 35.6 ms tiled against 34.7, hellvile 25.9 / 24.1, the story map with
 * its dialogue 51.7 / 37.8: the canvas HUD and text, the world pass and
 * the vblank-aligned copy), so the window stays until G7. */
#define CV_TILES_WIN 64
#define CV_BACKOFF_MIN 30u
#define CV_REMEASURE   16u
#define CV_REMEASURE_FAR 64u
#define CV_BACKOFF_MAX 480u

void cv_invalidate(void)
{
    cv.meta_ok = 0;
}

/* plat_begin_frame (drawn ticks and skipped ones alike) */
void cv_frame_begin(void)
{
    cv.mode = CV_OFF;
    cv.measure = 0;
    cv.big = 0;
    cv.overflow = 0;
    cv.ow_now = 0;
}

static void enter_canvas(int mode)
{
    cv.mode = (uint8_t)mode;
    gba_nodraw = GBA_NODRAW_CANVAS;
    if (mode == CV_DENSE) {
        memset(cv.used, 0, sizeof(cv.used));
        cv.used_n = 0;
    } else {
        int i;
        uint16_t *m = cv_meta->cmap;
        for (i = 0; i < CV_TH * 32; i++) m[i] = CV_ZERO_TILE;
        cv.pool_next = 1;                       /* pool tile 0: the zero tile */
        /* the zero tile itself: page 0 was a Mode 4 page until now (word
         * stores: a DMA "fill" from a ROM word reads on past it, the
         * source address of a ROM DMA always increments) */
        {
            volatile uint32_t *z = (volatile uint32_t *)(CV_VRAM + CV_ZERO_TILE * 64u);
            for (i = 0; i < 16; i++) z[i] = 0;
        }
    }
}

/* the frame kind for an eligible plat_overworld (see canvas_int.h) */
int cv_ow_begin(int eligible)
{
    int first = gba_nodraw == GBA_NODRAW_FIRST;
    cv.ow_now = 1;
    if (gba_nodraw == GBA_NODRAW_FIRST) gba_nodraw = 0;
    if (!first || !eligible) return CV_OFF;
    if (cv.disp == DISP_T && cv.meta_ok && !cv.prefer_m4) {
        iwram_ovl_use(OVL_2D);
        enter_canvas(CV_DENSE);
        return CV_DENSE;
    }
    if (cv.disp == DISP_M4_P1 && !cv.backoff && cv.ow_last && !cv.last_big &&
        cv.est_last + CV_ENTRY_SLACK <= (int)CV_POOL_N - 1 && cv.est_last <= CV_TILES_WIN &&
        (!cv.ref || cv.est_last * 10u <= cv.ref * 9u)) {
        iwram_ovl_use(OVL_2D);
        cv.meta_ok = 0;                         /* page 0 held a Mode 4 frame */
        cv.prefer_m4 = 0;
        enter_canvas(CV_SPARSE);
        return CV_SPARSE;
    }
    /* the estimate (every 2D call takes the slow path to note its rect) is
     * only needed when the next frame may enter, or for the reference */
    if (!cv.backoff || cv.pending_ref) {
        cv.measure = 1;
        memset(cv.est, 0, sizeof(cv.est));
        gba_nodraw = GBA_NODRAW_MEASURE;
    }
    return CV_OFF;
}

/* plat_overworld could not build the world (composite slots full): the
 * frame is Mode 4, nothing has been drawn into the canvas yet */
void cv_ow_failed(void)
{
    cv.mode = CV_OFF;
    gba_nodraw = 0;
}

/* ---- leaving the canvas mid-frame ---------------------------------------- */
static void backoff_grow(void)
{
    cv.backoff_len = (uint16_t)(cv.backoff_len ? cv.backoff_len * 2u : CV_BACKOFF_MIN);
    if (cv.backoff_len > CV_BACKOFF_MAX) cv.backoff_len = CV_BACKOFF_MAX;
    cv.backoff = cv.backoff_len;
    cv.ok_run = 0;
}

void cv_to_m4(void)
{
    if (cv.mode != CV_DENSE) return;
    bb_flatten(0, 0, SCREEN_W, SCREEN_H);       /* G6: the OBJ billboards too */
    cv_convert();
    cv.mode = CV_OFF;
    gba_nodraw = 0;                             /* gba_back is page 1 */
    cv.n_conv++;
    cv.pending_ref = 1;
    backoff_grow();
}

void cv_drop(void)
{
    cv.mode = CV_OFF;
    bb_drop();
    cv.n_drop++;
    cv.pending_ref = 1;
    backoff_grow();
    gba_drop_frame();                           /* gba_nodraw = 1: no flip */
}

/* a call that must draw into a bitmap page (3D, the house, a zoom) */
void cv_need_m4(void)
{
    if (cv.mode == CV_DENSE) cv_to_m4();
    else if (cv.mode == CV_SPARSE) cv_drop();
}

void cv_scale(void)
{
    cv.big = 1;
    cv_need_m4();
}

/* ---- the Mode 4 estimate ---------------------------------------------------- */
void cv_note(int x0, int y0, int x1, int y1)
{
    int ty;
    uint32_t bits;
    if ((x1 - x0) * (y1 - y0) >= CV_BIG_AREA) cv.big = 1;
    bits = (0xFFFFFFFFu >> (32 - (((x1 + 7) >> 3) - (x0 >> 3)))) << (x0 >> 3);
    for (ty = y0 >> 3; ty <= (y1 - 1) >> 3; ty++) cv.est[ty] |= bits;
}

static uint32_t est_count(void)
{
    uint32_t n = 0;
    int i;
    for (i = 0; i < CV_TH; i++) {
        uint32_t v = cv.est[i];
        while (v) { v &= v - 1; n++; }
    }
    return n;
}

/* every 2D call in tile mode, clipped and non-empty: 1 = draw it into the
 * canvas, 0 = the frame left the canvas (it is Mode 4 now, or dropped) */
int cv_op(int x0, int y0, int x1, int y1)
{
    if ((x1 - x0) * (y1 - y0) >= CV_BIG_AREA) {
        cv.big = 1;
        cv_need_m4();
        return 0;
    }
    return 1;
}

/* ---- readback -------------------------------------------------------------- */
uint32_t cv_read_px(int x, int y)
{
    if (gba_nodraw == GBA_NODRAW_CANVAS) {
        uint32_t t = (uint32_t)((y >> 3) * CV_TW + (x >> 3)), v = 0;
        const volatile uint8_t *p = 0;
        if (cv.mode == CV_DENSE) {
            if (cv.used[t >> 5] & (1u << (t & 31)))
                p = (const volatile uint8_t *)(CV_VRAM + CV_DENSE_OFS + t * 64u);
        } else {
            uint32_t e = cv_meta->cmap[(y >> 3) * 32 + (x >> 3)];
            if (e != CV_ZERO_TILE) p = (const volatile uint8_t *)(CV_VRAM + e * 64u);
        }
        if (p) v = p[((y & 7) << 3) | (x & 7)];
        if (!v) v = bb_read_px(x, y);           /* G6: an OBJ billboard */
        if (!v) {
            int wx = x + (cv.sx & 7), wy = y + (cv.sy & 7);
            uint32_t e = cv_meta->wmap[(wy >> 3) * CV_WMAP_W + (wx >> 3)];
            v = *(const volatile uint8_t *)(CV_VRAM + e * 64u + (uint32_t)(((wy & 7) << 3) | (wx & 7)));
        }
        return v;
    }
    {
        uint16_t h = gba_back[y * (SCREEN_W / 2) + (x >> 1)];
        return (x & 1) ? (uint32_t)(h >> 8) : (uint32_t)(h & 0xFF);
    }
}

/* ---- the flip ------------------------------------------------------------------ */
void cv_pre_flip(void)
{
    if (cv.backoff) cv.backoff--;
    if (cv.mode == CV_DENSE && cv_plan() > CV_POOL_N) cv_to_m4();   /* does not fit: Mode 4 */
    if (cv.mode != CV_OFF) {
        uint32_t used = cv.mode == CV_DENSE ? cv.used_n : (uint32_t)cv.pool_next - 1u;
        if (used > CV_TILES_WIN) {              /* Mode 4 is faster for this */
            cv.prefer_m4 = 1;
            cv.pending_ref = 1;
            backoff_grow();
        }
        cv.n_tiles++;
        if (++cv.ok_run >= 300) { cv.ok_run = 0; cv.backoff_len = 0; cv.ref = 0; }
    }
    /* the estimate of a measured Mode 4 frame (sprites + 2D after the world) */
    if (cv.measure) {
        cv.est_last = (uint16_t)est_count();
        if (cv.pending_ref) { cv.ref = cv.est_last; cv.pending_ref = 0; }
        cv.n_m4++;
        /* no entry from this estimate: measure again in 16 frames, not on
         * every frame (each measured frame's 2D calls take the slow path) */
        if (cv.big || cv.est_last + CV_ENTRY_SLACK > (int)CV_POOL_N - 1 ||
            (cv.ref && cv.est_last * 10u > cv.ref * 9u) || gba_back != (uint16_t *)VRAM_PAGE1)
            if (cv.backoff < CV_REMEASURE) cv.backoff = CV_REMEASURE;
        /* (G2) a frame Mode 4 draws faster: look again only now and then */
        if (cv.est_last > CV_TILES_WIN && cv.backoff < CV_REMEASURE_FAR) cv.backoff = CV_REMEASURE_FAR;
    } else {
        cv.est_last = 0xFFFF;                   /* no estimate: no entry */
    }
    cv.last_big = cv.big;
    cv.measure = 0;
    if (gba_nodraw == GBA_NODRAW_MEASURE || gba_nodraw == GBA_NODRAW_FIRST) gba_nodraw = 0;
    /* a scene without the overworld forgets the history */
    if (!cv.ow_now) { cv.ref = 0; cv.pending_ref = 0; cv.backoff = 0; cv.backoff_len = 0; cv.prefer_m4 = 0; }
    cv.ow_last = cv.ow_now;
    cv.frame++;
}

/* the scanlines the vblank part of a tile frame's flip takes (the maps
 * from EWRAM ~616 B a line, the pool tiles VRAM to VRAM ~1,232 B a line;
 * measured: 320 tiles in 21 lines), with a margin; 0 for a Mode 4 frame */
int cv_vblank_lines(void)
{
    uint32_t i, n = 0;
    if (cv.mode == CV_OFF) return 0;
    if (cv.mode == CV_DENSE)
        for (i = 0; i < cv_meta->nruns; i++) n += cv_meta->run_n[i];
    return 8 + (int)(n * 64u / 1100u);
}

/* in the vblank of the flip; returns 1 if this frame shows the tiles */
int cv_vblank(void)
{
    if (cv.mode == CV_OFF) return 0;
    /* maps from their shadows, then (dense) the canvas tiles into the pool */
    dma3_copy32((void *)(CV_VRAM + CV_SB_WORLD * 0x800u), cv_meta->wmap, CV_WMAP_H * CV_WMAP_W / 2);
    dma3_copy32((void *)(CV_VRAM + CV_SB_CANVAS * 0x800u), cv_meta->cmap, CV_TH * 32 / 2);
    if (cv.mode == CV_DENSE) {
        const CvMeta *M = cv_meta;
        uint32_t i;
        for (i = 0; i < M->nruns; i++)
            dma3_copy32((void *)(CV_VRAM + (CV_ZERO_TILE + M->run_dst[i]) * 64u),
                        (const void *)(CV_VRAM + CV_DENSE_OFS + M->run_src[i] * 64u), M->run_n[i] * 16u);
    }
    REG_BG0CNT = (uint16_t)(0x0080 | (CV_SB_CANVAS << 8));          /* prio 0, 8bpp */
    REG_BG2CNT = (uint16_t)(0x0080 | (CV_SB_WORLD << 8) | 1);       /* prio 1, 8bpp */
    {
        int dx, dy, s, cx, cy, wx0, wy0, wx1, wy1;
        postfx_xform(&dx, &dy, &s, &cx, &cy);   /* no zoom here (cv_scale) */
        REG_BG0HOFS = (uint16_t)(-dx & 511);
        REG_BG0VOFS = (uint16_t)(-dy & 511);
        REG_BG2HOFS = (uint16_t)(((cv.sx & 7) - dx) & 511);
        REG_BG2VOFS = (uint16_t)(((cv.sy & 7) - dy) & 511);
        /* a shake moves the picture; what it uncovers is the backdrop, as
         * in Mode 4 (an affine bitmap does not wrap): window 0 is the
         * moved screen, outside it only the OBJs (blend applies to both) */
        wx0 = DOOMO_CLAMP(dx, 0, SCREEN_W); wx1 = DOOMO_CLAMP(SCREEN_W + dx, 0, SCREEN_W);
        wy0 = DOOMO_CLAMP(dy, 0, SCREEN_H); wy1 = DOOMO_CLAMP(SCREEN_H + dy, 0, SCREEN_H);
        REG_WIN0H = (uint16_t)((wx0 << 8) | wx1);
        REG_WIN0V = (uint16_t)((wy0 << 8) | wy1);
        REG_WININ = 0x0035;                     /* BG0, BG2, OBJ, blend   */
        /* (G6: no OBJ outside when a billboard OBJ would show there:
         * the billboards belong to the moved picture) */
        REG_WINOUT = bb_edge ? 0x0020 : 0x0030; /* OBJ, blend             */
        REG_DISPCNT = (uint16_t)(DCNT_MODE0 | DCNT_BG0 | DCNT_BG2 | DCNT_OBJ | DCNT_OBJ_1D |
                                 ((dx | dy) ? DCNT_WIN0 : 0));
    }
    cv.disp = DISP_T;
    gba_nodraw = 0;
    return 1;
}

/* a Mode 4 frame was shown from `page` (0 / 1) */
void cv_shown_m4(int page)
{
    cv.disp = (uint8_t)(page ? DISP_M4_P1 : DISP_M4_P0);
    if (!page) cv.meta_ok = 0;                  /* page 0 covers the tile region */
}

/* ---- the 2D calls' slow path ------------------------------------------------- */
int vid_route(int x0, int y0, int x1, int y1)
{
    switch (gba_nodraw) {
    case 0: return 1;
    case GBA_NODRAW_FIRST: gba_nodraw = 0; return 1;
    case GBA_NODRAW_MEASURE: cv_note(x0, y0, x1, y1); return 1;
    case GBA_NODRAW_CANVAS:
        if (cv_op(x0, y0, x1, y1)) return 2;
        return gba_nodraw ? 0 : 1;              /* dropped / converted   */
    default: return 0;                          /* a skipped tick        */
    }
}

/* the screen rect a string covers (the estimate and the big-op test):
 * text is never big, but a long string still covers many tiles */
void cv_text_box(int x, int y, const char *s, int pad, int *x0, int *y0, int *x1, int *y1)
{
    int w = 0, cw = 0, lines = 1;
    for (; *s; s++) {
        if (*s == '\n') { lines++; cw = 0; continue; }
        cw += FONT_ADVANCE;
        if (cw > w) w = cw;
    }
    *x0 = DOOMO_CLAMP(x - pad, 0, SCREEN_W);
    *y0 = DOOMO_CLAMP(y - pad, 0, SCREEN_H);
    *x1 = DOOMO_CLAMP(x + w + pad, 0, SCREEN_W);
    *y1 = DOOMO_CLAMP(y + lines * FONT_LINE + pad + 2, 0, SCREEN_H);
}

/* plat_checker / plat_line's slow paths (video.iwram.c: gba_nodraw != 0;
 * the IWRAM entry points keep only the test, the routing lives here) */
void plat_checker_m4(int x, int y, int w, int h, uint8_t color) IWRAM_CALL;
void plat_line_m4(int x0, int y0, int x1, int y1, uint8_t color) IWRAM_CALL;
void plat_checker_slow(int x, int y, int w, int h, uint8_t color)
{
    int x1 = x + w, y1 = y + h, par = (x + y) & 1, x0 = x, y0 = y, k;
    if (gba_nodraw == 1 || w <= 0 || h <= 0) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x0 >= x1 || y0 >= y1) return;
    k = vid_route(x0, y0, x1, y1);
    if (k == 2) cv_checker(x0, y0, x1, y1, par, color ? color : PAL_BLACK);
    else if (k == 1) plat_checker_m4(x, y, w, h, color);
}

void plat_line_slow(int x0, int y0, int x1, int y1, uint8_t color)
{
    int a = DOOMO_MIN(x0, x1), b = DOOMO_MAX(x0, x1), c = DOOMO_MIN(y0, y1), d = DOOMO_MAX(y0, y1), k;
    int dx, dy, sx, sy, err, n;
    if (gba_nodraw == 1) return;
    a = DOOMO_MAX(a, 0); c = DOOMO_MAX(c, 0);
    b = DOOMO_MIN(b, SCREEN_W - 1); d = DOOMO_MIN(d, SCREEN_H - 1);
    if (a > b || c > d) return;
    k = vid_route(a, c, b + 1, d + 1);
    if (k == 1) { plat_line_m4(x0, y0, x1, y1, color); return; }
    if (k != 2) return;
    if (!color) color = PAL_BLACK;
    /* plat_line_m4's pixels: runs for horizontal / vertical lines, else
     * Bresenham with the same cap */
    if (y0 == y1 || x0 == x1) { cv_fill(a, c, b + 1, d + 1, color); return; }
    dx = x1 > x0 ? x1 - x0 : x0 - x1;
    dy = y1 > y0 ? y0 - y1 : y1 - y0;
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;
    n = 4096;
    while (n-- > 0) {
        int e2;
        if ((unsigned)x0 < SCREEN_W && (unsigned)y0 < SCREEN_H) cv_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* plat_rect's slow path (video_loops.s: gba_nodraw != 0) */
void plat_rect_m4(int x, int y, int w, int h, uint8_t color) IWRAM_CALL;
void plat_rect_slow(int x, int y, int w, int h, uint8_t color)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, k;
    int x1 = x + w > SCREEN_W ? SCREEN_W : x + w, y1 = y + h > SCREEN_H ? SCREEN_H : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    k = vid_route(x0, y0, x1, y1);
    if (k == 2) cv_fill(x0, y0, x1, y1, color ? color : PAL_BLACK);
    else if (k == 1) plat_rect_m4(x, y, w, h, color);
}
