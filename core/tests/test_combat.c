/* test_combat.c - the shooter (owner: action): the debug autopilot plays
 * the Vape Cellar to the exit, every weapon frame kills a demon, demons
 * gib per the gore option, infight and get charmed, the player can die,
 * doors / lifts / secrets / pickups / keys work in a live session, Lord
 * Vapula's death opens the summit exit, the third-person camera never sits
 * in a wall, and the COMBAT / WORKSHOP / WELD_GAME / ARENA scenes run
 * through their menus. Screens land in the test tmp dir. */
#include "test.h"
#include "../src/combat_int.h"
#include "../src/weld_int.h"

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
static void tick(uint32_t b) { input_update(b); combat_tick(); }

/* Render the current combat frame (view + HUD); returns how many view
 * pixels the renderer left unpainted (the frame is cleared to red first). */
static int render_combat(const char *name)
{
    DoomoView3D v;
    char path[512];
    int i, holes = 0;
    combat_build_view(&v);
    plat_clear(PAL_UI_RED);
    plat_view3d(&v);
    if (!v.tint_level)
        for (i = 0; i < nat_w * nat_h; i++) holes += nat_fb[i] == PAL_UI_RED;
    combat_draw_hud();
    if (name) {
        snprintf(path, sizeof(path), "%s/combat_%s.png", test_tmp_dir, name);
        nat_png_write(path, nat_fb, nat_w, nat_h, 2);
    }
    return holes;
}

static void shot(const char *name)
{
    char path[512];
    plat_begin_frame();
    scene_draw();
    plat_end_frame();
    snprintf(path, sizeof(path), "%s/scene_%s.png", test_tmp_dir, name);
    nat_png_write(path, nat_fb, nat_w, nat_h, 2);
}

static int count_gibs(int spr)
{
    int i, n = 0;
    for (i = 0; i < ACTOR_MAX; i++)
        if ((doomo_actors[i].flags & AF_USED) && doomo_actors[i].type == AK_GIB && doomo_actors[i].spr == spr) n++;
    return n;
}

static int find_actor(int type)
{
    int i;
    for (i = 0; i < ACTOR_MAX; i++)
        if ((doomo_actors[i].flags & AF_USED) && doomo_actors[i].type == type && doomo_actors[i].hp > 0) return i;
    return -1;
}

static int cell_of(fix16 v) { return FIX2INT(v) / DOOMO_CELL_UNITS; }

/* first cell of the current map with all of `flags` (and tag if >= 0) */
static int find_cell(int flags, int tag, int *cx, int *cy)
{
    int i;
    for (i = 0; i < doomo_map.w * doomo_map.h; i++) {
        const DoomoCell *c = &doomo_map.cells[i];
        if ((c->flags & flags) == flags && !c->wall && (tag < 0 || c->tag == tag)) {
            *cx = i % doomo_map.w;
            *cy = i / doomo_map.w;
            return 1;
        }
    }
    return 0;
}

/* ---- the autopilot plays the tutorial dungeon -------------------------- */
TEST(combat_bot_clears_vape_cellar)
{
    const DoomoCombatStats *s;
    uint32_t killed0 = progress_get(PROG_DEMONS_KILLED);
    int t, hell_seen = 0;
    opt_set(OPT_DIFFICULTY, DIFF_PLUSHIE);
    rng_seed(&doomo_rng, 7);
    combat_start(MAPID_VAPE_CELLAR);
    s = combat_stats();
    CHECK_EQ(combat_finished(), 0);
    CHECK(s->kills_total >= 8);
    CHECK(s->secrets_total >= 1);
    CHECK(combat_weapon() != NULL);
    CHECK(combat_hud_style() < 64);                   /* starts cute */
    combat_autopilot(1);
    CHECK(combat_autopilot_on());
    for (t = 0; t < 30 * 60 * 15 && !combat_finished(); t++) {
        tick(0);
        if (combat_hud_style() > 192) hell_seen = 1;
        if (t % 2400 == 1200) render_combat(t == 3600 ? "bot_cellar" : NULL);
    }
    render_combat("bot_cellar_end");
    combat_autopilot(0);
    CHECK_MSG(combat_finished() == 1, "autopilot stuck: t %d finished %d hp %d cell %d,%d kills %d/%d",
              t, combat_finished(), doomo_player.hp, cell_of(doomo_player.x), cell_of(doomo_player.y),
              s->kills, s->kills_total);
    CHECK_EQ(combat_last_result(), 1);
    CHECK(doomo_player.hp > 0);
    CHECK_MSG(s->kills == s->kills_total, "kills %d/%d", s->kills, s->kills_total);
    CHECK(s->shots > 0 && s->hits > 0 && s->hits <= s->shots * 12);
    CHECK(s->score > 0 && s->ticks > 0);
    CHECK(hell_seen);                                  /* the cellar turns hellish */
    CHECK(progress_get(PROG_DEMONS_KILLED) >= killed0 + s->kills);
    CHECK(unlock_has(530));                            /* the arena opens */
    CHECK(unlock_has(540));                            /* impling in the gallery */
}

/* ---- the player can die ------------------------------------------------ */
TEST(combat_player_can_die)
{
    int t, k;
    opt_set(OPT_DIFFICULTY, DIFF_NORMAL);
    combat_start_at(MAPID_ARENA, CGOAL_ENDLESS, 16, 22, 3072);
    for (k = 0; k < 4; k++) CHECK(combat_spawn(k & 1 ? THING_HELLHOUND_PUP : THING_IMPLING, 14 + k, 19, 0) >= 0);
    CHECK_EQ(combat_demons_alive(), 4);
    for (t = 0; t < 30 * 90 && !combat_finished(); t++) tick(0);
    CHECK_MSG(combat_finished() == -1, "still alive after %d ticks: hp %d", t, doomo_player.hp);
    CHECK_EQ(combat_last_result(), -1);
    CHECK(doomo_player.hp <= 0);
    CHECK(combat_stats()->damage_taken >= 100);
    for (k = 0; k < 40; k++) tick(0);                  /* the death cam keeps running */
    render_combat("dead");
}

/* ---- every weapon frame kills an impling -------------------------------- */
static int fight_with(int frame, int style, char *why, int cap)
{
    DoomoWeapon w;
    DoomoWeaponStats st;
    int t, d, slot;
    weld_default(&w, frame, style);
    memset(doomo_town.weapons, 0, sizeof(doomo_town.weapons));
    doomo_town.weapons[0] = w;
    doomo_town.story.equipped = 0;
    combat_start_at(MAPID_ARENA, CGOAL_ENDLESS, 8, 22, 0);
    combat_select_weapon(0);
    combat_refill_ammo(100);
    slot = doomo_player.weapon_slot;
    weld_stats(combat_weapon(), &st);
    d = combat_spawn(THING_IMPLING, 12, 22, 0);
    if (d < 0) { snprintf(why, (size_t)cap, "spawn failed"); return 0; }
    for (t = 0; t < 1800; t++) {
        DoomoActor *a = &doomo_actors[d];
        uint32_t b = 0;
        int dist;
        if (a->hp <= 0) break;
        doomo_player.angle = (int16_t)fx_atan2(a->y - doomo_player.y, a->x - doomo_player.x);
        dist = FIX2INT(fx_dist(a->x - doomo_player.x, a->y - doomo_player.y));
        if (st.behaviour == WB_MELEE && dist > 72) b |= BTN_UP;
        if (st.behaviour == WB_LOB && dist < 150) b |= BTN_DOWN;          /* lobbers like room */
        if (st.charge) { if (t % (st.charge + 8) < st.charge + 4) b |= BTN_A; }
        else if (st.auto_fire || st.spin_up) b |= BTN_A;
        else if (t % 6 < 3) b |= BTN_A;
        tick(b);
        if (doomo_player.weapon_slot != slot) { snprintf(why, (size_t)cap, "switched weapon"); return 0; }
    }
    if (doomo_actors[d].hp > 0) {
        snprintf(why, (size_t)cap, "impling survived %d ticks with %d hp (shots %d hits %d)", t,
                 doomo_actors[d].hp, combat_stats()->shots, combat_stats()->hits);
        return 0;
    }
    if (combat_stats()->kills != 1) { snprintf(why, (size_t)cap, "kill not counted"); return 0; }
    return 1;
}

TEST(combat_every_weapon_frame_kills)
{
    char why[160];
    int f;
    opt_set(OPT_DIFFICULTY, DIFF_NORMAL);
    opt_set(OPT_CHEAT_GOD_MODE, 1);
    for (f = 0; f < WFRAME_COUNT; f++) {
        CHECK_MSG(fight_with(f, f & 1, why, sizeof(why)), "%s: %s", doomo_wframe_names[f], why);
        if (f == WFRAME_SUPER_SHOTGUN) CHECK(render_combat("fight_shotgun") < nat_w);
        if (f == WFRAME_BUBBLE_WAND) CHECK(render_combat("fight_bubbles") < nat_w);
    }
    opt_set(OPT_CHEAT_GOD_MODE, 0);
}

/* ---- gore follows the option ---------------------------------------------- */
TEST(combat_gore_cute_and_brutal)
{
    int style, d, t;
    for (style = 0; style < 2; style++) {
        opt_set(OPT_GORE, style ? GORE_BRUTAL : GORE_CUTE);
        combat_start_at(MAPID_ARENA, CGOAL_ENDLESS, 16, 22, 3072);
        d = combat_spawn(THING_IMPLING, 16, 18, 0);
        CHECK(d >= 0);
        CHECK_EQ(actor_damage(d, 500, PLAYER_OWNER, 3072, 0, 0, 0), 1);
        if (style == 0) {
            CHECK(count_gibs(SPR_STUFFING) + count_gibs(SPR_CONFETTI) > 0);
            CHECK_EQ(count_gibs(SPR_GIBS), 0);
            CHECK_EQ(doomo_actors[d].spr, SPR_CORPSE_CUTE);     /* overkill = plush corpse */
        } else {
            CHECK(count_gibs(SPR_GIBS) > 0);
            CHECK_EQ(count_gibs(SPR_CONFETTI) + count_gibs(SPR_STUFFING), 0);
            CHECK_EQ(doomo_actors[d].spr, SPR_CORPSE_BRUTAL);
        }
        for (t = 0; t < 10; t++) tick(0);
        render_combat(style ? "gore_brutal" : "gore_cute");
        for (t = 0; t < 200; t++) tick(0);
        CHECK_EQ(combat_demons_alive(), 0);
        CHECK_EQ(combat_stats()->kills, 1);
    }
    /* a plain kill is not an overkill */
    opt_set(OPT_GORE, GORE_CUTE);
    combat_start_at(MAPID_ARENA, CGOAL_ENDLESS, 16, 22, 3072);
    d = combat_spawn(THING_IMPLING, 16, 18, 0);
    CHECK_EQ(actor_damage(d, doomo_actors[d].hp, PLAYER_OWNER, 3072, 0, 0, 0), 1);
    CHECK(doomo_actors[d].spr != SPR_CORPSE_CUTE);
}

/* ---- demons fight each other and can be charmed --------------------------- */
TEST(combat_infighting_and_charm)
{
    int a, b, k, charmed = 0;
    combat_start_at(MAPID_ARENA, CGOAL_ENDLESS, 16, 22, 3072);
    a = combat_spawn(THING_IMPLING, 10, 10, 0);
    b = combat_spawn(THING_BONE_JESTER, 22, 10, 0);
    CHECK(a >= 0 && b >= 0);
    actor_damage(a, 5, b, 0, 0, 0, 0);
    CHECK_EQ(doomo_actors[a].target, b);               /* goes after the jester */
    /* same species do not hold grudges */
    k = combat_spawn(THING_IMPLING, 12, 10, 0);
    doomo_actors[k].target = TARGET_PLAYER;
    actor_damage(k, 5, a, 0, 0, 0, 0);
    CHECK(doomo_actors[k].target != a);
    for (k = 0; k < 40 && !charmed; k++) {
        doomo_actors[b].hp = 500;
        actor_damage(b, 1, PLAYER_OWNER, 0, 0, WFX_CHARM, 0);
        charmed = (doomo_actors[b].status_fx & ST_CHARMED) != 0;
    }
    CHECK(charmed);
    /* a charmed jester turns on the other demons */
    for (k = 0; k < 300 && doomo_actors[a].hp > 0; k++) tick(0);
    CHECK(doomo_actors[a].hp < 60 || doomo_actors[b].target == a);
}

/* ---- doors, lifts, secrets, pickups and keys in a live session ---------- */
TEST(combat_doors_lifts_secrets_pickups)
{
    int cx, cy, t, z0, hp;
    opt_set(OPT_DIFFICULTY, DIFF_PLUSHIE);
    /* a plain door opens when walked into */
    combat_start(MAPID_VAPE_CELLAR);
    CHECK(find_cell(CELL_DOOR, 0, &cx, &cy));
    {
        static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        static const int ang[4] = { 2048, 0, 3072, 1024 };   /* face back to the door */
        int k, sx = -1, sy = -1, sa = 0;
        for (k = 0; k < 4 && sx < 0; k++) {
            const DoomoCell *c = &doomo_map.cells[(cy + dy[k]) * doomo_map.w + cx + dx[k]];
            if (!c->wall && !(c->flags & CELL_DOOR)) { sx = cx + dx[k]; sy = cy + dy[k]; sa = ang[k]; }
        }
        CHECK(sx >= 0);
        combat_start_at(MAPID_VAPE_CELLAR, CGOAL_EXIT, sx, sy, sa);
        CHECK_EQ(map_door_open(&doomo_map, cx, cy), 0);
        for (t = 0; t < 90; t++) tick(BTN_UP);
        CHECK(map_door_open(&doomo_map, cx, cy) > 0);
        CHECK(cell_of(doomo_player.x) != sx || cell_of(doomo_player.y) != sy);    /* walked through */
    }
    /* a lift carries the player */
    combat_start(MAPID_VAPE_CELLAR);
    CHECK(find_cell(CELL_LIFT, -1, &cx, &cy));
    combat_start_at(MAPID_VAPE_CELLAR, CGOAL_EXIT, cx, cy, 0);
    z0 = FIX2INT(doomo_player.z);
    for (t = 0; t < 120; t++) tick(0);
    CHECK_MSG(FIX2INT(doomo_player.z) != z0, "lift at %d,%d did not move (z %d)", cx, cy, z0);
    /* a secret is counted once */
    combat_start(MAPID_VAPE_CELLAR);
    CHECK(find_cell(CELL_SECRET, -1, &cx, &cy));
    combat_start_at(MAPID_VAPE_CELLAR, CGOAL_EXIT, cx, cy, 0);
    for (t = 0; t < 10; t++) tick(0);
    CHECK_EQ(combat_stats()->secrets, 1);
    /* pickups */
    combat_start_at(MAPID_ARENA, CGOAL_ENDLESS, 16, 22, 3072);
    doomo_player.hp = 40;
    hp = doomo_player.hp;
    CHECK(combat_spawn(THING_HEALTH_BIG, 16, 22, 0) >= 0);
    CHECK(combat_spawn(THING_KEY_RED, 16, 21, 0) >= 0);
    CHECK(combat_spawn(THING_ARMOR, 16, 20, 0) >= 0);
    tick(0); tick(0);
    CHECK(doomo_player.hp > hp);
    for (t = 0; t < 60; t++) tick(BTN_UP);
    CHECK(doomo_player.keys & 1);
    CHECK(doomo_player.armor > 0);
    CHECK(combat_stats()->items >= 2);
    /* a red door needs the red key */
    combat_start(MAPID_VAPE_CELLAR);
    if (find_cell(CELL_DOOR, DOOMO_DOOR_TAG_RED, &cx, &cy)) {
        CHECK_EQ(map_door_bump(&doomo_map, cx, cy, 0), 0);
        CHECK_EQ(map_door_bump(&doomo_map, cx, cy, 1), 1);
    }
    /* lava hurts */
    combat_start_at(MAPID_CLOUD_NINE_SPIRE, CGOAL_EXIT, 8, 30, 0);
    CHECK(doomo_map.cells[30 * doomo_map.w + 8].flags & CELL_LAVA);
    for (t = 0; t < 60; t++) tick(0);
    CHECK(doomo_player.hp < 100);
}

/* ---- the boss fight opens the way out ----------------------------------- */
TEST(combat_vapula_phases_and_exit)
{
    int boss, t, phases = 0, last_state = -1, ex, ey;
    opt_set(OPT_DIFFICULTY, DIFF_NORMAL);
    opt_set(OPT_CHEAT_GOD_MODE, 1);
    combat_start_at(MAPID_CLOUD_NINE_SPIRE, CGOAL_EXIT, 28, 30, 3072);
    boss = find_actor(THING_LORD_VAPULA);
    CHECK(boss >= 0);
    CHECK(doomo_actors[boss].flags & AF_BOSS);
    CHECK(find_cell(CELL_DOOR, 50, &ex, &ey));
    CHECK_EQ(map_door_open(&doomo_map, ex, ey), 0);
    for (t = 0; t < 30 * 120 && doomo_actors[boss].hp > 0; t++) {
        DoomoActor *b = &doomo_actors[boss];
        if (b->state == AST_PHASE && last_state != AST_PHASE) phases++;
        last_state = b->state;
        if (t % 10 == 0) actor_damage(boss, 60, PLAYER_OWNER, 0, 0, 0, 0);
        doomo_player.angle = (int16_t)fx_atan2(b->y - doomo_player.y, b->x - doomo_player.x);
        tick(0);
        if (t == 300) CHECK(render_combat("vapula") < nat_w);   /* open sky, map edge */
    }
    CHECK_MSG(doomo_actors[boss].hp <= 0, "Vapula survived with %d hp", doomo_actors[boss].hp);
    CHECK_MSG(phases >= 2, "only %d phase changes", phases);
    for (t = 0; t < 200; t++) tick(0);
    CHECK(map_door_open(&doomo_map, ex, ey) > 0);
    CHECK(unlock_has(436));                            /* the BFG blueprint */
    CHECK_EQ(combat_finished(), 0);
    /* walk out */
    CHECK(find_cell(CELL_EXIT, -1, &ex, &ey));
    doomo_player.x = INT2FIX(ex * DOOMO_CELL_UNITS + DOOMO_CELL_UNITS / 2);
    doomo_player.y = INT2FIX(ey * DOOMO_CELL_UNITS + DOOMO_CELL_UNITS / 2);
    doomo_player.z = INT2FIX(cw_floor(ex, ey));
    for (t = 0; t < 40 && !combat_finished(); t++) tick(0);     /* the exit fanfare */
    CHECK_EQ(combat_finished(), 1);
    opt_set(OPT_CHEAT_GOD_MODE, 0);
}

/* ---- cameras ---------------------------------------------------------------- */
TEST(combat_cameras)
{
    DoomoView3D v;
    int t, k, px, py;
    combat_start(MAPID_VAPE_CELLAR);
    doomo_player.camera = CAM_FIRST_PERSON;
    tick(0);
    combat_build_view(&v);
    CHECK_EQ(v.camera, CAM_FIRST_PERSON);
    CHECK_EQ(v.player_sprite, -1);
    CHECK(fx_dist(v.x - doomo_player.x, v.y - doomo_player.y) < INT2FIX(4));
    CHECK(v.map == &doomo_map && v.sprite_count >= 0);
    CHECK(render_combat("cellar_fp") < nat_w);
    combat_toggle_camera();
    CHECK_EQ(doomo_player.camera, CAM_THIRD_PERSON);
    for (t = 0; t < 40; t++) tick(0);
    combat_build_view(&v);
    CHECK_EQ(v.camera, CAM_THIRD_PERSON);
    CHECK(v.player_sprite >= 0 && v.player_sprite < v.sprite_count);
    CHECK(v.sprites[v.player_sprite].img != NULL);
    CHECK(!cw_solid(cell_of(v.x), cell_of(v.y)));
    CHECK(fx_dist(v.x - doomo_player.x, v.y - doomo_player.y) > INT2FIX(24));
    CHECK(render_combat("cellar_tp") < nat_w);
    /* walk around: the camera never ends up inside a wall */
    for (t = 0; t < 600; t++) {
        uint32_t b = (t / 40) % 3 == 0 ? BTN_UP : (t / 40) % 3 == 1 ? BTN_UP | BTN_RIGHT : BTN_DOWN | BTN_LEFT;
        tick(b);
        combat_build_view(&v);
        px = cell_of(v.x); py = cell_of(v.y);
        CHECK_MSG(!cw_solid(px, py), "camera in a wall at %d,%d (tick %d)", px, py, t);
        CHECK(v.z > INT2FIX(cw_floor(px, py)));
    }
    /* shoulder swap moves the camera */
    {
        fix16 x0, y0;
        combat_build_view(&v);
        x0 = v.x; y0 = v.y;
        doomo_player.shoulder ^= 1;
        for (t = 0; t < 30; t++) tick(0);
        combat_build_view(&v);
        CHECK(v.x != x0 || v.y != y0);
    }
    /* back against a wall: the camera pulls in */
    for (k = 0; k < doomo_map.w * doomo_map.h; k++) {
        int x = k % doomo_map.w, y = k / doomo_map.w;
        if (x < 1 || y < 1 || x >= doomo_map.w - 1 || y >= doomo_map.h - 1) continue;
        if (!cw_solid(x, y) && cw_solid(x - 1, y) && !cw_solid(x + 1, y) && !cw_solid(x + 2, y) &&
            !(doomo_map.cells[k].flags & (CELL_DOOR | CELL_LIFT))) {
            combat_start_at(MAPID_VAPE_CELLAR, CGOAL_EXIT, x, y, 0);    /* facing east, wall behind */
            doomo_player.camera = CAM_THIRD_PERSON;
            for (t = 0; t < 30; t++) tick(0);
            combat_build_view(&v);
            CHECK(!cw_solid(cell_of(v.x), cell_of(v.y)));
            CHECK(fx_dist(v.x - doomo_player.x, v.y - doomo_player.y) < INT2FIX(64));
            render_combat("tp_wall");
            break;
        }
    }
    CHECK(k < doomo_map.w * doomo_map.h);
}

/* ---- the scenes ----------------------------------------------------------------- */
TEST(combat_scene_pause_finish_and_death)
{
    uint32_t t0;
    int i, cam, ex, ey;
    uint32_t money0;
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_COMBAT, MAPID_VAPE_CELLAR);
    CHECK(scene_top() == &SCENE_COMBAT);
    for (i = 0; i < 40; i++) frame(i > 20 && i < 30 ? BTN_UP : 0);
    shot("combat_play");
    t0 = combat_stats()->ticks;
    CHECK(t0 > 0);
    frame(BTN_START); frame(0);
    for (i = 0; i < 10; i++) frame(0);
    CHECK_EQ(combat_stats()->ticks, t0);              /* paused */
    shot("combat_pause");
    cam = doomo_player.camera;
    frame(BTN_DOWN); frame(0); frame(BTN_DOWN); frame(0);       /* -> Camera */
    frame(BTN_A); frame(0);
    CHECK(doomo_player.camera != cam);
    frame(BTN_UP); frame(0);                                    /* -> Weapons */
    frame(BTN_A); frame(0);
    shot("combat_weapons");
    frame(BTN_B); frame(0);
    frame(BTN_START); frame(0);
    for (i = 0; i < 5; i++) frame(0);
    CHECK(combat_stats()->ticks > t0);                 /* resumed */
    /* die -> retry */
    combat_hurt_player(999, 0, 0, -1);
    for (i = 0; i < 80; i++) frame(0);                          /* the death cam */
    CHECK_EQ(combat_finished(), -1);
    shot("combat_dead");
    frame(BTN_A); frame(0);
    CHECK_EQ(combat_finished(), 0);
    CHECK_EQ(doomo_player.hp, 100);
    CHECK(scene_top() == &SCENE_COMBAT);
    /* reach the exit -> tally -> leave */
    money0 = doomo_town.money;
    CHECK(find_cell(CELL_EXIT, -1, &ex, &ey));
    doomo_player.x = INT2FIX(ex * DOOMO_CELL_UNITS + DOOMO_CELL_UNITS / 2);
    doomo_player.y = INT2FIX(ey * DOOMO_CELL_UNITS + DOOMO_CELL_UNITS / 2);
    doomo_player.z = INT2FIX(cw_floor(ex, ey));
    for (i = 0; i < 90; i++) frame(0);
    CHECK_EQ(combat_finished(), 1);
    shot("combat_done");
    CHECK(doomo_town.money > money0);
    frame(BTN_A); frame(0);
    CHECK(scene_top() != &SCENE_COMBAT);
    CHECK_EQ(combat_last_result(), 1);
    /* quitting from the pause menu */
    scene_push(&SCENE_COMBAT, MAPID_CLOUD_NINE_SPIRE);
    for (i = 0; i < 10; i++) frame(0);
    shot("spire_play");
    frame(BTN_START); frame(0);
    frame(BTN_UP); frame(0);                                    /* wraps to Quit level */
    frame(BTN_A); frame(0);
    for (i = 0; i < 4 && scene_top() != &SCENE_COMBAT; i++) { frame(BTN_LEFT); frame(0); frame(BTN_A); frame(0); }
    CHECK(scene_top() != &SCENE_COMBAT);
    CHECK_EQ(combat_last_result(), -2);
}

TEST(combat_scene_workshop_and_weld_game)
{
    int i, before = 0, after = 0, frame_before;
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_WORKSHOP, 0);
    for (i = 0; i < 10; i++) frame(0);
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++) before += (doomo_town.weapons[i].flags & WF_VALID) != 0;
    CHECK(before >= 1);
    shot("workshop");
    frame_before = weld_pending.frame;
    frame(BTN_RIGHT); frame(0);
    CHECK(weld_pending.frame != frame_before);
    frame(BTN_X); frame(0);                                      /* surprise me */
    for (i = 0; i < 6; i++) frame(0);
    shot("workshop_random");
    weld_default(&weld_pending, WFRAME_PISTOL, WSTYLE_CUTE);     /* affordable */
    frame(BTN_A); frame(0);                                      /* jump to WELD! */
    frame(BTN_A); frame(0);                                      /* confirm modal */
    frame(BTN_LEFT); frame(0);
    frame(BTN_A); frame(0);                                      /* yes */
    CHECK(scene_top() == &SCENE_WELD_GAME);
    for (i = 0; i < 30; i++) frame(0);
    shot("weld_intro");
    /* weld: hold the torch on, steer roughly along the seam */
    for (i = 0; i < 30 * 40 && scene_top() == &SCENE_WELD_GAME; i++) {
        uint32_t b = (i % 5) ? BTN_A : 0;
        if (i == 400) shot("weld_game");
        frame(b);
    }
    for (i = 0; i < 300 && scene_top() == &SCENE_WELD_GAME; i++) frame(i % 2 ? BTN_A : 0);
    CHECK(scene_top() == &SCENE_WORKSHOP);
    for (i = 0; i < 4; i++) frame(0);
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++) after += (doomo_town.weapons[i].flags & WF_VALID) != 0;
    CHECK_EQ(after, before + 1);
    CHECK(progress_get(PROG_WEAPONS_WELDED) >= 1);
    shot("workshop_armory");
    /* practice from the weld game itself never welds */
    scene_push(&SCENE_WELD_GAME, 1);
    for (i = 0; i < 30 * 50 && scene_top() == &SCENE_WELD_GAME; i++) frame(i % 3 ? BTN_A : 0);
    CHECK(scene_top() == &SCENE_WORKSHOP);
    for (i = 0; i < 4; i++) frame(0);
    after = 0;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++) after += (doomo_town.weapons[i].flags & WF_VALID) != 0;
    CHECK_EQ(after, before + 1);
    frame(BTN_B); frame(0);
    CHECK(scene_top() != &SCENE_WORKSHOP);
}

TEST(combat_scene_arena_waves)
{
    int i;
    uint32_t money0 = doomo_town.money, kills;
    opt_set(OPT_CHEAT_GOD_MODE, 1);
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_ARENA, 0);
    combat_autopilot(1);
    for (i = 0; i < 30 * 150; i++) {
        frame(0);
        if (i == 400) shot("arena");
    }
    kills = combat_stats()->kills;
    CHECK_MSG(kills >= 8, "only %u arena kills", (unsigned)kills);
    combat_autopilot(0);
    opt_set(OPT_CHEAT_GOD_MODE, 0);
    frame(BTN_START); frame(0);
    shot("arena_pause");
    frame(BTN_DOWN); frame(0); frame(BTN_DOWN); frame(0);
    frame(BTN_A); frame(0);                                      /* give up */
    for (i = 0; i < 70; i++) frame(0);
    shot("arena_over");
    CHECK(doomo_town.money > money0);
    CHECK(progress_get(PROG_ARENA_WAVES) >= 1);
    frame(BTN_A); frame(0);
    CHECK(scene_top() != &SCENE_ARENA);
}

/* ---- misc API ----------------------------------------------------------------- */
TEST(combat_api_misc)
{
    int t;
    CHECK(combat_demon_name(THING_LORD_VAPULA)[0]);
    CHECK(combat_demon_blurb(THING_IMPLING)[0]);
    CHECK(combat_give_starter_weapons() >= 0);
    combat_start_ex(MAPID_ARENA, CGOAL_KILL_ALL);
    for (t = 0; t < 40; t++) tick(0);
    CHECK_EQ(combat_finished(), 1);                    /* nothing to kill */
    combat_start_ex(MAPID_ARENA, CGOAL_KILL_ALL);
    CHECK(combat_spawn(THING_PUFF_DEMON, 16, 16, 1) >= 0);
    tick(0);
    CHECK_EQ(combat_finished(), 0);
    CHECK_EQ(combat_demons_alive(), 1);
    combat_refill_ammo(100);
    for (t = 0; t < AMMO_COUNT; t++) CHECK(combat_ammo(t) > 0 && combat_ammo(t) <= combat_ammo_max(t));
    CHECK_EQ(combat_ammo(AMMO_COUNT), 0);
    combat_next_weapon(1);
    combat_next_weapon(-1);
    CHECK(combat_weapon() != NULL);
    /* an empty map id falls back to a playable room */
    combat_start(MAPID_COUNT + 3);
    for (t = 0; t < 30; t++) tick(BTN_UP);
    CHECK(doomo_player.hp == 100);
}
