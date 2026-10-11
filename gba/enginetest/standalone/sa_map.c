/* sa_map.c - NOCORE=1 stand-ins for the core's map runtime (owner: gba).
 * Only linked into the standalone engine test (see sa_art.c). */
#include "doomo/doomo.h"

DoomoMap doomo_map;
const DoomoMapDef *const doomo_maps[MAPID_COUNT];

const DoomoCell *map_cell_safe(const DoomoMap *m, int cx, int cy)
{
    static const DoomoCell solid = { 1, 1, 1, 0, 0, 8, 0, 0 };
    if (!m || cx < 0 || cy < 0 || cx >= m->w || cy >= m->h) return &solid;
    return &m->cells[cy * m->w + cx];
}

int map_floor_z(const DoomoMap *m, int cx, int cy)
{
    const DoomoCell *c = map_cell_safe(m, cx, cy);
    if (c->flags & CELL_LIFT) {
        /* showcase lift: rides between floor_h and tag every 8 seconds */
        int t = m->time % 240, top = (int8_t)c->tag * DOOMO_HEIGHT_STEP;
        int bot = c->floor_h * DOOMO_HEIGHT_STEP, z;
        if (t < 60) z = bot;
        else if (t < 120) z = bot + (top - bot) * (t - 60) / 60;
        else if (t < 180) z = top;
        else z = top - (top - bot) * (t - 180) / 60;
        return z;
    }
    return c->floor_h * DOOMO_HEIGHT_STEP;
}

int map_ceil_z(const DoomoMap *m, int cx, int cy)
{
    return map_cell_safe(m, cx, cy)->ceil_h * DOOMO_HEIGHT_STEP;
}

void map_tick(DoomoMap *m) { if (m) m->time++; }
void map_load(int map_id) { (void)map_id; }
void map_use(DoomoMap *m, int cx, int cy) { (void)m; (void)cx; (void)cy; }

/* the rest of the core the platform calls (NOCORE stand-ins) */
int opt_get(int opt) { (void)opt; return 1; }      /* OPT_FOV: normal */
const int8_t *audio_sfx_pcm(int sfx, uint32_t *len, uint32_t *rate)
{
    static const int8_t blip[16] = { 0, 40, 60, 40, 0, -40, -60, -40,
                                     0, 20, 30, 20, 0, -20, -30, -20 };
    (void)sfx;
    if (len) *len = sizeof(blip);
    if (rate) *rate = 8000;
    return blip;
}
