/* show_portrait.h - the show's portrait override channels (spec 4.3).
 *
 * Read by the patched copies of core/src/portrait.c (hunks (b), (b'), (c))
 * and portrait_head.c (hunk (d)); defined in show/engine/show_actor.c.
 * -1 means "the engine's own value", so with both at -1 every image is
 * the game's (AT1.3). The mouth is folded into the image cache key, so a
 * changed mouth is new content and renders at once.
 *
 * Draw each image (plat_image) right after its fetch: a later fetch may
 * evict it from the pool (patch hunk (g)); the GPU packet already holds
 * a copy of the pixels, so drawing first is always safe.
 */
#ifndef SHOW_PORTRAIT_H
#define SHOW_PORTRAIT_H

#include <stdint.h>

extern int8_t portrait_show_mouth;      /* MOUTH_* (0..MOUTH_COUNT-1) or -1 */
extern int8_t portrait_show_blink;      /* 0 open, 1 shut, or -1           */

/* set both (out-of-range values mean -1); call show_portrait_overrides(-1, -1)
 * after the fetch */
void show_portrait_overrides(int mouth, int blink);

#endif
