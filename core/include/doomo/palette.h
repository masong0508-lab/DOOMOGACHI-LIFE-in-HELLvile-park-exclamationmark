/* palette.h - the single 256-colour master palette used by every image,
 * texture and UI element on every platform.
 *
 * Colours are 15-bit BGR555 (bits 0-4 red, 5-9 green, 10-14 blue) which
 * is the native format of both the GBA and the PS1 (PS1 platforms must
 * set the STP bit / avoid 0x0000 for opaque black themselves).
 *
 * Layout (fixed - art, textures and saves depend on it):
 *   0          transparent in images; draws as black for rect/clear
 *   1..15      UI / system colours (PAL_* below)
 *   16..111    32 "paint" colours x 3 shades  -> PAL_PAINT(p, shade)
 *   112..135   8 natural skin tones x 3 shades -> PAL_SKIN(s, shade)
 *   136..255   15 environment ramps x 8 shades -> PAL_RAMP(r, shade)
 * Shade order is always dark -> light (paint/skin: 0 dark, 1 base, 2 light;
 * ramps: 0 darkest .. 7 brightest).
 */
#ifndef DOOMO_PALETTE_H
#define DOOMO_PALETTE_H

#include "types.h"

enum {
    PAL_CLEAR = 0,
    PAL_BLACK = 1, PAL_WHITE, PAL_GRAY_L, PAL_GRAY, PAL_GRAY_D,
    PAL_UI_PINK,      /* default cutesy window fill          */
    PAL_UI_PINK_D,    /* window border / shadow               */
    PAL_UI_MINT, PAL_UI_MINT_D,
    PAL_UI_CREAM,     /* text background, paper              */
    PAL_UI_RED,       /* hell UI accents                      */
    PAL_UI_RED_D,
    PAL_UI_GOLD,      /* money, highlights                    */
    PAL_UI_PURPLE,    /* vape / possession                    */
    PAL_UI_LCD        /* tamagotchi LCD green                 */
};

#define PAL_PAINT_BASE   16
#define PAL_PAINT_COUNT  32
#define PAL_SKIN_BASE    112
#define PAL_SKIN_COUNT   8
#define PAL_RAMP_BASE    136
#define PAL_RAMP_COUNT   15
#define PAL_RAMP_SHADES  8

#define PAL_PAINT(p, shade) ((uint8_t)(PAL_PAINT_BASE + (p) * 3 + (shade)))
#define PAL_SKIN(s, shade)  ((uint8_t)(PAL_SKIN_BASE + (s) * 3 + (shade)))
#define PAL_RAMP(r, shade)  ((uint8_t)(PAL_RAMP_BASE + (r) * 8 + (shade)))

/* Paint colours (selectable for hair, eyes, clothes, horns, ...). */
enum {
    PAINT_WHITE, PAINT_SILVER, PAINT_CHARCOAL, PAINT_JET,
    PAINT_CHERRY, PAINT_BLOOD, PAINT_ROSE, PAINT_BUBBLEGUM,
    PAINT_PEACH, PAINT_TANGERINE, PAINT_RUST, PAINT_HONEY,
    PAINT_LEMON, PAINT_CHARTREUSE, PAINT_LIME, PAINT_MINT,
    PAINT_FOREST, PAINT_TEAL, PAINT_CYAN, PAINT_SKY,
    PAINT_COBALT, PAINT_NAVY, PAINT_LAVENDER, PAINT_GRAPE,
    PAINT_VAPE_PURPLE, PAINT_MAGENTA, PAINT_CHOCOLATE, PAINT_CARAMEL,
    PAINT_BONE, PAINT_SLIME, PAINT_HELLFIRE, PAINT_ECTOPLASM
};

/* Natural skin tones; skin may also use any paint colour (see look.skin). */
enum { SKIN_PORCELAIN, SKIN_FAIR, SKIN_PEACH, SKIN_TAN,
       SKIN_OLIVE, SKIN_BROWN, SKIN_UMBER, SKIN_EBONY };

/* Environment ramps for textures, tiles, demons and sprites. */
enum {
    RAMP_STONE, RAMP_WOOD, RAMP_GRASS, RAMP_PASTEL_PINK, RAMP_SKY,
    RAMP_VAPE, RAMP_CANDY, RAMP_MINT, RAMP_PEACH, RAMP_BLOOD,
    RAMP_LAVA, RAMP_FLESH, RAMP_BONE, RAMP_SICK, RAMP_STEEL
};

/* The palette itself, BGR555. */
extern const uint16_t doomo_palette[256];
extern const char *const doomo_paint_names[PAL_PAINT_COUNT];
extern const char *const doomo_skin_names[PAL_SKIN_COUNT];

/* Light / distance shading. doomo_colormap[level][index] gives the palette
 * index that best represents `index` at light `level` (0 = black,
 * 15 = full bright, unchanged). Index 0 always maps to 0.
 * Built by palette_init(); lives in RAM (4 KB). */
#define DOOMO_LIGHT_LEVELS 16
extern uint8_t doomo_colormap[DOOMO_LIGHT_LEVELS][256];

/* Tints: doomo_tintmap[tint][index] -> index pushed toward a hue.
 * Used for damage flash, holy flash, vape haze and night time. */
enum { TINT_NONE, TINT_RED, TINT_GOLD, TINT_GREEN, TINT_PURPLE, TINT_NIGHT,
       TINT_COUNT };
extern uint8_t doomo_tintmap[TINT_COUNT][256];

void palette_init(void);
/* Nearest palette index (1..255) to an 8-bit RGB colour. */
uint8_t palette_nearest(int r, int g, int b);
/* Decode an entry to 8-bit RGB. */
void palette_rgb(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b);

#endif
