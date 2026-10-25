/* show_host_room.c - the ROOM stage kind (owner: show, ENGINE lane 3; spec
 * 8.3): the game's 2.5D house renderer (ps1/src/room.c) on a room the
 * record lays out: its size, wallpaper, floor and paints, the furniture
 * pieces of the house modules (furn_data / furn_iso, by FURN_* id) and
 * world actors (chibis, figures, sprites) standing and walking on their
 * keys in tile space. The camera is keyed (yaw, close-up, punch, pan) and
 * exactly that every frame (show_rcopy_room.c drops the spring).
 */
#include <string.h>
#include "show_host_int.h"
#include "ps1.h"
#include "doomo/furn.h"
#include "../../core/src/home_int.h"     /* home_room_template, HomeRoom (read-only) */

void sh_gore_decide(int kids_in_frame);

DOOMO_STATIC_ASSERT(sizeof(ShowHostFurn) == sizeof(DoomoRoomObj), furn_is_room_obj);

#define ROOM_MAX_OBJ 64
static DoomoRoomObj   s_obj[ROOM_MAX_OBJ];
static DoomoRoomActor s_act[SH_MAX_ACTORS];
static int s_nfixed, s_w, s_d;
static uint8_t s_wall_paint, s_floor_paint, s_wallpaper, s_floor, s_kind;

int sh_room_prep(void)
{
    const ShowHostWorld *w = g_sh.world;
    int i;
    s_nfixed = 0;
    s_w = w->room_w; s_d = w->room_d;
    s_wall_paint = w->wall_paint; s_floor_paint = w->floor_paint;
    s_wallpaper = w->wallpaper; s_floor = w->floor; s_kind = w->room_kind;
    if (w->tmpl && w->tmpl <= HOME_ROOM_COUNT) {
        /* the game's own room: its size, defaults and fixed pieces */
        static HomeRoom hr;
        const HomeRoomDef *d = &home_room_defs[w->tmpl - 1];
        home_room_template(w->tmpl - 1, &hr);
        if (!s_w) { s_w = HR_W(&hr); s_d = HR_D(&hr); }
        if (!s_wall_paint) s_wall_paint = d->wall_paint;
        if (!s_floor_paint) s_floor_paint = d->floor_paint;
        if (!s_wallpaper) s_wallpaper = d->wallpaper;
        if (!s_floor) s_floor = d->floor;
        s_kind = d->kind;
        for (i = 0; i < hr.nobj && s_nfixed < ROOM_MAX_OBJ; i++)
            home_obj_view(&hr.obj[i], &s_obj[s_nfixed++]);
    }
    if (!s_w || !s_d || s_w > 16 || s_d > 16)
        return -1;
    if (g_sh.nfurn + s_nfixed > ROOM_MAX_OBJ)
        return -1;
    for (i = 0; i < g_sh.nfurn; i++)
        if (g_sh.furn[i].furn >= FURN_MAX)
            return -1;
    return 0;
}

void sh_room_draw(int t)
{
    const ShowHostWorld *w = g_sh.world;
    DoomoRoomView v;
    int32_t c[5] = { 0, 0, 0, 0, 0 };
    int i, n = 0, kids = 0, wt = g_sh.wt;
    memset(&v, 0, sizeof(v));
    if (g_sh.ncam) sh_key_eval(g_sh.cam, g_sh.ncam, wt, c);
    memcpy(s_obj + s_nfixed, g_sh.furn, (size_t)g_sh.nfurn * sizeof(DoomoRoomObj));
    for (i = 0; i < g_sh.nact && n < SH_MAX_ACTORS; i++) {
        const ShowHostActor *a = &g_sh.act[i];
        const DoomoResident *r;
        DoomoRoomActor *ra = &s_act[n];
        ShActor s;
        sh_actor_at(i, t, wt, &s);
        if (!s.visible) continue;
        memset(ra, 0, sizeof(*ra));
        switch (a->kind) {
        case SAK_CHIBI:  ra->img = bodyspr_image(a->ref, s.dir, s.frame); break;
        case SAK_FIGURE: r = town_res(a->ref);
                         ra->img = r ? figure_image(&r->look, r->status, s.expr) : NULL; break;
        case SAK_SPRITE:
        case SAK_MONSTER: {
            int nf = sprite_frames(a->ref);
            ra->img = sprite_image(a->ref, nf > 1 ? (wt / 8) % nf : 0);
            break;
        }
        default: break;
        }
        if (!ra->img) continue;
        ra->x = s.x;
        ra->y = s.y;
        ra->z = (int16_t)s.z;
        ra->sx = ra->sy = (int16_t)(a->scale ? a->scale : 256);
        ra->flags = (uint8_t)(((a->flags & SAF_SHADOW) ? BB_SHADOW : 0) |
                              ((a->flags & SAF_TRANSLUCENT) ? BB_TRANSLUCENT : 0) |
                              ((a->flags & SAF_FLIPX) ? BB_FLIPX : 0));
        if (a->kind != SAK_CHIBI && s.dir == SDIR_LEFT) ra->flags ^= BB_FLIPX;
        if (a->flags & SAF_KID) kids++;          /* a room is all in frame */
        n++;
    }
    sh_gore_decide(kids);
    v.w = (uint8_t)s_w;
    v.d = (uint8_t)s_d;
    v.wallpaper = s_wallpaper;
    v.floor = s_floor;
    v.wall_paint = s_wall_paint;
    v.floor_paint = s_floor_paint;
    v.ceiling = w->ceiling;
    v.room_kind = s_kind;
    v.objs = s_obj;
    v.nobj = s_nfixed + g_sh.nfurn;
    v.actors = s_act;
    v.nact = n;
    v.gen = 1;
    v.yaw = (int16_t)(c[0] & 4095);
    v.zoom = (uint8_t)(c[1] ? 1 : 0);
    v.punch_q8 = (int16_t)(c[2] ? c[2] : 256);
    v.pan_x = (int8_t)c[3];
    v.pan_y = (int8_t)c[4];
    v.daylight = w->daylight > 15 ? 15 : w->daylight;
    v.tint = w->tint;
    v.tint_level = w->tint_level;
    v.weather = w->weather;
    v.cursor_x = v.cursor_y = -1;
    v.time = (uint16_t)t;
    v.vx = w->vx; v.vy = w->vy; v.vw = w->vw; v.vh = w->vh;
    if (!v.vw || !v.vh) { v.vx = 0; v.vy = 30; v.vw = 320; v.vh = 180; }
    g_show_host_stats.sprites = (uint16_t)n;
    show_room_render(&v);
}
