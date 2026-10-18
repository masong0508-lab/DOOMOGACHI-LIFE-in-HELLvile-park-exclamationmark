/* grumbleguts_gfx.h - GATEKEEPER GRUMBLEGUTS, drawn in the core the way the
 * concept sheets draw him (trailer/concept/grumbleguts.c, docs/design/story/
 * concept/gg_*.png): a pot-bellied cartoon devil with buff gym-demon
 * muscle, bat wings, a spiked tail, bone horns, a leather X harness with
 * brass studs, a studded belt, a spiked collar and the gate key. Every
 * shape is a shaded blob: a deep-wine outline, five tones of an 8-shade
 * ramp lit from the upper left, a rim light bottom-right, a specular, and
 * a checkerboard dither on every tone seam. Cartoon-modest: smooth peach
 * belly, smooth red cheeks, nothing else. (owner: story)
 *
 * Cost: a full figure is ~20 blobs (~140 ellipses). The GBA cannot draw
 * that every frame (7 fps measured), so gg_draw_cached() renders the still
 * layers once per (view, pose, expression, scale) into the scene scratch
 * (scene.h: it claims the block for owner GG_SCRATCH_TAG) and blits them,
 * and only the parts that move - the belly / the cheeks, the wings - are
 * drawn live. Scenes that need the scratch for themselves draw with
 * gg_draw() (every frame, direct; fine on the PS1's GPU and in close-ups).
 *
 * Also the staging the storyboard uses: the festival hall backdrop with
 * its lantern bunting and bagel table, a crowd of real residents, gas
 * clouds, speech bubbles, hands, the close-up cheeks.
 */
#ifndef DOOMO_GRUMBLEGUTS_GFX_H
#define DOOMO_GRUMBLEGUTS_GFX_H

#include "doomo/doomo.h"

enum { GGV_FRONT, GGV_SIDE, GGV_BACK, GGV_BACK_LOOK };
enum { GGE_GRIN, GGE_WINK, GGE_SHOCK, GGE_SMUG, GGE_SUCK, GGE_YELP, GGE_LOVE,
       GGE_BLUSH, GGE_SULTRY, GGE_DOPEY, GGE_DEADPAN, GGE_RELIEF, GGE_GRIT,
       GGE_COUNT };
enum { GGP_STAND, GGP_SNACK, GGP_SUCK_IN, GGP_SPRING, GGP_WIGGLE, GGP_UNBUCKLE,
       GGP_CROP, GGP_INFLATED, GGP_MIRROR, GGP_COUNT };
/* layers of the figure (GgParams.layers; 0 = all) */
enum { GGL_WINGS = 1, GGL_BACK = 2, GGL_LIVE = 4, GGL_FRONT = 8, GGL_FACE = 16,
       GGL_HEAD = 32, GGL_ALL = 31 };
/* WINGS, LIVE (belly + belt / the cheeks, and the tail) and FACE (the
 * expression) move every frame; BACK (legs, torso, harness) and FRONT
 * (arms, collar, head) are the still layers gg_draw_cached() keeps. GGL_HEAD
 * is the head alone (no arms, no collar) for face close-ups: GGL_HEAD |
 * GGL_FACE. */
/* Drawing quality of the DIRECT blobs (the cache always renders in full):
 * 2 = five tones + rim + specular (the concept look), 1 = four tones (the
 * PS1's live parts), 0 = three tones (the GBA's: every rect costs ~15 us).
 * Neither cheap level dithers. */
void gg_set_quality(int q);

typedef struct GgParams {
    int8_t  dir;            /* GGV_SIDE: +1 belly to the right, -1 to the left */
    int8_t  layers;         /* GGL_* mask, 0 = GGL_ALL                          */
    int16_t swell;          /* 0..512: the belly and rump swell on top of the
                               pose (256 = the INFLATED pose's size)           */
    int16_t bob, chest, tail, flap;   /* spring offsets in px (0 = still)       */
    int16_t belly_x, belly_y;         /* the belly lags the beat                */
    int16_t lx, ly, rx, ry;           /* the left / right cheek                  */
    uint8_t no_harness, no_belt;      /* they pinged off                         */
    uint8_t seams;                    /* 1 = dithered seams even when drawn
                                         direct (big shapes only); the cache
                                         always dithers                         */
    uint8_t blink;                    /* 1 = eyes shut this frame               */
} GgParams;
void gg_params_init(GgParams *p);

/* Draw him with his feet at (x, y_feet). scale 1 (240x160) or 2 (320x240):
 * ~80 px tall at 1. Direct: every frame costs the whole figure. */
void gg_draw(int x, int y_feet, int scale, int view, int pose, int expr, const GgParams *p);
/* The same picture, with the still layers cached in the scene scratch
 * (claims it for GG_SCRATCH_TAG; falls back to gg_draw when they do not
 * fit, e.g. at scale 2). */
void gg_draw_cached(int x, int y_feet, int scale, int view, int pose, int expr, const GgParams *p);
#define GG_SCRATCH_TAG SAVE_EXT_TAG('G', 'G', 'F', 'X')
/* Scene-side caches in the same scratch: slot 0 (laid out first, so it
 * survives the figure's rebuilds: a backdrop strip) and slot 5 (after the
 * figure's slots 1-4: body, arms, head). The image for `key` is returned, built through
 * `fn` (which must draw ONLY with the gg_* primitives, at origin (0, 0))
 * when it is missing; NULL when it does not fit, then draw it direct.
 * *ox / *oy = where the image's top-left sits relative to that origin. A
 * slot's rebuild drops every slot above it. */
typedef void (*GgDrawFn)(void *ctx);
const DoomoImage *gg_cache_image(int slot, uint32_t key, GgDrawFn fn, void *ctx, int *ox, int *oy);
/* Size of his belly / rump at this scale and swell (for staging). */
int  gg_belly_r(int scale, int pose, int swell);
int  gg_belly_ry(int scale, int pose, int swell);
int  gg_butt_r(int scale, int pose, int swell);
int  gg_belly_cy(int y_feet, int scale, int pose, int swell, int bob, int belly_y);

/* ---- the shaded shapes (direct) ------------------------------------------- */
#define GG_LINE   PAL_RAMP(RAMP_BLOOD, 0)        /* deep wine outline            */
#define GG_HORN   PAL_RAMP(RAMP_BONE, 6)
#define GG_LEATH  PAL_RAMP(RAMP_WOOD, 1)
#define GG_STUD   PAL_RAMP(RAMP_CANDY, 4)        /* brass                        */
#define GG_CHAIN  PAL_RAMP(RAMP_STEEL, 6)
/* outlined 5-tone blob of ramp `ramp` (RAMP_BLOOD = skin, RAMP_PEACH =
 * belly), seams dithered when `seams` */
void gg_blob(int cx, int cy, int rx, int ry, int ramp, int seams);
void gg_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t c);
void gg_ellipse(int cx, int cy, int rx, int ry, uint8_t c);
void gg_line(int x0, int y0, int x1, int y1, uint8_t c);
void gg_rect(int x, int y, int w, int h, uint8_t c);
void gg_checker(int x, int y, int w, int h, uint8_t c);
void gg_studs(int x0, int y0, int x1, int y1, int n, int s);
void gg_strap(int x0, int y0, int x1, int y1, int w);
/* outline text clamped to the screen */
void gg_shout(int x, int y, const char *s, uint8_t c);

/* ---- staging ---------------------------------------------------------------- */
/* The festival hall: a dithered dusk gradient (night: moon and stars, dawn:
 * pink), the park floor from floor_y, a lantern string. t animates. */
enum { GG_HALL_DUSK, GG_HALL_NIGHT, GG_HALL_DAWN };
void gg_hall(int floor_y, int mood, uint32_t t);
void gg_lanterns(int y, uint32_t t);
/* one lantern of radius r hanging from (x, y) (cacheable: gg_* primitives
 * only), and the string that blits a cached one along it */
void gg_lantern(int x, int y, int r);
void gg_lantern_string(int y, int r, uint32_t t, const DoomoImage *lantern, int ox, int oy);
/* buffet table at (x, y) (top edge), w wide, with n bagels left */
void gg_table(int x, int y, int w, int n, uint32_t t);
/* a chair seen from the side, seat at (x, y) */
void gg_chair(int x, int y, int w, int tilt);
/* a row of n real residents (bodyspr chibis), feet at y, from x0 every dx
 * px; dir 0 faces the camera, 3 shows their backs; mood GG_CROWD_* */
enum { GG_CROWD_CALM, GG_CROWD_SHOCK, GG_CROWD_SWOON, GG_CROWD_CHEER,
       GG_CROWD_SLEEP };
void gg_crowd(const int16_t *ids, int n, int x0, int y, int dx, int dir, int scale,
              int mood, uint32_t t);
/* a puff of gas: cute = minty with hearts and sparkles, else sulphur with
 * flies */
void gg_gas(int cx, int cy, int r, int cute, uint32_t t);
/* a speech bubble with its tail pointing down-left (tail < 0) or
 * down-right; w = 0 fits the text */
void gg_bubble(int x, int y, int w, const char *txt, int tail);
/* a cartoon hand, palm at (x, y), fingers pointing toward dir (+1 right,
 * -1 left, 0 up), skin colour `col` */
void gg_hand(int x, int y, int s, int dir, uint8_t col);
/* THE close-up: the belt edge along the top and two enormous cheeks filling
 * the screen. cy = centre line, r = cheek radius, sq_l / sq_r squash each
 * cheek toward the cleft, jig = up/down wobble (opposite on each cheek). */
void gg_cheeks_closeup(int cx, int cy, int r, int sq_l, int sq_r, int jig, int belt);
/* comic impact star */
void gg_burst(int x, int y, int r, uint8_t c);
/* small heart / sparkle that also work on the cache canvas */
void gg_heart(int x, int y, int size, uint8_t c);

#endif
