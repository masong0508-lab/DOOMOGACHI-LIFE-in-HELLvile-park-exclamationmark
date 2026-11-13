/* plat_stub.c - placeholders for the platform calls the later Dreamcast
 * packages implement (owner: dreamcast).
 *
 * DC2 was the skeleton: the game booted, ran its 30 Hz loop and changed
 * scenes, but drew nothing except the clear colour. Each group below is
 * replaced, and this file shrinks, as the packages land:
 *   2D layer, post effects, held frame  -> DC3: done (dc2d.h, gfx2d.c, ...)
 *   saves on the VMU                -> DC4: done (vmusave.c)
 *   3D view                         -> DC6
 *   house                           -> DC8: done (room.c)
 *   overworld                       -> DC7: done (overworld.c, dc3d.c, pvr3d.c)
 *   sound                           -> DC5: done (audio.c, mixer.c, sfx.c)
 */
#include "dc_plat.h"

/* ---- display (the logical screen, dc2d.h) ------------------------------------------------------------ */
int plat_screen_w(void) { return DC_SCREEN_W; }
int plat_screen_h(void) { return DC_SCREEN_H; }

/* every 2D call, the post effects and plat_hold_frame: gfx2d.c (DC3) */

/* ---- 3D and the world renderers (DC6; the overworld is DC7's, the house DC8's) ---- */
/* plat_view3d: view3d.c (DC6) */
/* plat_overworld: overworld.c (DC7); plat_room, plat_room_project: room.c (DC8) */

/* plat_tone, plat_sample and the music hooks: audio.c (DC5) */

/* saves, the save scratch and the config record: vmusave.c (DC4, the VMU) */
