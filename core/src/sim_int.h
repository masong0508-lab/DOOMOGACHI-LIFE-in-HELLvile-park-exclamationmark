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
#define SIM_BITS ((DOOMO_MAX_RESIDENTS + 7) / 8)
#define SIM_MAX_ELAPSED 720        /* minutes one resident update may cover */
#define SIM_MIN_ELAPSED 15         /* a resident is updated at most every 15
                                      game minutes (keeps turbo speed cheap) */

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
    uint32_t  edge_credit;               /* owed edge visits x 60          */
    uint16_t  stamp[DOOMO_MAX_RESIDENTS];/* last update, absolute minute   */
    uint8_t   transient[DOOMO_MAX_RESIDENTS];
    uint8_t   job[DOOMO_MAX_RESIDENTS];  /* 0 unknown, 0xFF none, else venue+1 */
    uint8_t   job_budget;                /* job lookups left this tick     */
    uint8_t   taken[SIM_BITS], crush[SIM_BITS];       /* current pass     */
    uint8_t   taken_nx[SIM_BITS], crush_nx[SIM_BITS]; /* being rebuilt    */
    uint16_t  ring[ACT_COUNT][SIM_RING];
    uint8_t   ring_pos[ACT_COUNT];
    DoomoProblem prob[SIM_MAX_PROBLEMS];
    uint8_t   nprob;
    char      news[NEWS_MAX][NEWS_LEN];
    uint8_t   news_head, news_n;
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
int  sim_is_kid(int id);
int  sim_is_teen(int id);
int  sim_alive_ok(const DoomoResident *r); /* ALIVE or GHOST (can socialise) */
void sim_set_transient(int id, int kind, int hours);
int  sim_transient(int id);              /* TR_* or TR_NONE                */
int  sim_is_venue(int act);              /* social venue (not home/sleep)  */
int  bit_get(const uint8_t *b, int i);
void bit_set(uint8_t *b, int i, int on);
void sim_ensure_init(void);
const char *sim_name(int id);            /* resident name or "Someone"     */
const char *sim_venue_name(int act);     /* "the park"                     */

/* ---- sim_social.c ---- */
void sim_social_init(void);              /* rebuild taken / crush bitsets  */
void sim_edges_step(int max_visits);
void sim_venue_meet(int id, DoomoResident *r, int minutes);
void sim_taken_update(int id);
void sim_edge_remove(int index);         /* keeps the list sorted          */

/* ---- sim_events.c ---- */
void sim_events_reset(void);
void sim_problems_minute(void);          /* age + expire                   */
void sim_check_problems(int id, DoomoResident *r);
void sim_problem_drop_for(int id);       /* forget every problem of id     */
int  sim_problem_find(int id, int kind); /* index or -1                    */
void sim_problem_remove(int index);
void sim_newsf(const char *fmt, ...);
void sim_news_fluff(void);               /* ambient town headline          */
void sim_news_daily(void);

/* ---- sim_soul.c ---- */
void sim_soul_update(int id, DoomoResident *r, int minutes);
void sim_corrupt_natural(int id, int amount);
void sim_possess(int id);
void sim_soul_daily(void);
void sim_recount_possessed(void);

#endif
