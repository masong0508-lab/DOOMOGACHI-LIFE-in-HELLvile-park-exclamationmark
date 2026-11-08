/* gfx2d.c - the platform.h 2D layer and post effects on the PVR (DC3;
 * owner: dreamcast). Portable: see dc2d.h for the design.
 *
 * Each call appends axis-aligned prims to the frame's list in the order
 * the core makes them; plat_end_frame submits the list as one translucent
 * list in presort mode, so the PVR keeps that order. Logical pixel (x, y)
 * is the 2x2 block at (2x, 2y) of the 640x480 frame.
 *
 *   plat_clear       the PVR background plane; the prims so far are dropped
 *                    (nothing can show through a clear). An opaque rect over
 *                    the whole screen does the same.
 *   plat_rect        a SOLID sprite, clipped to the screen. The core fills
 *                    shapes with thousands of 1-row spans, so a rect that
 *                    continues the previous one (same colour, next to it
 *                    along a row or a column) grows it instead: the same
 *                    pixels in the same order, far fewer prims.
 *   plat_line        the native runner's Bresenham walk, as runs of pixels
 *                    (one SOLID rect per straight run): pixel-exact.
 *   plat_shade_rect  EXACT, in palette space: the native runner remaps every
 *                    pixel through doomo_colormap[level]; the PVR cannot read
 *                    the frame, but the list holds everything under the rect,
 *                    so the rect's part of every earlier prim is drawn again
 *                    with its colour remapped (an image through a remapped
 *                    copy of its texture, texcache.c), over the remapped
 *                    background. A full-screen shade replaces the list by its
 *                    remapped copy. Only when that would cost too many prims
 *                    (or a texture cannot be made) is it a MUL sprite: the
 *                    frame times the level's fitted per-channel factor (pal.c).
 *                    Blended prims under the rect (shadows, blends) are drawn
 *                    again as they are, over the remapped picture.
 *   plat_checker     the 8x8 checker texture, repeating, times the colour:
 *                    the exact 50% pattern, phase at the rect's corner.
 *   plat_text        one TINT sprite per inked glyph from the glyph page;
 *                    plat_text_outline first draws every character's
 *                    outline-page cell in the outline colour, then the text:
 *                    exactly the ten passes of the weak default.
 *   plat_image       the image's texture (texcache.c) at scale 1..4; FLIPX
 *                    swaps the U coordinates; SILHOUETTE is a TINT of the
 *                    colour; SHADOW darkens the frame one scaled pixel down-
 *                    right of every opaque pixel, exactly (exact shades over
 *                    the image's runs merged into rects), or, for a shape
 *                    of more than 48 rects, a DARK copy (one grey factor).
 *   plat_image_xform any Q8.8 scale about the feet as ONE quad (the weak
 *                    default rounds it to rect runs); the shadow is a black
 *                    silhouette at (+1, +1) as in the default.
 *   post offset/scale  move / scale the prims drawn so far (the picture;
 *                    the HUD drawn after camfx_apply stays put)
 *   post blend       an ALPHA sprite over the frame (level 16: solid)
 *   post iris        black outside camfx_iris_row()'s spans, plus the 1 px
 *                    rim: the weak default's exact pixels, as merged rects
 *   pal lerp         bank 2/3 = the lerped palette (camfx_pal_entry); the
 *                    prims drawn so far switch to it
 *   plat_hold_frame  U4: the last submitted list is kept and submitted under
 *                    every new frame until released, so the core repaints
 *                    only its own rects (a held frame keeps no palette lerp).
 *   3D views (DC7)   a marker prim (DC2D_VIEW3D, dc3d.h) where the core
 *                    called the view: the backend sends the view's polygons
 *                    there. Full-screen exact shades copy it with a colour
 *                    multiplier, palette lerps flag it, post effects move it,
 *                    plat_hold_frame keeps its polygons (the held batch); a
 *                    partial shade over it is the MUL fallback.
 */
#include <string.h>
#include "dc2d.h"
#include "dc3d.h"
#include "doomo/doomo.h"
#include "doomo/camfx.h"
#include "doomo/font.h"

DcStats dc2d_stats;

static DcPrim  buf_a[DC2D_MAX_PRIMS], buf_b[DC2D_MAX_PRIMS], held_buf[DC2D_MAX_PRIMS];
static DcPrim *cur = buf_a, *prev = buf_b;
static int     ncur, nprev, nheld;
static int     seal;              /* no rect merges into prims before this     */
static int     drawing;           /* inside a drawn tick                       */
static int     held, cur_cleared, prev_on_held;
static uint8_t bg_cur, bg_prev, bg_held;
static uint8_t lerp_on, lerp_bank;
static uint32_t frame_no;
static DcFrame last_frame;
static int     ncap = DC2D_MAX_PRIMS;    /* capacity of cur (the side frame's is smaller) */

/* bounding boxes of 64-prim chunks (exact shades test a chunk before its
 * prims): cbox for the frame's list, filled lazily up to cbox_n; hbox for
 * the held backdrop */
#define CHUNK_SHIFT 6
#define NCHUNK ((DC2D_MAX_PRIMS >> CHUNK_SHIFT) + 1)
typedef struct Box { float x0, y0, x1, y1; } Box;
static Box cbox[NCHUNK], hbox[NCHUNK];
static int cbox_n;
/* a shade that would leave more prims than this in the list is a MUL
 * (about 90 B a prim: 9000 prims stay well inside the vertex budget, with
 * room for the HUD drawn after); an exact IMG_SHADOW only below 6000 */
#define SHADE_MAX_PRIMS 9000
#define SHADOW_EXACT_MAX_PRIMS 6000
/* the last exact shades of the frame: everything drawn before a shade's
 * copy and lying wholly inside its rect is hidden by that copy (its
 * remapped background is opaque), so a later shade skips it; without this,
 * nested and growing shades (vignette bands) would copy copies */
#define NGROUPS 16
typedef struct Group { float x0, y0, x1, y1; int start; } Group;
static Group groups[NGROUPS];
static int ngroups, group_wr;
/* DC7: the 3D markers in the frame's list and in the held backdrop: how
 * many, and a box around them (conservative: post effects widen it) */
static int n3d_cur, n3d_held;
static Box m3d_cur, m3d_held;

/* ---- the list ---------------------------------------------------------------------- */
static DcPrim *emit(int mode)
{
    DcPrim *p;
    if (ncur >= ncap) { dc2d_stats.dropped++; return NULL; }
    p = &cur[ncur++];
    p->mode = (uint8_t)mode;
    p->flags = 0;
    p->tex = DC2D_NOTEX;
    p->argb = 0;
    p->color = 0;
    p->u0 = p->v0 = p->u1 = p->v1 = 0.0f;
    return p;
}

static void set_xy(DcPrim *p, int x, int y, int w, int h)
{
    p->x0 = (float)(x * DC2D_SCALE);
    p->y0 = (float)(y * DC2D_SCALE);
    p->x1 = (float)((x + w) * DC2D_SCALE);
    p->y1 = (float)((y + h) * DC2D_SCALE);
}

static int clip(int *x, int *y, int *w, int *h)
{
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > DC2D_LW) *w = DC2D_LW - *x;
    if (*y + *h > DC2D_LH) *h = DC2D_LH - *y;
    return *w > 0 && *h > 0;
}

static void box_empty(Box *b) { b->x0 = b->y0 = 1e9f; b->x1 = b->y1 = -1e9f; }
static void box_add(Box *b, const DcPrim *p)
{
    if (p->x0 < b->x0) b->x0 = p->x0;
    if (p->y0 < b->y0) b->y0 = p->y0;
    if (p->x1 > b->x1) b->x1 = p->x1;
    if (p->y1 > b->y1) b->y1 = p->y1;
}
static void cbox_update(void)
{
    int i;
    for (i = cbox_n; i < ncur; i++) {
        if ((i & ((1 << CHUNK_SHIFT) - 1)) == 0) box_empty(&cbox[i >> CHUNK_SHIFT]);
        box_add(&cbox[i >> CHUNK_SHIFT], &cur[i]);
    }
    cbox_n = ncur;
}

/* everything drawn so far is hidden: start the list again */
static void covered(void)
{
    n3d_cur = 0;
    ncur = 0;
    seal = 0;
    cbox_n = 0;
    ngroups = group_wr = 0;
    cur_cleared = 1;
}

static void solid(int x, int y, int w, int h, uint8_t c)
{
    DcPrim *p;
    float X0, Y0, X1, Y1;
    if (!clip(&x, &y, &w, &h)) return;
    if (x == 0 && y == 0 && w == DC2D_LW && h == DC2D_LH) covered();
    X0 = (float)(x * DC2D_SCALE); Y0 = (float)(y * DC2D_SCALE);
    X1 = (float)((x + w) * DC2D_SCALE); Y1 = (float)((y + h) * DC2D_SCALE);
    if (ncur > seal) {
        p = &cur[ncur - 1];
        if (p->mode == DC2D_SOLID && p->color == c && p->flags == 0) {
            int grew = 0;
            if (p->y0 == Y0 && p->y1 == Y1 && p->x1 == X0) { p->x1 = X1; grew = 1; }
            else if (p->x0 == X0 && p->x1 == X1 && p->y1 == Y0) { p->y1 = Y1; grew = 1; }
            if (grew) {
                if (ncur - 1 < cbox_n) box_add(&cbox[(ncur - 1) >> CHUNK_SHIFT], p);
                return;
            }
        }
    }
    p = emit(DC2D_SOLID);
    if (!p) return;
    p->color = c;
    p->x0 = X0; p->y0 = Y0; p->x1 = X1; p->y1 = Y1;
}

/* a textured prim: logical rect (x, y, w, h) showing texels [0, iw) x [0, ih)
 * of slot `slot` (flipped: right to left) */
static DcPrim *textured(int mode, int slot, int x, int y, int w, int h, int iw, int ih, int flip)
{
    const DcTex *t = dc2d_tex(slot);
    DcPrim *p = emit(mode);
    float u1;
    if (!p) return NULL;
    p->tex = (uint16_t)slot;
    set_xy(p, x, y, w, h);
    u1 = (float)iw / (float)t->tw;
    p->u0 = flip ? u1 : 0.0f;
    p->u1 = flip ? 0.0f : u1;
    p->v0 = 0.0f;
    p->v1 = (float)ih / (float)t->th;
    return p;
}

static int offscreen(int x, int y, int w, int h)
{
    return x >= DC2D_LW || y >= DC2D_LH || x + w <= 0 || y + h <= 0;
}

/* ---- the frame ------------------------------------------------------------------------ */
void dc2d_init(void)
{
    memset(&dc2d_stats, 0, sizeof(dc2d_stats));
    dc2d_tex_init();
    dc2d_pal_init();
    dc2d_font_init();
    dc3d_init();
    ncur = nprev = nheld = 0;
    n3d_cur = n3d_held = 0;
    held = drawing = 0;
}

void dc2d_begin(int drawn)
{
    int i;
    if (!drawn) { drawing = 0; return; }
    frame_no++;
    dc2d_tex_frame(frame_no);
    dc2d_font_frame();
    dc3d_frame(frame_no);
    n3d_cur = 0;
    ncur = 0;
    seal = 0;
    cbox_n = 0;
    ngroups = group_wr = 0;
    cur_cleared = 0;
    lerp_on = 0;
    lerp_bank = (frame_no & 1) ? 3 : 2;
    bg_cur = held ? bg_held : bg_prev;
    drawing = 1;
    if (held)                           /* the backdrop's textures stay resident */
        for (i = 0; i < nheld; i++) {
            if (held_buf[i].tex != DC2D_NOTEX) dc2d_tex_mark(held_buf[i].tex, frame_no);
            else if (held_buf[i].mode == DC2D_VIEW3D)
                dc3d_seg_mark(dc3d_marker_slot(&held_buf[i]), dc3d_marker_seg(&held_buf[i]), frame_no);
        }
}

static uint32_t prim_bytes(const DcFrame *f, const DcPrim *last, const DcPrim *p)
{
    uint32_t b = dc2d_is_sprite(p) ? 64u : 128u;
    if (!last || !dc2d_same_header(f, last, p)) { b += 32u; dc2d_stats.headers++; }
    return b;
}

void dc2d_end(void)
{
    DcFrame *f = &last_frame;
    const DcPrim *lastp = NULL;
    DcPrim *tmp;
    uint32_t bytes = 0;
    int i, k, use_held, n[2];
    if (!drawing) return;
    drawing = 0;
    use_held = held && !cur_cleared;
    f->held = use_held ? held_buf : NULL;
    f->nheld = use_held ? nheld : 0;
    f->prims = cur;
    f->n = ncur;
    f->bg = bg_cur;
    f->lerp = lerp_on;
    f->lerp_bank = lerp_bank;
    f->frame = frame_no;
    /* the vertex budget, in submission order */
    dc2d_stats.headers = dc2d_stats.sprites = dc2d_stats.quads = 0;
    n[0] = f->nheld; n[1] = f->n;
    for (k = 0; k < 2; k++) {
        const DcPrim *list = k ? f->prims : f->held;
        for (i = 0; i < n[k]; i++) {
            uint32_t b = list[i].mode == DC2D_VIEW3D
                       ? dc3d_seg_bytes(dc3d_marker_slot(&list[i]), dc3d_marker_seg(&list[i]))
                       : prim_bytes(f, lastp, &list[i]);
            if (bytes + b > DC2D_VBUF_BUDGET) {
                dc2d_stats.dropped += (uint32_t)(n[k] - i) + (k ? 0u : (uint32_t)n[1]);
                if (k) f->n = i;
                else { f->nheld = i; f->n = 0; }
                k = 2;
                break;
            }
            bytes += b;
            if (list[i].mode == DC2D_VIEW3D) {
                /* the backend sends the view's own headers: the next 2D
                 * prim needs its header again */
                dc3d_seg_mark(dc3d_marker_slot(&list[i]), dc3d_marker_seg(&list[i]), frame_no);
                lastp = NULL;
                continue;
            }
            if (dc2d_is_sprite(&list[i])) dc2d_stats.sprites++; else dc2d_stats.quads++;
            if (list[i].tex != DC2D_NOTEX) dc2d_tex_mark(list[i].tex, frame_no);
            lastp = &list[i];
        }
    }
    dc2d_stats.prims = (uint32_t)(f->nheld + f->n);
    dc2d_stats.vbytes = bytes;
    if (dc2d_stats.prims > dc2d_stats.prims_max) dc2d_stats.prims_max = dc2d_stats.prims;
    if (bytes > dc2d_stats.vbytes_max) dc2d_stats.vbytes_max = bytes;
    dc2d_be_submit(f);
    /* this frame becomes "the picture just shown" (plat_hold_frame) */
    tmp = prev; prev = cur; cur = tmp;
    nprev = ncur;
    prev_on_held = use_held;
    bg_prev = bg_cur;
    f->prims = prev;
}

const DcFrame *dc2d_last(void) { return &last_frame; }

/* ---- a side frame (DC4) ------------------------------------------------------------------
 * A picture shown while the game's own tick is blocked (the VMU card of
 * vmusave.c): the tick's list is put aside, the 2D calls fill a small list
 * of their own, the PVR shows it at once, and the tick's list comes back
 * untouched. Its textures are the font pages (static) and none other. */
#define SIDE_PRIMS 512
static DcPrim side_buf[SIDE_PRIMS];
static struct {
    DcPrim *cur;
    int ncur, ncap, seal, cbox_n, ngroups, group_wr, drawing, cur_cleared, active;
    uint8_t bg_cur, lerp_on;
} side;

void dc2d_side_begin(void)
{
    if (side.active) return;
    side.cur = cur; side.ncur = ncur; side.ncap = ncap; side.seal = seal; side.cbox_n = cbox_n;
    side.ngroups = ngroups; side.group_wr = group_wr; side.drawing = drawing;
    side.cur_cleared = cur_cleared; side.bg_cur = bg_cur; side.lerp_on = lerp_on;
    side.active = 1;
    cur = side_buf; ncap = SIDE_PRIMS; ncur = 0; seal = 0; cbox_n = 0;
    ngroups = group_wr = 0; cur_cleared = 0; lerp_on = 0; bg_cur = PAL_BLACK;
    drawing = 1;
}

void dc2d_side_end(void)
{
    DcFrame f;
    int i;
    if (!side.active) return;
    memset(&f, 0, sizeof(f));
    f.prims = side_buf;
    f.n = ncur;
    f.bg = bg_cur;
    f.lerp_bank = 2;
    f.frame = frame_no;
    for (i = 0; i < ncur; i++)
        if (side_buf[i].tex != DC2D_NOTEX) dc2d_tex_mark(side_buf[i].tex, frame_no);
    dc2d_be_submit(&f);
    cur = side.cur; ncur = side.ncur; ncap = side.ncap; seal = side.seal; cbox_n = side.cbox_n;
    ngroups = side.ngroups; group_wr = side.group_wr; drawing = side.drawing;
    cur_cleared = side.cur_cleared; bg_cur = side.bg_cur; lerp_on = side.lerp_on;
    side.active = 0;
}

int DC2D_FN(hold_frame)(int on)
{
    int k;
    if (on == 4) return held;
    if (!on) { held = 0; return 0; }
    /* the picture just shown: its own backdrop (if it had one) + its prims */
    {
        int reset3d;
        if (!(held && prev_on_held)) nheld = 0;
        /* a new backdrop starts the held batch again, unless the picture
         * itself shows held polygons (a full-screen shade copied them) */
        reset3d = nheld == 0;
        for (k = 0; k < nprev && reset3d; k++)
            if (prev[k].mode == DC2D_VIEW3D && dc3d_marker_slot(&prev[k]) == DC3D_HELD) reset3d = 0;
        held = 0;
        for (k = 0; k < nprev && nheld < DC2D_MAX_PRIMS; k++) {
            DcPrim q = prev[k];
            if (q.mode == DC2D_VIEW3D && dc3d_marker_slot(&q) != DC3D_HELD) {
                /* its polygons move to the held batch */
                int s = dc3d_hold_seg(dc3d_marker_slot(&q), dc3d_marker_seg(&q), reset3d);
                reset3d = 0;
                if (s < 0) continue;
                q.pad[0] = DC3D_HELD;
                q.color = (uint8_t)s;
            }
            held_buf[nheld] = q;
            held_buf[nheld].flags &= (uint8_t)~DC2D_PF_LERP;
            nheld++;
        }
    }
    n3d_held = 0;
    box_empty(&m3d_held);
    for (k = 0; k < nheld; k++) {
        if ((k & ((1 << CHUNK_SHIFT) - 1)) == 0) box_empty(&hbox[k >> CHUNK_SHIFT]);
        box_add(&hbox[k >> CHUNK_SHIFT], &held_buf[k]);
        if (held_buf[k].mode == DC2D_VIEW3D) { n3d_held++; box_add(&m3d_held, &held_buf[k]); }
    }
    bg_held = bg_prev;
    held = 1;
    return 1;
}

/* ---- 2D ---------------------------------------------------------------------------------- */
void DC2D_FN(clear)(uint8_t color)
{
    if (!drawing) return;
    covered();
    bg_cur = color;
}

void DC2D_FN(rect)(int x, int y, int w, int h, uint8_t color)
{
    if (!drawing) return;
    solid(x, y, w, h, color);
}

/* the part of prim p inside [X0, X1) x [Y0, Y1), its colour sent through
 * colormap level `level` (exact shade); 0 if the list is full */
/* per-channel product of two ARGB colours (255 = 1) */
static uint32_t argb_mul(uint32_t a, uint32_t b)
{
    uint32_t r = 0;
    int k;
    for (k = 0; k < 32; k += 8)
        r |= ((((a >> k) & 255u) * ((b >> k) & 255u) + 127u) / 255u) << k;
    return r;
}

static int reemit(const DcPrim *p, float X0, float Y0, float X1, float Y1, int level)
{
    DcPrim *q;
    if (p->x1 <= X0 || p->x0 >= X1 || p->y1 <= Y0 || p->y0 >= Y1) return 1;
    dc2d_stats.shade_prims++;
    if (p->mode == DC2D_VIEW3D) {       /* only full-screen shades get here: the */
        q = emit(DC2D_VIEW3D);          /* view again, its colours times the factor */
        if (!q) return 0;
        *q = *p;
        q->argb = argb_mul(p->argb, dc2d_shade_argb(level) | 0xFF000000u);
        return 1;
    }
    q = emit(p->mode);
    if (!q) return 0;
    *q = *p;
    if (p->x0 < X0) { q->x0 = X0; q->u0 = p->u0 + (X0 - p->x0) / (p->x1 - p->x0) * (p->u1 - p->u0); }
    if (p->x1 > X1) { q->x1 = X1; q->u1 = p->u0 + (X1 - p->x0) / (p->x1 - p->x0) * (p->u1 - p->u0); }
    if (p->y0 < Y0) { q->y0 = Y0; q->v0 = p->v0 + (Y0 - p->y0) / (p->y1 - p->y0) * (p->v1 - p->v0); }
    if (p->y1 > Y1) { q->y1 = Y1; q->v1 = p->v0 + (Y1 - p->y0) / (p->y1 - p->y0) * (p->v1 - p->v0); }
    switch (p->mode) {
    case DC2D_SOLID:
    case DC2D_TINT:
        q->color = doomo_colormap[level][p->color];
        break;
    case DC2D_GLASS:                    /* DC8: a translucent image: its texels remapped, */
    case DC2D_IMAGE: {                  /* blended as before                              */
        int t = dc2d_tex_remap(p->tex, level, doomo_colormap[level]);
        if (t >= 0) {
            q->tex = (uint16_t)t;
        } else if (p->mode == DC2D_GLASS) {   /* no texture: its colour times the factor */
            q->argb = argb_mul(p->argb, dc2d_shade_argb(level) | 0xFF000000u);
        } else {                        /* no texture: the image times the fitted factor */
            DcPrim *m = emit(DC2D_MUL);
            if (!m) return 0;
            *m = *q;
            m->mode = DC2D_MUL;
            m->tex = DC2D_NOTEX;
            m->argb = dc2d_shade_argb(level);
        }
        break;
    }
    case DC2D_ADD:                      /* DC8: a glow dims with the picture under it */
        q->argb = argb_mul(p->argb, dc2d_shade_argb(level) | 0xFF000000u);
        break;
    default:                            /* blends: again, over the remapped picture */
        break;
    }
    return 1;
}

/* prim p (index i of the frame's list; -1: the held backdrop, under
 * everything) is hidden by the copy of a later exact shade */
static int hidden(const DcPrim *p, int i)
{
    int g;
    for (g = 0; g < ngroups; g++) {
        const Group *q = &groups[g];
        if (q->start > i && p->x0 >= q->x0 && p->x1 <= q->x1 && p->y0 >= q->y0 && p->y1 <= q->y1)
            return 1;
    }
    return 0;
}

static int reemit_list(const DcPrim *list, int n, const Box *box, int is_held, float X0, float Y0,
                       float X1, float Y1, int level)
{
    int c, i;
    for (c = 0; c <= (n - 1) >> CHUNK_SHIFT && n > 0; c++) {
        int i1 = (c + 1) << CHUNK_SHIFT;
        const Box *b = &box[c];
        if (b->x1 <= X0 || b->x0 >= X1 || b->y1 <= Y0 || b->y0 >= Y1) continue;
        if (i1 > n) i1 = n;
        for (i = c << CHUNK_SHIFT; i < i1; i++) {
            const DcPrim *p = &list[i];
            dc2d_stats.shade_tests++;
            if (p->x1 <= X0 || p->x0 >= X1 || p->y1 <= Y0 || p->y0 >= Y1) continue;
            if (ngroups && hidden(p, is_held ? -1 : i)) continue;
            if (!reemit(p, X0, Y0, X1, Y1, level)) return 0;
        }
    }
    return 1;
}

static int box_hits(const Box *b, float X0, float Y0, float X1, float Y1)
{
    return !(b->x1 <= X0 || b->x0 >= X1 || b->y1 <= Y0 || b->y0 >= Y1);
}

static void m3d_rescan(void)
{
    int i;
    n3d_cur = 0;
    box_empty(&m3d_cur);
    for (i = 0; i < ncur; i++)
        if (cur[i].mode == DC2D_VIEW3D) { n3d_cur++; box_add(&m3d_cur, &cur[i]); }
}

/* a post effect moved or scaled the prims: the 3D box covers everything */
static void m3d_widen(void)
{
    if (n3d_cur) { m3d_cur.x0 = m3d_cur.y0 = -1e9f; m3d_cur.x1 = m3d_cur.y1 = 1e9f; }
}

void dc2d_view3d(int slot, int seg, float x0, float y0, float x1, float y1)
{
    DcPrim *p;
    if (!drawing) return;
    p = emit(DC2D_VIEW3D);
    if (!p) return;
    p->x0 = x0; p->y0 = y0; p->x1 = x1; p->y1 = y1;
    p->u0 = x0; p->v0 = y0; p->u1 = x1; p->v1 = y1;
    p->argb = 0xFFFFFFFFu;
    p->color = (uint8_t)seg;
    p->pad[0] = (uint8_t)slot;
    p->pad[1] = p->pad[2] = 0;
    seal = ncur;
    if (!n3d_cur) box_empty(&m3d_cur);
    n3d_cur++;
    box_add(&m3d_cur, p);
}

void DC2D_FN(shade_rect)(int x, int y, int w, int h, int level)
{
    DcPrim *p;
    int n0, ok = 1, full, use_held;
    float X0, Y0, X1, Y1;
    if (!drawing || !clip(&x, &y, &w, &h)) return;
    level = DOOMO_CLAMP(level, 0, DOOMO_LIGHT_LEVELS - 1);
    if (level == DOOMO_LIGHT_LEVELS - 1) return;    /* colormap 15 = unchanged */
    X0 = (float)(x * DC2D_SCALE); Y0 = (float)(y * DC2D_SCALE);
    X1 = (float)((x + w) * DC2D_SCALE); Y1 = (float)((y + h) * DC2D_SCALE);
    full = x == 0 && y == 0 && w == DC2D_LW && h == DC2D_LH;
    use_held = held && !cur_cleared;
    n0 = ncur;
    /* DC7: the exact path would paint the remapped background over a 3D
     * view under a part of the screen: multiply the frame instead */
    if (!full && ((n3d_cur && box_hits(&m3d_cur, X0, Y0, X1, Y1)) ||
                  (use_held && n3d_held && box_hits(&m3d_held, X0, Y0, X1, Y1))))
        goto fallback;
    cbox_update();
    /* the background under the rect, remapped, then every prim again */
    if (!full) {
        p = emit(DC2D_SOLID);
        if (p) {
            p->color = doomo_colormap[level][bg_cur];
            p->x0 = X0; p->y0 = Y0; p->x1 = X1; p->y1 = Y1;
        } else ok = 0;
    }
    if (ok && use_held) ok = reemit_list(held_buf, nheld, hbox, 1, X0, Y0, X1, Y1, level);
    if (ok) ok = reemit_list(cur, n0, cbox, 0, X0, Y0, X1, Y1, level);
    if (ok && (full ? ncur - n0 : ncur) <= SHADE_MAX_PRIMS) {
        dc2d_stats.shades++;
        if (full) {                     /* the copy hides the originals: keep only it */
            memmove(cur, cur + n0, (size_t)(ncur - n0) * sizeof(DcPrim));
            ncur -= n0;
            cbox_n = 0;
            ngroups = group_wr = 0;
            cur_cleared = 1;
            bg_cur = doomo_colormap[level][bg_cur];
            m3d_rescan();               /* the copy holds the held backdrop's views too */
        } else {
            Group *g = &groups[group_wr];
            g->x0 = X0; g->y0 = Y0; g->x1 = X1; g->y1 = Y1;
            g->start = n0;              /* its opaque remapped background */
            group_wr = (group_wr + 1) % NGROUPS;
            if (ngroups < NGROUPS) ngroups++;
        }
        seal = ncur;
        return;
    }
    /* too many prims: the fitted multiply instead */
fallback:
    ncur = n0;
    if (cbox_n > ncur) cbox_n = ncur;
    dc2d_stats.shade_fallbacks++;
    p = emit(DC2D_MUL);
    if (!p) return;
    set_xy(p, x, y, w, h);
    p->argb = dc2d_shade_argb(level);
    seal = ncur;
}

/* plat_line: the native Bresenham walk, gathered into straight runs */
typedef struct Run { int x0, y0, x1, y1, on; } Run;

static void run_flush(Run *r, uint8_t c)
{
    if (r->on) solid(r->x0, r->y0, r->x1 - r->x0 + 1, r->y1 - r->y0 + 1, c);
    r->on = 0;
}

static void run_add(Run *r, int x, int y, uint8_t c)
{
    if (r->on) {
        if (r->y0 == y && r->y1 == y) {
            if (x == r->x1 + 1) { r->x1 = x; return; }
            if (x == r->x0 - 1) { r->x0 = x; return; }
        }
        if (r->x0 == x && r->x1 == x) {
            if (y == r->y1 + 1) { r->y1 = y; return; }
            if (y == r->y0 - 1) { r->y0 = y; return; }
        }
        run_flush(r, c);
    }
    r->x0 = r->x1 = x;
    r->y0 = r->y1 = y;
    r->on = 1;
}

void DC2D_FN(line)(int x0, int y0, int x1, int y1, uint8_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, n = 0;
    Run r;
    if (!drawing) return;
    r.on = 0;
    for (;;) {
        run_add(&r, x0, y0, color);
        if ((x0 == x1 && y0 == y1) || ++n > 4096) break;
        {
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
    run_flush(&r, color);
}

void DC2D_FN(checker)(int x, int y, int w, int h, uint8_t color)
{
    int ox = x, oy = y, slot = dc2d_checker_tex();
    DcPrim *p;
    if (!drawing || slot < 0 || !clip(&x, &y, &w, &h)) return;
    p = emit(DC2D_TINT);
    if (!p) return;
    p->tex = (uint16_t)slot;
    p->color = color;
    set_xy(p, x, y, w, h);
    /* one texel per logical pixel, the pattern's phase at the original corner */
    p->u0 = (float)(x - ox) / 8.0f;
    p->v0 = (float)(y - oy) / 8.0f;
    p->u1 = p->u0 + (float)w / 8.0f;
    p->v1 = p->v0 + (float)h / 8.0f;
}

/* one pass of glyphs (outline: the outline page, cells at -1,-1) */
static void glyph_pass(int x, int y, const char *s, uint8_t color, int outline)
{
    int slot = outline ? dc2d_font_outline_tex() : dc2d_font_tex();
    int cw = outline ? DC2D_OUTL_CW : DC2D_GLYPH_CW, chh = outline ? DC2D_OUTL_CH : DC2D_GLYPH_CH;
    int qw = outline ? 7 : 5, qh = outline ? 11 : 8, o = outline ? 1 : 0;
    int cx = x;
    const DcTex *t;
    if (slot < 0) return;
    t = dc2d_tex(slot);
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        if (ch == '\n') { cx = x; y += FONT_LINE; continue; }
        if (!dc2d_font_blank(ch) && !offscreen(cx - o, y - o, qw, qh)) {
            DcPrim *p = emit(DC2D_TINT);
            if (!p) return;
            p->tex = (uint16_t)slot;
            p->color = color;
            set_xy(p, cx - o, y - o, qw, qh);
            p->u0 = (float)((int)(ch & 15) * cw) / (float)t->tw;
            p->v0 = (float)((int)(ch >> 4) * chh) / (float)t->th;
            p->u1 = p->u0 + (float)qw / (float)t->tw;
            p->v1 = p->v0 + (float)qh / (float)t->th;
        }
        cx += FONT_ADVANCE;
    }
}

void DC2D_FN(text)(int x, int y, const char *s, uint8_t color)
{
    if (!s) return;
    dc2d_be_text(s);                    /* the harness's expect-text */
    if (!drawing) return;
    glyph_pass(x, y, s, color, 0);
}

void DC2D_FN(text_outline)(int x, int y, const char *s, uint8_t color, uint8_t outline)
{
    if (!s) return;
    dc2d_be_text(s);
    if (!drawing) return;
    glyph_pass(x, y, s, outline, 1);
    glyph_pass(x, y, s, color, 0);
}

/* IMG_SHADOW exactly: the native runner darkens, through colormap level 5,
 * every pixel one scaled pixel down-right of an opaque image pixel. The
 * image's opaque runs are merged into rects (equal runs of consecutive
 * rows join) and each rect is an exact shade. 0 (nothing drawn) when the
 * shape needs more than SHADOW_MAX_RECTS rects: then the DARK copy. */
#define SHADOW_MAX_RECTS 48
static int exact_shadow(int x, int y, const DoomoImage *img, int scale, int flip)
{
    struct { int16_t x0, x1, y0, y1; } r[SHADOW_MAX_RECTS];
    int nr = 0, iy, ix, k, w = img->w, h = img->h;
    if (ncur >= SHADOW_EXACT_MAX_PRIMS) return 0;
    for (iy = 0; iy < h; iy++) {
        const uint8_t *row = img->pix + (size_t)iy * (size_t)w;
        ix = 0;
        while (ix < w) {
            int a, b;
            while (ix < w && !row[flip ? w - 1 - ix : ix]) ix++;
            if (ix >= w) break;
            a = ix;
            while (ix < w && row[flip ? w - 1 - ix : ix]) ix++;
            b = ix;
            for (k = 0; k < nr; k++)
                if (r[k].y1 == iy && r[k].x0 == a && r[k].x1 == b) { r[k].y1 = (int16_t)(iy + 1); break; }
            if (k == nr) {
                if (nr == SHADOW_MAX_RECTS) return 0;
                r[nr].x0 = (int16_t)a; r[nr].x1 = (int16_t)b;
                r[nr].y0 = (int16_t)iy; r[nr].y1 = (int16_t)(iy + 1);
                nr++;
            }
        }
    }
    for (k = 0; k < nr; k++)
        DC2D_FN(shade_rect)(x + scale + r[k].x0 * scale, y + scale + r[k].y0 * scale,
                            (r[k].x1 - r[k].x0) * scale, (r[k].y1 - r[k].y0) * scale, 5);
    return 1;
}

void DC2D_FN(image)(int x, int y, const DoomoImage *img, int scale, int flags, uint8_t color)
{
    int w, h, slot, sh, flip;
    DcPrim *p;
    if (!drawing || !img || !img->pix || !img->w || !img->h) return;
    scale = DOOMO_CLAMP(scale, 1, 4);
    w = img->w * scale;
    h = img->h * scale;
    sh = (flags & IMG_SHADOW) ? scale : 0;
    if (offscreen(x, y, w + sh, h + sh)) return;
    slot = dc2d_tex_image(img);
    if (slot < 0) return;
    flip = (flags & IMG_FLIPX) != 0;
    if ((flags & IMG_SHADOW) && !exact_shadow(x, y, img, scale, flip)) {
        p = textured(DC2D_DARK, slot, x + scale, y + scale, w, h, img->w, img->h, flip);
        if (p) p->argb = dc2d_shadow_argb();
    }
    if (flags & IMG_SILHOUETTE) {
        p = textured(DC2D_TINT, slot, x, y, w, h, img->w, img->h, flip);
        if (p) p->color = color;
    } else {
        textured(DC2D_IMAGE, slot, x, y, w, h, img->w, img->h, flip);
    }
}

void DC2D_FN(image_xform)(int ax, int ay, const DoomoImage *img, int sx_q8, int sy_q8,
                          int flags, uint8_t color)
{
    int asx, asy, s, dw, dh, flip, slot;
    DcPrim *p;
    if (!drawing || !img || !img->pix || !img->w || !img->h || sx_q8 == 0 || sy_q8 <= 0) return;
    flip = (flags & IMG_FLIPX) ? 1 : 0;
    if (sx_q8 < 0) flip ^= 1;
    asx = sx_q8 < 0 ? -sx_q8 : sx_q8;
    asy = sy_q8;
    s = (asx + asy + 256) / 512;
    if (s >= 1 && s <= 4 && asx == s * 256 && asy == s * 256) {
        DC2D_FN(image)(ax - (int)img->w * s / 2, ay - (int)img->h * s, img, s,
                       (flags & ~IMG_FLIPX) | (flip ? IMG_FLIPX : 0), color);
        return;
    }
    /* the same integer size as the weak default, as one quad */
    dw = ((int)img->w * asx + 128) >> 8;
    dh = ((int)img->h * asy + 128) >> 8;
    if (dw <= 0 || dh <= 0 || offscreen(ax - dw / 2, ay - dh, dw + 1, dh + 1)) return;
    slot = dc2d_tex_image(img);
    if (slot < 0) return;
    if (flags & IMG_SHADOW) {
        p = textured(DC2D_TINT, slot, ax - dw / 2 + 1, ay - dh + 1, dw, dh, img->w, img->h, flip);
        if (p) p->color = PAL_BLACK;
    }
    if (flags & IMG_SILHOUETTE) {
        p = textured(DC2D_TINT, slot, ax - dw / 2, ay - dh, dw, dh, img->w, img->h, flip);
        if (p) p->color = color;
    } else {
        textured(DC2D_IMAGE, slot, ax - dw / 2, ay - dh, dw, dh, img->w, img->h, flip);
    }
}

/* ---- DC8: prims in frame pixels (the house, room.c) ------------------------------------- */
static int foff(float x0, float y0, float x1, float y1)
{
    return x1 <= 0.0f || y1 <= 0.0f || x0 >= (float)DC2D_W || y0 >= (float)DC2D_H || x1 <= x0 || y1 <= y0;
}

DcPrim *dc2d_tex_rect(int mode, int slot, float x0, float y0, float x1, float y1,
                      int iw, int ih, int flip, uint32_t argb, uint8_t color)
{
    const DcTex *t;
    DcPrim *p;
    float u1;
    if (!drawing || slot < 0 || foff(x0, y0, x1, y1)) return NULL;
    t = dc2d_tex(slot);
    p = emit(mode);
    if (!p) return NULL;
    p->tex = (uint16_t)slot;
    p->x0 = x0; p->y0 = y0; p->x1 = x1; p->y1 = y1;
    u1 = (float)iw / (float)t->tw;
    p->u0 = flip ? u1 : 0.0f;
    p->u1 = flip ? 0.0f : u1;
    p->v0 = 0.0f;
    p->v1 = (float)ih / (float)t->th;
    p->argb = argb;
    p->color = color;
    return p;
}

int dc2d_prim_count(void) { return ncur; }

void dc2d_frect(float x0, float y0, float x1, float y1, uint8_t c)
{
    DcPrim *p;
    if (!drawing || foff(x0, y0, x1, y1)) return;
    p = emit(DC2D_SOLID);
    if (!p) return;
    p->color = c;
    p->x0 = x0; p->y0 = y0; p->x1 = x1; p->y1 = y1;
}

/* ---- post effects ---------------------------------------------------------------------- */
void DC2D_FN(post_offset)(int dx, int dy)
{
    float fx = (float)(dx * DC2D_SCALE), fy = (float)(dy * DC2D_SCALE);
    int i;
    if (!drawing || (!dx && !dy)) return;
    for (i = 0; i < ncur; i++) {
        cur[i].x0 += fx; cur[i].x1 += fx;
        cur[i].y0 += fy; cur[i].y1 += fy;
    }
    m3d_widen();
    seal = ncur;
    cbox_n = 0;
    ngroups = group_wr = 0;
}

void DC2D_FN(post_scale)(int sx_q8, int sy_q8, int cx, int cy)
{
    float sx = (float)sx_q8 / 256.0f, sy = (float)sy_q8 / 256.0f;
    float c_x = (float)(cx * DC2D_SCALE), c_y = (float)(cy * DC2D_SCALE);
    int i;
    if (!drawing || sx_q8 <= 0 || sy_q8 <= 0 || (sx_q8 == 256 && sy_q8 == 256)) return;
    for (i = 0; i < ncur; i++) {
        cur[i].x0 = c_x + (cur[i].x0 - c_x) * sx;
        cur[i].x1 = c_x + (cur[i].x1 - c_x) * sx;
        cur[i].y0 = c_y + (cur[i].y0 - c_y) * sy;
        cur[i].y1 = c_y + (cur[i].y1 - c_y) * sy;
    }
    m3d_widen();
    seal = ncur;
    cbox_n = 0;
    ngroups = group_wr = 0;
}

void DC2D_FN(post_blend)(uint8_t color, int level16)
{
    DcPrim *p;
    if (!drawing || level16 <= 0) return;
    if (level16 >= 16) { solid(0, 0, DC2D_LW, DC2D_LH, color); return; }
    p = emit(DC2D_ALPHA);
    if (!p) return;
    set_xy(p, 0, 0, DC2D_LW, DC2D_LH);
    p->argb = ((uint32_t)(level16 * 16) << 24) | (dc2d_pal_argb(0, color) & 0x00FFFFFFu);
}

/* black outside the iris: the k-th black run of every row in one sweep (so
 * runs of equal rows merge into one rect), then the rim pixels */
void DC2D_FN(post_iris)(int cx, int cy, int r, int heart, uint8_t rim)
{
    int y, k, pass;
    if (!drawing || r < 0) return;
    for (pass = 0; pass < 4; pass++) {
        for (y = 0; y < DC2D_LH; y++) {
            int sp[4], n = camfx_iris_row(y, cx, cy, r, heart, sp), x = 0, run = 0;
            for (k = 0; k < n; k++) {
                int l = DOOMO_CLAMP(sp[k * 2], 0, DC2D_LW), rr = DOOMO_CLAMP(sp[k * 2 + 1], 0, DC2D_LW);
                if (l > x) {
                    if (run == pass && pass < 3) solid(x, y, l - x, 1, PAL_BLACK);
                    run++;
                }
                if (pass == 3 && rim && rr > l) {
                    if (sp[k * 2] >= 0) solid(l, y, 1, 1, rim);
                    if (sp[k * 2 + 1] <= DC2D_LW) solid(rr - 1, y, 1, 1, rim);
                }
                if (rr > x) x = rr;
            }
            if (x < DC2D_LW && run == pass && pass < 3) solid(x, y, DC2D_LW - x, 1, PAL_BLACK);
        }
    }
}

void DC2D_FN(pal_lerp)(int pal_id, int level16)
{
    int i;
    if (!drawing || pal_id <= PAL_LERP_NONE || pal_id >= PAL_LERP_COUNT || level16 <= 0) return;
    dc2d_pal_lerp_build(lerp_bank, pal_id, level16);
    lerp_on = 1;
    for (i = 0; i < ncur; i++)
        if (cur[i].mode == DC2D_SOLID || cur[i].mode == DC2D_TINT || cur[i].mode == DC2D_IMAGE ||
            cur[i].mode == DC2D_VIEW3D || cur[i].mode == DC2D_GLASS)
            cur[i].flags |= DC2D_PF_LERP;
    seal = ncur;
}
