/* weld_gfx_int.h - the weapon view cache, shared by the two builds of
 * weld_gfx.c: the plain one (weld_view_image: combat, the library, every
 * scene but the workshop) and weld_slice.c's (S2-9: the same pictures
 * composed in slices, weld_view_image_sliced). Not part of the contract. */
#ifndef DOOMO_WELD_GFX_INT_H
#define DOOMO_WELD_GFX_INT_H

#include "doomo/weld.h"
#include "doomo/save_ext.h"

#define WELD_SCRATCH_TAG SAVE_EXT_TAG('W', 'E', 'L', 'D')
typedef uint8_t WeldViewPix[WELD_VIEW_W * WELD_VIEW_H];

/* The WELD_VIEW_SLOTS slots are the scene scratch (scene.h, tag WELD):
 * weld_view_pix[i], (re)claimed by whoever finds it gone, which empties
 * every key. A slot's key is 0 while it holds no finished picture.
 * weld_gfx_gen counts weld_gfx_invalidate() calls. */
#define WELD_VIEW_SLOTS 2
extern WeldViewPix *weld_view_pix;
extern DoomoImage   weld_view_img[WELD_VIEW_SLOTS];
extern uint32_t     weld_view_key[WELD_VIEW_SLOTS], weld_view_used[WELD_VIEW_SLOTS];
extern uint32_t     weld_view_clock;          /* counts the askings (the slots' last use) */
extern uint32_t     weld_gfx_gen;

#endif
