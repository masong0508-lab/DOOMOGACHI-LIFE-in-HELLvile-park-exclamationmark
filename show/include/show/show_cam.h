/* show_cam.h - the stage camera (WP8; spec 5.5 "Camera", 8.6) and the plane
 * transform it gives every 2D plane: Q16-exact parallax, per-plane scale and
 * the shake as a camera offset (spec 2.3). Owner: ENGINE lane 2.
 *
 * The camera is a PURE FUNCTION OF THE SHOT FRAME: show_cam_eval(t) walks the
 * shot's key list and never accumulates, so a seek to any frame gives the
 * same camera as a full run (spec 8.2) and a drift has no accumulated error
 * after any number of frames (AT5.1).
 *
 * Units
 *   position  "cam units" (CU): 1/3600 show px. 3600 is divisible by 2..6, 8,
 *             9, 10, 12, 15, 16, 18, 20, 24, 25 ... so every rate the docs use
 *             (doc:1.0 = 5/6 px/f = 3000 CU/f; 0.25 px/f; 0.5 px/f) and every
 *             parallax product with a decimal par (0.05 = 1/20) is exact.
 *   scale     Q16 (65536 = 1.0).
 *   parallax  a fraction num/den per axis (the compiler reduces "0.05" to 1/20).
 *
 * The camera is the show-space point at the screen's top-left: a plane with
 * parallax p draws its pixel (u, v) at
 *     X = Cx + (u + at_x - cam_x * p - Cx) * s_plane,   s_plane = 1 + (s - 1) * sp
 * with C the scale centre (default the picture centre, 160, 90) and sp the
 * plane's scale parallax (default 1). At s = 1 this is an exact integer
 * translation. The shake (camfx_offset) is added to every plane equally; the
 * matte bars and SUPERs do not move (spec 2.1, 2.3).
 *
 * Key kinds (one ShowCamKey each; the compiler writes the start value of every
 * move, so no key ever depends on evaluating another at run time):
 *   CK_SET    value = a from t0 on                     (cam at=, cuts inside a shot)
 *   CK_MOVE   from b to a over [t0, t1], eased         (cam to, crane, whip, scale moves)
 *   CK_VEL    value = b + a * (t - c) / 3600 * ...     (drift zero=, pan dx=, constant rate)
 *   CK_CREEP  CK_VEL whose speed ramps in over r frames (creep: dread)
 * A move reaches its target ON its last frame t1 (camfx's ramp convention),
 * so `over=F` starting at t0 has t1 = t0 + F - 1.
 */
#ifndef SHOW_CAM_H
#define SHOW_CAM_H

#include <stdint.h>

#define CAM_CU          3600            /* cam units per show pixel                  */
#define CAM_Q16         65536

enum { CK_SET = 0, CK_MOVE = 1, CK_VEL = 2, CK_CREEP = 3 };
enum { CA_X = 1, CA_Y = 2, CA_S = 4 };                     /* ShowCamKey.axis (one bit) */
enum { CE_LINEAR = 0, CE_IN = 1, CE_OUT = 2, CE_INOUT = 3, CE_WHIP = 4 };
enum { CKF_WHIP = 1 };                  /* ShowCamKey.flags: draw whip speed lines        */

typedef struct ShowCamKey {             /* 20 B; sorted by t0 (stable)                    */
    uint16_t t0, t1;                    /* shot frames (t1 >= t0)                         */
    uint8_t  kind, axis, ease, flags;
    int32_t  a;                         /* SET/MOVE: target (CU or Q16 scale); VEL: CU per frame */
    int32_t  b;                         /* MOVE: start value; VEL/CREEP: value at frame c */
    int32_t  c;                         /* VEL: anchor frame (signed); CREEP: ramp frames  */
} ShowCamKey;

typedef struct ShowCamSetup {           /* 16 B: frame 0                                   */
    int32_t  x, y;                      /* CU                                              */
    int32_t  s;                         /* Q16 (0 = 1.0)                                   */
    int16_t  cx, cy;                    /* scale centre, show px (0,0 = picture centre)    */
} ShowCamSetup;

typedef struct ShowCam {                /* the camera at one frame                         */
    int32_t  x, y;                      /* CU                                              */
    int32_t  s;                         /* Q16                                             */
    int16_t  cx, cy;                    /* scale centre (show px)                          */
    int16_t  shake_x, shake_y;          /* camfx shake + punch (show px), added to planes  */
    int32_t  vx;                        /* CU per frame this frame (whip lines, plog)      */
    uint8_t  whip;                      /* a CKF_WHIP move is running                      */
} ShowCam;

/* per-plane camera data, from the plane record (show_stage / my planes) */
typedef struct ShowPlaneCam {
    int16_t  par_xn, par_xd;            /* x parallax = par_xn / par_xd (den > 0)          */
    int16_t  par_yn, par_yd;            /* y parallax                                      */
    int16_t  at_x, at_y;                /* plane origin in show px (plate (0,0) at cam 0)  */
    uint16_t spar_q8;                   /* scale parallax, Q8 (256 = moves with the camera) */
    uint8_t  noshake, pad;              /* 1: ignore the shake (a HUD-like plane)          */
} ShowPlaneCam;

/* where a plane's pixel (0,0) lands and how big its pixels are this frame:
 * screen show-px X = ox + u * s (u in plane px). ox/oy are floored integer
 * show px; fx/fy their Q16 fractions (used only when s is not a whole
 * number). s includes the shot's px: a px=2 plate at camera scale 1 has
 * s = 2.0. */
typedef struct ShowXform {
    int32_t  ox, oy;                    /* show px (floor)                                  */
    int32_t  fx, fy;                    /* Q16 fraction of ox/oy, 0..65535                  */
    int32_t  s;                         /* Q16 plane scale                                  */
} ShowXform;

/* ---- the shot's camera ---------------------------------------------------- */
void show_cam_reset(void);                                   /* shot start: identity       */
void show_cam_bind(const ShowCamSetup *setup, const ShowCamKey *keys, int n);
/* evaluate frame t (shot frame) and keep it as the current camera; reads
 * camfx_offset() for the shake (call after camfx_tick and the frame's beats) */
void show_cam_frame(int t);
const ShowCam *show_cam(void);                               /* the current camera         */
void show_cam_eval(int t, ShowCam *out);                     /* pure: no shake, no state   */
/* one axis at frame t (CA_X / CA_Y in CU, CA_S in Q16) */
int32_t show_cam_axis(int t, int axis);

/* ---- the plane transform --------------------------------------------------- */
void show_cam_plane(const ShowCam *cam, const ShowPlaneCam *pc, int px, ShowXform *out);
/* x of plane pixel u under xf, in show px, floor (exact at s = 1) */
int  show_xf_x(const ShowXform *xf, int u);
int  show_xf_y(const ShowXform *xf, int v);

/* floor division for signed numerators (den > 0) */
static inline int32_t show_floordiv(int32_t n, int32_t d)
{
    int32_t q = n / d;
    return (n % d != 0 && ((n < 0) != (d < 0))) ? q - 1 : q;
}

/* ---- drawing helpers built on the transform ------------------------------- */
/* Draw a DoomoImage plate under a plane transform: plat_image at a whole
 * scale (1..4, exact), nearest-sampled quads with shared, seam-free edges
 * otherwise (spec 2.3 "scale moves are done per layer"). Show px in, screen
 * out (+SHOW_CANVAS_Y). flags: IMG_FLIPX. */
struct DoomoImage;
void show_draw_image_xf(const struct DoomoImage *img, const ShowXform *xf, int flags);
/* is xf.s a whole number 1..4 (the exact path)? returns it, else 0 */
int  show_xf_whole(const ShowXform *xf);

#endif
