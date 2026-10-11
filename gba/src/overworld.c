/* overworld.c - plat_overworld(): cutesy top-down view (owner: gba).
 * (first bring-up version: flat cells in their texture's average colour) */
#include "gba_plat.h"
#include "doomo/textures.h"
#include "doomo/palette.h"

void ow_init(void) { }

void plat_overworld(const DoomoOverworldView *v)
{
    int x, y;
    const DoomoMap *m;
    if (gba_nodraw || !v || !v->map) return;
    m = v->map;
    for (y = 0; y < v->vh; y += 16) {
        for (x = 0; x < v->vw; x += 16) {
            int wx = (v->cx >> 16) - v->vw / 2 + x, wy = (v->cy >> 16) - v->vh / 2 + y;
            int cx = wx >> 6, cy = wy >> 6;
            uint8_t c = PAL_BLACK;
            if (cx >= 0 && cy >= 0 && cx < m->w && cy < m->h) {
                const DoomoCell *cl = &m->cells[cy * m->w + cx];
                c = texture_avg(cl->wall ? cl->wall : cl->floor_tex);
            }
            plat_rect(v->vx + x, v->vy + y, 16, 16, c);
        }
    }
}
