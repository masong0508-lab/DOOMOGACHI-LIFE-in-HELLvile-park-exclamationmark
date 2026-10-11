/* stubs.c - weak placeholders for every cross-module symbol (owner:
 * foundation).
 *
 * Every function, table and scene declared in a header that foundation
 * does not implement gets a __attribute__((weak)) definition here, so the
 * game always links and runs while the other modules land. The owners'
 * strong definitions replace these at link time (objects are linked
 * directly, never through archives - see core/sources.mk).
 *
 * Each stub function calls plat_log("STUB: <name>") once and returns a
 * safe default. Placeholder art is const (ROM on GBA). Stub scenes draw a
 * cute "UNDER CONSTRUCTION" card; the stub title screen is a launcher for
 * every scene. The native runner's --assert-no-stubs flag fails a run if
 * any "STUB:" line was logged.
 */
#include "doomo/doomo.h"

#define WEAK __attribute__((weak))
#define STUB(name) do { static uint8_t once_; \
        if (!once_) { once_ = 1; plat_log("STUB: " name); } } while (0)

/* ======================================================================
 * palette (art) - palette.c normally provides all of this
 * ==================================================================== */
#define SC(r, g, b) ((uint16_t)((((r) >> 3) & 31) | ((((g) >> 3) & 31) << 5) | \
                               ((((b) >> 3) & 31) << 10)))
/* paint: dark / base / light from one base colour */
#define SP(r, g, b) SC((r) * 3 / 4, (g) * 5 / 8, (b) * 3 / 4 + 16), SC(r, g, b), \
                    SC((r) + (255 - (r)) / 2, (g) + (255 - (g)) / 2, (b) + (255 - (b)) / 2)
/* ramp: 8 shades lerped from a dark to a light colour */
#define SL(a, b, i) ((a) + ((b) - (a)) * (i) / 7)
#define SR1(r0, g0, b0, r1, g1, b1, i) SC(SL(r0, r1, i), SL(g0, g1, i), SL(b0, b1, i))
#define SR(r0, g0, b0, r1, g1, b1) \
    SR1(r0, g0, b0, r1, g1, b1, 0), SR1(r0, g0, b0, r1, g1, b1, 1), \
    SR1(r0, g0, b0, r1, g1, b1, 2), SR1(r0, g0, b0, r1, g1, b1, 3), \
    SR1(r0, g0, b0, r1, g1, b1, 4), SR1(r0, g0, b0, r1, g1, b1, 5), \
    SR1(r0, g0, b0, r1, g1, b1, 6), SR1(r0, g0, b0, r1, g1, b1, 7)

WEAK const uint16_t doomo_palette[256] = {
    SC(0, 0, 0), SC(0, 0, 0), SC(255, 255, 255), SC(204, 200, 216),
    SC(140, 136, 156), SC(76, 72, 92), SC(255, 184, 208), SC(208, 80, 136),
    SC(168, 240, 208), SC(48, 160, 128), SC(255, 248, 228), SC(232, 40, 48),
    SC(120, 8, 24), SC(255, 208, 64), SC(168, 88, 224), SC(176, 200, 120),
    SP(238, 240, 250), SP(164, 168, 188), SP(64, 64, 84), SP(24, 22, 32),
    SP(224, 40, 72), SP(136, 8, 24), SP(232, 96, 140), SP(255, 144, 200),
    SP(255, 168, 144), SP(248, 128, 40), SP(152, 68, 32), SP(232, 160, 40),
    SP(255, 224, 64), SP(176, 216, 40), SP(88, 208, 72), SP(120, 224, 184),
    SP(32, 104, 64), SP(24, 152, 160), SP(72, 216, 248), SP(120, 184, 248),
    SP(48, 104, 232), SP(32, 40, 120), SP(192, 168, 248), SP(120, 48, 176),
    SP(176, 80, 232), SP(240, 56, 200), SP(96, 56, 36), SP(200, 136, 72),
    SP(224, 216, 176), SP(144, 216, 40), SP(255, 104, 0), SP(104, 248, 200),
    SP(255, 224, 216), SP(248, 204, 176), SP(240, 180, 144), SP(216, 152, 104),
    SP(192, 144, 88), SP(160, 104, 64), SP(120, 72, 48), SP(80, 48, 40),
    SR(20, 18, 30, 226, 226, 238), SR(40, 20, 20, 248, 216, 168),
    SR(12, 40, 40, 228, 248, 164), SR(76, 20, 52, 255, 236, 244),
    SR(8, 10, 36, 214, 236, 255), SR(26, 10, 40, 242, 220, 255),
    SR(68, 34, 22, 255, 252, 216), SR(10, 40, 48, 226, 255, 238),
    SR(72, 30, 32, 255, 242, 224), SR(24, 2, 8, 255, 112, 96),
    SR(50, 8, 6, 255, 248, 172), SR(40, 12, 22, 248, 196, 178),
    SR(36, 28, 26, 250, 246, 228), SR(18, 28, 12, 234, 242, 130),
    SR(14, 18, 26, 214, 222, 236)
};
WEAK const char *const doomo_paint_names[PAL_PAINT_COUNT] = {
    "White", "Silver", "Charcoal", "Jet", "Cherry", "Blood", "Rose",
    "Bubblegum", "Peach", "Tangerine", "Rust", "Honey", "Lemon",
    "Chartreuse", "Lime", "Mint", "Forest", "Teal", "Cyan", "Sky", "Cobalt",
    "Navy", "Lavender", "Grape", "Vape Purple", "Magenta", "Chocolate",
    "Caramel", "Bone", "Slime", "Hellfire", "Ectoplasm"
};
WEAK const char *const doomo_skin_names[PAL_SKIN_COUNT] = {
    "Porcelain", "Fair", "Peach", "Tan", "Olive", "Brown", "Umber", "Ebony"
};
WEAK const char *const doomo_ramp_names[PAL_RAMP_COUNT] = {
    "Stone", "Wood", "Grass", "Pastel Pink", "Sky", "Vape", "Candy", "Mint",
    "Peach", "Blood", "Lava", "Flesh", "Bone", "Sick", "Steel"
};
WEAK const char *const doomo_tint_names[TINT_COUNT] = {
    "None", "Red", "Gold", "Green", "Purple", "Night"
};
WEAK uint8_t doomo_colormap[DOOMO_LIGHT_LEVELS][256];
WEAK uint8_t doomo_tintmap[TINT_COUNT][256];
/* All-zero placeholders for the generated tables; doomo_init() notices the
 * broken identity row and calls doomo_palette_fallback(). */
WEAK const uint8_t doomo_art_colormap[DOOMO_LIGHT_LEVELS][256];
WEAK const uint8_t doomo_art_tintmap[TINT_COUNT][256];
WEAK const uint8_t doomo_art_darker[256];
WEAK const uint8_t doomo_art_lighter[256];

WEAK void palette_rgb(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint16_t c = doomo_palette[index];
    int r5 = c & 31, g5 = (c >> 5) & 31, b5 = (c >> 10) & 31;
    if (r) *r = (uint8_t)((r5 << 3) | (r5 >> 2));
    if (g) *g = (uint8_t)((g5 << 3) | (g5 >> 2));
    if (b) *b = (uint8_t)((b5 << 3) | (b5 >> 2));
}

WEAK uint32_t palette_distance(int r1, int g1, int b1, int r2, int g2, int b2)
{
    int rm = (r1 + r2) >> 1;
    int dr = r1 - r2, dg = g1 - g2, db = b1 - b2;
    return (uint32_t)((((512 + rm) * dr * dr) >> 8) + 4 * dg * dg +
                      (((767 - rm) * db * db) >> 8));
}

WEAK uint8_t palette_nearest(int r, int g, int b)
{
    uint32_t best = 0xFFFFFFFFu;
    int i, besti = 1;
    for (i = 1; i < 256; i++) {
        uint8_t pr, pg, pb;
        uint32_t d;
        palette_rgb((uint8_t)i, &pr, &pg, &pb);
        d = palette_distance(r, g, b, pr, pg, pb);
        if (d < best) { best = d; besti = i; }
    }
    return (uint8_t)besti;
}

WEAK int palette_luma(uint8_t index)
{
    uint8_t r, g, b;
    palette_rgb(index, &r, &g, &b);
    return (r * 77 + g * 150 + b * 29) >> 8;
}

WEAK uint8_t palette_darker(uint8_t index) { return ui_darker(index); }
WEAK uint8_t palette_lighter(uint8_t index) { return ui_lighter(index); }
WEAK uint8_t palette_shade(uint8_t index, int light)
{
    light = DOOMO_CLAMP(light, 0, DOOMO_LIGHT_LEVELS - 1);
    return doomo_colormap[light][index];
}

WEAK void palette_init(void)
{
    STUB("palette_init");
    doomo_palette_fallback();
}

/* Not weak: foundation's runtime fallback for missing generated tables. */
void doomo_palette_fallback(void)
{
    static const uint8_t tint_rgb[TINT_COUNT][3] = {
        { 0, 0, 0 }, { 255, 40, 40 }, { 255, 220, 80 }, { 80, 255, 80 },
        { 180, 80, 255 }, { 40, 50, 140 }
    };
    int lv, i;
    for (i = 0; i < 256; i++) {
        uint8_t r, g, b;
        palette_rgb((uint8_t)i, &r, &g, &b);
        for (lv = 0; lv < DOOMO_LIGHT_LEVELS; lv++) {
            if (i == 0) { doomo_colormap[lv][i] = 0; continue; }
            if (lv == DOOMO_LIGHT_LEVELS - 1) { doomo_colormap[lv][i] = (uint8_t)i; continue; }
            doomo_colormap[lv][i] = palette_nearest(r * lv / 15, g * lv / 15, b * lv / 15);
        }
        for (lv = 0; lv < TINT_COUNT; lv++) {
            if (i == 0 || lv == TINT_NONE) { doomo_tintmap[lv][i] = (uint8_t)i; continue; }
            doomo_tintmap[lv][i] = palette_nearest((r + tint_rgb[lv][0]) / 2,
                                                   (g + tint_rgb[lv][1]) / 2,
                                                   (b + tint_rgb[lv][2]) / 2);
        }
    }
}

/* ======================================================================
 * placeholder art (const, so it lives in ROM)
 * ==================================================================== */
#define X4(v) v, v, v, v
#define X8(v) X4(v), X4(v)
#define X16(v) X8(v), X8(v)
#define X32(v) X16(v), X16(v)
#define X64(v) X32(v), X32(v)
#define X128(v) X64(v), X64(v)
#define X256(v) X128(v), X128(v)
#define X2048(v) X256(v), X256(v), X256(v), X256(v), X256(v), X256(v), X256(v), X256(v)
/* 64 px row of 8 px checker cells; each 8-row band starts with a seam row */
#define CROW(a, b) X8(a), X8(b), X8(a), X8(b), X8(a), X8(b), X8(a), X8(b)
#define CBAND(a, b, s) CROW(s, s), CROW(a, b), CROW(a, b), CROW(a, b), \
                       CROW(a, b), CROW(a, b), CROW(a, b), CROW(a, b)
#define CTEX(a, b, s) CBAND(a, b, s), CBAND(b, a, s), CBAND(a, b, s), CBAND(b, a, s), \
                      CBAND(a, b, s), CBAND(b, a, s), CBAND(a, b, s), CBAND(b, a, s)

static const uint8_t stub_tex_pix[3][64 * 64] = {
    { CTEX(PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 4)) },
    { CTEX(PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 3)) },
    { CTEX(PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 6), PAL_RAMP(RAMP_STONE, 4)) }
};
static const uint8_t stub_sky_pix[SKY_W * SKY_H] = {
    X2048((PAL_RAMP(RAMP_SKY, 4))), X2048((PAL_RAMP(RAMP_SKY, 4))),
    X2048((PAL_RAMP(RAMP_SKY, 5))), X2048((PAL_RAMP(RAMP_SKY, 5))),
    X2048((PAL_RAMP(RAMP_SKY, 6))), X2048((PAL_RAMP(RAMP_SKY, 6))),
    X2048((PAL_RAMP(RAMP_SKY, 7))), X2048((PAL_RAMP(RAMP_SKY, 7))),
    X2048((PAL_RAMP(RAMP_PASTEL_PINK, 7))), X2048((PAL_RAMP(RAMP_PASTEL_PINK, 7))),
    X2048((PAL_RAMP(RAMP_PASTEL_PINK, 6))), X2048((PAL_RAMP(RAMP_PASTEL_PINK, 6)))
};
DOOMO_STATIC_ASSERT(sizeof(stub_sky_pix) == SKY_W * SKY_H, stub_sky_size);

/* A little pink plush ghost, 16x16 ("?" sprite for anything missing). */
#define t_ 0
#define o_ PAL_UI_PINK_D
#define f_ PAL_UI_PINK
#define h_ PAL_RAMP(RAMP_PASTEL_PINK, 7)
#define s_ PAL_RAMP(RAMP_PASTEL_PINK, 4)
#define k_ PAL_RAMP(RAMP_PASTEL_PINK, 0)
#define w_ PAL_WHITE
#define b_ PAL_PAINT(PAINT_ROSE, 1)
static const uint8_t stub_blob_pix[16 * 16] = {
    t_,t_,t_,t_,t_,o_,o_,o_,o_,o_,o_,t_,t_,t_,t_,t_,
    t_,t_,t_,o_,o_,h_,h_,f_,f_,f_,f_,o_,o_,t_,t_,t_,
    t_,t_,o_,h_,h_,h_,f_,f_,f_,f_,f_,f_,f_,o_,t_,t_,
    t_,o_,h_,h_,f_,f_,f_,f_,f_,f_,f_,f_,f_,f_,o_,t_,
    t_,o_,h_,f_,f_,f_,f_,f_,f_,f_,f_,f_,f_,f_,o_,t_,
    o_,f_,f_,f_,k_,k_,f_,f_,f_,f_,k_,k_,f_,f_,s_,o_,
    o_,f_,f_,k_,w_,k_,k_,f_,f_,k_,w_,k_,k_,f_,s_,o_,
    o_,f_,f_,k_,k_,k_,k_,f_,f_,k_,k_,k_,k_,f_,s_,o_,
    o_,f_,b_,b_,k_,k_,f_,f_,f_,f_,k_,k_,b_,b_,s_,o_,
    o_,f_,f_,f_,f_,f_,k_,f_,f_,k_,f_,f_,f_,f_,s_,o_,
    o_,f_,f_,f_,f_,f_,f_,k_,k_,f_,f_,f_,f_,f_,s_,o_,
    o_,f_,f_,f_,f_,f_,f_,f_,f_,f_,f_,f_,f_,s_,s_,o_,
    o_,s_,f_,f_,f_,f_,f_,f_,f_,f_,f_,f_,s_,s_,s_,o_,
    o_,s_,s_,f_,f_,s_,s_,f_,f_,s_,s_,f_,f_,s_,s_,o_,
    t_,o_,s_,s_,o_,o_,s_,s_,o_,o_,s_,s_,o_,s_,o_,t_,
    t_,t_,o_,o_,t_,t_,o_,o_,t_,t_,o_,o_,t_,o_,t_,t_
};
#undef t_
#undef o_
#undef f_
#undef h_
#undef s_
#undef k_
#undef w_
#undef b_
static const DoomoImage stub_blob = { 16, 16, stub_blob_pix, 0x00F00001u };

/* ======================================================================
 * textures / sprites (art)
 * ==================================================================== */
static const DoomoImage stub_tex_img[3] = {
    { 64, 64, stub_tex_pix[0], 0x00F00010u },
    { 64, 64, stub_tex_pix[1], 0x00F00011u },
    { 64, 64, stub_tex_pix[2], 0x00F00012u }
};
static int stub_tex_kind(int tex_id)
{
    switch (tex_id) {
    case TEX_GRASS: case TEX_FLOWERS: case TEX_HEDGE: case TEX_TREE:
    case TEX_INFECTED_GRASS: case TEX_SLIME: case TEX_PATH: case TEX_SAND:
        return 0;
    case TEX_PASTEL_BRICK: case TEX_CANDY_STRIPE: case TEX_WALLPAPER_HEARTS:
    case TEX_CARPET: case TEX_FLESH_WALL: case TEX_BLOOD_BRICK: case TEX_LAVA:
    case TEX_SHOP_FRONT: case TEX_CAFE_FRONT: case TEX_DOOR_CUTE:
        return 1;
    default:
        return 2;
    }
}
WEAK const uint8_t *texture_pixels(int tex_id, uint16_t time)
{
    (void)time;
    STUB("texture_pixels");
    return stub_tex_pix[stub_tex_kind(tex_id)];
}
WEAK const DoomoImage *texture_image(int tex_id, uint16_t time)
{
    (void)time;
    STUB("texture_image");
    return &stub_tex_img[stub_tex_kind(tex_id)];
}
WEAK int texture_is_animated(int tex_id) { (void)tex_id; return 0; }
WEAK uint8_t texture_avg(int tex_id)
{
    static const uint8_t avg[3] = { PAL_RAMP(RAMP_GRASS, 5),
                                    PAL_RAMP(RAMP_PASTEL_PINK, 5),
                                    PAL_RAMP(RAMP_STONE, 5) };
    STUB("texture_avg");
    return avg[stub_tex_kind(tex_id)];
}
WEAK const uint8_t *sky_pixels(int sky_id, uint16_t time)
{
    (void)sky_id; (void)time;
    STUB("sky_pixels");
    return stub_sky_pix;
}
WEAK void textures_init(void) { STUB("textures_init"); }
WEAK int texture_frames(int tex_id) { (void)tex_id; return 1; }
WEAK const char *texture_name(int tex_id) { (void)tex_id; return ""; }
WEAK int sky_is_animated(int sky_id) { (void)sky_id; return 0; }
WEAK uint8_t sky_horizon(int sky_id) { (void)sky_id; return PAL_RAMP(RAMP_PASTEL_PINK, 7); }

WEAK const DoomoImage *sprite_image(int spr_id, int frame)
{
    (void)spr_id; (void)frame;
    STUB("sprite_image");
    return &stub_blob;
}
WEAK int sprite_frames(int spr_id) { (void)spr_id; return 1; }
WEAK void sprites_init(void) { STUB("sprites_init"); }
WEAK int sprite_flags(int spr_id) { (void)spr_id; return 0; }
WEAK const char *sprite_name(int spr_id) { (void)spr_id; return ""; }

/* ======================================================================
 * portraits (portrait) - a simple round plush face placeholder
 * ==================================================================== */
static void sp_px(uint8_t *out, int w, int h, int x, int y, uint8_t c)
{
    if (x >= 0 && y >= 0 && x < w && y < h) out[y * w + x] = c;
}
static void sp_disc(uint8_t *out, int w, int h, int cx, int cy, int rx, int ry,
                    uint8_t c)
{
    int x, y;
    for (y = -ry; y <= ry; y++)
        for (x = -rx; x <= rx; x++)
            if ((x * x) * (ry * ry) + (y * y) * (rx * rx) <= rx * rx * ry * ry)
                sp_px(out, w, h, cx + x, cy + y, c);
}
/* Outlined, shaded disc: outline, base, light upper-left. */
static void sp_ball(uint8_t *out, int w, int h, int cx, int cy, int rx, int ry,
                    uint8_t dark, uint8_t base, uint8_t light)
{
    sp_disc(out, w, h, cx, cy, rx + 1, ry + 1, dark);
    sp_disc(out, w, h, cx, cy, rx, ry, base);
    sp_disc(out, w, h, cx - rx / 4, cy - ry / 4, rx * 2 / 3, ry * 2 / 3, light);
    sp_disc(out, w, h, cx + rx / 6, cy + ry / 5, rx * 3 / 4, ry * 3 / 4, base);
}
static void stub_face(const DoomoLook *l, int status, int expr, int anim,
                      uint8_t *out, int w, int h, int cx, int cy, int r)
{
    uint8_t d = look_skin(l, 0), b = look_skin(l, 1), li = look_skin(l, 2);
    int eye = r / 3 + 1, ey = cy + r / 8;
    int blink = (anim % 90) < 4;
    int ears = l->ears;
    /* ears */
    if (ears == EARS_BUNNY) {
        sp_ball(out, w, h, cx - r / 2, cy - r, r / 5, r / 2, d, b, li);
        sp_ball(out, w, h, cx + r / 2, cy - r, r / 5, r / 2, d, b, li);
    } else if (ears != EARS_NONE && ears != EARS_NORMAL) {
        sp_ball(out, w, h, cx - r * 2 / 3, cy - r * 2 / 3, r / 3, r / 3, d, b, li);
        sp_ball(out, w, h, cx + r * 2 / 3, cy - r * 2 / 3, r / 3, r / 3, d, b, li);
    }
    sp_ball(out, w, h, cx, cy, r, r * 7 / 8, d, b, li);
    /* eyes */
    if (status == RES_POSSESSED || expr == EXPR_POSSESSED) {
        sp_disc(out, w, h, cx - r / 2, ey, eye / 2 + 1, eye / 2 + 1, PAL_PAINT(PAINT_BLOOD, 1));
        sp_disc(out, w, h, cx + r / 2, ey, eye / 2 + 1, eye / 2 + 1, PAL_PAINT(PAINT_BLOOD, 1));
    } else if (blink || expr == EXPR_HAPPY || expr == EXPR_SLEEPY) {
        int i;
        for (i = -eye / 2; i <= eye / 2; i++) {
            sp_px(out, w, h, cx - r / 2 + i, ey - (i == 0), PAL_RAMP(RAMP_PASTEL_PINK, 0));
            sp_px(out, w, h, cx + r / 2 + i, ey - (i == 0), PAL_RAMP(RAMP_PASTEL_PINK, 0));
        }
    } else {
        uint8_t ec = look_paint(l->eye_color, 0);
        sp_disc(out, w, h, cx - r / 2, ey, eye / 2, eye * 2 / 3, PAL_RAMP(RAMP_PASTEL_PINK, 0));
        sp_disc(out, w, h, cx + r / 2, ey, eye / 2, eye * 2 / 3, PAL_RAMP(RAMP_PASTEL_PINK, 0));
        sp_px(out, w, h, cx - r / 2, ey + eye / 3, ec);
        sp_px(out, w, h, cx + r / 2, ey + eye / 3, ec);
        sp_px(out, w, h, cx - r / 2 - 1, ey - 1, PAL_WHITE);
        sp_px(out, w, h, cx + r / 2 - 1, ey - 1, PAL_WHITE);
    }
    /* blush + mouth */
    sp_disc(out, w, h, cx - r * 2 / 3, ey + r / 3, r / 6, r / 10 + 1, PAL_PAINT(PAINT_ROSE, 2));
    sp_disc(out, w, h, cx + r * 2 / 3, ey + r / 3, r / 6, r / 10 + 1, PAL_PAINT(PAINT_ROSE, 2));
    sp_px(out, w, h, cx - 1, ey + r / 3, d);
    sp_px(out, w, h, cx + 1, ey + r / 3, d);
    sp_px(out, w, h, cx, ey + r / 3 + 1, d);
}
WEAK void portrait_render(const DoomoLook *l, int status, int expr, int anim,
                          uint8_t *out)
{
    STUB("portrait_render");
    memset(out, 0, PORTRAIT_W * PORTRAIT_H);
    sp_ball(out, PORTRAIT_W, PORTRAIT_H, 24, 56, 16, 8,
            look_paint(l->top_color, 0), look_paint(l->top_color, 1),
            look_paint(l->top_color, 2));
    stub_face(l, status, expr, anim, out, PORTRAIT_W, PORTRAIT_H, 24, 26, 16);
}
WEAK void bodyspr_render(const DoomoLook *l, int status, int dir, int frame,
                         uint8_t *out)
{
    (void)dir;
    STUB("bodyspr_render");
    memset(out, 0, BODYSPR_W * BODYSPR_H);
    sp_ball(out, BODYSPR_W, BODYSPR_H, 8, 18 + (frame & 1), 4, 4,
            look_paint(l->top_color, 0), look_paint(l->top_color, 1),
            look_paint(l->top_color, 2));
    stub_face(l, status, EXPR_NEUTRAL, frame, out, BODYSPR_W, BODYSPR_H, 8, 9, 6);
}
WEAK void figure_render(const DoomoLook *l, int status, int expr, int anim,
                        uint8_t *out)
{
    STUB("figure_render");
    memset(out, 0, FIGURE_W * FIGURE_H);
    sp_ball(out, FIGURE_W, FIGURE_H, 24, 60, 11, 15,
            look_paint(l->top_color, 0), look_paint(l->top_color, 1),
            look_paint(l->top_color, 2));
    stub_face(l, status, expr, anim, out, FIGURE_W, FIGURE_H, 24, 28, 15);
}
static const DoomoLook *stub_look_of(int id, int *status)
{
    static DoomoResident def;
    const DoomoResident *r = town_res(id);
    if (!r || r->status == RES_EMPTY) {
        if (def.look.eye_count == 0) resident_default(&def);
        r = &def;
    }
    *status = r->status;
    return &r->look;
}
WEAK const DoomoImage *portrait_image(int resident_id, int expr)
{
    static uint8_t buf[PORTRAIT_W * PORTRAIT_H];
    static DoomoImage img;
    int status;
    const DoomoLook *l = stub_look_of(resident_id, &status);
    STUB("portrait_image");
    portrait_render(l, status, expr, (int)doomo_frame_count(), buf);
    img.w = PORTRAIT_W; img.h = PORTRAIT_H; img.pix = buf;
    img.uid = doomo_image_new_uid();
    return &img;
}
WEAK const DoomoImage *bodyspr_image(int resident_id, int dir, int frame)
{
    static uint8_t buf[BODYSPR_W * BODYSPR_H];
    static DoomoImage img;
    int status;
    const DoomoLook *l = stub_look_of(resident_id, &status);
    STUB("bodyspr_image");
    bodyspr_render(l, status, dir, frame, buf);
    img.w = BODYSPR_W; img.h = BODYSPR_H; img.pix = buf;
    img.uid = doomo_image_new_uid();
    return &img;
}
WEAK const DoomoImage *figure_image(const DoomoLook *l, int status, int expr)
{
    static uint8_t buf[FIGURE_W * FIGURE_H];
    static DoomoImage img;
    STUB("figure_image");
    figure_render(l, status, expr, (int)doomo_frame_count(), buf);
    img.w = FIGURE_W; img.h = FIGURE_H; img.pix = buf;
    img.uid = doomo_image_new_uid();
    return &img;
}
WEAK const DoomoImage *fighter_image(int resident_id, int view, int anim,
                                     int weapon_frame)
{
    static uint8_t buf[FIGHTER_W * FIGHTER_H];
    static DoomoImage img;
    int status;
    const DoomoLook *l = stub_look_of(resident_id, &status);
    (void)view; (void)weapon_frame;
    STUB("fighter_image");
    memset(buf, 0, sizeof(buf));
    sp_ball(buf, FIGHTER_W, FIGHTER_H, 16, 36 + (anim & 1), 9, 10,
            look_paint(l->top_color, 0), look_paint(l->top_color, 1),
            look_paint(l->top_color, 2));
    stub_face(l, status, EXPR_NEUTRAL, anim, buf, FIGHTER_W, FIGHTER_H, 16, 15, 11);
    img.w = FIGHTER_W; img.h = FIGHTER_H; img.pix = buf;
    img.uid = doomo_image_new_uid();
    return &img;
}
WEAK void portrait_invalidate(int resident_id) { (void)resident_id; }

/* ======================================================================
 * roster (roster)
 * ==================================================================== */
WEAK const DoomoResident *roster_premade(int id)
{
    (void)id;
    STUB("roster_premade");
    return NULL;
}
WEAK const char *roster_bio(int id) { (void)id; STUB("roster_bio"); return ""; }
WEAK const char *roster_job(int id) { (void)id; STUB("roster_job"); return ""; }
WEAK const DoomoEdge doomo_roster_edges[1] = { { 0, 0, 0, 0 } };
WEAK const uint16_t doomo_roster_edge_count = 0;
WEAK const uint16_t doomo_story_npc[NPC_COUNT] = { 0, 1, 2, 3, 4, 5, 500, 999 };

/* ======================================================================
 * life: sim / exorcism / pet
 * ==================================================================== */
WEAK const char *const doomo_mood_names[MOOD_COUNT] = {
    "Okay", "Happy", "Ecstatic", "Sad", "Angry", "Lovesick", "Hungry",
    "Sleepy", "Sick", "Filthy", "Lonely", "Scared", "Corrupted", "Possessed"
};
WEAK const char *const doomo_activity_names[ACT_COUNT] = {
    "At home", "Sleeping", "At the park", "At the cafe", "Shopping",
    "At church", "At the workshop", "At the beach", "At a concert",
    "At the vape shop", "At the hospital", "Wandering", "Haunting"
};
WEAK const char *const doomo_death_names[DEATH_COUNT] = {
    "", "Laid to rest", "Monster Mode", "Demon attack", "Neglect", "Old age"
};
WEAK void sim_init(void) { STUB("sim_init"); }
WEAK void sim_tick(void) { STUB("sim_tick"); }
WEAK void sim_advance_minutes(int minutes)
{
    STUB("sim_advance_minutes");
    while (minutes-- > 0) {
        if (++doomo_town.minute >= 1440) {
            doomo_town.minute = 0;
            doomo_town.day++;
            doomo_town.weekday = (uint8_t)((doomo_town.weekday + 1) % 7);
        }
    }
}
WEAK const char *sim_feed(int id, int item) { (void)id; (void)item; STUB("sim_feed"); return "Nom nom!"; }
WEAK const char *sim_gift(int id, int item) { (void)id; (void)item; STUB("sim_gift"); return "For me?!"; }
WEAK const char *sim_clean(int id) { (void)id; STUB("sim_clean"); return "Squeaky clean!"; }
WEAK const char *sim_play(int id) { (void)id; STUB("sim_play"); return "Wheee!"; }
WEAK const char *sim_talk(int id) { (void)id; STUB("sim_talk"); return "Hi hi!"; }
WEAK const char *sim_heal(int id, int item) { (void)id; (void)item; STUB("sim_heal"); return "I feel better!"; }
WEAK const char *sim_comfort(int id) { (void)id; STUB("sim_comfort"); return "Thank you..."; }
WEAK const char *sim_introduce(int a, int b) { (void)a; (void)b; STUB("sim_introduce"); return "Nice to meet you!"; }
WEAK const char *sim_advise_confess(int id, int target, int encourage)
{
    (void)id; (void)target; (void)encourage;
    STUB("sim_advise_confess");
    return "Okay... I'll think about it.";
}
WEAK const char *sim_make_up(int a, int b) { (void)a; (void)b; STUB("sim_make_up"); return "Sorry!"; }
WEAK int sim_problem_count(void) { STUB("sim_problem_count"); return 0; }
WEAK const DoomoProblem *sim_problem(int index) { (void)index; return NULL; }
WEAK void sim_problem_resolve(int index) { (void)index; }
WEAK int sim_problem_for(int resident_id) { (void)resident_id; return -1; }
WEAK int sim_news_count(void) { STUB("sim_news_count"); return 0; }
WEAK const char *sim_news(int index) { (void)index; return ""; }
WEAK void sim_news_push(const char *msg) { (void)msg; STUB("sim_news_push"); }
WEAK void sim_corrupt(int id, int amount)
{
    DoomoResident *r = town_res(id);
    STUB("sim_corrupt");
    if (!r || r->status != RES_ALIVE) return;
    amount += r->stat[STAT_CORRUPTION];
    r->stat[STAT_CORRUPTION] = (uint8_t)DOOMO_CLAMP(amount, 0, 100);
    if (r->stat[STAT_CORRUPTION] >= 100) r->status = RES_POSSESSED;
}
WEAK int sim_can_lay_to_rest(int id)
{
    DoomoResident *r = town_res(id);
    STUB("sim_can_lay_to_rest");
    if (!r) return 0;
    return r->status == RES_SOUL_LOST ||
           (opt_get(OPT_MONSTER_MODE) && (r->status == RES_ALIVE ||
                                          r->status == RES_POSSESSED));
}
WEAK void sim_lay_to_rest(int id, int cause)
{
    DoomoResident *r = town_res(id);
    STUB("sim_lay_to_rest");
    if (!r || id == DOOMO_PLAYER_ID) return;
    r->status = RES_DEAD;
    r->death_cause = (uint8_t)cause;
    if (cause == DEATH_MONSTER) r->flags |= RF_MONSTER_KILL;
    grave_add(id);
}
WEAK int sim_raise_ghost(int id)
{
    DoomoResident *r = town_res(id);
    STUB("sim_raise_ghost");
    if (!r || r->status != RES_DEAD) return 0;
    r->status = RES_GHOST;
    return 1;
}
WEAK int sim_mood_of(int id)
{
    DoomoResident *r = town_res(id);
    if (!r) return MOOD_OK;
    if (r->status == RES_POSSESSED) return MOOD_POSSESSED;
    return r->stat[STAT_HAPPY] >= 70 ? MOOD_HAPPY :
           r->stat[STAT_HAPPY] < 30 ? MOOD_SAD : MOOD_OK;
}

WEAK int exorcism_odds(int resident_id) { (void)resident_id; STUB("exorcism_odds"); return 50; }
WEAK void exorcism_apply(int resident_id, int success)
{
    DoomoResident *r = town_res(resident_id);
    int att, fails;
    STUB("exorcism_apply");
    if (!r) return;
    att = RES_EXO_ATTEMPTS(r); fails = RES_EXO_FAILS(r);
    if (att < 15) att++;
    if (success) {
        r->status = RES_ALIVE;
        r->stat[STAT_CORRUPTION] = 0;
    } else {
        if (fails < 15) fails++;
        if (fails >= DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL) r->status = RES_SOUL_LOST;
    }
    r->exorcism = (uint8_t)((att << 4) | fails);
}

WEAK const char *const doomo_pet_form_names[PET_FORM_COUNT] = {
    "Mochiblob", "Puffling", "Sprinkle", "Cupcakeling",
    "Halopup", "Seraphuff", "Choirbun", "Archangelo",
    "Scruffle", "Grumblin", "Feralfloof", "Wildbeastie",
    "Impkin", "Hornlet", "Hellmuffin", "Demogorgoo"
};
WEAK void pet_new(const char *name)
{
    DoomoPet *p = &doomo_town.pet;
    STUB("pet_new");
    memset(p, 0, sizeof(*p));
    if (name) {
        int i;
        for (i = 0; i < 7 && name[i]; i++) p->name[i] = name[i];
    }
    p->holiness = 50;
    p->hunger = p->joy = 4;
    p->weight = 5;
}
WEAK void pet_tick_minute(void) { STUB("pet_tick_minute"); }
WEAK void pet_feed(int snack) { (void)snack; STUB("pet_feed"); }
WEAK void pet_play_result(int won) { (void)won; STUB("pet_play_result"); }
WEAK void pet_clean(void) { STUB("pet_clean"); }
WEAK void pet_medicine(void) { STUB("pet_medicine"); }
WEAK void pet_discipline(void) { STUB("pet_discipline"); }
WEAK void pet_lights(int off) { (void)off; STUB("pet_lights"); }
WEAK int pet_line(void) { return PET_LINE_CUTE; }
WEAK int pet_combat_buff(void) { return 0; }
WEAK const DoomoImage *pet_image(int frame) { (void)frame; STUB("pet_image"); return &stub_blob; }

/* ======================================================================
 * action: map runtime, actors, welding
 * ==================================================================== */
WEAK DoomoMap doomo_map;
WEAK const DoomoMapDef *const doomo_maps[MAPID_COUNT];

/* A small pastel test room with a raised platform, steps and a pit, used
 * when no map registry exists yet. */
static void stub_room(DoomoMap *m)
{
    int x, y;
    memset(m, 0, sizeof(*m));
    m->w = 16; m->h = 16; m->kind = MAP_DUNGEON; m->sky = SKY_PASTEL_DAY;
    m->ambient = 12;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            DoomoCell *c = &m->cells[y * 16 + x];
            c->floor_tex = TEX_WOOD_FLOOR;
            c->ceil_tex = (x > 9) ? DOOMO_TEX_SKY : TEX_CLOUD_PUFF;
            c->floor_h = 0;
            c->ceil_h = 12;
            c->light = 13;
            if (x == 0 || y == 0 || x == 15 || y == 15) c->wall = TEX_PASTEL_BRICK;
            if (x >= 6 && x <= 8 && y >= 4 && y <= 6) c->floor_h = 3;
            if (x == 5 && y >= 4 && y <= 6) c->floor_h = 1;
            if (x >= 11 && x <= 12 && y >= 9 && y <= 10) c->floor_h = -2;
        }
}
WEAK void map_load(int map_id)
{
    const DoomoMapDef *d = (map_id >= 0 && map_id < MAPID_COUNT) ? doomo_maps[map_id] : NULL;
    STUB("map_load");
    if (!d) { stub_room(&doomo_map); return; }
    memset(&doomo_map, 0, sizeof(doomo_map));
    doomo_map.def = d;
    doomo_map.w = d->w; doomo_map.h = d->h;
    doomo_map.kind = d->kind; doomo_map.sky = d->sky; doomo_map.ambient = d->ambient;
    memcpy(doomo_map.cells, d->cells, (size_t)d->w * d->h * sizeof(DoomoCell));
}
WEAK const DoomoCell *map_cell_safe(const DoomoMap *m, int cx, int cy)
{
    static const DoomoCell solid = { 1, 1, 1, 0, 0, 8, 0, 0 };
    if (!m || cx < 0 || cy < 0 || cx >= m->w || cy >= m->h) return &solid;
    return &m->cells[cy * m->w + cx];
}
WEAK int map_floor_z(const DoomoMap *m, int cx, int cy)
{
    return map_cell_safe(m, cx, cy)->floor_h * DOOMO_HEIGHT_STEP;
}
WEAK int map_ceil_z(const DoomoMap *m, int cx, int cy)
{
    return map_cell_safe(m, cx, cy)->ceil_h * DOOMO_HEIGHT_STEP;
}
WEAK void map_tick(DoomoMap *m) { if (m) m->time++; }
WEAK void map_use(DoomoMap *m, int cx, int cy) { (void)m; (void)cx; (void)cy; STUB("map_use"); }

WEAK DoomoActor doomo_actors[ACTOR_MAX];
WEAK DoomoPlayerState doomo_player;
WEAK void combat_start(int map_id)
{
    STUB("combat_start");
    map_load(map_id);
    memset(&doomo_player, 0, sizeof(doomo_player));
    doomo_player.x = INT2FIX(3 * DOOMO_CELL_UNITS + 32);
    doomo_player.y = INT2FIX(8 * DOOMO_CELL_UNITS + 32);
    doomo_player.hp = 100;
    doomo_player.lock_target = -1;
}
WEAK void combat_tick(void) { STUB("combat_tick"); }
WEAK int combat_finished(void) { return 0; }
WEAK void combat_build_view(DoomoView3D *v)
{
    STUB("combat_build_view");
    memset(v, 0, sizeof(*v));
    v->map = &doomo_map;
    v->x = doomo_player.x; v->y = doomo_player.y;
    v->z = INT2FIX(map_floor_z(&doomo_map, FIX2INT(v->x) / DOOMO_CELL_UNITS,
                               FIX2INT(v->y) / DOOMO_CELL_UNITS) + DOOMO_EYE_HEIGHT);
    v->angle = doomo_player.angle;
    v->vw = (int16_t)plat_screen_w(); v->vh = (int16_t)plat_screen_h();
    v->player_sprite = -1;
}
WEAK void combat_draw_hud(void) { STUB("combat_draw_hud"); }

WEAK const char *const doomo_wframe_names[WFRAME_COUNT] = {
    "Pistol", "Revolver", "Dual Pistols", "Machine Pistol", "SMG",
    "Assault Rifle", "Battle Rifle", "Bullpup", "LMG", "Minigun", "DMR",
    "Sniper", "Railgun", "Crossbow", "Bow", "Pump Shotgun", "Super Shotgun",
    "Auto Shotgun", "Lever Shotgun", "Grenade Launcher", "Rocket Launcher",
    "Flamethrower", "Plasma Rifle", "Nailgun", "BFG", "Chainsaw", "Katana",
    "Sledgehammer", "Fists", "Bubble Wand", "Plush Launcher",
    "Confetti Cannon", "Lollipop Hammer", "Heart Bow", "Squeaky Mallet",
    "Glitter Sprayer", "Water Pistol", "Sticker Gun", "Yarn Crossbow"
};
WEAK const char *const doomo_wbarrel_names[WBARREL_COUNT] = {
    "Standard", "Short", "Long", "Heavy", "Double", "Triple", "Spiral",
    "Wide", "Sawed-off", "Suppressed", "Heart", "Star"
};
WEAK const char *const doomo_wcore_names[WCORE_COUNT] = {
    "Bullet", "AP Bullet", "Explosive", "Shell", "Slug", "Flechette",
    "Plasma", "Rocket", "Grenade", "Nail", "Rail Slug", "Fuel", "Bubble",
    "Glitter", "Holy Water", "Hellfire", "Ice", "Love", "Sawblade", "Slime",
    "Candy", "Plush"
};
WEAK const char *const doomo_wmod_names[WMOD_COUNT] = {
    "None", "Ext. Mag", "Spikes", "Bayonet", "Stickers", "Cooler",
    "Overclock", "Ricochet", "Homing", "Burst", "Full Auto", "Charge Shot",
    "Lifesteal", "Squeaker"
};
WEAK const char *const doomo_wsight_names[WSIGHT_COUNT] = {
    "Iron", "Red Dot", "Holo", "4x Scope", "8x Scope", "Thermal",
    "Heart Dot", "Laser"
};
WEAK const char *const doomo_wstock_names[WSTOCK_COUNT] = {
    "None", "Fixed", "Folding", "Heavy", "Skeleton", "Plush"
};
WEAK const char *const doomo_wmag_names[WMAG_COUNT] = {
    "Standard", "Extended", "Drum", "Quick", "Belt", "Candy Tube"
};
WEAK const char *const doomo_wunder_names[WUNDER_COUNT] = {
    "None", "Grip", "Grenade", "Shotgun", "Flashlight", "Chainsaw", "Bubble"
};
WEAK const char *const doomo_wcharm_names[WCHARM_COUNT] = {
    "None", "Teddy", "Skull", "Dice", "Heart", "Eyeball", "Bell", "Rosary"
};
WEAK void weld_stats(const DoomoWeapon *w, DoomoWeaponStats *out)
{
    STUB("weld_stats");
    memset(out, 0, sizeof(*out));
    out->damage = (int16_t)(10 + (w ? w->quality / 10 : 0));
    out->pellets = 1;
    out->spread = 8;
    out->refire = 8;
    out->ammo_type = AMMO_BULLETS;
    out->ammo_use = 1;
    out->range = 32;
    out->mag_size = 12;
    out->reload = 30;
    out->recoil = 6;
    out->sfx = SFX_PISTOL;
}
WEAK void weld_name(const DoomoWeapon *w, char *buf, int cap)
{
    const char *n = (w && w->frame < WFRAME_COUNT) ? doomo_wframe_names[w->frame] : "Mystery Gun";
    STUB("weld_name");
    doomo_snprintf(buf, cap, "%s %s", (w && w->style == WSTYLE_BRUTAL) ? "Brutal" : "Cute", n);
}
WEAK int weld_cost(const DoomoWeapon *w, uint8_t *items, uint8_t *counts, int max)
{
    (void)w; (void)items; (void)counts; (void)max;
    STUB("weld_cost");
    return 0;
}
WEAK const DoomoImage *weld_view_image(const DoomoWeapon *w, int frame)
{
    (void)w; (void)frame;
    STUB("weld_view_image");
    return &stub_blob;
}
WEAK const DoomoImage *weld_icon_image(const DoomoWeapon *w)
{
    (void)w;
    STUB("weld_icon_image");
    return &stub_blob;
}

/* ======================================================================
 * story + audio (story)
 * ==================================================================== */
WEAK int story_flag(int f)
{
    if (f < 0 || f >= DOOMO_STORY_FLAGS) return 0;
    return (doomo_town.story.flags[f >> 3] >> (f & 7)) & 1;
}
WEAK void story_set_flag(int f, int on)
{
    if (f < 0 || f >= DOOMO_STORY_FLAGS) return;
    if (on) doomo_town.story.flags[f >> 3] |= (uint8_t)(1 << (f & 7));
    else    doomo_town.story.flags[f >> 3] &= (uint8_t)~(1 << (f & 7));
}
WEAK void story_new_game(void) { STUB("story_new_game"); }
WEAK void story_interact_npc(int resident_id) { (void)resident_id; STUB("story_interact_npc"); }
WEAK void story_trigger(int tag) { (void)tag; STUB("story_trigger"); }
WEAK const char *story_objective(void) { STUB("story_objective"); return ""; }

WEAK void audio_init(void) { STUB("audio_init"); }
WEAK void audio_tick(void) { }
WEAK void audio_music(int song) { (void)song; STUB("audio_music"); }
WEAK void audio_sfx(int sfx) { (void)sfx; STUB("audio_sfx"); }
WEAK const int8_t *audio_sfx_pcm(int sfx, uint32_t *len, uint32_t *rate)
{
    static const int8_t blip[16] = { 0, 40, 60, 40, 0, -40, -60, -40,
                                     0, 20, 30, 20, 0, -20, -30, -20 };
    (void)sfx;
    STUB("audio_sfx_pcm");
    if (len) *len = sizeof(blip);
    if (rate) *rate = 8000;
    return blip;
}
WEAK void audio_voice_char(const uint8_t *voice, char c) { (void)voice; (void)c; }

/* ======================================================================
 * per-team unlock tables (each team defines its own, strong)
 * ==================================================================== */
#define EMPTY_UNLOCKS(name) WEAK const DoomoUnlock name[1] = { { 0, 0, 0, 0, NULL, NULL, 0, 0 } }
EMPTY_UNLOCKS(doomo_unlocks_creator);
EMPTY_UNLOCKS(doomo_unlocks_life);
EMPTY_UNLOCKS(doomo_unlocks_action);
EMPTY_UNLOCKS(doomo_unlocks_story);
EMPTY_UNLOCKS(doomo_unlocks_freeplay);
EMPTY_UNLOCKS(doomo_unlocks_art);
EMPTY_UNLOCKS(doomo_unlocks_portrait);
EMPTY_UNLOCKS(doomo_unlocks_roster);

/* ======================================================================
 * scenes owned by other modules
 * ==================================================================== */
static void stub_scene_enter(int arg)
{
    static uint32_t logged;                 /* one bit per registry slot */
    const DoomoScene *s = scene_top();
    int i;
    (void)arg;
    for (i = 0; doomo_all_scenes[i]; i++) {
        if (doomo_all_scenes[i] == s) {
            if (i < 32 && !(logged & (1u << i))) {
                char buf[48];
                logged |= 1u << i;
                doomo_snprintf(buf, sizeof(buf), "STUB: SCENE_%s", s->name);
                plat_log(buf);
            }
            break;
        }
    }
}

static void stub_scene_update(void)
{
    if (btn_pressed(BTN_B | BTN_START)) {
        if (scene_depth() > 1) scene_pop();
        else if (scene_top() != &SCENE_TITLE) scene_set(&SCENE_TITLE, 0);
    }
}

static void stub_scene_draw(void)
{
    const DoomoScene *s = scene_top();
    int w = plat_screen_w(), h = plat_screen_h();
    int t = (int)scene_ticks();
    int ww = DOOMO_MIN(w - 24, 200), wh = 72;
    int x = (w - ww) / 2, y = (h - wh) / 2 - 6;
    ui_backdrop(WIN_MINT, (uint32_t)t);
    if (ui_window_pop(x, y, ww, wh, WIN_CREAM, t)) {
        const UiStyle *st = ui_style(WIN_CREAM);
        ui_title(w / 2, y - 6, "UNDER CONSTRUCTION", WIN_MINT);
        ui_text_center(w / 2, y + 16, s ? s->name : "?", st->accent_d);
        ui_text_wrap(x + 10, y + 30, ww - 20,
                     "Tiny builder critters are still hammering this screen "
                     "together. Come back soon!", st->text, 1);
        ui_sparkle(x + ww - 10, y + 10, t, PAL_UI_GOLD);
        ui_sparkle(x + 10, y + wh - 10, t + 7, PAL_UI_GOLD);
    }
    ui_hint(STR_ICON_B " Back", WIN_MINT);
}

#define STUB_SCENE(var, nm) \
    WEAK const DoomoScene var = { nm, stub_scene_enter, NULL, \
                                  stub_scene_update, stub_scene_draw, 0 }

/* The stub title screen is a launcher for every scene (debug), so the
 * whole game can be explored while modules land. */
static UiList stub_title_list;
static const char *stub_title_item(int index, char *buf)
{
    int i, n = 0;
    for (i = 0; doomo_all_scenes[i]; i++) {
        if (doomo_all_scenes[i] == &SCENE_TITLE || doomo_all_scenes[i] == &SCENE_BOOT)
            continue;
        if (n++ == index) {
            doomo_snprintf(buf, 48, "%s", doomo_all_scenes[i]->name);
            return buf;
        }
    }
    return "";
}
static const DoomoScene *stub_title_scene(int index)
{
    int i, n = 0;
    for (i = 0; doomo_all_scenes[i]; i++) {
        if (doomo_all_scenes[i] == &SCENE_TITLE || doomo_all_scenes[i] == &SCENE_BOOT)
            continue;
        if (n++ == index) return doomo_all_scenes[i];
    }
    return NULL;
}
static void stub_title_enter(int arg)
{
    int i, n = 0;
    stub_scene_enter(arg);
    for (i = 0; doomo_all_scenes[i]; i++)
        if (doomo_all_scenes[i] != &SCENE_TITLE && doomo_all_scenes[i] != &SCENE_BOOT) n++;
    memset(&stub_title_list, 0, sizeof(stub_title_list));
    stub_title_list.item_fn = stub_title_item;
    stub_title_list.count = n;
    stub_title_list.visible = plat_screen_h() >= 200 ? 11 : 6;
}
static void stub_title_update(void)
{
    if (ui_list_update(&stub_title_list) == UI_SELECT) {
        const DoomoScene *s = stub_title_scene(stub_title_list.sel);
        if (s) scene_push(s, s == &SCENE_CREATOR ? -1 : 0);
    }
}
static void stub_title_draw(void)
{
    int w = plat_screen_w(), h = plat_screen_h();
    int t = (int)scene_ticks();
    int lw = 132, lh = stub_title_list.visible * UI_LIST_ROW_H + 8;
    int scale = w >= 300 ? 3 : 2;
    int ty = 8 + ui_wave(t, 60, 2);
    const char *l1 = "DOOMOGACHI";
    ui_backdrop(WIN_CUTE, (uint32_t)t);
    ui_text_big((w - ui_text_big_width(l1, scale)) / 2, ty, l1, scale,
                PAL_WHITE, PAL_UI_PINK_D);
    ui_text_center(w / 2, ty + 8 * scale + 4, "LIFE in HELLvile Park!", PAL_UI_PINK_D);
    ui_heart(w / 2 - ui_text_big_width(l1, scale) / 2 - 10, ty + 4 * scale, 4 + (t / 8) % 2,
             PAL_PAINT(PAINT_ROSE, 1));
    ui_window(w / 2 - lw / 2, h - lh - 18, lw, lh, WIN_CUTE);
    ui_list_draw(&stub_title_list, w / 2 - lw / 2 + 4, h - lh - 14, lw - 8, WIN_CUTE);
    ui_hint(STR_ICON_A " Open  (scene launcher)", WIN_CUTE);
}

WEAK const DoomoScene SCENE_TITLE = { "TITLE", stub_title_enter, NULL,
                                      stub_title_update, stub_title_draw, 0 };
STUB_SCENE(SCENE_NEW_ISLAND, "NEW_ISLAND");
STUB_SCENE(SCENE_ISLAND, "ISLAND");
STUB_SCENE(SCENE_APARTMENTS, "APARTMENTS");
STUB_SCENE(SCENE_ROSTER, "ROSTER");
STUB_SCENE(SCENE_RESIDENT, "RESIDENT");
STUB_SCENE(SCENE_SHOP, "SHOP");
STUB_SCENE(SCENE_GRAVEYARD, "GRAVEYARD");
STUB_SCENE(SCENE_NEWS, "NEWS");
STUB_SCENE(SCENE_OPTIONS, "OPTIONS");
STUB_SCENE(SCENE_SAVELOAD, "SAVELOAD");
STUB_SCENE(SCENE_CREATOR, "CREATOR");
STUB_SCENE(SCENE_EXORCISM, "EXORCISM");
STUB_SCENE(SCENE_PET, "PET");
STUB_SCENE(SCENE_COMBAT, "COMBAT");
STUB_SCENE(SCENE_WORKSHOP, "WORKSHOP");
STUB_SCENE(SCENE_WELD_GAME, "WELD_GAME");
STUB_SCENE(SCENE_ARENA, "ARENA");
STUB_SCENE(SCENE_STORY, "STORY");
STUB_SCENE(SCENE_QUESTLOG, "QUESTLOG");
STUB_SCENE(SCENE_CUTSCENE, "CUTSCENE");
STUB_SCENE(SCENE_CREDITS, "CREDITS");
