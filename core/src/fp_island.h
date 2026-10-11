/* fp_island.h - private glue between the island hub (fp_island.c) and its
 * illustration (fp_island_gfx.c). Owner: freeplay. */
#ifndef DOOMO_FP_ISLAND_H
#define DOOMO_FP_ISLAND_H

#include "fp_int.h"

/* Location kinds (buildings drawn by fp_island_building). */
enum { LOC_TOWERS, LOC_PITS, LOC_TOWN_HALL, LOC_SHOP, LOC_BLACK_MARKET, LOC_CAFE,
       LOC_CHURCH, LOC_HOSPITAL, LOC_FUN_FAIR, LOC_LIBRARY, LOC_CONCERT, LOC_WORKSHOP,
       LOC_BEACH, LOC_PARK, LOC_WALL, LOC_ARENA, LOC_SQUARE, LOC_LAKE, LOC_GRAVEYARD,
       LOC_COUNT,
       /* cursor targets that are not buildings */
       LOC_PET = LOC_COUNT, LOC_ROSTER, LOC_MENU };

#define FP_LOC_MAX 24
#define FP_ISLAND_TAG SAVE_EXT_TAG('F', 'P', 'I', 'S')

typedef struct { int x, y; uint8_t kind, locked; } FpLocDraw;
typedef struct { int x, y, w, h; } FpMap;

/* The island's per-screen geometry (coastline, river per map row). It
 * lives in the shared scene scratch (scene.h) together with the hub's
 * menu, so it costs no RAM while other scenes run; fp_island_geo()
 * re-claims and the geometry is rebuilt whenever another scene took it. */
#define FP_GEO_ROWS 260
typedef struct {
    FpMap   map;
    int16_t geo_w, geo_h;
    int16_t coast_l[FP_GEO_ROWS], coast_r[FP_GEO_ROWS], river_c[FP_GEO_ROWS];
} FpIslandGeo;
FpIslandGeo *fp_island_geo(void);

enum { WX_CLEAR, WX_RAIN, WX_SNOW, WX_PETALS };

const FpMap *fp_map(void);
int  fp_mx(int u);                 /* map units 0..255 -> screen */
int  fp_my(int u);
void fp_island_geometry(void);
int  fp_island_side_at(int x, int y);
void fp_island_building(int kind, int x, int y, int locked);
int  fp_island_weather(void);
int  fp_island_tint(void);
/* Sea, land, river + Wall, roads, decorations, walkers, the buildings
 * (back to front; sel bounces) and the weather. */
void fp_island_draw_map(const FpLocDraw *locs, int n, int sel);

#endif
