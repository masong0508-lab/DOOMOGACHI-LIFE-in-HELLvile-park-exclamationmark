/* test_side_a.c - side missions batch A (owner: S4; docs/design/story/SPEC.md
 * sec.7 S4 acceptance): the 54 rows of S100-S136, S143-S150 and S196-S204
 * are live, offered, titled and hinted, their givers stand on reachable
 * cells, every chain can finish under the S2 gate (grafts included); the
 * six story_mg_a games run to a result; and six missions play through the
 * real talk path with autowin - S100 (housing), S101 (escort, photo,
 * timing), S118 (the practice rite), S133 (the reverse rite), S135 (three
 * nights) and S200 (no music, autowin off) - get their rewards and fire
 * their Later. */
#include "test.h"
#include "../src/story_int.h"
#include "../src/overworld_int.h"
#include "../src/audio_int.h"
#include "../gen/furn_ids.h"
#include "doomo/house_api.h"
#include "doomo/exorcism.h"
#include "doomo/furn.h"

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
static void frames(int n, uint32_t b) { while (n-- > 0) frame(b); }
static void press(uint32_t b) { frame(b); frame(0); }

/* the batch: S100-S136, S143-S150, S196-S204 */
static int batch_a(int m) { return (m >= 100 && m <= 136) || (m >= 143 && m <= 150) || (m >= 196 && m <= 204); }

/* ---- reachability (as test_story.c) -------------------------------------------- */
static uint8_t reach[64 * 64];

static void flood(int map, int sx, int sy)
{
    static uint16_t q[64 * 64];
    int qh = 0, qt = 0;
    const DoomoMap *m;
    map_load(map);
    m = &doomo_map;
    memset(reach, 0, sizeof(reach));
    if (!ow_walkable_cell(sx, sy, map_cell(m, sx, sy)->floor_h)) return;
    reach[sy * 64 + sx] = 1;
    q[qt++] = (uint16_t)(sy * 64 + sx);
    while (qh < qt) {
        int c = q[qh++], x = c % 64, y = c / 64, d;
        static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        int fh = map_cell(m, x, y)->floor_h;
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

TEST(side_a_rows_are_live_and_their_givers_reachable)
{
    int m, live = 0, chains[26] = { 0 }, steps[26] = { 0 };
    char why[96];
    town_new(TOWN_STORY, 11, TOWN_POP_PREMADE);
    for (m = 100; m < 100 + STORY_SIDE2_ROWS; m++) {
        const StorySideRow *r = &doomo_side_rows[m - 100];
        int x, y, map;
        if (!batch_a(m)) continue;
        doomo_snprintf(why, sizeof(why), "S%d", m);
        CHECK_MSG(r->kind == 1, "%s is not live", why);
        if (r->kind != 1) continue;
        live++;
        CHECK_EQ(r->sid, m - 100);
        CHECK_MSG(r->offer != 0xFFFFu && r->active != 0xFFFFu, "%s: offer / active scripts", why);
        CHECK_MSG(r->title != 0xFFFFu && story_side2_title(m - 100)[0] && story_side2_hint(m - 100)[0], "%s: title / hint", why);
        CHECK_MSG(r->ch_lo >= 1 && r->ch_lo <= r->ch_hi && r->ch_hi <= 5, "%s: window", why);
        CHECK_MSG(r->later_min >= 1 && r->later_min <= 10, "%s: Later in 1..10 min", why);
        CHECK_MSG(r->reward % 10 == 0, "%s: reward", why);
        CHECK_MSG(town_res(r->giver) && town_res(r->giver)->status == RES_ALIVE, "%s: giver %d", why, r->giver);
        CHECK_MSG(r->spot >= 1 && r->spot <= doomo_side_spot_count, "%s: at= spot", why);
        if (r->spot < 1 || r->spot > doomo_side_spot_count) continue;
        x = doomo_side_spots[r->spot - 1][0];
        y = doomo_side_spots[r->spot - 1][1];
        map = r->side == 1 ? MAPID_HELLVILE : MAPID_HAHALAND;
        flood(map, map == MAPID_HELLVILE ? 3 : 34, map == MAPID_HELLVILE ? 27 : 26);
        CHECK_MSG(reachable(x, y), "%s: giver at (%d,%d) on map %d unreachable", why, x, y, map);
        if (r->chain >= 'A' && r->chain <= 'Z') {
            chains[r->chain - 'A']++;
            if (r->step > steps[r->chain - 'A']) steps[r->chain - 'A'] = r->step;
        }
    }
    CHECK_EQ(live, 54);
    for (m = 0; m < 26; m++)                              /* steps 1..n, no gaps */
        CHECK_MSG(chains[m] == steps[m], "chain %c: %d parts, last step %d", 'A' + m, chains[m], steps[m]);
    /* Ch3: the Hillvile givers shelter round the church on the invaded map */
    flood(MAPID_HAHALAND_INVADED, 52, 39);
    for (m = 0; m < 12; m++) {
        static const uint8_t ring[12][2] = { { 12, 27 }, { 14, 28 }, { 16, 27 }, { 13, 29 }, { 18, 28 }, { 15, 29 },
                                             { 11, 27 }, { 17, 29 }, { 20, 27 }, { 19, 29 }, { 2, 27 }, { 21, 28 } };
        CHECK_MSG(reachable(ring[m][0], ring[m][1]) || reachable(ring[m][0], ring[m][1] + 1),
                  "church ring cell (%d,%d)", ring[m][0], ring[m][1]);
    }
}

/* every chain finishes under the S2 gate (step order, one part per chapter -
 * grafts exempt - and cross-border part 3+ from Ep 4.1), best case */
TEST(side_a_chains_can_finish)
{
    uint8_t side2[30], done_ch[26];
    int ch, pass, m, n = 0;
    memset(side2, 0, sizeof(side2));
    memset(done_ch, 0, sizeof(done_ch));
    for (ch = 1; ch <= 5; ch++)
        for (pass = 0; pass < 8; pass++)
            for (m = 100; m < 100 + STORY_SIDE2_ROWS; m++) {
                int sid = m - 100;
                const StorySideRow *r = &doomo_side_rows[sid];
                if (!batch_a(m) || !story_side2_gate(doomo_side_rows, sid, side2, ch, ch >= 4, done_ch)) continue;
                side2[sid >> 2] = (uint8_t)(side2[sid >> 2] | (3u << ((sid & 3) * 2)));
                if (r->chain && sid < STORY_SIDE2_GRAFT_FIRST) done_ch[r->chain - 'A'] = (uint8_t)ch;
                n++;
            }
    CHECK_EQ(n, 54);
}

/* ---- story_mg_a: each game runs to a result --------------------------------------- */
static int play_mg(int id, int policy)
{
    int i;
    scene_push(&SCENE_STORY_MG, id);
    for (i = 0; i < 6000 && scene_top() == &SCENE_STORY_MG; i++) {
        uint32_t b = 0;
        switch (policy) {
        case 0: b = (i % 5) == 0 ? BTN_A : 0; break;                              /* mash */
        case 1: b = (i % 10) < 8 ? BTN_A : BTN_RIGHT; break;                       /* paint, roll */
        default: b = (i % 3) == 0 ? BTN_A : (i % 37) == 1 ? BTN_RIGHT : (i % 53) == 2 ? BTN_DOWN : 0; break;
        }
        frame(b);
    }
    if (scene_top() == &SCENE_STORY_MG) {
        test_fail(__FILE__, __LINE__, "a story minigame never ended");
        return -1;
    }
    while (scene_depth() > 0) scene_pop();
    return story_mg_result();
}

TEST(side_a_minigames_run)
{
    static const int ids[6] = { SMG_A_HOPSCOTCH, SMG_A_ROLLER, SMG_A_WHACK, SMG_A_WHACK_OFF, SMG_A_BALLOONS, SMG_A_BOBBER };
    int k, r;
    town_new(TOWN_STORY, 3, TOWN_POP_PREMADE);
    for (k = 0; k < 6; k++) {
        const StoryMgEntry *e = story_mg_find(ids[k]);
        CHECK_MSG(e && e->name && e->enter && e->update && e->draw, "story_mg 0x%x registered", ids[k]);
    }
    r = play_mg(SMG_A_HOPSCOTCH, 0);
    CHECK(r == 0 || r == 1);
    CHECK_EQ(play_mg(SMG_A_ROLLER, 1), 1);               /* paint every column: 1000 */
    r = play_mg(SMG_A_WHACK, 2);
    CHECK(r == 1 || r == 2);
    CHECK_EQ(play_mg(SMG_A_WHACK_OFF, 2), 1);            /* it ends on the "beep." */
    r = play_mg(SMG_A_BALLOONS, 2);
    CHECK(r == 0 || r == 1);
    r = play_mg(SMG_A_BOBBER, 0);
    CHECK(r == 0 || r == 1);
    /* the lullaby is a real song now, and the layouts are in the house */
    CHECK(audio_songs[SONG_WALL_LULLABY].ch[0] && strlen(audio_songs[SONG_WALL_LULLABY].ch[0]) > 40);
    for (k = 4; k <= 8; k++) {
        int i, has = 0;
        for (i = 0; i < doomo_home_layout_count; i++)
            if (doomo_home_layouts[i].id == k && doomo_home_layouts[i].n >= doomo_home_layouts[i].need) has = 1;
        CHECK_MSG(has, "decorate-for layout %d (layouts_a.txt)", k);
    }
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
    story_debug_autowin(1);
    town_new(TOWN_STORY, seed, TOWN_POP_PREMADE);
    sim_init();
    scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
    story_debug_jump(chapter, beat);
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

/* the Later: time passes on the overworld; the callback plays and is gone */
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

/* talk to the giver until it is done: the offer ("yes" is the first
 * option), the task; an old quest of the same giver takes its turn first
 * now and then (story_side.c side_npc), and is simply taken too */
static void run_mission(int m, int giver)
{
    int k;
    CHECK_MSG(story_side2_available(m - 100), "S%d not on offer", m);
    for (k = 0; k < 6 && story_side2_state(m - 100) != 3; k++) talk(giver);
    CHECK_MSG(story_side2_state(m - 100) == 3, "S%d not done (state %d)", m, story_side2_state(m - 100));
}

TEST(side_a_s100_a_bed_for_mr_flops)
{
    uint32_t money;
    begin(100, 1, BEAT_EP1_4);
    inv_add(ITEM_PLUSHIE, 1);
    money = doomo_town.money;
    run_mission(100, ID_PIP);
    CHECK_EQ(inv_count(ITEM_PLUSHIE), 0);
    CHECK(doomo_town.money >= money + 30);
    CHECK(home_has(FURN_MR_FLOPS_BED) > 0);
    fire_later(SCR_S100_LATER);
    story_debug_autowin(0);
}

TEST(side_a_s101_night_patrol)
{
    uint32_t money;
    begin(101, 2, BEAT_EP2_2);
    done(100);
    money = doomo_town.money;
    run_mission(101, ID_PIP);
    CHECK(doomo_town.money >= money + 40);
    CHECK(home_has(FURN_WALL_WATCH_BADGE) > 0);
    fire_later(SCR_S101_LATER);
    story_debug_autowin(0);
}

TEST(side_a_s118_the_practice_rite)
{
    int water;
    begin(118, 3, BEAT_EP3_2);
    done(117);
    water = inv_count(ITEM_HOLY_WATER);
    run_mission(118, ID_FUZZ);
    CHECK(inv_count(ITEM_HOLY_WATER) >= water + 3);
    fire_later(SCR_S118_LATER);
    story_debug_autowin(0);
}

TEST(side_a_s133_the_reverse_rite)
{
    uint8_t out[16];
    int k, n, found = 0;
    begin(133, 4, BEAT_EP4_3);
    done(130);
    done(131);
    done(132);
    exorcism_possess(81, 1);                             /* Ep 2.5: WHEEZLE, scripted to fail */
    exorcism_apply(81, 0);
    CHECK_EQ(town_res(81)->status, RES_SOUL_LOST);
    run_mission(133, ID_BLOOM);
    CHECK_EQ(town_res(81)->status, RES_ALIVE);           /* EXORCISM UNFAILED x1 */
    CHECK(home_has(FURN_HEART_BALLOON) > 0);
    n = story_trophies(doomo_side_rows, story_stry()->side2, out, 16);
    for (k = 0; k < n; k++) if (out[k] == 33) found = 1;
    CHECK_MSG(found, "the Unfailed trophy");
    fire_later(SCR_S133_LATER);
    story_debug_autowin(0);
}

TEST(side_a_s135_three_nights)
{
    uint32_t day;
    begin(135, 2, BEAT_EP2_2);
    done(134);
    day = doomo_town.day;
    run_mission(135, 69);
    CHECK(doomo_town.day >= day + 2u);                    /* three nights at the Wall */
    CHECK(home_has(FURN_VAPOR_DETECTOR) > 0);
    fire_later(SCR_S135_LATER);
    story_debug_autowin(0);
}

TEST(side_a_s200_no_music)
{
    int i;
    begin(200, 2, BEAT_EP2_5);
    done(127);
    story_debug_autowin(0);                              /* stop at the verb */
    CHECK(story_side2_available(200 - 100));
    story_interact_npc(41);
    for (i = 0; i < 600 && story_verb_active() != VERB_FIND; i++) press(BTN_A);
    CHECK_EQ(story_verb_active(), VERB_FIND);
    CHECK_EQ(story_side2_state(200 - 100), 2);
    CHECK(town_is_night());
    CHECK(audio_fx_music_gated());                       /* the whole errand without music */
    story_verbs_cell(24, 44);                            /* the bath house */
    for (i = 0; i < 1200 && story_side2_state(200 - 100) != 3; i++) {
        if (i < 900) CHECK_MSG(audio_fx_music_gated() || story_side2_state(200 - 100) == 3, "music back at %d", i);
        press(BTN_A);
    }
    CHECK_EQ(story_side2_state(200 - 100), 3);
    settle();
    frames(10, 0);                                       /* the next audio tick */
    CHECK(!audio_fx_music_gated());                      /* ...and it comes back after */
    CHECK(home_has(FURN_FACING_DUCK) > 0);
    story_debug_autowin(1);
    fire_later(SCR_S200_LATER);
    story_debug_autowin(0);
}
