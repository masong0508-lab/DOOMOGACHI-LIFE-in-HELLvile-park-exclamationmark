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

#endif
