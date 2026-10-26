/* cast.h - the cast library. The Mayor lives in mayor.c/h (from the cold
 * open); the others follow the same pattern: world position, scale, a clock
 * and a few acting controls, drawn natively at any zoom with wd_tube limbs. */
#ifndef CAST_H
#define CAST_H
#include "mayor.h"

typedef struct Sunny {     /* silent bunnykin newcomer: acts with ears, eyes and posture */
    float x, y, s, t, flip;
    float ears;            /* 0 droop .. 1 up */
    float look_x, look_y, blink, squash;
    float page;            /* 0 no page; >0 holds up the soggy page (0..1 raise) */
    int suitcase;
} Sunny;
void sunny_default(Sunny *u, float x, float y, float s, float t);
void sunny_draw(const Sunny *u);

typedef struct Bones {     /* Bonejangles: skeleton bandleader who plays his own ribs */
    float x, y, s, t, flip;
    float jaw;             /* 0..1 */
    float hitL, hitR;      /* mallet swing 0 rest .. 1 striking */
    float squash;
} Bones;
void bones_default(Bones *b, float x, float y, float s, float t);
void bones_draw(const Bones *b);
#endif
