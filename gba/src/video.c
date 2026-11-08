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
#include "canvas_int.h"
#include "iwram_ovl.h"

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
void vid_glyph_dilate(const uint8_t *g, uint16_t out[11])
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
    if (gba_nodraw == 1) return;
    if (gba_nodraw && vid_route(0, 0, SCREEN_W, SCREEN_H) != 1) return;   /* never the canvas: big */
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
/* MS-PF8: the music studio's curtains (core/src/studio_stage.c draw_curtains). Strips are 6 px: body, body, line, body x3,
 * alternating shade; left strips start at x = 0, right ones end at the screen edge, so each side is a 12 px period
 * (3 words) held in registers and stored down every row: no per-rect set-up, no DMA from slow EWRAM. Returns 0 (caller
 * draws rects) on anything but a plain Mode 4 frame. */
typedef volatile uint32_t cur_vu32_t;
static inline void cur_run(cur_vu32_t *w, int n, uint32_t a, uint32_t b, uint32_t c)
{
    for (; n >= 3; n -= 3, w += 3) { w[0] = a; w[1] = b; w[2] = c; }
    if (n > 0) w[0] = a;
    if (n > 1) w[1] = b;
}

int gba_curtains(int sw, int sh, int cw, const uint8_t *body, const uint8_t *line)
{
    uint8_t pl[12], pr[12];
    uint32_t tl[3], tr[3], lmask, rmask, la, lb, lc, ra, rb, rc;
    int cover, nl, rem, hw, fw, rn, ph, s, j, y;
    cur_vu32_t *row;
    if (gba_nodraw != 0 || sw != SCREEN_W || (sw % 12) || sh <= 0 || sh > SCREEN_H) return 0;
    cover = (cw + 5) / 6 * 6;
    if (cover <= 0 || cover > sw / 2) return 0;
    for (s = 0; s < 2; s++)
        for (j = 0; j < 6; j++) {
            pl[s * 6 + j] = j == 2 ? line[s] : body[s];
            pr[s * 6 + j] = j == 2 ? line[1 - s] : body[1 - s];
        }
    for (j = 0; j < 3; j++) {
        tl[j] = pl[j * 4] | (uint32_t)pl[j * 4 + 1] << 8 | (uint32_t)pl[j * 4 + 2] << 16 | (uint32_t)pl[j * 4 + 3] << 24;
        tr[j] = pr[j * 4] | (uint32_t)pr[j * 4 + 1] << 8 | (uint32_t)pr[j * 4 + 2] << 16 | (uint32_t)pr[j * 4 + 3] << 24;
    }
    nl = cover / 4;                      /* whole words on the left; rem (0 or 2) px in the word after */
    rem = cover & 3;
    lmask = rem ? (1u << (8 * rem)) - 1u : 0;
    hw = (sw - cover) / 4;               /* right side: the head word (only when rem), then whole words */
    fw = (sw - cover + 3) / 4;
    rmask = rem ? 0xFFFFFFFFu << (8 * rem) : 0;
    rn = SCREEN_W / 4 - fw;
    ph = fw % 3;                         /* the right tile rotated to start at word fw (absolute phase) */
    la = tl[0]; lb = tl[1]; lc = tl[2];
    ra = tr[ph]; rb = tr[(ph + 1) % 3]; rc = tr[(ph + 2) % 3];
    row = (cur_vu32_t *)gba_back;
    /* rows 0..8 are painted over by the valance (a full-width rect, drawn right after this): start below them */
    y = sh > 9 ? 9 : 0;
    row += y * (SCREEN_W / 4);
    if (nl == 6 && !rem) {               /* fully open (cover 24): both sides are 6 words, straight-line stores */
        cur_vu32_t *rr = row + 54;       /* fw = 54, phase 0 */
        for (; y < sh; y++, row += SCREEN_W / 4, rr += SCREEN_W / 4) {
            row[0] = la; row[1] = lb; row[2] = lc; row[3] = la; row[4] = lb; row[5] = lc;
            rr[0] = ra; rr[1] = rb; rr[2] = rc; rr[3] = ra; rr[4] = rb; rr[5] = rc;
        }
        return 1;
    }
    for (; y < sh; y++, row += SCREEN_W / 4) {
        cur_run(row, nl, la, lb, lc);
        if (lmask) row[nl] = (row[nl] & ~lmask) | (tl[nl % 3] & lmask);
        if (rmask) row[hw] = (row[hw] & ~rmask) | (tr[hw % 3] & rmask);
        cur_run(row + fw, rn, ra, rb, rc);
    }
    return 1;
}

/* MS-PF10: the stage wall's unlit LED (core/src/studio_stage.c led_dot): pixels x-2,x-1,x+1,x+2 on rows y-1 and y, and
 * x-1..x+1 on row y+1, as word read-modify-writes. 0 = not a plain bitmap frame / not word aligned: draw the rects. */
int gba_led_dot(int x, int y, uint8_t c)
{
    cur_vu32_t *w;
    uint32_t c4 = c * 0x01010101u;
    if (gba_nodraw != 0 || ((x - 2) & 3) || x < 2 || x + 2 >= SCREEN_W || y < 1 || y + 1 >= SCREEN_H) return 0;
    w = (cur_vu32_t *)((uint8_t *)gba_back + (y - 1) * SCREEN_W + (x - 2));
    w[0] = (w[0] & 0x00FF0000u) | (c4 & 0xFF00FFFFu);
    w[1] = (w[1] & 0xFFFFFF00u) | (c4 & 0x000000FFu);
    w += SCREEN_W / 4;
    w[0] = (w[0] & 0x00FF0000u) | (c4 & 0xFF00FFFFu);
    w[1] = (w[1] & 0xFFFFFF00u) | (c4 & 0x000000FFu);
    w += SCREEN_W / 4;
    w[0] = (w[0] & 0x000000FFu) | (c4 & 0xFFFFFF00u);
    return 1;
}

/* MS-PF12: a lit wall LED (core/src/studio_stage.c led_heart), the five rects (x-2,y-3,2x1) (x+1,y-3,2x1) (x-3,y-2,7x3)
 * (x-2,y+1,5x1) (x,y+2,1x1) as 14 word writes. x - 2 must be word aligned (the LED grid is); 0 = draw the rects. */
int gba_led_heart(int x, int y, uint8_t c)
{
    cur_vu32_t *w;
    uint32_t c4 = c * 0x01010101u;
    if (gba_nodraw != 0 || ((x - 2) & 3) || x < 6 || x + 3 >= SCREEN_W || y < 3 || y + 2 >= SCREEN_H) return 0;
    w = (cur_vu32_t *)((uint8_t *)gba_back + (y - 3) * SCREEN_W + (x - 2));      /* w[0] = x-2..x+1 */
    w[0] = (w[0] & 0x00FF0000u) | (c4 & 0xFF00FFFFu);                           /* row y-3 */
    w[1] = (w[1] & 0xFFFFFF00u) | (c4 & 0x000000FFu);
    w += SCREEN_W / 4;
    w[-1] = (w[-1] & 0x00FFFFFFu) | (c4 & 0xFF000000u);                          /* rows y-2 .. y: x-3 .. x+3 */
    w[0] = c4;
    w[1] = (w[1] & 0xFFFF0000u) | (c4 & 0x0000FFFFu);
    w += SCREEN_W / 4;
    w[-1] = (w[-1] & 0x00FFFFFFu) | (c4 & 0xFF000000u);
    w[0] = c4;
    w[1] = (w[1] & 0xFFFF0000u) | (c4 & 0x0000FFFFu);
    w += SCREEN_W / 4;
    w[-1] = (w[-1] & 0x00FFFFFFu) | (c4 & 0xFF000000u);
    w[0] = c4;
    w[1] = (w[1] & 0xFFFF0000u) | (c4 & 0x0000FFFFu);
    w += SCREEN_W / 4;
    w[0] = c4;                                                                  /* row y+1: x-2 .. x+2 */
    w[1] = (w[1] & 0xFFFFFF00u) | (c4 & 0x000000FFu);
    w += SCREEN_W / 4;
    w[0] = (w[0] & 0xFF00FFFFu) | (c4 & 0x00FF0000u);                           /* row y+2: x */
    return 1;
}

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
            vid_glyph_dilate(vid_glyph_rows(ch), g);
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
    if (gba_nodraw == 1) return;
    if (level < 0) level = 0;
    if (level > 15) level = 15;
    if (level == 15) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x >= x1 || y >= y1) return;
    if (gba_nodraw) {
        int k;
        pn_op = PN_OP_SHADE;                    /* G7: not a panel's body */
        k = vid_route(x, y, x1, y1);
        pn_op = PN_OP_OTHER;
        if (k == 2) {
            bb_flatten(x, y, x1, y1);           /* G6: shade the billboards too */
            cv_shade(x, y, x1, y1, doomo_colormap[level]);
        }
        if (k != 1) return;
    }
    vid_shade(x, y, x1, y1, doomo_colormap[level]);
}

/* the slow path of the text calls: 1 = Mode 4, 2 = canvas, 0 = nothing */
static int text_route(int x, int y, const char *s, int pad)
{
    int x0, y0, x1, y1, k;
    cv_text_box(x, y, s, pad, &x0, &y0, &x1, &y1);
    if (x0 >= x1 || y0 >= y1)                  /* off screen: nothing to draw */
        return (gba_nodraw == 1 || gba_nodraw == GBA_NODRAW_CANVAS) ? 0 : 1;
    pn_op = PN_OP_TEXT;
    k = vid_route(x0, y0, x1, y1);
    pn_op = PN_OP_OTHER;
    return k;
}

void plat_text(int x, int y, const char *s, uint8_t color)
{
    if (gba_text_armed && s) gba_text_probe(s);   /* harness expect-text */
    if (gba_nodraw == 1 || !s) return;
    if (!color) color = PAL_BLACK;
    if (gba_nodraw && !pn_clear_rows(y, SCREEN_H)) {    /* (G7: text runs down from y) */
        int k = text_route(x, y, s, 0);
        if (k == 2) cv_text(x, y, s, color, 0);
        if (k != 1) return;
    }
    vid_text(x, y, s, color);
}

/* platform.h: the outline in one dilated pass, then the text (was ten
 * plat_text passes: 8% of a title frame) */
void plat_text_outline(int x, int y, const char *s, uint8_t color, uint8_t outline)
{
    if (gba_text_armed && s) gba_text_probe(s);   /* harness expect-text */
    if (gba_nodraw == 1 || !s) return;
    if (!color) color = PAL_BLACK;
    if (!outline) outline = PAL_BLACK;
    if (gba_nodraw && !pn_clear_rows(y - 2, SCREEN_H)) {
        int k = text_route(x, y, s, 2);
        if (k == 2) { cv_text(x, y, s, outline, 1); cv_text(x, y, s, color, 0); }
        if (k != 1) return;
    }
    vid_text_dil(x, y, s, outline);
    vid_text(x, y, s, color);
}

/* G5: a nonzero gba_nodraw (a skipped tick, a tile frame, the estimate)
 * takes the slow path in ROM (canvas.c), which calls back the *_m4 body.
 * (These two were in video.iwram.c; they only dispatch, so they live in ROM
 * and the IWRAM they used goes to the blit loops.) */
void plat_checker_slow(int x, int y, int w, int h, uint8_t color) ROM_CALL;
void plat_line_slow(int x0, int y0, int x1, int y1, uint8_t color) ROM_CALL;
void plat_checker_m4(int x, int y, int w, int h, uint8_t color) IWRAM_CALL;
void plat_line_m4(int x0, int y0, int x1, int y1, uint8_t color) IWRAM_CALL;

void plat_checker(int x, int y, int w, int h, uint8_t color)
{
    if (!gba_nodraw && iwram_ovl_current() == OVL_UI) vid_checker_ui(x, y, w, h, color);   /* SP6: blit2.c */
    else if (!gba_nodraw) plat_checker_m4(x, y, w, h, color);
    else if (gba_nodraw != 1) plat_checker_slow(x, y, w, h, color);
}

void plat_line(int x0, int y0, int x1, int y1, uint8_t color)
{
    if (!gba_nodraw) plat_line_m4(x0, y0, x1, y1, color);
    else if (gba_nodraw != 1) plat_line_slow(x0, y0, x1, y1, color);
}

/* 8bpp image, index 0 transparent, integer scale 1..4 */
void plat_image(int x, int y, const DoomoImage *img, int scale, int flags,
                uint8_t color)
{
    if (gba_nodraw == 1 || !img || !img->pix || !img->w || !img->h) return;
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    if (!color) color = PAL_BLACK;
    if (gba_nodraw && !pn_clear_rows(y, y + img->h * scale + 1)) {
        int sh = (flags & IMG_SHADOW) ? 1 : 0, k;
        int x0 = DOOMO_MAX(x, 0), y0 = DOOMO_MAX(y, 0);
        int x1 = DOOMO_MIN(x + img->w * scale + sh, SCREEN_W), y1 = DOOMO_MIN(y + img->h * scale + sh, SCREEN_H);
        if (x0 >= x1 || y0 >= y1) return;
        k = vid_route(x0, y0, x1, y1);
        if (k == 2) {
            if (sh) cv_blit(x + 1, y + 1, img, scale, (flags & IMG_FLIPX) | IMG_SILHOUETTE, PAL_BLACK);
            cv_blit(x, y, img, scale, flags & (IMG_FLIPX | IMG_SILHOUETTE), color);
        }
        if (k != 1) return;
    }
    if (scale == 2 && !(flags & IMG_FLIPX) && iwram_ovl_current() == OVL_UI) {
        /* SP6: the UI group's scale-2 loop (blit2.c), the same pixels as vid_blit_c */
        if (flags & IMG_SHADOW) vid_blit2(x + 1, y + 1, img, PAL_BLACK);
        vid_blit2(x, y, img, (flags & IMG_SILHOUETTE) ? color : 0u);
        return;
    }
    if (scale > 1 && iwram_ovl_current() == OVL_UI) {
        /* S2-5: any other scale or a flip, the UI group's ARM loop (blit2.c), the same pixels as vid_blit_c */
        if (flags & IMG_SHADOW) vid_blitn(x + 1, y + 1, img, scale, (flags & IMG_FLIPX) | IMG_SILHOUETTE, PAL_BLACK);
        vid_blitn(x, y, img, scale, flags & (IMG_FLIPX | IMG_SILHOUETTE), color);
        return;
    }
    if (flags & IMG_SHADOW)
        vid_blit(x + 1, y + 1, img, scale, (flags & IMG_FLIPX) | IMG_SILHOUETTE, PAL_BLACK);
    vid_blit(x, y, img, scale, flags & (IMG_FLIPX | IMG_SILHOUETTE), color);
}
