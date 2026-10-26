#include "cast.h"
#include "wd.h"
#include "tk.h"
#include <math.h>

#define INK PAL_PAINT(PAINT_JET, 1)

float sunny_hand_x, sunny_hand_y;
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
        wd_plush(bx, by, ex, ey, 2.8f * s, INK, fur);
        wd_line(bx + (ex - bx) * 0.2f, by + (ey - by) * 0.2f, bx + (ex - bx) * 0.8f, by + (ey - by) * 0.8f, 1.4f * s, PAL_PAINT(PAINT_ROSE, 2));
    }
    wd_blob_ol(x, hy, 10.5f * s, 9.5f * s, 0, fur, INK, WB_SHADE);
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
    float hx = shx + cosf(a) * 6.5f * s, hyy = shy + sinf(a) * 6.5f * s;
    wd_plush(x - 6 * s, shy, x - 7 * s, shy + 5 * s, 2.4f * s, INK, cl);
    wd_blob_ol(x - 7 * s, shy + 5.5f * s, 2.2f * s, 2.2f * s, 0, fur, INK, WB_SHADE);
    wd_plush(shx, shy, hx, hyy, 2.4f * s, INK, cl);
    wd_blob_ol(hx, hyy, 2 * s, 2 * s, 0, fur, INK, 0);
    sunny_hand_x = hx; sunny_hand_y = hyy;
    if (u->page > 0 && !u->no_page) {   /* page one: wet, sagging, dripping */
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
    if (b->possess > 0) {   /* purple possession aura, flickering */
        for (int k = 0; k < 9; k++) {
            float a = k * 0.7f + b->t * 0.05f, r = (9 + 3 * sinf(b->t * 0.3f + k)) * s;
            wd_blob(x + cosf(a) * r * 0.7f, y - 24 * s + sinf(a) * r * 1.4f, 4 * s * b->possess, 4 * s * b->possess, 0, PAL_PAINT(PAINT_GRAPE, 1), 0);
        }
    }
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
    for (int k = -1; k <= 1; k += 2) wd_blob(x + k * 2.4f * s + b->look * 1.3f * s, hy + 0.5f * s, 1.7f * s, 1.9f * s, 0, INK, 0);
    for (int k = -1; k <= 1; k += 2) wd_blob(x + k * 2.4f * s + 0.4f * s + b->look * 1.3f * s, hy, (0.5f + b->possess * 0.5f) * s, (0.5f + b->possess * 0.5f) * s, 0, b->possess > 0.2f ? PAL_PAINT(PAINT_GRAPE, 2) : PAL_WHITE, 0);
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

float nana_hand_x[2], nana_hand_y[2], nana_mouth_x, nana_mouth_y;
void nana_default(Nana *n, float x, float y, float s, float t) {
    *n = (Nana){ .x = x, .y = y, .s = s, .t = t, .flip = 1, .squash = 1, .armL = 0.5f, .armR = 0.5f, .buttons = 4 };
}
void nana_draw(const Nana *n) {
    float s = n->s, x = n->x, y = n->y, sq = n->squash * (1 + 0.015f * sinf(n->t * 0.12f));
    uint8_t wool = PAL_PAINT(PAINT_BONE, 2), wooll = PAL_WHITE, woold = PAL_PAINT(PAINT_BONE, 0);
    uint8_t card = PAL_PAINT(PAINT_LAVENDER, 1), cardd = PAL_PAINT(PAINT_LAVENDER, 0), cardl = PAL_PAINT(PAINT_LAVENDER, 2);
    uint8_t face = PAL_SKIN(SKIN_PEACH, 2);
    /* legs: little dark hooves under the skirt */
    for (int k = -1; k <= 1; k += 2) wd_blob_ol(x + k * 3.5f * s, y - 1 * s, 2.6f * s, 1.6f * s, 0, PAL_PAINT(PAINT_CHARCOAL, 1), INK, 0);
    /* woolly body: a ring of puffs, then the cardigan and the apron */
    float by = y - 13 * s * sq;
    for (int k = 0; k < 10; k++) {
        float a = k * 0.628f;
        wd_blob_ol(x + cosf(a) * 8 * s, by + sinf(a) * 9 * s * sq, 4 * s, 4 * s, 0, k & 1 ? wool : wooll, woold, 0);
    }
    wd_blob_ol(x, by, 9 * s, 10 * s * sq, 0, card, INK, WB_SHADE);
    wd_quad(x - 6 * s, by - 3 * s, x + 6 * s, by - 3 * s, x + 7 * s, by + 10 * s, x - 7 * s, by + 10 * s, PAL_WHITE, PAL_PAINT(PAINT_ROSE, 1));
    wd_blob(x, by + 4 * s, 2.4f * s, 1.6f * s, 0, PAL_PAINT(PAINT_ROSE, 2), 0);                 /* apron pocket */
    for (int k = 0; k < 4; k++) {                                                             /* cardigan buttons, one may be gone */
        if (k >= n->buttons) { wd_line(x - 7.4f * s, by - 6 * s + k * 3 * s, x - 6.6f * s, by - 5.4f * s + k * 3 * s, 0.3f * s, PAL_GRAY_D); continue; }
        wd_blob_ol(x - 7 * s, by - 6 * s + k * 3 * s, 0.9f * s, 0.9f * s, 0, PAL_PAINT(PAINT_HONEY, 2), INK, 0);
    }
    /* flour dust on the apron */
    for (int k = 0; k < 5; k++) wd_blob(x - 3 * s + k * 1.6f * s, by + (k & 1) * 3 * s, 0.6f * s, 0.6f * s, 0, PAL_WHITE, 0);
    /* head: woolly cap of curls, floppy ears, tiny curled horns, round glasses */
    float hy = y - 30 * s * sq;
    for (int k = -1; k <= 1; k += 2) {
        wd_blob_ol(x + k * 8.5f * s, hy + 1 * s, 3.4f * s, 1.8f * s, k * 0.5f, face, INK, 0);    /* ears */
        wd_blob_ol(x + k * 6 * s, hy - 5 * s, 2.2f * s, 2.2f * s, 0, PAL_PAINT(PAINT_CARAMEL, 2), INK, 0);   /* horn curls */
        wd_blob(x + k * 6 * s, hy - 5 * s, 0.9f * s, 0.9f * s, 0, PAL_PAINT(PAINT_CARAMEL, 1), 0);
    }
    wd_blob_ol(x, hy, 7.5f * s, 7 * s, 0, face, INK, WB_SHADE);
    for (int k = 0; k < 7; k++) wd_blob_ol(x - 6 * s + k * 2 * s, hy - 6 * s - (k % 2) * 1.2f * s, 2.4f * s, 2.2f * s, 0, k & 1 ? wool : wooll, woold, 0);
    for (int k = -1; k <= 1; k += 2)   /* round glasses: lenses first, eyes behind the glass */
        wd_blob_ol(x + k * 3 * s, hy + 0.5f * s, 2.3f * s, 2.1f * s, 0, PAL_PAINT(PAINT_SKY, 2), PAL_PAINT(PAINT_CARAMEL, 0), 0);
    for (int k = -1; k <= 1; k += 2) {
        float ex = x + k * 3 * s + n->look_x * 0.6f * s, ey = hy + 0.5f * s;
        if (n->blink > 0.5f) wd_line(ex - 1.2f * s, ey, ex + 1.2f * s, ey, 0.5f * s, INK);
        else wd_blob(ex, ey, 0.9f * s, 1.2f * s, 0, INK, 0);
    }
    wd_line(x - 0.8f * s, hy + 0.5f * s, x + 0.8f * s, hy + 0.5f * s, 0.3f * s, PAL_PAINT(PAINT_CARAMEL, 0));
    wd_blob(x, hy + 3.5f * s, 1.3f * s, 0.9f * s, 0, PAL_PAINT(PAINT_ROSE, 1), 0);           /* nose */
    nana_mouth_x = x; nana_mouth_y = hy + 5.2f * s;
    wd_blob(x, hy + 5.2f * s, 1.6f * s, (0.3f + n->mouth * 1.4f) * s, 0, PAL_PAINT(PAINT_CHERRY, 0), 0);  /* mouth */
    for (int k = -1; k <= 1; k += 2) wd_blob(x + k * 5 * s, hy + 3.5f * s, 1.5f * s, 0.9f * s, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
    /* arms: cardigan sleeves with woolly cuffs and round paws */
    for (int side = -1; side <= 1; side += 2) {
        float ang = side < 0 ? n->armL : n->armR;
        float shx = x + side * 8 * s * n->flip, shy = by - 6 * s;
        float a = 1.5708f - ang;
        float ux = cosf(a) * side * n->flip, uy = sinf(a);
        float ex = shx + ux * 5.5f * s + side * 1.2f * s, ey = shy + uy * 5.5f * s + 1.5f * s;
        float hx = ex + ux * 5.5f * s, hy2 = ey + uy * 5.5f * s;
        hx = shx + ux * 7 * s; hy2 = shy + uy * 7 * s;
        wd_plush(shx, shy, hx, hy2, 3.2f * s, INK, card);
        wd_blob_ol(shx, shy, 3.4f * s, 3.2f * s, 0, card, INK, WB_SHADE);                  /* puffy shoulder */
        wd_blob(ex, ey, 2.2f * s, 2.0f * s, 0, card, 0);                                    /* soft elbow */
        for (int q = 0; q < 3; q++) wd_line(shx + (ex - shx) * (0.3f + q * 0.2f) - 1.2f * s, shy + (ey - shy) * (0.3f + q * 0.2f), shx + (ex - shx) * (0.3f + q * 0.2f) + 1.2f * s, shy + (ey - shy) * (0.3f + q * 0.2f) + 0.4f * s, 0.35f * s, cardd);  /* knit ribs */
        wd_blob_ol(hx, hy2, 2.4f * s, 2.0f * s, 0, wooll, woold, 0);
        wd_blob_ol(hx + ux * 1.6f * s, hy2 + uy * 1.6f * s, 1.9f * s, 1.9f * s, 0, face, INK, WB_SHADE);
        int i = side > 0 ? 1 : 0;
        nana_hand_x[i] = hx + ux * 1.6f * s; nana_hand_y[i] = hy2 + uy * 1.6f * s;
    }
}

float critter_hand_x[2], critter_hand_y[2], critter_mouth_x, critter_mouth_y, critter_head_y;
void critter_draw(const Critter *c) {
    float s = c->s, x = c->x, sq = c->squash * (1 + 0.015f * sinf(c->t * 0.13f));
    float y = c->y, kneel = c->kneel;
    float leg = 3.5f * (1 - kneel * 0.5f);
    /* legs + feet */
    for (int k = -1; k <= 1; k += 2) {
        float fx = x + k * 3.2f * s, fy = y;
        if (kneel > 0 && k < 0) fy = y - 0.5f * s;
        wd_blob_ol(fx, fy - 2.2f * s, 3.0f * s, 3.0f * s, 0, c->outfit, INK, WB_SHADE);
        wd_blob_ol(fx + c->flip * 0.6f * s, fy - 0.6f * s, 3.0f * s, 1.6f * s, 0, c->furd, INK, 0);
    }
    float by = y - (leg + 7) * s * sq;
    wd_blob_ol(x, by, 7.5f * s, 7 * s * sq, 0, c->outfit, INK, WB_SHADE);
    if (c->extras & CX_COLLAR) { wd_rect(x - 2 * s, by - 8 * s, 4 * s, 2 * s, PAL_WHITE, INK); for (int k = 0; k < 3; k++) wd_blob(x, by - 3 * s + k * 3 * s, 0.6f * s, 0.6f * s, 0, c->outfitl, 0); }
    if (c->extras & CX_STETHO) { wd_line(x - 3 * s, by - 7 * s, x - 1 * s, by + 1 * s, 0.5f * s, PAL_GRAY); wd_line(x + 3 * s, by - 7 * s, x + 1 * s, by + 1 * s, 0.5f * s, PAL_GRAY); wd_blob_ol(x, by + 2 * s, 1.3f * s, 1.3f * s, 0, PAL_GRAY_L, INK, 0); }
    if (c->extras & CX_SASH) wd_line(x - 6 * s, by - 6 * s, x + 6 * s, by + 6 * s, 2 * s, PAL_PAINT(PAINT_LEMON, 1));
    if (c->extras & CX_SATCHEL) { wd_line(x - 6 * s, by - 7 * s, x + 5 * s, by + 4 * s, 0.8f * s, PAL_PAINT(PAINT_CHOCOLATE, 1)); wd_rect(x + 3 * s, by + 3 * s, 5 * s, 4 * s, PAL_PAINT(PAINT_CARAMEL, 1), INK); }
    /* head */
    float hy = by - 12 * s * sq;
    critter_head_y = hy;
    if (c->ears == CR_EARS_ROUND) for (int k = -1; k <= 1; k += 2) {
        wd_blob_ol(x + k * 7.5f * s + c->ear_turn * s, hy - 7.5f * s, 3.4f * s, 3.4f * s, 0, c->fur, INK, 0);
        wd_blob(x + k * 7.5f * s + c->ear_turn * s, hy - 7.5f * s, 1.4f * s, 1.4f * s, 0, c->furd, 0);
    }
    if (c->ears == CR_EARS_LONG) for (int k = -1; k <= 1; k += 2) {
        float a = -1.5708f + k * 0.2f + c->ear_turn * 0.5f + 0.05f * sinf(c->t * 0.1f + k);
        float bx = x + k * 3 * s, b2 = hy - 5 * s;
        wd_plush(bx, b2 - 2 * s, bx + cosf(a) * 12 * s, b2 - 2 * s + sinf(a) * 12 * s, 2.6f * s, INK, c->fur);
    }
    wd_blob_ol(x, hy, 10 * s, 9 * s, 0, c->fur, INK, WB_SHADE);
    if (c->extras & CX_BUN) { wd_blob_ol(x, hy - 7.5f * s, 3.2f * s, 2.6f * s, 0, PAL_PAINT(PAINT_GRAPE, 1), INK, 0); wd_blob_ol(x, hy - 3.5f * s, 7.6f * s, 3 * s, 0, PAL_PAINT(PAINT_GRAPE, 1), INK, 0); }
    wd_blob(x + c->flip * 0.6f * s, hy + 2.5f * s, 3 * s, 2.2f * s, 0, c->furd == c->fur ? PAL_WHITE : PAL_PAINT(PAINT_BONE, 2), 0);   /* muzzle */
    for (int k = -1; k <= 1; k += 2) {
        float ex = x + k * 3.8f * s + c->flip * 0.6f * s + c->look_x * 0.6f * s, ey = hy - 0.5f * s + c->look_y * 0.4f * s;
        if (c->blink > 0.5f) wd_line(ex - 1.2f * s, ey, ex + 1.2f * s, ey, 0.5f * s, INK);
        else { wd_blob(ex, ey, 1.5f * s, 1.8f * s, 0, INK, 0); wd_blob(ex - 0.4f * s, ey - 0.6f * s, 0.55f * s, 0.55f * s, 0, PAL_WHITE, 0); }
    }
    wd_blob(x + c->flip * 0.6f * s, hy + 1.5f * s, 1.1f * s, 0.8f * s, 0, INK, 0);   /* nose */
    critter_mouth_x = x + c->flip * 0.6f * s; critter_mouth_y = hy + 3.6f * s;
    wd_blob(critter_mouth_x, critter_mouth_y, 1.3f * s, (0.25f + c->mouth * 1.2f) * s, 0, PAL_PAINT(PAINT_CHERRY, 0), 0);
    for (int k = -1; k <= 1; k += 2) wd_blob(x + k * 5 * s, hy + 2 * s, 1.3f * s, 0.8f * s, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
    /* arms: shoulder, elbow, paw */
    for (int side = -1; side <= 1; side += 2) {
        float ang = side < 0 ? c->armL : c->armR;
        float shx = x + side * 6.5f * s, shy = by - 5 * s;
        float a = 1.5708f - ang, ux = cosf(a) * side, uy = sinf(a);
        float hx = shx + ux * 6 * s, hy2 = shy + uy * 6 * s;
        wd_plush(shx, shy, hx, hy2, 2.6f * s, INK, c->outfit);
        wd_blob_ol(hx, hy2, 2.5f * s, 2.4f * s, 0, c->fur, INK, WB_SHADE);
        critter_hand_x[side > 0] = hx; critter_hand_y[side > 0] = hy2;
    }
    if (c->extras & CX_FLOPS) {   /* Mr. Flops under one arm: toy bunny, one floppy ear */
        float fx = x - 6 * s, fy = by + 1 * s;
        wd_blob_ol(fx, fy, 3 * s, 3.4f * s, 0, PAL_PAINT(PAINT_ROSE, 2), INK, 0);
        wd_blob_ol(fx, fy - 4 * s, 2.6f * s, 2.4f * s, 0, PAL_PAINT(PAINT_ROSE, 2), INK, 0);
        wd_tube(fx - 1, fy - 6 * s, fx - 1.5f, fy - 11 * s, 0.9f * s, 0.7f * s, INK, PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_ROSE, 2), PAL_WHITE);
        wd_tube(fx + 1, fy - 6 * s, fx + 4 * s, fy - 5 * s, 0.9f * s, 0.7f * s, INK, PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_ROSE, 2), PAL_WHITE);
        wd_blob(fx - 0.8f * s, fy - 4 * s, 0.4f * s, 0.4f * s, 0, INK, 0); wd_blob(fx + 0.8f * s, fy - 4 * s, 0.4f * s, 0.4f * s, 0, INK, 0);
    }
}
Critter cast_fuzz(float x, float y, float t) {   /* plushie bear priest of St. Cuddles */
    return (Critter){ .x = x, .y = y, .s = 1.0f, .t = t, .flip = 1, .fur = PAL_PAINT(PAINT_CARAMEL, 1), .furd = PAL_PAINT(PAINT_CARAMEL, 0),
        .outfit = PAL_PAINT(PAINT_CHARCOAL, 1), .outfitd = PAL_PAINT(PAINT_JET, 2), .outfitl = PAL_PAINT(PAINT_CHARCOAL, 2),
        .ears = CR_EARS_ROUND, .extras = CX_COLLAR | CX_SATCHEL, .squash = 1, .armL = 0.4f, .armR = 0.4f };
}
Critter cast_bloom(float x, float y, float t) {  /* ghoul doctor in pastel scrubs */
    return (Critter){ .x = x, .y = y, .s = 0.95f, .t = t, .flip = 1, .fur = PAL_PAINT(PAINT_MINT, 2), .furd = PAL_PAINT(PAINT_MINT, 1),
        .outfit = PAL_PAINT(PAINT_BUBBLEGUM, 2), .outfitd = PAL_PAINT(PAINT_BUBBLEGUM, 1), .outfitl = PAL_WHITE,
        .ears = CR_EARS_NONE, .extras = CX_STETHO | CX_BUN, .squash = 1, .armL = 0.3f, .armR = 0.3f };
}
Critter cast_pip(float x, float y, float t) {    /* the tiniest bunnykin: Junior Festival Patrol */
    return (Critter){ .x = x, .y = y, .s = 0.55f, .t = t, .flip = 1, .fur = PAL_WHITE, .furd = PAL_PAINT(PAINT_BONE, 1),
        .outfit = PAL_PAINT(PAINT_SKY, 1), .outfitd = PAL_PAINT(PAINT_SKY, 0), .outfitl = PAL_PAINT(PAINT_SKY, 2),
        .ears = CR_EARS_LONG, .extras = CX_SASH | CX_FLOPS, .squash = 1, .armL = 0.5f, .armR = 0.5f };
}
void pet_draw(float x, float y, float s, float t, float look_x, int beep) {
    /* the pet: a round little hatchling still wearing the top of its shell */
    float b = beep ? 0.9f : 1.0f;
    wd_blob_ol(x, y - 4 * s * b, 4.5f * s, 4 * s * b, 0, PAL_PAINT(PAINT_MINT, 2), INK, WB_SHADE);
    wd_quad(x - 4 * s, y - 6 * s, x + 4 * s, y - 6 * s, x + 2.5f * s, y - 9.5f * s, x - 2.5f * s, y - 9.5f * s, PAL_PAINT(PAINT_BONE, 2), INK);
    for (int k = -1; k <= 1; k += 2) wd_blob(x + k * 1.6f * s + look_x * 0.6f * s, y - 4 * s, 0.7f * s, 0.9f * s, 0, INK, 0);
    if (beep) wd_blob(x, y - 2 * s, 0.8f * s, 0.6f * s, 0, PAL_PAINT(PAINT_CHERRY, 0), 0);
    (void)t;
}
