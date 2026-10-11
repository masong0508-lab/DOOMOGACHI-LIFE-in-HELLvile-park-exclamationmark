/* gfx.c - PS1 display, double buffering, display lists, VRAM uploads.
 *
 * Every frame is ONE linked GPU display list, built in painter's order:
 *
 *   [uploads] -> [2D prims ...] -> [view clip][3D OT 1535..0][full clip]
 *             -> [2D prims ...] -> ...
 *
 * 2D primitives are appended to the chain as they are issued. A 3D view
 * gets its own reverse-cleared ordering table which is spliced into the
 * chain at the point plat_view3d()/plat_overworld() was called, so the core
 * can freely interleave 2D and 3D. VRAM uploads (texture cache misses) are
 * GP0(A0h) packets carrying a copy of the pixels; they live in their own
 * chain that runs before anything else in the frame, i.e. strictly after
 * the previous frame finished drawing, so the cache never overwrites a
 * texture that is still in use.
 *
 * Buffers: while the GPU draws frame N into framebuffer N&1, the CPU builds
 * frame N+1 in the other prim buffer. gfx_end() waits for the GPU, waits
 * for the 30 Hz vsync slot, shows the finished frame and kicks off the new
 * list.
 */
#include <string.h>
#include <psxgpu.h>
#include <psxapi.h>
#include <psxetc.h>
#include "ps1.h"

typedef struct FrameBuf {
    DISPENV  disp;
    DRAWENV  draw;
    uint32_t up_head[1];          /* zero-length link node            */
    uint32_t head[2];             /* DR_TPAGE: dither on              */
    uint32_t flush[2];            /* GP0(01h) texture cache flush      */
    uint32_t ot[OT_VIEWS][OT_LEN];
    uint32_t envs[OT_VIEWS][2][4];/* clip in / clip out per view      */
    uint8_t  prim[PRIM_BYTES] __attribute__((aligned(4)));
} FrameBuf;

static FrameBuf  s_fb[2];
int       g_cur;
uint32_t  g_frame;
uint32_t  g_rgb[256];
uint16_t  g_pal15[256];

static uint8_t  *s_ptr, *s_end;
static uint32_t *s_tail;          /* last node of the draw chain      */
static uint32_t *s_up_tail;       /* last node of the upload chain    */
static int       s_views;
static uint32_t *s_view_ot;
static int       s_view_idx;
static int       s_first = 1;
static uint32_t  s_prims;
static uint32_t  s_vb_last;
static int       s_pal_phase;

/* link helpers on raw tag words */
#define TAG_LINK(node, to) (*(node) = (*(node) & 0xff000000u) | ((uint32_t)(to) & 0x00ffffffu))

void gfx_fb_xy(int *x, int *y) { *x = 0; *y = g_cur ? SCR_H : 0; }

static void palette_tables(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        uint16_t c = doomo_palette[i];
        int r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;
        r |= r >> 5; g |= g >> 5; b |= b >> 5;
        g_rgb[i] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
        /* VRAM colour: STP set on every opaque entry so translucent prims
         * blend all texels; pure black stays opaque (0x8000). Index 0 is
         * the transparent texel (0x0000). */
        g_pal15[i] = (uint16_t)(i == 0 ? 0 : ((c & 0x7fff) | 0x8000));
    }
}

void gfx_init(void)
{
    int i;
    ResetGraph(0);
#ifdef DOOMO_PAL
    SetVideoMode(MODE_PAL);
#else
    SetVideoMode(MODE_NTSC);
#endif
    for (i = 0; i < 2; i++) {
        FrameBuf *f = &s_fb[i];
        /* s_fb[i] draws into framebuffer i; it is displayed while the
         * next frame is being drawn into the other one */
        SetDefDispEnv(&f->disp, 0, i * SCR_H, SCR_W, SCR_H);
        SetDefDrawEnv(&f->draw, 0, i * SCR_H, SCR_W, SCR_H);
#ifdef DOOMO_PAL
        f->disp.screen.y = 8; /* centre 240 lines in the 256-line PAL frame */
        f->disp.screen.h = SCR_H;
#endif
        f->draw.dtd  = 1;     /* dithering on */
        f->draw.isbg = 1;     /* clear to black */
        setRGB0(&f->draw, 0, 0, 0);
    }
    palette_tables();
    g_cur = 0;
    s_vb_last = (uint32_t)VSync(-1);
    gfx_begin();
}

void *gfx_alloc(size_t bytes)
{
    uint8_t *p;
    bytes = (bytes + 3) & ~(size_t)3;
    if ((size_t)(s_end - s_ptr) < bytes)
        return NULL;
    p = s_ptr;
    s_ptr += bytes;
    return p;
}

size_t gfx_free_bytes(void) { return (size_t)(s_end - s_ptr); }
uint32_t gfx_prims_used(void) { return s_prims; }

void gfx_chain(void *prim)
{
    uint32_t *p = (uint32_t *)prim;
    TAG_LINK(s_tail, p);
    s_tail = p;
    s_prims++;
}

void gfx_tpage(uint16_t tpage)
{
    DR_TPAGE *t = (DR_TPAGE *)gfx_alloc(sizeof(DR_TPAGE));
    if (!t) return;
    setDrawTPage(t, 1, 1, tpage);
    gfx_chain(t);
}

void gfx_begin(void)
{
    FrameBuf *f = &s_fb[g_cur];
    s_ptr = f->prim;
    s_end = f->prim + PRIM_BYTES;
    s_views = 0;
    s_view_ot = NULL;
    s_prims = 0;
    /* upload chain: up_head -> ... -> flush -> head -> draw chain */
    f->up_head[0] = 0x00ffffffu;        /* len 0 */
    s_up_tail = f->up_head;
    setDrawTPage((DR_TPAGE *)f->head, 1, 1, getTPage(0, 0, 0, 0));
    termPrim((DR_TPAGE *)f->head);
    s_tail = f->head;
    f->flush[0] = 0x01ffffffu;          /* len 1 */
    f->flush[1] = 0x01000000u;          /* GP0(01h) clear texture cache */
}

uint32_t *gfx_ot_begin(int vx, int vy, int vw, int vh)
{
    FrameBuf *f = &s_fb[g_cur];
    int fx, fy;
    uint32_t *ot;
    DR_AREA *a;
    if (s_views >= OT_VIEWS || s_view_ot)
        return NULL;
    s_view_idx = s_views++;
    ot = f->ot[s_view_idx];
    ClearOTagR(ot, OT_LEN);
    gfx_fb_xy(&fx, &fy);
    if (vx < 0) { vw += vx; vx = 0; }
    if (vy < 0) { vh += vy; vy = 0; }
    if (vx + vw > SCR_W) vw = SCR_W - vx;
    if (vy + vh > SCR_H) vh = SCR_H - vy;
    if (vw < 1) vw = 1;
    if (vh < 1) vh = 1;
    a = (DR_AREA *)f->envs[s_view_idx][0];
    setDrawAreaXY(a, fx + vx, fy + vy, fx + vx + vw - 1, fy + vy + vh - 1);
    gfx_chain(a);
    /* splice the OT: chain -> ot[LEN-1] ... ot[0] */
    TAG_LINK(s_tail, &ot[OT_LEN - 1]);
    s_tail = &ot[0];
    s_view_ot = ot;
    return ot;
}

void gfx_ot_end(void)
{
    FrameBuf *f = &s_fb[g_cur];
    int fx, fy;
    DR_AREA *a;
    if (!s_view_ot)
        return;
    gfx_fb_xy(&fx, &fy);
    a = (DR_AREA *)f->envs[s_view_idx][1];
    setDrawAreaXY(a, fx, fy, fx + SCR_W - 1, fy + SCR_H - 1);
    gfx_chain(a);
    s_view_ot = NULL;
}

static int upload_rows(int x, int y, int w, int h, const uint8_t *src, int stride)
{
    /* GP0(A0h) packets of at most 252 data words */
    int rows_per = (252 * 2) / w;
    if (rows_per < 1)
        return 0;
    while (h > 0) {
        int rows = h < rows_per ? h : rows_per;
        int halfs = w * rows;
        int words = (halfs + 1) >> 1;
        uint32_t *p = (uint32_t *)gfx_alloc((size_t)(4 + words) * 4);
        uint16_t *d;
        int r;
        if (!p)
            return 0;
        p[0] = ((uint32_t)(3 + words) << 24) | 0x00ffffffu;
        p[1] = 0xa0000000u;
        p[2] = ((uint32_t)y << 16) | (uint32_t)x;
        p[3] = ((uint32_t)rows << 16) | (uint32_t)w;
        d = (uint16_t *)&p[4];
        for (r = 0; r < rows; r++) {
            memcpy(d, src, (size_t)w * 2);
            d += w;
            src += stride;
        }
        if (halfs & 1)
            *d = 0;
        TAG_LINK(s_up_tail, p);
        s_up_tail = p;
        y += rows;
        h -= rows;
    }
    return 1;
}

int gfx_upload(int x, int y, int w, int h, const uint16_t *data)
{
    return upload_rows(x, y, w, h, (const uint8_t *)data, w * 2);
}

/* strided variant used by vram.c (rows of `stride` bytes) */
int gfx_upload_strided(int x, int y, int w, int h, const uint8_t *src, int stride)
{
    return upload_rows(x, y, w, h, src, stride);
}

void gfx_upload_now(int x, int y, int w, int h, const void *data)
{
    /* LoadImage needs a word-aligned source and an even pixel count
     * (every boot-time upload has an even width) */
    static uint32_t tmp[4096];
    RECT r;
    const uint8_t *src = (const uint8_t *)data;
    int rows_per = (int)(sizeof(tmp) / 2) / w;
    while (h > 0) {
        int rows = h < rows_per ? h : rows_per;
        int n = w * rows;
        memcpy(tmp, src, (size_t)n * 2);
        setRECT(&r, x, y, w, rows);
        DrawSync(0);
        LoadImage(&r, tmp);
        DrawSync(0);
        src += n * 2;
        y += rows;
        h -= rows;
    }
}

void gfx_end(void)
{
    FrameBuf *f = &s_fb[g_cur];
    uint32_t vb;
    termPrim((DR_TPAGE *)s_tail);
    /* uploads -> flush -> draw chain */
    TAG_LINK(s_up_tail, f->flush);
    TAG_LINK(f->flush, f->head);

    DrawSync(0);                /* previous frame finished drawing */
    /* 30 Hz: two vblanks per logic tick (PAL: 2,2,1 pattern = 30/s) */
#ifdef DOOMO_PAL
    VSync((s_pal_phase++ % 3) == 2 ? 1 : 2);
#else
    VSync(2);
#endif
    (void)s_pal_phase;
    vb = (uint32_t)VSync(-1);
    s_vb_last = vb;
    /* show what the GPU just finished, draw the new list into the other */
    PutDispEnv(&s_fb[g_cur ^ 1].disp);
    DrawOTagEnv(f->up_head, &f->draw);
    if (s_first) {
        s_first = 0;
        SetDispMask(1);
    }
    g_cur ^= 1;
    g_frame++;
}
