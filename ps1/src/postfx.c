/* postfx.c - the PS1 comic post-FX (owner: S1; postfx.h, platform.h S0
 * block, docs/design/story/SPEC.md sec.7 S1). camfx_apply() calls these
 * while the frame's display list is built, after the world and before a
 * steady HUD.
 *
 * RAM: 12 B of .bss. Resident (ps1/src is outside the overlay groups).
 */
#pragma GCC optimize("Os")
#include <psxgpu.h>
#include "ps1.h"
#include "postfx.h"
#include "doomo/camfx.h"

/* Engine-test builds may link without the core's camfx.c. */
extern uint16_t camfx_pal_entry(int i, int pal_id, int pal_level, uint8_t color, int level16)
    __attribute__((weak));

static struct {
    int16_t zoom_next, zoom_cur;     /* Q8.8 GTE H multiplier                 */
    uint8_t pal_set;                 /* plat_pal_lerp ran this frame          */
    uint8_t pal_id, pal_l, pal_live; /* what g_rgb / the master CLUT hold     */
} px = { 256, 256, 0, 0, 0, 0 };

void plat_post_offset(int dx, int dy)
{
    if (dx || dy) gfx_post_offset(DOOMO_CLAMP(dx, -96, 96), DOOMO_CLAMP(dy, -96, 96));
}

void plat_post_scale(int sx_q8, int sy_q8, int cx, int cy)
{
    int s = (sx_q8 + sy_q8) / 2;
    (void)cx; (void)cy;                       /* the GTE zooms about the view centre */
    px.zoom_next = (int16_t)DOOMO_CLAMP(s, 128, 1024);
}

int postfx_zoom_q8(void) { return px.zoom_cur; }

/* one full-screen TILE; abr < 0 = opaque */
static void blend_tile(int abr, uint32_t rgb)
{
    TILE *t;
    if (abr >= 0) gfx_tpage(getTPage(0, abr, 0, 0));
    t = (TILE *)gfx_alloc(sizeof(TILE));
    if (!t) return;
    setTile(t);
    setXY0(t, 0, 0);
    setWH(t, SCR_W, SCR_H);
    setRGB0(t, rgb & 0xff, (rgb >> 8) & 0xff, (rgb >> 16) & 0xff);
    if (abr >= 0) setSemiTrans(t, 1);
    gfx_chain(t);
}

static uint32_t rgb_scale(uint32_t rgb, int l16)
{
    uint32_t r = (rgb & 0xff) * (uint32_t)l16 / 16u, g = ((rgb >> 8) & 0xff) * (uint32_t)l16 / 16u;
    uint32_t b = ((rgb >> 16) & 0xff) * (uint32_t)l16 / 16u;
    return r | (g << 8) | (b << 16);
}

void plat_post_blend(uint8_t color, int level16)
{
    uint16_t c = core_palette()[color] & 0x7fff;
    uint32_t rgb = g_rgb[color];
    if (level16 <= 0) return;
    if (level16 >= 16) { blend_tile(-1, rgb); return; }
    if (c == 0x7fff) {
        blend_tile(1, rgb_scale(0xffffff, level16));            /* B + F      */
    } else if (c == 0) {
        blend_tile(2, rgb_scale(0xffffff, level16));            /* B - F      */
    } else {                       /* B - l*255 + l*C: the lerp, near enough */
        blend_tile(2, rgb_scale(0xffffff, level16));
        blend_tile(1, rgb_scale(rgb, level16));
    }
    gfx_tpage(getTPage(0, 0, 0, 0));                            /* ABR back to 0 */
}

/* rebuild g_rgb / g_pal15 (gfx.c palette_tables' format) and queue the
 * master CLUT upload */
static void pal_apply(int id, int l)
{
    const uint16_t *pal = core_palette();
    int i;
    if (id && !camfx_pal_entry) return;
    for (i = 0; i < 256; i++) {
        uint16_t c = id ? camfx_pal_entry(i, id, l, 0, 0) : pal[i];
        int r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;
        r |= r >> 5; g |= g >> 5; b |= b >> 5;
        g_rgb[i] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
        g_pal15[i] = (uint16_t)(i == 0 ? 0 : ((c & 0x7fff) | 0x8000));
    }
    gfx_upload(VR_CLUT_MASTER_X, VR_CLUT_MASTER_Y, 256, 1, g_pal15);
}

void plat_pal_lerp(int pal_id, int level16)
{
    if (pal_id <= PAL_LERP_NONE || pal_id >= PAL_LERP_COUNT || level16 <= 0) return;
    level16 = DOOMO_MIN(level16, 16);
    px.pal_set = 1;
    if (px.pal_live && px.pal_id == pal_id && px.pal_l == level16) return;
    pal_apply(pal_id, level16);
    px.pal_live = 1;
    px.pal_id = (uint8_t)pal_id;
    px.pal_l = (uint8_t)level16;
}

void postfx_frame_begin(void)
{
    px.zoom_cur = px.zoom_next;
    px.zoom_next = 256;
    if (px.pal_live && !px.pal_set) {          /* the lerp ended: master palette */
        pal_apply(0, 0);
        px.pal_live = 0;
    }
    px.pal_set = 0;
}
