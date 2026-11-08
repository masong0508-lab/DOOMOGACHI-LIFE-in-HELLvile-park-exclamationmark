/* test_pet_lcd.c - SP4 (G-P1): the pet LCD's dot buffer. pet_dots_rects()
 * (the rect path, native and PS1) must draw exactly the pixels of one
 * plat_rect a dot in fill[v], then (s >= 3) each dot's last row and column
 * in edge[v] (plat_ext.h plat_dots): random dot grids at s = 1..4 with
 * random tables (some edges equal to their fill), checked pixel by pixel
 * against that per-dot reference, at an offset that clips nothing and at
 * one the screen clips. The weak plat_dots says "cannot" (0) on native.
 * The whole scene's frames are checked against the old drawing by the
 * frame hashes (docs/GBA.md "SP4"). */
#include "test.h"
#include "doomo/plat_ext.h"

void pet_dots_rects(int x, int y, int s, const uint8_t *d, int dw, int dh, const uint8_t *fill, const uint8_t *edge);

static void ref_dots(int x, int y, int s, const uint8_t *d, int dw, int dh, const uint8_t *fill, const uint8_t *edge)
{
    int i, j;
    for (j = 0; j < dh; j++)
        for (i = 0; i < dw; i++) {
            int v = d[j * dw + i] & 7, px = x + i * s, py = y + j * s;
            plat_rect(px, py, s, s, fill[v]);
            if (s >= 3) {
                plat_rect(px, py + s - 1, s, 1, edge[v]);
                plat_rect(px + s - 1, py, 1, s, edge[v]);
            }
        }
}

TEST(pet_lcd_rects_match_per_dot)
{
    static uint8_t a[320 * 240];
    uint8_t d[PET_LCD_W * PET_LCD_H], fill[8], edge[8];
    uint32_t seed = 12345u;
    int round, s, k, n;
    NatOptions o;
    memset(&o, 0, sizeof(o));
    o.w = 320; o.h = 240; o.seed = 1; o.quiet = 1; o.save_dir = test_tmp_dir;
    nat_init(&o);
    n = nat_w * nat_h;
    for (round = 0; round < 24; round++) {
        int x = round & 1 ? -5 : 7, y = round & 2 ? 230 - PET_LCD_H * 3 : 3, kinds = 2 + round % 6;
        s = 1 + round % 4;
        for (k = 0; k < 8; k++) {
            seed = seed * 1103515245u + 12345u;
            fill[k] = (uint8_t)(1 + (seed >> 16) % 200);
            edge[k] = (seed >> 8) & 1 ? fill[k] : (uint8_t)(1 + (seed >> 20) % 200);
        }
        for (k = 0; k < PET_LCD_W * PET_LCD_H; k++) {
            seed = seed * 1103515245u + 12345u;
            /* blocks and runs, as an LCD has them, and noise */
            if (round % 3 == 0) d[k] = (uint8_t)((seed >> 16) % (uint32_t)kinds);
            else d[k] = (uint8_t)(((k / 7 + k / PET_LCD_W / 5) % kinds) ^ ((seed >> 27) == 0));
        }
        if (round == 5) memset(d, 0, sizeof(d));                     /* a blank LCD */
        memset(nat_fb, 9, (size_t)n);
        ref_dots(x, y, s, d, PET_LCD_W, PET_LCD_H, fill, edge);
        memcpy(a, nat_fb, (size_t)n);
        memset(nat_fb, 9, (size_t)n);
        pet_dots_rects(x, y, s, d, PET_LCD_W, PET_LCD_H, fill, edge);
        CHECK(memcmp(a, nat_fb, (size_t)n) == 0);
    }
    /* native has no plat_dots: the caller draws the rects */
    CHECK_EQ(plat_dots(0, 0, 2, d, PET_LCD_W, PET_LCD_H, fill, edge), 0);
}
