/* show_amb.c - the sets' ambient layers (SHWR_AMB; series/sets/SETS.md sec.6.2)
 * (owner: ENGINE; the math is ART's show/art/engine/amb_math.h).
 *
 * Every layer is a pure function of (record, shot frame t), so a seek draws
 * the same pixels and the module keeps no state but the current frame.
 *
 *   DRIFT  the plane's own slide (the compiler sets SHWPF_TILE_X: it wraps)
 *   WAVE   the plane is drawn in clipped horizontal bands, each shifted by
 *          amb_wave_dx (bands with the same shift are merged: amp 1 = ~4 draws)
 *   SWAY   a prop plane swings (amb_sway_dx, k-th prop `lag` frames late)
 *   FLIP   a looping drawing (SHWR_IMAGE run) on a plane (amb_flip_frame)
 *   MOTES  fireflies, sparkles, embers, dust... as 1-5 px rects (amb_mote),
 *          after their plane, or after the actors (flag FRONT)
 *   LAMPS  every lantern core flickers: dim = its recolour runs, bloom = a
 *          1 px ring (amb_lamp_state)
 * PCYCLE (CLUT rotation) is deferred (DEFERRED.md). Crowds are CRWD records.
 *
 * show_stage.c calls show_amb_plane_off (from show_plane_xf),
 * show_amb_plane_draw (instead of a plane's draw) and show_amb_after_plane.
 */
#include <string.h>
#include "show_core.h"
#include "show/show_fx.h"
#include "../art/engine/amb_math.h"
#if defined(DOOMO_PLATFORM_PS1) && !defined(SHOW_L2_HOST)
#include "ps1.h"
extern int g_show_upload_sync;            /* show_host.c / show_host_wrap.c */
#endif

#define SHWR_AMB   36
#define AMB_MAX    48
enum { AK_DRIFT = 1, AK_WAVE, AK_SWAY, AK_FLIP, AK_MOTES, AK_LAMPS, AK_PCYCLE };
enum { AF_FRONT = 1, AF_HIDDEN = 2 };
#define NOPLANE 0xFFFF

typedef struct AmbHdr { uint8_t kind, flags; uint16_t bytes, plane; } AmbHdr;

static const AmbHdr *s_item[AMB_MAX];
static int s_n;
static int s_t;
static const ShwPlane *s_wave_p;        /* the plane a WAVE band is drawing */
static int s_wave_dx;

static const int16_t *P(const AmbHdr *h) { return (const int16_t *)((const uint8_t *)h + 6); }

static void warm_images(void);

static void amb_reset(void) { s_n = 0; s_t = 0; s_wave_p = NULL; }

static void amb_prep(const ShowShot *s)
{
    int n = 0, i;
    uint32_t bytes = 0, off = 0;
    const uint8_t *p = (const uint8_t *)show_chunk_find(SHWR_AMB, &n, &bytes);
    (void)s;
    s_n = 0;
    for (i = 0; p && i < n && s_n < AMB_MAX && off + sizeof(AmbHdr) <= bytes; i++) {
        const AmbHdr *h = (const AmbHdr *)(p + off);
        if (h->bytes < sizeof(AmbHdr) || off + h->bytes > bytes) break;
        s_item[s_n++] = h;
        off += h->bytes;
    }
    warm_images();
}

static void amb_tick(int t) { s_t = t; }

/* the planes of the cut's first frame into VRAM, synchronously: a frame's
 * display list (56 KB) takes ~3 plate tiles, so a 640x240 midway would pop
 * in over its first frames. Only what frame 0 draws is warmed (hidden prop
 * states, later flip drawings and off-screen cells load when they show), so
 * the image cache's pages keep room for the cast. The list is thrown away. */
static void warm_images(void)
{
#if defined(DOOMO_PLATFORM_PS1) && !defined(SHOW_L2_HOST)
    if (!show_image_count()) return;
    s_t = 0;
    g_show_upload_sync = 1;
    show_stage_draw(0);
    g_show_upload_sync = 0;
    gfx_begin();
#endif
}

static int scale_of(const ShowPlaneXf *xf)
{
    int s = (xf->s + 0x8000) >> 16;
    return s < 1 ? 1 : s;
}

/* show_plane_xf: drift, sway and the wave band's shift */
void show_amb_plane_off(const ShwPlane *p, ShowPlaneXf *xf)
{
    int i, s;
    if (!p || !s_n) return;
    s = scale_of(xf);
    for (i = 0; i < s_n; i++) {
        const AmbHdr *h = s_item[i];
        const int16_t *v = P(h);
        if (h->plane != p->id) continue;
        if (h->kind == AK_DRIFT) {
            xf->ox += amb_drift(v[0], s_t) * s;
            xf->oy += amb_drift(v[1], s_t) * s;
        } else if (h->kind == AK_SWAY) {
            int dx = amb_sway_dx(v[0], v[1], s_t, v[2], v[3]);
            xf->ox += dx * s;
            if (v[4]) xf->oy -= ((dx < 0 ? -dx : dx) * v[4] + (v[0] > 0 ? v[0] - 1 : 0)) / (v[0] > 0 ? v[0] : 1) * s;
        }
    }
    if (p == s_wave_p) xf->ox += s_wave_dx * s;
}

static const AmbHdr *find(const ShwPlane *p, int kind)
{
    int i;
    for (i = 0; i < s_n; i++)
        if (s_item[i]->kind == kind && s_item[i]->plane == p->id) return s_item[i];
    return NULL;
}

/* WAVE: draw the plane in bands; returns 1 when it drew the plane */
int show_amb_plane_draw(const ShwPlane *p, const void *pay, int t,
                        void (*draw)(const ShwPlane *, const void *, int))
{
    const AmbHdr *h;
    const int16_t *v;
    ShowPlaneXf xf;
    ShowClip c;
    int s, r, r0, r1, strip, y0, ybot;
    if (!s_n || !p || !draw || !(h = find(p, AK_WAVE))) return 0;
    v = P(h);
    strip = v[3] > 0 ? v[3] : 1;
    r0 = v[4];
    r1 = v[5];
    s_wave_p = NULL;
    show_plane_xf(p, &xf);
    s = scale_of(&xf);
    c.x = -8;
    c.w = SHOW_CANVAS_W + 16;
    /* rows above the wave: unshifted */
    y0 = xf.oy + r0 * s;
    if (y0 > -8) {
        c.y = -8; c.h = y0 + 8;
        show_fx_clip(&c);
        draw(p, pay, t);
    }
    r = r0;
    while (r < r1) {
        int dx = amb_wave_dx(v[0], v[1], v[2], r, t), re = r + strip;
        while (re < r1 && amb_wave_dx(v[0], v[1], v[2], re, t) == dx) re += strip;
        if (re > r1) re = r1;
        c.y = xf.oy + r * s;
        c.h = (re - r) * s;
        if (c.y < SHOW_CANVAS_H + 8 && c.y + c.h > -8) {
            s_wave_p = p;
            s_wave_dx = dx;
            show_fx_clip(&c);
            draw(p, pay, t);
            s_wave_p = NULL;
        }
        r = re;
    }
    ybot = xf.oy + r1 * s;
    if (ybot < SHOW_CANVAS_H + 8) {
        c.y = ybot; c.h = SHOW_CANVAS_H + 8 - ybot;
        show_fx_clip(&c);
        draw(p, pay, t);
    }
    show_fx_clip_off();
    return 1;
}

static void dot(int x, int y, int s, uint8_t col)
{
    if (x < -8 || y < -8 || x > SHOW_CANVAS_W + 8 || y > SHOW_CANVAS_H + 8) return;
    plat_rect(x, SHOW_Y(y), s, s, col);
}

static void motes(const AmbHdr *h, int ox, int oy, int s)
{
    const int16_t *v = P(h);
    uint32_t seed = (uint16_t)v[7] | ((uint32_t)(uint16_t)v[8] << 16);
    int i, n = v[1];
    for (i = 0; i < n && i < 96; i++) {
        AmbMote m;
        uint8_t col;
        int x, y;
        if (!amb_mote(v[0], seed, i, s_t, v[2], v[3], v[4], v[5], v[6], &m)) continue;
        col = (uint8_t)(m.role == AMB_ROLE_CORE ? v[9] : m.role == AMB_ROLE_HI ? v[10] : v[11]);
        x = ox + m.x * s;
        y = oy + m.y * s;
        switch (m.size) {
        case 0: dot(x, y, s, col); break;
        case 2: dot(x, y, s, col); dot(x + s, y, s, col); dot(x, y + s, s, col); dot(x + s, y + s, s, col); break;
        case 3: dot(x - 2 * s, y, s, col); dot(x + 2 * s, y, s, col); dot(x, y - 2 * s, s, col); dot(x, y + 2 * s, s, col);
                /* fall through */
        default: dot(x, y, s, col); dot(x - s, y, s, col); dot(x + s, y, s, col); dot(x, y - s, s, col); dot(x, y + s, s, col); break;
        }
    }
    show_stat_add(SHOW_STAT_SPRITES, n);
}

static void lamps(const AmbHdr *h, int ox, int oy, int s)
{
    const int16_t *v = P(h), *q;
    const int16_t *end = (const int16_t *)((const uint8_t *)h + h->bytes);
    uint32_t seed = (uint16_t)v[0] | ((uint32_t)(uint16_t)v[1] << 16);
    int i, n = v[7];
    uint8_t ring = (uint8_t)v[6];
    q = v + 8;
    for (i = 0; i < n && q + 5 <= end; i++) {
        int x = q[0], y = q[1], w = q[2], hh = q[3], nr = q[4], k;
        int st = amb_lamp_state(seed, i, s_t, v[2], v[3], v[4], v[5]);
        const int16_t *runs = q + 5;
        q = runs + nr * 4;
        if (q > end) break;
        if (st == 1) {
            for (k = 0; k < nr; k++)
                plat_rect(ox + (x + runs[k * 4]) * s, SHOW_Y(oy + (y + runs[k * 4 + 1]) * s),
                          runs[k * 4 + 2] * s, s, (uint8_t)runs[k * 4 + 3]);
        } else if (st == 2) {
            int X = ox + (x - 1) * s, Y = oy + (y - 1) * s, W = (w + 2) * s, H = (hh + 2) * s;
            plat_rect(X + s, SHOW_Y(Y), W - 2 * s, s, ring);
            plat_rect(X + s, SHOW_Y(Y + H - s), W - 2 * s, s, ring);
            plat_rect(X, SHOW_Y(Y + s), s, H - 2 * s, ring);
            plat_rect(X + W - s, SHOW_Y(Y + s), s, H - 2 * s, ring);
        }
    }
}

static void flip(const AmbHdr *h, const ShowPlaneXf *pxf, int s)
{
    const int16_t *v = P(h);
    uint8_t holds[32];
    int k, n = v[1] < 32 ? v[1] : 32;
    ShowPlaneXf xf = *pxf;
    for (k = 0; k < n; k++) holds[k] = (uint8_t)(v[6 + k] > 0 ? v[6 + k] : 1);
    k = amb_flip_frame(holds, n, s_t, v[4], v[5]);
    xf.ox += v[2] * s;
    xf.oy += v[3] * s;
    show_draw_image(show_image(v[0] + k), &xf, 0);
}

/* after a plane: its flips, motes and lamps */
void show_amb_after_plane(const ShwPlane *p, int t)
{
    int i, s;
    ShowPlaneXf xf;
    (void)t;
    if (!s_n || !p) return;
    xf.s = 0;
    for (i = 0; i < s_n; i++) {
        const AmbHdr *h = s_item[i];
        if (h->plane != p->id || (h->flags & (AF_FRONT | AF_HIDDEN))) continue;
        if (h->kind != AK_FLIP && h->kind != AK_MOTES && h->kind != AK_LAMPS) continue;
        if (!xf.s) show_plane_xf(p, &xf);
        s = scale_of(&xf);
        if (h->kind == AK_FLIP) flip(h, &xf, s);
        else if (h->kind == AK_MOTES) motes(h, xf.ox, xf.oy, s);
        else lamps(h, xf.ox, xf.oy, s);
    }
}

/* the FX layer: front motes and plane-less motes */
static void amb_draw(int layer, int t)
{
    int i;
    (void)t;
    if (layer != SHOW_L_FX) return;
    for (i = 0; i < s_n; i++) {
        const AmbHdr *h = s_item[i];
        if (h->kind != AK_MOTES || (h->flags & AF_HIDDEN)) continue;
        if (h->plane == NOPLANE) {
            motes(h, 0, 0, g_show_shot.px > 0 ? g_show_shot.px : 1);
        } else if (h->flags & AF_FRONT) {
            const ShwPlane *p = show_plane_by_id(h->plane);
            ShowPlaneXf xf;
            if (!p) continue;
            show_plane_xf(p, &xf);
            motes(h, xf.ox, xf.oy, scale_of(&xf));
        }
    }
}

static void amb_hash(ShowCrc *c)
{
    int32_t v[2];
    v[0] = s_n;
    v[1] = s_t;
    show_crc_add(c, v, sizeof v);
}

const ShowModule show_mod_amb = { "amb", SHOW_API_VERSION, 0, amb_reset, amb_prep, NULL, amb_tick,
                                  amb_draw, amb_hash, NULL };
