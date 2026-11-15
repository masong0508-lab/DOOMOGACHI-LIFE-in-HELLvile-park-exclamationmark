/* test_dream_speed.c - S2-4d, the dreams at a steady 30 (docs/GBA.md "Speed wave 2: S2-4d").
 *
 * Every drawing step of the package is exact, so each is played against the drawing it replaced:
 *   - the sky's wave table and its rows cut at the floor (dream_draw_sky_rows) against the old sky (one ui_wave per
 *     column and border, kept here verbatim), and the sky's skipped rows against what the vignettes paint over them;
 *   - the platform's one-call paths (plat_hshapes, plat_tile_rows: the native platform draws them as plat_ext.h says
 *     when nat_fast2d is set, as the GBA does) against the core's own rects;
 *   - the shape helpers (dream_circles, dream_ellipses) against ui_fill_circle / ui_fill_ellipse;
 *   - the rim (its bumps made once) against the old rim, kept here verbatim;
 *   - the thought bubble: what it hides when it fills the screen;
 *   - the caption's cloud box (the bump rows its rects cover left out) against the old box, kept here verbatim;
 *   - the held wake-up choice: holds happen, a held tick changes nothing outside its band (but the toast's lane,
 *     which S2-1 puts back and draws again), the buttons' insides are the full frame's, and a cursor moved away and
 *     back leaves the band as it was (nothing piles up). */
#include "test.h"
#include "../src/concert_int.h"
#include "../native/native.h"

static uint32_t h2(uint32_t a, uint32_t b) { return doomo_hash32(a * 2246822519u ^ (b * 3266489917u + 7u)); }

static void cspan(int x0, int x1, int y, int h, uint8_t c)
{
    if ((x0 + y) & 1) x0++;
    if (x1 > x0) plat_checker(x0, y, x1 - x0, h, c);
}

static uint8_t sky_col(int i, int tier, int nightmare)
{
    static const uint8_t cute[6] = { RAMP_PASTEL_PINK, RAMP_PASTEL_PINK, RAMP_VAPE, RAMP_VAPE, RAMP_SKY, RAMP_SKY };
    if (tier == 0) return PAL_RAMP(cute[i], 7 - (i & 1));
    if (tier == 1) return PAL_RAMP(RAMP_VAPE, 6 - i / 2 - (i & 1));
    return nightmare ? PAL_RAMP(RAMP_BLOOD, 3 - i / 2) : PAL_RAMP(RAMP_VAPE, 3 - i / 2);
}

/* dream_draw_sky before S2-4d (d5305653), verbatim */
static void old_sky(int creep, int nightmare, uint32_t t)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, x, band = sh / 6 + 1;
    int tier = creep < 6 ? 0 : creep < 12 ? 1 : 2;
    plat_rect(0, 0, sw, band + 8, sky_col(0, tier, nightmare));
    for (i = 1; i < 6; i++) {
        uint8_t c = sky_col(i, tier, nightmare);
        for (x = 0; x < sw; x += 8) {
            int y0 = i * band + ui_wave((int)t + x * 2 + i * 40, 160, 3 + tier);
            plat_rect(x, y0, 8, (i == 5 ? sh : (i + 1) * band + 6) - y0, c);
            cspan(x, x + 8, y0 - 2, 2, c);
        }
    }
    for (i = 0; i < 5; i++) {
        int px = (int)((h2((uint32_t)i, 17u) % (uint32_t)(sw + 60) + t / (2 + (uint32_t)i % 3)) % (uint32_t)(sw + 60)) - 30;
        int py = 18 + (int)(h2((uint32_t)i, 19u) % (uint32_t)(sh / 2));
        uint8_t pc = tier == 0 ? PAL_PAINT(PAINT_WHITE, 2) : tier == 1 ? PAL_RAMP(RAMP_VAPE, 6) :
                     nightmare ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_VAPE, 1);
        ui_fill_ellipse(px, py, 14 + (i & 1) * 6, 4, pc);
        ui_fill_ellipse(px - 6, py - 3, 7, 4, pc);
        ui_fill_ellipse(px + 5, py - 4, 8, 5, pc);
    }
    for (i = 0; i < 14; i++) {
        uint32_t r = h2((uint32_t)i, 41u);
        int sx = (int)(r % (uint32_t)sw), sy = (int)((r >> 9) % (uint32_t)(sh * 2 / 3));
        if (creep < 10) ui_sparkle(sx, sy, (int)(t / 3 + r) & 15, tier ? PAL_PAINT(PAINT_LEMON, 2) : PAL_WHITE);
        else if ((i & 1) == 0) {
            int open = ((t / 2 + r) % 70) > 5;
            ui_fill_ellipse(sx, sy, 4, open ? 2 : 0, PAL_WHITE);
            if (open) plat_rect(sx - 1 + (int)((t / 20) % 3) - 1, sy - 1, 2, 2, nightmare ? PAL_RAMP(RAMP_BLOOD, 5) : PAL_BLACK);
        }
    }
}

/* dream_draw_frame before S2-4d (d5305653), verbatim */
static void old_frame(int creep, int nightmare, uint32_t t)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), x, r = sh >= 200 ? 12 : 8, step = r * 12 / 5, band = r / 2, pass;
    uint8_t c = creep < 8 ? PAL_PAINT(PAINT_WHITE, 2) : creep < 13 ? PAL_RAMP(RAMP_VAPE, 6) :
                nightmare ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_VAPE, 2);
    uint8_t ol = creep < 8 ? PAL_RAMP(RAMP_VAPE, 5) : creep < 13 ? PAL_RAMP(RAMP_VAPE, 3) :
                 nightmare ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_RAMP(RAMP_VAPE, 0);
    for (pass = 0; pass < 2; pass++) {
        uint8_t col = pass ? c : ol;
        int g = pass ? 0 : 1;
        plat_rect(0, 0, sw, band + g, col);
        plat_rect(0, sh - band - g, sw, band + g, col);
        plat_rect(0, 0, band + g, sh, col);
        plat_rect(sw - band - g, 0, band + g, sh, col);
        for (x = step / 2 - r; x < sw + r; x += step) {
            int w1 = ui_wave((int)t + x, 50, 2);
            ui_fill_circle(x, band - 2 + w1, r / 2 + 2 + g, col);
            ui_fill_circle(x + step / 2, sh - band + 1 - w1, r / 2 + 2 + g, col);
        }
        for (x = step; x < sh - step / 2; x += step) {
            int w1 = ui_wave((int)t + x * 2, 60, 2);
            ui_fill_circle(band - 2 + w1, x, r / 2 + 2 + g, col);
            ui_fill_circle(sw - band + 1 - w1, x + step / 2, r / 2 + 2 + g, col);
        }
    }
    if (creep >= 13)
        for (x = 0; x < 6; x++) {
            plat_line(4, sh / 3 + x * 8, 10 + x * 2, sh / 3 + x * 8 + 3, PAL_RAMP(RAMP_STONE, 0));
            plat_line(sw - 5, sh / 3 + x * 8, sw - 11 - x * 2, sh / 3 + x * 8 + 3, PAL_RAMP(RAMP_STONE, 0));
        }
}

/* the thought bubble's big circles before S2-4d (d5305653): what dream_draw_bubble_in drew over the bedroom */
static void old_bubble(int t, int total, uint32_t clock)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), big = sh >= 200, k;
    int hx = sw * 58 / 100 - (big ? 75 : 56) + (big ? 30 : 24), hy = sh * 70 / 100 - (big ? 30 : 22) - 30;
    for (k = 0; k < 3; k++)
        if (t > k * 6) ui_fill_circle(hx + 8 + k * 9, hy - k * 10, 2 + k * 2, PAL_PAINT(PAINT_WHITE, 2));
    if (t > 18) {
        int p = (t - 18) * 256 / (total - 18), r = 8 + (sw + sh) * p / 256 * p / 256;
        int cx = hx + 40 + (sw / 2 - hx - 40) * p / 256, cy = hy - 34 + (sh / 2 - hy + 34) * p / 256;
        ui_fill_circle(cx, cy, r + 2, PAL_RAMP(RAMP_VAPE, 4));
        ui_fill_circle(cx, cy, r, PAL_RAMP(RAMP_PASTEL_PINK, 7));
        if (r > 16) {
            ui_fill_ellipse(cx - r / 3, cy + r / 4, r / 3, r / 8 + 1, PAL_PAINT(PAINT_WHITE, 2));
            ui_fill_ellipse(cx + r / 3, cy - r / 4, r / 4, r / 10 + 1, PAL_PAINT(PAINT_WHITE, 2));
            ui_fill_circle(cx - r / 3, cy - r / 3, r / 10 + 1, PAL_RAMP(RAMP_VAPE, 6));
        }
        for (k = 0; k < 6; k++)
            ui_sparkle(cx + (int)((fx_cos(k * 682 + t * 40) * r) >> 17), cy + (int)((fx_sin(k * 682 + t * 40) * r) >> 17),
                       (int)(clock / 2 + (uint32_t)k * 3) & 15, PAL_WHITE);
    }
}

/* the caption's cloud box before S2-4d (d5305653, dream.c caption_box), verbatim */
static void old_cloud(int x, int y, int w, int h, uint8_t ol, uint8_t bg)
{
    int k;
    for (k = 0; k < w; k += 14) ui_fill_circle(x + 7 + k, y + 1, 7, ol);
    ui_round_rect(x - 1, y - 1, w + 2, h + 2, 8, ol);
    for (k = 0; k < w; k += 14) ui_fill_circle(x + 7 + k, y + 2, 6, bg);
    ui_round_rect(x, y, w, h, 7, bg);
}

static uint8_t ref_fb[NAT_MAX_W * NAT_MAX_H];

static void res(int w, int h)
{
    nat_w = w;
    nat_h = h;
    portrait_cache_reset();
}

static void keep(void) { memcpy(ref_fb, nat_fb, (size_t)(nat_w * nat_h)); }

/* the first differing pixel's index, -1 when the frames are the same (rows [0, rows) only) */
static int differs(int rows)
{
    int i;
    for (i = 0; i < nat_w * rows; i++) if (nat_fb[i] != ref_fb[i]) return i;
    return -1;
}

static void island(void)
{
    town_new(TOWN_FREEPLAY, 777, TOWN_POP_PREMADE);
    sim_init();
}

TEST(dream_sky_is_the_old_sky)
{
    int r, creep, nm, fast;
    uint32_t t;
    island();
    for (r = 0; r < 2; r++)
        for (fast = 0; fast < 2; fast++) {
            res(r ? 320 : 240, r ? 240 : 160);
            nat_fast2d = fast;
            for (nm = 0; nm < 2; nm++)
                for (creep = 0; creep <= 16; creep += 4)
                    for (t = 0; t < 700; t += 23) {
                        int d;
                        memset(nat_fb, 0, sizeof(nat_fb));
                        old_sky(creep, nm, t);
                        keep();
                        memset(nat_fb, 0, sizeof(nat_fb));
                        dream_draw_sky(creep, nm, t);
                        d = differs(nat_h);
                        CHECK_MSG(d < 0, "%dx%d fast %d creep %d nm %d t %u: pixel (%d, %d)", nat_w, nat_h, fast,
                                  creep, nm, (unsigned)t, d % nat_w, d / nat_w);
                    }
        }
    nat_fast2d = 0;
    res(240, 160);
}

/* rows [0, y1) of the sky are the full sky's, for any y1 (the hook and the rects alike) */
TEST(dream_sky_rows_are_the_skys_top)
{
    int fast, y1;
    island();
    for (fast = 0; fast < 2; fast++) {
        nat_fast2d = fast;
        for (y1 = 0; y1 <= 160; y1 += 3) {
            uint32_t t = (uint32_t)(y1 * 7);
            int d;
            memset(nat_fb, 0, sizeof(nat_fb));
            dream_draw_sky(y1 & 16 ? 14 : 3, y1 & 1, t);
            keep();
            memset(nat_fb, 0, sizeof(nat_fb));
            dream_draw_sky_rows(y1 & 16 ? 14 : 3, y1 & 1, t, y1);
            d = differs(y1);
            CHECK_MSG(d < 0, "fast %d y1 %d: pixel (%d, %d)", fast, y1, d % nat_w, d / nat_w);
        }
    }
    nat_fast2d = 0;
}

/* the sky rows a vignette hides (its floor, the doors' wall) do not show: the frame is the same without them */
TEST(dream_vignettes_hide_the_skipped_sky)
{
    int r, v, lvl;
    island();
    for (r = 0; r < 2; r++) {
        res(r ? 320 : 240, r ? 240 : 160);
        for (lvl = 0; lvl < 3; lvl++)
            for (v = 0; v < DREAM_VIGNETTES; v++) {
                DreamCast c;
                int creep = lvl * 8, k, d;
                dream_cast(lvl == 2 ? 666 : 42, &c);
                for (k = 0; k < 3; k++) {                    /* let the portrait cache settle */
                    portrait_cache_clock((uint32_t)(100 + k), 0);
                    dream_draw_sky(creep, lvl == 2, 100);
                    dream_draw_vignette(v, &c, 100, creep, lvl == 2);
                }
                memset(nat_fb, 0, sizeof(nat_fb));
                dream_draw_sky(creep, lvl == 2, 100);
                dream_draw_vignette(v, &c, 100, creep, lvl == 2);
                dream_draw_frame(creep, lvl == 2, 100);
                keep();
                memset(nat_fb, 0, sizeof(nat_fb));
                dream_draw_sky_rows(creep, lvl == 2, 100, dream_sky_rows(v));
                dream_draw_vignette(v, &c, 100, creep, lvl == 2);
                dream_draw_frame(creep, lvl == 2, 100);
                d = differs(nat_h);
                CHECK_MSG(d < 0, "%dx%d vignette %d creep %d: pixel (%d, %d) (sky rows %d)", nat_w, nat_h, v, creep,
                          d % nat_w, d / nat_w, dream_sky_rows(v));
            }
    }
    res(240, 160);
}

/* every vignette, its sky and rim, with the one-call paths (the native platform drawing them as plat_ext.h says)
 * and without (the core's rects): the same frame */
TEST(dream_vignettes_with_the_one_call_paths)
{
    int r, v, lvl;
    uint32_t t;
    island();
    for (r = 0; r < 2; r++) {
        res(r ? 320 : 240, r ? 240 : 160);
        for (lvl = 0; lvl < 3; lvl++)
            for (v = 0; v < DREAM_VIGNETTES; v++)
                for (t = 37; t < 180; t += 71) {
                    DreamCast c;
                    int creep = lvl * 8, k, d;
                    dream_cast(lvl == 2 ? 666 : 42, &c);
                    for (k = 0; k < 2; k++) {
                        nat_fast2d = 0;
                        memset(nat_fb, 0, sizeof(nat_fb));
                        portrait_cache_clock(t, 0);
                        dream_draw_sky_rows(creep, lvl == 2, t, dream_sky_rows(v));
                        dream_draw_vignette(v, &c, (int)t, creep, lvl == 2);
                        dream_draw_frame(creep, lvl == 2, t);
                    }
                    keep();
                    nat_fast2d = 1;
                    memset(nat_fb, 0, sizeof(nat_fb));
                    dream_draw_sky_rows(creep, lvl == 2, t, dream_sky_rows(v));
                    dream_draw_vignette(v, &c, (int)t, creep, lvl == 2);
                    dream_draw_frame(creep, lvl == 2, t);
                    nat_fast2d = 0;
                    d = differs(nat_h);
                    CHECK_MSG(d < 0, "%dx%d vignette %d creep %d t %u: pixel (%d, %d)", nat_w, nat_h, v, creep,
                              (unsigned)t, d % nat_w, d / nat_w);
                }
    }
    res(240, 160);
}

TEST(dream_frame_is_the_old_rim)
{
    int r, creep, nm, fast;
    uint32_t t;
    for (r = 0; r < 2; r++)
        for (fast = 0; fast < 2; fast++) {
            res(r ? 320 : 240, r ? 240 : 160);
            nat_fast2d = fast;
            for (nm = 0; nm < 2; nm++)
                for (creep = 0; creep <= 16; creep += 4)
                    for (t = 0; t < 400; t += 13) {
                        int d;
                        memset(nat_fb, 0, sizeof(nat_fb));
                        old_frame(creep, nm, t);
                        keep();
                        memset(nat_fb, 0, sizeof(nat_fb));
                        dream_draw_frame(creep, nm, t);
                        d = differs(nat_h);
                        CHECK_MSG(d < 0, "%dx%d fast %d creep %d nm %d t %u: pixel (%d, %d)", nat_w, nat_h, fast,
                                  creep, nm, (unsigned)t, d % nat_w, d / nat_w);
                    }
        }
    nat_fast2d = 0;
    res(240, 160);
}

/* the helpers and the platform's one call: ui_fill_circle's / ui_fill_ellipse's pixels, on and off the screen */
TEST(dream_shapes_are_ui_fill_shapes)
{
    uint32_t seed = 12345;
    int i, fast;
    for (fast = 0; fast < 2; fast++) {
        nat_fast2d = fast;
        for (i = 0; i < 1500; i++) {
            int16_t xy[2 * 5], e[4 * 5];
            int n = 1 + i % 5, k, r, d;
            uint8_t c = (uint8_t)(1 + i % 200);
            seed = seed * 1103515245u + 12345u;
            r = (int)((seed >> 8) % (i % 3 ? 20u : 300u));
            for (k = 0; k < n; k++) {
                seed = seed * 1103515245u + 12345u;
                xy[2 * k] = (int16_t)((int)((seed >> 4) % 400u) - 80);
                xy[2 * k + 1] = (int16_t)((int)((seed >> 14) % 300u) - 70);
                e[4 * k] = xy[2 * k];
                e[4 * k + 1] = xy[2 * k + 1];
                e[4 * k + 2] = (int16_t)((seed >> 20) % 90u);
                e[4 * k + 3] = (int16_t)((seed >> 24) % (k & 1 ? 30u : 16u));
            }
            memset(nat_fb, 0, sizeof(nat_fb));
            for (k = 0; k < n; k++) ui_fill_circle(xy[2 * k], xy[2 * k + 1], r, c);
            keep();
            memset(nat_fb, 0, sizeof(nat_fb));
            dream_circles(xy, n, r, c);
            d = differs(nat_h);
            CHECK_MSG(d < 0, "fast %d case %d circles r %d: pixel (%d, %d)", fast, i, r, d % nat_w, d / nat_w);
            memset(nat_fb, 0, sizeof(nat_fb));
            for (k = 0; k < n; k++) ui_fill_ellipse(e[4 * k], e[4 * k + 1], e[4 * k + 2], e[4 * k + 3], c);
            keep();
            memset(nat_fb, 0, sizeof(nat_fb));
            dream_ellipses(e, n, c);
            d = differs(nat_h);
            CHECK_MSG(d < 0, "fast %d case %d ellipses: pixel (%d, %d)", fast, i, d % nat_w, d / nat_w);
        }
    }
    nat_fast2d = 0;
}

/* (LAND) a circle wider than 256 on a 320-wide screen: a half-width cut to 255 from a centre near one edge does not
 * reach the other (the review's probe): the one-call path must leave such circles to ui_fill_circle */
TEST(dream_circles_wider_than_256)
{
    static const int16_t cx[6] = { 10, 60, 160, 260, 310, -40 };
    int fast, i, r;
    res(320, 240);
    for (fast = 0; fast < 2; fast++) {
        nat_fast2d = fast;
        for (i = 0; i < 6; i++)
            for (r = 250; r <= 330; r += 20) {
                int16_t xy[2];
                int d;
                xy[0] = cx[i]; xy[1] = (int16_t)(i * 47 - 20);
                memset(nat_fb, 0, sizeof(nat_fb));
                ui_fill_circle(xy[0], xy[1], r, 7);
                keep();
                memset(nat_fb, 0, sizeof(nat_fb));
                dream_circles(xy, 1, r, 7);
                d = differs(nat_h);
                CHECK_MSG(d < 0, "fast %d centre (%d, %d) r %d: pixel (%d, %d)", fast, xy[0], xy[1], r, d % nat_w,
                          d / nat_w);
            }
    }
    nat_fast2d = 0;
    res(240, 160);
}

/* (LAND) the caption's cloud box: the old box's pixels for every width the captions use and more (the bumps near the
 * corners drawn whole, the others only above their rects), on and off the screen, with and without the one-call path;
 * and the rows it leaves out are real (the one-call path draws fewer rows than the old box) */
TEST(dream_caption_cloud_is_the_old_box)
{
    int r, fast, w, h, y, x;
    for (r = 0; r < 2; r++)
        for (fast = 0; fast < 2; fast++) {
            res(r ? 320 : 240, r ? 240 : 160);
            nat_fast2d = fast;
            for (w = 1; w <= nat_w + 40; w += (w < 60 ? 3 : 17))
                for (h = 1; h <= 60; h += (h < 20 ? 1 : 13))
                    for (y = -12; y < nat_h + 8; y += 37)
                        for (x = -20; x < nat_w; x += 61) {
                            int d;
                            memset(nat_fb, 0, sizeof(nat_fb));
                            old_cloud(x, y, w, h, 77, 141);
                            keep();
                            memset(nat_fb, 0, sizeof(nat_fb));
                            dream_caption_cloud(x, y, w, h, 77, 141);
                            d = differs(nat_h);
                            CHECK_MSG(d < 0, "%dx%d fast %d box (%d, %d) %d x %d: pixel (%d, %d)", nat_w, nat_h, fast,
                                      x, y, w, h, d % nat_w, d / nat_w);
                        }
        }
    nat_fast2d = 0;
    res(240, 160);
}

/* the swelling bubble: the same picture as before, with the bedroom and the ring left out once the disc fills the
 * screen (and that happens: the last ticks of the swell skip the bedroom) */
TEST(dream_bubble_hides_only_what_it_covers)
{
    int r, t, fast, covered = 0;
    DreamCast c;
    island();
    dream_cast(42, &c);
    for (r = 0; r < 2; r++)
        for (fast = 0; fast < 2; fast++) {
            res(r ? 320 : 240, r ? 240 : 160);
            nat_fast2d = fast;
            for (t = 0; t <= 46; t++) {
                int d, k;
                for (k = 0; k < 2; k++) dream_draw_bedroom(42, &c, EXPR_SLEEPY, 300, 0);   /* the cache settles */
                memset(nat_fb, 0, sizeof(nat_fb));
                dream_draw_bedroom(42, &c, EXPR_SLEEPY, 300, 0);
                old_bubble(t, 46, 300);
                keep();
                memset(nat_fb, 0, sizeof(nat_fb));
                if (!dream_bubble_covers(t, 46)) dream_draw_bedroom(42, &c, EXPR_SLEEPY, 300, 0);
                else covered++;
                dream_draw_bubble_in(42, t, 46, 300);
                d = differs(nat_h);
                CHECK_MSG(d < 0, "%dx%d fast %d t %d: pixel (%d, %d)", nat_w, nat_h, fast, t, d % nat_w, d / nat_w);
            }
        }
    CHECK(covered >= 8);
    nat_fast2d = 0;
    res(240, 160);
}

/* ---- the held wake-up choice --------------------------------------------------------------------------------- */
static uint32_t rows_hash(int y0, int y1)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = y0 * nat_w; i < y1 * nat_w; i++) h = (h ^ nat_fb[i]) * 16777619u;
    return h;
}

static uint32_t box_hash(int x0, int y0, int x1, int y1)
{
    uint32_t h = 2166136261u;
    int x, y;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) h = (h ^ nat_fb[y * nat_w + x]) * 16777619u;
    return h;
}

#define CH_N 200
typedef struct {
    uint32_t above[CH_N], band[CH_N], btn[CH_N];
    uint8_t held[CH_N], choice[CH_N];
    uint32_t ft[CH_N];
} ChRun;

/* Vex's forced nightmare, straight to its choice, then CH_N ticks with the cursor moved right at 60 and back at 120 */
static void choice_run(int hold, ChRun *o)
{
    int f, top = 80 / 2 - 24 + 37, y = 80 / 2 - 24;     /* 240 x 160: the band from the buttons' top */
    (void)y;
    doomo_init();
    test_reset();
    ui_modal_hold_enable(hold);
    island();
    scene_set(&SCENE_DREAM, -3666);
    for (f = 0; f < 40; f++) { plat_begin_frame(); doomo_frame(f == 20 ? BTN_A : 0); plat_end_frame(); }
    for (f = 0; f < 640; f++) { plat_begin_frame(); doomo_frame(0); plat_end_frame(); }   /* the three vignettes */
    top = 160 / 2 - 24 + 37;
    for (f = 0; f < CH_N; f++) {
        int before = ui_modal_hold_enable(-1);
        uint32_t pad = f == 60 ? BTN_RIGHT : f == 120 ? BTN_LEFT : 0;
        o->ft[f] = doomo_frame_count();
        plat_begin_frame();
        doomo_frame(pad);
        plat_end_frame();
        o->held[f] = (uint8_t)(ui_modal_hold_enable(-1) > before);
        {                                               /* the rows above the band, without the toast's lane */
            int by, bh;
            ui_toast_band(&by, &bh);
            o->above[f] = rows_hash(0, DOOMO_MIN(by, top)) ^ rows_hash(DOOMO_MIN(by + bh, top), top) * 31u;
        }
        o->band[f] = rows_hash(top, 160);
        /* the two buttons' bodies, inside their rounded corners and their one-row bob */
        o->btn[f] = box_hash(120 - 88 - 4 + 4, top + 3, 120 - 4 - 4, top + 12) ^
                    box_hash(120 + 4 + 4, top + 3, 120 + 4 + 88 - 4, top + 12) * 31u;
        o->choice[f] = (uint8_t)(f > 60 && f <= 120);
    }
    ui_modal_hold_enable(0);
}

TEST(dream_choice_holds_its_card)
{
    static ChRun a, b;
    int f, held = 0, same_back = 0;
    res(240, 160);
    choice_run(0, &b);
    choice_run(1, &a);
    for (f = 0; f < CH_N; f++) {
        CHECK_MSG(!b.held[f], "the hold-off run held tick %d", f);
        CHECK_MSG(a.btn[f] == b.btn[f], "tick %d: the buttons' bodies differ from the full frame", f);
        if (!a.held[f]) continue;
        held++;
        CHECK_MSG(f == 0 || a.above[f] == a.above[f - 1], "held tick %d changed the rows above the band", f);
    }
    CHECK_MSG(held >= CH_N - 10, "only %d of %d ticks held", held, CH_N);
    /* the band is the kept rows plus the buttons: the same cursor and bob phase give the same rows, before the
     * cursor went right and after it came back */
    for (f = 20; f < 60; f++) {
        int g;
        for (g = 130; g < CH_N; g++)
            if (a.held[f] && a.held[g] && (a.ft[g] - a.ft[f]) % 20u == 0) {
                CHECK_MSG(a.band[f] == a.band[g], "ticks %d and %d: the band differs (choice 0, same bob)", f, g);
                same_back++;
                break;
            }
    }
    CHECK(same_back > 10);
}
