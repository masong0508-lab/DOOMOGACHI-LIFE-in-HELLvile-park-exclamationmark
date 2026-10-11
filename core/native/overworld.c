/* overworld.c - the native top-down plat_overworld() (owner: foundation).
 *
 * Semantics (shared with the GBA's 2D overworld):
 *  - one cell = a 16x16 tile; world units map to pixels at 1/4
 *    (DOOMO_CELL_UNITS / 16); the camera focus is the viewport centre
 *  - open cells: the floor texture sampled down to 16x16; raised floors
 *    (floor_h > 0) are drawn lighter and lifted, pits darker
 *  - solid cells: raised blocks - a top face (floor_tex if set, else the
 *    wall texture) lifted by the block height, and a darker front face
 *  - billboards are drawn feet-anchored, y-sorted with the blocks, at
 *    1 texel = 1 pixel for scale 256 (2x for 512 ...), lifted by z / 4
 *  - daylight 0..15 picks the colormap level (night keeps lit cells
 *    (light >= 13) glowing), then the view tint is applied
 */
#include <string.h>
#include "native.h"

#define TILE 16
#define UNITS_PER_PX (DOOMO_CELL_UNITS / TILE)
#define BLOCK_H 10

static const DoomoOverworldView *OV;
static int ox, oy, ow, oh, scrollx, scrolly, day_level;

static int cell_level(const DoomoCell *c)
{
    int l = day_level;
    if (c->light >= 13 && l < 10) l += 3;     /* lamps glow at night */
    if (c->light && c->light < 8) l -= (8 - c->light) / 2;
    return DOOMO_CLAMP(l, 2, 15);
}

static void tile_from_tex(int px, int py, int tex, int lvl, int lift_px)
{
    const uint8_t *tp = (tex > 0 && tex < 255) ? texture_pixels(tex, OV->time) : NULL;
    const uint8_t *cm = doomo_colormap[DOOMO_CLAMP(lvl, 0, 15)];
    uint8_t avg = tp ? 0 : texture_avg(tex);
    int x, y;
    for (y = 0; y < TILE; y++) {
        int sy = py + y - lift_px;
        if (sy < oy || sy >= oy + oh) continue;
        for (x = 0; x < TILE; x++) {
            int sx = px + x;
            if (sx < ox || sx >= ox + ow) continue;
            nat_fb[sy * nat_w + sx] = cm[tp ? tp[(y * 4 + 1) * 64 + x * 4 + 1] : avg];
        }
    }
}

static void block_front(int px, int py, int tex, int lvl, int h)
{
    const uint8_t *tp = (tex > 0 && tex < 255) ? texture_pixels(tex, OV->time) : NULL;
    const uint8_t *cm = doomo_colormap[DOOMO_CLAMP(lvl - 4, 1, 15)];
    uint8_t avg = tp ? 0 : texture_avg(tex);
    int x, y;
    for (y = 0; y < h; y++) {
        int sy = py + TILE - h + y;
        if (sy < oy || sy >= oy + oh) continue;
        for (x = 0; x < TILE; x++) {
            int sx = px + x;
            uint8_t c;
            if (sx < ox || sx >= ox + ow) continue;
            c = tp ? tp[((y * 64 / h) & 63) * 64 + x * 4 + 2] : avg;
            nat_fb[sy * nat_w + sx] = cm[c];
        }
    }
    /* a darker contact line where the block meets the ground */
    if (py + TILE - 1 >= oy && py + TILE - 1 < oy + oh)
        for (x = 0; x < TILE; x++) {
            int sx = px + x;
            if (sx >= ox && sx < ox + ow) {
                uint8_t *p = &nat_fb[(py + TILE - 1) * nat_w + sx];
                *p = doomo_colormap[3][*p];
            }
        }
}

static void blit_sprite(const DoomoBillboard *b)
{
    const DoomoImage *img = b->img;
    int s, w, h, x0, y0, x, y, lvl;
    const uint8_t *cm;
    if (!img || !img->pix) return;
    s = DOOMO_CLAMP((b->scale ? b->scale : 256) / 256, 1, 4);
    w = img->w * s;
    h = img->h * s;
    x0 = ox + (int)((b->x >> 16) / UNITS_PER_PX) - scrollx - w / 2;
    y0 = oy + (int)((b->y >> 16) / UNITS_PER_PX) - scrolly - h - (int)((b->z >> 16) / UNITS_PER_PX);
    lvl = (b->flags & BB_FULLBRIGHT) ? 15 : DOOMO_CLAMP(day_level + b->light, 2, 15);
    cm = doomo_colormap[lvl];
    if (b->flags & BB_SHADOW) {
        int fy = oy + (int)((b->y >> 16) / UNITS_PER_PX) - scrolly;
        int half = w / 3 + 1, k;
        for (k = -1; k <= 0; k++)
            for (x = -half + (k ? 1 : 0); x <= half - (k ? 1 : 0); x++) {
                int sx = x0 + w / 2 + x, sy = fy + k;
                if (sx >= ox && sx < ox + ow && sy >= oy && sy < oy + oh) {
                    uint8_t *p = &nat_fb[sy * nat_w + sx];
                    *p = doomo_colormap[8][*p];
                }
            }
    }
    for (y = 0; y < h; y++) {
        int sy = y0 + y;
        if (sy < oy || sy >= oy + oh) continue;
        for (x = 0; x < w; x++) {
            int sx = x0 + x, tx = x / s;
            uint8_t c;
            if (sx < ox || sx >= ox + ow) continue;
            if (b->flags & BB_FLIPX) tx = img->w - 1 - tx;
            c = img->pix[(y / s) * img->w + tx];
            if (!c) continue;
            if ((b->flags & BB_TRANSLUCENT) && ((sx + sy) & 1)) continue;
            nat_fb[sy * nat_w + sx] = cm[c];
        }
    }
}

static int16_t order[256];

void plat_overworld(const DoomoOverworldView *v)
{
    const DoomoMap *m;
    int tx0, ty0, tx1, ty1, tx, ty, i, n, si;
    if (!v) return;
    OV = v;
    m = v->map;
    ox = v->vw > 0 ? v->vx : 0;
    oy = v->vh > 0 ? v->vy : 0;
    ow = v->vw > 0 ? v->vw : nat_w;
    oh = v->vh > 0 ? v->vh : nat_h;
    if (ox < 0) { ow += ox; ox = 0; }
    if (oy < 0) { oh += oy; oy = 0; }
    if (ox + ow > nat_w) ow = nat_w - ox;
    if (oy + oh > nat_h) oh = nat_h - oy;
    if (ow <= 0 || oh <= 0) return;
    day_level = 5 + DOOMO_CLAMP(v->daylight, 0, 15) * 10 / 15;
    if (!m) { plat_rect(ox, oy, ow, oh, PAL_RAMP(RAMP_GRASS, 5)); return; }

    scrollx = (int)((v->cx >> 16) / UNITS_PER_PX) - ow / 2;
    scrolly = (int)((v->cy >> 16) / UNITS_PER_PX) - oh / 2;
    tx0 = (scrollx >= 0 ? scrollx / TILE : (scrollx - TILE + 1) / TILE) - 1;
    ty0 = (scrolly >= 0 ? scrolly / TILE : (scrolly - TILE + 1) / TILE) - 1;
    tx1 = tx0 + ow / TILE + 3;
    ty1 = ty0 + oh / TILE + 4;

    /* ground */
    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++) {
            const DoomoCell *c = map_cell_safe(m, tx, ty);
            int px = ox + tx * TILE - scrollx, py = oy + ty * TILE - scrolly;
            int lvl = cell_level(c);
            if (c->wall) {
                tile_from_tex(px, py, c->floor_tex ? c->floor_tex : TEX_GRASS, lvl - 3, 0);
                continue;
            }
            if (c->floor_h < 0) lvl -= 3;
            tile_from_tex(px, py, c->floor_tex, lvl, 0);
        }

    /* sort sprites by feet y */
    n = v->sprites ? DOOMO_MIN(v->sprite_count, 256) : 0;
    for (i = 0; i < n; i++) {
        int j = i;
        while (j > 0 && v->sprites[order[j - 1]].y > v->sprites[i].y) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = (int16_t)i;
    }

    /* raised things row by row, sprites interleaved by their feet */
    si = 0;
    for (ty = ty0; ty <= ty1; ty++) {
        int32_t row_bottom = (int32_t)(ty + 1) * DOOMO_CELL_UNITS;
        for (tx = tx0; tx <= tx1; tx++) {
            const DoomoCell *c = map_cell_safe(m, tx, ty);
            int px = ox + tx * TILE - scrollx, py = oy + ty * TILE - scrolly;
            int lvl = cell_level(c);
            if (c->wall) {
                int h = c->ceil_h > c->floor_h ? DOOMO_CLAMP((c->ceil_h - c->floor_h) * 2, 6, 14) : BLOCK_H;
                block_front(px, py, c->wall, lvl, h);
                tile_from_tex(px, py, c->floor_tex ? c->floor_tex : c->wall, lvl + 1, h);
            } else if (c->floor_h > 0) {
                int lift = DOOMO_MIN(c->floor_h * 2, 8);
                block_front(px, py, c->floor_tex, lvl, lift);
                tile_from_tex(px, py, c->floor_tex, lvl + 1, lift);
            }
        }
        while (si < n && (v->sprites[order[si]].y >> 16) < row_bottom) {
            blit_sprite(&v->sprites[order[si]]);
            si++;
        }
    }
    while (si < n) blit_sprite(&v->sprites[order[si++]]);

    /* night: lean blue; then the view tint */
    if (v->daylight < 8) {
        int y, x, lvl = (8 - v->daylight) * 2;
        for (y = oy; y < oy + oh; y++)
            for (x = ox; x < ox + ow; x++)
                if (nat_bayer(x, y) < lvl) {
                    uint8_t *p = &nat_fb[y * nat_w + x];
                    *p = doomo_tintmap[TINT_NIGHT][*p];
                }
    }
    if (v->tint != TINT_NONE && v->tint < TINT_COUNT && v->tint_level > 0) {
        int y, x;
        for (y = oy; y < oy + oh; y++)
            for (x = ox; x < ox + ow; x++)
                if (nat_bayer(x, y) < v->tint_level) {
                    uint8_t *p = &nat_fb[y * nat_w + x];
                    *p = doomo_tintmap[v->tint][*p];
                }
    }
}
