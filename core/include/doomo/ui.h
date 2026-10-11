/* ui.h - the cutesy UI kit shared by every 2D screen (owner: foundation).
 * Pastel rounded windows, bouncy cursors, typewriter dialogue with voice
 * babble. All functions draw immediately via plat_*. Layouts must adapt to
 * plat_screen_w()/h() (240x160 GBA, 320x240 PS1).
 */
#ifndef DOOMO_UI_H
#define DOOMO_UI_H

#include "types.h"

enum { UI_NONE = 0, UI_SELECT, UI_CANCEL, UI_CHANGED, UI_LEFT, UI_RIGHT };

/* Window styles. CUTE is the default pastel look; HELL is used once the
 * invasion starts (and in Hell Breach levels); LCD is the tamagotchi look. */
enum { WIN_CUTE, WIN_MINT, WIN_CREAM, WIN_HELL, WIN_LCD, WIN_DARK };

void ui_window(int x, int y, int w, int h, int style);
void ui_text(int x, int y, const char *s, uint8_t color);
void ui_text_center(int cx, int y, const char *s, uint8_t color);
void ui_textf(int x, int y, uint8_t color, const char *fmt, ...);
/* Word-wrap into width w; returns number of lines drawn. If draw==0 only
 * measures. */
int  ui_text_wrap(int x, int y, int w, const char *s, uint8_t color, int draw);
/* Horizontal meter. */
void ui_bar(int x, int y, int w, int h, int value, int max, uint8_t fill);
/* Labelled slider row "Eye size  <=====o===>  7". */
void ui_slider(int x, int y, int w, const char *label, int value, int max,
               int selected);
/* Bouncing selection cursor (heart) at x,y. */
void ui_cursor(int x, int y);
/* Colour swatch grid for paint colours; returns UI_* and updates *paint. */
int  ui_paint_picker_update(uint8_t *paint, int cols);
void ui_paint_picker_draw(int x, int y, uint8_t paint, int cols);

/* Scrolling list widget. */
typedef struct UiList {
    const char *const *items;    /* or NULL to use item_fn */
    const char *(*item_fn)(int index, char *buf); /* buf has 48 bytes */
    int count, sel, top, visible;
} UiList;
int  ui_list_update(UiList *l);         /* UI_NONE/SELECT/CANCEL/CHANGED */
void ui_list_draw(const UiList *l, int x, int y, int w, int style);

/* Modal scenes (push themselves; callbacks run after they pop). */
void ui_confirm(const char *question, void (*done)(int yes));
void ui_text_entry(const char *prompt, char *buf, int maxlen,
                   void (*done)(int ok));
/* Typewriter dialogue box with speaker portrait + voice babble.
 * speaker = resident id, or -1 for narrator. Multiple calls queue up.
 * done (may be NULL) runs after the last queued line is dismissed. */
void ui_say(int speaker, const char *text);
void ui_say_done(void (*done)(void));
/* Multiple choice after a line of dialogue. */
void ui_choice(int speaker, const char *question, const char *const *options,
               int count, void (*done)(int choice));
/* Small transient message in the corner. */
void ui_toast(const char *msg);
void ui_toast_draw(void);               /* called by doomo_frame */
/* Draw the resident's portrait (portrait.h) at x,y with optional frame. */
void ui_portrait(int x, int y, int resident_id, int scale, int expression);

/* Tiny printf: %d %i %u %x %s %c %% with width/zero-pad (%3d, %02d). */
int  doomo_snprintf(char *buf, int cap, const char *fmt, ...);

#endif
