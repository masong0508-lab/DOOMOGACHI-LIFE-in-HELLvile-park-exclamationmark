/* levels_mech_move.c - mechanic kit A, movement (owner: levels, LV3). See levels_mech_move.h.
 * Pure rules on the caller's DoomoMapExt and MmMove: no statics, nothing allocated. On the PS1 this is
 * in the combat + levels overlay group (ps1/tools/ovl_plan.py matches levels_*), and nothing of it
 * links until a level calls it (--gc-sections; LV_HAS_NEW is 0 there).
 */
#include <string.h>
#include "levels_mech_move.h"

#define MM_ZIP_HANG   20        /* world units the zipline hangs the player above the floor */
#define MM_NO_PATH    255

void mm_reset(MmMove *m) { memset(m, 0, sizeof *m); }

/* ---- paths ---------------------------------------------------------------- */
static int seg_len(const uint8_t *a, const uint8_t *b)
{
    int dx = a[0] > b[0] ? a[0] - b[0] : b[0] - a[0];
    int dy = a[1] > b[1] ? a[1] - b[1] : b[1] - a[1];
    int hi = dx > dy ? dx : dy, lo = dx > dy ? dy : dx;
    return (2 * hi + lo) * (MM_U / 2);               /* (max + min / 2) cells */
}

int mm_path_index(const DoomoMapExt *x, const DoomoPath *p)
{
    return (x && p && p >= x->paths && p < x->paths + x->path_count) ? (int)(p - x->paths) : -1;
}

int mm_path_len(const DoomoMapExt *x, const DoomoPath *p)
{
    const uint8_t *pt;
    int i, n = 0;
    if (!x || !p) return 0;
    pt = &x->path_pts[p->first * 2];
    for (i = 1; i < p->count; i++) n += seg_len(pt + (i - 1) * 2, pt + i * 2);
    return n;
}

/* the distance of point i from the first point */
static int point_dist(const uint8_t *pt, int i)
{
    int k, n = 0;
    for (k = 1; k <= i; k++) n += seg_len(pt + (k - 1) * 2, pt + k * 2);
    return n;
}

/* the position on segment k of the path at `dist` (the unit vector of the segment goes to *dir) */
static void pos_on(const DoomoMapExt *x, const DoomoPath *p, int dist, int *px, int *py, int *dirx, int *diry)
{
    const uint8_t *pt = &x->path_pts[p->first * 2];
    int i, d = dist < 0 ? 0 : dist;
    *dirx = *diry = 0;
    for (i = 1; i < p->count; i++) {
        const uint8_t *a = pt + (i - 1) * 2, *b = pt + i * 2;
        int len = seg_len(a, b);
        *dirx = (b[0] > a[0]) - (b[0] < a[0]);
        *diry = (b[1] > a[1]) - (b[1] < a[1]);
        if (d <= len || i == p->count - 1) {
            if (d > len) d = len;
            *px = a[0] * 256 + (len ? ((int)b[0] - a[0]) * 256 * d / len : 0);
            *py = a[1] * 256 + (len ? ((int)b[1] - a[1]) * 256 * d / len : 0);
            return;
        }
        d -= len;
    }
    *px = pt[0] * 256;
    *py = pt[1] * 256;
}

void mm_path_pos(const DoomoMapExt *x, const DoomoPath *p, int dist, int *px256, int *py256)
{
    int dx, dy;
    if (!x || !p || p->count < 1) { *px256 = *py256 = 0; return; }
    pos_on(x, p, dist, px256, py256, &dx, &dy);
}

/* ---- gravity -------------------------------------------------------------- */
int mm_gravity_at(const DoomoMapExt *x, int cx, int cy)
{
    int g = 100, z;
    if (!x) return g;
    if (x->gravity) g = x->gravity;
    z = map_zone_at(x, cx, cy, ZONE_GRAVITY, 0);
    if (z >= 0) g = x->zones[z].arg;
    return g < 1 ? 1 : g > 250 ? 250 : g;
}

/* the same number as map_bfs_rise (core/tools/map_bfs.h): test_levels_mech proves it */
int mm_step_rise(const DoomoMapExt *x, int cx, int cy)
{
    int rise = DOOMO_STEP_UP, z;
    if (!x) return rise;
    rise = DOOMO_STEP_UP * 100 / mm_gravity_at(x, cx, cy);
    z = map_zone_at(x, cx, cy, ZONE_BOUNCE, 0);
    if (z >= 0 && x->zones[z].arg * DOOMO_HEIGHT_STEP > rise) rise = x->zones[z].arg * DOOMO_HEIGHT_STEP;
    return rise;
}

int mm_arc_height(int rise, int air, int t)
{
    if (air < 1 || rise < 1) return 0;
    if (t < 0) t = 0;
    if (t > air) t = air;
    return rise * DOOMO_HEIGHT_STEP * 4 * t * (air - t) / (air * air);
}

/* ---- ride cycles ---------------------------------------------------------- */
void mm_ride_platform(const DoomoMapExt *x, const DoomoPath *p, int clock, int speed, int *px256, int *py256)
{
    int len = mm_path_len(x, p), d;
    if (len < 1) { mm_path_pos(x, p, 0, px256, py256); return; }
    d = (clock * (speed ? speed : MM_RIDE_SPEED)) % (2 * len);
    if (d > len) d = 2 * len - d;                    /* out and back */
    mm_path_pos(x, p, d, px256, py256);
}

void mm_ride_tick(MmMove *m)
{
    if (!m->stopped) m->clock++;
}

void mm_ride_stop(MmMove *m, int stopped) { m->stopped = stopped ? 1 : 0; }

static int cell_of(int v256) { return (v256 + 128) >> 8; }     /* nearest cell */

int mm_ride_board(const DoomoMapExt *x, MmMove *m, int cx, int cy)
{
    int i;
    if (!x || m->mode != MM_ON_FOOT) return -1;
    for (i = 0; i < x->path_count; i++) {
        const DoomoPath *p = &x->paths[i];
        int px, py;
        if (p->kind != MPATH_RIDE) continue;
        mm_ride_platform(x, p, m->clock, 0, &px, &py);
        if (cell_of(px) != cx || cell_of(py) != cy) continue;
        m->mode = MM_RIDE; m->path = (uint8_t)i; m->t = 0;
        return i;
    }
    return -1;
}

/* ---- ziplines ------------------------------------------------------------- */
static int board_at_point(const DoomoMapExt *x, MmMove *m, int kind, int mode, int cx, int cy, int any_point)
{
    int i, j;
    if (!x || m->mode != MM_ON_FOOT) return -1;
    for (i = 0; i < x->path_count; i++) {
        const DoomoPath *p = &x->paths[i];
        const uint8_t *pt = &x->path_pts[p->first * 2];
        if (p->kind != kind || p->count < 2) continue;
        for (j = 0; j < (any_point ? p->count - 1 : 1); j++) {
            if (pt[j * 2] != cx || pt[j * 2 + 1] != cy) continue;
            m->mode = (uint8_t)mode; m->path = (uint8_t)i; m->t = 0; m->side = 0; m->buoy = (uint8_t)j;
            m->dist = (uint16_t)point_dist(pt, j);
            return i;
        }
    }
    return -1;
}

int mm_zip_board(const DoomoMapExt *x, MmMove *m, int cx, int cy)
{
    return board_at_point(x, m, MPATH_ZIP, MM_ZIP, cx, cy, 0);
}

int mm_raft_board(const DoomoMapExt *x, MmMove *m, int cx, int cy)
{
    return board_at_point(x, m, MPATH_RAFT, MM_RAFT, cx, cy, 1);
}

/* ---- bounce pads ---------------------------------------------------------- */
int mm_bounce_start(const DoomoMapExt *x, MmMove *m, int cx, int cy)
{
    int z, i, g, air;
    if (!x || m->mode != MM_ON_FOOT) return 0;
    z = map_zone_at(x, cx, cy, ZONE_BOUNCE, 0);
    if (z < 0) return 0;
    g = mm_gravity_at(x, cx, cy);
    air = MM_BOUNCE_AIR * 100 / g;                   /* low gravity hangs longer */
    m->air = (uint8_t)(air < 4 ? 4 : air > 255 ? 255 : air);
    m->rise = x->zones[z].arg;
    m->mode = MM_BOUNCE; m->t = 0; m->path = MM_NO_PATH;
    for (i = 0; i < x->path_count; i++) {
        const DoomoPath *p = &x->paths[i];
        const uint8_t *pt = &x->path_pts[p->first * 2];
        if (p->kind == MPATH_ARC && p->count >= 2 && pt[0] == cx && pt[1] == cy) { m->path = (uint8_t)i; break; }
    }
    return 1;
}

/* ---- tubes ---------------------------------------------------------------- */
int mm_tube_dest(const DoomoMapExt *x, const MmMove *m, int cx, int cy, int *to_x, int *to_y)
{
    int i, pick = -1, pick_to = 0;
    if (!x) return -1;
    for (i = 0; i < x->path_count; i++) {
        const DoomoPath *p = &x->paths[i];
        const uint8_t *pt = &x->path_pts[p->first * 2];
        int to;
        if (p->kind != MPATH_TUBE || p->count < 2) continue;
        if (pt[0] == cx && pt[1] == cy) to = p->count - 1;
        else if (pt[(p->count - 1) * 2] == cx && pt[(p->count - 1) * 2 + 1] == cy) to = 0;
        else continue;
        /* the first tube whose lever bit is set; with none set, the first one at the station */
        if (pick < 0) { pick = i; pick_to = to; }
        if (i < 8 && (m->lever & (1 << i))) { pick = i; pick_to = to; break; }
    }
    if (pick < 0) return -1;
    *to_x = x->path_pts[(x->paths[pick].first + pick_to) * 2];
    *to_y = x->path_pts[(x->paths[pick].first + pick_to) * 2 + 1];
    return pick;
}

int mm_tube_enter(const DoomoMapExt *x, MmMove *m, int cx, int cy)
{
    int tx, ty, i;
    if (m->mode != MM_ON_FOOT) return -1;
    i = mm_tube_dest(x, m, cx, cy, &tx, &ty);
    if (i < 0) return -1;
    m->mode = MM_TUBE; m->path = (uint8_t)i; m->t = 0; m->cx = (uint8_t)tx; m->cy = (uint8_t)ty;
    return i;
}

int mm_tube_fade(int t)
{
    int h = MM_TUBE_TICKS / 2;
    if (t < 0) t = 0;
    if (t > MM_TUBE_TICKS) t = MM_TUBE_TICKS;
    return (t <= h ? t : MM_TUBE_TICKS - t) * 255 / h;
}

void mm_tube_lever(MmMove *m, int n)
{
    if (n >= 0 && n < 8) m->lever ^= (uint8_t)(1 << n);
}

/* ---- carry ---------------------------------------------------------------- */
int mm_carry_take(MmMove *m, int item)
{
    if (m->carry || item < 1 || item > 255) return 0;
    m->carry = (uint8_t)item;
    return 1;
}

int mm_carry_drop(MmMove *m)
{
    int it = m->carry;
    m->carry = 0;
    return it;
}

int mm_carry_blocks_fire(const MmMove *m) { return m->carry != 0; }

int mm_carry_lay_tag(const DoomoMapExt *x, int cx, int cy)
{
    int z = -1;
    if (!x) return 0;
    while ((z = map_zone_at(x, cx, cy, ZONE_NONE, z + 1)) >= 0)
        if (x->zones[z].tag) return x->zones[z].tag;
    return 0;
}

/* ---- the tick ------------------------------------------------------------- */
static void end_move(MmMove *m) { m->mode = MM_ON_FOOT; m->t = 0; m->side = 0; }

int mm_tick(const DoomoMapExt *x, MmMove *m, int steer, int off, int *px256, int *py256, int *height)
{
    const DoomoPath *p;
    int len, dx, dy;
    *px256 = *py256 = -1;
    *height = 0;
    if (!x || m->mode == MM_ON_FOOT || (m->mode != MM_BOUNCE && m->path >= x->path_count)) { m->mode = MM_ON_FOOT; return 0; }
    p = &x->paths[m->path < x->path_count ? m->path : 0];
    switch (m->mode) {
    case MM_RIDE:
        mm_ride_platform(x, p, m->clock, 0, px256, py256);
        m->t++;
        if (off) { end_move(m); return 0; }
        return 1;
    case MM_ZIP:
        len = mm_path_len(x, p);
        m->dist = (uint16_t)(m->dist + MM_ZIP_SPEED);
        *height = MM_ZIP_HANG;
        if (m->dist >= len || off) {
            mm_path_pos(x, p, m->dist >= len ? len : m->dist, px256, py256);
            *height = 0;
            end_move(m);
            return 0;
        }
        mm_path_pos(x, p, m->dist, px256, py256);
        return 1;
    case MM_BOUNCE:
        m->t++;
        *height = mm_arc_height(m->rise, m->air, m->t);
        if (m->path != MM_NO_PATH && m->path < x->path_count) {
            const DoomoPath *a = &x->paths[m->path];
            mm_path_pos(x, a, mm_path_len(x, a) * m->t / m->air, px256, py256);
        }
        if (m->t >= m->air) { *height = 0; end_move(m); return 0; }
        return 1;
    case MM_TUBE:
        m->t++;
        if (m->t >= MM_TUBE_TICKS / 2) { *px256 = m->cx * 256; *py256 = m->cy * 256; }
        if (m->t >= MM_TUBE_TICKS) { end_move(m); return 0; }
        return 1;
    case MM_RAFT: {
        const uint8_t *pt = &x->path_pts[p->first * 2];
        int i, px, py;
        len = mm_path_len(x, p);
        m->dist = (uint16_t)(m->dist + MM_RAFT_SPEED);
        m->side = (int8_t)(m->side + steer * 2);
        for (i = 0; i < p->count; i++)
            if (point_dist(pt, i) <= m->dist) m->buoy = (uint8_t)i;
        if (m->side > MM_RAFT_SIDE || m->side < -MM_RAFT_SIDE) {
            /* capsized: back to the last buoy, on foot */
            *px256 = pt[m->buoy * 2] * 256;
            *py256 = pt[m->buoy * 2 + 1] * 256;
            end_move(m);
            return 0;
        }
        pos_on(x, p, m->dist >= len ? len : m->dist, &px, &py, &dx, &dy);
        *px256 = px - dy * m->side * (256 / MM_U);        /* sideways: perpendicular to the current */
        *py256 = py + dx * m->side * (256 / MM_U);
        if (m->dist >= len || off) {
            if (m->dist >= len) { *px256 = pt[(p->count - 1) * 2] * 256; *py256 = pt[(p->count - 1) * 2 + 1] * 256; }
            end_move(m);
            return 0;
        }
        return 1;
    }
    }
    end_move(m);
    return 0;
}

/* ---- the autopilot's edges ------------------------------------------------- */
static int edge_add(MmEdge *out, int n, int max, const uint8_t *pt, int to, int kind, int path, int ticks)
{
    if (n >= max) return n;
    out[n].to_x = pt[to * 2]; out[n].to_y = pt[to * 2 + 1];
    out[n].kind = (uint8_t)kind; out[n].path = (uint8_t)path;
    out[n].ticks = (uint16_t)(ticks > 65535 ? 65535 : ticks);
    return n + 1;
}

int mm_edges(const DoomoMapExt *x, int cx, int cy, MmEdge *out, int max)
{
    int i, j, n = 0;
    if (!x || !x->path_count) return 0;
    for (i = 0; i < x->path_count; i++) {
        const DoomoPath *p = &x->paths[i];
        const uint8_t *pt = &x->path_pts[p->first * 2];
        int len, last, at;
        if (p->count < 2 || p->kind == MPATH_ROUTE || p->kind == MPATH_CHASE) continue;
        len = mm_path_len(x, p);
        last = p->count - 1;
        for (j = 0; j < p->count; j++) {
            if (pt[j * 2] != cx || pt[j * 2 + 1] != cy) continue;
            at = j;
            if (p->kind == MPATH_ZIP && at == 0) n = edge_add(out, n, max, pt, last, p->kind, i, len / MM_ZIP_SPEED + 1);
            if (p->kind == MPATH_ARC && at == 0) n = edge_add(out, n, max, pt, last, p->kind, i, MM_BOUNCE_AIR);
            if (p->kind == MPATH_TUBE && (at == 0 || at == last))
                n = edge_add(out, n, max, pt, at ? 0 : last, p->kind, i, MM_TUBE_TICKS);
            if (p->kind == MPATH_RIDE || p->kind == MPATH_RAFT) {
                int t;
                for (t = p->kind == MPATH_RAFT ? at + 1 : 0; t < p->count; t++) {
                    int d = point_dist(pt, t) - point_dist(pt, at);
                    if (t == at) continue;
                    if (d < 0) d = -d;
                    /* a ride: wait for the platform (at most one full out-and-back) and ride; a raft: drift */
                    n = edge_add(out, n, max, pt, t, p->kind, i,
                                 p->kind == MPATH_RAFT ? d / MM_RAFT_SPEED + 1 : (2 * len + d) / MM_RIDE_SPEED);
                }
            }
        }
    }
    return n;
}
