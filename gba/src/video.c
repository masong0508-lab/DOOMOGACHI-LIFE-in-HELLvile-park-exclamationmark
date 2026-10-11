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
#include "doomo/palette.h"
#include "doomo/font.h"

uint16_t *gba_back = (uint16_t *)VRAM_PAGE1;

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
    build_font_rows();
}

int plat_screen_w(void) { return SCREEN_W; }
int plat_screen_h(void) { return SCREEN_H; }

/* ---------------------------------------------------------------------- */
void plat_clear(uint8_t color)
{
    static volatile uint32_t fill;
    if (gba_nodraw) return;
    if (!color) color = PAL_BLACK;            /* index 0 is "transparent" */
    fill = color * 0x01010101u;
    dma3_fill32(gba_back, &fill, SCREEN_W * SCREEN_H / 4);
}

/* plat_rect, plat_shade_rect, plat_line and plat_text are ARM code in
 * IWRAM (video.iwram.c): the core's UI calls them thousands of times a
 * frame for small spans */

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
