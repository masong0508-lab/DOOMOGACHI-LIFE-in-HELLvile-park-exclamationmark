/* test_story_ch34.c - Chapters 3-4, Interlude III, F2-F4, D2 and the
 * epilogue (owner: S3; SPEC sec.7 S3 acceptance): the length budgets, every
 * episode jumped to (objective, done bits, target and cast on reachable
 * cells), a playthrough from the eruption to the suitcase through the real
 * scripts (autowin), the choices' consequences, the pet's story marks, the
 * Say It picks, the S3 cutscenes and the pet-care boss on their stages, a
 * save round trip mid-chapter, the exorcism skins in the real scene. */
#include "test.h"
#include "../src/story_int.h"
#include "../src/story_ch34.h"
#include "../src/overworld_int.h"
#include "doomo/beat.h"
#include "doomo/exorcism.h"
#include "doomo/unlock_ids.h"
#include "doomo/home.h"
#include "doomo/house_api.h"

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
static void frames(int n, uint32_t b) { while (n-- > 0) frame(b); }
static void press(uint32_t b) { frame(b); frame(0); }

static void story_town(uint32_t seed)
{
    test_reset();
    story_debug_autowin(1);
    town_new(TOWN_STORY, seed, TOWN_POP_PREMADE);
    sim_init();
    scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
}

/* the episodes of Chapters 3-4 and the epilogue, in story order */
static const uint8_t episodes[] = {
    BEAT_EP3_1, BEAT_EP3_2, BEAT_EP3_3, BEAT_EP3_4, BEAT_F3, BEAT_EP3_5, BEAT_EP3_6, BEAT_EP3_7,
    BEAT_F2, BEAT_INT3, BEAT_EP4_1, BEAT_EP4_2, BEAT_EP4_3, BEAT_EP4_4, BEAT_D2, BEAT_EP4_5,
    BEAT_F4, BEAT_EP4_6, BEAT_EP4_7, BEAT_E1, BEAT_E2
};
#define NEP ((int)DOOMO_COUNTOF(episodes))

/* A through dialogue and cutscenes, START through the credits, the debug
 * step on the overworld, B (then a pop) on anything else the plot opened */
static int play_until_done(int ep, int max_iter)
{
    int i;
    for (i = 0; i < max_iter; i++) {
        const DoomoScene *top = scene_top();
        if (story_episode_done(ep) && top == &SCENE_STORY && !story_talking()) return 1;
        if (top == &SCENE_DIALOG || top == &SCENE_CUTSCENE || top == &SCENE_CONFIRM || top == &SCENE_CUTX) {
            press(BTN_A);
            continue;
        }
        if (top == &SCENE_CREDITS) { press(BTN_START); frames(40, 0); press(BTN_A); continue; }
        if (top == &SCENE_STORY) { story_debug_step(); frame(0); continue; }
        press(BTN_B);
        if (scene_top() == top) scene_pop();
    }
    return 0;
}

/* ---- the length budgets: Ch3 210, Ch4 210 (+-10%), the epilogue >= 25 ------------------ */
static int chapter_minutes(int c)
{
    int p, m = 0;
    for (p = 0; p < txt_pack_count(); p++)
        if (doomo_txt_pack_kinds[p] == TXT_KIND_EPISODE && doomo_txt_pack_chapters[p] == c)
            m += doomo_txt_pack_minutes[p];
    return m;
}

TEST(story_ch34_length_budget)
{
    int m3 = chapter_minutes(3), m4 = chapter_minutes(4), me = chapter_minutes(5);
    CHECK_MSG(m3 >= 189 && m3 <= 231, "chapter 3 declares %d minutes", m3);
    CHECK_MSG(m4 >= 189 && m4 <= 231, "chapter 4 declares %d minutes", m4);
    CHECK_MSG(me >= 25, "the epilogue declares %d minutes", me);
}

/* ---- every episode: done bits, objective, target, cast on reachable cells ------------- */
static uint8_t reach[64 * 64];

static void flood(int map, int sx, int sy)
{
    static uint16_t q[64 * 64];
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    int qh = 0, qt = 0;
    const DoomoMap *m;
    map_load(map);
    m = &doomo_map;
    memset(reach, 0, sizeof(reach));
    if (!ow_walkable_cell(sx, sy, map_cell(m, sx, sy)->floor_h)) return;
    reach[sy * 64 + sx] = 1;
    q[qt++] = (uint16_t)(sy * 64 + sx);
    while (qh < qt) {
        int c = q[qh++], x = c % 64, y = c / 64, d, fh = map_cell(m, x, y)->floor_h;
        for (d = 0; d < 4; d++) {
            int nx = x + dx[d], ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h || reach[ny * 64 + nx]) continue;
            if (!ow_walkable_cell(nx, ny, fh)) continue;
            reach[ny * 64 + nx] = 1;
            q[qt++] = (uint16_t)(ny * 64 + nx);
        }
    }
}

static int reachable(int x, int y) { return x >= 0 && y >= 0 && x < 64 && y < 64 && reach[y * 64 + x]; }
static int spot_ok(int x, int y)
{
    return reachable(x, y) || reachable(x, y + 1) || reachable(x, y - 1) || reachable(x - 1, y) || reachable(x + 1, y);
}

TEST(story_ch34_every_episode_lands_consistent_and_reachable)
{
    static const struct { uint8_t map, x, y; } starts[4] = {
        { MAPID_HAHALAND, 34, 26 }, { MAPID_HAHALAND_INVADED, 52, 39 },
        { MAPID_HELLVILE, 3, 27 }, { MAPID_PARK_UNITED, 16, 26 }
    };
    StoryNpcSpot sp[32];
    int k, i;
    for (k = 0; k < NEP; k++) {
        int ep = episodes[k], m, cx, cy, o = story_episode_order(ep), e;
        test_reset();
        town_new(TOWN_STORY, 31 + (uint32_t)k, TOWN_POP_PREMADE);
        sim_init();
        story_new_game();
        story_debug_jump(0, ep);
        CHECK_MSG(ch34_episode() == ep, "jump to %d landed on episode %d", ep, ch34_episode());
        for (e = 0; e < BEAT_ALL_COUNT; e++)
            if (story_episode_order(e) >= 0 && story_episode_order(e) < o)
                CHECK_MSG(story_episode_done(e), "episode %d: episode %d not done", ep, e);
        CHECK_MSG(!story_episode_done(ep), "episode %d is done on arrival", ep);
        story_objective_refresh();
        CHECK_MSG(story_objective()[0] != 0, "no objective at episode %d", ep);
        story_target(&m, &cx, &cy);
        for (i = 0; i < 4; i++) {
            int n, j;
            flood(starts[i].map, starts[i].x, starts[i].y);
            if (m == starts[i].map)
                CHECK_MSG(spot_ok(cx, cy), "episode %d target (%d,%d) on map %d unreachable", ep, cx, cy, m);
            n = script_cast(starts[i].map, sp, 32);
            for (j = 0; j < n; j++)
                CHECK_MSG(spot_ok(sp[j].x, sp[j].y), "episode %d: cast %d at (%d,%d) map %d unreachable", ep,
                          sp[j].id, sp[j].x, sp[j].y, starts[i].map);
        }
        /* the state earlier episodes leave behind */
        if (o <= story_episode_order(BEAT_EP3_5))
            CHECK_MSG(town_res(ID_HATTIE)->status == RES_POSSESSED, "episode %d: Hattie not possessed", ep);
        else CHECK(story_flag(SF_C3_EXO0) && story_flag(SF_C3_EXO0 + 1) && town_res(ID_HATTIE)->status != RES_POSSESSED);
        if (o > story_episode_order(BEAT_EP3_1)) CHECK(story_flag(SF_HOUSE_PLATED));
        if (o > story_episode_order(BEAT_EP3_7)) CHECK(story_flag(SF_PET_HORNS) && (story_stry()->pet_form & 2));
        if (o >= story_episode_order(BEAT_EP4_1) && o <= story_episode_order(BEAT_EP4_3))
            CHECK_MSG(town_res(ID_LADY_SMOG)->status == RES_POSSESSED, "episode %d: Lady Smog not possessed", ep);
        if (o > story_episode_order(BEAT_EP4_3)) CHECK(story_flag(SF_C4_LADY_SMOG) && story_flag(SF_C4_SOOTKIN));
        if (o >= story_episode_order(BEAT_EP4_1)) CHECK(home_phase() >= HP_OPEN);
        if (o > story_episode_order(BEAT_EP4_4)) CHECK(story_flag(SF_WISP_AWAKE));
        while (scene_depth() > 0) scene_pop();
    }
}

/* ---- from the eruption to the suitcase ----------------------------------------------- */
TEST(story_ch34_playthrough_through_the_new_scripts)
{
    int k;
    story_town(4343);
    story_debug_jump(0, BEAT_EP3_1);
    frames(5, 0);
    for (k = 0; k < NEP; k++) {
        int ok = play_until_done(episodes[k], 8000);
        CHECK_MSG(ok, "stuck in episode %d (now %d, step %d, beat %s)", episodes[k], ch34_episode(), story_step(),
                  story_beat_name(story_beat()));
        if (!ok) break;
    }
    CHECK_EQ(story_beat(), BEAT_DONE);
    CHECK_EQ(story_chapter(), STORY_CH_DONE);
    /* what the chapters left */
    CHECK(story_flag(SF_CREDITS_SEEN));
    CHECK(unlock_has(USTORY_UNITED_PARK) && unlock_has(USTORY_NEW_GAME_PLUS) && unlock_has(USTORY_HELLVILE_VISITS));
    CHECK(unlock_has(UL_STORY_MISC2_FIRST + 0) && unlock_has(UL_STORY_MISC2_FIRST + 8));
    CHECK(story_flag(SF_PET_HORNS) && (story_stry()->pet_form & 2));
    CHECK(story_flag(SF_HOUSE_PLATED));
    CHECK(story_flag(SF_C3_EXO0) && story_flag(SF_C3_EXO0 + 1));
    CHECK_EQ(story_stry()->nests & 15, 15);
    CHECK(story_flag(SF_C4_SOOTKIN) && story_flag(SF_C4_LADY_SMOG) && story_flag(SF_C4_RIVET_HOME));
    CHECK(story_flag(SF_WISP_AWAKE) && story_flag(SF_VAPULA_PLUSH) && story_flag(SF_SUITCASE_OPEN));
    CHECK(story_flag(SF_MASCOTTY_HEAD_OFF) && story_flag(SF_FOUNTAIN_TWOWAY));
    CHECK(home_phase() >= HP_OPEN);
    CHECK(story_stry()->branch & 3);                          /* Wisp answered */
    for (k = 0; k < NEP; k++) CHECK_MSG(story_episode_done(episodes[k]), "episode %d not done", episodes[k]);
    /* nothing pops up afterwards: free roam */
    frames(120, 0);
    CHECK(scene_top() == &SCENE_STORY);
    CHECK(!story_talking());
    story_debug_autowin(0);
}

/* ---- the choices and what reads them -------------------------------------------------- */
TEST(story_ch34_choice_consequences)
{
    DoomoStry *st;
    char a[128];
    story_town(77);
    story_debug_jump(0, BEAT_EP4_4);
    st = story_stry();
    st->branch = 0;
    /* Wisp's answer (the second player line) */
    vm_set_var(0, 2);
    script_callback(CB_C34, C34A_WISP);
    CHECK_EQ(st->branch & 3, 3);
    /* the third player line -> the polaroid's caption (E2) */
    vm_set_var(0, 2);
    script_callback(CB_C34, C34A_SAYIT);
    CHECK_EQ((st->branch >> 2) & 3, 2);
    CHECK_EQ(st->branch & 3, 3);
    /* the brutal weapon: no pacifist, no halo */
    story_set_flag(SF_WEAPON_BRUTAL, 1);
    st->pet_form = 3;
    script_callback(CB_C34, C34A_PACIFIST);
    CHECK(!(st->pet_form & 4));
    CHECK(!unlock_has(USTORY_PACIFIST));
    story_set_flag(SF_WEAPON_BRUTAL, 0);
    script_callback(CB_C34, C34A_PACIFIST);
    CHECK(st->pet_form & 4);
    CHECK(story_flag(SF_PET_HALO) && unlock_has(USTORY_PACIFIST));
    /* the knocks */
    st->knocks = 0;
    script_callback(CB_C34, C34A_KNOCK | (6 << 8));
    CHECK(story_flag(SF_KNOCK(6)));
    CHECK_EQ(st->knocks, 7);
    /* the Spire's captions follow the Vapor Sample and the question */
    story_set_flag(SF_VAPE_SAMPLE, 1);
    story_set_flag(SF_ASKED_OTHER_SIDE, 0);
    script_callback(CB_C34, C34A_SPIRE_CAPS);
    CHECK(txt_get(TXT_V36_F3P, a, sizeof(a)) > 0);
    CHECK(strcmp(txt_stash_get(1), a) == 0);
    CHECK(txt_get(TXT_V36_F4N, a, sizeof(a)) > 0);
    CHECK(strcmp(txt_stash_get(2), a) == 0);
    while (scene_depth() > 0) scene_pop();
    story_debug_autowin(0);
}

/* ---- the pet's story marks on every form ---------------------------------------------- */
int pet_story_marks(void);

TEST(story_pet_forms)
{
    static uint8_t a[PET_SPR * PET_SPR], b[PET_SPR * PET_SPR];
    int form, fr, marks, differ = 0, total = 0;
    story_town(5);
    for (form = 0; form < PET_FORM_COUNT; form++)
        for (fr = 0; fr < PET_ANIM_COUNT; fr++) {
            story_stry()->pet_form = 0;
            CHECK_EQ(pet_story_marks(), 0);
            pet_render(form, fr, a);
            for (marks = 1; marks <= 4; marks <<= 1) {
                story_stry()->pet_form = (uint8_t)marks;
                CHECK_EQ(pet_story_marks(), marks);
                pet_render(form, fr, b);
                differ += memcmp(a, b, sizeof(a)) != 0;
                total++;
            }
        }
    CHECK_MSG(differ * 3 >= total * 2, "the marks changed only %d of %d renders", differ, total);  /* own horns / halo stay */
    /* the image cache keys on the marks */
    story_stry()->pet_form = 0;
    {
        const DoomoImage *i0 = pet_form_image(3, 0);
        uint32_t u0 = i0->uid;
        story_stry()->pet_form = 2;
        CHECK(pet_form_image(3, 0)->uid != u0);
    }
    /* free play never shows them */
    story_stry()->pet_form = 7;
    doomo_town.mode = TOWN_FREEPLAY;
    CHECK_EQ(pet_story_marks(), 0);
}

/* ---- Say It ----------------------------------------------------------------------------- */
static int said_count(void)
{
    int k, n = 0;
    for (k = 0; k < 96; k++) n += (story_stry()->sayit_said[k >> 3] >> (k & 7)) & 1;
    return n;
}

TEST(story_sayit_picks_eight_friends_and_remembers)
{
    int k, j;
    char line[128];
    story_town(9);
    story_debug_jump(0, BEAT_EP4_6);
    memset(story_stry()->sayit_said, 0, sizeof(story_stry()->sayit_said));
    summit_begin();
    for (k = 0; k < SAYIT_FRIENDS; k++) {
        CHECK(summit_friend(k) >= 0);
        CHECK_MSG(summit_line(k, line, sizeof(line)) > 0, "friend %d has nothing to say", k);
        for (j = 0; j < k; j++) CHECK(summit_friend(j) != summit_friend(k));
    }
    CHECK_EQ(said_count(), SAYIT_FRIENDS);
    summit_begin();                                            /* the next run: new things are said */
    CHECK_EQ(said_count(), 2 * SAYIT_FRIENDS);
    for (k = 0; k < 20; k++) summit_begin();                   /* the pool never runs dry */
    for (k = 0; k < SAYIT_FRIENDS; k++) CHECK(summit_line(k, line, sizeof(line)) > 0);
    while (scene_depth() > 0) scene_pop();
    story_debug_autowin(0);
}

/* ---- the S3 cutscenes and the pet-care boss on their stages ----------------------------- */
TEST(story_ch34_cutscenes_run_and_end)
{
    static const uint8_t cuts[6] = { CUT_F2, CUT_F3, CUT_F4, CUT_SUMMIT, CUT_SAYIT, CUT_E1 };
    static const uint8_t eps[6] = { BEAT_F2, BEAT_F3, BEAT_F4, BEAT_EP4_6, BEAT_EP4_6, BEAT_E1 };
    int k, i;
    for (k = 0; k < 6; k++) {
        CHECK(CUTX_OWNS(cuts[k]));
        story_town(11 + (uint32_t)k);
        story_debug_jump(0, eps[k]);
        frames(2, 0);
        scene_push(&SCENE_CUTX, cuts[k]);
        CHECK(scene_top() == &SCENE_CUTX);
        frames(8, 0);
        for (i = 0; i < 4000 && scene_top() == &SCENE_CUTX; i++) frame((i & 15) == 0 ? BTN_A : 0);
        CHECK_MSG(scene_top() != &SCENE_CUTX, "cut %d never ended", cuts[k]);
        while (scene_depth() > 0) scene_pop();
    }
    /* START skips */
    story_town(17);
    story_debug_jump(0, BEAT_F2);
    scene_push(&SCENE_CUTX, CUT_F2);
    frames(10, 0);
    press(BTN_START);
    frames(2, 0);
    CHECK(scene_top() != &SCENE_CUTX);
    while (scene_depth() > 0) scene_pop();
    story_debug_autowin(0);
}

TEST(story_pet_care_boss_ends)
{
    int i;
    story_town(23);
    story_debug_jump(0, BEAT_EP4_5);
    while (scene_depth() > 0) scene_pop();
    scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
    scene_push(&SCENE_PET, PET_ARG_BOSS);
    CHECK(scene_top() == &SCENE_PET);
    for (i = 0; i < 3000 && scene_top() == &SCENE_PET; i++)
        frame((i % 9) == 0 ? BTN_A : (i % 23) < 3 ? BTN_LEFT : (i % 31) < 3 ? BTN_RIGHT : 0);
    CHECK_MSG(scene_top() != &SCENE_PET, "the boss never let go");
    while (scene_depth() > 0) scene_pop();
    story_debug_autowin(0);
}

/* ---- a save in the middle of Ep 4.3 ----------------------------------------------------- */
TEST(story_ch34_save_round_trip_mid_chapter)
{
    int i;
    story_town(29);
    story_debug_jump(0, BEAT_EP4_3);
    frames(3, 0);
    story_interact_npc(ID_MOTHER_NOX);                         /* the three favours begin */
    for (i = 0; i < 2000 && (story_talking() || scene_top() != &SCENE_STORY); i++) press(i & 1 ? BTN_A : 0);
    CHECK_EQ(ch34_episode(), BEAT_EP4_3);
    CHECK_EQ(story_step(), 1);
    CHECK(town_res(ID_LADY_SMOG)->status == RES_POSSESSED);
    story_set_flag(SF_C4_RIVET_HOME, 1);
    ow_sync_to_save();
    CHECK(save_game(0));
    story_set_beat(BEAT_C1_KEY);
    story_set_flag(SF_C4_RIVET_HOME, 0);
    exorcism_slots_clear();
    CHECK(load_game(0));
    CHECK_EQ(story_beat(), BEAT_C4_HELP);
    CHECK(story_flag(SF_C4_RIVET_HOME));
    scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
    frames(20, 0);
    CHECK_EQ(ch34_episode(), BEAT_EP4_3);
    CHECK_EQ(story_step(), 1);
    CHECK_EQ(map_current_id(), MAPID_HELLVILE);
    CHECK(town_res(ID_LADY_SMOG)->status == RES_POSSESSED);
    CHECK(exorcism_authored(ID_LADY_SMOG) != NULL);
    while (scene_depth() > 0) scene_pop();
    story_debug_autowin(0);
}

/* ---- the exorcism skins run in the real scene ------------------------------------------- */
TEST(story_exorcism_skins_run)
{
    static const int16_t hosts[3] = { ID_HATTIE, ID_GRAMPY, ID_LADY_SMOG };
    static const uint8_t eps[3] = { BEAT_EP3_5, BEAT_EP3_5, BEAT_EP4_3 };
    int k, i;
    for (k = 0; k < 3; k++) {
        story_town(41 + (uint32_t)k);
        story_debug_jump(0, eps[k]);
        story_debug_autowin(0);
        frames(2, 0);
        CHECK_MSG(town_res(hosts[k])->status == RES_POSSESSED, "host %d not possessed", hosts[k]);
        CHECK(exorcism_authored(hosts[k]) != NULL);
        while (scene_depth() > 0) scene_pop();
        scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
        frames(2, 0);
        scene_push(&SCENE_EXORCISM, hosts[k]);
        CHECK(scene_top() != &SCENE_STORY);
        for (i = 0; i < 12000 && scene_top() != &SCENE_STORY; i++)
            frame((i % 7) == 0 ? BTN_A : (i % 11) == 0 ? BTN_B : (i % 400) == 399 ? BTN_START : 0);
        CHECK_MSG(scene_top() == &SCENE_STORY, "rite %d never ended (top %s)", k, scene_top() ? scene_top()->name : "-");
        while (scene_depth() > 0) scene_pop();
    }
}
