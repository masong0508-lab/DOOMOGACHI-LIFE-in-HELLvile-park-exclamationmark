/* mayor.h - MAYOR GIGGS (turkey, Mad Hatter energy) for the pilot proof. */
#ifndef MAYOR_H
#define MAYOR_H

enum { MX_NEUTRAL, MX_HAPPY, MX_SHOCK, MX_SMUG, MX_SQUINT, MX_WORRY, MX_ANGRY };

typedef struct Mayor {
    float x, y;        /* world position of the feet            */
    float s;           /* world units per design unit           */
    float flip;        /* +1 faces right, -1 faces left         */
    float t;           /* animation clock (frames)              */
    float squash;      /* 1 = rest, >1 stretch, <1 squash       */
    float lean;        /* shear of the upper body (+ = forward) */
    float fan;         /* tail fan 0 folded .. 1 full display   */
    float flutter;     /* busy wing/tail flutter 0..1           */
    float armL, armR;  /* far / near arm angle (0 hang, 1.57 fwd, 3 up) */
    float grip;        /* finger spread                          */
    int   expr;        /* MX_*                                   */
    float blink;       /* 0 open .. 1 shut                       */
    float eye_pop;     /* 0..1 eyes bulge (shock)                */
    float look_x, look_y; /* pupil direction -1..1               */
    float brow_up;     /* 0..1                                   */
    float mouth;       /* beak open 0..1                         */
    float wattle;      /* extra wattle swing (radians), from springs */
    float head_dx, head_dy, head_tilt;
    float hat_tilt, hat_stretch, hair_kick;
    float step;
    float reach;       /* cartoon stretch of the near arm (design units) */        /* foot lift for walking/climbing         */
} Mayor;

void mayor_default(Mayor *m, float x, float y, float s, float t);
void mayor_draw(const Mayor *m);
extern float mayor_hand_x[2], mayor_hand_y[2];   /* [0] far, [1] near, world coords */
extern float mayor_head_x, mayor_head_y;
#endif
