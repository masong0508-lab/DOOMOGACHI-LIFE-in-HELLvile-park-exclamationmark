/* test_sticks.c - ST1: real analog stick support.
 *   - the control scheme: Auto (the new default; stored 1, the old Modern default, reads as
 *     Auto) is Dual stick while an analog pad is plugged in and Modern otherwise, live;
 *     Classic / Modern picks win;
 *   - the shared dead zone + curve, the right stick's flick-with-repeat and
 *     the Dreamcast's trigger strafe;
 *   - every stick consumer: the shooter turns off the right stick (Auto, no setup) and
 *     strafes off the Dreamcast triggers, the overworld walks with analog speed, the house
 *     turns its view on a right-stick flick and the creator spins the citizen.
 */
#include "test.h"
#include "../src/combat_int.h"
#include "../src/overworld_int.h"
#include "../src/home_int.h"
#include "../src/creator_int.h"
#include "doomo/tips.h"
#include "doomo/options_store.h"

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
static void frames(int n, uint32_t b) { while (n-- > 0) frame(b); }
static void tick(uint32_t b) { input_update(b); combat_tick(); }
static void pad(int lx, int ly, int rx, int ry) { input_set_analog(1, lx, ly, rx, ry, input_stick_dpad(lx, ly)); }
static void no_pad(void) { input_set_analog(0, 0, 0, 0, 0, 0); }

TEST(sticks_scheme_auto_follows_the_pad)
{
    test_reset();
    no_pad();
    CHECK_EQ(doomo_option_info[OPT_CONTROL_SCHEME].def, CONTROLS_AUTO);
    CHECK_EQ(CONTROLS_AUTO, 1);                     /* old saves' Modern default reads as Auto */
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_AUTO);
    CHECK_EQ(controls_scheme(), CONTROLS_MODERN);
    pad(0, 0, 0, 0);
    CHECK_EQ(controls_scheme(), CONTROLS_DUAL_STICK);  /* plugged in: no setup */
    no_pad();
    CHECK_EQ(controls_scheme(), CONTROLS_MODERN);      /* pulled out: live */
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_MODERN);
    pad(0, 0, 0, 0);
    CHECK_EQ(controls_scheme(), CONTROLS_MODERN);      /* an explicit pick wins */
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_CLASSIC);
    CHECK_EQ(controls_scheme(), CONTROLS_CLASSIC);
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_DUAL_STICK);
    CHECK_EQ(controls_scheme(), CONTROLS_DUAL_STICK);
    no_pad();
    CHECK_EQ(controls_scheme(), CONTROLS_MODERN);
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_AUTO);
}

TEST(sticks_dead_zone_and_curve)
{
    int x, y, m, prev = 0, v;
    CHECK_EQ(stick_dz(0, 0, &x, &y), 0);
    CHECK_EQ(stick_dz(STICK_DEAD, 0, &x, &y), 0);
    CHECK(x == 0 && y == 0);
    CHECK_EQ(stick_dz(12, -12, &x, &y), 0);          /* a worn stick's drift */
    m = stick_dz(127, 0, &x, &y);
    CHECK(m == 256 && x == 256 && y == 0);
    m = stick_dz(-128, 0, &x, &y);
    CHECK(m == 256 && x == -256);
    m = stick_dz(0, -128, &x, &y);
    CHECK(y == -256 && x == 0);
    m = stick_dz(90, 90, &x, &y);                    /* the rim on a diagonal */
    CHECK(m == 256 && x > 170 && x < 190 && x == y);
    for (v = 0; v <= 127; v++) {                     /* monotonic, no jump past the dead zone */
        m = stick_dz(v, 0, &x, &y);
        CHECK(m >= prev && m - prev <= 3);
        prev = m;
    }
    CHECK_EQ(stick_curve(0), 0);
    CHECK_EQ(stick_curve(256), 256);
    CHECK_EQ(stick_curve(-256), -256);
    CHECK(stick_curve(64) < 64 && stick_curve(64) > 16);   /* fine near the centre */
    CHECK_EQ(stick_curve(-100), -stick_curve(100));
    CHECK_EQ(input_stick_dpad(-100, 0), (uint32_t)BTN_LEFT);
    CHECK_EQ(input_stick_dpad(0, 100), (uint32_t)BTN_DOWN);
    CHECK_EQ(input_stick_dpad(40, -40), 0u);
    /* no analog pad: the sticks read zero */
    no_pad();
    CHECK_EQ(input_lstick(&x, &y), 0);
    CHECK_EQ(input_caps(), 0u);
    /* a pad without a right stick (Dreamcast) */
    pad(0, 0, 127, 0);
    input_set_caps(INPUT_CAP_TRIGGERS, 10, 300);
    CHECK_EQ(input_rstick(&x, &y), 0);
    CHECK(input_triggers(&x, &y) && x == 10 && y == 255);
    no_pad();
}

TEST(sticks_flick_repeats)
{
    int i, n = 0, first = -1;
    test_reset();
    pad(0, 0, 127, 0);
    for (i = 0; i < 30; i++) {
        input_update(0);
        if (input_rflick()) { n++; if (first < 0) first = i; CHECK_EQ(input_rflick(), 1); }
    }
    CHECK_EQ(first, 0);                               /* fires the tick it is pushed */
    CHECK(n >= 4 && n <= 7);                          /* then repeats like btn_repeat */
    pad(0, 0, -127, 0);
    input_update(0);
    CHECK_EQ(input_rflick(), -1);                     /* the other way: at once */
    pad(0, 0, 60, 0);                                 /* under half way: nothing */
    for (i = 0; i < 20; i++) { input_update(0); CHECK_EQ(input_rflick(), 0); }
    /* eaten (a scene change): wait for the stick to come back to the centre */
    pad(0, 0, 127, 0);
    input_update(0);
    input_eat();
    for (i = 0; i < 20; i++) { input_update(0); CHECK_EQ(input_rflick(), 0); }
    pad(0, 0, 0, 0);
    input_update(0);
    pad(0, 0, 127, 0);
    input_update(0);
    CHECK_EQ(input_rflick(), 1);
    no_pad();
    input_update(0);
    CHECK_EQ(input_rflick(), 0);
}

TEST(sticks_dreamcast_trigger_strafe)
{
    CHECK_EQ(combat_trigger_strafe(0, 0), 0);
    CHECK_EQ(combat_trigger_strafe(0, 20), 0);        /* a resting trigger */
    CHECK_EQ(combat_trigger_strafe(0, 255), 256);
    CHECK_EQ(combat_trigger_strafe(255, 0), -256);
    CHECK_EQ(combat_trigger_strafe(255, 255), 0);     /* both in: lock on, no strafe */
    CHECK(combat_trigger_strafe(0, 128) > 0 && combat_trigger_strafe(0, 128) < 128);
    CHECK(combat_trigger_strafe(0, 200) > combat_trigger_strafe(0, 128));
}

/* the shooter, Auto (the default) with a DualShock: no setup */
TEST(sticks_combat_turns_off_the_right_stick)
{
    int t;
    int16_t a0;
    fix16 x0, y0;
    test_reset();
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_AUTO);
    combat_start_at(MAPID_ARENA, CGOAL_ENDLESS, 16, 22, 0);
    no_pad();
    a0 = doomo_player.angle;
    for (t = 0; t < 10; t++) tick(0);
    CHECK_EQ(doomo_player.angle, a0);
    pad(0, 0, 127, 0);                                /* right stick: turn */
    for (t = 0; t < 10; t++) tick(0);
    CHECK_MSG(doomo_player.angle != a0, "the right stick did not turn the view");
    /* an explicit Modern ignores the right stick */
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_MODERN);
    for (t = 0; t < 10; t++) tick(0);                 /* glide out */
    a0 = doomo_player.angle;
    for (t = 0; t < 10; t++) tick(0);
    CHECK_EQ(doomo_player.angle, a0);
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_AUTO);
    /* the left stick walks */
    pad(0, -127, 0, 0);
    x0 = doomo_player.x; y0 = doomo_player.y;
    for (t = 0; t < 10; t++) tick(0);
    CHECK(doomo_player.x != x0 || doomo_player.y != y0);
    no_pad();
}

TEST(sticks_combat_dreamcast_stick_and_triggers)
{
    int t;
    int16_t a0;
    fix16 x0, y0;
    test_reset();
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_AUTO);
    combat_start_at(MAPID_ARENA, CGOAL_ENDLESS, 16, 22, 0);   /* facing east (+x) */
    for (t = 0; t < 4; t++) tick(0);
    /* one stick: X turns */
    pad(127, 0, 0, 0);
    input_set_caps(INPUT_CAP_TRIGGERS, 0, 0);
    a0 = doomo_player.angle;
    x0 = doomo_player.x;
    for (t = 0; t < 8; t++) { input_set_caps(INPUT_CAP_TRIGGERS, 0, 0); tick(0); }
    CHECK_MSG(doomo_player.angle != a0, "the stick's X did not turn");
    CHECK(doomo_player.x - x0 < INT2FIX(2) && x0 - doomo_player.x < INT2FIX(2));   /* and did not strafe */
    /* the right trigger strafes (pressure = speed), the stick centred */
    combat_start_at(MAPID_ARENA, CGOAL_ENDLESS, 16, 22, 0);
    for (t = 0; t < 4; t++) tick(0);
    pad(0, 0, 0, 0);
    a0 = doomo_player.angle;
    y0 = doomo_player.y;
    for (t = 0; t < 10; t++) { input_set_caps(INPUT_CAP_TRIGGERS, 0, 255); tick(0); }
    CHECK_EQ(doomo_player.angle, a0);
    CHECK_MSG(doomo_player.y != y0, "the trigger did not strafe");
    no_pad();
}

/* the overworld: the left stick walks with analog speed */
static int32_t walk_x(int lx, int n)
{
    int i;
    int32_t x0 = ow.px;
    pad(lx, 0, 0, 0);
    for (i = 0; i < n; i++) frame(0);
    no_pad();
    return ow.px - x0;
}

TEST(sticks_overworld_analog_walk)
{
    int i;
    int32_t creep, walk, run;
    CHECK_EQ(ow_stick_speed_q8(0, 0), 0);
    CHECK(ow_stick_speed_q8(10, 0) >= 64 && ow_stick_speed_q8(10, 0) < 100);    /* creep */
    CHECK(ow_stick_speed_q8(200, 0) > 200 && ow_stick_speed_q8(200, 0) <= 256); /* walk */
    CHECK(ow_stick_speed_q8(256, 0) > 256);                                     /* run at the rim */
    CHECK(ow_stick_speed_q8(256, 1) == ow_stick_speed_q8(256, 0));
    for (i = 1; i < 232; i++) CHECK(ow_stick_speed_q8(i + 1, 0) >= ow_stick_speed_q8(i, 0));
    test_reset();
    scene_set(&SCENE_STORY, STORY_ARG_DEBUG_BEAT + BEAT_C1_FRIENDS);
    for (i = 0; i < 400 && scene_top() != &SCENE_STORY; i++) frame(i & 1 ? BTN_A : 0);
    frames(10, 0);
    CHECK(scene_top() == &SCENE_STORY);
    /* walk back and forth from the same spot so walls do not differ */
    creep = walk_x(40, 6);  walk_x(-40, 6);
    walk = walk_x(100, 6);  walk_x(-100, 6);
    run = walk_x(127, 6);   walk_x(-127, 6);
    CHECK_MSG(creep > 0 && walk > creep && run > walk, "creep %d walk %d run %d", (int)creep, (int)walk, (int)run);
    CHECK_EQ(ow.dir, 1);                              /* faces the way it walked (left) */
}

/* the house: a right-stick flick turns the view one step, as R */
TEST(sticks_house_view_turns_on_a_flick)
{
    int i, k0;
    test_reset();
    no_pad();
    scene_set(&SCENE_HOME, -1);
    frames(60, 0);
    for (i = 0; i < 20 && tip_active(); i++) { frame(BTN_A); frame(0); }
    frames(5, 0);
    k0 = home_view_k();
    pad(0, 0, 127, 0);
    frame(0);
    pad(0, 0, 0, 0);
    frames(20, 0);
    CHECK_EQ(home_view_k(), (k0 + 1) & 3);
    pad(0, 0, -127, 0);
    frame(0);
    pad(0, 0, 0, 0);
    frames(20, 0);
    CHECK_EQ(home_view_k(), k0);
    no_pad();
}

/* the creator: the right stick opens the turntable as a peek and spins the citizen */
static const DoomoScene base_scene = { "TESTBASE", NULL, NULL, NULL, NULL, 0 };

TEST(sticks_creator_spins_on_the_right_stick)
{
    int i, v0;
    test_reset();
    no_pad();
    scene_set(&base_scene, 0);
    scene_push(&SCENE_CREATOR, -1);
    frames(3, 0);
    for (i = 0; i < 20 && tip_active(); i++) { frame(BTN_A); frame(0); }
    frames(3, 0);
    CHECK_EQ(cr.phase, CRP_MAIN);
    pad(0, 0, 127, 0);
    frame(0);
    CHECK_EQ(cr.phase, CRP_TURN);
    v0 = cr.view8;
    frames(6, 0);
    CHECK_MSG(cr.view8 != v0, "the citizen did not turn");
    pad(0, 0, 0, 0);
    frames(40, 0);
    CHECK_EQ(cr.phase, CRP_MAIN);                     /* let go: back to the editor */
    no_pad();
}

/* (STKOPT) Options > Controls > Stick: Turn (default) / Strafe on the one-stick Dreamcast pad */
TEST(sticks_option_strafe_mode)
{
    int t;
    int16_t a0;
    fix16 y0;
    test_reset();
    CHECK_EQ(doomo_option2_info[OPT_STICK_MODE - OPT2_FIRST].def, STICK_TURN);
    CHECK_EQ(opt_get(OPT_STICK_MODE), STICK_TURN);          /* default = today's behaviour */
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_AUTO);
    opt_set(OPT_STICK_MODE, STICK_STRAFE);
    combat_start_at(MAPID_ARENA, CGOAL_ENDLESS, 16, 22, 0);   /* facing east (+x) */
    for (t = 0; t < 4; t++) tick(0);
    pad(127, 0, 0, 0);                                      /* stick X: strafes now, no turn */
    a0 = doomo_player.angle; y0 = doomo_player.y;
    for (t = 0; t < 10; t++) { input_set_caps(INPUT_CAP_TRIGGERS, 0, 0); tick(0); }
    CHECK_EQ(doomo_player.angle, a0);
    CHECK_MSG(doomo_player.y != y0, "the stick's X did not strafe");
    combat_start_at(MAPID_ARENA, CGOAL_ENDLESS, 16, 22, 0);
    for (t = 0; t < 4; t++) tick(0);
    pad(0, 0, 0, 0);                                        /* the right trigger turns */
    a0 = doomo_player.angle; y0 = doomo_player.y;
    for (t = 0; t < 10; t++) { input_set_caps(INPUT_CAP_TRIGGERS, 0, 255); tick(0); }
    CHECK_MSG(doomo_player.angle != a0, "the trigger did not turn");
    opt_set(OPT_STICK_MODE, STICK_TURN);
    no_pad();
}
