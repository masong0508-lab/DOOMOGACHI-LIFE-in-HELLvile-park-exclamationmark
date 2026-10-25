/* show_cam.c - the stage camera and the plane transform (WP8 + WP5's Q16
 * parallax and per-plane scale; show_cam.h has the model). Owner: ENGINE
 * lane 2.
 *
 * Nothing here accumulates: every value is computed from the shot frame, the
 * key list and integer arithmetic (a rate is whole cam units per frame, a
 * parallax a fraction), so frame 10,000 of a drift is exactly
 * x0 + v * 10,000 (AT5.1) and a seek equals a full run (spec 8.2).
 */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/camfx.h"
#include "show/show_cam.h"
#include "show/show_l2.h"

#define MAX_KEYS 96

static struct {
    ShowCamSetup setup;
    ShowCamKey   keys[MAX_KEYS];
    int          n;
    ShowCam      cur;
} s_cam;

/* ---- eases (Q16 in, Q16 out) ---------------------------------------------- */
static int32_t ease(int kind, int32_t t)
{
    int64_t u = t, one = CAM_Q16;
    switch (kind) {
    case CE_IN:    return (int32_t)(u * u / one);
    case CE_OUT:   return (int32_t)(one - (one - u) * (one - u) / one);
    case CE_INOUT: return (int32_t)(u * u / one * (3 * one - 2 * u) / one);
    case CE_WHIP:                                   /* in-out cubic: a fast middle */
        if (u < one / 2) return (int32_t)(4 * u * u / one * u / one);
        u = 2 * one - 2 * u;                         /* (-2t + 2), Q16 */
        return (int32_t)(one - u * u / one * u / one / 2);
    default:       return t;
    }
}

static int64_t floordiv64(int64_t n, int64_t d)
{
    int64_t q = n / d;
    return (n % d != 0 && ((n < 0) != (d < 0))) ? q - 1 : q;
}

/* a key's value at t (t >= t0) */
static int32_t key_value(const ShowCamKey *k, int t)
{
    switch (k->kind) {
    case CK_MOVE: {
        int32_t f;
        if (t >= k->t1) return k->a;
        f = (int32_t)(((int64_t)(t - k->t0 + 1) * CAM_Q16) / (k->t1 - k->t0 + 1));
        return k->b + (int32_t)(((int64_t)(k->a - k->b) * ease(k->ease, f)) / CAM_Q16);
    }
    case CK_VEL:
        return k->b + k->a * (t - (int)k->c);
    case CK_CREEP: {
        int tau = t - k->t0, r = k->c > 0 ? (int)k->c : 1;
        if (tau <= 0) return k->b;
        if (tau < r) return k->b + (int32_t)floordiv64((int64_t)k->a * tau * tau, 2 * r);
        return k->b + (int32_t)floordiv64((int64_t)k->a * (2 * tau - r), 2);
    }
    default:
        return k->a;
    }
}

/* the CU per frame of a key at t (for the plog and the whip lines) */
static int32_t key_speed(const ShowCamKey *k, int t)
{
    return key_value(k, t) - (t > k->t0 ? key_value(k, t - 1)
                                        : (k->kind == CK_MOVE ? k->b : key_value(k, t)));
}

void show_cam_reset(void)
{
    memset(&s_cam, 0, sizeof(s_cam));
    s_cam.setup.s = CAM_Q16;
    s_cam.cur.s = CAM_Q16;
    s_cam.cur.cx = SHOW_CANVAS_W / 2;
    s_cam.cur.cy = SHOW_CANVAS_H / 2;
}

void show_cam_bind(const ShowCamSetup *setup, const ShowCamKey *keys, int n)
{
    show_cam_reset();
    if (setup) {
        s_cam.setup = *setup;
        if (!s_cam.setup.s) s_cam.setup.s = CAM_Q16;
    }
    if (n > MAX_KEYS) n = MAX_KEYS;
    if (keys && n > 0) memcpy(s_cam.keys, keys, sizeof(ShowCamKey) * (size_t)n);
    s_cam.n = n > 0 ? n : 0;
}

int32_t show_cam_axis(int t, int axis)
{
    const ShowCamKey *last = 0;
    int i;
    for (i = 0; i < s_cam.n; i++) {
        const ShowCamKey *k = &s_cam.keys[i];
        if (k->t0 > t) break;                    /* sorted by t0 */
        if (k->axis == axis) last = k;
    }
    if (last) return key_value(last, t);
    return axis == CA_X ? s_cam.setup.x : axis == CA_Y ? s_cam.setup.y : s_cam.setup.s;
}

void show_cam_eval(int t, ShowCam *out)
{
    const ShowCamKey *kx = 0;
    int i;
    memset(out, 0, sizeof(*out));
    out->x = show_cam_axis(t, CA_X);
    out->y = show_cam_axis(t, CA_Y);
    out->s = show_cam_axis(t, CA_S);
    if (out->s <= 0) out->s = CAM_Q16;
    out->cx = s_cam.setup.cx ? s_cam.setup.cx : SHOW_CANVAS_W / 2;
    out->cy = s_cam.setup.cy ? s_cam.setup.cy : SHOW_CANVAS_H / 2;
    for (i = 0; i < s_cam.n; i++) {
        if (s_cam.keys[i].t0 > t) break;
        if (s_cam.keys[i].axis == CA_X) kx = &s_cam.keys[i];
    }
    if (kx) {
        out->vx = key_speed(kx, t);
        out->whip = (kx->flags & CKF_WHIP) && kx->kind == CK_MOVE && t <= kx->t1;
    }
}

void show_cam_frame(int t)
{
    int dx = 0, dy = 0, z;
    show_cam_eval(t, &s_cam.cur);
    z = camfx_zoom();                      /* BOP_ZOOM's snap zoom scales the 2D planes too */
    if (z != 256) s_cam.cur.s = (int32_t)(((int64_t)s_cam.cur.s * z) >> 8);
    camfx_offset(&dx, &dy);
    /* the planes carry SHOW_OVERSCAN px past the picture: a shake never
     * moves them further, so no edge ever shows (AT8.3; SHK_SHOTGUN's 9 px
     * becomes 8) */
    s_cam.cur.shake_x = (int16_t)DOOMO_CLAMP(dx, -SHOW_OVERSCAN, SHOW_OVERSCAN);
    s_cam.cur.shake_y = (int16_t)DOOMO_CLAMP(dy, -SHOW_OVERSCAN, SHOW_OVERSCAN);
}

const ShowCam *show_cam(void) { return &s_cam.cur; }

/* ---- plane transform -------------------------------------------------------- */
static void axis_xf(int32_t cam_cu, int pn, int pd, int at, int shake, int c,
                    int32_t s_plane, int px, int32_t *o, int32_t *f)
{
    int64_t base_cu, q16;
    if (pd <= 0) pd = 1;
    /* exact: at + shake - floor(cam * pn / pd), all in cam units */
    base_cu = (int64_t)(at + shake) * CAM_CU - floordiv64((int64_t)cam_cu * pn, pd);
    if (s_plane == CAM_Q16) {
        int64_t ip = floordiv64(base_cu, CAM_CU);
        *o = (int32_t)ip;
        *f = (int32_t)(((base_cu - ip * CAM_CU) * CAM_Q16) / CAM_CU);
        return;
    }
    /* scaled about c: c + (base - c) * s */
    q16 = (int64_t)c * CAM_Q16 +
          floordiv64((floordiv64(base_cu * CAM_Q16, CAM_CU) - (int64_t)c * CAM_Q16) * s_plane, CAM_Q16);
    *o = (int32_t)floordiv64(q16, CAM_Q16);
    *f = (int32_t)(q16 - (int64_t)*o * CAM_Q16);
    (void)px;
}

void show_cam_plane(const ShowCam *cam, const ShowPlaneCam *pc, int px, ShowXform *out)
{
    int32_t sp;
    int sx = pc->noshake ? 0 : cam->shake_x, sy = pc->noshake ? 0 : cam->shake_y;
    if (px < 1) px = 1;
    sp = CAM_Q16 + (int32_t)(((int64_t)(cam->s - CAM_Q16) * (pc->spar_q8 ? pc->spar_q8 : 256)) >> 8);
    if (sp < CAM_Q16 / 8) sp = CAM_Q16 / 8;
    axis_xf(cam->x, pc->par_xn, pc->par_xd, pc->at_x, sx, cam->cx, sp, px, &out->ox, &out->fx);
    axis_xf(cam->y, pc->par_yn, pc->par_yd, pc->at_y, sy, cam->cy, sp, px, &out->oy, &out->fy);
    out->s = sp * px;
}

int show_xf_x(const ShowXform *xf, int u)
{
    return xf->ox + (int)show_floordiv(xf->fx + u * xf->s, CAM_Q16);
}

int show_xf_y(const ShowXform *xf, int v)
{
    return xf->oy + (int)show_floordiv(xf->fy + v * xf->s, CAM_Q16);
}

int show_xf_whole(const ShowXform *xf)
{
    int k = xf->s >> 16;
    if ((xf->s & 0xffff) || k < 1 || k > 4) return 0;
    return k;
}

/* ---- drawing a plate under a transform -------------------------------------- */
#if defined(DOOMO_PLATFORM_PS1) && !defined(SHOW_L2_HOST)
#include <psxgpu.h>
#include "ps1.h"

static void quad_tile(const DoomoImage *img, const ShowXform *xf, int tx, int ty, int flip)
{
    TexRef r;
    POLY_FT4 *p;
    int u0 = tx * IMG_TILE, v0 = ty * IMG_TILE;
    int tw = DOOMO_MIN(img->w - u0, IMG_TILE), th = DOOMO_MIN(img->h - v0, IMG_TILE);
    int l = flip ? img->w - u0 - tw : u0;
    int X0 = show_xf_x(xf, l), X1 = show_xf_x(xf, l + tw);
    int Y0 = show_xf_y(xf, v0) + SHOW_CANVAS_Y, Y1 = show_xf_y(xf, v0 + th) + SHOW_CANVAS_Y;
    if (X1 <= X0 || Y1 <= Y0 || X0 >= SCR_W || X1 <= 0 || Y0 >= SCR_H || Y1 <= 0) return;
    if (X1 - X0 > 1023 || Y1 - Y0 > 511) return;              /* the GPU's quad limit */
    if (!vram_image(img, tx | (flip << 8), ty, &r)) return;
    p = (POLY_FT4 *)gfx_alloc(sizeof(POLY_FT4));
    if (!p) return;
    setPolyFT4(p);
    setShadeTex(p, 1);
    setXY4(p, X0, Y0, X1, Y0, X0, Y1, X1, Y1);
    if (!flip)
        setUV4(p, r.u, r.v, r.u + tw, r.v, r.u, r.v + th, r.u + tw, r.v + th);
    else
        setUV4(p, r.u + tw, r.v, r.u, r.v, r.u + tw, r.v + th, r.u, r.v + th);
    p->tpage = r.tpage;
    p->clut = r.clut;
    gfx_chain(p);
    g_tpage_cur = r.tpage;
}

static void draw_scaled(const DoomoImage *img, const ShowXform *xf, int flags)
{
    int tx, ty, ntx = (img->w + IMG_TILE - 1) / IMG_TILE, nty = (img->h + IMG_TILE - 1) / IMG_TILE;
    int flip = (flags & IMG_FLIPX) ? 1 : 0;
    for (ty = 0; ty < nty; ty++)
        for (tx = 0; tx < ntx; tx++)
            quad_tile(img, xf, tx, ty, flip);
}
#else
/* host builds (the lane-2 test harness): every plate pixel as a rect, the
 * same edges the quads use */
static void draw_scaled(const DoomoImage *img, const ShowXform *xf, int flags)
{
    int u, v;
    for (v = 0; v < img->h; v++) {
        int y0 = show_xf_y(xf, v), y1 = show_xf_y(xf, v + 1);
        if (y1 <= y0 || y1 + SHOW_CANVAS_Y <= 0 || y0 + SHOW_CANVAS_Y >= 240) continue;
        for (u = 0; u < img->w; u++) {
            int su = (flags & IMG_FLIPX) ? img->w - 1 - u : u;
            uint8_t c = img->pix[(size_t)v * img->w + su];
            int x0 = show_xf_x(xf, u), x1 = show_xf_x(xf, u + 1);
            if (!c || x1 <= x0) continue;
            plat_rect(x0, y0 + SHOW_CANVAS_Y, x1 - x0, y1 - y0, c);
        }
    }
}
#endif

void show_draw_image_xf(const DoomoImage *img, const ShowXform *xf, int flags)
{
    int k;
    if (!img || !img->pix || !img->w || !img->h) return;
    k = show_xf_whole(xf);
    if (k) {
        plat_image(xf->ox, xf->oy + SHOW_CANVAS_Y, img, k, flags & IMG_FLIPX, 0);
        return;
    }
    draw_scaled(img, xf, flags);
}
