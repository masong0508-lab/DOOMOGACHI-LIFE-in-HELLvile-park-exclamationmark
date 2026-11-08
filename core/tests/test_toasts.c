/* test_toasts.c - CL1 (clarity C1 + C5): toast kinds and their queue order, a scene change dropping the old
 * screen's toasts, unlock news told once per burst, and the story HUD band the toast lane keeps clear
 * (owner: foundation; the HUD band check reads the story's overworld_int.h). */
#include "test.h"
#include "../src/story_int.h"
#include "../src/overworld_int.h"

static void frames(int n)
{
    while (n-- > 0) {
        plat_begin_frame();
        doomo_frame(0);
        plat_end_frame();
    }
}

static int queued(const char *s)
{
    int i;
    for (i = 0; ui_toast_text(i); i++) if (strstr(ui_toast_text(i), s)) return i;
    return -1;
}

TEST(toast_kinds_queue_by_priority)
{
    scene_set(&SCENE_UI_GALLERY, 0);
    frames(2);
    ui_toast_clear();
    ui_toast_kind("hint", UI_TOAST_HINT);
    frames(2);                                       /* the hint is on screen now */
    ui_toast_kind("hint two", UI_TOAST_HINT);
    ui_toast("feedback");
    ui_toast_kind("saved", UI_TOAST_SYSTEM);
    CHECK_EQ(ui_toast_pending(), 4);
    CHECK_EQ(queued("hint"), 0);                     /* the one on screen keeps its place */
    CHECK_EQ(queued("saved"), 1);                    /* SYSTEM jumps FEEDBACK, which jumps the waiting HINT */
    CHECK_EQ(queued("feedback"), 2);
    CHECK_EQ(queued("hint two"), 3);
    /* full: the oldest waiting line of the lowest kind makes room for a higher one ... */
    ui_toast("more feedback");
    CHECK_EQ(ui_toast_pending(), 4);
    CHECK_EQ(queued("hint two"), -1);
    CHECK_EQ(queued("more feedback"), 3);
    /* ... and a line lower than everything waiting is the one dropped */
    ui_toast_kind("late hint", UI_TOAST_HINT);
    CHECK_EQ(queued("late hint"), -1);
    CHECK_EQ(queued("hint"), 0);
}

TEST(toast_scene_change_drops_the_old_screens_toasts)
{
    scene_set(&SCENE_UI_GALLERY, 0);
    frames(2);
    ui_toast_clear();
    ui_toast_kind("how to use the gallery", UI_TOAST_HINT);
    ui_toast("you did a thing");
    ui_toast_kind("saved", UI_TOAST_SYSTEM);
    frames(1);
    /* an overlay (a dialogue box) is not a new screen: everything stays */
    ui_say(-1, "Hello!");
    CHECK(ui_dialog_active());
    CHECK_EQ(ui_toast_pending(), 3);
    scene_pop();
    CHECK_EQ(ui_toast_pending(), 3);
    /* a pushed full screen: the hint goes, the news stays */
    scene_push(&SCENE_OPTIONS, 0);
    CHECK_EQ(queued("how to use"), -1);
    CHECK(queued("you did a thing") >= 0);
    ui_toast_kind("options hint", UI_TOAST_HINT);
    frames(1);
    scene_pop();                                     /* back: the popped screen's hint goes too */
    CHECK_EQ(queued("options hint"), -1);
    CHECK(queued("you did a thing") >= 0);
    /* a scene_set: everything but SYSTEM goes, except what was posted on the tick of the change */
    frames(1);
    ui_toast("welcome back");
    scene_set(&SCENE_TITLE, 0);
    CHECK_EQ(queued("you did a thing"), -1);
    CHECK(queued("saved") >= 0);
    CHECK(queued("welcome back") >= 0);
    CHECK_EQ(ui_toast_pending(), 2);
}

TEST(toast_hint_waits_for_the_dialogue)
{
    scene_set(&SCENE_UI_GALLERY, 0);
    frames(2);
    ui_toast_clear();
    ui_toast_shown_reset();
    ui_say(-1, "Read me first.");
    ui_toast_kind("then this", UI_TOAST_HINT);
    frames(60);
    CHECK_EQ(ui_toast_shown(UI_TOAST_HINT), 0);      /* still waiting under the box */
    CHECK_EQ(ui_toast_pending(), 1);
    while (ui_dialog_active()) {
        plat_begin_frame(); doomo_frame(BTN_A); plat_end_frame();
        frames(3);
    }
    frames(2);
    CHECK_EQ(ui_toast_shown(UI_TOAST_HINT), 1);
}

TEST(unlock_news_tells_a_burst_once)
{
    int n;
    scene_set(&SCENE_UI_GALLERY, 0);
    frames(2);
    ui_toast_clear();
    ui_toast_shown_reset();
    /* the first news of a session gathers for 20 s: three unlocks a few seconds apart are one digest */
    ui_toast_unlock("Cozy Scarf", "Creator outfit");
    frames(90);
    ui_toast_unlock("Lemon", "paint");
    frames(90);
    ui_toast_unlock("Dice Roller", "trophy");
    CHECK_EQ(ui_toast_news_waiting(), 3);
    for (n = 0; n < 700 && !ui_toast_pending(); n++) frames(1);
    CHECK(ui_toast_pending());
    CHECK(strstr(ui_toast_text(0), "3 new unlocks!") != NULL);
    CHECK_EQ(ui_toast_news_waiting(), 0);
    frames(120);
    CHECK_EQ(ui_toast_shown(UI_TOAST_UNLOCK), 1);
    /* later: a single unlock says what kind of thing it is, after the gap since the last news */
    ui_toast_unlock("Spiral Horns", "Creator look");
    frames(200);
    CHECK_EQ(ui_toast_pending(), 0);                 /* still inside the 40 s gap */
    for (n = 0; n < 1400 && !ui_toast_pending(); n++) frames(1);
    CHECK(ui_toast_pending());
    CHECK_STR(ui_toast_text(0), "New Creator look: Spiral Horns");
    /* feedback is never held back by news, and news waits behind it */
    ui_toast_clear();
    ui_toast_unlock("Sparkle Eyes", NULL);
    ui_toast("Bought a Board Game");
    frames(30);
    CHECK_EQ(ui_toast_news_waiting(), 1);
    CHECK(strstr(ui_toast_text(0), "Board Game") != NULL);
}

TEST(unlock_grant_names_the_kind)
{
    const DoomoUnlock *u = NULL;
    int i;
    for (i = 0; i < unlock_count(); i++) {
        const DoomoUnlock *c = unlock_at(i);
        if (c->cat == UL_CAT_PART && c->list == CLIST_ACC) { u = c; break; }
    }
    CHECK(u != NULL);
    CHECK_STR(unlock_kind_word(u), "Creator outfit");
    ui_toast_clear();
    CHECK(unlock_grant(u->id));
    CHECK_EQ(ui_toast_pending(), 0);                 /* news, not a toast straight away */
    CHECK_EQ(ui_toast_news_waiting(), 1);
}

/* The story HUD band: with toasts coming and going over a new story's first minute, no toast is ever drawn
 * over the objective pill or the clock row (C1, 1.4a / 1.4b). */
TEST(story_toasts_never_cover_the_objective)
{
    int i, checked = 0;
    town_new(TOWN_STORY, 1234, TOWN_POP_PREMADE);
    sim_init();
    scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
    for (i = 0; i < 400 && (scene_top() != &SCENE_STORY || story_talking()); i++) {
        plat_begin_frame(); doomo_frame(scene_top() == &SCENE_CUTSCENE ? BTN_START : BTN_A); plat_end_frame();
        frames(4);
    }
    CHECK(scene_top() == &SCENE_STORY);
    ui_toast_clear();
    ui_toast("A short one");
    ui_toast_kind("A much longer feedback line that scrolls inside its pill", UI_TOAST_SYSTEM);
    ui_toast_kind("Walk into a nest to fight!", UI_TOAST_HINT);
    for (i = 0; i < 400; i++) {
        int ox, oy, ow_, oh, tx, ty, tw, th, band;
        frames(1);
        band = owfx_hud_band(&ox, &oy, &ow_, &oh);
        if (!band || !ui_toast_last_rect(&tx, &ty, &tw, &th)) continue;
        checked++;
        CHECK_MSG(ty >= band, "frame %d: toast top %d above the HUD band's bottom %d", i, ty, band);
        CHECK_MSG(ow_ == 0 || tx + tw <= ox || tx >= ox + ow_ || ty + th <= oy || ty >= oy + oh,
                  "frame %d: toast (%d,%d %dx%d) over the objective (%d,%d %dx%d)", i, tx, ty, tw, th, ox, oy, ow_, oh);
    }
    CHECK(checked > 100);
}
