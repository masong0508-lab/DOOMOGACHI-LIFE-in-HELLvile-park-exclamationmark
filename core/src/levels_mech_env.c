/* levels_mech_env.c - mechanic kit B, environment and light (owner: levels, LV4).
 *
 * See core/include/doomo/lv_env.h. Every function reads ROM zones and writes the caller's LvEnv
 * and the live map's `light`, `floor_h`, CELL_WATER and `wall` bytes; no statics. A cell is
 * REBUILT from the definition's cell plus the state (cell_sync), never patched, so a fog that
 * comes back, a drained basin or a named ward restores the exact original bytes.
 *
 * Not on the PS1 (no new resident byte before PS1-RAM: LV_HAS_NEW is 0 there): the file is empty
 * in that build. On the GBA nothing links until a level calls it (--gc-sections).
 */
#include "doomo/lv_env.h"

#if !defined(DOOMO_PLATFORM_PS1)

#include "doomo/textures.h"
#include <stddef.h>
#include <string.h>

#define NO_LAMP 255
#define IDLE    255

static int in_rect(const DoomoZone *z, int cx, int cy)
{
    return cx >= z->x0 && cx <= z->x1 && cy >= z->y0 && cy <= z->y1;
}

static int is_decay(const DoomoZone *z) { return z->kind == ZONE_WARD || z->kind == ZONE_COLLAPSE; }

/* the ordinal of a tide / decay zone among its kind, -1 beyond the state's size */
static int tide_slot(const DoomoMapExt *x, int zi)
{
    int i, k = 0;
    for (i = 0; i < zi; i++) if (x->zones[i].kind == ZONE_TIDE) k++;
    return k < LV_ENV_TIDE_N ? k : -1;
}

static int decay_slot(const DoomoMapExt *x, int zi)
{
    int i, k = 0;
    for (i = 0; i < zi; i++) if (is_decay(&x->zones[i])) k++;
    return k < LV_ENV_DECAY_N ? k : -1;
}

static int cheb(int a, int b) { a = a < 0 ? -a : a; b = b < 0 ? -b : b; return a > b ? a : b; }

/* ---- the header --------------------------------------------------------------------- */
int lv_env_view(const DoomoMapExt *x)
{
    if (!x || !x->view) return 0;
    return x->view < LV_ENV_VIEW_MIN ? LV_ENV_VIEW_MIN : x->view;
}

/* ---- one cell, rebuilt ---------------------------------------------------------------- */
static int cell_sync(LvEnv *e, int cx, int cy)
{
    const DoomoMapExt *x = e->x;
    DoomoMap *m = e->map;
    int i = cy * m->w + cx, zi, eff, mod = 0, fl, wall, water;
    DoomoCell *c = &m->cells[i];
    const DoomoCell *d = &m->def->cells[i];
    DoomoCell was = *c;
    int mover = (d->flags & (CELL_DOOR | CELL_LIFT)) != 0;

    /* floor: the tide raises it, a fallen floor drops it */
    fl = d->floor_h;
    wall = d->wall;
    water = d->flags & CELL_WATER;
    if (!mover && !d->wall) {
        for (zi = map_zone_at(x, cx, cy, ZONE_TIDE, 0); zi >= 0; zi = map_zone_at(x, cx, cy, ZONE_TIDE, zi + 1)) {
            int k = tide_slot(x, zi);
            if (k >= 0 && e->tide[k] > d->floor_h && e->tide[k] <= d->ceil_h - 7) {
                fl = e->tide[k];
                water = CELL_WATER;
            }
        }
        for (zi = map_zone_at(x, cx, cy, ZONE_COLLAPSE, 0); zi >= 0; zi = map_zone_at(x, cx, cy, ZONE_COLLAPSE, zi + 1))
            if (e->gone & (1u << zi)) {
                fl = d->floor_h - x->zones[zi].arg;
                if (fl < -128) fl = -128;
            }
    }
    for (zi = map_zone_at(x, cx, cy, ZONE_WARD, 0); zi >= 0; zi = map_zone_at(x, cx, cy, ZONE_WARD, zi + 1))
        if (e->gone & (1u << zi)) wall = LV_ENV_FOGWALL;
    c->floor_h = (int8_t)fl;
    c->wall = (uint8_t)wall;
    c->flags = (uint8_t)((c->flags & ~CELL_WATER) | water);

    /* light: the cell's own, held by a dark zone / a show room between shows, raised by candles,
     * the lantern and the lights, lowered by fog */
    eff = d->light ? d->light : (m->ambient ? m->ambient : 12);
    zi = map_zone_at(x, cx, cy, ZONE_DARK, 0);
    if (zi >= 0) { eff = x->zones[zi].arg; mod = 1; }
    zi = map_zone_at(x, cx, cy, ZONE_SHOW, 0);
    if (zi >= 0) {
        if (e->show_on) { if (eff < LV_ENV_SHOW_LIGHT) eff = LV_ENV_SHOW_LIGHT; mod = 1; }
        else if (x->zones[zi].arg) { eff = x->zones[zi].arg; mod = 1; }
    }
    for (zi = map_zone_at(x, cx, cy, ZONE_CANDLE, 0); zi >= 0; zi = map_zone_at(x, cx, cy, ZONE_CANDLE, zi + 1))
        if (e->lit & (1u << zi)) {
            int l = x->zones[zi].arg ? x->zones[zi].arg : LV_ENV_CANDLE_LIGHT;
            if (eff < l) eff = l;
            mod = 1;
        }
    if (e->lamp_x != NO_LAMP) {
        int dd = cheb(cx - e->lamp_x, cy - e->lamp_y);
        if (dd <= LV_ENV_LAMP_R) {
            int l = LV_ENV_LAMP_LIGHT - 2 * dd;
            if (eff < l) eff = l;
            mod = 1;
        }
    }
    for (zi = map_zone_at(x, cx, cy, ZONE_FOG, 0); zi >= 0; zi = map_zone_at(x, cx, cy, ZONE_FOG, zi + 1))
        if (!(e->fog_off & (1u << zi))) { eff -= LV_ENV_FOG_DROP; if (eff < 1) eff = 1; mod = 1; }
    c->light = mod ? (uint8_t)(eff > 15 ? 15 : eff) : d->light;
    return memcmp(&was, c, sizeof(was)) != 0;
}

static int sync_rect(LvEnv *e, int x0, int y0, int x1, int y1)
{
    int cx, cy, n = 0;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= e->map->w) x1 = e->map->w - 1;
    if (y1 >= e->map->h) y1 = e->map->h - 1;
    for (cy = y0; cy <= y1; cy++)
        for (cx = x0; cx <= x1; cx++) n += cell_sync(e, cx, cy);
    return n;
}

static int sync_zone(LvEnv *e, int zi)
{
    const DoomoZone *z = &e->x->zones[zi];
    return sync_rect(e, z->x0, z->y0, z->x1, z->y1);
}

/* the lowest open floor of a tide zone in the definition: its basin's bottom */
static int basin_bottom(const LvEnv *e, const DoomoZone *z)
{
    int cx, cy, lo = 127;
    for (cy = z->y0; cy <= z->y1 && cy < e->map->h; cy++)
        for (cx = z->x0; cx <= z->x1 && cx < e->map->w; cx++) {
            const DoomoCell *d = &e->map->def->cells[cy * e->map->w + cx];
            if (!d->wall && d->floor_h < lo) lo = d->floor_h;
        }
    return lo;
}

/* ---- begin / check ---------------------------------------------------------------------- */
int lv_env_check(const DoomoMapExt *x)
{
    int i, nt = 0, nd = 0, s;
    if (!x) return LVENV_OK;
    for (i = 0; i < x->zone_count; i++) {
        const DoomoZone *z = &x->zones[i];
        if (z->kind == ZONE_TIDE) {
            nt++;
            if (z->arg > 127) return LVENV_TIDE_ARG;
        }
        if (is_decay(z)) nd++;
    }
    if (nt > LV_ENV_TIDE_N) return LVENV_TIDE_MANY;
    if (nd > LV_ENV_DECAY_N) return LVENV_DECAY_MANY;
    /* a swap set may not rewrite what a zone of this kit owns */
    for (s = 0; s < x->swap_count; s++) {
        const DoomoSwapSet *ss = &x->swaps[s];
        for (i = 0; i < ss->count; i++) {
            const DoomoSwapCell *o = &x->swap_cells[ss->first + i];
            int z, cx, cy;
            if (!x->def || !x->def->w) continue;
            cx = o->cell % x->def->w;
            cy = o->cell / x->def->w;
            if (!o->x[offsetof(DoomoCell, wall)] && !o->x[offsetof(DoomoCell, floor_h)] &&
                !o->x[offsetof(DoomoCell, light)]) continue;
            for (z = 0; z < x->zone_count; z++) {
                int k = x->zones[z].kind;
                if ((k == ZONE_TIDE || k == ZONE_COLLAPSE || k == ZONE_WARD || k == ZONE_FOG || k == ZONE_CANDLE ||
                     k == ZONE_SHOW || k == ZONE_DARK) && in_rect(&x->zones[z], cx, cy))
                    return LVENV_SWAP_OWNED;
            }
        }
    }
    return LVENV_OK;
}

int lv_env_begin(LvEnv *e, DoomoMap *map, const DoomoMapExt *x)
{
    int i, k = 0, r;
    memset(e, 0, sizeof(*e));
    e->lamp_x = e->lamp_y = NO_LAMP;
    memset(e->decay, IDLE, sizeof(e->decay));
    if (!map || !map->def || !x) return LVENV_OK;      /* a v1 map: inert */
    r = lv_env_check(x);
    if (r != LVENV_OK) return r;
    e->x = x;
    e->map = map;
    e->oil = LV_ENV_OIL_MAX;
    for (i = 0; i < x->zone_count && k < LV_ENV_TIDE_N; i++)
        if (x->zones[i].kind == ZONE_TIDE) e->tide[k++] = (int8_t)basin_bottom(e, &x->zones[i]);
    /* the zones that start changed (fog, held darkness, show rooms off) */
    for (i = 0; i < x->zone_count; i++) {
        int kd = x->zones[i].kind;
        if (kd == ZONE_FOG || kd == ZONE_DARK || kd == ZONE_SHOW) sync_zone(e, i);
    }
    return LVENV_OK;
}

/* ---- the tick ----------------------------------------------------------------------------- */
int lv_env_tick(LvEnv *e, int cx, int cy)
{
    const DoomoMapExt *x = e->x;
    int i, n = 0, in_tide = 0;
    if (!x) return 0;
    e->tick++;
    for (i = 0; i < x->zone_count; i++) {
        const DoomoZone *z = &x->zones[i];
        int inside = cx >= 0 && in_rect(z, cx, cy);
        if (z->kind == ZONE_TIDE && inside) {
            int k = tide_slot(x, i);
            in_tide = 1;
            if (k >= 0 && !(e->drained & (1u << i)) && e->tide[k] < (int)z->arg) {
                if (++e->tide_t >= LV_ENV_TIDE_TICKS) {
                    e->tide_t = 0;
                    e->tide[k]++;
                    n += sync_zone(e, i);
                }
            }
        } else if (is_decay(z)) {
            int k = decay_slot(x, i);
            uint32_t bit = 1u << i;
            if (k < 0) continue;
            if (inside) {
                e->seen |= bit;
                e->decay[k] = IDLE;
            } else if ((e->seen & bit) && !(e->gone & bit) && !(z->kind == ZONE_WARD && (e->named & bit))) {
                if (e->decay[k] == IDLE) {
                    e->decay[k] = (uint8_t)((z->kind == ZONE_WARD ? LV_ENV_DISSOLVE_TICKS : LV_ENV_COLLAPSE_TICKS) /
                                            LV_ENV_DECAY_DIV);
                } else if (e->tick % LV_ENV_DECAY_DIV == 0 && --e->decay[k] == 0) {
                    e->gone |= bit;
                    e->decay[k] = IDLE;
                    n += sync_zone(e, i);
                }
            }
        }
    }
    if (!in_tide) e->tide_t = 0;

    /* showtime: the lights blaze for the last LV_ENV_SHOW_ON ticks of every cycle */
    if (map_zone_next(x, ZONE_SHOW, 0) >= 0) {
        int on;
        if (++e->show_t >= LV_ENV_SHOW_TICKS) e->show_t = 0;
        on = e->show_t >= LV_ENV_SHOW_TICKS - LV_ENV_SHOW_ON;
        if (on != e->show_on) {
            e->show_on = (uint8_t)on;
            for (i = map_zone_next(x, ZONE_SHOW, 0); i >= 0; i = map_zone_next(x, ZONE_SHOW, i + 1)) n += sync_zone(e, i);
        }
    }

    /* the lantern (dark levels): burns while you walk, lights the 5 x 5 round you */
    if (x->flags & MAPX_DARK) {
        int nx = NO_LAMP, ny = NO_LAMP;
        if (cx >= 0 && e->oil > 0) e->oil--;
        if (cx >= 0 && e->oil > 0) { nx = cx; ny = cy; }
        if (nx != e->lamp_x || ny != e->lamp_y) {
            int ox = e->lamp_x, oy = e->lamp_y;
            e->lamp_x = (uint8_t)nx;
            e->lamp_y = (uint8_t)ny;
            if (ox != NO_LAMP) n += sync_rect(e, ox - LV_ENV_LAMP_R, oy - LV_ENV_LAMP_R, ox + LV_ENV_LAMP_R, oy + LV_ENV_LAMP_R);
            if (nx != NO_LAMP) n += sync_rect(e, nx - LV_ENV_LAMP_R, ny - LV_ENV_LAMP_R, nx + LV_ENV_LAMP_R, ny + LV_ENV_LAMP_R);
        }
    }
    return n;
}

int lv_env_view_at(const LvEnv *e, int cx, int cy)
{
    const DoomoMapExt *x = e->x;
    int v, f, zi;
    if (!x) return 0;
    v = lv_env_view(x);
    f = x->fog;
    for (zi = map_zone_at(x, cx, cy, ZONE_FOG, 0); zi >= 0; zi = map_zone_at(x, cx, cy, ZONE_FOG, zi + 1))
        if (!(e->fog_off & (1u << zi)) && (!f || x->zones[zi].arg < f)) f = x->zones[zi].arg;
    if (f) {
        if (f < LV_ENV_VIEW_MIN) f = LV_ENV_VIEW_MIN;
        if (!v || f < v) v = f;
    }
    return v;
}

uint32_t lv_env_hash(const LvEnv *e)
{
    uint32_t h = 2166136261u;
    int i;
#define MIX(v) do { h ^= (uint32_t)(v); h *= 16777619u; } while (0)
    MIX(e->fog_off); MIX(e->lit); MIX(e->seen); MIX(e->gone); MIX(e->named); MIX(e->drained);
    MIX(e->tick); MIX(e->oil); MIX(e->show_t); MIX(e->applied);
    MIX(e->lamp_x); MIX(e->lamp_y); MIX(e->tide_t); MIX(e->face); MIX(e->show_on);
    for (i = 0; i < LV_ENV_TIDE_N; i++) MIX((uint8_t)e->tide[i]);
    for (i = 0; i < LV_ENV_DECAY_N; i++) MIX(e->decay[i]);
#undef MIX
    return h;
}

/* ---- fog and valves ------------------------------------------------------------------------- */
int lv_env_fog_set(LvEnv *e, int tag, int on)
{
    int zi, n = 0;
    if (!e->x || !tag) return 0;
    for (zi = map_zone_next(e->x, ZONE_FOG, 0); zi >= 0; zi = map_zone_next(e->x, ZONE_FOG, zi + 1)) {
        uint32_t bit = 1u << zi;
        int off = (e->fog_off & bit) != 0;
        if (e->x->zones[zi].tag != tag || off == !on) continue;
        if (on) e->fog_off &= ~bit; else e->fog_off |= bit;
        sync_zone(e, zi);
        n++;
    }
    return n;
}

int lv_env_fog_at(const LvEnv *e, int cx, int cy)
{
    int zi;
    if (!e->x) return 0;
    for (zi = map_zone_at(e->x, cx, cy, ZONE_FOG, 0); zi >= 0; zi = map_zone_at(e->x, cx, cy, ZONE_FOG, zi + 1))
        if (!(e->fog_off & (1u << zi))) return 1;
    return 0;
}

/* ---- the tide ------------------------------------------------------------------------------- */
int lv_env_drain(LvEnv *e, int tag)
{
    int zi, n = 0;
    if (!e->x || !tag) return 0;
    for (zi = map_zone_next(e->x, ZONE_TIDE, 0); zi >= 0; zi = map_zone_next(e->x, ZONE_TIDE, zi + 1)) {
        int k = tide_slot(e->x, zi);
        if (e->x->zones[zi].tag != tag || k < 0 || (e->drained & (1u << zi))) continue;
        e->drained |= 1u << zi;
        e->tide[k] = (int8_t)basin_bottom(e, &e->x->zones[zi]);
        sync_zone(e, zi);
        n++;
    }
    return n;
}

int lv_env_tide_level(const LvEnv *e, int tag)
{
    int zi;
    if (!e->x) return -128;
    for (zi = map_zone_next(e->x, ZONE_TIDE, 0); zi >= 0; zi = map_zone_next(e->x, ZONE_TIDE, zi + 1))
        if (e->x->zones[zi].tag == tag) {
            int k = tide_slot(e->x, zi);
            return k >= 0 ? e->tide[k] : -128;
        }
    return -128;
}

/* ---- lantern and candles -------------------------------------------------------------------- */
int lv_env_candle_use(LvEnv *e, int cx, int cy)
{
    int zi;
    if (!e->x) return 0;
    for (zi = map_zone_next(e->x, ZONE_CANDLE, 0); zi >= 0; zi = map_zone_next(e->x, ZONE_CANDLE, zi + 1)) {
        const DoomoZone *z = &e->x->zones[zi];
        if (cx < z->x0 - LV_ENV_USE_RANGE || cx > z->x1 + LV_ENV_USE_RANGE ||
            cy < z->y0 - LV_ENV_USE_RANGE || cy > z->y1 + LV_ENV_USE_RANGE) continue;
        if (e->lit & (1u << zi)) continue;           /* lit already: a candle beside it may be out */
        e->lit |= 1u << zi;
        sync_zone(e, zi);
        return zi + 1;
    }
    return 0;
}

int lv_env_candles_total(const LvEnv *e)
{
    int zi, n = 0;
    if (!e->x) return 0;
    for (zi = map_zone_next(e->x, ZONE_CANDLE, 0); zi >= 0; zi = map_zone_next(e->x, ZONE_CANDLE, zi + 1)) n++;
    return n;
}

int lv_env_candles_lit(const LvEnv *e)
{
    uint32_t v = e->lit;
    int n = 0;
    for (; v; v &= v - 1) n++;
    return n;
}

void lv_env_refill(LvEnv *e) { e->oil = LV_ENV_OIL_MAX; }
int  lv_env_lantern_on(const LvEnv *e) { return e->oil > 0; }

/* ---- the two faces ---------------------------------------------------------------------------- */
int lv_env_face_flip(LvEnv *e, int set)
{
    int n;
    if (!e->x || !e->map || set < 0 || set >= e->x->swap_count) return 0;
    if (e->applied & (1u << set)) n = map_swap_revert(e->map, e->x, set, &e->applied);
    else n = map_swap_apply(e->map, e->x, set, &e->applied);
    e->face ^= 1;
    return n;
}

int lv_env_face_of(const LvEnv *e, int cx, int cy)
{
    if (!e->x) return 0;
    if (map_zone_at(e->x, cx, cy, ZONE_FACE_A, 0) >= 0) return 1;
    if (map_zone_at(e->x, cx, cy, ZONE_FACE_B, 0) >= 0) return 2;
    return 0;
}

int lv_env_out_of_phase(const LvEnv *e, int cx, int cy)
{
    int f = lv_env_face_of(e, cx, cy);
    return f && f != (e->face ? 2 : 1);
}

/* ---- showtime ---------------------------------------------------------------------------------- */
int lv_env_showtime(const LvEnv *e) { return e->show_on; }

/* ---- wards and collapses ----------------------------------------------------------------------- */
int lv_env_name(LvEnv *e, int ward)
{
    int zi, n = 0;
    if (!e->x) return 0;
    for (zi = map_zone_next(e->x, ZONE_WARD, 0); zi >= 0; zi = map_zone_next(e->x, ZONE_WARD, zi + 1)) {
        uint32_t bit = 1u << zi;
        int k = decay_slot(e->x, zi);
        if (e->x->zones[zi].arg != ward || (e->named & bit)) continue;
        e->named |= bit;
        if (k >= 0) e->decay[k] = IDLE;
        if (e->gone & bit) {
            e->gone &= ~bit;
            sync_zone(e, zi);
        }
        n++;
    }
    return n != 0;
}

int lv_env_gone_at(const LvEnv *e, int cx, int cy)
{
    int zi;
    if (!e->x) return 0;
    for (zi = map_zone_at(e->x, cx, cy, ZONE_NONE, 0); zi >= 0; zi = map_zone_at(e->x, cx, cy, ZONE_NONE, zi + 1))
        if (is_decay(&e->x->zones[zi]) && (e->gone & (1u << zi))) return 1;
    return 0;
}

#endif /* !DOOMO_PLATFORM_PS1 */
