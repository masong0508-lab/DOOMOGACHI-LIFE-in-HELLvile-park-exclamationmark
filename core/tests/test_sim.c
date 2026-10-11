/* test_sim.c - the life simulation (owner: life).
 *
 * 60 simulated days with all 1000 premade citizens: invariants (stats in
 * range, edges valid, possession bounded), that social life actually
 * happens (friendships, crushes, couples, news), determinism with a
 * seeded RNG, the per-tick cost budget, care reactions, possession and
 * lay-to-rest gating.
 */
#include <time.h>
#include "test.h"

static void fast_clock(void)
{
    opt_set(OPT_DAY_LENGTH, 4);          /* 15 minutes per day           */
    opt_set(OPT_SIM_SPEED, 3);           /* turbo                        */
}

/* Run the sim until `days` more game days have passed. */
static void run_days(int days)
{
    uint32_t end = doomo_town.day + (uint32_t)days;
    while (doomo_town.day < end) sim_tick();
}

static int check_invariants(char *why, int cap)
{
    int i, s;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        const DoomoResident *r = &doomo_town.res[i];
        if (r->status > RES_GHOST) { snprintf(why, cap, "res %d bad status %d", i, r->status); return 0; }
        if (r->status == RES_EMPTY) continue;
        for (s = 0; s < STAT_COUNT; s++)
            if (r->stat[s] > 100) { snprintf(why, cap, "res %d stat %d = %d", i, s, r->stat[s]); return 0; }
        if (r->mood >= MOOD_COUNT) { snprintf(why, cap, "res %d mood %d", i, r->mood); return 0; }
        if (r->activity >= ACT_COUNT) { snprintf(why, cap, "res %d act %d", i, r->activity); return 0; }
        if (r->status == RES_POSSESSED && r->stat[STAT_CORRUPTION] != 100) {
            snprintf(why, cap, "res %d possessed at corruption %d", i, r->stat[STAT_CORRUPTION]);
            return 0;
        }
    }
    if (doomo_town.edge_count > DOOMO_MAX_EDGES) { snprintf(why, cap, "edge_count %d", doomo_town.edge_count); return 0; }
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        if (e->a >= e->b || e->b > DOOMO_PLAYER_ID) { snprintf(why, cap, "edge %d ids %d,%d", i, e->a, e->b); return 0; }
        if ((e->kind & REL_KIND_MASK) >= REL_KIND_COUNT) { snprintf(why, cap, "edge %d kind", i); return 0; }
        if (e->value < -100 || e->value > 100) { snprintf(why, cap, "edge %d value", i); return 0; }
        if (i > 0) {
            const DoomoEdge *p = &doomo_town.edges[i - 1];
            if (((uint32_t)p->a << 16 | p->b) >= ((uint32_t)e->a << 16 | e->b)) {
                snprintf(why, cap, "edges unsorted at %d", i); return 0;
            }
        }
    }
    for (i = 0; i < sim_problem_count(); i++) {
        const DoomoProblem *p = sim_problem(i);
        if (!p || p->kind <= PROB_NONE || p->kind >= PROB_COUNT || p->resident >= DOOMO_MAX_RESIDENTS) {
            snprintf(why, cap, "problem %d bad", i); return 0;
        }
    }
    for (i = 0; i < sim_news_count(); i++)
        if (strlen(sim_news(i)) >= NEWS_LEN || !sim_news(i)[0]) { snprintf(why, cap, "news %d", i); return 0; }
    return 1;
}

static int count_kind(int kind)
{
    int i, n = 0;
    for (i = 0; i < doomo_town.edge_count; i++)
        if ((doomo_town.edges[i].kind & REL_KIND_MASK) == kind) n++;
    return n;
}

TEST(sim_sixty_days_premade)
{
    char why[160];
    int day, friends0, married0, dating0, maxposs = 0;
    uint32_t prog_days0;
    clock_t t0;
    const DoomoSimStats *st;
    town_new(TOWN_FREEPLAY, 4242, TOWN_POP_PREMADE);
    fast_clock();
    sim_init();
    sim_seed(777);
    friends0 = count_kind(REL_FRIEND) + count_kind(REL_BEST_FRIEND);
    married0 = count_kind(REL_MARRIED);
    dating0 = count_kind(REL_DATING);
    prog_days0 = progress_get(PROG_DAYS_LIVED);
    t0 = clock();
    for (day = 0; day < 60; day++) {
        run_days(1);
        CHECK_MSG(check_invariants(why, sizeof(why)), "day %d: %s", day, why);
        if (sim_possessed_count() > maxposs) maxposs = sim_possessed_count();
        CHECK_MSG(sim_possessed_count() <= sim_possession_cap(),
                  "day %d: %d possessed > cap %d", day, sim_possessed_count(), sim_possession_cap());
        CHECK_EQ(sim_possessed_count(), town_count(RES_POSSESSED));
    }
    st = sim_stats();
    {
        int hist[11] = { 0 }, i, hap = 0, n = 0, act[ACT_COUNT] = { 0 }, avg[STAT_COUNT] = { 0 }, k;
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
            const DoomoResident *r = &doomo_town.res[i];
            if (r->status != RES_ALIVE) continue;
            hist[r->stat[STAT_CORRUPTION] / 10]++;
            hap += r->stat[STAT_HAPPY]; n++;
            act[r->activity]++;
            for (k = 0; k < STAT_COUNT; k++) avg[k] += r->stat[k];
        }
        printf("    avg stats:");
        for (k = 0; k < STAT_COUNT; k++) printf(" %d", n ? avg[k] / n : 0);
        printf("\n");
        printf("    corruption histogram:");
        for (i = 0; i < 11; i++) printf(" %d", hist[i]);
        printf("  avg happy %d  acts:", n ? hap / n : 0);
        for (i = 0; i < ACT_COUNT; i++) printf(" %d", act[i]);
        printf("\n");
    }
    printf("    60 days in %.2fs: chats %u meetings %u forgotten %u args %u crushes %u confess %u dates %u rej %u "
           "proposals %u weddings %u breakups %u babies %u possessions %u nightmares %u "
           "auto-exorcisms %u news %u problems %u\n",
           (double)(clock() - t0) / CLOCKS_PER_SEC,
           st->chats, st->meetings, st->forgotten, st->arguments, st->crushes, st->confessions, st->dates, st->rejections,
           st->proposals, st->marriages, st->breakups, st->babies, st->possessions,
           st->nightmares, st->exorcisms_auto, st->news, st->problems);
    printf("    edges %d (friends %d->%d, dating %d->%d, married %d->%d, rivals %d, nemeses %d, exes %d)"
           " possessed now %d max %d cap %d, residents %d, max ops/tick %u\n",
           doomo_town.edge_count, friends0, count_kind(REL_FRIEND) + count_kind(REL_BEST_FRIEND),
           dating0, count_kind(REL_DATING), married0, count_kind(REL_MARRIED),
           count_kind(REL_RIVAL), count_kind(REL_NEMESIS), count_kind(REL_EX),
           sim_possessed_count(), maxposs, sim_possession_cap(), town_count(-1), sim_max_tick_ops());
    CHECK_EQ(progress_get(PROG_DAYS_LIVED) - prog_days0, 60);
    CHECK(st->meetings > 100);
    CHECK(st->crushes > 5);
    CHECK(st->dates > 2);
    CHECK(st->news > 50);
    CHECK(st->problems > 20);
    CHECK(progress_get(PROG_FRIENDSHIPS) > 10);
    CHECK(sim_news_count() == NEWS_MAX);
    CHECK(sim_max_tick_ops() <= SIM_BATCH * 2 + SIM_EDGE_BATCH * 2);
}

/* FNV-1a over everything the sim changes (residents, edges, clock). */
static uint32_t town_hash(void)
{
    uint32_t h = 2166136261u;
    const uint8_t *p;
    size_t i, n;
    p = (const uint8_t *)doomo_town.res;
    n = sizeof(doomo_town.res);
    for (i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    p = (const uint8_t *)doomo_town.edges;
    n = (size_t)doomo_town.edge_count * sizeof(DoomoEdge);
    for (i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    h = (h ^ doomo_town.day) * 16777619u;
    h = (h ^ doomo_town.minute) * 16777619u;
    return h;
}

static uint32_t run_seeded(uint32_t seed, int days)
{
    town_new(TOWN_FREEPLAY, 2024, TOWN_POP_PREMADE);
    fast_clock();
    sim_init();
    sim_seed(seed);
    run_days(days);
    return town_hash() ^ (uint32_t)sim_news_count() ^ (uint32_t)sim_problem_count() * 31u;
}

TEST(sim_deterministic_with_seed)
{
    uint32_t a = run_seeded(55, 4), b = run_seeded(55, 4), c = run_seeded(56, 4);
    CHECK_EQ_U(a, b);
    CHECK(a != c);
}

TEST(sim_clock_follows_options)
{
    int i;
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);
    sim_init();
    doomo_town.minute = 600;
    /* 1 hour per day at normal speed: one game minute every 75 ticks */
    opt_set(OPT_DAY_LENGTH, 2);
    opt_set(OPT_SIM_SPEED, 1);
    for (i = 0; i < 750; i++) sim_tick();
    CHECK_EQ(doomo_town.minute, 610);
    /* turbo, 15 minute days: 16x faster */
    opt_set(OPT_DAY_LENGTH, 4);
    opt_set(OPT_SIM_SPEED, 3);
    for (i = 0; i < 750; i++) sim_tick();
    CHECK(doomo_town.minute >= 610 + 158 && doomo_town.minute <= 610 + 161);
    /* fast-forward over midnight */
    doomo_town.minute = 1430;
    i = (int)doomo_town.day;
    sim_advance_minutes(20);
    CHECK_EQ(doomo_town.minute, 10);
    CHECK_EQ(doomo_town.day, (uint32_t)i + 1);
    CHECK(town_is_night());
    {
        int m, d;
        doomo_town.day = 0;
        sim_date(&m, &d);
        CHECK_EQ(m, 3);                              /* April */
        CHECK_EQ(d, 1);
        doomo_town.day = 28 * 9 + 4;
        sim_date(&m, &d);
        CHECK_EQ(m, 0);
        CHECK_EQ(d, 5);
    }
}

TEST(sim_empty_island_ticks)
{
    int i;
    town_new(TOWN_FREEPLAY, 77, TOWN_POP_EMPTY);
    fast_clock();
    sim_init();
    for (i = 0; i < 3000; i++) sim_tick();
    run_days(1);
    CHECK_EQ(town_count(-1), 0);
    CHECK_EQ(sim_possessed_count(), 0);
    CHECK(sim_news_count() > 0);                     /* the ticker still chats */
}

TEST(sim_random_island_ten_days)
{
    char why[160];
    town_new(TOWN_FREEPLAY, 31337, TOWN_POP_RANDOM);
    fast_clock();
    sim_init();
    run_days(10);
    CHECK_MSG(check_invariants(why, sizeof(why)), "%s", why);
    CHECK(sim_stats()->chats > 1000);
    CHECK(sim_possessed_count() <= sim_possession_cap());
}

TEST(sim_possession_rate_off_means_none)
{
    town_new(TOWN_FREEPLAY, 4242, TOWN_POP_PREMADE);
    fast_clock();
    opt_set(OPT_POSSESSION_RATE, 0);
    sim_init();
    run_days(10);
    CHECK_EQ(sim_possession_cap(), 0);
    CHECK_EQ(town_count(RES_POSSESSED), 0);
    CHECK_EQ(sim_stats()->possessions, 0);
    /* story chapter 1 is possession-free too */
    town_new(TOWN_STORY, 4242, TOWN_POP_PREMADE);
    doomo_town.story.chapter = 1;
    opt_set(OPT_POSSESSION_RATE, 3);
    sim_init();
    run_days(5);
    CHECK_EQ(town_count(RES_POSSESSED), 0);
}

TEST(sim_care_reactions_depend_on_taste)
{
    DoomoResident *r;
    const char *s;
    char fav[96], hate[96];
    int fav_item, hate_item, h0;
    town_new(TOWN_FREEPLAY, 9, TOWN_POP_PREMADE);
    sim_init();
    r = town_res(10);
    r->like_food = 0;                                /* Cupcakes */
    r->hate_food = 6;                                /* Hell Peppers */
    fav_item = ITEM_CUPCAKE;
    hate_item = ITEM_HELL_PEPPERS;
    r->stat[STAT_HUNGER] = 40;
    r->stat[STAT_HAPPY] = 50;
    inv_add(fav_item, 2);
    inv_add(hate_item, 1);
    s = sim_feed(10, fav_item);
    snprintf(fav, sizeof(fav), "%s", s);
    CHECK(strstr(fav, "Cupcake") != NULL);
    CHECK_EQ(r->mood, MOOD_ECSTATIC);
    CHECK(r->stat[STAT_HAPPY] >= 80);
    h0 = r->stat[STAT_HAPPY];
    r->stat[STAT_HUNGER] = 40;
    s = sim_feed(10, hate_item);
    snprintf(hate, sizeof(hate), "%s", s);
    CHECK(strcmp(fav, hate) != 0);
    CHECK(r->stat[STAT_HAPPY] < h0);
    CHECK_EQ(inv_count(hate_item), 0);
    s = sim_feed(10, hate_item);                     /* none left */
    CHECK(strstr(s, "don't have") != NULL);
    s = sim_feed(10, ITEM_SOAP);
    CHECK(strstr(s, "can't eat") != NULL);
    CHECK(progress_get(PROG_MEALS_SERVED) >= 2);
    /* everything answers with something sensible */
    CHECK(sim_clean(10)[0]);
    CHECK(sim_play(10)[0]);
    CHECK(sim_talk(10)[0]);
    CHECK(sim_comfort(10)[0]);
    inv_add(ITEM_MEDICINE, 1);
    CHECK(sim_heal(10, ITEM_MEDICINE)[0]);
    inv_add(ITEM_FLOWERS, 1);
    CHECK(sim_gift(10, ITEM_FLOWERS)[0]);
    CHECK(progress_get(PROG_GIFTS_GIVEN) >= 1);
    CHECK(r->level > 0);
    /* the dead don't eat */
    town_res(11)->status = RES_DEAD;
    CHECK(strstr(sim_feed(11, ITEM_CUPCAKE), "silence") != NULL);
    /* the possessed answer with the demon's voice */
    sim_corrupt(12, 100);
    CHECK(sim_talk(12)[0] == '"' || sim_talk(12)[0] == '*');
}

TEST(sim_problems_resolve_and_pay)
{
    int idx;
    uint32_t money;
    DoomoResident *r;
    char buf[64];
    town_new(TOWN_FREEPLAY, 9, TOWN_POP_PREMADE);
    sim_init();
    r = town_res(20);
    r->stat[STAT_HUNGER] = 10;
    idx = sim_problem_raise(20, PROB_HUNGRY, -1, 0);
    CHECK(idx >= 0);
    CHECK_EQ(sim_problem_for(20), idx);
    CHECK(strstr(sim_problem_text(idx, buf, sizeof(buf)), "starving") != NULL);
    CHECK(sim_problem_raise(20, PROB_DIRTY, -1, 0) < 0);   /* one at a time */
    money = doomo_town.money;
    sim_problem_resolve(idx);
    CHECK(r->stat[STAT_HUNGER] >= 50);
    CHECK(doomo_town.money > money);
    CHECK(sim_problem_for(20) < 0);
    /* wanting an item you don't own stays open */
    idx = sim_problem_raise(21, PROB_WANTS_ITEM, -1, ITEM_VIDEO_GAME);
    sim_problem_resolve(idx);
    CHECK(sim_problem_for(21) >= 0);
    inv_add(ITEM_VIDEO_GAME, 1);
    sim_problem_resolve(sim_problem_for(21));
    CHECK(sim_problem_for(21) < 0);
    CHECK_EQ(inv_count(ITEM_VIDEO_GAME), 0);
    /* a fight the player patches up */
    rel_adjust(22, 23, -40);
    idx = sim_problem_raise(22, PROB_FIGHT, 23, 0);
    sim_problem_resolve(idx);
    CHECK(rel_value(22, 23) > -40);
    /* problems expire with a small sting */
    idx = sim_problem_raise(24, PROB_BORED, -1, 0);
    CHECK(idx >= 0);
    sim_advance_minutes(721);
    CHECK(sim_problem_for(24) < 0 || sim_problem(sim_problem_for(24))->kind != PROB_BORED);
}

TEST(sim_romance_rules)
{
    DoomoResident *a, *b;
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_RANDOM);
    sim_init();
    a = town_res(1);
    b = town_res(2);
    a->status = b->status = RES_ALIVE;
    a->romance = ROMANCE_ANY;
    b->pronoun = PRONOUN_SHE;
    CHECK(sim_romance_ok(1, 2));
    a->romance = ROMANCE_HE;
    CHECK(!sim_romance_ok(1, 2));
    b->pronoun = PRONOUN_HE;
    CHECK(sim_romance_ok(1, 2));
    a->romance = ROMANCE_THEY;
    b->pronoun = PRONOUN_XE;
    CHECK(sim_romance_ok(1, 2));
    a->romance = ROMANCE_NONE;
    CHECK(!sim_romance_ok(1, 2));
    a->romance = ROMANCE_ANY;
    rel_set_kind(1, 2, REL_FAMILY);
    CHECK(!sim_romance_ok(1, 2));                    /* never family */
    /* kids never, teens only with teens (premade traits) */
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_PREMADE);
    sim_init();
    {
        int kid = -1, adult = -1, i;
        for (i = 0; i < DOOMO_DISTRICT_SIZE && (kid < 0 || adult < 0); i++) {
            int t = roster_traits(i);
            if ((t & ROSTER_T_KID) && kid < 0) kid = i;
            if (!(t & (ROSTER_T_KID | ROSTER_T_TEEN)) && adult < 0 && town_res(i)->romance == ROMANCE_ANY) adult = i;
        }
        if (kid >= 0 && adult >= 0) {
            CHECK(!sim_romance_ok(adult, kid));
            CHECK(!sim_romance_ok(kid, adult));
        }
    }
}

TEST(sim_confession_dating_wedding)
{
    DoomoResident *a, *b;
    int ok = 0, tries;
    town_new(TOWN_FREEPLAY, 3, TOWN_POP_RANDOM);
    sim_init();
    a = town_res(30);
    b = town_res(31);
    rel_remove_all(30);
    rel_remove_all(31);
    sim_init();
    a->romance = b->romance = ROMANCE_ANY;
    rel_adjust(30, 31, 90);
    rel_set_crush(31, 30, 1);
    for (tries = 0; tries < 20 && !ok; tries++) ok = sim_confess(30, 31, 40);
    CHECK(ok);
    CHECK_EQ(rel_kind(30, 31), REL_DATING);
    CHECK_EQ(sim_partner_of(30), 31);
    CHECK_EQ(sim_partner_of(31), 30);
    CHECK(sim_stats()->dates >= 1);
    rel_adjust(30, 31, 100);
    for (tries = 0, ok = 0; tries < 20 && !ok && rel_kind(30, 31) == REL_DATING; tries++)
        ok = sim_propose(30, 31, 40);
    CHECK(ok);
    CHECK_EQ(rel_kind(30, 31), REL_MARRIED);
    CHECK(progress_get(PROG_MARRIAGES) >= 1);
    CHECK(unlock_has(341));                          /* Heart Eyes */
    /* and a breakup makes exes */
    sim_breakup(30, 31, 0);
    CHECK_EQ(rel_kind(30, 31), REL_EX);
    CHECK_EQ(sim_partner_of(30), -1);
    /* player advice: discouraging a crush ends it */
    rel_adjust(32, 33, 60);
    rel_set_crush(32, 33, 1);
    sim_advise_confess(32, 33, 0);
    CHECK(!rel_has_crush(32, 33));
}

TEST(sim_babies_blend_their_parents)
{
    int baby, i;
    DoomoResident *a, *b, *k;
    town_new(TOWN_FREEPLAY, 11, TOWN_POP_RANDOM);
    sim_init();
    /* make room: a free slot in Hillvile */
    town_free(400);
    a = town_res(40);
    b = town_res(41);
    rel_set_kind(40, 41, REL_MARRIED);
    rel_adjust(40, 41, 90);
    baby = sim_make_baby(40, 41);
    CHECK_EQ(baby, 400);
    k = town_res(baby);
    CHECK_EQ(k->status, RES_ALIVE);
    CHECK(k->look.species == a->look.species || k->look.species == b->look.species);
    CHECK(k->look.skin == a->look.skin || k->look.skin == b->look.skin);
    CHECK_EQ(k->look.body, BODY_TINY);
    CHECK_EQ(k->romance, ROMANCE_NONE);
    CHECK_EQ(rel_kind(40, baby), REL_FAMILY);
    CHECK_EQ(rel_kind(41, baby), REL_FAMILY);
    CHECK_EQ(progress_get(PROG_BABIES), 1);
    CHECK(strstr(sim_news(0), k->name) != NULL);
    CHECK_EQ(resident_sanitize(k), 0);
    /* a full island has no room for babies */
    town_new(TOWN_FREEPLAY, 11, TOWN_POP_RANDOM);
    sim_init();
    CHECK_EQ(sim_make_baby(40, 41), -1);
    /* across the wall the more devilish parent picks the district */
    town_free(600);
    for (i = 0; i < DOOMO_DISTRICT_SIZE; i++) if (town_res(i)->status == RES_EMPTY) break;
    town_res(5)->stat[STAT_CORRUPTION] = 0;
    town_res(5)->flags &= (uint8_t)~RF_HELLBORN;
    town_res(700)->stat[STAT_CORRUPTION] = 60;
    baby = sim_make_baby(5, 700);
    CHECK_EQ(baby, 600);
    CHECK(town_res(baby)->flags & RF_HELLBORN);
}

TEST(sim_per_tick_budget)
{
    int i;
    clock_t t0;
    double us;
    town_new(TOWN_FREEPLAY, 4242, TOWN_POP_PREMADE);
    fast_clock();
    sim_init();
    t0 = clock();
    for (i = 0; i < 20000; i++) sim_tick();
    us = (double)(clock() - t0) * 1e6 / CLOCKS_PER_SEC / 20000.0;
    printf("    sim_tick: %.2f us/tick native, max %u ops/tick (budget %d residents + %d edges)\n",
           us, sim_max_tick_ops(), SIM_BATCH, SIM_EDGE_BATCH);
    CHECK(sim_max_tick_ops() <= SIM_BATCH + SIM_EDGE_BATCH + 40);
    CHECK(us < 200.0);
}

TEST(sim_unlock_table_is_well_formed)
{
    int i, n = 0;
    for (i = 0; doomo_unlocks_life[i].name; i++, n++) {
        const DoomoUnlock *u = &doomo_unlocks_life[i];
        CHECK_MSG(u->id >= 320 && u->id <= 415, "life unlock %s has id %d", u->name, u->id);
        CHECK(u->how && u->how[0]);
        if (u->cat == UL_CAT_PET) CHECK(u->index < PET_FORM_COUNT);
        CHECK(unlock_by_id(u->id) == u);
    }
    CHECK(n >= 50);
    /* days lived unlock the party hat */
    progress_set(PROG_DAYS_LIVED, 2);
    CHECK(unlock_has(345));
}
