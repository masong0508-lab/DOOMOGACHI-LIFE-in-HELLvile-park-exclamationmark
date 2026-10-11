/* arena_modes.c - SCENE_ARENA_MODE: the extra arena modes (owner: levels).
 * arg = AM_* (levels_int.h). All three run in the Demon Arena map on top
 * of a combat session and the levels runtime (mini-boss variants and
 * their mechanics):
 *
 *  BOSS RUSH  every boss and mini-boss of the Hell Breach levels back to
 *             back (the Cellar Wraith, the five new mini-bosses, Lord
 *             Vapula), a short breather with a heal between rounds, a
 *             final time; Boss Rush trophies.
 *  HORDE      endless escalating waves (new demon types join, a mini-boss
 *             every 5th wave), and every 3rd wave a SHOP BREAK where the
 *             loot you collected buys candy hearts, plush armor, ammo, a
 *             Guardian Angel (revives you once) or Holy Fury (untouchable
 *             for the start of the next wave). The best wave is saved.
 *  MONSTER MODE MAYHEM  (unlocked by turning Monster Mode on) three
 *             minutes of chaos: tiny, big and huge demons pour in,
 *             mini-bosses drop by, ammo never runs out, and every 25 kills
 *             the MONSTER METER sends you berserk (untouchable, x2 score).
 */
#include "levels_int.h"

enum { AMS_INTRO, AMS_FIGHT, AMS_BREAK, AMS_SHOP, AMS_PAUSE, AMS_OVER, AMS_WIN };
enum { SHOP_HEART, SHOP_ARMOR, SHOP_AMMO, SHOP_ANGEL, SHOP_FURY, SHOP_GO, SHOP_COUNT };

#define RUSH_ROUNDS 7
#define MAYHEM_TICKS (180 * DOOMO_TICK_HZ)
#define FURY_TICKS (15 * DOOMO_TICK_HZ)
#define BERSERK_TICKS (10 * DOOMO_TICK_HZ)
#define METER_FULL 25

static const uint8_t rush_lineup[RUSH_ROUNDS] = {
    LB_CELLAR_WRAITH, LB_GUTTERKING, LB_FLUFFNUTS, LB_WIDOW, LB_BONEBOTTOM, LB_GRUMBLEGUTS, LB_VAPULA
};
static const char *const shop_names[SHOP_COUNT] = {
    "Candy Heart  +50 HP", "Plush Armor  +50", "Ammo Crate  (full)", "Guardian Angel",
    "Holy Fury  (15 s)", "Next wave!"
};
static const char *const shop_descs[SHOP_COUNT] = {
    "A heart-shaped candy. Tastes like healing.",
    "Stuffed with love and a little kevlar.",
    "Every ammo pouch, filled to the brim.",
    "A plush angel catches you once if you fall.",
    "Untouchable for the first 15 s of the next wave.",
    "Break's over. Here they come!"
};
static const uint8_t shop_cost[SHOP_COUNT] = { 40, 60, 50, 150, 120, 0 };
static const char *const mode_titles[AM_COUNT] = { "BOSS RUSH", "HORDE MODE", "MONSTER MAYHEM" };

static struct {
    uint8_t  mode, state, prev_state, round, sel, angel, fury_armed, rewarded;
    uint8_t  nspawn, spawn_x[16], spawn_y[16], boss_spawned, boss_cycle;
    uint16_t t, wave, to_spawn, spawn_t, meter, berserk_t, fury_t, last_kills;
    uint32_t start_tick, end_tick;
    uint16_t spent, payout;
    int16_t  hp0;
} am;

/* ---- helpers ----------------------------------------------------------------- */
static void find_spawns(void)
{
    const DoomoMapDef *d = doomo_map.def;
    int i;
    am.nspawn = 0;
    if (d && d->things)
        for (i = 0; i < d->thing_count && am.nspawn < 16; i++)
            if (d->things[i].type == THING_SPAWNER || d->things[i].type == THING_VAPE_PORTAL) {
                am.spawn_x[am.nspawn] = d->things[i].x;
                am.spawn_y[am.nspawn++] = d->things[i].y;
            }
    if (!am.nspawn) {
        am.spawn_x[0] = (uint8_t)(doomo_map.w / 2);
        am.spawn_y[0] = (uint8_t)(doomo_map.h / 4);
        am.nspawn = 1;
    }
}

static int far_spawn(void)
{
    int i, best = 0;
    fix16 bd = -1;
    for (i = 0; i < am.nspawn; i++) {
        fix16 d = fx_dist(INT2FIX(am.spawn_x[i] * CELLU) - doomo_player.x, INT2FIX(am.spawn_y[i] * CELLU) - doomo_player.y);
        if (d > bd) { bd = d; best = i; }
    }
    return best;
}

static int coins(void) { return DOOMO_MAX(0, (int)combat_st.money - (int)am.spent); }

static void respawn_pickups(void)
{
    const DoomoMapDef *d = doomo_map.def;
    int i, k;
    if (!d || !d->things) return;
    for (i = 0; i < d->thing_count; i++) {
        const DoomoThing *t = &d->things[i];
        int have = 0;
        if (t->type < THING_HEALTH_SMALL || t->type > THING_CANDY) continue;
        for (k = 0; k < ACTOR_MAX && !have; k++) {
            const DoomoActor *a = &doomo_actors[k];
            have = (a->flags & AF_PICKUP) && a->type == t->type &&
                   FIX2INT(a->x) / CELLU == t->x && FIX2INT(a->y) / CELLU == t->y;
        }
        if (!have && (k = actor_spawn_thing(t, 0)) >= 0)
            actor_fx(SPR_SPARKLE, doomo_actors[k].x, doomo_actors[k].y, doomo_actors[k].z + INT2FIX(12), 3);
    }
}

static int pick_type(int wave)
{
    static const uint8_t pool[] = { THING_IMPLING, THING_HELLHOUND_PUP, THING_PUFF_DEMON, THING_SLIME_BELCHER,
                                    THING_BONE_JESTER, THING_GARGOYLE, THING_VAPE_WRAITH, THING_CYBER_PIGGY,
                                    THING_CLOUD_SPIDER, THING_BRUTE };
    int avail = DOOMO_CLAMP(2 + wave * 2 / 3, 2, (int)sizeof(pool)), r = rng_range(&doomo_rng, 100);
    int k = r < 40 ? rng_range(&doomo_rng, 2) : r < 75 ? rng_range(&doomo_rng, DOOMO_MIN(avail, 5)) : rng_range(&doomo_rng, avail);
    return pool[k];
}

static int spawn_demon(int type)
{
    int s = rng_range(&doomo_rng, am.nspawn), fl, ce, a;
    fix16 x = INT2FIX(am.spawn_x[s] * CELLU + CELLU / 2), y = INT2FIX(am.spawn_y[s] * CELLU + CELLU / 2);
    if (fx_dist(x - doomo_player.x, y - doomo_player.y) < INT2FIX(2 * CELLU)) return -1;
    if (!cw_fits(x, y, 24, cw_floor(am.spawn_x[s], am.spawn_y[s]), 60, 24, &fl, &ce)) return -1;
    a = actor_spawn_demon(type, x, y, 1, 0, 0);
    if (a >= 0) {
        doomo_actors[a].target = TARGET_PLAYER;
        lv_run.size_cls[a] = LVS_NONE;
    }
    return a;
}

static void spawn_variant(int boss, int hp8)
{
    int s = far_spawn();
    if (lv_spawn_boss(boss, am.spawn_x[s], am.spawn_y[s], hp8) >= 0) am.boss_spawned = 1;
}

static void heal(int n)
{
    if (doomo_player.hp > 0) doomo_player.hp = (int16_t)DOOMO_MIN(doomo_player.hp + n, 100);
}

/* ---- enter / leave ---------------------------------------------------------------- */
static void am_start(void)
{
    combat_start_ex(MAPID_ARENA, CGOAL_ENDLESS);
    combat_refill_ammo(100);
    doomo_player.armor = 50;
    lv_runtime_begin(255);
    find_spawns();
    am.state = AMS_INTRO;
    am.t = 0;
    am.round = 0;
    am.wave = 0;
    am.to_spawn = 0;
    am.meter = 0;
    am.berserk_t = am.fury_t = 0;
    am.angel = am.fury_armed = 0;
    am.spent = am.payout = 0;
    am.rewarded = 0;
    am.boss_spawned = 0;
    am.boss_cycle = 0;
    am.last_kills = 0;
    am.start_tick = 0;
    audio_music(am.mode == AM_BOSS_RUSH ? SONG_BOSS : SONG_INVASION);
}

static void am_enter(int arg)
{
    memset(&am, 0, sizeof(am));
    am.mode = (uint8_t)(arg >= 0 && arg < AM_COUNT ? arg : AM_HORDE);
    lv_check_monster();
    am_start();
}

static void am_leave(void)
{
    combat_s.active = 0;
    combat_set_result_quit();
}

static void leave_scene(void)
{
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_TITLE, 0);
}

/* ---- results ---------------------------------------------------------------------- */
static void give_rewards(int won)
{
    int waves = am.mode == AM_HORDE ? DOOMO_MAX((int)am.wave - (am.state == AMS_FIGHT ? 1 : 0), 0) : 0;
    uint32_t secs;
    if (am.rewarded) return;
    am.rewarded = 1;
    am.end_tick = combat_st.ticks;
    secs = (am.end_tick - am.start_tick) / DOOMO_TICK_HZ;
    switch (am.mode) {
    case AM_BOSS_RUSH:
        am.payout = (uint16_t)DOOMO_MIN(coins() + am.round * 120 + (won ? 1000 : 0), 60000);
        if (won) {
            unlock_grant(LVU_BOSS_RUSH);
            if (secs < 7 * 60) unlock_grant(LVU_BOSS_RUSH_FAST);
            inv_add(ITEM_HELLSTEEL, 3);
            inv_add(ITEM_DEMON_HORN, 2);
        }
        break;
    case AM_HORDE:
        am.payout = (uint16_t)DOOMO_MIN(coins() + waves * 25, 60000);
        inv_add(ITEM_SCRAP, 1 + waves);
        if (waves >= 3) inv_add(ITEM_GEARS, waves / 3);
        if (waves >= 8) inv_add(ITEM_HELLSTEEL, 1);
        break;
    default:
        am.payout = (uint16_t)DOOMO_MIN(combat_st.score / 5 + combat_st.kills * 2, 60000);
        if (combat_st.kills >= 100) unlock_grant(LVU_MAYHEM100);
        inv_add(ITEM_DEMON_HORN, 1 + combat_st.kills / 25);
        break;
    }
    doomo_town.money += am.payout;
    progress_add(PROG_MONEY_EARNED, am.payout);
}

static void game_over(int won)
{
    give_rewards(won);
    am.state = won ? AMS_WIN : AMS_OVER;
    am.t = 0;
    am.sel = 0;
    audio_music(won ? SONG_VICTORY : SONG_SAD);
}

/* ---- boss rush ------------------------------------------------------------------- */
static void rush_next(void)
{
    int boss = rush_lineup[am.round];
    spawn_variant(boss, boss == LB_VAPULA ? 5 : 6);
    am.state = AMS_FIGHT;
    am.t = 0;
}

static void rush_tick(void)
{
    if (am.state == AMS_FIGHT && !am.boss_spawned && am.t > 30) {
        rush_next();                                 /* the spawn was blocked: try again */
        return;
    }
    if (am.state == AMS_FIGHT && am.t > 40 && lv_bosses_alive(0) == 0 && am.boss_spawned) {
        am.boss_spawned = 0;
        am.round++;
        if (am.round >= RUSH_ROUNDS) { game_over(1); return; }
        am.state = AMS_BREAK;
        am.t = 0;
        heal(30);
        combat_refill_ammo(60);
        audio_sfx(SFX_EVOLVE);
    } else if (am.state == AMS_BREAK && am.t > 100) {
        rush_next();
    }
}

/* ---- horde ----------------------------------------------------------------------- */
static void horde_start_wave(void)
{
    am.wave++;
    am.to_spawn = (uint16_t)DOOMO_MIN(4 + am.wave * 2, 40);
    am.spawn_t = 20;
    am.state = AMS_FIGHT;
    am.t = 0;
    if (am.fury_armed) { am.fury_t = FURY_TICKS; am.fury_armed = 0; combat_toast("HOLY FURY! " STR_ICON_CROSS); }
    if (am.wave % 5 == 0) {
        static const uint8_t cycle[6] = { LB_GUTTERKING, LB_FLUFFNUTS, LB_WIDOW, LB_BONEBOTTOM, LB_GRUMBLEGUTS, LB_CELLAR_WRAITH };
        spawn_variant(cycle[am.boss_cycle++ % 6], DOOMO_MIN(4 + am.wave / 5, 10));
    }
    audio_sfx(SFX_DEMON_ROAR);
}

static void horde_tick(void)
{
    if (am.state == AMS_INTRO && am.t > 60) { horde_start_wave(); return; }
    if (am.state == AMS_FIGHT) {
        if (am.to_spawn > 0 && --am.spawn_t == 0) {
            if (combat_demons_alive() < 14 && spawn_demon(pick_type(am.wave)) >= 0) am.to_spawn--;
            am.spawn_t = (uint16_t)DOOMO_MAX(8, 30 - am.wave * 2);
        }
        if (am.to_spawn == 0 && combat_demons_alive() == 0 && am.t > 30) {
            progress_add(PROG_ARENA_WAVES, 1);
            lv_set_horde_best(am.wave);
            if (am.wave >= 10) unlock_grant(LVU_HORDE10);
            if (am.wave >= 20) unlock_grant(LVU_HORDE20);
            if (am.wave >= 30) unlock_grant(LVU_HORDE30);
            combat_st.score += (uint32_t)am.wave * 100;
            heal(15);
            respawn_pickups();
            combat_toast("WAVE CLEARED! " STR_ICON_STAR);
            audio_sfx(SFX_EVOLVE);
            am.state = (am.wave % 3 == 0) ? AMS_SHOP : AMS_BREAK;
            am.t = 0;
            am.sel = 0;
        }
    } else if (am.state == AMS_BREAK && am.t > 90) {
        horde_start_wave();
    }
}

static void shop_update(void)
{
    if (btn_repeat(BTN_UP)) { am.sel = (uint8_t)((am.sel + SHOP_COUNT - 1) % SHOP_COUNT); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { am.sel = (uint8_t)((am.sel + 1) % SHOP_COUNT); audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_B) || (btn_pressed(BTN_A) && am.sel == SHOP_GO)) {
        audio_sfx(SFX_SELECT);
        am.state = AMS_BREAK;
        am.t = 60;                                   /* 1 s to get ready */
        return;
    }
    if (!btn_pressed(BTN_A)) return;
    if (coins() < shop_cost[am.sel] || (am.sel == SHOP_ANGEL && am.angel) || (am.sel == SHOP_FURY && am.fury_armed) ||
        (am.sel == SHOP_HEART && doomo_player.hp >= 100) || (am.sel == SHOP_ARMOR && doomo_player.armor >= 100)) {
        audio_sfx(SFX_ERROR);
        return;
    }
    am.spent = (uint16_t)(am.spent + shop_cost[am.sel]);
    switch (am.sel) {
    case SHOP_HEART: heal(50); break;
    case SHOP_ARMOR: doomo_player.armor = (int16_t)DOOMO_MIN(doomo_player.armor + 50, 100); break;
    case SHOP_AMMO: {
        int t;
        for (t = 0; t < AMMO_COUNT; t++) doomo_town.story.ammo[t] = (uint16_t)combat_ammo_max(t);
        break; }
    case SHOP_ANGEL: am.angel = 1; break;
    default: am.fury_armed = 1; break;
    }
    doomo_player.pickup_flash = 10;
    audio_sfx(SFX_PICKUP);
}

/* ---- mayhem ---------------------------------------------------------------------- */
static void mayhem_tick(void)
{
    uint32_t el = combat_st.ticks - am.start_tick;
    if (am.state == AMS_INTRO && am.t > 60) {
        am.state = AMS_FIGHT;
        am.t = 0;
        am.start_tick = combat_st.ticks;
        am.last_kills = combat_st.kills;
        return;
    }
    if (am.state != AMS_FIGHT) return;
    combat_refill_ammo(100);                         /* ammo never runs out */
    if ((am.t % 10) == 0 && combat_demons_alive() < 16) {
        int a = spawn_demon(pick_type(12));
        if (a >= 0) {
            int r = rng_range(&doomo_rng, 100);
            lv_size_demon(a, r < 30 ? LVS_TINY : r < 70 ? LVS_NONE : r < 90 ? LVS_BIG : LVS_HUGE);
        }
    }
    if (el % (45 * DOOMO_TICK_HZ) == 20 * DOOMO_TICK_HZ) {
        static const uint8_t cycle[5] = { LB_GUTTERKING, LB_WIDOW, LB_FLUFFNUTS, LB_GRUMBLEGUTS, LB_BONEBOTTOM };
        spawn_variant(cycle[am.boss_cycle++ % 5], 4);
    }
    /* the monster meter */
    if (combat_st.kills != am.last_kills) {
        am.meter = (uint16_t)(am.meter + (combat_st.kills - am.last_kills));
        am.last_kills = combat_st.kills;
        if (am.meter >= METER_FULL && !am.berserk_t) {
            am.meter = 0;
            am.berserk_t = BERSERK_TICKS;
            combat_toast("MONSTER MODE!! BERSERK! " STR_ICON_SKULL);
            audio_sfx(SFX_DEMON_ROAR);
            combat_shake(12);
        }
    }
    if (el >= MAYHEM_TICKS) game_over(1);
}

/* ---- the shared tick ---------------------------------------------------------------- */
static void fight_tick(void)
{
    uint32_t score0 = combat_st.score;
    int protect = am.berserk_t || am.fury_t;
    am.hp0 = doomo_player.hp;
    lv_runtime_pre();
    combat_tick();
    lv_runtime_post();
    if (protect && doomo_player.hp < am.hp0 && am.hp0 > 0) {
        /* untouchable: the hit bounces off */
        doomo_player.hp = am.hp0;
        doomo_player.hurt_flash = 0;
        combat_px.dead_t = 0;
        combat_px.face_expr = EXPR_HAPPY;
    }
    if (am.berserk_t) {
        am.berserk_t--;
        if (combat_st.score > score0) combat_st.score += combat_st.score - score0;    /* x2 */
    }
    if (am.fury_t) am.fury_t--;
    if (combat_finished() == -1) {
        if (am.angel) {
            am.angel = 0;
            combat_s.finished = 0;
            doomo_player.hp = 60;
            doomo_player.hurt_flash = 0;
            doomo_player.pitch = 0;
            combat_px.dead_t = 0;
            combat_px.view_h = INT2FIX(DOOMO_EYE_HEIGHT);
            combat_px.face_expr = EXPR_LOVE;
            combat_px.face_t = 60;
            am.fury_t = 3 * DOOMO_TICK_HZ;
            actor_fx(SPR_HEARTS, doomo_player.x, doomo_player.y, doomo_player.z + INT2FIX(40), 3);
            combat_toast("A plush angel catches you! " STR_ICON_HEART);
            audio_sfx(SFX_HOLY);
        } else {
            game_over(0);
        }
    }
}

static void pause_update(void)
{
    if (btn_pressed(BTN_START) || btn_pressed(BTN_B)) { am.state = am.prev_state; audio_sfx(SFX_CANCEL); return; }
    if (btn_pressed(BTN_UP) || btn_pressed(BTN_DOWN)) {
        am.sel = (uint8_t)((am.sel + (btn_pressed(BTN_UP) ? 2 : 1)) % 3);
        audio_sfx(SFX_CURSOR);
    }
    if (!btn_pressed(BTN_A)) return;
    audio_sfx(SFX_SELECT);
    if (am.sel == 0) am.state = am.prev_state;
    else if (am.sel == 1) combat_toggle_camera();
    else game_over(0);
}

static void am_update(void)
{
    am.t++;
    switch (am.state) {
    case AMS_PAUSE: pause_update(); return;
    case AMS_SHOP: shop_update(); return;
    case AMS_OVER: case AMS_WIN:
        if (am.t > 60 && btn_pressed(BTN_A)) {
            audio_sfx(SFX_SELECT);
            if (am.state == AMS_OVER && am.sel == 0 && am.t > 60) { am_start(); return; }
            leave_scene();
        }
        if (am.state == AMS_OVER && (btn_pressed(BTN_UP) || btn_pressed(BTN_DOWN))) { am.sel ^= 1; audio_sfx(SFX_CURSOR); }
        return;
    default:
        break;
    }
    if (btn_pressed(BTN_START) && !combat_autopilot_on()) {
        am.prev_state = am.state;
        am.state = AMS_PAUSE;
        am.sel = 0;
        audio_sfx(SFX_SELECT);
        return;
    }
    if (am.state == AMS_INTRO && am.t == 1) am.start_tick = combat_st.ticks;
    if (am.mode == AM_BOSS_RUSH && am.state == AMS_INTRO && am.t > 60) rush_next();
    fight_tick();
    if (am.state == AMS_OVER || am.state == AMS_WIN) return;
    if (am.mode == AM_BOSS_RUSH) rush_tick();
    else if (am.mode == AM_HORDE) horde_tick();
    else mayhem_tick();
}

/* ---- drawing ---------------------------------------------------------------------- */
static void hud(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    char buf[40], tb[12];
    int y = sh / 2 - 50;
    /* the intro banners (the mode's and a mini-boss's) own the screen */
    if (am.state == AMS_INTRO || lv_run.intro_t > 10) return;
    if (am.mode == AM_BOSS_RUSH) {
        doomo_snprintf(buf, sizeof(buf), "ROUND %d/%d", DOOMO_MIN(am.round + 1, RUSH_ROUNDS), RUSH_ROUNDS);
        ui_text_outline(sw - ui_text_width(buf) - 4, y, buf, PAL_RAMP(RAMP_BONE, 7), PAL_RAMP(RAMP_BLOOD, 0));
        lvd_time(tb, sizeof(tb), (int)((combat_st.ticks - am.start_tick) / DOOMO_TICK_HZ));
        ui_text_outline(sw - ui_text_width(tb) - 4, y + 10, tb, PAL_WHITE, PAL_BLACK);
    } else if (am.mode == AM_HORDE) {
        doomo_snprintf(buf, sizeof(buf), "WAVE %d", am.wave);
        ui_text_outline(sw - ui_text_width(buf) - 4, y, buf, PAL_RAMP(RAMP_BONE, 7), PAL_RAMP(RAMP_BLOOD, 0));
        doomo_snprintf(buf, sizeof(buf), STR_ICON_COIN "%d", coins());
        ui_text_outline(sw - ui_text_width(buf) - 4, y + 10, buf, PAL_UI_GOLD, PAL_BLACK);
        if (am.angel) ui_text_outline(sw - 12, y + 20, STR_ICON_HEART, PAL_UI_PINK, PAL_BLACK);
        if (am.fury_t) ui_text_outline(sw / 2 - ui_text_width("HOLY FURY") / 2, sh - 24, "HOLY FURY",
                                       (am.t & 4) ? PAL_UI_GOLD : PAL_WHITE, PAL_BLACK);
    } else {
        int left = am.state == AMS_FIGHT ? (int)(MAYHEM_TICKS - (combat_st.ticks - am.start_tick)) / DOOMO_TICK_HZ : 180;
        /* below the boss bar / the map title while either shows */
        int w = lvd_big() ? 120 : 90, x = sw / 2 - w / 2, ty = lv_run.bar >= 0 ? lvd_bar_y() + 11 : combat_st.ticks < 150 ? 24 : 4;
        int by = ty + 19;
        lvd_time(tb, sizeof(tb), DOOMO_MAX(left, 0));
        ui_text_big(sw / 2 - ui_text_big_width(tb, 2) / 2, ty, tb, 2, left <= 10 && (am.t & 4) ? PAL_UI_RED : PAL_WHITE, PAL_BLACK);
        ui_round_rect(x - 1, by - 1, w + 2, 8, 3, PAL_BLACK);
        ui_bar(x, by, w, 6, am.berserk_t ? am.berserk_t : am.meter, am.berserk_t ? BERSERK_TICKS : METER_FULL,
               am.berserk_t ? ((am.t & 2) ? PAL_UI_RED : PAL_UI_GOLD) : PAL_UI_PURPLE);
        ui_text_outline(x - ui_text_width(STR_ICON_SKULL) - 3, by - 1, STR_ICON_SKULL, PAL_UI_RED, PAL_BLACK);
        doomo_snprintf(buf, sizeof(buf), STR_ICON_SKULL "%d", combat_st.kills);
        ui_text_outline(sw - ui_text_width(buf) - 4, y, buf, PAL_RAMP(RAMP_BONE, 7), PAL_RAMP(RAMP_BLOOD, 0));
        if (am.berserk_t) ui_text_outline(sw / 2 - ui_text_width("BERSERK x2") / 2, by + 10, "BERSERK x2",
                                          (am.t & 4) ? PAL_UI_RED : PAL_UI_GOLD, PAL_BLACK);
    }
}

static void shop_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), big = lvd_big();
    int w = big ? 240 : 212, row = big ? 17 : 14, h = SHOP_COUNT * row + (big ? 52 : 42);
    int x = sw / 2 - w / 2, y = sh / 2 - h / 2 + (big ? 4 : -1), i;
    char buf[40];
    plat_shade_rect(0, 0, sw, sh, 6);
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, am.t)) return;
    ui_title(sw / 2, y - 6, "SHOP BREAK", WIN_CREAM);
    doomo_snprintf(buf, sizeof(buf), STR_ICON_COIN " %d loot to spend", coins());
    ui_text_center(sw / 2, y + 8, buf, PAL_UI_PINK_D);
    for (i = 0; i < SHOP_COUNT; i++) {
        int ry = y + 20 + i * row, sel = i == am.sel;
        int owned = (i == SHOP_ANGEL && am.angel) || (i == SHOP_FURY && am.fury_armed);
        if (i == SHOP_GO) ui_button(x + 16, ry, w - 32, shop_names[i], sel, WIN_CREAM);
        else {
            doomo_snprintf(buf, sizeof(buf), owned ? "owned" : STR_ICON_COIN "%d", shop_cost[i]);
            ui_value_row(x + 8, ry, w - 16, shop_names[i], buf, sel);
            if (!owned && coins() < shop_cost[i]) plat_shade_rect(x + 8, ry, w - 16, row - 3, 3);
        }
    }
    ui_text_wrap(x + 8, y + h - 21, w - 16, shop_descs[am.sel], ui_style(WIN_CREAM)->text_dim, 1);
    ui_hint(STR_ICON_A " Buy  " STR_ICON_B " Next wave", WIN_CREAM);
}

static void pause_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), w = 160, h = 66, x = sw / 2 - w / 2, y = sh / 2 - h / 2;
    plat_shade_rect(0, 0, sw, sh, 7);
    if (!ui_window_pop(x, y, w, h, WIN_HELL, am.t)) return;
    ui_title(sw / 2, y - 6, "PAUSED", WIN_HELL);
    ui_button(x + 14, y + 10, w - 28, "Resume", am.sel == 0, WIN_HELL);
    ui_button(x + 14, y + 27, w - 28, doomo_player.camera == CAM_FIRST_PERSON ? "Camera: 1st" : "Camera: 3rd", am.sel == 1, WIN_HELL);
    ui_button(x + 14, y + 44, w - 28, "Give up", am.sel == 2, WIN_HELL);
}

static void results_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), big = lvd_big(), won = am.state == AMS_WIN;
    int w = big ? 250 : 216, h = big ? 150 : 120, x = sw / 2 - w / 2, y = sh / 2 - h / 2 + 8, row = big ? 13 : 11, r = 0;
    const char *title = won ? (am.mode == AM_MAYHEM ? "TIME'S UP!" : "VICTORY!") : (am.mode == AM_HORDE ? "OVERRUN!" : "DEFEATED");
    char buf[40], tb[12];
    int secs = (int)((am.end_tick - am.start_tick) / DOOMO_TICK_HZ), style = won ? WIN_CUTE : WIN_HELL;
    plat_shade_rect(0, 0, sw, sh, 6);
    ui_text_big(sw / 2 - ui_text_big_width(title, 2) / 2, y - 22 + ui_wave(am.t, 30, 2), title, 2,
                won ? PAL_UI_GOLD : PAL_UI_RED, won ? PAL_UI_PINK_D : PAL_RAMP(RAMP_BLOOD, 0));
    if (!ui_window_pop(x, y, w, h, style, am.t)) return;
    ui_text_center(sw / 2, y + 5, mode_titles[am.mode], ui_style(style)->text);
    plat_rect(x + 12, y + 14, w - 24, 1, ui_style(style)->fill_lo);
    lvd_time(tb, sizeof(tb), secs);
    if (am.mode == AM_BOSS_RUSH) {
        doomo_snprintf(buf, sizeof(buf), "%d / %d", am.round, RUSH_ROUNDS);
        ui_value_row(x + 8, y + 17 + row * r++, w - 16, "Bosses beaten", buf, 0);
        ui_value_row(x + 8, y + 17 + row * r++, w - 16, "Time", tb, 0);
    } else if (am.mode == AM_HORDE) {
        doomo_snprintf(buf, sizeof(buf), "%d", DOOMO_MAX(am.wave - 1, 0));
        ui_value_row(x + 8, y + 17 + row * r++, w - 16, "Waves survived", buf, 0);
        doomo_snprintf(buf, sizeof(buf), "wave %d", lv_horde_best());
        ui_value_row(x + 8, y + 17 + row * r++, w - 16, "Best ever", buf, 0);
    } else {
        doomo_snprintf(buf, sizeof(buf), "%d", (int)combat_st.score);
        ui_value_row(x + 8, y + 17 + row * r++, w - 16, "Score", buf, 0);
        doomo_snprintf(buf, sizeof(buf), "%d", doomo_town.monster_meter);
        ui_value_row(x + 8, y + 17 + row * r++, w - 16, "Town shame meter", buf, 0);
    }
    doomo_snprintf(buf, sizeof(buf), "%d", combat_st.kills);
    ui_value_row(x + 8, y + 17 + row * r++, w - 16, "Demons", buf, 0);
    doomo_snprintf(buf, sizeof(buf), "x%d", combat_st.best_combo);
    ui_value_row(x + 8, y + 17 + row * r++, w - 16, "Best combo", buf, 0);
    doomo_snprintf(buf, sizeof(buf), "+%d H$", am.payout);
    ui_value_row(x + 8, y + 17 + row * r++ + 3, w - 16, STR_ICON_COIN " Rewards", buf, 1);
    if (am.t > 60) {
        if (am.state == AMS_OVER) {
            ui_button(x + 14, y + h - 20, w / 2 - 18, "Again!", am.sel == 0, style);
            ui_button(x + w / 2 + 4, y + h - 20, w / 2 - 18, "Leave", am.sel == 1, style);
        } else {
            ui_hint(STR_ICON_A " Continue", style);
        }
    }
}

static void am_draw(void)
{
    DoomoView3D v;
    int sw = plat_screen_w(), sh = plat_screen_h();
    char buf[40];
    combat_build_view(&v);
    lv_runtime_view(&v);
    if (am.berserk_t && v.tint_level < 3) { v.tint = TINT_RED; v.tint_level = (uint8_t)(2 + ((am.t >> 2) & 1)); }
    if (am.fury_t && v.tint_level < 2) { v.tint = TINT_GOLD; v.tint_level = 2; }
    plat_view3d(&v);
    if (plat_draw_skipped()) return;
    if (am.state == AMS_INTRO || am.state == AMS_FIGHT || am.state == AMS_BREAK) {
        lv_runtime_hud();
        hud();
    }
    switch (am.state) {
    case AMS_INTRO:
        lvd_banner(sh / 3, am.mode == AM_MAYHEM ? "180 SECONDS OF" : "THE ARENA PRESENTS", mode_titles[am.mode], am.t, 1);
        if (am.mode == AM_HORDE && lv_horde_best()) {
            doomo_snprintf(buf, sizeof(buf), "Best: wave %d", lv_horde_best());
            ui_text_outline(sw / 2 - ui_text_width(buf) / 2, sh / 3 + 24, buf, PAL_UI_GOLD, PAL_BLACK);
        }
        break;
    case AMS_FIGHT:
        if (am.mode == AM_HORDE && am.t < 70) {
            doomo_snprintf(buf, sizeof(buf), "WAVE %d", am.wave);
            lvd_banner(sh / 4, am.wave % 5 == 0 ? "MINI-BOSS WAVE" : "", buf, am.t, 1);
        }
        break;
    case AMS_BREAK:
        if (am.mode == AM_BOSS_RUSH) {
            doomo_snprintf(buf, sizeof(buf), "ROUND %d CLEAR!", am.round);
            lvd_banner(sh / 4, "NEXT UP", buf, am.t, 1);
            if (am.t > 30 && am.round < RUSH_ROUNDS) {
                const LevelBoss *b = &lv_boss[rush_lineup[am.round]];
                ui_text_outline(sw / 2 - ui_text_width(b->name) / 2, sh / 4 + 24, b->name, PAL_RAMP(RAMP_LAVA, 7), PAL_BLACK);
                lvd_demon(sw / 2, sh / 4 + 30 + (lvd_big() ? 90 : 70), b->spr, am.t, 1, 0);
            }
        } else {
            doomo_snprintf(buf, sizeof(buf), "Next wave in %d", DOOMO_MAX((90 - am.t) / 30 + 1, 1));
            ui_text_outline(sw / 2 - ui_text_width(buf) / 2, sh / 4 + 24, buf, PAL_UI_GOLD, PAL_RAMP(RAMP_BLOOD, 0));
        }
        break;
    case AMS_SHOP: shop_draw(); break;
    case AMS_PAUSE: pause_draw(); break;
    case AMS_OVER: case AMS_WIN: results_draw(); break;
    default: break;
    }
}

const DoomoScene SCENE_ARENA_MODE = { "ARENA_MODE", am_enter, am_leave, am_update, am_draw, 0 };
