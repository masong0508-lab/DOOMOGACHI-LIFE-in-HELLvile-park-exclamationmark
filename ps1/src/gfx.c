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
#include "postfx.h"

typedef struct FrameBuf {
    DISPENV  disp;
    DRAWENV  draw;
    uint32_t up_head[1];          /* zero-length link node            */
    uint32_t head[2];             /* DR_TPAGE: dither on              */
    uint32_t flush[2];            /* GP0(01h) texture cache flush      */
    uint32_t ofs[2];              /* GP0(E5h) draw offset (S1 post-FX) */
    uint32_t ot[OT_VIEWS][OT_LEN];
    uint32_t envs[OT_VIEWS][2][4];/* clip in / clip out per view      */
    uint8_t  *prim;               /* its half of s_prim               */
} FrameBuf;

static FrameBuf  s_fb[2];
/* Both frames' display lists, back to back: together they are also the
 * save scratch (plat_save_scratch below). */
static uint8_t   s_prim[2][PRIM_BYTES] __attribute__((aligned(4)));
int       g_cur;
uint32_t  g_frame;
uint32_t  g_rgb[256];
uint16_t  g_pal15[256];

uint8_t  *g_prim_ptr, *g_prim_end;   /* prim allocator (inlined by r3d) */
#define s_ptr g_prim_ptr
#define s_end g_prim_end
uint32_t *g_chain_tail;           /* last node of the draw chain      */
#define s_tail g_chain_tail
static uint32_t *s_up_tail;       /* last node of the upload chain    */
static int       s_views;
static uint32_t *s_view_ot;
static int       s_view_idx;
static int       s_first = 1;
static int       s_open;          /* between gfx_begin and gfx_end     */
static uint32_t  s_prims;
static uint32_t  s_vb_last;
static int       s_pal_phase;
static uint32_t  s_t_begin;
GfxPerf          g_perf;
static uint32_t *s_ofs_follow;    /* H3: gfx_ofs_follow()              */

/* link helpers on raw tag words */
#define TAG_LINK(node, to) (*(node) = (*(node) & 0xff000000u) | ((uint32_t)(to) & 0x00ffffffu))

void gfx_fb_xy(int *x, int *y) { *x = 0; *y = g_cur ? SCR_H : 0; }

static void palette_tables(void)
{
    const uint16_t *pal = core_palette();
    int i;
    for (i = 0; i < 256; i++) {
        uint16_t c = pal[i];
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
        f->prim = s_prim[i];
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
    gfx2d_flush();
    TAG_LINK(s_tail, p);
    s_tail = p;
    s_prims++;
}

/* The texpage the draw chain last set (0xffff: unknown, e.g. after a 3D
 * view's own prims): a DR_TPAGE equal to it is redundant and skipped. The
 * core's text draws one per string, the checker seams one per row. */
uint16_t g_tpage_cur = 0xffff;

void gfx_tpage(uint16_t tpage)
{
    DR_TPAGE *t;
    gfx2d_flush();
    if (g_tpage_cur == tpage) return;
    t = (DR_TPAGE *)gfx_alloc(sizeof(DR_TPAGE));
    if (!t) return;
    setDrawTPage(t, 1, 1, tpage);
    gfx_chain(t);
    g_tpage_cur = tpage;
}

void gfx_begin(void)
{
    FrameBuf *f = &s_fb[g_cur];
    s_t_begin = timer_us();
    s_open = 1;
    s_ptr = f->prim;
    s_end = f->prim + PRIM_BYTES;
    s_views = 0;
    s_view_ot = NULL;
    s_prims = 0;
    s_ofs_follow = NULL;
    /* upload chain: up_head -> ... -> flush -> head -> draw chain */
    f->up_head[0] = 0x00ffffffu;        /* len 0 */
    s_up_tail = f->up_head;
    setDrawTPage((DR_TPAGE *)f->head, 1, 1, getTPage(0, 0, 0, 0));
    /* S1: head -> ofs (the frame's draw offset, which gfx_post_offset may
     * move) -> the draw chain */
    {
        int fx, fy;
        gfx_fb_xy(&fx, &fy);
        f->ofs[0] = 0x01ffffffu;        /* len 1, end */
        f->ofs[1] = 0xe5000000u | ((uint32_t)(fy & 0x7ff) << 11) | (uint32_t)(fx & 0x7ff);
        TAG_LINK(f->head, f->ofs);
    }
    s_tail = f->ofs;
    g_tpage_cur = getTPage(0, 0, 0, 0);
    gfx2d_reset();
    f->flush[0] = 0x01ffffffu;          /* len 1 */
    f->flush[1] = 0x01000000u;          /* GP0(01h) clear texture cache */
    postfx_frame_begin();
}

/* S1 post-FX (postfx.h): everything chained so far shakes by (dx, dy),
 * everything after this point (the HUD) is drawn at the plain offset. */
void gfx_post_offset(int dx, int dy)
{
    FrameBuf *f = &s_fb[g_cur];
    uint32_t *e;
    int fx, fy;
    if (!s_open) return;
    gfx_fb_xy(&fx, &fy);
    f->ofs[1] = 0xe5000000u | ((uint32_t)((fy + dy) & 0x7ff) << 11) | (uint32_t)((fx + dx) & 0x7ff);
    if (s_ofs_follow) *s_ofs_follow = f->ofs[1];        /* H3 */
    e = (uint32_t *)gfx_alloc(8);
    if (!e) return;
    e[0] = 0x01ffffffu;
    e[1] = 0xe5000000u | ((uint32_t)(fy & 0x7ff) << 11) | (uint32_t)(fx & 0x7ff);
    gfx_chain(e);
}

/* --- H3 (ps1.h) --- */
void gfx_ofs_follow(uint32_t *e5)
{
    s_ofs_follow = e5;
    if (e5) *e5 = s_fb[g_cur].ofs[1];
}
/* --- end H3 --- */

uint32_t *gfx_ot_begin(int vx, int vy, int vw, int vh)
{
    FrameBuf *f = &s_fb[g_cur];
    int fx, fy;
    uint32_t *ot;
    DR_AREA *a;
    gfx2d_flush();
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
    g_tpage_cur = 0xffff;                   /* the view's prims set their own */
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
    /* --- H4: the rows are staged in the other frame's display list (the GPU
     * is idle after the DrawSync, and the CPU fills only this frame's), not
     * in 4 KB of .bss of their own (only boot calls this) --- */
    uint32_t *tmp;
    RECT r;
    const uint8_t *src = (const uint8_t *)data;
    int rows_per = (4096 / 2) / w;
    DrawSync(0);
    tmp = (uint32_t *)s_prim[g_cur ^ 1];
    /* --- end H4 --- */
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

/* A small "Saving..." banner on the picture on screen while the BIOS
 * blocks the game loop (memory card I/O runs one sector per vblank, about
 * a second per 8 KB block), so a long save does not look like a hang.
 * Drawn straight into the newest finished frame, which is then shown;
 * gfx_busy_end() puts the pixels under it back (so that frame stays what
 * the game drew, e.g. for harness screenshots). */
#define BANNER_MAX 40
#define BANNER_W (BANNER_MAX * 6 + 12)
#define BANNER_H 16
static uint16_t s_banner_save[BANNER_W * BANNER_H];
static RECT s_banner_rect;

void gfx_busy_banner(const char *msg)
{
    static TILE box[2];
    static SPRT glyph[BANNER_MAX];
    static DR_TPAGE tp;
    DRAWENV env;
    int fb = g_cur ^ 1, n = 0, x, y, w, i;
    uint16_t clut = vram_font_clut(PAL_UI_CREAM);

    DrawSync(0);                        /* newest frame complete */
    g_tpage_cur = 0xffff;               /* immediate draws below: state unknown */
    while (msg[n] && n < BANNER_MAX)
        n++;
    w = n * FONT_ADVANCE + 10;
    x = SCR_W - w - 10;
    y = SCR_H - 26;
    setRECT(&s_banner_rect, x - 1, fb * SCR_H + y - 1, w + 2, BANNER_H);
    StoreImage(&s_banner_rect, (uint32_t *)s_banner_save);
    DrawSync(0);
    env = s_fb[fb].draw;
    env.isbg = 0;
    env.dtd = 0;
    PutDrawEnv(&env);
    setTile(&box[0]);
    setXY0(&box[0], x - 1, y - 1);
    setWH(&box[0], w + 2, BANNER_H);
    setRGB0(&box[0], 255, 168, 208);
    setTile(&box[1]);
    setXY0(&box[1], x, y);
    setWH(&box[1], w, BANNER_H - 2);
    setRGB0(&box[1], 56, 28, 72);
    DrawPrim(&box[0]);
    DrawPrim(&box[1]);
    setDrawTPage(&tp, 0, 0, vram_font_tpage());
    DrawPrim(&tp);
    for (i = 0; i < n; i++) {
        unsigned c = (uint8_t)msg[i];
        SPRT *p = &glyph[i];
        if (c <= ' ' || c > FONT_LAST)
            continue;
        c -= FONT_FIRST;
        setSprt(p);
        setShadeTex(p, 1);
        setXY0(p, x + 5 + i * FONT_ADVANCE, y + 3);
        setUV0(p, FONT_U(c), FONT_V(c));
        setWH(p, 5, 8);
        p->clut = clut;
        DrawPrim(p);
    }
    DrawSync(0);
    PutDispEnv(&s_fb[fb].disp);
}

void gfx_busy_end(void)
{
    if (!s_banner_rect.w)
        return;
    LoadImage(&s_banner_rect, (const uint32_t *)s_banner_save);
    DrawSync(0);
    s_banner_rect.w = 0;
}

/* ---- full-screen loading screen --------------------------------------------
 * Used for saving and loading the memory card. The BIOS card driver blocks
 * the game loop for about a second per 8 KB block, so the screen cannot
 * animate by itself: memcard.c calls gfx_loading_screen() again after every
 * block, which moves the progress bar. It is drawn straight into the picture
 * that is on screen (like the small banner above). Nothing has to be put
 * back afterwards: every game frame is cleared (isbg) and redrawn in full. */
#define LS_TEXT_MAX 64

static void busy_box(TILE *t, int x, int y, int w, int h, int r, int g, int b)
{
    setTile(t);
    setXY0(t, x, y);
    setWH(t, w, h);
    setRGB0(t, r, g, b);
    DrawPrim(t);
}

static void busy_puts(int y, const char *msg, uint8_t pal)
{
    static SPRT glyph[LS_TEXT_MAX];
    static DR_TPAGE tp;
    uint16_t clut = vram_font_clut(pal);
    int n = 0, x, i;

    while (msg[n] && n < LS_TEXT_MAX)
        n++;
    x = (SCR_W - n * FONT_ADVANCE) / 2;
    DrawSync(0);                        /* the shared prims are free again */
    setDrawTPage(&tp, 0, 0, vram_font_tpage());
    DrawPrim(&tp);
    g_tpage_cur = 0xffff;               /* drawn outside the chain: state unknown */
    for (i = 0; i < n; i++) {
        unsigned c = (uint8_t)msg[i];
        SPRT *p = &glyph[i];
        if (c <= ' ' || c > FONT_LAST)
            continue;
        c -= FONT_FIRST;
        setSprt(p);
        setShadeTex(p, 1);
        setXY0(p, x + i * FONT_ADVANCE, y);
        setUV0(p, FONT_U(c), FONT_V(c));
        setWH(p, 5, 8);
        p->clut = clut;
        DrawPrim(p);
    }
    DrawSync(0);
}

void gfx_loading_screen(const char *title, const char *detail, int done, int total)
{
    static TILE tl[8];
    DRAWENV env;
    const int bx = 60, by = 128, bw = 200, bh = 14;
    int fb = g_cur ^ 1, fill = 0;

    DrawSync(0);                        /* newest frame complete */
    env = s_fb[fb].draw;
    env.isbg = 0;
    env.dtd = 0;
    PutDrawEnv(&env);
    busy_box(&tl[0], 0, 0, SCR_W, SCR_H, 255, 214, 232);        /* pink    */
    busy_box(&tl[1], 0, 58, SCR_W, 2, 255, 150, 200);           /* stripes */
    busy_box(&tl[2], 0, 180, SCR_W, 2, 255, 150, 200);
    busy_box(&tl[3], bx - 2, by - 2, bw + 4, bh + 4, 110, 40, 90);  /* bar  */
    busy_box(&tl[4], bx, by, bw, bh, 255, 244, 250);
    if (total > 0) {
        if (done < 0) done = 0;
        if (done > total) done = total;
        fill = bw * done / total;
    }
    if (fill > 0)
        busy_box(&tl[5], bx, by, fill, bh, 255, 90, 160);
    DrawSync(0);
    busy_puts(70, "DOOMOGACHI LIFE", PAL_UI_PURPLE);
    busy_puts(82, "in HELLvile Park!", PAL_UI_PINK_D);
    busy_puts(104, title, PAL_UI_PURPLE);
    busy_puts(152, detail, PAL_UI_PURPLE);
    busy_puts(194, "Do not remove the MEMORY CARD", PAL_UI_RED_D);
    busy_puts(206, "or switch the power off!", PAL_UI_RED_D);
    PutDispEnv(&s_fb[fb].disp);
}

/* platform.h: the save/load scratch buffer is the display-list memory of
 * BOTH frames (s_prim, contiguous). Once the GPU has finished the previous
 * frame (DrawSync), its half is unused until the current frame is
 * presented; the current half is empty while the core runs its update(),
 * which is where saves and loads happen (one call, inside one frame). If
 * something was drawn already, that much is drawn now (into the back
 * buffer, like gfx_end would, so queued VRAM uploads are not lost) and the
 * list starts over. This replaces the core's 88 KB static default
 * (dropped by --gc-sections). */
typedef char scratch_fits[2 * PRIM_BYTES >= DOOMO_SAVE_SCRATCH_MIN ? 1 : -1];

void *plat_save_scratch(uint32_t *cap)
{
    FrameBuf *f = &s_fb[g_cur];
    DrawSync(0);
    /* outside a frame (the harness's commands run before gfx_begin) both
     * halves are idle; inside one, only if nothing was drawn yet */
    if (s_open && (s_ptr != f->prim || s_up_tail != f->up_head || s_tail != f->head)) {
        gfx2d_flush();
        termPrim((DR_TPAGE *)s_tail);
        TAG_LINK(s_up_tail, f->flush);
        TAG_LINK(f->flush, f->head);
        DrawOTag(f->up_head);
        DrawSync(0);
        dbg("gfx: save scratch taken mid-frame, list drawn early\n");
        gfx_begin();
    }
    if (cap)
        *cap = 2 * PRIM_BYTES;
    return s_prim[0];
}

/* framebuffer row the newest frame is being drawn into (harness shots) */
uint16_t g_last_draw_y;

void gfx_end(void)
{
    FrameBuf *f = &s_fb[g_cur];
    uint32_t vb;
    gfx2d_flush();
    termPrim((DR_TPAGE *)s_tail);
    /* uploads -> flush -> draw chain */
    TAG_LINK(s_up_tail, f->flush);
    TAG_LINK(f->flush, f->head);

    {
        uint32_t t1 = timer_us();
        g_perf.cpu_us = t1 - s_t_begin;
        DrawSync(0);            /* previous frame finished drawing */
        g_perf.sync_us = timer_us() - t1;
        g_perf.prim_bytes = (uint32_t)(s_ptr - f->prim);
        g_perf.prims = s_prims;
    }
    /* 30 Hz: two vblanks per logic tick (PAL: 2,2,1 pattern = 30/s) */
#ifdef DOOMO_PAL
    VSync((s_pal_phase++ % 3) == 2 ? 1 : 2);
#else
    VSync(2);
#endif
    (void)s_pal_phase;
    vb = (uint32_t)VSync(-1);
    g_perf.frame_vblanks = vb - s_vb_last;
    s_vb_last = vb;
    /* frame rate over the last ~second */
    {
        static uint32_t win_vb, win_frames;
        win_frames++;
        if (vb - win_vb >= 60) {
            g_perf.fps_x10 = win_frames * 600 / (vb - win_vb);
            win_vb = vb;
            win_frames = 0;
        }
    }
    /* show what the GPU just finished, draw the new list into the other */
    PutDispEnv(&s_fb[g_cur ^ 1].disp);
    DrawOTagEnv(f->up_head, &f->draw);
    g_last_draw_y = (uint16_t)f->draw.clip.y;
    if (s_first) {
        s_first = 0;
        SetDispMask(1);
    }
    g_cur ^= 1;
    g_frame++;
    s_open = 0;
}
