/* test_talk_scene.c - SCENE_TALK played through the pad (owner: TK, T8; docs/TALK.md 6k).
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
#include "doomo/ui.h"
#include "doomo/items.h"
#include "doomo/sim.h"
#include "../src/ui_internal.h"     /* UiDialogDebug */

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

/* The thought bubble is backed by the simulation's real problem queue, not
 * arbitrary Talk text. Reading the thought does not solve it; choosing the
 * corresponding care action does, and refreshes the main page afterwards. */
TEST(talk_scene_thought_bubble_leads_to_real_care)
{
    TalkSceneDebug d;
    DoomoResident *r;
    int row, k;
    town();
    r = town_res(ID_NANA);
    CHECK(r != NULL);
    r->stat[STAT_HYGIENE] = 20;
    CHECK(sim_problem_raise(ID_NANA, PROB_DIRTY, -1, 0) >= 0);
    CHECK(open_talk(ID_NANA));
    row = find_row("concern", PROB_DIRTY, -1);
    CHECK(row > find_row("help", -1, -1));
    go_to(row); press(BTN_A);
    CHECK(talk_scene_debug(&d));
    CHECK_STR(d.page, "concern");
    row = find_row("concerninfo", PROB_DIRTY, -1);
    CHECK(row >= 0);
    go_to(row); press(BTN_A);
    CHECK(talk_scene_debug(&d) && d.busy);
    CHECK(sim_problem_for(ID_NANA) >= 0); /* listening is not free problem resolution */
    for (k = 0; k < 40 && talk_scene_debug(&d) && d.busy; k++) {
        frames(0, 60); press(BTN_A);
    }
    CHECK(talk_scene_debug(&d) && !d.busy);
    row = find_row("concernact", PROB_DIRTY, -1);
    CHECK(row >= 0);
    go_to(row); press(BTN_A);
    CHECK(r->stat[STAT_HYGIENE] >= 95);
    CHECK_EQ(sim_problem_for(ID_NANA), -1);
    CHECK(talk_scene_debug(&d) && d.busy);
}

/* A requested item must really be held, can be delivered only once, and
 * must not pay out simply because a thought bubble was inspected. */
TEST(talk_scene_thought_item_requires_inventory_and_delivers_once)
{
    TalkSceneDebug d;
    int row, item = ITEM_PLUSHIE, cash;
    town();
    while (inv_count(item) > 0) CHECK(inv_take(item, 1));
    CHECK(sim_problem_raise(ID_NANA, PROB_WANTS_ITEM, -1, item) >= 0);
    CHECK(open_talk(ID_NANA));
    cash = (int)doomo_town.money;
    row = find_row("concern", PROB_WANTS_ITEM, -1);
    CHECK(row >= 0);
    go_to(row); press(BTN_A);
    CHECK(talk_scene_debug(&d));
    CHECK_STR(d.page, "concern");
    row = find_row("concernact", PROB_WANTS_ITEM, -1);
    CHECK(row >= 0);
    go_to(row); press(BTN_A);
    CHECK(sim_problem_for(ID_NANA) >= 0);
    CHECK_EQ((int)doomo_town.money, cash);
    CHECK_EQ(inv_count(item), 0);
    CHECK(inv_add(item, 1) >= 1);
    press(BTN_A); /* the same live row now has the missing item */
    CHECK_EQ(inv_count(item), 0);
    CHECK_EQ(sim_problem_for(ID_NANA), -1);
    CHECK(talk_scene_debug(&d) && d.busy);
}

TEST(talk_scene_needs_help_changes_real_sim_stats)
{
    TalkSceneDebug d;
    DoomoResident *r;
    int row, before, food;
    town();
    r = town_res(ID_NANA);
    r->stat[STAT_HYGIENE] = 30;
    r->stat[STAT_HAPPY] = 25;
    r->stat[STAT_SOCIAL] = 30;
    r->stat[STAT_HEALTH] = 30;
    r->stat[STAT_HUNGER] = 25;
    CHECK(inv_add(ITEM_CUPCAKE, 1) >= 1);
    CHECK(inv_add(ITEM_MEDICINE, 1) >= 1);
    CHECK(open_talk(ID_NANA));
    row = find_row("help", -1, -1);
    CHECK(row >= 0);
    go_to(row); press(BTN_A);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "care") == 0);
    CHECK(find_row("comfort", -1, -1) >= 0);
    CHECK(find_row("clean", -1, -1) >= 0);
    CHECK(find_row("feed", -1, -1) >= 0);
    CHECK(find_row("heal", -1, -1) >= 0);
    before = r->stat[STAT_HYGIENE];
    go_to(find_row("clean", -1, -1)); press(BTN_A);
    CHECK(r->stat[STAT_HYGIENE] > before);
    CHECK(talk_scene_debug(&d) && d.busy);
    {
        int step;
        for (step = 0; step < 40 && talk_scene_debug(&d) && d.busy; step++) {
            frames(0, 30);
            press(BTN_A);
        }
        CHECK(talk_scene_debug(&d) && !d.busy);
    }
    row = find_row("help", -1, -1);
    CHECK(row >= 0);
    go_to(row); press(BTN_A);
    row = find_row("feed", -1, -1);
    CHECK(row >= 0);
    go_to(row); press(BTN_A);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "feed") == 0);
    food = find_row("feeditem", ITEM_CUPCAKE, -1);
    CHECK(food >= 0);
    before = inv_count(ITEM_CUPCAKE);
    go_to(food); press(BTN_A);
    CHECK_EQ(inv_count(ITEM_CUPCAKE), before - 1);
    CHECK(talk_scene_debug(&d) && d.busy);
}

TEST(talk_scene_real_outing_menu_and_play_action)
{
    TalkSceneDebug d;
    DoomoResident *r;
    int row, i, has_real_venue = 0, happy, energy;
    town();
    doomo_town.minute = 12 * 60;  /* park and outdoor venues open */
    r = town_res(ID_NANA);
    r->activity = ACT_HOME;
    r->stat[STAT_HUNGER] = 80;
    r->stat[STAT_ENERGY] = 80;
    r->stat[STAT_HEALTH] = 85;
    r->stat[STAT_HAPPY] = 50;
    r->stat[STAT_SOCIAL] = 50;
    CHECK(open_talk(ID_NANA));
    row = find_row("out", -1, -1);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "outing") == 0);
    for (i = 0; i < d.nrows; i++) {
        const char *kind = "";
        int arg, value;
        talk_scene_row_debug(i, &kind, &arg, &value, NULL);
        if (strcmp(kind, "outing") == 0 && value > 0 &&
            talk_act_venue_of((uint16_t)value) == ACT_PARK)
            has_real_venue = 1;
    }
    CHECK(has_real_venue);          /* only valid world destinations are shown */
    press(BTN_B);                   /* back without promising an outing */
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "main") == 0);
    row = find_row("play", -1, -1);
    CHECK(row >= 0);
    happy = r->stat[STAT_HAPPY];
    energy = r->stat[STAT_ENERGY];
    go_to(row);
    press(BTN_A);
    CHECK(r->stat[STAT_HAPPY] > happy);
    CHECK(r->stat[STAT_ENERGY] < energy);
    CHECK(talk_scene_debug(&d) && d.busy);
}

TEST(talk_scene_show_only_a_real_held_item)
{
    TalkSceneDebug d;
    int row, i, shown = 0, count;
    town();
    {
        int before = inv_count(ITEM_PLUSHIE);
        CHECK(before < 99);
        CHECK(inv_add(ITEM_PLUSHIE, 1) == before + 1);
    }
    CHECK(open_talk(ID_NANA));
    row = find_row("show", -1, -1);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "show") == 0);
    for (i = 0; i < d.nrows; i++) {
        const char *kind = "";
        int item = -1;
        talk_scene_row_debug(i, &kind, &item, NULL, NULL);
        if (strcmp(kind, "showitem") == 0 && item == ITEM_PLUSHIE) {
            shown = i + 1;
            break;
        }
    }
    CHECK(shown > 0);
    count = inv_count(ITEM_PLUSHIE);
    go_to(shown - 1);
    press(BTN_A);
    CHECK_EQ(inv_count(ITEM_PLUSHIE), count); /* show never steals the item */
    CHECK(talk_scene_debug(&d) && d.busy);
    CHECK(strstr(d.you, "Plushie") != NULL || strstr(d.you, "plushie") != NULL);
}

TEST(talk_scene_give_a_real_item_and_apply_care)
{
    TalkSceneDebug d;
    int row, i, gift = -1, count;
    town();
    {
        int before = inv_count(ITEM_PLUSHIE);
        CHECK(before < 99);
        CHECK(inv_add(ITEM_PLUSHIE, 1) == before + 1);
    }
    CHECK(open_talk(ID_NANA));
    row = find_row("give", -1, -1);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "give") == 0);
    for (i = 0; i < d.nrows; i++) {
        const char *kind = "";
        int arg = -1;
        talk_scene_row_debug(i, &kind, &arg, NULL, NULL);
        if (strcmp(kind, "giveitem") == 0 && arg == ITEM_PLUSHIE) {
            gift = i;
            break;
        }
    }
    CHECK(gift >= 0);
    count = inv_count(ITEM_PLUSHIE);
    go_to(gift);
    press(BTN_A);
    CHECK_EQ(inv_count(ITEM_PLUSHIE), count - 1);
    CHECK(talk_scene_debug(&d) && d.busy);
}

TEST(talk_scene_only_remembers_actual_shared_events)
{
    TalkSceneDebug d;
    int row;
    town();
    CHECK(open_talk(ID_NANA));
    CHECK_EQ(find_row("remember", -1, -1), -1);
    talk_mem_note_shared(ID_NANA, TALK_SHARED_GIFTED);
    /* Rebuild the main menu when the player returns from Options. */
    row = find_row("more", -1, -1);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    press(BTN_B);
    row = find_row("remember", -1, -1);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    CHECK(talk_scene_debug(&d) && d.busy);
    CHECK(strstr(d.you, "Remember") != NULL);
}

TEST(talk_scene_history_only_recalls_real_events_and_favors)
{
    TalkSceneDebug d;
    UiDialogDebug box;
    const char *kind = "";
    char label[64];
    int row;
    town();
    CHECK(open_talk(ID_NANA));
    go_to(find_row("more", -1, -1));
    press(BTN_A);
    row = find_row("history", -1, -1);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    CHECK(talk_scene_debug(&d));
    CHECK_STR(d.page, "history");
    CHECK(find_row("historyinfo", -1, -1) >= 0);
    CHECK_EQ(find_row("historyquest", -1, -1), -1);
    CHECK_EQ(find_row("historyevent", -1, -1), -1); /* no invented past */
    press(BTN_B);
    CHECK(talk_scene_debug(&d));
    CHECK_STR(d.page, "more"); /* B goes to Options, not goodbye */

    talk_mem_note_shared(ID_NANA, TALK_SHARED_PLAYED | TALK_SHARED_GIFTED);
    CHECK(talk_mem_quest_set(ID_NANA, TQK_FETCH, TQS_ACTIVE, 3, 0));
    CHECK_EQ(talk_mem_pinned_count(), 1);
    row = find_row("history", -1, -1);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    CHECK(talk_scene_debug(&d));
    CHECK_STR(d.page, "history");
    row = find_row("historyinfo", -1, -1);
    CHECK(row >= 0);
    CHECK(talk_scene_row_debug(row, &kind, NULL, NULL, label));
    CHECK(strstr(label, "1 open") != NULL);
    CHECK(find_row("historyquest", TQK_FETCH, -1) >= 0);
    CHECK(find_row("historyevent", TALK_SHARED_PLAYED, -1) >= 0);
    CHECK(find_row("historyevent", TALK_SHARED_GIFTED, -1) >= 0);
    CHECK_EQ(find_row("historyevent", TALK_SHARED_INVITED, -1), -1);
    row = find_row("historyevent", TALK_SHARED_GIFTED, -1);
    go_to(row);
    press(BTN_A);
    CHECK(talk_scene_debug(&d) && d.busy);
    memset(&box, 0, sizeof box);
    ui_dialog_debug(&box);
    CHECK(box.text != NULL && (strstr(box.text, "gift") != NULL ||
                                strstr(box.text, "present") != NULL));
}

TEST(talk_scene_history_reports_real_favor_progress)
{
    TalkSceneDebug d;
    TalkQuest q;
    const char *kind;
    char label[64];
    int row, i, saw_goal = 0, money0;

    town();
    CHECK(open_talk(ID_NANA));
    /* Shared play is a remembered event, not an unfinished obligation. */
    talk_mem_note_shared(ID_NANA, TALK_SHARED_PLAYED);
    CHECK_EQ(talk_quest_open_count(), 0);
    go_to(find_row("more", -1, -1)); press(BTN_A);
    go_to(find_row("history", -1, -1)); press(BTN_A);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "history") == 0);
    row = find_row("historyinfo", -1, -1);
    CHECK(row >= 0);
    CHECK(talk_scene_row_debug(row, &kind, NULL, NULL, label));
    CHECK(strstr(label, "0 open") != NULL);

    /* An actual open fetch request reports its specific goal when picked. */
    CHECK(talk_quest_derive(ID_NANA, TQK_FETCH, NULL, 0, 0, &q));
    CHECK(q.item > ITEM_NONE);
    while (inv_count(q.item) > 0) CHECK(inv_take(q.item, 1));
    CHECK(talk_mem_quest_set(ID_NANA, TQK_FETCH, TQS_ACTIVE, 3, 0));
    money0 = (int)doomo_town.money;
    press(BTN_B);  /* Our History -> Options */
    go_to(find_row("history", -1, -1)); press(BTN_A);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "history") == 0);
    row = find_row("historyinfo", -1, -1);
    CHECK(row >= 0);
    CHECK(talk_scene_row_debug(row, NULL, NULL, NULL, label));
    CHECK(strstr(label, "1 open") != NULL);
    row = find_row("historyquest", TQK_FETCH, -1);
    CHECK(row >= 0);
    CHECK(talk_scene_row_debug(row, NULL, NULL, NULL, label));
    CHECK(strstr(label, "active") != NULL);
    go_to(row); press(BTN_A);
    for (i = 0; ui_toast_text(i); i++)
        if (strstr(ui_toast_text(i), "Bring") != NULL) saw_goal = 1;
    CHECK(saw_goal);
    CHECK_EQ((int)doomo_town.money, money0);

    /* Receiving the requested item while chatting updates the journal.
     * An unready request can be inspected without consequence; selecting
     * one that IS ready now starts the real thank-you and hand-in flow.
     * The other end-to-end test verifies payout after reading that reply. */
    CHECK(inv_add(q.item, 1) >= 1);
    press(BTN_B);
    go_to(find_row("history", -1, -1)); press(BTN_A);
    row = find_row("historyquest", TQK_FETCH, -1);
    CHECK(row >= 0);
    CHECK(talk_scene_row_debug(row, NULL, NULL, NULL, label));
    CHECK(strstr(label, "ready") != NULL);
    go_to(row); press(BTN_A);
    CHECK(talk_scene_debug(&d) && d.busy); /* a resident really answers */
    CHECK_EQ(inv_count(q.item), 1);        /* no premature consumption */
    CHECK_EQ((int)doomo_town.money, money0); /* no premature reward */
}

TEST(talk_scene_open_favor_is_accessible_from_main)
{
    TalkSceneDebug d;
    char label[64];
    int row, item;
    TalkQuest request;

    town();
    CHECK(open_talk(ID_NANA));
    CHECK_EQ(find_row("history", 1, -1), -1);  /* no fake goals */
    CHECK(talk_quest_derive(ID_NANA, TQK_FETCH, NULL, 0, 0, &request));
    item = request.item;
    CHECK(item > ITEM_NONE);
    while (inv_count(item) > 0) CHECK(inv_take(item, 1));
    CHECK(talk_mem_quest_set(ID_NANA, TQK_FETCH, TQS_ACTIVE, 3, 0));

    /* The main page is built on returning from Options; check that
     * it now exposes THIS person's actual ongoing favor. */
    go_to(find_row("more", -1, -1)); press(BTN_A);
    press(BTN_B);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "main") == 0);
    row = find_row("history", 1, TQS_ACTIVE);
    CHECK(row >= 0);
    CHECK(talk_scene_row_debug(row, NULL, NULL, NULL, label));
    CHECK(strstr(label, "Your favor") != NULL);
    go_to(row); press(BTN_A);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "history") == 0);
    CHECK_EQ(d.sel, find_row("historyquest", TQK_FETCH, -1));
    press(BTN_B);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "more") == 0);

    /* The player picks up the real requested item: next main-page
     * refresh immediately advertises readiness, without spending it. */
    CHECK(inv_add(item, 1) >= 1);
    press(BTN_B);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "main") == 0);
    row = find_row("history", 1, TQS_READY);
    CHECK(row >= 0);
    CHECK(talk_scene_row_debug(row, NULL, NULL, NULL, label));
    CHECK(strstr(label, "Favor ready") != NULL);
    CHECK_EQ(inv_count(item), 1);
}

TEST(talk_scene_hands_in_a_real_favor_without_leaving_chat)
{
    TalkSceneDebug d;
    TalkQuest q;
    int row, k, cash, before, kind, state;
    town();
    CHECK(open_talk(ID_NANA));
    CHECK(talk_quest_derive(ID_NANA, TQK_FETCH, NULL, 0, 0, &q));
    CHECK(q.item > ITEM_NONE);
    while (inv_count(q.item) > 0) CHECK(inv_take(q.item, 1));
    CHECK(talk_mem_quest_set(ID_NANA, TQK_FETCH, TQS_ACTIVE, 3, 0));
    CHECK(inv_add(q.item, 1) >= 1);
    before = inv_count(q.item);
    cash = (int)doomo_town.money;

    go_to(find_row("more", -1, -1)); press(BTN_A);
    press(BTN_B);                 /* refresh the main action list */
    row = find_row("history", 1, TQS_READY);
    CHECK(row >= 0);
    go_to(row); press(BTN_A);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "history") == 0);
    row = find_row("historyquest", TQK_FETCH, -1);
    CHECK(row >= 0);
    CHECK_EQ(d.sel, row);
    press(BTN_A);                 /* present the item NOW, not on the next visit */
    CHECK(talk_scene_debug(&d) && d.busy);
    CHECK_EQ((int)doomo_town.money, cash); /* reward waits for the real response */
    for (k = 0; k < 50 && talk_scene_debug(&d) && d.busy; k++) {
        frames(0, 90);
        press(BTN_A);
    }
    CHECK(talk_scene_debug(&d) && !d.busy);
    CHECK_STR(d.page, "main");
    CHECK_EQ((int)doomo_town.money, cash + talk_quest_reward(TQK_FETCH, 0));
    CHECK_EQ(inv_count(q.item), before - 1);
    CHECK(talk_quest_state(ID_NANA, &kind, &state, NULL, NULL));
    CHECK_EQ(state, TQS_SETTLED);
    CHECK_EQ(find_row("history", 1, -1), -1); /* no fake ready reward left */
}

TEST(talk_scene_history_topic_is_a_real_followup_question)
{
    TalkSceneDebug d;
    uint16_t concept = 0, wf = 0;
    uint32_t tags[4] = {0, 0, 0, 0};
    int row;
    town();
    CHECK(open_talk(ID_NANA));
    CHECK(talk_lex_find("pie", &concept, &wf, tags) >= 0 && concept != TC_NONE);
    talk_mem_note_topic(ID_NANA, concept);
    go_to(find_row("more", -1, -1));
    press(BTN_A);
    row = find_row("history", -1, -1);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    CHECK(talk_scene_debug(&d));
    CHECK_STR(d.page, "history");
    row = find_row("historytopic", 0, concept);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    CHECK(talk_scene_debug(&d) && d.busy);
    CHECK(strstr(d.you, "pie") != NULL); /* asks the resident, not a fake transcript */
}

TEST(talk_scene_linked_topics_ask_and_continue_the_conversation)
{
    TalkSceneDebug d;
    uint16_t band = 0, song = 0, lyrics = 0, wf = 0;
    uint32_t tags[4];
    int i, row;
    town();
    CHECK(open_talk(ID_NANA));
    CHECK(talk_lex_find("band", &band, &wf, tags) >= 0 && band != TC_NONE);
    CHECK(talk_lex_find("song", &song, &wf, tags) >= 0 && song != TC_NONE);
    CHECK(talk_lex_find("lyrics", &lyrics, &wf, tags) >= 0 && lyrics != TC_NONE);
    go_to(find_row("more", -1, -1)); press(BTN_A);
    press(BTN_A); /* Topic */
    row = find_row("pick", 0, band);
    CHECK(row >= 0);
    go_to(row); press(BTN_A);
    CHECK(talk_scene_debug(&d) && !d.busy);
    go_to(find_row("more", -1, -1)); press(BTN_A);
    press(BTN_A); /* Topic */
    row = find_row("topiclink", 0, song);
    CHECK(row >= 0);
    go_to(row); press(BTN_A);
    CHECK(talk_scene_debug(&d) && d.busy);
    CHECK(strstr(d.you, "song") != NULL);
    for (i = 0; i < 40 && talk_scene_debug(&d) && d.busy; i++) {
        frames(0, 90); press(BTN_A);
    }
    CHECK(talk_scene_debug(&d) && !d.busy);
    go_to(find_row("more", -1, -1)); press(BTN_A);
    press(BTN_A); /* Topic */
    row = find_row("topiclink", 0, lyrics);
    CHECK(row >= 0);
    go_to(row); press(BTN_A);
    CHECK(talk_scene_debug(&d) && d.busy);
    CHECK(strstr(d.you, "lyric") != NULL);
}

TEST(talk_scene_history_repeat_play_really_changes_simulation)
{
    TalkSceneDebug d;
    DoomoResident *p;
    int before_happy, before_energy, row;
    town();
    p = town_res(ID_NANA);
    p->status = RES_ALIVE;
    p->stat[STAT_ENERGY] = 80;
    p->stat[STAT_HAPPY] = 45;
    p->stat[STAT_SOCIAL] = 40;
    CHECK(open_talk(ID_NANA));
    talk_mem_note_shared(ID_NANA, TALK_SHARED_PLAYED);
    go_to(find_row("more", -1, -1)); press(BTN_A);
    go_to(find_row("history", -1, -1)); press(BTN_A);
    CHECK(talk_scene_debug(&d));
    CHECK_STR(d.page, "history");
    CHECK(find_row("historyrepeat", TALK_SHARED_PLAYED, -1) >= 0);
    CHECK_EQ(find_row("historyrepeat", TALK_SHARED_GIFTED, -1), -1);
    CHECK_EQ(find_row("historyrepeat", TALK_SHARED_INVITED, -1), -1);
    before_happy = p->stat[STAT_HAPPY];
    before_energy = p->stat[STAT_ENERGY];
    row = find_row("historyrepeat", TALK_SHARED_PLAYED, -1);
    go_to(row); press(BTN_A);
    CHECK(p->stat[STAT_HAPPY] > before_happy);
    CHECK(p->stat[STAT_ENERGY] < before_energy);
    CHECK(talk_scene_debug(&d) && d.busy);
}

TEST(talk_scene_offer_and_give_are_real_actions)
{
    TalkSceneDebug d;
    int row;
    town();
    CHECK(inv_add(ITEM_PLUSHIE, 1) >= 1);
    CHECK(open_talk(ID_NANA));
    /* Be nice -> Offer help is a real care menu, not a canned promise. */
    row = find_row("cat", talk_scene_act_cat(TA_OFFER), -1);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    row = find_row("act", TA_OFFER, -1);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "care") == 0);
    CHECK(find_row("comfort", -1, -1) >= 0);
    press(BTN_B);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "main") == 0);

    /* Gestures -> Give requires a real item in the bag. The item is not
     * consumed until its specific inventory row is selected. */
    row = find_row("cat", talk_scene_act_cat(TA_GIVE), -1);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    row = find_row("act", TA_GIVE, -1);
    CHECK(row >= 0);
    go_to(row);
    press(BTN_A);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "give") == 0);
    CHECK(find_row("giveitem", ITEM_PLUSHIE, -1) >= 0);
    CHECK(inv_count(ITEM_PLUSHIE) >= 1);
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
        const char *b = strstr(t, "Bond ");
        if (b) {                                           /* the sign is printed by hand: "+2", "-1" or "0" */
            found = 1;
            CHECK(b[5] == '+' || b[5] == '-' || b[5] == '0');
            CHECK(b[5] == '0' || (b[6] >= '0' && b[6] <= '9'));
        }
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
/* TUI-TALK: the screen shows one small control card (A Pick, B Goodbye on the replies page, B Back below it),
 * and no mood word while the room is calm */
TEST(talk_scene_has_a_control_card_on_every_page)
{
    UiPromptStats st;
    TalkSceneDebug d;
    town();
    CHECK(open_talk(ID_NANA));
    ui_prompt_stats(NULL, 1);
    frames(0, 2);
    ui_prompt_stats(&st, 0);
    CHECK(st.bars >= 1);
    CHECK(strstr(st.last, "Pick") != NULL);
    CHECK(strstr(st.last, "Goodbye") != NULL);
    CHECK_EQ(st.too_wide, 0);
    go_to(find_row("cat", -1, -1));                 /* a category page: B goes back, not goodbye */
    press(BTN_A);
    CHECK(talk_scene_debug(&d) && strcmp(d.page, "acts") == 0);
    ui_prompt_stats(NULL, 1);
    frames(0, 2);
    ui_prompt_stats(&st, 0);
    CHECK(strstr(st.last, "Back") != NULL);
    CHECK(strstr(st.last, "Goodbye") == NULL);
}

/* TD2: talk_scene_slice(1) (the Dreamcast: a generator step is 3 to 17 ms there, an answer of three steps a tick over
 * 33.3 ms) runs the answer one step a tick. The answer comes a few ticks later than at once, the screen reports it
 * as busy meanwhile, no press changes anything meanwhile, and the answer is a real one. Without the slice the box
 * is up on the tick after the press. */
static int ticks_to_box(UiDialogDebug *dd)
{
    int k;
    for (k = 0; k < 40; k++) {
        memset(dd, 0, sizeof *dd);
        ui_dialog_debug(dd);
        if (dd->text && dd->text[0]) return k;
        frames(0, 1);
    }
    return -1;
}

extern int plat_native_talk_loading;

static int count_toast(const char *m)
{
    int i, n = 0;
    for (i = 0; ui_toast_text(i); i++) if (!strcmp(ui_toast_text(i), m)) n++;
    return n;
}

TEST(talk_note_says_warming_up_while_the_bank_loads_once)
{
    town();
    talk_db_close();
    plat_native_talk_loading = 1;
    CHECK(!home_talk_enter(ID_NANA));                       /* not ready: no scene ... */
    frames(0, 2);
    {
        TalkSceneDebug d;
        CHECK(!talk_scene_debug(&d));
    }
    CHECK(home_talk_note());                                /* ... and a note instead of silence */
    CHECK_EQ(count_toast("Talk is warming up..."), 1);
    CHECK(home_talk_note());                                /* a second press adds no second note */
    CHECK_EQ(count_toast("Talk is warming up..."), 1);
    plat_native_talk_loading = 0;
    CHECK(!home_talk_note());                               /* the bank is in: no note, Talk opens */
    CHECK(home_talk_enter(ID_NANA));
    frames(0, 2);
    {
        TalkSceneDebug d;
        CHECK(talk_scene_debug(&d));                        /* the screen is open (the dialogue box may be on top) */
    }
}

TEST(talk_scene_slice_runs_the_answer_one_step_a_tick)
{
    UiDialogDebug dd;
    TalkSceneDebug d;
    int plain, sliced, sel, k;
    town();
    CHECK(open_talk(ID_NANA));
    go_to(find_row("quick", 0, -1));
    press(BTN_A);
    plain = ticks_to_box(&dd);
    CHECK(plain >= 0 && talk_scene_debug(&d) && d.nlines >= 1);
    town();
    talk_scene_slice(1);
    CHECK(open_talk(ID_NANA));                              /* the opener is sliced too */
    CHECK(top_is(&SCENE_TALK));
    go_to(find_row("quick", 0, -1));
    press(BTN_A);
    memset(&dd, 0, sizeof dd);
    ui_dialog_debug(&dd);
    CHECK(dd.text == NULL || !dd.text[0]);                  /* not up yet: the steps run in the next ticks */
    CHECK(talk_scene_debug(&d) && d.busy);                  /* but the list is not up either */
    sel = d.sel;
    for (k = 0; k < 2; k++) {                               /* the pad does nothing while the answer is made */
        frames(BTN_DOWN, 1);
        frames(0, 1);
        CHECK(talk_scene_debug(&d) && d.sel == sel);
        memset(&dd, 0, sizeof dd);
        ui_dialog_debug(&dd);
        if (dd.text && dd.text[0]) break;
    }
    sliced = ticks_to_box(&dd);
    CHECK(sliced >= 0 && sliced + 2 >= plain);              /* it comes, within a few ticks */
    CHECK(strlen(dd.text) > 2);
    CHECK(talk_scene_debug(&d) && d.busy && d.nlines >= 1 && d.reply_act > 0);
    for (k = 0; k < 40 && talk_scene_debug(&d) && d.busy; k++) { frames(0, 90); press(BTN_A); }
    CHECK(talk_scene_debug(&d) && !d.busy);                 /* the list is back after the answer */
    go_to(find_row("bye", -1, -1));                         /* a farewell still ends the talk when sliced */
    press(BTN_A);
    for (k = 0; k < 60 && top_is(&SCENE_TALK); k++) { frames(0, 90); press(BTN_A); }
    CHECK(!top_is(&SCENE_TALK));
    talk_scene_slice(0);
}

#endif /* !DOOMO_PLATFORM_GBA */
