/* house_api.h - the resident getters and setters story code uses to talk
 * to the house (the only door besides scene_push(&SCENE_HOME, arg)).
 *
 * CONTRACT (docs/design/story/SPEC.md sec.4, C6, C9): written by S0,
 * maintained by H1 (add-only). Implementation: core/src/house_api.c (H1;
 * resident on the PS1, so NOT named home_*.c - SPEC R4). It only reads and
 * writes the HOME chunk's .bss: anything that needs catalog, score or layout
 * code runs in the home group and caches its result in HOME, and these
 * functions serve the cache. Until house_api.c lands every function is a
 * weak stub in core/src/stubs.c.
 */
#ifndef DOOMO_HOUSE_API_H
#define DOOMO_HOUSE_API_H

#include "types.h"
#include "home.h"

int  home_grade(int room);              /* HG_* of HOME_ROOM_* (cached)              */
int  home_stat(int stat);               /* HS_* 0..100 of the active room (cached)    */
int  home_grant(int furn);              /* +1 owned (capped); 0 if full / bad id      */
int  home_has(int furn);                /* owned count 0..15                         */
int  home_place_gift(int furn);         /* 1 placed, 0 sent to storage (no room/space) */
void home_remove_only(int on);          /* the remove-only editor mode                */
int  home_goal_check(int goal);         /* 1 if goal/stamp `goal` is met (cached)     */
int  home_decorate_for(int layout, int tol); /* progress 0..8 (cached)                */
void home_haunt_step(int night);        /* advance the Ch2 haunting                   */
int  home_defend_result(void);          /* guests kept safe in the last defence       */
int  home_rivet_visits(void);
int  home_days_since_fire(void);
void home_set_phase(int phase);         /* HP_*; writes DoomoStry.home_phase          */
int  home_phase(void);                  /* HP_*                                       */
int  home_prop_gag(int gag);            /* queue a "Later" prop gag; 0 if the queue is full */
int  home_moved_in(void);               /* 1 once the player has a home               */
/* (H4 M2, add-only) the Library's photo wing: a saved photo's place (a
 * citizen id 0..999, or 1000 + HOME_ROOM_*), -1 for an empty slot (0..15).
 * The wing re-renders it through SCENE_HOME, HOME_ARG_STORY(HSM_PHOTO) |
 * slot << 8. */
int  home_photo_place(int slot);

#endif
