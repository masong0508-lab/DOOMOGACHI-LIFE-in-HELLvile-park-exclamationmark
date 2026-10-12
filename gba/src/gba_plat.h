/* gba_plat.h - internal interfaces between the GBA platform modules
 * (owner: gba). The public contract is core/include/doomo/platform.h. */
#ifndef GBA_PLAT_H
#define GBA_PLAT_H

#include "gba_hw.h"
#include "doomo/types.h"
#include "doomo/platform.h"

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

/* ---- shared big buffer (bigbuf.c) -------------------------------------- */
/* gba_big (bigbuf.c): one 17 KB EWRAM block used in turn by the
 * overworld's tile cache, the raycaster's buffers and the save code's
 * stream window; whoever uses it claims it first, which invalidates the
 * previous owner's cache. */
#define GBA_BIG_SIZE   (17u * 1024u)
#define GBA_BIG_OW     0u               /* overworld lit tiles  < 17 KB */
#define GBA_BIG_CLIPS  0u               /* r3d clip checkpoints < 8 KB  */
#define GBA_BIG_ROOF   (8u * 1024u)     /* r3d roof table         4 KB  */
#define GBA_BIG_CMT    (12u * 1024u)    /* (end of the roof table)      */
#define GBA_BIG_SPR    (12u * 1024u)    /* r3d sprite order       1 KB  */
extern uint8_t gba_big[];
enum { GBA_BIG_FREE, GBA_BIG_BY_OW, GBA_BIG_BY_R3D, GBA_BIG_BY_SAVE };
void gba_big_claim(int who);

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
