/* show_l2.c - the lane-2 glue (show_l2.h): binds a shot's "PIC1" blob
 * (showtk/picture/blob.py) into the camera, effects, planes and text; runs
 * its events; draws what lane 2 owns; dispatches the SOP_L2 ops; and holds
 * weak defaults for the three things lane 2 asks of the player core.
 * Owner: ENGINE lane 2.
 */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/camfx.h"
#include "doomo/beat.h"
#include "show/show_l2.h"
#include "show/show_cam.h"
#include "show/show_fx.h"
#include "show/show_strokes.h"
#include "show/show_tilemap.h"
#include "show/show_text.h"

#define WEAK __attribute__((weak))

/* the player core defines these (show_stage.c / show_game.c); the defaults
 * keep the lane-2 modules linkable on their own */
WEAK int show_stage_slot_xy(int slot, int *x, int *y) { (void)slot; (void)x; (void)y; return 0; }
WEAK int show_shot_frame(void) { return 0; }
WEAK int show_preroll(void) { return 0; }

int show_l2_op(int op, const int16_t *args, int nargs, int t)
{
    (void)t;
    if (op < SOP_L2_BASE || op >= SOP_L2_END) return 0;
    return show_fx_op(op, args, nargs);
}

/* ---- the bound shot ---------------------------------------------------------- */
typedef struct {
    uint8_t      kind, side, px, boil;
    ShowPlaneCam pc;
    int          handle;               /* STROKES / TILEMAP handle             */
    DoomoImage   img;                  /* IMAGE                                */
    uint8_t      fill;                 /* FILL colour                          */
} L2Plane;

typedef struct { uint16_t t, op; int16_t a[8]; } L2Ev;

#define L2_PLANES 16
static struct {
    L2Plane      pl[L2_PLANES];
    int          npl;
    const L2Ev  *ev;
    uint32_t     nev;
    const int16_t *slot;
    uint32_t     nslot;
    int          frames;
} s_l2;

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

void show_l2_shot_reset(void)
{
    show_cam_reset();
    show_fx_reset();
    show_strokes_reset();
    show_tilemap_reset();
    show_text_reset();
    memset(&s_l2, 0, sizeof(s_l2));
}

static void bind_plane(const uint8_t *p, uint32_t n)
{
    L2Plane *q;
    uint32_t len;
    const uint8_t *pay;
    if (s_l2.npl >= L2_PLANES || n < 4 + sizeof(ShowPlaneCam) + 4) return;
    q = &s_l2.pl[s_l2.npl];
    memset(q, 0, sizeof(*q));
    q->kind = p[0]; q->side = p[1]; q->px = p[2] ? p[2] : 1; q->boil = p[3];
    memcpy(&q->pc, p + 4, sizeof(ShowPlaneCam));
    len = rd32(p + 4 + sizeof(ShowPlaneCam));
    pay = p + 8 + sizeof(ShowPlaneCam);
    if (8 + sizeof(ShowPlaneCam) + len > n) return;
    switch (q->kind) {
    case L2P_IMAGE:
        if (len < 4) return;
        q->img.w = rd16(pay);
        q->img.h = rd16(pay + 2);
        if ((uint32_t)q->img.w * q->img.h + 4 > len) return;
        q->img.pix = pay + 4;
        q->img.uid = doomo_image_new_uid();
        break;
    case L2P_STROKES: q->handle = show_strokes_bind(pay, len); if (q->handle < 0) return; break;
    case L2P_TILEMAP: q->handle = show_tilemap_bind(pay, len); if (q->handle < 0) return; break;
    case L2P_FILL: q->fill = len ? pay[0] : PAL_BLACK; break;
    default: return;
    }
    s_l2.npl++;
}

int show_l2_bind(const void *blob, uint32_t bytes)
{
    const uint8_t *b = (const uint8_t *)blob, *end = b + bytes, *p;
    int nsec, i;
    show_l2_shot_reset();
    if (!blob || bytes < 8 || memcmp(b, "PIC1", 4)) return -1;
    s_l2.frames = rd16(b + 4);
    nsec = rd16(b + 6);
    p = b + 8;
    for (i = 0; i < nsec && p + 8 <= end; i++) {
        uint32_t n = rd32(p + 4);
        const uint8_t *q = p + 8;
        if (q + n > end) return -1;
        if (!memcmp(p, "CAM ", 4) && n >= sizeof(ShowCamSetup) + 4)
            show_cam_bind((const ShowCamSetup *)q, (const ShowCamKey *)(q + sizeof(ShowCamSetup) + 4),
                          (int)rd32(q + sizeof(ShowCamSetup)));
        else if (!memcmp(p, "FX  ", 4) && n >= sizeof(ShowFxSetup))
            show_fx_bind((const ShowFxSetup *)q, (const ShowWipe *)(q + sizeof(ShowFxSetup)));
        else if (!memcmp(p, "PLAN", 4))
            bind_plane(q, n);
        else if (!memcmp(p, "TEXT", 4))
            show_text_bind(q, n);
        else if (!memcmp(p, "EVTS", 4) && n >= 4) {
            s_l2.nev = rd32(q);
            s_l2.ev = (const L2Ev *)(q + 4);
        } else if (!memcmp(p, "SLOT", 4) && n >= 4) {
            s_l2.nslot = rd32(q);
            s_l2.slot = (const int16_t *)(q + 4);
        }
        p = q + n;
    }
    return s_l2.frames;
}

int show_l2_slot(int slot, int *x, int *y)
{
    if (slot < 0 || (uint32_t)slot >= s_l2.nslot) return 0;
    *x = s_l2.slot[slot * 2];
    *y = s_l2.slot[slot * 2 + 1];
    return 1;
}

/* this frame's events, run the way beat.c runs them (the engine-native
 * ops through camfx, the host kinds and lane-2 ops through show_fx) */
void show_l2_events(int t)
{
    uint32_t i;
    for (i = 0; i < s_l2.nev; i++) {
        const L2Ev *e = &s_l2.ev[i];
        int16_t a = e->a[0], b = e->a[1];
        if (e->t != t) continue;
        if (e->op >= SOP_L2_BASE) { show_l2_op(e->op, e->a, 8, t); continue; }
        switch (e->op) {
        case BOP_SHAKE: camfx_shake(a); break;
        case BOP_PUNCH: camfx_punch(a, b); break;
        case BOP_ZOOM:  camfx_zoom_snap(a, b); break;
        case BOP_FLASH: camfx_flash((uint8_t)a, b & 0xFF, (b >> 8) & 0xFF); break;
        case BOP_FADE:  camfx_fade_to(b & 0xFF, (b >> 8) & 0xFF); break;
        case BOP_PAL_LERP: camfx_pal_lerp_to(a, b & 0xFF, (b >> 8) & 0xFF); break;
        case BOP_IRIS_IN: case BOP_IRIS_OUT: {
            BeatEv ev;
            ev.at = (uint16_t)t; ev.anchor = 0; ev.op = (uint8_t)e->op; ev.a = a; ev.b = b;
            show_fx_beat_ext(&ev);
            break;
        }
        default: break;
        }
    }
}

void show_l2_frame_begin(int t)
{
    show_fx_frame_begin(t);
    show_cam_frame(t);
}

/* ---- drawing ---------------------------------------------------------------------- */
void show_l2_image_draw(const DoomoImage *img, const ShowPlaneCam *pc, int px, int boil)
{
    ShowXform xf;
    int every = boil ? show_fx_boil_every(boil) : 0, dx, dy;
    show_cam_plane(show_cam(), pc, px, &xf);
    if (every && show_fx_boil(img->uid, 0, every, &dx, &dy)) {
        xf.ox += dx * px;
        xf.oy += dy * px;
    }
    show_draw_image_xf(img, &xf, 0);
}

void show_l2_plane_draw(int i, int t)
{
    const L2Plane *q;
    ShowXform xf;
    if (i < 0 || i >= s_l2.npl || show_preroll()) return;
    q = &s_l2.pl[i];
    switch (q->kind) {
    case L2P_IMAGE: show_l2_image_draw(&q->img, &q->pc, q->px, q->boil); break;
    case L2P_STROKES:
        show_cam_plane(show_cam(), &q->pc, q->px, &xf);
        show_strokes_draw(q->handle, t, &xf);
        break;
    case L2P_TILEMAP:
        show_cam_plane(show_cam(), &q->pc, q->px, &xf);
        show_tilemap_draw(q->handle, &xf);
        break;
    case L2P_FILL: plat_rect(0, 0, 320, 240, q->fill); break;
    default: break;
    }
}

int show_l2_planes(void) { return s_l2.npl; }

int show_l2_plane_xform(int i, struct ShowXform *xf)
{
    if (i < 0 || i >= s_l2.npl) return 0;
    show_cam_plane(show_cam(), &s_l2.pl[i].pc, s_l2.pl[i].px, xf);
    return 1;
}
int show_l2_plane_side(int i) { return (i >= 0 && i < s_l2.npl) ? s_l2.pl[i].side : 0; }

void show_l2_draw(int t)
{
    int i;
    if (show_preroll()) return;
    for (i = 0; i < s_l2.npl; i++)
        if (s_l2.pl[i].side == 0) show_l2_plane_draw(i, t);
    if (show_fx_split_begin(t)) {
        for (i = 0; i < s_l2.npl; i++)
            if (s_l2.pl[i].side == 1) show_l2_plane_draw(i, t);
        show_fx_split_end();
    }
    show_fx_draw(t);
    show_text_draw(t);
    show_fx_post(t);
    show_fx_matte();
}
