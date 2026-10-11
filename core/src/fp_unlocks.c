/* fp_unlocks.c - SCENE_UNLOCKS: the Unlocks gallery (owner: freeplay).
 *
 * Every unlock of every team (unlock_count / unlock_at) as a grid of
 * candy tiles: owned ones in colour with their icon (paints show their
 * swatch), locked ones as grey "?" tiles. The card underneath names the
 * unlock, its category and how to earn it, with a progress bar for the
 * automatic rules (PROG_* counter vs need). L / R switch categories; the
 * last page lists every progress counter.
 */
#include "fp_int.h"

#define CATS 10
enum { TAB_ALL = 0, TAB_PROGRESS = CATS + 1, TAB_N };
static const char *const tab_names[TAB_N] = {
    "All", "Parts", "Paints", "Weapons", "Items", "Modes", "Cheats", "Gallery",
    "Styles", "Pets", "Secrets", "Progress"
};

static struct {
    int tab, sel, top, t;
    int n, owned;
} U;

static int tab_cat(int tab) { return tab - 1; }

/* the index list lives in the shared scene scratch (scene.h) */
#define UNLOCKS_TAG SAVE_EXT_TAG('F', 'P', 'U', 'L')
static uint16_t *fp_ids;
static void build(void);
static void ensure(void)
{
    if (!fp_ids || !scene_scratch_mine(UNLOCKS_TAG)) {
        fp_ids = (uint16_t *)scene_scratch(UNLOCKS_TAG, FP_IDS_MAX * sizeof(uint16_t));
        build();
    }
}

static void build(void)
{
    int i, total = unlock_count();
    U.n = 0;
    U.owned = 0;
    for (i = 0; i < total && U.n < FP_IDS_MAX; i++) {
        const DoomoUnlock *u = unlock_at(i);
        if (!u || !u->name) continue;
        if (U.tab != TAB_ALL && u->cat != tab_cat(U.tab)) continue;
        fp_ids[U.n++] = (uint16_t)i;
        if (unlock_has(u->id)) U.owned++;
    }
    if (U.tab == TAB_PROGRESS) U.n = PROG_COUNT;
    U.sel = DOOMO_MIN(U.sel, DOOMO_MAX(U.n - 1, 0));
}

static void un_enter(int arg)
{
    (void)arg;
    memset(&U, 0, sizeof(U));
    fp_ids = NULL;
    ensure();
    fp_achieve(UL_FP_BOOKWORM);
}

static int cell(void) { return fp_big() ? 24 : 19; }
static int cols(void) { return (fp_sw() - 16) / cell(); }
static int rows(void) { return (fp_sh() - (fp_big() ? 110 : 84)) / cell(); }
static int prog_vis(void) { return fp_big() ? 15 : 9; }

static void un_update(void)
{
    int c = cols(), vis = U.tab == TAB_PROGRESS ? prog_vis() : rows();
    U.t++;
    ensure();
    if (fp_is_freeplay()) fp_live(); else fp_fx_tick();
    if (btn_pressed(BTN_L | BTN_R)) {
        U.tab = (U.tab + (btn_pressed(BTN_L) ? TAB_N - 1 : 1)) % TAB_N;
        U.sel = U.top = 0;
        build();
        audio_sfx(SFX_BLIP);
        return;
    }
    if (U.n) {
        if (U.tab == TAB_PROGRESS) {
            if (btn_repeat(BTN_UP)) { U.sel = (U.sel + U.n - 1) % U.n; audio_sfx(SFX_CURSOR); }
            if (btn_repeat(BTN_DOWN)) { U.sel = (U.sel + 1) % U.n; audio_sfx(SFX_CURSOR); }
            if (U.sel < U.top) U.top = U.sel;
            if (U.sel >= U.top + vis) U.top = U.sel - vis + 1;
        } else {
            if (btn_repeat(BTN_LEFT)) { U.sel = (U.sel + U.n - 1) % U.n; audio_sfx(SFX_CURSOR); }
            if (btn_repeat(BTN_RIGHT)) { U.sel = (U.sel + 1) % U.n; audio_sfx(SFX_CURSOR); }
            if (btn_repeat(BTN_UP) && U.sel >= c) { U.sel -= c; audio_sfx(SFX_CURSOR); }
            if (btn_repeat(BTN_DOWN) && U.sel + c < U.n) { U.sel += c; audio_sfx(SFX_CURSOR); }
            if (U.sel / c < U.top) U.top = U.sel / c;
            if (U.sel / c >= U.top + vis) U.top = U.sel / c - vis + 1;
        }
    }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop();
        else scene_set(&SCENE_TITLE, 0);
    }
}

/* ====================================================================== */
static int cat_icon(const DoomoUnlock *u)
{
    switch (u->cat) {
    case UL_CAT_PART:
        switch (u->list) {
        case CLIST_SPECIES: case CLIST_BODY: case CLIST_HEAD: return ICON_HEART;
        case CLIST_EYE: case CLIST_BROW: case CLIST_NOSE: case CLIST_MOUTH: return ICON_SMILE;
        case CLIST_HAIR: case CLIST_FHAIR: return ICON_SPARK;
        case CLIST_HORNS: case CLIST_TAIL: case CLIST_WINGS: case CLIST_HALO: return ICON_PENTAGRAM;
        case CLIST_MARK: case CLIST_PATTERN: case CLIST_AURA: return ICON_DROP;
        default: return ICON_STAR;
        }
    case UL_CAT_WEAPON_PART: return ICON_GUN;
    case UL_CAT_ITEM: return fp_item_icon(u->index);
    case UL_CAT_MODE: return ICON_STAR;
    case UL_CAT_CHEAT: return ICON_BOLT;
    case UL_CAT_GALLERY: return ICON_NOTE;
    case UL_CAT_SKIN: return ICON_SPARK;
    case UL_CAT_PET: return ICON_EGG;
    default: return ICON_CHECK;
    }
}

static uint8_t cat_color(int cat)
{
    static const uint8_t c[CATS] = {
        PP(PAINT_BUBBLEGUM, 1), PP(PAINT_LEMON, 1), PP(PAINT_SILVER, 0), PP(PAINT_TANGERINE, 1),
        PP(PAINT_LAVENDER, 1), PP(PAINT_LIME, 1), PP(PAINT_SKY, 1), PP(PAINT_MINT, 1),
        PP(PAINT_PEACH, 1), PP(PAINT_ROSE, 1)
    };
    return (cat >= 0 && cat < CATS) ? c[cat] : PAL_UI_GOLD;
}

static void tile(int x, int y, int s, const DoomoUnlock *u, int sel)
{
    int own = unlock_has(u->id), t = U.t;
    uint8_t c = own ? cat_color(u->cat) : ui_darker(ui_darker(cat_color(u->cat)));
    char g[2] = { 0, 0 };
    int b = sel ? (ui_wave(t, 20, 1) > 0) : 0;
    y -= b;
    ui_round_rect(x, y, s - 2, s - 2, 3, ui_darker(ui_darker(c)));
    ui_round_rect(x + 1, y + 1, s - 4, s - 4, 2, c);
    plat_rect(x + 3, y + 1, s - 8, 1, ui_lighter(c));
    if (own && u->cat == UL_CAT_PAINT) {
        ui_round_rect(x + 3, y + 3, s - 8, s - 8, 2, PAL_PAINT(u->index % 32, 0));
        ui_round_rect(x + 4, y + 4, s - 10, s - 10, 1, PAL_PAINT(u->index % 32, 1));
        plat_rect(x + 5, y + 5, 2, 1, PAL_PAINT(u->index % 32, 2));
    } else if (own) {
        g[0] = (char)cat_icon(u);
        plat_text(x + (s - 2) / 2 - 2, y + (s - 2) / 2 - 4, g, PAL_WHITE);
    } else {
        plat_checker(x + 1, y + 1, s - 4, s - 4, ui_darker(c));
        plat_text(x + (s - 2) / 2 - 2, y + (s - 2) / 2 - 4, "?", ui_lighter(ui_lighter(c)));
    }
    if (sel) {
        ui_frame(x - 2, y - 2, s + 2, s + 2, (t / 6) & 1 ? PAL_UI_GOLD : PAL_WHITE);
        if (own) ui_sparkle(x + s - 3, y, (t / 2) & 15, PAL_WHITE);
    }
}

static void draw_progress(int x, int y, int w)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int i, vis = prog_vis();
    char buf[24];
    for (i = 0; i < vis && U.top + i < PROG_COUNT; i++) {
        int k = U.top + i, ry = y + i * 11;
        if (k == U.sel) {
            fp_pill(x + 6, ry - 1, w - 6, 11, st->accent_d, ui_darker(st->accent_d), ui_lighter(st->accent_d));
            ui_cursor(x - 3, ry + 1);
        }
        ui_text(x + 10, ry + 1, doomo_progress_names[k] ? doomo_progress_names[k] : "?", st->text);
        doomo_snprintf(buf, sizeof(buf), "%u", (unsigned)progress_get(k));
        ui_text(x + w - ui_text_width(buf) - 4, ry + 1, buf, st->accent);
    }
    if (PROG_COUNT > vis) ui_scroll_arrows(x + w, y - 6, y + vis * 11 - 2, U.top > 0, U.top + vis < PROG_COUNT);
}

static void un_draw(void)
{
    int sw = fp_sw(), sh = fp_sh(), big = fp_big(), i, s = cell(), c = cols(), r = rows();
    const UiStyle *st = ui_style(WIN_CUTE);
    int gx = (sw - c * s) / 2, gy = 44, total = unlock_count(), own_all = unlock_owned_count();
    char buf[64];
    ensure();
    ui_backdrop(WIN_CUTE, (uint32_t)fp_t());
    doomo_snprintf(buf, sizeof(buf), "Unlocks  %d / %d", own_all, total);
    ui_title(sw / 2, 3, buf, WIN_CUTE);
    ui_bar(sw / 2 - 50, 17, 100, 5, own_all, DOOMO_MAX(total, 1), PAL_UI_GOLD);
    {
        int first = U.tab > 2 ? U.tab - 2 : 0, count = big ? 5 : 3, k;
        const char *names[6];
        if (first + count > TAB_N) first = TAB_N - count;
        for (k = 0; k < count; k++) names[k] = tab_names[first + k];
        ui_tabs(8, 27, names, count, U.tab - first, WIN_CUTE);
        if (first > 0) plat_text(1, 28, STR_ICON_ARROW_L, st->accent);
        if (first + count < TAB_N) plat_text(sw - 7, 28, STR_ICON_ARROW_R, st->accent);
    }
    if (U.tab == TAB_PROGRESS) {
        ui_window(6, gy - 4, sw - 12, prog_vis() * 11 + 8, WIN_CUTE);
        draw_progress(14, gy, sw - 30);
        ui_hint("L/R Category  " STR_ICON_B " Back", WIN_CUTE);
        return;
    }
    ui_window(gx - 6, gy - 5, c * s + 10, r * s + 8, WIN_CUTE);
    for (i = 0; i < r * c; i++) {
        int k = U.top * c + i;
        const DoomoUnlock *u;
        if (k >= U.n) break;
        u = unlock_at(fp_ids[k]);
        if (u) tile(gx + (i % c) * s, gy + (i / c) * s, s, u, k == U.sel);
    }
    if ((U.n + c - 1) / c > r)
        ui_scroll_arrows(gx + c * s, gy - 6, gy + r * s - 4, U.top > 0, (U.top + r) * c < U.n);
    /* the card */
    {
        int cy = gy + r * s + 6, ch = sh - cy - 16;
        ui_window(6, cy, sw - 12, ch, WIN_CREAM);
        if (U.n) {
            const DoomoUnlock *u = unlock_at(fp_ids[U.sel]);
            const UiStyle *cs = ui_style(WIN_CREAM);
            if (u) {
                int own = unlock_has(u->id);
                doomo_snprintf(buf, sizeof(buf), "%s  (%s)", own ? u->name : "???",
                               u->cat < 10 ? doomo_unlock_cat_names[u->cat] : "?");
                fp_marquee(12, cy + 5, sw - 24, buf, own ? cs->accent : cs->text_dim, U.t);
                if (own) {
                    ui_text(12, cy + 15, STR_ICON_CHECK " Unlocked!", PP(PAINT_FOREST, 1));
                } else {
                    doomo_snprintf(buf, sizeof(buf), "How: %s", u->how ? u->how : "a secret...");
                    fp_marquee(12, cy + 15, sw - 24, buf, cs->text, U.t);
                    if (u->need && u->prog < PROG_COUNT && ch > 30) {
                        uint32_t have = progress_get(u->prog);
                        doomo_snprintf(buf, sizeof(buf), "%u / %u", (unsigned)DOOMO_MIN(have, u->need), (unsigned)u->need);
                        ui_text(12, cy + 25, buf, cs->text_dim);
                        ui_bar(18 + ui_text_width(buf), cy + 26, sw - 40 - ui_text_width(buf), 6,
                               (int)DOOMO_MIN(have, u->need), (int)u->need, PAL_UI_GOLD);
                    }
                }
            }
        } else {
            ui_text(12, cy + 6, "Nothing here yet!", st->text_dim);
        }
    }
    doomo_snprintf(buf, sizeof(buf), "%d/%d here  L/R Category  " STR_ICON_B " Back", U.owned, U.n);
    ui_hint(buf, WIN_CUTE);
    fp_fx_draw();
}

const DoomoScene SCENE_UNLOCKS = { "UNLOCKS", un_enter, NULL, un_update, un_draw, 0 };
