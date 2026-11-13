/* test_levels2.c - LV10 (owner: levels): the skeleton every new level's tests grow from. Today it
 * covers the 7 existing levels; a level added to the list (LV_COUNT, later the LV0 registry) is
 * covered by the same loops with no new line here.
 *   - the autopilot (LVF_DEMO, through the real SCENE_BREACH) finishes every level at Plushie in
 *     Hell Breach and Time Trial mode within a tick cap, and the demo records nothing.
 *   - LEVELS2_SWEEP=1 (not in the normal run: 84 runs of up to 12 game minutes) plays the whole
 *     plan matrix (4 modes x Plushie / Normal / Ultra-Hellvile) and prints it: the autopilot does not
 *     finish every cell yet (the two challenge modes need a weapon choice it lacks; the Ultra-Hellvile
 *     and Normal cells of some levels stall); docs/LEVELS.md keeps the measured matrix. When the
 *     autopilot gets there (LV6), move the cell into the asserted set below.
 *   - tools/breach_scripts.py refuses to run when its list and LV_COUNT differ, so a level cannot
 *     ship without its generated scripts.
 * Add a level-specific block below when a level brings a mechanic. */
#include <stdlib.h>
#include "test.h"
#include "../src/levels_int.h"

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

#define DEMO_CAP (30 * 60 * 12)          /* ticks: the autopilot's cap per run (12 game minutes) */

/* the demo of one level in one mode: the ticks it took, or -1 when it was still playing at the cap */
static int demo_run(int lv, int mode, int diff)
{
    int i;
    opt_set(OPT_DIFFICULTY, diff);
    opt_set(OPT_CHEAT_GOD_MODE, 1);
    rng_seed(&doomo_rng, 11 + lv);
    scene_set(&SCENE_TITLE, 0);
    frame(0);
    scene_push(&SCENE_BREACH, LV_ARG(lv, mode) | LVF_DEMO);
    for (i = 0; i < DEMO_CAP && scene_top() == &SCENE_BREACH; i++) frame(0);
    opt_set(OPT_CHEAT_GOD_MODE, 0);
    return scene_top() == &SCENE_BREACH ? -1 : i;
}

TEST(levels2_autopilot_finishes_every_level_at_plushie)
{
    int lv, mode;
    for (lv = 0; lv < LV_COUNT; lv++)
        for (mode = LVM_NORMAL; mode <= LVM_TIME_TRIAL; mode++) {
            int ticks = demo_run(lv, mode, DIFF_PLUSHIE);
            CHECK_MSG(ticks >= 0, "%s, %s: the autopilot is still playing after %d ticks",
                      lv_info[lv].title, lv_mode_names[mode], DEMO_CAP);
            CHECK_MSG(scene_top() == &SCENE_TITLE, "%s, %s: the demo did not return to the scene below",
                      lv_info[lv].title, lv_mode_names[mode]);
            CHECK_EQ(lv_rank(lv), RANK_NONE);                  /* a demo records nothing */
        }
    opt_set(OPT_DIFFICULTY, DIFF_NORMAL);
}

/* the whole matrix, printed (env LEVELS2_SWEEP=1): ticks to the exit, "-" when the cap hit first */
TEST(levels2_sweep)
{
    static const uint8_t diffs[3] = { DIFF_PLUSHIE, DIFF_NORMAL, DIFF_ULTRA_HELLVILE };
    int lv, mode, d, done = 0, total = 0;
    if (!getenv("LEVELS2_SWEEP")) return;
    for (lv = 0; lv < LV_COUNT; lv++) {
        printf("      %-18s", lv_info[lv].title);
        for (mode = 0; mode < LVM_COUNT; mode++)
            for (d = 0; d < 3; d++) {
                int t = demo_run(lv, mode, diffs[d]);
                total++;
                done += t >= 0;
                if (t >= 0) printf(" %5d", t); else printf("     -");
            }
        printf("\n");
    }
    printf("      autopilot finished %d of %d cells (modes: Hell Breach, Time Trial, Cute-Only, Brutal-Only; "
           "each at Plushie, Normal, Ultra)\n", done, total);
    opt_set(OPT_DIFFICULTY, DIFF_NORMAL);
}
