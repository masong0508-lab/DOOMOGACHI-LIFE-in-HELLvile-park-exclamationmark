/* story_ch12.h - Chapters 1-2 of Story Mode as VM content: the glue's
 * constants (owner: S2; docs/design/story/SPEC.md sec.7 S2 content,
 * story_final.md sec.9-11; the scripts are core/data/story/ch1/, ch2/).
 *
 * The 18 legacy beats of Chapters 1-2 (BEAT_C1_ARRIVE .. BEAT_C2_CELLAR,
 * the saved spine; story.h) still say where the plot is. What happens in
 * them is VM scripts in the episode packs (p0, ep11..ep16, int1, ep21..
 * ep27, f1, d1) and the chapter-common packs (ch1, ch2). story_ch12.c picks
 * the script for a talk / walk / door from the beat and its stage
 * (DoomoStoryState.quest_step, reset at every beat), places the cast and
 * says what the HUD objective is. The scripts change state through VM ops
 * (flags, items, beats, pushes) and through one native callback, CB_C12,
 * whose argument is an action below (textpack: `native CB_C12 C12A_X [P]`,
 * the parameter in the high byte).
 *
 * textpack.py reads this header (EXTRA_HEADERS), so every name here can be
 * used in a .dsc file.
 */
#ifndef DOOMO_STORY_CH12_H
#define DOOMO_STORY_CH12_H

/* the native callback id (story_script.c script_callback range 1..63) */
#define CB_C12 40

/* CB_C12 actions (the low byte of the argument; P = the high byte) */
enum {
    C12A_NONE,
    C12A_STEP,        /* quest_step = P (the beat's stage); the cast moves     */
    C12A_BIT,         /* quest_step |= 1 << P                                  */
    C12A_TIME_AT,     /* the clock moves forward to P:00 today (never back)    */
    C12A_WEAPON,      /* Rivet's first weapon: P 0 cute, 1 brutal              */
    C12A_ODDS,        /* v1 = exorcism_odds(Steamy)                            */
    C12A_SOUL_LOST,   /* Sprinklepop's scripted rite: her soul is lost          */
    C12A_CLUE,        /* STRY.clues |= 1 << P; v2 = clues found                */
    C12A_CLUES,       /* v2 = clues found (0..5)                               */
    C12A_NIGHT,       /* v2 = 1 at night                                       */
    C12A_PET_FORM,    /* STRY.pet_form |= P (1 nubs, 2 horns)                  */
    C12A_REFRESH,     /* the cast is placed again (after a scripted walk)      */
    C12A_HOME_DOOR,   /* the player stands at the cottage door (after a walk home) */
    C12A_EP_DONE,     /* story_episode_set_done(P): P is a BEAT_* id (< 256)   */
    C12A_SLEEP,       /* the night passes: the next morning, 8:00              */
    C12A_MASCOTTY,    /* Mascotty waves, very slowly (the overworld's starer)  */
    C12A_PUFF,        /* the free sample: corruption +15, STRY.branch puff bit  */
    C12A_COUNT
};

/* quest_step bits of C1_FRIENDS (Ep 1.4): the three neighbours */
#define C12_MET_STEAMY 0
#define C12_MET_HATTIE 1
#define C12_MET_GRAMPY 2

/* SCENE_CUTSCENE vignettes of the Ch1-2 content (cutscene_ch12.c); the
 * new CUT_* ids P0 / BOTTLE / F1 are drawn there too. push_args.txt lists
 * the range. */
enum { CUTV_FOG = 0x120,          /* Ep 1.6: eyes in the fog, the Mayor's face   */
       CUTV_RITE_FAIL,            /* Ep 2.5: THE POWER OF CUDDLES (Sprinklepop)  */
       CUTV_GATE_CRACKS,          /* Ep 2.7: Grumbleguts' first scene            */
       CUTV_END };

/* ---- the glue (core/src/story_ch12.c; story_script.c calls these for the
 * Chapter 1-2 beats; C only - textpack.py reads nothing below) ---- */
#ifndef DOOMO_TEXTPACK_SKIP
struct StoryPending;
struct StoryNpcSpot;
int  ch12_beat(int beat);                 /* 1 for BEAT_C1_ARRIVE .. BEAT_C2_CELLAR */
int  ch12_npc(int id);
int  ch12_trigger(int tag);
int  ch12_place(int place);
int  ch12_auto(void);                     /* 1 if something started by itself  */
void ch12_enter_beat(int beat);
int  ch12_resume(const struct StoryPending *p, int result);
const char *ch12_objective(void);
int  ch12_target(int *map_id, int *cx, int *cy);
int  ch12_cast(int map, struct StoryNpcSpot *out, int max);
void ch12_debug_setup(int beat);
void ch12_debug_episode(int ep);
int  ch12_debug_step(void);
void ch12_action(int arg);                /* CB_C12                            */
void ch12_give_weapon(int brutal);
int  ch12_pack(int beat);                 /* the episode pack, -1              */
int  ch12_idle(int id, int chapter, char *out, int cap);   /* 1 = a line */
/* story_script.c: what starts by itself on an idle overworld (story.c's
 * pump calls it; the chapters' halves answer for their beats) */
int  script_auto(void);
/* the beat stages (core/src/cutscene_ch12.c, the cutscene group): cutscene.c
 * hands them CUT_P0, CUT_BOTTLE, CUT_F1 and the CUTV_* vignettes */
int  cut12_owns(int arg);
void cut12_enter(int arg);
int  cut12_update(void);                  /* 1 = over (cutscene.c pops it)     */
void cut12_draw(void);
#endif

#endif
