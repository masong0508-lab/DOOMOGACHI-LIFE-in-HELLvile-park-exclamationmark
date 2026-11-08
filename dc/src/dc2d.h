/* dc2d.h - the Dreamcast 2D layer (DC3; owner: dreamcast).
 *
 * The core draws a 320x240 logical screen in painter's order through the
 * platform.h 2D calls. On the Dreamcast every call becomes one or more
 * axis-aligned primitives (DcPrim) in a list for the frame; plat_end_frame
 * hands the list to the PVR as ONE translucent list in presort mode (no
 * autosort), so the PVR draws it in submission order: exactly the
 * painter's order of the core, including the shaded and blended ones.
 * Every vertex is scaled 2x onto the 640x480 video mode.
 *
 * This header and gfx2d.c, texcache.c, font.c and pal.c are PORTABLE C:
 * no KallistiOS. They are also built on the host (dc/tests/gfx2d) with a
 * software PVR (swpvr.c), which renders the same lists so a test can
 * compare every frame with the native runner's 8bpp reference. Only the
 * backend (pvr2d.c on the console, swpvr.c on the host) talks to the
 * hardware: the dc2d_be_* calls at the end of this file.
 *
 * Primitive modes (how a prim's colour is made and blended):
 *   SOLID  untextured, opaque palette colour (plat_rect, lines, text bg)
 *   MUL    untextured, frame *= colour (the fallback of plat_shade_rect:
 *          a per-channel multiply fitted to the colormap level, pal.c)
 *   ALPHA  untextured, colour over the frame at alpha (plat_post_blend)
 *   IMAGE  PAL8 texture through palette bank 0 (or the lerp bank)
 *   TINT   PAL8 texture through bank 1 (all white, index 0 clear) times a
 *          palette colour: silhouettes, glyphs, the 50% checker
 *   DARK   PAL8 texture through bank 1 times black at alpha: the soft
 *          drop shadow of IMG_SHADOW (frame *= 1 - alpha under it)
 *   VIEW3D a marker, not a rect: a 3D view's projected polygons (a segment
 *          of a dc3d.h batch) are sent here, in the same list (DC7)
 *   GLASS  PAL8 texture through bank 0 (or the lerp bank) times the prim's
 *          ARGB, blended at its alpha: a translucent image (DC8: the house's
 *          ghosts, translucent residents, the invalid ghost's red)
 *   ADD    PAL8 texture through bank 1 times the prim's ARGB, ADDED to the
 *          frame at its alpha: a glow over an image's shape (DC8: the
 *          selected pieces' pulse)
 */
#ifndef DC_DC2D_H
#define DC_DC2D_H

#include <stdint.h>
#include "doomo/types.h"
#include "doomo/platform.h"

/* The logical screen and the video mode. */
#define DC2D_LW    320
#define DC2D_LH    240
#define DC2D_SCALE 2
#define DC2D_W     (DC2D_LW * DC2D_SCALE)
#define DC2D_H     (DC2D_LH * DC2D_SCALE)

/* Prims per frame and the vertex budget. The budget counts what the TA is
 * sent (32 B per header, 64 B per sprite, 128 B per 4-vertex quad) and
 * must stay below the TR vertex buffer of pvr2d.c (DC2D_VBUF_SIZE), which
 * also holds the TA's own list overhead. A frame over either limit drops
 * its last prims and counts them (dc2d_stats.dropped). */
#define DC2D_MAX_PRIMS   12288
#define DC2D_VBUF_SIZE   (1024 * 1024)
#define DC2D_VBUF_BUDGET (DC2D_VBUF_SIZE - 64 * 1024)

/* The z of every 2D prim (DC7). The 2D compares depth ALWAYS and never
 * writes it, so z only matters to the 3D views in the same list (dc3d.h):
 * it is just above the background plane (KOS zclip, 0.0001) and below
 * every 3D polygon (z >= DC3D_ZK / far depth), because Flycast writes the
 * 2D sprites' depth despite their depth write being off; then a backdrop
 * drawn before a view still never hides it. */
#define DC2D_Z 0.001f

enum { DC2D_SOLID, DC2D_MUL, DC2D_ALPHA, DC2D_IMAGE, DC2D_TINT, DC2D_DARK,
       DC2D_VIEW3D,          /* DC7: a 3D view's polygons go here (dc3d.h) */
       DC2D_GLASS, DC2D_ADD, /* DC8: a translucent image, an additive glow */
       DC2D_MODES };

#define DC2D_NOTEX   0xFFFFu
#define DC2D_PF_LERP 1u          /* palette colours from the lerp bank (plat_pal_lerp) */

typedef struct DcPrim {
    float    x0, y0, x1, y1;     /* screen pixels of the 640x480 frame, x0 < x1, y0 < y1 */
    float    u0, v0, u1, v1;     /* texture coordinates (0..1 = the texture; checker repeats);
                                    u0 > u1 is a horizontal flip */
    uint32_t argb;               /* MUL / ALPHA / DARK: the colour; others: unused */
    uint16_t tex;                /* texture slot (texcache.c) or DC2D_NOTEX */
    uint8_t  mode;               /* DC2D_* */
    uint8_t  flags;              /* DC2D_PF_* */
    uint8_t  color;              /* palette index (SOLID, TINT) */
    uint8_t  pad[3];
} DcPrim;

/* A frame's list as the backend submits it: the held backdrop first (a
 * plat_hold_frame picture, U4), then the prims of this tick. */
typedef struct DcFrame {
    const DcPrim *held;  int nheld;
    const DcPrim *prims; int n;
    uint8_t  bg;                 /* plat_clear colour: the PVR background plane */
    uint8_t  lerp;               /* 1: the lerp bank is in use this frame       */
    uint8_t  lerp_bank;          /* 2 or 3 (alternates, so the frame the PVR is
                                    still drawing keeps its palette)            */
    uint32_t frame;              /* frame number (texture LRU)                  */
} DcFrame;

/* Statistics, also mirrored into the harness mailbox on the console. */
typedef struct DcStats {
    uint32_t prims, vbytes;      /* last frame                                  */
    uint32_t prims_max, vbytes_max;
    uint32_t dropped;            /* prims lost to a full list or the budget     */
    uint32_t uploads, upload_bytes, evictions, tex_fail;
    uint32_t sprites, quads, headers;     /* last frame                         */
    uint32_t shades, shade_prims, shade_fallbacks, shade_tests;  /* totals: exact shades,
                                    prims they re-drew, shades left as a multiply, prims tested */
} DcStats;
extern DcStats dc2d_stats;

/* ---- gfx2d.c: the frame ---------------------------------------------------- */
void dc2d_init(void);            /* once, after the backend is up              */
void dc2d_begin(int drawn);      /* plat_begin_frame; drawn = 0 on a skipped tick */
void dc2d_end(void);             /* plat_end_frame of a drawn tick: submit     */
const DcFrame *dc2d_last(void);  /* the frame last submitted (host tests)      */
/* DC4: a picture of its own while the tick is blocked (the VMU card): the
 * 2D calls between begin and end draw it (at most 512 prims, the font as
 * the only texture), end shows it; the tick's own list is left as it was. */
void dc2d_side_begin(void);
void dc2d_side_end(void);
/* DC7: the marker of a 3D view (dc3d_seg_end): segment `seg` of batch
 * `slot` over the frame rect x0..y1 (pixels of the 640x480 frame) */
void dc2d_view3d(int slot, int seg, float x0, float y0, float x1, float y1);
/* DC8 (the house, room.c): prims placed in frame pixels (the room's zoom
 * scales them; at zoom 1 they are the logical rect times 2, as the 2D
 * calls place theirs). dc2d_tex_rect: texture `slot` showing texels
 * [0, iw) x [0, ih) over (x0, y0)-(x1, y1), U swapped when flip; mode
 * IMAGE, GLASS, ADD or TINT, `argb` the GLASS / ADD colour and alpha,
 * `color` the TINT colour. dc2d_frect: a SOLID rect of palette colour c.
 * Nothing is drawn wholly off the frame. NULL / nothing when the list is
 * full or the tick is not drawn. */
DcPrim *dc2d_tex_rect(int mode, int slot, float x0, float y0, float x1, float y1,
                      int iw, int ih, int flip, uint32_t argb, uint8_t color);
void dc2d_frect(float x0, float y0, float x1, float y1, uint8_t c);
int  dc2d_prim_count(void);      /* prims in this tick's list so far (DC8's counters) */

/* The 2D calls. On the console they ARE the platform.h functions; the
 * host test build (-DDC2D_HOST) gives them dc2d_ names so a tee can call
 * both them and the native reference. */
#ifdef DC2D_HOST
#define DC2D_FN(n) dc2d_##n
#else
#define DC2D_FN(n) plat_##n
#endif
void DC2D_FN(clear)(uint8_t color);
void DC2D_FN(rect)(int x, int y, int w, int h, uint8_t color);
void DC2D_FN(shade_rect)(int x, int y, int w, int h, int level);
void DC2D_FN(line)(int x0, int y0, int x1, int y1, uint8_t color);
void DC2D_FN(checker)(int x, int y, int w, int h, uint8_t color);
void DC2D_FN(text)(int x, int y, const char *s, uint8_t color);
void DC2D_FN(text_outline)(int x, int y, const char *s, uint8_t color, uint8_t outline);
void DC2D_FN(image)(int x, int y, const DoomoImage *img, int scale, int flags, uint8_t color);
void DC2D_FN(image_xform)(int ax, int ay, const DoomoImage *img, int sx_q8, int sy_q8,
                          int flags, uint8_t color);
void DC2D_FN(post_offset)(int dx, int dy);
void DC2D_FN(post_scale)(int sx_q8, int sy_q8, int cx, int cy);
void DC2D_FN(post_blend)(uint8_t color, int level16);
void DC2D_FN(post_iris)(int cx, int cy, int r, int heart, uint8_t rim);
void DC2D_FN(pal_lerp)(int pal_id, int level16);
int  DC2D_FN(hold_frame)(int on);

/* ---- encoding rules shared by both backends ------------------------------- */
/* 1 when f survives the sprite's 16-bit UV (the upper half of the float). */
static inline int dc2d_uv16_exact(float f)
{
    union { float f; uint32_t u; } v;
    v.f = f;
    return (v.u & 0xFFFFu) == 0;
}
static inline float dc2d_uv16(float f)
{
    union { float f; uint32_t u; } v;
    v.f = f;
    v.u &= 0xFFFF0000u;
    return v.f;
}
/* A prim goes to the PVR as a sprite (one header colour, 64 B, 16-bit
 * UVs) when its UVs are exact in 16 bits, else as a 4-vertex polygon
 * (128 B, float UVs). Untextured prims are always sprites. */
static inline int dc2d_is_sprite(const DcPrim *p)
{
    return p->tex == DC2D_NOTEX ||
           (dc2d_uv16_exact(p->u0) && dc2d_uv16_exact(p->u1) &&
            dc2d_uv16_exact(p->v0) && dc2d_uv16_exact(p->v1));
}
/* The palette bank a prim samples (TINT, DARK and ADD: the white bank 1). */
static inline int dc2d_bank(const DcFrame *f, const DcPrim *p)
{
    if (p->mode == DC2D_TINT || p->mode == DC2D_DARK || p->mode == DC2D_ADD) return 1;
    return (p->flags & DC2D_PF_LERP) ? f->lerp_bank : 0;
}
/* The prim's colour as ARGB8888 (pal.c resolves palette colours, lerped
 * when the prim is flagged). */
uint32_t dc2d_prim_argb(const DcFrame *f, const DcPrim *p);
/* Two prims share a header (same mode, texture, bank and, for a sprite,
 * colour: a sprite's colour lives in its header). */
int dc2d_same_header(const DcFrame *f, const DcPrim *a, const DcPrim *b);

/* ---- texcache.c: PAL8 textures in video RAM --------------------------------- */
typedef struct DcTex {
    uint32_t handle;             /* backend address of the texels               */
    uint16_t w, h;               /* the image                                   */
    uint16_t tw, th;             /* the texture: powers of two, 8..1024         */
} DcTex;
void dc2d_tex_init(void);
/* The slot of an image's texture, uploaded on a miss (the pixels are read
 * now, so a buffer reused later in the frame is safe); -1 if it cannot be
 * uploaded. Keyed by uid, else by pixel pointer plus a checksum of the
 * pixels (an image without a uid may change in place). */
int  dc2d_tex_image(const DoomoImage *img);
/* DC8: the slot of the cached texture of uid `uid` (w x h) without its
 * pixels (marked as used this frame), or -1 when it is not resident; the
 * house asks before it decodes a piece's sprite. */
int  dc2d_tex_find(uint32_t uid, int w, int h);
/* A copy of texture `slot` with its texel indices sent through map[256]
 * (plat_shade_rect over an image: doomo_colormap[level]); cached under
 * (slot's key, level). -1 if it cannot be made. */
int  dc2d_tex_remap(int slot, int level, const uint8_t *map);
/* A texture that stays (font pages, the checker): slot or -1. */
int  dc2d_tex_static(const uint8_t *pix, int w, int h, int tw, int th);
/* Replace a static texture's texels (a new glyph page). */
void dc2d_tex_restatic(int slot, const uint8_t *pix, int w, int h);
const DcTex *dc2d_tex(int slot);
void dc2d_tex_mark(int slot, uint32_t frame);   /* used by this frame       */
void dc2d_tex_frame(uint32_t frame);            /* the frame being built    */
int  dc2d_tex_live(void);                       /* cached images (tests)    */
/* Twiddle (Morton order, v in the low bit; a rectangle is a row of
 * squares along its longer side) a w x h PAL8 image into a tw x th
 * texture of 16-bit pairs, padding with index 0. */
void dc2d_twiddle8(uint16_t *dst, const uint8_t *src, int w, int h, int tw, int th);
uint32_t dc2d_twiddle_index(uint32_t x, uint32_t y, int tw, int th);

/* ---- font.c: glyph pages ------------------------------------------------------ */
/* Glyph cells: 8x8 in a 128x128 page, one per byte value (0..255, so text
 * needs no lookup); the outline page: 8x12 cells (the glyph dilated by the
 * ten-pass outline stamp, origin at -1,-1) in 128x256. */
#define DC2D_GLYPH_CW 8
#define DC2D_GLYPH_CH 8
#define DC2D_OUTL_CW  8
#define DC2D_OUTL_CH  12
void dc2d_font_init(void);
void dc2d_font_frame(void);      /* rebuild the pages if the language's glyph page changed */
int  dc2d_font_tex(void);        /* slot of the glyph page   */
int  dc2d_font_outline_tex(void);/* slot of the outline page */
int  dc2d_font_blank(unsigned ch);/* 1: the glyph has no ink (space) */
int  dc2d_checker_tex(void);     /* slot of the 8x8 checker  */
/* Build the pages for glyph page `page` (lang.h DoomoLangInfo.page) into
 * glyph[128*128] and outline[128*256] (host tests). */
void dc2d_font_build(int page, uint8_t *glyph, uint8_t *outline);

/* ---- pal.c: palettes and shade factors --------------------------------------- */
void     dc2d_pal_init(void);                   /* banks 0 and 1 to the backend */
uint32_t dc2d_pal_argb(int bank, uint8_t index);/* ARGB8888 of an entry        */
uint16_t dc2d_pal_1555(int bank, uint8_t index);
void     dc2d_pal_lerp_build(int bank, int pal_id, int level16);
uint32_t dc2d_shade_argb(int level);            /* MUL colour of a colormap level */
uint32_t dc2d_shadow_argb(void);                /* DARK colour of IMG_SHADOW    */

/* ---- the backend (pvr2d.c on the console, swpvr.c on the host) ---------------- */
int  dc2d_be_tex_alloc(uint32_t bytes, uint32_t *handle);   /* 1 ok */
void dc2d_be_tex_free(uint32_t handle);
void dc2d_be_tex_write(uint32_t handle, const uint16_t *src, uint32_t bytes);
void dc2d_be_tex_read(uint32_t handle, uint16_t *dst, uint32_t bytes);
void dc2d_be_pal(int bank, const uint16_t *argb1555);       /* 256 entries   */
void dc2d_be_submit(const DcFrame *f);
void dc2d_be_text(const char *s);                           /* harness probe */
void dc2d_be_wait_idle(void);       /* the PVR has finished the frame it was drawing */
void dc2d_be_log(const char *msg);

#endif
