/* show_host_mod.c - the stage-kind hosts plugged into the player (owner:
 * show, ENGINE lane 3; show_api.h, spec 8.3).
 *
 *   show_mod_view3d  the plane kinds SHWP_OVERWORLD, SHWP_ROOM, SHWP_VIEW3D
 *                    (their payload is a show_hosts.h record) and the tick
 *   show_mod_hud     the first-person HUD on SHOW_L_HUD (over camfx_apply,
 *                    so a fade or a shake never moves it)
 *
 * One host plane per shot (a shot is one place). The plane's prep binds the
 * record (show_host_prep: it loads the map, makes the NPC looks and warms
 * VRAM by drawing frame 0 once and discarding the list, which is why it
 * must run before anything of the frame is drawn: the core's plane prep
 * does). Ticks run on every shot frame t >= 0 (pre-roll frames are before
 * the shot's own clock and are skipped: a host record is shot-local).
 */
#include <stdio.h>
#include "show/show_api.h"
#include "show/show_hosts.h"

static int s_bound;

static void h_reset(void)
{
    show_host_end();
    s_bound = 0;
}

static void h_plane_prep(const ShwPlane *p, const void *payload)
{
    char msg[96];
    if (s_bound) {
        show_warn("PLAN-HOST", "a second host plane in one shot is ignored");
        return;
    }
    if (show_host_prep(payload, (uint32_t)p->bytes - (uint32_t)sizeof(ShwPlane)) < 0) {
        snprintf(msg, sizeof(msg), "host plane %d: %s", p->id, show_host_error());
        show_error("SHW-HOST", msg);
        return;
    }
    s_bound = 1;
}

static void h_plane_draw(const ShwPlane *p, const void *payload, int t)
{
    (void)p; (void)payload;
    if (!s_bound || t < 0) return;
    show_host_draw(t);
    show_stat_add(SHOW_STAT_SPRITES, g_show_host_stats.sprites);
}

static void h_tick(int t)
{
    if (s_bound && t >= 0) show_host_tick(t);
}

static void h_hash(ShowCrc *c)
{
    int32_t st[4];
    st[0] = s_bound;
    st[1] = show_host_kind();
    st[2] = show_gore_effective();
    st[3] = show_gore_forced_frames();
    show_crc_add(c, st, sizeof(st));
}

static const ShowPlaneKind k_kinds[] = {
    { SHWP_OVERWORLD, "overworld", h_plane_prep, h_plane_draw },
    { SHWP_ROOM,      "room",      h_plane_prep, h_plane_draw },
    { SHWP_VIEW3D,    "view3d",    h_plane_prep, h_plane_draw },
    { 0, 0, 0, 0 }
};

const ShowModule show_mod_view3d = { "hosts", SHOW_API_VERSION, 0,
    h_reset, NULL, NULL, h_tick, NULL, h_hash, k_kinds };

static void hud_draw(int layer, int t)
{
    if (layer == SHOW_L_HUD && s_bound && t >= 0) show_host_draw_hud(t);
}

const ShowModule show_mod_hud = { "hud", SHOW_API_VERSION, 0,
    NULL, NULL, NULL, NULL, hud_draw, NULL, NULL };
