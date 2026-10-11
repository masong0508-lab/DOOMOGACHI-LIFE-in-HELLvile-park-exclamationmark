/* fp_news.c - SCENE_NEWS: The Hellvile Herald (owner: freeplay).
 *
 * Two pages (L / R): Headlines (the sim's news feed, newest first, each
 * story with a little icon guessed from what happened) and Help Wanted
 * (every citizen problem on the island; A jumps to that citizen).
 */
#include "fp_int.h"

static struct { int tab, sel, top, t; } W;

static const char *const tabs[2] = { "Headlines", "Help wanted" };

static int has(const char *s, const char *w) { return fp_strcasestr(s, w); }

static void story_icon(const char *s, uint8_t *icon, uint8_t *color)
{
    *icon = ICON_STAR;
    *color = PAL_UI_GOLD;
    if (has(s, "possess") || has(s, "demon") || has(s, "MONSTER")) { *icon = ICON_PENTAGRAM; *color = PAL_UI_RED; }
    else if (has(s, "ghost") || has(s, "rest") || has(s, "vanished") || has(s, "grave")) { *icon = ICON_GHOST; *color = PP(PAINT_CYAN, 0); }
    else if (has(s, "married") || has(s, "wedding") || has(s, "dating") || has(s, "love") || has(s, "crush") || has(s, "engaged")) { *icon = ICON_HEART; *color = PP(PAINT_ROSE, 1); }
    else if (has(s, "baby")) { *icon = ICON_EGG; *color = PP(PAINT_LEMON, 1); }
    else if (has(s, "broke up") || has(s, "fight") || has(s, "argu") || has(s, "rival") || has(s, "nemes")) { *icon = ICON_BOLT; *color = PP(PAINT_TANGERINE, 0); }
    else if (has(s, "friend")) { *icon = ICON_SMILE; *color = PP(PAINT_MINT, 0); }
    else if (has(s, "exorcis")) { *icon = ICON_CROSS; *color = PP(PAINT_LAVENDER, 0); }
    else if (has(s, "concert") || has(s, "sing") || has(s, "song")) { *icon = ICON_NOTE; *color = PP(PAINT_GRAPE, 1); }
    else if (has(s, "picnic") || has(s, "cake") || has(s, "ate ")) { *icon = ICON_FOOD; *color = PP(PAINT_CARAMEL, 0); }
}

static int count(void) { return W.tab == 0 ? sim_news_count() : sim_problem_count(); }

static void news_enter(int arg)
{
    (void)arg;
    memset(&W, 0, sizeof(W));
}

static int vis(void) { return fp_big() ? 7 : 4; }

static void news_update(void)
{
    int n;
    fp_live();
    W.t++;
    n = count();
    if (btn_pressed(BTN_L | BTN_R | BTN_SELECT)) { W.tab ^= 1; W.sel = W.top = 0; audio_sfx(SFX_BLIP); return; }
    if (n) {
        if (btn_repeat(BTN_UP)) { W.sel = (W.sel + n - 1) % n; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { W.sel = (W.sel + 1) % n; audio_sfx(SFX_CURSOR); }
        W.sel = DOOMO_MIN(W.sel, n - 1);
        if (W.sel < W.top) W.top = W.sel;
        if (W.sel >= W.top + vis()) W.top = W.sel - vis() + 1;
        if (W.tab == 1 && btn_pressed(BTN_A)) {
            const DoomoProblem *p = sim_problem(W.sel);
            if (p) { audio_sfx(SFX_SELECT); fp_open_resident(p->resident); }
            return;
        }
    }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop();
        else fp_goto_island(FP_ISLAND_ARG_RETURN);
    }
}

static void news_draw(void)
{
    int sw = fp_sw(), big = fp_big(), i, n = count(), t = W.t;
    int x = 8, w = sw - 16, y0 = big ? 50 : 40, rh = big ? 24 : 24;
    const UiStyle *st = ui_style(WIN_CREAM);
    char buf[80];
    int m, d;
    ui_backdrop(WIN_CREAM, (uint32_t)fp_t());
    /* masthead */
    sim_date(&m, &d);
    ui_window(4, 3, sw - 8, big ? 30 : 26, WIN_CREAM);
    ui_text_big(sw / 2 - ui_text_big_width("Hellvile Herald", big ? 2 : 1) / 2, 6, "Hellvile Herald",
                big ? 2 : 1, PAL_RAMP(RAMP_WOOD, 1), PAL_RAMP(RAMP_PEACH, 5));
    doomo_snprintf(buf, sizeof(buf), "Day %u - %s %d %s - only 1 H$", (unsigned)doomo_town.day + 1,
                   doomo_weekday_names[doomo_town.weekday % 7], d, doomo_month_names[m % 12]);
    ui_text_center(sw / 2, big ? 22 : 16, buf, st->text_dim);
    ui_tabs(x, y0 - 13, tabs, 2, W.tab, WIN_CREAM);
    ui_window(x - 2, y0 - 2, w + 4, vis() * rh + 4, WIN_CREAM);
    if (!n) {
        ui_text_center(sw / 2, y0 + 20, W.tab ? "Nobody needs help. Bliss!" : "No news is good news!", st->text_dim);
    }
    for (i = 0; i < vis() && W.top + i < n; i++) {
        int idx = W.top + i, y = y0 + i * rh, sel = idx == W.sel;
        uint8_t icon, color;
        const char *s;
        if (W.tab == 0) {
            s = sim_news(idx);
            if (!s) continue;
            story_icon(s, &icon, &color);
        } else {
            sim_problem_text(idx, buf, sizeof(buf));
            s = buf;
            icon = ICON_CLOUD;
            color = PAL_UI_RED;
        }
        if (sel) {
            fp_pill(x + 6, y, w - 8, rh - 2, st->accent_d, ui_darker(st->accent_d), ui_lighter(st->accent_d));
            ui_cursor(x - 2, y + 4);
        } else if (i) {
            plat_checker(x + 10, y - 1, w - 20, 1, st->fill_lo);
        }
        fp_icon(x + 10, y + 3, icon, color);
        ui_text_wrap(x + 20, y + 2, w - 26, s, st->text, 1);
    }
    if (n > vis()) ui_scroll_arrows(x + w - 4, y0 - 6, y0 + vis() * rh - 2, W.top > 0, W.top + vis() < n);
    ui_hint(W.tab ? STR_ICON_A " Visit  L/R Page  " STR_ICON_B " Back" : "L/R Page  " STR_ICON_B " Back", WIN_CREAM);
    (void)t;
    fp_fx_draw();
}

const DoomoScene SCENE_NEWS = { "NEWS", news_enter, NULL, news_update, news_draw, 0 };
