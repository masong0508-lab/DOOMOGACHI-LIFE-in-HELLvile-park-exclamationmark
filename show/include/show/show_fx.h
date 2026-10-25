/* show_fx.h - the show's effects (WP8 + the crayon register of WP5; spec 5.5
 * "Effects", 8.6, 9.4). Owner: ENGINE lane 2.
 *
 *   irises    circle and heart, centred on a prop/actor slot or a point:
 *             the engine-native BOP_IRIS_IN/OUT (beat.c hands them back as
 *             BT_EXT; show_fx_beat_ext() resolves the slot and calls camfx)
 *             and SOP_IRIS for holds longer than 63 f (the co21 heart).
 *   fades, flashes, shakes, punches, palette lerps: engine-native beat ops
 *             (beat.c runs them through camfx). The shot's frame-0 state
 *             (a fade-in's black, a look's palette lerp) comes from ShowFxSetup.
 *             show_fx_post() applies camfx WITHOUT its screen offset: the
 *             shake is a camera offset on the planes (show_cam), so the frame
 *             edge never moves (spec 2.3) and the matte stays put.
 *   boil      the crayon register's +-1 px jitter on twos, by
 *             hash(seed, element, frame / every) (spec 9.4).
 *   wipes     the brick wipe (5 courses x 2 f, GAG_BIBLE 8.2) and the Split
 *             (the wipe stopped on the Wall line and held): ShowWipe records.
 *             The side-B elements of the shot are drawn between
 *             show_fx_split_begin() and show_fx_split_end(), clipped to the
 *             part of the frame the wipe has given them.
 *   dissolve  the credits' 13 f ordered-dither dissolve between two plates.
 *   whips     speed lines while a `cam whip` move runs.
 */
#ifndef SHOW_FX_H
#define SHOW_FX_H

#include <stdint.h>

struct BeatEv;
struct DoomoImage;

enum { LOOK_PIXEL = 0, LOOK_CRAYON, LOOK_SEPIA, LOOK_LCD, LOOK_HELLIFY };
enum { MATTE_OPEN = 0, MATTE_BLACK = 1 };

typedef struct ShowFxSetup {            /* 16 B: the shot's frame-0 effects        */
    uint8_t  look;                      /* LOOK_*                                   */
    uint8_t  boil_every;                /* 0 off; 2 = on twos                       */
    uint8_t  matte;                     /* MATTE_*                                  */
    uint8_t  fade0;                     /* fade level at frame 0 (16 = from black)  */
    uint8_t  pal_id, pal_level;         /* a palette lerp from frame 0 (PAL_*)      */
    uint8_t  nwipes;                    /* ShowWipe records that follow             */
    uint8_t  flags;                     /* reserved                                 */
    uint32_t seed;                      /* boil seed (episode ^ shot)               */
    uint16_t dissolve_t0, dissolve_len; /* a plate dissolve (len 0 = none)          */
} ShowFxSetup;

enum { WIPE_SWEEP = 0, WIPE_STACK = 1 };
enum { WIPE_B_RIGHT = 0, WIPE_B_LEFT = 1 };
#define WIPE_HOLD_END 0xFFFF

typedef struct ShowWipe {               /* 20 B                                      */
    uint16_t t0;                        /* first frame of the in-move               */
    uint16_t hold;                      /* frames held after it (WIPE_HOLD_END = to the end) */
    uint8_t  kind;                      /* WIPE_SWEEP (a cut between the sides) / WIPE_STACK (the Split) */
    uint8_t  side;                      /* where side B is: WIPE_B_RIGHT (Hellvile) / LEFT */
    uint8_t  courses, step;             /* 5 courses x 2 f (GAG_BIBLE 8.2)          */
    int16_t  line;                      /* STACK: the Wall line; SWEEP: where the edge stops */
    uint8_t  out;                       /* 1: after the hold, the same move back    */
    uint8_t  brick_w, brick_h;          /* show px (16 x 8)                         */
    uint8_t  color, hilite, mortar;     /* palette indices                          */
    uint8_t  px;                        /* the shot's px (brick pixel size)         */
    uint8_t  pad[3];
} ShowWipe;

/* a clip rectangle in show px (x, y, w, h) */
typedef struct ShowClip { int16_t x, y, w, h; } ShowClip;

/* ---- shot ---------------------------------------------------------------- */
void show_fx_reset(void);
void show_fx_bind(const ShowFxSetup *fx, const ShowWipe *wipes);
void show_fx_frame_begin(int t);        /* first thing each frame (palette lerp)   */
/* host kinds (BOP_IRIS_IN, BOP_IRIS_OUT): 1 if handled */
int  show_fx_beat_ext(const struct BeatEv *ev);
/* lane-2 ops SOP_IRIS, SOP_BOIL, SOP_FADE_SET: 1 if handled */
int  show_fx_op(int op, const int16_t *args, int nargs);

/* ---- draw ------------------------------------------------------------------ */
void show_fx_draw(int t);               /* whip lines, the wipe's brick edge       */
void show_fx_post(int t);               /* camfx: palette, iris, flash/fade blend  */
void show_fx_matte(void);               /* black bars when matte=black             */

/* ---- the crayon boil ---------------------------------------------------------- */
/* the boil period for an element whose own setting is `own` (0 = follow
 * the shot's look): 0 = no boil now (also after `boil off`) */
int  show_fx_boil_every(int own);
/* jitter of element `id`, point `i` this frame at period `every`: dx, dy in
 * -1..1 (art px); returns 0 (and zeros) when every is 0 */
int  show_fx_boil(uint32_t id, int i, int every, int *dx, int *dy);

/* ---- wipes and the Split -------------------------------------------------------- */
/* side B's rectangle this frame (show px). 0 = side B is not visible. */
int  show_fx_split_rect(int t, ShowClip *out);
/* clip everything drawn until show_fx_split_end() to side B's rectangle;
 * returns 0 (and clips nothing) when side B is not visible: skip side B then */
int  show_fx_split_begin(int t);
void show_fx_split_end(void);
/* clip drawing to a show-px rectangle (PS1: a DR_AREA), and back to the frame */
void show_fx_clip(const ShowClip *c);
void show_fx_clip_off(void);

/* ---- dissolve ---------------------------------------------------------------- */
/* level16 0..16 of the shot's plate dissolve at t (-1 = none) */
int  show_fx_dissolve_level(int t);
/* b's pixels where the 4x4 ordered threshold is below level16, as an image
 * (static buffer, up to 160x120 = a px=2 plate; larger returns b at >= 8) */
const struct DoomoImage *show_fx_dissolve(const struct DoomoImage *b, int level16);

#endif
