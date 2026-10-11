/* test_ui_raster.c - the span rasteriser (ui_raster.c) must draw exactly
 * what the original per-pixel / per-row UI primitives drew (owner:
 * foundation). The reference implementations below are the pre-rework
 * code, kept here verbatim in spirit: coverage masks shaded with nine
 * lookups per pixel, one plat_rect per row, int64 maths. Every case is
 * drawn twice with the new code (shape cache miss, then hit) at several
 * positions, including partly off screen. */
#include <stdint.h>
#include "test.h"
#include "../src/ui_internal.h"

static uint8_t ref_fb[NAT_MAX_W * NAT_MAX_H];

static void bg(void)
{
    int i;
    for (i = 0; i < nat_w * nat_h; i++) nat_fb[i] = (uint8_t)(16 + (i * 7 + i / nat_w * 13) % 200);
}

static void keep_ref(void) { memcpy(ref_fb, nat_fb, sizeof(ref_fb)); }

static int fb_diff(void)
{
    int i, n = 0;
    for (i = 0; i < nat_w * nat_h; i++) n += nat_fb[i] != ref_fb[i];
    return n;
}

/* ---- reference implementations (the original UI code) --------------- */
static int ref_corner_inset(int r, int i)
{
    int d, k;
    if (r <= 0 || i >= r) return 0;
    if (r == 1) return 1;
    if (r == 2) return i == 0 ? 2 : 1;
    if (r == 3) { static const int t[] = { 3, 1, 1 }; return t[i]; }
    if (r == 4) { static const int t[] = { 4, 2, 1, 1 }; return t[i]; }
    d = r - i;
    k = (int)fx_isqrt((uint32_t)(r * r - (d - 1) * (d - 1)) * 4u) / 2;
    return r - k;
}

static void ref_hspan(int x0, int x1, int y, uint8_t c)
{
    if (x1 >= x0) plat_rect(x0, y, x1 - x0 + 1, 1, c);
}

static void ref_fill_ellipse(int cx, int cy, int rx, int ry, uint8_t c)
{
    int y;
    if (rx < 0 || ry < 0) return;
    if (ry == 0) { ref_hspan(cx - rx, cx + rx, cy, c); return; }
    for (y = -ry; y <= ry; y++) {
        uint32_t t = (uint32_t)((ry * ry - y * y) * 4 + ry * 2);
        int dx = (int)(fx_isqrt(t * (uint32_t)(rx * rx) / (uint32_t)(ry * ry)) / 2);
        ref_hspan(cx - dx, cx + dx, cy + y, c);
    }
}

static void ref_round_rect(int x, int y, int w, int h, int r, uint8_t c)
{
    int i;
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    for (i = 0; i < h; i++) {
        int in = 0;
        if (i < r) in = ref_corner_inset(r, i);
        else if (i >= h - r) in = ref_corner_inset(r, h - 1 - i);
        ref_hspan(x + in, x + w - 1 - in, y + i, c);
    }
}

static void ref_drop_shadow(int x, int y, int w, int h, int r)
{
    int i;
    for (i = 0; i < h; i++) {
        int in = 0;
        if (i < r) in = ref_corner_inset(r, i);
        else if (i >= h - r) in = ref_corner_inset(r, h - 1 - i);
        if (w - 2 * in > 0) plat_shade_rect(x + in, y + i, w - 2 * in, 1, 9);
    }
}

#define RMASK 72
static uint8_t rmask[(RMASK * RMASK + 7) / 8];
static int rmw, rmh;
static int rm_in(int x, int y)
{
    int i;
    if (x < 0 || y < 0 || x >= rmw || y >= rmh) return 0;
    i = y * rmw + x;
    return (rmask[i >> 3] >> (i & 7)) & 1;
}
static void rm_set(int x, int y, int on)
{
    int i = y * rmw + x;
    if (on) rmask[i >> 3] |= (uint8_t)(1 << (i & 7));
}
static void rm_begin(int w, int h)
{
    if (w > RMASK) w = RMASK;
    if (h > RMASK) h = RMASK;
    rmw = w; rmh = h;
    memset(rmask, 0, sizeof(rmask));
}
static void ref_shade_mask(int ox, int oy, const uint8_t pal[5], int d)
{
    int x, y;
    for (y = 0; y < rmh; y++) {
        int run_x = -1;
        uint8_t run_c = 0;
        for (x = 0; x <= rmw; x++) {
            uint8_t c = 0;
            if (x < rmw && rm_in(x, y)) {
                if (!rm_in(x - 1, y) || !rm_in(x + 1, y) || !rm_in(x, y - 1) || !rm_in(x, y + 1))
                    c = pal[0];
                else if (!rm_in(x + d, y + d) || !rm_in(x, y + d + 1))
                    c = pal[1];
                else if (!rm_in(x - d, y - d))
                    c = pal[3];
                else
                    c = pal[2];
            }
            if (c != run_c || x == rmw) {
                if (run_c) plat_rect(ox + run_x, oy + y, x - run_x, 1, run_c);
                run_c = c;
                run_x = x;
            }
        }
    }
}
static int ref_heart_in(int px, int py, int s)
{
    int lr = (s + 1) / 2, bottom = 2 * s - 1;
    int lx = px + lr * 2 - 1, rx = px - lr * 2 + 1, ly = py + lr;
    int ax = px < 0 ? -px : px;
    if (lx * lx + ly * ly <= lr * lr * 4 + 2) return 1;
    if (rx * rx + ly * ly <= lr * lr * 4 + 2) return 1;
    return py >= -lr && py <= bottom && ax * 10 <= (bottom - py) * 11;
}
static void ref_heart_squash(int cx, int cy, int size, int sx256, int sy256, uint8_t fill)
{
    uint8_t pal[5];
    int s = DOOMO_CLAMP(size, 2, 32);
    int hw = s * DOOMO_CLAMP(sx256, 128, 400) / 256, hh = s * DOOMO_CLAMP(sy256, 128, 400) / 256;
    int x, y, lr = (s + 1) / 2;
    ui_ramp5(fill, pal);
    if (s <= 2) {
        static const char *const tiny[4] = { ".#.#.", "#####", ".###.", "..#.." };
        for (y = 0; y < 4; y++)
            for (x = 0; x < 5; x++)
                if (tiny[y][x] == '#')
                    plat_rect(cx - 2 + x, cy - 2 + y, 1, 1,
                              (y == 0 || x == 0 || x == 4) ? pal[0] : pal[2]);
        return;
    }
    hw = DOOMO_CLAMP(hw, 2, (RMASK - 1) / 2);
    hh = DOOMO_CLAMP(hh, 2, (RMASK - 1) / 2);
    rm_begin(2 * hw + 1, 2 * hh + 1);
    for (y = 0; y < rmh; y++)
        for (x = 0; x < rmw; x++)
            rm_set(x, y, ref_heart_in((x - hw) * 2 * s / hw, (y - hh) * 2 * s / hh, s));
    ref_shade_mask(cx - hw, cy - hh, pal, s >= 8 ? 2 : 1);
    {
        int gx = cx - lr * hw / s, gy = cy - lr * hh / s;
        if (s >= 5) {
            plat_rect(gx - lr / 3, gy - lr / 4, DOOMO_MAX(1, s / 5), DOOMO_MAX(1, s / 4), pal[4]);
            plat_rect(gx + lr / 3 + 1, gy - lr / 2, 1, 1, pal[4]);
        } else {
            plat_rect(cx - lr, cy - lr / 2 - 1, 1, 1, pal[4]);
        }
    }
}
static void ref_star(int cx, int cy, int r, uint8_t fill)
{
    uint8_t pal[5];
    int32_t vx[10], vy[10];
    int i, x, y, s = DOOMO_CLAMP(r, 3, 34);
    int w = 2 * s + 1;
    rm_begin(w, w);
    for (i = 0; i < 10; i++) {
        int rad = (i & 1) ? s * 2 / 5 + 1 : s;
        int a = 3072 + i * 4096 / 10;
        vx[i] = (fx_cos(a) * rad) >> 8;
        vy[i] = (fx_sin(a) * rad) >> 8;
    }
    for (y = 0; y < rmh; y++)
        for (x = 0; x < rmw; x++) {
            int32_t px = (x - s) * 256, py = (y - s) * 256 + 64;
            int inside = 0, j;
            for (i = 0, j = 9; i < 10; j = i++)
                if (((vy[i] > py) != (vy[j] > py)) &&
                    (px < (int32_t)(((int64_t)(vx[j] - vx[i]) * (py - vy[i])) /
                                    (vy[j] - vy[i])) + vx[i]))
                    inside = !inside;
            rm_set(x, y, inside);
        }
    ui_ramp5(fill, pal);
    ref_shade_mask(cx - s, cy - s, pal, 1);
    if (s >= 5) plat_rect(cx - s / 4, cy - s / 4, 1, 1, PAL_WHITE);
}
static void ref_heart_flat(int cx, int cy, int s, uint8_t c)
{
    int y, lr, bottom;
    if (s < 2) s = 2;
    lr = (s + 1) / 2;
    bottom = 2 * s - 1;
    for (y = -s; y <= s; y++) {
        int py = 2 * y, ly = py + lr, k;
        int64_t r2 = (int64_t)lr * lr * 4 + 2 - (int64_t)ly * ly;
        if (r2 >= 0) {
            int dx = (int)fx_isqrt((uint32_t)DOOMO_MIN(r2, 0x7FFFFFFF));
            for (k = -1; k <= 1; k += 2) {
                int c0 = k * (2 * lr - 1);
                int x0 = (c0 - dx + 1) >> 1, x1 = (c0 + dx) >> 1;
                if (x1 >= x0) plat_rect(cx + x0, cy + y, x1 - x0 + 1, 1, c);
            }
        }
        if (py >= -lr && py <= bottom) {
            int half = DOOMO_MIN((bottom - py) * 11 / 10, 2 * s);
            int x0 = (-half + 1) >> 1, x1 = half >> 1;
            if (x1 >= x0) plat_rect(cx + x0, cy + y, x1 - x0 + 1, 1, c);
        }
    }
}
static void ref_big_glyphs(int x, int y, const char *s, int scale, uint8_t c, int top_only)
{
    int cx = x;
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        int col, row;
        if (*s == '\n') { cx = x; y += FONT_LINE * scale; continue; }
        if (ch < FONT_FIRST || ch > FONT_LAST) ch = '?';
        for (col = 0; col < 5; col++) {
            uint8_t bits = doomo_font[ch - FONT_FIRST][col];
            for (row = 0; row < 8; row++) {
                if (!(bits & (1 << row))) continue;
                if (top_only) {
                    if (row < 3) plat_rect(cx + col * scale, y + row * scale, scale,
                                           DOOMO_MAX(scale / 2, 1), c);
                } else {
                    plat_rect(cx + col * scale, y + row * scale, scale, scale, c);
                }
            }
        }
        cx += FONT_ADVANCE * scale;
    }
}
static void ref_text_big(int x, int y, const char *s, int scale, uint8_t color, uint8_t outline)
{
    int dx, dy;
    scale = DOOMO_CLAMP(scale, 1, 4);
    ref_big_glyphs(x + 1, y + 3, s, scale, ui_darker(outline), 0);
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (dx || dy) ref_big_glyphs(x + dx, y + dy, s, scale, outline, 0);
    ref_big_glyphs(x, y + 1, s, scale, outline, 0);
    ref_big_glyphs(x, y, s, scale, color, 0);
    ref_big_glyphs(x, y, s, scale, ui_lighter(color), 1);
}
static void ref_window(int x, int y, int w, int h, int style)
{
    const UiStyle *st = ui_style(style);
    int r = (w >= 24 && h >= 20) ? 4 : 2;
    int ir = r - 1, i;
    int band = DOOMO_CLAMP(h / 6, 3, 8);
    int lo = DOOMO_CLAMP(h / 8, 2, 5);
    if (w < 6 || h < 6) { plat_rect(x, y, w, h, st->fill); return; }
    ref_drop_shadow(x + 2, y + 3, w, h, r);
    ref_round_rect(x, y, w, h, r, st->outline);
    ref_round_rect(x + 1, y + 1, w - 2, h - 2, ir, st->fill_lo);
    ref_round_rect(x + 1, y + 1, w - 2, h - 2 - lo, ir, st->fill);
    for (i = 0; i < band && i < h - 2; i++) {
        int in = i < ir ? ref_corner_inset(ir, i) : 0;
        plat_rect(x + 1 + in, y + 1 + i, w - 2 - 2 * in, 1, st->fill_hi);
    }
    for (i = x + 2; i < x + w - 2; i += 2) {
        plat_rect(i, y + 1 + band, 1, 1, st->fill_hi);
        plat_rect(i + 1, y + h - 2 - lo, 1, 1, st->fill_lo);
    }
    plat_rect(x + r + 2, y + 2, DOOMO_MAX(w / 4, 4), 1, st->gloss);
    plat_rect(x + r + 4 + DOOMO_MAX(w / 4, 4), y + 2, 2, 1, st->gloss);
    plat_rect(x + w - 2, y + r + 1, 1, DOOMO_MAX(h - 2 * r - 2, 0), st->fill_lo);
    if (opt_get(OPT_UI_BOUNCE) && w > 24) {
        int span = w - 2 * r - 8, pos = ((int)doomo_frame_count() * 4 + x * 13 + y * 7) % (span + 240);
        if (pos < span) {
            plat_rect(x + r + 2 + pos, y + 2, 3, 1, PAL_WHITE);
            plat_rect(x + r + 1 + pos, y + 3, 2, 1, PAL_WHITE);
            plat_rect(x + r + pos, y + 4, 1, 1, st->gloss);
        }
    }
}
static void ref_pill(int x, int y, int w, int h, uint8_t fill, uint8_t outline, uint8_t hi)
{
    int r = DOOMO_MIN(h / 2, 4);
    if (w <= 2 || h <= 2) { plat_rect(x, y, w, h, fill); return; }
    ref_round_rect(x, y, w, h, r, outline);
    ref_round_rect(x + 1, y + 1, w - 2, h - 2, DOOMO_MAX(r - 1, 0), fill);
    if (h >= 6) {
        int in = DOOMO_MAX(r - 1, 1) + 1;
        plat_rect(x + in, y + 1, w - 2 * in, 1, hi);
    }
}

/* the case being checked, reported on failure */
static int case_a, case_b, case_c;

/* draw `ref` and `cur` (twice: cache miss then hit) on the same
 * background; returns the differing pixel count */
#define SAME(refcall, curcall) do { \
        int d_; bg(); refcall; keep_ref(); \
        bg(); curcall; d_ = fb_diff(); \
        CHECK_MSG(d_ == 0, "%.60s: %d px differ (case %d %d %d)", #curcall, d_, \
                  case_a, case_b, case_c); \
        bg(); curcall; d_ = fb_diff(); \
        CHECK_MSG(d_ == 0, "%.60s (cached): %d px differ", #curcall, d_); \
    } while (0)

static const int px_[] = { 60, 3, 236, 120 }, py_[] = { 50, 2, 157, 80 };

TEST(ui_raster_ellipses_and_round_rects)
{
    int r, rx, k;
    test_reset();
    for (r = 0; r <= 20; r++)
        for (k = 0; k < 4; k++) {
            SAME(ref_fill_ellipse(px_[k], py_[k], r, r, 77), ui_fill_circle(px_[k], py_[k], r, 77));
            for (rx = 0; rx <= 24; rx += 3)
                SAME(ref_fill_ellipse(px_[k], py_[k], rx, r, 78),
                     ui_fill_ellipse(px_[k], py_[k], rx, r, 78));
        }
    for (r = -1; r <= 9; r++) {
        int w, h;
        for (w = 0; w <= 40; w += 3)
            for (h = 0; h <= 30; h += 4) {
                SAME(ref_round_rect(10, 20, w, h, r, 99), ui_round_rect(10, 20, w, h, r, 99));
                SAME(ref_round_rect(-5, 150, w, h, r, 99), ui_round_rect(-5, 150, w, h, r, 99));
                SAME(ref_drop_shadow(10, 20, w, h, r), ui_drop_shadow(10, 20, w, h, r));
                SAME(ref_pill(30, 40, w, h / 2, 50, 51, 52), ui_pill(30, 40, w, h / 2, 50, 51, 52));
            }
    }
}

TEST(ui_raster_hearts_and_stars)
{
    int s, sx, k;
    test_reset();
    for (s = 1; s <= 34; s++)
        for (k = 0; k < 4; k++) {
            uint8_t fill = (s & 1) ? PAL_RAMP(RAMP_PASTEL_PINK, 4) : PAL_PAINT(PAINT_MINT, 1);
            SAME(ref_heart_squash(px_[k], py_[k], s, 256, 256, fill), ui_heart(px_[k], py_[k], s, fill));
            SAME(ref_star(px_[k], py_[k], s, PAL_UI_GOLD), ui_star(px_[k], py_[k], s, PAL_UI_GOLD));
            for (sx = 120; sx <= 420; sx += 60)
                SAME(ref_heart_squash(px_[k], py_[k], s, sx, 512 - sx, fill),
                     ui_heart_squash(px_[k], py_[k], s, sx, 512 - sx, fill));
        }
    for (s = 0; s < 700; s += 13)
        for (k = 0; k < 4; k++)
            SAME(ref_heart_flat(px_[k], py_[k], s, 33), ui_heart_flat(px_[k], py_[k], s, 33));
}

TEST(ui_raster_big_text)
{
    static const char *const strs[] = { "DOOMOGACHI", "H", "a", "HaHaland", "Hi!\nyo",
                                        "gjpqy|~", "\x80\x81 \x9f", "WWWWWWWWWWWWWWWWWWWWWWWW" };
    int i, sc, k;
    test_reset();
    for (i = 0; i < DOOMO_COUNTOF(strs); i++)
        for (sc = 1; sc <= 4; sc++)
            for (k = 0; k < 4; k++)
            {
                case_a = i; case_b = sc; case_c = k;
                SAME(ref_text_big(px_[k] - 30, py_[k] - 10, strs[i], sc, PAL_PAINT(PAINT_BUBBLEGUM, 1),
                                  PAL_RAMP(RAMP_PASTEL_PINK, 1)),
                     ui_text_big(px_[k] - 30, py_[k] - 10, strs[i], sc, PAL_PAINT(PAINT_BUBBLEGUM, 1),
                                 PAL_RAMP(RAMP_PASTEL_PINK, 1)));
            }
}

TEST(ui_raster_windows)
{
    int w, h, st, k;
    test_reset();
    for (st = 0; st < 6; st++)
        for (w = 4; w <= 120; w += 13)
            for (h = 4; h <= 90; h += 11)
                for (k = 0; k < 2; k++)
                    SAME(ref_window(px_[k] - 20, py_[k] - 20, w, h, st),
                         ui_window(px_[k] - 20, py_[k] - 20, w, h, st));
}

TEST(ui_raster_checker_and_wave)
{
    int x, y, w, h, t, p, a;
    test_reset();
    for (w = 0; w <= 9; w++)
        for (h = 0; h <= 3; h++)
            for (x = -3; x <= 2; x++)
                for (y = -2; y <= 1; y++) {
                    int i, j;
                    bg();
                    for (j = 0; j < h; j++)
                        for (i = j & 1; i < w; i += 2) plat_rect(x + 5 + i, y + 5 + j, 1, 1, 9);
                    keep_ref();
                    bg();
                    plat_checker(x + 5, y + 5, w, h, 9);
                    CHECK_EQ(fb_diff(), 0);
                }
    for (p = 1; p <= 200; p += 7)
        for (a = -40; a <= 40; a += 9)
            for (t = -500; t <= 5000; t += 37) {
                int ref = (int)(((int64_t)fx_sin((int)((int64_t)t * DOOMO_ANG_FULL / p)) * a) >> 16);
                CHECK_EQ(ui_wave(t, p, a), ref);
            }
}
