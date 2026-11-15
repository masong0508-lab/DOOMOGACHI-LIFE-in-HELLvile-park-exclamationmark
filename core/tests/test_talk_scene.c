/* test_talk_scene.c - SCENE_TALK played through the pad (owner: TK, T8; docs/TALK.md 6j).
 *
 * The scene is opened the way the house opens it (home_talk_enter) over the real TALK_en.BIN, and every step
 * is a button press, as the runner's talk-* commands do it (core/native/runner_main.c). The runner's eyes on
 * the screen (talk_scene_debug, talk_scene_row_debug) are checked too: the scripts and journeys lean on them.
 * What it pins down: the press budgets of TALK_DESIGN 2.3 (a quick reply 1 press, turn to the next person and
 * compliment them 4), the Romance page never listed for a child, the safety gate's answer to a typed line,
 * and that a talk goes on after it.
 */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/lang.h"
#include "doomo/roster.h"
#include "doomo/town.h"

#define ID_NANA 1
#define ID_PIP  42            /* kid cast */

static int presses;

static void frames(uint32_t b, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        plat_begin_frame();
        doomo_frame(b);
        plat_end_frame();
    }
}

static void press(uint32_t b) { presses++; frames(b, 1); frames(0, 1); }

static int top_is(const DoomoScene *s) { return scene_top() == s; }

static void town(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    presses = 0;
}

/* open Talk with `who` and read the greeting (A until the list is back; a patient reader waits for the typing) */
static int open_talk(int who)
{
    TalkSceneDebug d;
    int k;
    if (!home_talk_enter(who)) return 0;
    frames(0, 3);
    for (k = 0; k < 40 && talk_scene_debug(&d) && d.busy; k++) { frames(0, 90); press(BTN_A); }
    presses = 0;
    return talk_scene_debug(&d) && !d.busy;
}

static int find_row(const char *kind, int arg, int v)
{
    TalkSceneDebug d;
    int i;
    if (!talk_scene_debug(&d)) return -1;
    for (i = 0; i < d.nrows; i++) {
        const char *k = "";
        int a = 0, vv = 0;
        talk_scene_row_debug(i, &k, &a, &vv, NULL);
        if (strcmp(k, kind) == 0 && (arg < 0 || a == arg) && (v < 0 || vv == v)) return i;
    }
    return -1;
}

static void go_to(int row)
{
    TalkSceneDebug d;
    int g;
    for (g = 0; g < 40 && talk_scene_debug(&d) && d.sel != row; g++) press(d.sel < row ? BTN_DOWN : BTN_UP);
}

TEST(talk_scene_the_debug_view_describes_the_page)
{
    TalkSceneDebug d;
    const char *kind = "";
    char tx[64];
    int arg = -1, v = -1, q;
    town();
    CHECK(!talk_scene_debug(&d));                          /* nothing running */
    CHECK(open_talk(ID_NANA));
    CHECK(top_is(&SCENE_TALK));
    CHECK(talk_scene_debug(&d));
    CHECK_STR(d.page, "main");
    CHECK_EQ(d.to_id, ID_NANA);
    CHECK_EQ(d.nparts, 1);
    CHECK(d.nrows >= 8);
    q = find_row("quick", 0, -1);
    CHECK_EQ(q, 0);                                        /* the replies come first */
    CHECK(talk_scene_row_debug(q, &kind, &arg, &v, tx));
    CHECK(tx[0] != 0);
    CHECK(v > 0 && v < TA_COUNT);                          /* a quick reply reports its act */
    CHECK(find_row("cat", -1, -1) > 3);
    CHECK(find_row("type", -1, -1) > 0);
    CHECK(find_row("bye", -1, -1) == d.nrows - 1);
    CHECK(!talk_scene_row_debug(d.nrows, NULL, NULL, NULL, NULL));
    CHECK_EQ(talk_scene_act_cat(TA_COMPLIMENT), 1);        /* Be nice */
    CHECK_EQ(talk_scene_act_cat(TA_HUG), 6);               /* Gestures */
    CHECK_EQ(talk_scene_act_cat(TA_FLIRT), 7);             /* Romance */
    CHECK_EQ(talk_scene_act_cat(TA_REFUSE_SEXUAL), -1);    /* never a row */
}

TEST(talk_scene_a_quick_reply_is_one_press)
{
    TalkSceneDebug d;
    town();
    CHECK(open_talk(ID_NANA));
    CHECK_EQ(find_row("quick", 0, -1), 0);
    CHECK_EQ(talk_scene_debug(&d) ? d.sel : -1, 0);        /* the cursor starts on the top reply */
    press(BTN_A);                                          /* the journey: one press says it */
    CHECK_EQ(presses, 1);
    frames(0, 3);
    CHECK(talk_scene_debug(&d) && d.busy);                 /* the answer is up */
    CHECK(d.you[0] != 0);                                  /* and the header shows what was said */
}

TEST(talk_scene_turning_to_the_next_person_and_complimenting_is_four_presses)
{
    TalkSceneDebug d;
    int steamy = roster_find("Steamy"), row, c;
    town();
    CHECK(steamy >= 0);
    talk_scene_party(&steamy, 1);
    CHECK(open_talk(ID_NANA));
    CHECK(talk_scene_debug(&d) && d.nparts == 2 && d.to_id == ID_NANA);
    press(BTN_SELECT);                                     /* 1: turn to the other */
    CHECK(talk_scene_debug(&d) && d.to_id == steamy);
    row = find_row("cat", -1, -1);
    CHECK_EQ(d.sel, row);                                  /* the cursor waits on the categories */
    c = find_row("cat", talk_scene_act_cat(TA_COMPLIMENT), -1);
    CHECK_EQ(c, row + 1);                                  /* "Be nice" is the next row */
    press(BTN_DOWN);                                       /* 2 */
    press(BTN_A);                                          /* 3: opens Be nice */
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "acts") == 0);
    row = find_row("act", TA_COMPLIMENT, -1);
    CHECK_EQ(row, 0);                                      /* compliment heads the page */
    CHECK_EQ(d.sel, 0);
    press(BTN_A);                                          /* 4: says it */
    CHECK_EQ(presses, 4);
    CHECK(talk_scene_debug(&d) && strstr(d.you, "You're great") != NULL);
    CHECK_EQ(d.to_id, steamy);
}

TEST(talk_scene_saying_goodbye_ends_with_a_clean_toast)
{
    TalkSceneDebug d;
    int k, found = 0;
    town();
    CHECK(open_talk(ID_NANA));
    go_to(find_row("bye", -1, -1));
    press(BTN_A);
    for (k = 0; k < 60 && talk_scene_debug(&d); k++) { frames(0, 90); press(BTN_A); }
    CHECK(!talk_scene_debug(&d));                          /* the talk is over */
    for (k = 0; ui_toast_text(k); k++) {
        const char *t = ui_toast_text(k);
        if (strstr(t, "Bond")) found = 1;
        CHECK(strchr(t, '%') == NULL);                     /* "Bond %+d" once leaked through: the game's snprintf has no "%+d" */
    }
    CHECK(found);
}

TEST(talk_scene_the_romance_page_is_never_listed_for_a_child)
{
    TalkSceneDebug d;
    int steamy = roster_find("Steamy");
    town();
    CHECK(open_talk(steamy));
    CHECK(find_row("cat", talk_scene_act_cat(TA_FLIRT), -1) >= 0);   /* two adults: listed (their own answer stays theirs) */
    talk_scene_debug(&d);
    scene_set(&SCENE_TITLE, 0);                            /* a new talk */
    frames(0, 2);
    town();
    CHECK(open_talk(ID_PIP));
    CHECK_EQ(find_row("cat", talk_scene_act_cat(TA_FLIRT), -1), -1);
    CHECK(find_row("cat", talk_scene_act_cat(TA_HUG), -1) >= 0);     /* a plain hug stays on the Gestures page */
}

#ifndef DOOMO_PLATFORM_GBA
static void type_line(const char *s)
{
    int row = find_row("type", -1, -1);
    go_to(row);
    press(BTN_A);                                          /* "Type your own line..." */
    frames(0, 2);
    for (; *s; s++) { ui_text_entry_feed((unsigned char)*s); frames(0, 1); }
    ui_text_entry_feed('\n');
    frames(0, 3);
}

TEST(talk_scene_a_typed_line_about_hurting_oneself_gets_care_and_the_talk_goes_on)
{
    TalkSceneDebug d;
    int k;
    town();
    CHECK(open_talk(ID_NANA));
    type_line("i want to hurt myself");
    CHECK(talk_scene_debug(&d));
    CHECK_EQ(d.gate, TG_DISTRESS);
    CHECK(d.busy);                                         /* the gentle line is in the box */
    for (k = 0; k < 40 && talk_scene_debug(&d) && d.busy; k++) { frames(0, 90); press(BTN_A); }
    CHECK(talk_scene_debug(&d) && !d.busy);                /* and the talk goes on */
    CHECK_STR(d.page, "main");
    CHECK_EQ(find_row("quick", -1, TA_JOKE), -1);          /* nothing to joke with next */
    CHECK_EQ(find_row("quick", -1, TA_TEASE), -1);
    CHECK_EQ(find_row("quick", -1, TA_MOCK), -1);
    CHECK_EQ(find_row("quick", -1, TA_GOSSIP), -1);
}

TEST(talk_scene_a_typed_romance_line_to_a_child_is_refused)
{
    TalkSceneDebug d;
    int k;
    town();
    CHECK(open_talk(ID_PIP));
    type_line("will you be my girlfriend");
    CHECK(talk_scene_debug(&d));
    CHECK_EQ(d.gate, TG_REFUSE_MINOR);
    CHECK(!TA_IS_ROMANCE(d.reply_act));
    for (k = 0; k < 40 && talk_scene_debug(&d) && d.busy; k++) { frames(0, 90); press(BTN_A); }
    CHECK(talk_scene_debug(&d));                           /* the talk is still up */
}
#endif /* !DOOMO_PLATFORM_GBA */
