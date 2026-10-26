/* show_platepool.c - a pinned VRAM pool for gridded plate cells (owner:
 * ENGINE lane 1; spec 8.2, 8.3).
 *
 * A big plate (the bandstand: > 64 KB, drawn as 126x120 grid cells) used to
 * go through vram.c's image cache with everything else: its cells took most
 * of the cache's six pages every frame, so a cast member's new pose/expression
 * image found no page free of this frame's use (img_alloc -1) and the actor
 * silently stopped drawing a few events into the shot.
 *
 * Staged (host-less) shots never use vram.c's world-texture region (x 320..960,
 * y 0..255: five 256x256 8bpp pages = 20 cells of 128x128), so grid cells are
 * pinned there instead, one cell per slot, uploaded once (synchronously, so
 * the shared 56 KB display list stays for drawing) and kept for the shot.
 * The image cache is left wholly to actors, props and ambient flips.
 * A shot with a host (it owns the region) or with more than 20 visible-ever
 * cells falls back to the image cache for the rest and logs it once:
 * plate detail degrades first, actors are never starved by it.
 */
#include "show_core.h"
#include "show/show_hosts.h"

#if defined(DOOMO_PLATFORM_PS1) && !defined(SHOW_L2_HOST)
#include <psxgpu.h>
#include "ps1.h"

#define POOL_N (VR_WTEX_PAGES * 4)
static uint32_t s_uid[POOL_N];
static int s_n, s_warned;
static const uint8_t s_zero[128];

void show_platepool_reset(void) { s_n = 0; s_warned = 0; }

/* zero the texture VRAM a shot can sample (purity, spec 8.2): the pool's
 * region, the image cache's pages and the framebuffers. A quad at a fractional scale samples
 * one texel past its tile's edge, and what an earlier shot of the batch left
 * there drew 1 px seam/edge pixels that a lone render never showed. */
void show_vram_scrub(void)
{
    int x, sync = g_show_upload_sync;
    g_show_upload_sync = 1;
    for (x = VR_WTEX_X; x < 960; x += 64)
        gfx_upload_strided(x, 0, 64, 512, s_zero, 0);
    gfx_upload_strided(960, VR_IMG_Y, 64, VR_FONTD_Y - VR_IMG_Y, s_zero, 0);
    /* both framebuffers too (to boot's black): an edge column a camera move
     * leaves undrawn showed the previous shot's last frame in a batch */
    for (x = 0; x < SCR_W; x += 64)
        gfx_upload_strided(x, 0, 64, 2 * SCR_H, s_zero, 0);
    g_show_upload_sync = sync;
}

static int slot_for(const DoomoImage *img)
{
    int i, s, sync;
    for (i = 0; i < s_n; i++)
        if (s_uid[i] == img->uid) return i;
    if (s_n >= POOL_N || img->w > 128 || img->h > 128) {
        if (!s_warned) { show_warn("PLATE-POOL", "plate cells exceed the pinned pool: rest share the image cache"); s_warned = 1; }
        return -1;
    }
    s = s_n;
    sync = g_show_upload_sync;
    g_show_upload_sync = 1;                         /* LoadImage now, not the frame's list */
    if (!gfx_upload_strided(VR_WTEX_X + (s >> 2) * 128 + (s & 1) * 64, ((s >> 1) & 1) * 128,
                            (img->w + 1) >> 1, img->h, packart_resolve(img->pix), img->w)) {
        g_show_upload_sync = sync;
        return -1;
    }
    g_show_upload_sync = sync;
    s_uid[s_n++] = img->uid;
    return s;
}

int show_platepool_draw(const DoomoImage *img, int x0, int y0, int x1, int y1)
{
    POLY_FT4 *p;
    int s, u, v, w, h;
    if (!img || !img->pix || !img->uid || show_host_kind() != SHOST_NONE) return 0;
    y0 = SHOW_Y(y0); y1 = SHOW_Y(y1);
    if (x1 <= x0 || y1 <= y0 || x0 >= SCR_W || x1 <= 0 || y0 >= SCR_H || y1 <= 0) return 1;
    if (x1 - x0 > 1023 || y1 - y0 > 511) return 0;
    s = slot_for(img);
    if (s < 0) return 0;
    p = (POLY_FT4 *)gfx_alloc(sizeof(POLY_FT4));
    if (!p) return 1;
    u = (s & 1) * 128; v = ((s >> 1) & 1) * 128;
    w = img->w; h = img->h;
    setPolyFT4(p);
    setShadeTex(p, 1);
    setXY4(p, x0, y0, x1, y0, x0, y1, x1, y1);
    setUV4(p, u, v, u + w, v, u, v + h, u + w, v + h);
    p->tpage = getTPage(1, 0, VR_WTEX_X + (s >> 2) * 128, 0);
    p->clut = getClut(VR_CLUT_MASTER_X, VR_CLUT_MASTER_Y);
    gfx_chain(p);
    g_tpage_cur = p->tpage;
    show_stat_add(SHOW_STAT_SPRITES, 1);
    return 1;
}
#else
void show_platepool_reset(void) {}
void show_vram_scrub(void) {}
int show_platepool_draw(const DoomoImage *img, int x0, int y0, int x1, int y1)
{
    (void)img; (void)x0; (void)y0; (void)x1; (void)y1;
    return 0;
}
#endif
