/* stubs.c - weak placeholders for every cross-module symbol (owner:
 * foundation).
 *
 * Every function, table and scene declared in a header that foundation
 * does not implement gets a __attribute__((weak)) definition here, so the
 * game always links and runs while the other modules land. The owners'
 * strong definitions replace these at link time (objects are linked
 * directly, never through archives - see core/sources.mk).
 *
 * Not stubbed on purpose: art's *internal* generated tables
 * (doomo_art_tex/_tex_pix/_sky/_sky_pix/_spr/_frames ...). They only exist
 * to back art's own textures.c/sprites.c, ship together with them, and a
 * zero-filled placeholder would turn a clear link error into a crash.
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
/* All-zero placeholders for the generated tables; doomo_init() notices the
 * broken identity row (a bring-up build calls doomo_palette_fallback()). */
WEAK const uint8_t doomo_art_colormap[DOOMO_LIGHT_LEVELS][256];
WEAK const uint8_t doomo_art_tintmap[TINT_COUNT][256];
WEAK const uint8_t (*doomo_colormap)[256] = doomo_art_colormap;
WEAK const uint8_t (*doomo_tintmap)[256] = doomo_art_tintmap;
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

/* Not weak: foundation's runtime fallback for missing generated tables:
 * builds them in RAM and points doomo_colormap / doomo_tintmap there.
 * Nothing in the game references it (the art is generated), so linkers
 * with --gc-sections drop it and its 5.6 KB of tables. */
void doomo_palette_fallback(void)
{
    static const uint8_t tint_rgb[TINT_COUNT][3] = {
        { 0, 0, 0 }, { 255, 40, 40 }, { 255, 220, 80 }, { 80, 255, 80 },
        { 180, 80, 255 }, { 40, 50, 140 }
    };
    static uint8_t cm[DOOMO_LIGHT_LEVELS][256], tm[TINT_COUNT][256];
    int lv, i;
    for (i = 0; i < 256; i++) {
        uint8_t r, g, b;
        palette_rgb((uint8_t)i, &r, &g, &b);
        for (lv = 0; lv < DOOMO_LIGHT_LEVELS; lv++) {
            if (i == 0) { cm[lv][i] = 0; continue; }
            if (lv == DOOMO_LIGHT_LEVELS - 1) { cm[lv][i] = (uint8_t)i; continue; }
            cm[lv][i] = palette_nearest(r * lv / 15, g * lv / 15, b * lv / 15);
        }
        for (lv = 0; lv < TINT_COUNT; lv++) {
            if (i == 0 || lv == TINT_NONE) { tm[lv][i] = (uint8_t)i; continue; }
            tm[lv][i] = palette_nearest((r + tint_rgb[lv][0]) / 2, (g + tint_rgb[lv][1]) / 2,
                                        (b + tint_rgb[lv][2]) / 2);
        }
    }
    doomo_colormap = (const uint8_t (*)[256])cm;
    doomo_tintmap = (const uint8_t (*)[256])tm;
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
/* (integration) the expansion teams' tables too, so a module that is not
 * linked (e.g. still being written) never breaks the link; ranges in
 * doomo/unlock_ids.h */
EMPTY_UNLOCKS(doomo_unlocks_minigames);
EMPTY_UNLOCKS(doomo_unlocks_concert);
EMPTY_UNLOCKS(doomo_unlocks_studio);
EMPTY_UNLOCKS(doomo_unlocks_library);
EMPTY_UNLOCKS(doomo_unlocks_fishcook);
EMPTY_UNLOCKS(doomo_unlocks_levels);
EMPTY_UNLOCKS(doomo_unlocks_home);      /* S0: the dream house (H1) */

/* ======================================================================
 * scenes owned by other modules
 * ==================================================================== */
static void stub_scene_enter(int arg)
{
    /* one bit per registry slot; (integration) was a uint32_t, so the
     * 33rd+ scenes (minigames, concert, library, fishing...) never logged
     * "STUB:" and --assert-no-stubs could not catch them */
    static uint8_t logged[16];
    const DoomoScene *s = scene_top();
    int i;
    (void)arg;
    for (i = 0; doomo_all_scenes[i]; i++) {
        if (doomo_all_scenes[i] == s) {
            if (i >= (int)sizeof(logged) * 8 || !(logged[i >> 3] & (1u << (i & 7)))) {
                char buf[48];
                if (i < (int)sizeof(logged) * 8) logged[i >> 3] |= (uint8_t)(1u << (i & 7));
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
        ui_text_outline(w / 2 - font_text_width(s ? s->name : "?") / 2, y + 13,
                        s ? s->name : "?", st->accent, PAL_WHITE);
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
        if (doomo_all_scenes[i] == &SCENE_TITLE || doomo_all_scenes[i] == &SCENE_BOOT ||
            doomo_all_scenes[i]->overlay)
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
        if (doomo_all_scenes[i] == &SCENE_TITLE || doomo_all_scenes[i] == &SCENE_BOOT ||
            doomo_all_scenes[i]->overlay)
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
        if (doomo_all_scenes[i] != &SCENE_TITLE && doomo_all_scenes[i] != &SCENE_BOOT &&
            !doomo_all_scenes[i]->overlay) n++;
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
                PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_RAMP(RAMP_PASTEL_PINK, 1));
    ui_text_outline(w / 2 - font_text_width("LIFE in HELLvile Park!") / 2, ty + 8 * scale + 5,
                    "LIFE in HELLvile Park!", PAL_RAMP(RAMP_PASTEL_PINK, 1), PAL_WHITE);
    ui_heart(w / 2 - ui_text_big_width(l1, scale) / 2 - 10, ty + 4 * scale, 4 + (t / 8) % 2,
             PAL_PAINT(PAINT_ROSE, 1));
    ui_window(w / 2 - lw / 2, h - lh - 18, lw, lh, WIN_CUTE);
    ui_list_draw(&stub_title_list, w / 2 - lw / 2 + 4, h - lh - 14, lw - 8, WIN_CUTE);
    ui_hint(STR_ICON_A " Open  (scene launcher)", WIN_CUTE);
}

WEAK const DoomoScene SCENE_TITLE = { "TITLE", stub_title_enter, NULL,
                                      stub_title_update, stub_title_draw, 0 };

/* (integration) 1 if `s` is still one of the weak placeholder scenes (no
 * module provides it). Tests use it instead of entering every scene. */
int scene_is_stub(const DoomoScene *s)
{
    return s && ((s->enter == stub_scene_enter && s->update == stub_scene_update) ||
                 s->enter == stub_title_enter);
}
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
/* (integration) expansion scenes listed in scenes.h / doomo_all_scenes */
STUB_SCENE(SCENE_UNLOCKS, "UNLOCKS");
STUB_SCENE(SCENE_PICKER, "PICKER");
STUB_SCENE(SCENE_MG_CATCH, "MG_CATCH");
STUB_SCENE(SCENE_MG_WHACK, "MG_WHACK");
STUB_SCENE(SCENE_MG_DODGE, "MG_DODGE");
STUB_SCENE(SCENE_MG_GALLERY, "MG_GALLERY");
STUB_SCENE(SCENE_MG_DERBY, "MG_DERBY");
STUB_SCENE(SCENE_MG_MEMORY, "MG_MEMORY");
STUB_SCENE(SCENE_MG_WHEEL, "MG_WHEEL");
STUB_SCENE(SCENE_CONCERT, "CONCERT");
STUB_SCENE(SCENE_DREAM, "DREAM");
STUB_SCENE(SCENE_STUDIO, "STUDIO");
STUB_SCENE(SCENE_LIBRARY, "LIBRARY");
STUB_SCENE(SCENE_LIB_BESTIARY, "LIB_BESTIARY");
STUB_SCENE(SCENE_LIB_ALMANAC, "LIB_ALMANAC");
STUB_SCENE(SCENE_LIB_WEAPONS, "LIB_WEAPONS");
STUB_SCENE(SCENE_LIB_ITEMS, "LIB_ITEMS");
STUB_SCENE(SCENE_LIB_TROPHIES, "LIB_TROPHIES");
STUB_SCENE(SCENE_LIB_JUKEBOX, "LIB_JUKEBOX");
STUB_SCENE(SCENE_LIB_GALLERY, "LIB_GALLERY");
STUB_SCENE(SCENE_FISHING, "FISHING");
STUB_SCENE(SCENE_COOKING, "COOKING");
STUB_SCENE(SCENE_LEVEL_SELECT, "LEVEL_SELECT");
STUB_SCENE(SCENE_BREACH, "BREACH");
STUB_SCENE(SCENE_ARENA_MODE, "ARENA_MODE");

/* ======================================================================
 * --- S0: story, comic timing and dream house contracts ---------------
 * (docs/design/story/SPEC.md sec.4; append-only block, one per spec). Weak
 * placeholders for every symbol the S0 headers declare, so the game links
 * while S1/S2/S3/S5/H1/H4 land. Each logs "STUB: <name>" once and returns
 * a neutral value; nothing in the shipped game calls them before their
 * owner lands. I9 deletes the ones that have strong definitions.
 * ==================================================================== */
#include "doomo/tween.h"
#include "doomo/beat.h"
#include "doomo/camfx.h"
#include "doomo/story_vm.h"
#include "doomo/txt.h"
#include "doomo/home.h"
#include "doomo/house_api.h"

/* ---- tween.h (S1: tween.c, tween_tables.c) ---- */
WEAK fix16 tw_t(int age, int dur)
{
    STUB("tw_t");
    if (dur <= 0 || age >= dur) return FIX_ONE;
    return age <= 0 ? 0 : (fix16)(((int64_t)age << 16) / dur);
}
WEAK fix16 tw_smooth(fix16 t) { STUB("tw_smooth"); return t; }
WEAK fix16 tw_in2(fix16 t) { STUB("tw_in2"); return t; }
WEAK fix16 tw_out2(fix16 t) { STUB("tw_out2"); return t; }
WEAK fix16 tw_back(fix16 t, fix16 s) { (void)s; STUB("tw_back"); return t; }
WEAK fix16 tw_bounce(fix16 t) { STUB("tw_bounce"); return t; }
WEAK int tw_lerp(int a, int b, fix16 t)
{
    STUB("tw_lerp");
    return a + (int)(((int64_t)(b - a) * t) >> 16);
}
WEAK int tw_tab(const TwTab *t, int age)
{
    STUB("tw_tab");
    if (!t || !t->v || !t->n) return 0;
    return t->v[age < 0 ? 0 : age >= t->n ? t->n - 1 : age];
}
WEAK TwSquash tw_squash(const TwTab *sx, const TwTab *sy, const TwTab *dy, int age, int amount256)
{
    TwSquash q;
    (void)sx; (void)sy; (void)dy; (void)age; (void)amount256;
    STUB("tw_squash");
    q.sx = 256; q.sy = 256; q.dy = 0;
    return q;
}
WEAK const TwTab *tw_table(int id) { (void)id; STUB("tw_table"); return NULL; }
#define EMPTY_TWTAB(n) WEAK const TwTab n = { NULL, 0 }
EMPTY_TWTAB(TW_POP_BUBBLE); EMPTY_TWTAB(TW_POP_LETTER); EMPTY_TWTAB(TW_POP_SOUL);
EMPTY_TWTAB(TW_POP_LOCKUP); EMPTY_TWTAB(TW_POP_EYE); EMPTY_TWTAB(TW_POP_MENU);
EMPTY_TWTAB(TW_POP_BLOB); EMPTY_TWTAB(TW_POP_BURST);
EMPTY_TWTAB(TW_SLAM_STAMP); EMPTY_TWTAB(TW_SLAM_COUNTER); EMPTY_TWTAB(TW_SLAM_STICKER);
EMPTY_TWTAB(TW_SLAM_STICK2);
EMPTY_TWTAB(TW_DROP_TITLE_SX); EMPTY_TWTAB(TW_DROP_TITLE_SY); EMPTY_TWTAB(TW_DROP_TITLE_DY);
EMPTY_TWTAB(TW_DROP_LET_SX); EMPTY_TWTAB(TW_DROP_LET_SY); EMPTY_TWTAB(TW_DROP_LET_DY);
EMPTY_TWTAB(TW_DROP_SOFT_SX); EMPTY_TWTAB(TW_DROP_SOFT_SY); EMPTY_TWTAB(TW_DROP_SOFT_DY);
EMPTY_TWTAB(TW_HEART_LAND_SQ); EMPTY_TWTAB(TW_DEVICE_DY); EMPTY_TWTAB(TW_SIGN_DY);
EMPTY_TWTAB(TW_CLUNK); EMPTY_TWTAB(TW_CLAP); EMPTY_TWTAB(TW_WINDOW_BOING_DY);
EMPTY_TWTAB(TW_BRAKE_DX); EMPTY_TWTAB(TW_ZOOM_WOBBLE_DY);
EMPTY_TWTAB(TW_HOP1); EMPTY_TWTAB(TW_HOP2); EMPTY_TWTAB(TW_TAG_DY); EMPTY_TWTAB(TW_DEADPAN_DY);
WEAK void tw_spring_init(TwSpring *s, int preset, int32_t pos_q4)
{
    (void)preset;
    STUB("tw_spring_init");
    if (s) { memset(s, 0, sizeof(*s)); s->e = 0; s->target = pos_q4; }
}
WEAK void tw_spring_target(TwSpring *s, int32_t target_q4)
{
    STUB("tw_spring_target");
    if (s) s->target = target_q4;
}
WEAK void tw_spring_punch(TwSpring *s, int32_t d_q4) { (void)s; (void)d_q4; STUB("tw_spring_punch"); }
WEAK void tw_spring_shove(TwSpring *s, int32_t dv_q4) { (void)s; (void)dv_q4; STUB("tw_spring_shove"); }
WEAK int32_t tw_spring_step(TwSpring *s) { STUB("tw_spring_step"); return s ? s->target : 0; }

/* ---- beat.h (S1: beat.c) ---- */
WEAK void beat_start(const BeatScript *s) { (void)s; STUB("beat_start"); }
WEAK int  beat_tick(void) { STUB("beat_tick"); return BT_DONE; }
WEAK const BeatEv *beat_ext_event(void) { STUB("beat_ext_event"); return NULL; }
WEAK int  beat_running(void) { return 0; }
WEAK int  beat_age(int label) { (void)label; STUB("beat_age"); return -1; }
WEAK int  beat_clock(int actor) { (void)actor; STUB("beat_clock"); return (int)plat_ticks(); }
WEAK void beat_skip(void) { STUB("beat_skip"); }
WEAK void beat_press_a(void) { STUB("beat_press_a"); }
WEAK void beat_set_actor_clock(int actor, int mode) { (void)actor; (void)mode; STUB("beat_set_actor_clock"); }

/* ---- camfx.h (S1: camfx.c) ---- */
WEAK void camfx_reset(void) { STUB("camfx_reset"); }
WEAK void camfx_shake(int profile) { (void)profile; STUB("camfx_shake"); }
WEAK void camfx_punch(int dx, int dy) { (void)dx; (void)dy; STUB("camfx_punch"); }
WEAK void camfx_zoom_snap(int scale_q8, int frames) { (void)scale_q8; (void)frames; STUB("camfx_zoom_snap"); }
WEAK void camfx_flash(uint8_t color, int level16, int frames)
{
    (void)color; (void)level16; (void)frames;
    STUB("camfx_flash");
}
WEAK void camfx_fade(int level16) { (void)level16; STUB("camfx_fade"); }
WEAK void camfx_iris(int cx, int cy, int r, int heart, uint8_t rim)
{
    (void)cx; (void)cy; (void)r; (void)heart; (void)rim;
    STUB("camfx_iris");
}
WEAK void camfx_iris_off(void) { STUB("camfx_iris_off"); }
WEAK void camfx_pal_lerp(int pal_id, int level16) { (void)pal_id; (void)level16; STUB("camfx_pal_lerp"); }
WEAK void camfx_tick(void) { }
WEAK void camfx_offset(int *dx, int *dy) { if (dx) *dx = 0; if (dy) *dy = 0; }
WEAK void camfx_apply(void) { }

/* ---- story_vm.h (S2: scriptvm.c) ---- */
WEAK void vm_start(int script_id) { (void)script_id; STUB("vm_start"); }
WEAK int  vm_tick(void) { return VM_IDLE; }
WEAK void vm_resume(int result) { (void)result; STUB("vm_resume"); }
WEAK const StoryOp *vm_ext_op(void) { return NULL; }
WEAK int  vm_running(void) { return 0; }
WEAK int  vm_var(int i) { (void)i; STUB("vm_var"); return 0; }
WEAK void vm_set_var(int i, int v) { (void)i; (void)v; STUB("vm_set_var"); }
WEAK uint32_t story_rng(uint32_t ctx)
{
    uint32_t x = doomo_town.seed ^ (ctx * 0x9E3779B9u) ^ 0x5EED5EEDu;
    STUB("story_rng");
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x;
}
WEAK uint32_t story_fingerprint(void) { STUB("story_fingerprint"); return 0; }

/* The story_mg registry (C14): weak NULL tables (story_mg_a.c S4,
 * story_mg_b.c S5) and its reader. The reader is a functional default
 * (S2 may keep it); the tables are read through a volatile pointer because
 * GCC would otherwise fold this file's weak NULL constants. */
WEAK const StoryMgEntry *const doomo_story_mg_a = 0;
WEAK const StoryMgEntry *const doomo_story_mg_b = 0;
#define MG_TABLE(p) (*(const StoryMgEntry *const volatile *)&(p))
static const StoryMgEntry *story_mg_table(int t)
{
    return t == 0 ? MG_TABLE(doomo_story_mg_a) : t == 1 ? MG_TABLE(doomo_story_mg_b) : NULL;
}
WEAK int story_mg_count(void)
{
    int t, n = 0;
    for (t = 0; t < 2; t++) {
        const StoryMgEntry *e = story_mg_table(t);
        while (e && e->name) { n++; e++; }
    }
    return n;
}
WEAK const StoryMgEntry *story_mg_at(int i)
{
    int t;
    if (i < 0) return NULL;
    for (t = 0; t < 2; t++) {
        const StoryMgEntry *e = story_mg_table(t);
        for (; e && e->name; e++) if (i-- == 0) return e;
    }
    return NULL;
}
WEAK const StoryMgEntry *story_mg_find(int id)
{
    int i, n = story_mg_count();
    for (i = 0; i < n; i++) {
        const StoryMgEntry *e = story_mg_at(i);
        if (e && e->id == id) return e;
    }
    return NULL;
}

/* ---- txt.h (S2: txt.c) ---- */
WEAK int txt_get(uint16_t id, char *out, int cap)
{
    (void)id;
    STUB("txt_get");
    if (out && cap > 0) out[0] = 0;
    return -1;
}
WEAK int txt_pack_load(int pack) { (void)pack; STUB("txt_pack_load"); return 0; }
WEAK int txt_pack_is_live(int pack) { (void)pack; return 0; }
WEAK void txt_stash(int slot, uint16_t id) { (void)slot; (void)id; STUB("txt_stash"); }
WEAK const char *txt_stash_get(int slot) { (void)slot; STUB("txt_stash_get"); return ""; }

/* ---- furn.h (H4: core/gen/furn_data.c) ---- */
WEAK const DoomoFurn doomo_furn[1] = { { NULL, NULL, 0, 0xFFFF, 0xFFFF, 0, 0, 0, 0,
                                          0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0xFF, 0, 0,
                                          THEME_NONE, 0, 0, 0, 0, 0 } };
WEAK const uint16_t doomo_furn_count = 0;
WEAK const char *doomo_furn_text(const char *p, char *buf, int cap) { (void)buf; (void)cap; return p ? p : ""; }

/* ---- house_api.h (H1: house_api.c) ---- */
WEAK int  home_grade(int room) { (void)room; STUB("home_grade"); return HG_NONE; }
WEAK int  home_stat(int stat) { (void)stat; STUB("home_stat"); return 0; }
WEAK int  home_grant(int furn) { (void)furn; STUB("home_grant"); return 0; }
WEAK int  home_has(int furn) { (void)furn; STUB("home_has"); return 0; }
WEAK int  home_place_gift(int furn) { (void)furn; STUB("home_place_gift"); return 0; }
WEAK void home_remove_only(int on) { (void)on; STUB("home_remove_only"); }
WEAK int  home_goal_check(int goal) { (void)goal; STUB("home_goal_check"); return 0; }
WEAK int  home_decorate_for(int layout, int tol) { (void)layout; (void)tol; STUB("home_decorate_for"); return 0; }
WEAK void home_haunt_step(int night) { (void)night; STUB("home_haunt_step"); }
WEAK int  home_defend_result(void) { STUB("home_defend_result"); return 0; }
WEAK int  home_rivet_visits(void) { STUB("home_rivet_visits"); return 0; }
WEAK int  home_days_since_fire(void) { STUB("home_days_since_fire"); return 0; }
WEAK void home_set_phase(int phase) { (void)phase; STUB("home_set_phase"); }
WEAK int  home_phase(void) { STUB("home_phase"); return HP_COSY; }
WEAK int  home_prop_gag(int gag) { (void)gag; STUB("home_prop_gag"); return 0; }
WEAK int  home_moved_in(void) { STUB("home_moved_in"); return 0; }

/* ---- platform.h: the room renderer (H2/H3; native reference room_ref.c,
 * H4, replaces this stub - delete it then) ---- */
WEAK void plat_room(const DoomoRoomView *v) { (void)v; STUB("plat_room"); }

/* ---- audio.h hooks (S1: audio_fx.c) ---- */
WEAK void audio_music_mute(int frames) { (void)frames; STUB("audio_music_mute"); }
WEAK void audio_music_duck(int gain_q8, int frames) { (void)gain_q8; (void)frames; STUB("audio_music_duck"); }
WEAK void audio_tapestop(int frames) { (void)frames; STUB("audio_tapestop"); }
WEAK void audio_music_resume_on_beat(void) { STUB("audio_music_resume_on_beat"); }
WEAK void audio_music_dual(int song_a, int song_b)
{
    (void)song_b;
    STUB("audio_music_dual");
    audio_music(song_a);
}
WEAK void audio_sfx_pitch(int sfx, int semis) { (void)semis; STUB("audio_sfx_pitch"); audio_sfx(sfx); }
WEAK void audio_sfx_synth(int sfx) { (void)sfx; STUB("audio_sfx_synth"); }
WEAK const uint8_t *audio_voice_preset(int preset) { (void)preset; STUB("audio_voice_preset"); return NULL; }

/* The 9 new songs' MML (R16, C15): silent placeholders, four channels each
 * (16 quiet eighths on the lead so the sequencer, the jukebox and the song
 * tests treat them as real songs). Owners define core/src/mml_<name>.c. */
#define SONG_STUB(n) \
    WEAK const char doomo_mml_##n##_0[] = "t120 v0 l8 [c]16"; \
    WEAK const char doomo_mml_##n##_1[] = ""; \
    WEAK const char doomo_mml_##n##_2[] = ""; \
    WEAK const char doomo_mml_##n##_3[] = ""
SONG_STUB(mingle); SONG_STUB(wall_lullaby); SONG_STUB(two_choirs);
SONG_STUB(boat_floats); SONG_STUB(thirteen_bells); SONG_STUB(hell_lullaby);
SONG_STUB(double_act); SONG_STUB(heehee_jingle); SONG_STUB(number_ten);

/* ---- story.h additions (S2 stry_save.c, S3 exorcism.c, S5 story_grumble.c) ---- */
WEAK DoomoStry *story_stry(void)
{
    static DoomoStry zero;
    STUB("story_stry");
    return &zero;
}
WEAK int story_episode_done(int beat) { (void)beat; STUB("story_episode_done"); return 0; }
WEAK int story_grumble_dishes(void) { STUB("story_grumble_dishes"); return 0; }
WEAK int story_grumble_pacifist(void) { STUB("story_grumble_pacifist"); return 0; }
WEAK int story_grumble_exes_met(void) { STUB("story_grumble_exes_met"); return 0; }
WEAK int story_grumble_dead(void) { STUB("story_grumble_dead"); return story_flag(SF_GRUMBLE_DEAD); }
WEAK int story_grumble_no_rite(void) { STUB("story_grumble_no_rite"); return story_flag(SF_GRUMBLE_NO_RITE); }
WEAK int story_rite_blocker(int resident_id) { (void)resident_id; return 0; }
WEAK int story_possessor_override(int resident_id) { (void)resident_id; return -1; }
WEAK int story_pacing_scale(void) { return 256; }
WEAK int story_encounter_rate(void) { return 256; }

/* ---- scenes.h (H4: home_draw.c, home_mart.c; S2: story_verbs.c,
 * story_timing.c) ---- */
STUB_SCENE(SCENE_HOME, "HOME");
STUB_SCENE(SCENE_HOME_MART, "HOME_MART");
STUB_SCENE(SCENE_STORY_MG, "STORY_MG");
STUB_SCENE(SCENE_STORY_TIMING, "STORY_TIMING");
/* --- end S0 --- */

/* --- H1: the dream house's generated tables (furngen, H4: core/gen/
 * furn_data.c, furn_iso.c, furn_grant.c). Weak empty defaults so the house
 * code links before (and, for the iso sprites, on the PS1 without) them. --- */
WEAK const DoomoFurnPrim doomo_furn_prims[1] = { { 0, 0, 0, 0, 0, 0, 0, 0 } };
WEAK const DoomoFurnIso doomo_furn_iso[1] = { { 0, 0, 0, 0, 0 } };
WEAK const uint16_t doomo_furn_iso_first[1] = { 0 };
WEAK const uint8_t doomo_furn_iso_data[1] = { 0xFF };
WEAK const uint16_t doomo_furn_iso_count = 0;
WEAK const DoomoFurnGrant doomo_furn_grants[1] = { { 0, 0, NULL } };
WEAK const uint8_t doomo_furn_cap1[FURN_MAX / 8] = { 0 };
/* --- end H1 --- */

/* --- music: MU0's weak placeholders for music.h (docs/design/music/SPEC.md
 * sec.9.1, 21.2). MU1's mus_*.c replace them milestone by milestone; until
 * then every call is a safe no-op or a refusal (-1). --- */
#include "doomo/music.h"
WEAK void music_set_cue(int cue, int grid) { (void)cue; (void)grid; STUB("music_set_cue"); }
WEAK void music_set_cue_ex(const MusCueReq *r) { (void)r; STUB("music_set_cue_ex"); }
WEAK void music_set_cue_dual(int cue_a, int cue_b) { (void)cue_a; (void)cue_b; STUB("music_set_cue_dual"); }
WEAK int  music_cue(void) { STUB("music_cue"); return MC_NONE; }
WEAK void music_resume(int cue) { (void)cue; STUB("music_resume"); }
WEAK void music_set_param(int p, int v) { (void)p; (void)v; STUB("music_set_param"); }
WEAK void music_set_param_now(int p, int v) { (void)p; (void)v; STUB("music_set_param_now"); }
WEAK int  music_param(int p) { (void)p; STUB("music_param"); return 0; }
WEAK void music_pulse_duck(int gain_q8, int ticks) { (void)gain_q8; (void)ticks; STUB("music_pulse_duck"); }
WEAK int  music_intensity(void) { STUB("music_intensity"); return 0; }
WEAK int  music_sting(int sting, int grid, int flags) { (void)sting; (void)grid; (void)flags; STUB("music_sting"); return -1; }
WEAK int  music_sting_at(int sting, uint32_t land_tick, int flags) { (void)sting; (void)land_tick; (void)flags; STUB("music_sting_at"); return -1; }
WEAK void music_quote(int motif, int prio) { (void)motif; (void)prio; STUB("music_quote"); }
WEAK int  music_vinyl(int pattern, int grid, int source, int after)
{ (void)pattern; (void)grid; (void)source; (void)after; STUB("music_vinyl"); return -1; }
WEAK int  music_vinyl_at(int pattern, uint32_t land_tick, int source, int after)
{ (void)pattern; (void)land_tick; (void)source; (void)after; STUB("music_vinyl_at"); return -1; }
WEAK void music_vinyl_live(int16_t speed_q8, uint8_t fader) { (void)speed_q8; (void)fader; STUB("music_vinyl_live"); }
WEAK void music_resume_on_beat(void) { STUB("music_resume_on_beat"); }
WEAK void music_sfx_synth(int sfx) { (void)sfx; STUB("music_sfx_synth"); }
WEAK void music_pos(MusPos *out) { STUB("music_pos"); if (out) memset(out, 0, sizeof(*out)); }
WEAK int  music_ticks_to(int grid) { (void)grid; STUB("music_ticks_to"); return 0; }
WEAK int  music_land(int grid, int frames, int max_bend_q8) { (void)grid; (void)max_bend_q8; STUB("music_land"); return frames; }
WEAK int  music_on_beat(MusBeatFn fn, uint8_t mask) { (void)fn; (void)mask; STUB("music_on_beat"); return -1; }
WEAK void music_off_beat(int slot) { (void)slot; STUB("music_off_beat"); }
WEAK int  music_beat_events(MusBeatEv *out, int max) { (void)out; (void)max; STUB("music_beat_events"); return 0; }
WEAK void music_mute_window(int from, int to) { (void)from; (void)to; STUB("music_mute_window"); }
WEAK void music_duck_window(int from, int to, int gain_q8) { (void)from; (void)to; (void)gain_q8; STUB("music_duck_window"); }
WEAK int  music_gated(void) { STUB("music_gated"); return 0; }
WEAK int  music_resting(void) { STUB("music_resting"); return 0; }
WEAK uint16_t music_snap_pitch(uint16_t pitch16) { STUB("music_snap_pitch"); return pitch16; }
WEAK void music_stall(int kind) { (void)kind; STUB("music_stall"); }
WEAK void music_prefill(int ms) { (void)ms; STUB("music_prefill"); }
WEAK void music_profile(int scene_class) { (void)scene_class; STUB("music_profile"); }
WEAK void music_update(void) { STUB("music_update"); }
WEAK uint32_t music_hash(int cue, int minutes) { (void)cue; (void)minutes; STUB("music_hash"); return 0; }
WEAK int  mus_bind_cue(int song, int scene, int map, int arg)
{ (void)song; (void)scene; (void)map; (void)arg; STUB("mus_bind_cue"); return MC_NONE; }
WEAK int  music_cue_exists(int cue) { (void)cue; STUB("music_cue_exists"); return 0; }
WEAK uint32_t music_cue_length(int cue) { (void)cue; STUB("music_cue_length"); return 0; }
WEAK const char *music_cue_name(int cue) { (void)cue; STUB("music_cue_name"); return ""; }
WEAK void mus_drv_tick(void) { STUB("mus_drv_tick"); }
/* --- end music --- */
