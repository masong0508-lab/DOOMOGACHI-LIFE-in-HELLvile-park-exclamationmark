/* story_questlog.c - SCENE_QUESTLOG: the story's notebook (owner: story).
 * Tabs: Story (chapter, objective, the beats so far), Side (every side
 * quest you can see, with its giver, hint and reward state), Items (the
 * inventory), Map (the current overworld, where you are and where to go),
 * Friends (the cast you've met, friends on both sides) and Trophies (S2,
 * SPEC R8: the trophy of every finished side mission S100..S217, read from
 * STRY.side2; trophies are not unlock ids). arg = first tab. L/R or
 * LEFT/RIGHT switch tabs, B closes.
 */
#include "overworld_int.h"

enum { QT_STORY, QT_SIDE, QT_ITEMS, QT_MAP, QT_FRIENDS, QT_TROPHIES, QT_COUNT };
static const char *const tab_names[QT_COUNT] = { "Story", "Side", "Items", "Map", "Friends", "Trophies" };
/* 240 wide: the Trophies tab is its star, so the six tabs fit the window */
static const char *const tab_names_narrow[QT_COUNT] = { "Story", "Side", "Items", "Map", "Friends", STR_ICON_STAR };

static int tab, sel, top;
static uint32_t qt;
/* (S4) one list for the open tab (refilled on every tab change): the Side
 * tab's rows (an old quest q 0..48, or SIDE2_ROW + an S100..S217 row), the
 * Items tab's item ids or the Trophies tab's rows. One buffer instead of
 * three saves ~110 B of .bss (GBA EWRAM). */
#define SIDE2_ROW 64
#define LIST_MAX (STORY_SIDE_MAX + STORY_SIDE2_ROWS)
DOOMO_STATIC_ASSERT(SIDE2_ROW >= STORY_SIDE_MAX && SIDE2_ROW + STORY_SIDE2_ROWS <= 255 &&
                    LIST_MAX >= ITEM_COUNT, questlog_list_fits);
static uint8_t list[LIST_MAX];
static int nlist;
#define side_ids list
#define item_ids list
#define trophy_ids list
#define nside nlist
#define nitems nlist
#define ntrophies nlist

/* the side rows by state class: 0 under way, 1 on offer, 2 done */
static int side_class(int e)
{
    int st;
    if (e >= SIDE2_ROW) {
        st = story_side2_state(e - SIDE2_ROW);
        return st == 2 ? 0 : st == 3 ? 2 : 1;
    }
    st = story_side_state(e);
    return (st == SIDE_ACTIVE || st == SIDE_READY) ? 0 : st == SIDE_DONE ? 2 : 1;
}

static void collect(void)
{
    int q, i, c;
    nlist = 0;
    if (tab == QT_SIDE) {
        uint8_t all[LIST_MAX];
        int n = 0;
        for (q = 0; q < story_side_count() && q < STORY_SIDE_MAX; q++)
            if (story_side_state(q) != SIDE_LOCKED) all[n++] = (uint8_t)q;
        for (q = 0; q < STORY_SIDE2_ROWS; q++)
            if (story_side2_visible(q)) all[n++] = (uint8_t)(SIDE2_ROW + q);
        for (c = 0; c < 3; c++)
            for (i = 0; i < n; i++) if (side_class(all[i]) == c) list[nlist++] = all[i];
    } else if (tab == QT_ITEMS) {
        for (i = 1; i < ITEM_COUNT; i++) if (inv_count(i) > 0) item_ids[nitems++] = (uint8_t)i;
    } else if (tab == QT_TROPHIES) {
        ntrophies = story_trophies(doomo_side_rows, story_stry()->side2, trophy_ids, STORY_SIDE2_ROWS);
    }
}

static void ql_enter(int arg)
{
    tab = (arg >= 0 && arg < QT_COUNT) ? arg : 0;
    sel = top = 0;
    qt = 0;
    collect();
}

static int list_len(void)
{
    if (tab == QT_SIDE) return nside;
    if (tab == QT_ITEMS) return nitems;
    if (tab == QT_FRIENDS) return 12;
    if (tab == QT_TROPHIES) return ntrophies;
    return 0;
}

static void ql_update(void)
{
    int n = list_len(), vis = plat_screen_h() >= 200 ? 9 : 5;
    qt++;
    if (btn_pressed(BTN_B) || btn_pressed(BTN_START)) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop(); else scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
        return;
    }
    if (btn_repeat(BTN_L | BTN_LEFT)) { tab = (tab + QT_COUNT - 1) % QT_COUNT; sel = top = 0; qt = 0; audio_sfx(SFX_CURSOR); collect(); }
    if (btn_repeat(BTN_R | BTN_RIGHT)) { tab = (tab + 1) % QT_COUNT; sel = top = 0; qt = 0; audio_sfx(SFX_CURSOR); collect(); }
    n = list_len();
    if (n > 0) {
        if (btn_repeat(BTN_UP)) { sel = (sel + n - 1) % n; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { sel = (sel + 1) % n; audio_sfx(SFX_CURSOR); }
        if (sel < top) top = sel;
        if (sel >= top + vis) top = sel - vis + 1;
    }
}

/* ---- tabs ---------------------------------------------------------------------- */
static int style_now(void)
{
    int m = story_map();
    return m == MAPID_HAHALAND_INVADED ? WIN_HELL : m == MAPID_HELLVILE ? WIN_DARK : WIN_CUTE;
}

static void draw_story(int x, int y, int w, int h, int style)
{
    const UiStyle *st = ui_style(style);
    int ch = story_chapter(), b = story_beat(), i, yy, done_n = 0, total = 0;
    char buf[64];
    if (ch == STORY_CH_DONE) doomo_snprintf(buf, sizeof(buf), "Epilogue: %s", story_chapter_title(ch));
    else doomo_snprintf(buf, sizeof(buf), "Chapter %d: %s", ch, story_chapter_title(ch));
    ui_text(x, y, buf, st->title);
    yy = y + 12;
    plat_text(x, yy, STR_ICON_STAR, PAL_UI_GOLD);
    yy += ui_text_wrap(x + 9, yy, w - 12, story_objective(), st->text, 1) * FONT_LINE + 4;
    /* progress bar over the whole story */
    for (i = 0; i < BEAT_COUNT; i++) { total++; if (i < b) done_n++; }
    ui_bar(x, yy, w - 4, 6, done_n, total - 1, PAL_UI_PINK);
    yy += 11;
    /* this chapter's beats */
    for (i = 0; i < BEAT_COUNT && yy < y + h - 9; i++) {
        static const char *const nice[BEAT_COUNT] = {
            "Arrived in HaHaland", "Met Mayor Giggs", "Got the house key", "Went home", "Hatched @E",
            "Met Pip", "Found Mr. Flops", "Cheered up Pip", "Made friends", "Delivered a cupcake",
            "Saw the purple fog", "Cloud Nine opened", "Folks got weird", "Steamy screamed",
            "Met Father Fuzz", "First exorcism", "Rivet's welding lesson", "The Vape Cellar",
            "The gate erupted", "Cleared the nests", "Cloud Nine Spire", "Crossed the Wall",
            "Met Hellvile", "Helped Hellvile", "The summit", "The festival", "The parade", "Free play!"
        };
        char line[64];
        if (story_beat_chapter(i) != (ch == STORY_CH_DONE ? 5 : ch)) continue;
        story_fmt(line, sizeof(line), nice[i]);
        if (i < b || b == BEAT_DONE) {
            plat_text(x + 2, yy, STR_ICON_CHECK, PAL_UI_MINT_D);
            ui_text(x + 11, yy, line, st->text_dim);
        } else if (i == b) {
            ui_cursor(x - 2, yy + 3);
            ui_text(x + 11, yy, line, st->text);
        }
        yy += FONT_LINE;
    }
}

/* (S4) an S100+ row's state as a SIDE_* (on offer / under way / done) */
static int side_state_of(int e)
{
    int st;
    if (e < SIDE2_ROW) return story_side_state(e);
    st = story_side2_state(e - SIDE2_ROW);
    return st == 2 ? SIDE_ACTIVE : st == 3 ? SIDE_DONE : SIDE_OFFERED;
}

/* (S4) an S100+ mission's kind: its first verb (the bit order of
 * core/tools/textpack.py VERBS; every mission talks, "talk" has no bit) */
static const char *side2_kind_name(int sid)
{
    static const char *const names[16] = {
        "Fetch", "Deliver", "Find", "Photo", "Fight", "Exorcism", "Weld", "Pet",
        "Minigame", "Fishing", "Cooking", "Music", "Housing", "Creator", "Escort", "Timing"
    };
    unsigned v = doomo_side_rows[sid].verbs;
    int k = 0;
    if (!v) return "Talk";
    while (!(v & 1u)) { v >>= 1; k++; }
    return names[k & 15];
}

static void draw_side(int x, int y, int w, int h, int style)
{
    const UiStyle *st = ui_style(style);
    int vis = plat_screen_h() >= 200 ? 9 : 5, i, list_h;
    char buf[64];
    if (!nside) {
        ui_text_wrap(x, y + 10, w, "No side quests yet. Look for citizens with a pink ? bubble over their heads!", st->text, 1);
        return;
    }
    list_h = vis * 11;
    for (i = top; i < nside && i < top + vis; i++) {
        int q = side_ids[i], s = side_state_of(q), yy = y + (i - top) * 11;
        const char *icon = s == SIDE_DONE ? STR_ICON_CHECK : s == SIDE_READY ? STR_ICON_HEART : s == SIDE_ACTIVE ? STR_ICON_STAR : "?";
        if (i == sel) {
            ui_round_rect(x - 2, yy - 1, w, 10, 3, st->accent);
            ui_cursor(x - 6, yy + 3);
        }
        plat_text(x, yy, icon, s == SIDE_DONE ? PAL_UI_MINT_D : s == SIDE_READY ? PAL_UI_RED : PAL_UI_GOLD);
        doomo_snprintf(buf, sizeof(buf), "%s", q >= SIDE2_ROW ? story_side2_title(q - SIDE2_ROW) : story_side_title(q));
        ui_text(x + 9, yy, buf, s == SIDE_DONE ? st->text_dim : st->text);
    }
    ui_scroll_arrows(x + w - 6, y - 2, y + list_h - 6, top > 0, top + vis < nside);
    /* details */
    if (sel < nside) {
        int q = side_ids[sel], s = side_state_of(q), dy = y + list_h + 4;
        int g = q >= SIDE2_ROW ? (int)doomo_side_rows[q - SIDE2_ROW].giver : story_side_giver(q);
        const DoomoResident *gr = town_res(g);
        const char *kind = q >= SIDE2_ROW ? side2_kind_name(q - SIDE2_ROW) : story_side_kind_name(story_side_kind(q));
        plat_rect(x, dy - 2, w - 4, 1, st->outline);
        ui_portrait(x, dy + 1, g, 1, s == SIDE_DONE ? EXPR_HAPPY : EXPR_NEUTRAL);
        doomo_snprintf(buf, sizeof(buf), "%s (%s)", gr ? gr->name : "?", kind);
        ui_text(x + PORTRAIT_W + 4, dy + 2, buf, st->title);
        ui_text_wrap(x + PORTRAIT_W + 4, dy + 12, w - PORTRAIT_W - 8,
                     s == SIDE_DONE ? "All done! Thank you!" : s == SIDE_READY ? "Done! Go back and tell them." :
                     q >= SIDE2_ROW ? story_side2_hint(q - SIDE2_ROW) : story_side_hint(q), st->text, 1);
        (void)h;
    }
}

static void draw_items(int x, int y, int w, int h, int style)
{
    const UiStyle *st = ui_style(style);
    int vis = plat_screen_h() >= 200 ? 9 : 5, i;
    char buf[48];
    if (!nitems) { ui_text(x, y + 10, "Your bag is empty!", st->text); return; }
    for (i = top; i < nitems && i < top + vis; i++) {
        int it = item_ids[i], yy = y + (i - top) * 11;
        char icon[2] = { (char)doomo_items[it].icon, 0 };
        if (i == sel) { ui_round_rect(x - 2, yy - 1, w, 10, 3, st->accent); ui_cursor(x - 6, yy + 3); }
        plat_text(x, yy, icon[0] ? icon : STR_ICON_STAR, PAL_UI_GOLD);
        doomo_snprintf(buf, sizeof(buf), "%s", doomo_items[it].name);
        ui_text(x + 9, yy, buf, st->text);
        doomo_snprintf(buf, sizeof(buf), "x%d", inv_count(it));
        ui_text(x + w - 8 - ui_text_width(buf), yy, buf, st->text_dim);
    }
    ui_scroll_arrows(x + w - 6, y - 2, y + vis * 11 - 6, top > 0, top + vis < nitems);
    if (sel < nitems) {
        int dy = y + vis * 11 + 4;
        plat_rect(x, dy - 2, w - 4, 1, st->outline);
        ui_text_wrap(x, dy + 2, w - 4, doomo_items[item_ids[sel]].desc, st->text, 1);
    }
    (void)h;
}

static void draw_map(int x, int y, int w, int h, int style)
{
    const UiStyle *st = ui_style(style);
    int s = (h >= 140 && w >= 140) ? 2 : 1, mw = doomo_map.w * s, mh = doomo_map.h * s;
    int mx = x + (w - mw) / 2, my = y + 2;
    const char *t = map_title(story_map());
    if (mh > h - 14) { s = 1; mw = doomo_map.w; mh = doomo_map.h; mx = x + (w - mw) / 2; }
    owfx_minimap(mx, my, s, story_map());
    ui_text_center(x + w / 2, my + mh + 4, t && t[0] ? t : "", st->title);
    if (mx > x + 50) {
        ui_text(x, y + 4, STR_ICON_STAR " goal", st->text);
        ui_text(x, y + 14, "\x9c you", st->text_dim);
    }
}

static void draw_friends(int x, int y, int w, int h, int style)
{
    static const int16_t cast[12] = { ID_MAYOR, ID_NANA, ID_PIP, ID_FUZZ, ID_RIVET, ID_BLOOM,
                                      ID_VEX, ID_LULLA, ID_MOTHER_NOX, ID_SOOTKIN, ID_ANVIL, ID_TALLYWORN };
    const UiStyle *st = ui_style(style);
    int i, cols = w >= 260 ? 6 : 4, cw = w / cols, rows = (12 + cols - 1) / cols;
    char buf[48];
    doomo_snprintf(buf, sizeof(buf), STR_ICON_HEART " Hillvile %d  " STR_ICON_HEART " Hellvile %d",
                   story_friend_count(0), story_friend_count(1));
    ui_text(x, y, buf, st->text);
    for (i = 0; i < 12; i++) {
        int cx = x + (i % cols) * cw, cy = y + 12 + (i / cols) * ((h - 14) / rows);
        const DoomoResident *r = town_res(cast[i]);
        const DoomoImage *im;
        int met = r && r->level > 0;
        if (!r) continue;
        im = bodyspr_image(cast[i], 0, 0);
        if (i == sel) ui_round_rect(cx, cy, cw - 2, (h - 14) / rows - 2, 3, st->accent);
        if (im) plat_image(cx + (cw - BODYSPR_W) / 2, cy + 2, im, 1, met ? 0 : IMG_SILHOUETTE, st->text_dim);
        ui_text_center(cx + cw / 2, cy + BODYSPR_H + 4, met ? r->name : "???", met ? st->text : st->text_dim);
    }
}

/* S2 (R8): one row per trophy; the selected one shows its mission */
static void draw_trophies(int x, int y, int w, int h, int style)
{
    const UiStyle *st = ui_style(style);
    int vis = plat_screen_h() >= 200 ? 9 : 5, i;
    char buf[64];
    doomo_snprintf(buf, sizeof(buf), STR_ICON_STAR " %d %s", ntrophies, ntrophies == 1 ? "trophy" : "trophies");
    ui_text(x, y, buf, st->text);
    if (!ntrophies) {
        ui_text_wrap(x, y + 14, w, "No trophies yet. Every side mission you finish puts one on your shelf!",
                     st->text, 1);
        return;
    }
    y += 12;
    for (i = top; i < ntrophies && i < top + vis; i++) {
        const StorySideRow *r = &doomo_side_rows[trophy_ids[i]];
        int yy = y + (i - top) * 11;
        if (i == sel) { ui_round_rect(x - 2, yy - 1, w, 10, 3, st->accent); ui_cursor(x - 6, yy + 3); }
        plat_text(x, yy, STR_ICON_STAR, PAL_UI_GOLD);
        if (txt_get(r->trophy, buf, (int)sizeof(buf)) < 0) buf[0] = 0;
        ui_text(x + 9, yy, buf, st->text);
    }
    ui_scroll_arrows(x + w - 6, y - 2, y + vis * 11 - 6, top > 0, top + vis < ntrophies);
    if (sel < ntrophies) {
        const StorySideRow *r = &doomo_side_rows[trophy_ids[sel]];
        char t[64];
        int dy = y + vis * 11 + 4;
        if (txt_get(r->title, t, (int)sizeof(t)) < 0) t[0] = 0;
        doomo_snprintf(buf, sizeof(buf), "S%d %s", 100 + r->sid, t);
        plat_rect(x, dy - 2, w - 4, 1, st->outline);
        ui_text_wrap(x, dy + 2, w - 4, buf, st->text_dim, 1);
    }
    (void)h;
}

static void ql_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), style = style_now();
    int x = 6, y = 22, w = sw - 12, h = sh - 30;
    if (plat_draw_skipped()) return;
    ui_backdrop(style, qt);
    if (!ui_window_pop(x, y, w, h, style, (int)qt + 12)) return;
    ui_tabs(x + 4, y - 14, sw >= 300 ? tab_names : tab_names_narrow, QT_COUNT, tab, style);
    switch (tab) {
    case QT_STORY: draw_story(x + 10, y + 8, w - 20, h - 24, style); break;
    case QT_SIDE: draw_side(x + 10, y + 8, w - 20, h - 24, style); break;
    case QT_ITEMS: draw_items(x + 10, y + 8, w - 20, h - 24, style); break;
    case QT_MAP: draw_map(x + 10, y + 6, w - 20, h - 22, style); break;
    case QT_FRIENDS: draw_friends(x + 10, y + 8, w - 20, h - 24, style); break;
    case QT_TROPHIES: draw_trophies(x + 10, y + 8, w - 20, h - 24, style); break;
    default: break;
    }
    ui_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R " Tabs  " STR_ICON_B " Back", style);
}

const DoomoScene SCENE_QUESTLOG = {
    "QUESTLOG", ql_enter, NULL, ql_update, ql_draw, 0
};
