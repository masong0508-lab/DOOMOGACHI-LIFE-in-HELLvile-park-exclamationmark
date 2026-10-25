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

static void m_reset(void)
{
    show_l2_shot_reset();
    memset(&s_m, 0, sizeof s_m);
    s_m.dis = -1;
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
        show_l2_op(op, w, 8, g_show_shot.t);
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
    case SHOW_L_FX: show_fx_draw(t); break;
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
