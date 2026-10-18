/* video.iwram.c - the 2D layer's pixel loops, ARM in IWRAM (owner: gba).
 * Mode 4 VRAM takes halfword writes only: single pixels are read-modify-
 * written through volatile pointers (a plain C read-modify-write may be
 * compiled into a byte store, which VRAM mirrors into both bytes). */
#include "video_int.h"
#include "doomo/platform.h"
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

void vid_pixel(int x, int y, uint32_t color)
{
    uint16_t *p = gba_back + y * LINE + (x >> 1);
    if (x & 1) put_hi(p, color);
    else put_lo(p, color);
}

/* vid_rect: video_loops.s */

void vid_shade(int x0, int y0, int x1, int y1, const uint8_t *cm)
{
    uint32_t lut32[64];                 /* the colormap row, copied to IWRAM */
    const uint8_t *lut = cm;
    int y;
    if ((x1 - x0) * (y1 - y0) >= 256) {
        /* the colormap lives in EWRAM (3 cycles a byte): big areas read
         * a copy on the IWRAM stack instead */
        int i;
        if (((uintptr_t)cm & 3) == 0) {
            const uint32_t *src = (const uint32_t *)cm;
            for (i = 0; i < 64; i++) lut32[i] = src[i];
        } else {
            for (i = 0; i < 256; i++) ((uint8_t *)lut32)[i] = cm[i];
        }
        lut = (const uint8_t *)lut32;
    }
    for (y = y0; y < y1; y++) {
        uint16_t *row = gba_back + y * LINE;
        int a = x0, b = x1;
        if (a & 1) {
            vu16_t *p = (vu16_t *)(row + (a >> 1));
            uint32_t v = *p;
            *p = (uint16_t)((v & 0xFF) | (lut[v >> 8] << 8));
            a++;
        }
        if (b & 1) {
            if (b - 1 >= a) {
                vu16_t *p = (vu16_t *)(row + (b >> 1));
                uint32_t v = *p;
                *p = (uint16_t)((v & 0xFF00) | lut[v & 0xFF]);
            }
            b--;
        }
        {
            vu16_t *p = (vu16_t *)(row + (a >> 1)), *e = (vu16_t *)(row + (b >> 1));
            if (p < e && ((uintptr_t)p & 2)) {     /* to a word boundary */
                uint32_t v = *p;
                *p++ = (uint16_t)(lut[v & 0xFF] | (lut[v >> 8] << 8));
            }
            {
                uintptr_t wa = (uintptr_t)p, we = (uintptr_t)e & ~(uintptr_t)3;
                if (we >= wa + 16) {                 /* 4+ words: the asm loop */
                    vid_shade_words((uint32_t *)wa, (int)((we - wa) >> 2), lut);
                    p = (vu16_t *)we;
                }
            }
            while (p + 1 < e) {                       /* short spans: halfwords */
                uint32_t v = p[0], v2 = p[1];
                p[0] = (uint16_t)(lut[v & 0xFF] | (lut[v >> 8] << 8));
                p[1] = (uint16_t)(lut[v2 & 0xFF] | (lut[v2 >> 8] << 8));
                p += 2;
            }
            if (p < e) {
                uint32_t v = *p;
                *p = (uint16_t)(lut[v & 0xFF] | (lut[v >> 8] << 8));
            }
        }
    }
}

/* nibble -> byte mask: bit i set = byte i of a word (also video.c) */
const uint32_t gba_nibmask[16] = {
    0x00000000, 0x000000FF, 0x0000FF00, 0x0000FFFF,
    0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00FFFFFF,
    0xFF000000, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF,
    0xFFFF0000, 0xFFFF00FF, 0xFFFFFF00, 0xFFFFFFFF,
};

/* a whole glyph on screen at (x, y): each row's 5 pixels land in at most
 * two aligned words, written with a word read-modify-write (VRAM takes
 * 32-bit accesses) */
static inline void glyph(const uint8_t *g, int x, int y, uint32_t c4)
{
    typedef volatile uint32_t vu32_t;
    vu32_t *w = (vu32_t *)((uint8_t *)gba_back + y * SCREEN_W + (x & ~3));
    const int sh = x & 3;
    int r;
    for (r = 0; r < 8; r++, w += SCREEN_W / 4) {
        uint32_t m = (uint32_t)g[r] << sh, k;
        if (!m) continue;
        k = gba_nibmask[m & 15];
        if (k == 0xFFFFFFFFu) w[0] = c4;
        else if (k) w[0] = (w[0] & ~k) | (c4 & k);
        k = gba_nibmask[m >> 4];
        if (k) w[1] = (w[1] & ~k) | (c4 & k);
    }
}

void vid_text(int x, int y, const char *s, uint32_t color)
{
    const uint32_t c4 = color * 0x01010101u;
    int cx = x, cy = y;
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        if (ch == '\n') { cx = x; cy += FONT_LINE; continue; }
        if (ch >= FONT_FIRST && ch <= FONT_LAST && ch != ' ' &&
            cx > -FONT_ADVANCE && cx < SCREEN_W && cy > -8 && cy < SCREEN_H) {
            const uint8_t *g = gba_font_rows[ch - FONT_FIRST];
            int r;
            if (cx >= 0 && cx + 5 <= SCREEN_W && cy >= 0 && cy + 8 <= SCREEN_H) {
                glyph(g, cx, cy, c4);
            } else {
                for (r = 0; r < 8; r++) {
                    int py = cy + r, c;
                    if ((unsigned)py >= SCREEN_H || !g[r]) continue;
                    for (c = 0; c < 5; c++)
                        if ((g[r] >> c) & 1 && (unsigned)(cx + c) < SCREEN_W)
                            vid_pixel(cx + c, py, color);
                }
            }
        }
        cx += FONT_ADVANCE;
    }
}

/* images: 8bpp, index 0 transparent, integer scale 1..4, walked in VRAM
 * halfword pairs (opaque pairs are written without reading) */
__attribute__((optimize("Os")))
void vid_blit(int x, int y, const DoomoImage *img, int scale, int flags, uint32_t color)
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

/* ---- platform.h entry points (clip, then the loops above) ------------- */

/* 50% checkerboard: pixel (x+i, y+j) when i + j is even. Each row's
 * pixels all sit in the same byte of consecutive halfwords. */
void plat_checker(int x, int y, int w, int h, uint8_t color)
{
    int x1 = x + w, y1 = y + h, yy, par;
    if (gba_nodraw || w <= 0 || h <= 0) return;
    if (!color) color = PAL_BLACK;
    par = (x + y) & 1;                         /* drawn: ((px + py) & 1) == par */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x >= x1 || y >= y1) return;
    for (yy = y; yy < y1; yy++) {
        int px = x + (((x + yy) & 1) != par), n;
        vu16_t *p;
        if (px >= x1) continue;
        n = (x1 - px + 1) >> 1;
        p = (vu16_t *)(gba_back + yy * LINE + (px >> 1));
        if (px & 1) {
            const uint32_t c = (uint32_t)color << 8;
            do { *p = (uint16_t)((*p & 0x00FF) | c); p++; } while (--n);
        } else {
            do { *p = (uint16_t)((*p & 0xFF00) | color); p++; } while (--n);
        }
    }
}


void plat_line(int x0, int y0, int x1, int y1, uint8_t color)
{
    int dx, dy, sx, sy, err, n;
    if (gba_nodraw) return;
    if (!color) color = PAL_BLACK;
    if (y0 == y1) {
        if (y0 >= 0 && y0 < SCREEN_H) {
            int a = x0 < x1 ? x0 : x1, b = x0 < x1 ? x1 : x0;
            if (a < 0) a = 0;
            if (b > SCREEN_W - 1) b = SCREEN_W - 1;
            if (a <= b) vid_rect(a, y0, b + 1, y0 + 1, color);
        }
        return;
    }
    if (x0 == x1) {
        if (x0 >= 0 && x0 < SCREEN_W) {
            int a = y0 < y1 ? y0 : y1, b = y0 < y1 ? y1 : y0;
            if (a < 0) a = 0;
            if (b > SCREEN_H - 1) b = SCREEN_H - 1;
            if (a <= b) vid_rect(x0, a, x0 + 1, b + 1, color);
        }
        return;
    }
    dx = x1 > x0 ? x1 - x0 : x0 - x1;
    dy = y1 > y0 ? y0 - y1 : y1 - y0;     /* negative */
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;
    n = 4096;                              /* hard cap */
    while (n-- > 0) {
        int e2;
        if ((unsigned)x0 < SCREEN_W && (unsigned)y0 < SCREEN_H)
            vid_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}


