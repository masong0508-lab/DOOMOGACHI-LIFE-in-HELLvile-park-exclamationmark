/* fp_shop.c - SCENE_SHOP (owner: freeplay).
 *
 * arg & 15:  0 the HaHaland shop (food, gifts, care, holy, welding),
 *            1 the Hellvile black market (edgier stock, cheap scrap, no
 *              holy water, a little markup on sweets),
 *            2 the Brimstone Bites counter (cakes & drinks).
 * arg >> 4:  the tab to open on.
 *
 * Stock rotates every day (seeded by island + day + shop) and there's a
 * deal of the day at 30% off. Locked items (unlocks.h UL_CAT_ITEM) show as
 * "???" with their hint until earned. The shopkeeper waves from behind
 * the counter.
 */
#include "fp_int.h"

#define SHOP_TABS_MAX 5
#define STOCK_MAX 12

typedef struct { const char *name; uint8_t cat; } Tab;

static const Tab shop_tabs[] = {
    { "Food", ICAT_FOOD }, { "Gifts", ICAT_GIFT }, { "Care", ICAT_CARE },
    { "Holy", ICAT_EXORCISM }, { "Weld", ICAT_MATERIAL }
};
static const Tab bm_tabs[] = {
    { "Snacks", ICAT_FOOD }, { "Trinkets", ICAT_GIFT }, { "Pills", ICAT_CARE }, { "Scrap", ICAT_MATERIAL }
};
static const Tab cafe_tabs[] = { { "Treats", ICAT_FOOD } };

/* what each counter will ever sell */
static int sells(int kind, int item)
{
    const DoomoItemInfo *it = &doomo_items[item];
    if (it->price == 0 || it->category == ICAT_KEY) return 0;
    switch (kind) {
    case FP_SHOP_ARG_BLACK_MARKET:
        if (it->category == ICAT_EXORCISM) return 0;
        if (item == ITEM_ANGEL_CAKE || item == ITEM_FLOWERS || item == ITEM_LOVE_LETTER ||
            item == ITEM_PLUSHIE || item == ITEM_RING || item == ITEM_SALAD || item == ITEM_SOAP)
            return 0;
        return 1;
    case FP_SHOP_ARG_CAFE:
        return it->category == ICAT_FOOD;
    default:
        return item != ITEM_HELL_PEPPERS && item != ITEM_BRIMSTONE_BAGEL && item != ITEM_DEMON_HORN;
    }
}

static int price_of(int kind, int item, int deal)
{
    int p = doomo_items[item].price, c = doomo_items[item].category;
    if (kind == FP_SHOP_ARG_BLACK_MARKET) {
        if (c == ICAT_MATERIAL) p = p * 7 / 10;
        else if (c == ICAT_FOOD) p = p * 11 / 10;
    } else if (kind == FP_SHOP_ARG_CAFE) {
        p = p * 9 / 10;
    }
    if (deal) p = p * 7 / 10;
    return DOOMO_MAX(p, 1);
}

static struct {
    int kind, tab, ntabs, t, sel, top;
    const Tab *tabs;
    uint8_t stock[STOCK_MAX];
    uint8_t locked[STOCK_MAX];
    int nstock;
    int deal_item;
    int qty_open, qty, qty_t;
    int host;
    int bought_t;
} S;

static void build_stock(void)
{
    int cat = S.tabs[S.tab].cat, i, n = 0, k;
    uint8_t cand[ITEM_COUNT];
    uint32_t h = doomo_hash32(doomo_town.seed ^ (doomo_town.day * 2654435761u) ^ (uint32_t)(S.kind * 77 + S.tab));
    S.nstock = 0;
    for (i = 1; i < ITEM_COUNT; i++)
        if (doomo_items[i].category == cat && sells(S.kind, i)) cand[n++] = (uint8_t)i;
    /* daily rotation: keep about 3/4 of the shelf (always at least 4) */
    for (k = 0; k < n && S.nstock < STOCK_MAX; k++) {
        int it = cand[k];
        int keep = n <= 4 || ((h >> (k % 29)) & 3) != 0 || k == 0;
        int ok = unlock_ok(UL_CAT_ITEM, 0, it);
        if (!keep && ok) continue;
        S.stock[S.nstock] = (uint8_t)it;
        S.locked[S.nstock] = (uint8_t)!ok;
        S.nstock++;
    }
    /* unlocked first, the locked teasers at the end */
    for (i = 1; i < S.nstock; i++) {
        int j = i;
        while (j > 0 && S.locked[j - 1] > S.locked[j]) {
            uint8_t a = S.stock[j], b = S.locked[j];
            S.stock[j] = S.stock[j - 1]; S.locked[j] = S.locked[j - 1];
            S.stock[j - 1] = a; S.locked[j - 1] = b;
            j--;
        }
    }
    S.sel = DOOMO_MIN(S.sel, DOOMO_MAX(S.nstock - 1, 0));
    S.top = 0;
}

static void pick_deal(void)
{
    int i, n = 0;
    uint8_t c[ITEM_COUNT];
    uint32_t h = doomo_hash32(doomo_town.seed + doomo_town.day * 977u + (uint32_t)S.kind);
    for (i = 1; i < ITEM_COUNT; i++)
        if (sells(S.kind, i) && unlock_ok(UL_CAT_ITEM, 0, i)) c[n++] = (uint8_t)i;
    S.deal_item = n ? c[h % (uint32_t)n] : -1;
}

static void shop_enter(int arg)
{
    memset(&S, 0, sizeof(S));
    S.kind = arg & 15;
    if (S.kind > FP_SHOP_ARG_CAFE) S.kind = FP_SHOP_ARG_SHOP;
    switch (S.kind) {
    case FP_SHOP_ARG_BLACK_MARKET: S.tabs = bm_tabs; S.ntabs = DOOMO_COUNTOF(bm_tabs); S.host = roster_find("Swipey"); break;
    case FP_SHOP_ARG_CAFE: S.tabs = cafe_tabs; S.ntabs = 1; S.host = roster_find("Cardamom"); break;
    default: S.tabs = shop_tabs; S.ntabs = DOOMO_COUNTOF(shop_tabs); S.host = roster_find("Bargain Bun"); break;
    }
    S.tab = DOOMO_CLAMP(arg >> 4, 0, S.ntabs - 1);
    build_stock();
    pick_deal();
    audio_music(S.kind == FP_SHOP_ARG_BLACK_MARKET ? SONG_HELLVILE : SONG_SHOP);
}

static void shop_leave(void)
{
    if (doomo_town.mode == TOWN_FREEPLAY) audio_music(fp_island_song());
}

static int max_qty(int item, int price)
{
    int room = 99 - inv_count(item), afford = opt_get(OPT_CHEAT_RICH) ? 99 : (int)(doomo_town.money / (uint32_t)price);
    return DOOMO_MAX(0, DOOMO_MIN(room, afford));
}

static void buy(void)
{
    int it = S.stock[S.sel], price = price_of(S.kind, it, it == S.deal_item), total = price * S.qty;
    char buf[64];
    if (S.qty <= 0 || max_qty(it, price) < S.qty) { audio_sfx(SFX_ERROR); return; }
    if (!opt_get(OPT_CHEAT_RICH)) doomo_town.money -= (uint32_t)total;
    inv_add(it, S.qty);
    doomo_snprintf(buf, sizeof(buf), STR_ICON_HEART " Bought %d x %s!", S.qty, doomo_items[it].name);
    ui_toast(buf);
    audio_sfx(SFX_PICKUP);
    fp_fx_burst(fp_sw() / 3, fp_sh() / 2, FX_COIN, 6 + S.qty / 3);
    S.bought_t = 40;
    S.qty_open = 0;
    if (total >= 500) fp_achieve(UL_FP_SHOPAHOLIC);
    if (S.kind == FP_SHOP_ARG_BLACK_MARKET) fp_achieve(UL_FP_BLACK_MARKET);
    if (inv_count(it) >= 99) fp_achieve(UL_FP_COLLECTOR);
}

static void shop_update(void)
{
    int vis = fp_big() ? 10 : 7;
    fp_live();
    S.t++;
    if (S.bought_t) S.bought_t--;
    if (S.qty_open) {
        int it = S.stock[S.sel], price = price_of(S.kind, it, it == S.deal_item), mx = max_qty(it, price);
        S.qty_t++;
        if (btn_repeat(BTN_RIGHT | BTN_UP) && S.qty < mx) { S.qty++; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_LEFT | BTN_DOWN) && S.qty > 1) { S.qty--; audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_R)) { S.qty = DOOMO_MIN(mx, S.qty + 10); audio_sfx(SFX_BLIP); }
        if (btn_pressed(BTN_L)) { S.qty = DOOMO_MAX(1, S.qty - 10); audio_sfx(SFX_BLIP); }
        if (btn_pressed(BTN_A)) buy();
        if (btn_pressed(BTN_B)) { S.qty_open = 0; audio_sfx(SFX_CANCEL); }
        return;
    }
    if (S.ntabs > 1 && btn_pressed(BTN_L | BTN_R)) {
        S.tab = (S.tab + (btn_pressed(BTN_L) ? S.ntabs - 1 : 1)) % S.ntabs;
        S.sel = 0;
        build_stock();
        audio_sfx(SFX_BLIP);
        return;
    }
    if (S.nstock) {
        if (btn_repeat(BTN_UP)) { S.sel = (S.sel + S.nstock - 1) % S.nstock; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { S.sel = (S.sel + 1) % S.nstock; audio_sfx(SFX_CURSOR); }
        if (S.sel < S.top) S.top = S.sel;
        if (S.sel >= S.top + vis) S.top = S.sel - vis + 1;
    }
    if (btn_pressed(BTN_A) && S.nstock) {
        int it = S.stock[S.sel], price = price_of(S.kind, it, it == S.deal_item);
        if (S.locked[S.sel]) {
            const DoomoUnlock *u = unlock_find(UL_CAT_ITEM, 0, it);
            audio_sfx(SFX_ERROR);
            ui_toast(u && u->how ? u->how : "Not in stock yet!");
        } else if (max_qty(it, price) <= 0) {
            audio_sfx(SFX_ERROR);
            ui_toast(inv_count(it) >= 99 ? "Your bag is full of those!" : "Not enough H$...");
        } else {
            S.qty_open = 1;
            S.qty = 1;
            S.qty_t = 0;
            audio_sfx(SFX_SELECT);
        }
        return;
    }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop();
        else fp_goto_island(FP_ISLAND_ARG_RETURN);
    }
}

/* ====================================================================== */
/* drawing                                                                  */
static void stat_line(int x, int y, int w, int it, uint8_t c)
{
    const DoomoItemInfo *d = &doomo_items[it];
    static const char *const nm[7] = { "Food", "Joy", "Clean", "Nrg", "HP", "Mind", "Dark" };
    int v[7], i, cx = x;
    char buf[16];
    v[0] = d->hunger; v[1] = d->happy; v[2] = d->hygiene; v[3] = d->energy;
    v[4] = d->health; v[5] = d->sanity; v[6] = d->corruption;
    for (i = 0; i < 7; i++) {
        int tw;
        if (!v[i]) continue;
        doomo_snprintf(buf, sizeof(buf), "%s%c%d", nm[i], v[i] > 0 ? '+' : '-', v[i] > 0 ? v[i] : -v[i]);
        tw = ui_text_width(buf) + 6;
        if (cx + tw > x + w) { y += FONT_LINE; cx = x; }
        ui_text(cx, y, buf, (i == 6) ? (v[i] > 0 ? PAL_UI_RED : PP(PAINT_FOREST, 1)) : (v[i] > 0 ? c : PAL_UI_RED));
        cx += tw;
    }
    if (d->holy) {
        doomo_snprintf(buf, sizeof(buf), "Holy %d", d->holy);
        if (cx + ui_text_width(buf) > x + w) { y += FONT_LINE; cx = x; }
        ui_text(cx, y, buf, PAL_UI_GOLD);
    }
}

static void shop_draw(void)
{
    int sw = fp_sw(), sh = fp_sh(), big = fp_big(), i, t = S.t;
    int bm = S.kind == FP_SHOP_ARG_BLACK_MARKET;
    int style = bm ? WIN_DARK : S.kind == FP_SHOP_ARG_CAFE ? WIN_MINT : WIN_CREAM;
    const UiStyle *st = ui_style(style);
    int lx = 6, ly = 34, lw = big ? 180 : 132, vis = big ? 10 : 7, rx, rw;
    const char *title = bm ? "Black Market" : S.kind == FP_SHOP_ARG_CAFE ? "Brimstone Bites" : "HaHaland Shop";
    char buf[48];
    ui_backdrop(bm ? WIN_HELL : style == WIN_MINT ? WIN_MINT : WIN_CUTE, (uint32_t)fp_t());
    ui_title(sw / 2 - (big ? 40 : 24), 3, title, style);
    fp_money_pill(sw - 4, 4, 1);
    /* tabs */
    {
        const char *names[SHOP_TABS_MAX];
        for (i = 0; i < S.ntabs; i++) names[i] = S.tabs[i].name;
        ui_tabs(lx + 2, 21, names, S.ntabs, S.tab, style);
    }
    /* shelf */
    ui_window(lx - 2, ly - 4, lw + 4, vis * 12 + 8, style);
    for (i = 0; i < vis && S.top + i < S.nstock; i++) {
        int it = S.stock[S.top + i], ry = ly + i * 12, sel = S.top + i == S.sel;
        int locked = S.locked[S.top + i], deal = it == S.deal_item && !locked;
        char price[16];
        if (sel) {
            fp_pill(lx + 8, ry - 1, lw - 8, 11, st->accent_d, ui_darker(st->accent_d), ui_lighter(st->accent_d));
            ui_cursor(lx - 1, ry + 1);
        }
        if (locked) {
            fp_icon(lx + 11, ry + 1, ICON_X, st->text_dim);
            ui_text(lx + 20, ry + 1, "???", st->text_dim);
            continue;
        }
        fp_icon(lx + 11, ry + 1, fp_item_icon(it), deal ? PAL_UI_GOLD : st->accent);
        fp_marquee(lx + 20, ry + 1, lw - 20 - 34, doomo_items[it].name, st->text, sel ? t : 0);
        doomo_snprintf(price, sizeof(price), "%d", price_of(S.kind, it, deal));
        ui_text(lx + lw - ui_text_width(price) - 3, ry + 1, price, deal ? PAL_UI_RED : st->text_dim);
        if (deal) ui_star(lx + lw - ui_text_width(price) - 9, ry + 4, 3, PAL_UI_GOLD);
    }
    if (S.nstock > vis) ui_scroll_arrows(lx + lw - 2, ly - 6, ly + vis * 12 - 2, S.top > 0, S.top + vis < S.nstock);
    /* the counter: shopkeeper + details */
    rx = lx + lw + 8;
    rw = sw - rx - 4;
    {
        int cy = ly - 4, ch = sh - cy - 16;
        int it = S.nstock ? S.stock[S.sel] : 0;
        ui_window(rx, cy, rw, ch, style);
        if (S.host >= 0 && rw >= FIGURE_W) {
            const DoomoResident *p = roster_premade(S.host);
            int fx = rx + (rw - FIGURE_W) / 2, fy = cy + 2 + (ui_wave(t, 60, 1) > 0);
            if (p) fp_figure(fx, fy - (big ? 0 : 22), fp_host_look(S.host), RES_ALIVE,
                             S.bought_t ? EXPR_LOVE : (S.qty_open ? EXPR_HAPPY : EXPR_NEUTRAL));
            /* the counter top */
            plat_rect(rx + 2, cy + (big ? 74 : 52), rw - 4, 4, bm ? PAL_RAMP(RAMP_STONE, 1) : PAL_RAMP(RAMP_WOOD, 3));
            plat_rect(rx + 2, cy + (big ? 74 : 52), rw - 4, 1, bm ? PAL_RAMP(RAMP_STONE, 3) : PAL_RAMP(RAMP_WOOD, 5));
        }
        if (S.nstock) {
            int ty = cy + (big ? 82 : 58);
            if (S.locked[S.sel]) {
                const DoomoUnlock *u = unlock_find(UL_CAT_ITEM, 0, it);
                ui_text(rx + 6, ty, "Coming soon!", st->accent);
                ui_text_wrap(rx + 6, ty + FONT_LINE, rw - 12, u && u->how ? u->how : "Locked", st->text_dim, 1);
            } else {
                fp_marquee(rx + 6, ty, rw - 12, doomo_items[it].name, st->accent, t);
                doomo_snprintf(buf, sizeof(buf), "Have: %d", inv_count(it));
                ui_text(rx + 6, ty + FONT_LINE, buf, st->text_dim);
                if (big) {
                    ui_text_wrap(rx + 6, ty + 2 * FONT_LINE + 1, rw - 12, doomo_items[it].desc, st->text, 1);
                    stat_line(rx + 6, ty + 5 * FONT_LINE, rw - 12, it, PP(PAINT_FOREST, 1));
                } else {
                    stat_line(rx + 6, ty + 2 * FONT_LINE, rw - 12, it, PP(PAINT_FOREST, 1));
                }
            }
        }
    }
    if (!big && S.nstock && !S.locked[S.sel])
        ui_text_wrap(lx + 2, ly + vis * 12 + 6, lw, doomo_items[S.stock[S.sel]].desc, PAL_WHITE, 0);
    ui_hint(S.ntabs > 1 ? STR_ICON_A " Buy  L/R Shelf  " STR_ICON_B " Leave" : STR_ICON_A " Buy  " STR_ICON_B " Leave", style);
    /* quantity popup */
    if (S.qty_open) {
        int it = S.stock[S.sel], price = price_of(S.kind, it, it == S.deal_item);
        int w = big ? 190 : 160, h = 60, x = (sw - w) / 2, y = (sh - h) / 2;
        const UiStyle *ps;
        plat_shade_rect(0, 0, sw, sh, 10);
        if (ui_window_pop(x, y, w, h, style, S.qty_t)) {
            ps = ui_style(style);
            doomo_snprintf(buf, sizeof(buf), "How many %s?", doomo_items[it].name);
            ui_text_center(sw / 2, y + 6, buf, ps->text);
            doomo_snprintf(buf, sizeof(buf), STR_ICON_ARROW_L " %d " STR_ICON_ARROW_R, S.qty);
            ui_text_big(sw / 2 - ui_text_big_width(buf, 2) / 2, y + 18, buf, 2, ps->accent, ps->outline);
            doomo_snprintf(buf, sizeof(buf), "Total: H$ %d", price * S.qty);
            ui_text_center(sw / 2, y + h - 13, buf, (uint32_t)(price * S.qty) > doomo_town.money ? PAL_UI_RED : ps->text_dim);
        }
    }
    fp_fx_draw();
}

const DoomoScene SCENE_SHOP = { "SHOP", shop_enter, shop_leave, shop_update, shop_draw, 0 };
