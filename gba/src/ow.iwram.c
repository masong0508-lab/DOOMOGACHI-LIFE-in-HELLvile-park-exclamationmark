/* ow.iwram.c - overworld pixel loops, ARM in IWRAM (owner: gba).
 * Mode 4 VRAM only takes halfword writes: odd edges read-modify-write,
 * the middle is copied in pixel pairs (halfword loads when the source is
 * aligned like the destination). */
#include "ow_int.h"

#define LINE 120

/* ow_loops.s reads OwSprJob at these offsets */
DOOMO_STATIC_ASSERT(offsetof(OwSprJob, phase) == 20 && offsetof(OwSprJob, q) == 24 &&
                    offsetof(OwSprJob, head) == 88 && offsetof(OwSprJob, tail) == 92,
                    ow_spr_job_layout);

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

__attribute__((optimize("Os")))
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
    /* x / s = (x * inv) >> 16 (s == 1, nearly every sprite: no software divide) */
    const uint32_t inv = s == 1 ? 65537u : 65536u / (uint32_t)s + 1u;
    const uint8_t *lc[4];
    int x;
    if (cx0 < owf.ox) cx0 = owf.ox;
    if (cx1 > owf.ox + owf.ow) cx1 = owf.ox + owf.ow;
    if (cy0 < owf.oy) cy0 = owf.oy;
    if (cy1 > owf.oy + owf.oh) cy1 = owf.oy + owf.oh;
    if (cx0 >= cx1 || cy0 >= cy1 || cx1 - cx0 > 256) return;
    if (owf.luts) owf_sprite_luts(cm, lc);
    else lc[0] = lc[1] = lc[2] = lc[3] = cm;    /* the house: no night, no tint */
    {
        /* every row in ow_spr_rows (ow_loops.s), from the even column xa to
         * xb, the clipped odd edge columns masked off (head / tail). At
         * scale 1, not mirrored, opaque (nearly every sprite), all rows in
         * one call straight from the image; else row by row through a row
         * buffer, from col[]: the texel column of each screen pixel, with
         * the translucency checker applied (only pixels with x + y even) */
        const int iw = img->w;
        const int xa = cx0 & ~1, xb = (cx1 + 1) & ~1;
        uint8_t col[256 + 2];
        uint32_t buf[(256 + 2) / 4 + 3];            /* + what ow_spr_rows over-reads */
        OwSprJob j;
        int k;
        j.n = xb - xa;
        j.stride = iw;
        j.head = (cx0 & 1) ? 0xFFu : 0u;
        j.tail = (j.n & 2) ? 0x0000FFFFu : 0xFFFFFFFFu;
        if (cx1 & 1) j.tail &= ~(0xFFu << ((j.n & 2) ? 8 : 24));
        {
            /* q[r][k] = lc[cls[r * 4 + ((xa + k) & 3)]]: xa is even, so (xa + k) & 3 = k ^ (xa & 2) */
            const uint8_t **qq = &j.q[0][0];
            const int b = xa & 2;
            for (k = 0; k < 16; k++) qq[k] = lc[owf.cls[k ^ b]];
        }
        if (s == 1 && !flip && !trans) {
            j.dst = gba_back + cy0 * LINE + (xa >> 1);
            j.src = img->pix + (cy0 - y0) * iw + (xa - x0);
            j.rows = cy1 - cy0;
            j.phase = (uint32_t)cy0 & 3u;
            ow_spr_rows(&j);
            return;
        }
        /* (V4b) mirrored, not scaled (a few billboards a frame): the column gather below costs
         * ~15 cycles a pixel, ~2/3 of ow_sprite's time on the invaded map. Pixel x shows texel
         * iw - 1 - (x - x0), so a row is the source row read backwards from there, four texels
         * a word read (ow_flip_row); the edge columns it reads outside the image are the ones
         * head / tail mask off */
        const int mirror = s == 1 && flip && !trans;
        const int moff = iw - 1 - (xa - x0);
        if (!mirror)
            for (x = xa; x < xb; x++) {
                /* the masked-off edge pixels just repeat a neighbour's column */
                int tx = (int)(((uint32_t)(DOOMO_CLAMP(x, cx0, cx1 - 1) - x0) * inv) >> 16);
                col[x - xa] = (uint8_t)(flip ? iw - 1 - tx : tx);
            }
        for (y = cy0; y < cy1; y++) {
            uint8_t *d = (uint8_t *)buf;
            if (mirror) {
                ow_flip_row(d, img->pix + (y - y0) * iw + moff, j.n);
            } else {
                const uint8_t *srow = img->pix + (int)(((uint32_t)(y - y0) * inv) >> 16) * iw;
                for (k = 0; k < j.n; k++) d[k] = srow[col[k]];
                if (trans)
                    for (k = (xa + y + 1) & 1; k < j.n; k += 2) d[k] = 0;
            }
            j.dst = gba_back + y * LINE + (xa >> 1);
            j.src = d;
            j.rows = 1;
            j.phase = (uint32_t)y & 3u;
            ow_spr_rows(&j);
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
