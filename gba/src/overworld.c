/* overworld.c - plat_overworld(): the cutesy top-down town (owner: gba).
 *
 * Semantics follow the native reference (core/native/overworld.c):
 *  - one cell = a 16x16 tile, 4 world units per pixel, camera focus at
 *    the viewport centre
 *  - open cells: their floor texture point-sampled down to 16x16; pits
 *    (floor_h < 0) darker; raised floors (floor_h > 0) lifted blocks
 *  - solid cells: raised blocks - a darker front face (6..14 px from
 *    ceil_h - floor_h, else 10) and a lit top face (floor_tex, else the
 *    wall texture) lifted by that height, with a dark contact line
 *  - billboards feet-anchored, y-sorted and interleaved with the block
 *    rows, 1 texel = 1 pixel at scale 256 (integer 1..4x), lifted by z/4,
 *    drop-shadow blobs, translucency as a checker
 *  - daylight 0..15 -> colormap level 5..15 (bright cells glow at night),
 *    a dithered night tint below daylight 8, then the view tint
 *
 * Speed: ground tiles are cached LIT (per texture frame + light level +
 * tint state) in a small EWRAM cache, so a frame of ground is mostly
 * halfword copies (ow.iwram.c). Front faces and sprites are drawn per
 * pixel through the colormap.
 */
#include "gba_plat.h"
#include "ow_int.h"
#include "doomo/textures.h"
#include "doomo/palette.h"
#include "doomo/map.h"

#define TILE 16
#define UNITS_PER_PX (DOOMO_CELL_UNITS / TILE)
#define BLOCK_H 10
#define CACHE_SETS 32            /* power of two: the set is a mask  */
#define CACHE_WAYS 2

typedef struct OwTile {
    uint8_t  pix[TILE * TILE];   /* first: word aligned for ow_ground */
    const uint8_t *src;          /* texture frame it was built from   */
    uint32_t key;                /* level | front h << 4 | tint << 8   */
    uint8_t  tex, used, pad[2];
} OwTile;

/* The cache is the overworld's region of the arena (bigbuf.c, G0): the
 * CACHE_SETS sets right below the billboards (GBA_BIG_OW = 17,152 B)
 * beside the story overworld's 6.5 KB of scene scratch. Beside a larger
 * scratch claim only the upper half of the sets is used (cache_half), which
 * clears any claim, instead of covering the scene's state. */
#define OW_SET_BYTES (sizeof(OwTile) * CACHE_WAYS)
#define cache ((OwTile (*)[CACHE_WAYS])(void *)gba_ow)
DOOMO_STATIC_ASSERT(OW_SET_BYTES * CACHE_SETS == GBA_BIG_OW, ow_region_size);
DOOMO_STATIC_ASSERT((uint32_t)GBA_ARENA_BB - DOOMO_SCENE_SCRATCH >= OW_SET_BYTES * CACHE_SETS / 2u,
                    ow_upper_half_clears_any_claim);
/* what lit_cached() reads on every call, in one struct so that one base
 * register reaches all of it (G0: the half-cache flag costs no load) */
typedef struct OwLit {
    uint32_t hi;                 /* 0, or CACHE_SETS / 2: only sets 16..31 */
    uint32_t tick;
    uint32_t tint_state;         /* changes invalidate by key        */
    /* this frame's texture pixels per id (animated textures change frame);
     * ids >= TEX_COUNT (textures.h: <= 64) fall back to TEX_GRASS below */
    const uint8_t *frame_tex[TEX_COUNT];
    uint8_t frame_tex_ok[TEX_COUNT];
} OwLit;
static OwLit lc;

const uint8_t ow_bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

OwFrame owf;

void ow_invalidate(void)
{
    if (lc.hi) memset(cache[CACHE_SETS / 2], 0, OW_SET_BYTES * CACHE_SETS / 2);
    else memset(cache, 0, OW_SET_BYTES * CACHE_SETS);
}

/* after the claim: the whole cache, or its upper half beside a claim that
 * reaches into the lower one */
static void cache_fit(void)
{
    uint32_t hi = gba_big_fits(GBA_BIG_OW) ? 0u : CACHE_SETS / 2u;
    if (hi != lc.hi) {
        lc.hi = hi;
        ow_invalidate();
    }
}

void ow_init(void)
{
    ow_invalidate();
}

/* ------------------------------------------------------------------------- */
static uint8_t post(uint8_t c, int x, int y)
{
    /* night dither, then the view tint (screen / world 4x4 ordered dither) */
    int b = ow_bayer[((y & 3) << 2) | (x & 3)];
    if (b < owf.night) c = owf.night_map[c];
    if (b < owf.tint_level) c = owf.tint_map[c];
    return c;
}

/* lit 16x16 tile (front == 0) or 16 x front block front face, cached */
static const OwTile *lit_cached(int tex, int lvl, int front)
{
    OwLit *L = &lc;
    const uint8_t *tp;
    uint32_t set, key, tick;
    OwTile *t, *victim;
    int w, x, y;
    if (tex <= 0 || tex >= TEX_COUNT) tex = TEX_GRASS;
    if (lvl < 0) lvl = 0;
    if (lvl > 15) lvl = 15;
    if (!L->frame_tex_ok[tex]) {
        L->frame_tex[tex] = texture_pixels(tex, owf.time);
        L->frame_tex_ok[tex] = 1;
    }
    tp = L->frame_tex[tex];
    key = (uint32_t)lvl | ((uint32_t)front << 4) | (L->tint_state << 8);
    set = ((((uint32_t)tex * 7u + (uint32_t)lvl * 13u + (uint32_t)front * 5u) & (CACHE_SETS - 1)) | L->hi);
    tick = L->tick;
    victim = &cache[set][0];
    for (w = 0; w < CACHE_WAYS; w++) {
        t = &cache[set][w];
        if (t->src == tp && t->key == key && t->tex == tex && tp) {
            t->used = (uint8_t)tick;
            return t;
        }
        if ((uint8_t)(tick - t->used) > (uint8_t)(tick - victim->used)) victim = t;
    }
    t = victim;
    t->src = tp;
    t->key = key;
    t->tex = (uint8_t)tex;
    t->used = (uint8_t)tick;
    {
        const uint8_t *cm = doomo_colormap[lvl];
        uint8_t avg = tp ? 0 : texture_avg(tex);
        if (!front) {
            for (y = 0; y < TILE; y++)
                for (x = 0; x < TILE; x++) {
                    uint8_t c = tp ? tp[(y * 4 + 1) * 64 + x * 4 + 1] : avg;
                    t->pix[y * TILE + x] = post(cm[c], x, y);
                }
        } else {
            for (y = 0; y < front; y++) {
                const uint8_t *tr = tp ? tp + ((y * 64 / front) & 63) * 64 + 2 : 0;
                for (x = 0; x < TILE; x++)
                    t->pix[y * TILE + x] = post(cm[tr ? tr[x * 4] : avg], x, y);
            }
        }
    }
    return t;
}

static const OwTile *lit_tile(int tex, int lvl) { return lit_cached(tex, lvl, 0); }

static int cell_level(const DoomoCell *c)
{
    int l = owf.day_level;
    if (c->light >= 13 && l < 10) l += 3;     /* lamps glow at night */
    if (c->light && c->light < 8) l -= (8 - c->light) / 2;
    return DOOMO_CLAMP(l, 2, 15);
}

/* tile at screen (px, py) lifted by `lift` pixels, clipped to the view */
static void draw_tile(int px, int py, int tex, int lvl, int lift)
{
    const OwTile *t;
    int x0 = px, x1 = px + TILE, y0 = py - lift, y1 = py - lift + TILE;
    if (x0 < owf.ox) x0 = owf.ox;
    if (x1 > owf.ox + owf.ow) x1 = owf.ox + owf.ow;
    if (y0 < owf.oy) y0 = owf.oy;
    if (y1 > owf.oy + owf.oh) y1 = owf.oy + owf.oh;
    if (x0 >= x1 || y0 >= y1) return;
    t = lit_tile(tex, lvl);
    if (x0 == px && x1 == px + TILE && !(px & 1))
        ow_tile16(gba_back + y0 * (SCREEN_W / 2) + (px >> 1),
                  t->pix + (y0 - (py - lift)) * TILE, y1 - y0);
    else
        ow_blit(x0, x1, y0, y1, t->pix + (y0 - (py - lift)) * TILE + (x0 - px), TILE);
}

/* front face of a block: h rows at the bottom of the cell (cached, lit
 * 4 levels darker) */
static void draw_front(int px, int py, int tex, int lvl, int h)
{
    const OwTile *t = lit_cached(tex, DOOMO_CLAMP(lvl - 4, 1, 15), h);
    int sy0 = py + TILE - h, y0 = sy0, y1 = sy0 + h, x0 = px, x1 = px + TILE;
    if (y0 < owf.oy) y0 = owf.oy;
    if (y1 > owf.oy + owf.oh) y1 = owf.oy + owf.oh;
    if (x0 < owf.ox) x0 = owf.ox;
    if (x1 > owf.ox + owf.ow) x1 = owf.ox + owf.ow;
    if (y0 < y1 && x0 == px && x1 == px + TILE && !(px & 1))
        ow_tile16(gba_back + y0 * (SCREEN_W / 2) + (px >> 1), t->pix + (y0 - sy0) * TILE, y1 - y0);
    else if (y0 < y1 && x0 < x1)
        ow_blit(x0, x1, y0, y1, t->pix + (y0 - sy0) * TILE + (x0 - px), TILE);
    /* a darker contact line where the block meets the ground */
    {
        int sy = py + TILE - 1;
        if (sy >= owf.oy && sy < owf.oy + owf.oh && x0 < x1)
            ow_shade(x0, x1, sy, sy + 1, doomo_colormap[3]);
    }
}

static void draw_sprite(const DoomoBillboard *b)
{
    const DoomoImage *img = b->img;
    int s, w, h, x0, y0, lvl;
    if (!img || !img->pix || !img->w || !img->h) return;
    s = DOOMO_CLAMP((b->scale ? b->scale : 256) / 256, 1, 4);
    w = img->w * s;
    h = img->h * s;
    x0 = owf.ox + (int)((b->x >> 16) / UNITS_PER_PX) - owf.scrollx - w / 2;
    y0 = owf.oy + (int)((b->y >> 16) / UNITS_PER_PX) - owf.scrolly - h -
         (int)((b->z >> 16) / UNITS_PER_PX);
    lvl = (b->flags & BB_FULLBRIGHT) ? 15 : DOOMO_CLAMP(owf.day_level + b->light, 2, 15);
    if (b->flags & BB_SHADOW) {
        int fy = owf.oy + (int)((b->y >> 16) / UNITS_PER_PX) - owf.scrolly;
        int half = w / 3 + 1, cx = x0 + w / 2;
        int a0 = cx - half, a1 = cx + half + 1;
        if (a0 < owf.ox) a0 = owf.ox;
        if (a1 > owf.ox + owf.ow) a1 = owf.ox + owf.ow;
        if (fy >= owf.oy && fy < owf.oy + owf.oh && a0 < a1)
            ow_shade(a0, a1, fy, fy + 1, doomo_colormap[8]);
        a0 = cx - half + 1; a1 = cx + half;
        if (a0 < owf.ox) a0 = owf.ox;
        if (a1 > owf.ox + owf.ow) a1 = owf.ox + owf.ow;
        if (fy - 1 >= owf.oy && fy - 1 < owf.oy + owf.oh && a0 < a1)
            ow_shade(a0, a1, fy - 1, fy, doomo_colormap[8]);
    }
    ow_sprite(img, x0, y0, s, b->flags, doomo_colormap[lvl]);
}

static int16_t order[128];

static inline const DoomoCell *cell_at(const DoomoMap *m, int tx, int ty)
{
    if ((unsigned)tx < (unsigned)m->w && (unsigned)ty < (unsigned)m->h)
        return &m->cells[ty * m->w + tx];
    return map_cell_safe(m, tx, ty);
}

void plat_overworld(const DoomoOverworldView *v)
{
    const DoomoMap *m;
    int tx0, ty0, tx1, ty1, tx, ty, i, n, si;
    if (gba_nodraw || !v) return;
    gba_big_claim(GBA_BIG_BY_OW);
    cache_fit();
    m = v->map;
    owf.ox = v->vw > 0 ? v->vx : 0;
    owf.oy = v->vh > 0 ? v->vy : 0;
    owf.ow = v->vw > 0 ? v->vw : SCREEN_W;
    owf.oh = v->vh > 0 ? v->vh : SCREEN_H;
    if (owf.ox < 0) { owf.ow += owf.ox; owf.ox = 0; }
    if (owf.oy < 0) { owf.oh += owf.oy; owf.oy = 0; }
    if (owf.ox + owf.ow > SCREEN_W) owf.ow = SCREEN_W - owf.ox;
    if (owf.oy + owf.oh > SCREEN_H) owf.oh = SCREEN_H - owf.oy;
    if (owf.ow <= 0 || owf.oh <= 0) return;
    owf.day_level = 5 + DOOMO_CLAMP(v->daylight, 0, 15) * 10 / 15;
    owf.time = v->time;
    if (!m) { plat_rect(owf.ox, owf.oy, owf.ow, owf.oh, PAL_RAMP(RAMP_GRASS, 5)); return; }

    /* night + tint state (part of the lit tile cache key) */
    owf.night = v->daylight < 8 ? (8 - v->daylight) * 2 : 0;
    owf.night_map = doomo_tintmap[TINT_NIGHT];
    owf.tint_level = (v->tint != TINT_NONE && v->tint < TINT_COUNT) ? v->tint_level : 0;
    owf.tint_map = doomo_tintmap[owf.tint_level ? v->tint : TINT_NONE];
    {
        uint32_t st = (uint32_t)owf.night | ((uint32_t)owf.tint_level << 5) |
                      ((uint32_t)(owf.tint_level ? v->tint : 0) << 10);
        if (st != (lc.tint_state & 0x0FFF)) lc.tint_state = st;
    }
    lc.tick++;
    memset(lc.frame_tex_ok, 0, sizeof(lc.frame_tex_ok));

    /* horizontal scroll in 2-pixel steps: every row copy is then halfword
     * aligned with VRAM (DMA / halfword copies, no byte shuffling) */
    owf.ox &= ~1;
    owf.scrollx = ((int)((v->cx >> 16) / UNITS_PER_PX) - owf.ow / 2) & ~1;
    owf.scrolly = (int)((v->cy >> 16) / UNITS_PER_PX) - owf.oh / 2;
    tx0 = (owf.scrollx >= 0 ? owf.scrollx / TILE : (owf.scrollx - TILE + 1) / TILE) - 1;
    ty0 = (owf.scrolly >= 0 ? owf.scrolly / TILE : (owf.scrolly - TILE + 1) / TILE) - 1;
    tx1 = tx0 + owf.ow / TILE + 3;
    ty1 = ty0 + owf.oh / TILE + 4;

    /* ground: every visible cell's lit tile (16-pixel block copies; the
     * clipped edge tiles go through ow_blit). Blocks and raised floors
     * are skipped: their lifted top plus front face cover the whole cell
     * (drawn in the next pass). */
    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++) {
            const DoomoCell *c = cell_at(m, tx, ty);
            int px = owf.ox + tx * TILE - owf.scrollx, py = owf.oy + ty * TILE - owf.scrolly;
            int lvl;
            if (c->wall || c->floor_h > 0) continue;
            if (px >= owf.ox + owf.ow || py >= owf.oy + owf.oh || px + TILE <= owf.ox ||
                py + TILE <= owf.oy)
                continue;
            lvl = cell_level(c);
            if (c->floor_h < 0) lvl -= 3;
            draw_tile(px, py, c->floor_tex, lvl, 0);
        }

    /* sort sprites by feet y */
    n = v->sprites ? DOOMO_MIN(v->sprite_count, 128) : 0;
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
            const DoomoCell *c = cell_at(m, tx, ty);
            int px = owf.ox + tx * TILE - owf.scrollx, py = owf.oy + ty * TILE - owf.scrolly;
            int lvl;
            if (!c->wall && c->floor_h <= 0) continue;
            if (px >= owf.ox + owf.ow || px + TILE <= owf.ox || py - 16 >= owf.oy + owf.oh ||
                py + TILE <= owf.oy)
                continue;
            lvl = cell_level(c);
            if (c->wall) {
                int h = c->ceil_h > c->floor_h
                            ? DOOMO_CLAMP((c->ceil_h - c->floor_h) * 2, 6, 14) : BLOCK_H;
                draw_front(px, py, c->wall, lvl, h);
                draw_tile(px, py, c->floor_tex ? c->floor_tex : c->wall, lvl + 1, h);
            } else {
                int lift = DOOMO_MIN(c->floor_h * 2, 8);
                draw_front(px, py, c->floor_tex, lvl, lift);
                draw_tile(px, py, c->floor_tex, lvl + 1, lift);
            }
        }
        while (si < n && (v->sprites[order[si]].y >> 16) < row_bottom) {
            draw_sprite(&v->sprites[order[si]]);
            si++;
        }
    }
    while (si < n) draw_sprite(&v->sprites[order[si++]]);
}
