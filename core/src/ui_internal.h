/* ui_internal.h - private glue between the ui*.c files (owner:
 * foundation). Not part of the public contract. */
#ifndef DOOMO_UI_INTERNAL_H
#define DOOMO_UI_INTERNAL_H

#include "doomo/doomo.h"

/* Style of the most recent ui_window(): widgets drawn inside it use its
 * colours. */
extern int ui_cur_style;

/* Small cursor glyph per OPT_CURSOR at x,y (top-left of a ~7x7 box). */
void ui_cursor_glyph(int x, int y, int kind, uint8_t fill);
/* A rounded "key cap" / pill used by buttons and the keyboard. */
void ui_pill(int x, int y, int w, int h, uint8_t fill, uint8_t outline, uint8_t hi);
/* Inset of a rounded corner of radius r at row i from the edge. */
int  ui_corner_inset(int r, int i);
/* Bytes of s consumed by its first `lines` wrapped lines at width w. */
int  ui_wrap_skip(const char *s, int w, int lines);
/* Shaded drop shadow under a rounded rect. */
void ui_drop_shadow(int x, int y, int w, int h, int r);

#endif
