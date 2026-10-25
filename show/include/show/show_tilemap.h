/* show_tilemap.h - the TILEMAP plane (WP5; spec 8.3): the pixel-art sets as
 * 16x16 tiles in 8bpp atlas pages plus a map. Owner: ENGINE lane 2.
 *
 * The bake (showtk/l2/bake.py) dedupes a plate's tiles and packs them 49 to a
 * 128x128 atlas page: a 7x7 grid of 18x18 cells, each tile with a 1-texel
 * gutter that repeats its edge, so a scaled quad never samples a neighbour.
 * An atlas is one 128x128 tile of the PS1 image cache (one entry, one
 * upload); each map cell is one SPRT (scale 1) or one quad (scale moves and
 * px=2), placed through the plane transform with shared edges, so tiles
 * never open seams.
 *
 * Payload (4-byte aligned, little-endian):
 *   ShowTilemap, uint8_t atlas[natlas][128 * 128], uint16_t map[rows * cols]
 *   (map value 0xFFFF = an empty cell; tile k lives in atlas k / 49,
 *    cell k % 49 at texel (1 + 18 * (c % 7), 1 + 18 * (c / 7)))
 */
#ifndef SHOW_TILEMAP_H
#define SHOW_TILEMAP_H

#include <stdint.h>

struct ShowXform;

#define TM_TILE        16
#define TM_ATLAS       128
#define TM_CELL        18
#define TM_PER_ROW     7
#define TM_PER_ATLAS   49
#define TM_EMPTY       0xFFFF
#define SHOW_TILEMAP_PLANES 4
#define TM_MAX_ATLAS   12

typedef struct ShowTilemap {            /* 16 B                                     */
    uint16_t cols, rows;                /* map size in tiles                        */
    uint16_t ntiles;                    /* unique tiles                             */
    uint8_t  natlas, tile;              /* atlas pages that follow; 16              */
    uint32_t id;
    uint16_t flags, pad;
} ShowTilemap;

void show_tilemap_reset(void);
int  show_tilemap_bind(const void *payload, uint32_t bytes);   /* handle or -1 */
void show_tilemap_draw(int h, const struct ShowXform *xf);
/* the tile index of map cell (c, r) (TM_EMPTY outside) - tests, lints */
int  show_tilemap_cell(int h, int c, int r);

#endif
