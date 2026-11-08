/* fp_news.c - SCENE_NEWS: The Hellvile Herald (owner: freeplay).
 *
 * Two pages (L / R): Headlines (the sim's news feed, newest first, each
 * story with a little icon guessed from what happened) and Help Wanted
 * (every citizen problem on the island; A jumps to that citizen).
 */
#include "fp_int.h"

static struct { int tab, sel, top, t; } W;

static const char *const tabs[2] = { "Headlines", "Help wanted" };

/* (perf, X2) The icon of a headline: the first group below (in order) with a keyword anywhere in the text, case
 * insensitive (fp_strcasestr's rule), else the star. Exactly the old chain of up to 33 fp_strcasestr() scans per row
 * per frame (29.7% of the GBA news screen), in ONE pass: the keywords are sorted by their first letter, and at each
 * letter of the text only the keywords starting with it are compared (news_kw_first). The text has to be read whole
 * (names, venues and foods are part of it: "Kate " has "ate "), so the pass runs per drawn row;
 * tests/test_freeplay.c checks it against the old chain.
 * Groups: 0 pentagram (possess demon monster), 1 ghost (ghost rest vanished grave), 2 heart (married wedding
 * dating love crush engaged), 3 egg (baby), 4 bolt (broke up, fight argu rival nemes), 5 smile (friend), 6 cross
 * (exorcis), 7 note (concert sing song), 8 food (picnic cake "ate "). */
static const char *const news_kw[] = {
    "argu", "ate ", "baby", "broke up", "crush", "concert", "cake", "demon", "dating", "engaged", "exorcis",
    "fight", "friend", "ghost", "grave", "love", "monster", "married", "nemes", "possess", "picnic", "rest",
    "rival", "sing", "song", "vanished", "wedding"
};
static const uint8_t news_kw_group[] = {
    4, 8, 3, 4, 2, 7, 8, 0, 2, 2, 6, 4, 5, 1, 1, 2, 0, 2, 4, 0, 8, 1, 4, 7, 7, 1, 2
};
/* the keywords starting with letter L are news_kw[news_kw_first[L - 'a'] .. news_kw_first[L - 'a' + 1] - 1] */
static const uint8_t news_kw_first[27] = {
    0, 2, 4, 7, 9, 11, 13, 15, 15, 15, 15, 15, 16, 18, 19, 19, 21, 21, 23, 25, 25, 25, 26, 27, 27, 27, 27
};
DOOMO_STATIC_ASSERT(sizeof(news_kw_group) == sizeof(news_kw) / sizeof(news_kw[0]), news_kw_groups_match);

static int news_lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

int fp_news_icon(const char *s, uint8_t *icon, uint8_t *color)
{
    static const uint8_t icons[9] = { ICON_PENTAGRAM, ICON_GHOST, ICON_HEART, ICON_EGG, ICON_BOLT, ICON_SMILE,
                                      ICON_CROSS, ICON_NOTE, ICON_FOOD };
    int best = 9, i, k;
    for (i = 0; s[i] && best > 0; i++) {
        int c = news_lower((unsigned char)s[i]), end;
        if (c < 'a' || c > 'z') continue;           /* every keyword starts with a letter */
        end = news_kw_first[c - 'a' + 1];
        for (k = news_kw_first[c - 'a']; k < end; k++) {
            const char *w = news_kw[k];
            int j;
            if (news_kw_group[k] >= best) continue;
            for (j = 1; w[j] && s[i + j]; j++)
                if (news_lower((unsigned char)s[i + j]) != w[j]) break;
            if (!w[j]) best = news_kw_group[k];
        }
    }
    *icon = ICON_STAR;
    *color = PAL_UI_GOLD;
    if (best < 9) {
        static const uint8_t colors[9] = { PAL_UI_RED, PP(PAINT_CYAN, 0), PP(PAINT_ROSE, 1), PP(PAINT_LEMON, 1),
                                           PP(PAINT_TANGERINE, 0), PP(PAINT_MINT, 0), PP(PAINT_LAVENDER, 0),
                                           PP(PAINT_GRAPE, 1), PP(PAINT_CARAMEL, 0) };
        *icon = icons[best];
        *color = colors[best];
    }
    return best;
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
            fp_news_icon(s, &icon, &color);
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
