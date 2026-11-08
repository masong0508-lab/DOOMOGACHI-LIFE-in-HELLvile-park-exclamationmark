/* panel.c - retained panels (G7; platform.h plat_panel_begin, owner: gba;
 * docs/GBA.md section 4b).
 *
 * Mode 4 draws every frame into the back page while the front page shows
 * the last one. A panel the core draws with the same id, rect and key as
 * on the last frame is therefore still in the front page, pixel for
 * pixel: plat_panel_end() of a kept frame copies its body from the front
 * page into the back page (pn_span, ARM in IWRAM: a DMA per row), after
 * the core has drawn the panel's drop shadows over the new picture. Its
 * body is what the first frame it is drawn on filled with plat_rect and
 * straight plat_line: those rects are kept as a list (one that lies inside
 * a rect already in it is skipped). Text, images and checkers lie on that
 * body (the platform.h rule; core/tests/test_panel.c checks the adopted
 * panels pixel by pixel on the native platform, which keeps panels the
 * same way), and shading is not body (the core shades again on a kept
 * frame). Not kept: a panel with plat_clear, a squash OBJ, a figure OBJ,
 * a rect outside its own rect, or more rects than the pool holds.
 *
 * A kept panel is valid only on the frame right after the one that showed
 * it, from the page that frame was shown from, and only if nothing drawn
 * after it on that frame touched its rect outside its hole: while a panel
 * is drawn, put back or shown, gba_nodraw is GBA_NODRAW_PANEL, so later 2D
 * calls that can touch pn_box (the shown panels' bounding box) take their
 * slow path (vid_route, canvas.c) and are checked here; once nothing is
 * left to watch the fast path is back. A panel missed twice in a row (it
 * changes or is drawn over on every frame) is drawn uncaptured for 2, 4,
 * then 8 frames between tries. Tile frames (section 6b), skipped ticks and
 * the harness's expect-text never keep: they draw (or skip) as before.
 *
 * RAM: the panel table (9 x 20 B) and the 80-rect pool (320 B) in EWRAM;
 * pn_span 116 B of resident IWRAM code.
 */
#include "gba_plat.h"
#include "canvas_int.h"
#include "hcmd.h"

#define PN_POOL 80
DOOMO_STATIC_ASSERT(PANEL_COUNT <= 16, panels_fit_the_shown_mask);
DOOMO_STATIC_ASSERT(PN_POOL <= 255, pool_index_fits);

typedef struct { uint8_t x0, y0, x1, y1; } PnRect;      /* [x0, x1) x [y0, y1) */
enum { PN_LIVE = 1, PN_KEEP = 2, PN_SPOILED = 4, PN_PAGE1 = 8, PN_SOLID = 16 };
typedef struct {
    uint32_t key;
    int16_t  x, y;                 /* the rect as passed            */
    uint8_t  w, h;
    uint8_t  flags;                /* PN_*; PN_PAGE1: it is in page 1 */
    uint8_t  miss_wait;            /* captures in a row not kept (low 4 bits),
                                      frames to draw it uncaptured (high 4) */
    uint8_t  r0, nr;               /* its rects in the pool         */
    uint8_t  hx0, hy0, hx1, hy1;   /* the hole, clipped              */
    uint16_t serial;               /* pn_flips after the frame that showed it */
} Pn;                              /* 20 B */

static Pn       pn[PANEL_COUNT];
static PnRect   pool[PN_POOL];
static uint8_t  pool_n;
static int8_t   cap = -1;          /* panel being drawn             */
static int8_t   put = -1;          /* kept panel, its shading drawn */
static uint8_t  nest;              /* begin calls inside a panel     */
static uint8_t  suspended;         /* I10: drawing into the shown page: no panel is kept, captured or put back */
static uint16_t shown;             /* bit i: panel i shown this frame */
static int8_t   shown_page = -1;   /* the page the last flip showed, -1 tiles */
static uint16_t flips;             /* Mode 4 flips                   */
uint8_t pn_op;                     /* PN_OP_* of the 2D call routing now */
/* the box a 2D call must touch to concern a panel: the shown panels'
 * bounding box, the whole screen while one is drawn or put back
 * (video_loops.s plat_rect tests it before taking the slow path) */
int32_t pn_box[4];

static int clipx(int v) { return v < 0 ? 0 : v > SCREEN_W ? SCREEN_W : v; }
static int clipy(int v) { return v < 0 ? 0 : v > SCREEN_H ? SCREEN_H : v; }

/* ---- the rect list ---------------------------------------------------- */
/* drop panel i's rects from the pool (the panels after it move down) */
static void pool_free(int id)
{
    Pn *p = &pn[id];
    int i, r0 = p->r0, n = p->nr;
    if (!n) return;
    memmove(pool + r0, pool + r0 + n, (size_t)(pool_n - r0 - n) * sizeof(PnRect));
    pool_n = (uint8_t)(pool_n - n);
    for (i = 0; i < PANEL_COUNT; i++)
        if (pn[i].nr && pn[i].r0 > r0) pn[i].r0 = (uint8_t)(pn[i].r0 - n);
    p->nr = 0;
    p->r0 = 0;
}

static void nokeep(void)
{
    if (cap >= 0) pn[cap].flags &= (uint8_t)~PN_KEEP;
}

/* a body rect of the capture */
static void add_rect(Pn *p, int x0, int y0, int x1, int y1)
{
    PnRect *r;
    int i;
    if (!(p->flags & PN_KEEP)) return;
    /* inside one rect already recorded (a fill inside its outline, a star
     * on a pill): nothing new */
    for (i = 0, r = pool + p->r0; i < p->nr; i++, r++)
        if (r->x0 <= x0 && r->x1 >= x1 && r->y0 <= y0 && r->y1 >= y1) return;
    if (pool_n >= PN_POOL) { nokeep(); return; }
    r = &pool[pool_n++];
    r->x0 = (uint8_t)x0; r->y0 = (uint8_t)y0; r->x1 = (uint8_t)x1; r->y1 = (uint8_t)y1;
    p->nr++;
}

/* a panel is drawn or put back: every 2D call is watched (slow path) */
static void box_screen(void)
{
    if (gba_nodraw == 0) gba_nodraw = GBA_NODRAW_PANEL;
    pn_box[0] = 0; pn_box[1] = 0; pn_box[2] = SCREEN_W; pn_box[3] = SCREEN_H;
}

/* the shown panels' bounding box (none: an empty box) */
static void box_shown(void)
{
    int i, x0 = SCREEN_W, y0 = SCREEN_H, x1 = 0, y1 = 0;
    unsigned m;
    /* (C3) only the shown panels' bits are walked: the loop stops after the highest */
    for (i = 0, m = shown; m; i++, m >>= 1) {
        const Pn *p = &pn[i];
        if (!(m & 1u) || (p->flags & PN_SPOILED)) continue;
        x0 = DOOMO_MIN(x0, clipx(p->x)); y0 = DOOMO_MIN(y0, clipy(p->y));
        x1 = DOOMO_MAX(x1, clipx(p->x + p->w)); y1 = DOOMO_MAX(y1, clipy(p->y + p->h));
    }
    if (x0 >= x1 || y0 >= y1) {
        x0 = y0 = x1 = y1 = 0;
        /* nothing left to watch: the 2D calls' fast path again */
        if (gba_nodraw == GBA_NODRAW_PANEL) gba_nodraw = 0;
    }
    pn_box[0] = x0; pn_box[1] = y0; pn_box[2] = x1; pn_box[3] = y1;
}

/* ---- routing: every 2D call while panels are up (vid_route) ------------- */
int pn_active(void) { return cap >= 0 || put >= 0 || shown != 0; }

void pn_route(int x0, int y0, int x1, int y1)
{
    int i, spoil = 0;
    unsigned m;
    if (x1 <= pn_box[0] || x0 >= pn_box[2] || y1 <= pn_box[1] || y0 >= pn_box[3]) return;
    /* drawn over a panel shown earlier on this frame: not kept next frame (C3: only the shown bits are walked) */
    for (i = 0, m = shown; m; i++, m >>= 1) {
        Pn *p = &pn[i];
        if (!(m & 1u) || i == cap || i == put || (p->flags & PN_SPOILED)) continue;
        if (x1 <= p->x || x0 >= p->x + p->w || y1 <= p->y || y0 >= p->y + p->h) continue;
        if (x0 >= p->hx0 && x1 <= p->hx1 && y0 >= p->hy0 && y1 <= p->hy1) continue;
        p->flags |= PN_SPOILED;
        spoil = 1;
    }
    if (spoil && cap < 0 && put < 0) box_shown();
    if (cap >= 0) {
        Pn *p = &pn[cap];
        /* shading is not body; text lies on body (platform.h), its box as
         * routed is padded */
        if (!(p->flags & PN_KEEP) || pn_op == PN_OP_SHADE || pn_op == PN_OP_TEXT) return;
        if (x0 < p->x || y0 < p->y || x1 > p->x + p->w || y1 > p->y + p->h) { nokeep(); return; }
        if (pn_op == PN_OP_RECT && !(p->flags & PN_SOLID)) add_rect(p, x0, y0, x1, y1);   /* (solid: one rect at the end) */
    }
}

/* a call that a kept panel cannot hold (plat_clear, OBJs) */
void pn_nokeep(void) { nokeep(); }

/* ---- the copy of a kept panel: front page -> back page ------------------- */
/* one row's span (video_loops.s, ARM in IWRAM: halfword edges, a DMA) */
void pn_span(volatile uint16_t *d, const volatile uint16_t *s, int x0, int x1) IWRAM_CALL;

/* the panel's rects, less its hole, from the front page to the back page */
static void put_back(const Pn *p)
{
    const uint16_t *front = (const uint16_t *)((p->flags & PN_PAGE1) ? VRAM_PAGE1 : VRAM_PAGE0);
    const PnRect *r = pool + p->r0;
    int i, y;
    for (i = 0; i < p->nr; i++, r++) {
        uint16_t *d = gba_back + r->y0 * (SCREEN_W / 2);
        const uint16_t *s = front + r->y0 * (SCREEN_W / 2);
        int split = r->x0 < p->hx1 && r->x1 > p->hx0;
        for (y = r->y0; y < r->y1; y++, d += SCREEN_W / 2, s += SCREEN_W / 2) {
            if (split && y >= p->hy0 && y < p->hy1) {
                if (r->x0 < p->hx0) pn_span(d, s, r->x0, p->hx0);
                if (r->x1 > p->hx1) pn_span(d, s, p->hx1, r->x1);
            } else {
                pn_span(d, s, r->x0, r->x1);
            }
        }
    }
}

/* ---- platform.h ------------------------------------------------------------ */
int plat_panel_begin(int id, int x, int y, int w, int h, uint32_t key)
{
    Pn *p;
    int back;
    if (suspended || cap >= 0 || put >= 0) { nest++; return 1; }   /* (no panels inside panels) */
    if (gba_nodraw == 1) { put = -2; return 0; }    /* a skipped tick: nothing to draw */
    if (id < 0 || id >= PANEL_COUNT || w <= 0 || h <= 0 || w > 255 || h > 255) { nest++; return 1; }
    if (gba_nodraw == GBA_NODRAW_FIRST) vid_route(clipx(x), clipy(y), clipx(x + w), clipy(y + h));
    if (gba_nodraw != 0 && gba_nodraw != GBA_NODRAW_MEASURE && gba_nodraw != GBA_NODRAW_PANEL) {
        nest++;                                    /* a tile frame: the canvas, as before */
        return 1;
    }
    p = &pn[id];
    back = gba_back == (uint16_t *)VRAM_PAGE1;
    if ((p->flags & (PN_LIVE | PN_KEEP | PN_SPOILED)) == (PN_LIVE | PN_KEEP) && p->key == key &&
        p->x == x && p->y == y && p->w == w && p->h == h && p->serial == flips &&
        shown_page == ((p->flags & PN_PAGE1) != 0) && back != ((p->flags & PN_PAGE1) != 0) &&
        !gba_text_armed) {
        put = (int8_t)id;                         /* kept: the core draws its shading */
        p->miss_wait = 0;
        box_screen();
        return 0;
    }
    pool_free(id);
    /* a panel that changes or is drawn over on every frame (the typewriter,
     * a toast over the clock, the pause panel) costs its capture for
     * nothing: after two misses in a row it is drawn uncaptured for 2, 4,
     * then 8 frames between tries */
    if (p->miss_wait >> 4) {
        p->miss_wait = (uint8_t)(p->miss_wait - 16);
        p->flags = 0;
        nest++;
        return 1;
    }
    if (p->flags & PN_LIVE) {
        int miss = p->miss_wait & 15;
        if (miss < 15) miss++;
        p->miss_wait = (uint8_t)(miss | (miss >= 2 ? (miss >= 4 ? 8 : 1 << (miss - 1)) : 0) << 4);
    }
    p->key = key;
    p->x = (int16_t)x; p->y = (int16_t)y; p->w = (uint8_t)w; p->h = (uint8_t)h;
    p->flags = PN_LIVE | PN_KEEP;           /* (the page: shown_now) */
    p->hx0 = p->hy0 = p->hx1 = p->hy1 = 0;
    p->r0 = pool_n;
    p->nr = 0;
    cap = (int8_t)id;
    box_screen();
    return 1;
}

void plat_panel_hole(int x, int y, int w, int h)
{
    Pn *p;
    if (cap < 0 || nest) return;
    p = &pn[cap];
    p->hx0 = (uint8_t)clipx(x); p->hy0 = (uint8_t)clipy(y);
    p->hx1 = (uint8_t)clipx(x + w); p->hy1 = (uint8_t)clipy(y + h);
}

static void shown_now(Pn *p, int id)
{
    p->flags = (uint8_t)((p->flags & ~PN_PAGE1) | (gba_back == (uint16_t *)VRAM_PAGE1 ? PN_PAGE1 : 0));
    p->serial = (uint16_t)(flips + 1);
    p->flags &= (uint8_t)~PN_SPOILED;
    shown |= (uint16_t)(1u << id);
}

/* CR9 (platform.h): the whole rect comes back on a kept frame: its rects become that one rect */
void plat_panel_solid(void)
{
    if (cap < 0 || nest) return;
    pn[cap].flags |= PN_SOLID;
}

void plat_panel_end(void)
{
    Pn *p;
    if (nest) { nest--; return; }
    if (put == -2) { put = -1; return; }         /* the skipped tick's */
    if (put >= 0) {
        p = &pn[put];
        if (gba_nodraw == GBA_NODRAW_MEASURE)      /* the tile estimate (canvas.c) */
            cv_note(clipx(p->x), clipy(p->y), clipx(p->x + p->w), clipy(p->y + p->h));
        put_back(p);
        shown_now(p, put);
        put = -1;
        box_shown();
        return;
    }
    if (cap < 0) return;
    p = &pn[cap];
    if (!(p->flags & PN_KEEP)) pool_free(cap);
    else if (p->flags & PN_SOLID) {
        /* one rect, one copy per row (its rects are the last ones in the pool) */
        pool_free(cap);
        p->r0 = pool_n;
        p->nr = 0;
        add_rect(p, clipx(p->x), clipy(p->y), clipx(p->x + p->w), clipy(p->y + p->h));
    }
    shown_now(p, cap);
    cap = -1;
    box_shown();
}

/* I10 (hold.c, PLAT_BUILD_SHOWN): the news strip is drawn into the shown page; panels must not see that page as a
 * back page. Every begin inside counts as nested (a plain draw) and its end only counts down. */
void pn_suspend(int on) { suspended = (uint8_t)(on != 0); }

/* ---- the frame ---------------------------------------------------------------- */
/* plat_begin_frame: nothing shown yet */
void pn_frame_begin(void)
{
    shown = 0;
    cap = put = -1;
    nest = 0;
    suspended = 0;
    pn_box[0] = pn_box[1] = pn_box[2] = pn_box[3] = 0;
}

/* video_flip: the page now shown (0 / 1), -1 for a tile frame */
void pn_flipped(int page)
{
    flips++;
    shown_page = (int8_t)page;
}
