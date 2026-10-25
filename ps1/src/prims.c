/* prims.c - low-poly 3D primitives: boxes, plush blobs, pyramids (owner:
 * H3). Moved verbatim from model.c (SPEC H3 M0); see prims.h.
 *
 * Speed (H3): every vertex is transformed ONCE (RTPT per three) and the
 * triangles are emitted from the transformed vertices exactly as
 * r3_tri_g() emits them (the same near test, winding cull, fog, ordering
 * slot and r3d's per-polygon drawing area); r3_quad_g() used to transform
 * a blob's vertices five or six times over. Output is unchanged.
 */
#include <inline_c.h>
#include "prims.h"

PrimXf prim_xf;

#define MEMBAR() __asm__ volatile("" ::: "memory")
#define SX(v) ((int)(int16_t)((v)->sxy))
#define SY(v) ((int)(int16_t)((v)->sxy >> 16))

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

/* transform n vertices in place (what r3_tri_g's xf3 does, once each) */
static void xf(R3V *v, int n)
{
    int i = 0;
    for (; i + 3 <= n; i += 3) {
        MEMBAR();
        gte_ldv3(&v[i], &v[i + 1], &v[i + 2]);
        gte_rtpt();
        gte_stsxy3(&v[i].sxy, &v[i + 1].sxy, &v[i + 2].sxy);
        gte_stsz3(&v[i].sz, &v[i + 1].sz, &v[i + 2].sz);
    }
    for (; i < n; i++) {
        MEMBAR();
        gte_ldv0(&v[i]);
        gte_rtps();
        gte_stsxy(&v[i].sxy);
        gte_stsz(&v[i].sz);
    }
    MEMBAR();
}

/* r3_tri_g() after its transform: near test, cull (closed meshes wound
 * counter-clockwise), fog per vertex, the ordering slot, r3d's area */
static void tri(const R3V *a, const R3V *b, const R3V *c, uint32_t ca, uint32_t cb, uint32_t cc, int bias)
{
    POLY_G3 *p;
    uint32_t *slot;
    int otz;
    if (a->sz < R3_NEAR || b->sz < R3_NEAR || c->sz < R3_NEAR)
        return;
    {
        int ax = SX(a), ay = SY(a);
        if ((SX(b) - ax) * (SY(c) - ay) - (SY(b) - ay) * (SX(c) - ax) <= 0)
            return;
    }
    p = (POLY_G3 *)gfx_alloc(sizeof(POLY_G3));
    if (!p)
        return;
    setPolyG3(p);
    ((uint32_t *)&p->r0)[0] = r3_fogged(ca, a->sz) | ((uint32_t)p->code << 24);
    *(uint32_t *)&p->r1 = r3_fogged(cb, b->sz);
    *(uint32_t *)&p->r2 = r3_fogged(cc, c->sz);
    *(uint32_t *)&p->x0 = a->sxy;
    *(uint32_t *)&p->x1 = b->sxy;
    *(uint32_t *)&p->x2 = c->sxy;
    otz = (int)((int32_t)(a->sz + b->sz + c->sz + c->sz) >> (2 + g_cam.ot_shift)) + bias;
    if (otz < 1) otz = 1;
    if (otz > OT_LEN - 2) otz = OT_LEN - 2;
    slot = g_cam.ot + otz;
    addPrim(slot, p);
    if (g_r3_area) {
        uint32_t *ar = (uint32_t *)gfx_alloc(12);
        if (ar) {
            ar[0] = 0x02000000u;
            ar[1] = g_r3_area[0];
            ar[2] = g_r3_area[1];
            addPrim(slot, ar);
        }
    }
    g_r3stats.polys++;
}

/* a b / d c around, one colour: r3_quad_g's split of the Z-order quad
 * (a, b, d, c) into (a, b, d) and (b, c, d) */
static void face(const R3V *a, const R3V *b, const R3V *c, const R3V *d, uint32_t col, int bias)
{
    tri(a, b, d, col, col, col, bias);
    tri(b, c, d, col, col, col, bias);
}

/* axis-aligned box in model space: centre (cx,cy,cz), half sizes, all in
 * 1/16 units; sw rotates it around a joint */
void prim_box(int cx, int cy, int cz, int hx, int hy, int hz, uint32_t rgb,
              const PrimSwing *sw, int bias)
{
    R3V v[8];
    int i;
    if (!g_cam.ot)
        return;
    for (i = 0; i < 8; i++)
        pt(cx + ((i & 1) ? hx : -hx), cy + ((i & 2) ? hy : -hy), cz + ((i & 4) ? hz : -hz),
           sw, &v[i]);
    xf(v, 8);
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
    R3V v[26];                         /* rings 0..2 x 8, then the two poles */
    int i, j;
    if (!g_cam.ot)
        return;
    for (j = 0; j < 3; j++)
        for (i = 0; i < 8; i++) {
            int nx = (k_cos8[i] * k_lat_c[j]) >> 12, ny = (k_sin8[i] * k_lat_c[j]) >> 12;
            int nz = k_lat_s[j];
            R3V *o = &v[j * 8 + i];
            pt(cx + ((rx * nx) >> 12), cy + ((ry * ny) >> 12), cz + ((rz * nz) >> 12), NULL, o);
            o->c = lit(rgb, nx, ny, nz);
        }
    pt(cx, cy, cz - rz, NULL, &v[24]);
    v[24].c = lit(rgb, 0, 0, -4096);
    pt(cx, cy, cz + rz, NULL, &v[25]);
    v[25].c = lit(rgb, 0, 0, 4096);
    xf(v, 26);
    for (i = 0; i < 8; i++) {
        int k = (i + 1) & 7;
        /* bands (counter-clockwise from outside): the Z-order quad
         * (ring j+1: i, k / ring j: i, k) split as r3_quad_g splits it */
        for (j = 0; j < 2; j++) {
            const R3V *q0 = &v[(j + 1) * 8 + i], *q1 = &v[(j + 1) * 8 + k];
            const R3V *q2 = &v[j * 8 + i], *q3 = &v[j * 8 + k];
            tri(q0, q1, q2, q0->c, q1->c, q2->c, bias);
            tri(q1, q3, q2, q1->c, q3->c, q2->c, bias);
        }
        tri(&v[16 + i], &v[16 + k], &v[25], v[16 + i].c, v[16 + k].c, v[25].c, bias);
        tri(&v[k], &v[i], &v[24], v[k].c, v[i].c, v[24].c, bias);
    }
}

/* square pyramid (ears, horns): base centre, half base, height (1/16) */
void prim_pyramid(int cx, int cy, int cz, int hb, int h, int lean_y, uint32_t rgb, int bias)
{
    static const int8_t nrm[4][2] = { {0, -1}, {1, 0}, {0, 1}, {-1, 0} };
    R3V v[5];                          /* the base 0..3, the tip */
    int i, k;
    if (!g_cam.ot)
        return;
    for (i = 0; i < 4; i++)
        pt(cx + ((i == 1 || i == 2) ? hb : -hb), cy + ((i >= 2) ? hb : -hb), cz, NULL, &v[i]);
    pt(cx, cy + lean_y, cz + h, NULL, &v[4]);
    xf(v, 5);
    for (k = 0; k < 4; k++) {
        uint32_t col = lit(rgb, nrm[k][0] * 3000, nrm[k][1] * 3000, 2800);
        tri(&v[k], &v[(k + 1) & 3], &v[4], col, col, col, bias);
    }
}
