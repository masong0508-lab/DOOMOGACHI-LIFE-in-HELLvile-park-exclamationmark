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
void gba_pacing_reset(void);             /* forget frame debt (after loads) */
void plat_init_hw(void);

#define GBA_MAILBOX_MAGIC 0x42474D44u    /* "DMGB" */
typedef struct GbaMailbox {
    uint32_t magic;                      /* GBA_MAILBOX_MAGIC once booted  */
    uint32_t ticks;                      /* doomo_frame() calls (30 Hz)    */
    uint32_t drawn;                      /* frames actually presented      */
    uint32_t fps10;                      /* engine test: fps x 10          */
} GbaMailbox;
extern volatile GbaMailbox gba_mailbox;
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
