/* font.h - the shared bitmap font. 5x8 glyphs, column-major, bit 0 = top
 * row; drawn in a 6x8 cell. Covers ASCII 0x20..0x7E plus 32 icon glyphs
 * at 0x80..0x9F (use the ICON_* string macros in text). */
#ifndef DOOMO_FONT_H
#define DOOMO_FONT_H

#include "types.h"

#define FONT_ADVANCE 6
#define FONT_LINE    9
#define FONT_FIRST   0x20
#define FONT_LAST    0x9F

/* doomo_font[c - FONT_FIRST][col] */
extern const uint8_t doomo_font[FONT_LAST - FONT_FIRST + 1][5];

enum {
    ICON_HEART = 0x80, ICON_SKULL, ICON_STAR, ICON_COIN, ICON_CROSS,
    ICON_PENTAGRAM, ICON_NOTE, ICON_ARROW_R, ICON_ARROW_L, ICON_ARROW_U,
    ICON_ARROW_D, ICON_BTN_A, ICON_BTN_B, ICON_SMILE, ICON_FROWN, ICON_FIRE,
    ICON_DROP, ICON_BOLT, ICON_FOOD, ICON_ZZZ, ICON_SPARK, ICON_BOMB,
    ICON_GUN, ICON_WRENCH, ICON_GHOST, ICON_EGG, ICON_HOUSE, ICON_CLOUD,
    ICON_CHECK, ICON_X, ICON_MALE, ICON_FEMALE
};
/* Use in string literals: "Press " STR_ICON_A " to feed" */
#define STR_ICON_HEART  "\x80"
#define STR_ICON_SKULL  "\x81"
#define STR_ICON_STAR   "\x82"
#define STR_ICON_COIN   "\x83"
#define STR_ICON_CROSS  "\x84"
#define STR_ICON_PENTA  "\x85"
#define STR_ICON_NOTE   "\x86"
#define STR_ICON_A      "\x8B"
#define STR_ICON_B      "\x8C"
#define STR_ICON_FIRE   "\x8F"
#define STR_ICON_GHOST  "\x98"

int font_text_width(const char *s);   /* widest line in pixels */

#endif
