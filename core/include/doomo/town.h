/* town.h - the whole saved world: 1000 resident slots (500 Hillvile +
 * 500 Hellvile), the relationship
 * graph, the clock, money, inventory, welded weapons, the Doomogachi pet,
 * story progress and options. One DoomoTown == one save slot.
 *
 * Data API (town.c, owner: foundation). Simulation that *changes* the town
 * over time lives in sim.c (owner: life).
 */
#ifndef DOOMO_TOWN_H
#define DOOMO_TOWN_H

#include "types.h"
#include "resident.h"
#include "items.h"
#include "unlocks.h"

/* ---- relationships ------------------------------------------------------ */
enum { REL_ACQUAINTANCE, REL_FRIEND, REL_BEST_FRIEND, REL_DATING,
       REL_MARRIED, REL_RIVAL, REL_NEMESIS, REL_FAMILY, REL_EX,
       REL_KIND_COUNT };
#define REL_KIND_MASK   0x0F
#define REL_CRUSH_AB    0x40   /* a has a crush on b (a < b) */
#define REL_CRUSH_BA    0x80   /* b has a crush on a         */

typedef struct DoomoEdge {
    uint16_t a, b;     /* resident ids, always a < b                 */
    int8_t   value;    /* -100 (hate) .. 100 (adore)                 */
    uint8_t  kind;     /* REL_* | REL_CRUSH_* bits                   */
} DoomoEdge;           /* 6 bytes */

/* ---- welded weapons (weld.h describes the parts) ------------------------ */
typedef struct DoomoWeapon {
    uint8_t frame;     /* WFRAME_*  - the gun/weapon body              */
    uint8_t barrel;    /* WBARREL_* */
    uint8_t core;      /* WCORE_*   - what it shoots                   */
    uint8_t mod;       /* WMOD_*    */
    uint8_t sight;     /* WSIGHT_*  */
    uint8_t stock;     /* WSTOCK_*  */
    uint8_t mag;       /* WMAG_*    */
    uint8_t under;     /* WUNDER_*  - underbarrel attachment           */
    uint8_t charm;     /* WCHARM_*  - dangly cute/brutal charm         */
    uint8_t style;     /* WSTYLE_CUTE / WSTYLE_BRUTAL                  */
    uint8_t paint;     /* main paint colour                            */
    uint8_t accent;    /* accent paint colour                          */
    uint8_t quality;   /* 0..100 from the welding minigame             */
    uint8_t flags;     /* WF_* */
    uint8_t xp;        /* kills level the weapon up (0..255)           */
    uint8_t reserved;
} DoomoWeapon;         /* 16 bytes */
enum { WF_VALID = 1, WF_FAVORITE = 2, WF_STARTER = 4, WF_LEGENDARY = 8 };

/* ---- the Doomogachi (tamagotchi pet, pet.h has the logic) ---------------- */
typedef struct DoomoPet {
    char    name[8];
    uint8_t stage;       /* PET_STAGE_* (egg, baby, child, teen, adult)   */
    uint8_t form;        /* evolution form id (pet.h)                      */
    uint8_t hunger;      /* 0..4 hearts                                    */
    uint8_t joy;         /* 0..4 hearts                                    */
    uint8_t discipline;  /* 0..100                                         */
    uint8_t holiness;    /* 0 = full demon .. 100 = full angel, 50 neutral */
    uint8_t weight;      /* grams-ish 1..99                                */
    uint8_t poop;        /* uncleaned piles 0..4                           */
    uint8_t sick;        /* 0/1, plus skull counter in high bits           */
    uint8_t asleep;      /* 0/1                                            */
    uint8_t mistakes;    /* care mistakes this stage                       */
    uint8_t age_days;
    uint16_t stage_timer;/* minutes until next evolution check             */
    uint16_t care_score; /* accumulated good care                          */
} DoomoPet;              /* 24 bytes */

/* ---- story progress (story.h has the script engine) ---------------------- */
typedef struct DoomoStoryState {
    uint8_t  chapter;        /* 0 = not started, 1..3 demo chapters, 4 = done */
    uint8_t  map;            /* current overworld/dungeon map id               */
    int16_t  x, y;           /* player position in world units (cell*64+..)    */
    uint8_t  facing;         /* 0..3                                           */
    uint8_t  equipped;       /* weapon slot index                              */
    uint8_t  hp, armor;      /* player combat stats 0..200                     */
    uint16_t ammo[8];        /* AMMO_* pools (weld.h)                          */
    uint8_t  quest;          /* active quest id                                */
    uint8_t  quest_step;
    uint16_t kills;
    uint8_t  flags[DOOMO_STORY_FLAGS / 8];
} DoomoStoryState;

/* ---- options --------------------------------------------------------------
 * A big data-driven settings list. Every option is one byte; the options
 * screen is generated from doomo_option_info[]. Append only. */
enum {
    OPT_GORE,            /* GORE_CUTE / GORE_BRUTAL / GORE_PER_WEAPON       */
    OPT_MONSTER_MODE,    /* 1 = kill-off always allowed (with shame)        */
    OPT_DIFFICULTY,      /* Plushie / Normal / Nightmare / Ultra-Hellvile   */
    OPT_MUSIC_VOL, OPT_SFX_VOL, OPT_VOICE_VOL,
    OPT_VOICE_BABBLE,    /* on/off                                          */
    OPT_TEXT_SPEED, OPT_SIM_SPEED, OPT_DAY_LENGTH,
    OPT_AUTOSAVE,
    OPT_CAMERA_DEFAULT,  /* first / third person                            */
    OPT_CONTROL_SCHEME,  /* classic / modern                                */
    OPT_INVERT_LOOK, OPT_LOOK_SPEED, OPT_AIM_ASSIST, OPT_LOCK_ON,
    OPT_CROSSHAIR, OPT_CROSSHAIR_COLOR, OPT_FOV, OPT_HEAD_BOB,
    OPT_SCREEN_SHAKE, OPT_DAMAGE_NUMBERS, OPT_HIT_MARKERS, OPT_HUD_LAYOUT,
    OPT_SHOULDER,        /* third-person shoulder                           */
    OPT_BLOOD_COLOR,     /* red / green / rainbow / confetti                */
    OPT_CONFETTI_AMOUNT,
    OPT_DEMON_DENSITY, OPT_POSSESSION_RATE, OPT_EVENT_RATE,
    OPT_WEATHER, OPT_LCD_FILTER, OPT_WINDOW_SKIN, OPT_FONT_STYLE,
    OPT_SUBTITLES, OPT_COLORBLIND, OPT_REDUCE_FLASH,
    OPT_PET_NOTIFICATIONS, OPT_NEWS_TICKER, OPT_CLOCK_24H,
    OPT_COUNT
};
#define DOOMO_OPT_BYTES 64
typedef struct DoomoOptionInfo {
    const char *name;
    uint8_t     count;            /* number of values                     */
    uint8_t     def;              /* default value                        */
    const char *const *values;    /* value names (NULL = show number)     */
    uint8_t     page;             /* screen page: 0 game,1 shooter,2 sim,
                                     3 audio,4 display,5 accessibility    */
} DoomoOptionInfo;
extern const DoomoOptionInfo doomo_option_info[OPT_COUNT];
int  opt_get(int opt);
void opt_set(int opt, int value);
void opt_defaults(void);

typedef struct DoomoGrave { uint16_t resident; uint16_t day; } DoomoGrave;

enum { TOWN_FREEPLAY = 0, TOWN_STORY = 1 };

typedef struct DoomoTown {
    uint8_t         mode;            /* TOWN_* */
    uint8_t         weekday;         /* 0..6 */
    uint16_t        minute;          /* 0..1439 minute of day */
    uint32_t        day;             /* days since moving in */
    uint32_t        money;           /* Hell-Bucks (H$) */
    uint32_t        seed;            /* town seed (for regeneration)  */
    uint16_t        monster_meter;   /* shame for Monster Mode kills  */
    uint16_t        exorcisms_won;
    uint16_t        edge_count;
    uint16_t        grave_count;
    uint8_t         items[DOOMO_MAX_ITEMS];       /* counts, 0..99 */
    DoomoWeapon     weapons[DOOMO_MAX_WEAPONS];
    DoomoPet        pet;
    DoomoStoryState story;
    uint8_t         opt[DOOMO_OPT_BYTES];               /* OPT_* values */
    uint8_t         unlocks[DOOMO_UNLOCK_BITS / 8];     /* unlocks.h    */
    uint32_t        progress[32];                       /* PROG_* counters (unlocks.h) */
    DoomoGrave      graves[DOOMO_MAX_GRAVES];
    DoomoEdge       edges[DOOMO_MAX_EDGES];
    DoomoResident   res[DOOMO_MAX_RESIDENTS];
    DoomoResident   player;          /* YOU: story protagonist / free play
                                        caretaker avatar (third-person body) */
} DoomoTown;

/* town_res(DOOMO_PLAYER_ID) returns &doomo_town.player. The player can be
 * in relationship edges like anyone else. */
#define DOOMO_PLAYER_ID DOOMO_MAX_RESIDENTS

/* The one live town (~110 KB). Lives in EWRAM on GBA (default .bss). */
extern DoomoTown doomo_town;

/* Lifecycle.
 *   TOWN_POP_EMPTY    - empty island, like the original games: the player
 *                       creates every citizen in the creator
 *   TOWN_POP_PREMADE  - move in all 1000 premade citizens (roster.h)
 *   TOWN_POP_RANDOM   - 1000 random citizens from `seed`
 * Story mode always uses TOWN_POP_PREMADE. */
enum { TOWN_POP_EMPTY, TOWN_POP_PREMADE, TOWN_POP_RANDOM };
void town_new(int mode, uint32_t seed, int population);
DoomoResident *town_res(int id);          /* NULL if out of range; accepts
                                             DOOMO_PLAYER_ID                */
int  town_alloc(int district);            /* free slot id in district or -1 */
void town_free(int id);                   /* slot -> RES_EMPTY, drop edges */
int  town_count(int status);              /* count residents with status (-1 = any non-empty) */
int  town_count_district(int district, int status);
int  town_find_by_name(const char *name); /* -1 if none */

/* Relationship graph (undirected, a != b, order-insensitive). */
DoomoEdge *rel_find(int a, int b);        /* NULL if no edge              */
DoomoEdge *rel_get_or_add(int a, int b);  /* may evict the weakest edge   */
int  rel_value(int a, int b);             /* 0 if none                    */
int  rel_kind(int a, int b);              /* -1 if none                   */
void rel_adjust(int a, int b, int delta); /* clamps, promotes/demotes kind */
void rel_set_kind(int a, int b, int kind);
int  rel_list(int a, uint16_t *out_ids, int max); /* sorted by value desc */
void rel_remove_all(int a);
int  rel_has_crush(int from, int to);
void rel_set_crush(int from, int to, int on);

/* Inventory. */
int  inv_count(int item);
int  inv_add(int item, int n);            /* returns new count (cap 99)   */
int  inv_take(int item, int n);           /* 1 if taken, 0 if not enough  */

/* Graveyard. */
void grave_add(int resident_id);

/* Clock helpers. */
int  town_hour(void);
int  town_is_night(void);                 /* 21:00..05:59 */

#endif
