/* video.c - Mode 4 display and the 2D layer of platform.h (owner: gba).
 *
 * Mode 4: 240x160, 8bpp, two pages (0x06000000 / 0x0600A000) flipped in
 * vblank. VRAM ignores byte writes (a byte store writes the byte to both
 * halves of the halfword), so every primitive here writes halfwords and
 * does a read-modify-write for odd pixels. All primitives clip against
 * the screen and do nothing during skipped catch-up ticks (gba_nodraw).
 */
#include "gba_plat.h"
#include "doomo/palette.h"
#include "doomo/font.h"

uint16_t *gba_back = (uint16_t *)VRAM_PAGE1;

void video_load_palette(void)
{
    dma3_copy32((void *)BG_PALETTE, doomo_palette, 128);
    dma3_copy32((void *)OBJ_PALETTE, doomo_palette, 128);
}

void video_init(void)
{
    static const volatile uint32_t zero = 0;
    REG_DISPCNT = DCNT_MODE4 | DCNT_BG2;     /* showing page 0 */
    video_load_palette();
    dma3_fill32((void *)VRAM_PAGE0, &zero, 0x14000 / 4);
    gba_back = (uint16_t *)VRAM_PAGE1;
    /* identity affine for BG2 (Mode 4 is an affine bitmap BG) */
    REG_BG2PA = 0x100; REG_BG2PB = 0; REG_BG2PC = 0; REG_BG2PD = 0x100;
    REG_BG2X = 0; REG_BG2Y = 0;
}

int plat_screen_w(void) { return SCREEN_W; }
int plat_screen_h(void) { return SCREEN_H; }

/* ---------------------------------------------------------------------- */
static inline void put_px(int x, int y, uint8_t c)
{
    uint16_t *p = gba_back + ((y * SCREEN_W + x) >> 1);
    if (x & 1) *p = (uint16_t)((*p & 0x00FF) | (c << 8));
    else       *p = (uint16_t)((*p & 0xFF00) | c);
}

void plat_clear(uint8_t color)
{
    static volatile uint32_t fill;
    if (gba_nodraw) return;
    fill = color * 0x01010101u;
    dma3_fill32(gba_back, &fill, SCREEN_W * SCREEN_H / 4);
}

/* Fill one row segment [x0, x1) with c. */
static void hspan(int y, int x0, int x1, uint8_t c)
{
    uint16_t *row = gba_back + y * (SCREEN_W / 2);
    uint16_t c2 = (uint16_t)(c | (c << 8));
    if (x0 >= x1) return;
    if (x0 & 1) {
        uint16_t *p = row + (x0 >> 1);
        *p = (uint16_t)((*p & 0x00FF) | (c << 8));
        x0++;
    }
    if (x1 & 1) {
        uint16_t *p = row + (x1 >> 1);
        if (x1 - 1 >= x0) *p = (uint16_t)((*p & 0xFF00) | c);
        x1--;
    }
    {
        uint16_t *p = row + (x0 >> 1), *e = row + (x1 >> 1);
        if (p < e && ((uintptr_t)p & 2)) *p++ = c2;
        {
            uint32_t c4 = c2 | ((uint32_t)c2 << 16);
            uint32_t *q = (uint32_t *)p;
            uint32_t *qe = (uint32_t *)((uintptr_t)e & ~3u);
            while (q + 4 <= qe) { q[0] = c4; q[1] = c4; q[2] = c4; q[3] = c4; q += 4; }
            while (q < qe) *q++ = c4;
            p = (uint16_t *)q;
        }
        while (p < e) *p++ = c2;
    }
}

void plat_rect(int x, int y, int w, int h, uint8_t color)
{
    int x1 = x + w, y1 = y + h;
    if (gba_nodraw) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x >= x1 || y >= y1) return;
    if (x == 0 && x1 == SCREEN_W) {
        static volatile uint32_t fill;
        fill = color * 0x01010101u;
        dma3_fill32(gba_back + y * (SCREEN_W / 2), &fill,
                    (uint32_t)(y1 - y) * SCREEN_W / 4);
        return;
    }
    for (; y < y1; y++) hspan(y, x, x1, color);
}

void plat_shade_rect(int x, int y, int w, int h, int level)
{
    int x1 = x + w, y1 = y + h;
    const uint8_t *cm;
    if (gba_nodraw) return;
    if (level < 0) level = 0;
    if (level > 15) level = 15;
    if (level == 15) return;
    cm = doomo_colormap[level];
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x >= x1 || y >= y1) return;
    for (; y < y1; y++) {
        uint16_t *row = gba_back + y * (SCREEN_W / 2);
        int a = x, b = x1;
        if (a & 1) {
            uint16_t v = row[a >> 1];
            row[a >> 1] = (uint16_t)((v & 0xFF) | (cm[v >> 8] << 8));
            a++;
        }
        if (b & 1) {
            if (b - 1 >= a) {
                uint16_t v = row[b >> 1];
                row[b >> 1] = (uint16_t)((v & 0xFF00) | cm[v & 0xFF]);
            }
            b--;
        }
        {
            uint16_t *p = row + (a >> 1), *e = row + (b >> 1);
            while (p < e) {
                uint16_t v = *p;
                *p++ = (uint16_t)(cm[v & 0xFF] | (cm[v >> 8] << 8));
            }
        }
    }
}

void plat_line(int x0, int y0, int x1, int y1, uint8_t color)
{
    int dx, dy, sx, sy, err, n;
    if (gba_nodraw) return;
    dx = x1 > x0 ? x1 - x0 : x0 - x1;
    dy = y1 > y0 ? y0 - y1 : y1 - y0;     /* negative */
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;
    n = 4096;                              /* hard cap */
    if (y0 == y1) {
        if (y0 >= 0 && y0 < SCREEN_H) {
            int a = x0 < x1 ? x0 : x1, b = x0 < x1 ? x1 : x0;
            if (a < 0) a = 0;
            if (b > SCREEN_W - 1) b = SCREEN_W - 1;
            hspan(y0, a, b + 1, color);
        }
        return;
    }
    while (n-- > 0) {
        int e2;
        if ((unsigned)x0 < SCREEN_W && (unsigned)y0 < SCREEN_H)
            put_px(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ---------------------------------------------------------------------- */
/* text: 5x8 column-major glyphs (bit 0 = top) in a 6x8 cell */
void plat_text(int x, int y, const char *s, uint8_t color)
{
    int cx = x, cy = y;
    if (gba_nodraw || !s) return;
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        if (ch == '\n') { cx = x; cy += FONT_LINE; continue; }
        if (ch >= FONT_FIRST && ch <= FONT_LAST && ch != ' ' &&
            cx > -FONT_ADVANCE && cx < SCREEN_W && cy > -8 && cy < SCREEN_H) {
            const uint8_t *g = doomo_font[ch - FONT_FIRST];
            int col;
            for (col = 0; col < 5; col++) {
                unsigned bits = g[col];
                int px = cx + col, row;
                if (!bits || (unsigned)px >= SCREEN_W) continue;
                for (row = 0; bits; row++, bits >>= 1) {
                    int py = cy + row;
                    if ((bits & 1) && (unsigned)py < SCREEN_H) put_px(px, py, color);
                }
            }
        }
        cx += FONT_ADVANCE;
    }
}

/* ---------------------------------------------------------------------- */
/* images: 8bpp, index 0 transparent, integer scale 1..4 */
static void blit(int x, int y, const DoomoImage *img, int scale, int flip,
                 int sil, uint8_t color)
{
    int w = img->w * scale, h = img->h * scale;
    int dx0 = 0, dy0 = 0, dx1 = w, dy1 = h, dy;
    if (x < 0) dx0 = -x;
    if (y < 0) dy0 = -y;
    if (x + w > SCREEN_W) dx1 = SCREEN_W - x;
    if (y + h > SCREEN_H) dy1 = SCREEN_H - y;
    if (dx0 >= dx1 || dy0 >= dy1) return;
    for (dy = dy0; dy < dy1; dy++) {
        const uint8_t *src = img->pix + (dy / scale) * img->w;
        uint16_t *row = gba_back + (y + dy) * (SCREEN_W / 2);
        int dx = dx0;
        int sx_of[2];
        /* walk destination pixels in halfword pairs */
        while (dx < dx1) {
            int px = x + dx;
            uint16_t *p = row + (px >> 1);
            int i, n = (px & 1) ? 1 : 2;
            uint8_t c[2];
            int opaque = 0;
            if (dx + n > dx1) n = dx1 - dx;
            for (i = 0; i < n; i++) {
                int sx = (dx + i) / scale;
                uint8_t v;
                if (flip) sx = img->w - 1 - sx;
                sx_of[i] = sx;
                v = src[sx];
                c[i] = sil ? color : v;
                if (v) opaque |= 1 << i;
            }
            if (opaque) {
                uint16_t old = *p;
                if (px & 1) {
                    *p = (uint16_t)((old & 0xFF) | (c[0] << 8));
                } else if (n == 2 && opaque == 3) {
                    *p = (uint16_t)(c[0] | (c[1] << 8));
                } else {
                    uint16_t v = old;
                    if (opaque & 1) v = (uint16_t)((v & 0xFF00) | c[0]);
                    if (opaque & 2) v = (uint16_t)((v & 0x00FF) | (c[1] << 8));
                    *p = v;
                }
            }
            dx += n;
        }
        (void)sx_of;
    }
}

void plat_image(int x, int y, const DoomoImage *img, int scale, int flags,
                uint8_t color)
{
    if (gba_nodraw || !img || !img->pix || !img->w || !img->h) return;
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    if (flags & IMG_SHADOW)
        blit(x + 1, y + 1, img, scale, flags & IMG_FLIPX, 1, PAL_BLACK);
    blit(x, y, img, scale, flags & IMG_FLIPX, flags & IMG_SILHOUETTE, color);
}
