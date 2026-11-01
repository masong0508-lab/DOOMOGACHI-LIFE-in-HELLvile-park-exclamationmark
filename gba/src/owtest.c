/* owtest.c - the overworld sprite blit against its reference (owner: gba,
 * G2). GBA_HC_OW_SPRITE_TEST (hcmd.c): random cases, each drawn by the
 * reference (ref_sprite below: the per-pixel ow_sprite of the G1 baseline,
 * one post() per pixel) and by ow_sprite (ow.iwram.c + ow_loops.s: the
 * composed tables, the word reads, the head / tail masks, the row buffer)
 * onto the same pseudo-random back buffer; the whole back buffer must
 * hash the same. Random: image size 1..48,
 * scale 1..4, any source alignment, transparent density, mirrored /
 * translucent, any position (clipped on every side), full-screen or odd
 * viewports, night 0..16, view tint 0..15 with any tint map, any
 * colormap level, and the house's way in (no tables: owf.luts NULL).
 * The images and the table cache sit in the overworld's arena region,
 * which is then invalidated (rebuilt on the next overworld frame).
 * arg = set << 16 | n: n cases of random set `set` (a harness wait ends
 * after 20 s of emulated time without a tick: ~400 cases per post).
 * Logs "ow sprite test: set S, N cases, M mismatches"; returns M.
 */
#include "gba_plat.h"
#include "ow_int.h"
#include "doomo/palette.h"

#define LINE 120
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

static inline uint8_t post(uint8_t c, int x, int y)
{
    int b = ow_bayer[((y & 3) << 2) | (x & 3)];
    if (b < owf.night) c = owf.night_map[c];
    if (b < owf.tint_level) c = owf.tint_map[c];
    return c;
}

/* the G1 baseline's ow_sprite, verbatim but for its name */
static void ref_sprite(const DoomoImage *img, int x0, int y0, int s, int flags, const uint8_t *cm)
{
    const int w = img->w * s, h = img->h * s;
    int cx0 = x0, cx1 = x0 + w, cy0 = y0, cy1 = y0 + h, y;
    const int post_on = owf.night || owf.tint_level;
    const int trans = flags & BB_TRANSLUCENT;
    const int flip = flags & BB_FLIPX;
    const uint32_t inv = 65536u / (uint32_t)s + 1u;     /* x / s = (x * inv) >> 16 */
    uint8_t lut[256 + 2];                               /* texel index per pixel */
    int x;
    if (cx0 < owf.ox) cx0 = owf.ox;
    if (cx1 > owf.ox + owf.ow) cx1 = owf.ox + owf.ow;
    if (cy0 < owf.oy) cy0 = owf.oy;
    if (cy1 > owf.oy + owf.oh) cy1 = owf.oy + owf.oh;
    if (cx0 >= cx1 || cy0 >= cy1 || cx1 - cx0 > 256) return;
    if (s == 1 && !flip && !trans && !post_on) {
        /* the common case: 1:1, opaque texels through the colormap,
         * walked in VRAM halfword pairs */
        int xa = cx0 & ~1, xb = (cx1 + 1) & ~1;
        for (y = cy0; y < cy1; y++) {
            const uint8_t *src = img->pix + (y - y0) * img->w + (xa - x0);
            uint16_t *row = gba_back + y * LINE + (xa >> 1);
            int px;
            for (px = xa; px < xb; px += 2, src += 2, row++) {
                uint32_t a = (px >= cx0) ? src[0] : 0;
                uint32_t b = (px + 1 < cx1) ? src[1] : 0;
                if (!(a | b)) continue;
                if (a && b) *row = (uint16_t)(cm[a] | (cm[b] << 8));
                else if (a) put_lo(row, cm[a]);
                else put_hi(row, cm[b]);
            }
        }
        return;
    }
    /* texel column of every screen pixel (pairs aligned to even x) */
    {
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
            const uint8_t *l = lut;
            int k, npairs = (xb - xa) >> 1;
            for (k = 0; k < npairs; k++, l += 2, row++) {
                uint32_t a = l[0] != 0xFF ? src[l[0]] : 0;
                uint32_t b = l[1] != 0xFF ? src[l[1]] : 0;
                if (trans) {
                    int px = xa + 2 * k;
                    if ((px + y) & 1) a = 0; else b = 0;
                }
                if (!(a | b)) continue;
                if (a) { a = cm[a]; if (post_on) a = post((uint8_t)a, xa + 2 * k, y); }
                if (b) { b = cm[b]; if (post_on) b = post((uint8_t)b, xa + 2 * k + 1, y); }
                if (a && b) *row = (uint16_t)(a | (b << 8));
                else if (a) put_lo(row, a);
                else put_hi(row, b);
            }
        }
    }
}

static uint32_t rnd_s;
static uint32_t rnd(uint32_t n)                     /* 0..n-1, n <= 65536 */
{
    rnd_s = rnd_s * 1664525u + 1013904223u;
    return ((rnd_s >> 16) * n) >> 16;
}

/* the back buffer from seed s (the same for both draws), as words */
static void fill_back(uint32_t s)
{
    uint32_t *p = (uint32_t *)(void *)gba_back;
    int i;
    for (i = 0; i < SCREEN_W * SCREEN_H / 4; i++) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        p[i] = s;
    }
}

/* the whole back buffer: a stray write anywhere shows */
static uint32_t hash_back(void)
{
    const uint32_t *p = (const uint32_t *)(const void *)gba_back;
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < SCREEN_W * SCREEN_H / 4; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

void ow_invalidate(void);

DOOMO_STATIC_ASSERT(sizeof(OwLuts) + 8 + 48 * 48 + 8 <= GBA_BIG_OW, owtest_fits);

uint32_t ow_sprite_selftest(uint32_t arg)
{
    const uint32_t n = arg & 0xFFFFu, set = arg >> 16;
    /* the arena's overworld region: table cache, then the image */
    OwLuts *luts = (OwLuts *)(void *)gba_ow;
    uint8_t *pixbuf = gba_ow + ((sizeof(OwLuts) + 7u) & ~7u);
    DoomoImage img;
    uint32_t i, bad = 0, first_bad = 0xFFFFFFFFu;
    OwFrame saved = owf;
    gba_big_claim(GBA_BIG_BY_OW);
    rnd_s = 0x6A09E667u + set * 0x9E3779B9u;
    for (i = 0; i < n; i++) {
        int w = 1 + (int)rnd(48), h = 1 + (int)rnd(48), s, k, x0, y0, flags = 0, lvl;
        uint32_t dens = rnd(4), seed = rnd_s, ha, hb;
        const uint8_t *cm;
        uint8_t *pix = pixbuf + rnd(4);
        s = rnd(3) ? 1 : 1 + (int)rnd(4);
        if (w * s > 256) w = 256 / s;
        for (k = 0; k < w * h; k++) {
            uint32_t r = rnd(256);
            /* transparent density: none, some, most, rows of runs */
            int t = dens == 0 ? 0 : dens == 1 ? r < 60 : dens == 2 ? r < 190 : ((k / 5) & 1);
            pix[k] = t ? 0 : (uint8_t)(1 + rnd(255));
        }
        img.w = (uint16_t)w;
        img.h = (uint16_t)h;
        img.pix = pix;
        img.uid = 0;
        if (rnd(3) == 0) flags |= BB_FLIPX;
        if (rnd(5) == 0) flags |= BB_TRANSLUCENT;
        x0 = (int)rnd(SCREEN_W + 2 * 60) - 60 - (w * s) / 2;
        y0 = (int)rnd(SCREEN_H + 2 * 60) - 60 - (h * s) / 2;
        if (rnd(4) == 0) {
            owf.ox = (int)rnd(120);
            owf.oy = (int)rnd(80);
            owf.ow = 1 + (int)rnd((uint32_t)(SCREEN_W - owf.ox));
            owf.oh = 1 + (int)rnd((uint32_t)(SCREEN_H - owf.oy));
        } else {
            owf.ox = 0; owf.oy = 0; owf.ow = SCREEN_W; owf.oh = SCREEN_H;
        }
        lvl = (int)rnd(16);
        cm = doomo_colormap[lvl];
        owf.night_map = doomo_tintmap[TINT_NIGHT];
        if (rnd(6) == 0) {
            /* the house's way in: no tables, no night, no tint */
            owf.night = 0;
            owf.tint_level = 0;
            owf.tint_map = doomo_tintmap[TINT_NONE];
            owf.luts = 0;
        } else {
            owf.night = rnd(2) ? 0 : (int)rnd(17);
            owf.tint_level = rnd(2) ? 0 : (int)rnd(16);
            owf.tint_map = doomo_tintmap[owf.tint_level ? (int)rnd(TINT_COUNT) : TINT_NONE];
            ow_frame_luts(luts);
            /* a cache that already holds other colormaps */
            for (k = (int)rnd(4); k > 0; k--) {
                const uint8_t *lc[4];
                owf_sprite_luts(doomo_colormap[rnd(16)], lc);
            }
        }
        fill_back(seed | 1u);
        ref_sprite(&img, x0, y0, s, flags, cm);
        ha = hash_back();
        fill_back(seed | 1u);
        ow_sprite(&img, x0, y0, s, flags, cm);
        hb = hash_back();
        if (ha != hb) {
            if (!bad) first_bad = i;
            bad++;
            if (bad <= 8)
                gba_logf("ow sprite test: case %u differs: %ux%u s%d flags %d at %d,%d view %d,%d %dx%d "
                         "night %d tint %d lvl %d", (unsigned)i, (unsigned)w, (unsigned)h, s, flags, x0, y0,
                         owf.ox, owf.oy, owf.ow, owf.oh, owf.night, owf.tint_level, lvl);
        }
    }
    owf = saved;
    owf.luts = 0;
    ow_invalidate();
    (void)first_bad;
    gba_logf("ow sprite test: set %u, %u cases, %u mismatches", (unsigned)set, (unsigned)n, (unsigned)bad);
    return bad;
}
