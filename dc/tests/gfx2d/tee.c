/* tee.c - the 2D layer's dual-render host runner (DC3; owner: dreamcast).
 *
 * The native runner (core/native) is linked with its 2D calls renamed to
 * ref_* (plat_native.c, and a second copy of the core's software post
 * effects, plat_weak_fx.c, drawing through them). This file defines the
 * real platform.h 2D calls: each one draws into the native 8bpp frame
 * (the reference) AND into the Dreamcast prim list (dc/src/gfx2d.c built
 * with -DDC2D_HOST). Every runner script then plays once and drives both.
 * At each `shot` (the runner writes it with nat_png_write, renamed to
 * tee_png_write here) the last submitted Dreamcast frame is rendered by
 * the software PVR (swpvr.c) at 640x480 and compared with the native
 * frame, logical pixel by logical pixel (all four screen pixels of the
 * 2x2 block, in RGB565):
 *   exact   the block equals the native pixel
 *   close   every channel within 16/255 (the shade and shadow fits)
 * Environment:
 *   DC3_REPORT=file  append one line per shot (and a summary at exit)
 *   DC3_PNG=0        do not write NAME_dc.png (640x480) / NAME_diff.png
 *   DC3_FX=sw        post effects and scaled blits in software on both sides
 *                    (see plat_post_offset below)
 * Only 320x240 runs are compared (the Dreamcast's logical screen). The 3D
 * views (plat_view3d, plat_overworld: DC6, DC7) are drawn by the native
 * renderer and handed to the Dreamcast list as an image, so only the 2D
 * over them is under test.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"
#include "dc2d.h"
#include "swpvr.h"
#include "png.h"
#include "doomo/plat_ext.h"

/* the native reference (renamed at compile time, see Makefile) */
void ref_begin_frame(void);
void ref_end_frame(void);
void ref_clear(uint8_t color);
void ref_rect(int x, int y, int w, int h, uint8_t color);
void ref_shade_rect(int x, int y, int w, int h, int level);
void ref_line(int x0, int y0, int x1, int y1, uint8_t color);
void ref_checker(int x, int y, int w, int h, uint8_t color);
void ref_text(int x, int y, const char *s, uint8_t color);
void ref_image(int x, int y, const DoomoImage *img, int scale, int flags, uint8_t color);
int  ref_hold_frame(int on);
void ref_image_xform(int ax, int ay, const DoomoImage *img, int sx_q8, int sy_q8, int flags, uint8_t color);
void ref_post_offset(int dx, int dy);
void ref_post_scale(int sx_q8, int sy_q8, int cx, int cy);
void ref_post_blend(uint8_t color, int level16);
void ref_post_iris(int cx, int cy, int r, int heart, uint8_t rim);
void ref_pal_lerp(int pal_id, int level16);
void sw_image_xform(int ax, int ay, const DoomoImage *img, int sx_q8, int sy_q8, int flags, uint8_t color);
void sw_post_offset(int dx, int dy);
void sw_post_scale(int sx_q8, int sy_q8, int cx, int cy);
void sw_post_blend(uint8_t color, int level16);
void sw_post_iris(int cx, int cy, int r, int heart, uint8_t rim);
void sw_pal_lerp(int pal_id, int level16);
void ref_view3d(const DoomoView3D *v);
void ref_overworld(const DoomoOverworldView *v);

static int inited;
uint32_t tee_hold_mismatch, tee_frames;

static void tee_init(void)
{
    if (inited) return;
    inited = 1;
    dc2d_init();
}

void plat_begin_frame(void)
{
    tee_init();
    ref_begin_frame();
    dc2d_begin(1);
}

void plat_end_frame(void)
{
    dc2d_end();
    ref_end_frame();
    tee_frames++;
}

void plat_clear(uint8_t c) { ref_clear(c); dc2d_clear(c); }
void plat_rect(int x, int y, int w, int h, uint8_t c) { ref_rect(x, y, w, h, c); dc2d_rect(x, y, w, h, c); }
void plat_shade_rect(int x, int y, int w, int h, int l) { ref_shade_rect(x, y, w, h, l); dc2d_shade_rect(x, y, w, h, l); }
void plat_line(int x0, int y0, int x1, int y1, uint8_t c) { ref_line(x0, y0, x1, y1, c); dc2d_line(x0, y0, x1, y1, c); }
void plat_checker(int x, int y, int w, int h, uint8_t c) { ref_checker(x, y, w, h, c); dc2d_checker(x, y, w, h, c); }
void plat_text(int x, int y, const char *s, uint8_t c) { ref_text(x, y, s, c); dc2d_text(x, y, s, c); }

/* the reference of plat_text_outline is the core's weak default (ui.c): ten passes */
void plat_text_outline(int x, int y, const char *s, uint8_t color, uint8_t outline)
{
    int dx, dy;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (dx || dy) ref_text(x + dx, y + dy, s, outline);
    ref_text(x + 1, y + 2, s, outline);
    ref_text(x, y, s, color);
    dc2d_text_outline(x, y, s, color, outline);
}

void plat_image(int x, int y, const DoomoImage *img, int scale, int flags, uint8_t color)
{
    ref_image(x, y, img, scale, flags, color);
    dc2d_image(x, y, img, scale, flags, color);
}

/* The post effects and the scaled blit. By default the Dreamcast does them
 * in hardware (as the GBA and PS1 do) while the native runner keeps its
 * cheap software look (offset, scale and palette lerp are no-ops there,
 * a blend below 16 is a checker or a darken), so shots that use them
 * differ by design. DC3_FX=sw draws the software versions through both
 * sides instead, so every other pixel of those shots can be compared. */
static int fx_sw = -1;
static int sw(void)
{
    if (fx_sw < 0) { const char *e = getenv("DC3_FX"); fx_sw = e && strcmp(e, "sw") == 0; }
    return fx_sw;
}

void plat_image_xform(int ax, int ay, const DoomoImage *img, int sx, int sy, int flags, uint8_t color)
{
    if (sw()) { sw_image_xform(ax, ay, img, sx, sy, flags, color); return; }
    ref_image_xform(ax, ay, img, sx, sy, flags, color);
    dc2d_image_xform(ax, ay, img, sx, sy, flags, color);
}

void plat_post_offset(int dx, int dy)
{
    if (sw()) { sw_post_offset(dx, dy); return; }
    ref_post_offset(dx, dy);
    dc2d_post_offset(dx, dy);
}
void plat_post_scale(int sx, int sy, int cx, int cy)
{
    if (sw()) { sw_post_scale(sx, sy, cx, cy); return; }
    ref_post_scale(sx, sy, cx, cy);
    dc2d_post_scale(sx, sy, cx, cy);
}
void plat_post_blend(uint8_t c, int l)
{
    if (sw()) { sw_post_blend(c, l); return; }
    ref_post_blend(c, l);
    dc2d_post_blend(c, l);
}
void plat_post_iris(int cx, int cy, int r, int heart, uint8_t rim)
{
    if (sw()) { sw_post_iris(cx, cy, r, heart, rim); return; }
    ref_post_iris(cx, cy, r, heart, rim);
    dc2d_post_iris(cx, cy, r, heart, rim);
}
void plat_pal_lerp(int id, int l)
{
    if (sw()) { sw_pal_lerp(id, l); return; }
    ref_pal_lerp(id, l);
    dc2d_pal_lerp(id, l);
}

/* The 3D views are not part of DC3 (DC6, DC7). So that the 2D drawn over
 * them can still be compared, the native renderer draws the view and its
 * pixels go to the Dreamcast list as an image (a stand-in, host only). */
static uint8_t view_pix[DC2D_LW * DC2D_LH];
static DoomoImage view_img;

static void view_standin(int vx, int vy, int vw, int vh)
{
    int x0 = vx < 0 ? 0 : vx, y0 = vy < 0 ? 0 : vy, x1 = vx + vw, y1 = vy + vh, y;
    if (x1 > nat_w) x1 = nat_w;
    if (y1 > nat_h) y1 = nat_h;
    if (x1 <= x0 || y1 <= y0 || nat_w != DC2D_LW || nat_h != DC2D_LH) return;
    for (y = y0; y < y1; y++) memcpy(&view_pix[(y - y0) * (x1 - x0)], &nat_fb[y * nat_w + x0], (size_t)(x1 - x0));
    view_img.w = (uint16_t)(x1 - x0);
    view_img.h = (uint16_t)(y1 - y0);
    view_img.pix = view_pix;
    view_img.uid = 0;
    dc2d_rect(x0, y0, x1 - x0, y1 - y0, 0);     /* index 0 of the view is black */
    dc2d_image(x0, y0, &view_img, 1, 0, 0);
}

void plat_view3d(const DoomoView3D *v)
{
    ref_view3d(v);
    if (v) view_standin(v->vx, v->vy, v->vw, v->vh);
}

void plat_overworld(const DoomoOverworldView *v)
{
    ref_overworld(v);
    if (v) view_standin(v->vx, v->vy, v->vw, v->vh);
}

int plat_hold_frame(int on)
{
    int r = ref_hold_frame(on), d = dc2d_hold_frame(on);
    if (r != d) tee_hold_mismatch++;
    return r;
}

/* ---- the comparison at each shot -------------------------------------------------- */
static uint8_t dc_rgb[DC2D_W * DC2D_H * 3];
static uint8_t diff_rgb[DC2D_LW * DC2D_LH * 3];
static double sum_exact, sum_close, min_exact = 100.0;
static int nshots;
static char min_name[256];

static int x5to8(int v) { return (v << 3) | (v >> 2); }

/* native palette colour as the console's RGB565 frame buffer shows it */
static void nat565(uint8_t idx, int *r, int *g, int *b)
{
    uint16_t c = doomo_palette[idx];
    int g5 = (c >> 5) & 31, g6 = (g5 << 1) | (g5 >> 4);
    *r = x5to8(c & 31);
    *g = (g6 << 2) | (g6 >> 4);
    *b = x5to8((c >> 10) & 31);
}

static void report(const char *line)
{
    const char *path = getenv("DC3_REPORT");
    FILE *f;
    if (!path || !*path) return;
    f = fopen(path, "a");
    if (!f) return;
    fputs(line, f);
    fclose(f);
}

static void summary(void)
{
    char line[600];
    if (!nshots) return;
    snprintf(line, sizeof(line),
             "summary: %d shots, mean exact %.3f%%, mean close %.3f%%, worst exact %.3f%% (%s), "
             "prims max %u, vbytes max %u, dropped %u, uploads %u, evictions %u, tex peak %u B, hold mismatches %u, "
             "frames %u, exact shades %u (re-drawn prims %u, prims tested %u), shade fallbacks %u, tex fails %u\n",
             nshots, sum_exact / nshots, sum_close / nshots, min_exact, min_name,
             dc2d_stats.prims_max, dc2d_stats.vbytes_max, dc2d_stats.dropped, dc2d_stats.uploads,
             dc2d_stats.evictions, swpvr_tex_peak, tee_hold_mismatch, tee_frames, dc2d_stats.shades,
             dc2d_stats.shade_prims, dc2d_stats.shade_tests, dc2d_stats.shade_fallbacks, dc2d_stats.tex_fail);
    report(line);
}

static void compare(const char *path)
{
    const char *png = getenv("DC3_PNG");
    int x, y, exact = 0, close = 0, n = DC2D_LW * DC2D_LH;
    char out[700], line[900];
    double pe, pc;
    size_t len = strlen(path);
    swpvr_render(dc2d_last(), dc_rgb);
    for (y = 0; y < DC2D_LH; y++)
        for (x = 0; x < DC2D_LW; x++) {
            int r, g, b, k, ex = 1, cl = 1;
            uint8_t *d = &diff_rgb[(y * DC2D_LW + x) * 3];
            nat565(nat_fb[y * nat_w + x], &r, &g, &b);
            for (k = 0; k < 4; k++) {
                const uint8_t *p = &dc_rgb[((2 * y + (k >> 1)) * DC2D_W + 2 * x + (k & 1)) * 3];
                int dr = abs(p[0] - r), dg = abs(p[1] - g), db = abs(p[2] - b);
                if (dr || dg || db) ex = 0;
                if (dr > 16 || dg > 16 || db > 16) cl = 0;
            }
            exact += ex;
            close += cl;
            /* diff picture: grey native where equal, yellow close, red not */
            if (ex) { d[0] = d[1] = d[2] = (uint8_t)((r + g + b) / 6); }
            else if (cl) { d[0] = 255; d[1] = 220; d[2] = 0; }
            else { d[0] = 255; d[1] = 0; d[2] = 0; }
        }
    pe = 100.0 * exact / n;
    pc = 100.0 * close / n;
    nshots++;
    sum_exact += pe;
    sum_close += pc;
    if (pe < min_exact) { min_exact = pe; snprintf(min_name, sizeof(min_name), "%s", path); }
    if (!png || strcmp(png, "0") != 0) {
        if (len > 4 && strcmp(path + len - 4, ".png") == 0) len -= 4;
        snprintf(out, sizeof(out), "%.*s_dc.png", (int)len, path);
        png_write_rgb(out, dc_rgb, DC2D_W, DC2D_H);
        snprintf(out, sizeof(out), "%.*s_diff.png", (int)len, path);
        png_write_rgb(out, diff_rgb, DC2D_LW, DC2D_LH);
    }
    snprintf(line, sizeof(line), "shot %s exact %.3f%% close %.3f%% prims %u vbytes %u sprites %u quads %u headers %u tex %d\n",
             path, pe, pc, dc2d_stats.prims, dc2d_stats.vbytes, dc2d_stats.sprites,
             dc2d_stats.quads, dc2d_stats.headers, dc2d_tex_live());
    report(line);
}

int tee_png_write(const char *path, const uint8_t *pix, int w, int h, int zoom)
{
    int rc = nat_png_write(path, pix, w, h, zoom);
    static int at;
    if (!at) { at = 1; atexit(summary); }
    if (rc && pix == nat_fb && w == DC2D_LW && h == DC2D_LH && inited) compare(path);
    return rc;
}

void dc2d_be_text(const char *s) { (void)s; }
void dc2d_be_log(const char *msg) { plat_log(msg); }
