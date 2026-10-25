/* show_host.c - the stage-kind hosts: the record, keys, time, actors, NPCs
 * and the cute/brutal filter (owner: show, ENGINE lane 3; spec 8.2, 8.3,
 * 13.5 STD-KIDS). The kinds themselves are show_host_ow.c, show_host_room.c
 * and show_host_v3d.c (+ show_hud.c); show_hosts.h is the contract.
 *
 * Purity (spec 8.2): everything a frame shows is a pure function of the
 * shot frame t and the record: camera and actor keys are evaluated at t,
 * events are found by scanning the sorted list up to t, hit-stops hold
 * the world clock by arithmetic, NPC walks are seeded loops. The only
 * side effects are edge events show_host_tick() fires on their own frame
 * (camfx shakes, the game's OPT_GORE), which is why the player calls it
 * for every frame in order, pre-roll included.
 */
#include <string.h>
#include "show_host_int.h"

ShowHostCtx g_sh;
ShowHostStats g_show_host_stats;
static const char *s_err = "";
static int s_warm_n;           /* show_host_warm() calls since prep */
static int s_forced;            /* frames the kid rule forced CUTE (this shot) */

const char *show_host_error(void) { return s_err; }
int show_host_kind(void) { return g_sh.ready ? g_sh.kind : SHOST_NONE; }

/* ---- the record ----------------------------------------------------------------- */
static int fail(const char *why)
{
    s_err = why;
    memset(&g_sh, 0, sizeof(g_sh));
    return -1;
}

static int parse(const void *rec, uint32_t bytes)
{
    const uint8_t *p = (const uint8_t *)rec, *end;
    const ShowHostHdr *h = (const ShowHostHdr *)rec;
    int s;
    memset(&g_sh, 0, sizeof(g_sh));
    if (!rec || bytes < sizeof(ShowHostHdr) || ((uintptr_t)rec & 3))
        return fail("host record missing or misaligned");
    if (h->magic != SHOST_MAGIC) return fail("host record: bad magic");
    if (h->abi != SHOST_ABI) return fail("host record: ABI mismatch (rebuild with this showtk)");
    if (h->bytes > bytes || h->bytes < sizeof(ShowHostHdr)) return fail("host record: truncated");
    if (h->kind <= SHOST_NONE || h->kind >= SHOST_KIND_COUNT) return fail("host record: unknown kind");
    end = p + h->bytes;
    p += sizeof(ShowHostHdr);
    g_sh.hdr = h;
    g_sh.kind = h->kind;
    for (s = 0; s < h->nsec; s++) {
        const ShowHostSec *sec = (const ShowHostSec *)p;
        const void *d = p + sizeof(ShowHostSec);
        int n;
        if (p + sizeof(ShowHostSec) > end || (const uint8_t *)d + sec->bytes > end || (sec->bytes & 3))
            return fail("host record: a section runs past the end");
        n = sec->count;
#define TAKE(field, cnt, type) do { \
            if ((uint32_t)n * sizeof(type) > sec->bytes) return fail("host record: section too small"); \
            g_sh.field = (const type *)d; \
            g_sh.cnt = n; \
        } while (0)
        switch (sec->tag) {
        case SEC_WORLD:
            if (sec->bytes < sizeof(ShowHostWorld)) return fail("host record: SEC_WORLD too small");
            g_sh.world = (const ShowHostWorld *)d;
            break;
        case SEC_CAMKEY: TAKE(cam, ncam, ShowHostKey); break;
        case SEC_ACTOR:
            if (n > SH_MAX_ACTORS) return fail("host record: too many world actors");
            TAKE(act, nact, ShowHostActor);
            break;
        case SEC_PATH: TAKE(path, npath, ShowHostKey); break;
        case SEC_EVENT: TAKE(ev, nev, ShowHostEvent); break;
        case SEC_FURN: TAKE(furn, nfurn, ShowHostFurn); break;
        case SEC_NPC: TAKE(npc, nnpc, ShowHostNpc); break;
        case SEC_STR:
            g_sh.str = (const char *)d;
            g_sh.nstr = sec->bytes;
            break;
        case SEC_HUD:
            if (sec->bytes < sizeof(ShowHostHud)) return fail("host record: SEC_HUD too small");
            g_sh.hud = (const ShowHostHud *)d;
            break;
        default:
            break;              /* a newer section: skipped */
        }
#undef TAKE
        p = (const uint8_t *)d + sec->bytes;
    }
    if (!g_sh.world) {
        memset(&g_sh.wdef, 0, sizeof(g_sh.wdef));
        g_sh.wdef.daylight = 15;
        g_sh.wdef.vx = 0; g_sh.wdef.vy = 30; g_sh.wdef.vw = 320; g_sh.wdef.vh = 180;
        g_sh.world = &g_sh.wdef;
    }
    {
        int i;
        for (i = 0; i < g_sh.nact; i++)
            if ((uint32_t)g_sh.act[i].path0 + g_sh.act[i].npath > (uint32_t)g_sh.npath)
                return fail("host record: an actor's keys run past SEC_PATH");
    }
    return 0;
}

const char *sh_str(int off)
{
    if (!g_sh.str || off < 0 || (uint32_t)off >= g_sh.nstr) return "";
    return g_sh.str + off;
}

/* ---- keys and eases --------------------------------------------------------------- */
static int32_t ease_q16(int ease, int32_t u)        /* u 0..65536 */
{
    switch (ease) {
    case SE_HOLD:   return 0;
    case SE_IN:     return (int32_t)(((int64_t)u * u) >> 16);
    case SE_OUT:    { int32_t v = 65536 - u; return 65536 - (int32_t)(((int64_t)v * v) >> 16); }
    case SE_INOUT:  return (int32_t)((((int64_t)u * u >> 16) * (3 * 65536 - 2 * (int64_t)u)) >> 16);
    default:        return u;
    }
}

static inline int32_t lerp32(int32_t a, int32_t b, int32_t q)
{
    return a + (int32_t)(((int64_t)(b - a) * q) >> 16);
}

/* camera keys blend all five values; actor keys (lerp_de = 0) blend the
 * position and keep the facing and the mode of the key they leave */
static void key_eval(const ShowHostKey *k, int n, int t, int32_t out[5], int lerp_de)
{
    int i = 0;
    const ShowHostKey *a, *b;
    if (n <= 0) { memset(out, 0, 5 * sizeof(int32_t)); return; }
    /* the last key at or before t (keys are sorted by frame) */
    while (i + 1 < n && k[i + 1].f <= t) i++;
    a = &k[i];
    out[3] = a->d; out[4] = a->e;
    if (t < k[0].f || t <= a->f || i + 1 >= n) {
        out[0] = a->a; out[1] = a->b; out[2] = a->c;
        return;
    }
    b = &k[i + 1];
    if ((b->flags & SKF_SNAP) || b->ease == SE_HOLD || b->f <= a->f) {
        out[0] = a->a; out[1] = a->b; out[2] = a->c;
        return;
    }
    {
        int32_t u = (int32_t)(((int64_t)(t - a->f) << 16) / (b->f - a->f));
        int32_t q = ease_q16(b->ease, u);
        out[0] = lerp32(a->a, b->a, q);
        out[1] = lerp32(a->b, b->b, q);
        out[2] = lerp32(a->c, b->c, q);
        if (lerp_de) {
            out[3] = lerp32(a->d, b->d, q);
            out[4] = lerp32(a->e, b->e, q);
        }
    }
}

void sh_key_eval(const ShowHostKey *k, int n, int t, int32_t out[5])
{
    key_eval(k, n, t, out, 1);
}

/* world time: hit-stops hold it, the HUD and the camera shake run on */
int sh_world_time(int t)
{
    int i, wt = t;
    for (i = 0; i < g_sh.nev && g_sh.ev[i].f <= t; i++) {
        const ShowHostEvent *e = &g_sh.ev[i];
        if (e->op == SEV_HITSTOP && e->a > 0) {
            int held = t - e->f;
            wt -= held < e->a ? held : e->a;
        }
    }
    return wt;
}

/* ---- world actors ----------------------------------------------------------------- */
void sh_actor_at(int i, int t, int wt, ShActor *o)
{
    const ShowHostActor *a = &g_sh.act[i];
    int32_t v[5], w[5];
    int k;
    memset(o, 0, sizeof(*o));
    o->dead_at = o->pain_until = o->attack_until = o->fall_at = -1;
    o->visible = !(a->flags & SAF_HIDDEN);
    o->expr = a->expr;
    if (a->npath) {
        const ShowHostKey *pk = g_sh.path + a->path0;
        key_eval(pk, a->npath, wt, v, 0);
        key_eval(pk, a->npath, wt > 0 ? wt - 1 : 0, w, 0);
        o->x = v[0];
        o->y = v[1];
        o->z = v[2];
        o->mode = v[4];
        o->moving = (v[0] != w[0] || v[1] != w[1]) && wt > 0;
        if ((v[3] & 0xFF) != SDIR_AUTO) {
            o->dir = v[3] & 3;
        } else {
            /* face the way it walks; standing, keep the last heading (looked
             * for in the last 4 s of its keys) */
            int32_t dx = v[0] - w[0], dy = v[1] - w[1];
            int back = wt;
            while (!dx && !dy && back > 0 && wt - back < 120) {
                int32_t u[5];
                back -= 2;
                key_eval(pk, a->npath, back > 0 ? back : 0, u, 0);
                key_eval(pk, a->npath, back > 1 ? back - 1 : 0, w, 0);
                dx = u[0] - w[0];
                dy = u[1] - w[1];
            }
            if (!dx && !dy) o->dir = SDIR_DOWN;
            else if ((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy)) o->dir = dx < 0 ? SDIR_LEFT : SDIR_RIGHT;
            else o->dir = dy < 0 ? SDIR_UP : SDIR_DOWN;
        }
        if (o->moving && (o->mode == SPM_WALK || o->mode == SPM_RUN || o->mode == SPM_HOP))
            o->frame = (int)(((uint32_t)wt + (uint32_t)i * 7u) / (o->mode == SPM_RUN ? 3u : 5u)) & 3;
        if (o->moving && o->mode == SPM_HOP) {
            int ph = (wt + i * 5) % 10;
            o->z += (ph < 5 ? ph : 10 - ph) * 2;
        }
    }
    /* events up to t (world frames for the monster's own animation) */
    for (k = 0; k < g_sh.nev && g_sh.ev[k].f <= t; k++) {
        const ShowHostEvent *e = &g_sh.ev[k];
        if (e->target != a->id) continue;
        switch (e->op) {
        case SEV_SHOW: o->visible = 1; break;
        case SEV_HIDE: o->visible = 0; break;
        case SEV_EXPR: o->expr = e->a; break;
        case SEV_KILL: if (o->dead_at < 0) { o->dead_at = sh_world_time(e->f); o->overkill = e->a; } break;
        case SEV_PAIN: o->pain_until = sh_world_time(e->f) + (e->a > 0 ? e->a : 6); break;
        case SEV_ATTACK: o->attack_until = sh_world_time(e->f) + (e->a > 0 ? e->a : 10); break;
        case SEV_FALL: if (o->fall_at < 0) o->fall_at = sh_world_time(e->f); break;
        default: break;
        }
    }
}

/* ---- NPC citizens: seeded loops (pure in wt) ----------------------------------------- */
#define NPC_WP 4
typedef struct NpcPlan {
    int16_t x[NPC_WP], y[NPC_WP];   /* waypoints, world units           */
    uint16_t seg[NPC_WP];           /* frames walking to the next one   */
    uint16_t rest[NPC_WP];          /* frames standing at each          */
    uint16_t period, phase;
    uint8_t  dir;                   /* standing citizens' facing        */
} NpcPlan;
static NpcPlan s_plan[SH_MAX_NPCS];
static uint8_t s_plan_group[SH_MAX_NPCS], s_plan_k[SH_MAX_NPCS];
static int s_nplan;
static int s_group_first[8];

static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static int isqrt32(int32_t v)
{
    int32_t r = 0, b = 1 << 30;
    if (v <= 0) return 0;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1;
        b >>= 2;
    }
    return (int)r;
}

static void npc_plan(int g, int k, NpcPlan *p)
{
    const ShowHostNpc *n = &g_sh.npc[g];
    uint32_t s = hash32(n->seed * 2654435761u + (uint32_t)k * 40503u + 17u);
    int w = n->x1 - n->x0, h = n->y1 - n->y0, i, sp = n->speed_q4 ? n->speed_q4 : 12;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    memset(p, 0, sizeof(*p));
    for (i = 0; i < NPC_WP; i++) {
        s = hash32(s + 0x9e3779b9u);
        p->x[i] = (int16_t)(n->x0 + (int)(s % (uint32_t)w));
        s = hash32(s + 0x9e3779b9u);
        p->y[i] = (int16_t)(n->y0 + (int)(s % (uint32_t)h));
    }
    if (n->mode >= SNM_LANE_E) {
        /* a lane: one straight crossing of the box, repeated */
        int horiz = n->mode == SNM_LANE_E || n->mode == SNM_LANE_W;
        int len = horiz ? w : h;
        p->seg[0] = (uint16_t)((len * 16) / sp + 1);
        p->period = p->seg[0];
    } else if (n->mode == SNM_STAND) {
        p->period = 1;
        s = hash32(s + 3);
        p->dir = (uint8_t)((s >> 8) % 7 < 4 ? SDIR_DOWN : (s & 1) ? SDIR_LEFT : SDIR_RIGHT);
    } else {
        uint32_t tot = 0;
        for (i = 0; i < NPC_WP; i++) {
            int j = (i + 1) % NPC_WP;
            int dx = p->x[j] - p->x[i], dy = p->y[j] - p->y[i];
            int d = isqrt32(dx * dx + dy * dy);
            s = hash32(s + 5u + (uint32_t)i);
            p->seg[i] = (uint16_t)((d * 16) / sp + 1);
            p->rest[i] = (uint16_t)(20 + (s % 70u));
            tot += p->seg[i] + p->rest[i];
        }
        p->period = (uint16_t)(tot > 65535u ? 65535u : tot);
    }
    s = hash32(s + 99u);
    p->phase = (uint16_t)(s % (p->period ? p->period : 1));
}

int sh_npc_looks(int g)
{
    const ShowHostNpc *n = &g_sh.npc[g];
    return n->looks && n->looks < n->n ? n->looks : n->n;
}

int sh_npc_count(int g)
{
    if (g < 0 || g >= g_sh.nnpc) return 0;
    return g_sh.npc[g].n;
}

void sh_npc_at(int g, int k, int wt, int32_t *x, int32_t *y, int *dir, int *frame, int *moving)
{
    const ShowHostNpc *n = &g_sh.npc[g];
    int idx = s_group_first[g & 7] + k, ph, i;
    const NpcPlan *p;
    *dir = SDIR_DOWN; *frame = 0; *moving = 0;
    if (g >= 8 || idx < 0 || idx >= s_nplan) { *x = *y = 0; return; }
    p = &s_plan[idx];
    ph = (int)(((uint32_t)wt + p->phase) % (p->period ? p->period : 1u));
    if (n->mode == SNM_STAND) {
        *x = INT2FIX(p->x[0]);
        *y = INT2FIX(p->y[0]);
        *dir = p->dir;
        return;
    }
    if (n->mode >= SNM_LANE_E) {
        int horiz = n->mode == SNM_LANE_E || n->mode == SNM_LANE_W;
        int len = horiz ? n->x1 - n->x0 : n->y1 - n->y0;
        int32_t pos = (int32_t)(((int64_t)ph * len << 16) / (p->seg[0] ? p->seg[0] : 1));
        *moving = 1;
        *frame = ((wt + k * 3) / 5) & 3;
        if (n->mode == SNM_LANE_E) { *x = INT2FIX(n->x0) + pos; *y = INT2FIX(p->y[0]); *dir = SDIR_RIGHT; }
        else if (n->mode == SNM_LANE_W) { *x = INT2FIX(n->x1) - pos; *y = INT2FIX(p->y[0]); *dir = SDIR_LEFT; }
        else if (n->mode == SNM_LANE_S) { *y = INT2FIX(n->y0) + pos; *x = INT2FIX(p->x[0]); *dir = SDIR_DOWN; }
        else { *y = INT2FIX(n->y1) - pos; *x = INT2FIX(p->x[0]); *dir = SDIR_UP; }
        return;
    }
    for (i = 0; i < NPC_WP; i++) {
        int j = (i + 1) % NPC_WP;
        if (ph < p->rest[i]) {
            *x = INT2FIX(p->x[i]);
            *y = INT2FIX(p->y[i]);
            *dir = SDIR_DOWN;
            return;
        }
        ph -= p->rest[i];
        if (ph < p->seg[i]) {
            int dx = p->x[j] - p->x[i], dy = p->y[j] - p->y[i];
            int32_t u = (int32_t)(((int64_t)ph << 16) / (p->seg[i] ? p->seg[i] : 1));
            *x = INT2FIX(p->x[i]) + (int32_t)(((int64_t)dx * u));
            *y = INT2FIX(p->y[i]) + (int32_t)(((int64_t)dy * u));
            *moving = 1;
            *frame = ((wt + k * 3) / 5) & 3;
            if ((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy)) *dir = dx < 0 ? SDIR_LEFT : SDIR_RIGHT;
            else *dir = dy < 0 ? SDIR_UP : SDIR_DOWN;
            return;
        }
        ph -= p->seg[i];
    }
    *x = INT2FIX(p->x[0]);
    *y = INT2FIX(p->y[0]);
}

static void npc_prep(void)
{
    int g, k;
    s_nplan = 0;
    g_sh.npc_total = 0;
    for (g = 0; g < g_sh.nnpc && g < 8; g++) {
        const ShowHostNpc *n = &g_sh.npc[g];
        s_group_first[g] = s_nplan;
        for (k = 0; k < n->n && s_nplan < SH_MAX_NPCS; k++) {
            int rid = n->rid + k;
            if (k < sh_npc_looks(g) && rid >= 0 && rid < DOOMO_MAX_RESIDENTS) {
                DoomoResident *r = &doomo_town.res[rid];
                resident_default(r);
                resident_randomize_look(r, n->seed + (uint32_t)k * 7919u);
                if (n->species) r->look.species = (uint8_t)(n->species - 1);
                r->status = RES_ALIVE;
            }
            npc_plan(g, k, &s_plan[s_nplan]);
            s_plan_group[s_nplan] = (uint8_t)g;
            s_plan_k[s_nplan] = (uint8_t)k;
            s_nplan++;
        }
    }
    g_sh.npc_total = s_nplan;
    (void)s_plan_group; (void)s_plan_k;
}

/* ---- the filter --------------------------------------------------------------------- */
void show_gore_kid_in_frame(void) { g_sh.kid_report = 1; }
int  show_gore_effective(void) { return g_sh.ready ? g_sh.gore : SG_CUTE; }
int  show_gore_forced_frames(void) { return s_forced; }

void sh_set_game_gore(int g)
{
    opt_set(OPT_GORE, g == SG_BRUTAL ? GORE_BRUTAL : GORE_CUTE);
}

static int gore_script_at(int t)
{
    int i, g = g_sh.hdr->gore == SG_BRUTAL ? SG_BRUTAL : SG_CUTE;
    for (i = 0; i < g_sh.nev && g_sh.ev[i].f <= t; i++)
        if (g_sh.ev[i].op == SEV_GORE) g = g_sh.ev[i].a == SG_BRUTAL ? SG_BRUTAL : SG_CUTE;
    return g;
}

/* ---- the plug-in calls ------------------------------------------------------------- */
int show_host_prep(const void *rec, uint32_t bytes)
{
    int r;
    s_forced = 0;
    if (parse(rec, bytes) < 0)
        return -1;
    /* A fresh VRAM image cache for every host shot (purity, and a guard):
     * vram.c's 512-entry uid hash only rebuilds away entries whose page was
     * evicted, so more than 384 distinct live images between two page
     * evictions (small chibis pack ~600 to VRAM, and every portrait-pool
     * re-render is a new uid) make vram_image() recurse for ever: measured,
     * a 20-citizen overworld shot hung at frame 114. vram_init() drops the
     * pages, the world-texture slots and the hash and reloads the CLUTs and
     * the font (a few KB, at prep only). */
    vram_init();
    npc_prep();
    switch (g_sh.kind) {
    case SHOST_OVERWORLD: r = sh_ow_prep(); break;
    case SHOST_ROOM:      r = sh_room_prep(); break;
    case SHOST_VIEW3D:    r = sh_v3d_prep(); break;
    default:              r = -1; break;
    }
    if (r < 0)
        return fail(s_err[0] ? s_err : "host prep failed");
    g_sh.ready = 1;
    s_warm_n = 0;
    g_sh.t = g_sh.wt = 0;
    g_sh.gore_script = gore_script_at(0);
    g_sh.gore = (g_sh.hdr->flags & SHF_KIDS) ? SG_CUTE : g_sh.gore_script;
    sh_set_game_gore(g_sh.gore);
    s_err = "";
    return 0;
}

void show_host_tick(int t)
{
    int i;
    if (!g_sh.ready) return;
    g_sh.t = t;
    g_sh.wt = sh_world_time(t);
    g_sh.gore_script = gore_script_at(t);
    for (i = 0; i < g_sh.nev; i++) {
        const ShowHostEvent *e = &g_sh.ev[i];
        if (e->f > t) break;
        if (e->f != t) continue;
        if (e->op == SEV_SHAKE) camfx_shake(e->a);
    }
    if (g_sh.kind == SHOST_VIEW3D) sh_v3d_tick(t);
}

/* the kid rule: decided per frame, before anything gory is drawn */
void sh_gore_decide(int kids_in_frame);
void sh_gore_decide(int kids_in_frame)
{
    int g = g_sh.gore_script;
    g_sh.kids = kids_in_frame;
    if ((g_sh.hdr->flags & SHF_KIDS) || kids_in_frame || g_sh.kid_report) {
        if (g == SG_BRUTAL) s_forced++;
        g = SG_CUTE;
    }
    if (g != g_sh.gore) sh_set_game_gore(g);
    g_sh.gore = g;
    g_sh.kid_report = 0;
    g_show_host_stats.kids_seen = (uint16_t)kids_in_frame;
    g_show_host_stats.gore = (uint16_t)g;
}

void show_host_draw(int t)
{
    uint32_t t0;
    if (!g_sh.ready) return;
    if (t != g_sh.t) show_host_tick(t);     /* a caller that skipped the tick */
    t0 = timer_us();
    switch (g_sh.kind) {
    case SHOST_OVERWORLD: sh_ow_draw(t); break;
    case SHOST_ROOM:      sh_room_draw(t); break;
    case SHOST_VIEW3D:    sh_v3d_draw(t); break;
    default: break;
    }
    g_show_host_stats.draw_us = timer_us() - t0;
}

/* Warm-up (spec 8.2 pre-roll): a host plane's first frame uploads its world
 * textures and images into the display list, which they share with the
 * drawing (56 KB): drawn cold, frame 0 of a map comes out cut off. The
 * player calls show_host_warm() on uncaptured frames after prep until it
 * returns 0 (at most SHOST_WARM_MAX): each draws frame 0 normally; it
 * returns 0 once a frame made no new VRAM upload. */
/* the chibi frames every actor and citizen look can show (4 dirs x 4 walk
 * frames, 1 pool block each), rendered a few per warm frame */
static int s_warm_img;
static int warm_images(int budget)
{
    int i, g, k, idx = 0;
    for (i = 0; i < g_sh.nact; i++) {
        if (g_sh.act[i].kind != SAK_CHIBI) continue;
        for (k = 0; k < 16; k++, idx++)
            if (idx >= s_warm_img && budget > 0) { bodyspr_image(g_sh.act[i].ref, k >> 2, k & 3); budget--; s_warm_img++; }
    }
    for (g = 0; g < g_sh.nnpc; g++)
        for (i = 0; i < sh_npc_looks(g); i++)
            for (k = 0; k < 16; k++, idx++)
                if (idx >= s_warm_img && budget > 0) { bodyspr_image(g_sh.npc[g].rid + i, k >> 2, k & 3); budget--; s_warm_img++; }
    return s_warm_img < idx;
}

int show_host_warm(void)
{
    uint32_t before;
    int more;
    if (!g_sh.ready || s_warm_n >= SHOST_WARM_MAX) return 0;
    if (s_warm_n == 0) {
        s_warm_img = 0;
        if (g_sh.kind == SHOST_VIEW3D) sh_hud_warm();
    }
    s_warm_n++;
    more = warm_images(24);
    before = vram_uploads();
    show_host_draw(0);
    show_host_draw_hud(0);
    return more || vram_uploads() != before;    /* still work to do: go again */
}

void show_host_draw_hud(int t)
{
    uint32_t t0;
    if (!g_sh.ready || g_sh.kind != SHOST_VIEW3D) return;
    t0 = timer_us();
    sh_hud_draw(t);
    g_show_host_stats.hud_us = timer_us() - t0;
}

void show_host_end(void)
{
    if (g_sh.ready) sh_set_game_gore(SG_CUTE);
    memset(&g_sh, 0, sizeof(g_sh));
}
