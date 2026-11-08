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
    /* DC3: the 2D layer (dc/src/dc2d.h, dc2d_stats) after each drawn frame:
     * prims and vertex bytes sent to the PVR (last frame, worst so far),
     * prims dropped over the list or vertex budget (total), texture uploads
     * and evictions (total), textures cached now */
    uint32_t gfx_prims, gfx_prims_max, gfx_vbytes, gfx_vbytes_max;
    uint32_t gfx_dropped, gfx_uploads, gfx_evictions, gfx_tex_live;
    /* DC4: plat_test_flags() (plat_ext.h PLAT_TEST_*). The harness sets
     * PLAT_TEST_NO_PICKER as soon as it sees the magic (dcrun --console
     * leaves 0, a real console's value), so the first boot of an empty VMU
     * skips the language picker in scripted runs, as the native runner does */
    uint32_t test_flags;
    /* DC5: the sound (dc/src/audio_int.h DcSndStats, audio.c) after each
     * tick: tick slots rendered / begun playing, underruns (the ring empty
     * while playing, not during a known pause), overruns (a tick pushed into
     * a full ring), slots skipped to catch up, the fullest ring, output
     * frames of content and of silent gaps, clipped samples, the FNV-1a
     * hash of the rendered stream (now, and frozen after 600 slots = 20 s),
     * the mixer's time per tick on the main thread in microseconds (sum,
     * ticks, worst), the sound effects started (sample voices,
     * plat_sample), the longest interval between two AICA refills in
     * microseconds, and the refills later than the whole AICA buffer (the
     * AICA played stale data: heard as a glitch) */
    uint32_t snd_pushed, snd_consumed, snd_underruns, snd_overruns;
    uint32_t snd_dropped, snd_queue_max, snd_gap_frames, snd_frames;
    uint32_t snd_clips, snd_hash, snd_hash600;
    uint32_t snd_us_sum, snd_us_n, snd_us_max;
    uint32_t snd_started, snd_cb_gap_max, snd_starved;
    /* DC7: the 3D views (dc/src/dc3d.h, dc3d_stats) after each drawn frame:
     * the time the views took to build in microseconds (last, worst, sum
     * and count of the views built), polygons of the last view and the
     * most in one view, the most TA bytes one view cost, and the polygons
     * left out over the batch or its budget (total) */
    uint32_t g3d_us, g3d_us_max, g3d_us_sum, g3d_n;
    uint32_t g3d_polys, g3d_polys_max, g3d_bytes_max, g3d_dropped;
} DcMailbox;

_Static_assert(sizeof(DcMailbox) == 236 + DC_MB_LOG_SIZE, "DcMailbox layout");

/* Command ids: the same numbers as the GBA's (gba_plat.h GBA_HC_* and
 * gba/src/hcmd.h), so hcmd.c can be shared in spirit. */
enum {
    DC_HC_NONE = 0, DC_HC_SAVE, DC_HC_LOAD, DC_HC_EXISTS, DC_HC_TOWN_FILL,
    DC_HC_TOWN_CRC, DC_HC_VERIFY, DC_HC_SCENE, DC_HC_PUSH,
    DC_HC_SEED = 9, DC_HC_FLAG = 10, DC_HC_TEXT_ARM = 11, DC_HC_TEXT_HIT = 12,
    DC_HC_STORY_FP = 13,
    /* 14 and 15 are the GBA's house commands (HC_ROOM_PARITY / _TEST) */
    DC_HC_TEXT_FEED = 16,                /* ui_text_entry_feed(arg): the native
                                          * runner's `type`, one char per call */
    DC_HC_NATIVE_SEED = 17,              /* the native runner's `--seed arg` start
                                          * (dcrun --native-seed, at tick 0) */
    /* DC7: the native runner's story commands (side_sample_a.txt) */
    DC_HC_SIDE = 18,                     /* side M S: arg = M | S << 16 */
    DC_HC_TALK = 19,                     /* talk ID: story_interact_npc(arg) */
    DC_HC_CELL = 20,                     /* cell X Y: arg = X | Y << 16 */
    DC_HC_STORY_STATE = 21,              /* result 1 + 1 (talking) + 2 (STORY on top) */
    DC_HC_SAYS = 22,                     /* result 2 when the dialogue's top line
                                          * contains name (its first 19 chars), else 1 */
    DC_HC_TOP_IS = 23,                   /* result 2 when the top scene is name, else 1 */
    DC_HC_CLOCK = 24                     /* clock HH MM: the town clock = arg minutes */
};

#endif
