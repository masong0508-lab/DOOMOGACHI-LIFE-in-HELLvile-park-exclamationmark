/* test_town.c - town lifecycle, relationship graph, inventory, graves,
 * clock, options and unlocks (owner: foundation). */
#include "test.h"

static int edges_sorted_valid(void)
{
    int i;
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        if (e->a >= e->b || e->b > DOOMO_PLAYER_ID) return 0;
        if ((e->kind & REL_KIND_MASK) >= REL_KIND_COUNT) return 0;
        if (e->value < -100 || e->value > 100) return 0;
        if (i && (((uint32_t)doomo_town.edges[i - 1].a << 16) | doomo_town.edges[i - 1].b) >=
                     (((uint32_t)e->a << 16) | e->b))
            return 0;
    }
    return 1;
}

TEST(town_new_populations)
{
    int i;
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);
    CHECK_EQ(town_count(-1), 0);
    CHECK_EQ(doomo_town.edge_count, 0);
    CHECK_EQ(doomo_town.money, DOOMO_START_MONEY);
    CHECK_EQ(doomo_town.player.flags & RF_PLAYER, RF_PLAYER);
    CHECK(town_res(DOOMO_PLAYER_ID) == &doomo_town.player);
    CHECK(town_res(-1) == NULL && town_res(DOOMO_PLAYER_ID + 1) == NULL);

    town_new(TOWN_STORY, 1, TOWN_POP_PREMADE);
    CHECK_EQ(doomo_town.mode, TOWN_STORY);
    CHECK_EQ(town_count(-1), DOOMO_MAX_RESIDENTS);
    CHECK_EQ(town_count_district(DISTRICT_HILLVILE, RES_ALIVE), 500);
    CHECK_EQ(town_count_district(DISTRICT_HELLVILE, -1), 500);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        DoomoResident base;
        town_premade_record(i, &base);
        resident_sanitize(&base);
        CHECK(memcmp(&base, &doomo_town.res[i], sizeof(base)) == 0);
    }
    CHECK(edges_sorted_valid());

    town_new(TOWN_FREEPLAY, 777, TOWN_POP_RANDOM);
    CHECK_EQ(town_count(-1), DOOMO_MAX_RESIDENTS);
    CHECK(doomo_town.edge_count > 500);
    CHECK(edges_sorted_valid());
    CHECK(doomo_town.res[700].flags & RF_HELLBORN);
    CHECK(!(doomo_town.res[3].flags & RF_HELLBORN));
    {   /* deterministic per seed */
        uint32_t fp = resident_fingerprint(&doomo_town.res[123]);
        int ec = doomo_town.edge_count;
        town_new(TOWN_FREEPLAY, 777, TOWN_POP_RANDOM);
        CHECK_EQ_U(resident_fingerprint(&doomo_town.res[123]), fp);
        CHECK_EQ(doomo_town.edge_count, ec);
        town_new(TOWN_FREEPLAY, 778, TOWN_POP_RANDOM);
        CHECK(resident_fingerprint(&doomo_town.res[123]) != fp);
    }
}

TEST(town_new_keeps_options_and_unlocks)
{
    opt_set(OPT_MUSIC_VOL, 3);
    unlock_grant(5);
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);
    CHECK_EQ(opt_get(OPT_MUSIC_VOL), 3);
    CHECK(unlock_has(5));
}

TEST(town_alloc_free_find)
{
    int id, i;
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);
    id = town_alloc(DISTRICT_HELLVILE);
    CHECK_EQ(id, DOOMO_DISTRICT_SIZE);
    resident_generate(town_res(id), 9);
    memcpy(town_res(id)->name, "Gloomy", 7);
    CHECK_EQ(town_alloc(DISTRICT_HELLVILE), DOOMO_DISTRICT_SIZE + 1);
    CHECK_EQ(town_find_by_name("Gloomy"), id);
    CHECK_EQ(town_find_by_name("gLOOMY"), id);
    CHECK_EQ(town_find_by_name("Nobody"), -1);
    CHECK(town_alive(id));
    rel_adjust(id, 3, 40);
    grave_add(id);
    town_free(id);
    CHECK_EQ(town_res(id)->status, RES_EMPTY);
    CHECK_EQ(rel_find(id, 3) == NULL, 1);
    CHECK_EQ(doomo_town.grave_count, 0);
    CHECK(!town_alive(id));
    /* fill a district */
    for (i = 0; i < DOOMO_DISTRICT_SIZE; i++) doomo_town.res[i].status = RES_ALIVE;
    CHECK_EQ(town_alloc(DISTRICT_HILLVILE), -1);
}

TEST(relationships_basic)
{
    DoomoEdge *e;
    uint16_t ids[8];
    int n;
    CHECK(rel_get_or_add(5, 5) == NULL);
    CHECK(rel_get_or_add(-1, 5) == NULL);
    e = rel_get_or_add(9, 4);
    CHECK(e && e->a == 4 && e->b == 9);
    CHECK(rel_find(4, 9) == rel_find(9, 4));
    CHECK_EQ(rel_kind(4, 9), REL_ACQUAINTANCE);
    CHECK_EQ(rel_kind(4, 10), -1);
    CHECK_EQ(rel_value(4, 10), 0);
    rel_adjust(4, 9, 30);
    CHECK_EQ(rel_kind(4, 9), REL_FRIEND);
    CHECK_EQ(progress_get(PROG_FRIENDSHIPS), 1);
    rel_adjust(4, 9, 50);
    CHECK_EQ(rel_value(4, 9), 80);
    CHECK_EQ(rel_kind(9, 4), REL_BEST_FRIEND);
    rel_adjust(4, 9, 100);
    CHECK_EQ(rel_value(4, 9), 100);
    rel_adjust(4, 9, -160);
    CHECK_EQ(rel_kind(4, 9), REL_RIVAL);
    rel_adjust(4, 9, -100);
    CHECK_EQ(rel_value(4, 9), -100);
    CHECK_EQ(rel_kind(4, 9), REL_NEMESIS);
    rel_set_kind(4, 9, REL_MARRIED);
    rel_adjust(4, 9, 50);
    CHECK_EQ(rel_kind(4, 9), REL_MARRIED);       /* special bonds stay */
    /* crushes are directional */
    rel_set_crush(9, 4, 1);
    CHECK(rel_has_crush(9, 4));
    CHECK(!rel_has_crush(4, 9));
    rel_set_crush(9, 4, 0);
    CHECK(!rel_has_crush(9, 4));
    /* cross-border friendship counter */
    rel_adjust(10, 600, 40);
    CHECK_EQ(progress_get(PROG_CROSS_BORDER_FRIENDS), 1);
    /* the player can have bonds too */
    rel_adjust(DOOMO_PLAYER_ID, 4, 20);
    CHECK_EQ(rel_value(4, DOOMO_PLAYER_ID), 20);
    /* rel_list sorted by value */
    rel_adjust(4, 1, -20);
    rel_adjust(4, 2, 60);
    n = rel_list(4, ids, 8);
    CHECK_EQ(n, 4);
    CHECK_EQ(ids[0], 2);                         /* 60 */
    CHECK_EQ(ids[1], DOOMO_PLAYER_ID);           /* 20 */
    CHECK_EQ(ids[2], 1);                         /* -20 */
    CHECK_EQ(ids[3], 9);                         /* -50 (married, but grumpy) */
    n = rel_list(4, ids, 2);
    CHECK_EQ(n, 2);
    CHECK_EQ(ids[0], 2);
    rel_remove_all(4);
    CHECK_EQ(rel_list(4, ids, 8), 0);
    CHECK(rel_find(10, 600) != NULL);
    CHECK(edges_sorted_valid());
}

TEST(relationships_evict_weakest_when_full)
{
    int i;
    DoomoRng r;
    rng_seed(&r, 3);
    /* fill the graph with weak bonds plus a few strong ones */
    for (i = 0; doomo_town.edge_count < DOOMO_MAX_EDGES; i++) {
        int a = rng_range(&r, 1000), b = rng_range(&r, 1000);
        DoomoEdge *e = rel_get_or_add(a, b);
        if (e) e->value = (int8_t)rng_between(&r, -40, 40);
    }
    rel_set_kind(1, 2, REL_MARRIED);           /* may evict one weak edge */
    rel_find(1, 2)->value = 1;
    CHECK_EQ(doomo_town.edge_count, DOOMO_MAX_EDGES);
    for (i = 0; i < 300; i++) {
        DoomoEdge *e = rel_get_or_add(990 + (i % 9), 100 + i);
        CHECK(e != NULL);
        e->value = 99;
        CHECK_EQ(doomo_town.edge_count, DOOMO_MAX_EDGES);
    }
    CHECK(rel_find(1, 2) != NULL);             /* marriage survived */
    for (i = 0; i < 300; i++) CHECK(rel_find(990 + (i % 9), 100 + i) != NULL);
    CHECK(edges_sorted_valid());
}

TEST(rel_normalize_cleans_junk)
{
    DoomoEdge *E = doomo_town.edges;
    E[0].a = 7; E[0].b = 3; E[0].value = 10; E[0].kind = REL_FRIEND | REL_CRUSH_AB;
    E[1].a = 5; E[1].b = 5; E[1].value = 0; E[1].kind = 0;          /* self */
    E[2].a = 3; E[2].b = 7; E[2].value = 20; E[2].kind = REL_RIVAL; /* dup */
    E[3].a = 1; E[3].b = 2000; E[3].value = 0; E[3].kind = 0;       /* bad id */
    E[4].a = 0; E[4].b = 1; E[4].value = -128; E[4].kind = 15;      /* bad kind */
    doomo_town.edge_count = 5;
    rel_normalize();
    CHECK_EQ(doomo_town.edge_count, 2);
    CHECK(edges_sorted_valid());
    CHECK(rel_find(3, 7) != NULL);
    CHECK_EQ(rel_find(0, 1)->value, -100);
    CHECK_EQ(rel_kind(0, 1), REL_ACQUAINTANCE);
}

TEST(inventory_graves_clock)
{
    int i;
    CHECK_EQ(inv_count(ITEM_CUPCAKE), 3);         /* starter kit */
    CHECK_EQ(inv_add(ITEM_CUPCAKE, 200), 99);
    CHECK(inv_take(ITEM_CUPCAKE, 9));
    CHECK_EQ(inv_count(ITEM_CUPCAKE), 90);
    CHECK(!inv_take(ITEM_CUPCAKE, 91));
    CHECK_EQ(inv_count(ITEM_CUPCAKE), 90);
    CHECK_EQ(inv_add(ITEM_NONE, 5), 0);
    CHECK_EQ(inv_add(ITEM_COUNT, 5), 0);
    CHECK(inv_total() >= 90);
    for (i = 0; i < DOOMO_MAX_GRAVES + 10; i++) grave_add(i);
    CHECK_EQ(doomo_town.grave_count, DOOMO_MAX_GRAVES);
    CHECK_EQ(doomo_town.graves[0].resident, 10);   /* oldest dropped */
    grave_add(20);                                  /* duplicate ignored */
    CHECK_EQ(doomo_town.grave_count, DOOMO_MAX_GRAVES);
    doomo_town.minute = 20 * 60 + 5;
    CHECK_EQ(town_hour(), 20);
    CHECK(!town_is_night());
    doomo_town.minute = 23 * 60;
    CHECK(town_is_night());
    doomo_town.minute = 5 * 60 + 59;
    CHECK(town_is_night());
    doomo_town.minute = 6 * 60;
    CHECK(!town_is_night());
    {
        char buf[16];
        doomo_town.minute = 20 * 60 + 5;
        town_time_str(buf, sizeof(buf));
        CHECK_STR(buf, "8:05 PM");
        opt_set(OPT_CLOCK_24H, 1);
        town_time_str(buf, sizeof(buf));
        CHECK_STR(buf, "20:05");
        doomo_town.minute = 0;
        opt_set(OPT_CLOCK_24H, 0);
        town_time_str(buf, sizeof(buf));
        CHECK_STR(buf, "12:00 AM");
    }
}

TEST(options_table_and_api)
{
    int i;
    CHECK(OPT_COUNT <= DOOMO_OPT_BYTES);
    for (i = 0; i < OPT_COUNT; i++) {
        const DoomoOptionInfo *o = &doomo_option_info[i];
        CHECK_MSG(o->name && o->name[0], "option %d unnamed", i);
        CHECK_MSG(o->count >= 2, "option %d has %d values", i, o->count);
        CHECK_MSG(o->def < o->count, "option %d default out of range", i);
        CHECK(o->page < OPTPAGE_COUNT);
        if (o->values) {
            int k;
            for (k = 0; k < o->count; k++) CHECK_MSG(o->values[k] && o->values[k][0], "opt %d value %d", i, k);
        }
        CHECK_EQ(opt_get(i), o->def);
    }
    opt_set(OPT_GORE, 99);
    CHECK_EQ(opt_get(OPT_GORE), doomo_option_info[OPT_GORE].count - 1);
    opt_set(OPT_GORE, -5);
    CHECK_EQ(opt_get(OPT_GORE), 0);
    CHECK_EQ(opt_get(-1), 0);
    CHECK_EQ(opt_get(OPT_COUNT), 0);
    doomo_town.opt[OPT_FOV] = 200;                 /* corrupt byte reads as default */
    CHECK_EQ(opt_get(OPT_FOV), doomo_option_info[OPT_FOV].def);
    CHECK_EQ(doomo_option_info[OPT_WINDOW_SKIN].count, WSKIN_COUNT);
    CHECK_EQ(opt_get(OPT_GORE), GORE_CUTE);
}

TEST(unlocks_registry)
{
    int i, j, n = unlock_count();
    CHECK(n >= 100);
    CHECK(n <= DOOMO_UNLOCK_BITS);
    for (i = 0; i < n; i++) {
        const DoomoUnlock *u = unlock_at(i);
        CHECK(u && u->name && u->how && u->how[0]);
        CHECK(u->id < DOOMO_UNLOCK_BITS);
        CHECK(unlock_by_id(u->id) == u);
        CHECK(u->cat <= UL_CAT_MISC);
        if (u->need) CHECK(u->prog < PROG_COUNT);
        for (j = i + 1; j < n; j++) CHECK_MSG(unlock_at(j)->id != u->id, "duplicate id %d", u->id);
    }
    /* foundation's ids stay in its range */
    for (i = 0; doomo_unlocks_foundation[i].name; i++) CHECK(doomo_unlocks_foundation[i].id < 256);
    CHECK(unlock_at(n) == NULL);
    CHECK(unlock_at(-1) == NULL);
}

TEST(unlocks_grant_and_progress)
{
    const DoomoUnlock *u = unlock_find(UL_CAT_PART, CLIST_HAIR, HAIR_FLAME);
    CHECK(u != NULL);
    CHECK(!unlock_option_ok(CLIST_HAIR, HAIR_FLAME));
    CHECK(unlock_option_ok(CLIST_HAIR, HAIR_BOB));            /* never locked */
    CHECK(unlock_option_ok(CLIST_HAIR, 63));
    progress_add(PROG_DEMONS_KILLED, 59);
    CHECK(!unlock_has(u->id));
    CHECK(ui_toast_pending() >= 1);              /* 10, 15, 20 ... 55 kills */
    ui_toast_clear();
    progress_add(PROG_DEMONS_KILLED, 1);
    CHECK(unlock_has(u->id));
    CHECK(unlock_option_ok(CLIST_HAIR, HAIR_FLAME));
    CHECK(ui_toast_pending() >= 1);
    CHECK_EQ(progress_get(PROG_DEMONS_KILLED), 60);
    CHECK(!unlock_grant(u->id));                               /* already owned */
    CHECK(!unlock_paint_ok(PAINT_HELLFIRE));
    CHECK(unlock_paint_ok(PAINT_ROSE));
    CHECK(unlock_mode_ok(MODE_ARENA));
    CHECK(unlock_grant_match(UL_CAT_PAINT, 0, PAINT_HELLFIRE));
    CHECK(unlock_paint_ok(PAINT_HELLFIRE));
    /* code-only unlocks are never granted by counters */
    progress_set(PROG_DEMONS_KILLED, 0xFFFFFFFFu);
    CHECK(!unlock_ok(UL_CAT_SKIN, OPT_TITLE_THEME, 5));
    progress_add(PROG_DEMONS_KILLED, 5);                       /* saturates */
    CHECK_EQ_U(progress_get(PROG_DEMONS_KILLED), 0xFFFFFFFFu);
    unlock_grant_all();
    CHECK_EQ(unlock_owned_count(), unlock_count());
    CHECK_EQ(unlock_check_all(), 0);
}
