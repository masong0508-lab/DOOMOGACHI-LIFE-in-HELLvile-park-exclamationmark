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
    int w = tk_text_w(text, 2);
    if (age_s >= 8 / 30.0f) tk_text((TW - w) / 2 + 2, y + 2, text, PAL_PAINT(PAINT_GRAPE, 0), 2);   /* Clinker, 8 f late, a shade under */
    tk_text_outline((TW - w) / 2, y, text, PAL_PAINT(PAINT_GRAPE, 2), PAL_PAINT(PAINT_JET, 1), 2);   /* Clinker, 8 f late, a shade under */
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
