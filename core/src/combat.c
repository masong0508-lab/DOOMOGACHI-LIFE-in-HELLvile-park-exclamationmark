/* combat.c - the shooter session (owner: action): controls (classic /
 * modern, invert look, look speed), 30 Hz player physics (acceleration and
 * friction, strafe, sprint, jump + mantle, crouch / auto-crouch,
 * dodge-roll with i-frames, step-up smoothing, lifts, doors, knockback,
 * lava, water), the welded weapon (hitscan / projectile / lob / float /
 * stream / rail / melee, bursts, spin-up, charge, reload, underbarrel
 * procs, recoil, view kick, muzzle flash), vertical aim assist + soft
 * lock-on, ADS zoom, pickups, keys, secrets, triggers, exits, death, and
 * the first- / third-person camera handed to plat_view3d().
 */
#include "combat_int.h"

DoomoPlayerState  doomo_player;
CombatSession     combat_s;
PlayerX           combat_px;
CombatCtl         combat_ctl;
DoomoCombatStats  combat_st;
DmgNum            combat_dmgnums[DMGNUM_MAX];
DoomoWeaponStats  combat_ws;
void (*combat_bot_fn)(CombatCtl *ctl);

#define BB_MAX 120
/* (integration, memory budget) the billboard list is rebuilt every frame
 * and only read inside plat_view3d(): it is the shared doomo_frame_bb
 * (scene.h), not a 2.4 KB array of its own */
#define bbs doomo_frame_bb
DOOMO_STATIC_ASSERT(BB_MAX <= DOOMO_FRAME_BB_MAX, combat_billboards_fit_frame_bb);
/* one flood-fill stack for both secret-region walks (run at map start,
 * never at the same time) */
static uint16_t flood_stack[256];
static int last_result;
static int turn_ticks;
static int select_used;
static int lock_lost;
static int msg_cool;

#define WALK_SPEED   ((fix16)(7 * 65536))
#define ROLL_SPEED   ((fix16)(13 * 65536))
#define GRAVITY      ((fix16)75366)          /* 1.15 units / tick^2 */
#define JUMP_VZ      ((fix16)(8 * 65536 + 13107))   /* 8.2 */
#define STAND_EYE    DOOMO_EYE_HEIGHT
#define TP_TILT      36                      /* third person looks a bit down */
#define CROUCH_EYE   22

static const int16_t ammo_max_tab[AMMO_COUNT] = { 400, 100, 600, 50, 400, 400, 500 };
static const int16_t ammo_start_tab[AMMO_COUNT] = { 80, 16, 80, 4, 100, 60, 120 };

/* ---- small helpers ------------------------------------------------------------- */
int combat_difficulty(void) { return opt_get(OPT_DIFFICULTY); }
int combat_feel(void) { return input_feel() ? opt_get(OPT_COMBAT_FEEL) : 0; }

void combat_shake(int amount)
{
    int lvl = opt_get(OPT_SCREEN_SHAKE);              /* 0 off, 1 low, 2 full */
    amount = lvl == 0 ? 0 : lvl == 1 ? amount / 2 : amount;
    if (amount > combat_px.shake) combat_px.shake = amount;
}

void combat_toast(const char *msg)
{
    combat_px.msg = msg;
    combat_px.msg_t = 75;
}

int combat_ammo(int t) { return (t >= 0 && t < AMMO_COUNT) ? doomo_town.story.ammo[t] : 0; }
int combat_ammo_max(int t) { return (t >= 0 && t < AMMO_COUNT) ? ammo_max_tab[t] : 0; }

static void ammo_add(int t, int n)
{
    if (t < 0 || t >= AMMO_COUNT) return;
    doomo_town.story.ammo[t] = (uint16_t)DOOMO_CLAMP(doomo_town.story.ammo[t] + n, 0, ammo_max_tab[t]);
}

void combat_refill_ammo(int percent)
{
    int t;
    for (t = 0; t < AMMO_COUNT; t++) {
        int want = ammo_start_tab[t] * percent / 100;
        if (doomo_town.story.ammo[t] < want) doomo_town.story.ammo[t] = (uint16_t)want;
    }
}

void combat_player_eye(fix16 *x, fix16 *y, int *z)
{
    *x = doomo_player.x;
    *y = doomo_player.y;
    *z = FIX2INT(doomo_player.z) + FIX2INT(combat_px.view_h);
}

int combat_player_z(void) { return FIX2INT(doomo_player.z); }

int combat_last_result(void) { return last_result; }
int combat_set_result_quit(void)
{
    if (last_result == 0) last_result = -2;
    return last_result;
}
const DoomoCombatStats *combat_stats(void) { return &combat_st; }
int combat_finished(void) { return combat_s.finished; }
int combat_demons_alive(void) { return actor_demons_alive(); }

void combat_add_dmgnum(fix16 x, fix16 y, fix16 z, int value, int crit)
{
    int i, best = 0;
    if (!opt_get(OPT_DAMAGE_NUMBERS)) return;
    for (i = 0; i < DMGNUM_MAX; i++)
        if (combat_dmgnums[i].t == 0 || combat_dmgnums[i].t > combat_dmgnums[best].t) {
            best = i;
            if (combat_dmgnums[i].t == 0) break;
        }
    combat_dmgnums[best].x = x;
    combat_dmgnums[best].y = y;
    combat_dmgnums[best].z = z;
    combat_dmgnums[best].value = (int16_t)value;
    combat_dmgnums[best].t = 1;
    combat_dmgnums[best].crit = (uint8_t)crit;
}

/* ---- weapons ------------------------------------------------------------------- */
static int weapon_valid(int slot)
{
    return slot >= 0 && slot < DOOMO_MAX_WEAPONS && (doomo_town.weapons[slot].flags & WF_VALID) &&
           doomo_town.weapons[slot].frame < WFRAME_COUNT;
}

const DoomoWeapon *combat_weapon(void)
{
    static const DoomoWeapon fists = { WFRAME_FISTS, 0, 0, 0, 0, 0, 0, 0, 0, WSTYLE_CUTE,
                                       PAINT_ROSE, PAINT_WHITE, 50, WF_VALID, 0, 0 };
    if (weapon_valid(doomo_player.weapon_slot)) return &doomo_town.weapons[doomo_player.weapon_slot];
    return &fists;
}

static void refresh_stats(void)
{
    weld_stats(combat_weapon(), &combat_ws);
}

int combat_give_starter_weapons(void)
{
    int i, n = 0, have = 0;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++) if (weapon_valid(i)) have++;
    if (have) return 0;
    for (i = 0; i < DOOMO_MAX_WEAPONS && n < 3; i++) {
        DoomoWeapon *w = &doomo_town.weapons[i];
        if (n == 0) {
            weld_default(w, WFRAME_PISTOL, WSTYLE_CUTE);
            w->paint = PAINT_BUBBLEGUM; w->accent = PAINT_MINT; w->charm = WCHARM_HEART;
            w->sight = WSIGHT_RED_DOT;
        } else if (n == 1) {
            weld_default(w, WFRAME_PUMP_SHOTGUN, WSTYLE_BRUTAL);
        } else {
            weld_default(w, WFRAME_BUBBLE_WAND, WSTYLE_CUTE);
        }
        w->flags = WF_VALID | WF_STARTER;
        n++;
    }
    return n;
}

static void fill_mags(void)
{
    int i;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++) {
        DoomoWeaponStats s;
        if (!weapon_valid(i)) { combat_px.mag[i] = 0; continue; }
        weld_stats(&doomo_town.weapons[i], &s);
        combat_px.mag[i] = s.mag_size;
    }
}

void combat_select_weapon(int slot)
{
    if (!weapon_valid(slot)) return;
    if (slot == doomo_player.weapon_slot && combat_px.switch_to < 0) return;
    combat_px.switch_to = slot;
    combat_px.reload_t = 0;
    combat_px.charge_t = 0;
    combat_px.burst_left = 0;
}

void combat_next_weapon(int dir)
{
    int i, s = combat_px.switch_to >= 0 ? combat_px.switch_to : doomo_player.weapon_slot;
    int favs = 0;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++)
        if (weapon_valid(i) && (doomo_town.weapons[i].flags & WF_FAVORITE)) favs++;
    for (i = 1; i <= DOOMO_MAX_WEAPONS; i++) {
        int k = (s + dir * i + DOOMO_MAX_WEAPONS * 4) % DOOMO_MAX_WEAPONS;
        if (!weapon_valid(k)) continue;
        if (favs >= 2 && !(doomo_town.weapons[k].flags & WF_FAVORITE)) continue;
        combat_select_weapon(k);
        audio_sfx(SFX_CURSOR);
        return;
    }
}

static int weapon_has_ammo(int slot)
{
    DoomoWeaponStats s;
    if (!weapon_valid(slot)) return 0;
    weld_stats(&doomo_town.weapons[slot], &s);
    if (s.ammo_type == 0xFF) return 1;
    return combat_px.mag[slot] >= s.ammo_use || combat_ammo(s.ammo_type) >= s.ammo_use;
}

void combat_toggle_camera(void)
{
    doomo_player.camera = (uint8_t)(doomo_player.camera == CAM_FIRST_PERSON ? CAM_THIRD_PERSON
                                                                             : CAM_FIRST_PERSON);
    combat_px.cam_ok = 0;
}

/* ---- session --------------------------------------------------------------------- */
static int count_secret_regions(void)
{
    /* each connected group of CELL_SECRET cells is one secret */
    uint16_t *stack = flood_stack;
    uint8_t seen[DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H / 8];
    int n = 0, i, w = doomo_map.w, total = doomo_map.w * doomo_map.h;
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < total; i++) {
        int sp = 0;
        if (!(doomo_map.cells[i].flags & CELL_SECRET) || (seen[i >> 3] & (1 << (i & 7)))) continue;
        n++;
        stack[sp++] = (uint16_t)i;
        seen[i >> 3] |= (uint8_t)(1 << (i & 7));
        while (sp > 0) {
            int c = stack[--sp], k;
            int nb[4];
            nb[0] = c - 1; nb[1] = c + 1; nb[2] = c - w; nb[3] = c + w;
            for (k = 0; k < 4; k++) {
                int j = nb[k];
                if (j < 0 || j >= total || (k < 2 && j / w != c / w)) continue;
                if (!(doomo_map.cells[j].flags & CELL_SECRET) || (seen[j >> 3] & (1 << (j & 7)))) continue;
                seen[j >> 3] |= (uint8_t)(1 << (j & 7));
                if (sp < 256) stack[sp++] = (uint16_t)j;
            }
        }
    }
    return n;
}

static void mark_secret_region(int start)
{
    uint16_t *stack = flood_stack;
    int sp = 0, w = doomo_map.w, total = doomo_map.w * doomo_map.h;
    stack[sp++] = (uint16_t)start;
    while (sp > 0) {
        int c = stack[--sp], k, nb[4];
        doomo_map.cells[c].flags &= (uint8_t)~CELL_SECRET;      /* counted once */
        nb[0] = c - 1; nb[1] = c + 1; nb[2] = c - w; nb[3] = c + w;
        for (k = 0; k < 4; k++) {
            int j = nb[k];
            if (j < 0 || j >= total || (k < 2 && j / w != c / w)) continue;
            if ((doomo_map.cells[j].flags & CELL_SECRET) && sp < 256) stack[sp++] = (uint16_t)j;
        }
    }
}

static int pick_resident(void)
{
    int i;
    if (doomo_town.player.status != RES_EMPTY) return DOOMO_PLAYER_ID;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (doomo_town.res[i].status == RES_ALIVE) return i;
    return DOOMO_PLAYER_ID;
}

void combat_start_at(int map_id, int goal, int cx, int cy, int angle)
{
    const DoomoMapDef *d;
    int i, density, demons = 0, have_start = 0;
    map_load(map_id);
    actors_clear();
    memset(&combat_s, 0, sizeof(combat_s));
    memset(&combat_px, 0, sizeof(combat_px));
    memset(&combat_st, 0, sizeof(combat_st));
    memset(&combat_dmgnums, 0, sizeof(combat_dmgnums));
    memset(&doomo_player, 0, sizeof(doomo_player));
    memset(&combat_ctl, 0, sizeof(combat_ctl));
    combat_s.map_id = map_id;
    combat_s.goal = goal;
    combat_s.boss = -1;
    combat_s.active = 1;
    combat_px.switch_to = -1;
    combat_px.view_h = INT2FIX(STAND_EYE);
    combat_px.cute_only = 1;
    doomo_player.lock_target = -1;
    doomo_player.hp = 100;
    doomo_player.armor = 0;
    doomo_player.on_ground = 1;
    doomo_player.camera = (uint8_t)opt_get(OPT_CAMERA_DEFAULT);
    doomo_player.shoulder = (uint8_t)opt_get(OPT_SHOULDER);
    doomo_player.resident_id = (int16_t)pick_resident();
    d = doomo_map.def;
    density = opt_get(OPT_DEMON_DENSITY);
    if (d && d->things) {
        for (i = 0; i < d->thing_count; i++) {
            const DoomoThing *t = &d->things[i];
            int a;
            if (t->type == THING_PLAYER_START) {
                if (!have_start) {
                    doomo_player.x = INT2FIX(t->x * CELLU + CELLU / 2);
                    doomo_player.y = INT2FIX(t->y * CELLU + CELLU / 2);
                    doomo_player.angle = (int16_t)(t->angle * 16);
                    have_start = 1;
                }
                continue;
            }
            if (IS_DEMON(t->type) && !(t->flags & THF_BOSS)) {
                demons++;
                if (density == 0 && (demons % 3) == 0) continue;            /* sparse */
            }
            a = actor_spawn_thing(t, 0);
            if (a < 0) continue;
            if (IS_DEMON(t->type)) {
                if (t->flags & THF_BOSS) {
                    combat_s.boss = a;
                    combat_s.boss_max = doomo_actors[a].hp;
                }
                if (density == 2 && (demons & 1) && !(t->flags & THF_BOSS)) {      /* swarm */
                    fix16 sx = doomo_actors[a].x + INT2FIX(24), sy = doomo_actors[a].y;
                    int fl, ce;
                    if (cw_fits(sx, sy, 16, FIX2INT(doomo_actors[a].z), 50, 24, &fl, &ce)) {
                        actor_spawn_demon(THING_IMPLING, sx, sy, 0, t->flags & (THF_DORMANT | THF_AMBUSH), t->arg);
                    }
                }
            } else if (doomo_actors[a].flags & AF_PICKUP) {
                combat_st.items_total++;
                if (t->flags & THF_SECRET) combat_st.secrets_total++;
            }
        }
    }
    if (cx >= 0 && cy >= 0) {
        doomo_player.x = INT2FIX(cx * CELLU + CELLU / 2);
        doomo_player.y = INT2FIX(cy * CELLU + CELLU / 2);
        doomo_player.angle = (int16_t)(angle & DOOMO_ANG_MASK);
    } else if (!have_start) {
        /* first open cell */
        for (i = 0; i < doomo_map.w * doomo_map.h; i++)
            if (!doomo_map.cells[i].wall) {
                doomo_player.x = INT2FIX((i % doomo_map.w) * CELLU + CELLU / 2);
                doomo_player.y = INT2FIX((i / doomo_map.w) * CELLU + CELLU / 2);
                break;
            }
    }
    doomo_player.z = INT2FIX(cw_floor(FIX2INT(doomo_player.x) / CELLU, FIX2INT(doomo_player.y) / CELLU));
    combat_st.secrets_total = (uint16_t)(combat_st.secrets_total + count_secret_regions());
    combat_px.last_cx = -1;
    /* weapons and ammo */
    combat_give_starter_weapons();
    doomo_player.weapon_slot = doomo_town.story.equipped;
    if (!weapon_valid(doomo_player.weapon_slot)) {
        for (i = 0; i < DOOMO_MAX_WEAPONS; i++) if (weapon_valid(i)) { doomo_player.weapon_slot = (uint8_t)i; break; }
    }
    {
        int t, any = 0;
        for (t = 0; t < AMMO_COUNT; t++) any += doomo_town.story.ammo[t];
        if (!any) combat_refill_ammo(100);
    }
    fill_mags();
    refresh_stats();
    combat_px.raise = 256;                 /* the weapon comes up */
    combat_hud_reset();
    if (d) audio_music(d->music);
    last_result = 0;
    memset(&combat_ctl, 0, sizeof(combat_ctl));
    combat_px.turn_vel = 0;
    combat_px.fire_buf = 0;
    if (opt_get(OPT_CONTROL_SCHEME) == CONTROLS_DUAL_STICK) {
        int a, b, c2, d2;
        if (!input_analog(&a, &b, &c2, &d2))
            combat_toast("Dual stick needs an analog pad - press ANALOG");
    }
}

void combat_start_ex(int map_id, int goal) { combat_start_at(map_id, goal, -1, -1, 0); }

void combat_start(int map_id)
{
    int goal = CGOAL_EXIT;
    const DoomoMapDef *d = (map_id >= 0 && map_id < MAPID_COUNT) ? doomo_maps[map_id] : NULL;
    if (d && d->kind == MAP_OVERWORLD) goal = CGOAL_KILL_ALL;
    if (d && d->kind == MAP_ARENA) goal = CGOAL_ENDLESS;
    combat_start_ex(map_id, goal);
}

int combat_spawn(int thing_type, int cx, int cy, int appear)
{
    DoomoThing t;
    int a;
    if (cx < 0 || cy < 0 || cx >= doomo_map.w || cy >= doomo_map.h || cw_solid(cx, cy)) return -1;
    memset(&t, 0, sizeof(t));
    t.x = (uint8_t)cx;
    t.y = (uint8_t)cy;
    t.type = (uint8_t)thing_type;
    t.angle = (uint8_t)rng_range(&doomo_rng, 256);
    a = actor_spawn_thing(&t, appear);
    return a;
}

void combat_event(int tag)
{
    if (tag <= 0 || tag > 255) return;
    if (combat_s.fired[tag >> 3] & (1 << (tag & 7))) return;
    combat_s.fired[tag >> 3] |= (uint8_t)(1 << (tag & 7));
    map_event(&doomo_map, tag);
    actor_event(tag);
}

/* ---- the player gets hurt --------------------------------------------------------------- */
void combat_hurt_player(int dmg, int src_angle, int knock, int src)
{
    int d = combat_difficulty(), absorbed;
    (void)src;
    if (doomo_player.hp <= 0 || dmg <= 0) return;
    if (combat_px.invuln > 0) return;
    if (opt_get(OPT_CHEAT_GOD_MODE)) { doomo_player.hurt_flash = 4; return; }
    dmg = d == DIFF_PLUSHIE ? (dmg + 1) / 2 : d == DIFF_NIGHTMARE ? dmg * 3 / 2 : d >= DIFF_ULTRA_HELLVILE ? dmg * 2 : dmg;
    if (pet_combat_buff() > 0) dmg = dmg * 9 / 10;
    if (doomo_player.armor > 0) {
        absorbed = DOOMO_MIN(dmg / 3, doomo_player.armor);
        doomo_player.armor = (int16_t)(doomo_player.armor - absorbed);
        dmg -= absorbed;
    }
    doomo_player.hp = (int16_t)(doomo_player.hp - dmg);
    combat_st.damage_taken = (uint16_t)DOOMO_MIN(combat_st.damage_taken + dmg, 60000);
    doomo_player.hurt_flash = (uint16_t)DOOMO_MIN(doomo_player.hurt_flash + dmg + 6, 24);
    combat_px.dmg_dir = src_angle;
    combat_px.dmg_dir_t = 40;
    combat_px.face_expr = dmg >= 20 ? EXPR_SHOCK : EXPR_ANGRY;
    combat_px.face_t = 30;
    if (knock > 0) {
        fix16 k = INT2FIX(DOOMO_MIN(knock, 16)) / 2;
        int away = (src_angle + 2048) & DOOMO_ANG_MASK;
        combat_px.vx += FIXMUL(fx_cos(away), k);
        combat_px.vy += FIXMUL(fx_sin(away), k);
    }
    combat_shake(DOOMO_MIN(4 + dmg / 4, 14));
    audio_sfx(SFX_PLAYER_HURT);
    if (doomo_player.hp <= 0) {
        doomo_player.hp = 0;
        combat_px.dead_t = 1;
        combat_px.face_expr = EXPR_DEAD;
        combat_px.face_t = 1000;
        audio_sfx(SFX_DEMON_ROAR);
    }
}

static void heal(int n, int cap)
{
    if (doomo_player.hp < cap) doomo_player.hp = (int16_t)DOOMO_MIN(doomo_player.hp + n, cap);
}

/* ---- kills ----------------------------------------------------------------------------- */
void combat_on_kill(int idx, int by_player, int headshot)
{
    DoomoActor *a = &doomo_actors[idx];
    const DemonInfo *di = DEMON(a->type);
    int style = combat_gore_style();
    if (a->flags & AF_COUNTKILL) {
        combat_st.kills++;
        a->flags &= (uint8_t)~AF_COUNTKILL;
    }
    combat_px.combo = combat_px.combo_t > 0 ? combat_px.combo + 1 : 1;
    combat_px.combo_t = 90;
    if (combat_px.combo > combat_st.best_combo) combat_st.best_combo = (uint16_t)combat_px.combo;
    combat_st.score += (uint32_t)di->score * (uint32_t)(10 + combat_px.combo * 2) / 10;
    combat_st.money = (uint16_t)DOOMO_MIN(combat_st.money + di->score / 2 + 1, 60000);
    progress_add(PROG_DEMONS_KILLED, 1);
    progress_add(style == WSTYLE_CUTE ? PROG_DEMONS_KILLED_CUTE : PROG_DEMONS_KILLED_BRUTAL, 1);
    if (style != WSTYLE_CUTE) combat_px.cute_only = 0;
    if (headshot && by_player) { progress_add(PROG_HEADSHOTS, 1); combat_st.headshots++; }
    unlock_grant(540 + (a->type - THING_IMPLING));
    combat_px.hitstop = (a->flags & AF_BOSS) ? 8 : 2;
    if (combat_feel() && by_player) {                     /* kills land with weight */
        combat_px.hitstop = (a->flags & AF_BOSS) ? 10 : headshot ? 4 : 3;
        combat_shake(headshot ? 6 : 3);
    }
    if (by_player) {
        combat_px.hit_kill = 10;
        if (weapon_valid(doomo_player.weapon_slot)) {
            DoomoWeapon *w = &doomo_town.weapons[doomo_player.weapon_slot];
            int before = weld_level(w);
            if (w->xp < 255) w->xp++;
            if (weld_level(w) > before) {
                combat_toast("Weapon level up! " STR_ICON_STAR);
                refresh_stats();
                audio_sfx(SFX_EVOLVE);
            }
        }
        combat_px.face_expr = EXPR_HAPPY;
        combat_px.face_t = 24;
        if (combat_ws.lifesteal) heal(DOOMO_MAX(1, di->hp * combat_ws.lifesteal / 400), 100);
    }
    /* drops */
    if (!(a->flags & AF_BOSS) && rng_chance(&doomo_rng, 30)) {
        static const uint8_t drops[6] = { THING_SCRAP, THING_CANDY, THING_AMMO_BULLETS, THING_AMMO_SHELLS,
                                          THING_CANDY, THING_HEALTH_SMALL };
        DoomoThing t;
        int k;
        memset(&t, 0, sizeof(t));
        t.x = (uint8_t)(FIX2INT(a->x) / CELLU);
        t.y = (uint8_t)(FIX2INT(a->y) / CELLU);
        t.type = drops[rng_range(&doomo_rng, 6)];
        k = actor_spawn_thing(&t, 0);
        if (k >= 0) { doomo_actors[k].x = a->x; doomo_actors[k].y = a->y; }
    }
    if (rng_chance(&doomo_rng, 4)) {
        inv_add(ITEM_DEMON_HORN, 1);
        combat_toast("Got a demon horn!");
    }
    /* kill events (remote doors, the exit after a boss) */
    if ((a->tflags & THF_KILL_EVENT) && actor_kill_group_alive(a->arg) == 0)
        combat_event((a->tflags & THF_DORMANT) ? a->arg + 1 : a->arg);
    if (a->flags & AF_BOSS) {
        combat_s.boss = -1;
        combat_shake(16);
        if (a->type == THING_LORD_VAPULA) {
            combat_toast("LORD VAPULA IS DEFEATED!");
            unlock_grant(436);
            unlock_grant(531);
        } else {
            combat_toast("The vape wraith dissolves!");
        }
    }
}

/* ---- controls -------------------------------------------------------------------------- */
/* Radial dead zone + rescale: raw pad axes (-128..127) -> -256..256. */
#define STICK_DEAD 22
static void stick_vec(int x, int y, int *ox, int *oy)
{
    int m = (int)fx_dist(INT2FIX(x), INT2FIX(y)) >> 16;
    *ox = *oy = 0;
    if (m <= STICK_DEAD) return;
    {
        int out = (m - STICK_DEAD) * 256 / (125 - STICK_DEAD);      /* 0..256 */
        if (out > 256) out = 256;
        *ox = x * out / m;
        *oy = y * out / m;
        *ox = DOOMO_CLAMP(*ox, -256, 256);
        *oy = DOOMO_CLAMP(*oy, -256, 256);
    }
}

/* Response curve: fine control near the centre, full speed at the rim. */
static int stick_curve(int v)
{
    int a = v < 0 ? -v : v;
    int c = (a * a / 256 * 2 + a) / 3;
    return v < 0 ? -c : c;
}

/* Turn rate easing for the D-pad (feel): ramps up and glides to a stop
 * instead of snapping between three fixed speeds. */
static int ease_turn(int target)
{
    int v = combat_px.turn_vel;
    int gap = target - v;
    if (target == 0) v = v * 5 / 8;                   /* release: glide out */
    else v += gap / 3 + (gap > 0 ? 1 : gap < 0 ? -1 : 0);
    if (v > -2 && v < 2 && target == 0) v = 0;
    combat_px.turn_vel = v;
    return v;
}

static void read_controls(CombatCtl *c)
{
    int scheme = opt_get(OPT_CONTROL_SCHEME);
    int classic = scheme == CONTROLS_CLASSIC;
    int speed = opt_get(OPT_LOOK_SPEED);                /* 0..9 */
    int inv = opt_get(OPT_INVERT_LOOK) ? -1 : 1;
    int feel = input_feel();
    int lax, lay, rax, ray, have;
    int slx = 0, sly = 0, srx = 0, sry = 0;             /* dead-zoned sticks */
    uint32_t smask = input_stick_mask();
    int lmod, left, right, up, down;
    int maxturn = 44 + speed * 9, look = 6 + speed;
    int dual;
    memset(c, 0, sizeof(*c));
    have = input_analog(&lax, &lay, &rax, &ray);
    dual = scheme == CONTROLS_DUAL_STICK && have;       /* no sticks: behaves like Modern */
    if (have) {
        stick_vec(lax, lay, &slx, &sly);
        stick_vec(rax, ray, &srx, &sry);
    }
    /* D-pad only: the pad driver also mirrors the left stick into the
     * D-pad bits (menus need that) - strip those here, the analog value
     * is used instead. */
#define DP(b) (btn_down(b) && !(smask & (b)))
    left = DP(BTN_LEFT); right = DP(BTN_RIGHT); up = DP(BTN_UP); down = DP(BTN_DOWN);
    if (!have) { left = btn_down(BTN_LEFT); right = btn_down(BTN_RIGHT); up = btn_down(BTN_UP); down = btn_down(BTN_DOWN); }
#undef DP
    lmod = classic && btn_down(BTN_L);
    if (up) c->move = 1;
    if (down) c->move = -1;
    if (dual) {
        /* dual stick: left stick = move + strafe, right stick = turn + look.
         * D-pad up/down walk, left/right strafe. */
        c->strafe = (int8_t)(right - left);
        if (sly || slx) {
            c->fmove = (int16_t)(-stick_curve(sly));
            c->fstrafe = (int16_t)stick_curve(slx);
            c->move = (int8_t)(sly < -64 ? 1 : sly > 64 ? -1 : c->move);
            c->strafe = (int8_t)(slx < -64 ? -1 : slx > 64 ? 1 : c->strafe);
        }
        if (srx) {
            c->turn = (int16_t)(stick_curve(srx) * (maxturn * 6 / 5) / 256);
            if (feel) {                 /* ease the stick so flicks do not whip the view */
                combat_px.turn_vel += (c->turn - combat_px.turn_vel) / 2;
                c->turn = (int16_t)combat_px.turn_vel;
            }
        } else {
            combat_px.turn_vel = 0;
        }
        if (sry) c->look = (int16_t)(-stick_curve(sry) * look * 2 / 256 * inv);
    } else if (lmod) {
        /* classic: hold L = strafe with left/right, look with up/down */
        c->strafe = (int8_t)(right - left);
        c->look = (int16_t)((up - down) * look * 2 * inv);
        c->move = 0;
        if (slx) c->fstrafe = (int16_t)stick_curve(slx), c->strafe = (int8_t)(slx < 0 ? -1 : 1);
        if (sly) c->look = (int16_t)(-stick_curve(sly) * look * 2 / 256 * inv);
    } else if (!classic) {
        /* modern: D-pad left/right (and the left stick) strafe; R1 turns
         * right, R2 turns left (L1 also turns left, for the GBA, which has
         * no R2). Hold L2 for D-pad up/down look. */
        int dir = btn_down(BTN_R) - (btn_down(BTN_R2) || btn_down(BTN_L));
        if (btn_down(BTN_L) && btn_down(BTN_R)) dir = 0;   /* L1+R1 = lock-on */
        c->strafe = (int8_t)(right - left);
        if (slx && have) c->fstrafe = (int16_t)stick_curve(slx), c->strafe = (int8_t)(slx < 0 ? -1 : 1);
        if (dir) {
            int t = ++turn_ticks;
            if (feel) {
                c->turn = (int16_t)ease_turn(dir * maxturn);
            } else {
                int sp = t < 4 ? maxturn / 3 : t < 8 ? maxturn * 2 / 3 : maxturn;
                c->turn = (int16_t)(dir * sp);
            }
        } else {
            turn_ticks = 0;
            if (feel) c->turn = (int16_t)ease_turn(0);
        }
        if (sly && have) {
            c->fmove = (int16_t)(-stick_curve(sly));
            c->move = (int8_t)(sly < 0 ? 1 : -1);
        }
        if (btn_down(BTN_L2)) {
            c->look = (int16_t)((up - down) * look * 2 * inv);
            c->move = 0; c->fmove = 0;
        }
    } else {
        int dir = right - left;
        if (slx && have) {
            /* analog turn straight off the stick (classic) */
            c->turn = (int16_t)(stick_curve(slx) * maxturn / 256);
            combat_px.turn_vel = c->turn;
            turn_ticks = 0;
        } else if (dir) {
            int t = ++turn_ticks;
            if (feel) {
                c->turn = (int16_t)ease_turn(dir * maxturn);
            } else {
                int sp = t < 4 ? maxturn / 3 : t < 8 ? maxturn * 2 / 3 : maxturn;
                c->turn = (int16_t)(dir * sp);
            }
        } else {
            turn_ticks = 0;
            if (feel) c->turn = (int16_t)ease_turn(0);
        }
        if (sly && have) {
            c->fmove = (int16_t)(-stick_curve(sly));
            c->move = (int8_t)(sly < 0 ? 1 : -1);
        }
    }
    if (classic) {
        if (btn_down(BTN_L2)) c->look = (int16_t)(-look * 2 * inv);
        if (btn_down(BTN_R2)) c->look = (int16_t)(look * 2 * inv);
    }
    /* aim / lock-on: classic R, modern L+R together; dual stick L2 (or L1+R1) */
    if (dual) {
        c->lock = (uint8_t)btn_down(BTN_L2);
        c->lock_pressed = (uint8_t)btn_pressed(BTN_L2);
    } else if (classic) {
        c->lock = (uint8_t)btn_down(BTN_R);
        c->lock_pressed = (uint8_t)btn_pressed(BTN_R);
    } else {
        c->lock = (uint8_t)(btn_down(BTN_L) && btn_down(BTN_R));
        c->lock_pressed = (uint8_t)(c->lock && (btn_pressed(BTN_L) || btn_pressed(BTN_R)));
    }
    if (dual) {
        /* trigger fingers: R2 / R1 / Cross all shoot */
        uint32_t f = BTN_A | BTN_R | BTN_R2;
        c->fire = (uint8_t)btn_down(f);
        c->fire_pressed = (uint8_t)btn_pressed(f);
        c->fire_released = (uint8_t)(btn_released(f) && !btn_down(f));
    } else {
        c->fire = (uint8_t)btn_down(BTN_A);
        c->fire_pressed = (uint8_t)btn_pressed(BTN_A);
        c->fire_released = (uint8_t)btn_released(BTN_A);
    }
    if (btn_pressed(BTN_B)) {
        if (down) c->crouch_toggle = 1;
        else c->jump = 1;
    }
    c->sprint = (uint8_t)(btn_down(BTN_B) && !down);
    if (btn_pressed(BTN_X)) { c->roll = 1; c->roll_dir = 0; select_used = 1; }
    if (dual && btn_pressed(BTN_L)) { c->roll = 1; c->roll_dir = 0; select_used = 1; }
    if (btn_pressed(BTN_Y)) { c->next_weapon = 1; select_used = 1; }
    /* double-tap a strafe direction / back = dodge-roll */
    {
        int tap = 0;
        if (dual) {
            /* no L/R taps here: Square / L1 roll toward the stick instead */
        } else if (classic) {
            if (lmod && btn_pressed(BTN_LEFT)) tap = 1;
            if (lmod && btn_pressed(BTN_RIGHT)) tap = 2;
        } else {
            if (btn_pressed(BTN_LEFT) && !(smask & BTN_LEFT)) tap = 1;
            if (btn_pressed(BTN_RIGHT) && !(smask & BTN_RIGHT)) tap = 2;
        }
        if (btn_pressed(BTN_DOWN) && !(smask & BTN_DOWN) && !lmod && !dual) tap = 3;
        if (tap) {
            if (combat_px.tap_dir == tap && combat_px.tap_t > 0) {
                c->roll = 1;
                c->roll_dir = (int8_t)(tap == 1 ? -1 : tap == 2 ? 1 : 2);
                combat_px.tap_t = 0;
            } else {
                combat_px.tap_dir = tap;
                combat_px.tap_t = 8;
            }
        }
        if (combat_px.tap_t > 0) combat_px.tap_t--;
    }
    if (btn_pressed(BTN_SELECT)) select_used = 0;
    if (btn_released(BTN_SELECT) && !select_used) c->camera = 1;
    if (btn_pressed(BTN_START)) c->pause = 1;
}

/* ---- aiming ------------------------------------------------------------------------- */
static fix16 pitch_slope(int pitch) { return (fix16)pitch * 215; }

/* Best target near the aim direction for vertical auto-aim / lock-on. */
static int find_target(fix16 ox, fix16 oy, int oz, int ang, int cone, int range_units)
{
    int i, best = -1, best_score = 1 << 30;
    for (i = 0; i < ACTOR_MAX; i++) {
        const DoomoActor *a = &doomo_actors[i];
        fix16 d;
        int ta, diff, score, size;
        if (!(a->flags & AF_SHOOTABLE) || !IS_DEMON(a->type) || a->hp <= 0) continue;
        if (a->state == AST_DORMANT) continue;
        d = fx_dist(a->x - ox, a->y - oy);
        if (d > INT2FIX(range_units) || d < INT2FIX(8)) continue;
        ta = fx_atan2(a->y - oy, a->x - ox);
        diff = ((ta - ang + 2048) & DOOMO_ANG_MASK) - 2048;
        if (diff < 0) diff = -diff;
        /* angular half-size of the target */
        size = (int)(((int64_t)a->radius * 652) / DOOMO_MAX(FIX2INT(d), 1));
        if (diff > cone + size) continue;
        if (!cw_los(ox, oy, oz, a->x, a->y, FIX2INT(a->z) + a->height / 2)) continue;
        score = diff * 4 + FIX2INT(d) / 8;
        if (score < best_score) { best_score = score; best = i; }
    }
    return best;
}

static void aim_solution(fix16 ox, fix16 oy, int oz, int *ang, fix16 *slope)
{
    int level = opt_get(OPT_AIM_ASSIST);
    static const int16_t cones[4] = { 0, 40, 80, 130 };
    int t = -1;
    *slope = pitch_slope(doomo_player.pitch);
    if (doomo_player.lock_target >= 0) t = doomo_player.lock_target;
    else if (level > 0) t = find_target(ox, oy, oz, *ang, cones[level] + (combat_ws.fx & WFX_HOMING ? 40 : 0),
                                        combat_ws.range * CELLU);
    if (t >= 0) {
        const DoomoActor *a = &doomo_actors[t];
        fix16 d = fx_dist(a->x - ox, a->y - oy);
        int ta = fx_atan2(a->y - oy, a->x - ox);
        int diff = ((ta - *ang + 2048) & DOOMO_ANG_MASK) - 2048;
        int aimz = FIX2INT(a->z) + a->height * 3 / 5;
        if (d < INT2FIX(8)) d = INT2FIX(8);
        *slope = FIXDIV(INT2FIX(aimz - oz), d);
        /* horizontal magnetism */
        if (doomo_player.lock_target >= 0) *ang = ta;
        else *ang += diff * level / 6;
    }
}

/* ---- firing ------------------------------------------------------------------------- */
typedef struct HitCand { int idx; fix16 t; } HitCand;

static void player_hit_actor(int k, int dmg, int ang, int knock, int fx, fix16 hx, fix16 hy, fix16 hz,
                             int head)
{
    DoomoActor *o = &doomo_actors[k];
    int died, before = o->hp;
    int is_demon = IS_DEMON(o->type);
    if (combat_feel() && is_demon) knock = knock * (combat_feel() + 2) / 2 + 2 * combat_feel();   /* shove them back */
    died = actor_damage(k, dmg, PLAYER_OWNER, ang, knock, fx, head);
    if (!is_demon) return;
    combat_st.hits++;
    combat_add_dmgnum(hx, hy, hz, DOOMO_MIN(dmg, before > 0 ? before : dmg), head);
    combat_px.hit_marker = 6;
    if (head) combat_px.hit_head = 8;
    if (!died) {
        int style = combat_gore_style();
        if (style == WSTYLE_CUTE || combat_gore_mixed())
            actor_fx(rng_chance(&doomo_rng, 50) ? SPR_SPARKLE : SPR_HEARTS, hx, hy, hz, 2);
        if (style != WSTYLE_CUTE) actor_fx(opt_get(OPT_BLOOD_COLOR) == 1 ? ART_BASE + ART_SPLAT_GREEN : SPR_BLOOD_SPRAY, hx, hy, hz, 2);
        if (combat_feel() > 1 && style != WSTYLE_CUTE)       /* a second spray and a chunk */
            actor_gib(opt_get(OPT_BLOOD_COLOR) == 1 ? ART_BASE + ART_SLIME : SPR_GIBS, hx, hy, hz,
                      FIXMUL(fx_cos(ang), INT2FIX(2)), FIXMUL(fx_sin(ang), INT2FIX(2)), INT2FIX(3));
    }
}

static void fire_hitscan(fix16 ox, fix16 oy, fix16 oz, int ang, fix16 slope, int pierce_all, int dmg,
                         int range_units)
{
    fix16 dx = fx_cos(ang), dy = fx_sin(ang);
    fix16 hx, hy, hz, wd;
    HitCand c[6];
    int n = 0, i, k;
    wd = cw_trace(ox, oy, oz, dx, dy, slope, INT2FIX(range_units), &hx, &hy, &hz);
    for (k = 0; k < ACTOR_MAX; k++) {
        const DoomoActor *o = &doomo_actors[k];
        fix16 rx, ry, t, perp, zt;
        if (!(o->flags & AF_SHOOTABLE) || o->hp <= 0) continue;
        rx = o->x - ox; ry = o->y - oy;
        t = FIXMUL(rx, dx) + FIXMUL(ry, dy);
        if (t <= 0 || t > wd + INT2FIX(o->radius)) continue;
        perp = FIXMUL(rx, dy) - FIXMUL(ry, dx);
        if (perp < 0) perp = -perp;
        if (perp > INT2FIX(o->radius)) continue;
        zt = oz + FIXMUL(slope, t);
        if (zt < o->z - INT2FIX(4) || zt > o->z + INT2FIX(o->height + 4)) continue;
        /* keep the nearest few, sorted */
        if (n < 6) n++;
        for (i = n - 1; i > 0 && c[i - 1].t > t; i--) c[i] = c[i - 1];
        if (i < 6) { c[i].idx = k; c[i].t = t; }
    }
    if (n == 0) {
        /* world impact */
        actor_fx(SPR_PUFF, hx - dx * 2, hy - dy * 2, hz, 2);
        if ((combat_px.shots_fired & 3) == 0) actor_fx(SPR_SPARKLE, hx - dx * 2, hy - dy * 2, hz, 2);
        return;
    }
    for (i = 0; i < n; i++) {
        const DoomoActor *o = &doomo_actors[c[i].idx];
        fix16 hzt = oz + FIXMUL(slope, c[i].t);
        int head = IS_DEMON(o->type) && FIX2INT(hzt) > FIX2INT(o->z) + o->height * 3 / 4;
        player_hit_actor(c[i].idx, dmg, ang, combat_ws.knockback, combat_ws.fx,
                         ox + FIXMUL(dx, c[i].t), oy + FIXMUL(dy, c[i].t), hzt, head);
        if (!pierce_all && !(combat_ws.fx & WFX_PIERCE)) break;
        if (!pierce_all && i >= 1) break;
    }
}

static void fire_melee(fix16 ox, fix16 oy, int oz, int ang, int dmg)
{
    int k, hits = 0, arc = DOOMO_MAX(combat_ws.spread * 8, 200);
    int reach = 64 + (combat_ws.knockback > 20 ? 12 : 0);
    for (k = 0; k < ACTOR_MAX; k++) {
        const DoomoActor *o = &doomo_actors[k];
        fix16 d;
        int ta, diff;
        if (!(o->flags & AF_SHOOTABLE) || o->hp <= 0) continue;
        d = fx_dist(o->x - ox, o->y - oy);
        if (d > INT2FIX(reach + o->radius)) continue;
        if (oz < FIX2INT(o->z) - 24 || oz > FIX2INT(o->z) + o->height + 24) continue;
        ta = fx_atan2(o->y - oy, o->x - ox);
        diff = ((ta - ang + 2048) & DOOMO_ANG_MASK) - 2048;
        if (diff < -arc || diff > arc) continue;
        player_hit_actor(k, dmg, ang, combat_ws.knockback, combat_ws.fx, o->x, o->y,
                         o->z + INT2FIX(o->height / 2), 0);
        hits++;
        if (combat_ws.spread == 0) break;                /* fists / saw: one at a time */
    }
    if (hits) {
        combat_shake(combat_ws.knockback / 6 + 2);
        combat_px.hitstop = DOOMO_MAX(combat_px.hitstop, 2);
        if (combat_ws.splash) actor_splash(ox + FIXMUL(fx_cos(ang), INT2FIX(48)),
                                           oy + FIXMUL(fx_sin(ang), INT2FIX(48)), oz - 24,
                                           combat_ws.splash * 2, dmg / 2, PLAYER_OWNER, combat_ws.fx);
    }
    audio_sfx(hits ? combat_ws.sfx : SFX_CURSOR);
}

static void shot_origin(fix16 *ox, fix16 *oy, int *oz, int *ang, fix16 *slope)
{
    combat_player_eye(ox, oy, oz);
    *ang = doomo_player.angle;
    if (doomo_player.camera == CAM_THIRD_PERSON && combat_px.cam_ok) {
        /* converge: shoot from the player's chest toward what the camera's
         * centre ray hits */
        fix16 cx = combat_px.cam_x, cy = combat_px.cam_y, cz = combat_px.cam_z;
        fix16 hx, hy, hz, dx = fx_cos(doomo_player.angle), dy = fx_sin(doomo_player.angle);
        int tgt;
        cw_trace(cx, cy, cz, dx, dy, pitch_slope(doomo_player.pitch - TP_TILT), INT2FIX(40 * CELLU), &hx, &hy, &hz);
        tgt = find_target(cx, cy, FIX2INT(cz), doomo_player.angle, 30, 40 * CELLU);
        *oz -= 10;
        if (tgt < 0 && doomo_player.lock_target < 0) {
            fix16 d = fx_dist(hx - *ox, hy - *oy);
            *ang = fx_atan2(hy - *oy, hx - *ox);
            *slope = d > INT2FIX(8) ? FIXDIV(hz - INT2FIX(*oz), d) : 0;
            return;
        }
    }
    aim_solution(*ox, *oy, *oz, ang, slope);
}

static void fire_under(fix16 ox, fix16 oy, int oz, int ang, fix16 slope)
{
    const DoomoWeapon *w = combat_weapon();
    switch (w->under) {
    case WUNDER_GRENADE:
        actor_projectile(PK_GRENADE, PLAYER_OWNER, ox, oy, INT2FIX(oz - 4), ang, slope + INT2FIX(1) / 5,
                         20, 50, WFX_SPLASH | WFX_BOUNCE, 40, NO_TARGET);
        audio_sfx(SFX_ROCKET);
        break;
    case WUNDER_SHOTGUN: {
        int p;
        for (p = 0; p < 6; p++)
            fire_hitscan(ox, oy, INT2FIX(oz - 4), ang + rng_between(&doomo_rng, -90, 90),
                         slope + rng_between(&doomo_rng, -900, 900), 0, 8, 20 * CELLU);
        audio_sfx(SFX_SHOTGUN);
        break;
    }
    case WUNDER_BUBBLE:
        actor_projectile(PK_BUBBLE, PLAYER_OWNER, ox, oy, INT2FIX(oz - 6), ang, slope, 7, 15,
                         WFX_FREEZE | WFX_SPLASH, 10, NO_TARGET);
        break;
    default:
        break;
    }
}

static void fire_weapon(int charge_mul16)
{
    const DoomoWeaponStats *s = &combat_ws;
    fix16 ox, oy, slope;
    int oz, ang, p, dmg;
    int slot = doomo_player.weapon_slot;
    /* ammo */
    if (s->ammo_type != 0xFF) {
        int infinite = s->ammo_type == AMMO_CANDY && opt_get(OPT_CHEAT_INFINITE_CANDY);
        if (s->mag_size) {
            if (combat_px.mag[slot] < s->ammo_use) return;
            if (!infinite) combat_px.mag[slot] = (uint16_t)(combat_px.mag[slot] - s->ammo_use);
        } else {
            if (combat_ammo(s->ammo_type) < s->ammo_use) return;
            if (!infinite) ammo_add(s->ammo_type, -s->ammo_use);
        }
    }
    shot_origin(&ox, &oy, &oz, &ang, &slope);
    dmg = s->damage * charge_mul16 / 16;
    if (pet_combat_buff() > 0) dmg = dmg * (100 + DOOMO_MIN(pet_combat_buff(), 50)) / 100;
    if (dmg < 1) dmg = 1;
    combat_st.shots++;
    combat_px.shots_fired++;
    if (s->behaviour != WB_MELEE) {
        combat_px.flash = 3;
        combat_px.kick = 16 * DOOMO_MIN(s->recoil / 2 + 3, 14);
        doomo_player.recoil = (int16_t)DOOMO_MIN(doomo_player.recoil + s->recoil / 2 + 1, 48);
        if (s->recoil >= 18) combat_shake(s->recoil / 6);
        if (combat_feel()) {
            /* Brutal-Doom punch: the gun bucks, the view jolts, the muzzle blazes */
            int f = combat_feel();
            combat_px.flash = f > 1 ? 5 : 4;
            combat_px.kick = 16 * DOOMO_MIN((s->recoil / 2 + 4) * (f + 1) / 2, 20);
            doomo_player.recoil = (int16_t)DOOMO_MIN(doomo_player.recoil + (s->recoil / 2 + 2) * (f + 1) / 2, 72);
            combat_shake(DOOMO_MAX(1 + s->damage / 12, s->recoil / 4) * f / 2 + 1);
            if (s->pellets > 3 || s->recoil >= 24)     /* shotguns / cannons shove the view sideways */
                doomo_player.angle = (int16_t)((doomo_player.angle + rng_between(&doomo_rng, -24, 24) * f) & DOOMO_ANG_MASK);
        }
    } else {
        combat_px.kick = 16 * 10;
    }
    if (!s->quiet) actor_wake_all(FIX2INT(ox) / CELLU, FIX2INT(oy) / CELLU, 9);
    audio_sfx(s->sfx);
    combat_px.face_expr = EXPR_ANGRY;
    combat_px.face_t = 12;
    switch (s->behaviour) {
    case WB_MELEE:
        fire_melee(ox, oy, oz, ang, dmg);
        break;
    case WB_RAIL: {
        fix16 dx = fx_cos(ang), dy = fx_sin(ang), hx, hy, hz, d, t;
        fire_hitscan(ox, oy, INT2FIX(oz), ang, slope, 1, dmg, s->range * CELLU);
        d = cw_trace(ox, oy, INT2FIX(oz), dx, dy, slope, INT2FIX(s->range * CELLU), &hx, &hy, &hz);
        for (t = INT2FIX(24); t < d; t += INT2FIX(40))
            actor_fx(SPR_SPARKLE, ox + FIXMUL(dx, t), oy + FIXMUL(dy, t), INT2FIX(oz) + FIXMUL(slope, t) - INT2FIX(4), 2);
        actor_fx(SPR_PLASMA, hx, hy, hz, 3);
        combat_shake(4);
        break;
    }
    case WB_HITSCAN:
        for (p = 0; p < s->pellets; p++) {
            int sp = s->spread * 2;
            int a = ang + (sp ? rng_between(&doomo_rng, -sp, sp) : 0);
            fix16 sl = slope + (sp ? rng_between(&doomo_rng, -sp, sp) * 140 : 0);
            if (doomo_player.crouch) { a = ang + (a - ang) / 2; sl = slope + (sl - slope) / 2; }
            fire_hitscan(ox, oy, INT2FIX(oz), a, sl, 0, dmg, s->range * CELLU);
        }
        break;
    default: {
        /* projectiles, lobs, floaty bubbles, streams */
        fix16 fwd = INT2FIX(12);
        int z0 = oz - 8;
        for (p = 0; p < s->pellets; p++) {
            int sp = s->spread * 2;
            int a = ang + (sp ? rng_between(&doomo_rng, -sp, sp) : 0);
            fix16 sl = slope + (sp ? rng_between(&doomo_rng, -sp, sp) * 140 : 0);
            if (s->behaviour == WB_LOB) sl += INT2FIX(1) / 4;
            actor_projectile(s->pkind, PLAYER_OWNER, ox + FIXMUL(fx_cos(ang), fwd),
                             oy + FIXMUL(fx_sin(ang), fwd), INT2FIX(z0), a, sl, s->speed, dmg,
                             s->fx, (s->fx & WFX_SPLASH) ? s->splash : 0,
                             NO_TARGET);
        }
        break;
    }
    }
    if (s->under_every && (combat_px.shots_fired % s->under_every) == 0)
        fire_under(ox, oy, oz, ang, slope);
    /* shell casings */
    if (s->behaviour == WB_HITSCAN && s->ammo_type == AMMO_BULLETS && (combat_px.shots_fired & 1)) {
        int side = (doomo_player.angle + 1024) & DOOMO_ANG_MASK;
        actor_gib(ART_BASE + ART_SHELL_CASING, ox + FIXMUL(fx_cos(side), INT2FIX(8)),
                  oy + FIXMUL(fx_sin(side), INT2FIX(8)), INT2FIX(oz - 10),
                  FIXMUL(fx_cos(side), INT2FIX(2)), FIXMUL(fx_sin(side), INT2FIX(2)), INT2FIX(3));
    }
}

static int start_reload(void)
{
    const DoomoWeaponStats *s = &combat_ws;
    int slot = doomo_player.weapon_slot;
    if (!s->mag_size || s->ammo_type == 0xFF || combat_px.reload_t) return 0;
    if (combat_px.mag[slot] >= s->mag_size) return 0;
    if (combat_ammo(s->ammo_type) <= 0) return 0;
    combat_px.reload_t = s->reload;
    audio_sfx(SFX_BLIP);
    return 1;
}

static void finish_reload(void)
{
    const DoomoWeaponStats *s = &combat_ws;
    int slot = doomo_player.weapon_slot;
    int need = s->mag_size - combat_px.mag[slot];
    int have = combat_ammo(s->ammo_type);
    int take = DOOMO_MIN(need, have);
    if (s->ammo_type == AMMO_CANDY && opt_get(OPT_CHEAT_INFINITE_CANDY)) take = need;
    else ammo_add(s->ammo_type, -take);
    combat_px.mag[slot] = (uint16_t)(combat_px.mag[slot] + take);
    audio_sfx(SFX_SELECT);
}

static void weapon_tick(const CombatCtl *c)
{
    const DoomoWeaponStats *s = &combat_ws;
    int slot = doomo_player.weapon_slot;
    int feel = input_feel();
    /* trigger buffer (feel): a press just before the gun is ready still fires */
    if (feel) {
        if (c->fire_pressed) combat_px.fire_buf = 6;
        else if (combat_px.fire_buf > 0) combat_px.fire_buf--;
    }
    /* switching: lower, swap, raise */
    if (combat_px.switch_to >= 0) {
        combat_px.raise += 48;
        if (combat_px.raise >= 256) {
            combat_px.raise = 256;
            doomo_player.weapon_slot = (uint8_t)combat_px.switch_to;
            doomo_town.story.equipped = (uint8_t)combat_px.switch_to;
            combat_px.switch_to = -1;
            refresh_stats();
            combat_px.spin = 0;
        }
        return;
    }
    if (combat_px.raise > 0) {
        combat_px.raise -= 40;
        if (combat_px.raise < 0) combat_px.raise = 0;
        return;
    }
    if (doomo_player.refire) doomo_player.refire--;
    if (combat_px.reload_t) {
        if (--combat_px.reload_t == 0) finish_reload();
        return;
    }
    /* minigun spin */
    if (s->spin_up) {
        if (c->fire) combat_px.spin = DOOMO_MIN(combat_px.spin + 256 / s->spin_up + 1, 256);
        else combat_px.spin = DOOMO_MAX(combat_px.spin - 16, 0);
    }
    /* bursts in progress */
    if (combat_px.burst_left > 0) {
        if (--combat_px.burst_t <= 0) {
            fire_weapon(16);
            combat_px.burst_left--;
            combat_px.burst_t = 2;
        }
        return;
    }
    /* out of ammo in the magazine: reload or switch */
    if (s->ammo_type != 0xFF) {
        int have = s->mag_size ? combat_px.mag[slot] : combat_ammo(s->ammo_type);
        if (have < s->ammo_use) {
            if (start_reload()) return;
            if (c->fire_pressed && msg_cool == 0) {
                combat_toast("Out of ammo! " STR_ICON_ARROW_R " next weapon");
                msg_cool = 45;
                audio_sfx(SFX_ERROR);
                {
                    int i;
                    for (i = 1; i < DOOMO_MAX_WEAPONS; i++) {
                        int k = (slot + i) % DOOMO_MAX_WEAPONS;
                        if (weapon_has_ammo(k)) { combat_select_weapon(k); break; }
                    }
                }
            }
            return;
        }
    }
    /* charge weapons (bows, charge shot): hold, release to fire */
    if (s->charge) {
        if (c->fire && doomo_player.refire == 0) {
            if (combat_px.charge_t < s->charge) combat_px.charge_t++;
            return;
        }
        if (combat_px.charge_t > 0 && !c->fire) {
            int mul = 8 + combat_px.charge_t * 32 / s->charge;      /* x0.5 .. x2.5 */
            fire_weapon(mul);
            combat_px.charge_t = 0;
            doomo_player.refire = s->refire;
        }
        return;
    }
    if (doomo_player.refire == 0 && (c->fire_pressed || (feel && combat_px.fire_buf > 0) ||
                                     (c->fire && (s->auto_fire || s->behaviour == WB_MELEE)))) {
        int refire = s->refire;
        if (s->spin_up) {
            if (combat_px.spin < 96) return;                          /* still spinning up */
            refire = refire + (256 - combat_px.spin) / 64;
        }
        combat_px.fire_buf = 0;
        fire_weapon(16);
        if (s->burst > 1) { combat_px.burst_left = s->burst - 1; combat_px.burst_t = 2; }
        doomo_player.refire = (uint16_t)refire;
        combat_px.stand_still = 0;
    } else if (!c->fire) {
        /* tactical reload when idle */
        combat_px.stand_still++;
        if (combat_px.stand_still > 75 && s->mag_size && combat_px.mag[slot] < s->mag_size / 2)
            start_reload();
    }
}

/* ---- pickups ---------------------------------------------------------------------------- */
static void pickups_tick(void)
{
    int i, pz = FIX2INT(doomo_player.z);
    for (i = 0; i < ACTOR_MAX; i++) {
        DoomoActor *a = &doomo_actors[i];
        fix16 rr;
        int got = 1;
        const char *msg = NULL;
        if (!(a->flags & AF_PICKUP)) continue;
        rr = INT2FIX(a->radius + PLAYER_RADIUS);
        if (a->x - doomo_player.x > rr || doomo_player.x - a->x > rr ||
            a->y - doomo_player.y > rr || doomo_player.y - a->y > rr) continue;
        if (FIX2INT(a->z) > pz + 40 || FIX2INT(a->z) < pz - 32) continue;
        switch (a->type) {
        case THING_HEALTH_SMALL: if (doomo_player.hp >= 100) got = 0; else { heal(10, 100); msg = "Bandage +10"; } break;
        case THING_HEALTH_BIG: if (doomo_player.hp >= 100) got = 0; else { heal(25, 100); msg = "First aid kit +25"; } break;
        case THING_SOUL_ORB: heal(50, 200); msg = "SOUL ORB! +50"; doomo_player.pickup_flash = 12; break;
        case THING_ARMOR:
            if (doomo_player.armor >= 100) got = 0;
            else { doomo_player.armor = (int16_t)DOOMO_MIN(doomo_player.armor + 50, 100); msg = "Plush armor +50"; }
            break;
        case THING_AMMO_BULLETS: ammo_add(AMMO_BULLETS, 20); ammo_add(AMMO_NAILS, 10); msg = "Bullets"; break;
        case THING_AMMO_SHELLS: ammo_add(AMMO_SHELLS, 8); msg = "Shells"; break;
        case THING_AMMO_CELLS: ammo_add(AMMO_CELLS, 40); ammo_add(AMMO_FUEL, 40); msg = "Energy cells"; break;
        case THING_AMMO_ROCKETS: ammo_add(AMMO_ROCKETS, 4); msg = "Rockets"; break;
        case THING_CANDY: ammo_add(AMMO_CANDY, 25); msg = "Candy ammo"; break;
        case THING_KEY_RED: doomo_player.keys |= 1; msg = "Got the RED heart key!"; break;
        case THING_KEY_BLUE: doomo_player.keys |= 2; msg = "Got the BLUE heart key!"; break;
        case THING_SCRAP: inv_add(ITEM_SCRAP, 1); if (rng_chance(&doomo_rng, 30)) inv_add(ITEM_GEARS, 1);
            msg = "Scrap for welding"; break;
        default: got = 0; break;
        }
        if (!got) continue;
        if (a->tflags & THF_SECRET) {
            combat_st.secrets++;
            progress_add(PROG_SECRETS_FOUND, 1);
        }
        combat_st.items++;
        a->flags = 0;
        doomo_player.pickup_flash = (uint16_t)DOOMO_MAX(doomo_player.pickup_flash, 6);
        combat_px.face_expr = EXPR_HAPPY;
        combat_px.face_t = 20;
        if (msg) combat_toast(msg);
        audio_sfx(SFX_PICKUP);
    }
}

/* ---- the player ------------------------------------------------------------------------ */
static int player_height(void) { return doomo_player.crouch ? DOOMO_CROUCH_HEIGHT : DOOMO_PLAYER_HEIGHT; }

static int player_fits(fix16 x, fix16 y, int z, int h, int *fl, int *ce)
{
    int step = DOOMO_STEP_UP, i;
    if (!cw_fits(x, y, PLAYER_RADIUS, z, h, step, fl, ce)) return 0;
    for (i = 0; i < ACTOR_MAX; i++) {
        const DoomoActor *o = &doomo_actors[i];
        fix16 rr, ddx, ddy;
        if ((o->flags & (AF_USED | AF_SOLID)) != (AF_USED | AF_SOLID)) continue;
        rr = INT2FIX(o->radius + PLAYER_RADIUS - 2);
        ddx = o->x - x; ddy = o->y - y;
        if (ddx > rr || ddx < -rr || ddy > rr || ddy < -rr) continue;
        if (FIX2INT(o->z) >= z + h || FIX2INT(o->z) + o->height <= z + DOOMO_STEP_UP) continue;
        if (fx_dist(ddx, ddy) >= rr) continue;
        /* allow moving away from something we already overlap */
        if (fx_dist(o->x - doomo_player.x, o->y - doomo_player.y) < fx_dist(ddx, ddy)) continue;
        return 0;
    }
    return 1;
}

static void bump_doors(fix16 x, fix16 y)
{
    int x0 = (FIX2INT(x) - PLAYER_RADIUS - 2) / CELLU, x1 = (FIX2INT(x) + PLAYER_RADIUS + 2) / CELLU;
    int y0 = (FIX2INT(y) - PLAYER_RADIUS - 2) / CELLU, y1 = (FIX2INT(y) + PLAYER_RADIUS + 2) / CELLU;
    int cx, cy;
    for (cy = y0; cy <= y1; cy++)
        for (cx = x0; cx <= x1; cx++) {
            const DoomoCell *c = map_cell_safe(&doomo_map, cx, cy);
            int r;
            if (!(c->flags & CELL_DOOR)) continue;
            r = map_door_bump(&doomo_map, cx, cy, doomo_player.keys);
            if (r == 0 && msg_cool == 0) {
                msg_cool = 60;
                if (c->tag == DOOMO_DOOR_TAG_RED) combat_toast("Locked: needs the RED heart key");
                else if (c->tag == DOOMO_DOOR_TAG_BLUE) combat_toast("Locked: needs the BLUE heart key");
                else combat_toast("This door opens somewhere else...");
                audio_sfx(SFX_ERROR);
            }
        }
}

static int try_move(fix16 dx, fix16 dy)
{
    int fl, ce, z = FIX2INT(doomo_player.z), h = player_height();
    fix16 nx = doomo_player.x + dx, ny = doomo_player.y + dy;
    if (player_fits(nx, ny, z, h, &fl, &ce)) {
        doomo_player.x = nx;
        doomo_player.y = ny;
        return 1;
    }
    /* auto-crouch into low passages */
    if (!doomo_player.crouch && player_fits(nx, ny, z, DOOMO_CROUCH_HEIGHT, &fl, &ce)) {
        doomo_player.crouch = 2;                     /* 2 = automatic */
        doomo_player.x = nx;
        doomo_player.y = ny;
        return 1;
    }
    bump_doors(nx, ny);
    return 0;
}

static void cell_specials(void)
{
    int cx = FIX2INT(doomo_player.x) / CELLU, cy = FIX2INT(doomo_player.y) / CELLU;
    DoomoCell *c = (DoomoCell *)map_cell_safe(&doomo_map, cx, cy);
    int pz = FIX2INT(doomo_player.z), fl = cw_floor(cx, cy);
    int entered = cx != combat_px.last_cx || cy != combat_px.last_cy;
    if (cx < 0 || cy < 0 || cx >= doomo_map.w || cy >= doomo_map.h) return;
    if (entered) {
        combat_px.last_cx = cx;
        combat_px.last_cy = cy;
        combat_px.lift_wait = 0;
        combat_px.lift_used = 0;
        if (c->flags & CELL_TRIGGER) {
            int tag = c->tag;
            c->flags &= (uint8_t)~CELL_TRIGGER;
            combat_event(tag);
            story_trigger(tag);
        }
        if (c->flags & CELL_SECRET) {
            mark_secret_region(cy * doomo_map.w + cx);
            combat_st.secrets++;
            progress_add(PROG_SECRETS_FOUND, 1);
            combat_toast("A SECRET! " STR_ICON_STAR);
            audio_sfx(SFX_SPARKLE);
            doomo_player.pickup_flash = 10;
        }
    }
    if ((c->flags & CELL_EXIT) && combat_s.goal == CGOAL_EXIT && !combat_s.exit_t && doomo_player.hp > 0) {
        combat_s.exit_t = 1;
        audio_sfx(SFX_EVOLVE);
    }
    if ((c->flags & CELL_LAVA) && pz <= fl + 2 && doomo_player.hp > 0) {
        if (++combat_px.lava_t >= 16) {
            combat_px.lava_t = 0;
            combat_hurt_player(8, doomo_player.angle, 0, -1);
            actor_fx(SPR_FIREBALL, doomo_player.x, doomo_player.y, doomo_player.z + INT2FIX(8), 3);
        }
    } else {
        combat_px.lava_t = 12;
    }
    if ((c->flags & CELL_LIFT) && doomo_player.on_ground && !combat_px.lift_used) {
        if (!map_lift_busy(&doomo_map, cx, cy) && ++combat_px.lift_wait >= 8) {
            map_use(&doomo_map, cx, cy);
            combat_px.lift_used = 1;
        }
    }
}

static void player_dead_tick(void)
{
    combat_px.dead_t++;
    combat_px.vx = combat_px.vx * 3 / 4;
    combat_px.vy = combat_px.vy * 3 / 4;
    if (combat_px.view_h > INT2FIX(8)) combat_px.view_h -= INT2FIX(2);
    if (doomo_player.pitch < 120) doomo_player.pitch += 6;
    if (combat_px.dead_t > 50 && !combat_s.finished) {
        combat_s.finished = -1;
        last_result = -1;
    }
}

static void update_lock(const CombatCtl *c)
{
    int mode = opt_get(OPT_LOCK_ON);                    /* 0 hold, 1 toggle */
    int scoped = combat_ws.zoom >= 2;
    if (scoped) {
        combat_px.ads = c->lock;
        doomo_player.lock_target = -1;
    } else {
        int want;
        combat_px.ads = 0;
        if (mode == 0) want = c->lock;
        else {
            want = doomo_player.lock_target >= 0;
            if (c->lock_pressed) want = !want;
        }
        if (!want) doomo_player.lock_target = -1;
        else if (doomo_player.lock_target < 0 && (c->lock_pressed || mode == 0)) {
            fix16 ex, ey;
            int ez;
            combat_player_eye(&ex, &ey, &ez);
            doomo_player.lock_target = (int16_t)find_target(ex, ey, ez, doomo_player.angle, 600, 24 * CELLU);
            if (doomo_player.lock_target >= 0) { audio_sfx(SFX_BLIP); combat_px.lock_ui = 0; lock_lost = 0; }
        }
    }
    if (doomo_player.lock_target >= 0) {
        const DoomoActor *t = &doomo_actors[doomo_player.lock_target];
        if (!(t->flags & AF_SHOOTABLE) || t->hp <= 0 || !IS_DEMON(t->type)) {
            doomo_player.lock_target = -1;
        } else {
            fix16 ex, ey;
            int ez;
            combat_player_eye(&ex, &ey, &ez);
            if (!cw_los(ex, ey, ez, t->x, t->y, FIX2INT(t->z) + t->height / 2)) {
                if (++lock_lost > 30) doomo_player.lock_target = -1;
            } else {
                lock_lost = 0;
            }
        }
        combat_px.lock_ui++;
    }
    {
        int target = combat_px.ads ? 256 : 0;
        if (combat_px.zoom_t < target) combat_px.zoom_t = DOOMO_MIN(combat_px.zoom_t + 48, 256);
        else if (combat_px.zoom_t > target) combat_px.zoom_t = DOOMO_MAX(combat_px.zoom_t - 64, 0);
    }
}

static void player_tick(const CombatCtl *c)
{
    fix16 wishx = 0, wishy = 0, speed, accel, fwdx, fwdy;
    int fl, ce, h, z, eye, look_level, i, moving;
    if (doomo_player.hp <= 0) { player_dead_tick(); return; }
    if (c->camera) combat_toggle_camera();
    if (c->next_weapon) combat_next_weapon(1);
    update_lock(c);
    /* turning: lock-on steers the view */
    if (doomo_player.lock_target >= 0) {
        const DoomoActor *t = &doomo_actors[doomo_player.lock_target];
        int want = fx_atan2(t->y - doomo_player.y, t->x - doomo_player.x);
        int diff = ((want - doomo_player.angle + 2048) & DOOMO_ANG_MASK) - 2048;
        doomo_player.angle = (int16_t)((doomo_player.angle + DOOMO_CLAMP(diff / 3, -110, 110)) & DOOMO_ANG_MASK);
    }
    {
        int turn = c->turn;
        if (combat_px.zoom_t > 128) turn /= 3;                      /* steady aim when scoped */
        else if (combat_ws.weight > 70) turn = turn * 4 / 5;
        /* aim friction (feel): the view slows as the reticle crosses a demon,
         * so the stick can settle on a target instead of skating past it */
        if (turn && input_feel() && doomo_player.lock_target < 0 && opt_get(OPT_AIM_ASSIST) > 0) {
            fix16 ex, ey;
            int ez;
            combat_player_eye(&ex, &ey, &ez);
            if (find_target(ex, ey, ez, doomo_player.angle, 50 + opt_get(OPT_AIM_ASSIST) * 24,
                            DOOMO_MIN(combat_ws.range, 24) * CELLU) >= 0)
                turn = turn * 11 / 20;
        }
        if (doomo_player.lock_target >= 0 && turn) { /* turning breaks a hold lock */ }
        doomo_player.angle = (int16_t)((doomo_player.angle + turn) & DOOMO_ANG_MASK);
    }
    /* pitch: manual look, else drift back to level (modern / auto-aim) */
    if (c->look) {
        doomo_player.pitch = (int16_t)DOOMO_CLAMP(doomo_player.pitch + c->look, -200, 200);
    } else if (opt_get(OPT_CONTROL_SCHEME) == CONTROLS_MODERN || doomo_player.lock_target >= 0) {
        int want = 0;
        if (doomo_player.lock_target >= 0) {
            const DoomoActor *t = &doomo_actors[doomo_player.lock_target];
            fix16 d = fx_dist(t->x - doomo_player.x, t->y - doomo_player.y);
            int ez = FIX2INT(doomo_player.z) + FIX2INT(combat_px.view_h);
            if (d > INT2FIX(16))
                want = DOOMO_CLAMP(FIXDIV(INT2FIX(FIX2INT(t->z) + t->height / 2 - ez), d) / 215, -160, 160);
        }
        doomo_player.pitch = (int16_t)(doomo_player.pitch + (want - doomo_player.pitch) / 4);
    }
    /* recoil recovers */
    if (doomo_player.recoil > 0) doomo_player.recoil = (int16_t)(doomo_player.recoil * 3 / 4);
    /* crouch */
    if (c->crouch_toggle) doomo_player.crouch = (uint8_t)(doomo_player.crouch ? 0 : 1);
    if (doomo_player.crouch) {
        /* stand up when there is room (auto-crouch stands up by itself) */
        if ((doomo_player.crouch == 2 || c->jump) &&
            cw_fits(doomo_player.x, doomo_player.y, PLAYER_RADIUS, FIX2INT(doomo_player.z),
                    DOOMO_PLAYER_HEIGHT, 0, &fl, &ce))
            doomo_player.crouch = 0;
    }
    /* dodge-roll */
    if (combat_px.roll_cool) combat_px.roll_cool--;
    if (c->roll && !doomo_player.roll_timer && !combat_px.roll_cool && doomo_player.on_ground) {
        int dir = doomo_player.angle;
        if (c->roll_dir == -1 || (c->roll_dir == 0 && c->strafe < 0)) dir += 3072;
        else if (c->roll_dir == 1 || (c->roll_dir == 0 && c->strafe > 0)) dir += 1024;
        else if (c->roll_dir == 2 || (c->roll_dir == 0 && c->move < 0)) dir += 2048;
        dir &= DOOMO_ANG_MASK;
        doomo_player.roll_timer = 12;
        combat_px.roll_dx = fx_cos(dir);
        combat_px.roll_dy = fx_sin(dir);
        combat_px.roll_cool = 26;
        audio_sfx(SFX_CURSOR);
    }
    if (doomo_player.roll_timer) {
        doomo_player.roll_timer--;
        combat_px.invuln = doomo_player.roll_timer >= 3 && doomo_player.roll_timer <= 10 ? 1 : 0;
    } else {
        combat_px.invuln = 0;
    }
    /* movement wish */
    fwdx = fx_cos(doomo_player.angle);
    fwdy = fx_sin(doomo_player.angle);
    speed = WALK_SPEED;
    if (combat_feel()) speed = speed * 5 / 4;                  /* Doom runs fast */
    doomo_player.sprint = (uint8_t)(c->sprint && c->move > 0 && !doomo_player.crouch && combat_px.zoom_t < 64);
    if (doomo_player.sprint) speed = speed * 8 / 5;
    if (doomo_player.crouch) speed /= 2;
    if (combat_px.zoom_t > 64) speed = speed * 3 / 5;
    speed = speed * (100 - combat_ws.weight / 3) / 100;
    {
        const DoomoCell *cc = map_cell_safe(&doomo_map, FIX2INT(doomo_player.x) / CELLU, FIX2INT(doomo_player.y) / CELLU);
        if (cc->flags & CELL_WATER) speed = speed * 3 / 5;
    }
    moving = c->move || c->strafe || c->fmove || c->fstrafe;
    if (c->fmove || c->fstrafe) {
        /* analog stick: speed scales with how far it is pushed */
        int fm = c->fmove, fs = c->fstrafe;
        int m = (int)(fx_dist(INT2FIX(fm), INT2FIX(fs)) >> 16);
        fix16 mx, my;
        if (m > 256) { fm = fm * 256 / m; fs = fs * 256 / m; }
        mx = (fwdx * fm - fwdy * fs) / 256;
        my = (fwdy * fm + fwdx * fs) / 256;
        wishx = FIXMUL(mx, speed);
        wishy = FIXMUL(my, speed);
    } else if (c->move || c->strafe) {
        fix16 mx = fwdx * c->move - fwdy * c->strafe;
        fix16 my = fwdy * c->move + fwdx * c->strafe;
        if (c->move && c->strafe) { mx = mx * 181 / 256; my = my * 181 / 256; }
        wishx = FIXMUL(mx, speed);
        wishy = FIXMUL(my, speed);
    }
    if (doomo_player.roll_timer) {
        wishx = FIXMUL(combat_px.roll_dx, ROLL_SPEED);
        wishy = FIXMUL(combat_px.roll_dy, ROLL_SPEED);
        combat_px.vx = wishx;
        combat_px.vy = wishy;
    } else {
        accel = doomo_player.on_ground ? INT2FIX(1) + INT2FIX(1) * 6 / 10 : INT2FIX(1) * 45 / 100;
        if (combat_feel() && doomo_player.on_ground) accel = accel * 3 / 2;     /* snappy starts */
        if (moving) {
            fix16 ddx = wishx - combat_px.vx, ddy = wishy - combat_px.vy;
            fix16 dl = fx_dist(ddx, ddy);
            if (dl > accel) { ddx = FIXMUL(FIXDIV(ddx, dl), accel); ddy = FIXMUL(FIXDIV(ddy, dl), accel); }
            combat_px.vx += ddx;
            combat_px.vy += ddy;
        } else if (doomo_player.on_ground) {
            int fr = combat_feel() ? 7 : 9;                 /* ...and quick stops */
            combat_px.vx = combat_px.vx * fr / 16;
            combat_px.vy = combat_px.vy * fr / 16;
        } else {
            combat_px.vx = combat_px.vx * 31 / 32;
            combat_px.vy = combat_px.vy * 31 / 32;
        }
    }
    if (combat_px.vx > -FIX_ONE / 16 && combat_px.vx < FIX_ONE / 16) combat_px.vx = 0;
    if (combat_px.vy > -FIX_ONE / 16 && combat_px.vy < FIX_ONE / 16) combat_px.vy = 0;
    /* move: sub-steps so fast rolls never tunnel through thin walls */
    {
        int steps = (fx_dist(combat_px.vx, combat_px.vy) > INT2FIX(10)) ? 2 : 1;
        for (i = 0; i < steps; i++) {
            fix16 sx = combat_px.vx / steps, sy = combat_px.vy / steps;
            if (!try_move(sx, sy)) {
                if (sx && try_move(sx, 0)) { combat_px.vy = combat_px.vy / 2; }
                else if (sy && try_move(0, sy)) { combat_px.vx = combat_px.vx / 2; }
                else { combat_px.vx = 0; combat_px.vy = 0; }
            }
        }
    }
    /* vertical */
    h = player_height();
    z = FIX2INT(doomo_player.z);
    cw_fits(doomo_player.x, doomo_player.y, PLAYER_RADIUS, 30000, 0, 30000, &fl, &ce);
    if (c->jump && doomo_player.on_ground && !doomo_player.crouch) {
        doomo_player.vz = JUMP_VZ;
        doomo_player.on_ground = 0;
        audio_sfx(SFX_BLIP);
    }
    if (!doomo_player.on_ground || z > fl) {
        fix16 g = opt_get(OPT_CHEAT_LOW_GRAV) ? GRAVITY * 2 / 5 : GRAVITY;
        doomo_player.vz -= g;
        doomo_player.z += doomo_player.vz;
        doomo_player.on_ground = 0;
        if (ce < SKY_CEIL && FIX2INT(doomo_player.z) + h > ce) {
            doomo_player.z = INT2FIX(ce - h);
            if (doomo_player.vz > 0) doomo_player.vz = 0;
        }
        if (doomo_player.z <= INT2FIX(fl)) {
            if (doomo_player.vz < -INT2FIX(6)) {
                combat_px.land_t = 8;
                combat_px.view_h -= INT2FIX(DOOMO_MIN(-FIX2INT(doomo_player.vz), 12));
                audio_sfx(SFX_CURSOR);
            }
            doomo_player.z = INT2FIX(fl);
            doomo_player.vz = 0;
            doomo_player.on_ground = 1;
        }
    } else if (z < fl) {
        /* stepped up (or a lift raised us): keep the eye smooth */
        combat_px.view_h -= INT2FIX(fl - z);
        doomo_player.z = INT2FIX(fl);
        doomo_player.on_ground = 1;
    } else {
        doomo_player.on_ground = 1;
        doomo_player.vz = 0;
    }
    /* eye height smoothing */
    eye = doomo_player.crouch ? CROUCH_EYE : STAND_EYE;
    if (doomo_player.roll_timer) eye = 24;
    if (combat_px.view_h < INT2FIX(eye)) {
        combat_px.view_h += (INT2FIX(eye) - combat_px.view_h) / 3 + FIX_ONE / 2;
        if (combat_px.view_h > INT2FIX(eye)) combat_px.view_h = INT2FIX(eye);
    } else if (combat_px.view_h > INT2FIX(eye)) {
        combat_px.view_h -= (combat_px.view_h - INT2FIX(eye)) / 3 + FIX_ONE / 2;
        if (combat_px.view_h < INT2FIX(eye)) combat_px.view_h = INT2FIX(eye);
    }
    if (combat_px.view_h < INT2FIX(8)) combat_px.view_h = INT2FIX(8);
    /* head bob + weapon sway */
    look_level = opt_get(OPT_HEAD_BOB);
    {
        int sp = FIX2INT(fx_dist(combat_px.vx, combat_px.vy) * 16);
        if (doomo_player.on_ground && sp > 8) {
            combat_px.bob_phase += 90 + sp * 2;
            combat_px.bob_amp = DOOMO_MIN(combat_px.bob_amp + 2, look_level == 0 ? 0 : look_level == 1 ? 3 : 6);
            combat_px.anim += sp / 4 + 4;
        } else {
            if (combat_px.bob_amp > 0) combat_px.bob_amp--;
            combat_px.bob_phase += 40;
        }
        combat_px.sway_x = (combat_px.sway_x * 3 + (c->turn * -3)) / 4;
        combat_px.sway_x = DOOMO_CLAMP(combat_px.sway_x, -160, 160);
    }
    if (combat_px.kick > 0) combat_px.kick = combat_px.kick * 2 / 3;
    if (combat_px.land_t) combat_px.land_t--;
    cell_specials();
    pickups_tick();
}

/* ---- tick ------------------------------------------------------------------------------- */
static void session_timers(void)
{
    int i;
    if (doomo_player.hurt_flash) doomo_player.hurt_flash--;
    if (doomo_player.pickup_flash) doomo_player.pickup_flash--;
    if (combat_px.flash) combat_px.flash--;
    if (combat_px.shake) combat_px.shake = combat_px.shake * 3 / 4;
    if (combat_px.hit_marker) combat_px.hit_marker--;
    if (combat_px.hit_kill) combat_px.hit_kill--;
    if (combat_px.hit_head) combat_px.hit_head--;
    if (combat_px.dmg_dir_t) combat_px.dmg_dir_t--;
    if (combat_px.msg_t) combat_px.msg_t--;
    if (combat_px.combo_t) combat_px.combo_t--;
    if (combat_px.face_t) combat_px.face_t--;
    if (msg_cool) msg_cool--;
    for (i = 0; i < DMGNUM_MAX; i++)
        if (combat_dmgnums[i].t) {
            combat_dmgnums[i].t++;
            combat_dmgnums[i].z += INT2FIX(1);
            if (combat_dmgnums[i].t > 28) combat_dmgnums[i].t = 0;
        }
    /* low health heartbeat */
    if (doomo_player.hp > 0 && doomo_player.hp <= 30) {
        combat_px.heartbeat++;
        if ((combat_px.heartbeat % (doomo_player.hp <= 15 ? 18 : 26)) == 0) {
            int vol = opt_get(OPT_SFX_VOL);
            plat_sample(SFX_SPLAT, (uint8_t)(vol > 4 ? vol / 2 : vol));
        }
    } else {
        combat_px.heartbeat = 0;
    }
    /* hell-ness of the HUD follows the floor under your feet */
    {
        const DoomoCell *c = map_cell_safe(&doomo_map, FIX2INT(doomo_player.x) / CELLU,
                                           FIX2INT(doomo_player.y) / CELLU);
        int hell = c->floor_tex >= TEX_FLESH_WALL && c->floor_tex != TEX_DOOR_CUTE ? 256 : 0;
        if (c->ceil_tex == DOOMO_TEX_SKY && (doomo_map.sky == SKY_HELL || doomo_map.sky == SKY_VAPE_STORM))
            hell = 256;
        if (combat_st.ticks <= 1) combat_s.hell = hell;
        else if (combat_s.hell < hell) combat_s.hell += 8;
        else if (combat_s.hell > hell) combat_s.hell -= 8;
    }
}

void combat_tick(void)
{
    CombatCtl *c = &combat_ctl;
    if (!combat_s.active || combat_s.finished) return;
    if (combat_bot_fn) combat_bot_fn(c);
    else read_controls(c);
    if (combat_px.hitstop > 0) {
        combat_px.hitstop--;
        session_timers();
        return;
    }
    combat_st.ticks++;
    player_tick(c);
    if (doomo_player.hp > 0) weapon_tick(c);
    map_tick(&doomo_map);
    actors_tick();
    session_timers();
    /* the boss bar follows the current boss */
    if (combat_s.boss >= 0 && !(doomo_actors[combat_s.boss].flags & AF_BOSS)) combat_s.boss = -1;
    if (combat_s.boss < 0) {
        int i;
        for (i = 0; i < ACTOR_MAX; i++)
            if ((doomo_actors[i].flags & AF_BOSS) && doomo_actors[i].hp > 0 &&
                doomo_actors[i].state != AST_DORMANT && doomo_actors[i].state != AST_IDLE) {
                combat_s.boss = i;
                combat_s.boss_max = doomo_actors[i].type == THING_LORD_VAPULA ? DEMON(THING_LORD_VAPULA)->hp
                                                                              : DEMON(doomo_actors[i].type)->hp * 4;
                break;
            }
    }
    /* goals */
    if (combat_s.exit_t) {
        if (++combat_s.exit_t > 20 && !combat_s.finished) {
            combat_s.finished = 1;
            last_result = 1;
        }
    }
    if (combat_s.goal == CGOAL_KILL_ALL && !combat_s.finished && combat_st.ticks > 30 &&
        actor_demons_alive() == 0) {
        combat_s.finished = 1;
        last_result = 1;
    }
    if (combat_s.finished == 1) {
        doomo_town.story.kills = (uint16_t)DOOMO_MIN(doomo_town.story.kills + combat_st.kills, 60000);
        doomo_town.money += combat_st.money + (uint32_t)combat_st.secrets * 50 + 100;
        progress_add(PROG_MONEY_EARNED, combat_st.money + combat_st.secrets * 50 + 100);
        combat_st.money = (uint16_t)DOOMO_MIN(combat_st.money + combat_st.secrets * 50 + 100, 60000);
        if (combat_s.map_id == MAPID_VAPE_CELLAR) unlock_grant(530);
        if (combat_st.kills > 0 && combat_px.cute_only) unlock_grant(561);
        if (combat_st.secrets_total > 0 && combat_st.secrets >= combat_st.secrets_total) unlock_grant(563);
        inv_add(ITEM_SCRAP, 2 + combat_st.kills / 6);
        if (combat_st.kills >= 10) inv_add(ITEM_GEARS, 1);
        if (combat_s.map_id == MAPID_CLOUD_NINE_SPIRE) inv_add(ITEM_HELLSTEEL, 2);
    }
}

/* ---- third-person camera ----------------------------------------------------------------- */
static void update_camera(void)
{
    fix16 hx, hy, hz, px = doomo_player.x, py = doomo_player.y;
    fix16 headz = doomo_player.z + INT2FIX(DOOMO_MIN(FIX2INT(combat_px.view_h) + 6, 50));
    int ang = doomo_player.angle, side = doomo_player.shoulder ? -1 : 1;
    int dist = doomo_player.sprint ? 136 : 124;
    fix16 bx = -fx_cos(ang), by = -fx_sin(ang);           /* behind */
    fix16 rx = -fx_sin(ang) * side, ry = fx_cos(ang) * side;   /* right shoulder */
    fix16 dx, dy, d, want_x, want_y, want_z, len;
    if (combat_px.zoom_t > 128) dist = 56;
    if (input_feel()) {
        /* living camera: it trails back while you run forward, drops in
         * tight when you aim, swings out on a dodge-roll and gets jolted
         * by recoil - and eases between them instead of snapping */
        fix16 fv = FIXMUL(combat_px.vx, fx_cos(ang)) + FIXMUL(combat_px.vy, fx_sin(ang));
        int fvi = FIX2INT(fv);
        if (combat_px.zoom_t <= 128) {
            dist += DOOMO_CLAMP(fvi, -4, 14) * 2;
            if (doomo_player.roll_timer) dist += 24;
            dist -= DOOMO_MIN(combat_px.kick / 40, 8);
        }
        if (!combat_px.cam_ok) combat_px.cam_dist = dist;
        else {
            int gap = dist - combat_px.cam_dist;
            combat_px.cam_dist += gap / 4 + (gap > 0 ? 1 : gap < 0 ? -1 : 0);
        }
        dist = combat_px.cam_dist;
    }
    /* desired point: behind + over the shoulder + a little up */
    want_x = px + FIXMUL(bx, INT2FIX(dist)) + FIXMUL(rx, INT2FIX(22));
    want_y = py + FIXMUL(by, INT2FIX(dist)) + FIXMUL(ry, INT2FIX(22));
    want_z = headz + INT2FIX(12) - FIXMUL(pitch_slope(doomo_player.pitch), INT2FIX(dist)) / 2;
    dx = want_x - px; dy = want_y - py;
    len = fx_dist(dx, dy);
    if (len < INT2FIX(1)) len = INT2FIX(1);
    d = cw_trace(px, py, headz, FIXDIV(dx, len), FIXDIV(dy, len), FIXDIV(want_z - headz, len), len,
                 &hx, &hy, &hz);
    if (d < len) {
        /* wall collision: pull the camera in front of the wall */
        fix16 keep = d - INT2FIX(10);
        if (keep < INT2FIX(6)) keep = INT2FIX(6);
        want_x = px + FIXMUL(FIXDIV(dx, len), keep);
        want_y = py + FIXMUL(FIXDIV(dy, len), keep);
        want_z = headz + FIXMUL(FIXDIV(want_z - headz, len), keep);
    }
    if (!combat_px.cam_ok) {
        combat_px.cam_x = want_x; combat_px.cam_y = want_y; combat_px.cam_z = want_z;
        combat_px.cam_ok = 1;
    } else {
        fix16 nx = combat_px.cam_x + (want_x - combat_px.cam_x) / 2;
        fix16 ny = combat_px.cam_y + (want_y - combat_px.cam_y) / 2;
        fix16 nz = combat_px.cam_z + (want_z - combat_px.cam_z) / 2;
        /* never smooth through a wall */
        if (d < len || !cw_los(px, py, FIX2INT(headz), nx, ny, FIX2INT(nz))) {
            nx = want_x; ny = want_y; nz = want_z;
        }
        combat_px.cam_x = nx; combat_px.cam_y = ny; combat_px.cam_z = nz;
    }
    /* keep the camera inside the cell's floor/ceiling */
    {
        int cx = FIX2INT(combat_px.cam_x) / CELLU, cy = FIX2INT(combat_px.cam_y) / CELLU;
        int fl = cw_floor(cx, cy) + 6, ce = cw_ceil(cx, cy) - 6;
        if (FIX2INT(combat_px.cam_z) < fl) combat_px.cam_z = INT2FIX(fl);
        if (ce < SKY_CEIL && FIX2INT(combat_px.cam_z) > ce) combat_px.cam_z = INT2FIX(ce);
    }
}

/* ---- the 3D view --------------------------------------------------------------------------- */
static int player_view_dir(fix16 camx, fix16 camy, int *flip)
{
    /* which way the player's body faces relative to the camera:
     * 0 back, 1 back-left, 2 left, 3 front-left, 4 front (+ mirrored) */
    static const uint8_t views[8] = { 4, 3, 2, 1, 0, 1, 2, 3 };
    static const uint8_t flips[8] = { 0, 1, 1, 1, 0, 0, 0, 0 };
    int to_cam = fx_atan2(camy - doomo_player.y, camx - doomo_player.x);
    int rel = (to_cam - doomo_player.angle) & DOOMO_ANG_MASK;
    int sector = ((rel + 256) >> 9) & 7;
    *flip = flips[sector];
    return views[sector];
}

void combat_build_view(DoomoView3D *v)
{
    int i, n = 0, shake = combat_px.shake;
    fix16 ex, ey, ez;
    int sw = plat_screen_w(), sh = plat_screen_h();
    memset(v, 0, sizeof(*v));
    v->map = &doomo_map;
    v->vx = 0; v->vy = 0;
    v->vw = (int16_t)sw; v->vh = (int16_t)sh;
    v->time = doomo_map.time;
    v->camera = doomo_player.camera;
    v->player_sprite = -1;
    v->angle = doomo_player.angle;
    v->pitch = (int16_t)DOOMO_CLAMP(doomo_player.pitch + doomo_player.recoil, -256, 256);
    if (shake > 0) {
        v->angle = (int16_t)(v->angle + rng_between(&doomo_rng, -shake, shake));
        v->pitch = (int16_t)DOOMO_CLAMP(v->pitch + rng_between(&doomo_rng, -shake, shake), -256, 256);
    }
    v->angle &= DOOMO_ANG_MASK;
    if (doomo_player.camera == CAM_THIRD_PERSON) {
        update_camera();
        ex = combat_px.cam_x; ey = combat_px.cam_y; ez = combat_px.cam_z;
        v->pitch = (int16_t)DOOMO_CLAMP(v->pitch - TP_TILT, -256, 256);
    } else {
        combat_px.cam_ok = 0;
        ex = doomo_player.x; ey = doomo_player.y;
        ez = doomo_player.z + combat_px.view_h;
        v->bob = (int16_t)(FIX2INT(fx_sin(combat_px.bob_phase) * combat_px.bob_amp) / 2);
        /* ADS: a scope "zooms" by dollying the eye toward what is aimed at
         * (the vignette hides the edges) */
        if (combat_px.zoom_t > 0 && combat_ws.zoom) {
            static const int16_t zf[4] = { 0, 64, 128, 192 };     /* 1 - 1/zoom, x256 */
            fix16 dx = fx_cos(v->angle), dy = fx_sin(v->angle), sl = pitch_slope(v->pitch);
            fix16 d = cw_trace(ex, ey, ez, dx, dy, sl, INT2FIX(40 * CELLU), NULL, NULL, NULL);
            fix16 mv = (fix16)(((int64_t)(d - INT2FIX(16)) * zf[combat_ws.zoom] * combat_px.zoom_t) >> 16);
            if (mv > 0) {
                ex += FIXMUL(dx, mv); ey += FIXMUL(dy, mv); ez += FIXMUL(sl, mv);
            }
        }
    }
    v->x = ex; v->y = ey; v->z = ez;
    /* billboards: nearest first, capped */
    {
        static int16_t order[ACTOR_MAX];
        static int32_t dist[ACTOR_MAX];
        int cnt = 0, k;
        fix16 fx = fx_cos(v->angle), fy = fx_sin(v->angle);
        for (i = 0; i < ACTOR_MAX; i++) {
            const DoomoActor *a = &doomo_actors[i];
            fix16 rx, ry, depth, dd;
            if (!(a->flags & AF_USED)) continue;
            rx = a->x - ex; ry = a->y - ey;
            depth = FIXMUL(rx, fx) + FIXMUL(ry, fy);
            if (depth < -INT2FIX(48)) continue;
            dd = fx_dist(rx, ry);
            if (dd > INT2FIX(30 * CELLU)) continue;
            for (k = cnt; k > 0 && dist[k - 1] > dd; k--) { dist[k] = dist[k - 1]; order[k] = order[k - 1]; }
            dist[k] = dd;
            order[k] = (int16_t)i;
            cnt++;
        }
        for (k = 0; k < cnt && n < BB_MAX - 2; k++) {
            const DoomoActor *a = &doomo_actors[order[k]];
            int flags, scale, zoff;
            const DoomoImage *img = actor_image(a, &flags, &scale, &zoff);
            if (!img) continue;
            bbs[n].x = a->x;
            bbs[n].y = a->y;
            bbs[n].z = a->z + INT2FIX(zoff);
            bbs[n].img = img;
            bbs[n].scale = (uint16_t)scale;
            bbs[n].flags = (uint8_t)flags;
            bbs[n].light = 0;
            if (combat_px.flash && dist[k] < INT2FIX(3 * CELLU)) bbs[n].light = 4;
            if (combat_px.zoom_t > 128 && combat_weapon()->sight == WSIGHT_THERMAL && IS_DEMON(a->type))
                bbs[n].flags |= BB_FULLBRIGHT;
            n++;
        }
    }
    /* the player's own body in third person */
    if (doomo_player.camera == CAM_THIRD_PERSON) {
        int flip, view = player_view_dir(ex, ey, &flip), anim;
        if (doomo_player.hp <= 0) anim = 6;
        else if (doomo_player.roll_timer) anim = 5;
        else if (doomo_player.hurt_flash > 14) anim = 6;
        else if (combat_px.flash || combat_px.kick > 64) anim = 4;
        else if (fx_dist(combat_px.vx, combat_px.vy) > INT2FIX(1)) anim = (combat_px.anim / 48) & 3;
        else anim = 0;
        bbs[n].x = doomo_player.x;
        bbs[n].y = doomo_player.y;
        bbs[n].z = doomo_player.z + (doomo_player.roll_timer ? 0 : 0);
        bbs[n].img = fighter_image(doomo_player.resident_id, view, anim, combat_weapon()->frame);
        bbs[n].scale = (uint16_t)(doomo_player.crouch ? 240 : 300);
        bbs[n].flags = (uint8_t)(BB_SHADOW | (flip ? BB_FLIPX : 0));
        bbs[n].light = (uint8_t)(combat_px.flash ? 3 : 0);
        if (combat_px.invuln && (combat_st.ticks & 1)) bbs[n].flags |= BB_TRANSLUCENT;
        v->player_sprite = (int8_t)n;
        n++;
    }
    v->sprites = bbs;
    v->sprite_count = n;
    /* tints: hurt, pickup, low health pulse, vape haze, disco */
    {
        int reduce = opt_get(OPT_REDUCE_FLASH);
        int lvl = 0, tint = TINT_NONE;
        const DoomoCell *c = map_cell_safe(&doomo_map, FIX2INT(doomo_player.x) / CELLU, FIX2INT(doomo_player.y) / CELLU);
        if (c->flags & CELL_PORTAL) { tint = TINT_PURPLE; lvl = 4; }
        if (doomo_player.hp > 0 && doomo_player.hp <= 30) {
            int beat = combat_px.heartbeat % (doomo_player.hp <= 15 ? 18 : 26);
            int pulse = beat < 4 ? 6 - beat : beat < 8 ? 3 : 1;
            if (pulse > lvl) { tint = TINT_RED; lvl = pulse; }
        }
        if (doomo_player.pickup_flash && doomo_player.pickup_flash * 2 > lvl) {
            tint = TINT_GOLD; lvl = DOOMO_MIN(doomo_player.pickup_flash, 6);
        }
        if (doomo_player.hurt_flash) {
            tint = TINT_RED; lvl = DOOMO_MIN(4 + doomo_player.hurt_flash / 2, 12);
        }
        if (doomo_player.hp <= 0) { tint = TINT_RED; lvl = DOOMO_MIN(6 + combat_px.dead_t / 4, 13); }
        if (combat_px.flash && combat_ws.behaviour != WB_MELEE && lvl < 2) { tint = TINT_GOLD; lvl = 1; }
        if (opt_get(OPT_CHEAT_DISCO) && lvl == 0) {
            static const uint8_t disco[4] = { TINT_PURPLE, TINT_GOLD, TINT_GREEN, TINT_RED };
            tint = disco[(doomo_map.time / 8) & 3];
            lvl = 3;
        }
        if (reduce) lvl /= 2;
        v->tint = (uint8_t)tint;
        v->tint_level = (uint8_t)DOOMO_CLAMP(lvl, 0, 15);
    }
    combat_hud_set_view(v);
}
