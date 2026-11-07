/* test_home_score_opaque.c - the Score window is named to the room renderer (owner: gba speed, nov07z3).
 *
 * gba/src/room.c skips its per-frame check and restore of the room under an opaque rect that a scene
 * registers BEFORE plat_room() in the same frame (plat_room_ui_opaque, C7.3). SCENE_HOME used to register
 * only its HUD bars (after plat_room), so a static Score window made the GBA redraw the room under it
 * every tick (home_modal: 51 ms, 17.5 drawn fps against a gate of 18). The host and the PS1 stub the call,
 * so this test is what keeps the registration right: only while the Score panel is up, before plat_room,
 * one inset rect that lies wholly inside the opaque window body. */
#include "test.h"
#include "../src/home_int.h"

/* strong versions of the two weak platform calls: log, then do what the weak ones do */
typedef struct { int x, y, w, h, after_room; } OpqCall;
static OpqCall opq[16];
static int opq_n;
static int room_seen;                          /* plat_room has run in this frame */

void plat_room(const DoomoRoomView *v)
{
    room_seen = 1;
    room_ref_draw(v);
}

void plat_room_ui_opaque(int x, int y, int w, int h)
{
    if (opq_n < (int)DOOMO_COUNTOF(opq)) {
        OpqCall *c = &opq[opq_n++];
        c->x = x; c->y = y; c->w = w; c->h = h; c->after_room = room_seen;
    }
}

/* one tick with `pad` held for it; returns how many window registrations (not the full-width HUD bars) it made */
static int frame(uint32_t pad)
{
    int i, n = 0, sw = plat_screen_w();
    opq_n = 0;
    room_seen = 0;
    plat_begin_frame();
    doomo_frame(pad);
    plat_end_frame();
    for (i = 0; i < opq_n; i++)
        if (!(opq[i].x == 0 && opq[i].w == sw)) n++;
    return n;
}

static void frames(int n)
{
    while (n-- > 0) frame(0);
}

/* the window draw_menu draws (score_box), inset by the corner radius */
static void expect_box(int *x, int *y, int *w, int *h)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    int bw = sw >= 300 ? 230 : 196, bh = sh >= 200 ? 170 : 128;
    *x = (sw - bw) / 2 + 4;
    *y = (sh - bh) / 2 + 4;
    *w = bw - 8;
    *h = bh - 8;
}

static void enter_house(void)
{
    test_reset();
    home_reset();
    scene_set(&SCENE_HOME, -1);
    frames(30);                                /* past the pop-in (12 ticks) and the iris */
}

TEST(home_score_window_is_named_to_the_room_before_plat_room)
{
    int i, ex, ey, ew, eh, seen = 0;
    enter_house();
    CHECK(scene_top() == &SCENE_HOME);
    frame(BTN_START); frames(14);              /* the main menu */
    frame(BTN_DOWN); frames(4);
    frame(BTN_DOWN); frames(4);
    frame(BTN_A);                              /* Score */
    expect_box(&ex, &ey, &ew, &eh);
    for (i = 0; i < 20; i++) {
        int n = frame(0), k;
        CHECK_MSG(n == 1, "tick %d: %d window registrations, expected 1", i, n);
        for (k = 0; k < opq_n; k++) {
            if (opq[k].x == 0 && opq[k].w == plat_screen_w()) continue;     /* a HUD bar */
            CHECK_EQ(opq[k].x, ex);
            CHECK_EQ(opq[k].y, ey);
            CHECK_EQ(opq[k].w, ew);
            CHECK_EQ(opq[k].h, eh);
            CHECK_MSG(!opq[k].after_room, "tick %d: the window was named after plat_room", i);
            seen++;
        }
    }
    CHECK_EQ(seen, 20);
}

TEST(home_score_registration_stops_with_the_window)
{
    int i;
    enter_house();
    for (i = 0; i < 10; i++) CHECK_EQ(frame(0), 0);          /* no menu: only the HUD bars */
    frame(BTN_START); frames(14);
    for (i = 0; i < 10; i++) CHECK_EQ(frame(0), 0);          /* the main menu is not the Score window */
    frame(BTN_DOWN); frames(4);
    frame(BTN_DOWN); frames(4);
    frame(BTN_A);
    CHECK_EQ(frame(0), 1);                                   /* Score is up */
    frame(BTN_B); frames(2);                                 /* closed */
    for (i = 0; i < 10; i++) CHECK_EQ(frame(0), 0);
    CHECK(scene_top() == &SCENE_HOME);
}

/* the registered body lies inside the window the player sees: the inset keeps the rounded corners (radius 4)
 * and the outline out of it, whatever the screen size */
TEST(home_score_registration_stays_inside_the_window)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    int bw = sw >= 300 ? 230 : 196, bh = sh >= 200 ? 170 : 128;
    int bx = (sw - bw) / 2, by = (sh - bh) / 2, ex, ey, ew, eh;
    expect_box(&ex, &ey, &ew, &eh);
    CHECK(ex >= bx + 4 && ey >= by + 4);
    CHECK(ex + ew <= bx + bw - 4 && ey + eh <= by + bh - 4);
    CHECK(ew > 0 && eh > 0);
}
