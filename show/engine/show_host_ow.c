/* show_host_ow.c - the OVERWORLD stage kind (owner: show, ENGINE lane 3;
 * spec 8.3): plat_overworld() on the game's own overworld maps (hahaland,
 * hahaland_invaded, hellvile, park_united) at a keyed show camera, with
 * world actors (cast chibis, figures, props, demons) standing and walking
 * on their keys, and seeded NPC citizens wandering or crossing in lanes,
 * every one a billboard the game's renderer draws on its feet.
 *
 * The camera focus is exactly the key at t (show_rcopy_ow.c drops the
 * game's glide). Looks: cast actors use the doomo_town.res[] slots the
 * player fills from the LOOK chunk; NPCs get theirs at prep from the
 * game's resident_randomize_look(), in the record's slots.
 */
#include <string.h>
#include "show_host_int.h"
#include "ps1.h"

void sh_gore_decide(int kids_in_frame);

static DoomoBillboard s_bb[SH_MAX_BB];

int sh_ow_prep(void)
{
    const ShowHostWorld *w = g_sh.world;
    int g, k;
    if (w->map >= MAPID_COUNT || !doomo_maps[w->map])
        return -1;
    map_load(w->map);
    /* the NPCs' looks changed: their cached images are stale */
    for (g = 0; g < g_sh.nnpc; g++)
        for (k = 0; k < sh_npc_looks(g); k++)
            portrait_invalidate(g_sh.npc[g].rid + k);
    return 0;
}

static int floor_at(int32_t x, int32_t y)
{
    int cx = x >> 22, cy = y >> 22;           /* 16.16 world -> cell (64 units) */
    if (cx < 0 || cy < 0 || cx >= doomo_map.w || cy >= doomo_map.h) return 0;
    return map_floor_z(&doomo_map, cx, cy);
}

/* the drawn footprint around the focus cell (overworld.c: 9 rows north,
 * 6 south, about 7-11 cells either side); a little wider for the kid rule */
static int in_frame(int32_t x, int32_t y, int ccx, int ccy)
{
    int bx = x >> 22, by = y >> 22;
    return by >= ccy - 10 && by <= ccy + 7 && bx >= ccx - 12 && bx <= ccx + 12;
}

static const DoomoImage *actor_image(const ShowHostActor *a, const ShActor *s, int wt)
{
    const DoomoResident *r;
    switch (a->kind) {
    case SAK_CHIBI:
        return bodyspr_image(a->ref, s->dir, s->frame);
    case SAK_FIGURE:
        r = town_res(a->ref);
        return r ? figure_image(&r->look, r->status, s->expr) : NULL;
    case SAK_SPRITE: {
        int n = sprite_frames(a->ref);
        return sprite_image(a->ref, n > 1 ? (wt / 8) % n : 0);
    }
    case SAK_MONSTER: {
        int f = (wt / 8) & 1;
        if (s->dead_at >= 0) {
            int d = (wt - s->dead_at) / 4;
            f = DFRAME_DEATH0 + (d > 3 ? 3 : d);
        } else if (s->pain_until > wt) f = DFRAME_PAIN;
        else if (s->attack_until > wt) f = DFRAME_ATTACK;
        return sprite_image(a->ref, f);
    }
    default:
        return NULL;
    }
}

void sh_ow_draw(int t)
{
    const ShowHostWorld *w = g_sh.world;
    DoomoOverworldView v;
    int32_t c[5];
    int i, n = 0, g, k, kids = 0, ccx, ccy, wt = g_sh.wt;
    memset(&v, 0, sizeof(v));
    if (g_sh.ncam) {
        sh_key_eval(g_sh.cam, g_sh.ncam, wt, c);
    } else {
        c[0] = INT2FIX(doomo_map.w * DOOMO_CELL_UNITS / 2);
        c[1] = INT2FIX(doomo_map.h * DOOMO_CELL_UNITS / 2);
    }
    ccx = c[0] >> 22;
    ccy = c[1] >> 22;
    /* world actors */
    for (i = 0; i < g_sh.nact && n < SH_MAX_BB; i++) {
        const ShowHostActor *a = &g_sh.act[i];
        ShActor s;
        DoomoBillboard *b = &s_bb[n];
        sh_actor_at(i, t, wt, &s);
        if (!s.visible) continue;
        b->img = actor_image(a, &s, wt);
        if (!b->img) continue;
        b->x = s.x;
        b->y = s.y;
        b->z = INT2FIX(floor_at(s.x, s.y) + s.z);
        b->scale = a->scale ? a->scale : (a->kind == SAK_FIGURE ? 128 : 256);
        b->flags = (uint8_t)(((a->flags & SAF_SHADOW) && !(w->flags & SWF_NO_SHADOWS) ? BB_SHADOW : 0) |
                             ((a->flags & SAF_FULLBRIGHT) ? BB_FULLBRIGHT : 0) |
                             ((a->flags & SAF_TRANSLUCENT) ? BB_TRANSLUCENT : 0) |
                             ((a->flags & SAF_FLIPX) ? BB_FLIPX : 0));
        if (a->kind != SAK_CHIBI && s.dir == SDIR_LEFT) b->flags ^= BB_FLIPX;
        b->light = 0;
        if ((a->flags & SAF_KID) && in_frame(s.x, s.y, ccx, ccy)) kids++;
        n++;
    }
    /* NPC citizens */
    for (g = 0; g < g_sh.nnpc; g++) {
        const ShowHostNpc *np = &g_sh.npc[g];
        for (k = 0; k < np->n && n < SH_MAX_BB; k++) {
            int32_t x, y;
            int dir, frame, moving;
            DoomoBillboard *b = &s_bb[n];
                    sh_npc_at(g, k, wt, &x, &y, &dir, &frame, &moving);
                    if (!in_frame(x, y, ccx, ccy)) continue;
            b->img = bodyspr_image(np->rid + k % sh_npc_looks(g), dir, frame);
            if (!b->img) continue;
            b->x = x;
            b->y = y;
            b->z = INT2FIX(floor_at(x, y) + (!moving && (((wt + k * 11) / 24) & 1) ? 2 : 0));
            b->scale = 256;
            b->flags = (uint8_t)((w->flags & SWF_NO_SHADOWS) ? 0 : BB_SHADOW);
            b->light = 0;
            if (np->flags & SAF_KID) kids++;
            n++;
        }
    }
    sh_gore_decide(kids);
    v.map = &doomo_map;
    v.cx = c[0];
    v.cy = c[1];
    v.vx = w->vx; v.vy = w->vy; v.vw = w->vw; v.vh = w->vh;
    if (!v.vw || !v.vh) { v.vx = 0; v.vy = 30; v.vw = 320; v.vh = 180; }
    v.sprites = s_bb;
    v.sprite_count = n;
    v.daylight = w->daylight > 15 ? 15 : w->daylight;
    v.tint = w->tint;
    v.tint_level = w->tint_level;
    v.time = (uint16_t)t;
    g_show_host_stats.sprites = (uint16_t)n;
    show_ow_render(&v);
}
