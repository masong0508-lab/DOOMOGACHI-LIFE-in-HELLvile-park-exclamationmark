/* raycast.c - the reference SOFTWARE HEIGHT RAYCASTER behind the native
 * plat_view3d() (owner: foundation). The GBA renderer implements the same
 * semantics in optimised form; PS1 renders the same maps as polygons.
 *
 * Semantics (shared with the console renderers):
 *  - one ray per screen column, DDA through the cell grid, front to back,
 *    with a per-column open window [ytop, ybot] that shrinks as geometry
 *    is drawn (pitch is a vertical shear of the horizon)
 *  - every open cell draws its textured floor (and ceiling, unless its
 *    ceil_tex is DOOMO_TEX_SKY) between the distances the ray enters and
 *    leaves it
 *  - entering a cell whose floor is higher draws a lower STEP FACE with the
 *    new cell's floor texture; a lower ceiling draws an upper face with its
 *    ceiling texture
 *  - a solid cell (wall != 0) draws a wall face with texture `wall`; under
 *    a roofed cell it closes the column; under open sky the wall is as tall
 *    as the wall cell's ceil_h (when above its floor_h) and the ray
 *    continues over its roof (roof texture = its floor_tex, else wall)
 *  - light = the cell's light (0 = map ambient) minus distance fade,
 *    x-side faces one level darker; doomo_colormap does the shading
 *  - whatever stays open at the end shows the sky (or dark fog indoors)
 *  - billboards are sorted far to near and clipped per column against the
 *    window that was open at their depth, so low walls hide feet only
 */
#include <string.h>
#include "native.h"

#define NEAR_UNITS 4            /* sprites closer than this are not drawn */
#define MAX_STEPS 96
#define MAX_DIST_UNITS (48 * DOOMO_CELL_UNITS)
#define CLIP_MAX 64
#define SKY_Z (100000)

typedef struct { int32_t d; int16_t top, bot; } ClipEnt;   /* d: world units */
static ClipEnt clips[NAT_MAX_W][CLIP_MAX];
static uint8_t nclips[NAT_MAX_W];
static int16_t clip_top0[NAT_MAX_W], clip_bot0[NAT_MAX_W];

/* per-frame camera */
static const DoomoView3D *V;
static const DoomoMap *M;
static int64_t eye;             /* fix16 world units */
static int64_t camx, camy;      /* fix16 world units */
static int focal;               /* pixels */
static int horizon;             /* screen row */
static int vx0, vy0, vw0, vh0;

static int light_at(const DoomoCell *c)
{
    int l = c->light ? c->light : (M->ambient ? M->ambient : 12);
    return DOOMO_CLAMP(l, 0, 15);
}

static int fade(int light, int64_t dist_fx)
{
    int l = light - (int)((dist_fx >> 16) / 128);
    return DOOMO_CLAMP(l, 1, 15);
}

/* screen row of world height z (fix16) at distance d (fix16, > 0) */
static int proj_y(int64_t z, int64_t d)
{
    int64_t r;
    if (d < 64) d = 64;
    r = ((z - eye) * focal) / d;
    if (r > 1000000) r = 1000000;
    if (r < -1000000) r = -1000000;
    return horizon - (int)r;
}

static const uint8_t *tex_of(int tex)
{
    if (tex <= 0 || tex >= 255) return NULL;
    return texture_pixels(tex, V->time);
}

/* vertical face column from world z_top..z_bot at distance d */
static void face(int x, int64_t z_top, int64_t z_bot, int64_t d, int tex, int u,
                 int light, int ytop, int ybot)
{
    int ys = proj_y(z_top, d), ye = proj_y(z_bot, d) - 1, y;
    const uint8_t *tp = tex_of(tex);
    const uint8_t *cm = doomo_colormap[fade(light, d)];
    uint8_t flat = tp ? 0 : texture_avg(tex);
    if (z_top >= ((int64_t)SKY_Z * 65536)) ys = ytop;
    if (ys < ytop) ys = ytop;
    if (ye > ybot) ye = ybot;
    for (y = ys; y <= ye; y++) {
        /* world z at the centre of this row */
        int64_t z = eye - ((int64_t)(2 * (y - horizon) + 1) * d) / (2 * focal);
        int v = (int)((-(z >> 16)) & 63);
        uint8_t c = tp ? tp[v * 64 + (u & 63)] : flat;
        nat_fb[y * nat_w + vx0 + x] = cm[c];
    }
}

/* floor (is_floor) or ceiling of one cell between distances d0..d1 */
static void flat(int x, int64_t rdx, int64_t rdy, int64_t z, int64_t d0, int64_t d1,
                 int tex, int light, int is_floor, int ytop, int ybot)
{
    int ys, ye, y;
    const uint8_t *tp = tex_of(tex);
    uint8_t avg = tp ? 0 : texture_avg(tex);
    if (is_floor) { ys = proj_y(z, d1); ye = proj_y(z, d0) - 1; }
    else { ys = proj_y(z, d0); ye = proj_y(z, d1) - 1; }
    if (ys < ytop) ys = ytop;
    if (ye > ybot) ye = ybot;
    for (y = ys; y <= ye; y++) {
        int64_t den = (int64_t)(2 * (y - horizon) + 1);
        int64_t d;
        int u, v;
        if (den == 0) continue;
        d = ((eye - z) * 2 * focal) / den;
        if (d < 0) d = -d;
        if (d < d0) d = d0;
        if (d > d1) d = d1;
        u = (int)(((camx + ((rdx * d) >> 16)) >> 16) & 63);
        v = (int)(((camy + ((rdy * d) >> 16)) >> 16) & 63);
        nat_fb[y * nat_w + vx0 + x] =
            doomo_colormap[fade(light, d)][tp ? tp[v * 64 + u] : avg];
    }
}

static void sky_column(int x, int64_t rdx, int64_t rdy, int ytop, int ybot)
{
    const uint8_t *sp = sky_pixels(M->sky, V->time);
    int ang = fx_atan2((int32_t)(rdy >> 4), (int32_t)(rdx >> 4));
    int sx = (ang * SKY_W / DOOMO_ANG_FULL) & (SKY_W - 1), y;
    int span = vh0 * 3 / 4;
    for (y = ytop; y <= ybot; y++) {
        int row = SKY_H - 8 - (horizon - y) * (SKY_H - 8) / (span ? span : 1);
        row = DOOMO_CLAMP(row, 0, SKY_H - 1);
        nat_fb[y * nat_w + vx0 + x] = sp ? sp[row * SKY_W + sx] : PAL_RAMP(RAMP_SKY, 5);
    }
}

/* (action, additive) The roof of a roofed open cell seen from under the
 * sky, as the GBA renderer: the tallest explicit top of its solid
 * neighbours (a doorway in a house), else 127 = none. */
static int roof_of(int cx, int cy)
{
    static const int8_t nb[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    int k, roof = 127;
    for (k = 0; k < 4; k++) {
        int nx = cx + nb[k][0], ny = cy + nb[k][1];
        const DoomoCell *o;
        if (nx < 0 || ny < 0 || nx >= M->w || ny >= M->h) continue;
        o = &M->cells[ny * M->w + nx];
        if (o->wall && o->ceil_h > o->floor_h && (roof == 127 || o->ceil_h > roof)) roof = o->ceil_h;
    }
    return roof;
}

static void push_clip(int x, int64_t d, int ytop, int ybot)
{
    if (nclips[x] < CLIP_MAX) {
        ClipEnt *e = &clips[x][nclips[x]++];
        e->d = (int32_t)((d >> 16) > 0x7FFFFFFF ? 0x7FFFFFFF : (d >> 16));
        e->top = (int16_t)ytop;
        e->bot = (int16_t)ybot;
    }
}

static void cast_column(int x, int64_t dirx, int64_t diry, int64_t plx, int64_t ply)
{
    int64_t camx_c = (2 * (int64_t)x + 1 - vw0) * 65536 / vw0;    /* -1..1 */
    int64_t rdx = dirx + ((plx * camx_c) >> 16);
    int64_t rdy = diry + ((ply * camx_c) >> 16);
    int64_t posx = camx / DOOMO_CELL_UNITS, posy = camy / DOOMO_CELL_UNITS; /* fix16 cells */
    int mx = (int)(posx >> 16), my = (int)(posy >> 16);
    int64_t deltax = rdx ? ((int64_t)1 << 32) / (rdx < 0 ? -rdx : rdx) : ((int64_t)1 << 40);
    int64_t deltay = rdy ? ((int64_t)1 << 32) / (rdy < 0 ? -rdy : rdy) : ((int64_t)1 << 40);
    int stepx = rdx < 0 ? -1 : 1, stepy = rdy < 0 ? -1 : 1;
    int64_t sidex = rdx < 0 ? (((posx - ((int64_t)mx * 65536)) * deltax) >> 16)
                            : ((((int64_t)(mx + 1) * 65536) - posx) * deltax >> 16);
    int64_t sidey = rdy < 0 ? (((posy - ((int64_t)my * 65536)) * deltay) >> 16)
                            : ((((int64_t)(my + 1) * 65536) - posy) * deltay >> 16);
    int ytop = vy0, ybot = vy0 + vh0 - 1, step;
    const DoomoCell *cur = map_cell_safe(M, mx, my);
    int64_t cur_f = (int64_t)map_floor_z(M, mx, my) * 65536;
    int64_t cur_c = (int64_t)map_ceil_z(M, mx, my) * 65536;
    int cur_sky = cur->ceil_tex == DOOMO_TEX_SKY;
    int cur_ftex = cur->floor_tex, cur_ctex = cur->ceil_tex, cur_light = light_at(cur);
    int64_t d0 = (int64_t)1 << 16;

    nclips[x] = 0;
    clip_top0[x] = (int16_t)ytop;
    clip_bot0[x] = (int16_t)ybot;
    /* local window: rows are absolute screen rows */
    for (step = 0; step < MAX_STEPS; step++) {
        int side, nmx = mx, nmy = my, u;
        int64_t dcell, d1;
        const DoomoCell *n;
        if (sidex < sidey) { dcell = sidex; sidex += deltax; nmx += stepx; side = 0; }
        else { dcell = sidey; sidey += deltay; nmy += stepy; side = 1; }
        d1 = dcell * DOOMO_CELL_UNITS;
        if (d1 < d0) d1 = d0;
        /* the current cell's floor and ceiling */
        flat(x, rdx, rdy, cur_f, d0, d1, cur_ftex, cur_light, 1, ytop, ybot);
        if (!cur_sky) flat(x, rdx, rdy, cur_c, d0, d1, cur_ctex, cur_light, 0, ytop, ybot);
        /* texture u along the face we hit */
        if (side == 0) u = (int)(((camy + ((rdy * d1) >> 16)) >> 16) & 63);
        else u = (int)(((camx + ((rdx * d1) >> 16)) >> 16) & 63);
        if ((side == 0 && stepx < 0) || (side == 1 && stepy > 0)) u = 63 - u;
        n = map_cell_safe(M, nmx, nmy);
        if (n->wall) {
            int64_t top;
            int lit = cur_light - side;
            if (cur_sky) {
                top = n->ceil_h > n->floor_h ? ((int64_t)map_ceil_z(M, nmx, nmy) * 65536)
                                             : cur_f + ((int64_t)256 * 65536);
            } else {
                top = cur_c;
            }
            face(x, top, cur_f, d1, n->wall, u, lit, ytop, ybot);
            if (!cur_sky || nmx < 0 || nmy < 0 || nmx >= M->w || nmy >= M->h) {
                if (cur_sky) {
                    /* the map's edge under the open sky: the sky above it
                     * (action, additive: as the GBA renderer does) */
                    int ny = proj_y(top, d1) - 1;
                    if (ny < ybot) ybot = ny;
                    if (ytop <= ybot) sky_column(x, rdx, rdy, ytop, ybot);
                }
                ytop = ybot + 1;
                push_clip(x, d1, ytop, ybot);
                break;
            }
            {
                int ny = proj_y(top, d1) - 1;
                if (ny < ybot) ybot = ny;
            }
            cur_f = top;
            cur_ftex = n->floor_tex ? n->floor_tex : n->wall;
            cur_light = light_at(n);
            cur_sky = 1;
        } else {
            int64_t nf = (int64_t)map_floor_z(M, nmx, nmy) * 65536;
            int64_t nc = (int64_t)map_ceil_z(M, nmx, nmy) * 65536;
            int nsky = n->ceil_tex == DOOMO_TEX_SKY;
            int64_t open_top = cur_sky ? ((int64_t)SKY_Z * 65536) : cur_c;
            int64_t new_top = nsky ? open_top : (nc < open_top ? nc : open_top);
            int64_t new_bot = nf > cur_f ? nf : cur_f;
            int lit = cur_light - side;
            if (nf > cur_f) face(x, nf, cur_f, d1, n->floor_tex, u, lit - 1, ytop, ybot);
            if (!nsky && nc < open_top) {
                int64_t ftop = open_top;
                if (cur_sky) {
                    /* (action, additive) a roofed cell seen from under the
                     * sky: sky above its roof, the lintel below; the roof
                     * is roof_of() or 256 units above the floor it is
                     * seen from (as the GBA renderer) */
                    int roof = roof_of(nmx, nmy), ys;
                    ftop = roof != 127 ? (int64_t)roof * DOOMO_HEIGHT_STEP * 65536
                                       : cur_f + ((int64_t)256 * 65536);
                    if (ftop < nc) ftop = nc;
                    ys = proj_y(ftop, d1);
                    if (ys > ybot + 1) ys = ybot + 1;
                    if (ys > ytop) { sky_column(x, rdx, rdy, ytop, ys - 1); ytop = ys; }
                }
                face(x, ftop, nc, d1, n->ceil_tex, u, lit - 1, ytop, ybot);
            }
            {
                int nb = proj_y(new_bot, d1) - 1;
                if (nb < ybot) ybot = nb;
                if (new_top < ((int64_t)SKY_Z * 65536)) {
                    int nt = proj_y(new_top, d1);
                    if (nt > ytop) ytop = nt;
                }
            }
            cur = n;
            cur_f = nf;
            cur_c = nc;
            cur_sky = nsky && (cur_sky || nsky);
            cur_ftex = n->floor_tex;
            cur_ctex = n->ceil_tex;
            cur_light = light_at(n);
        }
        mx = nmx;
        my = nmy;
        d0 = d1;
        push_clip(x, d1, ytop, ybot);
        if (ytop > ybot || d1 > ((int64_t)MAX_DIST_UNITS * 65536)) break;
    }
    if (ytop <= ybot) {
        if (cur_sky) sky_column(x, rdx, rdy, ytop, ybot);
        else {
            int y;
            for (y = ytop; y <= ybot; y++) nat_fb[y * nat_w + vx0 + x] = PAL_RAMP(RAMP_STONE, 0);
        }
    }
}

/* ---- billboards ----------------------------------------------------------- */
static int64_t spr_depth[256];
static int16_t spr_order[256];

static void window_at(int x, int64_t depth_units, int *top, int *bot)
{
    int i;
    *top = clip_top0[x];
    *bot = clip_bot0[x];
    for (i = 0; i < nclips[x]; i++) {
        if (clips[x][i].d > depth_units) break;
        *top = clips[x][i].top;
        *bot = clips[x][i].bot;
    }
}

static void draw_sprite(int idx, int64_t dirx, int64_t diry)
{
    const DoomoBillboard *b = &V->sprites[idx];
    const DoomoImage *img = b->img;
    int64_t rx, ry, depth, lat, wwu, hwu;
    int sw, sh, sx0, sy0, sx, cx, base_light, lvl, translucent;
    if (!img || !img->pix || !img->w || !img->h) return;
    rx = (int64_t)b->x - camx;
    ry = (int64_t)b->y - camy;
    depth = (rx * dirx + ry * diry) >> 16;                 /* fix16 units */
    if (depth < ((int64_t)NEAR_UNITS * 65536)) return;
    lat = (rx * -diry + ry * dirx) >> 16;
    wwu = (int64_t)img->w * (b->scale ? b->scale : 256) * 256;   /* fix16 */
    hwu = (int64_t)img->h * (b->scale ? b->scale : 256) * 256;
    sw = (int)(wwu * focal / depth);
    sh = (int)(hwu * focal / depth);
    if (sw < 1 || sh < 1 || sw > 4 * nat_w || sh > 4 * nat_h) return;
    cx = vw0 / 2 + (int)(lat * focal / depth);
    sx0 = cx - sw / 2;
    sy0 = proj_y((int64_t)b->z + hwu, depth);
    {
        int ccx = (int)((b->x >> 16) / DOOMO_CELL_UNITS), ccy = (int)((b->y >> 16) / DOOMO_CELL_UNITS);
        base_light = light_at(map_cell_safe(M, ccx, ccy));
    }
    lvl = (b->flags & BB_FULLBRIGHT) ? 15 : DOOMO_CLAMP(fade(base_light, depth) + b->light, 1, 15);
    translucent = (b->flags & BB_TRANSLUCENT) ||
                  (idx == V->player_sprite && depth < ((int64_t)40 * 65536));
    if (b->flags & BB_SHADOW) {
        int fy = proj_y(b->z, depth), half = sw / 3;
        for (sx = cx - half; sx <= cx + half; sx++) {
            int top, bot;
            if (sx < 0 || sx >= vw0) continue;
            window_at(sx, depth >> 16, &top, &bot);
            if (fy >= top && fy <= bot) {
                uint8_t *p = &nat_fb[fy * nat_w + vx0 + sx];
                *p = doomo_colormap[6][*p];
            }
        }
    }
    for (sx = sx0; sx < sx0 + sw; sx++) {
        int top, bot, y, tx;
        if (sx < 0 || sx >= vw0) continue;
        window_at(sx, depth >> 16, &top, &bot);
        tx = (int)((int64_t)(sx - sx0) * img->w / sw);
        if (b->flags & BB_FLIPX) tx = img->w - 1 - tx;
        for (y = DOOMO_MAX(sy0, top); y < sy0 + sh && y <= bot; y++) {
            int ty = (int)((int64_t)(y - sy0) * img->h / sh);
            uint8_t c = img->pix[ty * img->w + tx];
            if (!c) continue;
            if (translucent && ((sx + y) & 1)) continue;
            nat_fb[y * nat_w + vx0 + sx] = doomo_colormap[lvl][c];
        }
    }
}

void plat_view3d(const DoomoView3D *v)
{
    int64_t dirx, diry, plx, ply, plane;
    int x, i, n, fov;
    if (!v) return;
    V = v;
    M = v->map;
    vx0 = v->vw > 0 ? v->vx : 0;
    vy0 = v->vh > 0 ? v->vy : 0;
    vw0 = v->vw > 0 ? v->vw : nat_w;
    vh0 = v->vh > 0 ? v->vh : nat_h;
    if (vx0 < 0) { vw0 += vx0; vx0 = 0; }
    if (vy0 < 0) { vh0 += vy0; vy0 = 0; }
    if (vx0 + vw0 > nat_w) vw0 = nat_w - vx0;
    if (vy0 + vh0 > nat_h) vh0 = nat_h - vy0;
    if (vw0 <= 0 || vh0 <= 0) return;
    if (!M) { plat_rect(vx0, vy0, vw0, vh0, PAL_RAMP(RAMP_SKY, 5)); return; }

    fov = opt_get(OPT_FOV);
    plane = fov == 0 ? 36000 : fov == 2 ? 52000 : 43250;    /* tan(fov/2) */
    dirx = fx_cos(v->angle);
    diry = fx_sin(v->angle);
    plx = (-diry * plane) >> 16;
    ply = (dirx * plane) >> 16;
    focal = (int)(((int64_t)vw0 / 2 * 65536) / plane);
    horizon = vy0 + vh0 / 2 + (int)((int64_t)v->pitch * focal * 84 / (256 * 100));
    camx = v->x;
    camy = v->y;
    eye = (int64_t)v->z + ((int64_t)v->bob * 65536);

    for (x = 0; x < vw0; x++) cast_column(x, dirx, diry, plx, ply);

    /* billboards, far to near */
    n = v->sprites ? DOOMO_MIN(v->sprite_count, 256) : 0;
    for (i = 0; i < n; i++) {
        int64_t rx = (int64_t)v->sprites[i].x - camx, ry = (int64_t)v->sprites[i].y - camy;
        int j = i;
        int64_t d = (rx * dirx + ry * diry) >> 16;
        while (j > 0 && spr_depth[j - 1] < d) {
            spr_depth[j] = spr_depth[j - 1];
            spr_order[j] = spr_order[j - 1];
            j--;
        }
        spr_depth[j] = d;
        spr_order[j] = (int16_t)i;
    }
    for (i = 0; i < n; i++) draw_sprite(spr_order[i], dirx, diry);

    /* tint (damage flash, holy flash, vape haze) with an ordered dither */
    if (v->tint != TINT_NONE && v->tint < TINT_COUNT && v->tint_level > 0) {
        int y;
        for (y = vy0; y < vy0 + vh0; y++)
            for (x = vx0; x < vx0 + vw0; x++)
                if (nat_bayer(x, y) < v->tint_level) {
                    uint8_t *p = &nat_fb[y * nat_w + x];
                    *p = doomo_tintmap[v->tint][*p];
                }
    }
}
