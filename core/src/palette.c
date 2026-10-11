/* palette.c - the 256-colour master palette (owner: art).
 *
 * Every colour is hand-picked. Paint colours and ramps are hue-shifted
 * (shadows lean cool/purple, highlights lean warm/yellow) the way pixel
 * artists shade, so even 3-shade paints look rich on a GBA screen.
 *
 * The light colormaps and tint maps are computed at BUILD time by artgen
 * (core/art/art_palette.c: a colour in shadow walks down its own shade
 * family - and a paint continues into its related environment ramp - so
 * shadows stay rich and hue-shifted instead of greying out; only below a
 * family's darkest shade does it fall back to the perceptually nearest,
 * hue-preserving colour) and stored as const tables in
 * core/gen/art_data.c; palette_init() just copies them into RAM so the
 * GBA boots instantly. artgen links this file too and provides empty
 * placeholders for those tables (it never calls palette_init()).
 */
#include "doomo/palette.h"

/* 8-bit RGB -> BGR555 */
#define C(r, g, b) ((uint16_t)((((r) >> 3) & 31) | ((((g) >> 3) & 31) << 5) | \
                               ((((b) >> 3) & 31) << 10)))
/* Three shades of one paint / skin colour: dark, base, light. */
#define C3(r0, g0, b0, r1, g1, b1, r2, g2, b2) \
    C(r0, g0, b0), C(r1, g1, b1), C(r2, g2, b2)

const uint16_t doomo_palette[256] = {
    /* ---- 0..15: transparent + UI / system ---------------------------- */
    C(  0,   0,   0),   /* 0  PAL_CLEAR (black when drawn as a rect)      */
    C(  0,   0,   0),   /* 1  PAL_BLACK                                   */
    C(255, 255, 255),   /* 2  PAL_WHITE                                   */
    C(204, 200, 216),   /* 3  PAL_GRAY_L                                  */
    C(140, 136, 156),   /* 4  PAL_GRAY                                    */
    C( 76,  72,  92),   /* 5  PAL_GRAY_D                                  */
    C(255, 184, 208),   /* 6  PAL_UI_PINK                                 */
    C(208,  80, 136),   /* 7  PAL_UI_PINK_D                               */
    C(168, 240, 208),   /* 8  PAL_UI_MINT                                 */
    C( 48, 160, 128),   /* 9  PAL_UI_MINT_D                               */
    C(255, 248, 228),   /* 10 PAL_UI_CREAM                                */
    C(232,  40,  48),   /* 11 PAL_UI_RED                                  */
    C(120,   8,  24),   /* 12 PAL_UI_RED_D                                */
    C(255, 208,  64),   /* 13 PAL_UI_GOLD                                 */
    C(168,  88, 224),   /* 14 PAL_UI_PURPLE                               */
    C(176, 200, 120),   /* 15 PAL_UI_LCD                                  */

    /* ---- 16..111: 32 paint colours x (dark, base, light) ------------- */
    C3(184, 188, 212,  238, 240, 250,  255, 255, 255),  /* WHITE       */
    C3(112, 116, 140,  164, 168, 188,  208, 212, 228),  /* SILVER      */
    C3( 36,  36,  52,   64,  64,  84,   96,  96, 120),  /* CHARCOAL    */
    C3(  8,   8,  16,   24,  22,  32,   44,  40,  56),  /* JET         */
    C3(144,  16,  48,  224,  40,  72,  255, 112, 128),  /* CHERRY      */
    C3( 72,   0,  16,  136,   8,  24,  192,  32,  40),  /* BLOOD       */
    C3(168,  48,  96,  232,  96, 140,  255, 156, 184),  /* ROSE        */
    C3(216,  88, 160,  255, 144, 200,  255, 200, 232),  /* BUBBLEGUM   */
    C3(216, 120, 104,  255, 168, 144,  255, 212, 192),  /* PEACH       */
    C3(192,  72,  24,  248, 128,  40,  255, 184,  96),  /* TANGERINE   */
    C3( 96,  36,  24,  152,  68,  32,  200, 108,  56),  /* RUST        */
    C3(168, 104,  24,  232, 160,  40,  255, 208, 104),  /* HONEY       */
    C3(200, 168,  24,  255, 224,  64,  255, 248, 160),  /* LEMON       */
    C3(120, 160,  24,  176, 216,  40,  224, 248, 120),  /* CHARTREUSE  */
    C3( 40, 144,  48,   88, 208,  72,  160, 240, 128),  /* LIME        */
    C3( 56, 168, 144,  120, 224, 184,  192, 248, 224),  /* MINT        */
    C3( 16,  64,  48,   32, 104,  64,   64, 152,  88),  /* FOREST      */
    C3(  8,  96, 112,   24, 152, 160,   88, 208, 208),  /* TEAL        */
    C3( 24, 160, 208,   72, 216, 248,  168, 240, 255),  /* CYAN        */
    C3( 64, 128, 216,  120, 184, 248,  184, 224, 255),  /* SKY         */
    C3( 24,  56, 168,   48, 104, 232,  112, 160, 255),  /* COBALT      */
    C3( 16,  20,  72,   32,  40, 120,   64,  80, 168),  /* NAVY        */
    C3(144, 112, 208,  192, 168, 248,  232, 216, 255),  /* LAVENDER    */
    C3( 72,  24, 120,  120,  48, 176,  168, 104, 224),  /* GRAPE       */
    C3(120,  32, 168,  176,  80, 232,  224, 152, 255),  /* VAPE_PURPLE */
    C3(168,  16, 136,  240,  56, 200,  255, 136, 232),  /* MAGENTA     */
    C3( 56,  28,  20,   96,  56,  36,  136,  88,  56),  /* CHOCOLATE   */
    C3(152,  88,  40,  200, 136,  72,  240, 184, 120),  /* CARAMEL     */
    C3(176, 160, 120,  224, 216, 176,  248, 244, 224),  /* BONE        */
    C3( 72, 144,  16,  144, 216,  40,  208, 248, 112),  /* SLIME       */
    C3(192,  32,   0,  255, 104,   0,  255, 200,  48),  /* HELLFIRE    */
    C3( 32, 192, 144,  104, 248, 200,  200, 255, 232),  /* ECTOPLASM   */

    /* ---- 112..135: 8 natural skin tones x (dark, base, light) -------- */
    C3(232, 184, 184,  255, 224, 216,  255, 244, 240),  /* PORCELAIN   */
    C3(224, 160, 136,  248, 204, 176,  255, 228, 208),  /* FAIR        */
    C3(208, 136, 104,  240, 180, 144,  255, 212, 184),  /* PEACH       */
    C3(176, 104,  72,  216, 152, 104,  240, 192, 144),  /* TAN         */
    C3(144, 100,  56,  192, 144,  88,  224, 184, 128),  /* OLIVE       */
    C3(112,  64,  40,  160, 104,  64,  200, 144, 104),  /* BROWN       */
    C3( 80,  44,  32,  120,  72,  48,  160, 112,  80),  /* UMBER       */
    C3( 48,  24,  24,   80,  48,  40,  112,  80,  64),  /* EBONY       */

    /* ---- 136..255: 15 environment ramps x 8 shades (dark -> light) --- */
    /* STONE: cool lavender-grey; cobbles, castles, hell stone */
    C( 20,  18,  30), C( 40,  36,  54), C( 62,  58,  80), C( 88,  84, 108),
    C(118, 114, 138), C(152, 148, 170), C(188, 186, 204), C(226, 226, 238),
    /* WOOD: warm and a little red, so it doubles as rust */
    C( 40,  20,  20), C( 68,  36,  28), C(100,  56,  36), C(134,  80,  48),
    C(168, 108,  64), C(200, 142,  88), C(228, 180, 124), C(248, 216, 168),
    /* GRASS: teal shadows to buttery highlights */
    C( 12,  40,  40), C( 20,  66,  50), C( 32,  98,  56), C( 58, 134,  60),
    C( 94, 170,  70), C(138, 204,  88), C(186, 230, 116), C(228, 248, 164),
    /* PASTEL_PINK: raspberry shadow to strawberry milk */
    C( 76,  20,  52), C(124,  38,  84), C(178,  64, 120), C(222, 102, 152),
    C(246, 142, 182), C(255, 180, 206), C(255, 212, 226), C(255, 236, 244),
    /* SKY: midnight navy to pale noon sky (also water and night) */
    C(  8,  10,  36), C( 18,  26,  72), C( 32,  50, 116), C( 50,  84, 160),
    C( 78, 126, 204), C(118, 170, 234), C(164, 208, 250), C(214, 236, 255),
    /* VAPE: violet vapour */
    C( 26,  10,  40), C( 50,  20,  76), C( 80,  36, 114), C(114,  58, 152),
    C(150,  90, 190), C(186, 128, 222), C(216, 172, 244), C(242, 220, 255),
    /* CANDY: butterscotch to lemon cream (sand, gold, stars, sunshine) */
    C( 68,  34,  22), C(122,  64,  26), C(178, 104,  30), C(224, 150,  38),
    C(248, 192,  56), C(255, 222,  96), C(255, 240, 152), C(255, 252, 216),
    /* MINT */
    C( 10,  40,  48), C( 18,  72,  76), C( 30, 110, 102), C( 52, 150, 128),
    C( 88, 190, 154), C(136, 220, 182), C(184, 240, 212), C(226, 255, 238),
    /* PEACH: cocoa shadow to peach cream (dirt paths, sand, cute walls) */
    C( 72,  30,  32), C(120,  54,  46), C(168,  88,  64), C(210, 126,  88),
    C(238, 162, 116), C(252, 194, 148), C(255, 220, 184), C(255, 242, 224),
    /* BLOOD */
    C( 24,   2,   8), C( 52,   4,  14), C( 84,   8,  18), C(120,  12,  22),
    C(160,  20,  28), C(200,  34,  36), C(232,  64,  54), C(255, 112,  96),
    /* LAVA: ember to white-hot */
    C( 50,   8,   6), C( 98,  16,   6), C(152,  32,   6), C(204,  64,   8),
    C(240, 110,  16), C(255, 160,  32), C(255, 212,  72), C(255, 248, 172),
    /* FLESH: bruised purple to raw pink */
    C( 40,  12,  22), C( 72,  22,  36), C(108,  38,  50), C(146,  58,  66),
    C(182,  86,  84), C(210, 118, 108), C(232, 154, 138), C(248, 196, 178),
    /* BONE */
    C( 36,  28,  26), C( 66,  54,  44), C(100,  86,  66), C(138, 122,  94),
    C(176, 160, 126), C(208, 194, 160), C(232, 222, 194), C(250, 246, 228),
    /* SICK: bile green */
    C( 18,  28,  12), C( 34,  50,  16), C( 56,  78,  20), C( 84, 110,  24),
    C(118, 146,  30), C(156, 182,  42), C(196, 214,  70), C(234, 242, 130),
    /* STEEL: blue-grey gunmetal */
    C( 14,  18,  26), C( 28,  34,  46), C( 46,  54,  70), C( 68,  80,  98),
    C( 96, 110, 130), C(130, 144, 164), C(170, 182, 200), C(214, 222, 236)
};

const char *const doomo_paint_names[PAL_PAINT_COUNT] = {
    "White", "Silver", "Charcoal", "Jet",
    "Cherry", "Blood", "Rose", "Bubblegum",
    "Peach", "Tangerine", "Rust", "Honey",
    "Lemon", "Chartreuse", "Lime", "Mint",
    "Forest", "Teal", "Cyan", "Sky",
    "Cobalt", "Navy", "Lavender", "Grape",
    "Vape Purple", "Magenta", "Chocolate", "Caramel",
    "Bone", "Slime", "Hellfire", "Ectoplasm"
};

const char *const doomo_skin_names[PAL_SKIN_COUNT] = {
    "Porcelain", "Fair", "Peach", "Tan", "Olive", "Brown", "Umber", "Ebony"
};

const char *const doomo_ramp_names[PAL_RAMP_COUNT] = {
    "Stone", "Wood", "Grass", "Pastel Pink", "Sky", "Vape", "Candy", "Mint",
    "Peach", "Blood", "Lava", "Flesh", "Bone", "Sick", "Steel"
};

const char *const doomo_tint_names[TINT_COUNT] = {
    "None", "Red", "Gold", "Green", "Purple", "Night"
};

/* the generated tables themselves (ROM on the GBA): no 5.6 KB RAM copy */
const uint8_t (*doomo_colormap)[256] = doomo_art_colormap;
const uint8_t (*doomo_tintmap)[256] = doomo_art_tintmap;

void palette_rgb(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint16_t c = doomo_palette[index];
    int r5 = c & 31, g5 = (c >> 5) & 31, b5 = (c >> 10) & 31;
    if (r) *r = (uint8_t)((r5 << 3) | (r5 >> 2));
    if (g) *g = (uint8_t)((g5 << 3) | (g5 >> 2));
    if (b) *b = (uint8_t)((b5 << 3) | (b5 >> 2));
}

uint32_t palette_distance(int r1, int g1, int b1, int r2, int g2, int b2)
{
    /* "redmean" weighted RGB distance: cheap, integer-only and much closer
     * to perceived difference than plain RGB. Max value < 2^20. */
    int rm = (r1 + r2) >> 1;
    int dr = r1 - r2, dg = g1 - g2, db = b1 - b2;
    return (uint32_t)((((512 + rm) * dr * dr) >> 8) + 4 * dg * dg +
                      (((767 - rm) * db * db) >> 8));
}

uint8_t palette_nearest(int r, int g, int b)
{
    uint32_t best = 0xFFFFFFFFu;
    int i, besti = 1;
    r = DOOMO_CLAMP(r, 0, 255);
    g = DOOMO_CLAMP(g, 0, 255);
    b = DOOMO_CLAMP(b, 0, 255);
    for (i = 1; i < 256; i++) {
        uint8_t pr, pg, pb;
        uint32_t d;
        palette_rgb((uint8_t)i, &pr, &pg, &pb);
        d = palette_distance(r, g, b, pr, pg, pb);
        if (d < best) {
            best = d;
            besti = i;
            if (d == 0) break;
        }
    }
    return (uint8_t)besti;
}

int palette_luma(uint8_t index)
{
    uint8_t r, g, b;
    palette_rgb(index, &r, &g, &b);
    return (r * 77 + g * 150 + b * 29) >> 8;
}

uint8_t palette_darker(uint8_t index)
{
    return doomo_art_darker[index];
}

uint8_t palette_lighter(uint8_t index)
{
    return doomo_art_lighter[index];
}

uint8_t palette_shade(uint8_t index, int light)
{
    light = DOOMO_CLAMP(light, 0, DOOMO_LIGHT_LEVELS - 1);
    return doomo_colormap[light][index];
}

void palette_init(void)
{
    doomo_colormap = doomo_art_colormap;
    doomo_tintmap = doomo_art_tintmap;
}
