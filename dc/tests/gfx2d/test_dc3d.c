/* test_dc3d.c - unit tests of the Dreamcast 3D batches and the overworld
 * on the host (DC7; owner: dreamcast). Built like test_dc2d.c (tee.c, the
 * software PVR swpvr.c), but the views are the Dreamcast's own
 * (dc/src/overworld.c through dc3d.c), rasterised by the software PVR.
 *
 *   camera     the focus is the viewport's centre; the camera looks north
 *              and down; a nearer point has a bigger z (1/w)
 *   clip       a polygon through the near plane is cut, every vertex in
 *              front with a finite z; one wholly behind adds nothing
 *   cover      every pixel of the viewport is opaque terrain (the depth
 *              buffer is written everywhere) on all four overworld maps
 *   order      a 2D rect drawn before the view is under it, one drawn
 *              after is on top (the marker keeps the painter's order)
 *   depth      a billboard behind a block is hidden at its feet, one in
 *              front of it shows: per pixel, through the depth buffer
 *   lift       a billboard lifted off the floor (a quest mark, a jump) at
 *              the screen's side stays over its feet: the same columns as
 *              on the ground, higher by the native runner's z / 4 pixels
 *   shade      a full-screen exact shade multiplies the view by the fitted
 *              factor; a partial one over it is the MUL fallback
 *   hold       plat_hold_frame keeps the view's polygons under new frames
 *   post       plat_post_offset moves the view with the other prims
 *   lerp       plat_pal_lerp switches the view to the lerp bank
 *   tint       the red tint pushes the picture toward red
 *   repeat     the same view twice gives the same polygons, byte for byte
 *   budget     the busiest views (every map, 400 billboards) stay inside
 *              the batch and the TA budget, nothing dropped
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "native.h"
#include "dc2d.h"
#include "dc3d.h"
#include "swpvr.h"
#include "png.h"
#include "doomo/doomo.h"
#include "doomo/map.h"
#include "doomo/textures.h"
#include "doomo/camfx.h"

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
                           printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint8_t rgb[DC2D_W * DC2D_H * 3], rgb2[DC2D_W * DC2D_H * 3];

static int x5to8(int v) { return (v << 3) | (v >> 2); }
static void pal565(uint8_t idx, int *r, int *g, int *b)
{
    uint16_t c = doomo_palette[idx];
    int g5 = (c >> 5) & 31, g6 = (g5 << 1) | (g5 >> 4);
    *r = x5to8(c & 31); *g = (g6 << 2) | (g6 >> 4); *b = x5to8((c >> 10) & 31);
}
static const uint8_t *px(const uint8_t *p, int x, int y) { return &p[(y * DC2D_W + x) * 3]; }
static int is_pal(const uint8_t *p, uint8_t idx)
{
    int r, g, b;
    pal565(idx, &r, &g, &b);
    return p[0] == r && p[1] == g && p[2] == b;
}

/* ---- a small synthetic map: grass, one tall block ---------------------------------- */
static DoomoMap tmap;
static void make_map(int w, int h)
{
    int i;
    memset(&tmap, 0, sizeof(tmap));
    tmap.w = (uint8_t)w; tmap.h = (uint8_t)h;
    tmap.ambient = 15;
    for (i = 0; i < w * h; i++) {
        DoomoCell *c = &tmap.cells[i];
        c->floor_tex = TEX_GRASS; c->ceil_tex = DOOMO_TEX_SKY; c->light = 15;
    }
}
static DoomoCell *tcell(int x, int y) { return &tmap.cells[y * tmap.w + x]; }

static void view_at(DoomoOverworldView *v, const DoomoMap *m, float cx_cells, float cy_cells)
{
    memset(v, 0, sizeof(*v));
    v->map = m;
    v->cx = (fix16)(cx_cells * 64.0f * 65536.0f);
    v->cy = (fix16)(cy_cells * 64.0f * 65536.0f);
    v->vw = DC2D_LW; v->vh = DC2D_LH;
    v->daylight = 15;
    v->time = 100;
}

static void frame(const DoomoOverworldView *v)
{
    plat_begin_frame();
    plat_clear(PAL_BLACK);
    if (v) dc2d_overworld(v);
    plat_end_frame();
}

/* a solid image for billboards */
static uint8_t solid_pix[16 * 32];
static DoomoImage solid_img;
static void make_solid(uint8_t idx)
{
    memset(solid_pix, idx, sizeof(solid_pix));
    solid_img.w = 16; solid_img.h = 32; solid_img.pix = solid_pix;
    solid_img.uid = 0;
}

/* ---- tests ---------------------------------------------------------------------------- */
static void test_camera(void)
{
    DoomoOverworldView v;
    const Dc3dCam *c;
    float sx, sy, z1, z2, sx2, sy2;
    make_map(32, 32);
    view_at(&v, &tmap, 16.5f, 16.5f);
    frame(&v);
    c = dc2d_overworld_cam();
    z1 = dc3d_project(c, 16.5f * 64, 16.5f * 64, 8.0f, &sx, &sy);
    CHECK(z1 > 0 && fabsf(sx - 320.0f) < 0.5f && fabsf(sy - 240.0f) < 0.5f,
          "camera: the focus projects to (%.1f, %.1f), want the centre (320, 240)", sx, sy);
    z2 = dc3d_project(c, 16.5f * 64, 13.5f * 64, 8.0f, &sx2, &sy2);
    CHECK(z2 > z1 && sy2 < sy, "camera: a point north is farther (%.0f > %.0f) and higher (%.1f < %.1f)",
          z2, z1, sy2, sy);
    z2 = dc3d_project(c, 18.5f * 64, 16.5f * 64, 8.0f, &sx2, &sy2);
    CHECK(sx2 > sx, "camera: east is to the right");
    z2 = dc3d_project(c, 16.5f * 64, 16.5f * 64, 64.0f, &sx2, &sy2);
    CHECK(z2 < z1 && sy2 < sy, "camera: a point above the focus is nearer and higher");
}

static void test_clip(void)
{
    Dc3dCam c;
    Dc3dIn q[4];
    const DcBatch *b;
    int i, n0, ok = 1;
    uint32_t cl0 = dc3d_stats.clipped;
    memset(&c, 0, sizeof(c));
    c.rx = 1.0f; c.dz = -1.0f; c.fy = -1.0f;            /* looking north, level */
    c.cx = 320; c.cy = 240; c.focal = 400;
    c.vx0 = 0; c.vy0 = 0; c.vx1 = 640; c.vy1 = 480;
    memset(q, 0, sizeof(q));
    /* a floor quad from behind the eye to far in front of it */
    q[0].x = -50; q[0].y = -500; q[0].z = -20;
    q[1].x = 50;  q[1].y = -500; q[1].z = -20;
    q[2].x = 50;  q[2].y = 100;  q[2].z = -20;
    q[3].x = -50; q[3].y = 100;  q[3].z = -20;
    for (i = 0; i < 4; i++) { q[i].argb = 0xFF808080u; q[i].u = (float)(i & 1); q[i].v = (float)(i >> 1); }
    plat_begin_frame();
    dc3d_seg_begin(0, 0, DC2D_LW, DC2D_LH);
    b = dc3d_batch(dc3d_cur_slot());
    n0 = b->np;
    CHECK(dc3d_add(&c, DC3D_OPAQUE, -1, q, 4) == 1, "clip: the quad was not added");
    CHECK(dc3d_stats.clipped == cl0 + 1, "clip: not counted as clipped");
    if (b->np == n0 + 1) {
        const DcPoly *p = &b->p[n0];
        for (i = 0; i < p->nv; i++) {
            const DcVert *v = &b->v[p->first + (uint32_t)i];
            if (!(v->z > 0.0f && v->z <= DC3D_ZK / DC3D_ZNEAR + 1e-3f) || !isfinite(v->x) || !isfinite(v->y)) ok = 0;
        }
        CHECK(ok && p->nv == 4, "clip: %d vertices, all in front with a finite z: %d", p->nv, ok);
    }
    for (i = 0; i < 4; i++) q[i].y = 200;               /* wholly behind */
    q[0].y = q[1].y = 300;
    CHECK(dc3d_add(&c, DC3D_OPAQUE, -1, q, 4) == 0, "clip: a quad behind the eye was added");
    dc3d_seg_end();
    plat_end_frame();
}

static int covered_pixels(void)
{
    const float *z = swpvr_zbuf();
    int i, n = 0;
    for (i = 0; i < DC2D_W * DC2D_H; i++) n += z[i] > 0.001f;
    return n;
}

static void test_cover(void)
{
    static const int maps[4] = { MAPID_HAHALAND, MAPID_HAHALAND_INVADED, MAPID_HELLVILE, MAPID_PARK_UNITED };
    static const float spots[4][2] = { {0.3f, 0.3f}, {0.5f, 0.5f}, {0.8f, 0.7f}, {0.05f, 0.95f} };
    int m, k;
    for (m = 0; m < 4; m++) {
        map_load(maps[m]);
        for (k = 0; k < 4; k++) {
            DoomoOverworldView v;
            int n;
            view_at(&v, &doomo_map, doomo_map.w * spots[k][0], doomo_map.h * spots[k][1]);
            v.daylight = (uint8_t)(k * 5);
            frame(&v);
            swpvr_render(dc2d_last(), rgb);
            n = covered_pixels();
            CHECK(n == DC2D_W * DC2D_H, "cover: map %d spot %d: %d of %d pixels have terrain",
                  maps[m], k, n, DC2D_W * DC2D_H);
            if (n != DC2D_W * DC2D_H && getenv("DC7_DUMP")) {
                const float *z = swpvr_zbuf();
                int i, xmin = 9999, xmax = -1, ymin = 9999, ymax = -1;
                char path[64];
                for (i = 0; i < DC2D_W * DC2D_H; i++)
                    if (z[i] <= 0.001f) {
                        int x = i % DC2D_W, y = i / DC2D_W;
                        rgb[i * 3] = 255; rgb[i * 3 + 1] = 0; rgb[i * 3 + 2] = 255;
                        if (x < xmin) xmin = x;
                        if (x > xmax) xmax = x;
                        if (y < ymin) ymin = y;
                        if (y > ymax) ymax = y;
                    }
                printf("  uncovered box (%d,%d)-(%d,%d)\n", xmin, ymin, xmax, ymax);
                snprintf(path, sizeof(path), "cover_%d_%d.png", maps[m], k);
                png_write_rgb(path, rgb, DC2D_W, DC2D_H);
            }
            CHECK(dc3d_stats.polys > 100 && dc3d_stats.bytes < DC3D_SEG_BUDGET,
                  "cover: map %d spot %d: %u polygons, %u bytes", maps[m], k, dc3d_stats.polys, dc3d_stats.bytes);
        }
    }
}

static void test_order(void)
{
    DoomoOverworldView v;
    make_map(32, 32);
    view_at(&v, &tmap, 16.5f, 16.5f);
    plat_begin_frame();
    plat_clear(PAL_BLACK);
    dc2d_rect(10, 10, 20, 20, PAL_WHITE);       /* under the view */
    dc2d_overworld(&v);
    dc2d_rect(100, 100, 20, 20, PAL_WHITE);     /* over it */
    plat_end_frame();
    swpvr_render(dc2d_last(), rgb);
    CHECK(!is_pal(px(rgb, 40, 40), PAL_WHITE), "order: a rect drawn before the view shows through");
    CHECK(is_pal(px(rgb, 220, 220), PAL_WHITE), "order: a rect drawn after the view is not on top");
}

static void test_depth(void)
{
    DoomoOverworldView v;
    DoomoBillboard bb[2];
    const Dc3dCam *c;
    float sx, sy;
    int x, y;
    uint8_t col = PAL_RAMP(RAMP_VAPE, 5);
    make_map(32, 32);
    tcell(16, 14)->wall = TEX_PASTEL_BRICK;
    tcell(16, 14)->ceil_h = 12;                 /* a 96-unit block over cell (16, 14) */
    make_solid(col);
    memset(bb, 0, sizeof(bb));
    bb[0].x = (fix16)(16.5f * 64 * 65536); bb[0].y = (fix16)(13.6f * 64 * 65536);   /* behind it */
    bb[1].x = (fix16)(16.5f * 64 * 65536); bb[1].y = (fix16)(15.5f * 64 * 65536);   /* in front */
    bb[0].img = bb[1].img = &solid_img;
    bb[0].scale = bb[1].scale = 256;
    bb[0].flags = bb[1].flags = BB_FULLBRIGHT;
    view_at(&v, &tmap, 16.5f, 16.5f);
    v.sprites = bb; v.sprite_count = 2;
    frame(&v);
    swpvr_render(dc2d_last(), rgb);
    c = dc2d_overworld_cam();
    dc3d_project(c, 16.5f * 64, 13.6f * 64, 8.0f, &sx, &sy);     /* the hidden one's legs */
    x = (int)sx; y = (int)sy;
    CHECK(!is_pal(px(rgb, x, y), col), "depth: the billboard behind the block shows at its feet (%d, %d)", x, y);
    dc3d_project(c, 16.5f * 64, 15.5f * 64, 30.0f, &sx, &sy);    /* the front one's body */
    x = (int)sx; y = (int)sy;
    CHECK(is_pal(px(rgb, x, y), col), "depth: the billboard in front of the block is hidden at (%d, %d)", x, y);
}

/* the columns and the lowest row of colour idx in rgb[] (-1: none) */
static void extent(uint8_t idx, int *x0, int *x1, int *y1)
{
    int x, y;
    *x0 = DC2D_W; *x1 = -1; *y1 = -1;
    for (y = 0; y < DC2D_H; y++)
        for (x = 0; x < DC2D_W; x++)
            if (is_pal(px(rgb, x, y), idx)) {
                if (x < *x0) *x0 = x;
                if (x > *x1) *x1 = x;
                if (y > *y1) *y1 = y;
            }
}

static void test_lift(void)
{
    DoomoOverworldView v;
    DoomoBillboard bb;
    uint8_t col = PAL_RAMP(RAMP_VAPE, 5);
    int side, ax0, ax1, ay1, bx0, bx1, by1;
    make_map(32, 32);
    make_solid(col);
    for (side = -1; side <= 1; side += 2) {
        memset(&bb, 0, sizeof(bb));
        bb.x = (fix16)((16.5f + 6.0f * (float)side) * 64 * 65536);   /* near the screen's side */
        bb.y = (fix16)(16.5f * 64 * 65536);
        bb.img = &solid_img;
        bb.scale = 256;
        bb.flags = BB_FULLBRIGHT;
        view_at(&v, &tmap, 16.5f, 16.5f);
        v.sprites = &bb; v.sprite_count = 1;
        frame(&v);
        swpvr_render(dc2d_last(), rgb);
        extent(col, &ax0, &ax1, &ay1);
        bb.z = (fix16)(108 * 65536);                                 /* the quest mark's height */
        frame(&v);
        swpvr_render(dc2d_last(), rgb);
        extent(col, &bx0, &bx1, &by1);
        CHECK(ax1 >= 0 && bx1 >= 0 && abs(bx0 - ax0) <= 1 && abs(bx1 - ax1) <= 1,
              "lift: side %d: the lifted billboard spans x %d..%d, on the ground %d..%d",
              side, bx0, bx1, ax0, ax1);
        /* 108 / 4 = 27 logical pixels at the focus: 54 screen pixels, a
         * little less at this depth */
        CHECK(ay1 - by1 >= 44 && ay1 - by1 <= 60, "lift: side %d: lifted by %d pixels, want about 54",
              side, ay1 - by1);
    }
}

static void test_shade(void)
{
    DoomoOverworldView v;
    int i, bad = 0, n = 0;
    uint32_t f = dc2d_shade_argb(8), fb0;
    make_map(32, 32);
    view_at(&v, &tmap, 16.5f, 16.5f);
    frame(&v);
    swpvr_render(dc2d_last(), rgb);
    plat_begin_frame();
    plat_clear(PAL_BLACK);
    dc2d_overworld(&v);
    dc2d_shade_rect(0, 0, DC2D_LW, DC2D_LH, 8);
    plat_end_frame();
    swpvr_render(dc2d_last(), rgb2);
    for (i = 0; i < DC2D_W * DC2D_H; i += 97) {
        int k;
        for (k = 0; k < 3; k++) {
            int want = rgb[i * 3 + k] * (int)((f >> (16 - 8 * k)) & 255u) / 255;
            if (abs(rgb2[i * 3 + k] - want) > 12) bad++;
        }
        n++;
    }
    CHECK(bad == 0, "shade: %d of %d samples off the fitted factor", bad, n * 3);
    fb0 = dc2d_stats.shade_fallbacks;
    plat_begin_frame();
    plat_clear(PAL_BLACK);
    dc2d_overworld(&v);
    dc2d_shade_rect(40, 40, 100, 50, 8);
    plat_end_frame();
    CHECK(dc2d_stats.shade_fallbacks == fb0 + 1, "shade: a partial shade over the view is not the multiply");
}

static void test_hold(void)
{
    DoomoOverworldView v;
    make_map(32, 32);
    view_at(&v, &tmap, 12.5f, 20.5f);
    frame(&v);
    swpvr_render(dc2d_last(), rgb);
    plat_begin_frame();
    CHECK(dc2d_hold_frame(1) == 1, "hold: not held");
    plat_end_frame();
    swpvr_render(dc2d_last(), rgb2);
    CHECK(memcmp(rgb, rgb2, sizeof(rgb)) == 0, "hold: the held view differs");
    plat_begin_frame();                         /* two more frames: the batches turn over */
    plat_end_frame();
    plat_begin_frame();
    plat_end_frame();
    swpvr_render(dc2d_last(), rgb2);
    CHECK(memcmp(rgb, rgb2, sizeof(rgb)) == 0, "hold: the held view changed after two frames");
    plat_begin_frame();
    dc2d_shade_rect(0, 0, DC2D_LW, DC2D_LH, 10);  /* a full shade copies the held view */
    plat_end_frame();
    swpvr_render(dc2d_last(), rgb2);
    CHECK(memcmp(rgb, rgb2, sizeof(rgb)) != 0 && covered_pixels() == DC2D_W * DC2D_H,
          "hold: the shaded copy of the held view is missing");
    dc2d_hold_frame(0);
}

static void test_post(void)
{
    DoomoOverworldView v;
    int x, y, bad = 0;
    make_map(32, 32);
    view_at(&v, &tmap, 16.5f, 16.5f);
    frame(&v);
    swpvr_render(dc2d_last(), rgb);
    plat_begin_frame();
    plat_clear(PAL_BLACK);
    dc2d_overworld(&v);
    dc2d_post_offset(3, 2);
    plat_end_frame();
    swpvr_render(dc2d_last(), rgb2);
    for (y = 100; y < 400; y += 7)
        for (x = 100; x < 500; x += 11)
            if (memcmp(px(rgb, x, y), px(rgb2, x + 6, y + 4), 3) != 0) bad++;
    CHECK(bad == 0, "post: %d samples not moved by (6, 4) pixels", bad);
}

static void test_lerp(void)
{
    DoomoOverworldView v;
    const DcFrame *f;
    int i, flagged = 0;
    make_map(32, 32);
    view_at(&v, &tmap, 16.5f, 16.5f);
    plat_begin_frame();
    plat_clear(PAL_BLACK);
    dc2d_overworld(&v);
    dc2d_pal_lerp(PAL_LERP_NONE + 1, 8);
    plat_end_frame();
    f = dc2d_last();
    for (i = 0; i < f->n; i++)
        if (f->prims[i].mode == DC2D_VIEW3D && (f->prims[i].flags & DC2D_PF_LERP)) flagged++;
    CHECK(flagged == 1 && f->lerp, "lerp: the view's marker is not on the lerp bank");
}

static void mean_rgb(const uint8_t *p, double *m)
{
    int i;
    m[0] = m[1] = m[2] = 0;
    for (i = 0; i < DC2D_W * DC2D_H; i++) { m[0] += p[i * 3]; m[1] += p[i * 3 + 1]; m[2] += p[i * 3 + 2]; }
    for (i = 0; i < 3; i++) m[i] /= DC2D_W * DC2D_H;
}

static void test_tint(void)
{
    DoomoOverworldView v;
    double a[3], b[3];
    make_map(32, 32);
    view_at(&v, &tmap, 16.5f, 16.5f);
    frame(&v);
    swpvr_render(dc2d_last(), rgb);
    v.tint = TINT_RED; v.tint_level = 8;
    frame(&v);
    swpvr_render(dc2d_last(), rgb2);
    mean_rgb(rgb, a);
    mean_rgb(rgb2, b);
    CHECK(b[0] - b[1] > a[0] - a[1] + 20.0, "tint: red minus green %.1f -> %.1f", a[0] - a[1], b[0] - b[1]);
}

static void test_repeat(void)
{
    static DcVert v1[DC3D_MAX_VERTS];
    DoomoOverworldView v;
    const DcBatch *b;
    int n1;
    map_load(MAPID_HELLVILE);
    view_at(&v, &doomo_map, 20.5f, 20.5f);
    frame(&v);
    b = dc3d_batch(dc3d_cur_slot());           /* the frame just shown */
    n1 = b->nv;
    memcpy(v1, b->v, (size_t)n1 * sizeof(DcVert));
    frame(&v);
    frame(&v);
    b = dc3d_batch(dc3d_cur_slot());
    CHECK(b->nv == n1 && memcmp(v1, b->v, (size_t)n1 * sizeof(DcVert)) == 0,
          "repeat: the same view gave different polygons (%d vs %d vertices)", b->nv, n1);
}

static void test_budget(void)
{
    static DoomoBillboard bb[400];
    static const int maps[4] = { MAPID_HAHALAND, MAPID_HAHALAND_INVADED, MAPID_HELLVILE, MAPID_PARK_UNITED };
    int m, i;
    uint32_t drop0 = dc3d_stats.dropped, d2 = dc2d_stats.dropped, worst = 0, worstp = 0;
    make_solid(PAL_WHITE);
    for (m = 0; m < 4; m++) {
        DoomoOverworldView v;
        map_load(maps[m]);
        view_at(&v, &doomo_map, doomo_map.w * 0.5f, doomo_map.h * 0.5f);
        for (i = 0; i < 400; i++) {
            memset(&bb[i], 0, sizeof(bb[i]));
            bb[i].x = v.cx + (fix16)(((i % 20) - 10) * 40) * 65536;
            bb[i].y = v.cy + (fix16)(((i / 20) - 12) * 30) * 65536;
            bb[i].img = &solid_img;
            bb[i].scale = 256;
            bb[i].flags = BB_SHADOW;
        }
        v.sprites = bb; v.sprite_count = 400;
        frame(&v);
        if (dc3d_stats.bytes > worst) worst = dc3d_stats.bytes;
        if (dc3d_stats.polys > worstp) worstp = dc3d_stats.polys;
    }
    printf("budget: worst view %u polygons, %u TA bytes (budget %u); total built %u, culled %u, clipped %u\n",
           worstp, worst, DC3D_SEG_BUDGET, dc3d_stats.segs, dc3d_stats.culled, dc3d_stats.clipped);
    CHECK(dc3d_stats.dropped == drop0, "budget: %u polygons dropped", dc3d_stats.dropped - drop0);
    CHECK(dc2d_stats.dropped == d2, "budget: %u 2D prims dropped", dc2d_stats.dropped - d2);
}

int main(void)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = 320; o.h = 240; o.save_dir = "."; o.seed = 1; o.quiet = 1;
    nat_init(&o);
    test_camera();
    test_clip();
    test_cover();
    test_order();
    test_depth();
    test_lift();
    test_shade();
    test_hold();
    test_post();
    test_lerp();
    test_tint();
    test_repeat();
    test_budget();
    printf("dc3d tests: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
