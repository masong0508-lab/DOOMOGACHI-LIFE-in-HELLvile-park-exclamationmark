/* story.h - Story Mode (owner: story).
 *
 * The full arc (all playable in this build):
 * Chapter 1 "Welcome to HaHaland": extremely cutesy open-world life in
 *   HaHaland and on the Hillvile (good) side of Hellvile Park. Meet the
 *   citizens, hatch your Doomogachi, little fetch quests, all pastel. The
 *   Hellvile (bad) side is walled off; its citizens are only rumours.
 * Chapter 2 "Something in the Vapor": Cloud Nine Vapor opens by the wall.
 *   People get weird. First exorcism (the possessor is a named Hellvile
 *   citizen). Rivet, a Hellvile runaway, teaches you to weld. Hell Breach
 *   1: the Vape Cellar (first-person).
 * Chapter 3 "Hell Invades": the gate under Hellvile erupts out of a giant
 *   vape cloud; demons of hell invade wherever you are. The cute town turns
 *   into Doom. Weld cute or brutal weapons, clear the infestation, climb
 *   Cloud Nine Spire, defeat Lord Vapula.
 * Chapter 4 "Hellvile Park" (the ending - why the park is in the title):
 *   with Vapula gone you cross into Hellvile itself and learn most of its
 *   500 bad citizens were Vapula's prisoners too. You redeem, befriend and
 *   exorcise your way to peace; the wall between Hillvile and Hellvile
 *   comes down and every citizen of both sides finally comingles in
 *   Hellvile Park for one giant festival. Credits = a parade of all 1000
 *   citizens through the united park. Free play then unlocks the united
 *   park (cross-border friendships, romances, families).
 *
 * A small bytecode-free script engine: each scene step is a C function or
 * a table row; flags live in doomo_town.story.flags.
 */
#ifndef DOOMO_STORY_H
#define DOOMO_STORY_H

#include "types.h"
#include "save_ext.h"

int  story_flag(int f);
void story_set_flag(int f, int on);
void story_new_game(void);           /* after the player made their citizen */
/* Called by the overworld when the player presses A on an NPC / object or
 * steps on a CELL_TRIGGER cell with `tag`. */
void story_interact_npc(int resident_id);
void story_trigger(int tag);
const char *story_objective(void);   /* current quest text for the HUD */

/* ======================================================================
 * story additions (owner: story; additive)
 * ======================================================================
 *
 * STARTING STORY MODE. The title screen calls story_begin(): it makes a
 * fresh story town (TOWN_STORY, all 1000 premade citizens), then shows
 * SCENE_STORY with STORY_ARG_NEW, which first pushes SCENE_CREATOR with
 * arg -3 so the player creates themselves (town_res(DOOMO_PLAYER_ID)).
 * When the creator closes, SCENE_STORY calls story_new_game() and the
 * arrival cutscene starts. (If the creator itself calls story_new_game()
 * and sets SCENE_STORY, that works too.) Continuing a loaded story save is
 * scene_set(&SCENE_STORY, STORY_ARG_CONTINUE).
 *
 * SCENE_STORY args. */
enum { STORY_ARG_CONTINUE = 0,       /* resume doomo_town.story            */
       STORY_ARG_NEW = 1,            /* create yourself, then a new story  */
       STORY_ARG_VISIT_HAHALAND = 10,/* free play: stroll around, no plot  */
       STORY_ARG_VISIT_HELLVILE = 11,
       STORY_ARG_VISIT_PARK = 12,
       /* debugging / runner scripts / attract mode: a fresh story town
        * jumped to beat (arg - STORY_ARG_DEBUG_BEAT) by story_debug_jump();
        * STORY_ARG_DEBUG_NIGHT + beat does the same at 21:30. */
       STORY_ARG_DEBUG_BEAT = 100,
       STORY_ARG_DEBUG_NIGHT = 200 };
void story_begin(void);

/* Chapters (DoomoStoryState.chapter). */
enum { STORY_CH_NONE, STORY_CH1, STORY_CH2, STORY_CH3, STORY_CH4,
       STORY_CH_DONE };              /* 5: the ending was seen; free roam */
int  story_chapter(void);
const char *story_chapter_title(int chapter);    /* "Welcome to HaHaland" */

/* Main beats (DoomoStoryState.quest). Append only (saved). */
enum {
    /* chapter 1 */
    BEAT_C1_ARRIVE, BEAT_C1_MAYOR, BEAT_C1_KEY, BEAT_C1_HOME, BEAT_C1_HATCH,
    BEAT_C1_PIP, BEAT_C1_BUNNY, BEAT_C1_BUNNY_RETURN, BEAT_C1_FRIENDS,
    BEAT_C1_CUPCAKE, BEAT_C1_WALL,
    /* chapter 2 */
    BEAT_C2_VAPOR, BEAT_C2_WEIRD, BEAT_C2_SCREAM, BEAT_C2_FUZZ,
    BEAT_C2_EXORCISE, BEAT_C2_RIVET, BEAT_C2_CELLAR,
    /* chapter 3 */
    BEAT_C3_ERUPTION, BEAT_C3_NESTS, BEAT_C3_SPIRE,
    /* chapter 4 */
    BEAT_C4_GATE, BEAT_C4_PRISONERS, BEAT_C4_HELP, BEAT_C4_MAYOR,
    BEAT_C4_FESTIVAL, BEAT_C4_STAGE,
    /* after the credits */
    BEAT_DONE,
    BEAT_COUNT
};
/* --- S0: the story SPEC's beats (docs/design/story/SPEC.md sec.4, C14) ---
 * 38 ids appended after the legacy ones (P0, 27 episodes, 8 specials, E1,
 * E2). They are ids, not an order: the story order is S2's table. The 28
 * legacy BEAT_C* ids alias onto episodes (S2) and are never reordered.
 * BEAT_COUNT stays the legacy count (the old tables are sized by it) until
 * S2 moves those tables to BEAT_ALL_COUNT. */
enum {
    BEAT_P0 = BEAT_COUNT,                         /* the crayon prologue      */
    BEAT_EP1_1, BEAT_EP1_2, BEAT_EP1_3, BEAT_EP1_4, BEAT_EP1_5, BEAT_EP1_6,
    BEAT_EP2_1, BEAT_EP2_2, BEAT_EP2_3, BEAT_EP2_4, BEAT_EP2_5, BEAT_EP2_6, BEAT_EP2_7,
    BEAT_EP3_1, BEAT_EP3_2, BEAT_EP3_3, BEAT_EP3_4, BEAT_EP3_5, BEAT_EP3_6, BEAT_EP3_7,
    BEAT_EP4_1, BEAT_EP4_2, BEAT_EP4_3, BEAT_EP4_4, BEAT_EP4_5, BEAT_EP4_6, BEAT_EP4_7,
    BEAT_F1, BEAT_F2, BEAT_F3, BEAT_F4,           /* flashbacks               */
    BEAT_D1, BEAT_D2,                             /* dreams                   */
    BEAT_INT1, BEAT_INT3,                         /* interludes I and III     */
    BEAT_E1, BEAT_E2,                             /* epilogue                 */
    BEAT_ALL_COUNT
};
#define BEAT_NEW_COUNT (BEAT_ALL_COUNT - BEAT_P0)  /* 38 */
/* --- end S0 --- */
int  story_beat(void);
const char *story_beat_name(int beat);           /* "C1_MAYOR" (tests)   */
int  story_beat_chapter(int beat);

/* Story-specific cutscenes (SCENE_CUTSCENE arg). Chapter title cards are
 * CUT_CHAPTER1..4. When a cutscene ends it pops itself; SCENE_STORY then
 * continues the plot. */
enum { CUT_CHAPTER1, CUT_CHAPTER2, CUT_CHAPTER3, CUT_CHAPTER4,
       CUT_ARRIVAL, CUT_FOG, CUT_ERUPTION, CUT_VAPULA_DOWN, CUT_WALL_FALLS,
       CUT_FREEPLAY, CUT_HOME,
       CUT_FEAST_POP,       /* Feast of Rebuilding: "What an Ass" (Grumbleguts pops) */
       CUT_COUNT };
/* --- S0: the story SPEC's cutscenes (SPEC sec.4) --- appended after
 * CUT_FEAST_POP. CUT_COUNT stays 12 (the old tables, the gallery's CUTU()
 * and the platforms' cutview.c are sized by it); the new ids unlock through
 * UL_STORY_CUT2(c) (unlock_ids.h), never CUTU() (which would land on 644). */
enum { CUT_P0 = CUT_COUNT, CUT_BOTTLE, CUT_F1, CUT_F2, CUT_F3, CUT_F4,
       CUT_INT3_KNOCKS, CUT_SUMMIT, CUT_SAYIT, CUT_FEAST_SAVE, CUT_E1, CUT_BALLOON,
       CUT_ALL_COUNT };
/* --- end S0 --- */
const char *story_cutscene_name(int cut);

/* The overworld map the story is on (MAPID_*), and teleporting. */
int  story_map(void);
void story_goto(int map_id, int cell_x, int cell_y);

/* Debug / tests ----------------------------------------------------------
 * story_debug_jump: set up the state as if the player had just reached
 * `beat` (chapter follows the beat): flags, items, pet, weapons, map,
 * possessed citizens. Does not change scenes.
 * story_debug_autowin: minigames the story starts (combat, exorcisms, the
 * pet tutorial, welding) resolve at once as wins instead of pushing their
 * scenes - scripted playthroughs and attract mode. */
void story_debug_jump(int chapter, int beat);
void story_debug_autowin(int on);
int  story_debug_autowin_on(void);
/* Advance the plot by whatever the current beat needs (tests): talks to
 * the right NPC, steps on the right trigger, wins the fight. Returns 1 if
 * something happened. With autowin on, repeated calls play the whole story
 * up to BEAT_DONE (dialogue is answered with the first choice). */
int  story_debug_step(void);
/* Resident id (or -1) and overworld cell of the current beat's target. */
int  story_target(int *map_id, int *cell_x, int *cell_y);

/* Side quests ------------------------------------------------------------
 * More than 40 errands from named citizens of both districts across the
 * four chapters: fetch, deliver, matchmaking, lost pets, photos, mini-boss
 * hunts in Hell Breach maps, exorcism requests, cross-border reconciliation.
 * State is 2 bits per quest in the story flags (see the flag map). */
enum { SIDE_LOCKED, SIDE_OFFERED, SIDE_ACTIVE, SIDE_READY, SIDE_DONE };
enum { SQ_FETCH, SQ_DELIVER, SQ_FIND, SQ_PET, SQ_PHOTO, SQ_MATCH, SQ_HUNT,
       SQ_EXORCISE, SQ_RECONCILE, SQ_VISIT, SQ_KIND_COUNT };
#define STORY_SIDE_MAX 50
int  story_side_count(void);
const char *story_side_title(int q);
int  story_side_kind(int q);
int  story_side_giver(int q);                    /* resident id          */
int  story_side_state(int q);                    /* SIDE_*               */
int  story_side_chapter(int q);                  /* first chapter        */
const char *story_side_hint(int q);              /* objective / log text */
/* Where the giver stands and where the task happens (map, cell). */
void story_side_where(int q, int *map_id, int *cx, int *cy);
int  story_side_target(int q, int *map_id, int *cx, int *cy);
const char *story_side_kind_name(int kind);
int  story_side_done_count(void);
/* Tests: accept / finish a side quest immediately (rewards included). */
void story_side_debug_accept(int q);
void story_side_debug_finish(int q);

/* Places: buildings you enter (door cells tagged STORY_TAG_DOOR + place)
 * and what they offer (talk, shop, activities registered in
 * activities.h at that location, rest/save ...). */
enum { PLACE_NONE, PLACE_HOME, PLACE_TOWN_HALL, PLACE_CAFE, PLACE_SHOP,
       PLACE_CHURCH, PLACE_HOSPITAL, PLACE_WORKSHOP, PLACE_APARTMENTS,
       PLACE_CONCERT_HALL, PLACE_LIBRARY, PLACE_FUN_FAIR, PLACE_VAPE_SHOP,
       PLACE_PIER, PLACE_WALL_GATE, PLACE_GRAVEYARD,
       /* Hellvile */
       PLACE_HELL_PITS, PLACE_FORGE, PLACE_GATE, PLACE_ARENA,
       PLACE_CHAPEL, PLACE_FACTORY, PLACE_HELL_PIER, PLACE_MARKET,
       /* the united park */
       PLACE_STAGE, PLACE_STALLS,
       PLACE_COUNT };
const char *story_place_name(int place);
/* Open a place's menu (as if its door was used). */
void story_enter_place(int place);

/* Map data conventions (core/data/maps/{hahaland,hahaland_invaded,
 * hellvile,park_united}.txt):
 *   - door cells are solid wall cells with tag STORY_TAG_DOOR + PLACE_*:
 *     bumping into them (or A while facing them) opens the place
 *   - walk-on events are open CELL_TRIGGER cells with STORY_TAG_EVENT_*
 *   - THING_NPC marks a wander spot for ambient citizens (arg = zone)
 *   - THING_SPAWNER marks a demon nest in the invaded town (arg = nest) */
#define STORY_TAG_DOOR  64           /* 64 + PLACE_* (64..95)             */
enum { STORY_TAG_EVENT_BEACH = 128,  /* where Mr. Flops washed up         */
       STORY_TAG_EVENT_FOG,          /* the purple fog at the Wall        */
       STORY_TAG_EVENT_CELLAR,       /* the cellar door of Cloud Nine     */
       STORY_TAG_EVENT_SPIRE,        /* the foot of Cloud Nine Spire      */
       STORY_TAG_EVENT_STAGE,        /* the festival stage                */
       STORY_TAG_EVENT_GATE_SEAL,    /* the sealed hell gate (Hellvile)   */
       STORY_TAG_EVENT_WELCOME,      /* the town sign by the bus stop     */
       STORY_TAG_EVENT_SECRET        /* secret sparkles: + 0..15          */
};

/* Story NPC chatter + ambient citizens' small talk (story_talk.c). Writes
 * one line for `resident_id` into buf and returns it. */
const char *story_small_talk(int resident_id, int variant, char *buf, int cap);

/* Hellvile citizens the player has met / befriended (chapter 4). */
int  story_hellvile_friends(void);

/* ---- the flag map (DoomoStoryState.flags, 256 bits) ----------------------
 *   0..31    main plot one-shots (SF_* in story_int.h): choices made,
 *            cutscenes seen, NPC introductions, chapter 4 help tasks
 *   32..47   chapter 3 demon nests cleared (bits 32..39) and the possessed
 *            citizens exorcised during the invasion (40..43)
 *   48..63   "met" markers for the story cast and tutorials shown
 *   64..163  side quests, 2 bits each (50 quests): 0 offered/locked,
 *            1 active, 2 ready to hand in, 3 done
 *   164..195 secret sparkles found in the overworld maps (one per secret:
 *            HaHaland 0..7, Hellvile 16..23, the united park 24..31)
 *   196..227 places visited (PLACE_*) for the "explorer" unlock
 *   228..255 reserved
 * quest = BEAT_*, quest_step = a small counter of the current beat. */
#define STORY_FLAG_SIDE_FIRST   64
#define STORY_FLAG_SECRET_FIRST 164
#define STORY_FLAG_PLACE_FIRST  196

/* --- S0: the story SPEC's flags, STRY and hooks (SPEC sec.4, SF sec.17, R22, C13) ---
 *
 * The reserved bits 228..255 become the hot one-shots resident code reads
 * every frame or at scene entry (SF sec.17.1). 253..255 stay spare. */
enum {
    SF_ASKED_OTHER_SIDE = 228, SF_VISITED_GRAVE, SF_PET_NUBS, SF_PET_HORNS,
    SF_PET_HALO, SF_HATTIE_LOST, SF_GRAMPY_LOST, SF_SMOG_LOST, SF_SMOG_NEAR_MISS,
    SF_TALLY_THAT_WAS_ME, SF_GRUMBLE_DEAD, SF_GRUMBLE_NO_RITE, SF_GRUMBLE_FRIENDLY,
    SF_HOUSE_PLATED, SF_HOUSE_TURRET, SF_HOUSE_HELD, SF_HOUSE_WALL_GONE,
    SF_WISP_AWAKE, SF_FOUNTAIN_TWOWAY, SF_MASCOTTY_HEAD_OFF, SF_FEAST_DONE,
    SF_SUITCASE_OPEN, SF_SIGN_1000, SF_BOTTLE_SEEN, SF_NGPLUS,
    SF_SPARE_253                     /* 253..255 spare */
};
/* The spare "met"/nest/possessed bits (SF sec.17.1): 36..39, 42..47, 48..55 and
 * 58..63 (24 bits; 56/57 are Tallyworn and Mother Nox). They hold the
 * Interlude III knocks (8) and the wrongness W1..W16 "noticed" bits (16).
 * 36..39 and 42..43 are only safe if no map prop sets them (nest props
 * extra < 4, possessed props extra < 2): S2's test checks it (SPEC C19). */
#define STORY_KNOCKS 8
#define STORY_WRONGS 16
#define SF_KNOCK(k)  ((k) < 4 ? 36 + (k) : (k) < 6 ? 42 + ((k) - 4) : 62 + ((k) - 6)) /* k 0..7 */
#define SF_WRONG(w)  ((w) < 4 ? 44 + (w) : (w) < 12 ? 48 + ((w) - 4) : 58 + ((w) - 12)) /* w 0..15 */

/* 'STRY' v1: the story's second save chunk, exactly 96 B (SF sec.17.2 with
 * C9's LATER queue in the 16 spare bytes; no spare left: the next field
 * needs STRY v2). home_phase is the single source of the house phase.
 * The live copy is story_stry() (stry_save.c, S2). Field order keeps every
 * u16 aligned; the layout is the save format, never reorder. */
typedef struct DoomoStryLater {
    uint16_t script;         /* callback script id, 0 = empty slot              */
    uint16_t due;            /* game-minute it fires, low 16 bits               */
} DoomoStryLater;
typedef struct DoomoStry {
    uint8_t  ver;            /*  0 chunk version (1)                            */
    uint8_t  home_phase;     /*  1 HP_* (home.h)                                */
    uint8_t  episode_done[5];/*  2 36+2 episode/special bits                    */
    uint8_t  branch;         /*  7 wisp_answer 2 | sayit_line 2 | feast 2 | puff 1 | brutal 1 */
    uint8_t  side2[30];      /*  8 118 mission rows x 2 bits                    */
    uint8_t  knocks;         /* 38 knocks received 0..8                         */
    uint8_t  nests;          /* 39 nests 4 | Night Raids won 3 | hidden fifth 1 */
    uint8_t  clues;          /* 40 Ch2 clue board 5 | rites attempted 3         */
    uint8_t  feast_seed;     /* 41 the culprit draw seed                        */
    uint8_t  wrong[2];       /* 42 W1..W16 noticed (NG+ subtitle copy)          */
    uint8_t  sayit_said[12]; /* 44 96 bits: pool and cast lines said            */
    uint8_t  heights[4];     /* 56 door-frame marks: Wisp, Pip, pet, Wisp's old (px) */
    uint16_t fountain_q8;    /* 60 the giggle's lateness, frames Q8             */
    uint16_t feast_clues;    /* 62 12 clue bits + 4 confirm bits                */
    uint16_t grumble;        /* 64 dishes fed 4 | pacifist 1 | exes met 3 | 8 spare (C13) */
    uint16_t tally_days;     /* 66 Tallyworn's board                            */
    uint8_t  feast_meter;    /* 68 0..9                                         */
    uint8_t  hee_count;      /* 69 S198's counter                               */
    uint8_t  pet_form;       /* 70 nubs/horns/halo/glow/nap-on-command/bonnet   */
    uint8_t  ngplus;         /* 71 runs 2 | subtitles 1 | Feast re-drawn 1      */
    uint8_t  gag_cool[8];    /* 72 spawner cooldowns in minutes                 */
    DoomoStryLater later[4]; /* 80 pending "Later" callbacks (C9, C14)          */
} DoomoStry;                 /* 96 */
DOOMO_STATIC_ASSERT(sizeof(DoomoStry) == 96, doomo_stry_is_96_bytes);
#define STRY_CHUNK_TAG     SAVE_EXT_TAG('S', 'T', 'R', 'Y')
#define STRY_CHUNK_VERSION 1
DoomoStry *story_stry(void);         /* the live copy                       */
int  story_episode_done(int beat);   /* BEAT_P0.. done bit                   */

/* Grumbleguts (roster resident 729) is a real citizen (C13): friendship,
 * romance, death and burial go through the sim. STRY.grumble keeps only the
 * story-only state these getters read (story_grumble.c, S5). */
int  story_grumble_dishes(void);     /* dishes fed 0..15                     */
int  story_grumble_pacifist(void);
int  story_grumble_exes_met(void);   /* 0..7                                 */
int  story_grumble_dead(void);       /* SF_GRUMBLE_DEAD                      */
int  story_grumble_no_rite(void);    /* SF_GRUMBLE_NO_RITE                   */
/* The graveyard's rite blocker clause (C13): 1 if `resident_id` may not be
 * raised by the ghost rite because of the story. */
int  story_rite_blocker(int resident_id);
/* Authored possessors (S3, exorcism.c): the true-name id that overrides
 * sim_soul.c's pick for `resident_id`, or -1. */
int  story_possessor_override(int resident_id);
/* Hooks the customisation spec re-targets (C10; S2). 256 = 1.0. */
int  story_pacing_scale(void);
int  story_encounter_rate(void);
/* --- end S0 --- */

#endif
