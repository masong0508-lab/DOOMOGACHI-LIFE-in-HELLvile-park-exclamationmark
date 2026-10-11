/* lib_almanac.c - SCENE_LIB_ALMANAC: the Citizen Almanac (owner: library).
 *
 * Every citizen of the island, both sides of the Wall: L/R picks the
 * shelf (all, Hillvile, Hellvile, story cast, friends, possessed, ghosts,
 * departed), SELECT the sort order (home number, name, friendship,
 * species), START opens the letter search. The detail page shows the
 * citizen's full-body figure and three pages: Profile (public record),
 * Likes and Bonds - the last two stay "???" until the player knows them
 * (friendship level, a relationship with the player, or a favourite).
 *
 * Cost: building a view walks the 1000 slots once and shell-sorts the
 * matches (on entry and when the shelf / sort / letter changes, never per
 * tick); portraits are only requested once the cursor rests (re-rendering
 * a portrait is the expensive part on the GBA).
 */
#include "lib_int.h"

#define A (lib_w.alm)

static uint8_t alm_pct;            /* completion, computed on entry */

enum { SH_ALL, SH_HILL, SH_HELL, SH_CAST, SH_FRIENDS, SH_POSSESSED, SH_GHOSTS,
       SH_DEPARTED, SH_COUNT };
static const char *const shelf_names[SH_COUNT] = {
    "Everyone", "Hillvile", "Hellvile", "Story Cast", "Friends", "Possessed",
    "Ghosts", "Departed"
};
enum { SO_HOME, SO_NAME, SO_FRIEND, SO_SPECIES, SO_COUNT };
static const char *const sort_names[SO_COUNT] = { "Home", "Name", "Friendship", "Species" };

/* Story cast roles (NPC_*). */
static const char *const cast_roles[NPC_COUNT] = {
    "Mayor of Hillvile", "Plushie priest of St. Cuddles", "Runaway imp welder",
    "Ghoul doctor, Hospital", "Baker, keeper of the egg", "Lost a bunny plush",
    "Owner of Cloud Nine Vapor", "Duke of the Ninth Cloud"
};

/* Job titles for citizens who are not premade records (by hobby). */
static const char *const hobby_jobs[HOBBY_COUNT] = {
    "Home Baker", "Knitter", "Gardener", "Stargazer", "Skater", "Gamer", "Painter",
    "Singer", "Dancer", "Bookworm", "Angler", "Professional Napper", "Cosplayer",
    "Karaoke Star", "Board Gamer", "Bug Collector", "Birdwatcher", "Yoga Teacher",
    "Potter", "Origami Artist", "Plush Maker", "Cloud Watcher", "Bubble Artist",
    "Poet", "Tarot Reader", "Medium", "Ghost Hunter", "Drummer", "Prankster",
    "Graffiti Artist", "Welder", "Skull Collector"
};

/* ---- facts about a citizen ------------------------------------------------ */
static const DoomoResident *res(int id) { return town_res(id); }

static int is_premade(int id)
{
    const DoomoResident *r = res(id);
    return r && doomo_town.population == TOWN_POP_PREMADE && !(r->flags & RF_PLAYER) &&
           strcmp(r->name, roster_name(id)) == 0;
}

static int cast_of(int id)
{
    return is_premade(id) ? roster_npc_of(id) : -1;
}

int lib_known(int id)
{
    const DoomoResident *r = res(id);
    if (!r || r->status == RES_EMPTY) return 0;
    if (r->level > 0 || (r->flags & (RF_FAVORITE | RF_PLAYER))) return 1;
    return rel_find(id, DOOMO_PLAYER_ID) != NULL;
}

int lib_almanac_known(void)
{
    int i, n = 0;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        const DoomoResident *r = &doomo_town.res[i];
        if (r->status != RES_EMPTY && (r->level > 0 || (r->flags & (RF_FAVORITE | RF_PLAYER)))) n++;
    }
    /* plus everyone with a relationship to the player that the loop above
     * did not count (edges are unique per pair) */
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        int o = e->b == DOOMO_PLAYER_ID ? e->a : e->a == DOOMO_PLAYER_ID ? e->b : -1;
        const DoomoResident *r;
        if (o < 0 || o >= DOOMO_MAX_RESIDENTS) continue;
        r = &doomo_town.res[o];
        if (r->status != RES_EMPTY && !(r->level > 0 || (r->flags & (RF_FAVORITE | RF_PLAYER)))) n++;
    }
    return n;
}

int lib_almanac_pct(void)
{
    int pop = town_count(-1), known = lib_almanac_known();
    if (known >= 100) unlock_grant(LIB_UL_HISTORIAN);
    if (known >= 500) unlock_grant(LIB_UL_KNOW_IT_ALL);
    lib_have = (int16_t)known;
    lib_total = (int16_t)pop;
    return lib_pct(known, pop);
}

/* What the pages show as known: the real rule, or in LIB_ARG_DEMO (the
 * runner's look-around mode) two citizens out of three. */
static int shown_known(int id)
{
    return lib_known(id) || (lib_b.demo && id % 3 != 0);
}

static const char *job_of(int id)
{
    const DoomoResident *r = res(id);
    if (is_premade(id)) return roster_job(id);
    if (!r) return "";
    if (r->flags & RF_PLAYER) return "You!";
    return hobby_jobs[r->like_hobby % HOBBY_COUNT];
}

static const char *bio_of(int id, char *buf, int cap)
{
    const DoomoResident *r = res(id);
    if (is_premade(id)) return roster_bio(id);
    if (!r) return "";
    doomo_snprintf(buf, cap, "Loves %s and %s. Scared of %s. Says \"%s\" a lot.",
                   doomo_food_names[r->like_food % FOOD_COUNT],
                   doomo_hobby_names[r->like_hobby % HOBBY_COUNT],
                   doomo_fear_names[r->fear % FEAR_COUNT],
                   doomo_catchphrases[r->catchphrase % CATCHPHRASE_COUNT]);
    return buf;
}

static const char *status_name(const DoomoResident *r, uint8_t *c)
{
    switch (r->status) {
    case RES_POSSESSED: *c = PAL_UI_PURPLE; return "Possessed";
    case RES_SOUL_LOST: *c = PAL_RAMP(RAMP_VAPE, 2); return "Soul lost";
    case RES_DEAD:      *c = PAL_GRAY_D; return "Departed";
    case RES_GHOST:     *c = PAL_PAINT(PAINT_TEAL, 1); return "Ghost";
    default:            *c = PAL_PAINT(PAINT_FOREST, 1); return "Alive";
    }
}

/* ---- the view ------------------------------------------------------------------ */
static int upper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

static int in_shelf(int id, int shelf)
{
    const DoomoResident *r = res(id);
    if (!r || r->status == RES_EMPTY) return 0;
    switch (shelf) {
    case SH_HILL: return !RES_DISTRICT(id);
    case SH_HELL: return RES_DISTRICT(id);
    case SH_CAST: return cast_of(id) >= 0;
    case SH_FRIENDS: return lib_known(id);
    case SH_POSSESSED: return r->status == RES_POSSESSED;
    case SH_GHOSTS: return r->status == RES_GHOST;
    case SH_DEPARTED: return r->status == RES_DEAD || r->status == RES_SOUL_LOST;
    default: return 1;
    }
}

static int name_cmp(const char *a, const char *b)
{
    while (*a && upper((unsigned char)*a) == upper((unsigned char)*b)) { a++; b++; }
    return upper((unsigned char)*a) - upper((unsigned char)*b);
}

static int cmp(int a, int b)
{
    const DoomoResident *ra = res(a), *rb = res(b);
    int d = 0;
    switch (lib_b.sort % SO_COUNT) {
    case SO_NAME: d = name_cmp(ra->name, rb->name); break;
    case SO_FRIEND: d = (int)rb->level - (int)ra->level; if (!d) d = name_cmp(ra->name, rb->name); break;
    case SO_SPECIES: d = (int)ra->look.species - (int)rb->look.species; if (!d) d = name_cmp(ra->name, rb->name); break;
    default: break;
    }
    return d ? d : a - b;
}

static void sort_view(void)
{
    static const int16_t gaps[] = { 701, 301, 132, 57, 23, 10, 4, 1 };
    int g, i, j, n = lib_b.count;
    if (lib_b.sort % SO_COUNT == SO_HOME) return;
    for (g = 0; g < (int)DOOMO_COUNTOF(gaps); g++) {
        int gap = gaps[g];
        for (i = gap; i < n; i++) {
            uint16_t v = A.idx[i];
            for (j = i; j >= gap && cmp(A.idx[j - gap], v) > 0; j -= gap) A.idx[j] = A.idx[j - gap];
            A.idx[j] = v;
        }
    }
}

static void rebuild(void)
{
    int id, n = 0, keep = lib_b.count > 0 ? A.idx[lib_b.sel] : -1;
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        if (!in_shelf(id, lib_b.tab)) continue;
        if (A.letter && upper((unsigned char)res(id)->name[0]) != A.letter) continue;
        A.idx[n++] = (uint16_t)id;
    }
    lib_b.count = (int16_t)n;
    sort_view();
    /* keep the cursor on the same citizen if they are still listed */
    lib_b.sel = 0;
    if (keep >= 0)
        for (id = 0; id < n; id++) if (A.idx[id] == keep) { lib_b.sel = (int16_t)id; break; }
    lib_clamp();
    A.rel_for = -1;
}

static void count_letters(void)
{
    int id;
    memset(A.letter_n, 0, sizeof(A.letter_n));
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        int c;
        if (!in_shelf(id, lib_b.tab)) continue;
        c = upper((unsigned char)res(id)->name[0]);
        if (c >= 'A' && c <= 'Z') A.letter_n[c - 'A']++;
        A.letter_n[26]++;
    }
}

/* Relationship summary for the Bonds page. */
static void summarize(int id)
{
    int i;
    if (A.rel_for == id) return;
    A.rel_for = (int16_t)id;
    A.rel_n = (uint8_t)rel_list(id, A.rel_id, 6);
    A.rel_friends = A.rel_family = A.rel_foes = 0;
    for (i = 0; i < A.rel_n; i++) {
        A.rel_val[i] = (int8_t)rel_value(id, A.rel_id[i]);
        A.rel_kind[i] = (uint8_t)rel_kind(id, A.rel_id[i]);
    }
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        int k;
        if (e->a != id && e->b != id) continue;
        k = e->kind & REL_KIND_MASK;
        if (k == REL_FAMILY || k == REL_MARRIED) A.rel_family++;
        else if (k == REL_RIVAL || k == REL_NEMESIS) A.rel_foes++;
        else if (k == REL_FRIEND || k == REL_BEST_FRIEND || k == REL_DATING) A.rel_friends++;
    }
}

/* ---- scene -------------------------------------------------------------------- */
static void alm_enter(int arg)
{
    lib_wing_enter(LIB_WING_ALMANAC, arg);
    if (lib_b.tab >= SH_COUNT) lib_b.tab = 0;
    A.rel_for = -1;
    A.expr = EXPR_HAPPY;
    rebuild();
    alm_pct = (uint8_t)lib_almanac_pct();     /* also grants the milestones */
}

static void picker_update(void)
{
    int old = A.pick;
    if (lib_b.tab_t < 60000) lib_b.tab_t++;
    if (btn_repeat(BTN_LEFT)) A.pick = (uint8_t)((A.pick + 26) % 27);
    if (btn_repeat(BTN_RIGHT)) A.pick = (uint8_t)((A.pick + 1) % 27);
    if (btn_repeat(BTN_UP)) A.pick = (uint8_t)((A.pick + 27 - 9) % 27);
    if (btn_repeat(BTN_DOWN)) A.pick = (uint8_t)((A.pick + 9) % 27);
    if (A.pick != old) audio_sfx(SFX_CURSOR);
    if (btn_pressed(BTN_A)) {
        A.letter = (uint8_t)(A.pick < 26 ? 'A' + A.pick : 0);
        A.picking = 0;
        lib_b.tab_t = 0;
        audio_sfx(SFX_SELECT);
        rebuild();
        lib_b.sel = 0;
        lib_b.top = 0;
    } else if (btn_pressed(BTN_B) || btn_pressed(BTN_START)) {
        A.picking = 0;
        audio_sfx(SFX_CANCEL);
    }
}

static void alm_update(void)
{
    if (lib_w_lost(1)) return;
    if (A.picking) { lib_b.t++; picker_update(); return; }
    switch (lib_nav(SH_COUNT, 3)) {
    case LIBE_TAB: rebuild(); break;
    case LIBE_FILTER:              /* SELECT = sort */
        lib_b.sort = (uint8_t)((lib_b.sort + 1) % SO_COUNT);
        rebuild();
        break;
    case LIBE_START:
        count_letters();
        A.picking = 1;
        lib_b.tab_t = 0;
        A.pick = (uint8_t)(A.letter ? A.letter - 'A' : 26);
        audio_sfx(SFX_SELECT);
        break;
    case LIBE_BACK:
        if (A.letter) { A.letter = 0; rebuild(); audio_sfx(SFX_CANCEL); }
        else lib_wing_leave();
        break;
    case LIBE_OPEN: case LIBE_FLIP: {
        const DoomoResident *r = res(A.idx[lib_b.sel]);
        A.expr = EXPR_HAPPY;
        if (r && shown_known(A.idx[lib_b.sel]) && r->status == RES_ALIVE) audio_sfx(SFX_HAPPY);
        break; }
    case LIBE_TOGGLE: {
        static const uint8_t exprs[] = { EXPR_HAPPY, EXPR_TALK, EXPR_LOVE, EXPR_SHOCK,
                                         EXPR_SLEEPY, EXPR_ANGRY, EXPR_SAD };
        int i;
        for (i = 0; i < (int)sizeof(exprs) && exprs[i] != A.expr; i++) {}
        A.expr = exprs[(i + 1) % (int)sizeof(exprs)];
        break; }
    default: break;
    }
}

/* ---- drawing ---------------------------------------------------------------- */
static void row(int i, int x, int y, int w, int sel)
{
    const UiStyle *st = ui_style(WIN_MINT);
    int id = A.idx[i];
    const DoomoResident *r = res(id);
    const char *g;
    uint8_t gc, nc = st->text;
    char buf[8];
    (void)sel;
    if (!r) return;
    switch (r->status) {
    case RES_POSSESSED: g = STR_ICON_PENTA; gc = PAL_UI_PURPLE; break;
    case RES_SOUL_LOST: g = STR_ICON_SKULL; gc = PAL_RAMP(RAMP_VAPE, 3); break;
    case RES_DEAD:      g = STR_ICON_CROSS; gc = PAL_GRAY; nc = st->text_dim; break;
    case RES_GHOST:     g = STR_ICON_GHOST; gc = PAL_PAINT(PAINT_TEAL, 1); break;
    default:
        if (shown_known(id)) { g = STR_ICON_HEART; gc = PAL_PAINT(PAINT_ROSE, 1); }
        else { g = "\x8D"; gc = st->text_dim; }
        break;
    }
    plat_text(x, y + 2, g, gc);
    ui_text(x + 8, y + 2, r->name, nc);
    if (cast_of(id) >= 0) plat_text(x + w - 6, y + 2, STR_ICON_STAR, PAL_UI_GOLD);
    else if (RES_DISTRICT(id)) plat_text(x + w - 6, y + 2, STR_ICON_FIRE, PAL_PAINT(PAINT_BLOOD, 1));
    if (lib_big() && lib_b.sort % SO_COUNT == SO_HOME) {
        doomo_snprintf(buf, sizeof(buf), "%03d", id % DOOMO_DISTRICT_SIZE);
        ui_text(x + w - 30, y + 2, buf, st->text_dim);
    }
}

static int portrait_ready(void) { return lib_b.rest >= 4; }

static void chip(int x, int y, const char *s, uint8_t c)
{
    int w = ui_text_width(s) + 8;
    ui_round_rect(x, y, w, 11, 3, ui_darker(c));
    ui_round_rect(x + 1, y + 1, w - 2, 9, 2, c);
    ui_text(x + 4, y + 2, s, PAL_WHITE);
}

static void preview(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(WIN_MINT);
    int id, big = lib_big(), px, py, tx, ty, tw;
    const DoomoResident *r;
    char buf[48];
    uint8_t sc = 0;
    ui_window(x, y, w, h, WIN_MINT);
    if (lib_b.count <= 0 || plat_draw_skipped()) return;
    id = A.idx[lib_b.sel];
    r = res(id);
    if (!r) return;
    /* the portrait frame (ui_portrait: 54x62) */
    px = big ? x + 6 : x + (w - 54) / 2;
    py = y + 5;
    if (portrait_ready()) {
        int expr = r->status == RES_POSSESSED || r->status == RES_SOUL_LOST ? EXPR_POSSESSED :
                   r->status == RES_DEAD ? EXPR_DEAD : shown_known(id) ? EXPR_HAPPY : EXPR_NEUTRAL;
        ui_portrait(px, py, id, 1, expr);
    } else {
        ui_round_rect(px, py, 54, 62, 4, st->outline);
        ui_round_rect(px + 1, py + 1, 52, 60, 3, st->fill_hi);
        ui_fill_circle(px + 27, py + 26, 14, st->fill_lo);
        ui_fill_ellipse(px + 27, py + 56, 20, 9, st->fill_lo);
        ui_text_center(px + 27, py + 22, "...", st->text_dim);
    }
    if (cast_of(id) >= 0) ui_star(px + 50, py + 4, 5, PAL_UI_GOLD);
    if (RES_DISTRICT(id)) plat_text(px + 3, py + 3, STR_ICON_FIRE, PAL_PAINT(PAINT_BLOOD, 1));
    if (big) { tx = px + 60; ty = y + 6; tw = x + w - tx - 6; }
    else { tx = x + 6; ty = py + 66; tw = w - 12; }
    ui_text_shadow(tx, ty, r->name, st->accent, st->fill_lo);
    ui_text(tx, ty + 10, lib_fit(job_of(id), tw, buf, sizeof(buf)), st->text);
    if (big) {
        ui_text(tx, ty + 20, lib_fit(doomo_species_names[r->look.species % SPECIES_COUNT], tw, buf, sizeof(buf)),
                st->text_dim);
        const char *sn = status_name(r, &sc);
        chip(tx, ty + 32, RES_DISTRICT(id) ? "Hellvile" : "Hillvile",
             RES_DISTRICT(id) ? PAL_PAINT(PAINT_BLOOD, 1) : PAL_PAINT(PAINT_SKY, 1));
        chip(tx, ty + 45, sn, sc);
        {
            int cy = py + 70, c = cast_of(id);
            if (c >= 0) {
                ui_text(x + 6, cy, STR_ICON_STAR " Story cast:", PAL_RAMP(RAMP_CANDY, 2));
                ui_text_wrap(x + 6, cy + 10, w - 12, cast_roles[c], st->text, 1);
                cy += 30;
            }
            {
                char bb[160];
                ui_text_wrap(x + 6, cy, w - 12, bio_of(id, bb, sizeof(bb)), st->text_dim, 1);
            }
        }
    } else {
        const char *sn = status_name(r, &sc);
        chip(tx, ty + 21, RES_DISTRICT(id) ? "Hellvile" : "Hillvile",
             RES_DISTRICT(id) ? PAL_PAINT(PAINT_BLOOD, 1) : PAL_PAINT(PAINT_SKY, 1));
        chip(tx + 58, ty + 21, sn, sc);
    }
}

static void bar5(int x, int y, int v, uint8_t c)
{
    ui_bar(x, y, 40, 5, v, 15, c);
}

static int page_profile(int id, int x, int y, int w, int bottom, const UiStyle *st)
{
    const DoomoResident *r = res(id);
    char v[48], bb[160];
    int ly = y, c = cast_of(id), lines;
    lib_kv(x, ly, w, "Job", job_of(id), st->text_dim, st->text); ly += FONT_LINE;
    lib_kv(x, ly, w, "Species", doomo_species_names[r->look.species % SPECIES_COUNT], st->text_dim, st->text); ly += FONT_LINE;
    lib_kv(x, ly, w, "Pronouns", doomo_pronoun_names[r->pronoun % PRONOUN_COUNT], st->text_dim, st->text); ly += FONT_LINE;
    doomo_snprintf(v, sizeof(v), "%s %d", doomo_month_names[r->bday_month % 12], r->bday_day);
    lib_kv(x, ly, w, "Birthday", v, st->text_dim, st->text); ly += FONT_LINE;
    lib_kv(x, ly, w, "Type", doomo_persona_type_names[persona_type(r) % PERSONA_TYPE_COUNT], st->text_dim, st->text);
    ly += FONT_LINE;
    if (lib_big()) {
        int k;
        static const char *const axis[PERS_COUNT] = { "Pace", "Speech", "Emote", "Chill", "Quirk" };
        for (k = 0; k < PERS_COUNT; k++) {
            int bx = x + (k % 2) * (w / 2), by = ly + 2 + (k / 2) * FONT_LINE;
            ui_text(bx, by, axis[k], st->text_dim);
            bar5(bx + w / 2 - 46, by + 2, r->persona[k], PAL_PAINT(PAINT_TEAL, 1));
        }
        ly += 3 * FONT_LINE + 4;
    }
    if (c >= 0) {
        char rb[200];
        doomo_snprintf(rb, sizeof(rb), STR_ICON_STAR " %s.\n%s", cast_roles[c], bio_of(id, bb, sizeof(bb)));
        lines = (bottom - ly - 2) / FONT_LINE;
        if (lines > 0) lib_text_block(x, ly + 2, w - 6, lines, rb, st->accent, 0);
        return ly;
    }
    lines = (bottom - ly - 2) / FONT_LINE;
    if (lines > 0) lib_text_block(x, ly + 2, w - 6, lines, bio_of(id, bb, sizeof(bb)), st->accent, 0);
    return ly;
}

static void page_likes(int id, int x, int y, int w, int bottom, const UiStyle *st)
{
    const DoomoResident *r = res(id);
    char v[64];
    int ly = y;
    lib_b.scroll_max = 0;
    lib_kv(x, ly, w, STR_ICON_FOOD " Food", doomo_food_names[r->like_food % FOOD_COUNT], st->text_dim, st->text); ly += FONT_LINE;
    lib_kv(x, ly, w, STR_ICON_STAR " Hobby", doomo_hobby_names[r->like_hobby % HOBBY_COUNT], st->text_dim, st->text); ly += FONT_LINE;
    lib_kv(x, ly, w, STR_ICON_NOTE " Music", doomo_music_names[r->like_music % MUSIC_COUNT], st->text_dim, st->text); ly += FONT_LINE;
    lib_kv(x, ly, w - 12, STR_ICON_HEART " Colour", doomo_paint_names[r->like_color % PAL_PAINT_COUNT], st->text_dim, st->text);
    ui_round_rect(x + w - 9, ly, 9, 8, 2, PAL_PAINT(r->like_color % PAL_PAINT_COUNT, 0));
    ui_round_rect(x + w - 8, ly + 1, 7, 6, 2, PAL_PAINT(r->like_color % PAL_PAINT_COUNT, 1));
    ly += FONT_LINE;
    lib_kv(x, ly, w, STR_ICON_X " Hates", doomo_food_names[r->hate_food % FOOD_COUNT], st->text_dim, st->text); ly += FONT_LINE;
    lib_kv(x, ly, w, STR_ICON_FROWN " Fear", doomo_fear_names[r->fear % FEAR_COUNT], st->text_dim, st->text); ly += FONT_LINE;
    if (ly + 2 * FONT_LINE <= bottom) {
        doomo_snprintf(v, sizeof(v), "\"%s\"", doomo_catchphrases[r->catchphrase % CATCHPHRASE_COUNT]);
        ui_text_wrap(x, ly + 2, w, v, st->accent, 1);
    }
    if (lib_big() && ly + 5 * FONT_LINE <= bottom) {
        int k;
        ly += 3 * FONT_LINE;
        for (k = 0; k < PERS_COUNT && ly + FONT_LINE <= bottom; k++, ly += FONT_LINE) {
            ui_text(x, ly, doomo_persona_axis_names[k], st->text_dim);
            bar5(x + w - 42, ly + 2, r->persona[k], PAL_PAINT(PAINT_TEAL, 1));
        }
    }
}

static void page_bonds(int id, int x, int y, int w, int bottom, const UiStyle *st)
{
    const DoomoResident *r = res(id);
    char v[48];
    int ly = y, i;
    lib_b.scroll_max = 0;
    summarize(id);
    ui_text(x, ly, "You", st->text_dim);
    {
        int hearts = (r->level + 19) / 20, k;
        for (k = 0; k < 5; k++)
            ui_heart(x + 30 + k * 10, ly + 4, 3, k < hearts ? PAL_PAINT(PAINT_ROSE, 1) : PAL_RAMP(RAMP_STONE, 5));
        doomo_snprintf(v, sizeof(v), "Lv%d", r->level);
        ui_text(x + w - ui_text_width(v), ly, v, st->text);
    }
    ly += FONT_LINE + 2;
    if (A.rel_n == 0) {
        ui_text_wrap(x, ly, w, "No ties yet. Introduce them to someone!", st->text_dim, 1);
        return;
    }
    for (i = 0; i < A.rel_n && ly + FONT_LINE <= bottom - FONT_LINE; i++, ly += FONT_LINE) {
        const DoomoResident *o = res(A.rel_id[i]);
        int k = A.rel_kind[i] == 0xFF ? REL_ACQUAINTANCE : A.rel_kind[i] & REL_KIND_MASK;
        const char *kn = A.rel_id[i] == DOOMO_PLAYER_ID ? "You" :
                         k < REL_KIND_COUNT ? doomo_rel_kind_names[k] : "?";
        uint8_t kc = (k == REL_RIVAL || k == REL_NEMESIS) ? PAL_PAINT(PAINT_BLOOD, 1) :
                     (k == REL_DATING || k == REL_MARRIED) ? PAL_PAINT(PAINT_ROSE, 1) :
                     k == REL_FAMILY ? PAL_PAINT(PAINT_HONEY, 1) : st->text_dim;
        if (!o) continue;
        if (rel_has_crush(id, A.rel_id[i])) kn = "Crush";
        ui_text(x, ly, A.rel_id[i] == DOOMO_PLAYER_ID ? "(you)" : o->name, st->text);
        ui_text(x + w - ui_text_width(kn), ly, kn, kc);
    }
    doomo_snprintf(v, sizeof(v), STR_ICON_HEART "%d  " STR_ICON_HOUSE "%d  " STR_ICON_SKULL "%d",
                   A.rel_friends, A.rel_family, A.rel_foes);
    ui_text(x, bottom - FONT_LINE + 1, v, st->text_dim);
}

static void detail(int sw, int sh)
{
    int id = A.idx[lib_b.sel], hell = RES_DISTRICT(id);
    int style = hell ? WIN_DARK : WIN_MINT;
    const UiStyle *st = ui_style(style);
    const DoomoResident *r = res(id);
    int x = 4, y = 18, w = sw - 8, h = sh - 18 - 15;
    int big = lib_big(), fs = big ? 2 : 1;
    int fx = x + 6, fy = y + 6, rx = x + (big ? 110 : 64), rw = w - (rx - x) - 6, ry = y + 5, known;
    char buf[48];
    uint8_t sc = 0;
    if (!r || !lib_detail_window(x, y, w, h, style) || plat_draw_skipped()) return;
    known = shown_known(id);
    /* figure on a little rug */
    ui_fill_ellipse(fx + 24 * fs + 2, y + h - 14, 24 * fs, 5, hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_MINT, 4));
    ui_fill_ellipse(fx + 24 * fs + 2, y + h - 15, 21 * fs, 3, hell ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_RAMP(RAMP_MINT, 5));
    if (lib_b.dt >= 2) {
        int expr = A.expr;
        const DoomoImage *img;
        if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) expr = EXPR_POSSESSED;
        else if (r->status == RES_DEAD) expr = EXPR_DEAD;
        img = figure_image(&r->look, r->status, expr);
        if (img) plat_image(fx + 2, y + h - 16 - img->h * fs, img, fs, 0, 0);
    }
    if (cast_of(id) >= 0) {
        ui_star(fx + 4, fy + 4, 5, PAL_UI_GOLD);
        ui_sparkle(fx + 48 * fs, fy + 10, (lib_b.t / 3) & 15, PAL_UI_GOLD);
    }
    if (hell) {
        ui_round_rect(fx - 2, fy - 1, 48, 11, 3, PAL_PAINT(PAINT_BLOOD, 0));
        ui_text(fx + 1, fy + 1, "HELLVILE", PAL_RAMP(RAMP_LAVA, 6));
    }
    /* header */
    ui_text_shadow(rx, ry, r->name, st->accent, st->fill_lo);
    {
        const char *sn = status_name(r, &sc);
        int cw = ui_text_width(sn) + 8;
        ui_round_rect(rx + rw - cw, ry - 1, cw, 11, 3, ui_darker(sc));
        ui_round_rect(rx + rw - cw + 1, ry, cw - 2, 9, 2, sc);
        ui_text(rx + rw - cw + 4, ry + 1, sn, PAL_WHITE);
    }
    if (r->status == RES_POSSESSED || r->status == RES_SOUL_LOST) {
        doomo_snprintf(buf, sizeof(buf), "Possessed by %s", sim_possessor_name(id));
        ui_text(rx, ry + 10, lib_fit(buf, rw, buf, sizeof(buf)), PAL_UI_PURPLE);
    } else if (r->status == RES_DEAD) {
        ui_text(rx, ry + 10, doomo_death_names[r->death_cause % DEATH_COUNT], st->text_dim);
    } else {
        doomo_snprintf(buf, sizeof(buf), "%s, #%03d", RES_DISTRICT(id) ? "Hellvile" : "Hillvile",
                       id % DOOMO_DISTRICT_SIZE);
        ui_text(rx, ry + 10, buf, st->text_dim);
    }
    {
        static const char *const subs[3] = { "Profile", "Likes", "Bonds" };
        int ty = ry + 24, bottom = y + h - 6;
        ui_tabs(rx, ty, subs, 3, lib_b.sub, style);
        ty += 15;
        if (lib_b.sub == 0) page_profile(id, rx, ty, rw, bottom, st);
        else if (!known) {
            lib_b.scroll_max = 0;
            ui_text_big(rx + rw / 2 - 15, ty + 4, "???", 2, st->text_dim, st->fill_lo);
            doomo_snprintf(buf, sizeof(buf), "Chat with %s to fill in this page!", r->name);
            ui_text_wrap(rx, ty + 26, rw, buf, st->text, 1);
        } else if (lib_b.sub == 1) page_likes(id, rx, ty, rw, bottom, st);
        else page_bonds(id, rx, ty, rw, bottom, st);
    }
    lib_detail_end(x, y, w, h, style);
}

static void picker_draw(int sw, int sh)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int cw = lib_big() ? 26 : 22, chh = lib_big() ? 20 : 17;
    int w = 9 * cw + 12, h = 3 * chh + 30, x = (sw - w) / 2, y = (sh - h) / 2, i;
    plat_shade_rect(0, 0, sw, sh, 9);
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, lib_b.tab_t)) return;
    ui_text_center(sw / 2, y + 5, "Search by first letter", st->accent);
    for (i = 0; i < 27; i++) {
        int cx = x + 6 + (i % 9) * cw, cy = y + 18 + (i / 9) * chh;
        int n = A.letter_n[i], sel = i == A.pick;
        char g[4];
        uint8_t c = n ? st->text : st->text_dim;
        if (i < 26) { g[0] = (char)('A' + i); g[1] = 0; }
        else { g[0] = 'A'; g[1] = 'l'; g[2] = 'l'; g[3] = 0; }
        if (sel) {
            int b = ui_wave((int)lib_b.t, 16, 1) > 0;
            ui_round_rect(cx, cy - b, cw - 2, chh - 2, 3, st->accent);
            ui_text_center(cx + cw / 2 - 1, cy + (chh - 8) / 2 - b, g, PAL_WHITE);
        } else {
            ui_round_rect(cx, cy, cw - 2, chh - 2, 3, n ? st->fill_hi : st->fill_lo);
            ui_text_center(cx + cw / 2 - 1, cy + (chh - 8) / 2, g, c);
        }
    }
    {
        char b[32];
        doomo_snprintf(b, sizeof(b), "%d citizens", A.letter_n[A.pick]);
        ui_text_center(sw / 2, y + h - 11, b, st->text_dim);
    }
}

static void alm_draw(void)
{
    int sw = lib_sw(), sh = lib_sh(), y, lw;
    char tabn[40];
    if (lib_w_lost(0)) return;
    lib_chrome(LIB_WING_ALMANAC, alm_pct);
    if (plat_draw_skipped()) return;
    y = 18;
    if (A.letter)
        doomo_snprintf(tabn, sizeof(tabn), "%s: %c..", shelf_names[lib_b.tab], A.letter);
    else
        doomo_snprintf(tabn, sizeof(tabn), "%s", shelf_names[lib_b.tab]);
    y += lib_tab_pill(sw / 2, y, tabn, lib_b.count, lib_b.tab, SH_COUNT, WIN_MINT);
    lw = lib_big() ? 140 : 104;
    lib_list(4, y + 1, lw, sh - y - 16, 12, WIN_MINT, row);
    preview(lw + 8, y + 1, sw - lw - 12, sh - y - 16);
    {
        /* sort label above the list's top-left corner */
        char sb[24];
        doomo_snprintf(sb, sizeof(sb), "by %s", sort_names[lib_b.sort % SO_COUNT]);
        ui_text_outline(6, y - 13, sb, PAL_WHITE, PAL_RAMP(RAMP_MINT, 2));
    }
    if (lib_b.detail) {
        lib_dim_for_detail();
        detail(sw, sh);
        lib_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R " next  L/R page  SEL face  " STR_ICON_B " close", WIN_MINT);
    } else if (A.picking) {
        picker_draw(sw, sh);
        lib_hint(STR_ICON_A " search  " STR_ICON_B " cancel", WIN_MINT);
    } else {
        lib_hint(STR_ICON_A " open L/R shelf SEL sort START find", WIN_MINT);
    }
}

const DoomoScene SCENE_LIB_ALMANAC = { "LIB_ALMANAC", alm_enter, NULL, alm_update, alm_draw, 0 };
