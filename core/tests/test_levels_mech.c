/* test_levels_mech.c - LV3 (owner: levels): mechanic kit A, movement (core/src/levels_mech_move.c).
 * A test per mechanic, then a scripted bot that plans over the walk edges of core/tools/map_bfs.h plus
 * mm_edges and drives the real state machine across the kit test map core/tests/maps/t3_kit.txt
 * (compiled by tools/mapc.py): zipline, ride cycle, bounce arc, low gravity, tube and raft in a line. */
#include "test.h"
#include "../src/levels_mech_move.h"
#include "../tools/map_bfs.h"

extern const DoomoMapDef map_def_t3_kit, map_def_t2_gravity, map_def_t2_bounce;
extern const DoomoMapExt map_ext_t3_kit, map_ext_t2_gravity, map_ext_t2_bounce;

#define KX (&map_ext_t3_kit)

/* ---- paths and gravity ----------------------------------------------------- */
TEST(mech_path_length_and_position)
{
    int x, y;
    const DoomoPath *p = map_path_by_id(KX, 1);                   /* zip 3,3 -> 6,3: 3 cells */
    CHECK(p != NULL);
    CHECK_EQ(mm_path_len(KX, p), 3 * MM_U);
    CHECK_EQ(mm_path_index(KX, p), 0);
    mm_path_pos(KX, p, 0, &x, &y);        CHECK_EQ(x, 3 * 256); CHECK_EQ(y, 3 * 256);
    mm_path_pos(KX, p, 24, &x, &y);       CHECK_EQ(x, 4 * 256 + 128); CHECK_EQ(y, 3 * 256);
    mm_path_pos(KX, p, 9999, &x, &y);     CHECK_EQ(x, 6 * 256);          /* past the end: the last point */
    p = map_path_by_id(KX, 2);                                    /* ride 8,3 10,3 13,3: 5 cells */
    CHECK_EQ(mm_path_len(KX, p), 5 * MM_U);
    mm_path_pos(KX, p, 2 * MM_U, &x, &y); CHECK_EQ(x, 10 * 256);          /* the island */
    CHECK_EQ(mm_path_len(NULL, p), 0);
    CHECK_EQ(mm_path_index(KX, NULL), -1);
}

TEST(mech_gravity_matches_the_bfs)
{
    /* the one rule of how high a step is: mm_step_rise is map_bfs_rise on every cell of three maps */
    static MapBfs b;
    static MapBfsResult r;
    const DoomoMapDef *defs[3] = { &map_def_t3_kit, &map_def_t2_gravity, &map_def_t2_bounce };
    const DoomoMapExt *exts[3] = { &map_ext_t3_kit, &map_ext_t2_gravity, &map_ext_t2_bounce };
    int i, c, n = 0;
    for (i = 0; i < 3; i++) {
        map_bfs_run(&b, defs[i], exts[i], NULL, 1, 1, &r);
        for (c = 0; c < defs[i]->w * defs[i]->h; c++) {
            CHECK_EQ(mm_step_rise(exts[i], c % defs[i]->w, c / defs[i]->w), map_bfs_rise(&b, c));
            n++;
        }
    }
    CHECK(n > 100);
    CHECK_EQ(mm_gravity_at(KX, 20, 3), 50);
    CHECK_EQ(mm_gravity_at(KX, 2, 3), 100);
    CHECK_EQ(mm_step_rise(KX, 20, 3), 48);          /* 24 * 100 / 50 */
    CHECK_EQ(mm_step_rise(KX, 2, 3), DOOMO_STEP_UP);
    CHECK_EQ(mm_step_rise(KX, 15, 3), 80);          /* the pad: 10 steps */
    CHECK_EQ(mm_step_rise(NULL, 15, 3), DOOMO_STEP_UP);
    CHECK_EQ(mm_gravity_at(NULL, 1, 1), 100);
}

TEST(mech_arc_height_is_a_parabola)
{
    CHECK_EQ(mm_arc_height(10, 18, 0), 0);
    CHECK_EQ(mm_arc_height(10, 18, 18), 0);
    CHECK_EQ(mm_arc_height(10, 18, 9), 80);          /* the peak is the rise */
    CHECK(mm_arc_height(10, 18, 4) == mm_arc_height(10, 18, 14));
    CHECK(mm_arc_height(10, 18, 4) < 80 && mm_arc_height(10, 18, 4) > 0);
    CHECK_EQ(mm_arc_height(0, 18, 9), 0);
    CHECK_EQ(mm_arc_height(10, 0, 9), 0);
    CHECK_EQ(mm_arc_height(10, 18, 99), 0);          /* clamped */
}

/* ---- ride cycles ------------------------------------------------------------ */
TEST(mech_ride_runs_out_and_back_and_a_booth_stops_it)
{
    const DoomoPath *p = map_path_by_id(KX, 2);
    MmMove m;
    int x, y, t, len = mm_path_len(KX, p);
    mm_reset(&m);
    mm_ride_platform(KX, p, 0, 0, &x, &y);          CHECK_EQ(x, 8 * 256);
    mm_ride_platform(KX, p, len, 0, &x, &y);        CHECK_EQ(x, 13 * 256);          /* the far end */
    mm_ride_platform(KX, p, 2 * len, 0, &x, &y);    CHECK_EQ(x, 8 * 256);           /* and back */
    mm_ride_platform(KX, p, len / 2, 2, &x, &y);    CHECK_EQ(x, 13 * 256);          /* twice as fast */
    for (t = 0; t < 10; t++) mm_ride_tick(&m);
    CHECK_EQ(m.clock, 10);
    mm_ride_stop(&m, 1);
    for (t = 0; t < 10; t++) mm_ride_tick(&m);
    CHECK_EQ(m.clock, 10);                          /* stopped */
    mm_ride_stop(&m, 0);
    mm_ride_tick(&m);
    CHECK_EQ(m.clock, 11);
}

TEST(mech_ride_boards_only_where_the_platform_is)
{
    MmMove m;
    int x, y, h, t;
    mm_reset(&m);
    m.clock = (uint16_t)(2 * MM_U);                 /* platform at the island (10,3) */
    CHECK_EQ(mm_ride_board(KX, &m, 8, 3), -1);
    CHECK_EQ(m.mode, MM_ON_FOOT);
    CHECK_EQ(mm_ride_board(KX, &m, 10, 3), 1);
    CHECK_EQ(m.mode, MM_RIDE);
    CHECK_EQ(mm_ride_board(KX, &m, 10, 3), -1);     /* already riding */
    /* the platform moves the player; the clock is the caller's */
    mm_ride_tick(&m);
    CHECK_EQ(mm_tick(KX, &m, 0, 0, &x, &y, &h), 1);
    CHECK(x > 10 * 256 - 1 && x < 10 * 256 + 64);
    CHECK_EQ(h, 0);
    for (t = 0; t < 1000; t++) mm_ride_tick(&m);
    CHECK_EQ(mm_tick(KX, &m, 0, 1, &x, &y, &h), 0);  /* off */
    CHECK_EQ(m.mode, MM_ON_FOOT);
    CHECK(x >= 8 * 256 && x <= 13 * 256);
}

/* ---- ziplines ---------------------------------------------------------------- */
TEST(mech_zip_slides_first_to_last_point)
{
    MmMove m;
    int x, y, h, n = 0, r;
    mm_reset(&m);
    CHECK_EQ(mm_zip_board(KX, &m, 4, 3), -1);       /* not at the anchor */
    CHECK_EQ(mm_zip_board(KX, &m, 6, 3), -1);       /* the far end is not an anchor: one way */
    CHECK_EQ(mm_zip_board(KX, &m, 3, 3), 0);
    CHECK_EQ(m.mode, MM_ZIP);
    while ((r = mm_tick(KX, &m, 0, 0, &x, &y, &h)) == 1 && n < 200) { n++; CHECK(h > 0); }
    CHECK_EQ(r, 0);
    CHECK_EQ(n, 3 * MM_U / MM_ZIP_SPEED - 1);       /* 16 ticks, the 16th lands */
    CHECK_EQ(x, 6 * 256); CHECK_EQ(y, 3 * 256); CHECK_EQ(h, 0);
    CHECK_EQ(m.mode, MM_ON_FOOT);
    /* dropping off early lands where the line is */
    mm_zip_board(KX, &m, 3, 3);
    mm_tick(KX, &m, 0, 0, &x, &y, &h);
    mm_tick(KX, &m, 0, 0, &x, &y, &h);
    CHECK_EQ(mm_tick(KX, &m, 0, 1, &x, &y, &h), 0);
    CHECK(x > 3 * 256 && x < 6 * 256);
    CHECK_EQ(m.mode, MM_ON_FOOT);
}

/* ---- bounce pads ------------------------------------------------------------- */
TEST(mech_bounce_arc_lands_on_the_far_ledge)
{
    MmMove m;
    int x, y, h, n = 0, r, top = 0;
    mm_reset(&m);
    CHECK_EQ(mm_bounce_start(KX, &m, 14, 3), 0);    /* not on the pad */
    CHECK_EQ(mm_bounce_start(KX, &m, 15, 3), 1);
    CHECK_EQ(m.mode, MM_BOUNCE);
    CHECK_EQ(m.path, 2);                            /* the arc that starts on the pad */
    CHECK_EQ(m.air, MM_BOUNCE_AIR);
    while ((r = mm_tick(KX, &m, 0, 0, &x, &y, &h)) == 1 && n < 100) { n++; if (h > top) top = h; }
    CHECK_EQ(r, 0);
    CHECK_EQ(n, MM_BOUNCE_AIR - 1);
    CHECK(top >= 78 && top <= 80);
    CHECK_EQ(x, 17 * 256); CHECK_EQ(y, 3 * 256); CHECK_EQ(h, 0);
}

TEST(mech_bounce_without_an_arc_hops_in_place_and_low_gravity_hangs_longer)
{
    MmMove m;
    int x, y, h, n = 0;
    mm_reset(&m);
    CHECK_EQ(mm_bounce_start(&map_ext_t2_bounce, &m, 3, 1), 1);
    CHECK_EQ(m.path, 255);
    while (mm_tick(&map_ext_t2_bounce, &m, 0, 0, &x, &y, &h) == 1 && n < 100) { n++; CHECK_EQ(x, -1); }   /* stays put */
    CHECK_EQ(n, MM_BOUNCE_AIR - 1);
    /* a pad in a half-gravity zone: twice the air time (the t2_gravity map has no pad: build the case by hand) */
    {
        static const DoomoZone zs[2] = { { 1, 1, 1, 1, ZONE_BOUNCE, 4, 0 }, { 0, 0, 3, 3, ZONE_GRAVITY, 50, 0 } };
        DoomoMapExt e;
        memset(&e, 0, sizeof e);
        e.zone_count = 2; e.zones = zs;
        mm_reset(&m);
        CHECK_EQ(mm_bounce_start(&e, &m, 1, 1), 1);
        CHECK_EQ(m.air, 2 * MM_BOUNCE_AIR);
        CHECK_EQ(m.rise, 4);
    }
}

TEST(mech_step_rise_edge_gravities_match_the_bfs)
{
    /* a zone gravity of 0 and of 255 (no clamp; the arg is a byte): the same rise as the BFS */
    static MapBfs b;
    static const DoomoZone z0[1] = { { 0, 0, 3, 3, ZONE_GRAVITY, 0, 0 } };
    static const DoomoZone z3[1] = { { 0, 0, 3, 3, ZONE_GRAVITY, 255, 0 } };
    DoomoMapExt e;
    memset(&e, 0, sizeof e);
    memset(&b, 0, sizeof b);
    b.d = &map_def_t3_kit;
    b.x = &e;
    e.zone_count = 1; e.zones = z0;
    CHECK_EQ(mm_step_rise(&e, 1, 1), map_bfs_rise(&b, 1 + map_def_t3_kit.w));
    CHECK_EQ(mm_step_rise(&e, 1, 1), DOOMO_STEP_UP);
    e.zones = z3;
    CHECK_EQ(mm_step_rise(&e, 1, 1), map_bfs_rise(&b, 1 + map_def_t3_kit.w));
    CHECK_EQ(mm_step_rise(&e, 1, 1), DOOMO_STEP_UP * 100 / 255);
}

/* ---- tubes ------------------------------------------------------------------- */
TEST(mech_tube_whoosh_teleports_under_the_fade)
{
    MmMove m;
    int x, y, h, tx, ty, n = 0, r;
    mm_reset(&m);
    CHECK_EQ(mm_tube_dest(KX, &m, 24, 3, &tx, &ty), 3);
    CHECK_EQ(tx, 27); CHECK_EQ(ty, 3);
    CHECK_EQ(mm_tube_dest(KX, &m, 27, 3, &tx, &ty), 3);       /* both ways */
    CHECK_EQ(tx, 24);
    CHECK_EQ(mm_tube_dest(KX, &m, 25, 3, &tx, &ty), -1);      /* not a station */
    CHECK_EQ(mm_tube_enter(KX, &m, 25, 3), -1);
    CHECK_EQ(mm_tube_enter(KX, &m, 24, 3), 3);
    CHECK_EQ(m.mode, MM_TUBE);
    while ((r = mm_tick(KX, &m, 0, 0, &x, &y, &h)) == 1 && n < 50) {
        n++;
        if (m.t < MM_TUBE_TICKS / 2) CHECK_EQ(x, -1);          /* not yet moved */
        else CHECK_EQ(x, 27 * 256);                            /* moved under the black */
    }
    CHECK_EQ(r, 0);
    CHECK_EQ(n + 1, MM_TUBE_TICKS);
    CHECK_EQ(x, 27 * 256); CHECK_EQ(y, 3 * 256);
    CHECK_EQ(m.mode, MM_ON_FOOT);
    CHECK_EQ(mm_tube_fade(0), 0);
    CHECK_EQ(mm_tube_fade(MM_TUBE_TICKS / 2), 255);
    CHECK_EQ(mm_tube_fade(MM_TUBE_TICKS), 0);
    CHECK(mm_tube_fade(2) == mm_tube_fade(MM_TUBE_TICKS - 2));
    CHECK_EQ(mm_tube_fade(-5), 0);
    CHECK_EQ(mm_tube_fade(99), 0);
}

TEST(mech_tube_junction_lever_picks_the_tube)
{
    /* two tubes leave (2,2): path 0 to (6,2), path 1 to (2,6). Without a lever the first; lever 1 the second. */
    static const uint8_t pts[8] = { 2, 2, 6, 2, 2, 2, 2, 6 };
    static const DoomoPath ps[2] = { { 10, MPATH_TUBE, 2, 0, 0 }, { 11, MPATH_TUBE, 2, 0, 2 } };
    DoomoMapExt e;
    MmMove m;
    int tx, ty;
    memset(&e, 0, sizeof e);
    e.path_count = 2; e.paths = ps; e.path_pts = pts;
    mm_reset(&m);
    CHECK_EQ(mm_tube_dest(&e, &m, 2, 2, &tx, &ty), 0);
    CHECK_EQ(tx, 6); CHECK_EQ(ty, 2);
    mm_tube_lever(&m, 1);
    CHECK_EQ(m.lever, 2);
    CHECK_EQ(mm_tube_dest(&e, &m, 2, 2, &tx, &ty), 1);
    CHECK_EQ(tx, 2); CHECK_EQ(ty, 6);
    mm_tube_lever(&m, 1);                                     /* flipped back */
    CHECK_EQ(mm_tube_dest(&e, &m, 2, 2, &tx, &ty), 0);
    mm_tube_lever(&m, 9);                                     /* out of range: ignored */
    CHECK_EQ(m.lever, 0);
    CHECK_EQ(mm_tube_dest(&e, &m, 6, 2, &tx, &ty), 0);        /* the far end of tube 0 */
    CHECK_EQ(tx, 2);
}

/* ---- the raft ---------------------------------------------------------------- */
TEST(mech_raft_drifts_to_the_end_and_can_hop_off_at_an_island)
{
    MmMove m;
    int x, y, h, n = 0, r;
    mm_reset(&m);
    CHECK_EQ(mm_raft_board(KX, &m, 29, 3), -1);
    CHECK_EQ(mm_raft_board(KX, &m, 28, 3), 4);
    CHECK_EQ(m.mode, MM_RAFT);
    while ((r = mm_tick(KX, &m, 0, 0, &x, &y, &h)) == 1 && n < 200) {
        n++;
        CHECK_EQ(y, 3 * 256);                                  /* centred: no sideways offset */
    }
    CHECK_EQ(r, 0);
    CHECK_EQ(n + 1, 6 * MM_U / MM_RAFT_SPEED);                  /* 6 cells of current */
    CHECK_EQ(x, 34 * 256); CHECK_EQ(y, 3 * 256);
    CHECK_EQ(m.mode, MM_ON_FOOT);
    /* hop off at the island (30,3): from a point to a later one only, and any point may board */
    mm_reset(&m);
    CHECK_EQ(mm_raft_board(KX, &m, 30, 3), 4);
    CHECK_EQ(m.dist, 2 * MM_U);
    CHECK_EQ(m.buoy, 1);
}

TEST(mech_raft_steering_and_capsizing)
{
    MmMove m;
    int x, y, h, n = 0, r;
    mm_reset(&m);
    mm_raft_board(KX, &m, 28, 3);
    /* steer right (+1) on an eastward current: sideways is +y (south) */
    mm_tick(KX, &m, 1, 0, &x, &y, &h);
    CHECK(y > 3 * 256);
    CHECK_EQ(m.side, 2);
    /* keep steering: past MM_RAFT_SIDE it capsizes to the last buoy (28,3) */
    while ((r = mm_tick(KX, &m, 1, 0, &x, &y, &h)) == 1 && n < 50) n++;
    CHECK_EQ(r, 0);
    CHECK_EQ(m.mode, MM_ON_FOOT);
    CHECK_EQ(x, 28 * 256); CHECK_EQ(y, 3 * 256);
    CHECK_EQ(n, MM_RAFT_SIDE / 2 - 1);                /* the first tick is above; tick 7 capsizes */
    /* a later buoy: capsizing after passing the island puts you on the island */
    mm_reset(&m);
    mm_raft_board(KX, &m, 28, 3);
    for (n = 0; n < 2 * MM_U / MM_RAFT_SPEED + 1; n++) mm_tick(KX, &m, 0, 0, &x, &y, &h);
    CHECK_EQ(m.buoy, 1);
    while (mm_tick(KX, &m, -1, 0, &x, &y, &h) == 1) {}
    CHECK_EQ(x, 30 * 256); CHECK_EQ(y, 3 * 256);
    /* off asks to leave where the raft is */
    mm_reset(&m);
    mm_raft_board(KX, &m, 28, 3);
    mm_tick(KX, &m, 0, 0, &x, &y, &h);
    CHECK_EQ(mm_tick(KX, &m, 0, 1, &x, &y, &h), 0);
    CHECK_EQ(m.mode, MM_ON_FOOT);
}

/* ---- carry ------------------------------------------------------------------- */
TEST(mech_carry_holds_one_item_and_blocks_the_gun)
{
    MmMove m;
    mm_reset(&m);
    CHECK_EQ(mm_carry_blocks_fire(&m), 0);
    CHECK_EQ(mm_carry_take(&m, 0), 0);                          /* no such item */
    CHECK_EQ(mm_carry_take(&m, 3), 1);
    CHECK_EQ(mm_carry_take(&m, 4), 0);                          /* both hands are full */
    CHECK_EQ(mm_carry_blocks_fire(&m), 1);
    CHECK_EQ(mm_carry_drop(&m), 3);
    CHECK_EQ(mm_carry_drop(&m), 0);
    CHECK_EQ(mm_carry_blocks_fire(&m), 0);
    CHECK_EQ(mm_carry_lay_tag(KX, 2, 1), 7);                    /* the tagged zone at (1..3, 1) */
    CHECK_EQ(mm_carry_lay_tag(KX, 2, 3), 0);                    /* nothing tagged here: dropped on the floor */
    CHECK_EQ(mm_carry_lay_tag(NULL, 2, 1), 0);
}

/* ---- idle and degenerate input ------------------------------------------------- */
TEST(mech_idle_and_bad_state_cost_nothing)
{
    MmMove m;
    int x, y, h;
    mm_reset(&m);
    CHECK_EQ(sizeof(MmMove), 16);
    CHECK_EQ(mm_tick(KX, &m, 0, 0, &x, &y, &h), 0);             /* on foot: nothing */
    CHECK_EQ(x, -1);
    CHECK_EQ(mm_tick(NULL, &m, 0, 0, &x, &y, &h), 0);
    m.mode = MM_ZIP; m.path = 99;                               /* a stale path: back on foot */
    CHECK_EQ(mm_tick(KX, &m, 0, 0, &x, &y, &h), 0);
    CHECK_EQ(m.mode, MM_ON_FOOT);
    CHECK_EQ(mm_zip_board(NULL, &m, 3, 3), -1);
    CHECK_EQ(mm_bounce_start(NULL, &m, 3, 3), 0);
    CHECK_EQ(mm_ride_board(NULL, &m, 3, 3), -1);
    CHECK_EQ(mm_edges(NULL, 3, 3, NULL, 0), 0);
    CHECK_EQ(mm_edges(&map_ext_t2_gravity, 1, 1, NULL, 0), 0);  /* a map without paths */
}

/* ---- the autopilot's edges ------------------------------------------------------- */
TEST(mech_edges_match_the_bfs_rules)
{
    MmEdge e[8];
    int n;
    n = mm_edges(KX, 3, 3, e, 8);                               /* zip anchor */
    CHECK_EQ(n, 1);
    CHECK_EQ(e[0].to_x, 6); CHECK_EQ(e[0].kind, MPATH_ZIP);
    CHECK_EQ(mm_edges(KX, 6, 3, e, 8), 0);                      /* not back */
    n = mm_edges(KX, 8, 3, e, 8);                               /* ride: to every other stop */
    CHECK_EQ(n, 2);
    CHECK_EQ(e[0].to_x, 10); CHECK_EQ(e[1].to_x, 13);
    CHECK_EQ(mm_edges(KX, 13, 3, e, 8), 2);
    n = mm_edges(KX, 15, 3, e, 8);
    CHECK_EQ(n, 1); CHECK_EQ(e[0].kind, MPATH_ARC); CHECK_EQ(e[0].to_x, 17);
    n = mm_edges(KX, 24, 3, e, 8);
    CHECK_EQ(n, 1); CHECK_EQ(e[0].kind, MPATH_TUBE); CHECK_EQ(e[0].ticks, MM_TUBE_TICKS);
    CHECK_EQ(mm_edges(KX, 27, 3, e, 8), 1);                     /* both ways */
    n = mm_edges(KX, 28, 3, e, 8);                              /* raft: to every LATER point */
    CHECK_EQ(n, 2);
    CHECK_EQ(e[0].to_x, 30); CHECK_EQ(e[1].to_x, 34);
    CHECK_EQ(mm_edges(KX, 34, 3, e, 8), 0);
    CHECK_EQ(mm_edges(KX, 28, 3, e, 1), 1);                     /* max respected */
    CHECK_EQ(mm_edges(KX, 2, 2, e, 8), 0);
}

/* ---- the bot crosses the kit map ------------------------------------------------- */
/* The bot plans with the BFS's walk rule and mm_edges, then plays the plan with the state machine. */
typedef struct { int from, to, kind, path; } Step;
static MapBfs g_bw;
static MapBfsResult g_br;
static int g_prev[64 * 64], g_pkind[64 * 64], g_ppath[64 * 64];

static int bot_plan(Step *out, int max, int sx, int sy, int use_edges, int *used_kinds)
{
    const DoomoMapDef *d = &map_def_t3_kit;
    static int q[64 * 64];
    int head = 0, tail = 0, start = sy * d->w + sx, goal = -1, i, n = 0;
    map_bfs_run(&g_bw, d, KX, NULL, sx, sy, &g_br);
    for (i = 0; i < d->w * d->h; i++) g_prev[i] = -2;
    g_prev[start] = -1;
    q[tail++] = start;
    while (head < tail) {
        int c = q[head++], k, ne;
        static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        MmEdge e[8];
        if (d->cells[c].flags & CELL_EXIT) { goal = c; break; }
        for (k = 0; k < 4; k++) {
            int nx = c % d->w + dx[k], ny = c / d->w + dy[k], ni;
            if (nx < 0 || ny < 0 || nx >= d->w || ny >= d->h) continue;
            ni = ny * d->w + nx;
            if (g_prev[ni] != -2 || !map_bfs_can_step(&g_bw, c, ni, 0)) continue;
            g_prev[ni] = c; g_pkind[ni] = -1; g_ppath[ni] = 0;
            q[tail++] = ni;
        }
        ne = use_edges ? mm_edges(KX, c % d->w, c / d->w, e, 8) : 0;
        for (k = 0; k < ne; k++) {
            int ni = e[k].to_y * d->w + e[k].to_x;
            if (g_prev[ni] != -2) continue;
            g_prev[ni] = c; g_pkind[ni] = e[k].kind; g_ppath[ni] = e[k].path;
            q[tail++] = ni;
        }
    }
    if (goal < 0) return -1;
    for (i = goal; g_prev[i] >= 0 && n < max; i = g_prev[i]) n++;
    for (i = goal, max = n; g_prev[i] >= 0; i = g_prev[i]) {
        out[--n].from = g_prev[i]; out[n].to = i; out[n].kind = g_pkind[i]; out[n].path = g_ppath[i];
        if (g_pkind[i] >= 0) *used_kinds |= 1 << g_pkind[i];
    }
    return max;
}

static int g_bad;
#define BCHK(c) do { if (!(c)) g_bad++; } while (0)

/* play one mechanic edge: the player is at `from`; returns the cell the player stands on after it */
static int bot_ride_edge(MmMove *m, int w, const Step *s, int *ticks)
{
    int fx = s->from % w, fy = s->from / w, tx = s->to % w, ty = s->to / w;
    int x = fx * 256, y = fy * 256, h, r, guard = 0;
    switch (s->kind) {
    case MPATH_ZIP: BCHK(mm_zip_board(KX, m, fx, fy) == s->path); break;
    case MPATH_ARC: BCHK(mm_bounce_start(KX, m, fx, fy) == 1); break;
    case MPATH_TUBE: BCHK(mm_tube_enter(KX, m, fx, fy) == s->path); break;
    case MPATH_RAFT: BCHK(mm_raft_board(KX, m, fx, fy) == s->path); break;
    case MPATH_RIDE: {
        /* wait until the platform is in the player's cell, step on, ride until it is in the target cell */
        int px, py;
        while (guard++ < 1000) {
            mm_ride_platform(KX, &KX->paths[s->path], m->clock, 0, &px, &py);
            if (((px + 128) >> 8) == fx && ((py + 128) >> 8) == fy) break;
            mm_ride_tick(m); (*ticks)++;
        }
        BCHK(mm_ride_board(KX, m, fx, fy) == s->path);
        break;
    }
    }
    guard = 0;
    for (;;) {
        int nx, ny;
        if (guard++ > 2000) { BCHK(0 == 1); break; }
        if (s->kind == MPATH_RIDE) mm_ride_tick(m);
        (*ticks)++;
        r = mm_tick(KX, m, 0, 0, &nx, &ny, &h);
        if (nx >= 0) { x = nx; y = ny; }
        if (s->kind == MPATH_RIDE && ((x + 128) >> 8) == tx && ((y + 128) >> 8) == ty) {
            mm_tick(KX, m, 0, 1, &nx, &ny, &h);                  /* off, on the far stop */
            x = nx; y = ny; break;
        }
        if (s->kind == MPATH_RAFT && s->to != s->from && ((x + 128) >> 8) == tx && ((y + 128) >> 8) == ty && r) {
            if (tx == 30) { mm_tick(KX, m, 0, 1, &nx, &ny, &h); x = nx; y = ny; break; }
        }
        if (!r) break;
    }
    BCHK((m->mode) == (MM_ON_FOOT));
    return ((y + 128) >> 8) * w + ((x + 128) >> 8);
}

static uint32_t bot_cross(int *used, int *ticks, int *steps_out)
{
    Step plan[128];
    MmMove m;
    int i, n, at, sx = 1, sy = 3;
    uint32_t hash = 2166136261u;
    *used = 0; *ticks = 0;
    mm_reset(&m);
    n = bot_plan(plan, 128, sx, sy, 1, used);
    BCHK(n > 0);
    *steps_out = n;
    at = sy * map_def_t3_kit.w + sx;
    for (i = 0; i < n; i++) {
        BCHK((plan[i].from) == (at));
        if (plan[i].kind < 0) { at = plan[i].to; (*ticks)++; }
        else at = bot_ride_edge(&m, map_def_t3_kit.w, &plan[i], ticks);
        BCHK((at) == (plan[i].to));                         /* the machine put the bot where the edge said */
        hash = (hash ^ (uint32_t)at) * 16777619u;
    }
    BCHK(map_def_t3_kit.cells[at].flags & CELL_EXIT);
    return hash;
}

TEST(mech_bot_crosses_the_kit_map_with_all_five_edge_kinds)
{
    int used, ticks, steps, used2, ticks2, steps2;
    uint32_t h1;
    g_bad = 0;
    h1 = bot_cross(&used, &ticks, &steps);
    uint32_t h2 = bot_cross(&used2, &ticks2, &steps2);
    CHECK_EQ(g_bad, 0);
    CHECK_EQ(used, (1 << MPATH_ZIP) | (1 << MPATH_RIDE) | (1 << MPATH_ARC) | (1 << MPATH_TUBE) | (1 << MPATH_RAFT));
    CHECK_EQ(h1, h2);                                    /* deterministic */
    CHECK_EQ(ticks, ticks2);
    CHECK(ticks < 600);                                  /* under 20 seconds of play */
}

TEST(mech_kit_map_needs_the_mechanics)
{
    Step plan[128];
    int used = 0;
    CHECK(bot_plan(plan, 128, 1, 3, 1, &used) > 0);
    CHECK(g_br.exit_reached == 1 && g_br.stuck == 0);
    CHECK_EQ(bot_plan(plan, 128, 1, 3, 0, &used), -1);   /* walking alone never gets there */
    /* and the low-gravity step is one of the BFS's walk edges: without the zone it is not */
    CHECK(map_bfs_can_step(&g_bw, 21 + 3 * map_def_t3_kit.w, 22 + 3 * map_def_t3_kit.w, 0) == 1);
    CHECK(map_bfs_can_step(&g_bw, 2 + 3 * map_def_t3_kit.w, 3 + 3 * map_def_t3_kit.w, 0) == 1);
    {
        MapBfsResult r2;
        static MapBfs b2;
        DoomoMapExt nog = map_ext_t3_kit;
        nog.zone_count = 0;                              /* no zones: neither the gravity nor the pad */
        map_bfs_run(&b2, &map_def_t3_kit, &nog, NULL, 1, 3, &r2);
        CHECK(!map_bfs_can_step(&b2, 21 + 3 * map_def_t3_kit.w, 22 + 3 * map_def_t3_kit.w, 0));
    }
}
