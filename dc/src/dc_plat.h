/* dc_plat.h - internal interfaces between the Dreamcast platform modules
 * (owner: dreamcast). The public contract is core/include/doomo/platform.h.
 *
 * DC2 is the skeleton: boot, the 30 Hz frame loop, the harness mailbox and
 * the disc image. The 2D layer (DC3), input and VMU saves (DC4), audio
 * (DC5) and 3D (DC6) replace the placeholders in plat_stub.c.
 */
#ifndef DC_PLAT_H
#define DC_PLAT_H

#include <stdint.h>
#include "doomo/types.h"
#include "doomo/platform.h"

/* The core's logical screen: 320x240, drawn 2x on the 640x480 video mode
 * (docs/DREAMCAST.md section 2). */
#define DC_SCREEN_W 320
#define DC_SCREEN_H 240

/* ---- main / timing (main.c) ------------------------------------------------- */
extern volatile uint32_t dc_vblanks;     /* incremented by the vblank handler   */
extern uint32_t dc_frames;               /* doomo_frame() calls (logic ticks)   */
extern int dc_nodraw;                    /* 1: this tick's drawing is skipped   */
void plat_init_hw(void);
uint32_t dc_read_pad(void);              /* BTN_* mask (main.c)                 */
void dc_pacing_reset(void);              /* forget frame debt (after loads)     */
uint32_t dc_us(void);                    /* microseconds since boot (wraps)     */

/* ---- the harness mailbox (misc.c) ----------------------------------------------
 * The Flycast harness (dc/harness, DC1) finds `dc_mailbox` with sh-elf-nm in
 * the ELF (or the .map) and reads and writes it through the libretro
 * RETRO_MEMORY_SYSTEM_RAM region: offset = address & 0x00FFFFFF. The first
 * part mirrors gba/src/gba_plat.h's GbaMailbox field for field (same
 * offsets up to draw_all), so the GBA harness's script engine carries over;
 * the Dreamcast fields follow. Layout changes bump DC_MAILBOX_VERSION.
 *
 * Log ring: plat_log() appends each line plus '\n' to log[] at
 * log_head % DC_LOG_RING; log_head counts every byte ever written, so the
 * harness reads [its last head, log_head) and knows when it fell behind by
 * more than the ring. Lines also go to dbgio (serial / emulator console). */
#define DC_MAILBOX_MAGIC   0x43444D44u   /* "DMDC" */
#define DC_MAILBOX_VERSION 1u
#define DC_LOG_RING        4096u          /* power of two */
typedef struct DcMailbox {
    uint32_t magic;                      /* DC_MAILBOX_MAGIC once booted        */
    uint32_t ticks;                      /* doomo_frame() calls (30 Hz)         */
    uint32_t drawn;                      /* frames actually presented           */
    uint32_t fps10;                      /* (engine test) fps x 10              */
    /* harness commands (hcmd.c): the harness writes arg (and name), then
     * cmd; the game runs it between two logic ticks, writes result, clears
     * cmd. `cycles` is the time it took, in microseconds on the DC. */
    uint32_t cmd, arg, result, cycles;
    /* profiling: microseconds of each drawn tick's work (plat_begin_frame
     * .. plat_end_frame, before the vblank wait): sum, count, worst */
    uint32_t work_sum, work_n, work_max;
    char name[20];                       /* DC_HC_SCENE / _PUSH / _TEXT_ARM arg */
    /* harness "sync on": the loop stops at its top once hold_at ticks have
     * run and serves commands there until the harness moves hold_at on.
     * ~0 (set at boot) = never. */
    uint32_t hold_at;
    uint32_t draw_all;                   /* nonzero: no catch-up ticks          */
    /* ---- Dreamcast additions (version 1) ---- */
    uint32_t version;                    /* DC_MAILBOX_VERSION                  */
    uint32_t vblanks;                    /* vblank interrupts since boot        */
    uint32_t video;                      /* vid_check_cable(): 0 VGA, 2 RGB, 3 composite */
    uint32_t boot_us;                    /* microseconds from main() to the loop */
    char     scene[24];                  /* name of the scene last set/pushed   */
    uint32_t log_head;                   /* bytes ever written to log[]         */
    char     log[DC_LOG_RING];
} DcMailbox;
extern volatile DcMailbox dc_mailbox;

/* harness command ids: 1..13 are the GBA's (gba_plat.h + gba/src/hcmd.h)
 * with the same numbers and meaning; the GBA renderer self-tests (14..16)
 * have no Dreamcast counterpart yet */
enum {
    DC_HC_NONE, DC_HC_SAVE, DC_HC_LOAD, DC_HC_EXISTS, DC_HC_TOWN_FILL,
    DC_HC_TOWN_CRC, DC_HC_VERIFY, DC_HC_SCENE, DC_HC_PUSH,
    DC_HC_SEED = 9, DC_HC_FLAG = 10, DC_HC_TEXT_ARM = 11, DC_HC_TEXT_HIT = 12,
    DC_HC_STORY_FP = 13
};
void dc_harness_cmd(void);               /* hcmd.c */
void dc_harness_hold(void);              /* hcmd.c: "sync on" hold at the loop top */

/* "seed N" pins plat_entropy(); "expect-text" probes plat_text */
extern uint8_t  dc_entropy_pinned;
extern uint32_t dc_entropy_pin;
extern uint8_t  dc_text_armed;
void dc_text_probe(const char *s);

void dc_log(const char *msg);            /* misc.c: dbgio + the mailbox ring    */
void dc_logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* ---- video (main.c) ------------------------------------------------------------ */
/* The background colour the PVR fills the frame with (plat_clear); the 2D
 * layer of DC3 draws over it. */
void dc_set_clear(uint8_t color);

#endif
