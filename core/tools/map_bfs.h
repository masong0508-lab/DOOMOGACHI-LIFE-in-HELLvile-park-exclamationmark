/* map_bfs.h - can the player get there? The one reachability search for
 * maps (owner: levels, LV1). Host only: core/tests/test_maps.c runs it on
 * the registered maps and the BFS test maps, core/tools/mapreach.c runs it
 * for tools/levelcheck.py on a mapc export. One implementation, so the
 * test and the tool cannot disagree.
 *
 * A state is (cell, bits). The low 16 bits are the applied swap sets, the
 * high 16 belong to the hooks (below). Edges, all from map data:
 *
 *   walk     4 neighbours: not a wall, the red / blue door only with its
 *            key, a floor at most `rise` higher (lifts count with both
 *            heights), 32 units of headroom. `rise` is DOOMO_STEP_UP,
 *            scaled by 100 / gravity where the cell you leave is in a
 *            ZONE_GRAVITY (arg) or the map sets `gravity`; in a
 *            ZONE_BOUNCE it is the zone's arg in height steps.
 *            Remote doors (tag >= 16) count as open, as before.
 *   path     MPATH_ZIP / MPATH_ARC: first point -> last point.
 *            MPATH_TUBE: first <-> last. MPATH_RIDE: any point -> any
 *            point. MPATH_RAFT: a point -> every later point.
 *            MPATH_ROUTE / MPATH_CHASE: no edge. Both ends must be open
 *            cells in the current swap state.
 *   swap     MSWAP_ONCE: entering a trigger cell whose tag is the set's
 *            event applies the set (forced, as the game does). Standing
 *            on the cell of a kill_event thing of that event may apply it
 *            (the group is dead). MSWAP_TOGGLE: on either kind of cell
 *            the player may flip the set as often as they like (a lever).
 *            MSWAP_SCRIPT sets never change here: the script VM (LV2)
 *            adds their edges through the hooks.
 *
 * Keys are monotone: the search repeats with the keys found until no new
 * key turns up (as test_maps always did).
 *
 * Results: every cell reachable in some state; whether an exit is; and
 * `stuck`: the states the player can get into from which no exit can be
 * reached any more (a soft lock: a one-way drop, a swap that walls the
 * way). At most MAP_BFS_MASKS swap / hook combinations are followed;
 * more sets `overflow` and the map fails.
 *
 * HOOKS for the mechanic kits (LV3-LV5). Edges that need mechanic rules
 * are not map data: stamp doors, named-room persistence, laid bricks. A
 * kit gives its rule here and its test passes the hooks:
 *   step   called for every walk edge the default rule decided: return
 *          1 to allow, 0 to refuse, -1 to keep the default.
 *   extra  called once per state: write up to `max` (cell, bits) states
 *          the player can also get to from here (a stamp picked up sets
 *          one of the high 16 bits; a laid brick is a swap bit).
 */
#ifndef DOOMO_MAP_BFS_H
#define DOOMO_MAP_BFS_H

#include <string.h>
#include "doomo/map.h"
#include "doomo/town.h"      /* actor.h needs DoomoWeapon */
#include "doomo/actor.h"

#define MAP_BFS_CELLS  (DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H)
#define MAP_BFS_MASKS  64

typedef struct MapBfs MapBfs;

typedef struct MapBfsHooks {
    void *user;
    int (*step)(void *user, const MapBfs *b, int from, int to, uint32_t bits, int allowed);
    int (*extra)(void *user, const MapBfs *b, int cell, uint32_t bits, int *to_cell, uint32_t *to_bits, int max);
} MapBfsHooks;

typedef struct MapBfsResult {
    int keys;            /* bit 0 red, bit 1 blue: found */
    int exits;           /* exit cells in the base map */
    int exit_reached;
    int states;          /* (cell, bits) states reached */
    int masks;           /* distinct bits combinations reached */
    int overflow;        /* more than MAP_BFS_MASKS combinations */
    int stuck;           /* reached states that cannot reach an exit */
    int stuck_cell;      /* the first one */
    uint32_t stuck_bits;
    uint8_t reach[MAP_BFS_CELLS];   /* 1: reachable in some state */
} MapBfsResult;

struct MapBfs {
    const DoomoMapDef *d;
    const DoomoCell   *cells;       /* the base cells (d->cells) */
    const DoomoMapExt *x;           /* NULL for a v1 map */
    const MapBfsHooks *hk;
    int keys;
    int nmasks;
    uint32_t masks[MAP_BFS_MASKS];
    uint8_t  has_swap[MAP_BFS_CELLS];
    uint8_t  seen[MAP_BFS_MASKS][MAP_BFS_CELLS];   /* 1 reached, 2 can exit */
    uint32_t queue[MAP_BFS_MASKS * MAP_BFS_CELLS];
    int head, tail, overflow;
};

/* The cell as it is with the swap sets of `bits` applied. */
static void map_bfs_cell(const MapBfs *b, int idx, uint32_t bits, DoomoCell *out)
{
    *out = b->cells[idx];
    if (b->x && b->has_swap[idx] && (bits & 0xFFFFu)) {
        int s, i, k;
        for (s = 0; s < b->x->swap_count; s++) {
            const DoomoSwapSet *set = &b->x->swaps[s];
            if (!(bits & (1u << s))) continue;
            for (i = 0; i < set->count; i++) {
                const DoomoSwapCell *o = &b->x->swap_cells[set->first + i];
                if (o->cell != idx) continue;
                for (k = 0; k < 8; k++) ((uint8_t *)out)[k] ^= o->x[k];
            }
        }
    }
}

static void map_bfs_frange(const DoomoCell *c, int *lo, int *hi)
{
    int a = c->floor_h * DOOMO_HEIGHT_STEP, b = a;
    if (c->flags & CELL_LIFT) b = (int8_t)c->tag * DOOMO_HEIGHT_STEP;
    *lo = DOOMO_MIN(a, b);
    *hi = DOOMO_MAX(a, b);
}

/* How high a floor the player can get onto from this cell (world units). */
static int map_bfs_rise(const MapBfs *b, int idx)
{
    int rise = DOOMO_STEP_UP, g = 0, z;
    int cx = idx % b->d->w, cy = idx / b->d->w;
    if (!b->x) return rise;
    if (b->x->gravity) g = b->x->gravity;
    z = map_zone_at(b->x, cx, cy, ZONE_GRAVITY, 0);
    if (z >= 0) g = b->x->zones[z].arg;
    if (g > 0) rise = DOOMO_STEP_UP * 100 / g;
    z = map_zone_at(b->x, cx, cy, ZONE_BOUNCE, 0);
    if (z >= 0 && b->x->zones[z].arg * DOOMO_HEIGHT_STEP > rise) rise = b->x->zones[z].arg * DOOMO_HEIGHT_STEP;
    return rise;
}

static int map_bfs_can_step(const MapBfs *b, int from, int to, uint32_t bits)
{
    DoomoCell a, n;
    int alo, ahi, blo, bhi, top, ok = 1;
    map_bfs_cell(b, from, bits, &a);
    map_bfs_cell(b, to, bits, &n);
    if (n.wall) ok = 0;
    if (ok && (n.flags & CELL_DOOR)) {
        if (n.tag == DOOMO_DOOR_TAG_RED && !(b->keys & 1)) ok = 0;
        if (n.tag == DOOMO_DOOR_TAG_BLUE && !(b->keys & 2)) ok = 0;
        /* remote doors are opened by events (cleared groups, bosses,
         * triggers) - the combat test proves those fire */
    }
    if (ok) {
        map_bfs_frange(&a, &alo, &ahi);
        map_bfs_frange(&n, &blo, &bhi);
        top = n.ceil_tex == DOOMO_TEX_SKY ? 30000 : n.ceil_h * DOOMO_HEIGHT_STEP;   /* doors: the open height */
        if (blo - ahi > map_bfs_rise(b, from)) ok = 0;
        else if (top - DOOMO_MAX(blo, alo) < DOOMO_CROUCH_HEIGHT) ok = 0;
    }
    if (b->hk && b->hk->step) {
        int r = b->hk->step(b->hk->user, b, from, to, bits, ok);
        if (r >= 0) ok = r;
    }
    return ok;
}

static int map_bfs_layer(MapBfs *b, uint32_t bits)
{
    int i;
    for (i = 0; i < b->nmasks; i++)
        if (b->masks[i] == bits) return i;
    if (b->nmasks >= MAP_BFS_MASKS) { b->overflow = 1; return -1; }
    b->masks[b->nmasks] = bits;
    return b->nmasks++;
}

/* Entering a cell: a trigger cell fires its event, which applies the
 * MSWAP_ONCE sets of that event. */
static uint32_t map_bfs_arrive(const MapBfs *b, int idx, uint32_t bits)
{
    DoomoCell c;
    int s;
    if (!b->x || !b->x->swap_count) return bits;
    map_bfs_cell(b, idx, bits, &c);
    if (c.wall || !(c.flags & CELL_TRIGGER)) return bits;
    for (s = 0; s < b->x->swap_count; s++)
        if (b->x->swaps[s].mode == MSWAP_ONCE && b->x->swaps[s].event == c.tag) bits |= 1u << s;
    return bits;
}

/* Every state one move away from (idx, bits): calls emit(ctx, cell, bits). */
static void map_bfs_succ(MapBfs *b, int idx, uint32_t bits, void (*emit)(MapBfs *, void *, int, uint32_t), void *ctx)
{
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    const DoomoMapDef *d = b->d;
    int x = idx % d->w, y = idx / d->w, k, i, j;
    for (k = 0; k < 4; k++) {
        int nx = x + dx[k], ny = y + dy[k], ni;
        if (nx < 0 || ny < 0 || nx >= d->w || ny >= d->h) continue;
        ni = ny * d->w + nx;
        if (map_bfs_can_step(b, idx, ni, bits)) emit(b, ctx, ni, map_bfs_arrive(b, ni, bits));
    }
    if (b->x) {
        const DoomoMapExt *e = b->x;
        DoomoCell c;
        for (i = 0; i < e->path_count; i++) {
            const DoomoPath *p = &e->paths[i];
            const uint8_t *pt = &e->path_pts[p->first * 2];
            int at = -1;
            if (p->kind == MPATH_ROUTE || p->kind == MPATH_CHASE || p->count < 2) continue;
            for (j = 0; j < p->count; j++) {
                int to = -1;
                if (pt[j * 2] != x || pt[j * 2 + 1] != y) continue;
                at = j;
                if ((p->kind == MPATH_ZIP || p->kind == MPATH_ARC) && at == 0) to = p->count - 1;
                if (p->kind == MPATH_TUBE && (at == 0 || at == p->count - 1)) to = at ? 0 : p->count - 1;
                if (to >= 0) {
                    int ti = pt[to * 2 + 1] * d->w + pt[to * 2];
                    map_bfs_cell(b, ti, bits, &c);
                    if (!c.wall) emit(b, ctx, ti, map_bfs_arrive(b, ti, bits));
                }
                if (p->kind == MPATH_RIDE || p->kind == MPATH_RAFT) {
                    int t;
                    for (t = p->kind == MPATH_RAFT ? at + 1 : 0; t < p->count; t++) {
                        int ti = pt[t * 2 + 1] * d->w + pt[t * 2];
                        if (t == at) continue;
                        map_bfs_cell(b, ti, bits, &c);
                        if (!c.wall) emit(b, ctx, ti, map_bfs_arrive(b, ti, bits));
                    }
                }
            }
        }
        if (e->swap_count) {
            /* the events the player can fire by hand on this cell */
            map_bfs_cell(b, idx, bits, &c);
            for (i = 0; i < e->swap_count; i++) {
                const DoomoSwapSet *s = &e->swaps[i];
                int here = (c.flags & CELL_TRIGGER) && c.tag == s->event;
                if (s->mode != MSWAP_ONCE && s->mode != MSWAP_TOGGLE) continue;
                for (j = 0; j < d->thing_count && !here; j++) {
                    const DoomoThing *t = &d->things[j];
                    if (t->x != x || t->y != y || !(t->flags & THF_KILL_EVENT)) continue;
                    if (t->arg + ((t->flags & THF_DORMANT) ? 1 : 0) == s->event) here = 1;
                }
                if (!here) continue;
                if (s->mode == MSWAP_TOGGLE) emit(b, ctx, idx, bits ^ (1u << i));
                else if (!(bits & (1u << i))) emit(b, ctx, idx, bits | (1u << i));
            }
        }
    }
    if (b->hk && b->hk->extra) {
        int to_cell[16], n;
        uint32_t to_bits[16];
        n = b->hk->extra(b->hk->user, b, idx, bits, to_cell, to_bits, 16);
        for (i = 0; i < n && i < 16; i++)
            if (to_cell[i] >= 0 && to_cell[i] < d->w * d->h) emit(b, ctx, to_cell[i], to_bits[i]);
    }
}

static void map_bfs_visit(MapBfs *b, void *ctx, int idx, uint32_t bits)
{
    int l = map_bfs_layer(b, bits);
    (void)ctx;
    if (l < 0 || b->seen[l][idx]) return;
    b->seen[l][idx] = 1;
    b->queue[b->tail++] = ((uint32_t)l << 16) | (uint32_t)idx;
}

static void map_bfs_can_exit(MapBfs *b, void *ctx, int idx, uint32_t bits)
{
    int l = map_bfs_layer(b, bits);
    if (l >= 0 && b->seen[l][idx] == 2) *(int *)ctx = 1;
}

/* One search with b->keys; returns the keys lying on reached cells. */
static int map_bfs_flood(MapBfs *b, int start, MapBfsResult *r)
{
    const DoomoMapDef *d = b->d;
    int i, l, found = 0, n = d->w * d->h;
    b->nmasks = 0;
    b->head = b->tail = 0;
    memset(b->seen, 0, sizeof(b->seen));
    map_bfs_visit(b, NULL, start, map_bfs_arrive(b, start, 0));
    while (b->head < b->tail) {
        uint32_t s = b->queue[b->head++];
        map_bfs_succ(b, (int)(s & 0xFFFFu), b->masks[s >> 16], map_bfs_visit, NULL);
    }
    memset(r->reach, 0, sizeof(r->reach));
    for (l = 0; l < b->nmasks; l++)
        for (i = 0; i < n; i++)
            if (b->seen[l][i]) r->reach[i] = 1;
    for (i = 0; i < d->thing_count; i++) {
        const DoomoThing *t = &d->things[i];
        if (!r->reach[t->y * d->w + t->x]) continue;
        if (t->type == THING_KEY_RED) found |= 1;
        if (t->type == THING_KEY_BLUE) found |= 2;
    }
    return found;
}

/* The whole search from the cell (sx, sy). `work` is caller storage (it
 * is about 1.3 MB: make it static). hk may be NULL. */
static void map_bfs_run(MapBfs *work, const DoomoMapDef *d, const DoomoMapExt *x, const MapBfsHooks *hk,
                        int sx, int sy, MapBfsResult *r)
{
    MapBfs *b = work;
    int i, l, n = d->w * d->h, prev = -1, changed;
    memset(r, 0, sizeof(*r));
    b->d = d;
    b->cells = d->cells;
    b->x = x;
    b->hk = hk;
    b->keys = 0;
    b->overflow = 0;
    memset(b->has_swap, 0, sizeof(b->has_swap));
    if (x)
        for (l = 0; l < x->swap_count; l++)
            for (i = 0; i < x->swaps[l].count; i++) {
                int c = x->swap_cells[x->swaps[l].first + i].cell;
                if (c < n) b->has_swap[c] = 1;
            }
    while (b->keys != prev) {              /* until no new keys */
        prev = b->keys;
        b->keys |= map_bfs_flood(b, sy * d->w + sx, r);
    }
    r->keys = b->keys;
    r->masks = b->nmasks;
    r->overflow = b->overflow;
    r->states = b->tail;
    /* which reached states can still reach an exit: sweep until stable */
    for (i = 0; i < n; i++)
        if (!d->cells[i].wall && (d->cells[i].flags & CELL_EXIT)) r->exits++;
    for (l = 0; l < b->nmasks; l++)
        for (i = 0; i < n; i++) {
            DoomoCell c;
            if (!b->seen[l][i]) continue;
            map_bfs_cell(b, i, b->masks[l], &c);
            if (!c.wall && (c.flags & CELL_EXIT)) { b->seen[l][i] = 2; r->exit_reached = 1; }
        }
    do {
        int pass;
        changed = 0;
        for (pass = 0; pass < 2; pass++)          /* forward then backward: converges fast */
            for (l = 0; l < b->nmasks; l++) {
                int k;
                for (k = 0; k < n; k++) {
                    int can = 0;
                    i = pass ? n - 1 - k : k;
                    if (b->seen[l][i] != 1) continue;
                    map_bfs_succ(b, i, b->masks[l], map_bfs_can_exit, &can);
                    if (can) { b->seen[l][i] = 2; changed = 1; }
                }
            }
    } while (changed);
    r->stuck_cell = -1;
    for (l = 0; l < b->nmasks; l++)
        for (i = 0; i < n; i++)
            if (b->seen[l][i] == 1) {
                if (!r->stuck) { r->stuck_cell = i; r->stuck_bits = b->masks[l]; }
                r->stuck++;
            }
}

#endif
