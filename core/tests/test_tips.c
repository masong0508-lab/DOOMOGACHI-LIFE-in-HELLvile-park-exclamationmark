/* test_tips.c - CL4 (clarity C4 + C12): the first-time tip cards (tips.c), their seen flags in the
 * 'OPT2' chunk, the Options rows, and the Field Guide book (lib_help.c). The machine config record
 * side of the seen flags is tested with test_options_store.c's fake record. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/tips.h"
#include "doomo/plat_ext.h"
#include "doomo/options_store.h"
#include "doomo/library.h"
#include "doomo/save_budget.h"
#include "doomo/ending.h"
#include "../src/fp_island.h"

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
static void idle(int n) { while (n-- > 0) frame(0); }
static void press(uint32_t b) { frame(b); frame(0); }

/* a scene with nothing in it, so only the card reacts */
static int quiet_presses;
static void q_enter(int arg) { (void)arg; quiet_presses = 0; }
static void q_update(void) { if (btn_pressed(0xFFFFFFFFu)) quiet_presses++; }
static void q_draw(void) { plat_clear(PAL_UI_CREAM); }
static const DoomoScene SCENE_QUIET = { "QUIET", q_enter, NULL, q_update, q_draw, 0 };

TEST(cl4_tips_are_off_under_the_native_harness)
{
    test_reset();
    CHECK(plat_test_flags() & PLAT_TEST_NO_TIPS);
    CHECK_EQ(opt_get(OPT_TIPS), 1);                         /* on for players */
    CHECK(!tips_enabled());
    scene_set(&SCENE_QUIET, 0);
    CHECK_EQ(tip_once(TIP_CARE), 0);
    CHECK(!tip_active());
    tips_test_force(1);
    CHECK(tips_enabled());
    opt_set(OPT_TIPS, 0);                                   /* the Options row turns them off */
    CHECK(!tips_enabled());
    CHECK_EQ(tip_once(TIP_CARE), 0);
    opt_set(OPT_TIPS, 1);
}

TEST(cl4_tip_shows_once_eats_presses_and_is_remembered)
{
    int i;
    test_reset();
    tips_test_force(1);
    scene_set(&SCENE_QUIET, 0);
    idle(2);
    CHECK_EQ(tip_once(TIP_CARE), 1);
    CHECK(tip_active());
    CHECK_EQ(tip_current(), TIP_CARE);
    CHECK_STR(tip_name(TIP_CARE), "care");
    CHECK_EQ(tip_once(TIP_HOUSE), 0);                       /* one card at a time */
    press(BTN_A);                                           /* too early: eaten, the card stays */
    CHECK(tip_active());
    CHECK_EQ(quiet_presses, 0);
    for (i = 0; i < 20; i++) frame(0);
    ui_toast("a toast waits behind the card");
    frame(0);
    CHECK(ui_toast_pending());
    press(BTN_DOWN);                                        /* closes it; the scene never sees it */
    CHECK(!tip_active());
    CHECK_EQ(quiet_presses, 0);
    CHECK(tip_seen(TIP_CARE));
    CHECK(tips_dirty());
    CHECK_EQ(tip_once(TIP_CARE), 0);                        /* seen: never again */
    press(BTN_A);                                           /* the scene has the pad back */
    CHECK_EQ(quiet_presses, 1);
    tips_reset();                                           /* Options: show all tips again */
    CHECK(!tip_seen(TIP_CARE));
    CHECK_EQ(tip_once(TIP_CARE), 1);
}

TEST(cl4_tip_waits_for_a_dialogue_and_goes_with_its_scene)
{
    test_reset();
    tips_test_force(1);
    scene_set(&SCENE_QUIET, 0);
    ui_say(-1, "Hello!");
    CHECK(ui_dialog_active());
    CHECK_EQ(tip_once(TIP_PET), 0);                         /* a dialogue is up: later */
    while (ui_dialog_active()) press(BTN_A);
    CHECK_EQ(tip_once(TIP_PET), 1);
    scene_push(&SCENE_OPTIONS, 0);                          /* pushed over: hidden, still pending */
    frame(0);
    CHECK(!tip_active());
    CHECK_EQ(tip_current(), TIP_PET);
    scene_pop();
    frame(0);
    CHECK(tip_active());
    scene_set(&SCENE_OPTIONS, 0);                           /* its scene left: dropped */
    frame(0);
    CHECK_EQ(tip_current(), -1);
    CHECK(!tip_seen(TIP_PET));                              /* it was barely up: it comes back */
}

TEST(cl4_every_tip_has_a_voice_a_plain_line_and_fits_the_gba_screen)
{
    int id, scheme, pad;
    char buf[TIP_PLAIN_MAX];
    test_reset();
    for (pad = 0; pad < PAD_COUNT; pad++) {
        input_set_pad(pad);
        for (scheme = 0; scheme < 3; scheme++) {
            opt_set(OPT_CONTROL_SCHEME, scheme);
            for (id = 0; id < TIP_COUNT; id++) {
                const char *v = tip_voice_text(id), *p = tip_plain_text(id, buf, (int)sizeof buf);
                int lines;
                CHECK(v && v[0] && p && p[0] && tip_speaker(id));
                CHECK_MSG(strlen(v) <= 200 && strlen(p) <= 200, "tip %s: a line over 200 bytes", tip_name(id));
                /* the pad's own button names went in (C3): no token left, no button the pad lacks */
                CHECK_MSG(!strchr(p, '%'), "tip %s: a button token left", tip_name(id));
                CHECK_MSG(pad != PAD_DC || !strstr(p, "SELECT"), "tip %s: SELECT on a Dreamcast", tip_name(id));
                CHECK_MSG(pad != PAD_PS1 || !strstr(p, "L/R") , "tip %s: L/R on a PS1", tip_name(id));
                /* the card at 240x160 (tips.c: w 232, text 218 / 212 px): at most 7 text lines */
                lines = ui_text_wrap(0, 0, 232 - 14, v, 0, 0) + ui_text_wrap(0, 0, 232 - 20, p, 0, 0);
                CHECK_MSG(lines <= 7, "tip %s: %d lines at 240x160", tip_name(id), lines);
            }
        }
    }
    input_set_pad(PAD_PS1);
    CHECK(strstr(tip_plain_text(TIP_ISLAND, buf, (int)sizeof buf), "L1: news. R1: citizens. SELECT: family."));
    input_set_pad(PAD_DC);
    CHECK(strstr(tip_plain_text(TIP_PET, buf, (int)sizeof buf), "START+Y: the Boredex."));
    input_set_pad(PAD_GBA);
    CHECK(strstr(tip_plain_text(TIP_CARE, buf, (int)sizeof buf), "L/R: tabs."));
    input_set_pad(-1);
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_MODERN);
    CHECK(tip_voice_text(TIP_COUNT) == NULL);
    CHECK(tip_plain_text(TIP_COUNT, buf, (int)sizeof buf) == NULL);
}

/* STKOPT: on the one-stick pad the shooter tip names what the stick really does */
TEST(stkopt_shooter_tip_follows_the_stick_mode)
{
    char buf[TIP_PLAIN_MAX];
    test_reset();
    input_set_pad(PAD_DC);
    input_set_analog(1, 0, 0, 0, 0, 0);
    input_set_caps(INPUT_CAP_TRIGGERS, 0, 0);          /* one stick: no INPUT_CAP_RSTICK */
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_AUTO);
    opt_set(OPT_STICK_MODE, STICK_TURN);
    CHECK(strstr(tip_plain_text(TIP_SHOOTER, buf, (int)sizeof buf), "walk and turn"));
    opt_set(OPT_STICK_MODE, STICK_STRAFE);
    CHECK(strstr(tip_plain_text(TIP_SHOOTER, buf, (int)sizeof buf), "walk and step aside"));
    CHECK(strstr(tip_plain_text(TIP_SHOOTER, buf, (int)sizeof buf), "triggers: turn"));
    opt_set(OPT_STICK_MODE, STICK_TURN);
    input_set_analog(0, 0, 0, 0, 0, 0);
    input_set_pad(-1);
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_MODERN);
}

TEST(cl4_options_game_page_has_tips_and_show_again)
{
    test_reset();
    scene_set(&SCENE_OPTIONS, OPTPAGE_GAME);
    idle(4);
    /* the page ends ... Tips, Show all tips again, Mature, then "Reset page" */
    press(BTN_UP);                                          /* wraps to "Reset page" */
    press(BTN_UP);                                          /* Mature */
    press(BTN_UP);                                          /* Show all tips again */
    press(BTN_UP);                                          /* Tips */
    CHECK_EQ(opt_get(OPT_TIPS), 1);
    press(BTN_LEFT);
    CHECK_EQ(opt_get(OPT_TIPS), 0);
    press(BTN_RIGHT);
    CHECK_EQ(opt_get(OPT_TIPS), 1);
    tip_seen_merge(1u << TIP_SHOOTER);
    press(BTN_DOWN);                                        /* "Show all tips again" */
    press(BTN_LEFT);                                        /* not a value row: nothing changes */
    CHECK(tip_seen(TIP_SHOOTER));
    press(BTN_A);
    CHECK(scene_top() == &SCENE_CONFIRM);
    idle(2);
    press(BTN_LEFT);                                        /* Yes */
    press(BTN_A);
    idle(2);
    CHECK(scene_top() == &SCENE_OPTIONS);
    CHECK(!tip_seen(TIP_SHOOTER));
}

TEST(cl4_field_guide_pages_open_flip_and_close)
{
    int i, n;
    test_reset();
    n = lib_help_pages();
    CHECK(n >= 14);
    scene_set(&SCENE_LIB_HELP, 0);
    idle(4);
    CHECK(scene_top() == &SCENE_LIB_HELP);
    for (i = 0; i < n; i++) idle(1), press(BTN_DOWN);       /* the list wraps round */
    press(BTN_A);                                           /* a page */
    idle(14);
    press(BTN_RIGHT);                                       /* the next page */
    idle(10);
    press(BTN_DOWN);                                        /* scroll */
    press(BTN_B);                                           /* close the page */
    idle(2);
    CHECK(scene_top() == &SCENE_LIB_HELP);
    /* every page draws at both sizes without leaving the book */
    for (i = 0; i < n; i++) {
        scene_set(&SCENE_LIB_HELP, HELP_ARG_PAGE(i));
        idle(16);
        CHECK(scene_top() == &SCENE_LIB_HELP);
    }
    /* B on the list leaves (alone on the stack: to the Library) */
    scene_set(&SCENE_LIB_HELP, 0);
    idle(4);
    press(BTN_B);
    idle(2);
    CHECK(scene_top() == &SCENE_LIBRARY);
}

/* the island hub keeps its frames (docs/ISLAND_SPEED.md I7/I8) with a steady card on them: the card is on
 * the kept page (tip_steady / tip_hold), clear of the news strip that repaints itself on kept frames */
TEST(cl4_island_hub_keeps_its_frames_under_a_card)
{
    int held0, held_alone, held_card;
    test_reset();
    fp_new_island(TOWN_POP_PREMADE, 4242);
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
    idle(60);                                               /* the hub settles (tips off) */
    held0 = fp_island_hub_held();
    idle(90);
    held_alone = fp_island_hub_held() - held0;
    tips_test_force(1);
    idle(30);                                               /* the card boings in and settles */
    CHECK_EQ(tip_current(), TIP_ISLAND);
    held0 = fp_island_hub_held();
    idle(90);
    held_card = fp_island_hub_held() - held0;
    printf("      island hub kept frames in 90 ticks: %d alone, %d with the card up\n", held_alone, held_card);
    CHECK(held_card * 10 >= held_alone * 8);                /* at least 80% as many kept frames */
    press(BTN_A);
    CHECK(!tip_active());
}

/* the card stays clear of the bottom band at both sizes (the island's news strip, hint pills) */
TEST(cl4_card_clears_the_bottom_band)
{
    static const int res[2][2] = { { 240, 160 }, { 320, 240 } };
    int r, y, x, w;
    for (r = 0; r < 2; r++) {
        NatOptions o;
        memset(&o, 0, sizeof(o));
        o.w = res[r][0]; o.h = res[r][1]; o.seed = 1; o.quiet = 1; o.save_dir = test_tmp_dir;
        nat_init(&o);
        test_reset();
        tips_test_force(1);
        scene_set(&SCENE_QUIET, 0);
        idle(2);
        CHECK_EQ(tip_once(TIP_ISLAND), 1);                  /* the tallest card */
        idle(20);
        /* the quiet scene clears to cream every frame: the band under the card must still be that */
        w = res[r][0];
        for (y = res[r][1] - 14; y < res[r][1]; y++)
            for (x = 0; x < w; x++)
                CHECK_MSG(nat_fb[y * w + x] == PAL_UI_CREAM, "card pixel at %d,%d (%dx%d)", x, y, w, res[r][1]);
    }
    {
        NatOptions o;
        memset(&o, 0, sizeof(o));
        o.w = 240; o.h = 160; o.seed = 1; o.quiet = 1; o.save_dir = test_tmp_dir;
        nat_init(&o);
    }
    test_reset();
}
