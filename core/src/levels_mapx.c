/* levels_mapx.c - map format v2: the side-table accessors (owner: levels, LV1).
 *
 * A v2 map carries a const DoomoMapExt next to its DoomoMapDef (map.h,
 * tools/mapc.py). Everything here reads ROM tables and the caller's
 * state: no statics, nothing allocated. On the PS1 this file is in the
 * combat + levels overlay group (ps1/tools/ovl_plan.py matches levels_*),
 * and nothing of it links until a level calls it (--gc-sections).
 */
#include "doomo/map.h"

/* The v2 maps. A new level adds `&map_ext_<name>` (a weak reference, like
 * maps.c) above the NULL. */
const DoomoMapExt *const doomo_map_exts[] = {
    NULL
};

const DoomoMapExt *map_ext_of(const DoomoMapDef *d)
{
    int i;
    if (!d) return NULL;
    for (i = 0; doomo_map_exts[i]; i++)
        if (doomo_map_exts[i]->def == d) return doomo_map_exts[i];
    return NULL;
}

int map_zone_at(const DoomoMapExt *x, int cx, int cy, int kind, int from)
{
    int i;
    if (!x || from < 0) return -1;
    for (i = from; i < x->zone_count; i++) {
        const DoomoZone *z = &x->zones[i];
        if (kind != ZONE_NONE && z->kind != kind) continue;
        if (cx >= z->x0 && cx <= z->x1 && cy >= z->y0 && cy <= z->y1) return i;
    }
    return -1;
}

int map_zone_next(const DoomoMapExt *x, int kind, int from)
{
    int i;
    if (!x || from < 0) return -1;
    for (i = from; i < x->zone_count; i++)
        if (kind == ZONE_NONE || x->zones[i].kind == kind) return i;
    return -1;
}

const DoomoPath *map_path_by_id(const DoomoMapExt *x, int id)
{
    int i;
    if (!x) return NULL;
    for (i = 0; i < x->path_count; i++)
        if (x->paths[i].id == id) return &x->paths[i];
    return NULL;
}

int map_path_point(const DoomoMapExt *x, const DoomoPath *p, int i, int *cx, int *cy)
{
    const uint8_t *pt;
    if (!x || !p || i < 0 || i >= p->count) return 0;
    pt = &x->path_pts[(p->first + i) * 2];
    *cx = pt[0];
    *cy = pt[1];
    return 1;
}

int map_thing_flags2(const DoomoMapExt *x, int thing)
{
    if (!x || !x->tflags2 || !x->def || thing < 0 || thing >= x->def->thing_count) return 0;
    return x->tflags2[thing];
}

/* XOR a set's deltas into the live cells: its own inverse. */
static int swap_xor(DoomoMap *m, const DoomoMapExt *x, const DoomoSwapSet *s)
{
    int i, k, n = 0, cells = m->w * m->h;
    if (s->count > DOOMO_SWAP_CELLS_MAX) return 0;
    for (i = 0; i < s->count; i++) {
        const DoomoSwapCell *o = &x->swap_cells[s->first + i];
        uint8_t *c;
        if (o->cell >= cells) continue;
        c = (uint8_t *)&m->cells[o->cell];
        for (k = 0; k < (int)sizeof(DoomoCell); k++) c[k] ^= o->x[k];
        n++;
    }
    return n;
}

int map_swap_apply(DoomoMap *m, const DoomoMapExt *x, int set, uint16_t *applied)
{
    if (!m || !x || !applied || set < 0 || set >= x->swap_count || set >= DOOMO_SWAP_MAX) return 0;
    if (*applied & (1u << set)) return 0;
    *applied = (uint16_t)(*applied | (1u << set));
    return swap_xor(m, x, &x->swaps[set]);
}

int map_swap_revert(DoomoMap *m, const DoomoMapExt *x, int set, uint16_t *applied)
{
    if (!m || !x || !applied || set < 0 || set >= x->swap_count || set >= DOOMO_SWAP_MAX) return 0;
    if (!(*applied & (1u << set))) return 0;
    *applied = (uint16_t)(*applied & ~(1u << set));
    return swap_xor(m, x, &x->swaps[set]);
}

int map_swap_event(DoomoMap *m, const DoomoMapExt *x, int tag, uint16_t *applied)
{
    int i, n = 0;
    if (!m || !x || !applied) return 0;
    for (i = 0; i < x->swap_count && i < DOOMO_SWAP_MAX; i++) {
        const DoomoSwapSet *s = &x->swaps[i];
        if (s->event != tag) continue;
        if (s->mode == MSWAP_ONCE) {
            if (map_swap_apply(m, x, i, applied)) n++;
        } else if (s->mode == MSWAP_TOGGLE) {
            if (*applied & (1u << i)) map_swap_revert(m, x, i, applied);
            else map_swap_apply(m, x, i, applied);
            n++;
        }
    }
    return n;
}
