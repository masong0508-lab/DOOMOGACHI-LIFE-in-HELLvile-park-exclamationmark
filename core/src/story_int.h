/* story_int.h - internals shared by the story module (owner: story):
 * story.c (state machine), story_script.c (the main plot), story_side.c
 * (side quests), story_talk.c (small talk), story_places.c (buildings),
 * overworld*.c (SCENE_STORY), story_questlog.c (SCENE_QUESTLOG),
 * cutscene.c (SCENE_CUTSCENE), credits.c (SCENE_CREDITS).
 */
#ifndef DOOMO_STORY_INT_H
#define DOOMO_STORY_INT_H

#include "doomo/doomo.h"
#include "doomo/story_vm.h"
#include "doomo/txt.h"
#include "../gen/story_ids.h"     /* SCR_* / TXT_* (textpack.py) */

/* ---- story flags 0..63 (the flag map is in story.h) ---------------------- */
enum {
    SF_STARTED = 0,      /* story_new_game ran                               */
    SF_KEY_GIVEN,        /* Nana gave the house key                          */
    SF_EGG_GIVEN,        /* ... and the Doomogachi egg                       */
    SF_HOME_SEEN,        /* the cottage cutscene played                      */
    SF_BUNNY_FOUND,      /* Mr. Flops picked up at the beach                 */
    SF_CUPCAKE_GIVEN,    /* Nana handed over the cupcake                     */
    SF_FOG_SEEN,         /* the fog cutscene played                          */
    SF_VAPE_SAMPLE,      /* took Vex's free sample (corruption +)            */
    SF_FUZZ_PRACTICE,    /* the practice ritual on the plush dummy           */
    SF_STEAMY_SAVED,     /* first exorcism worked                            */
    SF_STEAMY_LOST,      /* ... or Steamy's soul was lost                    */
    SF_WEAPON_CHOSEN,    /* Rivet's first weapon was given                   */
    SF_WEAPON_BRUTAL,    /* ... and it was the brutal one                    */
    SF_VEX_GONE,         /* Vex fled into the cellar                         */
    SF_RIVET_UPGRADE,    /* chapter 3: upgraded at the workshop              */
    SF_C4_SOOTKIN,       /* chapter 4 help: Sootkin's sunflower              */
    SF_C4_LADY_SMOG,     /* chapter 4 help: Lady Smog freed from the smoke   */
    SF_C4_RIVET_HOME,    /* chapter 4 help: Rivet hugged her mum             */
    SF_C4_MAYOR_CAME,    /* the Mayor crossed into Hellvile                  */
    SF_CREDITS_SEEN,     /* the parade rolled                                */
    SF_TITLE_CH1, SF_TITLE_CH2, SF_TITLE_CH3, SF_TITLE_CH4,  /* cards shown   */
    SF_VEX_REFORMED,     /* chapter 4: Vex says sorry                        */
    SF_VAPULA_PLUSH,     /* chapter 4: met the deflated Vapula               */
    SF_PET_HINT,         /* "your pet follows you" hint shown                */
    SF_WALL_HINT,        /* first whisper at the Wall (chapter 1)            */
    SF_MAP_HINT,         /* START menu hint shown                            */
    SF_INVASION_HINT,    /* "walk into a demon nest" hint                    */
    SF_31_FREE,
    /* 32..39 chapter 3 nests cleared */
    SF_NEST0 = 32,
    /* 40..43 chapter 3 possessed citizens saved (or lost) */
    SF_C3_EXO0 = 40,
    /* 48..63 met the cast (NPC_* index) and tutorials */
    SF_MET0 = 48
};
#define STORY_NESTS 4
#define STORY_C3_POSSESSED 2

/* Story cast (roster ids, see roster.h) and other fixed citizens. */
#define ID_MAYOR        ROSTER_ID_MAYOR
#define ID_NANA         ROSTER_ID_NANA_PUDDING
#define ID_RIVET        ROSTER_ID_RIVET
#define ID_PIP          ROSTER_ID_PIP
#define ID_FUZZ         ROSTER_ID_FATHER_FUZZ
#define ID_BLOOM        ROSTER_ID_DR_BLOOM
#define ID_VEX          ROSTER_ID_VEX
#define ID_VAPULA       ROSTER_ID_LORD_VAPULA
#define ID_STEAMY       2      /* bath house keeper: first possession        */
#define ID_LULLA        36     /* daycare worker who adores cupcakes          */
#define ID_HATTIE       71     /* chapter 3 possessed #1 (comedian)          */
#define ID_GRAMPY       88     /* chapter 3 possessed #2 (grandpa)           */
#define ID_SOOTKIN      525    /* gate laborer with a secret sunflower        */
#define ID_ANVIL        595    /* Rivet's mum                                 */
#define ID_LADY_SMOG    502    /* loyalist courtier, choking on old smoke     */
#define ID_MOTHER_NOX   778    /* Hellvile's high priestess (potluck host)    */
#define ID_TALLYWORN    712    /* counts the days until the Wall falls        */
#define ID_HAZE         709    /* Cloud Nine clerk who secretly hates smoke   */

/* ---- pending sub-scenes ---------------------------------------------------
 * SCENE_STORY pushes another scene (creator, pet, exorcism, workshop,
 * combat, cutscene, credits, shop, activity...) and remembers why; when it
 * is on top again it hands the result to story_resume(). The PEND_* kinds
 * are story_vm.h's. */
/* What a pending result is for (CTX_VM: a script's PUSH, resumed in the VM). */
enum { CTX_NONE, CTX_BEAT, CTX_SIDE, CTX_NEST, CTX_C3_EXO, CTX_C4_SMOG,
       CTX_FREE, CTX_VM };
typedef struct StoryPending {
    uint8_t kind;          /* PEND_* */
    uint8_t ctx;           /* CTX_*  */
    int16_t arg;           /* resident / map / cutscene id                */
    int16_t extra;         /* beat, side quest, nest index ...            */
} StoryPending;
extern StoryPending story_pending;

/* Start a sub-scene for a reason (with autowin the result is applied at
 * once and nothing is pushed). */
void story_push_combat(int map_id, int ctx, int extra, int cx, int cy, int nest);
void story_push_exorcism(int resident_id, int ctx, int extra);
void story_push_cutscene(int cut, int ctx, int extra);
void story_push_scene(const DoomoScene *s, int arg, int pend, int ctx, int extra);
/* Called by SCENE_STORY when it is the top scene again. */
void story_resume(void);

/* ---- conversations ----------------------------------------------------------
 * Text may contain @P (player name), @E (pet name), @D (the demon
 * possessing the current target), @T (the target's name), @V (Vex's
 * flavour of the day), @0..@9 (numbers). Speaker -1 = narrator,
 * DOOMO_PLAYER_ID = you. The plot's lines are VM scripts in the text packs:
 * story_conv(SCR_*, cb) runs one; when it ends (its lines read, its choice
 * made) callback id cb runs with the choice (var 0). Callbacks are ids
 * (story_script.c CB_* 1..63, story_side.c 64..127, story_places.c
 * 128..255), never function pointers (SPEC C6). A conversation started while
 * another runs waits its turn. */
void story_conv(unsigned script, int cb);
void story_conv_arg(unsigned script, int cb, int arg);     /* @1 = arg */
void story_say_txt(int who, unsigned text_id);             /* one line */
void story_say_txt_n(int who, unsigned text_id, int n0);   /* @0 = n0  */
void story_choice_id(int who, const char *q, const char *const *opts, int n, int cb);
void story_poll_callbacks(void);
void story_vm_pump(void);                 /* SCENE_STORY's update, every tick */
void story_text_prefetch(void);           /* SCENE_STORY's enter              */
/* story_trigger() from the overworld itself (the public one only records
 * the tag for story_trigger_poll(), see story.c). */
void story_trigger_ow(int tag);
void story_trigger_poll(void);
void story_vm_minute(void);               /* due "Later" callbacks           */
void script_callback(int cb, int choice);           /* story_script.c */
void side_callback(int cb, int choice);             /* story_side.c (64..127)    */
void place_callback(int cb, int choice);            /* story_places.c (128..)    */
void story_say(int who, const char *text);          /* formatted, queued */
void story_say_then(void (*done)(void));            /* after the queue   */
void story_choice(int who, const char *q, const char *const *opts, int n,
                  void (*done)(int));
void story_fmt(char *out, int cap, const char *tmpl);
/* story_feast.c: CUT_FEAST_POP's picture and sound (called by cutscene.c) */
void feast_tick(int panel, uint32_t pt);
void feast_draw(int panel, uint32_t pt, uint32_t ct);
int  feast_front_shot(int panel, uint32_t pt);
/* cutscene.c: screen y of the top of the current caption box (sh if none) */
int  cutscene_caption_y(void);
#define story_fmt_target vm_fmt_target              /* @T / @D subject (scriptvm.c) */
/* 1 while a conversation the story started is still running. */
int  story_talking(void);
/* (S2) Recompute the resident HUD objective story_objective() returns. */
void story_objective_refresh(void);

/* ---- beats ------------------------------------------------------------------ */
void story_set_beat(int beat);                       /* + chapter, music  */
int  story_step(void);
void story_set_step(int n);
/* Per-beat logic in story_script.c. Return 1 if the beat handled it. */
int  script_npc(int id);
int  script_trigger(int tag);
int  script_place(int place);
void script_resume(const StoryPending *p, int result);
const char *script_objective(void);
int  script_target(int *map_id, int *cx, int *cy);
void script_enter_beat(int beat);                    /* when it starts   */
void script_debug_setup(int beat);                   /* story_debug_jump */
int  script_debug_step(void);
/* Who stands where in the current beat (story cast placement). Fills up
 * to max entries {id, cell x, cell y} for map_id; returns the count. */
typedef struct StoryNpcSpot { int16_t id; uint8_t x, y, dir, flags; } StoryNpcSpot;
enum { NPCF_FIXED = 1,     /* does not wander                            */
       NPCF_MAIN = 2,      /* main quest marker (!)                      */
       NPCF_SIDE = 4,      /* side quest offered (?)                     */
       NPCF_READY = 8,     /* side quest ready to hand in (heart)        */
       NPCF_DEMON = 16 };  /* id is a THING_* demon (invasion nests)     */
int  script_cast(int map_id, StoryNpcSpot *out, int max);
/* The overworld walked into chapter 3 demon nest `nest`. */
void script_nest_encounter(int nest);
/* Small talk finished (FRIENDS / WEIRD / PRISONERS / FESTIVAL counters). */
void script_ambient_talked(int id, int new_friend);
/* Price of an item in the story's mini shops. */
int  story_item_price(int item);
/* Add a billboard to the overworld's list (overworld.c). */
void ow_add_billboard(int32_t x, int32_t y, int z, const DoomoImage *img, int flags, int light);

/* ---- side quests (story_side.c) -------------------------------------------- */
int  side_npc(int id);                    /* 1 if a side quest used the talk */
int  side_trigger_spot(int map_id, int cx, int cy, int press);  /* find/photo */
int  side_place(int place);
void side_resume(const StoryPending *p, int result);
int  side_cast(int map_id, StoryNpcSpot *out, int max);
/* World markers for active find / photo / pet quests on a map. */
typedef struct SideSpot { uint8_t x, y, kind, q; } SideSpot;
int  side_spots(int map_id, SideSpot *out, int max);
int  side_active_count(void);
const char *side_objective(void);         /* first active side quest text  */
int  story_side2_giver_map(int sid);      /* S2: MAPID_* a mission's giver is on */
/* --- S4: side missions batch A (docs/design/story/SPEC.md sec.7 S4) ---
 * `native CB_SIDE_* ARG` in a side script: little questions the VM cannot
 * ask by itself (story_side.c side_callback); the answer is var 0. */
enum { CB_SIDE_HAVE = 70,       /* ARG = ITEM_*: how many you carry           */
       CB_SIDE_STATE,           /* ARG = mission number (100..217): its row state 0..3 */
       CB_SIDE_NIGHT,           /* 1 if it is night                           */
       CB_SIDE_STATUS,          /* ARG = resident: RES_* status               */
       CB_SIDE_QUEST,           /* ARG = old quest #0..#48: SIDE_* state       */
       CB_SIDE_MONEY,           /* H$ / 10 you carry (capped at 32767)        */
       CB_SIDE_DAY,             /* the day of the week 0 Monday .. 6 Sunday    */
       CB_SIDE_MUSIC,           /* the map's own music back (after a song)     */
       CB_SIDE_ONCE,            /* ARG = SONG_*: play it once, then the map's
                                   music comes back (and it joins the jukebox) */
       CB_SIDE_CHAPTER };       /* the chapter now, 1..5                        */
/* S4's story minigames (story_mg_a.c, the doomo_story_mg_a registry; `push
 * minigame SMG_A_*`). Results: 0 lost, 1 won; SMG_A_WHACK 2 = you bonked
 * Father Fuzz. */
enum { SMG_A_HOPSCOTCH = 0x0A01,   /* S150 lava hopscotch: ten squares on the beat  */
       SMG_A_ROLLER,               /* S143 the roller and the population counter    */
       SMG_A_WHACK,                /* S146 Whack-an-Imp, switched on                */
       SMG_A_WHACK_OFF,            /* S146 ... switched off (they come anyway)      */
       SMG_A_BALLOONS,             /* S130 Twisty's stall: keep twenty pink ones    */
       SMG_A_BOBBER };             /* S144 the float dips: pull                     */
/* The quest log's rows for S100..S217 (the Side tab, after the 49 old ones):
 * the visible ones (offered, active or done) in row order; title and hint
 * come from the mission's index pack (one pack, so the PS1 never thrashes
 * its side slot). */
int  story_side2_visible(int sid);
const char *story_side2_title(int sid);
const char *story_side2_hint(int sid);
/* The resident a running verb needs on the map (escort / photo / deliver)
 * and the cell the script gave it; -1 none (story_verbs.c). */
int  story_verb_who(int *cx, int *cy);
/* --- end S4 --- */

/* ---- S2: the verbs and the story minigames (story_verbs.c, story_timing.c) ----
 * A verb runs on the overworld while the VM is free; SCENE_STORY's update
 * and its player code feed it. */
void story_verb_begin(int kind, int who, int arg, int then_script);   /* VERB_* */
int  story_verb_active(void);             /* VERB_* running, -1 none          */
void story_verb_cancel(void);
void story_verbs_tick(void);              /* SCENE_STORY's update             */
void story_verbs_cell(int cx, int cy);    /* the player stepped on a cell     */
int  story_verbs_press_a(void);           /* 1 if the verb used this A press  */
int  story_verbs_npc(int id);             /* 1 if talking to id ended a verb  */
int  story_verbs_walk_q8(void);           /* walking speed (256 = normal)      */
const char *story_verbs_objective(void);  /* the HUD line while a verb runs, NULL */
int  story_mg_result(void);               /* SCENE_STORY_MG's last result      */
int  story_timing_result(void);           /* SCENE_STORY_TIMING's: rounds 0..3  */
int  story_timing_debug(int *round, int *to_punch);   /* tests: state 0 card .. 3 result */
/* the rules (pure; tests) */
enum { ESC_FOLLOW, ESC_WAIT, ESC_FAR, ESC_LOST };
int  story_escort_judge(int dist_cells, int far_ticks);
int  story_photo_judge(int px, int py, int facing, int tx, int ty);   /* 0 / 1 / 2 framed */
int  story_find_heat(int dx, int dy);     /* 0 cold .. 3 hot, 4 found          */
int  story_timing_judge(int err_frames);  /* 2 perfect, 1 good, 0 miss         */
int  ow_player_facing(void);              /* overworld.c: 0 down 1 left 2 right 3 up */

/* ---- places (story_places.c) -------------------------------------------- */
int  place_of_tag(int tag);               /* PLACE_* or 0                    */
const char *place_sign_text(int map_id, int cx, int cy); /* signs, "" if none */
/* The overworld region name at a cell ("Town Plaza"), for the banner. */
const char *story_region_name(int map_id, int cx, int cy);

/* ---- small talk (story_talk.c) -------------------------------------------- */
/* A full little chat with an ambient citizen (2-3 lines + friendship). */
void talk_ambient(int id);
/* One line of a story NPC's idle chatter for the current chapter. */
const char *talk_cast_idle(int id);

/* ---- the overworld (overworld*.c) ------------------------------------------ */
void ow_enter_map(int map_id, int cx, int cy);       /* teleport + load   */
void ow_refresh_cast(void);                          /* re-place the NPCs */
int  ow_player_cell(int *cx, int *cy);
void ow_shake(int ticks);
void ow_flash(int color, int ticks);
void ow_banner(const char *text);
void ow_sync_to_save(void);
int  ow_music_for_map(int map_id);
/* The demon nests visible on the invaded map. */
int  ow_nest_cell(int nest, int *cx, int *cy);
/* Overworld ticks (animation clock). */
uint32_t ow_time(void);
/* 1 while SCENE_STORY runs the plot on the overworld. */
int  ow_active(void);

/* Friends: residents whose friendship level with you is at least this. */
#define STORY_FRIEND_LEVEL 2
int  story_friend_count(int district);   /* -1 = both                        */
void story_befriend(int id, int amount);

/* Open a place's menu: talk, shop, activities, rest... (story_places.c). */
void story_place_menu(int place);
void story_mark_friends_dirty(void);
int  story_unlock_quiet(int id);
/* Pick flowers at a flower bed prop (once per bed per day). */
int  story_pick_flowers(int map_id, int cx, int cy);

/* Pet name or "your egg". */
const char *story_pet_name(void);
/* Player name (never empty). */
const char *story_player_name(void);

/* Rewards: money + item + unlock; queues a toast. */
void story_reward(int money, int item, int count, int unlock_id);

/* Unlock ids (unlocks_story.c, range 608..735). */
enum {
    USTORY_UNITED_PARK = 608, USTORY_HELLVILE_VISITS, USTORY_NEW_GAME_PLUS,
    USTORY_SONG_FIRST = 611,            /* + SONG_* - 1 (19 songs)          */
    USTORY_CUT_FIRST = 632,             /* + CUT_* (12 cutscenes)           */
    USTORY_CH1_DONE = 644, USTORY_CH2_DONE, USTORY_CH3_DONE, USTORY_CH4_DONE,
    USTORY_ALL_SIDE, USTORY_EXPLORER, USTORY_SECRETS, USTORY_PACIFIST,
    USTORY_FRIEND_OF_ALL, USTORY_BRUTAL_START, USTORY_CUTE_START,
    USTORY_REWARD_FIRST = 660            /* side quest reward parts / skins  */
};
#define USTORY_NONE 0xFFFF

#endif
