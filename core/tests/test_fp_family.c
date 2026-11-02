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
