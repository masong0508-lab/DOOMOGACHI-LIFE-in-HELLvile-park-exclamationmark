#include "cast.h"
#include "wd.h"
#include "tk.h"
#include <math.h>

#define INK PAL_PAINT(PAINT_JET, 1)

void sunny_default(Sunny *u, float x, float y, float s, float t) {
    *u = (Sunny){ .x = x, .y = y, .s = s, .t = t, .flip = 1, .ears = 1, .squash = 1, .suitcase = 1 };
}
void sunny_draw(const Sunny *u) {
    float s = u->s, x = u->x, y = u->y, sq = u->squash * (1 + 0.02f * sinf(u->t * 0.15f));
    uint8_t fur = PAL_PAINT(PAINT_BONE, 2), furd = PAL_PAINT(PAINT_BONE, 1), cl = PAL_PAINT(PAINT_SKY, 1);
    if (u->suitcase) {   /* suitcase at the side, stickers and all */
        float sx = x - 9 * s * u->flip;
        wd_rect(sx - 5 * s, y - 9 * s, 10 * s, 9 * s, PAL_PAINT(PAINT_CARAMEL, 1), INK);
        wd_rect(sx - 5 * s, y - 6 * s, 10 * s, 1.2f * s, PAL_PAINT(PAINT_CARAMEL, 0), 0);
        wd_blob(sx + 2 * s, y - 3 * s, 1.6f * s, 1.6f * s, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
        wd_tube(sx - 2 * s, y - 9 * s, sx + 2 * s, y - 9 * s, 0.6f * s, 0.6f * s, INK, PAL_PAINT(PAINT_CHOCOLATE, 1), PAL_PAINT(PAINT_CHOCOLATE, 2), PAL_PAINT(PAINT_CHOCOLATE, 2));
    }
    /* feet, body (a little blue jacket) */
    for (int k = -1; k <= 1; k += 2) wd_blob_ol(x + k * 3 * s, y - 1 * s, 3 * s, 1.6f * s, 0, fur, INK, 0);
    wd_blob_ol(x, y - 9 * s * sq, 7 * s, 8 * s * sq, 0, cl, INK, WB_SHADE);
    wd_blob(x, y - 8 * s * sq, 3 * s, 5 * s * sq, 0, fur, 0);
    /* head */
    float hy = y - 23 * s * sq;
    /* ears: up and alert, or drooping */
    for (int k = -1; k <= 1; k += 2) {
        float a = -1.5708f + k * (0.18f + (1 - u->ears) * 1.1f) + 0.05f * sinf(u->t * 0.11f + k);
        float bx = x + k * 3.5f * s, by = hy - 6 * s;
        float ex = bx + cosf(a) * 13 * s, ey = by + sinf(a) * 13 * s;
        wd_tube(bx, by, ex, ey, 2.4f * s, 1.8f * s, INK, furd, fur, PAL_WHITE);
        wd_line(bx + (ex - bx) * 0.2f, by + (ey - by) * 0.2f, bx + (ex - bx) * 0.8f, by + (ey - by) * 0.8f, 1.4f * s, PAL_PAINT(PAINT_ROSE, 2));
    }
    wd_blob_ol(x, hy, 9 * s, 8 * s, 0, fur, INK, WB_SHADE);
    /* eyes: big, they notice everything */
    for (int k = -1; k <= 1; k += 2) {
        float ex = x + k * 3.6f * s + u->flip * 0.8f * s, ey = hy - 0.5f * s;
        if (u->blink > 0.5f) { wd_line(ex - 1.8f * s, ey, ex + 1.8f * s, ey, 0.7f * s, INK); continue; }
        wd_blob(ex, ey, 2.2f * s, 2.8f * s, 0, INK, 0);
        wd_blob(ex + u->look_x * 0.8f * s - 0.6f * s, ey - 1 * s + u->look_y * 0.6f * s, 0.8f * s, 0.9f * s, 0, PAL_WHITE, 0);
    }
    wd_blob(x + u->flip * 0.8f * s, hy + 3 * s, 1.0f * s, 0.7f * s, 0, PAL_PAINT(PAINT_ROSE, 1), 0);
    for (int k = -1; k <= 1; k += 2) wd_blob(x + k * 6 * s, hy + 2.5f * s, 1.6f * s, 0.9f * s, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
    /* arms; the near one raises the soggy page */
    float shx = x + 6 * s, shy = y - 13 * s * sq;
    float a = u->page > 0 ? -1.5708f * u->page - 0.3f : 1.3f;
    float hx = shx + cosf(a) * 8 * s, hyy = shy + sinf(a) * 8 * s;
    wd_tube(x - 6 * s, shy, x - 7 * s, shy + 6 * s, 1.8f * s, 1.5f * s, INK, PAL_PAINT(PAINT_SKY, 0), cl, PAL_PAINT(PAINT_SKY, 2));
    wd_tube(shx, shy, hx, hyy, 1.8f * s, 1.5f * s, INK, PAL_PAINT(PAINT_SKY, 0), cl, PAL_PAINT(PAINT_SKY, 2));
    wd_blob_ol(hx, hyy, 2 * s, 2 * s, 0, fur, INK, 0);
    if (u->page > 0) {   /* page one: wet, sagging, dripping */
        float px = hx + 1 * s, py = hyy - 7 * s;
        wd_quad(px - 6 * s, py - 6 * s, px + 6 * s, py - 6.5f * s, px + 5 * s, py + 7 * s, px - 6.5f * s, py + 6 * s, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0));
        for (int k = 0; k < 4; k++) wd_line(px - 4 * s, py - 3 * s + k * 2.6f * s, px + 3.5f * s, py - 3 * s + k * 2.6f * s, 0.5f * s, PAL_PAINT(PAINT_SKY, 1));
        float dr = fmodf(u->t * 0.6f, 14);
        wd_blob(px + 3 * s, py + 7 * s + dr * s, 0.7f * s, 1.0f * s, 0, PAL_PAINT(PAINT_SKY, 1), 0);
    }
}

void bones_default(Bones *b, float x, float y, float s, float t) {
    *b = (Bones){ .x = x, .y = y, .s = s, .t = t, .flip = 1, .squash = 1 };
}
void bones_draw(const Bones *b) {
    float s = b->s, x = b->x, y = b->y, sq = b->squash;
    uint8_t bone = PAL_PAINT(PAINT_BONE, 2), boned = PAL_PAINT(PAINT_BONE, 0);
    /* legs */
    for (int k = -1; k <= 1; k += 2) {
        wd_tube(x + k * 3 * s, y - 14 * s * sq, x + k * 3.5f * s, y - 1 * s, 1.1f * s, 1.0f * s, INK, boned, bone, PAL_WHITE);
        wd_blob_ol(x + k * 3.5f * s + 1.5f * s, y - 0.5f * s, 2.6f * s, 1.2f * s, 0, PAL_PAINT(PAINT_JET, 2), INK, 0);
    }
    wd_blob_ol(x, y - 15 * s * sq, 5 * s, 2.5f * s, 0, bone, INK, 0);           /* pelvis */
    wd_tube(x, y - 15 * s * sq, x, y - 34 * s * sq, 1.0f * s, 1.0f * s, INK, boned, bone, bone);   /* spine */
    /* the ribs: his xylophone, bars in rainbow order */
    static const uint8_t rc[6] = { PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_TANGERINE, 2), PAL_PAINT(PAINT_LEMON, 2), PAL_PAINT(PAINT_MINT, 2), PAL_PAINT(PAINT_SKY, 2), PAL_PAINT(PAINT_LAVENDER, 2) };
    for (int k = 0; k < 6; k++) {
        float ry = y - (31 - k * 2.6f) * s * sq, w = (6.5f - fabsf(k - 2.5f) * 0.6f) * s;
        float hit = (k == 2 && b->hitL > 0.8f) || (k == 4 && b->hitR > 0.8f);
        wd_tube(x - w, ry, x + w, ry, (hit ? 1.25f : 1.0f) * s, (hit ? 1.25f : 1.0f) * s, INK, bone, rc[k], PAL_WHITE);
    }
    /* sunglasses-cool skull, bow tie, band cap */
    float hy = y - 41 * s * sq;
    wd_blob_ol(x, y - 35 * s * sq, 2.4f * s, 1.3f * s, 0, PAL_PAINT(PAINT_CHERRY, 1), INK, 0);
    wd_blob_ol(x, hy + 4 * s + b->jaw * 2 * s, 4.2f * s, 2.2f * s, 0, bone, INK, 0);   /* jaw */
    wd_blob_ol(x, hy, 6 * s, 5.5f * s, 0, bone, INK, WB_SHADE);
    for (int k = -1; k <= 1; k += 2) wd_blob(x + k * 2.4f * s, hy + 0.5f * s, 1.7f * s, 1.9f * s, 0, INK, 0);
    for (int k = -1; k <= 1; k += 2) wd_blob(x + k * 2.4f * s + 0.4f * s, hy, 0.5f * s, 0.5f * s, 0, PAL_WHITE, 0);
    for (int k = -2; k <= 2; k++) wd_line(x + k * 1.3f * s, hy + 3.2f * s, x + k * 1.3f * s, hy + 4.4f * s, 0.35f * s, INK);
    wd_rect(x - 5 * s, hy - 7.5f * s, 10 * s, 3 * s, PAL_PAINT(PAINT_CHERRY, 1), INK);
    wd_rect(x - 6.5f * s, hy - 4.8f * s, 13 * s, 1.2f * s, PAL_PAINT(PAINT_JET, 2), 0);
    /* arms with mallets */
    for (int k = -1; k <= 1; k += 2) {
        float hit = k < 0 ? b->hitL : b->hitR;
        float shx = x + k * 6 * s, shy = y - 32 * s * sq;
        float ex = shx + k * 4 * s, ey = shy + 7 * s;
        float hx = x + k * (3 - hit * 1) * s, hy2 = shy + (2 + hit * 3) * s - (1 - hit) * 6 * s;
        wd_tube(shx, shy, ex, ey, 0.9f * s, 0.8f * s, INK, boned, bone, PAL_WHITE);
        wd_tube(ex, ey, hx, hy2, 0.8f * s, 0.7f * s, INK, boned, bone, PAL_WHITE);
        float mx = hx - k * 2 * s, my = hy2 - 6 * s * (1 - hit) + 2 * s * hit;
        wd_tube(hx, hy2, mx, my, 0.45f * s, 0.45f * s, INK, PAL_PAINT(PAINT_CARAMEL, 0), PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CARAMEL, 2));
        wd_blob_ol(mx, my, 1.7f * s, 1.7f * s, 0, PAL_PAINT(PAINT_CHERRY, 1), INK, WB_SHADE);
        wd_blob_ol(hx, hy2, 1.4f * s, 1.4f * s, 0, bone, INK, 0);
    }
}
