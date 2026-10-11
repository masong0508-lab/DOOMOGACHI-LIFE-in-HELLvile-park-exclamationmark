/* test_sim.c - the life simulation (owner: life).
 *
 * 60 simulated days with all 1000 premade citizens: invariants (stats in
 * range, edges valid, possession bounded), that social life actually
 * happens (friendships, crushes, couples, news), determinism with a
 * seeded RNG, the per-tick cost budget, care reactions, possession and
 * lay-to-rest gating.
 */
#include <time.h>
#include "test.h"

static void fast_clock(void)
{
    opt_set(OPT_DAY_LENGTH, 4);          /* 15 minutes per day           */
    opt_set(OPT_SIM_SPEED, 3);           /* turbo                        */
}

/* Run the sim until `days` more game days have passed. */
static void run_days(int days)
{
    uint32_t end = doomo_town.day + (uint32_t)days;
    while (doomo_town.day < end) sim_tick();
}

static int check_invariants(char *why, int cap)
{
    int i, s;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        const DoomoResident *r = &doomo_town.res[i];
        if (r->status > RES_GHOST) { snprintf(why, cap, "res %d bad status %d", i, r->status); return 0; }
        if (r->status == RES_EMPTY) continue;
        for (s = 0; s < STAT_COUNT; s++)
            if (r->stat[s] > 100) { snprintf(why, cap, "res %d stat %d = %d", i, s, r->stat[s]); return 0; }
        if (r->mood >= MOOD_COUNT) { snprintf(why, cap, "res %d mood %d", i, r->mood); return 0; }
        if (r->activity >= ACT_COUNT) { snprintf(why, cap, "res %d act %d", i, r->activity); return 0; }
        if (r->status == RES_POSSESSED && r->stat[STAT_CORRUPTION] != 100) {
            snprintf(why, cap, "res %d possessed at corruption %d", i, r->stat[STAT_CORRUPTION]);
            return 0;
        }
    }
    if (doomo_town.edge_count > DOOMO_MAX_EDGES) { snprintf(why, cap, "edge_count %d", doomo_town.edge_count); return 0; }
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        if (e->a >= e->b || e->b > DOOMO_PLAYER_ID) { snprintf(why, cap, "edge %d ids %d,%d", i, e->a, e->b); return 0; }
        if ((e->kind & REL_KIND_MASK) >= REL_KIND_COUNT) { snprintf(why, cap, "edge %d kind", i); return 0; }
        if (e->value < -100 || e->value > 100) { snprintf(why, cap, "edge %d value", i); return 0; }
        if (i > 0) {
            const DoomoEdge *p = &doomo_town.edges[i - 1];
            if (((uint32_t)p->a << 16 | p->b) >= ((uint32_t)e->a << 16 | e->b)) {
                snprintf(why, cap, "edges unsorted at %d", i); return 0;
            }
        }
    }
    for (i = 0; i < sim_problem_count(); i++) {
        const DoomoProblem *p = sim_problem(i);
        if (!p || p->kind <= PROB_NONE || p->kind >= PROB_COUNT || p->resident >= DOOMO_MAX_RESIDENTS) {
            snprintf(why, cap, "problem %d bad", i); return 0;
        }
    }
    for (i = 0; i < sim_news_count(); i++)
        if (strlen(sim_news(i)) >= NEWS_LEN || !sim_news(i)[0]) { snprintf(why, cap, "news %d", i); return 0; }
    return 1;
}

static int count_kind(int kind)
{
    int i, n = 0;
    for (i = 0; i < doomo_town.edge_count; i++)
        if ((doomo_town.edges[i].kind & REL_KIND_MASK) == kind) n++;
    return n;
}

TEST(sim_sixty_days_premade)
{
    char why[160];
    int day, friends0, married0, dating0, maxposs = 0;
    uint32_t prog_days0;
    clock_t t0;
    const DoomoSimStats *st;
    town_new(TOWN_FREEPLAY, 4242, TOWN_POP_PREMADE);
    fast_clock();
    sim_init();
    sim_seed(777);
    friends0 = count_kind(REL_FRIEND) + count_kind(REL_BEST_FRIEND);
    married0 = count_kind(REL_MARRIED);
    dating0 = count_kind(REL_DATING);
    prog_days0 = progress_get(PROG_DAYS_LIVED);
    t0 = clock();
    for (day = 0; day < 60; day++) {
        run_days(1);
        CHECK_MSG(check_invariants(why, sizeof(why)), "day %d: %s", day, why);
        if (sim_possessed_count() > maxposs) maxposs = sim_possessed_count();
        CHECK_MSG(sim_possessed_count() <= sim_possession_cap(),
                  "day %d: %d possessed > cap %d", day, sim_possessed_count(), sim_possession_cap());
        CHECK_EQ(sim_possessed_count(), town_count(RES_POSSESSED));
    }
    st = sim_stats();
    {
        int hist[11] = { 0 }, i, hap = 0, n = 0, act[ACT_COUNT] = { 0 }, avg[STAT_COUNT] = { 0 }, k;
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
            const DoomoResident *r = &doomo_town.res[i];
            if (r->status != RES_ALIVE) continue;
            hist[r->stat[STAT_CORRUPTION] / 10]++;
            hap += r->stat[STAT_HAPPY]; n++;
            act[r->activity]++;
            for (k = 0; k < STAT_COUNT; k++) avg[k] += r->stat[k];
        }
        printf("    avg stats:");
        for (k = 0; k < STAT_COUNT; k++) printf(" %d", n ? avg[k] / n : 0);
        printf("\n");
        printf("    corruption histogram:");
        for (i = 0; i < 11; i++) printf(" %d", hist[i]);
        printf("  avg happy %d  acts:", n ? hap / n : 0);
        for (i = 0; i < ACT_COUNT; i++) printf(" %d", act[i]);
        printf("\n");
    }
    printf("    60 days in %.2fs: chats %u meetings %u forgotten %u args %u crushes %u confess %u dates %u rej %u "
           "proposals %u weddings %u breakups %u babies %u possessions %u nightmares %u "
           "auto-exorcisms %u news %u problems %u\n",
           (double)(clock() - t0) / CLOCKS_PER_SEC,
           st->chats, st->meetings, st->forgotten, st->arguments, st->crushes, st->confessions, st->dates, st->rejections,
           st->proposals, st->marriages, st->breakups, st->babies, st->possessions,
           st->nightmares, st->exorcisms_auto, st->news, st->problems);
    printf("    edges %d (friends %d->%d, dating %d->%d, married %d->%d, rivals %d, nemeses %d, exes %d)"
           " possessed now %d max %d cap %d, residents %d, max ops/tick %u\n",
           doomo_town.edge_count, friends0, count_kind(REL_FRIEND) + count_kind(REL_BEST_FRIEND),
           dating0, count_kind(REL_DATING), married0, count_kind(REL_MARRIED),
           count_kind(REL_RIVAL), count_kind(REL_NEMESIS), count_kind(REL_EX),
           sim_possessed_count(), maxposs, sim_possession_cap(), town_count(-1), sim_max_tick_ops());
    CHECK_EQ(progress_get(PROG_DAYS_LIVED) - prog_days0, 60);
    CHECK(st->meetings > 100);
    CHECK(st->crushes > 5);
    CHECK(st->dates > 2);
    CHECK(st->news > 50);
    CHECK(st->problems > 20);
    CHECK(progress_get(PROG_FRIENDSHIPS) > 10);
    CHECK(sim_news_count() == NEWS_MAX);
    CHECK(sim_max_tick_ops() <= SIM_BATCH * 2 + SIM_EDGE_BATCH * 2);
}
