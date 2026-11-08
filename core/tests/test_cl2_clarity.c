/* test_cl2_clarity.c - the week-2 clarity package CL2 (C2, C11).
 *
 * C2: a story begun from the title (story_begin -> STORY_ARG_NEW) whose hero is
 * still the untouched "You" opens the hero cards right after the arrival
 * cutscene, with the story already at the Mayor beat; the runner's
 * "scene STORY 0" start (every older story script) does not, so their flow and
 * fingerprints stay as they were.
 * C11: the story pause panel's rows say whether they can be used yet, with the
 * reason (the pet before it hatches, the Armory before Rivet's first weapon).
 * Runner scripts with screenshots: story_newgame_hero.txt, title_clarity.txt,
 * first_boot.txt. */
#include "test.h"
#include "../src/story_int.h"

int ow_pause_row(int i, const char **help);   /* overworld.c (CL2) */
int ow_pause_count(void);                      /* overworld.c (CL4: + the Help row) */
enum { ROW_PET = 2, ROW_ARMORY = 3 };         /* PM_PET, PM_ARMORY in overworld.c */

/* START skips a cutscene, A moves its shots on; stop at the first frame the
 * top scene is `want` or after `max` ticks */
static int play_until(const DoomoScene *want, int max)
{
    int t;
    for (t = 0; t < max; t++) {
        uint32_t pad = 0;
        if (scene_top() == want) return 1;
        if (scene_top() == &SCENE_CUTSCENE && (t % 8) == 0) pad = (t % 16) ? BTN_A : BTN_START;
        plat_begin_frame();
        doomo_frame(pad);
        plat_end_frame();
    }
    return scene_top() == want;
}

TEST(cl2_new_story_offers_the_hero_cards_after_the_arrival)
{
    const DoomoResident *p;
    test_reset();
    story_begin();
    CHECK(scene_top() == &SCENE_STORY);
    p = town_res(DOOMO_PLAYER_ID);
    CHECK(p && strcmp(p->name, "You") == 0);
    CHECK_MSG(play_until(&SCENE_CREATOR, 3000), "the hero cards never opened (top: %s)",
              scene_top() ? scene_top()->name : "none");
    CHECK_EQ(story_beat(), BEAT_C1_MAYOR);         /* the arrival is over: B on the cards keeps "You" */
    CHECK_EQ(story_pending.kind, PEND_CREATOR);
}

TEST(cl2_story_continue_start_keeps_its_old_flow)
{
    int t, creator = 0;
    test_reset();
    scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);   /* what "scene STORY 0" does */
    for (t = 0; t < 3000 && story_beat() != BEAT_C1_MAYOR; t++) {
        uint32_t pad = scene_top() == &SCENE_CUTSCENE && (t % 8) == 0 ? ((t % 16) ? BTN_A : BTN_START) : 0;
        plat_begin_frame();
        doomo_frame(pad);
        plat_end_frame();
        if (scene_top() == &SCENE_CREATOR) creator = 1;
    }
    for (t = 0; t < 60; t++) {
        plat_begin_frame();
        doomo_frame(0);
        plat_end_frame();
        if (scene_top() == &SCENE_CREATOR) creator = 1;
    }
    CHECK_EQ(story_beat(), BEAT_C1_MAYOR);
    CHECK_EQ(creator, 0);
}

TEST(cl2_pause_rows_say_why_they_are_greyed)
{
    const char *help = NULL;
    test_reset();
    scene_set(&SCENE_STORY, STORY_ARG_DEBUG_BEAT + BEAT_C1_MAYOR);
    CHECK(!pet_exists());
    CHECK_EQ(ow_pause_row(ROW_PET, &help), 0);
    CHECK(help && strstr(help, "Chapter 1") != NULL);
    CHECK_EQ(ow_pause_row(ROW_ARMORY, &help), 0);
    CHECK(help && strstr(help, "Chapter 2") != NULL);
    pet_new("Doomo");
    story_set_flag(SF_WEAPON_CHOSEN, 1);
    CHECK_EQ(ow_pause_row(ROW_PET, &help), 1);
    CHECK_EQ(ow_pause_row(ROW_ARMORY, &help), 1);
    for (int i = 0; i < ow_pause_count(); i++) {   /* every row has a help line (CL4: Help too) */
        help = NULL;
        ow_pause_row(i, &help);
        CHECK_MSG(help && help[0], "row %d has no help line", i);
    }
}
