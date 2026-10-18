/* camfx.h - full-screen comic post effects: shake, punch, zoom, flash,
 * fade, iris and palette lerps. The core keeps the state; the platform
 * applies it in hardware where it can, through the plat_post_* hooks in
 * platform.h (weak software defaults in core/src/plat_weak_fx.c).
 *
 * CONTRACT (docs/design/story/SPEC.md sec.4): written by S0, maintained by S1
 * (add-only). Implementation: core/src/camfx.c (S1). Until it lands every
 * function is a weak stub in core/src/stubs.c.
 *
 * Options scale amplitude only, never timing (CT sec.9): OPT_SCREEN_SHAKE
 * (0/50/100%), OPT_REDUCE_FLASH (caps flashes), OPT_UI_BOUNCE. Full-screen
 * flashes stay under 3 Hz.
 */
#ifndef DOOMO_CAMFX_H
#define DOOMO_CAMFX_H

#include "types.h"

/* The 14 shake profiles (CT Appendix C; data: {frames, xmode, ymode, yc,
 * amp[14]}, about 250 B of ROM). Append only. */
enum { SHK_TAP, SHK_BRAKE, SHK_POW, SHK_IMPACT, SHK_STAMP, SHK_STAMP3, SHK_SLAM,
       SHK_IGNITE, SHK_SHOTGUN, SHK_FWOOMP, SHK_CRACK, SHK_RUMBLE, SHK_ROAR,
       SHK_GUNFIRE, SHK_COUNT };

/* Palette lerps (SF sec.15.5): a 256-entry palette lerp (GBA: DMA at vblank;
 * PS1: a CLUT swap). PAL_LILAC is the one-frame lilac of W13. */
enum { PAL_LERP_NONE, PAL_SEPIA, PAL_CRAYON, PAL_HELLIFY, PAL_LILAC, PAL_LERP_COUNT };

void camfx_reset(void);                        /* all effects off (scene enter)   */
void camfx_shake(int profile);                 /* x OPT_SCREEN_SHAKE (0/50/100%)  */
void camfx_punch(int dx, int dy);              /* TW_SLAM4 spring kick            */
void camfx_zoom_snap(int scale_q8, int frames);/* GBA BG2 affine; PS1: GTE H      */
void camfx_flash(uint8_t color, int level16, int frames); /* REDUCE_FLASH caps it */
void camfx_fade(int level16);                  /* 0 clear .. 16 black             */
void camfx_iris(int cx, int cy, int r, int heart, uint8_t rim);
void camfx_iris_off(void);
void camfx_pal_lerp(int pal_id, int level16);  /* PAL_* above, 0..16              */
void camfx_tick(void);                         /* once per 30 Hz tick             */
void camfx_offset(int *dx, int *dy);           /* for software paths              */
/* Push the current state to the plat_post_* hooks (called once per drawn
 * frame by whoever owns the frame, after the scene's draw). */
void camfx_apply(void);

#endif
