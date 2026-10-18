/* story_sprites.c - the small procedural props of Chapters 3-4 (owner: S3;
 * SPEC sec.7 S3): Vapula in his jar (a cupcake-sized grey cloud, air holes
 * in the lid, two drops of rain when he cries), the pet's horns and halo,
 * Wisp (see-through in Chapter 3, in a body from Ep 4.4), the horned poop
 * of E1, and three tiny const billboards for the credits parade (horns,
 * the jar, a halo). Everything is a few rects and circles, cartoon-modest,
 * at any integer scale s (1 at 240x160, 2 at 320x240 close-ups).
 *
 * Used by SCENE_CUTX (cutstage.c, the cutscene group) and the credits (the
 * story group), so on the PS1 these few functions are resident.
 */
#include "story_int.h"
#include "story_ch34.h"

/* ---- the credits' billboards: const 8bpp images (index 0 = clear) --------------- */
#define K PAL_BLACK
#define R1 PAL_RAMP(RAMP_BLOOD, 2)
#define R2 PAL_RAMP(RAMP_BLOOD, 4)
#define R3 PAL_RAMP(RAMP_BLOOD, 6)
#define G1 PAL_RAMP(RAMP_STONE, 3)
#define G2 PAL_RAMP(RAMP_STONE, 5)
#define G3 PAL_RAMP(RAMP_STONE, 7)
#define W1 PAL_RAMP(RAMP_SKY, 7)
#define W2 PAL_RAMP(RAMP_SKY, 5)
#define Y1 PAL_RAMP(RAMP_CANDY, 6)
#define Y2 PAL_RAMP(RAMP_CANDY, 4)
static const uint8_t px_horns[12 * 5] = {
    0, R3, 0, 0, 0, 0, 0, 0, 0, 0, R3, 0,
    0, R2, R3, 0, 0, 0, 0, 0, 0, R3, R2, 0,
    0, R1, R2, 0, 0, 0, 0, 0, 0, R2, R1, 0,
    R1, R1, R2, R1, 0, 0, 0, 0, R1, R2, R1, R1,
    0, K, K, 0, 0, 0, 0, 0, 0, K, K, 0,
};
static const uint8_t px_jar[10 * 13] = {
    0, K, K, K, K, K, K, K, K, 0,
    0, K, G1, K, G1, G1, K, G1, K, 0,
    0, 0, K, K, K, K, K, K, 0, 0,
    0, K, W1, W2, W2, W2, W2, W1, K, 0,
    K, W1, W2, 0, 0, 0, 0, W2, W1, K,
    K, W1, 0, G2, G3, G3, G2, 0, W1, K,
    K, W1, G2, G3, G3, G3, G3, G2, W1, K,
    K, W1, G1, G2, K, G2, K, G2, W1, K,
    K, W1, 0, G1, G2, G2, G1, 0, W1, K,
    K, W1, 0, 0, 0, 0, 0, 0, W1, K,
    K, W1, W2, 0, 0, 0, 0, W2, W1, K,
    0, K, W1, W1, W1, W1, W1, W1, K, 0,
    0, 0, K, K, K, K, K, K, 0, 0,
};
static const uint8_t px_halo[10 * 3] = {
    0, 0, Y1, Y1, Y1, Y1, Y1, Y1, 0, 0,
    Y1, Y2, 0, 0, 0, 0, 0, 0, Y2, Y1,
    0, 0, Y2, Y2, Y2, Y2, Y2, Y2, 0, 0,
};
#undef K
static const DoomoImage ss_imgs[SS_IMG_COUNT] = {
    { 12, 5, px_horns, SS_UID_BASE + 0 },
    { 10, 13, px_jar, SS_UID_BASE + 1 },
    { 10, 3, px_halo, SS_UID_BASE + 2 },
};

const DoomoImage *ss_image(int k)
{
    return (k >= 0 && k < SS_IMG_COUNT) ? &ss_imgs[k] : NULL;
}

/* ---- procedural props -------------------------------------------------------------- */
/* Vapula, small: a grey cloud with two dot eyes and a pout */
void ss_tiny_cloud(int cx, int cy, int r, uint32_t t)
{
    int w = ui_wave((int)t, 60, 1);
    ui_fill_circle(cx - r / 2, cy + w, r * 2 / 3 + 1, G1);
    ui_fill_circle(cx + r / 2, cy - w, r * 2 / 3 + 1, G1);
    ui_fill_circle(cx, cy - r / 3, r, G1);
    ui_fill_circle(cx - r / 2, cy + w, r * 2 / 3, G2);
    ui_fill_circle(cx + r / 2, cy - w, r * 2 / 3, G2);
    ui_fill_circle(cx, cy - r / 3, r - 1, G3);
    plat_rect(cx - r / 3, cy - r / 3, 1 + r / 8, 1 + r / 8, PAL_BLACK);
    plat_rect(cx + r / 4, cy - r / 3, 1 + r / 8, 1 + r / 8, PAL_BLACK);
    plat_rect(cx - r / 6, cy + r / 6, r / 3 + 1, 1, PAL_RAMP(RAMP_STONE, 1));
}

/* the jar Vex keeps him in: bottom at y, s = scale; rain = drops inside */
void ss_jar(int cx, int y, int s, int rain, uint32_t t)
{
    int w = 12 * s, h = 16 * s, x = cx - w / 2, top = y - h, k;
    ui_round_rect(x - 1, top - 1, w + 2, h + 2, 3 * s, PAL_BLACK);
    ui_round_rect(x, top, w, h, 3 * s, PAL_RAMP(RAMP_SKY, 6));
    plat_checker(x + 1, top + 2, w - 2, h - 3, PAL_RAMP(RAMP_SKY, 4));
    ss_tiny_cloud(cx, top + h / 2 + s, 3 * s, t);
    if (rain)
        for (k = 0; k < 2; k++) {
            int dy = (int)((t * 2 + (uint32_t)k * 9) % (uint32_t)(h / 2));
            plat_rect(cx - 2 * s + k * 4 * s, top + h / 2 + 2 * s + dy, s, 2 * s, PAL_RAMP(RAMP_SKY, 3));
        }
    plat_rect(x - s, top - 2 * s, w + 2 * s, 3 * s, PAL_BLACK);            /* the lid */
    plat_rect(x, top - 2 * s + 1, w, 3 * s - 2, PAL_RAMP(RAMP_STONE, 4));
    for (k = 0; k < 3; k++) plat_rect(x + 2 * s + k * 3 * s, top - s, s, s, PAL_BLACK);   /* air holes */
    plat_rect(x + 2, top + 2, s, h / 2, PAL_WHITE);                          /* glint */
}

/* two little horns on a head whose top centre is (cx, y); size s */
void ss_horns(int cx, int y, int s)
{
    int k;
    for (k = -1; k <= 1; k += 2) {
        int hx = cx + k * 4 * s;
        plat_rect(hx - s, y - s, 3 * s, 2 * s, PAL_RAMP(RAMP_BLOOD, 2));
        plat_rect(hx - (k < 0 ? 0 : s) , y - 3 * s, 2 * s, 2 * s, PAL_RAMP(RAMP_BLOOD, 4));
        plat_rect(hx + (k < 0 ? -s : s), y - 4 * s, s, s, PAL_RAMP(RAMP_BLOOD, 6));
    }
}

void ss_halo(int cx, int y, int s)
{
    plat_rect(cx - 5 * s, y, 10 * s, s, PAL_RAMP(RAMP_CANDY, 6));
    plat_rect(cx - 6 * s, y + s, s, s, PAL_RAMP(RAMP_CANDY, 6));
    plat_rect(cx + 5 * s, y + s, s, s, PAL_RAMP(RAMP_CANDY, 6));
    plat_rect(cx - 5 * s, y + 2 * s, 10 * s, s, PAL_RAMP(RAMP_CANDY, 4));
}

/* Wisp: a small imp kid with big ears; ghost = see-through (Chapter 3) */
void ss_wisp(int cx, int y, int s, int ghost, uint32_t t)
{
    uint8_t body = ghost ? PAL_RAMP(RAMP_SKY, 6) : PAL_RAMP(RAMP_LAVA, 5);
    uint8_t dark = ghost ? PAL_RAMP(RAMP_SKY, 4) : PAL_RAMP(RAMP_BLOOD, 3);
    int bob = ghost ? ui_wave((int)t, 50, s) : 0, hy = y - 13 * s + bob;
    if (ghost) {
        plat_checker(cx - 4 * s, y - 8 * s + bob, 8 * s, 8 * s, body);
        plat_checker(cx - 5 * s, hy - 4 * s, 10 * s, 9 * s, body);
    } else {
        ui_round_rect(cx - 4 * s, y - 8 * s, 8 * s, 8 * s, 2 * s, dark);
        ui_round_rect(cx - 3 * s, y - 7 * s, 6 * s, 6 * s, 2 * s, PAL_RAMP(RAMP_SKY, 5));   /* pyjamas */
        ui_fill_circle(cx, hy, 5 * s, dark);
        ui_fill_circle(cx, hy, 5 * s - 1, body);
    }
    plat_rect(cx - 7 * s, hy - 4 * s, 3 * s, 2 * s, dark);                   /* ears */
    plat_rect(cx - 9 * s, hy - 6 * s, 2 * s, 2 * s, dark);
    plat_rect(cx + 4 * s, hy - 4 * s, 3 * s, 2 * s, dark);
    plat_rect(cx + 7 * s, hy - 6 * s, 2 * s, 2 * s, dark);
    plat_rect(cx - 2 * s, hy - s, s, s, PAL_BLACK);
    plat_rect(cx + s, hy - s, s, s, PAL_BLACK);
    plat_rect(cx - s, hy + 2 * s, 2 * s, s / 2 + 1, PAL_RAMP(RAMP_BLOOD, 1));
}

/* E1's punchline: a small poop that grows horns */
void ss_poop(int cx, int y, int s, int horns)
{
    uint8_t c0 = PAL_RAMP(RAMP_WOOD, 1), c1 = PAL_RAMP(RAMP_WOOD, 3), c2 = PAL_RAMP(RAMP_WOOD, 5);
    ui_round_rect(cx - 7 * s, y - 5 * s, 14 * s, 5 * s, 2 * s, c0);
    ui_round_rect(cx - 6 * s, y - 4 * s, 12 * s, 3 * s, s, c1);
    ui_round_rect(cx - 5 * s, y - 8 * s, 10 * s, 4 * s, 2 * s, c0);
    ui_round_rect(cx - 4 * s, y - 7 * s, 8 * s, 2 * s, s, c1);
    ui_fill_circle(cx, y - 9 * s, 2 * s, c0);
    ui_fill_circle(cx, y - 9 * s, 2 * s - 1, c2);
    plat_rect(cx - 2 * s, y - 6 * s, s, s, PAL_BLACK);
    plat_rect(cx + s, y - 6 * s, s, s, PAL_BLACK);
    if (horns) ss_horns(cx, y - 10 * s, s);
}
