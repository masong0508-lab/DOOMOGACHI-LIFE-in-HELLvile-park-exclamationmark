/* PS2 software framebuffer: fast 1x image composition when fully visible.
 * Index 0 is transparent. No allocations and no assumptions about the
 * image's row alignment or x-flip. The caller handles clipped/scaled
 * images and drop shadows using the generic renderer.
 *
 * Kept independent of ps2sdk so the exact path is tested with a host C
 * compiler before the EE build runs. */
#ifndef DOOMO_PS2_INDEXED_BLIT_H
#define DOOMO_PS2_INDEXED_BLIT_H

#include <stdint.h>

static inline int ps2_blit_visible_1x(uint8_t *screen, int sw, int sh,
                                      int x, int y, const uint8_t *src,
                                      int iw, int ih, int flip_x,
                                      int silhouette, uint8_t color)
{
    int iy, ix;
    if (!screen || !src || iw <= 0 || ih <= 0 || sw <= 0 || sh <= 0 ||
        iw > sw || ih > sh || x < 0 || y < 0 ||
        x > sw - iw || y > sh - ih)
        return 0;

    for (iy = 0; iy < ih; iy++) {
        const uint8_t *row = src + iy * iw;
        uint8_t *dst = screen + (y + iy) * sw + x;
        if (flip_x) {
            for (ix = 0; ix < iw; ix++) {
                uint8_t c = row[iw - 1 - ix];
                if (c) dst[ix] = silhouette ? color : c;
            }
        } else {
            for (ix = 0; ix < iw; ix++) {
                uint8_t c = row[ix];
                if (c) dst[ix] = silhouette ? color : c;
            }
        }
    }
    return 1;
}

#endif
