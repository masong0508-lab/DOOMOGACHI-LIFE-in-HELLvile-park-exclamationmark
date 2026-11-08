/* ui.c - the cutesy UI kit: windows, text, meters, sliders, cursors, the
 * list widget, the paint picker, toasts and small decorations (owner:
 * foundation). Modals live in ui_modal.c and ui_dialog.c.
 */
#include <stdarg.h>
#include "ui_internal.h"
#include "doomo/plat_ext.h"
#include "doomo/tips.h"

static int frame_t(void) { return (int)doomo_frame_count(); }

/* ====================================================================== */
/* windows                                                                  */
/* Drop shadow (sx, sy, w, h, 4) behind a pill (px, py, pw, ph) that is
 * drawn right afterwards: only the part the pill leaves visible. */
void ui_pill_shadow(int sx, int sy, int w, int h, int px, int py, int pw, int ph)
{
    int pr = (pw <= 2 || ph <= 2) ? 0 : DOOMO_MIN(ph / 2, 4);
    ui_drop_shadow_under(sx, sy, w, h, 4, px, py, pw, ph, pr);
}

static void pill_draw(int x, int y, int w, int h, uint8_t fill, uint8_t outline, uint8_t hi)
{
    int r = DOOMO_MIN(h / 2, 4);
    ui_round_rect(x, y, w, h, r, outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, DOOMO_MAX(r - 1, 0), fill);
    if (h >= 6) {
        int in = DOOMO_MAX(r - 1, 1) + 1;
        ur_rect(x + in, y + 1, w - 2 * in, 1, hi);
    }
}

/* Pills (buttons, list selections, key caps, tabs, hints) are drawn many
 * times a frame in a few sizes: their rect lists are cached. */
void ui_pill(int x, int y, int w, int h, uint8_t fill, uint8_t outline, uint8_t hi)
{
    uint32_t k0, k1;
    if (plat_draw_skipped()) return;
    if (w <= 2 || h <= 2) { plat_rect(x, y, w, h, fill); return; }
    if (w > 255 || h > 63) { pill_draw(x, y, w, h, fill, outline, hi); return; }
    k0 = UR_K_PILL | (uint32_t)w << 8 | (uint32_t)h << 16;
    k1 = (uint32_t)fill | (uint32_t)outline << 8 | (uint32_t)hi << 16;
    if (ur_cache_draw(k0, k1, 0, x, y)) return;
    ur_cache_begin(k0, k1, 0, x, y);
    pill_draw(x, y, w, h, fill, outline, hi);
    if (!ur_cache_end()) pill_draw(x, y, w, h, fill, outline, hi);
}

void ui_window(int x, int y, int w, int h, int style)
{
    const UiStyle *st = ui_style(style);
    int r = (w >= 24 && h >= 20) ? 4 : 2;
    int ir = r - 1, i;
    int band = DOOMO_CLAMP(h / 6, 3, 8);
    int lo = DOOMO_CLAMP(h / 8, 2, 5);
    ui_cur_style = style;
    if (plat_draw_skipped()) return;
    if (w < 6 || h < 6) {
        plat_rect(x, y, w, h, st->fill);
        return;
    }
    ui_window_shadow(x, y, w, h);
    /* Layers: outline round rect, lower band (inner round rect), main fill
     * (inner round rect lo rows shorter), the upper glossy band. Each layer
     * is only drawn where the later ones do not cover it - the same
     * picture with a third of the pixel writes: the outline's straight
     * middle rows are covered by the lower band but for the two edge
     * columns, the lower band by the main fill down to the main fill's
     * bottom corners, and the main fill's top rows by the glossy band. */
    ui_round_rect_rows(x, y, w, h, r, st->outline, 0, r);
    ui_round_rect_rows(x, y, w, h, r, st->outline, h - r, h);
    plat_rect(x, y + r, 1, h - 2 * r, st->outline);
    plat_rect(x + w - 1, y + r, 1, h - 2 * r, st->outline);
    ui_round_rect_rows(x + 1, y + 1, w - 2, h - 2, ir, st->fill_lo, h - 2 - lo - ir, h - 2);
    ui_round_rect_rows(x + 1, y + 1, w - 2, h - 2 - lo, ir, st->fill,
                       DOOMO_MIN(band, h - 2), h - 2 - lo);
    for (i = 0; i < band && i < h - 2;) {
        int in = i < ir ? ui_corner_inset(ir, i) : 0, j = i + 1;
        while (j < band && j < h - 2 && (j < ir ? ui_corner_inset(ir, j) : 0) == in) j++;
        plat_rect(x + 1 + in, y + 1 + i, w - 2 - 2 * in, j - i, st->fill_hi);
        i = j;
    }
    /* soft seams (one row of 50% checker) */
    plat_checker(x + 2, y + 1 + band, w - 4, 1, st->fill_hi);
    plat_checker(x + 3, y + h - 2 - lo, w - 4, 1, st->fill_lo);
    /* glossy streak + sparkle dot along the top */
    plat_rect(x + r + 2, y + 2, DOOMO_MAX(w / 4, 4), 1, st->gloss);
    plat_rect(x + r + 4 + DOOMO_MAX(w / 4, 4), y + 2, 2, 1, st->gloss);
    /* inner rim shadow on the right edge */
    plat_rect(x + w - 2, y + r + 1, 1, DOOMO_MAX(h - 2 * r - 2, 0), st->fill_lo);
    /* a glint that slides along the glossy band every few seconds */
    {
        int pos = ui_window_glint(x, y, w, h);
        if (pos >= 0) {
            plat_rect(x + r + 2 + pos, y + 2, 3, 1, PAL_WHITE);
            plat_rect(x + r + 1 + pos, y + 3, 2, 1, PAL_WHITE);
            plat_rect(x + r + pos, y + 4, 1, 1, st->gloss);
        }
    }
}

/* ui_window(x, y, w, h, style)'s body pixels in [cx0, cx1) x [ry0, ry1),
 * a rect clear of its edges, corners, rim and gloss row (cx0 >= x + 4,
 * cx1 <= x + w - 4, ry0 >= y + 5, ry1 <= y + h - 1): what a retained
 * panel's hole repaints under redrawn text (G7, ui_dialog.c). */
void ui_window_rows(int x, int y, int w, int h, int style, int cx0, int cx1, int ry0, int ry1)
{
    const UiStyle *st = ui_style(style);
    int band = DOOMO_CLAMP(h / 6, 3, 8), lo = DOOMO_CLAMP(h / 8, 2, 5);
    int g1 = y + 1 + DOOMO_MIN(band, h - 2), a, b, row;
    (void)w;
    if (plat_draw_skipped() || cx0 >= cx1 || ry0 >= ry1) return;
    /* the gloss rows, the main fill, the lower band (ui_window's layers) */
    a = DOOMO_MAX(ry0, y + 1); b = DOOMO_MIN(ry1, g1);
    if (a < b) plat_rect(cx0, a, cx1 - cx0, b - a, st->fill_hi);
    a = DOOMO_MAX(ry0, g1); b = DOOMO_MIN(ry1, y + h - 1 - lo);
    if (a < b) plat_rect(cx0, a, cx1 - cx0, b - a, st->fill);
    a = DOOMO_MAX(ry0, DOOMO_MAX(g1, y + h - 1 - lo)); b = DOOMO_MIN(ry1, y + h - 1);
    if (a < b) plat_rect(cx0, a, cx1 - cx0, b - a, st->fill_lo);
    /* the two seams, in phase with ui_window's checkers */
    row = y + 1 + band;
    if (row >= ry0 && row < ry1) {
        int s0 = cx0 + ((cx0 - (x + 2)) & 1);
        if (s0 < cx1) plat_checker(s0, row, cx1 - s0, 1, st->fill_hi);
    }
    row = y + h - 2 - lo;
    if (row >= ry0 && row < ry1) {
        int s0 = cx0 + ((cx0 - (x + 3)) & 1);
        if (s0 < cx1) plat_checker(s0, row, cx1 - s0, 1, st->fill_lo);
    }
}

/* ---- S2-1 modal hold (ui.h) ---------------------------------------------- */
#if !defined(DOOMO_PLATFORM_PS1)
static const void *mh_key;           /* the modal the state belongs to */
static uint8_t mh_st;                /* 0 none, 1 a full frame with ok was drawn (and is the one shown), 2 held */
static uint8_t mh_in;                /* inside a held draw: the window rect below gets no drop shadow */
static int16_t mh_x, mh_y, mh_w, mh_h;
#if defined(DOOMO_PLATFORM_GBA)
static uint8_t mh_on = 1;
#else
static uint8_t mh_on;
#endif
static int mh_holds;                 /* frames drawn held (tests) */

int ui_modal_hold_enable(int on)
{
    if (on >= 0) mh_on = (uint8_t)(on != 0);
    return mh_holds;
}

static uint8_t mh_strip;             /* the kept frame's toast band has a clean copy (plat_strip) */
static uint8_t mh_noband;            /* ui_modal_no_band(): this frame's mark keeps no band copy */
static int16_t mh_sy, mh_sh;         /* that band's rows */

/* nothing else on screen moves: no dialogue box, no tip card, no overlay scene drawn above; toasts may come and go
 * when the platform kept a clean copy of their band (plat_strip, SP3): a held tick puts the band back and the toast
 * draws over it as on a full frame */
static int mh_steady(void)
{
    if (ui_dialog_active() || tip_active() || scene_drawn_above()) return 0;
    return !ui_toast_pending() || mh_strip;
}

void ui_modal_drop(void)
{
    if (mh_st == 2) plat_hold_frame(0);
    mh_st = 0;
}

/* scene.c: the stack changed (it released the platform's hold itself) */
void ui_modal_reset(void)
{
    mh_st = 0;
}

int ui_modal_hold(const void *key, int ok, int x, int y, int w, int h)
{
    if (!ok || !mh_on || key != mh_key || !mh_steady()) {
        ui_modal_drop();
        mh_key = key;
        return 0;
    }
    if (mh_st == 2 && !plat_hold_frame(4)) mh_st = 0;           /* lost (a tile frame, a tip card) */
    /* the full frame drawn on the last drawn tick is the one on screen (callers ask on every drawn tick; a
     * catch-up tick draws nothing): keep it in both pages */
    if (mh_st == 1 && !plat_draw_skipped()) mh_st = plat_hold_frame(2) ? 2 : 0;
    if (mh_st != 2) return 0;
    if (mh_strip && !plat_draw_skipped()) {
        int by, bh;
        ui_toast_band(&by, &bh);
        if (by != mh_sy || bh != mh_sh || !plat_strip(PLAT_STRIP_RESTORE, by, bh)) {
            ui_modal_drop();                                    /* another lane, or the copy is gone */
            return 0;
        }
    }
    mh_holds++;
    mh_in = 1;
    mh_x = (int16_t)x; mh_y = (int16_t)y; mh_w = (int16_t)w; mh_h = (int16_t)h;
    ui_shade_clip(x, y, w, h);
    return 1;
}

void ui_modal_hold_end(void)
{
    mh_in = 0;
    ui_shade_clip_off();
}

void ui_modal_no_band(void)
{
    mh_noband = 1;
}

int ui_modal_in(void)
{
    return mh_in;
}

void ui_modal_full(const void *key, int ok)
{
    int noband = mh_noband;
    mh_noband = 0;
    if (key != mh_key || plat_draw_skipped()) return;
    if (!ok) { ui_modal_drop(); return; }
    if (!mh_on || mh_st != 0) return;
    mh_strip = 0;
    if (!noband) {
        /* the toast band as drawn so far (the toast itself is drawn after the scene): its clean copy */
        int by, bh;
        ui_toast_band(&by, &bh);
        mh_strip = (uint8_t)plat_strip(PLAT_STRIP_SAVE, by, bh);
        mh_sy = (int16_t)by;
        mh_sh = (int16_t)bh;
    }
    if (mh_steady()) mh_st = 1;
}
#define MH_NO_SHADOW(x, y, w, h) (mh_in && (x) == mh_x && (y) == mh_y && (w) == mh_w && (h) == mh_h)
#else
#define MH_NO_SHADOW(x, y, w, h) 0
#endif

/* ui_window(x, y, w, h)'s drop shadow alone (a kept panel's shading, G7) */
void ui_window_shadow(int x, int y, int w, int h)
{
    int r = (w >= 24 && h >= 20) ? 4 : 2;
    if (w < 6 || h < 6) return;
    if (MH_NO_SHADOW(x, y, w, h)) return;   /* S2-1: it is on the held page already */
    ui_drop_shadow_under(x + 2, y + 3, w, h, r, x, y, w, h, r);
}

/* Where ui_window(x, y, w, h)'s glint is this frame, -1 = not on it (a
 * retained panel's key, G7). */
static int glint_held(void);  /* ui_glint_hold() (its byte lives with the toast's, below) */

int ui_window_glint(int x, int y, int w, int h)
{
    int r = (w >= 24 && h >= 20) ? 4 : 2, span, pos;
    if (w < 6 || h < 6 || !opt_get(OPT_UI_BOUNCE) || w <= 24 || (UI_PANELS && glint_held())) return -1;
    span = w - 2 * r - 8;
    pos = (frame_t() * 4 + x * 13 + y * 7) % (span + 240);
    return pos < span ? pos : -1;
}

/* The rect and corner radius of ui_window(x, y, w, h)'s opaque body. */
static void window_body(int w, int h, int *r)
{
    *r = (w < 6 || h < 6) ? 0 : (w >= 24 && h >= 20) ? 4 : 2;
}

void ui_dim_under_window_pop(int x, int y, int w, int h, int ticks, int level)
{
    int s = ui_pop_scale(ticks), r;
    if (!(s == 256 && ticks >= 0)) {
        int nw = w * s / 256, nh = h * s / 256;
        if (nw < 8) nw = 8;
        if (nh < 8) nh = 8;
        x += (w - nw) / 2;
        y += (h - nh) / 2;
        w = nw;
        h = nh;
    }
    window_body(w, h, &r);
    ui_shade_screen_under(level, x, y, w, h, r);
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
    if (plat_draw_skipped()) return;
    static const char *const la[5] = { ".#.", "#.#", "###", "#.#", "#.#" };
    static const char *const lb[5] = { "##.", "#.#", "##.", "#.#", "##." };
    const char *const *l = which == ICON_BTN_A ? la : lb;
    uint8_t fill = which == ICON_BTN_A ? PAL_PAINT(PAINT_ROSE, 1) : PAL_PAINT(PAINT_SKY, 0);
    int r, c;
    ui_fill_circle(x + 3, y + 3, 3, ui_darker(ui_darker(fill)));
    ui_fill_circle(x + 3, y + 3, 2, fill);
    plat_rect(x + 2, y + 1, 1, 1, ui_lighter(fill));
    ur_begin();
    for (r = 0; r < 5; r++)
        for (c = 0; c < 3; c++)
            if (l[r][c] == '#') ur_run(y + 1 + r, x + 2 + c, 1, PAL_WHITE);
    ur_end();
}

static void text_run(int x, int y, const char *s, uint8_t color, int fs)
{
    if (fs == 2) plat_text(x + 1, y + 1, s, ui_lighter(ui_lighter(color)));
    plat_text(x, y, s, color);
    if (fs == 1) plat_text(x + 1, y, s, color);
}

/* L2a: every user-visible string is translated once, here in the ui_*
 * entry points; the platforms' plat_text draw bytes as given (lang.h
 * "LOOKUP"). Text that was translated is never looked up again, so a
 * translation that happens to be another English key stays as it is
 * (Spanish "No" of "Off"). Text that had no translation of its own may
 * still have translated pieces: each line of a centred or wrapped text,
 * each run between button icons of a hint ("...  [B]Back"). `pieces`
 * asks text_draw() to look those runs up (it never does for English). */
static int tr_pieces(const char *tr, const char *src)
{
    return tr == src && lang_get() != LANG_EN;
}

static void text_draw(int x, int y, const char *s, uint8_t color, int pieces)
{
    char run[128];
    int n = 0, cx = x, rx = x, fs = opt_get(OPT_FONT_STYLE), k = 0;
    for (;; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == 0 || c == '\n' || c == ICON_BTN_A || c == ICON_BTN_B || n == 127) {
            if (n) {
                run[n] = 0;
                /* a piece of the text (never all of it: that was looked up) */
                text_run(rx, y, pieces && (k || c) ? lang_tr(run) : run, color, fs);
                n = 0;
            }
            k++;
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

void ui_text(int x, int y, const char *s, uint8_t color)
{
    const char *t;
    if (!s || plat_draw_skipped()) return;
    t = lang_tr(s);
    text_draw(x, y, t, color, tr_pieces(t, s));
}

/* not inlined: it is called from many places, and inlined into each it cost the PS1 3 KB of resident code (C3) */
__attribute__((noinline)) int ui_width_raw(const char *s)
{
    int best = 0, cur = 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '\n') { if (cur > best) best = cur; cur = 0; continue; }
        cur += (c == ICON_BTN_A || c == ICON_BTN_B) ? CAP_ADV : FONT_ADVANCE;
    }
    if (cur > best) best = cur;
    return best ? best - 1 : 0;
}

int ui_text_width(const char *s)
{
    return s ? ui_width_raw(lang_tr(s)) : 0;
}

/* Text (translated already) clipped to [x0, x1): the glyphs that fit whole,
 * one plat_text per run of them between blanks (L2a; was one call per
 * glyph). Same pixels: every glyph keeps its own x. */
void ui_text_clip(int x, int y, const char *s, uint8_t color, int x0, int x1)
{
    char run[64];
    int n = 0, rx = x;
    /* (C3 speed) the glyphs left of x0 are never drawn: step over them in a tight loop (the news ticker's line is
     * hundreds of glyphs, most of them off to the left); the same runs as the loop below would cut */
    while (x < x0 && *s) { s++; x += FONT_ADVANCE; }
    for (;; s++, x += FONT_ADVANCE) {
        unsigned char c = (unsigned char)*s;
        int stop = c == 0 || x + FONT_ADVANCE > x1;
        if (stop || c == ' ' || c == '\n' || x < x0 || n == (int)sizeof(run) - 1) {
            if (n) { run[n] = 0; plat_text(rx, y, run, color); n = 0; }
            if (stop) break;
            if (c == ' ' || c == '\n' || x < x0) continue;
        }
        if (!n) rx = x;
        run[n++] = (char)c;
    }
}

void ui_text_shadow(int x, int y, const char *s, uint8_t color, uint8_t shadow)
{
    const char *t;
    if (!s || plat_draw_skipped()) return;
    t = lang_tr(s);
    plat_text(x + 1, y + 1, t, shadow);
    text_draw(x, y, t, color, tr_pieces(t, s));
}

/* Weak default of plat_text_outline (platform.h): the ten passes. */
__attribute__((weak)) void plat_text_outline(int x, int y, const char *s, uint8_t color,
                                             uint8_t outline)
{
    int dx, dy;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (dx || dy) plat_text(x + dx, y + dy, s, outline);
    plat_text(x + 1, y + 2, s, outline);
    plat_text(x, y, s, color);
}

void ui_text_outline(int x, int y, const char *s, uint8_t color, uint8_t outline)
{
    if (plat_draw_skipped() || !s) return;
    plat_text_outline(x, y, lang_tr(s), color, outline);
}

void ui_text_center(int cx, int y, const char *s, uint8_t color)
{
    char line[64];
    const char *src = s, *t;
    int pieces;
    if (!s || plat_draw_skipped()) return;
    s = lang_tr(s);
    pieces = tr_pieces(s, src);
    while (*s) {
        int n = 0;
        while (s[n] && s[n] != '\n' && n < 63) { line[n] = s[n]; n++; }
        line[n] = 0;
        /* a line of an untranslated text may have a translation */
        t = pieces && (s != src || s[n]) ? lang_tr(line) : line;
        text_draw(cx - ui_width_raw(t) / 2, y, t, color, pieces && t == line);
        y += FONT_LINE;
        s += n;
        if (*s == '\n') s++;
        else if (*s) { while (*s && *s != '\n') s++; if (*s) s++; }
    }
}

void ui_textf(int x, int y, uint8_t color, const char *fmt, ...)
{
    char buf[128];
    const char *t;
    va_list ap;
    if (!fmt || plat_draw_skipped()) return;
    t = lang_tr(fmt);
    va_start(ap, fmt);
    doomo_vsnprintf(buf, sizeof(buf), t, ap);
    va_end(ap);
    if (tr_pieces(t, fmt)) ui_text(x, y, buf, color);   /* "%s": the text may have one */
    else text_draw(x, y, buf, color, 0);
}

static int wrap_core(int x, int y, int w, const char *s, uint8_t color, int draw,
                     int limit)
{
    char line[64];
    int maxc = w / FONT_ADVANCE, lines = 0, shown = 0;
    const char *p, *src = s;
    int pieces;
    if (!s) return 0;
    p = s = lang_tr(s);
    pieces = tr_pieces(s, src);
    if (maxc < 1) maxc = 1;
    if (maxc > 63) maxc = 63;
    if (draw && plat_draw_skipped()) draw = 0;          /* lines still count */
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
            const char *t;
            memcpy(line, p, (size_t)k);
            line[k] = 0;
            /* a line of an untranslated text may have a translation */
            t = pieces && (p != s || p[k]) ? lang_tr(line) : line;
            text_draw(x, y, t, color, pieces && t == line);
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
    return font_text_width(s) * scale + 2;        /* font_text_width translates */
}

void ui_text_big(int x, int y, const char *s, int scale, uint8_t color, uint8_t outline)
{
    int dx, dy;
    if (!s) return;
    s = lang_tr(s);
    scale = DOOMO_CLAMP(scale, 1, 4);
    if (ur_text_big(x, y, s, scale, color, outline)) return;
    /* too big for the bit-row path: the layers one by one */
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
    if (plat_draw_skipped()) return;
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
    if (plat_draw_skipped()) return;
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
        ui_glyph7(x, y, paw, pal);
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
        ui_glyph7(x, y, skull, pal);
        break; }
    case 5: {   /* fang */
        static const char *const fang[7] = {
            "ooooooo", "owwwwso", "owwwwso", ".owwso.", ".owwso.", "..ows..", "...o..."
        };
        ui_glyph7(x, y, fang, pal);
        break; }
    default:    /* heart */
        ui_heart(x + 3, y + 3, 3, fill);
        break;
    }
}

void ui_cursor(int x, int y)
{
    if (plat_draw_skipped()) return;
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
    if (plat_draw_skipped()) return;
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
    const char *tr;
    int pieces;
    ui_cur_style = style;
    if (plat_draw_skipped()) return;
    if (maxc > 47) maxc = 47;
    for (i = 0; i < rows; i++) {
        int idx = l->top + i, ry = y + i * UI_LIST_ROW_H, n;
        const char *txt = l->items ? l->items[idx] : (l->item_fn ? l->item_fn(idx, buf) : "");
        int locked = ui_list_locked_fn && ui_list_locked_fn(idx);
        if (!txt) txt = "";
        tr = lang_tr(txt);               /* L2a: translate, then cut to the row */
        for (n = 0; tr[n] && n < maxc; n++) line[n] = tr[n];
        line[n] = 0;
        pieces = tr_pieces(tr, txt);
        if (idx == l->sel) {
            ui_pill(x + 9, ry, w - 9 - (bar ? 6 : 0), UI_LIST_ROW_H - 1,
                    st->accent_d, ui_darker(st->accent_d), ui_lighter(st->accent_d));
            ui_cursor(x, ry + 2);
            text_draw(x + 13, ry + 2, line, locked ? st->text_dim : st->text, pieces);
        } else {
            text_draw(x + 13, ry + 2, line, locked ? st->text_dim : st->text, pieces);
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
    if (plat_draw_skipped()) return;
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
        ui_drop_shadow_under(cx + 1, cy + 2, SW_W + 2, SW_H + 2, 3,
                             cx - 1, cy - 1, SW_W + 4, SW_H + 4, 3);
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
static char toast_msg[TOAST_Q][UI_TOAST_MAX + 1];
static uint8_t toast_kd[TOAST_Q];       /* CL1: UI_TOAST_* of each queued line */
static uint16_t toast_at[TOAST_Q];      /* CL1: the tick it was posted, low 16 bits (a scene change keeps that tick's toasts) */
static int toast_n, toast_t;
static uint8_t toast_gen;               /* changes when the line on screen is replaced (ui_toast_steady) */
static uint8_t ui_held;                 /* bit 0: ui_toast_hold(): this frame's toast is already on the kept page;
                                           bit 1: ui_glint_hold() (one byte: GBA EWRAM) */
void ui_glint_hold(int on) { ui_held = (uint8_t)(on ? ui_held | 2 : ui_held & ~2); }
static int glint_held(void) { return (ui_held & 2) != 0; }
static int16_t toast_y = UI_TOAST_LANE_TOP;     /* CL1: this frame's lane (ui_toast_lane) */
static int16_t toast_rect[4];           /* CL1: the last drawn toast (x, y, w, h; w 0: none) */
static uint16_t toast_count[UI_TOAST_KINDS];    /* CL1: toasts shown per kind (tests, runner) */

/* CL1 (C5): unlock news. A burst of unlocks is told once, after it settles and when nothing else is queued:
 * UL_NEWS_SETTLE quiet ticks after the last unlock, and UL_NEWS_GAP ticks after the last unlock news. The first
 * news of a session (a new island grants a handful in its first minute) gathers for UL_NEWS_FIRST ticks from its
 * first unlock, so the opening minute tells one digest instead of a stream of names. */
#define UL_NEWS_SETTLE 45
#define UL_NEWS_GAP (30 * 40)
#define UL_NEWS_FIRST (30 * 20)
static const char *news_name, *news_what;   /* the first unlock of the news (kept, not copied: ui.h) */
static uint8_t news_n, news_told;
static uint16_t news_quiet, news_gap, news_age;

/* drop queued line i (the one on screen when i is 0) */
static void toast_drop(int i)
{
    for (i++; i < toast_n; i++) {
        memcpy(toast_msg[i - 1], toast_msg[i], UI_TOAST_MAX + 1);
        toast_kd[i - 1] = toast_kd[i];
        toast_at[i - 1] = toast_at[i];
    }
    toast_n--;
}

void ui_toast_kind(const char *msg, int kind)
{
    int i, pos, lo;
    if (!msg) return;
    kind = DOOMO_CLAMP(kind, 0, UI_TOAST_KINDS - 1);
    lo = toast_n > 0 && toast_t > 0 ? 1 : 0;     /* the line on screen keeps its place */
    if (toast_n == TOAST_Q) {
        /* full: the oldest waiting line of the lowest kind makes room, unless the new one is lower still */
        int v = -1;
        for (i = lo; i < toast_n; i++)
            if (v < 0 || toast_kd[i] < toast_kd[v]) v = i;
        if (v < 0 || toast_kd[v] > kind) return;
        toast_drop(v);
        if (v == 0) { toast_gen++; toast_t = 0; }
    }
    pos = toast_n;
    while (pos > lo && toast_kd[pos - 1] < kind) pos--;
    for (i = toast_n; i > pos; i--) {
        memcpy(toast_msg[i], toast_msg[i - 1], UI_TOAST_MAX + 1);
        toast_kd[i] = toast_kd[i - 1];
        toast_at[i] = toast_at[i - 1];
    }
    for (i = 0; i < UI_TOAST_MAX && msg[i]; i++) toast_msg[pos][i] = msg[i];
    toast_msg[pos][i] = 0;
    toast_kd[pos] = (uint8_t)kind;
    toast_at[pos] = (uint16_t)doomo_frame_count();
    if (pos == 0 && toast_n > 0) toast_gen++;
    toast_n++;
}

void ui_toast(const char *msg) { ui_toast_kind(msg, UI_TOAST_FEEDBACK); }

void ui_toast_unlock(const char *name, const char *what)
{
    if (!name) name = "???";
    if (!news_n) {
        news_age = 0;
        news_name = name;
        news_what = what && what[0] ? what : NULL;
    }
    if (news_n < 99) news_n++;
    news_quiet = 0;
    {
        char lg[64];
        doomo_snprintf(lg, sizeof(lg), "unlock news: %s", name);
        plat_log(lg);
    }
}

int ui_toast_news_waiting(void) { return news_n; }

/* the news is told when it has settled, nothing else waits and no dialogue box is up */
static void toast_news_tick(void)
{
    if (news_gap) news_gap--;
    if (!news_n) return;
    if (news_quiet < UL_NEWS_SETTLE) news_quiet++;
    if (news_age < UL_NEWS_FIRST) news_age++;
    if (news_quiet < UL_NEWS_SETTLE || news_gap || toast_n || ui_dialog_active()) return;
    if (!news_told && news_age < UL_NEWS_FIRST) return;
    {
        char buf[UI_TOAST_MAX + 1];     /* the line is written now, into a whole toast line (no shorter copy) */
        if (news_n > 1) doomo_snprintf(buf, sizeof(buf), "%d new unlocks! See Unlocks", (int)news_n);
        else if (news_what) doomo_snprintf(buf, sizeof(buf), "New %s: %s", news_what, news_name);
        else doomo_snprintf(buf, sizeof(buf), "NEW UNLOCK! %s", news_name);
        ui_toast_kind(buf, UI_TOAST_UNLOCK);
    }
    news_n = 0;
    news_told = 1;
    news_gap = UL_NEWS_GAP;
    audio_sfx(SFX_SPARKLE);
}

void ui_toast_scene_change(int set)
{
    uint16_t now = (uint16_t)doomo_frame_count();
    int i = 0;
    while (i < toast_n) {
        int k = toast_kd[i];
        int keep = toast_at[i] == now || k == UI_TOAST_SYSTEM || (!set && k != UI_TOAST_HINT);
        if (keep) { i++; continue; }
        toast_drop(i);
        if (i == 0) { toast_t = 0; toast_gen++; ui_held &= 2; }
    }
}

int ui_toast_pending(void) { return toast_n; }

const char *ui_toast_text(int i) { return i >= 0 && i < toast_n ? toast_msg[i] : NULL; }

int ui_toast_shown(int kind)
{
    int k, n = 0;
    if (kind >= 0 && kind < UI_TOAST_KINDS) return toast_count[kind];
    for (k = 0; k < UI_TOAST_KINDS; k++) n += toast_count[k];
    return n;
}

void ui_toast_shown_reset(void) { memset(toast_count, 0, sizeof(toast_count)); }

void ui_toast_lane(int y) { toast_y = (int16_t)DOOMO_CLAMP(y, 0, plat_screen_h() - UI_TOAST_H); }

int ui_toast_last_rect(int *x, int *y, int *w, int *h)
{
    if (x) *x = toast_rect[0];
    if (y) *y = toast_rect[1];
    if (w) *w = toast_rect[2];
    if (h) *h = toast_rect[3];
    return toast_rect[2] > 0;
}

/* How long the first toast holds still. A line that is wider than its pill
 * scrolls to its end (1 px a tick after a short beat), so it holds for the
 * scroll too; when others wait behind it, a short line moves along sooner
 * so a burst of toasts clears instead of covering the screen's title row. */
#define TOAST_BEAT 14
#define TOAST_SHOW_QUEUED 48
static int toast_overflow(int w)
{
    int o = ui_text_width(toast_msg[0]) - (w - 22);
    return o > 0 ? o : 0;
}

static int toast_show(int w)
{
    int o = toast_overflow(w);
    if (o > 0) return TOAST_SHOW + TOAST_BEAT + o;
    return toast_n > 1 ? TOAST_SHOW_QUEUED : TOAST_SHOW;
}

void ui_toast_clear(void)
{
    toast_n = 0; toast_t = 0; toast_gen++; ui_held &= 2;
    news_n = 0; news_quiet = 0; news_gap = 0; news_age = 0; news_told = 0;
    news_name = news_what = NULL;
}

/* The toast's left edge on its tick t (width w): slides in, holds, slides out. */
static int toast_x(int t, int w, int show)
{
    int sw = plat_screen_w(), xf = sw - w - 4, d;
    if (t < TOAST_IN) {
        d = TOAST_IN - t;
        return xf + d * d * (sw - xf) / (TOAST_IN * TOAST_IN) - (t >= TOAST_IN - 3 ? 2 : 0);
    }
    if (t < TOAST_IN + show) return xf;
    d = t - TOAST_IN - show;
    return xf + d * d * (sw - xf) / (TOAST_OUT * TOAST_OUT);
}

/* 1 when the toast that ui_toast_draw() draws later in this same frame can
 * touch the rect (x, y, w, h): its box, its drop shadow and its sparkle, with
 * a margin. The HUD asks before a retained panel (platform.h plat_panel_begin):
 * a panel the toast covers is spoiled on every frame, so drawing it plainly
 * is the same picture for less work (a capture costs more than a draw). It
 * only has to be generous: a false 1 costs a capture, never a pixel. */
int ui_toast_over(int x, int y, int w, int h)
{
    int tw, tx, show, ty = toast_y;
    if (toast_n == 0) return 0;
    tw = DOOMO_MIN(ui_text_width(toast_msg[0]) + 22, plat_screen_w() - 8);
    show = toast_show(tw);
    if (toast_t >= TOAST_IN + show + TOAST_OUT) return 0;
    tx = toast_x(toast_t, tw, show);
    /* the toast panel's rect (x, y - 1, w + 2, 18) at this frame's lane, plus 6 px */
    return !(x + w <= tx - 6 || x >= tx + tw + 2 + 6 || y + h <= ty - 1 - 6 || y >= ty - 1 + 18 + 6);
}

/* The toast holds still from the end of its slide-in to the start of its slide-out, but a line wider than its pill
 * also scrolls (toast_overflow) and is steady only once the scroll has ended. The sparkle is the one thing that
 * keeps changing; a scene that keeps the frame lets it step with the scene. */
int ui_toast_steady(void)
{
    int tw, show, o;
    if (toast_n == 0) return 0;
    tw = DOOMO_MIN(ui_text_width(toast_msg[0]) + 22, plat_screen_w() - 8);
    show = toast_show(tw);
    o = toast_overflow(tw);
    if (toast_t < TOAST_IN || toast_t >= TOAST_IN + show) return 0;
    if (o > 0 && toast_t - TOAST_IN - TOAST_BEAT < o) return 0;
    return 1 + toast_gen;
}

void ui_toast_hold(void) { ui_held |= 1; }   /* this frame's toast is already on the kept page */

/* (SP3, I11) the toast panel's rect is (x, y - 1, w + 2, 18) at this frame's lane y (ui_toast_draw): its box, its
 * shadow (rows y + 2 .. y + 16) and its sparkle (y + 2 -/+ 3) all lie in rows y - 1 .. y + 16 */
void ui_toast_band(int *y, int *h) { *y = DOOMO_MAX(toast_y - 1, 0); *h = 18; }

void ui_toast_draw(void)
{
    const UiStyle *st;
    int t, w, x, y = toast_y;
    int saved = ui_cur_style, show, over, off, kept = ui_held & 1;
    ui_held &= 2;
    toast_y = UI_TOAST_LANE_TOP;            /* CL1: a lane lasts one frame */
    toast_rect[2] = 0;
    toast_news_tick();
    if (toast_n == 0) return;
    /* a hint waits for the dialogue box to close (the box is what the player reads first) */
    if (toast_t == 0 && toast_kd[0] == UI_TOAST_HINT && ui_dialog_active()) return;
    t = toast_t++;
    if (t == 0) {
        /* CL1: one count and one log line per toast, when it starts (tests, the runner's toast checks) */
        char lg[72];
        toast_count[toast_kd[0]]++;
        doomo_snprintf(lg, sizeof(lg), "toast: %s", toast_msg[0]);
        plat_log(lg);
    }
    st = ui_style(WIN_CREAM);
    w = DOOMO_MIN(ui_text_width(toast_msg[0]) + 22, plat_screen_w() - 8);
    show = toast_show(w);
    over = toast_overflow(w);
    off = over > 0 ? DOOMO_CLAMP(t - TOAST_IN - TOAST_BEAT, 0, over) : 0;
    x = toast_x(t, w, show);
    toast_rect[0] = (int16_t)x; toast_rect[1] = (int16_t)y;
    toast_rect[2] = (int16_t)(w + 2); toast_rect[3] = UI_TOAST_H;
    /* G7: a retained panel (it holds still for TOAST_SHOW ticks; the
     * sparkle is off for 14 of every 32); a kept toast draws its shadow */
    if (plat_draw_skipped() || kept) {
        /* nothing (a kept frame has this toast on it already: the timer is all that moves) */
    } else if (UI_PANELS && !plat_panel_begin(PANEL_TOAST, x, y - 1, w + 2, 18,
                                              ui_key_int(ui_key_int(ui_key_str(ui_panel_key(), toast_msg[0]), off),
                                                         ui_sparkle_size((t / 2) & 15)))) {
        ui_drop_shadow_under(x + 2, y + 2, w, 15, 4, x, y, w, 15, 4);
        plat_panel_end();
    } else {
        ui_drop_shadow_under(x + 2, y + 2, w, 15, 4, x, y, w, 15, 4);
        ui_round_rect(x, y, w, 15, 4, PAL_RAMP(RAMP_CANDY, 3));
        ui_round_rect(x + 1, y + 1, w - 2, 13, 3, st->fill_hi);
        plat_rect(x + 3, y + 10, w - 6, 3, st->fill);
        plat_rect(x + 5, y + 2, DOOMO_MAX(w / 4, 4), 1, PAL_WHITE);
        ui_star(x + 8, y + 7, 4, PAL_UI_GOLD);
        if (over > 0) ui_text_clip(x + 15 - off, y + 4, lang_tr(toast_msg[0]), st->text, x + 15, x + w - 7);
        else ui_text(x + 15, y + 4, toast_msg[0], st->text);
        ui_sparkle(x + w - 4, y + 2, (t / 2) & 15, PAL_UI_GOLD);
        if (UI_PANELS) plat_panel_end();
    }
    ui_cur_style = saved;
    if (toast_t >= TOAST_IN + show + TOAST_OUT) {
        toast_drop(0);
        toast_t = 0;
        toast_gen++;
    }
}

/* ====================================================================== */
/* decorations                                                              */
void ui_portrait(int x, int y, int resident_id, int scale, int expression)
{
    if (plat_draw_skipped()) return;
    ui_portrait_img(x, y, portrait_image(resident_id, expression), scale);
}

/* ui_portrait() with its image already asked for (portrait_image() renders
 * and animates on the call, so a caller that needs the image first asks
 * only once: G7, ui_dialog.c) */
void ui_portrait_img(int x, int y, const DoomoImage *img, int scale)
{
    const UiStyle *st = ui_style(WIN_MINT);
    int w, h;
    if (plat_draw_skipped()) return;
    if (scale < 1) scale = 1;
    w = (img ? img->w : PORTRAIT_W) * scale + 6;
    h = (img ? img->h : PORTRAIT_H) * scale + 6;
    ui_portrait_shadow(x, y, img, scale);
    ui_round_rect(x, y, w, h, 4, st->outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 3, st->fill_hi);
    plat_rect(x + 2, y + h * 2 / 3, w - 4, h / 3 - 2, st->fill);
    /* little heart wallpaper */
    ui_heart(x + w - 8, y + 8, 2, PAL_RAMP(RAMP_MINT, 5));
    ui_heart(x + 7, y + h / 2, 2, PAL_RAMP(RAMP_MINT, 5));
    if (img) plat_image(x + 3, y + 3, img, scale, 0, 0);
}

/* ui_portrait()'s drop shadow alone, for its image img (G7) */
void ui_portrait_shadow(int x, int y, const DoomoImage *img, int scale)
{
    int w, h;
    if (scale < 1) scale = 1;
    w = (img ? img->w : PORTRAIT_W) * scale + 6;
    h = (img ? img->h : PORTRAIT_H) * scale + 6;
    ui_drop_shadow_under(x + 2, y + 2, w, h, 4, x, y, w, h, 4);
}

void ui_title(int cx, int y, const char *s, int style)
{
    if (plat_draw_skipped()) return;
    const UiStyle *st = ui_style(style);
    int tw = ui_text_width(s), w = tw + 18, x = cx - w / 2;
    int t = frame_t();
    ui_pill_shadow(x + 1, y + 2, w, 13, x, y, w, 13);
    ui_pill(x, y, w, 13, st->title_bg, st->outline, ui_lighter(st->title_bg));
    ui_text_shadow(x + 9, y + 3, s, st->title, st->outline);
    ui_sparkle(x + 3, y + 1, (t / 3) & 15, PAL_WHITE);
    ui_sparkle(x + w - 3, y + 11, (t / 3 + 8) & 15, PAL_WHITE);
}

/* ====================================================================== */
/* C3: the prompt bar's pill and ui_hint. The bar's text is built in ui_prompt.c
 * (not every frame: a memo), so it is out of this per-frame file (the PS1 compiles
 * it for size). */
/* The pill. t is translated already (pieces: its runs between button caps
 * are looked up when drawn, as ui_text does); a retained panel once a
 * frame (a second bar on the same frame is drawn plainly). */
static uint32_t hint_frame = 0xFFFFFFFFu;
void ui_hint_pill_at(const char *t, int pieces, int style, int y)
{
    const UiStyle *st = ui_style(style);
    int sw = plat_screen_w();
    int w = ui_width_raw(t) + 12, x = (sw - w) / 2;
    int panel = UI_PANELS && hint_frame != doomo_frame_count();
    if (panel) {
        hint_frame = doomo_frame_count();
        if (!plat_panel_begin(PANEL_PROMPT, x, y, w, 12,
                              ui_key_int(ui_key_str(ui_panel_key(), t), style * 2 + pieces))) {
            plat_panel_end();                 /* kept: a pill has no shadow */
            return;
        }
    }
    ui_pill(x, y, w, 12, st->title_bg, st->outline, ui_lighter(st->title_bg));
    text_draw(x + 6, y + 2, t, st->title, pieces);
    if (panel) plat_panel_end();
}

static void hint_pill(const char *t, int pieces, int style)
{
    ui_hint_pill_at(t, pieces, style, plat_screen_h() - 13);
}

void ui_hint(const char *s, int style)
{
    const char *t;
    char buf[160];
    int cut = 0, pieces = -1;
    if (!s || plat_draw_skipped()) return;
    t = lang_tr(s);
    if (ui_width_raw(t) > ui_prompts_maxw()) {
        /* the bar's rule (ui_prompts): one space between items first, then whole words go */
        int i, n = 0;
        for (i = 0; t[i] && n < (int)sizeof(buf) - 1; i++)
            if (!(t[i] == ' ' && n && buf[n - 1] == ' ')) buf[n++] = t[i];
        buf[n] = 0;
        if (ui_width_raw(buf) > ui_prompts_maxw()) {
            ui_prompt_cut(buf, ui_prompts_maxw());
            cut = 1;
        }
        t = buf;
        pieces = 0;
    }
#if UI_PROMPT_CHECK
    ui_prompt_note(t, cut);
#else
    (void)cut;                                  /* the consoles keep no stats */
#endif
    hint_pill(t, pieces < 0 ? tr_pieces(t, s) : pieces, style);
}

void ui_button(int x, int y, int w, const char *label, int selected, int style)
{
    if (plat_draw_skipped()) return;
    const UiStyle *st = ui_style(style);
    int tw = ui_text_width(label);
    int b = selected ? (ui_wave(frame_t(), 20, 1) > 0) : 0;
    y -= b;
    if (selected) {
        ui_pill_shadow(x + 1, y + 2 + b, w, 14, x, y, w, 14);
        ui_pill(x, y, w, 14, st->accent, ui_darker(ui_darker(st->accent)), ui_lighter(st->accent));
        ui_text_shadow(x + (w - tw) / 2, y + 3, label, PAL_WHITE, ui_darker(st->accent));
        ui_cursor(x - 9, y + 3);
    } else {
        ui_pill(x, y, w, 14, st->fill_hi, st->outline, PAL_WHITE);
        ui_text(x + (w - tw) / 2, y + 3, label, st->text);
    }
}

/* CL2 (C8 / C11): a row that is there but not available yet. Grey body,
 * dim label, no glow; the cursor still lands on it so the screen's help
 * line can say why. */
void ui_button_off(int x, int y, int w, const char *label, int selected, int style)
{
    if (plat_draw_skipped()) return;
    const UiStyle *st = ui_style(style);
    int tw = ui_text_width(label);
    ui_pill(x, y, w, 14, PAL_GRAY_L, selected ? st->accent : PAL_GRAY, PAL_WHITE);
    ui_text(x + (w - tw) / 2, y + 3, label, selected ? PAL_GRAY_D : PAL_GRAY);
    if (selected) ui_cursor(x - 9, y + 3);
}

void ui_value_row(int x, int y, int w, const char *label, const char *value,
                  int selected)
{
    if (plat_draw_skipped()) return;
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
            ui_pill_shadow(cx + 1, y + 1 - b, tw, 13, cx, y - 2 - b, tw, 13);
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

/* ====================================================================== */
/* retained panels' keys (G7, ui.h)                                         */
uint32_t ui_key_int(uint32_t h, int32_t v)
{
    h ^= (uint32_t)v;
    h *= 16777619u;
    return h ^ (h >> 15);
}

uint32_t ui_key_str(uint32_t h, const char *s)
{
    if (!s) return ui_key_int(h, -1);
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return ui_key_int(h, 0x5EED);
}

uint32_t ui_panel_key(void)
{
    int l = lang_get();
    uint32_t h = ui_key_int(2166136261u, l);
    if (l >= 0 && l < LANG_COUNT) h = ui_key_int(h, doomo_lang_info[l].page);
    /* the options every UI drawing reads */
    h = ui_key_int(h, opt_get(OPT_FONT_STYLE) | opt_get(OPT_UI_BOUNCE) << 4 |
                      opt_get(OPT_WINDOW_SKIN) << 8 | opt_get(OPT_CURSOR) << 12);
    return h;
}
