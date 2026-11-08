/* dots.c - plat_dots (plat_ext.h, SP4 / G-P1): a dot-matrix screen in one
 * call (owner: gba; docs/GBA.md "SP4").
 *
 * The pet's LCD (core/src/pet_scene.c) is a 64 x 44 buffer of dot values
 * drawn 2x on the GBA. It used to be one plat_rect per run of lit dots and
 * per row of its shadow, thousands of calls a frame (lcd_rect + lcd_sprite
 * were 36% of the scene). Here the whole buffer goes into the back page in
 * one ARM pass: a dot is one halfword (two pixels of its colour), a pair of
 * dots one word, stored on both of its two rows. Same pixels as the rects:
 * at s = 2 a dot is its fill colour all over (edges only exist at s >= 3).
 * plat_hshapes (the egg shell) runs the shell's rows the same way.
 *
 * The loop is ARM code of the IWRAM overlay's UI group (IWRAM_UI_CODE,
 * iwram_ovl.h): plat_dots loads the group (a compare when it is in). It
 * takes only the plain case: s == 2, x and the width multiples of 4, all
 * on screen, a Mode 4 frame; anything else answers 0 and the core draws
 * rects. A skipped tick draws nothing (1); a tile frame (the canvas) and
 * the slow paths (panels up, the tile estimate) go through vid_route like
 * one big plat_rect, so panels and the canvas see it as they saw the rects.
 */
#include "gba_plat.h"
#include "canvas_int.h"
#include "iwram_ovl.h"
#include "doomo/plat_ext.h"
#include "doomo/palette.h"
#include "video_int.h"

#define DOTS_ARM IWRAM_UI_CODE __attribute__((optimize("O2", "no-tree-loop-distribute-patterns")))

/* rows of dots -> pixel rows: a pair of dots (values < 8) becomes one word of the 64-entry table pal2 (both pixels of
 * both dots), four dots are read with one word load (the dot buffer is EWRAM: a byte at a time cost 3 cycles each),
 * and each word is stored on both pixel rows of its dots. dw is a multiple of 4, d and dst word aligned. */
DOTS_ARM void dots_expand2(volatile uint32_t *dst, const uint8_t *d, int dw, int dh, const uint32_t *pal2)
{
    int j, i, n = dw >> 2;
    for (j = 0; j < dh; j++, d += dw, dst += SCREEN_W / 2) {
        volatile uint32_t *r0 = dst, *r1 = dst + SCREEN_W / 4;
        const uint32_t *q = (const uint32_t *)d;
        for (i = 0; i < n; i++) {
            uint32_t v = q[i];
            uint32_t a = pal2[(v & 7) | ((v >> 5) & 0x38)], b = pal2[((v >> 16) & 7) | ((v >> 21) & 0x38)];
            r0[2 * i] = a; r1[2 * i] = a;
            r0[2 * i + 1] = b; r1[2 * i + 1] = b;
        }
    }
}

/* (plat_hshapes) the shapes in order, each row by row: the row's span clipped to the screen and filled by vid_rect
 * (the resident span fill plat_rect ends in), so the same fills in the same order as one plat_rect a row, without
 * the core's THUMB row loop and a call, veneer and clip test a row. Colour 0 is black, as plat_rect draws it. */
DOTS_ARM void hshapes_fill(const DoomoHShape *s, int n)
{
    for (; n > 0; n--, s++) {
        int cy = s->cy, y = cy - s->top, y1 = cy + s->bot;
        uint32_t c = s->c ? s->c : PAL_BLACK;
        if (y < 0) y = 0;
        if (y1 > SCREEN_H - 1) y1 = SCREEN_H - 1;
        for (; y <= y1; y++) {
            int hw = y < cy ? s->ht[cy - y] : s->hb[y - cy];
            int x0 = s->cx - hw, x1 = s->cx + hw + 1;
            if (x0 < 0) x0 = 0;
            if (x1 > SCREEN_W) x1 = SCREEN_W;
            if (x0 < x1) vid_rect(x0, y, x1, y + 1, c);
        }
    }
}

int plat_hshapes(const DoomoHShape *s, int n)
{
    if (gba_nodraw == 1) return 1;                  /* a skipped tick: nothing is drawn */
    if (gba_nodraw != 0) return 0;                  /* tiles, panels, the estimate: plat_rect's paths */
    iwram_ovl_use(OVL_UI);
    hshapes_fill(s, n);
    return 1;
}

int plat_dots(int x, int y, int s, const uint8_t *dots, int dw, int dh,
              const uint8_t fill[8], const uint8_t edge[8])
{
    uint32_t pal2[64];
    int i, w = dw * 2, h = dh * 2;
    (void)edge;                                     /* s == 2: no dot gaps */
    if (s != 2 || (x & 3) || (dw & 3) || ((uintptr_t)dots & 3) || dw <= 0 || dh <= 0 || x < 0 || y < 0 ||
        x + w > SCREEN_W || y + h > SCREEN_H)
        return 0;
    if (gba_nodraw == 1) return 1;                  /* a skipped tick: nothing is drawn */
    if (gba_nodraw == GBA_NODRAW_CANVAS) return 0;  /* a tile frame: the rects go to the canvas */
    if (gba_nodraw) {                               /* the slow path, as one plat_rect over it all */
        int k;
        pn_op = PN_OP_RECT;
        k = vid_route(x, y, x + w, y + h);
        pn_op = PN_OP_OTHER;
        if (k != 1) return k == 0;
    }
    for (i = 0; i < 64; i++) {                      /* (colour 0 is "transparent": plat_rect draws black) */
        uint32_t a = fill[i & 7] ? fill[i & 7] : PAL_BLACK, b = fill[i >> 3] ? fill[i >> 3] : PAL_BLACK;
        pal2[i] = (a | a << 8) | (b | b << 8) << 16;
    }
    iwram_ovl_use(OVL_UI);
    dots_expand2((volatile uint32_t *)(gba_back + y * (SCREEN_W / 2) + x / 2), dots, dw, dh, pal2);
    return 1;
}
