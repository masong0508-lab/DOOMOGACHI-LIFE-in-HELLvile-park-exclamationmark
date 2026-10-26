/* cast.h - the cast library. The Mayor lives in mayor.c/h (from the cold
 * open); the others follow the same pattern: world position, scale, a clock
 * and a few acting controls, drawn natively at any zoom with wd_tube limbs. */
#ifndef CAST_H
#define CAST_H
#include "mayor.h"
#include <stdint.h>

typedef struct Sunny {     /* silent bunnykin newcomer: acts with ears, eyes and posture */
    float x, y, s, t, flip;
    float ears;            /* 0 droop .. 1 up */
    float look_x, look_y, blink, squash;
    float page;            /* 0 no page; >0 holds up the soggy page (0..1 raise) */
    int suitcase;
    int no_page;           /* raise the paw without the page (holding a muffin, key...) */
} Sunny;
extern float sunny_hand_x, sunny_hand_y;
void sunny_default(Sunny *u, float x, float y, float s, float t);
void sunny_draw(const Sunny *u);

typedef struct Bones {     /* Bonejangles: skeleton bandleader who plays his own ribs */
    float x, y, s, t, flip;
    float jaw;             /* 0..1 */
    float hitL, hitR;      /* mallet swing 0 rest .. 1 striking */
    float squash;
    float look;            /* skull turn -1..1 (eye sockets slide) */
    float possess;         /* 0..1 purple possession glow (dims as water lands) */
} Bones;
void bones_default(Bones *b, float x, float y, float s, float t);
void bones_draw(const Bones *b);

typedef struct Nana {      /* Nana Puddin: sheepkin head baker, all wool and flour */
    float x, y, s, t, flip;
    float mouth, blink, look_x, squash;
    float armL, armR;      /* 0 hang .. 1.57 forward .. 3 up (far, near) */
    int buttons;           /* cardigan buttons showing (4 = all) */
} Nana;
void nana_default(Nana *n, float x, float y, float s, float t);
void nana_draw(const Nana *n);
extern float nana_hand_x[2], nana_hand_y[2], nana_mouth_x, nana_mouth_y;

/* one rig for the critter-folk cast (Fuzz, Bloom, Pip, ...): parts swapped by flags */
enum { CR_EARS_ROUND, CR_EARS_LONG, CR_EARS_NONE };
enum { CX_COLLAR = 1, CX_STETHO = 2, CX_SASH = 4, CX_FLOPS = 8, CX_SATCHEL = 16, CX_BUN = 32 };
typedef struct Critter {
    float x, y, s, t, flip;
    uint8_t fur, furd, outfit, outfitd, outfitl;
    int ears, extras;
    float mouth, blink, look_x, look_y, squash, kneel;
    float armL, armR;      /* 0 hang .. 1.57 forward .. 3 up */
    float ear_turn;        /* -1..1 ears swivel */
} Critter;
void critter_draw(const Critter *c);
extern float critter_hand_x[2], critter_hand_y[2], critter_mouth_x, critter_mouth_y, critter_head_y;
Critter cast_fuzz(float x, float y, float t);
Critter cast_bloom(float x, float y, float t);
Critter cast_pip(float x, float y, float t);
void pet_draw(float x, float y, float s, float t, float look_x, int beep);
#endif
