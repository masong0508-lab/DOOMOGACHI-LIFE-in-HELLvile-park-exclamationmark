/* portrait_gun.c - the weapon a third-person fighter holds, shaped from
 * its WFRAME_* only (owner: portrait). Weapon space: origin at the grip,
 * +x forward toward the muzzle, +y down, units of 1/2 pixel. */
#include "portrait_int.h"
#include "doomo/weld.h"

static int GX, GY, GD, GL;      /* grip (Q4), direction, length scale Q8 */
static int MX, MF;              /* last muzzle x and frame              */

static int gx(int x) { return GX + GD * x * 8 * GL / 256; }
static int gy(int y) { return GY + y * 8; }

static void gR(const PMat *m, int x0, int y0, int x1, int y1)
{
    int a = gx(x0), b = gx(x1);
    pc_part(m, SH_BALL, 0);
    if (a > b) { int t = a; a = b; b = t; }
    pc_frame((a + b) / 2, (gy(y0) + gy(y1)) / 2, (b - a) / 2 + 8, (gy(y1) - gy(y0)) / 2 + 8);
    pc_rect(a, gy(y0), b + 8, gy(y1));
}

static void gE(const PMat *m, int x, int y, int rx, int ry)
{
    pc_part(m, SH_BALL, 0);
    pc_ellipse(gx(x), gy(y), DOOMO_MAX(rx * 8 * GL / 256, 8), ry * 8);
}

static void gC(const PMat *m, int x0, int y0, int x1, int y1, int r)
{
    pc_part(m, SH_BALL, 0);
    pc_capsule(gx(x0), gy(y0), gx(x1), gy(y1), r * 8, r * 8);
}

static void gT(const PMat *m, int x0, int y0, int x1, int y1, int x2, int y2)
{
    pc_part(m, SH_BALL, 0);
    pc_tri(gx(x0), gy(y0), gx(x1), gy(y1), gx(x2), gy(y2));
}

static void gPx(int x, int y, uint8_t c) { pc_px(gx(x) >> 4, gy(y) >> 4, c); }

static int is_cute(int f) { return f >= WFRAME_BUBBLE_WAND; }

/* returns muzzle x (weapon units) */
static int weapon_shape(int f, int anim)
{
    PMat met, dk, wood, c1, c2, wh, glow, red, gold;
    int k;
    pm_ramp(&met, RAMP_STEEL, 4);
    pm_ramp(&dk, RAMP_STEEL, 2);
    pm_ramp(&wood, RAMP_WOOD, 4);
    pm_paint(&c1, PAINT_BUBBLEGUM);
    pm_paint(&c2, PAINT_MINT);
    pm_paint(&wh, PAINT_WHITE);
    pm_paint(&glow, PAINT_CYAN);
    pm_paint(&red, PAINT_CHERRY);
    pm_paint(&gold, PAINT_LEMON);
    switch (f) {
    case WFRAME_PISTOL:
        gR(&dk, -1, 0, 3, 6);
        gR(&met, -1, -4, 12, 0);
        return 12;
    case WFRAME_REVOLVER:
        gR(&wood, -2, 0, 2, 7);
        gR(&met, 4, -4, 16, -1);
        gE(&dk, 4, -2, 3, 3);
        return 16;
    case WFRAME_DUAL_PISTOLS:
        gR(&dk, -1, 0, 3, 6);
        gR(&met, -1, -4, 12, 0);
        gR(&dk, 2, 6, 6, 12);
        gR(&met, 2, 2, 15, 6);
        return 12;
    case WFRAME_MACHINE_PISTOL:
        gR(&dk, -1, 0, 3, 12);
        gR(&met, -2, -4, 14, 0);
        return 14;
    case WFRAME_SMG:
        gR(&dk, -8, -3, -2, -1);
        gR(&met, -2, -5, 14, 1);
        gR(&dk, 14, -3, 18, -1);
        gR(&dk, 3, 1, 6, 10);
        return 18;
    case WFRAME_ASSAULT_RIFLE:
        gR(&dk, -12, -4, -2, 2);
        gR(&met, -2, -6, 18, 1);
        gR(&dk, 18, -4, 26, -2);
        gT(&dk, 6, 1, 11, 1, 10, 9);
        gR(&dk, 2, -9, 10, -6);
        return 26;
    case WFRAME_BATTLE_RIFLE:
        gR(&wood, -14, -4, -2, 2);
        gR(&met, -2, -6, 20, 0);
        gR(&wood, 8, -5, 18, 1);
        gR(&dk, 20, -4, 30, -2);
        gR(&dk, 4, 0, 8, 8);
        return 30;
    case WFRAME_BULLPUP:
        gR(&met, -10, -6, 16, 2);
        gR(&dk, -8, 2, -3, 9);
        gR(&dk, 16, -4, 24, -2);
        gR(&dk, 0, -9, 8, -6);
        return 24;
    case WFRAME_LMG:
        gR(&dk, -12, -4, -2, 2);
        gR(&met, -2, -7, 20, 1);
        gR(&dk, 20, -5, 32, -2);
        gR(&met, 4, 1, 12, 9);
        gPx(28, 0, PAL_GRAY_D); gPx(29, 2, PAL_GRAY_D); gPx(26, 2, PAL_GRAY_D);
        return 32;
    case WFRAME_MINIGUN:
        gR(&dk, -6, -9, 6, 3);
        for (k = 0; k < 3; k++) gR(&met, 6, -8 + k * 3, 28, -6 + k * 3);
        gE(&dk, 26, -4, 3, 6);
        gC(&dk, -2, 3, 2, 8, 1);
        return 30;
    case WFRAME_DMR:
        gR(&wood, -12, -4, -2, 2);
        gR(&met, -2, -5, 22, 0);
        gR(&dk, 22, -4, 32, -2);
        gR(&dk, 2, -9, 12, -6);
        gPx(12, -8, PAL_PAINT(PAINT_SKY, 2));
        return 32;
    case WFRAME_SNIPER:
        gR(&dk, -14, -4, -2, 3);
        gR(&met, -2, -5, 24, 0);
        gR(&dk, 24, -4, 40, -2);
        gR(&dk, 0, -11, 14, -6);
        gE(&glow, 14, -8, 1, 2);
        return 40;
    case WFRAME_RAILGUN:
        gR(&dk, -10, -5, 0, 2);
        gR(&met, 0, -7, 28, 1);
        for (k = 0; k < 4; k++) gR(&glow, 4 + k * 6, -8, 6 + k * 6, 2);
        gR(&dk, 28, -4, 34, -2);
        return 34;
    case WFRAME_CROSSBOW:
        gR(&wood, -10, -3, 12, 1);
        gC(&dk, 12, -12, 12, 8, 1);
        pc_lineq(gx(12), gy(-12), gx(4), gy(-1), PAL_GRAY_L);
        pc_lineq(gx(12), gy(8), gx(4), gy(-1), PAL_GRAY_L);
        return 14;
    case WFRAME_BOW:
        gC(&wood, 6, -16, 10, -2, 1);
        gC(&wood, 10, -2, 6, 12, 1);
        pc_lineq(gx(6), gy(-16), gx(6), gy(12), PAL_GRAY_L);
        gC(&met, 0, -2, 16, -2, 0);
        return 16;
    case WFRAME_PUMP_SHOTGUN:
        gR(&wood, -12, -4, -2, 2);
        gR(&met, -2, -6, 24, -2);
        gR(&dk, 2, -2, 22, 0);
        gR(&wood, 8, -3, 16, 2);
        return 24;
    case WFRAME_SUPER_SHOTGUN:
        gR(&wood, -10, -4, 0, 3);
        gR(&met, 0, -7, 18, -4);
        gR(&met, 0, -4, 18, -1);
        gR(&wood, 4, -2, 12, 2);
        return 18;
    case WFRAME_AUTO_SHOTGUN:
        gR(&dk, -12, -4, -2, 2);
        gR(&met, -2, -7, 22, -1);
        gE(&dk, 6, 3, 5, 5);
        gR(&dk, 22, -6, 26, -2);
        return 26;
    case WFRAME_LEVER_SHOTGUN:
        gR(&wood, -12, -4, -2, 2);
        gR(&met, -2, -6, 24, -2);
        gR(&gold, -2, -2, 2, 0);
        pc_lineq(gx(-2), gy(2), gx(2), gy(6), PAL_PAINT(PAINT_HONEY, 0));
        pc_lineq(gx(2), gy(6), gx(4), gy(1), PAL_PAINT(PAINT_HONEY, 0));
        return 24;
    case WFRAME_GRENADE_LAUNCHER:
        gR(&dk, -10, -4, -2, 2);
        gE(&met, 4, -2, 5, 5);
        gR(&dk, 6, -6, 22, 0);
        gE(&dk, 22, -3, 1, 3);
        return 22;
    case WFRAME_ROCKET_LAUNCHER: {
        PMat ol;
        pm_paint(&ol, PAINT_FOREST);
        gR(&ol, -14, -10, 18, -2);
        gE(&ol, 18, -6, 2, 4);
        gT(&red, 18, -9, 18, -3, 24, -6);
        gR(&dk, 0, -2, 4, 6);
        gR(&dk, 4, -14, 8, -10);
        return 24; }
    case WFRAME_FLAMETHROWER:
        gE(&red, -4, 2, 5, 5);
        gR(&met, -2, -5, 18, -1);
        gR(&dk, 18, -6, 22, 0);
        gPx(23, -3, (anim & 2) ? PAL_RAMP(RAMP_LAVA, 6) : PAL_RAMP(RAMP_LAVA, 4));
        return 22;
    case WFRAME_PLASMA_RIFLE:
        gR(&met, -8, -7, 18, 2);
        gE(&met, 18, -2, 3, 4);
        for (k = 0; k < 3; k++) gR(&glow, -4 + k * 6, -5, -1 + k * 6, -1);
        gE(&glow, 21, -2, 1, 2);
        return 22;
    case WFRAME_NAILGUN:
        gR(&red, -6, -7, 14, 2);
        gR(&dk, 14, -4, 20, -1);
        for (k = 0; k < 4; k++) gPx(-2 + k * 4, 3, PAL_GRAY_L);
        return 20;
    case WFRAME_BFG: {
        PMat gr;
        pm_paint(&gr, PAINT_LIME);
        gE(&met, 8, -4, 14, 9);
        gE(&gr, 10, -4, 5, 5);
        gR(&dk, 20, -7, 30, -1);
        gPx(10, -5, PAL_WHITE);
        return 30; }
    case WFRAME_CHAINSAW:
        gR(&red, -6, -8, 6, 2);
        gR(&met, 6, -6, 30, -1);
        for (k = 0; k < 6; k++) gPx(8 + k * 4 + ((anim >> 1) & 1), (k & 1) ? -7 : 0, PAL_GRAY_D);
        gC(&dk, -6, -10, 2, -10, 1);
        return 30;
    case WFRAME_KATANA:
        gR(&dk, -6, -2, 2, 2);
        gE(&gold, 2, 0, 1, 3);
        gT(&wh, 3, -2, 3, 2, 34, -10);
        return 34;
    case WFRAME_SLEDGEHAMMER:
        gC(&wood, -4, 4, 20, -6, 1);
        gR(&dk, 16, -16, 26, 2);
        return 26;
    case WFRAME_FISTS:
        gE(&red, 4, 0, 5, 5);
        gE(&red, 10, 6, 5, 5);
        gPx(3, -2, PAL_WHITE);
        return 12;
    case WFRAME_BUBBLE_WAND:
        gC(&c1, -2, 2, 14, -8, 1);
        pc_part(&glow, SH_SOFT, 0);
        {
            int cx = gx(18), cy = gy(-11), a;
            for (a = 0; a < 256; a += 16)
                pc_px((cx + p_cos(a) * 40 / 256) >> 4, (cy + p_sin(a) * 40 / 256) >> 4, PAL_PAINT(PAINT_CYAN, 1));
        }
        gPx(24, -18 - ((anim >> 1) & 3), PAL_PAINT(PAINT_SKY, 2));
        return 20;
    case WFRAME_PLUSH_LAUNCHER: {
        PMat br;
        pm_paint(&br, PAINT_CARAMEL);
        gR(&c2, -10, -8, 16, 0);
        gE(&c2, 16, -4, 2, 5);
        gE(&br, 20, -4, 4, 4);
        gE(&br, 18, -9, 2, 2);
        gE(&br, 23, -9, 2, 2);
        gPx(21, -4, PAL_PAINT(PAINT_JET, 1));
        return 24; }
    case WFRAME_CONFETTI_CANNON:
        gT(&c1, -6, -4, 18, -12, 18, 4);
        gR(&gold, -8, -6, -2, 0);
        gPx(22, -6, PAL_PAINT(PAINT_SKY, 2)); gPx(24, -2, PAL_PAINT(PAINT_LEMON, 2));
        gPx(23, -10, PAL_PAINT(PAINT_MINT, 2));
        return 20;
    case WFRAME_LOLLIPOP_HAMMER:
        gC(&wh, -4, 4, 18, -6, 1);
        gE(&c1, 22, -9, 7, 7);
        pc_part(&wh, SH_FLAT, 0);
        pc_lineq(gx(18), gy(-12), gx(24), gy(-6), PAL_WHITE);
        pc_lineq(gx(20), gy(-14), gx(26), gy(-8), PAL_WHITE);
        return 28;
    case WFRAME_HEART_BOW:
        gC(&c1, 6, -14, 10, -2, 1);
        gC(&c1, 10, -2, 6, 10, 1);
        pc_lineq(gx(6), gy(-14), gx(6), gy(10), PAL_WHITE);
        gC(&wh, 0, -2, 14, -2, 0);
        ph_heart(gx(16) >> 4, gy(-2) >> 4, 1, PAL_PAINT(PAINT_CHERRY, 1), PAL_WHITE);
        return 16;
    case WFRAME_SQUEAKY_MALLET:
        gC(&gold, -4, 4, 16, -6, 1);
        gR(&red, 12, -16, 24, 0);
        gR(&gold, 12, -16, 14, 0);
        gR(&gold, 22, -16, 24, 0);
        return 24;
    case WFRAME_GLITTER_SPRAYER: {
        PMat lv;
        pm_paint(&lv, PAINT_LAVENDER);
        gR(&lv, -4, -6, 8, 6);
        gR(&wh, 6, -10, 12, -6);
        gR(&wh, 12, -9, 16, -7);
        for (k = 0; k < 3; k++) gPx(18 + k * 3, -8 - k + ((anim + k) & 1), (k & 1) ? PAL_UI_GOLD : PAL_WHITE);
        return 16; }
    case WFRAME_WATER_PISTOL:
        gR(&c2, -2, -6, 16, 0);
        gR(&c2, -1, 0, 3, 7);
        gE(&glow, 5, -8, 4, 3);
        gR(&gold, 16, -4, 20, -2);
        return 20;
    case WFRAME_STICKER_GUN:
        gR(&c1, -2, -6, 14, 0);
        gR(&c1, -1, 0, 3, 7);
        gE(&gold, 6, -9, 3, 3);
        gPx(6, -9, PAL_WHITE);
        return 14;
    case WFRAME_YARN_CROSSBOW: {
        PMat yn;
        pm_paint(&yn, PAINT_ROSE);
        gR(&c2, -10, -3, 12, 1);
        gC(&c1, 12, -12, 12, 8, 1);
        gE(&yn, 0, -6, 4, 4);
        pc_lineq(gx(-2), gy(-8), gx(2), gy(-4), PAL_RAMP(RAMP_PASTEL_PINK, 2));
        return 14; }
    default:
        gR(&dk, -1, 0, 3, 6);
        gR(&met, -1, -4, 12, 0);
        return 12;
    }
}

static void muzzle_flash(int f, int mx, int anim)
{
    int x = gx(mx) >> 4, y = gy(-3) >> 4, big = (anim & 1) ? 2 : 1;
    uint8_t a = PAL_RAMP(RAMP_LAVA, 7), b = PAL_RAMP(RAMP_LAVA, 5), cc = PAL_RAMP(RAMP_LAVA, 3);
    if (f == WFRAME_KATANA || f == WFRAME_SLEDGEHAMMER || f == WFRAME_FISTS || f == WFRAME_CHAINSAW ||
        f == WFRAME_LOLLIPOP_HAMMER || f == WFRAME_SQUEAKY_MALLET) {
        /* melee swoosh */
        int k;
        for (k = 0; k < 6; k++) pc_fx(x + GD * (k / 2), y - 4 + k, PAL_WHITE);
        return;
    }
    if (is_cute(f)) { a = PAL_WHITE; b = PAL_PAINT(PAINT_BUBBLEGUM, 2); cc = PAL_PAINT(PAINT_SKY, 2); }
    pc_fx(x + GD, y, a);
    pc_fx(x + GD * 2, y, a);
    pc_fx(x + GD, y - 1, b); pc_fx(x + GD, y + 1, b);
    pc_fx(x + GD * 3, y, b);
    if (big > 1) {
        pc_fx(x + GD * 2, y - 2, cc); pc_fx(x + GD * 2, y + 2, cc);
        pc_fx(x + GD * 4, y, cc);
        pc_fx(x + GD * 3, y - 1, b); pc_fx(x + GD * 3, y + 1, b);
    }
}

void pg_draw_weapon(PCtx *c, int wframe, int view, int anim, int hx, int hy, int front)
{
    int mx;
    (void)c;
    if (wframe < 0 || wframe >= WFRAME_COUNT) wframe = WFRAME_PISTOL;
    GX = hx; GY = hy; GD = -1; GL = 256;
    switch (view) {
    case 0: GD = 1; GL = 110; GY -= PX(2); break;     /* aiming away */
    case 1: GL = 150; GY -= PX(1); break;
    case 2: GL = 256; break;
    case 3: GL = 170; break;
    default: GL = 80; break;                          /* pointing at us */
    }
    if (anim == 4) GX -= GD * 8;                      /* recoil */
    (void)front;
    mx = weapon_shape(wframe, anim);
    MX = mx; MF = wframe;
    if (view == 4) {
        /* muzzle hole facing the camera */
        int x = gx(mx) >> 4, y = gy(-3) >> 4;
        if (wframe != WFRAME_KATANA && wframe != WFRAME_FISTS && wframe < WFRAME_CHAINSAW) {
            pc_px(x, y, PAL_PAINT(PAINT_JET, 0));
        }
    }
}

void pg_draw_flash(int anim)
{
    muzzle_flash(MF, MX, anim);
}
