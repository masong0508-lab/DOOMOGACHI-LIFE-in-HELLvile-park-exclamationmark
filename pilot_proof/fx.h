/* fx.h - shared crowd and effects for the Act Two scenes (SC31-SC33 and on):
 * critter-rig extras with idle / frozen / panic / turn states, Puff Demons,
 * the possessed "verdict" text (the words just appear), and the cake table. */
#ifndef FX_H
#define FX_H
#include <stdint.h>

typedef struct CrowdOpts {
    int n;               /* extras */
    float x0, x1, y0, y1;/* world band they stand in */
    float t;             /* clock (frames) */
    float freeze_at;     /* frames: breathing stops in a ripple from x0 starting here (<0 never) */
    float ripple;        /* frames per world unit for the freeze ripple */
    float panic;         /* 0..1 running and sweating */
    float turn;          /* 0..1 all face camera-left (NOT NOW, PIP) */
    float eyes;          /* 0..1 eyes go wide */
    float scale;
    uint32_t seed;
    float avoid_x, avoid_r; /* keep a clear circle (Sunny) */
} CrowdOpts;
void crowd_draw(const CrowdOpts *o);

void puff_demon(float x, float y, float s, float t, int dir);
/* the possessed verdict: flat purple words, no type-on; Clinker's line 8 f late below */
void verdict(const char *text, float age_s, int y);
/* cake table at the foot of the bandstand (pie, cupcakes, cloth) */
void cake_table(float x, float y, float t, int pie_cut);
#endif
/* the Midsummer Giggle at dusk: sky, lantern strings, candy-striped stalls */
void fair_backdrop(float t, float x0, float x1);
void fair_stall(float x, float w, uint8_t stripe, const char *label, int label_on);
void fair_tent(float x, float w, float h, uint8_t stripe);
/* shared scene helpers: a bubble at a world point, and babble */
void say_at_world(const char *text, float ta, float t0, float secs, float wx, float wy, int side);
void babble_line(double t, float secs, float pitch, float vol);
