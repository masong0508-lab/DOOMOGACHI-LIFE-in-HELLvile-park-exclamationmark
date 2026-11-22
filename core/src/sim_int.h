/* sim_int.h - private glue between the life-sim files (owner: life).
 * Not part of the public contract (see doomo/sim.h).
 *
 *   sim.c         clock, round-robin resident updates, needs, activities,
 *                 moods, the per-tick budget
 *   sim_social.c  relationship edges: meetings, friendships, arguments,
 *                 crushes, confessions, dating, proposals, weddings,
 *                 breakups, exes, rivals, nemeses, babies
 *   sim_events.c  problems queue (thought bubbles) + news ticker headlines
 *   sim_care.c    the player's care actions and their reaction strings
 *   sim_soul.c    corruption, night possession, possessors, lay to rest,
 *                 ghosts, grief
 */
#ifndef DOOMO_SIM_INT_H
#define DOOMO_SIM_INT_H

#include "doomo/doomo.h"

#define SIM_MAX_PROBLEMS 24
#define SIM_RING 8                 /* recent visitors remembered per venue */
#define SIM_BITS ((DOOMO_LIVE_RESIDENTS + 7) / 8)
/* (GBA-500) the per-resident arrays below are by slot (town.h): SIM_IX(id) is the
 * index of id's entries, -1 for an id with no slot (only where not all 1000 live) */
#define SIM_IX(id) town_slot(id)
#define SIM_MAX_ELAPSED 720        /* minutes one resident update may cover */
#define SIM_MIN_ELAPSED 15         /* a fast-forward chunk updates a resident
                                      only when 15 game minutes have passed */
#define SIM_INTERVAL    15         /* S3-1: game minutes in which every resident is visited once    */
#define SIM_PASS_MIN    8          /* ... and a visit of the pass lives at least this many of them  */
/* S3-1: the resident pass is spread over the whole interval. The cursor
 * moves at a steady DOOMO_MAX_RESIDENTS slots per SIM_INTERVAL game minutes
 * (about 0.9 slots a tick at the default clock, 14.2 at the fastest), so
 * every resident is updated once per SIM_INTERVAL and no tick does a
 * town's worth of work. A tick earns SIM_PASS_NUM * speed credit, a slot
 * costs SIM_PASS_DEN * ticks_per_minute_q (200 / 3 = 1000 / 15). The first
 * pass after sim_init, a load or a fast-forward finds every stamp at "now":
 * a visit that would live fewer than SIM_PASS_MIN minutes leaves the stamp
 * alone and the next one lives them all, so no minute is lost or counted
 * twice. (GBA-500: the cursor walks the DOOMO_LIVE_RESIDENTS slots, so the
 * GBA's pass moves 500 slots per SIM_INTERVAL, about 0.45 a tick: every
 * resident is still visited once per interval.) */
#define SIM_PASS_NUM    (DOOMO_LIVE_RESIDENTS / 5)
#define SIM_PASS_DEN    (SIM_INTERVAL / 5)
DOOMO_STATIC_ASSERT(SIM_PASS_NUM * 5 == DOOMO_LIVE_RESIDENTS && SIM_PASS_DEN * 5 == SIM_INTERVAL,
                    sim_pass_rate_is_exact);

/* Short-lived feelings stored per resident (hi nibble = TR_*, lo nibble =
 * game hours left). */
enum { TR_NONE, TR_ANGRY, TR_SAD, TR_LOVESICK, TR_ECSTATIC, TR_SCARED,
       TR_GRIEF };

typedef struct SimState {
    DoomoRng  rng;
    uint8_t   inited;
    uint8_t   town_pop, town_mode;           /* the town we were set up for   */
    uint32_t  town_seed, day_seen;
    uint16_t  acc;                       /* clock accumulator (1/4 ticks)  */
    uint16_t  cursor;                    /* resident round robin           */
    uint16_t  edge_cursor;               /* edge round robin               */
    uint16_t  pass_credit;               /* S3-1: owed resident slots x SIM_PASS_DEN x
                                            ticks_per_minute_q (in the padding
                                            before edge_credit: no new RAM) */
    uint32_t  edge_credit;               /* owed edge visits x 60          */
    uint16_t  stamp[DOOMO_LIVE_RESIDENTS];/* last update, absolute minute  */
    uint8_t   transient[DOOMO_LIVE_RESIDENTS];
    uint8_t   job[DOOMO_LIVE_RESIDENTS]; /* 0 unknown, 0xFF none, else venue+1 */
    uint8_t   job_budget;                /* job lookups left this tick     */
    uint8_t   taken[SIM_BITS], crush[SIM_BITS];       /* current pass     */
    uint8_t   taken_nx[SIM_BITS], crush_nx[SIM_BITS]; /* being rebuilt    */
    uint16_t  ring[ACT_COUNT][SIM_RING];
    uint8_t   ring_pos[ACT_COUNT];
    DoomoProblem prob[SIM_MAX_PROBLEMS];
    uint8_t   nprob;
    /* LS1: the news feed moved out (sim_events.c news_s, 8 B records) */
    int16_t   possessed;                 /* POSSESSED residents right now  */
    uint8_t   new_edges;                 /* new edges made this tick       */
    uint8_t   rough_day;                 /* rough-patch news today         */
    uint8_t   night;                     /* town_is_night() cached         */
    uint8_t   args_hour;                 /* argument news this hour        */
    uint32_t  ops, max_ops;
    DoomoSimStats stats;
} SimState;

extern SimState sim_s;

/* ---- sim.c ---- */
uint32_t sim_now(void);                  /* absolute game minute           */
int  sim_rand(int n);                    /* 0..n-1 from the sim RNG        */
int  sim_chance(int percent);
uint32_t sim_roll(uint32_t den);           /* 0..den-1 (den may exceed 65536) */
/* Stochastic integer share: round(rate * minutes / 60) without bias. */
int  sim_scaled(int rate_per_hour, int minutes);
void sim_stat_add(DoomoResident *r, int stat, int delta);
int  sim_traits(int id);                 /* ROSTER_T_* of premade citizens */
uint32_t sim_udiv_test(uint32_t q, uint32_t div);   /* X4: sim.c's exact constant divisions (tests) */
int  sim_div100_test(int x);
int  sim_is_kid(int id);
int  sim_is_teen(int id);
int  sim_alive_ok(const DoomoResident *r); /* ALIVE or GHOST (can socialise) */
void sim_set_transient(int id, int kind, int hours);
int  sim_transient(int id);              /* TR_* or TR_NONE                */
int  sim_is_venue(int act);              /* social venue (not home/sleep)  */
int  sim_venue_social_bias(int id, int venue); /* bounded live relationship influence on routine choice */
int  bit_get(const uint8_t *b, int i);
void bit_set(uint8_t *b, int i, int on);
void sim_ensure_init(void);
extern const uint8_t doomo_hobby_venue[];       /* where a hobby is practised (ACT_*), HOBBY_COUNT entries (sim.c) */
const char *sim_name(int id);            /* resident name or "Someone"     */

/* ---- sim_social.c ---- */
void sim_social_init(void);              /* rebuild taken / crush bitsets  */
void sim_edges_step(int max_visits);
void sim_venue_meet(int id, DoomoResident *r, int minutes);
void sim_taken_update(int id);

/* ---- sim_events.c ---- */
void sim_events_reset(void);
void sim_problems_minute(void);          /* age + expire                   */
void sim_check_problems(int id, DoomoResident *r);
void sim_problem_drop_for(int id);       /* forget every problem of id     */
int  sim_problem_find(int id, int kind); /* index or -1                    */
void sim_problem_remove(int index);
void sim_problem_complete(int index);   /* The player helped. -1 if the action already removed it. */
void sim_news_welcome(int pop);          /* sim_init's greeting (LS1)     */
void sim_news_fluff(void);               /* ambient town headline          */
void sim_news_daily(void);

/* ---- sim_soul.c ---- */
void sim_soul_update(int id, DoomoResident *r, int minutes);
void sim_corrupt_natural(int id, int amount);
void sim_possess(int id);
void sim_soul_daily(void);
void sim_recount_possessed(void);
const char *sim_possessor_name_by(int p);  /* p from sim_possessor_of     */

#endif
