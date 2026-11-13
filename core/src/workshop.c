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
 *
 * C3 / C15 (clarity): one prompt bar (ui.h ui_prompts, the pad's own
 * button names: "SEL+A Random" on a GBA), and START asks Rivet what the
 * stat letters mean (DMG, RATE... and the Armory's Q / Lv).
 *
 * S2-9 (speed): the weapon picture never makes a tick wait. It comes from
 * weld_view_image_sliced() (weld.h): on the GBA and the PS1 a new picture
 * is composed over a few ticks while the one shown before stays up, and
 * the pictures that come next (the test fire's two frames, the design one
 * step further along the row under the cursor, the next weapon of the
 * Armory list) are composed ahead (weld_view_prefetch).
 * The backdrop leaves out what the two windows cover (ui_backdrop_holes),
 * and where the platform keeps panels (the GBA) the part list's rows but
 * the cursor's and the stat bars are solid retained panels on their
 * windows' plain fill (ui_window_open): put back from the last frame
 * while nothing in them changes. The same pixels (core/tests/test_panel.c).
 */
#include "weld_int.h"
#include "doomo/tips.h"
#include "doomo/plat_ext.h"     /* S2-9: plat_ui_loops */

/* S2-9: where panels are kept (the GBA; the native runner's panel model in the tests) the scene draws its list,
 * stat bars and header as kept panels (below). The Dreamcast keeps none and draws the screen as it always did. */
#if UI_PANELS && !defined(DOOMO_PLATFORM_DC)
#define WS_KEPT 1
#else
#define WS_KEPT 0
#endif

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

/* (S2-9: the small ones are bytes, which pays for the new ones: no more .bss than before) */
static uint8_t ws_tab, ws_sel, ws_arm_sel;
static int16_t ws_msg_t;
static int ws_t, ws_preview_t;
static const char *ws_msg;
static int8_t ws_dir;           /* S2-9: the way the cursor last stepped (a part's value, the Armory list) */
static uint8_t ws_shown;        /* S2-9: the animation frame of the preview picture on show */
#if WS_KEPT
/* S2-9, pixel-changing: the stat bars have no shine (it would change the panel's pixels on every frame it is on a
 * bar) */
#define WS_BAR ui_bar_still
#define WS_SHINE 0
#else
#define WS_BAR ui_bar
#define WS_SHINE 1
#endif
#if WS_KEPT
/* S2-9: the kept panels (ids of the overworld's HUD: never on this screen) and the keys they were last drawn
 * with: a frame whose key is new draws its panel's content before the panel (fp_unlocks.c has the why) */
#define PANEL_WS_LIST  PANEL_OW_OBJECTIVE
#define PANEL_WS_STATS PANEL_OW_CLOCK
#define PANEL_WS_HEAD  PANEL_OW_PROMPT
static uint32_t ws_list_key, ws_stat_key, ws_stat_wkey, ws_head_key;
static uint8_t ws_stat_low;     /* the weapon of ws_stat_wkey has a bar that blinks (ui_bar: under a quarter) */
#endif
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

/* part k of design *wp becomes option v (wrapped) */
static void wp_set_on(DoomoWeapon *wp, int k, int v)
{
    int n = wp_count(k);
    if (n <= 0) return;
    v = (v % n + n) % n;
    if (k == WP_FRAME) {
        /* a new frame comes with its natural core; looks and charm stay */
        DoomoWeapon keep = *wp;
        weld_default(wp, v, keep.style);
        wp->paint = keep.paint;
        wp->accent = keep.accent;
        wp->charm = keep.charm;
        wp->barrel = keep.barrel;
        wp->mod = keep.mod;
        wp->mag = keep.mag;
        wp->under = keep.under;
        if (keep.sight != WSIGHT_IRON) wp->sight = keep.sight;
    } else if (k < WP_STYLE) {
        weld_part_set(wp, wp_clist[k], v);
    } else if (k == WP_STYLE) {
        wp->style = (uint8_t)v;
    } else if (k == WP_PAINT) {
        wp->paint = (uint8_t)v;
    } else if (k == WP_ACCENT) {
        wp->accent = (uint8_t)v;
    }
    wp->quality = 60;
    wp->xp = 0;
}

static void wp_set(int k, int v) { wp_set_on(&weld_pending, k, v); }

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
    ws_dir = 1;
#if WS_KEPT
    ws_list_key = ws_stat_key = ws_stat_wkey = ws_head_key = 0;
#endif
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
            ws_arm_sel = (uint8_t)slot;
        }
        weld_game_result = -1;
        audio_music(SONG_WORKSHOP);
    }
}

static void say(const char *m) { ws_msg = m; ws_msg_t = 90; }

/* ---- armory helpers --------------------------------------------------------- */
/* is there a weapon in the Armory (its pane is drawn) */
static int armory_any(void)
{
    int i;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++) if (doomo_town.weapons[i].flags & WF_VALID) return 1;
    return 0;
}

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
    if (ws_t > 8 && tip_once(TIP_WORKSHOP)) return;                  /* CL4: the stats, once */
    if (btn_pressed(BTN_L) || btn_pressed(BTN_R)) {
        ws_tab ^= 1;
        audio_sfx(SFX_CURSOR);
        return;
    }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop(); else scene_set(&SCENE_TITLE, 0);
        return;
    }
    /* C15: START = the stat key, from Rivet */
    if (btn_pressed(BTN_START)) {
        audio_sfx(SFX_SELECT);
        if (plat_screen_h() >= 200) {   /* the labels as the stat bars spell them on this screen */
            ui_say(ROSTER_ID_RIVET, "The bars! DMG is damage, RATE is shots a second, RNG is range, ACC is aim.");
            ui_say(ROSTER_ID_RIVET, "AMMO is shots per load, RLD is reload speed, CTRL is recoil control, MOB is how fast you run.");
        } else {
            ui_say(ROSTER_ID_RIVET, "The bars! DMG is damage, RAT is fire rate, RNG is range, ACC is aim.");
            ui_say(ROSTER_ID_RIVET, "AMM is shots per load, RLD is reload speed, CTR is recoil control, MOB is how fast you run.");
        }
        ui_say(ROSTER_ID_RIVET, "In the Armory, Q is the weld's quality out of 100. Lv goes up as the weapon fights.");
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
        if (btn_repeat(BTN_UP)) { ws_sel = (uint8_t)((ws_sel + WP_COUNT - 1) % WP_COUNT); audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { ws_sel = (uint8_t)((ws_sel + 1) % WP_COUNT); audio_sfx(SFX_CURSOR); }
        if (ws_sel < WP_WELD && (btn_repeat(BTN_LEFT) || btn_repeat(BTN_RIGHT))) {
            ws_dir = (int8_t)(btn_down(BTN_LEFT) ? -1 : 1);
            wp_set(ws_sel, wp_get(ws_sel) + ws_dir);
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
        if (btn_repeat(BTN_UP)) { cur = (cur + n - 1) % n; ws_dir = -1; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { cur = (cur + 1) % n; ws_dir = 1; audio_sfx(SFX_CURSOR); }
        ws_arm_sel = (uint8_t)slots[cur];
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
/* (S2-9) The preview breathes on the idle picture and test-fires through frames 1 and 2 every 96 ticks. The
 * picture asked for is the frame the clock says; while it is not ready the one on show stays, drawn where its own
 * frame sits (ws_shown), and the muzzle star waits for the real fire frame. Once the picture is ready the next
 * frames of the fire are composed ahead, then the design one step further along the row under the cursor. */
static void preview(int x, int y, const DoomoWeapon *w, int silhouette)
{
    int frame = 0, t = ws_preview_t % 96, bob = ui_wave(ws_t, 60, 1), ready = 0;
    int mx, my, gx, gy;
    const DoomoImage *img;
    if (t >= 70 && t < 74) frame = 1;
    else if (t >= 74 && t < 78) frame = 2;
    if (silhouette) frame = 0;
    img = weld_view_image_sliced(w, frame, &ready);
    if (ready) ws_shown = (uint8_t)frame;
    weld_view_points(w, &mx, &my, &gx, &gy);
    /* soft shadow + spotlight disc */
    ui_fill_ellipse(x + 48, y + 60, 34, 5, PAL_RAMP(RAMP_WOOD, 2));
    if (!img) { /* (the first picture is still under way) */ }
    else if (silhouette) plat_image(x, y + bob, img, 1, IMG_SILHOUETTE, PAL_RAMP(RAMP_STONE, 1));
    else plat_image(x, y + bob + (ws_shown == 1 ? 3 : 0), img, 1, 0, 0);
    if (!silhouette && ready && frame == 1) {
        ui_star(x + mx, y + my + bob, 7, w->style == WSTYLE_CUTE ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_RAMP(RAMP_LAVA, 5));
        ui_star(x + mx, y + my + bob, 3, PAL_WHITE);
    }
    if (!silhouette && (ws_t / 20) % 3 == 0) ui_sparkle(x + 12 + (ws_t * 7) % 70, y + 8 + (ws_t * 3) % 30, ws_t & 15, PAL_WHITE);
    if (ready) {
        /* ahead: the fire's next two pictures (none for a locked design: it never fires), then the neighbour */
        int next = (frame + 1) % 3;
        if (silhouette || (weld_view_prefetch(w, next) && weld_view_prefetch(w, (next + 1) % 3))) {
            if (ws_sel < WP_WELD) {
                DoomoWeapon nb = *w;
                wp_set_on(&nb, ws_sel, wp_get(ws_sel) + ws_dir);
                weld_view_prefetch(&nb, 0);
            }
        }
    }
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
            const char *sn = lang_tr(short_names[i]);
            char l3[4];
            int k;
            for (k = 0; k < 3 && sn[k]; k++) l3[k] = sn[k];
            l3[k] = 0;
            plat_text(cx, cy, l3, PAL_UI_PINK_D);
        } else {
            plat_text(cx, cy, lang_tr(short_names[i]), PAL_UI_PINK_D);
        }
        WS_BAR(cx + lab, cy + 1, col_w - lab - 3, 6, r[i], 100, cols[i]);
    }
}

static void cost_box(int x, int y, int w, int rows)
{
    uint8_t items[12], counts[12];
    int n = weld_cost(&weld_pending, items, counts, 12), i;
    char buf[32];
    int cw = w / 2, maxc = cw / FONT_ADVANCE;
    plat_text(x, y, lang_tr("Materials:"), PAL_UI_PINK_D);
    for (i = 0; i < n && i < rows * 2; i++) {
        int have = inv_count(items[i]);
        int cx = x + (i % 2) * cw, cy = y + 9 + (i / 2) * 9;
        doomo_snprintf(buf, sizeof(buf), "%c%d/%d %s", doomo_items[items[i]].icon, have, counts[i],
                       lang_tr(doomo_items[items[i]].name));
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

/* ---- S2-9: the stat bars as a kept panel ------------------------------------------------------- */
/* The bars lie on the plain fill of the pane's window. stat_window() draws that window and decides: the bars'
 * rect is left out of it (ui_window_open) when the panel can put it back; stat_show(), after everything else
 * of the pane, draws the bars or lets the platform put them back. The key is the weapon, the place, and the
 * two things ui_bar animates: the shine while it is on a bar, the blink of a bar under a quarter. Where no
 * panels are kept (the PS1) this is ui_window() and stat_bars(). */
typedef struct StatKeep {
    int16_t x, y, w, h;         /* the bars' rect */
    uint8_t keep, fresh, compact, style;
    uint32_t key;
} StatKeep;

static void stat_window(StatKeep *k, int wx, int wy, int ww, int wh, int style, int x, int y, int w,
                        const DoomoWeapon *wp, int compact, int on)
{
    memset(k, 0, sizeof(*k));
    k->x = (int16_t)x; k->y = (int16_t)y; k->w = (int16_t)w; k->h = (int16_t)((compact ? 4 : WRATE_COUNT) * 8);
    k->compact = (uint8_t)compact; k->style = (uint8_t)style;
#if WS_KEPT
    /* (not under the message box, which slides over the bars at 240x160) */
    if (on && !(ws_msg_t && ws_msg) && ui_window_open_ok(wx, wy, ww, wh, x, y, x + w, y + k->h)) {
        uint32_t wkey = ui_panel_key();
        const uint8_t *b = (const uint8_t *)wp;
        int t = (int)doomo_frame_count(), shine = 0, i;
        for (i = 0; i < (int)sizeof(*wp); i++) wkey = (wkey ^ b[i]) * 16777619u;
        wkey = ui_key_int(wkey, x | y << 10 | w << 20);
        wkey = ui_key_int(wkey, compact | style << 1 | 0x5700);
        if (wkey != ws_stat_wkey) {
            DoomoWeaponStats st;
            uint8_t r[WRATE_COUNT];
            weld_stats(wp, &st);
            weld_ratings(&st, r);
            ws_stat_low = 0;
            for (i = 0; i < WRATE_COUNT; i++) if (r[i] * 4 < 100) ws_stat_low = 1;
            ws_stat_wkey = wkey;
        }
#if WS_SHINE
        {   /* ui_bar's shine: where it is, when it is on a bar */
            int bw = (compact ? w / 2 : w) - (compact ? 20 : 28) - 3, sx = (t * 3) % (bw + 60) - 10;
            if (sx >= 0 && sx < bw) shine = sx + 1;
        }
#endif
        k->key = ui_key_int(wkey, shine | (ws_stat_low ? (t / 8) % 2 : 0) << 12);
        k->keep = 1;
        k->fresh = k->key != ws_stat_key;
    }
    if (k->keep && !k->fresh) {
        ui_window_open(wx, wy, ww, wh, style, x, y, x + w, y + k->h);
        return;
    }
#else
    (void)on;
#endif
    ui_window(wx, wy, ww, wh, style);
}

#if WS_KEPT
/* What the frame's panels need once the rest of the screen is drawn (workshop_draw shows them last: a panel shown
 * earlier makes every later 2D call over the screen take the GBA's slow, checked path). */
static StatKeep ws_sk;                  /* the stat bars' panel (keep == 0: none this frame) */
static const DoomoWeapon *ws_skw;       /* ... and the weapon they show */
#endif

/* the bars: where a panel keeps them, on a frame whose key is new they are drawn here (the panel itself is
 * stat_show()'s, last of the frame); else in place */
static void stat_draw(const StatKeep *k, const DoomoWeapon *wp)
{
#if WS_KEPT
    if (k->keep) {
        ws_sk = *k;
        ws_skw = wp;
        if (k->fresh) stat_bars(k->x, k->y, k->w, wp, k->compact);
        return;
    }
#endif
    stat_bars(k->x, k->y, k->w, wp, k->compact);
}

#if WS_KEPT
static void stat_show(void)
{
    const StatKeep *k = &ws_sk;
    if (!k->keep) {
        ws_stat_key = 0;                /* (no panel on this frame: the next one draws it whole) */
        return;
    }
    if (plat_panel_begin(PANEL_WS_STATS, k->x, k->y, k->w, k->h, k->key)) {
        plat_panel_solid();
        if (!k->fresh) {
            /* the platform lost it: the window's fill that stat_window() left out, then the bars */
            plat_rect(k->x, k->y, k->w, k->h, ui_style(k->style)->fill);
            stat_bars(k->x, k->y, k->w, ws_skw, k->compact);
        }
    }
    plat_panel_end();
    ws_stat_key = k->key;
}
#endif

/* ---- the Weld tab --------------------------------------------------------------------------------- */
typedef struct WsLay { int sw, sh, big, lx, ly, lw, rows, px, pw, ph; } WsLay;

#if WS_KEPT
/* the part list's panel (draw_weld_tab fills it in; list_show() shows it) */
typedef struct ListKeep {
    uint8_t keep, fresh, hole;
    uint32_t key;
    int x0, y0, x1, y1;                 /* the panel's rect */
    int hy;                             /* the hole's first row (12 rows: the cursor's pill) */
    int top, seli;
    WsLay lay;
} ListKeep;
static ListKeep ws_lk;
#endif

static void ws_layout(WsLay *l, int tab)
{
    l->sw = plat_screen_w(); l->sh = plat_screen_h();
    l->big = l->sh >= 200;
    l->lx = 2; l->ly = 21;
    if (tab == WT_WELD) { l->lw = l->big ? 164 : 136; l->rows = l->big ? WP_COUNT : 11; }
    else { l->lw = l->big ? 180 : 136; l->rows = l->big ? 9 : 6; }
    l->px = l->lx + l->lw + 3;
    l->pw = l->sw - l->px - 2;
    l->ph = l->big ? 152 : 126;
}

/* row i of the part list (part top + i) */
static void part_row(const WsLay *l, int top, int i)
{
    int k = top + i, ry = l->ly + i * 11, lx = l->lx, lw = l->lw;
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

#if WS_KEPT
/* the part list's panel, shown after the rest of the screen: draw_weld_tab() drew the window and its rows but the
 * panel's (on a frame whose key is new: all of them; else the window leaves the rect out, and the cursor's row,
 * the panel's hole, is drawn) */
static void list_show(void)
{
    const ListKeep *k = &ws_lk;
    int i;
    if (!k->keep) {
        ws_list_key = 0;                /* (no panel on this frame: the next one draws it whole) */
        return;
    }
    if (plat_panel_begin(PANEL_WS_LIST, k->x0, k->y0, k->x1 - k->x0, k->y1 - k->y0, k->key)) {
        plat_panel_solid();
        if (k->hole) plat_panel_hole(k->x0, k->hy, k->x1 - k->x0, 12);
        if (!k->fresh) {
            /* the platform lost it: the fill ui_window_open() left out, the rows, then the hole again */
            plat_rect(k->x0, k->y0, k->x1 - k->x0, k->y1 - k->y0, ui_style(WIN_CREAM)->fill);
            ui_style_use(WIN_CREAM);            /* (the rows read the style of the last window: the pane's by now) */
            for (i = 1; i <= k->lay.rows - 2; i++) if (i != k->seli) part_row(&k->lay, k->top, i);
            plat_panel_end();
            if (k->hole) part_row(&k->lay, k->top, k->seli);
        } else {
            plat_panel_end();
        }
    } else {
        plat_panel_end();
    }
    ws_list_key = k->key;
}
#endif

static void draw_weld_tab(const WsLay *l)
{
    int big = l->big, lx = l->lx, ly = l->ly, lw = l->lw, rows = l->rows, px = l->px, pw = l->pw, ph = l->ph;
    int lh = rows * 11 + 7, i, top;
    char name[32];
    const char *how = NULL;
    int locked;
    StatKeep sk;
    /* S2-9: rows 1 .. rows - 2 of the list (value rows on the window's plain fill, clear of its gloss, its lower
     * band and its edges) are a solid kept panel; the cursor's row is its hole, drawn on every frame (its arrows
     * and cursor bob), so stepping through a part's options keeps the panel. The key: what those rows show.
     * A cursor pill is 12 rows tall from 2 rows above its row, one more than the 11 between two rows, so it
     * reaches into its neighbours' rows: the rect is [row 0's pill ends, row rows - 1's pill starts), the hole is
     * the pill's 12 rows, and with the cursor on row 1 or row rows - 2 (the pill would stick out of the rect) the
     * list is drawn whole, as before. */
    int kx0 = lx + 10, kx1 = lx + lw - 2, ky0 = ly + 10, ky1 = ly + (rows - 1) * 11 - 2;
    int keep = 0, fresh = 1, seli;
#if WS_KEPT
    uint32_t key = 0;
#endif
    top = ws_sel - rows + 1 > 0 ? ws_sel - rows + 1 : 0;
    if (top > WP_COUNT - rows) top = WP_COUNT - rows;
    if (top < 0) top = 0;
    seli = ws_sel - top;
#if WS_KEPT
    if (rows >= 5 && top + rows - 1 <= WP_WELD && seli != 1 && seli != rows - 2 &&
        ui_window_open_ok(lx, ly - 4, lw, lh, kx0, ky0, kx1, ky1)) {
        key = ui_key_int(ui_panel_key(), top | seli << 8 | rows << 16);
        key = ui_key_int(key, lx | ly << 10 | lw << 20);
        for (i = 1; i <= rows - 2; i++) {
            int k = top + i, v = wp_get(k);
            if (i != seli) key = ui_key_int(key, v | wp_locked(k, v) << 16);
        }
        keep = 1;
        fresh = key != ws_list_key;
    } else {
        ws_list_key = 0;                /* (the panel is not on this frame: the next one draws it whole) */
    }
#endif
#if WS_KEPT
    if (!fresh) ui_window_open(lx, ly - 4, lw, lh, WIN_CREAM, kx0, ky0, kx1, ky1);
    else
#endif
        ui_window(lx, ly - 4, lw, lh, WIN_CREAM);
    for (i = 0; i < rows && top + i < WP_COUNT; i++) {
        if (!fresh && i >= 1 && i <= rows - 2) {
            if (i != seli) continue;                    /* the panel puts it back */
            plat_rect(kx0, ly + i * 11 - 2, kx1 - kx0, 12, ui_style(WIN_CREAM)->fill);      /* the hole's fill */
        }
        part_row(l, top, i);
    }
    ui_scroll_arrows(lx + lw - 6, ly - 2, ly + rows * 11 - 4, top > 0, top + rows < WP_COUNT);
#if WS_KEPT
    ws_lk.keep = (uint8_t)keep;
    if (keep) {
        ws_lk.fresh = (uint8_t)fresh;
        ws_lk.hole = (uint8_t)(seli >= 2 && seli <= rows - 3);
        ws_lk.key = key;
        ws_lk.x0 = kx0; ws_lk.y0 = ky0; ws_lk.x1 = kx1; ws_lk.y1 = ky1;
        ws_lk.hy = ly + seli * 11 - 2;
        ws_lk.top = top; ws_lk.seli = seli;
        ws_lk.lay = *l;
    }
#else
    (void)keep; (void)kx0; (void)kx1; (void)ky0; (void)ky1;
#endif
    /* preview + name + stats */
    locked = design_locked(&how);
    stat_window(&sk, px, ly - 4, pw, ph, WIN_CUTE, px + 4, ly + (big ? 84 : 81), pw - 8, &weld_pending, !big, !locked);
    preview(px + (pw - WELD_VIEW_W) / 2, ly - 3, &weld_pending, locked);
    weld_name(&weld_pending, name, sizeof(name));
    if (locked) {
        ui_text_center(px + pw / 2, ly + 62, STR_ICON_X " LOCKED", PAL_UI_RED);
        ui_text_wrap(px + 4, ly + 72, pw - 8, how ? how : "", PAL_UI_PINK_D, 1);
    } else {
        name_lines(px + 3, ly + 62, pw - 6, name, PAL_UI_PINK_D);
    }
    if (!locked) stat_draw(&sk, &weld_pending);
    if (big) {
        ui_window(lx, ly + rows * 11 + 6, lw, 44, WIN_CREAM);
        cost_box(lx + 6, ly + rows * 11 + 9, lw - 10, 4);
        ui_portrait(l->sw - 52, ly + ph - 2, ROSTER_ID_RIVET, 1, (ws_t / 90) & 1 ? EXPR_HAPPY : EXPR_TALK);
    }
}

static void draw_armory_tab(const WsLay *l)
{
    int slots[DOOMO_MAX_WEAPONS], n = armory_list(slots), cur = 0, i, top;
    int big = l->big, rows = l->rows, lw = l->lw, lx = l->lx, ly = l->ly;
    int px = l->px, pw = l->pw;
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
        doomo_snprintf(buf, sizeof(buf), big ? "Quality %d Lv%d%s%s" : "Q%d Lv%d%s%s", w->quality, weld_level(w),
                       (w->flags & WF_FAVORITE) ? " " STR_ICON_HEART : "",
                       slots[top + i] == doomo_town.story.equipped ? " " STR_ICON_CHECK : "");
        plat_text(lx + 32, ry + 9, buf, sel ? PAL_UI_CREAM : PAL_RAMP(RAMP_PASTEL_PINK, 3));
    }
    ui_scroll_arrows(lx + lw - 6, ly - 2, ly + rows * 20 - 4, top > 0, top + rows < n);
    if (n > 0) {
        const DoomoWeapon *w = &doomo_town.weapons[ws_arm_sel];
        const DoomoImage *img;
        DoomoWeaponStats st;
        StatKeep sk;
        int ph = l->ph, ready = 0;
        weld_stats(w, &st);
        stat_window(&sk, px, ly - 4, pw, ph, WIN_CUTE, px + 4, ly + (big ? 74 : 72), pw - 8, w, !big, 1);
        /* (S2-9) composed over a few ticks (the picture shown before stays up meanwhile), and the next weapon
         * down or up the list ahead */
        img = weld_view_image_sliced(w, 0, &ready);
        ws_shown = 0;                   /* (an idle picture, whatever the Weld tab showed last) */
        if (img) plat_image(px + (pw - WELD_VIEW_W) / 2, ly - 3 + ui_wave(ws_t, 60, 1), img, 1, 0, 0);
        if (ready && n > 1) weld_view_prefetch(&doomo_town.weapons[slots[(cur + n + ws_dir) % n]], 0);
        doomo_snprintf(buf, sizeof(buf), "%s %s", doomo_wclass_names[st.wclass],
                       st.ammo_type == 0xFF ? "" : doomo_ammo_names[st.ammo_type]);
        ui_text_center(px + pw / 2, ly + 62, buf, PAL_UI_PINK_D);
        if (big) {
            doomo_snprintf(buf, sizeof(buf), "DPS %d  XP %d", weld_dps(&st), w->xp);
            ui_text_center(px + pw / 2, ly + 142, buf, PAL_UI_PINK_D);
        }
        stat_draw(&sk, w);
    }
}

/* ---- S2-9: the header strip (tabs, title, the bench hint) as a kept panel -------------------------- */
#define HDR_H 17                /* the strip's rows: the tabs' bounce reaches row 0, their shadow row 16 (the windows start on 17) */
static void header_content(int sw, int still)
{
    static const char *const tabs[2] = { "Weld", "Armory" };
    const char *title = "Rivet's Workshop";
#if WS_KEPT
    if (still) plat_rect(0, 0, sw, HDR_H, PAL_UI_CREAM);         /* the backdrop's top band, left flat here */
#else
    (void)still;
#endif
    ui_tabs(4, 3, tabs, 2, ws_tab, WIN_CUTE);
    ui_text_outline(sw - ui_text_width(title) - 6, 4, title, PAL_WHITE, PAL_UI_PINK_D);
    ui_heart(sw - ui_text_width(title) - 12, 7, 3, PAL_PAINT(PAINT_CHERRY, 1));
    {   /* H4: SELECT opens the furniture bench (home_mart.c), between the tabs and the title */
        const char *bench = sw >= 300 ? "SEL: furniture" : "SEL" STR_ICON_HOUSE;
        ui_text(sw - ui_text_width(title) - 20 - ui_text_width(bench), 4, bench, PAL_UI_PINK_D);
    }
}

/* Where panels are kept (the GBA) the strip is a solid panel on a flat band instead of the drifting backdrop
 * (the backdrop leaves the strip out): put back from the last frame while the selected tab stands where it did
 * (its bounce is the key). A frame whose key is new draws the strip here, before the panel (head_show()). */
#if WS_KEPT
static uint32_t ws_head_next;           /* this frame's key */
#endif
static void head_draw(int sw)
{
#if WS_KEPT
    ws_head_next = ui_key_int(ui_panel_key(), ws_tab | (ui_wave((int)doomo_frame_count(), 20, 1) > 0) << 1 | sw << 4);
    if (ws_head_next != ws_head_key) header_content(sw, 1);
#else
    header_content(sw, 0);
#endif
}

#if WS_KEPT
static void head_show(int sw)
{
    int fresh = ws_head_next != ws_head_key;
    if (plat_panel_begin(PANEL_WS_HEAD, 0, 0, sw, HDR_H, ws_head_next)) {
        plat_panel_solid();
        if (!fresh) header_content(sw, 1);                       /* the platform lost it */
    }
    plat_panel_end();
    ws_head_key = ws_head_next;
}
#endif

static void workshop_draw(void)
{
    WsLay l;
    int sw, sh;
    ws_layout(&l, ws_tab);
    sw = l.sw; sh = l.sh;
    plat_ui_loops();                    /* S2-9: the platform's 2D loops stay in for the whole visit (GBA) */
    {
        /* S2-9: no backdrop under the two windows (opaque inside their rounded corners; the Armory's pane is only
         * there with a weapon to show) and, where the header is a kept panel, none under the header strip */
        int holes[12], n = 1, lh = l.rows * (ws_tab == WT_WELD ? 11 : 20) + 7;
        holes[0] = l.lx + 1; holes[1] = l.ly - 4 + 5; holes[2] = l.lx + l.lw - 1; holes[3] = l.ly - 4 + lh - 5;
        if (ws_tab == WT_WELD || armory_any()) {
            holes[4] = l.px + 1; holes[5] = l.ly - 4 + 5; holes[6] = l.px + l.pw - 1; holes[7] = l.ly - 4 + l.ph - 5;
            n = 2;
        }
#if WS_KEPT
        holes[4 * n] = 0; holes[4 * n + 1] = 0; holes[4 * n + 2] = sw; holes[4 * n + 3] = HDR_H;
        n++;
#endif
#if WS_KEPT
        ui_backdrop_flat(WIN_CREAM, holes, n);          /* (pixel-changing: what is left of it is a few slivers) */
#else
        ui_backdrop_holes(WIN_CREAM, (uint32_t)ws_t, holes, n);
#endif
    }
    /* a few welding sparks drifting across the backdrop */
    {
        int k;
        for (k = 0; k < 6; k++) {
            int x = (k * 53 + ws_t * (2 + k % 3)) % (sw + 20) - 10;
            int y = sh - 20 - ((k * 37 + ws_t * (1 + k % 2)) % (sh - 30));
            ui_sparkle(x, y, (ws_t + k * 4) & 15, k & 1 ? PAL_UI_GOLD : PAL_WHITE);
        }
    }
    head_draw(sw);
#if WS_KEPT
    ws_lk.keep = 0;
    ws_sk.keep = 0;
#endif
    if (ws_tab == WT_WELD) draw_weld_tab(&l);
    else draw_armory_tab(&l);
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
#if WS_KEPT
    /* the kept panels last: one shown earlier makes the 2D calls after it over its box take the GBA's slow path.
     * Nothing drawn above (the message box, the speech) lies on a panel's rect */
    head_show(sw);
    list_show();
    stat_show();
#endif
    /* C3: the prompt bar; at 240x160 the lower-priority items give way */
    if (ws_tab == WT_WELD) {
        static const UiPrompt weld_bar[] = {
            { BTN_LEFT | BTN_RIGHT, 1, "Pick" }, { BTN_A, 0, "Weld" }, { BTN_X, 2, "Random" },
            { BTN_L | BTN_R, 3, "Armory" }, { BTN_START, 4, "Stats?" }, { BTN_B, 0, "Exit" }
        };
        ui_prompts(weld_bar, (int)DOOMO_COUNTOF(weld_bar), WIN_CUTE);
    } else {
        static const UiPrompt arm_bar[] = {
            { BTN_A, 0, "Equip" }, { BTN_RIGHT, 2, STR_ICON_HEART }, { BTN_LEFT, 3, "Copy" },
            { BTN_X, 1, "Scrap" }, { BTN_START, 4, "Stats?" }, { BTN_B, 0, "Exit" }
        };
        ui_prompts(arm_bar, (int)DOOMO_COUNTOF(arm_bar), WIN_CUTE);
    }
}

const DoomoScene SCENE_WORKSHOP = { "WORKSHOP", workshop_enter, NULL, workshop_update, workshop_draw, 0 };
