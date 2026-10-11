/* ui.c - the cutesy UI kit: windows, text, meters, sliders, cursors, the
 * list widget, the paint picker, toasts and small decorations (owner:
 * foundation). Modals live in ui_modal.c and ui_dialog.c.
 */
#include <stdarg.h>
#include "ui_internal.h"

static int frame_t(void) { return (int)doomo_frame_count(); }

/* ====================================================================== */
/* windows                                                                  */
void ui_drop_shadow(int x, int y, int w, int h, int r)
{
    int i;
    for (i = 0; i < h; i++) {
        int in = 0;
        if (i < r) in = ui_corner_inset(r, i);
        else if (i >= h - r) in = ui_corner_inset(r, h - 1 - i);
        if (w - 2 * in > 0) plat_shade_rect(x + in, y + i, w - 2 * in, 1, 9);
    }
}

void ui_pill(int x, int y, int w, int h, uint8_t fill, uint8_t outline, uint8_t hi)
{
    int r = DOOMO_MIN(h / 2, 4);
    if (w <= 2 || h <= 2) { plat_rect(x, y, w, h, fill); return; }
    ui_round_rect(x, y, w, h, r, outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, DOOMO_MAX(r - 1, 0), fill);
    if (h >= 6) {
        int in = DOOMO_MAX(r - 1, 1) + 1;
        plat_rect(x + in, y + 1, w - 2 * in, 1, hi);
    }
}

void ui_window(int x, int y, int w, int h, int style)
{
    const UiStyle *st = ui_style(style);
    int r = (w >= 24 && h >= 20) ? 4 : 2;
    int ir = r - 1, i;
    int band = DOOMO_CLAMP(h / 6, 3, 8);
    int lo = DOOMO_CLAMP(h / 8, 2, 5);
    ui_cur_style = style;
    if (w < 6 || h < 6) {
        plat_rect(x, y, w, h, st->fill);
        return;
    }
    ui_drop_shadow(x + 2, y + 3, w, h, r);
    ui_round_rect(x, y, w, h, r, st->outline);
    /* body: lower band, main fill, then the upper glossy band row by row */
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, ir, st->fill_lo);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2 - lo, ir, st->fill);
    for (i = 0; i < band && i < h - 2; i++) {
        int in = i < ir ? ui_corner_inset(ir, i) : 0;
        plat_rect(x + 1 + in, y + 1 + i, w - 2 - 2 * in, 1, st->fill_hi);
    }
    /* soft seams (one row of 50% checker) */
    for (i = x + 2; i < x + w - 2; i += 2) {
        plat_rect(i, y + 1 + band, 1, 1, st->fill_hi);
        plat_rect(i + 1, y + h - 2 - lo, 1, 1, st->fill_lo);
    }
    /* glossy streak + sparkle dot along the top */
    plat_rect(x + r + 2, y + 2, DOOMO_MAX(w / 4, 4), 1, st->gloss);
    plat_rect(x + r + 4 + DOOMO_MAX(w / 4, 4), y + 2, 2, 1, st->gloss);
    /* inner rim shadow on the right edge */
    plat_rect(x + w - 2, y + r + 1, 1, DOOMO_MAX(h - 2 * r - 2, 0), st->fill_lo);
    /* a glint that slides along the glossy band every few seconds */
    if (opt_get(OPT_UI_BOUNCE) && w > 24) {
        int span = w - 2 * r - 8, pos = (frame_t() * 4 + x * 13 + y * 7) % (span + 240);
        if (pos < span) {
            plat_rect(x + r + 2 + pos, y + 2, 3, 1, PAL_WHITE);
            plat_rect(x + r + 1 + pos, y + 3, 2, 1, PAL_WHITE);
            plat_rect(x + r + pos, y + 4, 1, 1, st->gloss);
        }
    }
}

int ui_window_pop(int x, int y, int w, int h, int style, int ticks)
{
    int s = ui_pop_scale(ticks);
    if (s == 256 && ticks >= 0) {
        ui_window(x, y, w, h, style);
        return 1;
    }
    {
        int nw = w * s / 256, nh = h * s / 256;
        if (nw < 8) nw = 8;
        if (nh < 8) nh = 8;
        ui_window(x + (w - nw) / 2, y + (h - nh) / 2, nw, nh, style);
    }
    return 0;
}

/* ====================================================================== */
/* text                                                                     */
/* Button icons (ICON_BTN_A / ICON_BTN_B) are drawn as little coloured
 * button caps with a white letter instead of the 1-colour font glyph. */
#define CAP_ADV 8
static void button_cap(int x, int y, int which)
{
    static const char *const la[5] = { ".#.", "#.#", "###", "#.#", "#.#" };
    static const char *const lb[5] = { "##.", "#.#", "##.", "#.#", "##." };
    const char *const *l = which == ICON_BTN_A ? la : lb;
    uint8_t fill = which == ICON_BTN_A ? PAL_PAINT(PAINT_ROSE, 1) : PAL_PAINT(PAINT_SKY, 0);
    int r, c;
    ui_fill_circle(x + 3, y + 3, 3, ui_darker(ui_darker(fill)));
    ui_fill_circle(x + 3, y + 3, 2, fill);
    plat_rect(x + 2, y + 1, 1, 1, ui_lighter(fill));
    for (r = 0; r < 5; r++)
        for (c = 0; c < 3; c++)
            if (l[r][c] == '#') plat_rect(x + 2 + c, y + 1 + r, 1, 1, PAL_WHITE);
}

static void text_run(int x, int y, const char *s, uint8_t color, int fs)
{
    if (fs == 2) plat_text(x + 1, y + 1, s, ui_lighter(ui_lighter(color)));
    plat_text(x, y, s, color);
    if (fs == 1) plat_text(x + 1, y, s, color);
}

void ui_text(int x, int y, const char *s, uint8_t color)
{
    char run[128];
    int n = 0, cx = x, rx = x, fs;
    if (!s) return;
    fs = opt_get(OPT_FONT_STYLE);
    for (;; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == 0 || c == '\n' || c == ICON_BTN_A || c == ICON_BTN_B || n == 127) {
            if (n) { run[n] = 0; text_run(rx, y, run, color, fs); n = 0; }
            if (c == 0) break;
            if (c == '\n') { cx = x; y += FONT_LINE; rx = cx; continue; }
            if (c == ICON_BTN_A || c == ICON_BTN_B) {
                button_cap(cx, y, c);
                cx += CAP_ADV;
                rx = cx;
                continue;
            }
            rx = cx;                          /* run buffer full: flush and go on */
        }
        run[n++] = (char)c;
        cx += FONT_ADVANCE;
    }
}

int ui_text_width(const char *s)
{
    int best = 0, cur = 0;
    if (!s) return 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '\n') { if (cur > best) best = cur; cur = 0; continue; }
        cur += (c == ICON_BTN_A || c == ICON_BTN_B) ? CAP_ADV : FONT_ADVANCE;
    }
    if (cur > best) best = cur;
    return best ? best - 1 : 0;
}

void ui_text_shadow(int x, int y, const char *s, uint8_t color, uint8_t shadow)
{
    plat_text(x + 1, y + 1, s, shadow);
    ui_text(x, y, s, color);
}

void ui_text_outline(int x, int y, const char *s, uint8_t color, uint8_t outline)
{
    int dx, dy;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (dx || dy) plat_text(x + dx, y + dy, s, outline);
    plat_text(x + 1, y + 2, s, outline);
    plat_text(x, y, s, color);
}

void ui_text_center(int cx, int y, const char *s, uint8_t color)
{
    char line[64];
    if (!s) return;
    while (*s) {
        int n = 0;
        while (s[n] && s[n] != '\n' && n < 63) { line[n] = s[n]; n++; }
        line[n] = 0;
        ui_text(cx - ui_text_width(line) / 2, y, line, color);
        y += FONT_LINE;
        s += n;
        if (*s == '\n') s++;
        else if (*s) { while (*s && *s != '\n') s++; if (*s) s++; }
    }
}

void ui_textf(int x, int y, uint8_t color, const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    doomo_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ui_text(x, y, buf, color);
}

static int wrap_core(int x, int y, int w, const char *s, uint8_t color, int draw,
                     int limit)
{
    char line[64];
    int maxc = w / FONT_ADVANCE, lines = 0, shown = 0;
    const char *p = s;
    if (!s) return 0;
    if (maxc < 1) maxc = 1;
    if (maxc > 63) maxc = 63;
    while (*p) {
        int n = 0, last_space = -1, len, adv;
        while (p[n] && p[n] != '\n' && n < maxc) {
            if (p[n] == ' ') last_space = n;
            n++;
        }
        if (p[n] == 0 || p[n] == '\n') { len = n; adv = n + (p[n] == '\n'); }
        else if (p[n] == ' ') { len = n; adv = n + 1; }
        else if (last_space > 0) { len = last_space; adv = last_space + 1; }
        else { len = n; adv = n; }
        if (draw && shown < limit) {
            int k = DOOMO_MIN(len, limit - shown);
            memcpy(line, p, (size_t)k);
            line[k] = 0;
            ui_text(x, y, line, color);
        }
        shown += adv;
        lines++;
        y += FONT_LINE;
        p += adv;
    }
    return lines;
}

/* Bytes of s consumed by its first `lines` wrapped lines at width w. */
int ui_wrap_skip(const char *s, int w, int lines)
{
    int maxc = w / FONT_ADVANCE, n_lines = 0;
    const char *p = s;
    if (!s) return 0;
    if (maxc < 1) maxc = 1;
    if (maxc > 63) maxc = 63;
    while (*p && n_lines < lines) {
        int n = 0, last_space = -1, adv;
        while (p[n] && p[n] != '\n' && n < maxc) {
            if (p[n] == ' ') last_space = n;
            n++;
        }
        if (p[n] == 0 || p[n] == '\n') adv = n + (p[n] == '\n');
        else if (p[n] == ' ') adv = n + 1;
        else if (last_space > 0) adv = last_space + 1;
        else adv = n;
        p += adv;
        n_lines++;
    }
    return (int)(p - s);
}

int ui_text_wrap(int x, int y, int w, const char *s, uint8_t color, int draw)
{
    return wrap_core(x, y, w, s, color, draw, 0x7FFFFFFF);
}

int ui_text_wrap_n(int x, int y, int w, const char *s, uint8_t color, int chars)
{
    return wrap_core(x, y, w, s, color, 1, chars);
}

static void big_glyphs(int x, int y, const char *s, int scale, uint8_t c, int top_only)
{
    int cx = x;
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        int col, row;
        if (*s == '\n') { cx = x; y += FONT_LINE * scale; continue; }
        if (ch < FONT_FIRST || ch > FONT_LAST) ch = '?';
        for (col = 0; col < 5; col++) {
            uint8_t bits = doomo_font[ch - FONT_FIRST][col];
            for (row = 0; row < 8; row++) {
                if (!(bits & (1 << row))) continue;
                if (top_only) {
                    /* light upper part of each stroke: two-tone candy text */
                    if (row < 3) plat_rect(cx + col * scale, y + row * scale, scale,
                                           DOOMO_MAX(scale / 2, 1), c);
                } else {
                    plat_rect(cx + col * scale, y + row * scale, scale, scale, c);
                }
            }
        }
        cx += FONT_ADVANCE * scale;
    }
}

int ui_text_big_width(const char *s, int scale)
{
    return font_text_width(s) * scale + 2;
}

void ui_text_big(int x, int y, const char *s, int scale, uint8_t color, uint8_t outline)
{
    int dx, dy;
    if (!s) return;
    scale = DOOMO_CLAMP(scale, 1, 4);
    big_glyphs(x + 1, y + 3, s, scale, ui_darker(outline), 0);        /* shadow */
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (dx || dy) big_glyphs(x + dx, y + dy, s, scale, outline, 0);
    big_glyphs(x, y + 1, s, scale, outline, 0);
    big_glyphs(x, y, s, scale, color, 0);
    big_glyphs(x, y, s, scale, ui_lighter(color), 1);
}

/* ====================================================================== */
/* meters & sliders                                                         */
void ui_bar(int x, int y, int w, int h, int value, int max, uint8_t fill)
{
    int v, fw, r = h >= 6 ? 2 : 1, t = frame_t();
    uint8_t outline = ui_darker(ui_darker(fill));
    uint8_t track = PAL_RAMP(RAMP_STONE, 2);
    if (w < 4 || h < 3) return;
    if (max <= 0) max = 1;
    v = DOOMO_CLAMP(value, 0, max);
    ui_round_rect(x, y, w, h, r, outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, r - 1, track);
    plat_rect(x + 2, y + h - 2, w - 4, 1, PAL_RAMP(RAMP_STONE, 3));
    fw = (w - 2) * v / max;
    if (fw > 0) {
        uint8_t f = fill;
        if (v * 4 < max && (t / 8) % 2) f = ui_lighter(fill);   /* low: blink */
        ui_round_rect(x + 1, y + 1, fw, h - 2, r - 1, f);
        if (fw > 2) {
            plat_rect(x + 2, y + 1, fw - 2, 1, ui_lighter(f));
            if (h >= 5) plat_rect(x + 2, y + h - 2, fw - 2, 1, ui_darker(f));
        }
        /* shine sweeping across every two seconds */
        {
            int sx = (t * 3) % (w + 60) - 10;
            if (sx >= 0 && sx < fw - 1 && h >= 4)
                plat_rect(x + 1 + sx, y + 2, 2, h - 4, PAL_WHITE);
        }
    }
}

void ui_slider(int x, int y, int w, const char *label, int value, int max,
               int selected)
{
    const UiStyle *st = ui_style(ui_cur_style);
    int t = frame_t();
    int lw = DOOMO_MIN(w * 2 / 5, 72);
    int tx = x + lw, tw = w - lw - 20, kx;
    char num[8];
    uint8_t tc = selected ? st->accent : st->text;
    if (max <= 0) max = 1;
    value = DOOMO_CLAMP(value, 0, max);
    ui_text(x + (selected ? 1 : 0), y, label, tc);
    if (tw < 8) tw = 8;
    ui_round_rect(tx, y + 2, tw, 5, 2, st->outline);
    ui_round_rect(tx + 1, y + 3, tw - 2, 3, 1, st->fill_lo);
    kx = tx + 2 + (tw - 4) * value / max;
    if (kx > tx + 2) plat_rect(tx + 1, y + 3, kx - tx - 1, 3, selected ? st->accent : st->title_bg);
    plat_rect(tx + 2, y + 3, DOOMO_MAX(kx - tx - 3, 0), 1, PAL_WHITE);
    if (selected) {
        int b = ui_wave(t, 16, 1);
        ui_heart(kx, y + 4 - (b > 0), 4, st->accent);
        plat_text(tx - 7 - (b > 0), y, "\x88", st->accent);
        plat_text(tx + tw + 2 + (b > 0), y, "\x87", st->accent);
    } else {
        ui_fill_circle(kx, y + 4, 3, st->outline);
        ui_fill_circle(kx, y + 4, 2, st->fill_hi);
        plat_rect(kx - 1, y + 3, 1, 1, PAL_WHITE);
    }
    doomo_snprintf(num, sizeof(num), "%2d", value);
    ui_text(x + w - 12, y, num, tc);
}

/* ====================================================================== */
/* cursors                                                                  */
static void glyph7(int x, int y, const char *const rows[7], const uint8_t pal[5])
{
    int r, c;
    for (r = 0; r < 7; r++) {
        for (c = 0; c < 7 && rows[r][c]; c++) {
            uint8_t col;
            switch (rows[r][c]) {
            case 'o': col = pal[0]; break;
            case 's': col = pal[1]; break;
            case 'b': col = pal[2]; break;
            case 'l': col = pal[3]; break;
            case 'w': col = pal[4]; break;
            default: continue;
            }
            plat_rect(x + c, y + r, 1, 1, col);
        }
    }
}

void ui_cursor_glyph(int x, int y, int kind, uint8_t fill)
{
    uint8_t pal[5];
    pal[2] = fill;
    pal[1] = ui_darker(fill);
    pal[0] = ui_darker(pal[1]);
    pal[3] = ui_lighter(fill);
    pal[4] = PAL_WHITE;
    switch (kind) {
    case 1: {   /* paw */
        static const char *const paw[7] = {
            "oo.o.oo", "lbolobl", "obo.obo", ".ooooo.", "olllbbo", "obbbbso", ".ossso."
        };
        glyph7(x, y, paw, pal);
        break; }
    case 2:     /* star */
        ui_star(x + 3, y + 3, 4, fill);
        break;
    case 3:     /* sparkle */
        ui_sparkle(x + 3, y + 3, 4 + (frame_t() / 6) % 3, fill);
        break;
    case 4: {   /* skull */
        static const char *const skull[7] = {
            ".ooooo.", "olllbbo", "ooobooo", "ooobooo", "obbobbo", ".obbbo.", ".o.o.o."
        };
        glyph7(x, y, skull, pal);
        break; }
    case 5: {   /* fang */
        static const char *const fang[7] = {
            "ooooooo", "owwwwso", "owwwwso", ".owwso.", ".owwso.", "..ows..", "...o..."
        };
        glyph7(x, y, fang, pal);
        break; }
    default:    /* heart */
        ui_heart(x + 3, y + 3, 3, fill);
        break;
    }
}

void ui_cursor(int x, int y)
{
    static const uint8_t cols[6] = {
        PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_CARAMEL, 1), PAL_UI_GOLD,
        PAL_UI_GOLD, PAL_PAINT(PAINT_BONE, 1), PAL_PAINT(PAINT_WHITE, 1)
    };
    int kind = opt_get(OPT_CURSOR);
    int b = ui_wave(frame_t(), 20, 2);
    if (kind < 0 || kind > 5) kind = 0;
    ui_cursor_glyph(x + b - 1, y, kind, cols[kind]);
}

void ui_scroll_arrows(int x, int y_top, int y_bottom, int up, int down)
{
    const UiStyle *st = ui_style(ui_cur_style);
    int b = ui_wave(frame_t(), 24, 1);
    if (up) {
        int y = y_top - b;
        plat_rect(x, y + 2, 5, 1, st->outline);
        plat_rect(x + 1, y + 1, 3, 1, st->accent);
        plat_rect(x + 2, y, 1, 1, st->accent);
        plat_rect(x + 1, y + 2, 3, 1, st->accent);
    }
    if (down) {
        int y = y_bottom + b;
        plat_rect(x, y, 5, 1, st->outline);
        plat_rect(x + 1, y, 3, 1, st->accent);
        plat_rect(x + 1, y + 1, 3, 1, st->accent);
        plat_rect(x + 2, y + 2, 1, 1, st->accent);
    }
}

/* ====================================================================== */
/* list                                                                     */
int (*ui_list_locked_fn)(int index);

int ui_list_update(UiList *l)
{
    int old = l->sel, vis;
    if (l->count <= 0) {
        if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); return UI_CANCEL; }
        return UI_NONE;
    }
    vis = l->visible > 0 ? l->visible : 1;
    if (btn_repeat(BTN_UP)) l->sel = l->sel > 0 ? l->sel - 1 : l->count - 1;
    if (btn_repeat(BTN_DOWN)) l->sel = l->sel < l->count - 1 ? l->sel + 1 : 0;
    if (btn_repeat(BTN_L)) l->sel = DOOMO_MAX(0, l->sel - vis);
    if (btn_repeat(BTN_R)) l->sel = DOOMO_MIN(l->count - 1, l->sel + vis);
    l->sel = DOOMO_CLAMP(l->sel, 0, l->count - 1);
    if (l->sel < l->top) l->top = l->sel;
    if (l->sel >= l->top + vis) l->top = l->sel - vis + 1;
    l->top = DOOMO_CLAMP(l->top, 0, DOOMO_MAX(0, l->count - vis));
    if (l->sel != old) { audio_sfx(SFX_CURSOR); return UI_CHANGED; }
    if (btn_pressed(BTN_A)) {
        if (ui_list_locked_fn && ui_list_locked_fn(l->sel)) {
            audio_sfx(SFX_ERROR);
            return UI_NONE;
        }
        audio_sfx(SFX_SELECT);
        return UI_SELECT;
    }
    if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); return UI_CANCEL; }
    return UI_NONE;
}

void ui_list_draw(const UiList *l, int x, int y, int w, int style)
{
    const UiStyle *st = ui_style(style);
    int vis = l->visible > 0 ? l->visible : 1;
    int rows = DOOMO_MIN(vis, l->count - l->top), i;
    int bar = l->count > vis;
    int tw = w - 14 - (bar ? 6 : 0);
    int maxc = tw / FONT_ADVANCE;
    char buf[48], line[48];
    ui_cur_style = style;
    if (maxc > 47) maxc = 47;
    for (i = 0; i < rows; i++) {
        int idx = l->top + i, ry = y + i * UI_LIST_ROW_H, n;
        const char *txt = l->items ? l->items[idx] : (l->item_fn ? l->item_fn(idx, buf) : "");
        int locked = ui_list_locked_fn && ui_list_locked_fn(idx);
        if (!txt) txt = "";
        for (n = 0; txt[n] && n < maxc; n++) line[n] = txt[n];
        line[n] = 0;
        if (idx == l->sel) {
            ui_pill(x + 9, ry, w - 9 - (bar ? 6 : 0), UI_LIST_ROW_H - 1,
                    st->accent_d, ui_darker(st->accent_d), ui_lighter(st->accent_d));
            ui_cursor(x, ry + 2);
            ui_text(x + 13, ry + 2, line, locked ? st->text_dim : st->text);
        } else {
            ui_text(x + 13, ry + 2, line, locked ? st->text_dim : st->text);
        }
        if (locked) {
            /* little padlock */
            int lx = x + w - (bar ? 16 : 10);
            plat_rect(lx + 1, ry + 2, 3, 1, st->text_dim);
            plat_rect(lx, ry + 3, 1, 2, st->text_dim);
            plat_rect(lx + 4, ry + 3, 1, 2, st->text_dim);
            plat_rect(lx - 1, ry + 5, 7, 4, st->text_dim);
            plat_rect(lx + 2, ry + 6, 1, 2, st->fill);
        }
    }
    if (bar) {
        int bx = x + w - 4, th = rows * UI_LIST_ROW_H - 2;
        int thumb = DOOMO_MAX(4, th * vis / l->count);
        int ty = y + 1 + (th - thumb) * l->top / DOOMO_MAX(1, l->count - vis);
        ui_round_rect(bx, y + 1, 3, th, 1, st->fill_lo);
        ui_round_rect(bx, ty, 3, thumb, 1, st->accent);
        ui_scroll_arrows(bx - 1, y - 4, y + rows * UI_LIST_ROW_H, l->top > 0,
                         l->top + rows < l->count);
    }
}

/* ====================================================================== */
/* paint picker                                                             */
#define SW_W UI_SWATCH_W
#define SW_H UI_SWATCH_H

int ui_paint_picker_update(uint8_t *paint, int cols)
{
    int p = *paint % PAL_PAINT_COUNT, old = p;
    int rows;
    if (cols < 1) cols = 8;
    rows = (PAL_PAINT_COUNT + cols - 1) / cols;
    if (btn_repeat(BTN_LEFT)) p = (p + PAL_PAINT_COUNT - 1) % PAL_PAINT_COUNT;
    if (btn_repeat(BTN_RIGHT)) p = (p + 1) % PAL_PAINT_COUNT;
    if (btn_repeat(BTN_UP)) {
        p -= cols;
        if (p < 0) { p += rows * cols; while (p >= PAL_PAINT_COUNT) p -= cols; }
    }
    if (btn_repeat(BTN_DOWN)) {
        p += cols;
        if (p >= PAL_PAINT_COUNT) p %= cols;
    }
    if (p != old) {
        *paint = (uint8_t)p;
        audio_sfx(SFX_CURSOR);
        return UI_CHANGED;
    }
    if (btn_pressed(BTN_A)) {
        if (!unlock_paint_ok(p)) { audio_sfx(SFX_ERROR); return UI_NONE; }
        audio_sfx(SFX_SELECT);
        return UI_SELECT;
    }
    if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); return UI_CANCEL; }
    return UI_NONE;
}

void ui_paint_picker_draw(int x, int y, uint8_t paint, int cols)
{
    const UiStyle *st = ui_style(ui_cur_style);
    int i, t = frame_t();
    int sel = paint % PAL_PAINT_COUNT;
    int sx = 0, sy = 0;
    if (cols < 1) cols = 8;
    for (i = 0; i < PAL_PAINT_COUNT; i++) {
        int cx = x + (i % cols) * SW_W, cy = y + (i / cols) * SW_H;
        int locked = !unlock_paint_ok(i);
        uint8_t d = locked ? PAL_RAMP(RAMP_STONE, 4) : PAL_PAINT(i, 0);
        uint8_t b = locked ? PAL_RAMP(RAMP_STONE, 5) : PAL_PAINT(i, 1);
        uint8_t l = locked ? PAL_RAMP(RAMP_STONE, 6) : PAL_PAINT(i, 2);
        if (i == sel) { sx = cx; sy = cy; continue; }
        ui_round_rect(cx, cy, SW_W - 2, SW_H - 2, 2, ui_darker(d));
        ui_round_rect(cx + 1, cy + 1, SW_W - 4, SW_H - 4, 1, b);
        plat_rect(cx + 2, cy + SW_H - 4, SW_W - 6, 1, d);
        plat_rect(cx + 2, cy + 1, 2, 1, l);
        plat_rect(cx + 1, cy + 2, 1, 1, l);
        if (locked) plat_text(cx + 2, cy + 1, "?", PAL_RAMP(RAMP_STONE, 2));
    }
    /* the selected swatch: raised, bigger, white frame, bouncing */
    {
        int b = ui_wave(t, 18, 1) > 0;
        int locked = !unlock_paint_ok(sel);
        int cx = sx - 2, cy = sy - 2 - b;
        uint8_t d = locked ? PAL_RAMP(RAMP_STONE, 4) : PAL_PAINT(sel, 0);
        uint8_t bb = locked ? PAL_RAMP(RAMP_STONE, 5) : PAL_PAINT(sel, 1);
        uint8_t l = locked ? PAL_RAMP(RAMP_STONE, 6) : PAL_PAINT(sel, 2);
        ui_drop_shadow(cx + 1, cy + 2, SW_W + 2, SW_H + 2, 3);
        ui_round_rect(cx - 1, cy - 1, SW_W + 4, SW_H + 4, 3, st->outline);
        ui_round_rect(cx, cy, SW_W + 2, SW_H + 2, 3, PAL_WHITE);
        ui_round_rect(cx + 2, cy + 2, SW_W - 2, SW_H - 2, 2, ui_darker(d));
        ui_round_rect(cx + 3, cy + 3, SW_W - 4, SW_H - 4, 1, bb);
        plat_rect(cx + 4, cy + SW_H - 2, SW_W - 6, 1, d);
        plat_rect(cx + 4, cy + 3, 3, 1, l);
        plat_rect(cx + 3, cy + 4, 1, 1, l);
        plat_rect(cx + 4, cy + 4, 1, 1, PAL_WHITE);
        if (locked) plat_text(cx + 4, cy + 3, "?", PAL_RAMP(RAMP_STONE, 2));
        ui_sparkle(cx + SW_W + 1, cy, (t / 3) & 15, PAL_UI_GOLD);
    }
}

/* ====================================================================== */
/* toasts                                                                   */
#define TOAST_Q 4
#define TOAST_IN 9
#define TOAST_SHOW 75
#define TOAST_OUT 8
static char toast_msg[TOAST_Q][56];
static int toast_n, toast_t;

void ui_toast(const char *msg)
{
    int i;
    if (!msg) return;
    if (toast_n == TOAST_Q) {
        for (i = 1; i < TOAST_Q; i++) memcpy(toast_msg[i - 1], toast_msg[i], 56);
        toast_n--;
        if (toast_n == 0) toast_t = 0;
    }
    for (i = 0; i < 55 && msg[i]; i++) toast_msg[toast_n][i] = msg[i];
    toast_msg[toast_n][i] = 0;
    toast_n++;
}

int ui_toast_pending(void) { return toast_n; }

void ui_toast_clear(void) { toast_n = 0; toast_t = 0; }

void ui_toast_draw(void)
{
    const UiStyle *st;
    int sw = plat_screen_w(), t, w, x, xf, y = 4, d;
    int saved = ui_cur_style;
    if (toast_n == 0) return;
    t = toast_t++;
    st = ui_style(WIN_CREAM);
    w = DOOMO_MIN(ui_text_width(toast_msg[0]) + 22, sw - 8);
    xf = sw - w - 4;
    if (t < TOAST_IN) {
        d = TOAST_IN - t;
        x = xf + d * d * (sw - xf) / (TOAST_IN * TOAST_IN) - (t >= TOAST_IN - 3 ? 2 : 0);
    } else if (t < TOAST_IN + TOAST_SHOW) {
        x = xf;
    } else {
        d = t - TOAST_IN - TOAST_SHOW;
        x = xf + d * d * (sw - xf) / (TOAST_OUT * TOAST_OUT);
    }
    ui_drop_shadow(x + 2, y + 2, w, 15, 4);
    ui_round_rect(x, y, w, 15, 4, PAL_RAMP(RAMP_CANDY, 3));
    ui_round_rect(x + 1, y + 1, w - 2, 13, 3, st->fill_hi);
    plat_rect(x + 3, y + 10, w - 6, 3, st->fill);
    plat_rect(x + 5, y + 2, DOOMO_MAX(w / 4, 4), 1, PAL_WHITE);
    ui_star(x + 8, y + 7, 4, PAL_UI_GOLD);
    ui_text(x + 15, y + 4, toast_msg[0], st->text);
    ui_sparkle(x + w - 4, y + 2, (t / 2) & 15, PAL_UI_GOLD);
    ui_cur_style = saved;
    if (toast_t >= TOAST_IN + TOAST_SHOW + TOAST_OUT) {
        int i;
        for (i = 1; i < toast_n; i++) memcpy(toast_msg[i - 1], toast_msg[i], 56);
        toast_n--;
        toast_t = 0;
    }
}

/* ====================================================================== */
/* decorations                                                              */
void ui_portrait(int x, int y, int resident_id, int scale, int expression)
{
    const DoomoImage *img = portrait_image(resident_id, expression);
    const UiStyle *st = ui_style(WIN_MINT);
    int w, h;
    if (scale < 1) scale = 1;
    w = (img ? img->w : PORTRAIT_W) * scale + 6;
    h = (img ? img->h : PORTRAIT_H) * scale + 6;
    ui_drop_shadow(x + 2, y + 2, w, h, 4);
    ui_round_rect(x, y, w, h, 4, st->outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 3, st->fill_hi);
    plat_rect(x + 2, y + h * 2 / 3, w - 4, h / 3 - 2, st->fill);
    /* little heart wallpaper */
    ui_heart(x + w - 8, y + 8, 2, PAL_RAMP(RAMP_MINT, 5));
    ui_heart(x + 7, y + h / 2, 2, PAL_RAMP(RAMP_MINT, 5));
    if (img) plat_image(x + 3, y + 3, img, scale, 0, 0);
}

void ui_title(int cx, int y, const char *s, int style)
{
    const UiStyle *st = ui_style(style);
    int tw = ui_text_width(s), w = tw + 18, x = cx - w / 2;
    int t = frame_t();
    ui_drop_shadow(x + 1, y + 2, w, 13, 4);
    ui_pill(x, y, w, 13, st->title_bg, st->outline, ui_lighter(st->title_bg));
    ui_text_shadow(x + 9, y + 3, s, st->title, st->outline);
    ui_sparkle(x + 3, y + 1, (t / 3) & 15, PAL_WHITE);
    ui_sparkle(x + w - 3, y + 11, (t / 3 + 8) & 15, PAL_WHITE);
}

void ui_hint(const char *s, int style)
{
    const UiStyle *st = ui_style(style);
    int sw = plat_screen_w(), sh = plat_screen_h();
    int w = ui_text_width(s) + 12, x = (sw - w) / 2, y = sh - 13;
    ui_pill(x, y, w, 12, st->title_bg, st->outline, ui_lighter(st->title_bg));
    ui_text(x + 6, y + 2, s, st->title);
}

void ui_button(int x, int y, int w, const char *label, int selected, int style)
{
    const UiStyle *st = ui_style(style);
    int tw = ui_text_width(label);
    int b = selected ? (ui_wave(frame_t(), 20, 1) > 0) : 0;
    y -= b;
    if (selected) {
        ui_drop_shadow(x + 1, y + 2 + b, w, 14, 4);
        ui_pill(x, y, w, 14, st->accent, ui_darker(ui_darker(st->accent)), ui_lighter(st->accent));
        ui_text_shadow(x + (w - tw) / 2, y + 3, label, PAL_WHITE, ui_darker(st->accent));
        ui_cursor(x - 9, y + 3);
    } else {
        ui_pill(x, y, w, 14, st->fill_hi, st->outline, PAL_WHITE);
        ui_text(x + (w - tw) / 2, y + 3, label, st->text);
    }
}

void ui_value_row(int x, int y, int w, const char *label, const char *value,
                  int selected)
{
    const UiStyle *st = ui_style(ui_cur_style);
    int vw = ui_text_width(value), right = x + w;
    int b = selected ? (ui_wave(frame_t(), 16, 1) > 0) : 0;
    if (selected) {
        ui_pill(x - 2, y - 2, w + 4, FONT_LINE + 3, st->accent_d,
                ui_darker(st->accent_d), ui_lighter(st->accent_d));
        ui_cursor(x - 12, y);
    }
    ui_text(x + 2, y, label, selected ? st->text : st->text_dim);
    if (selected) {
        plat_text(right - vw - 19 - b, y, STR_ICON_ARROW_L, st->accent);
        plat_text(right - 8 + b, y, STR_ICON_ARROW_R, st->accent);
    }
    ui_text(right - vw - 11, y, value, selected ? st->accent : st->text);
}

int ui_tabs(int x, int y, const char *const *names, int count, int sel, int style)
{
    const UiStyle *st = ui_style(style);
    int i, cx = x, t = frame_t();
    for (i = 0; i < count; i++) {
        int tw = ui_text_width(names[i]) + 10;
        if (i == sel) {
            int b = ui_wave(t, 20, 1) > 0;
            ui_drop_shadow(cx + 1, y + 1 - b, tw, 13, 4);
            ui_pill(cx, y - 2 - b, tw, 13, st->accent, ui_darker(ui_darker(st->accent)),
                    ui_lighter(st->accent));
            ui_text_shadow(cx + 5, y + 1 - b, names[i], PAL_WHITE, ui_darker(st->accent));
        } else {
            ui_pill(cx, y, tw, 11, st->fill_lo, st->outline, st->fill_hi);
            ui_text(cx + 5, y + 2, names[i], st->text_dim);
        }
        cx += tw + 2;
    }
    return cx - x - 2;
}
