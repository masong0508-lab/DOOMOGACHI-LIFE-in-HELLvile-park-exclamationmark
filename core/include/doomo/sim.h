/* sim.h - the life simulation (owner: life). Runs all 500 residents
 * cheaply: residents are updated in round-robin batches each tick so the
 * cost per tick is bounded on GBA. */
#ifndef DOOMO_SIM_H
#define DOOMO_SIM_H

#include "types.h"

/* Moods (DoomoResident.mood). */
enum { MOOD_OK, MOOD_HAPPY, MOOD_ECSTATIC, MOOD_SAD, MOOD_ANGRY, MOOD_LOVESICK,
       MOOD_HUNGRY, MOOD_SLEEPY, MOOD_SICK, MOOD_FILTHY, MOOD_LONELY,
       MOOD_SCARED, MOOD_CORRUPTED, MOOD_POSSESSED, MOOD_COUNT };
/* Activities / locations (DoomoResident.activity). */
enum { ACT_HOME, ACT_SLEEPING, ACT_PARK, ACT_CAFE, ACT_SHOP, ACT_CHURCH,
       ACT_WORKSHOP, ACT_BEACH, ACT_CONCERT, ACT_VAPE_SHOP, ACT_HOSPITAL,
       ACT_WANDERING, ACT_HAUNTING, ACT_COUNT };
/* Death causes (DoomoResident.death_cause). */
enum { DEATH_NONE, DEATH_LAID_TO_REST, DEATH_MONSTER, DEATH_DEMON_ATTACK,
       DEATH_NEGLECT, DEATH_OLD_AGE, DEATH_COUNT };

extern const char *const doomo_mood_names[MOOD_COUNT];
extern const char *const doomo_activity_names[ACT_COUNT];
extern const char *const doomo_death_names[DEATH_COUNT];

/* "Problems" residents raise for the player to solve (thought bubbles). */
enum { PROB_NONE, PROB_HUNGRY, PROB_DIRTY, PROB_BORED, PROB_SICK,
       PROB_WANTS_FRIEND, PROB_FIGHT, PROB_CRUSH, PROB_CONFESS_HELP,
       PROB_BREAKUP, PROB_WANTS_ITEM, PROB_NIGHTMARE, PROB_CORRUPTION,
       PROB_POSSESSED, PROB_PROPOSAL, PROB_LONELY, PROB_COUNT };

typedef struct DoomoProblem {
    uint16_t resident;
    uint16_t other;      /* other resident involved or 0xFFFF */
    uint8_t  kind;       /* PROB_* */
    uint8_t  item;       /* wanted item */
    uint16_t age;        /* minutes since raised */
} DoomoProblem;

/* Town news ticker entries ("Gus and Mo are now best friends!"). */
#define NEWS_MAX 32
#define NEWS_LEN 64

void sim_init(void);                 /* after town_new / load            */
void sim_tick(void);                 /* advance clock + batch of residents */
void sim_advance_minutes(int minutes); /* fast-forward (sleeping, travel)  */

/* Care actions the player performs on a resident (returns a short
 * reaction string for the UI). */
const char *sim_feed(int id, int item);
const char *sim_gift(int id, int item);
const char *sim_clean(int id);
const char *sim_play(int id);
const char *sim_talk(int id);
const char *sim_heal(int id, int item);
const char *sim_comfort(int id);
/* Social interventions. */
const char *sim_introduce(int a, int b);
const char *sim_advise_confess(int id, int target, int encourage);
const char *sim_make_up(int a, int b);

/* Problems queue. */
int  sim_problem_count(void);
const DoomoProblem *sim_problem(int index);
void sim_problem_resolve(int index);
int  sim_problem_for(int resident_id);   /* index or -1 */

/* News feed (newest first). */
int  sim_news_count(void);
const char *sim_news(int index);
void sim_news_push(const char *msg);

/* Possession / death rules. */
void sim_corrupt(int id, int amount);    /* possession at 100              */
int  sim_can_lay_to_rest(int id);        /* SOUL_LOST, or monster mode     */
void sim_lay_to_rest(int id, int cause); /* graveyard, grief for friends   */
int  sim_raise_ghost(int id);            /* graveyard rite -> RES_GHOST    */

int  sim_mood_of(int id);                /* recompute + return MOOD_*      */

/* ======================================================================
 * life additions (owner: life; additive)
 * ==================================================================== */

/* Who drives the sim: free play / story scenes call sim_init() once after
 * town_new() or load_game(), then sim_tick() once per frame while the
 * world should be alive (sim_tick never draws or plays sounds). The clock
 * follows OPT_DAY_LENGTH (a game day lasts 24 h / 2 h / 1 h / 30 min /
 * 15 min of real time) times OPT_SIM_SPEED (x0.5, x1, x2, x4). Night is
 * 21:00..05:59 (town_is_night). pet_tick_minute() runs once per game
 * minute from inside the sim.
 *
 * Cost: one sim_tick() updates at most SIM_BATCH residents (round robin,
 * ~32 ticks per full pass over the 1000 slots) and at most SIM_EDGE_BATCH
 * relationship edges (about one pass over the graph per game hour), all
 * cheap integer math. Never loops over all residents per tick. */
#define SIM_BATCH       32
#define SIM_EDGE_BATCH  32

/* Care actions (sim_feed, sim_gift, sim_heal) take the item from the
 * inventory themselves (inv_take; OPT_CHEAT_INFINITE_CANDY skips that)
 * and answer "You don't have any ..." without effect when it is missing.
 * The reaction strings live in a small rotating static buffer: copy them
 * if you keep them across two calls. */

/* The named Hellvile citizen (id 500..999) possessing a POSSESSED or
 * SOUL_LOST resident, or -1. Deterministic for a town seed, the host and
 * how many exorcisms the host has been through (a new possession may be
 * a different demon). Prefers a living Hellvile neighbour; on an island
 * without any it still names a premade Hellvile citizen (roster). */
int  sim_possessor_of(int id);
/* Display name of that possessor ("Grimgramps"), "" if none. */
const char *sim_possessor_name(int id);

/* Calendar: 12 months of 28 days (birthdays use the same calendar); day 0
 * of a new island is the 1st of April. */
void sim_date(int *month, int *day_of_month);
/* Today's concert genre (MUSIC_*): residents who like it flock there. */
int  sim_concert_genre(void);

/* Problems: a one-line description for thought bubbles / lists, e.g.
 * "Gus is starving!" (returns buf). */
const char *sim_problem_text(int index, char *buf, int cap);
extern const char *const doomo_problem_names[PROB_COUNT];
/* Reaction line of the last sim_problem_resolve() ("Thank you!"). A
 * PROB_WANTS_ITEM without the item in the inventory, and a PROB_POSSESSED
 * whose host is still possessed (push SCENE_EXORCISM), stay open. */
const char *sim_last_reaction(void);
/* Raise a problem for a resident (story/tests); returns its index or -1.
 * One problem per resident; POSSESSED replaces anything else. */
int  sim_problem_raise(int resident, int kind, int other, int item);

/* Social helpers (also used by the UI to show hearts / warnings). */
int  sim_compat(int a, int b);            /* -6..+6 personality + likes   */
int  sim_romance_ok(int from, int to);    /* from could fall for to       */
int  sim_partner_of(int id);              /* dating / married partner, -1 */
/* Ask a dating resident to propose to their partner right now (player
 * advice); returns the reaction. */
const char *sim_advise_propose(int id, int encourage);
/* Talk a resident through a rocky relationship (PROB_BREAKUP advice):
 * encourage = 1 "work it out", 0 "let them go" (breaks up). */
const char *sim_advise_breakup(int id, int encourage);

/* Possession bookkeeping. */
int  sim_possessed_count(void);           /* POSSESSED residents now     */
int  sim_possession_cap(void);            /* max at once (OPT_POSSESSION_RATE) */
/* Exorcism success/failure hook (exorcism_apply calls it): news, problem
 * queue, counters. */
void sim_on_exorcism(int id, int success);

/* Births: a married couple's baby in a free slot of the right district,
 * look blended from both parents. Returns the new id or -1 (no room).
 * The sim calls it (rarely) by itself; story / tests may force one. */
int  sim_make_baby(int parent_a, int parent_b);

/* Fast path for tests / tools: reseed the sim's private RNG (it is
 * otherwise seeded from the town seed by sim_init). */
void sim_seed(uint32_t seed);
/* Work done by the last sim_tick(): resident updates + edge visits +
 * relationship lookups (for the per-tick cost budget). */
uint32_t sim_last_tick_ops(void);
uint32_t sim_max_tick_ops(void);          /* worst tick since sim_init   */

/* Running totals since sim_init (tests, stats screen). */
typedef struct DoomoSimStats {
    uint32_t chats, meetings, forgotten, arguments, crushes, confessions, dates, rejections,
             proposals, marriages, breakups, babies, possessions,
             nightmares, exorcisms_auto, news, problems, deaths;
} DoomoSimStats;
const DoomoSimStats *sim_stats(void);

#endif
