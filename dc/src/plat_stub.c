/* plat_stub.c - placeholders for the platform calls the later Dreamcast
 * packages implement (owner: dreamcast).
 *
 * DC2 was the skeleton: the game booted, ran its 30 Hz loop and changed
 * scenes, but drew nothing except the clear colour. Each group below is
 * replaced, and this file shrinks, as the packages land:
 *   2D layer, post effects, held frame  -> DC3: done (dc2d.h, gfx2d.c, ...)
 *   saves on the VMU                -> DC4: done (vmusave.c)
 *   3D view, overworld, house       -> DC6 / DC7 / DC8
 *   sound                           -> DC5: done (audio.c, mixer.c, sfx.c)
 */
#include "dc_plat.h"

/* ---- display (the logical screen, dc2d.h) ------------------------------------------------------------ */
int plat_screen_w(void) { return DC_SCREEN_W; }
int plat_screen_h(void) { return DC_SCREEN_H; }

/* every 2D call, the post effects and plat_hold_frame: gfx2d.c (DC3) */

/* ---- 3D and the world renderers (DC6 / DC7 / DC8) ----------------------------- */
void plat_view3d(const DoomoView3D *v) { (void)v; }
void plat_overworld(const DoomoOverworldView *v) { (void)v; }

/* plat_tone, plat_sample and the music hooks: audio.c (DC5) */

/* saves, the save scratch and the config record: vmusave.c (DC4, the VMU) */
