/* show_l2.h - what the lane-2 modules (show_cam, show_fx, show_strokes,
 * show_tilemap, show_text) share: the canvas numbers, the seeded hash, the
 * show-op numbers they own, and the few things they ask of the player core.
 * Owner: ENGINE lane 2.
 *
 * The player core (show_game / show_stage / show_ext, lane 1) owns the shot
 * loop. Every hook below that it does not define has a weak default in
 * show_l2.c, so these modules link and run on their own (the lane-2 test
 * driver in show/tests/engine/wp578/ uses exactly that).
 */
#ifndef SHOW_L2_H
#define SHOW_L2_H

#include <stdint.h>

#if defined(__has_include)
#if __has_include("show/show_game.h")
#include "show/show_game.h"
#endif
#endif
#ifndef SHOW_CANVAS_W
#define SHOW_CANVAS_W   320
#define SHOW_CANVAS_H   180
#define SHOW_CANVAS_Y   30          /* show y + 30 = screen y            */
#define SHOW_WALL_X     160
#define SHOW_SAFE_X     16
#define SHOW_SAFE_Y     9
#endif
#define SHOW_OVERSCAN   8           /* px every plane carries past the picture (spec 2.3) */

/* ---- the seeded hash every deterministic jitter uses ---------------------- */
static inline uint32_t show_hash3(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}

/* ---- show ops owned by lane 2 (BOP_SHOT a = 0x4000 | op, b = arg index) ----
 * The block 0x0100..0x01FF is lane 2's; the dispatcher (show_ext.c) hands any
 * op in it to show_l2_op(). Arguments are int16 words from the shot's arg
 * table (layouts in each module's header). Append only. */
#define SOP_L2_BASE      0x0100
#define SOP_L2_END       0x0200
enum {
    SOP_IRIS = SOP_L2_BASE,     /* x y r heart mode hold: an iris anywhere, any hold    */
                                /*   (mode 0 in, 1 out, 2 static, 3 off; x,y show px)    */
    SOP_BOIL,                   /* every (0 = off): the crayon boil from now on          */
    SOP_FADE_SET,               /* level16: the fade level now, no ramp                  */
    SOP_L2_COUNT
};
/* Everything else lane 2 draws (strokes and their reveals, SUPERs, signs,
 * cards, wipes, the Split, dissolves, camera keys) is baked by the compiler
 * into the shot's records with its frames, so it is a pure function of the
 * shot frame and needs no event. */

/* Dispatch one lane-2 show op (show_l2.c). Returns 1 if it was lane 2's. */
int  show_l2_op(int op, const int16_t *args, int nargs, int t);

/* ---- what lane 2 asks of the player core (weak defaults in show_l2.c) ------ */
/* Screen-space (show px) anchor of a prop or actor slot (the iris centre of
 * `iris_in at=prop:<id>`, a sign's prop). Returns 0 if the slot is unknown. */
int  show_stage_slot_xy(int slot, int *x, int *y);
/* The shot frame being drawn (0 = the shot's first frame). */
int  show_shot_frame(void);
/* 1 while a seek pre-rolls (draw nothing, keep state). */
int  show_preroll(void);

/* ---- frame order for the lane-2 parts (spec 8.1 step 6) --------------------
 *   shot prep:   show_l2_shot_reset(); then each module's bind from the chunk
 *   per tick:    camfx_tick(); beats (BT_EXT -> show_fx_beat_ext / show_l2_op);
 *                show_l2_frame_begin(t)   [palette lerp first, camera, boil]
 *   draw:        planes (show_strokes_draw / show_tilemap_draw / IMAGE via
 *                show_draw_image_xf) -> actors -> props, crowds ->
 *                [Split side B: show_fx_split_begin() .. its planes/actors ..
 *                 show_fx_split_end()] -> show_fx_draw(t) -> show_text_draw(t)
 *                -> show_fx_post(t) -> show_fx_matte()
 * Nothing is drawn while show_preroll() is 1; the state still advances.
 */
void show_l2_shot_reset(void);
void show_l2_frame_begin(int t);

/* ---- the shot's lane-2 blob ("PIC1", showtk/picture/blob.py) ----------------
 * show_l2_bind() resets the lane-2 state and binds every section; returns the
 * shot length it carries (frames) or -1. The blob must stay valid for the shot.
 * The player core then, per frame t:
 *   show_l2_events(t)          the blob's events (until the beat stream carries them)
 *   show_l2_frame_begin(t)
 *   for its own planes in order, and show_l2_plane_draw(i, t) for lane-2 planes;
 *   or show_l2_draw(t), which draws every PLAN entry in order, the Split's
 *   side B, the effects, the text, the post pass and the matte. */
enum { L2P_IMAGE = 0, L2P_STROKES = 1, L2P_TILEMAP = 2, L2P_FILL = 3 };
struct DoomoImage;
struct ShowPlaneCam;
int  show_l2_bind(const void *blob, uint32_t bytes);
void show_l2_events(int t);
void show_l2_draw(int t);
int  show_l2_planes(void);
int  show_l2_plane_side(int i);
struct ShowXform;
int  show_l2_plane_xform(int i, struct ShowXform *xf);     /* plane i this frame */
void show_l2_plane_draw(int i, int t);
/* an IMAGE plane (lane 1's, or a blob's) through the camera: parallax,
 * per-plane scale, the shake, and the plane's boil (0 = none) */
void show_l2_image_draw(const struct DoomoImage *img, const struct ShowPlaneCam *pc, int px, int boil);
/* the blob's SLOT table (the test driver's stand-in for prop slots) */
int  show_l2_slot(int slot, int *x, int *y);

#endif
