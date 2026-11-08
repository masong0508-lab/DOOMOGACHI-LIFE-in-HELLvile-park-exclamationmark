/* test_prompts.c - C3: the shared prompt bar (ui.h ui_prompts) and the
 * pad-aware button names (input.h btn_name).
 *   - every pad's names: no X / Y / L2 / R2 on a GBA (SEL+A, SEL+B), L1 /
 *     R1 / Square / Triangle on a PS1, START+Y for SELECT on a Dreamcast;
 *   - the fitter: two spaces between items, then one, then the
 *     highest-prio item goes (the later one on a tie), never a prio-0 one;
 *     an item whose button the pad lacks is left out;
 *   - the converted screens (the pet, the workshop, the studio, the house,
 *     the island hub) at 240x160 and 320x240 with a GBA pad: every bar fits
 *     and names no button the GBA lacks (ui_prompt_stats);
 *   - the bar is a retained panel (PANEL_PROMPT): with the native panel
 *     cache on, every frame of the pet screen and the island hub is
 *     identical to the cache off, and the bar is kept on most frames;
 *   - the last resort (ui_prompt_cut): whole words, never wider than asked;
 *   - C15: the house's caption for its four score dots, once per save.
 */
#include "test.h"
#include "../src/fp_island.h"
#include "../src/weld_int.h"
#include "../src/home_int.h"

/* a bar's text against the expected one (short names in the failure message) */
#define CHECK_BAR(got, lit) do { if (strcmp((got), (lit)) != 0) { \
        printf("      got  \"%s\"\n      want \"%s\"\n", (got), (lit)); \
        test_fail(__FILE__, __LINE__, "the bar's text differs"); return; } } while (0)

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

static void set_res(int w, int h)
{
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = w; o.h = h; o.seed = 0x5EED1234u; o.quiet = 1;
    o.save_dir = test_tmp_dir;
    nat_init(&o);
}

/* ---- names ----------------------------------------------------------------- */
TEST(prompts_button_names_follow_the_pad)
{
    input_set_pad(PAD_GBA);
    CHECK_STR(btn_name(BTN_X), "SEL+A");
    CHECK_STR(btn_name(BTN_Y), "SEL+B");
    CHECK_STR(btn_name(BTN_L), "L");
    CHECK_STR(btn_name(BTN_SELECT), "SEL");
    CHECK(btn_name(BTN_L2) == NULL);
    CHECK(btn_name(BTN_R2) == NULL);
    input_set_pad(PAD_PS1);
    CHECK_STR(btn_name(BTN_L), "L1");
    CHECK_STR(btn_name(BTN_R), "R1");
    CHECK_STR(btn_name(BTN_X), "Square");
    CHECK_STR(btn_name(BTN_Y), "Triangle");
    CHECK_STR(btn_name(BTN_R2), "R2");
    input_set_pad(PAD_DC);
    CHECK_STR(btn_name(BTN_SELECT), "START+Y");
    CHECK_STR(btn_name(BTN_X), "X");
    CHECK(btn_name(BTN_L2) == NULL);
    input_set_pad(PAD_HOST);
    CHECK_STR(btn_name(BTN_X), "X");
    input_set_pad(-1);
    /* the native runner's --gba-input is the GBA pad */
    input_reset();
    input_update(DOOMO_INPUT_SYNTH_XY);
    CHECK_EQ(input_pad(), PAD_GBA);
    input_update(0);
    CHECK_EQ(input_pad(), PAD_HOST);
}

/* ---- the fitter --------------------------------------------------------------- */
TEST(prompts_text_fits_by_spacing_then_priority)
{
    static const UiPrompt p[] = {
        { BTN_A, 0, "Open" }, { BTN_B, 0, "Back" }, { BTN_SELECT, 2, "Sort" }, { BTN_START, 1, "Find" },
        { BTN_X, 2, "Random" }
    };
    char buf[160];
    int w;
    test_reset();
    input_set_pad(PAD_GBA);
    w = ui_prompts_text(buf, (int)sizeof(buf), p, 5, 1000);
    CHECK_BAR(buf, STR_ICON_A " Open  " STR_ICON_B " Back  SEL Sort  START Find  SEL+" STR_ICON_A " Random");
    CHECK_EQ(w, ui_text_width(buf));
    /* one space between items fits */
    w = ui_prompts_text(buf, (int)sizeof(buf), p, 5, w - 4);
    CHECK_BAR(buf, STR_ICON_A " Open " STR_ICON_B " Back SEL Sort START Find SEL+" STR_ICON_A " Random");
    /* then the highest prio goes first, the later one on a tie (Random before Sort) */
    w = ui_prompts_text(buf, (int)sizeof(buf), p, 5, w - 1);
    CHECK_BAR(buf, STR_ICON_A " Open  " STR_ICON_B " Back  SEL Sort  START Find");
    w = ui_prompts_text(buf, (int)sizeof(buf), p, 5, ui_text_width(STR_ICON_A " Open  " STR_ICON_B " Back"));
    CHECK_BAR(buf, STR_ICON_A " Open  " STR_ICON_B " Back");
    /* prio 0 never goes: a too small width returns the prio-0 items, wider than asked */
    w = ui_prompts_text(buf, (int)sizeof(buf), p, 5, 20);
    CHECK_BAR(buf, STR_ICON_A " Open " STR_ICON_B " Back");
    CHECK(w > 20);
    /* a button the pad lacks is left out; L / R and the arrows */
    {
        static const UiPrompt q[] = {
            { BTN_L2, 0, "Look" }, { BTN_L | BTN_R, 0, "Tab" }, { BTN_LEFT | BTN_RIGHT, 0, "Pick" },
            { BTN_SELECT | BTN_A, 0, "Undo" }, { 0, 0, "Status" }
        };
        ui_prompts_text(buf, (int)sizeof(buf), q, 5, 1000);
        CHECK_BAR(buf, "L/R Tab  " STR_ICON_ARROW_L STR_ICON_ARROW_R " Pick  SEL+" STR_ICON_A " Undo  Status");
        input_set_pad(PAD_PS1);
        ui_prompts_text(buf, (int)sizeof(buf), q, 5, 1000);
        CHECK_BAR(buf, "L2 Look  L1/R1 Tab  " STR_ICON_ARROW_L STR_ICON_ARROW_R " Pick  SEL+" STR_ICON_A " Undo  Status");
    }
    input_set_pad(-1);
}

/* ---- the last resort: cut to whole words, never wider than asked ------------------ */
TEST(prompts_cut_keeps_whole_words_and_fits)
{
    static const char *const s[] = {
        "Hold " STR_ICON_A " to reel - let go when it pulls!",
        STR_ICON_A " Abrir  " STR_ICON_B " Volver  SEL Ordenar  START buscar",
        "one two three four five six seven eight nine ten eleven twelve thirteen",
        "Supercalifragilisticexpialidocious-and-then-some-more-glyphs"
    };
    char buf[160];
    int i, maxw;
    test_reset();
    for (i = 0; i < (int)DOOMO_COUNTOF(s); i++)
        for (maxw = 8; maxw <= ui_text_width(s[i]) + 1; maxw++) {
            int w, n;
            snprintf(buf, sizeof(buf), "%s", s[i]);
            ui_prompt_cut(buf, maxw);
            w = ui_text_width(buf);
            n = (int)strlen(buf);
            if (w > maxw) {
                char m[260];
                snprintf(m, sizeof(m), "\"%s\" at maxw %d is %d px", buf, maxw, w);
                test_fail(__FILE__, __LINE__, m);
                return;
            }
            CHECK(strncmp(buf, s[i], (size_t)n) == 0);             /* a prefix of the text */
            if (ui_text_width(s[i]) <= maxw) CHECK_STR(buf, s[i]); /* a text that fits is not touched */
            else if (strchr(buf, ' ')) {                           /* cut after a whole word */
                CHECK(n > 0 && buf[n - 1] != ' ');
                CHECK(s[i][n] == ' ');
            }
        }
    /* one glyph too wide (the whole string was never measured before): the last word goes */
    snprintf(buf, sizeof(buf), "%s", s[0]);
    ui_prompt_cut(buf, ui_text_width(s[0]) - 1);
    CHECK_STR(buf, "Hold " STR_ICON_A " to reel - let go when it");
    /* a cut in the middle of the text: the longest whole-word prefix that fits */
    snprintf(buf, sizeof(buf), "%s", s[2]);
    ui_prompt_cut(buf, ui_text_width("one two three four") + 5);
    CHECK_STR(buf, "one two three four");
}

/* ---- the converted screens, GBA pad, both sizes ---------------------------------- */
typedef void (*Setup)(void);
static void s_pet(void) { scene_set(&SCENE_PET, 0); }
static void s_work(void)
{
    memset(&weld_pending, 0, sizeof(weld_pending));     /* the design on the table outlives a run */
    scene_set(&SCENE_WORKSHOP, 0);
}
static void s_studio(void) { scene_set(&SCENE_STUDIO, -107); }
static void s_home(void) { scene_set(&SCENE_HOME, -1); }
static void s_island(void)
{
    fp_new_island(TOWN_POP_PREMADE, 4242);
    fp_island_select_kind(LOC_TOWN_HALL);      /* the hub keeps its cursor between visits: every run starts alike */
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
}

/* a few presses that move through each screen's states */
static uint32_t pad_of(int f)
{
    static const uint32_t seq[] = { BTN_RIGHT, BTN_DOWN, BTN_R, BTN_DOWN, BTN_LEFT, BTN_R, BTN_DOWN, BTN_RIGHT };
    if (f % 25 != 24) return 0;
    return seq[(f / 25) % 8];
}

static int sweep(const char *name, Setup s, int w, int h)
{
    UiPromptStats st;
    int f;
    set_res(w, h);
    test_reset();
    input_set_pad(PAD_GBA);
    ui_prompt_stats(NULL, 1);
    s();
    for (f = 0; f < 240; f++) frame(pad_of(f));
    ui_prompt_stats(&st, 1);
    input_set_pad(-1);
    if (st.bars == 0 || st.too_wide || st.wrong_pad) {
        char m[300];
        snprintf(m, sizeof(m), "%s %dx%d: %u bars, %u too wide, %u with a button a GBA lacks (first: \"%s\")",
                 name, w, h, (unsigned)st.bars, (unsigned)st.too_wide, (unsigned)st.wrong_pad, st.first_bad);
        test_fail(__FILE__, __LINE__, m);
        return 0;
    }
    return 1;
}

TEST(prompts_fit_and_name_gba_buttons_on_the_converted_screens)
{
    static const struct { const char *name; Setup s; } sc[] = {
        { "pet", s_pet }, { "workshop", s_work }, { "studio", s_studio }, { "home", s_home }, { "island", s_island }
    };
    int i;
    for (i = 0; i < (int)DOOMO_COUNTOF(sc); i++) {
        if (!sweep(sc[i].name, sc[i].s, 240, 160)) return;
        if (!sweep(sc[i].name, sc[i].s, 320, 240)) return;
    }
    set_res(240, 160);
}

/* ---- C15: the house names its four score dots once per save ------------------------ */
static int dots_told(void) { return (doomo_town.unlocks[HUL_RAW_DOTS >> 3] >> (HUL_RAW_DOTS & 7)) & 1; }

TEST(prompts_house_names_its_dots_once_per_save)
{
    int f, at = -1;
    set_res(240, 160);
    test_reset();
    CHECK(!dots_told());
    s_home();
    for (f = 0; f < 400 && at < 0; f++) {
        frame(0);
        if (dots_told()) at = f;
    }
    /* after the room is in and the move-in toast has gone (the dots are not covered), and only once */
    CHECK(at >= 20);
    CHECK(ui_toast_pending() == 0);
    s_home();
    for (f = 0; f < 60; f++) frame(0);
    CHECK(dots_told());
}

/* ---- the retained panel ------------------------------------------------------------ */
static uint32_t fb_hash(void)
{
    uint32_t hh = 2166136261u;
    int i, n = nat_w * nat_h;
    for (i = 0; i < n; i++) hh = (hh ^ nat_fb[i]) * 16777619u;
    return hh;
}

#define PF 200
static void run_hashes(Setup s, uint32_t *out)
{
    int f;
    nat_panel_reset();
    doomo_init();
    test_reset();
    s();
    for (f = 0; f < PF; f++) {
        frame(f % 60 == 59 ? BTN_RIGHT : 0);
        out[f] = fb_hash();
    }
}

static int panel_same(const char *name, Setup s, int w, int h)
{
    static uint32_t off[PF], on[PF];
    int f;
    set_res(w, h);
    nat_panel_cache = 0;
    run_hashes(s, off);              /* a warm-up: what a scene leaves in its statics is the same for both runs */
    run_hashes(s, off);
    nat_panel_cache = 1;
    run_hashes(s, on);
    nat_panel_cache = 0;
    for (f = 0; f < PF; f++)
        if (off[f] != on[f]) {
            char m[200];
            snprintf(m, sizeof(m), "%s %dx%d: frame %d differs with the panel cache on", name, w, h, f);
            test_fail(__FILE__, __LINE__, m);
            return 0;
        }
    if (nat_panel_stats.rule) {
        char m[200];
        snprintf(m, sizeof(m), "%s %dx%d: panel %d broke a rule (%s)", name, w, h, nat_panel_stats.rule_panel,
                 nat_panel_stats.first_rule ? nat_panel_stats.first_rule : "?");
        test_fail(__FILE__, __LINE__, m);
        return 0;
    }
    if (nat_panel_stats.kept < PF / 2) {
        char m[200];
        snprintf(m, sizeof(m), "%s %dx%d: only %u of %u panels kept", name, w, h, nat_panel_stats.kept,
                 nat_panel_stats.begins);
        test_fail(__FILE__, __LINE__, m);
        return 0;
    }
    return 1;
}

TEST(prompts_bar_is_a_kept_panel_and_pixel_identical)
{
    if (!panel_same("pet", s_pet, 240, 160)) return;
    if (!panel_same("pet", s_pet, 320, 240)) return;
    if (!panel_same("workshop", s_work, 240, 160)) return;
    if (!panel_same("island", s_island, 240, 160)) return;
    if (!panel_same("island", s_island, 320, 240)) return;
    set_res(240, 160);
}
