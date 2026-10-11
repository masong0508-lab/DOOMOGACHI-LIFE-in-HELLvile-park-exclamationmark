/* test_exorcism.c - Father Fuzz's ritual and the death rules (owner:
 * life): odds and tools, the final chance, the true-name puzzle, success
 * and failure transitions (three failures lose the soul), lay-to-rest
 * gating (never an ALIVE citizen unless Monster Mode), grief, ghosts, and
 * the scene itself (practice and a real host). */
#include "test.h"

static int premade_possessed(int id)
{
    town_new(TOWN_FREEPLAY, 99, TOWN_POP_PREMADE);
    sim_init();
    sim_corrupt(id, 100);
    return town_res(id)->status == RES_POSSESSED;
}

static void frames(int n, uint32_t b)
{
    while (n-- > 0) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
}

TEST(exorcism_possession_names_a_hellvile_citizen)
{
    int p, p2, i;
    CHECK(premade_possessed(42));
    CHECK_EQ(sim_possessed_count(), 1);
    p = sim_possessor_of(42);
    CHECK(p >= DOOMO_DISTRICT_SIZE && p < DOOMO_MAX_RESIDENTS);
    CHECK(sim_possessor_name(42)[0] != 0);
    CHECK(sim_problem_for(42) >= 0);
    CHECK_EQ(sim_problem(sim_problem_for(42))->kind, PROB_POSSESSED);
    CHECK_EQ(sim_mood_of(42), MOOD_POSSESSED);
    CHECK(strstr(sim_news(0), "POSSESSED") != NULL);
    /* the same demon through failed attempts */
    exorcism_apply(42, 0);
    p2 = sim_possessor_of(42);
    CHECK_EQ(p, p2);
    /* deterministic for the island */
    for (i = 0; i < 3; i++) CHECK_EQ(sim_possessor_of(42), p);
    CHECK_EQ(sim_possessor_of(43), -1);           /* not possessed */
}

TEST(exorcism_odds_and_tools)
{
    int base, with, all, i;
    unsigned mask = 0;
    CHECK(premade_possessed(42));
    base = exorcism_odds(42);
    CHECK(base >= 5 && base <= 95);
    with = exorcism_odds_with(42, 1u << EXI_ROSARY);
    CHECK(with > base);
    for (i = 0; i < EXI_COUNT; i++) mask |= 1u << i;
    all = exorcism_odds_with(42, mask);
    CHECK(all >= with);
    CHECK(all <= 95);
    /* every failure lets it dig in deeper */
    town_res(42)->exorcism = (2 << 4) | 2;
    CHECK(exorcism_odds(42) < base);
    /* difficulty */
    town_res(42)->exorcism = 0;
    opt_set(OPT_DIFFICULTY, DIFF_PLUSHIE);
    CHECK(exorcism_odds(42) > base);
    opt_set(OPT_DIFFICULTY, DIFF_ULTRA_HELLVILE);
    CHECK(exorcism_odds(42) < base);
    /* the chance formula */
    CHECK(exorcism_chance(60, 100, 100, 1) > exorcism_chance(60, 50, 50, 1));
    CHECK(exorcism_chance(60, 100, 100, 1) > exorcism_chance(60, 100, 100, 0) + 20);
    CHECK(exorcism_chance(95, 100, 100, 1) <= 97);
    CHECK(exorcism_chance(5, 0, 0, 0) >= 3);
    for (i = 0; i < EXI_COUNT; i++) {
        CHECK(doomo_items[doomo_exorcism_items[i]].category == ICAT_EXORCISM);
        CHECK(doomo_exorcism_item_help[i][0] != 0);
    }
}

TEST(exorcism_true_name_puzzle)
{
    int ch[4], c, i, j;
    char buf[80];
    CHECK(premade_possessed(42));
    c = exorcism_name_choices(42, ch);
    CHECK(c >= 0 && c < 4);
    CHECK_EQ(ch[c], sim_possessor_of(42));
    for (i = 0; i < 4; i++) {
        CHECK(ch[i] >= DOOMO_DISTRICT_SIZE && ch[i] < DOOMO_MAX_RESIDENTS);
        for (j = i + 1; j < 4; j++) CHECK(ch[i] != ch[j]);
    }
    for (i = 0; i < 4; i++) {
        exorcism_clue(42, i, buf, sizeof(buf));
        CHECK(strlen(buf) > 4);
    }
    /* the practice dummy works on an empty island too */
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_EMPTY);
    sim_init();
    c = exorcism_name_choices(-1, ch);
    CHECK(c >= 0 && c < 4);
    for (i = 0; i < 4; i++) CHECK(roster_name(ch[i])[0] != 0);
    exorcism_clue(-1, 0, buf, sizeof(buf));
    CHECK(strlen(buf) > 4);
}

TEST(exorcism_success_frees_the_host)
{
    uint32_t won = progress_get(PROG_EXORCISMS_WON);
    DoomoResident *r;
    CHECK(premade_possessed(42));
    r = town_res(42);
    exorcism_apply(42, 1);
    CHECK_EQ(r->status, RES_ALIVE);
    CHECK_EQ(r->stat[STAT_CORRUPTION], 10);
    CHECK_EQ(RES_EXO_ATTEMPTS(r), 1);
    CHECK_EQ(RES_EXO_FAILS(r), 0);
    CHECK_EQ(progress_get(PROG_EXORCISMS_WON), won + 1);
    CHECK_EQ(doomo_town.exorcisms_won, 1);
    CHECK_EQ(sim_possessed_count(), 0);
    CHECK(sim_problem_for(42) < 0 || sim_problem(sim_problem_for(42))->kind != PROB_POSSESSED);
    CHECK(unlock_has(336));                        /* Nun Veil */
    /* applying to someone who isn't possessed does nothing */
    exorcism_apply(42, 0);
    CHECK_EQ(r->status, RES_ALIVE);
    CHECK_EQ(RES_EXO_ATTEMPTS(r), 1);
}

TEST(exorcism_three_failures_lose_the_soul)
{
    DoomoResident *r;
    CHECK(premade_possessed(42));
    r = town_res(42);
    CHECK(!sim_can_lay_to_rest(42));
    exorcism_apply(42, 0);
    CHECK_EQ(r->status, RES_POSSESSED);
    CHECK_EQ(exorcism_tries_left(42), 2);
    CHECK(!sim_can_lay_to_rest(42));
    exorcism_apply(42, 0);
    CHECK_EQ(r->status, RES_POSSESSED);
    CHECK_EQ(exorcism_tries_left(42), 1);
    exorcism_apply(42, 0);
    CHECK_EQ(r->status, RES_SOUL_LOST);
    CHECK_EQ(RES_EXO_FAILS(r), DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL);
    CHECK_EQ(exorcism_tries_left(42), 0);
    CHECK_EQ(progress_get(PROG_EXORCISMS_FAILED), 3);
    CHECK_EQ(sim_possessed_count(), 0);
    CHECK(strstr(sim_news(0), "soul is lost") != NULL);
    /* now, and only now, they may be laid to rest */
    CHECK(sim_can_lay_to_rest(42));
    exorcism_apply(42, 1);                         /* too late for exorcisms */
    CHECK_EQ(r->status, RES_SOUL_LOST);
}

TEST(lay_to_rest_gating_and_grief)
{
    int friend_id = -1, i, happy_before = 0;
    DoomoResident *r;
    CHECK(premade_possessed(42));
    r = town_res(42);
    /* ALIVE citizens can never be laid to rest without Monster Mode */
    CHECK(!sim_can_lay_to_rest(7));
    sim_lay_to_rest(7, DEATH_LAID_TO_REST);
    CHECK_EQ(town_res(7)->status, RES_ALIVE);
    sim_lay_to_rest(42, DEATH_LAID_TO_REST);       /* possessed: not yet */
    CHECK_EQ(r->status, RES_POSSESSED);
    /* a friend to grieve */
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        if ((e->a == 42 || e->b == 42) && e->value >= 30) { friend_id = e->a == 42 ? e->b : e->a; break; }
    }
    if (friend_id < 0) { rel_adjust(42, 3, 80); friend_id = 3; }
    town_res(friend_id)->stat[STAT_HAPPY] = 90;
    happy_before = town_res(friend_id)->stat[STAT_HAPPY];
    for (i = 0; i < 3; i++) exorcism_apply(42, 0);
    CHECK(sim_can_lay_to_rest(42));
    sim_lay_to_rest(42, DEATH_LAID_TO_REST);
    CHECK_EQ(r->status, RES_DEAD);
    CHECK_EQ(r->death_cause, DEATH_LAID_TO_REST);
    CHECK(!(r->flags & RF_MONSTER_KILL));
    CHECK_EQ(doomo_town.grave_count, 1);
    CHECK_EQ(doomo_town.graves[0].resident, 42);
    CHECK(town_res(friend_id)->stat[STAT_HAPPY] < happy_before);
    CHECK_EQ(doomo_town.monster_meter, 0);
    CHECK(unlock_has(354));
    CHECK(strstr(sim_news(0), "Farewell") != NULL);
    /* and a ghost can rise */
    CHECK(sim_raise_ghost(42));
    CHECK_EQ(r->status, RES_GHOST);
    CHECK_EQ(progress_get(PROG_GHOSTS_RAISED), 1);
    CHECK(!sim_raise_ghost(42));
    CHECK(!sim_raise_ghost(5));                    /* not dead */
}

TEST(monster_mode_allows_killing_with_shame)
{
    DoomoResident *r;
    town_new(TOWN_FREEPLAY, 3, TOWN_POP_PREMADE);
    sim_init();
    r = town_res(12);
    CHECK(!sim_can_lay_to_rest(12));
    opt_set(OPT_MONSTER_MODE, 1);
    CHECK(sim_can_lay_to_rest(12));
    CHECK(!sim_can_lay_to_rest(DOOMO_PLAYER_ID));
    rel_adjust(12, 13, 90);
    sim_lay_to_rest(12, DEATH_LAID_TO_REST);
    CHECK_EQ(r->status, RES_DEAD);
    CHECK_EQ(r->death_cause, DEATH_MONSTER);
    CHECK(r->flags & RF_MONSTER_KILL);
    CHECK_EQ(doomo_town.monster_meter, 1);
    CHECK(rel_value(13, DOOMO_PLAYER_ID) < 0);     /* they remember */
    CHECK(unlock_has(355));
    /* story NPCs survive even monsters in story mode */
    town_new(TOWN_STORY, 3, TOWN_POP_PREMADE);
    sim_init();
    town_res(ROSTER_ID_FATHER_FUZZ)->flags |= RF_STORY_NPC;
    CHECK(!sim_can_lay_to_rest(ROSTER_ID_FATHER_FUZZ));
    CHECK(sim_can_lay_to_rest(12));
    opt_set(OPT_MONSTER_MODE, 0);
    CHECK(!sim_can_lay_to_rest(12));
}

TEST(exorcism_scene_practice)
{
    int i;
    town_new(TOWN_FREEPLAY, 8, TOWN_POP_EMPTY);
    sim_init();
    scene_set(&SCENE_EXORCISM, -1);
    for (i = 0; i < 2000 && (scene_top() == &SCENE_EXORCISM || scene_top() == &SCENE_DIALOG ||
                               scene_top() == &SCENE_CONFIRM); i++) {
        uint32_t b = 0;
        if (scene_top() == &SCENE_DIALOG && (i & 3) == 0) b = BTN_A;
        else if (scene_top() == &SCENE_EXORCISM) {
            static const uint32_t seq[] = { BTN_START, BTN_A, BTN_B, BTN_LEFT, BTN_UP, BTN_RIGHT, BTN_DOWN };
            if ((i % 5) == 0) b = seq[(i / 5) % DOOMO_COUNTOF(seq)];
        }
        frames(1, b);
        frames(1, 0);
    }
    /* the ritual ended and handed control back (title / previous scene) */
    CHECK(scene_top() != &SCENE_EXORCISM);
    CHECK_EQ(town_count(-1), 0);                   /* practice touched nobody */
}

TEST(exorcism_scene_real_host_counts_an_attempt)
{
    int i;
    DoomoResident *r;
    CHECK(premade_possessed(42));
    r = town_res(42);
    inv_add(ITEM_ROSARY, 1);
    scene_set(&SCENE_EXORCISM, 42);
    for (i = 0; i < 3000 && RES_EXO_ATTEMPTS(r) == 0; i++) {
        uint32_t b = 0;
        if (scene_top() == &SCENE_DIALOG && (i & 3) == 0) b = BTN_A;
        else if (scene_top() == &SCENE_EXORCISM && (i % 5) == 0) {
            static const uint32_t seq[] = { BTN_START, BTN_A, BTN_RIGHT, BTN_A, BTN_UP, BTN_A };
            b = seq[(i / 5) % DOOMO_COUNTOF(seq)];
        }
        frames(1, b);
        frames(1, 0);
    }
    CHECK_EQ(RES_EXO_ATTEMPTS(r), 1);
    CHECK(r->status == RES_ALIVE || r->status == RES_POSSESSED);
    CHECK(inv_count(ITEM_HOLY_WATER) <= 1);
}

TEST(exorcism_scene_soul_lost_offers_rest)
{
    int i;
    DoomoResident *r;
    CHECK(premade_possessed(42));
    r = town_res(42);
    for (i = 0; i < 3; i++) exorcism_apply(42, 0);
    CHECK_EQ(r->status, RES_SOUL_LOST);
    scene_set(&SCENE_EXORCISM, 42);
    /* the lost-soul moment, then Father Fuzz's offer; accept twice */
    for (i = 0; i < 1200 && r->status == RES_SOUL_LOST; i++) {
        uint32_t b = 0;
        if ((scene_top() == &SCENE_DIALOG || scene_top() == &SCENE_CONFIRM) && (i & 7) == 0) b = BTN_A;
        if (scene_top() == &SCENE_CONFIRM && (i & 7) == 4) b = BTN_LEFT;
        frames(1, b);
        frames(1, 0);
    }
    CHECK_EQ(r->status, RES_DEAD);
    CHECK_EQ(r->death_cause, DEATH_LAID_TO_REST);
}
