/* test_home.c - the dream house core (owner: H1; HF sec.11.1, SPEC H1):
 * placement rules, the undo ring, the 'HOME' chunk round trip and sizes,
 * the save caps, the score with hand-computed answers, house_api and the
 * registries. The rules run on a hand-written catalog (home_test_catalog)
 * so the expected numbers never move when catalog.txt is retuned; the
 * real catalog gets its own validity test. */
#include "test.h"
#include "../src/home_int.h"

/* ---- a tiny catalog ------------------------------------------------------- */
enum { T_DOOR, T_WINDOW, T_HEARTH, T_TABLE, T_CHAIR, T_VASE, T_RUG, T_POSTER,
       T_BED, T_LAMP, T_PLUSH, T_PENTA, T_MIRROR, T_HELLCHAIR, T_FRIDGE, T_WINDOW2,
       T_STARTER, T_N };
#define ROW(name, cat, fl, fl2, fi, w, d, h, cute, spook, comfy, lr, theme, mass, role) \
    { name, "", 10, 0xFFFF, 0xFFFF, cat, fl, fl2, fi, w, d, h, 0xFF, 0xFF, cute, spook, comfy, \
      0xFF, 0, lr, theme, mass, 0, 0, 1, role }
static const DoomoFurn tcat[T_N] = {
    ROW("Door", FCAT_WALL, FF_WALL, FF_PORTAL, FI_NONE, 1, 1, 13, 0, 0, 0, 0, THEME_PASTEL, 0, FR_DOOR),
    ROW("Window", FCAT_WALL, FF_WALL, FF_PORTAL, FI_NONE, 1, 1, 6, 0, 0, 0, 0, THEME_PASTEL, 0, FR_WINDOW),
    ROW("Hearth", FCAT_PASTEL, FF_SURFACE, FF_PORTAL, FI_NONE, 2, 1, 12, 0, 0, 0, 0, THEME_COTTAGE, 0, FR_HEARTH),
    ROW("Table", FCAT_PASTEL, FF_SURFACE, 0, FI_NONE, 2, 1, 5, 2, 0, 3, 0, THEME_PASTEL, 10, FR_TABLE),
    ROW("Chair", FCAT_PASTEL, 0, 0, FI_SIT, 1, 1, 6, 2, 0, 4, 0, THEME_PASTEL, 6, FR_SEAT),
    ROW("Vase", FCAT_PASTEL, FF_SMALL, 0, FI_NONE, 1, 1, 3, 5, 0, 1, 0, THEME_PASTEL, 0, FR_PLANT),
    ROW("Rug", FCAT_RUG, FF_RUG, 0, FI_NONE, 2, 2, 0, 6, 0, 3, 0, THEME_PASTEL, 0, FR_NONE),
    ROW("Poster", FCAT_WALL, FF_WALL, 0, FI_NONE, 1, 1, 5, 6, 0, 1, 0, THEME_PASTEL, 0, FR_NONE),
    ROW("Bed", FCAT_PASTEL, FF_SURFACE, 0, FI_SLEEP, 1, 2, 5, 3, 0, 7, 0, THEME_PASTEL, 14, FR_BED),
    ROW("Lamp", FCAT_LIGHT, FF_LIGHT, 0, FI_NONE, 1, 1, 9, 4, 0, 2, 2, THEME_PASTEL, 2, FR_LAMP),
    ROW("Plush", FCAT_PLUSH, FF_SMALL, 0, FI_NONE, 1, 1, 3, 9, 0, 4, 0, THEME_PASTEL, 0, FR_PLUSH),
    ROW("Pentagram", FCAT_HELLISH, FF_RUG | FF_HELL, FF_CURSED, FI_NONE, 2, 2, 0, 2, 9, 1, 0, THEME_LAVA, 0, FR_PENTAGRAM),
    ROW("Mirror", FCAT_WALL, FF_WALL, 0, FI_EXAMINE, 1, 1, 6, 6, 0, 1, 0, THEME_PASTEL, 0, FR_MIRROR),
    ROW("Hell chair", FCAT_HELLISH, FF_HELL, 0, FI_SIT, 1, 1, 4, 3, 4, 3, 0, THEME_LAVA, 5, FR_SEAT),
    ROW("Fridge", FCAT_KITCHEN, 0, 0, FI_EAT, 1, 1, 13, 5, 0, 2, 0, THEME_CANDY, 18, FR_NONE),
    ROW("Round window", FCAT_WALL, FF_WALL, FF_PORTAL, FI_NONE, 1, 1, 6, 3, 0, 1, 0, THEME_CLOUD, 0, FR_WINDOW),
    ROW("Crate", FCAT_STORY, FF_SURFACE, FF_STARTER | FF_UNSELLABLE, FI_NONE, 1, 1, 5, 2, 0, 1, 0, THEME_COTTAGE, 8, FR_TABLE),
};

static void use_tcat(void)
{
    test_reset();
    home_test_catalog(tcat, T_N);
    home_reset();
}

static void done_tcat(void) { home_test_catalog(NULL, 0); }

static void own_all(int n)
{
    int f, k;
    for (f = 0; f < T_N; f++) for (k = 0; k < n; k++) home_grant(f);
}

/* ---- placement rules (HF sec.2.2) ----------------------------------------------- */
TEST(home_place_rules)
{
    HomeEdit e;
    HomeFit fit;
    int i, tbl, n0;
    use_tcat();
    home_move_in();
    own_all(4);
    home_edit_begin(&e, HOME_ROOM_LIVING);
    /* the template: door, two windows and the hearth, all fixed */
    CHECK_EQ(e.room.nobj, 4);
    CHECK_EQ(HR_W(&e.room), 6);
    CHECK_EQ(HR_D(&e.room), 6);
    for (i = 0; i < 4; i++) CHECK(home_obj_is_fixed(&e.room, i));
    n0 = e.room.nobj;
    /* a 2x1 table, then something on top of it, then something under it */
    tbl = home_edit_place(&e, T_TABLE, 2, 3, 0, -1, -1);
    CHECK_EQ(tbl, n0);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_CHAIR, 3, 3, 0, -1, NULL), HPR_OCCUPIED);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_TABLE, 5, 4, 0, -1, NULL), HPR_OFFGRID);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_TABLE, 5, 4, 1, -1, NULL), HPR_OK); /* 1x2 */
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_VASE, 3, 3, 0, -1, &fit), HPR_OK);
    CHECK(fit.on_surface && fit.surface == tbl && fit.z == tcat[T_TABLE].fh);
    CHECK(home_edit_place(&e, T_VASE, 3, 3, 0, -1, -1) >= 0);
    CHECK(HO_ONSURF(&e.room.obj[e.room.nobj - 1]));
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_PLUSH, 3, 3, 0, -1, NULL), HPR_OCCUPIED);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_PLUSH, 4, 4, 0, -1, NULL), HPR_OK); /* floor */
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_RUG, 2, 3, 0, -1, NULL), HPR_OK);   /* under */
    CHECK(home_edit_place(&e, T_RUG, 2, 3, 0, -1, -1) >= 0);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_PENTA, 3, 4, 0, -1, NULL), HPR_OCCUPIED); /* rug on rug */
    /* the table cannot be picked up with the vase on it; the vase can */
    CHECK_EQ(home_edit_pickup(&e, tbl), -HPR_OCCUPIED);
    /* wall pieces: only on a wall, never twice in the same spot */
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_POSTER, 2, 2, 0, -1, NULL), HPR_WALL_ONLY);
    CHECK(home_edit_place(&e, T_POSTER, 2, 0, 0, -1, -1) >= 0);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_MIRROR, 2, 0, 0, -1, NULL), HPR_OCCUPIED);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_MIRROR, 0, 4, 1, -1, NULL), HPR_OCCUPIED); /* the B window */
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_MIRROR, 0, 3, 1, -1, NULL), HPR_OK);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_POSTER, 6, 0, 0, -1, NULL), HPR_OFFGRID);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_POSTER, 0, 1, 1, -1, NULL), HPR_FIREPLACE);
    /* Kevin's rule, the door's tile, fixed pieces, window slots */
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_CHAIR, 1, 2, 0, -1, NULL), HPR_FIREPLACE);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_RUG, 1, 1, 0, -1, NULL), HPR_OK); /* rugs may */
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_CHAIR, 4, 0, 0, -1, NULL), HPR_DOOR);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_DOOR, 2, 0, 0, -1, NULL), HPR_FIXED);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_HEARTH, 3, 3, 0, -1, NULL), HPR_FIXED);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_WINDOW2, 3, 0, 0, -1, NULL), HPR_SLOT_ONLY);
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, T_WINDOW2, 1, 0, 0, -1, &fit), HPR_OK);
    CHECK(fit.replaces >= 0 && HO_FURN(&e.room.obj[(int)fit.replaces]) == T_WINDOW);
    for (i = 0; i < 4; i++) CHECK_EQ(home_edit_pickup(&e, i), -HPR_FIXED);
    /* ownership: 4 of each, none left after four tables */
    CHECK(home_edit_place(&e, T_TABLE, 4, 4, 0, -1, -1) >= 0);
    CHECK(home_edit_place(&e, T_TABLE, 2, 5, 0, -1, -1) >= 0);
    CHECK(home_edit_place(&e, T_TABLE, 2, 1, 1, -1, -1) >= 0);
    CHECK_EQ(home_edit_avail(&e, T_TABLE), 0);
    CHECK_EQ(home_edit_place(&e, T_TABLE, 4, 1, 1, -1, -1), -HPR_NOT_OWNED);
    /* remove-only mode (Interlude III) refuses to place, allows picking up */
    e.remove_only = 1;
    CHECK_EQ(home_edit_place(&e, T_CHAIR, 5, 5, 0, -1, -1), -HPR_REMOVE_ONLY);
    CHECK(home_edit_pickup(&e, e.room.nobj - 1) == T_TABLE);
    done_tcat();
}

/* ---- the undo ring (HF sec.9.3) ----------------------------------------------- */
TEST(home_undo_ring)
{
    HomeEdit e;
    HomeRoom start;
    DoomoRng r;
    int ops = 0, tries, undone = 0;
    use_tcat();
    home_move_in();
    own_all(15);
    home_edit_begin(&e, HOME_ROOM_KITCHEN);
    home_edit_place(&e, T_TABLE, 2, 2, 0, -1, -1);
    home_edit_place(&e, T_CHAIR, 5, 3, 0, -1, -1);
    e.undo_n = 0;
    start = e.room;
    rng_seed(&r, 7);
    for (tries = 0; ops < HOME_UNDO_MAX && tries < 5000; tries++) {
        int k = (int)(rng_next(&r) % 3), x = (int)(rng_next(&r) % 8), y = (int)(rng_next(&r) % 6);
        int rot = (int)(rng_next(&r) % 4), rc;
        if (k == 0) rc = home_edit_place(&e, 3 + (int)(rng_next(&r) % 12), x, y, rot, -1, -1);
        else if (k == 1 && e.room.nobj) rc = home_edit_pickup(&e, (int)(rng_next(&r) % e.room.nobj));
        else if (e.room.nobj) rc = home_edit_move(&e, (int)(rng_next(&r) % e.room.nobj), x, y, rot);
        else rc = -1;
        if (rc >= 0) ops++;
    }
    CHECK_EQ(ops, HOME_UNDO_MAX);
    while (home_edit_undo(&e)) undone++;
    CHECK_EQ(undone, HOME_UNDO_MAX);
    CHECK_EQ(e.room.nobj, start.nobj);
    CHECK(memcmp(start.obj, e.room.obj, start.nobj * sizeof(HomeObj)) == 0);
    CHECK(memcmp(&start, &e.room, 10) == 0);
    /* the ring keeps the newest 32 steps */
    for (ops = 0; ops < HOME_UNDO_MAX + 8; ops++) {
        int idx = home_edit_place(&e, T_VASE, 0, 0, 0, -1, -1);
        if (idx < 0) { idx = home_edit_place(&e, T_VASE, 7, 5, 0, -1, -1); }
        if (idx >= 0) home_edit_pickup(&e, idx); else home_edit_move(&e, 0, 2, 2, 0);
    }
    undone = 0;
    while (home_edit_undo(&e)) undone++;
    CHECK_EQ(undone, HOME_UNDO_MAX);
    done_tcat();
}

/* ---- the chunk (SPEC R9, R10, R13, C9) ------------------------------------------ */
static int chunk_roundtrip(uint8_t *buf, int *bytes)
{
    HomeState before;
    uint32_t n = home_chunk_write(buf, DOOMO_SAVE_EXT_MAX);
    *bytes = (int)n;
    before = *home_state();
    if ((int)n != home_serialize_size()) return 0;
    home_reset();
    if (!home_chunk_read(buf, n, HOME_CHUNK_VERSION)) return 0;
    return memcmp(&before, home_state(), sizeof(before)) == 0;
}

TEST(home_save_roundtrip)
{
    static uint8_t buf[DOOMO_SAVE_EXT_MAX];
    HomeState *s;
    int n, r, i;
    use_tcat();
    s = home_state();
    CHECK(s != NULL);
    /* nobody has moved in: the 10-byte form (R13) */
    CHECK_EQ(home_serialize_size(), HOME_CHUNK_EMPTY);
    CHECK(chunk_roundtrip(buf, &n));
    CHECK_EQ(n, HOME_CHUNK_EMPTY);
    /* moved in, nothing placed but the Living Room's fixed slots */
    home_move_in();
    CHECK(home_moved_in());
    CHECK(chunk_roundtrip(buf, &n));
    CHECK_EQ(n, HOME_SZ_HEADER + 8 * HOME_SZ_ROOM_HEADER + HOME_SZ_ROOM_HEADER + 4 * HOME_SZ_OBJ +
                HOME_SZ_OWNED + HOME_SZ_SEEN_NEW + HOME_SZ_GIFTS + HOME_SZ_VISITED + HOME_SZ_HAUNT +
                HOME_BLUEPRINTS * HOME_SZ_ROOM_HEADER + HOME_SZ_PHOTOS + HOME_SZ_WANTS +
                HOME_SZ_VISITOR_LOG + HOME_SZ_DECORATE_FOR + HOME_SZ_PENDING_GAG + HOME_SZ_CACHE);
    /* every room and both saved blueprints full, every photo taken: the v2
     * worst case, exactly */
    for (r = 0; r < HOME_ROOM_COUNT + HOME_BLUEPRINTS; r++) {
        HomeRoom *rm = r < HOME_ROOM_COUNT ? &s->room[r] : &s->bp[r - HOME_ROOM_COUNT];
        if (r < HOME_ROOM_COUNT) home_unlock_room(r);
        rm->nobj = HOME_OBJ_MAX;
        for (i = 0; i < HOME_OBJ_MAX; i++) {
            rm->obj[i].w0 = HO_W0((r * 40 + i) % FURN_MAX, i & 3, i & 1);
            rm->obj[i].pos = (uint8_t)(i * 37);
            rm->obj[i].z = (uint8_t)i;
            rm->obj[i].pa = (uint8_t)(i * 3);
            rm->obj[i].pb = (uint8_t)(i * 5);
        }
    }
    memset(s->owned, 0xFF, sizeof(s->owned));
    memset(s->visited, 0xA5, sizeof(s->visited));
    memset(s->photo, 0x5A, sizeof(s->photo));
    s->gag[3] = 0x1234;
    s->days_since_fire = 999;
    CHECK(chunk_roundtrip(buf, &n));
    CHECK_EQ(n, HOME_CHUNK_WORST);
    CHECK(n <= (int)home_ext.max_size && home_ext.max_size == HOME_CHUNK_WORST);
    /* malformed input is refused and leaves a clean house */
    CHECK(!home_chunk_read(buf, 50, HOME_CHUNK_VERSION));
    CHECK(!home_moved_in());
    CHECK(!home_chunk_read(buf, (uint32_t)n, 1));
    done_tcat();
}

TEST(home_save_caps)
{
    HomeState *s;
    int r, i;
    use_tcat();
    /* the computed bounds (SPEC C9): the measured towns + a worst-case HOME */
    CHECK(SAVE_BUDGET_TOWN_BOUND_STORY + (HOME_CHUNK_WORST * 129 + 127) / 128 + SAVE_BUDGET_CHUNK_HDR <
          SAVE_SLOT_STORY_CAP);
    CHECK(SAVE_BUDGET_TOWN_BOUND_FP + (HOME_CHUNK_WORST * 129 + 127) / 128 + SAVE_BUDGET_CHUNK_HDR <
          SAVE_SLOT_FP_CAP);
    /* and on the real codec: a full house in a story save, loaded back */
    doomo_town.mode = TOWN_STORY;
    home_move_in();
    s = home_state();
    for (r = 0; r < HOME_ROOM_COUNT; r++) {
        home_unlock_room(r);
        s->room[r].nobj = HOME_OBJ_MAX;
        for (i = 0; i < HOME_OBJ_MAX; i++) {
            s->room[r].obj[i].w0 = HO_W0((r * 13 + i * 7) % T_N, i & 3, 0);
            s->room[r].obj[i].pos = (uint8_t)(i * 29 + r);
        }
    }
    CHECK(save_game(0));
    CHECK(save_last_size() <= SAVE_SLOT_STORY_CAP);
    home_reset();
    CHECK(!home_moved_in());
    CHECK(load_game(0));
    CHECK(home_moved_in());
    CHECK_EQ(home_state()->room[HOME_ROOM_PARK_PLOT].nobj, HOME_OBJ_MAX);
    CHECK_EQ(home_state()->room[3].obj[5].pos, (uint8_t)(5 * 29 + 3));
    done_tcat();
}

/* ---- the score (HF sec.4) ---------------------------------------------------------- */
static void put(HomeRoom *rm, int furn, int x, int y, int rot, int surf)
{
    HomeObj *o = &rm->obj[rm->nobj++];
    o->w0 = HO_W0(furn, rot, surf);
    o->pos = HO_POS(x, y);
    o->z = surf ? 5 : 0;
    o->pa = o->pb = FURN_PAINT_DEFAULT;
}

TEST(home_score)
{
    HomeRoom rm;
    HomeScore sc;
    use_tcat();
    /* 1. the empty Living Room (door, 2 windows, hearth) by day */
    home_room_template(HOME_ROOM_LIVING, &rm);
    home_score_room(&rm, HOME_ROOM_LIVING, 0, &sc);
    CHECK_EQ(sc.pieces, 0);
    CHECK_EQ(sc.stat[HS_COMFORT], 0);
    CHECK_EQ(sc.stat[HS_LIGHT], 50);         /* daylight through the windows */
    CHECK_EQ(sc.stat[HS_STYLE], 0);
    CHECK(sc.rules & HRULE_FIREPLACE_CLEAR);
    CHECK_EQ(sc.stamps, 0);
    /* 2. a cosy bedroom at night: bed (3,2) rot 0 covers (3,2)(3,3); lamp at
     * (4,2) r2; two chairs; a plush on the bed; a table; a rug.
     * comfy 7+2+4+4+4+3+3 = 27, +10 bed, +4*2 seats, light: the lamp at
     * (4,2) r 2 covers 12 of 36 tiles = 33 -> /4 = 8; bed not facing the
     * door +5; lamp near the bed +5; plush on the bed +3 = 66. */
    home_room_template(HOME_ROOM_BEDROOM, &rm);
    put(&rm, T_BED, 3, 2, 0, 0);
    put(&rm, T_LAMP, 4, 2, 0, 0);
    put(&rm, T_CHAIR, 1, 4, 0, 0);
    put(&rm, T_CHAIR, 2, 4, 0, 0);
    put(&rm, T_PLUSH, 3, 3, 0, 1);
    put(&rm, T_TABLE, 4, 4, 0, 0);
    put(&rm, T_RUG, 0, 2, 0, 0);
    home_score_room(&rm, HOME_ROOM_BEDROOM, HSC_NIGHT, &sc);
    CHECK_EQ(sc.pieces, 7);
    CHECK_EQ(sc.beds, 1);
    CHECK_EQ(sc.seats, 2);
    CHECK_EQ(sc.lamps, 1);
    CHECK(sc.rules & HRULE_BED_NOT_DOOR);
    CHECK(sc.rules & HRULE_LAMP_NEAR_BED);
    CHECK(sc.rules & HRULE_PLUSH_ON_BED);
    CHECK_EQ(sc.stat[HS_LIGHT], 33);
    CHECK_EQ(sc.stat[HS_COMFORT], 66);
    CHECK_EQ(sc.stat[HS_STYLE], 100);        /* all pastel */
    CHECK_EQ(sc.stat[HS_SPOOK], 0);          /* 0 - 5 (lamp near the bed) */
    CHECK(sc.stamps & HOME_STAMP(HG_COSY));
    CHECK(!(sc.stamps & HOME_STAMP(HG_LIT)));
    /* 3. the pentagram under the bed: comfort -10 + its own 1, spook 9 + 15
     * cursed + 15 rule - 5 lamp = 34; style 7 of 8 pastel = 87 */
    put(&rm, T_PENTA, 2, 2, 0, 0);
    home_score_room(&rm, HOME_ROOM_BEDROOM, HSC_NIGHT, &sc);
    CHECK(sc.rules & HRULE_PENTAGRAM_BED);
    CHECK_EQ(sc.stat[HS_COMFORT], 66 - 10 + 1);
    CHECK_EQ(sc.stat[HS_SPOOK], 34);
    CHECK_EQ(sc.stat[HS_STYLE], 87);
    CHECK_EQ(sc.cursed, 1);
    /* 4. the bed facing the door (door on wall A, bed rot 2) */
    home_room_template(HOME_ROOM_BEDROOM, &rm);
    put(&rm, T_BED, 2, 2, 2, 0);
    home_score_room(&rm, HOME_ROOM_BEDROOM, 0, &sc);
    CHECK(sc.rules & HRULE_BED_FACES_DOOR);
    CHECK_EQ(sc.stat[HS_COMFORT], 7 + 10 - 5);
    /* 5. the bed walled in: unreachable from the door (-8) */
    home_room_template(HOME_ROOM_BEDROOM, &rm);
    put(&rm, T_BED, 0, 4, 0, 0);
    put(&rm, T_CHAIR, 0, 3, 0, 0);
    put(&rm, T_CHAIR, 1, 4, 0, 0);
    put(&rm, T_CHAIR, 1, 5, 0, 0);
    home_score_room(&rm, HOME_ROOM_BEDROOM, 0, &sc);
    CHECK(sc.rules & HRULE_BED_UNREACHABLE);
    CHECK_EQ(sc.stat[HS_COMFORT], 7 + 4 * 3 + 10 + 4 * 3 + 5 - 8);
    /* 6. Ch4 Harmony: 2 Hillvile + 2 Hellvile pieces side by side, two tables
     * pushed together: 100*2*2/4 = 100 -> capped; a fridge outside the
     * kitchen costs Style 3 (in Ch1-3 terms) */
    home_room_template(HOME_ROOM_LIVING, &rm);
    put(&rm, T_TABLE, 2, 3, 0, 0);
    put(&rm, T_TABLE, 2, 4, 0, 0);
    put(&rm, T_HELLCHAIR, 4, 3, 0, 0);
    put(&rm, T_HELLCHAIR, 4, 4, 0, 0);
    home_score_room(&rm, HOME_ROOM_LIVING, HSC_CH4, &sc);
    CHECK(sc.rules & HRULE_TABLES_TOGETHER);
    CHECK(sc.rules & HRULE_MIXED_PAIRS);
    CHECK_EQ(sc.stat[HS_STYLE], 100);
    put(&rm, T_FRIDGE, 5, 5, 0, 0);
    home_score_room(&rm, HOME_ROOM_LIVING, 0, &sc);
    CHECK(sc.rules & HRULE_KITCHEN_OUTSIDE);
    CHECK_EQ(sc.stat[HS_STYLE], 100 * 2 / 5 - 3);    /* pastel 2 of 5 is the top theme */
    /* 7. Ch3 Barricade: the table next to the door (mass 10) + a chair */
    home_room_template(HOME_ROOM_LIVING, &rm);
    put(&rm, T_TABLE, 4, 1, 0, 0);
    put(&rm, T_CHAIR, 5, 0, 0, 0);
    home_score_room(&rm, HOME_ROOM_LIVING, HSC_CH3, &sc);
    CHECK_EQ(sc.stat[HS_BARRICADE], 10 + 6);
    home_score_room(&rm, HOME_ROOM_LIVING, 0, &sc);
    CHECK_EQ(sc.stat[HS_BARRICADE], 0);
    done_tcat();
}

/* ---- house_api (resident) and the cache ----------------------------------------- */
TEST(home_api)
{
    HomeEdit e;
    use_tcat();
    CHECK(!home_moved_in());
    CHECK_EQ(home_has(T_CHAIR), 0);
    CHECK(home_grant(T_CHAIR));
    CHECK_EQ(home_has(T_CHAIR), 1);
    CHECK(home_state()->seen_new[T_CHAIR >> 3] & (1 << (T_CHAIR & 7)));
    CHECK(!home_grant(-1) && !home_grant(FURN_MAX));
    while (home_grant(T_CHAIR)) { }
    CHECK_EQ(home_has(T_CHAIR), 15);
    CHECK(home_prop_gag(7) && home_prop_gag(8) && home_prop_gag(9) && home_prop_gag(10));
    CHECK(!home_prop_gag(11));
    home_state()->days_since_fire = 12;
    CHECK_EQ(home_rivet_visits(), 12);
    CHECK_EQ(home_days_since_fire(), 0);
    CHECK_EQ(home_phase(), HP_OPEN);          /* free play */
    home_haunt_step(2);
    CHECK_EQ(home_state()->haunt[0], 2);
    /* moving in grants the starter kit and the first trophy */
    home_move_in();
    CHECK(home_moved_in());
    CHECK_EQ(home_has(T_STARTER), 1);
    CHECK(unlock_has(HUL_FIRST_HOUSE));
    /* a cosy room shows up in the cache, the getters and the trophies */
    own_all(2);
    home_edit_begin(&e, HOME_ROOM_LIVING);
    CHECK(home_edit_place(&e, T_BED, 3, 2, 0, -1, -1) >= 0);
    CHECK(home_edit_place(&e, T_LAMP, 4, 2, 0, -1, -1) >= 0);
    CHECK(home_edit_place(&e, T_CHAIR, 2, 4, 0, -1, -1) >= 0);
    CHECK(home_edit_place(&e, T_CHAIR, 3, 5, 0, -1, -1) >= 0);
    CHECK(home_edit_place(&e, T_RUG, 2, 2, 0, -1, -1) >= 0);
    CHECK(home_edit_place(&e, T_PLUSH, 3, 3, 0, -1, -1) >= 0);
    CHECK(home_edit_place(&e, T_TABLE, 4, 4, 0, -1, -1) >= 0);
    home_room_store(HOME_ROOM_LIVING, &e.room);
    home_refresh_cache(NULL, -1);
    CHECK_EQ(home_grade(HOME_ROOM_LIVING), HG_COSY);
    CHECK(home_goal_check(HG_COSY));
    CHECK(!home_goal_check(HG_HELD));
    CHECK(home_stat(HS_COMFORT) >= 40);
    CHECK(unlock_has(HUL_COSY));
    CHECK(home_state()->house_points > 0);
    done_tcat();
}

/* ---- registries (HF sec.11.1 home_registry) ---------------------------------------- */
TEST(home_registry)
{
    const DoomoSaveExt *e = *(const DoomoSaveExt *const volatile *)&doomo_save_ext_home;
    int i, n = 0;
    CHECK(e == &home_ext);
    CHECK(e->tag == HOME_CHUNK_TAG && e->version == HOME_CHUNK_VERSION);
    CHECK_EQ(e->max_size, HOME_CHUNK_WORST);
    for (i = 0; i < save_ext_count(); i++) if (save_ext_at(i) == e) n++;
    CHECK_EQ(n, 1);
    for (i = 0; doomo_unlocks_home[i].name; i++) {
        CHECK(doomo_unlocks_home[i].id >= UL_IDS_HOME_FIRST && doomo_unlocks_home[i].id < HUL_TROPHY_END);
        CHECK_EQ(doomo_unlocks_home[i].id, UL_IDS_HOME_FIRST + i);
        CHECK_EQ(doomo_unlocks_home[i].list, UL_MISC_HOME);
    }
    CHECK_EQ(i, 20);
    CHECK(HUL_RAW_LAST == UL_IDS_HOME_LAST && HUL_RAW_BLACK_MARKET <= HUL_RAW_LAST);
}

/* ---- the real catalog (once furngen has generated it) ---------------------------- */
TEST(home_catalog_valid)
{
    int i, n = doomo_furn_count, door = 0, window = 0, hearth = 0, starters = 0;
    if (n == 0) return;                      /* furngen (H4) not linked yet */
    CHECK(n >= 306 && n <= 330 && n <= FURN_MAX);
    for (i = 0; i < n; i++) {
        const DoomoFurn *f = &doomo_furn[i];
        CHECK_MSG(f->name && f->name[0] && strlen(f->name) <= 20, "row %d name", i);
        CHECK_MSG(f->desc && strlen(f->desc) <= 40, "row %d desc", i);
        CHECK_MSG(f->fw >= 1 && f->fw <= 4 && f->fd >= 1 && f->fd <= 4, "%s footprint", f->name);
        CHECK_MSG(f->cat < FCAT_COUNT && f->interact < FI_COUNT && f->role < FR_COUNT, "%s enums", f->name);
        CHECK_MSG(f->theme < THEME_COUNT, "%s theme", f->name);
        CHECK_MSG(!(f->flags & (FF_WALL | FF_SMALL | FF_RUG)) || f->mass == 0, "%s mass", f->name);
        CHECK_MSG(f->unlock == 0xFFFF || f->unlock < DOOMO_UNLOCK_BITS, "%s unlock", f->name);
        CHECK_MSG(f->giver == 0xFFFF || f->giver < DOOMO_MAX_RESIDENTS, "%s giver", f->name);
        CHECK_MSG(!(f->flags2 & FF_GIFT) || (f->flags2 & FF_UNSELLABLE), "%s gift sellable", f->name);
        CHECK_MSG(f->nprim >= 1 && f->frames >= 1, "%s has no recipe", f->name);
        CHECK_MSG(!(f->flags2 & FF_UNSELLABLE) || (doomo_furn_cap1[i >> 3] >> (i & 7) & 1), "%s cap", f->name);
        if (f->role == FR_DOOR) door++;
        if (f->role == FR_WINDOW) window++;
        if (f->role == FR_HEARTH) hearth++;
        if (f->flags2 & FF_STARTER) starters++;
    }
    CHECK(door >= 1 && window >= 1 && hearth >= 1 && starters >= 2);
    CHECK_EQ(home_furn_by_role(FR_DOOR), 0);     /* the first door is the default */
}

/* ---- H1 M3: themes, blueprints, photos, groups, half tiles, the haunting ------------ */
#include "../gen/furn_ids.h"

static void own_set(const DoomoHomeSet *b)
{
    int k;
    for (k = 0; k < b->n; k++) home_grant(doomo_home_pieces[b->first + k].furn);
}

TEST(home_blueprints_apply)
{
    static HomeEdit e;
    HomePlan plan;
    int b, k, nrom = doomo_home_blueprint_count;
    CHECK_EQ(nrom, 12);
    CHECK_EQ(home_blueprint_count(), 14);
    for (b = 0; b < nrom; b++) {
        const DoomoHomeSet *bp = &doomo_home_blueprints[b];
        test_reset();
        home_reset();
        home_move_in();
        home_unlock_room(bp->room);
        own_set(bp);
        home_edit_begin(&e, bp->room);
        home_edit_blueprint(&e, b, &plan);
        CHECK_MSG(plan.blocked == 0 && plan.missing == 0, "%s: %d blocked, %d missing (ghost %s at %d,%d)",
                  bp->name, plan.blocked, plan.missing, plan.nghost ? doomo_furn[plan.ghost[0].furn].name : "-",
                  plan.nghost ? plan.ghost[0].x : -1, plan.nghost ? plan.ghost[0].y : -1);
        CHECK_EQ(plan.placed, bp->n);
        CHECK(e.undo_n == 0);                 /* a fresh start */
        if (bp->theme != THEME_NONE) CHECK_EQ(e.room.wallpaper, doomo_home_themes[bp->theme].wallpaper);
        /* owning nothing: every piece becomes a ghost on the shopping list */
        home_reset();
        home_move_in();
        home_unlock_room(bp->room);
        for (k = 0; k < FURN_MAX; k++) home_state()->owned[k >> 1] = 0;
        home_edit_begin(&e, bp->room);
        home_edit_blueprint(&e, b, &plan);
        CHECK_EQ(plan.placed, 0);
        CHECK_EQ(plan.missing, bp->n);
        CHECK_EQ(plan.nghost, bp->n);
        CHECK(plan.ghost[0].status == HPS_MISSING);
    }
    CHECK(unlock_has(HUL_BLUEPRINT));
    /* a saved slot round-trips a room */
    test_reset();
    home_reset();
    home_move_in();
    own_set(&doomo_home_blueprints[1]);
    own_set(&doomo_home_blueprints[1]);
    home_edit_begin(&e, HOME_ROOM_LIVING);
    home_edit_blueprint(&e, 1, &plan);
    CHECK(home_blueprint_save(0, &e.room));
    CHECK(home_blueprint_name(nrom) != NULL);
    CHECK(home_blueprint_name(nrom + 1) == NULL);
    home_edit_begin(&e, HOME_ROOM_LIVING);
    CHECK_EQ(home_edit_blueprint(&e, nrom, &plan), doomo_home_blueprints[1].n);
    CHECK_EQ(plan.missing + plan.blocked, 0);
    CHECK_EQ(home_edit_blueprint(&e, nrom + 1, &plan), 0);     /* empty slot */
}

TEST(home_themes_undo)
{
    static HomeEdit e;
    HomeRoom before;
    int i, t, idx;
    test_reset();
    home_reset();
    home_move_in();
    for (t = 0; t < THEME_COUNT; t++) CHECK(doomo_home_themes[t].name && doomo_home_themes[t].name[0]);
    home_grant(FURN_CUPCAKE_LAMP);
    home_grant(FURN_BUNNY_BEANBAG);
    home_edit_begin(&e, HOME_ROOM_LIVING);
    CHECK(home_edit_place(&e, FURN_CUPCAKE_LAMP, 3, 3, 0, -1, -1) >= 0);
    idx = home_edit_place(&e, FURN_BUNNY_BEANBAG, 4, 4, 0, PAINT_CHERRY, -1);
    CHECK(idx >= 0);
    before = e.room;
    CHECK(home_edit_theme(&e, THEME_GOTH));
    CHECK_EQ(e.room.wallpaper, doomo_home_themes[THEME_GOTH].wallpaper);
    CHECK_EQ(e.room.wall_paint, doomo_home_themes[THEME_GOTH].wall_paint);
    CHECK_EQ(home_state()->theme, THEME_GOTH);
    /* the beanbag's slot A was the player's: kept; its slot B was free: themed */
    CHECK_EQ(e.room.obj[idx].pa & 0x1F, PAINT_CHERRY);
    CHECK_EQ(e.room.obj[idx].pb & 0x1F, doomo_home_themes[THEME_GOTH].paint_b);
    CHECK_EQ(e.room.obj[idx - 1].pa & 0x1F, doomo_home_themes[THEME_GOTH].paint_a);
    CHECK(home_edit_undo(&e));            /* one step */
    CHECK(memcmp(&before, &e.room, sizeof(before)) == 0);
    /* a recolour and a wallpaper change undo too */
    CHECK(home_edit_paint(&e, idx, PAINT_LIME, PAINT_SKY));
    CHECK(home_edit_walls(&e, 5, 2, PAINT_GRAPE, -1));
    CHECK(home_edit_undo(&e) && home_edit_undo(&e));
    CHECK(memcmp(&before, &e.room, sizeof(before)) == 0);
    for (i = 0; i < e.room.nobj; i++) CHECK(home_obj_is_fixed(&e.room, i) || i >= 4);
}

TEST(home_photo_recipe)
{
    static HomeEdit e;
    HomePhoto p, q;
    uint8_t buf[8];
    uint16_t h0;
    int i;
    test_reset();
    home_reset();
    home_move_in();
    memset(&p, 0, sizeof(p));
    p.place = 999; p.view = 3; p.zoom = 1; p.filter = 7; p.daylight = 5;
    p.sticker[0] = 1; p.sx[0] = 15; p.sy[0] = 7;
    p.sticker[1] = 2; p.sx[1] = 3; p.sy[1] = 1;
    p.sticker[2] = 3; p.sx[2] = 0; p.sy[2] = 4;
    p.hash = 0xBEEF;
    home_photo_pack(&p, buf);
    CHECK(home_photo_unpack(buf, &q));
    CHECK(memcmp(&p, &q, sizeof(p)) == 0);
    p.place = HOME_PHOTO_OWN + HOME_ROOM_PARK_PLOT;
    home_photo_pack(&p, buf);
    CHECK(home_photo_unpack(buf, &q) && q.place == HOME_PHOTO_OWN + HOME_ROOM_PARK_PLOT);
    memset(buf, 0, sizeof(buf));
    CHECK(!home_photo_unpack(buf, &q));
    /* the layout hash moves with the layout */
    home_grant(FURN_TEACUP_TABLE);
    home_edit_begin(&e, HOME_ROOM_LIVING);
    h0 = home_room_hash(&e.room);
    CHECK(home_edit_place(&e, FURN_TEACUP_TABLE, 3, 3, 0, -1, -1) >= 0);
    CHECK(home_room_hash(&e.room) != h0);
    home_edit_undo(&e);
    CHECK_EQ(home_room_hash(&e.room), h0);
    /* 16 slots, then full; the 10th photo is a trophy */
    for (i = 0; i < HOME_PHOTOS; i++) { p.hash = (uint16_t)i; CHECK_EQ(home_photo_add(&p), i); }
    CHECK_EQ(home_photo_add(&p), -1);
    CHECK_EQ(home_photo_count(), 16);
    CHECK(unlock_has(HUL_PHOTOS_10));
    home_photo_delete(5);
    CHECK(!home_photo_get(5, &q));
    CHECK(home_photo_get(6, &q) && q.hash == 6);
    CHECK_EQ(home_photo_add(&p), 5);
}

TEST(home_groups_and_half_tiles)
{
    static HomeEdit e;
    static HomeClip c;
    HomeRoom before;
    uint8_t sel[HOME_SEL_MAX];
    int n, tbl, v1, v2, i;
    test_reset();
    home_reset();
    home_move_in();
    for (i = 0; i < 3; i++) { home_grant(FURN_PINE_TABLE); home_grant(FURN_JELLYBEAN_JAR); home_grant(FURN_WOODEN_CHAIR); }
    home_edit_begin(&e, HOME_ROOM_LIVING);
    tbl = home_edit_place(&e, FURN_PINE_TABLE, 2, 3, 0, -1, -1);       /* 2x1: (2,3) (3,3) */
    CHECK(tbl >= 0);
    /* half tiles: two jars on one tile at different sub-positions */
    v1 = home_edit_place_sub(&e, FURN_JELLYBEAN_JAR, 2, 3, 0, 0, -1, -1);
    v2 = home_edit_place_sub(&e, FURN_JELLYBEAN_JAR, 2, 3, 0, 3, -1, -1);
    CHECK(v1 >= 0 && v2 >= 0);
    CHECK_EQ(HO_SUB(&e.room.obj[v2]), 3);
    CHECK_EQ(home_edit_place_sub(&e, FURN_JELLYBEAN_JAR, 2, 3, 0, 3, -1, -1), -HPR_OCCUPIED);
    CHECK(home_edit_place(&e, FURN_WOODEN_CHAIR, 2, 4, 2, -1, -1) >= 0);
    before = e.room;
    /* the band takes the table and both jars (and the chair) */
    n = home_edit_select(&e, 2, 3, 3, 4, sel, HOME_SEL_MAX);
    CHECK_EQ(n, 4);
    CHECK_EQ(sel[0], tbl);
    CHECK(home_edit_group_move(&e, sel, n, 1, 0) == n);
    CHECK_EQ(HO_X(&e.room.obj[tbl]), 3);
    CHECK_EQ(HO_X(&e.room.obj[v2]), 3);
    CHECK_EQ(HO_SUB(&e.room.obj[v2]), 3);
    CHECK(HO_ONSURF(&e.room.obj[v1]));
    CHECK(home_edit_group_move(&e, sel, n, 5, 0) < 0);                 /* off the grid */
    CHECK(home_edit_undo(&e));                                         /* one step */
    CHECK(memcmp(&before, &e.room, sizeof(before)) == 0);
    /* a quarter turn about the band's centre */
    CHECK(home_edit_group_rotate(&e, sel, n, 1) == n);
    CHECK_EQ(HO_ROT(&e.room.obj[tbl]), 1);
    CHECK(home_edit_undo(&e));
    CHECK(memcmp(&before, &e.room, sizeof(before)) == 0);
    /* copy, paste (only the spares you own), delete to storage */
    CHECK_EQ(home_edit_copy(&e, sel, n, &c), n);
    CHECK_EQ(home_edit_paste(&e, &c, 2, 0), 3);            /* one jar short: 3 owned, 2 placed */
    CHECK(home_edit_undo(&e));
    CHECK(memcmp(&before, &e.room, sizeof(before)) == 0);
    CHECK_EQ(home_edit_group_remove(&e, sel, n), n);
    CHECK_EQ(e.room.nobj, before.nobj - n);
    CHECK(home_edit_undo(&e));
    CHECK(memcmp(&before, &e.room, sizeof(before)) == 0);
    /* the sub-position survives the chunk */
    home_room_store(HOME_ROOM_LIVING, &e.room);
    {
        static uint8_t buf[DOOMO_SAVE_EXT_MAX];
        uint32_t len = home_chunk_write(buf, sizeof(buf));
        home_reset();
        CHECK(home_chunk_read(buf, len, HOME_CHUNK_VERSION));
        CHECK_EQ(HO_SUB(&home_state()->room[HOME_ROOM_LIVING].obj[v2]), 3);
    }
}

TEST(home_haunting_three_nights)
{
    static HomeEdit e, b;
    int crate, lamp, bed, r, i, eye = -1, penta = 0;
    test_reset();
    home_reset();
    home_move_in();
    home_unlock_room(HOME_ROOM_BEDROOM);
    home_grant(FURN_CUPCAKE_LAMP);
    home_grant(FURN_MARSHMALLOW_SOFA);
    home_grant(FURN_SINGLE_BED);
    home_edit_begin(&e, HOME_ROOM_LIVING);
    crate = home_edit_place(&e, FURN_PICNIC_CRATE, 3, 3, 0, -1, -1);
    lamp = home_edit_place(&e, FURN_CUPCAKE_LAMP, 5, 5, 0, -1, -1);
    CHECK(crate >= 0 && lamp >= 0);
    CHECK(home_edit_place(&e, FURN_MARSHMALLOW_SOFA, 2, 5, 0, -1, -1) >= 0);
    home_room_store(HOME_ROOM_LIVING, &e.room);
    home_edit_begin(&b, HOME_ROOM_BEDROOM);
    bed = home_edit_place(&b, FURN_SINGLE_BED, 3, 3, 0, -1, -1);
    CHECK(bed >= 0);
    home_room_store(HOME_ROOM_BEDROOM, &b.room);
    /* night 1: the crate is one tile to the left in the morning */
    home_haunt_step(1);
    home_edit_begin(&e, HOME_ROOM_LIVING);
    r = home_haunt_apply(&e.room, HOME_ROOM_LIVING);
    CHECK(r & HHA_CRATE);
    CHECK_EQ(HO_X(&e.room.obj[crate]), 2);
    CHECK_EQ(home_haunt_apply(&e.room, HOME_ROOM_LIVING), 0);       /* once */
    /* night 2: the lamp is off; one heart is an eye, and it stays */
    home_haunt_step(2);
    r = home_haunt_apply(&e.room, HOME_ROOM_LIVING);
    CHECK((r & HHA_LAMP) && (r & HHA_EYE));
    CHECK(e.room.obj[lamp].pa & HOS_OFF);
    for (i = 0; i < e.room.nobj; i++) if (HO_FURN(&e.room.obj[i]) == FURN_HEART_WALLPAPER_EYE) eye = i;
    CHECK(eye >= 0);
    CHECK(home_obj_is_fixed(&e.room, eye));
    CHECK_EQ(home_edit_pickup(&e, eye), -HPR_FIXED);
    CHECK(home_haunt_eye_blinks());
    CHECK_EQ(home_fit(&e.room, HOME_ROOM_LIVING, FURN_HEART_WALLPAPER_EYE, 2, 0, 0, -1, NULL), HPR_FIXED);
    /* night 3: two pieces move toward the hatch; the rug is under the bed */
    home_haunt_step(3);
    {
        HomeRoom before = e.room;
        int moved = 0;
        r = home_haunt_apply(&e.room, HOME_ROOM_LIVING);
        CHECK(r & HHA_PENTA);
        for (i = 0; i < before.nobj; i++) moved += before.obj[i].pos != e.room.obj[i].pos;
        CHECK(moved >= 1 && moved <= 3);   /* two pieces (and a small riding one) */
    }
    for (i = 0; i < home_state()->room[HOME_ROOM_BEDROOM].nobj; i++)
        if (HO_FURN(&home_state()->room[HOME_ROOM_BEDROOM].obj[i]) == FURN_PENTAGRAM_RUG) penta = 1;
    CHECK(penta);
    CHECK(home_owned(FURN_PENTAGRAM_RUG) >= 1);
    CHECK_EQ(home_state()->haunt[HH_APPLIED], 3);
    /* the eye survives the save */
    home_room_store(HOME_ROOM_LIVING, &e.room);
    {
        static uint8_t buf[DOOMO_SAVE_EXT_MAX];
        uint32_t len = home_chunk_write(buf, sizeof(buf));
        home_reset();
        CHECK(home_chunk_read(buf, len, HOME_CHUNK_VERSION));
        CHECK(home_state()->haunt[HH_FLAGS] & HHF_EYE);
        CHECK_EQ(HO_FURN(&home_state()->room[HOME_ROOM_LIVING].obj[eye]), FURN_HEART_WALLPAPER_EYE);
    }
    /* LIT ends it: a night after that changes nothing, the eye stops blinking */
    home_state()->cache[HC_STAMPS + HOME_ROOM_LIVING] |= HOME_STAMP(HG_LIT);
    CHECK(!home_haunt_eye_blinks());
    home_edit_begin(&e, HOME_ROOM_LIVING);
    CHECK_EQ(home_haunt_3am(&e.room, HOME_ROOM_LIVING), -1);
}

TEST(home_decorate_for_layouts)
{
    static HomeEdit e;
    const DoomoHomeSet *l = home_layout_find(1);
    int k, p_full, p_half;
    test_reset();
    home_reset();
    home_move_in();
    CHECK(l != NULL);
    CHECK_EQ(l->room, HOME_ROOM_ANNEX);
    CHECK(home_layout_find(15) == NULL);
    home_unlock_room(HOME_ROOM_ANNEX);
    for (k = 0; k < l->n; k++) {
        const DoomoHomePiece *q = &doomo_home_pieces[l->first + k];
        home_grant(q->furn & HOME_PIECE_ROLE ? FURN_CUPCAKE_LAMP : q->furn);
    }
    home_edit_begin(&e, HOME_ROOM_ANNEX);
    CHECK_EQ(home_layout_progress(&e.room, 1, -1), 0);
    for (k = 0; k < l->n; k++) {
        const DoomoHomePiece *q = &doomo_home_pieces[l->first + k];
        int furn = q->furn & HOME_PIECE_ROLE ? FURN_CUPCAKE_LAMP : q->furn;
        home_edit_place(&e, furn, q->x, q->y, q->rot, -1, -1);
        if (k == 2) {
            p_half = home_layout_progress(&e.room, 1, -1);
            CHECK(p_half > 0 && p_half < 8);
        }
    }
    p_full = home_layout_progress(&e.room, 1, -1);
    CHECK_EQ(p_full, 8);
    CHECK_EQ(home_decorate_for(1, 1), 8);           /* house_api serves the cache */
    CHECK_EQ(home_decorate_for(2, 1), 0);
}
