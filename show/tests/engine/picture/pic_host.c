/* pic_host.c - the lane-2 test driver on the host: a 320x240 BGR555 software
 * raster standing in for the PS1's 2D layer, for exact-number tests in
 * seconds. Diagnostics only: never a master pixel (spec 1.2); the contact
 * sheets come from the PS1 build of the same driver.
 *
 *   pic_host BLOB OUTDIR [--png-frames a,b,c|all] [--no-draw]
 * writes OUTDIR/fNNNN.ppm for the listed frames and the P-log on stdout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/camfx.h"

int  pic_init(const void *blob, unsigned len);
void pic_frame(int t, int draw);

static uint16_t fb[240][320];
static int cx0, cy0, cx1 = 320, cy1 = 240;
static int pal_id, pal_l, pal_set;

/* palette.c points at the game's light tables; the host never uses them */
const uint8_t doomo_art_colormap[DOOMO_LIGHT_LEVELS][256];
const uint8_t doomo_art_tintmap[TINT_COUNT][256];
const uint8_t doomo_art_darker[256];
const uint8_t doomo_art_lighter[256];

uint32_t doomo_image_new_uid(void) { static uint32_t n = 0x01000000u; return ++n; }
int opt_get(int opt) { return opt == OPT_SCREEN_SHAKE ? 2 : 0; }
int plat_screen_w(void) { return 320; }
int plat_screen_h(void) { return 240; }

static uint16_t col(uint8_t c)
{
    return pal_id ? camfx_pal_entry(c, pal_id, pal_l, 0, 0) : doomo_palette[c];
}

void show_l2_host_clip(int x, int y, int w, int h)
{
    cx0 = x; cy0 = y; cx1 = x + w; cy1 = y + h;
    if (cx0 < 0) cx0 = 0;
    if (cy0 < 0) cy0 = 0;
    if (cx1 > 320) cx1 = 320;
    if (cy1 > 240) cy1 = 240;
}

static void px(int x, int y, uint16_t c)
{
    if (x >= cx0 && x < cx1 && y >= cy0 && y < cy1) fb[y][x] = c;
}

void plat_rect(int x, int y, int w, int h, uint8_t c)
{
    int i, j;
    uint16_t v = col(c);
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++) px(i, j, v);
}

void plat_clear(uint8_t c) { plat_rect(0, 0, 320, 240, c); }

void plat_line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    uint16_t v = col(c);
    for (;;) {
        px(x0, y0, v);
        if (x0 == x1 && y0 == y1) break;
        if (2 * e >= dy) { e += dy; x0 += sx; }
        if (2 * e <= dx) { e += dx; y0 += sy; }
    }
}

void plat_image(int x, int y, const DoomoImage *img, int scale, int flags, uint8_t color)
{
    int u, v, i, j;
    if (!img || !img->pix) return;
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    for (v = 0; v < img->h; v++)
        for (u = 0; u < img->w; u++) {
            int su = (flags & IMG_FLIPX) ? img->w - 1 - u : u;
            uint8_t c = img->pix[v * img->w + su];
            if (!c) continue;
            if (flags & IMG_SILHOUETTE) c = color;
            for (j = 0; j < scale; j++)
                for (i = 0; i < scale; i++) px(x + u * scale + i, y + v * scale + j, col(c));
        }
}

void plat_image_xform(int ax, int ay, const DoomoImage *img, int sx_q8, int sy_q8, int flags, uint8_t color)
{
    int flip, asx, dw, dh, x0, y0, X, Y;
    if (!img || !img->pix || sx_q8 == 0 || sy_q8 <= 0) return;
    flip = (flags & IMG_FLIPX) ? 1 : 0;
    if (sx_q8 < 0) flip ^= 1;
    asx = sx_q8 < 0 ? -sx_q8 : sx_q8;
    if (asx > 1024) asx = 1024;
    if (sy_q8 > 1024) sy_q8 = 1024;
    dw = (img->w * asx) >> 8;
    dh = (img->h * sy_q8) >> 8;
    if (dw <= 0 || dh <= 0) return;
    x0 = ax - dw / 2;
    y0 = ay - dh;
    for (Y = 0; Y < dh; Y++)
        for (X = 0; X < dw; X++) {
            int u = X * img->w / dw, v = Y * img->h / dh;
            uint8_t c;
            if (flip) u = img->w - 1 - u;
            c = img->pix[v * img->w + u];
            if (!c) continue;
            if (flags & IMG_SILHOUETTE) c = color;
            px(x0 + X, y0 + Y, col(c));
        }
}

void plat_post_iris(int cx, int cy, int r, int heart, uint8_t rim)
{
    int y, x, s[4], n;
    (void)rim;
    for (y = 0; y < 240; y++) {
        n = camfx_iris_row(y, cx, cy, r, heart, s);
        for (x = 0; x < 320; x++) {
            int in = 0, k;
            for (k = 0; k < n; k++) if (x >= s[2 * k] && x < s[2 * k + 1]) in = 1;
            if (!in) fb[y][x] = doomo_palette[PAL_BLACK];
        }
    }
}

void plat_post_blend(uint8_t color, int level16)
{
    uint16_t t = doomo_palette[color];
    int x, y;
    for (y = 0; y < 240; y++)
        for (x = 0; x < 320; x++) {
            uint16_t c = fb[y][x];
            int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
            r += ((t & 31) - r) * level16 / 16;
            g += (((t >> 5) & 31) - g) * level16 / 16;
            b += (((t >> 10) & 31) - b) * level16 / 16;
            fb[y][x] = (uint16_t)(r | (g << 5) | (b << 10));
        }
}

void plat_pal_lerp(int id, int level16) { pal_id = id; pal_l = level16; pal_set = 1; }
void plat_post_scale(int sx_q8, int sy_q8, int cx, int cy) { (void)sx_q8; (void)sy_q8; (void)cx; (void)cy; }
void plat_post_offset(int dx, int dy) { (void)dx; (void)dy; }

static void write_ppm(const char *dir, int t)
{
    char path[512];
    FILE *f;
    int x, y;
    snprintf(path, sizeof(path), "%s/f%04d.ppm", dir, t);
    f = fopen(path, "wb");
    if (!f) { perror(path); exit(2); }
    fprintf(f, "P6\n320 240\n255\n");
    for (y = 0; y < 240; y++)
        for (x = 0; x < 320; x++) {
            uint16_t c = fb[y][x];
            int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
            fputc((r << 3) | (r >> 2), f);
            fputc((g << 3) | (g >> 2), f);
            fputc((b << 3) | (b >> 2), f);
        }
    fclose(f);
}

int main(int argc, char **argv)
{
    FILE *f;
    long n;
    unsigned char *blob;
    int frames, t, draw = 1, all = 0;
    char want[100000];
    if (argc < 3) { fprintf(stderr, "usage: pic_host BLOB OUTDIR [--png-frames a,b|all] [--no-draw]\n"); return 2; }
    memset(want, 0, sizeof(want));
    for (t = 3; t < argc; t++) {
        if (!strcmp(argv[t], "--no-draw")) draw = 0;
        else if (!strcmp(argv[t], "--png-frames") && t + 1 < argc) {
            char *s = argv[++t];
            if (!strcmp(s, "all")) all = 1;
            else while (*s) {
                int k = atoi(s);
                if (k >= 0 && k < (int)sizeof(want)) want[k] = 1;
                while (*s && *s != ',') s++;
                if (*s) s++;
            }
        }
    }
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    blob = malloc((size_t)n + 4);
    if (fread(blob, 1, (size_t)n, f) != (size_t)n) { fclose(f); return 2; }
    fclose(f);
    frames = pic_init(blob, (unsigned)n);
    if (frames < 0) { fprintf(stderr, "bad blob\n"); return 2; }
    for (t = 0; t < frames; t++) {
        /* the PS1 frame start: a lerp nobody re-set ends (postfx_frame_begin) */
        if (!pal_set) pal_id = 0;
        pal_set = 0;
        cx0 = cy0 = 0; cx1 = 320; cy1 = 240;
        pic_frame(t, draw);
        if (draw && (all || (t < (int)sizeof(want) && want[t]))) write_ppm(argv[2], t);
    }
    printf("PIC END %d\n", frames);
    return 0;
}
