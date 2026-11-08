/* viewhold.c - the 3D view held under a card (S2-2; plat_ext.h plat_view_hold, owner: gba; docs/GBA.md
 * "Speed wave 2: S2-2").
 *
 * The Hell Breach and arena intro cards, pause menus, results and death screens, and the level select's preview
 * between two level changes show a 3D view that stands still, but the raycaster drew it again on every frame
 * (~80 ms of the cards' 85-106 ms). KEEP takes a copy of the picture right after plat_view3d drew it and PUT puts
 * it back on the next frames instead of the raycaster; the core draws the HUD and the card over it as before.
 *
 * Room without new RAM: the raycaster draws 2-pixel columns (r3d.c: 120 rays, planes and sprites in the same
 * pairs, the half-resolution view doubled by r3d_up2), so every halfword of the view holds one pixel twice and
 * the picture is 120 x 160 bytes at most. KEEP checks that it is (any halfword with two different bytes: no
 * copy) and packs one byte per halfword into VRAM that a frame without a 3D view does not use:
 *   - OBJ VRAM (16 KB at 0x06014000): the raycaster's texture cache while a view runs (r3d.c vtex), owner
 *     OBJV_HOLD here (postfx.c gba_objvram_claim): the raycaster's next claim refills its cache, obj.c keeps its
 *     squash and figure OBJs off while the hold claims it each frame (as for a 3D frame), and any other claim
 *     means the copy is lost (PUT says 0);
 *   - the two 2,560-byte Mode 4 page gaps (0x06009600, 0x06013600): the raycaster's colormap rows (rebuilt:
 *     r3d_cmrow[0] = 0), the overworld's tile slots, the house's roof tables and plat_strip's band: each of those
 *     drops the copy before it writes there (plat_view3d, plat_overworld, plat_room, PLAT_STRIP_SAVE), so the
 *     combat cards ask S2-1's modal hold for no toast band (ui_modal_no_band).
 * Rows go whole into the first area with room (a full-screen view: 136 + 21 + 21 rows of 120 B for 160).
 * PUT expands them back (ARM loops of the IWRAM overlay's UI group, ~4 ms for the full screen); a shaded PUT
 * (a card's full-screen dim) darkens the copy itself once, so later puts cost the same. The copy is valid for the
 * view rect the raycaster last drew and the key the core gave (a hash of the view, its billboards and the world's
 * tick), plus the two options the raycaster reads itself (FOV, Combat detail); a skipped tick leaves it alone.
 * RAM: 22 B of .bss; ~0.3 KB of the UI group (which has room), the rest ROM.
 */
#include "gba_plat.h"
#include "canvas_int.h"
#include "r3d_int.h"
#include "iwram_ovl.h"
#include "postfx.h"
#include "video_int.h"
#include "doomo/plat_ext.h"
#include "doomo/town.h"
#include "doomo/palette.h"

#define VH_OBJ   ((uint8_t *)0x06014000)
#define VH_GAP_A ((uint8_t *)0x06009600)
#define VH_GAP_B ((uint8_t *)0x06013600)

/* r3d.c: the rect the last drawn view covered, and the frame it was drawn in */
extern int16_t r3d_last_x, r3d_last_y, r3d_last_w, r3d_last_h;
extern uint32_t r3d_last_frame;

static uint32_t vh_key;
static uint8_t  vh_ok;                        /* a copy is held                                */
static uint8_t  vh_x, vh_y, vh_w, vh_h;       /* its rect on the screen (x, w even)            */
static uint8_t  vh_rows_obj, vh_rows_a;       /* rows in the OBJ area, in gap A (the rest: B)  */
static uint8_t  vh_fov, vh_fast;              /* the options the raycaster read                */
static int8_t   vh_level = -1;                /* the copy is darkened as by plat_shade_rect at this level */

#define VH_ARM IWRAM_UI_CODE __attribute__((optimize("O2", "no-tree-loop-distribute-patterns")))

/* pack n halfwords (each one pixel twice) of src into n bytes at dst (words: n rounded up to 4, the tail
 * zero); 0 when a halfword holds two different pixels */
VH_ARM int vh_pack_row(volatile uint32_t *dst, const volatile uint16_t *src, int n)
{
    int k;
    for (k = 0; k < n; k += 4) {
        uint32_t w = 0;
        int j;
        for (j = 0; j < 4; j++) {
            uint32_t h = k + j < n ? src[k + j] : 0;
            if ((h ^ (h >> 8)) & 0xFFu) return 0;
            w |= (h & 0xFFu) << (8 * j);
        }
        *dst++ = w;
    }
    return 1;
}

/* the reverse: n halfwords of dst from the packed bytes at src (each byte twice), for a row that is not word
 * aligned (the level select's preview) */
VH_ARM void vh_unpack_row(uint16_t *dst, const uint32_t *src, int n)
{
    for (; n >= 4; n -= 4) {
        uint32_t p = *src++;
        dst[0] = (uint16_t)((p & 0xFFu) * 0x101u);
        dst[1] = (uint16_t)(((p >> 8) & 0xFFu) * 0x101u);
        dst[2] = (uint16_t)(((p >> 16) & 0xFFu) * 0x101u);
        dst[3] = (uint16_t)((p >> 24) * 0x101u);
        dst += 4;
    }
    if (n > 0) {
        uint32_t p = *src;
        for (; n > 0; n--, p >>= 8) *dst++ = (uint16_t)((p & 0xFFu) * 0x101u);
    }
}

/* a word-aligned row, n a multiple of 8: each packed word [a b c e] becomes the
 * page words [a a b b] [c c e e] in eight ALU steps, two packed words a turn (half the time of the table) */
VH_ARM void vh_unpack_plain(uint32_t *d, const uint32_t *src, int n)
{
    const uint32_t m = 0x00FF00FFu;
    for (; n > 0; n -= 8) {
        uint32_t p = src[0], q = src[1], r, o0, o1, o2, o3;
        src += 2;
        r = m & (p >> 8);                              /* [b 0 e 0] */
        o0 = (p & 0xFFu) | r << 16;                    /* [a 0 b 0] */
        o1 = (r & ~0xFFu) | (m & (p >> 16));           /* [c 0 e 0] */
        r = m & (q >> 8);
        o2 = (q & 0xFFu) | r << 16;
        o3 = (r & ~0xFFu) | (m & (q >> 16));
        d[0] = o0 | o0 << 8;
        d[1] = o1 | o1 << 8;
        d[2] = o2 | o2 << 8;
        d[3] = o3 | o3 << 8;
        d += 4;
    }
}

/* where row r of the copy lives (stride bytes a row) */
static volatile uint32_t *vh_row(int r, int stride)
{
    if (r < vh_rows_obj) return (volatile uint32_t *)(void *)(VH_OBJ + r * stride);
    r -= vh_rows_obj;
    if (r < vh_rows_a) return (volatile uint32_t *)(void *)(VH_GAP_A + r * stride);
    return (volatile uint32_t *)(void *)(VH_GAP_B + (r - vh_rows_a) * stride);
}

void view_hold_drop(void)
{
    vh_ok = 0;
}

#if defined(ENGINE_TEST)
#define VH_OPT(o) 0                            /* (the engine test ROM has no options) */
#else
#define VH_OPT(o) ((uint8_t)opt_get(o))
#endif

static int vh_options_same(void)
{
    return vh_fov == VH_OPT(OPT_FOV) && vh_fast == VH_OPT(OPT_3D_FAST);
}

int plat_view_hold(int op, uint32_t key)
{
    int n, stride, r;
    if (op == PLAT_VIEW_KEEP) {
        int cap_obj, cap_g;
        /* only right after a view this frame drew into a Mode 4 page (not a skipped tick, a tile frame, a frame
         * built over several ticks) */
        if (gba_nodraw == 1) return 0;         /* a skipped tick drew nothing: an earlier copy stays */
        vh_ok = 0;
        if (gba_nodraw == GBA_NODRAW_CANVAS || hold_building() || r3d_last_frame != gba_frames) return 0;
        n = r3d_last_w / 2;
        stride = (n + 3) & ~3;
        cap_obj = 16384 / stride;
        cap_g = 2560 / stride;
        if (n <= 0 || r3d_last_h <= 0 || cap_obj + 2 * cap_g < r3d_last_h) return 0;
        vh_rows_obj = (uint8_t)DOOMO_MIN(cap_obj, r3d_last_h);
        vh_rows_a = (uint8_t)DOOMO_MIN(cap_g, r3d_last_h - vh_rows_obj);
        gba_objvram_claim(OBJV_HOLD);          /* the raycaster's texture cache goes (refilled on its next view) */
        r3d_cmrow[0] = 0;                      /* and its colormap rows in the gaps */
        iwram_ovl_use(OVL_UI);
        for (r = 0; r < r3d_last_h; r++) {
            const volatile uint16_t *src = gba_back + (r3d_last_y + r) * (SCREEN_W / 2) + r3d_last_x / 2;
            if (!vh_pack_row(vh_row(r, stride), src, n)) return 0;
        }
        vh_x = (uint8_t)r3d_last_x; vh_y = (uint8_t)r3d_last_y;
        vh_w = (uint8_t)(r3d_last_w / 2); vh_h = (uint8_t)r3d_last_h;   /* (w in halfwords: 240 does not fit) */
        vh_fov = VH_OPT(OPT_FOV);
        vh_fast = VH_OPT(OPT_3D_FAST);
        vh_key = key;
        vh_level = -1;
        vh_ok = 1;
        return 1;
    }
    if ((op & 0xFF) == PLAT_VIEW_PUT) {
        int level = (op >> 8) - 1;
        if (level > 14) level = -1;            /* (level 15 shades nothing, as plat_shade_rect) */
        /* a skipped tick draws nothing (its view may differ: SP8's catch-up ticks build no billboards): the copy
         * stays for the next drawn tick */
        if (gba_nodraw == 1) return vh_ok;
        if (!vh_ok || key != vh_key || !vh_options_same() || gba_objvram_owner() != OBJV_HOLD) return 0;
        /* (shaded) the darkening plat_shade_rect(0, 0, 240, 160, level) gives: only a full-screen view, and only
         * from a copy not darkened at another level */
        if (level >= 0 && (vh_x || vh_y || vh_w != SCREEN_W / 2 || vh_h != SCREEN_H)) return 0;
        if (level != vh_level && vh_level >= 0) return 0;
        if (gba_nodraw) {                      /* G5: like plat_view3d, the frame becomes a Mode 4 frame */
            cv_need_m4();
            if (gba_nodraw == 1) return 1;
            if (gba_nodraw == GBA_NODRAW_FIRST) gba_nodraw = 0;
        }
        if (hold_building()) return 0;
        gba_objvram_claim(OBJV_HOLD);          /* (keeps obj.c's OBJs off this frame, as a 3D view does) */
        n = vh_w;
        stride = (n + 3) & ~3;
        if (level != vh_level) {
            /* the first shaded put: darken the copy itself, once (the card's dim stays on while it is held) */
            uint32_t lut[64];
            const uint32_t *cm = (const uint32_t *)(const void *)doomo_colormap[level];
            int a = vh_rows_obj, b = DOOMO_MIN(vh_rows_a, vh_h - a), c = vh_h - a - b, i;
            for (i = 0; i < 64; i++) lut[i] = cm[i];
            vid_shade_words((uint32_t *)(void *)VH_OBJ, a * stride / 4, (const uint8_t *)lut);
            if (b > 0) vid_shade_words((uint32_t *)(void *)VH_GAP_A, b * stride / 4, (const uint8_t *)lut);
            if (c > 0) vid_shade_words((uint32_t *)(void *)VH_GAP_B, c * stride / 4, (const uint8_t *)lut);
            vh_level = (int8_t)level;
        }
        iwram_ovl_use(OVL_UI);
        for (r = 0; r < vh_h; r++) {
            uint16_t *dst = gba_back + (vh_y + r) * (SCREEN_W / 2) + vh_x / 2;
            if (!(vh_x & 3) && !(n & 7)) vh_unpack_plain((uint32_t *)dst, (const uint32_t *)vh_row(r, stride), n);
            else vh_unpack_row(dst, (const uint32_t *)vh_row(r, stride), n);
        }
        return 1;
    }
    vh_ok = 0;
    return 0;
}
