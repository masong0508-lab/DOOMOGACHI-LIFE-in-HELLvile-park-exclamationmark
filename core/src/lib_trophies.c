/* lib_trophies.c - SCENE_LIB_TROPHIES: the Trophy Wall (owner: library).
 *
 * The whole unlocks registry as a wall of little cups: L/R picks the
 * kind (creator parts, paints, weapon parts, shop items, modes, cheats,
 * gallery, styles, pet forms, secrets) or the Stats page (every progress
 * counter); SELECT shows all / won / still to win / "almost there" (the
 * locked ones with a counter rule, closest first). Each entry shows its
 * hint and, for counter rules, a progress bar. The view is rebuilt only
 * when the kind or filter changes (one pass over the registry).
 */
#include "lib_int.h"

#define T (lib_w.tro)
#define N_CATS 10
enum { TAB_ALL = 0, TAB_STATS = N_CATS + 1, N_TABS = N_CATS + 2 };
static const char *const filter_names[4] = { "", "won", "to win", "almost there" };
static const uint8_t cat_icon[N_CATS] = {
    ICON_SMILE, ICON_DROP, ICON_GUN, ICON_COIN, ICON_STAR, ICON_BOLT, ICON_SPARK,
    ICON_HEART, ICON_EGG, ICON_CHECK
};
static const uint8_t cat_paint[N_CATS] = {
    PAINT_ROSE, PAINT_SKY, PAINT_COBALT, PAINT_HONEY, PAINT_GRAPE, PAINT_LIME,
    PAINT_MAGENTA, PAINT_BUBBLEGUM, PAINT_TEAL, PAINT_TANGERINE
};

/* ---- completion ------------------------------------------------------------ */
int lib_trophies_pct(void)
{
    int owned = unlock_owned_count();
    if (owned >= 100) unlock_grant(LIB_UL_POLISHER);
    lib_have = (int16_t)unlock_owned_count();
    lib_total = (int16_t)unlock_count();
    return lib_pct(lib_have, lib_total);
}

static int stats_tab(void) { return lib_b.tab == TAB_STATS; }

/* Progress of a counter rule in 1/1000 (0 if none). */
static int permille(const DoomoUnlock *u)
{
    uint32_t v;
    if (!u->need || u->prog >= PROG_COUNT) return 0;
    v = progress_get(u->prog);
    if (v >= u->need) return 1000;
    return (int)((uint64_t)v * 1000u / u->need);
}

static void rebuild(void)
{
    int i, n = 0, total = unlock_count(), f = lib_b.filter % 4;
    if (stats_tab()) {
        for (i = 0; i < PROG_COUNT; i++) T.idx[n++] = (uint16_t)i;
        lib_b.count = (int16_t)n;
        lib_clamp();
        return;
    }
    for (i = 0; i < total && n < LIB_TROPHY_MAX; i++) {
        const DoomoUnlock *u = unlock_at(i);
        int own = unlock_has(u->id);
        if (lib_b.tab != TAB_ALL && u->cat != lib_b.tab - 1) continue;
        if (f == 1 && !own) continue;
        if (f == 2 && own) continue;
        if (f == 3 && (own || !u->need)) continue;
        T.idx[n++] = (uint16_t)i;
    }
    lib_b.count = (int16_t)n;
    if (f == 3) {            /* closest first */
        static const int16_t gaps[] = { 132, 57, 23, 10, 4, 1 };
        int g, j;
        for (g = 0; g < (int)DOOMO_COUNTOF(gaps); g++) {
            int gap = gaps[g];
            for (i = gap; i < n; i++) {
                uint16_t v = T.idx[i];
                int pv = permille(unlock_at(v));
                for (j = i; j >= gap && permille(unlock_at(T.idx[j - gap])) < pv; j -= gap)
                    T.idx[j] = T.idx[j - gap];
                T.idx[j] = v;
            }
        }
    }
    lib_clamp();
}

/* ---- scene -------------------------------------------------------------------- */
static uint8_t tro_pct;
static int16_t tro_owned, tro_total;

static void tro_enter(int arg)
{
    lib_wing_enter(LIB_WING_TROPHIES, arg);
    if (lib_b.tab >= N_TABS) lib_b.tab = 0;
    tro_pct = (uint8_t)lib_trophies_pct();
    tro_owned = (int16_t)unlock_owned_count();
    tro_total = (int16_t)unlock_count();
    rebuild();
}

static void tro_update(void)
{
    switch (lib_nav(N_TABS, 0)) {
    case LIBE_TAB: case LIBE_FILTER: rebuild(); break;
    case LIBE_BACK: lib_wing_leave(); break;
    case LIBE_OPEN: case LIBE_FLIP:
        if (!stats_tab() && unlock_has(unlock_at(T.idx[lib_b.sel])->id)) audio_sfx(SFX_SPARKLE);
        break;
    default: break;
    }
}

/* ---- drawing -------------------------------------------------------------------- */
/* A little trophy cup: gold when won, a grey silhouette when not. */
static void cup(int cx, int cy, int s, int won, int shine)
{
    uint8_t c0 = won ? PAL_RAMP(RAMP_CANDY, 2) : PAL_RAMP(RAMP_STONE, 3);
    uint8_t c1 = won ? PAL_UI_GOLD : PAL_RAMP(RAMP_STONE, 4);
    uint8_t c2 = won ? PAL_RAMP(RAMP_CANDY, 7) : PAL_RAMP(RAMP_STONE, 5);
    /* handles */
    ui_fill_circle(cx - 5 * s, cy - 3 * s, 3 * s, c0);
    ui_fill_circle(cx - 5 * s, cy - 3 * s, 2 * s - 1, won ? PAL_UI_CREAM : PAL_RAMP(RAMP_STONE, 6));
    ui_fill_circle(cx + 5 * s, cy - 3 * s, 3 * s, c0);
    ui_fill_circle(cx + 5 * s, cy - 3 * s, 2 * s - 1, won ? PAL_UI_CREAM : PAL_RAMP(RAMP_STONE, 6));
    /* bowl */
    ui_fill_ellipse(cx, cy - 3 * s, 5 * s, 5 * s, c0);
    ui_fill_ellipse(cx, cy - 4 * s, 5 * s - 1, 4 * s, c1);
    plat_rect(cx - 5 * s, cy - 8 * s, 10 * s + 1, 2 * s, c1);
    plat_rect(cx - 5 * s, cy - 8 * s, 10 * s + 1, 1, c2);
    plat_rect(cx - 3 * s, cy - 6 * s, s, 3 * s, c2);              /* gloss */
    /* stem + base */
    plat_rect(cx - s, cy + s, 2 * s + 1, 2 * s, c0);
    ui_round_rect(cx - 4 * s, cy + 3 * s, 8 * s + 1, 2 * s + 1, 1, c0);
    plat_rect(cx - 4 * s + 1, cy + 3 * s, 8 * s - 1, 1, c1);
    if (won && shine >= 0) ui_sparkle(cx + 3 * s, cy - 7 * s, shine & 15, PAL_WHITE);
    if (!won) plat_text(cx - 2, cy - 7 - 2 * s, "?", PAL_RAMP(RAMP_STONE, 6));
}

static void row(int i, int x, int y, int w, int sel)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    char buf[48], g[2];
    (void)sel;
    if (stats_tab()) {
        int p = T.idx[i];
        doomo_snprintf(g, sizeof(g), "%c", ICON_STAR);
        plat_text(x, y + 2, g, PAL_UI_GOLD);
        ui_text(x + 9, y + 2, lib_fit(doomo_progress_names[p], w - 44, buf, sizeof(buf)), st->text);
        doomo_snprintf(buf, sizeof(buf), "%u", (unsigned)progress_get(p));
        ui_text(x + w - ui_text_width(buf), y + 2, buf, st->accent);
        return;
    }
    {
        const DoomoUnlock *u = unlock_at(T.idx[i]);
        int own = unlock_has(u->id), c = u->cat % N_CATS;
        g[0] = (char)cat_icon[c];
        g[1] = 0;
        plat_text(x, y + 2, g, own ? PAL_PAINT(cat_paint[c], 1) : st->text_dim);
        ui_text(x + 9, y + 2, lib_fit(u->name, w - 20, buf, sizeof(buf)), own ? st->text : st->text_dim);
        if (own) plat_text(x + w - 7, y + 2, STR_ICON_CHECK, PAL_PAINT(PAINT_FOREST, 1));
        else lib_padlock(x + w - 7, y + 2, st->text_dim);
    }
}

static int progress_lines(int x, int y, int w, const DoomoUnlock *u, const UiStyle *st)
{
    char buf[32], nb[40];
    uint32_t v;
    if (!u->need || u->prog >= PROG_COUNT) return y;
    v = progress_get(u->prog);
    if (v > u->need) v = u->need;
    doomo_snprintf(buf, sizeof(buf), "%u / %u", (unsigned)v, (unsigned)u->need);
    ui_text(x, y, lib_fit(doomo_progress_names[u->prog], w - ui_text_width(buf) - 6, nb, sizeof(nb)), st->text_dim);
    ui_text(x + w - ui_text_width(buf), y, buf, st->text);
    ui_bar(x, y + FONT_LINE + 1, w, 6, (int)v, (int)u->need,
           v >= u->need ? PAL_UI_GOLD : PAL_PAINT(PAINT_TANGERINE, 1));
    return y + FONT_LINE + 10;
}

static void stats_panel(int x, int y, int w, int h, int p, const UiStyle *st)
{
    static int16_t next_for1, next_cached;     /* p + 1 (0 = nothing cached) */
    static uint32_t next_v;
    char buf[48];
    int i, n = unlock_count(), next = 0;
    uint32_t v = progress_get(p);
    ui_text_wrap(x, y, w, doomo_progress_names[p], st->accent, 1);
    doomo_snprintf(buf, sizeof(buf), "%u", (unsigned)v);
    ui_star(x + 6, y + 29, 6, PAL_UI_GOLD);
    ui_text_big(x + 16, y + 22, buf, 2, st->accent, PAL_WHITE);
    /* the next unlock this counter is working towards (one registry pass
     * per selection, not per frame) */
    if (next_for1 != p + 1 || next_v != v) {
        for (i = 0; i < n; i++) {
            const DoomoUnlock *u = unlock_at(i);
            if (u->need && u->prog == p && u->need > v && (!next || u->need < unlock_at(next - 1)->need))
                next = i + 1;
        }
        next_for1 = (int16_t)(p + 1);
        next_v = v;
        next_cached = (int16_t)next;
    }
    next = next_cached;
    if (next) {
        const DoomoUnlock *u = unlock_at(next - 1);
        ui_text(x, y + 44, "Next:", st->text_dim);
        ui_text_wrap(x, y + 44 + FONT_LINE, w, u->name, st->text, 1);
        ui_bar(x, y + 44 + 3 * FONT_LINE, w, 6, (int)v, (int)u->need, PAL_PAINT(PAINT_TANGERINE, 1));
    } else if (y + 44 < y + h) {
        ui_text_wrap(x, y + 44, w, "Every prize for this one is won!", st->text_dim, 1);
    }
}

static void preview(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    const DoomoUnlock *u;
    int own, ty, big = lib_big();
    char buf[48];
    ui_window(x, y, w, h, WIN_CREAM);
    if (lib_b.count <= 0 || plat_draw_skipped()) return;
    if (stats_tab()) { stats_panel(x + 6, y + 6, w - 12, h - 12, T.idx[lib_b.sel], st); return; }
    u = unlock_at(T.idx[lib_b.sel]);
    own = unlock_has(u->id);
    /* the cup on a velvet cushion */
    ui_fill_ellipse(x + w / 2, y + (big ? 50 : 40), big ? 26 : 20, 5, PAL_PAINT(PAINT_BLOOD, 0));
    ui_fill_ellipse(x + w / 2, y + (big ? 49 : 39), big ? 24 : 18, 4, PAL_PAINT(PAINT_CHERRY, 1));
    cup(x + w / 2, y + (big ? 34 : 27) + (own ? ui_wave(lib_b.t, 50, 1) : 0), big ? 3 : 2, own,
        (int)(lib_b.t / 3));
    ty = y + (big ? 60 : 48);
    ui_text_shadow(x + 6, ty, lib_fit(u->name, w - 12, buf, sizeof(buf)), st->accent, st->fill_lo);
    ty += 10;
    ui_text(x + 6, ty, doomo_unlock_cat_names[u->cat % N_CATS], PAL_PAINT(cat_paint[u->cat % N_CATS], 0));
    if (own) ui_text(x + w - 6 - ui_text_width("WON!"), ty, "WON!", PAL_PAINT(PAINT_FOREST, 1));
    ty += 11;
    ty += FONT_LINE * ui_text_wrap(x + 6, ty, w - 12, u->how, st->text, 1) + 2;
    if (!own && ty + 16 <= y + h) progress_lines(x + 6, ty, w - 12, u, st);
}

static void detail(int sw, int sh)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    int x = 4, y = 18, w = sw - 8, h = sh - 18 - 15, big = lib_big();
    int cx = x + (big ? 64 : 46), rx = x + (big ? 128 : 92), rw = x + w - rx - 6, ty;
    char buf[64];
    const DoomoUnlock *u;
    int own;
    if (!lib_detail_window(x, y, w, h, WIN_CREAM) || plat_draw_skipped()) return;
    lib_b.scroll_max = 0;
    if (stats_tab()) {
        stats_panel(x + 8, y + 8, w - 16, h - 16, T.idx[lib_b.sel], st);
        lib_detail_end(x, y, w, h, WIN_CREAM);
        return;
    }
    u = unlock_at(T.idx[lib_b.sel]);
    own = unlock_has(u->id);
    /* a pedestal with a spotlight */
    ui_fill_ellipse(cx, y + h - 22, big ? 46 : 34, 8, PAL_RAMP(RAMP_PEACH, 5));
    plat_rect(cx - (big ? 30 : 22), y + h - 40, (big ? 60 : 44), 18, PAL_RAMP(RAMP_WOOD, 3));
    plat_rect(cx - (big ? 30 : 22), y + h - 40, (big ? 60 : 44), 2, PAL_RAMP(RAMP_WOOD, 5));
    plat_rect(cx - (big ? 26 : 18), y + h - 34, (big ? 52 : 36), 8, PAL_UI_GOLD);
    doomo_snprintf(buf, sizeof(buf), "#%d", u->id);
    ui_text_center(cx, y + h - 33, buf, PAL_RAMP(RAMP_WOOD, 1));
    {
        int s = big ? 5 : 4, bob = own ? ui_wave(lib_b.dt, 50, 2) : 0;
        cup(cx, y + h - 42 - 5 * s + bob, s, own, (int)(lib_b.dt / 2));
        if (own && lib_b.dt < 40) {
            const DoomoImage *img = sprite_image(SPR_CONFETTI, (lib_b.dt / 6) & 3);
            if (img) plat_image(cx - img->w / 2, y + 12, img, 1, 0, 0);
        }
    }
    ty = y + 8;
    ui_text_shadow(rx, ty, u->name, st->accent, st->fill_lo);
    ty += 11;
    ui_text(rx, ty, doomo_unlock_cat_names[u->cat % N_CATS], PAL_PAINT(cat_paint[u->cat % N_CATS], 0));
    ty += 13;
    ui_text(rx, ty, own ? "How you won it:" : "How to win it:", st->text_dim);
    ty += FONT_LINE;
    ty += FONT_LINE * ui_text_wrap(rx, ty, rw, u->how, st->text, 1) + 4;
    if (u->need && u->prog < PROG_COUNT) ty = progress_lines(rx, ty, rw, u, st);
    if (own) lib_stamp(rx + rw / 2, y + h - 14, "WON " STR_ICON_STAR, PAL_PAINT(PAINT_FOREST, 1));
    else lib_stamp(rx + rw / 2, y + h - 14, u->need ? "IN PROGRESS" : "SECRET-ISH", PAL_PAINT(PAINT_COBALT, 1));
    lib_detail_end(x, y, w, h, WIN_CREAM);
}

static void tro_draw(void)
{
    int sw = lib_sw(), sh = lib_sh(), y, lw;
    char tabn[48];
    const char *cn;
    lib_chrome(LIB_WING_TROPHIES, tro_pct);
    if (plat_draw_skipped()) return;
    y = 18;
    cn = lib_b.tab == TAB_ALL ? "All Prizes" : stats_tab() ? "Stats" : doomo_unlock_cat_names[lib_b.tab - 1];
    if (!stats_tab() && lib_b.filter % 4)
        doomo_snprintf(tabn, sizeof(tabn), "%s, %s", cn, filter_names[lib_b.filter % 4]);
    else
        doomo_snprintf(tabn, sizeof(tabn), "%s", cn);
    y += lib_tab_pill(sw / 2, y, tabn, lib_b.count, lib_b.tab, N_TABS, WIN_CREAM);
    {
        char ob[24];
        doomo_snprintf(ob, sizeof(ob), STR_ICON_CHECK "%d/%d", tro_owned, tro_total);
        ui_text_outline(6, 5, ob, PAL_WHITE, PAL_RAMP(RAMP_WOOD, 2));
    }
    lw = lib_big() ? 150 : 116;
    lib_list(4, y + 1, lw, sh - y - 16, 12, WIN_CREAM, row);
    preview(lw + 8, y + 1, sw - lw - 12, sh - y - 16);
    if (lib_b.detail) {
        plat_shade_rect(0, 0, sw, sh, 9);
        detail(sw, sh);
        lib_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R " next  " STR_ICON_B " close", WIN_CREAM);
    } else {
        lib_hint(STR_ICON_A " open  L/R kind  SEL filter  " STR_ICON_B " back", WIN_CREAM);
    }
}

const DoomoScene SCENE_LIB_TROPHIES = { "LIB_TROPHIES", tro_enter, NULL, tro_update, tro_draw, 0 };
