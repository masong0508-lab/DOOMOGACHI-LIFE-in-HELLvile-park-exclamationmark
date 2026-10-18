/* home_defend.c - Hold the Door, the house defence (owner: H1; HF sec.10.3
 * retimed by SPEC R11: wave 1 is Ep 3.2 under the red night sky, the siege
 * is Ep 3.7).
 *
 * The house is the map: nothing here leaves the `home` overlay group, and
 * the scene (home_draw.c, H4) draws the state below through plat_room().
 * Pure logic on the 30 Hz tick, integer only, deterministic: the waves, the
 * door's HP (= the room's Barricade), welded plates on the window slots,
 * the toaster turret (fires every 12 f at the nearest demon in its window's
 * arc, re-wound every 20 shots), the guests' "safe" count, the Mayor's
 * decoy (it works once), the rule of three of false alarms, the chimney
 * beat (everything freezes, then wave 3 runs at 2x clock) and dawn.
 *
 * The demons outside are on lanes: the door, and one per window slot. An
 * imp walks its lane, hits the door (1 HP every 16 f; two fit in the frame),
 * and runs off once its nerve is spent (imps have the attention span of
 * imps). A brute presses its face against a window's plate ([SQUASH 1.3/0.8]
 * every 24 f); a plate takes 10 hits, then the glass breaks and the brute
 * is stuck in the frame. A broken door lets imps in: a guest in the room
 * with a demon loses "safe" unless a comfort tick (A on a guest) absorbs
 * the scare. Guests near a breached window flee into the Bedroom (they stay
 * safe); the decoy at the window does not get away with it.
 *
 * The result (guests kept safe) is cached in HOME (house_api.c serves it to
 * the story, SPEC C6, C9). Every frame count lives in the table below.
 */
#include "home_int.h"
#include "doomo/items.h"
#include "doomo/story_vm.h"

/* ---- the timing table (frames at 30 Hz; HF sec.10.3) ----------------------------- */
enum { HDF_WALK_PX, HDF_QUEUE_PX, HDF_IMP_HIT, HDF_BRUTE_HIT, HDF_PLATE_HITS, HDF_DOOR_SLOTS,
       HDF_IMP_HP, HDF_BRUTE_HP, HDF_IMP_NERVE, HDF_BRUTE_NERVE, HDF_TURRET_CD, HDF_AMMO,
       HDF_RANGE_PX, HDF_REPAIR_HP, HDF_REPAIR_BUSY, HDF_REWIND_BUSY, HDF_COMFORT_BUSY,
       HDF_PLATE_BUSY, HDF_CALM_MAX, HDF_HOP, HDF_INSIDE_HOPS, HDF_POP, HDF_STARE,
       HDF_ALARM_GAP, HDF_ALARM_AT, HDF_CHIMNEY_AT, HDF_CHIMNEY_LEN, HDF_DAWN, HDF_FLEE_PX,
       HDF_RIMSHOT_EVERY, HDF_COUNT };
static const uint8_t hdf[HDF_COUNT] = {
    48,     /* HDF_WALK_PX       a lane's length outside the wall                 */
    7,      /* HDF_QUEUE_PX      where a third imp waits behind the two at the door */
    16,     /* HDF_IMP_HIT       one door hit every 16 f                          */
    24,     /* HDF_BRUTE_HIT     the brute's squash against the plate             */
    10,     /* HDF_PLATE_HITS    a plate takes 10 hits                            */
    2,      /* HDF_DOOR_SLOTS    imps that fit in the door frame                  */
    4,      /* HDF_IMP_HP                                                         */
    20,     /* HDF_BRUTE_HP                                                       */
    8,      /* HDF_IMP_NERVE     hits before an imp loses interest                */
    14,     /* HDF_BRUTE_NERVE                                                    */
    12,     /* HDF_TURRET_CD     the toaster fires every 12 f                     */
    20,     /* HDF_AMMO          then it must be re-wound                         */
    40,     /* HDF_RANGE_PX      how far down a lane the turret reaches           */
    10,     /* HDF_REPAIR_HP     A on the door: +10                               */
    12,     /* HDF_REPAIR_BUSY                                                    */
    8,      /* HDF_REWIND_BUSY   A on the turret: 8 f                             */
    10,     /* HDF_COMFORT_BUSY  A on a guest: 2 hearts                           */
    12,     /* HDF_PLATE_BUSY    a fallen plate DROPs back on                     */
    3,      /* HDF_CALM_MAX      comfort ticks a guest can bank                   */
    16,     /* HDF_HOP           an imp indoors hops every 16 f                   */
    3,      /* HDF_INSIDE_HOPS   ... three times, then leaves                     */
    8,      /* HDF_POP           a toasted imp's pop                              */
    48,     /* HDF_STARE         the brute staring back at the decoy              */
    15,     /* HDF_ALARM_GAP     the false alarms: 15 f apart                     */
    200,    /* HDF_ALARM_AT      ... at the latest this far into the wave (x4)    */
    240,    /* HDF_CHIMNEY_AT    the fog comes down the chimney (wave 3)          */
    120,    /* HDF_CHIMNEY_LEN   FREEZE ALL ... THAW PANIC                        */
    90,     /* HDF_DAWN                                                           */
    2,      /* HDF_FLEE_PX       px per frame running away                        */
    10,     /* HDF_RIMSHOT_EVERY every 10th door hit is a rimshot                 */
};

/* ---- the wave tables (HOME DEFEND b, HF sec.10.3) ---------------------------------- */
enum { HWF_CHIMNEY = 1, HWF_DECOY = 2, HWF_ALARMS = 4 };
typedef struct HdWave { uint8_t imps, brutes, gap, flags; uint16_t timeout, quiet; } HdWave;
typedef struct HdTable { uint8_t nwaves, red_sky, guests; HdWave w[HOME_DEFEND_WAVES]; } HdTable;
static const HdTable hd_tables[HDT_COUNT] = {
    /* HDT_AUTO: resolved from the story before use */
    { 0, 0, 0, { { 0, 0, 0, 0, 0, 0 } } },
    /* HDT_TUTORIAL: Ep 3.2, three imps at the door under the red sky */
    { 1, 1, 0, { { 3, 0, 60, 0, 1800, 0 } } },
    /* HDT_SIEGE: Ep 3.7. +90 s, a 30 s authored quiet, +90 s; wave 3 is the longest */
    { 3, 1, 1, { { 6, 0, 40, 0, 1800, 900 }, { 10, 1, 32, 0, 1800, 900 },
                 { 14, 2, 26, HWF_CHIMNEY | HWF_DECOY | HWF_ALARMS, 2700, 0 } } },
    /* HDT_RAID: a Ch3 Night Raid, one wave */
    { 1, 1, 0, { { 5, 0, 40, 0, 1500, 0 } } },
    /* HDT_ARENA: the post-game "Hold the Door" (no chimney: the pet is home) */
    { 4, 0, 1, { { 6, 0, 36, 0, 1800, 450 }, { 10, 1, 30, 0, 1800, 450 },
                 { 14, 2, 24, HWF_DECOY | HWF_ALARMS, 2100, 450 },
                 { 16, 3, 20, HWF_ALARMS, 2400, 0 } } },
};

/* The guests of the siege (SF Ep 3.7): Pip and Mr. Flops, Miss Lulla and two
 * kids, Nana, Kevin, and the Mayor at the window "as a decoy". Kevin has no
 * roster row (SPEC C23): his resident id is -1 and the scene draws him. */
const int16_t home_defend_guest_id[HDG_COUNT] = { 42, 36, 1, -1, 0 };
/* where they like to stand in the Living Room (the nearest free tile wins) */
static const uint8_t guest_spot[HDG_COUNT][2] = { { 3, 3 }, { 4, 4 }, { 2, 5 }, { 5, 2 }, { 0, 0 } };

int home_defend_nwaves(int table) { return table > 0 && table < HDT_COUNT ? hd_tables[table].nwaves : 0; }
int home_defend_wave_imps(int table, int w)
{
    return home_defend_nwaves(table) > w && w >= 0 ? hd_tables[table].w[w].imps : 0;
}
int home_defend_wave_brutes(int table, int w)
{
    return home_defend_nwaves(table) > w && w >= 0 ? hd_tables[table].w[w].brutes : 0;
}
int home_defend_red_sky(int table) { return table > 0 && table < HDT_COUNT && hd_tables[table].red_sky; }

int home_defend_table_for_story(void)
{
    if (doomo_town.mode != TOWN_STORY) return HDT_ARENA;
    /* Ep 3.2 has the first wave; once it is done the next push is the siege */
    return story_episode_done(BEAT_EP3_2) ? HDT_SIEGE : HDT_TUTORIAL;
}

/* ---- geometry ------------------------------------------------------------------------- */
static int lane_wall(const HomeDefend *d, int lane) { return d->lane_wall[lane]; }

/* the floor tile in front of a slot on wall A (y = 0) or B (x = 0) */
static void slot_tile(int wall, int at, uint8_t *x, uint8_t *y)
{
    if (wall == HW_A) { *x = (uint8_t)at; *y = 0; }
    else { *x = 0; *y = (uint8_t)at; }
}

static int tile_dist(int x0, int y0, int x1, int y1)
{
    return (x0 > x1 ? x0 - x1 : x1 - x0) + (y0 > y1 ? y0 - y1 : y1 - y0);
}

/* occupancy of the room (floor pieces; rugs and smalls are walkable) */
static void occupancy(const HomeRoom *r, uint16_t occ[16])
{
    int i, x, y;
    memset(occ, 0, 16 * sizeof(uint16_t));
    for (i = 0; i < r->nobj; i++) {
        const HomeObj *o = &r->obj[i];
        const DoomoFurn *f = home_furn(HO_FURN(o));
        int w, dd;
        if (!f || (f->flags & (FF_WALL | FF_RUG | FF_SMALL)) || HO_ONSURF(o)) continue;
        home_footprint(f, HO_ROT(o), &w, &dd);
        for (y = HO_Y(o); y < HO_Y(o) + dd && y < 16; y++)
            for (x = HO_X(o); x < HO_X(o) + w && x < 16; x++) occ[y] |= (uint16_t)(1u << x);
    }
}

static int tile_free(const HomeDefend *d, const uint16_t occ[16], int x, int y)
{
    int i;
    if (x < 0 || y < 0 || x >= d->w || y >= d->d) return 0;
    if ((occ[y] >> x) & 1) return 0;
    if (x == d->lane_x[HDL_DOOR] && y == d->lane_y[HDL_DOOR]) return 0;
    for (i = 0; i < HDG_COUNT; i++)
        if (d->g[i].present && d->g[i].x == x && d->g[i].y == y) return 0;
    return 1;
}

/* the free tile nearest (x, y), scanning rings outward; 0 if the room is full */
static int nearest_free(const HomeDefend *d, const uint16_t occ[16], int x, int y, uint8_t *ox, uint8_t *oy)
{
    int r, i, j;
    for (r = 0; r < 12; r++)
        for (j = -r; j <= r; j++)
            for (i = -r; i <= r; i++) {
                if ((i < 0 ? -i : i) + (j < 0 ? -j : j) != r) continue;
                if (tile_free(d, occ, x + i, y + j)) { *ox = (uint8_t)(x + i); *oy = (uint8_t)(y + j); return 1; }
            }
    return 0;
}

/* ---- setup ------------------------------------------------------------------------------- */
void home_defend_begin(HomeDefend *d, int table, const HomeRoom *r, int room)
{
    const HomeRoomDef *def = &home_room_defs[(unsigned)room < HOME_ROOM_COUNT ? room : 0];
    const HdTable *tb;
    HomeScore sc;
    uint16_t occ[16];
    int i, k;
    memset(d, 0, sizeof(*d));
    if (table <= HDT_AUTO || table >= HDT_COUNT) table = home_defend_table_for_story();
    tb = &hd_tables[table];
    d->table = (uint8_t)table;
    d->nwaves = tb->nwaves;
    d->room = (uint8_t)room;
    d->w = (uint8_t)HR_W(r);
    d->d = (uint8_t)HR_D(r);
    d->turret = -1;
    d->mayor_win = -1;
    rng_seed(&d->rng, story_rng(HOME_CTX_DEFEND ^ (uint32_t)table));
    /* the lanes: the door, then the window slots */
    d->lane_wall[HDL_DOOR] = def->door_wall;
    slot_tile(def->door_wall, def->door_at, &d->lane_x[HDL_DOOR], &d->lane_y[HDL_DOOR]);
    d->lanes = 1;
    d->nwin = (uint8_t)(def->nwin > 2 ? 2 : def->nwin);
    for (k = 0; k < d->nwin; k++) {
        d->lane_wall[HDL_WIN0 + k] = def->win_wall[k];
        slot_tile(def->win_wall[k], def->win_at[k], &d->lane_x[HDL_WIN0 + k], &d->lane_y[HDL_WIN0 + k]);
        d->lanes |= (uint8_t)(1 << (HDL_WIN0 + k));
    }
    /* door HP = Barricade (HF sec.4.1, scored in Ch3 terms) */
    home_score_room(r, room, HSC_CH3, &sc);
    d->barricade = sc.stat[HS_BARRICADE];
    d->door_hp = d->door_max = d->barricade;
    /* plates on the window slots, and the turret's window */
    for (i = 0; i < r->nobj; i++) {
        const HomeObj *o = &r->obj[i];
        const DoomoFurn *f = home_furn(HO_FURN(o));
        if (!f) continue;
        for (k = 0; k < d->nwin; k++) {
            int wb = def->win_wall[k] == HW_B;
            uint8_t sx = d->lane_x[HDL_WIN0 + k], sy = d->lane_y[HDL_WIN0 + k];
            if (f->role == FR_PLATE && (HO_ROT(o) & 1) == wb && HO_X(o) == sx && HO_Y(o) == sy) {
                d->plate_hp[k] = hdf[HDF_PLATE_HITS];
                d->plated |= (uint8_t)(1 << k);
            }
            if (f->role == FR_TURRET && HO_X(o) == sx && HO_Y(o) == sy && d->turret < 0) {
                d->turret = (int8_t)k;
                d->turret_x = sx;
                d->turret_y = sy;
            }
        }
        /* a turret placed anywhere else still guards the nearest window */
        if (f->role == FR_TURRET && d->turret < 0 && d->nwin) {
            int best = 0, bd = 99;
            for (k = 0; k < d->nwin; k++) {
                int dd = tile_dist(HO_X(o), HO_Y(o), d->lane_x[HDL_WIN0 + k], d->lane_y[HDL_WIN0 + k]);
                if (dd < bd) { bd = dd; best = k; }
            }
            d->turret = (int8_t)best;
            d->turret_x = (uint8_t)HO_X(o);
            d->turret_y = (uint8_t)HO_Y(o);
        }
    }
    d->ammo = d->turret >= 0 ? hdf[HDF_AMMO] : 0;
    /* the guests (the siege and the arena) */
    if (tb->guests) {
        occupancy(r, occ);
        if (d->nwin) d->mayor_win = (int8_t)((d->turret == 0 && d->nwin > 1) ? 1 : 0);
        for (i = 0; i < HDG_COUNT; i++) {
            HomeGuest *g = &d->g[i];
            int sx = guest_spot[i][0], sy = guest_spot[i][1];
            if (i == HDG_MAYOR && d->mayor_win >= 0) {
                sx = d->lane_x[HDL_WIN0 + d->mayor_win];
                sy = d->lane_y[HDL_WIN0 + d->mayor_win];
            }
            if (sx >= d->w) sx = d->w - 1;
            if (sy >= d->d) sy = d->d - 1;
            if (!nearest_free(d, occ, sx, sy, &g->x, &g->y)) continue;
            g->present = 1;
            g->safe = 1;
        }
    }
    d->phase = HDP_READY;
}

/* ---- the guests ----------------------------------------------------------------------- */
static void scare(HomeDefend *d, HomeGuest *g)
{
    if (!g->present || g->fled || !g->safe) return;
    if (g->calm) { g->calm--; g->flinch = 1; d->ev |= HDE_SCARE; return; }
    g->safe = 0;
    g->fled = 1;
    d->ev |= HDE_FLEE;
}

static void scare_all(HomeDefend *d)
{
    int i;
    for (i = 0; i < HDG_COUNT; i++) scare(d, &d->g[i]);
}

/* A window's glass breaks with a brute in the frame: the guests near it
 * flee into the Bedroom (and stay safe); the decoy standing at it is face
 * to face with a demon. */
static void breach(HomeDefend *d, int k)
{
    int i, lx = d->lane_x[HDL_WIN0 + k], ly = d->lane_y[HDL_WIN0 + k];
    d->ev |= HDE_WINDOW_BREAK;
    for (i = 0; i < HDG_COUNT; i++) {
        HomeGuest *g = &d->g[i];
        if (!g->present || g->fled) continue;
        if (i == HDG_MAYOR && d->mayor_win == k) scare(d, g);
        else if (tile_dist(g->x, g->y, lx, ly) <= 2) { g->fled = 1; d->ev |= HDE_FLEE; }
    }
}

/* ---- the demons ----------------------------------------------------------------------- */
static int attackers(const HomeDefend *d, int lane)
{
    int i, n = 0;
    for (i = 0; i < HD_DEMONS; i++)
        if (d->dm[i].state == HDS_HIT && d->dm[i].lane == lane) n++;
    return n;
}

static int lane_slots(int lane) { return lane == HDL_DOOR ? hdf[HDF_DOOR_SLOTS] : 1; }

static int door_open(const HomeDefend *d) { return d->door_broken; }

static void spawn(HomeDefend *d, int kind)
{
    const HdWave *w = &hd_tables[d->table].w[d->wave];
    HomeDemon *m = NULL;
    int i, lane = HDL_DOOR;
    for (i = 0; i < HD_DEMONS; i++)
        if (d->dm[i].state == HDS_NONE || d->dm[i].state == HDS_GONE) { m = &d->dm[i]; break; }
    if (!m) return;                                   /* full: it waits at the gate */
    if (kind == HDK_BRUTE && d->nwin) {
        /* the brutes see the decoy at the window; otherwise they take turns */
        if ((w->flags & HWF_DECOY) && d->mayor_win >= 0) lane = HDL_WIN0 + d->mayor_win;
        else lane = HDL_WIN0 + (d->brutes_spawned % d->nwin);
        d->brutes_spawned++;
    } else {
        kind = HDK_IMP;
    }
    memset(m, 0, sizeof(*m));
    m->kind = (uint8_t)kind;
    m->lane = (uint8_t)lane;
    m->state = HDS_WALK;
    m->dist = hdf[HDF_WALK_PX] + (int16_t)rng_range(&d->rng, 6);
    m->hp = kind == HDK_BRUTE ? hdf[HDF_BRUTE_HP] : hdf[HDF_IMP_HP];
    m->nerve = kind == HDK_BRUTE ? hdf[HDF_BRUTE_NERVE] : hdf[HDF_IMP_NERVE];
    m->tx = d->lane_x[lane];
    m->ty = d->lane_y[lane];
    d->spawned++;
}

static void flee(HomeDemon *m)
{
    m->state = HDS_FLEE;
    m->timer = 0;
    if (m->dist < 0) m->dist = 0;
}

static void enter(HomeDefend *d, HomeDemon *m)
{
    m->state = HDS_INSIDE;
    m->timer = 0;
    m->hops = hdf[HDF_INSIDE_HOPS];
    m->dist = 0;
    m->tx = d->lane_x[HDL_DOOR];
    m->ty = d->lane_y[HDL_DOOR];
    d->ev |= HDE_ENTER;
    scare_all(d);
}

static void demon_hit(HomeDefend *d, HomeDemon *m)
{
    if (m->lane == HDL_DOOR) {
        if (door_open(d)) { enter(d, m); return; }
        d->door_hits++;
        d->ev |= HDE_DOOR_HIT;
        if (d->door_hits % hdf[HDF_RIMSHOT_EVERY] == 0) d->ev |= HDE_RIMSHOT;
        if (d->door_hp > 0) d->door_hp--;
        else { d->door_broken = 1; d->ev |= HDE_DOOR_BREAK; enter(d, m); return; }
    } else {
        int k = m->lane - HDL_WIN0;
        if (d->plate_hp[k]) {
            d->plate_hp[k]--;
            d->ev |= HDE_PLATE_HIT;
            if (!d->plate_hp[k]) { d->plate_down |= (uint8_t)(1 << k); d->ev |= HDE_PLATE_FALL; }
        } else if (!(d->broken & (1 << k))) {
            d->broken |= (uint8_t)(1 << k);
            breach(d, k);
        }
    }
    if (m->nerve) m->nerve--;
    if (!m->nerve) flee(m);
}

static void demon_step(HomeDefend *d, HomeDemon *m, int idx)
{
    switch (m->state) {
    case HDS_WALK: {
        int stop = 0;
        if (m->kind == HDK_IMP || ((d->t + idx) & 1)) m->dist--;
        if (!(m->lane == HDL_DOOR && door_open(d)) && attackers(d, m->lane) >= lane_slots(m->lane))
            stop = hdf[HDF_QUEUE_PX];
        if (m->dist < stop) m->dist = (int16_t)stop;
        if (m->dist <= 0) {
            m->dist = 0;
            if (m->lane == HDL_DOOR && door_open(d)) { enter(d, m); break; }
            m->state = HDS_HIT;
            m->timer = 0;
            /* the decoy: the first brute at the Mayor's window stares back at
             * him, waves, and wanders off. It only works once. */
            if (m->kind == HDK_BRUTE && m->lane == HDL_WIN0 + d->mayor_win && d->mayor_win >= 0 &&
                (hd_tables[d->table].w[d->wave].flags & HWF_DECOY) && !d->decoy_used &&
                d->g[HDG_MAYOR].present && !d->g[HDG_MAYOR].fled) {
                d->decoy_used = 1;
                d->ev |= HDE_DECOY;
                m->state = HDS_STARE;
            }
        }
        break; }
    case HDS_HIT:
        if (++m->timer >= (m->kind == HDK_BRUTE ? hdf[HDF_BRUTE_HIT] : hdf[HDF_IMP_HIT])) {
            m->timer = 0;
            demon_hit(d, m);
        }
        break;
    case HDS_STARE:
        if (++m->timer >= hdf[HDF_STARE]) flee(m);
        break;
    case HDS_INSIDE:
        if (++m->timer >= hdf[HDF_HOP]) {
            int nx = m->tx + rng_range(&d->rng, 3) - 1, ny = m->ty + rng_range(&d->rng, 3) - 1;
            int i;
            m->timer = 0;
            if (nx >= 0 && ny >= 0 && nx < d->w && ny < d->d) { m->tx = (uint8_t)nx; m->ty = (uint8_t)ny; }
            for (i = 0; i < HDG_COUNT; i++)
                if (d->g[i].present && tile_dist(d->g[i].x, d->g[i].y, m->tx, m->ty) <= 1) scare(d, &d->g[i]);
            if (m->hops) m->hops--;
            if (!m->hops) { flee(m); m->tx = d->lane_x[HDL_DOOR]; m->ty = d->lane_y[HDL_DOOR]; }
        }
        break;
    case HDS_FLEE:
        m->dist = (int16_t)(m->dist + hdf[HDF_FLEE_PX]);
        if (m->dist > hdf[HDF_WALK_PX] + 8) m->state = HDS_GONE;
        break;
    case HDS_DEAD:
        if (++m->timer >= hdf[HDF_POP]) m->state = HDS_GONE;
        break;
    default:
        break;
    }
}

/* the turret's arc: its window's wall (the door too if it shares it), and
 * anything that got indoors */
static int covered(const HomeDefend *d, const HomeDemon *m)
{
    if (m->state == HDS_INSIDE) return 1;
    if (m->state != HDS_WALK && m->state != HDS_HIT && m->state != HDS_STARE) return 0;
    if (m->dist > hdf[HDF_RANGE_PX]) return 0;
    return lane_wall(d, m->lane) == lane_wall(d, HDL_WIN0 + d->turret);
}

static void turret_step(HomeDefend *d)
{
    int i, best = -1, key = 0x7FFF;
    if (d->turret < 0 || !d->ammo) return;
    if (d->fire_cd) { d->fire_cd--; return; }
    for (i = 0; i < HD_DEMONS; i++) {
        const HomeDemon *m = &d->dm[i];
        int k;
        if (!covered(d, m)) continue;
        /* indoors first, then whoever is hurting the door, nearest first */
        k = (m->state == HDS_INSIDE ? 0 : 200) + m->dist * 2 + (m->kind == HDK_BRUTE ? 1 : 0) +
            (m->lane == HDL_DOOR ? 0 : 100);
        if (k < key) { key = k; best = i; }
    }
    if (best < 0) return;
    d->ammo--;
    d->fire_cd = hdf[HDF_TURRET_CD];
    d->shots++;
    d->target = (int8_t)best;
    d->ev |= HDE_SHOT;
    if (d->dm[best].hp) d->dm[best].hp--;
    if (!d->dm[best].hp) {
        d->dm[best].state = HDS_DEAD;
        d->dm[best].timer = 0;
        d->killed++;
        d->ev |= HDE_KILL;
    }
}

static int active(const HomeDefend *d)
{
    int i, n = 0;
    for (i = 0; i < HD_DEMONS; i++)
        if (d->dm[i].state != HDS_NONE && d->dm[i].state != HDS_GONE) n++;
    return n;
}

static void wave_step(HomeDefend *d)
{
    const HdWave *w = &hd_tables[d->table].w[d->wave];
    int total = w->imps + w->brutes, i;
    d->wave_t++;
    /* the chimney beat (wave 3 of the siege): everything freezes */
    if ((w->flags & HWF_CHIMNEY) && !d->chimney && d->wave_t == hdf[HDF_CHIMNEY_AT]) {
        d->chimney = 1;
        d->freeze = hdf[HDF_CHIMNEY_LEN];
        d->ev |= HDE_CHIMNEY;
        return;
    }
    /* spawns: the brutes evenly spread among the imps */
    if (d->spawned < total && d->wave_t >= (uint16_t)(d->spawned * w->gap + 1)) {
        int n = d->spawned, brute = 0;
        if (w->brutes) {
            int b;
            for (b = 0; b < w->brutes; b++)
                if (n == (b + 1) * total / (w->brutes + 1)) brute = 1;
        }
        spawn(d, brute ? HDK_BRUTE : HDK_IMP);
    }
    /* the rule of three: "the door is going to give" x3, 15 f apart */
    if ((w->flags & HWF_ALARMS) && !d->alarm && d->door_max &&
        (d->door_hp * 2 <= d->door_max || d->wave_t >= hdf[HDF_ALARM_AT] * 4)) {
        d->alarm = 1;
        d->alarm_t = 0;
        d->ev |= HDE_ALARM;
    } else if (d->alarm && d->alarm < 3 && ++d->alarm_t >= hdf[HDF_ALARM_GAP]) {
        d->alarm++;
        d->alarm_t = 0;
        d->ev |= HDE_ALARM;
    }
    for (i = 0; i < HD_DEMONS; i++) demon_step(d, &d->dm[i], i);
    turret_step(d);
    /* the wave ends when everyone has run off (or at its timeout) */
    if ((d->spawned >= total && !active(d)) || d->wave_t >= w->timeout) {
        for (i = 0; i < HD_DEMONS; i++)
            if (d->dm[i].state != HDS_NONE && d->dm[i].state != HDS_GONE) d->dm[i].state = HDS_GONE;
        d->held = (uint8_t)(d->wave + 1);
        d->panic = 0;
        if (d->wave + 1 < d->nwaves) {
            d->phase = HDP_QUIET;
            d->phase_t = w->quiet;
        } else {
            d->phase = HDP_DAWN;
            d->phase_t = hdf[HDF_DAWN];
            d->ev |= HDE_DAWN;
        }
    }
}

int home_defend_tick(HomeDefend *d)
{
    int steps, s;
    d->ev = 0;
    if (d->phase == HDP_DONE) return 0;
    d->t++;
    if (d->busy) d->busy--;
    for (s = 0; s < HDG_COUNT; s++) d->g[s].flinch = 0;
    if (d->freeze) {
        if (!--d->freeze) { d->panic = 1; d->ev |= HDE_THAW; }
        return 1;
    }
    steps = d->panic ? 2 : 1;                         /* [PANIC]: the 2x clock */
    for (s = 0; s < steps && d->phase != HDP_DONE; s++) {
        switch (d->phase) {
        case HDP_READY:
            d->phase = HDP_WAVE;
            d->wave_t = 0;
            d->spawned = 0;
            d->brutes_spawned = 0;
            d->alarm = 0;
            d->ev |= HDE_WAVE;
            break;
        case HDP_WAVE:
            wave_step(d);
            if (d->freeze) return 1;
            break;
        case HDP_QUIET:
            if (d->phase_t) d->phase_t--;
            if (!d->phase_t) { d->wave++; d->phase = HDP_READY; }
            break;
        case HDP_DAWN:
            if (d->phase_t) d->phase_t--;
            if (!d->phase_t) d->phase = HDP_DONE;
            break;
        default:
            break;
        }
    }
    return d->phase != HDP_DONE;
}

/* ---- the player ------------------------------------------------------------------------ */
int home_defend_can(const HomeDefend *d, int act, int idx)
{
    if (d->busy || d->phase == HDP_DONE || d->freeze) return 0;
    switch (act) {
    /* a door with no barricade at all can still be held shut: 10 HP */
    case HDA_REPAIR: return d->door_hp < DOOMO_MAX(d->door_max, hdf[HDF_REPAIR_HP]) || d->door_broken;
    case HDA_REWIND: return d->turret >= 0 && d->ammo < hdf[HDF_AMMO];
    case HDA_COMFORT:
        return idx >= 0 && idx < HDG_COUNT && d->g[idx].present && !d->g[idx].fled && d->g[idx].safe &&
               d->g[idx].calm < hdf[HDF_CALM_MAX];
    case HDA_PLATE:
        return idx >= 0 && idx < d->nwin && (d->plate_down & (1 << idx)) && !(d->broken & (1 << idx));
    default: return 0;
    }
}

int home_defend_act(HomeDefend *d, int act, int idx)
{
    if (!home_defend_can(d, act, idx)) return 0;
    switch (act) {
    case HDA_REPAIR:
        d->door_hp = (int16_t)DOOMO_MIN(d->door_hp + hdf[HDF_REPAIR_HP], DOOMO_MAX(d->door_max, hdf[HDF_REPAIR_HP]));
        d->door_broken = 0;
        d->busy = hdf[HDF_REPAIR_BUSY];
        d->ev |= HDE_REPAIR;
        break;
    case HDA_REWIND:
        d->ammo = hdf[HDF_AMMO];
        d->busy = hdf[HDF_REWIND_BUSY];
        d->ev |= HDE_REWIND;
        break;
    case HDA_COMFORT:
        d->g[idx].calm++;
        d->busy = hdf[HDF_COMFORT_BUSY];
        d->ev |= HDE_COMFORT;
        break;
    case HDA_PLATE:
        d->plate_hp[idx] = hdf[HDF_PLATE_HITS];
        d->plate_down &= (uint8_t)~(1 << idx);
        d->busy = hdf[HDF_PLATE_BUSY];
        d->ev |= HDE_PLATE_BACK;
        break;
    }
    d->busy_act = (uint8_t)act;
    return 1;
}

/* What A does at tile (x, y): the action and its index, -1 none. */
int home_defend_action_at(const HomeDefend *d, int x, int y, int *idx)
{
    int i;
    *idx = 0;
    for (i = 0; i < HDG_COUNT; i++)
        if (d->g[i].present && !d->g[i].fled && d->g[i].x == x && d->g[i].y == y) { *idx = i; return HDA_COMFORT; }
    if (d->turret >= 0 && d->turret_x == x && d->turret_y == y) return HDA_REWIND;
    for (i = 0; i < d->nwin; i++)
        if ((d->plate_down & (1 << i)) && d->lane_x[HDL_WIN0 + i] == x && d->lane_y[HDL_WIN0 + i] == y) {
            *idx = i;
            return HDA_PLATE;
        }
    if (tile_dist(d->lane_x[HDL_DOOR], d->lane_y[HDL_DOOR], x, y) <= 1) return HDA_REPAIR;
    return -1;
}

int home_defend_safe(const HomeDefend *d)
{
    int i, n = 0;
    for (i = 0; i < HDG_COUNT; i++) n += d->g[i].present && d->g[i].safe;
    return n;
}

/* Dawn: the result goes into HOME (the cache the story reads, the best wave
 * held, plates owned) and a won Night Raid drops scrap. */
void home_defend_finish(HomeDefend *d)
{
    HomeState *s = home_state();
    int safe = home_defend_safe(d), best, plates;
    if (d->table == HDT_RAID && d->held >= d->nwaves && !d->door_broken) {
        DoomoStry *st = story_stry();
        int won = (st->nests >> 4) & 7;
        inv_add(ITEM_SCRAP, 1 + d->killed / 2);
        if (won < 7) st->nests = (uint8_t)((st->nests & 0x8F) | ((won + 1) << 4));
    }
    if (!s) return;
    if (hd_tables[d->table].guests) s->cache[HC_DEFEND] = (uint8_t)safe;
    best = s->defence & 15;
    if (d->held > best) best = d->held;
    plates = home_owned(home_furn_by_role(FR_PLATE));
    s->defence = (uint8_t)((best & 15) | ((plates > 15 ? 15 : plates) << 4));
    s->ver_flags |= HVF_TOUCHED;
    if (doomo_town.mode == TOWN_STORY) {
        if (d->plated) story_set_flag(SF_HOUSE_PLATED, 1);
        if (d->turret >= 0) story_set_flag(SF_HOUSE_TURRET, 1);
    }
}

/* The post-game Arena's view of the player's house (HF sec.10.3: "home_defend.c
 * exports the room as waves + a spawn list"): the Living Room as saved. */
int home_defend_export(HomeDefendMap *m)
{
    HomeRoom r;
    HomeDefend d;
    int k, w;
    memset(m, 0, sizeof(*m));
    home_room_load(HOME_ROOM_LIVING, &r);
    home_defend_begin(&d, HDT_ARENA, &r, HOME_ROOM_LIVING);
    m->w = d.w;
    m->d = d.d;
    m->door_x = d.lane_x[HDL_DOOR];
    m->door_y = d.lane_y[HDL_DOOR];
    m->nwin = d.nwin;
    for (k = 0; k < d.nwin; k++) { m->win_x[k] = d.lane_x[HDL_WIN0 + k]; m->win_y[k] = d.lane_y[HDL_WIN0 + k]; }
    m->barricade = (uint8_t)d.barricade;
    m->plated = d.plated;
    m->turret_win = (uint8_t)d.turret;
    m->nwaves = d.nwaves;
    for (w = 0; w < d.nwaves && w < HOME_DEFEND_WAVES; w++) {
        m->imps[w] = (uint8_t)home_defend_wave_imps(HDT_ARENA, w);
        m->brutes[w] = (uint8_t)home_defend_wave_brutes(HDT_ARENA, w);
    }
    return d.nwaves;
}
