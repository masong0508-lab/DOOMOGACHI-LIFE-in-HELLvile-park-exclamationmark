/* tex_lv.c - the 7 shared level textures (LV7, ids 48..54 in textures.h).
 *
 *   steam_tile     pale aqua bath-house tile, dewy              (Steam Room Panic)
 *   pipe_steel     four horizontal steel pipes, flanges, rust   (Glum Pipes)
 *   office_carpet  grey-blue speckled carpet with a seam grid   (Department of Contracts)
 *   cubicle_fabric woven peach panel with a pin strip           (Department of Contracts)
 *   felt           green baize with a faint diamond quilt       (Bandstand, Boneyard Buffet)
 *   ledger_page    cream ruled paper, red margin, tick marks    (The Ledger)
 *   vault_metal    dark steel plates, bolts, an amber lamp      (Vault of Unfelt Things)
 *
 * Same rules as the other texture files: 64x64, seamless, integer only.
 * Nothing here is adult-only or gory.
 */
#include "art.h"

#define PX(x, y, c) img_put(im, (x), (y), (uint8_t)(c))
#define GX(x, y) img_get(im, (x), (y))

static void bolt(ArtImg *im, int x, int y)
{
    PX(x, y, RP(RAMP_STEEL, 6));
    PX(x + 1, y, RP(RAMP_STEEL, 4));
    PX(x, y + 1, RP(RAMP_STEEL, 4));
    PX(x + 1, y + 1, RP(RAMP_STEEL, 2));
}

static void t_steam_tile(ArtImg *im, int f, int n)
{
    int x, y, i;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int lx = x % 16, ly = y % 16;
            uint32_t id = art_hash((uint32_t)(x / 16), (uint32_t)(y / 16), 0x57E4u);
            int s = 4 + (int)(id & 1u);
            if (lx == 0 || ly == 0) { PX(x, y, RP(RAMP_STONE, 3)); continue; }
            s += tx_bevel(lx - 1, ly - 1, 15, 15, 1);
            if (art_noise(x, y, 8, 64, 0x57E5u) > 200) s++;
            if (art_hash((uint32_t)x, (uint32_t)y, 0x57E6u) % 23u == 0) s--;
            PX(x, y, RP(RAMP_MINT, s));
        }
    /* condensation: a bright bead with a dark tail */
    for (i = 0; i < 14; i++) {
        uint32_t h = art_hash((uint32_t)i, 3u, 0x57E7u);
        int bx = (int)(h % 64u), by = (int)((h >> 8) % 60u);
        if (bx % 16 < 2 || by % 16 < 2) continue;
        PX(bx, by, RP(RAMP_MINT, 7));
        PX(bx, by + 1, RP(RAMP_MINT, 4));
        PX(bx, by + 2, RP(RAMP_MINT, 4));
    }
}

static void t_pipe_steel(ArtImg *im, int f, int n)
{
    static const uint8_t prof[16] = {0, 2, 4, 5, 6, 7, 6, 5, 4, 4, 3, 3, 2, 2, 1, 0};
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int s = prof[y % 16];
            int fl = art_wrap(x, 32);
            if (art_noise(x, y, 4, 64, 0x91BEu) > 215) s++;
            if (fl < 4 || fl > 28) s = (s > 1 ? s - 1 : 0) + (fl == 0 || fl == 31 ? 0 : 1);
            PX(x, y, RP(RAMP_STEEL, s));
        }
    for (y = 0; y < 64; y += 16) {
        bolt(im, 1, y + 3); bolt(im, 1, y + 11);
        bolt(im, 33, y + 3); bolt(im, 33, y + 11);
    }
    /* rust weeps down from the flanges */
    for (x = 0; x < 64; x++) {
        int fl = art_wrap(x, 32);
        if (fl > 6 && fl < 26) continue;
        for (y = 0; y < 64; y++) {
            int ly = y % 16;
            if (ly < 8 || ly > 14) continue;
            if (art_noise(x, y, 4, 64, 0x91BFu) > 140 + (ly - 8) * 10)
                PX(x, y, PP(PAINT_RUST, ly > 11 ? 0 : 1));
        }
    }
}

static void t_office_carpet(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int v = (int)(art_hash((uint32_t)x, (uint32_t)y, 0xCA12u) % 100u);
            int s = 3;
            if (v < 20) s = 2;
            else if (v > 86) s = 4;
            if (((x + y) & 7) == 0 && v < 60) s++;
            if (x % 32 == 0 || y % 32 == 0) s = 1;
            else if (x % 32 == 1 || y % 32 == 1) s = 2;
            PX(x, y, RP(RAMP_STEEL, s));
        }
    /* a few dropped staples, because it is an office */
    for (n = 0; n < 4; n++) {
        uint32_t h = art_hash((uint32_t)n, 9u, 0xCA13u);
        int bx = 3 + (int)(h % 56u), by = 3 + (int)((h >> 8) % 56u);
        if (bx % 32 < 3 || by % 32 < 3) continue;
        PX(bx, by, RP(RAMP_STEEL, 7)); PX(bx + 1, by, RP(RAMP_STEEL, 6)); PX(bx + 2, by, RP(RAMP_STEEL, 7));
    }
}

static void t_cubicle_fabric(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int s = 3 + (((x ^ y) & 1) ? 1 : 0);
            if ((y & 3) == 0) s--;
            if (art_noise(x, y, 4, 64, 0xCB1Cu) > 200) s++;
            PX(x, y, RP(RAMP_PEACH, s));
        }
    /* the metal trim and its pin holes */
    for (x = 0; x < 64; x++) {
        PX(x, 28, RP(RAMP_STEEL, 6));
        PX(x, 29, RP(RAMP_STEEL, 4));
        PX(x, 30, RP(RAMP_STEEL, 3));
        PX(x, 31, RP(RAMP_STEEL, 1));
    }
    for (x = 4; x < 64; x += 8) PX(x, 29, RP(RAMP_STEEL, 1));
    /* a stitched seam down the middle */
    for (y = 0; y < 64; y++)
        if (y % 4 < 2 && (y < 28 || y > 31)) { PX(31, y, RP(RAMP_PEACH, 1)); PX(32, y, RP(RAMP_PEACH, 6)); }
}

static void t_felt(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int v = (int)(art_hash((uint32_t)x, (uint32_t)y, 0xFE17u) % 100u);
            int s = 2;
            if (v < 25) s = 1;
            else if (v > 80) s = 3;
            if (art_noise(x, y, 8, 64, 0xFE18u) > 190) s++;
            if (((x + y) % 16 == 0) || (art_wrap(x - y, 16) == 0)) s++;
            PX(x, y, RP(RAMP_GRASS, s));
        }
    /* buttons at the quilt crossings */
    for (y = 0; y < 64; y += 16)
        for (x = 0; x < 64; x += 16) {
            PX(x, y, RP(RAMP_GRASS, 0));
            PX(x + 1, y, RP(RAMP_GRASS, 4));
        }
}

static void t_ledger_page(ArtImg *im, int f, int n)
{
    int x, y, row;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int s = 6;
            if (art_hash((uint32_t)x, (uint32_t)y, 0x1ED6u) % 17u == 0) s = 5;
            PX(x, y, RP(RAMP_BONE, s));
        }
    for (y = 7; y < 64; y += 8)
        for (x = 0; x < 64; x++) PX(x, y, RP(RAMP_SKY, 4));
    for (y = 0; y < 64; y++) {
        PX(10, y, PP(PAINT_CHERRY, 1));
        PX(12, y, PP(PAINT_CHERRY, 1));
        PX(40, y, RP(RAMP_SKY, 3));
        PX(52, y, RP(RAMP_SKY, 3));
    }
    /* pencilled ticks and short totals, one row at a time */
    for (row = 0; row < 8; row++) {
        uint32_t h = art_hash((uint32_t)row, 5u, 0x1ED7u);
        int len = 3 + (int)(h % 6u), x0 = 16 + (int)((h >> 8) % 14u), y0 = row * 8 + 3;
        for (x = 0; x < len; x++) PX(x0 + x * 2, y0 + (x & 1), RP(RAMP_STEEL, 1));
        for (x = 42; x < 42 + 3 + (int)((h >> 16) % 7u); x++) PX(x, y0 + 2, RP(RAMP_STEEL, 2));
        if (row % 3 == 1) { PX(54, y0, PP(PAINT_CHERRY, 2)); PX(55, y0 + 1, PP(PAINT_CHERRY, 2)); PX(54, y0 + 2, PP(PAINT_CHERRY, 2)); }
    }
}

static void t_vault_metal(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int lx = x % 32, ly = y % 32;
            int s = 3 + tx_bevel(lx, ly, 32, 32, 2);
            if (art_noise(x / 16 + 40, y, 2, 64, 0x7A17u) > 170) s--;     /* brushed streaks */
            if (art_noise(x, y, 8, 64, 0x7A18u) > 215) s++;
            if (lx < 2 || ly < 2) s = lx < 2 && ly < 2 ? 1 : s;
            PX(x, y, RP(RAMP_STEEL, s < 0 ? 0 : s));
        }
    for (y = 0; y < 64; y += 32)
        for (x = 0; x < 64; x += 32) {
            bolt(im, x + 4, y + 4); bolt(im, x + 25, y + 4);
            bolt(im, x + 4, y + 25); bolt(im, x + 25, y + 25);
        }
    /* an amber lamp on one plate, with its glow */
    PX(46, 14, PP(PAINT_HONEY, 2)); PX(47, 14, PP(PAINT_HONEY, 2));
    PX(46, 15, PP(PAINT_HONEY, 1)); PX(47, 15, PP(PAINT_HONEY, 1));
    PX(45, 14, PP(PAINT_HONEY, 0)); PX(48, 14, PP(PAINT_HONEY, 0));
    PX(45, 15, PP(PAINT_HONEY, 0)); PX(48, 15, PP(PAINT_HONEY, 0));
    PX(46, 13, PP(PAINT_HONEY, 0)); PX(47, 13, PP(PAINT_HONEY, 0));
    PX(46, 16, PP(PAINT_HONEY, 0)); PX(47, 16, PP(PAINT_HONEY, 0));
}

const ArtTexDef art_tex_lv[] = {
    {TEX_STEAM_TILE, "steam_tile", 1, 1, DOOMO_ANIM_LOOP, t_steam_tile},
    {TEX_PIPE_STEEL, "pipe_steel", 1, 1, DOOMO_ANIM_LOOP, t_pipe_steel},
    {TEX_OFFICE_CARPET, "office_carpet", 1, 1, DOOMO_ANIM_LOOP, t_office_carpet},
    {TEX_CUBICLE_FABRIC, "cubicle_fabric", 1, 1, DOOMO_ANIM_LOOP, t_cubicle_fabric},
    {TEX_FELT, "felt", 1, 1, DOOMO_ANIM_LOOP, t_felt},
    {TEX_LEDGER_PAGE, "ledger_page", 1, 1, DOOMO_ANIM_LOOP, t_ledger_page},
    {TEX_VAULT_METAL, "vault_metal", 1, 1, DOOMO_ANIM_LOOP, t_vault_metal},
};
const int art_tex_lv_n = (int)(sizeof art_tex_lv / sizeof art_tex_lv[0]);
