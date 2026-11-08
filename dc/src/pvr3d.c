/* pvr3d.c - the PVR backend of the 3D batches (DC7; owner: dreamcast).
 *
 * pvr2d.c sends the frame's 2D list as one translucent list in presort
 * mode; where it meets a DC2D_VIEW3D marker it calls dc3d_be_draw(), which
 * sends the marker's segment right there (dc3d.h):
 *   - one polygon header per change of mode or texture (the builder groups
 *     the opaque terrain by texture), then each polygon as a strip of
 *     pvr_vertex_t (EOL on its last vertex)
 *   - every header compares depth GEQUAL (z = 1/w, bigger is nearer):
 *     DC3D_OPAQUE writes depth and does not blend (texel x colour + offset
 *     colour), DC3D_SPRITE blends with the texture's alpha times the
 *     colour's alpha and does not write depth, DC3D_BLEND is untextured
 *     Gouraud colour with alpha (the blob shadows)
 *   - textures are the 2D layer's PAL8 twiddled ones (texcache.c), through
 *     palette bank 0, or the frame's lerp bank when the marker is flagged;
 *     the shadows' blob is a 32x32 ARGB4444 texture of its own, filtered
 *   - vertices are mapped from the viewport the segment was built for to
 *     the marker's rect (the 2D post effects move and scale it) and their
 *     colours multiplied by the marker's colour (a full-screen shade)
 * Culling is off (the builder only makes faces the camera can see), point
 * sampling as the rest of the game, UVs repeat (walls tile their texture
 * up their height).
 */
#include <kos.h>
#include <dc/pvr.h>
#include "dc_plat.h"
#include "dc3d.h"

static pvr_poly_hdr_t hdr __attribute__((aligned(32)));
static pvr_vertex_t   vtx __attribute__((aligned(32)));
static pvr_ptr_t      blob_tex;

/* the shadows' blob: uploaded once, into video RAM of its own */
static int blob_ready(void)
{
    if (!blob_tex) {
        uint32_t bytes = DC3D_BLOB_SIZE * DC3D_BLOB_SIZE * 2;
        blob_tex = pvr_mem_malloc(bytes);
        if (blob_tex) pvr_txr_load(dc3d_blob(), blob_tex, bytes);
    }
    return blob_tex != NULL;
}

static void header(int mode, int tex, int bank)
{
    pvr_poly_cxt_t cx;
    if (tex == DC3D_TEX_BLOB && blob_ready()) {
        pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED,
                         DC3D_BLOB_SIZE, DC3D_BLOB_SIZE, blob_tex, PVR_FILTER_BILINEAR);
    } else if (tex != DC2D_NOTEX && tex != DC3D_TEX_BLOB) {
        const DcTex *t = dc2d_tex(tex);
        pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY,
                         PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(bank) | PVR_TXRFMT_TWIDDLED,
                         t->tw, t->th, (pvr_ptr_t)t->handle, PVR_FILTER_NONE);
    } else {
        pvr_poly_cxt_col(&cx, PVR_LIST_TR_POLY);
    }
    cx.gen.culling = PVR_CULLING_NONE;
    cx.gen.fog_type = PVR_FOG_DISABLE;
    cx.gen.shading = true;
    cx.gen.specular = mode != DC3D_BLEND;      /* the offset colour: glow, fade, tint */
    cx.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cx.depth.write = mode == DC3D_OPAQUE;
    cx.txr.uv_clamp = PVR_UVCLAMP_NONE;
    cx.txr.uv_flip = PVR_UVFLIP_NONE;
    if (mode == DC3D_OPAQUE) {
        cx.gen.alpha = false;
        cx.txr.alpha = true;                   /* ignore the texture's alpha */
        cx.txr.env = PVR_TXRENV_MODULATE;
        cx.blend.src = PVR_BLEND_ONE;
        cx.blend.dst = PVR_BLEND_ZERO;
    } else {
        cx.gen.alpha = true;
        cx.txr.alpha = false;
        cx.txr.env = PVR_TXRENV_MODULATEALPHA;
        cx.blend.src = PVR_BLEND_SRCALPHA;
        cx.blend.dst = PVR_BLEND_INVSRCALPHA;
    }
    pvr_poly_compile(&hdr, &cx);
    pvr_prim(&hdr, sizeof(hdr));
}

static inline uint32_t mulc(uint32_t c, uint32_t m)
{
    uint32_t r = 0;
    int k;
    for (k = 0; k < 32; k += 8)
        r |= ((((c >> k) & 255u) * ((m >> k) & 255u) + 127u) / 255u) << k;
    return r;
}

void dc3d_be_draw(const DcFrame *f, const DcPrim *m)
{
    const DcBatch *b = dc3d_batch(dc3d_marker_slot(m));
    const DcSeg *s;
    float sx, sy, ox, oy;
    uint32_t i, mul = m->argb;
    int bank = (m->flags & DC2D_PF_LERP) ? f->lerp_bank : 0, lm = -1, lt = -1, shade, k;
    if (dc3d_marker_seg(m) >= b->nseg) return;
    s = &b->seg[dc3d_marker_seg(m)];
    sx = (m->x1 - m->x0) / (s->vx1 - s->vx0);
    sy = (m->y1 - m->y0) / (s->vy1 - s->vy0);
    ox = m->x0 - s->vx0 * sx;
    oy = m->y0 - s->vy0 * sy;
    shade = mul != 0xFFFFFFFFu;
    for (i = s->poly0; i < s->poly0 + s->npoly; i++) {
        const DcPoly *p = &b->p[i];
        const DcVert *v = &b->v[p->first];
        if (p->mode != lm || p->tex != lt) {
            header(p->mode, p->tex, bank);
            lm = p->mode;
            lt = p->tex;
        }
        for (k = 0; k < p->nv; k++) {
            vtx.flags = k == p->nv - 1 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            vtx.x = v[k].x * sx + ox;
            vtx.y = v[k].y * sy + oy;
            vtx.z = v[k].z;
            vtx.u = v[k].u;
            vtx.v = v[k].v;
            vtx.argb = shade ? mulc(v[k].argb, mul) : v[k].argb;
            vtx.oargb = shade ? mulc(v[k].oargb, mul) : v[k].oargb;
            pvr_prim(&vtx, sizeof(vtx));
        }
    }
}

uint32_t dc3d_be_us(void) { return dc_us(); }
