/* world.c - per-map caches shared by the 3D views (owner: ps1).
 *
 * Everything about the map that does not change from frame to frame is
 * computed once when a new map appears (and then refreshed a few rows per
 * frame, so light changes, blown-up walls etc. show up within a second):
 *  - the smoothed light colour of every grid corner (average of the open
 *    cells around it, tinted by lava / water / portal glow)
 *  - the height walls reach next to open-sky cells (s_sky_top)
 * Per frame the views only read these tables, which keeps the per-cell
 * cost of the renderer down to emitting its faces.
 */
#include <string.h>
#include "r3d.h"
#include "world.h"

#define GW (DOOMO_MAP_MAX_W + 1)
#define GH (DOOMO_MAP_MAX_H + 1)

static const DoomoMap    *s_map;
static const DoomoMapDef *s_def;
static uint8_t  s_w, s_h;
static uint16_t s_last_time;
static int      s_row;
int             g_world_sky_top;
int16_t         g_world_wtop[DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H];
uint32_t        g_world_corner[GW * GH];

static uint32_t cell_glow(const DoomoCell *c, int *add)
{
    *add = 0;
    if (c->flags & CELL_LAVA)   { *add = 6; return 0x6eaaff; }   /* 0xBBGGRR */
    if (c->flags & CELL_PORTAL) { *add = 5; return 0xff96e6; }
    if (c->flags & CELL_WATER)  { return 0xffd2aa; }
    if (c->flags & CELL_EXIT)   { *add = 3; return 0xc8ffc8; }
    return 0xffffff;
}

static const uint16_t k_recip[5] = { 0, 256, 128, 85, 64 };

static void corner_row(int gy)
{
    const DoomoMap *m = s_map;
    int gx, w = m->w, h = m->h;
    for (gx = 0; gx <= w; gx++) {
        uint32_t r = 0, g = 0, b = 0, n = 0;
        int dy, dx;
        for (dy = -1; dy <= 0; dy++) {
            int cy = gy + dy;
            if (cy < 0 || cy >= h)
                continue;
            for (dx = -1; dx <= 0; dx++) {
                int cx = gx + dx, add, l;
                const DoomoCell *c;
                uint32_t t, br;
                if (cx < 0 || cx >= w)
                    continue;
                c = &m->cells[cy * w + cx];
                if (c->wall)
                    continue;
                t = cell_glow(c, &add);
                l = (c->light ? c->light : m->ambient) + add;
                if (l > 18) l = 18;
                br = 26 + ((uint32_t)l * 444 >> 6);      /* 26 .. ~150 */
                r += ((t & 0xff) * br) >> 8;
                g += (((t >> 8) & 0xff) * br) >> 8;
                b += (((t >> 16) & 0xff) * br) >> 8;
                n++;
            }
        }
        if (n) {
            uint32_t k = k_recip[n];
            r = (r * k) >> 8; g = (g * k) >> 8; b = (b * k) >> 8;
            g_world_corner[gy * GW + gx] = r | (g << 8) | (b << 16);
        } else {
            g_world_corner[gy * GW + gx] = 0x303030;
        }
    }
}

static void sky_top(void)
{
    const DoomoMap *m = s_map;
    int i, n = m->w * m->h, top = -32768;
    for (i = 0; i < n; i++) {
        const DoomoCell *c = &m->cells[i];
        int v;
        if (c->wall) {
            v = c->ceil_h > c->floor_h ? c->ceil_h * DOOMO_HEIGHT_STEP
                                       : c->floor_h * DOOMO_HEIGHT_STEP + 256;
            g_world_wtop[i] = (int16_t)v;
        } else {
            v = c->ceil_tex == DOOMO_TEX_SKY ? c->floor_h * DOOMO_HEIGHT_STEP + 256
                                             : c->ceil_h * DOOMO_HEIGHT_STEP;
        }
        if (v > top)
            top = v;
    }
    if (top < 128) top = 128;
    g_world_sky_top = (top + 63) & ~63;
}

void world_sync(const DoomoMap *m)
{
    int gy;
    if (m != s_map || m->def != s_def || m->w != s_w || m->h != s_h ||
        m->time < s_last_time) {
        s_map = m;
        s_def = m->def;
        s_w = m->w;
        s_h = m->h;
        for (gy = 0; gy <= m->h; gy++)
            corner_row(gy);
        sky_top();
        s_row = 0;
    } else {
        /* refresh one row every other frame (lights / walls changed by the
         * game show up within ~4 s on the largest map) */
        if (m->time & 1) {
            corner_row(s_row);
            s_row = s_row >= m->h ? 0 : s_row + 1;
            if (s_row == 0)
                sky_top();
        }
    }
    s_last_time = m->time;
}

/* Force a full rebuild on the next world_sync (e.g. the engine test
 * switches between maps that share the same DoomoMap storage). */
void world_invalidate(void) { s_map = NULL; }
