/* test_wave6.c - W6: PA9 (the play-as romance gate) and E4 (routes, bad and sexy endings). */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/ending.h"
#include "doomo/family.h"
#include "doomo/life.h"
#include "doomo/playas.h"
#include "doomo/roster.h"
#include "doomo/resident.h"
#include "doomo/sim.h"
#include "doomo/town.h"
#include "doomo/save_ext.h"
#include "doomo/options_store.h"

static void fresh6(void)
{
    test_reset();
    town_new(TOWN_STORY, 5, TOWN_POP_PREMADE);
    doomo_save_ext_endings->reset();
}

/* ---- PA9 ---- */
TEST(pa9_matches_the_sim_away_from_the_player)
{
    int a = ROSTER_ID_GRUMBLEGUTS, b = ROSTER_ID_VEX;
    fresh6();
    CHECK_EQ(playas_romance_ok(a, b), sim_romance_ok(a, b));
    CHECK_EQ(playas_romance_ok(b, a), sim_romance_ok(b, a));
    CHECK_EQ(playas_romance_ok(a, a), 0);
}

TEST(pa9_never_lets_a_kid_or_teen_near_the_player)
{
    int id;
    fresh6();
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++)
        if (roster_traits(id) & (ROSTER_T_KID | ROSTER_T_TEEN)) {
            CHECK_EQ(playas_romance_ok(DOOMO_PLAYER_ID, id), 0);
            CHECK_EQ(playas_romance_ok(id, DOOMO_PLAYER_ID), 0);
        }
}

TEST(pa9_player_pairs_need_both_sides_to_accept)
{
    DoomoResident *r;
    fresh6();
    r = town_res(ROSTER_ID_VEX);
    r->romance = (uint8_t)((r->romance & ~ROMANCE_PREF_MASK) | ROMANCE_ANY);
    doomo_town.player.romance = (uint8_t)((doomo_town.player.romance & ~ROMANCE_PREF_MASK) | ROMANCE_ANY);
    CHECK_EQ(playas_romance_ok(DOOMO_PLAYER_ID, ROSTER_ID_VEX), 1);
    CHECK_EQ(playas_romance_ok(ROSTER_ID_VEX, DOOMO_PLAYER_ID), 1);
    r->romance = (uint8_t)((r->romance & ~ROMANCE_PREF_MASK) | ROMANCE_NONE);     /* she is not open to it */
    CHECK_EQ(playas_romance_ok(DOOMO_PLAYER_ID, ROSTER_ID_VEX), 0);
    CHECK_EQ(playas_romance_ok(ROSTER_ID_VEX, DOOMO_PLAYER_ID), 0);
}

/* ---- E4: routes ---- */
TEST(e4_route_meter_offers_then_dates_and_refusal_sticks)
{
    fresh6();
    CHECK_EQ(ending_route_stage(ROUTE_VEX), RSTAGE_NONE);
    CHECK_EQ(ending_route_add(ROUTE_VEX, 30), 30);
    CHECK_EQ(ending_route_stage(ROUTE_VEX), RSTAGE_NONE);
    ending_route_add(ROUTE_VEX, 20);
    CHECK_EQ(ending_route_stage(ROUTE_VEX), RSTAGE_OFFERED);
    ending_route_add(ROUTE_VEX, 100);
    CHECK_EQ(ending_route_stage(ROUTE_VEX), RSTAGE_DATING);
    CHECK_EQ(ending_route_add(ROUTE_VEX, 255), 255);                   /* clamps */
    CHECK_EQ(ending_route_add(99, 5), 0);                               /* bad route ignored */
    CHECK_EQ(ending_route_set(ROUTE_GRUMBLEGUTS, RSTAGE_REFUSED, -1), 1);
    CHECK_EQ(ending_route_add(ROUTE_GRUMBLEGUTS, 200), 0);              /* refused stays refused */
    CHECK_EQ(ending_route_set(ROUTE_GRUMBLEGUTS, RSTAGE_DATING, -1), 0);
}

TEST(e4_route_set_only_moves_forward_and_only_for_adults)
{
    fresh6();
    doomo_town.player.romance = (uint8_t)((doomo_town.player.romance & ~ROMANCE_PREF_MASK) | ROMANCE_ANY);
    town_res(ROSTER_ID_VEX)->romance = (uint8_t)((town_res(ROSTER_ID_VEX)->romance & ~ROMANCE_PREF_MASK) | ROMANCE_ANY);
    CHECK_EQ(ending_route_set(ROUTE_VEX, RSTAGE_KISSED, -1), 1);
    CHECK_EQ(ending_route_set(ROUTE_VEX, RSTAGE_DATING, -1), 0);        /* no going back */
    CHECK_EQ(ending_route_set(ROUTE_ANY, RSTAGE_DATING, ROSTER_ID_PIP), 0);   /* a kid never */
    CHECK_EQ(ending_route_set(ROUTE_ANY, RSTAGE_DATING, -1), 0);
    CHECK_EQ(ending_route_partner(ROUTE_ANY), -1);
    town_res(ROSTER_ID_MAYOR)->romance = (uint8_t)((town_res(ROSTER_ID_MAYOR)->romance & ~ROMANCE_PREF_MASK) | ROMANCE_ANY);
    CHECK_EQ(ending_route_set(ROUTE_ANY, RSTAGE_DATING, ROSTER_ID_MAYOR), 1);
    CHECK_EQ(ending_route_partner(ROUTE_ANY), ROSTER_ID_MAYOR);
}

/* ---- E4: the finale ---- */
TEST(e4_sexy_finale_needs_mature_on_a_committed_adult_route)
{
    fresh6();
    doomo_town.player.romance = (uint8_t)((doomo_town.player.romance & ~ROMANCE_PREF_MASK) | ROMANCE_ANY);
    town_res(ROSTER_ID_GRUMBLEGUTS)->romance = (uint8_t)((town_res(ROSTER_ID_GRUMBLEGUTS)->romance & ~ROMANCE_PREF_MASK) | ROMANCE_ANY);
    ending_route_set(ROUTE_GRUMBLEGUTS, RSTAGE_COMMITTED, -1);
    opt2_set(OPT_MATURE, MATURE_OFF);
    CHECK_EQ(ending_resolve_finale(), ENDING_E01);                    /* Off: nothing adult fires */
    opt2_set(OPT_MATURE, MATURE_ON);
    CHECK_EQ(ending_resolve_finale(), ENDING_E12);
    ending_route_set(ROUTE_VEX, RSTAGE_COMMITTED, -1);                  /* Grumbleguts still comes first */
    CHECK_EQ(ending_resolve_finale(), ENDING_E12);
    opt2_set(OPT_MATURE, MATURE_OFF);
}

TEST(e4_the_default_finale_is_not_a_bad_ending)
{
    fresh6();
    CHECK(ending_resolve_finale() == ENDING_E01 || ending_resolve_finale() == ENDING_E06);
    CHECK(ending_category(ending_resolve_finale()) != ENDCAT_SEXY);
}
