/* test_textfit.c - (CL5, clarity C6 / C9 / C13) every label fits its box at 240x160 (and at 320x240):
 * the island's place menus (labels, help lines, lock reasons, host greetings), the registered activities,
 * the citizen care screen (actions, need meters, the help strip, the name band), the Options help lines and
 * the first-boot / security code prompts. Plus the restructured Town Hall (at most FP_TOWN_HALL_ROWS rows, the
 * moved rows in the START menu) and the one stat vocabulary (Energy, never Nrg).
 *
 * The checks measure with the same geometry the drawing uses (fp_island_menu_overflow and friends live next
 * to their draw code), so a longer string, a narrower box or a new row that does not fit fails here instead
 * of being cut on a GBA screen. */
#include <stdlib.h>
#include "test.h"
#include "../src/fp_int.h"
#include "../src/fp_island.h"
#include "doomo/activities.h"

int fp_island_select_kind(int kind);
int fp_island_menu_open(void);
int fp_island_menu_count(void);
const char *fp_island_menu_label(int i);
int fp_island_menu_select(const char *label);
int fp_island_menu_overflow(char *why, int cap);
int fp_island_menu_enabled(int i);
const char *fp_island_menu_help(int i);
const char *fp_island_menu_key(int i);
int fp_resident_text_overflow(char *why, int cap);
int fp_options_help_overflow(char *why, int cap);
int lang_pick_text_overflow(char *why, int cap);
int seccode_text_overflow(char *why, int cap);

static void set_res(int w, int h)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = w; o.h = h; o.seed = 0x5EED1234u; o.quiet = 1;
    o.save_dir = test_tmp_dir;
    nat_init(&o);
}

static void frames(uint32_t b, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        plat_begin_frame();
        doomo_frame(b);
        plat_end_frame();
    }
}

static void press(uint32_t b) { frames(b, 1); frames(0, 1); }

static void island(int pop)
{
    test_reset();
    fp_new_island(pop, 4242);
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
    frames(0, 3);
    while (scene_top() == &SCENE_DIALOG) press(BTN_A);
}

static int menu_has(const char *label)
{
    int i;
    for (i = 0; i < fp_island_menu_count(); i++) if (strcmp(fp_island_menu_label(i), label) == 0) return i + 1;
    return 0;
}

/* every place's menu and the START menu: nothing overflows */
static void all_menus_fit(int w, int h, int pop)
{
    char why[900];
    int k;
    set_res(w, h);
    island(pop);
    for (k = 0; k <= LOC_COUNT; k++) {
        if (k < LOC_COUNT) {
            fp_island_select_kind(k);
            press(BTN_A);
        } else {
            press(BTN_START);
        }
        CHECK_MSG(fp_island_menu_open(), "%dx%d: place %d opened no menu", w, h, k);
        CHECK_MSG(fp_island_menu_overflow(why, sizeof(why)) == 0, "%dx%d pop %d: %s", w, h, pop, why);
        press(BTN_B);
    }
}

TEST(textfit_island_menus_fit_at_240_and_320)
{
    all_menus_fit(240, 160, TOWN_POP_PREMADE);
    all_menus_fit(240, 160, TOWN_POP_EMPTY);
    all_menus_fit(320, 240, TOWN_POP_PREMADE);
    all_menus_fit(320, 240, TOWN_POP_EMPTY);
    set_res(240, 160);
}

/* C9: Town Hall keeps the civic rows only (Leave included, at most FP_TOWN_HALL_ROWS); Save / Load, Options and
 * Name the island are in the START menu, which shows the map's shortcut keys and has one way to the title */
TEST(textfit_town_hall_is_short_and_the_start_menu_has_the_rest)
{
    static const char *const moved[] = { "Save / Load", "Options", "Name the island", "Music Studio",
                                         "Expand the House" };
    int i, n;
    set_res(240, 160);
    island(TOWN_POP_PREMADE);
    fp_island_select_kind(LOC_TOWN_HALL);
    press(BTN_A);
    CHECK(fp_island_menu_open());
    n = fp_island_menu_count();
    CHECK_MSG(n <= FP_TOWN_HALL_ROWS, "Town Hall has %d rows", n);
    for (i = 0; i < (int)DOOMO_COUNTOF(moved); i++) CHECK_MSG(!menu_has(moved[i]), "Town Hall still has %s", moved[i]);
    CHECK(menu_has("Create (Hillvile)") == 1);              /* the first row, as every script expects */
    CHECK(menu_has("Collect rent"));
    press(BTN_B);
    press(BTN_START);
    CHECK(fp_island_menu_open());
    CHECK(menu_has("Save / Load") == 4 && menu_has("Options") == 5 && menu_has("Family") == 6);   /* rows unmoved */
    CHECK(menu_has("Name the island"));
    CHECK(menu_has("Quit to title"));
    CHECK(!menu_has("Back to title") && !menu_has("Leave"));
    CHECK_STR(fp_island_menu_key(menu_has("Citizens") - 1), "R");
    CHECK_STR(fp_island_menu_key(menu_has("News") - 1), "L");
    CHECK_STR(fp_island_menu_key(menu_has("Family") - 1), "SEL");
    press(BTN_B);
    /* the moved rows still lead somewhere: the studio from the Concert Hall, Expand from the homes */
    fp_island_select_kind(LOC_CONCERT);
    press(BTN_A);
    CHECK(menu_has("Music Studio"));
    press(BTN_B);
    fp_island_select_kind(LOC_TOWERS);
    press(BTN_A);
    CHECK(menu_has("Expand the House"));
    press(BTN_B);
}

/* C9 1.7b: a locked row says why in its help line, before it is pressed */
TEST(textfit_locked_rows_give_their_reason)
{
    int i, n, k;
    set_res(240, 160);
    island(TOWN_POP_PREMADE);                                 /* Classic: both sides full */
    fp_island_select_kind(LOC_TOWN_HALL);
    press(BTN_A);
    k = menu_has("Create (Hillvile)") - 1;
    CHECK(k >= 0 && !fp_island_menu_enabled(k));
    CHECK_MSG(strstr(fp_island_menu_help(k), "Full") != NULL, "help: %s", fp_island_menu_help(k));
    press(BTN_B);
    /* every locked row of every place has a help line */
    for (i = 0; i < LOC_COUNT; i++) {
        fp_island_select_kind(i);
        press(BTN_A);
        n = fp_island_menu_count();
        for (k = 0; k < n; k++)
            CHECK_MSG(fp_island_menu_enabled(k) || fp_island_menu_help(k)[0],
                      "place %d: \"%s\" is locked without a reason", i, fp_island_menu_label(k));
        press(BTN_B);
    }
    island(TOWN_POP_EMPTY);                                   /* an empty island: room on both sides */
    fp_island_select_kind(LOC_TOWN_HALL);
    press(BTN_A);
    CHECK(fp_island_menu_enabled(menu_has("Create (Hillvile)") - 1));
    press(BTN_B);
}

/* every registered activity's name fits a menu row next to a host, and its description the two-line help pill */
TEST(textfit_activity_names_fit_a_menu_row)
{
    char why[900];
    int i, n = activity_count(), w = 240 - 12 - FIGURE_W - 18;
    set_res(240, 160);
    for (i = 0; i < n; i++) {
        const DoomoActivity *a = activity_at(i);
        CHECK_MSG(ui_text_width(a->name) <= w - 26, "activity \"%s\" is %d px, a row has %d", a->name,
                  ui_text_width(a->name), w - 26);
        CHECK_MSG(ui_text_wrap(0, 0, w - 8, a->desc, 0, 0) <= 2, "activity \"%s\": description \"%s\"", a->name, a->desc);
    }
    (void)why;
}

static int first_alive(void)
{
    int i;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (doomo_town.res[i].status == RES_ALIVE) return i;
    return -1;
}

static void care_fits(int w, int h)
{
    char why[900];
    int id;
    set_res(w, h);
    island(TOWN_POP_PREMADE);
    id = first_alive();
    CHECK(id >= 0);
    scene_push(&SCENE_RESIDENT, id);
    frames(0, 4);
    CHECK(scene_top() == &SCENE_RESIDENT);
    CHECK_MSG(fp_resident_text_overflow(why, sizeof(why)) == 0, "%dx%d: %s", w, h, why);
}

/* C6 / C13: the care screen at both sizes: actions, need meters (name + value at 240), help strip, name band */
TEST(textfit_care_screen_fits_at_240_and_320)
{
    care_fits(240, 160);
    care_fits(320, 240);
    set_res(240, 160);
}

/* C6: one need vocabulary, the care screen's, in the shop and the item dex too (no "Nrg", "HP", "Sanity",
 * "Corrupt" tables of their own): the sources use doomo_stat_names */
static char *slurp(const char *rel)
{
    char path[512];
    const char *g = test_golden_dir;              /* <core>/tests/golden */
    FILE *f;
    long n;
    char *buf;
    snprintf(path, sizeof(path), "%s/../../%s", g, rel);
    f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char *)malloc((size_t)n + 1);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    if (buf) buf[n] = 0;
    fclose(f);
    return buf;
}

TEST(textfit_one_need_vocabulary_everywhere)
{
    static const char *const files[] = { "src/fp_shop.c", "src/lib_items.c", "src/fp_resident.c", "src/fp_common.c" };
    static const char *const banned[] = { "\"Nrg\"", "\"Sanity\"", "\"Corrupt\"", "\"HP\", \"Mind\"" };
    int i, k;
    CHECK_STR(doomo_stat_names[STAT_ENERGY], "Energy");
    CHECK_STR(doomo_stat_names[STAT_SANITY], "Mind");
    CHECK_STR(doomo_stat_names[STAT_CORRUPTION], "Dark");
    for (i = 0; i < (int)DOOMO_COUNTOF(files); i++) {
        char *s = slurp(files[i]);
        CHECK_MSG(s != NULL, "cannot read %s (golden dir %s)", files[i], test_golden_dir);
        for (k = 0; k < (int)DOOMO_COUNTOF(banned); k++)
            if (strstr(s, banned[k])) { free(s); CHECK_MSG(0, "%s still spells a need %s", files[i], banned[k]); }
        if (i < 2 && !strstr(s, "doomo_stat_names")) { free(s); CHECK_MSG(0, "%s has its own need names", files[i]); }
        free(s);
    }
}

/* C13 1.14a: every option's help (and every preset's) fits the help pill, three lines at most, at both sizes */
TEST(textfit_options_help_fits)
{
    char why[900];
    set_res(240, 160);
    test_reset();
    CHECK_MSG(fp_options_help_overflow(why, sizeof(why)) == 0, "240x160: %s", why);
    set_res(320, 240);
    CHECK_MSG(fp_options_help_overflow(why, sizeof(why)) == 0, "320x240: %s", why);
    set_res(240, 160);
}

/* C13 1.1b / 1.1c: the Mature question's lines and the security code's prompts fit their windows at 240x160 */
TEST(textfit_first_boot_and_code_prompts_fit)
{
    char why[900];
    set_res(240, 160);
    CHECK_MSG(lang_pick_text_overflow(why, sizeof(why)) == 0, "Mature: %s", why);
    CHECK_MSG(seccode_text_overflow(why, sizeof(why)) == 0, "code: %s", why);
}
