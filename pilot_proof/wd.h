/* wd.h - world-space drawing for the pilot proof scene.
 *
 * Everything is drawn through a 2D camera (centre + zoom), so a push-in is a
 * real re-rasterisation at the new size (more detail as we get closer, never
 * an upscaled sprite). Shapes reuse the teaser's look: soft 3-tone shaded
 * blobs with a coloured selective outline, all on the game's real palette.
 */
#ifndef WD_H
#define WD_H
#include "tk.h"

extern float wd_cx, wd_cy, wd_z;      /* camera: world point at screen centre, px per unit */
void  wd_cam(float cx, float cy, float z);
float wd_sx(float x);
float wd_sy(float y);

uint8_t wd_dk(uint8_t c);             /* one shade darker / lighter in the family */
uint8_t wd_lt(uint8_t c);
uint8_t wd_ol(uint8_t c);             /* selective outline colour */

enum { WB_OUT = 1, WB_SHADE = 2, WB_RIM = 4, WB_ALL = 7 };
/* rotated ellipse (rot in radians), shaded and outlined per flags */
void wd_blob(float cx, float cy, float rx, float ry, float rot, uint8_t c, int flags);
void wd_blob_ol(float cx, float cy, float rx, float ry, float rot, uint8_t c, uint8_t ol, int flags);
void wd_tri(float ax, float ay, float bx, float by, float qx, float qy, uint8_t c);
void wd_quad(float ax, float ay, float bx, float by, float qx, float qy, float dx, float dy,
             uint8_t c, uint8_t ol);          /* ol = 0: no outline */
void wd_rect(float x, float y, float w, float h, uint8_t c, uint8_t ol);
void wd_tube(float ax, float ay, float bx, float by, float ra, float rb,
             uint8_t ol, uint8_t dark, uint8_t mid, uint8_t light);
void wd_plush(float ax, float ay, float bx, float by, float r, uint8_t ol, uint8_t base);
void wd_line(float x0, float y0, float x1, float y1, float w, uint8_t c);
/* world-scaled 5x8 font text; px = world units per font pixel. oc = 0: no outline */
void wd_text(float x, float y, const char *s, float px, uint8_t c, uint8_t oc);
float wd_text_w(const char *s, float px);
/* font mask of one char: returns 5 columns (bit per row), like the game font */
void wd_glyph(char ch, uint8_t cols[5]);
/* dithered fill of a rect in world space, level 0..16 */
void wd_dither(float x, float y, float w, float h, uint8_t c, int level);

/* post: children's-crayon look - boiling edges, waxy gaps, paper grain */
void wd_post_crayon(int f, uint8_t paper);
/* rotate a local point (for props) */
void wd_rot(float *x, float *y, float ox, float oy, float a);

/* spring jiggle: response at time t (frames) to a kick at t0 */
float wd_spring(float t, float t0, float amp, float freq, float damp);
float wd_clamp01(float v);
float wd_seg(float t, float a, float b);   /* 0 before a, 1 after b, eased in between */
#endif
