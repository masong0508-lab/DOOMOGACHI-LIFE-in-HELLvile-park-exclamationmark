/* room_int.h - the GBA dream-house renderer's private interface (owner: H2;
 * docs/design/story/SPEC.md sec.7 H2, C7, C9, C22; HF sec.6).
 *
 * gba/src/room.c implements plat_room() and plat_room_ui_opaque() for the
 * GBA; hcmd.c drives its harness hooks (parity hashes, test layouts).
 */
#ifndef GBA_ROOM_INT_H
#define GBA_ROOM_INT_H

#include "gba_plat.h"

/* the house's owner id and region: GBA_BIG_BY_ROOM, GBA_BIG_ROOM, gba_room
 * (gba_plat.h, the arena; G0) */

/* The harness parity frame (SPEC H2 acceptance, HF sec.11.5): the
 * test_home_render.c layout drawn at 240x160 exactly as the room_ref golden
 * draws it, then FNV-1a of the 38,400 bytes of the back page.
 *   arg bits 0-1  view k (yaw = k * 1024)
 *   arg bit  4    night (daylight 3: the golden's view 4)
 *   arg bits 8-11 path: 0 a full recompose;
 *                       1 the dirty-rect path: the frame is first drawn
 *                         with moved / extra items (actors, the cursor, a
 *                         ghost, an animated frame, a squash), then the
 *                         parity state is drawn incrementally over it;
 *                       2 the UI restore path: a window and text are drawn
 *                         over the room, then the parity state again.
 * Every path must give the golden hash. */
uint32_t room_hc_parity(uint32_t arg);

/* Harness test layouts for the live SCENE_HOME (the GBA build keeps no
 * HOME state before G0, so the scene shows an empty template room): the
 * renderer replaces the view's furniture / actors with a fixed layout while
 * armed. 0 off; ROOM_TEST_*. */
enum { ROOM_TEST_OFF, ROOM_TEST_STEADY, ROOM_TEST_CELLAR, ROOM_TEST_DEFEND, ROOM_TEST_COUNT };
void room_hc_test(uint32_t mode);

/* Forget every page and cache (after a harness command drew on the page). */
void room_invalidate(void);

/* CPU cycles of the last drawn plat_room() call, and how it went (kept in
 * the gba_big claim; logged by the harness hooks). */
typedef struct RoomStats {
    uint32_t cycles;          /* gba_cycles() spent in plat_room           */
    uint16_t full, rects;     /* 1 = full recompose; dirty rects drawn     */
    uint16_t blocks, items;   /* dirty 16x8 blocks; items in the draw list */
} RoomStats;

#endif
