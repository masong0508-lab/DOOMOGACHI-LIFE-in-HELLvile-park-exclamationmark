/* et_map.c - the GBA engine-test showcase map (owner: gba).
 *
 * Used by the ENGINE_TEST ROM when the core's map registry has no Hell
 * Breach map yet. It exercises everything plat_view3d() can do:
 *   courtyard under the sky: hedges (low blocks you look over), a house
 *     block with a roof and a door, a tall tower, crates, a pond, a path
 *   the hall: stairs up to a mezzanine (step faces), a crate on it, a
 *     skylight, pillars, a lava pit, a lift, a door to the courtyard
 *   a low-ceilinged tech corridor down into a dark hell room with flesh
 *     pillars and a raised pentagram dais
 */
#include "doomo/doomo.h"
#include "enginetest.h"

static const char *const et_rows[ET_MAP_H] = {
    "################################",
    "#,,,,,,,,,,,,,,,#HHHHHHHHHHHHHH#",
    "#,hhhhh,,,,,,hh,#HHHHHH1234PPPP#",
    "#,h,,,,,,___,,,,#HHSSHH1234PPPP#",
    "#,,,,,,,,_,_,TT,#HHSSHHHHHHPcPP#",
    "#,,BBBBB,_,,,TT,#HHHHHHHHHHPPPP#",
    "#,,B...B,_,,,,,,#HHpHHpHLLLHHFH#",
    "#,,B...B,______DHHHHHHHHLLLHHHH#",
    "#,,BBDBB,_,,,,,,#HHpHHpHLLLHHHH#",
    "#,,,,_,,,_,,WWW,#HHHHHHHHHHHHHH#",
    "#,,,,_,,,_,,WWW,#HHHHHHHHHHHHHH#",
    "#,,,,______,,,,,######ll########",
    "#,cc,,,,,,_,,,,,######ll########",
    "#,cc,,,,,,_,,hh,######ll########",
    "#,,,,,,,,,_,,hh,#ggggggggggggggX",
    "#,,hhhhh,,_,,,,,#ggggggggggggggX",
    "#,,,,,,,,,_,,,,,#ggXXggggggXXggX",
    "#,TT,,,,,,_,,,,,#ggXXggRRggXXggX",
    "#,TT,,,,,,_,,,,,#ggggggRRggggggX",
    "#,,,,,,,,,_,,,,,#ggXXggggggXXggX",
    "#,,,,,,,,,_,,,,,#ggXXggggggXXggX",
    "#,,,,,,,,,_,,,,,#ggggggggggggggX",
    "#,,,,,,,,,,,,,,,#ggggggggggggggX",
    "################################",
};

typedef struct Legend {
    char ch;
    DoomoCell c;            /* wall, floor_tex, ceil_tex, floor_h, ceil_h, light, flags, tag */
} Legend;

#define SKY DOOMO_TEX_SKY
static const Legend legend[] = {
    /* solid, full height (ceil_h <= floor_h: up to the ceiling) */
    { '#', { TEX_PASTEL_BRICK, TEX_COBBLE, TEX_COBBLE, 0, 0, 12, 0, 0 } },
    { 'X', { TEX_FLESH_WALL, TEX_GORE_FLOOR, TEX_GORE_FLOOR, 0, 0, 7, 0, 0 } },
    { 'p', { TEX_CANDY_STRIPE, TEX_TILE_FLOOR, TEX_TILE_FLOOR, 0, 0, 12, 0, 0 } },
    /* blocks with a height (ceil_h = top): you can see over them */
    { 'h', { TEX_HEDGE, TEX_HEDGE, SKY, 0, 5, 15, 0, 0 } },
    { 'B', { TEX_WOOD_WALL, TEX_ROOF_TILES, SKY, 0, 14, 15, 0, 0 } },
    { 'T', { TEX_CHURCH_WALL, TEX_COBBLE, SKY, 0, 30, 15, 0, 0 } },
    { 'c', { TEX_WOOD_WALL, TEX_WOOD_FLOOR, SKY, 0, 4, 15, 0, 0 } },
    /* open cells */
    { ',', { 0, TEX_GRASS, SKY, 0, 24, 15, 0, 0 } },
    { '_', { 0, TEX_PATH, SKY, 0, 24, 15, 0, 0 } },
    { 'W', { 0, TEX_WATER, SKY, -2, 24, 15, CELL_WATER, 0 } },
    { '.', { 0, TEX_CARPET, TEX_WOOD_FLOOR, 0, 12, 12, 0, 0 } },
    { 'D', { 0, TEX_WOOD_FLOOR, TEX_DOOR_CUTE, 0, 11, 13, CELL_DOOR, 0 } },
    { 'H', { 0, TEX_TILE_FLOOR, TEX_CLOUD_PUFF, 0, 20, 12, 0, 0 } },
    { 'S', { 0, TEX_TILE_FLOOR, SKY, 0, 20, 14, 0, 0 } },
    { '1', { 0, TEX_WOOD_FLOOR, TEX_CLOUD_PUFF, 2, 20, 12, 0, 0 } },
    { '2', { 0, TEX_WOOD_FLOOR, TEX_CLOUD_PUFF, 4, 20, 12, 0, 0 } },
    { '3', { 0, TEX_WOOD_FLOOR, TEX_CLOUD_PUFF, 6, 20, 12, 0, 0 } },
    { '4', { 0, TEX_WOOD_FLOOR, TEX_CLOUD_PUFF, 8, 20, 12, 0, 0 } },
    { 'P', { 0, TEX_CARPET, TEX_CLOUD_PUFF, 8, 20, 13, 0, 0 } },
    { 'L', { 0, TEX_LAVA, TEX_CLOUD_PUFF, -4, 20, 15, CELL_LAVA, 0 } },
    { 'F', { 0, TEX_METAL_FLOOR, TEX_CLOUD_PUFF, 0, 20, 12, CELL_LIFT, 8 } },
    { 'l', { 0, TEX_METAL_FLOOR, TEX_TECH_PANEL, 0, 8, 9, 0, 0 } },
    { 'g', { 0, TEX_GORE_FLOOR, TEX_RUST_METAL, 0, 14, 6, 0, 0 } },
    { 'R', { 0, TEX_PENTAGRAM_FLOOR, TEX_RUST_METAL, 3, 14, 10, 0, 0 } },
};

static const DoomoMapDef et_def = {
    "ENGINE TEST", ET_MAP_W, ET_MAP_H, MAP_DUNGEON, SKY_PASTEL_DAY, 0, 12,
    NULL, NULL, 0
};

void et_build_map(DoomoMap *m)
{
    int x, y;
    memset(m, 0, sizeof(*m));
    m->def = &et_def;
    m->w = ET_MAP_W;
    m->h = ET_MAP_H;
    m->kind = MAP_DUNGEON;
    m->sky = SKY_PASTEL_DAY;
    m->ambient = 12;
    for (y = 0; y < ET_MAP_H; y++) {
        for (x = 0; x < ET_MAP_W; x++) {
            char ch = et_rows[y][x];
            unsigned i;
            DoomoCell *c = &m->cells[y * ET_MAP_W + x];
            *c = legend[0].c;
            for (i = 0; i < sizeof(legend) / sizeof(legend[0]); i++)
                if (legend[i].ch == ch) { *c = legend[i].c; break; }
        }
    }
}
