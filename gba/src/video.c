/* video.c - Mode 4 display and the 2D layer of platform.h (owner: gba).
 *
 * Mode 4: 240x160, 8bpp, two pages (0x06000000 / 0x0600A000) flipped in
 * vblank. VRAM ignores byte writes (a byte store writes the byte to both
 * halves of the halfword), so every primitive writes halfwords and does a
 * read-modify-write for odd pixels - through volatile pointers, or GCC
 * would turn the read-modify-write back into a byte store. The pixel
 * loops are ARM code in IWRAM (video.iwram.c). All primitives clip
 * against the screen and do nothing during skipped catch-up ticks
 * (gba_nodraw).
 */
#include "gba_plat.h"
#include "video_int.h"
#include "hcmd.h"
#include "postfx.h"
#include "doomo/palette.h"
#include "doomo/font.h"

uint16_t *gba_back = (uint16_t *)VRAM_PAGE1;
/* the font, transposed: rows[ch][r] bit c = pixel (c, r) */
EWRAM_BSS uint8_t gba_font_rows[FONT_LAST - FONT_FIRST + 1][8];

void video_load_palette(void)
{
    dma3_copy32((void *)BG_PALETTE, doomo_palette, 128);
    dma3_copy32((void *)OBJ_PALETTE, doomo_palette, 128);
}

static void build_font_rows(void)
{
    int ch, col, r;
    for (ch = 0; ch <= FONT_LAST - FONT_FIRST; ch++)
        for (r = 0; r < 8; r++) {
            uint8_t m = 0;
            for (col = 0; col < 5; col++)
                if (doomo_font[ch][col] & (1 << r)) m |= (uint8_t)(1 << col);
            gba_font_rows[ch][r] = m;
        }
}

/* The outline of ui_text_outline for one glyph: the glyph at its eight
 * 1-pixel neighbours and at (+1, +2), as 11 rows of a 7-wide box whose
 * origin is (-1, -1) from the glyph's (bit c = column c). Computed per
 * glyph drawn (a few dozen ops) rather than kept as a 2 KB EWRAM table. */
static void glyph_dilate(const uint8_t *g, uint16_t out[11])
{
    int r;
    for (r = 0; r < 11; r++) {
        uint32_t m = 0;
        int gr;
        for (gr = r - 2; gr <= r; gr++) {               /* glyph rows r-1-1 .. r-1+1 */
            if (gr < 0 || gr >= 8) continue;
            m |= g[gr] | ((uint32_t)g[gr] << 1) | ((uint32_t)g[gr] << 2);  /* cols -1, 0, +1 */
        }
        gr = r - 3;                                       /* (+1, +2) */
        if (gr >= 0 && gr < 8) m |= (uint32_t)g[gr] << 2;
        out[r] = (uint16_t)m;
    }
}

void video_init(void)
{
    static const volatile uint32_t zero = 0;
    obj_init();                               /* S1: all OBJs hidden first */
    REG_DISPCNT = DCNT_MODE4 | DCNT_BG2 | DCNT_OBJ | DCNT_OBJ_1D;  /* showing page 0 */
    video_load_palette();
    dma3_fill32((void *)VRAM_PAGE0, &zero, 0x14000 / 4);
    gba_back = (uint16_t *)VRAM_PAGE1;
    /* identity affine for BG2 (Mode 4 is an affine bitmap BG) */
    REG_BG2PA = 0x100; REG_BG2PB = 0; REG_BG2PC = 0; REG_BG2PD = 0x100;
    REG_BG2X = 0; REG_BG2Y = 0;
    build_font_rows();
}

int plat_screen_w(void) { return SCREEN_W; }
int plat_screen_h(void) { return SCREEN_H; }

/* ---------------------------------------------------------------------- */
/* The DMA fills' source word lives in IWRAM: DMA re-reads a fixed source
 * for every word it writes, and a 32-bit EWRAM read costs 6 cycles against
 * 1 (a 38 KB clear from an EWRAM word took 2.5 ms more). */
IWRAM_BSS static volatile uint32_t gba_fill;

void plat_clear(uint8_t color)
{
    if (gba_nodraw) return;
    if (!color) color = PAL_BLACK;            /* index 0 is "transparent" */
    gba_fill = color * 0x01010101u;
    dma3_fill32(gba_back, &gba_fill, SCREEN_W * SCREEN_H / 4);
}

/* plat_rect's DMA fill for full-width rects (video_loops.s tail-calls it;
 * a few per frame, so it needs no IWRAM) */
void vid_rect_full(int x0, int y0, int x1, int y1, uint32_t color)
{
    (void)x0; (void)x1;
    gba_fill = (color & 0xFF) * 0x01010101u;
    dma3_fill32(gba_back + y0 * (SCREEN_W / 2), &gba_fill, (uint32_t)(y1 - y0) * SCREEN_W / 4);
}

/* A dilated glyph (7 columns x 11 rows, glyph_dilate) on screen at the box
 * origin (x, y): each row lands in at most three aligned words, written
 * with word read-modify-writes (VRAM takes 32-bit accesses). ROM: the
 * outline pass is a few strings a frame, and IWRAM is full. */
static void glyph_dil(const uint16_t *g, int x, int y, uint32_t c4)
{
    typedef volatile uint32_t vu32_t;
    vu32_t *w = (vu32_t *)((uint8_t *)gba_back + y * SCREEN_W + (x & ~3));
    const int sh = x & 3;
    int r;
    for (r = 0; r < 11; r++, w += SCREEN_W / 4) {
        uint32_t m = (uint32_t)g[r] << sh, k;
        if (!m) continue;
        k = gba_nibmask[m & 15];
        if (k == 0xFFFFFFFFu) w[0] = c4;
        else if (k) w[0] = (w[0] & ~k) | (c4 & k);
        k = gba_nibmask[(m >> 4) & 15];
        if (k == 0xFFFFFFFFu) w[1] = c4;
        else if (k) w[1] = (w[1] & ~k) | (c4 & k);
        k = gba_nibmask[m >> 8];
        if (k) w[2] = (w[2] & ~k) | (c4 & k);
    }
}

void vid_text_dil(int x, int y, const char *s, uint32_t color)
{
    const uint32_t c4 = color * 0x01010101u;
    int cx = x, cy = y;
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        if (ch == '\n') { cx = x; cy += FONT_LINE; continue; }
        if (ch >= FONT_FIRST && ch <= FONT_LAST && ch != ' ' &&
            cx > -FONT_ADVANCE - 1 && cx < SCREEN_W + 1 && cy > -10 && cy < SCREEN_H + 1) {
            uint16_t g[11];
            int bx = cx - 1, by = cy - 1, r;
            glyph_dilate(gba_font_rows[ch - FONT_FIRST], g);
            if (bx >= 0 && bx + 7 <= SCREEN_W && by >= 0 && by + 11 <= SCREEN_H) {
                glyph_dil(g, bx, by, c4);
            } else {
                for (r = 0; r < 11; r++) {
                    int py = by + r, c;
                    if ((unsigned)py >= SCREEN_H || !g[r]) continue;
                    for (c = 0; c < 7; c++)
                        if ((g[r] >> c) & 1 && (unsigned)(bx + c) < SCREEN_W)
                            vid_pixel(bx + c, py, color);
                }
            }
        }
        cx += FONT_ADVANCE;
    }
}

/* plat_rect and plat_line are ARM code in IWRAM (video.iwram.c): the
 * core's UI calls them thousands of times a frame for small spans. The
 * thin wrappers below clip and call the IWRAM loops. */
void plat_shade_rect(int x, int y, int w, int h, int level)
{
    int x1 = x + w, y1 = y + h;
    if (gba_nodraw) return;
    if (level < 0) level = 0;
    if (level > 15) level = 15;
    if (level == 15) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x >= x1 || y >= y1) return;
    vid_shade(x, y, x1, y1, doomo_colormap[level]);
}

void plat_text(int x, int y, const char *s, uint8_t color)
{
    if (gba_text_armed && s) gba_text_probe(s);   /* harness expect-text */
    if (gba_nodraw || !s) return;
    if (!color) color = PAL_BLACK;
    vid_text(x, y, s, color);
}

/* platform.h: the outline in one dilated pass, then the text (was ten
 * plat_text passes: 8% of a title frame) */
void plat_text_outline(int x, int y, const char *s, uint8_t color, uint8_t outline)
{
    if (gba_text_armed && s) gba_text_probe(s);   /* harness expect-text */
    if (gba_nodraw || !s) return;
    if (!color) color = PAL_BLACK;
    if (!outline) outline = PAL_BLACK;
    vid_text_dil(x, y, s, outline);
    vid_text(x, y, s, color);
}

/* 8bpp image, index 0 transparent, integer scale 1..4 */
void plat_image(int x, int y, const DoomoImage *img, int scale, int flags,
                uint8_t color)
{
    if (gba_nodraw || !img || !img->pix || !img->w || !img->h) return;
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    if (!color) color = PAL_BLACK;
    if (flags & IMG_SHADOW)
        vid_blit(x + 1, y + 1, img, scale, (flags & IMG_FLIPX) | IMG_SILHOUETTE, PAL_BLACK);
    vid_blit(x, y, img, scale, flags & (IMG_FLIPX | IMG_SILHOUETTE), color);
}
