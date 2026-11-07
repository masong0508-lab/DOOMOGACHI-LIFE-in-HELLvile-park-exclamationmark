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

/* (I7) the hub holds its last clean full frame between full redraws: a held frame is the previous frame everywhere
 * but the news strip, holds really happen, and a press redraws in full at once. */
static void rows_hash(uint32_t *h, int y0, int y1)
{
    int i;
    *h = 2166136261u;
    for (i = y0 * nat_w; i < y1 * nat_w; i++) *h = (*h ^ nat_fb[i]) * 16777619u;
}

static void test_set_res(int w, int h)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = w; o.h = h; o.seed = 0x5EED1234u; o.quiet = 1;
    o.save_dir = test_tmp_dir;
    nat_init(&o);
}

static void hub_hold_run(int w, int h)
{
    int f, held_frames = 0, press_redrawn = 0, press_changed = 0;
    uint32_t prev = 0, cur = 0;
    test_set_res(w, h);
    doomo_init();
    test_reset();
    fp_new_island(TOWN_POP_PREMADE, 4242);
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
    for (f = 0; f < 120; f++) {
        int before = fp_island_hub_held();
        uint32_t pad = f == 80 ? BTN_RIGHT : 0;
        plat_begin_frame();
        doomo_frame(pad);
        plat_end_frame();
        rows_hash(&cur, 0, nat_h - 14);              /* everything above the news strip */
        if (fp_island_hub_held() > before) {
            held_frames++;
            CHECK_MSG(cur == prev, "%dx%d: held frame %d differs above the news strip", w, h, f);
        }
        if (f == 80) {
            press_redrawn = fp_island_hub_held() == before;
            press_changed = cur != prev;
        }
        prev = cur;
    }
    CHECK_MSG(held_frames > 40, "%dx%d: only %d of 120 frames held", w, h, held_frames);
    CHECK_MSG(press_redrawn, "%dx%d: a press frame was held", w, h);
    CHECK_MSG(press_changed, "%dx%d: a press did not change the picture", w, h);
}

TEST(island_hub_holds_clean_frames_and_redraws_on_input)
{
    hub_hold_run(240, 160);
    hub_hold_run(320, 240);
    test_set_res(240, 160);
}
