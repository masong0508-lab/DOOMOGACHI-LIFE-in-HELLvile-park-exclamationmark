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
    /* ---- S2 additions (append only) ---- */
    OP_JMP,          /* -                      label: jump                                */
    OP_NATIVE,       /* callback id (CB_*)     arg: run the host's C glue by id (VM_EXT;
                        the legacy port's beat logic, story_script.c)                     */
    OP_STASH,        /* slot (0..3)            text id: decode into the resident caption
                        stash (txt_stash, SPEC C5) for the scene pushed next              */
    OP_COUNT
};

/* OP_DATA tags (a): what the inline operand b is for. */
enum { VMD_CMP,          /* JLT / JGE: the value compared with                     */
       VMD_LINE,         /* SAY: DL_* flags | EXPR_* << 8                          */
       VMD_WHO,          /* a 16-bit speaker / resident (VM_WHO_* or an id)        */
       VMD_OPT,          /* CHOICE: an option's text id                            */
       VMD_LABEL,        /* CHOICE: an option's jump label (0xFFFF: continue)      */
       VMD_UNLOCK,       /* REWARD: the unlock id                                  */
       VMD_ARG,          /* CAST: the extra argument                               */
       VMD_SCRIPT };     /* (S2) a NATIVE verb's continuation script id            */
/* OP_SAY a: 0..0xFB a resident id, or one of these. */
enum { VM_SAY_TARGET = 0xFC,  /* story_fmt_target (the conversation's subject) */
       VM_SAY_BIG = 0xFD,     /* a VMD_WHO DATA op follows                      */
       VM_SAY_NAR = 0xFE, VM_SAY_PLAYER = 0xFF };
/* 16-bit who codes (VMD_WHO): 0..999 residents, DOOMO_PLAYER_ID, and: */
#define VM_WHO_NAR    0xFFFFu
#define VM_WHO_TARGET 0xFFFDu

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

/* --- S2: the side-mission verbs (story_verbs.c; docs/STORY_ENGINE.md) ---
 * `verb KIND [WHO] [cell=x,y] [fragile] then=SCRIPT` in a script: an
 * OP_NATIVE with id STORY_CB_VERB + kind, b = WHO, a VMD_ARG DATA op (the
 * cell x | y << 8, VERB_ARG_FRAGILE) and a VMD_SCRIPT DATA op. The verb
 * runs on the overworld while the VM is free (the plot can still talk);
 * when it ends, SCRIPT runs with v1 = the result. */
#define STORY_CB_VERB 160             /* .. 160 + VERB_COUNT - 1 (story.c cb ranges) */
enum { VERB_ESCORT,      /* WHO follows you to cell; 1 arrived, 0 lost them      */
       VERB_FIND,        /* walk onto the hidden cell (warmer / colder hints); 1  */
       VERB_PHOTO,       /* A with WHO in front of you, 2-6 cells: 2 framed, 1 ok */
       VERB_DELIVER,     /* carry it to WHO (slower; fragile: no running); 2
                            perfect, 1 delivered with spills                      */
       VERB_COUNT };
#define VERB_ARG_FRAGILE 0x8000u      /* VMD_ARG bit for VERB_DELIVER             */
/* SCENE_STORY_MG built-in games (ids above the S4/S5 registry ids). */
#define STORY_MG_TEXT_ENTRY 0xFF01u   /* type the stashed answer (stash 1) to the
                                         stashed prompt (stash 0); 1 right     */
/* SCENE_STORY_TIMING (story_timing.c): arg = TPAT_* | (TSKIN_* | TIMING_DRIFT)
 * << 8; result = the rounds landed, 0..3 (the stars). */
enum { TPAT_THREE,       /* the rule of three: 3-1, 3-1-2, 3-1-2-1               */
       TPAT_KNOCK,       /* Interlude III's knocks: 2-2-1, 2-1-1, 1-1-2          */
       TPAT_PRAYER,      /* Hattie / HAZE's prayer: 4-4, 4-2-2, 2-2-2-2          */
       TPAT_RIMSHOT,     /* ba-dum tss: 2-1, 1-1-1, 3-2-1                        */
       TPAT_COUNT };
enum { TSKIN_THUMP, TSKIN_LAUGH, TSKIN_BELL, TSKIN_HEART, TSKIN_COUNT };
#define TIMING_DRIFT 0x80             /* the metronome drifts (S155): listen, don't count */
#define TIMING_ARG(pat, skin) ((pat) | ((skin) << 8))
/* SCENE_DREAM's scripted mode (dream.c): the story's own dreams, D1 / D2.
 * The script stashes up to 3 captions (and the wake-up line in slot 3). */
enum { DREAM_SCRIPTED_D1, DREAM_SCRIPTED_D2, DREAM_SCRIPTED_COUNT };
#define DREAM_ARG_SCRIPTED(n) (0x4000 | (n))
/* --- end S2 --- */

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
/* Text in SAY / CHOICE lines may use @P (player), @E (pet), @T (the
 * target's name), @D (its demon), @V (the flavour of the day), @0..@9 (a
 * VM var as a number) and @@. */
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

/* --- S2: the VM's host API, the formatter and the generated tables --- */
/* vm_start() binds the script; the host's first vm_tick() (in the same
 * frame) queues its opening SAY lines at once, so a caller's later
 * ui_say / ui_choice lands after them. */
void vm_stop(void);
/* The operands of the last VM_EXT / VM_PUSHED op: its resident (a VMD_WHO
 * DATA, decoded: -1 narrator) and its VMD_ARG / VMD_UNLOCK operand. */
int  vm_ext_who(void);
int  vm_ext_arg(void);
/* (S2) Any DATA operand of the last VM_EXT / VM_PUSHED op by tag (VMD_*). */
int  vm_ext_data(int tag, int dflt);
int  vm_waiting_dialog(void);        /* 1 while the VM waits for the box     */
/* Format a line (the codes above) into out; vm_fmt_n takes @0..@9 from
 * nums[] instead of the VM vars. */
void vm_fmt(char *out, int cap, const char *in);
void vm_fmt_n(char *out, int cap, const char *in, const int *nums, int n);
/* The subject of @T / @D (story_fmt_target in the story group). */
extern int16_t vm_fmt_target;
/* The resident id a VM who code / OP_SAY byte means (-1 narrator). */
int  vm_who(unsigned code16);
/* Scheduled "Later" callbacks (STRY.later, C9/C14): run from the host's
 * minute tick; returns the script id due now, or 0. */
int  vm_later_due(void);

/* The side missions S100..S217 (core/gen/side_table.c, from @mission). */
#define STORY_SIDE2_ROWS 118
typedef struct StorySideRow {
    uint8_t  sid;            /* mission number - 100                           */
    uint8_t  kind;           /* 0 retired / unwritten, 1 live                   */
    uint8_t  ch_lo, ch_hi;   /* first and last chapter it is offered in         */
    uint8_t  side;           /* 0 Hillvile, 1 Hellvile, 2 the park, 3 both      */
    uint8_t  chain;          /* 'A'..'Z', 0 = a stand-alone                     */
    uint8_t  step;           /* part k of its chain (1..)                       */
    uint8_t  requires;       /* 1 + the sid it requires, 0 none                 */
    uint8_t  later_min;      /* the Later callback, minutes after turn-in       */
    uint8_t  tag;
    uint16_t giver;          /* resident id                                     */
    uint16_t verbs;          /* 1 << verb (docs/STORY_ENGINE.md)                */
    uint16_t reward;         /* H$                                              */
    uint16_t offer;          /* script id run by the giver (none / offered), 0xFFFF none */
    uint16_t active;         /* script id run by the giver while active, 0xFFFF none */
    uint16_t title;          /* text id, 0xFFFF none                            */
    uint16_t trophy;         /* text id of its trophy (R8), 0xFFFF none          */
    uint8_t  pack;           /* the pack its scripts live in                    */
} StorySideRow;
/* The quest log's Trophies (R8): the rows whose mission is done (STRY.side2
 * state 3) and that name a trophy, in row order; returns the count. */
int  story_trophies(const StorySideRow *rows, const uint8_t *side2, uint8_t *out, int max);
extern const StorySideRow doomo_side_rows[STORY_SIDE2_ROWS];
/* The side walker (story_side.c): chain gating (step k requires k-1; at
 * most one part of a chain per chapter; cross-border parts 3+ from Ep 4.1).
 * Returns 1 if row `sid` may be offered now. */
int  story_side2_available(int sid);
int  story_side2_state(int sid);     /* 0 none, 1 offered, 2 active, 3 done (STRY.side2) */
void story_side2_set_state(int sid, int st);
/* The pure rule (tests): state = STRY.side2 bits, chapter, ep41 = Ep 4.1 reached. */
int  story_side2_gate(const StorySideRow *rows, int sid, const uint8_t *side2,
                      int chapter, int ep41, const uint8_t *done_in_chapter);

/* Authored possessors: core/gen/possess_table.c, from the @possessor
 * blocks, in exorcism.h's DoomoPossessor format (S3). While no .dsc has
 * an @possessor block the file defines no table, and exorcism.c's weak
 * default rows hold.
 *
 * The armed slots are not saved (STRY is full), so the story re-arms them
 * on every resume (story_rearm_possessors, story.c): the @armed rows say in
 * which beats row `row` is the story's possession of `host` (from..to in
 * the story order, unless a flag says it is over). A possessed host inside
 * its window is re-armed; any other possession is the sim's own. */
typedef struct StoryArmed {
    uint16_t host;           /* resident id                                       */
    uint8_t  row;            /* the authored possessor row (exorcism.h)           */
    uint8_t  from, to;       /* BEAT_* ids, inclusive, in the story order         */
    uint8_t  pad;
    uint16_t unless[2];      /* story flags that end the window, 0xFFFF none      */
} StoryArmed;
extern const StoryArmed doomo_story_armed[];
extern const uint16_t doomo_story_armed_count;
/* 1 if `a` covers story position `beat` with the flags as they are (pure). */
int  story_armed_window(const StoryArmed *a, int beat);
/* Clear the armed slots and re-arm every possessed host whose @armed
 * window covers the current beat (a load, a debug jump, a new game). */
void story_rearm_possessors(void);
/* The authored row a story host is possessed by: its first row in the
 * possessor table that is not a reverse rite, -1 none. */
int  story_possess_row_of(int host);
/* --- end S2 --- */

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
