/* test_panel.c - retained panels (G7, platform.h plat_panel_begin): the
 * native platform's panel cache (core/native/panel.c) keeps panels the way
 * the GBA does, so every frame of a run with it on must be pixel-identical
 * to the same run with it off. Covers the overworld HUD (objective, clock,
 * prompt), the dialogue box (narration, a portrait line with the name
 * pill, a choice), toasts and the island HUD and news ticker, at 240x160
 * and 320x240, and a language switch in the middle of a run; (SP8) the
 * shooter's HUD badges (face, health, ammo) through a fight in first and
 * third person: shooting, a reload, a weapon swap, damage, armour. Every
 * adopted panel must also follow the panel rules (no rule breaks) and be
 * kept on most frames. */
#include "test.h"
#include "../src/story_int.h"
#include "../src/fp_island.h"
#include "doomo/tips.h"
#include "../src/creator_int.h"
#include "../src/combat_int.h"     /* SP8: combat_hurt_player */
#include "doomo/library.h"         /* S2-8 */
#include "../src/weld_int.h"       /* S2-9: weld_pending */
#include "doomo/talk.h"

#define MAXF 420

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

static uint32_t fb_hash(void)
{
    uint32_t h = 2166136261u;
    int i, n = nat_w * nat_h;
    for (i = 0; i < n; i++) h = (h ^ nat_fb[i]) * 16777619u;
    return h;
}

typedef void (*Setup)(void);
typedef uint32_t (*Pad)(int f);

static int set_res(int w, int h)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = w; o.h = h; o.seed = 0x5EED1234u; o.quiet = 1;
    o.save_dir = test_tmp_dir;
    nat_init(&o);
    return 1;
}

static void story_town(void)
{
    test_reset();
    story_debug_autowin(1);
    town_new(TOWN_STORY, 4242, TOWN_POP_PREMADE);
    sim_init();
}

static int first_resident(void)
{
    int i;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        const DoomoResident *r = town_res(i);
        if (r && r->status != RES_EMPTY) return i;
    }
    return -1;
}

/* ---- scenarios ----------------------------------------------------------- */
static void s_visit(void) { story_town(); scene_set(&SCENE_STORY, 10); }
static uint32_t p_visit(int f)
{
    /* stand, walk about (the prompt comes and goes), a toast, stand */
    if (f == 150) ui_toast("A toast for the panels!");
    if (f >= 60 && f < 120) return (f / 20) % 2 ? BTN_RIGHT : BTN_DOWN;
    if (f >= 200 && f < 260) return (f / 15) % 2 ? BTN_LEFT : BTN_UP;
    return 0;
}

/* the invaded map: the objective pill with its bobbing star and a toast
 * sliding over both pills (the case INVADED_SPEED.md V7 measured) */
static void s_invaded(void) { story_town(); scene_set(&SCENE_STORY, 219); }
static uint32_t p_invaded(int f)
{
    if (f == 20) ui_toast("Walk into a nest to fight!");
    if (f == 200) ui_toast("A second toast, with a longer line to show.");
    return 0;
}

static void s_narration(void) { story_town(); scene_set(&SCENE_STORY, 110); }
static uint32_t p_narration(int f)
{
    if (f == 250 || f == 330) return BTN_A;           /* complete / advance */
    return 0;
}

static void s_portrait(void)
{
    static const char *const opts[3] = { "Yes", "No way", "Maybe?" };
    int id;
    story_town();
    scene_set(&SCENE_STORY, 10);
    id = first_resident();
    ui_say_ex(id, EXPR_HAPPY, DL_PILL, "Hello there!{.} This line has a name pill / and two lines.");
    ui_say_ex(id, EXPR_NEUTRAL, DL_SHOUT, "WHAT.{!} A shout!");
    ui_say(id, "A legacy line with the name plate.");
    ui_choice(id, "Pick one:", opts, 3, NULL);
}
static uint32_t p_portrait(int f)
{
    if (f % 90 == 89) return BTN_A;
    if (f > 300 && f % 30 == 15) return BTN_DOWN;
    return 0;
}

static void s_island(void)
{
    test_reset();
    fp_new_island(TOWN_POP_PREMADE, 4242);
    fp_island_select_kind(LOC_TOWN_HALL);      /* the hub keeps its cursor between visits: every run starts alike */
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
}
static uint32_t p_island(int f)
{
    if (f == 100) ui_toast("Island toast");
    return 0;
}

/* (CL4) the island's first-time tip card: a retained panel once it has boinged in (it borrows the
 * toast's id), drawn over the hub's kept frames; A closes it at frame 150, then a toast */
static void s_island_tip(void) { s_island(); tips_test_force(1); }
static uint32_t p_island_tip(int f)
{
    if (f == 150) return BTN_A;
    if (f == 175) ui_toast("After the card");
    return 0;
}

/* CR9: the creator's tab strip and list (creator_draw.c): idle, rows, values, tabs, the grid */
static void s_creator(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 4242, TOWN_POP_PREMADE);
    scene_set(&SCENE_CREATOR, -1);
}
static uint32_t p_creator(int f)
{
    static const uint32_t seq[] = { BTN_DOWN, 0, BTN_RIGHT, 0, BTN_RIGHT, BTN_DOWN, BTN_DOWN, BTN_R, BTN_DOWN,
                                    BTN_RIGHT, BTN_RIGHT, BTN_RIGHT, BTN_LEFT, BTN_R, BTN_R, BTN_R, BTN_DOWN,
                                    BTN_A, BTN_RIGHT, BTN_DOWN, BTN_R, 0, 0, BTN_LEFT, BTN_B, BTN_UP, BTN_L };
    if (f >= 60 && f < 60 + (int)(sizeof(seq) / sizeof(seq[0])) * 12 && f % 12 == 0) return seq[(f - 60) / 12];
    return 0;
}

/* CR9: the same with the GBA's still stage (cr_calm): the preview pane and the button bar are panels
 * too, every creator panel is put back whole (plat_panel_solid), and a new look is drawn in slices
 * (one layer group a tick, a fake portrait clock) while the last one stays up */
static void s_creator_calm(void)
{
    portrait_cache_reset();
    cr_calm = 1;
    s_creator();
}
static uint32_t p_creator_calm(int f)
{
    portrait_cache_clock((uint32_t)f + 1, 1);
    return p_creator(f);
}

/* (SP8) the shooter: walk and shoot in the Vape Cellar, swap weapons, take hits (the low-health
 * heartbeat and the hurt bounce of the face), pick up armour, then third person */
static void s_combat(void)
{
    story_town();
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_COMBAT, MAPID_VAPE_CELLAR);
}
static uint32_t p_combat(int f)
{
    if (f == 120) combat_hurt_player(40, 0, 0, -1);
    if (f == 180) combat_hurt_player(35, 1024, 0, -1);           /* low: the heartbeat */
    if (f == 200) doomo_player.armor = 25;
    if (f == 260 && doomo_player.hp > 0) doomo_player.hp = 90;
    if (f == 300) return BTN_SELECT;                              /* third person */
    if (f >= 40 && f < 100) return BTN_UP | ((f / 6) % 2 ? BTN_A : 0);
    if (f >= 140 && f < 170) return BTN_A;
    if (f == 230) return BTN_Y;                                   /* next weapon */
    if (f >= 320 && f < 380) return BTN_LEFT | ((f / 5) % 2 ? BTN_A : 0);
    return 0;
}

/* (S2-6) the Unlocks gallery's grid (fp_unlocks.c): a panel with the cursor's row as its hole. Some of every kind
 * owned (paints show their swatch, owned ones sparkle under the cursor), the cursor along a row (no new capture),
 * down the rows and past the last one (the page scrolls), a grant on the screen (the owned count is in the key),
 * the Parts and Paints tabs, and back */
static void s_unlocks(void)
{
    int i, n;
    test_reset();
    fp_new_island(TOWN_POP_PREMADE, 4242);
    n = unlock_count();
    for (i = 0; i < n; i += 3) unlock_grant_quiet(unlock_at(i)->id);
    scene_set(&SCENE_UNLOCKS, 0);
}
static uint32_t p_unlocks(int f)
{
    if (f == 230) {
        int i, n = unlock_count();
        for (i = 1; i < n; i += 3)
            if (!unlock_has(unlock_at(i)->id)) { unlock_grant(unlock_at(i)->id); break; }
    }
    if (f >= 40 && f < 130 && f % 9 == 0) return BTN_RIGHT;
    if (f >= 130 && f < 220 && f % 11 == 0) return BTN_DOWN;
    if (f >= 220 && f < 260 && f % 13 == 0) return BTN_LEFT;
    if (f == 270 || f == 300) return BTN_R;                        /* Parts, then Paints */
    if (f >= 310 && f < 350 && f % 7 == 0) return BTN_RIGHT;
    if (f == 360) return BTN_L;
    if (f >= 370 && f < 410 && f % 10 == 0) return BTN_UP;
    return 0;
}

/* (S2-8) the Almanac's detail page (lib_almanac.c): Profile, Likes and the locked pages under the tabs as a
 * panel. Opens a citizen, scrolls the bio (the arrows bob inside the panel), the face toggle (the figure is left of
 * the panel), Likes, Bonds (not a panel), the next citizens (a page turn), and back to the list. */
static void s_almanac(void)
{
    test_reset();
    portrait_cache_reset();             /* the figures drawn from scratch on every run (no stand-in from the last) */
    scene_set(&SCENE_LIB_ALMANAC, LIB_ARG_DEMO);
}
static uint32_t p_almanac(int f)
{
    static const uint16_t at[] = { 20, 60, 80, 120, 150, 190, 230, 270, 300, 340, 380 };
    static const uint32_t b[] = { BTN_DOWN, BTN_A, BTN_DOWN, BTN_SELECT, BTN_R, BTN_R, BTN_RIGHT, BTN_L,
                                  BTN_RIGHT, BTN_B, BTN_DOWN };
    unsigned i;
    for (i = 0; i < sizeof(at) / sizeof(at[0]); i++)
        if (f == at[i]) return b[i];
    return 0;
}

/* (S2-8) the Field Guide (lib_help.c): the contents' preview text and the open page's text as a panel. Down the
 * contents, open a page, scroll it down and up, turn pages, close it */
static void s_help(void)
{
    test_reset();
    scene_set(&SCENE_LIB_HELP, 0);
}
static uint32_t p_help(int f)
{
    if (f >= 20 && f < 80 && f % 15 == 0) return BTN_DOWN;
    if (f == 100) return BTN_A;
    if (f >= 140 && f < 200 && f % 12 == 0) return BTN_DOWN;
    if (f == 220) return BTN_UP;
    if (f == 250 || f == 290) return BTN_RIGHT;
    if (f == 330) return BTN_LEFT;
    if (f == 370) return BTN_B;
    if (f == 390) return BTN_DOWN;
    return 0;
}

/* (S2-9) Rivet's workshop (workshop.c): the part list's rows but the cursor's, and the stat bars (of the Weld tab's
 * pane and the Armory's), are panels. A story town with four weapons in the Armory; the cursor down the whole list
 * (past the scroll and back to the first row), the options of a few parts (the rows under the cursor change, a
 * locked option shows its mark, the stat bars change), up again, the Armory (the list and its pane, the cursor
 * down and up the weapons), and back to the Weld tab (the shine of the bars passes over them in all of it) */
static void s_workshop(void)
{
    int i;
    story_town();
    memset(&weld_pending, 0, sizeof(weld_pending));     /* the design on the table outlives a run */
    for (i = 1; i < 5; i++) {
        DoomoWeapon *w = &doomo_town.weapons[i];
        weld_default(w, (i * 3) % WFRAME_COUNT, i & 1);
        w->flags = WF_VALID;
        w->quality = 60;
    }
    scene_set(&SCENE_WORKSHOP, 0);
}
static uint32_t p_workshop(int f)
{
    if (f == 12) return BTN_A;                                  /* the tip card, if it is up */
    if (f >= 20 && f < 120 && f % 6 == 2) return BTN_DOWN;     /* every row, the scroll, the wrap to the top */
    if (f >= 120 && f < 200 && f % 8 == 4) return (f / 8) % 3 ? BTN_RIGHT : BTN_LEFT;
    if (f >= 200 && f < 260 && f % 6 == 0) return BTN_UP;
    if (f == 270) return BTN_L;                                 /* the Armory */
    if (f >= 280 && f < 340 && f % 10 == 0) return f < 310 ? BTN_DOWN : BTN_UP;
    if (f == 350) return BTN_R;
    if (f >= 360 && f < 410 && f % 5 == 0) return BTN_RIGHT;
    return 0;
}

/* the language switch: Spanish from frame 120 on */
static uint32_t p_visit_lang(int f)
{
    if (f == 120) opt_set(OPT_LANGUAGE, lang_find("es") >= 0 ? lang_find("es") : 0);
    return p_visit(f);
}

static uint32_t run(Setup s, Pad pad, int n, uint32_t *out)
{
    int f;
    nat_panel_reset();
    doomo_init();                     /* the frame counter and the rest */
    s();
    for (f = 0; f < n; f++) {
        frame(pad(f));
        out[f] = fb_hash();
    }
    opt_set(OPT_LANGUAGE, 0);
    return 0;
}

static int compare(const char *name, Setup s, Pad pad, int n, int w, int h, int min_kept)
{
    static uint32_t off[MAXF], on[MAXF];
    int f;
    set_res(w, h);
    nat_panel_cache = 0;
    run(s, pad, n, off);             /* a warm-up: a scene's first run in the process fills statics later runs find */
    run(s, pad, n, off);
    nat_panel_cache = 1;
    run(s, pad, n, on);
    nat_panel_cache = 0;
    for (f = 0; f < n; f++)
        if (off[f] != on[f]) {
            static uint8_t a[NAT_MAX_W * NAT_MAX_H];
            char m[300], path[600];
            int i, x0 = w, y0 = h, x1 = -1, y1 = -1, nd = 0;
            /* where: both runs again up to that frame */
            nat_panel_cache = 0;
            run(s, pad, f + 1, on);
            memcpy(a, nat_fb, (size_t)(w * h));
            snprintf(path, sizeof(path), "%s/panel_%s_off.png", test_tmp_dir, name);
            nat_png_write(path, nat_fb, w, h, 2);
            nat_panel_cache = 1;
            run(s, pad, f + 1, on);
            snprintf(path, sizeof(path), "%s/panel_%s_on.png", test_tmp_dir, name);
            nat_png_write(path, nat_fb, w, h, 2);
            nat_panel_cache = 0;
            for (i = 0; i < w * h; i++)
                if (a[i] != nat_fb[i]) {
                    nd++;
                    x0 = DOOMO_MIN(x0, i % w); x1 = DOOMO_MAX(x1, i % w);
                    y0 = DOOMO_MIN(y0, i / w); y1 = DOOMO_MAX(y1, i / w);
                }
            snprintf(m, sizeof(m), "%s %dx%d: frame %d differs with the panel cache on (%d px in %d,%d..%d,%d; "
                     "kept %u, drawn %u)", name, w, h, f, nd, x0, y0, x1, y1,
                     nat_panel_stats.kept, nat_panel_stats.drawn);
            test_fail(__FILE__, __LINE__, m);
            return 0;
        }
    if (nat_panel_stats.rule) {
        char m[200];
        snprintf(m, sizeof(m), "%s %dx%d: panel %d broke a rule %u times (%s)", name, w, h,
                 nat_panel_stats.rule_panel, nat_panel_stats.rule,
                 nat_panel_stats.first_rule ? nat_panel_stats.first_rule : "?");
        test_fail(__FILE__, __LINE__, m);
        return 0;
    }
    if ((int)nat_panel_stats.kept < min_kept) {
        char m[200];
        snprintf(m, sizeof(m), "%s %dx%d: only %u of %u panels kept", name, w, h,
                 nat_panel_stats.kept, nat_panel_stats.begins);
        test_fail(__FILE__, __LINE__, m);
        return 0;
    }
    printf("      panel %-10s %dx%d: %u begins, %u kept, %u drawn, %u spoiled\n", name, w, h,
           nat_panel_stats.begins, nat_panel_stats.kept, nat_panel_stats.drawn, nat_panel_stats.spoiled);
    return 1;
}

static int both(const char *name, Setup s, Pad pad, int n, int min_kept)
{
    int ok = compare(name, s, pad, n, 240, 160, min_kept) && compare(name, s, pad, n, 320, 240, min_kept);
    set_res(240, 160);
    return ok;
}

TEST(panel_overworld_hud_is_pixel_identical)
{
    CHECK(both("visit", s_visit, p_visit, 320, 500));
}

TEST(panel_invaded_toast_and_star_bob_are_pixel_identical)
{
    CHECK(both("invaded", s_invaded, p_invaded, 400, 200));
}

TEST(panel_dialogue_narration_is_pixel_identical)
{
    CHECK(both("narration", s_narration, p_narration, 400, 300));
}

TEST(panel_dialogue_portrait_and_choice_are_pixel_identical)
{
    CHECK(both("portrait", s_portrait, p_portrait, 420, 150));
}

TEST(panel_island_hud_and_ticker_are_pixel_identical)
{
    CHECK(both("island", s_island, p_island, 240, 150));
}

TEST(panel_tip_card_is_pixel_identical)
{
    CHECK(both("tip", s_island_tip, p_island_tip, 240, 120));
    tips_session_reset();
}

TEST(panel_creator_tabs_and_list_are_pixel_identical)
{
    CHECK(both("creator", s_creator, p_creator, 400, 200));
}

TEST(panel_creator_calm_pane_and_bar_are_pixel_identical)
{
    int ok = both("creator calm", s_creator_calm, p_creator_calm, 400, 400);
    uint32_t empty = nat_panel_stats.empty, drawn = nat_panel_stats.drawn;
    cr_calm = 0;
    portrait_cache_reset();
    CHECK(ok);
    /* S3-3 (the recipe of S2-6): a region whose key is new (the tab strip, the list or grid, the pane) is drawn
     * before its panel and the panel then takes the rect as it is: its capture sees no 2D call of its own. Without
     * it every capture frame draws inside the panel (the GBA: every call routed, 10-30 ms more) */
    CHECK(empty > 0);
    CHECK(empty * 2 >= drawn);
}

/* the species grid (A on the first row), the cursor moved along it every few frames */
static uint32_t p_creator_grid(int f)
{
    if (f == 60) return BTN_A;
    if (f >= 100 && f % 6 == 0) return BTN_RIGHT;
    return 0;
}

TEST(panel_creator_grid_page_is_pixel_identical_and_drawn_before_its_panel)
{
    /* the part grid's window is the list region's panel: each move of the cursor makes a new key */
    int ok = both("creator grid", s_creator_calm, p_creator_grid, 260, 40);
    uint32_t empty = nat_panel_stats.empty, drawn = nat_panel_stats.drawn;
    cr_calm = 0;
    portrait_cache_reset();
    CHECK(ok);
    CHECK(drawn > 0);
    CHECK(empty * 2 >= drawn);
}

TEST(panel_combat_hud_is_pixel_identical)
{
    CHECK(both("combat", s_combat, p_combat, 400, 300));
}

TEST(panel_unlocks_grid_is_pixel_identical)
{
    CHECK(both("unlocks", s_unlocks, p_unlocks, 420, 150));
}

TEST(panel_almanac_page_is_pixel_identical)
{
    CHECK(both("almanac", s_almanac, p_almanac, 420, 150));
}

TEST(panel_field_guide_text_is_pixel_identical)
{
    CHECK(both("help", s_help, p_help, 420, 200));
}

TEST(panel_workshop_list_and_bars_are_pixel_identical)
{
    CHECK(both("workshop", s_workshop, p_workshop, 420, 100));
}

/* ---- (S2-9) the ui helpers the workshop's panels stand on ------------------------------------------- */
static uint32_t rect_hash(int x, int y, int w, int h)
{
    uint32_t hh = 2166136261u;
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++) hh = (hh ^ nat_fb[j * nat_w + i]) * 16777619u;
    return hh;
}

/* ui_window_open(): the window without a rect of its plain fill; with the rect filled in again it is the window */
TEST(panel_window_open_is_the_window_less_its_rect)
{
    int st, k, i;
    static uint8_t full[NAT_MAX_W * NAT_MAX_H];
    set_res(240, 160);
    nat_panel_cache = 0;
    test_reset();
    for (st = 0; st < 6; st++)
        for (k = 0; k < 3; k++) {
            int x = 10 + k * 3, y = 20 + k, w = 100 + k * 7, h = 60 + k * 9;
            int x0 = x + 8, x1 = x + w - 8, y0 = y + 14, y1 = y + h - 12;
            CHECK(ui_window_open_ok(x, y, w, h, x0, y0, x1, y1));
            plat_clear(PAL_BLACK);
            ui_window(x, y, w, h, st);
            memcpy(full, nat_fb, (size_t)(nat_w * nat_h));
            plat_clear(PAL_BLACK);
            ui_window_open(x, y, w, h, st, x0, y0, x1, y1);
            for (i = 0; i < nat_w * nat_h; i++) {                   /* outside the rect: the window's pixels */
                int px = i % nat_w, py = i / nat_w;
                if (px >= x0 && px < x1 && py >= y0 && py < y1) {
                    CHECK_EQ(nat_fb[i], PAL_BLACK);                 /* ... inside it: nothing drawn */
                } else if (nat_fb[i] != full[i]) {
                    CHECK_EQ(nat_fb[i], full[i]);
                    return;
                }
            }
            plat_rect(x0, y0, x1 - x0, y1 - y0, ui_style(st)->fill);
            CHECK(memcmp(nat_fb, full, (size_t)(nat_w * nat_h)) == 0);
            /* rects the window's other layers reach are not taken (the whole window is drawn) */
            CHECK(!ui_window_open_ok(x, y, w, h, x0, y, x1, y1 - 12));              /* the gloss band */
            CHECK(!ui_window_open_ok(x, y, w, h, x, y0, x1, y1));                   /* the outline column */
            CHECK(!ui_window_open_ok(x, y, w, h, x0, y0, x1, y + h - 1));           /* the rounded bottom */
            plat_clear(PAL_BLACK);
            ui_window_open(x, y, w, h, st, x0, y, x1, y1 - 12);
            memcpy(&full[0], nat_fb, (size_t)(nat_w * nat_h));
            plat_clear(PAL_BLACK);
            ui_window(x, y, w, h, st);
            CHECK(memcmp(nat_fb, full, (size_t)(nat_w * nat_h)) == 0);
        }
}

/* ui_bar_still(): ui_bar() without its shine; the shine is the only difference, in every frame of its sweep */
TEST(panel_bar_still_differs_from_the_bar_by_its_shine_only)
{
    static uint8_t a[NAT_MAX_W * NAT_MAX_H];
    int f, i, v, shone = 0, same = 0;
    set_res(240, 160);
    nat_panel_cache = 0;
    test_reset();
    doomo_init();
    for (f = 0; f < 90; f++) {
        frame(0);
        for (v = 8; v <= 100; v += 46) {                    /* a bar under a quarter blinks; both blink alike */
            int diff = 0;
            plat_clear(PAL_BLACK);
            ui_bar(20, 30, 60, 6, v, 100, PAL_PAINT(PAINT_CHERRY, 1));
            memcpy(a, nat_fb, (size_t)(nat_w * nat_h));
            plat_clear(PAL_BLACK);
            ui_bar_still(20, 30, 60, 6, v, 100, PAL_PAINT(PAINT_CHERRY, 1));
            for (i = 0; i < nat_w * nat_h; i++)
                if (a[i] != nat_fb[i]) {
                    diff++;
                    CHECK_EQ(a[i], PAL_WHITE);              /* what the bar has over the still one: shine pixels */
                }
            if (diff) shone++; else same++;
        }
    }
    CHECK(shone > 20 && same > 20);                         /* the shine was on and off in the run */
}

/* ui_backdrop_flat(): the bands and seams of ui_backdrop(), none of its dots, hearts and twinkles, and nothing in a hole */
TEST(panel_backdrop_flat_is_the_backdrop_without_its_drifting_things)
{
    static uint8_t full[NAT_MAX_W * NAT_MAX_H];
    int holes[8] = { 20, 30, 100, 120, 130, 40, 220, 100 };
    int i, nb = 0, bands[8], t;
    set_res(240, 160);
    nat_panel_cache = 0;
    test_reset();
    for (t = 0; t < 3; t++) {
        plat_clear(PAL_BLACK);
        ui_backdrop_flat(WIN_CREAM, holes, 2);
        nb = 0;
        for (i = 0; i < nat_w * nat_h; i++) {
            int px = i % nat_w, py = i / nat_w, j, c = nat_fb[i], hole = 0;
            for (j = 0; j < 2; j++)
                if (px >= holes[4 * j] && px < holes[4 * j + 2] && py >= holes[4 * j + 1] && py < holes[4 * j + 3]) hole = 1;
            if (hole) {
                /* a hole is left alone (but for the four seam rows, which run the screen's width as ui_backdrop's) */
                if (py != nat_h / 3 - 1 && py != nat_h / 3 && py != nat_h * 2 / 3 - 1 && py != nat_h * 2 / 3)
                    CHECK_EQ(c, PAL_BLACK);
                continue;
            }
            for (j = 0; j < nb && bands[j] != c; j++) {}
            if (j == nb) { if (nb < 8) bands[nb++] = c; }
        }
        CHECK(nb >= 3 && nb <= 3);                          /* top, middle and bottom band (a seam row mixes two) */
        memcpy(full, nat_fb, (size_t)(nat_w * nat_h));
        plat_clear(PAL_BLACK);
        ui_backdrop_holes(WIN_CREAM, (uint32_t)(t * 37), holes, 2);
        for (i = 0; i < nat_w * nat_h; i++) {
            int j, band = 0;
            for (j = 0; j < nb; j++) band |= nat_fb[i] == bands[j];
            if (band && nat_fb[i] != full[i]) {             /* wherever the full backdrop shows a band, so does the flat one */
                CHECK_EQ(nat_fb[i], full[i]);
                return;
            }
        }
    }
}

/* ui_style_use(): the style ui_value_row() reads, as ui_window() leaves it */
TEST(panel_style_use_sets_the_style_widgets_read)
{
    uint32_t a, b;
    set_res(240, 160);
    nat_panel_cache = 0;
    test_reset();
    plat_clear(PAL_BLACK);
    ui_window(150, 20, 60, 40, WIN_CUTE);                   /* the last window drawn was another style ... */
    ui_style_use(WIN_CREAM);
    ui_value_row(20, 30, 100, "Barrel", "Standard", 0);
    ui_value_row(20, 50, 100, "Core", "Bullet", 1);
    a = rect_hash(10, 20, 120, 50);
    plat_clear(PAL_BLACK);
    ui_window(150, 20, 60, 40, WIN_CREAM);                  /* ... or this one, as the list's window is */
    ui_value_row(20, 30, 100, "Barrel", "Standard", 0);
    ui_value_row(20, 50, 100, "Core", "Bullet", 1);
    b = rect_hash(10, 20, 120, 50);
    CHECK_EQ(a, b);
    plat_clear(PAL_BLACK);
    ui_window(150, 20, 60, 40, WIN_CUTE);                   /* (and without it the rows do read the last window's) */
    ui_value_row(20, 30, 100, "Barrel", "Standard", 0);
    ui_value_row(20, 50, 100, "Core", "Bullet", 1);
    CHECK(rect_hash(10, 20, 120, 50) != a);
}

TEST(panel_language_switch_redraws)
{
    CHECK(both("lang", s_visit, p_visit_lang, 240, 200));
}

/* The weak default keeps nothing: every panel is drawn every frame. */
TEST(panel_cache_off_draws_everything)
{
    nat_panel_cache = 0;
    nat_panel_reset();
    CHECK_EQ(plat_panel_begin(PANEL_DIALOG, 0, 0, 10, 10, 1), 1);
    CHECK_EQ(plat_panel_begin(PANEL_DIALOG, 0, 0, 10, 10, 1), 1);
    CHECK_EQ(nat_panel_stats.begins, 0);
}

#ifdef DOOMO_HAS_TALK
/* Compare a single live session at a frozen logic tick: starting two
 * sessions would intentionally generate different resident lines/seeds. */
static int talk_panel_picture(void)
{
    uint32_t want, kept;
    int i;
    nat_panel_cache = 0;
    plat_begin_frame(); scene_draw(); plat_end_frame();
    want = fb_hash();
    nat_panel_reset();
    nat_panel_cache = 1;
    for (i = 0; i < 3; i++) {
        plat_begin_frame(); scene_draw(); plat_end_frame();
        if (fb_hash() != want) { nat_panel_cache = 0; return 0; }
    }
    kept = nat_panel_stats.kept;
    CHECK_EQ_U(nat_panel_stats.rule, 0);
    nat_panel_cache = 0;
    return kept >= 4;
}

TEST(panel_talk_keeps_unchanged_strip_and_replies)
{
    int i, size;
    for (size = 0; size < 2; size++) {
        set_res(size ? 320 : 240, size ? 240 : 160);
        test_reset();
        town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
        scene_set(&SCENE_TITLE, 0);
        CHECK_EQ(talk_db_open(lang_find("en")), TALK_DB_OK);
        CHECK(home_talk_enter(1));
        for (i = 0; i < 60; i++) {
            int busy = 1;
            if (scene_top() == &SCENE_TALK &&
                talk_scene_view(NULL, NULL, NULL, NULL, NULL, &busy, NULL) && !busy) break;
            frame(BTN_A); frame(0); frame(0); frame(0);
        }
        CHECK(scene_top() == &SCENE_TALK);
        if (scene_top() == &SCENE_TALK) {
            CHECK(talk_panel_picture());
            frame(BTN_DOWN); frame(0);
            CHECK(talk_panel_picture());
        }
        scene_set(&SCENE_TITLE, 0);
        talk_db_close();
    }
    set_res(240, 160);
}
#endif
