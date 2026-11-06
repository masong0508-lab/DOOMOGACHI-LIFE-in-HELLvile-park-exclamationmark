/* test_sim_core.c - life sim core fidelity (owner: life).
 *
 * Fast-forward (sim_advance_minutes: the story's bed, travel) lives each hour of
 * the skip with that hour's routine; one big skip and many small ones agree; the
 * clock moves exactly; the town stays valid after long skips. The relationship
 * graph: the edge pass never steps over an edge when it drops a forgotten
 * acquaintance, and best friends / nemeses hold their rank a little (hysteresis).
 */
#include "test.h"
#include "../src/sim_int.h"

static void fresh(int hour)
{
    int i;
    town_new(TOWN_FREEPLAY, 4242, TOWN_POP_PREMADE);
    doomo_town.minute = (uint16_t)(hour * 60);
    sim_init();
    sim_seed(1);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        DoomoResident *r = &doomo_town.res[i];
        if (r->status != RES_ALIVE) continue;
        r->stat[STAT_ENERGY] = 30;
        r->stat[STAT_HUNGER] = 80;
        r->stat[STAT_HYGIENE] = 80;
    }
}

typedef struct Avg { int n, energy, hunger, hygiene, rested; } Avg;

/* Hillvile adults (the plain 22:00-06:00 sleepers) or everyone alive */
static Avg average(int hillvile_adults_only)
{
    Avg a = { 0, 0, 0, 0, 0 };
    int i;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        const DoomoResident *r = &doomo_town.res[i];
        if (r->status != RES_ALIVE) continue;
        if (hillvile_adults_only && (RES_DISTRICT(i) || sim_life_stage(i) != LIFE_ADULT)) continue;
        a.n++;
        a.energy += r->stat[STAT_ENERGY];
        a.hunger += r->stat[STAT_HUNGER];
        a.hygiene += r->stat[STAT_HYGIENE];
        if (r->stat[STAT_ENERGY] >= 90) a.rested++;
    }
    if (a.n) { a.energy /= a.n; a.hunger /= a.n; a.hygiene /= a.n; }
    return a;
}

static int absdiff(int a, int b) { return a > b ? a - b : b - a; }

/* eight hours of sleep rest the sleepers, and are not paid for as eight
 * breakfasts and eight showers (the old single catch-up did both) */
TEST(simcore_a_night_skipped_rests_the_town_and_empties_the_stomach)
{
    Avg a;
    fresh(22);
    sim_advance_minutes(8 * 60);
    a = average(1);
    printf("    night skip, hillvile adults: %d, rested %d, energy %d hunger %d hygiene %d\n",
           a.n, a.rested, a.energy, a.hunger, a.hygiene);
    CHECK(a.n > 300);
    CHECK(a.rested * 100 >= a.n * 80);       /* most slept the night through   */
    CHECK(a.energy >= 90);
    CHECK(a.hunger < 75);                    /* a night without a meal ...     */
    CHECK(a.hygiene < 80);                   /* ... and without a shower       */
}

/* the same eight hours in one call or in eight: the same town, give or take the dice */
TEST(simcore_one_long_skip_matches_hourly_skips)
{
    Avg one, many;
    int h;
    fresh(22);
    sim_advance_minutes(8 * 60);
    one = average(0);
    fresh(22);
    for (h = 0; h < 8; h++) sim_advance_minutes(60);
    many = average(0);
    printf("    all alive: one skip e%d/h%d/y%d, hourly e%d/h%d/y%d\n", one.energy, one.hunger,
           one.hygiene, many.energy, many.hunger, many.hygiene);
    CHECK(absdiff(one.energy, many.energy) <= 4);
    CHECK(absdiff(one.hunger, many.hunger) <= 4);
    CHECK(absdiff(one.hygiene, many.hygiene) <= 4);
}

TEST(simcore_skips_move_the_clock_exactly)
{
    static const int spans[] = { 1, 14, 59, 60, 61, 479, 720, 721, 1440, 3000 };
    int i;
    for (i = 0; i < (int)DOOMO_COUNTOF(spans); i++) {
        uint32_t before, days0;
        int wd0;
        fresh(21);
        doomo_town.minute = 21 * 60 + 17;
        before = sim_now();
        days0 = doomo_town.day;
        wd0 = doomo_town.weekday;
        sim_advance_minutes(spans[i]);
        CHECK_MSG(sim_now() - before == (uint32_t)spans[i], "span %d: clock moved %u", spans[i],
                  (unsigned)(sim_now() - before));
        CHECK_MSG(doomo_town.minute < 1440, "span %d: minute %d", spans[i], doomo_town.minute);
        CHECK_MSG(doomo_town.weekday == (wd0 + (int)(doomo_town.day - days0)) % 7,
                  "span %d: weekday follows the day", spans[i]);
    }
    /* nothing to do for a span of zero or less */
    fresh(9);
    {
        uint32_t t = sim_now();
        sim_advance_minutes(0);
        sim_advance_minutes(-30);
        CHECK_EQ_U(sim_now(), t);
    }
}

TEST(simcore_long_skips_keep_the_town_valid)
{
    int i, s, round;
    fresh(20);
    for (round = 0; round < 4; round++) {
        sim_advance_minutes(round == 3 ? 5 * 1440 + 333 : 9 * 60 + round * 211);
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
            const DoomoResident *r = &doomo_town.res[i];
            if (r->status == RES_EMPTY) continue;
            for (s = 0; s < STAT_COUNT; s++)
                CHECK_MSG(r->stat[s] <= 100, "round %d res %d stat %d = %d", round, i, s, r->stat[s]);
            CHECK_MSG(r->activity < ACT_COUNT, "round %d res %d act %d", round, i, r->activity);
            CHECK_MSG(r->mood < MOOD_COUNT, "round %d res %d mood %d", round, i, r->mood);
        }
        CHECK_EQ(sim_possessed_count(), town_count(RES_POSSESSED));
        CHECK(sim_possessed_count() <= sim_possession_cap());
        CHECK(doomo_town.edge_count <= DOOMO_MAX_EDGES);
        for (i = 1; i < doomo_town.edge_count; i++) {
            const DoomoEdge *p = &doomo_town.edges[i - 1], *e = &doomo_town.edges[i];
            CHECK_MSG(((uint32_t)p->a << 16 | p->b) < ((uint32_t)e->a << 16 | e->b),
                      "round %d: edges unsorted at %d", round, i);
        }
    }
}

/* a day skipped is a day lived: every midnight crossed counts, once */
TEST(simcore_skipped_days_count_as_lived)
{
    uint32_t days0, p0;
    fresh(12);
    days0 = doomo_town.day;
    p0 = progress_get(PROG_DAYS_LIVED);
    sim_advance_minutes(3 * 1440);
    CHECK_EQ_U(doomo_town.day - days0, 3u);
    CHECK_EQ_U(progress_get(PROG_DAYS_LIVED) - p0, 3u);
}

/* dropping a forgotten acquaintance slides the next edge into its place: the
 * cursor stays, so no edge is stepped over in that pass */
TEST(simcore_edge_pass_visits_every_edge_once)
{
    enum { N = 40, VISITS = 20 };
    int i, removed;
    fresh(10);
    doomo_town.edge_count = 0;
    sim_social_init();
    for (i = 0; i < N; i++) {
        int a = 100 + 2 * i, b = 101 + 2 * i;
        rel_adjust(a, b, 0);                              /* a value-0 acquaintance */
        doomo_town.res[a].activity = ACT_HOME;            /* not at a venue: out of sight */
        doomo_town.res[b].activity = ACT_HOME;
        doomo_town.res[a].status = doomo_town.res[b].status = RES_ALIVE;
    }
    CHECK_EQ(doomo_town.edge_count, N);
    sim_s.edge_cursor = 0;
    sim_s.edge_credit = 60u * VISITS;
    sim_s.ops = 0;
    sim_edges_step(VISITS);
    removed = N - doomo_town.edge_count;
    printf("    edge pass: %d visits, %d forgotten, cursor %d\n", VISITS, removed, sim_s.edge_cursor);
    CHECK(removed > 0);                                   /* the case under test happened */
    CHECK_EQ(sim_s.edge_cursor + removed, VISITS);        /* each visit removed one or moved on */
    CHECK_EQ_U(sim_s.ops, (uint32_t)VISITS);
}

TEST(simcore_best_friends_and_nemeses_hold_their_rank)
{
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_EMPTY);
    CHECK_EQ(rel_kind(4, 9), -1);                         /* a clean pair */
    /* best friends until below 60 */
    rel_adjust(4, 9, 80);
    CHECK_EQ(rel_kind(4, 9), REL_BEST_FRIEND);
    rel_adjust(4, 9, -15);                                /* 65 */
    CHECK_EQ(rel_kind(4, 9), REL_BEST_FRIEND);
    rel_adjust(4, 9, -5);                                 /* 60 */
    CHECK_EQ(rel_kind(4, 9), REL_BEST_FRIEND);
    rel_adjust(4, 9, -1);                                 /* 59 */
    CHECK_EQ(rel_kind(4, 9), REL_FRIEND);
    rel_adjust(4, 9, 10);                                 /* 69: friends climb back at 70 */
    CHECK_EQ(rel_kind(4, 9), REL_FRIEND);
    rel_adjust(4, 9, 1);
    CHECK_EQ(rel_kind(4, 9), REL_BEST_FRIEND);
    /* nemeses until above -60 */
    rel_adjust(4, 9, -150);                               /* -80 */
    CHECK_EQ(rel_kind(4, 9), REL_NEMESIS);
    rel_adjust(4, 9, 15);                                 /* -65 */
    CHECK_EQ(rel_kind(4, 9), REL_NEMESIS);
    rel_adjust(4, 9, 5);                                  /* -60 */
    CHECK_EQ(rel_kind(4, 9), REL_NEMESIS);
    rel_adjust(4, 9, 1);                                  /* -59 */
    CHECK_EQ(rel_kind(4, 9), REL_RIVAL);
    rel_adjust(4, 9, -10);                                /* -69: rivals fall at -70 */
    CHECK_EQ(rel_kind(4, 9), REL_RIVAL);
    rel_adjust(4, 9, -1);
    CHECK_EQ(rel_kind(4, 9), REL_NEMESIS);
    /* a big swing still skips straight over the band */
    rel_adjust(4, 9, 130);                                /* 60 -> from nemesis to friend */
    CHECK_EQ(rel_kind(4, 9), REL_FRIEND);
    /* special bonds are never touched by it */
    rel_set_kind(4, 9, REL_DATING);
    rel_adjust(4, 9, -40);
    CHECK_EQ(rel_kind(4, 9), REL_DATING);
}

/* a bond that wobbles around 70 posts its headline once, not on every crossing */
TEST(simcore_a_wobbling_bond_does_not_repeat_the_headline)
{
    int i, crossings = 0, before;
    town_new(TOWN_FREEPLAY, 99, TOWN_POP_EMPTY);
    CHECK_EQ(rel_kind(20, 21), -1);                       /* a clean pair */
    rel_adjust(20, 21, 72);
    CHECK_EQ(rel_kind(20, 21), REL_BEST_FRIEND);
    before = rel_kind(20, 21);
    for (i = 0; i < 40; i++) {
        rel_adjust(20, 21, (i & 1) ? 9 : -9);             /* 63 <-> 72 */
        if (rel_kind(20, 21) != before) { crossings++; before = rel_kind(20, 21); }
    }
    CHECK_EQ(crossings, 0);
    /* the same at the other end */
    rel_adjust(20, 21, -144);                             /* -72 */
    CHECK_EQ(rel_kind(20, 21), REL_NEMESIS);
    before = rel_kind(20, 21);
    for (i = 0; i < 40; i++) {
        rel_adjust(20, 21, (i & 1) ? -9 : 9);             /* -63 <-> -72 */
        if (rel_kind(20, 21) != before) { crossings++; before = rel_kind(20, 21); }
    }
    CHECK_EQ(crossings, 0);
}
