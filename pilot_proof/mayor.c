/* mayor.c - MAYOR GIGGS, drawn natively in the toolkit style.
 *
 * A round turkey with Mad Hatter energy: tall lavender top hat (no card),
 * wild white hair, peach head, orange beak, red wattle on a spring, teal tail
 * coat, magenta bow tie, wing-hands and a tail fan that opens on emotions.
 * Local design space: feet at (0,0), y up is negative, about 66 units tall
 * to the hat top. Everything is drawn through the world camera (wd.h).
 */
#include "mayor.h"
#include "wd.h"
#include <math.h>

float mayor_hand_x[2], mayor_hand_y[2];
float mayor_head_x, mayor_head_y;

static const Mayor *M;
static float sqx, sqy;   /* squash */

/* local -> world */
static float WX(float lx, float ly) {
    float sx = lx * sqx - ly * M->lean;           /* lean = shear of the upper body */
    return M->x + sx * M->flip * M->s;
}
static float WY(float lx, float ly) { (void)lx; return M->y + ly * sqy * M->s; }
static float WR(float r) { return r * M->s; }
static void B(float lx, float ly, float rx, float ry, float rot, uint8_t c, int fl) {
    wd_blob(WX(lx, ly), WY(lx, ly), WR(rx) * sqx, WR(ry) * sqy, rot * M->flip, c, fl);
}
static void BO(float lx, float ly, float rx, float ry, float rot, uint8_t c, uint8_t ol, int fl) {
    wd_blob_ol(WX(lx, ly), WY(lx, ly), WR(rx) * sqx, WR(ry) * sqy, rot * M->flip, c, ol, fl);
}
static void T(float ax, float ay, float bx, float by, float cx, float cy, uint8_t c) {
    wd_tri(WX(ax, ay), WY(ax, ay), WX(bx, by), WY(bx, by), WX(cx, cy), WY(cx, cy), c);
}
static void L(float ax, float ay, float bx, float by, float w, uint8_t c) {
    wd_line(WX(ax, ay), WY(ax, ay), WX(bx, by), WY(bx, by), WR(w), c);
}

#define TEAL   PAL_PAINT(PAINT_TEAL, 1)
#define TEALD  PAL_PAINT(PAINT_TEAL, 0)
#define TEALL  PAL_PAINT(PAINT_TEAL, 2)
#define LAV    PAL_PAINT(PAINT_LAVENDER, 1)
#define LAVL   PAL_PAINT(PAINT_LAVENDER, 2)
#define LAVD   PAL_PAINT(PAINT_LAVENDER, 0)
#define MAG    PAL_PAINT(PAINT_MAGENTA, 1)
#define MAGL   PAL_PAINT(PAINT_MAGENTA, 2)
#define PEACH  PAL_SKIN(SKIN_PEACH, 2)
#define PEACHM PAL_SKIN(SKIN_PEACH, 1)
#define BEAK   PAL_PAINT(PAINT_TANGERINE, 1)
#define BEAKL  PAL_PAINT(PAINT_HONEY, 2)
#define RED    PAL_PAINT(PAINT_CHERRY, 1)
#define REDD   PAL_PAINT(PAINT_CHERRY, 0)
#define HAIR   PAL_WHITE
#define HAIRD  PAL_PAINT(PAINT_SILVER, 2)
#define INK    PAL_PAINT(PAINT_JET, 1)
#define FEATH1 PAL_PAINT(PAINT_CARAMEL, 1)
#define FEATH2 PAL_PAINT(PAINT_CHOCOLATE, 2)
#define FEATH3 PAL_PAINT(PAINT_RUST, 2)
#define TIP    PAL_PAINT(PAINT_BONE, 2)
#define PANTS  PAL_PAINT(PAINT_CHARCOAL, 2)
#define GOLD   PAL_UI_GOLD

static void tail_fan(float t) {
    float hipx = -1.5f, hipy = -17;
    float spread = 0.55f + M->fan * 0.95f;              /* half-angle */
    float len = 15 + M->fan * 9;
    int n = 9;
    static const uint8_t cols[3] = { FEATH1, FEATH2, FEATH3 };
    for (int i = 0; i < n; i++) {
        float u = (float)i / (n - 1) * 2 - 1;           /* -1..1 */
        float a = -1.5708f + u * spread;
        a += 0.035f * sinf(t * 0.23f + i * 0.9f);        /* breathing ripple */
        a += M->flutter * 0.09f * sinf(t * 1.9f + i * 1.3f);
        float l = len * (1.0f - 0.18f * fabsf(u));
        float cx = hipx + cosf(a) * l * 0.55f, cy = hipy + sinf(a) * l * 0.55f;
        uint8_t c = cols[(i + 1) % 3];
        /* rotation of the feather's long axis (local, before flip) */
        B(cx, cy, 3.4f, l * 0.55f, a + 1.5708f, c, WB_ALL);
        float tx = hipx + cosf(a) * l * 0.98f, ty = hipy + sinf(a) * l * 0.98f;
        B(tx, ty, 2.6f, 1.8f, a + 1.5708f, TIP, WB_OUT);
        float ex = hipx + cosf(a) * l * 0.82f, ey = hipy + sinf(a) * l * 0.82f;
        B(ex, ey, 2.2f, 0.7f, a + 1.5708f, FEATH2, 0);
    }
}

static void arm(int side, float ang, float t, int front) {
    /* side -1 = far arm, +1 = near arm. ang 0 = hanging, 1.57 = straight forward, 3 = up */
    float shx = side > 0 ? 6.5f : -6.0f, shy = -23.5f;
    float len = 10.5f + (side > 0 ? M->reach : 0);
    float a = 1.5708f - ang;                          /* local angle (0 = +x) */
    float fl = M->flutter * 0.25f * sinf(t * 2.1f + side);
    a -= fl;
    float hx = shx + cosf(a) * len, hy = shy + sinf(a) * len;
    if (!front) {
        L(shx, shy, hx, hy, 5.4f, TEALD);
        L(shx, shy, hx, hy, 3.8f, TEAL);
    } else {
        L(shx, shy, hx, hy, 6.0f, TEALD);
        L(shx, shy, hx, hy, 4.4f, TEAL);
        /* cuff */
        B(shx + cosf(a) * len * 0.82f, shy + sinf(a) * len * 0.82f, 2.6f, 2.2f, 0, PAL_WHITE, WB_OUT);
    }
    /* feather fingers */
    for (int k = -1; k <= 1; k++) {
        float fa = a + k * 0.45f + M->grip * 0.2f * k;
        float fx = hx + cosf(fa) * 2.6f, fy = hy + sinf(fa) * 2.6f;
        B(fx, fy, 1.4f, 2.8f, fa + 1.5708f, k == 0 ? FEATH1 : FEATH3, WB_OUT | WB_SHADE);
    }
    int idx = side > 0 ? 1 : 0;
    mayor_hand_x[idx] = WX(hx, hy);
    mayor_hand_y[idx] = WY(hx, hy);
}

void mayor_draw(const Mayor *m) {
    M = m;
    float t = m->t;
    float breath = sinf(t * 0.16f);
    sqy = m->squash * (1.0f + 0.02f * breath);
    sqx = 1.0f / sqrtf(sqy);

    /* --- tail fan (behind everything) --- */
    tail_fan(t);

    /* --- far arm --- */
    arm(-1, m->armL, t, 0);

    /* --- legs + feet --- */
    for (int s = -1; s <= 1; s += 2) {
        float lx = s * 3.4f;
        float lift = s > 0 ? m->step : -m->step;
        float ly = lift > 0 ? -lift : 0;
        L(lx, -9 + ly * 0.3f, lx, -2 + ly, 1.6f, BEAK);
        for (int k = -1; k <= 1; k++) L(lx, -1.5f + ly, lx + k * 2.4f + 0.8f, -0.2f + ly, 1.1f, BEAK);
        B(lx, -9.5f + ly * 0.4f, 3.4f, 3.6f, 0, PANTS, WB_ALL);
    }

    /* --- coat tails --- */
    T(-8, -16, -1, -16, -5.5f, -5, TEALD);
    T(8, -16, 1, -16, 5.5f, -5.5f, TEALD);

    /* --- body: round teal coat --- */
    B(0, -17.5f, 10.5f, 10.0f, 0, TEAL, WB_ALL);
    /* shirt front + lapels */
    B(1.2f, -19, 4.0f, 7.0f, 0, PAL_WHITE, WB_OUT | WB_SHADE);
    T(-2.5f, -26, 1, -26, -1, -16, TEALL);
    T(4.8f, -26, 1.5f, -26, 3.4f, -16, TEALL);
    B(1.2f, -16, 0.9f, 0.9f, 0, GOLD, WB_OUT);
    B(1.2f, -12.8f, 0.9f, 0.9f, 0, GOLD, WB_OUT);

    /* --- neck --- */
    B(0.5f, -26.5f, 3.4f, 3.0f, 0, PEACHM, WB_OUT | WB_SHADE);

    /* --- head --- */
    float hx = 0.6f + m->head_dx, hy = -33.5f + m->head_dy + 0.4f * breath;
    mayor_head_x = WX(hx, hy);
    mayor_head_y = WY(hx, hy);
    float ht = m->head_tilt;

    /* wild white hair behind the head (bounces with lag) */
    float hb = 0.6f * sinf(t * 0.16f - 0.8f) + m->hair_kick;
    for (int i = 0; i < 7; i++) {
        float a = 3.14159f + (float)i / 6 * 3.14159f;   /* back half circle */
        float r = 7.5f + (i % 2) * 1.2f;
        float px = hx - 1 + cosf(a) * r * 1.05f, py = hy - 1.5f + sinf(a) * r * 0.55f + hb * (i % 3 == 0 ? 1 : 0.5f);
        B(px, py, 3.2f, 2.6f, a, (i & 1) ? HAIR : HAIRD, WB_ALL);
    }
    B(hx - 7.8f, hy + 1.5f + hb, 3.0f, 3.8f, 0.4f, HAIR, WB_ALL);  /* side puff */
    B(hx + 7.8f, hy + 1.0f + hb, 2.6f, 3.4f, -0.4f, HAIRD, WB_ALL);

    BO(hx, hy, 7.6f, 7.2f, ht, PEACH, PAL_RAMP(RAMP_PEACH, 2), WB_ALL);
    /* snood: little red droop over the beak */
    B(hx + 2.2f, hy - 1.6f, 1.1f, 2.4f, 0.35f + 0.1f * sinf(t * 0.2f), RED, WB_OUT | WB_SHADE);

    /* eyes */
    float ex = hx + 1.6f, ey = hy - 1.2f;
    float es = 1.0f + m->eye_pop * 0.45f;
    for (int s = -1; s <= 1; s += 2) {
        float cx = ex + s * 3.0f, cy = ey;
        float rx = 2.3f * es, ry = 2.8f * es;
        if (m->expr == MX_HAPPY) {
            L(cx - 1.8f, cy + 0.6f, cx, cy - 1.0f, 0.9f, INK);
            L(cx, cy - 1.0f, cx + 1.8f, cy + 0.6f, 0.9f, INK);
            continue;
        }
        float lid = m->blink;
        if (m->expr == MX_SMUG && lid < 0.45f) lid = 0.45f;
        if (m->expr == MX_SQUINT && lid < 0.55f) lid = 0.55f;
        if (lid > 0.85f) { L(cx - rx, cy + 0.3f, cx + rx, cy + 0.3f, 0.9f, INK); continue; }
        B(cx, cy, rx, ry, 0, PAL_WHITE, WB_OUT);
        float pr = (m->expr == MX_SHOCK ? 0.8f : 1.35f) * (es > 1.2f ? 0.85f : 1.0f);
        float pxo = m->look_x * (rx - pr - 0.3f), pyo = m->look_y * (ry - pr - 0.3f);
        B(cx + pxo, cy + pyo, pr, pr * 1.15f, 0, INK, 0);
        B(cx + pxo - pr * 0.35f, cy + pyo - pr * 0.45f, pr * 0.35f, pr * 0.35f, 0, PAL_WHITE, 0);
        if (lid > 0.02f) {
            /* eyelid: peach band drawn from the top */
            float lh = ry * 2 * lid;
            BO(cx, cy - ry + lh * 0.5f - 0.3f, rx + 0.3f, lh * 0.5f + 0.3f, 0, PEACH, PAL_RAMP(RAMP_PEACH, 2), WB_OUT);
        }
    }
    /* brows: bushy white */
    for (int s = -1; s <= 1; s += 2) {
        float cx = ex + s * 3.0f, cy = ey - 3.6f * es - m->brow_up * 1.5f;
        float tilt = 0;
        if (m->expr == MX_ANGRY || m->expr == MX_SQUINT) tilt = -0.35f * s;
        if (m->expr == MX_WORRY) tilt = 0.4f * s;
        B(cx, cy, 2.4f, 0.9f, tilt, HAIR, WB_OUT);
    }
    /* blush */
    B(hx - 3.4f, hy + 2.6f, 1.4f, 0.8f, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
    B(hx + 6.0f, hy + 2.4f, 1.2f, 0.8f, 0, PAL_PAINT(PAINT_ROSE, 2), 0);

    /* beak + wattle */
    float bx = hx + 3.4f, by = hy + 1.0f;
    float open = m->mouth;
    /* wattle on a spring: idle sway + kicks from the shots */
    float wa = 0.12f * sinf(t * 0.21f) + m->wattle;
    float w1x = bx - 0.6f + sinf(wa) * 2.6f, w1y = by + 2.6f + cosf(wa) * 2.6f;
    float wa2 = wa * 1.6f + 0.08f * sinf(t * 0.37f);
    float w2x = w1x + sinf(wa2) * 2.4f, w2y = w1y + cosf(wa2) * 2.4f;
    B(w1x, w1y, 1.6f, 2.2f, -wa, RED, WB_ALL);
    B(w2x, w2y, 1.4f, 1.8f, -wa2, RED, WB_ALL);
    /* lower beak (opens) */
    T(bx - 1.6f, by + 0.6f, bx + 1.6f, by + 0.6f, bx + 0.6f, by + 1.6f + open * 2.6f, PAL_PAINT(PAINT_TANGERINE, 0));
    /* upper beak */
    T(bx - 2.2f, by - 1.3f, bx + 2.4f, by - 0.6f, bx + 1.2f, by + 1.6f - open * 0.4f, PAL_PAINT(PAINT_TANGERINE, 0));
    T(bx - 1.6f, by - 0.9f, bx + 1.8f, by - 0.5f, bx + 1.0f, by + 1.0f - open * 0.4f, BEAK);
    B(bx - 0.5f, by - 0.6f, 0.6f, 0.4f, 0, BEAKL, 0);

    /* --- top hat (tilted, lags behind the head) --- */
    {
        float ha = ht + m->hat_tilt + 0.12f;
        float cx = hx - 0.4f, cy = hy - 6.2f;
        float hh = 15.0f + m->hat_stretch, wb = 5.6f, wt = 6.6f;
        float ux = sinf(ha), uy = -cosf(ha);      /* up vector */
        float rx = cosf(ha), ry = sinf(ha);       /* right vector */
        float ax_ = cx - rx * wb, ay_ = cy - ry * wb, bx_ = cx + rx * wb, by_ = cy + ry * wb;
        float tx = cx + ux * hh, ty = cy + uy * hh;
        float qx = tx + rx * wt, qy = ty + ry * wt, dx = tx - rx * wt, dy = ty - ry * wt;
        uint8_t ol = LAVD;
        wd_quad(WX(ax_, ay_), WY(ax_, ay_), WX(bx_, by_), WY(bx_, by_), WX(qx, qy), WY(qx, qy), WX(dx, dy), WY(dx, dy), LAV, PAL_PAINT(PAINT_GRAPE, 0));
        /* highlight stripe */
        float h0x = cx - rx * wb * 0.45f, h0y = cy - ry * wb * 0.45f;
        float h1x = tx - rx * wt * 0.5f, h1y = ty - ry * wt * 0.5f;
        L(h0x + ux * 3, h0y + uy * 3, h1x - ux * 1, h1y - uy * 1, 1.2f, LAVL);
        /* magenta band */
        float b0 = 2.0f, b1 = 4.4f;
        float k0 = wb + (wt - wb) * b0 / hh, k1 = wb + (wt - wb) * b1 / hh;
        wd_quad(WX(cx - rx * k0 + ux * b0, cy - ry * k0 + uy * b0), WY(0, cy - ry * k0 + uy * b0),
                WX(cx + rx * k0 + ux * b0, cy + ry * k0 + uy * b0), WY(0, cy + ry * k0 + uy * b0),
                WX(cx + rx * k1 + ux * b1, cy + ry * k1 + uy * b1), WY(0, cy + ry * k1 + uy * b1),
                WX(cx - rx * k1 + ux * b1, cy - ry * k1 + uy * b1), WY(0, cy - ry * k1 + uy * b1), MAG, 0);
        /* top ellipse + brim */
        wd_blob_ol(WX(tx, ty), WY(tx, ty), WR(wt) * sqx, WR(1.6f) * sqy, ha * m->flip, LAVL, PAL_PAINT(PAINT_GRAPE, 0), WB_OUT);
        wd_blob_ol(WX(cx, cy), WY(cx, cy), WR(9.6f) * sqx, WR(2.0f) * sqy, ha * m->flip, LAV, PAL_PAINT(PAINT_GRAPE, 0), WB_OUT | WB_SHADE);
        (void)ol;
    }

    /* --- bow tie --- */
    {
        float cx = 1.0f, cy = -25.6f, w = 1.0f + 0.12f * sinf(t * 0.3f);
        T(cx, cy, cx - 4.2f * w, cy - 2.4f, cx - 4.2f * w, cy + 2.4f, PAL_PAINT(PAINT_MAGENTA, 0));
        T(cx, cy, cx + 4.2f * w, cy - 2.4f, cx + 4.2f * w, cy + 2.4f, PAL_PAINT(PAINT_MAGENTA, 0));
        T(cx, cy, cx - 3.4f * w, cy - 1.6f, cx - 3.4f * w, cy + 1.6f, MAG);
        T(cx, cy, cx + 3.4f * w, cy - 1.6f, cx + 3.4f * w, cy + 1.6f, MAG);
        B(cx - 2.2f, cy - 0.7f, 0.6f, 0.4f, 0, MAGL, 0);
        B(cx, cy, 1.2f, 1.2f, 0, MAGL, WB_OUT);
    }

    /* --- near arm (in front) --- */
    arm(1, m->armR, t, 1);
}

void mayor_default(Mayor *m, float x, float y, float s, float t) {
    *m = (Mayor){ 0 };
    m->x = x; m->y = y; m->s = s; m->flip = 1; m->t = t;
    m->squash = 1; m->fan = 0.25f; m->flutter = 0.6f;
    m->armL = 0.35f; m->armR = 0.4f;
    /* natural blink every ~2.7 s */
    float bt = fmodf(t + 37.0f, 81.0f);
    m->blink = bt < 3 ? 1.0f - fabsf(bt - 1.5f) / 1.5f : 0;
    if (m->blink > 0) m->blink = m->blink > 0.5f ? 1 : 0.6f;
}
