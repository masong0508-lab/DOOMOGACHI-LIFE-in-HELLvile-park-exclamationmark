/* ui_dialog.c - SCENE_DIALOG: Tomodachi-style typewriter dialogue with the
 * speaker's portrait, a name plate, voice babble, multiple-choice menus
 * and a queue (owner: foundation).
 *
 *   ui_say(id, "Hi!"); ui_say(id, "Want cake?");
 *   ui_choice(id, "Pick one:", opts, 3, on_pick);   // queued after
 *   ui_say_done(on_finished);                         // after the last
 *
 * Long lines are paged automatically. A / B complete the line, then
 * advance. Callbacks run after the dialogue box has closed (when the
 * queue is empty), so they may push scenes.
 */
#include "ui_internal.h"

#define DLG_Q 8
#define DLG_TEXT 200
#define DLG_OPTS 6
#define DLG_OPT_LEN 28

enum { DK_SAY, DK_CHOICE };

typedef struct {
    int16_t speaker;
    uint8_t kind, nopts;
    char text[DLG_TEXT];
    char opts[DLG_OPTS][DLG_OPT_LEN];
    void (*choice_done)(int choice);
    void (*say_done)(void);
} DlgEntry;

static DlgEntry dq[DLG_Q];
static int dq_n;
static int d_page;                     /* byte offset of the current page */
static int d_shown;                    /* chars shown on the page, x2 */
static int d_line_t;                   /* ticks since the page started */
static int d_sel;
static int d_style = WIN_CUTE;
static uint8_t d_open;

void ui_dialog_style(int style) { d_style = style; }
int ui_dialog_active(void) { return d_open; }

static void copy_str(char *dst, const char *src, int cap)
{
    int i;
    if (!src) src = "";
    for (i = 0; i < cap - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

static DlgEntry *enqueue(int speaker, int kind, const char *text)
{
    DlgEntry *e;
    if (dq_n >= DLG_Q) {
        plat_log("ui_say: dialogue queue full, dropping a line");
        return NULL;
    }
    e = &dq[dq_n++];
    memset(e, 0, sizeof(*e));
    e->speaker = (int16_t)speaker;
    e->kind = (uint8_t)kind;
    copy_str(e->text, text, DLG_TEXT);
    if (!d_open) scene_push(&SCENE_DIALOG, 0);
    return e;
}

void ui_say(int speaker, const char *text)
{
    enqueue(speaker, DK_SAY, text);
}

void ui_say_done(void (*done)(void))
{
    if (dq_n > 0) dq[dq_n - 1].say_done = done;
    else if (done) done();
}

void ui_choice(int speaker, const char *question, const char *const *options,
               int count, void (*done)(int choice))
{
    DlgEntry *e = enqueue(speaker, DK_CHOICE, question);
    int i;
    if (!e) return;
    if (count > DLG_OPTS) count = DLG_OPTS;
    if (count < 1) count = 1;
    e->nopts = (uint8_t)count;
    for (i = 0; i < count; i++) copy_str(e->opts[i], options ? options[i] : "OK", DLG_OPT_LEN);
    e->choice_done = done;
}

/* ---------------------------------------------------------------------- */
static int has_portrait(int speaker)
{
    const DoomoResident *r;
    if (speaker < 0) return 0;
    r = town_res(speaker);
    return r && r->status != RES_EMPTY;
}

/* Geometry of the box for the current screen. */
typedef struct { int x, y, w, h, tx, tw, lines; } DlgBox;

static void dlg_box(const DlgEntry *e, DlgBox *b)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    b->lines = sh >= 200 ? 4 : 3;
    b->h = b->lines * FONT_LINE + 13;
    b->x = 4;
    b->w = sw - 8;
    b->y = sh - b->h - 4;
    if (has_portrait(e->speaker)) { b->tx = b->x + 60; b->tw = b->w - 70; }
    else { b->tx = b->x + 10; b->tw = b->w - 20; }
}

static int page_len(const DlgEntry *e, const DlgBox *b)
{
    return ui_wrap_skip(e->text + d_page, b->tw, b->lines);
}

static int page_is_last(const DlgEntry *e, const DlgBox *b)
{
    return e->text[d_page + page_len(e, b)] == 0;
}

static void reset_page(int offset)
{
    d_page = offset;
    d_shown = 0;
    d_line_t = 0;
    d_sel = 0;
}

static void dlg_enter(int arg)
{
    (void)arg;
    d_open = 1;
    reset_page(0);
}

static void dlg_leave(void) { d_open = 0; }

static void advance(int choice)
{
    DlgEntry done = dq[0];
    int i;
    for (i = 1; i < dq_n; i++) dq[i - 1] = dq[i];
    dq_n--;
    reset_page(0);
    if (dq_n == 0) scene_pop();
    if (done.kind == DK_CHOICE && done.choice_done) done.choice_done(choice);
    if (done.say_done) done.say_done();
}

static void dlg_update(void)
{
    DlgEntry *e;
    DlgBox b;
    int len, speed, before;
    if (dq_n == 0) { scene_pop(); return; }
    e = &dq[0];
    dlg_box(e, &b);
    len = page_len(e, &b);
    d_line_t++;
    /* typewriter */
    speed = opt_get(OPT_TEXT_SPEED);
    before = d_shown / 2;
    if (speed >= 3) d_shown = len * 2;
    else d_shown += speed == 0 ? 1 : speed == 1 ? 2 : 4;
    if (d_shown > len * 2) d_shown = len * 2;
    if (opt_get(OPT_VOICE_BABBLE)) {
        static const uint8_t narrator[VOICE_COUNT] = { 16, 18, 10, 16, 8 };
        const DoomoResident *r = has_portrait(e->speaker) ? town_res(e->speaker) : NULL;
        int i;
        for (i = before; i < d_shown / 2 && i < len; i++) {
            char c = e->text[d_page + i];
            if (c != ' ' && c != '\n') audio_voice_char(r ? r->voice : narrator, c);
        }
    }
    if (d_shown / 2 < len) {
        if (btn_pressed(BTN_A | BTN_B)) d_shown = len * 2;
        return;
    }
    if (!page_is_last(e, &b)) {
        if (btn_pressed(BTN_A | BTN_B)) {
            audio_sfx(SFX_BLIP);
            reset_page(d_page + len);
        }
        return;
    }
    if (e->kind == DK_CHOICE) {
        if (btn_repeat(BTN_UP)) { d_sel = (d_sel + e->nopts - 1) % e->nopts; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { d_sel = (d_sel + 1) % e->nopts; audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_A)) { audio_sfx(SFX_SELECT); advance(d_sel); }
        return;
    }
    if (btn_pressed(BTN_A | BTN_B)) {
        audio_sfx(SFX_BLIP);
        advance(0);
    }
}

static void dlg_draw(void)
{
    DlgEntry *e;
    DlgBox b;
    const UiStyle *st;
    int t = (int)scene_ticks(), len, slide, portrait;
    if (dq_n == 0) return;
    e = &dq[0];
    dlg_box(e, &b);
    len = page_len(e, &b);
    portrait = has_portrait(e->speaker);
    /* the box slides up with a little overshoot when the dialogue opens */
    slide = t < 8 ? (8 - t) * (8 - t) / 2 - (t >= 5 ? 2 : 0) : 0;
    b.y += slide;
    ui_window(b.x, b.y, b.w, b.h, portrait ? d_style : WIN_CREAM);
    st = ui_style(portrait ? d_style : WIN_CREAM);
    ui_text_wrap_n(b.tx, b.y + 8, b.tw, e->text + d_page, st->text, d_shown / 2);

    if (portrait) {
        const DoomoResident *r = town_res(e->speaker);
        int talking = d_shown / 2 < len;
        int expr = talking && (d_line_t / 4) % 2 ? EXPR_TALK : EXPR_NEUTRAL;
        int bob = talking ? ((d_line_t / 4) % 2) : (ui_wave(t, 60, 1) > 0);
        int nw = font_text_width(r->name) + 14;
        if (r->status == RES_POSSESSED) expr = EXPR_POSSESSED;
        ui_portrait(b.x + 2, b.y - 22 - bob, e->speaker, 1, expr);
        /* name plate */
        ui_pill_shadow(b.x + 61, b.y - 8, nw, 12, b.x + 60, b.y - 10, nw, 12);
        ui_pill(b.x + 60, b.y - 10, nw, 12, st->title_bg, st->outline, ui_lighter(st->title_bg));
        ui_text_shadow(b.x + 67, b.y - 8, r->name, st->title, st->outline);
    }

    if (d_shown / 2 >= len) {
        if (!page_is_last(e, &b) || e->kind == DK_SAY) {
            int bx = b.x + b.w - 12, by = b.y + b.h - 10 + (ui_wave(t, 18, 2) > 0);
            ui_heart(bx, by, 3, st->accent);
        }
        if (page_is_last(e, &b) && e->kind == DK_CHOICE) {
            int i, ow = 0, cw, ch, cx, cy;
            for (i = 0; i < e->nopts; i++)
                ow = DOOMO_MAX(ow, font_text_width(e->opts[i]));
            cw = ow + 28;
            ch = e->nopts * UI_LIST_ROW_H + 10;
            cx = b.x + b.w - cw - 2;
            cy = b.y - ch - 4;
            if (cy < 2) cy = 2;
            if (ui_window_pop(cx, cy, cw, ch, d_style, d_line_t - len / 2)) {
                const UiStyle *cs = ui_style(d_style);
                for (i = 0; i < e->nopts; i++) {
                    int ry = cy + 5 + i * UI_LIST_ROW_H;
                    if (i == d_sel) {
                        ui_pill(cx + 13, ry, cw - 18, UI_LIST_ROW_H - 1, cs->accent_d,
                                ui_darker(cs->accent_d), ui_lighter(cs->accent_d));
                        ui_cursor(cx + 4, ry + 2);
                    }
                    ui_text(cx + 17, ry + 2, e->opts[i], cs->text);
                }
            }
        }
    }
}

const DoomoScene SCENE_DIALOG = { "DIALOG", dlg_enter, dlg_leave, dlg_update, dlg_draw, 1 };
