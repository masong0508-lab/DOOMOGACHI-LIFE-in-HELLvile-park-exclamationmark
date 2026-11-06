/* test_fp_family.c - SCENE_FAMILY (F2, owner: freeplay): the strong scene
 * replaces K0's placeholder, and it draws every row kind (people, a pet, a
 * resident, the two action rows, a memorial) at both screen classes. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/family.h"
#include "doomo/scenes.h"

TEST(fp_family_scene_is_strong_and_draws_a_household)
{
    int i, pet, kid, r;
    town_new(TOWN_FREEPLAY, 31, TOWN_POP_PREMADE);
    sim_init();
    CHECK(!scene_is_reserved_stub(&SCENE_FAMILY));
    CHECK(!scene_is_stub(&SCENE_FAMILY));
    scene_set(&SCENE_TITLE, 0);
    kid = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 5u, "Pip", PRONOUN_THEY);
    pet = family_pool_add(FAMROLE_PET, LIFE_ADULT, 6u, "Biscuit", -1);
    CHECK(kid >= FAMILY_POOL_FIRST);
    CHECK(pet >= FAMILY_POOL_FIRST);
    for (r = 0; r < DOOMO_MAX_RESIDENTS; r++)
        if (town_res(r)->status == RES_ALIVE && family_add(r, FAMROLE_HOUSEMATE, 0) >= 0) break;
    CHECK(family_count() >= 3);
    /* a hungry member gets the need marker; a neglected pet is drawn too */
    for (i = 0; i < family_count(); i++) {
        DoomoResident *m = town_res(family_member(i));
        if (m && m->status == RES_ALIVE) m->stat[STAT_HUNGER] = 5;
    }
    scene_push(&SCENE_FAMILY, 0);
    CHECK(scene_top() == &SCENE_FAMILY);
    for (i = 0; i < 3; i++) {
        plat_begin_frame();
        scene_draw();
        plat_end_frame();
        scene_update();
    }
    CHECK(scene_top() == &SCENE_FAMILY);
    /* a loss leaves a memorial, and the screen still draws with it */
    CHECK(family_lose(pet, FAMLOSS_AGE));
    CHECK_EQ(family_memorial_count(), 1);
    for (i = 0; i < 3; i++) {
        plat_begin_frame();
        scene_draw();
        plat_end_frame();
        scene_update();
    }
    CHECK(scene_top() == &SCENE_FAMILY);
}

/* ---- F5 part 2: the Uplift row in a pet's menu (drives the real screen) -------------- */
static void fam_frames(uint32_t b, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        plat_begin_frame();
        doomo_frame(b);
        plat_end_frame();
    }
}

/* one press, then enough idle frames for a menu to finish opening or closing */
static void fam_press(uint32_t b) { fam_frames(b, 1); fam_frames(0, 30); }

TEST(fp_family_pet_menu_offers_the_uplift_only_once_unlocked)
{
    int pet, i;
    town_new(TOWN_FREEPLAY, 31, TOWN_POP_PREMADE);
    sim_init();
    family_clear();
    doomo_town.unlocks[UL_LIFE_HOUSEHOLD_T1 >> 3] &= (uint8_t)~(1u << (UL_LIFE_HOUSEHOLD_T1 & 7));
    doomo_town.unlocks[UL_LIFE_PET_UPLIFT >> 3] &= (uint8_t)~(1u << (UL_LIFE_PET_UPLIFT & 7));
    pet = family_pool_add(FAMROLE_PET, LIFE_ADULT, 6u, "Biscuit", -1);
    CHECK(pet >= FAMILY_POOL_FIRST);
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_FAMILY, 0);
    fam_frames(0, 3);
    CHECK(scene_top() == &SCENE_FAMILY);
    /* locked: Feed, Play, Rename, Move out, Never mind (no Uplift row); UP wraps to "Never mind" */
    fam_press(BTN_A);                                    /* open the pet's menu */
    CHECK(scene_top() == &SCENE_DIALOG);
    fam_press(BTN_UP);
    fam_press(BTN_A);
    CHECK(scene_top() == &SCENE_FAMILY);
    CHECK(family_role(pet) == FAMROLE_PET);
    /* unlocked and close: the menu has Uplift third; confirm it and the pet becomes a housemate */
    CHECK(family_tier_story_done() == HOUSEHOLD_TIER_GAME_DONE);
    family_bond_add(pet, 8);
    CHECK_EQ(family_uplift_check(pet), FAMUP_OK);
    fam_frames(0, 20);
    fam_press(BTN_A);                                    /* the pet's menu */
    CHECK(scene_top() == &SCENE_DIALOG);
    fam_press(BTN_DOWN); fam_press(BTN_DOWN);            /* Feed -> Play -> Uplift */
    fam_press(BTN_A);                                    /* "Uplift them?" */
    CHECK(scene_top() == &SCENE_CONFIRM);
    CHECK_EQ(family_role(pet), FAMROLE_PET);             /* nothing happens until Yes */
    fam_press(BTN_LEFT);                                 /* the default is No: move to Yes */
    fam_press(BTN_A);
    CHECK_EQ(family_role(pet), FAMROLE_HOUSEMATE);
    CHECK(family_uplifted_kind(pet) >= 0);
    for (i = 0; i < 3; i++) fam_frames(0, 30);           /* the toast runs out; the screen still draws */
    CHECK(scene_top() == &SCENE_FAMILY || scene_top() == &SCENE_DIALOG);
}
