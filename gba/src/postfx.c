/* postfx.c - the GBA comic post-FX (owner: S1; postfx.h, platform.h S0
 * block, docs/design/story/SPEC.md sec.7 S1, C7.6-C7.7).
 *
 * The plat_post_* calls only latch the frame's state; video_flip() applies
 * it in the vblank of the page flip, so the shake / zoom / flash appear on
 * exactly the frame that asked for them:
 *   offset + scale  BG2X/Y and BG2PA/PD (mode 4 is an affine bitmap BG);
 *                   the whole screen moves, HUD included (the PS1 keeps
 *                   its HUD steady with a second E5)
 *   blend           white: BLDY brighten, black: BLDY darken (including backdrop);
 *                   any other colour: a palette lerp toward it
 *   pal_lerp        palette lerp (camfx_pal_entry, one formula on every
 *                   platform) prepared in EWRAM, then copied to BG and
 *                   OBJ palette RAM during the same vblank as the flip
 *   iris            the core's software spans (plat_weak_fx.c)
 *
 * The prepared palette uses 512 bytes of EWRAM.
 */
#include "postfx.h"
#include "canvas_int.h"
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
    uint32_t pending;                     /* palette selected for this flip     */
} pf;

EWRAM_BSS static uint16_t pending_palette[256] ALIGN4;

/* ---- the plat_post_* hooks (camfx_apply) ------------------------------- */
void plat_post_offset(int dx, int dy)
{
    if (gba_nodraw == 1) return;
    pf.dx = (int16_t)DOOMO_CLAMP(dx, -64, 64);
    pf.dy = (int16_t)DOOMO_CLAMP(dy, -64, 64);
    pf.set |= PF_OFFSET;
}

void plat_post_scale(int sx_q8, int sy_q8, int cx, int cy)
{
    int s = (sx_q8 + sy_q8) / 2;              /* BG2 zooms uniformly */
    if (gba_nodraw == 1) return;
    if (s == 256 || s <= 0) return;
    cv_scale();                               /* G5: the zoom needs a bitmap */
    if (gba_nodraw == 1) return;              /* (a dropped tile frame) */
    pf.s = (int16_t)DOOMO_CLAMP(s, 64, 1024);
    pf.cx = (int16_t)cx;
    pf.cy = (int16_t)cy;
    pf.set |= PF_SCALE;
}

void plat_post_blend(uint8_t color, int level16)
{
    if (gba_nodraw == 1 || level16 <= 0) return;
    pf.blend_c = color;
    pf.blend_l = (uint8_t)DOOMO_MIN(level16, 16);
    pf.set |= PF_BLEND;
}

void plat_pal_lerp(int pal_id, int level16)
{
    if (gba_nodraw == 1 || pal_id <= PAL_LERP_NONE || pal_id >= PAL_LERP_COUNT || level16 <= 0) return;
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
static int is_white(uint8_t c) { return doomo_palette[c] == 0x7FFF; }
static int is_black(uint8_t c) { return (doomo_palette[c] & 0x7FFF) == 0; }

/* A color-only blend can reuse the 32 possible results per channel.
 * Use the same truncation toward zero as camfx_pal_entry. */
static void prepare_blend(uint8_t color, int level)
{
    uint16_t r[32], g[32], b[32];
    uint16_t target = doomo_palette[color];
    int tr = target & 31, tg = (target >> 5) & 31, tb = (target >> 10) & 31;
    int i;
    for (i = 0; i < 32; i++) {
        r[i] = (uint16_t)(i + (tr - i) * level / 16);
        g[i] = (uint16_t)((i + (tg - i) * level / 16) << 5);
        b[i] = (uint16_t)((i + (tb - i) * level / 16) << 10);
    }
    for (i = 0; i < 256; i++) {
        uint16_t p = doomo_palette[i];
        pending_palette[i] = r[p & 31] | g[(p >> 5) & 31] | b[(p >> 10) & 31];
    }
}

void postfx_pre_flip(void)
{
    int id = (pf.set & PF_PAL) ? pf.pal_id : 0;
    int level = id ? pf.pal_l : 0;
    int blend_level = 0;
    uint8_t color = 0;

    if ((pf.set & PF_BLEND) && !is_white(pf.blend_c) && !is_black(pf.blend_c)) {
        color = pf.blend_c;
        blend_level = pf.blend_l;
    }
    pf.pending = 0;
    if ((id || blend_level) && camfx_pal_entry) {
        int i;
        pf.pending = 0x80000000u | (uint32_t)id | ((uint32_t)level << 8) |
                     ((uint32_t)color << 16) | ((uint32_t)blend_level << 24);
        if (pf.pending != pf.live) {
            if (!id) {
                prepare_blend(color, blend_level);
            } else {
                for (i = 0; i < 256; i++)
                    pending_palette[i] = camfx_pal_entry(i, id, level, color, blend_level);
            }
        }
    }
    obj_pre_flip();
}

int postfx_vblank_lines(void)
{
    /* Reserve time for both palette banks and the display registers and
     * OAM that must change in the same blank. Master restore costs two
     * transfers as well. */
    return pf.pending != pf.live ? 8 : 0;
}

void postfx_vblank(int tiles)
{
    int bldcnt = 0, bldy = 0;
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
            /* Apply the same brightness to backdrop pixels exposed by shake or zoom. */
            bldcnt = 0x0004 | 0x0010 | 0x0020 | (is_white(pf.blend_c) ? 0x0080 : 0x00C0);
            if (tiles) bldcnt |= 0x0001;         /* G5: + the canvas (BG0) */
            bldy = pf.blend_l;
        }
    }
    ld_flip(&bldcnt, &bldy);                    /* SP5 G-LD: a loading fade lifts (loadfade.c) */
    REG_BLDCNT = (uint16_t)bldcnt;
    REG_BLDY = (uint16_t)bldy;
    if (pf.pending != pf.live) {
        if (pf.pending) {
            dma3_copy32((void *)BG_PALETTE, pending_palette, 128);
            dma3_copy32((void *)OBJ_PALETTE, pending_palette, 128);
        } else {
            video_load_palette();
        }
        pf.live = pf.pending;
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
    /* (a held view counts as one, and G2-CO's kept rows: obj.c keeps its OBJs off, the billboards start again) */
    if (who == OBJV_R3D || who == OBJV_HOLD || who == OBJV_KEEP) objv_r3d_frame = gba_frames;
    objv_owner = (uint8_t)who;
    return prev;
}

uint32_t gba_objvram_r3d_frame(void) { return objv_r3d_frame; }
int gba_objvram_owner(void) { return objv_owner; }
