/* lv_script.h - the level script VM (owner: levels, LV2; plan section 8.2).
 *
 * A level's scripted moments are bytecode in ROM (DoomoMapExt.script,
 * written by tools/mapc.py from `on EVENT do ACTION ; ACTION` lines) and
 * this small interpreter runs it: fixed tables, no allocation, no clock,
 * no rng. The same script and the same events give the same state and the
 * same host calls on every platform. Everything the VM cannot do itself
 * (doors, text, music, spawning ...) goes out through ONE host function,
 * so the game wires it once and a test records the calls.
 *
 * Script bytes:   [u8 handler count] then that many handlers:
 *     handler:    [event][a][b][flags][len]  then `len` bytes of actions
 *     action:     [op][operands ...]         (LV_OP_LEN[op] operand bytes)
 * A handler runs when its event happens: once (default), or every time
 * (LVSF_REPEAT). Its actions run in order, at most LV_VM_BUDGET actions a
 * tick over all handlers; what is left waits for the next tick.
 *
 * LVE_* and LVO_* are read by tools/mapc.py: append only.
 */
#ifndef DOOMO_LV_SCRIPT_H
#define DOOMO_LV_SCRIPT_H

#include "map.h"

#define LV_VM_VARS      32       /* byte variables                           */
#define LV_VM_HANDLERS  64       /* handlers in one script                   */
#define LV_VM_BYTES     2048     /* script size                              */
#define LV_VM_BUDGET    16       /* actions per tick                         */
#define LV_VM_QUEUE     16       /* handlers waiting to run                  */
#define LV_VM_HDR       5        /* bytes of a handler header                */

/* handler events: what a, b mean */
enum { LVE_NONE,
       LVE_START,    /* the level began                                      */
       LVE_TAG,      /* a = tag: a trigger / remote event fired              */
       LVE_CLEAR,    /* a = group: a kill-event group was cleared            */
       LVE_TIMER,    /* a | b << 8 = ticks (1..65535) since start; repeat: every N */
       LVE_ZONE,     /* a = tag: the player entered a zone with that tag     */
       LVE_USE,      /* a = thing index: a usable was used                   */
       LVE_VAR,      /* a = variable, b = value: the variable became b       */
       LVE_PHASE,    /* a = phase: a boss reached a phase                    */
       LVE_COUNT };

/* handler flags */
enum { LVSF_REPEAT = 1 };

/* actions (operand bytes in the comment). Also the first argument of the
 * host function for the ones that go out. */
enum { LVO_NOP,
       LVO_OPEN,        /* tag             remote door / lift open            (host) */
       LVO_CLOSE,       /* tag             and close                          (host) */
       LVO_SWAP,        /* set             apply a swap set                   (VM, then host: a = set, b = cells) */
       LVO_UNSWAP,      /* set             revert it                          (VM, then host) */
       LVO_ZONE,        /* zone, arg       set a zone's parameter             (host) */
       LVO_SPAWN,       /* group           wake a dormant group               (host) */
       LVO_CAPTION,     /* id lo, id hi    a caption line                     (host: a = id) */
       LVO_BARK,        /* id lo, id hi    a spoken line                      (host: a = id) */
       LVO_MUSIC,       /* cue             music cue                          (host) */
       LVO_TINT,        /* color, level    screen tint pulse                  (host) */
       LVO_CAM,         /* path, t lo, t hi, done tag   camera sweep: input frozen for t ticks
                         *                 (host each tick: a = path, b = progress 0..256);
                         *                 then tag `done` fires (0 = none)   */
       LVO_SET,         /* var, value      variable = value                   (VM) */
       LVO_ADD,         /* var, delta      variable += signed delta, clamped 0..255 (VM) */
       LVO_GIVE,        /* weapon          give a weapon                      (host) */
       LVO_TAKE,        /* weapon          take it                            (host) */
       LVO_CHECKPOINT,  /*                 save a checkpoint                  (host) */
       LVO_CHOICE,      /* var, id lo, id hi, tag0, tag1   offer a choice (ui_choice): host a = id,
                         *                 b = var; the answer comes back through lv_vm_choose() */
       LVO_FLAG,        /* index, on       set a story flag                   (host) */
       LVO_END,         /* result          end the level                      (host) */
       LVO_POST,        /* tag             fire a tag event                   (VM) */
       LVO_COUNT };

/* the host function: one call per action that leaves the VM */
typedef void (*LvHostFn)(void *ctx, int op, int a, int b);

/* lv_vm_begin / lv_script_check results (0 = fine) */
enum { LVERR_OK, LVERR_FORMAT, LVERR_SIZE, LVERR_OP, LVERR_OPERAND };
/* LvVm.err bits: a runtime budget was broken (deterministic: the extra is dropped) */
enum { LVVE_QUEUE = 1,     /* more than LV_VM_QUEUE handlers waiting      */
       LVVE_CHOICE = 2 };  /* a choice was offered while one was open     */

typedef struct LvVm {
    const DoomoMapExt *x;      /* the level's side tables and script      */
    DoomoMap          *map;    /* the live map (swap sets)                */
    LvHostFn           host;
    void              *ctx;
    uint32_t zone_in;          /* bit per zone: the player is inside      */
    uint16_t tick;             /* ticks since lv_vm_begin()               */
    uint16_t applied;          /* swap sets applied, a bit each           */
    uint16_t run_pc, run_end;  /* running handler: its next action, where its actions end */
    uint16_t cam_t, cam_n;     /* camera sweep: tick, length (0 = none)   */
    uint8_t  var[LV_VM_VARS];
    uint8_t  fired[LV_VM_HANDLERS / 8];   /* once-handlers that ran       */
    uint8_t  q[LV_VM_QUEUE];   /* handler indices waiting (ring)          */
    uint8_t  q_head, q_n;
    uint8_t  run_h;            /* running handler index, 255 = none       */
    uint8_t  cam_path, cam_tag;
    uint8_t  choice_var, choice_tag0, choice_tag1, choice_open;
    uint8_t  ended, result;    /* LVO_END ran; its result                 */
    uint8_t  err;              /* LVVE_*                                  */
} LvVm;

/* Check a script: structure, sizes, opcodes and operands (variables, swap
 * sets, zones and camera paths against `x` when it is given). Returns
 * LVERR_*. NULL / 0 is a fine empty script. */
int  lv_script_check(const uint8_t *s, int len, const DoomoMapExt *x);
/* Start a run: clears the state, checks the script of x, then fires
 * LVE_START. Returns LVERR_*; on an error the VM stays inert. */
int  lv_vm_begin(LvVm *vm, DoomoMap *map, const DoomoMapExt *x, LvHostFn host, void *ctx);
/* An event happened (LVE_TAG ... LVE_PHASE): queue the handlers it wakes. */
void lv_vm_event(LvVm *vm, int ev, int a);
/* One game tick: timers, zone entry (cx < 0: no player), the camera
 * sweep, then at most LV_VM_BUDGET actions. */
void lv_vm_tick(LvVm *vm, int cx, int cy);
/* 1 while a camera sweep freezes the player's input. */
int  lv_vm_frozen(const LvVm *vm);
/* The answer to an LVO_CHOICE: variable = pick (0 / 1), then that tag fires. */
void lv_vm_choose(LvVm *vm, int pick);
/* 1 when nothing is queued, running or sweeping. */
int  lv_vm_idle(const LvVm *vm);
/* A hash of the whole state: two runs that fed the same events must agree. */
uint32_t lv_vm_hash(const LvVm *vm);

#endif
