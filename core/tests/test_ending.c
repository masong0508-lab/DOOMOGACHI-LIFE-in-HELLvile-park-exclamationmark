/* test_ending.c - E1: the endings engine (core/src/ending.c): reach, seen
 * counters, the pending card, the finale and the 'ENDS' chunk round trip. */
#include <string.h>
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/ending.h"
#include "doomo/life.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/scenes.h"
#include "doomo/story.h"
#include "doomo/map.h"
#include "../src/story_int.h"

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
    CHECK_EQ(ending_reach(ENDING_E06), 1);
    CHECK_EQ(ending_reach(ENDING_E06), 0);          /* not new the second time */
    CHECK(ending_seen(ENDING_E06));
    CHECK_EQ(ending_state()->times[ENDING_E06], 2);
    CHECK_EQ(ending_count_seen(), 1);
    CHECK_EQ(ending_pending(), ENDING_E06);
    CHECK_EQ(ending_take_pending(), ENDING_E06);
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
    ending_reach(ENDING_E08);
    n = doomo_save_ext_endings->write(buf, sizeof buf);
    CHECK_EQ(n, 32u);
    CHECK(n <= SAVE_EXT_MAX_ENDS);
    fresh();
    CHECK(doomo_save_ext_endings->read(buf, n, ENDS_VERSION));
    CHECK(ending_seen(ENDING_E01) && ending_seen(ENDING_E08));
    CHECK_EQ(ending_count_seen(), 2);
    CHECK_EQ(ending_pending(), ENDING_E08);
}

TEST(ending_family_check_needs_partner_and_eternal_house)
{
    fresh();
    ending_check_family();                 /* no partner, no house: nothing */
    CHECK(!ending_seen(ENDING_E04));
}

TEST(ending_finale_is_e01_for_an_empty_household_and_facts_are_sane)
{
    LifeEndingFacts f;
    fresh();
    life_ending_facts(&f);
    CHECK_EQ(f.partner, -1);
    CHECK_EQ(f.neglect_deaths, 0);
    CHECK_EQ(ending_resolve_finale(), ENDING_E01);
}

TEST(ending_retired_slots_never_trigger_count_or_show)
{
    fresh();
    CHECK_EQ(ENDING_ACTIVE_COUNT, 11);
    CHECK_EQ(ending_reach(ENDING_E03), 0);
    CHECK_EQ(ending_reach(ENDING_E05), 0);
    CHECK_EQ(ending_reach(ENDING_E07), 0);
    CHECK(!ending_seen(ENDING_E03) && !ending_seen(ENDING_E05) && !ending_seen(ENDING_E07));
    CHECK_EQ(ending_count_seen(), 0);
    CHECK_EQ(ending_pending(), ENDING_NONE);
    CHECK(!ending_visible(ENDING_E03) && !ending_visible(ENDING_E05) && !ending_visible(ENDING_E07));
    CHECK(ending_visible(ENDING_E06));                 /* a live one is still listed */
}

TEST(ending_e09_needs_the_feast_over_and_the_heart_found)
{
    fresh();
    story_set_flag(SF_FEAST_DONE, 0);
    story_set_flag(SF_BONE_HEART, 0);
    ending_check_bonejangles();                         /* neither */
    CHECK(!ending_seen(ENDING_E09));
    story_set_flag(SF_FEAST_DONE, 1);
    ending_check_bonejangles();                         /* the Feast alone is not enough */
    CHECK(!ending_seen(ENDING_E09));
    story_set_flag(SF_FEAST_DONE, 0);
    story_set_flag(SF_BONE_HEART, 1);
    ending_check_bonejangles();                         /* nor the heart alone */
    CHECK(!ending_seen(ENDING_E09));
    story_set_flag(SF_FEAST_DONE, 1);
    ending_check_bonejangles();                         /* both: E09, queued for its card */
    CHECK(ending_seen(ENDING_E09));
    CHECK_EQ(ending_pending(), ENDING_E09);
    CHECK_EQ(ending_count_seen(), 1);
    story_set_flag(SF_FEAST_DONE, 0);
    story_set_flag(SF_BONE_HEART, 0);
}

TEST(ending_e09_feast_cutscene_end_sets_the_flag_only_once_the_story_is_done)
{
    int saved = story_beat();
    fresh();
    story_set_flag(SF_FEAST_DONE, 0);
    story_set_flag(SF_BONE_HEART, 1);
    story_set_beat(BEAT_C4_FESTIVAL);
    feast_cut_done(CUT_FEAST_POP);                      /* story not complete yet */
    CHECK(!story_flag(SF_FEAST_DONE));
    CHECK(!ending_seen(ENDING_E09));
    story_set_beat(BEAT_DONE);
    feast_cut_done(CUT_CHAPTER1);                       /* some other cutscene */
    CHECK(!story_flag(SF_FEAST_DONE));
    feast_cut_done(CUT_FEAST_POP);                      /* the Feast ends, heart in hand */
    CHECK(story_flag(SF_FEAST_DONE));
    CHECK(ending_seen(ENDING_E09));
    CHECK_EQ(ending_pending(), ENDING_E09);
    fresh();
    story_set_flag(SF_FEAST_DONE, 0);
    story_set_flag(SF_BONE_HEART, 0);
    feast_cut_done(CUT_FEAST_POP);                      /* no heart: the Feast is done, no E09 */
    CHECK(story_flag(SF_FEAST_DONE));
    CHECK(!ending_seen(ENDING_E09));
    story_set_flag(SF_FEAST_DONE, 0);
    story_set_beat(saved);
}

TEST(ending_e09_the_heart_is_hidden_once_in_bone_cathedral_only)
{
    int id, i, hearts = 0;
    for (id = 0; id < MAPID_COUNT; id++) {
        const DoomoMapDef *d = doomo_maps[id];
        if (!d) continue;
        for (i = 0; i < d->thing_count; i++)
            if (d->things[i].flags & THF_MACHINE_HEART) {
                CHECK_EQ(id, MAPID_BONE_CATHEDRAL);
                CHECK_EQ(d->things[i].type, THING_SCRAP);
                CHECK(!(d->things[i].flags & THF_SECRET));   /* does not shift the secret count */
                hearts++;
            }
    }
    CHECK_EQ(hearts, 1);
}
