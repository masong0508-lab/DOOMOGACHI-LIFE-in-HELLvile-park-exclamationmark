/* view3d.c - plat_view3d() on the PVR (DC6 part 1; owner: dreamcast).
 *
 * The first-person / third-person view of the live doomo_map (Hell Breach
 * levels, the arena, combat, the level select's preview) as real polygons
 * with a depth buffer, through the 3D batches (dc3d.h): a SEGMENT inside
 * the 2D list's painter's order, so the HUD the core draws after it is on
 * top. The picture is the reference raycaster's (core/native/raycast.c):
 * the same projection, the same faces, textures, light and fade.
 *
 *  camera      the eye the core gives (x, y, z + bob; first and third
 *              person alike: the core places the eye), looking level along
 *              the yaw; the pitch is the reference's VERTICAL SHEAR (the
 *              horizon moves up or down the screen, a lens shift), not a
 *              rotation: walls stay upright, and the core's own projection
 *              of world points (combat_hud_project: the lock-on marks, the
 *              hit numbers) lands on the polygons to the pixel. Focal
 *              length and horizon use the reference's integers.
 *  visibility  a fan of rays through the grid (one for every 2 logical
 *              columns, a DDA as the reference casts per column), each
 *              with the reference's open window as a pair of slopes: a
 *              ray stops where the window closes (a wall under a roof, a
 *              closed door, a tall step), goes on over blocks standing
 *              under the open sky, and marks every cell it sees into. A
 *              ray passing closer to a grid corner than the rays are apart
 *              there marks the cells around that corner too, so no cell
 *              can show between two rays.
 *  cells       every marked open cell: its floor, its ceiling (not under
 *              the sky), and on each edge the eye is inside of: the wall of
 *              a solid neighbour (to the ceiling; under the sky to the
 *              block's ceil_h, else 256 units up), the step face up to a
 *              higher floor, the upper face down to a lower ceiling (doors,
 *              lintels), the facade over a roofed cell seen from under the
 *              sky. Heights come from map_floor_z() / map_ceil_z(), so doors
 *              and lifts move live (their faces keep the texture fixed to
 *              the moving edge). A marked block under the sky: its roof when
 *              the eye is above it, and the faces of taller blocks behind
 *              it. One quad a face, the texture repeating up its height.
 *  light       the cell's light (0: the map's ambient) minus the distance
 *              fade (one level per 128 units of depth); faces across the y
 *              axis one level darker, step and upper faces one more, as the
 *              reference. A face's texture is a copy already remapped
 *              through the colormap of its brightest corner's whole level
 *              (the reference's own colours: the colormap walks down each
 *              colour's ramp, which no multiply gives), and Gouraud does
 *              the last fraction of a level down to each corner (the ratio
 *              of the levels' fitted factors, pal.c). The fade is
 *              scaled to the view distance (dc_view3d_set_dist: the hook of
 *              part 3's governor and OPT_DRAW_DIST), so a shorter view
 *              ends in the dark.
 *  sky         the map's sky as a backdrop behind everything (the far
 *              depth): the reference's mapping, the column's angle across,
 *              the row against the horizon down. Views that see no cell
 *              under the sky get the reference's dark stone instead.
 *  billboards  after the opaque world, far to near: rectangles parallel to
 *              the screen at their depth (the reference's size and place),
 *              tested against the walls' depth per pixel, never writing it;
 *              BB_SHADOW a soft blob on the floor. Part 2 of DC6 owns their
 *              final look (the player's model, the weapon, the effects).
 *  tint        the view tint (TINT_*, tint_level) as the overworld applies
 *              it: the native tintmap's per-channel fit on every vertex.
 *  viewport    honoured: a view smaller than the screen (the level select)
 *              is cut at its edges in camera space (Dc3dCam.clip).
 *
 * Portable C (dc3d.h); the host tests build it with -DDC2D_HOST, where the
 * entry point is dc2d_view3d_draw().
 */
#include <string.h>
#include <math.h>
#include "dc3d.h"
#include "doomo/doomo.h"
#include "doomo/map.h"
#include "doomo/textures.h"
#include "doomo/palette.h"
#include "doomo/fixmath.h"
#include "doomo/town.h"

#define CELL       DOOMO_CELL_UNITS
#define MAXCELLS   (DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H)
#define V3D_ZNEAR  4.0f              /* the player's radius is 14: a wall the eye
                                        leans on is never cut where it shows   */
#define V3D_FAR_Z  0.002f            /* the backdrop's depth: behind every polygon
                                        (64 / 3072 = 0.02 at 48 cells), in front
                                        of the 2D prims' (DC2D_Z)              */
#define V3D_FADE_UNITS 128.0f        /* a light level per this much depth      */
#define V3D_SKY_COLS 16
#define V3D_SKY_UID 0x00BF0000u      /* const-image uids of the sky frames     */
#define V3D_MAX_SPRITES 256
#define V3D_BIG   1.0e9f

typedef struct Col { float r, g, b; } Col;

static int s_view_dist = DC_V3D_DIST_MAX;

void dc_view3d_set_dist(int cells)
{
    s_view_dist = DOOMO_CLAMP(cells, DC_V3D_DIST_MIN, DC_V3D_DIST_MAX);
}
int dc_view3d_dist(void) { return s_view_dist; }

/* ---- the frame's state ------------------------------------------------------------- */
static const DoomoMap *M;
static Dc3dCam  cam;
static float    s_ex, s_ey, s_ez;            /* the eye, world units */
static float    s_dx, s_dy;                  /* forward (unit) */
static float    s_fade;                      /* light levels per unit of depth */
static uint16_t s_time;
static Col      s_fit[DOOMO_LIGHT_LEVELS];   /* a level's fitted colour factor (pal.c), 0..1 */
static uint32_t s_oargb;                     /* the tint's offset colour */
static Col      s_tmul, s_tadd;
static int      s_any_sky;
static int      s_backdrop = 1;

static uint16_t s_vis[MAXCELLS], s_stamp;
static uint16_t s_vlist[MAXCELLS];
static int      s_nvis;
static int16_t  s_tslot[TEX_COUNT][DOOMO_LIGHT_LEVELS];   /* a texture at a light level */
static uint16_t s_tstamp[TEX_COUNT][DOOMO_LIGHT_LEVELS];

static int in_map(int cx, int cy) { return cx >= 0 && cy >= 0 && cx < M->w && cy < M->h; }
static int is_sky(const DoomoCell *c) { return c->ceil_tex == DOOMO_TEX_SKY; }

static int light_at(const DoomoCell *c)
{
    int l = c->light ? c->light : (M->ambient ? M->ambient : 12);
    return DOOMO_CLAMP(l, 0, 15);
}

/* a block's top under the open sky, seen from a floor at fz */
static int block_top(const DoomoCell *n, int nx, int ny, int fz)
{
    return n->ceil_h > n->floor_h ? map_ceil_z(M, nx, ny) : fz + 256;
}
/* ... and the height of its roof */
static int roof_top(const DoomoCell *c, int cx, int cy)
{
    return block_top(c, cx, cy, c->floor_h * DOOMO_HEIGHT_STEP);
}

/* the reference's roof of a roofed open cell seen from under the sky: the
 * tallest explicit top of its solid neighbours, else 127 = none */
static int roof_of(int cx, int cy)
{
    static const int8_t nb[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    int k, roof = 127;
    for (k = 0; k < 4; k++) {
        int nx = cx + nb[k][0], ny = cy + nb[k][1];
        const DoomoCell *o;
        if (!in_map(nx, ny)) continue;
        o = &M->cells[ny * M->w + nx];
        if (o->wall && o->ceil_h > o->floor_h && (roof == 127 || o->ceil_h > roof)) roof = o->ceil_h;
    }
    return roof;
}

/* ---- colours ------------------------------------------------------------------------- */
static uint32_t pack_rgb(float r, float g, float b)
{
    int ir = (int)(r + 0.5f), ig = (int)(g + 0.5f), ib = (int)(b + 0.5f);
    ir = DOOMO_CLAMP(ir, 0, 255); ig = DOOMO_CLAMP(ig, 0, 255); ib = DOOMO_CLAMP(ib, 0, 255);
    return ((uint32_t)ir << 16) | ((uint32_t)ig << 8) | (uint32_t)ib;
}

/* the light level at depth zc: the reference steps one level down every
 * 128 units (fade = depth / 128, rounded down, on average half a level
 * less than the straight line): here the line through the steps' middles */
static float level_at(int light, float zc)
{
    float f = zc * s_fade - 0.5f, l = (float)light - (f > 0.0f ? f : 0.0f);
    return l < 1.0f ? 1.0f : l > 15.0f ? 15.0f : l;
}

/* a level's fitted colour factor, between the whole levels */
static Col fit_at(float l)
{
    int i = (int)l, j;
    float f = l - (float)i;
    Col c;
    if (i >= DOOMO_LIGHT_LEVELS - 1) return s_fit[DOOMO_LIGHT_LEVELS - 1];
    if (i < 0) return s_fit[0];
    j = i + 1;
    c.r = s_fit[i].r + (s_fit[j].r - s_fit[i].r) * f;
    c.g = s_fit[i].g + (s_fit[j].g - s_fit[i].g) * f;
    c.b = s_fit[i].b + (s_fit[j].b - s_fit[i].b) * f;
    return c;
}

/* What a face is drawn with: the texture ALREADY at whole light level
 * `lv` (a copy remapped through doomo_colormap[lv], texcache.c: the
 * reference's own colours, which no multiply gives: the colormap walks
 * down each colour's ramp), or no texture and the flat colour the
 * reference uses (texture_avg) at that level. A vertex at level l <= lv
 * then only needs the small step from lv down to l: the ratio of the two
 * levels' fitted factors (lit). */
typedef struct Face {
    int      slot;                           /* texture slot, -1: none */
    int      lv;                             /* the whole level the texels are at */
    uint32_t base;                           /* 0xFFFFFF, or the flat colour (0xRRGGBB) */
} Face;

static void face_tex(Face *f, int id, float lmax)
{
    int lv = (int)ceilf(lmax - 0.001f);
    lv = DOOMO_CLAMP(lv, 1, DOOMO_LIGHT_LEVELS - 1);
    f->lv = lv;
    f->base = 0xFFFFFFu;
    if (id > 0 && id < TEX_COUNT) {
        if (s_tstamp[id][lv] != s_stamp) {
            int slot;
            s_tstamp[id][lv] = s_stamp;
            if (s_tstamp[id][DOOMO_LIGHT_LEVELS - 1] != s_stamp || lv == DOOMO_LIGHT_LEVELS - 1) {
                const DoomoImage *img = texture_image(id, s_time);
                s_tslot[id][DOOMO_LIGHT_LEVELS - 1] = (int16_t)(img ? dc2d_tex_image(img) : -1);
                s_tstamp[id][DOOMO_LIGHT_LEVELS - 1] = s_stamp;
            }
            slot = s_tslot[id][DOOMO_LIGHT_LEVELS - 1];
            if (lv < DOOMO_LIGHT_LEVELS - 1) {
                /* no room for the copy: the plain texture, darkened by the fit alone */
                int r = slot >= 0 ? dc2d_tex_remap(slot, lv, doomo_colormap[lv]) : -1;
                s_tslot[id][lv] = (int16_t)(r >= 0 ? r : -2 - slot);
            }
        }
        if (s_tslot[id][lv] >= 0) { f->slot = s_tslot[id][lv]; return; }
        if (s_tslot[id][lv] < -1) { f->slot = -2 - s_tslot[id][lv]; f->lv = DOOMO_LIGHT_LEVELS - 1; return; }
    }
    {
        uint8_t r, g, b;
        palette_rgb(doomo_colormap[lv][texture_avg(id)], &r, &g, &b);
        f->base = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        if (f->base == 0xFFFFFFu) f->base = 0xFEFEFEu;
        f->slot = -1;
    }
}

/* the vertex colour of a face at level l (the tint folded in) */
static uint32_t lit(const Face *f, float l)
{
    Col c = fit_at(l < (float)f->lv ? l : (float)f->lv), d = s_fit[f->lv];
    c.r = c.r / d.r * 255.0f * s_tmul.r;
    c.g = c.g / d.g * 255.0f * s_tmul.g;
    c.b = c.b / d.b * 255.0f * s_tmul.b;
    if (f->base != 0xFFFFFFu) {
        c.r = c.r * (float)((f->base >> 16) & 255u) / 255.0f;
        c.g = c.g * (float)((f->base >> 8) & 255u) / 255.0f;
        c.b = c.b * (float)(f->base & 255u) / 255.0f;
    }
    return 0xFF000000u | pack_rgb(c.r, c.g, c.b);
}

/* a palette colour through the tint, as a vertex colour (no texture) */
static uint32_t tinted_pal(uint8_t idx)
{
    uint8_t r, g, b;
    palette_rgb(idx, &r, &g, &b);
    return 0xFF000000u | pack_rgb((float)r * s_tmul.r + s_tadd.r, (float)g * s_tmul.g + s_tadd.g,
                                  (float)b * s_tmul.b + s_tadd.b);
}

static float depth_of(float x, float y) { return (x - s_ex) * s_dx + (y - s_ey) * s_dy; }

/* ---- faces ----------------------------------------------------------------------------- */
/* a horizontal quad over cell (cx, cy) at height z */
static void flat(int cx, int cy, int z, int texid, int light)
{
    Dc3dIn q[4];
    Face f;
    float X0 = (float)(cx * CELL), Y0 = (float)(cy * CELL), l[4], lmax = 0.0f;
    int k;
    for (k = 0; k < 4; k++) {
        int right = k == 1 || k == 2, low = k >= 2;
        q[k].x = X0 + (right ? CELL : 0);
        q[k].y = Y0 + (low ? CELL : 0);
        q[k].z = (float)z;
        q[k].u = right ? 1.0f : 0.0f;
        q[k].v = low ? 1.0f : 0.0f;
        l[k] = level_at(light, depth_of(q[k].x, q[k].y));
        if (l[k] > lmax) lmax = l[k];
    }
    face_tex(&f, texid, lmax);
    for (k = 0; k < 4; k++) {
        q[k].argb = lit(&f, l[k]);
        q[k].oargb = s_oargb;
    }
    dc3d_add(&cam, DC3D_OPAQUE, f.slot, q, 4);
}

/* A vertical face from grid corner a (left, as seen) to b (right), heights
 * zb..zt. The texture repeats every 64 units, fixed to the world (v = 0 at
 * multiples of 64, as the reference), or counted from the face's foot
 * (`anchor`: a door's or a lift's moving edge carries it). */
static void vface(int gax, int gay, int gbx, int gby, int zb, int zt, int texid, int light, int anchor)
{
    Dc3dIn q[4];
    Face f;
    uint32_t ca, cb;
    float xa, ya, xb, yb, vt, vb, h, la, lb;
    if (zt <= zb) return;
    xa = (float)(gax * CELL); ya = (float)(gay * CELL);
    xb = (float)(gbx * CELL); yb = (float)(gby * CELL);
    h = (float)(zt - zb) / 64.0f;
    if (anchor) {
        vb = ceilf(h);
        vt = vb - h;
    } else {
        vt = ceilf((float)zt / 64.0f) - (float)zt / 64.0f;
        vb = vt + h;
    }
    la = level_at(light, depth_of(xa, ya));
    lb = level_at(light, depth_of(xb, yb));
    face_tex(&f, texid, la > lb ? la : lb);
    ca = lit(&f, la);
    cb = lit(&f, lb);
    q[0].x = xa; q[0].y = ya; q[0].z = (float)zt; q[0].u = 0.0f; q[0].v = vt; q[0].argb = ca;
    q[1].x = xb; q[1].y = yb; q[1].z = (float)zt; q[1].u = 1.0f; q[1].v = vt; q[1].argb = cb;
    q[2].x = xb; q[2].y = yb; q[2].z = (float)zb; q[2].u = 1.0f; q[2].v = vb; q[2].argb = cb;
    q[3].x = xa; q[3].y = ya; q[3].z = (float)zb; q[3].u = 0.0f; q[3].v = vb; q[3].argb = ca;
    q[0].oargb = q[1].oargb = q[2].oargb = q[3].oargb = s_oargb;
    dc3d_add(&cam, DC3D_OPAQUE, f.slot, q, 4);
}

/* the faces on the edge between open cell C (floor fz, ceiling cz, `sky`)
 * and its neighbour (nx, ny), from corner a (left) to b (right) */
static void edge(int nx, int ny, int gax, int gay, int gbx, int gby, int fz, int cz, int sky, int light)
{
    const DoomoCell *n = map_cell_safe(M, nx, ny);
    int nfz, ncz, lo;
    if (n->wall) {
        vface(gax, gay, gbx, gby, fz, sky ? block_top(n, nx, ny, fz) : cz, n->wall, light, 0);
        return;
    }
    nfz = map_floor_z(M, nx, ny);
    if (is_sky(n)) {
        if (nfz > fz) vface(gax, gay, gbx, gby, fz, sky || nfz < cz ? nfz : cz, n->floor_tex, light - 1,
                            (n->flags & CELL_LIFT) != 0);
        return;
    }
    ncz = map_ceil_z(M, nx, ny);
    lo = ncz > fz ? ncz : fz;                    /* the upper face's foot */
    if (sky) {
        /* the facade over a roofed cell seen from under the sky */
        int roof = roof_of(nx, ny), top = roof != 127 ? roof * DOOMO_HEIGHT_STEP : fz + 256;
        if (top < ncz) top = ncz;
        if (nfz > fz) vface(gax, gay, gbx, gby, fz, nfz < lo ? nfz : lo, n->floor_tex, light - 1,
                            (n->flags & CELL_LIFT) != 0);
        vface(gax, gay, gbx, gby, lo, top, n->ceil_tex, light - 1, (n->flags & CELL_DOOR) != 0);
        return;
    }
    if (ncz >= cz) lo = cz;                      /* no upper face */
    if (nfz > fz) vface(gax, gay, gbx, gby, fz, nfz < lo ? nfz : lo, n->floor_tex, light - 1,
                        (n->flags & CELL_LIFT) != 0);
    if (ncz < cz) vface(gax, gay, gbx, gby, lo, cz, n->ceil_tex, light - 1, (n->flags & CELL_DOOR) != 0);
}

static void emit_cell(int cx, int cy)
{
    const DoomoCell *c = &M->cells[cy * M->w + cx];
    float X0 = (float)(cx * CELL), Y0 = (float)(cy * CELL), X1 = X0 + CELL, Y1 = Y0 + CELL;
    int light = light_at(c), fz, cz, sky;
    if (c->wall) {
        /* a block under the sky the rays went over: its roof from above,
         * and the taller blocks' faces that rise behind it */
        static const int8_t nb[4][6] = {     /* dx, dy, corner a, corner b (as seen from here) */
            { 0, -1, 0, 0, 1, 0 }, { 1, 0, 1, 0, 1, 1 }, { 0, 1, 1, 1, 0, 1 }, { -1, 0, 0, 1, 0, 0 } };
        int top = roof_top(c, cx, cy), k;
        if (s_ez > (float)top) flat(cx, cy, top, c->floor_tex ? c->floor_tex : c->wall, light);
        for (k = 0; k < 4; k++) {
            int nx = cx + nb[k][0], ny = cy + nb[k][1], ntop;
            const DoomoCell *n;
            if (!in_map(nx, ny)) continue;
            n = &M->cells[ny * M->w + nx];
            if (!n->wall) continue;
            ntop = roof_top(n, nx, ny);
            if (ntop <= top) continue;
            if ((k == 0 && s_ey <= Y0) || (k == 1 && s_ex >= X1) || (k == 2 && s_ey >= Y1) || (k == 3 && s_ex <= X0))
                continue;
            vface(cx + nb[k][2], cy + nb[k][3], cx + nb[k][4], cy + nb[k][5], top, ntop, n->wall,
                  light - (k == 0 || k == 2), 0);
        }
        return;
    }
    fz = map_floor_z(M, cx, cy);
    cz = map_ceil_z(M, cx, cy);
    sky = is_sky(c);
    if (sky) s_any_sky = 1;
    if (s_ez > (float)fz) flat(cx, cy, fz, c->floor_tex, light);
    if (!sky && s_ez < (float)cz && cz > fz) flat(cx, cy, cz, c->ceil_tex, light);
    /* each edge only from its inside; a = left, b = right as seen. Faces
     * across the y axis (north, south) are a level darker (the reference's
     * `side`) */
    if (s_ey > Y0) edge(cx, cy - 1, cx, cy, cx + 1, cy, fz, cz, sky, light - 1);
    if (s_ex < X1) edge(cx + 1, cy, cx + 1, cy, cx + 1, cy + 1, fz, cz, sky, light);
    if (s_ey < Y1) edge(cx, cy + 1, cx + 1, cy + 1, cx, cy + 1, fz, cz, sky, light - 1);
    if (s_ex > X0) edge(cx - 1, cy, cx, cy + 1, cx, cy, fz, cz, sky, light);
}

/* ---- visibility ---------------------------------------------------------------------- */
static void mark(int cx, int cy)
{
    int i;
    if (!in_map(cx, cy)) return;
    i = cy * M->w + cx;
    if (s_vis[i] != s_stamp) {
        s_vis[i] = s_stamp;
        s_vlist[s_nvis++] = (uint16_t)i;
    }
}

/* One ray from the eye along (rdx, rdy) = forward + sideways (so the DDA's
 * parameter is the camera depth in cells), with the reference's window as
 * slopes (height over depth, relative to the eye): [sb, st]. `eps`: how
 * close to a grid corner (in cells per cell of depth) a crossing marks the
 * cells around the corner too. */
static void cast(float rdx, float rdy, float sb, float st, float eps)
{
    float px = s_ex / CELL, py = s_ey / CELL;
    int mx = (int)floorf(px), my = (int)floorf(py);
    float ddx = rdx != 0.0f ? fabsf(1.0f / rdx) : V3D_BIG, ddy = rdy != 0.0f ? fabsf(1.0f / rdy) : V3D_BIG;
    int stx = rdx < 0.0f ? -1 : 1, sty = rdy < 0.0f ? -1 : 1, step, steps = 2 * s_view_dist + 4;
    float sdx = rdx < 0.0f ? (px - (float)mx) * ddx : ((float)(mx + 1) - px) * ddx;
    float sdy = rdy < 0.0f ? (py - (float)my) * ddy : ((float)(my + 1) - py) * ddy;
    float cur_f, cur_c, tmax = (float)s_view_dist;
    int cur_sky;
    if (in_map(mx, my) && !M->cells[my * M->w + mx].wall) {
        const DoomoCell *c = &M->cells[my * M->w + mx];
        cur_f = (float)map_floor_z(M, mx, my);
        cur_c = (float)map_ceil_z(M, mx, my);
        cur_sky = is_sky(c);
    } else {                                 /* the eye inside a block or off the map: see out */
        cur_f = -V3D_BIG;
        cur_c = V3D_BIG;
        cur_sky = 1;
    }
    for (step = 0; step < steps; step++) {
        int side, nmx = mx, nmy = my;
        float t, d1, fr;
        const DoomoCell *n;
        if (sdx < sdy) { t = sdx; sdx += ddx; nmx += stx; side = 0; }
        else { t = sdy; sdy += ddy; nmy += sty; side = 1; }
        if (t > tmax) break;
        d1 = t * CELL;
        if (d1 < 0.01f) d1 = 0.01f;
        /* near a grid corner: the cells around it */
        fr = side == 0 ? py + rdy * t - (float)my : px + rdx * t - (float)mx;
        if (fr < t * eps || fr > 1.0f - t * eps) {
            int o = fr < 0.5f ? -1 : 1;
            if (side == 0) { mark(mx, my + o); mark(nmx, my + o); }
            else { mark(mx + o, my); mark(mx + o, nmy); }
        }
        n = map_cell_safe(M, nmx, nmy);
        if (n->wall) {
            float top;
            if (!cur_sky || !in_map(nmx, nmy)) break;
            top = cur_f > -V3D_BIG ? (float)block_top(n, nmx, nmy, (int)cur_f) : (float)roof_top(n, nmx, nmy);
            mark(nmx, nmy);
            if ((top - s_ez) / d1 > sb) sb = (top - s_ez) / d1;
            cur_f = top;
        } else {
            float nf = (float)map_floor_z(M, nmx, nmy), nc = (float)map_ceil_z(M, nmx, nmy);
            int nsky = is_sky(n);
            float open_top = cur_sky ? V3D_BIG : cur_c;
            float new_top = nsky ? open_top : (nc < open_top ? nc : open_top);
            float new_bot = nf > cur_f ? nf : cur_f;
            if ((new_bot - s_ez) / d1 > sb) sb = (new_bot - s_ez) / d1;
            if (new_top < V3D_BIG && (new_top - s_ez) / d1 < st) st = (new_top - s_ez) / d1;
            if (sb >= st) break;
            mark(nmx, nmy);
            cur_f = nf;
            cur_c = nc;
            cur_sky = nsky;
        }
        if (sb >= st) break;
        mx = nmx;
        my = nmy;
    }
}

static void visibility(float plane, int vw)
{
    int n = vw / 2 + 5, i, cx = (int)floorf(s_ex / CELL), cy = (int)floorf(s_ey / CELL), dx, dy;
    float span = 1.0f + 4.0f / (float)(n - 1);          /* two rays beyond each edge */
    float dc = 2.0f * span / (float)(n - 1);
    float st = (cam.cy - cam.vy0) / cam.focal, sb = (cam.cy - cam.vy1) / cam.focal;
    s_nvis = 0;
    /* the eye's cell and the ones around it: the near plane is 4 units out */
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++) mark(cx + dx, cy + dy);
    for (i = 0; i < n; i++) {
        float c = -span + dc * (float)i;
        cast(s_dx - s_dy * plane * c, s_dy + s_dx * plane * c, sb, st, plane * dc);
    }
}

/* ---- the backdrop: the sky, or the dark beyond the view ----------------------------- */
static DoomoImage s_sky_img;

static void backdrop_quad(int t, float y0, float y1, float v0, float v1, const float *u, uint32_t argb)
{
    int k, cols = u ? V3D_SKY_COLS : 1;
    DcVert *v;
    if (y1 <= y0) return;
    v = dc3d_poly(DC3D_OPAQUE, t, 2 * (cols + 1));
    if (!v) return;
    for (k = 0; k <= cols; k++) {
        float x = cam.vx0 + (cam.vx1 - cam.vx0) * (float)k / (float)cols;
        DcVert *a = &v[2 * k], *b = &v[2 * k + 1];
        a->x = b->x = x;
        a->y = y0; b->y = y1;
        a->z = b->z = V3D_FAR_Z;
        a->u = b->u = u ? u[k] : 0.0f;
        a->v = v0; b->v = v1;
        a->argb = b->argb = argb;
        a->oargb = b->oargb = s_oargb;
    }
}

/* The reference's sky: column x shows the sky's column at its ray's angle
 * (the full turn = the texture once across, repeating), row y the sky's
 * row SKY_H - 8 - (horizon - y) * (SKY_H - 8) / (3/4 of the view's height),
 * held at the first and the last row beyond them. Three bands of
 * V3D_SKY_COLS quads: above, the mapped rows, below. */
static void sky(int angle, float plane, int vh)
{
    const uint8_t *pix = sky_pixels(M->sky, s_time);
    const DcTex *tx;
    float u[V3D_SKY_COLS + 1], span = (float)(vh * 3 / 4) * DC2D_SCALE, ya, yb, vscale;
    uint32_t argb = 0xFF000000u | pack_rgb(255.0f * s_tmul.r, 255.0f * s_tmul.g, 255.0f * s_tmul.b);
    int k, slot;
    if (!pix) {
        backdrop_quad(-1, cam.vy0, cam.vy1, 0.0f, 0.0f, NULL, tinted_pal(PAL_RAMP(RAMP_SKY, 5)));
        return;
    }
    s_sky_img.w = SKY_W;
    s_sky_img.h = SKY_H;
    s_sky_img.pix = pix;
    s_sky_img.uid = V3D_SKY_UID + (uint32_t)((pix - doomo_art_sky_pix[0]) / (SKY_W * SKY_H));
    slot = dc2d_tex_image(&s_sky_img);
    if (slot < 0) {
        backdrop_quad(-1, cam.vy0, cam.vy1, 0.0f, 0.0f, NULL, tinted_pal(sky_horizon(M->sky)));
        return;
    }
    tx = dc2d_tex(slot);
    for (k = 0; k <= V3D_SKY_COLS; k++) {
        float c = -1.0f + 2.0f * (float)k / V3D_SKY_COLS;
        u[k] = ((float)(angle + DOOMO_ANG_FULL) + atanf(plane * c) * (DOOMO_ANG_FULL / 6.2831853f)) / (float)DOOMO_ANG_FULL *
               ((float)SKY_W / (float)tx->tw);
    }
    if (span < 1.0f) span = 1.0f;
    vscale = 1.0f / (float)tx->th;
    ya = cam.cy - span;                                      /* the sky's row 0 */
    yb = cam.cy + span * 7.5f / (float)(SKY_H - 8);          /* ... and its last row */
    {
        float y0 = cam.vy0, y1 = cam.vy1, a = DOOMO_CLAMP(ya, y0, y1), b = DOOMO_CLAMP(yb, y0, y1);
        float rows = (float)(SKY_H - 8) / span;              /* sky rows per frame pixel */
        float va = ((float)(SKY_H - 8) - (cam.cy - a) * rows + 0.5f), vb = ((float)(SKY_H - 8) - (cam.cy - b) * rows + 0.5f);
        va = DOOMO_CLAMP(va, 0.5f, (float)SKY_H - 0.5f);
        vb = DOOMO_CLAMP(vb, 0.5f, (float)SKY_H - 0.5f);
        backdrop_quad(slot, y0, a, 0.5f * vscale, 0.5f * vscale, u, argb);
        backdrop_quad(slot, a, b, va * vscale, vb * vscale, u, argb);
        backdrop_quad(slot, b, y1, ((float)SKY_H - 0.5f) * vscale, ((float)SKY_H - 0.5f) * vscale, u, argb);
    }
}

/* ---- billboards (part 2 of DC6 owns their final look) --------------------------------- */
static int16_t s_order[V3D_MAX_SPRITES];
static float   s_depth[V3D_MAX_SPRITES];

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

/* a rectangle parallel to the screen at the sprite's depth: the
 * reference's size (texels x scale / 256 world units) and place (centred
 * on x, standing on z), cut to the viewport */
static void billboard(const DoomoView3D *view, int idx, float depth)
{
    const DoomoBillboard *b = &view->sprites[idx];
    const DoomoImage *img = b->img;
    const DcTex *tx;
    DcVert *v;
    float x = (float)b->x / 65536.0f, y = (float)b->y / 65536.0f, z = (float)b->z / 65536.0f;
    float scale = (float)(b->scale ? b->scale : 256) / 256.0f, w = (float)img->w * scale, h = (float)img->h * scale;
    float lat = (x - s_ex) * -s_dy + (y - s_ey) * s_dx, k = cam.focal / depth;
    float sx0 = cam.cx + (lat - w * 0.5f) * k, sx1 = sx0 + w * k;
    float sy0 = cam.cy - (z + h - s_ez) * k, sy1 = sy0 + h * k;
    float u0 = 0.0f, u1, v0 = 0.0f, v1, level;
    uint32_t argb;
    int slot, lv, cx = (int)floorf(x / CELL), cy = (int)floorf(y / CELL);
    if (sx1 <= cam.vx0 || sx0 >= cam.vx1 || sy1 <= cam.vy0 || sy0 >= cam.vy1) { dc3d_stats.culled++; return; }
    if (sx1 - sx0 > 4.0f * DC2D_W || sy1 - sy0 > 4.0f * DC2D_H) return;
    if (b->flags & BB_SHADOW) shadow(x, y, z + 1.0f, w * 0.375f + 2.0f);
    slot = dc2d_tex_image(img);
    if (slot < 0) return;
    /* the reference's colours: the sprite through the colormap of its
     * whole light level (a remapped copy; the plain image when there is
     * no room, darkened by the level's fitted factor) */
    if (b->flags & BB_FULLBRIGHT) level = 15.0f;
    else {
        level = level_at(light_at(map_cell_safe(M, cx, cy)), depth) + (float)b->light;
        level = level < 1.0f ? 1.0f : level > 15.0f ? 15.0f : level;
    }
    lv = (int)(level + 0.5f);
    argb = pack_rgb(255.0f * s_tmul.r, 255.0f * s_tmul.g, 255.0f * s_tmul.b);
    if (lv < DOOMO_LIGHT_LEVELS - 1) {
        int r = dc2d_tex_remap(slot, lv, doomo_colormap[lv]);
        if (r >= 0) slot = r;
        else argb = pack_rgb(255.0f * s_fit[lv].r * s_tmul.r, 255.0f * s_fit[lv].g * s_tmul.g, 255.0f * s_fit[lv].b * s_tmul.b);
    }
    tx = dc2d_tex(slot);
    u1 = (float)img->w / (float)tx->tw;
    v1 = (float)img->h / (float)tx->th;
    if (b->flags & BB_FLIPX) { u0 = u1; u1 = 0.0f; }
    /* cut to the viewport, the texture with it */
    if (sx0 < cam.vx0) { u0 += (u1 - u0) * (cam.vx0 - sx0) / (sx1 - sx0); sx0 = cam.vx0; }
    if (sx1 > cam.vx1) { u1 -= (u1 - u0) * (sx1 - cam.vx1) / (sx1 - sx0); sx1 = cam.vx1; }
    if (sy0 < cam.vy0) { v0 += (v1 - v0) * (cam.vy0 - sy0) / (sy1 - sy0); sy0 = cam.vy0; }
    if (sy1 > cam.vy1) { v1 -= (v1 - v0) * (sy1 - cam.vy1) / (sy1 - sy0); sy1 = cam.vy1; }
    /* see-through: BB_TRANSLUCENT, and the player's own body when the
     * camera is very close (the reference dithers every other pixel) */
    argb |= ((b->flags & BB_TRANSLUCENT) || (idx == view->player_sprite && depth < 40.0f)) ? 0x80000000u : 0xFF000000u;
    v = dc3d_poly(DC3D_SPRITE, slot, 4);
    if (!v) return;
    v[0].x = sx0; v[0].y = sy0; v[0].u = u0; v[0].v = v0;
    v[1].x = sx1; v[1].y = sy0; v[1].u = u1; v[1].v = v0;
    v[2].x = sx0; v[2].y = sy1; v[2].u = u0; v[2].v = v1;
    v[3].x = sx1; v[3].y = sy1; v[3].u = u1; v[3].v = v1;
    v[0].z = v[1].z = v[2].z = v[3].z = DC3D_ZK / depth;
    v[0].argb = v[1].argb = v[2].argb = v[3].argb = argb;
    v[0].oargb = v[1].oargb = v[2].oargb = v[3].oargb = s_oargb;
}

static void billboards(const DoomoView3D *view)
{
    int i, n = 0, count = DOOMO_MIN(view->sprite_count, V3D_MAX_SPRITES);
    float maxd = (float)(s_view_dist * CELL);
    for (i = 0; i < count; i++) {
        const DoomoBillboard *b = &view->sprites[i];
        float d, x, y;
        int j;
        if (!b->img || !b->img->pix || !b->img->w || !b->img->h) continue;
        x = (float)b->x / 65536.0f;
        y = (float)b->y / 65536.0f;
        d = depth_of(x, y);
        if (d < V3D_ZNEAR || d > maxd) continue;
        for (j = n; j > 0 && s_depth[j - 1] < d; j--) {      /* far to near, ties in call order */
            s_depth[j] = s_depth[j - 1];
            s_order[j] = s_order[j - 1];
        }
        s_depth[j] = d;
        s_order[j] = (int16_t)i;
        n++;
    }
    for (i = 0; i < n; i++) billboard(view, s_order[i], s_depth[i]);
}

/* ---- the view ---------------------------------------------------------------------------- */
#ifdef DC2D_HOST
void dc2d_view3d_draw(const DoomoView3D *v)
#else
void plat_view3d(const DoomoView3D *v)
#endif
{
    int vx, vy, vw, vh, fov, plane_fx, focal, horizon, i, poly0;
    float plane;
    uint32_t t0;
    if (!v) return;
    vx = v->vw > 0 ? v->vx : 0;
    vy = v->vh > 0 ? v->vy : 0;
    vw = v->vw > 0 ? v->vw : DC2D_LW;
    vh = v->vh > 0 ? v->vh : DC2D_LH;
    if (vx < 0) { vw += vx; vx = 0; }
    if (vy < 0) { vh += vy; vy = 0; }
    if (vx + vw > DC2D_LW) vw = DC2D_LW - vx;
    if (vy + vh > DC2D_LH) vh = DC2D_LH - vy;
    if (vw <= 0 || vh <= 0) return;
    if (!v->map || !v->map->w || !v->map->h) {
        DC2D_FN(rect)(vx, vy, vw, vh, PAL_RAMP(RAMP_SKY, 5));
        return;
    }
    t0 = dc3d_be_us();
    M = v->map;
    s_time = v->time;
    if (++s_stamp == 0) {
        memset(s_vis, 0, sizeof(s_vis));
        memset(s_tstamp, 0, sizeof(s_tstamp));
        s_stamp = 1;
    }

    /* the camera: the reference's integers (raycast.c, combat_hud_project) */
    fov = opt_get(OPT_FOV);
    plane_fx = fov == 0 ? 36000 : fov == 2 ? 52000 : 43250;         /* tan(fov / 2), 16.16 */
    plane = (float)plane_fx / 65536.0f;
    focal = (int)(((int64_t)vw / 2 * 65536) / plane_fx);
    horizon = vy + vh / 2 + (int)((int64_t)v->pitch * focal * 84 / (256 * 100));
    s_dx = (float)fx_cos(v->angle) / 65536.0f;
    s_dy = (float)fx_sin(v->angle) / 65536.0f;
    s_ex = (float)v->x / 65536.0f;
    s_ey = (float)v->y / 65536.0f;
    s_ez = (float)v->z / 65536.0f + (float)v->bob;
    memset(&cam, 0, sizeof(cam));
    cam.ex = s_ex; cam.ey = s_ey; cam.ez = s_ez;
    cam.rx = -s_dy; cam.ry = s_dx;                   /* right */
    cam.dz = -1.0f;                                  /* down on screen */
    cam.fx = s_dx; cam.fy = s_dy;                    /* forward: level */
    cam.vx0 = (float)(vx * DC2D_SCALE);
    cam.vy0 = (float)(vy * DC2D_SCALE);
    cam.vx1 = (float)((vx + vw) * DC2D_SCALE);
    cam.vy1 = (float)((vy + vh) * DC2D_SCALE);
    cam.cx = (float)((2 * vx + vw) * DC2D_SCALE) * 0.5f;
    cam.cy = (float)(horizon * DC2D_SCALE);          /* the pitch: the horizon's row */
    cam.focal = (float)(focal > 0 ? focal : 1) * DC2D_SCALE;
    cam.znear = V3D_ZNEAR;
    cam.clip = vw < DC2D_LW || vh < DC2D_LH;
    s_fade = (float)DC_V3D_DIST_MAX / (float)s_view_dist / V3D_FADE_UNITS;

    /* the view tint: lerp(c, tint(c), level / 16), tint(c) = a c + b; and
     * the light levels' fitted factors (never 0: they divide) */
    {
        int t = v->tint < TINT_COUNT ? v->tint : TINT_NONE;
        float f = t == TINT_NONE ? 0.0f : (float)DOOMO_CLAMP(v->tint_level, 0, 16) / 16.0f, a[3], b[3];
        dc3d_tint_fit(t, a, b);
        s_tmul.r = 1.0f - f + f * a[0]; s_tadd.r = f * b[0];
        s_tmul.g = 1.0f - f + f * a[1]; s_tadd.g = f * b[1];
        s_tmul.b = 1.0f - f + f * a[2]; s_tadd.b = f * b[2];
        s_oargb = pack_rgb(s_tadd.r, s_tadd.g, s_tadd.b);
        for (i = 0; i < DOOMO_LIGHT_LEVELS; i++) {
            uint32_t c = dc2d_shade_argb(i);
            s_fit[i].r = (float)(((c >> 16) & 255u) + 1u) / 256.0f;
            s_fit[i].g = (float)(((c >> 8) & 255u) + 1u) / 256.0f;
            s_fit[i].b = (float)((c & 255u) + 1u) / 256.0f;
        }
    }

    if (!dc3d_seg_begin(vx, vy, vw, vh)) return;
    poly0 = dc3d_batch(dc3d_cur_slot())->np;
    s_any_sky = 0;
    visibility(plane, vw);
    /* the backdrop goes FIRST: on the console the list is drawn in order and
     * the backdrop's far z does not keep it behind the walls (Flycast, the
     * translucent list), so a backdrop emitted last painted over everything */
    if (s_backdrop) {
        for (i = 0; i < s_nvis; i++) {             /* open cells under the sky: the sky is drawn */
            const DoomoCell *c = &M->cells[s_vlist[i]];
            if (!c->wall && is_sky(c)) { s_any_sky = 1; break; }
        }
        if (s_any_sky) sky(v->angle, plane, vh);
        else backdrop_quad(-1, cam.vy0, cam.vy1, 0.0f, 0.0f, NULL, tinted_pal(PAL_RAMP(RAMP_STONE, 0)));
    }
    for (i = 0; i < s_nvis; i++) {
        int idx = s_vlist[i], cy = idx / M->w;
        emit_cell(idx - cy * M->w, cy);
    }
    dc3d_sort_from(poly0);               /* the opaque world: one header per texture */
    /* billboards over it, far to near (DC6 part 2: the player's model,
     * the weapon and the effects join here) */
    if (v->sprites && v->sprite_count > 0) billboards(v);
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
const Dc3dCam *dc2d_view3d_cam(void) { return &cam; }
int dc2d_view3d_cells(void) { return s_nvis; }
void dc2d_view3d_backdrop(int on) { s_backdrop = on; }
#endif
