/* show_mbox.h - g_show_mbox, the player <-> harness mailbox (spec 8.9).
 * Owner: ENGINE lane 1. The Lua fork (showtk/emu/harness_show.lua) reads it
 * at the byte offsets in the comments; ADD-ONLY at the end, and change the
 * Lua's offset table in the same commit.
 *
 * Protocol (lockstep, one capture per tick):
 *   - Lua writes req_* while the game is stopped at the top of its loop.
 *   - The next tick: show_mbox_poll() takes the request (seek = bind the shot,
 *     pre-roll to req_frame with drawing skipped), draws that frame, and at the
 *     end of the tick publishes cur_shot / cur_frame / P1 / P4 / stats for the
 *     frame JUST DRAWN, bumping `seq`. At the following stop, g_hcmd.shot_y is
 *     the VRAM row of that frame, so the Lua crop and this record agree.
 *   - Playback then continues frame by frame through the episode; after the
 *     last frame of req_last_shot (or of the episode) state becomes DONE and
 *     the player draws black and publishes cur_shot = -1.
 */
#ifndef SHOW_MBOX_H
#define SHOW_MBOX_H

#include <stdint.h>

#define SHOW_MBOX_MAGIC 0x424d4853u     /* 'SHMB' */
#define SHOW_MBOX_ABI   1
#define SHOW_P4_MAX     14

enum { SHOW_ST_BOOT = 0, SHOW_ST_CHART = 1, SHOW_ST_PLAY = 2, SHOW_ST_DONE = 3,
       SHOW_ST_ERROR = 4 };

typedef struct ShowMbox {
    uint32_t magic;                         /*   0 'SHMB'                       */
    uint16_t abi, canvas_w, canvas_h, y_off;/*   4                              */
    volatile int32_t req_shot, req_frame;   /*  12, 16 seek (-1 = none)         */
    int32_t  cur_shot, cur_frame;           /*  20, 24 the frame just drawn     */
    uint32_t p1_state_crc;                  /*  28                              */
    uint32_t p2_draw_crc;                   /*  32 deferred: always 0           */
    uint16_t p4_n, p4_k[SHOW_P4_MAX];       /*  36, 38..65                      */
    uint16_t pad0;                          /*  66                              */
    uint32_t cyc_update, cyc_draw, cyc_sync;/*  68, 72, 76 microseconds (timer_us) */
    uint32_t prims, prim_bytes, uploads, evictions, pool_blocks_used; /* 80..99 */
    /* ---- v1 extensions ---- */
    uint32_t seq;                           /* 100 frames published             */
    volatile int32_t req_episode;           /* 104 0 = the default (101)        */
    volatile int32_t req_last_shot;         /* 108 stop after this shot (-1 = end) */
    uint32_t state;                         /* 112 SHOW_ST_*                    */
    uint32_t ep_frame;                      /* 116 episode frame just drawn     */
    uint32_t nshots, total_frames;          /* 120, 124 of the loaded episode    */
    uint32_t warn_n, err_n;                 /* 128, 132                          */
    uint32_t sprites, user0;                /* 136, 140                          */
    char     shot_id[8];                    /* 144 of the frame just drawn       */
    uint32_t load_us;                       /* 152 last chunk load               */
    uint32_t p4_overflow;                   /* 156 events beyond SHOW_P4_MAX     */
    uint32_t reserved[4];                   /* 160..175                          */
} ShowMbox;                                 /* 176 B                             */

extern ShowMbox g_show_mbox;

#endif
