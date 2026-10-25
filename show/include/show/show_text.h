/* show_text.h - on-screen text (WP7; spec 5.5 "Text", 8.6): SUPERs in the
 * styles type / pop / deadpan, signs on props, cards from templates (the
 * TV-MA card, act cards, title cards), in the game font or a .pxt show font
 * (the child's hand). Owner: ENGINE lane 2.
 *
 * Every glyph is drawn as a small palette-exact image built on the PS1 from
 * the font bitmap with its outline and drop shadow baked in, so a glyph is
 * one SPRT, scales by whole numbers exactly and pops (a scale overshoot)
 * through plat_image_xform. Text is drawn on the frame, not in the world: it
 * ignores the camera and the shake (spec 2.3).
 *
 * Timing is baked by the compiler (a pure function of the shot frame):
 *   item visible   t_on <= t < t_off             (t_off TX_END = to the shot end)
 *   type           glyph k of N appears on t0 + round(k * (t1 - t0) / (N - 1))
 *                  (the co02 SUPER: 52 glyphs over +30..+132, every 2 f, AT7.1);
 *                  every glyph hops 2 px on its first two frames (TX_HOP)
 *   pop            the whole item scales 0.5, 1.2, 1.1, 0.97, 1.0 over its
 *                  first 5 frames (a 4-6 f overshoot)
 *   deadpan        whole on t_on, no motion
 *   sign           like deadpan, anchored to a prop slot (moves with it)
 *   box            a filled rectangle (card backgrounds, rating boxes)
 *
 * Payload: ShowTextSet, ShowTextItem[nitems], char pool[pool_bytes]
 * ('\n' starts a new line; alignment is per line about x).
 */
#ifndef SHOW_TEXT_H
#define SHOW_TEXT_H

#include <stdint.h>

enum { TS_TYPE = 0, TS_POP, TS_DEADPAN, TS_SIGN, TS_BOX };
enum { TF_GAME = 0, TF_CHILD = 1, TF_COUNT };
enum { TA_LEFT = 0, TA_CENTER = 1, TA_RIGHT = 2 };
enum { TX_HOP = 1, TX_SLOT = 2, TX_NOOUTLINE = 4 };
#define TX_END 0xFFFF

typedef struct ShowTextItem {           /* 28 B                                       */
    uint16_t t_on, t_off;               /* visible frames [t_on, t_off)               */
    uint16_t t0, t1;                    /* type: first and last glyph frames          */
    uint16_t str, len;                  /* bytes in the pool                          */
    int16_t  x, y;                      /* anchor (show px): top of the first line    */
    int16_t  w, h;                      /* TS_BOX size; TX_SLOT: the slot id in w     */
    uint8_t  style, font, scale, align; /* TS_*, TF_*, 1..4, TA_*                     */
    uint8_t  color, outline, shadow;    /* palette indices, 0 = none                  */
    uint8_t  flags;                     /* TX_*                                       */
} ShowTextItem;

typedef struct ShowTextSet {            /* 8 B                                        */
    uint16_t nitems, pool_bytes;
    uint32_t pad;
} ShowTextSet;

/* a .pxt show font, compiled into show.exe (show_font_child.c, generated
 * from show/engine/fonts/child_hand.pxt by showtk/l2/pxt.py) */
typedef struct ShowPxtFont {
    uint8_t  first, count;              /* code range                                 */
    uint8_t  h, line;                   /* cell height, line advance                  */
    uint8_t  space;                     /* gap between glyphs                         */
    uint8_t  pad[3];
    const uint8_t *w;                   /* advance width per glyph (0 = missing)      */
    const uint8_t *rows;                /* count x h bytes, bit 7 = leftmost column   */
} ShowPxtFont;
extern const ShowPxtFont show_font_child;

void show_text_reset(void);
int  show_text_bind(const void *payload, uint32_t bytes);     /* 0 ok, -1 bad      */
void show_text_draw(int t);
/* measurement (the compiler mirrors it): width of one line of n bytes, and
 * the line advance, in show px at scale 1 */
int  show_text_line_w(int font, const char *s, int n);
int  show_text_line_h(int font);
/* glyphs of item i visible at t (type schedule), and the item's glyph count */
int  show_text_glyphs(int i, int t, int *n);

#endif
