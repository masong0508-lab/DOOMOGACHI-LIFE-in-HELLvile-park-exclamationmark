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
/* half-width of row y of a filled circle of radius r (ui_fill_circle) and
 * the rounded-corner band tables (const lookup tables in ui_raster.c,
 * exposed for the tests) */
int  ui_circle_dx(int r, int y);
int  ui_corner_bands(int r, const uint8_t **len, const uint8_t **in);
/* Only rows [row0, row1) of ui_round_rect(x, y, w, h, r, c). */
void ui_round_rect_rows(int x, int y, int w, int h, int r, uint8_t c, int row0, int row1);
/* Shaded drop shadow under a rounded rect. */
void ui_drop_shadow(int x, int y, int w, int h, int r);
/* ui_drop_shadow(sx, sy, w, h, 4) behind a ui_pill(px, py, pw, ph, ...)
 * that is drawn right afterwards (only the visible part is shaded). */
void ui_pill_shadow(int sx, int sy, int w, int h, int px, int py, int pw, int ph);
/* The same shadow minus what the opaque rounded rect (ox, oy, ow, oh, orr)
 * covers; for callers that paint exactly that rect right afterwards. */
void ui_drop_shadow_under(int sx, int sy, int w, int h, int r,
                          int ox, int oy, int ow, int oh, int orr);
/* plat_shade_rect(whole screen, level) minus the opaque rounded rect
 * (ox, oy, ow, oh, orr) the caller paints right afterwards. */
void ui_shade_screen_under(int level, int ox, int oy, int ow, int oh, int orr);
/* Dim the whole screen (modals) except what ui_window_pop(x, y, w, h, *,
 * ticks), drawn right afterwards, covers. */
void ui_dim_under_window_pop(int x, int y, int w, int h, int ticks, int level);
/* Five shades around a fill colour: outline, shadow, base, light, glint. */
void ui_ramp5(uint8_t fill, uint8_t pal[5]);
/* 7x7 pixel glyph: rows of 'o' 's' 'b' 'l' 'w' (pal[0..4]), '.' clear. */
void ui_glyph7(int x, int y, const char *const rows[7], const uint8_t pal[5]);

/* ---- ui_raster.c: span rasteriser --------------------------------------
 * Run emitter: ur_begin(); ur_run(y, x, w, c)...; ur_end(). Rows in
 * increasing order, runs of a row left to right and never overlapping;
 * identical runs of consecutive rows are merged into one plat_rect. */
#define UR_MASK_MAX 72
void ur_begin(void);
void ur_run(int y, int x, int w, uint8_t c);
void ur_end(void);
/* plat_rect, or recorded while a shape is being cached */
void ur_rect(int x, int y, int w, int h, uint8_t c);
/* Shape cache (rect lists relative to an anchor, keyed by three words):
 *   if (!ur_cache_draw(k0, k1, k2, ax, ay)) {
 *       ur_cache_begin(k0, k1, k2, ax, ay);  draw via ur_run / ur_rect;
 *       if (!ur_cache_end()) draw again directly;   }                     */
/* k0 & 255 is the shape kind; kinds below 16 belong to ui_raster.c */
enum { UR_K_PILL = 16 };
int  ur_cache_draw(uint32_t k0, uint32_t k1, uint32_t k2, int ax, int ay);
void ur_cache_begin(uint32_t k0, uint32_t k1, uint32_t k2, int ax, int ay);
int  ur_cache_end(void);
/* rects in use, entries, and lookup / reset counters (tests, tuning) */
void ur_cache_stats(int *used, int *entries, uint32_t *hits, uint32_t *misses,
                    uint32_t *resets);
/* rects replayed, shape images built and drawn (PS1 / native: a shape
 * drawn two frames running becomes one plat_image) */
void ur_cache_image_stats(uint32_t *replayed, uint32_t *built, uint32_t *drawn);
/* 1 if ui_heart_flat(cx, cy, s, ..) paints every pixel of the screen */
int  ui_heart_flat_covers_screen(int cx, int cy, int s);
/* ui_text_big through bit rows; 0 = too big, draw it the slow way. */
int  ur_text_big(int x, int y, const char *s, int scale, uint8_t color, uint8_t outline);

#endif
