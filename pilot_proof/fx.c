#include "fx.h"
#include "cast.h"
#include "wd.h"
#include "tk.h"
#include <math.h>

#define INK PAL_PAINT(PAINT_JET, 1)

static const uint8_t FURS[6][2] = { { PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CARAMEL, 0) }, { PAL_WHITE, PAL_PAINT(PAINT_BONE, 1) },
    { PAL_PAINT(PAINT_HONEY, 2), PAL_PAINT(PAINT_HONEY, 1) }, { PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_ROSE, 1) },
    { PAL_PAINT(PAINT_SKY, 2), PAL_PAINT(PAINT_SKY, 1) }, { PAL_PAINT(PAINT_CHOCOLATE, 2), PAL_PAINT(PAINT_CHOCOLATE, 1) } };
static const uint8_t OUTS[5][3] = { { PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_MINT, 0), PAL_PAINT(PAINT_MINT, 2) },
    { PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_LEMON, 0), PAL_PAINT(PAINT_LEMON, 2) }, { PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_LAVENDER, 0), PAL_PAINT(PAINT_LAVENDER, 2) },
    { PAL_PAINT(PAINT_TANGERINE, 1), PAL_PAINT(PAINT_TANGERINE, 0), PAL_PAINT(PAINT_TANGERINE, 2) }, { PAL_PAINT(PAINT_TEAL, 1), PAL_PAINT(PAINT_TEAL, 0), PAL_PAINT(PAINT_TEAL, 2) } };

void crowd_draw(const CrowdOpts *o) {
    for (int i = 0; i < o->n; i++) {
        uint32_t h = tk_hash((uint32_t)i * 977u + o->seed);
        float bx = o->x0 + (o->x1 - o->x0) * ((i + 0.5f) / o->n) + (float)(h % 9) - 4;
        float y = o->y0 + (o->y1 - o->y0) * (((h >> 8) % 100) / 100.0f);
        /* this extra's own clock: stops when the freeze ripple passes */
        float t = o->t;
        if (o->freeze_at >= 0) { float stop = o->freeze_at + (bx - o->x0) * o->ripple; if (t > stop) t = stop; }
        float x = bx;
        int dir = (h >> 3) & 1 ? 1 : -1;
        if (o->panic > 0) {   /* flee in every direction except toward the Wall (east) */
            dir = (h >> 4) % 3 == 0 ? 1 : -1;
            float run = o->panic * (o->t * (0.9f + (h % 5) * 0.15f));
            x = bx + dir * fmodf(run + (h % 60), 140) - dir * 30;
            if (dir > 0 && x > 0) x = -x * 0.3f;   /* nobody runs toward the Wall */
        }
        if (o->avoid_r > 0 && fabsf(x - o->avoid_x) < o->avoid_r) x = o->avoid_x + (x < o->avoid_x ? -o->avoid_r : o->avoid_r);
        Critter c = { 0 };
        c.x = x; c.y = y; c.s = (0.55f + 0.08f * ((h >> 5) % 3)) * (o->scale > 0 ? o->scale : 1); c.t = t + i * 9;
        c.flip = o->turn > 0.5f ? -1 : (o->panic > 0 ? dir : ((h >> 3) & 1 ? 1 : -1));
        c.fur = FURS[h % 6][0]; c.furd = FURS[h % 6][1];
        c.outfit = OUTS[(h >> 7) % 5][0]; c.outfitd = OUTS[(h >> 7) % 5][1]; c.outfitl = OUTS[(h >> 7) % 5][2];
        c.ears = (h >> 11) % 3 == 0 ? CR_EARS_LONG : CR_EARS_ROUND;
        c.squash = 1; c.armL = c.armR = 0.4f;
        c.blink = t == o->t ? fmodf(t + i * 13, 90) > 86 : 0;
        c.look_x = o->turn > 0.5f ? -1 : 0;
        if (o->eyes > 0) c.look_y = -0.3f * o->eyes;
        if (o->panic > 0) {
            float ph = o->t * 0.6f + i;
            c.y -= fabsf(sinf(ph)) * 3 * o->panic;
            c.armL = 2.6f + 0.4f * sinf(ph * 1.3f); c.armR = 2.6f + 0.4f * cosf(ph * 1.3f);
            c.mouth = 0.8f;
        } else if (t >= o->t - 0.5f) {    /* idle: breathing, a small bit-action */
            if (i % 4 == 0) c.armR = 1.0f + 0.2f * sinf(t * 0.1f + i);
        } else c.mouth = 0;                /* frozen mid-breath */
        critter_draw(&c);
        if (o->eyes > 0.3f) for (int k = -1; k <= 1; k += 2) wd_blob(x + k * 3 * c.s, critter_head_y - 0.8f * c.s, 1.6f * c.s * o->eyes, 1.9f * c.s * o->eyes, 0, PAL_WHITE, WB_OUT);
        if (o->panic > 0 && ((int)o->t + i) % 3 == 0) wd_blob(x + 6 * c.s, critter_head_y - 6 * c.s - fmodf(o->t, 3) * 2, 0.8f * c.s, 1.1f * c.s, 0, PAL_PAINT(PAINT_SKY, 2), WB_OUT);   /* sweat drop */
    }
}

void puff_demon(float x, float y, float s, float t, int dir) {
    /* fist-sized purple puff, stubby horns, red eyes, far too many teeth */
    float b = fabsf(sinf(t * 0.35f));
    y -= b * 4 * s;
    float sq = 1 + 0.15f * (1 - b);
    wd_tri(x - 3 * s, y - 4 * s, x - 1.5f * s, y - 5 * s, x - 3.5f * s, y - 8 * s, PAL_PAINT(PAINT_BONE, 2));
    wd_tri(x + 3 * s, y - 4 * s, x + 1.5f * s, y - 5 * s, x + 3.5f * s, y - 8 * s, PAL_PAINT(PAINT_BONE, 2));
    for (int k = 0; k < 6; k++) { float a = k * 1.047f + t * 0.05f; wd_blob(x + cosf(a) * 3 * s * sq, y + sinf(a) * 2.6f * s / sq, 2.6f * s, 2.4f * s, 0, PAL_PAINT(PAINT_GRAPE, 1), 0); }
    wd_blob(x, y, 4 * s * sq, 3.6f * s / sq, 0, PAL_PAINT(PAINT_GRAPE, 2), 0);
    for (int k = -1; k <= 1; k += 2) wd_blob(x + k * 1.6f * s + dir * 0.6f * s, y - 1 * s, 0.8f * s, 0.8f * s, 0, PAL_PAINT(PAINT_CHERRY, 1), 0);
    wd_blob(x + dir * 0.5f * s, y + 1.6f * s, 2.4f * s, 1.0f * s, 0, INK, 0);
    for (int k = -2; k <= 2; k++) wd_tri(x + dir * 0.5f * s + k * 0.9f * s - 0.4f * s, y + 0.9f * s, x + dir * 0.5f * s + k * 0.9f * s + 0.4f * s, y + 0.9f * s, x + dir * 0.5f * s + k * 0.9f * s, y + 1.8f * s, PAL_WHITE);
}

void verdict(const char *text, float age_s, int y) {
    if (age_s < 0) return;
    int sc = tk_text_w(text, 2) > TW - 8 ? 1 : 2;
    int w = tk_text_w(text, sc);
    if (age_s >= 8 / 30.0f) tk_text((TW - w) / 2 + sc, y + sc, text, PAL_PAINT(PAINT_GRAPE, 0), sc);   /* Clinker, 8 f late, a shade under */
    tk_text_outline((TW - w) / 2, y, text, PAL_PAINT(PAINT_GRAPE, 2), PAL_PAINT(PAINT_JET, 1), sc);   /* Clinker, 8 f late, a shade under */
}

void cake_table(float x, float y, float t, int pie_cut) {
    wd_rect(x - 24, y - 12, 48, 3, PAL_PAINT(PAINT_CARAMEL, 1), INK);
    wd_quad(x - 25, y - 10, x + 25, y - 10, x + 27, y, x - 27, y, PAL_WHITE, PAL_PAINT(PAINT_ROSE, 1));    /* the cloth */
    for (int k = 0; k < 8; k++) wd_blob(x - 23 + k * 6.6f, y - 1, 2.6f, 1.2f, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
    /* the pie (a slice gone when cut) */
    wd_blob_ol(x - 10, y - 13.5f, 7, 2.4f, 0, PAL_PAINT(PAINT_CARAMEL, 1), INK, WB_SHADE);
    wd_blob(x - 10, y - 14, 5.6f, 0.8f, 0, PAL_PAINT(PAINT_CHERRY, 1), 0);
    if (pie_cut) wd_tri(x - 10, y - 14, x - 4, y - 15, x - 4, y - 12.5f, PAL_WHITE);
    /* cupcakes */
    for (int k = 0; k < 3; k++) {
        float cx = x + 6 + k * 6, cy = y - 13;
        wd_quad(cx - 2, cy - 1, cx + 2, cy - 1, cx + 1.6f, cy + 1.5f, cx - 1.6f, cy + 1.5f, PAL_PAINT(PAINT_SKY, 2), INK);
        wd_blob_ol(cx, cy - 1.8f, 2.4f, 1.6f, 0, PAL_PAINT(PAINT_BUBBLEGUM, 2), INK, 0);
    }
    (void)t;
}
#include "show.h"
void fair_backdrop(float t, float x0, float x1) {
    static const uint8_t cols[] = { PAL_PAINT(PAINT_LAVENDER, 2), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PEACH, 6) };
    tk_vgrad(0, 0, TW, TH, cols, 3);
    wd_rect(x0 - 200, -2, x1 - x0 + 400, 200, PAL_RAMP(RAMP_PEACH, 6), 0);
    for (int k = 0; k < 8; k++) wd_line(x0 - 200, k * 8.0f, x1 + 200, k * 8.0f, 0.4f, PAL_RAMP(RAMP_PEACH, 5));
    for (float x = x0 - 40; x < x1 + 40; x += 30) {     /* lantern strings */
        for (int k = 0; k < 10; k++) { float u0 = k / 10.0f, u1 = (k + 1) / 10.0f; wd_line(x + u0 * 30, -70 + sinf(u0 * 3.14f) * 6, x + u1 * 30, -70 + sinf(u1 * 3.14f) * 6, 0.35f, PAL_PAINT(PAINT_CHOCOLATE, 1)); }
        for (int k = 1; k < 3; k++) { float u = k / 3.0f; int i = (int)(x / 30) + k; wd_blob_ol(x + u * 30, -66 + sinf(u * 3.14f) * 6 + 0.6f * sinf(t * 0.08f + i), 1.8f, 2.2f, 0, (i % 3 == 0) ? PAL_PAINT(PAINT_LEMON, 2) : (i % 3 == 1) ? PAL_PAINT(PAINT_MINT, 2) : PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_CARAMEL, 0), WB_SHADE); }
    }
}
void fair_stall(float x, float w, uint8_t stripe, const char *label, int label_on) {
    wd_rect(x - w / 2, -30, w, 30, PAL_PAINT(PAINT_BONE, 2), INK);
    for (int k = 0; k < (int)(w / 6); k++) wd_quad(x - w / 2 + k * 6, -44, x - w / 2 + k * 6 + 6, -44, x - w / 2 + k * 6 + 6, -36, x - w / 2 + k * 6, -36, (k & 1) ? PAL_WHITE : stripe, 0);
    for (int k = 0; k < (int)(w / 6); k++) wd_blob(x - w / 2 + k * 6 + 3, -36, 3, 2, 0, (k & 1) ? PAL_WHITE : stripe, 0);
    wd_rect(x - w / 2 - 1, -44, 2, 44, PAL_PAINT(PAINT_CARAMEL, 1), INK); wd_rect(x + w / 2 - 1, -44, 2, 44, PAL_PAINT(PAINT_CARAMEL, 1), INK);
    wd_rect(x - w / 2, -12, w, 12, stripe, INK);
    if (label && label_on) { int lw = tk_text_w(label, 1); tk_rect((int)wd_sx(x) - lw / 2 - 3, (int)wd_sy(-52) - 2, lw + 6, 11, PAL_WHITE); tk_text((int)wd_sx(x) - lw / 2, (int)wd_sy(-52), label, PAL_PAINT(PAINT_MAGENTA, 0), 1); }
}
void fair_tent(float x, float w, float h, uint8_t stripe) {
    for (int k = 0; k < 8; k++) { float a = x - w / 2 + k * w / 8, b2 = a + w / 8; wd_quad(a, -h * 0.6f, b2, -h * 0.6f, b2, 0, a, 0, (k & 1) ? PAL_WHITE : stripe, 0); }
    wd_tri(x - w / 2 - 4, -h * 0.6f, x + w / 2 + 4, -h * 0.6f, x, -h, stripe);
    wd_blob(x, -h - 2, 2, 2, 0, PAL_UI_GOLD, WB_OUT);
}
void say_at_world(const char *text, float ta, float t0, float secs, float wx, float wy, int side) {
    if (ta < t0 || ta >= t0 + secs + 0.5f) return;
    bubble_side = side; bubble(text, (int)((ta - t0) * 24) + 1, (int)wd_sx(wx), (int)wd_sy(wy)); bubble_side = 0;
}
void babble_line(double t, float secs, float pitch, float vol) {
    int n = (int)(secs * 7.5f); if (n < 1) n = 1;
    for (int k = 0; k < n; k++) {
        uint32_t h = tk_hash((uint32_t)k * 31u + (uint32_t)(secs * 100) + (uint32_t)pitch);
        tk_note(t + k * secs / n, 0.07, (int)pitch + (int)(h % 5) - 2, TKV_TRI, vol);
        tk_note(t + k * secs / n, 0.05, (int)pitch + 12 + (int)(h % 5) - 2, TKV_SQ12, vol * 0.25f);
    }
}
