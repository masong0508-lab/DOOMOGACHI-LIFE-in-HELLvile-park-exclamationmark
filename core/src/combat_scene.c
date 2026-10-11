/* combat_scene.c - SCENE_COMBAT (arg = map id): a Hell Breach level or an
 * invasion encounter (owner: action). Intro title card, the shooter, a
 * pause menu (resume, weapon select with icons, camera, shoulder,
 * controls, aim assist, crosshair, HUD layout, quit), the level-complete
 * tally (kills, secrets, items, time, rewards) and the death screen with
 * retry. When the scene closes, combat_last_result() tells the caller how
 * it went (1 cleared, -1 died, -2 quit).
 */
#include "combat_int.h"

enum { CS_PLAY, CS_PAUSE, CS_WEAPONS, CS_DONE, CS_DEAD };

static int cs_state, cs_map, cs_t, cs_sel, cs_wsel, cs_top;
static uint32_t cs_rewarded;

static int style_now(void) { return combat_s.hell >= 128 ? WIN_HELL : WIN_CUTE; }

static void combat_enter(int arg)
{
    cs_map = (arg >= 0 && arg < MAPID_COUNT) ? arg : MAPID_VAPE_CELLAR;
    combat_start(cs_map);
    cs_state = CS_PLAY;
    cs_t = 0;
    cs_sel = 0;
    cs_rewarded = 0;
}

static void combat_leave(void)
{
    combat_s.active = 0;
    combat_set_result_quit();          /* left without finishing = quit */
}

static void leave_scene(void)
{
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_TITLE, 0);
}

/* Cut a string to `maxw` pixels (ends with ".." when cut). */
static void fit_text(char *s, int maxw)
{
    int n = (int)strlen(s);
    if (n < 3 || ui_text_width(s) <= maxw) return;
    n -= 2;
    for (;;) {
        int k = n;
        while (k > 0 && s[k - 1] == ' ') k--;
        s[k] = '.';
        s[k + 1] = '.';
        s[k + 2] = 0;
        if (ui_text_width(s) <= maxw || n == 0) return;
        n--;
    }
}

/* ---- pause menu --------------------------------------------------------- */
enum { PM_RESUME, PM_WEAPON, PM_CAMERA, PM_SHOULDER, PM_CONTROLS, PM_INVERT, PM_AIM,
       PM_CROSSHAIR, PM_HUD, PM_GORE, PM_QUIT, PM_COUNT };
static const char *const pm_names[PM_COUNT] = {
    "Resume", "Weapons", "Camera", "Shoulder", "Controls", "Invert look", "Aim assist",
    "Crosshair", "HUD", "Gore", "Quit level"
};
static const int pm_opts[PM_COUNT] = { -1, -1, -1, OPT_SHOULDER, OPT_CONTROL_SCHEME, OPT_INVERT_LOOK,
                                       OPT_AIM_ASSIST, OPT_CROSSHAIR, OPT_HUD_LAYOUT, OPT_GORE, -1 };

static void opt_cycle(int opt, int dir)
{
    const DoomoOptionInfo *oi = &doomo_option_info[opt];
    int v = opt_get(opt), n = oi->count, k;
    for (k = 0; k < n; k++) {
        v = (v + dir + n) % n;
        /* skip values that are still locked (crosshairs) */
        if (unlock_ok(UL_CAT_SKIN, opt, v)) break;
    }
    opt_set(opt, v);
}

static void pause_update(void)
{
    if (btn_pressed(BTN_START) || btn_pressed(BTN_B)) { cs_state = CS_PLAY; audio_sfx(SFX_CANCEL); return; }
    if (btn_repeat(BTN_UP)) { cs_sel = (cs_sel + PM_COUNT - 1) % PM_COUNT; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { cs_sel = (cs_sel + 1) % PM_COUNT; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_LEFT) || btn_repeat(BTN_RIGHT)) {
        int dir = btn_down(BTN_LEFT) ? -1 : 1;
        if (cs_sel == PM_CAMERA) combat_toggle_camera();
        else if (pm_opts[cs_sel] >= 0) {
            opt_cycle(pm_opts[cs_sel], dir);
            if (cs_sel == PM_SHOULDER) doomo_player.shoulder = (uint8_t)opt_get(OPT_SHOULDER);
        }
        audio_sfx(SFX_CURSOR);
    }
    if (btn_pressed(BTN_A)) {
        switch (cs_sel) {
        case PM_RESUME: cs_state = CS_PLAY; break;
        case PM_WEAPON: cs_state = CS_WEAPONS; cs_wsel = doomo_player.weapon_slot; cs_top = 0; break;
        case PM_CAMERA: combat_toggle_camera(); break;
        case PM_QUIT: combat_set_result_quit(); leave_scene(); return;
        default:
            if (pm_opts[cs_sel] >= 0) opt_cycle(pm_opts[cs_sel], 1);
            if (cs_sel == PM_SHOULDER) doomo_player.shoulder = (uint8_t)opt_get(OPT_SHOULDER);
            break;
        }
        audio_sfx(SFX_SELECT);
    }
}

static const char *pm_value(int i, char *buf)
{
    if (i == PM_CAMERA) return doomo_player.camera == CAM_FIRST_PERSON ? "First person" : "Third person";
    if (i == PM_WEAPON) {
        weld_name(combat_weapon(), buf, 32);
        return buf;
    }
    if (pm_opts[i] >= 0) {
        const DoomoOptionInfo *oi = &doomo_option_info[pm_opts[i]];
        int v = opt_get(pm_opts[i]);
        if (oi->values) return oi->values[v];
        doomo_snprintf(buf, 32, "%d", v);
        return buf;
    }
    return "";
}

static void pause_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    int w = sw > 260 ? 220 : 200, rows = sh > 200 ? PM_COUNT : 9;
    int h = rows * 11 + 38, x = sw / 2 - w / 2, y = sh / 2 - h / 2, i, top, ry;
    char buf[40];
    int style = style_now();
    plat_shade_rect(0, 0, sw, sh, 7);
    if (!ui_window_pop(x, y, w, h, style, cs_t)) return;
    ui_title(sw / 2, y - 6, "PAUSED", style);
    top = cs_sel - rows + 1 > 0 ? cs_sel - rows + 1 : 0;
    ry = y + 10;
    for (i = 0; i < rows && top + i < PM_COUNT; i++) {
        int k = top + i;
        const char *val = pm_value(k, buf);
        if (val && val != buf) {                /* cut long weapon names */
            doomo_snprintf(buf, sizeof(buf), "%s", val);
            val = buf;
        }
        if (val) fit_text(buf, w - 12 - ui_text_width(pm_names[k]) - 14);
        if (k == PM_RESUME || k == PM_QUIT) {          /* 14 px pills */
            ui_button(x + 10, ry + 1, w - 20, pm_names[k], k == cs_sel, style);
            ry += 16;
        } else {
            ui_value_row(x + 6, ry, w - 12, pm_names[k], val, k == cs_sel);
            ry += 11;
        }
    }
    ui_scroll_arrows(x + w - 8, y + 8, y + h - 10, top > 0, top + rows < PM_COUNT);
    ui_hint(STR_ICON_A " Select  " STR_ICON_ARROW_L STR_ICON_ARROW_R " Change  " STR_ICON_B " Back", style);
}

/* ---- weapon select ----------------------------------------------------- */
static int weapon_slots(int *slots)
{
    int i, n = 0;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++)
        if ((doomo_town.weapons[i].flags & WF_VALID) && doomo_town.weapons[i].frame < WFRAME_COUNT)
            slots[n++] = i;
    return n;
}

static void weapons_update(void)
{
    int slots[DOOMO_MAX_WEAPONS], n = weapon_slots(slots), cur = 0, i;
    if (btn_pressed(BTN_B) || btn_pressed(BTN_START)) { cs_state = CS_PAUSE; audio_sfx(SFX_CANCEL); return; }
    if (n == 0) return;
    for (i = 0; i < n; i++) if (slots[i] == cs_wsel) cur = i;
    if (btn_repeat(BTN_UP)) { cur = (cur + n - 1) % n; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { cur = (cur + 1) % n; audio_sfx(SFX_CURSOR); }
    cs_wsel = slots[cur];
    if (btn_pressed(BTN_L) || btn_pressed(BTN_R)) {
        doomo_town.weapons[cs_wsel].flags ^= WF_FAVORITE;
        audio_sfx(SFX_BLIP);
    }
    if (btn_pressed(BTN_A)) {
        combat_select_weapon(cs_wsel);
        cs_state = CS_PLAY;
        audio_sfx(SFX_SELECT);
    }
}

static void weapons_draw(void)
{
    int slots[DOOMO_MAX_WEAPONS], n = weapon_slots(slots);
    int sw = plat_screen_w(), sh = plat_screen_h(), style = style_now();
    int rows = sh > 200 ? 10 : 6, w = sw - 24, h = rows * 18 + 26, x = 12, y = (sh - h) / 2 - 4;
    int i, cur = 0, top;
    char buf[40];
    plat_shade_rect(0, 0, sw, sh, 7);
    if (!ui_window_pop(x, y, w, h, style, cs_t)) return;
    ui_title(sw / 2, y - 6, "ARMORY", style);
    for (i = 0; i < n; i++) if (slots[i] == cs_wsel) cur = i;
    top = cur - rows + 1 > 0 ? cur - rows + 1 : 0;
    for (i = 0; i < rows && top + i < n; i++) {
        const DoomoWeapon *wp = &doomo_town.weapons[slots[top + i]];
        DoomoWeaponStats st;
        int ry = y + 12 + i * 18, sel = top + i == cur;
        weld_stats(wp, &st);
        if (sel) {
            ui_round_rect(x + 6, ry - 1, w - 12, 17, 4, ui_style(style)->accent_d);
            ui_cursor(x + 4, ry + 8);
        }
        plat_image(x + 16, ry, weld_icon_image(wp), 1, 0, 0);
        weld_name(wp, buf, sizeof(buf));
        fit_text(buf, w - 44 - 12);
        ui_text(x + 44, ry + 1, buf, sel ? PAL_WHITE : ui_style_text(style));
        doomo_snprintf(buf, sizeof(buf), "%s %s%s", doomo_wclass_names[st.wclass],
                       st.ammo_type == 0xFF ? "melee" : doomo_ammo_names[st.ammo_type],
                       (wp->flags & WF_FAVORITE) ? " " STR_ICON_HEART : "");
        ui_text(x + 44, ry + 9, buf, sel ? ui_lighter(ui_style(style)->accent) : ui_style(style)->text_dim);
    }
    ui_scroll_arrows(x + w - 8, y + 8, y + h - 10, top > 0, top + rows < n);
    ui_hint(STR_ICON_A " Equip  L/R " STR_ICON_HEART " Favourite  " STR_ICON_B " Back", style);
}

/* ---- results ---------------------------------------------------------------- */
static void tally_row(int x, int y, int w, const char *label, int a, int b, int show_b, int t, int delay)
{
    char buf[24];
    int shown = t < delay ? 0 : DOOMO_MIN(a, (t - delay) * (a / 12 + 1));
    if (show_b) doomo_snprintf(buf, sizeof(buf), "%d / %d", shown, b);
    else doomo_snprintf(buf, sizeof(buf), "%d", shown);
    ui_value_row(x, y, w, label, t < delay ? "" : buf, 0);
}

static void done_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), style = style_now();
    int w = sw > 260 ? 240 : 208, h = sh > 200 ? 170 : 132, x = sw / 2 - w / 2, y = sh / 2 - h / 2 + 4;
    const DoomoCombatStats *s = combat_stats();
    char buf[40];
    int secs = (int)(s->ticks / DOOMO_TICK_HZ);
    plat_shade_rect(0, 0, sw, sh, 6);
    if (!ui_window_pop(x, y, w, h, style, cs_t)) return;
    ui_text_big(sw / 2 - ui_text_big_width("CLEARED!", 2) / 2, y - 18 + ui_wave(cs_t, 30, 2), "CLEARED!", 2,
                style == WIN_HELL ? PAL_RAMP(RAMP_LAVA, 7) : PAL_UI_GOLD,
                style == WIN_HELL ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_UI_PINK_D);
    ui_text_center(sw / 2, y + 6, map_title(cs_map), ui_style(style)->title);
    tally_row(x + 8, y + 18, w - 16, "Demons", s->kills, s->kills_total, 1, cs_t, 12);
    tally_row(x + 8, y + 29, w - 16, "Secrets", s->secrets, s->secrets_total, 1, cs_t, 22);
    tally_row(x + 8, y + 40, w - 16, "Items", s->items, s->items_total, 1, cs_t, 32);
    tally_row(x + 8, y + 51, w - 16, "Headshots", s->headshots, 0, 0, cs_t, 40);
    tally_row(x + 8, y + 62, w - 16, "Best combo", s->best_combo, 0, 0, cs_t, 46);
    if (cs_t > 52) {
        doomo_snprintf(buf, sizeof(buf), "%d:%02d", secs / 60, secs % 60);
        ui_value_row(x + 8, y + 73, w - 16, "Time", buf, 0);
    }
    if (cs_t > 60) {
        doomo_snprintf(buf, sizeof(buf), "+%d H$  + scrap", s->money);
        ui_value_row(x + 8, y + 86, w - 16, STR_ICON_COIN " Reward", buf, 1);
    }
    if (cs_t > 70 && sh > 200) {
        int k;
        for (k = 0; k < 5; k++) ui_sparkle(x + 20 + k * (w - 40) / 4, y + h - 30 + ui_wave(cs_t + k * 9, 24, 3), (cs_t + k * 3) & 15, PAL_UI_GOLD);
    }
    if (cs_t > 70) ui_hint(STR_ICON_A " Continue", style);
}

static void dead_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), style = WIN_HELL;
    int w = 180, h = 64, x = sw / 2 - w / 2, y = sh / 2 - 10;
    const char *title = combat_gore_style() == WSTYLE_CUTE ? "CUDDLED TO DEATH" : "YOU DIED";
    int sc = sw > 260 ? 2 : 2;
    plat_shade_rect(0, 0, sw, sh, 5);
    ui_text_big(sw / 2 - ui_text_big_width(title, sc) / 2, y - 36 + ui_wave(cs_t, 40, 2), title, sc,
                PAL_UI_RED, PAL_RAMP(RAMP_BLOOD, 0));
    if (!ui_window_pop(x, y, w, h, style, cs_t)) return;
    ui_button(x + 12, y + 12, w - 24, "Try again", cs_sel == 0, style);
    ui_button(x + 12, y + 34, w - 24, "Give up", cs_sel == 1, style);
    ui_hint(STR_ICON_A " Select", style);
}

/* ---- scene ------------------------------------------------------------------ */
static void combat_update(void)
{
    cs_t++;
    switch (cs_state) {
    case CS_PLAY:
        if (combat_ctl.pause || (btn_pressed(BTN_START) && !combat_autopilot_on())) {
            cs_state = CS_PAUSE;
            cs_t = 0;
            cs_sel = 0;
            audio_sfx(SFX_SELECT);
            combat_ctl.pause = 0;
            return;
        }
        combat_tick();
        if (combat_finished() == 1) { cs_state = CS_DONE; cs_t = 0; audio_music(SONG_VICTORY); }
        else if (combat_finished() == -1) { cs_state = CS_DEAD; cs_t = 0; cs_sel = 0; audio_music(SONG_SAD); }
        break;
    case CS_PAUSE: pause_update(); break;
    case CS_WEAPONS: weapons_update(); break;
    case CS_DONE:
        if (cs_t > 70 && btn_pressed(BTN_A)) { audio_sfx(SFX_SELECT); leave_scene(); }
        break;
    case CS_DEAD:
        if (cs_t < 20) break;
        if (btn_pressed(BTN_UP) || btn_pressed(BTN_DOWN)) { cs_sel ^= 1; audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_A)) {
            audio_sfx(SFX_SELECT);
            if (cs_sel == 0) { combat_start(cs_map); cs_state = CS_PLAY; cs_t = 0; }
            else leave_scene();
        }
        break;
    default: break;
    }
}

static void intro_card(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    const char *title = map_title(cs_map);
    const char *sub = cs_map == MAPID_VAPE_CELLAR ? "HELL BREACH 1" :
                      cs_map == MAPID_CLOUD_NINE_SPIRE ? "HELL BREACH 2" :
                      cs_map == MAPID_ARENA ? "FREE PLAY" : "INVASION!";
    int t = (int)combat_stats()->ticks;
    int y = sh / 3, s = ui_pop_scale(t), sc = s > 270 ? 3 : 2;
    if (t > 60 || cs_state != CS_PLAY) return;
    if (t > 50 && (t & 1)) return;
    ui_text_outline(sw / 2 - ui_text_width(sub) / 2, y - 12, sub,
                    combat_s.hell >= 128 ? PAL_RAMP(RAMP_LAVA, 6) : PAL_UI_GOLD, PAL_RAMP(RAMP_BLOOD, 0));
    ui_text_big(sw / 2 - ui_text_big_width(title, sc) / 2, y, title, sc,
                combat_s.hell >= 128 ? PAL_RAMP(RAMP_BONE, 7) : PAL_WHITE,
                combat_s.hell >= 128 ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_UI_PINK_D);
}

static void combat_draw(void)
{
    DoomoView3D v;
    combat_build_view(&v);
    plat_view3d(&v);
    combat_draw_hud();
    intro_card();
    switch (cs_state) {
    case CS_PAUSE: pause_draw(); break;
    case CS_WEAPONS: weapons_draw(); break;
    case CS_DONE: done_draw(); break;
    case CS_DEAD: if (cs_t > 10) dead_draw(); break;
    default: break;
    }
}

const DoomoScene SCENE_COMBAT = { "COMBAT", combat_enter, combat_leave, combat_update, combat_draw, 0 };
