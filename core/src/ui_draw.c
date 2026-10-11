/* ui_draw.c - colours, styles and shaded primitives of the UI kit (owner:
 * foundation). Everything is drawn with plat_rect spans, so it works on
 * every platform and clips safely.
 *
 * Shading recipe (ARCHITECTURE.md "Art direction"): coloured outline (a
 * dark shade of the local colour, never flat black), a hue-shifted shadow
 * on the lower-right, a warm light rim on the upper-left and two glossy
 * white glints.
 */
#include "ui_internal.h"

/* ====================================================================== */
/* colour families                                                          */
static uint8_t cache_dark[256], cache_light[256];

static uint8_t nearest_scaled(uint8_t c, int num, int den, int addr, int addg, int addb)
{
    uint8_t r, g, b;
    int R, G, B;
    palette_rgb(c, &r, &g, &b);
    R = r * num / den + addr; G = g * num / den + addg; B = b * num / den + addb;
    return palette_nearest(DOOMO_CLAMP(R, 0, 255), DOOMO_CLAMP(G, 0, 255),
                           DOOMO_CLAMP(B, 0, 255));
}

uint8_t ui_darker(uint8_t c)
{
    if (c == 0) return 0;
    if (c >= PAL_RAMP_BASE) {
        int s = (c - PAL_RAMP_BASE) & 7;
        return s ? (uint8_t)(c - 1) : c;
    }
    if (c >= PAL_PAINT_BASE) {               /* paints and skins: 3 shades */
        int s = (c - PAL_PAINT_BASE) % 3;
        if (s) return (uint8_t)(c - 1);
        if (!cache_dark[c])                   /* shift cool + darker */
            cache_dark[c] = nearest_scaled(c, 5, 8, -4, -6, 10);
        return cache_dark[c];
    }
    switch (c) {
    case PAL_WHITE: return PAL_GRAY_L;
    case PAL_GRAY_L: return PAL_GRAY;
    case PAL_GRAY: return PAL_GRAY_D;
    case PAL_GRAY_D: return PAL_BLACK;
    case PAL_UI_PINK: return PAL_UI_PINK_D;
    case PAL_UI_MINT: return PAL_UI_MINT_D;
    case PAL_UI_CREAM: return PAL_RAMP(RAMP_PEACH, 6);
    case PAL_UI_RED: return PAL_UI_RED_D;
    case PAL_UI_GOLD: return PAL_RAMP(RAMP_CANDY, 3);
    case PAL_UI_PURPLE: return PAL_RAMP(RAMP_VAPE, 2);
    case PAL_UI_LCD: return PAL_RAMP(RAMP_GRASS, 2);
    case PAL_UI_PINK_D: return PAL_RAMP(RAMP_PASTEL_PINK, 1);
    case PAL_UI_MINT_D: return PAL_RAMP(RAMP_MINT, 1);
    case PAL_UI_RED_D: return PAL_RAMP(RAMP_BLOOD, 1);
    default: return PAL_BLACK;
    }
}

uint8_t ui_lighter(uint8_t c)
{
    if (c == 0) return 0;
    if (c >= PAL_RAMP_BASE) {
        int s = (c - PAL_RAMP_BASE) & 7;
        return s < 7 ? (uint8_t)(c + 1) : PAL_WHITE;
    }
    if (c >= PAL_PAINT_BASE) {
        int s = (c - PAL_PAINT_BASE) % 3;
        if (s < 2) return (uint8_t)(c + 1);
        if (!cache_light[c])                  /* toward warm white */
            cache_light[c] = nearest_scaled(c, 1, 2, 132, 128, 112);
        return cache_light[c];
    }
    switch (c) {
    case PAL_BLACK: return PAL_GRAY_D;
    case PAL_GRAY_D: return PAL_GRAY;
    case PAL_GRAY: return PAL_GRAY_L;
    case PAL_GRAY_L: return PAL_WHITE;
    case PAL_UI_PINK_D: return PAL_UI_PINK;
    case PAL_UI_PINK: return PAL_RAMP(RAMP_PASTEL_PINK, 6);
    case PAL_UI_MINT_D: return PAL_UI_MINT;
    case PAL_UI_MINT: return PAL_RAMP(RAMP_MINT, 7);
    case PAL_UI_CREAM: return PAL_WHITE;
    case PAL_UI_RED_D: return PAL_UI_RED;
    case PAL_UI_RED: return PAL_RAMP(RAMP_BLOOD, 7);
    case PAL_UI_GOLD: return PAL_RAMP(RAMP_CANDY, 6);
    case PAL_UI_PURPLE: return PAL_RAMP(RAMP_VAPE, 5);
    case PAL_UI_LCD: return PAL_RAMP(RAMP_GRASS, 6);
    default: return PAL_WHITE;
    }
}

/* ====================================================================== */
/* styles                                                                   */
#define RP(r, s) PAL_RAMP(r, s)
/* fill, hi, lo, outline, gloss, text, text_dim, accent, accent_d, title, title_bg */
static const UiStyle skins[WSKIN_COUNT] = {
    /* STRAWBERRY */ { RP(RAMP_PASTEL_PINK, 6), RP(RAMP_PASTEL_PINK, 7), RP(RAMP_PASTEL_PINK, 5),
                       RP(RAMP_PASTEL_PINK, 2), PAL_WHITE, RP(RAMP_PASTEL_PINK, 0),
                       RP(RAMP_PASTEL_PINK, 2), PAL_PAINT(PAINT_ROSE, 1), RP(RAMP_PASTEL_PINK, 5),
                       PAL_WHITE, RP(RAMP_PASTEL_PINK, 3) },
    /* MINT_CHOC */  { RP(RAMP_MINT, 6), RP(RAMP_MINT, 7), RP(RAMP_MINT, 5),
                       RP(RAMP_WOOD, 1), PAL_WHITE, RP(RAMP_WOOD, 0),
                       RP(RAMP_WOOD, 3), PAL_PAINT(PAINT_CHOCOLATE, 1), RP(RAMP_MINT, 4),
                       PAL_WHITE, RP(RAMP_WOOD, 3) },
    /* VANILLA */    { RP(RAMP_PEACH, 7), PAL_UI_CREAM, RP(RAMP_PEACH, 6),
                       RP(RAMP_WOOD, 3), PAL_WHITE, RP(RAMP_WOOD, 1),
                       RP(RAMP_WOOD, 4), PAL_PAINT(PAINT_TANGERINE, 1), RP(RAMP_PEACH, 6),
                       PAL_WHITE, RP(RAMP_PEACH, 4) },
    /* LAVENDER */   { RP(RAMP_VAPE, 6), RP(RAMP_VAPE, 7), RP(RAMP_VAPE, 5),
                       RP(RAMP_VAPE, 2), PAL_WHITE, RP(RAMP_VAPE, 0),
                       RP(RAMP_VAPE, 2), PAL_PAINT(PAINT_GRAPE, 2), RP(RAMP_VAPE, 5),
                       PAL_WHITE, RP(RAMP_VAPE, 3) },
    /* SKY */        { RP(RAMP_SKY, 6), RP(RAMP_SKY, 7), RP(RAMP_SKY, 5),
                       RP(RAMP_SKY, 2), PAL_WHITE, RP(RAMP_SKY, 0),
                       RP(RAMP_SKY, 2), PAL_PAINT(PAINT_COBALT, 1), RP(RAMP_SKY, 5),
                       PAL_WHITE, RP(RAMP_SKY, 3) },
    /* PEACH */      { RP(RAMP_PEACH, 6), RP(RAMP_PEACH, 7), RP(RAMP_PEACH, 5),
                       RP(RAMP_PEACH, 2), PAL_WHITE, RP(RAMP_PEACH, 0),
                       RP(RAMP_PEACH, 2), PAL_PAINT(PAINT_TANGERINE, 0), RP(RAMP_PEACH, 5),
                       PAL_WHITE, RP(RAMP_PEACH, 3) },
    /* LEMON */      { RP(RAMP_CANDY, 6), RP(RAMP_CANDY, 7), RP(RAMP_CANDY, 5),
                       RP(RAMP_CANDY, 2), PAL_WHITE, RP(RAMP_CANDY, 0),
                       RP(RAMP_CANDY, 2), PAL_PAINT(PAINT_TANGERINE, 1), RP(RAMP_CANDY, 5),
                       PAL_WHITE, RP(RAMP_CANDY, 3) },
    /* BUBBLEGUM */  { PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE, PAL_PAINT(PAINT_BUBBLEGUM, 1),
                       PAL_PAINT(PAINT_MAGENTA, 0), PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0),
                       PAL_PAINT(PAINT_MAGENTA, 0), PAL_PAINT(PAINT_MAGENTA, 1),
                       PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_WHITE, PAL_PAINT(PAINT_MAGENTA, 1) },
    /* HELLFIRE */   { RP(RAMP_BLOOD, 2), RP(RAMP_BLOOD, 3), RP(RAMP_BLOOD, 1),
                       RP(RAMP_LAVA, 4), RP(RAMP_LAVA, 5), RP(RAMP_BONE, 7),
                       RP(RAMP_BONE, 4), RP(RAMP_LAVA, 6), RP(RAMP_BLOOD, 4),
                       RP(RAMP_LAVA, 7), RP(RAMP_BLOOD, 4) },
    /* MIDNIGHT */   { RP(RAMP_STONE, 1), RP(RAMP_STONE, 2), RP(RAMP_STONE, 0),
                       RP(RAMP_VAPE, 4), RP(RAMP_VAPE, 5), RP(RAMP_STONE, 7),
                       RP(RAMP_STONE, 5), PAL_PAINT(PAINT_LAVENDER, 1), RP(RAMP_STONE, 3),
                       PAL_WHITE, RP(RAMP_VAPE, 3) },
    /* TAMAGOTCHI */ { PAL_UI_LCD, RP(RAMP_GRASS, 6), RP(RAMP_SICK, 5),
                       RP(RAMP_GRASS, 1), RP(RAMP_GRASS, 7), RP(RAMP_GRASS, 0),
                       RP(RAMP_GRASS, 2), RP(RAMP_GRASS, 1), RP(RAMP_SICK, 6),
                       RP(RAMP_GRASS, 7), RP(RAMP_GRASS, 2) },
    /* GOTH */       { PAL_PAINT(PAINT_JET, 1), PAL_PAINT(PAINT_CHARCOAL, 0), PAL_PAINT(PAINT_JET, 0),
                       PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_BUBBLEGUM, 2),
                       PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_BUBBLEGUM, 1),
                       PAL_PAINT(PAINT_CHARCOAL, 1), PAL_WHITE, PAL_PAINT(PAINT_ROSE, 0) }
};

static const UiStyle style_mint = {
    RP(RAMP_MINT, 6), RP(RAMP_MINT, 7), RP(RAMP_MINT, 5), RP(RAMP_MINT, 2), PAL_WHITE,
    RP(RAMP_MINT, 0), RP(RAMP_MINT, 2), PAL_PAINT(PAINT_TEAL, 1), RP(RAMP_MINT, 5),
    PAL_WHITE, RP(RAMP_MINT, 3)
};

int ui_cur_style = WIN_CUTE;

const UiStyle *ui_style(int style)
{
    switch (style) {
    case WIN_MINT:  return &style_mint;
    case WIN_CREAM: return &skins[WSKIN_VANILLA];
    case WIN_HELL:  return &skins[WSKIN_HELLFIRE];
    case WIN_LCD:   return &skins[WSKIN_TAMAGOTCHI];
    case WIN_DARK:  return &skins[WSKIN_MIDNIGHT];
    default: {
        int s = opt_get(OPT_WINDOW_SKIN);
        if (s < 0 || s >= WSKIN_COUNT) s = 0;
        return &skins[s];
    }
    }
}

uint8_t ui_style_text(int style) { return ui_style(style)->text; }

/* ====================================================================== */
/* animation helpers                                                        */
int ui_wave(int t, int period, int amplitude)
{
    /* amplitude * sin(t * 4096 / period), floor(.. >> 16), without 64-bit
     * maths: t is reduced modulo the period first (same angle), and the
     * 17-bit sine is split so every product fits in 32 bits (exact for
     * |amplitude| < 2^22). */
    int32_t s, a, b;
    if (period <= 0) return 0;
    s = fx_sin((t % period) * DOOMO_ANG_FULL / period);
    a = (s >> 8) * amplitude;               /* (s >> 8) in -256..256       */
    b = (s & 255) * amplitude;
    return (int)((a + (b >> 8)) >> 8);
}

int ui_pop_scale(int ticks)
{
    static const uint16_t k[] = { 70, 140, 205, 255, 290, 300, 288, 268, 250, 244, 249, 254 };
    int bounce = opt_get(OPT_UI_BOUNCE);
    if (bounce == 0 || ticks < 0) return 256;
    if (ticks >= DOOMO_COUNTOF(k)) return 256;
    if (bounce == 1) return 256 - (256 - k[ticks]) / 2;
    return k[ticks];
}

/* ====================================================================== */
/* primitives (circles, rounded rects, hearts, stars: ui_raster.c)          */
void ui_frame(int x, int y, int w, int h, uint8_t c)
{
    if (plat_draw_skipped()) return;
    if (w <= 0 || h <= 0) return;
    plat_rect(x, y, w, 1, c);
    plat_rect(x, y + h - 1, w, 1, c);
    plat_rect(x, y, 1, h, c);
    plat_rect(x + w - 1, y, 1, h, c);
}

void ui_heart(int cx, int cy, int size, uint8_t fill)
{
    ui_heart_squash(cx, cy, size, 256, 256, fill);
}

void ui_sparkle(int cx, int cy, int phase, uint8_t c)
{
    static const uint8_t sz[16] = { 0, 1, 2, 3, 3, 3, 2, 2, 1, 1, 0, 0, 0, 0, 0, 0 };
    int s = sz[phase & 15];
    if (s == 0 || plat_draw_skipped()) return;
    plat_rect(cx - s, cy, 2 * s + 1, 1, c);
    plat_rect(cx, cy - s, 1, 2 * s + 1, c);
    if (s >= 2) {
        plat_rect(cx - 1, cy - 1, 3, 3, c);
        plat_rect(cx, cy, 1, 1, PAL_WHITE);
    }
    if (s >= 3) {
        plat_rect(cx - 2, cy - 2, 1, 1, c);
        plat_rect(cx + 2, cy + 2, 1, 1, c);
        plat_rect(cx - 2, cy + 2, 1, 1, c);
        plat_rect(cx + 2, cy - 2, 1, 1, c);
    }
}

/* ====================================================================== */
/* backdrop                                                                 */
typedef struct { uint8_t top, mid, bot, dot, deco, deco2; } Backdrop;

static void backdrop_colors(int style, Backdrop *b)
{
    switch (style) {
    case WIN_MINT:
        b->top = RP(RAMP_MINT, 7); b->mid = RP(RAMP_MINT, 6); b->bot = RP(RAMP_MINT, 5);
        b->dot = PAL_WHITE; b->deco = PAL_PAINT(PAINT_BUBBLEGUM, 1); b->deco2 = PAL_UI_GOLD;
        break;
    case WIN_CREAM:
        b->top = PAL_UI_CREAM; b->mid = RP(RAMP_PEACH, 7); b->bot = RP(RAMP_PEACH, 6);
        b->dot = PAL_WHITE; b->deco = PAL_PAINT(PAINT_ROSE, 1); b->deco2 = PAL_PAINT(PAINT_SKY, 1);
        break;
    case WIN_HELL:
        b->top = RP(RAMP_BLOOD, 0); b->mid = RP(RAMP_BLOOD, 1); b->bot = RP(RAMP_BLOOD, 2);
        b->dot = RP(RAMP_BLOOD, 3); b->deco = RP(RAMP_LAVA, 5); b->deco2 = RP(RAMP_LAVA, 6);
        break;
    case WIN_LCD:
        b->top = PAL_UI_LCD; b->mid = PAL_UI_LCD; b->bot = RP(RAMP_SICK, 5);
        b->dot = RP(RAMP_SICK, 6); b->deco = RP(RAMP_GRASS, 2); b->deco2 = RP(RAMP_GRASS, 1);
        break;
    case WIN_DARK:
        b->top = RP(RAMP_STONE, 0); b->mid = RP(RAMP_STONE, 1); b->bot = RP(RAMP_VAPE, 1);
        b->dot = RP(RAMP_STONE, 2); b->deco = RP(RAMP_VAPE, 5); b->deco2 = PAL_UI_GOLD;
        break;
    default:
        b->top = RP(RAMP_PASTEL_PINK, 7); b->mid = RP(RAMP_PASTEL_PINK, 6);
        b->bot = RP(RAMP_PASTEL_PINK, 5); b->dot = PAL_WHITE;
        b->deco = PAL_PAINT(PAINT_ROSE, 1); b->deco2 = PAL_UI_GOLD;
        break;
    }
}

/* Two-colour band seam: one row of 50% checker so gradients look soft. */
static void seam_row(int y, int w, uint8_t c, int phase)
{
    plat_checker(phase & 1, y, w - (phase & 1), 1, c);
}

void ui_backdrop(int style, uint32_t t)
{
    Backdrop b;
    int w = plat_screen_w(), h = plat_screen_h();
    int b1 = h / 3, b2 = h * 2 / 3, x, y, i;
    int cell = 24, off;
    if (plat_draw_skipped()) return;
    backdrop_colors(style, &b);
    plat_rect(0, 0, w, b1, b.top);
    plat_rect(0, b1, w, b2 - b1, b.mid);
    plat_rect(0, b2, w, h - b2, b.bot);
    seam_row(b1 - 1, w, b.mid, 0);
    seam_row(b1, w, b.top, 1);
    seam_row(b2 - 1, w, b.bot, 0);
    seam_row(b2, w, b.mid, 1);
    /* drifting polka dots (diagonal scroll, staggered rows) */
    off = (int)((t / 2) % (uint32_t)cell);
    for (y = -cell; y < h + cell; y += cell) {
        int row = (y + off) / cell;
        for (x = -cell; x < w + cell; x += cell) {
            int dx = x + off + ((row & 1) ? cell / 2 : 0);
            int dy = y + off;
            /* ui_fill_circle(dx, dy, 2, b.dot): rows of half-width 1, 2, 2,
             * 2, 1 = a 3x5 and a 5x3 rect */
            if (dx < -2 || dy < -2 || dx > w + 1 || dy > h + 1) continue;
            plat_rect(dx - 1, dy - 2, 3, 5, b.dot);
            plat_rect(dx - 2, dy - 1, 5, 3, b.dot);
        }
    }
    /* floating hearts and stars: rise slowly with a gentle sway */
    for (i = 0; i < 7; i++) {
        uint32_t seed = doomo_hash32((uint32_t)i * 7919u + 17u);
        int speed = 3 + (int)(seed % 4);
        int span = h + 24;
        int fy = h + 12 - (int)(((t * (uint32_t)speed) / 8 + (seed >> 8)) % (uint32_t)span);
        int fx = (int)((seed >> 4) % (uint32_t)(w - 16)) + 8 + ui_wave((int)t + i * 23, 90, 6);
        if (i % 3 == 2) ui_star(fx, fy, 4, b.deco2);
        else ui_heart(fx, fy, (i & 1) ? 4 : 3, b.deco);
    }
    /* twinkles */
    for (i = 0; i < 5; i++) {
        uint32_t seed = doomo_hash32((uint32_t)i * 104729u + 5u);
        int phase = (int)((t / 2 + seed) & 31);
        if (phase < 16)
            ui_sparkle((int)(seed % (uint32_t)w), (int)((seed >> 12) % (uint32_t)h), phase,
                       style == WIN_HELL ? b.deco : PAL_WHITE);
    }
}
