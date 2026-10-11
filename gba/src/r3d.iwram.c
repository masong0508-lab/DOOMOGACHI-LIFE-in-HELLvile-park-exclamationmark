/* r3d.iwram.c - the GBA height raycaster's hot code: ARM, in IWRAM
 * (owner: gba). See r3d.c for the overview; pixel loops are hand-written
 * in r3d_loops.s.
 *
 * Column pass (one ray per 2-pixel-wide column, front to back):
 *   The ray walks the cell grid (DDA). The column keeps an open window of
 *   rows [ytop, ybot) that are still unpainted - Doom's per-column clip.
 *   Cells identical to the region the ray is in cost a few cycles (raw
 *   8-byte compare). At every boundary where something changes it
 *     - closes the current floor / ceiling run (when height, texture or
 *       light change) by adding its rows to a visplane,
 *     - draws the step face between different floor heights (riser) and
 *       ceiling heights (upper face, lintels, doors),
 *     - draws solid walls; a solid cell whose ceil_h > floor_h is a block
 *       of that height (crates, pillars, hedges, low walls, buildings
 *       under the sky): its side is drawn, its top becomes the new floor
 *       and the ray CONTINUES over it,
 *     - records a clip checkpoint (distance, window) for sprites.
 *   Sky is the background: it fills whatever rows are still open when
 *   the ray ends, and the opening above a sky cell where a roofed cell
 *   starts (skylights, doorways: up to the roof of the doorway).
 * Plane pass: each visplane becomes horizontal spans (Doom's
 *   R_MakeSpans). A span has a constant depth: one colormap (per-row
 *   distance light) and linear texture stepping with packed u/v. Far
 *   spans use the texture's average colour (no shimmer).
 * Sprites: drawn far to near, every column clipped by the checkpoint
 *   window at the sprite's depth, with transparency, dithered
 *   translucency, light and drop shadows.
 */
#include "r3d_int.h"

R3DFrame r3f;
uint8_t  r3d_cmap[16 * 256];
int32_t  r3d_rowk[R3D_KROWS];
uint8_t  r3d_fade[R3D_FADE_N];
int32_t  r3d_colt[R3D_MAXCOLS];
int16_t  r3d_colang[R3D_MAXCOLS];
uint8_t  r3d_dither[R3D_MAXCOLS];
const uint8_t *r3d_texc[R3D_MAXTEX];

/* big per-frame buffers live in EWRAM */
EWRAM_BSS R3DClip  r3d_clips[R3D_MAXCOLS][R3D_MAXCP];
EWRAM_BSS int8_t   r3d_roof[DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H];
uint8_t  r3d_nclips[R3D_MAXCOLS];

#define LINE 120                         /* halfwords per screen row */

static inline const uint8_t *tex_get(int id)
{
    const uint8_t *t = r3d_texc[id & (R3D_MAXTEX - 1)];
    return t ? t : r3d_texture(id & (R3D_MAXTEX - 1));
}

/* row edge of height z4 (1/16 units) at scale k8: rows >= result are
 * below the edge. */
static inline int rowof(int32_t z4, int32_t k8)
{
    int32_t dz = r3f.ez4 - z4;
    if (dz > 32767) dz = 32767;
    if (dz < -32767) dz = -32767;
    return (r3f.hor12 + dz * k8 + 0x800) >> 12;
}

static inline const uint8_t *light_cm(const uint8_t *cm, int light, uint32_t d16)
{
    uint32_t b = d16 >> 21;
    int l = light - r3d_fade[b < R3D_FADE_N ? b : R3D_FADE_N - 1];
    if (l < 0) l = 0;
    if (l > 15) l = 15;
    return cm + (l << 8);
}

/* ------------------------------------------------------------------------ */
/* column primitives                                                         */

/* textured vertical face: rows [y0, y1) of the column at p (row 0) */
static void draw_face(uint16_t *p, int y0, int y1, const uint8_t *texcol,
                      uint32_t d16, const uint8_t *cm)
{
    int n = y1 - y0;
    uint32_t dv;
    int32_t v;
    if (n <= 0) return;
    r3f.dbg_facepx += (uint32_t)n;
    dv = (uint32_t)(((uint64_t)d16 * r3f.invproj) >> 32);    /* texels/pixel */
    v = (int32_t)(((y0 - r3f.hor) * 2 + 1) * (int32_t)(dv >> 1)) - r3f.ez;
    r3d_face_loop(p + y0 * LINE, n, texcol, v, (int32_t)dv, cm);
}

static void fill_col(uint16_t *p, int y0, int y1, uint32_t c)
{
    if (y1 <= y0) return;
    r3d_fill_loop(p + y0 * LINE, y1 - y0, c | (c << 8), LINE * 2);
}

/* sky background: rows [y0, y1) of column c. Sky row = 95 - (hor - r)/2,
 * clamped: rows above the sky's top repeat row 0, rows below the horizon
 * repeat row 95. */
static void draw_sky(uint16_t *p, int c, int y0, int y1, const uint8_t *cm)
{
    const uint8_t *col;
    const uint8_t *map = cm == r3d_cmap ? 0 : cm + (15 << 8);
    int r, a, b, hor2;
    if (y1 <= y0) return;
    r3f.dbg_skypx += (uint32_t)(y1 - y0);
    col = r3f.sky + (((r3f.skyang + r3d_colang[c]) >> 4) & 255);
    p += y0 * LINE;
    r = y0;
    /* v2 = 2 * sky row = 191 - (hor - r) = r + 191 - hor */
    hor2 = 191 - r3f.hor;
    a = -hor2;                  /* first row with v2 >= 0 */
    b = 192 - hor2;             /* first row with v2 > 191 */
    if (r < a) {
        int n = (a < y1 ? a : y1) - r;
        uint32_t c0 = map ? map[col[0]] : col[0];
        r3d_fill_loop(p, n, c0 | (c0 << 8), LINE * 2);
        p += n * LINE;
        r += n;
    }
    if (r < y1 && r < b) {
        int n = (b < y1 ? b : y1) - r;
        r3d_sky_loop(p, n, col, r + hor2, map);
        p += n * LINE;
        r += n;
    }
    if (r < y1) {
        uint32_t c0 = map ? map[col[95 << 8]] : col[95 << 8];
        r3d_fill_loop(p, y1 - r, c0 | (c0 << 8), LINE * 2);
    }
}

/* ------------------------------------------------------------------------ */
/* floors and ceilings: cast down the column                                 */

/* rows [y0, y1) of a horizontal surface at height z4 (1/16 units) */
static void flat(uint16_t *col, const uint8_t *cm, int32_t rdx, int32_t rdy,
                 int32_t z4, int tex, int light, int y0, int y1)
{
    R3DFlat fl;
    int32_t hd4 = r3f.ez4 - z4;
    int m0;
    if (y1 <= y0 || hd4 == 0) return;
    r3f.dbg_flatpx += (uint32_t)(y1 - y0);
    if (hd4 > 0) {                              /* floor: below the eye */
        m0 = y0 - r3f.hor;
        fl.kdir = 4;
    } else {                                    /* ceiling: above the eye */
        m0 = r3f.hor - 1 - y0;
        fl.kdir = -4;
        hd4 = -hd4;
    }
    if (hd4 > 0x3FFF) hd4 = 0x3FFF;             /* 1024 units */
    if (m0 < 0) m0 = 0;
    if (m0 >= R3D_KROWS) m0 = R3D_KROWS - 1;
    fl.p = col + y0 * LINE;
    fl.n = y1 - y0;
    fl.kp = &r3d_rowk[m0];
    fl.u0 = (uint32_t)r3f.ex << 10;
    fl.v0 = (uint32_t)r3f.ey << 10;
    fl.A = (int32_t)(((int64_t)rdx * hd4) >> 10);
    fl.B = (int32_t)(((int64_t)rdy * hd4) >> 10);
    fl.tex = tex_get(tex);
    fl.cm = cm;
    fl.L = light;
    fl.hd = (hd4 >> 4) + 1;
    fl.fade = r3d_fade;
    fl.far = 1400u << 8;
    fl.avg = r3d_texavg(tex);
    r3d_flat_loop(&fl);
}

/* ------------------------------------------------------------------------ */
/* the column pass                                                           */

static inline uint32_t dir_recip(int32_t r)
{
    /* |1/r| in 16.16 cells per cell (r is 16.16), from the 1/x table:
     * gba_recip8[i] = 2^26 / i  ->  2^32 / r = recip8[r >> 2] << 4 */
    uint32_t a = (uint32_t)(r < 0 ? -r : r) >> 2;
    if (a < 64) return 0x40000000u;
    if (a > 32767) a = 32767;
    return gba_recip8[a] << 4;
}

static inline uint32_t mul16u(uint32_t a, uint32_t b)
{
    uint64_t r = ((uint64_t)a * b) >> 16;
    return r > 0x40000000u ? 0x40000000u : (uint32_t)r;
}

#define CW1_DYN  ((uint32_t)(CELL_DOOR | CELL_LIFT) << 16)

/* Everything a column needs between walker calls. Kept in memory (IWRAM)
 * and handed around by pointer: the per-boundary code touches only what
 * it uses instead of shuffling 30 live variables through registers. */
typedef struct ColState {
    R3DWalk wk;                     /* first: r3d_walk() uses its layout  */
    int32_t rdx, rdy, stx, sty;     /* ray direction / step signs         */
    int32_t fz4, cz4;               /* region floor / ceiling, 1/16 units */
    int32_t ftex, ctex, light, sky;
    int32_t ytop, ybot;             /* open window                        */
    int32_t c, ncp;
    uint16_t *col;
    const uint8_t *cm;              /* column colormap base (tint dither) */
    R3DClip *cps;
} ColState;

static ColState CS;

static void face(ColState *s, int y0, int y1, int tex, uint32_t d16)
{
    int u;
    if (s->wk.side == 0) {
        int32_t hit = r3f.ey + (int32_t)(((int64_t)s->rdy * (int32_t)d16) >> 16);
        u = (hit >> 16) & 63;
        if (s->stx < 0) u = 63 - u;
    } else {
        int32_t hit = r3f.ex + (int32_t)(((int64_t)s->rdx * (int32_t)d16) >> 16);
        u = (hit >> 16) & 63;
        if (s->sty > 0) u = 63 - u;
    }
    draw_face(s->col, y0, y1, tex_get(tex) + u, d16,
              light_cm(s->cm, s->light - s->wk.side, d16));
}

/* close the region's floor run at row edge yf */
static inline void close_floor(ColState *s, int yf)
{
    if (s->fz4 < r3f.ez4 && yf < s->ybot) {
        int tp = yf > s->ytop ? yf : s->ytop;
        flat(s->col, s->cm, s->rdx, s->rdy, s->fz4, s->ftex, s->light, tp, s->ybot);
        s->ybot = tp;
    }
}

static inline void close_ceil(ColState *s, int yc)
{
    if (s->cz4 > r3f.ez4 && yc > s->ytop) {
        int bt = yc < s->ybot ? yc : s->ybot;
        flat(s->col, s->cm, s->rdx, s->rdy, s->cz4, s->ctex, s->light, s->ytop, bt);
        s->ytop = bt;
    }
}

static inline void sky_to(ColState *s, int y)
{
    if (y > s->ytop) {
        int bt = y < s->ybot ? y : s->ybot;
        draw_sky(s->col, s->c, s->ytop, bt, s->cm);
        s->ytop = bt;
    }
}

/* One boundary where the next cell differs from the region (res 0) or
 * the map ends (res 1). Returns 1 when the column is finished. */
static int __attribute__((noinline)) boundary(ColState *s, int res)
{
    const uint32_t d16 = s->wk.d << 6;
    const int32_t k8 = r3d_k8(d16);
    const int otop = s->ytop, obot = s->ybot;
    uint32_t w0, w1;
    int done = 0;
    if (res == 0) { w0 = s->wk.w0; w1 = s->wk.w1; }
    else { w0 = 1; w1 = 0; }                          /* outside: solid */
    s->wk.c1 = w1;
    if (!(w0 & 0xFF)) {
        /* ---- open cell ---- */
        int32_t nfz4, ncz4;
        int nftex = (w0 >> 8) & 0xFF, nctex = (w0 >> 16) & 0xFF;
        int nl = (w1 >> 8) & 0xFF, nsky = nctex == DOOMO_TEX_SKY;
        if (w1 & CW1_DYN) {
            int w = r3f.mw, h = r3f.mh;
            int mx = s->stx > 0 ? w - 1 - s->wk.cx : s->wk.cx;
            int my = s->sty > 0 ? h - 1 - s->wk.cy : s->wk.cy;
            const DoomoCell *cl = (const DoomoCell *)((const uint8_t *)s->wk.cells + s->wk.idx8);
            nfz4 = r3d_cell_floor(mx, my, cl) << 4;
            ncz4 = r3d_cell_ceil(mx, my, cl) << 4;
            s->wk.c0 = 0xFFFFFFFFu;
        } else {
            nfz4 = ((int32_t)w0 >> 24) << 7;
            ncz4 = (int32_t)(int8_t)(w1 & 0xFF) << 7;
            s->wk.c0 = w0;
        }
        if (nfz4 != s->fz4 || nftex != s->ftex || nl != s->light)
            close_floor(s, rowof(s->fz4, k8));
        if (!s->sky && (nsky || ncz4 != s->cz4 || nctex != s->ctex || nl != s->light))
            close_ceil(s, rowof(s->cz4, k8));
        /* riser: the next floor is higher */
        if (nfz4 > s->fz4 && s->ytop < s->ybot) {
            int yr = rowof(nfz4, k8), yf = rowof(s->fz4, k8);
            int y0 = yr > s->ytop ? yr : s->ytop;
            int y1 = yf < s->ybot ? yf : s->ybot;
            if (y0 < y1) face(s, y0, y1, nftex, d16);
            if (y0 < s->ybot) s->ybot = y0;
        }
        if (s->ytop < s->ybot) {
            if (s->sky && !nsky) {
                /* a roofed cell seen from under the sky: sky above its
                 * roof, the lintel (or door) below */
                int32_t top4 = s->cz4;
                int roof = r3d_roof[s->wk.idx8 >> 3];
                if (roof != 127 && ((int32_t)roof << 7) < top4) top4 = (int32_t)roof << 7;
                if (top4 < ncz4) top4 = ncz4;
                sky_to(s, rowof(top4, k8));
                if (ncz4 < top4 && s->ytop < s->ybot) {
                    int yu = rowof(ncz4, k8);
                    int y1 = yu < s->ybot ? yu : s->ybot;
                    if (s->ytop < y1) { face(s, s->ytop, y1, nctex, d16); s->ytop = y1; }
                }
            } else if (ncz4 < s->cz4 && !s->sky) {
                /* upper face: the next ceiling is lower */
                int yu = rowof(ncz4, k8);
                int y1 = yu < s->ybot ? yu : s->ybot;
                if (s->ytop < y1) {
                    face(s, s->ytop, y1, nsky ? s->ctex : nctex, d16);
                    s->ytop = y1;
                }
            }
        }
        s->fz4 = nfz4; s->cz4 = ncz4;
        s->ftex = nftex; s->ctex = nctex; s->light = nl; s->sky = nsky;
    } else {
        /* ---- solid cell (or outside the map) ---- */
        int wtex = w0 & 0xFF;
        int fh = (int8_t)(w0 >> 24), ch = (int8_t)(w1 & 0xFF);
        int32_t top4 = (res == 0 && ch > fh) ? (int32_t)ch << 7 : s->cz4;
        close_floor(s, rowof(s->fz4, k8));
        if (res == 0 && (s->sky || top4 < s->cz4)) {
            /* a block of height top4: draw its side, walk over its top */
            int nl = (w1 >> 8) & 0xFF;
            if (!s->sky && nl != s->light) close_ceil(s, rowof(s->cz4, k8));
            if (top4 > s->fz4) {
                int yt = rowof(top4, k8), yf = rowof(s->fz4, k8);
                int y0 = yt > s->ytop ? yt : s->ytop;
                int y1 = yf < s->ybot ? yf : s->ybot;
                if (y0 < y1) face(s, y0, y1, wtex, d16);
                if (y0 < s->ybot) s->ybot = y0;
            }
            s->fz4 = top4;
            s->ftex = ((w0 >> 8) & 0xFF) ? (int)((w0 >> 8) & 0xFF) : wtex;
            s->light = nl;
            /* consecutive identical blocks (thick walls) walk fast */
            s->wk.c0 = w0;
            s->wk.c1 = w1 & ~CW1_DYN;
        } else {
            /* full wall up to the ceiling (or the sky height) */
            if (!s->sky) close_ceil(s, rowof(s->cz4, k8));
            else sky_to(s, rowof(s->cz4, k8));
            if (s->ytop < s->ybot) face(s, s->ytop, s->ybot, wtex, d16);
            s->ytop = s->ybot;
            done = 1;
        }
    }
    if (s->ytop != otop || s->ybot != obot) {
        uint32_t d8 = d16 >> 13;
        R3DClip *k;
        if (s->ncp < R3D_MAXCP) k = &s->cps[s->ncp++];
        else { k = &s->cps[R3D_MAXCP - 1]; r3f.cp_over++; }
        k->d8 = (uint16_t)(d8 > 65535 ? 65535 : d8);
        k->top = (uint8_t)(s->ytop > 255 ? 255 : (s->ytop < 0 ? 0 : s->ytop));
        k->bot = (uint8_t)(s->ybot > 255 ? 255 : (s->ybot < 0 ? 0 : s->ybot));
    }
    if (done || s->ytop >= s->ybot) return 1;
    /* early out under the sky: the open rows are all above anything
     * that can still come (the map's tallest top) */
    if (s->sky && r3f.maxtop4 > r3f.ez4 && s->ybot <= rowof(r3f.maxtop4, k8))
        return 1;
    return 0;
}

static void column(int c)
{
    R3DFrame *const f = &r3f;
    ColState *const s = &CS;
    const int32_t t = r3d_colt[c];
    const int32_t px = f->ex >> 6, py = f->ey >> 6;    /* 16.16 cells */
    const int w = f->mw, h = f->mh;
    int mx = px >> 16, my = py >> 16;
    int32_t rdx, rdy;

    s->c = c;
    s->col = f->dst + c;
    s->ytop = 0;
    s->ybot = f->vh;
    s->ncp = 0;
    s->cps = r3d_clips[c];
    {
        uint32_t d = r3d_dither[c];
        if (f->frame & 1) d ^= 8;
        s->cm = (f->cmt && d < f->tint_thresh) ? f->cmt : f->cm;
    }
    if ((unsigned)mx >= (unsigned)w || (unsigned)my >= (unsigned)h) {
        fill_col(s->col, 0, f->vh, f->cm[15 * 256 + 1]);
        r3d_nclips[c] = 0;
        return;
    }
    rdx = f->fx + (int32_t)(((int64_t)f->rx * t) >> 16);
    rdy = f->fy + (int32_t)(((int64_t)f->ry * t) >> 16);
    s->rdx = rdx;
    s->rdy = rdy;
    s->wk.ddx = dir_recip(rdx);
    s->wk.ddy = dir_recip(rdy);
    if (rdx < 0) { s->stx = -1; s->wk.sdx = mul16u((uint32_t)px & 0xFFFF, s->wk.ddx); s->wk.cx = mx; }
    else { s->stx = 1; s->wk.sdx = mul16u(0x10000u - ((uint32_t)px & 0xFFFF), s->wk.ddx); s->wk.cx = w - 1 - mx; }
    if (rdy < 0) { s->sty = -1; s->wk.sdy = mul16u((uint32_t)py & 0xFFFF, s->wk.ddy); s->wk.cy = my; }
    else { s->sty = 1; s->wk.sdy = mul16u(0x10000u - ((uint32_t)py & 0xFFFF), s->wk.ddy); s->wk.cy = h - 1 - my; }
    s->wk.idx8 = (my * w + mx) * 8;
    s->wk.ix8 = s->stx * 8;
    s->wk.iy8 = s->sty * w * 8;
    s->wk.cells = (const uint32_t *)f->cells;
    s->wk.maxd = f->maxd;
    {
        const uint32_t *wp = (const uint32_t *)((const uint8_t *)s->wk.cells + s->wk.idx8);
        uint32_t w0 = wp[0], w1 = wp[1];
        if (w1 & CW1_DYN) {
            s->fz4 = r3d_cell_floor(mx, my, (const DoomoCell *)wp) << 4;
            s->cz4 = r3d_cell_ceil(mx, my, (const DoomoCell *)wp) << 4;
        } else {
            s->fz4 = ((int32_t)w0 >> 24) << 7;
            s->cz4 = (int32_t)(int8_t)(w1 & 0xFF) << 7;
        }
        s->ftex = (w0 >> 8) & 0xFF;
        s->ctex = (w0 >> 16) & 0xFF;
        s->light = (w1 >> 8) & 0xFF;
        s->sky = s->ctex == DOOMO_TEX_SKY;
        s->wk.c0 = ((w1 & CW1_DYN) || (w0 & 0xFF)) ? 0xFFFFFFFFu : w0;
        s->wk.c1 = w1;
    }

    for (;;) {
        int res = r3d_walk(&s->wk);
        if (res == 2) {
            /* far end: floor and ceiling run to the horizon */
            close_floor(s, f->hor);
            if (!s->sky) close_ceil(s, f->hor);
            break;
        }
        f->dbg_steps++;
        if (boundary(s, res)) break;
    }
    /* whatever is still open shows the sky (or the void) */
    if (s->ytop < s->ybot) draw_sky(s->col, c, s->ytop, s->ybot, s->cm);
    r3d_nclips[c] = (uint8_t)s->ncp;
}

void r3d_columns(void)
{
    int c, n = r3f.ncol;
    r3f.dbg_steps = r3f.dbg_facepx = r3f.dbg_skypx = r3f.dbg_flatpx = 0;
    for (c = 0; c < n; c++) column(c);
}

/* ------------------------------------------------------------------------ */
/* sprites                                                                    */

static void clip_at(int c, uint32_t d8, int *top, int *bot)
{
    const R3DClip *k = r3d_clips[c];
    int n = r3d_nclips[c], i;
    *top = 0;
    *bot = r3f.vh;
    for (i = 0; i < n; i++) {
        if (k[i].d8 > d8) break;
        *top = k[i].top;
        *bot = k[i].bot;
    }
}

void r3d_draw_sprite(const R3DSprite *s)
{
    R3DFrame *const f = &r3f;
    const DoomoImage *img = s->img;
    const int iw = img->w, ih = img->h;
    const uint8_t *pix = img->pix;
    const uint32_t d8 = (uint32_t)s->depth16 >> 13;
    int32_t x0 = s->x0_12, x1 = s->x0_12 + s->w12;
    int c0 = (x0 + 0x0FFF - 0x1000) >> 13;     /* first column whose centre >= x0 */
    int c, r0, r1;
    int32_t vr0;
    if (c0 < 0) c0 = 0;
    r0 = (s->ytop12 + 0x800) >> 12;
    r1 = (s->ytop12 + s->h12 + 0x800) >> 12;
    if (r0 < 0) r0 = 0;
    if (r1 > f->vh) r1 = f->vh;

    /* drop shadow: a darkened ellipse on the floor under the feet */
    if (s->flags & BB_SHADOW) {
        int32_t cx12 = x0 + (s->w12 >> 1);
        int32_t hw12 = (s->w12 * 3) >> 3;
        int sy = (s->shadow_y12 + 0x800) >> 12;
        int hh = s->shadow_hh;
        const uint8_t *dk = f->cm + (8 << 8);
        int cs = (cx12 - hw12 - 0x1000 + 0x1FFF) >> 13, ce = (cx12 + hw12 - 0x1000) >> 13;
        if (cs < 0) cs = 0;
        if (ce >= f->ncol) ce = f->ncol - 1;
        if (hw12 > 0)
        for (c = cs; c <= ce; c++) {
            int top, bot, y, ya, yb, hc;
            int32_t dx = ((c * 2 + 1) << 12) - cx12;
            int32_t q;
            if (dx < 0) dx = -dx;
            if (dx >= hw12) continue;
            /* half height of the ellipse at this x: hh * sqrt(1 - (dx/hw)^2) */
            q = (int32_t)(((int64_t)dx << 8) / hw12);          /* 0..255 */
            q = 256 - ((q * q) >> 8);
            hc = (hh * (q > 192 ? 4 : q > 112 ? 3 : q > 48 ? 2 : 1) + 2) >> 2;
            if (hc < 1) hc = 1;
            clip_at(c, d8, &top, &bot);
            ya = sy - hc; yb = sy + hc;
            if (ya < top) ya = top;
            if (yb > bot) yb = bot;
            for (y = ya; y < yb; y++) {
                uint16_t *p = f->dst + y * LINE + c;
                uint32_t v = dk[*p & 0xFF];
                *p = (uint16_t)(v | (v << 8));
            }
        }
    }

    vr0 = (int32_t)(((int64_t)(((r0 << 12) + 0x800) - s->ytop12) * s->step16) >> 12);
    for (c = c0; c < f->ncol; c++) {
        int32_t cx12 = (c * 2 + 1) << 12;
        int32_t u16;
        int u, top, bot, ya, yb, y;
        int32_t v;
        const uint8_t *src;
        const uint8_t *cm;
        uint16_t *p;
        if (cx12 >= x1) break;
        u16 = (int32_t)(((int64_t)(cx12 - x0) * s->step16) >> 12);
        u = u16 >> 16;
        if (u >= iw) u = iw - 1;
        if (u < 0) u = 0;
        if (s->flags & BB_FLIPX) u = iw - 1 - u;
        clip_at(c, d8, &top, &bot);
        ya = r0 > top ? r0 : top;
        yb = r1 < bot ? r1 : bot;
        if (ya >= yb) continue;
        {
            uint32_t d = r3d_dither[c];
            if (f->frame & 1) d ^= 8;
            cm = (f->cmt && d < f->tint_thresh) ? f->cmt : f->cm;
        }
        cm += s->level << 8;
        src = pix + u;
        v = vr0 + (int32_t)((ya - r0) * (int32_t)s->step16);
        p = f->dst + ya * LINE + c;
        if (s->trans) {
            for (y = ya; y < yb; y++, p += LINE, v += (int32_t)s->step16) {
                int tv = v >> 16;
                uint32_t t;
                if (((y + c + (int)f->frame) & 1)) continue;
                if (tv >= ih) break;
                t = src[tv * iw];
                if (t) { t = cm[t]; *p = (uint16_t)(t | (t << 8)); }
            }
        } else {
            for (y = ya; y < yb; y++, p += LINE, v += (int32_t)s->step16) {
                int tv = v >> 16;
                uint32_t t;
                if (tv >= ih) break;
                t = src[tv * iw];
                if (t) { t = cm[t]; *p = (uint16_t)(t | (t << 8)); }
            }
        }
    }
}
