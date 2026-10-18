/* test_home_defend.c - Hold the Door (owner: H1; HF sec.11.1 home_defend_waves,
 * SPEC H1 acceptance): the wave tables add up, the door's HP is the room's
 * Barricade, a scripted wave 3 with no input loses exactly the Mayor's
 * "safe", the chimney beat fires on wave 3, the decoy works once, input
 * changes the outcome, and the whole thing is deterministic. The rules run
 * on a hand-written catalog so the numbers never move with catalog.txt. */
#include "test.h"
#include "../src/home_int.h"

enum { D_DOOR, D_WINDOW, D_HEARTH, D_PLATE, D_TURRET, D_DRESSER, D_CHAIR, D_N };
#define ROW(name, cat, fl, fl2, fi, w, d, h, mass, role) \
    { name, "", 10, 0xFFFF, 0xFFFF, cat, fl, fl2, fi, w, d, h, 0xFF, 0xFF, 1, 0, 1, \
      0xFF, 0, 0, THEME_PASTEL, mass, 0, 0, 1, role }
static const DoomoFurn dcat[D_N] = {
    ROW("Door", FCAT_WALL, FF_WALL, FF_PORTAL, FI_NONE, 1, 1, 13, 0, FR_DOOR),
    ROW("Window", FCAT_WALL, FF_WALL, FF_PORTAL, FI_NONE, 1, 1, 6, 0, FR_WINDOW),
    ROW("Hearth", FCAT_PASTEL, FF_SURFACE, FF_PORTAL, FI_NONE, 2, 1, 12, 0, FR_HEARTH),
    ROW("Plate", FCAT_WALL, FF_WALL, FF_UNSELLABLE, FI_NONE, 1, 1, 6, 0, FR_PLATE),
    ROW("Turret", FCAT_TECH, 0, FF_UNSELLABLE, FI_TURRET, 1, 1, 6, 8, FR_TURRET),
    ROW("Dresser", FCAT_PASTEL, FF_SURFACE, 0, FI_NONE, 2, 1, 8, 20, FR_NONE),
    ROW("Chair", FCAT_PASTEL, 0, 0, FI_SIT, 1, 1, 6, 6, FR_SEAT),
};

static void use_dcat(void)
{
    test_reset();
    home_test_catalog(dcat, D_N);
    home_reset();
}

static void done_dcat(void) { home_test_catalog(NULL, 0); }

static void put(HomeRoom *rm, int furn, int x, int y, int rot, int z)
{
    HomeObj *o = &rm->obj[rm->nobj++];
    o->w0 = HO_W0(furn, rot, 0);
    o->pos = HO_POS(x, y);
    o->z = (uint8_t)z;
    o->pa = o->pb = FURN_PAINT_DEFAULT;
}

/* The Living Room boarded up for the siege: plates on both window slots
 * (wall A x = 1, wall B y = 4), the turret in front of the wall-A window,
 * the dresser braced next to the door (wall A x = 4). */
static void shelter(HomeRoom *rm)
{
    home_room_template(HOME_ROOM_LIVING, rm);
    put(rm, D_PLATE, 1, 0, 0, 6);
    put(rm, D_PLATE, 0, 4, 1, 6);
    put(rm, D_TURRET, 1, 0, 0, 0);
    put(rm, D_DRESSER, 2, 1, 0, 0);
}

/* run to the end; returns the OR of every tick's events */
static uint32_t run(HomeDefend *d, int *chimney_wave, int *decoys)
{
    uint32_t all = 0;
    int guard = 0;
    *chimney_wave = -1;
    *decoys = 0;
    while (home_defend_tick(d) && guard++ < 30000) {
        all |= d->ev;
        if (d->ev & HDE_CHIMNEY) *chimney_wave = d->wave;
        if (d->ev & HDE_DECOY) (*decoys)++;
    }
    all |= d->ev;
    return all;
}

TEST(home_defend_waves)
{
    HomeRoom rm;
    HomeScore sc;
    static HomeDefend d;
    int w, imps = 0, brutes = 0, chimney, decoys, i;
    uint32_t ev;
    use_dcat();
    /* the tables match HF sec.10.3 (retimed by SPEC R11) */
    CHECK_EQ(home_defend_nwaves(HDT_TUTORIAL), 1);
    CHECK_EQ(home_defend_wave_imps(HDT_TUTORIAL, 0), 3);
    CHECK_EQ(home_defend_nwaves(HDT_SIEGE), 3);
    for (w = 0; w < 3; w++) { imps += home_defend_wave_imps(HDT_SIEGE, w); brutes += home_defend_wave_brutes(HDT_SIEGE, w); }
    CHECK_EQ(imps, 6 + 10 + 14);
    CHECK_EQ(brutes, 0 + 1 + 2);
    CHECK_EQ(home_defend_wave_brutes(HDT_SIEGE, 1), 1);
    CHECK(home_defend_red_sky(HDT_TUTORIAL) && home_defend_red_sky(HDT_SIEGE));
    CHECK(!home_defend_red_sky(HDT_ARENA));
    for (w = 0; w < home_defend_nwaves(HDT_ARENA); w++)
        CHECK(home_defend_wave_imps(HDT_ARENA, w) + home_defend_wave_brutes(HDT_ARENA, w) <= 2 * HD_DEMONS);

    /* door HP = Barricade */
    shelter(&rm);
    home_score_room(&rm, HOME_ROOM_LIVING, HSC_CH3, &sc);
    home_defend_begin(&d, HDT_SIEGE, &rm, HOME_ROOM_LIVING);
    CHECK_EQ(d.door_max, sc.stat[HS_BARRICADE]);
    CHECK_EQ(d.door_hp, d.door_max);
    CHECK_EQ(d.door_max, 100);                /* 2 plates 50 + turret 40 + 8 + 20, capped */
    CHECK_EQ(d.plated, 3);
    CHECK_EQ(d.turret, 0);
    CHECK_EQ(d.ammo, 20);
    CHECK_EQ(d.mayor_win, 1);                 /* the decoy takes the other window */
    for (i = 0; i < HDG_COUNT; i++) CHECK(d.g[i].present && d.g[i].safe);
    CHECK(d.g[HDG_MAYOR].x == 0 && d.g[HDG_MAYOR].y == 4);
    home_room_template(HOME_ROOM_LIVING, &rm);
    home_defend_begin(&d, HDT_SIEGE, &rm, HOME_ROOM_LIVING);
    CHECK_EQ(d.door_max, 0);
    CHECK_EQ(d.turret, -1);

    /* a scripted wave 3 with no input loses exactly the Mayor's "safe"; the
     * chimney beat fires on wave 3; the decoy works once */
    shelter(&rm);
    home_defend_begin(&d, HDT_SIEGE, &rm, HOME_ROOM_LIVING);
    d.wave = 2;
    ev = run(&d, &chimney, &decoys);
    CHECK_EQ(d.phase, HDP_DONE);
    CHECK_EQ(chimney, 2);
    CHECK(ev & HDE_THAW);
    CHECK_EQ(decoys, 1);
    CHECK(ev & HDE_WINDOW_BREAK);
    CHECK(ev & HDE_ALARM);
    CHECK_EQ(d.alarm, 3);                     /* the third alarm... which holds */
    CHECK(!d.door_broken);
    CHECK_EQ(home_defend_safe(&d), 4);
    CHECK_EQ(d.g[HDG_MAYOR].safe, 0);
    for (i = 0; i < HDG_MAYOR; i++) CHECK_MSG(d.g[i].safe, "guest %d lost", i);
    CHECK_EQ(d.held, 3);
    CHECK(d.shots <= 20);                     /* nobody re-wound the toaster */
    done_dcat();
}

TEST(home_defend_input_matters)
{
    HomeRoom rm;
    static HomeDefend d, e;
    int chimney, decoys, guard = 0, rewinds = 0, repairs = 0;
    use_dcat();
    /* comforting the Mayor before the breach keeps him safe: 5 */
    shelter(&rm);
    home_defend_begin(&d, HDT_SIEGE, &rm, HOME_ROOM_LIVING);
    d.wave = 2;
    home_defend_tick(&d);
    CHECK(home_defend_can(&d, HDA_COMFORT, HDG_MAYOR));
    CHECK(home_defend_act(&d, HDA_COMFORT, HDG_MAYOR));
    CHECK(!home_defend_act(&d, HDA_COMFORT, HDG_PIP));   /* busy */
    run(&d, &chimney, &decoys);
    CHECK_EQ(home_defend_safe(&d), 5);
    /* a bare door (Barricade 0) falls to wave 1 and the imps come in */
    home_room_template(HOME_ROOM_LIVING, &rm);
    home_defend_begin(&d, HDT_SIEGE, &rm, HOME_ROOM_LIVING);
    while (home_defend_tick(&d) && d.wave == 0 && guard++ < 10000) { }
    CHECK(d.door_broken || home_defend_safe(&d) < 5);
    CHECK(home_defend_safe(&d) < 4);
    /* ... unless somebody keeps repairing it: the A spam keeps it shut */
    home_defend_begin(&e, HDT_SIEGE, &rm, HOME_ROOM_LIVING);
    guard = 0;
    while (home_defend_tick(&e) && e.wave == 0 && guard++ < 10000)
        if (home_defend_act(&e, HDA_REPAIR, 0)) repairs++;
    CHECK(repairs > 0);
    CHECK(!e.door_broken);
    CHECK_EQ(home_defend_safe(&e), 5);
    /* the turret runs dry after 20 shots unless re-wound */
    shelter(&rm);
    home_defend_begin(&d, HDT_SIEGE, &rm, HOME_ROOM_LIVING);
    guard = 0;
    while (home_defend_tick(&d) && guard++ < 30000)
        if (d.ammo == 0 && home_defend_act(&d, HDA_REWIND, 0)) rewinds++;
    CHECK(rewinds > 0);
    CHECK(d.shots > 20);
    CHECK(!d.door_broken);
    for (guard = 0; guard < HDG_MAYOR; guard++) CHECK(d.g[guard].safe);   /* the cast holds */
    done_dcat();
}

TEST(home_defend_determinism)
{
    HomeRoom rm;
    static HomeDefend a, b;
    int c1, c2, d1, d2;
    use_dcat();
    doomo_town.seed = 1234;
    shelter(&rm);
    home_defend_begin(&a, HDT_SIEGE, &rm, HOME_ROOM_LIVING);
    home_defend_begin(&b, HDT_SIEGE, &rm, HOME_ROOM_LIVING);
    run(&a, &c1, &d1);
    run(&b, &c2, &d2);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK_EQ(c1, c2);
    done_dcat();
}

TEST(home_defend_result_and_rules)
{
    HomeRoom rm;
    HomeEdit e;
    static HomeDefend d;
    HomeFit fit;
    HomeDefendMap m;
    int chimney, decoys, scrap0, i;
    use_dcat();
    home_move_in();
    for (i = 0; i < D_N; i++) { home_grant(i); home_grant(i); }
    /* the Shelter rules: plates only over a window slot, the turret only in
     * front of one */
    home_edit_begin(&e, HOME_ROOM_LIVING);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, D_PLATE, 2, 0, 0, -1, NULL), HPR_SLOT_ONLY);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, D_PLATE, 1, 0, 1, -1, NULL), HPR_SLOT_ONLY);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, D_PLATE, 1, 0, 0, -1, &fit), HPR_OK);
    CHECK_EQ(fit.z, e.room.obj[1].z);         /* level with the window */
    CHECK(home_edit_place(&e, D_PLATE, 1, 0, 0, -1, -1) >= 0);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, D_PLATE, 1, 0, 0, -1, NULL), HPR_OCCUPIED);
    CHECK(home_edit_place(&e, D_PLATE, 0, 4, 1, -1, -1) >= 0);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, D_TURRET, 3, 3, 0, -1, NULL), HPR_SLOT_ONLY);
    CHECK(home_edit_place(&e, D_TURRET, 0, 4, 0, -1, -1) >= 0);
    home_room_store(HOME_ROOM_LIVING, &e.room);
    /* the result reaches house_api.c through the cache */
    home_room_load(HOME_ROOM_LIVING, &rm);
    home_defend_begin(&d, HDT_SIEGE, &rm, HOME_ROOM_LIVING);
    CHECK_EQ(d.turret, 1);
    CHECK_EQ(d.mayor_win, 0);
    d.wave = 2;
    run(&d, &chimney, &decoys);
    home_defend_finish(&d);
    CHECK_EQ(home_defend_result(), home_defend_safe(&d));
    CHECK_EQ(home_state()->defence & 15, 3);
    /* the Arena's export of the house */
    CHECK_EQ(home_defend_export(&m), 4);
    CHECK(m.w == 6 && m.d == 6 && m.door_x == 4 && m.door_y == 0 && m.nwin == 2);
    CHECK_EQ(m.turret_win, 1);
    CHECK_EQ(m.plated, 3);
    CHECK(m.barricade >= 90);
    /* a won Night Raid drops scrap and counts */
    scrap0 = inv_count(ITEM_SCRAP);
    home_defend_begin(&d, HDT_RAID, &rm, HOME_ROOM_LIVING);
    run(&d, &chimney, &decoys);
    CHECK_EQ(chimney, -1);
    home_defend_finish(&d);
    CHECK(inv_count(ITEM_SCRAP) > scrap0);
    CHECK_EQ((story_stry()->nests >> 4) & 7, 1);
    /* the table a bare "home DEFEND" push gets */
    doomo_town.mode = TOWN_STORY;
    story_episode_set_done(BEAT_EP3_2, 0);
    CHECK_EQ(home_defend_table_for_story(), HDT_TUTORIAL);
    story_episode_set_done(BEAT_EP3_2, 1);
    CHECK_EQ(home_defend_table_for_story(), HDT_SIEGE);
    doomo_town.mode = TOWN_FREEPLAY;
    CHECK_EQ(home_defend_table_for_story(), HDT_ARENA);
    CHECK_EQ(HOME_ARG_STORY_MODE(HOME_ARG_DEFEND(HDT_RAID)), HSM_DEFEND);
    CHECK_EQ(HOME_ARG_STORY_PARAM(HOME_ARG_DEFEND(HDT_RAID)), HDT_RAID);
    CHECK(HOME_ARG_IS_STORY(HOME_ARG_DEFEND(HDT_ARENA)));
    done_dcat();
}
