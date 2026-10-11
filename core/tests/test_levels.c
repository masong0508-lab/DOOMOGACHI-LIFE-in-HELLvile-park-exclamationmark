/* test_levels.c - the levels expansion (owner: levels): the five new Hell
 * Breach maps showcase the 3D features and are sound (an independent
 * check on top of test_maps: heights, lifts, doors, sky, water / lava,
 * secrets, a mini-boss whose kill event opens a remote door), the debug
 * autopilot plays every level to its exit through the real shooter with
 * the levels runtime on (mini-bosses, conveyors), every level renders in
 * first and third person at 240x160 and 320x240 without holes, ranks and
 * records pack into PROG_BREACH_RANKS, unlocks and activities are
 * registered, mini-boss variants scale / glow / summon / blink / fire,
 * conveyors push, and the LEVEL_SELECT / BREACH / ARENA_MODE scenes run
 * (Boss Rush, Horde with a shop break, Monster Mode Mayhem). Screens land
 * in the test tmp dir. */
#include "test.h"
#include "../src/levels_int.h"

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

static void shot(const char *name)
{
    char path[600];
    snprintf(path, sizeof(path), "%s/levels_%s.png", test_tmp_dir, name);
    nat_png_write(path, nat_fb, nat_w, nat_h, 1);
}

static int cell_of(fix16 v) { return FIX2INT(v) / DOOMO_CELL_UNITS; }

/* ---- tables -------------------------------------------------------------- */
TEST(levels_tables_are_sane)
{
    int i, n = 0;
    for (i = 0; i < LV_COUNT; i++) {
        const LevelInfo *L = &lv_info[i];
        const DoomoMapDef *d = doomo_maps[L->map];
        char tb[12], how[80];
        CHECK_MSG(d && d->cells && d->kind == MAP_DUNGEON, "level %d map %d missing", i, L->map);
        CHECK_STR(map_title(L->map), L->title);
        CHECK(L->dev > 30 && L->dev < L->par);
        CHECK(L->boss < LB_COUNT);
        CHECK(L->pv_x < d->w && L->pv_y < d->h && !d->cells[L->pv_y * d->w + L->pv_x].wall);
        CHECK(strlen(L->blurb) < 70 && strlen(L->flavor) <= 40);
        CHECK(ui_text_width(L->flavor) + 16 <= 240);
        /* the gold trophy's hint quotes the dev time */
        lvd_time(tb, sizeof(tb), L->dev);
        CHECK(unlock_by_id(LVU_GOLD0 + i) != NULL);
        doomo_snprintf(how, sizeof(how), "%s", unlock_by_id(LVU_GOLD0 + i)->how);
        CHECK_MSG(strstr(how, tb) != NULL, "gold hint of level %d (%s) does not say %s", i, how, tb);
    }
    for (i = 0; i < LB_COUNT; i++) {
        const LevelBoss *b = &lv_boss[i];
        CHECK(IS_DEMON(b->type) && IS_DEMON(b->add));
        CHECK(b->scale >= 256 && b->hp8 >= 8);
        CHECK(sprite_image(b->spr, 0) != NULL);
        CHECK_MSG(ui_text_width(b->taunt) + 14 <= 240, "taunt of %s too wide", b->name);
        CHECK_MSG(ui_text_width(b->title) <= 236, "title of %s too wide", b->name);
    }
    /* unlocks: our range, our MISC list, all registered */
    for (i = 0; doomo_unlocks_levels[i].name; i++) {
        const DoomoUnlock *u = &doomo_unlocks_levels[i];
        CHECK(u->id >= UL_IDS_LEVELS_FIRST && u->id <= UL_IDS_LEVELS_LAST);
        CHECK(unlock_by_id(u->id) == u);
        if (u->cat == UL_CAT_MISC) CHECK(u->list == UL_MISC_LEVELS);
        CHECK(u->how && u->how[0]);
        n++;
    }
    CHECK(n >= 27);
    CHECK(unlock_by_id(LVU_LAST) != NULL);
    /* the activities: Arena + Hellvile, every gate is a real unlock */
    {
        int arena = 0, hell = 0, k = activity_count();
        for (i = 0; i < k; i++) {
            const DoomoActivity *a = activity_at(i);
            if (a->scene != &SCENE_LEVEL_SELECT && a->scene != &SCENE_ARENA_MODE && a->scene != &SCENE_BREACH) continue;
            if (a->location == ACTLOC_ARENA) arena++;
            if (a->location == ACTLOC_HELLVILE) hell++;
            CHECK(a->unlock_id == ACT_ALWAYS || unlock_by_id(a->unlock_id) != NULL);
            CHECK(strlen(a->desc) <= 40);
        }
        CHECK(arena >= 7);
        CHECK(hell >= 1);
    }
    CHECK(scene_by_name("LEVEL_SELECT") == &SCENE_LEVEL_SELECT);
    CHECK(scene_by_name("BREACH") == &SCENE_BREACH);
    CHECK(scene_by_name("ARENA_MODE") == &SCENE_ARENA_MODE);
}

/* ---- the maps showcase the 3D features ------------------------------------ */
TEST(levels_maps_showcase_3d)
{
    int lv;
    for (lv = LV_FIRST_NEW; lv < LV_COUNT; lv++) {
        const DoomoMapDef *d = doomo_maps[lv_info[lv].map];
        uint8_t seen_f[256], seen_c[256];
        int i, nf = 0, nc = 0, lifts = 0, doors = 0, sky = 0, wet = 0, secret_cells = 0, exits = 0;
        int boss = -1, boss_tag = -1, remote_with_tag = 0, triggers = 0;
        memset(seen_f, 0, sizeof(seen_f));
        memset(seen_c, 0, sizeof(seen_c));
        for (i = 0; i < d->w * d->h; i++) {
            const DoomoCell *c = &d->cells[i];
            if (c->wall) continue;
            if (!seen_f[(uint8_t)c->floor_h]) { seen_f[(uint8_t)c->floor_h] = 1; nf++; }
            if (c->ceil_tex != DOOMO_TEX_SKY && !seen_c[(uint8_t)c->ceil_h]) { seen_c[(uint8_t)c->ceil_h] = 1; nc++; }
            else if (c->ceil_tex == DOOMO_TEX_SKY) sky++;
            lifts += (c->flags & CELL_LIFT) != 0;
            doors += (c->flags & CELL_DOOR) != 0;
            wet += (c->flags & (CELL_WATER | CELL_LAVA)) != 0;
            secret_cells += (c->flags & CELL_SECRET) != 0;
            exits += (c->flags & CELL_EXIT) != 0;
            triggers += (c->flags & CELL_TRIGGER) != 0;
        }
        for (i = 0; i < d->thing_count; i++)
            if (d->things[i].flags & THF_BOSS) {
                CHECK_MSG(boss < 0, "level %d has two bosses", lv);
                boss = d->things[i].type;
                CHECK(d->things[i].flags & THF_KILL_EVENT);
                boss_tag = d->things[i].arg;
            }
        for (i = 0; i < d->w * d->h; i++)
            if ((d->cells[i].flags & CELL_DOOR) && d->cells[i].tag == boss_tag) remote_with_tag++;
        CHECK_MSG(nf >= 8, "level %d: only %d floor heights", lv, nf);
        CHECK_MSG(nc >= 4, "level %d: only %d ceiling heights", lv, nc);
        CHECK_MSG(lifts >= 1 && doors >= 2, "level %d: lifts %d doors %d", lv, lifts, doors);
        CHECK_MSG(sky >= 40, "level %d: only %d sky cells", lv, sky);
        CHECK_MSG(wet >= 4, "level %d: no water / lava", lv);
        CHECK_MSG(secret_cells >= 2, "level %d: secrets", lv);
        CHECK(exits >= 1 && triggers >= 1);
        CHECK_MSG(boss == lv_boss[lv_info[lv].boss].type, "level %d boss type %d", lv, boss);
        CHECK_MSG(boss_tag >= DOOMO_DOOR_TAG_REMOTE && remote_with_tag >= 1, "level %d: the boss opens nothing", lv);
        CHECK(d->thing_count >= 60 && d->thing_count <= 100);   /* lively, but room for effects */
        map_load(lv_info[lv].map);
        CHECK_EQ(map_mover_count(), lifts + doors);              /* every mover fits the table */
    }
}

/* ---- the autopilot plays every level to its exit ----------------------------- */
static int play_level(int lv, int god, uint32_t max_ticks, int render)
{
    uint32_t t;
    char name[40];
    combat_start(lv_info[lv].map);
    combat_refill_ammo(100);                 /* like SCENE_BREACH */
    lv_runtime_begin(lv);
    opt_set(OPT_CHEAT_GOD_MODE, god);
    combat_autopilot(1);
    for (t = 0; t < max_ticks && !combat_finished(); t++) {
        input_update(0);
        lv_runtime_pre();
        combat_tick();
        lv_runtime_post();
        if (render && t == 1800) {
            DoomoView3D v;
            combat_build_view(&v);
            lv_runtime_view(&v);
            plat_view3d(&v);
            lv_runtime_hud();
            snprintf(name, sizeof(name), "bot_%d", lv);
            shot(name);
        }
    }
    combat_autopilot(0);
    opt_set(OPT_CHEAT_GOD_MODE, 0);
    return combat_finished();
}

TEST(levels_autopilot_reaches_every_exit)
{
    int lv;
    opt_set(OPT_DIFFICULTY, DIFF_PLUSHIE);
    for (lv = LV_FIRST_NEW; lv < LV_COUNT; lv++) {
        const DoomoCombatStats *s;
        int fin;
        rng_seed(&doomo_rng, 11 + lv);
        fin = play_level(lv, 1, 30 * 60 * 12, 1);
        s = combat_stats();
        CHECK_MSG(fin == 1, "%s: autopilot stuck at cell %d,%d after %u ticks (kills %d/%d, boss alive %d)",
                  lv_info[lv].title, cell_of(doomo_player.x), cell_of(doomo_player.y), (unsigned)s->ticks,
                  s->kills, s->kills_total, lv_bosses_alive(0));
        CHECK_MSG(unlock_has(LVU_BOSS0 + lv_info[lv].boss), "%s: the mini-boss trophy", lv_info[lv].title);
        printf("      %s: exit after %u s, kills %d/%d, secrets %d/%d\n", lv_info[lv].title,
               (unsigned)(s->ticks / DOOMO_TICK_HZ), s->kills, s->kills_total, s->secrets, s->secrets_total);
    }
}

/* ---- every level renders in first and third person at both sizes -------------- */
TEST(levels_render_fp_tp_both_sizes)
{
    int lv, res, cam, spot;
    for (res = 0; res < 2; res++) {
        nat_w = res ? 320 : 240;
        nat_h = res ? 240 : 160;
        for (lv = LV_FIRST_NEW; lv < LV_COUNT; lv++) {
            const DoomoMapDef *d = doomo_maps[lv_info[lv].map];
            int bx = 0, by = 0, i;
            for (i = 0; i < d->thing_count; i++)
                if (d->things[i].flags & THF_BOSS) { bx = d->things[i].x; by = d->things[i].y; }
            /* stand a few cells from the boss, on a plain floor */
            for (i = 0; i < 64; i++) {
                int x = bx + (i % 9) - 4, y = by + 3 + (i / 9) % 3 - 1;
                const DoomoCell *c = (x > 0 && y > 0 && x < d->w && y < d->h) ? &d->cells[y * d->w + x] : NULL;
                if (c && !c->wall && !(c->flags & (CELL_LAVA | CELL_DOOR | CELL_LIFT))) { bx = x; by = y - 3; break; }
            }
            for (spot = 0; spot < 2; spot++) {
                if (spot == 0) combat_start(lv_info[lv].map);
                else combat_start_at(lv_info[lv].map, CGOAL_EXIT, bx, by + 3, 3072);
                lv_runtime_begin(lv);
                lv_runtime_pre();
                combat_tick();
                lv_runtime_post();
                lv_run.intro_t = 0;                        /* the HUD, not the intro card */
                for (cam = 0; cam < 2; cam++) {
                    DoomoView3D v;
                    char name[48];
                    int holes = 0;
                    doomo_player.camera = (uint8_t)cam;
                    combat_build_view(&v);
                    lv_runtime_view(&v);
                    plat_clear(PAL_UI_RED);
                    plat_view3d(&v);
                    if (!v.tint_level)
                        for (i = 0; i < nat_w * nat_h; i++) holes += nat_fb[i] == PAL_UI_RED;
                    CHECK_MSG(holes < nat_w, "%s %s %s: %d unpainted pixels", lv_info[lv].title,
                              spot ? "boss" : "start", cam ? "tp" : "fp", holes);
                    lv_runtime_hud();
                    snprintf(name, sizeof(name), "%d_%s_%s_%dx%d", lv, spot ? "boss" : "start", cam ? "tp" : "fp", nat_w, nat_h);
                    shot(name);
                }
            }
        }
    }
    nat_w = 240;
    nat_h = 160;
}

/* ---- ranks & records -------------------------------------------------------------- */
TEST(levels_ranks_and_records)
{
    DoomoCombatStats s;
    LvScore sc;
    int bonus = 0, news, lv;
    memset(&s, 0, sizeof(s));
    /* scoring */
    s.ticks = (uint32_t)lv_info[LV_SEWERS].par * DOOMO_TICK_HZ;
    s.kills = s.kills_total = 20;
    s.secrets = s.secrets_total = 3;
    lv_score(LV_SEWERS, &s, &sc);
    CHECK_EQ(sc.total, 100);
    CHECK_EQ(sc.rank, RANK_S);
    s.ticks *= 3;
    s.kills = 10;
    s.secrets = 0;
    lv_score(LV_SEWERS, &s, &sc);
    CHECK_EQ(sc.time_pts, 0);
    CHECK_EQ(sc.rank, RANK_C);
    s.ticks = (uint32_t)lv_info[LV_SEWERS].par * DOOMO_TICK_HZ * 5 / 4;
    s.kills = 20;
    s.secrets = 2;
    lv_score(LV_SEWERS, &s, &sc);
    CHECK_EQ(sc.rank, RANK_A);                                  /* 30 + 35 + 16 */
    /* the unlock chain */
    CHECK(lv_unlocked(LV_VAPE_CELLAR));
    CHECK(!lv_unlocked(LV_SEWERS) && !lv_unlocked(LV_CLOUD_NINE));
    CHECK(lv_set_rank(LV_VAPE_CELLAR, RANK_B));
    CHECK(lv_unlocked(LV_SEWERS) && lv_unlocked(LV_CLOUD_NINE) && !lv_unlocked(LV_FACTORY));
    CHECK(!lv_set_rank(LV_VAPE_CELLAR, RANK_C));                /* only improves */
    CHECK_EQ(lv_rank(LV_VAPE_CELLAR), RANK_B);
    /* the horde best shares the counter without touching ranks */
    lv_set_horde_best(17);
    lv_set_horde_best(9);
    CHECK_EQ(lv_horde_best(), 17);
    CHECK_EQ(lv_rank(LV_VAPE_CELLAR), RANK_B);
    for (lv = 0; lv < LV_COUNT; lv++) CHECK(lv_set_rank(lv, RANK_S) || lv_rank(lv) == RANK_S);
    CHECK_EQ(lv_horde_best(), 17);
    CHECK_EQ(progress_get(PROG_BREACH_RANKS) >> 21, 17u);
    /* finishing: first clear bonus, trophies, the time trial gold */
    test_reset();
    s.ticks = (uint32_t)lv_info[LV_FACTORY].dev * DOOMO_TICK_HZ;
    s.kills = s.kills_total = 25;
    s.secrets = s.secrets_total = 3;
    {
        uint32_t money0 = doomo_town.money;
        news = lv_finish(LV_FACTORY, LVM_TIME_TRIAL, &s, &sc, &bonus);
        CHECK(news & LVR_FIRST_CLEAR);
        CHECK(news & LVR_NEW_RANK);
        CHECK(news & LVR_GOLD);
        CHECK(bonus >= 300 + 200);
        CHECK_EQ(doomo_town.money, money0 + (uint32_t)bonus);
    }
    CHECK(lv_gold(LV_FACTORY));
    CHECK(unlock_has(LVU_TIME_TRIAL) && unlock_has(LVU_FIRST_S));
    CHECK(!unlock_has(LVU_CHALLENGES));
    lv_set_rank(LV_SEWERS, RANK_C);
    lv_set_rank(LV_GRAVEYARD, RANK_C);
    news = lv_finish(LV_GRAVEYARD, LVM_CUTE_ONLY, &s, &sc, &bonus);
    CHECK(news & LVR_CHALLENGE);
    CHECK(unlock_has(LVU_CHALLENGES) && unlock_has(LVU_CUTE_CLEAR));
    CHECK_EQ(lv_rank(LV_GRAVEYARD), RANK_S);
}

/* ---- mini-boss variants ------------------------------------------------------------ */
TEST(levels_variants_scale_glow_and_fight)
{
    DoomoView3D v;
    int a, slot, k, base_hp, base_r;
    const DoomoBillboard *bb = NULL;
    combat_start_ex(MAPID_ARENA, CGOAL_ENDLESS);
    lv_runtime_begin(255);
    a = actor_spawn_demon(THING_SLIME_BELCHER, INT2FIX(16 * 64 + 32), INT2FIX(10 * 64 + 32), 0, THF_BOSS, 0);
    CHECK(a >= 0);
    base_hp = doomo_actors[a].hp;
    base_r = doomo_actors[a].radius;
    slot = lv_make_boss(a, LB_GUTTERKING, 8);
    CHECK(slot >= 0);
    CHECK_EQ(doomo_actors[a].hp, base_hp * lv_boss[LB_GUTTERKING].hp8 / 8);
    CHECK(doomo_actors[a].radius > base_r && doomo_actors[a].radius <= 30);
    CHECK(lv_bosses_alive(0) == 1 && lv_bosses_alive(1) == 0);
    /* the billboard is scaled up and glows */
    doomo_player.x = INT2FIX(16 * 64 + 32);
    doomo_player.y = INT2FIX(16 * 64 + 32);
    doomo_player.angle = 3072;
    combat_build_view(&v);
    for (k = 0; k < v.sprite_count; k++)
        if (v.sprites[k].x == doomo_actors[a].x && v.sprites[k].y == doomo_actors[a].y) bb = &v.sprites[k];
    CHECK(bb != NULL);
    k = bb->scale;
    lv_runtime_view(&v);
    CHECK_EQ(bb->scale, k * lv_boss[LB_GUTTERKING].scale / 256);
    CHECK(bb->light >= 2);
    /* it wakes: intro banner, boss music, our bar */
    doomo_actors[a].state = AST_CHASE;
    doomo_actors[a].target = TARGET_PLAYER;
    lv_runtime_post();
    CHECK(lv_run.intro_t > 0 && lv_run.intro_boss == LB_GUTTERKING);
    CHECK_EQ(lv_run.bar, slot);
    combat_build_view(&v);
    lv_runtime_view(&v);
    CHECK(v.tint == TINT_GREEN && v.tint_level >= 2);
    plat_view3d(&v);
    lv_runtime_hud();
    shot("variant_gutterking");
    /* it summons when hurt */
    doomo_actors[a].hp = (int16_t)(lv_run.track[slot].hp_max / 2);
    k = combat_demons_alive();
    for (lv_run.track[slot].t = 0; lv_run.track[slot].t < 210;) lv_runtime_post();
    CHECK(combat_demons_alive() > k);
    /* death: trophy + our toast */
    doomo_actors[a].hp = 0;
    doomo_actors[a].state = AST_DYING;
    lv_runtime_post();
    CHECK(unlock_has(LVU_BOSS0 + LB_GUTTERKING));
    CHECK_EQ(lv_bosses_alive(0), 0);
    /* the widow blinks, the bishop's organ fires, the gatekeeper opens portals */
    a = lv_spawn_boss(LB_WIDOW, 16, 8, 8);
    CHECK(a >= 0);
    doomo_actors[a].state = AST_CHASE;
    {
        fix16 x0 = doomo_actors[a].x, y0 = doomo_actors[a].y;
        for (k = 0; k < 160 && doomo_actors[a].x == x0 && doomo_actors[a].y == y0; k++) {
            doomo_actors[a].state = AST_CHASE;
            lv_runtime_post();
        }
        CHECK(doomo_actors[a].x != x0 || doomo_actors[a].y != y0);
    }
    a = lv_spawn_boss(LB_BONEBOTTOM, 10, 10, 8);
    CHECK(a >= 0);
    doomo_actors[a].state = AST_CHASE;
    {
        int proj0 = 0, proj1 = 0, i;
        for (i = 0; i < ACTOR_MAX; i++) proj0 += doomo_actors[i].type == AK_PROJECTILE && (doomo_actors[i].flags & AF_USED);
        for (k = 0; k < 80; k++) { doomo_actors[a].state = AST_CHASE; lv_runtime_post(); }
        for (i = 0; i < ACTOR_MAX; i++) proj1 += doomo_actors[i].type == AK_PROJECTILE && (doomo_actors[i].flags & AF_USED);
        CHECK(proj1 > proj0);
    }
}

TEST(levels_conveyors_push)
{
    int i, cx = -1, cy = -1;
    fix16 x0;
    combat_start(MAPID_PLUSHIE_FACTORY);
    lv_runtime_begin(LV_FACTORY);
    for (i = 0; i < doomo_map.w * doomo_map.h && cx < 0; i++)
        if (doomo_map.cells[i].tag == LV_TAG_CONVEYOR_E && !doomo_map.cells[i].wall) {
            cx = i % doomo_map.w + 2;
            cy = i / doomo_map.w;
        }
    CHECK(cx > 0);
    combat_start_at(MAPID_PLUSHIE_FACTORY, CGOAL_EXIT, cx, cy, 0);
    lv_runtime_begin(LV_FACTORY);
    for (i = 0; i < 10; i++) { input_update(0); combat_tick(); }
    x0 = doomo_player.x;
    for (i = 0; i < 30; i++) { input_update(0); lv_runtime_pre(); combat_tick(); lv_runtime_post(); }
    CHECK_MSG(doomo_player.x > x0 + INT2FIX(40), "pushed only %d units", FIX2INT(doomo_player.x - x0));
    CHECK(cell_of(doomo_player.y) == cy);
}

/* ---- the scenes -------------------------------------------------------------------- */
TEST(levels_select_and_breach_scenes)
{
    int i;
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_LEVEL_SELECT, LVM_NORMAL);
    frame(0); frame(0);
    CHECK(scene_top() == &SCENE_LEVEL_SELECT);
    for (i = 0; i < 20; i++) frame(0);
    shot("select_fresh");
    frame(BTN_RIGHT); frame(0);                          /* the spire: locked */
    frame(BTN_A); frame(0);
    CHECK(scene_top() == &SCENE_LEVEL_SELECT);
    frame(BTN_LEFT); frame(0);
    frame(BTN_A); frame(0);                              /* the cellar */
    CHECK(scene_top() == &SCENE_BREACH);
    for (i = 0; i < 20; i++) frame(0);
    shot("breach_intro");
    for (i = 0; i < 80; i++) frame(0);
    frame(BTN_START); frame(0);
    shot("breach_pause");
    frame(BTN_UP); frame(0);                             /* -> quit level */
    frame(BTN_A); frame(0);
    CHECK(scene_top() == &SCENE_LEVEL_SELECT);
    CHECK(combat_last_result() == -2);
    /* a demo: the autopilot clears the cellar, then back to the select */
    opt_set(OPT_CHEAT_GOD_MODE, 1);
    scene_push(&SCENE_BREACH, LV_ARG(LV_VAPE_CELLAR, LVM_NORMAL) | LVF_DEMO);
    for (i = 0; i < 30 * 60 * 10 && scene_top() == &SCENE_BREACH; i++) {
        frame(0);
        if (i == 900) shot("breach_demo");
    }
    opt_set(OPT_CHEAT_GOD_MODE, 0);
    CHECK_MSG(scene_top() == &SCENE_LEVEL_SELECT, "demo still running after %d frames", i);
    CHECK_EQ(lv_rank(LV_VAPE_CELLAR), RANK_NONE);        /* demos record nothing */
    scene_set(&SCENE_TITLE, 0);
}

/* a well-played island: every rank colour, gold medals, all modes open */
TEST(levels_select_ranked_screens)
{
    static const uint8_t ranks[LV_COUNT] = { RANK_S, RANK_A, RANK_S, RANK_B, RANK_C, RANK_A, RANK_NONE };
    int i, res;
    for (i = 0; i < LV_COUNT; i++) if (ranks[i]) lv_set_rank(i, ranks[i]);
    unlock_grant(LVU_GOLD0 + LV_VAPE_CELLAR);
    unlock_grant(LVU_GOLD0 + LV_SEWERS);
    unlock_grant(LVU_TIME_TRIAL);
    unlock_grant(LVU_CHALLENGES);
    CHECK(lv_unlocked(LV_UNDERBELLY) && lv_cleared_count() == 6);
    for (res = 0; res < 2; res++) {
        char name[40];
        nat_w = res ? 320 : 240;
        nat_h = res ? 240 : 160;
        scene_set(&SCENE_TITLE, 0);
        scene_push(&SCENE_LEVEL_SELECT, LVM_NORMAL);
        for (i = 0; i < 30; i++) frame(0);
        CHECK(scene_top() == &SCENE_LEVEL_SELECT);
        snprintf(name, sizeof(name), "select_ranked_%dx%d", nat_w, nat_h);   /* the first uncleared one */
        shot(name);
        frame(BTN_LEFT); frame(0);
        frame(BTN_LEFT); frame(0);
        frame(BTN_LEFT); frame(0);
        frame(BTN_LEFT); frame(0);                                          /* the S-rank sewers */
        frame(BTN_R); frame(0);                                             /* -> Time Trial */
        for (i = 0; i < 20; i++) frame(0);
        snprintf(name, sizeof(name), "select_timetrial_%dx%d", nat_w, nat_h);
        shot(name);
        frame(BTN_R); frame(0);                                             /* -> Cute-Only */
        for (i = 0; i < 20; i++) frame(0);
        snprintf(name, sizeof(name), "select_cute_%dx%d", nat_w, nat_h);
        shot(name);
        frame(BTN_R); frame(0);                                             /* -> Brutal-Only */
        for (i = 0; i < 20; i++) frame(0);
        snprintf(name, sizeof(name), "select_brutal_%dx%d", nat_w, nat_h);
        shot(name);
        frame(BTN_B); frame(0);
        CHECK(scene_top() == &SCENE_TITLE);
    }
    nat_w = 240;
    nat_h = 160;
}

TEST(levels_breach_clear_grades_and_unlocks)
{
    int i;
    opt_set(OPT_DIFFICULTY, DIFF_PLUSHIE);
    opt_set(OPT_CHEAT_GOD_MODE, 1);
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_LEVEL_SELECT, LVM_NORMAL | ((LV_VAPE_CELLAR + 1) << 4));
    frame(0); frame(0);
    frame(BTN_A); frame(0);
    CHECK(scene_top() == &SCENE_BREACH);
    combat_autopilot(1);
    for (i = 0; i < 30 * 60 * 10 && !combat_finished(); i++) frame(0);
    combat_autopilot(0);
    opt_set(OPT_CHEAT_GOD_MODE, 0);
    CHECK(combat_finished() == 1);
    for (i = 0; i < 160; i++) frame(0);
    shot("breach_results");
    CHECK(lv_rank(LV_VAPE_CELLAR) != RANK_NONE);
    CHECK(unlock_has(LVU_TIME_TRIAL));
    frame(BTN_A); frame(0); frame(0);
    CHECK(scene_top() == &SCENE_LEVEL_SELECT);
    for (i = 0; i < 6; i++) frame(0);
    CHECK(lv_unlocked(LV_SEWERS));
    shot("select_after_clear");
    scene_set(&SCENE_TITLE, 0);
}

TEST(levels_arena_modes_run)
{
    int i, saw_shop = 0;
    opt_set(OPT_CHEAT_GOD_MODE, 1);
    opt_set(OPT_DIFFICULTY, DIFF_PLUSHIE);
    /* horde: a few waves, a shop break, buy something */
    unlock_grant(530);
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_ARENA_MODE, AM_HORDE);
    combat_autopilot(1);
    for (i = 0; i < 30 * 60 * 6 && !saw_shop; i++) {
        frame(0);
        if (i == 300) shot("horde_fight");
        if (lv_horde_best() >= 3) saw_shop = 1;
    }
    combat_autopilot(0);
    CHECK_MSG(saw_shop, "horde only reached wave %d", lv_horde_best());
    for (i = 0; i < 24; i++) frame(0);                       /* the window pops in */
    shot("horde_shop");
    frame(BTN_A); frame(0);                                  /* candy heart (if affordable) */
    frame(BTN_B); frame(0);                                  /* next wave */
    frame(BTN_START); frame(0);
    frame(BTN_UP); frame(0);                                 /* give up */
    frame(BTN_A); frame(0);
    for (i = 0; i < 70; i++) frame(0);
    shot("horde_over");
    CHECK(progress_get(PROG_ARENA_WAVES) >= 3);
    frame(BTN_DOWN); frame(0);
    frame(BTN_A); frame(0); frame(0);
    CHECK(scene_top() != &SCENE_ARENA_MODE);
    /* boss rush: the autopilot beats all seven */
    scene_push(&SCENE_ARENA_MODE, AM_BOSS_RUSH);
    combat_autopilot(1);
    for (i = 0; i < 30 * 60 * 15 && !unlock_has(LVU_BOSS_RUSH); i++) {
        frame(0);
        if (i == 400) shot("rush_round1");
    }
    combat_autopilot(0);
    CHECK_MSG(unlock_has(LVU_BOSS_RUSH), "boss rush stuck after %d frames", i);
    for (i = 0; i < 70; i++) frame(0);
    shot("rush_victory");
    frame(BTN_A); frame(0); frame(0);
    CHECK(scene_top() != &SCENE_ARENA_MODE);
    /* mayhem: needs Monster Mode; three minutes of chaos */
    CHECK(!unlock_has(LVU_MAYHEM));
    opt_set(OPT_MONSTER_MODE, 1);
    lv_check_monster();
    CHECK(unlock_has(LVU_MAYHEM));
    scene_push(&SCENE_ARENA_MODE, AM_MAYHEM);
    combat_autopilot(1);
    for (i = 0; i < 30 * 190 && scene_top() == &SCENE_ARENA_MODE; i++) {
        frame(0);
        if (i == 1500) shot("mayhem");
        if (i > 5600) break;
    }
    combat_autopilot(0);
    for (i = 0; i < 70; i++) frame(0);
    shot("mayhem_over");
    CHECK(combat_stats()->kills > 10);
    opt_set(OPT_CHEAT_GOD_MODE, 0);
    opt_set(OPT_MONSTER_MODE, 0);
    scene_set(&SCENE_TITLE, 0);
}
