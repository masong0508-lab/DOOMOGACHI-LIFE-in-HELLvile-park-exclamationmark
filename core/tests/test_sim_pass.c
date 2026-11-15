/* test_sim_pass.c - S3-1 (speed audit 3): the resident pass of the sim is spread over its 15 game minutes.
 *
 * Before: the round robin took 32 slots a tick and a resident was updated once its stamp was 15 game minutes
 * old. Every stamp is set to the same minute (sim_init, a load, a fast-forward), so all 1,000 residents came
 * due in the same 32 ticks, every 15 game minutes, forever: 10-15 ms of extra work in each of those ticks on
 * the GBA (17 frames of 50-67 ms on every free-play screen, every 37.7 s).
 * Now the cursor earns slots with the clock (1,000 per SIM_INTERVAL game minutes), so a resident is visited,
 * and updated, once per interval and a tick does a few updates at most.
 *
 * The tests watch sim_s.stamp: a visit that updates a resident moves its stamp, and the move is the number of
 * game minutes the update lived (the "el" of update_resident_ex). */
#include "test.h"
#include "../src/sim_int.h"

/* the cursor and sim_s.stamp walk the slots (GBA-500: 500 with LIVE=500; the slot is the id where all 1000 live) */
#define NRES DOOMO_LIVE_RESIDENTS
#define DEFAULT_PER 300         /* OPT_DAY_LENGTH 2 (ticks_per_minute_q) and OPT_SIM_SPEED 1 (speed_q 4):   */
#define DEFAULT_SPD 4           /* one game minute every 75 ticks, the interval 1,125 ticks (37.5 s)        */

typedef struct {
    uint16_t prev[NRES];        /* the stamp last seen */
    uint16_t n[NRES];           /* updates seen since the watch started (measured ones only) */
    uint16_t first_el[NRES];    /* the minutes the first update covered, 0 = none yet */
    int max_visits, max_updates, max_ops;
    int el_min, el_max;         /* over the measured updates */
    int measure;                /* 1 while the updates count towards n / el_min / el_max */
} Watch;

static Watch w;

static void watch_start(void)
{
    memset(&w, 0, sizeof(w));
    memcpy(w.prev, sim_s.stamp, sizeof(w.prev));
    w.el_min = 1 << 20;
}

/* One sim_tick(), then: how many slots the cursor moved over, how many of them were updated. */
static void watch_tick(void)
{
    int c0 = sim_s.cursor, visits, k, upd = 0;
    sim_tick();
    visits = (sim_s.cursor + NRES - c0) % NRES;
    for (k = 0; k < visits; k++) {
        int id = (c0 + k) % NRES, el;
        if (sim_s.stamp[id] == w.prev[id]) continue;
        el = (uint16_t)(sim_s.stamp[id] - w.prev[id]);
        w.prev[id] = sim_s.stamp[id];
        upd++;
        if (!w.first_el[id]) w.first_el[id] = (uint16_t)el;
        if (w.measure) {
            w.n[id]++;
            if (el < w.el_min) w.el_min = el;
            if (el > w.el_max) w.el_max = el;
        }
    }
    if (visits > w.max_visits) w.max_visits = visits;
    if (upd > w.max_updates) w.max_updates = upd;
    if ((int)sim_last_tick_ops() > w.max_ops) w.max_ops = (int)sim_last_tick_ops();
}

static int live(int s)            /* by slot, like the cursor */
{
    return doomo_town.res[s].status != RES_EMPTY && doomo_town.res[s].status != RES_DEAD;
}

/* ticks of one SIM_INTERVAL at a clock (ticks_per_minute_q and speed_q of sim.c) */
static int interval_ticks(int per, int spd) { return (SIM_INTERVAL * per + spd - 1) / spd; }

static void set_clock(int day_len, int speed)
{
    opt_set(OPT_DAY_LENGTH, day_len);
    opt_set(OPT_SIM_SPEED, speed);
}

static void premade_town(uint32_t seed)
{
    town_new(TOWN_FREEPLAY, seed, TOWN_POP_PREMADE);
    sim_init();
    sim_seed(seed * 3u + 1u);
}

/* Every live resident is updated once per interval, evenly: after the first two passes every update lives
 * 14..16 game minutes, no tick does more than one update at the default clock, and over K intervals each
 * resident got K updates (the visit is 1,125 ticks = 15.0 minutes apart: K - 1 .. K + 1 for the minute
 * rounding of the clock). */
TEST(sim_pass_default_clock_once_per_interval_one_update_a_tick)
{
    const int T = interval_ticks(DEFAULT_PER, DEFAULT_SPD), K = 6;
    int i, bad = 0, minmax = 0;
    CHECK_EQ(T, 1125);
    premade_town(4242);
    CHECK_EQ(opt_get(OPT_DAY_LENGTH), 2);                 /* the defaults are the clock these numbers are for */
    CHECK_EQ(opt_get(OPT_SIM_SPEED), 1);
    watch_start();
    for (i = 0; i < 2 * T + 40; i++) watch_tick();         /* the first two passes (the stamps start equal) */
    w.measure = 1;
    memset(w.n, 0, sizeof(w.n));
    for (i = 0; i < K * T; i++) watch_tick();
    for (i = 0; i < NRES; i++) {
        if (!live(i)) continue;
        if (w.n[i] + 1 < K || w.n[i] > K + 1) { if (!bad) minmax = i; bad++; }
    }
    CHECK_MSG(bad == 0, "%d residents were not updated once per interval (first: %d got %d updates in %d intervals)",
              bad, minmax, w.n[minmax], K);
    CHECK_MSG(w.el_min >= SIM_INTERVAL - 1 && w.el_max <= SIM_INTERVAL + 1, "updates lived %d..%d minutes, not %d",
              w.el_min, w.el_max, SIM_INTERVAL);
    CHECK_MSG(w.max_visits <= 1 && w.max_updates <= 1, "a tick visited %d and updated %d residents (the pass is "
              "0.89 a tick at this clock)", w.max_visits, w.max_updates);
    CHECK_MSG(w.max_ops <= 1 + SIM_EDGE_BATCH + 40, "a tick cost %d ops", w.max_ops);
}

/* The first passes after a load (sim_init: every stamp is the same minute, the cursor is at 0). The burst of
 * the old code is the whole point: at tick 1,125 it updated every resident in 32 ticks. No tick may do more than
 * one update, nobody is updated twice in the first interval, nobody waits longer than 1.5 intervals and the
 * minutes a visit found too few (under SIM_PASS_MIN: the first passes) are lived by the next one. */
TEST(sim_pass_after_a_load_has_no_burst_and_loses_nothing)
{
    const int T = interval_ticks(DEFAULT_PER, DEFAULT_SPD);
    int i, id, late = 0, early = 0, never = 0;
    static uint8_t image[256 * 1024];
    uint32_t n;
    premade_town(77);
    watch_start();
    for (i = 0; i < 700; i++) watch_tick();                /* mid pass: the cursor is somewhere, stamps differ */
    CHECK(sim_s.cursor != 0);
    /* a save, more play, a load: the sim is set up again from the loaded town (the stamps are not saved) */
    n = save_serialize(image, sizeof(image));
    CHECK(n > 0);
    for (i = 0; i < 300; i++) sim_tick();
    CHECK(save_deserialize(image, n));
    sim_init();
    CHECK_EQ(sim_s.cursor, 0);
    watch_start();
    for (id = 1; id < NRES; id++) CHECK_EQ(sim_s.stamp[id], sim_s.stamp[0]);   /* the old shape: all the same */
    for (i = 0; i < 3 * T; i++) {
        watch_tick();
        if (i == T - 1) {                                   /* one interval after the load */
            for (id = 0; id < NRES; id++)
                if (live(id) && w.first_el[id] && w.first_el[id] < SIM_PASS_MIN) early++;
        }
    }
    CHECK_MSG(w.max_updates <= 1 && w.max_visits <= 1, "burst after the load: %d updates in a tick", w.max_updates);
    for (id = 0; id < NRES; id++) {
        if (!live(id)) continue;
        if (!w.first_el[id]) never++;
        else if (w.first_el[id] > SIM_INTERVAL + SIM_INTERVAL / 2 + 1) late++;
    }
    CHECK_MSG(never == 0, "%d residents were not updated in three intervals after a load", never);
    CHECK_MSG(late == 0, "%d residents first lived more than 1.5 intervals at once", late);
    CHECK_EQ(early, 0);                                     /* an update never lives fewer than SIM_PASS_MIN minutes */
}

/* A fast-forward (sleep, travel) sets every stamp to "now" again: the passes after it are spread too. */
TEST(sim_pass_after_a_fast_forward_has_no_burst)
{
    const int T = interval_ticks(DEFAULT_PER, DEFAULT_SPD);
    int i;
    premade_town(9);
    for (i = 0; i < 400; i++) sim_tick();
    sim_advance_minutes(8 * 60);
    watch_start();
    for (i = 0; i < 3 * T; i++) watch_tick();
    CHECK_MSG(w.max_updates <= 1 && w.max_visits <= 1, "burst after the fast-forward: %d updates in a tick",
              w.max_updates);
}

/* Every clock the options offer (5 day lengths x 4 speeds): the pass keeps its rate (a resident every 15 game
 * minutes whatever the ticks per minute) and a tick's budget is bounded. */
TEST(sim_pass_every_clock_and_speed)
{
    static const int per[5] = { 7200, 600, 300, 150, 75 };
    static const int spd[4] = { 2, 4, 8, 16 };
    int d, s, i;
    for (d = 0; d < 5; d++) {
        for (s = 0; s < 4; s++) {
            const int T = interval_ticks(per[d], spd[s]);
            int bad = 0, cap = (NRES * spd[s] + SIM_INTERVAL * per[d] - 1) / (SIM_INTERVAL * per[d]);   /* slots a tick, rounded up */
            if (d == 0 && s != 3) continue;                 /* the 2 h day: 54,000 / 27,000 / 13,500 ticks an interval, its fastest speed is enough */
            town_new(TOWN_FREEPLAY, 100u + (uint32_t)(d * 4 + s), TOWN_POP_PREMADE);
            set_clock(d, s);
            sim_init();
            sim_seed(5);
            watch_start();
            for (i = 0; i < 2 * T + 20; i++) watch_tick();
            w.measure = 1;
            for (i = 0; i < T + T / 2; i++) watch_tick();
            for (i = 0; i < NRES; i++)
                if (live(i) && w.n[i] != 1 && w.n[i] != 2) bad++;
            CHECK_MSG(bad == 0, "day length %d speed %d: %d residents updated 0 or 3+ times in 1.5 intervals", d, s, bad);
            CHECK_MSG(w.el_min >= SIM_INTERVAL - 1 && w.el_max <= SIM_INTERVAL + 1,
                      "day length %d speed %d: updates lived %d..%d minutes", d, s, w.el_min, w.el_max);
            CHECK_MSG(w.max_visits <= cap && w.max_visits <= SIM_BATCH, "day length %d speed %d: %d slots in a tick "
                      "(rate %d, cap %d)", d, s, w.max_visits, cap, SIM_BATCH);
            CHECK_MSG(w.max_ops <= SIM_BATCH + SIM_EDGE_BATCH + 40, "day length %d speed %d: %d ops in a tick", d, s,
                      w.max_ops);
        }
    }
}

/* The day length is changed between two ticks (the Options page): the credit the slow clock left is in its units,
 * and a tick still never visits more than SIM_BATCH residents. */
TEST(sim_pass_day_length_change_does_not_burst)
{
    int i;
    premade_town(61);
    set_clock(0, 0);                 /* 7,200 / 2: a slot costs 21,600, a tick earns 400 */
    for (i = 0; i < 53; i++) sim_tick();                    /* 21,200 of credit, no slot yet */
    CHECK_EQ(sim_s.pass_credit, 53 * 2 * SIM_PASS_NUM);     /* (LIVE=500: 10,600) */
    set_clock(4, 3);                 /* 75 / 16: a slot costs 225, a tick earns 3,200 */
    watch_start();
    for (i = 0; i < 4; i++) watch_tick();
    CHECK_MSG(w.max_visits <= SIM_BATCH && w.max_updates <= SIM_BATCH, "%d slots in a tick after the change", w.max_visits);
    CHECK(w.max_visits > 1);                                /* the credit was paid out, at the cap */
}

/* The same seed gives the same town (the order of the updates changed once, S3-1, not their determinism). */
TEST(sim_pass_is_deterministic)
{
    uint32_t h[2];
    int run, i, k;
    for (run = 0; run < 2; run++) {
        uint32_t x = 2166136261u;
        premade_town(31);
        for (i = 0; i < 3000; i++) sim_tick();
        for (k = 0; k < NRES; k++) x = (x ^ doomo_town.res[k].stat[STAT_HAPPY]) * 16777619u ^ doomo_town.res[k].activity;
        h[run] = x ^ (uint32_t)doomo_town.edge_count * 2654435761u;
    }
    CHECK_EQ_U(h[0], h[1]);
}
