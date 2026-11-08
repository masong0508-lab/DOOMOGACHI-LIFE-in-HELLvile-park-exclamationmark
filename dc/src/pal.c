/* pal.c - PVR palette banks and the shade factors of the 2D layer (DC3;
 * owner: dreamcast). Portable: the backend gets the banks through
 * dc2d_be_pal().
 *
 * The PVR has 1024 palette entries: four banks of 256 for PAL8 textures.
 *   bank 0  the master palette (doomo_palette, BGR555) as ARGB1555, which
 *           holds every colour exactly; index 0 is transparent
 *   bank 1  all white, index 0 transparent: TINT and DARK prims multiply
 *           it by the header colour (silhouettes, glyphs, the checker,
 *           shadows), so one texture serves every colour
 *   bank 2, bank 3  the master palette lerped toward a camfx palette
 *           (plat_pal_lerp, camfx_pal_entry: the formula every port uses).
 *           Frames alternate between the two, so the frame the PVR is
 *           still drawing keeps its palette.
 * ARGB1555 (not 8888) keeps the PVR's texture fetch at full speed and
 * allows bilinear filtering for a later "Smooth" display mode (X3).
 *
 * Shading. plat_shade_rect darkens through doomo_colormap[level], a
 * palette remap (each colour walks down its own shade family). The PVR
 * cannot remap the frame, so a MUL prim multiplies it by a colour: per
 * level, the per-channel factor that fits the colormap best (least
 * squares over the 255 colours). The IMG_SHADOW drop shadow (colormap
 * level 5 in the native runner) is a DARK prim: one grey factor, because
 * a texture's transparent texels must leave the frame alone, which the
 * multiply blend cannot do and alpha blending can.
 */
#include "dc2d.h"
#include "doomo/palette.h"
#include "doomo/camfx.h"

static uint16_t bank0[256], bank1[256], lerpb[2][256];
static uint32_t shade_argb[DOOMO_LIGHT_LEVELS];
static uint32_t shadow_argb;

static int x5to8(int v) { return (v << 3) | (v >> 2); }

static uint16_t bgr555_to_1555(uint16_t c)
{
    return (uint16_t)(0x8000u | ((c & 31u) << 10) | (c & (31u << 5)) | ((c >> 10) & 31u));
}

static uint32_t argb_of_1555(uint16_t c)
{
    uint32_t a = (c & 0x8000u) ? 0xFFu : 0u;
    return (a << 24) | ((uint32_t)x5to8((c >> 10) & 31) << 16) |
           ((uint32_t)x5to8((c >> 5) & 31) << 8) | (uint32_t)x5to8(c & 31);
}

static int chan(uint8_t i, int c)       /* 8-bit channel c (0 r, 1 g, 2 b) of palette i */
{
    uint16_t v = doomo_palette[i];
    return x5to8(c == 0 ? (v & 31) : c == 1 ? ((v >> 5) & 31) : ((v >> 10) & 31));
}

/* least-squares factor (0..255) of out = f * in over the palette, channel
 * c (or every channel for c < 0) */
static uint32_t fit(int level, int c)
{
    int64_t num = 0, den = 0;
    int i, k;
    for (i = 1; i < 256; i++) {
        uint8_t o = doomo_colormap[level][i];
        for (k = 0; k < 3; k++) {
            int a, b;
            if (c >= 0 && k != c) continue;
            a = chan((uint8_t)i, k);
            b = chan(o, k);
            num += (int64_t)a * b;
            den += (int64_t)a * a;
        }
    }
    if (den <= 0) return 255;
    k = (int)((num * 255 + den / 2) / den);
    return (uint32_t)(k < 0 ? 0 : k > 255 ? 255 : k);
}

void dc2d_pal_init(void)
{
    int i, l;
    for (i = 0; i < 256; i++) {
        bank0[i] = i ? bgr555_to_1555(doomo_palette[i]) : 0;
        bank1[i] = i ? 0xFFFFu : 0;
        lerpb[0][i] = lerpb[1][i] = bank0[i];
    }
    for (l = 0; l < DOOMO_LIGHT_LEVELS; l++)
        shade_argb[l] = 0xFF000000u | (fit(l, 0) << 16) | (fit(l, 1) << 8) | fit(l, 2);
    shadow_argb = (255u - fit(5, -1)) << 24;
    dc2d_be_pal(0, bank0);
    dc2d_be_pal(1, bank1);
    dc2d_be_pal(2, lerpb[0]);
    dc2d_be_pal(3, lerpb[1]);
}

static const uint16_t *bank_tab(int bank)
{
    return bank == 1 ? bank1 : bank == 2 ? lerpb[0] : bank == 3 ? lerpb[1] : bank0;
}

uint16_t dc2d_pal_1555(int bank, uint8_t index) { return bank_tab(bank)[index]; }

uint32_t dc2d_pal_argb(int bank, uint8_t index)
{
    /* a palette colour drawn as a rect is opaque, index 0 included
     * (PAL_CLEAR draws black, as on every port) */
    return 0xFF000000u | argb_of_1555(bank_tab(bank)[index]);
}

void dc2d_pal_lerp_build(int bank, int pal_id, int level16)
{
    uint16_t *t = bank == 3 ? lerpb[1] : lerpb[0];
    int i;
    t[0] = 0;
    for (i = 1; i < 256; i++)
        t[i] = bgr555_to_1555(camfx_pal_entry(i, pal_id, level16, 0, 0));
    dc2d_be_pal(bank == 3 ? 3 : 2, t);
}

uint32_t dc2d_shade_argb(int level)
{
    if (level < 0) level = 0;
    if (level >= DOOMO_LIGHT_LEVELS) level = DOOMO_LIGHT_LEVELS - 1;
    return shade_argb[level];
}

uint32_t dc2d_shadow_argb(void) { return shadow_argb; }

uint32_t dc2d_prim_argb(const DcFrame *f, const DcPrim *p)
{
    switch (p->mode) {
    case DC2D_SOLID:
        return dc2d_pal_argb((p->flags & DC2D_PF_LERP) ? f->lerp_bank : 0, p->color);
    case DC2D_TINT:
        return dc2d_pal_argb((p->flags & DC2D_PF_LERP) ? f->lerp_bank : 0, p->color);
    case DC2D_IMAGE:
        return 0xFFFFFFFFu;
    default:
        return p->argb;
    }
}

int dc2d_same_header(const DcFrame *f, const DcPrim *a, const DcPrim *b)
{
    int sa = dc2d_is_sprite(a), sb = dc2d_is_sprite(b);
    if (a->mode != b->mode || a->tex != b->tex || sa != sb || dc2d_bank(f, a) != dc2d_bank(f, b))
        return 0;
    /* a sprite's colour is in its header; a polygon's is in its vertices */
    return !sa || a->mode == DC2D_IMAGE || dc2d_prim_argb(f, a) == dc2d_prim_argb(f, b);
}
