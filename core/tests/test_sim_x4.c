/* test_sim_x4.c - SP2 (X4): the resident update's cheaper inputs (persona and liked hobby unpacked once per
 * update, the possession cap decided without counting the town when it can be) give the same town as the code
 * before it, byte for byte.
 *
 * The goldens are the FNV-1a of every resident record, the edges and the clock after the runs below, taken with
 * the tree before X4 (origin 4851147). A long freeplay run at turbo speed (hourly routines, needs, venues,
 * problems), a story-mode town past chapter 2 at the highest possession rate (residents at 99 corruption ask
 * whether they may be possessed on every update) and a fast-forward (sim_advance_minutes: every resident's
 * routine is chosen again each hour) all have to end in the same bytes. */
#include "test.h"
#include "../src/sim_int.h"

static uint32_t fnv(const void *p, size_t n, uint32_t h)
{
    const uint8_t *b = (const uint8_t *)p;
    size_t i;
    for (i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
    return h;
}

static uint32_t town_bytes(void)
{
    uint32_t h = 2166136261u;
    h = fnv(doomo_town.res, sizeof(doomo_town.res), h);
    h = fnv(doomo_town.edges, sizeof(doomo_town.edges[0]) * (size_t)doomo_town.edge_count, h);
    h = fnv(&doomo_town.edge_count, sizeof(doomo_town.edge_count), h);
    h = fnv(&doomo_town.day, sizeof(doomo_town.day), h);
    h = fnv(&doomo_town.minute, sizeof(doomo_town.minute), h);
    return h ^ (uint32_t)sim_news_count() * 31u ^ (uint32_t)sim_problem_count() * 131u ^
           (uint32_t)sim_possessed_count() * 1031u;
}

static uint32_t run(int mode, uint32_t seed, int days, int possession, int skip)
{
    uint32_t end;
    town_new(mode, seed, TOWN_POP_PREMADE);
    if (mode == TOWN_STORY) doomo_town.story.chapter = 3;
    opt_set(OPT_DAY_LENGTH, 4);
    opt_set(OPT_SIM_SPEED, 3);
    opt_set(OPT_POSSESSION_RATE, possession);
    sim_init();
    sim_seed(seed * 7u + 1u);
    end = doomo_town.day + (uint32_t)days;
    while (doomo_town.day < end) sim_tick();
    if (skip) sim_advance_minutes(skip);
    return town_bytes();
}

TEST(sim_x4_same_town_as_before)
{
    uint32_t a = run(TOWN_FREEPLAY, 2024, 6, 1, 0);
    uint32_t b = run(TOWN_STORY, 77, 5, 3, 0);
    int possessed = sim_possessed_count();
    uint32_t c = run(TOWN_FREEPLAY, 9, 2, 2, 600);
    opt_set(OPT_POSSESSION_RATE, 1);
    CHECK_MSG(possessed > 0, "the story run possessed nobody: it does not exercise the cap");
    CHECK_MSG(a == 0x81fe3990u && b == 0x9c13a1d2u && c == 0x97b018b2u,
              "freeplay 0x%08x, story possession 0x%08x, fast-forward 0x%08x", (unsigned)a, (unsigned)b,
              (unsigned)c);
}

TEST(sim_x4_constant_divisions_are_exact)
{
    static const uint32_t divs[4] = { 60, 120, 240, 360 };
    uint32_t q;
    int k, x, bad = 0;
    for (k = 0; k < 4; k++)
        for (q = 0; q < (1u << 19); q++)                 /* the fast range (2^18) and past it */
            if (sim_udiv_test(q, divs[k]) != q / divs[k]) bad++;
    for (x = -60000; x <= 60000; x++)
        if (sim_div100_test(x) != x / 100) bad++;
    CHECK_MSG(bad == 0, "%d quotients differ from the division", bad);
}
