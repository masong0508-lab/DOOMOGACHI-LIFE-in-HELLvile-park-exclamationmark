/* tilerow_test.c - host test of the image-cache tile rows (ps1/src/tilerow.h).
 *
 *   cc -O2 -I ps1/src ps1/harness/tilerow_test.c -o tilerow_test
 *   ./tilerow_test
 *
 * RAM headroom 2026-11-12: gfx.c writes a tile's bordered rows straight into
 * the GPU upload packets (tile_row) instead of copying them from the 130 x
 * 130 staging buffer vram.c used to fill. The reference here IS that old
 * staging code; every tile shape the cache can ask for (up to 128 x 128, plain and
 * mirrored, at the image's left edge, right edge and in the middle, a full
 * tile and the narrow / short ones at the image's end) must give the same bytes.
 * Exit code 0 = pass.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tilerow.h"

#define IMG_TILE 128

/* vram.c vram_image() before the change: the tile with its border, staged */
static void stage_ref(uint8_t *stage, int sw, const uint8_t *pix, int img_w, int x0, int y0,
                      int tw, int th, int flip)
{
    int row, col;
    memset(stage, 0, (size_t)sw * (th + 1));
    for (row = 0; row < th; row++) {
        const uint8_t *src = pix + (size_t)(y0 + row) * img_w;
        uint8_t *d = stage + row * sw + 1;
        if (!flip) {
            memcpy(d, src + x0, (size_t)tw);
        } else {
            int sx = img_w - 1 - x0;
            for (col = 0; col < tw; col++)
                d[col] = src[sx - col];
        }
    }
}

/* vram.c + gfx.c now: the rows as upload_rows() writes them into the packets */
static void rows_now(uint8_t *out, int sw, const uint8_t *pix, int img_w, int x0, int y0,
                     int tw, int th, int flip)
{
    const uint8_t *src = pix + (size_t)y0 * img_w + (flip ? img_w - 1 - x0 : x0);
    int h = th + 1, r;
    for (r = 0; r < h; r++) {
        tile_row(out + r * sw, sw, src, tw, flip ? -1 : 1, h - r <= 1);
        src += img_w;
    }
}

int main(void)
{
    static uint8_t ref[(IMG_TILE + 2) * (IMG_TILE + 2)], now[(IMG_TILE + 2) * (IMG_TILE + 2)];
    static const int widths[] = { 1, 2, 3, 7, 64, 127, 128, 129, 200, 300 };
    unsigned wi;
    long tiles = 0;
    for (wi = 0; wi < sizeof(widths) / sizeof(widths[0]); wi++) {
        int img_w = widths[wi], img_h = 150, i, flip, x0, y0;
        /* exactly the image: a read outside it is a fault under a sanitizer */
        uint8_t *pix = (uint8_t *)malloc((size_t)img_w * img_h);
        if (!pix) return 2;
        for (i = 0; i < img_w * img_h; i++)
            pix[i] = (uint8_t)(1 + (i * 7 + i / img_w * 13) % 255);     /* never 0: a border byte shows */
        for (flip = 0; flip < 2; flip++)
            for (x0 = 0; x0 < img_w; x0 += IMG_TILE)
                for (y0 = 0; y0 < img_h; y0 += IMG_TILE) {
                    int tw = img_w - x0 < IMG_TILE ? img_w - x0 : IMG_TILE;
                    int th = img_h - y0 < IMG_TILE ? img_h - y0 : IMG_TILE;
                    int sw = (tw + 2 + 1) & ~1;
                    memset(ref, 0xaa, sizeof(ref));
                    memset(now, 0x55, sizeof(now));
                    stage_ref(ref, sw, pix, img_w, x0, y0, tw, th, flip);
                    rows_now(now, sw, pix, img_w, x0, y0, tw, th, flip);
                    if (memcmp(ref, now, (size_t)sw * (th + 1))) {
                        printf("tilerow: FAIL image %dx%d tile at (%d,%d) %dx%d flip %d\n",
                               img_w, img_h, x0, y0, tw, th, flip);
                        return 1;
                    }
                    /* nothing written past the tile's rows */
                    if ((size_t)sw * (th + 1) < sizeof(now) && now[(size_t)sw * (th + 1)] != 0x55) {
                        printf("tilerow: FAIL wrote past the rows (image %d wide, tile %dx%d)\n", img_w, tw, th);
                        return 1;
                    }
                    tiles++;
                }
        free(pix);
    }
    printf("tilerow: PASS (%ld tiles, plain and mirrored, the same bytes as the staging buffer)\n", tiles);
    return 0;
}
