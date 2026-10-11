/* lib_common.c - the Grand Library of HaHaland (owner: library): the
 * lobby (SCENE_LIBRARY), the browser framework every book uses, the
 * completion maths, the Bestiary discovery API and the activities table.
 *
 * Quillby the owlkin bookseller (roster id 123) keeps the place. The lobby
 * is a cosy reading room: an arched window (day or night sky), shelves of
 * tiny books, seven big featured books on the display shelf - one per wing
 * - and Quillby behind his desk, commenting on whatever you point at. A
 * possessed or departed Quillby changes the mood accordingly.
 */
#include "lib_int.h"

DOOMO_STATIC_ASSERT(THING_IMPLING == 3, lib_seen_ids_assume_impling_is_3);
DOOMO_STATIC_ASSERT(LIB_UL_SEEN(THING_LORD_VAPULA) == LIB_UL_JUKEBOX - 1, lib_seen_fit);
DOOMO_STATIC_ASSERT(LIB_UL_VISITED(LIB_WING_COUNT - 1) == LIB_UL_LAST, lib_visited_fit);

LibBrowse   lib_b;
LibWingData *lib_wp;                    /* the scene scratch (lib_int.h) */
LibWpnData  lib_wpn;
int16_t     lib_have, lib_total;
DOOMO_STATIC_ASSERT(sizeof(LibWingData) <= DOOMO_SCENE_SCRATCH, lib_wing_data_fits_the_scratch);

int lib_w_lost(int in_update)
{
    if (lib_wp && scene_scratch_mine(LIB_SCRATCH_TAG)) return 0;
    if (in_update) {
        plat_log("library: scene scratch lost, closing the wing");
        lib_wing_leave();
    } else {
        plat_clear(PAL_UI_CREAM);
    }
    return 1;
}

#define QUILLBY_ID 123

/* ====================================================================== */
/* the seven books                                                          */
extern const DoomoScene SCENE_LIB_BESTIARY, SCENE_LIB_ALMANAC, SCENE_LIB_WEAPONS,
       SCENE_LIB_ITEMS, SCENE_LIB_TROPHIES, SCENE_LIB_JUKEBOX, SCENE_LIB_GALLERY;

const LibWingInfo lib_wings[LIB_WING_COUNT] = {
    { STR_ICON_SKULL " BESTIARY", "Bestiary",
      "Every demon from the Gate, pressed like flowers. A few are still wriggling.",
      ICON_SKULL, PAINT_BLOOD, WIN_CREAM, WIN_HELL, &SCENE_LIB_BESTIARY },
    { STR_ICON_HOUSE " CITIZEN ALMANAC", "Citizen Almanac",
      "All our neighbours, both sides of the Wall. Alphabetised. Mostly.",
      ICON_HOUSE, PAINT_TEAL, WIN_MINT, WIN_MINT, &SCENE_LIB_ALMANAC },
    { STR_ICON_GUN " WEAPON CATALOG", "Weapon Catalog",
      "Rivet's blueprints! Please do not weld in the reading room.",
      ICON_GUN, PAINT_COBALT, WIN_CREAM, WIN_DARK, &SCENE_LIB_WEAPONS },
    { STR_ICON_FOOD " FOOD & ITEM DEX", "Food & Item Dex",
      "Snacks, gifts and holy water - and who loves or hates each treat.",
      ICON_FOOD, PAINT_TANGERINE, WIN_CUTE, WIN_CREAM, &SCENE_LIB_ITEMS },
    { STR_ICON_STAR " TROPHY WALL", "Trophy Wall",
      "Every prize in HaHaland, and a hint for each one you haven't won.",
      ICON_STAR, PAINT_HONEY, WIN_CREAM, WIN_CREAM, &SCENE_LIB_TROPHIES },
    { STR_ICON_NOTE " JUKEBOX", "Jukebox",
      "Every tune and every boing. Headphones are on the hook. Shhh!",
      ICON_NOTE, PAINT_GRAPE, WIN_DARK, WIN_DARK, &SCENE_LIB_JUKEBOX },
    { STR_ICON_SPARK " ART GALLERY", "Art Gallery",
      "Paint, skies and portraits of every critter. No flash photography.",
      ICON_SPARK, PAINT_ROSE, WIN_CREAM, WIN_CUTE, &SCENE_LIB_GALLERY },
};

/* ====================================================================== */
/* small helpers                                                            */
int lib_sw(void) { return plat_screen_w(); }
int lib_sh(void) { return plat_screen_h(); }
int lib_big(void) { return plat_screen_w() >= 300 && plat_screen_h() >= 200; }

int lib_bit(int id)
{
    return unlock_has(id);
}

void lib_bit_set(int id)
{
    if (id < 0 || id >= DOOMO_UNLOCK_BITS) return;
    doomo_town.unlocks[id >> 3] |= (uint8_t)(1u << (id & 7));
}

int lib_pct(int n, int d)
{
    if (d <= 0 || n <= 0) return 0;
    if (n >= d) return 100;
    n = n * 100 / d;
    return n > 99 ? 99 : n;          /* 100 only when really complete */
}

const char *lib_fit(const char *s, int w, char *buf, int cap)
{
    int i = 0, maxc;
    if (!s) s = "";
    if (ui_text_width(s) <= w) return s;
    maxc = w / FONT_ADVANCE - 1;
    if (maxc < 1) maxc = 1;
    if (maxc > cap - 2) maxc = cap - 2;
    while (s[i] && i < maxc) { buf[i] = s[i]; i++; }
    while (i > 1 && buf[i - 1] == ' ') i--;
    buf[i++] = '.';
    buf[i] = 0;
    return buf;
}

/* ====================================================================== */
/* the Bestiary's discovery API                                             */
static int is_demon(int t) { return t >= THING_IMPLING && t <= THING_LORD_VAPULA; }

int library_defeated(int thing_type)
{
    const DoomoUnlock *u;
    if (!is_demon(thing_type)) return 0;
    u = unlock_find(UL_CAT_GALLERY, GAL_DEMONS, thing_type - THING_IMPLING);
    return u && unlock_has(u->id);
}

int library_seen(int thing_type)
{
    if (!is_demon(thing_type)) return 0;
    return unlock_has(LIB_UL_SEEN(thing_type)) || library_defeated(thing_type);
}

int library_seen_count(void)
{
    int t, n = 0;
    for (t = THING_IMPLING; t <= THING_LORD_VAPULA; t++) n += library_seen(t);
    return n;
}

int library_mark_seen(int thing_type)
{
    if (!is_demon(thing_type) || unlock_has(LIB_UL_SEEN(thing_type))) return 0;
    unlock_grant(LIB_UL_SEEN(thing_type));
    if (library_seen_count() >= LIBRARY_DEMONS) unlock_grant(LIB_UL_DEMONOLOGIST);
    return 1;
}

/* Defeated demons count as sighted: copy action's gallery bits silently
 * (their own toast already played when the demon fell). */
static void sync_sightings(void)
{
    int t, defeated = 0;
    for (t = THING_IMPLING; t <= THING_LORD_VAPULA; t++)
        if (library_defeated(t)) {
            lib_bit_set(LIB_UL_SEEN(t));
            defeated++;
        }
    if (library_seen_count() >= LIBRARY_DEMONS) unlock_grant(LIB_UL_DEMONOLOGIST);
    if (defeated >= LIBRARY_DEMONS) unlock_grant(LIB_UL_FIELD);
}

/* ====================================================================== */
/* completion                                                               */
int library_wing_pct(int wing)
{
    switch (wing) {
    case LIB_WING_BESTIARY: return lib_bestiary_pct();
    case LIB_WING_ALMANAC:  return lib_almanac_pct();
    case LIB_WING_WEAPONS:  return lib_weapons_pct();
    case LIB_WING_ITEMS:    return lib_items_pct();
    case LIB_WING_TROPHIES: return lib_trophies_pct();
    case LIB_WING_JUKEBOX:  return lib_jukebox_pct();
    case LIB_WING_GALLERY:  return lib_gallery_pct();
    default: return -1;
    }
}

int library_total_pct(void)
{
    int w, s = 0;
    for (w = 0; w < LIB_WING_COUNT; w++) s += library_wing_pct(w);
    return s / LIB_WING_COUNT;
}

int library_wing_open(int wing)
{
    if (wing == LIB_WING_JUKEBOX) return unlock_has(LIB_UL_JUKEBOX);
    if (wing == LIB_WING_GALLERY) return unlock_has(LIB_UL_GALLERY);
    return wing >= 0 && wing < LIB_WING_COUNT;
}

int library_wing_visited(int wing)
{
    if (wing < 0 || wing >= LIB_WING_COUNT) return 0;
    return lib_bit(LIB_UL_VISITED(wing));
}

/* The library's own milestones that are granted by code. */
static void check_milestones(int total_pct)
{
    int w, all = 1;
    for (w = 0; w < LIB_WING_COUNT; w++) all &= library_wing_visited(w);
    if (all) unlock_grant(LIB_UL_BOOKWORM);
    if (unlock_owned_count() >= 100) unlock_grant(LIB_UL_POLISHER);
    if (progress_get(PROG_DAYS_LIVED) >= 1 || library_seen_count() > 0)
        unlock_grant(LIB_UL_JUKEBOX);
    if (progress_get(PROG_DAYS_LIVED) >= 3 || total_pct >= 25)
        unlock_grant(LIB_UL_GALLERY);
}

/* ====================================================================== */
/* opening / leaving a wing                                                 */
void lib_demo_island(void)
{
    if (town_count(-1) == 0) {
        town_new(doomo_town.mode, doomo_town.seed ? doomo_town.seed : 1234u, TOWN_POP_PREMADE);
        sim_init();
    }
}

void lib_wing_enter(int wing, int arg)
{
    memset(&lib_b, 0, sizeof(lib_b));
    if (wing == LIB_WING_WEAPONS) {
        memset(&lib_wpn, 0, sizeof(lib_wpn));
    } else {
        lib_wp = (LibWingData *)scene_scratch(LIB_SCRATCH_TAG, (uint32_t)sizeof(LibWingData));
        memset(&lib_w, 0, sizeof(lib_w));
    }
    lib_b.demo = (arg & LIB_ARG_DEMO) != 0;
    lib_b.tab = (uint8_t)LIB_ARG_TAB(arg);
    lib_b.vis = 8;
    if (lib_b.demo) lib_demo_island();
    if (wing >= 0 && wing < LIB_WING_COUNT) {
        int w, all = 1;
        lib_bit_set(LIB_UL_VISITED(wing));
        for (w = 0; w < LIB_WING_COUNT; w++) all &= library_wing_visited(w);
        if (all) unlock_grant(LIB_UL_BOOKWORM);
    }
    if (wing == LIB_WING_BESTIARY) sync_sightings();
    audio_sfx(SFX_SELECT);
    input_eat();
}

void lib_wing_leave(void)
{
    audio_sfx(SFX_CANCEL);
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_LIBRARY, 0);
}

/* ====================================================================== */
/* navigation                                                               */
void lib_clamp(void)
{
    int vis = lib_b.vis > 0 ? lib_b.vis : 1;
    if (lib_b.count <= 0) { lib_b.sel = 0; lib_b.top = 0; return; }
    lib_b.sel = (int16_t)DOOMO_CLAMP(lib_b.sel, 0, lib_b.count - 1);
    if (lib_b.sel < lib_b.top) lib_b.top = lib_b.sel;
    if (lib_b.sel >= lib_b.top + vis) lib_b.top = (int16_t)(lib_b.sel - vis + 1);
    lib_b.top = (int16_t)DOOMO_CLAMP(lib_b.top, 0, DOOMO_MAX(0, lib_b.count - vis));
}

static void moved(void)
{
    lib_b.rest = 0;
    audio_sfx(SFX_CURSOR);
}

int lib_nav(int tabs, int subs)
{
    int vis = lib_b.vis > 0 ? lib_b.vis : 1;
    lib_b.t++;
    if (lib_b.rest < 60000) lib_b.rest++;
    if (lib_b.dt < 60000) lib_b.dt++;
    if (lib_b.tab_t < 60000) lib_b.tab_t++;
    if (!lib_b.detail) {
        int old = lib_b.sel;
        if (lib_b.count > 0) {
            if (btn_repeat(BTN_UP)) lib_b.sel = (int16_t)(lib_b.sel > 0 ? lib_b.sel - 1 : lib_b.count - 1);
            if (btn_repeat(BTN_DOWN)) lib_b.sel = (int16_t)(lib_b.sel < lib_b.count - 1 ? lib_b.sel + 1 : 0);
            if (btn_repeat(BTN_LEFT)) lib_b.sel = (int16_t)DOOMO_MAX(0, lib_b.sel - vis);
            if (btn_repeat(BTN_RIGHT)) lib_b.sel = (int16_t)DOOMO_MIN(lib_b.count - 1, lib_b.sel + vis);
        }
        lib_clamp();
        if (lib_b.sel != old) { moved(); return LIBE_MOVED; }
        if (tabs > 1 && (btn_repeat(BTN_L) || btn_repeat(BTN_R))) {
            int d = btn_repeat(BTN_R) ? 1 : -1;
            lib_b.tab = (uint8_t)((lib_b.tab + tabs + d) % tabs);
            lib_b.sel = 0;
            lib_b.top = 0;
            lib_b.tab_t = 0;
            lib_b.rest = 0;
            audio_sfx(SFX_BLIP);
            return LIBE_TAB;
        }
        if (btn_pressed(BTN_SELECT)) {
            lib_b.filter++;
            lib_b.tab_t = 0;
            lib_b.rest = 0;
            audio_sfx(SFX_BLIP);
            return LIBE_FILTER;
        }
        if (btn_pressed(BTN_START)) return LIBE_START;
        if (btn_pressed(BTN_A) && lib_b.count > 0) {
            lib_b.detail = 1;
            lib_b.dt = 0;
            lib_b.sub = 0;
            lib_b.flip = 0;
            lib_b.scroll = 0;
            audio_sfx(SFX_SELECT);
            return LIBE_OPEN;
        }
        if (btn_pressed(BTN_B)) return LIBE_BACK;
        return LIBE_NONE;
    }
    /* the detail page */
    if (btn_pressed(BTN_B) || btn_pressed(BTN_A)) {
        lib_b.detail = 0;
        lib_b.rest = 0;
        audio_sfx(SFX_CANCEL);
        return LIBE_CLOSE;
    }
    if (lib_b.count > 1 && (btn_repeat(BTN_LEFT) || btn_repeat(BTN_RIGHT))) {
        int d = btn_repeat(BTN_RIGHT) ? 1 : -1;
        lib_b.sel = (int16_t)((lib_b.sel + lib_b.count + d) % lib_b.count);
        lib_clamp();
        lib_b.flip = (int8_t)d;
        lib_b.dt = 0;
        lib_b.rest = 0;
        lib_b.scroll = 0;
        audio_sfx(SFX_BLIP);
        return LIBE_FLIP;
    }
    if (subs > 1 && (btn_pressed(BTN_L) || btn_pressed(BTN_R))) {
        int d = btn_pressed(BTN_R) ? 1 : -1;
        lib_b.sub = (uint8_t)((lib_b.sub + subs + d) % subs);
        lib_b.scroll = 0;
        lib_b.flip = (int8_t)d;
        lib_b.dt = 0;
        audio_sfx(SFX_CURSOR);
        return LIBE_SUB;
    }
    if (btn_repeat(BTN_UP) && lib_b.scroll > 0) { lib_b.scroll--; audio_sfx(SFX_CURSOR); return LIBE_SCROLL; }
    if (btn_repeat(BTN_DOWN) && lib_b.scroll < lib_b.scroll_max) {
        lib_b.scroll++;
        audio_sfx(SFX_CURSOR);
        return LIBE_SCROLL;
    }
    if (btn_pressed(BTN_SELECT)) {
        lib_b.toggle ^= 1;
        audio_sfx(SFX_BLIP);
        return LIBE_TOGGLE;
    }
    return LIBE_NONE;
}

/* ====================================================================== */
/* drawing helpers                                                          */
void lib_padlock(int x, int y, uint8_t c)
{
    plat_rect(x + 2, y, 3, 1, c);
    plat_rect(x + 1, y + 1, 1, 3, c);
    plat_rect(x + 5, y + 1, 1, 3, c);
    plat_rect(x, y + 3, 7, 5, c);
    plat_rect(x + 1, y + 4, 5, 1, ui_lighter(c));
    plat_rect(x + 3, y + 5, 1, 2, ui_darker(ui_darker(c)));
}

void lib_badge(int x, int y, int pct, int style)
{
    const UiStyle *st = ui_style(style);
    char buf[8];
    int t = (int)doomo_frame_count();
    uint8_t fill = pct >= 100 ? PAL_UI_GOLD : st->title_bg;
    uint8_t out = pct >= 100 ? PAL_PAINT(PAINT_RUST, 0) : st->outline;
    if (plat_draw_skipped()) return;
    doomo_snprintf(buf, sizeof(buf), "%d%%", pct < 0 ? 0 : pct);
    ui_round_rect(x + 1, y + 2, 40, 13, 4, ui_darker(st->fill_lo));
    ui_round_rect(x, y, 40, 13, 4, out);
    ui_round_rect(x + 1, y + 1, 38, 11, 3, fill);
    plat_rect(x + 4, y + 2, 14, 1, ui_lighter(fill));
    ui_star(x + 8, y + 6, 4, pct >= 100 ? PAL_WHITE : PAL_UI_GOLD);
    ui_text_shadow(x + 16, y + 3, buf, pct >= 100 ? PAL_PAINT(PAINT_RUST, 0) : st->title, out);
    /* tiny progress strip under the number */
    plat_rect(x + 16, y + 11, 20, 1, ui_darker(fill));
    plat_rect(x + 16, y + 11, 20 * DOOMO_CLAMP(pct, 0, 100) / 100, 1, PAL_WHITE);
    if (pct >= 100) ui_sparkle(x + 38, y + 1, (t / 3) & 15, PAL_WHITE);
}

int lib_chrome(int wing, int pct)
{
    const LibWingInfo *wi = &lib_wings[wing];
    int sw = lib_sw();
    ui_backdrop(wi->backdrop, doomo_frame_count());
    if (plat_draw_skipped()) return 18;
    ui_title(sw / 2, 2, wi->title, wi->style);
    lib_badge(sw - 44, 2, pct, wi->style);
    return 18;
}

int lib_tab_pill(int cx, int y, const char *name, int n, int tab, int tabs, int style)
{
    const UiStyle *st = ui_style(style);
    char buf[40];
    int tw, w, x, t = (int)doomo_frame_count(), i, b, pop;
    if (plat_draw_skipped()) return 14;
    if (n >= 0) doomo_snprintf(buf, sizeof(buf), "%s (%d)", name, n);
    else doomo_snprintf(buf, sizeof(buf), "%s", name);
    tw = ui_text_width(buf);
    w = tw + 30;
    x = cx - w / 2;
    pop = lib_b.tab_t < 6 ? (6 - lib_b.tab_t) / 2 : 0;    /* squash on change */
    ui_round_rect(x + 1, y + 2 + pop, w, 12 - pop, 4, ui_darker(st->fill_lo));
    ui_round_rect(x, y + pop, w, 12 - pop, 4, st->outline);
    ui_round_rect(x + 1, y + 1 + pop, w - 2, 10 - pop, 3, st->accent_d);
    plat_rect(x + 5, y + 2 + pop, w - 10, 1, ui_lighter(st->accent_d));
    ui_text(x + 15, y + 2 + pop / 2, buf, st->text);
    if (tabs > 1) {
        b = ui_wave(t, 20, 1);
        plat_text(x + 3 - (b > 0), y + 2, STR_ICON_ARROW_L, st->accent);
        plat_text(x + w - 9 + (b > 0), y + 2, STR_ICON_ARROW_R, st->accent);
        /* position dots */
        if (tabs <= 16) {
            int dx = cx - tabs * 2;
            for (i = 0; i < tabs; i++)
                plat_rect(dx + i * 4, y + 13, i == tab ? 3 : 2, i == tab ? 2 : 1,
                          i == tab ? st->accent : st->text_dim);
        }
    }
    return 16;
}

void lib_hint(const char *s, int style)
{
    ui_hint(s, style);
}

void lib_list(int x, int y, int w, int h, int row_h, int style, LibRowFn row)
{
    const UiStyle *st = ui_style(style);
    int vis = (h - 8) / row_h, i, rows, bar;
    if (vis < 1) vis = 1;
    lib_b.vis = (int16_t)vis;
    lib_clamp();
    ui_window(x, y, w, h, style);
    if (plat_draw_skipped()) return;
    if (lib_b.count <= 0) {
        ui_text_center(x + w / 2, y + h / 2 - 8, "Nothing here...", st->text_dim);
        ui_text_center(x + w / 2, y + h / 2 + 2, "yet!", st->text_dim);
        return;
    }
    rows = DOOMO_MIN(vis, lib_b.count - lib_b.top);
    bar = lib_b.count > vis;
    for (i = 0; i < rows; i++) {
        int idx = lib_b.top + i, ry = y + 4 + i * row_h;
        int sel = idx == lib_b.sel;
        if (sel) {
            int rw = w - 12 - (bar ? 6 : 0);
            ui_round_rect(x + 9, ry, rw, row_h - 1, 3, ui_darker(st->accent_d));
            ui_round_rect(x + 10, ry + 1, rw - 2, row_h - 3, 2, st->accent_d);
            plat_rect(x + 12, ry + 1, rw - 6, 1, ui_lighter(st->accent_d));
            ui_cursor(x + 2, ry + (row_h - 7) / 2);
        }
        row(idx, x + 12, ry, w - 16 - (bar ? 6 : 0), sel);
    }
    if (bar) {
        int bx = x + w - 7, th = rows * row_h - 2;
        int thumb = DOOMO_MAX(4, th * vis / lib_b.count);
        int ty = y + 5 + (th - thumb) * lib_b.top / DOOMO_MAX(1, lib_b.count - vis);
        ui_round_rect(bx, y + 5, 3, th, 1, st->fill_lo);
        ui_round_rect(bx, ty, 3, thumb, 1, st->accent);
    }
}

void lib_stamp(int cx, int cy, const char *s, uint8_t c)
{
    int tw = ui_text_width(s), w = tw + 12, h = 15, x = cx - w / 2, y = cy - h / 2;
    uint8_t lc = ui_lighter(c);
    if (plat_draw_skipped()) return;
    /* an inked rubber stamp: thick frame, a little uneven */
    plat_rect(x + 1, y, w - 2, 2, c);
    plat_rect(x + 2, y + h - 2, w - 3, 2, c);
    plat_rect(x, y + 1, 2, h - 3, c);
    plat_rect(x + w - 2, y + 2, 2, h - 3, c);
    plat_rect(x + 3, y + 2, 2, 1, lc);                  /* worn ink */
    plat_rect(x + w - 7, y + h - 2, 3, 1, lc);
    ui_text(x + 7, y + 4, s, lc);
    ui_text(x + 6, y + 4, s, c);
}

void lib_kv(int x, int y, int w, const char *k, const char *v, uint8_t kc, uint8_t vc)
{
    char buf[48];
    int kw = ui_text_width(k);
    ui_text(x, y, k, kc);
    v = lib_fit(v, w - kw - 6, buf, sizeof(buf));
    ui_text(x + w - ui_text_width(v), y, v, vc);
}

void lib_pips(int x, int y, int n, int max, int icon, uint8_t on, uint8_t off)
{
    int i;
    char g[2];
    g[0] = (char)icon;
    g[1] = 0;
    for (i = 0; i < max; i++) {
        if (icon == ICON_STAR) ui_star(x + i * 8 + 3, y + 3, 3, i < n ? on : off);
        else ui_text(x + i * 7, y, g, i < n ? on : off);
    }
}

void lib_icon_chip(int cx, int cy, int r, int icon, uint8_t fill)
{
    char g[2];
    if (plat_draw_skipped()) return;
    g[0] = (char)icon;
    g[1] = 0;
    ui_fill_circle(cx, cy + 1, r, ui_darker(ui_darker(fill)));
    ui_fill_circle(cx, cy, r, ui_darker(fill));
    ui_fill_circle(cx, cy, r - 1, fill);
    plat_rect(cx - r / 2, cy - r + 2, DOOMO_MAX(r / 2, 1), 1, ui_lighter(fill));
    if (r >= 8) ui_text_big(cx - 5, cy - 8, g, 2, PAL_WHITE, ui_darker(ui_darker(fill)));
    else ui_text_outline(cx - 2, cy - 3, g, PAL_WHITE, ui_darker(ui_darker(fill)));
}

void lib_dim_for_detail(void)
{
    int sw = lib_sw(), sh = lib_sh();
    if (plat_draw_skipped()) return;
    /* while the page boings in, dim everything; afterwards only the strips
     * the page leaves visible (a full-screen shade is costly on the GBA) */
    if (lib_b.dt < 12 && !lib_b.flip) {
        plat_shade_rect(0, 0, sw, sh, 9);
        return;
    }
    plat_shade_rect(0, 0, sw, 18, 9);
    plat_shade_rect(0, sh - 15, sw, 15, 9);
    plat_shade_rect(0, 18, 4, sh - 33, 9);
    plat_shade_rect(sw - 4, 18, 4, sh - 33, 9);
}

int lib_detail_window(int x, int y, int w, int h, int style)
{
    if (lib_b.flip) {
        ui_window(x, y, w, h, style);
        return 1;
    }
    return ui_window_pop(x, y, w, h, style, lib_b.dt);
}

/* Page turn: a sheet of paper sweeps off the new page with a soft fold. */
static void page_turn(int x, int y, int w, int h, int style)
{
    const UiStyle *st = ui_style(style);
    int k = lib_b.dt, cov, px;
    if (!lib_b.flip || k >= 8 || plat_draw_skipped()) return;
    cov = (w - 8) * (8 - k) / 8;
    px = lib_b.flip > 0 ? x + w - 4 - cov : x + 4;
    plat_rect(px, y + 4, cov, h - 8, st->fill_hi);
    plat_checker(px, y + 4, cov, h - 8, st->fill);
    if (lib_b.flip > 0) {
        plat_rect(px, y + 4, 2, h - 8, ui_darker(st->fill_lo));
        plat_rect(px - 2, y + 6, 2, h - 12, st->fill_lo);
    } else {
        plat_rect(px + cov - 2, y + 4, 2, h - 8, ui_darker(st->fill_lo));
        plat_rect(px + cov, y + 6, 2, h - 12, st->fill_lo);
    }
}

void lib_detail_end(int x, int y, int w, int h, int style)
{
    page_turn(x, y, w, h, style);
}

void lib_text_block(int x, int y, int w, int lines, const char *s, uint8_t color, int style)
{
    char buf[400];
    int total, skip, n;
    if (!s) s = "";
    total = ui_text_wrap(0, 0, w, s, 0, 0);
    lib_b.scroll_max = (int16_t)DOOMO_MAX(0, total - lines);
    if (lib_b.scroll > lib_b.scroll_max) lib_b.scroll = lib_b.scroll_max;
    if (plat_draw_skipped()) return;
    skip = ui_wrap_skip(s, w, lib_b.scroll);
    n = ui_wrap_skip(s + skip, w, lines);
    if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;
    memcpy(buf, s + skip, (size_t)n);
    buf[n] = 0;
    ui_text_wrap(x, y, w, buf, color, 1);
    (void)style;           /* the arrows use the style of the open window */
    if (lib_b.scroll_max > 0)
        ui_scroll_arrows(x + w + 1, y - 5, y + lines * FONT_LINE - 2,
                         lib_b.scroll > 0, lib_b.scroll < lib_b.scroll_max);
}

/* ====================================================================== */
/* SCENE_LIBRARY: the lobby                                                 */
static struct {
    uint8_t  sel;
    uint8_t  pct[LIB_WING_COUNT];
    int16_t  have_n[LIB_WING_COUNT], total_n[LIB_WING_COUNT];
    uint8_t  total;
    uint8_t  prev_song;
    uint8_t  quip;
    uint8_t  demo;
    uint8_t  away;            /* a book is open on top of the lobby         */
    uint16_t sel_t;           /* ticks since the cursor moved               */
    uint16_t nope_t;          /* ticks since a locked book was poked        */
    uint16_t t;
} lob;

static const char *const quips_day[] = {
    "Welcome, welcome! Quiet giggling only, please.",
    "I recommend books by smelling you. You smell like an adventure!",
    "Late fees are paid in hugs. Hellvile pays in screams.",
    "Every page is pressed, dusted and lightly blessed.",
    "The Bestiary bites. Turn its pages gently.",
    "Ooh, a new reader! Mind the stairs, the third step yawns.",
};
static const char *const quips_night[] = {
    "Hoo! Night owls get the best reading lamps.",
    "Shh... the books are asleep. Read on tippy-toes.",
    "After midnight the Bestiary rattles. Don't worry. Mostly.",
};
static const char *const quips_creepy[] = {
    "THE BOOKS... ARE... OVERDUE. ALL OF THEM. FOREVER.",
    "Hoo... hoo... who reads in MY library? Heh. Heh heh.",
    "Shhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh.",
};

static const DoomoResident *quillby(int *status)
{
    const DoomoResident *r = town_res(QUILLBY_ID);
    const DoomoResident *pre = roster_premade(QUILLBY_ID);
    if (r && r->status != RES_EMPTY && pre && strcmp(r->name, pre->name) == 0) {
        *status = r->status;
        return r;
    }
    *status = RES_ALIVE;
    return pre;
}

static void lobby_refresh(void)
{
    int w, tot = 0;
    for (w = 0; w < LIB_WING_COUNT; w++) {
        int p;
        lib_have = lib_total = 0;
        p = library_wing_pct(w);
        lob.have_n[w] = lib_have;
        lob.total_n[w] = lib_total;
        lob.pct[w] = (uint8_t)DOOMO_CLAMP(p, 0, 100);
        tot += lob.pct[w];
    }
    lob.total = (uint8_t)(tot / LIB_WING_COUNT);
    check_milestones(lob.total);
}

static void lobby_enter(int arg)
{
    lob.demo = (arg & LIB_ARG_DEMO) != 0;
    if (lob.demo) lib_demo_island();
    if (arg >= 0 && LIB_ARG_TAB(arg) < LIB_WING_COUNT) lob.sel = (uint8_t)LIB_ARG_TAB(arg);
    lob.t = 0;
    lob.away = 0;
    lob.sel_t = 0;
    lob.nope_t = 999;
    lob.quip = (uint8_t)(doomo_town.day % 7);
    lob.prev_song = (uint8_t)audio_music_current();
    audio_music(SONG_HAHALAND_NIGHT);
    sync_sightings();
    unlock_grant(LIB_UL_CARD);
    if (town_is_night()) unlock_grant(LIB_UL_NIGHT_OWL);
    lobby_refresh();
    input_eat();
}

static void lobby_leave(void)
{
    if (lob.prev_song != SONG_HAHALAND_NIGHT) audio_music(lob.prev_song);
}

static int book_open(int w)
{
    return library_wing_open(w) || lob.demo;
}

static void lobby_update(void)
{
    int old = lob.sel;
    lob.t++;
    if (lob.sel_t < 60000) lob.sel_t++;
    if (lob.nope_t < 60000) lob.nope_t++;
    /* back from a book: its numbers may have changed */
    if (lob.away) {
        lob.away = 0;
        lobby_refresh();
    }
    if (btn_repeat(BTN_LEFT)) lob.sel = (uint8_t)((lob.sel + LIB_WING_COUNT - 1) % LIB_WING_COUNT);
    if (btn_repeat(BTN_RIGHT)) lob.sel = (uint8_t)((lob.sel + 1) % LIB_WING_COUNT);
    if (lob.sel != old) {
        lob.sel_t = 0;
        lob.nope_t = 999;
        audio_sfx(SFX_CURSOR);
    }
    if (btn_pressed(BTN_A)) {
        if (book_open(lob.sel)) {
            lob.away = 1;
            scene_push(lib_wings[lob.sel].scene, lob.demo ? LIB_ARG_DEMO : 0);
            return;
        }
        lob.nope_t = 0;
        audio_sfx(SFX_ERROR);
    }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop();
        else scene_set(&SCENE_TITLE, 0);
    }
}

/* ---- lobby drawing --------------------------------------------------------- */
static void draw_wall(int sw, int sh, int floor_y)
{
    int x;
    plat_rect(0, 0, sw, floor_y, PAL_RAMP(RAMP_PEACH, 6));
    plat_rect(0, 0, sw, 16, PAL_RAMP(RAMP_PEACH, 5));
    plat_checker(0, 16, sw, 1, PAL_RAMP(RAMP_PEACH, 5));
    /* wallpaper: soft stripes with tiny hearts */
    for (x = 6; x < sw; x += 16) {
        plat_rect(x, 17, 3, floor_y - 17, PAL_RAMP(RAMP_PEACH, 7));
        ui_heart(x + 9, 26 + ((x / 16) & 1) * 30, 2, PAL_RAMP(RAMP_PASTEL_PINK, 6));
    }
    /* wainscot + floor boards */
    plat_rect(0, floor_y - 6, sw, 6, PAL_RAMP(RAMP_WOOD, 4));
    plat_rect(0, floor_y - 6, sw, 1, PAL_RAMP(RAMP_WOOD, 6));
    plat_rect(0, floor_y, sw, sh - floor_y, PAL_RAMP(RAMP_WOOD, 3));
    for (x = 0; x < sw; x += 22) plat_rect(x + ((x / 22) & 1) * 9, floor_y + 1, 1, sh - floor_y, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(0, floor_y, sw, 1, PAL_RAMP(RAMP_WOOD, 5));
}

static void draw_window(int cx, int y, int w, int h)
{
    int x = cx - w / 2, night = town_is_night(), i, t = lob.t;
    uint8_t sky0 = night ? PAL_RAMP(RAMP_SKY, 0) : PAL_RAMP(RAMP_SKY, 6);
    uint8_t sky1 = night ? PAL_RAMP(RAMP_SKY, 1) : PAL_RAMP(RAMP_SKY, 5);
    ui_round_rect(x - 3, y - 3, w + 6, h + 5, w / 2 > 12 ? 12 : w / 2, PAL_RAMP(RAMP_WOOD, 2));
    ui_round_rect(x - 2, y - 2, w + 4, h + 3, w / 2 > 11 ? 11 : w / 2, PAL_RAMP(RAMP_WOOD, 5));
    ui_round_rect(x, y, w, h, w / 2 > 10 ? 10 : w / 2, sky1);
    plat_rect(x + 2, y + h / 2, w - 4, h / 2, sky0);
    plat_checker(x + 2, y + h / 2 - 1, w - 4, 1, sky0);
    if (night) {
        ui_fill_circle(x + w * 2 / 3, y + h / 3, 5, PAL_RAMP(RAMP_CANDY, 7));
        ui_fill_circle(x + w * 2 / 3 + 2, y + h / 3 - 1, 4, sky1);
        for (i = 0; i < 6; i++) {
            uint32_t s = doomo_hash32((uint32_t)i * 31u + 7u);
            int sx = x + 3 + (int)(s % (uint32_t)(w - 6)), sy = y + 3 + (int)((s >> 8) % (uint32_t)(h - 6));
            if (((t / 8 + i) & 7) != 0) plat_rect(sx, sy, 1, 1, PAL_WHITE);
        }
    } else {
        int cxl = x + (t / 6) % (w + 20) - 10;
        ui_fill_circle(x + 6, y + 6, 4, PAL_RAMP(RAMP_CANDY, 7));
        ui_fill_ellipse(cxl, y + h / 3, 6, 3, PAL_WHITE);
        ui_fill_ellipse(cxl + 4, y + h / 3 - 2, 4, 3, PAL_WHITE);
    }
    /* mullions */
    plat_rect(cx - 1, y, 2, h, PAL_RAMP(RAMP_WOOD, 5));
    plat_rect(x, y + h / 2, w, 2, PAL_RAMP(RAMP_WOOD, 5));
    plat_rect(x + 2, y + 2, 2, h / 3, PAL_WHITE);
    /* the sill */
    plat_rect(x - 5, y + h, w + 10, 3, PAL_RAMP(RAMP_WOOD, 4));
    plat_rect(x - 5, y + h, w + 10, 1, PAL_RAMP(RAMP_WOOD, 6));
}

/* A small shelf of tiny book spines (decor). */
static void draw_mini_shelf(int x, int y, int w, uint32_t seed)
{
    int cx = x + 2;
    plat_rect(x, y + 12, w, 3, PAL_RAMP(RAMP_WOOD, 4));
    plat_rect(x, y + 12, w, 1, PAL_RAMP(RAMP_WOOD, 6));
    plat_rect(x, y + 15, w, 1, PAL_RAMP(RAMP_WOOD, 1));
    while (cx < x + w - 4) {
        uint32_t s = doomo_hash32(seed++);
        int bw = 3 + (int)(s % 3), bh = 8 + (int)((s >> 4) % 5);
        int p = (int)((s >> 9) % 32u);
        uint8_t c = PAL_PAINT(p, 1);
        if (cx + bw > x + w - 2) break;
        if ((s >> 15) % 7 == 0) {                          /* a leaning one */
            plat_rect(cx, y + 12 - bh + 3, bw + 1, bh - 3, PAL_PAINT(p, 0));
            cx += bw + 2;
            continue;
        }
        plat_rect(cx, y + 12 - bh, bw, bh, c);
        plat_rect(cx, y + 12 - bh, 1, bh, PAL_PAINT(p, 2));
        plat_rect(cx, y + 12 - bh + 2, bw, 1, PAL_UI_GOLD);
        cx += bw + ((s >> 20) & 1);
    }
}

static void draw_book(int x, int y, int bw, int bh, int w, int selected, int open)
{
    const LibWingInfo *wi = &lib_wings[w];
    uint8_t c0, c1, c2;
    char g[2];
    int pct = lob.pct[w];
    if (open) {
        c0 = PAL_PAINT(wi->paint, 0); c1 = PAL_PAINT(wi->paint, 1); c2 = PAL_PAINT(wi->paint, 2);
    } else {
        c0 = PAL_RAMP(RAMP_STONE, 2); c1 = PAL_RAMP(RAMP_STONE, 4); c2 = PAL_RAMP(RAMP_STONE, 5);
    }
    /* shadow on the shelf */
    plat_checker(x + 2, y + bh, bw, 2, PAL_RAMP(RAMP_WOOD, 1));
    ui_round_rect(x, y, bw, bh, 2, ui_darker(c0));
    ui_round_rect(x + 1, y + 1, bw - 2, bh - 2, 1, c1);
    /* spine band + highlight */
    plat_rect(x + 1, y + 2, 3, bh - 4, c0);
    plat_rect(x + 4, y + 2, 1, bh - 4, c2);
    /* page block peeking at the bottom-right */
    plat_rect(x + 5, y + bh - 3, bw - 7, 2, PAL_UI_CREAM);
    plat_rect(x + 5, y + bh - 2, bw - 7, 1, PAL_RAMP(RAMP_PEACH, 6));
    /* gold bands */
    plat_rect(x + 5, y + 4, bw - 7, 2, open ? PAL_UI_GOLD : PAL_RAMP(RAMP_STONE, 6));
    plat_rect(x + 5, y + bh - 8, bw - 7, 1, open ? PAL_UI_GOLD : PAL_RAMP(RAMP_STONE, 6));
    /* the emblem */
    g[0] = (char)wi->icon;
    g[1] = 0;
    {
        int ex = x + 4 + (bw - 4) / 2, ey = y + bh / 2 - 1;
        ui_fill_circle(ex, ey, bw >= 28 ? 8 : 7, c0);
        ui_fill_circle(ex, ey - 1, bw >= 28 ? 7 : 6, c2);
        if (bw >= 28) ui_text_big(ex - 5, ey - 9, g, 2, PAL_WHITE, c0);
        else ui_text_outline(ex - 2, ey - 4, g, PAL_WHITE, c0);
    }
    /* bookmark ribbon (gold when the wing is complete) */
    {
        int rx = x + bw - 6, ry = y + bh - 2;
        uint8_t rc = pct >= 100 ? PAL_UI_GOLD : PAL_PAINT(PAINT_CHERRY, 1);
        int sway = selected ? ui_wave(lob.t, 24, 1) : 0;
        plat_rect(rx + sway, ry, 3, 6, rc);
        plat_rect(rx + sway, ry + 6, 1, 1, rc);
        plat_rect(rx + 2 + sway, ry + 6, 1, 1, rc);
    }
    if (!open) {
        /* chains + padlock */
        plat_rect(x - 1, y + bh / 2 - 6, bw + 2, 2, PAL_RAMP(RAMP_STEEL, 4));
        plat_rect(x - 1, y + bh / 2 + 6, bw + 2, 2, PAL_RAMP(RAMP_STEEL, 4));
        lib_padlock(x + bw / 2 - 3, y + bh / 2 + 3, PAL_UI_GOLD);
    } else if (pct >= 100) {
        ui_star(x + bw - 3, y + 2, 4, PAL_UI_GOLD);
    }
}

static void draw_books(int sw, int shelf_y, int bw, int bh)
{
    int gap = lib_big() ? 10 : 7, n = LIB_WING_COUNT, i;
    int total = n * bw + (n - 1) * gap, x0 = (sw - total) / 2;
    /* the display shelf */
    plat_rect(x0 - 10, shelf_y, total + 20, 5, PAL_RAMP(RAMP_WOOD, 4));
    plat_rect(x0 - 10, shelf_y, total + 20, 1, PAL_RAMP(RAMP_WOOD, 6));
    plat_rect(x0 - 10, shelf_y + 5, total + 20, 1, PAL_RAMP(RAMP_WOOD, 1));
    plat_rect(x0 - 6, shelf_y + 6, 4, 6, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(x0 + total + 2, shelf_y + 6, 4, 6, PAL_RAMP(RAMP_WOOD, 2));
    for (i = 0; i < n; i++) {
        int x = x0 + i * (bw + gap), y = shelf_y - bh, sel = i == lob.sel;
        int open = book_open(i);
        if (sel) {
            /* pulled out: anticipation dip, overshoot, then a gentle bob */
            int k = lob.sel_t, lift;
            if (k < 2) lift = -1;
            else if (k < 6) lift = 3 + (k - 2) * 2;
            else if (k < 9) lift = 11 - (k - 6);
            else lift = 8 + ui_wave(lob.t, 30, 1);
            if (lob.nope_t < 12) x += (lob.nope_t & 2) ? -2 : 2;   /* rattle */
            y -= lift;
            ui_sparkle(x - 3, y + 4, (lob.t / 2) & 15, PAL_WHITE);
            ui_sparkle(x + bw + 2, y + bh / 2, (lob.t / 2 + 8) & 15, PAL_UI_GOLD);
        }
        draw_book(x, y, bw, bh, i, sel, open);
    }
    /* a candle at the end of the shelf, a little skull bookend at the other */
    {
        int cx = x0 + total + 8, fl = (lob.t / 3) & 3;
        plat_rect(cx - 2, shelf_y - 9, 4, 9, PAL_UI_CREAM);
        plat_rect(cx - 2, shelf_y - 9, 1, 9, PAL_WHITE);
        plat_rect(cx, shelf_y - 12, 1, 3, PAL_GRAY_D);
        ui_fill_ellipse(cx, shelf_y - 14 - (fl == 1), 2, 3 + (fl == 2), PAL_RAMP(RAMP_LAVA, 6));
        plat_rect(cx, shelf_y - 14, 1, 2, PAL_WHITE);
        cx = x0 - 9;
        ui_fill_circle(cx, shelf_y - 5, 4, PAL_RAMP(RAMP_BONE, 7));
        plat_rect(cx - 2, shelf_y - 6, 2, 2, PAL_RAMP(RAMP_BONE, 1));
        plat_rect(cx + 1, shelf_y - 6, 2, 2, PAL_RAMP(RAMP_BONE, 1));
        plat_rect(cx - 2, shelf_y - 2, 5, 2, PAL_RAMP(RAMP_BONE, 6));
    }
}

static const char *lobby_quip(int status)
{
    if (status == RES_POSSESSED || status == RES_SOUL_LOST)
        return quips_creepy[(lob.t / 150) % DOOMO_COUNTOF(quips_creepy)];
    if (status == RES_DEAD)
        return "(A note on the desk: \"Self-service. Please return books anyway. - Q\")";
    if (lob.nope_t < 90) {
        const DoomoUnlock *u = unlock_by_id(lob.sel == LIB_WING_JUKEBOX ? LIB_UL_JUKEBOX : LIB_UL_GALLERY);
        return u ? u->how : "That one's locked!";
    }
    if (lob.sel_t < 150) return lib_wings[lob.sel].blurb;
    if (town_is_night())
        return quips_night[(lob.quip + lob.t / 150) % DOOMO_COUNTOF(quips_night)];
    return quips_day[(lob.quip + lob.t / 150) % DOOMO_COUNTOF(quips_day)];
}

static void lobby_draw(void)
{
    int sw = lib_sw(), sh = lib_sh(), big = lib_big();
    int floor_y = sh - (big ? 34 : 22);
    int bw = big ? 30 : 22, bh = big ? 46 : 34;
    int shelf_y = big ? 104 : 66;
    int status;
    const DoomoResident *q = quillby(&status);
    const UiStyle *st = ui_style(WIN_CREAM);
    if (plat_draw_skipped()) return;
    draw_wall(sw, sh, floor_y);
    draw_window(sw / 2, big ? 26 : 22, big ? 64 : 48, big ? 44 : 26);
    draw_mini_shelf(6, big ? 26 : 18, sw / 2 - (big ? 50 : 38), 11u);
    draw_mini_shelf(sw / 2 + (big ? 44 : 32), big ? 26 : 18, sw / 2 - (big ? 50 : 38), 77u);
    if (big) {
        draw_mini_shelf(6, 48, sw / 2 - 50, 211u);
        draw_mini_shelf(sw / 2 + 44, 48, sw / 2 - 50, 307u);
    }
    ui_title(sw / 2, 2, STR_ICON_STAR " THE GRAND LIBRARY " STR_ICON_STAR, WIN_CREAM);
    lib_badge(sw - 44, 2, lob.total, WIN_CREAM);
    draw_books(sw, shelf_y, bw, bh);
    /* Quillby behind the desk */
    {
        int qx = big ? 8 : 3, qy = big ? sh - 112 : sh - 92;
        int dy = big ? sh - 44 : sh - 30;
        if (status != RES_DEAD && q) {
            int expr = EXPR_HAPPY;
            const DoomoImage *img;
            if (status == RES_POSSESSED || status == RES_SOUL_LOST) expr = EXPR_POSSESSED;
            else if (lob.nope_t < 40) expr = EXPR_SHOCK;
            else if (lob.sel_t < 45) expr = EXPR_TALK;
            else if (town_is_night() && ((lob.t / 200) & 1)) expr = EXPR_SLEEPY;
            img = figure_image(&q->look, status, expr);
            if (img) plat_image(qx, qy, img, 1, 0, 0);
        }
        /* desk */
        plat_rect(0, dy, (big ? 66 : 52), sh - dy, PAL_RAMP(RAMP_WOOD, 3));
        plat_rect(0, dy, (big ? 66 : 52), 3, PAL_RAMP(RAMP_WOOD, 5));
        plat_rect(0, dy + 3, (big ? 66 : 52), 1, PAL_RAMP(RAMP_WOOD, 1));
        /* a stack of books and a stamp on the desk */
        plat_rect(4, dy - 5, 14, 5, PAL_PAINT(PAINT_SKY, 1));
        plat_rect(5, dy - 9, 12, 4, PAL_PAINT(PAINT_ROSE, 1));
        plat_rect(5, dy - 9, 12, 1, PAL_PAINT(PAINT_ROSE, 2));
        plat_rect(4, dy - 5, 14, 1, PAL_PAINT(PAINT_SKY, 2));
        plat_rect(big ? 50 : 38, dy - 6, 6, 6, PAL_PAINT(PAINT_CHERRY, 1));
        plat_rect(big ? 52 : 40, dy - 9, 2, 3, PAL_RAMP(RAMP_WOOD, 2));
        ui_text(4, dy + 6, "Quillby", PAL_RAMP(RAMP_WOOD, 7));
    }
    /* the info / speech panel */
    {
        int x = big ? 72 : 56, y = shelf_y + (big ? 14 : 12), w = sw - x - 4;
        int h = sh - 16 - y, w_sel = lob.sel, open = book_open(w_sel);
        char line[48];
        uint8_t ink = st->text;
        ui_window(x, y, w, h, WIN_CREAM);
        /* the tail of the speech bubble towards Quillby */
        plat_rect(x - 3, y + 10, 4, 3, st->outline);
        plat_rect(x - 1, y + 11, 3, 1, st->fill_hi);
        ui_text_shadow(x + 6, y + 4, lib_wings[w_sel].name, st->accent, st->fill_lo);
        if (!open) lib_padlock(x + 10 + ui_text_width(lib_wings[w_sel].name), y + 4, st->accent);
        {
            static const char *const what[LIB_WING_COUNT] = {
                "seen", "known", "parts", "in bag", "won", "songs", "exhibits"
            };
            if (lob.total_n[w_sel] > 0 && big)
                doomo_snprintf(line, sizeof(line), "%d/%d %s", lob.have_n[w_sel], lob.total_n[w_sel], what[w_sel]);
            else if (lob.total_n[w_sel] > 0)
                doomo_snprintf(line, sizeof(line), "%d/%d", lob.have_n[w_sel], lob.total_n[w_sel]);
            else
                doomo_snprintf(line, sizeof(line), "%d%%", lob.pct[w_sel]);
            ui_text(x + w - 8 - ui_text_width(line), y + 4, line, st->text_dim);
        }
        ui_text_wrap(x + 6, y + 15, w - 12, lobby_quip(status), ink, 1);
        ui_bar(x + 6, y + h - 10, w - 12, 6, lob.pct[w_sel], 100,
               lob.pct[w_sel] >= 100 ? PAL_UI_GOLD : PAL_PAINT(lib_wings[w_sel].paint, 1));
    }
    /* dust motes in the window light */
    {
        int i;
        for (i = 0; i < 5; i++) {
            uint32_t s = doomo_hash32((uint32_t)i * 977u + 3u);
            int span = shelf_y - 30;
            int mx = sw / 2 - 30 + (int)(s % 60u) + ui_wave(lob.t + i * 40, 120, 5);
            int my = 20 + (int)((s >> 8) % (uint32_t)span + lob.t / 4) % span;
            ui_sparkle(mx, my, (int)((lob.t / 3 + s) & 15), PAL_RAMP(RAMP_CANDY, 7));
        }
    }
    lib_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R " browse  " STR_ICON_A " read  " STR_ICON_B " leave", WIN_CREAM);
}

const DoomoScene SCENE_LIBRARY = { "LIBRARY", lobby_enter, lobby_leave, lobby_update, lobby_draw, 0 };

/* ====================================================================== */
/* activities: the lobby everywhere, a shortcut to every book in free play  */
#define FP ACTF_FREEPLAY
static const DoomoActivity lib_activities[] = {
    { "Grand Library", "Quillby's books: demons, folks & more", ACTLOC_LIBRARY,
      ACTF_FREEPLAY | ACTF_STORY, ICON_STAR, 1, ACT_ALWAYS, &SCENE_LIBRARY, 0 },
    { "Bestiary", "Every demon, pressed like a flower", ACTLOC_LIBRARY, FP,
      ICON_SKULL, 0, ACT_ALWAYS, &SCENE_LIB_BESTIARY, 0 },
    { "Citizen Almanac", "All 1000 neighbours, alphabetised", ACTLOC_LIBRARY, FP,
      ICON_HOUSE, 0, ACT_ALWAYS, &SCENE_LIB_ALMANAC, 0 },
    { "Weapon Catalog", "Every weld part, with blueprints", ACTLOC_LIBRARY, FP,
      ICON_GUN, 0, ACT_ALWAYS, &SCENE_LIB_WEAPONS, 0 },
    { "Food & Item Dex", "Snacks, gifts and who loves them", ACTLOC_LIBRARY, FP,
      ICON_FOOD, 0, ACT_ALWAYS, &SCENE_LIB_ITEMS, 0 },
    { "Trophy Wall", "Every unlock and how to earn it", ACTLOC_LIBRARY, FP,
      ICON_CHECK, 0, ACT_ALWAYS, &SCENE_LIB_TROPHIES, 0 },
    { "Jukebox", "Every song and sound effect", ACTLOC_LIBRARY, FP,
      ICON_NOTE, 0, LIB_UL_JUKEBOX, &SCENE_LIB_JUKEBOX, 0 },
    { "Art Gallery", "Textures, skies, sprites, paint", ACTLOC_LIBRARY, FP,
      ICON_SPARK, 0, LIB_UL_GALLERY, &SCENE_LIB_GALLERY, 0 },
    { NULL, NULL, 0, 0, 0, 0, 0, NULL, 0 }
};
const DoomoActivity *const doomo_activities_collections = lib_activities;
