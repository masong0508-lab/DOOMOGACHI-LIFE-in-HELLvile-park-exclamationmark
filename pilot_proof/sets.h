/* sets.h - sets and props for the pilot proof (world space, see wd.h). */
#ifndef SETS_H
#define SETS_H
#include "tk.h"

/* bandstand geometry (world units; ground y = 0, up is negative) */
#define BAN_X0   (-70.0f)
#define BAN_X1   (-2.0f)
#define BAN_Y0   (-71.0f)
#define BAN_Y1   (-49.0f)
#define LET_PX   1.7f
#define LET_X0   (-64.75f)
#define LET_Y0   (-66.8f)
#define N_CX     (-40.1f)
#define N_CY     (-60.0f)
#define LADDER_X (-74.0f)
#define STEP_Y   (-30.0f)
#define FOUNT_X  (-34.0f)
#define FOUNT_Y  (34.0f)

typedef struct Banner {
    float n_amt;     /* 0..1 how much of the old N shows through          */
    float n_scale;   /* size of the N relative to a letter                 */
    int   n_red;     /* last bloom: CHERRY red paint                       */
    float roll_y;    /* while rolling: world y of the roller; stain hidden above it (fresh paint) */
    int   rolling;
    float drip;      /* red drip length (world units)                      */
    float wet;       /* 0..1 fresh-paint shine on the N patch              */
} Banner;

void set_sky_dawn(float t, float sun_par);
void set_bandstand(float t, const Banner *b, int crayon);
void set_banner(float t, const Banner *b, int crayon);
void set_paw(float x, float y, float t);
void set_wall(float t);
void set_fountain(float t, float giggle, float giggle_t0);
void set_ladder(void);
void set_roller(float hx, float hy, float ang, float wet);
/* cupcake balloon floating at (x,y); face -1..1 (1 = turned to the Wall);
 * sq = squash; returns nothing. string_to: tie point */
void set_balloon(float x, float y, float face, float sq, float tilt, float tx, float ty, float t);
void set_plaza(float t);
void set_bakery(float t, float paw, float towel, float lamp, float tag_t);
void set_egg(float cx, float cy, float r, float rot, float crack, float t);
void set_crayon_hand(int f);
void set_storybook(float t);
#endif
