/* et_blittest.c - checks the IWRAM vid_blit (src/video_loops.s: scale 1 without flip in ARM, the rest in
 * video.iwram.c vid_blit_c) against a verbatim copy of the old all-C vid_blit, at engine-test boot (owner: gba).
 * Random images (odd and even widths and source alignments), positions that clip on every side, scales 1..3,
 * flip, silhouette, over a random background so the edge pairs' read-modify-write is checked too; the two
 * results (VRAM pages 0 and 1) must be equal after every call. Logs "ET blittest ok (N cases)" or
 * "ET blittest FAIL ...". */
#include "video_int.h"
#include "doomo/platform.h"
#include "enginetest.h"
#include "iwram_ovl.h"
#include "doomo/plat_ext.h"

void plat_checker_m4(int x, int y, int w, int h, uint8_t color) __attribute__((long_call));

#define LINE 120
typedef volatile uint16_t vu16_t;
#define PAGE_A ((uint16_t *)0x06000000)
#define PAGE_B ((uint16_t *)0x0600A000)

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

/* ---- the old vid_blit, verbatim (only the name differs) ---- */
static void ref_blit(int x, int y, const DoomoImage *img, int scale, int flags, uint32_t color)
{
    const int w = img->w * scale, h = img->h * scale;
    const int flip = flags & IMG_FLIPX, sil = flags & IMG_SILHOUETTE;
    const uint32_t inv = 65536u / (uint32_t)scale + 1u;    /* v / scale */
    int x0 = x, x1 = x + w, y0 = y, y1 = y + h, yy, xa, xb;
    uint16_t lut[SCREEN_W + 2];
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x0 >= x1 || y0 >= y1) return;
    xa = x0 & ~1;
    xb = (x1 + 1) & ~1;
    if (scale == 1 && !flip) {
        /* the common case (cached chibis, building sprites, shape images):
         * source pairs straight from the row, no column table; only the
         * first / last VRAM pair may reach one pixel outside the image */
        int n = (xb - xa) >> 1, lead = xa < x0, trail = xb > x1;
        for (yy = y0; yy < y1; yy++) {
            const uint8_t *src = img->pix + (yy - y) * img->w + (xa - x);
            uint16_t *p = gba_back + yy * LINE + (xa >> 1);
            int k = 0;
            if (lead) {                         /* only its right pixel is in */
                uint32_t b = src[1];
                if (b) put_hi(p, sil ? color : b);
                p++; src += 2; k = 1;
            }
            if (!sil && !((uintptr_t)src & 1)) {
                /* aligned source: one halfword read per pair (the chibi
                 * cache is in EWRAM, a 16-bit bus) */
                const uint16_t *s2 = (const uint16_t *)src;
                for (; k < n - trail; k++, p++, s2++) {
                    uint32_t v = *s2;
                    if (!v) continue;
                    if ((v & 0xFF) && (v & 0xFF00)) *p = (uint16_t)v;
                    else if (v & 0xFF) put_lo(p, v);
                    else put_hi(p, v >> 8);
                }
                src = (const uint8_t *)s2;
            } else if (!sil) {
                /* odd source: aligned halfwords, each pair is the high
                 * byte of one and the low byte of the next (the bytes
                 * read beside the row are never used) */
                const uint16_t *s2 = (const uint16_t *)(src - 1);
                uint32_t prev = *s2++;
                for (; k < n - trail; k++, p++) {
                    uint32_t next = *s2++;
                    uint32_t v = (prev >> 8) | ((next & 0xFF) << 8);
                    prev = next;
                    if (!v) continue;
                    if ((v & 0xFF) && (v & 0xFF00)) *p = (uint16_t)v;
                    else if (v & 0xFF) put_lo(p, v);
                    else put_hi(p, v >> 8);
                }
                src = (const uint8_t *)s2 - 1;
            }
            for (; k < n - trail; k++, p++, src += 2) {
                uint32_t a = src[0], b = src[1];
                if (!(a | b)) continue;
                if (sil) { if (a) a = color; if (b) b = color; }
                if (a && b) *p = (uint16_t)(a | (b << 8));
                else if (a) put_lo(p, a);
                else put_hi(p, b);
            }
            if (trail && k < n) {               /* only its left pixel is in */
                uint32_t a = src[0];
                if (a) put_lo(p, sil ? color : a);
            }
        }
        return;
    }
    {
        int px;
        for (px = xa; px < xb; px++) {
            int tx;
            if (px < x0 || px >= x1) { lut[px - xa] = 0xFFFF; continue; }
            tx = (int)(((uint32_t)(px - x) * inv) >> 16);
            if (tx >= img->w) tx = img->w - 1;
            if (flip) tx = img->w - 1 - tx;
            lut[px - xa] = (uint16_t)tx;
        }
    }
    for (yy = y0; yy < y1; yy++) {
        const uint8_t *src = img->pix + (((uint32_t)(yy - y) * inv) >> 16) * img->w;
        uint16_t *p = gba_back + yy * LINE + (xa >> 1);
        const uint16_t *l = lut;
        int k, n = (xb - xa) >> 1;
        for (k = 0; k < n; k++, l += 2, p++) {
            uint32_t a = l[0] != 0xFFFF ? src[l[0]] : 0;
            uint32_t b = l[1] != 0xFFFF ? src[l[1]] : 0;
            if (!(a | b)) continue;
            if (sil) { if (a) a = color; if (b) b = color; }
            if (a && b) *p = (uint16_t)(a | (b << 8));
            else if (a) put_lo(p, a);
            else put_hi(p, b);
        }
    }
}

static uint32_t rng = 0x2468ACE1u;
static uint32_t next(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}

static uint8_t srcbuf[4 + 64 * 20 + 4];

void et_blit_test(void)
{
    uint16_t *keep = gba_back;
    int i, n = 0, bad = 0;
    uint32_t k;
    for (k = 0; k < (uint32_t)(LINE * 160); k++) {
        uint16_t v = (uint16_t)next();
        PAGE_A[k] = v; PAGE_B[k] = v;
    }
    for (i = 0; i < 6000 && !bad; i++) {
        DoomoImage im;
        int w = 1 + (int)(next() % (i & 1 ? 40 : 17)), h = 1 + (int)(next() % 14), off = (int)(next() & 3);
        int dens = (int)(next() % 4), scale = (next() % 5) ? 1 : 1 + (int)(next() % 3);
        int flags = (next() % 3) ? 0 : (int)(next() % 8) & ~2;
        int x = (int)(next() % 300) - 40, y = (int)(next() % 200) - 25;
        uint32_t color = 1 + next() % 255;
        int j, y0, y1, r;
        if (i % 7 == 0) { x = (int)(next() % 8) - 4; } else if (i % 7 == 1) { x = 240 - (int)(next() % 12); }
        for (j = 0; j < w * h; j++) {
            uint32_t v = next();
            srcbuf[4 + off + j] = (uint8_t)((v >> 8) % 4 < (uint32_t)dens ? 0 : 1 + (v >> 16) % 255);
        }
        im.w = (uint16_t)w; im.h = (uint16_t)h; im.pix = srcbuf + 4 + off; im.uid = 1;
        gba_back = PAGE_A; vid_blit(x, y, &im, scale, flags, color);
        gba_back = PAGE_B; ref_blit(x, y, &im, scale, flags, color);
        y0 = y < 0 ? 0 : y; y1 = y + h * scale > 160 ? 160 : y + h * scale;
        for (r = y0; r < y1 && !bad; r++)
            for (j = 0; j < LINE; j++)
                if (PAGE_A[r * LINE + j] != PAGE_B[r * LINE + j]) {
                    gba_logf("ET blittest FAIL case %d x %d y %d w %d h %d sc %d fl %d row %d hw %d: %x vs %x",
                             i, x, y, w, h, scale, flags, r, j, PAGE_A[r * LINE + j], PAGE_B[r * LINE + j]);
                    bad = 1;
                    break;
                }
        n++;
    }
    for (k = 0; k < (uint32_t)(LINE * 160) && !bad; k++)
        if (PAGE_A[k] != PAGE_B[k]) {
            gba_logf("ET blittest FAIL page differs at %u", (unsigned)k);
            bad = 1;
        }
    gba_back = keep;
    if (!bad) gba_logf("ET blittest ok (%d cases)", n);
}

/* (SP6) vid_blit2 (src/blit2.c, the IWRAM UI group's scale-2 loop that plat_image takes while the group is in)
 * against the same verbatim reference at scale 2 without a flip: odd and even x (the odd case merges two source
 * pixels per halfword), clipping on all four sides, silhouette on and off, over a random background; then the
 * group's word-at-a-time checker (vid_checker_ui) against plat_checker_m4, and its whole-beam loop (plat_beam)
 * against the concert's old per-band beam. Logs
 * "ET blit2test ok (N cases)" or "ET blit2test FAIL ...". */
void et_blit2_test(void)
{
    uint16_t *keep = gba_back;
    int i, n = 0, bad = 0;
    uint32_t k;
    iwram_ovl_use(OVL_UI);
    for (k = 0; k < (uint32_t)(LINE * 160); k++) {
        uint16_t v = (uint16_t)next();
        PAGE_A[k] = v; PAGE_B[k] = v;
    }
    for (i = 0; i < 2000 && !bad; i++) {
        DoomoImage im;
        int w = 1 + (int)(next() % (i & 1 ? 33 : 17)), h = 1 + (int)(next() % 16), off = (int)(next() & 3);
        int dens = (int)(next() % 4), sil = (next() % 3) == 0;
        int x = (int)(next() % 320) - 70, y = (int)(next() % 240) - 60;
        uint32_t color = 1 + next() % 255;
        int j, y0, y1, r;
        if (i % 7 == 0) { x = (int)(next() % 8) - 4; } else if (i % 7 == 1) { x = 240 - (int)(next() % 12); }
        else if (i % 7 == 2) { y = (int)(next() % 6) - 3; } else if (i % 7 == 3) { y = 160 - (int)(next() % 8) - 2 * h + 3; }
        for (j = 0; j < w * h; j++) {
            uint32_t v = next();
            srcbuf[4 + off + j] = (uint8_t)((v >> 8) % 4 < (uint32_t)dens ? 0 : 1 + (v >> 16) % 255);
        }
        im.w = (uint16_t)w; im.h = (uint16_t)h; im.pix = srcbuf + 4 + off; im.uid = 1;
        gba_back = PAGE_A; vid_blit2(x, y, &im, sil ? color : 0u);
        gba_back = PAGE_B; ref_blit(x, y, &im, 2, sil ? IMG_SILHOUETTE : 0, color);
        y0 = y < 0 ? 0 : y; y1 = y + h * 2 > 160 ? 160 : y + h * 2;
        for (r = y0; r < y1 && !bad; r++)
            for (j = 0; j < LINE; j++)
                if (PAGE_A[r * LINE + j] != PAGE_B[r * LINE + j]) {
                    gba_logf("ET blit2test FAIL case %d x %d y %d w %d h %d sil %d row %d hw %d: %x vs %x",
                             i, x, y, w, h, sil, r, j, PAGE_A[r * LINE + j], PAGE_B[r * LINE + j]);
                    bad = 1;
                    break;
                }
        n++;
    }
    /* the group's word-at-a-time checker against plat_checker_m4 (video.iwram.c) */
    for (i = 0; i < 3000 && !bad; i++) {
        int x = (int)(next() % 280) - 20, y = (int)(next() % 190) - 15, w = (int)(next() % 250) - 4;
        int h = (int)(next() % (i & 3 ? 4 : 40)) - 1, j, r, y0, y1;
        uint32_t color = next() % 256;               /* 0 too: both draw PAL_BLACK */
        gba_back = PAGE_A; vid_checker_ui(x, y, w, h, color);
        gba_back = PAGE_B; plat_checker_m4(x, y, w, h, (uint8_t)color);
        y0 = y < 0 ? 0 : y; y1 = y + h > 160 ? 160 : y + h;
        for (r = y0; r < y1 && !bad; r++)
            for (j = 0; j < LINE; j++)
                if (PAGE_A[r * LINE + j] != PAGE_B[r * LINE + j]) {
                    gba_logf("ET blit2test FAIL checker case %d x %d y %d w %d h %d row %d hw %d: %x vs %x",
                             i, x, y, w, h, r, j, PAGE_A[r * LINE + j], PAGE_B[r * LINE + j]);
                    bad = 1;
                    break;
                }
        n++;
    }
    /* the group's whole-beam loop (plat_beam) against the core's beam() + cspan() of 7729ada over plat_checker_m4 */
    for (i = 0; i < 600 && !bad; i++) {
        int ax = (int)(next() % 280) - 20, ay = (int)(next() % 40) - 8, tx = (int)(next() % 400) - 80;
        int ty = ay + (int)(next() % 170) - 10, hw0 = (int)(next() % 4), hw1 = hw0 + (int)(next() % 20);
        uint32_t color = next() % 256;
        int j, r, y, dy = ty - ay;
        gba_back = PAGE_A;
        if (!plat_beam(ax, ay, tx, ty, hw0, hw1, (uint8_t)color)) { gba_logf("ET blit2test FAIL beam not drawn"); bad = 1; break; }
        gba_back = PAGE_B;
        if (dy > 0)
            for (y = ay; y < ty; y += 3) {
                int f = (y - ay) * 256 / dy;
                int cx = ax + (tx - ax) * f / 256, hw = hw0 + (hw1 - hw0) * f / 256, x0 = cx - hw, x1 = cx + hw + 1;
                if ((x0 + y) & 1) x0++;
                if (x1 > x0) plat_checker_m4(x0, y, x1 - x0, 3, (uint8_t)color);
            }
        for (r = ay < 0 ? 0 : ay; r < ty + 3 && r < 160 && !bad; r++)
            for (j = 0; j < LINE; j++)
                if (PAGE_A[r * LINE + j] != PAGE_B[r * LINE + j]) {
                    gba_logf("ET blit2test FAIL beam case %d a %d,%d t %d,%d hw %d..%d row %d hw %d: %x vs %x", i, ax, ay,
                             tx, ty, hw0, hw1, r, j, PAGE_A[r * LINE + j], PAGE_B[r * LINE + j]);
                    bad = 1;
                    break;
                }
        n++;
    }
    for (k = 0; k < (uint32_t)(LINE * 160) && !bad; k++)
        if (PAGE_A[k] != PAGE_B[k]) {
            gba_logf("ET blit2test FAIL page differs at %u", (unsigned)k);
            bad = 1;
        }
    gba_back = keep;
    if (!bad) gba_logf("ET blit2test ok (%d cases)", n);
}
