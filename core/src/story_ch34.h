/* story_ch34.h - Chapters 3-4, Interlude III, the flashbacks F2-F4, the
 * dream D2 and the epilogue as VM content: the glue's constants (owner:
 * S3; docs/design/story/SPEC.md sec.7 S3, story_final.md sec.12-13a; the
 * scripts are core/data/story/ch3/, ch4/, epi/).
 *
 * The legacy beats BEAT_C3_ERUPTION .. BEAT_DONE (story.h, the saved spine)
 * still say where the plot is; each one holds a run of episodes in the
 * story order (C3_ERUPTION: Ep 3.1-3.2; C3_NESTS: 3.3, 3.4, F3, 3.5;
 * C3_SPIRE: 3.6, 3.7, F2, Interlude III; C4_GATE: 4.1; C4_PRISONERS: 4.2;
 * C4_HELP: 4.3, 4.4, D2, 4.5, F4; C4_MAYOR: 4.6; C4_FESTIVAL / C4_STAGE:
 * 4.7; DONE: E1, E2). The current episode is the first of the run whose
 * STRY done bit is clear, its stage is DoomoStoryState.quest_step (reset
 * at every episode). story_ch34.c picks the script for a talk / walk /
 * door from the episode and its stage, places the cast and says what the
 * HUD objective is; story_script.c hands it every beat from C3_ERUPTION on.
 *
 * The scripts change state through VM ops and through one native
 * callback, CB_C34 (`native CB_C34 C34A_X`, the action's parameter in the
 * high byte: `native CB_C34 C34A_X + 256 * P` is spelled C34(X, P) below
 * for the C side). textpack.py reads this header (EXTRA_HEADERS).
 */
#ifndef DOOMO_STORY_CH34_H
#define DOOMO_STORY_CH34_H 1

/* the native / conversation callback (story_script.c's range 1..63; S2's
 * Chapters 1-2 glue is CB_C12 = 40, the VM tests use 63) */
#define CB_C34 62
/* the C side's own conversation callbacks: CB_C34_FIRST .. CB_C34 - 1 */
#define CB_C34_FIRST 54

/* SCENE_PET arg: the pet-care boss of Ep 4.5 (pet under fire; C pushes it) */
#define PET_ARG_BOSS 1

/* CB_C34 actions: the argument's low byte (P = the high byte). As a
 * conversation callback (story_conv(scr, CB_C34)) the "argument" is the
 * choice the script ended on, and story_ch34.c runs the action it queued. */
enum {
    C34A_NONE,
    C34A_NEXT,        /* the episode's stage + 1                                  */
    C34A_STEP,        /* stage = P                                                */
    C34A_BIT,         /* stage |= 1 << P (a set of any-order tasks)               */
    C34A_DONE,        /* the current episode is done: the next one (or beat)      */
    C34A_PET,         /* STRY.pet_form |= P and SF_PET_* (1 nubs, 2 horns, 4 halo) */
    C34A_HELD,        /* v0 = 1 if the siege kept four guests safe (SF_HOUSE_HELD) */
    C34A_WALL_GONE,   /* the siege took a wall (SF_HOUSE_WALL_GONE)                */
    C34A_WISP,        /* STRY.branch wisp_answer = 1 + v0 (the second player line) */
    C34A_SAYIT,       /* STRY.branch sayit_line = v0 (the third player line)       */
    C34A_KNOCK,       /* Interlude III: knock P received (STRY.knocks, SF_KNOCK)    */
    C34A_NIGHT,       /* v2 = 1 at night                                           */
    C34A_LOST,        /* v2 = 1 if host P's soul is lost (P: 0 Steamy, 1 Hattie, 2 Grampy, 3 Smog) */
    C34A_PACIFIST,    /* USTORY_PACIFIST when no brutal weapon was used and the gate was fed */
    C34A_BREACH,      /* push Hell Breach P (LV_*: SCENE_BREACH; the result -> the stage) */
    C34A_PETBOSS,     /* push the pet-care boss (SCENE_PET, PET_ARG_BOSS)          */
    C34A_CUT,         /* push the S3 cutscene CUT_P0 + P (SCENE_CUTX)             */
    C34A_HOME,        /* the player stands at the cottage door                     */
    C34A_TRAVEL,      /* teleport: P 0 HaHaland gate, 1 Hellvile gate, 2 the park   */
    C34A_SPIRE_CAPS,  /* stash the Spire's floor captions for the climb (C5)        */
    C34A_FRIENDS,     /* v2 = Hellvile friends                                     */
    C34A_GRAVE,       /* STRY.nests |= 1 << P: a nest's heart (its Breach) is beaten */
    C34A_E2,          /* the suitcase: the frame and the polaroid (SF_SUITCASE_OPEN) */
    C34A_REFRESH,     /* the cast is placed again                                  */
    C34A_COUNT
};

/* quest_step bits of the any-order episodes */
#define C34_FAV_SOOTKIN 0          /* Ep 4.3: the three favours      */
#define C34_FAV_SMOG    1
#define C34_FAV_ANVIL   2
#define C34_ST_COOK  0             /* Ep 4.7: the four stalls        */
#define C34_ST_FISH  1
#define C34_ST_PHOTO 2
#define C34_ST_JOKE  3

/* resident ids of the Ch3-4 cast who are not in story_int.h (roster.h) */
#define ID_WISP       563
#define ID_MASCOTTY   106
#define ID_SQUEAKER   64
#define ID_SPROCKET   596
#define ID_BOLT       597
#define ID_SIGNHERE   567
#define ID_HAZEY      788
#define ID_THUMPKIN   894
#define ID_CLINKER    753
#define ID_PLODDY     701
#define ID_WHEEZLE    707
#define ID_SHACKLES   604
#define ID_DRUDGE     787
#define ID_SHUTTERBUG 46
#define ID_HONKERS    501
#define ID_GRAVELEY   730
#define ID_CHARLOTTE  566
#define ID_SPRINKLE   81
#define ID_GRUMBLE    729
#define ID_TOCK       104
#define ID_RATTLECUP  507

/* ---- the glue (core/src/story_ch34.c; story_script.c calls these for the
 * beats from C3_ERUPTION on; C only - textpack.py reads nothing below) ---- */
#ifndef DOOMO_TEXTPACK_SKIP
struct StoryPending;
struct StoryNpcSpot;
int  ch34_beat(int beat);                 /* 1 for BEAT_C3_ERUPTION .. BEAT_DONE */
int  ch34_episode(void);                  /* the current BEAT_EP3_1 .. BEAT_E2 id, -1 */
int  ch34_pack(void);                     /* its pack (the PS1 episode slot), -1 */
int  ch34_npc(int id);
int  ch34_trigger(int tag);
int  ch34_place(int place);
int  ch34_auto(void);                     /* 1 if something started by itself  */
void ch34_enter_beat(int beat);
void ch34_resume(const struct StoryPending *p, int result);
const char *ch34_objective(void);
int  ch34_target(int *map_id, int *cx, int *cy);
int  ch34_cast(int map, struct StoryNpcSpot *out, int max);
void ch34_debug_setup(int beat);
void ch34_debug_episode(int beat);       /* after a jump to a new episode id */
int  ch34_debug_step(void);
int  ch34_owns_cb(int cb);                /* CB_C34_FIRST .. CB_C34            */
void ch34_callback(int cb, int arg);

/* SCENE_CUTX (cutstage.c, the PS1 cutscene group): the S3 cutscenes on
 * beat.h stages, their scripts in the episode packs. story.c's
 * story_push_cutscene pushes it for these ids instead of SCENE_CUTSCENE. */
struct DoomoScene;
extern const struct DoomoScene SCENE_CUTX;
#define CUTX_OWNS(c) ((c) == CUT_F2 || (c) == CUT_F3 || (c) == CUT_F4 || (c) == CUT_SUMMIT || \
                      (c) == CUT_SAYIT || (c) == CUT_E1)

/* Say It (story_summit.c): the k-th friend on the walk (0..7) and their
 * one unsaid thing, decoded into line (STRY.sayit_said remembers what was
 * said: pool bits 0..47, cast bits 48..77). summit_begin picks the friends. */
#define SAYIT_FRIENDS 8
#define SAYIT_POOL    48
void summit_begin(void);
int  summit_friend(int k);
int  summit_line(int k, char *line, int cap);

/* story_sprites.c: the small props (any integer scale s) and the credits'
 * const billboards */
enum { SS_IMG_HORNS, SS_IMG_JAR, SS_IMG_HALO, SS_IMG_COUNT };
#define SS_UID_BASE 0x00C30000u           /* fixed uids of the const images   */
const DoomoImage *ss_image(int k);
void ss_tiny_cloud(int cx, int cy, int r, uint32_t t);
void ss_jar(int cx, int y_bottom, int s, int rain, uint32_t t);
void ss_horns(int cx, int y, int s);
void ss_halo(int cx, int y, int s);
void ss_wisp(int cx, int y_feet, int s, int ghost, uint32_t t);
void ss_poop(int cx, int y_bottom, int s, int horns);
#endif

#endif
