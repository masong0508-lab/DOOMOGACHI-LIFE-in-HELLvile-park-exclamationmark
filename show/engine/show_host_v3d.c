/* show_host_v3d.c - the VIEW3D stage kind (owner: show, ENGINE lane 3;
 * spec 8.3): first person through the game's own 3D maps with
 * plat_view3d(), on a cam3d rail (the record's camera keys: eye x, y, z,
 * yaw, pitch), with the combat art's demons as billboards, their deaths
 * gored by the game's cute/brutal filter (actor.c's actor_gore(): CUTE
 * throws confetti, hearts and stuffing, BRUTAL a blood spray and gibs;
 * an overkill leaves SPR_CORPSE_CUTE / SPR_CORPSE_BRUTAL), hit-stop
 * (the world clock holds) and the muzzle flash, kick and lens of the HUD
 * (show_hud.c).
 *
 * The filter re-skins on the frame it switches: the same gibs fly the
 * same arcs as confetti or as meat, which is the gag of a truth insert.
 * It is CUTE whenever a child is in frame (STD-KIDS).
 */
#include <string.h>
#include "show_host_int.h"
#include "ps1.h"

void sh_gore_decide(int kids_in_frame);

static DoomoBillboard s_bb[SH_MAX_BB];
static DoomoView3D s_view;
static int s_sky;

/* an eyeball and a tooth (the gore buttons; the combat art has neither),
 * drawn with the palette like every sprite: 0 clear */
#define EW 7
#define EH 7
static const uint8_t k_eye_pix[EW * EH] = {
    0, 0, 1, 1, 1, 0, 0,
    0, 1, 2, 2, 2, 1, 0,
    1, 2, 2, 2, 166, 2, 1,
    1, 2, 2, 22, 1, 166, 1,
    1, 2, 166, 22, 22, 2, 1,
    0, 1, 2, 166, 2, 1, 0,
    0, 0, 1, 1, 1, 0, 0,
};
static const DoomoImage k_eye = { EW, EH, k_eye_pix, 0x00B30001u };

int sh_v3d_prep(void)
{
    const ShowHostWorld *w = g_sh.world;
    int i, n;
    if (w->map >= MAPID_COUNT || !doomo_maps[w->map])
        return -1;
    map_load(w->map);
    n = doomo_map.w * doomo_map.h;
    if (w->flags & SWF_DOORS_OPEN) {
        int k;
        for (i = 0; i < n; i++)
            if (doomo_map.cells[i].flags & CELL_DOOR)
                map_door_bump(&doomo_map, i % doomo_map.w, i / doomo_map.w, 3);
        for (k = 0; k < 90; k++) map_tick(&doomo_map);
    }
    s_sky = 0;
    for (i = 0; i < n; i++)
        if (!doomo_map.cells[i].wall && doomo_map.cells[i].ceil_tex == DOOMO_TEX_SKY) { s_sky = 1; break; }
    opt_set(OPT_FOV, w->fov <= 2 ? w->fov : 1);
    memset(&s_view, 0, sizeof(s_view));
    sh_hud_prep();
    return 0;
}

void sh_v3d_tick(int t) { (void)t; }

/* floor height under (x, y), averaged over a 32-unit square so the eye
 * glides up stairs instead of stepping (16.16 in, world units out) */
static int floor_smooth(int32_t x, int32_t y)
{
    static const int8_t o[4][2] = { { -16, -16 }, { 16, -16 }, { -16, 16 }, { 16, 16 } };
    int i, s = 0;
    for (i = 0; i < 4; i++) {
        int cx = ((x >> 16) + o[i][0]) / DOOMO_CELL_UNITS, cy = ((y >> 16) + o[i][1]) / DOOMO_CELL_UNITS;
        const DoomoCell *c = map_cell_safe(&doomo_map, cx, cy);
        s += c->wall ? map_floor_z(&doomo_map, (x >> 22), (y >> 22)) : map_floor_z(&doomo_map, cx, cy);
    }
    return s / 4;
}

static int floor_at(int32_t x, int32_t y)
{
    int cx = x >> 22, cy = y >> 22;
    if (cx < 0 || cy < 0 || cx >= doomo_map.w || cy >= doomo_map.h) return 0;
    return map_floor_z(&doomo_map, cx, cy);
}

/* the eye at world time wt (camera keys; c is the eye height above the
 * floor when the key carries SKF_EYE in flags bit 1) */
static void eye_at(int wt, int32_t e[5])
{
    sh_key_eval(g_sh.cam, g_sh.ncam, wt, e);
    if (g_sh.ncam && (g_sh.cam[0].flags & 2))
        e[2] = INT2FIX(floor_smooth(e[0], e[1])) + e[2];
}

/* a shot event's muzzle flash and kick (world time: a hit-stop freezes the
 * flash on its brightest frame) */
int sh_v3d_flash(int t)
{
    int i, wt = sh_world_time(t), left = 0;
    for (i = 0; i < g_sh.nev && g_sh.ev[i].f <= t; i++) {
        const ShowHostEvent *e = &g_sh.ev[i];
        if (e->op == SEV_FIRE) {
            int age = wt - sh_world_time(e->f), dur = e->a > 0 ? e->a : 3;
            left = age < dur ? dur - age : 0;
        }
    }
    return left;
}

int sh_v3d_kick(int t)
{
    int i, wt = sh_world_time(t), k = 0;
    for (i = 0; i < g_sh.nev && g_sh.ev[i].f <= t; i++) {
        const ShowHostEvent *e = &g_sh.ev[i];
        if (e->op == SEV_FIRE) {
            int age = wt - sh_world_time(e->f);
            k = age < 0 ? 0 : age >= 12 ? 0 : 256 - age * 21;
        }
    }
    return k;
}

void sh_v3d_view(DoomoView3D *out) { *out = s_view; }

/* ---- billboards ----------------------------------------------------------------- */
static int add_bb(int n, int32_t x, int32_t y, int32_t z, const DoomoImage *img, int scale, int flags)
{
    DoomoBillboard *b;
    if (!img || n >= SH_MAX_BB) return n;
    b = &s_bb[n];
    b->x = x; b->y = y; b->z = z;
    b->img = img;
    b->scale = (uint16_t)scale;
    b->flags = (uint8_t)flags;
    b->light = 0;
    return n + 1;
}

/* in the view fan (the kid rule and the stats): within ~40 degrees of the
 * view axis and nearer than the view distance */
static int in_view(int32_t x, int32_t y, const DoomoView3D *v)
{
    int32_t dx = (x - v->x) >> 16, dy = (y - v->y) >> 16;
    int32_t fwd = (dx * (fx_cos(v->angle) >> 4) + dy * (fx_sin(v->angle) >> 4)) >> 12;
    int32_t lat = (dx * -(fx_sin(v->angle) >> 4) + dy * (fx_cos(v->angle) >> 4)) >> 12;
    if (fwd <= 8 || fwd > 18 * DOOMO_CELL_UNITS) return 0;
    return (lat < 0 ? -lat : lat) < fwd + 16;
}

static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

/* the gore of one kill, dt world frames after it */
static int gore_fx(int n, int ei, const ShActor *s, int32_t x, int32_t y, int z0, int dt, int gore)
{
    const ShowHostEvent *e = &g_sh.ev[ei];
    int k, ng = e->a ? 8 : 4, fz = floor_at(x, y);
    uint32_t seed = hash32(g_sh.hdr->seed ^ (uint32_t)(ei * 2654435761u));
    int cz = z0 + 24;
    (void)s;
    /* the burst: a spray (BRUTAL) or confetti and hearts (CUTE), once */
    if (gore == SG_BRUTAL) {
        int nf = sprite_frames(SPR_BLOOD_SPRAY), f = dt / 3;
        if (f < nf) n = add_bb(n, x, y, INT2FIX(cz - 12), sprite_image(SPR_BLOOD_SPRAY, f), 300, 0);
        if (e->a && dt < 12) n = add_bb(n, x, y, INT2FIX(cz - 16), sprite_image(SPR_GIBS, dt / 3), 300, 0);
    } else {
        int nf = sprite_frames(SPR_CONFETTI), f = dt / 3;
        if (f < nf) n = add_bb(n, x, y, INT2FIX(cz - 12), sprite_image(SPR_CONFETTI, f), 300, BB_FULLBRIGHT);
        nf = sprite_frames(SPR_HEARTS);
        f = dt / 4;
        if (f < nf) n = add_bb(n, x, y, INT2FIX(cz + dt), sprite_image(SPR_HEARTS, f), 300, BB_FULLBRIGHT);
    }
    /* the pieces: the same arcs in both registers */
    for (k = 0; k < ng; k++) {
        uint32_t r = hash32(seed + (uint32_t)k * 97u);
        int ang = (int)(r & 4095), sp = 1 + (int)((r >> 12) % 3u) + (e->a ? 1 : 0);
        int vz = 4 + (int)((r >> 16) % 6u);
        int tt = dt > 40 ? 40 : dt;
        int32_t px = x + (fx_cos(ang) >> 4) * sp * tt / 16 * 16;
        int32_t py = y + (fx_sin(ang) >> 4) * sp * tt / 16 * 16;
        int pz = cz + vz * tt - (tt * tt) / 4;        /* g = 0.5 unit/f^2 */
        const DoomoImage *img;
        int spr;
        if (pz < fz) pz = fz;
        if (gore == SG_BRUTAL) spr = SPR_GIBS;
        else spr = (k & 1) ? SPR_STUFFING : SPR_CONFETTI;
        img = sprite_image(spr, (int)(r >> 20) % (sprite_frames(spr) > 0 ? sprite_frames(spr) : 1));
        n = add_bb(n, px, py, INT2FIX(pz), img, 160, 0);
    }
    /* the button: an eyeball rolls to a stop (BRUTAL; CUTE shows a heart) */
    if (e->a) {
        uint32_t r = hash32(seed + 1234u);
        int ang = (int)(r & 4095), d = dt > 30 ? 30 : dt;
        int roll = d * 6 - (d * d) / 10;               /* decelerates to rest */
        int32_t px = x + (fx_cos(ang) >> 4) * roll / 16 * 16;
        int32_t py = y + (fx_sin(ang) >> 4) * roll / 16 * 16;
        if (gore == SG_BRUTAL) n = add_bb(n, px, py, INT2FIX(fz), &k_eye, 256, 0);
        else if (dt < 60) n = add_bb(n, px, py, INT2FIX(fz + 4), sprite_image(SPR_HEARTS, 0), 200, BB_FULLBRIGHT);
    }
    return n;
}

static const DoomoImage *monster_image(const ShowHostActor *a, const ShActor *s, int wt, int gore,
                                       int *scale, int *flags)
{
    int spr = a->ref, f;
    *scale = a->scale ? a->scale : 320;
    *flags = BB_SHADOW;
    if (sprite_flags(spr) & SPRF_TRANSLUCENT) *flags |= BB_TRANSLUCENT;
    if (s->dead_at >= 0) {
        int d = wt - s->dead_at;
        *flags &= ~BB_SHADOW;
        if (s->overkill) {
            /* blown apart: the corpse sprite of the register drawn now */
            *scale = 300;
            return d < 3 ? sprite_image(spr, DFRAME_DEATH0)
                         : sprite_image(gore == SG_BRUTAL ? SPR_CORPSE_BRUTAL : SPR_CORPSE_CUTE, 0);
        }
        f = DFRAME_DEATH0 + (d / 4 > 3 ? 3 : d / 4);
        return sprite_image(spr, f);
    }
    if (s->pain_until > wt) f = DFRAME_PAIN;
    else if (s->attack_until > wt) { f = DFRAME_ATTACK; *flags |= BB_FULLBRIGHT; }
    else f = ((wt + a->id * 5) / 8) & 1;
    return sprite_image(spr, f);
}

void sh_v3d_draw(int t)
{
    const ShowHostWorld *w = g_sh.world;
    DoomoView3D *v = &s_view;
    int32_t e[5], p[5];
    int i, n = 0, kids = 0, wt = g_sh.wt, gore;
    memset(v, 0, sizeof(*v));
    eye_at(wt, e);
    v->map = &doomo_map;
    v->x = e[0];
    v->y = e[1];
    v->z = e[2];
    v->angle = (int16_t)(e[3] & 4095);
    v->pitch = (int16_t)e[4];
    /* a walking bob while the eye moves on its rail */
    eye_at(wt > 0 ? wt - 1 : 0, p);
    {
        int32_t dx = (e[0] - p[0]) >> 16, dy = (e[1] - p[1]) >> 16;
        if (dx * dx + dy * dy >= 4) v->bob = (int16_t)(FIX2INT(fx_sin(wt * 205) * 2));
    }
    v->vx = w->vx; v->vy = w->vy; v->vw = w->vw; v->vh = w->vh;
    if (!v->vw || !v->vh) { v->vx = 0; v->vy = 30; v->vw = 320; v->vh = 180; }
    v->camera = CAM_FIRST_PERSON;
    v->player_sprite = -1;
    v->time = (uint16_t)t;
    /* the kid rule first: who is in the fan this frame */
    for (i = 0; i < g_sh.nact; i++) {
        ShActor s;
        if (!(g_sh.act[i].flags & SAF_KID)) continue;
        sh_actor_at(i, t, wt, &s);
        if (s.visible && in_view(s.x, s.y, v)) kids++;
    }
    sh_gore_decide(kids);
    gore = g_sh.gore;
    /* actors: demons, props, the cast */
    for (i = 0; i < g_sh.nact; i++) {
        const ShowHostActor *a = &g_sh.act[i];
        ShActor s;
        const DoomoImage *img;
        int scale = a->scale ? a->scale : 256, flags = 0, z;
        sh_actor_at(i, t, wt, &s);
        if (!s.visible) continue;
        z = floor_at(s.x, s.y) + s.z;
        switch (a->kind) {
        case SAK_MONSTER:
            img = monster_image(a, &s, wt, gore, &scale, &flags);
            if ((sprite_flags(a->ref) & SPRF_FLOATS) && s.dead_at < 0)
                z += 16 + ui_wave(wt + a->id * 13, 40, 6);
            break;
        case SAK_SPRITE: {
            int nf = sprite_frames(a->ref);
            img = sprite_image(a->ref, nf > 1 ? (wt / 8) % nf : 0);
            flags = (sprite_flags(a->ref) & SPRF_FULLBRIGHT) ? BB_FULLBRIGHT : 0;
            break;
        }
        case SAK_CHIBI:
            img = bodyspr_image(a->ref, s.dir, s.frame);
            scale = a->scale ? a->scale : 512;
            flags = BB_SHADOW;
            break;
        case SAK_FIGURE: {
            const DoomoResident *r = town_res(a->ref);
            img = r ? figure_image(&r->look, r->status, s.expr) : NULL;
            flags = BB_SHADOW;
            break;
        }
        default: img = NULL; break;
        }
        if (s.fall_at >= 0) {
            /* falls over flat: sinks into the floor over 6 frames */
            int d = wt - s.fall_at;
            z -= (d > 6 ? 6 : d) * (img ? img->h * scale / 256 : 32) / 7;
            flags &= ~BB_SHADOW;
        }
        if (a->flags & SAF_FULLBRIGHT) flags |= BB_FULLBRIGHT;
        if (a->flags & SAF_TRANSLUCENT) flags |= BB_TRANSLUCENT;
        if (a->flags & SAF_FLIPX) flags ^= BB_FLIPX;
        if (w->flags & SWF_NO_SHADOWS) flags &= ~BB_SHADOW;
        n = add_bb(n, s.x, s.y, INT2FIX(z), img, scale, flags);
    }
    /* the gore of every kill so far */
    for (i = 0; i < g_sh.nev && g_sh.ev[i].f <= t; i++) {
        const ShowHostEvent *ev = &g_sh.ev[i];
        int k;
        if (ev->op != SEV_KILL) continue;
        for (k = 0; k < g_sh.nact; k++) {
            if (g_sh.act[k].id == ev->target) {
                ShActor s;
                int kw = sh_world_time(ev->f);
                sh_actor_at(k, ev->f, kw, &s);
                n = gore_fx(n, i, &s, s.x, s.y, floor_at(s.x, s.y) + s.z, wt - kw, gore);
                break;
            }
        }
    }
    /* the lens tint (SEV_LENS: a ramp to a level over b frames) */
    {
        int lv = 0, tint = TINT_RED;
        for (i = 0; i < g_sh.nev && g_sh.ev[i].f <= t; i++) {
            const ShowHostEvent *ev = &g_sh.ev[i];
            if (ev->op != SEV_LENS) continue;
            {
                int age = t - ev->f, dur = ev->b > 0 ? ev->b : 1;
                int from = lv;
                lv = age >= dur ? ev->a : from + (ev->a - from) * age / dur;
                tint = ev->c ? (int)ev->c : TINT_RED;
            }
        }
        if (lv > 0) { v->tint = (uint8_t)tint; v->tint_level = (uint8_t)(lv > 15 ? 15 : lv); }
        else if (w->tint) { v->tint = w->tint; v->tint_level = w->tint_level; }
    }
    v->sprites = s_bb;
    v->sprite_count = n;
    g_show_host_stats.sprites = (uint16_t)n;
    show_v3d_render(v, w->view_dist ? w->view_dist : 18, s_sky);
}
