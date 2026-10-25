/* show_fx.c - irises, the camfx post pass without its screen offset, the
 * crayon boil, the brick wipe and the Split, the plate dissolve, whip lines
 * and the matte (show_fx.h). Owner: ENGINE lane 2.
 *
 * Everything is a function of the shot frame and the shot's records, except
 * the camfx state, which beat.c drives from the shot's events (and which a
 * seek re-creates by pre-rolling from frame 0).
 */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/camfx.h"
#include "doomo/beat.h"
#include "show/show_l2.h"
#include "show/show_cam.h"
#include "show/show_fx.h"

#define MAX_WIPES 4

static struct {
    ShowFxSetup set;
    ShowWipe    wipe[MAX_WIPES];
    int         nwipe;
    int         t;                   /* the frame being drawn                */
    int         boil_every;          /* the look's period (SOP_BOIL changes it) */
    uint8_t     boil_off;            /* `boil off` from now on                  */
    uint8_t     clipped;
    int16_t     iris_cx, iris_cy;    /* every iris goes through iris_at()    */
    uint8_t     iris_heart;
} s_fx;

void show_fx_reset(void)
{
    memset(&s_fx, 0, sizeof(s_fx));
}

void show_fx_bind(const ShowFxSetup *fx, const ShowWipe *wipes)
{
    show_fx_reset();
    if (fx) s_fx.set = *fx;
    s_fx.boil_every = s_fx.set.boil_every;
    s_fx.nwipe = DOOMO_MIN(s_fx.set.nwipes, MAX_WIPES);
    if (wipes && s_fx.nwipe) memcpy(s_fx.wipe, wipes, sizeof(ShowWipe) * (size_t)s_fx.nwipe);
    /* frame-0 state: a fade-in starts black, a look's lerp is on from frame 0 */
    if (s_fx.set.fade0) camfx_fade(s_fx.set.fade0);
    if (s_fx.set.pal_id && s_fx.set.pal_level)
        camfx_pal_lerp(s_fx.set.pal_id, s_fx.set.pal_level);
}

void show_fx_frame_begin(int t)
{
    int lvl, id;
    s_fx.t = t;
    /* the palette first: the PS1 builds flat prim colours from the live
     * palette when they are emitted, so a lerp set late in the frame would
     * leave this frame's rects and lines in the old colours */
    id = camfx_pal_state(&lvl);
    if (id != PAL_LERP_NONE && !show_preroll()) plat_pal_lerp(id, lvl);
}

/* ---- irises -------------------------------------------------------------- */
static void iris_at(int mode, int x, int y, int r, int heart, int hold)
{
    int sy = y + SHOW_CANVAS_Y;                    /* camfx works in screen px */
    s_fx.iris_cx = (int16_t)x;
    s_fx.iris_cy = (int16_t)sy;
    s_fx.iris_heart = heart ? 1 : 0;
    switch (mode) {
    case 0: camfx_iris_in(x, sy, r, heart, hold); break;
    case 1: camfx_iris_out(x, sy, r, heart, hold); break;
    case 2: camfx_iris(x, sy, r, heart, 0); break;
    default: camfx_iris_off(); break;
    }
}

int show_fx_beat_ext(const struct BeatEv *ev)
{
    int x = SHOW_CANVAS_W / 2, y = SHOW_CANVAS_H / 2, b;
    if (!ev || (ev->op != BOP_IRIS_IN && ev->op != BOP_IRIS_OUT)) return 0;
    if (!show_stage_slot_xy(ev->a, &x, &y)) {
        x = SHOW_CANVAS_W / 2;
        y = SHOW_CANVAS_H / 2;
    }
    b = (uint16_t)ev->b;
    iris_at(ev->op == BOP_IRIS_IN ? 0 : 1, x, y, ((b >> 7) & 0x1ff) * 2, (b >> 6) & 1, b & 63);
    return 1;
}

int show_fx_op(int op, const int16_t *a, int n)
{
    switch (op) {
    case SOP_IRIS: {            /* slot(-1 = point) x y r heart mode hold */
        int x, y;
        if (n < 7) return 1;
        x = a[1]; y = a[2];
        if (a[0] >= 0 && !show_stage_slot_xy(a[0], &x, &y)) { x = a[1]; y = a[2]; }
        iris_at(a[5], x, y, a[3], a[4], a[6]);
        return 1;
    }
    case SOP_BOIL:
        if (n > 0 && a[0] > 0) { s_fx.boil_every = DOOMO_CLAMP(a[0], 1, 30); s_fx.boil_off = 0; }
        else s_fx.boil_off = 1;
        return 1;
    case SOP_FADE_SET:
        camfx_fade(n > 0 ? DOOMO_CLAMP(a[0], 0, 16) : 0);
        return 1;
    default:
        return 0;
    }
}

/* ---- boil -------------------------------------------------------------------- */
int show_fx_boil_every(int own)
{
    if (s_fx.boil_off) return 0;
    return own > 0 ? own : s_fx.boil_every;
}

int show_fx_boil(uint32_t id, int i, int every, int *dx, int *dy)
{
    uint32_t h;
    static const int8_t J[4] = { -1, 0, 0, 1 };      /* half the points move */
    *dx = *dy = 0;
    if (every <= 0) return 0;
    h = show_hash3(s_fx.set.seed ^ id, (uint32_t)i, (uint32_t)(s_fx.t / every));
    *dx = J[h & 3];
    *dy = J[(h >> 2) & 3];
    return 1;
}

/* ---- wipes ------------------------------------------------------------------- */
/* courses of side B shown at t (0..courses), and the frame the newest landed */
static int wipe_courses(const ShowWipe *w, int t, int *age)
{
    int n = w->courses ? w->courses : 5, st = w->step ? w->step : 2, k, in_len = n * st;
    int a = t - w->t0;
    *age = 99;
    if (a < 0) return 0;
    if (a < in_len) { k = a / st + 1; *age = a % st; return k; }
    a -= in_len;
    if (w->hold == WIPE_HOLD_END || a < w->hold || !w->out) return n;
    a -= w->hold;
    if (a < in_len) return n - a / st;            /* out: one course every step */
    return 0;
}

static const ShowWipe *wipe_now(int t, int *k, int *age)
{
    int i;
    for (i = s_fx.nwipe - 1; i >= 0; i--) {
        const ShowWipe *w = &s_fx.wipe[i];
        if (t >= w->t0) {
            *k = wipe_courses(w, t, age);
            return w;
        }
    }
    *k = 0;
    return 0;
}

/* side B's rectangle for k courses */
static int wipe_rect(const ShowWipe *w, int k, ShowClip *c)
{
    int n = w->courses ? w->courses : 5;
    if (k <= 0) return 0;
    if (w->kind == WIPE_STACK) {
        int line = w->line ? w->line : SHOW_WALL_X, ch = (SHOW_CANVAS_H + n - 1) / n;
        int y1 = DOOMO_MIN(SHOW_CANVAS_H, k * ch);
        c->y = 0;
        c->h = (int16_t)y1;
        if (k >= n) { c->y = -SHOW_CANVAS_Y; c->h = 240; }   /* landed: open matte too */
        if (w->side == WIPE_B_LEFT) { c->x = 0; c->w = (int16_t)line; }
        else { c->x = (int16_t)line; c->w = (int16_t)(SHOW_CANVAS_W - line); }
    } else {
        int stop = w->line ? w->line : SHOW_CANVAS_W;    /* how far the edge travels */
        int e = stop * k / n;
        c->y = -SHOW_CANVAS_Y;
        c->h = 240;
        if (w->side == WIPE_B_LEFT) { c->x = 0; c->w = (int16_t)e; }   /* edge L -> R */
        else { c->x = (int16_t)(SHOW_CANVAS_W - e); c->w = (int16_t)e; }  /* edge R -> L */
    }
    return c->w > 0 && c->h > 0;
}

int show_fx_split_rect(int t, ShowClip *out)
{
    int k, age;
    const ShowWipe *w = wipe_now(t, &k, &age);
    if (!w) return 0;
    return wipe_rect(w, k, out);
}

#if defined(DOOMO_PLATFORM_PS1) && !defined(SHOW_L2_HOST)
#include <psxgpu.h>
#include "ps1.h"
static void clip_set(int x, int y, int w, int h)
{
    DR_AREA *a;
    int fx, fy;
    gfx2d_flush();
    a = (DR_AREA *)gfx_alloc(sizeof(DR_AREA));
    if (!a) return;
    gfx_fb_xy(&fx, &fy);
    if (w <= 0 || h <= 0) { w = 1; h = 1; x = SCR_W; y = SCR_H; }   /* nothing visible */
    setDrawAreaXY(a, fx + x, fy + y, fx + x + w - 1, fy + y + h - 1);
    gfx_chain(a);
}
#else
void show_l2_host_clip(int x, int y, int w, int h);     /* the test driver's raster */
static void clip_set(int x, int y, int w, int h) { show_l2_host_clip(x, y, w, h); }
#endif

void show_fx_clip(const ShowClip *c)
{
    int x0 = DOOMO_MAX(0, c->x), y0 = DOOMO_MAX(0, c->y + SHOW_CANVAS_Y);
    int x1 = DOOMO_MIN(320, c->x + c->w), y1 = DOOMO_MIN(240, c->y + SHOW_CANVAS_Y + c->h);
    clip_set(x0, y0, x1 - x0, y1 - y0);
    s_fx.clipped = 1;
}

void show_fx_clip_off(void)
{
    if (!s_fx.clipped) return;
    clip_set(0, 0, 320, 240);
    s_fx.clipped = 0;
}

int show_fx_split_begin(int t)
{
    ShowClip c;
    if (!show_fx_split_rect(t, &c)) return 0;
    show_fx_clip(&c);
    return 1;
}

void show_fx_split_end(void) { show_fx_clip_off(); }

/* one brick: face, a highlight row on top, mortar is whatever shows between */
static void brick(int x, int y, int w, int h, const ShowWipe *wp)
{
    plat_rect(x, y + SHOW_CANVAS_Y, w, h, wp->mortar);
    if (w > 1 && h > 1) {
        plat_rect(x, y + SHOW_CANVAS_Y, w - 1, h - 1, wp->color);
        plat_rect(x, y + SHOW_CANVAS_Y, w - 1, 1, wp->hilite);
    }
}

/* the brick edge: a running-bond column two bricks wide on the line, its
 * rows offset by half a brick so the edge interlocks. STACK: courses k of
 * n are laid (the newest drops in 2 px from above on its first frame);
 * SWEEP: the column rides the moving edge, full height. */
static void wipe_edge(const ShowWipe *w, int k, int age)
{
    int n = w->courses ? w->courses : 5, bw = w->brick_w ? w->brick_w : 16;
    int bh = w->brick_h ? w->brick_h : 8, row, x, y0, y1, ex;
    int top = -SHOW_CANVAS_Y, bottom = SHOW_CANVAS_H + SHOW_CANVAS_Y;
    if (k <= 0) return;
    if (w->kind == WIPE_STACK) {
        int ch = (SHOW_CANVAS_H + n - 1) / n;
        ex = w->line ? w->line : SHOW_WALL_X;
        y0 = top;
        y1 = k >= n ? bottom : k * ch;
    } else {
        int stop = w->line ? w->line : SHOW_CANVAS_W, e = stop * k / n;
        ex = w->side == WIPE_B_LEFT ? e : SHOW_CANVAS_W - e;
        if (k >= n && stop >= SHOW_CANVAS_W) return;            /* off the frame: done */
        y0 = top;
        y1 = bottom;
    }
    for (row = 0; ; row++) {
        int y = y0 + row * bh, drop = 0, off = (row & 1) ? bw / 2 : 0;
        if (y >= y1) break;
        if (w->kind == WIPE_STACK && age == 0 && k < n) {
            int ch = (SHOW_CANVAS_H + n - 1) / n;
            if (y + bh > (k - 1) * ch) drop = -2;                /* the newest course */
        }
        for (x = ex - bw - off; x < ex + bw - off; x += bw) {
            int h = DOOMO_MIN(bh, y1 - y);
            if (h > 0) brick(x, y + drop, bw, h, w);
        }
    }
}

/* ---- whip lines ---------------------------------------------------------------- */
static void whip_lines(int t)
{
    const ShowCam *c = show_cam();
    int i, v = c->vx / CAM_CU, len;
    if (!c->whip) return;
    if (v < 0) v = -v;
    if (v < 6) return;
    len = DOOMO_MIN(v * 3, 200);
    for (i = 0; i < 14; i++) {
        uint32_t h = show_hash3(0x5157u, (uint32_t)i, (uint32_t)t);
        int y = (int)(h % SHOW_CANVAS_H), x = (int)((h >> 9) % (SHOW_CANVAS_W + len)) - len;
        plat_rect(x, y + SHOW_CANVAS_Y, len, 1, (h >> 20) & 1 ? PAL_WHITE : PAL_GRAY_L);
    }
}

void show_fx_draw(int t)
{
    int k, age;
    const ShowWipe *w;
    if (show_preroll()) return;
    whip_lines(t);
    w = wipe_now(t, &k, &age);
    if (w) wipe_edge(w, k, age);
}

/* ---- camfx without the offset (the camera carries the shake) ------------------ */
void show_fx_post(int t)
{
    int r, lvl, pid, pl;
    uint8_t c;
    (void)t;
    if (show_preroll()) return;
    pid = camfx_pal_state(&pl);
    if (pid != PAL_LERP_NONE) plat_pal_lerp(pid, pl);          /* keeps the CLUT live */
    if (camfx_zoom() != 256)
        plat_post_scale(camfx_zoom(), camfx_zoom(), SHOW_CANVAS_W / 2, SHOW_CANVAS_Y + SHOW_CANVAS_H / 2);
    r = camfx_iris_radius();
    if (r >= 0) plat_post_iris(s_fx.iris_cx, s_fx.iris_cy, r, s_fx.iris_heart, 0);
    lvl = camfx_blend(&c);
    if (lvl > 0) plat_post_blend(c, lvl);
}

void show_fx_matte(void)
{
    if (show_preroll()) return;
    if (s_fx.set.matte != MATTE_BLACK) return;
    plat_rect(0, 0, 320, SHOW_CANVAS_Y, PAL_BLACK);
    plat_rect(0, SHOW_CANVAS_Y + SHOW_CANVAS_H, 320, 240 - SHOW_CANVAS_Y - SHOW_CANVAS_H, PAL_BLACK);
}

/* ---- dissolve -------------------------------------------------------------------- */
int show_fx_dissolve_level(int t)
{
    int a, n = s_fx.set.dissolve_len;
    if (!n) return -1;
    a = t - s_fx.set.dissolve_t0;
    if (a < 0) return 0;
    if (a >= n - 1) return 16;
    return (a + 1) * 16 / n;
}

#define DIS_MAX (160 * 120)
static uint8_t s_dis_pix[DIS_MAX];
static DoomoImage s_dis_img;
static const struct DoomoImage *s_dis_src;
static int s_dis_level = -1;

const struct DoomoImage *show_fx_dissolve(const struct DoomoImage *b, int level16)
{
    static const uint8_t BAYER[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
    int x, y;
    if (!b || level16 <= 0) return 0;
    if (level16 >= 16) return b;
    if ((uint32_t)b->w * b->h > DIS_MAX) return level16 >= 8 ? b : 0;
    if (s_dis_src != b || s_dis_level != level16) {
        for (y = 0; y < b->h; y++)
            for (x = 0; x < b->w; x++) {
                size_t i = (size_t)y * b->w + x;
                s_dis_pix[i] = BAYER[((y & 3) << 2) | (x & 3)] < level16 ? b->pix[i] : 0;
            }
        s_dis_img.w = b->w;
        s_dis_img.h = b->h;
        s_dis_img.pix = s_dis_pix;
        s_dis_img.uid = doomo_image_new_uid();             /* new content, new key */
        s_dis_src = b;
        s_dis_level = level16;
    }
    return &s_dis_img;
}
