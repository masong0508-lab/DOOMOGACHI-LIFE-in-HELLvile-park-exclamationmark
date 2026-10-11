/* portrait.h - procedural pixel art for residents (owner: portrait).
 *
 * ART DIRECTION: extremely cutesy - think tamagotchi / Tomodachi Life on a
 * GBA. Huge heads, tiny bodies, chunky 1px dark outlines, big shiny eyes,
 * blush, pastel shading. Demonic parts (horns, wings, fangs) are drawn
 * *adorable*. Only POSSESSED / SOUL_LOST residents look properly creepy
 * (hollow red eyes, veins, dripping, glitchy aura) - that contrast is the
 * whole joke.
 *
 * Every catalog option must visibly change the result.
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
 * looks. `anim` is a tick counter for blinking / aura animation. */
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
 * status or expression changes). Small LRU cache; pointers stay valid
 * until the next call that evicts them (at least 8 live entries). */
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

#endif
