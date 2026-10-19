/* overworld.c - plat_overworld(): the cutesy town as real 3D terrain.
 *
 * The same cells as the GBA's top-down view, built as geometry:
 *  - open cells are floor quads at their floor height; where a neighbour is
 *    lower, a cliff/terrace face drops down to it (south, east and west
 *    sides - the camera looks north, so north faces are never visible)
 *  - solid cells are extruded blocks (houses, hedges, fences, trees) with a
 *    roof: height = ceil_h above floor_h when set (else a default by
 *    texture), walls textured `wall`, roof textured floor_tex (else wall;
 *    building fronts get roof tiles)
 *  - beyond the map edge a gentle sea of water tiles surrounds the island
 *  - residents, the pet, the player and props are camera-facing billboards
 *    standing on their feet, with soft blob shadows
 *  - daylight (0 midnight .. 15 noon) tints every vertex colour (warm
 *    sunrise/sunset, blue night) while cells with light >= 13 keep glowing
 *    at night (street lamps, windows), then the tint overlay
 * The camera is a tilted third-person view looking north at (cx,cy),
 * smoothed so that small focus jumps glide instead of snapping.
 */
#include <string.h>
#include "r3d.h"
#include "world.h"
#include "postfx.h"

#define CELL      DOOMO_CELL_UNITS
#define OW_PITCH  (-660)          /* ~ -58 degrees                           */
#define OW_DIST   1100            /* camera distance from the focus          */
#define OW_TAN    1843            /* tan(hfov/2) = 0.45: a calm telephoto view */
#define OW_ROWS_N 9               /* rows visible north of the focus          */
#define OW_ROWS_S 6               /* rows south of the focus (+ tall blocks)  */
#define OW_HALF_MAX (7 + ((OW_ROWS_S + OW_ROWS_N) * 4) / 14)  /* widest row's half */
#define OW_SPRITE 2               /* billboard scale vs the 3D view: chibi
                                     residents ~ as tall as a cottage        */

static const DoomoMap *s_map;
static uint16_t s_time;
static uint32_t s_day;             /* daylight colour multiplier 0x00BBGGRR (128 = 1) */
static int      s_night;           /* 0..15, for lamp glow */
static int32_t  s_camx, s_camy;    /* smoothed focus, 16.16 */
static int      s_have_cam;
static int32_t  s_prevx, s_prevy;  /* last focus, for the look-ahead */
static int32_t  s_leadx, s_leady;  /* smoothed look-ahead, 16.16 */
static uint16_t s_stamp = 1;
static uint16_t s_tstamp[64];
static const TexRef *s_tref[64];

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

static inline uint32_t col_scale(uint32_t c, uint32_t f)
{
    return ((((c & 0xff00ffu) * f) >> 8) & 0xff00ffu) | ((((c & 0xff00u) * f) >> 8) & 0xff00u);
}

/* per-channel multiply by the daylight colour (128 = 1.0) */
static inline uint32_t daylit(uint32_t c)
{
    uint32_t r = ((c & 0xff) * (s_day & 0xff)) >> 7;
    uint32_t g = (((c >> 8) & 0xff) * ((s_day >> 8) & 0xff)) >> 7;
    uint32_t b = (((c >> 16) & 0xff) * ((s_day >> 16) & 0xff)) >> 7;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return r | (g << 8) | (b << 16);
}

static uint32_t corner_lit(int gx, int gy)
{
    uint32_t c;
    if (gx < 0 || gy < 0 || gx > s_map->w || gy > s_map->h)
        return daylit(0x707070);
    c = world_corner(gx, gy);
    return daylit(c);
}

/* lamps / windows: cells with light >= 13 glow when it is dark */
static uint32_t glow(const DoomoCell *c, uint32_t lit)
{
    if (c->light >= 13 && s_night > 4) {
        uint32_t add = (uint32_t)(s_night - 4) * 7;
        uint32_t r = (lit & 0xff) + add * 2, g = ((lit >> 8) & 0xff) + add + add / 2,
                 b = ((lit >> 16) & 0xff) + add / 2;
        return r3_rgb((int)r, (int)g, (int)b);
    }
    return lit;
}

static void quad(int x0, int y0, int x1, int y1, int z0, int z1, const TexRef *t,
                 uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3, int vt, int vb)
{
    /* generic quad: (x0,y0,z1) TL, (x1,y1,z1) TR, (x0,y0,z0) BL, (x1,y1,z0) BR */
    R3V q[4];
    int px = g_cam.px, py = g_cam.py, pz = g_cam.pz;
    if (!t)
        return;
    q[0].x = (int16_t)(x0 - px); q[0].y = (int16_t)(y0 - py); q[0].z = (int16_t)(z1 - pz);
    q[1].x = (int16_t)(x1 - px); q[1].y = (int16_t)(y1 - py); q[1].z = (int16_t)(z1 - pz);
    q[2].x = (int16_t)(x0 - px); q[2].y = (int16_t)(y0 - py); q[2].z = (int16_t)(z0 - pz);
    q[3].x = (int16_t)(x1 - px); q[3].y = (int16_t)(y1 - py); q[3].z = (int16_t)(z0 - pz);
    q[0].u = 0;  q[0].v = (uint8_t)vt;  q[0].c = c0;
    q[1].u = 64; q[1].v = (uint8_t)vt;  q[1].c = c1;
    q[2].u = 0;  q[2].v = (uint8_t)vb;  q[2].c = c2;
    q[3].u = 64; q[3].v = (uint8_t)vb;  q[3].c = c3;
    r3_quad(q, t, R3F_NOFOG, 0);
}

static void flat(int x0, int y0, int z, const TexRef *t, uint32_t c00, uint32_t c10,
                 uint32_t c01, uint32_t c11)
{
    R3V q[4];
    int px = g_cam.px, py = g_cam.py;
    int16_t zz = (int16_t)(z - g_cam.pz);
    if (!t)
        return;
    q[0].x = (int16_t)(x0 - px);        q[0].y = (int16_t)(y0 - py);        q[0].u = 0;  q[0].v = 0;  q[0].c = c00;
    q[1].x = (int16_t)(x0 + CELL - px); q[1].y = (int16_t)(y0 - py);        q[1].u = 64; q[1].v = 0;  q[1].c = c10;
    q[2].x = (int16_t)(x0 - px);        q[2].y = (int16_t)(y0 + CELL - py); q[2].u = 0;  q[2].v = 64; q[2].c = c01;
    q[3].x = (int16_t)(x0 + CELL - px); q[3].y = (int16_t)(y0 + CELL - py); q[3].u = 64; q[3].v = 64; q[3].c = c11;
    q[0].z = q[1].z = q[2].z = q[3].z = zz;
    r3_quad(q, t, R3F_NOFOG, 0);
}

/* vertical face split at 64-unit texture rows (top anchored) */
static void wall(int x0, int y0, int x1, int y1, int zb, int zt, const TexRef *t,
                 uint32_t cl, uint32_t cr)
{
    int s1 = zt;
    uint32_t bl = col_scale(cl, 176), br = col_scale(cr, 176);
    while (s1 > zb) {
        int s0 = s1 - 64 < zb ? zb : s1 - 64;
        int vb = s1 - s0;
        int fb = 176 + ((s0 - zb) * 80) / (zt - zb + 1), ft = 176 + ((s1 - zb) * 80) / (zt - zb + 1);
        (void)bl; (void)br;
        quad(x0, y0, x1, y1, s0, s1, t, col_scale(cl, (uint32_t)ft), col_scale(cr, (uint32_t)ft),
             col_scale(cl, (uint32_t)fb), col_scale(cr, (uint32_t)fb), 0, vb);
        s1 = s0;
    }
}

static int block_top(const DoomoCell *c)
{
    if (c->ceil_h > c->floor_h)
        return c->ceil_h * DOOMO_HEIGHT_STEP;
    switch (c->wall) {
    case TEX_HEDGE: case TEX_PICKET_FENCE: return c->floor_h * DOOMO_HEIGHT_STEP + 24;
    case TEX_TREE: return c->floor_h * DOOMO_HEIGHT_STEP + 56;
    default: return c->floor_h * DOOMO_HEIGHT_STEP + 48;
    }
}

static int roof_tex(const DoomoCell *c)
{
    if (c->floor_tex)
        return c->floor_tex;
    switch (c->wall) {
    case TEX_SHOP_FRONT: case TEX_CAFE_FRONT: case TEX_APARTMENT_FRONT:
    case TEX_WORKSHOP_FRONT: case TEX_VAPE_SHOP_FRONT: case TEX_CHURCH_WALL:
    case TEX_PASTEL_BRICK: case TEX_WOOD_WALL:
        return TEX_ROOF_TILES;
    default:
        return c->wall;
    }
}

static const DoomoCell s_sea = { 0, TEX_WATER, DOOMO_TEX_SKY, -2, 0, 12, CELL_WATER, 0 };

static const DoomoCell *cell_at(int cx, int cy)
{
    if (cx < 0 || cy < 0 || cx >= s_map->w || cy >= s_map->h)
        return &s_sea;
    return &s_map->cells[cy * s_map->w + cx];
}

static int floor_of(const DoomoCell *c, int cx, int cy)
{
    if (c == &s_sea)
        return -2 * DOOMO_HEIGHT_STEP;
    return world_floor(s_map, c, cx, cy);
}

/* lit corners: lt = the cell's two north corners, lb = its two south ones
 * (rows of corner_lit() computed once per frame by plat_overworld: every
 * corner is shared by four cells, and corner_lit was 20% of a frame) */
static void emit_cell(int cx, int cy, const uint32_t *lt, const uint32_t *lb)
{
    const DoomoCell *c = cell_at(cx, cy);
    int x0 = cx * CELL, y0 = cy * CELL;
    int fz = floor_of(c, cx, cy);
    uint32_t c00 = glow(c, lt[0]), c10 = glow(c, lt[1]);
    uint32_t c01 = glow(c, lb[0]), c11 = glow(c, lb[1]);
    g_r3stats.cells++;
    if (c->wall) {
        int top = block_top(c);
        const TexRef *wt = tex(c->wall);
        const DoomoCell *s = cell_at(cx, cy + 1), *e = cell_at(cx + 1, cy), *w = cell_at(cx - 1, cy);
        /* roof */
        flat(x0, y0, top, tex(roof_tex(c)), col_scale(c00, 250), col_scale(c10, 250),
             col_scale(c01, 250), col_scale(c11, 250));
        /* south face (toward the camera), then east / west when exposed */
        if (!s->wall || block_top(s) < top) {
            int base = s->wall ? block_top(s) : floor_of(s, cx, cy + 1);
            wall(x0, y0 + CELL, x0 + CELL, y0 + CELL, base < fz ? base : fz, top, wt,
                 col_scale(c01, 236), col_scale(c11, 236));
        }
        if (x0 + CELL < g_cam.px && (!e->wall || block_top(e) < top)) {
            int base = e->wall ? block_top(e) : floor_of(e, cx + 1, cy);
            wall(x0 + CELL, y0 + CELL, x0 + CELL, y0, base < fz ? base : fz, top, wt,
                 col_scale(c11, 200), col_scale(c10, 200));
        }
        if (x0 > g_cam.px && (!w->wall || block_top(w) < top)) {
            int base = w->wall ? block_top(w) : floor_of(w, cx - 1, cy);
            wall(x0, y0, x0, y0 + CELL, base < fz ? base : fz, top, wt,
                 col_scale(c00, 200), col_scale(c01, 200));
        }
        return;
    }
    flat(x0, y0, fz, tex(c->floor_tex ? c->floor_tex : TEX_GRASS), c00, c10, c01, c11);
    /* terrace / cliff faces down to lower neighbours */
    {
        const DoomoCell *s = cell_at(cx, cy + 1), *e = cell_at(cx + 1, cy), *w = cell_at(cx - 1, cy);
        int t = c->floor_tex ? c->floor_tex : TEX_GRASS;
        const TexRef *ct = tex(t == TEX_GRASS || t == TEX_FLOWERS ? TEX_HELL_ROCK : t);
        if (!s->wall) {
            int sz = floor_of(s, cx, cy + 1);
            if (sz < fz)
                wall(x0, y0 + CELL, x0 + CELL, y0 + CELL, sz, fz, ct,
                     col_scale(c01, 220), col_scale(c11, 220));
        }
        if (!e->wall && x0 + CELL < g_cam.px) {
            int ez = floor_of(e, cx + 1, cy);
            if (ez < fz)
                wall(x0 + CELL, y0 + CELL, x0 + CELL, y0, ez, fz, ct,
                     col_scale(c11, 190), col_scale(c10, 190));
        }
        if (!w->wall && x0 > g_cam.px) {
            int wz = floor_of(w, cx - 1, cy);
            if (wz < fz)
                wall(x0, y0, x0, y0 + CELL, wz, fz, ct, col_scale(c00, 190), col_scale(c01, 190));
        }
    }
}

static void day_colour(int d)
{
    /* noon white, warm sunrise / sunset, blue moonlit night */
    static const uint8_t k[16][3] = {
        { 52, 60, 104}, { 56, 64, 108}, { 62, 70, 112}, { 70, 76, 116},
        { 84, 82, 116}, {104, 92, 112}, {124, 104, 104}, {132, 116, 108},
        {132, 122, 114}, {132, 126, 120}, {130, 128, 124}, {130, 128, 126},
        {130, 129, 127}, {130, 130, 128}, {131, 130, 129}, {132, 132, 132} };
    if (d < 0) d = 0;
    if (d > 15) d = 15;
    s_day = (uint32_t)k[d][0] | ((uint32_t)k[d][1] << 8) | ((uint32_t)k[d][2] << 16);
    s_night = 15 - d;
}

void plat_overworld(const DoomoOverworldView *v)
{
    int i, cx, cy, ccx, ccy, row;
    int32_t ex, ey, ez, fx, fy;
    uint32_t t0 = timer_us();
    if (!v || !v->map || !v->map->w || !v->map->h)
        return;
    s_map = v->map;
    s_time = v->time;
    if (++s_stamp == 0) {
        memset(s_tstamp, 0, sizeof(s_tstamp));
        s_stamp = 1;
    }
    world_sync(s_map);
    r3_stats_reset();
    day_colour(v->daylight);

    /* smoothed focus; snap on big jumps (scene changes, teleports) */
    if (!s_have_cam || (v->cx - s_camx) > (4 * CELL << 16) || (s_camx - v->cx) > (4 * CELL << 16) ||
        (v->cy - s_camy) > (4 * CELL << 16) || (s_camy - v->cy) > (4 * CELL << 16)) {
        s_camx = v->cx;
        s_camy = v->cy;
        s_prevx = v->cx;
        s_prevy = v->cy;
        s_leadx = s_leady = 0;
        s_have_cam = 1;
    } else {
        /* look-ahead: lead the camera along the walking direction (about
         * 12 frames of motion, at most 1.25 cells), smoothed so starts and
         * stops glide instead of jerking */
        int32_t lim = (5 * CELL << 16) / 4;
        int32_t tx = (v->cx - s_prevx) * 12, ty = (v->cy - s_prevy) * 12;
        if (tx > lim) tx = lim;
        if (tx < -lim) tx = -lim;
        if (ty > lim) ty = lim;
        if (ty < -lim) ty = -lim;
        s_leadx += (tx - s_leadx) >> 3;
        s_leady += (ty - s_leady) >> 3;
        s_camx += ((v->cx + s_leadx) - s_camx) >> 2;
        s_camy += ((v->cy + s_leady) - s_camy) >> 2;
    }
    s_prevx = v->cx;
    s_prevy = v->cy;
    fx = s_camx; fy = s_camy;
    ccx = fx >> 22; ccy = fy >> 22;
    /* camera south of the focus, above it, looking north and down */
    {
        int fz = 0;
        if (ccx >= 0 && ccy >= 0 && ccx < s_map->w && ccy < s_map->h)
            fz = floor_of(cell_at(ccx, ccy), ccx, ccy);
        ex = fx;
        ey = fy + ((icos(-OW_PITCH) * OW_DIST) << 4);
        ez = ((fz + 8) << 16) + ((isin(-OW_PITCH) * OW_DIST) << 4);
    }
    r3_camera(ex, ey, ez, 3072, OW_PITCH, v->vx, v->vy, v->vw, v->vh,
              ((((v->vw / 2) << 12) / OW_TAN) * postfx_zoom_q8()) >> 8,   /* S1: GTE H zoom */
              40 * CELL, 40 * CELL, 0);
    if (!r3_begin())
        return;
    /* backdrop: sky colour behind everything (tilted view: horizon at top) */
    r3_sky(s_map->sky, v->time, 96 + (v->daylight * 32) / 15);

    /* footprint: rows north of the focus widen with distance */
    {
        /* rows narrow southward, so the south corners of one row cover the
         * north corners the next row needs: the two buffers swap */
        uint32_t lit[2][2 * OW_HALF_MAX + 2], *top = lit[0], *bot = lit[1], *sw;
        int x0 = ccx - OW_HALF_MAX, k;
        for (k = 0; k < 2 * OW_HALF_MAX + 2; k++)
            top[k] = corner_lit(x0 + k, ccy - OW_ROWS_N);
        for (row = -OW_ROWS_N; row <= OW_ROWS_S; row++) {
            /* the view frustum on the ground is a trapezoid: ~4 cells either
             * side at the bottom of the screen, ~9 at the top */
            int half = 7 + ((OW_ROWS_S - row) * 4) / 14;
            cy = ccy + row;
            for (cx = ccx - half; cx <= ccx + half + 1; cx++)
                bot[cx - x0] = corner_lit(cx, cy + 1);
            for (cx = ccx - half; cx <= ccx + half; cx++)
                emit_cell(cx, cy, top + (cx - x0), bot + (cx - x0));
            sw = top; top = bot; bot = sw;
        }
    }

    /* billboards: camera-facing, standing on their feet */
    for (i = 0; i < v->sprite_count; i++) {
        const DoomoBillboard *b = &v->sprites[i];
        int bx = b->x >> 22, by = b->y >> 22;
        uint32_t col;
        if (!b->img)
            continue;
        if (by < ccy - OW_ROWS_N - 1 || by > ccy + OW_ROWS_S + 1 || bx < ccx - 12 || bx > ccx + 12)
            continue;
        if (b->flags & BB_FULLBRIGHT)
            col = 0x808080;
        else
            col = glow(cell_at(bx, by), corner_lit(bx, by));
        {
            DoomoBillboard big = *b;
            big.scale = (uint16_t)((b->scale ? b->scale : 256) * OW_SPRITE);
            if (b->flags & BB_SHADOW) {
                int w = (b->img->w * big.scale) >> 8;
                r3_shadow(b->x, b->y, b->z, (w * 3 >> 3) + 2);
            }
            r3_billboard(&big, b->x, b->y, b->z, col,
                         R3F_FACECAM | R3F_NOFOG | ((b->flags & BB_TRANSLUCENT) ? R3F_SEMI : 0));
        }
    }
    r3_end();

    /* tint overlay (vape haze over the invaded town, night...) */
    if (v->tint != TINT_NONE && v->tint < TINT_COUNT && v->tint_level > 0) {
        int lv = v->tint_level > 15 ? 15 : v->tint_level;
        static const uint8_t tint_rgb[TINT_COUNT][3] = {
            {0, 0, 0}, {200, 20, 20}, {220, 180, 40}, {40, 200, 60}, {150, 50, 210}, {0, 0, 0} };
        TILE *t = (TILE *)gfx_alloc(sizeof(TILE));
        if (t) {
            if (v->tint == TINT_NIGHT) {
                int s = lv * 6;
                gfx_tpage(getTPage(0, 2, 0, 0));
                setTile(t);
                setColor0(t, r3_rgb(s * 2, s * 2, s));
            } else {
                gfx_tpage(getTPage(0, 1, 0, 0));
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
