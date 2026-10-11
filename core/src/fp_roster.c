/* fp_roster.c - SCENE_ROSTER: browse all 1000 citizens, and SCENE_PICKER:
 * the same browser picking a citizen for someone else (owner: freeplay).
 *
 *   L / R        district tab (All / Hillvile / Hellvile)
 *   UP / DOWN    citizen (hold to scroll), LEFT / RIGHT a page
 *   A            open the resident screen (picker: choose)
 *   START        filters & sorting (status, mood, tags, sort order)
 *   SELECT       search by name (on-screen keyboard)
 *   B            back
 *
 * Both scenes share one id buffer (rebuilt whenever a scene comes back on
 * top: the island keeps living underneath, so statuses change).
 */
#include "fp_int.h"

enum { SORT_ROOM, SORT_NAME, SORT_FRIEND, SORT_CORRUPT, SORT_UNHAPPY, SORT_COUNT };
enum { TAG_ANY, TAG_PROBLEM, TAG_CUSTOM, TAG_FAV, TAG_PREMADE, TAG_COUPLE, TAG_SINGLE,
       TAG_CROSS, TAG_COUNT };
enum { FROW_DISTRICT, FROW_STATUS, FROW_MOOD, FROW_TAG, FROW_SORT, FROW_SEARCH,
       FROW_CLEAR, FROW_COUNT };

static const char *const sort_names[SORT_COUNT] = {
    "Home", "Name", "Friendship", "Corruption", "Unhappiest"
};
static const char *const tag_names[TAG_COUNT] = {
    "Anyone", "Has a problem", "Made by you", "Favourites", "Premade",
    "Couples", "Singles", "Cross-border"
};
static const char *const district_names[3] = { "All", "Hillvile", "Hellvile" };
static const char *const status_filter_names[6] = {
    "Anyone", "Alive", "Possessed", "Soul lost", "Dead", "Ghost"
};

typedef struct {
    int16_t sel_id;           /* selected resident (survives rebuilds)   */
    int16_t sel, top;
    uint8_t district, status, mood, tag, sort;
    char    search[DOOMO_NAME_LEN];
    uint8_t picking, pick_kind;
    int16_t exclude;
    char    title[40];
    void  (*done)(int id);
    uint8_t filter_open;
    int8_t  frow;
    int16_t stable;           /* ticks the selection has not moved        */
    uint32_t last_frame;
    int16_t pop_t;
} RosterState;

static RosterState st_browse, st_pick;
static RosterState *rs;
/* the id list lives in the shared scene scratch (scene.h) */
#define ROSTER_TAG SAVE_EXT_TAG('F', 'P', 'R', 'O')
#define PICKER_TAG SAVE_EXT_TAG('F', 'P', 'P', 'K')
static uint16_t *ids;
static int n_ids;
static const RosterState *built_for;
static int pick_act = -1;

/* ====================================================================== */
/* matching                                                                 */
static int has_partner(int id)
{
    int p = sim_partner_of(id);
    return p >= 0;
}

static int has_cross_friend(int id)
{
    uint16_t out[12];
    int n = rel_list(id, out, 12), i;
    for (i = 0; i < n; i++) {
        int k = rel_kind(id, out[i]);
        if (out[i] < DOOMO_MAX_RESIDENTS && RES_DISTRICT(out[i]) != RES_DISTRICT(id) &&
            (k == REL_FRIEND || k == REL_BEST_FRIEND || k == REL_DATING || k == REL_MARRIED))
            return 1;
    }
    return 0;
}

int fp_pick_match(int kind, int id)
{
    const DoomoResident *r = fp_res(id);
    if (!r || r->status == RES_EMPTY) return 0;
    switch (kind) {
    case PICK_ANY: return 1;
    case PICK_LIVING: return fp_living_status(r->status);
    case PICK_AWAKE: return fp_living_status(r->status) && r->activity != ACT_SLEEPING;
    case PICK_POSSESSED: return r->status == RES_POSSESSED || r->status == RES_SOUL_LOST;
    case PICK_SICK:
        return r->status == RES_ALIVE &&
               (r->stat[STAT_HEALTH] < 60 || r->mood == MOOD_SICK);
    case PICK_DEAD: return r->status == RES_DEAD;
    case PICK_HERE: return fp_living_status(r->status) && r->activity == pick_act;
    case PICK_SLEEPING: return fp_living_status(r->status) && r->activity == ACT_SLEEPING;
    default: return 0;
    }
}

static int match(const RosterState *s, int id)
{
    const DoomoResident *r = &doomo_town.res[id];
    if (r->status == RES_EMPTY) return 0;
    if (s->picking) {
        if (!fp_pick_match(s->pick_kind, id) || id == s->exclude) return 0;
    }
    if (s->district == 1 && RES_DISTRICT(id)) return 0;
    if (s->district == 2 && !RES_DISTRICT(id)) return 0;
    if (s->status && r->status != s->status) return 0;
    if (s->mood && r->mood != s->mood - 1) return 0;
    switch (s->tag) {
    case TAG_PROBLEM: if (sim_problem_for(id) < 0) return 0; break;
    case TAG_CUSTOM: if (!(r->flags & RF_CUSTOM)) return 0; break;
    case TAG_FAV: if (!(r->flags & RF_FAVORITE)) return 0; break;
    case TAG_PREMADE: if (r->flags & RF_CUSTOM) return 0; break;
    case TAG_COUPLE: if (!has_partner(id)) return 0; break;
    case TAG_SINGLE: if (has_partner(id) || !fp_living_status(r->status)) return 0; break;
    case TAG_CROSS: if (!has_cross_friend(id)) return 0; break;
    default: break;
    }
    if (s->search[0] && !fp_strcasestr(r->name, s->search)) return 0;
    return 1;
}

static int sort_mode;
static int cmp_res(int a, int b)
{
    const DoomoResident *ra = &doomo_town.res[a], *rb = &doomo_town.res[b];
    int d = 0;
    switch (sort_mode) {
    case SORT_NAME: {
        int i;
        for (i = 0; i < DOOMO_NAME_LEN; i++) {
            int ca = fp_lower((unsigned char)ra->name[i]), cb = fp_lower((unsigned char)rb->name[i]);
            if (ca != cb) { d = ca - cb; break; }
            if (!ca) break;
        }
        break;
    }
    case SORT_FRIEND: d = (int)rb->level - (int)ra->level; break;
    case SORT_CORRUPT: d = (int)rb->stat[STAT_CORRUPTION] - (int)ra->stat[STAT_CORRUPTION]; break;
    case SORT_UNHAPPY: d = (int)ra->stat[STAT_HAPPY] - (int)rb->stat[STAT_HAPPY]; break;
    default: break;
    }
    return d ? d : a - b;
}

static void rebuild(RosterState *s)
{
    int i, keep = s->sel_id;
    n_ids = 0;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (match(s, i)) ids[n_ids++] = (uint16_t)i;
    sort_mode = s->sort;
    if (s->sort != SORT_ROOM) fp_sort_u16(ids, n_ids, cmp_res);
    built_for = s;
    s->sel = 0;
    for (i = 0; i < n_ids; i++)
        if (ids[i] == keep) { s->sel = (int16_t)i; break; }
    if (n_ids) s->sel_id = (int16_t)ids[s->sel];
    else s->sel_id = -1;
}

/* ====================================================================== */
/* layout                                                                   */
typedef struct { int lx, ly, lw, rows, cx, cy, cw, ch; } Layout;

static void layout(Layout *L)
{
    int sw = fp_sw(), sh = fp_sh();
    L->ly = 42;
    L->rows = (sh - L->ly - 22) / UI_LIST_ROW_H;
    L->lx = 6;
    L->lw = fp_big() ? 168 : 124;
    L->cx = L->lx + L->lw + 10;
    L->cy = 30;
    L->cw = sw - L->cx - 5;
    L->ch = sh - L->cy - 17;
}

static void clamp_view(const Layout *L)
{
    if (n_ids <= 0) { rs->sel = 0; rs->top = 0; return; }
    rs->sel = (int16_t)DOOMO_CLAMP(rs->sel, 0, n_ids - 1);
    if (rs->sel < rs->top) rs->top = rs->sel;
    if (rs->sel >= rs->top + L->rows) rs->top = (int16_t)(rs->sel - L->rows + 1);
    rs->top = (int16_t)DOOMO_CLAMP(rs->top, 0, DOOMO_MAX(0, n_ids - L->rows));
}

/* ====================================================================== */
/* scene glue                                                               */
static void use_state(void)
{
    uint32_t tag;
    rs = scene_top() == &SCENE_PICKER ? &st_pick : &st_browse;
    tag = rs == &st_pick ? PICKER_TAG : ROSTER_TAG;
    if (!ids || !scene_scratch_mine(tag)) {
        ids = (uint16_t *)scene_scratch(tag, DOOMO_MAX_RESIDENTS * sizeof(uint16_t));
        n_ids = 0;
        built_for = NULL;              /* rebuilt below / on the next update */
    }
}

static void enter_common(RosterState *s)
{
    use_state();
    rs = s;
    s->filter_open = 0;
    s->stable = 0;
    s->pop_t = 0;
    s->last_frame = doomo_frame_count();
    rebuild(s);
    {
        Layout L;
        layout(&L);
        s->top = (int16_t)DOOMO_MAX(0, s->sel - L.rows / 2);
        clamp_view(&L);
    }
}

static void roster_enter(int arg)
{
    (void)arg;
    st_browse.picking = 0;
    enter_common(&st_browse);
}

static void picker_enter(int arg)
{
    (void)arg;
    st_pick.picking = 1;
    st_pick.sel_id = -1;
    st_pick.district = 0;
    st_pick.status = 0;
    st_pick.mood = 0;
    st_pick.tag = 0;
    st_pick.search[0] = 0;
    enter_common(&st_pick);
}

void fp_pick(const char *title, int kind, int exclude, void (*done)(int id))
{
    fp_strcpy(st_pick.title, title ? title : "Choose a citizen", sizeof(st_pick.title));
    st_pick.pick_kind = (uint8_t)kind;
    st_pick.exclude = (int16_t)exclude;
    st_pick.done = done;
    st_pick.sort = SORT_ROOM;
    scene_push(&SCENE_PICKER, 0);
}

void fp_pick_act(const char *title, int act, void (*done)(int id))
{
    pick_act = act;
    fp_pick(title, PICK_HERE, -1, done);
}

static void finish_pick(int id)
{
    void (*done)(int) = st_pick.done;
    st_pick.done = NULL;
    scene_pop();
    if (done) done(id);
}

static void leave_browse(void)
{
    if (scene_depth() > 1) scene_pop();
    else fp_goto_island(FP_ISLAND_ARG_RETURN);
}

static void cb_search(int ok)
{
    (void)ok;
    use_state();
    rebuild(rs);
    rs->top = 0;
    if (ok && rs->search[0] && n_ids > 0) fp_achieve(UL_FP_SEARCHER);
}

static void change_filter(int row, int d)
{
    switch (row) {
    case FROW_DISTRICT: rs->district = (uint8_t)((rs->district + 3 + d) % 3); break;
    case FROW_STATUS: rs->status = (uint8_t)((rs->status + 6 + d) % 6); break;
    case FROW_MOOD: rs->mood = (uint8_t)((rs->mood + MOOD_COUNT + 1 + d) % (MOOD_COUNT + 1)); break;
    case FROW_TAG: rs->tag = (uint8_t)((rs->tag + TAG_COUNT + d) % TAG_COUNT); break;
    case FROW_SORT: rs->sort = (uint8_t)((rs->sort + SORT_COUNT + d) % SORT_COUNT); break;
    default: return;
    }
    audio_sfx(SFX_CURSOR);
    rebuild(rs);
}

static void filter_update(void)
{
    if (btn_repeat(BTN_UP)) { rs->frow = (int8_t)((rs->frow + FROW_COUNT - 1) % FROW_COUNT); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { rs->frow = (int8_t)((rs->frow + 1) % FROW_COUNT); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_LEFT)) change_filter(rs->frow, -1);
    if (btn_repeat(BTN_RIGHT)) change_filter(rs->frow, 1);
    if (btn_pressed(BTN_A)) {
        if (rs->frow == FROW_SEARCH) {
            audio_sfx(SFX_SELECT);
            ui_text_entry("Search by name", rs->search, DOOMO_NAME_LEN, cb_search);
            return;
        }
        if (rs->frow == FROW_CLEAR) {
            rs->district = rs->status = rs->mood = rs->tag = 0;
            rs->sort = SORT_ROOM;
            rs->search[0] = 0;
            audio_sfx(SFX_SPARKLE);
            rebuild(rs);
            return;
        }
        change_filter(rs->frow, 1);
    }
    if (btn_pressed(BTN_B | BTN_START)) {
        audio_sfx(SFX_CANCEL);
        rs->filter_open = 0;
    }
}

static void roster_update(void)
{
    Layout L;
    int old;
    use_state();
    fp_live();
    layout(&L);
    if (doomo_frame_count() - rs->last_frame > 1 || built_for != rs) {
        rebuild(rs);                     /* back on top: things changed */
        clamp_view(&L);
    }
    rs->last_frame = doomo_frame_count();
    rs->pop_t++;
    if (rs->filter_open) { filter_update(); return; }
    old = rs->sel;
    if (btn_repeat(BTN_UP) && n_ids) rs->sel = (int16_t)(rs->sel > 0 ? rs->sel - 1 : n_ids - 1);
    if (btn_repeat(BTN_DOWN) && n_ids) rs->sel = (int16_t)(rs->sel < n_ids - 1 ? rs->sel + 1 : 0);
    if (btn_repeat(BTN_LEFT)) rs->sel = (int16_t)DOOMO_MAX(0, rs->sel - L.rows);
    if (btn_repeat(BTN_RIGHT)) rs->sel = (int16_t)DOOMO_MIN(DOOMO_MAX(n_ids - 1, 0), rs->sel + L.rows);
    clamp_view(&L);
    if (rs->sel != old) {
        audio_sfx(SFX_CURSOR);
        rs->stable = 0;
        rs->sel_id = n_ids ? (int16_t)ids[rs->sel] : -1;
    } else if (rs->stable < 1000) {
        rs->stable++;
    }
    if (btn_pressed(BTN_L | BTN_R)) {
        rs->district = (uint8_t)((rs->district + (btn_pressed(BTN_L) ? 2 : 1)) % 3);
        audio_sfx(SFX_BLIP);
        rebuild(rs);
        rs->top = (int16_t)DOOMO_MAX(0, rs->sel - L.rows / 2);
        clamp_view(&L);
        rs->stable = 0;
        return;
    }
    if (btn_pressed(BTN_START)) {
        audio_sfx(SFX_SELECT);
        rs->filter_open = 1;
        rs->frow = 0;
        return;
    }
    if (btn_pressed(BTN_SELECT)) {
        audio_sfx(SFX_SELECT);
        ui_text_entry("Search by name", rs->search, DOOMO_NAME_LEN, cb_search);
        return;
    }
    if (btn_pressed(BTN_A) && n_ids) {
        int id = ids[rs->sel];
        audio_sfx(SFX_SELECT);
        if (rs->picking) finish_pick(id);
        else fp_open_resident(id);
        return;
    }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (rs->picking) finish_pick(-1);
        else leave_browse();
    }
}

/* ====================================================================== */
/* drawing                                                                  */
static void draw_row(int x, int y, int w, int id, int selected, int style)
{
    const UiStyle *st = ui_style(style);
    const DoomoResident *r = &doomo_town.res[id];
    int t = fp_t(), rx = x + w - 8;
    uint8_t tc = st->text;
    char name[DOOMO_NAME_LEN + 1];
    fp_strcpy(name, r->name, sizeof(name));
    if (selected) {
        fp_pill(x + 9, y, w - 9, UI_LIST_ROW_H - 1, st->accent_d, ui_darker(st->accent_d),
                ui_lighter(st->accent_d));
        ui_cursor(x, y + 2);
    }
    if (r->status == RES_DEAD) tc = st->text_dim;
    fp_icon(x + 12, y + 2, fp_status_icon(r->status), fp_status_color(r->status));
    ui_text(x + 21, y + 2, name, tc);
    /* right side markers */
    if (sim_problem_for(id) >= 0) {
        int b = ui_wave(t + id * 5, 24, 1) > 0;
        ui_fill_circle(rx + 2, y + 5 - b, 4, PAL_UI_RED_D);
        ui_fill_circle(rx + 2, y + 5 - b, 3, PAL_UI_RED);
        plat_text(rx, y + 1 - b, "!", PAL_WHITE);
        rx -= 9;
    }
    if (r->flags & RF_FAVORITE) { ui_star(rx + 2, y + 5, 3, PAL_UI_GOLD); rx -= 8; }
    if (fp_living_status(r->status)) {
        fp_icon(rx - 1, y + 2, fp_mood_icon(r->mood), fp_mood_color(r->mood));
        rx -= 8;
    }
    /* district dot: pink heart for Hillvile, purple horns for Hellvile */
    if (RES_DISTRICT(id)) {
        plat_rect(rx, y + 3, 1, 2, PP(PAINT_GRAPE, 0));
        plat_rect(rx + 4, y + 3, 1, 2, PP(PAINT_GRAPE, 0));
        ui_fill_circle(rx + 2, y + 6, 2, PP(PAINT_VAPE_PURPLE, 1));
    } else {
        ui_heart(rx + 2, y + 5, 2, PP(PAINT_BUBBLEGUM, 1));
    }
}

static void draw_card(const Layout *L, int style)
{
    const UiStyle *st = ui_style(style);
    int id = n_ids ? ids[rs->sel] : -1, x = L->cx, y = L->cy, w = L->cw, h = L->ch;
    const DoomoResident *r = fp_res(id);
    int t = fp_t(), px, ty, pi;
    char buf[64];
    ui_window(x, y, w, h, style);
    if (!r) {
        ui_text_center(x + w / 2, y + h / 2 - 8, "Nobody here!", st->text_dim);
        ui_heart(x + w / 2, y + h / 2 + 10 + ui_wave(t, 40, 2), 5, st->accent);
        return;
    }
    px = x + (w - (PORTRAIT_W + 6)) / 2;
    if (rs->stable >= 6) fp_portrait(px, y + 6, id, 1, -1, WIN_MINT);
    else {
        fp_portrait(px, y + 6, -1, 1, 0, WIN_MINT);
        fp_chibi(id, px + 3 + (PORTRAIT_W - BODYSPR_W) / 2, y + 9 + (PORTRAIT_H - BODYSPR_H) / 2,
                 0, 0, 0);
    }
    ty = y + PORTRAIT_H + 13;
    ui_text_center(x + w / 2, ty, r->name, st->text);
    ty += FONT_LINE;
    doomo_snprintf(buf, sizeof(buf), "%s", fp_job(id));
    fp_marquee(x + 6, ty, w - 12, buf, st->text_dim, rs->stable);
    ty += FONT_LINE;
    doomo_snprintf(buf, sizeof(buf), "%s %d-%d", RES_DISTRICT(id) ? "Pits" : "Towers",
                   fp_floor(id), fp_room(id));
    fp_icon(x + 6, ty, ICON_HOUSE, RES_DISTRICT(id) ? PP(PAINT_VAPE_PURPLE, 1) : PP(PAINT_BUBBLEGUM, 0));
    ui_text(x + 15, ty, buf, st->text);
    ty += FONT_LINE;
    if (fp_living_status(r->status)) {
        fp_icon(x + 6, ty, fp_mood_icon(r->mood), fp_mood_color(r->mood));
        ui_text(x + 15, ty, doomo_mood_names[r->mood % MOOD_COUNT], st->text);
        ty += FONT_LINE;
        if (ty + FONT_LINE <= y + h - 4) {
            fp_icon(x + 6, ty, fp_activity_icon(r->activity), st->accent);
            fp_marquee(x + 15, ty, w - 20, fp_activity_name(r->activity), st->text_dim, rs->stable);
            ty += FONT_LINE;
        }
    } else {
        fp_icon(x + 6, ty, fp_status_icon(r->status), fp_status_color(r->status));
        ui_text(x + 15, ty, fp_status_name(r->status), st->text);
        ty += FONT_LINE;
    }
    pi = sim_problem_for(id);
    if (pi >= 0 && ty + FONT_LINE <= y + h - 3) {
        char pb[64];
        sim_problem_text(pi, pb, sizeof(pb));
        fp_icon(x + 6, ty, ICON_CLOUD, PAL_UI_RED);
        fp_marquee(x + 15, ty, w - 20, pb, PAL_UI_RED, rs->stable);
        ty += FONT_LINE;
    }
    if (fp_big() && ty + 2 * FONT_LINE <= y + h - 3) {
        ui_text_wrap(x + 6, ty + 2, w - 12, fp_bio(id), st->text_dim, 1);
    }
}

static void draw_filters(int style)
{
    const UiStyle *st;
    int sw = fp_sw(), sh = fp_sh(), w = DOOMO_MIN(sw - 24, 220), h = FROW_COUNT * 13 + 22;
    int x = (sw - w) / 2, y = (sh - h) / 2, i;
    char val[32];
    plat_shade_rect(0, 0, sw, sh, 10);
    ui_window(x, y, w, h, style);
    st = ui_style(style);
    ui_text_center(x + w / 2, y + 5, STR_ICON_STAR " Filters & sorting " STR_ICON_STAR, st->accent);
    for (i = 0; i < FROW_COUNT; i++) {
        const char *label = "", *v = "";
        switch (i) {
        case FROW_DISTRICT: label = "District"; v = district_names[rs->district % 3]; break;
        case FROW_STATUS: label = "Status"; v = status_filter_names[rs->status % 6]; break;
        case FROW_MOOD: label = "Mood"; v = rs->mood ? doomo_mood_names[(rs->mood - 1) % MOOD_COUNT] : "Any"; break;
        case FROW_TAG: label = "Show"; v = tag_names[rs->tag % TAG_COUNT]; break;
        case FROW_SORT: label = "Sort by"; v = sort_names[rs->sort % SORT_COUNT]; break;
        case FROW_SEARCH:
            label = "Name has";
            doomo_snprintf(val, sizeof(val), "%s", rs->search[0] ? rs->search : "(anything)");
            v = val;
            break;
        default: label = "Clear all filters"; v = ""; break;
        }
        ui_value_row(x + 16, y + 18 + i * 13, w - 26, label, v, i == rs->frow);
    }
}

static void roster_draw(void)
{
    Layout L;
    const UiStyle *st;
    int style = WIN_CUTE, i, sw = fp_sw(), t = fp_t();
    char buf[48];
    use_state();
    layout(&L);
    if (built_for != rs) { rebuild(rs); clamp_view(&L); }
    ui_backdrop(rs->district == 2 ? WIN_DARK : WIN_CUTE, (uint32_t)t);
    if (rs->district == 2) style = WIN_DARK;
    st = ui_style(style);
    /* header */
    if (rs->picking) doomo_snprintf(buf, sizeof(buf), "%s", rs->title);
    else doomo_snprintf(buf, sizeof(buf), "Citizens  %d", n_ids);
    ui_title(sw / 2, 3, buf, style);
    ui_tabs(L.lx + 4, 22, district_names, 3, rs->district, style);
    if (rs->search[0] || rs->status || rs->mood || rs->tag || rs->sort) {
        /* a gold tag on the card says filters are on */
        if (rs->search[0]) doomo_snprintf(buf, sizeof(buf), STR_ICON_STAR "\"%s\"", rs->search);
        else doomo_snprintf(buf, sizeof(buf), STR_ICON_STAR "%s", sort_names[rs->sort % SORT_COUNT]);
        fp_tag(L.cx + 2, L.cy - 6, buf, PAL_UI_GOLD, PAL_RAMP(RAMP_WOOD, 0));
    }
    /* list */
    ui_window(L.lx - 2, L.ly - 6, L.lw + 4, L.rows * UI_LIST_ROW_H + 10, style);
    for (i = 0; i < L.rows && rs->top + i < n_ids; i++)
        draw_row(L.lx, L.ly - 1 + i * UI_LIST_ROW_H, L.lw - 6, ids[rs->top + i],
                 rs->top + i == rs->sel, style);
    if (!n_ids) {
        ui_text_center(L.lx + L.lw / 2, L.ly + 20, rs->picking ? "Nobody fits..." : "Nobody here yet!",
                       st->text_dim);
        ui_text_center(L.lx + L.lw / 2, L.ly + 32, rs->picking ? "" : "Make citizens at Town Hall", st->text_dim);
    }
    if (n_ids > L.rows) {
        int th = L.rows * UI_LIST_ROW_H - 4, bx = L.lx + L.lw - 4;
        int thumb = DOOMO_MAX(5, th * L.rows / n_ids);
        int ty = L.ly + (th - thumb) * rs->top / DOOMO_MAX(1, n_ids - L.rows);
        ui_round_rect(bx, L.ly, 3, th, 1, st->fill_lo);
        ui_round_rect(bx, ty, 3, thumb, 1, st->accent);
        ui_scroll_arrows(bx - 1, L.ly - 5, L.ly + th + 1, rs->top > 0, rs->top + L.rows < n_ids);
    }
    draw_card(&L, style);
    if (fp_big())
        ui_hint(rs->picking ? STR_ICON_A " Choose  L/R Side  START Filter  " STR_ICON_B " Back"
                            : STR_ICON_A " Open  L/R Side  START Filter  SEL Find", style);
    else
        ui_hint(rs->picking ? STR_ICON_A " Choose  L/R Side  " STR_ICON_B " Back"
                            : STR_ICON_A " Open  START Filter  SEL Find", style);
    if (rs->filter_open) draw_filters(style);
    fp_fx_draw();
}

const DoomoScene SCENE_ROSTER = { "ROSTER", roster_enter, NULL, roster_update, roster_draw, 0 };
const DoomoScene SCENE_PICKER = { "PICKER", picker_enter, NULL, roster_update, roster_draw, 0 };
