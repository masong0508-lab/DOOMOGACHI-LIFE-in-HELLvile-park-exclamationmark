/* spr_demons.c - the eleven demons of the invasion.
 *
 * Menacing-but-cute-ish: chunky chibi proportions (big heads, stubby
 * limbs), glossy soft shading, coloured outlines, glowing eyes and fangs.
 * Every demon has DFRAME_COUNT frames (sprites.h):
 *   0-1 walk (alternating legs / wing flaps / wisps, with a 1px bob),
 *   2 attack, 3 pain (recoil, squeezed eyes),
 *   4-7 death: shock -> falling -> hitting the ground -> lying still with
 *   X eyes. The gore effect sprites (blood/gibs or confetti/hearts/stuffing)
 *   are layered on top by the game, so these frames stay gore-neutral.
 * All drawing is relative to the feet (0,0), y up = negative.
 */
#include "art.h"

#define M5(a, b, c, d, e) art_mat5((uint8_t)(a), (uint8_t)(b), (uint8_t)(c), (uint8_t)(d), (uint8_t)(e))
#define M4(a, b, c, d) art_mat4((uint8_t)(a), (uint8_t)(b), (uint8_t)(c), (uint8_t)(d))
#define M3(a, b, c) art_mat3((uint8_t)(a), (uint8_t)(b), (uint8_t)(c))
#define MR(r, lo, hi) art_mat_ramp((r), (lo), (hi))

typedef struct {
    int walk;       /* -1 / +1 leg phase, 0 when not walking   */
    int bob;        /* body offset (px, + = down)               */
    int attack;
    int pain;
    int dstage;     /* 0 alive, 1..4 death frames               */
} Pose;

static Pose pose_of(int f)
{
    Pose p;
    memset(&p, 0, sizeof p);
    if (f == 0) { p.walk = -1; p.bob = 0; }
    else if (f == 1) { p.walk = 1; p.bob = 1; }
    else if (f == 2) p.attack = 1;
    else if (f == 3) p.pain = 1;
    else p.dstage = f - 3;
    return p;
}

/* death transform shared by the walkers. Seen from the front, falling
 * over backwards foreshortens the body towards its feet, so the frames
 * shrink instead of sprawling sideways (cheaper and it reads well):
 * 1 shock (jolt up), 2 knees buckle, 3 falling back, 4 lying flat. */
static void death_xform(int stage, int H, int W)
{
    (void)H; (void)W;
    switch (stage) {
    case 1: spr_translate(0, -2); spr_scale(Q12 * 94 / 100, Q12 * 108 / 100); break;
    case 2: spr_rotate(90); spr_scale(Q12 * 104 / 100, Q12 * 80 / 100); break;
    case 3: spr_rotate(-60); spr_scale(Q12 * 110 / 100, Q12 * 56 / 100); break;
    case 4: spr_scale(Q12 * 114 / 100, Q12 * 38 / 100); break;
    default: break;
    }
}

static void finish(void)
{
    spr_contours();
    spr_outline();
}

/* glowing slit eye with a highlight; dead -> X, pain -> squeezed */
static void eye_demon(int x, int y, int r, const ArtMat *glow, int pain, int dead, int left)
{
    if (dead) { part_eye_x(x, y, r > 2 ? 2 : 1, PAL_PAINT(PAINT_JET, 1)); return; }
    if (pain) { part_eye_squeeze(x, y, left ? -(r > 2 ? 2 : 1) : (r > 2 ? 2 : 1), PAL_PAINT(PAINT_JET, 1)); return; }
    part_eye_glow(x, y, r, r, glow, 1, PAL_PAINT(PAINT_JET, 0));
}

static void fangs_row(int x0, int x1, int y, uint8_t c)
{
    int x;
    for (x = x0; x <= x1; x += 3) part_fang(x, y, 2, 2, 1, c);
}

/* ======================================================================
 * IMPLING - little crimson imp: ivory horns, bat wings, spade tail
 * ==================================================================== */
static void impling(int f)
{
    Pose p = pose_of(f);
    ArtMat skin = M5(RP(RAMP_BLOOD, 3), RP(RAMP_BLOOD, 4), PP(PAINT_CHERRY, 1), RP(RAMP_BLOOD, 6), RP(RAMP_BLOOD, 7));
    ArtMat dark = M4(RP(RAMP_BLOOD, 2), RP(RAMP_BLOOD, 3), RP(RAMP_BLOOD, 4), PP(PAINT_CHERRY, 1));
    ArtMat belly = MR(RAMP_PEACH, 3, 7);
    ArtMat horn = MR(RAMP_BONE, 3, 7);
    ArtMat wing = M4(RP(RAMP_VAPE, 1), RP(RAMP_BLOOD, 2), RP(RAMP_BLOOD, 3), RP(RAMP_FLESH, 3));
    ArtMat fire = M5(RP(RAMP_LAVA, 3), RP(RAMP_LAVA, 5), RP(RAMP_LAVA, 6), RP(RAMP_LAVA, 7), PAL_WHITE);
    ArtMat eyes = M4(RP(RAMP_LAVA, 5), RP(RAMP_LAVA, 6), RP(RAMP_CANDY, 6), RP(RAMP_LAVA, 7));
    int dead = p.dstage >= 2, b = p.bob, flap = (f == 1) ? 2 : 0;
    spr_push();
    death_xform(p.dstage, 34, 26);
    if (p.pain) { spr_translate(1, 0); spr_rotate(-120); }
    /* tail (behind) */
    spr_bezier(3, -8 + b, 14, -6 + b, 13, -18 + b + (f & 1), 2, 1, &dark, 0);
    spr_tri(10, -18 + b + (f & 1), 16, -19 + b + (f & 1), 13, -23 + b + (f & 1), &dark, 0);
    /* wings (behind) */
    {
        int w[10];
        int k;
        for (k = -1; k <= 1; k += 2) {
            w[0] = k * 5; w[1] = -19 + b;
            w[2] = k * 17; w[3] = -27 + b - flap;
            w[4] = k * 16; w[5] = -21 + b - flap / 2;
            w[6] = k * 18; w[7] = -15 + b;
            w[8] = k * 6; w[9] = -14 + b;
            spr_poly(w, 5, &wing, 0);
            spr_line(k * 6, -18 + b, k * 16, -26 + b - flap, RP(RAMP_BLOOD, 4));
        }
    }
    /* legs */
    {
        int lf = p.walk * 2;
        spr_capsule(-4, -9 + b, 3, -5 - (lf > 0), -2 - (lf > 0 ? 1 : 0), 2, &dark, 0);
        spr_capsule(4, -9 + b, 3, 5 + (lf < 0), -2 - (lf < 0 ? 1 : 0), 2, &dark, 0);
        spr_ellipse(-6, -1 - (lf > 0), 3, 2, &dark, 0);
        spr_ellipse(6, -1 - (lf < 0), 3, 2, &dark, 0);
    }
    /* body + belly */
    spr_ellipse(0, -14 + b, 8, 7, &skin, 0);
    spr_ellipse(0, -12 + b, 5, 4, &belly, 0);
    /* arms */
    if (p.attack) {
        spr_capsule(-7, -17, 2, -12, -12, 2, &skin, 0);
        part_paw(-12, -11, 2, &skin, 1, RP(RAMP_BONE, 6));
        spr_capsule(7, -18, 2, 11, -27, 2, &skin, 0);
        part_paw(11, -28, 2, &skin, 0, 0);
        part_glow(11, -34, 7, &fire);
        spr_ellipse(11, -34, 4, 4, &fire, SH_GLOW | SH_NOCONTOUR);
        spr_px(10, -36, PAL_WHITE);
    } else {
        int sw = p.walk;
        spr_capsule(-7, -17 + b, 2, -10, -11 + b - sw, 2, &skin, 0);
        spr_capsule(7, -17 + b, 2, 10, -11 + b + sw, 2, &skin, 0);
        part_paw(-10, -10 + b - sw, 2, &skin, 1, RP(RAMP_BONE, 6));
        part_paw(10, -10 + b + sw, 2, &skin, 1, RP(RAMP_BONE, 6));
    }
    /* head: ears, horns, face */
    spr_tri(-8, -29 + b, -15, -31 + b, -8, -24 + b, &skin, 0);
    spr_tri(8, -29 + b, 15, -31 + b, 8, -24 + b, &skin, 0);
    part_horn(-5, -32 + b, -4, -8, 3, 2, &horn);
    part_horn(5, -32 + b, 4, -8, -3, 2, &horn);
    spr_ellipse(0, -26 + b, 9, 8, &skin, 0);
    /* brows + eyes */
    eye_demon(-4, -27 + b, 2, &eyes, p.pain, dead, 1);
    eye_demon(4, -27 + b, 2, &eyes, p.pain, dead, 0);
    if (!dead && !p.pain) {
        spr_line(-7, -31 + b, -2, -29 + b, RP(RAMP_BLOOD, 1));
        spr_line(7, -31 + b, 2, -29 + b, RP(RAMP_BLOOD, 1));
    }
    /* mouth */
    if (p.attack || p.pain || p.dstage == 1) {
        ArtMat mo = M3(RP(RAMP_BLOOD, 0), RP(RAMP_BLOOD, 1), RP(RAMP_BLOOD, 1));
        spr_ellipse(0, -21 + b, 4, 3, &mo, SH_FLAT);
        spr_ellipse(0, -19 + b, 2, 1, &belly, SH_FLAT | SH_NOCONTOUR);
        part_fang(-3, -23 + b, 2, 2, 1, PAL_WHITE);
        part_fang(2, -23 + b, 2, 2, 1, PAL_WHITE);
    } else if (dead) {
        spr_line(-3, -21 + b, 3, -21 + b, RP(RAMP_BLOOD, 1));
        spr_px(2, -20 + b, PP(PAINT_ROSE, 1));
        spr_px(2, -19 + b, PP(PAINT_ROSE, 2));
    } else {
        spr_line(-4, -22 + b, -2, -21 + b, RP(RAMP_BLOOD, 1));
        spr_line(-2, -21 + b, 2, -21 + b, RP(RAMP_BLOOD, 1));
        spr_line(2, -21 + b, 4, -22 + b, RP(RAMP_BLOOD, 1));
        part_fang(-2, -20 + b, 1, 2, 1, PAL_WHITE);
        part_fang(2, -20 + b, 1, 2, 1, PAL_WHITE);
    }
    part_blush(-6, -23 + b, 1);
    part_blush(6, -23 + b, 1);
    spr_pop();
    finish();
}

/* ======================================================================
 * PUFF DEMON - a floating bubblegum caco: one big eye, toothy grin,
 * ivory horns, fluttering little wings
 * ==================================================================== */
static void puff_demon(int f)
{
    Pose p = pose_of(f);
    ArtMat body = M5(RP(RAMP_PASTEL_PINK, 2), RP(RAMP_PASTEL_PINK, 3), RP(RAMP_PASTEL_PINK, 4),
                     RP(RAMP_PASTEL_PINK, 5), RP(RAMP_PASTEL_PINK, 6));
    ArtMat horn = MR(RAMP_BONE, 3, 7);
    ArtMat wing = M4(RP(RAMP_VAPE, 2), RP(RAMP_VAPE, 3), RP(RAMP_VAPE, 4), RP(RAMP_VAPE, 5));
    ArtMat mouth = M3(RP(RAMP_BLOOD, 0), RP(RAMP_BLOOD, 1), RP(RAMP_BLOOD, 2));
    ArtMat tongue = MR(RAMP_FLESH, 4, 7);
    ArtMat fire = M5(RP(RAMP_VAPE, 4), PP(PAINT_MAGENTA, 1), PP(PAINT_MAGENTA, 2), RP(RAMP_VAPE, 7), PAL_WHITE);
    int cy = -17 - (f == 1 ? 1 : 0), dead = p.dstage >= 2;
    int k;
    spr_push();
    if (p.dstage == 1) spr_translate(0, -3);
    spr_scale(Q12 * 94 / 100, Q12 * 94 / 100);
    if (p.dstage == 2) spr_scale(Q12 * 110 / 100, Q12 * 88 / 100);
    if (p.dstage == 3) spr_scale(Q12 * 120 / 100, Q12 * 64 / 100);
    if (p.dstage == 4) spr_scale(Q12 * 130 / 100, Q12 * 42 / 100);
    if (p.pain) spr_rotate(140);
    /* wings */
    if (p.dstage < 3)
        for (k = -1; k <= 1; k += 2) {
            int up = (f == 1) ? -4 : 2;
            int w[8];
            w[0] = k * 15; w[1] = cy - 4;
            w[2] = k * 24; w[3] = cy - 10 + up;
            w[4] = k * 22; w[5] = cy - 1 + up / 2;
            w[6] = k * 16; w[7] = cy + 3;
            spr_poly(w, 4, &wing, 0);
        }
    /* horns */
    part_horn(-8, cy - 14, -5, -9, 3, 3, &horn);
    part_horn(8, cy - 14, 5, -9, -3, 3, &horn);
    /* body */
    spr_ellipse(0, cy, 17, 16, &body, SH_SPEC);
    /* little spikes on top */
    for (k = -1; k <= 1; k++) spr_tri(k * 5 - 2, cy - 15 + art_abs(k), k * 5 + 2, cy - 15 + art_abs(k), k * 5, cy - 19 + art_abs(k), &body, 0);
    /* freckle spots */
    spr_ellipse(-11, cy + 3, 2, 1, &body, SH_FLAT | SH_NOCONTOUR);
    spr_px(-11, cy + 3, RP(RAMP_PASTEL_PINK, 2)); spr_px(-10, cy + 3, RP(RAMP_PASTEL_PINK, 2));
    spr_px(12, cy - 4, RP(RAMP_PASTEL_PINK, 3)); spr_px(11, cy - 4, RP(RAMP_PASTEL_PINK, 3));
    spr_px(-9, cy - 9, RP(RAMP_PASTEL_PINK, 3));
    /* the eye */
    if (dead) {
        part_eye_x(0, cy - 5, 3, RP(RAMP_BLOOD, 1));
    } else if (p.pain) {
        spr_line(-5, cy - 6, 5, cy - 4, RP(RAMP_BLOOD, 1));
        spr_line(-5, cy - 4, 5, cy - 6, RP(RAMP_BLOOD, 1));
    } else {
        ArtMat white = M3(PAL_GRAY_L, PAL_WHITE, PAL_WHITE);
        ArtMat iris = M4(PP(PAINT_LIME, 0), PP(PAINT_LIME, 1), PP(PAINT_LIME, 2), PP(PAINT_CHARTREUSE, 2));
        int lid = (f == 2 || p.dstage == 1) ? 0 : 2;          /* half-lidded menace */
        spr_ellipse(0, cy - 5, 7, 6, &white, SH_FLAT);
        spr_ellipse(0, cy - 4, 4, 5, &iris, SH_VGRAD | SH_ONTOP | SH_NOCONTOUR);
        for (k = -4; k <= 5; k++) spr_px(0, cy - 4 + k, PP(PAINT_JET, 1));
        spr_px(-1, cy - 4, PP(PAINT_JET, 1)); spr_px(1, cy - 4, PP(PAINT_JET, 1));
        spr_px(-2, cy - 7, PAL_WHITE); spr_px(-3, cy - 7, PAL_WHITE); spr_px(-2, cy - 6, PAL_WHITE);
        spr_px(2, cy - 1, PAL_WHITE);
        if (lid) {
            ArtMat lidm = M3(RP(RAMP_PASTEL_PINK, 2), RP(RAMP_PASTEL_PINK, 3), RP(RAMP_PASTEL_PINK, 4));
            spr_ellipse(0, cy - 10, 8, 4, &lidm, SH_ONTOP);
        }
    }
    /* mouth */
    if (p.attack || p.dstage == 1) {
        spr_ellipse(0, cy + 8, 11, 6, &mouth, SH_FLAT);
        spr_ellipse(0, cy + 12, 6, 2, &tongue, SH_ONTOP);
        fangs_row(-9, 9, cy + 3, PAL_WHITE);
        for (k = -8; k <= 8; k += 3) part_fang(k, cy + 13, 2, 2, -1, PAL_WHITE);
        if (p.attack) {
            part_glow(0, cy + 8, 6, &fire);
            spr_ellipse(0, cy + 8, 3, 3, &fire, SH_GLOW | SH_NOCONTOUR);
        }
    } else if (dead) {
        spr_line(-8, cy + 8, 8, cy + 8, RP(RAMP_BLOOD, 1));
        spr_ellipse(4, cy + 11, 2, 3, &tongue, 0);
    } else {
        /* toothy crescent grin */
        spr_ellipse(0, cy + 7, 10, 4, &mouth, SH_FLAT);
        spr_ellipse(0, cy + 4, 11, 3, &body, SH_ONTOP | SH_NOCONTOUR);
        fangs_row(-8, 8, cy + 7, PAL_WHITE);
        spr_ellipse(-3, cy + 10, 2, 1, &tongue, SH_ONTOP | SH_NOCONTOUR);
    }
    spr_pop();
    finish();
}

/* ======================================================================
 * HELLHOUND PUP - fluffy charcoal puppy with ember horns, floppy ears,
 * spiked collar and a flaming tail tip
 * ==================================================================== */
static void hellhound_pup(int f)
{
    Pose p = pose_of(f);
    ArtMat fur = M5(RP(RAMP_STONE, 1), RP(RAMP_STONE, 2), RP(RAMP_STONE, 3), RP(RAMP_STONE, 4), RP(RAMP_STONE, 5));
    ArtMat fluff = M5(RP(RAMP_BONE, 3), RP(RAMP_BONE, 4), RP(RAMP_BONE, 5), RP(RAMP_BONE, 6), RP(RAMP_BONE, 7));
    ArtMat ear = M4(RP(RAMP_VAPE, 1), RP(RAMP_VAPE, 2), RP(RAMP_STONE, 2), RP(RAMP_STONE, 3));
    ArtMat ember = M5(RP(RAMP_LAVA, 3), RP(RAMP_LAVA, 4), RP(RAMP_LAVA, 5), RP(RAMP_LAVA, 6), RP(RAMP_LAVA, 7));
    ArtMat eyes = M4(RP(RAMP_BLOOD, 5), RP(RAMP_BLOOD, 6), RP(RAMP_LAVA, 6), RP(RAMP_LAVA, 7));
    ArtMat collar = M4(RP(RAMP_BLOOD, 2), RP(RAMP_BLOOD, 3), RP(RAMP_BLOOD, 4), RP(RAMP_BLOOD, 5));
    ArtMat steel = MR(RAMP_STEEL, 3, 7);
    ArtMat nose = M3(PP(PAINT_JET, 0), PP(PAINT_JET, 1), PP(PAINT_CHARCOAL, 2));
    ArtMat mouth = M3(RP(RAMP_BLOOD, 0), RP(RAMP_BLOOD, 1), RP(RAMP_BLOOD, 2));
    ArtMat tongue = MR(RAMP_PASTEL_PINK, 3, 6);
    int b = p.bob, dead = p.dstage >= 2, wag = (f == 1) ? 3 : -2, lunge = p.attack ? 2 : 0;
    spr_push();
    death_xform(p.dstage, 30, 26);
    if (p.pain) spr_translate(0, 1);
    /* tail: fluffy, wagging, flaming tip */
    spr_bezier(6, -8 + b, 14, -10 + b, 13 + wag, -19 + b, 3, 2, &fur, 0);
    spr_fluffy(13 + wag, -21 + b, 3, 3, 5, Q12 * 4 / 10, 0, 3000, 600, &ember, SH_GLOW);
    /* back legs peeking out */
    spr_capsule(-9, -7 + b, 3, -10, -2, 3, &fur, 0);
    spr_capsule(9, -7 + b, 3, 10, -2, 3, &fur, 0);
    spr_ellipse(-10, -1, 3, 2, &fur, 0);
    spr_ellipse(10, -1, 3, 2, &fur, 0);
    /* body with a fluffy chest */
    spr_fluffy(0, -11 + b, 10, 8, 9, Q12 * 3 / 10, 0, 300, 2800, &fur, 0);
    spr_fluffy(0, -10 + b, 6, 5, 6, Q12 * 4 / 10, 0, 300, 2800, &fluff, 0);
    /* front legs + paws */
    {
        int lf = p.walk;
        spr_capsule(-4, -8 + b, 3, -4, -2 - (lf > 0), 3, &fur, 0);
        spr_capsule(4, -8 + b, 3, 4, -2 - (lf < 0), 3, &fur, 0);
        spr_ellipse(-4, -1 - (lf > 0), 3, 2, &fluff, 0);
        spr_ellipse(4, -1 - (lf < 0), 3, 2, &fluff, 0);
        spr_px(-5, -1 - (lf > 0), RP(RAMP_STONE, 2)); spr_px(-3, -1 - (lf > 0), RP(RAMP_STONE, 2));
        spr_px(3, -1 - (lf < 0), RP(RAMP_STONE, 2)); spr_px(5, -1 - (lf < 0), RP(RAMP_STONE, 2));
    }
    /* spiked collar */
    spr_ellipse(0, -16 + b, 8, 2, &collar, 0);
    spr_tri(-6, -15 + b, -4, -15 + b, -5, -12 + b, &steel, 0);
    spr_tri(-1, -14 + b, 1, -14 + b, 0, -11 + b, &steel, 0);
    spr_tri(4, -15 + b, 6, -15 + b, 5, -12 + b, &steel, 0);
    /* head */
    spr_push();
    spr_translate(0, b + lunge);
    if (p.pain) spr_rotate(-150);
    /* ember horns */
    part_horn(-4, -29, -2, -5, 2, 2, &ember);
    part_horn(4, -29, 2, -5, -2, 2, &ember);
    spr_fluffy(0, -22, 10, 8, 11, Q12 * 25 / 100, 0, 0, 4096, &fur, 0);
    /* floppy ears */
    spr_ellipse_r(-10, -21 + (p.pain ? -3 : 0), 3, 6, p.pain ? -500 : 300, &ear, 0);
    spr_ellipse_r(10, -21 + (p.pain ? -3 : 0), 3, 6, p.pain ? 500 : -300, &ear, 0);
    /* muzzle */
    spr_ellipse(0, -18, 5, 4, &fluff, 0);
    if (p.attack || p.dstage == 1) {
        spr_ellipse(0, -15, 4, 3, &mouth, SH_FLAT);
        spr_ellipse(0, -14, 2, 1, &tongue, SH_NOCONTOUR);
        part_fang(-3, -17, 1, 2, 1, PAL_WHITE);
        part_fang(3, -17, 1, 2, 1, PAL_WHITE);
    } else {
        spr_line(-2, -16, 0, -17, RP(RAMP_STONE, 1));
        spr_line(0, -17, 2, -16, RP(RAMP_STONE, 1));
        part_fang(-2, -15, 1, 1, 1, PAL_WHITE);
        if (dead || f == 1) { spr_px(1, -15, PP(PAINT_ROSE, 2)); spr_px(1, -14, PP(PAINT_ROSE, 1)); }
    }
    spr_ellipse(0, -20, 2, 1, &nose, SH_NOCONTOUR);
    spr_px(-1, -21, PAL_GRAY_L);
    eye_demon(-4, -24, 3, &eyes, p.pain, dead, 1);
    eye_demon(4, -24, 3, &eyes, p.pain, dead, 0);
    /* glowing cracks in the fur */
    spr_px(-7, -26, RP(RAMP_LAVA, 5)); spr_px(-6, -27, RP(RAMP_LAVA, 4));
    spr_px(7, -19, RP(RAMP_LAVA, 5));
    spr_pop();
    spr_px(-6, -9 + b, RP(RAMP_LAVA, 5)); spr_px(-7, -10 + b, RP(RAMP_LAVA, 4));
    spr_px(7, -12 + b, RP(RAMP_LAVA, 5));
    spr_pop();
    finish();
}

/* ======================================================================
 * VAPE WRAITH - a ghost of flavoured vapour: hollow glowing eyes, wispy
 * tail, smoky arms, a little vape pen
 * ==================================================================== */
static void vape_wraith(int f)
{
    Pose p = pose_of(f);
    ArtMat vap = M5(RP(RAMP_VAPE, 3), RP(RAMP_VAPE, 4), RP(RAMP_VAPE, 5), RP(RAMP_VAPE, 6), RP(RAMP_VAPE, 7));
    ArtMat wisp = M4(RP(RAMP_VAPE, 2), RP(RAMP_VAPE, 3), RP(RAMP_VAPE, 4), RP(RAMP_VAPE, 5));
    ArtMat hole = M3(RP(RAMP_VAPE, 0), RP(RAMP_VAPE, 0), RP(RAMP_VAPE, 1));
    ArtMat glow = M4(PP(PAINT_MAGENTA, 1), PP(PAINT_MAGENTA, 2), RP(RAMP_PASTEL_PINK, 7), PAL_WHITE);
    ArtMat puff = M4(RP(RAMP_VAPE, 5), RP(RAMP_VAPE, 6), RP(RAMP_VAPE, 7), PAL_WHITE);
    ArtMat pen = MR(RAMP_STEEL, 2, 6);
    int sway = (f == 1) ? 2 : -2, cy = -28 - (f == 1 ? 1 : 0), k;
    int dead = p.dstage >= 2;
    if (p.dstage >= 4) {
        /* all that is left: a sad little puff and the dropped vape pen */
        spr_ellipse(-2, -4, 7, 4, &wisp, SH_DITHER);
        spr_ellipse(3, -6, 5, 4, &vap, SH_DITHER | SH_SAMEPART);
        part_eye_x(-2, -5, 1, RP(RAMP_VAPE, 1));
        part_eye_x(3, -6, 1, RP(RAMP_VAPE, 1));
        spr_capsule(6, -1, 1, 13, -2, 1, &pen, SH_CYL);
        spr_px(13, -2, PP(PAINT_CYAN, 2));
        finish();
        return;
    }
    spr_push();
    if (p.dstage == 2) { spr_translate(0, 6); spr_scale(Q12 * 90 / 100, Q12 * 90 / 100); }
    if (p.dstage == 3) { spr_translate(0, 16); spr_scale(Q12 * 70 / 100, Q12 * 70 / 100); }
    if (p.pain) { spr_translate(0, -1); spr_rotate(-140); }
    /* tail wisps */
    spr_bezier(-6, cy + 8, -12 + sway, cy + 18, -8 + sway * 2, -3, 5, 1, &wisp, SH_DITHER);
    spr_bezier(0, cy + 10, 3 - sway, cy + 20, sway, -1, 6, 1, &wisp, SH_DITHER | SH_SAMEPART);
    spr_bezier(6, cy + 8, 12 + sway, cy + 16, 9 - sway, -5, 5, 1, &wisp, SH_DITHER | SH_SAMEPART);
    /* smoky left arm, waving */
    spr_bezier(-10, cy + 4, -18, cy + 2, -16, cy - 6 - (f & 1) * 2, 4, 1, &vap, SH_DITHER);
    /* body: head-torso cloud with puffy rim bumps */
    spr_ellipse(0, cy, 13, 12, &vap, SH_DITHER);
    for (k = 0; k < 7; k++) {
        int a = 2200 + k * 300;
        spr_ellipse(13 * art_cos(a) / Q12, cy + 12 * art_sin(a) / Q12, 5, 4, &vap, SH_DITHER | SH_SAMEPART);
    }
    /* right arm holding the pen */
    spr_bezier(10, cy + 4, 17, cy + 8, 14, cy + 2, 4, 2, &vap, SH_DITHER);
    if (p.attack) spr_capsule(13, cy - 1, 1, 19, cy - 8, 1, &pen, SH_CYL);
    else spr_capsule(13, cy + 2, 1, 18, cy - 5, 1, &pen, SH_CYL);
    spr_px(p.attack ? 19 : 18, p.attack ? cy - 8 : cy - 5, PP(PAINT_CYAN, 2));
    /* hollow eyes with glowing pupils */
    if (dead) {
        part_eye_x(-5, cy - 2, 2, RP(RAMP_VAPE, 0));
        part_eye_x(5, cy - 2, 2, RP(RAMP_VAPE, 0));
    } else if (p.pain) {
        part_eye_squeeze(-5, cy - 2, -2, RP(RAMP_VAPE, 0));
        part_eye_squeeze(5, cy - 2, 2, RP(RAMP_VAPE, 0));
    } else {
        int big = (p.dstage == 1) ? 1 : 0;
        spr_ellipse(-5, cy - 2, 3 + big, 4 + big, &hole, SH_FLAT);
        spr_ellipse(5, cy - 2, 3 + big, 4 + big, &hole, SH_FLAT);
        spr_ellipse(-5, cy - 1, 1, 1, &glow, SH_GLOW | SH_NOCONTOUR);
        spr_ellipse(5, cy - 1, 1, 1, &glow, SH_GLOW | SH_NOCONTOUR);
        spr_px(-6, cy - 2, PAL_WHITE);
        spr_px(4, cy - 2, PAL_WHITE);
    }
    /* mouth */
    if (p.attack) {
        spr_ellipse(0, cy + 6, 4, 4, &hole, SH_FLAT);
        spr_ellipse(-3, cy + 12, 4, 3, &puff, SH_NOCONTOUR);
        spr_ellipse(3, cy + 13, 5, 3, &puff, SH_NOCONTOUR | SH_SAMEPART);
        spr_ellipse(0, cy + 9, 3, 2, &puff, SH_NOCONTOUR | SH_SAMEPART);
    } else if (p.pain || p.dstage == 1) {
        spr_ellipse(0, cy + 6, 2, 3, &hole, SH_FLAT);
    } else {
        spr_ellipse(0, cy + 6, 3, 2, &hole, SH_FLAT);
        spr_ellipse(0, cy + 7, 1, 1, &glow, SH_NOCONTOUR);
    }
    spr_pop();
    if (p.dstage == 2) spr_erode(0xA11u, 40);
    if (p.dstage == 3) spr_erode(0xA12u, 100);
    spr_clean();
    finish();
}

/* ======================================================================
 * BONE JESTER - a skeleton harlequin: belled hat, ruff, marotte
 * ==================================================================== */
static void jester_hat(int x, int y, const ArtMat *a, const ArtMat *b, const ArtMat *gold, int jig)
{
    spr_bezier(x - 2, y - 1, x - 10, y - 5 - jig, x - 12, y + 3, 3, 1, a, 0);
    spr_bezier(x + 2, y - 1, x + 10, y - 5 + jig, x + 12, y + 3, 3, 1, a, 0);
    spr_bezier(x, y - 2, x - 1, y - 10, x + 4, y - 11 - jig, 3, 1, b, 0);
    spr_ellipse(x - 12, y + 4, 2, 2, gold, SH_SPEC);
    spr_ellipse(x + 12, y + 4, 2, 2, gold, SH_SPEC);
    spr_ellipse(x + 5, y - 11 - jig, 2, 2, gold, SH_SPEC);
}

static void skull_head(int x, int y, int jaw_open, int dead, int pain, const ArtMat *bone, const ArtMat *glow)
{
    ArtMat hole = M3(RP(RAMP_VAPE, 0), RP(RAMP_VAPE, 0), RP(RAMP_VAPE, 1));
    spr_ellipse(x, y + 4 + jaw_open, 4, 2, bone, 0);
    spr_ellipse(x, y, 6, 5, bone, 0);
    if (dead) {
        spr_ellipse(x - 3, y, 2, 2, &hole, SH_FLAT);
        spr_ellipse(x + 3, y, 2, 2, &hole, SH_FLAT);
    } else if (pain) {
        part_eye_squeeze(x - 3, y, -2, RP(RAMP_VAPE, 0));
        part_eye_squeeze(x + 3, y, 2, RP(RAMP_VAPE, 0));
    } else {
        spr_ellipse(x - 3, y, 2, 2, &hole, SH_FLAT);
        spr_ellipse(x + 3, y, 2, 2, &hole, SH_FLAT);
        spr_px(x - 3, y, glow->c[glow->n - 1]);
        spr_px(x + 2, y, glow->c[glow->n - 1]);
        spr_px(x - 3, y + 1, glow->c[1]);
        spr_px(x + 2, y + 1, glow->c[1]);
    }
    spr_px(x, y + 3, RP(RAMP_BONE, 1));
    spr_px(x - 1, y + 3, RP(RAMP_BONE, 2));
    {
        int k;
        for (k = -3; k <= 2; k++) spr_px(x + k, y + 5, (k & 1) ? RP(RAMP_BONE, 2) : RP(RAMP_BONE, 7));
        if (jaw_open) {
            for (k = -2; k <= 1; k++) spr_px(x + k, y + 6, RP(RAMP_VAPE, 0));
            for (k = -3; k <= 2; k++) spr_px(x + k, y + 7, (k & 1) ? RP(RAMP_BONE, 2) : RP(RAMP_BONE, 7));
        }
    }
}

static void bone_jester(int f)
{
    Pose p = pose_of(f);
    ArtMat bone = MR(RAMP_BONE, 3, 7);
    ArtMat purple = M5(RP(RAMP_VAPE, 1), RP(RAMP_VAPE, 2), RP(RAMP_VAPE, 3), RP(RAMP_VAPE, 4), RP(RAMP_VAPE, 5));
    ArtMat red = M5(RP(RAMP_BLOOD, 2), RP(RAMP_BLOOD, 3), RP(RAMP_BLOOD, 4), RP(RAMP_BLOOD, 5), RP(RAMP_BLOOD, 6));
    ArtMat ruff = M4(RP(RAMP_STONE, 5), RP(RAMP_STONE, 6), RP(RAMP_STONE, 7), PAL_WHITE);
    ArtMat gold = MR(RAMP_CANDY, 2, 6);
    ArtMat wood = MR(RAMP_WOOD, 2, 5);
    ArtMat glow = M4(PP(PAINT_CYAN, 0), PP(PAINT_CYAN, 1), PP(PAINT_CYAN, 2), PAL_WHITE);
    ArtMat orb = M5(RP(RAMP_VAPE, 4), PP(PAINT_MAGENTA, 1), PP(PAINT_MAGENTA, 2), RP(RAMP_PASTEL_PINK, 7), PAL_WHITE);
    int b = p.bob * 2 - (f == 0 ? 0 : 0), k;
    int hop = (f == 1) ? -2 : 0;
    if (p.dstage >= 2) {
        /* collapsing into a bone pile: tunic slumps, skull drops, hat falls */
        int st = p.dstage, sk = st == 2 ? -20 : (st == 3 ? -10 : -7);
        int pts[18];
        int flat = st == 2 ? 10 : (st == 3 ? 6 : 4);
        spr_capsule(-9, -2, 1, 2, -4, 1, &bone, 0);
        spr_capsule(-3, -1, 1, 9, -3, 1, &bone, 0);
        spr_ellipse(-9, -2, 2, 1, &purple, 0);
        spr_ellipse(10, -2, 2, 1, &red, 0);
        pts[0] = -10; pts[1] = -1; pts[2] = -8; pts[3] = -flat - 1; pts[4] = 0; pts[5] = -flat - 2;
        pts[6] = 8; pts[7] = -flat - 1; pts[8] = 10; pts[9] = -1; pts[10] = 6; pts[11] = 1; pts[12] = 2; pts[13] = -1;
        pts[14] = -2; pts[15] = 1; pts[16] = -6; pts[17] = -1;
        spr_poly(pts, 9, &purple, 0);
        spr_rect(0, -flat - 1, 9, flat, &red, SH_ONTOP | SH_NOCONTOUR);
        for (k = 0; k < 5; k++) spr_ellipse(-6 + k * 3, -flat - 2, 2, 2, &ruff, k ? SH_SAMEPART : 0);
        spr_push();
        spr_translate(st == 4 ? 4 : 0, sk);
        spr_rotate(st == 2 ? 200 : 500);
        skull_head(0, 0, st == 2, 1, 0, &bone, &glow);
        spr_pop();
        jester_hat(st == 4 ? -9 : -4, sk - 6 + (st == 4 ? 4 : 0), &purple, &red, &gold, 0);
        finish();
        return;
    }
    spr_push();
    if (p.dstage == 1) spr_translate(0, -3);
    if (p.pain) { spr_translate(1, 0); spr_rotate(-150); }
    /* legs + curly shoes */
    {
        int la = (f == 0) ? -3 : 0, ra = (f == 1) ? -3 : 0;
        spr_capsule(-3, -15 + hop, 1, -4, -3 + la, 1, &bone, 0);
        spr_capsule(3, -15 + hop, 1, 4, -3 + ra, 1, &bone, 0);
        spr_ellipse(-5, -2 + la, 3, 2, &purple, 0);
        spr_bezier(-7, -2 + la, -10, -2 + la, -10, -5 + la, 1, 1, &purple, SH_SAMEPART);
        spr_ellipse(-10, -6 + la, 1, 1, &gold, 0);
        spr_ellipse(5, -2 + ra, 3, 2, &red, 0);
        spr_bezier(7, -2 + ra, 10, -2 + ra, 10, -5 + ra, 1, 1, &red, SH_SAMEPART);
        spr_ellipse(10, -6 + ra, 1, 1, &gold, 0);
    }
    /* marotte (behind the right hand) */
    if (!p.attack) {
        spr_capsule(10, -12 + b + hop, 1, 12, -32 + b + hop, 1, &wood, 0);
        spr_ellipse(12, -35 + b + hop, 3, 3, &bone, 0);
        spr_px(11, -35 + b + hop, RP(RAMP_VAPE, 0)); spr_px(13, -35 + b + hop, RP(RAMP_VAPE, 0));
        spr_tri(9, -37 + b + hop, 15, -37 + b + hop, 14, -42 + b + hop, &red, 0);
        spr_ellipse(14, -42 + b + hop, 1, 1, &gold, 0);
    }
    /* tunic: harlequin split + gold diamond + zigzag hem */
    {
        int t[22], y0 = -27 + b + hop, y1 = -14 + b + hop, n = 0;
        t[n++] = -6; t[n++] = y0; t[n++] = 0; t[n++] = y0;
        t[n++] = 0; t[n++] = y1 + 3; t[n++] = -2; t[n++] = y1;
        t[n++] = -4; t[n++] = y1 + 3; t[n++] = -6; t[n++] = y1; t[n++] = -8; t[n++] = y1 + 3;
        spr_poly(t, n / 2, &purple, 0);
        n = 0;
        t[n++] = 0; t[n++] = y0; t[n++] = 6; t[n++] = y0;
        t[n++] = 8; t[n++] = y1 + 3; t[n++] = 6; t[n++] = y1;
        t[n++] = 4; t[n++] = y1 + 3; t[n++] = 2; t[n++] = y1; t[n++] = 0; t[n++] = y1 + 3;
        spr_poly(t, n / 2, &red, 0);
        n = 0;
        t[n++] = 0; t[n++] = y0 + 3; t[n++] = 3; t[n++] = y0 + 7; t[n++] = 0; t[n++] = y0 + 11;
        t[n++] = -3; t[n++] = y0 + 7;
        spr_poly(t, n / 2, &gold, 0);
        spr_px(0, y0 + 6, RP(RAMP_CANDY, 7));
    }
    /* arms */
    if (p.attack) {
        spr_capsule(-6, -25 + hop, 1, -10, -19 + hop, 1, &bone, 0);
        spr_capsule(-10, -19 + hop, 1, -12, -13 + hop, 1, &bone, 0);
        spr_capsule(6, -25, 1, 11, -32, 1, &bone, 0);
        spr_capsule(11, -32, 1, 12, -39, 1, &bone, 0);
        part_glow(12, -44, 6, &orb);
        spr_ellipse(12, -44, 3, 3, &orb, SH_GLOW | SH_NOCONTOUR);
        spr_px(11, -45, PAL_WHITE);
    } else {
        int w = (f == 1) ? -3 : 0;
        spr_capsule(-6, -25 + b + hop, 1, -10, -20 + b + hop + w, 1, &bone, 0);
        spr_capsule(-10, -20 + b + hop + w, 1, -11, -26 + b + hop + w * 2, 1, &bone, 0);
        spr_capsule(6, -25 + b + hop, 1, 9, -18 + b + hop, 1, &bone, 0);
        spr_capsule(9, -18 + b + hop, 1, 10, -14 + b + hop, 1, &bone, 0);
        spr_ellipse(10, -14 + b + hop, 1, 1, &bone, 0);
    }
    /* ruff collar */
    for (k = 0; k < 6; k++) spr_ellipse(-6 + k * 12 / 5, -27 + b + hop, 2, 2, &ruff, k ? SH_SAMEPART : 0);
    skull_head(0, -34 + b + hop, p.attack || p.dstage == 1, 0, p.pain, &bone, &glow);
    jester_hat(0, -38 + b + hop, &purple, &red, &gold, f == 1 ? 2 : 0);
    spr_pop();
    finish();
}

/* ======================================================================
 * SLIME BELCHER - a wobbling bile blob with goofy eyestalks and a
 * swallowed skull sloshing inside
 * ==================================================================== */
static void slime_eye(int x, int y, int r, int look, int dead)
{
    ArtMat white = M3(PAL_GRAY_L, PAL_WHITE, PAL_WHITE);
    ArtMat iris = M3(PP(PAINT_CHARTREUSE, 0), PP(PAINT_CHARTREUSE, 1), PP(PAINT_LEMON, 2));
    spr_ellipse(x, y, r, r, &white, 0);
    if (dead) { part_eye_x(x, y, 1, RP(RAMP_SICK, 1)); return; }
    spr_ellipse(x + look, y, r - 1, r - 1, &iris, SH_ONTOP | SH_NOCONTOUR);
    spr_px(x + look, y, PP(PAINT_JET, 0));
    spr_px(x + look, y + 1, PP(PAINT_JET, 0));
    spr_px(x + look - 1, y - 1, PAL_WHITE);
}

static void slime_belcher(int f)
{
    Pose p = pose_of(f);
    ArtMat slime = M5(RP(RAMP_SICK, 3), RP(RAMP_SICK, 4), RP(RAMP_SICK, 5), PP(PAINT_SLIME, 2), RP(RAMP_SICK, 7));
    ArtMat dslime = M4(RP(RAMP_SICK, 2), RP(RAMP_SICK, 3), RP(RAMP_SICK, 4), RP(RAMP_SICK, 5));
    ArtMat mouth = M3(RP(RAMP_SICK, 0), RP(RAMP_SICK, 1), RP(RAMP_SICK, 1));
    ArtMat bone = MR(RAMP_BONE, 2, 5);
    ArtMat spray = M4(RP(RAMP_SICK, 5), PP(PAINT_SLIME, 2), RP(RAMP_SICK, 7), PAL_WHITE);
    int sx = Q12, sy = Q12, ey = 0, k;
    int dead = p.dstage >= 2;
    switch (f) {
    case 0: sx = Q12 * 106 / 100; sy = Q12 * 94 / 100; break;
    case 1: sx = Q12 * 95 / 100; sy = Q12 * 106 / 100; break;
    case 2: sx = Q12 * 102 / 100; sy = Q12 * 104 / 100; break;
    case 3: sx = Q12 * 110 / 100; sy = Q12 * 90 / 100; break;
    case 4: sx = Q12 * 92 / 100; sy = Q12 * 112 / 100; break;
    case 5: sx = Q12 * 116 / 100; sy = Q12 * 72 / 100; ey = 4; break;
    case 6: sx = Q12 * 128 / 100; sy = Q12 * 42 / 100; ey = 10; break;
    default: sx = Q12 * 134 / 100; sy = Q12 * 22 / 100; ey = 14; break;
    }
    /* eyestalks first (behind the dome) */
    if (f < 6) {
        int ls = (f == 1) ? -1 : 1;
        spr_capsule(-6, -26 * sy / Q12, 2, -9 + ls, -36 * sy / Q12 - 2 + ey / 2, 1, &slime, 0);
        spr_capsule(6, -26 * sy / Q12, 2, 9 + ls, -37 * sy / Q12 - 2 + ey / 2, 1, &slime, 0);
    }
    spr_push();
    spr_scale(sx, sy);
    /* puddle base + dome */
    spr_ellipse(0, -3, 20, 3, &dslime, 0);
    spr_ellipse(0, -16, 18, 16, &slime, SH_SPEC);
    /* swallowed skull and bubbles inside */
    spr_ellipse(7, -10, 3, 3, &bone, SH_ONTOP | SH_NOCONTOUR);
    spr_px(6, -10, RP(RAMP_SICK, 1)); spr_px(8, -10, RP(RAMP_SICK, 1));
    spr_px(-10, -8, RP(RAMP_SICK, 6)); spr_px(-9, -9, RP(RAMP_SICK, 7));
    spr_px(-12, -20, RP(RAMP_SICK, 6)); spr_px(11, -22, RP(RAMP_SICK, 6));
    /* mouth */
    if (p.attack) {
        spr_ellipse(0, -15, 10, 7, &mouth, SH_FLAT);
        spr_ellipse(0, -11, 6, 2, &dslime, SH_ONTOP | SH_NOCONTOUR);
    } else if (dead) {
        spr_line(-7, -14, 7, -14, RP(RAMP_SICK, 1));
    } else {
        spr_ellipse(0, -14, 9, 3, &mouth, SH_FLAT);
        spr_ellipse(0, -16, 10, 2, &slime, SH_ONTOP | SH_NOCONTOUR);
        spr_capsule(4, -12, 1, 4, -8 + (f & 1), 1, &spray, SH_NOCONTOUR);      /* drool */
    }
    /* side drips */
    spr_capsule(-15, -10, 2, -16, -5 + (f & 1), 1, &slime, SH_SAMEPART);
    spr_capsule(16, -12, 2, 17, -7, 1, &slime, SH_SAMEPART);
    spr_pop();
    /* eyes on the stalk tips (or lying in the puddle) */
    if (f < 6) {
        int ls = (f == 1) ? -1 : 1;
        int look = p.pain ? 0 : ((f == 1) ? -1 : 1);
        if (p.pain) {
            ArtMat white = M3(PAL_GRAY_L, PAL_WHITE, PAL_WHITE);
            spr_ellipse(-9 + ls, -37 * sy / Q12 - 3 + ey / 2, 3, 3, &white, 0);
            spr_ellipse(9 + ls, -38 * sy / Q12 - 3 + ey / 2, 3, 3, &white, 0);
            part_eye_squeeze(-9 + ls, -37 * sy / Q12 - 3, -1, RP(RAMP_SICK, 1));
            part_eye_squeeze(9 + ls, -38 * sy / Q12 - 3, 1, RP(RAMP_SICK, 1));
        } else {
            slime_eye(-9 + ls, -37 * sy / Q12 - 3 + ey / 2, 3, look, dead);
            slime_eye(9 + ls, -38 * sy / Q12 - 3 + ey / 2, 3, -look, dead);
        }
    } else {
        slime_eye(-8, -5, 3, 0, 1);
        slime_eye(9, -4, 3, 0, 1);
        spr_capsule(-2, -2, 1, 5, -3, 1, &bone, 0);
    }
    if (p.attack)
        for (k = 0; k < 5; k++) {
            uint32_t h = art_hash((uint32_t)k, 5u, 0x5B1Eu);
            int bx = (int)(h % 17u) - 8, by = -10 - (int)((h >> 8) % 6u);
            spr_ellipse(bx, by, 2, 2, &spray, SH_GLOW | SH_NOCONTOUR);
        }
    finish();
}

/* ======================================================================
 * GARGOYLE - lavender stone, bat wings, glowing eyes; crumbles to rubble
 * ==================================================================== */
static void gargoyle(int f)
{
    Pose p = pose_of(f);
    ArtMat stone = M5(RP(RAMP_STONE, 2), RP(RAMP_STONE, 3), RP(RAMP_STONE, 4), RP(RAMP_STONE, 5), RP(RAMP_STONE, 6));
    ArtMat lstone = M5(RP(RAMP_STONE, 3), RP(RAMP_STONE, 4), RP(RAMP_STONE, 5), RP(RAMP_STONE, 6), RP(RAMP_STONE, 7));
    ArtMat memb = M4(RP(RAMP_VAPE, 1), RP(RAMP_STONE, 2), RP(RAMP_VAPE, 2), RP(RAMP_STONE, 3));
    ArtMat horn = M4(RP(RAMP_STONE, 1), RP(RAMP_STONE, 2), RP(RAMP_STONE, 3), RP(RAMP_STONE, 5));
    ArtMat eyes = M4(RP(RAMP_CANDY, 4), RP(RAMP_CANDY, 5), RP(RAMP_CANDY, 6), RP(RAMP_CANDY, 7));
    ArtMat mouth = M3(RP(RAMP_STONE, 0), RP(RAMP_STONE, 0), RP(RAMP_STONE, 1));
    int hov = ((f == 1) ? -2 : 0) + 6, flap = (f == 1) ? -8 : (f == 0 ? 4 : -2), k;
    int fold = (p.dstage == 2) ? 9 : 0;
    int dead = p.dstage >= 2;
    if (p.dstage >= 3) {
        /* rubble: chunks of stone with the head on top */
        int st = p.dstage;
        for (k = 0; k < 9; k++) {
            uint32_t h = art_hash((uint32_t)k, (uint32_t)st, 0x6A26u);
            int bx = (int)(h % 31u) - 15, r = 2 + (int)((h >> 8) % 3u);
            spr_ellipse_r(bx, -r, r + 1, r, (int)(h >> 12) & 4095, k & 1 ? &stone : &lstone, 0);
        }
        spr_tri(-14, -2, -6, -3, -12, -9, &memb, 0);
        spr_tri(8, -2, 16, -2, 13, -7, &memb, 0);
        spr_push();
        spr_translate(st == 3 ? -2 : 1, st == 3 ? -9 : -7);
        spr_rotate(st == 3 ? 400 : 900);
        spr_ellipse(0, 0, 6, 5, &stone, 0);
        part_horn(-3, -4, -3, -4, 2, 2, &horn);
        part_horn(3, -4, 3, -4, -2, 2, &horn);
        part_eye_x(-2, -1, 1, RP(RAMP_STONE, 0));
        part_eye_x(3, -1, 1, RP(RAMP_STONE, 0));
        spr_pop();
        finish();
        return;
    }
    spr_push();
    spr_scale(Q12 * 94 / 100, Q12 * 94 / 100);
    if (p.dstage == 2) { spr_translate(0, 2); spr_rotate(220); }
    if (p.dstage == 1) spr_translate(0, -2);
    if (p.pain) spr_rotate(160);
    /* wings */
    for (k = -1; k <= 1; k += 2) {
        int w[12];
        w[0] = k * 6; w[1] = -36 + hov;
        w[2] = k * (22 - fold); w[3] = -48 + hov + flap;
        w[4] = k * (24 - fold); w[5] = -32 + hov + flap / 2;
        w[6] = k * (20 - fold); w[7] = -22 + hov + flap / 3;
        w[8] = k * (16 - fold / 2); w[9] = -26 + hov;
        w[10] = k * 8; w[11] = -24 + hov;
        spr_poly(w, 6, &memb, 0);
        spr_line(k * 7, -35 + hov, k * (22 - fold), -47 + hov + flap, RP(RAMP_STONE, 5));
        spr_line(k * 7, -35 + hov, k * (24 - fold), -32 + hov + flap / 2, RP(RAMP_STONE, 4));
        spr_line(k * 7, -35 + hov, k * (20 - fold), -23 + hov + flap / 3, RP(RAMP_STONE, 4));
    }
    /* tail */
    spr_bezier(2, -18 + hov, 12, -8 + hov, 14, -16 + hov, 2, 1, &stone, 0);
    spr_tri(12, -16 + hov, 17, -18 + hov, 15, -13 + hov, &stone, 0);
    /* crouched legs */
    spr_capsule(-5, -20 + hov, 3, -9, -14 + hov, 3, &stone, 0);
    spr_capsule(-9, -14 + hov, 2, -6, -8 + hov, 2, &stone, 0);
    spr_capsule(5, -20 + hov, 3, 9, -14 + hov, 3, &stone, 0);
    spr_capsule(9, -14 + hov, 2, 6, -8 + hov, 2, &stone, 0);
    part_paw(-6, -7 + hov, 2, &lstone, 1, RP(RAMP_STONE, 7));
    part_paw(6, -7 + hov, 2, &lstone, 1, RP(RAMP_STONE, 7));
    /* torso */
    spr_ellipse(0, -26 + hov, 8, 9, &stone, 0);
    spr_ellipse(0, -24 + hov, 5, 5, &lstone, 0);
    /* arms */
    if (p.attack) {
        spr_capsule(-7, -31 + hov, 2, -12, -38 + hov, 2, &stone, 0);
        spr_capsule(7, -31 + hov, 2, 12, -38 + hov, 2, &stone, 0);
        part_paw(-12, -40 + hov, 3, &lstone, 1, RP(RAMP_STONE, 7));
        part_paw(12, -40 + hov, 3, &lstone, 1, RP(RAMP_STONE, 7));
    } else {
        spr_capsule(-7, -31 + hov, 2, -10, -22 + hov, 2, &stone, 0);
        spr_capsule(7, -31 + hov, 2, 10, -22 + hov, 2, &stone, 0);
        part_paw(-10, -21 + hov, 2, &lstone, 1, RP(RAMP_STONE, 7));
        part_paw(10, -21 + hov, 2, &lstone, 1, RP(RAMP_STONE, 7));
    }
    /* head */
    part_horn(-4, -42 + hov, -6, -6, 4, 2, &horn);
    part_horn(4, -42 + hov, 6, -6, -4, 2, &horn);
    spr_tri(-6, -40 + hov, -12, -45 + hov, -5, -36 + hov, &stone, 0);
    spr_tri(6, -40 + hov, 12, -45 + hov, 5, -36 + hov, &stone, 0);
    spr_ellipse(0, -38 + hov, 7, 6, &stone, 0);
    spr_ellipse(0, -34 + hov, 4, 3, &lstone, 0);
    eye_demon(-3, -39 + hov, 2, &eyes, p.pain, dead, 1);
    eye_demon(3, -39 + hov, 2, &eyes, p.pain, dead, 0);
    if (!p.pain && !dead) {
        spr_line(-6, -42 + hov, -1, -41 + hov, RP(RAMP_STONE, 1));
        spr_line(6, -42 + hov, 1, -41 + hov, RP(RAMP_STONE, 1));
    }
    if (p.attack || p.dstage == 1 || p.pain) {
        spr_ellipse(0, -33 + hov, 3, 2, &mouth, SH_FLAT);
        part_fang(-2, -35 + hov, 1, 2, 1, RP(RAMP_STONE, 7));
        part_fang(2, -35 + hov, 1, 2, 1, RP(RAMP_STONE, 7));
    } else {
        spr_line(-2, -33 + hov, 2, -33 + hov, RP(RAMP_STONE, 1));
        part_fang(-2, -32 + hov, 1, 2, -1, RP(RAMP_STONE, 7));
        part_fang(2, -32 + hov, 1, 2, -1, RP(RAMP_STONE, 7));
    }
    /* cracks + moss */
    spr_line(-4, -29 + hov, -2, -25 + hov, RP(RAMP_STONE, 1));
    spr_px(5, -21 + hov, RP(RAMP_MINT, 4)); spr_px(6, -21 + hov, RP(RAMP_MINT, 3));
    spr_px(-3, -44 + hov, RP(RAMP_MINT, 4));
    if (p.dstage >= 1) {
        spr_line(-6, -36 + hov, 0, -28 + hov, RP(RAMP_STONE, 0));
        spr_line(0, -28 + hov, 3, -18 + hov, RP(RAMP_STONE, 0));
    }
    spr_pop();
    if (p.dstage == 2) spr_erode(0x6A27u, 20);
    finish();
}

/* ======================================================================
 * CYBER PIGGY - a pink pig demon with a cyber eye and an arm cannon
 * ==================================================================== */
static void cyber_piggy(int f)
{
    Pose p = pose_of(f);
    ArtMat pink = M5(RP(RAMP_PASTEL_PINK, 3), RP(RAMP_FLESH, 5), RP(RAMP_PASTEL_PINK, 4), RP(RAMP_PASTEL_PINK, 5),
                     RP(RAMP_PASTEL_PINK, 6));
    ArtMat snout = M4(RP(RAMP_FLESH, 4), RP(RAMP_FLESH, 5), RP(RAMP_FLESH, 6), RP(RAMP_FLESH, 7));
    ArtMat steel = MR(RAMP_STEEL, 2, 6);
    ArtMat dsteel = MR(RAMP_STEEL, 1, 4);
    ArtMat hoof = M3(PP(PAINT_CHOCOLATE, 0), PP(PAINT_CHOCOLATE, 1), PP(PAINT_CHOCOLATE, 2));
    ArtMat led = M4(RP(RAMP_BLOOD, 5), RP(RAMP_BLOOD, 6), RP(RAMP_BLOOD, 7), PAL_WHITE);
    ArtMat core = M4(PP(PAINT_CYAN, 0), PP(PAINT_CYAN, 1), PP(PAINT_CYAN, 2), PAL_WHITE);
    ArtMat flash = M5(RP(RAMP_LAVA, 5), RP(RAMP_LAVA, 6), RP(RAMP_CANDY, 6), RP(RAMP_LAVA, 7), PAL_WHITE);
    ArtMat cable = M3(RP(RAMP_STONE, 1), RP(RAMP_STONE, 2), RP(RAMP_STONE, 3));
    int b = p.bob, dead = p.dstage >= 2, lf = p.walk;
    spr_push();
    death_xform(p.dstage, 40, 30);
    if (p.pain) { spr_translate(-1, 0); spr_rotate(130); }
    /* cables from the back to the cannon */
    spr_bezier(6, -24 + b, 18, -30 + b, 15, -17 + b, 1, 1, &cable, 0);
    /* legs */
    spr_capsule(-5, -9 + b, 3, -5, -3 - (lf > 0), 3, &pink, 0);
    spr_capsule(5, -9 + b, 3, 5, -3 - (lf < 0), 3, &pink, 0);
    spr_ellipse(-5, -6 + b, 3, 2, &steel, 0);
    spr_ellipse(5, -6 + b, 3, 2, &steel, 0);
    spr_ellipse(-5, -1 - (lf > 0), 3, 2, &hoof, 0);
    spr_ellipse(5, -1 - (lf < 0), 3, 2, &hoof, 0);
    spr_px(-5, -1 - (lf > 0), PP(PAINT_CHOCOLATE, 0));
    spr_px(5, -1 - (lf < 0), PP(PAINT_CHOCOLATE, 0));
    /* body + chest plate with a glowing core */
    spr_ellipse(0, -16 + b, 11, 10, &pink, 0);
    {
        int c[12];
        c[0] = -6; c[1] = -23 + b; c[2] = 6; c[3] = -23 + b; c[4] = 7; c[5] = -16 + b;
        c[6] = 3; c[7] = -11 + b; c[8] = -3; c[9] = -11 + b; c[10] = -7; c[11] = -16 + b;
        spr_poly(c, 6, &steel, 0);
        spr_ellipse(0, -17 + b, 2, 2, &core, SH_GLOW);
        spr_px(-4, -21 + b, RP(RAMP_STEEL, 7)); spr_px(4, -21 + b, RP(RAMP_STEEL, 3));
    }
    /* left arm: pink with a hoof fist */
    spr_capsule(-10, -21 + b, 3, -13, -13 + b - lf, 3, &pink, 0);
    spr_ellipse(-13, -11 + b - lf, 3, 3, &hoof, 0);
    /* right arm: the cannon */
    spr_ellipse(10, -22 + b, 5, 4, &steel, 0);
    if (p.attack) {
        spr_capsule(12, -20, 4, 18, -15, 4, &dsteel, SH_CYL);
        spr_ellipse(19, -14, 3, 3, &led, SH_GLOW);
        part_glow(21, -12, 7, &flash);
        spr_star(21, -12, 5, RP(RAMP_CANDY, 7));
        spr_px(21, -12, PAL_WHITE);
    } else {
        spr_capsule(12, -20 + b, 4, 15, -11 + b, 4, &dsteel, SH_CYL);
        spr_ellipse(15, -9 + b, 3, 2, &led, SH_GLOW);
    }
    /* head */
    spr_push();
    spr_translate(0, b);
    spr_tri(-8, -33, -12, -42, -3, -36, &pink, 0);
    spr_tri(8, -33, 12, -42, 3, -36, &pink, 0);
    spr_px(-9, -37, RP(RAMP_FLESH, 4)); spr_px(-10, -39, RP(RAMP_FLESH, 4));
    spr_px(9, -37, RP(RAMP_FLESH, 4)); spr_px(10, -39, RP(RAMP_FLESH, 4));
    spr_ellipse(0, -29, 10, 8, &pink, 0);
    /* metal jaw */
    spr_ellipse(0, -23, 6, 2, &steel, 0);
    part_fang(-5, -24, 1, 3, -1, PAL_WHITE);
    part_fang(5, -24, 1, 3, -1, PAL_WHITE);
    /* snout */
    spr_ellipse(0, -27, 4, 3, &snout, 0);
    spr_px(-1, -27, RP(RAMP_FLESH, 2)); spr_px(1, -27, RP(RAMP_FLESH, 2));
    /* eyes: angry pig eye + cyber eye */
    if (dead) {
        part_eye_x(-5, -31, 1, RP(RAMP_FLESH, 1));
        spr_ellipse(5, -31, 3, 3, &steel, 0);
        spr_px(5, -31, RP(RAMP_STONE, 2));
    } else if (p.pain) {
        part_eye_squeeze(-5, -31, -2, RP(RAMP_FLESH, 1));
        spr_ellipse(5, -31, 3, 3, &steel, 0);
        spr_ellipse(5, -31, 1, 1, &led, SH_GLOW);
    } else {
        ArtMat ered = M3(RP(RAMP_BLOOD, 4), RP(RAMP_BLOOD, 6), RP(RAMP_LAVA, 6));
        spr_ellipse(-5, -31, 2, 2, &ered, SH_GLOW);
        spr_px(-6, -32, PAL_WHITE);
        spr_line(-8, -35, -3, -33, RP(RAMP_FLESH, 2));
        spr_ellipse(5, -31, 3, 3, &steel, 0);
        spr_ellipse(5, -31, 2, 2, &led, SH_GLOW);
        spr_px(4, -32, PAL_WHITE);
    }
    spr_line(8, -36, 9, -27, RP(RAMP_FLESH, 3));          /* scar */
    spr_pop();
    if (p.pain || p.dstage == 1) {
        spr_px(14, -26, RP(RAMP_CANDY, 7)); spr_px(16, -28, RP(RAMP_CANDY, 6));
        spr_px(13, -29, RP(RAMP_CANDY, 7)); spr_px(-12, -24, RP(RAMP_CANDY, 6));
    }
    spr_pop();
    finish();
}

/* ======================================================================
 * BRUTE - a hulking baron of a demon: goat-fur legs, huge horns and fists
 * ==================================================================== */
static void brute(int f)
{
    Pose p = pose_of(f);
    ArtMat skin = M5(RP(RAMP_BLOOD, 2), RP(RAMP_BLOOD, 3), RP(RAMP_FLESH, 3), RP(RAMP_FLESH, 4), RP(RAMP_FLESH, 5));
    ArtMat fur = M5(RP(RAMP_WOOD, 0), RP(RAMP_WOOD, 1), RP(RAMP_WOOD, 2), RP(RAMP_WOOD, 3), RP(RAMP_WOOD, 4));
    ArtMat horn = M5(RP(RAMP_BONE, 2), RP(RAMP_BONE, 3), RP(RAMP_BONE, 4), RP(RAMP_BONE, 5), RP(RAMP_BONE, 6));
    ArtMat hoof = M3(PP(PAINT_JET, 1), PP(PAINT_JET, 2), PP(PAINT_CHARCOAL, 1));
    ArtMat belt = M4(RP(RAMP_WOOD, 0), RP(RAMP_WOOD, 1), RP(RAMP_WOOD, 2), RP(RAMP_WOOD, 3));
    ArtMat bone = MR(RAMP_BONE, 3, 7);
    ArtMat eyes = M4(RP(RAMP_SICK, 5), RP(RAMP_SICK, 6), RP(RAMP_SICK, 7), PAL_WHITE);
    ArtMat fire = M5(RP(RAMP_SICK, 4), PP(PAINT_SLIME, 1), PP(PAINT_SLIME, 2), RP(RAMP_SICK, 7), PAL_WHITE);
    ArtMat mouth = M3(RP(RAMP_BLOOD, 0), RP(RAMP_BLOOD, 0), RP(RAMP_BLOOD, 1));
    int b = p.bob, lf = p.walk, dead = p.dstage >= 2, k;
    spr_push();
    spr_scale(Q12 * 90 / 100, Q12 * 90 / 100);
    death_xform(p.dstage, 60, 46);
    if (p.pain) { spr_translate(0, 1); spr_rotate(-90); }
    /* furry goat legs */
    for (k = -1; k <= 1; k += 2) {
        int up = (lf == k) ? 2 : 0;
        spr_fluffy(k * 8, -16 + b, 6, 7, 7, Q12 * 3 / 10, 0, 300, 2800, &fur, 0);
        spr_capsule(k * 8, -11 + b, 3, k * 9, -3 - up, 3, &fur, 0);
        spr_ellipse(k * 9, -1 - up, 4, 2, &hoof, 0);
    }
    /* torso */
    spr_ellipse(0, -36 + b, 16, 13, &skin, 0);
    spr_ellipse(-6, -39 + b, 7, 5, &skin, 0);
    spr_ellipse(6, -39 + b, 7, 5, &skin, 0);
    spr_ellipse(0, -29 + b, 7, 5, &skin, 0);
    spr_line(0, -33 + b, 0, -25 + b, RP(RAMP_BLOOD, 2));
    spr_line(-5, -29 + b, 5, -29 + b, RP(RAMP_BLOOD, 3));
    /* belt with skull buckle */
    spr_ellipse(0, -23 + b, 13, 3, &belt, 0);
    spr_ellipse(0, -23 + b, 3, 3, &bone, 0);
    spr_px(-1, -23 + b, RP(RAMP_BONE, 1)); spr_px(1, -23 + b, RP(RAMP_BONE, 1));
    /* shoulders with bone spikes */
    for (k = -1; k <= 1; k += 2) {
        spr_tri(k * 13, -46 + b, k * 17, -46 + b, k * 16, -53 + b, &bone, 0);
        spr_tri(k * 18, -44 + b, k * 21, -42 + b, k * 22, -49 + b, &bone, 0);
        spr_ellipse(k * 16, -42 + b, 6, 6, &skin, 0);
    }
    /* arms */
    spr_capsule(-17, -40 + b, 5, -21, -30 + b, 5, &skin, 0);
    spr_capsule(-21, -30 + b, 5, -20, -20 + b - lf, 5, &skin, 0);
    spr_ellipse(-20, -17 + b - lf, 6, 5, &skin, 0);
    spr_line(-23, -18 + b - lf, -17, -18 + b - lf, RP(RAMP_BLOOD, 2));
    if (p.attack) {
        spr_capsule(17, -42, 5, 22, -54, 5, &skin, 0);
        spr_capsule(22, -54, 5, 20, -63, 5, &skin, 0);
        spr_ellipse(19, -66, 6, 5, &skin, 0);
        part_glow(19, -75, 9, &fire);
        spr_ellipse(19, -75, 5, 5, &fire, SH_GLOW | SH_NOCONTOUR);
        spr_px(17, -77, PAL_WHITE);
    } else {
        spr_capsule(17, -40 + b, 5, 21, -30 + b, 5, &skin, 0);
        spr_capsule(21, -30 + b, 5, 20, -20 + b + lf, 5, &skin, 0);
        spr_ellipse(20, -17 + b + lf, 6, 5, &skin, 0);
        spr_line(17, -18 + b + lf, 23, -18 + b + lf, RP(RAMP_BLOOD, 2));
    }
    /* head + horns */
    part_horn(-4, -55 + b, -10, -8, 4, 3, &horn);
    part_horn(4, -55 + b, 10, -8, -4, 3, &horn);
    spr_ellipse(0, -51 + b, 6, 6, &skin, 0);
    eye_demon(-2, -52 + b, 1, &eyes, p.pain, dead, 1);
    eye_demon(3, -52 + b, 1, &eyes, p.pain, dead, 0);
    spr_line(-5, -55 + b, -1, -54 + b, RP(RAMP_BLOOD, 1));
    spr_line(5, -55 + b, 1, -54 + b, RP(RAMP_BLOOD, 1));
    if (p.attack || p.pain || p.dstage == 1) {
        spr_ellipse(0, -47 + b, 3, 2, &mouth, SH_FLAT);
        part_fang(-2, -49 + b, 1, 2, 1, PAL_WHITE);
        part_fang(2, -49 + b, 1, 2, 1, PAL_WHITE);
    } else {
        spr_line(-3, -47 + b, 3, -47 + b, RP(RAMP_BLOOD, 1));
        part_fang(-3, -48 + b, 1, 2, -1, PAL_WHITE);
        part_fang(3, -48 + b, 1, 2, -1, PAL_WHITE);
    }
    spr_pop();
    finish();
}

/* ======================================================================
 * CLOUD SPIDER - a puffy vape cloud on eight spindly legs, six red eyes
 * ==================================================================== */
static void cloud_spider(int f)
{
    Pose p = pose_of(f);
    ArtMat cloud = M5(RP(RAMP_VAPE, 4), RP(RAMP_VAPE, 5), RP(RAMP_VAPE, 6), RP(RAMP_VAPE, 7), PAL_WHITE);
    ArtMat leg = M4(RP(RAMP_VAPE, 0), RP(RAMP_VAPE, 1), RP(RAMP_STONE, 1), RP(RAMP_VAPE, 2));
    ArtMat eyes = M4(RP(RAMP_BLOOD, 5), RP(RAMP_BLOOD, 6), RP(RAMP_BLOOD, 7), PAL_WHITE);
    ArtMat fang = M4(RP(RAMP_VAPE, 0), RP(RAMP_VAPE, 1), RP(RAMP_VAPE, 2), RP(RAMP_BONE, 6));
    ArtMat orb = M5(RP(RAMP_VAPE, 4), PP(PAINT_MAGENTA, 1), PP(PAINT_MAGENTA, 2), RP(RAMP_PASTEL_PINK, 7), PAL_WHITE);
    int k, side, dead = p.dstage >= 2, by = -18 - (f == 1 ? 1 : 0);
    if (p.dstage >= 2) by = p.dstage == 2 ? -9 : -7;
    spr_push();
    spr_scale(Q12 * 95 / 100, Q12 * 95 / 100);
    /* legs (behind the body) */
    for (side = -1; side <= 1; side += 2)
        for (k = 0; k < 4; k++) {
            int lift = ((k + (side > 0) + f) & 1) && f < 2 ? 3 : 0;
            int hx = side * 7, hy = by + 1 + k;
            int kx = side * (14 + k * 2), ky = by - 10 + k * 3 - lift;
            int fx = side * (18 + k * 3), fy = -lift;
            if (p.attack && k == 0) { kx = side * 12; ky = by - 16; fx = side * 16; fy = by - 22; }
            if (p.dstage >= 2) {
                /* curled up: knees up, feet folded back over the body */
                int c = p.dstage - 1;
                kx = side * (12 + k * 2 - c); ky = by - 10 - c * 2 + k;
                fx = side * (6 + k * 2); fy = by - 14 - c * 2 + k * 2;
            }
            spr_capsule(hx, hy, 2, kx, ky, 1, &leg, 0);
            spr_capsule(kx, ky, 1, fx, fy, 1, &leg, 0);
            spr_px(kx, ky, RP(RAMP_VAPE, 3));
        }
    /* abdomen + head cloud */
    spr_push();
    if (p.pain) spr_rotate(120);
    if (p.dstage == 4) { spr_translate(0, 4); spr_scale(Q12, Q12 * 70 / 100); }
    spr_ellipse(0, by - 6, 12, 9, &cloud, SH_DITHER);
    for (k = 0; k < 6; k++) {
        int a = 2048 + 300 + k * 290;
        spr_ellipse(12 * art_cos(a) / Q12, by - 6 + 9 * art_sin(a) / Q12, 4, 4, &cloud, SH_DITHER | SH_SAMEPART);
    }
    spr_ellipse(0, by + 1, 9, 6, &cloud, SH_DITHER);
    /* six eyes */
    if (dead) {
        part_eye_x(-3, by, 1, RP(RAMP_VAPE, 1));
        part_eye_x(3, by, 1, RP(RAMP_VAPE, 1));
    } else if (p.pain) {
        spr_line(-5, by, -1, by, RP(RAMP_VAPE, 1));
        spr_line(1, by, 5, by, RP(RAMP_VAPE, 1));
    } else {
        spr_ellipse(-3, by, 2, 2, &eyes, SH_GLOW);
        spr_ellipse(3, by, 2, 2, &eyes, SH_GLOW);
        spr_ellipse(-6, by - 3, 1, 1, &eyes, SH_GLOW);
        spr_ellipse(6, by - 3, 1, 1, &eyes, SH_GLOW);
        spr_ellipse(-2, by - 4, 1, 1, &eyes, SH_GLOW);
        spr_ellipse(2, by - 4, 1, 1, &eyes, SH_GLOW);
        spr_px(-4, by - 1, PAL_WHITE);
        spr_px(2, by - 1, PAL_WHITE);
    }
    /* fangs */
    {
        int spread = p.attack ? 3 : 0;
        spr_bezier(-2, by + 4, -4 - spread, by + 7, -2 - spread, by + 9, 2, 0, &fang, 0);
        spr_bezier(2, by + 4, 4 + spread, by + 7, 2 + spread, by + 9, 2, 0, &fang, 0);
        if (p.attack) {
            part_glow(0, by + 8, 5, &orb);
            spr_ellipse(0, by + 8, 2, 2, &orb, SH_GLOW | SH_NOCONTOUR);
        }
    }
    spr_pop();
    spr_pop();
    finish();
}

/* ======================================================================
 * LORD VAPULA, Duke of the Ninth Cloud - a towering vapour demon with
 * curling ram horns, a golden crown and an enormous vape mod
 * ==================================================================== */
static void vapula_mod(int x, int y, int ang)
{
    ArtMat mod = M5(RP(RAMP_STEEL, 1), RP(RAMP_STEEL, 2), RP(RAMP_STEEL, 3), RP(RAMP_STEEL, 4), RP(RAMP_STEEL, 6));
    ArtMat scr = M3(PP(PAINT_CYAN, 0), PP(PAINT_CYAN, 1), PP(PAINT_CYAN, 2));
    ArtMat tip = M4(RP(RAMP_VAPE, 2), RP(RAMP_VAPE, 3), PP(PAINT_MAGENTA, 1), PP(PAINT_MAGENTA, 2));
    spr_push();
    spr_translate(x, y);
    spr_rotate(ang);
    spr_rect(-5, -8, 10, 16, &mod, SH_CYL);
    spr_rect(-3, -5, 6, 5, &scr, SH_GLOW | SH_FLAT);
    spr_px(-2, -4, PAL_WHITE);
    spr_rect(-2, -12, 4, 4, &tip, 0);
    spr_ellipse(0, 4, 2, 1, &scr, SH_GLOW);
    spr_pop();
}

static void lord_vapula(int f)
{
    Pose p = pose_of(f);
    ArtMat vap = M5(RP(RAMP_VAPE, 2), RP(RAMP_VAPE, 3), RP(RAMP_VAPE, 4), RP(RAMP_VAPE, 5), RP(RAMP_VAPE, 6));
    ArtMat vapl = M5(RP(RAMP_VAPE, 3), RP(RAMP_VAPE, 4), RP(RAMP_VAPE, 5), RP(RAMP_VAPE, 6), RP(RAMP_VAPE, 7));
    ArtMat dark = M4(RP(RAMP_VAPE, 0), RP(RAMP_VAPE, 1), RP(RAMP_VAPE, 2), RP(RAMP_VAPE, 3));
    ArtMat face = M5(RP(RAMP_VAPE, 1), RP(RAMP_VAPE, 2), PP(PAINT_GRAPE, 1), RP(RAMP_VAPE, 4), RP(RAMP_VAPE, 5));
    ArtMat horn = M5(RP(RAMP_BONE, 1), RP(RAMP_BONE, 2), RP(RAMP_BONE, 3), RP(RAMP_BONE, 4), RP(RAMP_BONE, 6));
    ArtMat gold = MR(RAMP_CANDY, 2, 7);
    ArtMat gem_r = M3(PP(PAINT_CHERRY, 0), PP(PAINT_CHERRY, 1), PP(PAINT_CHERRY, 2));
    ArtMat gem_c = M3(PP(PAINT_CYAN, 0), PP(PAINT_CYAN, 1), PP(PAINT_CYAN, 2));
    ArtMat eyes = M4(PP(PAINT_MAGENTA, 1), PP(PAINT_MAGENTA, 2), RP(RAMP_PASTEL_PINK, 7), PAL_WHITE);
    ArtMat mouth = M3(RP(RAMP_VAPE, 0), RP(RAMP_VAPE, 0), RP(RAMP_BLOOD, 1));
    ArtMat claw = M3(RP(RAMP_VAPE, 0), RP(RAMP_VAPE, 1), RP(RAMP_VAPE, 3));
    ArtMat puff = M4(RP(RAMP_VAPE, 5), RP(RAMP_VAPE, 6), RP(RAMP_VAPE, 7), PAL_WHITE);
    ArtMat flame = M5(RP(RAMP_VAPE, 4), PP(PAINT_MAGENTA, 1), PP(PAINT_MAGENTA, 2), RP(RAMP_PASTEL_PINK, 7), PAL_WHITE);
    int b = (f == 1) ? 1 : 0, sw = (f == 1) ? 2 : -2, k;
    int dead = p.dstage >= 2;
    spr_push();
    spr_scale(Q12 * 82 / 100, Q12 * 82 / 100);       /* ~78 px tall */
    if (p.dstage == 4) {
        /* what remains: a wisp of vapour, the crown and the mod */
        spr_ellipse(-4, -5, 12, 5, &vap, SH_DITHER);
        spr_ellipse(6, -7, 8, 5, &vapl, SH_DITHER | SH_SAMEPART);
        spr_push();
        spr_translate(-2, -12);
        spr_rotate(-300);
        spr_rect(-7, -3, 14, 4, &gold, SH_CYL);
        for (k = -1; k <= 1; k++) spr_tri(k * 5 - 2, -3, k * 5 + 2, -3, k * 5, -8, &gold, SH_SAMEPART);
        spr_ellipse(0, -1, 1, 1, &gem_r, SH_GLOW);
        spr_pop();
        vapula_mod(14, -4, 1024);
        spr_pop();
        finish();
        return;
    }
    spr_push();
    if (p.dstage == 2) spr_scale(Q12 * 80 / 100, Q12 * 80 / 100);
    if (p.dstage == 3) { spr_scale(Q12 * 55 / 100, Q12 * 50 / 100); }
    if (p.pain) spr_rotate(-80);
    /* smoke cape */
    spr_ellipse(0, -52 + b, 30, 30, &dark, SH_DITHER);
    for (k = 0; k < 5; k++)
        spr_bezier(-24 + k * 12, -30, -26 + k * 12 + sw, -18, -22 + k * 11 - sw, -8 + (k & 1) * 4, 5, 1, &dark,
                   SH_DITHER | SH_SAMEPART);
    /* swirling vapour tail down to the ground mist */
    spr_bezier(-8, -32 + b, -2, -16, -14 + sw, -4, 9, 3, &vap, SH_DITHER);
    spr_bezier(8, -32 + b, 12, -14, 4 - sw, -3, 8, 3, &vap, SH_DITHER | SH_SAMEPART);
    spr_ellipse(0, -3, 16, 3, &vap, SH_DITHER);
    /* left arm: holds the mod (raised to the mouth when attacking) */
    spr_ellipse(-21, -58 + b, 9, 8, &vapl, SH_DITHER);
    if (p.attack) {
        spr_bezier(-24, -54, -26, -66, -12, -70, 7, 5, &vapl, SH_DITHER);
        vapula_mod(-12, -68, -700);
        spr_ellipse(-11, -64, 4, 4, &claw, 0);
    } else {
        spr_bezier(-24, -54 + b, -32, -46, -30 + sw / 2, -38, 7, 5, &vapl, SH_DITHER);
        vapula_mod(-33 + sw / 2, -44, 200);
        spr_ellipse(-30 + sw / 2, -38, 4, 4, &claw, 0);
        spr_px(-34 + sw / 2, -36, RP(RAMP_BONE, 6)); spr_px(-27 + sw / 2, -35, RP(RAMP_BONE, 6));
    }
    /* chest of billowing puffs */
    spr_ellipse(0, -48 + b, 17, 15, &vapl, SH_DITHER);
    spr_ellipse(-12, -40 + b, 9, 8, &vapl, SH_DITHER | SH_SAMEPART);
    spr_ellipse(12, -40 + b, 9, 8, &vapl, SH_DITHER | SH_SAMEPART);
    spr_ellipse(0, -36 + b, 10, 7, &vapl, SH_DITHER | SH_SAMEPART);
    /* golden chain + amethyst */
    for (k = -8; k <= 8; k += 2) spr_px(k, -56 + b + (k * k) / 10, RP(RAMP_CANDY, (k & 2) ? 6 : 4));
    spr_ellipse(0, -48 + b, 3, 3, &gem_c, SH_SPEC);
    /* right arm: open claw with a magenta flame */
    spr_ellipse(21, -58 + b, 9, 8, &vapl, SH_DITHER);
    if (p.attack) {
        spr_bezier(24, -54, 34, -60, 34, -72, 7, 5, &vapl, SH_DITHER);
        spr_ellipse(34, -74, 5, 4, &claw, 0);
        part_glow(34, -82, 8, &flame);
        spr_ellipse(34, -82, 4, 5, &flame, SH_GLOW | SH_NOCONTOUR);
    } else {
        spr_bezier(24, -54 + b, 34, -48, 33 - sw / 2, -38, 7, 5, &vapl, SH_DITHER);
        spr_ellipse(33 - sw / 2, -36, 5, 4, &claw, 0);
        for (k = -1; k <= 1; k++) part_fang(33 - sw / 2 + k * 3, -33, 1, 3, 1, RP(RAMP_BONE, 6));
    }
    /* horns */
    part_horn(-8, -80 + b, -14, -6, -6, 4, &horn);
    part_horn(-22, -86 + b, -4, 10, -6, 2, &horn);
    part_horn(8, -80 + b, 14, -6, 6, 4, &horn);
    part_horn(22, -86 + b, 4, 10, 6, 2, &horn);
    /* face */
    spr_ellipse(0, -73 + b, 12, 11, &face, 0);
    spr_ellipse(-9, -69 + b, 4, 4, &vapl, SH_DITHER);
    spr_ellipse(9, -69 + b, 4, 4, &vapl, SH_DITHER);
    if (dead) {
        part_eye_x(-5, -75 + b, 2, RP(RAMP_VAPE, 0));
        part_eye_x(5, -75 + b, 2, RP(RAMP_VAPE, 0));
    } else if (p.pain) {
        part_eye_squeeze(-5, -75 + b, -2, RP(RAMP_VAPE, 0));
        part_eye_squeeze(5, -75 + b, 2, RP(RAMP_VAPE, 0));
    } else {
        spr_ellipse(-5, -75 + b, 3, 2, &eyes, SH_GLOW);
        spr_ellipse(5, -75 + b, 3, 2, &eyes, SH_GLOW);
        spr_px(-5, -75 + b, PAL_WHITE); spr_px(5, -75 + b, PAL_WHITE);
        spr_line(-9, -80 + b, -2, -77 + b, RP(RAMP_VAPE, 0));
        spr_line(9, -80 + b, 2, -77 + b, RP(RAMP_VAPE, 0));
        spr_line(-9, -79 + b, -3, -77 + b, RP(RAMP_VAPE, 0));
        spr_line(9, -79 + b, 3, -77 + b, RP(RAMP_VAPE, 0));
    }
    if (p.attack) {
        spr_ellipse(0, -67, 3, 3, &mouth, SH_FLAT);
        spr_ellipse(-6, -60, 6, 5, &puff, SH_NOCONTOUR);
        spr_ellipse(2, -56, 7, 5, &puff, SH_NOCONTOUR | SH_SAMEPART);
        spr_ellipse(10, -61, 5, 4, &puff, SH_NOCONTOUR | SH_SAMEPART);
    } else if (p.pain || p.dstage == 1 || dead) {
        spr_ellipse(0, -66 + b, 5, 3, &mouth, SH_FLAT);
        fangs_row(-4, 4, -69 + b, PAL_WHITE);
    } else {
        /* cheshire grin */
        spr_ellipse(0, -67 + b, 8, 3, &mouth, SH_FLAT);
        spr_ellipse(0, -69 + b, 9, 2, &face, SH_ONTOP | SH_NOCONTOUR);
        fangs_row(-7, 7, -67 + b, PAL_WHITE);
    }
    spr_bezier(0, -62 + b, -1, -58 + b, -3 + sw / 2, -55 + b, 2, 0, &vap, 0);  /* goatee wisp */
    /* crown */
    {
        int cy = -84 + b, c[16];
        c[0] = -9; c[1] = cy + 3; c[2] = -9; c[3] = cy - 6; c[4] = -5; c[5] = cy - 2;
        c[6] = 0; c[7] = cy - 9; c[8] = 5; c[9] = cy - 2; c[10] = 9; c[11] = cy - 6;
        c[12] = 9; c[13] = cy + 3; c[14] = 0; c[15] = cy + 4;
        if (p.dstage >= 2) {
            spr_push();
            spr_translate(-3, cy - 4);
            spr_rotate(-250);
            spr_translate(0, -(cy - 4));
        }
        spr_poly(c, 8, &gold, SH_SPEC);
        spr_ellipse(0, cy, 2, 2, &gem_r, SH_GLOW);
        spr_ellipse(-6, cy + 1, 1, 1, &gem_c, SH_GLOW);
        spr_ellipse(6, cy + 1, 1, 1, &gem_c, SH_GLOW);
        spr_px(-9, cy - 7, RP(RAMP_CANDY, 7)); spr_px(0, cy - 10, RP(RAMP_CANDY, 7)); spr_px(9, cy - 7, RP(RAMP_CANDY, 7));
        if (p.dstage >= 2) spr_pop();
    }
    /* light beams cracking out when he is destroyed */
    if (p.dstage == 1)
        for (k = 0; k < 6; k++) {
            int a = k * 683 + 200;
            spr_line(0, -55, 40 * art_cos(a) / Q12, -55 + 40 * art_sin(a) / Q12, PAL_WHITE);
        }
    spr_pop();
    spr_pop();
    if (p.dstage == 2) spr_erode(0x7A7Au, 50);
    if (p.dstage == 3) spr_erode(0x7A7Bu, 110);
    spr_clean();
    finish();
}

const ArtSprDef art_spr_demons[] = {
    {SPR_IMPLING, "impling", 8, ANCHOR_GROUND, 0, impling},
    {SPR_VAPE_WRAITH, "vape_wraith", 8, ANCHOR_GROUND, SPRF_FLOATS | SPRF_TRANSLUCENT, vape_wraith},
    {SPR_PUFF_DEMON, "puff_demon", 8, ANCHOR_GROUND, SPRF_FLOATS, puff_demon},
    {SPR_HELLHOUND_PUP, "hellhound_pup", 8, ANCHOR_GROUND, 0, hellhound_pup},
    {SPR_BONE_JESTER, "bone_jester", 8, ANCHOR_GROUND, 0, bone_jester},
    {SPR_SLIME_BELCHER, "slime_belcher", 8, ANCHOR_GROUND, 0, slime_belcher},
    {SPR_GARGOYLE, "gargoyle", 8, ANCHOR_GROUND, SPRF_FLOATS, gargoyle},
    {SPR_CYBER_PIGGY, "cyber_piggy", 8, ANCHOR_GROUND, 0, cyber_piggy},
    {SPR_BRUTE, "brute", 8, ANCHOR_GROUND, 0, brute},
    {SPR_CLOUD_SPIDER, "cloud_spider", 8, ANCHOR_GROUND, 0, cloud_spider},
    {SPR_LORD_VAPULA, "lord_vapula", 8, ANCHOR_GROUND, SPRF_FLOATS, lord_vapula},
};
const int art_spr_demons_n = (int)(sizeof art_spr_demons / sizeof art_spr_demons[0]);
