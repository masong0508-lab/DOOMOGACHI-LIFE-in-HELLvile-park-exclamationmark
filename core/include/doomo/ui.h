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
/* On-screen keyboard (D-pad grid: upper / lower / symbols pages). maxlen
 * is the size of buf in bytes INCLUDING the NUL (DOOMO_NAME_LEN for
 * resident names). buf is only written when the player confirms. */
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

/* ======================================================================
 * foundation additions (additive; every 2D screen may use these)
 * ==================================================================== */
#include <stdarg.h>
int  doomo_vsnprintf(char *buf, int cap, const char *fmt, va_list ap);

/* ---- style colours ---------------------------------------------------- */
/* Resolved colours of a window style (WIN_CUTE follows the player's
 * OPT_WINDOW_SKIN choice). */
typedef struct UiStyle {
    uint8_t fill, fill_hi, fill_lo;   /* body: base, top band, bottom band  */
    uint8_t outline;                  /* coloured outline (dark local hue)  */
    uint8_t gloss;                    /* glossy highlight line              */
    uint8_t text, text_dim;           /* body text / secondary text         */
    uint8_t accent, accent_d;         /* cursor, selection pill             */
    uint8_t title, title_bg;          /* name plates / headings             */
} UiStyle;
const UiStyle *ui_style(int style);
uint8_t ui_style_text(int style);
/* Window skins selectable for WIN_CUTE (OPT_WINDOW_SKIN values). */
enum { WSKIN_STRAWBERRY, WSKIN_MINT_CHOC, WSKIN_VANILLA, WSKIN_LAVENDER,
       WSKIN_SKY, WSKIN_PEACH, WSKIN_LEMON, WSKIN_BUBBLEGUM, WSKIN_HELLFIRE,
       WSKIN_MIDNIGHT, WSKIN_TAMAGOTCHI, WSKIN_GOTH, WSKIN_COUNT };

/* One shade lighter / darker inside a colour's own family (works without
 * the art tables; paints, skins, ramps and UI colours). */
uint8_t ui_lighter(uint8_t c);
uint8_t ui_darker(uint8_t c);

/* ---- animation helpers ------------------------------------------------- */
/* Pop-in scale in 1/256 for `ticks` since appearing: anticipation dip,
 * overshoot (~115%), settle at 256 after ~12 ticks. */
int  ui_pop_scale(int ticks);
/* Integer sine wave: amplitude * sin(t * 4096 / period). */
int  ui_wave(int t, int period, int amplitude);
/* Window that boings in: drawn scaled about its centre while ticks < 12.
 * Returns 1 once fully open (draw the contents then). */
int  ui_window_pop(int x, int y, int w, int h, int style, int ticks);

/* ---- primitives (pure plat_rect spans, clipped) ------------------------- */
/* (integration) Offscreen target: while set, the rects of the primitives
 * below (and of every shape built from them) are written into pix (w x h
 * 8bpp, its top-left at screen (x0, y0), clipped; colour 0 becomes
 * PAL_BLACK, as on the GBA) instead of being drawn. pix == NULL only
 * measures: ui_raster_bounds() then returns their bounding box [x0, x1) x
 * [y0, y1) (0 if nothing was drawn). Used to pre-render static sprites
 * (the free play island's buildings) into the scene scratch. */
void ui_raster_target(uint8_t *pix, int x0, int y0, int w, int h);
void ui_raster_target_off(void);
int  ui_raster_bounds(int *x0, int *y0, int *x1, int *y1);
void ui_fill_circle(int cx, int cy, int r, uint8_t c);
void ui_fill_ellipse(int cx, int cy, int rx, int ry, uint8_t c);
void ui_round_rect(int x, int y, int w, int h, int r, uint8_t c);
void ui_frame(int x, int y, int w, int h, uint8_t c);   /* 1 px outline */
/* Shaded, outlined heart centred at cx,cy; size = half-width in px (>=2). */
void ui_heart(int cx, int cy, int size, uint8_t fill);
/* Same with squash & stretch: sx256/sy256 scale x/y (256 = 1.0). */
void ui_heart_squash(int cx, int cy, int size, int sx256, int sy256, uint8_t fill);
/* Unshaded heart of any size (iris wipes, silhouettes). */
void ui_heart_flat(int cx, int cy, int size, uint8_t c);
/* Shaded 5-point star; r = outer radius. */
void ui_star(int cx, int cy, int r, uint8_t fill);
/* Twinkling 4-point sparkle; phase 0..15 drives its size. */
void ui_sparkle(int cx, int cy, int phase, uint8_t c);
/* Animated backdrop behind menus: soft gradient bands, drifting polka
 * dots, rising hearts and stars, twinkles (style picks the palette:
 * WIN_CUTE / MINT / CREAM / HELL / LCD / DARK). Call it first. */
void ui_backdrop(int style, uint32_t t);

/* ---- text extras ------------------------------------------------------- */
/* Width of ui_text() output (button icons are drawn as 7 px caps). */
int  ui_text_width(const char *s);
void ui_text_shadow(int x, int y, const char *s, uint8_t color, uint8_t shadow);
void ui_text_outline(int x, int y, const char *s, uint8_t color, uint8_t outline);
/* Big text (integer scale 2..4) with outline + drop shadow, for titles. */
void ui_text_big(int x, int y, const char *s, int scale, uint8_t color,
                 uint8_t outline);
int  ui_text_big_width(const char *s, int scale);
/* Measure / draw only the first `chars` characters of a wrapped text
 * (typewriter); returns lines. */
int  ui_text_wrap_n(int x, int y, int w, const char *s, uint8_t color,
                    int chars);
/* Bytes of s consumed by its first `lines` wrapped lines at width w
 * (paging long texts). */
int  ui_wrap_skip(const char *s, int w, int lines);
/* "[A] Select  [B] Back" hint pill at the bottom edge of the screen. */
void ui_hint(const char *s, int style);
/* Title plate (pill with outline) centred at cx. */
void ui_title(int cx, int y, const char *s, int style);
/* Pill-shaped button; selected ones bounce and glow. */
void ui_button(int x, int y, int w, const char *label, int selected, int style);
/* "Label        < Value >" row for option / creator screens; the
 * selected row gets a pill, the cursor and bouncing arrows. */
void ui_value_row(int x, int y, int w, const char *label, const char *value,
                  int selected);
/* A row of tab pills (pages, creator categories). The selected tab is
 * raised and bounces. Returns the total width drawn. */
int  ui_tabs(int x, int y, const char *const *names, int count, int sel, int style);
/* Up/down scroll arrows (bouncing) at x, y_top / y_bottom. */
void ui_scroll_arrows(int x, int y_top, int y_bottom, int up, int down);

/* ---- list / picker metrics --------------------------------------------- */
#define UI_LIST_ROW_H 12      /* list height = visible * UI_LIST_ROW_H       */
#define UI_SWATCH_W 12        /* paint picker cell: grid = cols * W ...      */
#define UI_SWATCH_H 11        /* ... by ceil(32 / cols) * H                  */
/* Optional per-item lock test used by ui_list_draw (locked rows are drawn
 * greyed with a padlock); NULL = none. */
extern int (*ui_list_locked_fn)(int index);

/* ---- modal extras ------------------------------------------------------ */
/* Type one character into the active text entry as if picked on the
 * keyboard ('\b' = delete, '\n' = OK). Used by the native runner's
 * `type` command and by platforms with a keyboard. 0 if none active. */
int  ui_text_entry_feed(int c);
int  ui_dialog_active(void);          /* SCENE_DIALOG is open            */
/* ---- S1 additions (comic-timing dialogue; SPEC sec.7 S1 M3, SF sec.15.6) --
 * ui_say_ex: a story line with an expression (EXPR_*), DL_* flags (txt.h)
 * and the beat codes / "/" line and "//" page breaks of ui_dialog.c.
 * ui_dialog_queued: lines waiting (the VM paces itself on it).
 * ui_dialog_insert: the {cut N} insert being shown (-1 none) and its age,
 * for the host scene to draw. */
void ui_say_ex(int speaker, int expr, int flags, const char *text);
int  ui_dialog_queued(void);
int  ui_dialog_insert(int *age);
/* Dialogue box style (default WIN_CUTE; HELL after the invasion). */
void ui_dialog_style(int style);
/* Pending toasts (for tests). */
int  ui_toast_pending(void);
void ui_toast_clear(void);

#endif
