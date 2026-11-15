/* test_talk_pointer.c - T9: the pointer (mouse, touch) on the Talk screen. Owner: TK.
 * core/src/input.c (input_set_pointer, input_pointer_click), core/src/ui.c (ui_list_click),
 * core/src/talk_scene.c (pointer_click). docs/TALK.md "The pointer".
 *
 * A click must do exactly what the matching pad press does, and the pointer is never needed: every
 * target is also reached with the pad and both ways end on the same page, row and person.
 */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/lang.h"
#include "doomo/town.h"
#include "doomo/ui.h"

#define ID_NANA 1

static void frames(uint32_t b, int n)
{
    while (n-- > 0) {
        plat_begin_frame();
        doomo_frame(b);
        plat_end_frame();
    }
}

/* a click at (x, y): the button is down one tick, then up */
static void click(int x, int y)
{
    input_set_pointer(1, x, y, 1);
    frames(0, 1);
    input_set_pointer(1, x, y, 0);
    frames(0, 1);
}

static void press(uint32_t b)
{
    frames(b, 1);
    frames(0, 1);
}

/* the screen's geometry (talk_scene.c layout): the panel is 90 px high at the bottom */
static int ly(int slot) { return plat_screen_h() - 90 + 17 + slot * UI_LIST_ROW_H + 5; }
static int hy(void) { return plat_screen_h() - 90 + 5; }
#define LX 100
static int bar_x(void) { return plat_screen_w() - 10; }

static int view(int *page, int *sel, int *top, int *to, int *nrows, int *busy, char *you)
{
    return talk_scene_view(page, sel, top, to, nrows, busy, you);
}
static int page_of(void) { int p = -1; view(&p, NULL, NULL, NULL, NULL, NULL, NULL); return p; }
static int sel_of(void) { int p = -1; view(NULL, &p, NULL, NULL, NULL, NULL, NULL); return p; }
static int to_of(void) { int p = -9; view(NULL, NULL, NULL, &p, NULL, NULL, NULL); return p; }
static int rows_of(void) { int p = -1; view(NULL, NULL, NULL, NULL, &p, NULL, NULL); return p; }

/* open the Talk screen with Nana and page her greeting until the list is up */
static int open_talk(void)
{
    int i;
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    if (talk_db_open(lang_find("en")) != TALK_DB_OK && !talk_db_is_open()) return 0;
    if (!home_talk_enter(ID_NANA)) return 0;
    for (i = 0; i < 60; i++) {
        int busy = 1;
        if (scene_top() == &SCENE_TALK && view(NULL, NULL, NULL, NULL, NULL, &busy, NULL) && !busy) break;
        press(BTN_A);
        frames(0, 4);
    }
    return scene_top() == &SCENE_TALK;
}

TEST(pointer_click_is_the_press_edge)
{
    int x = 0, y = 0;
    input_reset();
    CHECK(!input_pointer(&x, &y));                       /* a platform without one: never */
    input_update(0);
    CHECK(!input_pointer_click(&x, &y));
    input_set_pointer(0, 5, 6, 1);                       /* not present: the button is ignored */
    input_update(0);
    CHECK(!input_pointer_click(&x, &y));
    input_set_pointer(1, 5, 6, 0);
    input_update(0);
    CHECK(input_pointer(&x, &y) && x == 5 && y == 6 && !input_pointer_click(&x, &y));
    input_set_pointer(1, 7, 8, 1);
    input_update(0);
    CHECK(input_pointer_click(&x, &y) && x == 7 && y == 8);
    input_update(0);                                     /* held: no second click */
    CHECK(!input_pointer_click(&x, &y));
    input_set_pointer(1, 7, 8, 0);
    input_update(0);
    input_set_pointer(1, 9, 9, 1);
    input_update(0);
    CHECK(input_pointer_click(&x, &y) && x == 9);
    input_eat();                                         /* a scene change swallows it like a press */
    CHECK(!input_pointer_click(&x, &y));
    input_reset();
    CHECK(!input_pointer(&x, &y));
}

TEST(pointer_list_rows_and_scroll_bar)
{
    UiList l;
    memset(&l, 0, sizeof l);
    l.count = 20;
    l.visible = 6;
    CHECK_EQ(ui_list_hit(&l, 6, 87, 228, 100, 87), 0);
    CHECK_EQ(ui_list_hit(&l, 6, 87, 228, 100, 87 + 12 * 5 + 11), 5);
    CHECK_EQ(ui_list_hit(&l, 6, 87, 228, 100, 87 + 12 * 6), -1);   /* below the last row */
    CHECK_EQ(ui_list_hit(&l, 6, 87, 228, 100, 86), -1);            /* above */
    CHECK_EQ(ui_list_hit(&l, 6, 87, 228, 5, 90), -1);              /* left of it */
    CHECK_EQ(ui_list_hit(&l, 6, 87, 228, 230, 90), -1);            /* the scroll bar is not a row */
    l.top = 7;
    CHECK_EQ(ui_list_hit(&l, 6, 87, 228, 100, 87 + 24), 9);
    l.top = 0;
    CHECK_EQ(ui_list_click(&l, 6, 87, 228, 100, 87 + 24), UI_SELECT);   /* a row: A on it */
    CHECK_EQ(l.sel, 2);
    CHECK_EQ(ui_list_click(&l, 6, 87, 228, 230, 87 + 60), UI_CHANGED);  /* lower half: R */
    CHECK_EQ(l.sel, 8);
    CHECK_EQ(l.top, 3);
    CHECK_EQ(ui_list_click(&l, 6, 87, 228, 230, 87 + 5), UI_CHANGED);   /* upper half: L */
    CHECK_EQ(l.sel, 2);
    CHECK_EQ(ui_list_click(&l, 6, 87, 228, 230, 300), UI_NONE);          /* off the list */
    l.count = 4;                                                         /* no bar: the edge is a row */
    l.top = 0;
    CHECK_EQ(ui_list_click(&l, 6, 87, 228, 230, 87 + 12), UI_SELECT);
    CHECK_EQ(l.sel, 1);
}

/* the same row through the pad: DOWN until it is selected, then A */
static void pad_to_row(int idx)
{
    int guard = 0;
    while (sel_of() != idx && guard++ < 40) press(BTN_DOWN);
}

TEST(pointer_talk_pages_rows_and_back)
{
    int n, chat, other;
    if (!open_talk()) { CHECK(!"the Talk screen opened"); return; }
    CHECK(scene_top() == &SCENE_TALK);
    CHECK_EQ(page_of(), 0);
    n = rows_of();
    chat = talk_scene_find("Chat >");
    CHECK(chat >= 4 && chat < 6);                        /* after the quick replies, still on the first page */
    /* a pointer that never clicks changes nothing */
    input_set_pointer(1, LX, ly(2), 0);
    frames(0, 3);
    CHECK_EQ(page_of(), 0);
    CHECK_EQ(sel_of(), 0);
    /* a click on the Chat row = DOWN to it + A: the acts page of the category */
    click(LX, ly(chat));
    CHECK_EQ(page_of(), 1);
    other = rows_of();
    CHECK(other >= 5);
    /* the header = B on a lower page */
    click(LX, hy());
    CHECK_EQ(page_of(), 0);
    CHECK_EQ(rows_of(), n);
    /* the same through the pad ends on the same page */
    pad_to_row(chat);
    press(BTN_A);
    CHECK_EQ(page_of(), 1);
    CHECK_EQ(rows_of(), other);
    press(BTN_B);
    CHECK_EQ(page_of(), 0);
    /* the header on the main page is not "Say goodbye" */
    click(LX, hy());
    CHECK(scene_top() == &SCENE_TALK && page_of() == 0);
    /* a stray click between the people strip and the panel does nothing */
    click(LX, 40);
    CHECK(scene_top() == &SCENE_TALK && page_of() == 0);
    /* a click on an act of the Chat page says it (the dialogue box opens), as A does */
    click(LX, ly(chat));
    CHECK_EQ(page_of(), 1);
    click(LX, ly(0));
    CHECK(scene_top() != &SCENE_TALK);                   /* the dialogue box is up */
}

TEST(pointer_talk_scroll_options_invite_people_typed_line)
{
    int busy, n, opt, i, to;
    if (!open_talk()) { CHECK(!"the Talk screen opened"); return; }
    /* more rows than fit: the scroll bar pages down (R) and up (L) */
    n = rows_of();
    CHECK(n > 6);
    click(bar_x(), ly(5));
    CHECK_EQ(sel_of(), 6);
    click(bar_x(), ly(0));
    CHECK_EQ(sel_of(), 0);
    for (i = 0; i < 4; i++) click(bar_x(), ly(5));       /* the last page: Type, Options, goodbye at the end */
    CHECK_EQ(sel_of(), n - 1);
    opt = talk_scene_find("Options");
    CHECK_EQ(opt, n - 2);
    click(LX, ly(4));                                    /* Options */
    CHECK_EQ(page_of(), 4);
    CHECK_EQ(rows_of(), 2);                              /* Topic, Invite someone (alone with Nana) */
    click(LX, ly(0));                                    /* Topic */
    CHECK_EQ(page_of(), 2);
    click(LX, hy());
    CHECK_EQ(page_of(), 0);
    for (i = 0; i < 4; i++) click(bar_x(), ly(5));
    click(LX, ly(4));
    click(LX, ly(1));                                    /* Invite someone */
    CHECK_EQ(page_of(), 3);
    CHECK(rows_of() >= 1);
    click(LX, ly(0));                                    /* the first person joins */
    CHECK(scene_top() != &SCENE_TALK);                   /* "Hey, what did I miss?" */
    for (i = 0; i < 40 && !(scene_top() == &SCENE_TALK && view(NULL, NULL, NULL, NULL, NULL, &busy, NULL) && !busy); i++) {
        press(BTN_A);
        frames(0, 4);
    }
    CHECK(scene_top() == &SCENE_TALK);
    CHECK_EQ(page_of(), 0);
    CHECK_EQ(to_of(), 0);
    /* the people strip: a click on the second name = SELECT until you speak to them */
    click(talk_scene_person_x(1), 10);
    CHECK_EQ(to_of(), 1);
    click(talk_scene_person_x(0), 10);
    CHECK_EQ(to_of(), 0);
    press(BTN_SELECT);                                   /* the pad: the next person */
    to = to_of();
    CHECK_EQ(to, 1);
    click(talk_scene_person_x(1), 10);                   /* already there: stays */
    CHECK_EQ(to_of(), 1);
    /* the typed line: the keyboard opens */
    for (i = 0; i < 4; i++) click(bar_x(), ly(5));
    click(LX, ly(3));
    CHECK(scene_top() == &SCENE_TEXT_ENTRY);
}

/* a quick reply: the pad (DOWN to the row, A) and the pointer (a click on it) say the same thing, the text the
 * row showed. The talk's seed differs between the two openings, so each is held to its own row. */
TEST(pointer_talk_quick_reply_equals_the_pad)
{
    char you[TALK_SHOWN_MAX], row[48];
    int k, i, way;
    for (way = 0; way < 2; way++) {
        for (k = 0; k < 3; k++) {
            if (!open_talk()) { CHECK(!"the Talk screen opened"); return; }
            talk_scene_row(k, row);
            CHECK(row[0]);
            if (way == 0) {
                for (i = 0; i < k; i++) press(BTN_DOWN);
                CHECK_EQ(sel_of(), k);
                press(BTN_A);
            } else {
                click(LX, ly(k));
            }
            CHECK(scene_top() != &SCENE_TALK);           /* the dialogue box with the answer is up */
            you[0] = 0;
            view(NULL, NULL, NULL, NULL, NULL, NULL, you);
            CHECK_MSG(strcmp(you, row) == 0, "way %d row %d: said \"%s\", the row was \"%s\"", way, k, you, row);
        }
    }
}
