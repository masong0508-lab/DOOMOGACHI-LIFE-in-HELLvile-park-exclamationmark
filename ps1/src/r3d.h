/* r3d.h - the GTE polygon pipeline (owner: ps1).
 *
 * World space: x east, y south, z up, 1 unit = 1 map unit (a cell is 64).
 * Vertices are fed to the GTE RELATIVE to the integer camera position, so
 * they always fit the GTE's 16-bit inputs. The rotation matrix maps world to
 * camera space (x right, y down, z forward) scaled by ~7.9 (R3_SCALE_Q12), so
 * the near plane (R3_NEAR camera units) stays above the GTE's H/2 projection
 * limit while being only ~12 world units in front of the eye.
 *
 * Pipeline per quad (r3_quad): RTPT/RTPS -> near/far/frustum tests ->
 * recursive 2x2 subdivision while the affine error (screen size x depth
 * ratio) is visible or a vertex is outside the GTE/GPU coordinate range ->
 * near-plane clipping of the leaves that still cross it -> per-vertex
 * distance fog on the gouraud colour -> POLY_GT4/GT3 into the ordering table.
 */
#ifndef PS1_R3D_H
#define PS1_R3D_H

#include "ps1.h"

/* camera units per world unit, 4.12 fixed: 7.886 (matrix entries stay
 * below the GTE's 1.3.12 limit of 7.999) */
#define R3_SCALE_Q12 32300
#define R3_W2C(w)    ((int32_t)(((int32_t)(w) * R3_SCALE_Q12) >> 12))
/* near plane: just above the GTE's H/2 projection limit; clip plane a
 * little further (clip points are rounded to integer world units) */
#define R3_NEAR      (g_cam.near_z)
#define R3_CLIPZ     (g_cam.clip_z)
#define R3_MAXDEPTH  3                    /* 8x8 pieces max per quad      */

/* A pipeline vertex. The first 8 bytes are SVECTOR-compatible (u,v sit in
 * the pad half-word, which the GTE ignores). */
typedef struct R3V {
    int16_t  x, y, z;      /* world units relative to the camera cell     */
    uint8_t  u, v;         /* texel offsets inside the texture (0..64)     */
    uint32_t sxy;          /* projected screen x | y << 16                  */
    int32_t  sz;           /* GTE SZ (camera depth, 0 if behind)          */
    uint32_t c;            /* base colour 0x00BBGGRR (0x80 = 1.0)          */
} R3V;

typedef struct R3Cam {
    int32_t  px, py, pz;   /* integer camera position (world units)        */
    int32_t  wx, wy, wz;   /* exact eye position, 16.16                     */
    int      ang, pitch;   /* yaw 0..4095, pitch in 4096-units             */
    int      sa, ca;       /* yaw sin/cos 4.12                              */
    int      sp, cp;       /* pitch sin/cos 4.12                            */
    MATRIX   m;            /* world -> camera (x 7.886)                      */
    int32_t  near_z, clip_z;  /* camera units                                 */
    int      hfov_a;          /* horizontal field of view, 4096-units          */
    int      vx, vy, vw, vh;
    int      cx, cy, H;    /* projection centre / distance                   */
    uint32_t *ot;
    int      ot_shift;
    int32_t  far_z;        /* camera units                                  */
    int32_t  fog_near, fog_far;
    int      fog_shift;
    int16_t  fog[256];     /* brightness factor by sz >> fog_shift (0..256) */
    uint32_t fog_rgb;      /* fog colour added back (0 = black fog)          */
    uint8_t  fog_mix[256]; /* how much fog colour per depth step (0..128)   */
} R3Cam;

extern R3Cam g_cam;

/* Camera setup. x,y,z: eye in 16.16 world units; ang 0..4095; pitch_a in
 * 4096-units (+ = look up). far / fog distances in world units. */
void r3_camera(int32_t x, int32_t y, int32_t z, int ang, int pitch_a,
               int vx, int vy, int vw, int vh, int hfov_px,
               int far_world, int fog_near_world, uint32_t fog_rgb);
/* Open the view's ordering table (must follow r3_camera). 0 if none left. */
int  r3_begin(void);
void r3_end(void);

/* Textured, gouraud quad in Z order (0 TL, 1 TR, 2 BL, 3 BR). The vertices
 * may be modified (they are transformed in place). flags: R3F_*. */
enum { R3F_SEMI = 1, R3F_NOFOG = 2, R3F_NOSUB = 4, R3F_FRONT = 8 /* internal */,
       R3F_FACECAM = 16 /* billboards: lean with the camera (overworld) */ };
void r3_quad(R3V *q, const TexRef *t, int flags, int ot_bias);
/* Flat/gouraud untextured triangle + quad (models). */
void r3_tri_g(R3V *a, R3V *b, R3V *c, int ot_bias);
void r3_quad_g(R3V *q, int ot_bias);

/* Transform one point (world-relative); returns sz (0 = behind/near) */
int32_t r3_point(int x, int y, int z, int32_t *sxy);

/* Fog + colour helpers */
uint32_t r3_fogged(uint32_t c, int32_t sz);
static inline uint32_t r3_rgb(int r, int g, int b)
{
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}
static inline int r3_otz(int32_t sz) { return (int)(sz >> g_cam.ot_shift); }

/* Billboard / sprite helpers (r3d.c) */
void r3_billboard(const DoomoBillboard *b, int32_t bx, int32_t by, int32_t bz,
                  uint32_t col, int extra_flags);
void r3_shadow(int32_t bx, int32_t by, int32_t bz, int radius);

/* Sky backdrop across the viewport (view3d). */
void r3_sky(int sky_id, uint16_t time, int brightness);

/* statistics for the current frame */
void r3_stats_reset(void);

#endif
