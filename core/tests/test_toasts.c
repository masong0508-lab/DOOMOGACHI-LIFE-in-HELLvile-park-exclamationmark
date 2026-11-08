/* test_toasts.c - CL1 (clarity C1 + C5): toast kinds and their queue order, a scene change dropping the old
 * screen's toasts, unlock news told once per burst, and the story HUD band the toast lane keeps clear
 * (owner: foundation; the HUD band check reads the story's overworld_int.h). */
#include "test.h"
#include "../src/story_int.h"
#include "../src/overworld_int.h"
#include "../src/fp_int.h"
#include "../src/fp_island.h"

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
    CHECK_STR(unlock_kind_word(u, NULL), "Creator outfit");
    ui_toast_clear();
    CHECK(unlock_grant(u->id));
    CHECK_EQ(ui_toast_pending(), 0);                 /* news, not a toast straight away */
    CHECK_EQ(ui_toast_news_waiting(), 1);
}

/* Every unlock's news names the right kind of thing and fits a toast line whole (the CL1 review: the Secrets lists
 * all said "trophy", and a 40-byte copy cut "New Creator outfit: Sharpshooter Eyepatch" mid-word). */
static void news_line(const DoomoUnlock *u, char *buf, int n)
{
    const char *name, *what = unlock_kind_word(u, &name);
    if (what) snprintf(buf, (size_t)n, "New %s: %s", what, name);
    else snprintf(buf, (size_t)n, "NEW UNLOCK! %s", name);
}

TEST(unlock_news_names_every_kind)
{
    static const struct { int id; const char *what, *name; } want[] = {
        { 989, "Fishdex entry", NULL }, { 1006, "recipe", "Marshmallow Cupcakes" }, { 994, "recipe", "Tempura Roll" },
        { 991, "fishing rod", "Hellsteel Rod" }, { 384, "Bestiary entry", "Impling" }, { 400, "trophy", NULL },
        { 240, "Studio sound", "Tiny Pulse" }, { 243, "Studio drum kit", "Toy Box" }, { 247, "Studio tune", NULL },
        { 252, "choir layout", "Choir Pyramid" }, { 908, "concert stage", "Disco Inferno" },
        { 864, "gold medal", "Vape Cellar" }, { 871, "trophy", "Time Trial" }, { 660, "badge", "Cupid's Arrow Pin" },
        { 644, "trophy", "Welcome Home" }, { 698, "title screen", "On Fire" }, { 701, "Hell Breach", "The Bakery" },
        { 707, "arena mode", "The Exes" }, { 709, "Creator set", "Stitched Parts" }, { 760, "trophy", "Dice Roller" },
        { 1009, "trophy", "First Bite" }, { 564, "trophy", "Home Sweet Home" }, { 832, "trophy", NULL },
    };
    int i, longest = 0;
    char buf[128];
    for (i = 0; i < (int)(sizeof(want) / sizeof(want[0])); i++) {
        const DoomoUnlock *u = unlock_by_id(want[i].id);
        const char *name = NULL;
        CHECK_MSG(u != NULL, "no unlock %d", want[i].id);
        if (!u) continue;
        CHECK_STR(unlock_kind_word(u, &name), want[i].what);
        if (want[i].name) CHECK_STR(name, want[i].name);
        CHECK_MSG(strchr(name, ':') == NULL, "unlock %d: the news still shows \"%s\"", want[i].id, name);
    }
    for (i = 0; i < unlock_count(); i++) {
        const DoomoUnlock *u = unlock_at(i);
        int len;
        news_line(u, buf, (int)sizeof(buf));
        len = (int)strlen(buf);
        if (len > longest) longest = len;
        CHECK_MSG(unlock_kind_word(u, NULL) != NULL, "unlock %d (%s) has no kind word", u->id, u->name);
        CHECK_MSG(len <= UI_TOAST_MAX, "unlock %d: \"%s\" is %d characters, a toast holds %d", u->id, buf, len,
                  UI_TOAST_MAX);
    }
    CHECK(longest > 40);                             /* the lines the 40-byte copy cut are among them */
}

/* The news line is written whole when it is told: the longest names arrive in the toast uncut. */
TEST(unlock_news_line_is_not_cut)
{
    static const int ids[] = { 847, 989 };
    int k, n;
    char want[128];
    scene_set(&SCENE_UI_GALLERY, 0);
    frames(2);
    for (k = 0; k < 2; k++) {
        const DoomoUnlock *u = unlock_by_id(ids[k]);
        CHECK(u != NULL);
        if (!u) continue;
        news_line(u, want, (int)sizeof(want));
        CHECK_MSG(strlen(want) > 39, "unlock %d: \"%s\" is no longer a long line", ids[k], want);
        ui_toast_clear();
        doomo_town.unlocks[u->id >> 3] &= (uint8_t)~(1u << (u->id & 7));
        CHECK(unlock_grant(u->id));
        for (n = 0; n < 700 && !ui_toast_pending(); n++) frames(1);
        CHECK(ui_toast_pending());
        if (ui_toast_text(0)) CHECK_STR(ui_toast_text(0), want);
    }
    ui_toast_clear();
}

/* The story HUD band: with toasts coming and going over a new story's first minute, no toast is ever drawn
 * over the objective pill or the clock row (C1, 1.4a / 1.4b). */
TEST(story_toasts_never_cover_the_objective)
{
    int i, checked = 0, calm = 0;
    town_new(TOWN_STORY, 1234, TOWN_POP_PREMADE);
    sim_init();
    scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
    /* through the chapter card and the arrival until the overworld has been calm for two seconds */
    for (i = 0; i < 2000 && calm < 60; i++) {
        if (scene_top() == &SCENE_STORY && !story_talking()) { calm++; frames(1); continue; }
        calm = 0;
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
    CHECK_MSG(checked > 100, "only %d frames checked (top %s, talking %d)", checked, scene_top() ? scene_top()->name : "-", story_talking());
}

/* 1.6f: money that changes on the hub (rent collecting itself) flashes the purse for about a second, and the hub
 * keeps no frame while it flashes; a change made while the purse was off screen is not news when it comes back */
TEST(hub_purse_flashes_when_the_money_changes)
{
    int i, lit = 0, held;
    fp_new_island(TOWN_POP_PREMADE, 4242);
    fp_set_rent_day(doomo_town.day + 1);              /* today's rent is in: nothing else moves the money */
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
    frames(60);
    CHECK_EQ(fp_money_flash(), 0);
    held = fp_island_hub_held();
    doomo_town.money += 2050;
    for (i = 0; i < 30; i++) {
        frames(1);
        if (fp_money_flash() == 1) lit++;
    }
    CHECK_MSG(lit >= 8, "purse lit on %d of 30 ticks", lit);
    CHECK_EQ(fp_island_hub_held(), held);             /* every flashing tick was drawn */
    frames(10);
    CHECK_EQ(fp_money_flash(), 0);
    doomo_town.money -= 120;
    frames(2);
    CHECK(fp_money_flash() == 2 || fp_money_flash() == 4);
    frames(40);
    scene_set(&SCENE_UI_GALLERY, 0);
    frames(60);
    doomo_town.money += 500;
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
    frames(3);
    CHECK_EQ(fp_money_flash(), 0);
}
