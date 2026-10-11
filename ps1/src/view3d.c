/* view3d.c - plat_view3d(): the live doomo_map as true 3D polygons.
 *
 * Per frame:
 *  1. camera from the view (eye x,y,z + bob, yaw, REAL pitch rotation)
 *  2. visibility: a fan of 2D rays is cast through the grid from the eye
 *     (a DDA, but only to decide WHICH cells to draw): rays stop at solid
 *     cells and closed doors, so rooms behind walls cost nothing. The fan
 *     widens automatically when looking up/down; the cells around the eye
 *     and around the player (third person) are always included.
 *  3. every visible open cell emits its floor, its ceiling and the vertical
 *     faces on its four edges that face into it: full walls of solid
 *     neighbours, step faces where a neighbour's floor is higher, upper
 *     faces where a neighbour's ceiling is lower (doors, lintels), facades
 *     above roofed cells seen from open-sky cells. Heights come from
 *     map_floor_z()/map_ceil_z() for door and lift cells, so they move live.
 *  4. faces go through the r3d pipeline (subdivision, near clipping, fog)
 *     with gouraud lighting from the smoothed per-corner light grid
 *     (world.c), darker wall bottoms and directional wall contrast
 *  5. sky backdrop, billboards (+ blob shadows), the player's 3D model in
 *     third person, then the tint overlay.
 */
#include <string.h>
#include "r3d.h"
#include "world.h"

#define CELL      DOOMO_CELL_UNITS
#define MAXCELLS  (DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H)
#define VIEW_DIST 18             /* cells */
#define NRAYS     36             /* rays per 45 degrees of fan */

static const DoomoMap *s_map;
static uint16_t s_stamp = 1;
static uint16_t s_vis[MAXCELLS];          /* == s_stamp: visible this frame */
static uint16_t s_vlist[MAXCELLS];
static int      s_nvis;
static int      s_any_sky;
static uint16_t s_time;
static const TexRef *s_tref[64];
static uint16_t s_tstamp[64];

/* ---- helpers ------------------------------------------------------------- */
static inline int in_map(int cx, int cy)
{
    return (unsigned)cx < (unsigned)s_map->w && (unsigned)cy < (unsigned)s_map->h;
}

static inline const DoomoCell *cellp(int cx, int cy)
{
    return &s_map->cells[cy * s_map->w + cx];
}

static inline int is_sky(const DoomoCell *c) { return c->ceil_tex == DOOMO_TEX_SKY; }

static inline uint32_t col_scale(uint32_t c, uint32_t f)   /* f/256, f <= 256 */
{
    return ((((c & 0xff00ffu) * f) >> 8) & 0xff00ffu) | ((((c & 0xff00u) * f) >> 8) & 0xff00u);
}

static inline const TexRef *tex(int id)
{
    if (id <= 0 || id >= 64)
        id = 1;
    if (s_tstamp[id] != s_stamp) {
        s_tref[id] = vram_wtex(id, s_time);
        s_tstamp[id] = s_stamp;
    }
    return s_tref[id];
}

/* ---- faces ----------------------------------------------------------------- */
/* Vertical face from (ax,ay) [left] to (bx,by) [right] as seen from the
 * front, between world heights zb..zt, split at multiples of 64 so the
 * texture tiles in world space (or counted from zb for doors and lifts).
 * The lowest 64 units darken toward the floor (contact shadow). */
static void vface(int ax, int ay, int bx, int by, int zb, int zt, int texid,
                  uint32_t cl, uint32_t cr, int anchor_bottom)
{
    const TexRef *t;
    int pz = g_cam.pz, s0 = zb;
    R3V q[4];
    if (zt <= zb)
        return;
    t = tex(texid);
    if (!t)
        return;
    q[0].x = q[2].x = (int16_t)ax; q[0].y = q[2].y = (int16_t)ay;
    q[1].x = q[3].x = (int16_t)bx; q[1].y = q[3].y = (int16_t)by;
    while (s0 < zt) {
        int s1, vt, vb, hb, ht;
        if (anchor_bottom) {
            s1 = zb + (((s0 - zb) >> 6) + 1) * 64;
            if (s1 > zt) s1 = zt;
            vb = 64;
            vt = 64 - (s1 - s0);
        } else {
            int top = ((s0 >> 6) + 1) << 6;       /* next multiple of 64 */
            s1 = top < zt ? top : zt;
            vt = top - s1;
            vb = top - s0;
        }
        q[0].z = q[1].z = (int16_t)(s1 - pz);
        q[2].z = q[3].z = (int16_t)(s0 - pz);
        q[0].u = 0;  q[0].v = (uint8_t)vt;
        q[1].u = 64; q[1].v = (uint8_t)vt;
        q[2].u = 0;  q[2].v = (uint8_t)vb;
        q[3].u = 64; q[3].v = (uint8_t)vb;
        hb = s0 - zb;
        ht = s1 - zb;
        if (hb >= 64) {
            q[0].c = q[2].c = cl;
            q[1].c = q[3].c = cr;
        } else {
            uint32_t fb = 176 + ((uint32_t)hb * 80 >> 6);
            uint32_t ft = ht >= 64 ? 256 : 176 + ((uint32_t)ht * 80 >> 6);
            q[2].c = col_scale(cl, fb);
            q[3].c = col_scale(cr, fb);
            q[0].c = ft >= 256 ? cl : col_scale(cl, ft);
            q[1].c = ft >= 256 ? cr : col_scale(cr, ft);
        }
        r3_quad(q, t, 0, 0);
        s0 = s1;
    }
}

static void hface(int x0, int y0, int z, int texid, uint32_t c00, uint32_t c10,
                  uint32_t c01, uint32_t c11)
{
    const TexRef *t = tex(texid);
    R3V q[4];
    int16_t zz = (int16_t)(z - g_cam.pz);
    if (!t)
        return;
    q[0].x = (int16_t)x0;          q[0].y = (int16_t)y0;          q[0].u = 0;  q[0].v = 0;  q[0].c = c00;
    q[1].x = (int16_t)(x0 + CELL); q[1].y = (int16_t)y0;          q[1].u = 64; q[1].v = 0;  q[1].c = c10;
    q[2].x = (int16_t)x0;          q[2].y = (int16_t)(y0 + CELL); q[2].u = 0;  q[2].v = 64; q[2].c = c01;
    q[3].x = (int16_t)(x0 + CELL); q[3].y = (int16_t)(y0 + CELL); q[3].u = 64; q[3].v = 64; q[3].c = c11;
    q[0].z = q[1].z = q[2].z = q[3].z = zz;
    r3_quad(q, t, 0, 0);
}

/* texture for faces under a lowered ceiling (doors, lintels, facades):
 * the neighbour's ceil_tex, like the reference raycaster */
static int upper_tex(const DoomoCell *n)
{
    return is_sky(n) ? n->floor_tex : n->ceil_tex;
}

static void edge(int nx, int ny, int ax, int ay, int bx, int by,
                 uint32_t cl, uint32_t cr, int fz, int cz, int sky)
{
    const DoomoCell *n;
    if (!in_map(nx, ny)) {
        /* outside the map: solid, as tall as the sky allows */
        vface(ax, ay, bx, by, fz, sky ? g_world_sky_top : cz, 1, cl, cr, 0);
        return;
    }
    n = cellp(nx, ny);
    if (n->wall) {
        /* under a roof the wall closes the room; in open air the block is
         * as tall as its ceil_h (else 256 above our floor) */
        int top = cz;
        if (sky)
            top = n->ceil_h > n->floor_h ? n->ceil_h * DOOMO_HEIGHT_STEP : fz + 256;
        vface(ax, ay, bx, by, fz, top, n->wall, cl, cr, 0);
        return;
    }
    {
        int nfz = world_floor(s_map, n, nx, ny), ncz, nsky;
        if (nfz > fz)
            vface(ax, ay, bx, by, fz, sky || nfz < cz ? nfz : cz,
                  n->floor_tex ? n->floor_tex : 1, cl, cr, (n->flags & CELL_LIFT) ? 1 : 0);
        nsky = is_sky(n);
        if (nsky)
            return;
        ncz = world_ceil(s_map, n, nx, ny);
        if (!sky) {
            if (ncz < cz)
                vface(ax, ay, bx, by, ncz > fz ? ncz : fz, cz, upper_tex(n), cl, cr,
                      (n->flags & CELL_DOOR) ? 1 : 0);
        } else if (ncz < g_world_sky_top) {
            /* facade above a roofed opening seen from the open air */
            vface(ax, ay, bx, by, ncz > fz ? ncz : fz, g_world_sky_top, upper_tex(n), cl, cr, 0);
        }
    }
}

static void emit_cell(int cx, int cy)
{
    const DoomoCell *c = cellp(cx, cy);
    int rx = cx * CELL - g_cam.px, ry = cy * CELL - g_cam.py;
    int fz, cz, sky, ez = g_cam.wz >> 16;
    uint32_t c00, c10, c01, c11;
    g_r3stats.cells++;
    if (c->wall) {
        /* roof of a block standing in open air, visible from above */
        int top = g_world_wtop[cy * s_map->w + cx];
        if (ez > top) {
            uint32_t k = world_corner(cx, cy);
            hface(rx, ry, top, c->floor_tex ? c->floor_tex : c->wall, k, k, k, k);
        }
        return;
    }
    fz = world_floor(s_map, c, cx, cy);
    cz = world_ceil(s_map, c, cx, cy);
    sky = is_sky(c);
    if (sky)
        s_any_sky = 1;
    c00 = world_corner(cx, cy);
    c10 = world_corner(cx + 1, cy);
    c01 = world_corner(cx, cy + 1);
    c11 = world_corner(cx + 1, cy + 1);
    if (ez > fz)
        hface(rx, ry, fz, c->floor_tex, c00, c10, c01, c11);
    if (!sky && ez < cz && cz > fz)
        hface(rx, ry, cz, c->ceil_tex, col_scale(c00, 200), col_scale(c10, 200),
              col_scale(c01, 200), col_scale(c11, 200));
    /* edges, each only when the eye is on this cell's side; walls facing
     * east/west are shaded a little darker (directional contrast) */
    if (ry < 0)          /* north edge, seen looking north: west..east */
        edge(cx, cy - 1, rx, ry, rx + CELL, ry, c00, c10, fz, cz, sky);
    if (rx + CELL > 0)   /* east edge, looking east: north..south */
        edge(cx + 1, cy, rx + CELL, ry, rx + CELL, ry + CELL,
             col_scale(c10, 216), col_scale(c11, 216), fz, cz, sky);
    if (ry + CELL > 0)   /* south edge, looking south: east..west */
        edge(cx, cy + 1, rx + CELL, ry + CELL, rx, ry + CELL,
             col_scale(c11, 238), col_scale(c01, 238), fz, cz, sky);
    if (rx < 0)          /* west edge, looking west: south..north */
        edge(cx - 1, cy, rx, ry + CELL, rx, ry,
             col_scale(c01, 216), col_scale(c00, 216), fz, cz, sky);
}

/* ---- visibility ------------------------------------------------------------- */
static inline void mark_idx(int i)
{
    if (s_vis[i] != s_stamp) {
        s_vis[i] = s_stamp;
        s_vlist[s_nvis++] = (uint16_t)i;
    }
}

static inline void mark(int cx, int cy)
{
    if (in_map(cx, cy))
        mark_idx(cy * s_map->w + cx);
}

static int door_closed(int cx, int cy)
{
    const DoomoCell *c = cellp(cx, cy);
    return world_ceil(s_map, c, cx, cy) <= world_floor(s_map, c, cx, cy);
}

/* DDA from (x,y) (16.16 world) along angle a; marks visited cells until a
 * solid cell (marked too: its roof may be visible) or maxcells. */
static int s_eye_z;

static void cast(int32_t x, int32_t y, int a, int maxcells)
{
    const DoomoCell *cells = s_map->cells;
    int open_air = 0;
    int w = s_map->w, h = s_map->h;
    int32_t dx = icos(a), dy = isin(a);          /* 4.12 */
    int32_t px = x >> 6, py = y >> 6;            /* 16.16 in cell units */
    int mx = px >> 16, my = py >> 16;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1, syw = dy < 0 ? -w : w;
    int32_t adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    int32_t ddx = adx > 16 ? (int32_t)((1 << 28) / adx) : (256 << 16);
    int32_t ddy = ady > 16 ? (int32_t)((1 << 28) / ady) : (256 << 16);
    int32_t fx = px & 0xffff, fy = py & 0xffff;
    int32_t sdx = ((dx < 0 ? fx : 0x10000 - fx) >> 4) * (ddx >> 12);
    int32_t sdy = ((dy < 0 ? fy : 0x10000 - fy) >> 4) * (ddy >> 12);
    int32_t lim = (int32_t)maxcells << 16;
    int i = my * w + mx;
    if ((unsigned)mx < (unsigned)w && (unsigned)my < (unsigned)h)
        open_air = cells[i].ceil_tex == DOOMO_TEX_SKY;
    for (;;) {
        if (sdx < sdy) {
            if (sdx > lim) break;
            sdx += ddx;
            mx += sx;
            i += sx;
            if ((unsigned)mx >= (unsigned)w) break;
        } else {
            if (sdy > lim) break;
            sdy += ddy;
            my += sy;
            i += syw;
            if ((unsigned)my >= (unsigned)h) break;
        }
        if (s_vis[i] != s_stamp) {
            s_vis[i] = s_stamp;
            s_vlist[s_nvis++] = (uint16_t)i;
        }
        if (cells[i].wall) {
            /* a block in the open air lower than the eye: look over it */
            if (!open_air || g_world_wtop[i] >= s_eye_z)
                break;
        } else {
            if ((cells[i].flags & CELL_DOOR) && door_closed(mx, my))
                break;
            open_air = cells[i].ceil_tex == DOOMO_TEX_SKY;
        }
    }
}

static void visibility(int32_t ex, int32_t ey, int ang, int pitch_a, int32_t fx, int32_t fy,
                       int th, int tv)
{
    int i, dx, dy, half, cx, cy, n;
    int sp = isin(pitch_a < 0 ? -pitch_a : pitch_a), cp = icos(pitch_a);
    /* horizontal half-angle of the frustum projected on the ground:
     * atan(tan(h) / (cos p - tan(v) sin|p|)); th, tv = tan(h), tan(v) 4.12 */
    int f = cp - ((sp * tv) >> 12);                /* 4.12 */
    if (f < 300) {
        half = 2048;
    } else {
        int lo = 0, hi = 1024;
        while (hi - lo > 4) {
            int mid = (lo + hi) >> 1;
            if (isin(mid) * f >= icos(mid) * th) hi = mid; else lo = mid;
        }
        half = hi + 40;
    }
    cx = ex >> 22; cy = ey >> 22;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            mark(cx + dx, cy + dy);
    if (fx || fy) {
        cx = fx >> 22; cy = fy >> 22;
        for (dy = -1; dy <= 1; dy++)
            for (dx = -1; dx <= 1; dx++)
                mark(cx + dx, cy + dy);
    }
    n = (half * NRAYS) / 512;
    if (n > NRAYS * 4) n = NRAYS * 4;
    if (!in_map(ex >> 22, ey >> 22))
        return;
    if (n < 8) n = 8;
    for (i = 0; i <= n; i++) {
        int a = ang - half + (2 * half * i) / n;
        cast(ex, ey, a & 4095, VIEW_DIST);
    }
}

/* ---- player model hook (model.c) --------------------------------------------- */
int model_player_draw(const DoomoView3D *v, const DoomoBillboard *bb, uint32_t light);

static uint32_t sprite_light(const DoomoBillboard *b, int *visible)
{
    int cx = b->x >> 22, cy = b->y >> 22, l;
    *visible = 1;
    if (in_map(cx, cy)) {
        const DoomoCell *c = cellp(cx, cy);
        if (s_vis[cy * s_map->w + cx] != s_stamp)
            *visible = 0;
        l = (c->light ? c->light : s_map->ambient) + b->light;
    } else {
        l = 15;
    }
    if (l > 15) l = 15;
    l = 40 + ((l * 376) >> 6);                    /* 40 .. 128 */
    return (uint32_t)l * 0x010101u;
}

/* ---- entry ---------------------------------------------------------------------- */
void plat_view3d(const DoomoView3D *v)
{
    int i, pitch_a, fogn, th, tv;
    int32_t ez, pfx = 0, pfy = 0;
    uint32_t t0 = timer_us();
    if (!v || !v->map || !v->map->w || !v->map->h)
        return;
    s_map = v->map;
    s_time = v->time;
    if (++s_stamp == 0) {
        memset(s_vis, 0, sizeof(s_vis));
        memset(s_tstamp, 0, sizeof(s_tstamp));
        s_stamp = 1;
    }
    world_sync(s_map);
    r3_stats_reset();

    /* camera: pitch is a real rotation (+-256 -> +-40 degrees); the field
     * of view follows OPT_FOV like the reference renderer:
     * tan(fov/2) = 0.55 / 0.66 / 0.79 */
    pitch_a = (v->pitch * 455) / 256;
    if (pitch_a > 470) pitch_a = 470;
    if (pitch_a < -470) pitch_a = -470;
    ez = v->z + ((int32_t)v->bob << 16);
    s_eye_z = ez >> 16;
    fogn = s_map->kind == MAP_OVERWORLD ? 10 * CELL : 4 * CELL;
    {
        static const uint16_t k_tan[3] = { 2253, 2703, 3236 };     /* 4.12 */
        int fov = core_opt(OPT_FOV, 1);
        th = k_tan[fov < 0 || fov > 2 ? 1 : fov];
        tv = (th * v->vh) / (v->vw ? v->vw : 1);
        r3_camera(v->x, v->y, ez, v->angle, pitch_a, v->vx, v->vy, v->vw, v->vh,
                  ((v->vw / 2) << 12) / th, VIEW_DIST * CELL, fogn, 0);
    }
    if (!r3_begin())
        return;

    if (v->camera == CAM_THIRD_PERSON && v->player_sprite >= 0 &&
        v->player_sprite < v->sprite_count) {
        pfx = v->sprites[v->player_sprite].x;
        pfy = v->sprites[v->player_sprite].y;
    }
    s_nvis = 0;
    g_r3stats.t_setup = timer_us() - t0;
    visibility(v->x, v->y, v->angle, pitch_a, pfx, pfy, th, tv);
    g_r3stats.t_vis = timer_us() - t0;
    s_any_sky = 0;
    {
        int w = s_map->w;
        for (i = 0; i < s_nvis; i++) {
            int idx = s_vlist[i], cy = idx / w;
            emit_cell(idx - cy * w, cy);
        }
    }
    g_r3stats.t_cells = timer_us() - t0;
    if (s_any_sky)
        r3_sky(s_map->sky, v->time, 128);

    /* billboards */
    for (i = 0; i < v->sprite_count; i++) {
        const DoomoBillboard *b = &v->sprites[i];
        int visible;
        uint32_t col;
        if (!b->img && i != v->player_sprite)
            continue;
        col = sprite_light(b, &visible);
        if (!visible)
            continue;
        if (b->flags & BB_SHADOW) {
            int w = b->img ? ((b->img->w * (b->scale ? b->scale : 256)) >> 8) : 24;
            r3_shadow(b->x, b->y, b->z, (w * 3 >> 3) + 2);
        }
        if (i == v->player_sprite && v->camera == CAM_THIRD_PERSON &&
            model_player_draw(v, b, col))
            continue;
        if (b->img) {
            int fl = (b->flags & BB_FULLBRIGHT) ? R3F_NOFOG : 0;
            if (i == v->player_sprite) {
                /* never hide the view: see-through when the camera is close */
                int32_t dx = (b->x - v->x) >> 16, dy = (b->y - v->y) >> 16;
                if (dx * dx + dy * dy < 40 * 40)
                    fl |= R3F_SEMI;
            }
            r3_billboard(b, b->x, b->y, b->z, (b->flags & BB_FULLBRIGHT) ? 0x808080 : col, fl);
        }
    }
    r3_end();

    /* tint overlay over the viewport (damage flash, holy light, haze...) */
    if (v->tint != TINT_NONE && v->tint < TINT_COUNT && v->tint_level > 0) {
        int lv = v->tint_level > 15 ? 15 : v->tint_level;
        static const uint8_t tint_rgb[TINT_COUNT][3] = {
            {0, 0, 0}, {200, 20, 20}, {220, 180, 40}, {40, 200, 60}, {150, 50, 210}, {0, 0, 0} };
        TILE *t = (TILE *)gfx_alloc(sizeof(TILE));
        if (t) {
            if (v->tint == TINT_NIGHT) {
                int s = lv * 6;
                gfx_tpage(getTPage(0, 2, 0, 0));        /* subtract */
                setTile(t);
                setColor0(t, r3_rgb(s * 2, s * 2, s));
            } else {
                gfx_tpage(getTPage(0, 1, 0, 0));        /* add */
                setTile(t);
                setColor0(t, r3_rgb(tint_rgb[v->tint][0] * lv / 24, tint_rgb[v->tint][1] * lv / 24,
                                    tint_rgb[v->tint][2] * lv / 24));
            }
            setSemiTrans(t, 1);
            setXY0(t, v->vx, v->vy);
            setWH(t, v->vw, v->vh);
            gfx_chain(t);
        }
    }
    g_r3stats.cpu_us = timer_us() - t0;
}

/* exported for the engine test HUD */
int view3d_visible_cells(void) { return s_nvis; }
