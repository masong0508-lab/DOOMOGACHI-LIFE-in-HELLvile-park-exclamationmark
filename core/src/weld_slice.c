/* weld_slice.c - the first-person weapon picture composed in slices (S2-9;
 * owner: action; weld.h weld_view_image_sliced / weld_view_prefetch).
 *
 * weld_gfx.c composes a 96x64 picture in one go: 220 ms of one tick on the
 * GBA, 100 ms on the PS1. Fine where a picture is made once (a weapon
 * switch in combat), not in Rivet's workshop, which shows a new one with
 * every part the player tries and three a cycle for the test fire.
 *
 * This file compiles weld_gfx.c's recipes a second time (WELD_SLICED) on
 * primitives of its own:
 *
 *   - Units. A composition writes its pixels in units, always in the same
 *     order: a lone pixel (decals, lines, rivets), or a filled primitive
 *     (seg, ell, tri, each scan of the outline, the canvas clear) row by
 *     row. A pass (job_slice) spends a number of rows' worth of pixels,
 *     starting at unit sl.at (its row sl.row), and notes where it stopped.
 *     The units before sl.at are skipped without their geometry. So the
 *     passes together do what one pass without a limit does: every write of
 *     the composition, once, in its order. Nothing a unit computes depends
 *     on the canvas except put_over() (one unit: its read and its write) and
 *     the outline scans (row by row, in the plain scan's order).
 *   - Rows. The filled primitives' rows are written for speed (on the GBA
 *     they are ARM code in the IWRAM overlay's UI group): numerators
 *     stepped instead of multiplied, the two divisions of a seg pixel by a
 *     reciprocal, tones compared by multiplication, a row cut to the run
 *     its pixels can lie in. Every shortcut is exact: the pixels are the
 *     plain build's (core/tests/test_weld_slice.c compares the two builds
 *     pixel by pixel over thousands of weapons, sliced at every size).
 *
 * The cache is weld_gfx.c's (weld_gfx_int.h): a finished picture is the
 * same whichever build made it. One picture is under way at a time (the
 * job), in a slot other than the one on show; its key stays 0 until the
 * last slice, so nothing finds it half drawn.
 *
 * When slices run:
 *   GBA     in the idle time of a frame: one pass that goes on until just
 *           before the vblank the frame waits for (gba/src/pslice.c,
 *           weld_idle_next / weld_idle_run: a pass costs ~0.9 ms before its
 *           first unit, so it is one long one, not many short ones); inside
 *           a frame only when the job got no slice for 4 ticks
 *   PS1     inside the frame, a slice at a time while the platform has
 *           room for one more (ps1/src/pslice.c, plat_portrait_slice_ok)
 *   others  none: the picture is composed in the call that asks for it, as
 *           weld_view_image() does (and nothing is composed ahead)
 */
#define WELD_SLICED 1
#include "weld_gfx.c"                   /* Mat, G, at(), the recipes, compose(), weapon_key() */
#include "doomo/game.h"
#include "doomo/plat_ext.h"

/* ---- where the row loops live -------------------------------------------- */
#if defined(DOOMO_PLATFORM_GBA)
/* ARM code in the IWRAM overlay's UI group (gba/src/iwram_ovl.h: IWRAM_UI_CODE). gba_weld_rows_in() brings the
 * group in and says what was there; gba_weld_rows_out() puts that back (gba/src/pslice.c). */
#define WELD_ROWS __attribute__((section(".iwram_ui"), target("arm"), long_call, noinline))
extern int  gba_weld_rows_in(void);
extern void gba_weld_rows_out(int had);
/* cycles of this tick's work so far (gba/src/main.c): a pass can be told to stop at a time (sl.until) */
extern uint32_t gba_work_now(void);
#define SL_TIMED 1
#else
#define WELD_ROWS
#define gba_weld_rows_in() 0
#define gba_weld_rows_out(had) ((void)(had))
#define gba_work_now() 0u
#define SL_TIMED 0
#endif
/* a helper of the row loops: always part of the loop that uses it */
#define WELD_ROWS_INL static inline __attribute__((always_inline))

/* word reads of the canvas (it is 4-aligned and its rows are whole words) */
typedef uint32_t __attribute__((may_alias)) Word;
DOOMO_STATIC_ASSERT((VW & 3) == 0, weld_view_rows_are_whole_words);

/* ---- units ------------------------------------------------------------------ */
#define SL_TOP (-1)                     /* sl.row: from the unit's first row */
#define SL_ALL 0x3FFFFFFF               /* a pass without a limit */
#define SL_LOOK 96                      /* a timed pass looks at the clock every so many units' cost */
typedef struct Sl {
    int32_t  left;                      /* what this pass may still spend (<= 0: it stops at the next unit or row) */
    uint32_t until;                     /* (SL_TIMED) a timed pass: it stops once gba_work_now() is past this; 0: not timed */
    uint16_t n;                         /* units met so far in this pass */
    uint16_t at;                        /* the first unit that is not finished */
    uint16_t cur;                       /* the filled primitive whose rows run now */
    int16_t  row;                       /* unit `at`: its first row not written yet, or SL_TOP */
    int16_t  look;                      /* (SL_TIMED) cost left until the next look at the clock */
    uint8_t  stopped;                   /* this pass ran out: (at, row) is where the next one goes on */
    uint8_t  box[4];                    /* the outline scans' box: x0, y0, x1, y1 (taken when the first scan starts) */
} Sl;                                   /* (it is g->sl: the state is not a static, see ws() below) */

/* spend `cost` of the pass: 0 = it is over (nothing was spent) */
#define SL_SPEND(sl, cost) \
    do { \
        if ((sl)->left <= 0) return 0; \
        if (SL_TIMED && (sl)->until && ((sl)->look -= (int16_t)(cost)) <= 0) { \
            (sl)->look = SL_LOOK; \
            if (gba_work_now() >= (sl)->until) { (sl)->left = 0; return 0; } \
        } \
        (sl)->left -= (cost); \
        return 1; \
    } while (0)
static int sl_spend1(Sl *sl) { SL_SPEND(sl, 1); }

/* a lone pixel: 1 = write it now */
static int sl_one(Sl *sl)
{
    unsigned k = sl->n++;
    if (sl->stopped || k < sl->at) return 0;
    if (!sl_spend1(sl)) {
        sl->stopped = 1;
        sl->at = (uint16_t)k;
        sl->row = SL_TOP;
        return 0;
    }
    return 1;
}

/* a filled primitive: -2 = nothing of it in this pass (done before, or the pass is over); SL_TOP = from its
 * first row; else the row to go on with. Called before its geometry, which a finished unit never needs. */
static int sl_fill(Sl *sl)
{
    unsigned k = sl->n++;
    if (sl->stopped || k < sl->at) return -2;
    sl->cur = (uint16_t)k;
    return k == sl->at ? sl->row : SL_TOP;
}

/* before row y of the filled primitive sl->cur, which costs `cost` (<= 255): 0 = the pass stops here */
WELD_ROWS_INL int sl_spend(Sl *sl, int cost) { SL_SPEND(sl, cost); }
WELD_ROWS_INL int sl_row(Sl *sl, int y, int cost)
{
    if (sl_spend(sl, cost)) return 1;
    sl->stopped = 1;
    sl->at = sl->cur;
    sl->row = (int16_t)y;
    return 0;
}

/* ---- lone pixels -------------------------------------------------------------- */
static void put(G *g, int x, int y, uint8_t c)
{
    if (!sl_one(g->sl)) return;
    if (c == PAL_BLACK) c = PAL_RAMP(RAMP_STONE, 0);       /* 1 is the outline marker */
    if ((unsigned)x < (unsigned)g->w && (unsigned)y < (unsigned)g->h && c) g->px[y * g->w + x] = c;
}

/* a pixel of the picture so far in another colour (nothing where it is empty): its read and its write are one
 * unit, so the read always sees every earlier unit and nothing else */
static void put_over(G *g, int x, int y, uint8_t c)
{
    if (!sl_one(g->sl)) return;
    if (!get(g, x, y)) return;
    if (c == PAL_BLACK) c = PAL_RAMP(RAMP_STONE, 0);
    if (c) g->px[y * g->w + x] = c;                        /* (get() said it is on the canvas) */
}

static void clear_px(G *g, int x, int y)
{
    if (!sl_one(g->sl)) return;
    if ((unsigned)x < (unsigned)g->w && (unsigned)y < (unsigned)g->h) g->px[y * g->w + x] = 0;
}

static void canvas_clear(G *g, uint8_t *px, int n)
{
    if (!sl_one(g->sl)) return;
    g->sl->left -= n / 64;
    memset(px, 0, (size_t)n);
}

/* a material's five tones as put() writes them (0: nothing) */
static void put_tones(const Mat *m, uint8_t *cm)
{
    int i;
    for (i = 0; i < 5; i++) cm[i] = m->c[i] == PAL_BLACK ? (uint8_t)PAL_RAMP(RAMP_STONE, 0) : m->c[i];
}

/* ---- exact helpers of the rows ---------------------------------------------------- */
/* n / d for 0 <= n <= lim by a reciprocal (the estimate is never low and at most a few high: the loop lands on
 * the quotient); anything else divides */
typedef struct Rcp { uint32_t d, rec, lim; } Rcp;
static void rcp_init(Rcp *r, int d)
{
    r->d = (uint32_t)(d > 0 ? d : 1);
    r->rec = (1u << 20) / r->d + 1u;
    r->lim = 0xFFFFFFFFu / r->rec;
}
WELD_ROWS_INL int rcp_div(const Rcp *r, int n)
{
    uint32_t q, un = (uint32_t)n;
    if (n < 0 || un > r->lim) return n / (int)r->d;
    q = (un * r->rec) >> 20;
    while (q * r->d > un) q--;
    return (int)q;
}

/* fx_isqrt()'s bit loop (the rows call nothing outside their section) */
WELD_ROWS_INL uint32_t isqrt_bits(uint32_t v)
{
    uint32_t res = 0, bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= res + bit) {
            v -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

/* isqrt_i(v), stepped from the root found last: next to each other along a row the roots differ by little */
WELD_ROWS_INL int isqrt_near(int v, int *last)
{
    int r = *last;
    if (v <= 0) return 0;
    if (r * r > v) {
        if (r < 4 || (r - 4) * (r - 4) > v) r = (int)isqrt_bits((uint32_t)v);
        else while (r * r > v) r--;
    } else if ((r + 4) * (r + 4) <= v) {
        r = (int)isqrt_bits((uint32_t)v);
    } else {
        while ((r + 1) * (r + 1) <= v) r++;
    }
    *last = r;
    return r;
}

/* floor(v / d) for 0 <= v < 2^31 from rec = 0xFFFFFFFF / d: the quotient or one less */
WELD_ROWS_INL int32_t quot_est(int32_t v, uint32_t rec)
{
    return (int32_t)(((uint64_t)(uint32_t)v * rec) >> 32);
}

/* The steps k in [*k0, *k1] at which lo < n0 + d * k < hi can hold: cut to a run that holds them all (it may hold
 * a few more: the caller still tests every pixel). rec = 0xFFFFFFFF / |d| (d != 0). */
WELD_ROWS_INL void run_cut(int32_t n0, int32_t d, uint32_t rec, int32_t lo, int32_t hi, int *k0, int *k1)
{
    int32_t a, b;
    if (d == 0) {
        if (n0 <= lo || n0 >= hi) *k1 = *k0 - 1;
        return;
    }
    if (d < 0) { int32_t t = -hi; n0 = -n0; hi = -lo; lo = t; }     /* -hi < -n0 + |d| k < -lo */
    a = lo - n0;                        /* |d| k > a */
    b = hi - n0;                        /* |d| k < b */
    if (b <= 0) { *k1 = *k0 - 1; return; }
    if (a > 0) {
        int32_t q = quot_est(a, rec) - 1;
        if (q > *k0) *k0 = (int)q;
    }
    {
        int32_t q = quot_est(b, rec) + 2;
        if (q < *k1) *k1 = (int)q;
    }
}

/* ---- seg ---------------------------------------------------------------------------- */
/* A segment along the axis from t0 to t1, centred s px to the side, half widths w0 -> w1 (px x16). weld_gfx.c's
 * seg() per pixel, with: the along / side numerators stepped; t = al * 256 / len and the width's interpolation by
 * a reciprocal; u = (sd - sc) * 256 / hw never formed, only compared (tone index below); a row cut to where
 * al can lie in [lt0, lt1] and sd in the band a pixel can be in at all. */
typedef struct SegRun {
    uint8_t *px;
    int w, minx, maxx, maxy;
    int ax, ay, ux, uy, vx, vy, persp, wsc;
    int lt0, lt1, w0, dw, adw, s16, sdlo, sdhi, shade, caps;
    Sl *sl;
    uint32_t reca, recs;                /* 0xFFFFFFFF / |16 ux|, / |16 vx| */
    Rcp rlen, rdl;
    uint8_t cm[5];
} SegRun;

/* the tone index of u = n / d (d > 0; truncated as C does) without the division:
 * for n >= 0, u < T exactly when n < T d; for n < 0 (u = -floor(-n / d)), u < T exactly when -n >= (1 - T) d */
WELD_ROWS_INL int tone_cyl_i(int n, int d)
{
    if (n >= 0) return n < 90 * d ? 2 : n < 190 * d ? 1 : 0;
    n = -n;
    return n >= 201 * d ? 2 : n >= 131 * d ? 4 : n >= 41 * d ? 3 : 2;
}

WELD_ROWS_INL int tone_box_i(int n, int d, int near_end, int far_end)
{
    if (n >= 151 * d) return n >= 226 * d ? 0 : 1;             /* u > 150: u > 225 */
    if (far_end) return 3;
    if (near_end) return 1;
    if (n >= 0) return 2;
    n = -n;
    return n >= 206 * d ? 2 : n >= 151 * d ? 4 : n >= 61 * d ? 3 : 2;
}

WELD_ROWS_INL int tone_glow_i(int n, int d)
{
    if (n < 0) n = -n;
    return n < 70 * d ? 4 : n < 150 * d ? 3 : n < 220 * d ? 2 : 1;
}

static WELD_ROWS void seg_rows(const SegRun *r, int y)
{
    int rnear = 0, rfar = 0;
    const int width = r->maxx - r->minx + 1, da = 16 * r->ux, ds = 16 * r->vx;
    /* the numerators at the row's first pixel (x16 x256) */
    int32_t ry = y * 16 + 8 - r->ay, rx0 = r->minx * 16 + 8 - r->ax;
    int32_t na0 = rx0 * r->ux + ry * r->uy, ns0 = rx0 * r->vx + ry * r->vy;
    for (; y <= r->maxy; y++, na0 += 16 * r->uy, ns0 += 16 * r->vy) {
        uint8_t *row = r->px + y * r->w + r->minx;
        int32_t na, ns;
        int k, k0 = 0, k1 = width - 1;
        if (!sl_row(r->sl, y, width)) return;
        /* al = na / 256 truncates, so al in [lt0, lt1] needs 256 (lt0 - 1) < na < 256 (lt1 + 1); sd alike */
        run_cut(na0, da, r->reca, 256 * (r->lt0 - 1), 256 * (r->lt1 + 1), &k0, &k1);
        run_cut(ns0, ds, r->recs, 256 * (r->sdlo - 1), 256 * (r->sdhi + 1), &k0, &k1);
        for (k = k0, na = na0 + da * k0, ns = ns0 + ds * k0; k <= k1; k++, na += da, ns += ds) {
            int al = na / 256;                              /* along, x16 */
            int sd = ns / 256;                              /* side, x16 */
            int t, ps, hw, sc, e0, e1, iw, n, i;
            if (al < r->lt0 || al > r->lt1 || sd < r->sdlo || sd > r->sdhi) continue;
            t = rcp_div(&r->rlen, al * 256);                /* al * 256 / len */
            ps = (256 - r->persp * t / 256) * r->wsc / 256; /* pscale() */
            iw = r->adw ? rcp_div(&r->rdl, r->adw * (al - r->lt0)) : 0;     /* |w1 - w0| (al - lt0) / dl */
            hw = (r->w0 + (r->dw < 0 ? -iw : iw)) * ps / 256;
            sc = r->s16 * ps / 256;
            if (hw < 6) hw = 6;
            /* rounded caps */
            e0 = al - r->lt0;
            e1 = r->lt1 - al;
            if ((r->caps & CAP_NEAR) && e0 < hw) hw = isqrt_near(hw * hw - (hw - e0) * (hw - e0), &rnear);
            if ((r->caps & CAP_FAR) && e1 < hw) hw = isqrt_near(hw * hw - (hw - e1) * (hw - e1), &rfar);
            if (sd < sc - hw || sd > sc + hw || hw <= 0) continue;
            n = (sd - sc) * 256;
            switch (r->shade) {
            case SH_CYL: i = tone_cyl_i(n, hw); break;
            case SH_BOX: i = tone_box_i(n, hw, e0 < 16, e1 < 16); break;
            case SH_GLOW: i = tone_glow_i(n, hw); break;
            default: i = 2; break;
            }
            if (r->cm[i]) row[k] = r->cm[i];
        }
    }
}

static void seg(G *g, int t0, int t1, int s16, int w0, int w1, const Mat *m, int shade, int caps)
{
    int x0, y0, x1, y1, x2, y2, x3, y3, minx, maxx, miny, maxy, ya;
    int lt0, lt1, wmax, ps0, ps1, sc0, sc1, hwmax, dl;
    SegRun r;
    if (t1 <= t0) return;
    ya = sl_fill(g->sl);
    if (ya == -2) return;
    lt0 = g->len * t0 / 256;
    lt1 = g->len * t1 / 256;
    wmax = DOOMO_MAX(w0, w1);
    at(g, t0, s16 / 16 - wmax / 16 - 1, &x0, &y0);
    at(g, t0, s16 / 16 + wmax / 16 + 1, &x1, &y1);
    at(g, t1, s16 / 16 - wmax / 16 - 1, &x2, &y2);
    at(g, t1, s16 / 16 + wmax / 16 + 1, &x3, &y3);
    minx = DOOMO_MIN(DOOMO_MIN(x0, x1), DOOMO_MIN(x2, x3)) - 1;
    maxx = DOOMO_MAX(DOOMO_MAX(x0, x1), DOOMO_MAX(x2, x3)) + 1;
    miny = DOOMO_MIN(DOOMO_MIN(y0, y1), DOOMO_MIN(y2, y3)) - 1;
    maxy = DOOMO_MAX(DOOMO_MAX(y0, y1), DOOMO_MAX(y2, y3)) + 1;
    minx = DOOMO_MAX(minx, 0); miny = DOOMO_MAX(miny, 0);
    maxx = DOOMO_MIN(maxx, g->w - 1); maxy = DOOMO_MIN(maxy, g->h - 1);
    if (maxx < minx) return;
    /* The side band a pixel can be in at all. t, the perspective scale ps and the centre sc are monotone along
     * the axis (a truncating division keeps that), so their ranges come from the two ends; the half width is
     * at most the larger width times the larger scale (+1 for the truncations), at least 6, and only shrinks
     * under the caps. A pixel outside the band fails the side test of the plain row. */
    ps0 = pscale(g, lt0 * 256 / g->len);
    ps1 = pscale(g, lt1 * 256 / g->len);
    sc0 = s16 * ps0 / 256;
    sc1 = s16 * ps1 / 256;
    hwmax = DOOMO_MAX(w0 < 0 ? -w0 : w0, w1 < 0 ? -w1 : w1) * DOOMO_MAX(ps0 < 0 ? -ps0 : ps0, ps1 < 0 ? -ps1 : ps1) / 256 + 1;
    if (hwmax < 6) hwmax = 6;
    dl = DOOMO_MAX(lt1 - lt0, 1);
    r.px = g->px; r.w = g->w; r.sl = g->sl;
    r.minx = minx; r.maxx = maxx; r.maxy = maxy;
    r.ax = g->ax; r.ay = g->ay; r.ux = g->ux; r.uy = g->uy; r.vx = g->vx; r.vy = g->vy;
    r.persp = g->persp; r.wsc = g->wsc;
    r.lt0 = lt0; r.lt1 = lt1;
    r.w0 = w0; r.dw = w1 - w0; r.adw = r.dw < 0 ? -r.dw : r.dw;
    r.s16 = s16;
    r.sdlo = DOOMO_MIN(sc0, sc1) - hwmax;
    r.sdhi = DOOMO_MAX(sc0, sc1) + hwmax;
    r.shade = shade; r.caps = caps;
    r.reca = g->ux ? 0xFFFFFFFFu / (uint32_t)(16 * (g->ux < 0 ? -g->ux : g->ux)) : 0;
    r.recs = g->vx ? 0xFFFFFFFFu / (uint32_t)(16 * (g->vx < 0 ? -g->vx : g->vx)) : 0;
    rcp_init(&r.rlen, g->len);
    rcp_init(&r.rdl, dl);
    put_tones(m, r.cm);
    seg_rows(&r, ya == SL_TOP ? miny : ya);
}

/* ---- ell ------------------------------------------------------------------------------ */
/* Shaded ellipse in screen space (centre x16, radii x16), lit top-left: weld_gfx.c's ell() with each column's dx
 * and each row's dy divided once (dxs / dys, made by ell()), only on the canvas (put() drops the rest), and the
 * glow's tone from d2 itself: isqrt(d2) < a exactly when d2 < a a. */
static WELD_ROWS void ell_rows(Sl *sl, uint8_t *px, int w, int x0, int y, int x1, int y1, const int16_t *dxs,
                               const int16_t *dys, const uint8_t *cm, int glow)
{
    for (; y <= y1; y++, dys++) {
        uint8_t *row = px + y * w + x0;
        int dy = *dys, k, n = x1 - x0;
        if (!sl_row(sl, y, n + 1)) return;
        if (dy * dy > 65536) continue;
        for (k = 0; k <= n; k++) {
            int dx = dxs[k], d2 = dx * dx + dy * dy, l, i;
            if (d2 > 65536) continue;
            if (glow) {
                i = d2 < 70 * 70 ? 4 : d2 < 150 * 150 ? 3 : d2 < 220 * 220 ? 2 : 1;
            } else {
                l = -dx - dy;                              /* light from the upper left */
                if (d2 > 52000 && l < 0) i = 0;
                else if (l > 200 && d2 < 30000 && d2 > 4000) i = 4;
                else if (l > 60) i = 3;
                else if (l > -150) i = 2;
                else i = 1;
            }
            if (cm[i]) row[k] = cm[i];
        }
    }
}

static void ell(G *g, int cx16, int cy16, int rx16, int ry16, const Mat *m, int glow)
{
    int x0 = (cx16 - rx16) >> 4, x1 = (cx16 + rx16) >> 4, y0 = (cy16 - ry16) >> 4, y1 = (cy16 + ry16) >> 4;
    int16_t dxs[VW], dys[VH];
    uint8_t cm[5];
    int x, y, ya;
    if (rx16 < 8 || ry16 < 8) return;
    ya = sl_fill(g->sl);
    if (ya == -2) return;
    x0 = DOOMO_MAX(x0, 0); x1 = DOOMO_MIN(x1, g->w - 1);
    y0 = DOOMO_MAX(y0, 0); y1 = DOOMO_MIN(y1, g->h - 1);
    if (x1 < x0 || y1 < y0 || x1 - x0 >= VW || y1 - y0 >= VH) return;
    if (ya != SL_TOP) y0 = ya;
    /* (|dx|, |dy| <= 257 + 2048 / r: inside the ellipse's box a pixel centre is at most r + 8 from the centre) */
    for (x = x0; x <= x1; x++) dxs[x - x0] = (int16_t)((x * 16 + 8 - cx16) * 256 / rx16);
    for (y = y0; y <= y1; y++) dys[y - y0] = (int16_t)((y * 16 + 8 - cy16) * 256 / ry16);
    put_tones(m, cm);
    ell_rows(g->sl, g->px, g->w, x0, y0, x1, y1, dxs, dys, cm, glow);
}

/* ---- tri -------------------------------------------------------------------------------- */
/* weld_gfx.c's tri() on the canvas only, its three edge values stepped along the row */
static void tri(G *g, int ax, int ay, int bx, int by, int cx, int cy, uint8_t c)
{
    int minx = DOOMO_MIN(ax, DOOMO_MIN(bx, cx)), maxx = DOOMO_MAX(ax, DOOMO_MAX(bx, cx));
    int miny = DOOMO_MIN(ay, DOOMO_MIN(by, cy)), maxy = DOOMO_MAX(ay, DOOMO_MAX(by, cy));
    int x, y, ya = sl_fill(g->sl);
    if (ya == -2) return;
    minx = DOOMO_MAX(minx, 0); maxx = DOOMO_MIN(maxx, g->w - 1);
    miny = DOOMO_MAX(miny, 0); maxy = DOOMO_MIN(maxy, g->h - 1);
    if (maxx < minx) return;
    if (c == PAL_BLACK) c = PAL_RAMP(RAMP_STONE, 0);
    for (y = ya == SL_TOP ? miny : ya; y <= maxy; y++) {
        int e0 = (bx - ax) * (y - ay) - (by - ay) * (minx - ax);
        int e1 = (cx - bx) * (y - by) - (cy - by) * (minx - bx);
        int e2 = (ax - cx) * (y - cy) - (ay - cy) * (minx - cx);
        uint8_t *row = g->px + y * g->w;
        if (!sl_row(g->sl, y, maxx - minx + 1)) return;
        if (!c) continue;
        for (x = minx; x <= maxx; x++, e0 -= by - ay, e1 -= cy - by, e2 -= ay - cy)
            if ((e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0)) row[x] = c;
    }
}

/* ---- the outline pass --------------------------------------------------------------------- */
/* weld_gfx.c's outline(): scan 1 marks an empty pixel (with 1, never a colour of the composition) when one of its
 * four neighbours is solid (neither empty nor a mark), whatever the order; scan 2 gives a marked pixel a dark
 * shade of its first solid neighbour right, down, left, up (left and up may be outline already), or empties it.
 * Here both scans read the canvas directly and look only inside the box of the non-empty pixels grown by one
 * (taken once, when scan 1 starts: every pixel either scan can change is in it), four pixels at a time where a
 * word shows there is nothing to do. */
#define OUTLINE_ROW(w) ((w) / 8)        /* a scan row's cost (a filled row costs its width) */
#define SOLID(v) ((uint8_t)((v) - 2) < 254)
#define HAS_ZERO(v) (((v) - 0x01010101u) & ~(v) & 0x80808080u)     /* a zero byte in the word */

/* the box of the non-empty pixels of a w x h canvas (word-aligned, w a multiple of 4), grown by one and clipped,
 * its columns rounded out to whole words; an empty canvas gives y0 > y1 */
static WELD_ROWS void outline_box(const uint8_t *p, int w, int h, uint8_t *box)
{
    int y, x0 = w, x1 = -1, y0 = h, y1 = -1;
    for (y = 0; y < h; y++, p += w) {
        const Word *r = (const Word *)(const void *)p;
        int a = 0, b = w / 4 - 1;
        while (a <= b && !r[a]) a++;
        if (a > b) continue;
        while (!r[b]) b--;
        if (y < y0) y0 = y;
        y1 = y;
        if (a * 4 < x0) x0 = a * 4;
        if (b * 4 + 3 > x1) x1 = b * 4 + 3;
    }
    if (y1 < 0) { box[0] = 0; box[1] = 1; box[2] = 0; box[3] = 0; return; }
    x0 -= 4; x1 += 4; y0--; y1++;
    box[0] = (uint8_t)(x0 < 0 ? 0 : x0);
    box[2] = (uint8_t)(x1 >= w ? w - 1 : x1);
    box[1] = (uint8_t)(y0 < 0 ? 0 : y0);
    box[3] = (uint8_t)(y1 >= h ? h - 1 : y1);
}

static WELD_ROWS void outline_mark_rows(Sl *sl, uint8_t *p, int w, int h, int y, const uint8_t *box)
{
    const int bx0 = box[0], by0 = box[1], bx1 = box[2], by1 = box[3];
    for (; y < h; y++) {
        uint8_t *r = p + y * w;
        int x;
        if (!sl_row(sl, y, OUTLINE_ROW(w))) return;
        if (y < by0 || y > by1) continue;
        for (x = bx0; x <= bx1; x += 4) {
            Word v = *(const Word *)(const void *)(r + x);
            int k;
            if (!HAS_ZERO(v)) continue;                     /* four pixels of the picture */
            if (!v) {
                /* four empty pixels: nothing to mark unless something solid touches them */
                Word nb = 0;
                if (y > 0) nb |= *(const Word *)(const void *)(r + x - w);
                if (y + 1 < h) nb |= *(const Word *)(const void *)(r + x + w);
                if (!(nb & 0xFEFEFEFEu) && !(x > 0 && SOLID(r[x - 1])) && !(x + 4 < w && SOLID(r[x + 4]))) continue;
            }
            for (k = x; k < x + 4; k++) {
                if (r[k]) continue;
                if ((k + 1 < w && SOLID(r[k + 1])) || (y + 1 < h && SOLID(r[k + w])) ||
                    (k > 0 && SOLID(r[k - 1])) || (y > 0 && SOLID(r[k - w])))
                    r[k] = 1;
            }
        }
    }
}

static WELD_ROWS void outline_ink_rows(Sl *sl, uint8_t *p, int w, int h, int y, const uint8_t *box,
                                       const uint8_t *darker)
{
    const int bx0 = box[0], by0 = box[1], bx1 = box[2], by1 = box[3];
    for (; y < h; y++) {
        uint8_t *r = p + y * w;
        int x;
        if (!sl_row(sl, y, OUTLINE_ROW(w))) return;
        if (y < by0 || y > by1) continue;
        for (x = bx0; x <= bx1; x += 4) {
            Word v = *(const Word *)(const void *)(r + x) ^ 0x01010101u;
            int k;
            if (!HAS_ZERO(v)) continue;                     /* no mark in these four */
            for (k = x; k < x + 4; k++) {
                uint8_t c, d;
                if (r[k] != 1) continue;
                if (k + 1 < w && SOLID(r[k + 1])) c = r[k + 1];
                else if (y + 1 < h && SOLID(r[k + w])) c = r[k + w];
                else if (k > 0 && SOLID(r[k - 1])) c = r[k - 1];
                else if (y > 0 && SOLID(r[k - w])) c = r[k - w];
                else { r[k] = 0; continue; }
                d = darker[darker[c]];
                if (d == c || d == 0 || d == PAL_BLACK) d = PAL_RAMP(RAMP_STONE, 0);
                r[k] = d;
            }
        }
    }
}

/* (the canvas is a view slot: 4-aligned, VW x VH; slot_pix() below) */
static void outline(G *g)
{
    Sl *sl = g->sl;
    int ya = sl_fill(sl);
    if (ya != -2) {
        if (ya == SL_TOP) {
            outline_box(g->px, g->w, g->h, sl->box);
            ya = 0;
        }
        outline_mark_rows(sl, g->px, g->w, g->h, ya, sl->box);
    }
    ya = sl_fill(sl);
    if (ya != -2) outline_ink_rows(sl, g->px, g->w, g->h, ya == SL_TOP ? 0 : ya, sl->box, doomo_art_darker);
}

/* ---- the job: one picture under way --------------------------------------------------------- */
/* The platforms' slice gate (portrait.h plat_portrait_slice_ok / _done) is asked with a class that is none of the
 * portrait cache's: they take it as "never timed" (a cautious fixed cost) and learn nothing, so their tables stay
 * as they are. */
#define SLICE_CLASS PORTRAIT_SLICE_CLASSES
/* a slice inside a frame: rows' worth of pixels (a filled row costs its width, an outline row an eighth, a lone
 * pixel 1); a picture is 4,000-12,000. About 2 ms on the PS1 (PCSX-Redux), 3-5 ms on the GBA */
#define SLICE_UNITS 350
#define PATIENCE 4                      /* WS_IDLE: ticks without a slice before one runs inside a frame */

enum { WS_NOW, WS_IDLE, WS_FRAME };
#if defined(DOOMO_PLATFORM_GBA)
#define ws_mode WS_IDLE
#define ws_units SLICE_UNITS
#define ws_far WELD_VIEW_FAR
#elif defined(DOOMO_PLATFORM_PS1)
#define ws_mode WS_FRAME
#define ws_units SLICE_UNITS
#define ws_far 0
#else
static uint8_t ws_mode = WS_NOW;        /* weld_slice_test() */
static uint8_t ws_far;
static int32_t ws_units = SLICE_UNITS;
#endif

/* Slots 0 .. WELD_VIEW_SLOTS-1 are weld_gfx.c's (weld_gfx_int.h: the scene scratch). Slots WELD_VIEW_SLOTS ..
 * are the far slots, on the GBA only: each is one picture of the portrait cache's block pool (portrait.h
 * portrait_pool_big: 16 of its 48 blocks, least recently used first out with the portraits and the weapon icons;
 * Rivet's workshop on the GBA shows no portrait). With them the three pictures of a test fire stay cached. A far
 * slot is looked for in the pool before every use: when the pool gave its blocks away the slot is empty. */
#define WELD_VIEW_FAR 2
#define WELD_VIEW_ALL (WELD_VIEW_SLOTS + WELD_VIEW_FAR)
#define FAR_POOL_KEY(i) (0x57564600u + (uint32_t)(i))

/* Everything this file remembers. Not a static on the PS1, whose resident RAM has no room (docs/PS1.md): there
 * it lives in the scene scratch the platform lends (plat_ext.h plat_scene_scratch: the overlay slot after the
 * workshop group's code), and starts empty whenever that memory was reused. */
typedef struct Ws {
#if defined(DOOMO_PLATFORM_PS1)
    uint32_t magic, gen;
#endif
    Sl       sl;                        /* where the job's composition stands */
    uint32_t key;                       /* the job: its picture ... */
    uint32_t asked;                     /* ... the tick it was last asked for */
    uint32_t last;                      /* ... the tick of its last slice (or of its start) */
    DoomoWeapon w;
    uint8_t  on, slot, frame;
    uint8_t  wanted;                    /* weld_view_image_sliced() waits for it (a prefetch does not replace it) */
    uint32_t shown_key;                 /* the picture weld_view_image_sliced() returned last ... */
    uint8_t  shown;                     /* ... and its slot + 1, 0: none (still that picture while the keys agree) */
    uint8_t  asked_mask;                /* slots asked for in tick asked_tick: a prefetch leaves them alone */
    uint32_t asked_tick;
    uint32_t gfx_gen;                   /* weld_gfx_gen when all this was made */
#if !defined(DOOMO_PLATFORM_PS1)
    uint32_t far_key[WELD_VIEW_FAR], far_used[WELD_VIEW_FAR];
    const DoomoImage *far_img[WELD_VIEW_FAR];   /* its picture in the pool as found by the last far_sync(), or NULL */
#endif
} Ws;

#if defined(DOOMO_PLATFORM_PS1)
#define WS_MAGIC 0x57534C31u
static Ws *ws(void)
{
    uint32_t gen = 0;
    Ws *s = (Ws *)plat_scene_scratch((uint32_t)sizeof(Ws), &gen);
    if (!s) return NULL;                /* (no room: the callers compose in the call, as weld_view_image() does) */
    if (s->magic != WS_MAGIC || s->gen != gen) {
        memset(s, 0, sizeof(*s));
        s->magic = WS_MAGIC;
        s->gen = gen;
    }
    return s;
}
#else
static Ws ws_state;
#define ws() (&ws_state)
#endif

static const DoomoWeapon view_fists = { WFRAME_FISTS, 0, 0, 0, 0, 0, 0, 0, 0, WSTYLE_CUTE,
                                        PAINT_ROSE, PAINT_WHITE, 50, WF_VALID, 0, 0 };

static uint32_t now(void) { return doomo_frame_count(); }

#if defined(DOOMO_PLATFORM_PS1)
#define far_sync(s) ((void)(s))
#define slot_key(s, i) weld_view_key[i]
#define slot_used(s, i) weld_view_used[i]
#define slot_img(s, i) (&weld_view_img[i])
#define slot_pix(s, i) weld_view_pix[i]
#define slot_touch(s, i) (weld_view_used[i] = weld_view_clock)
#define slot_claim(s, i) 1
#else
/* the far slots as the pool holds them now: one whose blocks went to someone else is empty, and a job in it is over */
static void far_sync(Ws *s)
{
    int i;
    for (i = 0; i < WELD_VIEW_FAR; i++) {
        const DoomoImage *im = NULL;
        int mine = s->on && s->slot == WELD_VIEW_SLOTS + i;
        if (i < ws_far && (s->far_key[i] || mine)) im = portrait_pool_big(FAR_POOL_KEY(i), VW, VH, NULL);
        if (!im) {
            s->far_key[i] = 0;
            if (mine) s->on = 0;
        }
        s->far_img[i] = im;
    }
}

static uint32_t slot_key(const Ws *s, int i)
{
    return i < WELD_VIEW_SLOTS ? weld_view_key[i] : s->far_key[i - WELD_VIEW_SLOTS];
}

static uint32_t slot_used(const Ws *s, int i)
{
    return i < WELD_VIEW_SLOTS ? weld_view_used[i] : s->far_used[i - WELD_VIEW_SLOTS];
}

static const DoomoImage *slot_img(const Ws *s, int i)
{
    return i < WELD_VIEW_SLOTS ? &weld_view_img[i] : s->far_img[i - WELD_VIEW_SLOTS];
}

static uint8_t *slot_pix(const Ws *s, int i)
{
    return i < WELD_VIEW_SLOTS ? weld_view_pix[i] : (uint8_t *)s->far_img[i - WELD_VIEW_SLOTS]->pix;
}

static void slot_touch(Ws *s, int i)
{
    if (i < WELD_VIEW_SLOTS) weld_view_used[i] = weld_view_clock;
    else s->far_used[i - WELD_VIEW_SLOTS] = weld_view_clock;
}

/* slot i is about to be composed into: a far slot takes its blocks from the pool now (0: the pool has none) */
static int slot_claim(Ws *s, int i)
{
    const DoomoImage *im;
    int fresh;
    if (i < WELD_VIEW_SLOTS) return 1;
    im = portrait_pool_big(FAR_POOL_KEY(i - WELD_VIEW_SLOTS), VW, VH, &fresh);
    if (im && ((uintptr_t)im->pix & 3)) im = NULL;          /* (the outline scans read words) */
    s->far_img[i - WELD_VIEW_SLOTS] = im;
    return im != NULL;
}
#endif

static void slot_set_key(Ws *s, int i, uint32_t key)
{
#if !defined(DOOMO_PLATFORM_PS1)
    if (i >= WELD_VIEW_SLOTS) { s->far_key[i - WELD_VIEW_SLOTS] = key; return; }
#else
    (void)s;
#endif
    weld_view_key[i] = key;
}

/* Claim or re-check the slots' memory and forget what was lost with it; returns how many slots there are. */
static int view_sync(Ws *s)
{
    if (!weld_view_pix || !scene_scratch_mine(WELD_SCRATCH_TAG)) {
        /* (re)claim the scene scratch: whatever the slots held is gone */
        weld_view_pix = (WeldViewPix *)scene_scratch(WELD_SCRATCH_TAG, (uint32_t)sizeof(WeldViewPix) * WELD_VIEW_SLOTS);
        memset(weld_view_key, 0, sizeof(weld_view_key));
        if (s->slot < WELD_VIEW_SLOTS) s->on = 0;
    }
    if (s->gfx_gen != weld_gfx_gen) {
        /* weld_gfx_invalidate(): every picture is composed again */
        int i;
        s->gfx_gen = weld_gfx_gen;
        s->on = 0;
        s->shown = 0;
        for (i = WELD_VIEW_SLOTS; i < WELD_VIEW_SLOTS + ws_far; i++) slot_set_key(s, i, 0);
    }
    far_sync(s);
    if (s->on && slot_key(s, s->slot) != 0) s->on = 0;      /* weld_view_image() composed over the job's slot */
    if (s->asked_tick != now()) { s->asked_tick = now(); s->asked_mask = 0; }
    weld_view_clock++;
    return WELD_VIEW_SLOTS + ws_far;
}

static int view_find(Ws *s, uint32_t key, int n)
{
    int i;
    for (i = 0; i < n; i++)
        if (slot_key(s, i) == key) {
            slot_touch(s, i);
            s->asked_mask |= (uint8_t)(1u << i);
            return i;
        }
    return -1;
}

static int shown_ok(const Ws *s, int n)
{
    return s->shown && s->shown <= n && s->shown_key && slot_key(s, s->shown - 1) == s->shown_key;
}

/* units [sl.at ..) of the job's picture: `units` of them, or (until != 0, where passes can be timed) all that end
 * before gba_work_now() reaches `until`; finishes the picture when they are the last */
static void job_slice(Ws *s, int32_t units, uint32_t until)
{
    int had;
    s->sl.left = units;
    s->sl.until = until;
    s->sl.look = 0;                     /* (a timed pass looks at the clock before its first unit) */
    s->sl.n = 0;
    s->sl.stopped = 0;
    had = gba_weld_rows_in();
    compose(slot_pix(s, s->slot), VW, VH, &s->w, s->frame, 0, &s->sl);
    gba_weld_rows_out(had);
    s->last = now();
    if (s->sl.stopped) return;
    slot_set_key(s, s->slot, s->key);
    slot_touch(s, s->slot);
    {
        DoomoImage *im = (DoomoImage *)slot_img(s, s->slot);    /* (a pool picture is ours to finish: portrait.h) */
        im->w = VW;
        im->h = VH;
        im->pix = slot_pix(s, s->slot);
        im->uid = doomo_image_new_uid();
    }
    s->on = 0;
}

/* Make (w, frame) the picture under way unless it is already. A picture someone waits for (wanted) replaces
 * whatever is under way; a prefetch leaves a wanted one alone while it is still asked for, and takes no slot that
 * was asked for in this tick. Never the slot on show. The first empty slot (weld_gfx.c's before the far ones),
 * else the one used longest ago. */
static void job_want(Ws *s, const DoomoWeapon *w, int frame, uint32_t key, int n, int wanted)
{
    int i, slot, lim = n;
    if (s->on && s->key == key) {
        s->asked = now();
        if (wanted) s->wanted = 1;
        return;
    }
    if (s->on && s->wanted && !wanted && s->asked == now()) return;
    for (;;) {
        slot = -1;
        for (i = 0; i < lim; i++) {
            if (shown_ok(s, n) && i == s->shown - 1) continue;
            if (!wanted && (s->asked_mask & (1u << i))) continue;
            if (slot < 0 || (slot_key(s, slot) != 0 && (slot_key(s, i) == 0 || slot_used(s, i) < slot_used(s, slot))))
                slot = i;
        }
        if (slot < 0) return;
        if (slot_claim(s, slot)) break;
        lim = WELD_VIEW_SLOTS;          /* the pool gave no far slot: one of weld_gfx.c's */
        if (slot < lim) return;
    }
    s->on = 1;
    s->key = key;
    s->w = *w;
    s->frame = (uint8_t)frame;
    s->slot = (uint8_t)slot;
    s->wanted = (uint8_t)wanted;
    s->asked = s->last = now();
    slot_set_key(s, slot, 0);
    s->sl.at = 0;
    s->sl.row = SL_TOP;
}

/* the slices the asking call itself may run */
static void job_pump(Ws *s)
{
    if (ws_mode == WS_FRAME) {
        while (s->on && plat_portrait_slice_ok(SLICE_CLASS, (int)(now() - s->last))) {
            job_slice(s, ws_units, 0);
            plat_portrait_slice_done(SLICE_CLASS);
        }
    } else if (s->on && (int32_t)(now() - s->last) >= PATIENCE) {
        /* the frames leave no idle time: one slice all the same, so the picture always comes (the platform
         * only times it: a slice the core forces, as the portrait cache's) */
        (void)plat_portrait_slice_ok(SLICE_CLASS, -1);
        job_slice(s, ws_units, 0);
        plat_portrait_slice_done(SLICE_CLASS);
    }
}

/* is (w, frame) in one of weld_gfx.c's slots (no slices here: weld_view_image() composes in the call) */
static int plain_cached(uint32_t key)
{
    int i;
    if (!weld_view_pix || !scene_scratch_mine(WELD_SCRATCH_TAG)) return 0;
    for (i = 0; i < WELD_VIEW_SLOTS; i++) if (weld_view_key[i] == key) return 1;
    return 0;
}

const DoomoImage *weld_view_image_sliced(const DoomoWeapon *w, int frame, int *ready)
{
    Ws *s = ws();
    uint32_t key;
    int n, i;
    if (!s || ws_mode == WS_NOW) {
        if (ready) *ready = 1;
        return weld_view_image(w, frame);
    }
    if (!w) w = &view_fists;
    frame = DOOMO_CLAMP(frame, 0, 3);
    key = weapon_key(w, frame);
    n = view_sync(s);
    i = view_find(s, key, n);
    if (i < 0) {
        job_want(s, w, frame, key, n, 1);
        job_pump(s);
        i = view_find(s, key, n);
    }
    if (ready) *ready = i >= 0;
    if (i >= 0) {
        s->shown = (uint8_t)(i + 1);
        s->shown_key = key;
        return slot_img(s, i);
    }
    if (!shown_ok(s, n)) return NULL;
    slot_touch(s, s->shown - 1);                        /* the old picture meanwhile */
    s->asked_mask |= (uint8_t)(1u << (s->shown - 1));
    return slot_img(s, s->shown - 1);
}

int weld_view_prefetch(const DoomoWeapon *w, int frame)
{
    Ws *s = ws();
    uint32_t key;
    int n;
    if (!w) w = &view_fists;
    frame = DOOMO_CLAMP(frame, 0, 3);
    key = weapon_key(w, frame);
    if (!s || ws_mode == WS_NOW) return plain_cached(key);  /* no slices: composed when it is asked for, as before */
    n = view_sync(s);
    if (view_find(s, key, n) >= 0) return 1;
    job_want(s, w, frame, key, n, 0);
    job_pump(s);
    return view_find(s, key, n) >= 0;
}

int weld_idle_next(void)
{
    Ws *s = ws();
    if (!s || !s->on || s->asked != now()) return -1;   /* (asked for by the frame just drawn) */
    if (!scene_scratch_mine(WELD_SCRATCH_TAG) || s->gfx_gen != weld_gfx_gen) {
        s->on = 0;
        return -1;
    }
    far_sync(s);
    if (s->on && slot_key(s, s->slot) != 0) s->on = 0;  /* its slot went to someone else */
    return s->on ? SLICE_CLASS : -1;
}

void weld_idle_run(uint32_t until)
{
    Ws *s = ws();
    if (weld_idle_next() < 0) return;
    if (SL_TIMED && until) job_slice(s, SL_ALL, until);
    else job_slice(s, ws_units, 0);
}

void weld_slice_test(int mode, int units, int far)
{
#if defined(DOOMO_PLATFORM_GBA) || defined(DOOMO_PLATFORM_PS1)
    (void)mode; (void)units; (void)far;
#else
    ws_mode = (uint8_t)(mode == 1 ? WS_IDLE : mode == 2 ? WS_FRAME : WS_NOW);
    ws_units = units > 0 ? units : SLICE_UNITS;
    ws_far = (uint8_t)DOOMO_CLAMP(far, 0, WELD_VIEW_FAR);
    memset(&ws_state, 0, sizeof(ws_state));
    ws_state.gfx_gen = weld_gfx_gen;
#endif
}
