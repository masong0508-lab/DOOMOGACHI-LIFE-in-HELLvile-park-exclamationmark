/* r3d.c - GTE polygon pipeline: camera, subdivision, clipping, fog,
 * billboards, shadows and the sky backdrop (see r3d.h). */
#include <string.h>
#include <psxgte.h>
#include <inline_c.h>
#include "r3d.h"

R3Cam   g_cam;
R3Stats g_r3stats;
/* --- H3: the house clips each model polygon to the player's box
 * (room.c draws its moving things into one table, each with its own
 * drawing area): when set, a copy of this DR_AREA precedes every
 * r3_tri_g polygon --- */
const uint32_t *g_r3_area;
/* --- end H3 --- */

/* the GTE load macros do not tell the compiler they read memory */
#define MEMBAR() __asm__ volatile("" ::: "memory")

static inline void xf3(R3V *a, R3V *b, R3V *c)
{
    MEMBAR();
    gte_ldv3(a, b, c);
    gte_rtpt();
    gte_stsxy3(&a->sxy, &b->sxy, &c->sxy);
    gte_stsz3(&a->sz, &b->sz, &c->sz);
}

static inline void xf1(R3V *a)
{
    MEMBAR();
    gte_ldv0(a);
    gte_rtps();
    gte_stsxy(&a->sxy);
    gte_stsz(&a->sz);
}

#define SX(v) ((int)(int16_t)((v)->sxy))
#define SY(v) ((int)(int16_t)((v)->sxy >> 16))

void r3_stats_reset(void) { memset(&g_r3stats, 0, sizeof(g_r3stats)); }

/* ---- camera ----------------------------------------------------------------- */
void r3_camera(int32_t x, int32_t y, int32_t z, int ang, int pitch_a,
               int vx, int vy, int vw, int vh, int hfov_px,
               int far_world, int fog_near_world, uint32_t fog_rgb)
{
    R3Cam *c = &g_cam;
    int sa, ca, sp, cp, i, s;
    int32_t fx, fy, fz;
    c->wx = x; c->wy = y; c->wz = z;
    c->px = x >> 16; c->py = y >> 16; c->pz = z >> 16;
    fx = (x & 0xffff) >> 4;
    fy = (y & 0xffff) >> 4;
    fz = (z & 0xffff) >> 4;
    c->ang = ang & 4095;
    c->pitch = pitch_a;
    sa = isin(c->ang); ca = icos(c->ang);
    sp = isin(pitch_a); cp = icos(pitch_a);
    c->sa = sa; c->ca = ca;
    c->sp = sp; c->cp = cp;
    /* rows: right, down, forward (see r3d.h), scaled by R3_SCALE_Q12 */
#define SC(v) ((int16_t)(((v) * R3_SCALE_Q12) >> 12))
    c->m.m[0][0] = SC(-sa);
    c->m.m[0][1] = SC(ca);
    c->m.m[0][2] = 0;
    c->m.m[1][0] = SC((ca * sp) >> 12);
    c->m.m[1][1] = SC((sa * sp) >> 12);
    c->m.m[1][2] = SC(-cp);
    c->m.m[2][0] = SC((ca * cp) >> 12);
    c->m.m[2][1] = SC((sa * cp) >> 12);
    c->m.m[2][2] = SC(sp);
#undef SC
    for (i = 0; i < 3; i++)
        c->m.t[i] = -((c->m.m[i][0] * fx + c->m.m[i][1] * fy + c->m.m[i][2] * fz) >> 24);
    c->vx = vx; c->vy = vy; c->vw = vw; c->vh = vh;
    c->cx = vx + vw / 2;
    c->cy = vy + vh / 2;
    c->H = hfov_px;
    gte_SetRotMatrix(&c->m);
    gte_SetTransMatrix(&c->m);
    gte_SetGeomOffset(c->cx, c->cy);
    gte_SetGeomScreen(c->H);

    {
        /* horizontal fov = 2 atan(vw/2 / H) */
        int lo = 0, hi = 1024, half = vw / 2;
        while (hi - lo > 2) {
            int mid = (lo + hi) >> 1;
            if (isin(mid) * c->H >= icos(mid) * half) hi = mid; else lo = mid;
        }
        c->hfov_a = hi * 2;
    }
    /* SZ must stay above H/2 for the GTE divide (else it saturates) */
    c->near_z = c->H / 2 + 6;
    /* polygons are clipped much closer than that: vertices between the
     * clip plane and near_z are projected in software (soft_proj). A clip
     * plane at near_z (~22 world units) cut away walls, floors and step
     * edges right next to the eye and left black holes / screen-edge
     * slivers. */
    c->clip_z = R3_CLIP_Q(c->H);
    c->far_z = R3_W2C(far_world);
    if (c->far_z > 65000) c->far_z = 65000;
    for (s = 0; (c->far_z >> s) >= OT_LEN - 24; s++)
        ;
    c->ot_shift = s;
    for (s = 0; (c->far_z >> s) >= 255; s++)
        ;
    c->fog_shift = s;
    if (c->fog_near == R3_W2C(fog_near_world) && c->fog_far == c->far_z &&
        c->fog[255] == 0 && c->fog_rgb == fog_rgb)
        return;     /* fog table still valid */
    c->fog_near = R3_W2C(fog_near_world);
    c->fog_far = c->far_z;
    c->fog_rgb = fog_rgb;
    for (i = 0; i < 256; i++) {
        int32_t zz = (int32_t)i << c->fog_shift;
        int f;
        if (zz <= c->fog_near)
            f = 256;
        else if (zz >= c->fog_far)
            f = 0;
        else {
            /* smooth (quadratic) falloff toward the far plane */
            int32_t t = ((c->fog_far - zz) << 8) / (c->fog_far - c->fog_near); /* 256..0 */
            f = (int)((t * (512 - t)) >> 8);
            if (f > 256) f = 256;
        }
        c->fog[i] = (int16_t)f;
    }
}

int r3_begin(void)
{
    R3Cam *c = &g_cam;
    c->ot = gfx_ot_begin(c->vx, c->vy, c->vw, c->vh);
    return c->ot != NULL;
}

void r3_end(void)
{
    gfx_ot_end();
    g_cam.ot = NULL;
}

static inline uint32_t fogc(uint32_t col, int32_t sz)
{
    int idx = sz >> g_cam.fog_shift;
    uint32_t f;
    if (idx > 255) idx = 255;
    f = (uint32_t)g_cam.fog[idx];
    if (f >= 256)
        return col;
    return ((((col & 0xff00ffu) * f) >> 8) & 0xff00ffu) | ((((col & 0xff00u) * f) >> 8) & 0xff00u);
}

uint32_t r3_fogged(uint32_t col, int32_t sz) { return fogc(col, sz); }

static inline uint32_t col_mid(uint32_t a, uint32_t b)
{
    return ((a & 0xfefefe) >> 1) + ((b & 0xfefefe) >> 1);
}

static inline void vmid(R3V *o, const R3V *a, const R3V *b)
{
    o->x = (int16_t)((a->x + b->x) >> 1);
    o->y = (int16_t)((a->y + b->y) >> 1);
    o->z = (int16_t)((a->z + b->z) >> 1);
    o->u = (uint8_t)((a->u + b->u) >> 1);
    o->v = (uint8_t)((a->v + b->v) >> 1);
    o->c = col_mid(a->c, b->c);
}

static inline int ot_index(int32_t zsum4, int bias)
{
    int otz = (int)(zsum4 >> (2 + g_cam.ot_shift)) + bias;
    if (otz < 1) otz = 1;
    if (otz > OT_LEN - 2) otz = OT_LEN - 2;
    return otz;
}


/* packed u|v<<8 of a vertex with 64 -> 63 (texture edge) */
static inline uint32_t uvq(const R3V *v)
{
    uint32_t uv = *(const uint16_t *)&v->u;
    return uv - ((uv >> 6) & 0x0101u);
}

static inline void link_prim(uint32_t *p, uint32_t len, int otz)
{
    uint32_t *o = g_cam.ot + otz;
    p[0] = (len << 24) | (*o & 0x00ffffffu);
    *o = (uint32_t)p & 0x00ffffffu;
}

static inline void emit_gt4(const R3V *a, const R3V *b, const R3V *c, const R3V *d,
                            const TexRef *t, int flags, int bias, int32_t zmax)
{
    uint32_t *p = (uint32_t *)g_prim_ptr;
    uint32_t tuv = *(const uint16_t *)&t->u;
    uint32_t code = (flags & R3F_SEMI) ? 0x3e000000u : 0x3c000000u;
    if (g_prim_ptr + 52 > g_prim_end)
        return;
    g_prim_ptr += 52;
    if ((flags & R3F_NOFOG) || zmax <= g_cam.fog_near) {
        p[1] = a->c | code;
        p[4] = b->c;
        p[7] = c->c;
        p[10] = d->c;
    } else {
        p[1] = fogc(a->c, a->sz) | code;
        p[4] = fogc(b->c, b->sz);
        p[7] = fogc(c->c, c->sz);
        p[10] = fogc(d->c, d->sz);
    }
    p[2] = a->sxy;
    p[5] = b->sxy;
    p[8] = c->sxy;
    p[11] = d->sxy;
    p[3] = (uvq(a) + tuv) | ((uint32_t)t->clut << 16);
    p[6] = (uvq(b) + tuv) | ((uint32_t)t->tpage << 16);
    p[9] = uvq(c) + tuv;
    p[12] = uvq(d) + tuv;
    link_prim(p, 12, ot_index(a->sz + b->sz + c->sz + d->sz, bias));
    g_r3stats.polys++;
}

/* same as emit_gt4 but sorted at camera depth zsort */
static void __attribute__((noinline)) emit_gt4_at(const R3V *a, const R3V *b, const R3V *c,
                                                  const R3V *d, const TexRef *t, int flags,
                                                  int32_t zsort, int32_t zmax)
{
    uint32_t *p;
    int otz = (int)(zsort >> g_cam.ot_shift);
    if (otz > OT_LEN - 2) otz = OT_LEN - 2;
    p = (uint32_t *)g_prim_ptr;
    emit_gt4(a, b, c, d, t, flags, 0, zmax);
    if ((uint8_t *)p == g_prim_ptr)
        return;
    /* move it from its average-depth bucket to the requested one */
    {
        uint32_t *o = g_cam.ot + ot_index(a->sz + b->sz + c->sz + d->sz, 0);
        *o = (*o & 0xff000000u) | (p[0] & 0x00ffffffu);
    }
    link_prim(p, 12, otz);
    g_r3stats.polys--;
    g_r3stats.backing++;
}

static void emit_gt3(const R3V *a, const R3V *b, const R3V *c, const TexRef *t,
                     int flags, int bias, int32_t zmax)
{
    uint32_t *p = (uint32_t *)g_prim_ptr;
    uint32_t tuv = *(const uint16_t *)&t->u;
    uint32_t code = (flags & R3F_SEMI) ? 0x36000000u : 0x34000000u;
    if (g_prim_ptr + 40 > g_prim_end)
        return;
    g_prim_ptr += 40;
    if ((flags & R3F_NOFOG) || zmax <= g_cam.fog_near) {
        p[1] = a->c | code;
        p[4] = b->c;
        p[7] = c->c;
    } else {
        p[1] = fogc(a->c, a->sz) | code;
        p[4] = fogc(b->c, b->sz);
        p[7] = fogc(c->c, c->sz);
    }
    p[2] = a->sxy;
    p[5] = b->sxy;
    p[8] = c->sxy;
    p[3] = (uvq(a) + tuv) | ((uint32_t)t->clut << 16);
    p[6] = (uvq(b) + tuv) | ((uint32_t)t->tpage << 16);
    p[9] = uvq(c) + tuv;
    link_prim(p, 9, ot_index(a->sz + b->sz + c->sz + c->sz, bias));
    g_r3stats.polys++;
}

/* signed camera-space depth of a world-relative vertex */
static inline int32_t cam_z(const R3V *v)
{
    const MATRIX *m = &g_cam.m;
    return ((m->m[2][0] * v->x + m->m[2][1] * v->y + m->m[2][2] * v->z) >> 12) + m->t[2];
}

static void soft_proj(R3V *v);
static void lerp_clip(R3V *o, const R3V *a, const R3V *b, int32_t za, int32_t zb)
{
    /* point on a->b where camera z == R3_CLIPZ; t in 1/4096 */
    int32_t t = ((R3_CLIPZ - za) << 12) / (zb - za);
    o->x = (int16_t)(a->x + (((b->x - a->x) * t + 2048) >> 12));
    o->y = (int16_t)(a->y + (((b->y - a->y) * t + 2048) >> 12));
    o->z = (int16_t)(a->z + (((b->z - a->z) * t + 2048) >> 12));
    o->u = (uint8_t)(a->u + (((b->u - a->u) * t + 2048) >> 12));
    o->v = (uint8_t)(a->v + (((b->v - a->v) * t + 2048) >> 12));
    {
        int ra = a->c & 0xff, ga = (a->c >> 8) & 0xff, ba = (a->c >> 16) & 0xff;
        int rb = b->c & 0xff, gb = (b->c >> 8) & 0xff, bb = (b->c >> 16) & 0xff;
        o->c = r3_rgb(ra + (((rb - ra) * t) >> 12), ga + (((gb - ga) * t) >> 12),
                      ba + (((bb - ba) * t) >> 12));
    }
    xf1(o);
    soft_proj(o);
}

static void quad_rec(R3V *a, R3V *b, R3V *c, R3V *d, const TexRef *t, int flags,
                     int depth, int bias);
static void tri_rec(R3V *a, R3V *b, R3V *c, const TexRef *t, int flags, int depth, int bias);

/* Clip a polygon given in perimeter order against the camera plane
 * z = R3_CLIPZ, then send the (fully visible) pieces through the normal
 * subdivision path: a quad stays a quad, 3 or 5 vertices become
 * triangles. */
static void __attribute__((noinline)) clip_poly(R3V **ring, int nr, const TexRef *t,
                                                int flags, int depth, int bias)
{
    R3V tmp[4];
    R3V *out[6];
    int32_t z[4];
    int i, n = 0, nt = 0;
    for (i = 0; i < nr; i++)
        z[i] = cam_z(ring[i]);
    for (i = 0; i < nr; i++) {
        int j = i + 1 == nr ? 0 : i + 1;
        int in_i = z[i] >= R3_CLIPZ, in_j = z[j] >= R3_CLIPZ;
        if (in_i) {
            soft_proj(ring[i]);
            out[n++] = ring[i];
        }
        if (in_i != in_j && nt < 4) {
            lerp_clip(&tmp[nt], ring[i], ring[j], z[i], z[j]);
            out[n++] = &tmp[nt++];
        }
    }
    if (n < 3)
        return;
    g_r3stats.clipped++;
    if (n == 4) {
        quad_rec(out[0], out[1], out[3], out[2], t, flags | R3F_FRONT, depth, bias);
    } else {
        tri_rec(out[0], out[1], out[2], t, flags | R3F_FRONT, depth, bias);
        if (n >= 4)
            tri_rec(out[0], out[2], out[3], t, flags | R3F_FRONT, depth, bias);
        if (n >= 5)
            tri_rec(out[0], out[3], out[4], t, flags | R3F_FRONT, depth, bias);
    }
}

/* screen-space bounds test shared by quads and triangles: returns
 * -1 cull, 0 draw, 1 subdivide */
static inline int judge(int minx, int maxx, int miny, int maxy, int32_t zmin, int32_t zmax,
                        int can_sub, int depth)
{
    int size, big;
    if (maxx < g_cam.vx || minx >= g_cam.vx + g_cam.vw ||
        maxy < g_cam.vy || miny >= g_cam.vy + g_cam.vh) {
        g_r3stats.culled++;
        return -1;
    }
    /* the GPU also drops any primitive wider than 1023 or taller than 511
     * pixels: a near wall seen in a corner spans more than that and left
     * a black sliver at the screen edge, so split those too */
    big = minx <= -1000 || maxx >= 1000 || miny <= -1000 || maxy >= 1000 ||
          maxx - minx > 1000 || maxy - miny > 500;
    size = maxx - minx;
    if (maxy - miny > size) size = maxy - miny;
    /* A piece with a vertex outside the GPU range is split until every
     * piece fits (off-screen children cull cheaply); dropping it instead
     * left holes in walls/floors right next to the camera. */
    if (big)
        return depth < R3_MAXDEPTH + 6 ? 1 : -1;
    if (can_sub &&
        (size > 128 || (size > 20 && size * (zmax - zmin) > 7 * (zmax + zmin))))
        return 1;
    return 0;
}

/* recursive part: split into 4 and re-test the children */
static void __attribute__((noinline)) quad_split(R3V *a, R3V *b, R3V *c, R3V *d,
                                                 const TexRef *t, int flags, int depth, int bias)
{
    R3V m01, m02, m13, m23, mc;
    g_r3stats.subdiv++;
    vmid(&m01, a, b);
    vmid(&m02, a, c);
    vmid(&m13, b, d);
    vmid(&m23, c, d);
    vmid(&mc, &m01, &m23);
    xf3(&m01, &m02, &m13);
    xf3(&m23, &mc, &mc);
    flags &= ~R3F_FRONT;     /* children re-test (rounded midpoints) */
    /* recursion is bounded by depth (judge), so children may re-clip */
    quad_rec(a, &m01, &m02, &mc, t, flags, depth + 1, bias);
    quad_rec(&m01, b, &mc, &m13, t, flags, depth + 1, bias);
    quad_rec(&m02, &mc, c, &m23, t, flags, depth + 1, bias);
    quad_rec(&mc, &m13, &m23, d, t, flags, depth + 1, bias);
}

/* leaf test: cull / draw / split. Kept free of big locals so the common
 * path (draw or cull without splitting) has a small stack frame. */
static void quad_rec(R3V *a, R3V *b, R3V *c, R3V *d, const TexRef *t, int flags,
                     int depth, int bias)
{
    int32_t zmin = a->sz, zmax = a->sz, z;
    int minx, maxx, miny, maxy, v, j;
    z = b->sz; if (z < zmin) zmin = z; if (z > zmax) zmax = z;
    z = c->sz; if (z < zmin) zmin = z; if (z > zmax) zmax = z;
    z = d->sz; if (z < zmin) zmin = z; if (z > zmax) zmax = z;
    if (zmax < R3_CLIPZ || zmin > g_cam.far_z)
        return;
    if (zmin < R3_NEAR && !(flags & R3F_FRONT)) {
        /* crosses the near plane: clip first, subdivide the pieces */
        R3V *ring[4];
        ring[0] = a; ring[1] = b; ring[2] = d; ring[3] = c;
        clip_poly(ring, 4, t, flags, depth, bias);
        return;
    }
    minx = maxx = SX(a);
    v = SX(b); if (v < minx) minx = v; if (v > maxx) maxx = v;
    v = SX(c); if (v < minx) minx = v; if (v > maxx) maxx = v;
    v = SX(d); if (v < minx) minx = v; if (v > maxx) maxx = v;
    miny = maxy = SY(a);
    v = SY(b); if (v < miny) miny = v; if (v > maxy) maxy = v;
    v = SY(c); if (v < miny) miny = v; if (v > maxy) maxy = v;
    v = SY(d); if (v < miny) miny = v; if (v > maxy) maxy = v;
    j = judge(minx, maxx, miny, maxy, zmin, zmax,
              depth < R3_MAXDEPTH && !(flags & R3F_NOSUB), depth);
    if (j == 0) {
        emit_gt4(a, b, c, d, t, flags, bias, zmax);
    } else if (j > 0) {
        /* Crack filler: the unsplit quad, drawn first (behind its own
         * pieces). Its edges are exactly the neighbours' edges, so the
         * T-junction slivers between split and unsplit faces show the
         * same texture instead of the background. */
        if (depth == 0 && !(flags & R3F_SEMI) && minx > -1000 && maxx < 1000 &&
            miny > -1000 && maxy < 1000 &&
            maxx - minx <= 1000 && maxy - miny <= 500)
            emit_gt4_at(a, b, c, d, t, flags, zmax + (zmax >> 4) + 8, zmax);
        quad_split(a, b, c, d, t, flags, depth, bias);
    }
}

static void tri_rec(R3V *a, R3V *b, R3V *c, const TexRef *t, int flags, int depth, int bias)
{
    int32_t zmin = a->sz, zmax = a->sz;
    R3V ab, bc, ca;
    int can_sub = depth < R3_MAXDEPTH && !(flags & R3F_NOSUB);
    if (b->sz < zmin) zmin = b->sz;
    if (b->sz > zmax) zmax = b->sz;
    if (c->sz < zmin) zmin = c->sz;
    if (c->sz > zmax) zmax = c->sz;
    if (zmax < R3_CLIPZ || zmin > g_cam.far_z)
        return;
    if (zmin < R3_NEAR && !(flags & R3F_FRONT)) {
        R3V *ring[3];
        ring[0] = a; ring[1] = b; ring[2] = c;
        clip_poly(ring, 3, t, flags, depth, bias);
        return;
    }
    {
        int x0 = SX(a), x1 = SX(b), x2 = SX(c);
        int y0 = SY(a), y1 = SY(b), y2 = SY(c);
        int minx = x0, maxx = x0, miny = y0, maxy = y0, j;
        if (x1 < minx) minx = x1;
        if (x1 > maxx) maxx = x1;
        if (x2 < minx) minx = x2;
        if (x2 > maxx) maxx = x2;
        if (y1 < miny) miny = y1;
        if (y1 > maxy) maxy = y1;
        if (y2 < miny) miny = y2;
        if (y2 > maxy) maxy = y2;
        j = judge(minx, maxx, miny, maxy, zmin, zmax, can_sub, depth);
        if (j < 0)
            return;
        if (j == 0) {
            emit_gt3(a, b, c, t, flags, bias, zmax);
            return;
        }
    }
    g_r3stats.subdiv++;
    vmid(&ab, a, b);
    vmid(&bc, b, c);
    vmid(&ca, c, a);
    xf3(&ab, &bc, &ca);
    flags &= ~R3F_FRONT;
    tri_rec(a, &ab, &ca, t, flags, depth + 1, bias);
    tri_rec(&ab, b, &bc, t, flags, depth + 1, bias);
    tri_rec(&ca, &bc, c, t, flags, depth + 1, bias);
    tri_rec(&ab, &bc, &ca, t, flags, depth + 1, bias);
}

void r3_quad(R3V *q, const TexRef *t, int flags, int ot_bias)
{
    if (!t || !g_cam.ot)
        return;
    xf3(&q[0], &q[1], &q[2]);
    xf1(&q[3]);
    g_r3stats.faces++;
    quad_rec(&q[0], &q[1], &q[2], &q[3], t, flags, 0, ot_bias);
}

int32_t r3_point(int x, int y, int z, int32_t *sxy)
{
    R3V v;
    v.x = (int16_t)x; v.y = (int16_t)y; v.z = (int16_t)z;
    v.u = v.v = 0;
    xf1(&v);
    if (sxy)
        *sxy = (int32_t)v.sxy;
    return v.sz;
}

/* camera-space x / y of a world-relative vertex */
static inline int32_t cam_x(const R3V *v)
{
    const MATRIX *m = &g_cam.m;
    return ((m->m[0][0] * v->x + m->m[0][1] * v->y + m->m[0][2] * v->z) >> 12) + m->t[0];
}

static inline int32_t cam_y(const R3V *v)
{
    const MATRIX *m = &g_cam.m;
    return ((m->m[1][0] * v->x + m->m[1][1] * v->y + m->m[1][2] * v->z) >> 12) + m->t[1];
}

/* The GTE divide saturates below SZ = H/2: project such a vertex on the
 * CPU (clamped to the GTE's screen range, so judge() splits it). */
static void __attribute__((noinline)) soft_proj(R3V *v)
{
    int32_t z = cam_z(v), x, y;
    if (z >= R3_NEAR || z <= 0)
        return;
    x = g_cam.cx + (cam_x(v) * g_cam.H) / z;
    y = g_cam.cy + (cam_y(v) * g_cam.H) / z;
    if (x < -1024) x = -1024;
    if (x > 1023) x = 1023;
    if (y < -1024) y = -1024;
    if (y > 1023) y = 1023;
    v->sxy = (uint32_t)(uint16_t)x | ((uint32_t)(uint16_t)y << 16);
}

/* ---- untextured (models) ------------------------------------------------------- */
void r3_tri_g(R3V *a, R3V *b, R3V *c, int bias)
{
    POLY_G3 *p;
    xf3(a, b, c);
    if (a->sz < R3_NEAR || b->sz < R3_NEAR || c->sz < R3_NEAR)
        return;
    /* back-face cull: models are closed meshes wound counter-clockwise */
    {
        int ax = SX(a), ay = SY(a);
        int cross = (SX(b) - ax) * (SY(c) - ay) - (SY(b) - ay) * (SX(c) - ax);
        if (cross <= 0)
            return;
    }
    p = (POLY_G3 *)gfx_alloc(sizeof(POLY_G3));
    if (!p)
        return;
    setPolyG3(p);
    ((uint32_t *)&p->r0)[0] = r3_fogged(a->c, a->sz) | ((uint32_t)p->code << 24);
    *(uint32_t *)&p->r1 = r3_fogged(b->c, b->sz);
    *(uint32_t *)&p->r2 = r3_fogged(c->c, c->sz);
    *(uint32_t *)&p->x0 = a->sxy;
    *(uint32_t *)&p->x1 = b->sxy;
    *(uint32_t *)&p->x2 = c->sxy;
    {
        uint32_t *slot = g_cam.ot + ot_index(a->sz + b->sz + c->sz + c->sz, bias);
        addPrim(slot, p);
        if (g_r3_area) {                                    /* H3 */
            uint32_t *ar = (uint32_t *)gfx_alloc(12);
            if (ar) {
                ar[0] = 0x02000000u;
                ar[1] = g_r3_area[0];
                ar[2] = g_r3_area[1];
                addPrim(slot, ar);
            }
        }
    }
    g_r3stats.polys++;
}

void r3_quad_g(R3V *q, int bias)
{
    /* Z order 0 1 / 2 3 -> triangles (0,1,2) and (1,3,2) keep the winding */
    R3V t0 = q[1], t1 = q[2];
    r3_tri_g(&q[0], &q[1], &q[2], bias);
    r3_tri_g(&t0, &q[3], &t1, bias);
}

/* ---- billboards ------------------------------------------------------------------ */
void r3_billboard(const DoomoBillboard *bb, int32_t bx, int32_t by, int32_t bz,
                  uint32_t col, int extra_flags)
{
    const DoomoImage *img = bb->img;
    R3Cam *c = &g_cam;
    int scale, w, h, rx, ry, ux, uy, uz, cxr, cyr, czr, ntx, nty, tx, ty;
    int32_t sz_feet, sxy;
    int flip = (bb->flags & BB_FLIPX) ? 1 : 0;
    int semi = ((bb->flags & BB_TRANSLUCENT) || (extra_flags & R3F_SEMI)) ? 1 : 0;
    int otz;
    if (!img || !img->pix || !img->w || !img->h || !c->ot)
        return;
    scale = bb->scale ? bb->scale : 256;
    w = (img->w * scale) >> 8;
    h = (img->h * scale) >> 8;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    cxr = (bx >> 16) - c->px;
    cyr = (by >> 16) - c->py;
    czr = (bz >> 16) - c->pz;
    if (cxr < -8000 || cxr > 8000 || cyr < -8000 || cyr > 8000)
        return;
    /* depth of the sprite's centre decides its OT slot */
    sz_feet = r3_point(cxr, cyr, czr + h / 2, &sxy);
    if (sz_feet < R3_CLIPZ || sz_feet > c->far_z)
        return;
    g_r3stats.sprites++;
    otz = (int)(sz_feet >> c->ot_shift) - 2;
    if (otz < 1) otz = 1;
    if (otz > OT_LEN - 2) otz = OT_LEN - 2;
    col = (extra_flags & R3F_NOFOG) ? col : r3_fogged(col, sz_feet);
    /* camera right vector (4.12); "up" is world up, or the camera's up
     * vector for camera-facing sprites (tilted overworld camera) */
    rx = -c->sa;
    ry = c->ca;
    if (extra_flags & R3F_FACECAM) {
        ux = (-c->ca * c->sp) >> 12;
        uy = (-c->sa * c->sp) >> 12;
        uz = c->cp;
    } else {
        ux = uy = 0;
        uz = 4096;
    }
    ntx = (img->w + IMG_TILE - 1) / IMG_TILE;
    nty = (img->h + IMG_TILE - 1) / IMG_TILE;
    for (ty = 0; ty < nty; ty++)
        for (tx = 0; tx < ntx; tx++) {
            TexRef r;
            R3V q[4];
            POLY_FT4 *p;
            int px0 = tx * IMG_TILE, py0 = ty * IMG_TILE;
            int tw = img->w - px0, th = img->h - py0;
            int l, rr, top, bot, i;
            if (tw > IMG_TILE) tw = IMG_TILE;
            if (th > IMG_TILE) th = IMG_TILE;
            /* horizontal extent of this tile in world units from the centre */
            if (!flip) {
                l = ((px0 * scale) >> 8) - w / 2;
                rr = (((px0 + tw) * scale) >> 8) - w / 2;
            } else {
                l = w / 2 - (((px0 + tw) * scale) >> 8);
                rr = w / 2 - ((px0 * scale) >> 8);
            }
            top = h - ((py0 * scale) >> 8);          /* above the feet */
            bot = h - (((py0 + th) * scale) >> 8);
            {
                int tx_ = (ux * top) >> 12, ty_ = (uy * top) >> 12, tz_ = czr + ((uz * top) >> 12);
                int bx_ = (ux * bot) >> 12, by_ = (uy * bot) >> 12, bz_ = czr + ((uz * bot) >> 12);
                int lx = cxr + ((rx * l) >> 12), ly = cyr + ((ry * l) >> 12);
                int rx2 = cxr + ((rx * rr) >> 12), ry2 = cyr + ((ry * rr) >> 12);
                q[0].x = (int16_t)(lx + tx_);  q[0].y = (int16_t)(ly + ty_);  q[0].z = (int16_t)tz_;
                q[1].x = (int16_t)(rx2 + tx_); q[1].y = (int16_t)(ry2 + ty_); q[1].z = (int16_t)tz_;
                q[2].x = (int16_t)(lx + bx_);  q[2].y = (int16_t)(ly + by_);  q[2].z = (int16_t)bz_;
                q[3].x = (int16_t)(rx2 + bx_); q[3].y = (int16_t)(ry2 + by_); q[3].z = (int16_t)bz_;
            }
            xf3(&q[0], &q[1], &q[2]);
            xf1(&q[3]);
            for (i = 0; i < 4; i++)
                if (q[i].sz < R3_CLIPZ || SX(&q[i]) <= -1000 || SX(&q[i]) >= 1000 ||
                    SY(&q[i]) <= -1000 || SY(&q[i]) >= 1000)
                    goto next_tile;
            {
                int minx = SX(&q[0]) < SX(&q[1]) ? SX(&q[0]) : SX(&q[1]);
                int maxx = SX(&q[0]) > SX(&q[1]) ? SX(&q[0]) : SX(&q[1]);
                int miny = SY(&q[0]) < SY(&q[1]) ? SY(&q[0]) : SY(&q[1]);
                int maxy = SY(&q[2]) > SY(&q[3]) ? SY(&q[2]) : SY(&q[3]);
                if (maxx < c->vx || minx >= c->vx + c->vw || maxy < c->vy || miny >= c->vy + c->vh)
                    goto next_tile;
            }
            if (!vram_image(img, tx | (flip << 8), ty, &r))
                goto next_tile;
            p = (POLY_FT4 *)gfx_alloc(sizeof(POLY_FT4));
            if (!p)
                return;
            setPolyFT4(p);
            setSemiTrans(p, semi);
            ((uint32_t *)&p->r0)[0] = col | ((uint32_t)p->code << 24);
            *(uint32_t *)&p->x0 = q[0].sxy;
            *(uint32_t *)&p->x1 = q[1].sxy;
            *(uint32_t *)&p->x2 = q[2].sxy;
            *(uint32_t *)&p->x3 = q[3].sxy;
            /* the cache stores flipped tiles already mirrored */
            p->u0 = r.u;              p->v0 = r.v;
            p->u1 = (uint8_t)(r.u + tw - 1); p->v1 = r.v;
            p->u2 = r.u;              p->v2 = (uint8_t)(r.v + th - 1);
            p->u3 = (uint8_t)(r.u + tw - 1); p->v3 = (uint8_t)(r.v + th - 1);
            p->tpage = r.tpage;       /* semi mode 0 (50/50) in the tpage */
            p->clut = r.clut;
            addPrim(c->ot + otz, p);
            g_r3stats.polys++;
next_tile:
            ;
        }
}

void r3_shadow(int32_t bx, int32_t by, int32_t bz, int radius)
{
    R3Cam *c = &g_cam;
    TexRef t;
    R3V q[4];
    POLY_FT4 *p;
    int cxr = (bx >> 16) - c->px, cyr = (by >> 16) - c->py, czr = (bz >> 16) - c->pz + 1;
    int i, minx, maxx, miny, maxy;
    if (!c->ot || czr >= 0)
        return;     /* only visible from above */
    if (cxr < -8000 || cxr > 8000 || cyr < -8000 || cyr > 8000)
        return;
    vram_blob(&t);
    q[0].x = (int16_t)(cxr - radius); q[0].y = (int16_t)(cyr - radius);
    q[1].x = (int16_t)(cxr + radius); q[1].y = (int16_t)(cyr - radius);
    q[2].x = (int16_t)(cxr - radius); q[2].y = (int16_t)(cyr + radius);
    q[3].x = (int16_t)(cxr + radius); q[3].y = (int16_t)(cyr + radius);
    for (i = 0; i < 4; i++)
        q[i].z = (int16_t)czr;
    xf3(&q[0], &q[1], &q[2]);
    xf1(&q[3]);
    minx = maxx = SX(&q[0]);
    miny = maxy = SY(&q[0]);
    for (i = 0; i < 4; i++) {
        int x = SX(&q[i]), y = SY(&q[i]);
        if (q[i].sz < R3_CLIPZ || x <= -1000 || x >= 1000 || y <= -1000 || y >= 1000)
            return;
        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
    }
    if (maxx < c->vx || minx >= c->vx + c->vw || maxy < c->vy || miny >= c->vy + c->vh)
        return;
    p = (POLY_FT4 *)gfx_alloc(sizeof(POLY_FT4));
    if (!p)
        return;
    setPolyFT4(p);
    setSemiTrans(p, 1);
    ((uint32_t *)&p->r0)[0] = r3_fogged(0x808080, q[0].sz) | ((uint32_t)p->code << 24);
    *(uint32_t *)&p->x0 = q[0].sxy;
    *(uint32_t *)&p->x1 = q[1].sxy;
    *(uint32_t *)&p->x2 = q[2].sxy;
    *(uint32_t *)&p->x3 = q[3].sxy;
    p->u0 = t.u;       p->v0 = t.v;
    p->u1 = (uint8_t)(t.u + 31); p->v1 = t.v;
    p->u2 = t.u;       p->v2 = (uint8_t)(t.v + 31);
    p->u3 = (uint8_t)(t.u + 31); p->v3 = (uint8_t)(t.v + 31);
    p->tpage = t.tpage;
    p->clut = t.clut;
    addPrim(c->ot + ot_index(q[0].sz + q[1].sz + q[2].sz + q[3].sz, -1), p);
}

/* ---- sky -------------------------------------------------------------------------- */
/* The sky is a cylinder: 256 texels = 360 degrees, so a 90 degree view
 * shows 64 texels. Drawn as up to three textured strips at the far end of
 * the OT, with solid fills above/below the texture. */
static void sky_strip(int x0, int x1, int y0, int y1, const TexRef *t, int u0, int u1,
                      uint32_t col)
{
    /* split vertically: the GPU drops polygons taller than 511 pixels */
    int k, parts = 4;
    for (k = 0; k < parts; k++) {
        int ya = y0 + ((y1 - y0) * k) / parts, yb = y0 + ((y1 - y0) * (k + 1)) / parts;
        int va = t->v + (SKY_H * k) / parts, vb = t->v + (SKY_H * (k + 1)) / parts - 1;
        POLY_FT4 *p;
        if (yb < g_cam.vy || ya >= g_cam.vy + g_cam.vh)
            continue;
        p = (POLY_FT4 *)gfx_alloc(sizeof(POLY_FT4));
        if (!p)
            return;
        setPolyFT4(p);
        ((uint32_t *)&p->r0)[0] = col | ((uint32_t)p->code << 24);
        setXY4(p, x0, ya, x1, ya, x0, yb, x1, yb);
        setUV4(p, u0, va, u1, va, u0, vb, u1, vb);
        p->tpage = t->tpage;
        p->clut = t->clut;
        addPrim(g_cam.ot + OT_LEN - 1, p);
    }
}

static void sky_fill(int x0, int y0, int x1, int y1, uint32_t rgb)
{
    TILE *t;
    if (y1 <= y0 || x1 <= x0)
        return;
    t = (TILE *)gfx_alloc(sizeof(TILE));
    if (!t)
        return;
    setTile(t);
    setColor0(t, rgb);
    setXY0(t, x0, y0);
    setWH(t, x1 - x0, y1 - y0);
    addPrim(g_cam.ot + OT_LEN - 1, t);
}

void r3_sky(int sky_id, uint16_t time, int bright)
{
    R3Cam *c = &g_cam;
    TexRef half[2];
    const uint8_t *pix;
    int span, ucenter, y_bottom, y_top, sky_px_h, x, u, seg;
    int sp, cp;
    uint32_t col = r3_rgb(bright, bright, bright);
    if (!c->ot || !vram_sky(sky_id, time, &half[0], &half[1]))
        return;
    pix = core_sky_pixels(sky_id, time);
    /* horizontal: texels across the viewport for this projection */
    span = (c->hfov_a * SKY_W) >> 12;              /* 64 texels at 90 deg */
    if (span < 8) span = 8;
    if (span > 255) span = 255;
    ucenter = (c->ang * 256) >> 12;
    /* vertical: horizon line from the pitch */
    sp = isin(c->pitch); cp = icos(c->pitch);
    if (cp < 64) cp = 64;
    sky_px_h = (SKY_H * c->vw) / span;              /* square texels */
    y_bottom = c->cy + (c->H * sp) / cp + c->vh / 10;
    y_top = y_bottom - sky_px_h;
    /* fills above and below with the edge rows' colours */
    if (pix) {
        uint8_t topc, botc;
        if (!packart_sky_edges(pix, &topc, &botc)) {
            topc = pix[(SKY_W / 2)];
            botc = pix[(SKY_H - 1) * SKY_W + SKY_W / 2];
        }
        uint32_t tr = g_rgb[topc], br = g_rgb[botc];
        sky_fill(c->vx, c->vy, c->vx + c->vw, y_top + 1, tr);
        sky_fill(c->vx, y_bottom - 1, c->vx + c->vw, c->vy + c->vh, br);
    }
    /* textured strips: split where the u range crosses a 128-texel half */
    u = (ucenter - span / 2) & 255;
    x = c->vx;
    seg = 0;
    while (x < c->vx + c->vw && seg < 4) {
        int h = u >> 7;                      /* which half */
        int uu = u & 127;
        int left = 128 - uu;                 /* texels until the half ends */
        int remain = ((c->vx + c->vw - x) * span + c->vw - 1) / c->vw;
        int n = left < remain ? left : remain;
        int x1 = x + (n * c->vw) / span;
        if (x1 <= x) x1 = x + 1;
        if (n >= remain) x1 = c->vx + c->vw;
        sky_strip(x, x1, y_top, y_bottom, &half[h], uu, uu + n - (n > 1 ? 1 : 0), col);
        x = x1;
        u = (u + n) & 255;
        seg++;
    }
}

/* micro benchmarks (engine test) */
void r3_bench(void)
{
    R3V q[4];
    TexRef t;
    int i;
    uint32_t t0, t1;
    memset(q, 0, sizeof(q));
    memset(&t, 0, sizeof(t));
    q[0].x = 100; q[0].y = -32; q[0].z = 20;
    q[1].x = 100; q[1].y = 32; q[1].z = 20;
    q[2].x = 100; q[2].y = -32; q[2].z = -40;
    q[3].x = 100; q[3].y = 32; q[3].z = -40;
    t0 = timer_us();
    for (i = 0; i < 1000; i++)
        xf3(&q[0], &q[1], &q[2]);
    t1 = timer_us();
    dbg("bench: 1000 xf3 %uus\n", (unsigned)(t1 - t0));
    xf1(&q[3]);
    t0 = timer_us();
    for (i = 0; i < 1000; i++)
        emit_gt4(&q[0], &q[1], &q[2], &q[3], &t, 0, 0, 0);
    t1 = timer_us();
    dbg("bench: 1000 emit_gt4 %uus\n", (unsigned)(t1 - t0));
    gfx_begin();
    t0 = timer_us();
    for (i = 0; i < 1000; i++)
        quad_rec(&q[0], &q[1], &q[2], &q[3], &t, R3F_NOSUB, 0, 0);
    t1 = timer_us();
    dbg("bench: 1000 quad_rec(nosub) %uus\n", (unsigned)(t1 - t0));
    gfx_begin();
    t0 = timer_us();
    for (i = 0; i < 1000; i++)
        gfx_alloc(52);
    t1 = timer_us();
    dbg("bench: 1000 gfx_alloc %uus\n", (unsigned)(t1 - t0));
    gfx_begin();
}
