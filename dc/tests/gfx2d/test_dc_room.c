/* test_dc_room.c - the Dreamcast house (DC8, dc/src/room.c) on the host
 * (owner: dreamcast). Built like test_dc2d.c / test_dc3d.c: the native
 * runner's 8bpp frame is the reference (core/src/room_ref.c draws it), the
 * Dreamcast list is rendered by the software PVR (swpvr.c) at 640x480, and
 * every logical pixel's 2x2 block is compared with the native pixel in
 * RGB565.
 *
 *   parity     every golden frame of core/tests/room_shells.h (the classic
 *              layout in 4 views and at night, every wallpaper, floor,
 *              ceiling ramp, tint, room kind, roof, fence, lamp pool) at
 *              320x240: identical to room_ref, pixel for pixel
 *   catalog    every piece of the catalog alone in a room, in the 4 views
 *              (H9's back facings, the door and window skins) by day and at
 *              night (glow, panes): identical
 *   dynamic    actors (flipped, squashed, tinted), the cursor, the rubber
 *              band, half-tile smalls, a jitter, pans: identical (squashes
 *              excepted: one quad on the Dreamcast, as plat_image_xform)
 *   boxes      no plat_cache: the shaded boxes, identical
 *   effects    ghosts are 50% GLASS prims (blended pixels), invalid ghosts
 *              red, the blueprint ghost grey, translucent residents GLASS,
 *              the selected piece an ADD glow that pulses
 *   sort       Kahn's order (dc_room_sort) equals room_ref's O(n^3) pick on
 *              random layouts, cycles included
 *   cache      a view at rest decodes nothing and renders no shell; the
 *              same frame twice gives the same prims, byte for byte; a
 *              light, a lamp or a turn renders a new shell, a pan does not
 *   zoom       zoom 1 glides to a 1.6x close-up on the player, whose feet
 *              plat_room_project puts where the sprite stands; zoom 2 fits
 *              Talk's focus circle; back at zoom 0 the picture is exact again
 *   budget     the busiest room (8x8, 40 pieces, 12 residents) stays far
 *              inside the prim list and the vertex budget
 * DC8_PNG=DIR writes each parity frame as DIR/room_N_nat.png (320x240) and
 * DIR/room_N_dc.png (640x480).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "native.h"
#include "dc2d.h"
#include "swpvr.h"
#include "png.h"
#include "room_int.h"
#include "doomo/doomo.h"
#include "../../../core/src/home_int.h"
#include "../../../core/gen/furn_ids.h"
#include "room_shells.h"

extern int tee_ref_only;

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
                           printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint8_t cache[12 * 1024];
static uint8_t rgb[DC2D_W * DC2D_H * 3];
static uint8_t rgb_a[DC2D_W * DC2D_H * 3], rgb_b[DC2D_W * DC2D_H * 3];

static int x5to8(int v) { return (v << 3) | (v >> 2); }
static void nat565(uint8_t idx, int *r, int *g, int *b)
{
    uint16_t c = doomo_palette[idx];
    int g5 = (c >> 5) & 31, g6 = (g5 << 1) | (g5 >> 4);
    *r = x5to8(c & 31);
    *g = (g6 << 2) | (g6 >> 4);
    *b = x5to8((c >> 10) & 31);
}

/* draw v on both sides (room_ref natively, room.c on the Dreamcast) */
static void both(const DoomoRoomView *v)
{
    plat_begin_frame();
    plat_clear(PAL_BLACK);
    tee_ref_only = 1;
    room_ref_draw(v);
    tee_ref_only = 0;
    dc2d_room(v);
    plat_end_frame();
    swpvr_render(dc2d_last(), rgb);
}

/* the Dreamcast side alone into out */
static void dc_only(const DoomoRoomView *v, uint8_t *out)
{
    plat_begin_frame();
    plat_clear(PAL_BLACK);
    dc2d_room(v);
    plat_end_frame();
    swpvr_render(dc2d_last(), out);
}

/* logical pixels whose 2x2 block differs from the native pixel */
static int diffs(int *fx, int *fy)
{
    int x, y, n = 0;
    *fx = *fy = -1;
    for (y = 0; y < DC2D_LH; y++)
        for (x = 0; x < DC2D_LW; x++) {
            int r, g, b, k, bad = 0;
            nat565(nat_fb[y * nat_w + x], &r, &g, &b);
            for (k = 0; k < 4; k++) {
                const uint8_t *p = &rgb[((2 * y + (k >> 1)) * DC2D_W + 2 * x + (k & 1)) * 3];
                if (p[0] != r || p[1] != g || p[2] != b) bad = 1;
            }
            if (bad) { if (!n) { *fx = x; *fy = y; } n++; }
        }
    return n;
}

static void view_of(DoomoRoomView *v, DoomoRoomObj *objs, HomeRoom *r, int line)
{
    if (line < ROOM_SHELL_FIRST) rs_parity_room(r);
    else rs_shell_room(line - ROOM_SHELL_FIRST, r);
    rs_view(v, objs, r, line);
    v->vy = 12;
    v->vw = (int16_t)nat_w;
    v->vh = (int16_t)(nat_h - 24);
    v->plat_cache = cache;
    v->plat_cache_cap = sizeof(cache);
}

/* ---- parity: every golden frame ---------------------------------------------------- */
static void test_parity(void)
{
    static DoomoRoomObj objs[HOME_OBJ_MAX];
    const char *dir = getenv("DC8_PNG");
    int line, bad_lines = 0;
    for (line = 0; line < ROOM_SHELL_LINES; line++) {
        HomeRoom r;
        DoomoRoomView v;
        int n, x, y;
        view_of(&v, objs, &r, line);
        both(&v);
        n = diffs(&x, &y);
        CHECK(n == 0, "parity: golden line %d differs from room_ref on %d pixels (first %d,%d)", line, n, x, y);
        if (n) bad_lines++;
        if (dir && *dir) {
            char path[600];
            snprintf(path, sizeof(path), "%s/room_%d_nat.png", dir, line);
            nat_png_write(path, nat_fb, nat_w, nat_h, 1);
            snprintf(path, sizeof(path), "%s/room_%d_dc.png", dir, line);
            png_write_rgb(path, rgb, DC2D_W, DC2D_H);
        }
    }
    printf("parity: %d golden frames, %d differ\n", ROOM_SHELL_LINES, bad_lines);
}

/* ---- the whole catalog, each piece alone -------------------------------------------- */
static void test_catalog(void)
{
    int furn, k, frames = 0, bad = 0, sprites = 0;
    uint32_t boxes0 = dc_room_stats.boxes;
    for (furn = 0; furn < doomo_furn_count; furn++) {
        const DoomoFurn *f = home_furn(furn);
        HomeRoom r;
        DoomoRoomObj objs[2];
        DoomoRoomView v;
        if (!f) continue;
        memset(&r, 0, sizeof(r));
        r.wallpaper = r.floor = r.wall_paint = r.floor_paint = 0xFF;
        r.flags = HRF_OPEN | HRF_LIGHTS;
        r.size = (uint8_t)(5 | (5 << 4));                   /* 6 x 6 */
        r.nobj = 1;
        if (f->flags & FF_WALL) {
            r.obj[0].w0 = HO_W0(furn, 0, 1);
            r.obj[0].pos = HO_POS(2, 0);
            r.obj[0].z = 6;
        } else {
            r.obj[0].w0 = HO_W0(furn, 0, 0);
            r.obj[0].pos = HO_POS(2, 2);
        }
        r.obj[0].pa = r.obj[0].pb = FURN_PAINT_DEFAULT;
        for (k = 0; k < 5; k++) {
            int n, x, y;
            rs_view(&v, objs, &r, ROOM_SHELL_FIRST);
            v.yaw = (int16_t)((k & 3) * 1024);
            v.daylight = (uint8_t)(k < 4 ? 15 : 3);
            v.roof = ROOF_NONE;
            v.vy = 12; v.vw = (int16_t)nat_w; v.vh = (int16_t)(nat_h - 24);
            v.plat_cache = cache; v.plat_cache_cap = sizeof(cache);
            both(&v);
            n = diffs(&x, &y);
            frames++;
            if (n) {
                bad++;
                if (bad <= 8) CHECK(0, "catalog: piece %d view %d differs on %d pixels (first %d,%d)", furn, k, n, x, y);
            }
        }
        sprites++;
    }
    CHECK(bad == 0, "catalog: %d of %d frames differ", bad, frames);
    printf("catalog: %d pieces, %d frames, %d differ, %u drawn as boxes (no sprite)\n", sprites, frames, bad,
           dc_room_stats.boxes - boxes0);
}

/* ---- actors, cursor, band, half tiles, jitter, pans, tint ------------------------------ */
static uint8_t chibi_pix[14 * 22];
static DoomoImage chibi;

static void make_chibi(void)
{
    int x, y;
    for (y = 0; y < 22; y++)
        for (x = 0; x < 14; x++) {
            int edge = x == 0 || x == 13 || y == 0 || y == 21;
            chibi_pix[y * 14 + x] = (uint8_t)((x + y) % 5 == 0 ? 0 : edge ? PAL_BLACK :
                                              y < 8 ? PAL_RAMP(RAMP_PEACH, 5) : PAL_PAINT(PAINT_COBALT, x < 7 ? 1 : 2));
        }
    chibi.w = 14; chibi.h = 22; chibi.pix = chibi_pix; chibi.uid = doomo_image_new_uid();
}

static void dynamic_view(DoomoRoomView *v, DoomoRoomObj *objs, HomeRoom *r, DoomoRoomActor *acts, int k)
{
    view_of(v, objs, r, 0);
    v->yaw = (int16_t)(k * 1024);
    memset(acts, 0, 3 * sizeof(*acts));
    acts[0].img = &chibi; acts[0].x = INT2FIX(1) + FIX_ONE / 2; acts[0].y = INT2FIX(2) + FIX_ONE / 3;
    acts[0].sx = acts[0].sy = 256; acts[0].flags = RA_PLAYER;
    acts[1].img = &chibi; acts[1].x = INT2FIX(4) + FIX_ONE / 4; acts[1].y = INT2FIX(3);
    acts[1].sx = acts[1].sy = 256; acts[1].flags = BB_FLIPX; acts[1].z = 6;
    acts[2].img = &chibi; acts[2].x = INT2FIX(3); acts[2].y = INT2FIX(5) - FIX_ONE / 5;
    acts[2].sx = acts[2].sy = 256;
    v->actors = acts;
    v->nact = 3;
    v->cursor_x = 2; v->cursor_y = 3;
    v->sel_mask[(1 * v->w + 1) >> 3] |= (uint8_t)(1u << ((1 * v->w + 1) & 7));
    v->sel_mask[(1 * v->w + 2) >> 3] |= (uint8_t)(1u << ((1 * v->w + 2) & 7));
    v->time = 9;
    objs[9].flags |= RO_HALF_X | RO_HALF_Y;              /* the jellybean jar on the table */
    objs[10].jitter = 2;                                 /* the possessed plush */
}

static void test_dynamic(void)
{
    static DoomoRoomObj objs[HOME_OBJ_MAX];
    DoomoRoomActor acts[3];
    HomeRoom r;
    DoomoRoomView v;
    int k, n, x, y;
    make_chibi();
    for (k = 0; k < 4; k++) {
        dynamic_view(&v, objs, &r, acts, k);
        both(&v);
        n = diffs(&x, &y);
        CHECK(n == 0, "dynamic: view %d differs on %d pixels (first %d,%d)", k, n, x, y);
        v.pan_x = 3; v.pan_y = -2;                       /* the brake shake */
        both(&v);
        n = diffs(&x, &y);
        CHECK(n == 0, "dynamic: view %d with a pan differs on %d pixels (first %d,%d)", k, n, x, y);
        v.pan_x = 0; v.pan_y = 0;
        v.tint = TINT_GOLD; v.tint_level = 12;           /* tinted actors through a remap */
        both(&v);
        n = diffs(&x, &y);
        CHECK(n == 0, "dynamic: view %d tinted differs on %d pixels (first %d,%d)", k, n, x, y);
    }
    /* a squash is one quad on the Dreamcast (as plat_image_xform, DC3: the
     * native default rounds it to runs of rects): every difference must lie
     * inside the two squashed sprites' rects */
    dynamic_view(&v, objs, &r, acts, 0);
    acts[0].sx = 300; acts[0].sy = 220;
    objs[3].sq_x = 10; objs[3].sq_y = -10;
    both(&v);
    {
        const DcFrame *f = dc2d_last();
        int qx0[4], qy0[4], qx1[4], qy1[4], nq = 0, i, out = 0;
        for (i = 0; i < f->n && nq < 4; i++) {
            const DcPrim *p = &f->prims[i];
            if (p->tex == DC2D_NOTEX || p->mode != DC2D_IMAGE) continue;
            if ((int)(p->x1 - p->x0) == (int)(fabsf(p->u1 - p->u0) * dc2d_tex(p->tex)->tw * 2 + 0.5f) &&
                (int)(p->y1 - p->y0) == (int)((p->v1 - p->v0) * dc2d_tex(p->tex)->th * 2 + 0.5f)) continue;
            qx0[nq] = (int)p->x0 / 2 - 2; qy0[nq] = (int)p->y0 / 2 - 2;     /* a scaled prim, +- 2 */
            qx1[nq] = (int)p->x1 / 2 + 2; qy1[nq] = (int)p->y1 / 2 + 2;
            nq++;
        }
        n = diffs(&x, &y);
        for (y = 0; y < DC2D_LH; y++)
            for (x = 0; x < DC2D_LW; x++) {
                int rr, g, b, k, bad = 0, in = 0;
                nat565(nat_fb[y * nat_w + x], &rr, &g, &b);
                for (k = 0; k < 4; k++) {
                    const uint8_t *p = &rgb[((2 * y + (k >> 1)) * DC2D_W + 2 * x + (k & 1)) * 3];
                    if (p[0] != rr || p[1] != g || p[2] != b) bad = 1;
                }
                for (k = 0; k < nq; k++)
                    if (x >= qx0[k] && x < qx1[k] && y >= qy0[k] && y < qy1[k]) in = 1;
                if (bad && !in) out++;
            }
        CHECK(nq == 2, "dynamic: %d scaled quads, not the two squashed sprites", nq);
        CHECK(out == 0, "dynamic: %d of the %d differing pixels lie outside the squashed sprites", out, n);
    }
}

/* ---- no plat_cache: the shaded boxes -------------------------------------------------- */
static void test_boxes(void)
{
    static DoomoRoomObj objs[HOME_OBJ_MAX];
    HomeRoom r;
    DoomoRoomView v;
    uint32_t b0 = dc_room_stats.boxes;
    int n, x, y;
    view_of(&v, objs, &r, 1);
    v.plat_cache = NULL;
    v.plat_cache_cap = 0;
    both(&v);
    n = diffs(&x, &y);
    CHECK(n == 0, "boxes: the no-cache room differs on %d pixels (first %d,%d)", n, x, y);
    CHECK(dc_room_stats.boxes - b0 >= 12, "boxes: only %u pieces drawn as boxes", dc_room_stats.boxes - b0);
}

/* ---- the Dreamcast's effects ------------------------------------------------------------ */
static int count_mode(int mode, uint32_t argb, int any_argb)
{
    const DcFrame *f = dc2d_last();
    int i, n = 0;
    for (i = 0; i < f->n; i++)
        if (f->prims[i].mode == mode && (any_argb || f->prims[i].argb == argb)) n++;
    return n;
}

static int blended_share(void)
{
    /* rgb_a: without the piece, rgb_b: opaque, rgb: as a ghost. Of the
     * pixels the piece changes, the share the ghost shows as a mix */
    int i, changed = 0, mixed = 0;
    for (i = 0; i < DC2D_W * DC2D_H; i++) {
        const uint8_t *a = &rgb_a[i * 3], *b = &rgb_b[i * 3], *c = &rgb[i * 3];
        int k, inb = 1;
        if (!memcmp(a, b, 3)) continue;
        changed++;
        for (k = 0; k < 3; k++) {
            int lo = a[k] < b[k] ? a[k] : b[k], hi = a[k] < b[k] ? b[k] : a[k];
            if (c[k] + 8 < lo || c[k] > hi + 8) inb = 0;
        }
        if (inb && memcmp(c, b, 3) && memcmp(c, a, 3)) mixed++;
    }
    return changed ? mixed * 100 / changed : -1;
}

static void test_effects(void)
{
    static DoomoRoomObj objs[HOME_OBJ_MAX + 1];
    HomeRoom r;
    DoomoRoomView v;
    DoomoRoomActor act;
    const DcFrame *f;
    int i, share, sel_tex = -1, img_tex = -2;
    view_of(&v, objs, &r, 0);
    v.nobj = 0;                                          /* an empty room */
    dc_only(&v, rgb_a);
    memset(&objs[0], 0, sizeof(objs[0]));
    objs[0].furn = FURN_CLOUD_BED; objs[0].x = 2; objs[0].y = 2;
    objs[0].paint_a = objs[0].paint_b = 0xFF;
    v.nobj = 1;
    dc_only(&v, rgb_b);
    objs[0].flags = RO_GHOST;
    dc_only(&v, rgb);
    CHECK(count_mode(DC2D_GLASS, 0x80FFFFFFu, 0) == 1, "effects: a ghost is not one 50%% GLASS prim");
    share = blended_share();
    CHECK(share >= 90, "effects: only %d%% of the ghost's pixels are a blend", share);
    objs[0].flags = RO_GHOST | RO_INVALID;
    dc_only(&v, rgb);
    CHECK(count_mode(DC2D_GLASS, 0xB0FF6A6Au, 0) == 1, "effects: an invalid ghost is not the red GLASS prim");
    {
        long rs = 0, gs = 0;
        for (i = 0; i < DC2D_W * DC2D_H; i++)
            if (memcmp(&rgb_a[i * 3], &rgb_b[i * 3], 3)) { rs += rgb[i * 3]; gs += rgb[i * 3 + 1]; }
        CHECK(rs > gs + gs / 4, "effects: the invalid ghost is not red (r %ld, g %ld)", rs, gs);
    }
    objs[0].flags = RO_GHOST | RO_SEMI;
    dc_only(&v, rgb);
    f = dc2d_last();
    CHECK(count_mode(DC2D_GLASS, 0x80FFFFFFu, 0) == 1, "effects: the blueprint ghost is not a GLASS prim");
    {
        int grey = 0, tot = 0;
        for (i = 0; i < DC2D_W * DC2D_H; i++)
            if (memcmp(&rgb_a[i * 3], &rgb_b[i * 3], 3)) {
                const uint8_t *c = &rgb[i * 3], *a = &rgb_a[i * 3];
                /* (ghost - background) is a grey (equal channels within rounding) */
                int dr = 2 * c[0] - a[0], dg = 2 * c[1] - a[1], db = 2 * c[2] - a[2];
                tot++;
                if (abs(dr - dg) <= 24 && abs(dg - db) <= 24) grey++;
            }
        CHECK(tot && grey * 100 / tot >= 85, "effects: the blueprint ghost is not grey (%d of %d)", grey, tot);
    }
    objs[0].flags = RO_SELECTED;
    v.time = 8;
    dc_only(&v, rgb);
    f = dc2d_last();
    for (i = 1; i < f->n; i++)
        if (f->prims[i].mode == DC2D_ADD) {
            sel_tex = f->prims[i].tex;
            if (f->prims[i - 1].mode == DC2D_IMAGE) img_tex = f->prims[i - 1].tex;   /* the piece, just before */
        }
    CHECK(count_mode(DC2D_ADD, 0, 1) == 1, "effects: the selected piece has no ADD glow");
    CHECK(sel_tex >= 0 && sel_tex == img_tex, "effects: the glow is not over the piece's own texture (%d vs %d)", sel_tex, img_tex);
    {
        uint32_t a8 = 0, a0 = 0;
        for (i = 0; i < f->n; i++) if (f->prims[i].mode == DC2D_ADD) a8 = f->prims[i].argb >> 24;
        v.time = 0;
        dc_only(&v, rgb);
        f = dc2d_last();
        for (i = 0; i < f->n; i++) if (f->prims[i].mode == DC2D_ADD) a0 = f->prims[i].argb >> 24;
        CHECK(a8 > a0, "effects: the glow does not pulse (alpha %u at tick 8, %u at 0)", a8, a0);
        long lit_ = 0, base = 0;
        for (i = 0; i < DC2D_W * DC2D_H; i++)
            if (memcmp(&rgb_a[i * 3], &rgb_b[i * 3], 3)) { lit_ += rgb[i * 3] + rgb[i * 3 + 1]; base += rgb_b[i * 3] + rgb_b[i * 3 + 1]; }
        CHECK(lit_ > base, "effects: the glow does not brighten the piece");
    }
    /* a translucent resident */
    objs[0].flags = 0;
    make_chibi();
    memset(&act, 0, sizeof(act));
    act.img = &chibi; act.x = INT2FIX(4); act.y = INT2FIX(1); act.sx = act.sy = 256; act.flags = BB_TRANSLUCENT;
    v.actors = &act; v.nact = 1;
    dc_only(&v, rgb);
    CHECK(count_mode(DC2D_GLASS, 0x80FFFFFFu, 0) == 1, "effects: a translucent resident is not a 50%% GLASS prim");
}

/* ---- the order --------------------------------------------------------------------------- */
static int ref_behind(const DcRoomItem *a, const DcRoomItem *b)
{
    int ox = a->x0 < b->x1 && b->x0 < a->x1, oy = a->y0 < b->y1 && b->y0 < a->y1;
    if (ox && oy) return a->layer < b->layer || (a->layer == b->layer && a->z < b->z);
    if (a->x1 <= b->x0 && oy) return 1;
    if (a->y1 <= b->y0 && ox) return 1;
    if (a->x1 <= b->x0 && a->y1 <= b->y0) return 1;
    return 0;
}

/* room_ref_draw()'s loop, verbatim but for the drawing */
static int ref_cycles;                  /* picks made by the cycle fallback */
static void ref_sort(const DcRoomItem *items, int n, int16_t *order)
{
    uint8_t done[DC_ROOM_MAX_ITEMS];
    int total, i, j;
    memset(done, 0, sizeof(done));
    for (total = 0; total < n; total++) {
        int pick = -1, best = 0x7FFF;
        for (i = 0; i < n; i++) {
            int free_ = 1, key;
            if (done[i]) continue;
            for (j = 0; j < n && free_; j++)
                if (j != i && !done[j] && ref_behind(&items[j], &items[i]) && !ref_behind(&items[i], &items[j])) free_ = 0;
            key = (items[i].x0 + items[i].y0) * 8 + items[i].layer * 2 + (items[i].z ? 1 : 0);
            if (items[i].layer == 0) key -= 1000;
            if (free_ && key < best) { best = key; pick = i; }
        }
        if (pick < 0) ref_cycles++;
        if (pick < 0)
            for (i = 0; i < n; i++)
                if (!done[i]) {
                    int key = (items[i].x0 + items[i].y0) * 8 + items[i].layer * 2;
                    if (key < best) { best = key; pick = i; }
                }
        done[pick] = 1;
        order[total] = (int16_t)pick;
    }
}

static void test_sort(void)
{
    DcRoomItem it[DC_ROOM_MAX_ITEMS];
    int16_t a[DC_ROOM_MAX_ITEMS], b[DC_ROOM_MAX_ITEMS];
    DoomoRng rng;
    int t, i, bad = 0, cycles = 0;
    rng_seed(&rng, 0xD8C0DEu);
    for (t = 0; t < 3000; t++) {
        int n = 1 + (int)(rng_next(&rng) % (t < 200 ? 8u : (uint32_t)DC_ROOM_MAX_ITEMS));
        for (i = 0; i < n; i++) {
            int w = 1 + (int)(rng_next(&rng) % 3), d = 1 + (int)(rng_next(&rng) % 3);
            it[i].x0 = (int8_t)(rng_next(&rng) % 10); it[i].y0 = (int8_t)(rng_next(&rng) % 10);
            it[i].x1 = (int8_t)(it[i].x0 + w); it[i].y1 = (int8_t)(it[i].y0 + d);
            it[i].layer = (uint8_t)(rng_next(&rng) % 3);
            it[i].z = (uint8_t)((rng_next(&rng) & 3) ? 0 : rng_next(&rng) % 40);
            it[i].idx = (int16_t)i;
        }
        ref_sort(it, n, a);
        dc_room_sort(it, n, b);
        if (memcmp(a, b, (size_t)n * sizeof(a[0]))) bad++;
        if (ref_cycles) { cycles++; ref_cycles = 0; }
    }
    CHECK(bad == 0, "sort: %d of 3000 random layouts ordered differently from room_ref", bad);
    CHECK(cycles > 0, "sort: no random layout reached room_ref's cycle fallback");
    printf("sort: 3000 random layouts (%d of them through the cycle fallback), %d differ\n", cycles, bad);
}

/* ---- the caches -------------------------------------------------------------------------- */
static void test_cache(void)
{
    static DoomoRoomObj objs[HOME_OBJ_MAX];
    static DcPrim keep[DC2D_MAX_PRIMS];
    HomeRoom r;
    DoomoRoomView v;
    uint32_t s0, d0;
    int nkeep, n, x, y, i, lamp = -1;
    view_of(&v, objs, &r, ROOM_SHELL_FIRST + 48);       /* the lit room at night (H8) */
    dc_only(&v, rgb);
    s0 = dc_room_stats.shells; d0 = dc_room_stats.decodes;
    nkeep = dc2d_last()->n;
    memcpy(keep, dc2d_last()->prims, (size_t)nkeep * sizeof(DcPrim));
    dc_only(&v, rgb);
    CHECK(dc_room_stats.shells == s0, "cache: a view at rest rendered its shell again");
    CHECK(dc_room_stats.decodes == d0, "cache: a view at rest decoded %u sprites", dc_room_stats.decodes - d0);
    CHECK(dc2d_last()->n == nkeep && !memcmp(keep, dc2d_last()->prims, (size_t)nkeep * sizeof(DcPrim)),
          "cache: the same view twice gave different prims (%d vs %d)", dc2d_last()->n, nkeep);
    v.pan_x = -4;
    dc_only(&v, rgb);
    CHECK(dc_room_stats.shells == s0, "cache: a pan rendered the shell again");
    v.pan_x = 0;
    v.daylight = 9;
    dc_only(&v, rgb);
    CHECK(dc_room_stats.shells == s0 + 1, "cache: a new light did not render a new shell");
    for (i = 0; i < v.nobj; i++)
        if ((v.objs[i].flags & RO_FULLBRIGHT) && home_furn(v.objs[i].furn) && (home_furn(v.objs[i].furn)->flags & FF_LIGHT)) {
            lamp = i;
            break;
        }
    CHECK(lamp >= 0, "cache: the lit room has no lamp on");
    if (lamp >= 0) {
        objs[lamp].flags &= (uint8_t)~RO_FULLBRIGHT;     /* switched off */
        both(&v);
        CHECK(dc_room_stats.shells == s0 + 2, "cache: switching a lamp off did not render a new shell");
        n = diffs(&x, &y);
        CHECK(n == 0, "cache: the room with a lamp switched off differs on %d pixels (first %d,%d)", n, x, y);
    }
    v.yaw = 1024;
    dc_only(&v, rgb);
    CHECK(dc_room_stats.shells == s0 + 3, "cache: a turn did not render a new shell");
}

/* ---- the zoom ------------------------------------------------------------------------------- */
static void test_zoom(void)
{
    static DoomoRoomObj objs[HOME_OBJ_MAX];
    DoomoRoomActor acts[3];
    HomeRoom r;
    DoomoRoomView v;
    int16_t px, py, rx, ry;
    float s, fx, fy, cx, cy;
    int t, i, n, x, y, found = 0;
    make_chibi();
    dynamic_view(&v, objs, &r, acts, 0);
    v.cursor_x = v.cursor_y = -1;
    memset(v.sel_mask, 0, sizeof(v.sel_mask));
    dc2d_room_project(&v, acts[0].x, acts[0].y, acts[0].z, &px, &py);
    room_ref_project(&v, acts[0].x, acts[0].y, acts[0].z, &rx, &ry);
    CHECK(px == rx && py == ry, "zoom: at zoom 0 plat_room_project is not room_ref's (%d,%d vs %d,%d)", px, py, rx, ry);
    v.zoom = 1;
    for (t = 0; t < 30; t++) { v.time = (uint16_t)(100 + t); dc_only(&v, rgb); }
    CHECK(dc_room_xform(&s, &fx, &fy, &cx, &cy), "zoom: zoom 1 left the identity");
    CHECK(s > 1.59f && s < 1.61f, "zoom: zoom 1 settled at %.3f, not 1.6", (double)s);
    dc2d_room_project(&v, acts[0].x, acts[0].y, acts[0].z, &px, &py);
    CHECK(abs(px - (int)(cx + 0.5f)) <= 1 && abs(py - (int)(cy + 16.0f * s + 0.5f)) <= 1,
          "zoom: the player's feet are at %d,%d, not under the viewport's centre %.1f,%.1f", px, py, (double)cx,
          (double)(cy + 16.0f * s));
    /* the player's sprite (the last IMAGE prim of the chibi's size) stands on that point */
    {
        const DcFrame *f = dc2d_last();
        for (i = 0; i < f->n; i++) {
            const DcPrim *p = &f->prims[i];
            float w = p->x1 - p->x0, h = p->y1 - p->y0, mx = (p->x0 + p->x1) * 0.5f;
            if (p->mode != DC2D_IMAGE || w < 14 * 2 * s - 1 || w > 14 * 2 * s + 1 || h < 22 * 2 * s - 1 || h > 22 * 2 * s + 1)
                continue;
            if (mx > 2 * px - 4 && mx < 2 * px + 4 && p->y1 > 2 * py - 6 && p->y1 < 2 * py + 2 * 2 * s + 6) found = 1;
        }
        CHECK(found, "zoom: no 1.6x player sprite stands on the projected feet");
    }
    v.zoom = 2;
    v.focus_x = INT2FIX(4); v.focus_y = INT2FIX(3); v.focus_r = INT2FIX(2);
    for (t = 0; t < 30; t++) { v.time = (uint16_t)(130 + t); dc_only(&v, rgb); }
    dc_room_xform(&s, &fx, &fy, &cx, &cy);
    CHECK(s > 1.0f && s <= 3.0f, "zoom: the Talk framing's scale %.3f is outside 1..3", (double)s);
    dc2d_room_project(&v, v.focus_x, v.focus_y, 20, &px, &py);
    CHECK(abs(px - (int)(cx + 0.5f)) <= 1 && abs(py - (int)(cy + 0.5f)) <= 1,
          "zoom: Talk's focus is at %d,%d, not the viewport's centre", px, py);
    v.zoom = 0;
    for (t = 0; t < 40; t++) { v.time = (uint16_t)(160 + t); dc_only(&v, rgb); }
    CHECK(!dc_room_xform(&s, NULL, NULL, NULL, NULL), "zoom: back at zoom 0 the view is still scaled (%.4f)", (double)s);
    both(&v);
    n = diffs(&x, &y);
    CHECK(n == 0, "zoom: back at zoom 0 the picture differs on %d pixels (first %d,%d)", n, x, y);
}

/* ---- the busiest room -------------------------------------------------------------------- */
static void test_budget(void)
{
    static DoomoRoomObj objs[HOME_OBJ_MAX];
    DoomoRoomActor acts[12];
    DoomoRoomView v;
    static const uint16_t set[10] = { FURN_CUPCAKE_LAMP, FURN_POTTED_FERN, FURN_TEACUP_TABLE, FURN_BUNNY_BEANBAG,
                                      FURN_HORNED_POOP_PLUSH, FURN_JELLYBEAN_JAR, FURN_LAVA_LAMP, FURN_TUBE_TV,
                                      FURN_MARSHMALLOW_SOFA, FURN_DAISY_RUG };
    uint32_t drop0 = dc2d_stats.dropped;
    int i, n, x, y;
    make_chibi();
    memset(&v, 0, sizeof(v));
    memset(objs, 0, sizeof(objs));
    for (i = 0; i < HOME_OBJ_MAX; i++) {
        objs[i].furn = set[i % 10];
        objs[i].x = (uint8_t)(i % 8); objs[i].y = (uint8_t)((i / 8) * 2 % 8);
        objs[i].paint_a = objs[i].paint_b = 0xFF;
        objs[i].rot_anim = RO_ROT_ANIM(i & 3, i % 3);
        if (set[i % 10] == FURN_CUPCAKE_LAMP || set[i % 10] == FURN_LAVA_LAMP) objs[i].flags = RO_FULLBRIGHT;
    }
    memset(acts, 0, sizeof(acts));
    for (i = 0; i < 12; i++) {
        acts[i].img = &chibi; acts[i].x = INT2FIX(i % 8) + FIX_ONE / 2; acts[i].y = INT2FIX((i * 3) % 8) + FIX_ONE / 2;
        acts[i].sx = acts[i].sy = 256;
    }
    v.w = v.d = 8; v.objs = objs; v.nobj = HOME_OBJ_MAX; v.actors = acts; v.nact = 12;
    v.wallpaper = 6; v.floor = 5; v.wall_paint = PAINT_CHARCOAL; v.floor_paint = PAINT_RUST;
    v.room_kind = ROOMK_WALLS; v.daylight = 6; v.roof = ROOF_COTTAGE; v.punch_q8 = 256;
    v.cursor_x = 3; v.cursor_y = 3; v.vy = 12; v.vw = (int16_t)nat_w; v.vh = (int16_t)(nat_h - 24);
    v.plat_cache = cache; v.plat_cache_cap = sizeof(cache);
    for (i = 0; i < 4; i++) {
        v.yaw = (int16_t)(i * 1024);
        both(&v);
        n = diffs(&x, &y);
        CHECK(n == 0, "budget: the busy room in view %d differs on %d pixels (first %d,%d)", i, n, x, y);
        CHECK(dc_room_stats.prims < 300, "budget: the busy room took %u prims", dc_room_stats.prims);
    }
    CHECK(dc2d_stats.dropped == drop0, "budget: prims dropped");
    printf("budget: the busy room (40 pieces, 12 residents): %u items, %u prims, %u vertex bytes a frame\n",
           dc_room_stats.items, dc_room_stats.prims, dc2d_stats.vbytes);
}

/* ---- the canvas's assumption: no shell colour is index 0 (transparent in a texture) ---------- */
static void test_maps(void)
{
    int l, c, t, z = 0;
    for (l = 0; l < DOOMO_LIGHT_LEVELS; l++)
        for (c = 1; c < 256; c++) if (!doomo_colormap[l][c]) z++;
    for (t = 0; t < TINT_COUNT; t++)
        for (c = 1; c < 256; c++) if (!doomo_tintmap[t][c]) z++;
    for (c = 1; c < 256; c++) if (!palette_darker((uint8_t)c) || !palette_lighter((uint8_t)c)) z++;
    CHECK(z == 0, "maps: %d colours map to index 0", z);
    CHECK(doomo_palette[0] == doomo_palette[PAL_BLACK], "maps: index 0 is not black");
}

int main(void)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = 320; o.h = 240; o.save_dir = "."; o.seed = 1; o.quiet = 1;
    nat_init(&o);
    test_maps();
    test_parity();
    test_catalog();
    test_dynamic();
    test_boxes();
    test_effects();
    test_sort();
    test_cache();
    test_zoom();
    test_budget();
    printf("dcroom tests: %d checks, %d failed (shells %u, decodes %u, worst view %u prims)\n", checks, fails,
           dc_room_stats.shells, dc_room_stats.decodes, dc_room_stats.prims_max);
    return fails ? 1 : 0;
}
