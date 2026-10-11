/* art_parts.c - reusable sprite parts: glossy eyes, demon eyes, fangs,
 * horns, paws, glows, cloud puffs (artgen only). */
#include "art.h"

void part_eye_cute(int cx, int cy, int rx, int ry, int paint, int lookx, int looky)
{
    ArtMat white = art_mat3(PAL_GRAY_L, PAL_WHITE, PAL_WHITE);
    ArtMat iris = art_mat3(PP(paint, 0), PP(paint, 1), PP(paint, 2));
    ArtMat dark = art_mat2(PAL_PAINT(PAINT_JET, 0), PAL_PAINT(PAINT_JET, 1));
    int irx = rx * 3 / 4 + 1, iry = ry * 4 / 5 + 1;
    if (irx > rx) irx = rx;
    if (iry > ry) iry = ry;
    /* white of the eye, iris filling most of it (cute = big iris) */
    spr_ellipse(cx, cy, rx, ry, &white, SH_FLAT | SH_NOCONTOUR);
    spr_ellipse(cx + lookx, cy + looky + (ry - iry + 1) / 2, irx, iry, &iris,
                SH_VGRAD | SH_ONTOP | SH_NOCONTOUR);
    /* dark top lid shading inside the iris */
    spr_ellipse(cx + lookx, cy + looky + ry / 3, irx * 3 / 5 + 1 > irx ? irx : irx * 3 / 5 + 1,
                iry * 3 / 5 + 1, &dark, SH_FLAT | SH_ONTOP | SH_NOCONTOUR);
    /* two highlights: big top-left, small bottom-right */
    spr_px(cx + lookx - irx / 2, cy + looky - iry / 3, PAL_WHITE);
    if (rx >= 3) {
        spr_px(cx + lookx - irx / 2 + 1, cy + looky - iry / 3, PAL_WHITE);
        spr_px(cx + lookx - irx / 2, cy + looky - iry / 3 + 1, PAL_WHITE);
    }
    if (rx >= 4) spr_px(cx + lookx - irx / 2 + 1, cy + looky - iry / 3 + 1, PAL_WHITE);
    spr_px(cx + lookx + irx / 2 - (rx >= 3 ? 1 : 0), cy + looky + iry / 2, PP(paint, 2));
    if (rx >= 3) spr_px(cx + lookx + irx / 2 - 1, cy + looky + iry / 2, PAL_WHITE);
}

void part_eye_glow(int cx, int cy, int rx, int ry, const ArtMat *glow, int slit, uint8_t pupil)
{
    int k;
    spr_ellipse(cx, cy, rx, ry, glow, SH_VGRAD | SH_GLOW | SH_NOCONTOUR);
    if (slit) {
        for (k = -ry + 1; k <= ry - 1; k++) spr_px(cx, cy + k, pupil);
        if (rx >= 3 && ry >= 3) {
            spr_px(cx - 1, cy, pupil);
            spr_px(cx - 1, cy - 1, pupil);
            spr_px(cx - 1, cy + 1, pupil);
        }
    }
    spr_px(cx - rx / 2 - (rx > 2 ? 0 : 1) + (rx > 2 ? 0 : 1), cy - ry / 2, PAL_WHITE);
}

void part_eye_x(int cx, int cy, int r, uint8_t c)
{
    spr_line(cx - r, cy - r, cx + r, cy + r, c);
    spr_line(cx - r, cy + r, cx + r, cy - r, c);
}

void part_eye_shut(int cx, int cy, int r, uint8_t c)
{
    spr_line(cx - r, cy + 1, cx, cy - 1 + (r < 2), c);
    spr_line(cx, cy - 1 + (r < 2), cx + r, cy + 1, c);
}

void part_eye_squeeze(int cx, int cy, int r, uint8_t c)
{
    /* drawn as a '>' for the left eye; callers mirror via sign of r */
    int s = r < 0 ? -1 : 1, a = art_abs(r);
    spr_line(cx - s * a, cy - a, cx + s * a, cy, c);
    spr_line(cx + s * a, cy, cx - s * a, cy + a, c);
}

void part_fang(int x, int y, int w, int h, int dir, uint8_t c)
{
    int j, i;
    for (j = 0; j < h; j++) {
        int ww = w - (w * j) / h;
        if (ww < 1) ww = 1;
        for (i = 0; i < ww; i++)
            spr_px(x + (w - ww) / 2 + i, y + dir * j, j == 0 && i == 0 ? art_lighter(c, 1) : c);
    }
}

void part_glow(int cx, int cy, int r, const ArtMat *m)
{
    int y, x;
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++) {
            int d = art_dist(x * 16, y * 16) * 255 / (r * 16 + 8);
            int v, k;
            if (d > 255) continue;
            v = 255 - d;
            /* soft dither at band edges */
            k = v * m->n;
            if ((k & 255) > 128 + (art_bayer(x, y) * 8)) k += 256;
            k >>= 8;
            if (k <= 0) {
                if (((x + y) & 1) == 0 || d > 230) continue;
                k = 0;
            }
            if (k > m->n - 1) k = m->n - 1;
            if (!spr_get(cx + x, cy + y)) spr_set(cx + x, cy + y, m->c[k]);
        }
}

void part_horn(int x, int y, int dx, int dy, int bend, int r, const ArtMat *m)
{
    /* control point bends the horn sideways (perpendicular to its axis) */
    int mx = x + dx / 2 - dy * bend / 8, my = y + dy / 2 + dx * bend / 8;
    spr_bezier(x, y, mx, my, x + dx, y + dy, r, 0, m, SH_CYL);
}

void part_paw(int x, int y, int r, const ArtMat *m, int claws, uint8_t claw)
{
    spr_ellipse(x, y, r, r, m, 0);
    if (claws) {
        int k;
        for (k = -1; k <= 1; k++) {
            spr_px(x + k * (r * 2 / 3 + (r > 2)), y + r, claw);
            if (r >= 3) spr_px(x + k * (r * 2 / 3 + 1), y + r + 1, art_darker(claw, 1));
        }
    }
}

void part_blush(int cx, int cy, int w)
{
    int x;
    for (x = -w; x < w; x++) {
        uint8_t cur = spr_get(cx + x, cy);
        if (cur) spr_set(cx + x, cy, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        if ((x & 1) == 0 && x > -w && x < w - 1) {
            cur = spr_get(cx + x, cy + 1);
            if (cur) spr_set(cx + x, cy + 1, PAL_PAINT(PAINT_BUBBLEGUM, 0));
        }
    }
}

void part_cloud(int cx, int cy, int r, const ArtMat *m, uint32_t seed, int fl)
{
    ArtRng rng;
    int k, n;
    art_seed(&rng, seed);
    n = 3 + art_rnd(&rng, 3);
    /* base row of puffs, then a bigger top puff, all merged into one part */
    for (k = 0; k < n; k++) {
        int ox = (k - (n - 1) / 2) * r * 2 / 3 + art_range(&rng, -1, 1);
        int oy = art_range(&rng, -r / 4, r / 6);
        int rr = r * art_range(&rng, 55, 80) / 100;
        spr_ellipse(cx + ox, cy + oy, rr, rr * 9 / 10, m, fl | (k ? SH_SAMEPART : 0));
    }
    spr_ellipse(cx + art_range(&rng, -r / 4, r / 4), cy - r / 3, r * 3 / 4, r * 2 / 3, m,
                fl | SH_SAMEPART);
}
