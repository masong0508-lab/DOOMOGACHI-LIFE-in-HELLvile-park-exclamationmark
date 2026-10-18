/* story_vm.h - the story bytecode VM: one resident interpreter for every
 * episode, special, side mission and conversation hook.
 *
 * CONTRACT (docs/design/story/SPEC.md sec.4): written by S0, maintained by S2
 * (add-only). Implementation: core/src/scriptvm.c (S2; resident on the PS1,
 * which is why the file is NOT named story_*.c - SPEC R4). Until it lands
 * every function is a weak stub in core/src/stubs.c.
 *
 * Scripts and their text live in packs (txt.h). The VM state is 64 B of
 * .bss. Everything advances on the 30 Hz tick; no wall clock is read.
 * Ops that need overlay code (CAST, NPC, CAM, TELEPORT, quest UI, INSERT and
 * FREEZE_CAP drawing, and the HOME sub-ops that need catalog/score/layout
 * code) are returned to the host scene as VM_EXT (SPEC C6).
 */
#ifndef DOOMO_STORY_VM_H
#define DOOMO_STORY_VM_H

#include "types.h"

typedef struct StoryOp { uint8_t op; uint8_t a; uint16_t b; } StoryOp;  /* 4 B, ROM / pack */
typedef struct StoryScript { uint16_t first_op, n_ops; uint8_t n_labels, pack; } StoryScript;

/* Op codes (SF sec.15.2 + SPEC sec.4 + C14 TIME). Stored in packs: append only.
 *   op            a                         b                                         */
enum {
    OP_END,          /* -                      -                                        */
    OP_SAY,          /* who (resident id lo; 0xFE narrator, 0xFF player; a DATA op
                        follows for ids > 253)  text id                                  */
    OP_SAY_X,        /* expr (EXPR_*)          text id (the speaker is the last SAY's)  */
    OP_CHOICE,       /* n (2-4)                question text id; n option text ids and n
                                                 labels follow as DATA ops               */
    OP_PLAYER_LINE,  /* n                      as CHOICE (the three pill lines, SF sec.8.4) */
    OP_JIF,          /* flag id                label: jump if the flag is set            */
    OP_JIFN,         /* flag id                label: jump if the flag is clear          */
    OP_JLT,          /* var id                 label: jump if var < the DATA value next  */
    OP_JGE,          /* var id                 label: jump if var >= the DATA value next */
    OP_SETF,         /* flag id lo             flag id hi | STRY bit space (VMF_*)       */
    OP_CLRF,         /* as SETF                                                          */
    OP_SETV,         /* var id                 value                                     */
    OP_ADDV,         /* var id                 delta (int16)                              */
    OP_BEAT,         /* script id hi           script id lo: run a BeatScript (yields)   */
    OP_WAIT,         /* frames                 -                                          */
    OP_HOLD,         /* frames                 - (cannot be skipped)                      */
    OP_WAIT_A,       /* min frames             -                                          */
    OP_PUSH,         /* PEND_* kind            arg: push a sub-scene; resume at the next
                                                 op with the result in var 0             */
    OP_POSSESS,      /* resident id lo         true-name id (authored possessor)          */
    OP_UNPOSSESS,    /* resident id lo         -                                          */
    OP_GIVE,         /* item id                count                                      */
    OP_TAKE,         /* item id                count                                      */
    OP_REWARD,       /* H$ / 10                item id; the unlock id follows as DATA     */
    OP_BEFRIEND,     /* resident id lo         delta                                      */
    OP_TELEPORT,     /* map id                 cell (x<<8 | y)            (VM_EXT)        */
    OP_CAST,         /* resident id lo         spot id | flags            (VM_EXT)        */
    OP_NPC,          /* actor                  act | arg<<8               (VM_EXT)        */
    OP_CAM,          /* CAM mode               arg                        (VM_EXT)        */
    OP_SFX,          /* -                      SFX_*                                      */
    OP_MUSIC,        /* -                      SONG_*                                     */
    OP_MUTE,         /* -                      frames                                     */
    OP_DUCK,         /* gain Q8                frames                                     */
    OP_TAPESTOP,     /* -                      frames                                     */
    OP_RESUME_ON_BEAT,
    OP_SHAKE,        /* SHK_*                  -                                          */
    OP_PUNCH,        /* dx (int8)              dy (int16)                                 */
    OP_FLASH,        /* colour                 level16 | frames<<8                        */
    OP_FADE,         /* -                      level16                                    */
    OP_IRIS,         /* heart                  r                                          */
    OP_TINT,         /* TINT_*                 level16                                    */
    OP_PAL_LERP,     /* PAL_* (camfx.h)        level16                                    */
    OP_INSERT,       /* insert id              frames (3-4)               (VM_EXT draw)   */
    OP_CUT,          /* -                      shot id                                    */
    OP_FREEZE_CAP,   /* caption style          text id                    (VM_EXT draw)   */
    OP_HOME,         /* HOME_* sub-op          arg (see below)                            */
    OP_QUEST,        /* -                      quest id | state<<12                       */
    OP_SET_BEAT,     /* -                      BEAT_* id                                   */
    OP_CALL,         /* -                      script id                                  */
    OP_RET,
    OP_RNG,          /* var id                 pool id: deterministic pick (story_rng)     */
    OP_LATER,        /* minutes                script id: schedule a callback (STRY.later) */
    OP_DATA,         /* -                      payload (inline operand of the op before)   */
    OP_TIME,         /* TIME_* (C14)           n                                           */
    OP_COUNT
};

/* OP_TIME a (C14): the .dsc [DAY n] / [NIGHT] codes. */
enum { TIME_SET_DAY, TIME_SET_NIGHT, TIME_ADVANCE };

/* OP_PUSH kinds. The first ten keep the values of the legacy enum in
 * core/src/story_int.h, which S2 deletes when it includes this header. */
#ifndef DOOMO_STORY_PEND_DEFINED
#define DOOMO_STORY_PEND_DEFINED
enum { PEND_NONE, PEND_CREATOR, PEND_PET, PEND_EXO_PRACTICE, PEND_EXORCISM,
       PEND_WORKSHOP, PEND_COMBAT, PEND_CUTSCENE, PEND_CREDITS, PEND_OTHER,
       /* ---- the VM's additions ---- */
       PEND_HOME,          /* SCENE_HOME, arg = house_api.h HOME_ARG_*           */
       PEND_COOKING, PEND_CONCERT, PEND_DREAM,
       PEND_TIMING,        /* SCENE_STORY_TIMING (S2, story_timing.c)            */
       PEND_MINIGAME_SIDE, /* SCENE_STORY_MG, arg = a story_mg registry id       */
       PEND_COUNT };
#endif

/* OP_HOME sub-ops (a). b is the argument. */
enum {
    HOME_SET_PHASE,      /* b = HP_* (home.h)                                   */
    HOME_PLACE_GIFT,     /* b = furn id; the giver comes from the catalogue      */
    HOME_REMOVE_ONLY,    /* b = 1 on / 0 off: the remove-only mode               */
    HOME_GOAL,           /* b = goal / stamp id; var 0 = met                     */
    HOME_DECORATE_FOR,   /* b = layout id | tolerance<<12; var 0 = progress 0..8 */
    HOME_HAUNT_STEP,     /* b = night                                            */
    HOME_DEFEND,         /* b = wave table id (a PUSH home in story mode)        */
    HOME_VISIT,          /* b = resident id (PUSH home arg 2 + id, H4; C14)      */
    HOME_GRANT,          /* b = furn id -> home_grant() (C14; trophies too, R8)  */
    HOME_PROP_GAG,       /* b = gag id -> HOME.pending_gag (C14)                 */
    HOME_SUBOP_COUNT
};

/* vm_tick() results. */
enum { VM_IDLE = 0,      /* no script running                                    */
       VM_RUN = 1,       /* running or waiting; call again next tick             */
       VM_PUSHED = 2,    /* a sub-scene was pushed; call vm_resume(result) after */
       VM_EXT = 3 };     /* a host-kind op is in vm_ext_op(); handle it, then
                            call vm_tick() again                                  */

#define VM_VARS 16
void vm_start(int script_id);
int  vm_tick(void);                  /* VM_*                                    */
void vm_resume(int result);          /* the pushed scene's result -> var 0       */
const StoryOp *vm_ext_op(void);      /* the op of the last VM_EXT, else NULL     */
int  vm_running(void);
int  vm_var(int i);
void vm_set_var(int i, int v);

/* Deterministic RNG for the story (SPEC C18): xorshift seeded from
 * doomo_town.seed ^ day ^ beat ^ ctx (never a save hash, never the wall
 * clock). Every caller passes a unique documented ctx constant. */
uint32_t story_rng(uint32_t ctx);
/* A hash of all story state (flags, STRY, VM vars, beat) for cross-platform
 * parity checks (I9). */
uint32_t story_fingerprint(void);

/* ---- story_mg registry (C14) -------------------------------------------
 * Side minigames run in SCENE_STORY_MG (shell: S2, story_verbs.c). Their
 * rows come from story_mg_a.c (S4) and story_mg_b.c (S5) through the weak
 * pointers below (NULL-name-terminated tables; weak NULL defaults in
 * stubs.c), read through a volatile pointer like every registry. */
typedef struct StoryMgEntry {
    uint16_t id;                     /* PUSH minigame_side arg                   */
    const char *name;                /* "Lava Hopscotch"                         */
    void (*enter)(int arg);
    int  (*update)(void);            /* returns 0 while running, else 1 + result */
    void (*draw)(void);
} StoryMgEntry;
extern const StoryMgEntry *const doomo_story_mg_a;   /* S4: story_mg_a.c */
extern const StoryMgEntry *const doomo_story_mg_b;   /* S5: story_mg_b.c */
int story_mg_count(void);
const StoryMgEntry *story_mg_at(int i);
const StoryMgEntry *story_mg_find(int id);

#endif
