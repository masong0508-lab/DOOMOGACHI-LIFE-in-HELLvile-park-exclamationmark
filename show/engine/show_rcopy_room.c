/* show_rcopy_room.c - the game's 2.5D house renderer, compiled for the show
 * (owner: show, ENGINE lane 3; spec 4.3, 8.2, 8.3).
 *
 * ps1/src/room.c is compiled here a second time, unchanged (#included,
 * never copied or edited), with its exported names renamed, so the show
 * reaches the renderer's private state (`R`): the game eases its camera
 * with a TW_BOING spring, smooths the zoom and the look-at, and draws a
 * resting room through a backdrop cached in VRAM. A show frame must be
 * the same whatever came before it (a cut, a seek, the previous shot), so
 * show_room_render() drops that history and always takes the single full
 * pass (RM_LIVE): the camera is exactly the keyed yaw / zoom / pan. The
 * game's own plat_room() stays in ps1/src/room.c, unused by the show
 * (--gc-sections drops it).
 */
#include "doomo/doomo.h"

#define plat_room show_room_plat_game
#define room_render show_room_render_game
#include "../../ps1/src/room.c"
#undef plat_room
#undef room_render

void show_room_render(const DoomoRoomView *v);

void show_room_render(const DoomoRoomView *v)
{
    R.have = 0;                  /* the spring snaps to the yaw, the walls settle */
    R.zoom_q8 = 0;               /* the zoom and the look-at snap                  */
    R.still = 0;
    R.bd_ok = 0;
    R.last_key = 0;
    R.prev_mode = RM_LIVE;
    R.time = (uint16_t)(v->time - 64);   /* dt > 40: every "since last call" resets */
    show_room_render_game(v, RM_LIVE);
}
