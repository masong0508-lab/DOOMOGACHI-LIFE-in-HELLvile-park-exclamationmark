/* test_dc2d.c - unit tests of the Dreamcast 2D layer on the host (DC3;
 * owner: dreamcast). Built with tee.c, so plat_* draws both the native
 * reference (8bpp) and the Dreamcast prim list; swpvr.c renders the list.
 *
 *   twiddle        the texture layout matches KallistiOS' pvr_txr_load_ex
 *   uv16           sprite UV truncation and the sprite/polygon choice
 *   merge          rect runs grow one prim; clear / full-screen rect reset
 *   fuzz           random rects, lines, checkers, text, outlined text,
 *                  images (scale, flip, silhouette), iris: the software PVR
 *                  frame equals the native frame on EVERY pixel
 *   shade          plat_shade_rect equals the colormap remap on all 256
 *                  colours at every level (the exact path); IMG_SHADOW is
 *                  exact for simple shapes; its DARK fallback and the blend
 *                  stay close
 *   texcache       uid hits, in-place changes of uid-less images, eviction
 *                  under video-RAM pressure never loses a frame's texture
 *   hold           plat_hold_frame keeps the picture under new frames
 *   budget         a frame over the vertex budget drops prims, counted
 *   post           offset / scale move only the earlier prims; pal_lerp
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"
#include "dc2d.h"
#include "swpvr.h"
#include "doomo/camfx.h"
#include "doomo/plat_ext.h"

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
                           printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint8_t rgb[DC2D_W * DC2D_H * 3];

static int x5to8(int v) { return (v << 3) | (v >> 2); }
static void nat565(uint8_t idx, int *r, int *g, int *b)
{
    uint16_t c = doomo_palette[idx];
    int g5 = (c >> 5) & 31, g6 = (g5 << 1) | (g5 >> 4);
    *r = x5to8(c & 31); *g = (g6 << 2) | (g6 >> 4); *b = x5to8((c >> 10) & 31);
}

/* pixels (logical) whose 2x2 block differs from the native frame; max channel error */
static int frame_diff(int *maxerr)
{
    int x, y, bad = 0, me = 0;
    swpvr_render(dc2d_last(), rgb);
    for (y = 0; y < DC2D_LH; y++)
        for (x = 0; x < DC2D_LW; x++) {
            int r, g, b, k, ex = 1;
            nat565(nat_fb[y * nat_w + x], &r, &g, &b);
            for (k = 0; k < 4; k++) {
                const uint8_t *p = &rgb[((2 * y + (k >> 1)) * DC2D_W + 2 * x + (k & 1)) * 3];
                int e = abs(p[0] - r);
                if (abs(p[1] - g) > e) e = abs(p[1] - g);
                if (abs(p[2] - b) > e) e = abs(p[2] - b);
                if (e) ex = 0;
                if (e > me) me = e;
            }
            bad += !ex;
        }
    if (maxerr) *maxerr = me;
    return bad;
}

static void frame_begin(void) { plat_begin_frame(); plat_clear(PAL_BLACK); }
static void frame_end(void) { plat_end_frame(); }

/* ---- twiddle --------------------------------------------------------------------------- */
#define TWIDTAB(x) ((x & 1) | ((x & 2) << 1) | ((x & 4) << 2) | ((x & 8) << 3) | ((x & 16) << 4) | \
                    ((x & 32) << 5) | ((x & 64) << 6) | ((x & 128) << 7) | ((x & 256) << 8) | ((x & 512) << 9))
#define TWIDOUT(x, y) (TWIDTAB((y)) | (TWIDTAB((x)) << 1))

static void test_twiddle(void)
{
    static const int sz[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };
    int a, b, bad = 0;
    for (a = 0; a < 8; a++)
        for (b = 0; b < 8; b++) {
            int w = sz[a], h = sz[b];
            uint32_t x, y, mn = (uint32_t)(w < h ? w : h), mask = mn - 1;
            if ((long)w * h > 512L * 1024) continue;
            for (y = 0; y < (uint32_t)h; y++)
                for (x = 0; x < (uint32_t)w; x++) {
                    /* KallistiOS pvr_txr_load_ex, 8bpp: one u16 holds rows y, y + 1 */
                    uint32_t u16 = TWIDOUT(((y & ~1u) & mask) / 2, x & mask) + (x / mn + y / mn) * mn * mn / 2;
                    uint32_t want = u16 * 2 + (y & 1);
                    if (dc2d_twiddle_index(x, y, w, h) != want) bad++;
                }
        }
    CHECK(bad == 0, "twiddle: %d texels differ from pvr_txr_load_ex", bad);
    {   /* round trip through dc2d_twiddle8 */
        static uint8_t src[24 * 13];
        static uint16_t dst[32 * 16 / 2];
        int i, x, y, err = 0;
        for (i = 0; i < 24 * 13; i++) src[i] = (uint8_t)(i * 7 + 1);
        dc2d_twiddle8(dst, src, 24, 13, 32, 16);
        for (y = 0; y < 16; y++)
            for (x = 0; x < 32; x++) {
                uint8_t v = ((uint8_t *)dst)[dc2d_twiddle_index((uint32_t)x, (uint32_t)y, 32, 16)];
                uint8_t want = (x < 24 && y < 13) ? src[y * 24 + x] : 0;
                err += v != want;
            }
        CHECK(err == 0, "twiddle8 round trip: %d texels wrong", err);
    }
}

static void test_uv16(void)
{
    DcPrim p;
    memset(&p, 0, sizeof(p));
    CHECK(dc2d_uv16_exact(0.5f) && dc2d_uv16_exact(37.0f / 128.0f) && dc2d_uv16_exact(300.0f / 512.0f),
          "uv16: exact values");
    CHECK(!dc2d_uv16_exact(1.0f / 3.0f) && !dc2d_uv16_exact(319.0f / 512.0f), "uv16: inexact values");
    p.tex = 0; p.u1 = 319.0f / 512.0f; p.v1 = 0.5f;
    CHECK(!dc2d_is_sprite(&p), "a 319/512 UV goes as a polygon");
    p.tex = DC2D_NOTEX;
    CHECK(dc2d_is_sprite(&p), "untextured prims are sprites");
}

/* ---- merging and clearing ------------------------------------------------------------------- */
static void test_merge(void)
{
    const DcFrame *f;
    frame_begin();
    plat_clear(PAL_BLACK);
    plat_rect(0, 0, 10, 1, 6);
    plat_rect(10, 0, 5, 1, 6);          /* grows to the right */
    plat_rect(0, 1, 15, 1, 6);          /* grows down */
    plat_rect(0, 2, 15, 1, 7);          /* another colour: a new prim */
    frame_end();
    f = dc2d_last();
    CHECK(f->n == 2, "merge: %d prims, want 2", f->n);
    CHECK(f->n >= 1 && f->prims[0].x1 == 30.0f && f->prims[0].y1 == 4.0f, "merge: the first rect is 15x2");
    CHECK(frame_diff(NULL) == 0, "merge: frame differs");
    frame_begin();
    plat_rect(5, 5, 5, 5, 3);
    plat_rect(-10, -10, 400, 300, 9);   /* covers the screen: earlier prims go */
    plat_rect(5, 5, 5, 5, 3);
    frame_end();
    f = dc2d_last();
    CHECK(f->n == 2, "full-screen rect: %d prims, want 2", f->n);
    CHECK(frame_diff(NULL) == 0, "full-screen rect: frame differs");
}

/* ---- the exact fuzz ------------------------------------------------------------------------- */
static uint32_t rs = 12345;
static int rnd(int n) { rs = rs * 1103515245u + 12345u; return (int)((rs >> 8) % (uint32_t)n); }

static uint8_t imgpix[8][40 * 40];
static DoomoImage imgs[8];

static void make_images(void)
{
    int i, k;
    for (i = 0; i < 8; i++) {
        int w = 3 + rnd(37), h = 3 + rnd(37);
        for (k = 0; k < w * h; k++) imgpix[i][k] = (uint8_t)(rnd(3) ? 0 : 1 + rnd(255));
        imgs[i].w = (uint16_t)w; imgs[i].h = (uint16_t)h; imgs[i].pix = imgpix[i];
        imgs[i].uid = (i & 1) ? 0x01000100u + (uint32_t)i : 0;
    }
}

static void random_frame(int calls, int with_iris)
{
    int i;
    static const char *const words[] = { "Hello!", "BOREagotchi", "a\nb c", "\x80\x81 ok", "Espa\xA5ol \xAE?",
                                         "~{}|", "\x01\x1F\xF0\xFF" };
    frame_begin();
    plat_clear((uint8_t)rnd(256));
    for (i = 0; i < calls; i++) {
        int k = rnd(9), x = rnd(360) - 20, y = rnd(280) - 20, c = rnd(256);
        switch (k) {
        case 0: case 1: plat_rect(x, y, rnd(80), rnd(40), (uint8_t)c); break;
        case 2: plat_line(x, y, rnd(360) - 20, rnd(280) - 20, (uint8_t)c); break;
        case 3: plat_checker(x, y, rnd(90), rnd(30), (uint8_t)c); break;
        case 4: plat_text(x, y, words[rnd(7)], (uint8_t)c); break;
        case 5: plat_text_outline(x, y, words[rnd(7)], (uint8_t)c, (uint8_t)rnd(256)); break;
        case 6: case 7: {
            int fl = rnd(2) ? IMG_FLIPX : 0;
            if (!rnd(3)) fl |= IMG_SILHOUETTE;
            plat_image(x, y, &imgs[rnd(8)], 1 + rnd(4), fl, (uint8_t)c);
            break;
        }
        default:                         /* a short horizontal span run, as the UI draws */
            plat_rect(x, y, 3, 1, (uint8_t)c); plat_rect(x + 3, y, 4, 1, (uint8_t)c);
            plat_rect(x, y + 1, 7, 1, (uint8_t)c);
            break;
        }
    }
    if (with_iris) plat_post_iris(rnd(320), rnd(240), rnd(200), rnd(2), (uint8_t)rnd(256));
    frame_end();
}

static void test_fuzz(void)
{
    int f, bad_frames = 0, worst = 0;
    make_images();
    for (f = 0; f < 120; f++) {
        int bad;
        random_frame(20 + rnd(200), (f % 5) == 4);
        bad = frame_diff(NULL);
        if (bad) {
            bad_frames++;
            if (bad > worst) worst = bad;
        }
    }
    CHECK(bad_frames == 0, "fuzz: %d of 120 frames differ from native (worst %d pixels)", bad_frames, worst);
}

/* ---- shading: exact remaps, close approximations ------------------------------------------------------------ */
static void test_shade(void)
{
    int lvl, me, bad;
    for (lvl = 0; lvl < 16; lvl++) {
        int i, x, y, far = 0;
        long sum = 0;
        frame_begin();
        for (i = 0; i < 256; i++) plat_rect((i % 16) * 20, (i / 16) * 15, 20, 15, (uint8_t)i);
        plat_shade_rect(0, 0, 320, 240, lvl);
        frame_end();
        bad = frame_diff(&me);
        /* per palette colour: the channel error against the colormap's remap */
        for (y = 0; y < 16; y++)
            for (x = 0; x < 16; x++) {
                int r, g, b, e;
                const uint8_t *p = &rgb[((y * 30 + 7) * DC2D_W + x * 40 + 10) * 3];
                nat565(nat_fb[(y * 15 + 7) * nat_w + x * 20 + 10], &r, &g, &b);
                e = abs(p[0] - r);
                if (abs(p[1] - g) > e) e = abs(p[1] - g);
                if (abs(p[2] - b) > e) e = abs(p[2] - b);
                sum += e;
                far += e > 32;
            }
        CHECK(bad == 0, "shade level %d: %d pixels differ (mean %ld, %d colours far off)", lvl, bad, sum / 256, far);
    }
    {
        uint8_t px[16 * 16];
        DoomoImage im = { 16, 16, px, 0x01000777u };
        memset(px, 0, sizeof(px));
        memset(px + 16 * 4, 2, 16 * 8);
        frame_begin();
        plat_rect(0, 0, 320, 240, 3);
        plat_image(40, 40, &im, 2, IMG_SHADOW, 0);
        frame_end();
        bad = frame_diff(&me);
        CHECK(bad == 0, "image shadow (exact, as rects): %d pixels differ, max %d", bad, me);
        /* a noisy shape (more than 48 rects): the DARK copy, close to the remap */
        make_images();
        frame_begin();
        plat_rect(0, 0, 320, 240, 3);
        plat_image(40, 40, &imgs[2], 2, IMG_SHADOW, 0);
        frame_end();
        bad = frame_diff(&me);
        CHECK(me <= 48, "image shadow (DARK fallback): %d pixels differ, max error %d", bad, me);
    }
}

/* exact shades over everything the UI draws: partial, nested, over images
 * (remapped textures), over a held backdrop; the fallback when too big */
static void test_shade_exact(void)
{
    int f, bad_frames = 0;
    uint32_t fb0;
    make_images();
    for (f = 0; f < 60; f++) {
        int k, n = 1 + rnd(6);
        frame_begin();
        plat_clear((uint8_t)rnd(256));
        for (k = 0; k < 40 + rnd(80); k++) {
            int x = rnd(340) - 10, y = rnd(260) - 10;
            switch (rnd(5)) {
            case 0: plat_rect(x, y, rnd(60), rnd(40), (uint8_t)rnd(256)); break;
            case 1: plat_text_outline(x, y, "Shade me", (uint8_t)rnd(256), (uint8_t)rnd(256)); break;
            case 2: plat_checker(x, y, rnd(50), rnd(9), (uint8_t)rnd(256)); break;
            default: plat_image(x, y, &imgs[rnd(8)], 1 + rnd(4), rnd(2) ? IMG_FLIPX : 0, 0); break;
            }
        }
        for (k = 0; k < n; k++)          /* overlapping, nested shades */
            plat_shade_rect(rnd(330) - 5, rnd(250) - 5, rnd(200), rnd(150), rnd(15));
        plat_rect(rnd(300), rnd(220), 20, 20, (uint8_t)rnd(256));   /* drawn after: not shaded */
        if (f % 10 == 9) plat_shade_rect(0, 0, 320, 240, rnd(15));  /* a full-screen dim */
        frame_end();
        if (frame_diff(NULL)) bad_frames++;
    }
    CHECK(bad_frames == 0, "exact shade: %d of 60 frames differ from native", bad_frames);
    /* a shade over a held backdrop */
    frame_begin();
    plat_image(10, 10, &imgs[0], 3, 0, 0);
    plat_rect(100, 100, 50, 50, 40);
    frame_end();
    plat_hold_frame(1);
    plat_begin_frame();
    plat_shade_rect(0, 0, 200, 200, 6);
    plat_rect(150, 150, 30, 30, 8);
    frame_end();
    CHECK(frame_diff(NULL) == 0, "exact shade over a held backdrop differs");
    plat_hold_frame(0);
    /* too many prims under a full-screen shade: the multiply */
    fb0 = dc2d_stats.shade_fallbacks;
    frame_begin();
    for (f = 0; f < 7200; f++) plat_rect((f * 3) % 318, (f * 3 / 318) % 240, 1, 1, (uint8_t)(1 + f % 250));
    plat_shade_rect(0, 0, 320, 240, 8);
    frame_end();
    CHECK(dc2d_stats.shade_fallbacks == fb0 + 1, "a shade over 7200 prims did not fall back to the multiply");
}

/* ---- texture cache -------------------------------------------------------------------------------- */
static void test_texcache(void)
{
    static uint8_t px[32 * 32];
    static uint8_t big[16][256 * 128];
    DoomoImage a = { 32, 32, px, 0x01000AAAu }, b = { 32, 32, px, 0 };
    DoomoImage bigimg[16];
    uint32_t up;
    int i, f;
    memset(px, 5, sizeof(px));
    frame_begin(); plat_image(0, 0, &a, 1, 0, 0); frame_end();
    up = dc2d_stats.uploads;
    frame_begin(); plat_image(0, 0, &a, 1, 0, 0); frame_end();
    CHECK(dc2d_stats.uploads == up, "uid image: uploaded again");
    frame_begin(); plat_image(0, 0, &b, 1, 0, 0); frame_end();
    up = dc2d_stats.uploads;
    px[100] = 9;                        /* changed in place, no uid */
    frame_begin(); plat_image(0, 0, &b, 1, 0, 0); frame_end();
    CHECK(dc2d_stats.uploads == up + 1, "uid-less image changed in place: not uploaded again");
    CHECK(frame_diff(NULL) == 0, "uid-less image: stale texture drawn");
    /* in-frame reuse of one buffer for two pictures */
    frame_begin();
    memset(px, 7, sizeof(px));
    plat_image(0, 0, &b, 1, 0, 0);
    memset(px, 8, sizeof(px));
    plat_image(40, 0, &b, 1, 0, 0);
    frame_end();
    /* (the native frame holds both pictures as drawn) */
    CHECK(frame_diff(NULL) == 0, "buffer reused within a frame: wrong picture");
    /* pressure: 16 images of 256x128 (32 KB each as 256x128 textures) every frame
     * for many frames with new uids, so video RAM (4 MB) fills and evicts */
    for (i = 0; i < 16; i++) {
        memset(big[i], 10 + i, sizeof(big[i]));
        bigimg[i].w = 256; bigimg[i].h = 128; bigimg[i].pix = big[i];
    }
    for (f = 0; f < 40; f++) {
        frame_begin();
        for (i = 0; i < 16; i++) {
            bigimg[i].uid = 0x02000000u + (uint32_t)(f * 16 + i);
            plat_image((i % 4) * 70, (i / 4) * 50, &bigimg[i], 1, 0, 0);
        }
        frame_end();
        if (frame_diff(NULL) != 0) { CHECK(0, "pressure frame %d differs", f); break; }
    }
    CHECK(dc2d_stats.evictions > 0, "pressure: nothing evicted");
    CHECK(swpvr_tex_used() <= 4u * 1024u * 1024u, "pressure: over the video RAM budget");
}

/* ---- held frame ------------------------------------------------------------------------------------ */
static void test_hold(void)
{
    frame_begin();
    plat_clear(4);
    plat_rect(10, 10, 100, 50, 11);
    plat_text(20, 20, "held", 2);
    frame_end();
    CHECK(plat_hold_frame(1) == 1, "hold: not held");
    plat_begin_frame();                 /* no clear: the held picture is the backdrop */
    plat_rect(200, 100, 30, 30, 8);     /* only the new rect */
    frame_end();
    CHECK(dc2d_last()->nheld > 0, "hold: no backdrop submitted");
    CHECK(frame_diff(NULL) == 0, "hold: frame differs from the native held frame");
    plat_begin_frame();
    plat_rect(150, 150, 30, 30, 9);
    frame_end();
    CHECK(frame_diff(NULL) == 0, "hold: second frame differs");
    CHECK(plat_hold_frame(4) == 1, "hold: still held");
    plat_hold_frame(0);
    frame_begin();
    plat_clear(1);
    frame_end();
    CHECK(dc2d_last()->nheld == 0 && frame_diff(NULL) == 0, "hold released: backdrop still drawn");
}

/* ---- vertex budget ----------------------------------------------------------------------------------- */
static void test_budget(void)
{
    int i;
    uint32_t d0 = dc2d_stats.dropped;
    frame_begin();
    for (i = 0; i < 14000; i++) plat_rect((i * 2) % 320, (i * 2 / 320) % 240, 1, 1, (uint8_t)(1 + i % 200));
    frame_end();
    CHECK(dc2d_stats.dropped > d0, "budget: nothing dropped from 14000 single pixels");
    CHECK(dc2d_stats.vbytes <= DC2D_VBUF_BUDGET, "budget: %u bytes over %u", dc2d_stats.vbytes, DC2D_VBUF_BUDGET);
}

/* ---- post effects -------------------------------------------------------------------------------------- */
static void test_post(void)
{
    const DcFrame *f;
    frame_begin();
    plat_rect(10, 10, 10, 10, 5);
    plat_post_offset(3, -2);
    plat_rect(10, 10, 10, 10, 6);       /* drawn after: not moved */
    frame_end();
    f = dc2d_last();
    CHECK(f->n == 2 && f->prims[0].x0 == 26.0f && f->prims[0].y0 == 16.0f && f->prims[1].x0 == 20.0f,
          "offset: moved the wrong prims");
    frame_begin();
    plat_rect(150, 110, 20, 20, 5);
    plat_post_scale(512, 512, 160, 120);
    frame_end();
    f = dc2d_last();
    CHECK(f->n == 1 && f->prims[0].x0 == 280.0f && f->prims[0].x1 == 360.0f, "scale: x %.1f..%.1f",
          (double)f->prims[0].x0, (double)f->prims[0].x1);
    frame_begin();
    plat_rect(0, 0, 50, 50, 20);
    plat_pal_lerp(PAL_SEPIA, 16);
    plat_rect(60, 0, 50, 50, 20);
    frame_end();
    f = dc2d_last();
    CHECK(f->lerp && (f->prims[0].flags & DC2D_PF_LERP) && !(f->prims[1].flags & DC2D_PF_LERP),
          "pal_lerp: flags");
    CHECK(dc2d_prim_argb(f, &f->prims[0]) ==
          (0xFF000000u | (dc2d_pal_argb(f->lerp_bank, 20) & 0xFFFFFFu)), "pal_lerp: colour from the lerp bank");
    frame_begin();
    plat_rect(0, 0, 320, 240, 2);
    plat_post_blend(PAL_BLACK, 8);
    frame_end();
    {
        int me;
        frame_diff(&me);
        CHECK(me <= 24, "blend 8 (black): max error %d against the native darken", me);
    }
}

int main(void)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = 320; o.h = 240; o.save_dir = "."; o.seed = 1; o.quiet = 1;
    nat_init(&o);
    test_twiddle();
    test_uv16();
    test_merge();
    test_fuzz();
    test_shade();
    test_shade_exact();
    test_texcache();
    test_hold();
    test_budget();
    test_post();
    printf("dc2d tests: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
