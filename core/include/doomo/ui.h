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
/* --- CL1 (clarity C1 + C5, additive): toast kinds, lanes, unlock news. ---
 * Kinds, lowest first. The queue is ordered by kind (a newer toast of a higher kind goes ahead of the waiting
 * ones of lower kinds, never in front of the one on screen), so "Saved!" jumps a waiting hint and a hint jumps
 * unlock news. ui_toast() is UI_TOAST_FEEDBACK.
 *   UNLOCK    unlock news (ui_toast_unlock builds it; never queued by hand)
 *   HINT      how to use the screen it was posted on ("A visits a place"): dropped when that screen goes
 *   FEEDBACK  what the player just did or what just happened (bought, moved in, rent)
 *   SYSTEM    saves and errors: kept even across a scene_set
 * A scene change (scene.c) drops the toasts that belong to the old screen: scene_set drops every toast but
 * SYSTEM ones, and a scene_push / scene_pop of a full screen (not an overlay) drops HINT ones. A toast posted on
 * the tick of the change belongs to the new screen and stays ("Welcome back!" then scene_set). */
enum { UI_TOAST_UNLOCK, UI_TOAST_HINT, UI_TOAST_FEEDBACK, UI_TOAST_SYSTEM, UI_TOAST_KINDS };
void ui_toast_kind(const char *msg, int kind);
#define UI_TOAST_MAX 55                 /* the characters a toast line holds */
/* Unlock news (unlocks.c unlock_grant): name is the unlock, what the kind of thing it is ("Creator outfit",
 * "paint", NULL: unknown; unlocks.h unlock_kind_word). Both are kept, not copied, until the news is told (the line
 * is written then, up to UI_TOAST_MAX characters), so pass static text such as the unlock tables' strings, never a
 * buffer. News waits until nothing else is queued, no dialogue is up and no other unlock came for a moment, so a
 * burst is told once: one unlock says "New paint: Lemon", more say "3 new unlocks! See Unlocks". The first news of
 * a session gathers for 20 s from its first unlock (the opening minute of a new island tells one digest), and
 * after unlock news has been shown the next gathers for 40 s (UL_NEWS_* in ui.c). */
void ui_toast_unlock(const char *name, const char *what);
/* The toast lane (C1): a screen with a HUD band says, on each frame before the HUD is drawn, which row the
 * toast may use (its top edge, y); the lane goes back to the default (the top edge, y 4) after the toast is
 * drawn. ui_toast_over() asks about the toast at this frame's lane. */
#define UI_TOAST_LANE_TOP 4
#define UI_TOAST_H 17                   /* the toast's rows from its lane: the pill and its drop shadow */
void ui_toast_lane(int y);
/* The scene stack changed (scene.c): set = a scene_set, else a push / pop of a full screen. */
void ui_toast_scene_change(int set);
/* (tests, runner) The rect of the toast the last frame drew (0 if none); toasts shown so far, per kind (a toast
 * counts once, when it starts to slide in); the text of queued toast i (0 = on screen) or NULL; unlock news
 * waiting to be told. */
int  ui_toast_last_rect(int *x, int *y, int *w, int *h);
int  ui_toast_shown(int kind);
void ui_toast_shown_reset(void);
const char *ui_toast_text(int i);
int  ui_toast_news_waiting(void);
/* --- end CL1 --- */
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
/* shade the screen at `level` everywhere but under the opaque window that
 * ui_window_pop(x, y, w, h, .., ticks) draws on the same frame (same pop
 * scale); call it BEFORE the window */
void ui_dim_under_window_pop(int x, int y, int w, int h, int ticks, int level);
/* cut every UI shade rect to a rect until _off (the held pause frame, P2/P3) */
void ui_shade_clip(int x, int y, int w, int h);
void ui_shade_clip_off(void);

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
/* (SP4) only rows [row0, row1) of it (also in ui_internal.h) */
void ui_round_rect_rows(int x, int y, int w, int h, int r, uint8_t c, int row0, int row1);
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
/* the same with a hole an opaque window will cover afterwards: nothing wholly inside it is drawn (hx0 >= hx1: none) */
void ui_backdrop_hole(int style, uint32_t t, int hx0, int hy0, int hx1, int hy1);
/* ... with n holes (holes[4 * i ..]: x0, y0, x1, y1 each; the PS1 cuts only the first: same pixels) */
void ui_backdrop_holes(int style, uint32_t t, const int *holes, int n);

/* ---- text extras ------------------------------------------------------- */
/* Width of ui_text() output (button icons are drawn as 7 px caps). */
int  ui_text_width(const char *s);
/* Text that is already translated (lang_tr), clipped to [x0, x1): the glyphs
 * that fit whole, one plat_text per run between blanks. fp_marquee and the
 * toast scroll through it. */
void ui_text_clip(int x, int y, const char *s, uint8_t color, int x0, int x1);
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
/* "[A] Select  [B] Back" hint pill at the bottom edge of the screen (a
 * retained panel, PANEL_PROMPT). Never wider than the screen: a text too
 * wide for it loses its last words (and the native build counts it, see
 * ui_prompt_stats). New screens use ui_prompts() instead. */
void ui_hint(const char *s, int style);

/* ---- C3: the shared prompt bar -------------------------------------------
 * One look, one verb style ("A Open", "B Back": the button, then one
 * capitalised verb) and button names that fit the pad in the player's
 * hands (input.h btn_name: no X / Y / R1 on a GBA). `btn` is a BTN_* mask:
 *   BTN_A, BTN_B             drawn as the round A / B caps
 *   BTN_LEFT | BTN_RIGHT     the left-right arrows (UP | DOWN, one arrow,
 *                            all four: the arrows)
 *   BTN_L | BTN_R            "L/R" ("L1/R1" on a PS1)
 *   BTN_X, BTN_Y, START...   btn_name() ("SEL+A" cap on a GBA)
 *   BTN_SELECT | another     a chord: "SEL+" and the other button
 *   0                        the verb alone (a short status word)
 * An item whose button this pad lacks (L2 on a GBA) is left out. `prio`
 * 0 = always shown; when the bar is too wide for the screen the single
 * spaces between items are tried first, then the item with the highest
 * prio (the later one on a tie) is dropped, and so on. Verbs are English
 * and translated here, one by one. */
typedef struct UiPrompt {
    uint16_t btn;
    uint8_t  prio;
    const char *verb;
} UiPrompt;
#define UI_PROMPTS_MAX 8
/* the widest text a bar may have on this screen (the pill adds 12 px) */
int  ui_prompts_maxw(void);
/* The bar's text (translated, button glyphs in it) into out[n]; returns its
 * width in pixels (> maxw only when even the prio-0 items do not fit). */
int  ui_prompts_text(char *out, int n, const UiPrompt *p, int count, int maxw);
/* The last resort when even that is too wide (and for ui_hint): t (text
 * already translated) cut in place to its longest run of whole words that
 * is at most maxw wide; a first word wider than maxw keeps the glyphs that
 * fit. t is left as it is when it fits. */
void ui_prompt_cut(char *t, int maxw);
void ui_prompts(const UiPrompt *p, int count, int style);
/* the same bar centred at row y, its text at most maxw wide (a screen with
 * something of its own at the bottom edge: the island hub's news ticker) */
void ui_prompts_at(const UiPrompt *p, int count, int style, int y, int maxw);
/* Native checks (core/tests/test_prompts.c, runner --check-prompts): every
 * bar drawn since the last reset; too_wide = cut to fit, wrong_pad = a
 * button word the pad does not have (X, Y, L1, R1, L2, R2, Square,
 * Triangle, DualShock on a GBA). Consoles keep no stats (all 0). */
typedef struct UiPromptStats {
    uint32_t bars, too_wide, wrong_pad;
    char last[100], first_bad[100];
} UiPromptStats;
void ui_prompt_stats(UiPromptStats *out, int reset);
/* Title plate (pill with outline) centred at cx. */
void ui_title(int cx, int y, const char *s, int style);
/* Pill-shaped button; selected ones bounce and glow. */
void ui_button(int x, int y, int w, const char *label, int selected, int style);
/* The same row greyed out: shown but not available yet (CL2). */
void ui_button_off(int x, int y, int w, const char *label, int selected, int style);
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
/* The top line: its DL_* flags, glyphs shown and a serial that changes
 * with every line (the spawner's pet "beep." on a sincere line). 0 if none. */
typedef struct UiDialogTop { int flags, shown; uint16_t serial; } UiDialogTop;
int  ui_dialog_top(UiDialogTop *out);
/* SP5: the band the dialogue draws in (ui_dialog.c): rows [y_need, screen h) with the opaque box body
 * [cx0, cx1) x [cy0, cy1) inside; 0 while the box slides in or nothing is shown. */
int  ui_dialog_band(int *y_need, int *cx0, int *cy0, int *cx1, int *cy1);
/* Dialogue box style (default WIN_CUTE; HELL after the invasion). */
void ui_dialog_style(int style);
/* Pending toasts (for tests). */
int  ui_toast_pending(void);
void ui_toast_clear(void);
/* 1 when the toast drawn later this frame can touch the rect (generous; ui.c). */
int  ui_toast_over(int x, int y, int w, int h);
/* Nonzero while the first toast holds perfectly still (slid in, text not scrolling, not yet sliding out): its pixels
 * cannot change, except the sparkle, until the value does. The value names the toast (it changes when the queue
 * replaces the line), so a scene that kept a frame with the toast on it can tell it is still the same toast. */
int  ui_toast_steady(void);
/* A scene that kept the last frame, toast included (docs/ISLAND_SPEED.md I8), asks for the toast not to be drawn
 * over it again this frame: its timer still runs (ui.c ui_toast_draw). Only while ui_toast_steady() is nonzero. */
void ui_toast_hold(void);
/* (SP3, docs/ISLAND_SPEED.md I11) the rows any toast can touch at this frame's lane (call it after ui_toast_lane),
 * wherever it is in its slide (its box, drop shadow and sparkle): [*y, *y + *h). A scene that holds its frame keeps
 * a clean copy of them (plat_ext.h plat_strip). */
void ui_toast_band(int *y, int *h);

/* Retained panels (G7, platform.h plat_panel_begin): a panel's key starts
 * from ui_panel_key() (the current language and its glyph page, so a
 * language switch never shows a kept panel) and mixes in everything else
 * its pixels depend on with ui_key_int() / ui_key_str() (the text as it
 * will be drawn, its translation included). */
uint32_t ui_panel_key(void);
/* A platform that keeps no panels (the PS1: platform.h's weak default)
 * builds none of the keys: the core tests UI_PANELS around them. */
#if defined(DOOMO_PLATFORM_PS1)
#define UI_PANELS 0
#else
#define UI_PANELS 1
#endif
uint32_t ui_key_int(uint32_t h, int32_t v);
uint32_t ui_key_str(uint32_t h, const char *s);
/* Where ui_window(x, y, w, h)'s sliding glint is this frame, -1 = none. */
int ui_window_glint(int x, int y, int w, int h);
/* CR9: ui_glint_hold(1): windows drawn until ui_glint_hold(0) have no sliding glint (a screen whose
 * windows are retained panels: the GBA creator) */
void ui_glint_hold(int on);
/* The drop shadows of ui_window() and ui_portrait() alone: what a kept
 * panel draws (platform.h plat_panel_begin). */
void ui_window_shadow(int x, int y, int w, int h);
/* ui_window()'s body pixels in an inner rect (see ui.c), for a panel hole */
void ui_window_rows(int x, int y, int w, int h, int style, int cx0, int cx1, int ry0, int ry1);
void ui_portrait_shadow(int x, int y, const DoomoImage *img, int scale);
void ui_portrait_img(int x, int y, const DoomoImage *img, int scale);
int ui_sparkle_size(int phase);          /* ui_sparkle's size, 0 = none */

/* Shared pause menu (combat, levels, arena, minigames): a "PAUSED" window
 * of n button rows, the same look, cursor and hints everywhere. Rows in
 * lr_mask (bit i) show < > arrows when selected (left/right changes them).
 * pause_menu_nav() moves *sel with up/down (wrapping) and returns 1 when it
 * moved. Options rows open options_open() (town.h). */
void pause_menu_draw(const char *const *labels, int n, int sel, unsigned lr_mask, int t, int style);
int  pause_menu_nav(int *sel, int n);
/* U3a, the shell's quick Music row: every pause menu puts it right under Resume and lets LEFT / RIGHT
 * change OPT_MUSIC_VOL on the spot (no trip through Options). pause_music_label() writes "Music volume 7"
 * ("Music: off" at 0); pause_music_nudge() moves the volume one step (clamped, plays a cursor tick) and
 * returns 1 when it changed. Keep the row in the menu's lr_mask. */
const char *pause_music_label(char *buf, int n);
int  pause_music_nudge(int dir);

#endif
