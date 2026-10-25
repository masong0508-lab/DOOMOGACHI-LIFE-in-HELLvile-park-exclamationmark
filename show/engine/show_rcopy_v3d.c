/* show_rcopy_v3d.c - the game's 3D view renderer, compiled for the show
 * (owner: show, ENGINE lane 3; spec 4.3, 8.2, 8.3).
 *
 * ps1/src/view3d.c is compiled here a second time, unchanged (#included,
 * never copied or edited), with its exported names renamed, so the show
 * reaches the renderer's private state:
 *  - the adaptive view distance follows the previous frames' CPU time; in
 *    the show it is pinned (the shot's view_dist), so a frame never depends
 *    on how fast the frames before it were, or on a seek;
 *  - the sky is fetched before the geometry only once a frame has seen sky
 *    cells; the show tells it up front, so frame 0 of an outdoor shot has
 *    its sky too.
 * The game's plat_view3d() stays in ps1/src/view3d.c, unused by the show
 * (--gc-sections drops it).
 */
#include "doomo/doomo.h"

#define plat_view3d show_v3d_render_game
#define view3d_visible_cells show_v3d_visible_cells
#include "../../ps1/src/view3d.c"
#undef plat_view3d
#undef view3d_visible_cells

void show_v3d_render(const DoomoView3D *v, int view_dist, int sky);

void show_v3d_render(const DoomoView3D *v, int view_dist, int sky)
{
    if (view_dist < VIEW_DIST_MIN || view_dist > VIEW_DIST)
        view_dist = VIEW_DIST;
    s_view_dist = view_dist;
    s_view_tick = -1000000;      /* adapt_view_dist() never steps it */
    if (s_map != v->map || s_map_def != v->map->def) {
        s_map = v->map;          /* (what plat_view3d does on a new map) */
        s_map_def = v->map->def;
    }
    s_sky_seen = sky ? 1 : 0;
    show_v3d_render_game(v);
    s_sky_seen = sky ? 1 : 0;
}
