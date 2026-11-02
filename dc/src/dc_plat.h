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
 * The layout is the contract with the Flycast harness: dc/harness/dc_mailbox.h
 * (DC1), the same file on both sides. The harness finds `dc_mailbox` with
 * sh-elf-nm in the ELF and reads and writes it through the libretro
 * RETRO_MEMORY_SYSTEM_RAM region: offset = address & 0x00FFFFFF. misc.c
 * also exports the name without the SH ABI's leading underscore, so a
 * lookup by either spelling finds it.
 *
 * Log ring: plat_log() appends each line plus '\n' to log[] at
 * log_wr % DC_MB_LOG_SIZE; log_wr counts every byte ever written, so the
 * harness reads [its last position, log_wr) and knows when it fell behind
 * by more than the ring. Lines also go to dbgio (serial / emulator console).
 * Command ids (DC_HC_*) are the GBA's, 1..13, with the same meaning. */
#include "dc_mailbox.h"
extern volatile DcMailbox dc_mailbox;

void dc_harness_cmd(void);               /* hcmd.c */
void dc_harness_hold(void);              /* hcmd.c: "sync on" hold at the loop top */

/* "seed N" pins plat_entropy(); "expect-text" probes plat_text */
extern uint8_t  dc_entropy_pinned;
extern uint32_t dc_entropy_pin;
extern uint8_t  dc_text_armed;
void dc_text_probe(const char *s);

void dc_log(const char *msg);            /* misc.c: dbgio + the mailbox ring    */
void dc_logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* ---- boot easter egg (egg.c, X8) ------------------------------------------
 * Called once from main() after the video mode is up: a short rainbow when
 * A+B+X+Y+START are held at power-on. No effect otherwise. */
void dc_boot_egg(void);

/* ---- video (main.c) ------------------------------------------------------------ */
/* The background colour the PVR fills the frame with (plat_clear); the 2D
 * layer of DC3 draws over it. */
void dc_set_clear(uint8_t color);

#endif
