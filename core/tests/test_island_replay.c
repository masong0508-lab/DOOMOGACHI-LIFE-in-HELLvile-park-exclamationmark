/* test_island_replay.c - the island's recorded lists (docs/ISLAND_SPEED.md I1-I3b): land, patches, roads, and the
 * animation-step lists (sea glints, walkers, weather). Every frame of a run that replays them must equal the same run
 * with every list dropped before each frame (drawn live), by day and by night, with weather on.
 * Also: the replays happen (a test that never replays proves nothing). */
#include "test.h"
#include "../src/fp_island.h"

#define NF 150

static uint32_t fb_hash(void)
{
    uint32_t h = 2166136261u;
    int i, n = nat_w * nat_h;
    for (i = 0; i < n; i++) h = (h ^ nat_fb[i]) * 16777619u;
    return h;
}

static void run(int flush, int minute, uint32_t *out, int *hits)
{
    int f;
    doomo_init();
    test_reset();
    opt_set(OPT_WEATHER, 2);
    fp_new_island(TOWN_POP_PREMADE, 4242);
    doomo_town.minute = (uint16_t)minute;
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
    for (f = 0; f < NF; f++) {
        if (flush) fp_island_replay_flush();
        plat_begin_frame();
        doomo_frame(0);
        plat_end_frame();
        out[f] = fb_hash();
    }
    *hits = fp_island_replay_hits();
}

static void compare(int minute)
{
    static uint32_t a[NF], b[NF];
    int f, ha, hb;
    run(0, minute, a, &ha);
    run(1, minute, b, &hb);
    for (f = 0; f < NF; f++)
        CHECK_MSG(a[f] == b[f], "minute %d: frame %d differs between replayed and live lists", minute, f);
    CHECK_MSG(ha > NF, "minute %d: only %d replays in %d frames", minute, ha, NF);
}

TEST(island_replayed_lists_equal_live_lists_by_day)
{
    compare(12 * 60);
}

TEST(island_replayed_lists_equal_live_lists_by_night)
{
    compare(23 * 60);
}
