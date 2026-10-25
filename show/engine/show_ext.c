/* show_ext.c - the BT_EXT dispatcher, the host kinds beat.c hands back, and
 * the core show ops 0x000-0x03F (owner: ENGINE lane 1; spec 4.2, 8.1).
 *
 * beat.c executes shake, punch, zoom, flash, fade, pal-lerp, hitstop, the
 * clock ops, labels, holds and flags itself. Everything else comes back as
 * BT_EXT: each event is offered to the modules' ext() hooks in registry
 * order, and what nobody takes is handled here:
 *   BOP_IRIS_OUT/IN  camfx_iris_out/in; b = hold | heart<<6 | (radius/2)<<7;
 *                    the centre is the anchor hook's position of actor/prop a
 *                    (lane 2 installs g_show_anchor_hook), else the picture
 *                    centre. SOP_IRIS gives an explicit centre instead.
 *   BOP_TINT         a = TINT_*, b = level16: a colour blend over the frame,
 *                    drawn just before camfx_apply (TINT_NONE or 0 = off)
 *   BOP_POP          object a, TWT table b: show_pop_age()
 *   BOP_EXPR         actor a, b = EXPR_* | mouth << 8: show_actor_chan()
 *   anything else    (SAY, CAPTION, CUT, ACT, TWITCH, NPC, CAM, HEADTURN, an
 *                    unknown show op): one warning per shot and op, then ignored
 * Fades (WP8): beat.c's BOP_FADE already ramps (camfx_fade_to); SOP_FADE_TO
 * and SOP_PAL_TO take their arguments from the ArgTable for long ramps.
 */
#include <stdio.h>
#include "show_core.h"

#define POP_MAX 32

static ShowActorChan s_chan[BEAT_ACTORS];
static int16_t s_pop_t[POP_MAX];
static uint8_t s_pop_tw[POP_MAX];
static uint8_t s_tint, s_tint_level;
static uint32_t s_warned[2];            /* BOP_* (bit per op) warned this shot */
static uint32_t s_marks;                /* SOP_MARK ids seen (P1)            */

int (*g_show_anchor_hook)(int id, int *x, int *y);

void show_ext_reset(void)
{
    int i;
    for (i = 0; i < BEAT_ACTORS; i++) {
        s_chan[i].expr = -1; s_chan[i].mouth = -1; s_chan[i].expr_t = -1; s_chan[i].pad = 0;
    }
    for (i = 0; i < POP_MAX; i++) { s_pop_t[i] = -1; s_pop_tw[i] = 0; }
    s_tint = 0; s_tint_level = 0;
    s_warned[0] = s_warned[1] = 0;
    s_marks = 0;
    g_show_anchor_hook = 0;
}

const ShowActorChan *show_actor_chan(int actor)
{
    static const ShowActorChan none = { -1, -1, -1, 0 };
    return (actor >= 0 && actor < BEAT_ACTORS) ? &s_chan[actor] : &none;
}

int show_pop_age(int obj, int *twt)
{
    if (obj < 0 || obj >= POP_MAX || s_pop_t[obj] < 0) return -1;
    if (twt) *twt = s_pop_tw[obj];
    return g_show_shot.t - s_pop_t[obj];
}

static void warn_once(int op, const char *what)
{
    char buf[64];
    int w = (op >> 5) & 1, b = op & 31;
    if (s_warned[w] & (1u << b)) return;
    s_warned[w] |= 1u << b;
    snprintf(buf, sizeof buf, "%s (op %d) not handled by any linked module", what, op);
    show_warn("SHW-EXT", buf);
}

static void iris(int out, int cx, int cy, int r, int heart, int hold)
{
    if (r <= 0) r = SHOW_CANVAS_H / 5;
    if (out) camfx_iris_out(cx, SHOW_Y(cy), r, heart, hold);
    else camfx_iris_in(cx, SHOW_Y(cy), r, heart, hold);
}

static void core_op(int op, const BeatEv *ev)
{
    const int32_t *a = show_args(ev->b);
    switch (op) {
    case SOP_NOP: break;
    case SOP_PLANE_SHOW: show_plane_set_visible(a[0], 1); break;
    case SOP_PLANE_HIDE: show_plane_set_visible(a[0], 0); break;
    case SOP_MARK: s_marks ^= 1u << (a[0] & 31); break;
    case SOP_FADE_TO: camfx_fade_to(a[0], a[1]); break;
    case SOP_PAL_TO: camfx_pal_lerp_to(a[0], a[1], a[2]); break;
    case SOP_IRIS: iris(a[0], a[1], a[2], a[3], a[4], a[5]); break;
    default: warn_once(BOP_SHOT, "show op"); break;
    }
}

void show_ext_event(const BeatEv *ev)
{
    int i, n = show_module_count(), op = show_op_of(ev);
    if (!ev) return;
    for (i = 0; i < n; i++) {
        const ShowModule *m = show_module(i);
        if (m->ext && m->ext(ev)) return;
    }
    if (op >= 0) {
        if (op < SOP_TEXT_BASE) core_op(op, ev);
        else warn_once(BOP_SHOT, "show op");
        return;
    }
    switch (ev->op) {
    case BOP_IRIS_OUT: case BOP_IRIS_IN: {
        int cx = SHOW_CANVAS_W / 2, cy = SHOW_CANVAS_H / 2;
        if (g_show_anchor_hook) g_show_anchor_hook(ev->a, &cx, &cy);
        iris(ev->op == BOP_IRIS_OUT, cx, cy, ((ev->b >> 7) & 63) * 2, (ev->b >> 6) & 1, ev->b & 63);
        break;
    }
    case BOP_TINT:
        s_tint = (uint8_t)(ev->a > 0 && ev->a < TINT_COUNT ? ev->a : 0);
        s_tint_level = (uint8_t)DOOMO_CLAMP(ev->b, 0, 16);
        break;
    case BOP_POP:
        if (ev->a >= 0 && ev->a < POP_MAX) {
            s_pop_t[ev->a] = (int16_t)g_show_shot.t;
            s_pop_tw[ev->a] = (uint8_t)ev->b;
        }
        break;
    case BOP_EXPR:
        if (ev->a >= 0 && ev->a < BEAT_ACTORS) {
            s_chan[ev->a].expr = (int16_t)(ev->b & 0xFF);
            s_chan[ev->a].mouth = (int16_t)(((ev->b >> 8) & 0xFF) ? ((ev->b >> 8) & 0xFF) - 1 : -1);
            s_chan[ev->a].expr_t = (int16_t)g_show_shot.t;
        }
        break;
    default:
        warn_once(ev->op, "host kind");
        break;
    }
}

/* the tint, drawn by the core right before camfx_apply */
void show_ext_draw_tint(void);
void show_ext_draw_tint(void)
{
    static const uint8_t k_col[TINT_COUNT] = {
        0, PAL_UI_RED, PAL_UI_GOLD, PAL_RAMP(RAMP_GRASS, 4), PAL_UI_PURPLE,
        PAL_RAMP(RAMP_STEEL, 0)
    };
    if (s_tint && s_tint_level) plat_post_blend(k_col[s_tint], s_tint_level);
}

void show_ext_hash(ShowCrc *c)
{
    show_crc_add(c, s_chan, sizeof s_chan);
    show_crc_add(c, s_pop_t, sizeof s_pop_t);
    show_crc_add(c, &s_tint, 1);
    show_crc_add(c, &s_tint_level, 1);
    show_crc_add(c, &s_marks, sizeof s_marks);
}
