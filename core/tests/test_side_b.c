/* test_side_b.c - side missions batch B (owner: S5; docs/design/story/SPEC.md
 * sec.7 S5 acceptance): the 55 rows of chains K M N O P Q R L U T V and the
 * stand-alones are live, titled, hinted and spotted (S186, the Feast, is a
 * row only: its scripts come with the Feast package); the seven
 * story_mg_b games run to a result (Clinker's poem scores exactly as the
 * board says, the hug line reaches 1,000); the six songs are real and the
 * four decorate-for layouts are in the house; Grumbleguts' story state, his
 * dating ticker (adults only), the no-rite lock; and missions through the
 * real talk path with autowin - S142 (timing), S164 (the poem, a text entry
 * in chalk), S166 (the haunting), S183 (cooking and the belly's verdict),
 * S206 (no comic beat), S211 ([NO GAG]) and S216 with Grumbleguts alive and
 * after the Feast. */
#include "test.h"
#include "../src/story_int.h"
#include "../src/overworld_int.h"
#include "../src/audio_int.h"
#include "../src/fp_int.h"
#include "../gen/furn_ids.h"
#include "doomo/house_api.h"
#include "doomo/furn.h"
#include "doomo/roster.h"
#include "doomo/sim.h"
#include <stdio.h>
#include <stdlib.h>

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
static void frames(int n, uint32_t b) { while (n-- > 0) frame(b); }
static void press(uint32_t b) { frame(b); frame(0); }

/* the batch: chains K M N O P Q R L U T V and the stand-alones */
static int batch_b(int m)
{
    return (m >= 137 && m <= 140) || m == 142 || (m >= 151 && m <= 159) || (m >= 161 && m <= 174) || m == 176 ||
           (m >= 178 && m <= 187) || m == 191 || m == 192 || m == 195 || (m >= 205 && m <= 217);
}

/* ---- the .dsc sources (the markers the compiler drops) ------------------------- */
static const char *const side_b_files[] = { "k_vex", "m_bloom", "o_lulla", "q_tock", "r_wisp", "l_vapula",
                                            "t_gate", "x_solo1", "x_solo2", "x_solo3", NULL };

static char *load_dsc(const char *name)
{
    char path[700];
    FILE *f;
    long n;
    char *t;
    snprintf(path, sizeof(path), "%s/../../data/story/side_b/%s.dsc", test_golden_dir, name);
    f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    t = (char *)malloc((size_t)n + 1);
    if (t && fread(t, 1, (size_t)n, f) != (size_t)n) n = 0;
    if (t) t[n] = 0;
    fclose(f);
    return t;
}

/* the text of mission `m`'s scripts: from its @mission line to the next */
static int mission_block(int m, char *out, int cap)
{
    char tag[24];
    int k;
    doomo_snprintf(tag, sizeof(tag), "@mission S%d ", m);
    for (k = 0; side_b_files[k]; k++) {
        char *t = load_dsc(side_b_files[k]), *a, *b;
        int n;
        if (!t) continue;
        a = strstr(t, tag);
        if (!a) { free(t); continue; }
        b = strstr(a + 1, "@mission ");
        n = b ? (int)(b - a) : (int)strlen(a);
        if (n >= cap) n = cap - 1;
        memcpy(out, a, (size_t)n);
        out[n] = 0;
        free(t);
        return n;
    }
    out[0] = 0;
    return 0;
}

/* ---- reachability (as test_side_a.c) -------------------------------------------- */
static uint8_t reach[64 * 64];

static void flood(int map)
{
    static uint16_t q[64 * 64];
    int qh = 0, qt = 0, sx = map == MAPID_HELLVILE ? 3 : map == MAPID_PARK_UNITED ? 16 : 34;
    int sy = map == MAPID_HELLVILE ? 27 : 26;
    const DoomoMap *mp;
    map_load(map);
    mp = &doomo_map;
    memset(reach, 0, sizeof(reach));
    if (!ow_walkable_cell(sx, sy, map_cell(mp, sx, sy)->floor_h)) return;
    reach[sy * 64 + sx] = 1;
    q[qt++] = (uint16_t)(sy * 64 + sx);
    while (qh < qt) {
        int c = q[qh++], x = c % 64, y = c / 64, d;
        static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        int fh = map_cell(mp, x, y)->floor_h;
        for (d = 0; d < 4; d++) {
            int nx = x + dx[d], ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= mp->w || ny >= mp->h || reach[ny * 64 + nx]) continue;
            if (!ow_walkable_cell(nx, ny, fh)) continue;
            reach[ny * 64 + nx] = 1;
            q[qt++] = (uint16_t)(ny * 64 + nx);
        }
    }
}

static int reachable(int x, int y) { return x >= 0 && y >= 0 && x < 64 && y < 64 && reach[y * 64 + x]; }

TEST(side_b_rows_are_live_and_their_givers_reachable)
{
    int m, live = 0, chains[26] = { 0 }, steps[26] = { 0 };
    char why[96];
    town_new(TOWN_STORY, 12, TOWN_POP_PREMADE);
    for (m = 100; m < 100 + STORY_SIDE2_ROWS; m++) {
        const StorySideRow *r = &doomo_side_rows[m - 100];
        int x, y, map;
        if (!batch_b(m)) continue;
        doomo_snprintf(why, sizeof(why), "S%d", m);
        CHECK_MSG(r->kind == 1, "%s is not live", why);
        if (r->kind != 1) continue;
        live++;
        CHECK_EQ(r->sid, m - 100);
        if (r->chain >= 'A' && r->chain <= 'Z') {
            chains[r->chain - 'A']++;
            if (r->step > steps[r->chain - 'A']) steps[r->chain - 'A'] = r->step;
        }
        CHECK_MSG(r->ch_lo >= 1 && r->ch_lo <= r->ch_hi && r->ch_hi <= 5, "%s: window", why);
        CHECK_MSG(r->later_min >= 1 && r->later_min <= 10, "%s: Later in 1..10 min", why);
        CHECK_MSG(r->reward % 10 == 0, "%s: reward", why);
        CHECK_MSG(town_res(r->giver) && town_res(r->giver)->status == RES_ALIVE, "%s: giver %d", why, r->giver);
        if (m == 186) {                                    /* the Feast: its package writes the rest */
            CHECK(r->offer == 0xFFFFu && r->spot == 0 && r->chain == 'T' && r->step == 5);
            continue;
        }
        CHECK_MSG(r->offer != 0xFFFFu && r->active != 0xFFFFu, "%s: offer / active scripts", why);
        CHECK_MSG(r->title != 0xFFFFu && story_side2_title(m - 100)[0] && story_side2_hint(m - 100)[0], "%s: title / hint", why);
        CHECK_MSG(r->spot >= 1 && r->spot <= doomo_side_spot_count, "%s: at= spot", why);
        if (r->spot < 1 || r->spot > doomo_side_spot_count) continue;
        x = doomo_side_spots[r->spot - 1][0];
        y = doomo_side_spots[r->spot - 1][1];
        map = r->side == 1 ? MAPID_HELLVILE : r->side == 2 ? MAPID_PARK_UNITED : MAPID_HAHALAND;
        flood(map);
        CHECK_MSG(reachable(x, y), "%s: giver at (%d,%d) on map %d unreachable", why, x, y, map);
    }
    CHECK_EQ(live, 55);
    for (m = 'K' - 'A'; m <= 'V' - 'A'; m++)              /* steps 1..n, no gaps */
        CHECK_MSG(chains[m] == steps[m], "chain %c: %d parts, last step %d", 'A' + m, chains[m], steps[m]);
}

/* ---- story_mg_b: each game runs to a result -------------------------------------- */
/* start the game (past its title card), then `policy` until it pops */
static int play_mg(int id, uint32_t (*policy)(int i))
{
    int i;
    scene_push(&SCENE_STORY_MG, id);
    frames(10, 0);
    press(BTN_A);                                          /* the title card */
    for (i = 0; i < 9000 && scene_top() == &SCENE_STORY_MG; i++) frame(policy ? policy(i) : 0);
    for (i = 0; i < 200 && scene_top() == &SCENE_STORY_MG; i++) press(BTN_A);   /* the result card */
    if (scene_top() == &SCENE_STORY_MG) {
        test_fail(__FILE__, __LINE__, "a story minigame never ended");
        return -1;
    }
    while (scene_depth() > 0) scene_pop();
    return story_mg_result();
}

static uint32_t idle(int i) { (void)i; return 0; }
static uint32_t tap(int i) { return (i & 1) ? 0 : BTN_A; }
static uint32_t right_then_idle(int i) { return i == 4 ? BTN_RIGHT : 0; }
static uint32_t mash(int i) { return (i % 3) == 0 ? BTN_A : (i % 7) == 0 ? BTN_LEFT : 0; }

/* Clinker's board: the downs for each of the 9 slots, then A; then nothing */
static const uint8_t *poem_downs;
static uint32_t poem(int i)
{
    static int slot, k, wait;
    if (i == 0) { slot = 0; k = 0; wait = 0; }
    if (slot >= 9) return 0;
    if (wait) { wait--; return 0; }
    wait = 2;
    if (k < poem_downs[slot]) { k++; return BTN_DOWN; }
    k = 0;
    slot++;
    return BTN_A;
}

/* the hug line: wait for the hugger, squeeze for their zone, let go */
static int hug_hold[8], hug_n;
static uint32_t hugs(int i)
{
    static int t, h, phase;
    if (i == 0) { t = 0; h = 0; phase = 0; }
    if (h >= hug_n) return 0;
    t++;
    if (phase == 0 && t >= (h == 0 ? 60 : 100)) { phase = 1; t = 0; }
    if (phase == 1) {
        if (t <= hug_hold[h]) return BTN_A;
        phase = 0;
        t = 0;
        h++;
    }
    return 0;
}

TEST(side_b_minigames_run)
{
    static const int ids[7] = { SMG_B_BUBBLES, SMG_B_BUBBLES_STAGE, SMG_B_FIREFLY, SMG_B_NAPOFF, SMG_B_POEM,
                                SMG_B_DUET, SMG_B_HUGS };
    static const uint8_t none_dead[9] = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };   /* "Small bells", "a day" */
    static const uint8_t one_dead[9] = { 0, 0, 0, 0, 0, 0, 0, 1, 0 };    /* "A skull" stays */
    static const uint8_t two_dead[9] = { 0 };                            /* "A skull", "the dead" */
    static const uint16_t queue[8] = { 855, 42, 1, 666, 77, 13, 111, ROSTER_ID_GRUMBLEGUTS };
    static const uint8_t hold[8] = { 33, 10, 21, 21, 21, 33, 10, 55 };   /* firm, gentler, gentle, all */
    int k, r;
    town_new(TOWN_STORY, 3, TOWN_POP_PREMADE);
    for (k = 0; k < 7; k++) {
        const StoryMgEntry *e = story_mg_find(ids[k]);
        CHECK_MSG(e && e->name && e->enter && e->update && e->draw, "story_mg 0x%x registered", ids[k]);
    }
    r = play_mg(SMG_B_BUBBLES, idle);
    CHECK(r == 0 || r == 1);
    CHECK_EQ(play_mg(SMG_B_BUBBLES_STAGE, tap), 1);       /* a press every other frame lands every ring */
    CHECK_EQ(play_mg(SMG_B_FIREFLY, idle), 1);            /* a heart, Glowbug finishing it */
    CHECK_EQ(play_mg(SMG_B_FIREFLY, right_then_idle), 2); /* two faces */
    r = play_mg(SMG_B_NAPOFF, mash);
    CHECK(r == 0 || r == 1);
    poem_downs = none_dead;
    CHECK_EQ(play_mg(SMG_B_POEM, poem), 1);               /* "Six. Needs death." */
    poem_downs = one_dead;
    CHECK_EQ(play_mg(SMG_B_POEM, poem), 2);               /* "Seven. The death is in the right place." */
    poem_downs = two_dead;
    CHECK_EQ(play_mg(SMG_B_POEM, poem), 3);               /* "Four. Too much death." */
    CHECK_EQ(play_mg(SMG_B_POEM, idle), 3);               /* it writes itself, slowly */
    r = play_mg(SMG_B_DUET, mash);
    CHECK(r == 0 || r == 1);
    CHECK_EQ(play_mg(SMG_B_HUGS, idle), 0);               /* nobody hugs back: it ends anyway */
    for (k = 0, hug_n = 0; k < 8; k++)
        if (town_res(queue[k])->status == RES_ALIVE) hug_hold[hug_n++] = hold[k];
    CHECK_EQ(play_mg(SMG_B_HUGS, hugs), 1);               /* ...and the counter reaches 1,000 */
    /* the six songs are real (equal channels, not the silent stub) */
    for (k = SONG_TWO_CHOIRS; k <= SONG_NUMBER_TEN; k++) {
        const char *err = NULL;
        if (k == SONG_HEEHEE_JINGLE) continue;
        CHECK_MSG(audio_songs[k].ch[0] && strlen(audio_songs[k].ch[0]) > 60, "song %d is a stub", k);
        CHECK_MSG(audio_song_check(k, &err), "song %d: %s", k, err ? err : "?");
    }
    for (k = 10; k <= 13; k++) {                          /* layouts_b.txt */
        int i, has = 0;
        for (i = 0; i < doomo_home_layout_count; i++)
            if (doomo_home_layouts[i].id == k && doomo_home_layouts[i].n >= doomo_home_layouts[i].need) has = 1;
        CHECK_MSG(has, "decorate-for layout %d (layouts_b.txt)", k);
    }
}

/* ---- Grumbleguts: story state, the ticker, the rite ------------------------------ */
TEST(side_b_grumbleguts_state_and_ticker)
{
    char line[NEWS_LEN];
    int d, id, posts;
    uint32_t day;
    town_new(TOWN_STORY, 729, TOWN_POP_PREMADE);
    memset(story_stry(), 0, sizeof(DoomoStry));
    for (d = 0; d < 20; d++) story_grumble_count(GRB_DISH, 0);
    CHECK_EQ(story_grumble_dishes(), 15);                /* capped */
    side_callback(CB_SIDE_GRUMBLE, GRB_EXES | 4 << 8);
    CHECK_EQ(story_grumble_exes_met(), 4);
    CHECK(!story_grumble_pacifist());
    side_callback(CB_SIDE_GRUMBLE, GRB_PACIFIST);
    CHECK(story_grumble_pacifist());
    CHECK_EQ(story_grumble_dishes(), 15);
    /* two thousand days of headlines: adults only, the Herald's width */
    for (day = 0; day < 2000; day++) {
        id = story_grumble_headline(day, line, (int)sizeof(line));
        CHECK_MSG(line[0] && strlen(line) <= NEWS_LEN - 1, "day %u: \"%s\"", (unsigned)day, line);
        if (id >= 0) {
            CHECK_MSG(id != ROSTER_ID_GRUMBLEGUTS && town_res(id)->status == RES_ALIVE, "day %u: target %d", (unsigned)day, id);
            CHECK_MSG(!(roster_traits(id) & (ROSTER_T_KID | ROSTER_T_TEEN)), "day %u: %s is a child", (unsigned)day, town_res(id)->name);
        }
    }
    /* the ticker starts with S173 and posts once a day at 13:00 */
    doomo_town.minute = 13 * 60;
    story_grumble_minute();
    CHECK(sim_news_count() == 0 || !strstr(sim_news(0), "Grumbleguts"));
    story_side2_set_state(73, 3);
    for (posts = 0, d = 0; d < 3; d++) {
        const char *before = sim_news_count() ? sim_news(0) : "";
        char was[NEWS_LEN];
        doomo_snprintf(was, sizeof(was), "%s", before);
        doomo_town.day++;
        doomo_town.minute = 12 * 60 + 59;
        story_grumble_minute();
        doomo_town.minute = 13 * 60;
        story_grumble_minute();
        story_grumble_minute();
        if (sim_news_count() && strcmp(sim_news(0), was)) posts++;
    }
    CHECK_EQ(posts, 3);
    /* after the Feast: one last line, then nothing; and no rite */
    story_set_flag(SF_GRUMBLE_DEAD, 1);
    story_set_flag(SF_GRUMBLE_NO_RITE, 1);
    town_res(ROSTER_ID_GRUMBLEGUTS)->status = RES_DEAD;
    CHECK(story_grumble_dead() && story_grumble_no_rite());
    CHECK_EQ(story_grumble_headline(doomo_town.day, line, (int)sizeof(line)), -1);
    doomo_town.day++;
    story_grumble_minute();
    CHECK(strstr(sim_news(0), "deceased") != NULL);
    doomo_town.day++;
    story_grumble_minute();
    CHECK(strstr(sim_news(0), "deceased") != NULL);       /* not again */
    CHECK(story_rite_blocker(ROSTER_ID_GRUMBLEGUTS));
    CHECK(!story_rite_blocker(42));
    CHECK(strstr(fp_ghost_rite_blocker(ROSTER_ID_GRUMBLEGUTS), "full") != NULL);
}

/* ---- missions through the real talk path ------------------------------------------ */
static void settle(void)
{
    int i;
    for (i = 0; i < 400 && scene_top() != &SCENE_STORY; i++) {
        const DoomoScene *t = scene_top();
        press(BTN_A);
        if (scene_top() == t && t != &SCENE_DIALOG && t != &SCENE_CUTSCENE) scene_pop();
    }
    for (i = 0; i < 600 && story_talking(); i++) press(BTN_A);
}

static void talk(int id)
{
    story_interact_npc(id);
    settle();
}

static void begin(uint32_t seed, int chapter, int beat)
{
    int e;
    story_debug_autowin(1);
    town_new(TOWN_STORY, seed, TOWN_POP_PREMADE);
    sim_init();
    scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
    story_debug_jump(chapter, beat);
    if (beat == BEAT_DONE)                                 /* the post-game: every episode seen */
        for (e = BEAT_P0; e < BEAT_ALL_COUNT; e++) story_episode_set_done(e, 1);
    frames(4, 0);
    settle();
}

static void done(int m) { story_side2_set_state(m - 100, 3); }

static int later_pending(unsigned scr)
{
    int i;
    for (i = 0; i < 4; i++) if (story_stry()->later[i].script == scr) return 1;
    return 0;
}

static void fire_later(unsigned scr)
{
    int i;
    CHECK_MSG(later_pending(scr), "Later %u not scheduled", scr);
    for (i = 0; i < 40 && later_pending(scr); i++) {
        doomo_town.minute = (uint16_t)(doomo_town.minute + 1);
        if (doomo_town.minute >= 1440) { doomo_town.minute = 0; doomo_town.day++; }
        frame(0);
        settle();
    }
    CHECK_MSG(!later_pending(scr), "Later %u never fired", scr);
}

static void run_mission(int m, int giver)
{
    int k;
    CHECK_MSG(story_side2_available(m - 100), "S%d not on offer", m);
    for (k = 0; k < 6 && story_side2_state(m - 100) != 3; k++) talk(giver);
    CHECK_MSG(story_side2_state(m - 100) == 3, "S%d not done (state %d)", m, story_side2_state(m - 100));
}

TEST(side_b_s142_the_fountain_is_late)
{
    uint32_t money;
    begin(142, 1, BEAT_EP1_4);
    money = doomo_town.money;
    run_mission(142, 69);
    CHECK(doomo_town.money >= money + 20);
    fire_later(SCR_S142_LATER);
    story_debug_autowin(0);
}

TEST(side_b_s164_the_poem_and_the_second_wall)
{
    begin(164, 4, BEAT_EP4_2);
    done(163);
    run_mission(164, ID_TALLYWORN);
    CHECK(home_has(FURN_CLINKERS_POEM) > 0);
    fire_later(SCR_S164_LATER);
    story_debug_autowin(0);
}

TEST(side_b_s166_the_haunting)
{
    begin(166, 2, BEAT_EP2_4);
    run_mission(166, 109);
    CHECK(home_has(FURN_WISP_DRAWING_1) > 0);
    fire_later(SCR_S166_LATER);
    story_debug_autowin(0);
}

TEST(side_b_s183_cooking_and_the_bellys_verdict)
{
    begin(183, 4, BEAT_EP4_7);
    run_mission(183, ROSTER_ID_GRUMBLEGUTS);
    CHECK_EQ(story_grumble_dishes(), 3);                  /* three dishes fed (GRB_DISH) */
    CHECK(home_has(FURN_FRAMED_GATE_KEY) > 0);
    fire_later(SCR_S183_LATER);
    story_debug_autowin(0);
}

/* S206: the chain's true finale has no comic beat at all; S211: the scare
 * is the whole quest, [NO GAG] */
static void no_comic_beat(int m)
{
    static char block[16384];
    CHECK_MSG(mission_block(m, block, (int)sizeof(block)) > 0, "S%d's scripts", m);
    CHECK_MSG(strstr(block, "[NO GAG]") != NULL, "S%d: [NO GAG]", m);
    CHECK_MSG(!strstr(block, "[PUNCH]") && !strstr(block, "{d}") && !strstr(block, "{r}") && !strstr(block, "SHK_") &&
              !strstr(block, "[GURGLE]"), "S%d: a comic beat", m);
}

TEST(side_b_s206_a_candle_goes_out)
{
    no_comic_beat(206);
    begin(206, 5, BEAT_DONE);
    done(166);
    done(167);
    done(205);
    done(168);
    story_stry()->branch = (uint8_t)((story_stry()->branch & ~3) | 2);   /* Ep 4.4: the kind lie */
    run_mission(206, 563);              /* Wisp */
    CHECK(home_has(FURN_WINDOW_CANDLE_OUT) > 0);
    fire_later(SCR_S206_LATER);
    story_debug_autowin(0);
}

TEST(side_b_s211_thirteen_bells)
{
    no_comic_beat(211);
    begin(211, 3, BEAT_EP3_3);
    story_side_debug_finish(24);                           /* Bellwether is saved */
    doomo_town.minute = 23 * 60;
    run_mission(211, 426);
    CHECK(home_has(FURN_SMALL_BELL) > 0);
    fire_later(SCR_S211_LATER);
    story_debug_autowin(0);
}

TEST(side_b_s216_free_hugs_and_after_the_feast)
{
    int dead;
    for (dead = 0; dead < 2; dead++) {
        begin(216 + (uint32_t)dead, 5, BEAT_DONE);
        if (dead) {
            story_set_flag(SF_GRUMBLE_DEAD, 1);
            town_res(ROSTER_ID_GRUMBLEGUTS)->status = RES_DEAD;
        }
        inv_add(ITEM_PLUSHIE, 1);
        run_mission(216, 20);
        CHECK_EQ(inv_count(ITEM_PLUSHIE), 0);
        fire_later(SCR_S216_LATER);
    }
    story_debug_autowin(0);
}
