/* dc3d.h - 3D polygons inside the 2D layer's painter's order (DC7; owner:
 * dreamcast).
 *
 * The 2D layer (dc2d.h) sends each frame to the PVR as ONE translucent
 * list in presort mode: the PVR draws it in submission order, which is the
 * core's painter's order. A 3D view (plat_overworld here; plat_view3d and
 * plat_room can use the same path) is built into a BATCH of polygons that
 * are already projected to the 640x480 frame, and the 2D list gets a
 * MARKER prim (DC2D_VIEW3D) where the core called the view. When the
 * backend meets the marker it sends the marker's SEGMENT of the batch, in
 * the same translucent list:
 *   - so whatever the core drew before the view is under it, whatever it
 *     draws after (the HUD, dialogue, fades) is on top, exactly as on the
 *     native runner;
 *   - inside the segment, depth is real: polygons carry z = 1/w (bigger is
 *     nearer) and compare GEQUAL against the depth buffer. In presort mode
 *     the PVR honours each translucent polygon's depth compare and depth
 *     write settings, so the opaque terrain writes depth (in any order)
 *     and the billboards and shadows that follow test against it without
 *     writing. The 2D prims compare ALWAYS and never write, so they leave
 *     the depth buffer alone.
 * The marker keeps the segment's viewport as it was built; the 2D post
 * effects move or scale the marker's rect like any prim (screen shake,
 * zoom), and the backend maps every vertex from the built viewport to the
 * marker's rect. A full-screen exact shade copies the marker with a colour
 * multiplier (the shade's fitted per-channel factor, pal.c), a palette
 * lerp switches its textures to the lerp bank, plat_hold_frame keeps its
 * segment alive in the held batch. A partial shade over a 3D view is the
 * MUL fallback (the frame times the factor), because the exact path would
 * draw the remapped background over the view.
 *
 * Portable C (no KallistiOS): dc3d.c, overworld.c; the host tests build
 * them with a software PVR (dc/tests/gfx2d/swpvr.c). The console backend
 * is pvr3d.c.
 */
#ifndef DC_DC3D_H
#define DC_DC3D_H

#include <stdint.h>
#include "dc2d.h"

/* Batches: two for the frames (the one being built and the one just
 * shown, which plat_hold_frame may keep) and the held one. */
#define DC3D_SLOTS      3
#define DC3D_HELD       2
#define DC3D_MAX_VERTS  12288
#define DC3D_MAX_POLYS  4096
#define DC3D_MAX_SEGS   8
#define DC3D_SEG_TEX    64          /* distinct textures a segment remembers (LRU marks) */
/* The bytes the TA is sent for one segment must stay well inside the
 * translucent vertex buffer the 2D layer budgets (DC2D_VBUF_BUDGET), so
 * the HUD drawn after the view always fits: a builder stops adding
 * polygons past this (and counts what it left out). */
#define DC3D_SEG_BUDGET (576u * 1024u)

/* Polygon modes. Every mode compares depth GEQUAL (nearer or equal wins). */
enum {
    DC3D_OPAQUE,    /* textured, texel x colour + offset, no blending, writes depth (terrain) */
    DC3D_SPRITE,    /* textured, alpha from the texture x the colour's alpha, no depth write */
    DC3D_BLEND,     /* untextured, Gouraud colour with alpha, no depth write (blob shadows) */
    DC3D_MODES
};

/* A vertex in the 640x480 frame: x, y in pixels, z = DC3D_ZK / camera depth,
 * u, v texture coordinates (0..1 = the texture, repeating), argb the
 * colour the texel is multiplied by (and the alpha), oargb the offset
 * colour added after it (lamp glow, distance fade, tint; alpha unused). */
typedef struct DcVert {
    float    x, y, z, u, v;
    uint32_t argb, oargb;
} DcVert;

/* The soft blob of the billboards' shadows: a texture of its own (not a
 * texcache.c slot), 32x32 ARGB4444, black with the alpha falling off from
 * the middle (ARGB1555 palettes have no soft alpha). */
#define DC3D_TEX_BLOB  0xFFFEu
#define DC3D_BLOB_SIZE 32
const uint16_t *dc3d_blob(void);           /* its texels, row-major (not twiddled) */

/* One triangle strip: vertices [first, first + nv), nv >= 3. */
typedef struct DcPoly {
    uint32_t first;
    uint8_t  nv;
    uint8_t  mode;               /* DC3D_* */
    uint16_t tex;                /* texture slot (texcache.c), DC3D_TEX_BLOB or DC2D_NOTEX */
} DcPoly;

/* A segment: the polygons of one view call. */
typedef struct DcSeg {
    uint32_t poly0, npoly;
    float    vx0, vy0, vx1, vy1; /* the viewport it was built for, frame pixels */
    uint32_t bytes;              /* what the TA is sent for it (headers + vertices) */
    uint16_t ntex;
    uint16_t tex[DC3D_SEG_TEX];  /* the distinct textures it uses (LRU marks) */
    uint8_t  tex_more;           /* 1: more than DC3D_SEG_TEX: mark from the polygons */
} DcSeg;

typedef struct DcBatch {
    DcVert   v[DC3D_MAX_VERTS];
    DcPoly   p[DC3D_MAX_POLYS];
    DcSeg    seg[DC3D_MAX_SEGS];
    int      nv, np, nseg;
} DcBatch;

/* z = DC3D_ZK / depth: z = 4 at DC3D_ZNEAR, 0.0064 at a depth of 10000
 * units (far beyond any view), all above the 2D prims' DC2D_Z (0.001) and
 * the background plane's (KOS zclip, 0.0001) */
#define DC3D_ZK    64.0f
#define DC3D_ZNEAR 16.0f

/* Statistics (mirrored into the harness mailbox on the console). */
typedef struct Dc3dStats {
    uint32_t polys, verts, bytes;          /* last segment built */
    uint32_t polys_max, bytes_max;
    uint32_t dropped;                      /* polygons left out (batch, budget or held
                                              batch full), total */
    uint32_t clipped;                      /* polygons cut at the near plane, total */
    uint32_t culled;                       /* polygons wholly off the viewport, total */
    uint32_t segs;                         /* segments built, total */
    uint32_t us_last, us_max, us_sum, us_n;/* build time of the views (console timer) */
} Dc3dStats;
extern Dc3dStats dc3d_stats;

/* ---- dc3d.c: batches --------------------------------------------------------------- */
void dc3d_init(void);
void dc3d_frame(uint32_t frame);           /* dc2d_begin of a drawn frame: the new frame's batch */
DcBatch *dc3d_batch(int slot);
int  dc3d_cur_slot(void);

/* Building a segment: begin, add polygons, end (which appends the marker
 * prim to the 2D list over the logical viewport). seg_begin returns 0 when
 * no segment is free (the view is not drawn). */
int     dc3d_seg_begin(int vx, int vy, int vw, int vh);
DcVert *dc3d_poly(int mode, int tex, int nv);  /* room for nv vertices, NULL when full */
int     dc3d_seg_polys(void);                   /* polygons in the open segment so far */
void    dc3d_sort_from(int poly0);              /* group polygons [poly0, now) by texture
                                                   (order-free opaque ones only) */
void    dc3d_seg_end(void);

/* The 2D layer's hooks (gfx2d.c). */
uint32_t dc3d_seg_bytes(int slot, int seg);
void     dc3d_seg_mark(int slot, int seg, uint32_t frame);      /* texture LRU */
/* plat_hold_frame: copy segment `seg` of `slot` into the held batch
 * (reset first when `reset`); the new segment index, -1 if it does not fit
 * (its polygons then count in dc3d_stats.dropped) */
int      dc3d_hold_seg(int slot, int seg, int reset);

/* ---- the camera (dc3d.c): a pinhole looking along `fwd` ------------------------------ */
typedef struct Dc3dCam {
    float ex, ey, ez;                       /* eye, world units */
    float rx, ry, rz;                       /* right    (screen +x) */
    float dx, dy, dz;                       /* down     (screen +y) */
    float fx, fy, fz;                       /* forward  (depth)     */
    float cx, cy, focal;                    /* screen centre and focal length, frame pixels */
    float vx0, vy0, vx1, vy1;               /* viewport, frame pixels */
} Dc3dCam;

/* A world-space vertex before projection. */
typedef struct Dc3dIn {
    float    x, y, z, u, v;
    uint32_t argb, oargb;
} Dc3dIn;

/* Project a convex polygon (n <= 8 vertices in order around it: a quad is
 * TL TR BR BL) and add it as `mode` / `tex`, as a triangle strip: clipped
 * at the near plane, culled when it is wholly off the viewport. 1 when
 * something was added. */
int dc3d_add(const Dc3dCam *c, int mode, int tex, const Dc3dIn *in, int n);
/* camera space of a world point: depth (0 if behind), screen x/y */
float dc3d_project(const Dc3dCam *c, float x, float y, float z, float *sx, float *sy);

/* ---- the views ------------------------------------------------------------------------- */
#ifdef DC2D_HOST
void dc2d_overworld(const DoomoOverworldView *v);   /* overworld.c (console: plat_overworld) */
const Dc3dCam *dc2d_overworld_cam(void);            /* the last view's camera (tests) */
#endif

/* ---- the backends -------------------------------------------------------------------- */
/* pvr3d.c (console) / swpvr.c (host): draw the segment a marker prim names,
 * mapped to the marker's rect, colours times the marker's multiplier,
 * textures through bank `bank`. */
void dc3d_be_draw(const DcFrame *f, const DcPrim *marker);
uint32_t dc3d_be_us(void);       /* a microsecond clock for the build times (host: 0) */

/* The marker prim: mode DC2D_VIEW3D, color = segment, pad[0] = batch
 * slot, tex = DC2D_NOTEX (so the 2D layer's texture marks skip it), argb =
 * colour multiplier (0xFFFFFFFF = none), x0..y1 = where the segment's
 * viewport is now, flags DC2D_PF_LERP = textures through the lerp bank. */
static inline int dc3d_marker_slot(const DcPrim *p) { return p->pad[0]; }
static inline int dc3d_marker_seg(const DcPrim *p) { return p->color; }

#endif
