/* story_feast.c - CUT_FEAST_POP: the "What an Ass" outcome of the post-game
 * Feast of Rebuilding (owner: story). Called by cutscene.c (SCENE_CUTSCENE):
 *   feast_tick(panel, pt)        sound effects, once per tick
 *   feast_draw(panel, pt, ct)    the picture (cutscene.c draws the caption)
 *
 * Slapstick, cartoon-modest: Grumbleguts is shown in profile, belly and
 * rump both round and smooth; the hose goes behind the chair and the rest
 * is implied by a squeak and his slowly turning head. CUTE gore setting:
 * confetti, plush stuffing, hearts, a halo. BRUTAL: a red shower and gibs.
 * HOSE PASS (user): panel 3 smash-cuts to two funny close shots: Grumbleguts
 * staring dead into the lens, then a straight-on REAR shot (cheeks only,
 * smooth, a brass inflation valve in the crease) as he yanks the hose off it
 * in three heaves until it comes loose with a SHLORP (see hose_shot()).
 * Panels (cutscene.c): 0 feast, 1 eyes on him, 2 "Look who's full of
 * himself, now?", 3 the hose, 4 burp (false pop 1), 5 fart (false pop 2),
 * 6 both at once, 7 the valve wins, 8 POP, 9 "...that's hot.", 10 the candlelit
 * afterglow, 11 the slow look into the lens (epilogue()), 12 title.
 * Three Ren & Stimpy style GROSS-OUT CLOSE-UPS smash-cut in (white flash,
 * hold, record scratch) over panels 4, 5 and 7: the burp, the gas cloud and
 * the straining belly / harness strap (see closeup()). Cartoon only.
 * And the camera will not look away from the swelling belly: angle shots
 * (low angle, push-in, overhead, from behind the crowd, a crowd reaction
 * cutaway) cut in during panels 6 and 7 (see angle_shot()).
 * GRATUITOUS PASS (user): Grumbleguts now performs: hip sway, hooded eyes, a
 * timed wink, blush, squash-and-stretch on the belly and a rump that jiggles a
 * beat behind it; the crowd fans itself and swoons; the harness PINGs off in the
 * belly close-up with the studs shooting at the lens; and the POP ends with a
 * shockwave and the rump zig-zagging round the park like a let-go balloon,
 * trailing puffs, before it flops down. Still cartoon-modest (cheeks only).
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
    int amp = 1 + extra / 14;                                    /* the wobble grows with the swell  */
    int sq = ui_wave((int)ct * 3, 14, amp);                      /* belly squash and stretch         */
    int lag = ui_wave((int)ct * 3 + 4, 14, amp * 3 / 2 + 1);     /* the rump jiggles a beat behind   */
    int flirt = eating || extra >= 14;                           /* blush + hooded eyes              */
    cx += shake;
    rx += shake;
    hx += shake;
    hy += ui_wave((int)ct * 3 + 2, 14, 1);                       /* head bobs with the belly         */
    /* rump: two round cheeks with a crease, always smooth; they bounce off each other */
    ui_fill_circle(rx, ry - rr / 3 + lag, rr + 1, dk);
    ui_fill_circle(rx, ry + rr / 3 - lag, rr + 1, dk);
    ui_fill_circle(rx, ry - rr / 3 + lag, rr, md);
    ui_fill_circle(rx, ry + rr / 3 - lag, rr, md);
    plat_line(rx - rr / 3, ry, rx + rr, ry, dk);
    ui_fill_circle(rx + rr / 4, ry - rr / 2 + lag, DOOMO_MAX(rr / 5, 2), lt);
    /* belly (an ellipse, so it can squash and stretch) */
    ui_fill_ellipse(cx, cy, r + 1 + sq, r + 1 - sq, dk);
    ui_fill_ellipse(cx, cy, r + sq, r - sq, md);
    ui_fill_circle(cx - r / 3, cy - r / 3, r / 4, lt);
    /* harness + studded belt (they ping off when he is huge) */
    if (extra < 60) {
        plat_line(cx - r / 2, cy - r + 4, cx + r / 3, cy + r - 4, PAL_BLACK);
        plat_line(cx - r / 2 + 1, cy - r + 4, cx + r / 3 + 1, cy + r - 4, PAL_BLACK);
    }
    if (extra < 75)
        for (i = -4; i <= 4; i++) plat_rect(cx + i * r / 5, cy + r / 3 + (i * i) / 4 + sq / 2, 2, 2, PAL_UI_GOLD);
    /* head, horns, grin */
    ui_fill_circle(hx, hy, 15, dk);
    ui_fill_circle(hx, hy, 14, md);
    plat_line(hx - 11, hy - 10, hx - 16, hy - 24, PAL_RAMP(RAMP_BONE, 6));
    plat_line(hx + 4, hy - 13, hx + 8, hy - 26, PAL_RAMP(RAMP_BONE, 6));
    if (extra > 30) {                                            /* eyes bulging, pupils on the crowd */
        ui_fill_circle(hx - 7, hy - 3, 5, PAL_WHITE);
        plat_rect(hx - 10 + (int)((ct / 6) & 1), hy - 3, 2, 2, PAL_BLACK);
    } else {
        ui_fill_circle(hx - 7, hy - 3, 3, PAL_WHITE);
        plat_rect(hx - 8, hy - 3, 2, 2, PAL_BLACK);
    }
    if (eating) {                                                /* bedroom eyes; a wink on a timer   */
        int wk = (ct % 64u) >= 40u && (ct % 64u) < 48u;
        plat_rect(hx - 10, hy - 6, 7, wk ? 6 : 3, md);
        plat_line(hx - 10, hy - (wk ? 1 : 3), hx - 4, hy - (wk ? 1 : 3), PAL_BLACK);
    }
    if (flirt) ui_fill_ellipse(hx + 3, hy + 3, 4, 2, PAL_UI_PINK);   /* blush */
    if (eating) {                                                /* lip bitten, a string of drool sliding off the chin */
        plat_rect(hx - 9, hy + 6, 2, 2, PAL_WHITE);
        plat_rect(hx - 11, hy + 8, 1, 3 + (int)((ct / 4) % 7u), PAL_RAMP(RAMP_SKY, 7));
    }
    if (extra > 30)                                              /* a bead of sweat, sliding */
        ui_fill_ellipse(hx + 11, hy - 8 + (int)((ct * 2) % 16u), 2, 3, PAL_RAMP(RAMP_SKY, 7));
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
    if (panel >= 1 && panel <= 3)                                /* the crowd is NOT coping */
        for (i = 0; i < 6; i++) {
            int px = sw - 150 + i * 22, py = gy - 40 - (i & 1) * 6;
            if (i & 1) {                                         /* swooning: hearts drifting up      */
                ui_heart(px + 8, py - (int)((ct / 2 + (uint32_t)i * 5) % 16u), 2, PAL_UI_PINK);
            } else {                                             /* fanning themselves, hard          */
                int fl = ui_wave((int)ct * 2 + i * 3, 8, 3);
                plat_line(px + 14, py + 8, px + 21, py + 3 + fl, PAL_WHITE);
                plat_line(px + 14, py + 8, px + 22, py + 9 + fl, PAL_UI_GOLD);
            }
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
#define CU_FACE_A  70u
#define CU_FACE_B  96u     /* panel 3: deadpan into the lens */
#define CU_REAR_A  96u
#define CU_REAR_B  146u    /* panel 3: the rear shot, the yank */

/* panel 3 close shots. face: he looks dead into the lens. rear: straight-on
 * view from behind (cheeks only), a brass valve in the crease, the hose
 * hauled on in three heaves until it comes off: SHLORP. */
static void hose_shot(int face, uint32_t t0)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, cy = sh / 2, i, t = (int)t0;
    int cute = F_CUTE;
    uint8_t dk = PAL_RAMP(RAMP_BLOOD, 2), md = PAL_UI_RED, lt = PAL_RAMP(RAMP_LAVA, 6);
    uint8_t hc = PAL_RAMP(RAMP_MINT, 6), hd = PAL_RAMP(RAMP_MINT, 3);

    plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_SKY, 6));
    plat_rect(0, sh * 4 / 5, sw, sh - sh * 4 / 5, PAL_RAMP(RAMP_GRASS, 4));
    if (face) {
        int hr = sh / 3, s, px;
        plat_line(cx - hr * 2 / 3, cy - hr * 3 / 4, cx - hr, cy - hr - 14, PAL_RAMP(RAMP_BONE, 6));
        plat_line(cx - hr * 2 / 3 + 1, cy - hr * 3 / 4, cx - hr + 1, cy - hr - 14, PAL_RAMP(RAMP_BONE, 6));
        plat_line(cx + hr * 2 / 3, cy - hr * 3 / 4, cx + hr, cy - hr - 14, PAL_RAMP(RAMP_BONE, 6));
        plat_line(cx + hr * 2 / 3 - 1, cy - hr * 3 / 4, cx + hr - 1, cy - hr - 14, PAL_RAMP(RAMP_BONE, 6));
        ui_fill_circle(cx, cy, hr + 2, dk);
        ui_fill_circle(cx, cy, hr, md);
        for (s = -1; s <= 1; s += 2) {
            px = cx + s * hr / 3;
            ui_fill_circle(px, cy - hr / 5, hr / 5 + 2, PAL_WHITE);
            /* the pupils dart around, then lock onto the lens */
            ui_fill_circle(px + (t < 14 ? ((t / 5) & 1 ? -3 : 3) : 0), cy - hr / 5, hr / 12 + 1, PAL_BLACK);
            plat_line(px - hr / 5, cy - hr / 2 + (t < 14 ? 0 : 3), px + hr / 5, cy - hr / 2 + 3, PAL_BLACK);
            ui_fill_ellipse(px + s * hr / 8, cy + hr / 4, hr / 6, hr / 12, PAL_UI_PINK);     /* blush */
        }
        plat_rect(cx - hr / 3, cy + hr / 3, hr * 2 / 3, 3, PAL_BLACK);                        /* flat, deadpan mouth */
        ui_fill_ellipse(cx + hr * 2 / 3, cy - hr / 3 + (t * 2) % 16, 2, 3, PAL_RAMP(RAMP_SKY, 7));   /* one bead of sweat */
        ui_text_outline(cx - 18, 8, t < 14 ? "ahem." : "...", PAL_WHITE, PAL_BLACK);
        if (t >= 14) ui_text_outline(cx - 36, sh - 22, "one sec, hon.", PAL_UI_GOLD, PAL_BLACK);
        return;
    }
    {
        int rr = sh / 3, cyc = cy - 12, lx = cx - rr * 3 / 4, rx = cx + rr * 3 / 4;
        int pull, jig = 0, fx, fy, vy;
        if (t < 10) pull = t / 3;
        else if (t < 19) pull = 3 + (t - 10);
        else if (t < 25) pull = 12 - (t - 19) * 2;
        else if (t < 36) pull = 2 + (t - 25) * 12 / 10;
        else if (t < 40) pull = 14;
        else pull = 0;
        if (t >= 40) jig = ui_wave(t * 5, 10, DOOMO_MAX(7 - (t - 40) / 2, 0));
        vy = cyc + rr / 3 + pull / 2;
        /* the two cheeks: pinched toward the crease as it is hauled on */
        ui_fill_ellipse(lx + pull / 2, cyc + jig, rr + 2 - pull / 2, rr + 2 + pull / 3, dk);
        ui_fill_ellipse(rx - pull / 2, cyc - jig, rr + 2 - pull / 2, rr + 2 + pull / 3, dk);
        ui_fill_ellipse(lx + pull / 2, cyc + jig, rr - pull / 2, rr + pull / 3, md);
        ui_fill_ellipse(rx - pull / 2, cyc - jig, rr - pull / 2, rr + pull / 3, md);
        ui_fill_circle(lx - rr / 3, cyc - rr / 2 + jig, DOOMO_MAX(rr / 6, 2), lt);
        ui_fill_circle(rx - rr / 3, cyc - rr / 2 - jig, DOOMO_MAX(rr / 6, 2), lt);
        plat_line(cx, cyc - rr / 2, cx, vy, dk);
        plat_line(cx + 1, cyc - rr / 2, cx + 1, vy, dk);
        /* the brass inflation valve in the crease */
        plat_rect(cx - 7, vy, 14, 10, PAL_BLACK);
        plat_rect(cx - 6, vy + 1, 12, 8, PAL_UI_GOLD);
        plat_rect(cx - 2, vy + 3, 4, 4, t >= 40 ? PAL_BLACK : PAL_RAMP(RAMP_STEEL, 2));
        if (t < 40) {
            int mx, my, k;
            fx = cx + sw / 4 + pull * 2;
            fy = sh - 30 + pull;
            mx = (cx + fx) / 2;
            my = DOOMO_MAX(vy + 5, fy) + 12 - pull;
            for (k = 0; k < 5; k++) {                       /* the hose, a thick sagging curve */
                plat_line(cx, vy + 9 + k, mx, my + k, k == 4 ? hd : hc);
                plat_line(mx, my + k, fx, fy + k - 1, k == 4 ? hd : hc);
            }
            for (i = 0; i < 6; i++)                         /* ribbing */
                plat_rect(cx + (fx - cx) * i / 6, vy + 9 + (my - vy) * (i < 3 ? i : 5 - i) / 3, 2, 4, hd);
            ui_fill_circle(fx, fy, 11, PAL_BLACK);          /* the fist */
            ui_fill_circle(fx, fy, 10, md);
            for (k = -1; k <= 1; k++) plat_line(fx - 6, fy + k * 4, fx + 3, fy + k * 4, dk);
            for (k = 0; k < 4; k++) {                       /* the forearm, hauling */
                plat_line(fx + 8, fy + 4 + k, sw, sh + 20 + k - pull, k == 3 ? dk : md);
            }
            if (pull > 8)                                   /* tension lines off the valve */
                for (k = 0; k < 4; k++)
                    plat_line(cx - 10 - k * 3, vy + 4 + k * 4, cx - 22 - k * 5, vy + 2 + k * 6, PAL_WHITE);
            if (t >= 10 && t < 19) ui_text_outline(cx - 22, 10, "nnngh!", PAL_WHITE, PAL_BLACK);
            if (t >= 25) ui_text_outline(cx - 34, 10, "NNNGHHH!!", PAL_UI_GOLD, PAL_BLACK);
        } else {
            int s = t - 40;
            ui_fill_ellipse(cx + s * 3, vy + s, 4 + s * 3, 3 + s * 2, PAL_BLACK);   /* the hose end, at the lens */
            ui_fill_ellipse(cx + s * 3, vy + s, 3 + s * 3, 2 + s * 2, hc);
            for (i = 0; i < 4; i++) plat_line(cx - 30 + i * 12, vy - 4 - i * 3, cx - 54 + i * 14, vy - 22 - i * 5, PAL_WHITE);
            if (s < 24) gas(cx, vy - 4 - s, 5 + s * 2 / 3, !cute);       /* the valve lets go */
            ui_fill_circle(cx + sw / 4 + s * 7, sh - 30 - s * 4, 10, md);   /* the fist flies off */
            ui_text_big((sw - ui_text_big_width("SHLORP!", 2)) / 2, 8, "SHLORP!", 2, PAL_UI_GOLD, PAL_RAMP(RAMP_BLOOD, 2));
        }
        /* his head glances back over his shoulder, straight at the lens */
        {
            int hx = 34, hy = 36, hrr = 15;
            ui_fill_circle(hx, hy, hrr + 1, dk);
            ui_fill_circle(hx, hy, hrr, md);
            plat_line(hx - 10, hy - 10, hx - 15, hy - 24, PAL_RAMP(RAMP_BONE, 6));
            plat_line(hx + 8, hy - 11, hx + 12, hy - 24, PAL_RAMP(RAMP_BONE, 6));
            if (t < 40) {
                ui_fill_circle(hx - 5, hy - 3, 4, PAL_WHITE);
                ui_fill_circle(hx + 5, hy - 3, 4, PAL_WHITE);
                plat_rect(hx - 5, hy - 3, 2, 2, PAL_BLACK);
                plat_rect(hx + 4, hy - 3, 2, 2, PAL_BLACK);
                plat_rect(hx - 7, hy + 6, 14, 4, PAL_WHITE);                  /* gritted teeth */
                for (i = 0; i < 4; i++) plat_rect(hx - 4 + i * 4, hy + 6, 1, 4, PAL_BLACK);
                ui_fill_ellipse(hx + 14, hy - 8 + (t * 2) % 14, 2, 3, PAL_RAMP(RAMP_SKY, 7));
            } else {
                plat_line(hx - 8, hy - 3, hx - 2, hy - 5, PAL_BLACK);        /* eyes shut: relief */
                plat_line(hx + 2, hy - 5, hx + 8, hy - 3, PAL_BLACK);
                ui_fill_ellipse(hx, hy + 7, 5, 3, PAL_BLACK);
                ui_fill_ellipse(hx - 9, hy + 3, 4, 2, PAL_UI_PINK);
                ui_fill_ellipse(hx + 9, hy + 3, 4, 2, PAL_UI_PINK);
                if (t >= 46) ui_text_outline(hx + 18, hy - 4, "ahhh~", PAL_UI_PINK, PAL_BLACK);
            }
        }
    }
}

/* returns 1 when it drew a full-screen close-up instead of the scene */
static int closeup(int panel, uint32_t pt, uint32_t ct)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, cy = sh / 2, i;
    int cute = F_CUTE;
    uint32_t t0;

    if (panel == 4 && pt >= CU_BURP_A && pt < CU_BURP_B) t0 = pt - CU_BURP_A;
    else if (panel == 5 && pt >= CU_GAS_A && pt < CU_GAS_B) t0 = pt - CU_GAS_A;
    else if (panel == 7 && pt >= CU_BELLY_A && pt < CU_BELLY_B) t0 = pt - CU_BELLY_A;
    else if (panel == 3 && pt >= CU_FACE_A && pt < CU_FACE_B) t0 = pt - CU_FACE_A;
    else if (panel == 3 && pt >= CU_REAR_A && pt < CU_REAR_B) t0 = pt - CU_REAR_A;
    else return 0;
    if (t0 < 2) { plat_rect(0, 0, sw, sh, PAL_WHITE); return 1; }      /* the smash cut */
    if (panel == 3) { hose_shot(pt < CU_REAR_A, t0); return 1; }

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
        ui_text_outline(cx - 30, sh - 40, "BUUURP (wet)", PAL_UI_GOLD, PAL_BLACK);
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
        if (t0 >= 26) {                                                                      /* PING: the harness lets go,  */
            int s = (int)t0 - 26, k;                                                         /* studs shoot at the lens     */
            for (k = 0; k < 10; k++) {
                int a = k * 410 + 90, d = 6 + s * (5 + k % 3), sz = 2 + s / 2;
                plat_rect(cx + FIX2INT(fx_cos(a) * d), cy + FIX2INT(fx_sin(a) * d) / 2, sz, sz, PAL_UI_GOLD);
            }
            ui_text_outline(cx - 14, cy - 4, "PING!", PAL_UI_GOLD, PAL_BLACK);
        }
    }
    return 1;
}


/* ---- camera angle shots on the swelling belly ---------------------------------------
 * Hard cuts (whip-flash, letterbox bars, a "CAM" tag) between angles while he
 * swells: panel 6 gets a crowd reaction cutaway, panel 7 (the valve) cycles
 * low angle -> push-in -> overhead -> (belly close-up) -> from behind the crowd. */
enum { AN_LOW, AN_PUSH, AN_TOP, AN_BEHIND, AN_REACT, AN_FRONT };
typedef struct { uint8_t panel, a, b, kind; const char *name; } AngleShot;
static const AngleShot shots[] = {
    { 5, 40,  62, AN_FRONT,  "CAM 6  FRONT" },
    { 6, 50,  70, AN_FRONT,  "CAM 6  FRONT" },
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

static int cute_shot(void) { return F_CUTE; }

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
        int rr = 28 + extra / 2, by = mid + 8, jg = ui_wave((int)ct * 4, 10, 2 + extra / 20);   /* cheeks jiggle against each other */
        plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_SKY, 6));
        bunting(sw);
        plat_rect(0, sh * 2 / 3, sw, sh / 3, PAL_RAMP(RAMP_GRASS, 4));
        ui_fill_circle(cx - rr * 2 / 3, by + jg, rr + 1, PAL_RAMP(RAMP_BLOOD, 2));        /* two smooth cheeks */
        ui_fill_circle(cx + rr * 2 / 3, by - jg, rr + 1, PAL_RAMP(RAMP_BLOOD, 2));
        ui_fill_circle(cx - rr * 2 / 3, by + jg, rr, PAL_UI_RED);
        ui_fill_circle(cx + rr * 2 / 3, by - jg, rr, PAL_UI_RED);
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
    case AN_FRONT: {                                     /* straight on: the big smooth belly shudders, an X shows between the cheeks */
        int R = DOOMO_MIN(sh / 4 + extra / 4, sh * 36 / 100), by = mid + 2, cyc = by + R + R / 6, j, xs, amp = 1 + (int)DOOMO_MIN(t0, 16u) / 4;
        int sx = ui_wave((int)ct * 9, 6, amp), pulse = ui_wave((int)ct * 9 + 2, 6, amp);
        uint8_t dk = PAL_RAMP(RAMP_BLOOD, 2), pur = PAL_RAMP(RAMP_VAPE, 6), gas_c = cute_shot() ? PAL_RAMP(RAMP_VAPE, 7) : PAL_RAMP(RAMP_SICK, 6);
        plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_VAPE, 2));
        plat_rect(0, by + R * 3 / 4, sw, sh, PAL_RAMP(RAMP_VAPE, 4));                    /* floor  */
        ui_fill_ellipse(cx, cyc + R / 3, R + 10, R / 6, PAL_RAMP(RAMP_VAPE, 1));          /* shadow */
        ui_fill_circle(cx - R / 2, by + R * 9 / 10, R / 2 + 3, dk);                      /* the two big cheeks */
        ui_fill_circle(cx + R / 2, by + R * 9 / 10, R / 2 + 3, dk);
        ui_fill_circle(cx - R / 2, by + R * 9 / 10, R / 2 + 2, PAL_UI_RED);
        ui_fill_circle(cx + R / 2, by + R * 9 / 10, R / 2 + 2, PAL_UI_RED);
        plat_rect(cx, cyc - R / 4, 1, R / 2, dk);                                        /* the crease */
        for (j = 26; j >= 0; j--) {                                                      /* the hose: thin and far where it plugs into the X,
                                                                                          * fatter and lower as it comes toward the camera */
            int hx2 = cx + 2 + j * (sw / 2 - 8) / 26, hy2 = cyc + 2 + j * j / 30 + ui_wave(j * 24, 96, 2), hr = 2 + j / 6;
            ui_fill_circle(hx2, hy2 + 1, hr + 1, PAL_RAMP(RAMP_MINT, 1));                 /* its shadow side */
            ui_fill_circle(hx2, hy2, hr, PAL_RAMP(RAMP_MINT, 3 + (j & 1)));
            ui_fill_circle(hx2 - hr / 3, hy2 - hr / 3, DOOMO_MAX(hr / 3, 1), PAL_RAMP(RAMP_MINT, 6));
        }
        ui_fill_circle(cx + 2, cyc + 2, 4, PAL_RAMP(RAMP_BLOOD, 1));                      /* the hose plugs in right here: collar, then the X */
        ui_fill_circle(cx + 2, cyc + 2, 3, PAL_UI_GOLD);
        xs = 1 + (int)DOOMO_MIN(t0, 14u) / 2;                                            /* X marks the spot: pops in as the belly shudders */
        if (t0 > 4) {
            for (j = 0; j < 2; j++) {
                plat_line(cx - xs + j, cyc - xs, cx + xs + j, cyc + xs, PAL_BLACK);
                plat_line(cx + xs + j, cyc - xs, cx - xs + j, cyc + xs, PAL_BLACK);
            }
            ui_sparkle(cx + xs + 5, cyc - xs - 3, (int)((ct / 2) & 15), PAL_WHITE);
            ui_heart(cx - xs - 7, cyc - xs, 2, PAL_UI_PINK);
        }
        for (j = -1; j <= 1; j += 2) {                                                   /* spiky shoulders + fists */
            int shx = cx + j * (R + R / 4), shy = by - R / 3, fx = cx + j * (R + R / 3), fy = by + R / 3, q;
            ui_fill_circle(shx, shy, R / 3 + 1, dk);
            ui_fill_circle(shx, shy, R / 3, PAL_UI_RED);
            plat_line(shx, shy - R / 3, shx + j * R / 3, shy - R * 2 / 3, pur);
            plat_line(shx + 1, shy - R / 3, shx + j * R / 3 + 1, shy - R * 2 / 3, pur);
            ui_fill_circle(fx, fy, R / 4 + 1, dk);
            ui_fill_circle(fx, fy, R / 4, PAL_UI_RED);
            for (q = -1; q <= 1; q++) ui_fill_circle(fx + q * R / 8, fy - R / 4, 2, PAL_RAMP(RAMP_BONE, 6));   /* knuckle studs */
        }
        ui_fill_circle(cx, by - R, R / 2 + 1, dk);                                       /* head, crown, gem */
        ui_fill_circle(cx, by - R, R / 2, PAL_UI_RED);
        plat_line(cx - R / 4, by - R - R / 2 + 2, cx - R / 4 - 3, by - R - R / 2 - 8, PAL_RAMP(RAMP_BONE, 6));
        plat_line(cx + R / 4, by - R - R / 2 + 2, cx + R / 4 + 3, by - R - R / 2 - 8, PAL_RAMP(RAMP_BONE, 6));
        plat_rect(cx - R / 3, by - R - R / 3, R * 2 / 3, 4, PAL_UI_GOLD);
        ui_fill_circle(cx, by - R - R / 3 + 2, 4, PAL_UI_GOLD);
        ui_fill_circle(cx, by - R - R / 3 + 2, 3, PAL_RAMP(RAMP_GRASS, 6));
        ui_fill_circle(cx - R / 2, by - R - R / 4, 2, PAL_RAMP(RAMP_SKY, 7));
        ui_fill_circle(cx + R / 2, by - R - R / 4, 2, PAL_RAMP(RAMP_SKY, 7));
        ui_fill_ellipse(cx + sx, by, R + 1 + pulse, R + 1 - pulse, PAL_RAMP(RAMP_FLESH, 4));   /* THE belly: huge, smooth, shuddering */
        ui_fill_ellipse(cx + sx, by, R + pulse, R - pulse, PAL_RAMP(RAMP_FLESH, 7));
        ui_fill_circle(cx + sx - R / 3, by - R / 3, R / 5, PAL_RAMP(RAMP_BONE, 7));
        if ((ct & 1u) != 0)                                                              /* shudder marks */
            for (j = 0; j < 3; j++) {
                plat_line(cx - R - 10 - j * 4, by - 8 + j * 8, cx - R - 15 - j * 4, by - 4 + j * 8, PAL_WHITE);
                plat_line(cx + R + 10 + j * 4, by - 8 + j * 8, cx + R + 15 + j * 4, by - 4 + j * 8, PAL_WHITE);
            }
        for (j = 0; j < 9; j++) {                                                        /* puffs spraying out of the crease */
            int a = 2300 + j * 190, d = 8 + (int)t0 * (3 + j % 3), pr = 2 + j % 3;
            ui_fill_circle(cx + FIX2INT(fx_cos(a) * d), cyc + FIX2INT(fx_sin(a) * d) / 2, pr, gas_c);
            ui_fill_circle(cx + FIX2INT(fx_cos(a) * d) - 1, cyc + FIX2INT(fx_sin(a) * d) / 2 - 1, 1, PAL_WHITE);
        }
        if (t0 < 10)                                                                     /* speed lines on the first beat */
            for (j = 0; j < 10; j++) {
                int a = j * 410;
                plat_line(cx + FIX2INT(fx_cos(a) * (R + 24)), by + FIX2INT(fx_sin(a) * (R + 24)) / 2,
                          cx + FIX2INT(fx_cos(a) * 220), by + FIX2INT(fx_sin(a) * 220) / 2, PAL_WHITE);
            }
        ui_text_outline(cx + R + 8, by + R / 2, shots[k].panel == 5 ? "TOOT" : "BRRRT!", PAL_UI_GOLD, PAL_BLACK);
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

/* where the loose rump is t ticks after the POP: it zig-zags round the park like a
 * let-go balloon (wide swings that settle, a slow fall), then lands */
static void rump_pos(int t, int cx, int cy, int gy, int *x, int *y)
{
    int f = DOOMO_MIN(t, 96);
    *x = cx + 10 + ui_wave(f * 3, 56, 70 - f / 2);
    *y = cy - 40 + f * 2 / 3 + ui_wave(f * 5 + 14, 36, 26 - f / 5);
    if (*y > gy - 12) *y = gy - 12;
}

/* 1 while the full-screen front fart shot is up: cutscene.c hides the caption so the X shows */
int feast_front_shot(int panel, uint32_t pt)
{
    int i;
    if (panel == 3 && pt >= CU_FACE_A && pt < CU_REAR_B) return 1;      /* the hose close shots too */
    for (i = 0; i < NSHOTS; i++)
        if (shots[i].kind == AN_FRONT && shots[i].panel == panel && pt >= shots[i].a && pt < shots[i].b) return 1;
    return 0;
}

void feast_tick(int panel, uint32_t pt)
{
    if (angle_start(panel, pt)) audio_sfx(SFX_BLIP);                  /* a cut */
    switch (panel) {
    case 0: if ((pt % 24) == 6) audio_sfx(SFX_EAT);
            if (pt == 50 || pt == 110) audio_sfx(SFX_SPARKLE);         /* the wink       */
            break;
    case 1: if (pt == 40) audio_sfx(SFX_LOVE); break;                  /* wolf whistle   */
    case 2: if (pt == 40) audio_sfx(SFX_ERROR); break;                 /* record scratch */
    case 3:
        if (pt == 20 || pt == 70 || pt == 120) audio_sfx(SFX_SQUEAK);
        if (pt == CU_FACE_A) audio_sfx(SFX_ERROR);                      /* record scratch */
        if (pt == CU_REAR_A) audio_sfx(SFX_BLIP);
        if (pt == CU_REAR_A + 12 || pt == CU_REAR_A + 28 || pt == CU_REAR_A + 34) audio_sfx(SFX_SQUEAK);   /* the heaves */
        if (pt == CU_REAR_A + 40) { audio_sfx(SFX_POOP); audio_sfx(SFX_SPLASH); }                         /* SHLORP */
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
            if (pt == CU_BELLY_A + 26) audio_sfx(SFX_PICKUP);          /* PING           */
            break;
    case 8: if (pt == 4) { audio_sfx(SFX_EXPLODE); audio_sfx(F_CUTE ? SFX_CONFETTI : SFX_SPLAT); }
            if (pt == 14 || pt == 34 || pt == 54 || pt == 74) audio_sfx(SFX_POOP);     /* balloon flight */
            if (pt == 24 || pt == 44 || pt == 64) audio_sfx(SFX_SQUEAK);
            if (pt == 90) audio_sfx(SFX_SPLAT);                        /* flop           */
            if (pt == 100) audio_sfx(SFX_EAT);                         /* last gurgle    */
            break;
    case 9: if (pt == 24) audio_sfx(SFX_ERROR); if (pt == 30) audio_sfx(SFX_SPLAT); break;
    case 10: if (pt == 20 || pt == 100) audio_sfx(SFX_LOVE);
             if (pt == 60) audio_sfx(SFX_SPARKLE);
             break;
    case 11: if (pt == 30) audio_sfx(SFX_LOVE);
             if (pt == 66) audio_sfx(SFX_SPARKLE);                       /* the slow wink  */
             break;
    case 12: if (pt == 4) audio_sfx(SFX_BELL); break;
    default: break;
    }
}

/* ---- epilogue: candlelit, slow, sultry (panels 10 and 11) -------------------------
 * Night in the park, a lit-up table, a drifting thread of smoke, the crowd still
 * there and still swooning. Then a close-up: half-lidded, one slow wink, smoke
 * breathed across the lens, an iris closing. Cartoon only, nothing below the
 * shoulders in the close-up. */
static void candles(int x, int y, uint32_t ct)
{
    int i;
    for (i = 0; i < 3; i++) {
        int cx = x + i * 12, h = 8 + (i & 1) * 4, fl = ui_wave((int)ct * 3 + i * 5, 9, 1);
        ui_fill_circle(cx + 2, y - h - 8, 8, PAL_RAMP(RAMP_LAVA, 1));                       /* glow */
        plat_rect(cx, y - h, 4, h, PAL_RAMP(RAMP_BONE, 6));
        ui_fill_ellipse(cx + 2 + fl, y - h - 3, 2, 3, PAL_UI_GOLD);
    }
}

static void smoke(int x, int y, int rise, uint32_t ct)
{
    int i;
    for (i = 0; i < 6; i++) {
        int yy = y - (int)((ct * 2 + (uint32_t)i * 14) % (uint32_t)rise);
        ui_fill_circle(x + ui_wave((int)ct + i * 9, 24, 5 + i), yy, 2 + i / 2, PAL_RAMP(RAMP_STONE, 6));
    }
}

static void epilogue(int panel, uint32_t pt, uint32_t ct)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), gy = sh * 3 / 4, i, t = (int)pt;
    int cx = sw / 2 - 20, cy = gy - 56;
    uint8_t dk = PAL_RAMP(RAMP_BLOOD, 2), md = PAL_UI_RED, lt = PAL_RAMP(RAMP_LAVA, 6);

    if (panel == 10) {
        plat_rect(0, 0, sw, gy, PAL_RAMP(RAMP_VAPE, 0));
        plat_rect(0, gy, sw, sh - gy, PAL_RAMP(RAMP_GRASS, 1));
        ui_fill_circle(sw - 46, 40, 16, PAL_RAMP(RAMP_BONE, 7));                           /* moon */
        ui_fill_circle(sw - 40, 36, 14, PAL_RAMP(RAMP_VAPE, 0));
        for (i = 0; i < 9; i++) ui_sparkle((i * 47) % sw, 14 + (i * 23) % 50, (int)((ct / 3 + (uint32_t)i * 3) & 15), PAL_WHITE);
        plat_rect(8, gy - 26, 70, 6, PAL_RAMP(RAMP_WOOD, 3));                               /* the table, lit */
        plat_rect(12, gy - 20, 4, 22, PAL_RAMP(RAMP_WOOD, 1));
        plat_rect(68, gy - 20, 4, 22, PAL_RAMP(RAMP_WOOD, 1));
        candles(18, gy - 26, ct);
        plat_rect(cx - 24, gy - 14, 60, 5, PAL_RAMP(RAMP_WOOD, 3));                         /* his chair */
        plat_rect(cx - 20, gy - 10, 8, 14, PAL_RAMP(RAMP_WOOD, 1));
        plat_rect(cx + 22, gy - 10, 8, 14, PAL_RAMP(RAMP_WOOD, 1));
        grumble(cx, cy, 2 + ui_wave((int)ct, 60, 2), ui_wave((int)ct, 64, 2), ct, 1);       /* slow breathing, slow sway */
        plat_rect(cx - 36, cy - 38, 8, 2, PAL_RAMP(RAMP_BONE, 6));                          /* the cigarette */
        plat_rect(cx - 37, cy - 38, 2, 2, PAL_UI_GOLD);
        smoke(cx - 38, cy - 40, 50, ct);
        crowd(sw, gy, 1, ct, 0);
        if (t > 20 && t < 110) ui_text_outline(cx - 56, cy - 70 - t / 8, "mmm~", PAL_UI_PINK, PAL_BLACK);
        for (i = 0; i < 3; i++) ui_heart(cx + 50 + i * 20, cy - 20 - (int)((ct + (uint32_t)i * 17) % 40u), 3, PAL_UI_PINK);
        return;
    }
    /* panel 11: the close-up, lit by candle glow, straight into the lens */
    {
        int hr = sh / 3 + (int)DOOMO_MIN(pt / 4, 16u), wk = t >= 60 && t < 78;
        int lid = t < 60 ? hr / 6 : (wk ? hr / 5 : hr / 7);
        plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_BLOOD, 0));
        ui_fill_circle(cx + 20, cy, hr + 30, PAL_RAMP(RAMP_LAVA, 0));                     /* warm glow */
        plat_line(cx + 20 - hr * 2 / 3, cy - hr * 3 / 4, cx + 20 - hr, cy - hr - 14, PAL_RAMP(RAMP_BONE, 6));
        plat_line(cx + 20 + hr * 2 / 3, cy - hr * 3 / 4, cx + 20 + hr, cy - hr - 14, PAL_RAMP(RAMP_BONE, 6));
        ui_fill_circle(cx + 20, cy, hr + 2, dk);
        ui_fill_circle(cx + 20, cy, hr, md);
        ui_fill_circle(cx + 20 - hr / 3, cy - hr / 2, hr / 8, lt);
        for (i = -1; i <= 1; i += 2) {
            int px = cx + 20 + i * hr / 3, py = cy - hr / 5;
            if (wk && i < 0) {                                                            /* the slow wink */
                plat_line(px - hr / 5, py, px + hr / 5, py + 2, PAL_BLACK);
                plat_line(px - hr / 5, py + 1, px + hr / 5, py + 3, PAL_BLACK);
            } else {
                ui_fill_ellipse(px, py, hr / 5 + 2, hr / 7 + 1, PAL_WHITE);
                ui_fill_circle(px, py + 1, hr / 12 + 1, PAL_BLACK);
                plat_rect(px - hr / 5 - 2, py - hr / 7 - 2, hr * 2 / 5 + 5, lid, md);     /* heavy lids */
                plat_line(px - hr / 5 - 2, py - hr / 7 - 2 + lid, px + hr / 5 + 2, py - hr / 7 - 2 + lid, PAL_BLACK);
            }
            ui_fill_ellipse(px + i * hr / 8, py + hr / 3, hr / 6, hr / 12, PAL_UI_PINK);   /* blush */
        }
        ui_fill_ellipse(cx + 20, cy + hr / 3 + 3, hr / 5 + (t > 90 ? 3 : 0), 3 + (t > 90 ? 3 : 0), PAL_BLACK);   /* soft, parted lips */
        plat_rect(cx + 20 - hr / 6, cy + hr / 3, hr / 3, 2, PAL_UI_PINK);
        if (t > 90) for (i = 0; i < 7; i++) {                                              /* the exhale, over the lens */
            int s = t - 90, k = i * 13;
            ui_fill_circle(cx + 20 + ui_wave(s * 3 + k, 40, 20 + i * 6), cy + hr / 3 - s * (i + 2) / 3, 6 + s / 3 + i, PAL_RAMP(RAMP_STONE, 5 + (i & 1)));
        }
        for (i = 0; i < 3; i++) ui_heart(cx - 40 + i * 70, cy - hr - 6 + (int)((ct + (uint32_t)i * 13) % 20u), 4, PAL_UI_PINK);
        if (t > 30 && t < 150) ui_text_outline(cx - 6, 8, "stay a while~", PAL_UI_PINK, PAL_BLACK);
        if (t > 130) {                                                                     /* iris-out: bars close */
            int bar = (t - 130) * sh / 80;
            plat_rect(0, 0, sw, DOOMO_MIN(bar, sh / 2), PAL_BLACK);
            plat_rect(0, sh - DOOMO_MIN(bar, sh / 2), sw, DOOMO_MIN(bar, sh / 2), PAL_BLACK);
        }
    }
}


void feast_draw(int panel, uint32_t pt, uint32_t ct)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), gy = sh * 3 / 4, i;
    int cx = sw / 2 - 20, cy = gy - 56, extra = inflate(panel, pt), shake = 0;
    int cute = F_CUTE;

    if (panel == 10 || panel == 11) { epilogue(panel, pt, ct); return; }

    if (panel == 12) {                                   /* title card, then the memorial */
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
    if (panel < 3) shake = ui_wave((int)ct, 48, 2);                          /* hip sway       */
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
    if (panel < 3) {                                                     /* a blown kiss, drifting at the crowd */
        int kx = cx - 28 + (int)(ct % 48u) * 3, ky = cy - 54 - ui_wave((int)ct * 2, 24, 4);
        ui_heart(kx, ky, 3, PAL_UI_PINK);
        if ((ct % 48u) < 6) ui_text_outline(cx - 40, cy - 66, "mwah", PAL_UI_PINK, PAL_BLACK);
    }
    if (panel == 4 && pt >= 30 && pt < 60) ui_text_outline(cx - 70, cy - 40, "*squelch*", PAL_UI_GOLD, PAL_BLACK);
    if (panel == 5 && pt >= 40 && pt < 90) ui_text_outline(cx + 40, cy + 54, "*pffrrt*", PAL_UI_GOLD, PAL_BLACK);
    if (panel == 7 && pt > 60 && pt < 90) ui_text_outline(cx - 40, cy - 90, "don't stop~", PAL_UI_PINK, PAL_BLACK);
    if (panel == 1 && pt >= 40 && pt < 100) ui_text_outline(sw - 150, gy - 96, "*wolf whistle*", PAL_UI_GOLD, PAL_BLACK);
    if (panel == 5 && pt < 40) ui_text_outline(cx - 44, cy - 62, "mmm~", PAL_WHITE, PAL_BLACK);
    if (panel == 6 && pt >= 50 && pt < 100) ui_text_outline(cx - 48, cy - 74 - (int)(pt - 50) / 4, "ohhh, finally~", PAL_UI_PINK, PAL_BLACK);
    if (panel == 7 && pt > 40) ui_text_outline(cx - 34, cy - 70 - extra / 2, "...gurgle", PAL_WHITE, PAL_BLACK);
    if (panel == 7 && pt > 120) {                                        /* the pressure gauge */
        ui_fill_circle(sw - 30, 44, 12, PAL_WHITE);
        plat_line(sw - 30, 44, sw - 30 + (int)DOOMO_MIN(pt - 120, 28u) / 2 - 2, 44 - 8, PAL_UI_RED);
        ui_text_outline(sw - 44, 60, "MAX", PAL_UI_RED, PAL_BLACK);
    }

    if (panel == 8) {                                    /* POP: flash, pieces, the belly lands last */
        int t = (int)pt;
        if (t < 6) plat_rect(0, 0, sw, sh, PAL_WHITE);
        if (t < 26) {                                    /* shockwave ring */
            int rad = t * 7, k;
            for (k = 0; k < 24; k++) {
                int a = k * 171, a2 = (k + 1) * 171;
                plat_line(cx + FIX2INT(fx_cos(a) * rad), cy - 30 + FIX2INT(fx_sin(a) * rad) * 3 / 4,
                          cx + FIX2INT(fx_cos(a2) * rad), cy - 30 + FIX2INT(fx_sin(a2) * rad) * 3 / 4,
                          t < 14 ? PAL_WHITE : PAL_UI_GOLD);
            }
        }
        for (i = 0; i < 40; i++) {
            int a = i * 163 + 40, rad = DOOMO_MIN(t * (2 + i % 5), 110);
            int x = cx + FIX2INT(fx_cos(a) * rad), y = cy - 30 + FIX2INT(fx_sin(a) * rad) + t * t / 40;
            if (y > gy - 3) y = gy - 3 - ((y - (gy - 3)) % 10);          /* bounce off the grass */
            if (cute) {
                if (i % 5 == 0) ui_heart(x, y, 3, PAL_UI_PINK);
                else if (i % 3 == 0) plat_rect(x, y, 5, 4, PAL_RAMP(RAMP_BONE, 6));          /* plush stuffing */
                else plat_rect(x, y, 3, 3, PAL_PAINT(i & 31, 2));                              /* confetti       */
            } else {
                plat_rect(x, y, 3 + i % 3, 3 + i % 2, i % 4 ? PAL_RAMP(RAMP_BLOOD, 3 + i % 3) : PAL_RAMP(RAMP_FLESH, 3));
            }
        }
        if (!cute) plat_checker(0, 0, sw, sh * 2 / 3, PAL_RAMP(RAMP_BLOOD, 3));
        /* the rump is the last thing to go: it tears off round the park like a let-go balloon,
         * trailing puffs (hearts on CUTE), then flops down and sits there, deflating */
        {
            int rx, ry, k, rad = 12 - DOOMO_MIN(t / 12, 5);
            for (k = 5; k >= 1; k--) {
                int tt = t - k * 4, qx, qy;
                if (tt < 6 || t >= 96) continue;
                rump_pos(tt, cx, cy, gy, &qx, &qy);
                gas(qx + 8, qy + 4, 2 + (5 - k) / 2, !cute);
            }
            rump_pos(t, cx, cy, gy, &rx, &ry);
            ui_fill_circle(rx, ry, rad + 1, PAL_RAMP(RAMP_BLOOD, 2));
            ui_fill_circle(rx, ry, rad, PAL_UI_RED);
            plat_line(rx - rad / 2, ry, rx + rad / 2, ry, PAL_RAMP(RAMP_BLOOD, 2));      /* the crease */
            if (t < 90 && ((t / 6) & 1)) ui_text_outline(rx - 22, ry - rad - 12, "pbbbbt~", PAL_WHITE, PAL_BLACK);
            if (t >= 90) ui_text_outline(rx - 12, ry - rad - 12, "plap.", PAL_WHITE, PAL_BLACK);
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
