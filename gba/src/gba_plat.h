/* gba_plat.h - internal interfaces between the GBA platform modules
 * (owner: gba). The public contract is core/include/doomo/platform.h. */
#ifndef GBA_PLAT_H
#define GBA_PLAT_H

#include "gba_hw.h"
#include "doomo/types.h"
#include "doomo/platform.h"
#include "doomo/scene.h"

#define SCREEN_W 240
#define SCREEN_H 160

/* ---- main / timing (main.c) ------------------------------------------- */
extern volatile uint32_t gba_vblanks;      /* incremented by the VBlank IRQ */
extern uint32_t gba_frames;                /* doomo_frame() calls          */
extern uint32_t gba_keys;                  /* raw KEYINPUT bits (1 = held)  */
/* 1 while the current doomo_frame() is a catch-up tick whose drawing is
 * skipped (the renderer could not keep 30 fps). All plat_* drawing calls
 * return immediately when set. */
extern int gba_nodraw;
uint32_t gba_read_pad(void);             /* BTN_* mask incl. SYNTH_XY flag */
void cutview_enter(void);                 /* debug cutscene viewer (cutview.c) */
void gba_pacing_reset(void);             /* forget frame debt (after loads) */
uint32_t gba_stack_used(void);          /* main stack high-water mark, bytes */
void plat_init_hw(void);

#define GBA_MAILBOX_MAGIC 0x42474D44u    /* "DMGB" */
typedef struct GbaMailbox {
    uint32_t magic;                      /* GBA_MAILBOX_MAGIC once booted  */
    uint32_t ticks;                      /* doomo_frame() calls (30 Hz)    */
    uint32_t drawn;                      /* frames actually presented      */
    uint32_t fps10;                      /* engine test: fps x 10          */
    /* harness commands (game ROM, hcmd.c): the harness writes arg, then
     * cmd; the ROM runs it between two logic ticks, writes result and
     * clears cmd */
    uint32_t cmd, arg, result, cycles;
    /* profiling (harness "fps"): CPU cycles of the logic ticks' work
     * (plat_begin_frame .. plat_end_frame, before the vblank wait) of the
     * drawn ticks: sum, count, worst; the harness reads and resets them */
    uint32_t work_sum, work_n, work_max;
    char name[20];                       /* GBA_HC_SCENE / _PUSH argument   */
} GbaMailbox;
extern volatile GbaMailbox gba_mailbox;
enum { GBA_HC_NONE, GBA_HC_SAVE, GBA_HC_LOAD, GBA_HC_EXISTS, GBA_HC_TOWN_FILL,
       GBA_HC_TOWN_CRC, GBA_HC_VERIFY, GBA_HC_SCENE, GBA_HC_PUSH };
void gba_harness_cmd(void);              /* hcmd.c */
void gba_irq_init(void) IWRAM_CALL;          /* irq.iwram.c */
/* Cycle counter: timers 2+3 cascaded at 16.78 MHz. */
static inline uint32_t gba_cycles(void) {
    uint16_t hi, lo, hi2;
    do { hi = REG_TM3D; lo = REG_TM2D; hi2 = REG_TM3D; } while (hi != hi2);
    return ((uint32_t)hi << 16) | lo;
}
void gba_log(const char *msg);
void gba_logf(const char *fmt, ...);   /* tiny printf: %d %u %x %s %c   */

/* ---- video (video.c) ---------------------------------------------------- */
/* The page being drawn (not displayed). 240x160 bytes, halfword access only. */
extern uint16_t *gba_back;
void video_init(void);
void video_load_palette(void);
void video_flip(void);                   /* wait for vblank pacing + flip */

/* ---- the arena (bigbuf.c; G0, task #77) --------------------------------- */
/* ONE EWRAM block holds, in turn or side by side, what used to be three:
 * the core's scene scratch (12 KB), the platform's 17 KB gba_big and the
 * core's per-frame billboard list (2.4 KB) - 32,096 B, now 26,992 B:
 *
 *   [0, claim)              the scene scratch: scene_scratch(owner, size)
 *                           claims `size` <= DOOMO_SCENE_SCRATCH from 0
 *   ... free ...
 *   [BB - bytes, BB)        the region of the platform owner that claimed
 *                           it last (gba_big_claim): the overworld's lit
 *                           tiles, the raycaster's buffers, the house
 *   [BB, GBA_ARENA_SIZE)    doomo_frame_bb (filled and read inside a draw)
 *   [claim, GBA_ARENA_SIZE) plat_save_scratch(): saves run in an update,
 *                           when no billboards are live
 *
 * A scene owns [0, claim) and nothing more; whatever a platform owner keeps
 * in its region is a cache that is rebuilt when another owner or a save
 * took the memory, or (overworld, house) after a new scratch claim
 * (scene_scratch_gen()). The r3d region fits beside the largest claim
 * (12 KB: the weapon views of combat), so claims never cost it its roof
 * table; the overworld's tile cache (17 KB) fits beside the story
 * overworld's 6.5 KB and uses only its upper half beside a larger claim;
 * the house needs a claim <= 7.7 KB (the house scene's is 2.8 KB). Measured
 * fps-neutral on every scene of the G0 tour (docs/GBA.md section 3). */
/* (plain integers: bigbuf.c hands them to the assembler too) */
#define GBA_ARENA_SIZE 26992
#define GBA_BB_BYTES   2400             /* DOOMO_FRAME_BB_MAX billboards (asserted) */
#define GBA_ARENA_BB   (GBA_ARENA_SIZE - GBA_BB_BYTES)
extern uint8_t gba_arena[];
/* a platform owner's region: the `bytes` right below the billboards */
#define GBA_BIG(bytes) (gba_arena + GBA_ARENA_BB - (bytes))
#define GBA_BIG_R3D    11840            /* the raycaster's region:        */
#define GBA_BIG_CLIPS  0                /*   clip checkpoints     6,720 B */
#define GBA_BIG_ROOF   6720             /*   roof table           4,096 B */
#define GBA_BIG_CMT    10816            /*   (end of the roof table)      */
#define GBA_BIG_SPR    10816            /*   sprite order         1,024 B */
#define GBA_BIG_OW     17152            /* the overworld's lit tiles (32 x 2 x 268) */
#define GBA_BIG_ROOM   16872            /* the house: RBig + harness RTest */
/* the hot regions' bases as plain symbols (bigbuf.c), so the renderers'
 * addressing is the same as with a block of their own */
extern uint8_t gba_r3d[];               /* == GBA_BIG(GBA_BIG_R3D) */
extern uint8_t gba_ow[];                /* == GBA_BIG(GBA_BIG_OW)  */
extern uint8_t gba_room[];              /* == GBA_BIG(GBA_BIG_ROOM) */
enum { GBA_BIG_FREE, GBA_BIG_BY_OW, GBA_BIG_BY_R3D, GBA_BIG_BY_SAVE, GBA_BIG_BY_ROOM };
void gba_big_claim(int who);
/* 1 if a region of `bytes` clears the scene scratch's current claim */
int  gba_big_fits(uint32_t bytes);
/* scene_scratch.c: the claimed size (0: none) and a counter that changes
 * on every claim */
uint32_t scene_scratch_used(void);
uint32_t scene_scratch_gen(void);

/* ---- 3D (r3d*.c) --------------------------------------------------------- */
void r3d_init(void);
typedef struct R3DStats {
    uint32_t cycles_total, cycles_cols, cycles_planes, cycles_sprites;
    uint16_t planes, spans, events_overflow, columns;
} R3DStats;
extern R3DStats r3d_stats;

/* ---- overworld (overworld.c) --------------------------------------------- */
void ow_init(void);

/* ---- audio (audio.c, mixer in irq.iwram.c) ------------------------------- */
void audio_hw_init(void);

/* ---- save (save_flash.c) ------------------------------------------------- */
void flash_init(void);
int  flash_detected(void);               /* 0 none, else chip size in KB */
const char *flash_chip_name(void);

#endif
