/* prims.h - low-poly 3D primitives in a model's local space (owner: H3).
 *
 * Factored out of model.c (SPEC H3 M0) so the player's chibi (model.c)
 * and the house's furniture (room.c, FurnPrim lists) share one set of
 * boxes, plush blobs and pyramids. Behaviour is exactly model.c's.
 *
 * Local space: x right, y forward, z up, in 1/16 model units; prim_xf
 * places the model (feet) relative to the camera and gives its yaw and
 * cell light. Every face is lit (fixed sun + prim_xf.light) and drawn as
 * gouraud quads / triangles into the r3d ordering table.
 */
#ifndef PS1_PRIMS_H
#define PS1_PRIMS_H

#include "r3d.h"

typedef struct PrimXf {
    int32_t px, py, pz;      /* model origin (feet), world units relative to camera */
    int fx, fy;              /* facing (4.12) */
    int rx, ry;              /* right (4.12) */
    uint32_t light;          /* 0..255 grey of the cell light */
} PrimXf;
extern PrimXf prim_xf;

/* a rotation about the local x axis through pivot depth py0 / height pz0
 * (1/16 units): s, c = sin, cos of the angle (4.12) */
typedef struct PrimSwing { int s, c, py0, pz0; } PrimSwing;

/* axis-aligned box: centre, half sizes (1/16); sw (may be NULL) rotates it */
void prim_box(int cx, int cy, int cz, int hx, int hy, int hz, uint32_t rgb,
              const PrimSwing *sw, int bias);
/* smooth plush ellipsoid (8 longitudes, 5 rings), lit per vertex */
void prim_blob(int cx, int cy, int cz, int rx, int ry, int rz, uint32_t rgb, int bias);
/* square pyramid: base centre, half base, height, tip lean along y (1/16) */
void prim_pyramid(int cx, int cy, int cz, int hb, int h, int lean_y, uint32_t rgb, int bias);

#endif
