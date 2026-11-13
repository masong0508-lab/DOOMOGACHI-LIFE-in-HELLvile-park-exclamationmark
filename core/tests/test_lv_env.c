/* test_lv_env.c - LV4 (owner: levels): mechanic kit B, environment and light (core/src/levels_mech_env.c)
 * on the test map core/tests/maps/t4_env.txt (compiled by tools/mapc.py into the test binary only):
 * fog zones and valves, the tide, the lantern and candles, the two faces, showtime, collapsing
 * floors, dissolving rooms, the per-level view distance, and the raycaster with a short view at both
 * screen sizes. Every piece restores the exact original bytes when it ends. */
#include "test.h"
#include "doomo/lv_env.h"
#include "doomo/platform.h"

extern const DoomoMapExt map_ext_t4_env;

static DoomoMap g_map;
static LvEnv    g_env;

#define CELL(cx, cy) (g_map.cells[(cy) * g_map.w + (cx)])
#define DEF(cx, cy)  (g_map.def->cells[(cy) * g_map.w + (cx)])

static void load(int ambient)
{
    const DoomoMapDef *d = map_ext_t4_env.def;
    memset(&g_map, 0, sizeof(g_map));
    g_map.def = d;
    g_map.w = d->w; g_map.h = d->h; g_map.kind = d->kind; g_map.sky = d->sky;
    g_map.ambient = (uint8_t)ambient;
    memcpy(g_map.cells, d->cells, (size_t)d->w * d->h * sizeof(DoomoCell));
}

static int begin(int ambient)
{
    load(ambient);
    return lv_env_begin(&g_env, &g_map, &map_ext_t4_env);
}

static void run(int n, int cx, int cy) { while (n-- > 0) lv_env_tick(&g_env, cx, cy); }

/* every live cell equals the definition's (what a restore must give) */
static int pristine(void)
{
    return memcmp(g_map.cells, g_map.def->cells, (size_t)g_map.w * g_map.h * sizeof(DoomoCell)) == 0;
}

/* the cells of a rectangle equal the definition's */
static int rect_pristine(int x0, int y0, int x1, int y1)
{
    int cx, cy;
    for (cy = y0; cy <= y1; cy++)
        for (cx = x0; cx <= x1; cx++)
            if (memcmp(&CELL(cx, cy), &DEF(cx, cy), sizeof(DoomoCell)) != 0) return 0;
    return 1;
}

TEST(lvenv_view_distance_header)
{
    DoomoMapExt x = map_ext_t4_env;
    CHECK_EQ(lv_env_view(&map_ext_t4_env), 12);
    CHECK_EQ(lv_env_view(NULL), 0);
    x.view = 0;  CHECK_EQ(lv_env_view(&x), 0);          /* not set: the renderer's own */
    x.view = 5;  CHECK_EQ(lv_env_view(&x), LV_ENV_VIEW_MIN);
    x.view = 40; CHECK_EQ(lv_env_view(&x), 40);
}

TEST(lvenv_check_and_inert)
{
    DoomoMapExt x = map_ext_t4_env;
    DoomoZone z[6];
    DoomoSwapCell sc;
    DoomoSwapSet ss;
    int i;
    CHECK_EQ(lv_env_check(&map_ext_t4_env), LVENV_OK);
    CHECK_EQ(lv_env_check(NULL), LVENV_OK);
    /* five tide zones */
    for (i = 0; i < 6; i++) { memset(&z[i], 0, sizeof(z[i])); z[i].x0 = z[i].x1 = (uint8_t)(1 + i); z[i].y0 = z[i].y1 = 1; z[i].kind = ZONE_TIDE; }
    x.zones = z; x.zone_count = 5;
    CHECK_EQ(lv_env_check(&x), LVENV_TIDE_MANY);
    z[0].arg = 200; x.zone_count = 1;
    CHECK_EQ(lv_env_check(&x), LVENV_TIDE_ARG);
    for (i = 0; i < 9; i++) { z[i % 6].kind = ZONE_WARD; }
    x.zone_count = 6;
    CHECK_EQ(lv_env_check(&x), LVENV_OK);
    {   DoomoZone w[9];
        for (i = 0; i < 9; i++) { memset(&w[i], 0, sizeof(w[i])); w[i].x0 = w[i].x1 = (uint8_t)(1 + i); w[i].y0 = w[i].y1 = 1; w[i].kind = ZONE_COLLAPSE; }
        x.zones = w; x.zone_count = 9;
        CHECK_EQ(lv_env_check(&x), LVENV_DECAY_MANY);
    }
    /* a swap set that rewrites a cell a zone owns */
    x = map_ext_t4_env;
    memset(&sc, 0, sizeof(sc)); memset(&ss, 0, sizeof(ss));
    sc.cell = (uint16_t)(2 * g_map.w + 2);           /* (2,2) is in the fog zone */
    sc.cell = (uint16_t)(2 * map_ext_t4_env.def->w + 2);
    sc.x[5] = 3;                                     /* a light delta */
    ss.first = 0; ss.count = 1;
    x.swaps = &ss; x.swap_cells = &sc; x.swap_count = 1;
    CHECK_EQ(lv_env_check(&x), LVENV_SWAP_OWNED);
    sc.x[5] = 0; sc.x[1] = 1;                        /* only the floor texture: not owned */
    CHECK_EQ(lv_env_check(&x), LVENV_OK);
    /* a v1 map or a refused ext leaves the env inert */
    load(8);
    CHECK_EQ(lv_env_begin(&g_env, &g_map, NULL), LVENV_OK);
    CHECK_EQ(lv_env_tick(&g_env, 3, 3), 0);
    CHECK_EQ(lv_env_view_at(&g_env, 3, 3), 0);
    CHECK_EQ(lv_env_fog_set(&g_env, 30, 0), 0);
    CHECK(pristine());
}

TEST(lvenv_fog_and_valves)
{
    CHECK_EQ(begin(8), LVENV_OK);
    /* the fog zone starts dark: light 8 - 5 = 3 inside, the byte untouched outside */
    CHECK_EQ(CELL(2, 2).light, 3);
    CHECK_EQ(CELL(7, 8).light, 0);
    CHECK(lv_env_fog_at(&g_env, 2, 2));
    CHECK(!lv_env_fog_at(&g_env, 7, 8));
    CHECK_EQ(lv_env_view_at(&g_env, 2, 2), 9);          /* inside the fog: its distance */
    CHECK_EQ(lv_env_view_at(&g_env, 7, 8), 12);         /* outside: the level's */
    CHECK_EQ(lv_env_fog_set(&g_env, 0, 0), 0);          /* tag 0 is no valve */
    CHECK_EQ(lv_env_fog_set(&g_env, 31, 0), 0);         /* no such valve */
    CHECK_EQ(lv_env_fog_set(&g_env, 30, 0), 1);         /* the valve turns: the fog clears */
    CHECK_EQ(CELL(2, 2).light, 0);                      /* the exact original byte */
    CHECK(!lv_env_fog_at(&g_env, 2, 2));
    CHECK_EQ(lv_env_view_at(&g_env, 2, 2), 12);
    CHECK_EQ(lv_env_fog_set(&g_env, 30, 0), 0);         /* twice: nothing changes */
    CHECK_EQ(lv_env_fog_set(&g_env, 30, 1), 1);         /* and back */
    CHECK_EQ(CELL(2, 2).light, 3);
    /* the header's fog cuts the view, never under the minimum */
    {
        DoomoMapExt x = map_ext_t4_env;
        LvEnv e;
        x.fog = 4; x.view = 0;
        load(8);
        CHECK_EQ(lv_env_begin(&e, &g_map, &x), LVENV_OK);
        CHECK_EQ(lv_env_view_at(&e, 7, 8), LV_ENV_VIEW_MIN);
    }
}

TEST(lvenv_tide_rises_while_you_are_in_the_basin)
{
    int i;
    CHECK_EQ(begin(8), LVENV_OK);
    CHECK_EQ(lv_env_tide_level(&g_env, 40), -2);         /* the basin's bottom */
    CHECK_EQ(lv_env_tide_level(&g_env, 99), -128);
    run(LV_ENV_TIDE_TICKS * 3, 3, 7);                    /* elsewhere: nothing rises */
    CHECK_EQ(lv_env_tide_level(&g_env, 40), -2);
    run(LV_ENV_TIDE_TICKS - 1, 9, 2);
    CHECK_EQ(lv_env_tide_level(&g_env, 40), -2);
    run(1, 9, 2);                                        /* 6 s in the basin: one step */
    CHECK_EQ(lv_env_tide_level(&g_env, 40), -1);
    CHECK_EQ(CELL(9, 2).floor_h, -1);
    CHECK(CELL(9, 2).flags & CELL_WATER);                /* sludge slows */
    CHECK_EQ(CELL(7, 2).wall != 0, 1);                   /* the basin's wall is untouched */
    CHECK_EQ(CELL(3, 7).floor_h, DEF(3, 7).floor_h);
    run(LV_ENV_TIDE_TICKS * 2, 9, 2);
    CHECK_EQ(CELL(9, 2).floor_h, 1);
    /* leaving resets the count: a half step does not carry over */
    run(LV_ENV_TIDE_TICKS - 5, 9, 2);
    run(1, 3, 7);
    run(LV_ENV_TIDE_TICKS - 5, 9, 2);
    CHECK_EQ(lv_env_tide_level(&g_env, 40), 1);
    /* it stops at the high-water mark (arg 2) */
    for (i = 0; i < 10; i++) run(LV_ENV_TIDE_TICKS, 9, 2);
    CHECK_EQ(lv_env_tide_level(&g_env, 40), 2);
    CHECK_EQ(CELL(10, 3).floor_h, 2);
    /* the valve drains it for good: the exact bytes come back and it does not rise again */
    CHECK_EQ(lv_env_drain(&g_env, 0), 0);
    CHECK_EQ(lv_env_drain(&g_env, 40), 1);
    CHECK_EQ(CELL(9, 2).floor_h, -2);
    CHECK(!(CELL(9, 2).flags & CELL_WATER));
    for (i = 0; i < 3; i++) run(LV_ENV_TIDE_TICKS, 9, 2);
    CHECK_EQ(lv_env_tide_level(&g_env, 40), -2);
    CHECK_EQ(lv_env_drain(&g_env, 40), 0);
    run(2, 3, 7);
    CHECK_EQ(memcmp(&CELL(9, 2), &DEF(9, 2), sizeof(DoomoCell)), 0);
}

TEST(lvenv_tide_leaves_doors_and_low_ceilings)
{
    static DoomoCell cells[DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H];
    static DoomoMapDef def;
    DoomoMapExt x = map_ext_t4_env;
    LvEnv e;
    int i, j;
    /* a copy of the definition with a door cell and a low ceiling in the basin */
    def = *map_ext_t4_env.def;
    memcpy(cells, def.cells, (size_t)def.w * def.h * sizeof(DoomoCell));
    cells[2 * def.w + 9].flags |= CELL_DOOR;
    cells[3 * def.w + 10].ceil_h = 3;                    /* floor -2, ceiling 3: no room for the sludge to come up */
    def.cells = cells;
    x.def = &def;
    memset(&g_map, 0, sizeof(g_map));
    g_map.def = &def; g_map.w = def.w; g_map.h = def.h; g_map.kind = def.kind; g_map.sky = def.sky; g_map.ambient = 8;
    memcpy(g_map.cells, cells, (size_t)def.w * def.h * sizeof(DoomoCell));
    CHECK_EQ(lv_env_begin(&e, &g_map, &x), LVENV_OK);
    for (i = 0; i < 4; i++) for (j = 0; j < LV_ENV_TIDE_TICKS; j++) lv_env_tick(&e, 10, 2);
    CHECK_EQ(CELL(9, 2).floor_h, -2);                    /* movers belong to map.c */
    CHECK_EQ(CELL(10, 3).floor_h, -2);                   /* under a low ceiling the floor stays */
    CHECK_EQ(CELL(10, 2).floor_h, 2);                    /* the rest of the basin is full */
}

TEST(lvenv_lantern_lights_a_five_by_five_and_burns_oil)
{
    int n, i;
    CHECK_EQ(begin(3), LVENV_OK);                        /* a dark level: ambient 3 */
    CHECK(lv_env_lantern_on(&g_env));
    n = lv_env_tick(&g_env, 13, 8);
    CHECK(n > 0 && n <= 25);
    CHECK_EQ(CELL(13, 8).light, 10);                     /* the centre */
    CHECK_EQ(CELL(14, 8).light, 8);
    CHECK_EQ(CELL(15, 9).light, 6);                      /* two away, diagonal */
    CHECK_EQ(CELL(16, 8).light, 0);                      /* three away: untouched */
    /* a step: at most 25 bytes change, the cells left behind get their own byte back exactly */
    n = lv_env_tick(&g_env, 14, 8);
    CHECK(n <= 25);
    CHECK_EQ(CELL(14, 8).light, 10);
    CHECK_EQ(CELL(11, 8).light, DEF(11, 8).light);
    CHECK_EQ(CELL(12, 8).light, 6);
    /* running through the level never writes more than 25 cells' light a tick */
    for (i = 1; i < 22; i++) { n = lv_env_tick(&g_env, i, 4); CHECK(n <= 50); }
    /* the lantern lights from the ring's cell up only: it never darkens a lit cell */
    CHECK_EQ(begin(3), LVENV_OK);
    lv_env_tick(&g_env, 18, 8);                          /* beside the exit; cell (15,8) is the pillar */
    CHECK(CELL(16, 8).light >= DEF(16, 8).light);
    /* the oil runs out after 60 s: every byte is the original again; a jar fills it and it relights */
    CHECK_EQ(begin(3), LVENV_OK);
    run(LV_ENV_OIL_MAX - 1, 13, 8);
    CHECK_EQ(CELL(13, 8).light, 10);
    run(1, 13, 8);
    CHECK(!lv_env_lantern_on(&g_env));
    CHECK(rect_pristine(10, 6, 16, 9));
    run(5, 13, 8);
    CHECK(rect_pristine(10, 6, 16, 9));
    lv_env_refill(&g_env);
    CHECK(lv_env_lantern_on(&g_env));
    lv_env_tick(&g_env, 13, 8);
    CHECK_EQ(CELL(13, 8).light, 10);
    /* nobody on the map: the lantern goes out of the map (no player, no light) */
    lv_env_tick(&g_env, -1, -1);
    CHECK(rect_pristine(10, 6, 16, 9));
}

TEST(lvenv_lantern_is_for_dark_levels_only)
{
    DoomoMapExt x = map_ext_t4_env;
    LvEnv e;
    x.flags = 0;                                         /* not `dark yes` */
    load(3);
    CHECK_EQ(lv_env_begin(&e, &g_map, &x), LVENV_OK);
    lv_env_tick(&e, 13, 8);
    CHECK_EQ(CELL(13, 8).light, 0);
}

TEST(lvenv_candles_stay_lit)
{
    int i;
    CHECK_EQ(begin(3), LVENV_OK);
    CHECK_EQ(lv_env_candles_total(&g_env), 2);
    CHECK_EQ(lv_env_candles_lit(&g_env), 0);
    CHECK_EQ(lv_env_candle_use(&g_env, 18, 8), 0);       /* nothing near */
    CHECK_EQ(lv_env_candle_use(&g_env, 15, 2) > 0, 1);   /* the first candle */
    CHECK_EQ(CELL(14, 1).light, LV_ENV_CANDLE_LIGHT);    /* its 3 x 3 cells */
    CHECK_EQ(CELL(16, 3).light, LV_ENV_CANDLE_LIGHT);
    CHECK_EQ(CELL(17, 2).light, 0);                      /* not beside it */
    CHECK_EQ(CELL(15, 6).light, 0);                      /* the other candle is still out */
    CHECK_EQ(lv_env_candle_use(&g_env, 15, 2), 0);       /* lit already */
    CHECK_EQ(lv_env_candles_lit(&g_env), 1);
    CHECK_EQ(lv_env_candle_use(&g_env, 15, 4) > 0, 1);   /* from beside it (use range 1) */
    CHECK_EQ(CELL(15, 6).light, 11);                     /* this one burns at its own arg */
    CHECK_EQ(lv_env_candles_lit(&g_env), 2);
    /* they stay lit whatever the lantern does, and the lantern never dims them */
    for (i = 0; i < 40; i++) lv_env_tick(&g_env, 14 + (i % 3), 2);
    CHECK_EQ(CELL(14, 1).light, LV_ENV_CANDLE_LIGHT + (CELL(14, 1).light - LV_ENV_CANDLE_LIGHT));
    CHECK(CELL(14, 1).light >= LV_ENV_CANDLE_LIGHT);
    run(LV_ENV_OIL_MAX, 2, 7);
    CHECK_EQ(CELL(14, 1).light, LV_ENV_CANDLE_LIGHT);
    CHECK_EQ(CELL(15, 6).light, 11);
}

TEST(lvenv_two_faces)
{
    DoomoCell before;
    CHECK_EQ(begin(8), LVENV_OK);
    CHECK_EQ(lv_env_face_of(&g_env, 2, 9), 1);
    CHECK_EQ(lv_env_face_of(&g_env, 6, 9), 2);
    CHECK_EQ(lv_env_face_of(&g_env, 10, 6), 0);
    CHECK(!lv_env_out_of_phase(&g_env, 2, 9));           /* face A shows first */
    CHECK(lv_env_out_of_phase(&g_env, 6, 9));
    CHECK(!lv_env_out_of_phase(&g_env, 10, 6));          /* both faces: never */
    before = CELL(15, 8);
    CHECK(before.wall != 0);
    CHECK_EQ(lv_env_face_flip(&g_env, 5), 0);            /* no such set: nothing flips */
    CHECK_EQ(g_env.face, 0);
    CHECK_EQ(lv_env_face_flip(&g_env, 0), 1);            /* the lever: the Wall's other face */
    CHECK_EQ(g_env.face, 1);
    CHECK_EQ(CELL(15, 8).wall, 0);
    CHECK(lv_env_out_of_phase(&g_env, 2, 9));
    CHECK(!lv_env_out_of_phase(&g_env, 6, 9));
    CHECK_EQ(lv_env_face_flip(&g_env, 0), 1);            /* and back, exactly */
    CHECK_EQ(g_env.face, 0);
    CHECK_EQ(memcmp(&CELL(15, 8), &before, sizeof(before)), 0);
    CHECK(rect_pristine(10, 6, 16, 9));
}

TEST(lvenv_showtime_swaps_the_room_light)
{
    int i, saw_on = 0, on_ticks = 0;
    CHECK_EQ(begin(8), LVENV_OK);
    CHECK_EQ(CELL(3, 6).light, 3);                       /* between shows: the room's own dim */
    CHECK_EQ(CELL(8, 6).light, 0);                       /* another room: untouched */
    CHECK(!lv_env_showtime(&g_env));
    for (i = 0; i < LV_ENV_SHOW_TICKS - LV_ENV_SHOW_ON - 1; i++) lv_env_tick(&g_env, 14, 8);
    CHECK(!lv_env_showtime(&g_env));
    CHECK_EQ(CELL(3, 6).light, 3);
    lv_env_tick(&g_env, 14, 8);                          /* the lights blaze */
    CHECK(lv_env_showtime(&g_env));
    CHECK_EQ(CELL(3, 6).light, LV_ENV_SHOW_LIGHT);
    CHECK_EQ(CELL(8, 6).light, 0);
    for (i = 0; i < LV_ENV_SHOW_TICKS * 2; i++) {
        lv_env_tick(&g_env, 14, 8);
        if (lv_env_showtime(&g_env)) { saw_on = 1; on_ticks++; CHECK_EQ(CELL(3, 6).light, LV_ENV_SHOW_LIGHT); }
        else CHECK_EQ(CELL(3, 6).light, 3);
    }
    CHECK(saw_on);
    CHECK_EQ(on_ticks, LV_ENV_SHOW_ON * 2);                       /* two 10 s cycles: the window of each */
}

TEST(lvenv_a_floor_falls_after_you_leave)
{
    int i;
    CHECK_EQ(begin(8), LVENV_OK);
    run(200, 3, 7);                                      /* never entered: it never falls */
    CHECK_EQ(CELL(9, 6).floor_h, 0);
    run(5, 9, 6);                                        /* stand on it */
    run(300, 9, 6);
    CHECK_EQ(CELL(9, 6).floor_h, 0);                     /* standing there it holds */
    CHECK(!lv_env_gone_at(&g_env, 9, 6));
    run(LV_ENV_COLLAPSE_TICKS / 2, 3, 7);                /* step off, come back in time */
    run(1, 9, 6);
    run(LV_ENV_COLLAPSE_TICKS * 2, 9, 6);
    CHECK_EQ(CELL(9, 6).floor_h, 0);
    for (i = 0; i < LV_ENV_COLLAPSE_TICKS + LV_ENV_DECAY_DIV; i++) {
        CHECK_EQ(CELL(9, 6).floor_h, 0);                 /* it holds until the delay is up */
        lv_env_tick(&g_env, 3, 7);
        if (lv_env_gone_at(&g_env, 9, 6)) break;
    }
    CHECK(lv_env_gone_at(&g_env, 9, 6));
    CHECK(i >= LV_ENV_COLLAPSE_TICKS - LV_ENV_DECAY_DIV);
    CHECK_EQ(CELL(9, 6).floor_h, -20);                   /* into the ink */
    CHECK_EQ(CELL(8, 5).floor_h, -20);
    CHECK_EQ(CELL(12, 6).floor_h, DEF(12, 6).floor_h);   /* outside the zone */
    run(2000, 3, 7);
    CHECK_EQ(CELL(9, 6).floor_h, -20);                   /* it does not come back */
}

TEST(lvenv_unnamed_rooms_dissolve_and_names_restore_them)
{
    int i;
    CHECK_EQ(begin(8), LVENV_OK);
    run(5, 19, 2);                                       /* into ward 1 */
    run(LV_ENV_DISSOLVE_TICKS * 2, 19, 2);               /* standing in it: it holds */
    CHECK_EQ(CELL(19, 2).wall, 0);
    run(LV_ENV_DISSOLVE_TICKS - 20, 3, 7);               /* out, but not long enough */
    CHECK_EQ(CELL(19, 2).wall, 0);
    for (i = 0; i < 40; i++) lv_env_tick(&g_env, 3, 7);
    CHECK_EQ(CELL(19, 2).wall, LV_ENV_FOGWALL);          /* a fog wall now */
    CHECK_EQ(CELL(18, 1).wall, LV_ENV_FOGWALL);
    CHECK(lv_env_gone_at(&g_env, 19, 2));
    CHECK_EQ(CELL(19, 6).wall, 0);                       /* the other ward was never entered */
    CHECK_EQ(lv_env_name(&g_env, 9), 0);                 /* no such ward */
    CHECK_EQ(lv_env_name(&g_env, 1), 1);                 /* its name: it comes back, byte for byte */
    CHECK_EQ(memcmp(&CELL(19, 2), &DEF(19, 2), sizeof(DoomoCell)), 0);
    CHECK(!lv_env_gone_at(&g_env, 19, 2));
    CHECK_EQ(lv_env_name(&g_env, 1), 0);                 /* once */
    /* a named ward never dissolves, however long you are away */
    run(5, 19, 2);
    run(LV_ENV_DISSOLVE_TICKS * 3, 3, 7);
    CHECK_EQ(CELL(19, 2).wall, 0);
    /* naming before it dissolves: ward 2 */
    run(5, 19, 6);
    run(20, 3, 7);
    CHECK_EQ(lv_env_name(&g_env, 2), 1);
    run(LV_ENV_DISSOLVE_TICKS * 3, 3, 7);
    CHECK_EQ(CELL(19, 6).wall, 0);
    CHECK(rect_pristine(18, 1, 21, 7));
}

TEST(lvenv_mechanics_compose_and_restore_exactly)
{
    CHECK_EQ(begin(3), LVENV_OK);
    /* the lantern in the fog: 10 - 5 = 5 at the centre; the valve clears the fog and the lantern shows */
    lv_env_tick(&g_env, 3, 2);
    CHECK_EQ(CELL(3, 2).light, 5);
    CHECK_EQ(CELL(5, 2).light, 1);                       /* 6 - 5 */
    lv_env_fog_set(&g_env, 30, 0);
    CHECK_EQ(CELL(3, 2).light, 10);
    /* a candle inside a dark level plus the lantern: the brighter wins */
    lv_env_candle_use(&g_env, 15, 2);
    lv_env_tick(&g_env, 15, 2);
    CHECK_EQ(CELL(15, 2).light, 10);
    CHECK_EQ(CELL(14, 1).light, LV_ENV_CANDLE_LIGHT > 8 ? LV_ENV_CANDLE_LIGHT : 8);
    /* a dissolve, a tide and a collapse in one level do not disturb each other's cells */
    run(5, 19, 2); run(300, 3, 7);
    run(LV_ENV_TIDE_TICKS, 9, 2);
    CHECK_EQ(CELL(19, 2).wall, LV_ENV_FOGWALL);
    CHECK_EQ(CELL(9, 2).floor_h, -1);
    CHECK_EQ(CELL(9, 6).floor_h, 0);
}

static uint32_t drive(void)
{
    int i;
    if (begin(3) != LVENV_OK) return 0;
    for (i = 0; i < 900; i++) {
        lv_env_tick(&g_env, 1 + (i / 7) % 22, 1 + (i / 90) % 8);
        if (i == 100) lv_env_candle_use(&g_env, 15, 2);
        if (i == 300) lv_env_fog_set(&g_env, 30, 0);
        if (i == 500) lv_env_name(&g_env, 1);
        if (i == 700) lv_env_face_flip(&g_env, 0);
    }
    return lv_env_hash(&g_env) ^ (uint32_t)(g_map.cells[3].light * 31u) ^
           (uint32_t)(memcmp(g_map.cells, g_map.def->cells, sizeof(DoomoCell) * 240) ? 1 : 2);
}

TEST(lvenv_deterministic)
{
    uint32_t a = drive(), b = drive(), h;
    CHECK_EQ_U(a, b);
    CHECK_EQ(begin(3), LVENV_OK);
    h = lv_env_hash(&g_env);
    g_env.oil--;
    CHECK(lv_env_hash(&g_env) != h);
    g_env.oil++;
    g_env.tide[1]++;
    CHECK(lv_env_hash(&g_env) != h);
    g_env.tide[1]--;
    CHECK_EQ_U(lv_env_hash(&g_env), h);
}

/* ---- the renderers: a short view distance leaves no holes at either screen size ------------------ */
static int count_color(int w, int h, uint8_t c)
{
    int x, y, n = 0;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            if (nat_fb[y * nat_w + x] == c) n++;
    return n;
}

static uint32_t fb_hash(int w, int h)
{
    uint32_t a = 2166136261u;
    int x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) { a ^= nat_fb[y * nat_w + x]; a *= 16777619u; }
    return a;
}

TEST(lvenv_short_view_distance_renders_without_holes)
{
    static const int sizes[2][2] = { { 240, 160 }, { 320, 240 } };
    static const int views[5] = { 0, 8, 12, 16, 40 };
    int s, k, a;
    for (s = 0; s < 2; s++) {
        NatOptions o;
        uint32_t h_default = 0, h_short = 0;
        memset(&o, 0, sizeof(o));
        o.w = sizes[s][0]; o.h = sizes[s][1]; o.seed = 1; o.quiet = 1; o.save_dir = test_tmp_dir;
        nat_init(&o);
        load(8);
        for (k = 0; k < 5; k++) {
            for (a = 0; a < 3; a++) {
                DoomoView3D v;
                memset(&v, 0, sizeof(v));
                v.map = &g_map;
                v.x = INT2FIX(1 * 64 + 32); v.y = INT2FIX(4 * 64 + 32);
                v.z = INT2FIX(map_floor_z(&g_map, 1, 4) + DOOMO_EYE_HEIGHT);
                v.angle = (int16_t)(a == 0 ? 0 : a == 1 ? 300 : 3800);          /* down the long hall, a bit either way */
                v.vw = (int16_t)o.w; v.vh = (int16_t)o.h;
                v.player_sprite = -1;
                v.view_dist = (uint8_t)views[k];
                plat_clear(PAL_UI_RED);
                plat_view3d(&v);
                CHECK_MSG(count_color(o.w, o.h, PAL_UI_RED) == 0, "%dx%d view %d angle %d left holes", o.w, o.h, views[k], a);
                if (a == 0 && k == 0) h_default = fb_hash(o.w, o.h);
                if (a == 0 && k == 1) h_short = fb_hash(o.w, o.h);
                if (a == 0 && k == 4) CHECK(fb_hash(o.w, o.h) != h_short);
            }
        }
        CHECK(h_default != h_short);                   /* the variable does something ... */
    }
    {   /* ... and the old call (view_dist 0) is the same picture as a view struct that never heard of it */
        NatOptions o;
        DoomoView3D v;
        uint32_t h1, h2;
        memset(&o, 0, sizeof(o));
        o.w = 240; o.h = 160; o.seed = 1; o.quiet = 1; o.save_dir = test_tmp_dir;
        nat_init(&o);
        load(8);
        memset(&v, 0, sizeof(v));
        v.map = &g_map; v.x = INT2FIX(96); v.y = INT2FIX(288); v.z = INT2FIX(DOOMO_EYE_HEIGHT);
        v.vw = 240; v.vh = 160; v.player_sprite = -1;
        plat_view3d(&v);
        h1 = fb_hash(240, 160);
        v.view_dist = 200;                             /* out of range: the renderer's own */
        plat_view3d(&v);
        h2 = fb_hash(240, 160);
        CHECK_EQ_U(h1, h2);
        v.view_dist = 3;                               /* under the minimum: the renderer's own */
        plat_view3d(&v);
        CHECK_EQ_U(fb_hash(240, 160), h1);
    }
}
