/* unlocks.h - an immense amount of unlockables (owner: foundation for the
 * engine; every team registers its own unlockables in the table).
 *
 * Anything can be locked: creator parts (any catalog list index), paint
 * colours, weapon frames/parts, items in the shop, pet forms, game modes
 * (Arena, Hellvile visits, Monster Mode, Boss Rush, Tamagotchi-only mode),
 * cheats (big-head mode, confetti everything, infinite candy, cute demons,
 * brutal residents, low-gravity, paper-mode), gallery/jukebox entries,
 * title-screen themes, UI window skins, crosshairs, camera filters.
 * Most options start unlocked so the creator is huge from minute one; the
 * rest are earned by playing: story progress, friendships, exorcisms,
 * kills per weapon type, pet evolutions, citizens created, days lived,
 * secrets found, marriages, ghost raisings, etc.
 *
 * Unlock state is saved in DoomoTown.unlocks (1024 bits).
 */
#ifndef DOOMO_UNLOCKS_H
#define DOOMO_UNLOCKS_H

#include "types.h"

#define DOOMO_UNLOCK_BITS 1024

/* What an unlock gates. For UL_CAT_PART, `list` is the catalog list id
 * (CLIST_*) and `index` the option within it. */
enum { UL_CAT_PART, UL_CAT_PAINT, UL_CAT_WEAPON_PART, UL_CAT_ITEM,
       UL_CAT_MODE, UL_CAT_CHEAT, UL_CAT_GALLERY, UL_CAT_SKIN,
       UL_CAT_PET, UL_CAT_MISC };

/* Catalog list ids for UL_CAT_PART (one per enum list in catalog.h). */
enum { CLIST_SPECIES, CLIST_BODY, CLIST_HEAD, CLIST_EYE, CLIST_BROW,
       CLIST_NOSE, CLIST_MOUTH, CLIST_EARS, CLIST_HAIR, CLIST_FHAIR,
       CLIST_HORNS, CLIST_TAIL, CLIST_WINGS, CLIST_HALO, CLIST_MARK,
       CLIST_TOP, CLIST_BOTTOM, CLIST_SHOES, CLIST_HAT, CLIST_ACC,
       CLIST_AURA, CLIST_PATTERN,
       /* weapon part lists for UL_CAT_WEAPON_PART */
       CLIST_WFRAME, CLIST_WBARREL, CLIST_WCORE, CLIST_WMOD, CLIST_WSIGHT,
       CLIST_WSTOCK, CLIST_WMAG, CLIST_WUNDER, CLIST_WCHARM,
       CLIST_COUNT };

typedef struct DoomoUnlock {
    uint16_t    id;        /* bit index 0..1023, unique                    */
    uint8_t     cat;       /* UL_CAT_*                                     */
    uint8_t     list;      /* CLIST_* for parts, else 0                    */
    uint16_t    index;     /* option index / mode id / cheat id            */
    const char *name;      /* "Flame Hair"                                 */
    const char *how;       /* "Exorcise 5 citizens"                        */
} DoomoUnlock;

/* The registry (unlocks.c concatenates per-team tables). */
extern const DoomoUnlock *const doomo_unlock_tables[];
int  unlock_count(void);
const DoomoUnlock *unlock_at(int i);

int  unlock_has(int id);
/* Grants and queues a "NEW UNLOCK!" toast. Returns 1 if newly granted. */
int  unlock_grant(int id);
/* Is this creator/weapon option available? (1 if it has no unlock entry
 * or its unlock is owned). Creator & workshop UIs show locked options as
 * silhouettes with the `how` hint. */
int  unlock_option_ok(int list, int index);
/* Progress counters (PROG_*) that unlock conditions read (saved). */
enum { PROG_CITIZENS_CREATED, PROG_EXORCISMS_WON, PROG_EXORCISMS_FAILED,
       PROG_DEMONS_KILLED, PROG_DEMONS_KILLED_CUTE, PROG_DEMONS_KILLED_BRUTAL,
       PROG_WEAPONS_WELDED, PROG_DAYS_LIVED, PROG_FRIENDSHIPS, PROG_MARRIAGES,
       PROG_BABIES, PROG_GHOSTS_RAISED, PROG_PET_EVOLUTIONS, PROG_GIFTS_GIVEN,
       PROG_MEALS_SERVED, PROG_SECRETS_FOUND, PROG_HEADSHOTS,
       PROG_CROSS_BORDER_FRIENDS, PROG_MONEY_EARNED, PROG_ARENA_WAVES,
       PROG_COUNT };
void progress_add(int counter, int amount);   /* also checks unlocks */
uint32_t progress_get(int counter);

#endif
