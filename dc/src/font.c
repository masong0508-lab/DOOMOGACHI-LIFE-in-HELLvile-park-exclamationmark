/* font.c - the 2D layer's glyph pages and the checker texture (DC3; owner:
 * dreamcast). Portable.
 *
 * The core font (font.h: 5x8 glyphs, 6 px advance) becomes two PAL8
 * textures drawn through the white palette bank times the text colour:
 *   glyph page    128x128, an 8x8 cell per BYTE VALUE 0..255, so plat_text
 *                 maps a byte straight to a cell. Bytes outside the font
 *                 (below 0x20, above FONT_LAST) hold the '?' glyph, as the
 *                 native runner draws them.
 *   outline page  128x256, an 8x12 cell per byte: the glyph stamped at the
 *                 eight 1-pixel neighbours and at (+1, +2), i.e. the ten-pass
 *                 outline of plat_text_outline (platform.h) minus the text
 *                 pass, with its origin at (-1, -1). Outlined text is one
 *                 quad per character for the outline and one for the glyph.
 * Bytes 0xA0..0xFF are the active language's glyph page (lang.h
 * DoomoLangInfo.page; L4's per-language pages). The pages are rebuilt when
 * the current language's page changes (dc2d_font_frame, once a frame).
 * Today every page reads the one table of font.h (page 0: the Spanish
 * letters at 0xA0..0xAF), exactly as the native runner does; glyph_cols()
 * is the single place a per-page charset plugs in.
 *
 * The checker: 8x8 texels, ink where (i + j) is even, drawn with the
 * texture repeating at one texel per logical pixel: plat_checker's exact
 * pattern, phase anchored at the rect's corner.
 */
#include <string.h>
#include "dc2d.h"
#include "doomo/font.h"
#include "doomo/lang.h"

#define GP_W 128
#define GP_H 128
#define OP_W 128
#define OP_H 256

static uint8_t glyph_pg[GP_W * GP_H], outl_pg[OP_W * OP_H], checker[8 * 8];
static uint8_t blank[256];
static int     t_glyph = -1, t_outl = -1, t_check = -1;
static int     cur_page = -1;

/* the 5 columns of byte `ch` in glyph page `page` (bit 0 = top row) */
static void glyph_cols(int page, unsigned ch, uint8_t out[5])
{
    int c;
    (void)page;                         /* one table today (font.h), see above */
    if (ch < FONT_FIRST || ch > FONT_LAST) ch = '?';
    for (c = 0; c < 5; c++) out[c] = doomo_font[ch - FONT_FIRST][c];
}

void dc2d_font_build(int page, uint8_t *glyph, uint8_t *outline)
{
    unsigned ch;
    memset(glyph, 0, GP_W * GP_H);
    memset(outline, 0, OP_W * OP_H);
    for (ch = 0; ch < 256; ch++) {
        uint8_t cols[5];
        int gx = (int)(ch & 15) * DC2D_GLYPH_CW, gy = (int)(ch >> 4) * DC2D_GLYPH_CH;
        int ox = (int)(ch & 15) * DC2D_OUTL_CW, oy = (int)(ch >> 4) * DC2D_OUTL_CH;
        int c, r, ink = 0;
        glyph_cols(page, ch, cols);
        for (c = 0; c < 5; c++)
            for (r = 0; r < 8; r++) {
                int dx, dy;
                if (!(cols[c] & (1 << r))) continue;
                ink = 1;
                glyph[(gy + r) * GP_W + gx + c] = 1;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        if (dx || dy) outline[(oy + r + dy + 1) * OP_W + ox + c + dx + 1] = 1;
                outline[(oy + r + 2 + 1) * OP_W + ox + c + 1 + 1] = 1;
            }
        blank[ch] = (uint8_t)!ink;
    }
}

static int lang_page(void)
{
    int l = lang_get();
    return (l >= 0 && l < LANG_COUNT) ? doomo_lang_info[l].page : 0;
}

void dc2d_font_init(void)
{
    int i, j;
    cur_page = lang_page();
    dc2d_font_build(cur_page, glyph_pg, outl_pg);
    for (j = 0; j < 8; j++)
        for (i = 0; i < 8; i++) checker[j * 8 + i] = (uint8_t)(((i + j) & 1) == 0);
    t_glyph = dc2d_tex_static(glyph_pg, GP_W, GP_H, GP_W, GP_H);
    t_outl  = dc2d_tex_static(outl_pg, OP_W, OP_H, OP_W, OP_H);
    t_check = dc2d_tex_static(checker, 8, 8, 8, 8);
    if (t_glyph < 0 || t_outl < 0 || t_check < 0)
        dc2d_be_log("DC3: font/checker textures did not fit in video RAM");
}

void dc2d_font_frame(void)
{
    int p = lang_page();
    if (p == cur_page) return;
    cur_page = p;
    dc2d_font_build(p, glyph_pg, outl_pg);
    dc2d_be_wait_idle();                /* the PVR may still read the old pages */
    dc2d_tex_restatic(t_glyph, glyph_pg, GP_W, GP_H);
    dc2d_tex_restatic(t_outl, outl_pg, OP_W, OP_H);
}

int dc2d_font_tex(void) { return t_glyph; }
int dc2d_font_outline_tex(void) { return t_outl; }
int dc2d_checker_tex(void) { return t_check; }
int dc2d_font_blank(unsigned ch) { return blank[ch & 255]; }
