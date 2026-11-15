/* test_busy.c - S2-10: plat_busy / plat_busy_step, the hooks that keep the screen alive while a save, a load or a
 * town build runs in one tick (plat_ext.h; owner: speed). The hooks here are strong and only count, so every
 * other test in this binary runs with them too: nothing they see may change a result.
 *
 * Also the cut-off tests of the streamed save: a write that stops at any point leaves the previous save as it
 * was (natively: the new file is written aside and renamed on commit, like the GBA's trailer written last
 * and the Dreamcast's replace). */
#include "test.h"
#include "doomo/save.h"
#include "doomo/plat_ext.h"

static int b_calls, b_kind, b_steps, b_bad, b_max_done, b_steps_before_busy, b_busy_calls_since;
static int b_keeps, b_back, b_last;      /* steps without progress; a bar that moved back; the last real step */

void plat_busy(int kind)
{
    b_calls++;
    b_busy_calls_since++;
    b_kind = kind;
    if (b_steps) b_steps_before_busy++;      /* a step that came before its busy call */
}

void plat_busy_step(int done, int total)
{
    b_steps++;
    if (done == PLAT_BUSY_KEEP) {                /* a sort pass, a run of relationships: the bar stays */
        b_keeps++;
        if (total != 0) b_bad++;
        return;
    }
    if (done < 0 || done > total || total != DOOMO_MAX_RESIDENTS) b_bad++;
    if (done > b_max_done) b_max_done = done;
    if (done < b_last) b_back++;
    b_last = done;
}

static void busy_reset(void)
{
    b_calls = b_kind = b_steps = b_bad = b_max_done = b_steps_before_busy = b_busy_calls_since = 0;
    b_keeps = b_back = b_last = 0;
}

extern int nat_save_stream, nat_stream_fail_at;
extern uint32_t nat_stream_writes;

TEST(busy_town_new_steps_every_citizen)
{
    busy_reset();
    town_new(TOWN_STORY, 1, TOWN_POP_PREMADE);
    CHECK_EQ(b_calls, 1);
    CHECK_EQ(b_kind, PLAT_BUSY_BUILD);
    /* one step per citizen built (never a long run without one), and a few more while the relationships sort */
    CHECK(b_steps >= DOOMO_MAX_RESIDENTS && b_steps <= 2 * DOOMO_MAX_RESIDENTS);
    CHECK_EQ(b_bad, 0);
    CHECK_EQ(b_max_done, DOOMO_MAX_RESIDENTS - 1);
    CHECK_EQ(b_steps_before_busy, 0);
    CHECK(b_keeps >= 8);                             /* the sort's passes step without moving the bar ... */
    CHECK_EQ(b_back, 0);                             /* ... so it only moves forward over a build */

    busy_reset();
    town_new(TOWN_FREEPLAY, 31337, TOWN_POP_RANDOM);
    CHECK_EQ(b_calls, 1);
    CHECK(b_steps >= DOOMO_MAX_RESIDENTS && b_steps <= 2 * DOOMO_MAX_RESIDENTS);
    CHECK_EQ(b_bad, 0);
    CHECK_EQ(b_back, 0);

    busy_reset();
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);       /* nothing to build: called (the platform decides if late) */
    CHECK_EQ(b_calls, 1);
    CHECK_EQ(b_kind, PLAT_BUSY_BUILD);
    CHECK_EQ(b_steps, 0);
}

static uint8_t keep[DOOMO_SAVE_SLOT1_MAX], now[DOOMO_SAVE_SLOT1_MAX];

TEST(busy_save_and_load_step_every_citizen)
{
    DoomoTown snap;
    int pass;
    for (pass = 0; pass < 2; pass++) {
        nat_save_stream = pass ? 4096 : 0;           /* 0: the whole image in one pass; else the GBA's two passes */
        town_new(TOWN_FREEPLAY, 5, TOWN_POP_PREMADE);
        resident_sanitize(&doomo_town.player);
        rel_normalize();
        snap = doomo_town;

        busy_reset();
        CHECK(save_game(1));
        CHECK_EQ(b_calls, 1);
        CHECK_EQ(b_kind, PLAT_BUSY_SAVE);
        CHECK_EQ(b_steps_before_busy, 0);
        CHECK(b_steps >= DOOMO_MAX_RESIDENTS * (pass ? 2 : 1));      /* every citizen, once per pass */
        CHECK(b_steps <= DOOMO_MAX_RESIDENTS * (pass ? 4 : 2));
        CHECK_EQ(b_bad, 0);
        CHECK_EQ(b_back, pass ? 1 : 0);              /* the bar fills once per pass of the codec */

        memset(&doomo_town, 0x5A, sizeof(doomo_town));
        busy_reset();
        CHECK(load_game(1));
        CHECK_EQ(b_calls, 1);
        CHECK_EQ(b_kind, PLAT_BUSY_LOAD);
        CHECK_EQ(b_steps_before_busy, 0);
        CHECK(b_steps >= DOOMO_MAX_RESIDENTS);       /* the applying decode rebuilds every citizen */
        CHECK_EQ(b_bad, 0);
        CHECK(b_back <= 1);                          /* the check pass, then the applying one; the sort keeps it */
        CHECK(memcmp(&snap.res, &doomo_town.res, sizeof(snap.res)) == 0);
    }
    nat_save_stream = 0;
}

/* the relationship graph is 0.2 s of encoding on the GBA before the first citizen: it steps too, every 32 edges */
TEST(busy_steps_cover_the_relationship_graph)
{
    DoomoRng r;
    int i, edges;
    town_new(TOWN_FREEPLAY, 3, TOWN_POP_PREMADE);
    rng_seed(&r, 5);
    for (i = 0; i < 1500; i++) rel_adjust(rng_range(&r, 1000), rng_range(&r, 1000), rng_between(&r, -40, 40));
    edges = doomo_town.edge_count;
    CHECK(edges > 1200);
    nat_save_stream = 0;
    busy_reset();
    CHECK(save_game(1));
    CHECK(b_steps >= DOOMO_MAX_RESIDENTS + edges / 32);
    busy_reset();
    CHECK(load_game(1));
    CHECK(b_steps >= DOOMO_MAX_RESIDENTS + edges / 32);
    CHECK(b_keeps >= edges / 32);                    /* the relationships step without moving the bar */
    busy_reset();
    rel_normalize();                                 /* the sort alone: a step before each of its 8 passes */
    CHECK(b_steps >= 8);
    CHECK_EQ(b_keeps, b_steps);
}

TEST(busy_a_missing_save_does_not_start_a_busy_card)
{
    nat_save_stream = 0;
    plat_save_erase(1);
    busy_reset();
    CHECK(!load_game(1));
    CHECK_EQ(b_calls, 0);                            /* nothing to wait for: no card for a quick "no save" */
    CHECK_EQ(b_steps, 0);
    nat_save_stream = 4096;
    busy_reset();
    CHECK(!load_game(1));
    CHECK_EQ(b_calls, 0);
    nat_save_stream = 0;
}

static uint32_t slot_image(int slot, uint8_t *buf)
{
    uint32_t size = 0;
    if (!plat_save_read(slot, buf, DOOMO_SAVE_SLOT1_MAX, &size)) return 0;
    return size;
}

static void island(uint32_t seed)
{
    town_new(TOWN_FREEPLAY, seed, TOWN_POP_RANDOM);
    resident_sanitize(&doomo_town.player);
    rel_normalize();
}

/* a save cut off at the first write, in the middle and at the last one: the previous save is untouched,
 * byte for byte, and loads; the next complete save then replaces it */
TEST(save_cut_at_any_write_keeps_the_previous_save)
{
    uint32_t n0, n;
    DoomoTown a;
    int writes, cuts[7], k;
    nat_save_stream = 512;
    island(11);
    a = doomo_town;
    CHECK(save_game(1));
    n0 = slot_image(1, keep);
    CHECK(n0 > 4 * 512);
    island(12);                                    /* the island that is saved over it */
    nat_stream_writes = 0;
    CHECK(save_game(1));                           /* a clean run counts the writes */
    writes = (int)nat_stream_writes;
    CHECK(writes > 8);
    n = slot_image(1, now);
    CHECK(n != n0 || memcmp(keep, now, n0) != 0);  /* it really replaced the first one */
    CHECK(plat_save_write(1, keep, n0));           /* the first one back in the slot */
    cuts[0] = 1; cuts[1] = 2; cuts[2] = 3; cuts[3] = writes / 4; cuts[4] = writes / 2; cuts[5] = writes - 1;
    cuts[6] = writes;
    for (k = 0; k < 7; k++) {
        island(12);
        nat_stream_fail_at = cuts[k];
        CHECK_MSG(!save_game(1), "a save cut at write %d of %d reported success", cuts[k], writes);
        nat_stream_fail_at = 0;
        n = slot_image(1, now);
        CHECK_EQ(n, n0);
        CHECK_MSG(memcmp(keep, now, n0) == 0, "the previous save changed after a cut at write %d", cuts[k]);
        CHECK(save_verify(1));
        memset(&doomo_town, 0x5A, sizeof(doomo_town));
        CHECK(load_game(1));
        CHECK(memcmp(&a.res, &doomo_town.res, sizeof(a.res)) == 0);
    }
    island(12);                                    /* a complete save after all those cuts works */
    CHECK(save_game(1));
    CHECK(save_verify(1));
    n = slot_image(1, now);
    CHECK(n != n0 || memcmp(keep, now, n0) != 0);
    nat_save_stream = 0;
}
