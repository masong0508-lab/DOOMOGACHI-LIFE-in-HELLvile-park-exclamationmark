/* world.h - per-map caches shared by the 3D views (owner: ps1). */
#ifndef PS1_WORLD_H
#define PS1_WORLD_H

#include "ps1.h"

#define WORLD_GW (DOOMO_MAP_MAX_W + 1)

extern int      g_world_sky_top;              /* highest roof/ceiling + margin */
/* height of a solid block standing in open air (reference semantics:
 * its ceil_h when above its floor_h, else floor + 256) */
extern int16_t  g_world_wtop[DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H];
extern uint32_t g_world_corner[WORLD_GW * (DOOMO_MAP_MAX_H + 1)];

void world_sync(const DoomoMap *m);           /* once per frame per view */
void world_invalidate(void);

static inline uint32_t world_corner(int gx, int gy)
{
    return g_world_corner[gy * WORLD_GW + gx];
}

/* floor / ceiling of a cell in world units (doors & lifts animated) */
static inline int world_floor(const DoomoMap *m, const DoomoCell *c, int cx, int cy)
{
    return (c->flags & (CELL_DOOR | CELL_LIFT)) ? core_floor_z(m, cx, cy)
                                                : c->floor_h * DOOMO_HEIGHT_STEP;
}
static inline int world_ceil(const DoomoMap *m, const DoomoCell *c, int cx, int cy)
{
    return (c->flags & (CELL_DOOR | CELL_LIFT)) ? core_ceil_z(m, cx, cy)
                                                : c->ceil_h * DOOMO_HEIGHT_STEP;
}

#endif
