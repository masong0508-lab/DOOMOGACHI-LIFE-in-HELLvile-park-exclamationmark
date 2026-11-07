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

/* (I8) a toast that holds still no longer forces the hub to redraw in full: while it is steady the hub keeps the
 * frame (toast on it) and only the toast's timer runs. Against the same run with the hold off (every tick drawn in
 * full), every tick that was drawn in full must be the same picture, the toast must end on the same tick, a
 * held tick must repeat the previous one above the news strip, and holds must really happen under a toast. */
#define TNF 220
#define T0 40
static void toast_run(int hold_off, const char *msg, uint32_t *full, uint32_t *top, uint8_t *held, uint8_t *pend)
{
    int f;
    doomo_init();
    test_reset();
    fp_island_hub_hold_off(hold_off);
    fp_new_island(TOWN_POP_PREMADE, 4242);
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
    for (f = 0; f < TNF; f++) {
        int before = fp_island_hub_held();
        if (f == T0) ui_toast(msg);
        plat_begin_frame();
        doomo_frame(0);
        plat_end_frame();
        full[f] = fb_hash();
        rows_hash(&top[f], 0, nat_h - 14);
        held[f] = (uint8_t)(fp_island_hub_held() > before);
        pend[f] = (uint8_t)ui_toast_pending();
    }
    fp_island_hub_hold_off(0);
}

static void toast_hold_run(const char *msg, int w, int h)
{
    static uint32_t fa[TNF], ta[TNF], fb[TNF], tb[TNF];
    static uint8_t ha[TNF], hb[TNF], pa[TNF], pb[TNF];
    int f, under = 0;
    test_set_res(w, h);
    toast_run(0, msg, fa, ta, ha, pa);
    toast_run(1, msg, fb, tb, hb, pb);
    for (f = 0; f < TNF; f++) {
        CHECK_MSG(pa[f] == pb[f], "%dx%d '%.12s': tick %d toast pending %d with holds, %d without", w, h, msg, f, pa[f], pb[f]);
        CHECK_MSG(!hb[f], "hold-off run held tick %d", f);
        if (!ha[f]) {
            CHECK_MSG(fa[f] == fb[f], "%dx%d '%.12s': tick %d was drawn in full but differs from the all-full run", w, h, msg, f);
        } else {
            CHECK_MSG(f > 0 && ta[f] == ta[f - 1], "%dx%d '%.12s': held tick %d changed above the news strip", w, h, msg, f);
            if (pa[f]) under++;
        }
    }
    CHECK_MSG(under > 30, "%dx%d '%.12s': only %d ticks held under the toast", w, h, msg, under);
}

TEST(island_hub_holds_under_a_steady_toast)
{
    toast_hold_run("Rent collected", 240, 160);
    toast_hold_run("Rent collected", 320, 240);
    toast_hold_run("NEW UNLOCK: a very long line that has to scroll across its pill", 240, 160);
    test_set_res(240, 160);
}
