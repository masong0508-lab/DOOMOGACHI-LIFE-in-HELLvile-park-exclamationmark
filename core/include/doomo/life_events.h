/* life_events.h - LS3, the life event engine (owner: life; roadmap W5).
 *
 * Once a town day (after the household's own daily passes, sim.c day_rollover) a small
 * chance fires ONE life event from a fixed table: a potluck, a picnic, a surprise party,
 * an anniversary, a lucky find, a fan letter, a rainy day, a rotten day, a talent show,
 * stargazing, and (LS4) a bedtime story, homework help, an elder's old stories, a pet's new
 * trick, a movie night and a bake-off. Each event has a weight, a cooldown (in town days) and a condition on the
 * household; the effect is mood / bond nudges, a little money or an item, and a news line.
 * Kid-safe: words only, nothing here can hurt or remove a member (that is F6 / F10).
 *
 * Nothing rolls without a household (no RNG draw, so a town without a family plays exactly
 * as before), in a STORY island before the story is cleared, or while it is "Subconscious"
 * free will for the news line (the effect still happens, silently).
 *
 * Saved: LIFE_SEC_EVENTS = [total u16 LE][cooldown u8 x events]; add-only (new events append
 * to the table; an older save's missing bytes read as 0). Omitted while nothing has fired.
 * Story scripts fire an event by id with CB_LIFE_EVENT (story_int.h). */
#ifndef DOOMO_LIFE_EVENTS_H
#define DOOMO_LIFE_EVENTS_H

#include "types.h"

enum { LEV_POTLUCK = 0, LEV_PICNIC, LEV_SURPRISE_PARTY, LEV_ANNIVERSARY, LEV_LUCKY_FIND,
       LEV_LETTER, LEV_RAINY_DAY, LEV_BAD_DAY, LEV_TALENT_SHOW, LEV_STARGAZING,
       /* LS4 (2026-11-04) */
       LEV_BEDTIME_STORY, LEV_HOMEWORK_HELP, LEV_OLD_STORIES, LEV_PET_TRICK,
       LEV_MOVIE_NIGHT, LEV_BAKE_OFF,
       LEV_COUNT };                       /* append-only: saved cooldowns are by index */
#define LEV_DAILY_PCT 30                  /* chance per day that one event is tried     */

/* 1 if the household and the cooldown allow event ev right now. */
int  life_event_can(int ev);
/* Fire event ev now (a script, the daily pass, tests): 0 if it cannot run, else 1.
 * Sets its cooldown, bumps the total, shows the news line unless free will is Subconscious. */
int  life_event_fire(int ev);
/* The daily pass: tick the cooldowns, then maybe fire one event. Returns the event id or -1. */
int  life_events_daily(void);
int  life_event_cooldown(int ev);         /* town days until ev may run again, 0 = ready */
int  life_event_total(void);              /* events fired in this save (saturates 65535)  */
int  life_event_last(void);               /* the last one fired this session, -1 = none   */

uint32_t life_events_save_size(void);
uint32_t life_events_save(uint8_t *dst, uint32_t cap);
int      life_events_load(const uint8_t *src, uint32_t len);
void     life_events_clear(void);

#endif
