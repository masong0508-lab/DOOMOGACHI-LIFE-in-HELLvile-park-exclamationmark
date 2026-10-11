/* ui_modal.c - SCENE_CONFIRM (yes/no) and SCENE_TEXT_ENTRY (on-screen
 * keyboard) (owner: foundation). Both are overlay scenes: the screen below
 * stays visible, dimmed. Their callbacks run after the modal has popped,
 * so a callback may push another scene.
 */
#include "ui_internal.h"

static void dim_below(void)
{
    plat_shade_rect(0, 0, plat_screen_w(), plat_screen_h(), 11);
}

/* ====================================================================== */
/* confirm                                                                  */
static char cf_question[160];
static void (*cf_done)(int yes);
static int cf_sel;                     /* 0 = Yes, 1 = No (default) */

void ui_confirm(const char *question, void (*done)(int yes))
{
    doomo_snprintf(cf_question, sizeof(cf_question), "%s", question ? question : "Are you sure?");
    cf_done = done;
    scene_push(&SCENE_CONFIRM, 0);
}

static void confirm_enter(int arg)
{
    (void)arg;
    cf_sel = 1;
}

static void confirm_finish(int yes)
{
    void (*done)(int) = cf_done;
    cf_done = NULL;
    scene_pop();
    if (done) done(yes);
}

static void confirm_update(void)
{
    if (btn_repeat(BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) {
        cf_sel ^= 1;
        audio_sfx(SFX_CURSOR);
    }
    if (btn_pressed(BTN_A)) {
        audio_sfx(cf_sel == 0 ? SFX_SELECT : SFX_CANCEL);
        confirm_finish(cf_sel == 0);
    } else if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        confirm_finish(0);
    }
}

static void confirm_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    int w = DOOMO_MIN(sw - 32, 220), x = (sw - w) / 2;
    int lines = ui_text_wrap(0, 0, w - 20, cf_question, 0, 0);
    int h = lines * FONT_LINE + 44, y = (sh - h) / 2 - 4;
    int t = (int)scene_ticks();
    dim_below();
    if (!ui_window_pop(x, y, w, h, WIN_CUTE, t)) return;
    {
        const UiStyle *st = ui_style(WIN_CUTE);
        int bw = 56, gap = 18, bx = sw / 2 - bw - gap / 2;
        ui_heart(x + 4, y + 3, 4, PAL_PAINT(PAINT_ROSE, 1));
        ui_sparkle(x + w - 7, y + 6, (t / 3) & 15, PAL_UI_GOLD);
        ui_text_wrap(x + 12, y + 11, w - 24, cf_question, st->text, 1);
        ui_button(bx, y + h - 22, bw, "Yes", cf_sel == 0, WIN_CUTE);
        ui_button(bx + bw + gap, y + h - 22, bw, "No", cf_sel == 1, WIN_CUTE);
    }
}

const DoomoScene SCENE_CONFIRM = { "CONFIRM", confirm_enter, NULL, confirm_update,
                                   confirm_draw, 1 };

/* ====================================================================== */
/* text entry                                                               */
#define KB_COLS 10
#define KB_ROWS 5                      /* 4 character rows + 1 special row */
#define TE_MAX 64

static const char *const kb_pages[3][4] = {
    { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ.,!?", "0123456789" },
    { "abcdefghij", "klmnopqrst", "uvwxyz.,!?", "0123456789" },
    { "!?.,:;'\"-_", "+=*/#%&@~^", "()<>[]{}$|", "0123456789" }
};
static const char *const kb_page_names[3] = { "ABC", "abc", "#!?" };
/* special row: 5 keys, each 2 columns wide */
enum { KS_SHIFT, KS_SYMBOLS, KS_SPACE, KS_DELETE, KS_OK };
static const char *const kb_special[5] = { "Aa", "#!?", "Space", "Del", "OK" };

static char te_prompt[48];
static char *te_buf;
static int te_cap;                     /* bytes in te_buf including the NUL */
static char te_edit[TE_MAX];
static int te_len, te_page, te_row, te_col;
static void (*te_done)(int ok);
static uint8_t te_active;
static int te_flash;                   /* ticks of key-press flash */

void ui_text_entry(const char *prompt, char *buf, int maxlen, void (*done)(int ok))
{
    int i;
    doomo_snprintf(te_prompt, sizeof(te_prompt), "%s", prompt ? prompt : "Type!");
    te_buf = buf;
    te_cap = DOOMO_CLAMP(maxlen, 1, TE_MAX);
    te_done = done;
    te_len = 0;
    if (buf) {
        for (i = 0; i < te_cap - 1 && buf[i]; i++) te_edit[i] = buf[i];
        te_len = i;
    }
    te_edit[te_len] = 0;
    scene_push(&SCENE_TEXT_ENTRY, 0);
}

static void te_enter(int arg)
{
    (void)arg;
    te_active = 1;
    te_page = 0;
    te_row = 0;
    te_col = 0;
    te_flash = 0;
}

static void te_leave(void) { te_active = 0; }

static void te_finish(int ok)
{
    void (*done)(int) = te_done;
    if (ok && te_buf) {
        int i;
        for (i = 0; i < te_len && i < te_cap - 1; i++) te_buf[i] = te_edit[i];
        for (; i < te_cap; i++) te_buf[i] = 0;
    }
    te_done = NULL;
    scene_pop();
    if (done) done(ok);
}

static void te_type(int c)
{
    if (c == '\b') {
        if (te_len > 0) { te_edit[--te_len] = 0; audio_sfx(SFX_CANCEL); }
        return;
    }
    if (c < 0x20 || c > 0x7E) return;
    if (te_len >= te_cap - 1) { audio_sfx(SFX_ERROR); return; }
    te_edit[te_len++] = (char)c;
    te_edit[te_len] = 0;
    /* after a capital at the start of a word, switch to lowercase */
    if (te_page == 0 && c >= 'A' && c <= 'Z' && te_len == 1) te_page = 1;
    audio_sfx(SFX_BLIP);
}

int ui_text_entry_feed(int c)
{
    if (!te_active || scene_top() != &SCENE_TEXT_ENTRY) return 0;
    if (c == '\n') { te_finish(1); return 1; }
    te_type(c);
    te_flash = 4;
    return 1;
}

static void te_press_key(void)
{
    te_flash = 4;
    if (te_row < 4) {
        te_type(kb_pages[te_page][te_row][te_col]);
        return;
    }
    switch (te_col / 2) {
    case KS_SHIFT: te_page = te_page == 0 ? 1 : 0; audio_sfx(SFX_BLIP); break;
    case KS_SYMBOLS: te_page = te_page == 2 ? 0 : 2; audio_sfx(SFX_BLIP); break;
    case KS_SPACE: te_type(' '); break;
    case KS_DELETE: te_type('\b'); break;
    default:
        audio_sfx(SFX_SELECT);
        te_finish(1);
        break;
    }
}

static void te_update(void)
{
    int moved = 0;
    if (te_flash > 0) te_flash--;
    if (btn_repeat(BTN_LEFT)) { te_col = (te_col + KB_COLS - 1) % KB_COLS; moved = 1; }
    if (btn_repeat(BTN_RIGHT)) { te_col = (te_col + 1) % KB_COLS; moved = 1; }
    if (btn_repeat(BTN_UP)) { te_row = (te_row + KB_ROWS - 1) % KB_ROWS; moved = 1; }
    if (btn_repeat(BTN_DOWN)) { te_row = (te_row + 1) % KB_ROWS; moved = 1; }
    if (moved) audio_sfx(SFX_CURSOR);
    if (btn_pressed(BTN_L | BTN_R | BTN_SELECT)) {
        te_page = (te_page + (btn_pressed(BTN_L) ? 2 : 1)) % 3;
        audio_sfx(SFX_BLIP);
    }
    if (btn_pressed(BTN_A)) te_press_key();
    else if (btn_pressed(BTN_START)) { audio_sfx(SFX_SELECT); te_finish(1); }
    else if (btn_repeat(BTN_B)) {
        if (te_len == 0 && btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); te_finish(0); }
        else te_type('\b');
    }
}

static void te_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), t = (int)scene_ticks();
    int kw = sw >= 300 ? 24 : 21, kh = sw >= 300 ? 18 : 14;
    int w = KB_COLS * kw + 16, h = KB_ROWS * kh + 56;
    int x = (sw - w) / 2, y = (sh - h) / 2 + 2;
    const UiStyle *st;
    int r, c, fx, fy, fw;
    char counter[16];
    if (h > sh - 4) { h = sh - 4; y = 2; }
    dim_below();
    if (!ui_window_pop(x, y, w, h, WIN_CUTE, t)) return;
    st = ui_style(WIN_CUTE);
    ui_title(sw / 2, y - 5, te_prompt, WIN_MINT);

    /* the text field */
    fx = x + 8; fy = y + 12; fw = w - 16;
    ui_round_rect(fx, fy, fw, 15, 3, st->outline);
    ui_round_rect(fx + 1, fy + 1, fw - 2, 13, 2, PAL_WHITE);
    plat_rect(fx + 2, fy + 12, fw - 4, 1, st->fill_hi);
    {
        int vis = (fw - 44) / FONT_ADVANCE, off = te_len > vis ? te_len - vis : 0;
        ui_text(fx + 5, fy + 4, te_edit + off, st->text);
        if ((t / 10) % 2 == 0) {
            int cx = fx + 5 + (te_len - off) * FONT_ADVANCE;
            plat_rect(cx, fy + 3, 1, 9, st->accent);
            plat_rect(cx + 1, fy + 3, 1, 9, ui_lighter(st->accent));
        }
    }
    doomo_snprintf(counter, sizeof(counter), "%d/%d", te_len, te_cap - 1);
    ui_text(fx + fw - 4 - ui_text_width(counter), fy + 4, counter, st->text_dim);

    /* page tabs */
    for (c = 0; c < 3; c++) {
        int tx = x + 8 + c * 30, ty = fy + 18;
        int on = c == te_page;
        ui_pill(tx, ty, 28, 10, on ? st->accent : st->fill_lo, st->outline,
                on ? ui_lighter(st->accent) : st->fill_hi);
        ui_text(tx + 14 - font_text_width(kb_page_names[c]) / 2, ty + 1,
                kb_page_names[c], on ? PAL_WHITE : st->text_dim);
    }
    ui_text(x + w - 8 - font_text_width("L/R page"), fy + 19, "L/R page", st->text_dim);

    /* keys */
    for (r = 0; r < KB_ROWS; r++) {
        int ky = y + 44 + r * kh;
        if (r < 4) {
            for (c = 0; c < KB_COLS; c++) {
                int kx = x + 8 + c * kw;
                int sel = r == te_row && c == te_col;
                char ch[2];
                ch[0] = kb_pages[te_page][r][c];
                ch[1] = 0;
                if (sel) {
                    int b = te_flash ? 1 : -(ui_wave(t, 16, 1) > 0);
                    ui_pill(kx, ky + b, kw - 2, kh - 2, st->accent,
                            ui_darker(ui_darker(st->accent)), ui_lighter(st->accent));
                    ui_text(kx + (kw - 2) / 2 - 2, ky + b + (kh - 2) / 2 - 4, ch, PAL_WHITE);
                } else {
                    ui_pill(kx, ky, kw - 2, kh - 2, st->fill_hi, st->fill_lo, PAL_WHITE);
                    plat_rect(kx + 2, ky + kh - 3, kw - 6, 1, st->fill_lo);
                    ui_text(kx + (kw - 2) / 2 - 2, ky + (kh - 2) / 2 - 4, ch, st->text);
                }
            }
        } else {
            for (c = 0; c < 5; c++) {
                int kx = x + 8 + c * 2 * kw;
                int sel = r == te_row && c == te_col / 2;
                int bw = 2 * kw - 2;
                const char *lbl = kb_special[c];
                uint8_t fill = c == KS_OK ? PAL_PAINT(PAINT_MINT, 1) : st->fill_hi;
                if (sel) {
                    int b = te_flash ? 1 : -(ui_wave(t, 16, 1) > 0);
                    ui_pill(kx, ky + b, bw, kh - 2, st->accent,
                            ui_darker(ui_darker(st->accent)), ui_lighter(st->accent));
                    ui_text(kx + bw / 2 - font_text_width(lbl) / 2, ky + b + (kh - 2) / 2 - 4,
                            lbl, PAL_WHITE);
                } else {
                    ui_pill(kx, ky, bw, kh - 2, fill, st->fill_lo, PAL_WHITE);
                    ui_text(kx + bw / 2 - font_text_width(lbl) / 2, ky + (kh - 2) / 2 - 4,
                            lbl, st->text);
                }
            }
        }
    }
    ui_hint(STR_ICON_A " Type  " STR_ICON_B " Delete  START Done", WIN_CUTE);
}

const DoomoScene SCENE_TEXT_ENTRY = { "TEXT_ENTRY", te_enter, te_leave, te_update,
                                      te_draw, 1 };
