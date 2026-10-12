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

void feast_tick(int panel, uint32_t pt)
{
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
