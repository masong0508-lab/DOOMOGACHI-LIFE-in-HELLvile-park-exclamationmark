/* workshop.c - SCENE_WORKSHOP: Rivet's workshop (owner: action).
 *
 * WELD tab: pick every part of a weapon (frame, barrel, core, mod, sight,
 * stock, magazine, underbarrel, charm, style and two paints) with a live
 * first-person preview that breathes and test-fires, eight stat bars,
 * the generated name and the material cost. Locked options show as a dark
 * silhouette with the unlock hint. "Weld!" starts the welding minigame;
 * its quality scales the final stats.
 * ARMORY tab: every welded weapon with its icon, stats and level; equip,
 * mark favourites (quick-switch cycles those), copy a design back onto the
 * table, or scrap it for half the materials.
 */
#include "weld_int.h"

DoomoWeapon weld_pending;
int weld_game_result = -1;

enum { WT_WELD, WT_ARMORY };
enum { WP_FRAME, WP_BARREL, WP_CORE, WP_MOD, WP_SIGHT, WP_STOCK, WP_MAG, WP_UNDER,
       WP_CHARM, WP_STYLE, WP_PAINT, WP_ACCENT, WP_WELD, WP_COUNT };
static const char *const wp_labels[WP_COUNT] = {
    "Frame", "Barrel", "Core", "Mod", "Sight", "Stock", "Mag", "Under",
    "Charm", "Style", "Paint", "Accent", "WELD!"
};
static const uint8_t wp_clist[WP_COUNT] = {
    CLIST_WFRAME, CLIST_WBARREL, CLIST_WCORE, CLIST_WMOD, CLIST_WSIGHT, CLIST_WSTOCK,
    CLIST_WMAG, CLIST_WUNDER, CLIST_WCHARM, 0, 0, 0, 0
};

static int ws_tab, ws_sel, ws_top, ws_t, ws_arm_sel, ws_preview_t, ws_msg_t;
static const char *ws_msg;
static const char *const rivet_lines[] = {
    "Sparks are just tiny fireworks!",
    "Cute or brutal, a weld's a weld.",
    "Hold the bead steady, keep it hot.",
    "Every combo gets its own name!",
    "Scrap from demons? Bring it here.",
    "Back in Hellvile I welded gates.",
    "Try a bubble core in a minigun...",
    "Quality 100? You'd be a legend."
};

static int wp_count(int k)
{
    if (k < WP_STYLE) return weld_part_count(wp_clist[k]);
    if (k == WP_STYLE) return 2;
    if (k == WP_PAINT || k == WP_ACCENT) return PAL_PAINT_COUNT;
    return 0;
}

static int wp_get(int k)
{
    if (k < WP_STYLE) return weld_part_get(&weld_pending, wp_clist[k]);
    if (k == WP_STYLE) return weld_pending.style;
    if (k == WP_PAINT) return weld_pending.paint;
    if (k == WP_ACCENT) return weld_pending.accent;
    return 0;
}

static int wp_locked(int k, int v)
{
    if (k < WP_STYLE) return !unlock_option_ok(wp_clist[k], v);
    if (k == WP_PAINT || k == WP_ACCENT) return !unlock_paint_ok(v);
    return 0;
}

static const char *wp_name(int k, int v)
{
    if (k < WP_STYLE) return weld_part_name(wp_clist[k], v);
    if (k == WP_STYLE) return doomo_wstyle_names[v & 1];
    if (k == WP_PAINT || k == WP_ACCENT) return doomo_paint_names[v & 31];
    return "";
}

static void wp_set(int k, int v)
{
    int n = wp_count(k);
    if (n <= 0) return;
    v = (v % n + n) % n;
    if (k == WP_FRAME) {
        /* a new frame comes with its natural core; looks and charm stay */
        DoomoWeapon keep = weld_pending;
        weld_default(&weld_pending, v, keep.style);
        weld_pending.paint = keep.paint;
        weld_pending.accent = keep.accent;
        weld_pending.charm = keep.charm;
        weld_pending.barrel = keep.barrel;
        weld_pending.mod = keep.mod;
        weld_pending.mag = keep.mag;
        weld_pending.under = keep.under;
        if (keep.sight != WSIGHT_IRON) weld_pending.sight = keep.sight;
    } else if (k < WP_STYLE) {
        weld_part_set(&weld_pending, wp_clist[k], v);
    } else if (k == WP_STYLE) {
        weld_pending.style = (uint8_t)v;
    } else if (k == WP_PAINT) {
        weld_pending.paint = (uint8_t)v;
    } else if (k == WP_ACCENT) {
        weld_pending.accent = (uint8_t)v;
    }
    weld_pending.quality = 60;
    weld_pending.xp = 0;
}

static int design_locked(const char **how)
{
    int k;
    for (k = 0; k < WP_STYLE; k++) {
        int v = wp_get(k);
        if (wp_locked(k, v)) {
            const DoomoUnlock *u = unlock_find(UL_CAT_WEAPON_PART, wp_clist[k], v);
            if (how) *how = u ? u->how : "Locked";
            return 1;
        }
    }
    if (!unlock_paint_ok(weld_pending.paint) || !unlock_paint_ok(weld_pending.accent)) {
        const DoomoUnlock *u = unlock_find(UL_CAT_PAINT, 0, !unlock_paint_ok(weld_pending.paint)
                                                              ? weld_pending.paint : weld_pending.accent);
        if (how) *how = u ? u->how : "Locked paint";
        return 1;
    }
    return 0;
}

static int free_slot(void)
{
    int i;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++)
        if (!(doomo_town.weapons[i].flags & WF_VALID)) return i;
    return -1;
}

int workshop_can_weld(const char **why)
{
    uint8_t items[12], counts[12];
    int n, i;
    if (design_locked(why)) return 0;
    if (free_slot() < 0) { if (why) *why = "The armory is full (scrap one)"; return 0; }
    n = weld_cost(&weld_pending, items, counts, 12);
    for (i = 0; i < n; i++)
        if (inv_count(items[i]) < counts[i]) { if (why) *why = "Not enough materials"; return 0; }
    if (why) *why = "";
    return 1;
}

int workshop_store_weld(int quality)
{
    uint8_t items[12], counts[12];
    int n, i, slot = free_slot();
    if (slot < 0 || quality < 0) return -1;
    n = weld_cost(&weld_pending, items, counts, 12);
    for (i = 0; i < n; i++) if (inv_count(items[i]) < counts[i]) return -1;
    for (i = 0; i < n; i++) inv_take(items[i], counts[i]);
    doomo_town.weapons[slot] = weld_pending;
    doomo_town.weapons[slot].quality = (uint8_t)DOOMO_CLAMP(quality, 0, 100);
    doomo_town.weapons[slot].xp = 0;
    doomo_town.weapons[slot].flags = WF_VALID | (quality >= 100 ? WF_LEGENDARY : 0);
    progress_add(PROG_WEAPONS_WELDED, 1);
    if (quality >= 100) unlock_grant(560);
    return slot;
}

static void starter_kit(void)
{
    if (progress_get(PROG_WEAPONS_WELDED) == 0 && inv_count(ITEM_SCRAP) == 0 && inv_count(ITEM_WELD_ROD) == 0) {
        inv_add(ITEM_SCRAP, 8);
        inv_add(ITEM_GEARS, 4);
        inv_add(ITEM_WELD_ROD, 4);
        inv_add(ITEM_GLITTER, 3);
        inv_add(ITEM_PLUSH_STUFFING, 3);
        inv_add(ITEM_HELLSTEEL, 2);
        ws_msg = "Rivet: \"Here's a starter kit!\"";
        ws_msg_t = 120;
    }
}

static void workshop_enter(int arg)
{
    (void)arg;
    ws_tab = WT_WELD;
    ws_sel = 0;
    ws_top = 0;
    ws_t = 0;
    ws_arm_sel = 0;
    ws_preview_t = 0;
    ws_msg = NULL;
    ws_msg_t = 0;
    combat_give_starter_weapons();
    if (!weld_pending.flags) {
        weld_default(&weld_pending, WFRAME_PISTOL, WSTYLE_CUTE);
        weld_pending.charm = WCHARM_HEART;
    }
    starter_kit();
    audio_music(SONG_WORKSHOP);
}

/* Back from the welding minigame: store the result. */
static void collect_weld(void)
{
    if (weld_game_result >= 0) {
        int slot = workshop_store_weld(weld_game_result);
        if (slot >= 0) {
            static char buf[48];
            char name[32];
            weld_name(&doomo_town.weapons[slot], name, sizeof(name));
            doomo_snprintf(buf, sizeof(buf), "Welded: %s!", name);
            ws_msg = buf;
            ws_msg_t = 150;
            ws_tab = WT_ARMORY;
            ws_arm_sel = slot;
        }
        weld_game_result = -1;
        audio_music(SONG_WORKSHOP);
    }
}

static void say(const char *m) { ws_msg = m; ws_msg_t = 90; }

/* ---- armory helpers --------------------------------------------------------- */
static int armory_list(int *slots)
{
    int i, n = 0;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++)
        if (doomo_town.weapons[i].flags & WF_VALID) slots[n++] = i;
    return n;
}

static int scrap_slot = -1;
static void scrap_done(int yes)
{
    uint8_t items[12], counts[12];
    int n, i, valid = 0;
    if (!yes || scrap_slot < 0) return;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++) if (doomo_town.weapons[i].flags & WF_VALID) valid++;
    if (valid <= 1) { say("Keep at least one weapon!"); return; }
    n = weld_cost(&doomo_town.weapons[scrap_slot], items, counts, 12);
    for (i = 0; i < n; i++) inv_add(items[i], (counts[i] + 1) / 2);
    memset(&doomo_town.weapons[scrap_slot], 0, sizeof(DoomoWeapon));
    if (doomo_town.story.equipped == scrap_slot) {
        for (i = 0; i < DOOMO_MAX_WEAPONS; i++)
            if (doomo_town.weapons[i].flags & WF_VALID) { doomo_town.story.equipped = (uint8_t)i; break; }
    }
    audio_sfx(SFX_WELD);
    say("Scrapped for parts.");
}

static void weld_go(int yes)
{
    if (!yes) return;
    weld_game_result = -1;
    scene_push(&SCENE_WELD_GAME, 0);
}

/* ---- update ---------------------------------------------------------------- */
static void workshop_update(void)
{
    ws_t++;
    ws_preview_t++;
    if (ws_msg_t) ws_msg_t--;
    collect_weld();
    if (btn_pressed(BTN_L) || btn_pressed(BTN_R)) {
        ws_tab ^= 1;
        audio_sfx(SFX_CURSOR);
        return;
    }
    if (btn_pressed(BTN_B) || btn_pressed(BTN_START)) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop(); else scene_set(&SCENE_TITLE, 0);
        return;
    }
    /* --- H4: the furniture bench (home_mart.c, HF sec.8.4) --- */
    if (btn_pressed(BTN_SELECT)) {
        audio_sfx(SFX_SELECT);
        scene_push(&SCENE_HOME_MART, 2);
        return;
    }
    /* --- end H4 --- */
    if (ws_tab == WT_WELD) {
        if (btn_repeat(BTN_UP)) { ws_sel = (ws_sel + WP_COUNT - 1) % WP_COUNT; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { ws_sel = (ws_sel + 1) % WP_COUNT; audio_sfx(SFX_CURSOR); }
        if (ws_sel < WP_WELD && (btn_repeat(BTN_LEFT) || btn_repeat(BTN_RIGHT))) {
            wp_set(ws_sel, wp_get(ws_sel) + (btn_down(BTN_LEFT) ? -1 : 1));
            ws_preview_t = 0;
            audio_sfx(SFX_BLIP);
        }
        if (btn_pressed(BTN_X) || btn_pressed(BTN_Y)) {
            /* surprise me: a random design (only unlocked parts) */
            int k, tries;
            for (k = 0; k < WP_WELD; k++) {
                int n = wp_count(k);
                for (tries = 0; tries < 8; tries++) {
                    int v = rng_range(&doomo_rng, n);
                    if (!wp_locked(k, v)) { if (k == WP_FRAME) wp_set(k, v); else wp_set(k, v); break; }
                }
            }
            ws_preview_t = 0;
            audio_sfx(SFX_SPARKLE);
        }
        if (btn_pressed(BTN_A)) {
            const char *why = "";
            if (ws_sel != WP_WELD) { ws_sel = WP_WELD; audio_sfx(SFX_CURSOR); return; }
            if (workshop_can_weld(&why)) {
                audio_sfx(SFX_SELECT);
                ui_confirm("Weld this beauty?", weld_go);
            } else {
                audio_sfx(SFX_ERROR);
                say(why);
            }
        }
    } else {
        int slots[DOOMO_MAX_WEAPONS], n = armory_list(slots), cur = 0, i;
        for (i = 0; i < n; i++) if (slots[i] == ws_arm_sel) cur = i;
        if (n == 0) return;
        if (btn_repeat(BTN_UP)) { cur = (cur + n - 1) % n; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { cur = (cur + 1) % n; audio_sfx(SFX_CURSOR); }
        ws_arm_sel = slots[cur];
        if (btn_pressed(BTN_A)) {
            doomo_town.story.equipped = (uint8_t)ws_arm_sel;
            audio_sfx(SFX_SELECT);
            say("Equipped!");
        }
        if (btn_pressed(BTN_RIGHT)) {
            doomo_town.weapons[ws_arm_sel].flags ^= WF_FAVORITE;
            audio_sfx(SFX_LOVE);
        }
        if (btn_pressed(BTN_LEFT)) {
            weld_pending = doomo_town.weapons[ws_arm_sel];
            weld_pending.flags = WF_VALID;
            weld_pending.xp = 0;
            ws_tab = WT_WELD;
            ws_sel = 0;
            audio_sfx(SFX_BLIP);
            say("Design copied to the table.");
        }
        if (btn_pressed(BTN_X) || btn_pressed(BTN_Y)) {
            scrap_slot = ws_arm_sel;
            ui_confirm("Scrap it for half the parts?", scrap_done);
        }
    }
}

/* ---- draw --------------------------------------------------------------------- */
static void preview(int x, int y, const DoomoWeapon *w, int silhouette)
{
    int frame = 0, t = ws_preview_t % 96, bob = ui_wave(ws_t, 60, 1);
    int mx, my, gx, gy;
    if (t >= 70 && t < 74) frame = 1;
    else if (t >= 74 && t < 78) frame = 2;
    weld_view_points(w, &mx, &my, &gx, &gy);
    /* soft shadow + spotlight disc */
    ui_fill_ellipse(x + 48, y + 60, 34, 5, PAL_RAMP(RAMP_WOOD, 2));
    if (silhouette) plat_image(x, y + bob, weld_view_image(w, 0), 1, IMG_SILHOUETTE, PAL_RAMP(RAMP_STONE, 1));
    else plat_image(x, y + bob + (frame == 1 ? 3 : 0), weld_view_image(w, frame), 1, 0, 0);
    if (!silhouette && frame == 1) {
        ui_star(x + mx, y + my + bob, 7, w->style == WSTYLE_CUTE ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_RAMP(RAMP_LAVA, 5));
        ui_star(x + mx, y + my + bob, 3, PAL_WHITE);
    }
    if (!silhouette && (ws_t / 20) % 3 == 0) ui_sparkle(x + 12 + (ws_t * 7) % 70, y + 8 + (ws_t * 3) % 30, ws_t & 15, PAL_WHITE);
}

static void stat_bars(int x, int y, int w, const DoomoWeapon *wp, int compact)
{
    static const char *const short_names[WRATE_COUNT] = { "DMG", "RATE", "RNG", "ACC", "AMMO", "RLD", "CTRL", "MOB" };
    static const uint8_t cols[WRATE_COUNT] = {
        PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_TANGERINE, 1), PAL_PAINT(PAINT_LEMON, 1),
        PAL_PAINT(PAINT_LIME, 1), PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_COBALT, 1),
        PAL_PAINT(PAINT_GRAPE, 1), PAL_PAINT(PAINT_BUBBLEGUM, 1)
    };
    DoomoWeaponStats st;
    uint8_t r[WRATE_COUNT];
    int i, col_w = compact ? w / 2 : w, lab = compact ? 20 : 28;
    weld_stats(wp, &st);
    weld_ratings(&st, r);
    for (i = 0; i < WRATE_COUNT; i++) {
        int cx = x + (compact ? (i / 4) * col_w : 0), cy = y + (compact ? (i % 4) : i) * 8;
        if (compact) {
            /* 3-letter labels in the tiny layout */
            char l3[4];
            l3[0] = short_names[i][0]; l3[1] = short_names[i][1]; l3[2] = short_names[i][2]; l3[3] = 0;
            plat_text(cx, cy, l3, PAL_UI_PINK_D);
        } else {
            plat_text(cx, cy, short_names[i], PAL_UI_PINK_D);
        }
        ui_bar(cx + lab, cy + 1, col_w - lab - 3, 6, r[i], 100, cols[i]);
    }
}

static void cost_box(int x, int y, int w, int rows)
{
    uint8_t items[12], counts[12];
    int n = weld_cost(&weld_pending, items, counts, 12), i;
    char buf[32];
    int cw = w / 2, maxc = cw / FONT_ADVANCE;
    plat_text(x, y, "Materials:", PAL_UI_PINK_D);
    for (i = 0; i < n && i < rows * 2; i++) {
        int have = inv_count(items[i]);
        int cx = x + (i % 2) * cw, cy = y + 9 + (i / 2) * 9;
        doomo_snprintf(buf, sizeof(buf), "%c%d/%d %s", doomo_items[items[i]].icon, have, counts[i],
                       doomo_items[items[i]].name);
        if (maxc < (int)sizeof(buf)) buf[maxc] = 0;
        plat_text(cx, cy, buf, have >= counts[i] ? PAL_RAMP(RAMP_GRASS, 1) : PAL_UI_RED);
    }
}

static void name_lines(int x, int y, int w, const char *name, uint8_t col)
{
    /* centred, wrapped onto up to two lines */
    int lw = ui_text_width(name), cut;
    char two[40];
    if (lw <= w) { ui_text_center(x + w / 2, y, name, col); return; }
    /* never more than two lines */
    cut = ui_wrap_skip(name, w, 2);
    if (cut <= 0 || cut >= (int)sizeof(two)) cut = (int)sizeof(two) - 1;
    memcpy(two, name, (size_t)cut);
    two[cut] = 0;
    ui_text_wrap(x, y, w, two, col, 1);
}

static void draw_weld_tab(int sw, int sh)
{
    int big = sh >= 200;
    int lx = 2, ly = 21, lw = big ? 164 : 136, rows = big ? WP_COUNT : 11, i, top;
    int px = lx + lw + 3, pw = sw - px - 2, ph = big ? 152 : 126;
    char name[32];
    const char *how = NULL;
    int locked;
    ui_window(lx, ly - 4, lw, rows * 11 + 7, WIN_CREAM);
    top = ws_sel - rows + 1 > 0 ? ws_sel - rows + 1 : 0;
    if (top > WP_COUNT - rows) top = WP_COUNT - rows;
    if (top < 0) top = 0;
    for (i = 0; i < rows && top + i < WP_COUNT; i++) {
        int k = top + i, ry = ly + i * 11;
        if (k == WP_WELD) {
            ui_button(lx + 14, ry, lw - 28, wp_labels[k], k == ws_sel, WIN_CUTE);
        } else {
            int v = wp_get(k), maxc;
            char val[24];
            doomo_snprintf(val, sizeof(val), "%s%s", wp_locked(k, v) ? STR_ICON_X : "", wp_name(k, v));
            maxc = (lw - 16 - ui_text_width(wp_labels[k]) - 22) / FONT_ADVANCE;
            if (maxc > 0 && maxc < (int)sizeof(val)) val[maxc] = 0;
            ui_value_row(lx + 12, ry, lw - 16, wp_labels[k], val, k == ws_sel);
            if (k == WP_PAINT || k == WP_ACCENT) {
                ui_round_rect(lx + 12 + ui_text_width(wp_labels[k]) + 6, ry + 1, 6, 6, 2, PAL_PAINT(v, 0));
                ui_round_rect(lx + 13 + ui_text_width(wp_labels[k]) + 6, ry + 2, 4, 4, 1, PAL_PAINT(v, 1));
            }
        }
    }
    ui_scroll_arrows(lx + lw - 6, ly - 2, ly + rows * 11 - 4, top > 0, top + rows < WP_COUNT);
    /* preview + name + stats */
    locked = design_locked(&how);
    ui_window(px, ly - 4, pw, ph, WIN_CUTE);
    preview(px + (pw - WELD_VIEW_W) / 2, ly - 3, &weld_pending, locked);
    weld_name(&weld_pending, name, sizeof(name));
    if (locked) {
        ui_text_center(px + pw / 2, ly + 62, STR_ICON_X " LOCKED", PAL_UI_RED);
        ui_text_wrap(px + 4, ly + 72, pw - 8, how ? how : "", PAL_UI_PINK_D, 1);
    } else {
        name_lines(px + 3, ly + 62, pw - 6, name, PAL_UI_PINK_D);
        stat_bars(px + 4, ly + (big ? 84 : 81), pw - 8, &weld_pending, !big);
    }
    if (big) {
        ui_window(lx, ly + rows * 11 + 6, lw, 44, WIN_CREAM);
        cost_box(lx + 6, ly + rows * 11 + 9, lw - 10, 4);
        ui_portrait(sw - 52, ly + ph - 2, ROSTER_ID_RIVET, 1, (ws_t / 90) & 1 ? EXPR_HAPPY : EXPR_TALK);
    }
}

static void draw_armory_tab(int sw, int sh)
{
    int slots[DOOMO_MAX_WEAPONS], n = armory_list(slots), cur = 0, i, top;
    int big = sh >= 200, rows = big ? 9 : 6, lw = big ? 180 : 136, lx = 2, ly = 21;
    int px = lx + lw + 3, pw = sw - px - 2;
    char buf[40];
    for (i = 0; i < n; i++) if (slots[i] == ws_arm_sel) cur = i;
    ui_window(lx, ly - 4, lw, rows * 20 + 7, WIN_CREAM);
    top = cur - rows + 1 > 0 ? cur - rows + 1 : 0;
    for (i = 0; i < rows && top + i < n; i++) {
        const DoomoWeapon *w = &doomo_town.weapons[slots[top + i]];
        int ry = ly + i * 20, sel = top + i == cur, maxc = (lw - 40) / FONT_ADVANCE;
        if (sel) {
            ui_round_rect(lx + 3, ry - 2, lw - 6, 19, 5, PAL_UI_PINK);
            ui_round_rect(lx + 4, ry - 1, lw - 8, 1, 0, PAL_RAMP(RAMP_PASTEL_PINK, 7));
        }
        plat_image(lx + 5, ry + 1 + (sel ? ui_wave(ws_t, 20, 1) : 0), weld_icon_image(w), 1, 0, 0);
        weld_name(w, buf, sizeof(buf));
        if (maxc < (int)sizeof(buf)) buf[maxc] = 0;
        plat_text(lx + 32, ry, buf, sel ? PAL_WHITE : PAL_UI_PINK_D);
        doomo_snprintf(buf, sizeof(buf), "Q%d Lv%d%s%s", w->quality, weld_level(w),
                       (w->flags & WF_FAVORITE) ? " " STR_ICON_HEART : "",
                       slots[top + i] == doomo_town.story.equipped ? " " STR_ICON_CHECK : "");
        plat_text(lx + 32, ry + 9, buf, sel ? PAL_UI_CREAM : PAL_RAMP(RAMP_PASTEL_PINK, 3));
    }
    ui_scroll_arrows(lx + lw - 6, ly - 2, ly + rows * 20 - 4, top > 0, top + rows < n);
    if (n > 0) {
        const DoomoWeapon *w = &doomo_town.weapons[ws_arm_sel];
        DoomoWeaponStats st;
        int ph = big ? 152 : 126;
        weld_stats(w, &st);
        ui_window(px, ly - 4, pw, ph, WIN_CUTE);
        plat_image(px + (pw - WELD_VIEW_W) / 2, ly - 3 + ui_wave(ws_t, 60, 1), weld_view_image(w, 0), 1, 0, 0);
        doomo_snprintf(buf, sizeof(buf), "%s %s", doomo_wclass_names[st.wclass],
                       st.ammo_type == 0xFF ? "" : doomo_ammo_names[st.ammo_type]);
        ui_text_center(px + pw / 2, ly + 62, buf, PAL_UI_PINK_D);
        stat_bars(px + 4, ly + (big ? 74 : 72), pw - 8, w, !big);
        if (big) {
            doomo_snprintf(buf, sizeof(buf), "DPS %d  XP %d", weld_dps(&st), w->xp);
            ui_text_center(px + pw / 2, ly + 142, buf, PAL_UI_PINK_D);
        }
    }
}

static void workshop_draw(void)
{
    static const char *const tabs[2] = { "Weld", "Armory" };
    int sw = plat_screen_w(), sh = plat_screen_h();
    const char *title = "Rivet's Workshop";
    ui_backdrop(WIN_CREAM, (uint32_t)ws_t);
    /* a few welding sparks drifting across the backdrop */
    {
        int k;
        for (k = 0; k < 6; k++) {
            int x = (k * 53 + ws_t * (2 + k % 3)) % (sw + 20) - 10;
            int y = sh - 20 - ((k * 37 + ws_t * (1 + k % 2)) % (sh - 30));
            ui_sparkle(x, y, (ws_t + k * 4) & 15, k & 1 ? PAL_UI_GOLD : PAL_WHITE);
        }
    }
    ui_tabs(4, 3, tabs, 2, ws_tab, WIN_CUTE);
    ui_text_outline(sw - ui_text_width(title) - 6, 4, title, PAL_WHITE, PAL_UI_PINK_D);
    ui_heart(sw - ui_text_width(title) - 12, 7, 3, PAL_PAINT(PAINT_CHERRY, 1));
    {   /* H4: SELECT opens the furniture bench (home_mart.c), between the tabs and the title */
        const char *bench = sw >= 300 ? "SEL: furniture" : "SEL" STR_ICON_HOUSE;
        ui_text(sw - ui_text_width(title) - 20 - ui_text_width(bench), 4, bench, PAL_UI_PINK_D);
    }
    if (ws_tab == WT_WELD) draw_weld_tab(sw, sh);
    else draw_armory_tab(sw, sh);
    if (ws_msg_t && ws_msg) {
        int w = ui_text_width(ws_msg) + 12;
        int y = sh - 30 + (ws_msg_t > 80 ? (ws_msg_t - 80) : 0);
        ui_window(sw / 2 - w / 2, y, w, 14, WIN_MINT);
        ui_text(sw / 2 - w / 2 + 6, y + 3, ws_msg, PAL_RAMP(RAMP_MINT, 1));
    } else if (sh >= 200 && ws_tab == WT_WELD) {
        const char *line = rivet_lines[(ws_t / 180) % DOOMO_COUNTOF(rivet_lines)];
        int w = ui_text_width(line) + 12;
        ui_window(sw - 58 - w, sh - 46, w, 14, WIN_MINT);
        ui_text(sw - 52 - w, sh - 43, line, PAL_RAMP(RAMP_MINT, 1));
    }
    if (ws_tab == WT_WELD)
        ui_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R "Part " STR_ICON_A "Weld X:Random L/R:Tab " STR_ICON_B "Exit", WIN_CUTE);
    else
        ui_hint(STR_ICON_A "Equip " STR_ICON_ARROW_R STR_ICON_HEART " " STR_ICON_ARROW_L "Copy X:Scrap " STR_ICON_B "Exit", WIN_CUTE);
}

const DoomoScene SCENE_WORKSHOP = { "WORKSHOP", workshop_enter, NULL, workshop_update, workshop_draw, 0 };
