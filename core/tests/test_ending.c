/* test_ending.c - E1: the endings engine (core/src/ending.c): reach, seen
 * counters, the pending card, the finale and the 'ENDS' chunk round trip. */
#include <string.h>
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/ending.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/scenes.h"

static void fresh(void) { doomo_save_ext_endings->reset(); }

TEST(ending_default_state_writes_no_chunk)
{
    fresh();
    CHECK_EQ(doomo_save_ext_endings->size(), 0u);
    CHECK_EQ(ending_count_seen(), 0);
    CHECK_EQ(ending_pending(), ENDING_NONE);
    CHECK(!scene_is_reserved_stub(&SCENE_ENDING_CARD));
}

TEST(ending_reach_marks_counts_and_queues_the_card)
{
    fresh();
    CHECK_EQ(ending_reach(ENDING_E03), 1);
    CHECK_EQ(ending_reach(ENDING_E03), 0);          /* not new the second time */
    CHECK(ending_seen(ENDING_E03));
    CHECK_EQ(ending_state()->times[ENDING_E03], 2);
    CHECK_EQ(ending_count_seen(), 1);
    CHECK_EQ(ending_pending(), ENDING_E03);
    CHECK_EQ(ending_take_pending(), ENDING_E03);
    CHECK_EQ(ending_pending(), ENDING_NONE);
    CHECK_EQ(ending_reach(99), 0);                  /* out of range is ignored */
}

TEST(ending_bonus_never_counts_in_the_14)
{
    fresh();
    ending_reach(ENDING_NOPE);
    CHECK_EQ(ending_count_seen(), 0);
    CHECK(ending_visible(ENDING_NOPE));
}

TEST(ending_finale_reaches_the_default_good_ending)
{
    fresh();
    ending_finale_reached();
    CHECK(ending_seen(ENDING_E01));
    CHECK(ending_state()->flags & ENDF_FINALE_DONE);
}

TEST(ending_chunk_round_trips_within_budget)
{
    uint8_t buf[64];
    uint32_t n;
    fresh();
    ending_reach(ENDING_E01);
    ending_reach(ENDING_E05);
    n = doomo_save_ext_endings->write(buf, sizeof buf);
    CHECK_EQ(n, 32u);
    CHECK(n <= SAVE_EXT_MAX_ENDS);
    fresh();
    CHECK(doomo_save_ext_endings->read(buf, n, ENDS_VERSION));
    CHECK(ending_seen(ENDING_E01) && ending_seen(ENDING_E05));
    CHECK_EQ(ending_count_seen(), 2);
    CHECK_EQ(ending_pending(), ENDING_E05);
}

TEST(ending_family_check_needs_partner_and_eternal_house)
{
    fresh();
    ending_check_family();                 /* no partner, no house: nothing */
    CHECK(!ending_seen(ENDING_E04));
}
