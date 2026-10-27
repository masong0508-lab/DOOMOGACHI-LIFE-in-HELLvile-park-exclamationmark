/* font.h - the shared bitmap font. 5x8 glyphs, column-major, bit 0 = top
 * row; drawn in a 6x8 cell. Covers ASCII 0x20..0x7E plus 32 icon glyphs
 * at 0x80..0x9F (use the ICON_* string macros in text) and the 16 Spanish
 * letters at 0xA0..0xAF (the STR_ES_* macros; core/lang/es.tsv is UTF-8 and
 * tools/langpack.py maps it onto these bytes). */
#ifndef DOOMO_FONT_H
#define DOOMO_FONT_H

#include "types.h"

#define FONT_ADVANCE 6
#define FONT_LINE    9
#define FONT_FIRST   0x20
#define FONT_LAST    0xAF

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
/* foundation additions: the rest of the icons */
#define STR_ICON_ARROW_R "\x87"
#define STR_ICON_ARROW_L "\x88"
#define STR_ICON_ARROW_U "\x89"
#define STR_ICON_ARROW_D "\x8A"
#define STR_ICON_SMILE  "\x8D"
#define STR_ICON_FROWN  "\x8E"
#define STR_ICON_DROP   "\x90"
#define STR_ICON_BOLT   "\x91"
#define STR_ICON_FOOD   "\x92"
#define STR_ICON_ZZZ    "\x93"
#define STR_ICON_SPARK  "\x94"
#define STR_ICON_BOMB   "\x95"
#define STR_ICON_GUN    "\x96"
#define STR_ICON_WRENCH "\x97"
#define STR_ICON_EGG    "\x99"
#define STR_ICON_HOUSE  "\x9A"
#define STR_ICON_CLOUD  "\x9B"
#define STR_ICON_CHECK  "\x9C"
#define STR_ICON_X      "\x9D"
#define STR_ICON_MALE   "\x9E"
#define STR_ICON_FEMALE "\x9F"

/* Spanish letters (bytes 0xA0..0xAF). Compose with string-literal
 * concatenation split after the escape: "Espa" STR_ES_N "ol". */
#define STR_ES_a  "\xA0"   /* a acute      */
#define STR_ES_e  "\xA1"
#define STR_ES_i  "\xA2"
#define STR_ES_o  "\xA3"
#define STR_ES_u  "\xA4"
#define STR_ES_n  "\xA5"   /* n tilde      */
#define STR_ES_ue "\xA6"   /* u dieresis   */
#define STR_ES_A  "\xA7"
#define STR_ES_E  "\xA8"
#define STR_ES_I  "\xA9"
#define STR_ES_O  "\xAA"
#define STR_ES_U  "\xAB"
#define STR_ES_N  "\xAC"
#define STR_ES_UE "\xAD"
#define STR_ES_QUEST "\xAE" /* inverted ?   */
#define STR_ES_EXCL  "\xAF" /* inverted !   */

int font_text_width(const char *s);   /* widest line in pixels */

#endif
