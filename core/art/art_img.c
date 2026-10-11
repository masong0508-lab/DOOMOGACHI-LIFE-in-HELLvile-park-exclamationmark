/* art_img.c - 8bpp image primitives for textures and skies (artgen only).
 * Coordinates wrap when the image says so, which makes seamless tiling
 * automatic: a brick that crosses the right edge re-enters on the left. */
#include "art.h"

void img_init(ArtImg *im, uint8_t *buf, int w, int h, int wrapx, int wrapy)
{
    im->w = w; im->h = h; im->wrapx = wrapx; im->wrapy = wrapy; im->p = buf;
    memset(buf, 0, (size_t)(w * h));
}

static int fix_xy(const ArtImg *im, int *x, int *y)
{
    if (im->wrapx) *x = art_wrap(*x, im->w);
    else if (*x < 0 || *x >= im->w) return 0;
    if (im->wrapy) *y = art_wrap(*y, im->h);
    else if (*y < 0 || *y >= im->h) return 0;
    return 1;
}

uint8_t img_get(const ArtImg *im, int x, int y)
{
    if (!fix_xy(im, &x, &y)) return 0;
    return im->p[y * im->w + x];
}

void img_put(ArtImg *im, int x, int y, uint8_t c)
{
    if (!fix_xy(im, &x, &y)) return;
    im->p[y * im->w + x] = c;
}

void img_fill(ArtImg *im, uint8_t c) { memset(im->p, c, (size_t)(im->w * im->h)); }

void img_rect(ArtImg *im, int x, int y, int w, int h, uint8_t c)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) img_put(im, x + i, y + j, c);
}

void img_hline(ArtImg *im, int x, int y, int w, uint8_t c) { img_rect(im, x, y, w, 1, c); }
void img_vline(ArtImg *im, int x, int y, int h, uint8_t c) { img_rect(im, x, y, 1, h, c); }

void img_line(ArtImg *im, int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = art_abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -art_abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        int e2;
        img_put(im, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void img_ellipse(ArtImg *im, int cx, int cy, int rx, int ry, uint8_t c)
{
    int x, y;
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    for (y = -ry; y <= ry; y++)
        for (x = -rx; x <= rx; x++) {
            /* pixel-centre test against (rx+0.5, ry+0.5) for round shapes */
            int64_t a = (int64_t)(2 * x) * (2 * x) * (2 * ry + 1) * (2 * ry + 1);
            int64_t b = (int64_t)(2 * y) * (2 * y) * (2 * rx + 1) * (2 * rx + 1);
            int64_t lim = (int64_t)(2 * rx + 1) * (2 * rx + 1) * (2 * ry + 1) * (2 * ry + 1);
            if (a + b <= lim) img_put(im, cx + x, cy + y, c);
        }
}

void img_disc(ArtImg *im, int cx, int cy, int r, uint8_t c) { img_ellipse(im, cx, cy, r, r, c); }

void img_ring(ArtImg *im, int cx, int cy, int r0, int r1, uint8_t c)
{
    int x, y;
    for (y = -r1; y <= r1; y++)
        for (x = -r1; x <= r1; x++) {
            int d2 = 4 * (x * x + y * y);
            if (d2 <= (2 * r1 + 1) * (2 * r1 + 1) && d2 >= (2 * r0 - 1) * (2 * r0 - 1))
                img_put(im, cx + x, cy + y, c);
        }
}

void img_shift(ArtImg *im, int x, int y, int steps)
{
    uint8_t c = img_get(im, x, y);
    if (!c) return;
    if (steps > 0) c = art_lighter(c, steps);
    else if (steps < 0) c = art_darker(c, -steps);
    img_put(im, x, y, c);
}

void img_ball(ArtImg *im, int cx, int cy, int rx, int ry, const ArtMat *m, int dither)
{
    int x, y;
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    for (y = -ry; y <= ry; y++)
        for (x = -rx; x <= rx; x++) {
            /* normalised coords in Q8 */
            int nx = (x * 2 * 256) / (2 * rx + 1), ny = (y * 2 * 256) / (2 * ry + 1);
            int r2 = nx * nx + ny * ny;
            int nz, v;
            if (r2 > 65536) continue;
            nz = (int)art_isqrt((uint32_t)(65536 - r2));
            /* light from top-left-front: (-0.45,-0.6,0.66) */
            v = (-nx * 115 - ny * 154 + nz * 169) / 256;   /* -256..256 */
            v = (v + 140) * 255 / 396;
            img_put(im, cx + x, cy + y, art_mat_pick(m, v, cx + x, cy + y, dither));
        }
}

void img_poly(ArtImg *im, const int *xy, int n, uint8_t c)
{
    int miny = 1 << 30, maxy = -(1 << 30), i, y;
    for (i = 0; i < n; i++) {
        if (xy[2 * i + 1] < miny) miny = xy[2 * i + 1];
        if (xy[2 * i + 1] > maxy) maxy = xy[2 * i + 1];
    }
    for (y = miny; y <= maxy; y++) {
        int xs[32], k = 0, a, b;
        /* sample at the pixel centre (y + 0.5) using 2x coordinates */
        int sy = 2 * y + 1;
        for (i = 0; i < n && k < 32; i++) {
            int x0 = 2 * xy[2 * i], y0 = 2 * xy[2 * i + 1];
            int x1 = 2 * xy[2 * ((i + 1) % n)], y1 = 2 * xy[2 * ((i + 1) % n) + 1];
            if ((y0 <= sy && y1 > sy) || (y1 <= sy && y0 > sy)) {
                xs[k++] = x0 + (int)((int64_t)(sy - y0) * (x1 - x0) / (y1 - y0));
            }
        }
        for (a = 1; a < k; a++)
            for (b = a; b > 0 && xs[b - 1] > xs[b]; b--) {
                int t = xs[b]; xs[b] = xs[b - 1]; xs[b - 1] = t;
            }
        for (a = 0; a + 1 < k; a += 2) {
            int x;
            for (x = (xs[a] + 1) / 2; 2 * x + 1 <= xs[a + 1]; x++) img_put(im, x, y, c);
        }
    }
}

void img_heart(ArtImg *im, int cx, int cy, int s, uint8_t c)
{
    int x, y;
    /* implicit heart: (x^2 + y^2 - 1)^3 - x^2 y^3 <= 0, in Q8 */
    for (y = -s - 1; y <= s + 1; y++)
        for (x = -s - 1; x <= s + 1; x++) {
            int64_t X = ((int64_t)(2 * x) * 256 * 13) / (2 * s * 10 + 1);
            int64_t Y = (-(int64_t)(2 * y + 1) * 256 * 13) / (2 * s * 10 + 1) + 40;
            int64_t q = (X * X + Y * Y) / 256 - 256;          /* Q8 */
            int64_t lhs = q * q / 256 * q / 256;              /* Q8 */
            int64_t rhs = X * X / 256 * (Y * Y / 256) / 256 * Y / 256;
            if (lhs - rhs <= 0) img_put(im, cx + x, cy + y, c);
        }
}

void img_star(ArtImg *im, int cx, int cy, int r, int filled, uint8_t c)
{
    int pts[20], k;
    for (k = 0; k < 10; k++) {
        int rr = (k & 1) ? r * 45 / 100 : r;
        int a = -1024 + k * 4096 / 10;
        pts[2 * k] = cx + (rr * art_cos(a) + (art_cos(a) > 0 ? 2048 : -2048)) / 4096;
        pts[2 * k + 1] = cy + (rr * art_sin(a) + (art_sin(a) > 0 ? 2048 : -2048)) / 4096;
    }
    if (filled) img_poly(im, pts, 10, c);
    else
        for (k = 0; k < 10; k++)
            img_line(im, pts[2 * k], pts[2 * k + 1], pts[(2 * k + 2) % 20], pts[(2 * k + 3) % 20], c);
}

/* ---- 5x7 block font ----------------------------------------------------- */
static const char font_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?.,-+:'/#*<>()=%_";
static const uint8_t font_rows[][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
    {7, 2, 2, 2, 2, 18, 12},      {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},
    {31, 2, 4, 2, 1, 17, 14},     {2, 6, 10, 18, 31, 2, 2},     {31, 16, 30, 1, 1, 17, 14},
    {6, 8, 16, 30, 17, 17, 14},   {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15, 1, 2, 12},
    {4, 4, 4, 4, 4, 0, 4},        {14, 17, 1, 2, 4, 0, 4},      {0, 0, 0, 0, 0, 12, 12},
    {0, 0, 0, 0, 12, 4, 8},       {0, 0, 0, 31, 0, 0, 0},       {0, 4, 4, 31, 4, 4, 0},
    {0, 12, 12, 0, 12, 12, 0},    {12, 4, 8, 0, 0, 0, 0},       {1, 2, 2, 4, 8, 8, 16},
    {10, 31, 10, 10, 31, 10, 0},  {0, 21, 14, 31, 14, 21, 0},   {2, 4, 8, 16, 8, 4, 2},
    {8, 4, 2, 1, 2, 4, 8},        {2, 4, 8, 8, 8, 4, 2},        {8, 4, 2, 2, 2, 4, 8},
    {0, 0, 31, 0, 31, 0, 0},      {24, 25, 2, 4, 8, 19, 3},     {0, 0, 0, 0, 0, 0, 31}};

static const uint8_t *glyph(char ch)
{
    int i;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    for (i = 0; font_chars[i]; i++)
        if (font_chars[i] == ch) return font_rows[i];
    return 0;
}

int img_text_w(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? (n * 6 - 1) * scale : 0;
}

void img_text(ArtImg *im, int x, int y, const char *s, int scale, uint8_t c)
{
    for (; *s; s++, x += 6 * scale) {
        const uint8_t *g = glyph(*s);
        int r, b;
        if (!g) continue;
        for (r = 0; r < 7; r++)
            for (b = 0; b < 5; b++)
                if (g[r] & (16 >> b)) img_rect(im, x + b * scale, y + r * scale, scale, scale, c);
    }
}

void img_blit(ArtImg *a, const ArtImg *b, int x, int y)
{
    int i, j;
    for (j = 0; j < b->h; j++)
        for (i = 0; i < b->w; i++) {
            uint8_t c = b->p[j * b->w + i];
            if (c) img_put(a, x + i, y + j, c);
        }
}

uint8_t img_avg(const ArtImg *im)
{
    long sr = 0, sg = 0, sb = 0, n = 0;
    int i;
    for (i = 0; i < im->w * im->h; i++) {
        int r, g, b;
        if (!im->p[i]) continue;
        art_rgb(im->p[i], &r, &g, &b);
        sr += r; sg += g; sb += b; n++;
    }
    if (!n) return PAL_BLACK;
    return art_nearest((int)(sr / n), (int)(sg / n), (int)(sb / n));
}
