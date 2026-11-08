/* pvr2d.c - the PVR backend of the 2D layer (DC3; owner: dreamcast).
 *
 * The portable part (gfx2d.c, texcache.c, font.c, pal.c; dc2d.h) builds
 * the frame's prim list; this file is the only one that touches the PVR:
 *   init     pvr_init with ONE translucent list in presort mode (autosort
 *            off: the PVR draws translucent polygons in submission order,
 *            which is the core's painter's order), a 1 MB vertex buffer,
 *            the opaque list kept for the 3D packages (DC6-DC8), palettes
 *            in ARGB1555, dithering off (palette colours stay exact)
 *   textures pvr_mem_malloc / pvr_mem_free / pvr_txr_load (texcache.c
 *            twiddles), read back as words for the remapped copies of exact
 *            shades; a texture is only freed or rewritten when neither
 *            this frame nor the one the PVR may still be drawing uses it
 *   submit   at plat_end_frame: wait until the PVR takes a new scene, then
 *            the held backdrop and the frame's prims as sprites (16-bit
 *            UVs, the colour in the header: 32 B per header + 64 B each)
 *            or, when a UV is not exact in 16 bits, as 4-vertex polygons.
 *            A header is sent only when mode, texture, bank or (sprite)
 *            colour change.
 * Every polygon has depth compare ALWAYS and no depth write, so the order
 * alone decides what is on top.
 */
#include <kos.h>
#include <dc/pvr.h>
#include <dc/video.h>
#include "dc_plat.h"
#include "dc2d.h"
#include "doomo/palette.h"

/* ---- init ------------------------------------------------------------------------------ */
void dc2d_be_init(void)
{
    pvr_init_params_t params = {
        .opb_sizes = { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        .vertex_buf_size = DC2D_VBUF_SIZE,
        .dma_enabled = 0,
        .fsaa_enabled = 0,
        .autosort_disabled = 1,
        .opb_overflow_count = 8,
    };
    pvr_init(&params);
    pvr_set_pal_format(PVR_PAL_ARGB1555);
    vid_set_dithering(false);
}

/* ---- textures and palettes ------------------------------------------------------------- */
int dc2d_be_tex_alloc(uint32_t bytes, uint32_t *handle)
{
    pvr_ptr_t p;
    /* keep 64 KB of video RAM spare for the 3D packages' own needs */
    if (pvr_mem_available() < bytes + 64u * 1024u) return 0;
    p = pvr_mem_malloc(bytes);
    if (!p) return 0;
    *handle = (uint32_t)p;
    return 1;
}

void dc2d_be_tex_free(uint32_t handle) { pvr_mem_free((pvr_ptr_t)handle); }

void dc2d_be_tex_write(uint32_t handle, const uint16_t *src, uint32_t bytes)
{
    pvr_txr_load(src, (pvr_ptr_t)handle, bytes);
}

/* video RAM is read as 32-bit words (an exact shade copies a texture) */
void dc2d_be_tex_read(uint32_t handle, uint16_t *dst, uint32_t bytes)
{
    const volatile uint32_t *s = (const volatile uint32_t *)handle;
    uint32_t *d = (uint32_t *)dst, i;
    for (i = 0; i < bytes / 4; i++) d[i] = s[i];
}

void dc2d_be_pal(int bank, const uint16_t *argb1555)
{
    int i;
    for (i = 0; i < 256; i++) pvr_set_pal_entry((uint32_t)(bank * 256 + i), argb1555[i]);
}

void dc2d_be_wait_idle(void) { pvr_wait_render_done(); }

void dc2d_be_text(const char *s)
{
    if (dc_text_armed) dc_text_probe(s);
}

void dc2d_be_log(const char *msg) { dc_log(msg); }

/* ---- headers ------------------------------------------------------------------------------ */
static pvr_poly_hdr_t hdr_base __attribute__((aligned(32)));
static pvr_poly_hdr_t hdr_out __attribute__((aligned(32)));
static uint32_t base_key = 0xFFFFFFFFu;

/* the PVR settings of a mode (shared by the sprite and the polygon context) */
#define SET_MODE(cx, mode)                                                           \
    do {                                                                             \
        (cx).depth.comparison = PVR_DEPTHCMP_ALWAYS;                                 \
        (cx).depth.write = false;                                                    \
        (cx).gen.culling = PVR_CULLING_NONE;                                         \
        (cx).gen.alpha = (mode) == DC2D_ALPHA || (mode) == DC2D_TINT || (mode) == DC2D_DARK; \
        (cx).gen.fog_type = PVR_FOG_DISABLE;                                         \
        (cx).txr.uv_clamp = PVR_UVCLAMP_NONE;                                        \
        (cx).txr.uv_flip = PVR_UVFLIP_NONE;                                          \
        (cx).txr.alpha = false;          /* use the texture's alpha */               \
        (cx).txr.env = (mode) == DC2D_IMAGE ? PVR_TXRENV_REPLACE : PVR_TXRENV_MODULATEALPHA; \
        switch (mode) {                                                              \
        case DC2D_SOLID: (cx).blend.src = PVR_BLEND_ONE; (cx).blend.dst = PVR_BLEND_ZERO; break; \
        case DC2D_MUL:   (cx).blend.src = PVR_BLEND_DESTCOLOR; (cx).blend.dst = PVR_BLEND_ZERO; break; \
        default:         (cx).blend.src = PVR_BLEND_SRCALPHA; (cx).blend.dst = PVR_BLEND_INVSRCALPHA; break; \
        }                                                                            \
    } while (0)

static void compile(const DcFrame *f, const DcPrim *p, int sprite)
{
    uint32_t key = (uint32_t)p->mode | ((uint32_t)dc2d_bank(f, p) << 4) |
                   ((uint32_t)sprite << 7) | ((uint32_t)p->tex << 8);
    if (key == base_key) return;
    base_key = key;
    if (p->tex == DC2D_NOTEX) {
        if (sprite) {
            pvr_sprite_cxt_t cx;
            pvr_sprite_cxt_col(&cx, PVR_LIST_TR_POLY);
            SET_MODE(cx, p->mode);
            pvr_sprite_compile(&hdr_base, &cx);
        } else {
            pvr_poly_cxt_t cx;
            pvr_poly_cxt_col(&cx, PVR_LIST_TR_POLY);
            SET_MODE(cx, p->mode);
            pvr_poly_compile(&hdr_base, &cx);
        }
    } else {
        const DcTex *t = dc2d_tex(p->tex);
        int fmt = PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(dc2d_bank(f, p)) | PVR_TXRFMT_TWIDDLED;
        if (sprite) {
            pvr_sprite_cxt_t cx;
            pvr_sprite_cxt_txr(&cx, PVR_LIST_TR_POLY, fmt, t->tw, t->th, (pvr_ptr_t)t->handle,
                               PVR_FILTER_NONE);
            SET_MODE(cx, p->mode);
            pvr_sprite_compile(&hdr_base, &cx);
        } else {
            pvr_poly_cxt_t cx;
            pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY, fmt, t->tw, t->th, (pvr_ptr_t)t->handle,
                             PVR_FILTER_NONE);
            SET_MODE(cx, p->mode);
            pvr_poly_compile(&hdr_base, &cx);
        }
    }
}

/* ---- prims --------------------------------------------------------------------------------- */
static union {
    pvr_sprite_col_t c;
    pvr_sprite_txr_t t;
    pvr_vertex_t v[4];
} vbuf __attribute__((aligned(32)));

static void send_prim(const DcFrame *f, const DcPrim *p, const DcPrim *last)
{
    int sprite = dc2d_is_sprite(p);
    uint32_t argb = dc2d_prim_argb(f, p);
    if (!last || !dc2d_same_header(f, last, p)) {
        compile(f, p, sprite);
        hdr_out = hdr_base;
        if (sprite) hdr_out.argb = argb;
        pvr_prim(&hdr_out, sizeof(hdr_out));
    }
    if (sprite && p->tex == DC2D_NOTEX) {
        pvr_sprite_col_t *s = &vbuf.c;
        s->flags = PVR_CMD_VERTEX_EOL;
        s->ax = p->x0; s->ay = p->y1; s->az = 1.0f;
        s->bx = p->x0; s->by = p->y0; s->bz = 1.0f;
        s->cx = p->x1; s->cy = p->y0; s->cz = 1.0f;
        s->dx = p->x1; s->dy = p->y1;
        s->d1 = s->d2 = s->d3 = s->d4 = 0;
        pvr_prim(s, sizeof(*s));
    } else if (sprite) {
        pvr_sprite_txr_t *s = &vbuf.t;
        s->flags = PVR_CMD_VERTEX_EOL;
        s->ax = p->x0; s->ay = p->y1; s->az = 1.0f;
        s->bx = p->x0; s->by = p->y0; s->bz = 1.0f;
        s->cx = p->x1; s->cy = p->y0; s->cz = 1.0f;
        s->dx = p->x1; s->dy = p->y1;
        s->dummy = 0;
        s->auv = PVR_PACK_16BIT_UV(p->u0, p->v1);
        s->buv = PVR_PACK_16BIT_UV(p->u0, p->v0);
        s->cuv = PVR_PACK_16BIT_UV(p->u1, p->v0);
        pvr_prim(s, sizeof(*s));
    } else {
        pvr_vertex_t *v = vbuf.v;
        int i;
        for (i = 0; i < 4; i++) {
            v[i].flags = i == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v[i].x = (i & 1) ? p->x1 : p->x0;
            v[i].y = (i & 2) ? p->y1 : p->y0;
            v[i].z = 1.0f;
            v[i].u = (i & 1) ? p->u1 : p->u0;
            v[i].v = (i & 2) ? p->v1 : p->v0;
            v[i].argb = argb;
            v[i].oargb = 0;
        }
        pvr_prim(v, 4 * sizeof(pvr_vertex_t));
    }
}

void dc2d_be_submit(const DcFrame *f)
{
    const DcPrim *last = NULL;
    uint8_t r, g, b;
    int i;
    pvr_wait_ready();
    palette_rgb(f->bg, &r, &g, &b);
    pvr_set_bg_color(r / 255.0f, g / 255.0f, b / 255.0f);
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    base_key = 0xFFFFFFFFu;
    for (i = 0; i < f->nheld; i++) { send_prim(f, &f->held[i], last); last = &f->held[i]; }
    for (i = 0; i < f->n; i++) { send_prim(f, &f->prims[i], last); last = &f->prims[i]; }
    pvr_list_finish();
    pvr_scene_finish();
}
