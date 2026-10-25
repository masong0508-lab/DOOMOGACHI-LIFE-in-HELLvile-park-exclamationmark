/* room_int.h - between the PS1 house renderer (room.c) and its harness
 * layouts (room_test.c) (owner: H3).
 *
 * The harness layouts only run under `room-test N` (hcmd.c), so on the game
 * disc they are a part of the house's overlay group of their own
 * (ps1/tools/ovl_plan.py PARTS "roomtest"): plat_room() asks for it
 * (scene.h plat_code_part) before it calls room_test_render().
 */
#ifndef PS1_ROOM_INT_H
#define PS1_ROOM_INT_H

#include "doomo/platform.h"

/* harness hook (hcmd.c `room-test N`): 0 off, else ROOM_TEST_* */
extern volatile uint8_t g_room_test;
enum { ROOM_TEST_OFF, ROOM_TEST_STEADY, ROOM_TEST_ORBIT, ROOM_TEST_CELLAR, ROOM_TEST_DROP,
       ROOM_TEST_DEFEND, ROOM_TEST_PARITY_LIVE, ROOM_TEST_PARITY_BD, ROOM_TEST_PARITY2_LIVE,
       ROOM_TEST_PARITY2_BD, ROOM_TEST_COUNT };

/* the frame's render path: one full pass, the backdrop drawn again, or
 * drawn back (room.c); force: one of them for the parity pairs, -1 = choose */
enum { RM_LIVE, RM_FILL, RM_REPLAY };
void room_render(const DoomoRoomView *v, int force);

/* room_test.c: the scene's view with the fixed layout of g_room_test */
void room_test_render(const DoomoRoomView *v);

#endif
