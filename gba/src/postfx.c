/* postfx.c - the GBA comic post-FX (owner: S1; postfx.h, platform.h S0
 * block, docs/design/story/SPEC.md sec.7 S1, C7.6-C7.7).
 *
 * The plat_post_* calls only latch the frame's state; video_flip() applies
 * it in the vblank of the page flip, so the shake / zoom / flash appear on
 * exactly the frame that asked for them:
 *   offset + scale  BG2X/Y and BG2PA/PD (mode 4 is an affine bitmap BG);
 *                   the whole screen moves, HUD included (the PS1 keeps
 *                   its HUD steady with a second E5)
 *   blend           white: BLDY brighten, black: BLDY darken (BG2 + OBJ);
 *                   any other colour: a palette lerp toward it
 *   pal_lerp        palette RAM lerp (camfx_pal_entry, one formula on
 *                   every platform), written straight into BG and OBJ
 *                   palette RAM in the vblank (about 30k cycles of the
 *                   80k vblank; no 512 B buffer), restored with
 *                   video_load_palette() on the first frame without it
 *   iris            the core's software spans (plat_weak_fx.c)
 *
 * RAM: 36 B of .bss and .data (+ obj.c).
 */
#include "postfx.h"
#include "doomo/camfx.h"
#include "doomo/palette.h"

/* Engine-test builds without the core (NOCORE=1) have no camfx.c. */
extern uint16_t camfx_pal_entry(int i, int pal_id, int pal_level, uint8_t color, int level16)
    __attribute__((weak));
void video_load_palette(void);

enum { PF_OFFSET = 1, PF_SCALE = 2, PF_BLEND = 4, PF_PAL = 8 };

static struct {
    int16_t dx, dy, s, cx, cy;            /* this frame's transform            */
    int16_t bx0, by0, bx1, by1;           /* 2D bbox accumulator               */
    uint8_t set;                          /* PF_* latched this frame           */
    uint8_t blend_c, blend_l, pal_id, pal_l;
    uint32_t live;                        /* what palette RAM holds, 0 = master */
} pf;

/* ---- the plat_post_* hooks (camfx_apply) ------------------------------- */
void plat_post_offset(int dx, int dy)
{
    if (gba_nodraw) return;
    pf.dx = (int16_t)DOOMO_CLAMP(dx, -64, 64);
    pf.dy = (int16_t)DOOMO_CLAMP(dy, -64, 64);
    pf.set |= PF_OFFSET;
}

void plat_post_scale(int sx_q8, int sy_q8, int cx, int cy)
{
    int s = (sx_q8 + sy_q8) / 2;              /* BG2 zooms uniformly */
    if (gba_nodraw) return;
    if (s == 256 || s <= 0) return;
    pf.s = (int16_t)DOOMO_CLAMP(s, 64, 1024);
    pf.cx = (int16_t)cx;
    pf.cy = (int16_t)cy;
    pf.set |= PF_SCALE;
}

void plat_post_blend(uint8_t color, int level16)
{
    if (gba_nodraw || level16 <= 0) return;
    pf.blend_c = color;
    pf.blend_l = (uint8_t)DOOMO_MIN(level16, 16);
    pf.set |= PF_BLEND;
}

void plat_pal_lerp(int pal_id, int level16)
{
    if (gba_nodraw || pal_id <= PAL_LERP_NONE || pal_id >= PAL_LERP_COUNT || level16 <= 0) return;
    pf.pal_id = (uint8_t)pal_id;
    pf.pal_l = (uint8_t)DOOMO_MIN(level16, 16);
    pf.set |= PF_PAL;
}

int postfx_xform(int *dx, int *dy, int *s_q8, int *cx, int *cy)
{
    *dx = (pf.set & PF_OFFSET) ? pf.dx : 0;
    *dy = (pf.set & PF_OFFSET) ? pf.dy : 0;
    *s_q8 = (pf.set & PF_SCALE) ? pf.s : 256;
    *cx = pf.cx;
    *cy = pf.cy;
    return (pf.set & (PF_OFFSET | PF_SCALE)) != 0;
}

/* ---- the 2D bbox accumulator --------------------------------------------- */
void postfx_bbox_add(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (pf.bx1 <= pf.bx0) {
        pf.bx0 = (int16_t)x; pf.by0 = (int16_t)y;
        pf.bx1 = (int16_t)(x + w); pf.by1 = (int16_t)(y + h);
        return;
    }
    if (x < pf.bx0) pf.bx0 = (int16_t)x;
    if (y < pf.by0) pf.by0 = (int16_t)y;
    if (x + w > pf.bx1) pf.bx1 = (int16_t)(x + w);
    if (y + h > pf.by1) pf.by1 = (int16_t)(y + h);
}

int postfx_bbox(int *x0, int *y0, int *x1, int *y1)
{
    *x0 = pf.bx0; *y0 = pf.by0; *x1 = pf.bx1; *y1 = pf.by1;
    return pf.bx1 > pf.bx0;
}

/* ---- flip ------------------------------------------------------------------ */
void postfx_pre_flip(void)
{
    obj_pre_flip();
}

static int is_white(uint8_t c) { return doomo_palette[c] == 0x7FFF; }
static int is_black(uint8_t c) { return (doomo_palette[c] & 0x7FFF) == 0; }

static void palette_write(int id, int l, uint8_t c, int cl)
{
    int i;
    vu16 *bg = BG_PALETTE, *ob = OBJ_PALETTE;
    for (i = 0; i < 256; i++) {
        uint16_t v = camfx_pal_entry(i, id, l, c, cl);
        bg[i] = v;
        ob[i] = v;
    }
}

void postfx_vblank(void)
{
    int pid = 0, pl = 0, cl = 0, bldcnt = 0, bldy = 0;
    uint8_t c = 0;
    /* BG2 affine: q = c + (p - d - c) / s, PA = PD = 65536 / s */
    if (pf.set & (PF_OFFSET | PF_SCALE)) {
        int s = (pf.set & PF_SCALE) ? pf.s : 256;
        int32_t pa = (int32_t)(65536 / s);
        int dx = (pf.set & PF_OFFSET) ? pf.dx : 0, dy = (pf.set & PF_OFFSET) ? pf.dy : 0;
        int cx = (pf.set & PF_SCALE) ? pf.cx : 0, cy = (pf.set & PF_SCALE) ? pf.cy : 0;
        REG_BG2PA = (uint16_t)pa; REG_BG2PB = 0; REG_BG2PC = 0; REG_BG2PD = (uint16_t)pa;
        REG_BG2X = (uint32_t)((int32_t)cx * 256 - (int32_t)(dx + cx) * pa);
        REG_BG2Y = (uint32_t)((int32_t)cy * 256 - (int32_t)(dy + cy) * pa);
    } else {
        REG_BG2PA = 0x100; REG_BG2PB = 0; REG_BG2PC = 0; REG_BG2PD = 0x100;
        REG_BG2X = 0; REG_BG2Y = 0;
    }
    /* blend: BLDY for white / black, the palette for any other colour */
    if (pf.set & PF_BLEND) {
        if (is_white(pf.blend_c) || is_black(pf.blend_c)) {
            bldcnt = 0x0004 | 0x0010 | (is_white(pf.blend_c) ? 0x0080 : 0x00C0);   /* BG2 + OBJ */
            bldy = pf.blend_l;
        } else {
            c = pf.blend_c;
            cl = pf.blend_l;
        }
    }
    REG_BLDCNT = (uint16_t)bldcnt;
    REG_BLDY = (uint16_t)bldy;
    if (pf.set & PF_PAL) { pid = pf.pal_id; pl = pf.pal_l; }
    if ((pid || cl) && camfx_pal_entry) {
        uint32_t sig = 0x80000000u | (uint32_t)pid | ((uint32_t)pl << 8) | ((uint32_t)c << 16) |
                       ((uint32_t)cl << 24);
        if (pf.live != sig) {
            palette_write(pid, pl, c, cl);
            pf.live = sig;
        }
    } else if (pf.live) {
        video_load_palette();
        pf.live = 0;
    }
    obj_vblank();
    /* the next frame starts clean */
    pf.set = 0;
    pf.bx0 = pf.by0 = pf.bx1 = pf.by1 = 0;
}

/* ---- OBJ VRAM: r3d's texture cache or obj.c's squash tiles (C7.4) ---- */
static uint8_t objv_owner = OBJV_R3D;
static uint32_t objv_r3d_frame = 0xFFFFFF00u;

int gba_objvram_claim(int who)
{
    int prev = objv_owner;
    if (who == OBJV_R3D) objv_r3d_frame = gba_frames;
    objv_owner = (uint8_t)who;
    return prev;
}

uint32_t gba_objvram_r3d_frame(void) { return objv_r3d_frame; }
int gba_objvram_owner(void) { return objv_owner; }
