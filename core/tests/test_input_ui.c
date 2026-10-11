/* test_input_ui.c - input edges/repeat, scene stack, printf, text wrap and
 * the UI modals driven through real input (owner: foundation). */
#include "test.h"

/* tick the whole game with these buttons */
static void frames(uint32_t b, int n)
{
    while (n-- > 0) {
        plat_begin_frame();
        doomo_frame(b);
        plat_end_frame();
    }
}

TEST(input_edges_and_hold)
{
    input_update(0);
    input_update(BTN_A);
    CHECK(btn_down(BTN_A) && btn_pressed(BTN_A) && !btn_released(BTN_A));
    CHECK(btn_pressed(BTN_A | BTN_B));
    CHECK(!btn_pressed(BTN_B));
    CHECK_EQ(btn_held_ticks(BTN_A), 1);
    input_update(BTN_A);
    CHECK(btn_down(BTN_A) && !btn_pressed(BTN_A));
    CHECK_EQ(btn_held_ticks(BTN_A), 2);
    input_update(0);
    CHECK(btn_released(BTN_A) && !btn_down(BTN_A));
    CHECK_EQ(btn_held_ticks(BTN_A), 0);
}

TEST(input_auto_repeat_12_then_every_3)
{
    int t, fired[60];
    input_update(0);
    for (t = 1; t <= 40; t++) {
        input_update(BTN_DOWN);
        fired[t] = btn_repeat(BTN_DOWN);
    }
    CHECK(fired[1]);                         /* the press itself */
    for (t = 2; t <= 12; t++) CHECK_MSG(!fired[t], "fired at %d", t);
    CHECK(fired[13]);
    CHECK(!fired[14] && !fired[15]);
    CHECK(fired[16]);
    CHECK(fired[19] && fired[22] && fired[25]);
    CHECK(!fired[20] && !fired[21]);
}

TEST(input_synth_xy_on_gba)
{
    input_update(DOOMO_INPUT_SYNTH_XY);
    input_update(DOOMO_INPUT_SYNTH_XY | BTN_SELECT);
    CHECK(btn_pressed(BTN_SELECT));
    input_update(DOOMO_INPUT_SYNTH_XY | BTN_SELECT | BTN_A);
    CHECK(btn_pressed(BTN_X));
    CHECK(!btn_down(BTN_A));
    input_update(DOOMO_INPUT_SYNTH_XY | BTN_SELECT | BTN_B);
    CHECK(btn_pressed(BTN_Y) && !btn_down(BTN_B) && !btn_down(BTN_X));
    input_update(DOOMO_INPUT_SYNTH_XY | BTN_A);
    CHECK(btn_pressed(BTN_A) && !btn_down(BTN_X));
    /* without the flag, raw X/Y pass through (PS1) */
    input_update(BTN_X);
    CHECK(btn_pressed(BTN_X));
}

TEST(input_eat_waits_for_release)
{
    input_update(0);
    input_update(BTN_A);
    input_eat();
    CHECK(!btn_pressed(BTN_A) && !btn_down(BTN_A));
    input_update(BTN_A);                      /* still held: ignored */
    CHECK(!btn_down(BTN_A) && !btn_pressed(BTN_A) && !btn_repeat(BTN_A));
    input_update(BTN_A | BTN_B);              /* other buttons work */
    CHECK(btn_pressed(BTN_B));
    input_update(0);
    input_update(BTN_A);                      /* released and pressed again */
    CHECK(btn_pressed(BTN_A));
}

static int enter_count, leave_count, last_arg;
static void t_enter(int arg) { enter_count++; last_arg = arg; }
static void t_leave(void) { leave_count++; }
static const DoomoScene scene_a = { "TEST_A", t_enter, t_leave, NULL, NULL, 0 };
static const DoomoScene scene_b = { "TEST_B", t_enter, t_leave, NULL, NULL, 1 };

TEST(scene_stack)
{
    enter_count = leave_count = 0;
    scene_set(&scene_a, 5);
    CHECK(scene_top() == &scene_a && scene_depth() == 1 && last_arg == 5);
    scene_push(&scene_b, 7);
    CHECK(scene_top() == &scene_b && scene_depth() == 2 && last_arg == 7);
    scene_update();
    scene_update();
    CHECK_EQ(scene_ticks(), 2);
    scene_draw();                              /* overlay draws both, no crash */
    scene_pop();
    CHECK(scene_top() == &scene_a);
    CHECK_EQ(leave_count, 1);
    scene_push(&scene_b, 0);
    scene_set(&scene_a, 1);                     /* leaves everything */
    CHECK_EQ(scene_depth(), 1);
    CHECK_EQ(leave_count, 4);
    CHECK(scene_by_name("UI_GALLERY") == &SCENE_UI_GALLERY);
    CHECK(scene_by_name("SCENE_TITLE") == &SCENE_TITLE);
    CHECK(scene_by_name("NOPE") == NULL);
    {
        int i;
        for (i = 0; doomo_all_scenes[i]; i++) {
            CHECK(doomo_all_scenes[i]->name && doomo_all_scenes[i]->name[0]);
            CHECK(scene_by_name(doomo_all_scenes[i]->name) == doomo_all_scenes[i]);
        }
        CHECK_EQ(i, 27);
    }
}

TEST(snprintf_formats)
{
    char b[64];
    CHECK_EQ(doomo_snprintf(b, sizeof(b), "%d|%i|%u", -42, 7, 3000000000u), 18);
    CHECK_STR(b, "-42|7|3000000000");
    doomo_snprintf(b, sizeof(b), "%3d|%-3d|%03d|%02d", 5, 5, 5, 123);
    CHECK_STR(b, "  5|5  |005|123");
    doomo_snprintf(b, sizeof(b), "%x %X %08x", 0xbeefu, 0xbeefu, 0x1fu);
    CHECK_STR(b, "beef BEEF 0000001f");
    doomo_snprintf(b, sizeof(b), "[%s][%5s][%-5s][%c]%%", "hi", "ab", "ab", 'z');
    CHECK_STR(b, "[hi][   ab][ab   ][z]%");
    doomo_snprintf(b, sizeof(b), "%d %s", -2147483647 - 1, (const char *)NULL);
    CHECK_STR(b, "-2147483648 (null)");
    CHECK_EQ(doomo_snprintf(b, 5, "hello world"), 11);   /* truncates */
    CHECK_STR(b, "hell");
    CHECK_EQ(doomo_snprintf(b, 1, "abc"), 3);
    CHECK_STR(b, "");
    doomo_snprintf(b, sizeof(b), "%-04d|%5", 3);
    CHECK_STR(b, "3   |");
}

TEST(text_measure_and_wrap)
{
    CHECK_EQ(font_text_width(""), 0);
    CHECK_EQ(font_text_width("a"), 5);
    CHECK_EQ(font_text_width("abc"), 17);
    CHECK_EQ(font_text_width("ab\nabcd"), 23);
    CHECK_EQ(ui_text_wrap(0, 0, 60, "hello world", 1, 0), 1);       /* 10 cols */
    CHECK_EQ(ui_text_wrap(0, 0, 60, "hello lovely world", 1, 0), 2);
    CHECK_EQ(ui_text_wrap(0, 0, 60, "a\nb\nc", 1, 0), 3);
    CHECK_EQ(ui_text_wrap(0, 0, 30, "abcdefghijkl", 1, 0), 3);      /* hard break */
    CHECK_EQ(ui_text_wrap(0, 0, 60, "", 1, 0), 0);
}

TEST(font_has_every_glyph)
{
    int c;
    for (c = 0x21; c <= 0x9F; c++) {
        int col, ink = 0;
        if (c == 0x7F) continue;
        for (col = 0; col < 5; col++) ink |= doomo_font[c - FONT_FIRST][col];
        CHECK_MSG(ink != 0, "glyph 0x%02X is blank", c);
    }
    /* descenders reach row 7 */
    CHECK(doomo_font['g' - FONT_FIRST][1] & 0x80);
    CHECK(doomo_font['y' - FONT_FIRST][1] & 0x80);
    CHECK(!(doomo_font['a' - FONT_FIRST][1] & 0x80));
}

static int confirm_result = -1;
static void on_confirm(int yes) { confirm_result = yes; }
static int entry_ok = -1;
static char entry_buf[DOOMO_NAME_LEN];
static void on_entry(int ok) { entry_ok = ok; }
static int choice_result = -1, say_finished;
static void on_choice(int c) { choice_result = c; }
static void on_said(void) { say_finished++; }

TEST(modal_confirm_via_input)
{
    scene_set(&SCENE_UI_GALLERY, 0);
    ui_confirm("Really?", on_confirm);
    CHECK(scene_top() == &SCENE_CONFIRM);
    frames(0, 15);
    frames(BTN_A, 1);                         /* default is No */
    frames(0, 1);
    CHECK_EQ(confirm_result, 0);
    CHECK(scene_top() == &SCENE_UI_GALLERY);
    ui_confirm("Really?", on_confirm);
    frames(0, 2);
    frames(BTN_LEFT, 1);
    frames(0, 1);
    frames(BTN_A, 1);
    frames(0, 1);
    CHECK_EQ(confirm_result, 1);
}

TEST(modal_text_entry_via_grid_and_feed)
{
    scene_set(&SCENE_UI_GALLERY, 0);
    memcpy(entry_buf, "Old", 4);
    ui_text_entry("Name?", entry_buf, DOOMO_NAME_LEN, on_entry);
    CHECK(scene_top() == &SCENE_TEXT_ENTRY);
    frames(0, 2);
    /* B deletes the old text ... */
    frames(BTN_B, 1); frames(0, 1);
    frames(BTN_B, 1); frames(0, 1);
    frames(BTN_B, 1); frames(0, 1);
    /* ... D-pad grid: 'A' is at 0,0; right x2 = 'C' */
    frames(BTN_A, 1); frames(0, 1);           /* 'A' (then lowercase) */
    frames(BTN_RIGHT, 1); frames(0, 1);
    frames(BTN_RIGHT, 1); frames(0, 1);
    frames(BTN_A, 1); frames(0, 1);           /* 'c' */
    CHECK(ui_text_entry_feed('t'));
    CHECK(ui_text_entry_feed('!'));
    CHECK(ui_text_entry_feed('\b'));
    {
        int i;                                /* overflow is refused */
        for (i = 0; i < 20; i++) ui_text_entry_feed('x');
    }
    CHECK(ui_text_entry_feed('\n'));
    frames(0, 1);
    CHECK_EQ(entry_ok, 1);
    CHECK_EQ((int)strlen(entry_buf), DOOMO_NAME_LEN - 1);
    CHECK(strncmp(entry_buf, "Act", 3) == 0);
    CHECK(!ui_text_entry_feed('z'));           /* closed */
    CHECK(scene_top() == &SCENE_UI_GALLERY);
}

TEST(dialog_queue_paging_and_choice)
{
    static const char *const opts[] = { "Yes", "No", "Maybe" };
    int guard;
    scene_set(&SCENE_UI_GALLERY, 0);
    resident_generate(town_res(1), 3);
    ui_say(1, "Hello there!");
    ui_say(-1, "A very long narrator line that will certainly need more than one "
               "page of the little dialogue box to be shown in full, so paging "
               "has to kick in at some point here.");
    ui_choice(1, "Pick?", opts, 3, on_choice);
    ui_say_done(on_said);
    CHECK(ui_dialog_active());
    CHECK(scene_top() == &SCENE_DIALOG);
    for (guard = 0; guard < 40 && ui_dialog_active(); guard++) {
        if (scene_top() == &SCENE_DIALOG) {
            frames(0, 3);
            frames(BTN_DOWN, 1);
            frames(0, 1);
            frames(BTN_A, 1);
            frames(0, 1);
        }
    }
    CHECK(!ui_dialog_active());
    CHECK(choice_result >= 1);                 /* moved down at least once */
    CHECK_EQ(say_finished, 1);
    CHECK(scene_top() == &SCENE_UI_GALLERY);
}

TEST(toasts_queue_and_expire)
{
    ui_toast("one");
    ui_toast("two");
    CHECK_EQ(ui_toast_pending(), 2);
    frames(0, 100);
    CHECK_EQ(ui_toast_pending(), 1);
    frames(0, 100);
    CHECK_EQ(ui_toast_pending(), 0);
    ui_toast("a"); ui_toast("b"); ui_toast("c"); ui_toast("d"); ui_toast("e");
    CHECK_EQ(ui_toast_pending(), 4);
}

TEST(gallery_and_boot_run_every_page)
{
    int p;
    scene_set(&SCENE_UI_GALLERY, 0);
    for (p = 0; p < 8; p++) {
        frames(0, 20);
        frames(BTN_A, 1); frames(0, 1);
        while (scene_depth() > 1) scene_pop();
        frames(BTN_R, 1); frames(0, 1);
    }
    scene_set(&SCENE_BOOT, 0);
    frames(0, 140);
    CHECK(scene_top() == &SCENE_TITLE);
}
