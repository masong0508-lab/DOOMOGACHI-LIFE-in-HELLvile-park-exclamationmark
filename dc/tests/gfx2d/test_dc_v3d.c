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
 *   gov        DC6C: the governor's rule (dc_gov_step: over a mode's limit the view
 *              loses 3 cells, under its comfort level it gains 2, in between it
 *              stays; Smooth / Balanced / Max detail limits 25 / 29 / 31 ms),
 *              the caps of the Combat detail and Draw distance rows, the sparse
 *              ray fan of Smooth, the entry view that starts short and grows,
 *              no work feed: the picture is the caps' only
 *   tintfit    dc3d_tint_fit: identity for TINT_NONE, b >= 0 everywhere
 *   edge wall  (DC6B) an eye by a low map edge sees the solid block past it, not the sky colour
 *   sprites    (DC6B) three billboards in front of the eye: the sprites' own pixels, their rectangles,
 *              a HUD pill over them
 *   tick       (DC6B) the tick's budget cuts the ray distance, and gives it back on an idle tick
 *   sprite budget  (DC6B) six unseen sprites in one view: at most 3 uploads, the rest wait
 * DC6_ONLY=sprites runs the DC6B tests alone (30 s), DC6_ONLY=parity the parity alone.
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
#include "doomo/sprites.h"
#include "doomo/fixmath.h"
#include "doomo/town.h"

void ref_view3d(const DoomoView3D *v);

#define TOL         40
#define PARITY_MIN  0.80   /* measured 85.6% (79.2% before the map-edge wall); see docs/DREAMCAST.md section 9 */
#define FRAME_MIN   0.45   /* at most 3% of the level frames may be below it */
#define PITCH_MIN   0.85   /* measured 90.1% (80.2% before the map-edge wall) */
#define SPR_MIN     0.72   /* sprite rectangles (measured 79.1%), see test_sprites */
#define SPR_FRAME_MIN 0.25
#define SPR_PX_MIN  0.85   /* the sprites' own pixels (measured 90.5%; 77.9% with the exact rectangle off) */

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
                    if (png && p < FRAME_MIN + (pitch ? 0.15 : 0.0) && nbad < 60) {
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
    CHECK(lowf * 100 <= ln, "parity: %d of %d level frames below %.0f%%", lowf, ln, FRAME_MIN * 100.0);
    printf("parity: %d level frames below %.0f%% (eyes beside a border block lower than their floor: docs/DREAMCAST.md)\n", lowf, FRAME_MIN * 100.0);
    CHECK(lsum / ln >= PARITY_MIN, "parity: level views mean %.1f%% below %.0f%%", lsum / ln * 100.0, PARITY_MIN * 100.0);
    CHECK(psum / pn >= PITCH_MIN, "parity: pitched views mean %.1f%% below %.0f%%", psum / pn * 100.0, PITCH_MIN * 100.0);
    CHECK(nodraw == 0, "parity: %d views saw no cell", nodraw);
}

/* an eye by the map's edge under the open sky looks out over a border block that is lower than the eye:
 * the reference sees the solid block past the map (map_cell_safe) rise 256 units above it, a wall of
 * texture 1; the sky stays above it. Without that wall these views were the sky colour, 0 % */
static void test_edge_wall(void)
{
    static const int heads[2] = { 2, 3 };
    int h;
    for (h = 0; h < 2; h++) {
        DoomoView3D v;
        double p;
        map_load(MAPID_PLUSHIE_FACTORY);
        view_of(&v, 1, 1, heads[h], 0);
        both(&v, rgb);
        p = parity(rgb, 0, 0, DC2D_LW, DC2D_LH);
        CHECK(p > 0.55, "edge wall: factory at 1,1 heading %d matches %.1f%%", heads[h], p * 100.0);
    }
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

/* ---- sprites and the HUD over them (DC6 part 2) -------------------------------------------
 * Three billboards (a shadowed imp, a full-bright tree, a see-through candy) stand 70..200 units
 * in front of an eye whose line of sight is open; the same view goes to the reference raycaster
 * and to the polygons. The parity is measured in the sprites' own rectangles (native pixels):
 * the sprite's colours through the light level's colormap, its transparent texels, its flip, the
 * far-to-near order and the wall that hides it. The HUD the core draws after the view is
 * the same 2D calls on both sides: a pill over a sprite must come out equal. */
static const struct { int spr, flags, light, lat, depth; } spr_set[3] = {
    { SPR_IMPLING, BB_SHADOW, 0, -26, 190 }, { SPR_TREE, BB_FULLBRIGHT, 0, 22, 130 }, { SPR_CANDY, BB_TRANSLUCENT, 0, 0, 80 } };

static int line_clear(int cx, int cy, float dx, float dy, float len)
{
    float t;
    for (t = 0.0f; t <= len; t += 8.0f) {
        int x = (int)floorf(((float)(cx * 64 + 32) + dx * t) / 64.0f), y = (int)floorf(((float)(cy * 64 + 32) + dy * t) / 64.0f);
        const DoomoCell *c;
        if (x < 1 || y < 1 || x >= doomo_map.w - 1 || y >= doomo_map.h - 1) return 0;
        c = &doomo_map.cells[y * doomo_map.w + x];
        if (c->wall || map_floor_z(&doomo_map, x, y) != map_floor_z(&doomo_map, cx, cy)) return 0;
    }
    return 1;
}

static DoomoBillboard sbb[3];

/* the three sprites in front of the eye (cx,cy), heading hd; NULL when the line is not open */
static int place_sprites(DoomoView3D *v, int cx, int cy, int hd)
{
    float dx = (float)fx_cos(v->angle) / 65536.0f, dy = (float)fx_sin(v->angle) / 65536.0f;
    int i;
    (void)hd;
    if (!line_clear(cx, cy, dx, dy, 210.0f)) return 0;
    for (i = 0; i < 3; i++) {
        float x = (float)(cx * 64 + 32) + dx * (float)spr_set[i].depth - dy * (float)spr_set[i].lat;
        float y = (float)(cy * 64 + 32) + dy * (float)spr_set[i].depth + dx * (float)spr_set[i].lat;
        memset(&sbb[i], 0, sizeof(sbb[i]));
        sbb[i].x = (fix16)(x * 65536.0f); sbb[i].y = (fix16)(y * 65536.0f);
        sbb[i].z = (fix16)(map_floor_z(&doomo_map, cx, cy) * 65536);
        sbb[i].img = sprite_image(spr_set[i].spr, 0);
        sbb[i].scale = 256;
        sbb[i].flags = (uint8_t)spr_set[i].flags;
        sbb[i].light = (uint8_t)spr_set[i].light;
    }
    v->sprites = sbb;
    v->sprite_count = 3;
    v->player_sprite = -1;
    return 1;
}

/* a sprite's rectangle on the 320x240 picture */
static int sprite_rect(const DoomoView3D *v, const DoomoBillboard *b, int *x0, int *y0, int *x1, int *y1)
{
    const Dc3dCam *c = dc2d_view3d_cam();
    float dx = (float)fx_cos(v->angle) / 65536.0f, dy = (float)fx_sin(v->angle) / 65536.0f;
    float ex = (float)v->x / 65536.0f, ey = (float)v->y / 65536.0f, ez = (float)v->z / 65536.0f;
    float x = (float)b->x / 65536.0f, y = (float)b->y / 65536.0f, z = (float)b->z / 65536.0f;
    float d = (x - ex) * dx + (y - ey) * dy, lat = (x - ex) * -dy + (y - ey) * dx;
    float w = (float)b->img->w * (float)b->scale / 256.0f, h = (float)b->img->h * (float)b->scale / 256.0f, k;
    if (d < 4.0f) return 0;
    k = c->focal / d;
    *x0 = (int)((c->cx + (lat - w * 0.5f) * k) / 2.0f);
    *x1 = (int)((c->cx + (lat + w * 0.5f) * k) / 2.0f);
    *y1 = (int)((c->cy - (z - ez) * k) / 2.0f);
    *y0 = (int)((c->cy - (z + h - ez) * k) / 2.0f);
    if (*x0 < 0) *x0 = 0;
    if (*y0 < 0) *y0 = 0;
    if (*x1 > DC2D_LW) *x1 = DC2D_LW;
    if (*y1 > DC2D_LH) *y1 = DC2D_LH;
    return *x1 > *x0 && *y1 > *y0;
}

/* the pixels where the reference drew the sprite (its native pixel is one of the texel's light levels):
 * how many, and how many of them the polygons match */
static void sprite_pixels(const DoomoView3D *v, const DoomoBillboard *b, int *vis, int *hit)
{
    const Dc3dCam *c = dc2d_view3d_cam();
    float dx = (float)fx_cos(v->angle) / 65536.0f, dy = (float)fx_sin(v->angle) / 65536.0f;
    float x = (float)b->x / 65536.0f - (float)v->x / 65536.0f, y = (float)b->y / 65536.0f - (float)v->y / 65536.0f;
    float d = x * dx + y * dy, lat = x * -dy + y * dx;
    int sw = (int)((float)b->img->w * (float)b->scale / 256.0f * c->focal / d / 2.0f);
    int sh = (int)((float)b->img->h * (float)b->scale / 256.0f * c->focal / d / 2.0f);
    int cx = (int)((c->cx + lat * c->focal / d) / 2.0f), sx0 = cx - sw / 2;
    int sy1 = (int)((c->cy - ((float)b->z / 65536.0f - (float)v->z / 65536.0f) * c->focal / d) / 2.0f), sy0 = sy1 - sh;
    int px, py;
    for (px = sx0; px < sx0 + sw; px++)
        for (py = sy0; py < sy0 + sh; py++) {
            int tx, ty, l, seen = 0;
            uint8_t t;
            if (px < 0 || py < 0 || px >= DC2D_LW || py >= DC2D_LH) continue;
            tx = (px - sx0) * b->img->w / sw;
            if (b->flags & BB_FLIPX) tx = b->img->w - 1 - tx;
            ty = (py - sy0) * b->img->h / sh;
            t = b->img->pix[ty * b->img->w + tx];
            if (!t) continue;
            for (l = 1; l < DOOMO_LIGHT_LEVELS; l++) if (doomo_colormap[l][t] == nat_fb[py * nat_w + px]) seen = 1;
            if (!seen) continue;
            (*vis)++;
            *hit += match_px(rgb, px, py);
        }
}

static void test_sprites(void)
{
    long pvis = 0, phit = 0;
    int mi, k, frames = 0, rects = 0, lowr = 0;
    double sum = 0, hsum = 0;
    int hn = 0;
    for (mi = 0; mi < NMAPS; mi++) {
        int cells[2000][2], nc = 0, x, y, stride;
        map_load(maps[mi]);
        for (y = 1; y < doomo_map.h - 1; y++)
            for (x = 1; x < doomo_map.w - 1; x++)
                if (open_cell(&doomo_map, x, y) && nc < 2000) { cells[nc][0] = x; cells[nc][1] = y; nc++; }
        stride = nc / 40 > 0 ? nc / 40 : 1;
        for (k = 0; k < nc; k += stride) {
            int hd;
            for (hd = 0; hd < 4; hd++) {
                DoomoView3D v;
                int i;
                view_of(&v, cells[k][0], cells[k][1], hd, 0);
                if (!place_sprites(&v, cells[k][0], cells[k][1], hd)) continue;
                plat_begin_frame();
                plat_clear(PAL_BLACK);
                ref_view3d(&v);
                dc2d_view3d_draw(&v);
                /* the HUD over the middle sprite: the same 2D calls on both sides */
                plat_rect(130, 90, 60, 40, PAL_UI_GOLD);
                plat_text(134, 100, "HUD 100", PAL_BLACK);
                plat_end_frame();
                swpvr_render(dc2d_last(), rgb);
                frames++;
                if (getenv("DC6_PNG") && frames % 40 == 1) {
                    char nm[512];
                    snprintf(nm, sizeof(nm), "%s/spr_%03d_nat.png", getenv("DC6_PNG"), frames);
                    nat_png_write(nm, nat_fb, nat_w, nat_h, 1);
                    snprintf(nm, sizeof(nm), "%s/spr_%03d_dc.png", getenv("DC6_PNG"), frames);
                    png_write_rgb(nm, rgb, DC2D_W, DC2D_H);
                }
                for (i = 0; i < 3; i++) {
                    int x0, y0, x1, y1;
                    double p;
                    if (!sprite_rect(&v, &sbb[i], &x0, &y0, &x1, &y1)) continue;
                    if ((x1 - x0) * (y1 - y0) < 64) continue;
                    p = parity(rgb, x0, y0, x1 - x0, y1 - y0);
                    sum += p; rects++;
                    if (p < SPR_FRAME_MIN) lowr++;
                    { int a = 0, h = 0; sprite_pixels(&v, &sbb[i], &a, &h); pvis += a; phit += h; }
                }
                hsum += parity(rgb, 130, 90, 60, 40); hn++;
            }
        }
    }
    printf("sprites: the sprites' own pixels (%ld seen in the reference): %.1f%% match\n", pvis, pvis ? (double)phit / (double)pvis * 100.0 : 0.0);
    printf("sprites: %d frames, %d sprite rectangles, mean %.1f%% within %d/255; %d below %.0f%%; HUD over them %.1f%%\n",
           frames, rects, sum / (rects ? rects : 1) * 100.0, TOL, lowr, SPR_FRAME_MIN * 100.0, hsum / (hn ? hn : 1) * 100.0);
    CHECK(rects > 100, "sprites: only %d rectangles measured", rects);
    CHECK(sum / (rects ? rects : 1) >= SPR_MIN, "sprites: mean %.1f%% below %.0f%%", sum / (rects ? rects : 1) * 100.0, SPR_MIN * 100.0);
    CHECK(pvis > 100000 && (double)phit / (double)pvis >= SPR_PX_MIN, "sprites: own pixels match %.1f%% (%ld seen), below %.0f%%",
          pvis ? (double)phit / (double)pvis * 100.0 : 0.0, pvis, SPR_PX_MIN * 100.0);
    CHECK(hsum / (hn ? hn : 1) >= 0.97, "sprites: the HUD over them matches only %.1f%%", hsum / (hn ? hn : 1) * 100.0);
}

/* the tick's budget (view3d.c view_budget): what is left of the 33 ms sets how far the rays go. The host
 * clock is set by the test: every view "costs" dc3d_test_step, the tick has run dc3d_test_tick_us */
extern uint32_t dc3d_test_step, dc3d_test_tick_us;
static void test_tick_budget(void)
{
    DoomoView3D v;
    int full, mid, low, n_mid, n_full, cut0;
    map_load(MAPID_ARENA);
    view_of(&v, doomo_map.w / 2, doomo_map.h / 2, 0, 0);
    dc3d_test_step = 10000;                       /* a view costs 10 ms at the full distance: K = 312 us per cell */
    dc3d_test_tick_us = 0;
    both(&v, rgb);
    both(&v, rgb);
    full = dc3d_stats.cast_dist;
    n_full = dc2d_view3d_cells();
    CHECK(full == dc_view3d_dist(), "budget: an idle tick casts %d cells, the draw distance is %d", full, dc_view3d_dist());
    cut0 = (int)dc3d_stats.cut_n;
    dc3d_test_tick_us = 20000;                    /* 20 ms gone: 33.3 - 7 - 20 = 6.3 ms left, 1.3 x 312 us a cell: 15 cells */
    dc3d_test_step = 15 * 312;                    /* (a shorter view costs less: the model stays at K = 312) */
    both(&v, rgb);
    mid = dc3d_stats.cast_dist;
    CHECK(mid >= 14 && mid <= 16, "budget: 20 ms spent, the view casts %d cells (want about 15)", mid);
    CHECK((int)dc3d_stats.cut_n == cut0 + 1, "budget: the cut view is counted");
    dc3d_test_tick_us = 33000;                    /* nothing left: the shortest view */
    dc3d_test_step = DC_V3D_DIST_MIN * 312;
    both(&v, rgb);
    low = dc3d_stats.cast_dist;
    n_mid = dc2d_view3d_cells();
    CHECK(low == DC_V3D_DIST_MIN, "budget: no time left, the view casts %d cells (want %d)", low, DC_V3D_DIST_MIN);
    CHECK(n_mid < n_full, "budget: the shortest view sees %d cells, the full one %d", n_mid, n_full);
    dc3d_test_tick_us = 0;
    dc3d_test_step = 10000;
    both(&v, rgb);
    CHECK(dc3d_stats.cast_dist == full, "budget: the next idle tick is back at %d (%d)", full, (int)dc3d_stats.cast_dist);
    dc3d_test_step = 0;
    dc3d_test_tick_us = 0;
}

/* DC6D: with a work feed (the console) the view leaves 15 ms for what comes after it, and a tick that has no
 * room for even the shortest view (10 cells, 1.3 x K us a cell) shows the last tick's picture again, once; with
 * nothing to show again (the one before was a repeat, or no view at all) it draws the backdrop alone */
static void test_view_hold(void)
{
    DoomoView3D v, w;
    uint32_t segs0, held0, bare0, cut0;
    int i, same = 1, same_bare = 1;
    map_load(MAPID_ARENA);
    view_of(&v, doomo_map.w / 2, doomo_map.h / 2, 0, 0);
    view_of(&w, doomo_map.w / 2, doomo_map.h / 2, 2, 0);          /* another heading: a different picture */
    dc_view3d_gov_reset();
    dc3d_test_step = 10000; dc3d_test_tick_us = 0;
    both(&v, rgb); both(&v, rgb);                                 /* K = 312 us a cell; no feed yet: nothing is held */
    CHECK(dc3d_stats.held_n == 0 && dc3d_stats.bare_n == 0, "hold: no work feed, but %u held %u bare", dc3d_stats.held_n, dc3d_stats.bare_n);
    dc3d_test_tick_us = 33000;
    dc3d_test_step = DC_V3D_DIST_MIN * 312;                      /* (the shortest view costs what the model says) */
    both(&v, rgb);
    dc3d_test_step = 10000;
    CHECK(dc3d_stats.held_n == 0 && dc3d_stats.bare_n == 0, "hold: no work feed and 33 ms gone: %u held %u bare (the shortest view is built)",
          dc3d_stats.held_n, dc3d_stats.bare_n);
    dc3d_test_tick_us = 0;
    dc_view3d_work(10000);                                        /* the feed is on */
    both(&v, rgb);
    segs0 = dc3d_stats.segs; held0 = dc3d_stats.held_n; bare0 = dc3d_stats.bare_n; cut0 = dc3d_stats.cut_n;
    CHECK(dc2d_view3d_cells() > 0, "hold: an idle tick builds nothing");
    memcpy(rgb2, rgb, sizeof(rgb));
    dc3d_test_tick_us = 12000;                                    /* 33.3 - 15 - 12 = 6.3 ms left: the shortest view costs 4.1 */
    dc3d_test_step = 15 * 312;
    both(&w, rgb);
    dc3d_test_step = 10000;
    CHECK(dc3d_stats.segs == segs0 + 1 && dc3d_stats.held_n == held0 && dc3d_stats.bare_n == bare0,
          "hold: 12 ms gone must still build a view (segments %u -> %u, held %u, bare %u)", segs0, dc3d_stats.segs, dc3d_stats.held_n, dc3d_stats.bare_n);
    CHECK(dc3d_stats.cut_n == cut0 + 1, "hold: ... a cut one");
    memcpy(rgb2, rgb, sizeof(rgb));                               /* the picture of the last built view */
    segs0 = dc3d_stats.segs;
    dc3d_test_tick_us = 16000;                                    /* 2.3 ms left: no room */
    both(&v, rgb);                                                /* (the eye turned: a built view would differ) */
    CHECK(dc3d_stats.held_n == held0 + 1 && dc3d_stats.segs == segs0 && dc3d_stats.bare_n == bare0,
          "hold: 16 ms gone must show the last view again (held %u, segments %u -> %u, bare %u)", dc3d_stats.held_n, segs0, dc3d_stats.segs, dc3d_stats.bare_n);
    for (i = 0; i < DC2D_W * DC2D_H * 3; i++) if (rgb[i] != rgb2[i]) { same = 0; break; }
    CHECK(same, "hold: the repeated view is not the last picture (byte %d differs)", i);
    held0 = dc3d_stats.held_n;
    dc3d_test_tick_us = 16000;                                    /* again: the one before was a repeat, nothing to show */
    both(&v, rgb);
    CHECK(dc3d_stats.bare_n == bare0 + 1 && dc3d_stats.held_n == held0 && dc3d_stats.segs == segs0 + 1,
          "hold: a second late tick in a row must draw the backdrop (bare %u, held %u)", dc3d_stats.bare_n, dc3d_stats.held_n);
    CHECK(dc2d_view3d_cells() == 0, "hold: the backdrop alone has %d cells", dc2d_view3d_cells());
    for (i = 0; i < DC2D_W * DC2D_H * 3; i++) if (rgb[i] != rgb2[i]) { same_bare = 0; break; }
    CHECK(!same_bare, "hold: the backdrop alone looks like the last view");
    dc3d_test_tick_us = 0;                                        /* and an idle tick builds again */
    segs0 = dc3d_stats.segs; held0 = dc3d_stats.held_n; bare0 = dc3d_stats.bare_n;
    both(&v, rgb);
    CHECK(dc3d_stats.segs == segs0 + 1 && dc3d_stats.held_n == held0 && dc3d_stats.bare_n == bare0 && dc2d_view3d_cells() > 0,
          "hold: the next idle tick must build the view (cells %d)", dc2d_view3d_cells());
    /* another map or viewport is not "the last picture": the backdrop */
    map_load(MAPID_VAPE_CELLAR);
    view_of(&w, doomo_map.w / 2, doomo_map.h / 2, 0, 0);
    dc3d_test_tick_us = 16000;
    bare0 = dc3d_stats.bare_n; held0 = dc3d_stats.held_n;
    both(&w, rgb);
    CHECK(dc3d_stats.bare_n == bare0 + 1 && dc3d_stats.held_n == held0, "hold: a view of another map must not show the old picture");
    dc3d_test_step = 0;
    dc3d_test_tick_us = 0;
    dc_view3d_gov_reset();
}

/* a sprite the cache has not seen costs an upload: a view makes at most V3D_SPR_BUDGET new ones, the rest
 * wait a tick (nine uploads in one tick were a 38 ms tick on Flycast) */
static void test_sprite_budget(void)
{
    static uint8_t pix[6][16 * 16];
    static DoomoImage img[6];
    static DoomoBillboard bb[6];
    DoomoView3D v;
    int i, d1, d2, k, x, y, ex = 0, ey = 0, found = 0;
    map_load(MAPID_VAPE_CELLAR);
    for (y = 2; y < doomo_map.h - 2 && !found; y++)
        for (x = 2; x < doomo_map.w - 2 && !found; x++) {
            if (!open_cell(&doomo_map, x, y)) continue;
            view_of(&v, x, y, 0, 0);
            if (line_clear(x, y, (float)fx_cos(v.angle) / 65536.0f, (float)fx_sin(v.angle) / 65536.0f, 200.0f)) { found = 1; ex = x; ey = y; }
        }
    CHECK(found, "sprite budget: no open line in the map");
    for (i = 0; i < 6; i++) {
        float fx = (float)fx_cos(v.angle) / 65536.0f, fy = (float)fx_sin(v.angle) / 65536.0f, dist = 70.0f + 25.0f * (float)i;
        float lat = (float)(i - 3) * 8.0f;
        for (k = 0; k < 16 * 16; k++) pix[i][k] = (uint8_t)(1 + (k + i) % 40);
        img[i].w = 16; img[i].h = 16; img[i].pix = pix[i]; img[i].uid = doomo_image_new_uid();
        bb[i].x = (fix16)(((float)(v.x / 65536) + fx * dist - fy * lat) * 65536.0f);
        bb[i].y = (fix16)(((float)(v.y / 65536) + fy * dist + fx * lat) * 65536.0f);
        bb[i].z = (fix16)(map_floor_z(&doomo_map, ex, ey) * 65536);
        bb[i].img = &img[i]; bb[i].scale = 256; bb[i].flags = BB_FULLBRIGHT;
    }
    v.sprites = bb; v.sprite_count = 6; v.player_sprite = -1;
    both(&v, rgb);
    d1 = (int)dc3d_stats.deferred;
    CHECK(d1 >= 3, "sprite budget: six new sprites in one view, %d waited (want at least 3)", d1);
    both(&v, rgb);
    both(&v, rgb);
    d2 = (int)dc3d_stats.deferred;
    both(&v, rgb);
    CHECK((int)dc3d_stats.deferred == d2, "sprite budget: still waiting after three views (%d -> %d)", d2, (int)dc3d_stats.deferred);
}

/* ---- DC6C: the governor ------------------------------------------------------------------- */
static int view_dist_of(const DoomoView3D *v)
{
    both(v, rgb);
    return dc_view3d_dist();
}

static void test_gov_rule(void)
{
    int d;
    /* the rule: -3 over the limit, +2 under the comfort level, still in between */
    CHECK(dc_gov_step(20, 32, OPT3D_BALANCED, 30000) == 17, "gov: Balanced at 30 ms must lose 3 cells");
    CHECK(dc_gov_step(20, 32, OPT3D_BALANCED, 20000) == 22, "gov: Balanced at 20 ms must gain 2 cells");
    CHECK(dc_gov_step(20, 32, OPT3D_BALANCED, 27000) == 20, "gov: Balanced at 27 ms must hold (between the limits)");
    /* the modes differ: 28 ms is fine for Max detail, over the limit for Smooth, in between for Balanced */
    CHECK(dc_gov_step(20, 32, OPT3D_MAX, 28000) == 20, "gov: Max detail must hold at 28 ms");
    CHECK(dc_gov_step(20, 32, OPT3D_SMOOTH, 28000) == 17, "gov: Smooth must lose cells at 28 ms");
    CHECK(dc_gov_step(20, 32, OPT3D_MAX, 32000) == 17, "gov: Max detail must lose cells at 32 ms");
    CHECK(dc_gov_step(20, 32, OPT3D_BALANCED, 28000) == 20, "gov: Balanced must hold at 28 ms");
    /* the clamps: never under the minimum, never over the cap */
    for (d = DC_V3D_DIST_MIN; d <= DC_V3D_DIST_MAX; d++) {
        int a = dc_gov_step(d, 20, OPT3D_BALANCED, 60000), b = dc_gov_step(d, 20, OPT3D_BALANCED, 1000);
        CHECK(a >= DC_V3D_DIST_MIN && a <= d, "gov: %d at 60 ms went to %d", d, a);
        CHECK(b <= 20 || b <= d, "gov: %d at 1 ms went to %d (cap 20)", d, b);
        CHECK(b >= DC_V3D_DIST_MIN, "gov: %d at 1 ms went to %d", d, b);
    }
    CHECK(dc_gov_step(DC_V3D_DIST_MIN, 32, OPT3D_BALANCED, 90000) == DC_V3D_DIST_MIN, "gov: not under the minimum");
    CHECK(dc_gov_step(31, 32, OPT3D_BALANCED, 1000) == 32, "gov: not over the cap (32)");
    CHECK(dc_gov_step(12, 12, OPT3D_BALANCED, 1000) == 12, "gov: not over the Near cap (12)");
    /* an unknown mode reads as Balanced */
    CHECK(dc_gov_step(20, 32, 77, 30000) == 17, "gov: an unknown mode is Balanced");
}

static void test_gov_caps(void)
{
    CHECK(dc_gov_cap(OPT3D_BALANCED, 0) == 32 && dc_gov_cap(OPT3D_MAX, 0) == 32, "gov: Balanced / Max detail see 32 cells");
    CHECK(dc_gov_cap(OPT3D_SMOOTH, 0) == DC_V3D_DIST_SMOOTH, "gov: Smooth caps the view at %d cells", DC_V3D_DIST_SMOOTH);
    CHECK(dc_gov_cap(OPT3D_BALANCED, 1) == DC_V3D_DIST_NEAR && dc_gov_cap(OPT3D_MAX, 1) == DC_V3D_DIST_NEAR,
          "gov: Draw distance Near caps the view at %d cells", DC_V3D_DIST_NEAR);
    CHECK(dc_gov_cap(OPT3D_SMOOTH, 1) == DC_V3D_DIST_NEAR, "gov: Near under Smooth is still Near");
    CHECK(dc_gov_rays(OPT3D_SMOOTH) == 4 && dc_gov_rays(OPT3D_BALANCED) == 4 && dc_gov_rays(OPT3D_MAX) == 2,
          "gov: ray fans 4 (Smooth, Balanced) / 2 (Max detail) columns");
    CHECK(dc_gov_tex(OPT3D_SMOOTH) < dc_gov_tex(OPT3D_BALANCED) && dc_gov_tex(OPT3D_BALANCED) < dc_gov_tex(OPT3D_MAX),
          "gov: new textures per view must grow Smooth < Balanced < Max detail");
}

/* the options reach the view: Near and Smooth shorten it, a shorter view sees fewer cells, and Smooth's
 * sparse ray fan loses no cell the dense one marks (it marks the corners' neighbours instead) */
static void test_gov_options(void)
{
    DoomoView3D v;
    int n_bal, n_smooth, n_near, n_bal20, n_smooth20;
    map_load(MAPID_ARENA);
    view_of(&v, doomo_map.w / 2, doomo_map.h / 2, 0, 0);
    opt_set(OPT_3D_FAST, OPT3D_BALANCED); opt_set(OPT_DRAW_DIST, 1);
    CHECK(view_dist_of(&v) == DC_V3D_DIST_MAX, "gov: Balanced draws %d cells", dc_view3d_dist());
    n_bal = dc2d_view3d_cells();
    opt_set(OPT_3D_FAST, OPT3D_SMOOTH);
    CHECK(view_dist_of(&v) == DC_V3D_DIST_SMOOTH, "gov: Smooth draws %d cells", dc_view3d_dist());
    n_smooth = dc2d_view3d_cells();
    opt_set(OPT_3D_FAST, OPT3D_MAX);
    CHECK(view_dist_of(&v) == DC_V3D_DIST_MAX, "gov: Max detail draws %d cells", dc_view3d_dist());
    opt_set(OPT_3D_FAST, OPT3D_BALANCED); opt_set(OPT_DRAW_DIST, 0);
    CHECK(view_dist_of(&v) == DC_V3D_DIST_NEAR, "gov: Near draws %d cells", dc_view3d_dist());
    n_near = dc2d_view3d_cells();
    /* (the sparse fan marks the cells around more corners, so Smooth may see a few more than Balanced in a small map) */
    CHECK(n_near < n_smooth && n_near < n_bal && n_smooth * 10 <= n_bal * 11, "gov: cells %d (Near) %d (Smooth) %d (Balanced)", n_near, n_smooth, n_bal);
    /* a manual cap goes under the rows: set_dist(11) is 11 with the rows at their longest */
    opt_set(OPT_DRAW_DIST, 1);
    dc_view3d_set_dist(11);
    CHECK(view_dist_of(&v) == 11, "gov: a manual cap of 11 draws %d cells", dc_view3d_dist());
    dc_view3d_set_dist(20);
    opt_set(OPT_3D_FAST, OPT3D_BALANCED);
    both(&v, rgb); n_bal20 = dc2d_view3d_cells();
    opt_set(OPT_3D_FAST, OPT3D_SMOOTH);
    both(&v, rgb); n_smooth20 = dc2d_view3d_cells();
    CHECK(n_smooth20 * 10 >= n_bal20 * 9, "gov: at 20 cells the sparse fan sees %d cells, the dense one %d", n_smooth20, n_bal20);
    dc_view3d_set_dist(DC_V3D_DIST_MAX);
    opt_set(OPT_3D_FAST, OPT3D_BALANCED); opt_set(OPT_DRAW_DIST, 1);
}

/* the work feed: an entry starts short and grows, work over the limit shortens, no feed leaves the caps alone */
static void test_gov_feed(void)
{
    DoomoView3D v;
    int i, d, prev, e;
    map_load(MAPID_ARENA);
    view_of(&v, doomo_map.w / 2, doomo_map.h / 2, 0, 0);
    opt_set(OPT_3D_FAST, OPT3D_BALANCED); opt_set(OPT_DRAW_DIST, 1);
    dc_view3d_gov_reset();
    CHECK(view_dist_of(&v) == DC_V3D_DIST_MAX, "gov: no work feed, but %d cells", dc_view3d_dist());
    /* the scene entry: the first view after the feed starts is short */
    dc_view3d_work(15000);
    dc_view3d_gov_reset();
    dc_view3d_work(15000);
    e = view_dist_of(&v);
    CHECK(e == DC_V3D_DIST_ENTRY, "gov: the entry view draws %d cells, not %d", e, DC_V3D_DIST_ENTRY);
    /* with room (15 ms of work) it grows 2 cells a view up to the cap */
    prev = e;
    for (i = 0; i < 12; i++) {
        dc_view3d_work(15000);
        d = view_dist_of(&v);
        CHECK(d == (prev + 2 > DC_V3D_DIST_MAX ? DC_V3D_DIST_MAX : prev + 2), "gov: view %d draws %d cells after %d", i, d, prev);
        prev = d;
    }
    CHECK(prev == DC_V3D_DIST_MAX, "gov: the view did not grow back to %d (%d)", DC_V3D_DIST_MAX, prev);
    /* a tick over the limit shortens it, a run of them goes down to the minimum and stays */
    dc_view3d_work(40000);
    d = view_dist_of(&v);
    CHECK(d == DC_V3D_DIST_MAX - 3, "gov: 40 ms of work must shorten the view by 3 (%d)", d);
    for (i = 0; i < 12; i++) { dc_view3d_work(40000); d = view_dist_of(&v); }
    CHECK(d == DC_V3D_DIST_MIN, "gov: a run of 40 ms ticks must end at %d cells (%d)", DC_V3D_DIST_MIN, d);
    /* the same work in between the limits holds it */
    dc_view3d_work(27000);
    CHECK(view_dist_of(&v) == DC_V3D_DIST_MIN, "gov: 27 ms must hold the view");
    /* the cap wins over the growth: Near */
    opt_set(OPT_DRAW_DIST, 0);
    for (i = 0; i < 12; i++) { dc_view3d_work(10000); d = view_dist_of(&v); }
    CHECK(d == DC_V3D_DIST_NEAR, "gov: with Draw distance Near the view stops at %d (%d)", DC_V3D_DIST_NEAR, d);
    /* a scene change (frames with no view) is an entry again */
    opt_set(OPT_DRAW_DIST, 1);
    for (i = 0; i < 4; i++) { plat_begin_frame(); plat_end_frame(); }
    dc_view3d_work(10000);
    e = view_dist_of(&v);
    CHECK(e == DC_V3D_DIST_ENTRY, "gov: a view after 4 frames without one must be an entry (%d)", e);
    dc_view3d_gov_reset();
    opt_set(OPT_3D_FAST, OPT3D_BALANCED);
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

/* Verify the optimized crowded-view sort preserves descending depth,
 * stable equal-depth order, the 16-sprite fast-path boundary and reversal. */
void dc2d_v3d_sort_test(int16_t *order, float *depth, int n);
static void test_sort_billboards(void)
{
    int n, i, j;
    int16_t order[256];
    float depth[256];
    static const int sizes[] = { 0, 1, 2, 15, 16, 17, 31, 32, 127, 256 };
    for (j = 0; j < (int)(sizeof sizes / sizeof sizes[0]); j++) {
        n = sizes[j];
        for (i = 0; i < n; i++) {
            order[i] = (int16_t)i;
            depth[i] = (float)((i * 11 + 3) % 17);
        }
        dc2d_v3d_sort_test(order, depth, n);
        for (i = 1; i < n; i++) {
            CHECK(depth[i - 1] >= depth[i],
                  "sort: %d items not descending at %d", n, i);
            if (depth[i - 1] == depth[i])
                CHECK(order[i - 1] < order[i],
                      "sort: %d items lost original order at %d", n, i);
        }
    }
    for (i = 0; i < 256; i++) {
        order[i] = (int16_t)i;
        depth[i] = (float)i;
    }
    dc2d_v3d_sort_test(order, depth, 256);
    for (i = 0; i < 256; i++)
        CHECK(order[i] == 255 - i, "sort: reverse-sorted crowd at %d", i);
}

int main(void)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = 320; o.h = 240; o.save_dir = "."; o.seed = 1; o.quiet = 1;
    nat_init(&o);
    opt_set(OPT_DRAW_DIST, 1); opt_set(OPT_3D_FAST, OPT3D_BALANCED);   /* DC6C: the view reads these two rows (the other options stay at 0, as the parity numbers were measured) */
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
    test_sort_billboards();
    if (getenv("DC6_GOV_ONLY")) {          /* the governor's tests alone (seconds, not minutes) */
        test_gov_rule(); test_gov_caps(); test_gov_options(); test_gov_feed();
        printf("dc6 view3d governor tests: %d checks, %d failed\n", checks, fails);
        return fails ? 1 : 0;
    }
    test_tintfit();
    test_tick_budget();
    test_view_hold();
    if (getenv("DC6_ONLY") && !strcmp(getenv("DC6_ONLY"), "hold")) {      /* the budget tests alone (seconds) */
        printf("dc6 view3d tests: %d checks, %d failed\n", checks, fails);
        return fails ? 1 : 0;
    }
    test_sprite_budget();
    if (getenv("DC6_ONLY") && !strcmp(getenv("DC6_ONLY"), "sprites")) {
        test_edge_wall();
        test_sprites();
        printf("dc6 view3d tests: %d checks, %d failed\n", checks, fails);
        return fails ? 1 : 0;
    }
    if (getenv("DC6_ONLY") && !strcmp(getenv("DC6_ONLY"), "parity")) {
        test_parity();
        printf("dc6 view3d tests: %d checks, %d failed\n", checks, fails);
        return fails ? 1 : 0;
    }
    test_parity();
    test_edge_wall();
    test_sprites();
    test_tint();
    test_remap_budget();
    test_viewport();
    test_dist();
    test_backdrop();
    test_backdrop_first();
    test_repeat();
    test_budget();
    test_gov_rule();
    test_gov_caps();
    test_gov_options();
    test_gov_feed();
    printf("dc6 view3d tests: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
