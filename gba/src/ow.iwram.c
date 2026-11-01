/* ow.iwram.c - overworld pixel loops, ARM in IWRAM (owner: gba).
 * Mode 4 VRAM only takes halfword writes: odd edges read-modify-write,
 * the middle is copied in pixel pairs (halfword loads when the source is
 * aligned like the destination). */
#include "ow_int.h"

#define LINE 120

/* VRAM is accessed through volatile halfword pointers in read-modify-
 * writes: GCC would otherwise turn "(*p & 0xFF00) | c" into a byte store,
 * and VRAM writes a byte to BOTH halves of its halfword. */
typedef volatile uint16_t vu16_t;

static inline void put_lo(uint16_t *p, uint32_t c)
{
    vu16_t *q = (vu16_t *)p;
    *q = (uint16_t)((*q & 0xFF00) | c);
}

static inline void put_hi(uint16_t *p, uint32_t c)
{
    vu16_t *q = (vu16_t *)p;
    *q = (uint16_t)((*q & 0x00FF) | (c << 8));
}

/* one screen row: pixels [x0, x1) from src */
static void row_out(uint16_t *row, int x0, int x1, const uint8_t *src)
{
    int n;
    if (x0 & 1) {
        put_hi(row + (x0 >> 1), src[0]);
        x0++;
        src++;
    }
    n = (x1 - x0) & ~1;
    if (n > 0) ow_out(row + (x0 >> 1), src, n);
    if ((x1 - x0) & 1) put_lo(row + ((x1 - 1) >> 1), src[n]);
}

void ow_blit(int x0, int x1, int y0, int y1, const uint8_t *src, int sstride)
{
    int y;
    for (y = y0; y < y1; y++, src += sstride)
        row_out(gba_back + y * LINE, x0, x1, src);
}

/* the ground: screen rows [y0, y1) of one cell row. tiles[k] is the lit
 * 16x16 tile of the k-th visible cell, o the pixel of the first tile at
 * the viewport's left edge, r0 the tile row of y0 */
static uint32_t line[(240 + 48) / 4];

void ow_ground(const uint8_t *const *tiles, int ntiles, int o, int y0, int y1, int r0)
{
    int y, k, r = r0;
    for (y = y0; y < y1; y++, r++) {
        uint32_t *l = line;
        for (k = 0; k < ntiles; k++, l += 4) {
            const uint32_t *t = (const uint32_t *)(tiles[k] + r * 16);
            l[0] = t[0]; l[1] = t[1]; l[2] = t[2]; l[3] = t[3];
        }
        if (!((owf.ox | o | owf.ow) & 1)) {
            /* everything halfword aligned (the usual case): DMA the row */
            REG_DMA3SAD = (uint32_t)((const uint8_t *)line + o);
            REG_DMA3DAD = (uint32_t)(gba_back + y * LINE + (owf.ox >> 1));
            REG_DMA3CNT = (uint32_t)(owf.ow >> 1) | DMA_16 | DMA_ENABLE;
        } else {
            row_out(gba_back + y * LINE, owf.ox, owf.ox + owf.ow, (const uint8_t *)line + o);
        }
    }
}

void ow_shade(int x0, int x1, int y0, int y1, const uint8_t *cm)
{
    int y;
    for (y = y0; y < y1; y++) {
        uint16_t *row = gba_back + y * LINE;
        int x = x0;
        if (x & 1) {
            vu16_t *p = (vu16_t *)(row + (x >> 1));
            uint32_t v = *p;
            *p = (uint16_t)((v & 0x00FF) | (cm[v >> 8] << 8));
            x++;
        }
        for (; x + 1 < x1; x += 2) {
            uint16_t *p = row + (x >> 1);
            uint32_t v = *p;
            *p = (uint16_t)(cm[v & 0xFF] | (cm[v >> 8] << 8));
        }
        if (x < x1) {
            vu16_t *p = (vu16_t *)(row + (x >> 1));
            uint32_t v = *p;
            *p = (uint16_t)((v & 0xFF00) | cm[v & 0xFF]);
        }
    }
}

static inline uint8_t post(uint8_t c, int x, int y)
{
    int b = ow_bayer[((y & 3) << 2) | (x & 3)];
    if (b < owf.night) c = owf.night_map[c];
    if (b < owf.tint_level) c = owf.tint_map[c];
    return c;
}

/* one screen pixel pair at d: texels a (even x) and b, 0 = transparent */
static inline void put_pair(uint16_t *d, uint32_t a, uint32_t b, const uint8_t *qa,
                            const uint8_t *qb)
{
    if (a) {
        if (b) *d = (uint16_t)(qa[a] | (qb[b] << 8));
        else put_lo(d, qa[a]);
    } else if (b) {
        put_hi(d, qb[b]);
    }
}

/* A sprite at screen (x0, y0), integer scale s, clipped to the viewport;
 * texel of pixel x = src[(x - x0) * inv >> 16] (flip: mirrored). Every
 * pixel is one lookup in its 4x4 position's composed table
 * (owf_sprite_luts). (G2) At scale 1, not mirrored (nearly every sprite),
 * the rows' middles go to ow_spr_rows: four texels per word read (ROM is a
 * 16-bit bus: one word instead of four byte reads), transparent words
 * skipped, opaque pairs stored without a read. */
void ow_sprite(const DoomoImage *img, int x0, int y0, int s, int flags, const uint8_t *cm)
{
    const int w = img->w * s, h = img->h * s;
    int cx0 = x0, cx1 = x0 + w, cy0 = y0, cy1 = y0 + h, y;
    const int trans = flags & BB_TRANSLUCENT;
    const int flip = flags & BB_FLIPX;
    const uint32_t inv = 65536u / (uint32_t)s + 1u;     /* x / s = (x * inv) >> 16 */
    const uint8_t *lc[4];
    int x;
    if (cx0 < owf.ox) cx0 = owf.ox;
    if (cx1 > owf.ox + owf.ow) cx1 = owf.ox + owf.ow;
    if (cy0 < owf.oy) cy0 = owf.oy;
    if (cy1 > owf.oy + owf.oh) cy1 = owf.oy + owf.oh;
    if (cx0 >= cx1 || cy0 >= cy1 || cx1 - cx0 > 256) return;
    if (owf.luts) owf_sprite_luts(cm, lc);
    else lc[0] = lc[1] = lc[2] = lc[3] = cm;    /* the house: no night, no tint */
    if (s == 1 && !flip) {
        /* the middle columns [xs, xe) in ow_spr_rows (ow_loops.s), the odd
         * edge columns here; translucent: only pixels with (x + y) even */
        const int iw = img->w;
        const int xs = (cx0 + 1) & ~1, xe = cx1 & ~1;
        int k;
        if (xe > xs) {
            OwSprJob j;
            j.dst = gba_back + cy0 * LINE + (xs >> 1);
            j.src = img->pix + (cy0 - y0) * iw + (xs - x0);
            j.n = xe - xs;
            j.rows = cy1 - cy0;
            j.stride = iw;
            j.phase = (uint32_t)cy0 & 3u;
            j.tmask[0] = trans ? 0x00FF00FFu : 0xFFFFFFFFu;
            j.tmask[1] = trans ? 0xFF00FF00u : 0xFFFFFFFFu;
            for (k = 0; k < 16; k++)
                j.q[k >> 2][k & 3] = lc[owf.cls[(k & ~3) | ((xs + k) & 3)]];
            ow_spr_rows(&j);
        }
        if ((cx0 | cx1) & 1) {
            for (y = cy0; y < cy1; y++) {
                const uint8_t *srow = img->pix + (y - y0) * iw;
                uint16_t *row = gba_back + y * LINE;
                const uint8_t *cl = owf.cls + ((y & 3) << 2);
                if (cx0 & 1) {
                    uint32_t t = srow[cx0 - x0];
                    if (t && !(trans && ((cx0 + y) & 1))) put_hi(row + (cx0 >> 1), lc[cl[cx0 & 3]][t]);
                }
                if (cx1 & 1) {
                    int xl = cx1 - 1;
                    uint32_t t = srow[xl - x0];
                    if (t && !(trans && ((xl + y) & 1))) put_lo(row + (xl >> 1), lc[cl[xl & 3]][t]);
                }
            }
        }
        return;
    }
    /* scaled or mirrored: texel column of every screen pixel (pairs
     * aligned to even x) */
    {
        uint8_t lut[256 + 2];                           /* texel index per pixel */
        int xa = cx0 & ~1, xb = (cx1 + 1) & ~1;
        for (x = xa; x < xb; x++) {
            int tx;
            if (x < cx0 || x >= cx1) { lut[x - xa] = 0xFF; continue; }
            tx = (int)(((uint32_t)(x - x0) * inv) >> 16);
            if (flip) tx = img->w - 1 - tx;
            lut[x - xa] = (uint8_t)tx;
        }
        for (y = cy0; y < cy1; y++) {
            const uint8_t *src = img->pix + (((uint32_t)(y - y0) * inv) >> 16) * img->w;
            uint16_t *row = gba_back + y * LINE + (xa >> 1);
            const uint8_t *cl = owf.cls + ((y & 3) << 2);
            const uint8_t *q0 = lc[cl[xa & 3]], *q1 = lc[cl[(xa + 1) & 3]];
            const uint8_t *q2 = lc[cl[(xa + 2) & 3]], *q3 = lc[cl[(xa + 3) & 3]];
            const uint8_t *l = lut;
            int k, npairs = (xb - xa) >> 1;
            for (k = 0; k < npairs; k++, l += 2, row++) {
                uint32_t a = l[0] != 0xFF ? src[l[0]] : 0;
                uint32_t b = l[1] != 0xFF ? src[l[1]] : 0;
                if (trans) {
                    int px = xa + 2 * k;
                    if ((px + y) & 1) a = 0; else b = 0;
                }
                put_pair(row, a, b, (k & 1) ? q2 : q0, (k & 1) ? q3 : q1);
            }
        }
    }
}

/* front face of a block: rows of 16 texels sampled with stride 4 from
 * texture row (y * 64 / h), lit through cm (+ night / tint dither) */
void ow_front(int px, int sy0, int h, int y0, int y1, const uint8_t *tp, uint8_t avg,
              const uint8_t *cm)
{
    uint8_t lineb[16];
    int y, x;
    int xa = px, xb = px + 16;
    const int post_on = owf.night || owf.tint_level;
    if (xa < owf.ox) xa = owf.ox;
    if (xb > owf.ox + owf.ow) xb = owf.ox + owf.ow;
    if (xa >= xb) return;
    for (y = y0; y < y1; y++) {
        int sy = sy0 + y;
        const uint8_t *tr = tp ? tp + (((y << 6) / h) & 63) * 64 + 2 : 0;
        for (x = 0; x < 16; x++) {
            uint8_t c = cm[tr ? tr[x << 2] : avg];
            lineb[x] = post_on ? post(c, px + x, sy) : c;
        }
        row_out(gba_back + sy * LINE, xa, xb, lineb + (xa - px));
    }
}
