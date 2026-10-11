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
    /* (foundation, additive) optional automatic rule: granted as soon as
     * progress_get(prog) >= need. need == 0: granted by code only
     * (unlock_grant / unlock_grant_match). */
    uint8_t     prog;      /* PROG_*                                       */
    uint32_t    need;
} DoomoUnlock;

/* Meaning of (list, index) per category:
 *   UL_CAT_PART         list = CLIST_* (creator lists), index = option
 *   UL_CAT_WEAPON_PART  list = CLIST_W*,  index = part
 *   UL_CAT_PAINT        list = 0, index = paint colour 0..31
 *   UL_CAT_ITEM         list = 0, index = ITEM_* (shop stocks it)
 *   UL_CAT_MODE         list = 0, index = MODE_* (below)
 *   UL_CAT_CHEAT        list = 0, index = OPT_CHEAT_* option (town.h)
 *   UL_CAT_GALLERY      list = GAL_*,  index = entry
 *   UL_CAT_SKIN         list = OPT_* option, index = that option's value
 *                       (window skins, title themes, crosshairs, cursors,
 *                        camera filters, font styles ...)
 *   UL_CAT_PET          list = 0, index = pet form
 *   UL_CAT_MISC         free for the owner
 *
 * Unlock ids are global bit indices. Each team owns a range so tables
 * never collide:
 *   foundation 0..255   creator 256..319   life 320..415
 *   action 416..607     story 608..735     freeplay 736..831
 *   art 832..895        portrait 896..959  roster 960..1023
 *
 * Per-team tables, each terminated by an entry whose name is NULL.
 * stubs.c has weak empty ones; define yours (strong) in your own .c. */
extern const DoomoUnlock doomo_unlocks_foundation[];
extern const DoomoUnlock doomo_unlocks_creator[];
extern const DoomoUnlock doomo_unlocks_life[];
extern const DoomoUnlock doomo_unlocks_action[];
extern const DoomoUnlock doomo_unlocks_story[];
extern const DoomoUnlock doomo_unlocks_freeplay[];
extern const DoomoUnlock doomo_unlocks_art[];
extern const DoomoUnlock doomo_unlocks_portrait[];
extern const DoomoUnlock doomo_unlocks_roster[];
extern const DoomoUnlock doomo_unlocks_minigames[];   /* 832..863 (Fun Fair) */
extern const DoomoUnlock doomo_unlocks_concert[];     /* 896..959 (Concert Hall + Dreams) */
extern const DoomoUnlock doomo_unlocks_studio[];      /* 240..255 (Music Studio) */

/* Game modes gated by UL_CAT_MODE. */
enum { MODE_ARENA, MODE_BOSS_RUSH, MODE_HELLVILE_VISITS, MODE_TAMA_ONLY,
       MODE_MONSTER, MODE_UNITED_PARK, MODE_NEW_GAME_PLUS, MODE_JUKEBOX,
       MODE_GALLERY, MODE_COUNT };
/* Gallery lists for UL_CAT_GALLERY. */
enum { GAL_JUKEBOX, GAL_ART, GAL_DEMONS, GAL_PETS, GAL_CUTSCENES,
       GAL_COUNT };

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
       /* minigames (Fun Fair): best scores, and the in-game day + 1 of the
        * last Wheel of Fate spin (0 = never) */
       PROG_HS_CATCH, PROG_HS_WHACK, PROG_HS_DODGE, PROG_HS_GALLERY,
       PROG_HS_DERBY, PROG_HS_MEMORY, PROG_FAIR_WHEEL_DAY,
       /* concert hall: shows put on; the Hit Parade (2 packed singles each) */
       PROG_CONCERTS, PROG_HIT_SINGLES, PROG_HIT_SINGLES2,
       PROG_COUNT };
void progress_add(int counter, int amount);   /* also checks unlocks */
uint32_t progress_get(int counter);

/* ---- foundation additions ------------------------------------------------ */
/* Unlock entry for (cat, list, index), or NULL. */
const DoomoUnlock *unlock_find(int cat, int list, int index);
const DoomoUnlock *unlock_by_id(int id);
/* 1 if (cat, list, index) has no unlock entry or it is owned. */
int  unlock_ok(int cat, int list, int index);
int  unlock_paint_ok(int paint);
int  unlock_mode_ok(int mode);
/* Grant the entry matching (cat, list, index); 1 if newly granted. */
int  unlock_grant_match(int cat, int list, int index);
/* Re-evaluate every automatic rule (after loading, cheats...). Returns
 * the number of newly granted unlocks. */
int  unlock_check_all(void);
int  unlock_owned_count(void);
void progress_set(int counter, uint32_t value);  /* also checks unlocks */
/* Developer/cheat: own everything (no toasts). */
void unlock_grant_all(void);
extern const char *const doomo_progress_names[PROG_COUNT];
extern const char *const doomo_unlock_cat_names[10];
extern const char *const doomo_mode_names[MODE_COUNT];

#endif
