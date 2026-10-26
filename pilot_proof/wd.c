/* wd.c - world-space drawing for the pilot proof (see wd.h). */
#include "wd.h"
#include "pal.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

float wd_cx = 0, wd_cy = 0, wd_z = 1;
void wd_cam(float cx, float cy, float z) { wd_cx = cx; wd_cy = cy; wd_z = z; }
float wd_sx(float x) { return (x - wd_cx) * wd_z + TW * 0.5f; }
float wd_sy(float y) { return (y - wd_cy) * wd_z + TH * 0.5f; }

static const uint8_t BAY[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
static int luma(uint8_t c) { return (TK_PAL[c][0] * 3 + TK_PAL[c][1] * 6 + TK_PAL[c][2]) / 10; }

uint8_t wd_dk(uint8_t c) {
    if (c == PAL_UI_CREAM) return PAL_PAINT(PAINT_BONE, 1);
    if (c == PAL_UI_PINK) return PAL_PAINT(PAINT_BUBBLEGUM, 1);
    if (c == PAL_UI_MINT) return PAL_PAINT(PAINT_MINT, 1);
    if (c >= PAL_PAINT_BASE && c < PAL_SKIN_BASE) { int k = (c - PAL_PAINT_BASE) % 3; return k ? (uint8_t)(c - 1) : c; }
    if (c >= PAL_SKIN_BASE && c < PAL_RAMP_BASE) { int k = (c - PAL_SKIN_BASE) % 3; return k ? (uint8_t)(c - 1) : PAL_RAMP(RAMP_PEACH, 3); }
    if (c >= PAL_RAMP_BASE) { int k = (c - PAL_RAMP_BASE) % 8; return k ? (uint8_t)(c - 1) : c; }
    if (c == PAL_WHITE) return PAL_GRAY_L;
    if (c == PAL_GRAY_L) return PAL_GRAY;
    if (c == PAL_GRAY) return PAL_GRAY_D;
    return PAL_BLACK;
}
uint8_t wd_lt(uint8_t c) {
    if (c == PAL_UI_CREAM) return PAL_WHITE;
    if (c >= PAL_PAINT_BASE && c < PAL_SKIN_BASE) { int k = (c - PAL_PAINT_BASE) % 3; return k < 2 ? (uint8_t)(c + 1) : PAL_WHITE; }
    if (c >= PAL_SKIN_BASE && c < PAL_RAMP_BASE) { int k = (c - PAL_SKIN_BASE) % 3; return k < 2 ? (uint8_t)(c + 1) : PAL_WHITE; }
    if (c >= PAL_RAMP_BASE) { int k = (c - PAL_RAMP_BASE) % 8; return k < 7 ? (uint8_t)(c + 1) : PAL_WHITE; }
    if (c == PAL_GRAY_D) return PAL_GRAY;
    if (c == PAL_GRAY) return PAL_GRAY_L;
    return PAL_WHITE;
}
uint8_t wd_ol(uint8_t c) {
    uint8_t d;
    if (c >= PAL_PAINT_BASE && c < PAL_SKIN_BASE) d = (uint8_t)(c - (c - PAL_PAINT_BASE) % 3);
    else if (c >= PAL_SKIN_BASE && c < PAL_RAMP_BASE) d = PAL_RAMP(RAMP_PEACH, 2);
    else if (c >= PAL_RAMP_BASE) { int k = (c - PAL_RAMP_BASE) % 8; d = (uint8_t)(c - (k >= 3 ? 3 : k)); }
    else d = wd_dk(wd_dk(c));
    if (luma(d) > 120) {
        int r = TK_PAL[c][0], b = TK_PAL[c][2];
        d = r >= b ? PAL_RAMP(RAMP_PEACH, 1) : PAL_PAINT(PAINT_GRAPE, 0);
        if (TK_PAL[c][1] > r && TK_PAL[c][1] > b) d = PAL_RAMP(RAMP_MINT, 1);
        if (abs(r - b) < 24 && abs(TK_PAL[c][1] - r) < 24) d = PAL_GRAY_D;
    }
    return d;
}

static inline void px(int x, int y, uint8_t c) {
    if ((unsigned)x < TW && (unsigned)y < TH) tk_fb[y][x] = c;
}

void wd_blob_ol(float cx, float cy, float rx, float ry, float rot, uint8_t c, uint8_t o, int flags) {
    float pcx = wd_sx(cx), pcy = wd_sy(cy), prx = rx * wd_z, pry = ry * wd_z;
    if (prx < 0.35f) prx = 0.35f;
    if (pry < 0.35f) pry = 0.35f;
    float e = (flags & WB_OUT) ? 1.0f : 0.0f;
    float orx = prx + e, ory = pry + e;
    float R = (orx > ory ? orx : ory) + 1;
    if (pcx + R < 0 || pcx - R > TW || pcy + R < 0 || pcy - R > TH) return;
    if (prx < 0.8f && pry < 0.8f) { px((int)pcx, (int)pcy, (flags & WB_OUT) ? o : c); return; }
    float cs = cosf(rot), sn = sinf(rot);
    uint8_t sh = wd_dk(c), li = wd_lt(c);
    int x0 = (int)floorf(pcx - R), x1 = (int)ceilf(pcx + R), y0 = (int)floorf(pcy - R), y1 = (int)ceilf(pcy + R);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > TW - 1) x1 = TW - 1;
    if (y1 > TH - 1) y1 = TH - 1;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - pcx, dy = y + 0.5f - pcy;
            float lx = dx * cs + dy * sn, ly = -dx * sn + dy * cs;
            if ((lx * lx) / (orx * orx) + (ly * ly) / (ory * ory) > 1.0f) continue;
            float d = (lx * lx) / (prx * prx) + (ly * ly) / (pry * pry);
            if (d > 1.0f) { tk_fb[y][x] = o; continue; }
            uint8_t col = c;
            if (flags & WB_SHADE) {
                /* light from the upper-left in SCREEN space */
                float nx = dx / (prx > pry ? prx : pry), ny = dy / (prx > pry ? prx : pry);
                float v = -0.55f * nx - 0.75f * ny + 0.55f * (1.0f - d);
                int t = (int)((v + 0.2f) * 16.0f);
                if (v < -0.18f) col = sh;
                else if (v < -0.02f && BAY[y & 3][x & 3] < (unsigned)(-t)) col = sh;
                if (v > 0.62f) col = li;
                if ((flags & WB_RIM) && d > 0.78f && nx > 0.35f && ny > -0.2f) col = li;
            }
            tk_fb[y][x] = col;
        }
}
void wd_blob(float cx, float cy, float rx, float ry, float rot, uint8_t c, int flags) {
    wd_blob_ol(cx, cy, rx, ry, rot, c, wd_ol(c), flags);
}

static int edge(float ax, float ay, float bx, float by, float px_, float py_) {
    return (bx - ax) * (py_ - ay) - (by - ay) * (px_ - ax) >= 0;
}
static void tri_s(float ax, float ay, float bx, float by, float qx, float qy, uint8_t c) {
    float minx = fminf(ax, fminf(bx, qx)), maxx = fmaxf(ax, fmaxf(bx, qx));
    float miny = fminf(ay, fminf(by, qy)), maxy = fmaxf(ay, fmaxf(by, qy));
    if (maxx < 0 || minx > TW || maxy < 0 || miny > TH) return;
    int x0 = (int)floorf(minx), x1 = (int)ceilf(maxx), y0 = (int)floorf(miny), y1 = (int)ceilf(maxy);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > TW - 1) x1 = TW - 1;
    if (y1 > TH - 1) y1 = TH - 1;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float X = x + 0.5f, Y = y + 0.5f;
            int e1 = edge(ax, ay, bx, by, X, Y), e2 = edge(bx, by, qx, qy, X, Y), e3 = edge(qx, qy, ax, ay, X, Y);
            if ((e1 && e2 && e3) || (!e1 && !e2 && !e3)) tk_fb[y][x] = c;
        }
}
void wd_tri(float ax, float ay, float bx, float by, float qx, float qy, uint8_t c) {
    tri_s(wd_sx(ax), wd_sy(ay), wd_sx(bx), wd_sy(by), wd_sx(qx), wd_sy(qy), c);
}
void wd_quad(float ax, float ay, float bx, float by, float qx, float qy, float dx, float dy, uint8_t c, uint8_t ol) {
    wd_tri(ax, ay, bx, by, qx, qy, c);
    wd_tri(ax, ay, qx, qy, dx, dy, c);
    if (ol) {
        tk_line((int)wd_sx(ax), (int)wd_sy(ay), (int)wd_sx(bx), (int)wd_sy(by), ol);
        tk_line((int)wd_sx(bx), (int)wd_sy(by), (int)wd_sx(qx), (int)wd_sy(qy), ol);
        tk_line((int)wd_sx(qx), (int)wd_sy(qy), (int)wd_sx(dx), (int)wd_sy(dy), ol);
        tk_line((int)wd_sx(dx), (int)wd_sy(dy), (int)wd_sx(ax), (int)wd_sy(ay), ol);
    }
}
void wd_rect(float x, float y, float w, float h, uint8_t c, uint8_t ol) {
    int x0 = (int)floorf(wd_sx(x)), y0 = (int)floorf(wd_sy(y));
    int x1 = (int)floorf(wd_sx(x + w)), y1 = (int)floorf(wd_sy(y + h));
    if (x1 <= x0) x1 = x0 + 1;
    if (y1 <= y0) y1 = y0 + 1;
    tk_rect(x0, y0, x1 - x0, y1 - y0, c);
    if (ol) tk_frame(x0, y0, x1 - x0, y1 - y0, ol);
}
void wd_line(float x0, float y0, float x1, float y1, float w, uint8_t c) {
    float sx0 = wd_sx(x0), sy0 = wd_sy(y0), sx1 = wd_sx(x1), sy1 = wd_sy(y1);
    float r = w * wd_z * 0.5f;
    if (r < 0.75f) { tk_line((int)sx0, (int)sy0, (int)sx1, (int)sy1, c); return; }
    float len = sqrtf((sx1 - sx0) * (sx1 - sx0) + (sy1 - sy0) * (sy1 - sy0));
    int n = (int)(len / (r * 0.5f + 0.5f)) + 1;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n;
        float cx = sx0 + (sx1 - sx0) * t, cy = sy0 + (sy1 - sy0) * t;
        for (int y = (int)(cy - r); y <= (int)(cy + r); y++)
            for (int x = (int)(cx - r); x <= (int)(cx + r); x++) {
                float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                if (dx * dx + dy * dy <= r * r) px(x, y, c);
            }
    }
}

void wd_glyph(char ch, uint8_t cols[5]) {
    /* render the game font glyph into a corner of the framebuffer, read it, restore */
    uint8_t save[8][6];
    for (int y = 0; y < 8; y++) memcpy(save[y], &tk_fb[y][0], 6);
    for (int y = 0; y < 8; y++) memset(&tk_fb[y][0], 2, 6);
    char s[2] = { ch, 0 };
    tk_text(0, 0, s, 0, 1);
    for (int x = 0; x < 5; x++) {
        cols[x] = 0;
        for (int y = 0; y < 8; y++) if (tk_fb[y][x] == 0) cols[x] |= (uint8_t)(1 << y);
    }
    for (int y = 0; y < 8; y++) memcpy(&tk_fb[y][0], save[y], 6);
}
float wd_text_w(const char *s, float p) { return ((float)strlen(s) * 6 - 1) * p; }
void wd_text(float x, float y, const char *s, float p, uint8_t c, uint8_t oc) {
    for (int pass = oc ? 0 : 1; pass < 2; pass++) {
        float cx = x;
        for (const char *q = s; *q; q++, cx += 6 * p) {
            if (*q == ' ') continue;
            uint8_t g[5];
            wd_glyph(*q, g);
            for (int col = 0; col < 5; col++)
                for (int row = 0; row < 8; row++)
                    if (g[col] >> row & 1) {
                        if (pass == 0) {
                            float e = p * 0.5f < 1.0f / wd_z ? 1.0f / wd_z : p * 0.5f;
                            wd_rect(cx + col * p - e, y + row * p - e, p + 2 * e, p + 2 * e, oc, 0);
                        } else wd_rect(cx + col * p, y + row * p, p, p, c, 0);
                    }
        }
    }
}
void wd_dither(float x, float y, float w, float h, uint8_t c, int level) {
    int x0 = (int)wd_sx(x), y0 = (int)wd_sy(y), x1 = (int)wd_sx(x + w), y1 = (int)wd_sy(y + h);
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
            if (BAY[yy & 3][xx & 3] < level) px(xx, yy, c);
}

void wd_post_crayon(int f, uint8_t paper) {
    static uint8_t tmp[TH][TW];
    memcpy(tmp, tk_fb, sizeof tmp);
    uint32_t step = (uint32_t)(f / 4);      /* lines "boil" at 7.5 fps, like a child's drawing */
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            uint32_t h = tk_hash((uint32_t)(x / 2) * 7919u + (uint32_t)(y / 2) * 104729u + step * 15485863u);
            int sx = x + (int)(h % 3) - 1, sy = y + (int)((h >> 3) % 3) - 1;
            if (sx < 0) sx = 0;
            if (sy < 0) sy = 0;
            if (sx >= TW) sx = TW - 1;
            if (sy >= TH) sy = TH - 1;
            uint8_t c = tmp[sy][sx];
            uint32_t g = tk_hash((uint32_t)x * 31u + (uint32_t)y * 977u + (step & 1));
            if (c != paper && (g % 100) < 16) c = paper;              /* waxy gaps */
            else if (c == paper && (g % 100) < 7) c = PAL_PAINT(PAINT_BONE, 1);   /* paper grain */
            tk_fb[y][x] = c;
        }
}
void wd_rot(float *x, float *y, float ox, float oy, float a) {
    float dx = *x - ox, dy = *y - oy, c = cosf(a), s = sinf(a);
    *x = ox + dx * c - dy * s;
    *y = oy + dx * s + dy * c;
}
float wd_spring(float t, float t0, float amp, float freq, float damp) {
    float d = t - t0;
    if (d < 0) return 0;
    return amp * expf(-damp * d) * sinf(freq * d);
}
float wd_clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
float wd_seg(float t, float a, float b) { return tk_ease((t - a) / (b - a)); }

/* a tapered limb with volume: 1 px dark outline, base tone, a mid band and a
 * thin highlight on the lit (upper-left) side, round caps at both ends */
void wd_tube(float ax, float ay, float bx, float by, float ra, float rb,
             uint8_t ol, uint8_t dark, uint8_t mid, uint8_t light) {
    float dx = bx - ax, dy = by - ay, L = sqrtf(dx * dx + dy * dy);
    if (L < 1e-4f) L = 1e-4f;
    float nx = -dy / L, ny = dx / L;
    if (nx + ny > 0) { nx = -nx; ny = -ny; }           /* normal toward the light */
    float o = 1.0f / wd_z;
    for (int pass = 0; pass < 4; pass++) {
        float off = pass < 2 ? 0 : pass == 2 ? 0.22f : 0.5f;
        float sc = pass == 0 ? 1 : pass == 1 ? 1 : pass == 2 ? 0.68f : 0.24f;
        float ea = pass == 0 ? o : 0;
        float r0 = ra * sc + ea, r1 = rb * sc + ea;
        float cx0 = ax + nx * ra * off, cy0 = ay + ny * ra * off, cx1 = bx + nx * rb * off, cy1 = by + ny * rb * off;
        uint8_t c = pass == 0 ? ol : pass == 1 ? dark : pass == 2 ? mid : light;
        if (pass == 3 && ra * wd_z < 1.6f) break;
        wd_quad(cx0 + nx * r0, cy0 + ny * r0, cx1 + nx * r1, cy1 + ny * r1, cx1 - nx * r1, cy1 - ny * r1, cx0 - nx * r0, cy0 - ny * r0, c, 0);
        if (pass < 2) { wd_blob(cx0, cy0, r0, r0, 0, c, 0); wd_blob(cx1, cy1, r1, r1, 0, c, 0); }
    }
}
