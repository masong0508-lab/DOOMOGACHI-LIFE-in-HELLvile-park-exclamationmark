/* test_camfx.c - comic post effects (camfx.h; owner: S1, SPEC sec.7 S1
 * "test_camfx": every profile's samples equal CT Appendix C;
 * OPT_SCREEN_SHAKE 0/50/100% scales amplitude only, so the same frames are
 * marked; at OPT_REDUCE_FLASH=1 no level exceeds 6/16 and no red flash
 * remains). Also the iris shapes, the palette lerps and the software
 * fallbacks (screenshots in the test tmp dir: camfx_*.png). */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/camfx.h"
#include "doomo/tween.h"

/* CT Appendix C, re-derived independently: {frames, x[], y[]} at 240x160 */
typedef struct { int frames; int x[14], y[14]; } Want;
static void want_of(int p, Want *w)
{
    static const struct { int frames, xmode, ymode, yc; int amp[14]; } T[SHK_COUNT] = {
        {  2, 2, 3, 0, { 3, -1 } },
        {  3, 1, 0, 0, { 1, -2, 1 } },
        {  3, 0, 2, 1, { 2, 2, 2 } },
        {  3, 0, 2, 2, { 3, 3, 3 } },
        {  3, 0, 2, 2, { 3, 2, 1 } },
        {  5, 0, 2, 2, { 3, 3, 2, 2, 1 } },
        {  5, 0, 1, 0, { 3, 3, 3, 3, 3 } },
        {  5, 0, 2, 2, { 3, 3, 3, 3, 3 } },
        {  7, 0, 1, 0, { 7, 6, 5, 4, 3, 2, 1 } },
        {  8, 0, 2, 2, { 4, 4, 3, 3, 2, 2, 1, 1 } },
        {  8, 0, 1, 0, { 3, 3, 3, 3, 3, 3, 3, 3 } },
        { 12, 0, 2, 2, { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 } },
        { 14, 0, 1, 0, { 4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1 } },
        {  1, 0, 2, 1, { 1 } },
    };
    int f;
    memset(w, 0, sizeof *w);
    w->frames = T[p].frames;
    for (f = 0; f < T[p].frames; f++) {
        int a = T[p].amp[f];
        w->x[f] = T[p].xmode == 0 ? ((f & 1) ? a : -a) : T[p].xmode == 1 ? a : 0;
        w->y[f] = T[p].ymode == 1 ? ((f & 2) ? a / 2 : -(a / 2))
                : T[p].ymode == 2 ? ((f & 2) ? T[p].yc : -T[p].yc)
                : T[p].ymode == 3 ? a : 0;
    }
}

static void res(int w, int h) { nat_w = w; nat_h = h; }

TEST(camfx_profiles_equal_ct_appendix_c)
{
    int p, f;
    res(240, 160);
    for (p = 0; p < SHK_COUNT; p++) {
        Want w;
        want_of(p, &w);
        camfx_reset();
        camfx_tick();
        camfx_shake(p);
        for (f = 0; f < w.frames + 3; f++) {
            int dx, dy;
            if (f) camfx_tick();
            camfx_offset(&dx, &dy);
            if (f < w.frames) {
                CHECK_MSG(camfx_shaking(), "profile %d frame %d not marked", p, f);
                CHECK_MSG(dx == w.x[f] && dy == w.y[f], "profile %d frame %d: (%d,%d) want (%d,%d)",
                          p, f, dx, dy, w.x[f], w.y[f]);
            } else {
                CHECK_MSG(!camfx_shaking() && dx == 0 && dy == 0, "profile %d frame %d still shaking", p, f);
            }
        }
    }
    /* the PS1 screen scales x by 4/3 and y by 3/2 (CT Appendix C) */
    res(320, 240);
    camfx_reset(); camfx_tick();
    camfx_shake(SHK_SHOTGUN);
    {
        int dx, dy;
        camfx_offset(&dx, &dy);
        CHECK_EQ(dx, -9);                       /* -7 x 4/3 = -9.3 */
        CHECK_EQ(dy, -5);                       /* -(7/2) x 3/2 = -4.5 -> -5 */
    }
    res(240, 160);
}

TEST(camfx_screen_shake_option_scales_amplitude_only)
{
    int lvl, f, frames[3] = { 0, 0, 0 }, peak[3] = { 0, 0, 0 };
    uint8_t c;
    res(240, 160);
    for (lvl = 0; lvl <= 2; lvl++) {
        test_reset();
        opt_set(OPT_SCREEN_SHAKE, lvl);
        camfx_reset(); camfx_tick();
        camfx_shake(SHK_ROAR);
        for (f = 0; f < 20; f++) {
            int dx, dy;
            if (f) camfx_tick();
            camfx_offset(&dx, &dy);
            if (camfx_shaking()) frames[lvl]++;
            if (dx < 0) dx = -dx;
            if (dx > peak[lvl]) peak[lvl] = dx;
            /* at 0% the first frame is a 4/16 darken pulse instead */
            if (lvl == 0) CHECK_EQ(camfx_blend(&c), f == 0 ? 4 : 0);
            if (lvl == 0 && f == 0) CHECK_EQ(c, PAL_BLACK);
        }
    }
    CHECK_EQ(frames[0], 14); CHECK_EQ(frames[1], 14); CHECK_EQ(frames[2], 14);
    CHECK_EQ(peak[0], 0); CHECK_EQ(peak[1], 2); CHECK_EQ(peak[2], 4);
    /* the punch is a TW_SLAM4 kick that comes back to rest, scaled the same */
    test_reset();
    camfx_reset(); camfx_tick();
    camfx_punch(6, -4);
    {
        int dx, dy;
        camfx_offset(&dx, &dy);
        CHECK_EQ(dx, 6); CHECK_EQ(dy, -4);
        for (f = 0; f < 30; f++) camfx_tick();
        camfx_offset(&dx, &dy);
        CHECK_EQ(dx, 0); CHECK_EQ(dy, 0);
        opt_set(OPT_SCREEN_SHAKE, 1);
        camfx_punch(6, 0);
        camfx_offset(&dx, &dy);
        CHECK_EQ(dx, 3);
    }
}

static int flash_seq(int color, int level, int frames, int *out, uint8_t *col)
{
    int f, n = 0;
    camfx_reset(); camfx_tick();
    camfx_flash((uint8_t)color, level, frames);
    for (f = 0; f < frames + 2; f++) {
        if (f) camfx_tick();
        out[n++] = camfx_blend(col);
    }
    return n;
}

TEST(camfx_flash_levels_and_safety)
{
    int s[20], i;
    uint8_t c;
    flash_seq(PAL_WHITE, 10, 2, s, &c);                  /* impact white */
    CHECK_EQ(s[0], 10); CHECK_EQ(s[1], 5); CHECK_EQ(s[2], 0);
    flash_seq(PAL_WHITE, 6, 3, s, &c);                   /* failure      */
    CHECK_EQ(s[0], 6); CHECK_EQ(s[1], 4); CHECK_EQ(s[2], 2); CHECK_EQ(s[3], 0);
    flash_seq(PAL_WHITE, 16, 4, s, &c);                  /* in from white */
    CHECK_EQ(s[0], 16); CHECK_EQ(s[1], 12); CHECK_EQ(s[2], 8); CHECK_EQ(s[3], 4);
    /* OPT_REDUCE_FLASH: nothing above 6/16, no red flash survives */
    opt_set(OPT_REDUCE_FLASH, 1);
    flash_seq(PAL_WHITE, 16, 4, s, &c);
    for (i = 0; i < 6; i++) CHECK(s[i] <= 6);
    CHECK_EQ(s[0], 6);
    flash_seq(PAL_UI_RED, 12, 3, s, &c);
    CHECK(s[0] > 0 && s[0] <= 6);
    CHECK_EQ(c, PAL_BLACK);
    flash_seq(PAL_RAMP(RAMP_LAVA, 5), 12, 3, s, &c);
    CHECK_EQ(c, PAL_BLACK);
    opt_set(OPT_REDUCE_FLASH, 0);
    flash_seq(PAL_UI_RED, 12, 3, s, &c);
    CHECK_EQ(c, PAL_UI_RED); CHECK_EQ(s[0], 12);
    /* full-screen flashes never start less than 10 frames apart (< 3 Hz) */
    {
        int f, starts = 0, prev = 0;
        camfx_reset(); camfx_tick();
        for (f = 0; f < 60; f++) {
            int l;
            camfx_tick();
            camfx_flash(PAL_WHITE, 8, 1);                /* asked for every frame */
            l = camfx_blend(&c);
            if (l && !prev) starts++;
            prev = l;
        }
        CHECK(starts <= 6);
        CHECK(starts >= 5);
    }
    /* fades: instant and ramped, black wins over a weaker black flash */
    camfx_reset(); camfx_tick();
    camfx_fade_to(16, 4);
    CHECK_EQ(camfx_fade_level(), 4);
    camfx_tick(); camfx_tick(); camfx_tick();
    CHECK_EQ(camfx_fade_level(), 16);
    CHECK_EQ(camfx_blend(&c), 16); CHECK_EQ(c, PAL_BLACK);
    camfx_fade(0);
    CHECK_EQ(camfx_blend(&c), 0);
}

TEST(camfx_zoom_iris_and_palettes)
{
    int f, r, prev, sp[4], n;
    res(240, 160);
    camfx_reset(); camfx_tick();
    camfx_zoom_snap(320, 2);
    CHECK_EQ(camfx_zoom(), 320);
    camfx_tick(); CHECK_EQ(camfx_zoom(), 320);
    camfx_tick(); CHECK_EQ(camfx_zoom(), 256);
    /* [IRIS out 10] on a point: R5 close, hold 10, R3 to black, stays black */
    camfx_iris_out(120, 80, 26, 1, 10);
    prev = 1000;
    for (f = 0; f < 30; f++) {
        r = camfx_iris_radius();
        CHECK(r <= prev);
        if (f >= 4 && f < 15) CHECK_EQ(r, 26);
        if (f >= 17) CHECK_EQ(r, 0);
        prev = r;
        camfx_tick();
    }
    camfx_iris_in(120, 80, 26, 0, 4);
    for (f = 0; f < 3 + 4 + 5; f++) { CHECK(camfx_iris_radius() >= 0); camfx_tick(); }
    CHECK_EQ(camfx_iris_radius(), -1);
    /* circle rows are symmetric; a heart has two lobes over one point */
    n = camfx_iris_row(80, 120, 80, 30, 0, sp);
    CHECK_EQ(n, 1); CHECK_EQ(sp[0], 90); CHECK_EQ(sp[1], 150);
    CHECK_EQ(camfx_iris_row(80 - 30, 120, 80, 30, 0, sp), 0);
    CHECK_EQ(camfx_iris_row(80 - 30, 120, 80, 40, 1, sp), 2);   /* the cleft */
    CHECK(sp[1] <= 120 && sp[2] >= 120);
    CHECK_EQ(camfx_iris_row(80 + 20, 120, 80, 40, 1, sp), 1);   /* the point */
    CHECK(sp[0] > 90 && sp[1] < 150);
    CHECK_EQ(camfx_iris_row(80 + 40, 120, 80, 40, 1, sp), 0);
    /* palettes: level 0 is the master palette, 16 the target */
    for (f = 0; f < 256; f++) {
        CHECK_EQ(camfx_pal_entry(f, PAL_SEPIA, 0, 0, 0), doomo_palette[f]);
        CHECK_EQ(camfx_pal_entry(f, PAL_LERP_NONE, 9, PAL_WHITE, 16), doomo_palette[PAL_WHITE]);
    }
    {
        uint16_t s = camfx_pal_entry(PAL_UI_MINT, PAL_SEPIA, 16, 0, 0);
        int rr = s & 31, g = (s >> 5) & 31, b = (s >> 10) & 31;
        CHECK(rr > g && g > b);                                 /* warm brown */
        s = camfx_pal_entry(PAL_UI_MINT, PAL_HELLIFY, 16, 0, 0);
        CHECK((s & 31) > ((s >> 5) & 31) * 2);                  /* red */
    }
    camfx_pal_lerp(PAL_CRAYON, 8);
    CHECK_EQ(camfx_pal_state(&r), PAL_CRAYON); CHECK_EQ(r, 8);
    camfx_pal_lerp_to(PAL_CRAYON, 0, 4);                        /* fade it out: 6 4 2 0 */
    CHECK_EQ(camfx_pal_state(&r), PAL_CRAYON); CHECK_EQ(r, 6);
    camfx_tick(); camfx_tick();
    CHECK_EQ(camfx_pal_state(&r), PAL_CRAYON); CHECK_EQ(r, 2);
    camfx_tick();
    CHECK_EQ(camfx_pal_state(&r), PAL_LERP_NONE);
}

static int count(uint8_t c)
{
    int i, n = 0;
    for (i = 0; i < nat_w * nat_h; i++) n += nat_fb[i] == c;
    return n;
}

static void scene_bg(void)
{
    plat_clear(PAL_UI_PINK);
    ui_window(8, 8, nat_w - 16, nat_h / 2, 0);
    ui_text_center(nat_w / 2, 20, "camfx: iris + flash", PAL_BLACK);
}

static void shot(const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/camfx_%s_%dx%d.png", test_tmp_dir, name, nat_w, nat_h);
    nat_png_write(path, nat_fb, nat_w, nat_h, 2);
}

TEST(camfx_software_fallbacks_draw)
{
    int k;
    for (k = 0; k < 2; k++) {
        int black;
        res(k ? 320 : 240, k ? 240 : 160);
        camfx_reset(); camfx_tick();
        /* a heart iris with a gold rim around a point */
        scene_bg();
        camfx_iris(nat_w / 2, nat_h / 2, nat_h / 3, 1, PAL_UI_GOLD);
        camfx_apply();
        black = count(PAL_BLACK);
        CHECK(black > nat_w * nat_h / 2);
        CHECK(count(PAL_UI_GOLD) > 20);
        CHECK(nat_fb[(nat_h / 2) * nat_w + nat_w / 2] != PAL_BLACK);
        CHECK_EQ(nat_fb[0], PAL_BLACK);
        shot("iris_heart");
        /* a circle iris closing */
        scene_bg();
        camfx_iris_off();
        camfx_iris(nat_w / 3, nat_h / 2, nat_h / 4, 0, 0);
        camfx_apply();
        CHECK(nat_fb[(nat_h / 2) * nat_w + nat_w / 3] != PAL_BLACK);
        CHECK_EQ(nat_fb[(nat_h / 2) * nat_w + nat_w - 2], PAL_BLACK);
        shot("iris_circle");
        /* a white flash at 10/16: the checker */
        scene_bg();
        camfx_iris_off();
        camfx_tick();
        camfx_tick();
        camfx_tick();
        camfx_tick();
        camfx_tick();
        camfx_tick();
        camfx_tick();
        camfx_tick();
        camfx_tick();
        camfx_tick();
        camfx_flash(PAL_WHITE, 10, 2);
        camfx_apply();
        CHECK(count(PAL_WHITE) > nat_w * nat_h / 3);
        shot("flash");
        /* a black fade half way */
        scene_bg();
        camfx_reset(); camfx_tick();
        camfx_fade(8);
        camfx_apply();
        shot("fade");
    }
    res(240, 160);
}

TEST(camfx_xform_squash_fallback)
{
    static uint8_t pix[16 * 16];
    DoomoImage img;
    int x, y, minx = 999, maxx = -1, miny = 999;
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) pix[y * 16 + x] = (x < 8) ? PAL_UI_RED : PAL_UI_GOLD;
    img.w = 16; img.h = 16; img.pix = pix; img.uid = 0x00F00001u;
    res(240, 160);
    plat_clear(PAL_UI_CREAM);
    /* a DROP squash: wide and short, anchored at the feet (120, 100) */
    plat_image_xform(120, 100, &img, 353, 159, 0, 0);
    for (y = 0; y < nat_h; y++)
        for (x = 0; x < nat_w; x++) {
            uint8_t c = nat_fb[y * nat_w + x];
            if (c == PAL_UI_RED || c == PAL_UI_GOLD) {
                if (x < minx) minx = x;
                if (x > maxx) maxx = x;
                if (y < miny) miny = y;
                CHECK(y < 100);
            }
        }
    CHECK_EQ(maxx - minx + 1, 22);                   /* 16 x 1.38 */
    CHECK_EQ(100 - miny, 10);                        /* 16 x 0.62 */
    CHECK_EQ(nat_fb[95 * nat_w + minx], PAL_UI_RED);
    /* a negative sx flips it */
    plat_clear(PAL_UI_CREAM);
    plat_image_xform(120, 100, &img, -256, 256, 0, 0);
    CHECK_EQ(nat_fb[95 * nat_w + 113], PAL_UI_GOLD);
    CHECK_EQ(nat_fb[95 * nat_w + 126], PAL_UI_RED);
    shot("xform");
}
