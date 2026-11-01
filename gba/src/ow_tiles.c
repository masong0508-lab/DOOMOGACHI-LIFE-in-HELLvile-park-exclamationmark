/* ow_tiles.c - the world of a tile-mode overworld frame (owner: gba; G5,
 * canvas_int.h, docs/GBA.md section 6b).
 *
 * BG2 shows the world as 16x16 cell composites: what the Mode 4 renderer
 * (overworld.c) leaves in a cell's 16x16 screen square before any
 * billboard is drawn. That is a function of the cell and the cell below
 * it only:
 *   - ground (no wall, floor_h <= 0): its lit tile (pits 3 levels darker)
 *   - raised (a wall, or floor_h > 0): its top's rows h..15 (the top is
 *     lifted by h), then the front face (h rows at the bottom, 4 levels
 *     darker) with the dark contact line on its last row
 *   - then, if the cell below is raised by hb, that cell's lifted top
 *     covers this cell's bottom hb rows (drawn later in Mode 4: rows go
 *     top to bottom)
 * The pieces come from lit_cached() (ow_lit), so a composite is the Mode 4
 * pixels byte for byte, dither included (it is anchored to the cell).
 * Composites are cached in VRAM (36 slots of 4 tiles); a slot the screen
 * shows (used by the last shown frame) is never rewritten.
 *
 * The billboards (ow_obj.c, G6) are OBJs over this world, in Mode 4
 * order. In Mode 4 a billboard is drawn right after the raised row its
 * feet are in, so the raised cells of the rows below it (their top and
 * front: x [px, px+16), y [py - h, py + 16)) paint over it; in its OBJ
 * tiles (or in the canvas, for one that gets no OBJ) those parts are
 * cleared (occluders: the lift grid below), which leaves exactly the Mode
 * 4 pixels.
 */
#include "canvas_int.h"
#include "ow_int.h"
#include "doomo/textures.h"
#include "doomo/palette.h"
#include "doomo/map.h"

#define TILE 16
#define UNITS_PER_PX (DOOMO_CELL_UNITS / TILE)

/* a cell's lift (raised: front face height = top lift), 0 = ground */
static int cell_lift(const DoomoCell *c)
{
    if (c->wall)
        return c->ceil_h > c->floor_h ? DOOMO_CLAMP((c->ceil_h - c->floor_h) * 2, 6, 14) : OW_BLOCK_H;
    if (c->floor_h > 0) return DOOMO_MIN(c->floor_h * 2, 8);
    return 0;
}

static int clamp_lvl(int l) { return l < 0 ? 0 : l > 15 ? 15 : l; }

/* the composite's description (what lit tiles it is made of) */
typedef struct CompDesc {
    int tex_a, lvl_a, h;           /* ground (h = 0) or front face     */
    int tex_t, lvl_t;              /* raised: the top                  */
    int tex_b, lvl_b, hb;          /* the raised cell below: its top   */
} CompDesc;

static void describe(const DoomoMap *m, int cx, int cy, CompDesc *d)
{
    const DoomoCell *c = ow_cell(m, cx, cy), *b = ow_cell(m, cx, cy + 1);
    int lvl = ow_cell_level(c);
    d->h = cell_lift(c);
    if (!d->h) {
        d->tex_a = c->floor_tex;
        d->lvl_a = clamp_lvl(c->floor_h < 0 ? lvl - 3 : lvl);
        d->tex_t = 0; d->lvl_t = 0;
    } else {
        d->tex_a = c->wall ? c->wall : c->floor_tex;
        d->lvl_a = DOOMO_CLAMP(lvl - 4, 1, 15);
        d->tex_t = c->wall ? (c->floor_tex ? c->floor_tex : c->wall) : c->floor_tex;
        d->lvl_t = clamp_lvl(lvl + 1);
    }
    d->hb = cell_lift(b);
    if (d->hb) {
        d->tex_b = b->wall ? (b->floor_tex ? b->floor_tex : b->wall) : b->floor_tex;
        d->lvl_b = clamp_lvl(ow_cell_level(b) + 1);
    } else {
        d->tex_b = 0; d->lvl_b = 0;
    }
}

static void unlink_slot(CvMeta *M, int s)
{
    CvComp *c = &M->comp[s];
    int8_t *p;
    if (!c->live) return;
    p = &M->hash[c->h];
    while (*p >= 0) {
        if (*p == s) { *p = c->next; break; }
        p = &M->comp[(int)*p].next;
    }
    c->live = 0;
}

/* the 16x16 pixels into VRAM slot s (4 tiles, quadrant q = qy * 2 + qx) */
static void build(int s, const CompDesc *d)
{
    uint32_t pix[TILE * TILE / 4];
    uint8_t *p = (uint8_t *)pix;
    int y, x;
    if (!d->h) {
        memcpy(p, ow_lit(d->tex_a, d->lvl_a, 0), TILE * TILE);
    } else {
        const uint8_t *cm3 = doomo_colormap[3];
        /* the top: tile rows h..15 at cell rows 0..15-h */
        memcpy(p, ow_lit(d->tex_t, d->lvl_t, 0) + d->h * TILE, (size_t)(TILE - d->h) * TILE);
        /* the front face: its rows 0..h-1 at cell rows 16-h..15, the
         * contact line darkening its last row */
        memcpy(p + (TILE - d->h) * TILE, ow_lit(d->tex_a, d->lvl_a, d->h), (size_t)d->h * TILE);
        for (x = 0; x < TILE; x++) p[(TILE - 1) * TILE + x] = cm3[p[(TILE - 1) * TILE + x]];
    }
    if (d->hb) /* the lifted top of the cell below: its rows 0..hb-1 */
        memcpy(p + (TILE - d->hb) * TILE, ow_lit(d->tex_b, d->lvl_b, 0), (size_t)d->hb * TILE);
    {
        volatile uint32_t *dst = (volatile uint32_t *)(CV_VRAM + CV_COMP_TILE(s) * 64u);
        int q;
        for (q = 0; q < 4; q++)
            for (y = 0; y < 8; y++) {
                const uint32_t *row = pix + ((q >> 1) * 8 + y) * (TILE / 4) + (q & 1) * 2;
                *dst++ = row[0];
                *dst++ = row[1];
            }
    }
}

/* cv_world_map's cache miss: a slot neither the shown frame (cv.frame - 1)
 * nor this one uses, keyed k (hash h), built; -1 = none free */
int ow_comp_miss(const DoomoMap *m, int cx, int cy, const uint32_t *k, uint32_t h)
{
    CvMeta *M = cv_meta;
    CompDesc d;
    int s, best = -1;
    for (s = 0; s < CV_COMP_N; s++) {
        const CvComp *c = &M->comp[s];
        if (!c->live) { best = s; break; }
        if (c->last + 1u >= cv.frame) continue;
        if (best < 0 || c->last < M->comp[best].last) best = s;
    }
    if (best < 0) return -1;
    unlink_slot(M, best);
    memcpy(M->comp[best].k, k, sizeof(M->comp[best].k));
    M->comp[best].last = cv.frame;
    M->comp[best].live = 1;
    M->comp[best].h = (uint8_t)h;
    M->comp[best].next = M->hash[h];
    M->hash[h] = (int8_t)best;
    describe(m, cx, cy, &d);
    build(best, &d);
    return best;
}

/* ---- the world map pass ------------------------------------------------------ *
 * Every visible cell's composite slot into the map shadow and the lift grid;
 * a cache miss builds the composite (ow_comp_miss). THUMB in ROM: a frame
 * whose cells, scroll origin and key state match the last pass's signature
 * only checks that. 0 = a miss found no free slot. */
static inline uint32_t khash(const uint32_t *k)
{
    uint32_t h = k[0] ^ (k[1] * 3u) ^ (k[2] * 5u) ^ (k[3] * 7u) ^ (k[4] * 0x9E3779B1u) ^ (k[5] * 0x85EBCA6Bu);
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    return (h >> 26) & 63;
}

static inline const DoomoCell *cellp(const DoomoMap *m, int x, int y)
{
    if ((unsigned)x < (unsigned)m->w && (unsigned)y < (unsigned)m->h) return &m->cells[y * m->w + x];
    return ow_cell(m, x, y);
}

/* overworld.c: front face height = top lift of a raised cell, 0 = ground */
static inline int lift_of(const DoomoCell *c)
{
    if (c->wall) {
        int h = (c->ceil_h - c->floor_h) * 2;
        if (c->ceil_h <= c->floor_h) return 10;   /* OW_BLOCK_H */
        return h < 6 ? 6 : h > 14 ? 14 : h;
    }
    if (c->floor_h > 0) return c->floor_h * 2 < 8 ? c->floor_h * 2 : 8;
    return 0;
}

static inline uint32_t raw0(const DoomoCell *c)
{
    return (uint32_t)c->wall | ((uint32_t)c->floor_tex << 8) | ((uint32_t)c->ceil_tex << 16) |
           ((uint32_t)(uint8_t)c->floor_h << 24);
}
static inline uint32_t raw1(const DoomoCell *c)
{
    return (uint32_t)(uint8_t)c->ceil_h | ((uint32_t)c->light << 8) | ((uint32_t)c->flags << 16) |
           ((uint32_t)c->tag << 24);
}
static inline int animated(const CvMeta *M, unsigned t)
{
    if (t >= 64) t = 0;
    return (M->anim_mask[t >> 5] >> (t & 31)) & 1;
}

static int world_map(const DoomoMap *m, uint32_t g)
{
    CvMeta *M = cv_meta;
    const int mx0 = cv.sx >> 3, my0 = cv.sy >> 3;
    const int cx0 = mx0 >> 1, cy0 = my0 >> 1, cx1 = (mx0 + 30) >> 1, cy1 = (my0 + CV_WMAP_H - 1) >> 1;
    int cx, cy;
    uint32_t sa = 2166136261u ^ g, sb = (uint32_t)mx0 * 65599u + (uint32_t)my0 + M->anim_epoch * 31u;
    uint32_t used0 = 0, used1 = 0;
    /* the signature of everything the map pass reads: the cells (and the
     * row below), the scroll origin, the key state. Unchanged (the camera
     * moved less than a tile, nothing changed): the last pass stands; its
     * slots are this frame's too. */
    for (cy = cy0; cy <= cy1 + 1; cy++) {
        if (cy >= 0 && cy < m->h && cx0 >= 0 && cx1 < m->w && !((uintptr_t)m->cells & 3)) {
            /* in the map: the row's cells as aligned words (8 B a cell) */
            const uint32_t *w = (const uint32_t *)(const void *)&m->cells[cy * m->w + cx0];
            int n = (cx1 - cx0 + 1) * 2, k;
            for (k = 0; k < n; k++) {
                sa = (sa ^ w[k]) * 16777619u;
                sb = sb * 33u + w[k];
            }
            continue;
        }
        for (cx = cx0; cx <= cx1; cx++) {
            const DoomoCell *c = cellp(m, cx, cy);
            uint32_t r0 = raw0(c), r1 = raw1(c);
            sa = (sa ^ r0) * 16777619u;
            sa = (sa ^ r1) * 16777619u;
            sb = sb * 33u + r0 + (r1 << 3);
        }
    }
    if (M->wvalid && M->wsig[0] == sa && M->wsig[1] == sb) {
        for (cx = 0; cx < CV_COMP_N; cx++)
            if ((cx < 32 ? M->wused[0] >> cx : M->wused[1] >> (cx - 32)) & 1) M->comp[cx].last = cv.frame;
        return 1;
    }
    M->wvalid = 0;
    M->gx0 = (int16_t)cx0;
    M->gy0 = (int16_t)cy0;
    M->gw = (uint8_t)(cx1 - cx0 + 1);
    M->gh = (uint8_t)(cy1 - cy0 + 2);
    for (cy = cy0; cy <= cy1 + 1; cy++)
        for (cx = cx0; cx <= cx1; cx++)
            M->lift[cy - cy0][cx - cx0] = (uint8_t)lift_of(cellp(m, cx, cy));
    for (cy = cy0; cy <= cy1; cy++)
        for (cx = cx0; cx <= cx1; cx++) {
            const DoomoCell *c = cellp(m, cx, cy);
            uint32_t k[6], h, t;
            int s, q, an;
            k[0] = raw0(c);
            k[1] = raw1(c);
            an = animated(M, c->wall) | animated(M, c->floor_tex);
            if (M->lift[cy - cy0 + 1][cx - cx0]) {
                const DoomoCell *b = cellp(m, cx, cy + 1);
                k[2] = raw0(b);
                k[3] = raw1(b);
                an |= animated(M, b->wall) | animated(M, b->floor_tex);
            } else {
                k[2] = k[3] = 0;
            }
            k[4] = g;
            k[5] = an ? M->anim_epoch : 0;
            h = khash(k);
            for (s = M->hash[h]; s >= 0; s = M->comp[s].next) {
                const uint32_t *ck = M->comp[s].k;
                if (ck[0] == k[0] && ck[1] == k[1] && ck[2] == k[2] && ck[3] == k[3] && ck[4] == k[4] &&
                    ck[5] == k[5])
                    break;
            }
            if (s >= 0) M->comp[s].last = cv.frame;
            else if ((s = ow_comp_miss(m, cx, cy, k, h)) < 0) return 0;
            if (s < 32) used0 |= 1u << s;
            else used1 |= 1u << (s - 32);
            t = CV_COMP_TILE(s);
            for (q = 0; q < 4; q++) {
                int wx = 2 * cx + (q & 1) - mx0, wy = 2 * cy + (q >> 1) - my0;
                if ((unsigned)wx < 31u && (unsigned)wy < (unsigned)CV_WMAP_H)
                    M->wmap[wy * CV_WMAP_W + wx] = (uint16_t)(t + (uint32_t)q);
            }
        }
    for (cy = 0; cy < CV_WMAP_H; cy++) M->wmap[cy * CV_WMAP_W + 31] = CV_ZERO_TILE;
    M->wsig[0] = sa;
    M->wsig[1] = sb;
    M->wused[0] = used0;
    M->wused[1] = used1;
    M->wvalid = 1;
    return 1;
}


DOOMO_STATIC_ASSERT(sizeof(CvMeta) <= CV_META_MAX, cv_meta_fits_the_lower_half);

int ow_tiles_world(const DoomoMap *m)
{
    CvMeta *M = cv_meta;
    int i;
    if (!cv.meta_ok) {
        if (cv.mode != CV_SPARSE) return 0;     /* the shown slots are unknown */
        for (i = 0; i < CV_COMP_N; i++) { M->comp[i].live = 0; M->comp[i].last = 0; M->comp[i].next = -1; }
        for (i = 0; i < 64; i++) M->hash[i] = -1;
        M->anim_mask[0] = M->anim_mask[1] = 0;
        for (i = 1; i < TEX_COUNT; i++)
            if (texture_is_animated(i)) {
                M->anim_mask[i >> 5] |= 1u << (i & 31);
                M->anim_px[i] = 0;
            }
        M->anim_epoch = 1;
        M->wvalid = 0;                          /* the slots are gone: no skip */
        cv.meta_ok = 1;
    }
    /* animated textures: a new frame of any of them is a new epoch (the
     * composites that use one are keyed by it) */
    {
        int changed = 0;
        for (i = 1; i < TEX_COUNT; i++)
            if ((M->anim_mask[i >> 5] >> (i & 31)) & 1) {
                const uint8_t *px = texture_pixels(i, owf.time);
                if (px != M->anim_px[i]) { M->anim_px[i] = px; changed = 1; }
            }
        if (changed) M->anim_epoch++;
    }
    cv.sx = (int16_t)owf.scrollx;
    cv.sy = (int16_t)owf.scrolly;
    /* the rest of the key: daylight (cell levels) and the night / tint
     * dither state of the lit tiles */
    return world_map(m, ow_tint_state() | ((uint32_t)owf.day_level << 16) | 0x80000000u);
}
/* ---- billboards: ow_obj.c (G6) ---- */
