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
    cr_calm = 0;
    portrait_cache_reset();
    CHECK(ok);
}

TEST(panel_combat_hud_is_pixel_identical)
{
    CHECK(both("combat", s_combat, p_combat, 400, 300));
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
