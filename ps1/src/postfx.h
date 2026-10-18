/* postfx.h - the PS1 comic post-FX (owner: S1; docs/design/story/SPEC.md
 * sec.7 S1, platform.h S0 block). ps1/src/postfx.c implements the
 * plat_post_* hooks, ps1/src/xform.c plat_image_xform.
 *
 *   offset  two GP0(E5h) draw offsets: the frame's first one (patched,
 *           gfx.c) moves everything drawn before camfx_apply(); a second
 *           one appended at that point puts the HUD back
 *   scale   GTE H: the zoom is latched and multiplies the projection
 *           distance of the next frame's 3D views (overworld.c); 2D-only
 *           frames do not zoom
 *   blend   full-screen ABR TILEs (add for white, subtract for black,
 *           subtract + add for a colour; solid at 16)
 *   pal     the master CLUT and the flat-colour table (g_rgb) lerped with
 *           camfx_pal_entry; restored on the first frame without a lerp
 *   iris    the core's software row spans (plat_rect TILEs)
 */
#ifndef PS1_POSTFX_H
#define PS1_POSTFX_H

#include <stdint.h>

void postfx_frame_begin(void);       /* gfx_begin(): per-frame reset         */
int  postfx_zoom_q8(void);           /* GTE H multiplier, 256 = none         */
/* gfx.c: patch the frame's first E5 by (dx, dy) and append an E5 that
 * restores the plain offset (everything chained later is steady). */
void gfx_post_offset(int dx, int dy);

#endif
