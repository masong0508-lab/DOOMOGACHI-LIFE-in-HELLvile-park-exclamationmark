/* test_fp_family.c - SCENE_FAMILY (F2, owner: freeplay): the strong scene
 * replaces K0's placeholder, and it draws every row kind (people, a pet, a
 * resident, the two action rows, a memorial) at both screen classes. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/family.h"
#include "doomo/items.h"
#include "doomo/save.h"
#include "doomo/scenes.h"
#include "doomo/talk.h"
#include "doomo/unlocks.h"
#include "../src/ui_internal.h"

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

static void open_family_time(void)
{
    UiDialogDebug dlg;
    fam_press(BTN_UP);
    fam_press(BTN_A);
    ui_dialog_debug(&dlg);
    CHECK(dlg.text && strstr(dlg.text, "Family time"));
}

TEST(fp_family_meals_use_a_serving_per_guest_before_feeding_anyone)
{
    const int host = 1, out = 13;
    DoomoResident *home, *away;
    UiDialogDebug dlg;
    int child, bond, item;
    uint32_t meals;
    test_reset();
    town_new(TOWN_STORY, 31, TOWN_POP_PREMADE);
    sim_init();
    home = town_res(host); away = town_res(out);
    CHECK(family_add(host, FAMROLE_HOUSEMATE, 0) >= 0);
    CHECK(family_add(out, FAMROLE_SIBLING, 0) >= 0);
    child = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 5u, "Mochi", PRONOUN_THEY);
    CHECK(child >= FAMILY_POOL_FIRST);
    home->activity = ACT_HOME; away->activity = ACT_BEACH;
    home->stat[STAT_HUNGER] = away->stat[STAT_HUNGER] = 20;
    bond = family_bond(child);
    meals = progress_get(PROG_MEALS_SERVED);
    for (item = ITEM_NONE + 1; item < ITEM_COUNT; item++)
        if (doomo_items[item].category == ICAT_FOOD) inv_take(item, inv_count(item));
    inv_add(ITEM_PUDDING, 1);
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_FAMILY, 0);
    fam_frames(0, 3);
    open_family_time();
    fam_press(BTN_A);                    /* Share a meal opens the pantry */
    fam_press(BTN_A);                    /* one Pudding for two guests */
    ui_dialog_debug(&dlg);
    CHECK(dlg.text && strstr(dlg.text, "need 2 servings"));
    CHECK_EQ(inv_count(ITEM_PUDDING), 1);
    CHECK_EQ(home->stat[STAT_HUNGER], 20);
    CHECK_EQ(family_bond(child), bond);
    CHECK_EQ(progress_get(PROG_MEALS_SERVED), meals);
    fam_frames(0, 90);
    fam_press(BTN_A);                    /* dismiss the shortage message */
    CHECK(scene_top() == &SCENE_FAMILY);
    inv_add(ITEM_PUDDING, 1);
    fam_press(BTN_A);                    /* the dashboard cursor is still on Family time */
    fam_press(BTN_A);
    fam_press(BTN_A);
    CHECK_EQ(inv_count(ITEM_PUDDING), 0);
    CHECK(home->stat[STAT_HUNGER] > 20);
    CHECK_EQ(away->stat[STAT_HUNGER], 20);
    CHECK(family_bond(child) > bond);
    CHECK_EQ(progress_get(PROG_MEALS_SERVED), meals + 2);
    CHECK(save_game(1));
    CHECK(load_game(1));
    CHECK_EQ(inv_count(ITEM_PUDDING), 0);
    CHECK(town_res(host)->stat[STAT_HUNGER] > 20);
    CHECK(family_bond(child) > bond);
    CHECK_EQ(progress_get(PROG_MEALS_SERVED), meals + 2);
}

TEST(fp_family_stories_care_for_babies_and_game_night_uses_energy)
{
    const int host = 1;
    DoomoResident *r;
    int baby, bond;
    test_reset();
    town_new(TOWN_STORY, 31, TOWN_POP_PREMADE);
    sim_init();
    CHECK(family_add(host, FAMROLE_HOUSEMATE, 0) >= 0);
    baby = family_pool_add(FAMROLE_CHILD, LIFE_BABY, 5u, "Mochi", PRONOUN_THEY);
    CHECK(baby >= FAMILY_POOL_FIRST);
    bond = family_bond(baby);
    r = town_res(host);
    r->activity = ACT_HOME;
    r->stat[STAT_ENERGY] = 60;
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_FAMILY, 0);
    fam_frames(0, 3);
    open_family_time();
    fam_press(BTN_DOWN); fam_press(BTN_A);     /* Game night */
    CHECK(r->stat[STAT_ENERGY] < 60);
    CHECK_EQ(family_bond(baby), bond);
    CHECK(scene_top() == &SCENE_FAMILY);
    fam_press(BTN_A);
    fam_press(BTN_DOWN); fam_press(BTN_DOWN); fam_press(BTN_A);   /* Story time */
    CHECK(family_bond(baby) > bond);
}

TEST(fp_family_game_night_includes_an_adult_housemate)
{
    int guest, bond;
    test_reset();
    town_new(TOWN_STORY, 31, TOWN_POP_PREMADE);
    sim_init();
    guest = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 5u, "Mochi", PRONOUN_THEY);
    CHECK(guest >= FAMILY_POOL_FIRST);
    bond = family_bond(guest);
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_FAMILY, 0);
    fam_frames(0, 3);
    open_family_time();
    fam_press(BTN_DOWN); fam_press(BTN_A);
    CHECK(family_bond(guest) > bond);
}

TEST(fp_family_member_menu_opens_talk_with_a_named_pool_child)
{
    TalkSceneDebug talk;
    int child;
    test_reset();
    town_new(TOWN_FREEPLAY, 31, TOWN_POP_PREMADE);
    sim_init();
    child = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 5u, "Mochi", PRONOUN_THEY);
    CHECK(child >= FAMILY_POOL_FIRST);
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_FAMILY, 0);
    fam_frames(0, 3);
    fam_press(BTN_A);                     /* Mochi's menu */
    fam_press(BTN_UP);                    /* Never mind must still be reachable */
    fam_press(BTN_A);
    CHECK(scene_top() == &SCENE_FAMILY);
    if (scene_top() != &SCENE_FAMILY) return;
    fam_press(BTN_A);
    fam_press(BTN_DOWN);                  /* Spend time together -> Talk to them */
    fam_press(BTN_A);
    CHECK(talk_scene_debug(&talk));
    CHECK_EQ(talk.to_id, child);
}

TEST(fp_family_group_chat_starts_with_the_people_at_home)
{
    const int present = 1, away = 13;
    TalkSceneDebug talk;
    int child;
    test_reset();
    town_new(TOWN_STORY, 31, TOWN_POP_PREMADE);
    sim_init();
    CHECK(family_add(present, FAMROLE_HOUSEMATE, 0) >= 0);
    CHECK(family_add(away, FAMROLE_SIBLING, 0) >= 0);
    child = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 5u, "Mochi", PRONOUN_THEY);
    CHECK(child >= FAMILY_POOL_FIRST);
    resident_set_stage(town_res(present), LIFE_ADULT);
    town_res(present)->activity = ACT_HOME;
    town_res(away)->activity = ACT_BEACH;
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_FAMILY, 0);
    fam_frames(0, 3);
    open_family_time();
    fam_press(BTN_DOWN); fam_press(BTN_DOWN); fam_press(BTN_DOWN);
    fam_press(BTN_A);
    CHECK(talk_scene_debug(&talk));
    CHECK_EQ(talk.to_id, present);
    CHECK_EQ(talk.nparts, 2);
    CHECK_EQ(talk.ids[1], child);
}

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

TEST(fp_family_adopt_wizard_names_a_child_without_a_partner)
{
    int id;
    town_new(TOWN_FREEPLAY, 31, TOWN_POP_PREMADE);
    sim_init();
    resident_set_stage(&doomo_town.player, LIFE_ADULT);
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_FAMILY, 0);
    fam_frames(0, 3);
    fam_press(BTN_DOWN);                                 /* Add a member */
    fam_press(BTN_A);
    CHECK(scene_top() == &SCENE_DIALOG);
    fam_press(BTN_UP); fam_press(BTN_UP);                  /* Never mind -> Adopt a child */
    fam_press(BTN_A);
    CHECK(scene_top() == &SCENE_DIALOG);                  /* choose an age */
    fam_press(BTN_UP); fam_press(BTN_A);                  /* Never mind */
    CHECK(scene_top() == &SCENE_FAMILY);
    CHECK_EQ(family_count(), 0);

    fam_press(BTN_A);                                    /* open Add a member again */
    fam_press(BTN_UP); fam_press(BTN_UP); fam_press(BTN_A);
    fam_press(BTN_DOWN); fam_press(BTN_DOWN); fam_press(BTN_A); /* Baby -> Toddler -> Child */
    CHECK(scene_top() == &SCENE_TEXT_ENTRY);
    fam_press(BTN_A);                                    /* keyboard starts on A */
    fam_press(BTN_START);                                /* accept the name */
    CHECK(scene_top() == &SCENE_DIALOG);
    fam_press(BTN_DOWN); fam_press(BTN_A);                /* she/her */
    CHECK(scene_top() == &SCENE_FAMILY);
    CHECK_EQ(family_count(), 1);
    CHECK_EQ(family_count_role(FAMROLE_PARTNER), 0);
    id = family_member(0);
    CHECK_EQ(family_role(id), FAMROLE_CHILD);
    CHECK_EQ(family_find(id)->stage, LIFE_CHILD);
    CHECK_STR(family_identity(id)->name, "A");
    CHECK_EQ(family_identity(id)->pronoun, PRONOUN_SHE);
}
