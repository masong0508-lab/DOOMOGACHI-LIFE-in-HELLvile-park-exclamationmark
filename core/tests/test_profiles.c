/* MP0: player profiles inside one shared world save. */
#include <string.h>
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/unlocks.h"
#include "doomo/unlock_ids.h"
#include "doomo/profiles.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/save.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/town.h"
#include "doomo/sim.h"

static void fresh(void)
{
    town_new(TOWN_FREEPLAY, 31, TOWN_POP_PREMADE);
    unlock_grant_quiet(UL_STORY_END(0));             /* FREE: Free play opens play-as after an ending */
    sim_init();
}

static int alive_from(int from)
{
    int id;
    for (id = from; id < DOOMO_PLAYER_ID; id++) {
        const DoomoResident *r = town_res(id);
        if (r && r->status == RES_ALIVE) return id;
    }
    return -1;
}

TEST(prof_starts_with_the_hero_only)
{
    fresh();
    CHECK_EQ(prof_guest_count(), 0);
    CHECK_EQ(prof_count(), 1);
    CHECK_EQ(prof_active(), 0);
    CHECK_EQ(playas_profile(), 0);
    CHECK_EQ(prof_resident(0), PLAYAS_ID_HERO);
    CHECK_EQ(prof_resident(1), -1);
    CHECK_EQ(prof_find_resident(alive_from(0)), -1);
    CHECK_EQ(playas_profile_avatar(1), PLAYAS_ID_NONE);
    CHECK_EQ(playas_profile_hero(2), -1);
    CHECK_EQ(doomo_save_ext_profiles->size(), 0u);               /* no guests: no chunk */
    CHECK_EQ(playas_profile_flags() & PLAYAS_HF_PROFILES, 0);
    doomo_save_ext_profiles->reset();                           /* leave no guests behind */
}

TEST(prof_guests_move_in_with_rules)
{
    int a, b, c, d;
    fresh();
    a = alive_from(0); b = alive_from(a + 1); c = alive_from(b + 1); d = alive_from(c + 1);
    CHECK(a >= 0 && b >= 0 && c >= 0 && d >= 0);
    CHECK_EQ(prof_guest_add("", a, -1), PROF_ERR_NAME);
    CHECK_EQ(prof_guest_add("Twelve chars", a, -1), PROF_ERR_NAME);
    CHECK_EQ(prof_guest_add("Bad\x01name", a, -1), PROF_ERR_NAME);
    CHECK_EQ(prof_guest_add("Ana", PLAYAS_ID_HERO, -1), PROF_ERR_RESIDENT);   /* never the hero */
    CHECK_EQ(prof_guest_add("Ana", -1, -1), PROF_ERR_RESIDENT);
    CHECK_EQ(prof_guest_add("Ana", a, HERO_COOP_LAST), PROF_ERR_HERO);        /* a reserved co-op kind is not filled */
    CHECK_EQ(prof_guest_add("Ana", a, 0), PROF_ERR_HERO);                     /* the hero's own kinds are not guests' */
    CHECK_EQ(prof_guest_add("Ana", a, -1), 1);
    CHECK_EQ(prof_guest_add("Dup", a, -1), PROF_ERR_RESIDENT);                /* one person per villager */
    CHECK_EQ(prof_guest_add("Ben", b, -1), 2);
    CHECK_EQ(prof_guest_add("Cy", c, -1), 3);
    CHECK_EQ(prof_guest_add("Di", d, -1), PROF_ERR_FULL);
    CHECK_EQ(prof_guest_count(), 3);
    CHECK_EQ(prof_count(), 4);
    CHECK(strcmp(prof_name(2), "Ben") == 0);
    CHECK_EQ(prof_resident(3), c);
    CHECK_EQ(prof_find_resident(b), 2);
    CHECK(playas_profile_flags() & PLAYAS_HF_PROFILES);
    CHECK_EQ(playas_profile_hero(1), -1);
    doomo_save_ext_profiles->reset();                           /* leave no guests behind */
}

TEST(prof_switching_is_derived_from_the_avatar)
{
    int a, b;
    fresh();
    a = alive_from(0); b = alive_from(a + 1);
    CHECK(prof_guest_add("Ana", a, -1) == 1 && prof_guest_add("Ben", b, -1) == 2);
    CHECK(prof_switch(0));                                       /* already the hero */
    CHECK(prof_switch(1));
    CHECK_EQ(playas_avatar(), a);
    CHECK_EQ(prof_active(), 1);
    CHECK_EQ(playas_profile_avatar(0), PLAYAS_ID_HERO);          /* profile 0 is still the hero's */
    CHECK_EQ(playas_profile_avatar(1), a);
    CHECK(playas_profile_flags() & PLAYAS_HF_PLAYING_OTHER);
    CHECK(prof_switch(2));
    CHECK_EQ(prof_active(), 2);
    CHECK(prof_switch(0));
    CHECK_EQ(playas_avatar(), PLAYAS_ID_HERO);
    CHECK_EQ(prof_active(), 0);
    CHECK(!prof_switch(3));                                      /* nobody there */
    /* moving out while playing sends you back to the hero */
    CHECK(prof_switch(2));
    CHECK(prof_guest_remove(2));
    CHECK_EQ(playas_avatar(), PLAYAS_ID_HERO);
    CHECK_EQ(prof_guest_count(), 1);
    CHECK_EQ(prof_find_resident(b), -1);
    CHECK(!prof_guest_remove(2));
    doomo_save_ext_profiles->reset();                           /* leave no guests behind */
}

TEST(prof_story_not_cleared_keeps_the_door_shut)
{
    int a;
    town_new(TOWN_STORY, 31, TOWN_POP_PREMADE);
    sim_init();
    a = alive_from(0);
    CHECK(!playas_available());
    CHECK_EQ(prof_guest_add("Ana", a, -1), PROF_ERR_LOCKED);
    CHECK_EQ(prof_guest_count(), 0);
    doomo_save_ext_profiles->reset();                           /* leave no guests behind */
}

TEST(prof_guest_state_flags_quests_ties_room_mail)
{
    int a, b, i;
    fresh();
    a = alive_from(0); b = alive_from(a + 1);
    CHECK(prof_guest_add("Ana", a, -1) == 1);
    /* profile 0 is the save itself: these are quiet no-ops */
    prof_flag_set(0, 3, 1); CHECK_EQ(prof_flag(0, 3), 0);
    CHECK_EQ(prof_mail_send(0, 1), 0);
    prof_flag_set(1, 0, 1); prof_flag_set(1, 127, 1); prof_flag_set(1, 128, 1);
    CHECK_EQ(prof_flag(1, 0), 1); CHECK_EQ(prof_flag(1, 127), 1); CHECK_EQ(prof_flag(1, 128), 0);
    CHECK_EQ(prof_flag(1, 5), 0);
    prof_flag_set(1, 0, 0); CHECK_EQ(prof_flag(1, 0), 0);
    prof_quest_set(1, 15, 200); prof_quest_set(1, 16, 9);
    CHECK_EQ(prof_quest(1, 15), 200); CHECK_EQ(prof_quest(1, 16), 0);
    CHECK_EQ(prof_rel(1, b), 0);
    CHECK(prof_rel_set(1, b, 40)); CHECK_EQ(prof_rel(1, b), 40);
    CHECK(prof_rel_set(1, b, 999)); CHECK_EQ(prof_rel(1, b), 255);   /* clamped */
    CHECK(prof_rel_set(1, b, 0)); CHECK_EQ(prof_rel(1, b), 0);       /* removed */
    CHECK(!prof_rel_set(1, PLAYAS_ID_HERO, 5));                      /* villagers only */
    for (i = 0; i < PROF_REL_MAX; i++) CHECK(prof_rel_set(1, i, 1 + i));
    CHECK(!prof_rel_set(1, PROF_REL_MAX, 7));                        /* full */
    CHECK_EQ(prof_rel(1, 3), 4);
    prof_room_set(1, 6); CHECK_EQ(prof_room(1), 6);
    for (i = 0; i < PROF_MAIL_MAX; i++) CHECK(prof_mail_send(1, 100 + i));
    CHECK(!prof_mail_send(1, 999));
    CHECK_EQ(prof_mail_count(1), PROF_MAIL_MAX);
    CHECK_EQ(prof_mail_take(1), 100);                                /* oldest first */
    CHECK_EQ(prof_mail_take(1), 101);
    CHECK_EQ(prof_mail_count(1), 2);
    CHECK_EQ(prof_mail_take(2), -1);
    doomo_save_ext_profiles->reset();                           /* leave no guests behind */
}

TEST(prof_chunk_round_trips_and_stays_inside_the_contract)
{
    static uint8_t buf[PROF_SZ_TOTAL + 64];
    int a, b, i;
    uint32_t n;
    fresh();
    a = alive_from(0); b = alive_from(a + 1);
    CHECK(prof_guest_add("Ana", a, -1) == 1 && prof_guest_add("Ben", b, -1) == 2);
    prof_flag_set(2, 77, 1); prof_quest_set(1, 4, 9); prof_room_set(2, 3);
    CHECK(prof_rel_set(1, b, 33)); CHECK(prof_mail_send(2, 4242));
    n = doomo_save_ext_profiles->size();
    CHECK_EQ(n, (uint32_t)PROF_SZ_HEADER + 2u * PROF_SZ_REC);
    CHECK(n <= (uint32_t)SAVE_EXT_MAX_PROF);
    CHECK_EQ(doomo_save_ext_profiles->write(buf, n), n);
    CHECK_EQ(doomo_save_ext_profiles->write(buf, n - 1), 0u);       /* too small: writes nothing */
    doomo_save_ext_profiles->reset();
    CHECK_EQ(prof_guest_count(), 0);
    CHECK(doomo_save_ext_profiles->read(buf, n, 1));
    CHECK_EQ(prof_guest_count(), 2);
    CHECK(strcmp(prof_name(1), "Ana") == 0 && strcmp(prof_name(2), "Ben") == 0);
    CHECK_EQ(prof_resident(2), b);
    CHECK_EQ(prof_flag(2, 77), 1); CHECK_EQ(prof_flag(1, 77), 0);
    CHECK_EQ(prof_quest(1, 4), 9); CHECK_EQ(prof_room(2), 3);
    CHECK_EQ(prof_rel(1, b), 33); CHECK_EQ(prof_mail_take(2), 4242);
    /* a wrong layout, a short chunk: refused, no guests */
    CHECK(!doomo_save_ext_profiles->read(buf, n, 2));
    CHECK_EQ(prof_guest_count(), 0);
    CHECK(!doomo_save_ext_profiles->read(buf, n - 1, 1));
    CHECK_EQ(prof_guest_count(), 0);
    /* a guest whose villager is gone by load time is dropped, the rest stay */
    CHECK(doomo_save_ext_profiles->read(buf, n, 1));
    town_res(a)->status = RES_EMPTY;
    CHECK(doomo_save_ext_profiles->read(buf, n, 1));
    CHECK_EQ(prof_guest_count(), 1);
    CHECK_EQ(prof_resident(1), b);
    /* hostile bytes: control chars in a name, counts past their caps, a tie to the hero */
    for (i = 0; i < (int)n; i++) buf[i] = 0xFF;
    buf[0] = 1; buf[1] = 9;
    CHECK(!doomo_save_ext_profiles->read(buf, PROF_SZ_HEADER + 2, 1));
    CHECK(doomo_save_ext_profiles->read(buf, PROF_SZ_TOTAL, 1) || prof_guest_count() == 0);
    CHECK(prof_guest_count() <= PROF_GUESTS);
    doomo_save_ext_profiles->reset();
    doomo_save_ext_profiles->reset();                           /* leave no guests behind */
}

TEST(prof_full_save_round_trip_and_header_bit)
{
    static uint8_t img[262144];
    int a, b;
    fresh();
    a = alive_from(0); b = alive_from(a + 1);
    CHECK(prof_guest_add("Ana", a, -1) == 1 && prof_guest_add("Ben", b, -1) == 2);
    prof_quest_set(2, 1, 77);
    CHECK(prof_switch(1));
    {
        uint32_t n = save_serialize(img, sizeof img);
        CHECK(n > 0);
        CHECK_EQ(img[47] & PLAYAS_HF_PROFILES, PLAYAS_HF_PROFILES);
        doomo_save_ext_profiles->reset();
        town_new(TOWN_FREEPLAY, 31, TOWN_POP_PREMADE);
        CHECK_EQ(prof_guest_count(), 0);
        CHECK(save_deserialize(img, n));
    }
    CHECK_EQ(prof_guest_count(), 2);
    CHECK_EQ(prof_quest(2, 1), 77);
    CHECK_EQ(prof_active(), 1);                                  /* derived: Ana is who you played */
    CHECK_EQ(playas_avatar(), a);
    doomo_save_ext_profiles->reset();                           /* leave no guests behind */
}

TEST(prof_daily_check_moves_out_a_guest_whose_villager_is_gone)
{
    int a, b;
    fresh();
    a = alive_from(0); b = alive_from(a + 1);
    CHECK(prof_guest_add("Ana", a, -1) == 1 && prof_guest_add("Ben", b, -1) == 2);
    CHECK_EQ(prof_check_daily(), 0);
    town_res(a)->status = RES_DEAD;
    CHECK_EQ(prof_check_daily(), 1);
    CHECK_EQ(prof_guest_count(), 1);
    CHECK_EQ(prof_resident(1), b);
    CHECK_EQ(prof_check_daily(), 0);
    doomo_save_ext_profiles->reset();                           /* leave no guests behind */
}

/* ---- MP1: co-op heroes and renaming ---- */
TEST(mp1_a_co_op_hero_moves_in_as_a_housemate_and_moves_out_with_them)
{
    int p, id, n0;
    fresh();
    n0 = family_count();
    p = prof_guest_add("Embie", -1, HERO_COOP_FIRST);
    CHECK_EQ(p, 1);
    CHECK(prof_is_coop(1));
    CHECK_EQ(playas_profile_hero(1), HERO_COOP_FIRST);
    id = prof_resident(1);
    CHECK(id >= PLAYAS_ID_FAMILY_FIRST && family_find(id) != NULL);   /* a household member made from the look */
    CHECK_EQ(family_count(), n0 + 1);
    CHECK_EQ(family_identity(id)->seed, hero_coop_seed(HERO_COOP_FIRST));
    CHECK_STR(family_identity(id)->name, "Embie");
    CHECK(prof_hero_used(HERO_COOP_FIRST));
    CHECK_EQ(prof_guest_add("Twin", -1, HERO_COOP_FIRST), PROF_ERR_HERO);   /* each hero once */
    CHECK_EQ(prof_guest_add("Rk", -1, HERO_COOP_FIRST + 1), 2);
    CHECK_EQ(prof_guest_add("Zz", -1, HERO_COOP_FIRST + HERO_COOP_FILLED), PROF_ERR_HERO);
    CHECK(prof_switch(1));
    CHECK_EQ(playas_avatar(), id);
    CHECK_EQ(prof_active(), 1);
    CHECK(prof_guest_remove(1));                     /* played: back to the hero; the housemate goes too */
    CHECK(playas_is_hero());
    CHECK(family_find(id) == NULL);
    CHECK_EQ(prof_guest_count(), 1);
    CHECK(!prof_hero_used(HERO_COOP_FIRST));         /* free to pick again */
    doomo_save_ext_profiles->reset();
}

TEST(mp1_a_co_op_guest_survives_the_chunk_and_a_lost_housemate_moves_out)
{
    static uint8_t buf[PROF_SZ_TOTAL + 64];
    uint32_t n;
    int id;
    fresh();
    CHECK_EQ(prof_guest_add("Pip", -1, HERO_COOP_FIRST + 2), 1);
    id = prof_resident(1);
    n = doomo_save_ext_profiles->size();
    CHECK_EQ(doomo_save_ext_profiles->write(buf, n), n);
    doomo_save_ext_profiles->reset();
    CHECK(doomo_save_ext_profiles->read(buf, n, 1));
    CHECK_EQ(prof_guest_count(), 1);
    CHECK(prof_is_coop(1));
    CHECK_EQ(prof_resident(1), id);
    CHECK_EQ(playas_profile_hero(1), HERO_COOP_FIRST + 2);
    CHECK_EQ(prof_check_daily(), 0);
    family_remove(id);                               /* the housemate is gone (lost, or removed in the family screen) */
    CHECK_EQ(prof_check_daily(), 1);
    CHECK_EQ(prof_guest_count(), 0);
    CHECK(doomo_save_ext_profiles->read(buf, n, 1)); /* a chunk whose hero byte is hostile is a villager or nothing */
    buf[PROF_SZ_HEADER + 14] = 6;                    /* HERO_COOP_LAST - 1: unfilled kind -> read as a villager: res is a pool id -> dropped */
    doomo_save_ext_profiles->reset();
    CHECK(doomo_save_ext_profiles->read(buf, n, 1));
    CHECK_EQ(prof_guest_count(), 0);
    doomo_save_ext_profiles->reset();
}

TEST(mp1_guests_can_be_renamed)
{
    int a, id;
    fresh();
    a = alive_from(0);
    CHECK_EQ(prof_guest_add("Ana", a, -1), 1);
    CHECK_EQ(prof_rename(1, "Anita"), 1);
    CHECK_STR(prof_name(1), "Anita");
    CHECK_EQ(prof_rename(1, ""), PROF_ERR_NAME);
    CHECK_EQ(prof_rename(1, "Twelve chars"), PROF_ERR_NAME);
    CHECK_EQ(prof_rename(1, "Bad\x01"), PROF_ERR_NAME);
    CHECK_STR(prof_name(1), "Anita");                /* a refused name changes nothing */
    CHECK_EQ(prof_rename(0, "Hero"), 0);             /* the hero's name is the town's, not a guest's */
    CHECK_EQ(prof_rename(2, "Nobody"), 0);
    CHECK_EQ(prof_guest_add("Ember", -1, HERO_COOP_FIRST), 2);
    id = prof_resident(2);
    CHECK_EQ(prof_rename(2, "Cinder"), 1);
    CHECK_STR(family_identity(id)->name, "Cinder");  /* the housemate they play is called the same */
    doomo_save_ext_profiles->reset();
}
