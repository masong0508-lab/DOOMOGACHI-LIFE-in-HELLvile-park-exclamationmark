/* show_l2_mod.c - plugs lane 2's picture modules (show_l2.h) into the player
 * (show_api.h): one module, `cam`, that
 *   - binds the shot's SHWR_PRIV_L2 "PIC1" blob (camera keys, FX setup and
 *     wipes, TEXT items, EVTS, SLOT table) at prep;
 *   - installs the plane transform hook so every lane-1 plane (BANDS, IMAGE,
 *     GRAIN) goes through the stage camera (keys, drift, pans, whips, cranes,
 *     shake, parallax) and the scaled image draw for fractional zooms;
 *   - registers the STROKES and TILEMAP plane kinds;
 *   - runs show ops 0x0100-0x01FF and the irises (slot-anchored, heart);
 *   - ticks the blob's events and the camera every frame (pre-roll too);
 *   - draws whip lines / brick wipes (FX), SUPERs / signs / cards (TEXT).
 * Fades, flashes, irises and palette lerps reach the screen through the
 * core's camfx_apply(). Owner: ENGINE.
 */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/camfx.h"
#include "doomo/beat.h"
#include "show/show_api.h"
#include "show/shw_format.h"
#include "show/show_l2.h"
#include "show/show_cam.h"
#include "show/show_fx.h"
#include "show/show_strokes.h"
#include "show/show_tilemap.h"
#include "show/show_text.h"
#include "show_core.h"

/* the three things lane 2 asks of the player core (strong over show_l2.c's weak) */
int show_stage_slot_xy(int slot, int *x, int *y) { return show_l2_slot(slot, x, y); }
int show_shot_frame(void) { return g_show_shot.t; }
int show_preroll(void) { return !g_show_drawing; }

#define L2M_PLANES 32
static struct {
    const void *pay[L2M_PLANES];
    int16_t     h[L2M_PLANES];
    uint8_t     kind[L2M_PLANES];
    int         n;
    int         bound;
    int         dis;              /* dissolve level this frame (-1 none)  */
} s_m;

static void pc_of(const ShwPlane *p, ShowPlaneCam *pc)
{
    memcpy(pc, &p->par_xn, 14);                       /* same field order (shw_format.h) */
    if (!pc->par_xd) { pc->par_xn = 1; pc->par_xd = 1; }
    if (!pc->par_yd) { pc->par_yn = 1; pc->par_yd = 1; }
    pc->noshake = (p->flags & SHWPF_NOSHAKE) ? 1 : 0;
    pc->pad = 0;
}

static void xf_hook(const ShwPlane *p, int px, ShowPlaneXf *out)
{
    ShowPlaneCam pc;
    if (!p) { memset(out, 0, sizeof *out); out->s = px << 16; return; }
    pc_of(p, &pc);
    show_cam_plane(show_cam(), &pc, px, (ShowXform *)out);
}

static void img_hook(const DoomoImage *img, const ShowPlaneXf *xf, int flags)
{
    show_draw_image_xf(img, (const ShowXform *)xf, flags);
    show_stat_add(SHOW_STAT_SPRITES, 1);
}

/* ---- particles (SOP_L2_PFX) and the lens (SOP_L2_LENS) ---------------------
 * Pure functions of (start frame, now): an emitter keeps its arguments and
 * the frame it fired; every particle is hashed from (seed, i). */
#define PFX_MAX 12
typedef struct { int16_t t0, x, y, n, seed, mode, color, life, size; } Pfx;
static Pfx s_pfx[PFX_MAX];
static int s_npfx;

typedef struct { int16_t t0, kind, over, twos, seed; } LensEv;
#define LENS_EV 8
static struct {
    int16_t t_hit, kind_hit, seed;    /* the last splat/gib/paint             */
    int16_t t_clear, clear_over, twos;/* the clear after it (-1 none)          */
    int16_t drip, drip_t;             /* drip on since drip_t                 */
} s_lens;

static void pfx_fire(const int16_t *a, int t)
{
    Pfx *p;
    if (s_npfx >= PFX_MAX) memmove(&s_pfx[0], &s_pfx[1], sizeof(Pfx) * (PFX_MAX - 1)), s_npfx--;
    p = &s_pfx[s_npfx++];
    p->t0 = (int16_t)t; p->x = a[0]; p->y = a[1]; p->n = (int16_t)DOOMO_CLAMP(a[2], 1, 64);
    p->seed = a[3]; p->mode = a[4]; p->color = a[5] ? a[5] : PAL_WHITE;
    p->life = (int16_t)(a[6] > 0 ? a[6] : 24); p->size = (int16_t)(a[7] > 0 ? a[7] : 1);
}

static void heart(int x, int y, int s, uint8_t c)
{
    plat_rect(x - s, y, s, s, c);
    plat_rect(x + s, y, s, s, c);
    plat_rect(x - s, y + s, 3 * s, s, c);
    plat_rect(x, y + 2 * s, s, s, c);
}

/* particle i of p at age a (show px); 0 if it is gone */
static int pfx_pos(const Pfx *p, int i, int a, uint32_t h, int *ox, int *oy)
{
    int dx = (int)(h & 255) - 128, dy = (int)((h >> 8) & 255) - 128;
    int sp = 24 + (int)((h >> 16) & 63), life = p->life - (int)((h >> 22) & 7);
    if (a < 0 || a >= life) return 0;
    switch (p->mode) {
    default:
    case 0: case 3: case 4: {            /* burst: fast out, easing, a little gravity */
        int e = a * (2 * life - a);
        *ox = p->x + dx * sp * e / (128 * 16 * life);
        *oy = p->y + dy * sp * e / (128 * 16 * life) + (p->mode == 4 ? a * a / 60 : a * a / 40);
        break;
    }
    case 1:                               /* rise: drift up, sway */
        *ox = p->x + dx / 8 + ((((a + i * 5) >> 3) & 1) ? 1 : -1) * (int)((h >> 27) & 1);
        *oy = p->y - a * sp / 40 - (int)((h >> 12) & 7);
        break;
    case 2:                               /* fall: from above the point, flutter */
        *ox = p->x + dx / 2 + ((((a + i * 3) >> 2) & 1) ? 1 : 0);
        *oy = p->y - 40 + (int)((h >> 12) & 31) + a * sp / 32;
        break;
    }
    return life;
}

static void pfx_draw(const Pfx *p, int t)
{
    int a = t - p->t0, i, px = g_show_shot.px > 0 ? g_show_shot.px : 1;
    if (a < 0 || a >= p->life) return;
    for (i = 0; i < p->n; i++) {
        uint32_t h = show_hash3(0x9F5u ^ (uint32_t)p->seed, (uint32_t)i, 77u);
        int x, y, x1, y1, life, s = p->size * px, k;
        uint8_t c = (uint8_t)p->color;
        if (!(life = pfx_pos(p, i, a, h, &x, &y))) continue;
        if (x < -8 || x > SHOW_CANVAS_W + 8 || y < -8 || y > SHOW_CANVAS_H + 8) continue;
        if (p->mode == 0 || p->mode == 4) {
            /* the trail: the last 3 positions, thinning; the head twinkles white */
            for (k = 3; k >= 1; k--)
                if (a - k >= 0 && pfx_pos(p, i, a - k, h, &x1, &y1) && (a < life - 6 || k == 1))
                    plat_rect(x1, SHOW_Y(y1), s, s, c);
            if (a > life - 5 && (i & 1)) continue;
            plat_rect(x - (s > 1 ? 0 : 0), SHOW_Y(y), s + 1, s + 1, ((a + i) & 3) == 0 ? PAL_WHITE : c);
            if (p->mode == 4 && ((a + i) % 5) == 0) {           /* a 4-point glint */
                plat_rect(x - s, SHOW_Y(y), 3 * s + 1, 1, PAL_WHITE);
                plat_rect(x, SHOW_Y(y) - s, 1, 3 * s + 1, PAL_WHITE);
            }
        } else if (p->mode == 3) {
            heart(x, SHOW_Y(y), s + (a < 4 ? 0 : 1), c);
        } else {
            plat_rect(x, SHOW_Y(y), 2 * s, 2 * s, c);
            if (p->mode == 1) plat_rect(x, SHOW_Y(y), s, s, PAL_WHITE);  /* a bubble/smoke highlight */
        }
    }
}

static void lens_op(const int16_t *a, int t)
{
    switch (a[0]) {
    case 0: case 1: case 2:
        s_lens.t_hit = (int16_t)t; s_lens.kind_hit = a[0]; s_lens.seed = (int16_t)(a[3] ^ t);
        s_lens.t_clear = -1;
        break;
    case 3: s_lens.drip = 1; s_lens.drip_t = (int16_t)t; break;
    case 4: s_lens.drip = 0; break;
    default:
        s_lens.t_clear = (int16_t)t; s_lens.clear_over = (int16_t)(a[1] > 0 ? a[1] : 12);
        s_lens.twos = a[2];
        break;
    }
}

static void blob(int cx, int cy, int r, uint8_t c)
{
    int y;
    for (y = -r; y <= r; y++) {
        int w = r - (y * y) / (r > 0 ? r : 1) / 2;
        if (w > 0) plat_rect(cx - w, SHOW_Y(cy + y), 2 * w, 1, c);
    }
}

static void lens_draw(int t)
{
    int a, n, i, keep = 256, nb;
    if (s_lens.t_hit < 0 || t < s_lens.t_hit) return;
    a = t - s_lens.t_hit;
    if (s_lens.t_clear >= 0 && t >= s_lens.t_clear) {
        int c = t - s_lens.t_clear;
        if (s_lens.twos) c &= ~1;
        if (c >= s_lens.clear_over) return;
        keep = 256 - c * 256 / s_lens.clear_over;
    }
    nb = s_lens.kind_hit == 0 ? 7 : s_lens.kind_hit == 1 ? 14 : 22;
    n = nb * keep / 256;
    for (i = 0; i < nb; i++) {
        uint32_t h = show_hash3(0x1E45u ^ (uint32_t)s_lens.seed, (uint32_t)i, 3u);
        int x = (int)(h % SHOW_CANVAS_W), y = (int)((h >> 9) % SHOW_CANVAS_H);
        int r = s_lens.kind_hit == 1 ? 3 + (int)((h >> 18) & 7) : 6 + (int)((h >> 18) & 15);
        uint8_t col = s_lens.kind_hit == 2 ? (uint8_t)((h >> 25) & 1 ? PAL_UI_PINK : PAL_UI_PURPLE)
                                           : (uint8_t)((h >> 25) & 1 ? PAL_UI_RED : PAL_UI_RED_D);
        if ((int)((h >> 26) & 31) * nb / 32 >= n) continue;   /* cleared ones first */
        if (a < 2) r = r * (a + 1) / 3;                       /* the hit lands over 2 f */
        if (s_lens.drip || s_lens.kind_hit != 1) y += (a - 2 > 0 ? (a - 2) : 0) * (int)((h >> 29) & 3) / 6;
        blob(x, y, r, col);
        if (s_lens.kind_hit != 1) plat_rect(x - r / 3, SHOW_Y(y - r / 2), DOOMO_MAX(1, r / 3), 1, PAL_WHITE);
    }
}

int show_l2_ext_op(int op, const int16_t *a, int n, int t)
{
    if (n < 8) return 0;
    if (op == SOP_L2_PFX) { pfx_fire(a, t); return 1; }
    if (op == SOP_L2_LENS) { lens_op(a, t); return 1; }
    return 0;
}

static void m_reset(void)
{
    show_l2_shot_reset();
    memset(&s_m, 0, sizeof s_m);
    s_m.dis = -1;
    s_npfx = 0;
    memset(&s_lens, 0, sizeof s_lens);
    s_lens.t_hit = -1;
    s_lens.t_clear = -1;
}

static void m_prep(const ShowShot *s)
{
    uint32_t bytes = 0;
    int n = 0;
    const void *b = show_chunk_find(SHWR_PRIV_L2, &n, &bytes);
    (void)s;
    s_m.bound = (b && show_l2_bind(b, bytes) >= 0);
    g_show_plane_xf_hook = xf_hook;
    g_show_draw_image_hook = img_hook;
}

static int m_ext(const BeatEv *ev)
{
    int op = show_op_of(ev);
    if (op >= SOP_L2_BASE && op < SOP_L2_END) {
        const int32_t *a = show_args(ev->b);
        int16_t w[8];
        int i;
        for (i = 0; i < 8; i++) w[i] = (int16_t)a[i];
        show_l2_op(op, w, 8, g_show_shot.t < 0 ? 0 : g_show_shot.t);
        return 1;
    }
    if (op < 0 && (ev->op == BOP_IRIS_IN || ev->op == BOP_IRIS_OUT))
        return show_fx_beat_ext(ev);
    return 0;
}

static void m_tick(int t)
{
    if (t >= 0) show_l2_events(t);
    show_l2_frame_begin(t);
}

/* ---- STROKES / TILEMAP plane kinds ------------------------------------------ */
static int slot_of(const void *pay)
{
    int i;
    for (i = 0; i < s_m.n; i++) if (s_m.pay[i] == pay) return i;
    return -1;
}

static void kind_prep(const ShwPlane *p, const void *pay)
{
    int h, bytes = (int)p->bytes - (int)sizeof(ShwPlane);
    if (s_m.n >= L2M_PLANES || bytes <= 0) return;
    h = p->kind == SHWP_STROKES ? show_strokes_bind(pay, (uint32_t)bytes)
                                : show_tilemap_bind(pay, (uint32_t)bytes);
    if (h < 0) { show_warn("L2-PLANE", "a STROKES/TILEMAP payload did not bind"); return; }
    s_m.pay[s_m.n] = pay;
    s_m.h[s_m.n] = (int16_t)h;
    s_m.kind[s_m.n] = p->kind;
    s_m.n++;
}

static void kind_draw(const ShwPlane *p, const void *pay, int t)
{
    ShowXform xf;
    int i = slot_of(pay);
    if (i < 0) return;
    xf_hook(p, g_show_shot.px > 0 ? g_show_shot.px : 1, (ShowPlaneXf *)&xf);
    if (s_m.kind[i] == SHWP_STROKES) show_strokes_draw(s_m.h[i], t, &xf);
    else show_tilemap_draw(s_m.h[i], &xf);
}

static const ShowPlaneKind k_kinds[] = {
    { SHWP_TILEMAP, "tilemap", kind_prep, kind_draw },
    { SHWP_STROKES, "strokes", kind_prep, kind_draw },
    { 0, 0, 0, 0 }
};

static void m_draw(int layer, int t)
{
    int i;
    switch (layer) {
    case SHOW_L_PLANES:                      /* any planes the blob itself carries */
        for (i = 0; i < show_l2_planes(); i++)
            if (show_l2_plane_side(i) == 0) show_l2_plane_draw(i, t);
        if (show_fx_split_begin(t)) {
            for (i = 0; i < show_l2_planes(); i++)
                if (show_l2_plane_side(i) == 1) show_l2_plane_draw(i, t);
            show_fx_split_end();
        }
        break;
    case SHOW_L_FX:
        for (i = 0; i < s_npfx; i++) pfx_draw(&s_pfx[i], t);
        show_fx_draw(t);
        lens_draw(t);
        break;
    case SHOW_L_TEXT: show_text_draw(t); break;
    case SHOW_L_HUD: show_fx_clip_off(); show_fx_matte(); break;
    default: break;
    }
}

static void m_hash(ShowCrc *c)
{
    const ShowCam *cam = show_cam();
    show_crc_add(c, &cam->x, 12);
}

const ShowModule show_mod_cam = { "cam", SHOW_API_VERSION, 0, m_reset, m_prep, m_ext, m_tick, m_draw,
                                  m_hash, k_kinds };
