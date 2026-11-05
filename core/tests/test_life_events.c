/* test_life_events.c - LS3, the life event engine (core/src/life_events.c), and E3's final
 * good-ending rule (ending_resolve_finale: E01 vs E02). */
#include <string.h>
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/life.h"
#include "doomo/life_events.h"
#include "doomo/family.h"
#include "doomo/ending.h"
#include "doomo/items.h"
#include "doomo/town.h"
#include "doomo/sim.h"

static void fresh(void)
{
    town_new(TOWN_FREEPLAY, 41, TOWN_POP_PREMADE);   /* free play: no story wait; resets LIFE */
    sim_init();
}

TEST(life_events_need_a_household)
{
    int i, d;
    fresh();
    CHECK_EQ(life_event_total(), 0);
    for (i = 0; i < LEV_COUNT; i++) CHECK_EQ(life_event_can(i), 0);   /* nobody home: nothing runs */
    for (d = 0; d < 200; d++) CHECK_EQ(life_events_daily(), -1);
    CHECK_EQ(life_event_total(), 0);
    CHECK_EQ(life_events_save_size(), 0);                              /* and nothing is saved */
    CHECK_EQ(life_event_fire(LEV_LUCKY_FIND), 0);
    CHECK_EQ(life_event_fire(-1), 0);
    CHECK_EQ(life_event_fire(LEV_COUNT), 0);
}

TEST(life_events_conditions_follow_the_household)
{
    int a, k;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(a >= 0);
    CHECK(life_event_can(LEV_LUCKY_FIND) && life_event_can(LEV_LETTER));
    CHECK(life_event_can(LEV_PICNIC) && life_event_can(LEV_STARGAZING) && life_event_can(LEV_BAD_DAY));
    CHECK(!life_event_can(LEV_POTLUCK));          /* needs two housemates */
    CHECK(!life_event_can(LEV_SURPRISE_PARTY));   /* needs a child or a teen */
    CHECK(!life_event_can(LEV_ANNIVERSARY));      /* needs a partner */
    k = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    CHECK(k >= 0);
    CHECK(life_event_can(LEV_POTLUCK) && life_event_can(LEV_RAINY_DAY));
    CHECK(life_event_can(LEV_SURPRISE_PARTY));
}

TEST(life_events_effects_and_cooldowns)
{
    int a, k, n0, m0, b0;
    uint32_t h0;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    k = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    CHECK(a >= 0 && k >= 0);
    h0 = doomo_town.money;
    CHECK_EQ(life_event_fire(LEV_LUCKY_FIND), 1);
    CHECK(doomo_town.money >= h0 + 20u && doomo_town.money <= h0 + 60u);
    CHECK_EQ(life_event_last(), LEV_LUCKY_FIND);
    CHECK(life_event_cooldown(LEV_LUCKY_FIND) > 0);
    CHECK_EQ(life_event_fire(LEV_LUCKY_FIND), 0);                  /* cooling down */
    CHECK_EQ(life_event_total(), 1);
    n0 = inv_count(ITEM_FLOWERS);
    CHECK_EQ(life_event_fire(LEV_LETTER), 1);
    CHECK_EQ(inv_count(ITEM_FLOWERS), n0 + 1);
    n0 = inv_count(ITEM_CUPCAKE);
    m0 = family_mood(k); b0 = family_bond(k);
    CHECK_EQ(life_event_fire(LEV_SURPRISE_PARTY), 1);              /* the only child */
    CHECK_EQ(inv_count(ITEM_CUPCAKE), n0 + 1);
    CHECK(family_mood(k) > m0 && family_bond(k) == b0 + 1);
    m0 = family_mood(a);
    CHECK_EQ(life_event_fire(LEV_POTLUCK), 1);
    CHECK(family_mood(a) > m0);
    CHECK_EQ(life_event_total(), 4);
}

TEST(life_events_a_rotten_day_is_only_words_and_a_mood_dip)
{
    int a, m0, b0;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(a >= 0);
    m0 = family_mood(a); b0 = family_bond(a);
    CHECK_EQ(life_event_fire(LEV_BAD_DAY), 1);
    CHECK(family_mood(a) < m0);
    CHECK_EQ(family_bond(a), b0);                                  /* the bond is untouched */
    CHECK_EQ(family_count(), 1);                                   /* nobody is lost */
}

TEST(life_events_daily_cooldowns_tick_and_it_fires_sometimes)
{
    int a, d, fired = 0, ev;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(a >= 0);
    CHECK_EQ(life_event_fire(LEV_PICNIC), 1);
    ev = life_event_cooldown(LEV_PICNIC);
    CHECK(ev > 0);
    for (d = 0; d < 400; d++) {
        int r = life_events_daily();
        if (r >= 0) { fired++; CHECK(r < LEV_COUNT); }
    }
    CHECK(fired > 40 && fired < 200);                              /* about LEV_DAILY_PCT of 400 */
    CHECK_EQ(life_event_total(), fired + 1);
}

TEST(life_events_wait_for_the_story_but_not_in_free_play)
{
    int a, d, n = 0;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(a >= 0);
    doomo_town.mode = TOWN_STORY;                                  /* the story is not cleared */
    for (d = 0; d < 200; d++) n += life_events_daily() >= 0;
    CHECK_EQ(n, 0);
    doomo_town.mode = TOWN_FREEPLAY;
    for (d = 0; d < 200; d++) n += life_events_daily() >= 0;
    CHECK(n > 0);
}

TEST(life_events_subconscious_is_silent_but_still_happens)
{
    int a;
    uint32_t h0;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(a >= 0);
    family_freewill_set(FAMILY_WILL_SUBCONSCIOUS);
    h0 = doomo_town.money;
    CHECK_EQ(life_event_fire(LEV_LUCKY_FIND), 1);
    CHECK(doomo_town.money > h0);
}

TEST(life_events_save_round_trip_and_forward_compat)
{
    uint8_t buf[64], big[64];
    uint32_t n;
    int a, t, c;
    fresh();
    CHECK_EQ(life_events_save_size(), 0);
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(a >= 0);
    CHECK_EQ(life_event_fire(LEV_PICNIC), 1);
    CHECK_EQ(life_event_fire(LEV_LETTER), 1);
    t = life_event_total(); c = life_event_cooldown(LEV_PICNIC);
    n = life_events_save_size();
    CHECK_EQ(n, 2u + LEV_COUNT);
    CHECK_EQ(life_events_save(buf, sizeof(buf)), n);
    life_events_clear();
    CHECK_EQ(life_event_total(), 0);
    CHECK(life_events_load(buf, n));
    CHECK_EQ(life_event_total(), t);
    CHECK_EQ(life_event_cooldown(LEV_PICNIC), c);
    /* an older save (fewer events) loads; a newer one (more) is read as far as we know */
    CHECK(life_events_load(buf, 4));
    CHECK_EQ(life_event_total(), t);
    memcpy(big, buf, n); memset(big + n, 9, sizeof(big) - n);
    CHECK(life_events_load(big, sizeof(big)));
    CHECK_EQ(life_event_total(), t);
    CHECK(!life_events_load(buf, 1));                               /* truncated: rejected */
    /* it rides the LIFE chunk */
    life_events_clear();
    CHECK(life_event_fire(LEV_LETTER) == 1);
    n = doomo_save_ext_life->size();
    CHECK(n > 0 && n <= doomo_save_ext_life->max_size);
}

TEST(finale_e02_needs_a_household_and_a_close_tie_and_no_neglect)
{
    int a, b, k, i;
    fresh();
    CHECK_EQ(ending_resolve_finale(), ENDING_E01);                 /* nobody home */
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(a >= 0);
    CHECK_EQ(ending_resolve_finale(), ENDING_E01);                 /* household of 2: not yet */
    k = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    CHECK(k >= 0);
    CHECK_EQ(ending_resolve_finale(), ENDING_E02);                 /* 3 members + a child */
    b = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 7u, "Bea", PRONOUN_SHE);
    CHECK(b >= 0);
    CHECK_EQ(ending_resolve_finale(), ENDING_E02);
    /* a member lost to neglect spoils it */
    CHECK(family_lose(b, FAMLOSS_NEGLECT));
    for (i = 0; i < 2; i++) { /* still 3+ members, but one neglect death */ }
    {
        LifeEndingFacts f;
        life_ending_facts(&f);
        CHECK_EQ(f.neglect_deaths, 1);
    }
    CHECK_EQ(ending_resolve_finale(), ENDING_E01);
}

/* ---- LS4: six more events (bedtime story, homework, old stories, pet trick, movie night, bake-off) ---- */
TEST(ls4_conditions_follow_the_household)
{
    int a, k, e, p;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(a >= 0);
    CHECK(!life_event_can(LEV_BEDTIME_STORY) && !life_event_can(LEV_HOMEWORK_HELP));   /* no kid yet */
    CHECK(!life_event_can(LEV_OLD_STORIES) && !life_event_can(LEV_PET_TRICK));
    CHECK(!life_event_can(LEV_MOVIE_NIGHT) && !life_event_can(LEV_BAKE_OFF));          /* needs two */
    k = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    CHECK(k >= 0);
    CHECK(life_event_can(LEV_BEDTIME_STORY) && life_event_can(LEV_HOMEWORK_HELP));
    CHECK(life_event_can(LEV_MOVIE_NIGHT) && life_event_can(LEV_BAKE_OFF));
    e = family_pool_add(FAMROLE_GRANDPARENT, LIFE_ELDER, 7u, "Gran", PRONOUN_SHE);
    CHECK(e >= 0 && life_event_can(LEV_OLD_STORIES));
    p = family_pool_add(FAMROLE_PET, LIFE_ADULT, 8u, "Fang", PRONOUN_THEY);
    CHECK(p >= 0 && life_event_can(LEV_PET_TRICK));
}

TEST(ls4_effects_are_kind_and_land_on_the_right_member)
{
    int a, k, e, p, m0, b0, n0;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    k = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    e = family_pool_add(FAMROLE_GRANDPARENT, LIFE_ELDER, 7u, "Gran", PRONOUN_SHE);
    p = family_pool_add(FAMROLE_PET, LIFE_ADULT, 8u, "Fang", PRONOUN_THEY);
    CHECK(a >= 0 && k >= 0 && e >= 0 && p >= 0);
    m0 = family_mood(k); b0 = family_bond(k);
    CHECK_EQ(life_event_fire(LEV_BEDTIME_STORY), 1);
    CHECK(family_mood(k) > m0 && family_bond(k) == b0 + 1);          /* only the child */
    b0 = family_bond(k);
    CHECK_EQ(life_event_fire(LEV_HOMEWORK_HELP), 1);
    CHECK(family_bond(k) >= b0 + 1);
    m0 = family_mood(e); b0 = family_bond(e);
    CHECK_EQ(life_event_fire(LEV_OLD_STORIES), 1);
    CHECK(family_mood(e) > m0 && family_bond(e) == b0 + 1);          /* the elder, not another */
    m0 = family_mood(p);
    CHECK_EQ(life_event_fire(LEV_PET_TRICK), 1);
    CHECK(family_mood(p) > m0);
    m0 = family_mood(a);
    CHECK_EQ(life_event_fire(LEV_MOVIE_NIGHT), 1);
    CHECK(family_mood(a) > m0);
    n0 = inv_count(ITEM_CUPCAKE);
    CHECK_EQ(life_event_fire(LEV_BAKE_OFF), 1);
    CHECK_EQ(inv_count(ITEM_CUPCAKE), n0 + 1);
    CHECK_EQ(family_count(), 4);                                     /* nobody is lost */
    CHECK_EQ(life_event_total(), 6);
}

TEST(ls4_old_saves_without_the_new_cooldowns_still_load)
{
    uint8_t old[2u + 10u];
    int a;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(a >= 0);
    memset(old, 0, sizeof(old));
    old[0] = 3;                                                      /* total 3, 10 old cooldowns */
    old[2 + LEV_PICNIC] = 2;
    CHECK_EQ(life_events_load(old, sizeof(old)), 1);
    CHECK_EQ(life_event_total(), 3);
    CHECK_EQ(life_event_cooldown(LEV_PICNIC), 2);
    CHECK_EQ(life_event_cooldown(LEV_BAKE_OFF), 0);                  /* missing bytes read as 0 */
}

/* LS4b: the F9 household helpers are life events and a script hook. */
TEST(life_events_helping_hand_and_away_trip)
{
    int a, k, m0;
    fresh();
    CHECK_EQ(life_event_can(LEV_HELPING_HAND), 0);
    CHECK_EQ(life_event_can(LEV_AWAY_TRIP), 0);
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    k = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    CHECK(a >= 0 && k >= 0);
    CHECK_EQ(family_helper_count(0), 0);                       /* bond is 3: nobody helps yet */
    CHECK_EQ(life_event_can(LEV_HELPING_HAND), 0);
    family_bond_add(k, 3);                                     /* a child at bond 5+: can help, cannot head out */
    CHECK_EQ(family_helper_count(0), 1);
    CHECK_EQ(family_helper_count(1), 0);
    CHECK(life_event_can(LEV_HELPING_HAND));
    CHECK_EQ(life_event_can(LEV_AWAY_TRIP), 0);
    CHECK_EQ(family_helper_pick(0), k);
    CHECK_EQ(family_helper_pick(1), -1);
    CHECK_EQ(life_event_fire(LEV_HELPING_HAND), 1);            /* a gift in the bag */
    CHECK_EQ(life_event_cooldown(LEV_HELPING_HAND), 5);
    CHECK_EQ(life_event_fire(LEV_HELPING_HAND), 0);            /* cooling down */
    family_bond_add(a, 3);                                     /* now an adult can head out */
    CHECK_EQ(family_helper_count(1), 1);
    CHECK(life_event_can(LEV_AWAY_TRIP));
    m0 = (int)doomo_town.money;
    CHECK_EQ(life_event_fire(LEV_AWAY_TRIP), 1);
    CHECK_EQ(family_away_id(), a);
    CHECK_EQ(life_event_can(LEV_AWAY_TRIP), 0);                /* one trip at a time */
    CHECK_EQ((int)doomo_town.money, m0);                       /* nothing yet: they come back tomorrow */
    CHECK_EQ(family_away_return(), 1);
    CHECK_EQ(family_away_id(), -1);
    /* the avatar never helps */
    CHECK_EQ(family_help_call(a), 1);                          /* head out again via the script hook */
    CHECK_EQ(family_away_id(), a);
    CHECK_EQ(family_help_call(9999), 0);                       /* nobody there */
    CHECK_EQ(family_help_call(k), 1);                          /* a child helps at once */
}
