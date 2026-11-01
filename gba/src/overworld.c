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
#include "canvas_int.h"
#include "r3d_int.h"                 /* r3d_cmrow: its colormap rows share the VRAM gaps */
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
    uint8_t  tex, used;
    uint8_t  vslot;              /* (G2) this frame's VRAM copy + 1, or 0 */
    uint8_t  uses;               /* (G2) copies drawn from it this frame */
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
    uint32_t builds;             /* (G2) tiles built (cache misses)  */
    uint32_t vused;              /* (G2) VRAM tile slots taken this frame */
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
    cv_invalidate();                    /* G5: the tile metadata, too */
}

/* after the claim: the whole cache, or its upper half beside a claim that
 * reaches into the lower one - or while tiles are on (G5): the lower half
 * then holds the tile frame's metadata (canvas_int.h cv_meta) */
static void cache_fit(int tiles)
{
    uint32_t hi = (gba_big_fits(GBA_BIG_OW) && !tiles) ? 0u : CACHE_SETS / 2u;
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

/* texels 4k..4k+3 of a colormap as a word (the generated tables are
 * word aligned in practice; a byte-aligned one still works) */
static inline uint32_t cm4(const uint8_t *cm, int k)
{
    if (!((uint32_t)cm & 3u)) return ((const uint32_t *)(const void *)cm)[k];
    cm += k * 4;
    return cm[0] | (cm[1] << 8) | (cm[2] << 16) | ((uint32_t)cm[3] << 24);
}

/* (G2) the composed tables of colormap cm: lc[c] for every class c the
 * frame uses (ow_int.h), from the frame's cache or built now (THUMB:
 * once per colormap and frame, IWRAM is kept for the pixel loops) */
void owf_sprite_luts(const uint8_t *cm, const uint8_t **lc)
{
    OwLuts *L = owf.luts;
    const int nc = owf.nc;
    int w, c;
    const uint8_t *tab;
    for (w = 0; w < L->ways; w++)
        if (L->key[w] == cm) goto found;
    w = L->victim;
    L->victim = (uint8_t)(w + 1 < L->ways ? w + 1 : 0);
    L->key[w] = cm;
    for (c = 0; c < 4; c++) {
        const uint8_t *nm = owf.night_map, *tm = owf.tint_map;
        uint32_t *d;
        int k;
        if (owf.slot[c] == 0xFF) continue;
        d = L->tab[w * nc + owf.slot[c]];
        switch (c) {
        case 0:
            for (k = 0; k < 64; k++) d[k] = cm4(cm, k);
            break;
        case 1:
            for (k = 0; k < 64; k++) {
                uint32_t v = cm4(cm, k);
                d[k] = nm[v & 0xFF] | (nm[(v >> 8) & 0xFF] << 8) | (nm[(v >> 16) & 0xFF] << 16) |
                       ((uint32_t)nm[v >> 24] << 24);
            }
            break;
        case 2:
            for (k = 0; k < 64; k++) {
                uint32_t v = cm4(cm, k);
                d[k] = tm[v & 0xFF] | (tm[(v >> 8) & 0xFF] << 8) | (tm[(v >> 16) & 0xFF] << 16) |
                       ((uint32_t)tm[v >> 24] << 24);
            }
            break;
        default:
            for (k = 0; k < 64; k++) {
                uint32_t v = cm4(cm, k);
                d[k] = tm[nm[v & 0xFF]] | (tm[nm[(v >> 8) & 0xFF]] << 8) |
                       (tm[nm[(v >> 16) & 0xFF]] << 16) | ((uint32_t)tm[nm[v >> 24]] << 24);
            }
            break;
        }
    }
found:
    tab = (const uint8_t *)(const void *)L->tab[w * nc];
    for (c = 0; c < 4; c++)
        if (owf.slot[c] != 0xFF) lc[c] = tab + owf.slot[c] * 256;
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
    L->builds++;
    t->vslot = 0;                /* its VRAM copy (if any) is stale now */
    t->uses = 0;
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

/* (G2) VRAM copies of this frame's busiest lit tiles. Copying a tile row
 * from EWRAM (16-bit bus, 2 wait states) costs about twice what it does
 * from VRAM, so a tile drawn a second time in a frame is first copied
 * (DMA) into one of 20 slots in the two 2,560-byte gaps the Mode 4 pages
 * leave (0x06009600, 0x06013600: never displayed) and drawn from there.
 * The raycaster keeps its colormap rows in the same gaps (r3d_int.h), so
 * an overworld frame that takes a slot makes it rebuild them. The slots
 * are per frame: plat_overworld clears every tile's vslot first, and a
 * tile rebuilt in the frame drops its slot. */
#define OW_VSLOTS 20

static const uint8_t *tile_src(const OwTile *ct)
{
    OwTile *t = (OwTile *)(uintptr_t)ct;
    uint8_t *d;
    if (t->vslot) return t->vslot <= 10 ? (const uint8_t *)0x06009600 + (t->vslot - 1) * 256
                                        : (const uint8_t *)0x06013600 + (t->vslot - 11) * 256;
    if (!t->uses++ || lc.vused >= OW_VSLOTS) return t->pix;
    if (!lc.vused) r3d_cmrow[0] = 0;             /* the raycaster's rows are gone */
    d = lc.vused < 10 ? (uint8_t *)0x06009600 + lc.vused * 256 : (uint8_t *)0x06013600 + (lc.vused - 10) * 256;
    dma3_copy32(d, t->pix, TILE * TILE / 4);
    t->vslot = (uint8_t)++lc.vused;
    return d;
}

/* (G2) the last lookup of one kind (ground, front, top) in this frame: a
 * neighbour with the same texture, level and front height reuses it
 * without the hash and the ways. Valid while no tile has been built since
 * (a build may have evicted it); cleared at the start of every frame
 * (animated textures and the tint change the tiles). */
typedef struct OwMemo {
    const OwTile *t;
    uint32_t builds;
    int tex, lvl, front;
} OwMemo;

static inline const OwTile *lit_memo(OwMemo *mo, int tex, int lvl, int front)
{
    if (mo->t && mo->tex == tex && mo->lvl == lvl && mo->front == front && mo->builds == lc.builds)
        return mo->t;
    mo->t = lit_cached(tex, lvl, front);
    mo->builds = lc.builds;
    mo->tex = tex;
    mo->lvl = lvl;
    mo->front = front;
    return mo->t;
}

/* ---- for the tile mode (ow_tiles.c, G5) ---- */
const uint8_t *ow_lit(int tex, int lvl, int front) { return lit_cached(tex, lvl, front)->pix; }

const uint8_t *ow_tex_frame(int *tex)
{
    int t = *tex;
    if (t <= 0 || t >= TEX_COUNT) t = TEX_GRASS;
    *tex = t;
    if (!lc.frame_tex_ok[t]) {
        lc.frame_tex[t] = texture_pixels(t, owf.time);
        lc.frame_tex_ok[t] = 1;
    }
    return lc.frame_tex[t];
}

uint32_t ow_tint_state(void) { return lc.tint_state; }

static int cell_level(const DoomoCell *c)
{
    int l = owf.day_level;
    if (c->light >= 13 && l < 10) l += 3;     /* lamps glow at night */
    if (c->light && c->light < 8) l -= (8 - c->light) / 2;
    return DOOMO_CLAMP(l, 2, 15);
}

/* tile at screen (px, py) lifted by `lift` pixels, clipped to the view */
static void draw_tile(int px, int py, int tex, int lvl, int lift, OwMemo *mo)
{
    const uint8_t *src;
    int x0 = px, x1 = px + TILE, y0 = py - lift, y1 = py - lift + TILE;
    if (x0 < owf.ox) x0 = owf.ox;
    if (x1 > owf.ox + owf.ow) x1 = owf.ox + owf.ow;
    if (y0 < owf.oy) y0 = owf.oy;
    if (y1 > owf.oy + owf.oh) y1 = owf.oy + owf.oh;
    if (x0 >= x1 || y0 >= y1) return;
    src = tile_src(lit_memo(mo, tex, lvl, 0));
    if (x0 == px && x1 == px + TILE && !(px & 1))
        ow_tile16(gba_back + y0 * (SCREEN_W / 2) + (px >> 1),
                  src + (y0 - (py - lift)) * TILE, y1 - y0);
    else
        ow_blit(x0, x1, y0, y1, src + (y0 - (py - lift)) * TILE + (x0 - px), TILE);
}

/* front face of a block: h rows at the bottom of the cell (cached, lit
 * 4 levels darker). (G2) The darker contact line where the block meets
 * the ground (screen row py + 15, the face's own last row: no block top
 * reaches it) is left to the caller, which shades a row's lines as runs
 * before the row's sprites; *cx0 / *cx1 get its columns. */
static void draw_front(int px, int py, int tex, int lvl, int h, int *cx0, int *cx1, OwMemo *mo)
{
    const uint8_t *src = tile_src(lit_memo(mo, tex, DOOMO_CLAMP(lvl - 4, 1, 15), h));
    int sy0 = py + TILE - h, y0 = sy0, y1 = sy0 + h, x0 = px, x1 = px + TILE;
    if (y0 < owf.oy) y0 = owf.oy;
    if (y1 > owf.oy + owf.oh) y1 = owf.oy + owf.oh;
    if (x0 < owf.ox) x0 = owf.ox;
    if (x1 > owf.ox + owf.ow) x1 = owf.ox + owf.ow;
    if (y0 < y1 && x0 == px && x1 == px + TILE && !(px & 1))
        ow_tile16(gba_back + y0 * (SCREEN_W / 2) + (px >> 1), src + (y0 - sy0) * TILE, y1 - y0);
    else if (y0 < y1 && x0 < x1)
        ow_blit(x0, x1, y0, y1, src + (y0 - sy0) * TILE + (x0 - px), TILE);
    *cx0 = x0;
    *cx1 = x1;
}

/* the height a raised cell's top is lifted by (= its front face's rows):
 * blocks 6..14 from ceil_h - floor_h (else BLOCK_H), raised floors
 * 2 * floor_h up to 8; 0 for a flat cell */
static int raise_h(const DoomoCell *c)
{
    if (c->wall)
        return c->ceil_h > c->floor_h ? DOOMO_CLAMP((c->ceil_h - c->floor_h) * 2, 6, 14) : BLOCK_H;
    return c->floor_h > 0 ? DOOMO_MIN(c->floor_h * 2, 8) : 0;
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
    if (gba_nodraw == GBA_NODRAW_MEASURE) {        /* G5: the canvas estimate */
        int a0 = DOOMO_MAX(x0, 0), a1 = DOOMO_MIN(x0 + w, SCREEN_W);
        int b0 = DOOMO_MAX(y0, 0), b1 = DOOMO_MIN(y0 + h + 1, SCREEN_H);
        if (a0 < a1 && b0 < b1) cv_note(a0, b0, a1, b1);
    }
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

int16_t ow_order[128];          /* billboards by feet y (ow_tiles.c too) */
#define order ow_order

static inline const DoomoCell *cell_at(const DoomoMap *m, int tx, int ty)
{
    if ((unsigned)tx < (unsigned)m->w && (unsigned)ty < (unsigned)m->h)
        return &m->cells[ty * m->w + tx];
    return map_cell_safe(m, tx, ty);
}

const DoomoCell *ow_cell(const DoomoMap *m, int tx, int ty) { return cell_at(m, tx, ty); }
int ow_cell_level(const DoomoCell *c) { return cell_level(c); }

/* G5: the tile mode is for frames that start with a full-screen overworld
 * (canvas_int.h); the engine-test ROM keeps Mode 4 (its test-et CRCs hash
 * the Mode 4 page) */
#if defined(ENGINE_TEST) || (defined(GBA_TILES) && !GBA_TILES)
#define OW_TILES 0                       /* (make TILES=0: Mode 4 only) */
#else
#define OW_TILES 1
#endif

void plat_overworld(const DoomoOverworldView *v)
{
    const DoomoMap *m;
    int ty0, ty1, ty, i, n, si, txa, pxa, nt, mode;
    uint32_t rmask[SCREEN_H / TILE + 5];   /* (G2) raised cells per row (bit k: txa + k) */
    OwLuts luts;                     /* (G2) this frame's sprite tables */
    OwMemo fm, tm;                   /* (G2) block fronts, block tops */
    if (gba_nodraw == 1 || !v) return;
    if (gba_nodraw == GBA_NODRAW_CANVAS) {      /* a second world this frame */
        cv_need_m4();
        if (gba_nodraw == 1) return;
    }
    gba_big_claim(GBA_BIG_BY_OW);
    m = v->map;
    owf.ox = v->vw > 0 ? v->vx : 0;
    owf.oy = v->vh > 0 ? v->vy : 0;
    owf.ow = v->vw > 0 ? v->vw : SCREEN_W;
    owf.oh = v->vh > 0 ? v->vh : SCREEN_H;
    if (owf.ox < 0) { owf.ow += owf.ox; owf.ox = 0; }
    if (owf.oy < 0) { owf.oh += owf.oy; owf.oy = 0; }
    if (owf.ox + owf.ow > SCREEN_W) owf.ow = SCREEN_W - owf.ox;
    if (owf.oy + owf.oh > SCREEN_H) owf.oh = SCREEN_H - owf.oy;
    /* G5: Mode 0 world + canvas, or Mode 4 (canvas_int.h) */
    mode = cv_ow_begin(OW_TILES && m && owf.ox == 0 && owf.oy == 0 && owf.ow == SCREEN_W &&
                       owf.oh == SCREEN_H && gba_big_fits(GBA_BIG_OW));
    cache_fit(mode != CV_OFF);
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
    {
        /* (G2) no VRAM tile copies yet this frame (only the sets in use:
         * beside a large scratch claim the lower half is the scene's) */
        uint32_t set;
        int w;
        lc.vused = 0;
        for (set = lc.hi; set < CACHE_SETS; set++)
            for (w = 0; w < CACHE_WAYS; w++) cache[set][w].vslot = cache[set][w].uses = 0;
    }

    /* horizontal scroll in 2-pixel steps: every row copy is then halfword
     * aligned with VRAM (DMA / halfword copies, no byte shuffling) */
    owf.ox &= ~1;
    owf.scrollx = ((int)((v->cx >> 16) / UNITS_PER_PX) - owf.ow / 2) & ~1;
    owf.scrolly = (int)((v->cy >> 16) / UNITS_PER_PX) - owf.oh / 2;
    ty0 = (owf.scrolly >= 0 ? owf.scrolly / TILE : (owf.scrolly - TILE + 1) / TILE) - 1;
    ty1 = ty0 + owf.oh / TILE + 4;

    if (mode != CV_OFF) {
        /* the world is BG2 (ow_tiles.c), the billboards go to the canvas */
        if (ow_tiles_world(m)) {
            ow_tiles_sprites(v, ty0, ty1);
            return;
        }
        cv_ow_failed();                         /* this frame is Mode 4 */
    }

    /* (G2) the sprites' pixel classes and their composed tables (ow_int.h) */
    {
        int present = 0, nc = 0, c;
        for (i = 0; i < 16; i++) {
            c = (ow_bayer[i] < owf.night) | ((ow_bayer[i] < owf.tint_level) << 1);
            owf.cls[i] = (uint8_t)c;
            present |= 1 << c;
        }
        for (c = 0; c < 4; c++) owf.slot[c] = (uint8_t)(((present >> c) & 1) ? nc++ : 0xFF);
        owf.nc = (uint8_t)nc;
        luts.ways = (uint8_t)(OW_LUT_TABLES / nc);
        luts.victim = 0;
        memset(luts.key, 0, sizeof(luts.key));
        owf.luts = &luts;
    }

    /* ground: every visible cell's lit tile (16-pixel block copies; the
     * clipped edge tiles go through ow_blit). Blocks and raised floors
     * are skipped: their lifted top plus front face cover the whole cell
     * (drawn in the next pass). (G2) One scan of the visible columns per
     * row, through the row's cells: a run of equal cells reuses one tile
     * lookup, and the raised cells are noted (rmask) for the next pass,
     * which then visits only them. The scan covers every row the next
     * pass draws in: a lifted top shows up to 16 px above its cell. */
    txa = owf.scrollx >= 0 ? owf.scrollx / TILE : (owf.scrollx - TILE + 1) / TILE;
    pxa = owf.ox + txa * TILE - owf.scrollx;     /* <= ox, even */
    nt = (owf.ox + owf.ow - pxa + TILE - 1) / TILE;
    {
        OwMemo gm;
        gm.t = 0;
        for (ty = ty0; ty <= ty1; ty++) {
            const int py = owf.oy + ty * TILE - owf.scrolly;
            const int ya = DOOMO_MAX(py, owf.oy), yb = DOOMO_MIN(py + TILE, owf.oy + owf.oh);
            const DoomoCell *row = (unsigned)ty < (unsigned)m->h ? m->cells + ty * m->w : 0;
            uint32_t mask = 0;
            int k, px;
            rmask[ty - ty0] = 0;
            if (py - 16 >= owf.oy + owf.oh || py + TILE <= owf.oy) continue;
            for (k = 0, px = pxa; k < nt; k++, px += TILE) {
                const int tx = txa + k;
                const DoomoCell *c = row && (unsigned)tx < (unsigned)m->w ? row + tx
                                                                         : map_cell_safe(m, tx, ty);
                const uint8_t *t;
                int lvl, x0, x1;
                if (c->wall || c->floor_h > 0) { mask |= 1u << k; continue; }
                if (ya >= yb) continue;
                lvl = cell_level(c);
                if (c->floor_h < 0) lvl -= 3;
                t = tile_src(lit_memo(&gm, c->floor_tex, lvl, 0));
                x0 = DOOMO_MAX(px, owf.ox);
                x1 = DOOMO_MIN(px + TILE, owf.ox + owf.ow);
                if (x0 == px && x1 == px + TILE)
                    ow_tile16(gba_back + ya * (SCREEN_W / 2) + (px >> 1), t + (ya - py) * TILE,
                              yb - ya);
                else
                    ow_blit(x0, x1, ya, yb, t + (ya - py) * TILE + (x0 - px), TILE);
            }
            rmask[ty - ty0] = mask;
        }
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
    fm.t = tm.t = 0;
    for (ty = ty0; ty <= ty1; ty++) {
        int32_t row_bottom = (int32_t)(ty + 1) * DOOMO_CELL_UNITS;
        int sy = owf.oy + ty * TILE - owf.scrolly + TILE - 1;   /* contact lines */
        int sy_on = sy >= owf.oy && sy < owf.oy + owf.oh;
        int rx0 = 0, rx1 = 0, cx0, cx1;
        uint32_t mask = rmask[ty - ty0];
        int k;
        for (k = 0; mask; k++, mask >>= 1) {
            const int tx = txa + k;
            const DoomoCell *c;
            int px = pxa + k * TILE, py = owf.oy + ty * TILE - owf.scrolly;
            int lvl, h;
            if (!(mask & 1)) continue;
            c = cell_at(m, tx, ty);
            lvl = cell_level(c);
            h = raise_h(c);
            /* (G2) a front face the next row's top covers (the cell below is
             * raised at least as high: its top spans rows py + 16 - h' ..
             * py + 32 - h' of this column, h' <= 14, and is drawn after
             * everything in between) is skipped, with its contact line.
             * Whatever lands there meanwhile, this row's sprites included,
             * is overwritten by that opaque top, as before. */
            cx0 = cx1 = 0;
            if (raise_h(cell_at(m, tx, ty + 1)) < h)
                draw_front(px, py, c->wall ? c->wall : c->floor_tex, lvl, h, &cx0, &cx1, &fm);
            if (c->wall) draw_tile(px, py, c->floor_tex ? c->floor_tex : c->wall, lvl + 1, h, &tm);
            else draw_tile(px, py, c->floor_tex, lvl + 1, h, &tm);
            /* this block's contact line joins the run, or flushes it */
            if (cx0 < cx1 && sy_on) {
                if (cx0 != rx1) {
                    if (rx0 < rx1) ow_shade(rx0, rx1, sy, sy + 1, doomo_colormap[3]);
                    rx0 = cx0;
                }
                rx1 = cx1;
            }
        }
        if (rx0 < rx1)
            ow_shade(rx0, rx1, sy, sy + 1, doomo_colormap[3]);
        while (si < n && (v->sprites[order[si]].y >> 16) < row_bottom) {
            draw_sprite(&v->sprites[order[si]]);
            si++;
        }
    }
    while (si < n) draw_sprite(&v->sprites[order[si++]]);
    owf.luts = 0;                    /* the tables die with this frame */
}
