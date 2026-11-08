/* swpvr.c - a software PVR for the 2D layer's host tests (DC3; owner:
 * dreamcast). It implements the dc2d_be_* backend (dc/src/dc2d.h) on the
 * host and renders a submitted DcFrame the way pvr2d.c asks the PVR to:
 *   - video RAM: textures are kept as the TWIDDLED bytes texcache.c
 *     uploads, inside a 4 MB budget (about what the console has left for
 *     textures beside the two 640x480 frame buffers, the two 1 MB vertex
 *     buffers and the tile bins), so the cache's eviction runs as it would
 *   - palettes: the four ARGB1555 banks as written
 *   - rendering: prims in list order over the background colour; a pixel is
 *     covered when its centre is inside the rect; textures are point
 *     sampled at the pixel centre (repeating), through the prim's bank;
 *     sprite UVs go through the 16-bit truncation; the blend of each mode
 *     as pvr2d.c sets it; the frame is kept in 8-bit RGB and stored as
 *     RGB565 without dithering, like the console's frame buffer.
 * DC7: a DC2D_VIEW3D marker is a segment of a 3D batch (dc3d.h): its
 * triangle strips are rasterised in submission order with a depth buffer
 * (cleared to the background plane, compare GEQUAL, written by the opaque
 * mode), perspective-correct texture coordinates and colours, texel x
 * colour + offset colour, and each mode's blend, as pvr3d.c sets them.
 * What it does not model: the PVR's exact rasterisation rules on non-
 * integer edges, its multiply rounding and the TA's bin limits. Those are
 * what the Flycast harness checks (docs/DREAMCAST.md, section 4).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dc2d.h"
#include "dc3d.h"
#include "swpvr.h"
#include "doomo/palette.h"

#define SW_TEX_MAX 4096
#define SW_TEX_BUDGET (4u * 1024u * 1024u)

static uint8_t *tex_mem[SW_TEX_MAX + 1];
static uint32_t tex_size[SW_TEX_MAX + 1];
static uint32_t tex_used;
static uint16_t pal[1024];
uint32_t swpvr_submits, swpvr_tex_peak;

int dc2d_be_tex_alloc(uint32_t bytes, uint32_t *handle)
{
    int i;
    if (tex_used + bytes > SW_TEX_BUDGET) return 0;
    for (i = 1; i <= SW_TEX_MAX; i++)
        if (!tex_mem[i]) {
            tex_mem[i] = (uint8_t *)calloc(1, bytes);
            if (!tex_mem[i]) return 0;
            tex_size[i] = bytes;
            tex_used += bytes;
            if (tex_used > swpvr_tex_peak) swpvr_tex_peak = tex_used;
            *handle = (uint32_t)i;
            return 1;
        }
    return 0;
}

void dc2d_be_tex_free(uint32_t handle)
{
    if (handle == 0 || handle > SW_TEX_MAX || !tex_mem[handle]) return;
    free(tex_mem[handle]);
    tex_mem[handle] = NULL;
    tex_used -= tex_size[handle];
    tex_size[handle] = 0;
}

void dc2d_be_tex_write(uint32_t handle, const uint16_t *src, uint32_t bytes)
{
    if (handle == 0 || handle > SW_TEX_MAX || !tex_mem[handle] || bytes > tex_size[handle]) {
        fprintf(stderr, "swpvr: bad texture write\n");
        abort();
    }
    memcpy(tex_mem[handle], src, bytes);
}

void dc2d_be_tex_read(uint32_t handle, uint16_t *dst, uint32_t bytes)
{
    if (handle == 0 || handle > SW_TEX_MAX || !tex_mem[handle] || bytes > tex_size[handle]) {
        fprintf(stderr, "swpvr: bad texture read\n");
        abort();
    }
    memcpy(dst, tex_mem[handle], bytes);
}

void dc2d_be_pal(int bank, const uint16_t *argb1555)
{
    memcpy(&pal[bank * 256], argb1555, 256 * sizeof(uint16_t));
}

void dc2d_be_submit(const DcFrame *f) { (void)f; swpvr_submits++; }
void dc2d_be_wait_idle(void) {}

/* ---- rendering -------------------------------------------------------------------- */
static int x5to8(int v) { return (v << 3) | (v >> 2); }

static int mul8(int a, int b) { return (a * b + 127) / 255; }

static uint32_t texel(const DcTex *t, int bank, float u, float v)
{
    int tx = (int)(u * (float)t->tw), ty = (int)(v * (float)t->th);
    uint8_t idx;
    uint16_t c;
    /* repeat (PVR_UVCLAMP_NONE): wrap, negative coordinates included */
    tx = ((tx % t->tw) + t->tw) % t->tw;
    ty = ((ty % t->th) + t->th) % t->th;
    idx = tex_mem[t->handle][dc2d_twiddle_index((uint32_t)tx, (uint32_t)ty, t->tw, t->th)];
    c = pal[bank * 256 + idx];
    return ((c & 0x8000u) ? 0xFF000000u : 0u) | ((uint32_t)x5to8((c >> 10) & 31) << 16) |
           ((uint32_t)x5to8((c >> 5) & 31) << 8) | (uint32_t)x5to8(c & 31);
}

/* ---- DC7: the 3D batches -------------------------------------------------------------- */
static float zbuf[DC2D_W * DC2D_H];
static int blob;                        /* the polygon samples the shadows' blob */
uint32_t swpvr_tris3d;

void dc3d_be_draw(const DcFrame *f, const DcPrim *marker) { (void)f; (void)marker; }
uint32_t dc3d_be_us(void) { return 0; }

static int chan(uint32_t c, int k) { return (int)((c >> (24 - 8 * k)) & 255u); }   /* k: 0 a, 1 r, 2 g, 3 b */

static void tri3d(const DcVert *const *v, const float *X, const float *Y, int mode, const DcTex *t,
                  int bank, uint32_t mul, uint8_t *rgb)
{
    float area = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]);
    float minx = X[0], maxx = X[0], miny = Y[0], maxy = Y[0];
    int x0, x1, y0, y1, x, y, k;
    if (area == 0.0f) return;
    for (k = 1; k < 3; k++) {
        if (X[k] < minx) minx = X[k];
        if (X[k] > maxx) maxx = X[k];
        if (Y[k] < miny) miny = Y[k];
        if (Y[k] > maxy) maxy = Y[k];
    }
    x0 = (int)floorf(minx); x1 = (int)ceilf(maxx);
    y0 = (int)floorf(miny); y1 = (int)ceilf(maxy);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > DC2D_W) x1 = DC2D_W;
    if (y1 > DC2D_H) y1 = DC2D_H;
    swpvr_tris3d++;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            float px = (float)x + 0.5f, py = (float)y + 0.5f, l[3], z, iz, u = 0, vv = 0, c[4], o[4];
            uint8_t *d;
            int r, g, b, a;
            l[0] = ((X[1] - px) * (Y[2] - py) - (X[2] - px) * (Y[1] - py)) / area;
            l[1] = ((X[2] - px) * (Y[0] - py) - (X[0] - px) * (Y[2] - py)) / area;
            l[2] = 1.0f - l[0] - l[1];
            if (l[0] < 0.0f || l[1] < 0.0f || l[2] < 0.0f) continue;
            z = l[0] * v[0]->z + l[1] * v[1]->z + l[2] * v[2]->z;
            if (z < zbuf[y * DC2D_W + x]) continue;                 /* GEQUAL */
            iz = 1.0f / z;
            for (k = 0; k < 4; k++) c[k] = o[k] = 0.0f;
            for (k = 0; k < 3; k++) {
                float wz = l[k] * v[k]->z * iz;                        /* perspective weight */
                int j;
                u += wz * v[k]->u;
                vv += wz * v[k]->v;
                for (j = 0; j < 4; j++) {
                    c[j] += wz * (float)chan(v[k]->argb, j);
                    o[j] += wz * (float)chan(v[k]->oargb, j);
                }
            }
            if (mul != 0xFFFFFFFFu)
                for (k = 0; k < 4; k++) {
                    c[k] = c[k] * (float)chan(mul, k) / 255.0f;
                    o[k] = o[k] * (float)chan(mul, k) / 255.0f;
                }
            if (blob) {                 /* black, the blob's alpha (nearest) */
                int bx = (int)(u * DC3D_BLOB_SIZE), by = (int)(vv * DC3D_BLOB_SIZE);
                bx = bx < 0 ? 0 : bx >= DC3D_BLOB_SIZE ? DC3D_BLOB_SIZE - 1 : bx;
                by = by < 0 ? 0 : by >= DC3D_BLOB_SIZE ? DC3D_BLOB_SIZE - 1 : by;
                a = mul8((dc3d_blob()[by * DC3D_BLOB_SIZE + bx] >> 12) * 17, (int)(c[0] + 0.5f));
                r = g = b = 0;
            } else if (t) {
                uint32_t tc = texel(t, bank, u, vv);
                int ta = (int)(tc >> 24);
                if (mode == DC3D_OPAQUE) ta = 255;
                a = mode == DC3D_OPAQUE ? 255 : mul8(ta, (int)(c[0] + 0.5f));
                r = mul8((int)((tc >> 16) & 255), (int)(c[1] + 0.5f)) + (int)(o[1] + 0.5f);
                g = mul8((int)((tc >> 8) & 255), (int)(c[2] + 0.5f)) + (int)(o[2] + 0.5f);
                b = mul8((int)(tc & 255), (int)(c[3] + 0.5f)) + (int)(o[3] + 0.5f);
            } else {
                a = (int)(c[0] + 0.5f);
                r = (int)(c[1] + 0.5f); g = (int)(c[2] + 0.5f); b = (int)(c[3] + 0.5f);
            }
            r = r > 255 ? 255 : r; g = g > 255 ? 255 : g; b = b > 255 ? 255 : b;
            d = &rgb[(y * DC2D_W + x) * 3];
            if (mode == DC3D_OPAQUE) {
                d[0] = (uint8_t)r; d[1] = (uint8_t)g; d[2] = (uint8_t)b;
                zbuf[y * DC2D_W + x] = z;
            } else if (a > 0) {
                d[0] = (uint8_t)((r * a + d[0] * (255 - a) + 127) / 255);
                d[1] = (uint8_t)((g * a + d[1] * (255 - a) + 127) / 255);
                d[2] = (uint8_t)((b * a + d[2] * (255 - a) + 127) / 255);
            }
        }
}

static void draw_view3d(const DcFrame *f, const DcPrim *m, uint8_t *rgb)
{
    const DcBatch *bt = dc3d_batch(dc3d_marker_slot(m));
    const DcSeg *s;
    float sx, sy, ox, oy;
    int bank = (m->flags & DC2D_PF_LERP) ? f->lerp_bank : 0;
    uint32_t i;
    if (dc3d_marker_seg(m) >= bt->nseg) return;
    s = &bt->seg[dc3d_marker_seg(m)];
    sx = (m->x1 - m->x0) / (s->vx1 - s->vx0);
    sy = (m->y1 - m->y0) / (s->vy1 - s->vy0);
    ox = m->x0 - s->vx0 * sx;
    oy = m->y0 - s->vy0 * sy;
    for (i = s->poly0; i < s->poly0 + s->npoly; i++) {
        const DcPoly *p = &bt->p[i];
        const DcTex *t = p->tex != DC2D_NOTEX && p->tex != DC3D_TEX_BLOB ? dc2d_tex(p->tex) : NULL;
        int k;
        blob = p->tex == DC3D_TEX_BLOB;
        for (k = 0; k + 2 < p->nv; k++) {
            const DcVert *v[3];
            float X[3], Y[3];
            int j;
            for (j = 0; j < 3; j++) {
                v[j] = &bt->v[p->first + (uint32_t)k + (uint32_t)j];
                X[j] = v[j]->x * sx + ox;
                Y[j] = v[j]->y * sy + oy;
            }
            tri3d(v, X, Y, p->mode, t, bank, m->argb, rgb);
        }
    }
}

static void draw_prim(const DcFrame *f, const DcPrim *p, uint8_t *rgb)
{
    int x0, y0, x1, y1, x, y, sprite = dc2d_is_sprite(p), bank = dc2d_bank(f, p);
    uint32_t col = dc2d_prim_argb(f, p);
    int ca = (int)(col >> 24), cr = (int)((col >> 16) & 255), cg = (int)((col >> 8) & 255), cb = (int)(col & 255);
    const DcTex *t = p->tex != DC2D_NOTEX ? dc2d_tex(p->tex) : NULL;
    float u0 = p->u0, v0 = p->v0, u1 = p->u1, v1 = p->v1;
    if (p->mode == DC2D_VIEW3D) { draw_view3d(f, p, rgb); return; }
    if (sprite) { u0 = dc2d_uv16(u0); v0 = dc2d_uv16(v0); u1 = dc2d_uv16(u1); v1 = dc2d_uv16(v1); }
    /* pixels whose centre is inside [x0, x1) x [y0, y1) */
    x0 = (int)ceilf(p->x0 - 0.5f); x1 = (int)ceilf(p->x1 - 0.5f);
    y0 = (int)ceilf(p->y0 - 0.5f); y1 = (int)ceilf(p->y1 - 0.5f);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > DC2D_W) x1 = DC2D_W;
    if (y1 > DC2D_H) y1 = DC2D_H;
    for (y = y0; y < y1; y++) {
        float fy = ((float)y + 0.5f - p->y0) / (p->y1 - p->y0);
        float v = v0 + fy * (v1 - v0);
        for (x = x0; x < x1; x++) {
            uint8_t *d = &rgb[(y * DC2D_W + x) * 3];
            int r = d[0], g = d[1], b = d[2];
            if (t) {
                float fx = ((float)x + 0.5f - p->x0) / (p->x1 - p->x0);
                uint32_t tc = texel(t, bank, u0 + fx * (u1 - u0), v);
                int ta = (int)(tc >> 24), tr = (int)((tc >> 16) & 255), tg = (int)((tc >> 8) & 255), tb = (int)(tc & 255);
                if (p->mode == DC2D_IMAGE) {
                    if (!ta) continue;
                    r = tr; g = tg; b = tb;
                } else if (p->mode == DC2D_ADD) {   /* DC8: MODULATEALPHA, SRCALPHA / ONE */
                    int a = mul8(ta, ca);
                    if (!a) continue;
                    r += mul8(mul8(tr, cr), a); g += mul8(mul8(tg, cg), a); b += mul8(mul8(tb, cb), a);
                    r = r > 255 ? 255 : r; g = g > 255 ? 255 : g; b = b > 255 ? 255 : b;
                } else {                /* MODULATEALPHA, SRCALPHA / INVSRCALPHA */
                    int a = mul8(ta, ca), sr = mul8(tr, cr), sg = mul8(tg, cg), sb = mul8(tb, cb);
                    if (!a) continue;
                    r = (sr * a + r * (255 - a) + 127) / 255;
                    g = (sg * a + g * (255 - a) + 127) / 255;
                    b = (sb * a + b * (255 - a) + 127) / 255;
                }
            } else if (p->mode == DC2D_SOLID) {
                r = cr; g = cg; b = cb;
            } else if (p->mode == DC2D_MUL) {
                r = mul8(r, cr); g = mul8(g, cg); b = mul8(b, cb);
            } else {                    /* ALPHA */
                r = (cr * ca + r * (255 - ca) + 127) / 255;
                g = (cg * ca + g * (255 - ca) + 127) / 255;
                b = (cb * ca + b * (255 - ca) + 127) / 255;
            }
            d[0] = (uint8_t)r; d[1] = (uint8_t)g; d[2] = (uint8_t)b;
        }
    }
}

void swpvr_render(const DcFrame *f, uint8_t *rgb)
{
    uint8_t r, g, b;
    int i;
    palette_rgb(f->bg, &r, &g, &b);
    for (i = 0; i < DC2D_W * DC2D_H; i++) { rgb[i * 3] = r; rgb[i * 3 + 1] = g; rgb[i * 3 + 2] = b; }
    for (i = 0; i < DC2D_W * DC2D_H; i++) zbuf[i] = 0.0001f;   /* the background plane (KOS zclip) */
    for (i = 0; i < f->nheld; i++) draw_prim(f, &f->held[i], rgb);
    for (i = 0; i < f->n; i++) draw_prim(f, &f->prims[i], rgb);
    /* the RGB565 frame buffer, no dithering (pvr2d.c turns it off) */
    for (i = 0; i < DC2D_W * DC2D_H; i++) {
        rgb[i * 3]     = (uint8_t)x5to8(rgb[i * 3] >> 3);
        rgb[i * 3 + 1] = (uint8_t)(((rgb[i * 3 + 1] >> 2) << 2) | (rgb[i * 3 + 1] >> 6));
        rgb[i * 3 + 2] = (uint8_t)x5to8(rgb[i * 3 + 2] >> 3);
    }
}

uint32_t swpvr_tex_used(void) { return tex_used; }
const float *swpvr_zbuf(void) { return zbuf; }
