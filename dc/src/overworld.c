/* overworld.c - plat_overworld() on the PVR (DC7; owner: dreamcast).
 *
 * The cutesy town as real 3D terrain (the PS1's design, ps1/src/overworld.c),
 * drawn by the PVR with a depth buffer, in the native runner's colours:
 *  - open cells are floor quads at their floor height; where a neighbour
 *    is lower, a terrace face drops to it (south, east and west sides: the
 *    camera looks north, so north faces are never seen)
 *  - solid cells are blocks (houses, hedges, fences, trees, the Wall) with
 *    a roof: height ceil_h when above floor_h, else a default by wall
 *    texture; walls textured `wall` (one quad, the texture repeating up its
 *    height), roofs floor_tex (else roof tiles for building fronts, else the
 *    wall); rows beyond the ground's edges add only the blocks tall enough
 *    to rise into view
 *  - beyond the map edge a sea of water tiles surrounds the island
 *  - residents, the pet, the player and props are billboards: rectangles
 *    parallel to the screen (they never lean), the native runner's size,
 *    standing on their feet, with soft blob shadows; their depth is that of
 *    an upright plane through the feet, so a house in front hides a
 *    resident's legs pixel by pixel; drawn back to front among themselves
 *  - light: each grid corner's colour is the average of the open cells
 *    around it (lava, portals, water and exits tint it, a cell's light under
 *    8 darkens it as on the native runner), times the daylight the native
 *    runner uses (colormap level 5 + d * 10 / 15 as a per-channel factor,
 *    and its night lean to blue as the night tint's fit); lamps (cells with
 *    light >= 13) light soft warm pools at night (the offset colour); the
 *    far rows fade toward the sky's horizon colour; the view tint (vape
 *    haze, the invaded town's red pulse) is the native tintmap fitted per
 *    channel (c * a + b), applied to every vertex
 * The camera: a tilted third-person view looking north at (cx, cy), the
 * PS1's numbers (58 degrees down, 1100 units back, tan(hfov/2) = 0.45),
 * with no smoothing of its own (the core's camera already glides), so the
 * player stays where the core's overlays expect it.
 *
 * Portable C (dc3d.h); the host tests build it with -DDC2D_HOST, where the
 * entry point is dc2d_overworld().
 */
#include <string.h>
#include "dc3d.h"
#include "doomo/doomo.h"
#include "doomo/map.h"
#include "doomo/textures.h"
#include "doomo/palette.h"

#define CELL      DOOMO_CELL_UNITS
#define OW_COSP   0.5298036f      /* pitch 660/4096 of a turn (58.0 degrees) */
#define OW_SINP   0.8481203f
#define OW_DIST   1100.0f         /* camera distance from the focus           */
#define OW_TAN    0.45f           /* tan(hfov / 2): a calm telephoto view     */
#define OW_ROWS_N 22              /* the most rows north of the focus drawn:
                                     the ground shows about 9, the Wall (up to
                                     480 units tall) rises into view from 12
                                     more                                     */
#define OW_ROWS_S 9               /* ... and south of it (about 6 + tall blocks) */
#define OW_SPRITE 3.0f            /* world units per texel of a billboard at
                                     scale 256: about one screen texel per
                                     logical pixel at the focus, the native
                                     runner's size (the PS1 draws them at 2) */
#define OW_FADE0  (OW_DIST * 1.12f)   /* depth where the far fade starts      */
#define OW_FADE1  (OW_DIST * 1.60f)   /* ... and reaches OW_FADE_MAX          */
#define OW_FADE_MAX 0.45f

/* ---- the map's corner light (a per-map cache, refreshed a row at a time) ---------- */
#define GW (DOOMO_MAP_MAX_W + 1)
#define GH (DOOMO_MAP_MAX_H + 1)
static const DoomoMap    *s_map;
static const DoomoMapDef *s_def;
static uint8_t  s_w, s_h;
static uint16_t s_last_time;
static int      s_row;
static uint32_t s_corner[GW * GH];          /* 0x00BBGGRR, 128 = 1.0 (the PS1's scale) */
static int      s_maxtop;                    /* the tallest block's top (world units) */

static uint32_t cell_glow(const DoomoCell *c, int *add)
{
    *add = 0;
    if (c->flags & CELL_LAVA)   { *add = 6; return 0x6eaaff; }   /* 0xBBGGRR */
    if (c->flags & CELL_PORTAL) { *add = 5; return 0xff96e6; }
    if (c->flags & CELL_WATER)  { return 0xffd2aa; }
    if (c->flags & CELL_EXIT)   { *add = 3; return 0xc8ffc8; }
    return 0xffffff;
}

/* A grid corner's light: the average over the open cells around it of
 * each cell's light colour (lava, portals, water and exits tint it) times
 * its brightness as the native runner sees it: a cell's light below 8
 * darkens it (by (8 - light) / 2 colormap levels there, about 6% a step
 * here), 8 and up (or 0: unset) leaves it alone, and a glowing cell's own
 * boost brightens it a little. A corner with no open cell around it (inside
 * a block) is neutral. */
static void corner_row(int gy)
{
    const DoomoMap *m = s_map;
    int gx, w = m->w, h = m->h;
    for (gx = 0; gx <= w; gx++) {
        uint32_t r = 0, g = 0, b = 0, n = 0;
        int dy, dx;
        for (dy = -1; dy <= 0; dy++) {
            int cy = gy + dy;
            if (cy < 0 || cy >= h) continue;
            for (dx = -1; dx <= 0; dx++) {
                int cx = gx + dx, add;
                const DoomoCell *c;
                uint32_t t, br;
                if (cx < 0 || cx >= w) continue;
                c = &m->cells[cy * w + cx];
                if (c->wall) continue;
                t = cell_glow(c, &add);
                br = 128;
                if (c->light && c->light < 8) br -= (uint32_t)(8 - c->light) * 8;
                br += (uint32_t)add * 4;
                r += ((t & 0xff) * br) >> 8;
                g += (((t >> 8) & 0xff) * br) >> 8;
                b += (((t >> 16) & 0xff) * br) >> 8;
                n++;
            }
        }
        /* 128 = 1.0 (0xff * 128 >> 8 = 127: white light, unchanged) */
        s_corner[gy * GW + gx] = n ? (r / n) | ((g / n) << 8) | ((b / n) << 16) : 0x7f7f7fu;
    }
}

static void world_sync(const DoomoMap *m)
{
    int gy;
    if (m != s_map || m->def != s_def || m->w != s_w || m->h != s_h || m->time < s_last_time) {
        s_map = m;
        s_def = m->def;
        s_w = m->w;
        s_h = m->h;
        for (gy = 0; gy <= m->h; gy++) corner_row(gy);
        s_row = 0;
        s_maxtop = 0;
        for (gy = 0; gy < m->w * m->h; gy++) {
            const DoomoCell *c = &m->cells[gy];
            int t = c->ceil_h > c->floor_h ? c->ceil_h * DOOMO_HEIGHT_STEP : c->floor_h * DOOMO_HEIGHT_STEP + 56;
            if (c->wall && t > s_maxtop) s_maxtop = t;
        }
    } else if (m->time & 1) {
        /* one row every other tick: lights the game changes show up within
         * about 4 s on the largest map */
        corner_row(s_row);
        s_row = s_row >= m->h ? 0 : s_row + 1;
    }
    s_last_time = m->time;
}

/* a cell of the map, or the sea around it */
static const DoomoCell s_sea = { 0, TEX_WATER, DOOMO_TEX_SKY, -2, 0, 12, CELL_WATER, 0 };

static const DoomoCell *cell_at(int cx, int cy)
{
    if (cx < 0 || cy < 0 || cx >= s_map->w || cy >= s_map->h) return &s_sea;
    return &s_map->cells[cy * s_map->w + cx];
}

/* ---- the frame's state ------------------------------------------------------------- */
typedef struct Col { float r, g, b; } Col;

static Dc3dCam  cam;
static uint16_t s_time;
static Col      s_day;                       /* daylight multiplier (1 = noon white) */
static int      s_night;                     /* 0..15 */
static Col      s_fog;                       /* the far fade's colour (0..255) */
static Col      s_tmul, s_tadd;              /* the view tint: c * mul + add */
static float    s_camx;
static int      s_tslot[TEX_COUNT];
static uint16_t s_tstamp[TEX_COUNT], s_stamp = 1;

/* tintmap fits, per tint and channel: out = a * in + b (in 0..255), least
 * squares over the palette (rms 11..18 for the colour tints) */
static float t_a[TINT_COUNT][3], t_b[TINT_COUNT][3];
static int   t_ready;

static int x5to8(int v) { return (v << 3) | (v >> 2); }
static int pchan(uint8_t i, int k)
{
    uint16_t c = doomo_palette[i];
    return x5to8(k == 0 ? (c & 31) : k == 1 ? ((c >> 5) & 31) : ((c >> 10) & 31));
}

static void tint_fit(void)
{
    int t, k, i;
    for (t = 0; t < TINT_COUNT; t++)
        for (k = 0; k < 3; k++) {
            float sx = 0, sy = 0, sxx = 0, sxy = 0, n = 255.0f, den;
            for (i = 1; i < 256; i++) {
                float x = (float)pchan((uint8_t)i, k), y = (float)pchan(doomo_tintmap[t][i], k);
                sx += x; sy += y; sxx += x * x; sxy += x * y;
            }
            den = n * sxx - sx * sx;
            t_a[t][k] = den > 0.0f ? (n * sxy - sx * sy) / den : 1.0f;
            t_b[t][k] = (sy - t_a[t][k] * sx) / n;
            if (t == TINT_NONE) { t_a[t][k] = 1.0f; t_b[t][k] = 0.0f; }
            if (t_b[t][k] < 0.0f) t_b[t][k] = 0.0f;   /* the offset colour only adds */
        }
    t_ready = 1;
}

static int tex(int id)
{
    if (id <= 0 || id >= TEX_COUNT) id = TEX_GRASS;
    if (s_tstamp[id] != s_stamp) {
        const DoomoImage *img = texture_image(id, s_time);
        s_tslot[id] = img ? dc2d_tex_image(img) : -1;
        s_tstamp[id] = s_stamp;
    }
    return s_tslot[id];
}

/* Daylight as the native runner shades it: daylight 0..15 picks colormap
 * level 5 + d * 10 / 15 (15 at noon: unchanged), here the level's fitted
 * per-channel factor (pal.c, the one a MUL shade uses); before 8 the
 * native runner also leans the picture blue, a dither of the night
 * tintmap over (8 - d) / 8 of the pixels: here that tint's per-channel fit
 * at that strength (s_nmul, s_nadd), applied under the view tint. */
static Col s_nmul, s_nadd;
static void day_colour(int d)
{
    uint32_t f;
    float fn;
    d = DOOMO_CLAMP(d, 0, 15);
    f = dc2d_shade_argb(5 + d * 10 / 15);
    s_day.r = (float)((f >> 16) & 255u) / 255.0f;
    s_day.g = (float)((f >> 8) & 255u) / 255.0f;
    s_day.b = (float)(f & 255u) / 255.0f;
    s_night = 15 - d;
    fn = d < 8 ? (float)((8 - d) * 2) / 16.0f : 0.0f;
    s_nmul.r = 1.0f - fn + fn * t_a[TINT_NIGHT][0]; s_nadd.r = fn * t_b[TINT_NIGHT][0];
    s_nmul.g = 1.0f - fn + fn * t_a[TINT_NIGHT][1]; s_nadd.g = fn * t_b[TINT_NIGHT][1];
    s_nmul.b = 1.0f - fn + fn * t_a[TINT_NIGHT][2]; s_nadd.b = fn * t_b[TINT_NIGHT][2];
}

/* a corner's light (0..1, may pass 1 a little) */
static Col corner_lit(int gx, int gy)
{
    Col c;
    uint32_t v = (gx < 0 || gy < 0 || gx > s_map->w || gy > s_map->h) ? 0x707070u : s_corner[gy * GW + gx];
    c.r = (float)(v & 0xff) / 128.0f * s_day.r;
    c.g = (float)((v >> 8) & 0xff) / 128.0f * s_day.g;
    c.b = (float)((v >> 16) & 0xff) / 128.0f * s_day.b;
    return c;
}

static Col col_scale(Col c, float f) { c.r *= f; c.g *= f; c.b *= f; return c; }

/* lamps and windows: cells with light >= 13 glow when it is dark (offset) */
static Col s_glowc;                          /* a lamp's glow tonight (offset, 0..255) */
static Col glow_of(const DoomoCell *c)
{
    Col o = {0.0f, 0.0f, 0.0f};
    return (c->light >= 13 && s_night > 4) ? s_glowc : o;
}

static uint32_t to_argb(float a, Col c)
{
    int r = (int)(c.r * 255.0f + 0.5f), g = (int)(c.g * 255.0f + 0.5f), b = (int)(c.b * 255.0f + 0.5f);
    int aa = (int)(a * 255.0f + 0.5f);
    r = DOOMO_CLAMP(r, 0, 255); g = DOOMO_CLAMP(g, 0, 255); b = DOOMO_CLAMP(b, 0, 255);
    aa = DOOMO_CLAMP(aa, 0, 255);
    return ((uint32_t)aa << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static uint32_t to_rgb(Col c)          /* the offset colour, already 0..255 */
{
    int r = (int)(c.r + 0.5f), g = (int)(c.g + 0.5f), b = (int)(c.b + 0.5f);
    r = DOOMO_CLAMP(r, 0, 255); g = DOOMO_CLAMP(g, 0, 255); b = DOOMO_CLAMP(b, 0, 255);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* the full shading of one point (billboards): light (multiplier), glow
 * (offset), the far fade by the camera depth of (x, y, z), then the view
 * tint; packed into *argb / *oargb */
static void shade_point(float y, float z, Col light, Col glow, float alpha, uint32_t *argb, uint32_t *oargb)
{
    float zc = (y - cam.ey) * cam.fy + (z - cam.ez) * cam.fz;      /* the camera looks along y-z */
    float f = (zc - OW_FADE0) / (OW_FADE1 - OW_FADE0);
    f = f < 0.0f ? 0.0f : f > 1.0f ? OW_FADE_MAX : f * OW_FADE_MAX;
    light.r *= (1.0f - f) * s_tmul.r; light.g *= (1.0f - f) * s_tmul.g; light.b *= (1.0f - f) * s_tmul.b;
    glow.r = (glow.r * (1.0f - f) + s_fog.r * f) * s_tmul.r + s_tadd.r;
    glow.g = (glow.g * (1.0f - f) + s_fog.g * f) * s_tmul.g + s_tadd.g;
    glow.b = (glow.b * (1.0f - f) + s_fog.b * f) * s_tmul.b + s_tadd.b;
    *argb = to_argb(alpha, light);
    *oargb = to_rgb(glow);
}

/* The terrain's grid corners, shaded once a frame: the corner light with
 * the far fade (taken at height 0: a cell's floor height moves it by a few
 * percent at most) and the tint folded in, and the offset colour (fade,
 * tint and the lamps' glow). At night a corner touching a lamp cell (light
 * >= 13) glows fully, so a lamp lights a soft pool over its own cell that
 * fades out across its neighbours (Gouraud), not a hard square. Faces
 * scale `lit` by their own factor; floors use the packed colours as they
 * are. */
typedef struct CornerShade {
    Col      lit, off;
    uint32_t argb, oargb;
    uint16_t stamp;
} CornerShade;
#define CG_W 48
#define CG_H (OW_ROWS_N + OW_ROWS_S + 3)          /* rows ccy - OW_ROWS_N - 1 .. */
static CornerShade cg[CG_H][CG_W], cg_spare;
static int cg_x0, cg_y0;

static const CornerShade *corner(int gx, int gy)
{
    int i = gy - cg_y0, j = gx - cg_x0;
    CornerShade *c = (i >= 0 && j >= 0 && i < CG_H && j < CG_W) ? &cg[i][j] : &cg_spare;
    if (c == &cg_spare || c->stamp != s_stamp) {
        Col l = corner_lit(gx, gy);
        float zc = ((float)(gy * CELL) - cam.ey) * cam.fy - cam.ez * cam.fz, f, k;
        f = (zc - OW_FADE0) / (OW_FADE1 - OW_FADE0);
        f = f < 0.0f ? 0.0f : f > 1.0f ? OW_FADE_MAX : f * OW_FADE_MAX;
        k = 1.0f - f;
        c->lit.r = l.r * k * s_tmul.r; c->lit.g = l.g * k * s_tmul.g; c->lit.b = l.b * k * s_tmul.b;
        c->off.r = s_fog.r * f * s_tmul.r + s_tadd.r;
        c->off.g = s_fog.g * f * s_tmul.g + s_tadd.g;
        c->off.b = s_fog.b * f * s_tmul.b + s_tadd.b;
        if (s_night > 4 && (cell_at(gx - 1, gy - 1)->light >= 13 || cell_at(gx, gy - 1)->light >= 13 ||
                            cell_at(gx - 1, gy)->light >= 13 || cell_at(gx, gy)->light >= 13)) {
            c->off.r += s_glowc.r * k * s_tmul.r;
            c->off.g += s_glowc.g * k * s_tmul.g;
            c->off.b += s_glowc.b * k * s_tmul.b;
        }
        c->argb = to_argb(1.0f, c->lit);
        c->oargb = to_rgb(c->off);
        c->stamp = s_stamp;
    }
    return c;
}

/* a terrain vertex at a corner: its light scaled by k (1: as cached) */
static void corner_vertex(Dc3dIn *o, const CornerShade *c, float k, float x, float y, float z,
                          float u, float v)
{
    o->x = x; o->y = y; o->z = z; o->u = u; o->v = v;
    o->argb = k == 1.0f ? c->argb : to_argb(1.0f, col_scale(c->lit, k));
    o->oargb = c->oargb;
}

/* ---- cells --------------------------------------------------------------------------- */

static int floor_of(const DoomoCell *c, int cx, int cy)
{
    if (c == &s_sea) return -2 * DOOMO_HEIGHT_STEP;
    return (c->flags & (CELL_DOOR | CELL_LIFT)) ? map_floor_z(s_map, cx, cy) : c->floor_h * DOOMO_HEIGHT_STEP;
}

static int block_top(const DoomoCell *c)
{
    if (c->ceil_h > c->floor_h) return c->ceil_h * DOOMO_HEIGHT_STEP;
    switch (c->wall) {
    case TEX_HEDGE: case TEX_PICKET_FENCE: return c->floor_h * DOOMO_HEIGHT_STEP + 24;
    case TEX_TREE: return c->floor_h * DOOMO_HEIGHT_STEP + 56;
    default: return c->floor_h * DOOMO_HEIGHT_STEP + 48;
    }
}

static int roof_tex(const DoomoCell *c)
{
    if (c->floor_tex) return c->floor_tex;
    switch (c->wall) {
    case TEX_SHOP_FRONT: case TEX_CAFE_FRONT: case TEX_APARTMENT_FRONT:
    case TEX_WORKSHOP_FRONT: case TEX_VAPE_SHOP_FRONT: case TEX_CHURCH_WALL:
    case TEX_PASTEL_BRICK: case TEX_WOOD_WALL:
        return TEX_ROOF_TILES;
    default:
        return c->wall;
    }
}

/* a horizontal quad over cell (cx, cy) at height z, its light scaled by k */
static void flat(int cx, int cy, int z, int t, float k)
{
    Dc3dIn q[4];
    float X0 = (float)(cx * CELL), Y0 = (float)(cy * CELL), X1 = X0 + CELL, Y1 = Y0 + CELL, Z = (float)z;
    if (t < 0) return;
    corner_vertex(&q[0], corner(cx, cy), k, X0, Y0, Z, 0.0f, 0.0f);
    corner_vertex(&q[1], corner(cx + 1, cy), k, X1, Y0, Z, 1.0f, 0.0f);
    corner_vertex(&q[2], corner(cx + 1, cy + 1), k, X1, Y1, Z, 1.0f, 1.0f);
    corner_vertex(&q[3], corner(cx, cy + 1), k, X0, Y1, Z, 0.0f, 1.0f);
    dc3d_add(&cam, DC3D_OPAQUE, t, q, 4);
}

/* a vertical face along the grid edge from corner (ga) to corner (gb),
 * heights zb..zt, the texture anchored at the top and repeating down;
 * light k at the top, 0.69 k at the foot (the PS1's 176..256 ramp) */
static void wall(int gax, int gay, int gbx, int gby, int zb, int zt, int t, float k)
{
    Dc3dIn q[4];
    CornerShade sa;
    const CornerShade *ca = corner(gax, gay), *cb;
    float xa = (float)(gax * CELL), ya = (float)(gay * CELL), xb = (float)(gbx * CELL), yb = (float)(gby * CELL);
    float vb = (float)(zt - zb) / 64.0f;
    if (t < 0 || zt <= zb) return;
    if (ca == &cg_spare) { sa = cg_spare; ca = &sa; }   /* both off the cache: keep a's light */
    cb = corner(gbx, gby);
    corner_vertex(&q[0], ca, k, xa, ya, (float)zt, 0.0f, 0.0f);
    corner_vertex(&q[1], cb, k, xb, yb, (float)zt, 1.0f, 0.0f);
    corner_vertex(&q[2], cb, k * 0.69f, xb, yb, (float)zb, 1.0f, vb);
    corner_vertex(&q[3], ca, k * 0.69f, xa, ya, (float)zb, 0.0f, vb);
    dc3d_add(&cam, DC3D_OPAQUE, t, q, 4);
}

static void emit_cell(int cx, int cy)
{
    const DoomoCell *c = cell_at(cx, cy);
    int x0 = cx * CELL;
    int fz = floor_of(c, cx, cy);
    const DoomoCell *s = cell_at(cx, cy + 1), *e = cell_at(cx + 1, cy), *w = cell_at(cx - 1, cy);
    if (c->wall) {
        int top = block_top(c), wt = tex(c->wall);
        flat(cx, cy, top, tex(roof_tex(c)), 0.98f);
        /* south face (toward the camera), then east / west when exposed */
        if (!s->wall || block_top(s) < top) {
            int base = s->wall ? block_top(s) : floor_of(s, cx, cy + 1);
            wall(cx, cy + 1, cx + 1, cy + 1, base < fz ? base : fz, top, wt, 0.92f);
        }
        if ((float)(x0 + CELL) < s_camx && (!e->wall || block_top(e) < top)) {
            int base = e->wall ? block_top(e) : floor_of(e, cx + 1, cy);
            wall(cx + 1, cy + 1, cx + 1, cy, base < fz ? base : fz, top, wt, 0.78f);
        }
        if ((float)x0 > s_camx && (!w->wall || block_top(w) < top)) {
            int base = w->wall ? block_top(w) : floor_of(w, cx - 1, cy);
            wall(cx, cy, cx, cy + 1, base < fz ? base : fz, top, wt, 0.78f);
        }
        return;
    }
    flat(cx, cy, fz, tex(c->floor_tex ? c->floor_tex : TEX_GRASS), 1.0f);
    /* terrace / cliff faces down to lower neighbours */
    {
        int t = c->floor_tex ? c->floor_tex : TEX_GRASS;
        int ct = tex(t == TEX_GRASS || t == TEX_FLOWERS ? TEX_HELL_ROCK : t);
        if (!s->wall) {
            int sz = floor_of(s, cx, cy + 1);
            if (sz < fz) wall(cx, cy + 1, cx + 1, cy + 1, sz, fz, ct, 0.86f);
        }
        if (!e->wall && (float)(x0 + CELL) < s_camx) {
            int ez = floor_of(e, cx + 1, cy);
            if (ez < fz) wall(cx + 1, cy + 1, cx + 1, cy, ez, fz, ct, 0.74f);
        }
        if (!w->wall && (float)x0 > s_camx) {
            int wz = floor_of(w, cx - 1, cy);
            if (wz < fz) wall(cx, cy, cx, cy + 1, wz, fz, ct, 0.74f);
        }
    }
}

/* ---- billboards ---------------------------------------------------------------------- */
#define OW_MAX_SPRITES 512
static int16_t order[OW_MAX_SPRITES];

/* the light at a world point: the four corners around it, interpolated */
static Col point_lit(float x, float y)
{
    int gx = (int)(x / CELL), gy = (int)(y / CELL);
    float fx, fy;
    Col a, b, c, d, r;
    if (x < 0.0f) gx--;
    if (y < 0.0f) gy--;
    fx = x / CELL - (float)gx;
    fy = y / CELL - (float)gy;
    a = corner_lit(gx, gy); b = corner_lit(gx + 1, gy);
    c = corner_lit(gx, gy + 1); d = corner_lit(gx + 1, gy + 1);
    r.r = (a.r * (1 - fx) + b.r * fx) * (1 - fy) + (c.r * (1 - fx) + d.r * fx) * fy;
    r.g = (a.g * (1 - fx) + b.g * fx) * (1 - fy) + (c.g * (1 - fx) + d.g * fx) * fy;
    r.b = (a.b * (1 - fx) + b.b * fx) * (1 - fy) + (c.b * (1 - fx) + d.b * fx) * fy;
    return r;
}

/* a soft blob on the ground under the feet: one quad with the blob
 * texture (dc3d.h), black at 44% in the middle */
static void shadow(float x, float y, float z, float rad)
{
    Dc3dIn q[4];
    int k;
    memset(q, 0, sizeof(q));
    for (k = 0; k < 4; k++) {
        q[k].x = x + ((k == 1 || k == 2) ? rad : -rad);
        q[k].y = y + (k >= 2 ? rad : -rad);
        q[k].z = z;
        q[k].u = (k == 1 || k == 2) ? 1.0f : 0.0f;
        q[k].v = k >= 2 ? 1.0f : 0.0f;
        q[k].argb = 0x70000000u;
    }
    dc3d_add(&cam, DC3D_SPRITE, (int)DC3D_TEX_BLOB, q, 4);
}

/* A billboard: a rectangle parallel to the screen (so it never leans, as
 * the PS1's camera-facing sprites), standing on its feet, the native
 * runner's size (OW_SPRITE world units per texel, about one screen texel
 * per logical pixel at the focus). Its DEPTH is that of an upright plane
 * through the feet (y = feet y): each vertex gets the z where its pixel's
 * ray meets that plane, so a house in front hides the legs and the wall
 * behind never cuts the head, pixel by pixel. The quad is cut into
 * OW_BB_STRIPS rows so the texture's perspective step between rows (the
 * z varies over the rectangle) stays well under a texel. Heights above the
 * floor (jumps, emote bubbles, the quest marks) lift the rectangle in
 * screen space as the native runner lifts them (z / 4 pixels, relative to
 * the sprite): x and the base row are projected at the ground point, so a
 * lifted sprite stays over its feet at the screen's sides (projecting the
 * lifted point itself, which is nearer the camera, pushed it outward by
 * about 13 % of its distance from the centre). */
#define OW_BB_STRIPS 4
static void billboard(const DoomoBillboard *b)
{
    const DoomoImage *img = b->img;
    const DcTex *tx;
    DcVert *v;
    Col lit, glow = {0.0f, 0.0f, 0.0f};
    float x = (float)b->x / 65536.0f, y = (float)b->y / 65536.0f, z = (float)b->z / 65536.0f;
    float scale = (float)(b->scale ? b->scale : 256) * OW_SPRITE / 256.0f;
    float w = (float)img->w * scale, h = (float)img->h * scale;
    float u0, u1, v1, alpha = (b->flags & BB_TRANSLUCENT) ? 0.5f : 1.0f, zf, zc, sx0, sx1, sy0, sy1;
    float dy = y - cam.ey, top, ny, lift = 0.0f;
    uint32_t argb, oargb;
    int slot, k, bx = (int)(x / CELL) - (x < 0.0f ? 1 : 0), by = (int)(y / CELL) - (y < 0.0f ? 1 : 0);
    zf = (float)floor_of(cell_at(bx, by), bx, by);
    if (z > zf) {                       /* lifted: in the screen's plane, at the ground's depth */
        lift = (z - zf) * (OW_SPRITE / 4.0f);
        z = zf;
    }
    if ((b->flags & BB_SHADOW) && img->w)
        shadow(x, y, zf + 1.5f, (float)(((int)w * 3 >> 3) + 2));
    /* the ground point in camera space: the rectangle sits at its depth */
    zc = dy * cam.fy + (z - cam.ez) * cam.fz;
    if (zc < 4.0f * DC3D_ZNEAR || dy >= 0.0f) return;
    sx0 = cam.cx + cam.focal * (x - w * 0.5f - cam.ex) / zc;
    sx1 = cam.cx + cam.focal * (x + w * 0.5f - cam.ex) / zc;
    sy1 = cam.cy + cam.focal * (dy * cam.dy + (z - cam.ez) * cam.dz - lift) / zc;   /* the feet's row */
    sy0 = sy1 - cam.focal * h / zc;                                                  /* the head's  */
    if (sx1 < cam.vx0 || sx0 > cam.vx1 || sy1 < cam.vy0 || sy0 > cam.vy1) { dc3d_stats.culled++; return; }
    slot = dc2d_tex_image(img);
    if (slot < 0) return;
    tx = dc2d_tex(slot);
    if (b->flags & BB_FULLBRIGHT) {
        lit.r = lit.g = lit.b = 1.0f;
    } else {
        float boost = (float)(b->light > 15 ? 15 : b->light) / 30.0f;
        lit = point_lit(x, y);
        lit.r += boost; lit.g += boost; lit.b += boost;
        glow = glow_of(cell_at(bx, by));
    }
    u1 = (float)img->w / (float)tx->tw;
    v1 = (float)img->h / (float)tx->th;
    u0 = 0.0f;
    if (b->flags & BB_FLIPX) { u0 = u1; u1 = 0.0f; }
    /* one colour for the whole sprite, shaded at its middle (a lift in the
     * screen's plane is lift / cos(pitch) units up in the world) */
    shade_point(y, z + lift / OW_COSP + h * 0.5f, lit, glow, alpha, &argb, &oargb);
    v = dc3d_poly(DC3D_SPRITE, slot, 2 * (OW_BB_STRIPS + 1));
    if (!v) return;
    /* a strip down the rectangle: left, right per row, top row first; the
     * depth of the upright plane y = feet along each row's ray */
    top = sy0;
    for (k = 0; k <= OW_BB_STRIPS; k++) {
        float t = (float)k / OW_BB_STRIPS, sy = top + (sy1 - top) * t;
        float bslope = (sy - cam.cy) / cam.focal;              /* ray: forward + bslope * down */
        float zr = dy / (cam.fy + bslope * cam.dy), zz;
        if (zr < DC3D_ZNEAR) zr = DC3D_ZNEAR;
        zz = DC3D_ZK / zr;
        ny = v1 * t;
        v[2 * k].x = sx0;     v[2 * k].y = sy; v[2 * k].z = zz;
        v[2 * k].u = u0;      v[2 * k].v = ny;
        v[2 * k + 1].x = sx1; v[2 * k + 1].y = sy; v[2 * k + 1].z = zz;
        v[2 * k + 1].u = u1;  v[2 * k + 1].v = ny;
        v[2 * k].argb = v[2 * k + 1].argb = argb;
        v[2 * k].oargb = v[2 * k + 1].oargb = oargb;
    }
}

static void billboards(const DoomoOverworldView *v, int ccx, int ccy)
{
    int i, n = 0;
    for (i = 0; i < v->sprite_count && n < OW_MAX_SPRITES; i++) {
        const DoomoBillboard *b = &v->sprites[i];
        int bx = b->x >> 22, by = b->y >> 22, j;
        if (!b->img || !b->img->pix || !b->img->w || !b->img->h) continue;
        if (by < ccy - 13 || by > ccy + 8 || bx < ccx - 16 || bx > ccx + 16) continue;   /* far off */
        /* back to front: north first, then west to east, then call order */
        for (j = n; j > 0; j--) {
            const DoomoBillboard *a = &v->sprites[order[j - 1]];
            if (a->y < b->y || (a->y == b->y && a->x <= b->x)) break;
            order[j] = order[j - 1];
        }
        order[j] = (int16_t)i;
        n++;
    }
    for (i = 0; i < n; i++) billboard(&v->sprites[order[i]]);
}

/* ---- the view ---------------------------------------------------------------------------- */
void DC2D_FN(overworld)(const DoomoOverworldView *v)
{
    int vx, vy, vw, vh, ccx, ccy, row, cx, poly0;
    float fx, fy, fz;
    uint32_t t0;
    if (!v || !v->map || !v->map->w || !v->map->h) return;
    vx = v->vw > 0 ? v->vx : 0;
    vy = v->vh > 0 ? v->vy : 0;
    vw = v->vw > 0 ? v->vw : DC2D_LW;
    vh = v->vh > 0 ? v->vh : DC2D_LH;
    t0 = dc3d_be_us();
    if (!t_ready) tint_fit();
    s_time = v->time;
    if (++s_stamp == 0) {               /* (every 36 minutes) no stale stamps */
        memset(s_tstamp, 0, sizeof(s_tstamp));
        memset(cg, 0, sizeof(cg));
        s_stamp = 1;
    }
    world_sync(v->map);
    day_colour(v->daylight);

    /* the far fade's colour: the sky's horizon, in the daylight */
    {
        uint8_t r, g, b, hz = sky_horizon(v->map->sky < SKY_COUNT ? v->map->sky : SKY_PASTEL_DAY);
        palette_rgb(hz, &r, &g, &b);
        s_fog.r = (float)r * (s_day.r > 1.0f ? 1.0f : s_day.r);
        s_fog.g = (float)g * (s_day.g > 1.0f ? 1.0f : s_day.g);
        s_fog.b = (float)b * (s_day.b > 1.0f ? 1.0f : s_day.b);
        /* the backdrop behind the terrain (seen only past the sea) */
        DC2D_FN(rect)(vx, vy, vw, vh, hz);
    }
    /* the view tint: lerp(c, tint(c), level / 16) with tint(c) = a c + b,
     * over the night's lean to blue (day_colour): c -> view(night(c)) */
    {
        int t = v->tint < TINT_COUNT ? v->tint : TINT_NONE;
        float f = t == TINT_NONE ? 0.0f : (float)DOOMO_CLAMP(v->tint_level, 0, 16) / 16.0f;
        Col m, a;
        m.r = 1.0f - f + f * t_a[t][0]; a.r = f * t_b[t][0];
        m.g = 1.0f - f + f * t_a[t][1]; a.g = f * t_b[t][1];
        m.b = 1.0f - f + f * t_a[t][2]; a.b = f * t_b[t][2];
        s_tmul.r = s_nmul.r * m.r; s_tadd.r = s_nadd.r * m.r + a.r;
        s_tmul.g = s_nmul.g * m.g; s_tadd.g = s_nadd.g * m.g + a.g;
        s_tmul.b = s_nmul.b * m.b; s_tadd.b = s_nadd.b * m.b + a.b;
    }

    /* the camera: south of the focus, above it, looking north and down */
    fx = (float)v->cx / 65536.0f;
    fy = (float)v->cy / 65536.0f;
    ccx = v->cx >> 22;
    ccy = v->cy >> 22;
    fz = 0.0f;
    if (ccx >= 0 && ccy >= 0 && ccx < v->map->w && ccy < v->map->h)
        fz = (float)floor_of(cell_at(ccx, ccy), ccx, ccy);
    memset(&cam, 0, sizeof(cam));
    cam.ex = fx;
    cam.ey = fy + OW_COSP * OW_DIST;
    cam.ez = fz + 8.0f + OW_SINP * OW_DIST;
    cam.rx = 1.0f;                                   /* right: east */
    cam.dy = OW_SINP; cam.dz = -OW_COSP;             /* down on screen */
    cam.fy = -OW_COSP; cam.fz = -OW_SINP;            /* forward: north and down */
    cam.vx0 = (float)(vx * DC2D_SCALE);
    cam.vy0 = (float)(vy * DC2D_SCALE);
    cam.vx1 = (float)((vx + vw) * DC2D_SCALE);
    cam.vy1 = (float)((vy + vh) * DC2D_SCALE);
    cam.cx = (cam.vx0 + cam.vx1) * 0.5f;
    cam.cy = (cam.vy0 + cam.vy1) * 0.5f;
    cam.focal = (cam.vx1 - cam.vx0) * 0.5f / OW_TAN;
    s_camx = cam.ex;

    /* tonight's lamp glow (offset colour), and the corner cache's origin */
    {
        float add = s_night > 4 ? (float)((s_night - 4) * 7) : 0.0f;
        s_glowc.r = add * 1.2f; s_glowc.g = add * 0.9f; s_glowc.b = add * 0.3f;
    }
    cg_x0 = ccx - CG_W / 2;
    cg_y0 = ccy - OW_ROWS_N - 1;

    if (!dc3d_seg_begin(vx, vy, vw, vh)) return;
    /* terrain: the footprint widens with distance (the frustum on the
     * ground is a trapezoid); grouped by texture afterwards */
    poly0 = dc3d_batch(dc3d_cur_slot())->np;
    {
        /* the rows the ground can show in: where the top edge's ray meets
         * the lowest floor (the sea) and the bottom edge's ray the highest
         * one; north of that only blocks can rise into view (2 rows) */
        float zlow = (float)(-4 * DOOMO_HEIGHT_STEP) - cam.ez, zhigh = 64.0f - cam.ez;
        float st = (cam.vy0 - cam.cy) / cam.focal, sb = (cam.vy1 - cam.cy) / cam.focal;
        float yn = cam.ey + zlow * (st * cam.fz - cam.dz) / (cam.dy - st * cam.fy);
        float ys = cam.ey + zhigh * (sb * cam.fz - cam.dz) / (cam.dy - sb * cam.fy);
        int rn = (int)(yn / CELL) - (yn < 0.0f ? 1 : 0), rs = (int)(ys / CELL) - (ys < 0.0f ? 1 : 0);
        /* beyond those rows only blocks can show: a block h units tall
         * rises into the top edge from about 1.25 h / 64 rows further
         * north (the top ray falls at 39 degrees), and into the bottom edge
         * from h / 260 rows further south (it falls at 77 degrees) */
        int r0 = DOOMO_MAX(rn - (s_maxtop * 5 / 4) / CELL - 1, ccy - OW_ROWS_N);
        int r1 = DOOMO_MIN(rs + s_maxtop / 260 + 1, ccy + OW_ROWS_S);
        for (row = r0; row <= r1; row++) {
            /* the frustum's half-width at the row's far edge, at the
             * lowest floor, plus half a cell: every cell that can show */
            float zc = ((float)(row * CELL) - cam.ey) * cam.fy + zlow * cam.fz;
            float half = zc * (cam.vx1 - cam.cx) / cam.focal + CELL * 0.5f;
            int x0 = (int)((cam.ex - half) / CELL) - 1, x1 = (int)((cam.ex + half) / CELL);
            for (cx = x0; cx <= x1; cx++)
                if ((row >= rn && row <= rs) || cell_at(cx, row)->wall) emit_cell(cx, row);
        }
    }
    dc3d_sort_from(poly0);
    /* billboards and their shadows, back to front, over the terrain */
    if (v->sprites && v->sprite_count > 0) billboards(v, ccx, ccy);
    dc3d_seg_end();
    {
        uint32_t us = dc3d_be_us() - t0;
        dc3d_stats.us_last = us;
        dc3d_stats.us_sum += us;
        dc3d_stats.us_n++;
        if (us > dc3d_stats.us_max) dc3d_stats.us_max = us;
    }
}

#ifdef DC2D_HOST
const Dc3dCam *dc2d_overworld_cam(void) { return &cam; }
#endif
