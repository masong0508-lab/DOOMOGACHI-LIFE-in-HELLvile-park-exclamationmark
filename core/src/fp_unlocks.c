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
/* (S2-6) the grid panel's key on the last frame drawn, after the ids in the scratch (no RAM of its own: any value
 * is safe, it only says whether this frame surely draws the grid whole) */
#define GRID_KEY (*(uint32_t *)(void *)(fp_ids + FP_IDS_MAX))
static void build(void);
static void ensure(void)
{
    if (!fp_ids || !scene_scratch_mine(UNLOCKS_TAG)) {
        fp_ids = (uint16_t *)scene_scratch(UNLOCKS_TAG, FP_IDS_MAX * sizeof(uint16_t) + (UI_PANELS ? 4 : 0));
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

/* (S2-6 speed) a tile's colours, looked up once a frame per category and state (ui_darker / ui_lighter are table
 * walks, and a page of tiles asked them ~500 times a frame) */
typedef struct { uint8_t ok, c, edge, hi, ink, lock; } TileInk;
#define INKS ((CATS + 1) * 2)

static const TileInk *tile_ink(TileInk *memo, int cat, int own)
{
    TileInk *k = &memo[(cat >= 0 && cat < CATS ? cat : CATS) * 2 + (own != 0)];
    if (!k->ok) {
        uint8_t c = own ? cat_color(cat) : ui_darker(ui_darker(cat_color(cat)));
        k->ok = 1;
        k->c = c;
        k->edge = ui_darker(ui_darker(c));
        k->hi = ui_lighter(c);
        k->ink = ui_darker(c);
        k->lock = ui_lighter(ui_lighter(c));
    }
    return k;
}

/* plat_rect cut in two at column xr (the same pixels: the grid panel's right edge, so the cursor's sparkle on the
 * last column touches the panel only inside its hole) */
static void cut_rect(int x, int y, int w, int h, uint8_t c, int xr)
{
    if (x < xr && x + w > xr) {
        plat_rect(x, y, xr - x, h, c);
        plat_rect(xr, y, x + w - xr, h, c);
    } else {
        plat_rect(x, y, w, h, c);
    }
}

/* ui_sparkle(cx, cy, phase, c) with its rects cut at column xr */
static void sparkle_cut(int cx, int cy, int phase, uint8_t c, int xr)
{
    int s = ui_sparkle_size(phase);
    if (s == 0 || plat_draw_skipped()) return;
    cut_rect(cx - s, cy, 2 * s + 1, 1, c, xr);
    cut_rect(cx, cy - s, 1, 2 * s + 1, c, xr);
    if (s >= 2) {
        cut_rect(cx - 1, cy - 1, 3, 3, c, xr);
        plat_rect(cx, cy, 1, 1, PAL_WHITE);
    }
    if (s >= 3) {
        plat_rect(cx - 2, cy - 2, 1, 1, c);
        plat_rect(cx + 2, cy + 2, 1, 1, c);
        plat_rect(cx - 2, cy + 2, 1, 1, c);
        plat_rect(cx + 2, cy - 2, 1, 1, c);
    }
}

/* xr: the grid's right edge (the cursor's sparkle is cut there) */
static void tile(int x, int y, int s, const DoomoUnlock *u, int sel, TileInk *memo, int xr)
{
    int own = unlock_has(u->id), t = U.t;
    const TileInk *k = tile_ink(memo, u->cat, own);
    uint8_t c = k->c;
    char g[2] = { 0, 0 };
    int b = sel ? (ui_wave(t, 20, 1) > 0) : 0;
    y -= b;
    ui_round_rect(x, y, s - 2, s - 2, 3, k->edge);
    ui_round_rect(x + 1, y + 1, s - 4, s - 4, 2, c);
    plat_rect(x + 3, y + 1, s - 8, 1, k->hi);
    if (own && u->cat == UL_CAT_PAINT) {
        ui_round_rect(x + 3, y + 3, s - 8, s - 8, 2, PAL_PAINT(u->index % 32, 0));
        ui_round_rect(x + 4, y + 4, s - 10, s - 10, 1, PAL_PAINT(u->index % 32, 1));
        plat_rect(x + 5, y + 5, 2, 1, PAL_PAINT(u->index % 32, 2));
    } else if (own) {
        g[0] = (char)cat_icon(u);
        plat_text(x + (s - 2) / 2 - 2, y + (s - 2) / 2 - 4, g, PAL_WHITE);
    } else {
        /* a dim silhouette of what it is, and a tiny padlock */
        plat_checker(x + 1, y + 1, s - 4, s - 4, k->ink);
        g[0] = (char)cat_icon(u);
        plat_text(x + (s - 2) / 2 - 2, y + (s - 2) / 2 - 4, g, k->hi);
        plat_rect(x + s - 7, y + s - 7, 4, 3, k->lock);
        plat_rect(x + s - 6, y + s - 9, 2, 2, k->lock);
    }
    if (sel) {
        ui_frame(x - 2, y - 2, s + 2, s + 2, (t / 6) & 1 ? PAL_UI_GOLD : PAL_WHITE);
        if (own) sparkle_cut(x + s - 3, y, (t / 2) & 15, PAL_WHITE, xr);
    }
}

/* ---- (S2-6) the grid as a retained panel (platform.h plat_panel_begin; the GBA) ----------------------------------
 * The window is drawn whole on every frame as before; the tiles of every row but the cursor's are the panel (put
 * back whole: plat_panel_solid, the window under it is the same on every frame), and the cursor's row is its hole,
 * drawn again on every frame: the cursor tile bobs, blinks and sparkles, and moves along its row without a new
 * capture. The hole reaches 4 rows above the cursor's row (the bob, the blink frame and the sparkle), so the 2 last
 * rows of the tiles above are drawn again in it too, row-clipped (tile_rows). The key: the page (tab, scroll, row
 * of the cursor, layout), the ids on it and what is owned (unlock_owned_count: nothing is ever taken away). */
#define PANEL_FP_UL_GRID PANEL_OW_OBJECTIVE      /* the overworld's: never on this screen */

#if UI_PANELS
static void clip_rect(int x, int y, int w, int h, uint8_t c, int y0, int y1)
{
    int a = DOOMO_MAX(y, y0), b = DOOMO_MIN(y + h, y1);
    if (a < b) plat_rect(x, a, w, b - a, c);
}

/* rows [y0, y1) of tile(x, y, s, u, 0): only its last rows are ever asked (the glyph is far above them) */
static void tile_rows(int x, int y, int s, const DoomoUnlock *u, TileInk *memo, int y0, int y1)
{
    int own = unlock_has(u->id), j;
    const TileInk *k = tile_ink(memo, u->cat, own);
    ui_round_rect_rows(x, y, s - 2, s - 2, 3, k->edge, y0 - y, y1 - y);
    ui_round_rect_rows(x + 1, y + 1, s - 4, s - 4, 2, k->c, y0 - y - 1, y1 - y - 1);
    clip_rect(x + 3, y + 1, s - 8, 1, k->hi, y0, y1);
    if (own && u->cat == UL_CAT_PAINT) {
        ui_round_rect_rows(x + 3, y + 3, s - 8, s - 8, 2, PAL_PAINT(u->index % 32, 0), y0 - y - 3, y1 - y - 3);
        ui_round_rect_rows(x + 4, y + 4, s - 10, s - 10, 1, PAL_PAINT(u->index % 32, 1), y0 - y - 4, y1 - y - 4);
        clip_rect(x + 5, y + 5, 2, 1, PAL_PAINT(u->index % 32, 2), y0, y1);
    } else if (!own) {
        /* plat_checker's parity: row j of the rect starts at column j & 1 */
        for (j = DOOMO_MAX(y0, y + 1); j < DOOMO_MIN(y1, y + s - 3); j++) {
            int d = (j - (y + 1)) & 1;
            plat_checker(x + 1 + d, j, s - 4 - d, 1, k->ink);
        }
        clip_rect(x + s - 7, y + s - 7, 4, 3, k->lock, y0, y1);
        clip_rect(x + s - 6, y + s - 9, 2, 2, k->lock, y0, y1);
    }
}

#endif

/* the tiles of visible rows [r0, r1) (the cursor's tile as such only when sel) */
static void tiles(int gx, int gy, int s, int c, int r0, int r1, int sel, TileInk *memo)
{
    int i;
    for (i = r0 * c; i < r1 * c; i++) {
        int k = U.top * c + i;
        const DoomoUnlock *u;
        if (k >= U.n) break;
        u = unlock_at(fp_ids[k]);
        if (u) tile(gx + (i % c) * s, gy + (i / c) * s, s, u, sel && k == U.sel, memo, gx + c * s);
    }
}

#if UI_PANELS
/* the panel's hole: the last rows of the tiles above the cursor's row, then that row */
static void draw_hole(int gx, int gy, int s, int c, int sr, int hy0, int hy1, TileInk *memo)
{
    int i;
    if (sr > 0)
        for (i = (sr - 1) * c; i < sr * c; i++) {
            int k = U.top * c + i;
            const DoomoUnlock *u = k < U.n ? unlock_at(fp_ids[k]) : NULL;
            if (u) tile_rows(gx + (i % c) * s, gy + (sr - 1) * s, s, u, memo, hy0, hy1);
        }
    tiles(gx, gy, s, c, sr, sr + 1, 1, memo);
}
#endif

static void draw_grid(int gx, int gy, int s, int c, int r, int own_all)
{
    TileInk memo[INKS];
#if UI_PANELS
    int sr = U.n ? U.sel / c - U.top : -1, hy0, hy1;
    uint32_t key;
#endif
    memset(memo, 0, sizeof(memo));
#if !UI_PANELS
    (void)own_all;
    tiles(gx, gy, s, c, 0, r, 1, memo);
#else
    if (sr < 0 || sr >= r) {
        tiles(gx, gy, s, c, 0, r, 1, memo);
        return;
    }
    hy0 = gy + sr * s - 4;           /* the first row: over the window's gloss (in the hole, so the glint can slide) */
    hy1 = gy + sr * s + s;
    key = ui_key_int(ui_panel_key(), U.tab | U.top << 8 | sr << 20 | fp_big() << 28);
    key = ui_key_int(key, U.n | own_all << 16);
    key = ui_key_int(key, gx | gy << 8 | s << 16 | c << 24);
    key = ui_key_int(key, r | fp_sw() << 8 | fp_sh() << 20);
    /* Everything is drawn before the panel when it can: a kept frame leaves what is drawn in the hole before the
     * panel, a solid panel keeps its whole rect as it is at plat_panel_end, however it got there, and the GBA draws
     * before a panel on its fast path (once a panel is up, every 2D call is checked against it). A new key is
     * never kept, so its frame draws the whole grid first; with the same key the platform may still say draw
     * (a spoiled or skipped frame): then the other rows are drawn inside the panel and the hole again after it. */
    if (key != GRID_KEY) {
        tiles(gx, gy, s, c, 0, sr, 0, memo);
        tiles(gx, gy, s, c, sr + 1, r, 0, memo);
    }
    draw_hole(gx, gy, s, c, sr, hy0, hy1, memo);
    if (plat_panel_begin(PANEL_FP_UL_GRID, gx - 2, DOOMO_MIN(hy0, gy), c * s + 2, gy + r * s - DOOMO_MIN(hy0, gy), key)) {
        plat_panel_solid();
        plat_panel_hole(gx - 2, hy0, c * s + 2, hy1 - hy0);
        if (key == GRID_KEY) {
            tiles(gx, gy, s, c, 0, sr, 0, memo);
            tiles(gx, gy, s, c, sr + 1, r, 0, memo);
            plat_panel_end();
            draw_hole(gx, gy, s, c, sr, hy0, hy1, memo);  /* the rows above were drawn over its top again */
        } else {
            plat_panel_end();
        }
    } else {
        plat_panel_end();
    }
    GRID_KEY = key;
#endif
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
    int sw = fp_sw(), sh = fp_sh(), big = fp_big(), s = cell(), c = cols(), r = rows();
    const UiStyle *st = ui_style(WIN_CUTE);
    int gx = (sw - c * s) / 2, gy = 44, total = unlock_count(), own_all = unlock_owned_count();
    char buf[64];
    ensure();
    if (U.tab != TAB_PROGRESS) {
        /* (S2-6) the backdrop leaves out what the grid and card windows (drawn below, opaque inside their rounded
         * corners) cover: the same picture (as the studio and the creator) */
        int cy = gy + r * s + (big ? 6 : 3), ch = sh - cy - (big ? 16 : 15), holes[8];
        holes[0] = gx - 5; holes[1] = gy; holes[2] = gx + c * s + 3; holes[3] = gy + r * s - 2;
        holes[4] = 7; holes[5] = cy + 5; holes[6] = sw - 7; holes[7] = cy + ch - 5;
        ui_backdrop_holes(WIN_CUTE, (uint32_t)fp_t(), holes, 2);
    } else {
        ui_backdrop(WIN_CUTE, (uint32_t)fp_t());
    }
    doomo_snprintf(buf, sizeof(buf), lang_tr("Unlocks  %d / %d"), own_all, total);
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
    draw_grid(gx, gy, s, c, r, own_all);
    if ((U.n + c - 1) / c > r)
        ui_scroll_arrows(gx + c * s, gy - 6, gy + r * s - 4, U.top > 0, (U.top + r) * c < U.n);
    /* the card */
    {
        /* (CL5, C13 1.17e) 240x160: the card sits right under the grid and its two lines are 9 px apart, so the
         * "How:" line is inside the card, not under the bottom bar */
        int cy = gy + r * s + (big ? 6 : 3), ch = sh - cy - (big ? 16 : 15), l1 = big ? 5 : 3, l2 = big ? 15 : 12;
        ui_window(6, cy, sw - 12, ch, WIN_CREAM);
        if (U.n) {
            const DoomoUnlock *u = unlock_at(fp_ids[U.sel]);
            const UiStyle *cs = ui_style(WIN_CREAM);
            if (u) {
                int own = unlock_has(u->id);
                doomo_snprintf(buf, sizeof(buf), "%s  (%s)", own ? u->name : "???",
                               u->cat < 10 ? doomo_unlock_cat_names[u->cat] : "?");
                fp_marquee(12, cy + l1, sw - 24, buf, own ? cs->accent : cs->text_dim, U.t);
                if (own) {
                    ui_text(12, cy + l2, STR_ICON_CHECK " Unlocked!", PP(PAINT_FOREST, 1));
                } else {
                    doomo_snprintf(buf, sizeof(buf), "How: %s", u->how ? u->how : "a secret...");
                    fp_marquee(12, cy + l2, sw - 24, buf, cs->text, U.t);
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
            ui_text(12, cy + l1, "Nothing here yet!", st->text_dim);
        }
    }
    doomo_snprintf(buf, sizeof(buf), lang_tr("%d of %d found  L/R Category  " STR_ICON_B " Back"), U.owned, U.n);   /* (CL5) was "0/643 here" */
    ui_hint(buf, WIN_CUTE);
    fp_fx_draw();
}

const DoomoScene SCENE_UNLOCKS = { "UNLOCKS", un_enter, NULL, un_update, un_draw, 0 };
