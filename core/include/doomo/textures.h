/* textures.h - procedural 64x64 world textures and skies (owner: art).
 * Generated at BUILD time: the generator code (core/art/ *.c) is compiled
 * natively into core/tools/artgen, which writes const arrays to
 * core/gen/art_data.c (committed; CI checks it is up to date). On GBA the
 * pixels therefore live in ROM and cost no RAM. textures_init() only sets
 * up lookup tables.
 *
 * ART DIRECTION: the CUTE set is pastel candy-land (it is what the town
 * looks like before the invasion); the HELL set is grotesque Doom-style
 * (flesh, bone, blood, lava, rusty tech, vape-cloud walls).
 */
#ifndef DOOMO_TEXTURES_H
#define DOOMO_TEXTURES_H

#include "types.h"

enum {
    TEX_NONE = 0,
    /* cute overworld + cute interiors */
    TEX_GRASS, TEX_FLOWERS, TEX_PATH, TEX_COBBLE, TEX_SAND, TEX_WATER,
    TEX_PASTEL_BRICK, TEX_CANDY_STRIPE, TEX_WOOD_FLOOR, TEX_WOOD_WALL,
    TEX_ROOF_TILES, TEX_HEDGE, TEX_PICKET_FENCE, TEX_SHOP_FRONT,
    TEX_CHURCH_WALL, TEX_STAINED_GLASS, TEX_CARPET, TEX_WALLPAPER_HEARTS,
    TEX_CLOUD_PUFF, TEX_TREE, TEX_PLAYGROUND, TEX_VAPE_SHOP_FRONT,
    TEX_APARTMENT_FRONT, TEX_WORKSHOP_FRONT, TEX_CAFE_FRONT, TEX_TILE_FLOOR,
    /* hell */
    TEX_FLESH_WALL, TEX_BONE_WALL, TEX_BLOOD_BRICK, TEX_LAVA, TEX_RUST_METAL,
    TEX_TECH_PANEL, TEX_SKULL_MOSAIC, TEX_DEMON_EYES, TEX_CHAINS,
    TEX_VAPE_CLOUD_WALL, TEX_HELL_ROCK, TEX_GORE_FLOOR, TEX_METAL_FLOOR,
    TEX_GRATE, TEX_PENTAGRAM_FLOOR, TEX_INFECTED_GRASS, TEX_SLIME,
    TEX_DOOR_CUTE, TEX_DOOR_HELL, TEX_EXIT_SIGN, TEX_SWITCH,
    TEX_COUNT
};
DOOMO_STATIC_ASSERT(TEX_COUNT <= 64, textures_fit);

/* Animated textures (water, lava, slime, portal) return a different frame
 * pointer depending on `time` (ticks). */
const uint8_t *texture_pixels(int tex_id, uint16_t time); /* 64*64 row-major */
const DoomoImage *texture_image(int tex_id, uint16_t time);
int  texture_is_animated(int tex_id);
/* Average colour of a texture (palette idx) for minimaps / far LOD. */
uint8_t texture_avg(int tex_id);

enum { SKY_PASTEL_DAY, SKY_SUNSET, SKY_NIGHT_STARS, SKY_HELL, SKY_VAPE_STORM,
       SKY_COUNT };
#define SKY_W 256
#define SKY_H 96
/* 256x96, wraps horizontally once per 360 degrees. */
const uint8_t *sky_pixels(int sky_id, uint16_t time);

void textures_init(void);

#endif
