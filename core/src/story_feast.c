/* story_feast.c - CUT_FEAST_POP: the "What an Ass" outcome of the post-game
 * Feast of Rebuilding (owner: story). Called by cutscene.c (SCENE_CUTSCENE):
 *   feast_tick(panel, pt)        sound effects, once per tick
 *   feast_draw(panel, pt, ct)    the picture (cutscene.c draws the caption)
 *
 * Slapstick, cartoon-modest: Grumbleguts is shown in profile, belly and
 * rump both round and smooth; the hose goes behind the chair and the rest
 * is implied by a squeak and his slowly turning head. CUTE gore setting:
 * confetti, plush stuffing, hearts, a halo. BRUTAL: a red shower and gibs.
 * Panels (cutscene.c): 0 feast, 1 eyes on him, 2 "Look who's full of
 * himself, now?", 3 the hose, 4 burp (false pop 1), 5 fart (false pop 2),
 * 6 both at once, 7 the valve wins, 8 POP, 9 "...that's hot.", 10 title.
 * Three Ren & Stimpy style GROSS-OUT CLOSE-UPS smash-cut in (white flash,
 * hold, record scratch) over panels 4, 5 and 7: the burp, the gas cloud and
 * the straining belly / harness strap (see closeup()). Cartoon only.
 * And the camera will not look away from the swelling belly: angle shots
 * (low angle, push-in, overhead, from behind the crowd, a crowd reaction
 * cutaway) cut in during panels 6 and 7 (see angle_shot()).
 */
#include "story_int.h"

#define F_CUTE  (opt_get(OPT_GORE) != GORE_BRUTAL)
#define F_CULPRIT ID_HATTIE          /* the possessed guest (comedian)   */

static const int16_t crowd_ids[6] = { ID_MAYOR, ID_NANA, ID_FUZZ, ID_BLOOM, ID_MOTHER_NOX, ID_ANVIL };

/* how many pixels bigger than normal Grumbleguts is, per panel and tick */
static int inflate(int panel, uint32_t pt)
{
    int t = (int)pt, v;
    switch (panel) {
    case 0: return t / 25;
    case 1: return 6 + t / 20;
    case 2: case 3: return 14;
    case 4: v = t < 30 ? t * 12 / 30 : 12 - (t - 30) / 10; return 14 + DOOMO_MAX(v, 0);
    case 5: v = t < 40 ? t * 22 / 40 : 22 - (t - 40) / 6; return 16 + DOOMO_MAX(v, 0);
    case 6: if (t < 50) return 14 + t * 30 / 50;                 /* the big one: swells ...   */
            return 44 - DOOMO_MIN((t - 50) * 40 / 10, 40);       /* ... and PFFT, deflates    */
    case 7: return 6 + t * 80 / 150;
    default: return 0;
    }
}

static void gas(int cx, int cy, int r, int sick)
{
    uint8_t c = sick ? PAL_RAMP(RAMP_SICK, 5) : PAL_RAMP(RAMP_MINT, 6);
    ui_fill_circle(cx, cy, r, c);
    ui_fill_circle(cx - r, cy + r / 3, r * 2 / 3, c);
    ui_fill_circle(cx + r, cy + r / 3, r * 2 / 3, c);
    if (!sick) ui_heart(cx + r / 2, cy - r / 3, 2, PAL_UI_PINK);
}

/* Grumbleguts in profile, facing left. (cx,cy) = middle of the belly. */
static void grumble(int cx, int cy, int extra, int shake, uint32_t ct, int eating)
{
    int r = 34 + extra, i, hx = cx - r * 3 / 4, hy = cy - r * 3 / 4;
    int rx = cx + r * 6 / 10, ry = cy + r / 5, rr = r * 7 / 10;
    uint8_t dk = PAL_RAMP(RAMP_BLOOD, 2), md = PAL_UI_RED, lt = PAL_RAMP(RAMP_LAVA, 6);
    cx += shake;
    rx += shake;
    hx += shake;
    /* rump: two round cheeks with a crease, always smooth */
    ui_fill_circle(rx, ry - rr / 3, rr + 1, dk);
    ui_fill_circle(rx, ry + rr / 3, rr + 1, dk);
    ui_fill_circle(rx, ry - rr / 3, rr, md);
    ui_fill_circle(rx, ry + rr / 3, rr, md);
    plat_line(rx - rr / 3, ry, rx + rr, ry, dk);
    ui_fill_circle(rx + rr / 4, ry - rr / 2, DOOMO_MAX(rr / 5, 2), lt);
    /* belly */
    ui_fill_circle(cx, cy, r + 1, dk);
    ui_fill_circle(cx, cy, r, md);
    ui_fill_circle(cx - r / 3, cy - r / 3, r / 4, lt);
    /* harness + studded belt (they ping off when he is huge) */
    if (extra < 60) {
        plat_line(cx - r / 2, cy - r + 4, cx + r / 3, cy + r - 4, PAL_BLACK);
        plat_line(cx - r / 2 + 1, cy - r + 4, cx + r / 3 + 1, cy + r - 4, PAL_BLACK);
    }
    if (extra < 75)
        for (i = -4; i <= 4; i++) plat_rect(cx + i * r / 5, cy + r / 3 + (i * i) / 4, 2, 2, PAL_UI_GOLD);
    /* head, horns, grin */
    ui_fill_circle(hx, hy, 15, dk);
    ui_fill_circle(hx, hy, 14, md);
    plat_line(hx - 11, hy - 10, hx - 16, hy - 24, PAL_RAMP(RAMP_BONE, 6));
    plat_line(hx + 4, hy - 13, hx + 8, hy - 26, PAL_RAMP(RAMP_BONE, 6));
    ui_fill_circle(hx - 7, hy - 3, 3, PAL_WHITE);
    plat_rect(hx - 8, hy - 3, 2, 2, PAL_BLACK);
    plat_rect(hx - 12, hy + 4 + (eating ? (int)((ct / 3) & 1) * 2 : 0), 10, 3, PAL_BLACK);
    plat_rect(hx - 11, hy + 4, 2, 2, PAL_WHITE);
    plat_rect(hx - 6, hy + 4, 2, 2, PAL_WHITE);
}

static void bunting(int sw)
{
    int i;
    plat_line(0, 18, sw, 26, PAL_RAMP(RAMP_WOOD, 3));
    for (i = 0; i < sw; i += 22) {
        int y = 18 + i * 8 / sw;
        plat_rect(i, y, 8, 8, PAL_PAINT((i / 22 * 5) & 31, 1));
    }
}

static void crowd(int sw, int gy, int panel, uint32_t ct, int turned)
{
    int i;
    for (i = 0; i < 6; i++) {
        const DoomoImage *b = bodyspr_image(crowd_ids[i], turned ? 1 : 2, (int)((ct / 8 + (uint32_t)i) & (panel >= 7 ? 3 : 0)));
        int hop = panel >= 7 ? ui_wave((int)ct + i * 9, 10, 2) : 0;
        if (b) plat_image(sw - 150 + i * 22, gy - b->h + 8 + (i & 1) * 6 + hop, b, 1, 0, 0);
    }
}



/* A raised arm with a big tuft of armpit hair: coarse strands, a few curly
 * long ones, sweat drips and stink lines (hearts and sparkles on CUTE, sulphur
 * and a fly on BRUTAL). (px,py) is the pit; the arm goes up and to the right
 * when dir > 0, up and to the left when dir < 0. */
static void pit(int px, int py, int size, int dir, uint32_t ct, int cute)
{
    int i, j;
    for (i = 0; i <= 8; i++) {                                   /* the upper arm */
        int ax = px + dir * i * size / 5, ay = py - i * size / 4;
        ui_fill_circle(ax, ay, size / 2 + 3 - i / 4, PAL_RAMP(RAMP_BLOOD, 2));
    }
    for (i = 0; i <= 8; i++) {
        int ax = px + dir * i * size / 5, ay = py - i * size / 4;
        ui_fill_circle(ax, ay, size / 2 + 1 - i / 4, PAL_UI_RED);
    }
    ui_fill_ellipse(px, py + 3, size / 2, size / 4, PAL_RAMP(RAMP_BLOOD, 1));    /* the shadowed crease */
    for (j = 0; j < 16; j++) {                                   /* the hair, thick and shaggy */
        int a = 1000 + j * 72, len = size + (j * 7 % 5) * 3 + (j & 1 ? 4 : 0);
        int ex = px + FIX2INT(fx_cos(a) * len), ey = py + FIX2INT(fx_sin(a) * len) / 2 + 4;
        uint8_t hc = (j & 1) ? PAL_BLACK : PAL_RAMP(RAMP_WOOD, 1);
        plat_line(px, py, ex, ey, hc);
        plat_line(px + 1, py, ex + 1, ey, hc);
        if ((j % 3) == 0) {                                      /* a curl at the end of the longer ones */
            plat_line(ex, ey, ex + (j & 2 ? 4 : -4), ey + 3, hc);
            plat_rect(ex + (j & 2 ? 4 : -5), ey + 3, 2, 2, hc);
        }
    }
    {                                                            /* sweat drips off the hair */
        int d = (int)(ct % 24u);
        ui_fill_ellipse(px - size / 2, py + size / 2 + 4 + d, 2, 3, PAL_RAMP(RAMP_SKY, 7));
        ui_fill_ellipse(px + size / 3, py + size / 2 + 2 + (d + 12) % 24, 2, 3, PAL_RAMP(RAMP_SKY, 7));
    }
    for (i = 0; i < 3; i++) {                                    /* the smell, rising */
        int sx = px - size / 2 + i * size / 2, sy = py + size + 12 - (int)((ct * 2 + (uint32_t)i * 9) % 30u);
        if (cute) { if (i & 1) ui_heart(sx, sy, 2, PAL_UI_PINK); else ui_sparkle(sx, sy, (int)((ct / 2 + (uint32_t)i) & 15), PAL_WHITE); }
        else { plat_line(sx, sy, sx + ui_wave((int)ct + i * 5, 8, 3), sy + 8, PAL_RAMP(RAMP_SICK, 5)); }
    }
    if (!cute) {                                                 /* one fly, circling */
        int fx = px + FIX2INT(fx_cos((int)ct * 90) * 20), fy = py + size + FIX2INT(fx_sin((int)ct * 90) * 10);
        plat_rect(fx, fy, 3, 2, PAL_BLACK);
        plat_rect(fx - 1, fy - 2, 2, 2, PAL_WHITE);
        plat_rect(fx + 2, fy - 2, 2, 2, PAL_WHITE);
    }
}

/* ---- gross-out close-ups: smash cut + hold + record scratch ------------------ */
#define CU_BURP_A 30u
#define CU_BURP_B 64u      /* panel 4 */
#define CU_GAS_A  62u
#define CU_GAS_B  94u      /* panel 5 */
#define CU_BELLY_A 90u
#define CU_BELLY_B 126u    /* panel 7 */

/* returns 1 when it drew a full-screen close-up instead of the scene */
static int closeup(int panel, uint32_t pt, uint32_t ct)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, cy = sh / 2, i;
    int cute = F_CUTE;
    uint32_t t0;

    if (panel == 4 && pt >= CU_BURP_A && pt < CU_BURP_B) t0 = pt - CU_BURP_A;
    else if (panel == 5 && pt >= CU_GAS_A && pt < CU_GAS_B) t0 = pt - CU_GAS_A;
    else if (panel == 7 && pt >= CU_BELLY_A && pt < CU_BELLY_B) t0 = pt - CU_BELLY_A;
    else return 0;
    if (t0 < 2) { plat_rect(0, 0, sw, sh, PAL_WHITE); return 1; }      /* the smash cut */

    if (panel == 4) {
        /* his face mid-burp: huge open mouth, wobbling uvula, bagel bits */
        int mw = sw / 3 + (int)DOOMO_MIN(t0, 10u), mh = sh / 4 + (int)DOOMO_MIN(t0, 10u) / 2;
        int w = ui_wave((int)ct, 6, 3);
        plat_rect(0, 0, sw, sh, PAL_UI_RED);
        ui_fill_circle(cx - sw / 3, cy - sh / 5, sh / 6, PAL_RAMP(RAMP_BLOOD, 2));          /* puffed cheeks */
        ui_fill_circle(cx + sw / 3, cy - sh / 5, sh / 6, PAL_RAMP(RAMP_BLOOD, 2));
        ui_fill_ellipse(cx, cy, mw + 3, mh + 3, PAL_BLACK);
        ui_fill_ellipse(cx, cy, mw, mh, PAL_RAMP(RAMP_BLOOD, 0));
        ui_fill_ellipse(cx, cy + mh * 2 / 3, mw * 2 / 3, mh / 3, PAL_RAMP(RAMP_FLESH, 5));  /* tongue */
        for (i = -3; i <= 3; i++) {                                                          /* fangs */
            int fx = cx + i * mw / 4;
            plat_rect(fx - 2, cy - mh + 4 + (i * i) / 3, 5, 9 + (i & 1) * 6, PAL_WHITE);
        }
        plat_rect(cx - 2, cy - mh + 8, 5, mh / 2 + w, PAL_RAMP(RAMP_FLESH, 6));              /* uvula wobbling */
        ui_fill_circle(cx, cy - mh + 8 + mh / 2 + w, 5, PAL_RAMP(RAMP_FLESH, 7));
        for (i = 0; i < 9; i++) {                                                            /* bagel bits */
            int a = i * 227 + 100, rr = (int)t0 * (3 + i % 4) + mh / 2;
            int bx = cx + FIX2INT(fx_cos(a) * rr), by = cy + FIX2INT(fx_sin(a) * rr) / 2 + (int)t0 * (int)t0 / 30;
            ui_fill_circle(bx, by, 4, PAL_RAMP(RAMP_WOOD, 6));
            ui_fill_circle(bx, by, 1, PAL_RAMP(RAMP_WOOD, 1));
        }
        pit(sw - 52, 56, 26, 1, ct, cute);                                                  /* arm up, pit on show */
        ui_text_outline(cx - 12, sh - 40, "BUUURP", PAL_UI_GOLD, PAL_BLACK);
    } else if (panel == 5) {
        /* the gas cloud, extreme close-up: every fly, one bead of sweat */
        plat_rect(0, 0, sw, sh, cute ? PAL_RAMP(RAMP_PASTEL_PINK, 6) : PAL_RAMP(RAMP_SICK, 2));
        for (i = 0; i < 7; i++) {
            int bx = cx - sw / 3 + i * sw / 9 + ui_wave((int)ct + i * 7, 30, 4);
            int by = cy - 6 + (i % 3) * 10 - ui_wave((int)ct + i * 5, 24, 4);
            ui_fill_circle(bx, by, sh / 6 + (i & 1) * 6, cute ? PAL_RAMP(RAMP_MINT, 5) : PAL_RAMP(RAMP_SICK, 6));
            ui_fill_circle(bx - 6, by - 8, sh / 12, cute ? PAL_RAMP(RAMP_MINT, 7) : PAL_RAMP(RAMP_SICK, 7));
        }
        for (i = 0; i < 16; i++) {
            int a = i * 163 + (int)ct * (4 + i % 3), rr = 36 + (i * 11) % 60;
            int fx = cx + FIX2INT(fx_cos(a) * rr), fy = cy + FIX2INT(fx_sin(a) * rr) / 2;
            if (cute) ui_sparkle(fx, fy, (int)((ct / 2 + (uint32_t)i) & 15), PAL_WHITE);
            else {                                                       /* a fly, wings and all */
                plat_rect(fx, fy, 3, 2, PAL_BLACK);
                plat_rect(fx - 1, fy - 2, 2, 2, PAL_WHITE);
                plat_rect(fx + 2, fy - 2, 2, 2, PAL_WHITE);
            }
        }
        if (cute) for (i = 0; i < 4; i++) ui_heart(cx - 40 + i * 28, cy - 20 + (i & 1) * 22, 5, PAL_UI_PINK);
        ui_fill_circle(sw - 34, sh - 34, 20, PAL_RAMP(RAMP_FLESH, 6));                      /* the culprit's nose */
        ui_fill_ellipse(sw - 34, sh - 36 + (int)DOOMO_MIN(t0, 24u) / 4, 3, 5, PAL_RAMP(RAMP_SKY, 7));   /* sweat bead */
        plat_rect(sw - 35, sh - 40 + (int)DOOMO_MIN(t0, 24u) / 4, 1, 2, PAL_WHITE);
    } else {
        /* the belly skin stretched tight: veins, a stitch-seam about to give,
         * and the harness strap going fibre by fibre */
        int gap = 1 + (int)t0 / 4, snap = (int)t0 * 14 / 36;
        plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_FLESH, 3));
        ui_fill_ellipse(cx, cy, sw / 2 + 8, sh / 2 + 6, PAL_UI_RED);
        ui_fill_ellipse(cx - 20, cy - 18, sw / 4, sh / 6, PAL_RAMP(RAMP_LAVA, 6));          /* shine */
        for (i = 0; i < 9; i++) {                                                            /* veins, pulsing */
            int vx = 20 + i * (sw - 40) / 8, vy = 14 + ((i * 37) % 50), p = ui_wave((int)ct + i * 6, 14, 1);
            plat_line(vx, vy, vx + 14 + p, vy + 24, PAL_RAMP(RAMP_VAPE, 2));
            plat_line(vx + 14 + p, vy + 24, vx + 6, vy + 46, PAL_RAMP(RAMP_VAPE, 2));
            plat_line(vx + 14 + p, vy + 24, vx + 28, vy + 38, PAL_RAMP(RAMP_VAPE, 2));
        }
        pit(46, 64, 28, -1, ct, cute);                                                       /* a flexed arm: armpit hair */
        plat_rect(cx - gap, 0, gap * 2, sh, PAL_BLACK);                                      /* the seam */
        for (i = 0; i < sh; i += 10)                                                         /* cross-stitches */
            if ((i / 10) >= (int)t0 / 6) {                                                   /* popped from the top down */
                plat_line(cx - gap - 6, i, cx + gap + 6, i + 8, PAL_RAMP(RAMP_BONE, 5));
                plat_line(cx - gap - 6, i + 8, cx + gap + 6, i, PAL_RAMP(RAMP_BONE, 5));
            }
        plat_rect(0, cy + 24, sw, 22, PAL_BLACK);                                            /* the strap */
        for (i = 0; i < 14; i++) {                                                           /* its fibres */
            int fy = cy + 25 + i * 21 / 14, broke = i < snap;
            plat_rect(0, fy, broke ? cx - 30 - i : sw, 1, PAL_RAMP(RAMP_STEEL, 2 + (i & 3)));
            if (broke) plat_rect(cx + 30 + i * 2, fy + ((int)ct & 1) * 2, sw - cx - 30 - i * 2, 1, PAL_RAMP(RAMP_STEEL, 2 + (i & 3)));
        }
        if (t0 > 20) ui_text_outline(cx - 14, 6, "creeeak", PAL_WHITE, PAL_BLACK);
    }
    return 1;
}


/* ---- camera angle shots on the swelling belly ---------------------------------------
 * Hard cuts (whip-flash, letterbox bars, a "CAM" tag) between angles while he
 * swells: panel 6 gets a crowd reaction cutaway, panel 7 (the valve) cycles
 * low angle -> push-in -> overhead -> (belly close-up) -> from behind the crowd. */
enum { AN_LOW, AN_PUSH, AN_TOP, AN_BEHIND, AN_REACT };
typedef struct { uint8_t panel, a, b, kind; const char *name; } AngleShot;
static const AngleShot shots[] = {
    { 6, 70, 104, AN_REACT,  "CAM 3  REACTION" },
    { 7, 20,  44, AN_LOW,    "CAM 2  LOW ANGLE" },
    { 7, 44,  68, AN_PUSH,   "CAM 1  PUSH IN" },
    { 7, 68,  90, AN_TOP,    "CAM 4  OVERHEAD" },
    { 7, 126, 150, AN_BEHIND, "CAM 5  BEHIND THE CROWD" }
};
#define NSHOTS ((int)(sizeof shots / sizeof shots[0]))

static int angle_start(int panel, uint32_t pt)     /* a cut happens this tick */
{
    int i;
    for (i = 0; i < NSHOTS; i++)
        if (shots[i].panel == panel && (pt == shots[i].a || pt == shots[i].b)) return 1;
    return 0;
}

static void crowd_big(int sw, int gy, uint32_t ct, uint32_t t0, int scale, int cheer)
{
    int i;
    for (i = 0; i < 4; i++) {
        const DoomoImage *b = bodyspr_image(crowd_ids[i], 2, (int)((ct / 5 + (uint32_t)i) & 3));
        int hop = cheer ? ui_wave((int)ct * 2 + i * 11, 8, 6) : 0;
        if (b) plat_image(sw / 2 - 2 * 28 * scale + i * 28 * scale + 4, gy - b->h * scale + 6 - (hop < 0 ? -hop : hop), b, scale, IMG_SHADOW, 0);
        if (cheer && ((t0 / 4 + (uint32_t)i) & 3) == 0) ui_sparkle(sw / 2 - 60 + i * 40, gy - 90 - (i & 1) * 12, (int)((t0 + (uint32_t)i * 3) & 15), PAL_UI_GOLD);
    }
}

/* returns 1 when it drew a whole angle shot instead of the wide shot */
static int angle_shot(int panel, uint32_t pt, uint32_t ct, int extra)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, i, k = -1, mid;
    uint32_t t0 = 0;
    for (i = 0; i < NSHOTS; i++)
        if (shots[i].panel == panel && pt >= shots[i].a && pt < shots[i].b) { k = i; t0 = pt - shots[i].a; }
    if (k < 0) return 0;
    if (t0 < 2) { plat_rect(0, 0, sw, sh, PAL_WHITE); return 1; }       /* whip cut */
    mid = sh / 2;

    switch (shots[k].kind) {
    case AN_LOW:                                         /* from the grass, looking up: the belly looms */
        plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_SKY, 6));
        bunting(sw);
        plat_rect(0, sh - 44, sw, 44, PAL_RAMP(RAMP_GRASS, 4));
        grumble(cx + 6, mid + 10, extra + 34 + (int)t0 / 3, 0, ct, 0);
        for (i = 0; i < 7; i++) {                         /* tiny awed crowd at the bottom edge */
            int hx = 26 + i * (sw - 52) / 6;
            ui_fill_circle(hx, sh - 22, 7, PAL_PAINT((i * 5 + 2) & 31, 1));
            plat_rect(hx - 6, sh - 16, 12, 6, PAL_PAINT((i * 5 + 2) & 31, 0));
            plat_rect(hx - 3, sh - 25, 2, 2, PAL_WHITE);
            plat_rect(hx + 2, sh - 25, 2, 2, PAL_WHITE);
        }
        break;
    case AN_PUSH: {                                      /* dolly in on the straining belly */
        int r = (int)t0 * 3;
        plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_SKY, 6));
        plat_rect(0, sh - 50, sw, 50, PAL_RAMP(RAMP_GRASS, 4));
        for (i = 0; i < 10; i++) {                        /* speed lines rushing past the edges */
            int y = (i * 53 + (int)ct * 9) % sh;
            plat_rect(0, y, 14 + r / 6, 1, PAL_WHITE);
            plat_rect(sw - 14 - r / 6, (y + 70) % sh, 14 + r / 6, 1, PAL_WHITE);
        }
        grumble(cx + 10 - r / 4, mid + 24, extra + 8 + r / 2, ((int)ct & 1) ? 1 : -1, ct, 0);
        break;
    }
    case AN_TOP: {                                       /* straight down: a ring of guests round a round gut */
        int ocx = cx - 24, ocy = mid, r = 22 + extra * 3 / 5, a, hr = r + 34;
        plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_GRASS, 4));
        plat_rect(8, sh - 40, 54, 22, PAL_RAMP(RAMP_WOOD, 4));                          /* buffet table, from above */
        ui_fill_circle(ocx, ocy, r + 8, PAL_RAMP(RAMP_WOOD, 3));                        /* the chair under him */
        for (i = 0; i < 14; i++) {                                                      /* crowd: heads + shoulders */
            a = i * 292 + 60;
            ui_fill_ellipse(ocx + FIX2INT(fx_cos(a) * (hr + 8)), ocy + FIX2INT(fx_sin(a) * (hr + 8)) * 3 / 4, 9, 6, PAL_PAINT((i * 5 + 1) & 31, 0));
            ui_fill_circle(ocx + FIX2INT(fx_cos(a) * hr), ocy + FIX2INT(fx_sin(a) * hr) * 3 / 4, 6, PAL_PAINT((i * 5 + 1) & 31, 1));
        }
        ui_fill_circle(ocx + r * 9 / 10, ocy - r / 3, r / 2 + 1, PAL_RAMP(RAMP_BLOOD, 2));      /* rump (top view) */
        ui_fill_circle(ocx + r * 9 / 10, ocy + r / 3, r / 2 + 1, PAL_RAMP(RAMP_BLOOD, 2));
        ui_fill_circle(ocx + r * 9 / 10, ocy - r / 3, r / 2, PAL_UI_RED);
        ui_fill_circle(ocx + r * 9 / 10, ocy + r / 3, r / 2, PAL_UI_RED);
        ui_fill_circle(ocx, ocy, r + 1, PAL_RAMP(RAMP_BLOOD, 2));                        /* belly */
        ui_fill_circle(ocx, ocy, r, PAL_UI_RED);
        ui_fill_circle(ocx - r / 3, ocy - r / 3, r / 5, PAL_RAMP(RAMP_LAVA, 6));
        plat_line(ocx - r, ocy - r / 3, ocx + r, ocy + r / 3, PAL_BLACK);                /* harness */
        ui_fill_circle(ocx - r - 8, ocy, 10, PAL_UI_RED);                                /* head, horns out */
        plat_line(ocx - r - 8, ocy - 9, ocx - r - 14, ocy - 20, PAL_RAMP(RAMP_BONE, 6));
        plat_line(ocx - r - 8, ocy + 9, ocx - r - 14, ocy + 20, PAL_RAMP(RAMP_BONE, 6));
        {   /* the valve being cranked, and the culprit, from above */
            int vx = ocx + r + 28, vy = ocy + r / 2, an = (int)ct * 48;
            ui_fill_circle(vx, vy, 4, PAL_RAMP(RAMP_STEEL, 3));
            plat_line(vx, vy, vx + FIX2INT(fx_cos(an) * 14), vy + FIX2INT(fx_sin(an) * 14), PAL_RAMP(RAMP_STEEL, 6));
            plat_line(vx, vy, vx - FIX2INT(fx_cos(an) * 14), vy - FIX2INT(fx_sin(an) * 14), PAL_RAMP(RAMP_STEEL, 6));
            ui_fill_circle(vx + 20, vy + 10, 7, PAL_PAINT(9, 1));
        }
        break;
    }
    case AN_BEHIND: {                                    /* low, from behind the crowd: backs of heads, the rump huge */
        int rr = 28 + extra / 2, by = mid + 8;
        plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_SKY, 6));
        bunting(sw);
        plat_rect(0, sh * 2 / 3, sw, sh / 3, PAL_RAMP(RAMP_GRASS, 4));
        ui_fill_circle(cx - rr * 2 / 3, by, rr + 1, PAL_RAMP(RAMP_BLOOD, 2));            /* two smooth cheeks */
        ui_fill_circle(cx + rr * 2 / 3, by, rr + 1, PAL_RAMP(RAMP_BLOOD, 2));
        ui_fill_circle(cx - rr * 2 / 3, by, rr, PAL_UI_RED);
        ui_fill_circle(cx + rr * 2 / 3, by, rr, PAL_UI_RED);
        plat_rect(cx, by - rr / 2, 1, rr + rr / 2, PAL_RAMP(RAMP_BLOOD, 2));
        plat_rect(cx - rr * 3 / 2, by - rr - 6, rr * 3, 5, PAL_BLACK);                   /* studded belt + back harness */
        for (i = -4; i <= 4; i++) plat_rect(cx + i * rr / 3, by - rr - 5, 2, 2, PAL_UI_GOLD);
        plat_line(cx - rr, by - rr - 40, cx + rr / 2, by - rr - 6, PAL_BLACK);
        plat_line(cx + rr, by - rr - 40, cx - rr / 2, by - rr - 6, PAL_BLACK);
        ui_fill_circle(cx, by - rr - 50, 13, PAL_UI_RED);                                /* head, from behind: horns up */
        plat_line(cx - 8, by - rr - 58, cx - 13, by - rr - 72, PAL_RAMP(RAMP_BONE, 6));
        plat_line(cx + 8, by - rr - 58, cx + 13, by - rr - 72, PAL_RAMP(RAMP_BONE, 6));
        plat_line(cx + rr * 2, by + rr / 2, cx + rr * 2 + 26, by + rr / 2 - 14 + ui_wave((int)ct, 20, 4), PAL_RAMP(RAMP_BLOOD, 3));   /* tail */
        for (i = 0; i < 6; i++) {                         /* foreground: the crowd's backs, every head turned his way */
            int hx = 22 + i * (sw - 44) / 5, hy = sh - 36 - (i & 1) * 8;
            ui_fill_circle(hx, hy, 15, PAL_BLACK);
            ui_fill_ellipse(hx, hy + 24, 24, 16, PAL_BLACK);
            if (((t0 / 5 + (uint32_t)i) & 3) == 0) ui_text_outline(hx - 3, hy - 32, "!", PAL_UI_GOLD, PAL_BLACK);
        }
        break;
    }
    default:                                             /* cutaway: the crowd, thinking it's over */
        plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_PASTEL_PINK, 6));
        bunting(sw);
        plat_rect(0, sh * 3 / 4, sw, sh / 4, PAL_RAMP(RAMP_GRASS, 4));
        crowd_big(sw, sh * 3 / 4 + 20, ct, t0, 2, 1);
        for (i = 0; i < 5; i++) ui_heart(36 + i * 56, 40 + (i & 1) * 18 - ui_wave((int)ct + i * 7, 20, 4), 4, PAL_UI_PINK);
        ui_text_outline(sw / 2 - 18, 24, "YAAAY!", PAL_UI_GOLD, PAL_BLACK);
        break;
    }
    /* letterbox bars + camera tag */
    plat_rect(0, 0, sw, 12, PAL_BLACK);
    plat_rect(0, sh - 12, sw, 12, PAL_BLACK);
    plat_text(14, 2, shots[k].name, PAL_WHITE);
    if ((ct / 12) & 1) ui_fill_circle(7, 6, 3, PAL_UI_RED);
    return 1;
}

void feast_tick(int panel, uint32_t pt)
{
    if (angle_start(panel, pt)) audio_sfx(SFX_BLIP);                  /* a cut */
    switch (panel) {
    case 0: if ((pt % 24) == 6) audio_sfx(SFX_EAT); break;
    case 2: if (pt == 40) audio_sfx(SFX_ERROR); break;                 /* record scratch */
    case 3:
        if (pt == 20 || pt == 70 || pt == 120) audio_sfx(SFX_SQUEAK);
        if (pt == 150) audio_sfx(SFX_SPLAT);                            /* the SLAP       */
        break;
    case 4: if (pt == CU_BURP_A) { audio_sfx(SFX_ERROR); audio_sfx(SFX_POOP); } break;   /* burp           */
    case 5: if (pt == 40) audio_sfx(SFX_POOP);
            if (pt == CU_GAS_A) audio_sfx(SFX_ERROR);
            break;
    case 6: if (pt == 50) { audio_sfx(SFX_POOP); audio_sfx(SFX_SPLASH); } break;
    case 7: if ((pt % 30) == 0) audio_sfx(SFX_SQUEAK);
            if (pt == CU_BELLY_A) audio_sfx(SFX_ERROR);
            if (pt > CU_BELLY_A && pt < CU_BELLY_B && (pt % 6) == 0) audio_sfx(SFX_SQUEAK);
            break;
    case 8: if (pt == 4) { audio_sfx(SFX_EXPLODE); audio_sfx(F_CUTE ? SFX_CONFETTI : SFX_SPLAT); }
            if (pt == 70) audio_sfx(SFX_EAT);                          /* last gurgle    */
            break;
    case 9: if (pt == 24) audio_sfx(SFX_ERROR); if (pt == 30) audio_sfx(SFX_SPLAT); break;
    case 10: if (pt == 4) audio_sfx(SFX_BELL); break;
    default: break;
    }
}

void feast_draw(int panel, uint32_t pt, uint32_t ct)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), gy = sh * 3 / 4, i;
    int cx = sw / 2 - 20, cy = gy - 56, extra = inflate(panel, pt), shake = 0;
    int cute = F_CUTE;

    if (panel == 10) {                                   /* title card, then the memorial */
        plat_rect(0, 0, sw, sh, PAL_BLACK);
        if (pt > 10) {
            int sc = sh >= 200 ? 3 : 2;
            ui_text_big((sw - ui_text_big_width("What an Ass", sc)) / 2, sh / 3 - 12, "What an Ass", sc,
                        PAL_UI_GOLD, PAL_RAMP(RAMP_BLOOD, 2));
        }
        if (pt > 50) {                                   /* oversized headstone + plaque */
            int hx = sw / 2 - 40, hy = sh / 2 + 6;
            ui_round_rect(hx, hy, 80, 56, 18, PAL_RAMP(RAMP_STONE, 4));
            ui_round_rect(hx + 3, hy + 3, 74, 53, 16, PAL_RAMP(RAMP_STONE, 5));
            plat_rect(hx + 12, hy + 34, 56, 12, PAL_RAMP(RAMP_STEEL, 3));
            ui_heart(hx + 40, hy + 18, 6, PAL_UI_PINK);
            ui_fill_ellipse(hx + 40, hy - 6 + ui_wave((int)ct, 40, 2), 14, 3, PAL_UI_GOLD);   /* halo */
        }
        return;
    }

    if (closeup(panel, pt, ct)) return;                  /* smash-cut gross-out close-ups */
    if (angle_shot(panel, pt, ct, extra)) return;        /* camera angles on the swelling belly */

    /* park: bunting, sky, grass, buffet table */
    plat_rect(0, 0, sw, gy, PAL_RAMP(RAMP_SKY, 6));
    plat_rect(0, 0, sw, 18, PAL_RAMP(RAMP_PASTEL_PINK, 6));
    plat_rect(0, gy, sw, sh - gy, PAL_RAMP(RAMP_GRASS, 4));
    bunting(sw);
    plat_rect(8, gy - 26, 70, 6, PAL_RAMP(RAMP_WOOD, 4));
    plat_rect(12, gy - 20, 4, 22, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(68, gy - 20, 4, 22, PAL_RAMP(RAMP_WOOD, 2));
    if (panel < 2) for (i = 0; i < 4; i++)                       /* the buffet is losing */
        ui_fill_circle(20 + i * 15, gy - 32 + i % 2 * 2, 6 - (int)DOOMO_MIN((ct + (uint32_t)i * 40) / 60, 5u), PAL_PAINT(i * 7 + 3, 1));

    if (panel == 3) {                                    /* hose coil shrinking on the ground */
        int coil = 18 - (int)DOOMO_MIN(pt / 10, 15u);
        for (i = 0; i < coil / 3 + 1; i++)
            ui_fill_ellipse(sw - 48, gy + 12 - i * 3, 18, 5, PAL_RAMP(RAMP_MINT, 3 + (i & 1)));
    }

    /* the chair (gives way in the big inflation) */
    if (panel < 8) {
        int tilt = (panel == 7 && pt > 90) ? (int)(pt - 90) / 6 : 0;
        plat_rect(cx - 20, gy - 10 + tilt, 8, 14 - tilt, PAL_RAMP(RAMP_WOOD, 2));
        plat_rect(cx + 22, gy - 10 + tilt, 8, 14 - tilt, PAL_RAMP(RAMP_WOOD, 2));
        plat_rect(cx - 24, gy - 14 + tilt, 60, 5, PAL_RAMP(RAMP_WOOD, 4));
    }

    /* Grumbleguts (panel 8: he is gone, only the pieces are left) */
    if (panel == 7) shake = (int)((ct * 5) % 5) - 2 + (int)pt / 40;
    if (panel == 6 && pt >= 40 && pt < 50) shake = ((int)ct & 1) ? 3 : -3;
    if (panel == 5 && pt < 40) shake = (int)(pt / 8) & 1 ? 1 : -1;
    if (panel == 5 && pt >= 40) cy -= 6;                                   /* lifts an inch  */
    if (panel == 7 && pt > 60) cy -= (int)(pt - 60) / 10;                  /* parade balloon */
    if (panel < 8) grumble(cx, cy - (panel == 6 && pt < 50 ? (int)pt / 8 : 0), extra, shake, ct, panel < 2);

    /* the culprit, thumb pointing at the rump, and the crowd's eye-lines */
    if (panel < 8) {
        const DoomoImage *c = bodyspr_image(F_CULPRIT, 1, panel == 3 ? (int)((pt / 6) & 3) : 0);
        int px = cx + 34 + extra / 2 + (panel == 3 ? 12 : 0);
        if (c) plat_image(px, gy - c->h + 12, c, 1, IMG_SHADOW, 0);
    }
    if (panel < 9) crowd(sw, gy, panel, ct, panel >= 6);

    /* per-panel effects */
    if (panel == 4 && pt >= 30) gas(cx - 52, cy - 36 - (int)(pt - 30) / 3, 5 + (int)(pt - 30) / 5, 0);
    if (panel == 5 && pt >= 40) gas(cx + 70, cy + 40, 6 + (int)(pt - 40) / 6, 0);
    if (panel == 6 && pt >= 50) {
        gas(cx - 56, cy - 30 - (int)(pt - 50) / 3, 8 + (int)(pt - 50) / 4, 0);
        gas(cx + 70, cy + 40, 8 + (int)(pt - 50) / 4, 0);
        if (!cute) for (i = 0; i < 6; i++) plat_rect(cx + 60 + i * 5, cy + 30 - (int)(pt - 50) / 2 + i * 3, 2, 2, PAL_BLACK);   /* flies */
    }
    if (panel == 3 && pt > 140 && pt < 190) {                            /* SLAP! pop-in */
        int s = ui_pop_scale((int)pt - 140);
        if (s > 0) ui_text_big(cx + 40, cy - 52, "SLAP!", 2, PAL_WHITE, PAL_UI_RED_D);
    }
    if (panel == 7 && pt > 40) ui_text_outline(cx - 34, cy - 70 - extra / 2, "...gurgle", PAL_WHITE, PAL_BLACK);
    if (panel == 7 && pt > 120) {                                        /* the pressure gauge */
        ui_fill_circle(sw - 30, 44, 12, PAL_WHITE);
        plat_line(sw - 30, 44, sw - 30 + (int)DOOMO_MIN(pt - 120, 28u) / 2 - 2, 44 - 8, PAL_UI_RED);
        ui_text_outline(sw - 44, 60, "MAX", PAL_UI_RED, PAL_BLACK);
    }

    if (panel == 8) {                                    /* POP: flash, pieces, the belly lands last */
        int t = (int)pt;
        if (t < 6) plat_rect(0, 0, sw, sh, PAL_WHITE);
        for (i = 0; i < 40; i++) {
            int a = i * 163 + 40, rad = DOOMO_MIN(t * (2 + i % 5), 110);
            int x = cx + FIX2INT(fx_cos(a) * rad), y = cy - 30 + FIX2INT(fx_sin(a) * rad) + t * t / 40;
            if (cute) {
                if (i % 5 == 0) ui_heart(x, y, 3, PAL_UI_PINK);
                else if (i % 3 == 0) plat_rect(x, y, 5, 4, PAL_RAMP(RAMP_BONE, 6));          /* plush stuffing */
                else plat_rect(x, y, 3, 3, PAL_PAINT(i & 31, 2));                              /* confetti       */
            } else {
                plat_rect(x, y, 3 + i % 3, 3 + i % 2, i % 4 ? PAL_RAMP(RAMP_BLOOD, 3 + i % 3) : PAL_RAMP(RAMP_FLESH, 3));
            }
        }
        if (!cute) plat_checker(0, 0, sw, sh * 2 / 3, PAL_RAMP(RAMP_BLOOD, 3));
        /* the rump is the last thing to go: it drifts down (cute) / lands (brutal) */
        {
            int ly = cute ? cy - 50 + t * 3 / 4 : DOOMO_MIN(cy - 50 + t * t / 24, gy - 14);
            ui_fill_circle(cx + 10, DOOMO_MIN(ly, gy - 12), 12, PAL_RAMP(RAMP_BLOOD, 2));
            ui_fill_circle(cx + 10, DOOMO_MIN(ly, gy - 12), 11, PAL_UI_RED);
            if (cute) ui_text_outline(cx - 6, DOOMO_MIN(ly, gy - 12) - 22, "pbbbbt~", PAL_WHITE, PAL_BLACK);
        }
        if (cute) ui_fill_ellipse(cx - 40, cy - 70 + ui_wave((int)ct, 30, 3), 14, 3, PAL_UI_GOLD);   /* his halo */
        if (t > 60) ui_text_outline(cx - 50, gy - 30, "gurgle...", PAL_WHITE, PAL_BLACK);
    }

    if (panel == 9) {                                    /* "...that's hot." then the dust-cloud brawl */
        int t = (int)pt;
        if (t > 30) {
            for (i = 0; i < 6; i++)
                ui_fill_circle(sw - 70 + (i * 13) % 50, gy - 20 + (i * 7) % 18 + ui_wave(t + i * 5, 6, 2), 12, PAL_RAMP(RAMP_STONE, 5 + (i & 1)));
            for (i = 0; i < 4; i++) ui_sparkle(sw - 60 + i * 14, gy - 40 - (i * 9) % 10, (t / 2 + i * 3) & 15, PAL_UI_GOLD);
            if (t > 60) ui_fill_ellipse(sw - 20, gy - 70 - (t - 60) * 2 + (t - 60) * (t - 60) / 20, 10, 2, PAL_UI_GOLD);   /* halo flies out */
        }
        if (t < 30) {                                    /* the Said-It-Out-Loud citizen, red-faced */
            const DoomoImage *v = bodyspr_image(ID_VEX, 1, 0);
            if (v) plat_image(sw - 60, gy - v->h + 10, v, 1, IMG_SHADOW, 0);
        }
    }
}
