/* postfx.h - the GBA comic post-FX and the affine-OBJ squash path
 * (owner: S1, docs/design/story/SPEC.md sec.7 S1, C7.4-C7.8, R18).
 *
 * gba/src/postfx.c: plat_post_offset/scale/blend, plat_pal_lerp. The core's
 *   camfx_apply() calls them while the frame is built; they only latch
 *   state, which video_flip() (main.c) applies in the vblank of the flip:
 *   BG2X/Y + BG2PA-PD (shake, zoom), BLDCNT/BLDY (white / black flash and
 *   fade), palette RAM (lerps and coloured flashes, computed straight into
 *   palette RAM in the vblank, no buffer). The state is per frame: a frame
 *   that does not call them shows a clean screen. The iris stays the core's
 *   software spans (core/src/plat_weak_fx.c).
 * gba/src/obj.c: plat_image_xform through affine OBJs for up to
 *   OBJ_XF_SLOTS squashed hero actors per frame, an OAM shadow, and
 *   obj_set_shadow() for scenes that need more OBJs (the house, H2).
 */
#ifndef GBA_POSTFX_H
#define GBA_POSTFX_H

#include "gba_plat.h"

#define DCNT_OBJ_1D 0x0040            /* REG_DISPCNT: 1D OBJ tile mapping   */

/* ---- video_flip() hooks (main.c) ----------------------------------------
 * postfx_pre_flip(): before the vblank wait (OBJ overlap checks, OAM and
 * affine composition with the frame's shake / zoom). postfx_vblank(): in
 * the vblank right after the page flip (registers, OAM DMA, palette). */
void postfx_pre_flip(void);
void postfx_vblank(void);

/* The frame's post transform, for the OBJ composition: screen p of bitmap
 * point q is c + s * (q - c) + d. Returns 0 when there is none. */
int  postfx_xform(int *dx, int *dy, int *s_q8, int *cx, int *cy);

/* The shared per-frame 2D bbox accumulator (C7.6): code that draws UI over
 * the bitmap may report its rects here; obj.c also checksums the bitmap
 * under each squash OBJ, so callers that do not report are still safe. */
void postfx_bbox_add(int x, int y, int w, int h);
int  postfx_bbox(int *x0, int *y0, int *x1, int *y1);   /* 0 = empty      */

/* ---- OBJ VRAM ownership (C7.4): r3d's texture cache vs obj.c ----------- */
enum { OBJV_R3D, OBJV_OBJ };
int      gba_objvram_claim(int who);  /* previous owner; r3d.c          */
uint32_t gba_objvram_r3d_frame(void); /* gba_frames of r3d's last claim   */
int      gba_objvram_owner(void);

/* ---- OAM (obj.c) --------------------------------------------------------- */
#define OBJ_XF_SLOTS 4                /* squashed hero actors per frame     */
#define OBJ_SHADOW_N 16               /* own shadow: entries + affine 0..3  */
void obj_init(void);                  /* hide all 128 OAM entries (boot)    */
/* Use buf (n entries of 4 halfwords, n <= 128) as the OAM shadow; NULL
 * returns to the own 16-entry shadow. A buffer in gba_big must be dropped
 * (obj_set_shadow(NULL, 0)) before the block changes hands (C7.8). Entries 0..OBJ_XF_SLOTS-1 and the attr3 words of entries
 * 0..15 (affine groups 0..3) belong to plat_image_xform; others are free. */
void obj_set_shadow(uint16_t *buf, int n);
uint16_t *obj_shadow(int *n);
void obj_pre_flip(void);              /* postfx_pre_flip's OBJ half         */
void obj_vblank(void);                /* OAM DMA                            */
int  obj_active(void);                /* 1 when a squash OBJ is shown        */

#endif
