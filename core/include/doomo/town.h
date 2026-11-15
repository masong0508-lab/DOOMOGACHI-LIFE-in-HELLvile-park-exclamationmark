/* town.h - the whole saved world: 1000 resident slots (500 Hillvile +
 * 500 Hellvile), the relationship
 * graph, the clock, money, inventory, welded weapons, the Boreagotchi pet,
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
DOOMO_STATIC_ASSERT(sizeof(DoomoEdge) == 6, edge_is_three_halfwords);   /* town.c moves them by halfwords */

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

/* ---- the Boreagotchi (tamagotchi pet, pet.h has the logic) ---------------- */
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
    OPT_CONTROL_SCHEME,  /* CONTROLS_*: classic / auto / dual stick / modern */
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
    /* ---- foundation additions (append only) ---- */
    OPT_TITLE_THEME,     /* title screen theme (unlockable values)          */
    OPT_CURSOR,          /* menu cursor: heart / paw / star / skull ...     */
    OPT_CAMERA_FILTER,   /* none / storybook / VHS / gameboy / sepia ...    */
    OPT_UI_BOUNCE,       /* UI animation amount: off / subtle / boingy      */
    /* cheats (page 6; values beyond 0 gated by UL_CAT_CHEAT unlocks) */
    OPT_CHEAT_BIG_HEAD, OPT_CHEAT_CONFETTI, OPT_CHEAT_INFINITE_CANDY,
    OPT_CHEAT_CUTE_DEMONS, OPT_CHEAT_BRUTAL_RESIDENTS, OPT_CHEAT_LOW_GRAV,
    OPT_CHEAT_PAPER_MODE, OPT_CHEAT_GOD_MODE, OPT_CHEAT_RICH,
    OPT_CHEAT_DISCO, OPT_CHEAT_TINY_DEMONS, OPT_CHEAT_ALL_GHOSTS,
    OPT_COMBAT_FEEL,     /* classic / punchy / brutal (PS1 only)            */
    OPT_TURN_SWAP,       /* modern scheme: 0 R1 turns right, 1 R1 turns left */
    OPT_EFFECTS,         /* effects detail: 0 low (fewer, shorter gibs) / 1 normal */
    OPT_DRAW_DIST,       /* PS1 3D view distance cap: 0 near / 1 normal     */
    OPT_SHOW_FPS,        /* 1 = frames-drawn counter in the shooter         */
    OPT_LANGUAGE,        /* a LANG_* id (lang.h, core/lang/languages.tsv; shown first on the Game page) */
    OPT_3D_FAST,         /* (G7, SP8) Combat detail: OPT3D_BALANCED / _SMOOTH / _MAX below
                            (every platform; G7's 0 Better / 1 Faster read as Balanced / Smooth) */
    OPT_COUNT
};
DOOMO_STATIC_ASSERT(OPT_COUNT <= 64, options_fit);
/* (SP8) OPT_3D_FAST's values, the Options "Combat detail" row (Performance page; DECISIONS
 * 2026-11-08). Logic never depends on it: only what is drawn, and how often.
 *   BALANCED (default)  the platform's best steady pace; the GBA drops its 3D view to half
 *                       resolution while full-resolution frames would show below 20 fps
 *                       (G-C1), the PS1 caps the affine subdivision of far and small quads
 *   SMOOTH              the speed path: the GBA's half-resolution view at up to 30 fps
 *                       (G7's "Faster"), the PS1's tighter subdivision cap; billboard drop
 *                       shadows only under the player (combat_view_detail)
 *   MAX                 full resolution and every effect; the platform's governor lowers the
 *                       detail only for the frames that would otherwise fall below its pace
 * The values keep G7's saved bytes meaning something close: 0 (Better) -> Balanced, 1
 * (Faster) -> Smooth. */
enum { OPT3D_BALANCED = 0, OPT3D_SMOOTH = 1, OPT3D_MAX = 2 };
/* (U0) the options with ids 64 and up. They do not live in doomo_town.opt (its 64 B are full);
 * core/src/options_store.c keeps them in the 'OPT2' save chunk and, for the machine-wide ones,
 * in the 48 B machine config record. opt_get / opt_set / opt_defaults take these ids too.
 * Add-only: a new option goes at the end, before OPT2_END. */
#define OPT2_FIRST 64
enum {
    OPT_MATURE = OPT2_FIRST,  /* Off / Censored / On / On + pack (ending.h MATURE_*); machine-wide */
    OPT_LOSS,                 /* family loss (F6): 0 off, 1 on; family_loss_set() follows          */
    OPT_RUMBLE,               /* controller rumble, apart from Screen shake: 0 off, 1 on; machine-wide */
    OPT_TIPS,                 /* (CL4) first-time tip cards (tips.h): 0 off, 1 on; machine-wide */
    OPT_STICK_MODE,           /* (STKOPT) Dreamcast one-stick: 0 Turn (stick X turns, triggers strafe), 1 Strafe (X strafes, triggers turn); saved with the world */
    OPT_SLIDER_RANGE,         /* (CR-SLIDERX) cheat "Slider range": 0 x1, 1 x2, 2 x3, 3 x4 (portrait.h look_slider_range); saved with the world, an old save reads x1 */
#if !defined(DOOMO_PLATFORM_PS1)    /* the PS1 lists no dark level yet and has no resident byte to spare: it has neither id */
    OPT_NOTES,                /* (LV11) content notes: 0 every time, 1 first time only; machine-wide */
    OPT_MEMORIAL,             /* (LV11) show the town's dead by name on the Vault's memorial wall: 0 off, 1 on; machine-wide */
#endif
    OPT2_END
};
enum { STICK_TURN = 0, STICK_STRAFE = 1 };
#if defined(DOOMO_PLATFORM_PS1)     /* (LV11) code shared with the PS1 still names them: no such option there (opt_get answers 0) */
#define OPT_NOTES    OPT2_END
#define OPT_MEMORIAL OPT2_END
#endif
#define OPT2_COUNT (OPT2_END - OPT2_FIRST)
/* Option pages (DoomoOptionInfo.page). */
/* (U2) OPTPAGE_QUICK is the front page: callers that pass 0 land on it. */
enum { OPTPAGE_QUICK, OPTPAGE_GAME, OPTPAGE_CONTROLS, OPTPAGE_SHOOTER, OPTPAGE_SIM, OPTPAGE_AUDIO,
       OPTPAGE_DISPLAY, OPTPAGE_PERF, OPTPAGE_ACCESS, OPTPAGE_CHEATS, OPTPAGE_COUNT };
/* Opens the paged Options screen on top of the current scene (pause menus
 * use it: the scene below stays frozen; B comes back). */
void options_open(int page);
extern const char *const doomo_option_page_names[OPTPAGE_COUNT];
/* Value constants for a few options other modules branch on. */
enum { GORE_CUTE, GORE_BRUTAL, GORE_PER_WEAPON, GORE_MIXED };   /* MIXED: unlocked by finishing the story */
enum { DIFF_PLUSHIE, DIFF_NORMAL, DIFF_NIGHTMARE, DIFF_ULTRA_HELLVILE };
/* (ST1) the stored values of OPT_CONTROL_SCHEME. 1 was Modern, the old default: it now reads
 * as Auto (Dual stick while an analog pad is plugged in, Modern otherwise), so old saves
 * migrate with no version bump; an explicit Modern pick is stored as 3. */
enum { CONTROLS_CLASSIC, CONTROLS_AUTO, CONTROLS_DUAL_STICK, CONTROLS_MODERN };
/* The scheme in effect now: CLASSIC, MODERN or DUAL_STICK (Auto resolved against the pad in
 * the player's hands, live; Dual stick without an analog pad plays as Modern). */
int controls_scheme(void);
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
    DoomoEdge       edges[DOOMO_LIVE_EDGES];   /* (GBA-500: 1024 on the GBA) */
    DoomoResident   res[DOOMO_LIVE_RESIDENTS];  /* by slot (GBA-500; == id where all 1000 live) */
    DoomoResident   player;          /* YOU: story protagonist / free play
                                        caretaker avatar (third-person body) */
    /* ---- foundation additions ---- */
    uint8_t         population;      /* TOWN_POP_* this island started as */
#if DOOMO_RES_SLOTS
    /* (GBA-500) id -> slot + 1 inside the id's district half (0: no slot, the id is not
     * in town), and slot -> id + 1 (0: free). All zero = nobody; not saved (a load binds
     * every resident it reads). A slot that holds a record always has an id. */
    uint8_t         res_slot[DOOMO_MAX_RESIDENTS];
    uint16_t        res_id[DOOMO_LIVE_RESIDENTS];
#endif
} DoomoTown;

/* town_res(DOOMO_PLAYER_ID) returns &doomo_town.player. The player can be
 * in relationship edges like anyone else. */
#define DOOMO_PLAYER_ID DOOMO_MAX_RESIDENTS

/* The one live town (~110 KB). Lives in EWRAM on GBA (default .bss). */
extern DoomoTown doomo_town;

/* ---- (GBA-500) the record of a resident id ---------------------------------------------
 * Every id 0..999 is a valid resident id on every platform; doomo_town.res[] holds
 * DOOMO_LIVE_RESIDENTS records (res_live.h). Where all 1000 live (PS1, Dreamcast, native)
 * the slot is the id and these are the plain array access of before. On the GBA:
 *   TOWN_RES(id)    the id's record; an id with no slot reads as an empty slot (a shared
 *                   all-zero record in ROM: never write through it; town_alloc / town_bind
 *                   give the id a slot first)
 *   town_slot(id)   its slot 0..DOOMO_LIVE_RESIDENTS-1, or -1
 *   town_slot_id(s) the id in slot s, or -1 for a free slot
 * id must be 0..DOOMO_MAX_RESIDENTS-1 (as for the array). A loop over every resident may
 * walk the slots (doomo_town.res[s], town_slot_id(s)) instead of the 1000 ids: same set,
 * slot order instead of id order. */
#if DOOMO_RES_SLOTS
/* the GBA keeps it in ROM (a stray write is lost, never seen as another id); the native
 * LIVE=500 build keeps it writable so the tests can catch a stray write (test_main.c) */
#ifdef DOOMO_PLATFORM_GBA
#define DOOMO_RES_NONE_CONST const
#else
#define DOOMO_RES_NONE_CONST
#endif
extern DOOMO_RES_NONE_CONST DoomoResident doomo_res_none;
static inline int town_slot(int id)
{
    int s = doomo_town.res_slot[id];
    return s ? (id >= DOOMO_DISTRICT_SIZE ? DOOMO_LIVE_PER_DISTRICT : 0) + s - 1 : -1;
}
static inline DoomoResident *town_rec_(int id)
{
    int s = town_slot(id);
    return s >= 0 ? &doomo_town.res[s] : (DoomoResident *)&doomo_res_none;
}
static inline int town_slot_id(int s) { return (int)doomo_town.res_id[s] - 1; }
#define TOWN_RES(id) town_rec_(id)
#else
#define town_slot(id)    (id)
#define town_slot_id(s)  (s)
#define TOWN_RES(id)     (&doomo_town.res[id])
#endif
/* TOWN_CLAIM(r, id): before code fills the record of an EMPTY id (a demo citizen, a test
 * cast), give id a slot and point r at its record; r becomes NULL when the district has no
 * free slot. Where every id lives it does nothing (r already is the record). */
#if DOOMO_RES_SLOTS
#define TOWN_CLAIM(r, id) ((r) = town_bind(id) >= 0 ? TOWN_RES(id) : NULL)
#else
#define TOWN_CLAIM(r, id) ((void)0)
#endif
/* Give id a slot in its district (GBA-500; where every id lives it is the id itself):
 * its own if it has one, else a free one (a slot whose record is empty). Returns the slot
 * or -1 when the district's slots are all in use. town_unbind() frees id's slot (its
 * record is cleared). */
int  town_bind(int id);
void town_unbind(int id);
/* Slots in use (records not RES_EMPTY) and free in a district. */
int  town_district_free(int district);

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
void rel_remove_at(int index);            /* drop edge #index, the table stays sorted */
/* (GBA-500) how much an edge is worth keeping when the graph is full (the weakest goes:
 * married / family, dating, best friends and nemeses, the avatar's ties first). */
int  rel_edge_strength(const DoomoEdge *e);
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

/* ---- foundation additions ------------------------------------------------ */
/* The record a premade citizen starts as: roster_premade(id) or, while the
 * roster module has not landed, a deterministic generated stand-in (ids
 * 500..999 hellborn). save.c deltas against this. */
void town_premade_record(int id, DoomoResident *out);
/* The record a Random Island citizen starts as for a town seed. */
void town_random_record(uint32_t seed, int id, DoomoResident *out);
/* LS0: after loading a version-1 save (no life stage yet), give every
 * citizen the stage that is known: premade citizens who still carry their
 * roster name get the roster's (KID / TEEN / ELDER), sim-born babies (their
 * unmistakable newborn look and no romance) become LIFE_BABY; everyone else
 * stays LIFE_ADULT. Returns how many stages were set. */
int  town_life_migrate_v1(void);
/* Starting money for a new island (H$). */
#define DOOMO_START_MONEY 500
/* Re-sort / validate the edge list (after loading or bulk edits): drops
 * self-edges, out-of-range ids and duplicates, normalises a < b. */
void rel_normalize(void);
/* Total of all inventory counts. */
int  inv_total(void);
/* 1 if the island has a living (non-empty, not dead) citizen in slot id. */
int  town_alive(int id);
/* "8:05 PM" / "20:05" per OPT_CLOCK_24H. buf >= 10 bytes. */
void town_time_str(char *buf, int cap);
extern const char *const doomo_weekday_names[7];
extern const char *const doomo_rel_kind_names[REL_KIND_COUNT];

#endif
