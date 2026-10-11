/* tk_chibi.c - plush critter-folk citizens for the trailer.
 *
 * Art direction: cute, lively, constantly animated, vaguely furry, and NOT
 * cheap pixel art. Characters are built from soft shaded blobs in a
 * resolution-independent "design space" and rasterised natively at the
 * requested size (so a 3x close-up has 3x the detail, not fat pixels).
 * Every blob gets 3-tone soft shading (hue-shifted shade, warm light, rim
 * light) and a coloured selective outline (a deep shade of its own colour).
 * The animation clock drives breathing, blinks, ear twitches, tail wags and
 * hair bounce, so nothing is ever static.
 */
#include "tk.h"
#include "pal.h"
#include <math.h>
#include <string.h>

/* design space: x -10..34, y -16..36 (feet at y = 33.5, centre x = 12) */
#define DX0 (-10.0f)
#define DY0 (-16.0f)
#define DW  44
#define DH  52
#define FEET_Y 33.5f
#define MAXS 4
#define CW (DW * MAXS)
#define CH (DH * MAXS)

static uint8_t cv[CH][CW];
static int S;            /* current scale (pixels per design unit) */
static int CWs, CHs;     /* active canvas size */

static const uint8_t BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

/* ---- palette shade helpers ------------------------------------------------ */
static int luma(uint8_t c) { return (TK_PAL[c][0] * 3 + TK_PAL[c][1] * 6 + TK_PAL[c][2]) / 10; }
static uint8_t dk(uint8_t c) {
    if (c == PAL_UI_CREAM) return PAL_PAINT(PAINT_BONE, 1);
    if (c == PAL_UI_PINK) return PAL_PAINT(PAINT_BUBBLEGUM, 1);
    if (c == PAL_UI_MINT) return PAL_PAINT(PAINT_MINT, 1);
    if (c >= PAL_PAINT_BASE && c < PAL_RAMP_BASE) { int k = (c - PAL_PAINT_BASE) % 3; return k ? (uint8_t)(c - 1) : (uint8_t)c; }
    if (c >= PAL_RAMP_BASE) { int k = (c - PAL_RAMP_BASE) % 8; return k ? (uint8_t)(c - 1) : (uint8_t)c; }
    if (c == PAL_WHITE) return PAL_GRAY_L;
    if (c == PAL_GRAY_L) return PAL_GRAY;
    if (c == PAL_GRAY) return PAL_GRAY_D;
    return PAL_BLACK;
}
static uint8_t lt(uint8_t c) {
    if (c == PAL_UI_CREAM) return PAL_WHITE;
    if (c >= PAL_PAINT_BASE && c < PAL_RAMP_BASE) { int k = (c - PAL_PAINT_BASE) % 3; return k < 2 ? (uint8_t)(c + 1) : PAL_WHITE; }
    if (c >= PAL_RAMP_BASE) { int k = (c - PAL_RAMP_BASE) % 8; return k < 7 ? (uint8_t)(c + 1) : PAL_WHITE; }
    if (c == PAL_GRAY_D) return PAL_GRAY;
    if (c == PAL_GRAY) return PAL_GRAY_L;
    return PAL_WHITE;
}
/* deep "selective outline" tone: two shades down, warm-dark if it bottoms out */
static uint8_t __attribute__((unused)) ol(uint8_t c) {
    uint8_t d = dk(dk(c));
    if (d == c || d == dk(c)) return PAL_PAINT(PAINT_JET, 1);
    if (c >= PAL_PAINT_BASE && c < PAL_RAMP_BASE) return PAL_PAINT(PAINT_JET, 2) == d ? d : (uint8_t)(dk(c) == c ? PAL_PAINT(PAINT_JET, 1) : d);
    return d;
}
static uint8_t outline_of(uint8_t c) {
    /* coloured selective outline: the darkest shade of the colour's own
     * family, pushed further down if that is still too light to read */
    uint8_t d = dk(dk(c));
    if (c >= PAL_PAINT_BASE && c < PAL_RAMP_BASE) d = (uint8_t)(c - (c - PAL_PAINT_BASE) % 3);
    if (luma(d) > 150) d = PAL_GRAY;
    if (luma(d) > 95 && luma(c) < 200) d = dk(d) != d ? dk(d) : PAL_GRAY_D;
    if (d == c) d = PAL_GRAY_D;
    return d;
}

/* ---- rasterising in design space ----------------------------------------- */
static inline void cpx(int x, int y, uint8_t c) {
    if ((unsigned)x < (unsigned)CWs && (unsigned)y < (unsigned)CHs) cv[y][x] = c;
}
static inline float PX(float x) { return (x - DX0) * S; }
static inline float PY(float y) { return (y - DY0) * S; }

/* Soft-shaded ellipse with its own coloured outline ring.
 * light from upper-left; `rim` adds a light rim on the lower-right edge. */
static void blob(float cx, float cy, float rx, float ry, uint8_t c, int outline, int rim) {
    float pcx = PX(cx), pcy = PY(cy), prx = rx * S, pry = ry * S;
    float orx = prx + (outline ? 1.0f : 0.0f), ory = pry + (outline ? 1.0f : 0.0f);
    uint8_t o = outline_of(c), sh = dk(c), li = lt(c);
    int x0 = (int)floorf(pcx - orx - 1), x1 = (int)ceilf(pcx + orx + 1);
    int y0 = (int)floorf(pcy - ory - 1), y1 = (int)ceilf(pcy + ory + 1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float fx = x + 0.5f - pcx, fy = y + 0.5f - pcy;
            float dout = (fx * fx) / (orx * orx) + (fy * fy) / (ory * ory);
            if (dout > 1.0f) continue;
            float d = (fx * fx) / (prx * prx) + (fy * fy) / (pry * pry);
            if (d > 1.0f) { cpx(x, y, o); continue; }
            float nx = fx / prx, ny = fy / pry;
            /* lambert-ish term from the upper-left + ambient bulge */
            float v = -0.55f * nx - 0.75f * ny + 0.55f * (1.0f - d);
            int t = (int)((v + 0.2f) * 16.0f);           /* dither position */
            uint8_t col = c;
            if (v < -0.18f) col = sh;
            else if (v < -0.02f && BAYER[y & 3][x & 3] < (unsigned)(-t)) col = sh;  /* soft fluff edge */
            if (v > 0.62f) col = li;
            if (rim && d > 0.78f && nx > 0.35f && ny > -0.2f) col = li;     /* rim light */
            cpx(x, y, col);
        }
}
/* flat ellipse (no shading), optional outline */
static void flat(float cx, float cy, float rx, float ry, uint8_t c, int outline) {
    float pcx = PX(cx), pcy = PY(cy), prx = rx * S, pry = ry * S;
    float orx = prx + (outline ? 1.0f : 0.0f), ory = pry + (outline ? 1.0f : 0.0f);
    uint8_t o = outline_of(c);
    for (int y = (int)(pcy - ory - 1); y <= (int)(pcy + ory + 1); y++)
        for (int x = (int)(pcx - orx - 1); x <= (int)(pcx + orx + 1); x++) {
            float fx = x + 0.5f - pcx, fy = y + 0.5f - pcy;
            if ((fx * fx) / (orx * orx) + (fy * fy) / (ory * ory) > 1.0f) continue;
            cpx(x, y, (fx * fx) / (prx * prx) + (fy * fy) / (pry * pry) > 1.0f ? o : c);
        }
}
static int in_tri(float px, float py, const float *v) {
    float d1 = (px - v[2]) * (v[1] - v[3]) - (v[0] - v[2]) * (py - v[3]);
    float d2 = (px - v[4]) * (v[3] - v[5]) - (v[2] - v[4]) * (py - v[5]);
    float d3 = (px - v[0]) * (v[5] - v[1]) - (v[4] - v[0]) * (py - v[1]);
    int neg = (d1 < 0) || (d2 < 0) || (d3 < 0), pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
}
/* triangle with coloured outline (inner tri shrunk toward the centroid) */
static void tri(float ax, float ay, float bx, float by, float qx, float qy, uint8_t c, int outline) {
    float v[6] = { PX(ax), PY(ay), PX(bx), PY(by), PX(qx), PY(qy) };
    float gx = (v[0] + v[2] + v[4]) / 3, gy = (v[1] + v[3] + v[5]) / 3;
    float in[6];
    float size = fabsf((v[2] - v[0]) * (v[5] - v[1]) - (v[4] - v[0]) * (v[3] - v[1]));
    float k = outline ? (1.0f - 2.2f / (sqrtf(size) + 1.0f)) : 1.0f;
    if (k < 0.2f) k = 0.2f;
    for (int i = 0; i < 3; i++) { in[i * 2] = gx + (v[i * 2] - gx) * k; in[i * 2 + 1] = gy + (v[i * 2 + 1] - gy) * k; }
    float minx = fminf(v[0], fminf(v[2], v[4])), maxx = fmaxf(v[0], fmaxf(v[2], v[4]));
    float miny = fminf(v[1], fminf(v[3], v[5])), maxy = fmaxf(v[1], fmaxf(v[3], v[5]));
    uint8_t o = outline_of(c), li = lt(c);
    for (int y = (int)miny; y <= (int)maxy; y++)
        for (int x = (int)minx; x <= (int)maxx; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            if (!in_tri(px, py, v)) continue;
            if (!in_tri(px, py, in)) { cpx(x, y, o); continue; }
            cpx(x, y, (py - gy) < -0.25f * (maxy - miny) && (px - gx) < 0 ? li : c);
        }
}
/* a fur tuft: small outlined spike leaving an ellipse edge at `ang` */
static void tuft(float cx, float cy, float rx, float ry, float ang, float len, float wid, uint8_t c) {
    float ex = cx + cosf(ang) * rx * 0.92f, ey = cy + sinf(ang) * ry * 0.92f;
    float tx = cx + cosf(ang) * (rx + len), ty = cy + sinf(ang) * (ry + len);
    float px = -sinf(ang) * wid, py = cosf(ang) * wid;
    tri(ex + px, ey + py, ex - px, ey - py, tx + px * 0.3f, ty + py * 0.3f, c, 1);
}
static void dot(float x, float y, float r, uint8_t c) { flat(x, y, r, r, c, 0); }
static void line_d(float x0, float y0, float x1, float y1, uint8_t c, float w) {
    float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    int n = (int)(len * S * 2) + 1;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n;
        float x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t;
        if (w * S <= 1.0f) cpx((int)PX(x), (int)PY(y), c);
        else flat(x, y, w * 0.5f, w * 0.5f, c, 0);
    }
}

/* ---- body parts -------------------------------------------------------------- */
static int is_critter(const TkChibi *c) { return c->ears != TKEAR_HUMAN && c->ears != TKEAR_NONE; }

static void draw_ears(const TkChibi *c, float hx, float hy, float twitchL, float twitchR, uint8_t fur, uint8_t inner) {
    int e = c->ears;
    for (int side = -1; side <= 1; side += 2) {
        float tw = side < 0 ? twitchL : twitchR;
        float bx = hx + side * 5.6f, by = hy - 5.4f;
        switch (e) {
        case TKEAR_CAT: case TKEAR_FOX: {
            float h = e == TKEAR_FOX ? 8.5f : 6.5f, w = e == TKEAR_FOX ? 3.4f : 3.0f;
            float ang = side * (0.32f + tw);
            float tx = bx + sinf(ang) * h, ty = by - cosf(ang) * h;
            tri(bx - w, by + 1.5f, bx + w, by + 1.5f, tx, ty, fur, 1);
            float ix = bx + sinf(ang) * h * 0.55f, iy = by - cosf(ang) * h * 0.55f;
            tri(bx - w * 0.45f, by + 0.6f, bx + w * 0.45f, by + 0.6f, ix, iy, inner, 0);
            if (e == TKEAR_FOX) tri(tx - sinf(ang) * 2.2f - 0.9f, ty + cosf(ang) * 2.2f, tx - sinf(ang) * 2.2f + 0.9f, ty + cosf(ang) * 2.2f, tx, ty, PAL_PAINT(PAINT_CHOCOLATE, 0), 0);
            break; }
        case TKEAR_BUNNY: {
            float ang = side * (0.18f + tw) , h = 11.0f;
            float tx = bx * 0.6f + hx * 0.4f + sinf(ang) * h, ty = by - cosf(ang) * h;
            float mx = (bx + tx) * 0.5f + side * 0.6f, my = (by + ty) * 0.5f;
            blob(mx, my, 2.3f, h * 0.5f, fur, 1, 1);
            flat(mx, my + 0.5f, 1.0f, h * 0.36f, inner, 0);
            break; }
        case TKEAR_BEAR: case TKEAR_MOUSE: {
            float r = e == TKEAR_MOUSE ? 4.2f : 2.9f;
            float ex = bx + side * (e == TKEAR_MOUSE ? 1.8f : 0.6f) + tw * side * 2, ey = by + (e == TKEAR_MOUSE ? -1.2f : 0.4f);
            blob(ex, ey, r, r, fur, 1, 1);
            flat(ex, ey + 0.3f, r * 0.55f, r * 0.55f, inner, 0);
            break; }
        case TKEAR_PUP: {
            float sw = tw * 3;
            blob(bx + side * 2.6f, hy - 1.0f + sw, 2.2f, 5.0f, dk(fur), 1, 0);
            break; }
        case TKEAR_SHEEP: {
            blob(bx + side * 3.2f, by + 3.2f + tw * 2, 3.2f, 1.6f, fur, 1, 0);
            flat(bx + side * 3.4f, by + 3.3f + tw * 2, 1.8f, 0.7f, inner, 0);
            break; }
        case TKEAR_OWL: {
            float ang = side * (0.5f + tw);
            tri(bx - 1.8f, by + 1.0f, bx + 1.8f, by + 1.0f, bx + sinf(ang) * 5, by - cosf(ang) * 5, fur, 1);
            break; }
        default: break;
        }
    }
}

static void draw_tail(const TkChibi *c, float wag, uint8_t fur, uint8_t fur2) {
    float bx = 16.5f, by = 27.5f;
    float ang = -0.9f + wag;
    switch (c->tail) {
    case TKT_DEVIL: {
        uint8_t t = PAL_PAINT(PAINT_CHERRY, 1);
        float px = bx, py = by;
        for (int i = 1; i <= 6; i++) {
            float a = ang - 0.35f * i * 0.5f;
            float nx = px + cosf(a) * 1.4f, ny = py + sinf(a) * 1.4f - 0.2f * i;
            line_d(px, py, nx, ny, t, 1.0f); px = nx; py = ny;
        }
        tri(px - 1.6f, py + 0.5f, px + 1.6f, py + 0.5f, px + 0.2f, py - 2.6f, t, 1);
        break; }
    case TKT_FOX: case TKT_CAT: case TKT_RACCOON: case TKT_PUP: {
        float len = c->tail == TKT_FOX ? 7.5f : c->tail == TKT_CAT ? 6.5f : 5.5f;
        float r = c->tail == TKT_FOX ? 3.4f : c->tail == TKT_CAT ? 1.4f : 2.4f;
        float tx = bx + cosf(ang) * len, ty = by + sinf(ang) * len;
        int n = 16;
        /* pass 1: one shared outline for the whole tail silhouette */
        for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i <= n; i++) {
                float t = (float)i / n;
                float cx = bx + (tx - bx) * t + sinf(t * 3.1f + wag) * 1.2f, cy = by + (ty - by) * t;
                float rr = r * (c->tail == TKT_FOX ? (0.55f + 0.65f * sinf(t * 2.9f + 0.1f)) : 1.0f);
                if (rr < 0.8f) rr = 0.8f;
                uint8_t col = fur;
                if (c->tail == TKT_FOX && t > 0.72f) col = fur2;
                if (c->tail == TKT_RACCOON && ((int)(t * 6)) & 1) col = PAL_PAINT(PAINT_CHARCOAL, 1);
                if (pass == 0) flat(cx, cy, rr + 1.0f / S, rr + 1.0f / S, outline_of(fur), 0);
                else blob(cx, cy, rr, rr, col, 0, 1);
            }
        break; }
    case TKT_BUNNY: case TKT_SHEEP: case TKT_BEAR:
        blob(bx + 0.8f, by + 0.5f + wag, 2.4f, 2.2f, c->tail == TKT_BEAR ? fur : PAL_WHITE, 1, 1);
        break;
    default: break;
    }
}

static void draw_eyes(int expr, uint8_t iris, int blink, float hx, float hy, int critter) {
    const float ex[2] = { hx - 3.9f, hx + 3.9f };
    float ey = hy + 0.6f;
    uint8_t k = PAL_PAINT(PAINT_JET, 0);
    for (int e = 0; e < 2; e++) {
        float x = ex[e], y = ey;
        int side = e ? 1 : -1;
        switch (expr) {
        case TKE_HAPPY:
            for (int i = -2; i <= 2; i++) line_d(x + i * 0.45f, y - 0.9f + fabsf((float)i) * 0.35f, x + (i + 1) * 0.45f, y - 0.9f + fabsf((float)(i + 1)) * 0.35f, k, 0.7f);
            break;
        case TKE_SMUG:
            line_d(x - 1.6f, y, x + 1.6f, y - 0.3f * side, k, 0.8f);
            flat(x + 0.3f, y + 0.7f, 1.1f, 0.6f, iris, 0);
            break;
        case TKE_SLEEPY:
            line_d(x - 1.5f, y + 0.4f, x + 1.5f, y + 0.4f, k, 0.7f);
            break;
        case TKE_DEAD:
            line_d(x - 1.3f, y - 1.3f, x + 1.3f, y + 1.3f, k, 0.8f);
            line_d(x - 1.3f, y + 1.3f, x + 1.3f, y - 1.3f, k, 0.8f);
            break;
        case TKE_LOVE: {
            uint8_t h = PAL_PAINT(PAINT_CHERRY, 1);
            dot(x - 0.75f, y - 0.6f, 1.0f, h); dot(x + 0.75f, y - 0.6f, 1.0f, h);
            tri(x - 1.75f, y - 0.4f, x + 1.75f, y - 0.4f, x, y + 1.9f, h, 0);
            dot(x - 0.8f, y - 0.9f, 0.35f, PAL_WHITE);
            break; }
        case TKE_SHOCK:
            flat(x, y, 1.9f, 2.3f, PAL_WHITE, 1);
            dot(x, y, 0.55f, k);
            break;
        case TKE_POSSESSED:
            flat(x, y, 2.0f, 2.4f, PAL_PAINT(PAINT_JET, 0), 1);
            dot(x, y + 0.2f, 0.7f, PAL_PAINT(PAINT_CHERRY, 2));
            line_d(x, y + 2.4f, x + 0.2f * side, y + 5.0f, PAL_PAINT(PAINT_JET, 0), 0.6f);
            break;
        case TKE_ANGRY:
            flat(x, y + 0.6f, 1.7f, 1.5f, k, 0);
            flat(x, y + 0.9f, 1.2f, 1.0f, iris, 0);
            dot(x - 0.5f * side, y + 0.3f, 0.35f, PAL_WHITE);
            line_d(x - 2.0f * side, y - 1.9f, x + 1.6f * side, y - 0.6f, k, 0.8f);
            break;
        case TKE_SAD:
            flat(x, y, 1.7f, 2.0f, k, 0);
            flat(x, y + 0.5f, 1.2f, 1.3f, iris, 0);
            dot(x - 0.6f, y - 0.6f, 0.5f, PAL_WHITE);
            line_d(x - 1.8f * side, y - 2.4f, x + 1.2f * side, y - 3.0f, k, 0.6f);
            flat(x + 1.0f * side, y + 2.8f, 0.5f, 0.9f, PAL_PAINT(PAINT_SKY, 2), 0);
            break;
        default: {
            if (blink) { line_d(x - 1.6f, y + 0.5f, x + 1.6f, y + 0.5f, k, 0.8f); break; }
            float sz = expr == TKE_SPARKLE ? 1.15f : 1.0f;
            flat(x, y, 1.85f * sz, 2.35f * sz, k, 0);               /* dark rim   */
            flat(x, y + 0.35f, 1.4f * sz, 1.85f * sz, iris, 0);      /* iris       */
            flat(x, y + 1.0f, 1.1f * sz, 1.0f * sz, lt(iris), 0);    /* glow       */
            dot(x, y + 0.2f, 0.75f * sz, k);                         /* pupil      */
            dot(x - 0.75f * sz, y - 0.9f * sz, 0.62f * sz, PAL_WHITE);  /* big shine  */
            dot(x + 0.7f * sz, y + 1.0f * sz, 0.32f * sz, PAL_WHITE);   /* small shine*/
            if (expr == TKE_SPARKLE) { dot(x + 0.9f, y - 1.2f, 0.4f, PAL_WHITE); }
            break; }
        }
        (void)critter;
    }
}

static void draw_mouth(int m, float x, float y) {
    uint8_t k = PAL_PAINT(PAINT_JET, 0), in = PAL_PAINT(PAINT_CHERRY, 0), tongue = PAL_PAINT(PAINT_ROSE, 1);
    switch (m) {
    case TKM_OPEN:
        flat(x, y + 0.6f, 1.5f, 1.2f, in, 1);
        flat(x, y + 1.2f, 0.9f, 0.5f, tongue, 0);
        break;
    case TKM_FROWN:
        line_d(x - 1.2f, y + 0.9f, x, y + 0.2f, k, 0.6f); line_d(x, y + 0.2f, x + 1.2f, y + 0.9f, k, 0.6f);
        break;
    case TKM_FLAT: line_d(x - 1.0f, y + 0.4f, x + 1.0f, y + 0.4f, k, 0.6f); break;
    case TKM_FANGS:
        flat(x, y + 0.4f, 1.9f, 0.9f, in, 1);
        tri(x - 1.2f, y, x - 0.4f, y, x - 0.8f, y + 1.3f, PAL_WHITE, 0);
        tri(x + 0.4f, y, x + 1.2f, y, x + 0.8f, y + 1.3f, PAL_WHITE, 0);
        break;
    case TKM_O: flat(x, y + 0.6f, 0.8f, 1.0f, in, 1); break;
    case TKM_SCREAM:
        flat(x, y + 1.2f, 2.0f, 2.2f, in, 1);
        flat(x, y + 2.2f, 1.2f, 0.8f, tongue, 0);
        break;
    case TKM_CAT: default:
        /* the classic critter "w" mouth (smile = soft w) */
        line_d(x - 1.6f, y + 0.1f, x - 0.8f, y + 0.8f, k, 0.55f);
        line_d(x - 0.8f, y + 0.8f, x, y + 0.1f, k, 0.55f);
        line_d(x, y + 0.1f, x + 0.8f, y + 0.8f, k, 0.55f);
        line_d(x + 0.8f, y + 0.8f, x + 1.6f, y + 0.1f, k, 0.55f);
        if (m == TKM_SMILE) flat(x, y + 1.0f, 0.5f, 0.35f, tongue, 0);
        break;
    }
}

static void draw_hair(const TkChibi *c, float hx, float hy, float bounce, int back) {
    uint8_t h = c->hair;
    int critter = is_critter(c);
    switch (c->hair_style) {
    case TKH_BALD: break;
    case TKH_SHORT: case TKH_BOB: case TKH_LONG: case TKH_TWINTAILS: case TKH_BUN:
        if (back) {
        if (c->hair_style == TKH_LONG || c->hair_style == TKH_BOB) {
            float len = c->hair_style == TKH_LONG ? 13.0f : 7.0f;
            blob(hx - 6.8f, hy + len * 0.4f + bounce * 0.5f, 3.2f, len * 0.62f, h, 1, 0);
            blob(hx + 6.8f, hy + len * 0.4f + bounce * 0.5f, 3.2f, len * 0.62f, h, 1, 1);
        }
        if (c->hair_style == TKH_TWINTAILS) {
            blob(hx - 10.2f, hy + 3.5f + bounce, 2.6f, 5.0f, h, 1, 0);
            blob(hx + 10.2f, hy + 3.5f + bounce, 2.6f, 5.0f, h, 1, 1);
            dot(hx - 8.4f, hy - 1.0f, 1.0f, PAL_PAINT(PAINT_BUBBLEGUM, 1));
            dot(hx + 8.4f, hy - 1.0f, 1.0f, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        }
        if (c->hair_style == TKH_BUN) blob(hx, hy - 9.0f + bounce * 0.5f, 3.4f, 2.8f, h, 1, 1);
        break;
        }
        /* cap + fringe tufts */
        blob(hx, hy - 4.2f, critter ? 6.0f : 8.6f, critter ? 3.0f : 4.6f, h, 1, 1);
        for (int i = -2; i <= 2; i++)
            tri(hx + i * 2.2f - 1.5f, hy - 4.0f, hx + i * 2.2f + 1.5f, hy - 4.0f, hx + i * 2.2f + 0.6f, hy - 1.2f + (i & 1) * 0.8f, h, 0);
        break;
    case TKH_SPIKY:
        if (back) break;
        for (int i = -3; i <= 3; i++) {
            float a = -1.5708f + i * 0.33f;
            tuft(hx, hy - 1.0f, 8.0f, 7.5f, a, 3.0f + (i & 1) * 1.5f + bounce * 0.4f, 1.6f, h);
        }
        blob(hx, hy - 4.5f, 7.6f, 3.8f, h, 1, 1);
        break;
    case TKH_AFRO:
        if (!back) break;
        blob(hx, hy - 4.0f + bounce * 0.4f, 11.5f, 8.0f, h, 1, 1);
        for (int i = 0; i < 9; i++) tuft(hx, hy - 4.0f + bounce * 0.4f, 11.0f, 7.6f, -3.1416f + i * 0.39f, 1.2f, 1.6f, h);
        break;
    case TKH_MOHAWK:
        if (!back) break;
        for (int i = -2; i <= 2; i++) tri(hx + i * 1.4f - 1.2f, hy - 6.0f, hx + i * 1.4f + 1.2f, hy - 6.0f, hx + i * 1.6f, hy - 12.0f - (i == 0) * 2 + bounce, h, 1);
        break;
    default: break;
    }
}

/* ---- the citizen ------------------------------------------------------------ */
void tk_chibi(int x, int y_feet, const TkChibi *c, int size, int expr, int mouth, int bob, int flip) {
    S = size < 1 ? 1 : size > MAXS ? MAXS : size;
    CWs = DW * S; CHs = DH * S;
    for (int y = 0; y < CHs; y++) memset(cv[y], 0, (size_t)CWs);

    int poss = expr == TKE_POSSESSED;
    int critter = is_critter(c);
    uint8_t fur = c->skin;
    uint8_t fur2 = c->fur2 ? c->fur2 : (critter ? PAL_PAINT(PAINT_BONE, 2) : lt(c->skin));
    uint8_t inner = critter ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : dk(c->skin);
    if (poss) { fur = PAL_GRAY_L; fur2 = PAL_GRAY; inner = PAL_PAINT(PAINT_GRAPE, 0); }

    /* animation clock */
    float t = (float)bob;
    float breathe = sinf(t * 0.16f);                       /* -1..1 */
    float hb = breathe * 0.45f;                            /* head bob */
    float bounce = sinf(t * 0.16f - 0.9f) * 0.6f;          /* hair lags the head */
    float wag = sinf(t * 0.28f) * 0.35f;
    int cyc = (bob % 97 + 97) % 97;
    float twL = (cyc >= 40 && cyc < 46) ? 0.22f * sinf((cyc - 40) * 1.05f) : 0.0f;
    float twR = (cyc >= 70 && cyc < 76) ? -0.22f * sinf((cyc - 70) * 1.05f) : 0.0f;
    int blink = (bob % 83 + 83) % 83 >= 80;
    if (poss) { twL = 0.05f * sinf(t * 1.7f); twR = -twL; }

    float hx = 12.0f, hy = 12.2f + hb;
    float bx = 12.0f, by = 25.0f;

    /* wings (behind everything) */
    if (c->wings == 1) {
        uint8_t w = PAL_PAINT(PAINT_GRAPE, 0);
        float flap = sinf(t * 0.3f) * 1.5f;
        for (int s = -1; s <= 1; s += 2) {
            tri(bx + s * 3, by - 3, bx + s * 11, by - 9 + flap, bx + s * 10, by + 1, w, 1);
            tri(bx + s * 3, by - 1, bx + s * 10, by + 1, bx + s * 7, by + 3, w, 1);
        }
    } else if (c->wings == 2) {
        float flap = sinf(t * 0.25f) * 1.2f;
        for (int s = -1; s <= 1; s += 2) {
            blob(bx + s * 8.0f, by - 3.5f + flap, 4.2f, 3.0f, PAL_WHITE, 1, 0);
            blob(bx + s * 10.0f, by - 1.0f + flap, 2.6f, 2.0f, PAL_WHITE, 1, 0);
        }
    }
    draw_tail(c, wag, fur, fur2);

    /* feet + legs */
    blob(bx - 2.8f, FEET_Y - 1.7f, 2.4f, 1.7f, critter ? fur : PAL_GRAY_D, 1, 0);
    blob(bx + 2.8f, FEET_Y - 1.7f, 2.4f, 1.7f, critter ? fur : PAL_GRAY_D, 1, 1);
    if (critter) { dot(bx - 2.8f, FEET_Y - 1.4f, 0.9f, inner); dot(bx + 2.8f, FEET_Y - 1.4f, 0.9f, inner); }

    /* body: plush round torso in the outfit, chest fluff on top */
    float bry = 5.2f + breathe * 0.25f;
    blob(bx, by, 5.8f, bry, c->outfit, 1, 1);
    flat(bx, by + bry - 1.2f, 4.6f, 0.6f, dk(c->outfit), 0);       /* hem shade */
    if (critter && c->fluff) {
        for (int i = -1; i <= 1; i++) tri(bx + i * 1.6f - 1.3f, by - bry + 0.6f, bx + i * 1.6f + 1.3f, by - bry + 0.6f, bx + i * 1.6f, by - bry + 3.0f, fur2, 0);
    }
    /* arms / paws, swinging a little */
    float sw = sinf(t * 0.16f + 1.0f) * 0.5f;
    blob(bx - 6.4f, by + 0.6f + sw, 1.9f, 2.3f, fur, 1, 0);
    blob(bx + 6.4f, by + 0.6f - sw, 1.9f, 2.3f, fur, 1, 1);
    if (critter) { dot(bx - 6.4f, by + 1.6f + sw, 0.7f, inner); dot(bx + 6.4f, by + 1.6f - sw, 0.7f, inner); }

    /* ears go behind the head */
    if (critter) draw_ears(c, hx, hy, twL, twR, fur, inner);
    else if (c->ears != TKEAR_NONE) {
        blob(hx - 8.6f, hy + 1.0f, 1.5f, 2.0f, fur, 1, 0);
        blob(hx + 8.6f, hy + 1.0f, 1.5f, 2.0f, fur, 1, 1);
    }
    /* cheek fluff tufts on the head silhouette */
    if (c->fluff) {
        static const float ang[] = { 2.62f, 3.02f, 0.52f, 0.12f };
        for (int i = 0; i < 4; i++) tuft(hx, hy + 0.8f, 8.8f, 7.6f, ang[i], 1.4f + (i & 1) * 0.5f, 2.0f, fur);
        tuft(hx, hy, 8.5f, 8.0f, -1.5708f + 0.15f, 2.0f + bounce * 0.3f, 1.4f, fur);   /* top tuft */
        tuft(hx, hy, 8.5f, 8.0f, -1.5708f - 0.35f, 1.4f, 1.2f, fur);
    }
    draw_hair(c, hx, hy, bounce, 1);
    /* head */
    blob(hx, hy, 9.2f, 8.2f, fur, 1, 1);
    if (critter) {
        blob(hx, hy + 4.1f, 3.7f, 2.6f, fur2, 0, 0);                 /* muzzle */
        flat(hx - 2.6f, hy + 3.4f, 1.4f, 1.2f, fur2, 0);             /* fluffy cheeks */
        flat(hx + 2.6f, hy + 3.4f, 1.4f, 1.2f, fur2, 0);
    }
    draw_hair(c, hx, hy, bounce, 0);

    /* face */
    draw_eyes(expr, c->eyes, blink, hx, hy, critter);
    if (c->blush && !poss) {
        for (int s = -1; s <= 1; s += 2) {
            float cx = hx + s * 6.0f, cy = hy + 3.0f;
            for (int yy = (int)PY(cy - 0.8f); yy <= (int)PY(cy + 0.8f); yy++)
                for (int xx = (int)PX(cx - 1.6f); xx <= (int)PX(cx + 1.6f); xx++)
                    if (BAYER[yy & 3][xx & 3] < 10) cpx(xx, yy, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        }
    }
    if (critter) {
        tri(hx - 1.1f, hy + 2.6f, hx + 1.1f, hy + 2.6f, hx, hy + 3.7f, c->ears == TKEAR_BEAR || c->ears == TKEAR_PUP ? PAL_PAINT(PAINT_CHOCOLATE, 0) : PAL_PAINT(PAINT_ROSE, 0), 0);
        dot(hx - 0.35f, hy + 2.85f, 0.3f, PAL_WHITE);
        draw_mouth(mouth == TKM_SMILE ? TKM_CAT : mouth, hx, hy + 4.0f);
    } else {
        draw_mouth(mouth, hx, hy + 3.4f);
    }
    if (poss) {
        for (int i = 0; i < 4; i++) line_d(hx - 7 + i * 0.6f, hy - 3 + i, hx - 5 + i * 0.6f, hy - 1 + i, PAL_PAINT(PAINT_GRAPE, 0), 0.5f);
        for (int i = 0; i < 4; i++) line_d(hx + 7 - i * 0.6f, hy - 3 + i, hx + 5 - i * 0.6f, hy - 1 + i, PAL_PAINT(PAINT_GRAPE, 0), 0.5f);
    }

    /* horns / halo / hats */
    if (c->horns == 1) {
        uint8_t h = PAL_PAINT(PAINT_CHERRY, 0);
        tri(hx - 6.2f, hy - 5.0f, hx - 3.8f, hy - 6.4f, hx - 7.4f, hy - 10.5f, h, 1);
        tri(hx + 6.2f, hy - 5.0f, hx + 3.8f, hy - 6.4f, hx + 7.4f, hy - 10.5f, h, 1);
    } else if (c->horns == 2) {
        uint8_t h = PAL_RAMP(RAMP_BONE, 5);
        blob(hx - 8.5f, hy - 2.5f, 2.6f, 3.0f, h, 1, 1); flat(hx - 8.5f, hy - 2.3f, 1.1f, 1.3f, PAL_RAMP(RAMP_BONE, 3), 0);
        blob(hx + 8.5f, hy - 2.5f, 2.6f, 3.0f, h, 1, 1); flat(hx + 8.5f, hy - 2.3f, 1.1f, 1.3f, PAL_RAMP(RAMP_BONE, 3), 0);
    }
    if (c->halo) {
        float hy2 = hy - 12.5f + sinf(t * 0.1f) * 0.6f;
        flat(hx, hy2, 5.0f, 1.4f, PAL_UI_GOLD, 1);
        flat(hx, hy2, 3.4f, 0.6f, 0, 0);
    }
    if (c->hat == 1) {
        for (int i = 0; i < 7; i++) {
            float w = 0.5f + i * 0.55f;
            uint8_t col = (i & 1) ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_PAINT(PAINT_LEMON, 1);
            flat(hx + 1.0f, hy - 15.0f + i * 1.2f, w, 0.7f, col, 0);
        }
        dot(hx + 1.0f, hy - 15.8f, 1.0f, PAL_WHITE);
    } else if (c->hat == 2) {
        blob(hx, hy - 6.5f, 8.0f, 2.4f, PAL_PAINT(PAINT_JET, 1), 1, 1);
        blob(hx, hy - 8.8f, 5.0f, 2.6f, PAL_PAINT(PAINT_JET, 1), 1, 1);
        flat(hx, hy - 6.6f, 1.5f, 1.0f, PAL_WHITE, 0);
    } else if (c->hat == 3) {
        blob(hx, hy - 4.5f, 9.8f, 5.6f, PAL_PAINT(PAINT_FOREST, 1), 1, 1);
        flat(hx, hy - 1.2f, 9.6f, 0.8f, PAL_PAINT(PAINT_FOREST, 0), 0);
    }

    /* blit: feet at y_feet, centred on x */
    int left = x - (int)((12.0f - DX0) * S);
    int top = y_feet - (int)((FEET_Y + 0.6f - DY0) * S);
    for (int yy = 0; yy < CHs; yy++)
        for (int xx = 0; xx < CWs; xx++) {
            uint8_t v = cv[yy][xx];
            if (!v) continue;
            int sx = flip ? (int)(2 * (12.0f - DX0) * S) - 1 - xx : xx;
            tk_px(left + sx, top + yy, v);
        }
}

/* ---- the cast ------------------------------------------------------------------ */
#define CAST(sk, ha, ou, ey, hs, ho, wi, hl, ta, bl, hat, ear, f2, fl) \
    { sk, ha, ou, ey, hs, ho, wi, hl, ta, bl, hat, ear, f2, fl }
/* hero: a fluffy fox kid in a mint hoodie */
const TkChibi TK_HERO = CAST(PAL_PAINT(PAINT_TANGERINE,1), PAL_PAINT(PAINT_TANGERINE,1), PAL_PAINT(PAINT_MINT,1), PAL_PAINT(PAINT_COBALT,1), TKH_BALD, 0,0,0, TKT_FOX, 1, 0, TKEAR_FOX, PAL_PAINT(PAINT_BONE,2), 1);
/* Mayor Gigglesworth: a cherub sheep with a halo and tiny angel wings */
const TkChibi TK_MAYOR = CAST(PAL_PAINT(PAINT_WHITE,1), PAL_PAINT(PAINT_WHITE,2), PAL_PAINT(PAINT_LEMON,1), PAL_PAINT(PAINT_SKY,1), TKH_AFRO, 0,2,1, TKT_SHEEP, 1, 0, TKEAR_SHEEP, PAL_SKIN(SKIN_PEACH,2), 1);
/* Father Fuzz: a teddy-bear priest */
const TkChibi TK_FUZZ = CAST(PAL_PAINT(PAINT_CARAMEL,1), PAL_PAINT(PAINT_CARAMEL,1), PAL_PAINT(PAINT_JET,1), PAL_PAINT(PAINT_JET,1), TKH_BALD, 0,0,0, TKT_BEAR, 1, 2, TKEAR_BEAR, PAL_PAINT(PAINT_HONEY,2), 1);
/* Rivet: a scrappy red imp-raccoon welder */
const TkChibi TK_RIVET = CAST(PAL_PAINT(PAINT_CHERRY,1), PAL_PAINT(PAINT_CHARCOAL,1), PAL_PAINT(PAINT_TANGERINE,1), PAL_PAINT(PAINT_LEMON,1), TKH_SPIKY, 1,0,0, TKT_RACCOON, 0, 0, TKEAR_CAT, PAL_PAINT(PAINT_PEACH,2), 1);
/* Nana Pudding: a bunny granny baker */
const TkChibi TK_NANA = CAST(PAL_PAINT(PAINT_WHITE,1), PAL_PAINT(PAINT_SILVER,2), PAL_PAINT(PAINT_ROSE,1), PAL_PAINT(PAINT_CHOCOLATE,1), TKH_BUN, 0,0,0, TKT_BUNNY, 1, 0, TKEAR_BUNNY, PAL_PAINT(PAINT_BUBBLEGUM,2), 1);
/* Pip: a tiny mouse kid */
const TkChibi TK_PIP = CAST(PAL_PAINT(PAINT_SILVER,1), PAL_PAINT(PAINT_TANGERINE,1), PAL_PAINT(PAINT_SKY,1), PAL_PAINT(PAINT_FOREST,1), TKH_BALD, 0,0,0, 0, 1, 0, TKEAR_MOUSE, PAL_PAINT(PAINT_BUBBLEGUM,2), 1);
/* Vex: a sly lavender cat with little devil horns */
const TkChibi TK_VEX = CAST(PAL_PAINT(PAINT_LAVENDER,1), PAL_PAINT(PAINT_VAPE_PURPLE,1), PAL_PAINT(PAINT_JET,1), PAL_PAINT(PAINT_MAGENTA,1), TKH_BOB, 1,0,0, TKT_CAT, 0, 0, TKEAR_CAT, PAL_PAINT(PAINT_GRAPE,2), 1);
/* a goth black cat with bat wings */
const TkChibi TK_GOTH = CAST(PAL_PAINT(PAINT_CHARCOAL,1), PAL_PAINT(PAINT_JET,1), PAL_PAINT(PAINT_JET,1), PAL_PAINT(PAINT_CHERRY,1), TKH_BOB, 0,1,0, TKT_CAT, 1, 0, TKEAR_CAT, PAL_PAINT(PAINT_SILVER,1), 1);
/* an imp: red, horned, bat-winged, pointy-eared */
const TkChibi TK_IMP = CAST(PAL_PAINT(PAINT_CHERRY,1), PAL_PAINT(PAINT_JET,1), PAL_PAINT(PAINT_CHARCOAL,1), PAL_PAINT(PAINT_LEMON,1), TKH_BALD, 1,1,0, TKT_DEVIL, 0, 0, TKEAR_HUMAN, 0, 0);

TkChibi tk_random_citizen(uint32_t i, int bad) {
    uint32_t s = tk_hash(i * 2654435761u + (bad ? 99991u : 7u));
    TkChibi c;
    memset(&c, 0, sizeof c);
    static const int good_fur[] = { PAINT_WHITE, PAINT_CARAMEL, PAINT_TANGERINE, PAINT_SILVER, PAINT_PEACH,
                                    PAINT_HONEY, PAINT_CHOCOLATE, PAINT_BUBBLEGUM, PAINT_LAVENDER, PAINT_SKY, PAINT_MINT };
    static const int bad_fur[] = { PAINT_CHERRY, PAINT_CHARCOAL, PAINT_GRAPE, PAINT_JET, PAINT_SLIME,
                                   PAINT_RUST, PAINT_VAPE_PURPLE, PAINT_BONE, PAINT_NAVY };
    static const int good_out[] = { PAINT_BUBBLEGUM, PAINT_MINT, PAINT_SKY, PAINT_LEMON, PAINT_LAVENDER,
                                    PAINT_PEACH, PAINT_ROSE, PAINT_LIME, PAINT_CYAN, PAINT_HONEY, PAINT_WHITE };
    static const int bad_out[] = { PAINT_JET, PAINT_CHARCOAL, PAINT_BLOOD, PAINT_GRAPE, PAINT_VAPE_PURPLE,
                                   PAINT_RUST, PAINT_SLIME, PAINT_HELLFIRE, PAINT_NAVY, PAINT_FOREST };
    static const int good_ears[] = { TKEAR_CAT, TKEAR_FOX, TKEAR_BUNNY, TKEAR_BEAR, TKEAR_PUP, TKEAR_MOUSE, TKEAR_SHEEP, TKEAR_OWL, TKEAR_HUMAN };
    static const int bad_ears[]  = { TKEAR_CAT, TKEAR_FOX, TKEAR_HUMAN, TKEAR_HUMAN, TKEAR_MOUSE, TKEAR_BEAR, TKEAR_PUP };
    int ear = bad ? bad_ears[tk_rand(&s, 7)] : good_ears[tk_rand(&s, 9)];
    c.ears = (uint8_t)ear;
    int furc = bad ? bad_fur[tk_rand(&s, 9)] : good_fur[tk_rand(&s, 11)];
    c.skin = ear == TKEAR_HUMAN ? (uint8_t)(bad && tk_rand(&s, 2) ? PAL_PAINT(furc, 1) : PAL_SKIN(tk_rand(&s, 8), 1)) : (uint8_t)PAL_PAINT(furc, 1);
    c.fur2 = (uint8_t)(tk_rand(&s, 3) ? PAL_PAINT(PAINT_BONE, 2) : PAL_PAINT(PAINT_WHITE, 1));
    c.hair = (uint8_t)PAL_PAINT(tk_rand(&s, 32), 1);
    c.outfit = (uint8_t)PAL_PAINT(bad ? bad_out[tk_rand(&s, 10)] : good_out[tk_rand(&s, 11)], 1);
    c.eyes = (uint8_t)PAL_PAINT(bad ? (tk_rand(&s, 2) ? PAINT_CHERRY : PAINT_LEMON) : tk_rand(&s, 32), 1);
    c.hair_style = (uint8_t)(ear == TKEAR_HUMAN ? 1 + tk_rand(&s, 8) : (tk_rand(&s, 3) ? TKH_BALD : 1 + tk_rand(&s, 8)));
    c.horns = (uint8_t)(bad ? 1 + tk_rand(&s, 2) : (tk_rand(&s, 12) == 0));
    c.wings = (uint8_t)(bad ? (tk_rand(&s, 3) == 0) : (tk_rand(&s, 14) == 0 ? 2 : 0));
    c.halo = (uint8_t)(!bad && tk_rand(&s, 16) == 0);
    static const int tails_for_ear[] = { TKT_NONE, TKT_CAT, TKT_FOX, TKT_BUNNY, TKT_BEAR, TKT_PUP, TKT_CAT, TKT_SHEEP, TKT_NONE, TKT_NONE };
    c.tail = (uint8_t)(bad && tk_rand(&s, 2) ? TKT_DEVIL : tails_for_ear[ear]);
    c.blush = (uint8_t)(!bad || tk_rand(&s, 4) == 0);
    c.hat = (uint8_t)(tk_rand(&s, 10) == 0 ? 1 : 0);
    c.fluff = (uint8_t)(ear != TKEAR_HUMAN);
    return c;
}
