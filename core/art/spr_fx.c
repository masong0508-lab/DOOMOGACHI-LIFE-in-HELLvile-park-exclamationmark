/* spr_fx.c - projectiles and effects (ANCHOR_CENTER: the image centre is
 * the object's centre, see SPRF_CENTERED in sprites.h).
 *
 * Gore comes in two flavours that the game picks per the gore setting:
 *   BRUTAL  blood_spray, gibs
 *   CUTE    confetti, hearts, stuffing (plush fluff, a button, a thread)
 */
#include "art.h"

#define M5(a, b, c, d, e) art_mat5((uint8_t)(a), (uint8_t)(b), (uint8_t)(c), (uint8_t)(d), (uint8_t)(e))
#define M4(a, b, c, d) art_mat4((uint8_t)(a), (uint8_t)(b), (uint8_t)(c), (uint8_t)(d))
#define M3(a, b, c) art_mat3((uint8_t)(a), (uint8_t)(b), (uint8_t)(c))

/* a flickering flame ball: tongues rotate with the frame */
static void flame_ball(int f, int r, const ArtMat *outer, const ArtMat *core, uint8_t hot)
{
    int k;
    for (k = 0; k < 6; k++) {
        int a = k * 683 + f * 300 + (k & 1) * 120;
        int len = r + 2 + ((k + f) % 3);
        spr_capsule(0, 0, r - 1, len * art_cos(a) / Q12, len * art_sin(a) / Q12 - 1, 0, outer, SH_GLOW | SH_SAMEPART);
    }
    spr_ellipse(0, 0, r, r, outer, SH_GLOW | SH_SAMEPART);
    spr_ellipse(-1, -1, r - 2 > 1 ? r - 2 : 1, r - 2 > 1 ? r - 2 : 1, core, SH_GLOW | SH_NOCONTOUR);
    spr_px(-2, -2, hot);
    spr_px(-1, -2, hot);
}

static void fireball(int f)
{
    ArtMat outer = M5(RP(RAMP_LAVA, 2), RP(RAMP_LAVA, 3), RP(RAMP_LAVA, 4), RP(RAMP_LAVA, 5), RP(RAMP_LAVA, 6));
    ArtMat core = M4(RP(RAMP_LAVA, 6), RP(RAMP_CANDY, 6), RP(RAMP_LAVA, 7), PAL_WHITE);
    flame_ball(f, 5, &outer, &core, PAL_WHITE);
    spr_outline_col(RP(RAMP_BLOOD, 2));
}

static void vape_bolt(int f)
{
    ArtMat swirl = M5(RP(RAMP_VAPE, 3), RP(RAMP_VAPE, 4), RP(RAMP_VAPE, 5), RP(RAMP_VAPE, 6), RP(RAMP_VAPE, 7));
    ArtMat core = M4(PP(PAINT_MAGENTA, 1), PP(PAINT_MAGENTA, 2), RP(RAMP_PASTEL_PINK, 7), PAL_WHITE);
    int k;
    /* spiral arms of vapour */
    for (k = 0; k < 3; k++) {
        int a = k * 1365 + f * 340;
        spr_bezier(0, 0, 6 * art_cos(a) / Q12, 6 * art_sin(a) / Q12,
                   7 * art_cos(a + 700) / Q12, 7 * art_sin(a + 700) / Q12, 3, 1, &swirl, SH_GLOW | SH_SAMEPART);
    }
    spr_ellipse(0, 0, 4, 4, &core, SH_GLOW);
    spr_px(-1, -1, PAL_WHITE);
    spr_twinkle(5 - f * 2, -5 + f, 1, PAL_WHITE, PP(PAINT_CYAN, 2));
    spr_outline_col(RP(RAMP_VAPE, 1));
}

static void bubble(int f)
{
    /* soap bubble: transparent middle, rainbow sheen ring, two highlights */
    int x, y, rx = f ? 6 : 5, ry = f ? 5 : 6;
    for (y = -ry - 1; y <= ry + 1; y++)
        for (x = -rx - 1; x <= rx + 1; x++) {
            int d = (2 * x + 1) * (2 * x + 1) * ry * ry + (2 * y + 1) * (2 * y + 1) * rx * rx;
            int R = 4 * rx * rx * ry * ry;
            int a;
            if (d > R) continue;
            if (d < R * 55 / 100) continue;                      /* see-through */
            a = art_atan2(2 * y + 1, 2 * x + 1);
            if (a < 1024) spr_px(x, y, PP(PAINT_CYAN, 2));
            else if (a < 2048) spr_px(x, y, PP(PAINT_LAVENDER, 2));
            else if (a < 3072) spr_px(x, y, PP(PAINT_BUBBLEGUM, 2));
            else spr_px(x, y, PP(PAINT_LEMON, 2));
        }
    spr_px(-rx / 2 - 1, -ry / 2, PAL_WHITE);
    spr_px(-rx / 2, -ry / 2 - 1, PAL_WHITE);
    spr_px(-rx / 2, -ry / 2, PAL_WHITE);
    spr_px(rx / 2, ry / 2, PAL_WHITE);
    spr_outline_col(RP(RAMP_SKY, 4));
}

static void glitter_shot(int f)
{
    static const int cols[5] = {PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_CYAN, PAINT_LAVENDER, PAINT_MINT};
    int k;
    spr_star(0, 0, 5, RP(RAMP_CANDY, 6));
    spr_star(0, 0, 3, RP(RAMP_CANDY, 7));
    spr_px(0, 0, PAL_WHITE);
    for (k = 0; k < 5; k++) {
        int a = k * 819 + f * 400, r = 7 + ((k + f) & 1);
        spr_px(r * art_cos(a) / Q12, r * art_sin(a) / Q12, PP(cols[k], 2));
        spr_px(r * art_cos(a) / Q12 + 1, r * art_sin(a) / Q12, PP(cols[k], 1));
    }
    spr_outline_col(RP(RAMP_CANDY, 2));
}

static void plasma(int f)
{
    ArtMat outer = M5(PP(PAINT_COBALT, 0), PP(PAINT_COBALT, 1), PP(PAINT_CYAN, 1), PP(PAINT_CYAN, 2), RP(RAMP_SKY, 7));
    ArtMat core = M3(PP(PAINT_CYAN, 2), RP(RAMP_SKY, 7), PAL_WHITE);
    int k;
    spr_ellipse(0, 0, 5, 5, &outer, SH_GLOW);
    spr_ellipse(-1, -1, 2, 2, &core, SH_GLOW | SH_NOCONTOUR);
    /* crackling arcs */
    for (k = 0; k < 3; k++) {
        int a = k * 1365 + f * 520;
        int x1 = 7 * art_cos(a) / Q12, y1 = 7 * art_sin(a) / Q12;
        spr_line(x1 / 2 + (k & 1), y1 / 2, x1, y1, PAL_WHITE);
    }
    spr_outline_col(PP(PAINT_NAVY, 1));
}

static void rocket(int f)
{
    /* rear view: steel tube with four fins and a roaring exhaust */
    ArtMat body = M5(RP(RAMP_STEEL, 2), RP(RAMP_STEEL, 3), RP(RAMP_STEEL, 4), RP(RAMP_STEEL, 5), RP(RAMP_STEEL, 6));
    ArtMat fin = M4(RP(RAMP_BLOOD, 3), RP(RAMP_BLOOD, 4), PP(PAINT_CHERRY, 1), PP(PAINT_CHERRY, 2));
    ArtMat fire = M5(RP(RAMP_LAVA, 4), RP(RAMP_LAVA, 5), RP(RAMP_LAVA, 6), RP(RAMP_CANDY, 7), PAL_WHITE);
    ArtMat core = M3(RP(RAMP_CANDY, 7), PAL_WHITE, PAL_WHITE);
    int k;
    spr_tri(-1, -7, 1, -7, 0, 0, &fin, 0);
    spr_tri(-1, 7, 1, 7, 0, 0, &fin, SH_SAMEPART);
    spr_tri(-7, -1, -7, 1, 0, 0, &fin, SH_SAMEPART);
    spr_tri(7, -1, 7, 1, 0, 0, &fin, SH_SAMEPART);
    spr_ellipse(0, 0, 4, 4, &body, 0);
    for (k = 0; k < 6; k++) {
        int a = k * 683 + f * 341, r = 3 + ((k + f) & 1);
        spr_capsule(0, 0, 2, r * art_cos(a) / Q12, r * art_sin(a) / Q12, 1, &fire, SH_GLOW | SH_NOCONTOUR);
    }
    spr_ellipse(0, 0, 2, 2, &core, SH_GLOW | SH_NOCONTOUR);
    spr_contours();
    spr_outline();
}

static void holy_splash(int f)
{
    ArtMat water = M5(RP(RAMP_SKY, 4), RP(RAMP_SKY, 5), RP(RAMP_SKY, 6), RP(RAMP_SKY, 7), PAL_WHITE);
    int k, n = 7, spread = 3 + f * 4;
    for (k = 0; k < n; k++) {
        int a = 2048 + 200 + k * (2048 - 400) / (n - 1);    /* upward fan */
        int r = spread + (k & 1) * 2;
        int x = r * art_cos(a) / Q12, y = r * art_sin(a) / Q12 + f * f / 2;
        int sz = f < 3 ? 2 - (f > 1) : 1;
        if (f == 3 && (k & 1)) continue;
        spr_ellipse(x, y, sz, sz + (f < 2), &water, SH_SPEC);
    }
    if (f < 2) spr_ellipse(0, 2, 3 + f * 2, 2, &water, 0);
    for (k = 0; k < 4; k++) {
        int a = k * 1024 + 512 + f * 200, r = 4 + f * 3;
        spr_twinkle(r * art_cos(a) / Q12, r * art_sin(a) / Q12 - 2, f == 3 ? 1 : 2, PAL_WHITE, RP(RAMP_CANDY, 6));
    }
    spr_outline_col(RP(RAMP_SKY, 3));
}

static void explosion(int f)
{
    ArtMat fire = M5(RP(RAMP_LAVA, 3), RP(RAMP_LAVA, 4), RP(RAMP_LAVA, 5), RP(RAMP_LAVA, 6), RP(RAMP_CANDY, 7));
    ArtMat hot = M3(RP(RAMP_CANDY, 6), RP(RAMP_LAVA, 7), PAL_WHITE);
    ArtMat smoke = M5(RP(RAMP_STONE, 2), RP(RAMP_STONE, 3), RP(RAMP_STONE, 4), RP(RAMP_STONE, 5), RP(RAMP_STONE, 6));
    int k;
    static const int R[5] = {5, 9, 12, 12, 11};
    int r = R[f];
    if (f >= 2) {
        /* smoke ring rolling outward */
        for (k = 0; k < 8; k++) {
            int a = k * 512 + f * 90;
            int rr = r + (f - 2) * 2;
            spr_ellipse(rr * art_cos(a) / Q12, rr * art_sin(a) / Q12 - f, 5 + (k & 1), 5, &smoke, SH_SAMEPART);
        }
    }
    if (f < 4) {
        for (k = 0; k < 7; k++) {
            int a = k * 585 + f * 200;
            int rr = r * 2 / 3;
            spr_ellipse(rr * art_cos(a) / Q12, rr * art_sin(a) / Q12, r / 2 + 1, r / 2 + 1, &fire,
                        SH_GLOW | SH_SAMEPART);
        }
        spr_ellipse(0, 0, r * 2 / 3, r * 2 / 3, &fire, SH_GLOW | SH_SAMEPART);
        if (f < 3) spr_ellipse(-1, -1, r / 3 + 1, r / 3 + 1, &hot, SH_GLOW | SH_NOCONTOUR);
    } else {
        spr_ellipse(0, -3, 8, 7, &smoke, 0);
    }
    /* flying sparks */
    for (k = 0; k < 6; k++) {
        int a = k * 683 + 100, rr = r + 3 + f * 2;
        if (f == 0) break;
        spr_px(rr * art_cos(a) / Q12, rr * art_sin(a) / Q12, f < 3 ? RP(RAMP_CANDY, 7) : RP(RAMP_LAVA, 4));
    }
    if (f >= 4) spr_erode(0xE4u + (uint32_t)f, 70);
    spr_clean();
    spr_outline();
}

static void puff(int f)
{
    ArtMat smoke = M5(RP(RAMP_STONE, 3), RP(RAMP_STONE, 4), RP(RAMP_STONE, 5), RP(RAMP_STONE, 6), RP(RAMP_STONE, 7));
    int r = 2 + f;
    spr_ellipse(0, -f, r, r, &smoke, 0);
    spr_ellipse(-r + 1, -f + 1, r - 1, r - 1, &smoke, SH_SAMEPART);
    spr_ellipse(r - 1, -f + 1, r - 1, r - 1, &smoke, SH_SAMEPART);
    if (f == 0) spr_px(0, 0, RP(RAMP_CANDY, 7));
    if (f >= 2) spr_erode(0x9Fu + (uint32_t)f, f == 2 ? 40 : 110);
    spr_clean();
    spr_outline();
}

static void blood_spray(int f)
{
    ArtMat blood = M4(RP(RAMP_BLOOD, 3), RP(RAMP_BLOOD, 4), RP(RAMP_BLOOD, 5), RP(RAMP_BLOOD, 6));
    int k;
    for (k = 0; k < 9; k++) {
        uint32_t h = art_hash((uint32_t)k, 0u, 0xB100u);
        int a = 2048 + 256 + (int)(h % 1536u), sp = 2 + (int)((h >> 12) % 4u);
        int r = sp * (f + 1) + 1;
        int x = r * art_cos(a) / Q12, y = r * art_sin(a) / Q12 + f * f;
        int sz = (k % 3 == 0) ? 2 : 1;
        if (f == 3 && (k & 1)) continue;
        spr_ellipse(x, y, sz, sz, &blood, SH_SPEC);
        if (f >= 1) spr_capsule(x, y, sz, x * 2 / 3, y * 2 / 3, 0, &blood, SH_SAMEPART);
    }
    if (f == 0) spr_ellipse(0, 0, 3, 3, &blood, SH_SPEC);
    spr_outline_col(RP(RAMP_BLOOD, 1));
}

static void gibs(int f)
{
    ArtMat meat = M5(RP(RAMP_FLESH, 2), RP(RAMP_FLESH, 3), RP(RAMP_FLESH, 4), RP(RAMP_FLESH, 5), RP(RAMP_FLESH, 6));
    ArtMat blood = M4(RP(RAMP_BLOOD, 2), RP(RAMP_BLOOD, 3), RP(RAMP_BLOOD, 4), RP(RAMP_BLOOD, 6));
    ArtMat bone = M4(RP(RAMP_BONE, 4), RP(RAMP_BONE, 5), RP(RAMP_BONE, 6), RP(RAMP_BONE, 7));
    ArtMat eye = M3(PAL_GRAY_L, PAL_WHITE, PAL_WHITE);
    int k;
    for (k = 0; k < 7; k++) {
        uint32_t h = art_hash((uint32_t)k, 1u, 0x61B5u);
        int a = 2048 + 200 + (int)(h % 1648u), sp = 3 + (int)((h >> 12) % 3u);
        int r = sp * (f + 1) + 2;
        int x = r * art_cos(a) / Q12, y = r * art_sin(a) / Q12 + f * f + f;
        int rot = (int)(h >> 4) + f * 700;
        switch (k % 4) {
        case 0: spr_ellipse_r(x, y, 3, 2, rot, &meat, 0); break;
        case 1: spr_ellipse_r(x, y, 2, 2, rot, &blood, SH_SPEC); break;
        case 2:
            spr_push(); spr_translate(x, y); spr_rotate(rot);
            spr_capsule(-3, 0, 1, 3, 0, 1, &bone, 0);
            spr_ellipse(-3, -1, 1, 1, &bone, SH_SAMEPART); spr_ellipse(3, 1, 1, 1, &bone, SH_SAMEPART);
            spr_pop();
            break;
        default:
            spr_ellipse(x, y, 2, 2, (k == 3) ? &eye : &meat, 0);
            if (k == 3) { spr_px(x, y, RP(RAMP_BLOOD, 5)); spr_px(x + 1, y, PAL_BLACK); }
            break;
        }
    }
    if (f == 0) spr_ellipse(0, 0, 4, 3, &blood, SH_SPEC);
    spr_contours();
    spr_outline();
}

static void confetti(int f)
{
    static const int cols[6] = {PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_SKY, PAINT_MINT, PAINT_LAVENDER, PAINT_TANGERINE};
    int k;
    for (k = 0; k < 22; k++) {
        uint32_t h = art_hash((uint32_t)k, 2u, 0xC0F7u);
        int a = 2048 + 100 + (int)(h % 1848u), sp = 2 + (int)((h >> 12) % 4u);
        int r = sp * (f + 1) + 1;
        int x = r * art_cos(a) / Q12, y = r * art_sin(a) / Q12 + f * f;
        int c = cols[k % 6], tw = ((int)(h >> 20) + f) & 3;
        if (tw == 0) { spr_px(x, y, PP(c, 2)); spr_px(x + 1, y, PP(c, 1)); spr_px(x, y + 1, PP(c, 1)); spr_px(x + 1, y + 1, PP(c, 0)); }
        else if (tw == 1) { spr_px(x, y, PP(c, 2)); spr_px(x + 1, y, PP(c, 1)); }
        else if (tw == 2) { spr_px(x, y, PP(c, 2)); spr_px(x, y + 1, PP(c, 0)); }
        else { spr_px(x, y, PP(c, 1)); }
    }
    /* two curly streamers */
    for (k = 0; k < 2; k++) {
        int i, sx = (k ? 4 : -4) * (f + 1), sy = -3 * (f + 1) + f * f;
        for (i = 0; i < 6; i++)
            spr_px(sx + (k ? i : -i), sy + ((i + f) % 3 == 0 ? 0 : 1) - i / 3, PP(k ? PAINT_CHERRY : PAINT_COBALT, 1 + (i & 1)));
    }
    if (f == 0) spr_star(0, 0, 4, RP(RAMP_CANDY, 7));
}

static void hearts(int f)
{
    static const int cols[3] = {PAINT_ROSE, PAINT_BUBBLEGUM, PAINT_CHERRY};
    int k;
    for (k = 0; k < 5; k++) {
        uint32_t h = art_hash((uint32_t)k, 3u, 0x4EA7u);
        int a = 2048 + 300 + (int)(h % 1448u), sp = 2 + (int)((h >> 12) % 3u);
        int r = sp * (f + 1);
        int x = r * art_cos(a) / Q12 + ((f + k) & 1), y = r * art_sin(a) / Q12 - f * 2;
        int c = cols[k % 3];
        int sz = (k == 0 && f < 3) ? 2 : (f == 3 ? 0 : 1);
        /* crisp bitmap heart drawn pixel by pixel through the transform */
        {
            static const char *const bm[3][8] = {
                {".X.X.", "XXXXX", "XXXXX", ".XXX.", "..X..", 0, 0, 0},
                {".XX.XX.", "XXXXXXX", "XXXXXXX", ".XXXXX.", "..XXX..", "...X...", 0, 0},
                {".XXX.XXX.", "XXXXXXXXX", "XXXXXXXXX", "XXXXXXXXX", ".XXXXXXX.", "..XXXXX..", "...XXX...", "....X...."}};
            int i, j, w = (int)strlen(bm[sz][0]), hh = 0;
            while (hh < 8 && bm[sz][hh]) hh++;
            for (j = 0; j < hh; j++)
                for (i = 0; i < w; i++)
                    if (bm[sz][j][i] == 'X') {
                        int edge = (i + 1 >= w || bm[sz][j][i + 1] != 'X') || (j + 1 >= hh || bm[sz][j + 1][i] != 'X');
                        spr_px(x - w / 2 + i, y - hh / 2 + j, edge ? PP(c, 0) : PP(c, 1));
                    }
            spr_px(x - w / 2 + 1, y - hh / 2 + 1, PAL_WHITE);
        }
    }
    if (f < 2) spr_twinkle(0, 0, 2, PAL_WHITE, PP(PAINT_BUBBLEGUM, 2));
    spr_outline_col(RP(RAMP_PASTEL_PINK, 2));
}

static void stuffing(int f)
{
    ArtMat fluff = M5(RP(RAMP_STONE, 5), RP(RAMP_STONE, 6), PP(PAINT_WHITE, 1), RP(RAMP_STONE, 7), PAL_WHITE);
    ArtMat button = M4(PP(PAINT_COBALT, 0), PP(PAINT_COBALT, 1), PP(PAINT_SKY, 1), PP(PAINT_SKY, 2));
    ArtMat fabric = M4(RP(RAMP_PASTEL_PINK, 3), RP(RAMP_PASTEL_PINK, 4), RP(RAMP_PASTEL_PINK, 5), RP(RAMP_PASTEL_PINK, 6));
    int k;
    for (k = 0; k < 6; k++) {
        uint32_t h = art_hash((uint32_t)k, 4u, 0x57F0u);
        int a = 2048 + 200 + (int)(h % 1648u), sp = 2 + (int)((h >> 12) % 3u);
        int r = sp * (f + 1) + 1;
        int x = r * art_cos(a) / Q12, y = r * art_sin(a) / Q12 + f * f / 2;
        int sz = 3 - (k & 1) - (f == 3);
        spr_fluffy(x, y, sz, sz, 5, Q12 * 35 / 100, f * 900, 0, 4096, &fluff, 0);
    }
    /* a button and a scrap of plush fabric with a stitch */
    {
        int bx = -3 - f * 3, by = -4 - f * 2 + f * f;
        spr_ellipse(bx, by, 2, 2, &button, 0);
        spr_px(bx - 1, by, PP(PAINT_NAVY, 1)); spr_px(bx + 1, by, PP(PAINT_NAVY, 1));
        spr_ellipse_r(4 + f * 3, -2 - f + f * f, 3, 2, f * 600, &fabric, 0);
        spr_px(4 + f * 3, -2 - f + f * f, PAL_WHITE);
    }
    spr_contours();
    spr_outline();
}

static void sparkle(int f)
{
    static const int r[4] = {2, 4, 3, 1};
    spr_twinkle(0, 0, r[f], PAL_WHITE, f == 1 ? RP(RAMP_CANDY, 7) : RP(RAMP_CANDY, 6));
    if (f == 1) {
        spr_px(-1, -1, RP(RAMP_CANDY, 6)); spr_px(1, -1, RP(RAMP_CANDY, 6));
        spr_px(-1, 1, RP(RAMP_CANDY, 6)); spr_px(1, 1, RP(RAMP_CANDY, 6));
    }
    spr_outline_col(RP(RAMP_CANDY, 2));
}

const ArtSprDef art_spr_fx[] = {
    {SPR_FIREBALL, "fireball", 4, ANCHOR_CENTER, SPRF_FULLBRIGHT | SPRF_FLOATS | SPRF_LOOP, fireball},
    {SPR_VAPE_BOLT, "vape_bolt", 4, ANCHOR_CENTER, SPRF_FULLBRIGHT | SPRF_FLOATS | SPRF_LOOP, vape_bolt},
    {SPR_BUBBLE, "bubble", 2, ANCHOR_CENTER, SPRF_FLOATS | SPRF_LOOP | SPRF_TRANSLUCENT, bubble},
    {SPR_GLITTER_SHOT, "glitter_shot", 4, ANCHOR_CENTER, SPRF_FULLBRIGHT | SPRF_FLOATS | SPRF_LOOP, glitter_shot},
    {SPR_PLASMA, "plasma", 4, ANCHOR_CENTER, SPRF_FULLBRIGHT | SPRF_FLOATS | SPRF_LOOP, plasma},
    {SPR_ROCKET, "rocket", 2, ANCHOR_CENTER, SPRF_FULLBRIGHT | SPRF_FLOATS | SPRF_LOOP, rocket},
    {SPR_HOLY_SPLASH, "holy_splash", 4, ANCHOR_CENTER, SPRF_FULLBRIGHT | SPRF_ONESHOT, holy_splash},
    {SPR_EXPLOSION, "explosion", 5, ANCHOR_CENTER, SPRF_FULLBRIGHT | SPRF_ONESHOT, explosion},
    {SPR_PUFF, "puff", 4, ANCHOR_CENTER, SPRF_ONESHOT, puff},
    {SPR_BLOOD_SPRAY, "blood_spray", 4, ANCHOR_CENTER, SPRF_ONESHOT, blood_spray},
    {SPR_GIBS, "gibs", 4, ANCHOR_CENTER, SPRF_ONESHOT, gibs},
    {SPR_CONFETTI, "confetti", 4, ANCHOR_CENTER, SPRF_FULLBRIGHT | SPRF_ONESHOT, confetti},
    {SPR_HEARTS, "hearts", 4, ANCHOR_CENTER, SPRF_FULLBRIGHT | SPRF_ONESHOT, hearts},
    {SPR_STUFFING, "stuffing", 4, ANCHOR_CENTER, SPRF_ONESHOT, stuffing},
    {SPR_SPARKLE, "sparkle", 4, ANCHOR_CENTER, SPRF_FULLBRIGHT | SPRF_LOOP, sparkle},
};
const int art_spr_fx_n = (int)(sizeof art_spr_fx / sizeof art_spr_fx[0]);
