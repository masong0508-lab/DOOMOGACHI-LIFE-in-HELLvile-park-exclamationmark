/* test_clarity_cl6.c - clarity C7 (wayfinding), C10 (the shooter explains
 * itself) and C16 (one name per thing), package CL6 (owner: story / action).
 *
 * core/tools/story_wayfind.py (story-check) lints the .dsc sources: every
 * objective names a direction or a landmark and no banned alias appears.
 * These tests check the same objectives as the game decodes and wraps them,
 * and the runtime pieces: the goal's place on the Map tab, the shooter's
 * control card that waits for A (Brutal Only included) and its fit at
 * 240x160, the labelled HUD, and the plain names. */
#include <string.h>
#include "test.h"
#include "../src/levels_int.h"
#include "../src/story_int.h"

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
static void frames(int n, uint32_t b) { while (n-- > 0) frame(b); }

/* every HUD objective of the main story (ch1-ch4, the interlude, the epilogue) */
static const uint16_t objectives[] = {
    TXT_OBJ_C1_MAYOR, TXT_OBJ_C1_KEY, TXT_OBJ_C1_HOME, TXT_OBJ_C1_HATCH, TXT_OBJ_C1_PIP, TXT_OBJ_C1_BUNNY,
    TXT_OBJ_C1_BUNNY_RETURN, TXT_OBJ_C1_FRIENDS, TXT_OBJ_C1_FRIENDS_DONE, TXT_OBJ_C1_CUPCAKE_LULLA,
    TXT_OBJ_C1_PARTY, TXT_OBJ_C1_WALL, TXT_OBJ_C1_HOME_NIGHT,
    TXT_OBJ_C2_VAPOR, TXT_OBJ_C2_BLOOM, TXT_OBJ_C2_CLUES, TXT_OBJ_C2_REPORT, TXT_OBJ_C2_NIGHT, TXT_OBJ_C2_NIGHT2,
    TXT_OBJ_C2_NIGHT3, TXT_OBJ_C2_SCREAM, TXT_OBJ_C2_FUZZ, TXT_OBJ_C2_EXORCISE, TXT_OBJ_C2_RIVET, TXT_OBJ_C2_REST,
    TXT_OBJ_C2_CELLAR,
    TXT_O31_WAKE, TXT_O31_RUN, TXT_O31_CHURCH, TXT_O31_RIVET, TXT_O32_BOARD, TXT_O32_NIGHT, TXT_O33_NESTS,
    TXT_O33_HEART, TXT_O33_RIVET, TXT_O34_FOUNTAIN, TXT_O35_RITES, TXT_O36_SPIRE, TXT_O37_PARTY, TXT_O37_TRAIL,
    TXT_OI3_HOME, TXT_OI3_KNOCK,
    TXT_O41_GATE, TXT_O41_TALLY, TXT_O42_SWAP, TXT_O43_NOX, TXT_O43_FAVOURS, TXT_O43_FLOWERS, TXT_O43_BACK,
    TXT_O44_PITS, TXT_O44_SLEEP, TXT_O45_GATE, TXT_O45_BOTTOM, TXT_OF4_FETCH, TXT_O46_SUMMIT, TXT_O47_STALLS,
    TXT_O47_STAGE, TXT_OE1_FREE, TXT_OE2_SUITCASE
};

/* the story_wayfind.py rule: a parenthetical naming a compass point or a landmark, or "follow the arrow" */
static int names_where(const char *s)
{
    static const char *const words[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW", "here", "home", "plaza", "anywhere" };
    const char *p = s;
    if (strstr(s, "follow the arrow")) return 1;
    while ((p = strchr(p, '(')) != NULL) {
        const char *e = strchr(p, ')'), *q = p + 1;
        if (!e) return 0;
        while (q < e) {
            const char *c = q;
            int k, n;
            while (c < e && *c != ',') c++;
            while (*q == ' ') q++;
            n = (int)(c - q);
            for (k = 0; k < (int)DOOMO_COUNTOF(words); k++)
                if ((int)strlen(words[k]) == n && strncmp(q, words[k], (size_t)n) == 0) return 1;
            q = c + 1;
        }
        p = e + 1;
    }
    return 0;
}

TEST(clarity_c7_every_objective_names_where_and_fits_the_pill)
{
    int i;
    char raw[96], out[96];
    test_reset();
    CHECK_EQ((int)DOOMO_COUNTOF(objectives), 59);
    for (i = 0; i < (int)DOOMO_COUNTOF(objectives); i++) {
        int n[1] = { 9 }, k, j = 0;
        CHECK(txt_get(objectives[i], raw, (int)sizeof(raw)) > 0);
        CHECK_MSG(names_where(raw), "objective \"%s\" names no direction or landmark", raw);
        /* @E is the pet's name: 7 letters at most (town.h name[8]) */
        for (k = 0; raw[k] && j < (int)sizeof(out) - 8; k++) {
            if (raw[k] == '@' && raw[k + 1] == 'E') { memcpy(out + j, "Abcdefg", 7); j += 7; k++; }
            else out[j++] = raw[k];
        }
        out[j] = 0;
        vm_fmt_n(raw, (int)sizeof(raw), out, n, 1);
        /* the pill's text width at 240x160 (overworld_fx.c draw_objective: plat_screen_w() - 92) */
        CHECK_MSG(ui_text_wrap(0, 0, 240 - 92, raw, 0, 0) <= 2, "objective \"%s\" takes 3+ lines at 240x160", raw);
        /* C16: one name per place */
        CHECK_MSG(!strstr(raw, "Nana's bakery") && !strstr(raw, "Hell Pits") && !strstr(raw, "Hillvile flowers"),
                  "objective \"%s\" uses an alias", raw);
    }
}

TEST(clarity_c7_map_tab_names_the_goal_building)
{
    char buf[64];
    test_reset();
    scene_set(&SCENE_STORY, STORY_ARG_DEBUG_BEAT + BEAT_C1_KEY);
    frames(30, 0);
    CHECK_EQ(story_beat(), BEAT_C1_KEY);
    story_goal_place(buf, (int)sizeof(buf));
    /* Nana waits at Brimstone Bites: who, then the building by its one name */
    CHECK_MSG(strstr(buf, "Brimstone Bites") != NULL, "goal place \"%s\"", buf);
    CHECK_MSG(strstr(buf, town_res(ID_NANA)->name) == buf, "goal place \"%s\" does not start with Nana", buf);
    CHECK(strlen(buf) > 0 && ui_text_width(buf) < 240);
    while (scene_depth() > 0) scene_pop();
}

TEST(clarity_c16_one_name_per_place)
{
    char b[32];
    /* the HUD banner of the park on the HaHaland map is the plain name (it was "Hellvile Park - Hillvile Side") */
    CHECK_STR(story_region_name(MAPID_HAHALAND, 30, 40), "The Park");
    CHECK_STR(story_region_name(MAPID_HAHALAND_INVADED, 30, 40), "The Park");
    CHECK_STR(map_title(MAPID_ARENA), "Demon Arena");
    CHECK_STR(story_place_name(PLACE_CAFE), "Brimstone Bites");
    /* the pause row is the music volume, not a song number */
    opt_set(OPT_MUSIC_VOL, 7);
    CHECK_STR(pause_music_label(b, (int)sizeof(b)), "Music volume 7");
}

TEST(clarity_c10_control_card_fits_every_scheme_at_240)
{
    static const int schemes[3] = { CONTROLS_CLASSIC, CONTROLS_MODERN, CONTROLS_DUAL_STICK };
    char l[CCARD_MAX_LINES][CCARD_LINE_CAP];
    int s, i, n, r;
    test_reset();
    for (s = 0; s < 3; s++) {
        opt_set(OPT_CONTROL_SCHEME, schemes[s]);
        /* the card is 230 px wide at 240x160 (6 px margins), 300 at 320x240 */
        n = ccard_lines(l, CCARD_MAX_LINES, 230 - 12);
        CHECK_MSG(n >= 2 && n <= 3, "scheme %d: %d control lines at 240x160", schemes[s], n);
        for (i = 0; i < n; i++) CHECK_MSG(ui_text_width(l[i]) <= 230 - 12, "scheme %d: \"%s\" too wide", schemes[s], l[i]);
        CHECK(strstr(l[0], "Shoot") != NULL);
        n = ccard_lines(l, CCARD_MAX_LINES, 300 - 12);
        CHECK(n >= 2 && n <= 3);
    }
    for (r = CCARD_RULE_CUTE; r <= CCARD_RULE_TIME; r++) {
        CHECK(ccard_mode_rule(r) != NULL);
        CHECK_MSG(ui_text_width(ccard_mode_rule(r)) <= 230 - 12, "rule \"%s\" too wide", ccard_mode_rule(r));
    }
    CHECK(ccard_mode_rule(CCARD_RULE_NONE) == NULL);
    /* the whole card leaves the banner, the flavour and the par line above it at 240x160 */
    CHECK(ccard_height(230, "x", ccard_mode_rule(CCARD_RULE_BRUTAL)) <= 160 - 17 - 77);
    opt_set(OPT_CONTROL_SCHEME, CONTROLS_MODERN);
}

/* every Breach mode (Brutal Only included) waits on its card for A; a restart starts by itself */
TEST(clarity_c10_breach_card_waits_for_a_in_every_mode)
{
    int mode, i;
    for (mode = 0; mode < LVM_COUNT; mode++) {
        test_reset();
        scene_set(&SCENE_TITLE, 0);
        scene_push(&SCENE_BREACH, LV_VAPE_CELLAR | mode << 8);
        frames(200, 0);
        CHECK(scene_top() == &SCENE_BREACH);
        CHECK_MSG(combat_stats()->ticks == 0, "mode %d: the level ran behind its card", mode);
        frame(BTN_A); frame(0);
        frames(10, 0);
        CHECK_MSG(combat_stats()->ticks > 0, "mode %d: A did not start the level", mode);
        while (scene_depth() > 0) scene_pop();
    }
    /* Restart from the pause menu: the card shows again but starts by itself */
    test_reset();
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_BREACH, LV_VAPE_CELLAR);
    frames(20, 0);
    frame(BTN_A); frame(0);
    frames(5, 0);
    frame(BTN_START); frame(0);
    for (i = 0; i < 5; i++) { frame(BTN_DOWN); frame(0); }          /* Restart level */
    frame(BTN_A); frame(0);
    frames(100, 0);
    CHECK(combat_stats()->ticks > 0);
    while (scene_depth() > 0) scene_pop();
}

TEST(clarity_c10_arena_title_card_waits_and_b_leaves)
{
    test_reset();
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_ARENA, 0);
    frames(200, 0);
    CHECK(scene_top() == &SCENE_ARENA);
    CHECK_EQ((int)combat_stats()->ticks, 0);                         /* no wave behind the card */
    frame(BTN_B); frame(0);
    CHECK(scene_top() == &SCENE_TITLE);                              /* B walks back out */
    scene_push(&SCENE_ARENA, 0);
    frames(20, 0);
    frame(BTN_A); frame(0);
    frames(30, 0);
    CHECK(combat_stats()->ticks > 0);                                /* A starts wave 1 */
    while (scene_depth() > 0) scene_pop();
    /* the arena modes: the first intro of a visit is a card too */
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_ARENA_MODE, AM_HORDE);
    frames(200, 0);
    CHECK(scene_top() == &SCENE_ARENA_MODE);
    CHECK_EQ((int)combat_stats()->ticks, 0);
    frame(BTN_A); frame(0);
    frames(30, 0);
    CHECK(combat_stats()->ticks > 0);
    while (scene_depth() > 0) scene_pop();
}
