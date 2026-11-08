/* blit2.c - plat_ui_loops (plat_ext.h, SP6 / G-K1) and the scale-2 image loop of the IWRAM UI group
 * (owner: gba; docs/GBA.md "SP6: the concert").
 *
 * The concert draws its front row of fans and the band at scale 2 (32x48 for a 16x24 chibi): six to ten
 * images a frame. vid_blit_c (video.iwram.c), the general scaled / flipped path, builds a line of the
 * scaled row and then merges it into the page pair by pair: about 2.6 ms for one such chibi, 10-20 ms of
 * the concert's frame. Here a scale-2 image without a flip is drawn straight from its source rows: a source
 * pixel is two pixels on two rows, so on an even x it is ONE halfword stored on both rows (never a read of
 * the page); on an odd x every halfword holds two neighbouring source pixels (the odd-source trick of the
 * scale-1 ARM blit), stored whole when both are opaque and merged with the page only when one of them is
 * transparent. Same pixels as vid_blit_c, every case: index 0 is transparent, IMG_SILHOUETTE paints every
 * opaque pixel `color`, clipped to the screen on all four sides.
 *
 * The loop is ARM code of the IWRAM overlay's UI group (IWRAM_UI_CODE, iwram_ovl.h; the same group as SP4's
 * dots.c). plat_ui_loops() loads the group for a scene that asks for it (the concert, at the start of its
 * draw); plat_image (video.c) takes vid_blit2 only while the group is in, so a scene that never asks, a 3D
 * view or a tile frame that loads its own group later in the frame, all keep the old path.
 */
#include "gba_plat.h"
#include "iwram_ovl.h"
#include "doomo/plat_ext.h"
#include "video_int.h"

#define BLIT2_ARM IWRAM_UI_CODE __attribute__((optimize("O2", "no-tree-loop-distribute-patterns")))

#define LINE (SCREEN_W / 2)

/* one source row at scale 2 onto dest rows r0 (and r1 = r0 + LINE when two) between halfwords q0 .. q1 - 1;
 * even: x is even and source pixel j lands in halfword xh + j; odd: halfword q holds source pixels
 * j = q - xh - 1 (low byte) and j + 1 (high byte), xh = (x - 1) / 2 rounded down, pixels -1 and w empty. */
BLIT2_ARM void vid_blit2_row(volatile uint16_t *r0, int two, const uint8_t *s, int w, int xh, int q0, int q1,
                             int odd, uint32_t sil)
{
    volatile uint16_t *r1 = r0 + LINE;
    int q;
    if (!odd) {
        for (q = q0; q < q1; q++) {
            uint32_t v = s[q - xh];
            if (!v) continue;
            if (sil) v = sil;
            v |= v << 8;
            r0[q] = (uint16_t)v;
            if (two) r1[q] = (uint16_t)v;
        }
        return;
    }
    for (q = q0; q < q1; q++) {
        int j = q - xh - 1;
        uint32_t lo = j >= 0 ? s[j] : 0u, hi = j + 1 < w ? s[j + 1] : 0u;
        if (!(lo | hi)) continue;
        if (sil) { if (lo) lo = sil; if (hi) hi = sil; }
        if (lo && hi) {
            uint32_t v = lo | hi << 8;
            r0[q] = (uint16_t)v;
            if (two) r1[q] = (uint16_t)v;
        } else if (lo) {
            r0[q] = (uint16_t)((r0[q] & 0xFF00u) | lo);
            if (two) r1[q] = (uint16_t)((r1[q] & 0xFF00u) | lo);
        } else {
            r0[q] = (uint16_t)((r0[q] & 0x00FFu) | hi << 8);
            if (two) r1[q] = (uint16_t)((r1[q] & 0x00FFu) | hi << 8);
        }
    }
}

/* Draw img at scale 2, no flip, at (x, y) into the back page; sil != 0: every opaque pixel is that colour.
 * THUMB, ROM: the clipping and the row walk; the rows are the group's ARM loop. */
void vid_blit2(int x, int y, const DoomoImage *img, uint32_t sil)
{
    const int w = img->w, h = img->h;
    int x0 = x, x1 = x + 2 * w, sy, odd = x & 1, xh, q0, q1;
    if (x0 < 0) x0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (x0 >= x1 || y >= SCREEN_H || y + 2 * h <= 0) return;
    /* halfwords: even x -> source j at (x >> 1) + j; odd x -> xh = (x - 1) >> 1 (arithmetic) */
    xh = odd ? (x - 1) >> 1 : x >> 1;
    q0 = x0 >> 1;
    q1 = (x1 + 1) >> 1;
    for (sy = 0; sy < h; sy++) {
        int ry = y + 2 * sy, two = 1;
        if (ry + 1 < 0) continue;
        if (ry >= SCREEN_H) break;
        if (ry < 0) { ry = 0; two = 0; }                 /* only the second row is on screen */
        else if (ry + 1 >= SCREEN_H) two = 0;           /* only the first */
        vid_blit2_row((volatile uint16_t *)gba_back + ry * LINE, two, img->pix + sy * w, w, xh, q0, q1, odd, sil);
    }
}

int plat_ui_loops(void)
{
    if (gba_nodraw == 1) return 0;                      /* a skipped tick: nothing is drawn */
    iwram_ovl_use(OVL_UI);
    return 1;
}
