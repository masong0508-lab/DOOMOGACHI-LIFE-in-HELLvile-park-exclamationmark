/* arena.c - SCENE_ARENA: free play survival in the Demon Arena (owner:
 * action). Endless waves with scaling demon mixes (new types join as the
 * waves climb, a mini-boss every fifth wave, Lord Vapula on wave 15 and
 * every tenth after), spawning in vape puffs at the portals; short breaks
 * between waves with refilled pickups and a wave bonus; score with combo
 * multipliers; rewards (H$, scrap, gears, demon horns) and arena-wave
 * unlocks when you fall. The shooter, HUD and options are the regular
 * SCENE_COMBAT machinery.
 */
#include "combat_int.h"

enum { AR_INTRO, AR_FIGHT, AR_BREAK, AR_OVER, AR_PAUSE };

static int ar_state, ar_t, ar_wave, ar_to_spawn, ar_spawn_t, ar_sel;
static int ar_best, ar_rewarded;
static uint16_t ar_money;
static uint8_t spawn_x[16], spawn_y[16];
static int nspawn;

static void find_spawns(void)
{
    const DoomoMapDef *d = doomo_map.def;
    int i;
    nspawn = 0;
    if (d && d->things)
        for (i = 0; i < d->thing_count && nspawn < 16; i++)
            if (d->things[i].type == THING_SPAWNER || d->things[i].type == THING_VAPE_PORTAL) {
                spawn_x[nspawn] = d->things[i].x;
                spawn_y[nspawn] = d->things[i].y;
                nspawn++;
            }
    if (!nspawn) {
        /* any open cell away from the player */
        for (i = 0; i < doomo_map.w * doomo_map.h && nspawn < 8; i += 37)
            if (!doomo_map.cells[i].wall) {
                spawn_x[nspawn] = (uint8_t)(i % doomo_map.w);
                spawn_y[nspawn] = (uint8_t)(i / doomo_map.w);
                nspawn++;
            }
    }
}

static int pick_type(int wave)
{
    static const uint8_t pool[] = { THING_IMPLING, THING_HELLHOUND_PUP, THING_PUFF_DEMON,
                                    THING_BONE_JESTER, THING_SLIME_BELCHER, THING_GARGOYLE,
                                    THING_CYBER_PIGGY, THING_CLOUD_SPIDER, THING_VAPE_WRAITH,
                                    THING_BRUTE };
    int avail = DOOMO_CLAMP(1 + wave * 2 / 3, 1, (int)sizeof(pool));
    int r = rng_range(&doomo_rng, 100);
    /* weight toward the cheap ones */
    int k = r < 45 ? rng_range(&doomo_rng, DOOMO_MIN(avail, 2))
                   : r < 80 ? rng_range(&doomo_rng, DOOMO_MIN(avail, 5)) : rng_range(&doomo_rng, avail);
    return pool[k];
}

static void respawn_pickups(void)
{
    const DoomoMapDef *d = doomo_map.def;
    int i, k;
    if (!d || !d->things) return;
    for (i = 0; i < d->thing_count; i++) {
        const DoomoThing *t = &d->things[i];
        int have = 0;
        if (t->type < THING_HEALTH_SMALL || t->type > THING_CANDY) continue;
        for (k = 0; k < ACTOR_MAX; k++) {
            const DoomoActor *a = &doomo_actors[k];
            if ((a->flags & AF_PICKUP) && a->type == t->type &&
                FIX2INT(a->x) / CELLU == t->x && FIX2INT(a->y) / CELLU == t->y) { have = 1; break; }
        }
        if (!have) {
            k = actor_spawn_thing(t, 0);
            if (k >= 0) actor_fx(SPR_SPARKLE, doomo_actors[k].x, doomo_actors[k].y, doomo_actors[k].z + INT2FIX(12), 3);
        }
    }
}

static void start_wave(void)
{
    ar_wave++;
    ar_to_spawn = 3 + ar_wave * 2;
    if (opt_get(OPT_DEMON_DENSITY) == 0) ar_to_spawn = ar_to_spawn * 2 / 3;
    if (opt_get(OPT_DEMON_DENSITY) == 2) ar_to_spawn = ar_to_spawn * 3 / 2;
    ar_spawn_t = 20;
    ar_state = AR_FIGHT;
    ar_t = 0;
    audio_sfx(SFX_DEMON_ROAR);
    /* bosses */
    if (ar_wave % 5 == 0 && nspawn) {
        int s = rng_range(&doomo_rng, nspawn);
        int type = (ar_wave == 15 || (ar_wave > 15 && ar_wave % 10 == 0)) ? THING_LORD_VAPULA : THING_VAPE_WRAITH;
        int a = actor_spawn_demon(type, INT2FIX(spawn_x[s] * CELLU + CELLU / 2),
                                  INT2FIX(spawn_y[s] * CELLU + CELLU / 2), 1, THF_BOSS, 0);
        if (a >= 0) {
            combat_s.boss = a;
            combat_s.boss_max = doomo_actors[a].hp;
            combat_toast(type == THING_LORD_VAPULA ? "LORD VAPULA ENTERS THE ARENA!" : "A VAPE WRAITH APPEARS!");
        }
    }
}

static void ar_enter(int arg)
{
    (void)arg;
    combat_start_ex(MAPID_ARENA, CGOAL_ENDLESS);
    combat_refill_ammo(100);
    doomo_player.armor = 50;
    find_spawns();
    ar_state = AR_INTRO;
    ar_t = 0;
    ar_wave = 0;
    ar_sel = 0;
    ar_rewarded = 0;
    ar_money = 0;
    audio_music(SONG_INVASION);
}

static void ar_leave(void)
{
    combat_s.active = 0;
}

static void give_rewards(void)
{
    int waves = DOOMO_MAX(ar_wave - 1, 0);
    if (ar_rewarded) return;
    ar_rewarded = 1;
    if (waves > ar_best) ar_best = waves;
    progress_add(PROG_ARENA_WAVES, waves);
    ar_money = (uint16_t)DOOMO_MIN(combat_st.score / 4 + waves * 25, 60000);
    doomo_town.money += ar_money;
    progress_add(PROG_MONEY_EARNED, ar_money);
    inv_add(ITEM_SCRAP, 1 + waves);
    if (waves >= 3) inv_add(ITEM_GEARS, waves / 3);
    if (waves >= 5) inv_add(ITEM_DEMON_HORN, waves / 5);
    if (waves >= 8) inv_add(ITEM_HELLSTEEL, 1);
}

static void ar_update(void)
{
    ar_t++;
    switch (ar_state) {
    case AR_INTRO:
        combat_tick();
        if (ar_t > 60) start_wave();
        return;
    case AR_PAUSE:
        if (btn_pressed(BTN_UP) || btn_pressed(BTN_DOWN)) { ar_sel = (ar_sel + 1) % 3; audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_START) || btn_pressed(BTN_B)) { ar_state = AR_FIGHT; return; }
        if (btn_pressed(BTN_A)) {
            if (ar_sel == 0) ar_state = AR_FIGHT;
            else if (ar_sel == 1) combat_toggle_camera();
            else { give_rewards(); ar_state = AR_OVER; ar_t = 0; }
        }
        return;
    case AR_OVER:
        if (ar_t > 60 && btn_pressed(BTN_A)) {
            audio_sfx(SFX_SELECT);
            if (scene_depth() > 1) scene_pop(); else scene_set(&SCENE_TITLE, 0);
        }
        return;
    default:
        break;
    }
    if (btn_pressed(BTN_START) && !combat_autopilot_on()) { ar_state = AR_PAUSE; ar_sel = 0; return; }
    combat_tick();
    if (combat_finished() == -1) {
        give_rewards();
        ar_state = AR_OVER;
        ar_t = 0;
        audio_music(SONG_SAD);
        return;
    }
    if (ar_state == AR_FIGHT) {
        /* trickle the wave in, a few at a time */
        if (ar_to_spawn > 0 && --ar_spawn_t <= 0 && nspawn && combat_demons_alive() < 14) {
            int s = rng_range(&doomo_rng, nspawn);
            int fl, ce;
            fix16 x = INT2FIX(spawn_x[s] * CELLU + CELLU / 2), y = INT2FIX(spawn_y[s] * CELLU + CELLU / 2);
            if (fx_dist(x - doomo_player.x, y - doomo_player.y) > INT2FIX(2 * CELLU) &&
                cw_fits(x, y, 24, cw_floor(spawn_x[s], spawn_y[s]), 60, 24, &fl, &ce)) {
                int a = actor_spawn_demon(pick_type(ar_wave), x, y, 1, 0, 0);
                if (a >= 0) ar_to_spawn--;
            }
            ar_spawn_t = DOOMO_MAX(8, 30 - ar_wave * 2);
        }
        if (ar_to_spawn <= 0 && combat_demons_alive() == 0) {
            ar_state = AR_BREAK;
            ar_t = 0;
            combat_st.score += (uint32_t)ar_wave * 100;
            progress_add(PROG_ARENA_WAVES, 0);
            respawn_pickups();
            if (doomo_player.hp < 100) doomo_player.hp = (int16_t)DOOMO_MIN(doomo_player.hp + 20, 100);
            combat_toast("WAVE CLEARED! " STR_ICON_STAR);
            audio_sfx(SFX_EVOLVE);
        }
    } else if (ar_state == AR_BREAK && ar_t > 120) {
        start_wave();
    }
}

static void banner(const char *text, int t)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    int s = ui_pop_scale(t), sc = s > 280 ? 3 : 2;
    if (t > 70 && (t & 2)) return;
    ui_text_big(sw / 2 - ui_text_big_width(text, sc) / 2, sh / 4, text, sc, PAL_RAMP(RAMP_LAVA, 7), PAL_RAMP(RAMP_BLOOD, 0));
}

static void ar_draw(void)
{
    DoomoView3D v;
    int sw = plat_screen_w(), sh = plat_screen_h();
    char buf[40];
    combat_build_view(&v);
    plat_view3d(&v);
    combat_draw_hud();
    /* wave counter under the score */
    doomo_snprintf(buf, sizeof(buf), "WAVE %d", ar_wave);
    ui_text_outline(sw - ui_text_width(buf) - 4, sh / 2 - 50, buf, PAL_RAMP(RAMP_BONE, 7), PAL_RAMP(RAMP_BLOOD, 0));
    switch (ar_state) {
    case AR_INTRO:
        banner("DEMON ARENA", ar_t);
        break;
    case AR_FIGHT:
        if (ar_t < 80) {
            doomo_snprintf(buf, sizeof(buf), "WAVE %d", ar_wave);
            banner(buf, ar_t);
        }
        break;
    case AR_BREAK:
        doomo_snprintf(buf, sizeof(buf), "Next wave in %d", (120 - ar_t) / 30 + 1);
        ui_text_outline(sw / 2 - ui_text_width(buf) / 2, sh / 4 + 24, buf, PAL_UI_GOLD, PAL_RAMP(RAMP_BLOOD, 0));
        break;
    case AR_PAUSE: {
        int w = 150, h = 56, x = sw / 2 - w / 2, y = sh / 2 - h / 2;
        plat_shade_rect(0, 0, sw, sh, 7);
        ui_window(x, y, w, h, WIN_HELL);
        ui_button(x + 10, y + 6, w - 20, "Resume", ar_sel == 0, WIN_HELL);
        ui_button(x + 10, y + 21, w - 20, doomo_player.camera == CAM_FIRST_PERSON ? "Camera: 1st" : "Camera: 3rd", ar_sel == 1, WIN_HELL);
        ui_button(x + 10, y + 36, w - 20, "Give up", ar_sel == 2, WIN_HELL);
        break;
    }
    case AR_OVER: {
        int w = sw > 260 ? 220 : 200, h = 104, x = sw / 2 - w / 2, y = sh / 2 - h / 2 + 6;
        int waves = DOOMO_MAX(ar_wave - 1, 0);
        plat_shade_rect(0, 0, sw, sh, 6);
        ui_text_big(sw / 2 - ui_text_big_width("ARENA OVER", 2) / 2, y - 20, "ARENA OVER", 2, PAL_UI_RED, PAL_RAMP(RAMP_BLOOD, 0));
        if (!ui_window_pop(x, y, w, h, WIN_HELL, ar_t)) break;
        doomo_snprintf(buf, sizeof(buf), "%d", waves);
        ui_value_row(x + 6, y + 8, w - 12, "Waves survived", buf, 0);
        doomo_snprintf(buf, sizeof(buf), "%d", (int)combat_st.score);
        ui_value_row(x + 6, y + 19, w - 12, "Score", buf, 0);
        doomo_snprintf(buf, sizeof(buf), "%d", combat_st.kills);
        ui_value_row(x + 6, y + 30, w - 12, "Demons", buf, 0);
        doomo_snprintf(buf, sizeof(buf), "x%d", combat_st.best_combo);
        ui_value_row(x + 6, y + 41, w - 12, "Best combo", buf, 0);
        doomo_snprintf(buf, sizeof(buf), "%d", ar_best);
        ui_value_row(x + 6, y + 52, w - 12, "Best this visit", buf, 0);
        doomo_snprintf(buf, sizeof(buf), "+%d H$ +scrap", ar_money);
        ui_value_row(x + 6, y + 66, w - 12, STR_ICON_COIN " Rewards", buf, 1);
        doomo_snprintf(buf, sizeof(buf), "%d arena waves total", (int)progress_get(PROG_ARENA_WAVES));
        ui_text_center(sw / 2, y + 82, buf, ui_style(WIN_HELL)->text_dim);
        if (ar_t > 60) ui_hint(STR_ICON_A " Continue", WIN_HELL);
        break;
    }
    default:
        break;
    }
}

const DoomoScene SCENE_ARENA = { "ARENA", ar_enter, ar_leave, ar_update, ar_draw, 0 };
