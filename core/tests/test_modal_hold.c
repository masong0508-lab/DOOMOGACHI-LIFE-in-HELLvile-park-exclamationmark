/* test_modal_hold.c - S2-1, the modal hold (ui.h ui_modal_hold; docs/GBA.md "Speed wave 2: S2-1").
 *
 * The native platform holds a frame the way the GBA does (plat_hold_frame(2): the picture just shown is put back
 * under every later frame), so the same scene is played twice, holds on and holds off (the native default), and:
 *   - holds really happen (a test that never holds proves nothing);
 *   - the modal's inside (its window inset by the corner radius) is the same picture on every tick of both runs:
 *     what the held path repaints is exactly what the full frame draws there;
 *   - a held frame is the previous frame everywhere outside the window: nothing is dimmed or shaded twice (no
 *     compounding dim or drop shadow), nothing outside it is drawn over the kept page;
 *   - every frame drawn in full before the first hold is the same in both runs.
 * Outside the window a held frame differs from the hold-off run by design (the dimmed backdrop's drift and the
 * scene's idle motion stand still: the pixel-changing part, GBA only). */
#include "test.h"
#include "../src/fp_int.h"
#include "../src/lib_int.h"

#define NF 150

typedef struct { int x, y, w, h; } Rect;

static uint32_t rect_hash(Rect r, int inside)
{
    uint32_t h = 2166136261u;
    int x, y;
    for (y = 0; y < nat_h; y++)
        for (x = 0; x < nat_w; x++) {
            int in = x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
            if (in == inside) h = (h ^ nat_fb[y * nat_w + x]) * 16777619u;
        }
    return h;
}

typedef struct {
    uint32_t in[NF], out[NF];
    uint8_t held[NF];
} Run;

typedef void (*SetupFn)(void);
typedef uint32_t (*PadFn)(int f);

static void run(int hold, SetupFn setup, PadFn pad, Rect win, Run *r)
{
    int f;
    Rect inner;
    doomo_init();
    test_reset();
    ui_modal_hold_enable(hold);
    setup();
    inner.x = win.x + 4; inner.y = win.y + 4; inner.w = win.w - 8; inner.h = win.h - 8;
    for (f = 0; f < NF; f++) {
        int before = ui_modal_hold_enable(-1);
        plat_begin_frame();
        doomo_frame(pad(f));
        plat_end_frame();
        r->held[f] = (uint8_t)(ui_modal_hold_enable(-1) > before);
        r->in[f] = rect_hash(inner, 1);
        r->out[f] = rect_hash(win, 0);
    }
    ui_modal_hold_enable(0);
}

static void compare(const char *what, SetupFn setup, PadFn pad, Rect win, int min_held)
{
    static Run a, b;
    int f, held = 0, first = -1;
    run(0, setup, pad, win, &b);              /* (a first run warms the caches both runs then share) */
    run(1, setup, pad, win, &a);
    run(0, setup, pad, win, &b);
    for (f = 0; f < NF; f++) {
        CHECK_MSG(!b.held[f], "%s: the hold-off run held tick %d", what, f);
        CHECK_MSG(a.in[f] == b.in[f], "%s: tick %d: the window's inside differs from the full frame", what, f);
        if (a.held[f]) {
            held++;
            if (first < 0) first = f;
            CHECK_MSG(f > 0 && a.out[f] == a.out[f - 1], "%s: held tick %d changed outside the window", what, f);
        } else if (first < 0) {
            CHECK_MSG(a.out[f] == b.out[f], "%s: tick %d (drawn in full, before any hold) differs", what, f);
        }
    }
    CHECK_MSG(held >= min_held, "%s: only %d of %d ticks held", what, held, NF);
}

/* ---- the shop's quantity popup ---------------------------------------------- */
static void shop_setup(void)
{
    fp_new_island(TOWN_POP_PREMADE, 4242);
    doomo_town.money = 5000;
    scene_set(&SCENE_SHOP, FP_SHOP_ARG_SHOP);
}

static uint32_t shop_pad(int f)
{
    if (f == 4) return BTN_A;                          /* the quantity popup */
    if (f == 50 || f == 60) return BTN_RIGHT;          /* one more, twice */
    return 0;
}

TEST(modal_hold_shop_quantity_popup)
{
    Rect r;
    r.w = fp_big() ? 190 : 160; r.h = 60; r.x = (fp_sw() - r.w) / 2; r.y = (fp_sh() - r.h) / 2;
    compare("shop", shop_setup, shop_pad, r, 50);
}

/* ---- a library detail page (the Bestiary's jar), with a page turn ------------ */
static void lib_setup(void)
{
    int t;
    for (t = THING_IMPLING; t <= THING_LORD_VAPULA; t++) unlock_grant_match(UL_CAT_GALLERY, GAL_DEMONS, t - THING_IMPLING);
    scene_set(&SCENE_LIB_BESTIARY, 0);
}

static uint32_t lib_pad(int f)
{
    if (f == 4) return BTN_A;                          /* the detail page */
    if (f == 50) return BTN_RIGHT;                     /* the next entry: a page turn */
    return 0;
}

TEST(modal_hold_library_detail_page)
{
    Rect r;
    r.x = 4; r.y = 18; r.w = plat_screen_w() - 8; r.h = plat_screen_h() - 33;
    compare("bestiary", lib_setup, lib_pad, r, 50);
}

/* ---- a fair title card: held whole (pixel-changing: its idle motion stops), a new tier redraws it ---------------- */
static void fair_setup(void)
{
    fp_new_island(TOWN_POP_PREMADE, 4242);
    scene_set(&SCENE_MG_CATCH, 1);
}

static uint32_t fair_pad(int f)
{
    return f == 40 ? BTN_RIGHT : 0;
}

TEST(modal_hold_fair_title_card)
{
    static Run a, b;
    Rect none = { 0, 0, 0, 0 };
    int f, held = 0;
    run(1, fair_setup, fair_pad, none, &a);
    run(0, fair_setup, fair_pad, none, &b);
    for (f = 0; f < NF; f++) {
        if (a.held[f]) {
            held++;
            CHECK_MSG(a.out[f] == a.out[f - 1], "fair: held tick %d changed", f);
        } else {
            /* a full frame: the same picture as the hold-off run's on that tick */
            CHECK_MSG(a.out[f] == b.out[f], "fair: tick %d drawn in full differs", f);
        }
    }
    CHECK_MSG(!a.held[40] && a.held[41], "fair: the tier change was not drawn in full, then held");
    CHECK_MSG(held > 70, "fair: only %d of %d ticks held", held, NF);
}

/* ---- the house: the Catalog over the room (the HUD bands repaint, the room is the held frame) ----------------------- */
static void house_setup(void)
{
    scene_set(&SCENE_HOME, -1);
}

static uint32_t house_pad(int f)
{
    if (f == 30) return BTN_START;                     /* the START menu: Catalog is its first row */
    if (f == 44) return BTN_A;
    if (f == 120) return BTN_DOWN;                     /* the next piece (after the first-visit caption is gone) */
    return 0;
}

TEST(modal_hold_house_catalog)
{
    Rect r;
    r.x = 6; r.y = 16; r.w = plat_screen_w() - 12; r.h = plat_screen_h() - 34;
    compare("house", house_setup, house_pad, r, 40);
}

/* ---- a toast comes and goes over a held popup: the hold goes on (the band is put back each held tick) and the
 * toast leaves no trace (the picture after it is the one before it) -------------------------------------------- */
TEST(modal_hold_toast_over_a_held_popup)
{
    static Run a;
    Rect r;
    int f, held_toast = 0, before = -1, after = -1;
    r.w = fp_big() ? 190 : 160; r.h = 60; r.x = (fp_sw() - r.w) / 2; r.y = (fp_sh() - r.h) / 2;
    doomo_init();
    test_reset();
    ui_modal_hold_enable(1);
    shop_setup();
    for (f = 0; f < NF; f++) {
        int h0 = ui_modal_hold_enable(-1);
        if (f == 30) ui_toast("A toast over the popup");
        plat_begin_frame();
        doomo_frame(f == 4 ? BTN_A : 0);
        plat_end_frame();
        a.held[f] = (uint8_t)(ui_modal_hold_enable(-1) > h0);
        a.out[f] = rect_hash(r, 0);
        if (a.held[f] && ui_toast_pending()) held_toast++;
        if (f == 29) before = (int)a.held[f];
    }
    after = (int)a.held[NF - 1];
    ui_modal_hold_enable(0);
    CHECK_MSG(before && after, "toast: not held before (%d) or after (%d) the toast", before, after);
    CHECK_MSG(held_toast > 60, "toast: only %d ticks held while the toast showed", held_toast);
    CHECK_MSG(!ui_toast_pending(), "toast: still up at the end");
    CHECK_MSG(a.out[NF - 1] == a.out[29], "toast: the picture after the toast is not the one before it");
}
