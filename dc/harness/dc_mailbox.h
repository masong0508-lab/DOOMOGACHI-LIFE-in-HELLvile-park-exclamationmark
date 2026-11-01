/* dc_mailbox.h - the RAM mailbox shared by the Dreamcast game and the
 * headless harness (owner: dreamcast).
 *
 * The game defines one `volatile DcMailbox dc_mailbox;` (DC2: dc/src) and
 * fills it; dc/harness/dcrun finds the symbol with sh-elf-nm (or takes
 * --mailbox ADDR) and reads and writes it through the core's
 * RETRO_MEMORY_SYSTEM_RAM (offset = addr & 0xFFFFFF; 16 MB of main RAM at
 * 0x8C000000). The layout mirrors gba/src/gba_plat.h's GbaMailbox so the
 * script languages stay the same, plus a log ring and the last scene,
 * because the Dreamcast has no debug-register log like mGBA's.
 *
 * Both sides are little-endian (SH4 in its default mode, x86/ARM hosts),
 * so the harness reads the fields as they are. Only fixed-size types: the
 * offsets are the same in sh-elf-gcc and the host gcc (checked by the
 * _Static_asserts below on both).
 */
#ifndef DC_MAILBOX_H
#define DC_MAILBOX_H

#include <stdint.h>

#define DC_MAILBOX_MAGIC 0x424D4344u     /* "DCMB" in memory order          */
#define DC_MB_LOG_SIZE   4096u           /* power of two                    */

typedef struct DcMailbox {
    uint32_t magic;                      /* DC_MAILBOX_MAGIC once booted    */
    uint32_t ticks;                      /* doomo_frame() calls (30 Hz)     */
    uint32_t drawn;                      /* frames actually presented       */
    uint32_t fps10;                      /* engine test: fps x 10           */
    /* harness commands: the harness writes arg (and name), then cmd; the
     * game runs it between two logic ticks, writes result and clears cmd.
     * result follows the GBA: save/load/verify/scene/push/seed give
     * nonzero on success, exists gives nonzero when the slot holds a save */
    uint32_t cmd, arg, result, cycles;
    /* profiling: microseconds of the logic ticks' work, sum/count/worst */
    uint32_t work_sum, work_n, work_max;
    char     name[20];                   /* DC_HC_SCENE / _PUSH / TEXT_ARM  */
    /* "sync on": the loop holds at its top while ticks == hold_at
     * (~0 = never), as gba_harness_hold() does */
    uint32_t hold_at;
    uint32_t draw_all;                   /* nonzero: draw every tick        */
    char     scene[24];                  /* last scene_set() name, NUL-ended */
    /* plat_log ring: byte i of the stream is log[i % DC_MB_LOG_SIZE];
     * log_wr counts every byte ever written (lines end with '\n'). The
     * harness polls it every video frame, so it must not fall more than
     * DC_MB_LOG_SIZE bytes behind (it reports a "log overrun" if it does). */
    uint32_t log_wr;
    char     log[DC_MB_LOG_SIZE];
} DcMailbox;

_Static_assert(sizeof(DcMailbox) == 100 + DC_MB_LOG_SIZE, "DcMailbox layout");

/* Command ids: the same numbers as the GBA's (gba_plat.h GBA_HC_* and
 * gba/src/hcmd.h), so hcmd.c can be shared in spirit. */
enum {
    DC_HC_NONE = 0, DC_HC_SAVE, DC_HC_LOAD, DC_HC_EXISTS, DC_HC_TOWN_FILL,
    DC_HC_TOWN_CRC, DC_HC_VERIFY, DC_HC_SCENE, DC_HC_PUSH,
    DC_HC_SEED = 9, DC_HC_FLAG = 10, DC_HC_TEXT_ARM = 11, DC_HC_TEXT_HIT = 12,
    DC_HC_STORY_FP = 13
};

#endif
