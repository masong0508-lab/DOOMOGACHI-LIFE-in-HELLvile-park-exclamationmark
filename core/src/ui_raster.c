/* ui_raster.c - the span rasteriser behind the UI kit (owner: foundation).
 *
 * Everything the UI draws ends up as horizontal spans, and on the consoles
 * every plat_rect call costs far more than the pixels it fills (GBA: a
 * THUMB -> ARM call, clipping and odd-pixel read-modify-writes; PS1: one
 * GPU primitive plus the platform's batching). This file turns shapes into
 * as few calls as possible, using integer scanline maths only (no division
 * per pixel, no 64-bit arithmetic):
 *
 *  * ur_run / ur_begin / ur_end: a run emitter. Shapes are produced row by
 *    row as runs of their FINAL colours (nothing inside one shape
 *    overlaps), so a run that repeats in the next row is merged into one
 *    taller rect, and the rects may go out in any order.
 *  * bit-row masks: shaded shapes (hearts, stars) are rasterised into rows
 *    of 32-bit words, one bit per pixel, built from per-row spans. The
 *    outline / shadow / light / base classes of the coverage-mask shader
 *    are computed with word operations and the runs are read off the
 *    class boundaries - pixel for pixel what the old per-pixel shader
 *    (nine mask lookups per pixel) drew.
 *  * a shape cache: the finished rect list of a cached shape (relative to
 *    an anchor) is kept, so the hearts, stars, cursors and big titles
 *    that menus redraw every frame cost one plat_rect per rect.
 */
#include "ui_internal.h"

/* Weak default of plat_draw_skipped (platform.h): every frame is shown. */
__attribute__((weak)) int plat_draw_skipped(void)
{
    return 0;
}

/* Weak default of plat_checker (platform.h): one pixel per call. */
__attribute__((weak)) void plat_checker(int x, int y, int w, int h, uint8_t color)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = j & 1; i < w; i += 2) plat_rect(x + i, y + j, 1, 1, color);
}

/* ====================================================================== */
/* shape cache                                                              */
#define SC_POOL 640                    /* rects, 4 bytes each (2.5 KB): the
                                          UI gallery's busiest pages run as
                                          fast with 512 as with 1024        */
#define SC_ENT  40
#define SC_HASH 128                    /* open-addressed index of entries   */
#define SC_PAL  8
/* packed rect: x 8 bits | w 8 | y 7 | h 6 | colour index 3 */
typedef uint32_t SRect;
typedef struct {
    uint32_t k0, k1, k2;
    uint16_t start, n;
    uint8_t pal[SC_PAL];
} SEnt;
static SRect sc_pool[SC_POOL];
static SEnt sc_ent[SC_ENT];
static int sc_top, sc_nent;
static uint8_t sc_hash[SC_HASH];       /* entry index + 1, 0 = empty slot   */
static uint32_t sc_resets, sc_misses, sc_hits;

static unsigned sc_slot(uint32_t k0, uint32_t k1, uint32_t k2)
{
    uint32_t h = k0 * 0x9E3779B1u ^ k1 * 0x85EBCA77u ^ k2 * 0xC2B2AE3Du;
    return (h ^ (h >> 15)) & (SC_HASH - 1);
}

static void sc_reset(void)
{
    sc_top = 0;
    sc_nent = 0;
    memset(sc_hash, 0, sizeof(sc_hash));
    sc_resets++;
}

/* recording state (ur_cache_begin .. ur_cache_end) */
static SEnt *rec_e;
static int rec_ax, rec_ay, rec_fail, rec_npal;

static void rec_add(int x, int y, int w, int h, uint8_t c)
{
    int rx = x - rec_ax, ry = y - rec_ay, i;
    if (rec_fail) return;
    if (rx < 0 || ry < 0 || rx > 255 || ry > 127 || w <= 0 || h <= 0 || w > 255 ||
        h > 63) {
        rec_fail = 2;                   /* this shape cannot be cached     */
        return;
    }
    if (sc_top + rec_e->n >= SC_POOL) { rec_fail = 1; return; }    /* full */
    for (i = 0; i < rec_npal && rec_e->pal[i] != c; i++) {}
    if (i == rec_npal) {
        if (rec_npal == SC_PAL) { rec_fail = 2; return; }
        rec_e->pal[rec_npal++] = c;
    }
    sc_pool[sc_top + rec_e->n++] = (uint32_t)rx | (uint32_t)w << 8 | (uint32_t)ry << 16 |
                                   (uint32_t)h << 23 | (uint32_t)i << 29;
}

static void sc_replay(const SEnt *e, int ax, int ay)
{
    const SRect *r = &sc_pool[e->start], *end = r + e->n;
    for (; r < end; r++) {
        uint32_t v = *r;
        plat_rect(ax + (int)(v & 255), ay + (int)((v >> 16) & 127), (int)((v >> 8) & 255),
                  (int)((v >> 23) & 63), e->pal[v >> 29]);
    }
}

int ur_cache_draw(uint32_t k0, uint32_t k1, uint32_t k2, int ax, int ay)
{
    unsigned slot = sc_slot(k0, k1, k2), n;
    for (n = 0; n < SC_HASH && sc_hash[slot]; n++, slot = (slot + 1) & (SC_HASH - 1)) {
        const SEnt *e = &sc_ent[sc_hash[slot] - 1];
        if (e->k0 == k0 && e->k1 == k1 && e->k2 == k2) {
            sc_replay(e, ax, ay);
            sc_hits++;
            return 1;
        }
    }
    sc_misses++;
    return 0;
}

void ur_cache_stats(int *used, int *entries, uint32_t *hits, uint32_t *misses,
                    uint32_t *resets)
{
    *used = sc_top; *entries = sc_nent;
    *hits = sc_hits; *misses = sc_misses; *resets = sc_resets;
}

void ur_cache_begin(uint32_t k0, uint32_t k1, uint32_t k2, int ax, int ay)
{
    if (sc_nent == SC_ENT) sc_reset();
    rec_e = &sc_ent[sc_nent];
    rec_e->k0 = k0; rec_e->k1 = k1; rec_e->k2 = k2;
    rec_e->start = (uint16_t)sc_top;
    rec_e->n = 0;
    rec_ax = ax; rec_ay = ay;
    rec_fail = 0;
    rec_npal = 0;
}

/* Returns 1 if the shape was recorded (and drawn by replaying it), 0 if it
 * did not fit: then nothing was drawn and the caller draws it directly. */
int ur_cache_end(void)
{
    SEnt *e = rec_e;
    rec_e = NULL;
    if (!e) return 0;
    if (rec_fail) {
        /* pool full: start over (the shapes in use come back next frame) */
        if (rec_fail == 1) sc_reset();
        return 0;
    }
    sc_top += e->n;
    sc_nent++;
    {
        unsigned slot = sc_slot(e->k0, e->k1, e->k2);
        while (sc_hash[slot]) slot = (slot + 1) & (SC_HASH - 1);   /* never full */
        sc_hash[slot] = (uint8_t)sc_nent;
    }
    sc_replay(e, rec_ax, rec_ay);
    return 1;
}

/* An emitter that skips rows outside the screen calls this: a recorded
 * shape that lost rows would come back cut when replayed elsewhere, so
 * such a shape is drawn directly instead (ur_cache_end returns 0). */
static void rec_clipped(void)
{
    if (rec_e && !rec_fail) rec_fail = 2;
}

/* every rect of a shape goes through here (direct draw or recording) */
static void out_rect(int x, int y, int w, int h, uint8_t c)
{
    if (rec_e) rec_add(x, y, w, h, c);
    else plat_rect(x, y, w, h, c);
}

void ur_rect(int x, int y, int w, int h, uint8_t c)
{
    if (w > 0 && h > 0) out_rect(x, y, w, h, c);
}

/* ====================================================================== */
/* run emitter: identical runs of consecutive rows become one rect          */
typedef struct { int16_t x, y, w; uint8_t h, c; } URect;   /* 8 bytes */
#define EM_MAX 64
static URect em_a[EM_MAX], em_b[EM_MAX];
static URect *em_open = em_a, *em_cur = em_b;
static int em_nopen, em_ncur;
static int em_cur_y, em_open_y;

static void em_flush_open(void)
{
    int i;
    for (i = 0; i < em_nopen; i++)
        out_rect(em_open[i].x, em_open[i].y, em_open[i].w, em_open[i].h, em_open[i].c);
    em_nopen = 0;
}

/* merge the collected row (em_cur, row em_cur_y) into the open rects */
static void em_finish_row(void)
{
    int i = 0, j;
    URect *t;
    if (em_ncur == 0) return;
    if (em_cur_y != em_open_y + 1) em_flush_open();
    for (j = 0; j < em_ncur; j++) {
        URect *r = &em_cur[j];
        while (i < em_nopen && em_open[i].x < r->x) {
            out_rect(em_open[i].x, em_open[i].y, em_open[i].w, em_open[i].h, em_open[i].c);
            i++;
        }
        if (i < em_nopen && em_open[i].x == r->x && em_open[i].w == r->w &&
            em_open[i].c == r->c && em_open[i].h < 255) {
            r->y = em_open[i].y;               /* continue the open rect */
            r->h = (uint8_t)(em_open[i].h + 1);
            i++;
        }
    }
    for (; i < em_nopen; i++)
        out_rect(em_open[i].x, em_open[i].y, em_open[i].w, em_open[i].h, em_open[i].c);
    t = em_open; em_open = em_cur; em_cur = t;
    em_nopen = em_ncur;
    em_ncur = 0;
    em_open_y = em_cur_y;
}

void ur_begin(void)
{
    em_nopen = em_ncur = 0;
    em_cur_y = em_open_y = -0x4000;
}

void ur_end(void)
{
    em_finish_row();
    em_flush_open();
}

/* Add the run [x, x + w) of row y. Rows must come in increasing order and
 * the runs of a row from left to right, never overlapping. */
void ur_run(int y, int x, int w, uint8_t c)
{
    URect *p;
    if (w <= 0) return;
    if (y != em_cur_y) { em_finish_row(); em_cur_y = y; }
    if (em_ncur) {
        p = &em_cur[em_ncur - 1];
        if (p->c == c && p->x + p->w == x) { p->w = (int16_t)(p->w + w); return; }
    }
    if (em_ncur == EM_MAX) { out_rect(x, y, w, 1, c); return; }
    p = &em_cur[em_ncur++];
    p->x = (int16_t)x; p->y = (int16_t)y; p->w = (int16_t)w; p->h = 1; p->c = c;
}

/* ====================================================================== */
/* tables                                                                   */
/* index of the lowest set bit (de Bruijn; the ARM7 has no clz) */
static const uint8_t debruijn[32] = {
    0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
    31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};
#define LOWBIT(v) (debruijn[(((v) & (0u - (v))) * 0x077CB531u) >> 27])

/* Rounded-corner insets per radius (ui_corner_inset semantics), const so
 * they cost ROM, not RAM: corner_inset_calc(r, i) for r <= CI_MAX
 * (generated with it; test_ui_raster.c checks every entry). */
#define CI_MAX 16
static const uint8_t ci_tab[CI_MAX + 1][CI_MAX] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 4, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 4, 3, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 4, 3, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 6, 4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 },
    { 7, 5, 4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 },
    { 8, 6, 5, 4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0 },
    { 8, 7, 5, 4, 3, 3, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
    { 9, 7, 6, 5, 4, 3, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0 },
    { 10, 8, 6, 5, 4, 3, 3, 2, 2, 1, 1, 1, 1, 1, 0, 0 },
    { 11, 9, 7, 6, 5, 4, 3, 3, 2, 2, 1, 1, 1, 1, 1, 0 },
};

static int corner_inset_calc(int r, int i)
{
    int d, k;
    if (r <= 0 || i >= r) return 0;
    if (r == 1) return 1;
    if (r == 2) return i == 0 ? 2 : 1;
    if (r == 3) { static const int t[] = { 3, 1, 1 }; return t[i]; }
    if (r == 4) { static const int t[] = { 4, 2, 1, 1 }; return t[i]; }
    d = r - i;                       /* distance from the corner centre row */
    k = (int)fx_isqrt((uint32_t)(r * r - (d - 1) * (d - 1)) * 4u) / 2;
    return r - k;
}

int ui_corner_inset(int r, int i)
{
    if (r <= 0 || i < 0 || i >= r) return 0;
    if (r <= CI_MAX) return ci_tab[r][i];
    return corner_inset_calc(r, i);
}

/* Half-widths of filled circles (ui_fill_ellipse with rx == ry):
 * circ_tab[r][y] = isqrt((r*r - y*y) * 4 + 2r) / 2 for y <= r (const,
 * generated; test_ui_raster.c checks it). */
#define CIRC_MAX 15
static const uint8_t circ_tab[CIRC_MAX + 1][CIRC_MAX + 1] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 4, 4, 3, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 4, 4, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 6, 6, 5, 5, 4, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 7, 7, 6, 6, 6, 5, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 8, 8, 8, 7, 7, 6, 5, 4, 2, 0, 0, 0, 0, 0, 0, 0 },
    { 9, 9, 9, 8, 8, 7, 7, 6, 4, 2, 0, 0, 0, 0, 0, 0 },
    { 10, 10, 10, 9, 9, 8, 8, 7, 6, 4, 2, 0, 0, 0, 0, 0 },
    { 11, 11, 11, 10, 10, 10, 9, 8, 7, 6, 5, 2, 0, 0, 0, 0 },
    { 12, 12, 12, 11, 11, 11, 10, 10, 9, 8, 7, 5, 2, 0, 0, 0 },
    { 13, 13, 13, 12, 12, 12, 11, 11, 10, 9, 8, 7, 5, 2, 0, 0 },
    { 14, 14, 14, 13, 13, 13, 12, 12, 11, 11, 10, 9, 7, 5, 2, 0 },
    { 15, 15, 15, 14, 14, 14, 14, 13, 12, 12, 11, 10, 9, 7, 6, 2 },
};

int ui_circle_dx(int r, int y)
{
    if (r < 0 || y < 0 || y > r) return 0;
    if (r <= CIRC_MAX) return circ_tab[r][y];
    return (int)(fx_isqrt((uint32_t)((r * r - y * y) * 4 + r * 2)) / 2);
}

static const uint8_t *circle_dx(int r)
{
    return circ_tab[r];
}

/* ====================================================================== */
/* filled ellipses: the union of one rect per distinct half-width           */
/* Half-widths are non-increasing away from the centre row, so the rows
 * whose half-width is >= v form one band around it: the shape is the union
 * of one rect per distinct half-width (same colour, so overlap is free). */
static void nested_bands(int cx, int cy, const int *dxs, int ry, uint8_t c)
{
    int y = ry;
    while (y >= 0) {
        int v = dxs[y], y2 = y;
        /* rows |dy| <= y all have half-width >= v; rows beyond y2 ... */
        while (y2 > 0 && dxs[y2 - 1] == v) y2--;
        plat_rect(cx - v, cy - y, 2 * v + 1, 2 * y + 1, c);
        y = y2 - 1;
    }
}

void ui_fill_ellipse(int cx, int cy, int rx, int ry, uint8_t c)
{
    int dxs[CIRC_MAX + 1];
    int y;
    if (plat_draw_skipped()) return;
    if (rx < 0 || ry < 0) return;
    if (ry == 0) { plat_rect(cx - rx, cy, 2 * rx + 1, 1, c); return; }
    if (rx == ry && ry <= CIRC_MAX) {
        const uint8_t *t = circle_dx(ry);
        for (y = 0; y <= ry; y++) dxs[y] = t[y];
        nested_bands(cx, cy, dxs, ry, c);
        return;
    }
    if (ry <= CIRC_MAX) {
        for (y = 0; y <= ry; y++) {
            /* x extent: rx * sqrt(1 - y^2/ry^2), +0.5 ry for rounder pixels */
            uint32_t t = (uint32_t)((ry * ry - y * y) * 4 + ry * 2);
            dxs[y] = (int)(fx_isqrt(t * (uint32_t)(rx * rx) / (uint32_t)(ry * ry)) / 2);
        }
        nested_bands(cx, cy, dxs, ry, c);
        return;
    }
    /* big: one span per row, rows of equal width merged */
    {
        int prev = -1, y0 = 0;
        int sh = plat_screen_h();
        for (y = -ry; y <= ry + 1; y++) {
            int dx = -2;
            if (y <= ry) {
                int ay = y < 0 ? -y : y;
                uint32_t t = (uint32_t)((ry * ry - ay * ay) * 4 + ry * 2);
                if (rx == ry) dx = (int)(fx_isqrt(t) / 2);
                else dx = (int)(fx_isqrt(t * (uint32_t)(rx * rx) / (uint32_t)(ry * ry)) / 2);
                if (cy + y < 0 || cy + y >= sh) dx = -2;     /* off screen */
            }
            if (dx != prev) {
                if (prev >= 0) plat_rect(cx - prev, cy + y0, 2 * prev + 1, y - y0, c);
                prev = dx;
                y0 = y;
            }
        }
    }
}

void ui_fill_circle(int cx, int cy, int r, uint8_t c)
{
    ui_fill_ellipse(cx, cy, r, r, c);
}

/* ====================================================================== */
/* rounded rects and shadows: rows grouped by inset                         */
/* Inset of row i of a rounded rect h rows tall with corner radius r (the
 * top corner wins where the two overlap, like the original row loop). */
static int corner(int r, int i)
{
    if (r <= CI_MAX) return ci_tab[r][i];
    return ui_corner_inset(r, i);
}

static int rr_inset(int r, int h, int i)
{
    if (i < r) return corner(r, i);
    if (i >= h - r) return corner(r, h - 1 - i);
    return 0;
}

/* Calls fn for each band of consecutive rows with the same inset. */
static void rr_bands(int x, int y, int w, int h, int r,
                     void (*fn)(int x, int y, int w, int h, int arg), int arg)
{
    int i = 0, m0, m1;
    if (r < 0) r = 0;
    m0 = r;                             /* straight middle rows [m0, m1)   */
    m1 = h - r;
    while (i < h) {
        int in = rr_inset(r, h, i), j;
        j = (i >= m0 && i < m1) ? m1 : i + 1;
        while (j < h && rr_inset(r, h, j) == in) j = (j >= m0 && j < m1) ? m1 : j + 1;
        if (w - 2 * in > 0) fn(x + in, y + i, w - 2 * in, j - i, arg);
        i = j;
    }
}

static void band_shade(int x, int y, int w, int h, int l) { plat_shade_rect(x, y, w, h, l); }

/* The top corner of radius r as bands of rows with equal inset (const,
 * generated from ci_tab; test_ui_raster.c checks them): rg_len[r][k] rows
 * with inset rg_in[r][k], k < rg_n[r]. */
static const uint8_t rg_n[CI_MAX + 1] = { 0, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 7, 8, 8, 9, 9, 10 };
static const uint8_t rg_len[CI_MAX + 1][CI_MAX] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 2, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 2, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 2, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 2, 4, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 2, 1, 5, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 1, 2, 5, 1, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 2, 2, 5, 1, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 1, 2, 2, 5, 1, 0, 0, 0, 0, 0, 0 },
};
static const uint8_t rg_in[CI_MAX + 1][CI_MAX] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 4, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 6, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 7, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 8, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 8, 7, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 9, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 10, 8, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 11, 9, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0 },
};

static int corner_bands(int r)
{
    return rg_n[r];
}

/* (tests) the band tables of radius r <= 16 */
int ui_corner_bands(int r, const uint8_t **len, const uint8_t **in)
{
    if (r < 0 || r > CI_MAX) return -1;
    if (len) *len = rg_len[r];
    if (in) *in = rg_in[r];
    return rg_n[r];
}

/* Rows [row0, row1) of ui_round_rect(x, y, w, h, r, c), r already clamped
 * (0 < r, 2r <= h): the top corner bands, the straight middle, the bottom
 * corner bands, each band one plat_rect. */
static void rr_rows(int x, int y, int w, int h, int r, uint8_t c, int row0, int row1)
{
    int k, n, i;
    if (r > CI_MAX) {                       /* rare: generic */
        int j;
        for (i = DOOMO_MAX(row0, 0); i < row1 && i < h;) {
            int in = rr_inset(r, h, i);
            j = i + 1;
            while (j < row1 && j < h && rr_inset(r, h, j) == in) j++;
            if (w - 2 * in > 0) out_rect(x + in, y + i, w - 2 * in, j - i, c);
            i = j;
        }
        return;
    }
    n = corner_bands(r);
    for (k = 0, i = 0; k < n; i += rg_len[r][k], k++) {          /* top */
        int a = DOOMO_MAX(i, row0), b = DOOMO_MIN(i + rg_len[r][k], row1), in = rg_in[r][k];
        if (a < b && w - 2 * in > 0) out_rect(x + in, y + a, w - 2 * in, b - a, c);
    }
    {                                                            /* middle */
        int a = DOOMO_MAX(r, row0), b = DOOMO_MIN(h - r, row1);
        if (a < b) out_rect(x, y + a, w, b - a, c);
    }
    for (k = 0, i = 0; k < n; i += rg_len[r][k], k++) {          /* bottom */
        int a = DOOMO_MAX(h - i - rg_len[r][k], row0), b = DOOMO_MIN(h - i, row1);
        int in = rg_in[r][k];
        if (a < b && w - 2 * in > 0) out_rect(x + in, y + a, w - 2 * in, b - a, c);
    }
}

void ui_round_rect(int x, int y, int w, int h, int r, uint8_t c)
{
    if (plat_draw_skipped()) return;
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r <= 0) { out_rect(x, y, w, h, c); return; }
    rr_rows(x, y, w, h, r, c, 0, h);
}

/* Only rows [row0, row1) of ui_round_rect(x, y, w, h, r, c). */
void ui_round_rect_rows(int x, int y, int w, int h, int r, uint8_t c, int row0, int row1)
{
    if (plat_draw_skipped()) return;
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r <= 0) {
        row0 = DOOMO_MAX(row0, 0);
        row1 = DOOMO_MIN(row1, h);
        if (row0 < row1) plat_rect(x, y + row0, w, row1 - row0, c);
        return;
    }
    rr_rows(x, y, w, h, r, c, row0, row1);
}

void ui_drop_shadow(int x, int y, int w, int h, int r)
{
    if (plat_draw_skipped()) return;
    if (w <= 0 || h <= 0) return;
    rr_bands(x, y, w, h, r, band_shade, 9);
}

/* The drop shadow (sx, sy, w, h, r) minus the part that the opaque rounded
 * rect (ox, oy, ow, oh, orr) - drawn by the caller right afterwards - will
 * cover anyway: the same picture for a fraction of the shading work. The
 * rows are walked in bands: inside the straight parts of both shapes
 * every row is the same. */
static void shade_minus(int sx, int sy, int w, int h, int r, int level,
                        int ox, int oy, int ow, int oh, int orr)
{
    int i, end, sh = plat_screen_h();
    int band_y = 0, band_n = 0, bn = 0, ba[2] = { 0, 0 }, bb[2] = { 0, 0 };
    if (plat_draw_skipped()) return;
    if (w <= 0 || h <= 0) return;
    if (r < 0) r = 0;
    if (orr > ow / 2) orr = ow / 2;
    if (orr > oh / 2) orr = oh / 2;
    if (orr < 0) orr = 0;
    i = DOOMO_MAX(0, -sy);
    end = DOOMO_MIN(h, sh - sy);
    while (i < end) {
        int y = sy + i, j = y - oy, len, n = 0, a2[2] = { 0, 0 }, b2[2] = { 0, 0 };
        int in = rr_inset(r, h, i), k;
        /* rows after this one that look the same */
        len = (i >= r && i < h - r) ? h - r - i : 1;
        if (j < 0) len = DOOMO_MIN(len, -j);
        else if (j < oh) len = DOOMO_MIN(len, (j >= orr && j < oh - orr) ? oh - orr - j : 1);
        len = DOOMO_MIN(len, end - i);
        if (w - 2 * in > 0) {
            int a = sx + in, b = sx + w - 1 - in;
            int oin = (j >= 0 && j < oh && ow > 0) ? rr_inset(orr, oh, j) : -1;
            int oa = ox + oin, ob = ox + ow - 1 - oin;
            if (oin >= 0 && oa <= ob) {
                if (a < oa) { a2[n] = a; b2[n] = DOOMO_MIN(b, oa - 1); n++; }
                if (b > ob) { a2[n] = DOOMO_MAX(a, ob + 1); b2[n] = b; n++; }
            } else {
                a2[n] = a; b2[n] = b; n++;
            }
        }
        if (n == bn && (n < 1 || (a2[0] == ba[0] && b2[0] == bb[0])) &&
            (n < 2 || (a2[1] == ba[1] && b2[1] == bb[1])) && band_y + band_n == y) {
            band_n += len;
        } else {
            for (k = 0; k < bn; k++)
                plat_shade_rect(ba[k], band_y, bb[k] - ba[k] + 1, band_n, level);
            bn = n; band_y = y; band_n = len;
            for (k = 0; k < n; k++) { ba[k] = a2[k]; bb[k] = b2[k]; }
        }
        i += len;
    }
    {
        int k;
        for (k = 0; k < bn; k++) plat_shade_rect(ba[k], band_y, bb[k] - ba[k] + 1, band_n, level);
    }
}

void ui_drop_shadow_under(int sx, int sy, int w, int h, int r,
                          int ox, int oy, int ow, int oh, int orr)
{
    shade_minus(sx, sy, w, h, r, 9, ox, oy, ow, oh, orr);
}

void ui_shade_screen_under(int level, int ox, int oy, int ow, int oh, int orr)
{
    shade_minus(0, 0, plat_screen_w(), plat_screen_h(), 0, level, ox, oy, ow, oh, orr);
}

/* ====================================================================== */
/* bit-row masks + the five-tone shader                                     */
#define MW 3                            /* words per row: up to 96 px      */
static uint32_t mrow[UR_MASK_MAX][MW];
static int mw, mh;

static void mask_begin(int w, int h)
{
    if (w > UR_MASK_MAX) w = UR_MASK_MAX;
    if (h > UR_MASK_MAX) h = UR_MASK_MAX;
    mw = w; mh = h;
    memset(mrow, 0, sizeof(mrow[0]) * (size_t)h);
}

/* set pixels x0..x1 (inclusive) of mask row y */
static void mask_span(int y, int x0, int x1)
{
    uint32_t *row = mrow[y];
    int w0, w1;
    if (x0 < 0) x0 = 0;
    if (x1 >= mw) x1 = mw - 1;
    if (x0 > x1) return;
    w0 = x0 >> 5; w1 = x1 >> 5;
    if (w0 == w1) {
        row[w0] |= (~0u << (x0 & 31)) & (~0u >> (31 - (x1 & 31)));
    } else {
        int k;
        row[w0] |= ~0u << (x0 & 31);
        for (k = w0 + 1; k < w1; k++) row[k] = ~0u;
        row[w1] |= ~0u >> (31 - (x1 & 31));
    }
}

/* o[x] = a[x - n]  (towards higher x), 1 <= n <= 31 */
static void sh_up(uint32_t *o, const uint32_t *a, int n)
{
    o[2] = (a[2] << n) | (a[1] >> (32 - n));
    o[1] = (a[1] << n) | (a[0] >> (32 - n));
    o[0] = a[0] << n;
}

/* o[x] = a[x + n] */
static void sh_dn(uint32_t *o, const uint32_t *a, int n)
{
    o[0] = (a[0] >> n) | (a[1] << (32 - n));
    o[1] = (a[1] >> n) | (a[2] << (32 - n));
    o[2] = a[2] >> n;
}

static const uint32_t zrow[MW];

/* Shade the mask at (ox, oy) through the emitter. pal: outline, shadow,
 * base, light. Per pixel (exactly the old coverage-mask shader): outline
 * if a 4-neighbour is outside; else shadow if (x+d, y+d) or (x, y+d+1) is
 * outside; else light if (x-d, y-d) is outside; else base. */
static void mask_shade(int ox, int oy, const uint8_t pal[5], int d)
{
    int y, k;
    for (y = 0; y < mh; y++) {
        const uint32_t *c = mrow[y];
        const uint32_t *u = y > 0 ? mrow[y - 1] : zrow;
        const uint32_t *dn = y + 1 < mh ? mrow[y + 1] : zrow;
        uint32_t l[MW], r[MW], s1[MW], lt[MW];
        uint32_t po[MW], ps[MW], pl[MW], pb[MW], ch[MW];
        uint32_t any = c[0] | c[1] | c[2];
        int run_x = 0, run_c = -1;
        if (!any) continue;
        sh_up(l, c, 1);
        sh_dn(r, c, 1);
        if (y + d < mh) sh_dn(s1, mrow[y + d], d); else memset(s1, 0, sizeof(s1));
        if (y - d >= 0) sh_up(lt, mrow[y - d], d); else memset(lt, 0, sizeof(lt));
        for (k = 0; k < MW; k++) {
            uint32_t in4 = c[k] & l[k] & r[k] & u[k] & dn[k];
            uint32_t s2 = y + d + 1 < mh ? mrow[y + d + 1][k] : 0;
            uint32_t sh = in4 & ~(s1[k] & s2);
            po[k] = c[k] & ~in4;
            ps[k] = sh;
            pl[k] = in4 & ~sh & ~lt[k];
            pb[k] = in4 & ~sh & lt[k];
        }
        /* class boundaries: bit x set where class(x) != class(x - 1) */
        {
            uint32_t a[MW];
            sh_up(a, po, 1);
            for (k = 0; k < MW; k++) ch[k] = po[k] ^ a[k];
            sh_up(a, ps, 1);
            for (k = 0; k < MW; k++) ch[k] |= ps[k] ^ a[k];
            sh_up(a, pl, 1);
            for (k = 0; k < MW; k++) ch[k] |= pl[k] ^ a[k];
            sh_up(a, pb, 1);
            for (k = 0; k < MW; k++) ch[k] |= pb[k] ^ a[k];
        }
        for (k = 0; k < MW; k++) {
            uint32_t v = ch[k];
            while (v) {
                int b = LOWBIT(v), x = (k << 5) + b, nc;
                uint32_t bit = 1u << b;
                v &= v - 1;
                if (run_c >= 0) ur_run(oy + y, ox + run_x, x - run_x, (uint8_t)run_c);
                if (po[k] & bit) nc = pal[0];
                else if (ps[k] & bit) nc = pal[1];
                else if (pl[k] & bit) nc = pal[3];
                else if (pb[k] & bit) nc = pal[2];
                else nc = -1;
                run_c = nc;
                run_x = x;
            }
        }
    }
}

/* ====================================================================== */
/* hearts                                                                   */
/* Fill the mask with the heart of half-size s scaled to (2hw+1)x(2hh+1):
 * pixel (x, y) is inside if heart_in(px, py) with px = (x - hw) * 2s / hw,
 * py = (y - hh) * 2s / hh (truncating), where heart_in is two lobes of
 * radius 2lr centred at (+-(2lr-1), -lr) and a point whose half-width is
 * 1.1 * (bottom - py). Per row the inside set is at most three px
 * intervals, mapped back to x through the inverse of the px staircase. */
static void heart_mask(int s, int hw, int hh)
{
    int lr = (s + 1) / 2, bottom = 2 * s - 1;
    int first[4 * 32 + 3];              /* first x with px(x) >= v, v = -2s.. */
    int pyv[UR_MASK_MAX];
    int x, y, v, q, rem, k;
    int pxv[UR_MASK_MAX];
    mask_begin(2 * hw + 1, 2 * hh + 1);
    /* px staircase (symmetric: trunc(k*2s/hw)) */
    q = 0; rem = 0;
    for (k = 0; k <= hw; k++) {
        if (hw + k < mw) pxv[hw + k] = q;
        if (hw - k >= 0 && hw - k < mw) pxv[hw - k] = -q;
        rem += 2 * s;
        while (rem >= hw) { rem -= hw; q++; }
    }
    v = -2 * s;
    for (x = 0; x < mw; x++)
        while (v <= pxv[x]) first[(v++) + 2 * s] = x;
    while (v <= 2 * s + 1) first[(v++) + 2 * s] = mw;
    q = 0; rem = 0;
    for (k = 0; k <= hh; k++) {
        if (hh + k < mh) pyv[hh + k] = q;
        if (hh - k >= 0 && hh - k < mh) pyv[hh - k] = -q;
        rem += 2 * s;
        while (rem >= hh) { rem -= hh; q++; }
    }
#define XFIRST(val) ((val) < -2 * s ? 0 : (val) > 2 * s + 1 ? mw : first[(val) + 2 * s])
    for (y = 0; y < mh; y++) {
        int py = pyv[y], ly = py + lr;
        int r2 = lr * lr * 4 + 2 - ly * ly;
        if (r2 >= 0) {
            int dl = (int)fx_isqrt((uint32_t)r2), c0 = 2 * lr - 1;
            int a = -c0 - dl, b = -c0 + dl;
            mask_span(y, XFIRST(a), XFIRST(b + 1) - 1);
            a = c0 - dl; b = c0 + dl;
            mask_span(y, XFIRST(a), XFIRST(b + 1) - 1);
        }
        if (py >= -lr && py <= bottom) {
            int n = bottom - py, t = n + n / 10;          /* floor(n * 11 / 10) */
            mask_span(y, XFIRST(-t), XFIRST(t + 1) - 1);
        }
    }
#undef XFIRST
}

/* Five shades around a fill colour: outline, shadow, base, light, glint. */
void ui_ramp5(uint8_t fill, uint8_t pal[5])
{
    if (fill >= PAL_RAMP_BASE) {
        int s = (fill - PAL_RAMP_BASE) & 7, base = fill - s;
        pal[0] = (uint8_t)(base + DOOMO_MAX(s - 3, 0));
        pal[1] = (uint8_t)(base + DOOMO_MAX(s - 1, 0));
        pal[2] = fill;
        pal[3] = (uint8_t)(base + DOOMO_MIN(s + 1, 7));
        pal[4] = PAL_WHITE;
        return;
    }
    pal[2] = fill;
    pal[1] = ui_darker(fill);
    pal[0] = ui_darker(pal[1]);
    if (pal[0] == pal[1]) pal[0] = ui_darker(pal[0]);
    pal[3] = ui_lighter(fill);
    pal[4] = PAL_WHITE;
}

enum { K_HEART = 1, K_TINY, K_STAR, K_GLYPH, K_TEXT };

static void heart_draw(int cx, int cy, int s, int hw, int hh, uint8_t fill)
{
    uint8_t pal[5];
    int lr = (s + 1) / 2;
    ui_ramp5(fill, pal);
    heart_mask(s, hw, hh);
    ur_begin();
    mask_shade(cx - hw, cy - hh, pal, s >= 8 ? 2 : 1);
    ur_end();
    /* two glossy glints on the left lobe */
    {
        int gx = cx - lr * hw / s, gy = cy - lr * hh / s;
        if (s >= 5) {
            ur_rect(gx - lr / 3, gy - lr / 4, DOOMO_MAX(1, s / 5), DOOMO_MAX(1, s / 4), pal[4]);
            ur_rect(gx + lr / 3 + 1, gy - lr / 2, 1, 1, pal[4]);
        } else {
            ur_rect(cx - lr, cy - lr / 2 - 1, 1, 1, pal[4]);
        }
    }
}

static void tiny_heart_draw(int cx, int cy, uint8_t fill)
{
    static const char *const tiny[4] = { ".#.#.", "#####", ".###.", "..#.." };
    uint8_t pal[5];
    int x, y;
    ui_ramp5(fill, pal);
    ur_begin();
    for (y = 0; y < 4; y++)
        for (x = 0; x < 5; x++)
            if (tiny[y][x] == '#')
                ur_run(cy - 2 + y, cx - 2 + x, 1, (y == 0 || x == 0 || x == 4) ? pal[0] : pal[2]);
    ur_end();
}

void ui_heart_squash(int cx, int cy, int size, int sx256, int sy256, uint8_t fill)
{
    int s = DOOMO_CLAMP(size, 2, 32);
    int hw = s * DOOMO_CLAMP(sx256, 128, 400) / 256, hh = s * DOOMO_CLAMP(sy256, 128, 400) / 256;
    uint32_t key;
    if (plat_draw_skipped()) return;
    if (s <= 2) {                          /* tiny: a 5x4 pixel heart */
        if (ur_cache_draw(K_TINY, fill, 0, cx - 2, cy - 2)) return;
        ur_cache_begin(K_TINY, fill, 0, cx - 2, cy - 2);
        tiny_heart_draw(cx, cy, fill);
        if (!ur_cache_end()) tiny_heart_draw(cx, cy, fill);
        return;
    }
    hw = DOOMO_CLAMP(hw, 2, (UR_MASK_MAX - 1) / 2);
    hh = DOOMO_CLAMP(hh, 2, (UR_MASK_MAX - 1) / 2);
    key = K_HEART | (uint32_t)s << 8 | (uint32_t)hw << 16 | (uint32_t)hh << 24;
    if (ur_cache_draw(key, fill, 0, cx - 40, cy - 40)) return;
    ur_cache_begin(key, fill, 0, cx - 40, cy - 40);
    heart_draw(cx, cy, s, hw, hh, fill);
    if (!ur_cache_end()) heart_draw(cx, cy, s, hw, hh, fill);
}

/* Spans of row y (relative to the centre) of the flat heart of half-size
 * s: two lobes and the point, merged; returns the count (<= 3). */
static int hf_row(int s, int y, int sp[3][2])
{
    int lr = (s + 1) / 2, bottom = 2 * s - 1;
    int py = 2 * y, ly = py + lr, n = 0, i, j, m;
    int32_t r2 = (int32_t)lr * lr * 4 + 2 - (int32_t)ly * ly;
    if (r2 >= 0) {
        int dx = (int)fx_isqrt((uint32_t)r2), k;
        for (k = -1; k <= 1; k += 2) {
            int c0 = k * (2 * lr - 1);
            int x0 = (c0 - dx + 1) >> 1, x1 = (c0 + dx) >> 1;
            if (x1 >= x0) { sp[n][0] = x0; sp[n][1] = x1; n++; }
        }
    }
    if (py >= -lr && py <= bottom) {
        int half = DOOMO_MIN((bottom - py) * 11 / 10, 2 * s);   /* bbox like ui_heart */
        int x0 = (-half + 1) >> 1, x1 = half >> 1;
        if (x1 >= x0) { sp[n][0] = x0; sp[n][1] = x1; n++; }
    }
    /* sort (<= 3) and merge touching / overlapping spans */
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && sp[j][0] < sp[j - 1][0]; j--) {
            int t0 = sp[j][0], t1 = sp[j][1];
            sp[j][0] = sp[j - 1][0]; sp[j][1] = sp[j - 1][1];
            sp[j - 1][0] = t0; sp[j - 1][1] = t1;
        }
    for (i = 0, m = 0; i < n; i++) {
        int a = sp[i][0], b = sp[i][1];
        while (i + 1 < n && sp[i + 1][0] <= b + 1) { b = DOOMO_MAX(b, sp[i + 1][1]); i++; }
        sp[m][0] = a; sp[m][1] = b; m++;
    }
    return m;
}

static int hf_size(int s)
{
    if (s < 2) s = 2;
    if (s > 16000) s = 16000;               /* keeps every product in 32 bits */
    return s;
}

/* Flat heart of any size as spans (wipes, silhouettes): per row the two
 * lobes and the point are merged into at most two spans, and only rows on
 * the screen are visited. */
void ui_heart_flat(int cx, int cy, int s, uint8_t c)
{
    int y, y0, y1, sh = plat_screen_h();
    if (plat_draw_skipped()) return;
    s = hf_size(s);
    y0 = DOOMO_MAX(-s, -cy);
    y1 = DOOMO_MIN(s, sh - 1 - cy);
    ur_begin();
    for (y = y0; y <= y1; y++) {
        int sp[3][2], n = hf_row(s, y, sp), i;
        for (i = 0; i < n; i++) ur_run(cy + y, cx + sp[i][0], sp[i][1] - sp[i][0] + 1, c);
    }
    ur_end();
}

int ui_heart_flat_covers_screen(int cx, int cy, int s)
{
    int y, sw = plat_screen_w(), sh = plat_screen_h();
    s = hf_size(s);
    if (cy - s > 0 || cy + s < sh - 1) return 0;
    for (y = -cy; y < sh - cy; y++) {
        int sp[3][2], n = hf_row(s, y, sp), i, ok = 0;
        for (i = 0; i < n; i++)
            if (cx + sp[i][0] <= 0 && cx + sp[i][1] >= sw - 1) ok = 1;
        if (!ok) return 0;
    }
    return 1;
}

/* ====================================================================== */
/* stars                                                                    */
/* Five-point star, outer radius s: a pixel (x, y) is inside when the
 * even-odd test of its point ((x - s) * 256, (y - s) * 256 + 64) against
 * the ten 8.8 vertices says so. Per row each crossing edge gives the first
 * x left of which it counts (one division per edge and row), and the
 * inside set is every other interval between the sorted crossings. */
static void star_mask(int s)
{
    int32_t vx[10], vy[10];
    int i, j, y, w = 2 * s + 1;
    mask_begin(w, w);
    for (i = 0; i < 10; i++) {
        int rad = (i & 1) ? s * 2 / 5 + 1 : s;
        int a = 3072 + i * 4096 / 10;
        vx[i] = (fx_cos(a) * rad) >> 8;   /* 8 fractional bits */
        vy[i] = (fx_sin(a) * rad) >> 8;
    }
    for (y = 0; y < mh; y++) {
        int32_t py = (y - s) * 256 + 64;
        int kx[10], n = 0, a, b;
        for (i = 0, j = 9; i < 10; j = i++) {
            if ((vy[i] > py) != (vy[j] > py)) {
                int32_t X = (vx[j] - vx[i]) * (py - vy[i]) / (vy[j] - vy[i]) + vx[i];
                /* px < X  <=>  x < s + ceil(X / 256) */
                int k = s + ((X + 255) >> 8);
                for (a = n++; a > 0 && kx[a - 1] > k; a--) kx[a] = kx[a - 1];
                kx[a] = k;
            }
        }
        for (a = 0; a + 1 < n; a += 2) {
            b = kx[a + 1] - 1;
            mask_span(y, kx[a], b);
        }
    }
}

static void star_draw(int cx, int cy, int s, uint8_t fill)
{
    uint8_t pal[5];
    ui_ramp5(fill, pal);
    star_mask(s);
    ur_begin();
    mask_shade(cx - s, cy - s, pal, 1);
    ur_end();
    if (s >= 5) ur_rect(cx - s / 4, cy - s / 4, 1, 1, PAL_WHITE);
}

void ui_star(int cx, int cy, int r, uint8_t fill)
{
    int s = DOOMO_CLAMP(r, 3, 34);
    uint32_t key = K_STAR | (uint32_t)s << 8;
    if (plat_draw_skipped()) return;
    if (s > 16) { star_draw(cx, cy, s, fill); return; }
    if (ur_cache_draw(key, fill, 0, cx - 40, cy - 40)) return;
    ur_cache_begin(key, fill, 0, cx - 40, cy - 40);
    star_draw(cx, cy, s, fill);
    if (!ur_cache_end()) star_draw(cx, cy, s, fill);
}

/* ====================================================================== */
/* 7x7 pixel glyphs (cursors)                                               */
static void glyph7_draw(int x, int y, const char *const rows[7], const uint8_t pal[5])
{
    int r, c;
    ur_begin();
    for (r = 0; r < 7; r++) {
        for (c = 0; c < 7 && rows[r][c]; c++) {
            int col;
            switch (rows[r][c]) {
            case 'o': col = pal[0]; break;
            case 's': col = pal[1]; break;
            case 'b': col = pal[2]; break;
            case 'l': col = pal[3]; break;
            case 'w': col = pal[4]; break;
            default: continue;
            }
            ur_run(y + r, x + c, 1, (uint8_t)col);
        }
    }
    ur_end();
}

void ui_glyph7(int x, int y, const char *const rows[7], const uint8_t pal[5])
{
    if (plat_draw_skipped()) return;
    uint32_t k1 = (uint32_t)pal[0] | (uint32_t)pal[1] << 8 | (uint32_t)pal[2] << 16 |
                  (uint32_t)pal[3] << 24;
    uint32_t k2 = (uint32_t)(uintptr_t)rows ^ ((uint32_t)pal[4] << 24);
    if (ur_cache_draw(K_GLYPH, k1, k2, x, y)) return;
    ur_cache_begin(K_GLYPH, k1, k2, x, y);
    glyph7_draw(x, y, rows, pal);
    if (!ur_cache_end()) glyph7_draw(x, y, rows, pal);
}

/* ====================================================================== */
/* big outlined text                                                        */
/* ui_text_big draws, in this order: the glyphs scaled `scale` times at
 * (+1, +3) in the shadow colour, at the eight 1-pixel neighbours and at
 * (0, +1) in the outline colour, at (0, 0) in the text colour, and the
 * upper part of the strokes of glyph rows 0..2 in the light colour. The
 * final pixel is therefore: light > text > outline (3x3 dilation of the
 * glyphs) > shadow. The text block is built row by row as bit rows (one
 * glyph row at a time, a rolling window of four), classified with word
 * operations and emitted as merged runs. */
#define TW 10                            /* words per text row: 320 px     */
#define TX_LINES 4

typedef struct {
    const char *line[TX_LINES];
    int len[TX_LINES];
    int nlines, scale, x, y, nw, bw, bh;
} TextBlock;

/* glyph bits of block row q (block origin = text origin - (1, 1)) */
static void text_grow(const TextBlock *tb, int q, uint32_t *o)
{
    int rel = q - 1, line, r, i, s = tb->scale;
    memset(o, 0, sizeof(uint32_t) * TW);
    if (rel < 0) return;
    line = rel / (FONT_LINE * s);                  /* lines are FONT_LINE apart */
    r = (rel - line * FONT_LINE * s) / s;          /* glyph row; 8.. = gap     */
    if (line >= tb->nlines || r >= 8) return;
    for (i = 0; i < tb->len[line]; i++) {
        unsigned ch = (unsigned char)tb->line[line][i];
        int col, x0 = 1 + i * FONT_ADVANCE * s;
        if (ch < FONT_FIRST || ch > FONT_LAST) ch = '?';
        for (col = 0; col < 5; col++) {
            int a, b;
            if (!((doomo_font[ch - FONT_FIRST][col] >> r) & 1)) continue;
            a = col;
            while (col + 1 < 5 && ((doomo_font[ch - FONT_FIRST][col + 1] >> r) & 1)) col++;
            a = x0 + a * s;
            b = x0 + (col + 1) * s - 1;
            {
                int w0 = a >> 5, w1 = b >> 5;
                if (w0 == w1) {
                    o[w0] |= (~0u << (a & 31)) & (~0u >> (31 - (b & 31)));
                } else {
                    int k;
                    o[w0] |= ~0u << (a & 31);
                    for (k = w0 + 1; k < w1; k++) o[k] = ~0u;
                    o[w1] |= ~0u >> (31 - (b & 31));
                }
            }
        }
    }
}

static void tw_up1(uint32_t *o, const uint32_t *a, int nw)   /* o[x] = a[x-1] */
{
    int k;
    for (k = nw - 1; k > 0; k--) o[k] = (a[k] << 1) | (a[k - 1] >> 31);
    o[0] = a[0] << 1;
}

static void tw_dn1(uint32_t *o, const uint32_t *a, int nw)   /* o[x] = a[x+1] */
{
    int k;
    for (k = 0; k < nw - 1; k++) o[k] = (a[k] >> 1) | (a[k + 1] << 31);
    o[nw - 1] = a[nw - 1] >> 1;
}

static void text_emit(const TextBlock *tb, const uint8_t col[4])
{
    /* col: light, text, outline, shadow. ring[q & 7] = glyph bits of
     * block row q (rows q - 3 .. q + 1 are live). */
    static uint32_t ring[8][TW];
    int q, k, nw = tb->nw, s = tb->scale, ox = tb->x - 1, oy = tb->y - 1;
    int top_h = DOOMO_MAX(s / 2, 1), sh = plat_screen_h();
    memset(ring, 0, sizeof(ring));
    text_grow(tb, 0, ring[0]);
    ur_begin();
    for (q = 0; q < tb->bh; q++) {
        const uint32_t *g = ring[q & 7], *gu = ring[(q - 1) & 7], *gs = ring[(q - 3) & 7];
        uint32_t *gd = ring[(q + 1) & 7];
        uint32_t dil[TW], t1[TW], t2[TW], shd[TW];
        uint32_t pt[TW], pc[TW], pol[TW], psh[TW], ch[TW];
        int rel = q - 1, is_top = 0, run_x = 0, run_c = -1;
        uint32_t any = 0;
        text_grow(tb, q + 1, gd);
        if (oy + q < 0 || oy + q >= sh) { rec_clipped(); continue; }
        for (k = 0; k < nw; k++) {
            dil[k] = gu[k] | g[k] | gd[k];
            any |= dil[k] | gs[k];
        }
        if (!any) continue;
        tw_up1(t1, dil, nw);
        tw_dn1(t2, dil, nw);
        tw_up1(shd, gs, nw);
        if (rel >= 0) {
            int in_line = rel % (FONT_LINE * s), r = in_line / s;
            is_top = r < 3 && in_line - r * s < top_h;
        }
        for (k = 0; k < nw; k++) {
            uint32_t d = dil[k] | t1[k] | t2[k];
            pt[k] = is_top ? g[k] : 0;
            pc[k] = g[k] & ~pt[k];
            pol[k] = d & ~g[k];
            psh[k] = shd[k] & ~d;
        }
        /* class boundaries */
        tw_up1(t1, pt, nw);
        for (k = 0; k < nw; k++) ch[k] = pt[k] ^ t1[k];
        tw_up1(t1, pc, nw);
        for (k = 0; k < nw; k++) ch[k] |= pc[k] ^ t1[k];
        tw_up1(t1, pol, nw);
        for (k = 0; k < nw; k++) ch[k] |= pol[k] ^ t1[k];
        tw_up1(t1, psh, nw);
        for (k = 0; k < nw; k++) ch[k] |= psh[k] ^ t1[k];
        for (k = 0; k < nw; k++) {
            uint32_t v = ch[k];
            while (v) {
                int b = LOWBIT(v), x = (k << 5) + b, nc;
                uint32_t bit = 1u << b;
                v &= v - 1;
                if (run_c >= 0) ur_run(oy + q, ox + run_x, x - run_x, (uint8_t)run_c);
                if (pt[k] & bit) nc = col[0];
                else if (pc[k] & bit) nc = col[1];
                else if (pol[k] & bit) nc = col[2];
                else if (psh[k] & bit) nc = col[3];
                else nc = -1;
                run_c = nc;
                run_x = x;
            }
        }
    }
    ur_end();
}

static uint32_t str_hash(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

/* Returns 0 if the text does not fit the bit-row path (then the caller
 * draws it the slow way). */
int ur_text_big(int x, int y, const char *s, int scale, uint8_t color, uint8_t outline)
{
    TextBlock tb;
    uint8_t col[4];
    const char *p = s;
    int maxlen = 0;
    uint32_t k0, k1, k2;
    if (plat_draw_skipped()) return 1;
    tb.nlines = 0;
    for (;;) {
        const char *e = p;
        while (*e && *e != '\n') e++;
        if (tb.nlines == TX_LINES) return 0;
        tb.line[tb.nlines] = p;
        tb.len[tb.nlines] = (int)(e - p);
        if (tb.len[tb.nlines] > maxlen) maxlen = tb.len[tb.nlines];
        tb.nlines++;
        if (!*e) break;
        p = e + 1;
    }
    tb.scale = scale;
    tb.x = x;
    tb.y = y;
    tb.bw = maxlen * FONT_ADVANCE * scale + 3;
    tb.bh = (tb.nlines - 1) * FONT_LINE * scale + 8 * scale + 4;
    tb.nw = (tb.bw + 31) >> 5;
    if (tb.nw > TW) return 0;
    col[0] = ui_lighter(color);
    col[1] = color;
    col[2] = outline;
    col[3] = ui_darker(outline);
    k0 = K_TEXT | (uint32_t)scale << 8 | (uint32_t)color << 16 | (uint32_t)outline << 24;
    k1 = str_hash(s);
    k2 = (uint32_t)(p - s) + (uint32_t)maxlen * 65536u;
    if (tb.bw > 256 || tb.bh > 256) {        /* too big for the cache */
        text_emit(&tb, col);
        return 1;
    }
    if (ur_cache_draw(k0, k1, k2, x - 1, y - 1)) return 1;
    ur_cache_begin(k0, k1, k2, x - 1, y - 1);
    text_emit(&tb, col);
    if (!ur_cache_end()) text_emit(&tb, col);
    return 1;
}
