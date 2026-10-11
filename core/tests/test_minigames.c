/* test_minigames.c - the Fun Fair minigames (owner: minigames). */
#include "test.h"
#include "../src/mg.h"

static const DoomoScene *const game_scenes[MG_GAME_COUNT] = {
    &SCENE_MG_CATCH, &SCENE_MG_WHACK, &SCENE_MG_DODGE,
    &SCENE_MG_GALLERY, &SCENE_MG_DERBY, &SCENE_MG_MEMORY
};

/* Runs a scene headless: logic every tick, drawing every 8th tick. Stops
 * once the flow reaches RESULTS (or after max ticks). */
static int run_until_results(int max)
{
    int i;
    for (i = 0; i < max; i++) {
        input_update(0);
        scene_update();
        if ((i & 7) == 0) scene_draw();
        if (mg.phase == MGP_RESULTS) return i;
    }
    return -1;
}

static void premade_island(void)
{
    town_new(TOWN_FREEPLAY, 777, TOWN_POP_PREMADE);
}

TEST(mg_activities_registered)
{
    const DoomoActivity *out[16];
    int n, i, found_wheel = 0;
    const DoomoActivity *a = doomo_activities_minigames;
    CHECK(a != NULL);
    for (n = 0; a[n].name; n++) {
        CHECK(a[n].location == ACTLOC_FUN_FAIR);
        CHECK(a[n].scene != NULL);
        CHECK((a[n].flags & (ACTF_FREEPLAY | ACTF_STORY)) == (ACTF_FREEPLAY | ACTF_STORY));
        CHECK(strlen(a[n].desc) <= 40);
        CHECK(a[n].unlock_id == ACT_ALWAYS);
        if (a[n].scene == &SCENE_MG_DODGE) CHECK(a[n].min_chapter == 3);
        if (a[n].scene == &SCENE_MG_WHEEL) found_wheel = 1;
    }
    CHECK_EQ(n, 7);
    CHECK(found_wheel);
    /* free play: all seven booths */
    CHECK_EQ(activities_at(ACTLOC_FUN_FAIR, out, 16), 7);
    /* story: chapter-gated */
    doomo_town.mode = TOWN_STORY;
    doomo_town.story.chapter = 0;
    CHECK_EQ(activities_at(ACTLOC_FUN_FAIR, out, 16), 0);
    doomo_town.story.chapter = 1;
    n = activities_at(ACTLOC_FUN_FAIR, out, 16);
    CHECK_EQ(n, 5);
    for (i = 0; i < n; i++) CHECK(out[i]->scene != &SCENE_MG_DODGE && out[i]->scene != &SCENE_MG_GALLERY);
    doomo_town.story.chapter = 2;
    CHECK_EQ(activities_at(ACTLOC_FUN_FAIR, out, 16), 6);
    doomo_town.story.chapter = 3;
    CHECK_EQ(activities_at(ACTLOC_FUN_FAIR, out, 16), 7);
}

TEST(mg_scenes_by_name)
{
    CHECK(scene_by_name("MG_CATCH") == &SCENE_MG_CATCH);
    CHECK(scene_by_name("MG_WHACK") == &SCENE_MG_WHACK);
    CHECK(scene_by_name("MG_DODGE") == &SCENE_MG_DODGE);
    CHECK(scene_by_name("MG_GALLERY") == &SCENE_MG_GALLERY);
    CHECK(scene_by_name("MG_DERBY") == &SCENE_MG_DERBY);
    CHECK(scene_by_name("MG_MEMORY") == &SCENE_MG_MEMORY);
    CHECK(scene_by_name("SCENE_MG_WHEEL") == &SCENE_MG_WHEEL);
}

TEST(mg_unlock_table_is_sane)
{
    const DoomoUnlock *u = doomo_unlocks_minigames;
    uint8_t seen[MG_UL_LAST - MG_UL_FIRST + 1];
    int n, g, m;
    memset(seen, 0, sizeof(seen));
    for (n = 0; u[n].name; n++) {
        CHECK(u[n].id >= MG_UL_FIRST && u[n].id <= MG_UL_LAST);
        CHECK(u[n].id <= 863);
        CHECK(!seen[u[n].id - MG_UL_FIRST]);
        seen[u[n].id - MG_UL_FIRST] = 1;
        CHECK(u[n].how && u[n].how[0]);
        CHECK(unlock_by_id(u[n].id) == &u[n]);          /* no other team uses the id */
        /* no other table locks the same thing */
        CHECK(unlock_find(u[n].cat, u[n].list, u[n].index) == &u[n]);
    }
    CHECK_EQ(n, MG_UL_LAST - MG_UL_FIRST + 1);
    /* medal rules agree with the games' medal scores */
    for (g = 0; g < MG_GAME_COUNT; g++) {
        CHECK_EQ(mg_games[g]->id, g);
        for (m = 1; m <= 3; m++) {
            const DoomoUnlock *x = unlock_by_id(MG_UL_MEDAL(g, m));
            CHECK(x != NULL);
            CHECK_EQ(x->prog, mg_games[g]->prog);
            CHECK_EQ(x->need, mg_games[g]->medal[m - 1]);
        }
        CHECK(mg_games[g]->medal[0] < mg_games[g]->medal[1]);
        CHECK(mg_games[g]->medal[1] < mg_games[g]->medal[2]);
        CHECK(doomo_progress_names[mg_games[g]->prog][0] != 0);
    }
}

TEST(mg_medals_and_rewards)
{
    const MgGame *g = &mg_game_catch;
    uint32_t money = doomo_town.money;
    CHECK_EQ(mg_medal_for(g, 0), MG_MEDAL_NONE);
    CHECK_EQ(mg_medal_for(g, g->medal[0] - 1), MG_MEDAL_NONE);
    CHECK_EQ(mg_medal_for(g, g->medal[0]), MG_MEDAL_BRONZE);
    CHECK_EQ(mg_medal_for(g, g->medal[1]), MG_MEDAL_SILVER);
    CHECK_EQ(mg_medal_for(g, g->medal[2] + 5000), MG_MEDAL_GOLD);
    CHECK_EQ(mg_reward_for(g, 0), 0);
    CHECK(mg_reward_for(g, 500) < mg_reward_for(g, 1500));
    CHECK(mg_reward_for(g, g->medal[2]) > mg_reward_for(g, g->medal[2] - 1) + 20);   /* gold bonus */
    CHECK(mg_reward_for(g, 9999999) == MG_REWARD_CAP + 50);                           /* capped */
    /* a silver round: best saved, money paid, bronze + silver unlocked */
    mg.tier = MG_TIER_SWEET;
    CHECK(!unlock_has(MG_UL_MEDAL(MG_CATCH, MG_MEDAL_BRONZE)));
    CHECK_EQ(mg_finish_round(g, g->medal[1] + 10), 1);
    CHECK_EQ(progress_get(PROG_HS_CATCH), g->medal[1] + 10);
    CHECK_EQ(doomo_town.money, money + (uint32_t)mg_reward_for(g, g->medal[1] + 10));
    CHECK(unlock_has(MG_UL_MEDAL(MG_CATCH, MG_MEDAL_BRONZE)));
    CHECK(unlock_has(MG_UL_MEDAL(MG_CATCH, MG_MEDAL_SILVER)));
    CHECK(!unlock_has(MG_UL_MEDAL(MG_CATCH, MG_MEDAL_GOLD)));
    CHECK(!unlock_ok(UL_CAT_PART, CLIST_HAIR, HAIR_TWINTAILS));
    CHECK(unlock_ok(UL_CAT_SKIN, OPT_WINDOW_SKIN, WSKIN_BUBBLEGUM));
    /* a worse round is not a new best and does not lower it */
    CHECK_EQ(mg_finish_round(g, 100), 0);
    CHECK_EQ(progress_get(PROG_HS_CATCH), g->medal[1] + 10);
    /* gold */
    mg_finish_round(g, g->medal[2]);
    CHECK(unlock_ok(UL_CAT_PART, CLIST_HAIR, HAIR_TWINTAILS));
    CHECK(!unlock_has(MG_UL_REGULAR));
}

TEST(mg_regular_and_all_gold_trophies)
{
    int g;
    for (g = 0; g < MG_GAME_COUNT; g++) {
        mg.tier = MG_TIER_SWEET;
        mg_finish_round(mg_games[g], 10);
    }
    CHECK(unlock_has(MG_UL_REGULAR));
    CHECK(!unlock_has(MG_UL_GOLD_ALL));
    for (g = 0; g < MG_GAME_COUNT; g++) mg_finish_round(mg_games[g], mg_games[g]->medal[2]);
    CHECK(unlock_has(MG_UL_GOLD_ALL));
    CHECK(unlock_ok(UL_CAT_PART, CLIST_SHOES, SHOES_HEELS));
    CHECK(!unlock_has(MG_UL_HELLFIRE));
    mg.tier = MG_TIER_HELLFIRE;
    mg_finish_round(mg_games[0], mg_games[0]->medal[2]);
    CHECK(unlock_has(MG_UL_HELLFIRE));
}

TEST(mg_title_card_tiers_and_leave)
{
    scene_push(&SCENE_MG_WHACK, 1);
    CHECK(mg.phase == MGP_TITLE);
    CHECK_EQ(mg.tier, MG_TIER_SWEET);
    input_update(0); scene_update(); scene_draw();
    input_update(BTN_RIGHT); scene_update();
    input_update(0); scene_update();
    input_update(BTN_RIGHT); scene_update();
    CHECK_EQ(mg.tier, MG_TIER_HELLFIRE);
    CHECK(mg.hell);
    scene_draw();
    input_update(0); scene_update();
    input_update(BTN_B); scene_update();
    CHECK_EQ(scene_depth(), 0);
    /* the tier of the last round played is remembered */
    scene_push(&SCENE_MG_WHACK, MG_ARG_AUTOSTART | 2);
    CHECK_EQ(mg.tier, MG_TIER_SPICY);
    scene_pop();
    scene_push(&SCENE_MG_WHACK, 0);
    CHECK_EQ(mg.tier, MG_TIER_SPICY);
}

TEST(mg_pause_and_quit)
{
    int i;
    scene_push(&SCENE_MG_CATCH, MG_ARG_AUTOSTART | 1);
    CHECK(mg.phase == MGP_READY);
    for (i = 0; i < 60; i++) { input_update(0); scene_update(); }
    CHECK(mg.phase == MGP_PLAY);
    input_update(BTN_START); scene_update();
    CHECK(mg.phase == MGP_PAUSE);
    {
        uint16_t t = mg.play_t;
        for (i = 0; i < 20; i++) { input_update(0); scene_update(); }
        CHECK_EQ(mg.play_t, t);                     /* frozen while paused */
    }
    scene_draw();
    input_update(BTN_DOWN); scene_update();
    input_update(0); scene_update();
    input_update(BTN_A); scene_update();
    CHECK_EQ(scene_depth(), 0);
}

static void play_all_tiers(int game, int island)
{
    int tier;
    for (tier = 0; tier < MG_TIERS; tier++) {
        int ticks, best_before;
        test_reset();
        if (island) premade_island();
        best_before = (int)progress_get(mg_games[game]->prog);
        scene_push(game_scenes[game], MG_ARG_AUTOSTART | MG_ARG_AUTOPLAY | (tier + 1));
        CHECK_EQ(mg.tier, tier);
        ticks = run_until_results(4000);
        CHECK_MSG(ticks > 0, "%s tier %d never reached results", mg_games[game]->title, tier);
        CHECK_MSG(mg.score > 0, "%s tier %d scored nothing", mg_games[game]->title, tier);
        CHECK((int)progress_get(mg_games[game]->prog) >= best_before);
        CHECK_EQ(progress_get(mg_games[game]->prog), (uint32_t)mg.score);
        CHECK(mg_fx_live() <= 40);
        printf("      %-17s %-8s score %5d  medal %d  reward %3d H$  (%d ticks)\n",
               mg_games[game]->title, mg_tier_names[tier], (int)mg.score, mg.medal, (int)mg.reward, ticks);
        /* the results screen draws and Leave pops the scene */
        while (mg.t < 40 || mg.shown < mg.score || mg.reward_shown < mg.reward) { input_update(0); scene_update(); }
        scene_draw();
        input_update(BTN_RIGHT); scene_update();     /* Leave */
        input_update(0); scene_update();
        input_update(BTN_A); scene_update();
        CHECK_EQ(scene_depth(), 0);
    }
}

TEST(mg_catch_plays) { play_all_tiers(MG_CATCH, 0); }
TEST(mg_whack_plays) { play_all_tiers(MG_WHACK, 0); }
TEST(mg_dodge_plays) { play_all_tiers(MG_DODGE, 0); }
TEST(mg_gallery_plays) { play_all_tiers(MG_GALLERY, 1); }
TEST(mg_gallery_plays_empty_island) { play_all_tiers(MG_GALLERY, 0); }
TEST(mg_derby_plays) { play_all_tiers(MG_DERBY, 0); }
TEST(mg_memory_plays) { play_all_tiers(MG_MEMORY, 1); }
TEST(mg_memory_plays_empty_island) { play_all_tiers(MG_MEMORY, 0); }

TEST(mg_bot_earns_bronze_on_sweet)
{
    /* every game is winnable: the (human-ish, imperfect) bot reaches
     * bronze on Sweet in at least one of three rounds */
    int g, k;
    for (g = 0; g < MG_GAME_COUNT; g++) {
        int best = 0;
        for (k = 0; k < 3 && best < mg_games[g]->medal[0]; k++) {
            test_reset();
            premade_island();
            mg_rng.s = 0;
            scene_push(game_scenes[g], MG_ARG_AUTOSTART | MG_ARG_AUTOPLAY | 1);
            rng_seed(&mg_rng, 1000u + (uint32_t)(g * 31 + k * 7));
            CHECK(run_until_results(4000) > 0);
            if (mg.score > best) best = (int)mg.score;
            while (scene_depth()) scene_pop();
        }
        CHECK_MSG(best >= mg_games[g]->medal[0], "%s bot best %d (bronze %d)",
                  mg_games[g]->title, best, mg_games[g]->medal[0]);
    }
}

TEST(mg_retry_restarts_round)
{
    scene_push(&SCENE_MG_WHACK, MG_ARG_AUTOSTART | MG_ARG_AUTOPLAY | 1);
    CHECK(run_until_results(4000) > 0);
    while (mg.t < 30) { input_update(0); scene_update(); }
    if (mg.shown < mg.score || mg.reward_shown < mg.reward) {
        input_update(BTN_A); scene_update();         /* skip the count-up */
        CHECK(mg.shown == mg.score);
        input_update(0); scene_update();
    }
    input_update(BTN_A); scene_update();             /* Retry */
    CHECK(mg.phase == MGP_READY);
    CHECK_EQ(mg.score, 0);
    CHECK_EQ(scene_depth(), 1);
}

TEST(mg_memory_faces)
{
    int16_t f[10];
    int i, j, n, hell = 0;
    n = mg_memory_pick_faces(10, f, 42);             /* empty island */
    CHECK_EQ(n, 10);
    for (i = 0; i < n; i++) {
        CHECK(f[i] < 0 && f[i] >= -10);
        for (j = 0; j < i; j++) CHECK(f[i] != f[j]);
    }
    premade_island();
    n = mg_memory_pick_faces(10, f, 42);
    CHECK_EQ(n, 10);
    for (i = 0; i < n; i++) {
        CHECK(f[i] >= 0 && town_alive(f[i]));
        if (f[i] >= DOOMO_DISTRICT_SIZE) hell++;
        for (j = 0; j < i; j++) CHECK(f[i] != f[j]);
    }
    CHECK(hell >= 2);                                /* some cards bite */
}

TEST(mg_derby_pet_stats_matter)
{
    int rental, baby, adult;
    rental = mg_derby_pet_speed();
    CHECK_EQ(rental, 75);
    pet_new("Zoom");
    doomo_town.pet.stage = PET_STAGE_BABY;
    doomo_town.pet.hunger = 0;
    doomo_town.pet.joy = 0;
    doomo_town.pet.weight = 10;
    baby = mg_derby_pet_speed();
    doomo_town.pet.stage = PET_STAGE_ADULT;
    doomo_town.pet.hunger = 4;
    doomo_town.pet.joy = 4;
    adult = mg_derby_pet_speed();
    CHECK(adult > rental);
    CHECK(baby < rental);
    doomo_town.pet.weight = 99;
    CHECK(mg_derby_pet_speed() < adult);
}

TEST(mg_wheel_once_a_day)
{
    int i;
    doomo_town.day = 3;
    CHECK(mg_wheel_ready());
    scene_push(&SCENE_MG_WHEEL, MG_ARG_AUTOPLAY);
    for (i = 0; i < 900 && progress_get(PROG_FAIR_WHEEL_DAY) == 0; i++) { input_update(0); scene_update(); }
    for (i = 0; i < 900; i++) { input_update(0); scene_update(); if ((i & 15) == 0) scene_draw(); }
    CHECK_EQ(progress_get(PROG_FAIR_WHEEL_DAY), 4u);
    CHECK(!mg_wheel_ready());
    scene_draw();
    while (scene_depth()) scene_pop();
    scene_push(&SCENE_MG_WHEEL, 0);                  /* already spun today */
    for (i = 0; i < 20; i++) { input_update(0); scene_update(); }
    scene_draw();
    input_update(BTN_A); scene_update();
    CHECK_EQ(scene_depth(), 0);
    doomo_town.day = 4;
    CHECK(mg_wheel_ready());
}

TEST(mg_wheel_prizes_and_curses)
{
    int w, curses = 0;
    for (w = 0; w < MG_WHEEL_WEDGES; w++) {
        CHECK(mg_wheel_names[w] && mg_wheel_names[w][0]);
        curses += mg_wheel_is_curse(w);
    }
    CHECK_EQ(curses, 3);
    doomo_town.money = 500;
    CHECK_EQ(mg_wheel_apply(7), 777);
    CHECK(unlock_has(MG_UL_JACKPOT));
    CHECK_EQ(doomo_town.money, 1277u);
    CHECK_EQ(mg_wheel_apply(2), -100);              /* the Tax Imp caps at 100 */
    doomo_town.money = 50;
    CHECK_EQ(mg_wheel_apply(2), -5);
    CHECK(unlock_has(MG_UL_CURSED));
    {
        int c0 = inv_count(ITEM_CUPCAKE), b0 = inv_count(ITEM_BRIMSTONE_BAGEL);
        CHECK_EQ(mg_wheel_apply(1), 0);
        CHECK_EQ(inv_count(ITEM_CUPCAKE), c0 + 3);
        mg_wheel_apply(5);
        CHECK_EQ(inv_count(ITEM_BRIMSTONE_BAGEL), b0 + 3);
    }
    doomo_town.money = 10;
    CHECK_EQ(mg_wheel_apply(8), -10);               /* nobody to spook: a fine */
    premade_island();
    {
        uint32_t before = doomo_town.money;
        mg_wheel_apply(8);                           /* spooks a citizen instead */
        CHECK_EQ(doomo_town.money, before);
    }
}

TEST(mg_draws_at_both_sizes)
{
    /* every screen at 320x240 too (layouts adapt) */
    int g, i;
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = 320; o.h = 240; o.seed = 1; o.quiet = 1; o.save_dir = test_tmp_dir;
    nat_init(&o);
    for (g = 0; g < MG_GAME_COUNT; g++) {
        test_reset();
        scene_push(game_scenes[g], 1);
        for (i = 0; i < 10; i++) { input_update(0); scene_update(); }
        scene_draw();
        input_update(BTN_A); scene_update();
        for (i = 0; i < 200; i++) { input_update(0); scene_update(); }
        scene_draw();
        CHECK(mg.phase == MGP_PLAY || mg.phase == MGP_END);
        while (scene_depth()) scene_pop();
    }
    o.w = 240; o.h = 160;
    nat_init(&o);
}
