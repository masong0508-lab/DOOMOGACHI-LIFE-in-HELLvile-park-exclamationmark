/* dc3d.c - batches of projected polygons for the 3D views (DC7; owner:
 * dreamcast). Portable: see dc3d.h for how a batch sits in the 2D list.
 *
 *   batches   slot (frame & 1) is the frame being built, the other one the
 *             frame just shown (plat_hold_frame copies its segments into
 *             slot DC3D_HELD, which lives until the hold ends)
 *   segments  one per view call: its polygons, the viewport it was built
 *             for, the TA bytes it costs and the textures it uses
 *   camera    a pinhole: world point -> camera space (right, down,
 *             forward) -> frame pixels, z = DC3D_ZK / depth
 *   clipping  convex polygons are cut at depth DC3D_ZNEAR in camera space
 *             (Sutherland-Hodgman, every attribute interpolated), so no
 *             vertex reaches the PVR behind or right at the eye; the PVR
 *             clips the rest to the screen itself
 */
#include <string.h>
#include "dc3d.h"
#include "doomo/doomo.h"

Dc3dStats dc3d_stats;

static DcBatch batches[DC3D_SLOTS];
static int     cur_slot;
static int     seg_open = -1;              /* the open segment of the current batch */
static uint32_t seg_bytes_est;             /* TA bytes so far (headers counted on every change) */
static int     last_mode = -1, last_tex = -1;

static uint16_t blob[DC3D_BLOB_SIZE * DC3D_BLOB_SIZE];
const uint16_t *dc3d_blob(void) { return blob; }

/* alpha 15 in the middle, falling to 0 at the rim, smoothly (1 - d^2)^2 */
static void blob_init(void)
{
    int x, y, n = DC3D_BLOB_SIZE;
    for (y = 0; y < n; y++)
        for (x = 0; x < n; x++) {
            float dx = ((float)x + 0.5f) / (float)(n / 2) - 1.0f, dy = ((float)y + 0.5f) / (float)(n / 2) - 1.0f;
            float d2 = dx * dx + dy * dy, a = d2 >= 1.0f ? 0.0f : (1.0f - d2) * (1.0f - d2);
            blob[y * n + x] = (uint16_t)((int)(a * 15.0f + 0.5f) << 12);
        }
}

void dc3d_init(void)
{
    int i;
    blob_init();
    memset(&dc3d_stats, 0, sizeof(dc3d_stats));
    for (i = 0; i < DC3D_SLOTS; i++) batches[i].nv = batches[i].np = batches[i].nseg = 0;
    cur_slot = 0;
    seg_open = -1;
}

void dc3d_frame(uint32_t frame)
{
    DcBatch *b;
    cur_slot = (int)(frame & 1u);
    b = &batches[cur_slot];
    b->nv = b->np = b->nseg = 0;
    seg_open = -1;
}

DcBatch *dc3d_batch(int slot) { return &batches[slot < 0 || slot >= DC3D_SLOTS ? 0 : slot]; }
int dc3d_cur_slot(void) { return cur_slot; }

/* ---- segments ------------------------------------------------------------------------- */
int dc3d_seg_begin(int vx, int vy, int vw, int vh)
{
    DcBatch *b = &batches[cur_slot];
    DcSeg *s;
    if (b->nseg >= DC3D_MAX_SEGS) { dc3d_stats.dropped++; return 0; }
    seg_open = b->nseg++;
    s = &b->seg[seg_open];
    memset(s, 0, sizeof(*s));
    s->poly0 = (uint32_t)b->np;
    s->vx0 = (float)(vx * DC2D_SCALE);
    s->vy0 = (float)(vy * DC2D_SCALE);
    s->vx1 = (float)((vx + vw) * DC2D_SCALE);
    s->vy1 = (float)((vy + vh) * DC2D_SCALE);
    seg_bytes_est = 0;
    last_mode = last_tex = -1;
    return 1;
}

DcVert *dc3d_poly(int mode, int tex, int nv)
{
    DcBatch *b = &batches[cur_slot];
    DcPoly *p;
    uint32_t add;
    if (seg_open < 0 || nv < 3 || nv > 255) return NULL;
    if (tex < 0) tex = (int)DC2D_NOTEX;
    add = (uint32_t)nv * 32u + ((mode != last_mode || tex != last_tex) ? 32u : 0u);
    if (b->np >= DC3D_MAX_POLYS || b->nv + nv > DC3D_MAX_VERTS || seg_bytes_est + add > DC3D_SEG_BUDGET) {
        dc3d_stats.dropped++;
        return NULL;
    }
    seg_bytes_est += add;
    last_mode = mode;
    last_tex = tex;
    p = &b->p[b->np++];
    p->first = (uint32_t)b->nv;
    p->nv = (uint8_t)nv;
    p->mode = (uint8_t)mode;
    p->tex = (uint16_t)tex;
    b->nv += nv;
    return &b->v[p->first];
}

int dc3d_seg_polys(void)
{
    const DcBatch *b = &batches[cur_slot];
    if (seg_open < 0) return 0;
    return b->np - (int)b->seg[seg_open].poly0;
}

/* stable counting sort of polygons [poly0, np) by (mode, texture): opaque
 * polygons that write depth may go in any order, and grouped they need one
 * header per texture instead of one per change */
#define SORT_KEYS 96
static DcPoly sort_tmp[DC3D_MAX_POLYS];
void dc3d_sort_from(int poly0)
{
    DcBatch *b = &batches[cur_slot];
    uint32_t keys[SORT_KEYS];
    int count[SORT_KEYS + 1], nk = 0, i, k, n = b->np - poly0;
    if (seg_open < 0 || n < 2 || poly0 < (int)b->seg[seg_open].poly0) return;
    memset(count, 0, sizeof(count));
    for (i = poly0; i < b->np; i++) {
        uint32_t key = ((uint32_t)b->p[i].mode << 16) | b->p[i].tex;
        for (k = 0; k < nk && keys[k] != key; k++) {}
        if (k == nk) {
            if (nk == SORT_KEYS) return;    /* too many kinds: leave the order */
            keys[nk++] = key;
        }
        count[k + 1]++;
    }
    for (k = 0; k < nk; k++) count[k + 1] += count[k];
    for (i = poly0; i < b->np; i++) {
        uint32_t key = ((uint32_t)b->p[i].mode << 16) | b->p[i].tex;
        for (k = 0; keys[k] != key; k++) {}
        sort_tmp[count[k]++] = b->p[i];
    }
    memcpy(&b->p[poly0], sort_tmp, (size_t)n * sizeof(DcPoly));
}

static uint32_t seg_cost(const DcBatch *b, const DcSeg *s)
{
    uint32_t i, bytes = 0;
    int lm = -1, lt = -1;
    for (i = s->poly0; i < s->poly0 + s->npoly; i++) {
        const DcPoly *p = &b->p[i];
        if (p->mode != lm || p->tex != lt) { bytes += 32u; lm = p->mode; lt = p->tex; }
        bytes += (uint32_t)p->nv * 32u;
    }
    return bytes;
}

static void seg_textures(const DcBatch *b, DcSeg *s)
{
    uint32_t i;
    int k;
    s->ntex = 0;
    s->tex_more = 0;
    for (i = s->poly0; i < s->poly0 + s->npoly; i++) {
        uint16_t t = b->p[i].tex;
        if (t == DC2D_NOTEX || t == DC3D_TEX_BLOB) continue;
        for (k = 0; k < s->ntex && s->tex[k] != t; k++) {}
        if (k < s->ntex) continue;
        if (s->ntex == DC3D_SEG_TEX) { s->tex_more = 1; return; }
        s->tex[s->ntex++] = t;
    }
}

void dc2d_view3d(int slot, int seg, float x0, float y0, float x1, float y1);  /* gfx2d.c */

void dc3d_seg_end(void)
{
    DcBatch *b = &batches[cur_slot];
    DcSeg *s;
    if (seg_open < 0) return;
    s = &b->seg[seg_open];
    s->npoly = (uint32_t)b->np - s->poly0;
    s->bytes = seg_cost(b, s);
    seg_textures(b, s);
    dc3d_stats.segs++;
    dc3d_stats.polys = s->npoly;
    dc3d_stats.verts = 0;
    {
        uint32_t i;
        for (i = s->poly0; i < s->poly0 + s->npoly; i++) dc3d_stats.verts += b->p[i].nv;
    }
    dc3d_stats.bytes = s->bytes;
    if (s->npoly > dc3d_stats.polys_max) dc3d_stats.polys_max = s->npoly;
    if (s->bytes > dc3d_stats.bytes_max) dc3d_stats.bytes_max = s->bytes;
    dc2d_view3d(cur_slot, seg_open, s->vx0, s->vy0, s->vx1, s->vy1);
    seg_open = -1;
}

uint32_t dc3d_seg_bytes(int slot, int seg)
{
    const DcBatch *b = dc3d_batch(slot);
    if (seg < 0 || seg >= b->nseg) return 0;
    return b->seg[seg].bytes;
}

void dc3d_seg_mark(int slot, int seg, uint32_t frame)
{
    const DcBatch *b = dc3d_batch(slot);
    const DcSeg *s;
    uint32_t i;
    int k;
    if (seg < 0 || seg >= b->nseg) return;
    s = &b->seg[seg];
    if (!s->tex_more) {
        for (k = 0; k < s->ntex; k++) dc2d_tex_mark(s->tex[k], frame);
        return;
    }
    for (i = s->poly0; i < s->poly0 + s->npoly; i++)
        if (b->p[i].tex != DC2D_NOTEX && b->p[i].tex != DC3D_TEX_BLOB) dc2d_tex_mark(b->p[i].tex, frame);
}

int dc3d_hold_seg(int slot, int seg, int reset)
{
    DcBatch *h = &batches[DC3D_HELD];
    const DcBatch *b = dc3d_batch(slot);
    const DcSeg *s;
    DcSeg *d;
    uint32_t i, nverts = 0;
    if (slot == DC3D_HELD || seg < 0 || seg >= b->nseg) return -1;
    if (reset) h->nv = h->np = h->nseg = 0;
    s = &b->seg[seg];
    for (i = s->poly0; i < s->poly0 + s->npoly; i++) nverts += b->p[i].nv;
    if (h->nseg >= DC3D_MAX_SEGS || h->np + (int)s->npoly > DC3D_MAX_POLYS ||
        h->nv + (int)nverts > DC3D_MAX_VERTS)
        return -1;
    d = &h->seg[h->nseg];
    *d = *s;
    d->poly0 = (uint32_t)h->np;
    for (i = 0; i < s->npoly; i++) {
        const DcPoly *p = &b->p[s->poly0 + i];
        DcPoly *q = &h->p[h->np++];
        *q = *p;
        q->first = (uint32_t)h->nv;
        memcpy(&h->v[h->nv], &b->v[p->first], (size_t)p->nv * sizeof(DcVert));
        h->nv += p->nv;
    }
    return h->nseg++;
}

/* ---- camera, clipping, projection -------------------------------------------------- */
float dc3d_project(const Dc3dCam *c, float x, float y, float z, float *sx, float *sy)
{
    float dx = x - c->ex, dy = y - c->ey, dz = z - c->ez;
    float xc = dx * c->rx + dy * c->ry + dz * c->rz;
    float yc = dx * c->dx + dy * c->dy + dz * c->dz;
    float zc = dx * c->fx + dy * c->fy + dz * c->fz;
    if (zc <= 0.0f) { *sx = *sy = 0.0f; return 0.0f; }
    *sx = c->cx + c->focal * xc / zc;
    *sy = c->cy + c->focal * yc / zc;
    return zc;
}

typedef struct CV { float xc, yc, zc, u, v; float a[4], o[4]; } CV;   /* camera space */

static uint32_t pack(const float *c)
{
    uint32_t r = 0;
    int k;
    for (k = 0; k < 4; k++) {
        float f = c[k] + 0.5f;
        int v = f <= 0.0f ? 0 : f >= 255.0f ? 255 : (int)f;
        r |= (uint32_t)v << (24 - 8 * k);           /* a r g b */
    }
    return r;
}

static void unpack(uint32_t c, float *o)
{
    int k;
    for (k = 0; k < 4; k++) o[k] = (float)((c >> (24 - 8 * k)) & 255u);
}

static void cv_lerp(CV *d, const CV *a, const CV *b, float t)
{
    int k;
    d->xc = a->xc + (b->xc - a->xc) * t;
    d->yc = a->yc + (b->yc - a->yc) * t;
    d->zc = a->zc + (b->zc - a->zc) * t;
    d->u = a->u + (b->u - a->u) * t;
    d->v = a->v + (b->v - a->v) * t;
    for (k = 0; k < 4; k++) {
        d->a[k] = a->a[k] + (b->a[k] - a->a[k]) * t;
        d->o[k] = a->o[k] + (b->o[k] - a->o[k]) * t;
    }
}

/* the clipping path: Sutherland-Hodgman against zc >= DC3D_ZNEAR, every
 * attribute interpolated in camera space (rare: only polygons reaching
 * the eye) */
static int add_clipped(const Dc3dCam *c, int mode, int tex, const Dc3dIn *in, const float *xc,
                       const float *yc, const float *zc, int n)
{
    CV a[8], b[16];
    float sx[16], sy[16];
    int i, nb = 0, k, left = 1, right = 1, above = 1, below = 1;
    DcVert *v;
    for (i = 0; i < n; i++) {
        a[i].xc = xc[i]; a[i].yc = yc[i]; a[i].zc = zc[i];
        a[i].u = in[i].u; a[i].v = in[i].v;
        unpack(in[i].argb, a[i].a);
        unpack(in[i].oargb, a[i].o);
    }
    for (i = 0; i < n; i++) {
        const CV *p = &a[i], *q = &a[(i + 1) % n];
        int pin = p->zc >= DC3D_ZNEAR, qin = q->zc >= DC3D_ZNEAR;
        if (pin) b[nb++] = *p;
        if (pin != qin) cv_lerp(&b[nb++], p, q, (DC3D_ZNEAR - p->zc) / (q->zc - p->zc));
    }
    if (nb < 3) return 0;
    for (i = 0; i < nb; i++) {
        float iz = 1.0f / b[i].zc;
        sx[i] = c->cx + c->focal * b[i].xc * iz;
        sy[i] = c->cy + c->focal * b[i].yc * iz;
        if (sx[i] >= c->vx0) left = 0;
        if (sx[i] <= c->vx1) right = 0;
        if (sy[i] >= c->vy0) above = 0;
        if (sy[i] <= c->vy1) below = 0;
    }
    if (left || right || above || below) { dc3d_stats.culled++; return 0; }
    v = dc3d_poly(mode, tex, nb);
    if (!v) return 0;
    dc3d_stats.clipped++;
    for (k = 0; k < nb; k++) {          /* fan order -> strip order: 0, 1, n-1, 2, n-2, ... */
        int j = k == 0 ? 0 : (k & 1) ? (k + 1) / 2 : nb - k / 2;
        const CV *p = &b[j];
        v[k].x = sx[j];
        v[k].y = sy[j];
        v[k].z = DC3D_ZK / p->zc;
        v[k].u = p->u;
        v[k].v = p->v;
        v[k].argb = pack(p->a);
        v[k].oargb = pack(p->o);
    }
    return 1;
}

int dc3d_add(const Dc3dCam *c, int mode, int tex, const Dc3dIn *in, int n)
{
    float xc[8], yc[8], zc[8], sx[8], sy[8], iz[8];
    int i, k, behind = 0, left = 1, right = 1, above = 1, below = 1;
    DcVert *v;
    if (n < 3 || n > 8) return 0;
    for (i = 0; i < n; i++) {
        float dx = in[i].x - c->ex, dy = in[i].y - c->ey, dz = in[i].z - c->ez;
        xc[i] = dx * c->rx + dy * c->ry + dz * c->rz;
        yc[i] = dx * c->dx + dy * c->dy + dz * c->dz;
        zc[i] = dx * c->fx + dy * c->fy + dz * c->fz;
        if (zc[i] < DC3D_ZNEAR) behind = 1;
    }
    if (behind) return add_clipped(c, mode, tex, in, xc, yc, zc, n);
    /* project; cull when wholly outside the viewport */
    for (i = 0; i < n; i++) {
        iz[i] = 1.0f / zc[i];
        sx[i] = c->cx + c->focal * xc[i] * iz[i];
        sy[i] = c->cy + c->focal * yc[i] * iz[i];
        if (sx[i] >= c->vx0) left = 0;
        if (sx[i] <= c->vx1) right = 0;
        if (sy[i] >= c->vy0) above = 0;
        if (sy[i] <= c->vy1) below = 0;
    }
    if (left || right || above || below) { dc3d_stats.culled++; return 0; }
    v = dc3d_poly(mode, tex, n);
    if (!v) return 0;
    for (k = 0; k < n; k++) {           /* fan order -> strip order: 0, 1, n-1, 2, n-2, ... */
        int j = k == 0 ? 0 : (k & 1) ? (k + 1) / 2 : n - k / 2;
        v[k].x = sx[j];
        v[k].y = sy[j];
        v[k].z = DC3D_ZK * iz[j];
        v[k].u = in[j].u;
        v[k].v = in[j].v;
        v[k].argb = in[j].argb;
        v[k].oargb = in[j].oargb;
    }
    return 1;
}
