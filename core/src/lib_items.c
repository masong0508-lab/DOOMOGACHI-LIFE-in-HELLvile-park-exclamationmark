/* lib_items.c - SCENE_LIB_ITEMS: the Food & Item Dex (owner: library).
 *
 * Every item in HaHaland by category (L/R), filtered by what is in your
 * bag (SELECT): its icon on a little pedestal, description, price, what
 * it does to a citizen (stat chips) and Quillby's note. Foods also show
 * how many citizens love / hate them and their biggest fan. The "Tastes"
 * shelf lists all 48 foods the citizens can be fond of (most of them only
 * exist in their dreams), sorted by love, hate or name.
 *
 * Completion = how many kinds of items are in your bag right now; having
 * every food at once earns the Gourmet trophy. Love / hate counts are
 * tallied once on entry (one pass over the 1000 slots).
 */
#include "lib_int.h"

#define I (lib_w.item)

enum { SH_ALL, SH_FOOD, SH_GIFT, SH_CARE, SH_EXO, SH_MAT, SH_KEY, SH_TASTES, SH_COUNT };
static const char *const shelf_names[SH_COUNT] = {
    "All Items", "Food", "Gifts", "Care", "Exorcism", "Materials", "Key Items", "Tastes"
};
static const char *const bag_filters[3] = { "", "in bag", "not in bag" };
static const char *const taste_sorts[3] = { "most loved", "most hated", "A-Z" };
static const uint8_t cat_paint[6] = { PAINT_TANGERINE, PAINT_ROSE, PAINT_SKY, PAINT_HONEY,
                                      PAINT_RUST, PAINT_GRAPE };
static const char *const cat_names[6] = { "Food", "Gift", "Care", "Exorcism", "Material", "Key item" };

/* Quillby's notes (by ITEM_*; items added later just have none). */
static const char *const notes[] = {
    "",
    "The official snack of HaHaland. The frosting is load-bearing.",
    "A rice triangle of comfort. The plum inside is a secret.",
    "Tastes like nostalgia. Please do not ask whose.",
    "Crunchy, smoky and slightly on fire. A Hellvile breakfast.",
    "They blink if you stare. So don't stare.",
    "Wobbly perfection. Nana Puddin's pride and joy.",
    "Spicy enough to make an Impling cry for its mum.",
    "So light it floats. Weigh it down with cream.",
    "Hellvile's idea of a balanced diet.",
    "Leaves! Crunchy, green and quietly judging you.",
    "Builds strong bones. The skeletons drink gallons.",
    "Liquid motivation. The night shift runs on it.",
    "Hug-rated for all ages and all species.",
    "Say it with petals. Hellvile says it with thorns.",
    "Handle with care: contains feelings.",
    "Hours of fun, minutes of sleep.",
    "A tiny skull. Hellvile folk find it adorable.",
    "Eau de Cupcake. Also available in Brimstone.",
    "Friendship-tested. Friendship-ending.",
    "A big question in a tiny box.",
    "Bubbles away grime, gloom and goo.",
    "Tastes like cherries and regret.",
    "Zoom! Then crash. Then zoom again.",
    "For when somebody needs to calm down. You know who.",
    "Blessed at St. Cuddles. Also great for houseplants.",
    "Smells like a forest giving you advice.",
    "A soft cross. Demons hate how cosy it is.",
    "Count prayers, count beads, count on it.",
    "Draw a line: demons can't cross it. Neither can snails.",
    "Sing it loud enough and the walls shake. So do demons.",
    "One imp's junk is Rivet's treasure.",
    "Clicky little bits that make things go.",
    "Shed by a demon. It would like it back.",
    "Impossible to clean up. Totally worth it.",
    "Forged in the Gate's fire. It rings like a scream.",
    "The fluffy insides of courage.",
    "Melts into everything Rivet makes.",
    "Rivet's scribbles. Mostly genius, partly doodles.",
    "Home is wherever this fits.",
    "Warm, wobbly and about to change your life.",
    "Mr. Flops! Pip's best friend. Slightly chewed.",
    "Confiscated. Smells of Unholy Grape. Ew.",
    "Opens the Vape Cellar. Maybe... don't?",
    "Hums with the voices of the Gate. Shhh.",
};

static int item_ok(int it)
{
    return it > ITEM_NONE && it < ITEM_COUNT && doomo_items[it].name && doomo_items[it].name[0];
}

/* The food (doomo_food_names) an item is, or -1 (same rule as sim_feed). */
static int food_of(int it)
{
    int f;
    if (!item_ok(it) || doomo_items[it].category != ICAT_FOOD) return -1;
    f = it - ITEM_CUPCAKE;
    return f >= 0 && f < FOOD_COUNT ? f : -1;
}

/* ---- completion -------------------------------------------------------------- */
int lib_items_pct(void)
{
    int it, n = 0, have = 0, foods = 0, food_have = 0;
    for (it = 1; it < ITEM_COUNT; it++) {
        if (!item_ok(it)) continue;
        n++;
        if (inv_count(it) > 0) have++;
        if (doomo_items[it].category == ICAT_FOOD) {
            foods++;
            if (inv_count(it) > 0) food_have++;
        }
    }
    if (foods > 0 && food_have == foods) unlock_grant(LIB_UL_GOURMET);
    lib_have = (int16_t)have;
    lib_total = (int16_t)n;
    return lib_pct(have, n);
}

/* ---- the view ------------------------------------------------------------------ */
static int tastes(void) { return lib_b.tab == SH_TASTES; }

static int taste_cmp(int a, int b)
{
    int d;
    switch (lib_b.filter % 3) {
    case 0: d = (int)I.love[b] - (int)I.love[a]; break;
    case 1: d = (int)I.hate[b] - (int)I.hate[a]; break;
    default: d = 0; break;
    }
    if (!d) d = strcmp(doomo_food_names[a], doomo_food_names[b]);
    return d;
}

static void rebuild(void)
{
    int n = 0, i, j;
    if (tastes()) {
        for (i = 0; i < FOOD_COUNT; i++) I.idx[n++] = (uint8_t)i;
        for (i = 1; i < n; i++) {                       /* 48 entries: insertion */
            uint8_t v = I.idx[i];
            for (j = i; j > 0 && taste_cmp(I.idx[j - 1], v) > 0; j--) I.idx[j] = I.idx[j - 1];
            I.idx[j] = v;
        }
    } else {
        for (i = 1; i < ITEM_COUNT && n < (int)sizeof(I.idx); i++) {
            int have;
            if (!item_ok(i)) continue;
            if (lib_b.tab != SH_ALL && doomo_items[i].category != lib_b.tab - 1) continue;
            have = inv_count(i) > 0;
            if ((lib_b.filter % 3) == 1 && !have) continue;
            if ((lib_b.filter % 3) == 2 && have) continue;
            I.idx[n++] = (uint8_t)i;
        }
    }
    lib_b.count = (int16_t)n;
    lib_clamp();
}

static void tally(void)
{
    int id;
    memset(I.love, 0, sizeof(I.love));
    memset(I.hate, 0, sizeof(I.hate));
    for (id = 0; id < FOOD_COUNT; id++) I.fan[id] = -1;
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        const DoomoResident *r = &doomo_town.res[id];
        int f;
        if (r->status == RES_EMPTY || r->status == RES_DEAD) continue;
        f = r->like_food % FOOD_COUNT;
        I.love[f]++;
        if (I.fan[f] < 0 || r->level > doomo_town.res[I.fan[f]].level) I.fan[f] = (int16_t)id;
        I.hate[r->hate_food % FOOD_COUNT]++;
    }
}

/* ---- scene -------------------------------------------------------------------- */
static uint8_t items_pct;

static void items_enter(int arg)
{
    lib_wing_enter(LIB_WING_ITEMS, arg);
    if (lib_b.tab >= SH_COUNT) lib_b.tab = 0;
    tally();
    I.club_for = 0;
    rebuild();
    items_pct = (uint8_t)lib_items_pct();
}

static void items_update(void)
{
    if (lib_w_lost(1)) return;
    switch (lib_nav(SH_COUNT, 0)) {
    case LIBE_TAB: case LIBE_FILTER: rebuild(); break;
    case LIBE_BACK: lib_wing_leave(); break;
    case LIBE_OPEN: case LIBE_FLIP:
        if (!tastes() && doomo_items[I.idx[lib_b.sel]].category == ICAT_FOOD) audio_sfx(SFX_EAT);
        break;
    default: break;
    }
}

/* ---- drawing -------------------------------------------------------------------- */
static void glyph(int x, int y, int icon, uint8_t c)
{
    char g[2];
    g[0] = (char)icon;
    g[1] = 0;
    plat_text(x, y, g, c);
}

static void row(int i, int x, int y, int w, int sel)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    char buf[40];
    (void)sel;
    if (tastes()) {
        int f = I.idx[i];
        int v = (lib_b.filter % 3) == 1 ? I.hate[f] : I.love[f];
        glyph(x, y + 2, (lib_b.filter % 3) == 1 ? ICON_FROWN : ICON_HEART,
              (lib_b.filter % 3) == 1 ? PAL_PAINT(PAINT_GRAPE, 1) : PAL_PAINT(PAINT_ROSE, 1));
        ui_text(x + 9, y + 2, lib_fit(doomo_food_names[f], w - 30, buf, sizeof(buf)), st->text);
        doomo_snprintf(buf, sizeof(buf), "%d", v);
        ui_text(x + w - ui_text_width(buf), y + 2, buf, st->text_dim);
    } else {
        int it = I.idx[i], n = inv_count(it);
        const DoomoItemInfo *in = &doomo_items[it];
        glyph(x, y + 2, in->icon, PAL_PAINT(cat_paint[in->category % 6], 1));
        ui_text(x + 9, y + 2, lib_fit(in->name, w - (n ? 34 : 12), buf, sizeof(buf)), n ? st->text : st->text_dim);
        if (n) {
            doomo_snprintf(buf, sizeof(buf), "x%d", n);
            ui_text(x + w - ui_text_width(buf), y + 2, buf, st->accent);
        }
    }
}

/* The item on a little pedestal: plate for food, present for gifts,
 * cushion for everything else. */
static void pedestal(int cx, int cy, int it, int big)
{
    const DoomoItemInfo *in = &doomo_items[it];
    uint8_t p = cat_paint[in->category % 6];
    int bob = ui_wave(lib_b.t, 40, 2), r = big ? 16 : 13;
    char g[2];
    g[0] = (char)in->icon;
    g[1] = 0;
    if (in->category == ICAT_FOOD) {
        ui_fill_ellipse(cx, cy + r - 2, r + 6, 5, PAL_RAMP(RAMP_STONE, 4));
        ui_fill_ellipse(cx, cy + r - 3, r + 5, 4, PAL_WHITE);
        ui_fill_ellipse(cx, cy + r - 3, r, 2, PAL_RAMP(RAMP_STONE, 6));
    } else if (in->category == ICAT_GIFT) {
        ui_round_rect(cx - r, cy + 2, 2 * r, r - 2, 2, PAL_PAINT(p, 0));
        ui_round_rect(cx - r + 1, cy + 3, 2 * r - 2, r - 4, 1, PAL_PAINT(p, 1));
        plat_rect(cx - 2, cy + 2, 4, r - 2, PAL_UI_GOLD);
    } else {
        ui_fill_ellipse(cx, cy + r - 2, r + 4, 5, PAL_PAINT(p, 0));
        ui_fill_ellipse(cx, cy + r - 3, r + 3, 4, PAL_PAINT(p, 1));
    }
    ui_fill_circle(cx, cy - 2 - bob, r - 2, PAL_PAINT(p, 0));
    ui_fill_circle(cx, cy - 3 - bob, r - 3, PAL_PAINT(p, 1));
    plat_rect(cx - r / 2, cy - r + 1 - bob, r / 2, 2, PAL_PAINT(p, 2));
    ui_text_big(cx - 5 * (big ? 3 : 2) / 2, cy - 3 - bob - 4 * (big ? 3 : 2), g, big ? 3 : 2, PAL_WHITE, PAL_PAINT(p, 0));
    ui_sparkle(cx + r, cy - r - bob, (lib_b.t / 3) & 15, PAL_WHITE);
}

static int effect_chips(int x, int y, int w, int bottom, int it)
{
    static const char *const nm[7] = { "Food", "Joy", "Clean", "Energy", "Health", "Sanity", "Corrupt" };
    const DoomoItemInfo *in = &doomo_items[it];
    int8_t v[7];
    int k, cx = x, cy = y, any = 0;
    v[0] = in->hunger; v[1] = in->happy; v[2] = in->hygiene; v[3] = in->energy;
    v[4] = in->health; v[5] = in->sanity; v[6] = in->corruption;
    for (k = 0; k < 7; k++) {
        char b[20];
        int cw, good;
        uint8_t c;
        if (!v[k]) continue;
        good = k == 6 ? v[k] < 0 : v[k] > 0;
        c = good ? PAL_PAINT(PAINT_FOREST, 1) : PAL_PAINT(PAINT_BLOOD, 1);
        doomo_snprintf(b, sizeof(b), "%s%c%d", nm[k], v[k] > 0 ? '+' : '-', v[k] > 0 ? v[k] : -v[k]);
        cw = ui_text_width(b) + 6;
        if (cx + cw > x + w) { cx = x; cy += 11; }
        if (cy + 10 > bottom) break;
        ui_round_rect(cx, cy, cw, 10, 3, c);
        ui_text(cx + 3, cy + 1, b, PAL_WHITE);
        cx += cw + 2;
        any = 1;
    }
    if (in->holy) {
        char b[20];
        int cw;
        doomo_snprintf(b, sizeof(b), "Holy %d", in->holy);
        cw = ui_text_width(b) + 6;
        if (cx + cw > x + w) { cx = x; cy += 11; }
        if (cy + 10 > bottom) return cy;
        ui_round_rect(cx, cy, cw, 10, 3, PAL_UI_GOLD);
        ui_text(cx + 3, cy + 1, b, PAL_PAINT(PAINT_RUST, 0));
        any = 1;
    }
    return any ? cy + 11 : y;
}

/* Up to four citizens who love food f (scanned once per page). */
static void fan_club(int f)
{
    int id;
    if (I.club_for == f + 1) return;
    I.club_for = (int16_t)(f + 1);
    I.club_n = 0;
    for (id = 0; id < DOOMO_MAX_RESIDENTS && I.club_n < 4; id++) {
        const DoomoResident *r = &doomo_town.res[id];
        if (r->status != RES_EMPTY && r->status != RES_DEAD && r->like_food % FOOD_COUNT == f)
            I.club[I.club_n++] = (int16_t)id;
    }
}

static int club_lines(int x, int y, int w, int bottom, int f, const UiStyle *st)
{
    char buf[96];
    int i, n = 0;
    fan_club(f);
    if (!I.club_n || y + 2 * FONT_LINE > bottom) return y;
    n = doomo_snprintf(buf, sizeof(buf), "Fan club: ");
    for (i = 0; i < I.club_n && n < (int)sizeof(buf) - 16; i++)
        n += doomo_snprintf(buf + n, (int)sizeof(buf) - n, "%s%s", i ? ", " : "", doomo_town.res[I.club[i]].name);
    if (I.love[f] > I.club_n) doomo_snprintf(buf + n, (int)sizeof(buf) - n, " +%d", I.love[f] - I.club_n);
    return y + FONT_LINE * ui_text_wrap(x, y, w, buf, st->text_dim, 1);
}

static void taste_panel(int x, int y, int w, int h, int f, int detail)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    char buf[64];
    int ly = y, maxv = 1, i;
    ui_text_shadow(x, ly, doomo_food_names[f], st->accent, st->fill_lo);
    ly += 12;
    doomo_snprintf(buf, sizeof(buf), STR_ICON_HEART " Loved by %d", I.love[f]);
    ui_text(x, ly, buf, PAL_PAINT(PAINT_ROSE, 1));
    ly += FONT_LINE;
    for (i = 0; i < FOOD_COUNT; i++) maxv = DOOMO_MAX(maxv, DOOMO_MAX(I.love[i], I.hate[i]));
    ui_bar(x, ly, w, 6, I.love[f], maxv, PAL_PAINT(PAINT_ROSE, 1));
    ly += 9;
    doomo_snprintf(buf, sizeof(buf), STR_ICON_FROWN " Hated by %d", I.hate[f]);
    ui_text(x, ly, buf, PAL_PAINT(PAINT_GRAPE, 1));
    ly += FONT_LINE;
    ui_bar(x, ly, w, 6, I.hate[f], maxv, PAL_PAINT(PAINT_GRAPE, 1));
    ly += 10;
    if (I.fan[f] >= 0) {
        doomo_snprintf(buf, sizeof(buf), "Biggest fan: %s", doomo_town.res[I.fan[f]].name);
        ly += FONT_LINE * ui_text_wrap(x, ly, w, buf, st->text, 1);
    }
    if (detail) ly = club_lines(x, ly, w, y + h - FONT_LINE, f, st);
    if (ly + FONT_LINE <= y + h)
        ui_text_wrap(x, ly, w, f < ITEM_HOUSE_KEY && food_of(f + ITEM_CUPCAKE) == f ?
                     "On the menu at Brimstone Bites." : "Only found in dreams and daydreams.",
                     st->text_dim, 1);
}

static void preview(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int it, big = lib_big(), ty, f;
    char buf[64];
    const DoomoItemInfo *in;
    ui_window(x, y, w, h, WIN_CUTE);
    if (lib_b.count <= 0 || plat_draw_skipped()) return;
    if (tastes()) { taste_panel(x + 6, y + 5, w - 12, h - 10, I.idx[lib_b.sel], 0); return; }
    it = I.idx[lib_b.sel];
    in = &doomo_items[it];
    pedestal(x + w / 2, y + (big ? 30 : 24), it, big);
    ty = y + (big ? 56 : 44);
    ui_text_shadow(x + 6, ty, lib_fit(in->name, w - 12, buf, sizeof(buf)), st->accent, st->fill_lo);
    if (in->price) {
        doomo_snprintf(buf, sizeof(buf), STR_ICON_COIN "%d", in->price);
        ui_text(x + w - 6 - ui_text_width(buf), ty, buf, PAL_RAMP(RAMP_CANDY, 2));
    }
    ty += 10;
    ty += FONT_LINE * ui_text_wrap(x + 6, ty, w - 12, in->desc, st->text, 1);
    ty = effect_chips(x + 6, ty + 2, w - 12, y + h - 4, it);
    f = food_of(it);
    if (f >= 0 && ty + FONT_LINE <= y + h - 4) {
        doomo_snprintf(buf, sizeof(buf), STR_ICON_HEART "%d  " STR_ICON_FROWN "%d", I.love[f], I.hate[f]);
        ui_text(x + 6, ty + 1, buf, st->text_dim);
    }
}

static void detail(int sw, int sh)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int x = 4, y = 18, w = sw - 8, h = sh - 18 - 15, big = lib_big();
    int px = x + (big ? 44 : 34), rx = x + (big ? 92 : 72), rw = x + w - rx - 6, ty;
    char buf[80];
    if (!lib_detail_window(x, y, w, h, WIN_CUTE) || plat_draw_skipped()) return;
    if (tastes()) {
        int f = I.idx[lib_b.sel], pw = big ? 90 : 64, it = f + ITEM_CUPCAKE;
        lib_b.scroll_max = 0;
        taste_panel(x + 8, y + 6, w - 16 - pw, h - 12, f, 1);
        if (food_of(it) == f) {
            pedestal(x + w - pw / 2 - 6, y + h / 2 - 4, it, big);
        } else {
            /* a dream food: a thought cloud with a question mark */
            int cx = x + w - pw / 2 - 6, cy = y + h / 2 - 8, b = ui_wave((int)lib_b.t, 40, 2);
            ui_fill_ellipse(cx, cy - b, big ? 30 : 22, big ? 18 : 14, PAL_RAMP(RAMP_PASTEL_PINK, 4));
            ui_fill_ellipse(cx, cy - 1 - b, big ? 28 : 20, big ? 16 : 12, PAL_WHITE);
            ui_fill_circle(cx - 14, cy + 18 - b, 4, PAL_WHITE);
            ui_fill_circle(cx - 20, cy + 26 - b, 2, PAL_WHITE);
            ui_text_big(cx - 5 * (big ? 3 : 2) / 2, cy - 4 * (big ? 3 : 2) - b, "?", big ? 3 : 2,
                        PAL_PAINT(PAINT_ROSE, 1), PAL_WHITE);
            ui_text_center(cx, cy + (big ? 24 : 20), "dream food", PAL_RAMP(RAMP_PASTEL_PINK, 2));
        }
        lib_detail_end(x, y, w, h, WIN_CUTE);
        return;
    }
    {
        int it = I.idx[lib_b.sel], f = food_of(it), lines;
        const DoomoItemInfo *in = &doomo_items[it];
        pedestal(px, y + h / 2 - 6, it, big);
        doomo_snprintf(buf, sizeof(buf), "%s", cat_names[in->category % 6]);
        ui_text_center(px, y + h / 2 + (big ? 22 : 18), buf, PAL_PAINT(cat_paint[in->category % 6], 0));
        doomo_snprintf(buf, sizeof(buf), "In bag: %d", inv_count(it));
        ui_text_center(px, y + h / 2 + (big ? 32 : 28), buf, st->text_dim);
        ty = y + 6;
        ui_text_shadow(rx, ty, in->name, st->accent, st->fill_lo);
        if (in->price) {
            doomo_snprintf(buf, sizeof(buf), STR_ICON_COIN " %d H$", in->price);
            ui_text(rx + rw - ui_text_width(buf), ty, buf, PAL_RAMP(RAMP_CANDY, 2));
        } else {
            ui_text(rx + rw - ui_text_width("Not for sale"), ty, "Not for sale", st->text_dim);
        }
        ty += 12;
        ty += FONT_LINE * ui_text_wrap(rx, ty, rw, in->desc, st->text, 1);
        ty = effect_chips(rx, ty + 2, rw, y + h - 4, it) + 2;
        if (f >= 0) {
            doomo_snprintf(buf, sizeof(buf), STR_ICON_HEART " %d love it  " STR_ICON_FROWN " %d hate it",
                           I.love[f], I.hate[f]);
            ui_text(rx, ty, lib_fit(buf, rw, buf, sizeof(buf)), st->text);
            ty += FONT_LINE;
            if (big) ty = club_lines(rx, ty, rw, y + h - 3 * FONT_LINE, f, st);
            else if (I.fan[f] >= 0) {
                doomo_snprintf(buf, sizeof(buf), "Biggest fan: %s", doomo_town.res[I.fan[f]].name);
                ui_text(rx, ty, buf, st->text_dim);
                ty += FONT_LINE;
            }
        }
        lines = (y + h - 6 - ty) / FONT_LINE;
        if (lines > 0 && it < (int)DOOMO_COUNTOF(notes) && notes[it][0]) {
            char nb[96];
            doomo_snprintf(nb, sizeof(nb), "Quillby: %s", notes[it]);
            lib_text_block(rx, ty + 2, rw - 6, lines, nb, PAL_PAINT(PAINT_RUST, 1), WIN_CUTE);
        } else {
            lib_b.scroll_max = 0;
        }
    }
    lib_detail_end(x, y, w, h, WIN_CUTE);
}

static void items_draw(void)
{
    int sw = lib_sw(), sh = lib_sh(), y, lw;
    char tabn[40];
    if (lib_w_lost(0)) return;
    lib_chrome(LIB_WING_ITEMS, items_pct);
    if (plat_draw_skipped()) return;
    y = 18;
    if (tastes())
        doomo_snprintf(tabn, sizeof(tabn), "%s, %s", shelf_names[lib_b.tab], taste_sorts[lib_b.filter % 3]);
    else if (lib_b.filter % 3)
        doomo_snprintf(tabn, sizeof(tabn), "%s, %s", shelf_names[lib_b.tab], bag_filters[lib_b.filter % 3]);
    else
        doomo_snprintf(tabn, sizeof(tabn), "%s", shelf_names[lib_b.tab]);
    y += lib_tab_pill(sw / 2, y, tabn, lib_b.count, lib_b.tab, SH_COUNT, WIN_CUTE);
    lw = lib_big() ? 140 : 110;
    lib_list(4, y + 1, lw, sh - y - 16, 12, WIN_CUTE, row);
    preview(lw + 8, y + 1, sw - lw - 12, sh - y - 16);
    if (lib_b.detail) {
        lib_dim_for_detail();
        detail(sw, sh);
        lib_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R " next  " STR_ICON_ARROW_U STR_ICON_ARROW_D " note  " STR_ICON_B " close", WIN_CUTE);
    } else {
        lib_hint(STR_ICON_A " open  L/R shelf  SEL filter  " STR_ICON_B " back", WIN_CUTE);
    }
}

const DoomoScene SCENE_LIB_ITEMS = { "LIB_ITEMS", items_enter, NULL, items_update, items_draw, 0 };
