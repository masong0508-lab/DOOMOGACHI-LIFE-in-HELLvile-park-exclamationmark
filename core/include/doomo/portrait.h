/* portrait.h - procedural pixel art for residents (owner: portrait).
 *
 * ART DIRECTION: premium, lively, vaguely furry plush critter-folk -
 * tamagotchi / Tomodachi Life charm at late-GBA quality. Huge heads, tiny
 * round plush bodies, fluffy fur tufts, expressive animal ears and tails,
 * little muzzles and button noses, paw hands, big glossy two-highlight
 * eyes, blush, soft 3-4 tone hue-shifted shading with coloured selective
 * outlines. Demonic parts (horns, wings, fangs) are drawn *adorable*. Only
 * POSSESSED / SOUL_LOST residents look properly creepy (hollow red eyes,
 * veins, drippy black tears, glitch + smoke aura, drained colour) - that
 * contrast is the whole joke. GHOST residents are pale translucent cyan
 * with a wispy tail; DEAD ones get X eyes.
 *
 * Every catalog option of every list visibly changes the result (the
 * portrait test checks it). Everything is drawn at runtime from the look:
 * integer-only, no allocation, bounded cost (a bust is ~5-9k shaded pixel
 * spans/visits, about 0.8M cycles on the GBA; the caches below hide it).
 * See docs/PORTRAIT.md.
 */
#ifndef DOOMO_PORTRAIT_H
#define DOOMO_PORTRAIT_H

#include "types.h"
#include "resident.h"

#define PORTRAIT_W 48
#define PORTRAIT_H 56
#define BODYSPR_W  16    /* overworld chibi sprite */
#define BODYSPR_H  24

enum { EXPR_NEUTRAL, EXPR_HAPPY, EXPR_SAD, EXPR_ANGRY, EXPR_LOVE,
       EXPR_SHOCK, EXPR_SLEEPY, EXPR_SICK, EXPR_POSSESSED, EXPR_DEAD,
       EXPR_TALK, EXPR_COUNT };

/* Render a bust portrait (head + shoulders + accessories + aura) into
 * out[PORTRAIT_W*PORTRAIT_H]. `status` (RES_*) switches possessed/ghost
 * looks. `anim` is a tick counter for blinking / aura animation (blink,
 * breathing, ear twitch, tail wag, particles; EXPR_TALK flaps the mouth). */
void portrait_render(const DoomoLook *l, int status, int expr, int anim,
                     uint8_t *out);
/* Full-body chibi for the overworld/apartments: dir 0=down,1=left,2=right,
 * 3=up; frame 0..3 walk cycle. out[BODYSPR_W*BODYSPR_H]. */
void bodyspr_render(const DoomoLook *l, int status, int dir, int frame,
                    uint8_t *out);
/* Full-body large figure for the creator preview (whole outfit, tail,
 * wings): out[FIGURE_W*FIGURE_H]. */
#define FIGURE_W 48
#define FIGURE_H 80
void figure_render(const DoomoLook *l, int status, int expr, int anim,
                   uint8_t *out);

/* Cached DoomoImages for residents (keeps uid stable until the look,
 * status, expression or animation phase changes - i.e. until the pixels
 * change). Small LRU cache: one 18 KB pool shared by all four kinds
 * (portrait 7 blocks, figure 10, fighter 4, chibi 1 block of 384 bytes);
 * pointers stay valid until a later call evicts them. It holds at least 6
 * portraits, 12 fighters or 48 chibis at once, so draw an image right
 * after asking for it. The cached portrait / figure animate by themselves
 * (blink, breathing, ear twitch, tail wag, aura, mouth flaps) from
 * doomo_frame_count(), staggered per resident. A new look / expression /
 * status is drawn at once; a new animation phase is re-drawn in slices
 * (on the GBA one layer group, <= ~0.23M cycles, per tick, and a few ticks
 * ahead of time) while the last phase stays on screen, so animating
 * portraits never stall a frame. An empty slot gives a lavender "?"
 * silhouette. */
const DoomoImage *portrait_image(int resident_id, int expr);
const DoomoImage *bodyspr_image(int resident_id, int dir, int frame);
const DoomoImage *figure_image(const DoomoLook *l, int status, int expr);
/* Third-person shooter body: the resident's full look (outfit, hair,
 * horns, wings, tail) holding a weapon, seen from `view` (0 back,
 * 1 back-left, 2 left, 3 front-left, 4 front, mirrored for right side via
 * BB_FLIPX), anim 0..3 run cycle, 4 = fire, 5 = roll, 6 = hurt.
 * weapon_frame is the WFRAME_* of the held weapon (shape in hand). */
#define FIGHTER_W 32
#define FIGHTER_H 48
const DoomoImage *fighter_image(int resident_id, int view, int anim,
                                int weapon_frame);
void portrait_invalidate(int resident_id);   /* after editing a look */

/* ---- portrait additions (tools, tests, previews) ----------------------- */
/* Uncached fighter render (same as fighter_image's pixels). */
void fighter_render(const DoomoLook *l, int status, int view, int anim,
                    int weapon_frame, uint8_t *out);
/* Pixel visits of the last render (cost check in tests / tools). */
uint32_t portrait_last_ops(void);
/* Drop every cached image and return to the real clock (tests). */
void portrait_cache_reset(void);
/* Tests / tools: run the cache on a fake clock `tick` (instead of
 * doomo_frame_count) with `steps` layer groups of phase re-rendering per
 * tick (0 = the platform default; the GBA uses 1). */
void portrait_cache_clock(uint32_t tick, int steps);

/* (integration, memory budget) Small images of other modules kept in the
 * portrait cache's block pool (LRU with the portraits) instead of a RAM
 * cache of their own - e.g. the weld icons (24x16, one block each).
 * Returns the image for `key` (any hash of what the pixels show), at most
 * PORTRAIT_POOL_IMAGE_MAX bytes; *fresh = 1 when it was (re)allocated and
 * the caller must draw its pixels now (cast away the const). Valid until
 * the pool reuses it: draw it in the same frame. */
#define PORTRAIT_POOL_IMAGE_MAX (PORTRAIT_W * PORTRAIT_H)
const DoomoImage *portrait_pool_image(uint32_t key, int w, int h, int *fresh);

#endif
