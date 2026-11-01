/* room.c - the dream house on the GBA: plat_room() and plat_room_ui_opaque()
 * (owner: H2; docs/design/story/SPEC.md sec.7 H2, C7, C9, C22; HF sec.6).
 *
 * WHAT IT DRAWS. Exactly what core/src/room_ref.c (H4, the reference)
 * draws, pixel for pixel: the same projection, wall columns, floor tiles,
 * paints, light, sprites and topological order. The GBA harness checks it
 * against the room_ref golden (core/tests/golden/room_ref.txt) on the full,
 * the dirty-rect and the UI-restore paths (room_hc_parity,
 * gba/harness/scripts/home.txt). Platform choices on top of the reference:
 * the held ghost (RO_GHOST) is drawn last, over everything, as an OBJ would
 * be, and the cursor just before it; small pans (|pan| <= 4: the view
 * brake's wobble) and the zoom punch are BG2 offset / scale
 * (plat_post_offset / plat_post_scale) instead of a recompose; a squashed
 * piece that nothing covers goes through plat_image_xform (S1's affine OBJs,
 * obj.c), a covered one draws unsquashed in order (as room_ref's software
 * default does at scale 1). (H5) A quarter turn maps (x, y) to (d - 1 - y,
 * x), d the depth before the turn, here and in the reference alike (both
 * used the width before: non-square rooms drew off their floor in turned
 * views).
 *
 * HOW (a static bitmap with dirty rects, HF sec.6.4). Mode 4 keeps two
 * pages; each page remembers what it shows: the shell key (room, view,
 * light, paints, viewport, HUD bands), the draw list it was composed from
 * (screen box + signature per item) and a checksum of every 16x8 block of
 * the managed area (the viewport minus the opaque HUD bands) as the
 * renderer left it. A frame then:
 *   1. re-reads the block checksums of the page: a block that changed was
 *      drawn over after plat_room (a menu, a caption, the iris, a dialogue
 *      box) and becomes a dirty rect (the platform's "UI bbox feedback":
 *      nothing in the core has to report its 2D drawing, nothing is missed);
 *   2. diffs the new draw list against the page's: a new, gone or changed
 *      item dirties its old and new boxes (exact rects, merged only when
 *      they overlap and the union wastes little); unchanged items that
 *      swapped order where they overlap dirty both;
 *   3. redraws each rect (background, walls, floor, then every item
 *      touching it, in order, clipped), then the squash OBJs;
 *   4. checksums the blocks the rects touched.
 * A changed shell key, a lost gba_big claim, a gap in the frames, too many
 * rects or more than DIRTY_FULL of the area is a full recompose; the other
 * page then starts from a VRAM-to-VRAM copy of it (copy-forward, ~2 ms)
 * instead of a second recompose. Catch-up ticks (plat_draw_skipped) draw
 * nothing; the next drawn frame diffs against what the page really shows.
 * Per object, the item (box, signature, sprite, LUT) and its sort entry are
 * cached until the object or the geometry changes; the order is room_ref's
 * topological pick over bitsets, re-run only for the entries that moved.
 *
 * THE PIXEL PATHS. No new IWRAM code (SPEC C7.1, task #78). Walls: the
 * wallpaper repeats every 16 units, so each wall is one 16 px sheared strip
 * repeated; its rows are re-laid as 8 "phase" rows so that a screen row of
 * a wall is one contiguous copy. Floor: the tiles repeat every 32 x 16 px
 * (upper-half tiles always win the 2 px overlaps), so a floor row is a copy
 * from one of 16 cached pattern rows plus <= 2 px patches on the two front
 * edges, where the later tile does not exist. Pieces: run by run, touching
 * only opaque pixels: plain pieces (no paint, full light, not mirrored)
 * straight from ROM; the others through a 256-byte LUT (paint recolour
 * composed with doomo_colormap[light]) into a stack buffer laid out at the
 * destination's pixel parity. Copies are DMA3 halfword transfers (ow_out
 * when the ROM source is misaligned), fills vid_rect(), actors ow_sprite(),
 * checksums ldmia word reads.
 *
 * MEMORY. All state lives in the house's region of the arena (bigbuf.c,
 * gba_plat.h GBA_BIG_ROOM; claim GBA_BIG_BY_ROOM; RBig <= 15,488 B, the C7
 * claim, plus the harness layouts behind it), 0 B of IWRAM, ~34 B of .bss
 * / .data (the claim canary, the HUD bands, the same-frame opaque rects).
 * Another claim on the region (overworld, r3d, a save, a new scene scratch
 * claim) is noticed through a canary word, a frame gap or the scratch's
 * generation and costs one full
 * recompose. The core's plat_cache (the scene scratch tail) is not used on
 * the GBA (SPEC C9). No OBJ of its own: OBJ VRAM belongs to obj.c's four
 * squash slots (16 KB) and obj.c only copies OAM while a squash shows, so
 * the cursor and the held ghost are bitmap items (cheap dirty rects).
 */
#include "gba_plat.h"
#include "ow_int.h"
#include "video_int.h"
#include "postfx.h"
#include "room_int.h"
#include "canvas_int.h"
#include "doomo/palette.h"
#include "doomo/furn.h"
#include "doomo/home.h"
#include "doomo/doomo.h"
#include "../../core/src/home_int.h"      /* HomeRoom, the parity layout (harness only) */
#include "../../core/gen/furn_ids.h"
#include "../../core/tests/room_shells.h"   /* the parity frames (harness only) */

/* Size: the file is built for size (-Os, SPEC H2: <= 12 KB of THUMB); the
 * pixel paths (HOT) keep -O2. */
#pragma GCC optimize("Os")
#define HOT __attribute__((optimize("O2")))

#define LINE        120                /* halfwords per screen row          */
#define RB_ITEMS    56                 /* draw list: 41 objects + 14 actors + cursor */
#define RB_BX       (SCREEN_W / 16)    /* 15 block columns of 16 px          */
#define RB_BY       17                 /* block rows of 8 px: the 136 px room viewport */
#define RB_BLOCKS   (RB_BX * RB_BY)
#define STAGE_BYTES 1800               /* xform image / wall strip scratch   */
#define LUT_SLOTS   3
#define STRIP_ROWS  56                 /* wall_h (48 below a 200 px viewport) + 8 */
#define WJ          (STRIP_ROWS / 8)   /* strip rows per phase row           */
#define WROW        (WJ * 16)          /* a phase row: WJ strip rows side by side */
#define PAT_W       96                 /* 3 periods of the floor pattern     */
#define DIRTY_FULL  70                 /* % of the managed blocks           */
#define RB_MAGIC    0x524F4F4Du        /* "ROOM"                             */

enum { RK_WALL, RK_PIECE, RK_ACTOR, RK_CURSOR, RK_GHOST };
#define ISO_NONE 0xFFFFu
#define LUT_PLAIN (0xFFFFu | 15u << 16)      /* no paint, full light */

typedef struct RItem {
    int16_t x0, y0, x1, y1;            /* screen box                         */
    int16_t px, py;                    /* image top-left                     */
    uint32_t sig;                      /* everything its pixels depend on    */
    uint32_t lut;                      /* LUT key: pa | pb << 8 | lvl << 16  */
    uint16_t src;                      /* object / actor index               */
    uint16_t iso;                      /* sprite entry, ISO_NONE = the box   */
    uint8_t  kind, flip, xf, pad;      /* RK_*, mirrored, through plat_image_xform */
    int8_t   bx0, by0, bx1, by1;       /* box fallback: rotated tile box     */
} RItem;                               /* 32 B */

typedef struct RRec { int16_t x0, y0, x1, y1; uint32_t sig; } RRec;

typedef struct RPage {
    uint32_t key;                      /* the shell key it was composed with */
    uint16_t n, valid;
    RRec rec[RB_ITEMS];                /* its draw list, in draw order       */
    uint32_t sum[RB_BLOCKS];           /* block checksums as left            */
} RPage;

typedef struct RSort { int8_t x0, y0, x1, y1; uint8_t layer, z; int16_t idx; } RSort;
typedef struct RRect { int16_t x0, y0, x1, y1; } RRect;
#define RB_RECTS 24
#define RB_OBJS  (HOME_OBJ_MAX + 1)
enum { OK_NONE, OK_WALL, OK_FLOOR, OK_GHOST, OK_SKIP, OK_FRONT };

typedef struct RBig {
    uint32_t magic;                    /* RB_MAGIC ^ rb_epoch: the canary    */
    uint32_t shell_key;
    uint16_t nitem, nsort;
    uint8_t  lut_next, strip_h, pad[2];
    uint32_t lut_key[LUT_SLOTS];
    RItem    item[RB_ITEMS];
    RSort    sort[RB_ITEMS];
    uint8_t  order[RB_ITEMS];
    uint32_t succ[RB_ITEMS][2];        /* behind-edges, a bit per item       */
    uint8_t  match[RB_ITEMS];          /* new item -> old record + 1, 0 none */
    /* per object, kept across frames: the object as last built, its item
     * and sort entry (rebuilt only when the object or the geometry changes) */
    uint32_t ocache_key;
    uint8_t  okind[RB_OBJS];           /* OK_* */
    DoomoRoomObj ocache[RB_OBJS];
    RItem    oitem[RB_OBJS];
    RSort    osort[RB_OBJS];
    RSort    sort_prev[RB_ITEMS];
    uint16_t nsort_prev;
    uint8_t  dirty[RB_BLOCKS];         /* blocks to redraw / re-checksum     */
    uint8_t  nrect, rect_over, pad2[2];
    RRect    rect[RB_RECTS];           /* this frame's dirty rects           */
    RPage    page[2];
    uint8_t  lut[LUT_SLOTS][256];
    uint8_t  pat[16][PAT_W + 4];       /* floor rows: (q, parity)            */
    uint8_t  wall[2][8][WROW + 4];     /* walls A, B: 8 phase rows each      */
    uint8_t  stage[STAGE_BYTES + 4];
    RoomStats stats;
} RBig;
/* the harness layouts (room_hc_parity, room_hc_test) live behind it */
typedef struct RTest {
    HomeRoom room;
    DoomoRoomObj objs[HOME_OBJ_MAX + 2], objs2[HOME_OBJ_MAX + 2];
    DoomoRoomActor acts[12];
} RTest;
DOOMO_STATIC_ASSERT(sizeof(RBig) <= 15488u, room_state_fits_the_c7_claim);
/* (HART) the roof's tables, in the two 2.5 KB VRAM gaps the mode 4 pages
 * leave (the raycaster keeps its colormaps there: r3d_invalidate() makes it
 * copy them again when it claims the arena after the house; the house
 * rebuilds these with its shell after any frame gap). Laid out so that a
 * screen row of the roof is a few straight copies, like the walls' strips:
 *  - the repeating pattern: column p = 16m + j of row rel = y - top lies in
 *    strip row rel - 8m (side B's rows stored right to left, as the screen
 *    runs), 16 bytes a row;
 *  - each special range [a, b) (valley, hip, dormer, skylight: where the
 *    pattern does not hold) as the screen rows it covers, b - a bytes each
 *    (side B right to left).
 * Both keep a column's parity equal to its screen x's (ox is even, ranges
 * start on even columns), so the copies are halfword DMAs. VRAM takes
 * halfword writes only; byte reads cost what IWRAM's do. */
#define RPH_S0   40                     /* S = s + 40, s = rel - 8m in -34 .. 7 */
#define RPH_G    6                      /* S >> 3 in 0 .. 5                   */
#define RPH_W    (RPH_G * 16)           /* a phase row: 6 strip rows of 16 B   */
#define RROOF_PHASE ((uint8_t (*)[8][RPH_W])0x06013600)          /* gap B: 1,536 of 2,560 B */
#define RROOF_SPEC  ((uint8_t *)0x06009600)                      /* gap A: the ranges' rows */
#define RROOF_SPEC_MAX 2560
#define RROOF_FEAT  ((uint8_t *)0x06013600 + 2 * 8 * RPH_W)     /* gap B: the chimney's rows */
#define RROOF_FEAT_MAX (2560 - 2 * 8 * RPH_W)
DOOMO_STATIC_ASSERT(2 * 8 * RPH_W <= 2560, roof_phase_rows_fit_gap_b);
DOOMO_STATIC_ASSERT(((sizeof(RBig) + 7u) & ~7u) + sizeof(RTest) <= GBA_BIG_ROOM, room_test_fits_its_region);
/* the house's region of the arena (bigbuf.c, G0): GBA_BIG_ROOM bytes below
 * the billboards, beside the house scene's 2.8 KB of scene scratch */
#define RB_BASE gba_room              /* == GBA_BIG(GBA_BIG_ROOM) */
#define TEST ((RTest *)(RB_BASE + ((sizeof(RBig) + 7u) & ~7u)))

/* ~30 B of .bss (SPEC H2: <= 64 B, with hcmd.c's 28) */
static uint32_t rb_epoch;              /* changes on every reset             */
static uint32_t rb_last_frame;         /* gba_frames of the last plat_room   */
static uint8_t  band_pend[2] = { 0, SCREEN_H };   /* HUD bands seen this frame */
static uint8_t  band_cur[2] = { 0, SCREEN_H };    /* ... in effect           */
static uint8_t  rb_test;               /* ROOM_TEST_* (harness)              */
static uint8_t  op_n;                  /* opaque rects registered before plat_room */
static uint8_t  op_rect[4][4];         /* x0, y0, x1, y1 of this frame's      */
static uint32_t op_frame;              /* the frame they belong to            */

#define B ((RBig *)(void *)RB_BASE)    /* the house's claim                   */
#define room_stats (B->stats)

/* ---- the frame's geometry (room_ref's RR, plus the platform's bounds) ---- */
typedef struct RG {
    const DoomoRoomView *v;
    int k, W, D, ox, oy, wall_h, level, walls;
    int vx, vy, vw, vh;                /* viewport (clipped to the screen)    */
    int dy0, dy1;                      /* drawable rows: between the HUD bands */
    int my0, my1;                      /* managed rows: viewport and bands    */
    int bx0, bx1, nby;                 /* managed block columns / rows        */
    uint8_t bg, corner, pad0, fl_g;    /* shell colours (lit)                 */
    uint8_t fl_a, fl_b, plank, pad;
    int roof_on, ry0, ry1;             /* (HART) the roof and its rows         */
    int roof_tab, nsp[2];              /* the tables hold it; special ranges:   */
    int16_t sp[2][4][5];               /*   sorted, disjoint [a, b) (even ends),
                                          VRAM offset, rel of its row 0, rows */
    int fy1, ftab, ftw;                /* the feature's rows end here; its rows
                                          are in RROOF_FEAT, ftw bytes each      */
    RoomRoof roof;
    RoomBackdrop bd;                   /* (H5) the door's backdrop (open kinds) */
    uint8_t lipc[5];                   /* (H5) room_lip_colours                 */
} RG;

/* (HART) the slope's row d of column p (side B's corner column p = 0
 * stands on the corner post's top). The roof's helpers are `noipa`: with
 * this file's mixed -Os / -O2 functions, GCC 13's IPA register allocation
 * once assumed a callee kept r1 (the side) across its call and the next
 * call read a code address as the side. */
#define roof_d(f, side, p, y) ((f)->top - (y) + ((side) ? ((p) ? ((p) + 2) >> 1 : 0) : (p) >> 1))
#define NOIPA __attribute__((noipa))

static inline uint8_t lit(const RG *g, uint8_t c)
{
    return g->level >= 15 ? c : doomo_colormap[g->level][c];
}

static inline uint32_t mix(uint32_t h, uint32_t v)
{
    return (h ^ v) * 16777619u;
}

/* ---- VRAM helpers --------------------------------------------------------- */
typedef volatile uint16_t vu16_t;

HOT static void put_px(int x, int y, uint8_t c)
{
    vu16_t *p = (vu16_t *)(gba_back + y * LINE + (x >> 1));
    if (x & 1) *p = (uint16_t)((*p & 0x00FF) | (c << 8));
    else *p = (uint16_t)((*p & 0xFF00) | c);
}

/* pixels [x0, x1) of row y from src (src[0] is x0's); clipped by the caller.
 * Halfword-aligned copies (even x0, even src: walls, floor and most runs)
 * go by DMA3 (2 cycles a pixel from EWRAM, no call into IWRAM); the rest
 * through ow_out (which reads up to 4 bytes past the end: every source has
 * 4 spare). */
HOT static void row_copy(int y, int x0, int x1, const uint8_t *src)
{
    uint16_t *row = gba_back + y * LINE;
    int n;
    if (x0 >= x1) return;
    if (x0 & 1) {
        put_px(x0, y, src[0]);
        x0++;
        src++;
    }
    n = (x1 - x0) & ~1;
    if (n > 0) {
        if (!((uintptr_t)src & 1)) {
            REG_DMA3SAD = (uint32_t)(uintptr_t)src;
            REG_DMA3DAD = (uint32_t)(uintptr_t)(row + (x0 >> 1));
            REG_DMA3CNT = (uint32_t)(n >> 1) | DMA_16 | DMA_ENABLE;
        } else {
            ow_out(row + (x0 >> 1), src, n);
        }
    }
    if ((x1 - x0) & 1) put_px(x1 - 1, y, src[n]);
}

HOT static void fill(int x0, int y0, int x1, int y1, uint8_t c)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x0 < x1 && y0 < y1) vid_rect(x0, y0, x1, y1, c ? c : PAL_BLACK);
}

/* ---- the claim -------------------------------------------------------------- */
static void reset_state(void)
{
    rb_epoch = (rb_epoch * 1103515245u + 12345u + gba_frames) | 1u;
    memset(B, 0, offsetof(RBig, lut));             /* caches rebuild on demand */
    B->magic = RB_MAGIC ^ rb_epoch;
    B->shell_key = 0xFFFFFFFFu;
    B->ocache_key = 0xFFFFFFFFu;
    B->nsort_prev = 0xFFFF;
}

static void claim(void)
{
    static uint32_t gen;                /* a new scratch claim may cover RBig */
    uint32_t g = scene_scratch_gen();
    gba_big_claim(GBA_BIG_BY_ROOM);
    if (!rb_epoch || B->magic != (RB_MAGIC ^ rb_epoch) || rb_last_frame + 1u != gba_frames || g != gen)
        reset_state();
    gen = g;
}

void room_invalidate(void)
{
    if (rb_epoch && B->magic == (RB_MAGIC ^ rb_epoch)) {
        B->page[0].valid = B->page[1].valid = 0;
        B->shell_key = 0xFFFFFFFFu;
    }
    rb_last_frame = 0xFFFFFF00u;
}

/* Opaque UI over the room (C7.3).
 *  - Registered AFTER plat_room (the HUD bars today): full-width bars at the
 *    top and bottom become the drawable band of the next frame; the
 *    renderer never draws or checks under them.
 *  - Registered BEFORE plat_room in the same frame (a modal window the
 *    scene is about to draw): its whole 16x8 blocks are not checked this
 *    frame, so a static window costs nothing; when it is gone, the check
 *    finds the window's pixels and restores the room under it. Up to 4. */
void plat_room_ui_opaque(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (rb_last_frame != gba_frames) {                  /* before plat_room */
        if (op_frame != gba_frames) { op_frame = gba_frames; op_n = 0; }
        if (op_n < 4) {
            uint8_t *r = op_rect[op_n++];
            r[0] = (uint8_t)DOOMO_CLAMP(x, 0, SCREEN_W); r[1] = (uint8_t)DOOMO_CLAMP(y, 0, SCREEN_H);
            r[2] = (uint8_t)DOOMO_CLAMP(x + w, 0, SCREEN_W); r[3] = (uint8_t)DOOMO_CLAMP(y + h, 0, SCREEN_H);
        }
        return;
    }
    if (x > 0 || x + w < SCREEN_W) return;
    if (y <= 0 && y + h > band_pend[0]) band_pend[0] = (uint8_t)DOOMO_MIN(y + h, SCREEN_H / 2);
    else if (y + h >= SCREEN_H && y < band_pend[1]) band_pend[1] = (uint8_t)DOOMO_MAX(y, SCREEN_H / 2);
}

/* 1 if block (bx, y..y+rows) lies inside one of this frame's opaque rects */
static int block_opaque(int bx, int y, int rows)
{
    int i;
    if (op_frame != gba_frames) return 0;
    for (i = 0; i < op_n; i++) {
        const uint8_t *r = op_rect[i];
        if (bx * 16 >= r[0] && bx * 16 + 16 <= r[2] && y >= r[1] && y + rows <= r[3]) return 1;
    }
    return 0;
}

/* ---- geometry ---------------------------------------------------------------- */
static void rot_box(int k, int w, int d, int *x0, int *y0, int *x1, int *y1)
{
    int i;
    for (i = 0; i < k; i++) {
        int nx0 = d - *y1, nx1 = d - *y0, ny0 = *x0, ny1 = *x1, t;   /* (H5) the turned width is d */
        *x0 = nx0; *x1 = nx1; *y0 = ny0; *y1 = ny1;
        t = w; w = d; d = t;
    }
}

static const DoomoFurn *furn_of(int id)
{
    return (id >= 0 && id < (int)doomo_furn_count) ? &doomo_furn[id] : NULL;
}

static void footprint(const DoomoFurn *f, int rot, int *w, int *d)
{
    int fw = f->fw ? f->fw : 1, fd = f->fd ? f->fd : 1;
    if (rot & 1) { *w = fd; *d = fw; } else { *w = fw; *d = fd; }
}

__attribute__((noinline)) static void geometry(RG *g, const DoomoRoomView *v)
{
    int vw, vh, total, fp, pan_x = v->pan_x, pan_y = v->pan_y;
    memset(g, 0, sizeof(*g));
    g->v = v;
    g->k = ((v->yaw + 512) >> 10) & 3;
    g->W = (g->k & 1) ? v->d : v->w;
    g->D = (g->k & 1) ? v->w : v->d;
    vw = v->vw ? v->vw : SCREEN_W;
    vh = v->vh ? v->vh : SCREEN_H;
    g->wall_h = vh >= 200 ? 64 : 48;
    g->level = 6 + (v->daylight > 15 ? 15 : v->daylight) * 9 / 15;
    g->walls = v->room_kind == ROOMK_WALLS;
    /* the view brake's 2 px wobble is a BG2 offset, not a recompose */
    if (pan_x >= -4 && pan_x <= 4 && pan_y >= -4 && pan_y <= 4) pan_x = pan_y = 0;
    room_backdrop_setup(&g->bd, v, g->k, g->W, g->D, g->wall_h, g->level, 0);
    if (!g->walls) room_lip_colours(v, g->level, 0, g->lipc);
    total = (g->walls || g->bd.style ? g->wall_h : 8) + (g->W + g->D) * 8;
    g->ox = v->vx + vw / 2 - (g->W - g->D) * 8 + pan_x;
    if (total + 4 <= vh) g->oy = v->vy + (vh - total) / 2 + (total - (g->W + g->D) * 8) + pan_y;
    else g->oy = v->vy + vh - (g->W + g->D) * 8 - 4 + pan_y;
    g->vx = DOOMO_MAX(v->vx, 0);
    g->vy = DOOMO_MAX(v->vy, 0);
    g->vw = DOOMO_MIN(v->vx + vw, SCREEN_W) - g->vx;
    g->vh = DOOMO_MIN(v->vy + vh, SCREEN_H) - g->vy;
    g->dy0 = band_cur[0];
    g->dy1 = band_cur[1];
    g->my0 = DOOMO_MAX(g->vy, g->dy0);
    g->my1 = DOOMO_MIN(g->vy + g->vh, g->dy1);
    if (g->my1 < g->my0) g->my1 = g->my0;
    g->bx0 = (g->vx + 15) >> 4;
    g->bx1 = (g->vx + g->vw) >> 4;
    g->nby = DOOMO_MIN((g->my1 - g->my0 + 7) >> 3, RB_BY);
    g->bg = g->walls ? PAL_RAMP(RAMP_VAPE, 1) : lit(g, PAL_RAMP(RAMP_SKY, 5));
    g->corner = lit(g, PAL_PAINT(v->wall_paint < 32 ? v->wall_paint : PAINT_BUBBLEGUM, 0));
    fp = v->floor_paint < 32 ? v->floor_paint : PAINT_CARAMEL;
    g->fl_a = lit(g, PAL_PAINT(fp, 1));
    g->fl_b = lit(g, PAL_PAINT(fp, 2));
    g->fl_g = lit(g, PAL_PAINT(fp, 0));
    g->plank = (v->floor & 3) == 1;
    /* (HART) room_ref's roof, same code (the reference ignores no tint the
     * GBA draws: 0 here, like the rest of the shell) */
    g->roof_on = g->walls && room_roof_setup(&g->roof, v, g->k, g->W, g->D, g->ox, g->oy, g->wall_h, g->level, 0);
    if (g->roof_on) {
        const RoomRoof *f = &g->roof;
        int side, off = 0;
        room_roof_rows(&g->roof, &g->ry0, &g->ry1);
        g->roof_tab = f->dmax < ROOM_ROOF_PAT_ROWS && f->dmax <= 34;
        /* the special ranges: merged, sorted, widened to even ends, clipped
         * to the slope; each with the screen rows it covers (d in [1, the
         * band], or up to dmax where the dormer stands up) */
        for (side = 0; side < 2; side++) {
            int16_t r[8];
            int n = room_roof_specials(f, side, r), L = side ? f->LB : f->LA, i, k, m = 0;
            for (i = 0; i < n; i++) {                          /* insertion by start */
                int a = DOOMO_MAX(r[2 * i] & ~1, 0), b = DOOMO_MIN((r[2 * i + 1] + 1) & ~1, L + ROOM_ROOF_EAVE);
                if (a >= b) continue;
                for (k = m; k > 0 && g->sp[side][k - 1][0] > a; k--) {
                    g->sp[side][k][0] = g->sp[side][k - 1][0];
                    g->sp[side][k][1] = g->sp[side][k - 1][1];
                }
                g->sp[side][k][0] = (int16_t)a;
                g->sp[side][k][1] = (int16_t)b;
                m++;
            }
            for (i = k = 0; i < m; i++) {                      /* merge overlaps */
                if (k && g->sp[side][i][0] <= g->sp[side][k - 1][1]) {
                    if (g->sp[side][i][1] > g->sp[side][k - 1][1]) g->sp[side][k - 1][1] = g->sp[side][i][1];
                } else {
                    g->sp[side][k][0] = g->sp[side][i][0];
                    g->sp[side][k][1] = g->sp[side][i][1];
                    k++;
                }
            }
            for (i = 0; i < k; i++) {
                int a = g->sp[side][i][0], b = g->sp[side][i][1], hi = f->band;
                int b0 = roof_d(f, side, a, f->top), b1 = roof_d(f, side, b - 1, f->top);
                if (f->dp && side == f->dside && a < f->dp + f->dw + 3 && b > f->dp - f->dw - 2) hi = f->dmax;
                /* d = base_off(p) - rel in [1, hi]: rel in [b0 - hi, b1 - 1] */
                g->sp[side][i][2] = (int16_t)off;
                g->sp[side][i][3] = (int16_t)(b0 - hi);
                g->sp[side][i][4] = (int16_t)(b1 - 1 - (b0 - hi) + 1);
                off += (b - a) * g->sp[side][i][4];
            }
            g->nsp[side] = k;
        }
        if (off > RROOF_SPEC_MAX) g->roof_tab = 0;
        /* the feature's last row (its lowest foot) and its table */
        g->fy1 = f->ftop;
        if (f->fx1 > f->fx0) {
            int x;
            for (x = f->fx0; x < f->fx1; x++) {
                int sd = x < f->ox, p = sd ? f->ox - 1 - x : x - f->ox, e = roof_d(f, sd, p, 0) - f->ffoot;
                if (e > g->fy1) g->fy1 = e;
            }
            g->ftw = f->fx1 - f->fx0 + 2;
            g->ftab = g->ftw * (g->fy1 - (f->ftop - 12)) <= RROOF_FEAT_MAX && !(g->ftw & 1);
        }
    }
}

static uint32_t shell_key(const RG *g)
{
    const DoomoRoomView *v = g->v;
    uint32_t h = 2166136261u;
    h = mix(h, (uint32_t)g->W | (uint32_t)g->D << 8 | (uint32_t)g->k << 16 | (uint32_t)g->walls << 20);
    h = mix(h, (uint32_t)(g->ox & 0xFFFF) | (uint32_t)(g->oy & 0xFFFF) << 16);
    h = mix(h, (uint32_t)g->wall_h | (uint32_t)g->level << 8 | (uint32_t)v->room_kind << 16);
    h = mix(h, (uint32_t)v->wallpaper | (uint32_t)v->floor << 8 | (uint32_t)v->wall_paint << 16 |
               (uint32_t)v->floor_paint << 24);
    h = mix(h, (uint32_t)g->vx | (uint32_t)g->vy << 8 | (uint32_t)g->vw << 16 | (uint32_t)g->vh << 24);
    h = mix(h, (uint32_t)g->dy0 | (uint32_t)g->dy1 << 8);
    if (g->bd.style)                    /* (H5) the door's backdrop */
        h = mix(mix(h, (uint32_t)g->bd.style | (uint32_t)g->bd.side << 8 | (uint32_t)g->bd.h << 16),
                (uint32_t)(g->bd.u0 & 0xFFFF) | (uint32_t)(g->bd.u1 & 0xFFFF) << 16);
    if (g->roof_on) {                   /* (HART) its style, colours, chimney, dormer */
        h = mix(h, (uint32_t)g->roof.style | (uint32_t)v->ceiling << 8 | (uint32_t)(g->roof.fx0 & 0xFFFF) << 16);
        h = mix(h, (uint32_t)(g->roof.ftop & 0xFFFF) | (uint32_t)(g->roof.dp & 0xFFFF) << 16);
    }
    return h;
}

/* ---- the shell caches ------------------------------------------------------- */
/* One wall column (room_ref's wall_column) into strip column c: 2 px wide,
 * v = 0 .. wall_h-1 at strip rows s0 + v. Each plat_rect becomes a span of
 * v; the panel line is the column's left pixel only. */
static void strip_col(const RG *g, uint8_t *S, int c, int u, int side, int s0)
{
    const DoomoRoomView *v = g->v;
    int paint = v->wall_paint < 32 ? v->wall_paint : PAINT_BUBBLEGUM;
    int pat = v->wallpaper == 0xFF ? 0 : (v->wallpaper & 3);
    int H = g->wall_h, cor = 4, skirt = 4, y, i;
    uint8_t base = PAL_PAINT(paint, side ? 1 : 2), alt = PAL_PAINT(paint, side ? 0 : 1);
    uint8_t ceil = PAL_RAMP(RAMP_WOOD, side ? 3 : 4), sk = PAL_RAMP(RAMP_WOOD, side ? 2 : 3);
    uint8_t fillc = base;
    uint8_t *col = S + s0 * 16 + 2 * c;
#define SPAN(a, b, mask, colr) \
    for (i = (a); i < (b); i++) { if ((mask) & 1) col[i * 16] = (colr); if ((mask) & 2) col[i * 16 + 1] = (colr); }
    if (pat == 0 && ((u >> 3) & 1)) fillc = alt;
    SPAN(0, 1, 3, lit(g, PAL_RAMP(RAMP_WOOD, 1)));
    SPAN(1, cor, 3, lit(g, ceil));
    SPAN(cor, H - skirt, 3, lit(g, fillc));
    if (pat == 1) {
        for (y = cor + 4 + (((u >> 3) & 1) << 2); y < H - skirt - 2; y += 8)
            if ((u & 7) < 2) SPAN(y, y + 2, 3, lit(g, alt));
    } else if (pat == 2) {
        SPAN(H - skirt - H / 3, H - skirt, 3, lit(g, alt));
        SPAN(H - skirt - H / 3, H - skirt - H / 3 + 1, 3, lit(g, PAL_PAINT(paint, 0)));
    } else if (pat == 3 && (u & 15) == 0) {
        SPAN(cor, H - skirt, 1, lit(g, alt));
    }
    SPAN(H - skirt, H, 3, lit(g, sk));
#undef SPAN
}

/* Floor pattern row (q, par): one 32 px period of a floor row whose upper
 * tiles have half-width h1 = 2(q+1), starting at an upper tile's left
 * grout: grout, upper fill, grout, then the visible lower tile's fill. */
__attribute__((noinline)) static void build_shell(RG *g)
{
    int q, par, o, c, r, j;
    B->strip_h = 0;
    if (g->walls && g->wall_h + 8 <= STRIP_ROWS) {
        /* the two 16 px strips (rows s = 0 .. wall_h + 7), then each strip
         * row s = 8j + r goes to phase row r: wall A at 16 (WJ - 1 - j) (x
         * grows as j falls), wall B at 16 j, so a screen row of a wall is
         * one contiguous run of one phase row */
        uint8_t *S = B->stage;
        for (c = 0; c < 2; c++) {
            uint8_t *Sc = S + c * STRIP_ROWS * 16;
            memset(Sc, 0, STRIP_ROWS * 16);
        }
        for (c = 0; c < 8; c++) {
            strip_col(g, S, c, 2 * c, 0, c);                                  /* A: u = 2c      */
            strip_col(g, S + STRIP_ROWS * 16, c, (16 - 2 * c) & 15, 1, 8 - c);  /* B: u = 16 - 2c */
        }
        for (r = 0; r < 8; r++)
            for (j = 0; j < WJ; j++) {
                memcpy(B->wall[0][r] + 16 * (WJ - 1 - j), S + (8 * j + r) * 16, 16);
                memcpy(B->wall[1][r] + 16 * j, S + STRIP_ROWS * 16 + (8 * j + r) * 16, 16);
            }
        B->strip_h = (uint8_t)(g->wall_h + 8);
    }
    if (g->roof_on && g->roof_tab) {                /* (HART) the roof's tables */
        const RoomRoof *f = &g->roof;
        uint16_t tmp[ROOM_ROOF_EAVE + 128];
        uint8_t *t8 = (uint8_t *)tmp;
        for (c = 0; c < 2; c++) {
            int i, p, w;
            for (r = 0; r < 8 * RPH_G; r++) {         /* S = r: s = r - 40 */
                int st = r - RPH_S0, gi = r >> 3;
                volatile uint16_t *d = (volatile uint16_t *)(RROOF_PHASE[c][r & 7] + 16 * (c ? gi : RPH_G - 1 - gi));
                for (j = 0; j < 16; j++) {
                    if (!c) t8[j] = room_roof_pattern(f, 0, j, (j >> 1) - st);
                    else t8[15 - j] = room_roof_pattern(f, 1, j, ((j + 2) >> 1) - st);
                }
                for (j = 0; j < 8; j++) d[j] = tmp[j];
            }
            if (!c && g->ftab) {                     /* the chimney / tank rows */
                int y;
                for (y = f->ftop - 12; y < g->fy1; y++) {
                    volatile uint16_t *d = (volatile uint16_t *)(RROOF_FEAT + (y - (f->ftop - 12)) * g->ftw);
                    memset(t8, 0, (size_t)g->ftw);
                    room_roof_feature_row(f, y, f->fx0 - 1, f->fx1 + 1, t8);
                    for (j = 0; j < g->ftw / 2; j++) d[j] = tmp[j];
                }
            }
            for (i = 0; i < g->nsp[c]; i++) {
                const int16_t *e = g->sp[c][i];
                w = e[1] - e[0];
                for (r = 0; r < e[4]; r++) {
                    volatile uint16_t *d = (volatile uint16_t *)(RROOF_SPEC + e[2] + r * w);
                    int rel = e[3] + r;
                    for (p = e[0]; p < e[1]; p++)
                        t8[c ? e[1] - 1 - p : p - e[0]] = room_roof_slope(f, c, p, roof_d(f, c, p, f->top + rel));
                    for (j = 0; j < w / 2; j++) d[j] = tmp[j];
                }
            }
        }
    }
    for (q = 0; q < 8; q++)
        for (par = 0; par < 2; par++) {
            int h1 = 2 * (q + 1);
            uint8_t fu, fl, *row = B->pat[q * 2 + par];
            if (g->plank) fu = fl = ((q >> 2) & 1) ? g->fl_a : g->fl_b;
            else { fu = par ? g->fl_a : g->fl_b; fl = par ? g->fl_b : g->fl_a; }
            for (o = 0; o < 32; o++)
                row[o] = (o == 0 || o == 2 * h1 - 1) ? g->fl_g : (o < 2 * h1 ? fu : fl);
            for (o = 32; o < PAT_W + 4; o++) row[o] = row[o & 31];
        }
}

/* ---- the shell, row by row ---------------------------------------------------- */
HOT static void wall_rows(const RG *g, int y, int x0, int x1)
{
    const int H = g->wall_h, tA = y - g->oy + H, a = tA >> 3, r = tA & 7;
    int lo, hi, s, m, xa, xb;
    /* wall A' (y' = 0): strip i at x = ox + 16i, its column m on strip rows
     * [m, m + H): the run spans strips lo..hi */
    lo = DOOMO_MAX((tA - H - 6 + 7) >> 3, 0);
    hi = DOOMO_MIN(a, g->W - 1);
    /* wall A covers x in [ox + 2(tA - H + 1), ox + 2 tA + 2) on this row */
    if (lo <= hi && x1 > g->ox + 2 * (tA - H + 1) && x0 < g->ox + 2 * tA + 2) {
        s = tA - 8 * lo;
        m = DOOMO_MAX(s - H + 1, 0);
        xa = g->ox + 16 * lo + 2 * m;
        s = tA - 8 * hi;
        xb = g->ox + 16 * hi + 2 * DOOMO_MIN(s, 7) + 2;
        if (xa < x0) xa = x0;
        if (xb > x1) xb = x1;
        if (xa < xb) row_copy(y, xa, xb, B->wall[0][r] + (16 * (WJ - 1 - a) + (xa - g->ox)));
    }
    /* wall B' (x' = 0): strip i at x = ox - 16i - 16, column c on strip rows
     * [8 - c, 8 - c + H): x grows as i falls */
    lo = DOOMO_MAX((tA - H - 7 + 7) >> 3, 0);
    hi = DOOMO_MIN((tA - 1) >> 3, g->D - 1);
    /* wall B covers x in [ox - 2 tA, ox - 2(tA - H)) on this row */
    if (lo <= hi && x1 > g->ox - 2 * tA - 2 && x0 < g->ox - 2 * (tA - H) + 2) {
        s = tA - 8 * hi;
        xa = g->ox - 16 * hi - 16 + 2 * DOOMO_MAX(8 - s, 0);
        s = tA - 8 * lo;
        xb = g->ox - 16 * lo - 16 + 2 * DOOMO_MIN(7 - s + H, 7) + 2;
        if (xa < x0) xa = x0;
        if (xb > x1) xb = x1;
        if (xa < xb) row_copy(y, xa, xb, B->wall[1][r] + (xa - (g->ox - 16 * (a + 1))));
    }
    /* the corner post */
    if (y >= g->oy - H && y < g->oy) {
        if (g->ox - 1 >= x0 && g->ox - 1 < x1) put_px(g->ox - 1, y, g->corner);
        if (g->ox >= x0 && g->ox < x1) put_px(g->ox, y, g->corner);
    }
}

HOT static void floor_row(const RG *g, int y, int x0, int x1)
{
    const int W = g->W, D = g->D, t = y - g->oy;
    int s, q, h1, h2, xL, xR, lp = 0, rp = 0, p0, a, b;
    uint8_t fl;
    const uint8_t *pat;
    if (t < 0 || t >= 8 * (W + D)) return;
    s = t >> 3;
    q = t & 7;
    h1 = 2 * (q + 1);
    h2 = 16 - 2 * q;
    /* the leftmost tile: an upper one (0, s) on the back edge, else the
     * lower (s - D, D - 1) on the front edge, whose left grout shows */
    if (s <= D - 1) xL = g->ox - 16 * s - h1;
    else { xL = g->ox + 16 * (s - 2 * D + 1) - h2; lp = 1; }
    if (s <= W - 1) xR = g->ox + 16 * s + h1;
    else { xR = g->ox + 16 * (2 * W - 1 - s) + h2; rp = 1; }
    pat = B->pat[q * 2 + (s & 1)];
    p0 = g->ox - 16 * s - h1;                      /* an upper tile's left grout */
    a = DOOMO_MAX(xL, x0);
    b = DOOMO_MIN(xR, x1);
    while (a < b) {
        int o = (a - p0) & 31, n = DOOMO_MIN(b - a, PAT_W - o);
        row_copy(y, a, a + n, pat + o);
        a += n;
    }
    if (g->plank) fl = ((q >> 2) & 1) ? g->fl_a : g->fl_b;
    else fl = ((s - 1) & 1) ? g->fl_a : g->fl_b;
    if (lp) {
        if (xL >= x0 && xL < x1) put_px(xL, y, g->fl_g);
        if (xL + 1 >= x0 && xL + 1 < x1) put_px(xL + 1, y, fl);
    }
    if (rp) {
        if (xR - 2 >= x0 && xR - 2 < x1) put_px(xR - 2, y, fl);
        if (xR - 1 >= x0 && xR - 1 < x1) put_px(xR - 1, y, g->fl_g);
    }
}

/* (H5) the plot edge lip of open rooms, room_ref's: on each front edge a
 * 2 px column per 2 units, ROOM_LIP_H rows from the edge row (row 0 the
 * turf edge, then soil, the last row darker). Column u of front A' covers
 * rows [oy + ((u + 16D) >> 1), + ROOM_LIP_H): on row y the columns with
 * (u + 16D) >> 1 in [t - LIP_H + 1, t], t = y - oy, i.e. u in
 * [2(t - LIP_H + 1) - 16D, 2t + 1 - 16D]; the row index r = t - ((u + 16D)
 * >> 1) is constant over each pair of columns. Front A' first, then B'. */
HOT static void lip_row(const RG *g, int y, int x0, int x1)
{
    int t = y - g->oy, side;
    for (side = 0; side < 2; side++) {
        int L = side ? 16 * g->D : 16 * g->W, off = side ? 16 * g->W : 16 * g->D, u;
        int ua = DOOMO_MAX(2 * (t - ROOM_LIP_H + 1) - off, 0), ub = DOOMO_MIN(2 * t + 1 - off, L - 1);
        ua &= ~1;
        for (u = ua; u <= ub; u += 2) {
            int r = t - ((u + off) >> 1), x = side ? g->ox + 16 * g->W - u - 2 : g->ox - 16 * g->D + u, xa, xb;
            uint8_t c;
            if (r < 0 || r >= ROOM_LIP_H) continue;
            c = r == 0 ? g->lipc[0] : r == ROOM_LIP_H - 1 ? g->lipc[3 + side] : g->lipc[1 + side];
            xa = DOOMO_MAX(x, x0);
            xb = DOOMO_MIN(x + 2, x1);
            if (xa < xb) fill(xa, y, xb, y + 1, c);
        }
    }
}

/* (H5) the door's backdrop on row y: column u of side A' at x = ox + u
 * spans rows [oy + (u >> 1) - h, oy + (u >> 1)), of side B' at x = ox -
 * u - 2 rows [oy + ((u + 2) >> 1) - h, ...): room_backdrop_px per pixel
 * pair (only the Garden's / the Park Plot's few dozen columns) */
HOT static void backdrop_row(const RG *g, int y, int x0, int x1)
{
    const RoomBackdrop *b = &g->bd;
    int u;
    for (u = b->u0; u < b->u1; u += 2) {
        int x = b->side ? g->ox - u - 2 : g->ox + u;
        int t = y - (g->oy + (b->side ? (u + 2) >> 1 : u >> 1) - b->h), xa, xb;
        uint8_t c;
        if (t < 0 || t >= b->h || x + 2 <= x0 || x >= x1) continue;
        c = room_backdrop_px(b, u, t);
        if (!c) continue;
        xa = DOOMO_MAX(x, x0);
        xb = DOOMO_MIN(x + 2, x1);
        fill(xa, y, xb, y + 1, c);
    }
}



/* (HART) a run of row y from a VRAM table: one halfword DMA when it is
 * even-aligned at both ends (it nearly always is), else row_copy */
#define ALWAYS_INLINE inline __attribute__((always_inline))
static inline void dma16(uint16_t *dst, const void *src, int halves);
static ALWAYS_INLINE void roof_copy(int y, int x0, int x1, const uint8_t *src)
{
    if (!((x0 | x1 | (int)(uintptr_t)src) & 1)) {
        if (x1 > x0) dma16(gba_back + y * LINE + (x0 >> 1), src, (x1 - x0) >> 1);
    } else {
        row_copy(y, x0, x1, src);
    }
}

/* (HART) columns [*p0, *p1) of a side clipped to [c0, c1) and narrowed until
 * the end columns' d is in [lo, hi] (d falls by one every two columns) */
static ALWAYS_INLINE int roof_span(const RoomRoof *f, int side, int y, int c0, int c1, int lo, int hi,
                                   int *p0, int *p1)
{
    int L = side ? f->LB : f->LA, rel = y - f->top, a, b;
    a = DOOMO_MAX(DOOMO_MAX(2 * (rel + lo) - 3, 0), c0);
    b = DOOMO_MIN(DOOMO_MIN(2 * (rel + hi) + 3, L + ROOM_ROOF_EAVE), c1);
    while (a < b && roof_d(f, side, a, y) < lo) a++;
    while (a < b && roof_d(f, side, b - 1, y) > hi) b--;
    *p0 = a;
    *p1 = b;
    return a < b;
}

/* (HART) one side's columns [p0, p1) of row y, every d in [1, band]:
 * straight copies, the pattern's phase row in one run between the special
 * ranges, whose stored rows are copied as they are - the same pixels as
 * room_roof_row() */
static ALWAYS_INLINE void roof_band(const RG *g, int side, int y, int p0, int p1)
{
    const RoomRoof *f = &g->roof;
    const int n = g->nsp[side], rel = y - f->top, ox = f->ox;
    int p = p0, k = 0;
    while (p < p1) {
        int nb = p1;
        while (k < n && g->sp[side][k][1] <= p) k++;
        if (k < n && p >= g->sp[side][k][0]) {
            const int16_t *e = g->sp[side][k];
            const uint8_t *row = RROOF_SPEC + e[2] + (rel - e[3]) * (e[1] - e[0]);
            nb = DOOMO_MIN(nb, e[1]);
            if (!side) roof_copy(y, ox + p, ox + nb, row + (p - e[0]));
            else roof_copy(y, ox - nb, ox - p, row + (e[1] - nb));
            p = nb;
            continue;
        }
        if (k < n) nb = DOOMO_MIN(nb, g->sp[side][k][0]);
        if (!side) {
            int S = rel - 8 * (p >> 4) + RPH_S0;
            roof_copy(y, ox + p, ox + nb, RROOF_PHASE[0][S & 7] + 16 * (RPH_G - 1 - (S >> 3)) + (p & 15));
        } else {
            int q = nb - 1, S = rel - 8 * (q >> 4) + RPH_S0;
            roof_copy(y, ox - nb, ox - p, RROOF_PHASE[1][S & 7] + 16 * (S >> 3) + (15 - (q & 15)));
        }
        p = nb;
    }
}

/* (HART) one side's columns [p0, p1) of row y above the band (the dormer's
 * gable, the spikes): pixel by pixel from the same tables, into o[] (o[0]
 * is column p0; side B runs leftwards); 0 where the background shows */
NOIPA HOT static void roof_above(const RG *g, int side, int y, int p0, int p1, uint8_t *o)
{
    const RoomRoof *f = &g->roof;
    const int n = g->nsp[side], rel = y - f->top;
    int p, k = 0;
    for (p = p0; p < p1; p++) {
        uint8_t c = 0;
        int d = roof_d(f, side, p, y);
        while (k < n && g->sp[side][k][1] <= p) k++;
        if (k < n && p >= g->sp[side][k][0]) {
            const int16_t *e = g->sp[side][k];
            if (rel >= e[3] && rel < e[3] + e[4])
                c = RROOF_SPEC[e[2] + (rel - e[3]) * (e[1] - e[0]) + (side ? e[1] - 1 - p : p - e[0])];
        } else if ((unsigned)(rel - 8 * (p >> 4) + RPH_S0) < 8 * RPH_G && d >= 1) {
            int S = rel - 8 * (p >> 4) + RPH_S0;
            c = side ? RROOF_PHASE[1][S & 7][16 * (S >> 3) + 15 - (p & 15)]
                     : RROOF_PHASE[0][S & 7][16 * (RPH_G - 1 - (S >> 3)) + (p & 15)];
        }
        o[side ? p1 - 1 - p : p - p0] = c;
    }
}

/* the non-zero runs of buf (screen x [x0, x1)) on row y */
NOIPA HOT static void roof_runs(int y, int x0, int x1, const uint8_t *buf)
{
    int a = x0, b;
    while (a < x1) {
        while (a < x1 && !buf[a - x0]) a++;
        for (b = a; b < x1 && buf[b - x0]; b++) {}
        if (a < b) row_copy(y, a, b, buf + (a - x0));
        a = b;
    }
}

/* (HART) the roof's pixels on row y within [x0, x1): per side the band
 * (every pixel opaque: copies), what stands above it (the dormer's gable,
 * the spikes), then the chimney / tank */
NOIPA HOT static void roof_row(const RG *g, int y, int x0, int x1)
{
    uint8_t buf[SCREEN_W + 4];
    const RoomRoof *f = &g->roof;
    int side, p0, p1, xa;
    if (x0 < 0) x0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (x0 >= x1) return;
    if (!g->roof_tab) {
        if (room_roof_row(f, y, x0, x1, buf)) roof_runs(y, x0, x1, buf);
        return;
    }
    for (side = 0; side < 2; side++) {
        /* the rect: side A x = ox + p, side B x = ox - 1 - p */
        int c0 = side ? f->ox - x1 : x0 - f->ox, c1 = side ? f->ox - x0 : x1 - f->ox;
        if (roof_span(f, side, y, c0, c1, 1, f->band, &p0, &p1)) roof_band(g, side, y, p0, p1);
        if (f->dmax <= f->band) continue;
        if (f->style != ROOF_SPIKE) {                   /* only the dormer stands up */
            if (!f->dp || side != f->dside) continue;
            c0 = DOOMO_MAX(c0, f->dp - f->dw - 2);
            c1 = DOOMO_MIN(c1, f->dp + f->dw + 3);
        }
        if (roof_span(f, side, y, c0, c1, f->band + 1, f->dmax, &p0, &p1)) {
            roof_above(g, side, y, p0, p1, buf);
            xa = side ? f->ox - p1 : f->ox + p0;
            roof_runs(y, xa, xa + (p1 - p0), buf);
        }
    }
    if (f->fx1 > f->fx0 && y >= f->ftop - 12 && y < g->fy1) {
        int fa = DOOMO_MAX(x0, f->fx0 - 1), fb = DOOMO_MIN(x1, f->fx1 + 1);
        if (fa >= fb) return;
        if (g->ftab) {
            roof_runs(y, fa, fb, RROOF_FEAT + (y - (f->ftop - 12)) * g->ftw + (fa - (f->fx0 - 1)));
        } else {
            memset(buf, 0, (size_t)(fb - fa));
            if (room_roof_feature_row(f, y, fa, fb, buf)) roof_runs(y, fa, fb, buf);
        }
    }
}

HOT static void draw_shell(const RG *g, int x0, int y0, int x1, int y1)
{
    const int fy0 = g->oy, fy1 = g->oy + 8 * (g->W + g->D);
    const int wy0 = g->oy - g->wall_h, wy1 = g->oy + 8 * DOOMO_MAX(g->W, g->D) + 1;
    const int walls = g->walls && B->strip_h == g->wall_h + 8;
    int y;
    /* the background: the viewport only (room_ref's plat_rect) */
    fill(DOOMO_MAX(x0, g->vx), DOOMO_MAX(y0, g->vy), DOOMO_MIN(x1, g->vx + g->vw),
         DOOMO_MIN(y1, g->vy + g->vh), g->bg);
    for (y = y0; y < y1; y++) {
        if (g->roof_on && y >= g->ry0 && y < g->ry1) roof_row(g, y, x0, x1);
        if (g->bd.style) backdrop_row(g, y, x0, x1);
        if (walls && y >= wy0 && y < wy1) wall_rows(g, y, x0, x1);
        if (y >= fy0 && y < fy1) floor_row(g, y, x0, x1);
        if (!g->walls) lip_row(g, y, x0, x1);
    }
}

/* ---- LUTs: paint recolour composed with the light level -------------------- */
static uint8_t recolour(uint8_t c, int pa, int pb)
{
    int s;
    if (pa >= 0 && pa < 32) {
        for (s = 0; s < 3; s++) if (c == PAL_PAINT(FURN_MARK_A, s)) return PAL_PAINT(pa, s);
        if (c == palette_darker(PAL_PAINT(FURN_MARK_A, 0))) return palette_darker(PAL_PAINT(pa, 0));
    }
    if (pb >= 0 && pb < 32) {
        for (s = 0; s < 3; s++) if (c == PAL_PAINT(FURN_MARK_B, s)) return PAL_PAINT(pb, s);
        if (c == palette_darker(PAL_PAINT(FURN_MARK_B, 0))) return palette_darker(PAL_PAINT(pb, 0));
    }
    return c;
}

/* key: pa | pb << 8 | lvl << 16, paints 0xFF when they change nothing */
static uint32_t lut_key(int pa, int pb, int lvl)
{
    if (pa < 0 || pa >= 32 || pa == FURN_MARK_A) pa = 0xFF;
    if (pb < 0 || pb >= 32 || pb == FURN_MARK_B) pb = 0xFF;
    return (uint32_t)pa | (uint32_t)pb << 8 | (uint32_t)lvl << 16;
}

static const uint8_t *lut_get(uint32_t key)
{
    int pa = (int)(key & 0xFF), pb = (int)((key >> 8) & 0xFF), lvl = (int)(key >> 16), i, s;
    const uint8_t *cm = doomo_colormap[lvl > 15 ? 15 : lvl];
    uint8_t *l;
    static const uint8_t marks[2] = { FURN_MARK_A, FURN_MARK_B };
    if (pa == 0xFF && pb == 0xFF) return cm;      /* the colormap row itself */
    for (i = 0; i < LUT_SLOTS; i++)
        if (B->lut_key[i] == key + 1u) return B->lut[i];
    i = B->lut_next++ % LUT_SLOTS;
    l = B->lut[i];
    B->lut_key[i] = key + 1u;
    memcpy(l, cm, 256);
    for (s = 0; s < 2; s++) {
        int m = marks[s], k;
        for (k = 0; k < 4; k++) {
            uint8_t c = k < 3 ? PAL_PAINT(m, k) : palette_darker(PAL_PAINT(m, 0));
            l[c] = cm[recolour(c, pa == 0xFF ? -1 : pa, pb == 0xFF ? -1 : pb)];
        }
    }
    return l;
}

/* ---- the draw list ------------------------------------------------------------- */
static const DoomoFurnIso *iso_of(const RItem *it)
{
    return it->iso == ISO_NONE ? NULL : &doomo_furn_iso[it->iso];
}

/* A piece at rotated tile box (x0, y0)-(x1, y1), facing rot (view space). */
static void piece_item(const RG *g, RItem *it, const DoomoRoomObj *o, int rot, int x0, int y0, int x1, int y1)
{
    const DoomoFurn *f = furn_of(o->furn);
    int facing = rot & 1, flip = rot >= 2, frames, e;
    int sx = g->ox + 16 * (x0 - y0), sy = g->oy + 8 * (x0 + y0) - o->z;
    int lvl = (o->flags & RO_FULLBRIGHT) ? 15 : g->level;
    uint32_t h;
    if (o->jitter) sx += (int)((g->v->time >> 1) & 1) * o->jitter - o->jitter / 2;
    it->lut = lut_key(o->paint_a == 0xFF ? -1 : o->paint_a, o->paint_b == 0xFF ? -1 : o->paint_b, lvl);
    it->flip = (uint8_t)flip;
    it->iso = ISO_NONE;
    frames = f->frames ? f->frames : 1;
    if (doomo_furn_iso_count && o->furn < doomo_furn_count) {
        e = doomo_furn_iso_first[o->furn] + (RO_ANIM(o) % frames) * 2 + facing;
        if (e < doomo_furn_iso_count && doomo_furn_iso[e].w && doomo_furn_iso[e].h) it->iso = (uint16_t)e;
    }
    if (it->iso != ISO_NONE) {
        const DoomoFurnIso *im = &doomo_furn_iso[it->iso];
        it->px = (int16_t)(sx - (flip ? im->w - im->ox : im->ox));
        it->py = (int16_t)(sy - im->oy);
        it->x0 = it->px; it->y0 = it->py;
        it->x1 = (int16_t)(it->px + im->w); it->y1 = (int16_t)(it->py + im->h);
    } else {
        /* room_ref's shaded box of the footprint (no sprite) */
        int hh = f->fh ? f->fh * 4 : 2;
        it->bx0 = (int8_t)x0; it->by0 = (int8_t)y0; it->bx1 = (int8_t)x1; it->by1 = (int8_t)y1;
        it->x0 = (int16_t)(g->ox + (x0 * 16 + 1) - (y1 * 16 - 1));
        it->x1 = (int16_t)(g->ox + (x1 * 16 - 1) - (y0 * 16 + 1) + 2);
        it->y0 = (int16_t)(g->oy + ((x0 + y0) * 16 >> 1) - o->z - hh - 2);
        it->y1 = (int16_t)(g->oy + ((x1 + y1) * 16 >> 1) - o->z + 2);
        it->px = (int16_t)o->z;
        it->py = (int16_t)hh;
    }
    h = mix(2166136261u, (uint32_t)it->kind | (uint32_t)o->furn << 8 | (uint32_t)it->flip << 20);
    h = mix(h, it->iso | (uint32_t)(o->flags & (RO_GHOST | RO_FULLBRIGHT)) << 16);
    h = mix(h, (uint32_t)(it->px & 0xFFFF) | (uint32_t)(it->py & 0xFFFF) << 16);
    h = mix(h, it->lut);
    h = mix(h, (uint32_t)(uint8_t)o->sq_x | (uint32_t)(uint8_t)o->sq_y << 8 |
               (uint32_t)(x0 & 0xFF) << 16 | (uint32_t)(y0 & 0xFF) << 24);
    if (it->iso == ISO_NONE) h = mix(h, (uint32_t)(x1 & 0xFF) | (uint32_t)(y1 & 0xFF) << 8 | (uint32_t)f->fh << 16 |
                                        (uint32_t)(o->paint_a) << 24);
    it->sig = h;
    it->xf = (uint8_t)((o->sq_x || o->sq_y) && it->iso != ISO_NONE);
}

/* room_ref's back-to-front order (behind(), the free-candidate pick with
 * its keys, the cycle fallback), cached on the items' boxes */
static int behind(const RSort *a, const RSort *b)
{
    int ox = a->x0 < b->x1 && b->x0 < a->x1, oy = a->y0 < b->y1 && b->y0 < a->y1;
    if (ox && oy) return a->layer < b->layer || (a->layer == b->layer && a->z < b->z);
    if (a->x1 <= b->x0 && oy) return 1;
    if (a->y1 <= b->y0 && ox) return 1;
    if (a->x1 <= b->x0 && a->y1 <= b->y0) return 1;
    return 0;
}

/* lowest set bit of a non-zero word (de Bruijn; the ARM7 has no CLZ) */
static int ctz32(uint32_t w)
{
    static const uint8_t db[32] = { 0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
                                    31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9 };
    return db[((w & (0u - w)) * 0x077CB531u) >> 27];
}

__attribute__((noinline)) static void sort_order(int rebuild)
{
    const int n = B->nsort;
    int16_t key[RB_ITEMS], key2[RB_ITEMS];
    uint8_t pred[RB_ITEMS], chg[RB_ITEMS];
    uint32_t undone[2] = { 0, 0 }, freeb[2] = { 0, 0 };
    int i, j, t, any = 0;
    if (n != B->nsort_prev) rebuild = 1;
    for (i = 0; i < n; i++) {
        chg[i] = (uint8_t)(rebuild || memcmp(&B->sort[i], &B->sort_prev[i], sizeof(RSort)) != 0);
        any |= chg[i];
    }
    if (!any) return;
    /* the behind-edges of every pair that involves a changed entry */
    for (i = 0; i < n; i++) {
        if (!chg[i]) continue;
        B->succ[i][0] = B->succ[i][1] = 0;
        for (j = 0; j < n; j++) B->succ[j][i >> 5] &= ~(1u << (i & 31));
    }
    for (i = 0; i < n; i++) {
        if (!chg[i]) continue;
        for (j = 0; j < n; j++) {
            int bij, bji;
            if (j == i || (chg[j] && j < i)) continue;
            bij = behind(&B->sort[i], &B->sort[j]);
            bji = behind(&B->sort[j], &B->sort[i]);
            if (bij && !bji) B->succ[i][j >> 5] |= 1u << (j & 31);
            if (bji && !bij) B->succ[j][i >> 5] |= 1u << (i & 31);
        }
    }
    memcpy(B->sort_prev, B->sort, (size_t)n * sizeof(RSort));
    B->nsort_prev = (uint16_t)n;
    /* keys, in-degrees, the free set */
    memset(pred, 0, sizeof(pred));
    for (i = 0; i < n; i++) {
        const RSort *s = &B->sort[i];
        key2[i] = (int16_t)((s->x0 + s->y0) * 8 + s->layer * 2);
        key[i] = (int16_t)(key2[i] + (s->z ? 1 : 0) - (s->layer == 0 ? 1000 : 0));
        undone[i >> 5] |= 1u << (i & 31);
        for (j = 0; j < 2; j++) {
            uint32_t w = B->succ[i][j];
            while (w) { pred[j * 32 + ctz32(w)]++; w &= w - 1; }
        }
    }
    for (i = 0; i < n; i++) if (!pred[i]) freeb[i >> 5] |= 1u << (i & 31);
    /* room_ref's pick: the free entry with the smallest key (the first on a
     * tie); none free (a cycle): the smallest plain depth key */
    for (t = 0; t < n; t++) {
        int pick = -1, best = 0x7FFF;
        for (j = 0; j < 2; j++) {
            uint32_t w = freeb[j];
            while (w) { i = j * 32 + ctz32(w); w &= w - 1; if (key[i] < best) { best = key[i]; pick = i; } }
        }
        if (pick < 0)
            for (j = 0; j < 2; j++) {
                uint32_t w = undone[j];
                while (w) { i = j * 32 + ctz32(w); w &= w - 1; if (key2[i] < best) { best = key2[i]; pick = i; } }
            }
        undone[pick >> 5] &= ~(1u << (pick & 31));
        freeb[pick >> 5] &= ~(1u << (pick & 31));
        B->order[t] = (uint8_t)pick;
        for (j = 0; j < 2; j++) {
            uint32_t w = B->succ[pick][j] & undone[j];
            while (w) {
                i = j * 32 + ctz32(w);
                w &= w - 1;
                if (--pred[i] == 0) freeb[j] |= 1u << (i & 31);
            }
        }
    }
}

/* An object's item and sort entry (okind says which), cached until the
 * object or the geometry changes. Jittering objects follow the clock. */
__attribute__((noinline)) static void object_item(const RG *g, const DoomoRoomView *v, int i)
{
    const DoomoRoomObj *o = &v->objs[i];
    const DoomoFurn *f = furn_of(o->furn);
    RItem *it = &B->oitem[i];
    RSort *s = &B->osort[i];
    int x0, y0, x1, y1, w, d;
    B->ocache[i] = *o;
    B->okind[i] = OK_SKIP;
    if (!f) return;
    if (o->flags & RO_GHOST) {
        footprint(f, RO_ROT(o), &w, &d);
        if (f->flags & FF_WALL) { w = (RO_ROT(o) & 1) ? 1 : (f->fw ? f->fw : 1); d = (RO_ROT(o) & 1) ? (f->fw ? f->fw : 1) : 1; }
        x0 = o->x; y0 = o->y; x1 = x0 + w; y1 = y0 + d;
        rot_box(g->k, v->w, v->d, &x0, &y0, &x1, &y1);
        it->kind = RK_GHOST;
        it->src = (uint16_t)i;
        piece_item(g, it, o, (RO_ROT(o) - g->k) & 3, x0, y0, x1, y1);
        B->okind[i] = OK_GHOST;
    } else if (f->flags & FF_WALL) {
        /* on one of the two visible walls, (H5) an open kind's front edge
         * (drawn after everything, as room_ref does), or cut away */
        int side = room_wall_side(v, o, g->k);
        if (side < 0) return;
        x0 = o->x; y0 = o->y;
        if (RO_ROT(o) & 1) { x1 = x0 + 1; y1 = y0 + (f->fw ? f->fw : 1); }
        else { x1 = x0 + (f->fw ? f->fw : 1); y1 = y0 + 1; }
        rot_box(g->k, v->w, v->d, &x0, &y0, &x1, &y1);
        if (side == 0) y0 = 0;
        else if (side == 1) x0 = 0;
        else if (side == 2) y0 = g->D;
        else x0 = g->W;
        it->kind = RK_WALL;
        it->src = (uint16_t)i;
        piece_item(g, it, o, side & 1, x0, y0, x1, y1);
        B->okind[i] = side >= 2 ? OK_FRONT : OK_WALL;
    } else {
        footprint(f, RO_ROT(o), &w, &d);
        x0 = o->x; y0 = o->y; x1 = x0 + w; y1 = y0 + d;
        rot_box(g->k, v->w, v->d, &x0, &y0, &x1, &y1);
        s->x0 = (int8_t)x0; s->y0 = (int8_t)y0; s->x1 = (int8_t)x1; s->y1 = (int8_t)y1;
        s->layer = (uint8_t)((f->flags & FF_RUG) ? 0 : 1);
        s->z = o->z;
        s->idx = (int16_t)i;
        it->kind = RK_PIECE;
        it->src = (uint16_t)i;
        piece_item(g, it, o, (RO_ROT(o) - g->k) & 3, x0, y0, x1, y1);
        B->okind[i] = OK_FLOOR;
    }
}

__attribute__((noinline)) static int build_items(const RG *g, const DoomoRoomView *v)
{
    RItem *L = B->item;
    int n = 0, i, k, ns = 0, rebuild;
    const int nobj = DOOMO_MIN(v->nobj, RB_OBJS), nact = DOOMO_MIN(v->nact, 16);
    uint32_t key = mix(mix(B->shell_key, (uint32_t)v->w | (uint32_t)v->d << 8), (uint32_t)nobj);
    rebuild = key != B->ocache_key;
    B->ocache_key = key;
    for (i = 0; i < nobj; i++)
        if (rebuild || v->objs[i].jitter || memcmp(&B->ocache[i], &v->objs[i], sizeof(DoomoRoomObj)) != 0)
            object_item(g, v, i);
    /* 1. wall pieces on the two visible walls, in object order */
    for (i = 0; i < nobj; i++)
        if (B->okind[i] == OK_WALL) L[n++] = B->oitem[i];
    /* 2. rugs, floor pieces and actors in room_ref's order */
    for (i = 0; i < nobj; i++)
        if (B->okind[i] == OK_FLOOR) B->sort[ns++] = B->osort[i];
    for (i = 0; i < nact && ns < RB_ITEMS - 2; i++) {
        int x0 = FIX2INT(v->actors[i].x), y0 = FIX2INT(v->actors[i].y), x1 = x0 + 1, y1 = y0 + 1;
        RSort *s = &B->sort[ns];
        rot_box(g->k, v->w, v->d, &x0, &y0, &x1, &y1);
        s->x0 = (int8_t)x0; s->y0 = (int8_t)y0; s->x1 = (int8_t)x1; s->y1 = (int8_t)y1;
        s->layer = 2;
        s->z = 0;
        s->idx = (int16_t)(-1 - i);
        ns++;
    }
    B->nsort = (uint16_t)ns;
    sort_order(rebuild);
    for (k = 0; k < ns && n < RB_ITEMS - 2; k++) {
        const RSort *s = &B->sort[B->order[k]];
        if (s->idx >= 0) {
            L[n++] = B->oitem[s->idx];
        } else {
            const DoomoRoomActor *a = &v->actors[-1 - s->idx];
            fix16 ax = a->x, ay = a->y, w = INT2FIX(v->w), d = INT2FIX(v->d), t;
            int u, vv;
            uint32_t h;
            if (!a->img || !a->img->pix || !a->img->w || !a->img->h) continue;
            for (i = 0; i < g->k; i++) { t = ax; ax = d - ay; ay = t; t = w; w = d; d = t; }
            u = g->ox + FIX2INT(ax * 16) - FIX2INT(ay * 16);
            vv = g->oy + ((FIX2INT(ax * 16) + FIX2INT(ay * 16)) >> 1) - a->z;
            L[n].kind = RK_ACTOR;
            L[n].src = (uint16_t)(-1 - s->idx);
            L[n].px = (int16_t)(u - a->img->w / 2);
            L[n].py = (int16_t)(vv - a->img->h + 2);
            L[n].x0 = L[n].px; L[n].y0 = L[n].py;
            L[n].x1 = (int16_t)(L[n].px + a->img->w); L[n].y1 = (int16_t)(L[n].py + a->img->h);
            L[n].flip = (uint8_t)(a->flags & BB_FLIPX);
            L[n].iso = ISO_NONE;
            L[n].xf = 0;
            h = mix(2166136261u, RK_ACTOR | (uint32_t)L[n].flip << 8);
            h = mix(h, (uint32_t)(uintptr_t)a->img->pix ^ a->img->uid);
            h = mix(h, (uint32_t)(L[n].px & 0xFFFF) | (uint32_t)(L[n].py & 0xFFFF) << 16);
            L[n].sig = mix(h, (uint32_t)a->img->w | (uint32_t)a->img->h << 8);
            n++;
        }
    }
    /* (H5) an open kind's front-edge pieces, in object order */
    for (i = 0; i < nobj && n < RB_ITEMS; i++)
        if (B->okind[i] == OK_FRONT) L[n++] = B->oitem[i];
    /* 3. the cursor (under the held ghost, as OBJs would stack) */
    if (v->cursor_x >= 0 && v->cursor_y >= 0 && n < RB_ITEMS) {
        int x0 = v->cursor_x, y0 = v->cursor_y, x1 = x0 + 1, y1 = y0 + 1, bx, by;
        rot_box(g->k, v->w, v->d, &x0, &y0, &x1, &y1);
        bx = g->ox + 16 * (x0 - y0);
        by = g->oy + 8 * (x0 + y0);
        L[n].kind = RK_CURSOR;
        L[n].px = (int16_t)bx; L[n].py = (int16_t)by;
        L[n].x0 = (int16_t)(bx - 16); L[n].y0 = (int16_t)by;
        L[n].x1 = (int16_t)(bx + 17); L[n].y1 = (int16_t)(by + 18);
        L[n].flip = (uint8_t)((v->time >> 3) & 1);
        L[n].iso = ISO_NONE;
        L[n].xf = 0;
        L[n].sig = mix(mix(2166136261u, RK_CURSOR | (uint32_t)L[n].flip << 8),
                       (uint32_t)(bx & 0xFFFF) | (uint32_t)(by & 0xFFFF) << 16);
        n++;
    }
    /* 4. held ghosts, on top */
    for (i = 0; i < nobj && n < RB_ITEMS; i++)
        if (B->okind[i] == OK_GHOST) L[n++] = B->oitem[i];
    /* squashed pieces: affine OBJs (obj.c) only when nothing later covers
     * them and they lie inside the managed area; else unsquashed in order */
    for (i = 0; i < n; i++) {
        RItem *a = &L[i];
        if (!a->xf) continue;
        if (a->x0 < 0 || a->x1 > SCREEN_W || a->y0 < g->my0 || a->y1 > g->my1) { a->xf = 0; continue; }
        for (k = i + 1; k < n; k++)
            if (L[k].x0 < a->x1 && a->x0 < L[k].x1 && L[k].y0 < a->y1 && a->y0 < L[k].y1) { a->xf = 0; break; }
    }
    B->nitem = (uint16_t)n;
    return n;
}

/* ---- drawing one item into a clip rect ----------------------------------------- */
static void clip_set(int x0, int y0, int x1, int y1)
{
    owf.ox = x0; owf.oy = y0; owf.ow = x1 - x0; owf.oh = y1 - y0;
    owf.night = 0;
    owf.tint_level = 0;
}

/* decode span rows [r0, r1) of a facing into stage (row stride w) */
/* ---- sprite runs --------------------------------------------------------------
 * Every piece is drawn run by run, touching only its opaque pixels: a plain
 * piece (no paint, full light, not mirrored) straight from ROM; the others
 * through a small row buffer on the stack (the LUT applied, mirrored when
 * needed), placed at the destination's pixel parity so the copy is always
 * a halfword DMA. */
#define RUNBUF 128

HOT static void lut_copy(uint8_t *d, const uint8_t *s, int n, const uint8_t *lut)
{
    while (n >= 4) {
        d[0] = lut[s[0]]; d[1] = lut[s[1]]; d[2] = lut[s[2]]; d[3] = lut[s[3]];
        d += 4; s += 4; n -= 4;
    }
    while (n-- > 0) *d++ = lut[*s++];
}

/* mirrored: d[k] = s[-k] */
HOT static void lut_copy_rev(uint8_t *d, const uint8_t *s, int n, const uint8_t *lut)
{
    if (lut) {
        while (n >= 4) {
            d[0] = lut[s[0]]; d[1] = lut[s[-1]]; d[2] = lut[s[-2]]; d[3] = lut[s[-3]];
            d += 4; s -= 4; n -= 4;
        }
        while (n-- > 0) *d++ = lut[*s--];
    } else {
        while (n-- > 0) *d++ = *s--;
    }
}

static inline void dma16(uint16_t *dst, const void *src, int halves)
{
    REG_DMA3SAD = (uint32_t)(uintptr_t)src;
    REG_DMA3DAD = (uint32_t)(uintptr_t)dst;
    REG_DMA3CNT = (uint32_t)halves | DMA_16 | DMA_ENABLE;
}

/* rows [r0, r1) of a plain facing (no LUT, not mirrored), clipped to
 * [cx0, cx1): runs straight from ROM */
HOT static void runs_plain(const DoomoFurnIso *im, int px, int py, int r0, int r1, int cx0, int cx1)
{
    const uint8_t *base = doomo_furn_iso_data + im->off;
    const int xe = DOOMO_MIN(px + im->w, cx1);
    int r;
    for (r = r0; r < r1; r++) {
        const uint8_t *p = base + (base[r * 2] | (base[r * 2 + 1] << 8));
        uint16_t *row = gba_back + (py + r) * LINE;
        int x = px, skip;
        while ((skip = p[0]) != 0xFF) {
            int len = p[1], a, b;
            const uint8_t *src = p + 2;
            x += skip;
            p += 2 + len;
            a = x; b = x + len;
            x = b;
            if (a < cx0) { src += cx0 - a; a = cx0; }
            if (b > xe) b = xe;
            if (a >= b) continue;
            if (a & 1) { put_px(a, py + r, *src++); a++; }
            if (b - a >= 2) {
                int n = (b - a) & ~1;
                if (!((uintptr_t)src & 1)) dma16(row + (a >> 1), src, n >> 1);
                else ow_out(row + (a >> 1), src, n);
                src += n; a += n;
            }
            if (a < b) put_px(a, py + r, *src);
        }
    }
}

/* rows [r0, r1) through the LUT and / or mirrored, clipped: each run goes
 * through a stack buffer laid out at the destination's pixel parity, so
 * the copy to VRAM is always a halfword DMA */
HOT static void runs_lut(const DoomoFurnIso *im, int px, int py, int flip, const uint8_t *lut,
                     int r0, int r1, int cx0, int cx1)
{
    const uint8_t *base = doomo_furn_iso_data + im->off;
    const int w = im->w;
    uint32_t bufw[(RUNBUF + 4) / 4];
    uint8_t *buf = (uint8_t *)bufw;
    int r;
    for (r = r0; r < r1; r++) {
        const uint8_t *p = base + (base[r * 2] | (base[r * 2 + 1] << 8));
        uint16_t *row = gba_back + (py + r) * LINE;
        int x = 0, skip;
        while ((skip = p[0]) != 0xFF) {
            int len = p[1], a, b, n;
            const uint8_t *run = p + 2;
            x += skip;
            p += 2 + len;
            n = DOOMO_MIN(len, w - x);
            if (!flip) { a = px + x; b = a + n; }
            else { b = px + w - x; a = b - n; }
            x += len;
            if (a < cx0) a = cx0;
            if (b > cx1) b = cx1;
            while (a < b) {
                int m = DOOMO_MIN(b - a, RUNBUF), k;
                uint8_t *d = buf + (a & 1);
                if (!flip) lut_copy(d, run + (a - (px + x - len)), m, lut);
                else lut_copy_rev(d, run + ((px + w - 1 - a) - (x - len)), m, lut);
                k = a;
                if (k & 1) { put_px(k, py + r, *d++); k++; }
                if (a + m - k >= 2) { dma16(row + (k >> 1), d, (a + m - k) >> 1); d += (a + m - k) & ~1; k += (a + m - k) & ~1; }
                if (k < a + m) put_px(k, py + r, *d);
                a += m;
            }
        }
    }
}

/* decode a facing into a w x h image (index 0 transparent), through the LUT */
static void runs_image(const DoomoFurnIso *im, const uint8_t *lut, uint8_t *dst)
{
    const uint8_t *base = doomo_furn_iso_data + im->off;
    const int w = im->w;
    int r;
    for (r = 0; r < im->h; r++) {
        const uint8_t *p = base + (base[r * 2] | (base[r * 2 + 1] << 8));
        int x = 0, skip;
        while ((skip = p[0]) != 0xFF) {
            int len = p[1], n;
            x += skip;
            n = DOOMO_MIN(len, w - x);
            if (n > 0) lut_copy(dst + r * w + x, p + 2, n, lut ? lut : doomo_colormap[15]);
            p += 2 + len;
            x += len;
        }
    }
}

static void draw_box_item(const RG *g, const RItem *it, int cx0, int cy0, int cx1, int cy1)
{
    const DoomoRoomObj *o = &g->v->objs[it->src];
    int pa2 = o->paint_a < 32 ? o->paint_a : PAINT_CARAMEL;
    uint8_t base = PAL_PAINT(pa2, 1);
    int x0 = it->bx0 * 16 + 1, y0 = it->by0 * 16 + 1, x1 = it->bx1 * 16 - 1, y1 = it->by1 * 16 - 1;
    int z = it->px, h = it->py, uB = x0 - y0, uR = x1 - y0, uF = x1 - y1, uL = x0 - y1, u;
    uint8_t top = lit(g, palette_lighter(base)), left = lit(g, base), right = lit(g, palette_darker(base));
    uint8_t edge = lit(g, palette_darker(palette_darker(base)));
#define CLIPFILL(ax, ay, bx2, by2, c) fill(DOOMO_MAX(ax, cx0), DOOMO_MAX(ay, cy0), DOOMO_MIN(bx2, cx1), DOOMO_MIN(by2, cy1), c)
    for (u = uL; u < uR; u += 2) {
        int vtop = u < uB ? ((2 * x0 - u) >> 1) : ((u + 2 * y0) >> 1);
        int vfront = u < uF ? ((u + 2 * y1) >> 1) : ((2 * x1 - u) >> 1);
        int x = g->ox + u, ytop = g->oy + vtop - z - h, yf = g->oy + vfront - z - h;
        if (yf > ytop) CLIPFILL(x, ytop, x + 2, yf, top);
        if (h > 0) CLIPFILL(x, yf, x + 2, yf + h, u < uF ? left : right);
        CLIPFILL(x, yf + h, x + 2, yf + h + 1, edge);
    }
#undef CLIPFILL
}

static void draw_cursor_item(const RItem *it, int cx0, int cy0, int cx1, int cy1)
{
    /* room_ref's diamond through the GBA plat_line's stepping, clipped */
    static const int8_t seg4[6][4] = {
        { 0, 0, 16, 8 }, { 16, 8, 0, 16 }, { 0, 16, -16, 8 }, { -16, 8, 0, 0 },
        { 0, 17, -15, 9 }, { 1, 17, 16, 9 } };
    int i, bx = it->px, by = it->py;
    uint8_t c = it->flip ? PAL_WHITE : PAL_UI_GOLD;
    for (i = 0; i < 6; i++) {
        int x0 = bx + seg4[i][0], y0 = by + seg4[i][1], x1 = bx + seg4[i][2], y1 = by + seg4[i][3];
        int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
        int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, n = 256;
        uint8_t col = i < 4 ? c : PAL_UI_PINK_D;
        while (n-- > 0) {
            int e2;
            if (x0 >= cx0 && x0 < cx1 && y0 >= cy0 && y0 < cy1) put_px(x0, y0, col);
            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

HOT static void draw_item(const RG *g, const RItem *it, int cx0, int cy0, int cx1, int cy1)
{
    if (it->kind == RK_CURSOR) { draw_cursor_item(it, cx0, cy0, cx1, cy1); return; }
    clip_set(cx0, cy0, cx1, cy1);
    if (it->kind == RK_ACTOR) {
        const DoomoRoomActor *a = &g->v->actors[it->src];
        ow_sprite(a->img, it->px, it->py, 1, it->flip ? BB_FLIPX : 0, doomo_colormap[15]);
        return;
    }
    if (it->iso == ISO_NONE) { draw_box_item(g, it, cx0, cy0, cx1, cy1); return; }
    {
        const DoomoFurnIso *im = iso_of(it);
        int r0 = DOOMO_MAX(0, cy0 - it->py), r1 = DOOMO_MIN(im->h, cy1 - it->py);
        if (r0 >= r1 || it->px >= cx1 || it->px + im->w <= cx0) return;
        if (!it->flip && it->lut == LUT_PLAIN) runs_plain(im, it->px, it->py, r0, r1, cx0, cx1);
        else runs_lut(im, it->px, it->py, it->flip, it->lut == LUT_PLAIN ? doomo_colormap[15] : lut_get(it->lut),
                      r0, r1, cx0, cx1);
    }
}

/* the squashed piece through S1's affine OBJ path, after the rect pass */
static void draw_xform(const RItem *it, const DoomoRoomObj *o)
{
    const DoomoFurnIso *im = iso_of(it);
    DoomoImage img;
    if ((uint32_t)im->w * im->h > STAGE_BYTES) return;
    memset(B->stage, 0, (size_t)(im->w * im->h));
    runs_image(im, it->lut == LUT_PLAIN ? NULL : lut_get(it->lut), B->stage);
    img.w = im->w; img.h = im->h; img.pix = B->stage;
    img.uid = 0xF0A00000u ^ ((uint32_t)o->furn << 6) ^ (uint32_t)(it->iso << 1) ^
              ((it->lut & 31) << 20) ^ (((it->lut >> 8) & 31) << 25) ^ ((it->lut >> 16) << 15);
    plat_image_xform(it->px + im->w / 2, it->py + im->h, &img, 256 + o->sq_x * 2, 256 + o->sq_y * 2,
                     it->flip ? IMG_FLIPX : 0, 0);
}

HOT static void draw_region(const RG *g, int x0, int y0, int x1, int y1)
{
    int i;
    if (x0 >= x1 || y0 >= y1) return;
    draw_shell(g, x0, y0, x1, y1);
    for (i = 0; i < B->nitem; i++) {
        const RItem *it = &B->item[i];
        if (it->xf || it->x0 >= x1 || it->x1 <= x0 || it->y0 >= y1 || it->y1 <= y0) continue;
        draw_item(g, it, x0, y0, x1, y1);
    }
}

/* ---- block checksums ----------------------------------------------------------- */
/* A 16 px x rows block of the back page: four words a row (ldmia), mixed
 * by add / xor / rotate - every word changes the result. */
#define BLOCK_ROW(s, p) do { uint32_t a_, b_, c_, d_; \
        __asm__ volatile("ldmia %4!, {%0, %1, %2, %3}" \
                         : "=l"(a_), "=l"(b_), "=l"(c_), "=l"(d_), "+l"(p) : : "memory"); \
        s = (((s + a_) ^ b_) + c_) ^ d_; s = (s << 5) | (s >> 27); p += LINE / 2 - 4; } while (0)
HOT static uint32_t block_sum(int bx, int y, int rows)
{
    const uint32_t *p = (const uint32_t *)(gba_back + y * LINE + bx * 8);
    uint32_t s = 0x9E3779B9u;
    if (rows == 8) {
        BLOCK_ROW(s, p); BLOCK_ROW(s, p); BLOCK_ROW(s, p); BLOCK_ROW(s, p);
        BLOCK_ROW(s, p); BLOCK_ROW(s, p); BLOCK_ROW(s, p); BLOCK_ROW(s, p);
        return s;
    }
    while (rows-- > 0) BLOCK_ROW(s, p);
    return s;
}

static int block_rows(const RG *g, int by)
{
    return DOOMO_MIN(8, g->my1 - (g->my0 + by * 8));
}

static void sums_store(const RG *g, RPage *pg, int all)
{
    int bx, by;
    for (by = 0; by < g->nby; by++)
        for (bx = g->bx0; bx < g->bx1; bx++) {
            int b = by * RB_BX + bx;
            if (all || B->dirty[b]) pg->sum[b] = block_sum(bx, g->my0 + by * 8, block_rows(g, by));
        }
}

static void mark(const RG *g, int x0, int y0, int x1, int y1)
{
    int bx, by, bx1, by1;
    if (x0 < g->bx0 * 16) x0 = g->bx0 * 16;
    if (x1 > g->bx1 * 16) x1 = g->bx1 * 16;
    if (y0 < g->my0) y0 = g->my0;
    if (y1 > g->my1) y1 = g->my1;
    if (x0 >= x1 || y0 >= y1) return;
    by1 = (y1 - 1 - g->my0) >> 3;
    bx1 = (x1 - 1) >> 4;
    for (by = (y0 - g->my0) >> 3; by <= by1; by++)
        for (bx = x0 >> 4; bx <= bx1; bx++) B->dirty[by * RB_BX + bx] = 1;
}

static void save_records(const RG *g, RPage *pg)
{
    int i;
    for (i = 0; i < B->nitem; i++) {
        const RItem *it = &B->item[i];
        pg->rec[i].x0 = it->x0; pg->rec[i].y0 = it->y0;
        pg->rec[i].x1 = it->x1; pg->rec[i].y1 = it->y1;
        pg->rec[i].sig = it->xf ? ~it->sig : it->sig;   /* an OBJ is never "unchanged" */
    }
    pg->n = B->nitem;
    pg->key = B->shell_key;
    pg->valid = 1;
    (void)g;
}

static void xforms(const RG *g)
{
    int i;
    for (i = 0; i < B->nitem; i++)
        if (B->item[i].xf) draw_xform(&B->item[i], &g->v->objs[B->item[i].src]);
}

__attribute__((noinline)) static void full(const RG *g, RPage *pg)
{
    draw_region(g, 0, g->dy0, SCREEN_W, g->dy1);
    xforms(g);
    sums_store(g, pg, 1);
    save_records(g, pg);
    room_stats.full = 1;
    room_stats.rects = 1;
    room_stats.blocks = (uint16_t)(g->nby * (g->bx1 - g->bx0));
}

/* A dirty rect (screen px), clipped to the managed area and widened to
 * even x; merged into a neighbour when that does not waste much area.
 * Too many rects: rect_over (the frame recomposes). */
static int rect_area(const RRect *r) { return (r->x1 - r->x0) * (r->y1 - r->y0); }

static int rect_merge(RRect *a, const RRect *b)
{
    RRect u;
    if (a->x0 > b->x1 + 2 || b->x0 > a->x1 + 2 || a->y0 > b->y1 + 2 || b->y0 > a->y1 + 2) return 0;
    u.x0 = DOOMO_MIN(a->x0, b->x0); u.y0 = DOOMO_MIN(a->y0, b->y0);
    u.x1 = DOOMO_MAX(a->x1, b->x1); u.y1 = DOOMO_MAX(a->y1, b->y1);
    if (rect_area(&u) > (rect_area(a) + rect_area(b)) * 9 / 8 + 64) return 0;
    *a = u;
    return 1;
}

static void add_rect(const RG *g, int x0, int y0, int x1, int y1)
{
    RRect r;
    int i, j;
    x0 = DOOMO_MAX(x0, g->bx0 * 16) & ~1;
    x1 = (DOOMO_MIN(x1, g->bx1 * 16) + 1) & ~1;
    y0 = DOOMO_MAX(y0, g->my0);
    y1 = DOOMO_MIN(y1, g->my1);
    if (x0 >= x1 || y0 >= y1) return;
    r.x0 = (int16_t)x0; r.y0 = (int16_t)y0; r.x1 = (int16_t)x1; r.y1 = (int16_t)y1;
    for (i = 0; i < B->nrect; i++)
        if (rect_merge(&B->rect[i], &r)) {
            /* the grown rect may now swallow others */
            for (j = 0; j < B->nrect; j++)
                if (j != i && rect_merge(&B->rect[i], &B->rect[j])) {
                    B->rect[j] = B->rect[--B->nrect];
                    if (i == B->nrect) i = j;
                    j = -1;
                }
            return;
        }
    if (B->nrect < RB_RECTS) B->rect[B->nrect++] = r;
    else B->rect_over = 1;
}

__attribute__((noinline)) static void incremental(const RG *g, RPage *pg)
{
    int bx, by, i, j, nb = g->nby * (g->bx1 - g->bx0), mono = 1, last = -1, area = 0;
    B->nrect = 0;
    B->rect_over = 0;
    memset(B->dirty, 0, sizeof(B->dirty));
    /* 1. whatever was drawn over the room since this page's last frame:
     *    each changed block row run becomes a rect */
    for (by = 0; by < g->nby; by++) {
        int y = g->my0 + by * 8, rows = block_rows(g, by), run = -1;
        for (bx = g->bx0; bx <= g->bx1; bx++) {
            int changed = bx < g->bx1 && block_sum(bx, y, rows) != pg->sum[by * RB_BX + bx] &&
                          !(op_n && block_opaque(bx, y, rows));
            if (changed && run < 0) run = bx;
            if (!changed && run >= 0) { add_rect(g, run * 16, y, bx * 16, y + rows); run = -1; }
        }
    }
    /* 2. the draw list against the page's: changed items dirty their old
     *    and new boxes */
    memset(B->match, 0, sizeof(B->match));
    {
        uint8_t used[RB_ITEMS];
        memset(used, 0, sizeof(used));
        for (i = 0; i < B->nitem; i++) {
            const RItem *it = &B->item[i];
            int m = -1;
            if (!it->xf) {
                if (i < pg->n && !used[i] && pg->rec[i].sig == it->sig) m = i;
                else
                    for (j = 0; j < pg->n; j++)
                        if (!used[j] && pg->rec[j].sig == it->sig) { m = j; break; }
            }
            if (m >= 0 && pg->rec[m].x0 == it->x0 && pg->rec[m].y0 == it->y0 &&
                pg->rec[m].x1 == it->x1 && pg->rec[m].y1 == it->y1) {
                used[m] = 1;
                B->match[i] = (uint8_t)(m + 1);
                if (m < last) mono = 0;
                last = m;
            } else {
                add_rect(g, it->x0, it->y0, it->x1, it->y1);
            }
        }
        for (j = 0; j < pg->n; j++)
            if (!used[j]) add_rect(g, pg->rec[j].x0, pg->rec[j].y0, pg->rec[j].x1, pg->rec[j].y1);
    }
    /* unchanged items whose order flipped where they overlap */
    if (!mono)
        for (i = 0; i < B->nitem; i++)
            for (j = i + 1; j < B->nitem; j++) {
                const RItem *a = &B->item[i], *b = &B->item[j];
                if (!B->match[i] || !B->match[j] || B->match[i] < B->match[j]) continue;
                if (a->x0 < b->x1 && b->x0 < a->x1 && a->y0 < b->y1 && b->y0 < a->y1) {
                    add_rect(g, a->x0, a->y0, a->x1, a->y1);
                    add_rect(g, b->x0, b->y0, b->x1, b->y1);
                }
            }
    for (i = 0; i < B->nrect; i++) area += rect_area(&B->rect[i]);
    room_stats.blocks = (uint16_t)(area >> 7);
    if (B->rect_over || area * 100 > nb * 128 * DIRTY_FULL) { full(g, pg); return; }
    /* 3. redraw the rects, then re-checksum every block they touched */
    for (i = 0; i < B->nrect; i++) {
        const RRect *r = &B->rect[i];
        draw_region(g, r->x0, r->y0, r->x1, r->y1);
        mark(g, r->x0, r->y0, r->x1, r->y1);
    }
    xforms(g);
    sums_store(g, pg, 0);
    save_records(g, pg);
    room_stats.full = 0;
    room_stats.rects = B->nrect;
}

/* ---- harness test layouts ---------------------------------------------------- */
static DoomoRoomView *test_view(const DoomoRoomView *v, DoomoRoomView *tv, DoomoRoomObj *objs,
                                DoomoRoomActor *acts);

/* ---- plat_room ------------------------------------------------------------------- */
static void render(const DoomoRoomView *v)
{
    RG g;
    RPage *pg;
    uint32_t c0 = gba_cycles(), key;
    if (!gba_big_fits(GBA_BIG_ROOM)) {   /* never with the house's 2.8 KB claim */
        gba_log("GBA: plat_room: the scene scratch claim covers the house's region");
        return;
    }
    claim();
    rb_last_frame = gba_frames;
    band_cur[0] = band_pend[0];
    band_cur[1] = band_pend[1];
    band_pend[0] = 0;
    band_pend[1] = SCREEN_H;
    geometry(&g, v);
    /* the brake wobble and the zoom punch are BG2 (postfx.c latches them) */
    if (v->pan_x >= -4 && v->pan_x <= 4 && v->pan_y >= -4 && v->pan_y <= 4 && (v->pan_x || v->pan_y))
        plat_post_offset(v->pan_x, v->pan_y);
    if (v->punch_q8 && v->punch_q8 != 256 && v->punch_q8 > 128 && v->punch_q8 < 512)
        plat_post_scale(v->punch_q8, v->punch_q8, g.vx + g.vw / 2, g.vy + g.vh / 2);
    key = shell_key(&g);
    if (key != B->shell_key) {
        B->shell_key = key;
        build_shell(&g);
    }
    build_items(&g, v);
    pg = &B->page[gba_back == (uint16_t *)VRAM_PAGE1];
    {
        OwFrame save = owf;
        RPage *fr = &B->page[gba_back != (uint16_t *)VRAM_PAGE1];
        if ((!pg->valid || pg->key != key) && fr->valid && fr->key == key && g.dy1 > g.dy0) {
            /* copy-forward: the shown page already holds this room (one full
             * recompose ago): copy it (VRAM to VRAM) with its records; the
             * incremental pass then restores the UI drawn over it and
             * applies one frame of changes */
            const uint16_t *src = (gba_back == (uint16_t *)VRAM_PAGE1) ? (const uint16_t *)VRAM_PAGE0
                                                                        : (const uint16_t *)VRAM_PAGE1;
            REG_DMA3SAD = (uint32_t)(uintptr_t)(src + g.dy0 * LINE);
            REG_DMA3DAD = (uint32_t)(uintptr_t)(gba_back + g.dy0 * LINE);
            REG_DMA3CNT = (uint32_t)((g.dy1 - g.dy0) * LINE / 2) | DMA_32 | DMA_ENABLE;
            memcpy(pg, fr, sizeof(*pg));
        }
        if (!pg->valid || pg->key != key) full(&g, pg);
        else incremental(&g, pg);
        owf = save;
    }
    room_stats.items = B->nitem;
    room_stats.cycles = gba_cycles() - c0;
}

void plat_room(const DoomoRoomView *v)
{
    if (!v) return;
    if (gba_nodraw == GBA_NODRAW_CANVAS) cv_need_m4();   /* G5: the house draws into a page */
    if (gba_nodraw == GBA_NODRAW_FIRST) gba_nodraw = 0;
    if (gba_nodraw == 1) {                  /* a catch-up tick: nothing drawn */
        rb_last_frame = gba_frames;
        return;
    }
    if (rb_test) {
        DoomoRoomView tv;
        render(test_view(v, &tv, TEST->objs, TEST->acts));
        return;
    }
    render(v);
}

/* ============================ harness ============================================ */
/* The parity frames of core/tests/test_home_render.c, built from the
 * shared table (core/tests/room_shells.h): golden line 0..4 is the classic
 * layout (the Living Room template plus 12 pieces, recoloured, lit,
 * stacked, on both walls), lines ROOM_SHELL_FIRST.. the shell frames. */
static void parity_view(DoomoRoomView *v, DoomoRoomObj *objs, HomeRoom *r, int line)
{
    if (line < ROOM_SHELL_FIRST) rs_parity_room(r);
    else rs_shell_room(line - ROOM_SHELL_FIRST, r);
    rs_view(v, objs, r, line);
    v->vy = 12;
    v->vw = SCREEN_W;
    v->vh = SCREEN_H - 24;
}

static uint32_t page_fnv(void)
{
    const uint16_t *p = gba_back;
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < SCREEN_W * SCREEN_H / 2; i++) {
        uint16_t w = p[i];
        h = (h ^ (w & 0xFF)) * 16777619u;
        h = (h ^ (w >> 8)) * 16777619u;
    }
    return h;
}

/* one harness frame on the current back page (no flip), as a logic tick */
static void hc_frame(const DoomoRoomView *v)
{
    gba_frames++;
    render(v);
}

uint32_t room_hc_parity(uint32_t arg)
{
    HomeRoom r;
    DoomoRoomObj *objs = TEST->objs;
    DoomoRoomView v;
    int k = (int)(arg & 3), path = (int)((arg >> 8) & 15), line = (int)((arg >> 12) & 0xFF);
    uint8_t keep_test = rb_test;
    int keep_nodraw = gba_nodraw;
    uint32_t h;
    rb_test = 0;
    gba_nodraw = 0;                       /* the last tick may have been a catch-up */
    if (line < ROOM_SHELL_FIRST || line >= ROOM_SHELL_LINES) {
        line = (arg & 0x10) ? 4 : k;              /* the classic layout: any view, day or night */
        parity_view(&v, objs, &r, line);
        v.yaw = (int16_t)(k * 1024);
        v.daylight = (uint8_t)((arg & 0x10) ? 3 : 15);
    } else {
        parity_view(&v, objs, &r, line);
    }
    if (path == 1 && r.nobj < 12) path = 0;   /* the busy frame moves the parity pieces */
    room_invalidate();
    band_pend[0] = 0;
    band_pend[1] = SCREEN_H;
    plat_clear(PAL_BLACK);
    if (path == 1) {
        /* a busier frame first: the cursor, a held ghost with a lift squash,
         * an animated frame, a jittering piece, a squashed piece, two actors */
        DoomoRoomView v2 = v;
        DoomoRoomActor *acts = TEST->acts;
        DoomoRoomObj *o2 = TEST->objs2;
        const DoomoImage *img = bodyspr_image(DOOMO_PLAYER_ID, 0, 1);
        memcpy(o2, objs, sizeof(TEST->objs));
        o2[r.nobj] = objs[r.nobj - 1];
        o2[r.nobj].x = 3; o2[r.nobj].y = 2; o2[r.nobj].z = 14;
        o2[r.nobj].flags = RO_GHOST; o2[r.nobj].sq_x = 13; o2[r.nobj].sq_y = -13;
        o2[r.nobj - 6].jitter = 4;                         /* the bookcase shakes */
        o2[r.nobj - 9].sq_x = -20; o2[r.nobj - 9].sq_y = 23; /* the lamp squashes */
        o2[r.nobj - 2].x = 1; o2[r.nobj - 2].y = 1;        /* the plush moved     */
        v2.objs = o2;
        v2.nobj = r.nobj + 1;
        v2.cursor_x = 2; v2.cursor_y = 4;
        v2.time = 9;
        memset(acts, 0, 2 * sizeof(*acts));
        acts[0].img = img; acts[0].x = INT2FIX(2) + FIX_ONE / 2; acts[0].y = INT2FIX(5) + FIX_ONE / 3;
        acts[1].img = img; acts[1].x = INT2FIX(4); acts[1].y = INT2FIX(3); acts[1].flags = BB_FLIPX;
        v2.actors = acts;
        v2.nact = 2;
        hc_frame(&v2);
        v2.cursor_x = 4; v2.cursor_y = 3; v2.time = 1;
        acts[0].x += FIX_ONE / 2;
        o2[r.nobj - 9].sq_x = 10;
        hc_frame(&v2);
        hc_frame(&v);
    } else if (path == 2) {
        /* UI drawn over the room after plat_room: a window, a caption */
        hc_frame(&v);
        plat_rect(40, 50, 120, 60, PAL_UI_PINK);
        plat_rect(41, 51, 118, 58, PAL_WHITE);
        plat_text(48, 60, "Catalog", PAL_BLACK);
        plat_rect(180, 130, 40, 12, PAL_UI_GOLD);
        plat_line(3, 20, 236, 140, PAL_UI_RED);
        hc_frame(&v);
    } else {
        hc_frame(&v);
    }
    h = page_fnv();
    room_invalidate();
    rb_test = keep_test;
    gba_nodraw = keep_nodraw;
    gba_logf("room: parity %x line %d path %d: fnv %x (%u cycles, %u rects)", (unsigned)arg, line, path, h,
             room_stats.cycles, room_stats.rects);
    return h;
}

void room_hc_test(uint32_t mode)
{
    rb_test = (uint8_t)(mode < ROOM_TEST_COUNT ? mode : 0);
    room_invalidate();
}

/* The test layouts replace the scene's furniture (the GBA keeps no HOME
 * state before G0) while the scene's own cursor, ghost, player and HUD run:
 *   STEADY  the parity room + 3 walking citizens + 3 animated pieces;
 *   CELLAR  8x8, 40 pieces, dim (light 6 of the Cellar);
 *   DEFEND  8x8, 24 pieces + 10 walkers (Hold the Door's wave 3 crowd). */
static DoomoRoomView *test_view(const DoomoRoomView *v, DoomoRoomView *tv, DoomoRoomObj *objs,
                                DoomoRoomActor *acts)
{
    HomeRoom *const rr = &TEST->room;
    static const uint16_t cellar_set[8] = {
        FURN_CUPCAKE_LAMP, FURN_POTTED_FERN, FURN_TEACUP_TABLE, FURN_BUNNY_BEANBAG,
        FURN_HORNED_POOP_PLUSH, FURN_JELLYBEAN_JAR, FURN_LAVA_LAMP, FURN_TUBE_TV };
    uint32_t t = v->time;
    int i, n = 0, na = 0, walkers = 0;
    *tv = *v;
    if (rb_test == ROOM_TEST_STEADY) {
        rs_parity_room(rr);
        for (i = 0; i < rr->nobj; i++) home_obj_view(&rr->obj[i], &objs[n++]);
        /* 3 animated pieces: the TV, a lava lamp, the nightlight */
        {
            static const uint8_t ax[3] = { 0, 1, 5 }, ay[3] = { 1, 2, 0 };
            static const uint16_t af[3] = { FURN_TUBE_TV, FURN_LAVA_LAMP, FURN_LULLA_NIGHTLIGHT };
            for (i = 0; i < 3; i++) {
                memset(&objs[n], 0, sizeof(objs[n]));
                objs[n].furn = af[i]; objs[n].x = ax[i]; objs[n].y = ay[i];
                objs[n].rot_anim = RO_ROT_ANIM(i == 0 ? 1 : 0, (t >> 3) & 1);
                objs[n].paint_a = objs[n].paint_b = 0xFF;
                objs[n].flags = RO_FULLBRIGHT;
                n++;
            }
        }
        walkers = 3;
    } else {
        tv->w = tv->d = 8;
        home_room_template(HOME_ROOM_CELLAR, rr);
        for (i = 0; i < rr->nobj && n < HOME_OBJ_MAX; i++) home_obj_view(&rr->obj[i], &objs[n++]);
        for (i = 0; n < (rb_test == ROOM_TEST_CELLAR ? HOME_OBJ_MAX : 24); i++) {
            int x = 1 + (i % 7), y = 1 + (i / 7) % 7;
            memset(&objs[n], 0, sizeof(objs[n]));
            objs[n].furn = cellar_set[i & 7];
            objs[n].x = (uint8_t)x; objs[n].y = (uint8_t)y;
            /* the looping pieces animate; the beanbag moves only while
             * someone sits on it (home_draw.c anim_frame) */
            objs[n].rot_anim = RO_ROT_ANIM(i & 3, cellar_set[i & 7] == FURN_BUNNY_BEANBAG ? 0 : (t >> 3) & 1);
            objs[n].paint_a = (uint8_t)(i % 5 == 0 ? PAINT_MINT : 0xFF);
            objs[n].paint_b = 0xFF;
            n++;
        }
        tv->daylight = 6;
        walkers = rb_test == ROOM_TEST_DEFEND ? 10 : 0;
    }
    /* the scene's ghost stays */
    for (i = 0; i < v->nobj && n < HOME_OBJ_MAX + 1; i++)
        if (v->objs[i].flags & RO_GHOST) objs[n++] = v->objs[i];
    for (i = 0; i < v->nact && na < 2; i++) acts[na++] = v->actors[i];
    for (i = 0; i < walkers && na < 12; i++) {
        uint32_t ph = (t * (uint32_t)(3 + i)) % 160u;
        fix16 p = (fix16)(ph < 80 ? ph : 160 - ph) * (FIX_ONE / 16);
        memset(&acts[na], 0, sizeof(acts[na]));
        acts[na].img = bodyspr_image(1 + i % 3, ph < 80 ? 0 : 3, (int)(t >> 2) & 3);
        acts[na].x = (i & 1) ? INT2FIX(1) / 2 + p : INT2FIX(i % 5) + FIX_ONE / 2;
        acts[na].y = (i & 1) ? INT2FIX(i % 5) + FIX_ONE / 2 : INT2FIX(1) / 2 + p;
        acts[na].flags = (uint8_t)(ph < 80 ? 0 : BB_FLIPX);
        na++;
    }
    tv->objs = objs;
    tv->nobj = n;
    tv->actors = acts;
    tv->nact = na;
    return tv;
}
