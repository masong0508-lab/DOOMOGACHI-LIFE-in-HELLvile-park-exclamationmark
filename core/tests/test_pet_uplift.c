/* test_pet_uplift.c - F5 part 2: pets grow old (and pass gently), and the late-game uplift
 * (family.h "F5 part 2"). Owner: life. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/unlocks.h"
#include "doomo/unlock_ids.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/roster.h"
#include "doomo/ui.h"
#include "doomo/story.h"
#include "../src/story_int.h"

static void fresh_free_play(void)
{
    town_new(TOWN_FREEPLAY, 31, TOWN_POP_PREMADE);
    unlock_grant_quiet(UL_STORY_END(0));             /* FREE: Free play opens play-as after an ending */
    sim_init();
    family_clear();
    family_loss_clear();
    family_loss_set(1);
}

static void clear_uplift(void)
{
    doomo_town.unlocks[UL_LIFE_HOUSEHOLD_T1 >> 3] &= (uint8_t)~(1u << (UL_LIFE_HOUSEHOLD_T1 & 7));
    doomo_town.unlocks[UL_LIFE_PET_UPLIFT >> 3] &= (uint8_t)~(1u << (UL_LIFE_PET_UPLIFT & 7));
}

/* one town day with the pet looked after, so neglect never decides the test */
static void cared_day(int pet)
{
    doomo_town.day++;
    if (pet >= 0) family_pet_tend(pet, FAMPET_FEED);
    family_loss_daily();
}

TEST(pet_turns_old_after_its_days_and_passes_gently)
{
    int p, i;
    fresh_free_play();
    p = family_pool_add(FAMROLE_PET, LIFE_ADULT, 4242u, "Rex", -1);
    CHECK(p >= 0);
    CHECK(!family_pet_old(p));
    CHECK_EQ(family_pet_old_days_left(p), 0);
    for (i = 0; i < FAMILY_PET_OLD_DAYS - 1; i++) cared_day(p);
    CHECK(!family_pet_old(p));
    CHECK_EQ(family_pet_age_days(p), FAMILY_PET_OLD_DAYS - 1);
    ui_toast_clear();
    cared_day(p);                                               /* the day it turns old */
    CHECK(family_pet_old(p));
    CHECK(ui_toast_pending());
    CHECK_EQ(family_stage_progress(p), 1);                      /* one old day counted */
    CHECK_EQ(family_pet_old_days_left(p), FAMILY_PET_OLD_PASS - 1);
    CHECK(family_find(p) != NULL);
    for (i = 1; i < FAMILY_PET_OLD_PASS - 1; i++) { cared_day(p); CHECK(family_find(p) != NULL); }
    CHECK_EQ(family_memorial_count(), 0);
    cared_day(p);                                               /* the last day */
    CHECK(family_find(p) == NULL);
    CHECK_EQ(family_memorial_count(), 1);
    CHECK_EQ(family_memorial(0)->cause, FAMLOSS_AGE);
    CHECK_EQ(family_memorial_role(0), FAMROLE_PET);
}

TEST(pet_old_age_waits_while_loss_is_off_and_warns_first)
{
    int p, i;
    fresh_free_play();
    p = family_pool_add(FAMROLE_PET, LIFE_ADULT, 4243u, "Bun", -1);
    CHECK(p >= 0);
    family_loss_set(0);
    for (i = 0; i < FAMILY_PET_OLD_DAYS + 60; i++) cared_day(p);
    CHECK(family_find(p) != NULL && family_pet_old(p));
    CHECK_EQ(family_pet_old_days_left(p), 0);                   /* Loss off: no countdown */
    CHECK(family_stage_progress(p) <= FAMILY_PET_OLD_PASS - 5); /* a warning day always comes first */
    family_loss_set(1);
    CHECK(family_pet_old_days_left(p) >= 5);
    ui_toast_clear();
    cared_day(p);                                               /* the first day with Loss on: "slowing down" */
    CHECK(family_find(p) != NULL);
    CHECK(ui_toast_pending());
    for (i = 0; i < 3; i++) { cared_day(p); CHECK(family_find(p) != NULL); }
    cared_day(p);                                               /* four days after the warning */
    CHECK(family_find(p) == NULL);
    CHECK_EQ(family_memorial(0)->cause, FAMLOSS_AGE);
}

TEST(pet_from_an_old_save_gets_a_grace_month)
{
    int p, i;
    fresh_free_play();
    doomo_town.day = 500;
    p = family_pool_add(FAMROLE_PET, LIFE_ADULT, 4244u, "Old", -1);
    CHECK(p >= 0);
    doomo_town.day = 900;                                       /* it joined long ago: old at once */
    CHECK(family_pet_old(p));
    CHECK_EQ(family_stage_progress(p), 0);                      /* ... but its old days start at 0 */
    cared_day(p);
    CHECK_EQ(family_stage_progress(p), 1);
    for (i = 1; i < FAMILY_PET_OLD_PASS - 1; i++) { cared_day(p); CHECK(family_find(p) != NULL); }
    cared_day(p);
    CHECK(family_find(p) == NULL);
}

TEST(only_pets_grow_old_this_way)
{
    int a;
    fresh_free_play();
    a = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(a >= 0);
    doomo_town.day += 400;
    CHECK(!family_pet_old(a));
    CHECK_EQ(family_pet_age_days(a), 0);
    CHECK_EQ(family_pet_old_days_left(a), 0);
    family_loss_daily();
    CHECK(family_find(a) != NULL);
    CHECK_EQ(family_pet_age_days(9999), 0);
}

TEST(uplift_is_locked_until_the_story_is_finished)
{
    int p;
    fresh_free_play();
    clear_uplift();
    p = family_pool_add(FAMROLE_PET, LIFE_ADULT, 77u, "Rex", -1);
    CHECK(p >= 0);
    family_bond_add(p, 10);
    CHECK(!family_uplift_unlocked());
    CHECK_EQ(family_uplift_check(p), FAMUP_LOCKED);
    CHECK_EQ(family_uplift(p), FAMUP_LOCKED);
    CHECK_EQ(family_role(p), FAMROLE_PET);
    CHECK_EQ(family_tier_story_done(), HOUSEHOLD_TIER_GAME_DONE);    /* the credits */
    CHECK(unlock_has(UL_LIFE_PET_UPLIFT));
    CHECK(family_uplift_unlocked());
    CHECK_EQ(family_uplift_check(p), FAMUP_OK);
    /* a story saved at tier 1 before the uplift existed still gets it through the tier itself */
    clear_uplift();
    doomo_town.unlocks[UL_LIFE_HOUSEHOLD_T1 >> 3] |= (uint8_t)(1u << (UL_LIFE_HOUSEHOLD_T1 & 7));
    CHECK(family_uplift_unlocked());
    clear_uplift();
    CHECK(family_tier_sync(1));
    CHECK(unlock_has(UL_LIFE_PET_UPLIFT));
}

TEST(uplift_needs_a_close_pet_and_makes_a_housemate)
{
    int p, a, kind, i;
    DoomoResident r;
    fresh_free_play();
    clear_uplift();
    family_tier_story_done();
    p = family_pool_add(FAMROLE_PET, LIFE_ADULT, 77u, "Rex", -1);
    a = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(p >= 0 && a >= 0);
    CHECK_EQ(family_uplift_check(a), FAMUP_NOT_PET);
    CHECK_EQ(family_uplift_check(DOOMO_PLAYER_ID), FAMUP_NOT_PET);
    CHECK_EQ(family_uplift(a), FAMUP_NOT_PET);
    CHECK_EQ(family_uplift_check(p), FAMUP_BOND);               /* bond 3 */
    for (i = 0; i < 10 && family_uplift_check(p) == FAMUP_BOND; i++) family_pet_tend(p, FAMPET_PLAY);
    CHECK(family_bond(p) >= FAMILY_UPLIFT_BOND);
    CHECK_EQ(family_uplift_check(p), FAMUP_OK);
    kind = family_pet_kind(p);
    CHECK(family_resident(p, &r) == 0);                         /* still a pet: not a person */
    CHECK(!playas_can_be(p));
    family_clear();                                             /* (the cap is 3 here: start clean) */
    p = family_pool_add(FAMROLE_PET, LIFE_ADULT, 77u, "Rex", -1);
    family_bond_add(p, 8);
    kind = family_pet_kind(p);
    CHECK_EQ(family_count(), 1);
    ui_toast_clear();
    CHECK_EQ(family_uplift(p), FAMUP_OK);
    CHECK(ui_toast_pending());
    CHECK_EQ(family_role(p), FAMROLE_HOUSEMATE);
    CHECK_EQ(family_find(p)->stage, LIFE_ADULT);
    CHECK_EQ(family_count(), 1);                                /* the same member, the same slot */
    CHECK_EQ(family_pet_kind(p), -1);                           /* no longer a pet ... */
    CHECK_EQ(family_uplifted_kind(p), kind);                    /* ... but it remembers what it was */
    CHECK_STR(family_identity(p)->name, "Rex");
    CHECK_EQ(family_identity(p)->romance, ROMANCE_NONE);        /* family, never a partner */
    CHECK_EQ(family_set_role(p, FAMROLE_PARTNER), FAM_ERR_GATE);   /* the partner gate agrees (LS0) */
    CHECK(family_resident(p, &r) == 1);                         /* now a full person */
    CHECK(playas_can_be(p));                                    /* play-as opens (free play) */
    CHECK_EQ(family_uplift(p), FAMUP_NOT_PET);                  /* once only */
    CHECK_EQ(family_uplift_check(p), FAMUP_NOT_PET);
}

TEST(an_uplifted_pet_never_grows_old_and_a_person_is_never_uplifted_by_neglect)
{
    int p, i;
    fresh_free_play();
    clear_uplift();
    family_tier_story_done();
    p = family_pool_add(FAMROLE_PET, LIFE_ADULT, 91u, "Pip", -1);
    for (i = 0; i < FAMILY_PET_OLD_DAYS + 10; i++) cared_day(p);
    CHECK(family_pet_old(p));
    CHECK(family_stage_progress(p) > 0);                        /* it has old days ... */
    family_bond_add(p, 8);
    CHECK_EQ(family_uplift(p), FAMUP_OK);
    CHECK_EQ(family_stage_progress(p), 0);                      /* ... the uplift wipes them */
    for (i = 0; i < 120; i++) { doomo_town.day++; family_care(p); family_loss_daily(); }
    CHECK(family_find(p) != NULL);                              /* an adult housemate does not pass */
    CHECK_EQ(family_role(p), FAMROLE_HOUSEMATE);
}

TEST(uplift_survives_a_save_and_bad_marks_are_rejected)
{
    static uint8_t fsec[FAMILY_SZ_WORST], psec[FAMILY_POOL_SZ_WORST], t[FAMILY_POOL_SZ_WORST];
    uint32_t fn, pn;
    int p, q, kind;
    fresh_free_play();
    clear_uplift();
    family_tier_story_done();
    p = family_pool_add(FAMROLE_PET, LIFE_ADULT, 91u, "Pip", -1);
    q = family_pool_add(FAMROLE_PET, LIFE_ADULT, 92u, "Dot", -1);
    family_bond_add(p, 8);
    kind = family_pet_kind(p);
    CHECK_EQ(family_uplift(p), FAMUP_OK);
    fn = family_save(fsec, sizeof(fsec));
    pn = family_pool_save(psec, sizeof(psec));
    CHECK(fn > 0 && pn > 0);
    CHECK_EQ(psec[FAMILY_SEC_HEAD + 20], (uint8_t)(kind + 1));  /* the first record is Pip's */
    CHECK_EQ(psec[FAMILY_SEC_HEAD + FAMILY_POOL_REC + 20], 0);  /* Dot is still a pet */
    family_clear();
    CHECK(family_load(fsec, fn));
    CHECK(family_pool_load(psec, pn));
    CHECK_EQ(family_role(p), FAMROLE_HOUSEMATE);
    CHECK_EQ(family_uplifted_kind(p), kind);
    CHECK_EQ(family_role(q), FAMROLE_PET);
    CHECK_EQ(family_uplifted_kind(q), -1);
    /* a mark past the last kind, or a used second spare byte, is a bad section */
    memcpy(t, psec, pn); t[FAMILY_SEC_HEAD + 20] = (uint8_t)(FAMPET_COUNT + 1);
    CHECK(!family_pool_load(t, pn));
    memcpy(t, psec, pn); t[FAMILY_SEC_HEAD + 21] = 1;
    CHECK(!family_pool_load(t, pn));
    memcpy(t, psec, pn); t[FAMILY_SEC_HEAD + 20] = FAMPET_COUNT;     /* the last kind is fine */
    CHECK(family_pool_load(t, pn));
}

TEST(uplift_story_callbacks)
{
    int p;
    fresh_free_play();
    clear_uplift();
    p = family_pool_add(FAMROLE_PET, LIFE_ADULT, 91u, "Pip", -1);
    side_callback(CB_FAM_UPLIFT_OK, p);
    CHECK_EQ(vm_var(0), 0);                                     /* locked */
    side_callback(CB_FAM_UPLIFT, p);
    CHECK_EQ(vm_var(0), 0);
    CHECK_EQ(family_role(p), FAMROLE_PET);
    family_tier_story_done();
    side_callback(CB_FAM_UPLIFT_OK, p);
    CHECK_EQ(vm_var(0), 0);                                     /* not close enough */
    family_bond_add(p, 8);
    side_callback(CB_FAM_UPLIFT_OK, p);
    CHECK_EQ(vm_var(0), 1);
    side_callback(CB_FAM_UPLIFT, p);
    CHECK_EQ(vm_var(0), 1);
    CHECK_EQ(family_role(p), FAMROLE_HOUSEMATE);
    side_callback(CB_FAM_UPLIFT, p);
    CHECK_EQ(vm_var(0), 0);                                     /* already done */
    side_callback(CB_FAM_UPLIFT, 9999);
    CHECK_EQ(vm_var(0), 0);                                     /* a stranger is harmless */
}
