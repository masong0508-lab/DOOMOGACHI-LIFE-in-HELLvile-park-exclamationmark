/* levels_play.c - SCENE_BREACH: play one Hell Breach level from the level
 * select (owner: levels). arg = level | mode << 8 | LVF_DEMO (levels_int.h).
 *
 * A combat session like SCENE_COMBAT's, plus: an intro card (tag, title,
 * tagline, par time, the mode), the levels runtime (mini-boss variants,
 * conveyors, music that turns hellish), the Time Trial clock, the
 * Cute-Only / Brutal-Only rules (weapons of the other style are put away
 * and the gore follows the style), a pause menu with restart, the
 * results tally that grades the run S/A/B/C with a rank stamp, and the
 * death screen with retry. LVF_DEMO lets the autopilot play (attract /
 * "Watch demo"; any button stops it; nothing is recorded).
 */
#include "levels_int.h"

enum { BR_INTRO, BR_PLAY, BR_PAUSE, BR_DONE, BR_DEAD };
enum { BP_RESUME, BP_WEAPON, BP_CAMERA, BP_RESTART, BP_QUIT, BP_COUNT };

static struct {
    uint8_t  level, mode, demo, state, sel, news;
    uint8_t  gore_forced, old_gore, deaths;
    uint16_t t, warn_t;
    int16_t  bonus;
    LvScore  sc;
} br;

static int br_style_wanted(void) { return br.mode == LVM_CUTE_ONLY ? WSTYLE_CUTE : WSTYLE_BRUTAL; }
static int br_challenge(void) { return br.mode == LVM_CUTE_ONLY || br.mode == LVM_BRUTAL_ONLY; }

static int weapon_ok(int slot)
{
    const DoomoWeapon *w = &doomo_town.weapons[slot];
    return (w->flags & WF_VALID) && w->frame < WFRAME_COUNT;
}

/* a weapon of the wanted style: one from the armory, else Rivet lends one */
static int find_style_weapon(int style)
{
    int i;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++)
        if (weapon_ok(i) && doomo_town.weapons[i].style == style) return i;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++)
        if (!weapon_ok(i)) {
            DoomoWeapon *w = &doomo_town.weapons[i];
            weld_default(w, style == WSTYLE_CUTE ? WFRAME_BUBBLE_WAND : WFRAME_PUMP_SHOTGUN, style);
            w->flags = WF_VALID | WF_STARTER;
            ui_toast(style == WSTYLE_CUTE ? "Rivet lends you a Bubble Wand!" : "Rivet lends you a pump shotgun!");
            return i;
        }
    return -1;
}

static void setup_challenge(void)
{
    int slot;
    if (!br_challenge()) return;
    if (!br.gore_forced) {
        br.old_gore = (uint8_t)opt_get(OPT_GORE);
        br.gore_forced = 1;
    }
    opt_set(OPT_GORE, br.mode == LVM_CUTE_ONLY ? GORE_CUTE : GORE_BRUTAL);
    slot = find_style_weapon(br_style_wanted());
    if (slot >= 0) {
        doomo_player.weapon_slot = (uint8_t)slot;
        combat_select_weapon(slot);
    }
}

/* weapons of the other style are put away again */
static void enforce_challenge(void)
{
    int i;
    if (br.warn_t) br.warn_t--;
    if (combat_weapon()->style == br_style_wanted() || combat_px.switch_to >= 0) return;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++)
        if (weapon_ok(i) && doomo_town.weapons[i].style == br_style_wanted()) {
            combat_select_weapon(i);
            break;
        }
    if (!br.warn_t) {
        combat_toast(br.mode == LVM_CUTE_ONLY ? "Cute weapons only! " STR_ICON_HEART
                                              : "Brutal weapons only! " STR_ICON_SKULL);
        audio_sfx(SFX_ERROR);
        br.warn_t = 90;
    }
}

static void br_start(void)
{
    combat_start(lv_info[br.level].map);
    combat_refill_ammo(100);                 /* every run starts with a full loadout */
    lv_runtime_begin(br.level);
    setup_challenge();
    if (br.demo) combat_autopilot(1);
    br.state = BR_INTRO;
    br.t = 0;
    br.sel = 0;
    br.warn_t = 0;
}

static void br_enter(int arg)
{
    memset(&br, 0, sizeof(br));
    br.level = (uint8_t)DOOMO_CLAMP(arg & 0xFF, 0, LV_COUNT - 1);
    br.mode = (uint8_t)(((arg >> 8) & 0xF) < LVM_COUNT ? (arg >> 8) & 0xF : LVM_NORMAL);
    br.demo = (uint8_t)((arg & LVF_DEMO) != 0);
    br_start();
}

static void br_leave(void)
{
    combat_s.active = 0;
    combat_set_result_quit();
    if (br.demo) combat_autopilot(0);
    if (br.gore_forced) {
        opt_set(OPT_GORE, br.old_gore);
        br.gore_forced = 0;
    }
}

static void leave_scene(void)
{
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_TITLE, 0);
}

/* ---- update ----------------------------------------------------------------- */
static void play_tick(void)
{
    int fin;
    lv_runtime_pre();
    if (br_challenge()) enforce_challenge();
    combat_tick();
    lv_runtime_post();
    fin = combat_finished();
    if (fin == 1) {
        br.state = BR_DONE;
        br.t = 0;
        if (br.demo) lv_score(br.level, combat_stats(), &br.sc);
        else {
            int bonus = 0;
            br.news = (uint8_t)lv_finish(br.level, br.mode, combat_stats(), &br.sc, &bonus);
            br.bonus = (int16_t)bonus;
        }
        audio_music(SONG_VICTORY);
    } else if (fin == -1) {
        br.state = BR_DEAD;
        br.t = 0;
        br.sel = 0;
        br.deaths++;
        audio_music(SONG_SAD);
    }
}

static void pause_update(void)
{
    if (btn_pressed(BTN_START) || btn_pressed(BTN_B)) { br.state = BR_PLAY; audio_sfx(SFX_CANCEL); return; }
    if (btn_repeat(BTN_UP)) { br.sel = (uint8_t)((br.sel + BP_COUNT - 1) % BP_COUNT); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { br.sel = (uint8_t)((br.sel + 1) % BP_COUNT); audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_LEFT) || btn_pressed(BTN_RIGHT)) {
        int dir = btn_down(BTN_LEFT) ? -1 : 1;
        if (br.sel == BP_WEAPON) combat_next_weapon(dir);
        else if (br.sel == BP_CAMERA) { combat_toggle_camera(); audio_sfx(SFX_CURSOR); }
    }
    if (!btn_pressed(BTN_A)) return;
    audio_sfx(SFX_SELECT);
    switch (br.sel) {
    case BP_RESUME: br.state = BR_PLAY; break;
    case BP_WEAPON: combat_next_weapon(1); break;
    case BP_CAMERA: combat_toggle_camera(); break;
    case BP_RESTART: br_start(); break;
    default: combat_set_result_quit(); leave_scene(); break;
    }
}

static void br_update(void)
{
    br.t++;
    switch (br.state) {
    case BR_INTRO:
        if (br.t > 80 || (br.t > 12 && !br.demo && btn_pressed(BTN_A | BTN_START))) {
            br.state = BR_PLAY;
            br.t = 0;
            audio_sfx(SFX_DEMON_ROAR);
        }
        break;
    case BR_PLAY:
        if (br.demo) {
            if (br.t > 15 && btn_pressed(BTN_A | BTN_B | BTN_START)) { audio_sfx(SFX_CANCEL); leave_scene(); return; }
        } else if (combat_ctl.pause || btn_pressed(BTN_START)) {
            combat_ctl.pause = 0;
            br.state = BR_PAUSE;
            br.sel = 0;
            br.t = 0;
            audio_sfx(SFX_SELECT);
            return;
        }
        play_tick();
        break;
    case BR_PAUSE:
        pause_update();
        break;
    case BR_DONE:
        if (br.t == 83) audio_sfx(br.sc.rank == RANK_S ? SFX_EVOLVE : SFX_EXPLODE);
        if ((br.t > 150 && btn_pressed(BTN_A)) || (br.demo && br.t > 240)) { audio_sfx(SFX_SELECT); leave_scene(); }
        break;
    case BR_DEAD:
        if (br.demo && br.t > 90) { leave_scene(); return; }
        if (br.t < 20) break;
        if (btn_pressed(BTN_UP) || btn_pressed(BTN_DOWN)) { br.sel ^= 1; audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_A)) {
            audio_sfx(SFX_SELECT);
            if (br.sel == 0) br_start();
            else leave_scene();
        }
        break;
    default:
        break;
    }
}

/* ---- drawing ------------------------------------------------------------------ */
static int style_now(void) { return combat_s.hell >= 128 ? WIN_HELL : WIN_CUTE; }

static void intro_draw(void)
{
    const LevelInfo *L = &lv_info[br.level];
    int sw = plat_screen_w(), sh = plat_screen_h(), t = br.t, hell = L->theme == WIN_HELL || L->theme == WIN_DARK;
    char buf[48], tb[12];
    plat_shade_rect(0, 0, sw, sh, t < 8 ? 8 - t / 2 : 4);
    lvd_banner(sh / 3, L->tag, L->title, t, hell);
    if (t > 14) {
        int w = ui_text_width(L->flavor) + 16, x = sw / 2 - w / 2, y = sh / 3 + (lvd_big() ? 26 : 20);
        ui_round_rect(x + 1, y + 2, w, 13, 6, PAL_BLACK);
        ui_round_rect(x, y, w, 13, 6, PAL_UI_CREAM);
        ui_text(x + 8, y + 3, L->flavor, PAL_UI_PINK_D);
    }
    if (t > 24) {
        int y = sh / 3 + (lvd_big() ? 46 : 38);
        lvd_time(tb, sizeof(tb), br.mode == LVM_TIME_TRIAL ? L->dev : L->par);
        doomo_snprintf(buf, sizeof(buf), br.mode == LVM_TIME_TRIAL ? STR_ICON_BOLT " Gold time %s" : STR_ICON_STAR " Par time %s", tb);
        ui_text_outline(sw / 2 - ui_text_width(buf) / 2, y, buf, PAL_UI_GOLD, PAL_BLACK);
        if (br.mode != LVM_NORMAL) {
            doomo_snprintf(buf, sizeof(buf), "- %s -", lv_mode_names[br.mode]);
            ui_text_outline(sw / 2 - ui_text_width(buf) / 2, y + 11, buf,
                            br.mode == LVM_BRUTAL_ONLY ? PAL_UI_RED : PAL_WHITE, PAL_BLACK);
        }
        if (lv_info[br.level].boss != LB_NONE && lvd_big()) {
            /* the mini-boss waiting at the end, in the corner */
            int boss = lv_info[br.level].boss;
            const LevelBoss *b = &lv_boss[boss];
            const DoomoImage *img = sprite_image(b->spr, 0);
            int h = boss == LB_GRUMBLEGUTS ? 66 : img ? img->h : 40, x = sw - 44;
            lvd_boss_figure(boss, x, sh - 18, t, 1, 1);
            ui_text_outline(x - ui_text_width("waiting:") / 2, sh - 30 - h, "waiting:", PAL_GRAY_L, PAL_BLACK);
        }
    }
    if (br.demo) ui_hint("DEMO  " STR_ICON_A " Stop", WIN_DARK);
    else if (t > 12) ui_hint(STR_ICON_A " GO!", hell ? WIN_HELL : WIN_CUTE);
}

static void hud_extras(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    char buf[24];
    int secs = (int)(combat_stats()->ticks / DOOMO_TICK_HZ);
    if (br.mode == LVM_TIME_TRIAL) {
        const LevelInfo *L = &lv_info[br.level];
        int sc = 2, w;
        uint8_t col = secs <= L->dev ? PAL_UI_GOLD : secs <= L->par ? PAL_WHITE : PAL_UI_RED;
        int cs = (int)(combat_stats()->ticks % DOOMO_TICK_HZ) * 10 / DOOMO_TICK_HZ;
        /* under the boss bar, or beside / below the map title while it shows */
        int y = lv_run.bar >= 0 ? lvd_bar_y() + 11 : (!lvd_big() && combat_stats()->ticks < 150) ? 24 : 6;
        doomo_snprintf(buf, sizeof(buf), "%d:%02d.%d", secs / 60, secs % 60, cs);
        w = ui_text_big_width(buf, sc);
        ui_text_big(sw / 2 - w / 2, y, buf, sc, col, PAL_BLACK);
        if (secs <= L->dev) ui_text_outline(sw / 2 - ui_text_width("GOLD PACE") / 2, y + 18,
                                            "GOLD PACE", (br.t & 8) ? PAL_UI_GOLD : PAL_WHITE, PAL_BLACK);
    } else if (!br.demo) {
        lvd_time(buf, sizeof(buf), secs);
        ui_text_outline(sw / 2 - ui_text_width(buf) / 2, sh - 12, buf, secs <= lv_info[br.level].par ? PAL_WHITE : PAL_GRAY_L, PAL_BLACK);
    }
    if (br_challenge()) {
        uint8_t c = br.mode == LVM_CUTE_ONLY ? PAL_UI_PINK : PAL_UI_RED_D;
        const char *icon = br.mode == LVM_CUTE_ONLY ? STR_ICON_HEART : STR_ICON_SKULL;
        if (lvd_big()) {
            const char *s = br.mode == LVM_CUTE_ONLY ? STR_ICON_HEART " CUTE ONLY" : STR_ICON_SKULL " BRUTAL ONLY";
            int w = ui_text_width(s) + 10, x = sw / 2 - w / 2, y = sh - 26;
            ui_round_rect(x, y, w, 11, 5, c);
            ui_text(x + 5, y + 2, s, PAL_WHITE);
        } else {
            /* the HUD badges leave no room here: a round rule badge by the clock */
            int x, y = sh - 9 + ui_wave(br.t, 30, 1);
            lvd_time(buf, sizeof(buf), secs);
            x = sw / 2 - ui_text_width(buf) / 2 - 9;
            ui_fill_circle(x, y, 6, PAL_BLACK);
            ui_fill_circle(x, y, 5, c);
            ui_text(x - ui_text_width(icon) / 2, y - 3, icon, PAL_WHITE);
        }
    }
    if (br.demo && (br.t & 16)) {
        int y = lvd_big() ? sh - 26 : (lv_run.bar >= 0 ? 24 : 26);      /* clear of the HUD badges */
        ui_round_rect(sw / 2 - 30, y, 60, 12, 5, PAL_BLACK);
        ui_text(sw / 2 - ui_text_width("DEMO") / 2, y + 2, "DEMO", PAL_UI_GOLD);
    }
}

static void pause_draw(void)
{
    static const char *const names[BP_COUNT] = { "Resume", "Weapon", "Camera", "Restart level", "Quit level" };
    int sw = plat_screen_w(), sh = plat_screen_h(), style = style_now();
    int w = lvd_big() ? 200 : 176, h = BP_COUNT * 17 + 30, x = sw / 2 - w / 2, y = sh / 2 - h / 2 + 4, i;
    char buf[40];
    plat_shade_rect(0, 0, sw, sh, 7);
    if (!ui_window_pop(x, y, w, h, style, br.t)) return;
    ui_title(sw / 2, y - 6, "PAUSED", style);
    for (i = 0; i < BP_COUNT; i++) {
        const char *label = names[i];
        if (i == BP_WEAPON) {
            int k;
            weld_name(combat_weapon(), buf, sizeof(buf));
            if (ui_text_width(buf) > w - 40) {
                /* too long for the button: trim it with an ellipsis */
                for (k = (int)strlen(buf); k > 3 && ui_text_width(buf) > w - 40; k--) {
                    buf[k - 3] = '.';
                    buf[k - 2] = '.';
                    buf[k - 1] = 0;
                }
            }
            label = buf;
        } else if (i == BP_CAMERA) {
            label = doomo_player.camera == CAM_FIRST_PERSON ? "Camera: 1st person" : "Camera: 3rd person";
        }
        ui_button(x + 14, y + 12 + i * 17, w - 28, label, i == br.sel, style);
        if (i == br.sel && (i == BP_WEAPON || i == BP_CAMERA)) {
            ui_text(x + 6 + ui_wave(br.t, 16, 1), y + 15 + i * 17, STR_ICON_ARROW_L, PAL_UI_GOLD);
            ui_text(x + w - 12 - ui_wave(br.t, 16, 1), y + 15 + i * 17, STR_ICON_ARROW_R, PAL_UI_GOLD);
        }
    }
    {
        const LevelInfo *L = &lv_info[br.level];
        char tb[12];
        lvd_time(tb, sizeof(tb), (int)(combat_stats()->ticks / DOOMO_TICK_HZ));
        doomo_snprintf(buf, sizeof(buf), "%s  %s", L->title, tb);
        ui_text_center(sw / 2, y + h - 12, buf, ui_style(style)->text_dim);
    }
    ui_hint(STR_ICON_A " Select  " STR_ICON_B " Back", style);
}

/* one tally row: label, value text, points (counting up) */
static void tally(int x, int y, int w, const char *label, const char *value, int pts, int max_pts, int t, int delay)
{
    char buf[40];
    int shown;
    if (t < delay) return;
    shown = DOOMO_MIN(pts, (t - delay) * 2);
    doomo_snprintf(buf, sizeof(buf), "%s  +%d", value, shown);
    ui_value_row(x, y, w, label, buf, 0);
    ui_bar(x + 4, y + 10, w - 8, 2, shown, max_pts, shown == max_pts ? PAL_UI_GOLD : PAL_UI_MINT);
}

static void done_draw(void)
{
    const DoomoCombatStats *s = combat_stats();
    const LevelInfo *L = &lv_info[br.level];
    int sw = plat_screen_w(), sh = plat_screen_h(), big = lvd_big(), style = style_now();
    int w = big ? 280 : 228, h = big ? 176 : 132, x = sw / 2 - w / 2, y = sh / 2 - h / 2 + (big ? 8 : 6);
    int secs = (int)(s->ticks / DOOMO_TICK_HZ), t = br.t, row = big ? 15 : 13, rx = x + 6, rw = w - (big ? 84 : 66);
    char v[32], tb[12], pb[12];
    plat_shade_rect(0, 0, sw, sh, 6);
    ui_text_big(sw / 2 - ui_text_big_width("CLEARED!", 2) / 2, y - 20 + ui_wave(t, 30, 2), "CLEARED!", 2,
                style == WIN_HELL ? PAL_RAMP(RAMP_LAVA, 7) : PAL_UI_GOLD, style == WIN_HELL ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_UI_PINK_D);
    if (!ui_window_pop(x, y, w, h, style, t)) return;
    ui_text_center(sw / 2, y + 5, L->title, ui_style(style)->text);
    plat_rect(x + 12, y + 14, w - 24, 1, ui_style(style)->fill_lo);
    lvd_time(tb, sizeof(tb), secs);
    lvd_time(pb, sizeof(pb), br.mode == LVM_TIME_TRIAL ? L->dev : L->par);
    doomo_snprintf(v, sizeof(v), "%s / %s", tb, pb);
    tally(rx, y + 17, rw, "Time", v, br.sc.time_pts, 40, t, 10);
    doomo_snprintf(v, sizeof(v), "%d/%d", s->kills, s->kills_total);
    tally(rx, y + 17 + row, rw, "Demons", v, br.sc.kill_pts, 35, t, 30);
    doomo_snprintf(v, sizeof(v), "%d/%d", s->secrets, s->secrets_total);
    tally(rx, y + 17 + row * 2, rw, "Secrets", v, br.sc.secret_pts, 25, t, 50);
    if (t > 70) {
        doomo_snprintf(v, sizeof(v), "%d / 100", br.sc.total);
        ui_value_row(rx, y + 17 + row * 3, rw, "Score", v, 1);
    }
    /* the stamp slams in */
    if (t > 82) {
        int cx = x + w - (big ? 40 : 31), cy = y + (big ? 58 : 50);
        ui_text_center(cx, cy - (big ? 38 : 33), "RANK", ui_style(style)->text_dim);
        lvd_rank_stamp(cx, cy, br.sc.rank, t - 82, 1);
        if ((br.news & LVR_NEW_RANK) && t > 96 && !(br.news & LVR_FIRST_CLEAR))
            ui_text_center(cx, cy + (big ? 28 : 25), "NEW BEST!", (t & 8) ? PAL_UI_GOLD : PAL_WHITE);
    }
    if (t > 100) {
        const char *msg = br.demo ? "Demo run - nothing recorded" :
                          (br.news & LVR_GOLD) ? STR_ICON_STAR " GOLD MEDAL! " STR_ICON_STAR :
                          (br.news & LVR_CHALLENGE) ? STR_ICON_HEART " CHALLENGE CLEARED!" :
                          (br.news & LVR_FIRST_CLEAR) ? STR_ICON_STAR " FIRST CLEAR!" :
                          br.mode == LVM_TIME_TRIAL ? "So close to gold!" : "";
        int my = y + h - (big ? 40 : 30);
        if (msg[0]) ui_text_center(sw / 2, my, msg, (t & 8) ? PAL_UI_GOLD : ui_style(style)->text);
        if (!br.demo) {
            doomo_snprintf(v, sizeof(v), STR_ICON_COIN " +%d H$ bonus  +%d H$ loot", br.bonus, s->money);
            ui_text_center(sw / 2, my + 11, v, ui_style(style)->text_dim);
        }
    }
    if (t > 110 && big) {
        int k;
        for (k = 0; k < 4; k++)
            ui_sparkle(x + 14 + k * (w - 28) / 3, y + h - 6 + ui_wave(t + k * 9, 24, 2), (t + k * 4) & 15, PAL_UI_GOLD);
    }
    if (t > 150) ui_hint(STR_ICON_A " Continue", style);
}

static void dead_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), style = WIN_HELL;
    int w = 180, h = 60, x = sw / 2 - w / 2, y = sh / 2 - 6;
    const char *title = combat_gore_style() == WSTYLE_CUTE ? "CUDDLED TO DEATH" : "YOU DIED";
    char buf[40];
    plat_shade_rect(0, 0, sw, sh, 5);
    ui_text_big(sw / 2 - ui_text_big_width(title, 2) / 2, y - 40 + ui_wave(br.t, 40, 2), title, 2, PAL_UI_RED, PAL_RAMP(RAMP_BLOOD, 0));
    doomo_snprintf(buf, sizeof(buf), "%s - %d demons down", lv_info[br.level].title, combat_stats()->kills);
    ui_text_outline(sw / 2 - ui_text_width(buf) / 2, y - 18, buf, PAL_GRAY_L, PAL_BLACK);
    if (br.t < 10 || !ui_window_pop(x, y, w, h, style, br.t - 10)) return;
    ui_button(x + 14, y + 10, w - 28, "Try again", br.sel == 0, style);
    ui_button(x + 14, y + 32, w - 28, "Give up", br.sel == 1, style);
    ui_hint(STR_ICON_A " Select", style);
}

static void br_draw(void)
{
    DoomoView3D v;
    combat_build_view(&v);
    lv_runtime_view(&v);
    plat_view3d(&v);
    if (plat_draw_skipped()) return;
    if (br.state == BR_INTRO) { intro_draw(); return; }
    lv_runtime_hud();
    if (br.state == BR_PLAY || br.state == BR_PAUSE) hud_extras();
    switch (br.state) {
    case BR_PAUSE: pause_draw(); break;
    case BR_DONE: done_draw(); break;
    case BR_DEAD: if (br.t > 10) dead_draw(); break;
    default: break;
    }
}

const DoomoScene SCENE_BREACH = { "BREACH", br_enter, br_leave, br_update, br_draw, 0 };
