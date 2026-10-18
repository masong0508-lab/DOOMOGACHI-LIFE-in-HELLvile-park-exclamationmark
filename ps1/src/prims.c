/* prims.c - low-poly 3D primitives: boxes, plush blobs, pyramids (owner:
 * H3). Moved verbatim from model.c (SPEC H3 M0); see prims.h.
 */
#include "prims.h"

PrimXf prim_xf;

/* local (x right, y forward, z up) -> camera-relative world */
static void to_world(int lx, int ly, int lz, R3V *o)
{
    o->x = (int16_t)(prim_xf.px + ((lx * prim_xf.rx + ly * prim_xf.fx) >> 12));
    o->y = (int16_t)(prim_xf.py + ((lx * prim_xf.ry + ly * prim_xf.fy) >> 12));
    o->z = (int16_t)(prim_xf.pz + lz);
}

/* light a face: normal in local space (unit 4.12) */
static uint32_t lit(uint32_t rgb, int nx, int ny, int nz)
{
    /* world normal (yaw only) */
    int wx = (nx * prim_xf.rx + ny * prim_xf.fx) >> 12;
    int wy = (nx * prim_xf.ry + ny * prim_xf.fy) >> 12;
    /* sun from the south-east, high up: (0.35, 0.45, 0.82) */
    int d = (wx * 1434 + wy * 1843 + nz * 3359) >> 12;
    int f = 176 + (d > 0 ? (d * 110) >> 12 : (d * 36) >> 12);    /* 140..286 */
    uint32_t r = ((rgb & 0xff) * (uint32_t)f) >> 8;
    uint32_t g = (((rgb >> 8) & 0xff) * (uint32_t)f) >> 8;
    uint32_t b = (((rgb >> 16) & 0xff) * (uint32_t)f) >> 8;
    r = (r * prim_xf.light) >> 7; g = (g * prim_xf.light) >> 7; b = (b * prim_xf.light) >> 7;
    return r3_rgb((int)r, (int)g, (int)b);
}

/* local point with an optional swing (rotation about the local x axis
 * through pivot height pz0 / depth py0), in 1/16 units for precision */
static void pt(int lx16, int ly16, int lz16, const PrimSwing *sw, R3V *o)
{
    int ly = ly16, lz = lz16;
    if (sw) {
        int dy = ly - sw->py0, dz = lz - sw->pz0;
        ly = sw->py0 + ((dy * sw->c - dz * sw->s) >> 12);
        lz = sw->pz0 + ((dy * sw->s + dz * sw->c) >> 12);
    }
    to_world(lx16 >> 4, ly >> 4, lz >> 4, o);
}

static void face(R3V *a, R3V *b, R3V *c, R3V *d, uint32_t col, int bias)
{
    /* a b / d c around: quad in Z order (a, b, d, c) */
    R3V q[4];
    q[0] = *a; q[1] = *b; q[2] = *d; q[3] = *c;
    q[0].c = q[1].c = q[2].c = q[3].c = col;
    r3_quad_g(q, bias);
}

/* axis-aligned box in model space: centre (cx,cy,cz), half sizes, all in
 * 1/16 units; sw rotates it around a joint */
void prim_box(int cx, int cy, int cz, int hx, int hy, int hz, uint32_t rgb,
              const PrimSwing *sw, int bias)
{
    R3V v[8];
    int i;
    for (i = 0; i < 8; i++)
        pt(cx + ((i & 1) ? hx : -hx), cy + ((i & 2) ? hy : -hy), cz + ((i & 4) ? hz : -hz),
           sw, &v[i]);
    /* faces wound counter-clockwise seen from outside */
    {
        int ny = sw ? sw->c : 4096, nz = sw ? sw->s : 0;
        face(&v[4], &v[5], &v[7], &v[6], lit(rgb, 0, -nz, ny), bias);         /* top    */
        face(&v[2], &v[3], &v[1], &v[0], lit(rgb, 0, nz, -ny), bias);         /* bottom */
        face(&v[6], &v[7], &v[3], &v[2], lit(rgb, 0, ny, nz), bias);          /* front  */
        face(&v[5], &v[4], &v[0], &v[1], lit(rgb, 0, -ny, -nz), bias);        /* back   */
        face(&v[7], &v[5], &v[1], &v[3], lit(rgb, 4096, 0, 0), bias);         /* right  */
        face(&v[4], &v[6], &v[2], &v[0], lit(rgb, -4096, 0, 0), bias);        /* left   */
    }
}

/* A smooth plush "blob": an ellipsoid with 8 longitudes and rings at
 * -90/-45/0/+45/+90 degrees, lit per vertex from its own normal (smooth
 * gouraud), so heads and tummies look round and soft. Centre and radii in
 * 1/16 units. squash flattens the bottom (sits on the body). */
void prim_blob(int cx, int cy, int cz, int rx, int ry, int rz, uint32_t rgb, int bias)
{
    static const int16_t k_cos8[8] = { 4096, 2896, 0, -2896, -4096, -2896, 0, 2896 };
    static const int16_t k_sin8[8] = { 0, 2896, 4096, 2896, 0, -2896, -4096, -2896 };
    static const int16_t k_lat_c[3] = { 2896, 4096, 2896 };   /* -45, 0, +45 */
    static const int16_t k_lat_s[3] = { -2896, 0, 2896 };
    R3V ring[3][8], pole[2];
    int i, j;
    for (j = 0; j < 3; j++)
        for (i = 0; i < 8; i++) {
            int nx = (k_cos8[i] * k_lat_c[j]) >> 12, ny = (k_sin8[i] * k_lat_c[j]) >> 12;
            int nz = k_lat_s[j];
            pt(cx + ((rx * nx) >> 12), cy + ((ry * ny) >> 12), cz + ((rz * nz) >> 12), NULL,
               &ring[j][i]);
            ring[j][i].c = lit(rgb, nx, ny, nz);
        }
    pt(cx, cy, cz - rz, NULL, &pole[0]);
    pole[0].c = lit(rgb, 0, 0, -4096);
    pt(cx, cy, cz + rz, NULL, &pole[1]);
    pole[1].c = lit(rgb, 0, 0, 4096);
    for (i = 0; i < 8; i++) {
        int k = (i + 1) & 7;
        R3V q[4], a, b, c;
        /* bands (counter-clockwise from outside) */
        for (j = 0; j < 2; j++) {
            q[0] = ring[j + 1][i]; q[1] = ring[j + 1][k];
            q[2] = ring[j][i];     q[3] = ring[j][k];
            r3_quad_g(q, bias);
        }
        a = ring[2][i]; b = ring[2][k]; c = pole[1];
        r3_tri_g(&a, &b, &c, bias);
        a = ring[0][k]; b = ring[0][i]; c = pole[0];
        r3_tri_g(&a, &b, &c, bias);
    }
}

/* square pyramid (ears, horns): base centre, half base, height (1/16) */
void prim_pyramid(int cx, int cy, int cz, int hb, int h, int lean_y, uint32_t rgb, int bias)
{
    R3V b[4], t;
    int i;
    for (i = 0; i < 4; i++)
        pt(cx + ((i == 1 || i == 2) ? hb : -hb), cy + ((i >= 2) ? hb : -hb), cz, NULL, &b[i]);
    pt(cx, cy + lean_y, cz + h, NULL, &t);
    {
        R3V a0, a1, a2;
        int k;
        for (k = 0; k < 4; k++) {
            static const int8_t nrm[4][2] = { {0, -1}, {1, 0}, {0, 1}, {-1, 0} };
            a0 = b[k]; a1 = b[(k + 1) & 3]; a2 = t;
            a0.c = a1.c = a2.c = lit(rgb, nrm[k][0] * 3000, nrm[k][1] * 3000, 2800);
            r3_tri_g(&a0, &a1, &a2, bias);
        }
    }
}
