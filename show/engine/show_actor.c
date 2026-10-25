/* show_actor.c - actors: busts, figures, chibis (owner: show; spec 8.4).
 *
 * WP1 ships only the two override channels the portrait patches read
 * (spec 4.3 hunks (b)-(d)): the player sets them around one actor's image
 * fetch and puts them back to -1 right after. With patch (f) every fetch
 * finishes inside the call, so a value never leaks into another actor.
 * WP6 adds the actor slots, clocks and tracks here.
 */
#include "doomo/doomo.h"
#include "show/show_portrait.h"

int8_t portrait_show_mouth = -1;    /* MOUTH_* or -1: the expression's own */
int8_t portrait_show_blink = -1;    /* 0 open, 1 shut, -1: the clock's own */

void show_portrait_overrides(int mouth, int blink)
{
    portrait_show_mouth = (int8_t)(mouth >= 0 && mouth < MOUTH_COUNT ? mouth : -1);
    portrait_show_blink = (int8_t)(blink >= 0 ? (blink != 0) : -1);
}
