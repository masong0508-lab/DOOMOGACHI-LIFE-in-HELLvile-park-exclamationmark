/* ui_prompt.c - C3: the shared prompt bar's text (ui.h ui_prompts,
 * ui_prompts_text, ui_prompt_cut, btn_name per pad) and the native
 * runner's prompt checks (owner: foundation). The pill itself is drawn
 * by ui.c (ui_hint_pill_at). Not per-frame work: a screen that shows
 * the same bar frame after frame builds it once (pmemo), so the PS1
 * compiles this file for size, unlike ui.c (ps1/CMakeLists.txt
 * DOOMO_CORE_HOT). */
#include "ui_internal.h"

#if UI_PROMPT_CHECK
static UiPromptStats pstats;

/* a button word this pad does not have (only the GBA is checked: the one
 * pad with no X / Y and no numbered shoulders) */
static int prompt_bad_word(const char *s)
{
    static const char *const bad[] = { "X", "Y", "L1", "R1", "L2", "R2", "Square", "Triangle", "DualShock" };
    char w[16];
    int n = 0, i;
    if (input_pad() != PAD_GBA) return 0;
    for (;; s++) {
        char c = *s;
        if (c == 0 || c == ' ' || c == '/' || c == ':' || c == '(' || c == ')' || c == ',' || c == '+' ||
            (unsigned char)c >= 0x80) {
            w[n] = 0;
            for (i = 0; n && i < (int)DOOMO_COUNTOF(bad); i++)
                if (strcmp(w, bad[i]) == 0) return 1;
            n = 0;
            if (c == 0) return 0;
            continue;
        }
        if (n < (int)sizeof(w) - 1) w[n++] = c;
    }
}

void ui_prompt_note(const char *t, int cut)
{
    int bad = prompt_bad_word(t);
    pstats.bars++;
    if (cut) pstats.too_wide++;
    if (bad) pstats.wrong_pad++;
    doomo_snprintf(pstats.last, sizeof(pstats.last), "%s", t);
    if ((cut || bad) && !pstats.first_bad[0])
        doomo_snprintf(pstats.first_bad, sizeof(pstats.first_bad), "%s%s", cut ? "(too wide) " : "", t);
}
#endif

void ui_prompt_stats(UiPromptStats *out, int reset)
{
#if UI_PROMPT_CHECK
    if (out) *out = pstats;
    if (reset) memset(&pstats, 0, sizeof(pstats));
#else
    (void)reset;
    if (out) memset(out, 0, sizeof(*out));
#endif
}

int ui_prompts_maxw(void) { return plat_screen_w() - 16; }

/* t when it is at most maxw wide, else its longest prefix of whole words
 * that is (no trailing blank); a first word wider than maxw keeps the
 * glyphs that fit. Measured on the whole string and on each word's end. */
void ui_prompt_cut(char *t, int maxw)
{
    int i, last = -1;
    if (ui_width_raw(t) <= maxw) return;
    for (i = 1; t[i]; i++) {
        if (t[i] != ' ' || t[i - 1] == ' ') continue;   /* the end of a word: the first blank after it */
        t[i] = 0;
        if (ui_width_raw(t) > maxw) { t[i] = ' '; break; }
        t[i] = ' ';
        last = i;
    }
    if (last >= 0) { t[last] = 0; return; }
    for (i = (int)strlen(t); i > 0 && ui_width_raw(t) > maxw;) t[--i] = 0;
}

/* (no printf: the GBA builds a bar on its frames) append s to g[*k] (n bytes) */
static void pg_add(char *g, int *k, int n, const char *s)
{
    while (s && *s && *k < n - 1) g[(*k)++] = *s++;
    g[*k] = 0;
}

/* one item's button part into g (n bytes); 0 = this pad has no such button */
static int prompt_glyph(uint32_t b, char *g, int n)
{
    const char *a, *c;
    int k = 0;
    g[0] = 0;
    if (!b) return 1;
    if (b == BTN_A) { pg_add(g, &k, n, STR_ICON_A); return 1; }
    if (b == BTN_B) { pg_add(g, &k, n, STR_ICON_B); return 1; }
    if ((b & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)) == b) {
        if (b & BTN_UP) pg_add(g, &k, n, STR_ICON_ARROW_U);
        if (b & BTN_DOWN) pg_add(g, &k, n, STR_ICON_ARROW_D);
        if (b & BTN_LEFT) pg_add(g, &k, n, STR_ICON_ARROW_L);
        if (b & BTN_RIGHT) pg_add(g, &k, n, STR_ICON_ARROW_R);
        return 1;
    }
    if (b == (BTN_L | BTN_R)) {
        a = btn_name(BTN_L); c = btn_name(BTN_R);
        if (!a || !c) return 0;
        pg_add(g, &k, n, a); pg_add(g, &k, n, "/"); pg_add(g, &k, n, c);
        return 1;
    }
    /* the GBA's X / Y are SELECT + the A / B cap */
    if (input_pad() == PAD_GBA && (b == BTN_X || b == BTN_Y)) {
        pg_add(g, &k, n, btn_name(BTN_SELECT)); pg_add(g, &k, n, "+");
        pg_add(g, &k, n, b == BTN_X ? STR_ICON_A : STR_ICON_B);
        return 1;
    }
    if ((b & BTN_SELECT) && b != BTN_SELECT) {         /* a chord with SELECT */
        char o[24];
        a = btn_name(BTN_SELECT);
        if (!a || !prompt_glyph(b & ~(uint32_t)BTN_SELECT, o, (int)sizeof(o))) return 0;
        pg_add(g, &k, n, a); pg_add(g, &k, n, "+"); pg_add(g, &k, n, o);
        return 1;
    }
    a = btn_name(b);
    if (!a) return 0;
    pg_add(g, &k, n, a);
    return 1;
}

static int prompt_build(char *out, int n, const UiPrompt *p, int count, const uint8_t *on, int sep)
{
    int i, len = 0, first = 1;
    out[0] = 0;
    for (i = 0; i < count; i++) {
        char g[24];
        const char *v;
        if (!on[i]) continue;
        prompt_glyph(p[i].btn, g, (int)sizeof(g));
        v = p[i].verb && p[i].verb[0] ? lang_tr(p[i].verb) : "";
        if (!first) pg_add(out, &len, n, sep == 2 ? "  " : " ");
        pg_add(out, &len, n, g);
        if (g[0] && v[0]) pg_add(out, &len, n, " ");
        pg_add(out, &len, n, v);
        first = 0;
    }
    return ui_width_raw(out);
}

int ui_prompts_text(char *out, int n, const UiPrompt *p, int count, int maxw)
{
    uint8_t on[UI_PROMPTS_MAX];
    int i, left = 0, w;
    char g[24];
    if (count > UI_PROMPTS_MAX) count = UI_PROMPTS_MAX;
    for (i = 0; i < count; i++) {
        on[i] = (uint8_t)prompt_glyph(p[i].btn, g, (int)sizeof(g));
        left += on[i];
    }
    for (;;) {
        int k = -1;
        w = prompt_build(out, n, p, count, on, 2);
        if (w <= maxw) return w;
        w = prompt_build(out, n, p, count, on, 1);
        if (w <= maxw) return w;
        for (i = 0; i < count; i++)
            if (on[i] && p[i].prio && (k < 0 || p[i].prio >= p[k].prio)) k = i;
        if (k < 0 || left <= 1) return w;
        on[k] = 0;
        left--;
    }
}

void ui_prompts(const UiPrompt *p, int count, int style)
{
    ui_prompts_at(p, count, style, plat_screen_h() - 13, ui_prompts_maxw());
}

/* the last bar built, kept: a screen asks for the same bar frame after frame (the GBA builds nothing then) */
static struct { uint32_t key; uint8_t cut; char text[160]; } pmemo;

void ui_prompts_at(const UiPrompt *p, int count, int style, int y, int maxw)
{
    char *buf = pmemo.text;
    int cut, i;
    uint32_t key = 2166136261u;
    if (!p || count <= 0 || plat_draw_skipped()) return;
    key = ui_key_int(key, count | maxw << 8 | input_pad() << 20 | lang_get() << 24);
    for (i = 0; i < count; i++)
        key = ui_key_str(ui_key_int(key, p[i].btn | p[i].prio << 16), p[i].verb ? p[i].verb : "");   /* the text: a verb may be a buffer */
    if (!key) key = 1;
    if (key != pmemo.key) {
        cut = 0;
        if (ui_prompts_text(buf, (int)sizeof(pmemo.text), p, count, maxw) > maxw) {
            ui_prompt_cut(buf, maxw);
            cut = 1;
        }
        pmemo.key = key;
        pmemo.cut = (uint8_t)cut;
    }
    cut = pmemo.cut;
#if UI_PROMPT_CHECK
    ui_prompt_note(buf, cut);
#else
    (void)cut;
#endif
    ui_hint_pill_at(buf, 0, style, y);
}
