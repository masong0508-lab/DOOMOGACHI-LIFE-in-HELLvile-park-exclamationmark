/* ow_obj.c - the billboards of a tile-mode overworld frame as hardware
 * sprites (owner: gba; G6, docs/GBA.md section 6c).
 *
 * In a tile frame (canvas_int.h) the world is BG2 (priority 1) and the
 * canvas BG0 (priority 0). A billboard becomes one or more 8bpp OBJs of
 * priority 1: above the world, below the canvas, and among themselves in
 * Mode 4 order (the OBJ with the lower OAM index wins, so the last one
 * drawn gets the lowest entry). Its tiles hold the final screen pixels:
 * texels lit through the colormap, the night / view tint dither at the
 * screen pixel (anchored to the screen, as in Mode 4: the dither phase
 * (x & 3, y & 3) is part of the key), translucency as a checker, and the
 * raised cells that Mode 4 paints over the billboard later (the rows below
 * its feet) cleared. So a tile frame shows the Mode 4 pixels exactly.
 *
 * VRAM: OBJ VRAM 0x06014000..0x06018000 (256 8bpp tiles, the 3D texture
 * cache and squash slot area; below it lie page 1's last bytes, which a
 * tile frame's dense canvas uses, and the raycaster's colormap gap). It is
 * shared with obj.c: region k (4 KB) is squash slot k's while that slot is
 * in use, and title figures use 2 and 3; billboards take what is left,
 * from the top. A block (one billboard image under one key: image, scale,
 * flip, light level, dither phase and tint state, checker parity,
 * occlusion mask) is uploaded when its key is new and kept LRU; the tiles
 * of the frame on screen (`shown`) are never rewritten.
 *
 * RAM: the block table, the frame's billboards and the line budget live in
 * the IWRAM overlay slot's bytes past the 2D group (iwram_ovl_2d_tail), a
 * scratch that exists while the 2D group is in: every tile frame loads it,
 * and a load of the 3D group (plat_view3d) wipes it, which the load count
 * detects (the table is rebuilt; OBJ VRAM was the 3D view's anyway). Only
 * the shown frame's tile bitmap and a few counters are EWRAM .bss; the OAM
 * entries are written into a buffer on the flip's stack (obj.c).
 *
 * What stays in the canvas: the shadow blobs (they darken what is under
 * them: the canvas, an earlier billboard's OBJ pixel or the world; and
 * they leave the pixels of the billboards drawn after them transparent,
 * so those OBJs show through), and the billboards that do not get OBJs
 * (OAM entries, VRAM or the per-line OBJ cycle budget ran out, or one
 * over 255 px), with every later billboard whose box meets one of them.
 *
 * Leaving the canvas (canvas.c cv_to_m4: a pause panel, a fade, 3D) first
 * draws the OBJ billboards into the canvas *under* what it holds (back to
 * front), so the conversion to Mode 4 keeps every pixel; a shade rect
 * (plat_shade_rect) does the same for the billboards under it first.
 */
#include "canvas_int.h"
#include "ow_int.h"
#include "postfx.h"
#include "iwram_ovl.h"
#include "doomo/palette.h"

#define TILE 16
#define UNITS_PER_PX (DOOMO_CELL_UNITS / TILE)
#define BB_VRAM    0x06014000u
#define BB_UNITS   256                  /* 8bpp tiles of 64 B */
#define BB_RUNITS  64                   /* units per squash region */
#define BB_BLK_N   32
#define BB_PUT_N   64
#define BB_ENT0    16                   /* OAM: 0..3 squash, 4..13 figures */
/* the OBJ renderer's cycles per scanline (no H-blank interval free), and
 * what one shown squash OBJ (double-size affine, 128 wide) takes of them;
 * the line budget counts in units of 8 cycles */
#define OBJ_LINE_CYCLES 1210
#define SQUASH_CYCLES   266
#define LINE_MARGIN     32
/* the per-pixel loops (shadow rows, row reads, occlusion) are ARM code in
 * the 2D group (a tile frame has it in); no library calls from there */
#define BB_ARM IWRAM_2D_CODE __attribute__((optimize("Os", "no-tree-loop-distribute-patterns")))

typedef struct BbBlk {
    uint32_t k[5];                      /* key                              */
    uint32_t fk;                        /* the last place key that found it */
    uint32_t last;                      /* cv.frame it was last used in     */
    uint8_t  t0, n1;                    /* units [t0, t0 + n1 + 1)          */
    uint8_t  w1, h1;                    /* the scaled image, minus 1        */
    uint8_t  live, nc, nr, pad;         /* nc x nr pieces                   */
    uint16_t cwc, rhc;                  /* piece widths / heights: 2 bits each, 8 << code */
} BbBlk;                                /* 40 B */
typedef struct BbPut {                  /* a billboard shown as OBJs        */
    int16_t  x, y;                      /* its screen box origin            */
    uint8_t  blk, gone;                 /* gone: drawn into the canvas      */
} BbPut;
typedef struct Lay {                    /* the OBJ pieces: column widths x row heights */
    uint8_t nc, nr, cw[8], rh[8];
    uint16_t wt, ht;
} Lay;
typedef struct BbTail {                 /* in the overlay slot (see above)  */
    BbBlk    blk[BB_BLK_N];
    BbPut    put[BB_PUT_N];
    uint32_t used[BB_UNITS / 32];       /* units held by a block            */
    uint16_t nput, ent_n;
} BbTail;
/* gba.ld asserts that the slot keeps this many bytes past the 2D group */
#define BB_TAIL_BYTES 1712u
DOOMO_STATIC_ASSERT(sizeof(BbTail) <= BB_TAIL_BYTES, bb_tail_fits_the_slot);

static struct {
    BbTail  *t;                         /* the tail, valid while loads == gen */
    uint32_t gen;                       /* iwram_ovl_loads() at the reset   */
    uint32_t r3d;                       /* gba_objvram_r3d_frame() then     */
    uint32_t shown[BB_UNITS / 32];      /* units the screen shows           */
    uint32_t lift;                      /* this frame's lift grid hash      */
} bb;
int bb_edge;                            /* canvas.c: a shake with a piece over an edge */

/* the tail, or NULL when the slot no longer holds it */
static BbTail *tail(void)
{
    if (!bb.t || iwram_ovl_current() != OVL_2D || iwram_ovl_loads() != bb.gen) return NULL;
    return bb.t;
}

/* ---- layout --------------------------------------------------------------- */
static int chunks(int d, uint8_t *o)
{
    int n = 0;
    d = (d + 7) & ~7;
    while (d > 64) { o[n++] = 64; d -= 64; }
    switch (d) {
    case 8:  o[n++] = 8; break;
    case 16: o[n++] = 16; break;
    case 24:
    case 32: o[n++] = 32; break;
    case 40: o[n++] = 32; o[n++] = 8; break;
    default: o[n++] = 64; break;        /* 48, 56, 64 */
    }
    return n;
}
static int has(const uint8_t *c, int n, int a, int b)
{
    int i;
    for (i = 0; i < n; i++) if (c[i] == a || c[i] == b) return 1;
    return 0;
}
static void split64(uint8_t *c, uint8_t *n)
{
    uint8_t t[8];
    int i, m = 0;
    for (i = 0; i < *n; i++) {
        if (c[i] == 64) { t[m++] = 32; t[m++] = 32; }
        else t[m++] = c[i];
    }
    memcpy(c, t, (size_t)m);
    *n = (uint8_t)m;
}
BB_ARM static void totals(Lay *L)
{
    int i;
    L->wt = L->ht = 0;
    for (i = 0; i < L->nc; i++) L->wt = (uint16_t)(L->wt + L->cw[i]);
    for (i = 0; i < L->nr; i++) L->ht = (uint16_t)(L->ht + L->rh[i]);
}
/* chunks of 8..64; no 64x8, 64x16, 8x64 or 16x64 piece (not OBJ shapes).
 * w, h <= 255: at most 4 chunks, 8 after a split */
static void layout(int w, int h, Lay *L)
{
    L->nc = (uint8_t)chunks(w, L->cw);
    L->nr = (uint8_t)chunks(h, L->rh);
    if (has(L->cw, L->nc, 64, 64) && has(L->rh, L->nr, 8, 16)) split64(L->cw, &L->nc);
    if (has(L->rh, L->nr, 64, 64) && has(L->cw, L->nc, 8, 16)) split64(L->rh, &L->nr);
    totals(L);
}
static uint16_t lay_code(const uint8_t *c, int n)
{
    uint32_t v = 0;
    int i;
    for (i = 0; i < n; i++) v |= (uint32_t)(c[i] == 8 ? 0 : c[i] == 16 ? 1 : c[i] == 32 ? 2 : 3) << (2 * i);
    return (uint16_t)v;
}
BB_ARM static void lay_of(const BbBlk *b, Lay *L)
{
    int i;
    L->nc = b->nc;
    L->nr = b->nr;
    for (i = 0; i < b->nc; i++) L->cw[i] = (uint8_t)(8 << ((b->cwc >> (2 * i)) & 3));
    for (i = 0; i < b->nr; i++) L->rh[i] = (uint8_t)(8 << ((b->rhc >> (2 * i)) & 3));
    totals(L);
}
/* attr0 shape << 14 | attr1 size << 14 for a w x h piece */
static uint32_t shape_size(int w, int h)
{
    int sh, sz;
    if (w == h) { sh = 0; sz = w == 8 ? 0 : w == 16 ? 1 : w == 32 ? 2 : 3; }
    else if (w > h) { sh = 1; sz = w == 16 ? 0 : (w == 32 && h == 8) ? 1 : w == 32 ? 2 : 3; }
    else { sh = 2; sz = h == 16 ? 0 : (h == 32 && w == 8) ? 1 : h == 32 ? 2 : 3; }
    return ((uint32_t)sh << 14) | ((uint32_t)sz << 30);
}

/* row y of a shown billboard over screen [a0, a1) into dst[x - a0], where
 * dst is still 0 (so the topmost one wins when called top first) */
BB_ARM static void put_row(const BbPut *p, const BbBlk *b, const Lay *L, int y, int a0, int a1, uint8_t *dst)
{
    int v = y - p->y, ri = 0, ry = 0, ci, cx = 0, th, rowbase;
    if ((unsigned)v > b->h1 || a1 <= p->x || a0 > p->x + b->w1) return;
    while (v >= ry + L->rh[ri]) ry += L->rh[ri++];
    rowbase = b->t0 + (ry >> 3) * (L->wt >> 3);
    th = L->rh[ri] >> 3;
    for (ci = 0; ci < L->nc; cx += L->cw[ci++]) {
        const int px0 = p->x + cx, tw = L->cw[ci] >> 3;
        int s = DOOMO_MAX(px0, a0), e = DOOMO_MIN(px0 + L->cw[ci], a1), x;
        const vu8 *base;
        if (s >= e) continue;
        base = (const vu8 *)(BB_VRAM + (uint32_t)(rowbase + th * (cx >> 3) + ((v - ry) >> 3) * tw) * 64u +
                             (uint32_t)((v & 7) << 3));
        for (x = s; x < e; x++) {
            const int u = x - px0;
            uint32_t c;
            if (dst[x - a0]) continue;
            c = base[((u >> 3) << 6) | (u & 7)];
            if (c) dst[x - a0] = (uint8_t)c;
        }
    }
}

/* ---- the canvas and the world, row by row ---------------------------------- */
/* the canvas row y over [a0, a1) into dst[x - a0] */
BB_ARM static void canvas_row(int y, int a0, int a1, uint8_t *dst)
{
    int x = a0;
    while (x < a1) {
        const int tx = x >> 3, e = DOOMO_MIN((tx + 1) << 3, a1);
        const uint32_t t = (uint32_t)((y >> 3) * CV_TW + tx);
        const vu8 *p = 0;
        if (cv.mode == CV_DENSE) {
            if (cv.used[t >> 5] & (1u << (t & 31))) p = (const vu8 *)(CV_VRAM + CV_DENSE_OFS + t * 64u);
        } else {
            uint32_t m = cv_meta->cmap[(y >> 3) * 32 + tx];
            if (m != CV_ZERO_TILE) p = (const vu8 *)(CV_VRAM + m * 64u);
        }
        if (p) {
            p += (y & 7) << 3;
            for (; x < e; x++) dst[x - a0] = p[x & 7];
        } else {
            for (; x < e; x++) dst[x - a0] = 0;
        }
    }
}
/* write src[x - a0] (0 = leave the pixel) into canvas row y */
BB_ARM static void canvas_put_row(int y, int a0, int a1, const uint8_t *src)
{
    int x = a0;
    while (x < a1) {
        const int tx = x >> 3, e = DOOMO_MIN((tx + 1) << 3, a1);
        vu16 *row = 0;
        for (; x < e; x++) {
            uint32_t c = src[x - a0], v;
            vu16 *h;
            if (!c) continue;
            if (!row) row = (vu16 *)(cv_tile(tx, y >> 3) + (y & 7) * 2);
            h = row + ((x & 7) >> 1);
            v = *h;
            *h = (uint16_t)((x & 1) ? ((v & 0x00FF) | (c << 8)) : ((v & 0xFF00) | c));
        }
    }
}

/* canvas.c cv_read_px: the shown billboard under a transparent canvas pixel */
uint32_t bb_read_px(int x, int y)
{
    BbTail *T = tail();
    int j;
    if (!T) return 0;
    for (j = T->nput - 1; j >= 0; j--) {
        const BbPut *p = &T->put[j];
        const BbBlk *b = &T->blk[p->blk];
        Lay L;
        uint8_t c = 0;
        if (p->gone || (unsigned)(x - p->x) > b->w1 || (unsigned)(y - p->y) > b->h1) continue;
        lay_of(b, &L);
        put_row(p, b, &L, y, x, x + 1, &c);
        if (c) return c;
    }
    return 0;
}

/* ---- occlusion: the raised cells Mode 4 draws after a billboard ------------- *
 * (canvas_2d.c occ_any / occ_rows over the frame's lift grid) */
__attribute__((long_call, noinline)) static int occ_any(int x0, int y0, int x1, int y1, int row)
{
    const CvMeta *M = cv_meta;
    int cy, cx;
    if (row >= CV_ROW_NONE) return 0;
    for (cy = (y0 + cv.sy) >> 4; cy <= ((y1 - 1 + cv.sy) >> 4) + 1; cy++) {
        int j = cy - M->gy0;
        if (cy <= row || (unsigned)j >= M->gh) continue;
        for (cx = (x0 + cv.sx) >> 4; cx <= (x1 - 1 + cv.sx) >> 4; cx++) {
            int i = cx - M->gx0, l;
            if ((unsigned)i >= M->gw || !(l = M->lift[j][i])) continue;
            if (cy * 16 - cv.sy - l < y1) return 1;
        }
    }
    return 0;
}
/* screen row y: bit i = cell column cxa + i is covered */
__attribute__((long_call, noinline)) static uint32_t occ_row(int y, int cxa, int cxb, int row)
{
    const CvMeta *M = cv_meta;
    const int gx0 = M->gx0, gy0 = M->gy0, gw = M->gw, gh = M->gh;
    const int wy = y + cv.sy, cyr = wy >> 4, j = cyr - gy0;
    uint32_t mk = 0;
    int cx;
    for (cx = cxa; cx <= cxb; cx++) {
        int i = cx - gx0, l;
        if ((unsigned)i >= (unsigned)gw) continue;
        if (cyr > row && (unsigned)j < (unsigned)gh && M->lift[j][i]) mk |= 1u << (cx - cxa);
        else if (cyr + 1 > row && (unsigned)(j + 1) < (unsigned)gh && (l = M->lift[j + 1][i]) &&
                 wy >= (cyr + 1) * 16 - l)
            mk |= 1u << (cx - cxa);
    }
    return mk;
}

/* ---- VRAM blocks -------------------------------------------------------------- */
static int bits_any(const uint32_t *m, int t0, int n)
{
    int u;
    for (u = t0; u < t0 + n; u++) if ((m[u >> 5] >> (u & 31)) & 1) return 1;
    return 0;
}
static void bits_set(uint32_t *m, int t0, int n, int on)
{
    int u;
    for (u = t0; u < t0 + n; u++) {
        if (on) m[u >> 5] |= 1u << (u & 31);
        else m[u >> 5] &= ~(1u << (u & 31));
    }
}
static int evictable(const BbBlk *b)
{
    return b->live && b->last != cv.frame && !bits_any(bb.shown, b->t0, b->n1 + 1);
}
static void blk_free(BbTail *T, int i)
{
    BbBlk *b = &T->blk[i];
    bits_set(T->used, b->t0, b->n1 + 1, 0);
    b->live = 0;
}
static int evict_lru(BbTail *T)
{
    int i, best = -1;
    for (i = 0; i < BB_BLK_N; i++)
        if (evictable(&T->blk[i]) && (best < 0 || T->blk[i].last < T->blk[best].last)) best = i;
    if (best >= 0) blk_free(T, best);
    return best >= 0;
}
/* n free units in the allowed regions (from the top), evicting LRU blocks */
static int units_alloc(BbTail *T, int n, uint32_t allow)
{
    for (;;) {
        int u, run = 0;
        for (u = BB_UNITS - 1; u >= 0; u--) {
            const uint32_t bit = 1u << (u & 31);
            if (((T->used[u >> 5] | bb.shown[u >> 5]) & bit) || !((allow >> (u / BB_RUNITS)) & 1)) {
                run = 0;
                continue;
            }
            if (++run >= n) return u;
        }
        if (!evict_lru(T)) return -1;
    }
}

/* obj.c: squash slot / figure k wants region k. Frees the blocks there
 * that neither the shown frame nor this one uses; 1 = some of it is still
 * in use (the caller waits a frame: the region is reserved from now on). */
int bb_region_take(int k)
{
    BbTail *T = tail();
    int i, busy = bits_any(bb.shown, k * BB_RUNITS, BB_RUNITS);
    if (!T) return busy;
    for (i = 0; i < BB_BLK_N; i++) {
        BbBlk *b = &T->blk[i];
        if (!b->live || b->t0 >= (k + 1) * BB_RUNITS || b->t0 + b->n1 + 1 <= k * BB_RUNITS) continue;
        if (evictable(b)) blk_free(T, i);
        else busy = 1;
    }
    return busy;
}

/* ---- one billboard -------------------------------------------------------------- */
typedef struct Spr {
    const DoomoImage *img;
    int s, w, h, x0, y0, flags, lvl, row;
    int shadow, fy, half, cx;
} Spr;

/* 0 = nothing of it (sprite or shadow) on screen */
static int geom(const DoomoBillboard *b, Spr *g)
{
    const DoomoImage *img = b->img;
    int bx0, by0, bx1, by1;
    if (!img || !img->pix || !img->w || !img->h) return 0;
    g->img = img;
    g->s = DOOMO_CLAMP((b->scale ? b->scale : 256) / 256, 1, 4);
    g->w = img->w * g->s;
    g->h = img->h * g->s;
    g->x0 = (int)((b->x >> 16) / UNITS_PER_PX) - owf.scrollx - g->w / 2;
    g->y0 = (int)((b->y >> 16) / UNITS_PER_PX) - owf.scrolly - g->h - (int)((b->z >> 16) / UNITS_PER_PX);
    g->lvl = (b->flags & BB_FULLBRIGHT) ? 15 : DOOMO_CLAMP(owf.day_level + b->light, 2, 15);
    g->flags = b->flags;
    bx0 = g->x0; by0 = g->y0; bx1 = g->x0 + g->w; by1 = g->y0 + g->h;
    g->shadow = (b->flags & BB_SHADOW) != 0;
    if (g->shadow) {
        g->fy = (int)((b->y >> 16) / UNITS_PER_PX) - owf.scrolly;
        g->half = g->w / 3 + 1;
        g->cx = g->x0 + g->w / 2;
        if (g->cx - g->half < bx0) bx0 = g->cx - g->half;
        if (g->cx + g->half + 1 > bx1) bx1 = g->cx + g->half + 1;
        if (g->fy - 1 < by0) by0 = g->fy - 1;
        if (g->fy + 1 > by1) by1 = g->fy + 1;
    }
    return DOOMO_MAX(bx0, 0) < DOOMO_MIN(bx1, SCREEN_W) && DOOMO_MAX(by0, 0) < DOOMO_MIN(by1, SCREEN_H);
}

static uint32_t content_hash(const DoomoImage *img)
{
    uint32_t h = 2166136261u, n = (uint32_t)img->w * img->h, i, step = n / 61u + 1u;
    for (i = 0; i < n; i += step) h = (h ^ img->pix[i]) * 16777619u;
    return h ^ n;
}

/* the billboard's final pixels into the block's tiles (piece by piece,
 * each piece's tiles row-major: 1D OBJ mapping) */
static void upload(const Spr *g, const BbBlk *b, const Lay *L, const uint32_t *rm, int o)
{
    const DoomoImage *img = g->img;
    const uint8_t *cm = doomo_colormap[g->lvl];
    const uint32_t inv = 65536u / (uint32_t)g->s + 1u;
    const int flip = g->flags & BB_FLIPX, trans = g->flags & BB_TRANSLUCENT;
    const int post = owf.night || owf.tint_level;
    const uint8_t *nmap = owf.night_map, *tmap = owf.tint_map;
    uint8_t lut[256];
    uint32_t rowb[256 / 4];
    uint8_t *rb = (uint8_t *)rowb;
    int u, v, ri, ry = 0;
    for (u = 0; u < g->w; u++) {
        int t = (int)(((uint32_t)u * inv) >> 16);
        lut[u] = (uint8_t)(flip ? img->w - 1 - t : t);
    }
    for (ri = 0; ri < L->nr; ri++) {
        const int rows = L->rh[ri];
        for (v = ry; v < ry + rows; v++) {
            int ci, cx = 0, base;
            memset(rowb, 0, L->wt);
            if (v < g->h) {
                const uint8_t *src = img->pix + (((uint32_t)v * inv) >> 16) * img->w;
                const int sy = g->y0 + v;
                const uint32_t mk = ((unsigned)sy < SCREEN_H) ? rm[sy] : 0;
                const uint8_t *by = ow_bayer + ((sy & 3) << 2);
                const int par = (g->x0 + sy) & 1;
                if (!mk && !trans && !post) {
                    for (u = 0; u < g->w; u++) {
                        uint32_t t = src[lut[u]];
                        if (t) rb[u] = cm[t];
                    }
                } else {
                    for (u = 0; u < g->w; u++) {
                        uint32_t t = src[lut[u]], c;
                        if (!t) continue;
                        if (trans && ((u ^ par) & 1)) continue;
                        if (mk && ((mk >> ((o + u) >> 4)) & 1)) continue;
                        c = cm[t];
                        if (post) {
                            int bv = by[(g->x0 + u) & 3];
                            if (bv < owf.night) c = nmap[c];
                            if (bv < owf.tint_level) c = tmap[c];
                        }
                        rb[u] = (uint8_t)c;
                    }
                }
            }
            /* the row's 8-pixel groups into their tiles */
            base = b->t0 + (ry >> 3) * (L->wt >> 3);
            for (ci = 0; ci < L->nc; ci++) {
                const int cw = L->cw[ci];
                int tx;
                for (tx = 0; tx < cw >> 3; tx++) {
                    const int tile = base + (rows >> 3) * (cx >> 3) + ((v - ry) >> 3) * (cw >> 3) + tx;
                    vu32 *d = (vu32 *)(BB_VRAM + (uint32_t)tile * 64u + (uint32_t)((v & 7) << 3));
                    const uint32_t *s = &rowb[(cx >> 2) + tx * 2];
                    d[0] = s[0];
                    d[1] = s[1];
                }
                cx += cw;
            }
        }
        ry += rows;
    }
}

/* as OBJs: 1, or 0 (the canvas draws it) */
static int place(BbTail *T, const Spr *g, uint32_t allow, int budget, uint32_t *rm, uint8_t *line)
{
    Lay L;
    uint32_t k[5], oh = 0, fk;
    int i, y, ya, yb, pieces, bi = -1, o = 0, cxa = 0, wu;
    if (g->w > 255 || g->h > 255 || T->nput >= BB_PUT_N) return 0;
    layout(g->w, g->h, &L);
    /* entries and the per-line cycle budget (whole pieces on the lines) */
    pieces = L.nc * L.nr;
    if (T->ent_n + pieces > OBJ_BB_ENT_N) return 0;
    wu = L.wt >> 3;
    ya = DOOMO_MAX(g->y0, 0);
    yb = DOOMO_MIN(g->y0 + L.ht, SCREEN_H);
    for (y = ya; y < yb; y++)
        if (line[y] + wu > budget) return 0;
    /* the place key: everything the tiles depend on, by the billboard's
     * world position instead of its occlusion mask (the mask is a function
     * of the position, its map row and the lift grid). A block found by it
     * skips the mask and the content key: a still billboard costs a lookup.
     * (A hash: equal keys of different content would need a 32-bit
     * collision between two billboards of one frame.) */
    {
        const DoomoImage *img = g->img;
        const uint32_t pix = (uint32_t)(uintptr_t)img->pix;
        uint32_t h = 2166136261u;
        h = (h ^ pix) * 16777619u;
        h = (h ^ (img->uid ? img->uid : (pix >= 0x08000000u ? 0u : content_hash(img)))) * 16777619u;
        h = (h ^ (uint32_t)(g->x0 + cv.sx) ^ ((uint32_t)(g->y0 + cv.sy) << 16)) * 16777619u;
        h = (h ^ (uint32_t)(g->x0 & 3) ^ ((uint32_t)(g->y0 & 3) << 2) ^ ((uint32_t)g->lvl << 4) ^
             ((uint32_t)g->s << 8) ^ ((uint32_t)(g->flags & (BB_FLIPX | BB_TRANSLUCENT)) << 12) ^
             ((uint32_t)(uint16_t)g->row << 16)) * 16777619u;
        h = (h ^ ((owf.night || owf.tint_level) ? ow_tint_state() | 0x80000000u : 0u)) * 16777619u;
        h = (h ^ bb.lift ^ ((uint32_t)img->w << 8) ^ ((uint32_t)img->h << 20)) * 16777619u;
        fk = h | 1u;
        for (i = 0; i < BB_BLK_N; i++)
            if (T->blk[i].live && T->blk[i].fk == fk) { bi = i; goto found; }
    }
    /* the occluders' mask over the rows on screen (rm by screen row) */
    {
        int cy0 = DOOMO_MAX(g->y0, 0), cy1 = DOOMO_MIN(g->y0 + g->h, SCREEN_H);
        int cx0 = DOOMO_MAX(g->x0, 0), cx1 = DOOMO_MIN(g->x0 + g->w, SCREEN_W);
        if (cy0 < cy1 && cx0 < cx1 && occ_any(cx0, cy0, cx1, cy1, g->row)) {
            int cxb = (g->x0 + g->w - 1 + cv.sx) >> 4;
            cxa = (g->x0 + cv.sx) >> 4;
            o = (g->x0 + cv.sx) & 15;
            for (y = cy0; y < cy1; y++) {
                uint32_t m = occ_row(y, cxa, cxb, g->row);
                rm[y] = m;
                if (m) oh = (oh ^ m ^ ((uint32_t)(y - g->y0) << 24)) * 16777619u + 1u;
            }
            if (oh) oh = (oh ^ ((uint32_t)o << 27)) | 1u;   /* 0 = no occluder */
        } else {
            for (y = cy0; y < cy1; y++) rm[y] = 0;
        }
    }
    {
        const DoomoImage *img = g->img;
        const int post = owf.night || owf.tint_level, trans = g->flags & BB_TRANSLUCENT;
        k[0] = img->uid ? img->uid : (uint32_t)(uintptr_t)img->pix >= 0x08000000u ? 0u : content_hash(img);
        k[1] = (uint32_t)(uintptr_t)img->pix;
        k[2] = (uint32_t)g->lvl | ((uint32_t)g->s << 4) | ((uint32_t)(g->flags & BB_FLIPX) << 7) |
               ((uint32_t)(trans ? 1 : 0) << 8) | ((uint32_t)(trans ? (g->x0 + g->y0) & 1 : 0) << 9) |
               ((uint32_t)(post ? ((g->x0 & 3) | ((g->y0 & 3) << 2) | 16) : 0) << 10) |
               ((uint32_t)img->w << 16) | ((uint32_t)img->h << 24);
        k[3] = post ? ow_tint_state() | 0x80000000u : 0;
        k[4] = oh;
    }
    for (i = 0; i < BB_BLK_N; i++) {
        const BbBlk *b = &T->blk[i];
        if (b->live && b->k[0] == k[0] && b->k[1] == k[1] && b->k[2] == k[2] && b->k[3] == k[3] &&
            b->k[4] == k[4]) { bi = i; break; }
    }
    if (bi < 0) {
        int n = wu * (L.ht >> 3), t0;
        BbBlk *b;
        for (i = 0; i < BB_BLK_N && T->blk[i].live; i++) {}
        if (i == BB_BLK_N) {
            if (!evict_lru(T)) return 0;
            for (i = 0; i < BB_BLK_N && T->blk[i].live; i++) {}
        }
        if ((t0 = units_alloc(T, n, allow)) < 0) return 0;
        b = &T->blk[i];
        memcpy(b->k, k, sizeof(k));
        b->t0 = (uint8_t)t0;
        b->n1 = (uint8_t)(n - 1);
        b->w1 = (uint8_t)(g->w - 1);
        b->h1 = (uint8_t)(g->h - 1);
        b->nc = L.nc;
        b->nr = L.nr;
        b->cwc = lay_code(L.cw, L.nc);
        b->rhc = lay_code(L.rh, L.nr);
        b->live = 1;
        bits_set(T->used, t0, n, 1);
        upload(g, b, &L, rm, o);
        bi = i;
    }
    T->blk[bi].fk = fk;
found:
    T->blk[bi].last = cv.frame;
    for (y = ya; y < yb; y++) line[y] = (uint8_t)(line[y] + wu);
    T->ent_n = (uint16_t)(T->ent_n + pieces);
    {
        BbPut *p = &T->put[T->nput++];
        p->x = (int16_t)g->x0;
        p->y = (int16_t)g->y0;
        p->blk = (uint8_t)bi;
        p->gone = 0;
    }
    return 1;
}

/* a shadow row [a0, a1) x y in the canvas: the pixel under it through cm
 * (the canvas, else the topmost billboard before put pb, else the world),
 * left transparent where a billboard from put pb on is opaque (its OBJ is
 * drawn over the shadow) or a raised cell drawn later covers it */
BB_ARM static void shadow_row(BbTail *T, int a0, int a1, int y, int row, int pb, const uint8_t *cm)
{
    uint8_t later[SCREEN_W], under[SCREEN_W], cv_px[SCREEN_W];
    int j, x, n, cxa = 0;
    uint32_t mk = 0;
    a0 = DOOMO_MAX(a0, 0);
    a1 = DOOMO_MIN(a1, SCREEN_W);
    if (y < 0 || y >= SCREEN_H || a0 >= a1) return;
    n = a1 - a0;
    for (x = 0; x < n; x++) later[x] = under[x] = 0;
    for (j = T->nput - 1; j >= 0; j--) {
        const BbPut *p = &T->put[j];
        const BbBlk *b = &T->blk[p->blk];
        Lay L;
        if (p->gone || y < p->y || y > p->y + b->h1 || a1 <= p->x || a0 > p->x + b->w1) continue;
        lay_of(b, &L);
        put_row(p, b, &L, y, a0, a1, j >= pb ? later : under);
    }
    if (occ_any(a0, y, a1, y + 1, row)) {
        cxa = (a0 + cv.sx) >> 4;
        mk = occ_row(y, cxa, (a1 - 1 + cv.sx) >> 4, row);
    }
    canvas_row(y, a0, a1, cv_px);
    for (x = a0; x < a1; x++) {
        const int i = x - a0;
        uint32_t c;
        if (later[i] || (mk && ((mk >> (((x + cv.sx) >> 4) - cxa)) & 1))) {
            cv_px[i] = 0;                       /* leave it */
            continue;
        }
        c = cv_px[i];
        if (!c) c = under[i];
        if (!c) {                               /* the world (BG2) */
            const int wx = x + (cv.sx & 7), wy = y + (cv.sy & 7);
            const uint32_t e = cv_meta->wmap[(wy >> 3) * CV_WMAP_W + (wx >> 3)];
            c = *(const vu8 *)(CV_VRAM + e * 64u + (uint32_t)(((wy & 7) << 3) | (wx & 7)));
        }
        cv_px[i] = cm[c];
    }
    canvas_put_row(y, a0, a1, cv_px);
}

/* ---- the frame's billboards -------------------------------------------------------- */
extern int16_t ow_order[128];   /* overworld.c */
#define order ow_order
uint32_t obj_bb_reserved(void);  /* obj.c: the squash / figure regions */

static BbTail *tail_reset(void)
{
    uint32_t bytes;
    BbTail *T = (BbTail *)(void *)iwram_ovl_2d_tail(&bytes);
    if (bytes < sizeof(BbTail)) return NULL;    /* (gba.ld asserts it) */
    memset(T, 0, sizeof(BbTail));
    bb.t = T;
    bb.gen = iwram_ovl_loads();
    bb.r3d = gba_objvram_r3d_frame();
    return T;
}

void ow_tiles_sprites(const DoomoOverworldView *v, int ty0, int ty1)
{
    int n = v->sprites ? DOOMO_MIN(v->sprite_count, 128) : 0, i, si = 0, r, nsp = 0, budget;
    int16_t rowof[128];
    uint8_t kind[128], pb[128];          /* 0 none, 1 OBJ, 2 canvas */
    int16_t spl[16][4];                   /* the canvas billboards' boxes */
    uint32_t rm[SCREEN_H], res = obj_bb_reserved(), allow;
    uint8_t line[SCREEN_H];               /* OBJ cycles / 8 per line */
    BbTail *T = tail();
    /* (the tile frame loaded the 2D group: the slot's tail is ours) */
    if (!T || bb.r3d != gba_objvram_r3d_frame()) T = tail_reset();   /* the 3D view had OBJ VRAM */
    for (i = 0; i < n; i++) {                   /* sort by feet y (stable) */
        int j = i;
        while (j > 0 && v->sprites[order[j - 1]].y > v->sprites[i].y) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = (int16_t)i;
    }
    if (T) {
        T->nput = 0;
        T->ent_n = 0;
    }
    memset(line, 0, sizeof(line));
    {                                     /* the lift grid (place keys) */
        const CvMeta *M = cv_meta;
        const uint8_t *l = &M->lift[0][0];
        uint32_t h = 2166136261u ^ (uint16_t)M->gx0 ^ ((uint32_t)(uint16_t)M->gy0 << 16);
        for (i = 0; i < (int)sizeof(M->lift); i++) h = (h ^ l[i]) * 16777619u;
        bb.lift = h ^ M->gw ^ ((uint32_t)M->gh << 8);
    }
    allow = ~res & 15u;
    budget = OBJ_LINE_CYCLES - LINE_MARGIN;
    for (i = 0; i < 4; i++) if ((res >> i) & 1) budget -= SQUASH_CYCLES;
    budget >>= 3;
    /* pass 1, Mode 4 order: after raised row r, the billboards with feet
     * y < (r + 1) * cell; the rest after the last row. OBJ or canvas. */
    for (r = ty0; r <= ty1 + 1 && si < n; r++) {
        int32_t row_bottom = (int32_t)(r + 1) * DOOMO_CELL_UNITS;
        while (si < n && (r > ty1 || (v->sprites[order[si]].y >> 16) < row_bottom)) {
            Spr g;
            int k = 0, q;
            pb[si] = (uint8_t)(T ? T->nput : 0);
            rowof[si] = (int16_t)(r <= ty1 ? r : CV_ROW_NONE);
            if (geom(&v->sprites[order[si]], &g)) {
                g.row = rowof[si];
                k = 3;                          /* only its shadow is on screen */
                if (g.x0 < SCREEN_W && g.x0 + g.w > 0 && g.y0 < SCREEN_H && g.y0 + g.h > 0) {
                    for (q = 0; q < nsp; q++)
                        if (g.x0 < spl[q][2] && g.x0 + g.w > spl[q][0] && g.y0 < spl[q][3] &&
                            g.y0 + g.h > spl[q][1])
                            break;
                    if (T && q == nsp && place(T, &g, allow, budget, rm, line)) {
                        k = 1;
                    } else {
                        k = 2;
                        if (nsp < 16) {
                            spl[nsp][0] = (int16_t)g.x0; spl[nsp][1] = (int16_t)g.y0;
                            spl[nsp][2] = (int16_t)(g.x0 + g.w); spl[nsp][3] = (int16_t)(g.y0 + g.h);
                            nsp++;
                        } else {                /* 16 already: every later one spills */
                            spl[15][0] = -32768; spl[15][1] = -32768; spl[15][2] = 32767; spl[15][3] = 32767;
                        }
                    }
                }
            }
            kind[si] = (uint8_t)k;
            si++;
        }
    }
    /* pass 2, the same order: the canvas part (shadows, canvas billboards) */
    for (i = 0; i < si; i++) {
        Spr g;
        if (!kind[i] || !geom(&v->sprites[order[i]], &g)) continue;
        if (g.shadow) {
            const uint8_t *cm8 = doomo_colormap[8];
            if (T) {
                shadow_row(T, g.cx - g.half, g.cx + g.half + 1, g.fy, rowof[i], pb[i], cm8);
                shadow_row(T, g.cx - g.half + 1, g.cx + g.half, g.fy - 1, rowof[i], pb[i], cm8);
            }
        }
        if (kind[i] == 2)
            cv_sprite(g.img, g.x0, g.y0, g.s, g.flags, doomo_colormap[g.lvl], rowof[i]);
    }
}

/* ---- into the canvas, under what it holds ------------------------------------------- */
static void under_put(const BbPut *p, const BbBlk *b)
{
    Lay L;
    uint8_t row[256], cvr[256];
    int y, y0 = DOOMO_MAX(p->y, 0), y1 = DOOMO_MIN(p->y + b->h1 + 1, SCREEN_H);
    int a0 = DOOMO_MAX(p->x, 0), a1 = DOOMO_MIN(p->x + b->w1 + 1, SCREEN_W), x;
    if (a0 >= a1) return;
    lay_of(b, &L);
    for (y = y0; y < y1; y++) {
        int any = 0;
        memset(row, 0, (size_t)(a1 - a0));
        put_row(p, b, &L, y, a0, a1, row);
        for (x = 0; x < a1 - a0; x++) any |= row[x];
        if (!any) continue;
        canvas_row(y, a0, a1, cvr);
        for (x = 0; x < a1 - a0; x++) if (cvr[x]) row[x] = 0;   /* the canvas stays on top */
        canvas_put_row(y, a0, a1, row);
    }
}

/* the shown billboards over [x0, x1) x [y0, y1), and every later one that
 * meets one of them, into the canvas (back to front, under its pixels) */
void bb_flatten(int x0, int y0, int x1, int y1)
{
    BbTail *T = tail();
    uint8_t mark[BB_PUT_N];
    int j, m, any = 0;
    if (cv.mode == CV_OFF || !T) return;
    for (j = 0; j < T->nput; j++) {
        const BbPut *p = &T->put[j];
        const BbBlk *b = &T->blk[p->blk];
        mark[j] = !p->gone && p->x < x1 && p->x + b->w1 >= x0 && p->y < y1 && p->y + b->h1 >= y0;
        any |= mark[j];
    }
    if (!any) return;
    for (j = 0; j < T->nput; j++)
        if (mark[j])
            for (m = j + 1; m < T->nput; m++) {
                const BbPut *a = &T->put[j], *c = &T->put[m];
                const BbBlk *ab = &T->blk[a->blk], *cb = &T->blk[c->blk];
                if (!c->gone && a->x <= c->x + cb->w1 && c->x <= a->x + ab->w1 && a->y <= c->y + cb->h1 &&
                    c->y <= a->y + ab->h1)
                    mark[m] = 1;
            }
    for (j = T->nput - 1; j >= 0; j--)
        if (mark[j]) {
            under_put(&T->put[j], &T->blk[T->put[j].blk]);
            T->put[j].gone = 1;
        }
}

void bb_drop(void)
{
    BbTail *T = tail();
    if (T) T->nput = 0;
}

/* ---- OAM (obj.c obj_pre_flip): the puts as entries BB_ENT0.., the last
 * drawn at the lowest entry, into buf (entry BB_ENT0 first); returns the
 * entries written. Remembers the tiles the screen shows from the flip on. */
int bb_emit(uint16_t *buf, int tiles)
{
    BbTail *T = tail();
    int dx = 0, dy = 0, s, cxp, cyp, e = 0, j, edge = 0;
    memset(bb.shown, 0, sizeof(bb.shown));
    if (tiles && T && T->nput) {
        int shake = postfx_xform(&dx, &dy, &s, &cxp, &cyp);
        if (!shake) dx = dy = 0;
        for (j = T->nput - 1; j >= 0; j--) {
            const BbPut *p = &T->put[j];
            const BbBlk *b = &T->blk[p->blk];
            Lay L;
            int ri, ci, ry = 0;
            if (p->gone) continue;
            bits_set(bb.shown, b->t0, b->n1 + 1, 1);
            lay_of(b, &L);
            for (ri = 0; ri < L.nr; ri++) {
                int cx = 0;
                for (ci = 0; ci < L.nc; ci++) {
                    const int pw = L.cw[ci], ph = L.rh[ri];
                    const int x = p->x + cx, y = p->y + ry;
                    const int tile = b->t0 + (ry >> 3) * (L.wt >> 3) + (ph >> 3) * (cx >> 3);
                    cx += pw;
                    if (x >= SCREEN_W || x + pw <= 0 || y >= SCREEN_H || y + ph <= 0 || e >= OBJ_BB_ENT_N)
                        continue;
                    if (x < 0 || y < 0 || x + pw > SCREEN_W || y + ph > SCREEN_H) edge = 1;
                    {
                        uint32_t ss = shape_size(pw, ph);
                        uint16_t *o = buf + e * 4;
                        o[0] = (uint16_t)(((y + dy) & 0xFF) | 0x2000 | (ss & 0xC000));
                        o[1] = (uint16_t)(((x + dx) & 0x1FF) | (ss >> 16));
                        o[2] = (uint16_t)((512 + 2 * tile) | (1 << 10));
                        o[3] = 0;
                        e++;
                    }
                }
                ry += L.rh[ri];
            }
        }
    }
    bb_edge = edge && (dx | dy);
    if (T) T->nput = 0;
    return e;
}
