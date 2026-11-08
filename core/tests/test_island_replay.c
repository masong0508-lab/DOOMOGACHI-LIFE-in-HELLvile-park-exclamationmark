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

/* (SP3, P-I1) the lists in a scene scratch, as on the PS1 (the overlay slot's tail): every frame equals the live
 * run's, also across "overlay loads" that wipe the scratch and change its generation (the lists start over), and the
 * replays happen. */
static void test_set_res(int w, int h);

static void run_scratch(int minute, uint32_t *out, int *hits, int *binds)
{
    int f, b0 = fp_island_lists_bound();
    nat_scene_scratch(1, 1);
    doomo_init();
    test_reset();
    opt_set(OPT_WEATHER, 2);
    fp_new_island(TOWN_POP_PREMADE, 4242);
    doomo_town.minute = (uint16_t)minute;
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
    for (f = 0; f < NF; f++) {
        if (f == 37 || f == 90 || f == 91) nat_scene_scratch(1, 1);
        plat_begin_frame();
        doomo_frame(0);
        plat_end_frame();
        out[f] = fb_hash();
    }
    *hits = fp_island_replay_hits();
    *binds = fp_island_lists_bound() - b0;
    nat_scene_scratch(0, 1);
    fp_island_replay_flush();                       /* the next run starts from its own lists */
}

TEST(island_lists_in_a_scene_scratch_equal_live_lists)
{
    static uint32_t a[NF], b[NF];
    int f, ha, hb, h0, o0, binds, m;
    for (m = 0; m < 3; m++) {
        int minute = m == 1 ? 23 * 60 : 12 * 60;
        if (m == 2) test_set_res(320, 240);           /* the PS1's size: the lists are longer */
        if (m != 1) run(0, minute, b, &hb);          /* warm-up: the first island run in a process draws its caches */
        run(1, minute, b, &hb);
        h0 = fp_island_replay_hits();
        o0 = fp_island_replay_overs();
        run_scratch(minute, a, &ha, &binds);
        CHECK_MSG(fp_island_replay_overs() == o0, "minute %d, %dx%d: %d recordings overflowed their scratch list",
                  minute, nat_w, nat_h, fp_island_replay_overs() - o0);
        for (f = 0; f < NF; f++)
            CHECK_MSG(a[f] == b[f], "minute %d: frame %d differs between lists in a scene scratch and live", minute, f);
        CHECK_MSG(binds >= 4, "minute %d: the lists were bound to the scratch %d times (wiped 4 times)", minute, binds);
        CHECK_MSG(ha - h0 > NF, "minute %d: only %d replays from the scene scratch in %d frames", minute, ha - h0, NF);
    }
    test_set_res(240, 160);
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
 * held tick must repeat the previous one above the news strip (I11: outside the toast's band, where a moving
 * toast now draws over the kept frame), and holds must really happen under a toast. */
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
        {
            uint32_t a, b;
            int by, bh;
            CHECK(nat_strip_band(&by, &bh));          /* the band the hub keeps, at its toast lane */
            rows_hash(&a, 0, by);
            rows_hash(&b, by + bh, nat_h - 14);
            top[f] = a * 31u + b;
        }
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

/* (I11) a toast that moves (slides in, scrolls, slides out) no longer forces the hub to redraw in full either: a
 * held tick puts the toast's band back from the clean copy of the kept frame (plat_strip) and the toast draws itself
 * on it. Against the hold-off run (every tick drawn in full): on every held tick under a toast, the toast's box is
 * the same pixels as there; everything in the band outside the toast (its rect grown by its shadow and sparkle) is
 * the band of the kept frame WITHOUT any toast (a hold-off run with every toast cleared, on the kept frame's tick: no ghost
 * of an earlier position, including the one the kept frame was drawn with); after the toast has gone the band is
 * clean again; and such ticks really happen while the toast moves. */
#define BW_MAX 320
static uint8_t band_a[TNF][18 * BW_MAX], band_b[TNF][18 * BW_MAX], band_c[TNF][18 * BW_MAX];
static int16_t tx_a[TNF], tw_a[TNF];
static uint8_t held_a[TNF], pend_a[TNF], steady_a[TNF];

static void band_run(int hold_off, const char *msg, uint8_t (*band)[18 * BW_MAX])
{
    int f, by = 0, bh = 0;
    doomo_init();
    test_reset();
    fp_island_hub_hold_off(hold_off);
    fp_new_island(TOWN_POP_PREMADE, 4242);
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
    for (f = 0; f < TNF; f++) {
        int before = fp_island_hub_held(), x, w;
        if (f == T0 && msg) ui_toast(msg);
        if (!msg) ui_toast_clear();                  /* the clean reference: no toast at all (the hub's own neither) */
        plat_begin_frame();
        doomo_frame(0);
        plat_end_frame();
        if (f == 0) CHECK(nat_strip_band(&by, &bh));    /* the band the hub keeps, at its toast lane */
        memcpy(band[f], &nat_fb[by * nat_w], (size_t)(bh * nat_w));
        if (!hold_off) {
            held_a[f] = (uint8_t)(fp_island_hub_held() > before);
            pend_a[f] = (uint8_t)ui_toast_pending();
            steady_a[f] = (uint8_t)(ui_toast_steady() != 0);
            if (ui_toast_last_rect(&x, NULL, &w, NULL)) { tx_a[f] = (int16_t)x; tw_a[f] = (int16_t)(w - 2); }
            else { tx_a[f] = -1000; tw_a[f] = 0; }
        }
    }
    fp_island_hub_hold_off(0);
}

static void toast_band_run(const char *msg, int w, int h)
{
    int f, g = -1, moving = 0, by, bh;
    test_set_res(w, h);
    band_run(0, msg, band_a);
    CHECK(nat_strip_band(&by, &bh));
    CHECK_EQ(bh, 18);
    band_run(1, msg, band_b);
    band_run(1, NULL, band_c);
    for (f = 0; f < TNF; f++) {
        int i, j, x0, x1, bad_box = 0, bad_bg = 0;
        if (!held_a[f]) { g = f; continue; }             /* drawn in full: the band copy is of this frame */
        if (g < 0 || f <= T0) continue;
        if (!tw_a[f]) {
            /* no toast drawn on this tick (one that was pending may draw its last slide-out tick and go): the band
             * is the kept frame's, clean */
            CHECK_MSG(memcmp(band_a[f], band_c[g], (size_t)(bh * nat_w)) == 0,
                      "%dx%d '%.12s': held tick %d without a toast: the band is not the clean one of tick %d", w, h,
                      msg, f, g);
            continue;
        }
        if (!steady_a[f]) moving++;
        /* the toast's opaque box (inside its rounded rim, left of its sparkle, which a steady toast on a kept frame
         * steps with the hub: I8): the same pixels as the all-full run's */
        for (j = 2; j < 14; j++)                          /* the box: rows lane + 1 .. lane + 12, the band from lane - 1 */
            for (i = DOOMO_MAX(tx_a[f] + 4, 0); i < DOOMO_MIN(tx_a[f] + tw_a[f] - 8, nat_w); i++)
                bad_box += band_a[f][j * nat_w + i] != band_b[f][j * nat_w + i];
        CHECK_MSG(!bad_box, "%dx%d '%.12s': held tick %d: %d toast pixels differ from the all-full run", w, h, msg, f,
                  bad_box);
        /* the band beside the toast (and its shadow and sparkle): the kept frame's without a toast. A steady toast
         * on the kept frame itself (I8: nothing is put back) is the one exception: it is where it was. */
        x0 = tx_a[f] - 4;
        x1 = tx_a[f] + tw_a[f] + 8;
        if (steady_a[f] && held_a[f] && tx_a[g] == tx_a[f]) continue;
        for (j = 0; j < bh; j++)
            for (i = 0; i < nat_w; i++)
                if (i < x0 || i >= x1) bad_bg += band_a[f][j * nat_w + i] != band_c[g][j * nat_w + i];
        CHECK_MSG(!bad_bg, "%dx%d '%.12s': held tick %d: %d band pixels beside the toast differ from the kept frame "
                  "without a toast (tick %d)", w, h, msg, f, bad_bg, g);
    }
    CHECK_MSG(moving >= 6, "%dx%d '%.12s': only %d held ticks while the toast moved", w, h, msg, moving);
}

TEST(island_hub_toast_slides_redraw_only_the_band)
{
    toast_band_run("Rent collected", 240, 160);
    toast_band_run("Rent collected", 320, 240);
    toast_band_run("NEW UNLOCK: a very long line that has to scroll across its pill", 240, 160);
    test_set_res(240, 160);
}
