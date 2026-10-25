/* show_wp6_mod.c - WP6 plugged into the player (show_api.h): the modules
 * show_mod_cast, show_mod_actor and show_mod_crowd (ENGINE lane 2).
 *
 * Records (shw_format.h): SHWR_LOOK (DoomoLook[n]), SHWR_ACTOR
 * (ShowActorRec[n]), SHWR_TRACK (the TRAK blob), SHWR_CROWD (ShowCrowdRec[n]),
 * SHWR_WP6_FLIP (the FLIP blob, a private lane-2 record). Show ops
 * SOP_ACTOR_BASE (args: actor, SHOP_*, a, b, c, d) and SOP_CROWD_BASE (args:
 * crowd, SHCO_*, a); the host kinds BOP_EXPR / POP / HEADTURN / TWITCH on an
 * actor. Actors and crowds stand on the camera's main plane (parallax 1:1,
 * origin 0,0); camera scale moves do not resize them (DEFERRED.md).
 */
#include "show/show_api.h"
#include "show/show_cam.h"
#include "show/show_actor.h"

#define SHWR_WP6_FLIP (SHWR_PRIV_L2 + 0x60)

static void wp6_reset(void) { show_wp6_reset(1); }   /* TV-MA: gore on (adults/demons) */

static void cast_prep(const ShowShot *s)
{
    int n = 0;
    const void *p = show_chunk_find(SHWR_LOOK, &n, NULL);
    (void)s;
    if (p && n > 0) show_cast_load((const DoomoLook *)p, n);
}

static void actor_prep(const ShowShot *s)
{
    int n = 0;
    uint32_t bytes = 0;
    const void *p;
    (void)s;
    if ((p = show_chunk_find(SHWR_ACTOR, &n, NULL)) != NULL && n > 0)
        show_actor_load((const ShowActorRec *)p, n);
    if ((p = show_chunk_find(SHWR_TRACK, &n, &bytes)) != NULL && bytes)
        show_actor_tracks((const uint8_t *)p, bytes);
    if ((p = show_chunk_find(SHWR_WP6_FLIP, &n, &bytes)) != NULL && bytes)
        show_actor_flips((const uint8_t *)p, bytes);
}

static void crowd_prep(const ShowShot *s)
{
    int n = 0;
    const void *p = show_chunk_find(SHWR_CROWD, &n, NULL);
    (void)s;
    if (p && n > 0) show_crowd_load((const ShowCrowdRec *)p, n);
}

static int actor_ext(const BeatEv *ev)
{
    int op = show_op_of(ev);
    if (op == SHOW_SOP_ACTOR) {
        const int32_t *v = show_args(ev->b);
        show_actor_op(v[0], v[1], v[2], v[3], v[4], v[5]);
        return 1;
    }
    return op < 0 ? show_wp6_ext(ev) : 0;
}

static int crowd_ext(const BeatEv *ev)
{
    if (show_op_of(ev) == SHOW_SOP_CROWD) {
        const int32_t *v = show_args(ev->b);
        show_crowd_op(v[0], v[1], v[2]);
        return 1;
    }
    return 0;
}

/* the main plane under the camera: screen x of show px 0, screen y of row 0 */
static void origin(int *ox, int *oy)
{
    static const ShowPlaneCam main_plane = { 1, 1, 1, 1, 0, 0, 256, 0, 0 };
    ShowXform xf;
    show_cam_plane(show_cam(), &main_plane, 1, &xf);
    *ox = xf.ox;
    *oy = SHOW_Y(xf.oy);
}

static void actor_tick(int t) { show_actor_tick(t); }

/* pre-roll and seek frames (spec 8.2): the same portrait fetches in the same
 * order as a drawn frame (crowds behind, actors, crowds in front), draw = 0,
 * so the cache matches a full run. Runs after actor_tick (registry order). */
static void crowd_tick(int t)
{
    int ox, oy;
    show_crowd_tick(t);
    if (!show_drawing()) {
        origin(&ox, &oy);
        show_crowd_draw(ox, oy, SHOW_LAYER_BACK, 0);
        show_actor_draw(ox, oy, 0);
        show_crowd_draw(ox, oy, SHOW_LAYER_FRONT, 0);
    }
}

static void actor_draw(int layer, int t)
{
    int ox, oy;
    (void)t;
    if (layer != SHOW_L_ACTORS) return;
    origin(&ox, &oy);
    show_crowd_draw(ox, oy, SHOW_LAYER_BACK, 1);       /* crowds behind the actors */
    show_actor_draw(ox, oy, 1);
}

static void crowd_draw(int layer, int t)
{
    int ox, oy;
    const ShowCrowdStats *cs;
    (void)t;
    if (layer != SHOW_L_CROWDS) return;
    origin(&ox, &oy);
    show_crowd_draw(ox, oy, SHOW_LAYER_FRONT, 1);
    cs = show_crowd_stats();
    show_stat_add(SHOW_STAT_EVICTIONS, cs->rerenders);
    show_stat_add(SHOW_STAT_SPRITES, cs->drawn);
}

static void actor_hash(ShowCrc *c) { uint32_t h = show_actor_hash(2166136261u); show_crc_add(c, &h, 4); }
static void crowd_hash(ShowCrc *c) { uint32_t h = show_crowd_hash(2166136261u); show_crc_add(c, &h, 4); }

const ShowModule show_mod_cast  = { "cast",  SHOW_API_VERSION, 0, wp6_reset, cast_prep,
                                    NULL, NULL, NULL, NULL, NULL };
const ShowModule show_mod_actor = { "actor", SHOW_API_VERSION, 0, NULL, actor_prep,
                                    actor_ext, actor_tick, actor_draw, actor_hash, NULL };
const ShowModule show_mod_crowd = { "crowd", SHOW_API_VERSION, 0, NULL, crowd_prep,
                                    crowd_ext, crowd_tick, crowd_draw, crowd_hash, NULL };
