/* show_stage.c - the plane table, the core plane loop and lane 1's plane
 * kinds BANDS, IMAGE, GRAIN and FILL (owner: ENGINE lane 1; spec 8.3, 2.3).
 *
 * The shot's SHWR_PLANE record is indexed at bind time and sorted by
 * `order` (stable). Every frame, with drawing on, the core walks it and
 * calls the plane kind's draw from whichever module registered the kind
 * (show_api.h ShowPlaneKind). Planes go through show_plane_xf() for their
 * position and scale, so the stage camera (lane 2) moves them.
 *
 *   BANDS  a palette-step gradient as flat rects (never Gouraud: on-palette)
 *   FILL   one flat colour over the picture (+ overscan)
 *   IMAGE  a baked 8bpp plate from SHWR_IMAGE (raw or RLE), drawn with
 *          plat_image at a whole scale (nearest), tiled if asked
 *   GRAIN  a 64x64 two-tone paper tile (seeded dots), tiled over the picture;
 *          `boil` re-seeds it every N frames
 * Every plane is drawn with an 8 px overscan so a shake never shows an edge.
 */
#include <string.h>
#include "show_core.h"

#define PLANES_MAX   32
#define IMAGES_MAX   96
#define GRID_W       126                /* a grid plate's cells (emit.py GRID_W/H) */
#define GRID_H       120
#define OVERSCAN     8
#define IMG_ARENA    (224 * 1024)       /* decoded RLE plates, per shot (a 640x240 midway + layers) */

static const ShwPlane *s_planes[PLANES_MAX];
static int s_nplanes;
static uint32_t s_hidden;               /* bit per plane slot (draw order)   */

static DoomoImage s_img[IMAGES_MAX];
static int s_nimg;
static uint32_t s_arena[IMG_ARENA / 4];

void (*g_show_plane_xf_hook)(const ShwPlane *p, int px, ShowPlaneXf *out);
void (*g_show_draw_image_hook)(const DoomoImage *img, const ShowPlaneXf *xf, int flags);

/* ---- the plane table ---------------------------------------------------- */
void show_stage_bind(void)
{
    const uint8_t *p;
    uint32_t bytes;
    int n, i, j;
    s_nplanes = 0;
    s_hidden = 0;
    p = (const uint8_t *)show_chunk_find(SHWR_PLANE, &n, &bytes);
    for (i = 0; p && i < n && s_nplanes < PLANES_MAX; i++) {
        const ShwPlane *pl = (const ShwPlane *)p;
        if (pl->bytes < sizeof(ShwPlane)) break;
        s_planes[s_nplanes++] = pl;
        p += (pl->bytes + 3u) & ~3u;
    }
    /* stable insertion sort by order */
    for (i = 1; i < s_nplanes; i++) {
        const ShwPlane *k = s_planes[i];
        for (j = i - 1; j >= 0 && s_planes[j]->order > k->order; j--)
            s_planes[j + 1] = s_planes[j];
        s_planes[j + 1] = k;
    }
    for (i = 0; i < s_nplanes; i++)
        if (s_planes[i]->flags & SHWPF_HIDDEN) s_hidden |= 1u << i;
}

int show_plane_count(void) { return s_nplanes; }
const ShwPlane *show_plane(int i) { return (i >= 0 && i < s_nplanes) ? s_planes[i] : NULL; }
const void *show_plane_payload(const ShwPlane *p) { return p ? (const void *)(p + 1) : NULL; }
uint32_t show_plane_vis_mask(void) { return ~s_hidden; }

static int slot_of(int id)
{
    int i;
    for (i = 0; i < s_nplanes; i++)
        if (s_planes[i]->id == id) return i;
    return -1;
}

const ShwPlane *show_plane_by_id(int id) { int s = slot_of(id); return s < 0 ? NULL : s_planes[s]; }
int show_plane_visible(int id) { int s = slot_of(id); return s >= 0 && !(s_hidden & (1u << s)); }
void show_plane_set_visible(int id, int on)
{
    int s = slot_of(id);
    if (s < 0) return;
    if (on) s_hidden &= ~(1u << s); else s_hidden |= 1u << s;
}

static const ShowPlaneKind *kind_of(int kind)
{
    int m, i, n = show_module_count();
    for (m = 0; m < n; m++) {
        const ShowPlaneKind *k = show_module(m)->planes;
        for (i = 0; k && k[i].kind; i++)
            if (k[i].kind == kind) return &k[i];
    }
    return NULL;
}

void show_stage_prep(void)
{
    int i;
    for (i = 0; i < s_nplanes; i++) {
        const ShowPlaneKind *k = kind_of(s_planes[i]->kind);
        if (!k) {
            show_warn("STAGE-KIND", "a plane kind with no host linked: not drawn");
            continue;
        }
        if (k->prep) k->prep(s_planes[i], show_plane_payload(s_planes[i]));
    }
}

void show_stage_draw(int t)
{
    int i;
    for (i = 0; i < s_nplanes; i++) {
        const ShowPlaneKind *k;
        if (s_hidden & (1u << i)) continue;
        k = kind_of(s_planes[i]->kind);
        if (k && k->draw) {
            const void *pay = show_plane_payload(s_planes[i]);
            if (!show_amb_plane_draw(s_planes[i], pay, t, k->draw)) k->draw(s_planes[i], pay, t);
            show_amb_after_plane(s_planes[i], t);           /* show_amb.c: flips, motes, lamps */
        }
    }
}

/* ---- transform and image drawing (core defaults) ------------------------ */
void show_plane_xf(const ShwPlane *p, ShowPlaneXf *out)
{
    int px = g_show_shot.px > 0 ? g_show_shot.px : 1;
    if (g_show_plane_xf_hook) {
        g_show_plane_xf_hook(p, px, out);
    } else {
        out->ox = p ? p->at_x : 0;
        out->oy = p ? p->at_y : 0;
        out->fx = out->fy = 0;
        out->s = px << 16;
    }
    show_amb_plane_off(p, out);                             /* show_amb.c: drift, sway, wave */
}

void show_draw_image(const DoomoImage *img, const ShowPlaneXf *xf, int flags)
{
    int s;
    if (!img) return;
    if (g_show_draw_image_hook) { g_show_draw_image_hook(img, xf, flags); return; }
    s = (xf->s + 0x8000) >> 16;
    if (s < 1) s = 1;
    if (s > 4) s = 4;
    plat_image(xf->ox, SHOW_Y(xf->oy), img, s, flags, 0);
    show_stat_add(SHOW_STAT_SPRITES, 1);
}

/* ---- images (SHWR_IMAGE) ------------------------------------------------- */
void show_images_prep(void)
{
    const uint8_t *p;
    uint32_t bytes, used = 0;
    int n, i;
    s_nimg = 0;
    p = (const uint8_t *)show_chunk_find(SHWR_IMAGE, &n, &bytes);
    for (i = 0; p && i < n && i < IMAGES_MAX; i++) {
        const ShwImage *im = (const ShwImage *)p;
        const uint8_t *data = p + sizeof(ShwImage);
        uint32_t px_n = (uint32_t)im->w * im->h;
        DoomoImage *d = &s_img[s_nimg++];
        d->w = im->w;
        d->h = im->h;
        d->uid = doomo_image_new_uid();
        d->pix = NULL;
        if (im->comp == SHWI_RAW) {
            d->pix = data;
        } else if (im->comp == SHWI_RLE && used + px_n <= IMG_ARENA) {
            uint8_t *o = (uint8_t *)s_arena + used, *oe = o + px_n;
            const uint8_t *q = data, *qe = p + im->bytes;
            d->pix = o;
            while (o < oe && q + 1 < qe) {
                int c = q[0];
                uint8_t v = q[1];
                q += 2;
                while (c-- > 0 && o < oe) *o++ = v;
            }
            while (o < oe) *o++ = 0;
            used += (px_n + 3u) & ~3u;
        } else {
            show_warn("SHW-IMAGE", "image does not fit the decode arena (or unknown comp)");
            d->w = d->h = 0;
        }
        p += (im->bytes + 3u) & ~3u;
    }
}

const DoomoImage *show_image(int i)
{
    return (i >= 0 && i < s_nimg && s_img[i].pix) ? &s_img[i] : NULL;
}
int show_image_count(void) { return s_nimg; }

/* ---- BANDS ---------------------------------------------------------------- */
static void bands_draw(const ShwPlane *p, const void *pay, int t)
{
    const ShwBands *b = (const ShwBands *)pay;
    const ShwBand *band = (const ShwBand *)(b + 1);
    ShowPlaneXf xf;
    int i, s;
    (void)t;
    show_plane_xf(p, &xf);
    s = xf.s >> 16;
    if (s < 1) s = 1;
    for (i = 0; i < b->n; i++) {
        int y0 = xf.oy + band[i].y * s;
        int y1 = (i + 1 < b->n) ? xf.oy + band[i + 1].y * s : SHOW_CANVAS_H + OVERSCAN;
        if (i == 0 && y0 > -OVERSCAN) y0 = -OVERSCAN;        /* cover the top edge */
        if (y0 < -OVERSCAN) y0 = -OVERSCAN;
        if (y1 > SHOW_CANVAS_H + OVERSCAN) y1 = SHOW_CANVAS_H + OVERSCAN;
        if (y1 > y0)
            plat_rect(-OVERSCAN, SHOW_Y(y0), SHOW_CANVAS_W + 2 * OVERSCAN, y1 - y0, band[i].color);
    }
}

/* ---- FILL ----------------------------------------------------------------- */
static void fill_draw(const ShwPlane *p, const void *pay, int t)
{
    (void)p; (void)t;
    plat_rect(-OVERSCAN, SHOW_Y(-OVERSCAN), SHOW_CANVAS_W + 2 * OVERSCAN,
              SHOW_CANVAS_H + 2 * OVERSCAN, ((const ShwFill *)pay)->color);
}

/* ---- IMAGE ---------------------------------------------------------------- */
/* a big plate as row-major GRID_W x GRID_H cells: flip = 0x80 | cols,
 * scale = px | rows << 3 (cells pack 4 to a VRAM page; a 128 tile takes one) */
static void grid_draw(const ShwPlane *p, const ShwImagePlane *ip, ShowPlaneXf xf)
{
    int cols = ip->flip & 0x7F, rows = ip->scale >> 3, r, c, s, x0, tw, k;
    const DoomoImage *last;
    if ((ip->scale & 7) != 0) xf.s = (int32_t)(ip->scale & 7) << 16;
    s = (xf.s + 0x8000) >> 16;
    if (s < 1) s = 1;
    last = show_image(ip->image + cols - 1);
    tw = ((cols - 1) * GRID_W + (last ? last->w : GRID_W)) * s;     /* the whole plate's width */
    x0 = xf.ox;
    if ((p->flags & SHWPF_TILE_X) && tw > 0) { x0 %= tw; if (x0 > -OVERSCAN) x0 -= tw; }
    for (k = 0; k < 4; k++, x0 += tw) {
        if (x0 >= SHOW_CANVAS_W + OVERSCAN) break;
        for (r = 0; r < rows; r++)
            for (c = 0; c < cols; c++) {
                ShowPlaneXf t2 = xf;
                int y = xf.oy + r * GRID_H * s, x = x0 + c * GRID_W * s;
                if (y >= SHOW_CANVAS_H + OVERSCAN || y + GRID_H * s <= -OVERSCAN ||
                    x >= SHOW_CANVAS_W + OVERSCAN || x + GRID_W * s <= -OVERSCAN) continue;
                const DoomoImage *cell = show_image(ip->image + r * cols + c);
                if (!cell || cell->w <= 1) continue;       /* an empty (hidden) cell */
                t2.ox = x;
                t2.oy = y;
                show_draw_image(cell, &t2, 0);
            }
        if (!(p->flags & SHWPF_TILE_X)) break;
    }
}

static void image_draw(const ShwPlane *p, const void *pay, int t)
{
    const ShwImagePlane *ip = (const ShwImagePlane *)pay;
    const DoomoImage *img = show_image(ip->image);
    ShowPlaneXf xf;
    int s, x0, y0, x, y, w, h;
    (void)t;
    if (!img) return;
    show_plane_xf(p, &xf);
    if (ip->flip & 0x80) { grid_draw(p, ip, xf); return; }
    if (ip->scale) xf.s = (int32_t)ip->scale << 16;
    s = (xf.s + 0x8000) >> 16;
    if (s < 1) s = 1;
    w = img->w * s;
    h = img->h * s;
    if (!(p->flags & (SHWPF_TILE_X | SHWPF_TILE_Y)) || w <= 0 || h <= 0) {
        show_draw_image(img, &xf, ip->flip ? IMG_FLIPX : 0);
        return;
    }
    x0 = xf.ox; y0 = xf.oy;
    if (p->flags & SHWPF_TILE_X) { x0 %= w; if (x0 > -OVERSCAN) x0 -= w; }
    if (p->flags & SHWPF_TILE_Y) { y0 %= h; if (y0 > -OVERSCAN) y0 -= h; }
    for (y = y0; y < SHOW_CANVAS_H + OVERSCAN; y += h) {
        for (x = x0; x < SHOW_CANVAS_W + OVERSCAN; x += w) {
            ShowPlaneXf t2 = xf;
            t2.ox = x; t2.oy = y;
            show_draw_image(img, &t2, ip->flip ? IMG_FLIPX : 0);
            if (!(p->flags & SHWPF_TILE_X)) break;
        }
        if (!(p->flags & SHWPF_TILE_Y)) break;
    }
}

/* ---- GRAIN ---------------------------------------------------------------- */
#define GRAIN_N 64
static uint8_t s_grain_pix[GRAIN_N * GRAIN_N];
static DoomoImage s_grain = { GRAIN_N, GRAIN_N, s_grain_pix, 0 };
static uint32_t s_grain_key = 0xFFFFFFFFu;

static void grain_make(const ShwGrain *g, uint32_t seed)
{
    uint32_t x = seed ? seed : 0x9E3779B9u;
    int i, dots = g->density * 16;
    memset(s_grain_pix, 0, sizeof s_grain_pix);
    for (i = 0; i < dots; i++) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        s_grain_pix[(x >> 8) & (GRAIN_N * GRAIN_N - 1)] = g->color;
    }
    s_grain.uid = doomo_image_new_uid();
}

static void grain_draw(const ShwPlane *p, const void *pay, int t)
{
    const ShwGrain *g = (const ShwGrain *)pay;
    ShowPlaneXf xf;
    int x, y, x0, y0, step = g->boil ? (t >= 0 ? t / g->boil : 0) : 0;
    uint32_t key = show_hash32(g->seed ^ (uint32_t)step * 0x85EBCA6Bu ^ g->color << 24 ^ g->density);
    if (key != s_grain_key) { grain_make(g, key); s_grain_key = key; }
    show_plane_xf(p, &xf);
    x0 = xf.ox % GRAIN_N; if (x0 > -OVERSCAN) x0 -= GRAIN_N;
    y0 = xf.oy % GRAIN_N; if (y0 > -OVERSCAN) y0 -= GRAIN_N;
    for (y = y0; y < SHOW_CANVAS_H + OVERSCAN; y += GRAIN_N)
        for (x = x0; x < SHOW_CANVAS_W + OVERSCAN; x += GRAIN_N) {
            plat_image(x, SHOW_Y(y), &s_grain, 1, 0, 0);
            show_stat_add(SHOW_STAT_SPRITES, 1);
        }
}

static void stage_reset(void)
{
    s_grain_key = 0xFFFFFFFFu;          /* a new uid per shot: never stale */
}

static const ShowPlaneKind k_kinds[] = {
    { SHWP_BANDS, "bands", NULL, bands_draw },
    { SHWP_FILL,  "fill",  NULL, fill_draw },
    { SHWP_IMAGE, "image", NULL, image_draw },
    { SHWP_GRAIN, "grain", NULL, grain_draw },
    { 0, NULL, NULL, NULL }
};

const ShowModule show_mod_stage = {
    "stage", SHOW_API_VERSION, 0, stage_reset, NULL, NULL, NULL, NULL, NULL, k_kinds
};
