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
#include "doomo/plat_ext.h"

/* strong versions of the two weak platform calls: log, then do what the weak ones do */
typedef struct { int x, y, w, h, after_room; } OpqCall;
static OpqCall opq[16];
static int opq_n;
static int room_seen;                          /* plat_room has run in this frame */

/* (S2-11) a platform that builds a turned view over several ticks (the GBA: gba/src/room.c) says so after
 * plat_room(): the tick is not presented, so the scene draws nothing over the room on it */
static int build_busy;                         /* what plat_build_frame(PLAT_BUILD_BUSY) answers */
static int busy_asked;                         /* ... and how often it was asked after plat_room in this frame */
static uint32_t room_fnv;                      /* the frame as plat_room left it */

static uint32_t fb_fnv(void)
{
    uint32_t h = 2166136261u;
    int i, n = nat_w * nat_h;
    for (i = 0; i < n; i++) h = (h ^ nat_fb[i]) * 16777619u;
    return h;
}

void plat_room(const DoomoRoomView *v)
{
    room_seen = 1;
    room_ref_draw(v);
    room_fnv = fb_fnv();
}

int plat_build_frame(int op)
{
    if (op != PLAT_BUILD_BUSY) return 0;
    if (room_seen) busy_asked++;
    return build_busy;
}

/* (S2-11) HomeMart and the Black Market ask the platform for its scaled-image / checker loops once a drawn frame
 * (the GBA: the UI group in IWRAM; its picture of the piece and the backdrop were drawn by slower code) */
static int ui_loops_asked;
int plat_ui_loops(void)
{
    ui_loops_asked++;
    return 0;
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
    busy_asked = 0;
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

/* (S2-11) the tick of a turn the platform is still building: plat_room() ran, the platform says "busy", and
 * the scene leaves the frame as plat_room left it (no HUD, no menu, no caption), as on a skipped tick. The
 * next tick, not busy, draws its HUD again. */
TEST(home_draws_nothing_over_the_room_on_a_build_tick)
{
    uint32_t after;
    enter_house();
    build_busy = 0;
    frame(0);
    CHECK(room_seen);
    CHECK_EQ(busy_asked, 1);                                 /* asked once, after plat_room */
    CHECK(fb_fnv() != room_fnv);                             /* the HUD bars went over the room's frame */
    build_busy = 1;
    frame(0);
    after = fb_fnv();
    build_busy = 0;
    CHECK(room_seen);
    CHECK_EQ(busy_asked, 1);
    CHECK_EQ_U(after, room_fnv);                             /* nothing drawn after plat_room */
    CHECK_EQ(frame(0), 0);                                   /* (and no window was named on the way) */
    CHECK(fb_fnv() != room_fnv);                             /* the HUD is back */
    CHECK(scene_top() == &SCENE_HOME);
}

/* (S2-11) HomeMart and the Black Market (Black Market: 33.2 ms of work a tick, a hair under the 33.3 limit,
 * 6 ms of it the preview picture through the C blit) ask for the platform's UI loops on every drawn frame */
TEST(home_mart_asks_for_the_platform_ui_loops_each_frame)
{
    test_reset();
    home_reset();
    scene_set(&SCENE_HOME_MART, 0);                     /* HMA_MART (home_mart.c) */
    frames(3);
    ui_loops_asked = 0;
    frames(4);
    CHECK_EQ(ui_loops_asked, 4);
    CHECK(scene_top() == &SCENE_HOME_MART);
    scene_set(&SCENE_HOME_MART, 1);                     /* HMA_BLACK */
    frames(3);
    ui_loops_asked = 0;
    frames(4);
    CHECK_EQ(ui_loops_asked, 4);
}
