/* test_ending.c - E1: the endings engine (core/src/ending.c): reach, seen
 * counters, the pending card, the finale and the 'ENDS' chunk round trip. */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/ending.h"
#include "doomo/life.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/scenes.h"
#include "doomo/story.h"
#include "doomo/town.h"
#include "doomo/roster.h"
#include "doomo/resident.h"
#include "doomo/playas.h"
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

static void all_sparkles(int on)
{
    int i;
    for (i = 0; i < 32; i++) story_set_flag(STORY_FLAG_SECRET_FIRST + i, on);
}

TEST(ending_e10_needs_every_sparkle_and_d2)
{
    int i;
    fresh();
    all_sparkles(0);
    story_episode_set_done(BEAT_D2, 0);
    ending_check_sparkles();
    CHECK(!ending_seen(ENDING_E10));
    for (i = 0; i < 32; i++) if (!(i >= 8 && i < 16)) story_set_flag(STORY_FLAG_SECRET_FIRST + i, 1);
    ending_check_sparkles();                            /* every sparkle, but D2 not played */
    CHECK(!ending_seen(ENDING_E10));
    CHECK_EQ(ending_resolve_finale(), ENDING_E01);
    story_episode_set_done(BEAT_D2, 1);
    story_set_flag(STORY_FLAG_SECRET_FIRST + 20, 0);    /* one missing */
    ending_check_sparkles();
    CHECK(!ending_seen(ENDING_E10));
    story_set_flag(STORY_FLAG_SECRET_FIRST + 20, 1);
    CHECK_EQ(ending_resolve_finale(), ENDING_E10);      /* secret outranks the good finale */
    ending_check_sparkles();
    CHECK(ending_seen(ENDING_E10));
    CHECK_EQ(ending_pending(), ENDING_E10);
    ending_take_pending();
    ending_check_sparkles();                            /* once only: no second card */
    CHECK_EQ(ending_pending(), ENDING_NONE);
    all_sparkles(0);
    story_episode_set_done(BEAT_D2, 0);
}

TEST(ending_e11_needs_new_game_plus_the_bottle_and_the_suitcase)
{
    fresh();
    story_set_flag(SF_NGPLUS, 0); story_set_flag(SF_BOTTLE_SEEN, 1); story_set_flag(SF_SUITCASE_OPEN, 1);
    CHECK_EQ(ending_resolve_finale(), ENDING_E01);      /* not a New Game+ */
    story_set_flag(SF_NGPLUS, 1); story_set_flag(SF_BOTTLE_SEEN, 0);
    CHECK_EQ(ending_resolve_finale(), ENDING_E01);      /* no bottle */
    story_set_flag(SF_BOTTLE_SEEN, 1); story_set_flag(SF_SUITCASE_OPEN, 0);
    CHECK_EQ(ending_resolve_finale(), ENDING_E01);      /* no suitcase */
    story_set_flag(SF_SUITCASE_OPEN, 1);
    CHECK_EQ(ending_resolve_finale(), ENDING_E11);
    ending_finale_reached();
    CHECK(ending_seen(ENDING_E11));
    story_set_flag(SF_NGPLUS, 0); story_set_flag(SF_BOTTLE_SEEN, 0); story_set_flag(SF_SUITCASE_OPEN, 0);
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

/* ---- the adult gate (R10): mature_adult_ok ---- */
TEST(adult_gate_refuses_everyone_it_must)
{
    int id;
    test_reset();
    town_new(TOWN_STORY, 5, TOWN_POP_PREMADE);
    CHECK_EQ(mature_adult_ok(-1), 0);
    CHECK_EQ(mature_adult_ok(DOOMO_MAX_RESIDENTS), 0);          /* the slot past the premades */
    CHECK_EQ(mature_adult_ok(DOOMO_PLAYER_ID), 0);              /* the hero is not a route participant */
    CHECK_EQ(mature_adult_ok(PLAYAS_ID_FAMILY_FIRST), 0);       /* a family pool member */
    CHECK_EQ(mature_adult_ok(DOOMO_MAX_RESIDENTS + 5000), 0);
    CHECK_EQ(mature_adult_ok(ROSTER_ID_PIP), 0);                /* a kid */
    CHECK_EQ(mature_adult_ok(563), 0);                          /* Wisp, the imp-kid ghost */
    /* the two route partners and a grown-up townsperson qualify */
    CHECK_EQ(mature_adult_ok(ROSTER_ID_GRUMBLEGUTS), 1);
    CHECK_EQ(mature_adult_ok(ROSTER_ID_VEX), 1);
    CHECK_EQ(mature_adult_ok(ROSTER_ID_MAYOR), 1);
    /* the property over all 1000 premades: a yes is always an adult, never a kid, teen or kid-cast */
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        if (!mature_adult_ok(id)) continue;
        CHECK(LIFE_IS_ADULT(RES_STAGE(town_res(id))));
        CHECK(!(roster_traits(id) & (ROSTER_T_KID | ROSTER_T_TEEN)));
        CHECK(!ending_kid_cast(id));
    }
    /* every KID and TEEN premade is a no */
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++)
        if (roster_traits(id) & (ROSTER_T_KID | ROSTER_T_TEEN)) CHECK_EQ(mature_adult_ok(id), 0);
}

TEST(adult_gate_follows_edits_to_the_slot)
{
    DoomoResident *r;
    test_reset();
    town_new(TOWN_STORY, 5, TOWN_POP_PREMADE);
    r = town_res(ROSTER_ID_VEX);
    CHECK_EQ(mature_adult_ok(ROSTER_ID_VEX), 1);
    resident_set_stage(r, LIFE_ELDER);                           /* an elder counts as an adult (LS0) */
    CHECK_EQ(mature_adult_ok(ROSTER_ID_VEX), 1);
    resident_set_stage(r, LIFE_TEEN);
    CHECK_EQ(mature_adult_ok(ROSTER_ID_VEX), 0);
    resident_set_stage(r, LIFE_CHILD);
    CHECK_EQ(mature_adult_ok(ROSTER_ID_VEX), 0);
    resident_set_stage(r, LIFE_ADULT);
    CHECK_EQ(mature_adult_ok(ROSTER_ID_VEX), 1);
    r->flags |= RF_CUSTOM;                                       /* player-made or edited */
    CHECK_EQ(mature_adult_ok(ROSTER_ID_VEX), 0);
    r->flags &= (uint8_t)~RF_CUSTOM;
    CHECK_EQ(mature_adult_ok(ROSTER_ID_VEX), 1);
    memcpy(r->name, "Somebody", 9);                             /* the slot no longer holds the premade */
    CHECK_EQ(mature_adult_ok(ROSTER_ID_VEX), 0);
    /* an island that did not start as the premade town has no premade citizens at all */
    test_reset();
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_EMPTY);
    CHECK_EQ(mature_adult_ok(ROSTER_ID_VEX), 0);
}

/* the compiled kid cast is the file (E5's core/data/story/kid_cast.txt): same ids both ways */
TEST(adult_gate_kid_cast_matches_the_file)
{
    char path[512], line[200];
    int from_file[DOOMO_MAX_RESIDENTS], n = 0, i, id;
    FILE *f;
    snprintf(path, sizeof path, "%s/../../data/story/kid_cast.txt", test_golden_dir);
    f = fopen(path, "r");
    CHECK(f != NULL);
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        if (!isdigit((unsigned char)line[0])) continue;       /* comments, @E, @scene */
        id = atoi(line);
        CHECK(id >= 0 && id < DOOMO_MAX_RESIDENTS);
        CHECK(ending_kid_cast(id));                           /* every id in the file is compiled in */
        if (n < DOOMO_MAX_RESIDENTS) from_file[n++] = id;
    }
    fclose(f);
    CHECK(n >= 2);
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {            /* and nothing compiled in that the file lacks */
        int in_file = 0;
        for (i = 0; i < n; i++) in_file |= from_file[i] == id;
        CHECK_EQ(ending_kid_cast(id), in_file);
    }
    CHECK_EQ(ending_kid_cast(-1), 0);
}

/* ---- E12: the daily check lets E09 arrive after the finale ------------------------------------ */
TEST(e12_daily_check_lets_bonejangles_bow_late)
{
    fresh();
    story_set_flag(SF_FEAST_DONE, 1);
    story_set_flag(SF_BONE_HEART, 0);
    ending_daily();                                     /* the Feast is over, no heart: nothing */
    CHECK(!ending_seen(ENDING_E09));
    story_set_flag(SF_BONE_HEART, 1);                   /* found afterwards, with no cutscene to check it */
    ending_daily();
    CHECK(ending_seen(ENDING_E09));
    CHECK_EQ(ending_pending(), ENDING_E09);
    ending_daily();                                     /* a second day does not count it twice */
    CHECK_EQ(ending_count_seen(), 1);
    story_set_flag(SF_FEAST_DONE, 0);
    story_set_flag(SF_BONE_HEART, 0);
}

TEST(e12_daily_check_waits_for_the_story_but_not_free_play)
{
    int mode = doomo_town.mode, ch = doomo_town.story.chapter;
    fresh();
    story_set_flag(SF_FEAST_DONE, 1);
    story_set_flag(SF_BONE_HEART, 1);
    doomo_town.mode = TOWN_STORY;
    doomo_town.story.chapter = STORY_CH3;               /* story not cleared */
    ending_daily();
    CHECK(!ending_seen(ENDING_E09));
    doomo_town.mode = TOWN_FREEPLAY;
    ending_daily();
    CHECK(ending_seen(ENDING_E09));
    doomo_town.mode = mode; doomo_town.story.chapter = ch;
    story_set_flag(SF_FEAST_DONE, 0);
    story_set_flag(SF_BONE_HEART, 0);
}
