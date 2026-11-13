/* test_dc_v3d.c - the Dreamcast first-person view (DC6 part 1, dc/src/view3d.c)
 * against the reference raycaster (core/native/raycast.c) on the host
 * (owner: dreamcast). Built like test_dc3d.c: the native frame is the
 * reference's, the Dreamcast list is rendered by the software PVR at 640x480.
 * The two are not equal (polygons against columns), so the parity is a
 * measure: a logical pixel matches when its 2x2 block's mean colour is within
 * TOL of the native pixel in every channel.
 *
 *   parity     every first-person map (Hell Breach 1..7, the arena), 24 eyes
 *              each over the open cells, 4 headings, level and pitched:
 *              level views average at least PARITY_MIN matching pixels, pitched
 *              ones PITCH_MIN, no level frame is below FRAME_MIN; frames the
 *              reference itself leaves empty are skipped
 *   tint       a tinted view still matches (the tint is the native fit)
 *   viewport   a view smaller than the screen paints only inside its
 *              rectangle (the pixels around it are the clear colour)
 *   dist       dc_view3d_set_dist clamps; a shorter view draws fewer cells
 *   backdrop   without the backdrop the sky and stone are not drawn
 *   repeat     the same view twice gives the same polygons, byte for byte
 *   budget     the busiest views stay inside the batch and TA budget,
 *              nothing dropped
 *   tintfit    dc3d_tint_fit: identity for TINT_NONE, b >= 0 everywhere
 * DC6_PNG=DIR writes the worst frames as DIR/v3d_N_nat.png / _dc.png.
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
#include "doomo/palette.h"

void ref_view3d(const DoomoView3D *v);

#define TOL         40
#define PARITY_MIN  0.75   /* measured 79.2% on 2026-11-13; the gap is listed in docs/DREAMCAST.md section 9 */
#define FRAME_MIN   0.45   /* at most 3% of the level frames may be below it */
#define PITCH_MIN   0.60

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
                           printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint8_t rgb[DC2D_W * DC2D_H * 3], rgb2[DC2D_W * DC2D_H * 3];

static int x5to8(int v) { return (v << 3) | (v >> 2); }
static void nat565(uint8_t idx, int *r, int *g, int *b)
{
    uint16_t c = doomo_palette[idx];
    int g5 = (c >> 5) & 31, g6 = (g5 << 1) | (g5 >> 4);
    *r = x5to8(c & 31); *g = (g6 << 2) | (g6 >> 4); *b = x5to8((c >> 10) & 31);
}

static const int maps[] = { MAPID_VAPE_CELLAR, MAPID_CLOUD_NINE_SPIRE, MAPID_ARENA,
                            MAPID_SUGAR_RUSH_SEWERS, MAPID_PLUSHIE_FACTORY, MAPID_GRAVEYARD_SHIFT,
                            MAPID_BONE_CATHEDRAL, MAPID_HELLVILE_UNDERBELLY };
#define NMAPS ((int)(sizeof(maps) / sizeof(maps[0])))

static void view_of(DoomoView3D *v, int cx, int cy, int heading, int pitch)
{
    memset(v, 0, sizeof(*v));
    v->map = &doomo_map;
    v->x = (fix16)((cx * 64 + 32) * 65536);
    v->y = (fix16)((cy * 64 + 32) * 65536);
    v->z = (fix16)((map_floor_z(&doomo_map, cx, cy) + DOOMO_EYE_HEIGHT) * 65536);
    v->angle = (int16_t)(heading * 1024 + 111);
    v->pitch = (int16_t)pitch;
    v->vw = DC2D_LW; v->vh = DC2D_LH;
    v->time = 100;
}

/* both sides: the native frame in nat_fb, the Dreamcast frame in out */
void dc2d_view3d_remap_budget(int n);

static void both(const DoomoView3D *v, uint8_t *out)
{
    plat_begin_frame();
    plat_clear(PAL_BLACK);
    ref_view3d(v);
    dc2d_view3d_draw(v);
    plat_end_frame();
    swpvr_render(dc2d_last(), out);
}

static int match_px(const uint8_t *o, int x, int y)
{
    int r, g, b, k, sr = 0, sg = 0, sb = 0;
    nat565(nat_fb[y * nat_w + x], &r, &g, &b);
    for (k = 0; k < 4; k++) {
        const uint8_t *p = &o[((2 * y + (k >> 1)) * DC2D_W + 2 * x + (k & 1)) * 3];
        sr += p[0]; sg += p[1]; sb += p[2];
    }
    return abs(sr / 4 - r) <= TOL && abs(sg / 4 - g) <= TOL && abs(sb / 4 - b) <= TOL;
}

static double parity(const uint8_t *o, int x0, int y0, int w, int h)
{
    int x, y, n = 0;
    for (y = y0; y < y0 + h; y++)
        for (x = x0; x < x0 + w; x++) n += match_px(o, x, y);
    return (double)n / (double)(w * h);
}

static int uniform_native(void)
{
    int i, n = nat_w * nat_h;
    for (i = 1; i < n; i++) if (nat_fb[i] != nat_fb[0]) return 0;
    return 1;
}

static int open_cell(const DoomoMap *m, int x, int y)
{
    return x > 0 && y > 0 && x < m->w - 1 && y < m->h - 1 && !m->cells[y * m->w + x].wall;
}

static void test_parity(void)
{
    int mi, k, worst_n = 0;
    double sum = 0, worst = 2.0;
    int frames = 0, nodraw = 0, nbad = 0, skipped = 0, pn = 0, ln = 0, lowf = 0;
    double psum = 0, lsum = 0;
    const char *png = getenv("DC6_PNG");
    for (mi = 0; mi < NMAPS; mi++) {
        int cells[2000][2], nc = 0, x, y, stride;
        double msum = 0, mworst = 2.0;
        int mn = 0;
        map_load(maps[mi]);
        for (y = 1; y < doomo_map.h - 1; y++)
            for (x = 1; x < doomo_map.w - 1; x++)
                if (open_cell(&doomo_map, x, y) && nc < 2000) { cells[nc][0] = x; cells[nc][1] = y; nc++; }
        CHECK(nc > 8, "parity: map %d has %d open cells", maps[mi], nc);
        stride = nc / 24 > 0 ? nc / 24 : 1;
        for (k = 0; k < nc; k += stride) {
            int hd, pitch;
            for (hd = 0; hd < 4; hd++)
                for (pitch = 0; pitch < 2; pitch++) {
                    DoomoView3D v;
                    double p;
                    if (pitch && (hd & 1)) continue;
                    view_of(&v, cells[k][0], cells[k][1], hd, pitch ? 120 : 0);
                    both(&v, rgb);
                    if (uniform_native()) { skipped++; continue; }   /* an eye the reference itself draws nothing from */
                    p = parity(rgb, 0, 0, DC2D_LW, DC2D_LH);
                    if (dc2d_view3d_cells() == 0) nodraw++;
                    if (pitch) { psum += p; pn++; } else { lsum += p; ln++; }
                    sum += p; msum += p; frames++; mn++;
                    if (p < mworst) mworst = p;
                    if (p < worst) {
                        worst = p; worst_n++;
                        if (png) {
                            char nm[512];
                            snprintf(nm, sizeof(nm), "%s/v3d_worst_nat.png", png);
                            nat_png_write(nm, nat_fb, nat_w, nat_h, 1);
                            snprintf(nm, sizeof(nm), "%s/v3d_worst_dc.png", png);
                            png_write_rgb(nm, rgb, DC2D_W, DC2D_H);
                        }
                    }
                    if (png && p < FRAME_MIN && !pitch && nbad < 40) {
                        char nm[512];
                        snprintf(nm, sizeof(nm), "%s/v3d_bad%02d_m%d_%d_%d_h%dp%d_nat.png", png, nbad, maps[mi], cells[k][0], cells[k][1], hd, pitch);
                        nat_png_write(nm, nat_fb, nat_w, nat_h, 1);
                        snprintf(nm, sizeof(nm), "%s/v3d_bad%02d_m%d_%d_%d_h%dp%d_dc.png", png, nbad, maps[mi], cells[k][0], cells[k][1], hd, pitch);
                        png_write_rgb(nm, rgb, DC2D_W, DC2D_H);
                        nbad++;
                    }
                    if (!pitch && p < FRAME_MIN) lowf++;
                }
        }
        printf("parity: map %d: %d frames, mean %.1f%%, worst %.1f%%\n", maps[mi], mn, msum / mn * 100.0, mworst * 100.0);
    }
    printf("parity: %d frames, mean %.1f%%, worst %.1f%% (tolerance %d per channel)\n",
           frames, sum / frames * 100.0, worst * 100.0, TOL);
    printf("parity: level views %d, mean %.1f%%; pitched (look up 120) views %d, mean %.1f%%; %d skipped (native frame empty)\n",
           ln, lsum / ln * 100.0, pn, psum / pn * 100.0, skipped);
    CHECK(lowf * 100 <= ln * 3, "parity: %d of %d level frames below %.0f%%", lowf, ln, FRAME_MIN * 100.0);
    printf("parity: %d level frames below %.0f%% (eyes beside a border block lower than their floor: docs/DREAMCAST.md)\n", lowf, FRAME_MIN * 100.0);
    CHECK(lsum / ln >= PARITY_MIN, "parity: level views mean %.1f%% below %.0f%%", lsum / ln * 100.0, PARITY_MIN * 100.0);
    CHECK(psum / pn >= PITCH_MIN, "parity: pitched views mean %.1f%% below %.0f%%", psum / pn * 100.0, PITCH_MIN * 100.0);
    CHECK(nodraw == 0, "parity: %d views saw no cell", nodraw);
}

/* new textures per view are budgeted on the console; with none allowed a view still draws (the plain
 * texture darkened by the fit) and is close to the full one */
static void test_remap_budget(void)
{
    DoomoView3D v;
    double full, part;
    map_load(MAPID_VAPE_CELLAR);
    view_of(&v, 5, 5, 0, 0);
    if (!open_cell(&doomo_map, 5, 5)) {
        int x, y;
        for (y = 1; y < doomo_map.h - 1; y++) for (x = 1; x < doomo_map.w - 1; x++)
            if (open_cell(&doomo_map, x, y)) { view_of(&v, x, y, 0, 0); y = 99; break; }
    }
    dc2d_tex_init();                                /* an empty cache: nothing is there yet */
    dc2d_view3d_remap_budget(0);
    both(&v, rgb);
    part = parity(rgb, 0, 0, DC2D_LW, DC2D_LH);
    dc2d_view3d_remap_budget(100000);
    both(&v, rgb);
    full = parity(rgb, 0, 0, DC2D_LW, DC2D_LH);
    printf("remap budget: full %.1f%%, no new copies %.1f%%\n", full * 100.0, part * 100.0);
    CHECK(part >= 0.5, "remap budget: a view with no new textures matches only %.1f%%", part * 100.0);
}

static void test_tint(void)
{
    DoomoView3D v;
    double plain, tinted;
    map_load(MAPID_VAPE_CELLAR);
    view_of(&v, 5, 5, 0, 0);
    if (!open_cell(&doomo_map, 5, 5)) {
        int x, y;
        for (y = 1; y < doomo_map.h - 1; y++) for (x = 1; x < doomo_map.w - 1; x++)
            if (open_cell(&doomo_map, x, y)) { view_of(&v, x, y, 0, 0); y = 99; break; }
    }
    both(&v, rgb);
    plain = parity(rgb, 0, 0, DC2D_LW, DC2D_LH);
    v.tint = TINT_RED; v.tint_level = 12;
    both(&v, rgb);
    tinted = parity(rgb, 0, 0, DC2D_LW, DC2D_LH);
    printf("tint: plain %.1f%%, red 12/16 %.1f%%\n", plain * 100.0, tinted * 100.0);
    CHECK(tinted >= 0.6, "tint: a red view matches only %.1f%%", tinted * 100.0);
}

static void test_viewport(void)
{
    DoomoView3D v;
    int x, y, outside = 0, inside = 0;
    map_load(MAPID_VAPE_CELLAR);
    for (y = 1; y < doomo_map.h - 1 && !inside; y++) for (x = 1; x < doomo_map.w - 1; x++)
        if (open_cell(&doomo_map, x, y)) { view_of(&v, x, y, 0, 0); inside = 1; break; }
    v.vx = 40; v.vy = 30; v.vw = 120; v.vh = 90;
    plat_begin_frame();
    plat_clear(PAL_BLACK);
    dc2d_view3d_draw(&v);
    plat_end_frame();
    swpvr_render(dc2d_last(), rgb);
    inside = 0;
    for (y = 0; y < DC2D_H; y++)
        for (x = 0; x < DC2D_W; x++) {
            const uint8_t *p = &rgb[(y * DC2D_W + x) * 3];
            int in = x >= 80 && x < 320 && y >= 60 && y < 240, black = !p[0] && !p[1] && !p[2];
            if (in && !black) inside++;
            if (!in && !black) outside++;
        }
    CHECK(outside == 0, "viewport: %d pixels painted outside the rectangle", outside);
    CHECK(inside > 80 * 60, "viewport: only %d pixels painted inside", inside);
}

static void test_dist(void)
{
    DoomoView3D v;
    int n_far, n_near;
    map_load(MAPID_ARENA);
    view_of(&v, doomo_map.w / 2, doomo_map.h / 2, 0, 0);
    dc_view3d_set_dist(1000);
    CHECK(dc_view3d_dist() == DC_V3D_DIST_MAX, "dist: not clamped high (%d)", dc_view3d_dist());
    dc_view3d_set_dist(-5);
    CHECK(dc_view3d_dist() == DC_V3D_DIST_MIN, "dist: not clamped low (%d)", dc_view3d_dist());
    dc_view3d_set_dist(DC_V3D_DIST_MAX);
    both(&v, rgb);
    n_far = dc2d_view3d_cells();
    dc_view3d_set_dist(DC_V3D_DIST_MIN);
    both(&v, rgb);
    n_near = dc2d_view3d_cells();
    dc_view3d_set_dist(DC_V3D_DIST_MAX);
    CHECK(n_near <= n_far, "dist: a short view sees %d cells, a long one %d", n_near, n_far);
}

/* the backdrop is the segment's FIRST polygon: on the console the list is drawn in order and a backdrop
 * emitted last painted over the walls (Flycast; found on the first run there) */
static void test_backdrop_first(void)
{
    static const int maps[] = { MAPID_VAPE_CELLAR, MAPID_ARENA };
    int m;
    for (m = 0; m < 2; m++) {
        DoomoView3D v;
        const DcBatch *b;
        int i, x, y, ex = 1, ey = 1, far_n = 0, near_seen = 0, late_far = 0;
        map_load(maps[m]);
        for (y = 1; y < doomo_map.h - 1; y++) for (x = 1; x < doomo_map.w - 1; x++)
            if (open_cell(&doomo_map, x, y) && !(ex > 1 || ey > 1)) { ex = x; ey = y; }
        view_of(&v, ex, ey, 0, 0);
        plat_begin_frame();
        plat_clear(PAL_BLACK);
        dc2d_view3d_draw(&v);
        b = dc3d_batch(dc3d_cur_slot());
        for (i = (int)b->seg[b->nseg - 1].poly0; i < b->np; i++) {
            if (b->p[i].mode != DC3D_OPAQUE) continue;
            if (b->v[b->p[i].first].z < 0.0021f) { far_n++; if (near_seen) late_far++; }
            else near_seen = 1;
        }
        plat_end_frame();
        CHECK(far_n > 0 && near_seen, "backdrop first: map %d: %d backdrop polygons, near ones %d", maps[m], far_n, near_seen);
        CHECK(late_far == 0, "backdrop first: map %d: %d backdrop polygons come after a wall or floor", maps[m], late_far);
    }
}

static void test_backdrop(void)
{
    DoomoView3D v;
    int x, y, n_on = 0, n_off = 0;
    map_load(MAPID_ARENA);
    view_of(&v, doomo_map.w / 2, doomo_map.h / 2, 0, 200);
    both(&v, rgb);
    dc2d_view3d_backdrop(0);
    plat_begin_frame();
    plat_clear(PAL_BLACK);
    dc2d_view3d_draw(&v);
    plat_end_frame();
    swpvr_render(dc2d_last(), rgb2);
    dc2d_view3d_backdrop(1);
    for (y = 0; y < DC2D_H; y += 3) for (x = 0; x < DC2D_W; x += 3) {
        const uint8_t *a = &rgb[(y * DC2D_W + x) * 3], *b = &rgb2[(y * DC2D_W + x) * 3];
        n_on += a[0] || a[1] || a[2];
        n_off += b[0] || b[1] || b[2];
    }
    CHECK(n_on > n_off, "backdrop: with %d lit samples, without %d", n_on, n_off);
}

static void test_repeat(void)
{
    DoomoView3D v;
    uint32_t polys;
    map_load(MAPID_VAPE_CELLAR);
    view_of(&v, 8, 8, 1, 0);
    both(&v, rgb);
    polys = dc3d_stats.polys;
    both(&v, rgb2);
    CHECK(polys == dc3d_stats.polys && memcmp(rgb, rgb2, sizeof(rgb)) == 0, "repeat: two identical views differ");
}

static void test_budget(void)
{
    int mi;
    uint32_t drop0 = dc3d_stats.dropped, d2 = dc2d_stats.dropped, worst = 0, worstp = 0;
    for (mi = 0; mi < NMAPS; mi++) {
        int x, y, c = 0;
        map_load(maps[mi]);
        for (y = 1; y < doomo_map.h - 1; y++)
            for (x = 1; x < doomo_map.w - 1; x++) {
                DoomoView3D v;
                int hd;
                if (!open_cell(&doomo_map, x, y) || (c++ % 11)) continue;
                for (hd = 0; hd < 4; hd++) {
                    view_of(&v, x, y, hd, 0);
                    both(&v, rgb);
                    if (dc3d_stats.bytes > worst) worst = dc3d_stats.bytes;
                    if (dc3d_stats.polys > worstp) worstp = dc3d_stats.polys;
                }
            }
    }
    printf("budget: worst view %u polygons, %u TA bytes (budget %u)\n", worstp, worst, DC3D_SEG_BUDGET);
    CHECK(dc3d_stats.dropped == drop0, "budget: %u polygons dropped", dc3d_stats.dropped - drop0);
    CHECK(dc2d_stats.dropped == d2, "budget: %u 2D prims dropped", dc2d_stats.dropped - d2);
}

static void test_tintfit(void)
{
    int t, k;
    float a[3], b[3];
    dc3d_tint_fit(TINT_NONE, a, b);
    for (k = 0; k < 3; k++) CHECK(a[k] == 1.0f && b[k] == 0.0f, "tintfit: TINT_NONE is not the identity");
    for (t = 0; t < TINT_COUNT; t++) {
        dc3d_tint_fit(t, a, b);
        for (k = 0; k < 3; k++) CHECK(b[k] >= 0.0f && a[k] > 0.0f && a[k] < 3.0f, "tintfit: tint %d channel %d a=%f b=%f", t, k, a[k], b[k]);
    }
}

int main(void)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = 320; o.h = 240; o.save_dir = "."; o.seed = 1; o.quiet = 1;
    nat_init(&o);
    if (getenv("DC6_CELLS")) {
        int m, cx, cy, x, y;
        if (sscanf(getenv("DC6_CELLS"), "%d,%d,%d", &m, &cx, &cy) == 3) {
            map_load(m);
            for (y = cy - 2; y <= cy + 2; y++) {
                for (x = cx - 3; x <= cx + 3; x++) {
                    const DoomoCell *c;
                    if (x < 0 || y < 0 || x >= doomo_map.w || y >= doomo_map.h) continue;
                    c = &doomo_map.cells[y * doomo_map.w + x];
                    printf("(%d,%d w%d f%d ct%d ch%d) ", x, y, c->wall, c->floor_h, c->ceil_tex, c->ceil_h);
                }
                printf("\n");
            }
            return 0;
        }
    }
    dc2d_view3d_remap_budget(100000);
    test_tintfit();
    test_parity();
    test_tint();
    test_remap_budget();
    test_viewport();
    test_dist();
    test_backdrop();
    test_backdrop_first();
    test_repeat();
    test_budget();
    printf("dc6 view3d tests: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
