/* tilerow.h - one row of an image-cache tile with its transparent border
 * (owner: ps1; RAM headroom 2026-11-12, docs/PS1.md).
 *
 * vram.c gives every cached tile a transparent border: one texel on the
 * left, the texels from tw + 1 up to the (even) upload width on the right,
 * and one row below. gfx.c writes the bordered rows straight into the GPU
 * upload packets with this (it used to copy them from a 16.9 KB staging
 * buffer). Host test: harness/tilerow_test.c (the old staging buffer is its
 * reference).
 */
#ifndef PS1_TILEROW_H
#define PS1_TILEROW_H
#include <stdint.h>
#include <string.h>

/* Writes n bytes (the upload width in texels, even, >= tw + 2) to b: one
 * transparent texel, tw texels read from src in steps of `step` (+1, or -1
 * for a mirrored image: src is then the row's LAST texel of the tile in the
 * image), then transparent texels; `blank` = the row below the tile, all
 * transparent (src is not read). */
static inline void tile_row(uint8_t *b, int n, const uint8_t *src, int tw, int step, int blank)
{
    int k = 0;
    if (!blank) {
        b[k++] = 0;
        if (step > 0) {
            memcpy(b + 1, src, (size_t)tw);
            k += tw;
        } else {
            for (; k <= tw; k++)
                b[k] = src[1 - k];
        }
    }
    while (k < n)
        b[k++] = 0;
}

#endif
