/* spr_items.c - pickups and props (ANCHOR_GROUND: bottom row = floor).
 *
 * Pickups are small, glossy and glinting (frame 1 adds a twinkle), so they
 * pop against any floor. Props cover the cute town (trees, lamps,
 * fountain, benches, mailboxes, the Doomogachi egg and pet) and the hell
 * levels (barrels, skull piles, torches, corpses, the vape portal).
 */
#include "art.h"

#define M5(a, b, c, d, e) art_mat5((uint8_t)(a), (uint8_t)(b), (uint8_t)(c), (uint8_t)(d), (uint8_t)(e))
#define M4(a, b, c, d) art_mat4((uint8_t)(a), (uint8_t)(b), (uint8_t)(c), (uint8_t)(d))
#define M3(a, b, c) art_mat3((uint8_t)(a), (uint8_t)(b), (uint8_t)(c))
#define MR(r, lo, hi) art_mat_ramp((r), (lo), (hi))

static void finish(void)
{
    spr_contours();
    spr_outline();
}

/* small crisp heart through the canvas transform */
static void heart_px(int x, int y, int sz, uint8_t fill, uint8_t dark, uint8_t hi)
{
    static const char *const bm[3][8] = {
        {".X.X.", "XXXXX", "XXXXX", ".XXX.", "..X..", 0, 0, 0},
        {".XX.XX.", "XXXXXXX", "XXXXXXX", ".XXXXX.", "..XXX..", "...X...", 0, 0},
        {".XXX.XXX.", "XXXXXXXXX", "XXXXXXXXX", "XXXXXXXXX", ".XXXXXXX.", "..XXXXX..", "...XXX...", "....X...."}};
    int i, j, w = (int)strlen(bm[sz][0]), h = 0;
    while (h < 8 && bm[sz][h]) h++;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (bm[sz][j][i] == 'X') {
                int edge = (i + 1 >= w || bm[sz][j][i + 1] != 'X') || (j + 1 >= h || bm[sz][j + 1][i] != 'X');
                spr_px(x - w / 2 + i, y - h / 2 + j, edge ? dark : fill);
            }
    if (hi) spr_px(x - w / 2 + 1, y - h / 2 + 1, hi);
}

static void glint(int f, int x, int y)
{
    if (f == 1) spr_twinkle(x, y, 2, PAL_WHITE, RP(RAMP_CANDY, 7));
}

/* ---- pickups ------------------------------------------------------------------- */
static void health_small(int f)
{
    ArtMat milk = M5(RP(RAMP_PASTEL_PINK, 3), RP(RAMP_PASTEL_PINK, 4), RP(RAMP_PASTEL_PINK, 5),
                     RP(RAMP_PASTEL_PINK, 6), RP(RAMP_PASTEL_PINK, 7));
    ArtMat glass = M3(RP(RAMP_SKY, 6), RP(RAMP_SKY, 7), PAL_WHITE);
    ArtMat cap = M4(RP(RAMP_BLOOD, 4), PP(PAINT_CHERRY, 1), PP(PAINT_CHERRY, 2), PP(PAINT_ROSE, 2));
    spr_capsule(0, -10, 4, 0, -4, 4, &milk, SH_CYL | SH_SPEC);
    spr_rect(-2, -16, 4, 4, &glass, SH_CYL);
    spr_ellipse(0, -16, 2, 1, &cap, 0);
    heart_px(0, -6, 0, PP(PAINT_CHERRY, 1), PP(PAINT_CHERRY, 0), PAL_WHITE);
    finish();
    glint(f, -3, -13);
}

static void health_big(int f)
{
    ArtMat box = M5(RP(RAMP_PASTEL_PINK, 4), RP(RAMP_PASTEL_PINK, 5), RP(RAMP_PASTEL_PINK, 6),
                    RP(RAMP_PASTEL_PINK, 7), PAL_WHITE);
    ArtMat lid = M4(RP(RAMP_PASTEL_PINK, 5), RP(RAMP_PASTEL_PINK, 6), RP(RAMP_PASTEL_PINK, 7), PAL_WHITE);
    ArtMat handle = MR(RAMP_STEEL, 3, 6);
    spr_capsule(-4, -14, 1, 4, -14, 1, &handle, 0);
    spr_rect(-9, -12, 18, 12, &box, 0);
    spr_rect(-9, -13, 18, 4, &lid, 0);
    heart_px(0, -5, 1, PP(PAINT_CHERRY, 1), PP(PAINT_CHERRY, 0), PAL_WHITE);
    spr_px(-8, -11, PAL_WHITE);
    finish();
    glint(f, 6, -12);
}

static void armor(int f)
{
    ArtMat plate = M5(RP(RAMP_MINT, 3), RP(RAMP_MINT, 4), RP(RAMP_MINT, 5), RP(RAMP_MINT, 6), RP(RAMP_MINT, 7));
    ArtMat gold = MR(RAMP_CANDY, 3, 7);
    int p[16];
    p[0] = -9; p[1] = -16; p[2] = -4; p[3] = -14; p[4] = 4; p[5] = -14; p[6] = 9; p[7] = -16;
    p[8] = 8; p[9] = -5; p[10] = 0; p[11] = 0; p[12] = -8; p[13] = -5; p[14] = -9; p[15] = -12;
    spr_poly(p, 8, &plate, 0);
    spr_capsule(-8, -15, 1, -4, -13, 1, &gold, 0);
    spr_capsule(8, -15, 1, 4, -13, 1, &gold, 0);
    heart_px(0, -8, 1, PP(PAINT_ROSE, 1), PP(PAINT_ROSE, 0), PAL_WHITE);
    finish();
    glint(f, -6, -12);
}

static void bullet(int x, int y)
{
    ArtMat brass = MR(RAMP_CANDY, 3, 7);
    ArtMat tip = M3(PP(PAINT_TANGERINE, 0), PP(PAINT_TANGERINE, 1), PP(PAINT_TANGERINE, 2));
    spr_rect(x - 1, y - 4, 3, 4, &brass, SH_CYL);
    spr_ellipse(x, y - 5, 1, 1, &tip, 0);
}

static void ammo_bullets(int f)
{
    ArtMat box = M5(RP(RAMP_WOOD, 2), RP(RAMP_WOOD, 3), RP(RAMP_WOOD, 4), RP(RAMP_WOOD, 5), RP(RAMP_WOOD, 6));
    (void)f;
    bullet(-4, -6);
    bullet(0, -7);
    bullet(4, -6);
    spr_rect(-7, -7, 14, 7, &box, 0);
    spr_rect(-4, -5, 8, 3, &box, SH_FLAT);
    spr_px(-3, -4, RP(RAMP_CANDY, 6)); spr_px(-1, -4, RP(RAMP_CANDY, 6)); spr_px(1, -4, RP(RAMP_CANDY, 6));
    finish();
}

static void ammo_shells(int f)
{
    ArtMat red = M5(RP(RAMP_BLOOD, 3), RP(RAMP_BLOOD, 4), PP(PAINT_CHERRY, 1), PP(PAINT_CHERRY, 2), PP(PAINT_ROSE, 2));
    ArtMat brass = MR(RAMP_CANDY, 3, 7);
    int k;
    static const int xs[4] = {-6, -2, 2, 6}, ys[4] = {0, -1, 0, -1};
    (void)f;
    for (k = 0; k < 4; k++) {
        spr_rect(xs[k] - 1, ys[k] - 9, 3, 6, &red, SH_CYL);
        spr_rect(xs[k] - 1, ys[k] - 3, 3, 3, &brass, SH_CYL);
    }
    finish();
}

static void ammo_cells(int f)
{
    ArtMat body = MR(RAMP_STEEL, 2, 6);
    ArtMat band = f ? M3(PP(PAINT_CYAN, 1), PP(PAINT_CYAN, 2), PAL_WHITE) : M3(PP(PAINT_CYAN, 0), PP(PAINT_CYAN, 1), PP(PAINT_CYAN, 2));
    spr_rect(-5, -15, 10, 15, &body, SH_CYL);
    spr_rect(-2, -17, 4, 2, &body, SH_CYL);
    spr_rect(-5, -11, 10, 6, &band, SH_CYL | SH_GLOW);
    spr_line(1, -11, -1, -8, PAL_WHITE);
    spr_line(-1, -8, 1, -8, PAL_WHITE);
    spr_line(1, -8, -1, -5, PAL_WHITE);
    finish();
}

static void ammo_rockets(int f)
{
    ArtMat body = MR(RAMP_STEEL, 3, 7);
    ArtMat nose = M4(RP(RAMP_BLOOD, 4), PP(PAINT_CHERRY, 1), PP(PAINT_CHERRY, 2), PP(PAINT_ROSE, 2));
    int k;
    (void)f;
    for (k = -1; k <= 1; k += 2) {
        spr_capsule(k * 3, -3, 2, k * 4, -13, 2, &body, SH_CYL);
        spr_ellipse(k * 4, -15, 2, 2, &nose, 0);
        spr_tri(k * 3 - 3, -1, k * 3 + 3, -1, k * 3, -5, &body, 0);
    }
    finish();
}

static void key(int f, int paint)
{
    ArtMat gold = MR(RAMP_CANDY, 3, 7);
    spr_rect(-1, -10, 3, 9, &gold, SH_CYL);
    spr_rect(2, -4, 3, 2, &gold, 0);
    spr_rect(2, -1, 2, 1, &gold, 0);
    finish();
    heart_px(0, -13, 2, PP(paint, 1), PP(paint, 0), PAL_WHITE);
    spr_outline();
    glint(f, 4, -15);
}

static void key_red(int f) { key(f, PAINT_CHERRY); }
static void key_blue(int f) { key(f, PAINT_COBALT); }

static void scrap(int f)
{
    ArtMat steel = MR(RAMP_STEEL, 2, 6);
    ArtMat rust = M4(PP(PAINT_RUST, 0), PP(PAINT_RUST, 1), PP(PAINT_RUST, 2), RP(RAMP_WOOD, 5));
    ArtMat gold = MR(RAMP_CANDY, 3, 6);
    int k;
    (void)f;
    spr_tri(-8, 0, 2, 0, -5, -6, &rust, 0);
    spr_rect(-2, -5, 9, 5, &steel, 0);
    for (k = 0; k < 8; k++) {
        int a = k * 512;
        spr_capsule(3, -7, 1, 3 + 4 * art_cos(a) / Q12, -7 + 4 * art_sin(a) / Q12, 1, &gold, SH_SAMEPART);
    }
    spr_ellipse(3, -7, 3, 3, &gold, SH_SAMEPART);
    spr_px(3, -7, RP(RAMP_CANDY, 1));
    spr_ellipse(-5, -2, 1, 1, &steel, 0);
    finish();
}

static void soul_orb(int f)
{
    ArtMat orb = M5(PP(PAINT_ECTOPLASM, 0), PP(PAINT_ECTOPLASM, 1), RP(RAMP_MINT, 6), PP(PAINT_ECTOPLASM, 2), PAL_WHITE);
    ArtMat halo = M3(RP(RAMP_MINT, 3), RP(RAMP_MINT, 5), RP(RAMP_MINT, 6));
    static const int bob[4] = {0, -1, -2, -1};
    int y = -9 + bob[f], r = 5 + (f == 2);
    part_glow(0, y, r + 4, &halo);
    spr_ellipse(0, y, r, r, &orb, SH_GLOW | SH_SPEC);
    /* a little ghost smile inside */
    spr_px(-2, y - 1, RP(RAMP_MINT, 2));
    spr_px(2, y - 1, RP(RAMP_MINT, 2));
    spr_px(-1, y + 1, RP(RAMP_MINT, 3));
    spr_px(0, y + 2, RP(RAMP_MINT, 3));
    spr_px(1, y + 1, RP(RAMP_MINT, 3));
    spr_twinkle(5 - f, y - 6 + f, 1, PAL_WHITE, RP(RAMP_MINT, 6));
}

static void candy(int f)
{
    ArtMat swirl = M5(PP(PAINT_ROSE, 0), PP(PAINT_ROSE, 1), PP(PAINT_BUBBLEGUM, 1), PP(PAINT_BUBBLEGUM, 2), PAL_WHITE);
    ArtMat wrap = M4(RP(RAMP_PASTEL_PINK, 4), RP(RAMP_PASTEL_PINK, 5), RP(RAMP_PASTEL_PINK, 6), RP(RAMP_PASTEL_PINK, 7));
    int k;
    spr_tri(-4, -4, -9, -8, -9, 0, &wrap, 0);
    spr_tri(4, -4, 9, -8, 9, 0, &wrap, 0);
    spr_ellipse(0, -4, 4, 4, &swirl, SH_SPEC);
    for (k = 0; k < 6; k++) {
        int a = k * 800;
        spr_px(k * art_cos(a) / Q12 / 2, -4 + k * art_sin(a) / Q12 / 2, PAL_WHITE);
    }
    finish();
    glint(f, 3, -8);
}

/* ---- props ----------------------------------------------------------------------- */
static void tree(int f)
{
    ArtMat bark = M5(RP(RAMP_WOOD, 1), RP(RAMP_WOOD, 2), RP(RAMP_WOOD, 3), RP(RAMP_WOOD, 4), RP(RAMP_WOOD, 5));
    ArtMat leaf = M5(RP(RAMP_GRASS, 2), RP(RAMP_GRASS, 3), RP(RAMP_GRASS, 4), RP(RAMP_GRASS, 5), RP(RAMP_GRASS, 6));
    ArtMat bloom = M3(PP(PAINT_BUBBLEGUM, 1), PP(PAINT_BUBBLEGUM, 2), PAL_WHITE);
    int k;
    (void)f;
    spr_capsule(0, -2, 4, 0, -20, 3, &bark, SH_CYL);
    spr_capsule(0, -16, 2, -6, -22, 1, &bark, SH_CYL);
    spr_fluffy(-9, -26, 9, 8, 9, Q12 * 2 / 10, 0, 1024, 4096, &leaf, 0);
    spr_fluffy(9, -27, 9, 8, 9, Q12 * 2 / 10, 0, 3072, 2048, &leaf, 0);
    spr_fluffy(0, -36, 12, 10, 12, Q12 * 2 / 10, 0, 1800, 0, &leaf, 0);
    spr_fluffy(0, -24, 10, 7, 9, Q12 * 2 / 10, 0, 300, 2800, &leaf, 0);
    for (k = 0; k < 7; k++) {
        uint32_t h = art_hash((uint32_t)k, 0u, 0x72EEu);
        int bx = (int)(h % 25u) - 12, by = -40 + (int)((h >> 8) % 20u);
        spr_px(bx, by, bloom.c[1]);
        spr_px(bx + 1, by, bloom.c[0]);
        spr_px(bx, by - 1, bloom.c[2]);
    }
    finish();
}

static void flower_bed(int f)
{
    ArtMat soil = M4(RP(RAMP_WOOD, 1), RP(RAMP_WOOD, 2), RP(RAMP_WOOD, 3), RP(RAMP_WOOD, 4));
    ArtMat leaf = MR(RAMP_GRASS, 3, 6);
    static const int cols[5] = {PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_LAVENDER, PAINT_CHERRY, PAINT_WHITE};
    int k;
    (void)f;
    spr_ellipse(0, -3, 14, 4, &soil, 0);
    for (k = 0; k < 7; k++) {
        int x = -12 + k * 4, h = 6 + (k * 5) % 4;
        int c = cols[k % 5];
        spr_line(x, -4, x, -4 - h, RP(RAMP_GRASS, 3));
        spr_ellipse(x + 1, -6 - h / 2, 1, 1, &leaf, 0);
        spr_ellipse(x, -5 - h, 2, 2, &leaf, SH_FLAT | SH_NOCONTOUR);
        spr_px(x - 1, -5 - h, PP(c, 1)); spr_px(x + 1, -5 - h, PP(c, 0));
        spr_px(x, -6 - h, PP(c, 2)); spr_px(x, -4 - h, PP(c, 0));
        spr_px(x - 1, -6 - h, PP(c, 2)); spr_px(x + 1, -4 - h, PP(c, 0));
        spr_px(x, -5 - h, RP(RAMP_CANDY, 5));
    }
    finish();
}

static void lamp(int f)
{
    ArtMat pole = M4(RP(RAMP_STONE, 5), RP(RAMP_STONE, 6), RP(RAMP_STONE, 7), PAL_WHITE);
    ArtMat base = MR(RAMP_MINT, 2, 6);
    ArtMat bulb = M4(RP(RAMP_CANDY, 6), RP(RAMP_CANDY, 7), RP(RAMP_PEACH, 7), PAL_WHITE);
    ArtMat halo = M3(RP(RAMP_CANDY, 4), RP(RAMP_CANDY, 5), RP(RAMP_CANDY, 6));
    int y;
    (void)f;
    spr_rect(-4, -4, 8, 4, &base, 0);
    spr_capsule(0, -4, 1, 0, -36, 1, &pole, SH_CYL);
    for (y = -34; y < -4; y += 4) { spr_px(-1, y, PP(PAINT_CHERRY, 1)); spr_px(0, y + 1, PP(PAINT_CHERRY, 1)); spr_px(1, y + 2, PP(PAINT_CHERRY, 0)); }
    spr_bezier(0, -36, 2, -42, 7, -40, 1, 1, &pole, SH_CYL);
    spr_rect(5, -40, 5, 2, &base, 0);
    part_glow(7, -35, 7, &halo);
    spr_ellipse(7, -35, 3, 3, &bulb, SH_GLOW);
    finish();
}

static void bench(int f)
{
    ArtMat wood = M5(RP(RAMP_MINT, 3), RP(RAMP_MINT, 4), RP(RAMP_MINT, 5), RP(RAMP_MINT, 6), RP(RAMP_MINT, 7));
    ArtMat iron = MR(RAMP_STONE, 1, 4);
    int k;
    (void)f;
    for (k = -1; k <= 1; k += 2) {
        spr_capsule(k * 11, -8, 1, k * 12, -1, 1, &iron, 0);
        spr_capsule(k * 11, -8, 1, k * 11, -18, 1, &iron, 0);
    }
    spr_rect(-14, -17, 28, 3, &wood, 0);
    spr_rect(-14, -13, 28, 3, &wood, 0);
    spr_rect(-15, -9, 30, 3, &wood, 0);
    heart_px(0, -15, 0, PP(PAINT_ROSE, 1), PP(PAINT_ROSE, 0), 0);
    finish();
}

static void fountain(int f)
{
    ArtMat stone = M5(RP(RAMP_STONE, 4), RP(RAMP_STONE, 5), RP(RAMP_STONE, 6), RP(RAMP_STONE, 7), PAL_WHITE);
    ArtMat water = M5(RP(RAMP_SKY, 4), RP(RAMP_SKY, 5), RP(RAMP_SKY, 6), RP(RAMP_SKY, 7), PAL_WHITE);
    int k;
    /* basin */
    spr_ellipse(0, -6, 20, 6, &stone, 0);
    spr_rect(-20, -6, 40, 5, &stone, SH_CYL | SH_SAMEPART);
    spr_ellipse(0, -8, 17, 4, &water, SH_NOCONTOUR);
    /* pillar + bowl */
    spr_capsule(0, -8, 3, 0, -22, 2, &stone, SH_CYL);
    spr_ellipse(0, -23, 9, 3, &stone, 0);
    spr_ellipse(0, -24, 7, 2, &water, SH_NOCONTOUR);
    spr_capsule(0, -24, 1, 0, -30, 1, &stone, SH_CYL);
    /* spray and arcing streams (animated) */
    for (k = 0; k < 4; k++) {
        int ph = (f + k) & 3;
        spr_px(-2 + k, -32 - ph, k & 1 ? RP(RAMP_SKY, 7) : PAL_WHITE);
    }
    for (k = -1; k <= 1; k += 2) {
        int i;
        for (i = 0; i < 6; i++) {
            int t = i * 2 + ((f & 1) ? 1 : 0);
            int x = k * (8 + t), y = -23 + (t * t) / 9;
            spr_px(x, y, ((i + f) & 1) ? RP(RAMP_SKY, 7) : RP(RAMP_SKY, 6));
        }
        spr_px(k * (19 - (f & 1)), -9, PAL_WHITE);
    }
    for (k = 0; k < 3; k++) spr_px(-12 + k * 9 + f * 2, -8, PAL_WHITE);
    spr_outline();
}

static void barrel(int f)
{
    ArtMat steel = M5(RP(RAMP_STEEL, 2), RP(RAMP_STEEL, 3), RP(RAMP_SICK, 3), RP(RAMP_STEEL, 5), RP(RAMP_STEEL, 6));
    ArtMat rim = MR(RAMP_STEEL, 2, 5);
    ArtMat slime = M4(RP(RAMP_SICK, 5), PP(PAINT_SLIME, 1), PP(PAINT_SLIME, 2), RP(RAMP_SICK, 7));
    spr_rect(-7, -20, 14, 20, &steel, SH_CYL);
    spr_rect(-7, -15, 14, 2, &rim, SH_CYL | SH_SAMEPART);
    spr_rect(-7, -6, 14, 2, &rim, SH_CYL | SH_SAMEPART);
    spr_ellipse(0, -20, 7, 2, &slime, SH_GLOW);
    spr_capsule(-4, -20, 1, -4, -13 - f, 1, &slime, SH_GLOW);
    /* hazard symbol */
    spr_tri(-3, -8, 3, -8, 0, -13, &slime, SH_FLAT);
    spr_px(0, -10, RP(RAMP_STEEL, 1));
    if (f) { spr_px(2, -22, RP(RAMP_SICK, 7)); spr_px(3, -23, PAL_WHITE); }
    else spr_px(-1, -21, RP(RAMP_SICK, 7));
    finish();
}

static void skull_at(int x, int y, const ArtMat *bone)
{
    spr_ellipse(x, y, 4, 4, bone, 0);
    spr_ellipse(x, y + 3, 3, 1, bone, SH_SAMEPART);
    spr_px(x - 2, y, RP(RAMP_BONE, 0)); spr_px(x - 1, y, RP(RAMP_BONE, 1));
    spr_px(x + 1, y, RP(RAMP_BONE, 0)); spr_px(x + 2, y, RP(RAMP_BONE, 1));
    spr_px(x, y + 2, RP(RAMP_BONE, 1));
}

static void skull_pile(int f)
{
    ArtMat bone = MR(RAMP_BONE, 3, 7);
    (void)f;
    spr_capsule(-12, -2, 1, 10, -3, 1, &bone, 0);
    skull_at(-8, -5, &bone);
    skull_at(0, -5, &bone);
    skull_at(8, -5, &bone);
    skull_at(-4, -12, &bone);
    skull_at(4, -12, &bone);
    skull_at(0, -19, &bone);
    spr_px(-1, -19, RP(RAMP_LAVA, 5)); spr_px(1, -19, RP(RAMP_LAVA, 5));
    finish();
}

static void corpse_cute(int f)
{
    ArtMat plush = M5(RP(RAMP_PEACH, 4), RP(RAMP_PEACH, 5), RP(RAMP_PEACH, 6), RP(RAMP_PEACH, 7), PAL_WHITE);
    ArtMat fluff = M4(RP(RAMP_STONE, 6), RP(RAMP_STONE, 7), PP(PAINT_WHITE, 1), PAL_WHITE);
    ArtMat button = M3(PP(PAINT_JET, 1), PP(PAINT_CHARCOAL, 1), PP(PAINT_CHARCOAL, 2));
    (void)f;
    spr_ellipse(-14, -9, 2, 5, &plush, 0);       /* floppy bunny ears */
    spr_ellipse(-10, -10, 2, 5, &plush, 0);
    spr_ellipse(3, -4, 10, 4, &plush, 0);
    spr_ellipse(-9, -5, 6, 5, &plush, 0);
    spr_ellipse(12, -3, 2, 2, &plush, 0);
    spr_fluffy(5, -8, 3, 2, 5, Q12 * 4 / 10, 0, 2048, 0, &fluff, 0);
    spr_fluffy(-1, -8, 2, 2, 4, Q12 * 4 / 10, 0, 0, 4096, &fluff, 0);
    spr_ellipse(-11, -6, 1, 1, &button, 0);
    spr_ellipse(-7, -6, 1, 1, &button, 0);
    spr_px(-11, -6, PAL_GRAY); spr_px(-7, -6, PAL_GRAY);
    spr_line(-10, -3, -8, -3, PP(PAINT_ROSE, 0));
    spr_line(0, -2, 6, -2, PP(PAINT_ROSE, 1));          /* stitches */
    spr_px(1, -1, PP(PAINT_ROSE, 1)); spr_px(3, -1, PP(PAINT_ROSE, 1)); spr_px(5, -1, PP(PAINT_ROSE, 1));
    finish();
}

static void corpse_brutal(int f)
{
    ArtMat meat = M5(RP(RAMP_FLESH, 2), RP(RAMP_FLESH, 3), RP(RAMP_FLESH, 4), RP(RAMP_FLESH, 5), RP(RAMP_FLESH, 6));
    ArtMat blood = M4(RP(RAMP_BLOOD, 1), RP(RAMP_BLOOD, 2), RP(RAMP_BLOOD, 3), RP(RAMP_BLOOD, 5));
    ArtMat bone = MR(RAMP_BONE, 3, 7);
    int k;
    (void)f;
    spr_ellipse(0, -2, 15, 3, &blood, SH_SPEC);
    spr_ellipse(-3, -5, 8, 4, &meat, 0);
    for (k = 0; k < 4; k++) spr_capsule(-8 + k * 3, -6, 1, -7 + k * 3, -10, 0, &bone, 0);
    spr_ellipse(7, -4, 4, 3, &meat, 0);
    spr_ellipse(9, -5, 2, 2, &bone, 0);
    spr_px(9, -5, RP(RAMP_BONE, 1));
    spr_px(-6, -3, RP(RAMP_BLOOD, 6)); spr_px(2, -2, RP(RAMP_BLOOD, 6));
    finish();
}

static void torch(int f)
{
    ArtMat iron = MR(RAMP_STEEL, 1, 4);
    ArtMat outer = M5(RP(RAMP_LAVA, 2), RP(RAMP_LAVA, 3), RP(RAMP_LAVA, 4), RP(RAMP_LAVA, 5), RP(RAMP_LAVA, 6));
    ArtMat core = M3(RP(RAMP_LAVA, 6), RP(RAMP_CANDY, 7), PAL_WHITE);
    static const int sway[4] = {0, 1, 0, -1}, tall[4] = {9, 11, 10, 12};
    spr_rect(-3, -3, 6, 3, &iron, 0);
    spr_capsule(0, -3, 1, 0, -22, 1, &iron, SH_CYL);
    spr_ellipse(0, -23, 5, 2, &iron, 0);
    /* teardrop flame: round base, licking tip that sways */
    spr_ellipse(0, -27, 4, 4, &outer, SH_GLOW);
    spr_bezier(0, -28, sway[f], -30 - tall[f] / 2, sway[f] * 2, -26 - tall[f], 3, 0, &outer, SH_GLOW | SH_SAMEPART);
    spr_ellipse(0, -27, 2, 2, &core, SH_GLOW | SH_NOCONTOUR);
    spr_bezier(0, -28, sway[f], -30, sway[f], -25 - tall[f] / 2, 1, 0, &core, SH_GLOW | SH_NOCONTOUR);
    spr_px(-sway[f] * 2 + 1, -31 - tall[f], RP(RAMP_LAVA, 5));
    finish();
}

static void sign(int f)
{
    ArtMat wood = M5(RP(RAMP_WOOD, 2), RP(RAMP_WOOD, 3), RP(RAMP_WOOD, 4), RP(RAMP_WOOD, 5), RP(RAMP_WOOD, 6));
    ArtMat board = M5(RP(RAMP_WOOD, 4), RP(RAMP_WOOD, 5), RP(RAMP_WOOD, 6), RP(RAMP_WOOD, 7), RP(RAMP_PEACH, 7));
    int p[10];
    (void)f;
    spr_rect(-2, -20, 4, 20, &wood, SH_CYL);
    p[0] = -11; p[1] = -24; p[2] = 7; p[3] = -24; p[4] = 12; p[5] = -19; p[6] = 7; p[7] = -14; p[8] = -11; p[9] = -14;
    spr_poly(p, 5, &board, 0);
    heart_px(-4, -19, 0, PP(PAINT_ROSE, 1), PP(PAINT_ROSE, 0), PAL_WHITE);
    spr_line(0, -20, 6, -20, RP(RAMP_WOOD, 2));
    spr_line(0, -18, 4, -18, RP(RAMP_WOOD, 2));
    spr_ellipse(-3, -1, 4, 1, &wood, 0);
    finish();
}

static void mailbox(int f)
{
    ArtMat box = M5(RP(RAMP_SKY, 4), RP(RAMP_SKY, 5), RP(RAMP_SKY, 6), RP(RAMP_SKY, 7), PAL_WHITE);
    ArtMat post = MR(RAMP_WOOD, 2, 5);
    ArtMat flag = M3(PP(PAINT_CHERRY, 0), PP(PAINT_CHERRY, 1), PP(PAINT_CHERRY, 2));
    (void)f;
    spr_rect(-1, -14, 3, 14, &post, SH_CYL);
    spr_capsule(-5, -18, 4, 5, -18, 4, &box, SH_CYL);
    spr_rect(-5, -18, 10, 4, &box, SH_CYL | SH_SAMEPART);
    spr_ellipse(-5, -17, 1, 2, &box, 0);
    spr_capsule(6, -18, 1, 6, -26, 1, &post, 0);
    heart_px(8, -26, 0, flag.c[1], flag.c[0], flag.c[2]);
    finish();
}

static void vape_portal(int f)
{
    ArtMat rim = M5(RP(RAMP_VAPE, 3), RP(RAMP_VAPE, 4), RP(RAMP_VAPE, 5), RP(RAMP_VAPE, 6), RP(RAMP_VAPE, 7));
    int x, y, k;
    /* swirling vortex: spiral bands rotate each frame */
    for (y = -40; y <= 0; y++)
        for (x = -15; x <= 15; x++) {
            int dx = x * 16 + 8, dy = (y + 20) * 16 * 15 / 20;
            int d = art_dist(dx, dy) / 16, a;
            int band;
            if (d > 14) continue;
            a = art_atan2(dy, dx);
            band = art_wrap(a + d * 220 - f * 1024, 4096) / 683;     /* 6 arms */
            if (d < 3) spr_px(x, y, PAL_WHITE);
            else if (d < 5) spr_px(x, y, (band & 1) ? PP(PAINT_MAGENTA, 2) : RP(RAMP_PASTEL_PINK, 7));
            else spr_px(x, y, (band & 1) ? RP(RAMP_VAPE, d > 10 ? 1 : 3) : (d > 10 ? RP(RAMP_VAPE, 2) : PP(PAINT_MAGENTA, 1)));
        }
    /* puffy rim */
    for (k = 0; k < 12; k++) {
        int a = k * 341 + f * 85;
        int px = 15 * art_cos(a) / Q12, py = -20 + 20 * art_sin(a) / Q12;
        spr_ellipse(px, py, 3, 3, &rim, k ? SH_SAMEPART | SH_BEHIND : SH_BEHIND);
    }
    for (k = 0; k < 3; k++) {
        uint32_t h = art_hash((uint32_t)k, (uint32_t)f, 0x9087u);
        spr_twinkle((int)(h % 25u) - 12, -34 + (int)((h >> 8) % 30u), 1, PAL_WHITE, PP(PAINT_CYAN, 2));
    }
    spr_outline();
}

static void egg(int f)
{
    ArtMat shell = M5(RP(RAMP_PEACH, 5), RP(RAMP_PEACH, 6), RP(RAMP_PEACH, 7), PAL_WHITE, PAL_WHITE);
    spr_push();
    if (f == 1) spr_rotate(-120);
    spr_ellipse(0, -9, 7, 9, &shell, SH_SPEC);
    spr_ellipse(-3, -12, 2, 2, &shell, SH_FLAT | SH_NOCONTOUR);
    spr_px(-3, -12, PP(PAINT_BUBBLEGUM, 1)); spr_px(-2, -12, PP(PAINT_BUBBLEGUM, 1));
    spr_px(-3, -11, PP(PAINT_BUBBLEGUM, 0)); spr_px(-2, -11, PP(PAINT_BUBBLEGUM, 1));
    spr_px(3, -7, PP(PAINT_MINT, 1)); spr_px(4, -7, PP(PAINT_MINT, 1)); spr_px(3, -6, PP(PAINT_MINT, 0));
    spr_px(1, -15, PP(PAINT_LAVENDER, 1)); spr_px(2, -15, PP(PAINT_LAVENDER, 0));
    spr_px(-1, -4, PP(PAINT_LEMON, 1)); spr_px(0, -4, PP(PAINT_LEMON, 0));
    if (f == 1) { spr_line(2, -13, 4, -11, RP(RAMP_PEACH, 3)); spr_line(4, -11, 3, -9, RP(RAMP_PEACH, 3)); }
    spr_pop();
    finish();
}

static void pet(int f)
{
    ArtMat fur = M5(RP(RAMP_PASTEL_PINK, 4), RP(RAMP_PASTEL_PINK, 5), RP(RAMP_PASTEL_PINK, 6),
                    RP(RAMP_PASTEL_PINK, 7), PAL_WHITE);
    ArtMat belly = MR(RAMP_PEACH, 5, 7);
    static const int hop[4] = {0, 0, -3, 0};
    int y = hop[f], sq = (f == 1) ? 1 : 0;
    spr_push();
    spr_translate(0, y);
    /* ears */
    spr_tri(-8, -14 + sq, -4, -16 + sq, -7, -20 + sq + (f == 3), &fur, 0);
    spr_tri(8, -14 + sq, 4, -16 + sq, 7, -20 + sq - (f == 3), &fur, 0);
    /* fluffy body */
    spr_fluffy(0, -8 + sq, 9 + sq, 8 - sq, 14, Q12 * 2 / 10, f * 400, 0, 4096, &fur, 0);
    spr_ellipse(0, -5 + sq, 5, 3, &belly, 0);
    /* paws */
    spr_ellipse(-4, -1, 2, 1, &fur, 0);
    spr_ellipse(4, -1, 2, 1, &fur, 0);
    /* face */
    if (f == 1) {
        part_eye_shut(-4, -9 + sq, 2, PP(PAINT_JET, 1));
        part_eye_shut(4, -9 + sq, 2, PP(PAINT_JET, 1));
    } else if (f == 3) {
        part_eye_shut(-4, -10, 2, PP(PAINT_JET, 1));
        part_eye_shut(4, -10, 2, PP(PAINT_JET, 1));
    } else {
        part_eye_cute(-4, -9, 2, 3, PAINT_GRAPE, 0, 0);
        part_eye_cute(4, -9, 2, 3, PAINT_GRAPE, 0, 0);
    }
    if (f == 3) {
        /* open happy smile */
        spr_px(-2, -6, PP(PAINT_JET, 1)); spr_px(2, -6, PP(PAINT_JET, 1));
        spr_px(-1, -6, RP(RAMP_BLOOD, 2)); spr_px(0, -6, RP(RAMP_BLOOD, 2)); spr_px(1, -6, RP(RAMP_BLOOD, 2));
        spr_px(-1, -5, PP(PAINT_ROSE, 1)); spr_px(0, -5, PP(PAINT_ROSE, 2)); spr_px(1, -5, PP(PAINT_ROSE, 1));
    } else {
        spr_px(-1, -6 + sq, PP(PAINT_JET, 1)); spr_px(1, -6 + sq, PP(PAINT_JET, 1));
        spr_px(0, -5 + sq, PP(PAINT_JET, 1));
    }
    part_blush(-7, -6 + sq, 1);
    part_blush(7, -6 + sq, 1);
    spr_pop();
    finish();
    if (f == 3) spr_twinkle(9, -18, 2, PAL_WHITE, PP(PAINT_BUBBLEGUM, 2));
}

const ArtSprDef art_spr_items[] = {
    {SPR_HEALTH_SMALL, "health_small", 2, ANCHOR_GROUND, SPRF_LOOP, health_small},
    {SPR_HEALTH_BIG, "health_big", 2, ANCHOR_GROUND, SPRF_LOOP, health_big},
    {SPR_ARMOR, "armor", 2, ANCHOR_GROUND, SPRF_LOOP, armor},
    {SPR_AMMO_BULLETS, "ammo_bullets", 1, ANCHOR_GROUND, 0, ammo_bullets},
    {SPR_AMMO_SHELLS, "ammo_shells", 1, ANCHOR_GROUND, 0, ammo_shells},
    {SPR_AMMO_CELLS, "ammo_cells", 2, ANCHOR_GROUND, SPRF_LOOP | SPRF_FULLBRIGHT, ammo_cells},
    {SPR_AMMO_ROCKETS, "ammo_rockets", 1, ANCHOR_GROUND, 0, ammo_rockets},
    {SPR_KEY_RED, "key_red", 2, ANCHOR_GROUND, SPRF_LOOP, key_red},
    {SPR_KEY_BLUE, "key_blue", 2, ANCHOR_GROUND, SPRF_LOOP, key_blue},
    {SPR_SCRAP, "scrap", 1, ANCHOR_GROUND, 0, scrap},
    {SPR_SOUL_ORB, "soul_orb", 4, ANCHOR_GROUND, SPRF_LOOP | SPRF_FULLBRIGHT | SPRF_FLOATS, soul_orb},
    {SPR_CANDY, "candy", 2, ANCHOR_GROUND, SPRF_LOOP, candy},
    {SPR_TREE, "tree", 1, ANCHOR_GROUND, 0, tree},
    {SPR_FLOWER_BED, "flower_bed", 1, ANCHOR_GROUND, 0, flower_bed},
    {SPR_LAMP, "lamp", 1, ANCHOR_GROUND, 0, lamp},
    {SPR_BENCH, "bench", 1, ANCHOR_GROUND, 0, bench},
    {SPR_FOUNTAIN, "fountain", 2, ANCHOR_GROUND, SPRF_LOOP, fountain},
    {SPR_BARREL, "barrel", 2, ANCHOR_GROUND, SPRF_LOOP, barrel},
    {SPR_SKULL_PILE, "skull_pile", 1, ANCHOR_GROUND, 0, skull_pile},
    {SPR_CORPSE_CUTE, "corpse_cute", 1, ANCHOR_GROUND, 0, corpse_cute},
    {SPR_CORPSE_BRUTAL, "corpse_brutal", 1, ANCHOR_GROUND, 0, corpse_brutal},
    {SPR_TORCH, "torch", 4, ANCHOR_GROUND, SPRF_LOOP | SPRF_FULLBRIGHT, torch},
    {SPR_SIGN, "sign", 1, ANCHOR_GROUND, 0, sign},
    {SPR_MAILBOX, "mailbox", 1, ANCHOR_GROUND, 0, mailbox},
    {SPR_VAPE_PORTAL, "vape_portal", 4, ANCHOR_GROUND, SPRF_LOOP | SPRF_FULLBRIGHT, vape_portal},
    {SPR_EGG, "egg", 2, ANCHOR_GROUND, SPRF_LOOP, egg},
    {SPR_PET, "pet", 4, ANCHOR_GROUND, SPRF_LOOP, pet},
};
const int art_spr_items_n = (int)(sizeof art_spr_items / sizeof art_spr_items[0]);
