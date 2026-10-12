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
 * REAR PUSH PASS (user): while the culprit works the hose in like a reverse rope
 * climb, panel 3 cuts to a second, more direct rear camera on the push itself
 * (see push_shot()): cheeks only, the brass valve in the crease as the one
 * thing shown down there, hose ribs marching in, hand-over-hand on the beat,
 * a pause (\"surely that's enough?\"), then it keeps going. Rule of three.
 * Panels (cutscene.c): 0 feast, 1 eyes on him, 2 "Look who's full of
 * himself, now?", 3 the hose, 4 burp (false pop 1), 5 fart (false pop 2),
 * 6 both at once, 7 the valve wins, 8 POP, 9 "...that's hot.", 10 the candlelit
 * afterglow, 11 the slow look into the lens (epilogue()),
 * 12-13 the bike pump: he inflates himself on the spot (pump_scene()), no pop, no
 * lift-off, 14-19 the long night: the leak, the burp-and-poke, the little black
 * book, the queue, the pump passed down the line, dawn (night_scene()), 20 title.
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
#define CU_PUSH_A  40u
#define CU_PUSH_B  112u    /* panel 3: rear camera, the hose pushed further and further in */
#define CU_FACE_A  112u
#define CU_FACE_B  138u    /* panel 3: deadpan into the lens */
#define CU_REAR_A  138u
#define CU_REAR_B  188u    /* panel 3: the rear shot, the yank */

/* PS1 build? (plat_name() is "PS1"; "GBA" and "NATIVE" keep the valve detail) */
static int on_ps1(void)
{
    const char *n = plat_name();
    return n && n[0] == 'P' && n[1] == 'S';
}

/* panel 3, second rear camera: the culprit works the hose in like a reverse
 * rope climb, hand over hand, one palm braced on a cheek. Cheeks only; the
 * brass valve in the crease is all that is shown down there and the hose
 * plugs into it (GBA/native; the PS1 version leaves the valve out). Three
 * beats (rule of three): four pulls, a pause with a sweaty "surely that's enough?", then it keeps going, faster, past any
 * sensible length. The ribs on the hose march toward the valve and a foot
 * counter climbs. t0 runs 0 .. CU_PUSH_B - CU_PUSH_A - 1. */
static int push_pulls(int t)
{
    return t < 24 ? t / 6 : t < 36 ? 4 : 4 + (t - 36) / 4;
}

static void push_shot(uint32_t t0)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, cy = sh / 2, i, k, t = (int)t0;
    int cute = F_CUTE;
    uint8_t dk = PAL_RAMP(RAMP_BLOOD, 2), md = PAL_UI_RED, lt = PAL_RAMP(RAMP_LAVA, 6);
    uint8_t hc = PAL_RAMP(RAMP_MINT, 6), hd = PAL_RAMP(RAMP_MINT, 3);
    uint8_t sk = PAL_RAMP(RAMP_PEACH, 6), skd = PAL_RAMP(RAMP_PEACH, 3);
    int rr = sh / 3, cyc = cy - 12, lx = cx - rr * 3 / 4, rx = cx + rr * 3 / 4;
    int pause = t >= 24 && t < 36;
    int len = t < 24 ? 6 : 4, ph = t < 24 ? t % 6 : (t >= 36 ? (t - 36) % 4 : 0);
    int pulls = push_pulls(t);
    int sq = pause ? 0 : DOOMO_MAX(3 - ph, 0);          /* the cheeks squash on each shove, spring back */
    int vy = cyc + rr / 3;
    int fx = cx + sw / 4 + 10, fy = sh - 34, off, s0, s1, f0, f1;
    int jig = pause ? 0 : ui_wave(t * 7, 8, 1);

    plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_SKY, 6));
    plat_rect(0, sh * 4 / 5, sw, sh - sh * 4 / 5, PAL_RAMP(RAMP_GRASS, 4));

    /* the two cheeks, squashed toward the crease by each shove */
    ui_fill_ellipse(lx + sq, cyc + jig, rr + 2 - sq, rr + 2 + sq / 2, dk);
    ui_fill_ellipse(rx - sq, cyc - jig, rr + 2 - sq, rr + 2 + sq / 2, dk);
    ui_fill_ellipse(lx + sq, cyc + jig, rr - sq, rr + sq / 2, md);
    ui_fill_ellipse(rx - sq, cyc - jig, rr - sq, rr + sq / 2, md);
    ui_fill_circle(lx - rr / 3, cyc - rr / 2 + jig, DOOMO_MAX(rr / 6, 2), lt);
    ui_fill_circle(rx - rr / 3, cyc - rr / 2 - jig, DOOMO_MAX(rr / 6, 2), lt);
    plat_line(cx, cyc - rr / 2, cx, vy, dk);
    plat_line(cx + 1, cyc - rr / 2, cx + 1, vy, dk);
    /* the brass valve in the crease: the only thing shown down there (GBA/native).
     * PS1: no valve detail, the hose just disappears into the crease */
    if (!on_ps1()) {
        plat_rect(cx - 7, vy, 14, 10, PAL_BLACK);
        plat_rect(cx - 6, vy + 1, 12, 8, PAL_UI_GOLD);
        plat_rect(cx - 2, vy + 3, 4, 4, PAL_RAMP(RAMP_STEEL, 2));
    }

    /* one palm braced flat on the near cheek for leverage, dimpling it */
    ui_fill_ellipse(lx - 4 + sq, cyc - rr / 3, 13, 10, PAL_BLACK);
    ui_fill_ellipse(lx - 4 + sq, cyc - rr / 3, 12, 9, sk);
    for (k = 0; k < 4; k++) {
        ui_fill_ellipse(lx - 14 + k * 8 + sq, cyc - rr / 3 - 12 - (k == 1 || k == 2 ? 3 : 0), 3, 6, sk);
        ui_fill_circle(lx - 14 + k * 8 + sq, cyc - rr / 3 - 17 - (k == 1 || k == 2 ? 3 : 0), 1, skd);
    }
    for (k = 0; k < 3; k++)                                  /* squash creases under the palm */
        plat_line(lx - 14 + k * 10 + sq, cyc - rr / 3 + 12, lx - 18 + k * 12 + sq, cyc - rr / 3 + 22, dk);

    /* the hose: valve to the fists, ribs marching in (u runs 48 -> 0) */
    for (k = 0; k < 5; k++) {
        plat_line(cx, vy + 9 + k, (cx + fx) / 2, vy + 9 + (fy - vy) / 2 + 8 + k, k == 4 ? hd : hc);
        plat_line((cx + fx) / 2, vy + 9 + (fy - vy) / 2 + 8 + k, fx, fy + k - 1, k == 4 ? hd : hc);
    }
    off = pulls * 8 + ph * 8 / len;
    for (i = 0; i < 6; i++) {
        int u = (i * 8 + 48 - (off % 48)) % 48;
        int px = cx + (fx - cx) * u / 48, py = vy + 9 + (fy - vy - 6) * u / 48;
        plat_rect(px, py + (u > 24 ? 6 : 3), 2, 5, hd);
    }
    /* hand over hand: each fist slides 85% -> 45% of the way to the valve, then snaps back */
    s0 = pause ? 0 : ph * 100 / len;
    s1 = pause ? 50 : ((ph + len / 2) % len) * 100 / len;
    f0 = 85 - 40 * s0 / 100;
    f1 = 85 - 40 * s1 / 100;
    for (k = 0; k < 2; k++) {
        int f = k ? f1 : f0;
        int hx = cx + (fx - cx) * f / 100 + 4, hy = vy + 12 + (fy - vy - 6) * f / 100 + (f > 50 ? 8 : 4);
        ui_fill_circle(hx, hy, 9, PAL_BLACK);
        ui_fill_circle(hx, hy, 8, md);
        for (i = -1; i <= 1; i++) plat_line(hx - 5, hy + i * 3, hx + 2, hy + i * 3, dk);
        for (i = 0; i < 3; i++) plat_line(hx + 6, hy - 2 + i, sw, sh + 6 + i, i == 2 ? dk : md);      /* the forearm */
    }
    /* the culprit's head in the corner: tongue out, brows down, bracing */
    {
        int hx = sw - 24, hy = sh - 18 + (pause ? 0 : sq);
        ui_fill_circle(hx, hy, 17, PAL_BLACK);
        ui_fill_circle(hx, hy, 16, sk);
        plat_line(hx - 11, hy - 8, hx - 3, hy - 5, PAL_BLACK);                    /* brows, down and in */
        plat_line(hx + 11, hy - 8, hx + 3, hy - 5, PAL_BLACK);
        plat_rect(hx - 9, hy - 3, 4, 3, PAL_BLACK);
        plat_rect(hx + 5, hy - 3, 4, 3, PAL_BLACK);
        plat_rect(hx - 6, hy + 5, 12, 2, PAL_BLACK);
        ui_fill_ellipse(hx + 3, hy + 9, 4, 5, PAL_UI_PINK);                       /* tongue, concentrating */
        ui_fill_ellipse(hx - 17, hy - 12 + (t * 2) % 14, 2, 3, PAL_RAMP(RAMP_SKY, 7));   /* sweat */
    }
    /* the coil on the ground behind him shrinks as the hose goes in */
    for (i = 0; i < DOOMO_MAX(6 - pulls / 2, 0); i++)
        ui_fill_ellipse(sw - 40, sh - 6 - i * 3, 16, 4, PAL_RAMP(RAMP_MINT, 3 + (i & 1)));

    /* a foot counter on the hose, climbing; the pause is the "surely that's enough" beat */
    {
        char buf[16];
        int ft = pulls * 6 + (pause ? 0 : ph * 6 / len);
        buf[0] = 'I'; buf[1] = 'N'; buf[2] = ':'; buf[3] = ' ';
        buf[4] = (char)('0' + (ft / 10) % 10); buf[5] = (char)('0' + ft % 10);
        buf[6] = ' '; buf[7] = 'F'; buf[8] = 'T'; buf[9] = 0;
        ui_text_outline(sw - 66, 22, buf, t >= 36 ? PAL_UI_GOLD : PAL_WHITE, PAL_BLACK);
    }
    ui_text_outline(sw - 84, 6, "CAM 7  REAR", PAL_WHITE, PAL_BLACK);
    if (t < 24 && ph < 3) ui_text_outline(cx - 60, sh - 14, "hnnng", PAL_WHITE, PAL_BLACK);
    if (pause) {
        ui_text_outline(cx - 54, sh - 14, "surely that's enough?", PAL_UI_GOLD, PAL_BLACK);
        ui_text_outline(cx - 14, 8, "phew", PAL_WHITE, PAL_BLACK);
    }
    if (t >= 36 && ph < 2) ui_text_outline(cx - 60, sh - 14, "...nope.", PAL_UI_GOLD, PAL_BLACK);
    if (!pause && ph == 0 && t > 0) ui_text_outline(cx - 76 + (pulls % 3) * 24, 40, "*squeak*", PAL_UI_GOLD, PAL_BLACK);
    if (t >= 44)                                             /* strain lines off the cheeks */
        for (k = 0; k < 3; k++) {
            plat_line(lx - rr - 6 - k * 4, cyc - 8 + k * 8, lx - rr - 20 - k * 5, cyc - 14 + k * 10, PAL_WHITE);
            plat_line(rx + rr + 6 + k * 4, cyc - 8 + k * 8, rx + rr + 20 + k * 5, cyc - 14 + k * 10, PAL_WHITE);
        }
    /* his head glances back over his shoulder: gritted, then a slow raised brow */
    {
        int hx = 34, hy = 40, hrr = 15;
        ui_fill_circle(hx, hy, hrr + 1, dk);
        ui_fill_circle(hx, hy, hrr, md);
        plat_line(hx - 10, hy - 10, hx - 15, hy - 24, PAL_RAMP(RAMP_BONE, 6));
        plat_line(hx + 8, hy - 11, hx + 12, hy - 24, PAL_RAMP(RAMP_BONE, 6));
        ui_fill_circle(hx - 5, hy - 3, 4, PAL_WHITE);
        ui_fill_circle(hx + 5, hy - 3, 4, PAL_WHITE);
        plat_rect(hx - 4 + (t >= 36 ? 2 : 0), hy - 3, 2, 2, PAL_BLACK);
        plat_rect(hx + 5 + (t >= 36 ? 2 : 0), hy - 3, 2, 2, PAL_BLACK);
        plat_line(hx - 9, hy - 8, hx - 1, hy - 8, PAL_BLACK);
        plat_line(hx + 1, hy - 8 - (t >= 36 ? 4 : 0), hx + 9, hy - 8 - (t >= 36 ? 6 : 0), PAL_BLACK);   /* the one raised eyebrow */
        plat_rect(hx - 6, hy + 6, 12, 3, t >= 36 ? PAL_BLACK : PAL_WHITE);
        if (!cute && t >= 36) ui_fill_circle(hx + 14, hy + 8, 1, PAL_RAMP(RAMP_SKY, 7));
    }
}

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
    else if (panel == 3 && pt >= CU_PUSH_A && pt < CU_PUSH_B) t0 = pt - CU_PUSH_A;
    else if (panel == 3 && pt >= CU_FACE_A && pt < CU_FACE_B) t0 = pt - CU_FACE_A;
    else if (panel == 3 && pt >= CU_REAR_A && pt < CU_REAR_B) t0 = pt - CU_REAR_A;
    else return 0;
    if (t0 < 2) { plat_rect(0, 0, sw, sh, PAL_WHITE); return 1; }      /* the smash cut */
    if (panel == 3) {
        if (pt < CU_PUSH_B) push_shot(t0); else hose_shot(pt < CU_REAR_A, t0);
        return 1;
    }

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

/* ---- the long night (panels 14-19): shot list and spring physics ---------------------
 * Jiggle is a stateless closed-form damped spring, so every frame is a pure function of
 * the panel tick (no per-frame state, replays and rewinds identically on every port):
 *     offset(t) = sum over the last few kicks of  amp * e^(-dt/tau) * cos(2*pi*dt/period)
 * Kicks land on a per-panel cadence (a slap, a pump stroke, a hiss pulse); belly, cheeks,
 * chest and tail ring on their own periods and lag the kick by a few ticks, so the body
 * ripples instead of bobbing. Integer maths only. */
enum { GG_V_FRONT, GG_V_SIDE, GG_V_BACK, GG_V_LOOK };      /* LOOK = rear, glancing back */

typedef struct { uint8_t panel; uint16_t a, b; uint8_t view; } GgCut;
static const GgCut gg_cuts[] = {
    { 14,  30,  84, GG_V_BACK }, { 14, 118, 160, GG_V_SIDE }, { 14, 176, 210, GG_V_LOOK },
    { 15,  22,  70, GG_V_SIDE }, { 15, 104, 168, GG_V_BACK },
    { 16,  34,  92, GG_V_BACK }, { 16, 146, 202, GG_V_LOOK },
    { 17,  22,  84, GG_V_BACK }, { 17, 118, 160, GG_V_SIDE }, { 17, 172, 236, GG_V_LOOK },
    { 18,  18,  78, GG_V_BACK }, { 18, 108, 150, GG_V_SIDE }, { 18, 160, 236, GG_V_BACK },
    { 19,  14,  76, GG_V_LOOK }
};

static const GgCut *gg_cut_at(int panel, uint32_t pt)
{
    unsigned i;
    if (panel < 14 || panel > 19) return 0;
    for (i = 0; i < sizeof gg_cuts / sizeof gg_cuts[0]; i++)
        if (gg_cuts[i].panel == panel && pt >= gg_cuts[i].a && pt < gg_cuts[i].b) return &gg_cuts[i];
    return 0;
}

/* ticks between kicks: the panels get quicker as the night goes on */
static int gg_every(int panel)
{
    static const uint8_t ev[6] = { 16, 14, 18, 12, 10, 28 };
    return (panel >= 14 && panel <= 19) ? ev[panel - 14] : 16;
}

/* one kick, dt ticks after it landed */
static int gg_ring(int dt, int amp, int period, int tau)
{
    int k, d = 4096;
    if (dt < 0 || dt > tau * 4 || amp == 0) return 0;
    for (k = 0; k < dt; k++) d = d * (tau - 1) / tau;
    return ui_wave(dt + period / 4, period, amp) * d / 4096;
}

/* a train of kicks every `every` ticks starting at `phase`; the last three still ring.
 * parity 0/1 = the kicks that land on this side hit full strength, the others only
 * ring a quarter (a slap on one cheek barely shakes the other); -1 = all full. */
static int gg_spring(int t, int every, int phase, int amp, int period, int tau, int parity)
{
    int n, j, sum = 0, kt = t - phase;
    if (kt < 0 || every <= 0) return 0;
    n = kt / every;
    for (j = 0; j < 3 && n - j >= 0; j++) {
        int idx = n - j, a = amp * (6 + (idx * 5) % 5) / 8;           /* every kick a little different */
        if (parity >= 0 && (idx & 1) != parity) a /= 4;
        sum += gg_ring(kt - idx * every, a, period, tau);
    }
    return sum;
}

typedef struct {
    int bob, chest, tail, flap;          /* whole body, pecs, tail whip, wing beat             */
    int belly_x, belly_y;                /* belly lags the kick: forward-back and up-down       */
    int lx, ly, rx, ry;                  /* left / right cheek: in-out, up-down                 */
} GgMo;

static void gg_motion(GgMo *m, int panel, int t, uint32_t ct, int s, const GgCut *cut)
{
    int every = gg_every(panel), phase = cut ? cut->a + 4 : 0;
    int amp = (panel == 17 ? 4 : panel == 18 ? 5 : panel == 19 ? 2 : 3) * s;
    int k = (int)ct;
    m->bob     = gg_spring(t, every, phase, amp / 2 + 1, 12, 10, -1) + ui_wave(k * 3, 24, 1);
    m->chest   = gg_spring(t, every, phase + 1, amp / 2 + 1, 14, 9, -1);
    m->tail    = gg_spring(t, every, phase + 3, amp * 3, 18, 14, -1) + ui_wave(k * 5, 16, s * 2);
    m->flap    = ui_wave(k * 4, 14, 3 * s) + gg_spring(t, every, phase, amp * 2, 10, 8, -1);
    m->belly_x = gg_spring(t, every, phase + 2, amp, 16, 14, -1);
    m->belly_y = gg_spring(t, every, phase + 3, amp * 3 / 2, 14, 12, -1);
    m->lx = gg_spring(t, every, phase + 1, amp, 12, 8, 0);
    m->ly = gg_spring(t, every, phase + 2, amp * 2, 16, 10, 0);
    m->rx = gg_spring(t, every, phase + 1, amp, 12, 8, 1);
    m->ry = gg_spring(t, every, phase + 2, amp * 2, 16, 10, 1);
    if (!cut) {                                                  /* front shots: both cheeks rock together, a beat apart */
        m->lx = m->ly = m->rx = m->ry = 0;
        m->ly = gg_spring(t, every, 0, amp * 2, 15, 10, -1);
        m->ry = gg_spring(t, every, 2, amp * 2, 17, 10, -1);
        m->lx = m->ly / 3;
        m->rx = -m->ry / 3;
    }
}

/* 1 while the full-screen front fart shot is up: cutscene.c hides the caption so the X shows */
int feast_front_shot(int panel, uint32_t pt)
{
    int i;
    if (gg_cut_at(panel, pt)) return 1;                                  /* the night's rump-cam / profile cut-ins */
    if (panel == 3 && pt >= CU_PUSH_A && pt < CU_REAR_B) return 1;      /* the hose close shots too */
    for (i = 0; i < NSHOTS; i++)
        if (shots[i].kind == AN_FRONT && shots[i].panel == panel && pt >= shots[i].a && pt < shots[i].b) return 1;
    return 0;
}

void feast_tick(int panel, uint32_t pt)
{
    if (angle_start(panel, pt)) audio_sfx(SFX_BLIP);                  /* a cut */
    {   const GgCut *gc = gg_cut_at(panel, pt);                       /* the night's cut-ins */
        if (gc && pt == gc->a) audio_sfx(SFX_BLIP);
        if (gc && panel != 14 && pt >= (uint32_t)gc->a + 4 && ((pt - gc->a - 4) % (uint32_t)gg_every(panel)) == 0) {
            audio_sfx(SFX_SPLAT);                                      /* the smack... */
            audio_sfx(SFX_SQUEAK);                                     /* ...and the wobble that follows */
        }
        if (gc && panel == 14 && ((pt - gc->a) % (uint32_t)gg_every(panel)) == 6) audio_sfx(SFX_POOP);   /* the pulses */
    }
    switch (panel) {
    case 0: if ((pt % 24) == 6) audio_sfx(SFX_EAT);
            if (pt == 50 || pt == 110) audio_sfx(SFX_SPARKLE);         /* the wink       */
            break;
    case 1: if (pt == 40) audio_sfx(SFX_LOVE); break;                  /* wolf whistle   */
    case 2: if (pt == 40) audio_sfx(SFX_ERROR); break;                 /* record scratch */
    case 3:
        if (pt == 20) audio_sfx(SFX_SQUEAK);
        if (pt == CU_PUSH_A) audio_sfx(SFX_BLIP);                       /* cut to the rear camera */
        if (pt >= CU_PUSH_A && pt < CU_PUSH_B) {                        /* one squeak per shove, the music drops to a pluck */
            uint32_t pt0 = pt - CU_PUSH_A;
            if ((pt0 < 24u && (pt0 % 6u) == 0) || (pt0 >= 36u && ((pt0 - 36u) % 4u) == 0)) audio_sfx(SFX_SQUEAK);
            if (pt0 == 36u) audio_sfx(SFX_BLIP);                        /* "...nope." */
        }
        if (pt == CU_FACE_A) audio_sfx(SFX_ERROR);                      /* record scratch */
        if (pt == CU_REAR_A) audio_sfx(SFX_BLIP);
        if (pt == CU_REAR_A + 12 || pt == CU_REAR_A + 28 || pt == CU_REAR_A + 34) audio_sfx(SFX_SQUEAK);   /* the heaves */
        if (pt == CU_REAR_A + 40) { audio_sfx(SFX_POOP); audio_sfx(SFX_SPLASH); }                         /* SHLORP */
        if (pt == CU_REAR_B + 4) audio_sfx(SFX_SPLAT);                  /* the SLAP       */
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
    case 12: if ((pt % 20) == 10) audio_sfx(SFX_SQUEAK);                  /* a stroke of the pump */
             break;
    case 13: if ((pt % 24) == 12) audio_sfx(SFX_SQUEAK);
             if (pt == 108 || pt == 156) audio_sfx(SFX_ERROR);            /* the skin complains */
             if (pt == 160) audio_sfx(SFX_LOVE);
             break;
    case 14: if ((pt % 24) == 6) audio_sfx(SFX_SQUEAK);                  /* the harness */
             if (pt == 20 || pt == 70 || pt == 118) audio_sfx(SFX_POOP);    /* the long hiss */
             if (pt == 40 || pt == 90) audio_sfx(SFX_BUBBLE);               /* gurgle */
             if (pt == 180) audio_sfx(SFX_SPLAT);                           /* BLORP */
             break;
    case 15: if (pt == 8) audio_sfx(SFX_EAT);                               /* the burp */
             if ((pt % 40) == 4) audio_sfx(SFX_SQUEAK);                     /* poke, squelch */
             if ((pt % 40) == 6) audio_sfx(SFX_SPLASH);
             if (pt == 100 || pt == 170) audio_sfx(SFX_LOVE);
             break;
    case 16: if ((pt % 28) == 10) audio_sfx(SFX_BLIP);                      /* a page turns */
             if (pt == 60 || pt == 130) audio_sfx(SFX_SPARKLE);
             if (pt == 160) audio_sfx(SFX_LOVE);
             break;
    case 17: if ((pt % 70) == 6) audio_sfx(SFX_BELL);                       /* the ticket machine */
             if (pt == 40 || pt == 110 || pt == 180) audio_sfx(SFX_LOVE);
             if ((pt % 35) == 20) audio_sfx(SFX_SQUEAK);                    /* the queue shuffles */
             break;
    case 18: if ((pt % 20) == 10) audio_sfx(SFX_SQUEAK);                    /* a stroke of the pump */
             if ((pt % 60) == 30) audio_sfx(SFX_LOVE);
             if (pt == 150 || pt == 200) audio_sfx(SFX_ERROR);              /* the skin complains */
             break;
    case 19: if (pt == 10) audio_sfx(SFX_SPARKLE);
             if (pt == 70) audio_sfx(SFX_EAT);                              /* a long, satisfied sigh */
             if (pt == 150) audio_sfx(SFX_HAPPY);
             break;
    case 20: if (pt == 4) audio_sfx(SFX_BELL); break;
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


/* ---- the bike pump (panels 12 and 13) -------------------------------------------------
 * Night park again. A floor pump, a hose that goes round to the rump (brass valve
 * at the back, cheeks only, profile), and Grumbleguts working the handle himself.
 * Every stroke adds a notch of belly. Eyes cross, tongue out. He just gets bigger
 * and stays put: no pop, no lift-off. */
static void pump_scene(int panel, uint32_t pt, uint32_t ct)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), gy = sh * 3 / 4, i, t = (int)pt;
    int cx = sw / 2 - 20, cy = gy - 56, per = panel == 12 ? 20 : 24, n = t / per, ph = t % per;
    int maxe = DOOMO_MIN(74, sh / 2 - 46), p, extra, r, shake, rx, ry, rr, ax, ay, px = sw - 22, py = gy - 34;
    int hx, hy, hy2, mx, my, k;
    uint8_t hc = PAL_RAMP(RAMP_MINT, 6), hd = PAL_RAMP(RAMP_MINT, 3);

    if (panel == 12) p = n * 6 + (ph >= per / 2 ? DOOMO_MIN((ph - per / 2) * 6 / 4, 6) : 0);
    else p = 60 + n * 5 + (ph >= per / 2 ? DOOMO_MIN((ph - per / 2) * 5 / 4, 5) : 0);
    p = DOOMO_MIN(p, 100);
    extra = 2 + (maxe - 2) * p / 100;
    r = 34 + extra;
    shake = ui_wave((int)ct * 5, 10, p / 25);
    rx = cx + r * 6 / 10 + shake;
    ry = cy + r / 5;
    rr = r * 7 / 10;
    ax = rx + rr - 2;
    ay = ry;

    /* the night park */
    plat_rect(0, 0, sw, gy, PAL_RAMP(RAMP_VAPE, 0));
    plat_rect(0, gy, sw, sh - gy, PAL_RAMP(RAMP_GRASS, 1));
    ui_fill_circle(sw - 46, 40, 16, PAL_RAMP(RAMP_BONE, 7));
    ui_fill_circle(sw - 40, 36, 14, PAL_RAMP(RAMP_VAPE, 0));
    for (i = 0; i < 9; i++) ui_sparkle((i * 47) % sw, 14 + (i * 23) % 50, (int)((ct / 3 + (uint32_t)i * 3) & 15), PAL_WHITE);
    plat_rect(8, gy - 26, 70, 6, PAL_RAMP(RAMP_WOOD, 3));
    plat_rect(12, gy - 20, 4, 22, PAL_RAMP(RAMP_WOOD, 1));
    plat_rect(68, gy - 20, 4, 22, PAL_RAMP(RAMP_WOOD, 1));
    candles(18, gy - 26, ct);
    plat_rect(cx - 24, gy - 14, 60, 5, PAL_RAMP(RAMP_WOOD, 3));                             /* his chair */
    plat_rect(cx - 20, gy - 10, 8, 14, PAL_RAMP(RAMP_WOOD, 1));
    plat_rect(cx + 22, gy - 10, 8, 14, PAL_RAMP(RAMP_WOOD, 1));
    if (panel == 12) crowd(sw, gy, 0, ct, 0);                                                /* the stragglers */

    /* the hose, pump to valve, with a bulge running down it after each push */
    mx = (px + ax) / 2;
    my = DOOMO_MAX(py, ay) + 14;
    for (k = 0; k < 5; k++) {
        plat_line(px, py + k, mx, my + k, k == 4 ? hd : hc);
        plat_line(mx, my + k, ax, ay + k, k == 4 ? hd : hc);
    }
    if (ph >= per / 2) {
        int u = (ph - per / 2) * 100 / (per / 2), bx, by;
        if (u < 50) { bx = px + (mx - px) * u / 50; by = py + (my - py) * u / 50; }
        else { bx = mx + (ax - mx) * (u - 50) / 50; by = my + (ay - my) * (u - 50) / 50; }
        ui_fill_circle(bx, by + 2, 4, hc);
    }
    /* the floor pump: base, barrel, handle going down on the push */
    hy2 = gy - 34 - 14 + (ph < per / 2 ? ph * 14 / (per / 2) : (per - ph) * 14 / (per / 2));
    plat_rect(px - 12, gy - 3, 24, 4, PAL_BLACK);
    plat_rect(px - 5, gy - 34, 10, 32, PAL_RAMP(RAMP_STEEL, 3));
    plat_rect(px - 5, gy - 34, 3, 32, PAL_RAMP(RAMP_STEEL, 6));
    plat_rect(px - 1, hy2, 2, gy - 34 - hy2, PAL_RAMP(RAMP_STEEL, 5));
    plat_rect(px - 10, hy2 - 3, 20, 4, PAL_BLACK);                                           /* T-handle */
    plat_rect(px - 9, hy2 - 2, 18, 2, PAL_UI_GOLD);

    /* Grumbleguts, then the dumb face over his usual one */
    grumble(cx, cy, extra, shake, ct, 0);
    hx = cx + shake - r * 3 / 4;
    hy = cy - r * 3 / 4 + ui_wave((int)ct * 3 + 2, 14, 1);
    ui_fill_circle(hx, hy, 14, PAL_UI_RED);
    ui_fill_circle(hx - 5, hy - 3, 4, PAL_WHITE);
    ui_fill_circle(hx + 5, hy - 3, 4, PAL_WHITE);
    plat_rect(hx - 3, hy - 3, 2, 2, PAL_BLACK);                                              /* crossed eyes */
    plat_rect(hx + 2, hy - 3, 2, 2, PAL_BLACK);
    plat_rect(hx - 9, hy + 5, 14, 3, PAL_BLACK);                                             /* dopey grin */
    ui_fill_ellipse(hx - 4, hy + 10 + ui_wave((int)ct * 2, 12, 1), 3, 4, PAL_UI_PINK);       /* tongue out */
    ui_fill_ellipse(hx + 8, hy + 3, 4, 2, PAL_UI_PINK);
    if (p > 40) ui_fill_ellipse(hx + 12, hy - 8 + (t * 2) % 16, 2, 3, PAL_RAMP(RAMP_SKY, 7));   /* sweat */
    /* the arm, working the handle */
    for (k = 0; k < 3; k++) {
        plat_line(cx + r / 4, cy - r / 2 + k, (cx + r / 4 + px) / 2, cy - r * 3 / 4 + k, k == 2 ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_UI_RED);
        plat_line((cx + r / 4 + px) / 2, cy - r * 3 / 4 + k, px, hy2 - 2 + k, k == 2 ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_UI_RED);
    }
    /* the brass valve at the back */
    plat_rect(ax - 2, ay - 4, 8, 9, PAL_BLACK);
    plat_rect(ax - 1, ay - 3, 6, 7, PAL_UI_GOLD);

    /* sound effects, in letters */
    if (ph >= per / 2 && ph < per / 2 + 8) ui_text_outline(px - 28, py - 40 - ph, "pff!", PAL_WHITE, PAL_BLACK);
    if (panel == 12 && p > 20 && (t % 40) > 22 && (t % 40) < 36) ui_text_outline(cx - 56, cy - 72, "mmm~", PAL_UI_PINK, PAL_BLACK);
    if (panel == 13 && p > 70) ui_text_outline(cx - 60, cy - r - 14, "squeeeeak", PAL_UI_GOLD, PAL_BLACK);
    if (panel == 13 && t > 130 && t < 168) ui_text_outline(cx - 62, cy + r + 4 < sh - 10 ? cy + r + 4 : sh - 10, "...one more.", PAL_UI_PINK, PAL_BLACK);
    if (p > 80 && (t % 16) < 8) ui_text_outline(cx + 4, cy + r / 3, "creeak", PAL_WHITE, PAL_BLACK);   /* the chair */
}

/* ---- the long night (panels 14-19) ----------------------------------------------------
 * 14 the leak: past the limit the valve whistles, gas rolls off his side, the belly
 *    gurgles and a bubble climbs his throat.   15 the burp rolls across the park, an
 *    imp pokes the belly (squelch, moan), he asks for harder.   16 the little black book:
 *    names, hearts, a licked thumb.   17 the queue and its ticket machine.   18 the pump
 *    gets passed down the line, a different name (wrong) every stroke.   19 dawn: heaps
 *    of sleepers, a tally on the fence, one smug gazebo.
 * Raunchy cartoon: hearts, sweat, moans in letters, cheeks and bellies only. He is drawn
 * here as on the concept sheets (trailer/concept/grumbleguts.c, front view): outlined,
 * five-tone shaded, X harness, studded belt, wings, spiked tail, claws, fanged face with
 * expressions. Integer maths, stacked ellipses, lines and span triangles only (no
 * per-pixel work: on the PS1 each plat_rect is a GPU primitive), 320x240 at scale 2,
 * 240x160 at scale 1; every word is clamped on screen with shout(). */
enum { GG_GRIN, GG_WINK, GG_SHOCK, GG_SMUG, GG_LOVE, GG_BLUSH };

#define GG_LINE  PAL_RAMP(RAMP_BLOOD, 0)
#define GG_HORN  PAL_RAMP(RAMP_BONE, 6)
#define GG_LEATH PAL_RAMP(RAMP_WOOD, 1)
#define GG_STUD  PAL_RAMP(RAMP_CANDY, 4)
#define GG_CHAIN PAL_RAMP(RAMP_STEEL, 6)

static void gg_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t c)
{
    int px[3], py[3], i, y, ymin, ymax;
    px[0] = x0; px[1] = x1; px[2] = x2; py[0] = y0; py[1] = y1; py[2] = y2;
    ymin = DOOMO_MIN(y0, DOOMO_MIN(y1, y2));
    ymax = DOOMO_MAX(y0, DOOMO_MAX(y1, y2));
    for (y = ymin; y <= ymax; y++) {
        int lo = 100000, hi = -100000;
        for (i = 0; i < 3; i++) {
            int j = (i + 1) % 3, ya = py[i], yb = py[j], xx;
            if (ya == yb) {
                if (ya == y) { lo = DOOMO_MIN(lo, DOOMO_MIN(px[i], px[j])); hi = DOOMO_MAX(hi, DOOMO_MAX(px[i], px[j])); }
                continue;
            }
            if ((y < ya && y < yb) || (y > ya && y > yb)) continue;
            xx = px[i] + (px[j] - px[i]) * (y - ya) / (yb - ya);
            lo = DOOMO_MIN(lo, xx);
            hi = DOOMO_MAX(hi, xx);
        }
        if (hi >= lo) plat_rect(lo, y, hi - lo + 1, 1, c);
    }
}

/* shaded, outlined blob: five tones lit from the upper left, a rim light and a glint */
static void gg_blob(int cx, int cy, int rx, int ry, int ramp)
{
    int k, sh;
    rx = DOOMO_MAX(rx, 3);
    ry = DOOMO_MAX(ry, 3);
    ui_fill_ellipse(cx, cy, rx + 1, ry + 1, GG_LINE);
    ui_fill_ellipse(cx, cy, rx, ry, PAL_RAMP(ramp, 2));
    ui_fill_ellipse(cx + rx / 10, cy + ry / 10, rx - rx / 8, ry - ry / 8, PAL_RAMP(ramp, 7));   /* rim light */
    ui_fill_ellipse(cx, cy, rx - rx / 7, ry - ry / 7, PAL_RAMP(ramp, 3));
    for (k = 2; k <= 3; k++) {
        sh = k - 1;
        ui_fill_ellipse(cx - rx * sh / 6, cy - ry * sh / 6, DOOMO_MAX(rx - rx * (k + 1) / 7, 1),
                        DOOMO_MAX(ry - ry * (k + 1) / 7, 1), PAL_RAMP(ramp, k == 2 ? 5 : 6));
    }
    ui_fill_ellipse(cx - rx / 2, cy - ry / 2, rx / 5 + 1, ry / 6 + 1, PAL_RAMP(ramp, 7));      /* glint */
}

static void gg_wing(int x, int y, int s, int dir, int flap)
{
    int tip = 17 * s, x1 = x + dir * tip, y1 = y - 8 * s + flap, x2 = x + dir * (tip - 2 * s), y2 = y + 2 * s + flap / 2;
    int x3 = x + dir * (tip / 2), y3 = y + 3 * s;
    gg_tri(x, y, x1, y1, x2, y2, GG_LINE);
    gg_tri(x, y, x2, y2, x3, y3, GG_LINE);
    gg_tri(x + dir, y, x1 - dir, y1 + 1, x2 - dir, y2 - 1, PAL_RAMP(RAMP_VAPE, 2));
    gg_tri(x + dir, y, x2 - dir, y2 - 1, x3 - dir, y3 - 1, PAL_RAMP(RAMP_VAPE, 1));
    plat_line(x, y, x1, y1, PAL_RAMP(RAMP_VAPE, 3));
    plat_line(x, y, x2, y2, PAL_RAMP(RAMP_VAPE, 3));
    ui_fill_circle(x1, y1, 1, GG_HORN);
}

static void gg_tail(int x, int y, int s, int dir, int lag)
{
    int i, tx, ty, r;
    for (i = 0; i < 9; i++) {
        tx = x + dir * i * 3 * s / 2;
        ty = y + (i * i) / 3 - i * 2 + lag * i / 9;
        r = s + 2 - i / 4;
        ui_fill_circle(tx, ty, r + 1, GG_LINE);
        ui_fill_circle(tx, ty, r, (i & 1) ? PAL_RAMP(RAMP_BLOOD, 5) : PAL_RAMP(RAMP_BLOOD, 3));
        if (i % 3 == 1) gg_tri(tx, ty - r, tx - 2, ty - r - 3 * s / 2, tx + 2, ty - r - 3 * s / 2, GG_HORN);
    }
    tx = x + dir * 27 * s / 2 * 3 / 3; ty = y + 9;
    gg_tri(tx - dir * 2, ty - 4 * s / 2 - 1, tx + dir * 5 * s / 2 + dir, ty, tx - dir * 2, ty + 4 * s / 2 + 1, GG_LINE);
    gg_tri(tx - dir * 2, ty - 4 * s / 2, tx + dir * 5 * s / 2, ty, tx - dir * 2, ty + 4 * s / 2, PAL_RAMP(RAMP_BLOOD, 3));
}

static void gg_studs(int x0, int y0, int x1, int y1, int n)
{
    int i;
    for (i = 0; i <= n; i++) plat_rect(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, 2, 2, GG_STUD);
}

static void gg_strap(int x0, int y0, int x1, int y1, int w)
{
    int k;
    for (k = -w; k <= w; k++) plat_line(x0 + k, y0, x1 + k, y1, GG_LEATH);
    plat_line(x0 - w - 1, y0, x1 - w - 1, y1, GG_LINE);
    plat_line(x0 + w + 1, y0, x1 + w + 1, y1, GG_LINE);
    gg_studs(x0, y0, x1, y1, 4);
}

static void gg_face(int cx, int cy, int s, int expr)
{
    int ex = 4 * s, ey = cy - 2 * s, side, my = cy + 4 * s, w, i;
    for (side = -1; side <= 1; side += 2) {
        int x = cx + side * ex, rx = 3 * s, ry = (expr == GG_SHOCK) ? 3 * s : 2 * s, by;
        if (expr == GG_WINK && side == 1) {
            plat_line(x - 2 * s, ey, x + 2 * s, ey + 1, GG_LINE);
            plat_line(x - 2 * s, ey + 1, x + 2 * s, ey + 2, GG_LINE);
            continue;
        }
        ui_fill_ellipse(x, ey, rx + 1, ry + 1, GG_LINE);
        ui_fill_ellipse(x, ey, rx, ry, PAL_RAMP(RAMP_LAVA, 4));
        ui_fill_ellipse(x - 1, ey - 1, rx - 1, ry - 1, PAL_RAMP(RAMP_LAVA, 6));
        if (expr == GG_LOVE) ui_heart(x - 1, ey - 1, 2, PAL_PAINT(PAINT_CHERRY, 1));
        else { ui_fill_circle(x + s, ey, expr == GG_SHOCK ? 1 : s + 1, PAL_RAMP(RAMP_STONE, 0)); plat_rect(x + s - 1, ey - 1, 1, 1, PAL_WHITE); }
        by = ey - ry - 2;                                         /* brow: angry V, raised when shocked */
        if (expr == GG_SHOCK) { plat_line(x - 3 * s, by - 2, x + 3 * s, by - 2, GG_LINE); plat_line(x - 3 * s, by - 3, x + 3 * s, by - 3, GG_LINE); }
        else { plat_line(x - 3 * s * side, by + 2, x + 3 * s * side, by - 1, GG_LINE); plat_line(x - 3 * s * side, by + 3, x + 3 * s * side, by, GG_LINE); }
    }
    if (expr == GG_BLUSH) {                                       /* strained: big blush, tiny "o" mouth */
        ui_fill_ellipse(cx - 7 * s, cy + 2 * s, 4 * s, 2 * s, PAL_RAMP(RAMP_CANDY, 6));
        ui_fill_ellipse(cx + 7 * s, cy + 2 * s, 4 * s, 2 * s, PAL_RAMP(RAMP_CANDY, 6));
        ui_fill_ellipse(cx, my + s, 3, 3, GG_LINE);
        ui_fill_ellipse(cx, my + s, 2, 2, PAL_RAMP(RAMP_FLESH, 1));
        return;
    }
    if (expr == GG_SHOCK) {
        ui_fill_ellipse(cx, my + s, 3 * s, 3 * s + 1, GG_LINE);
        ui_fill_ellipse(cx, my + s, 3 * s - 1, 3 * s, PAL_RAMP(RAMP_FLESH, 1));
        ui_fill_ellipse(cx, my + 2 * s + 1, 2 * s, s, PAL_RAMP(RAMP_FLESH, 4));
        return;
    }
    w = (expr == GG_SMUG) ? 6 * s : 8 * s;                        /* the wide grin */
    ui_fill_ellipse(cx + s, my, w + 1, 3 * s, GG_LINE);
    ui_fill_ellipse(cx + s, my, w, 3 * s - 1, PAL_RAMP(RAMP_FLESH, 1));
    ui_fill_ellipse(cx + s, my - 2 * s, w + 1, 2 * s, PAL_RAMP(RAMP_BLOOD, 5));
    plat_line(cx + s - w - 1, my - s, cx + s + w + 1, my - s, GG_LINE);
    for (i = -2; i <= 2; i++) {                                   /* fangs */
        int fx = cx + s + i * w / 2;
        gg_tri(fx - 2, my - s, fx + 2, my - s, fx, my + s + (i == 0 ? 2 : 0), PAL_RAMP(RAMP_BONE, 7));
    }
    ui_fill_ellipse(cx + s, my + 2 * s, w / 2, s, PAL_RAMP(RAMP_FLESH, 4));
    if (expr == GG_WINK || expr == GG_LOVE) {
        ui_fill_circle(cx + 6 * s, cy + 2 * s, 2, PAL_PAINT(PAINT_ROSE, 0));
        ui_fill_circle(cx - 6 * s, cy + 2 * s, 2, PAL_PAINT(PAINT_ROSE, 0));
    }
}

/* horns, ears, goatee: shared by the front and rear heads. side = -1/+1 */
static void gg_horns(int cx, int head_y, int s)
{
    int side;
    for (side = -1; side <= 1; side += 2) {
        gg_tri(cx + side * 13 * s, head_y, cx + side * 19 * s, head_y - 7 * s, cx + side * 10 * s, head_y - 7 * s, GG_LINE);
        gg_tri(cx + side * 13 * s, head_y + 1, cx + side * 18 * s, head_y - 6 * s, cx + side * 11 * s, head_y - 6 * s, PAL_RAMP(RAMP_BLOOD, 5));
        ui_fill_ellipse(cx + side * 9 * s / 2, head_y - 11 * s, 3 * s / 2 + 1, 4 * s + 1, GG_LINE);
        ui_fill_ellipse(cx + side * 9 * s / 2, head_y - 11 * s, 3 * s / 2, 4 * s, PAL_RAMP(RAMP_BONE, 3));
        ui_fill_ellipse(cx + side * 9 * s / 2 - side, head_y - 12 * s, s, 3 * s, GG_HORN);
    }
}

/* the legs, shared by every view: hips, thighs, calves, claws */
static void gg_legs(int cx, int yf, int s, int hip_y, int dx)
{
    int side, c;
    for (side = -1; side <= 1; side += 2) {
        int lx = cx + side * dx;
        gg_blob(lx, (hip_y + yf - 8 * s) / 2, 7 * s, (yf - 8 * s - hip_y) / 2 + 4 * s, RAMP_BLOOD);
        gg_blob(lx + side * s, yf - 7 * s, 6 * s, 7 * s, RAMP_BLOOD);
        plat_line(lx - 5 * s, yf - 12 * s, lx + 5 * s, yf - 12 * s, PAL_RAMP(RAMP_BLOOD, 3));
        for (c = -1; c <= 1; c++) gg_tri(lx + c * 3 * s - 2, yf - 2 * s, lx + c * 3 * s + 2, yf - 2 * s, lx + c * 3 * s, yf + 2, GG_HORN);
    }
}

static void gg_arms(int cx, int top, int s, int dx, int swing)
{
    int side, c;
    for (side = -1; side <= 1; side += 2) {
        int ax = cx + side * dx, ay = top + 10 * s, hx = ax + side * 2 * s + swing * side, hy = ay + 12 * s;
        gg_blob(ax, ay, 6 * s, 9 * s, RAMP_BLOOD);
        gg_blob(hx, hy, 5 * s, 5 * s, RAMP_BLOOD);
        plat_rect(hx - 5 * s, hy - 7 * s, 10 * s, 2 * s, GG_LEATH);
        gg_studs(hx - 4 * s, hy - 6 * s, hx + 4 * s, hy - 6 * s, 3);
        for (c = -1; c <= 1; c++) gg_tri(hx + c * 3 * s - 1, hy + 4 * s, hx + c * 3 * s + 1, hy + 4 * s, hx + c * 3 * s, hy + 6 * s + 1, GG_HORN);
    }
}

/* Grumbleguts, FRONT view, feet at (cx, yf), s = 1 or 2. big = the inflated belly. m = the
 * springs: the belly and cheeks lag the beat, the pecs bounce, the wings and tail whip. */
static void gg_figure(int cx, int yf, int s, int big, int expr, const GgMo *m)
{
    int belly_r = (big ? 27 : 18) * s, belly_ry = (big ? 22 : 14) * s;
    int body_y = yf - 30 * s, chest_top = body_y - 24 * s + m->bob, chest_bot = body_y - 6 * s + m->bob;
    int belly_cy = chest_bot - 2 * s + belly_ry + m->belly_y, head_y = yf - 60 * s + m->bob * 2 / 3, wy = body_y - 26 * s + m->bob;
    int brx = belly_r - m->belly_y / 3, bry = belly_ry + m->belly_y / 3;
    int hip_y = belly_cy + belly_ry - 6 * s - m->belly_y, by, ny, i;

    gg_wing(cx - 14 * s, wy, s, -1, m->flap);                     /* wings, tail, behind everything */
    gg_wing(cx + 14 * s, wy, s, 1, m->flap);
    gg_tail(cx + 22 * s, yf - 16 * s, s, 1, m->tail);

    gg_legs(cx, yf, s, hip_y, 9 * s);
    gg_blob(cx - 8 * s + m->lx, hip_y + m->ly, 9 * s, 7 * s - m->ly / 4, RAMP_BLOOD);    /* the two hip cheeks, ringing a beat apart */
    gg_blob(cx + 8 * s + m->rx, hip_y + m->ry, 9 * s, 7 * s - m->ry / 4, RAMP_BLOOD);

    /* chest slab, pecs, belly */
    gg_tri(cx - 19 * s - 1, chest_top, cx + 19 * s + 1, chest_top, cx + 15 * s + 1, chest_bot + 1, GG_LINE);
    gg_tri(cx - 19 * s - 1, chest_top, cx + 15 * s + 1, chest_bot + 1, cx - 15 * s - 1, chest_bot + 1, GG_LINE);
    gg_tri(cx - 19 * s, chest_top, cx + 19 * s, chest_top, cx + 15 * s, chest_bot, PAL_RAMP(RAMP_BLOOD, 5));
    gg_tri(cx - 19 * s, chest_top, cx + 15 * s, chest_bot, cx - 15 * s, chest_bot, PAL_RAMP(RAMP_BLOOD, 5));
    gg_tri(cx - 19 * s, chest_top, cx - 19 * s / 3, chest_top, cx - 15 * s, chest_bot, PAL_RAMP(RAMP_BLOOD, 3));
    gg_tri(cx + 19 * s / 3, chest_top, cx + 19 * s, chest_top, cx + 15 * s, chest_bot, PAL_RAMP(RAMP_BLOOD, 6));
    gg_blob(cx - 17 * s + 2, chest_top + 3, 7, 6, RAMP_BLOOD);    /* deltoids */
    gg_blob(cx + 17 * s - 2, chest_top + 3, 7, 6, RAMP_BLOOD);
    gg_blob(cx - 8 * s, chest_top + 8 * s + m->chest, 8 * s + 1, 5 * s, RAMP_BLOOD);   /* pecs bounce a beat after the kick */
    gg_blob(cx + 8 * s, chest_top + 8 * s - m->chest / 2, 8 * s + 1, 5 * s, RAMP_BLOOD);
    plat_line(cx, chest_top + 8 * s - 5 * s, cx, chest_top + 8 * s + 5 * s, GG_LINE);
    gg_blob(cx + m->belly_x, belly_cy, brx, bry, RAMP_PEACH);
    ui_fill_ellipse(cx + m->belly_x, belly_cy + bry / 3, 2, 3, PAL_RAMP(RAMP_PEACH, 3));   /* navel */

    gg_strap(cx - 15 * s, chest_top + 2 * s, cx + 11 * s, chest_bot + 4 * s, s);   /* the X harness */
    gg_strap(cx + 15 * s, chest_top + 2 * s, cx - 11 * s, chest_bot + 4 * s, s);
    ui_fill_circle(cx, chest_top + 10 * s, 3 * s, GG_LINE);
    ui_fill_circle(cx, chest_top + 10 * s, 2 * s, GG_CHAIN);
    by = belly_cy + bry - 5 * s;                                  /* studded belt, brass buckle, rides the belly */
    plat_rect(cx - brx + 2 + m->belly_x, by, 2 * brx - 4, 3 * s, GG_LEATH);
    ui_frame(cx - brx + 2 + m->belly_x, by, 2 * brx - 4, 3 * s, GG_LINE);
    gg_studs(cx - brx + 6 + m->belly_x, by + s, cx + brx - 6 + m->belly_x, by + s, 7);
    plat_rect(cx - 2 * s + m->belly_x, by, 4 * s, 3 * s, GG_STUD);
    ui_frame(cx - 2 * s + m->belly_x, by, 4 * s, 3 * s, GG_LINE);

    gg_arms(cx, chest_top, s, 22 * s, m->chest / 2);

    ny = head_y + 15 * s;                                         /* spiked collar, the gate key */
    plat_rect(cx - 10 * s, ny, 20 * s, 3 * s, GG_LEATH);
    ui_frame(cx - 10 * s, ny, 20 * s, 3 * s, GG_LINE);
    for (i = -3; i <= 3; i++) gg_tri(cx + i * 3 * s - 1, ny, cx + i * 3 * s + 1, ny, cx + i * 3 * s, ny - 3 * s, GG_CHAIN);
    plat_line(cx, ny + 3 * s, cx, ny + 8 * s, GG_CHAIN);
    ui_fill_circle(cx, ny + 10 * s, 2, GG_STUD);

    gg_blob(cx, head_y, 14 * s, 13 * s, RAMP_BLOOD);              /* head, ears, horns, goatee, face */
    gg_horns(cx, head_y, s);
    gg_tri(cx - 3 * s, head_y + 11 * s, cx + 3 * s, head_y + 11 * s, cx, head_y + 17 * s, PAL_RAMP(RAMP_VAPE, 1));
    gg_face(cx, head_y, s, expr);
}

/* Grumbleguts from BEHIND: the camera sits on his rump. The cheeks are two big springy
 * blobs that each take a slap and ring on their own (m->l*, m->r*); the leather G-string
 * strap and studded waistband stay put while the skin wobbles around them. look = he
 * glances back over his shoulder at the lens (wink, smirk). Cartoon only. */
static void gg_rear(int cx, int yf, int s, int big, int expr, const GgMo *m, int look)
{
    int body_y = yf - 30 * s + m->bob, chest_top = body_y - 24 * s, chest_bot = body_y - 6 * s + 4 * s;
    int head_y = yf - 60 * s + m->bob * 2 / 3, wy = body_y - 26 * s;
    int r = (big ? 21 : 15) * s, by = body_y + 4 * s, cyc = by + r / 2;
    int lcx = cx - r * 7 / 10 + m->lx, rcx = cx + r * 7 / 10 + m->rx;
    int lcy = cyc + m->ly, rcy = cyc + m->ry;
    int ny = head_y + 15 * s, i;

    gg_wing(cx - 10 * s, wy, s, -1, m->flap);
    gg_wing(cx + 10 * s, wy, s, 1, m->flap);
    gg_tail(cx + 2 * s, yf - 24 * s, s, 1, m->tail);
    gg_legs(cx, yf, s, body_y + 8 * s, 9 * s);

    gg_blob(lcx, lcy, r * 19 / 20 + (m->ly > 0 ? m->ly / 3 : 0), r - m->ly / 4, RAMP_BLOOD);          /* THE CHEEKS: a slap squashes, then rings */
    gg_blob(rcx, rcy, r * 19 / 20 + (m->ry > 0 ? m->ry / 3 : 0), r - m->ry / 4, RAMP_BLOOD);
    plat_line(cx, by - 2, cx, cyc + r - 2 * s, GG_LINE);                                    /* the crease, held in by... */
    plat_line(cx + 1, by - 2, cx + 1, cyc + r - 2 * s, GG_LINE);
    gg_tri(cx - 4 * s, by, cx + 4 * s, by, cx, by + 11 * s, GG_LINE);                      /* ...a studded leather strap */
    gg_tri(cx - 4 * s + 2, by + 1, cx + 4 * s - 2, by + 1, cx, by + 10 * s, GG_LEATH);
    gg_studs(cx - s, by + 3 * s, cx - s, by + 8 * s, 2);

    gg_tri(cx - 19 * s - 1, chest_top, cx + 19 * s + 1, chest_top, cx + 13 * s + 1, chest_bot, GG_LINE);   /* back */
    gg_tri(cx - 19 * s - 1, chest_top, cx + 13 * s + 1, chest_bot, cx - 13 * s - 1, chest_bot, GG_LINE);
    gg_tri(cx - 19 * s, chest_top, cx + 19 * s, chest_top, cx + 13 * s, chest_bot, PAL_RAMP(RAMP_BLOOD, 5));
    gg_tri(cx - 19 * s, chest_top, cx + 13 * s, chest_bot, cx - 13 * s, chest_bot, PAL_RAMP(RAMP_BLOOD, 5));
    gg_blob(cx - 10 * s, chest_top + 9 * s + m->chest, 8 * s, 6 * s, RAMP_BLOOD);          /* shoulder blades */
    gg_blob(cx + 10 * s, chest_top + 9 * s - m->chest / 2, 8 * s, 6 * s, RAMP_BLOOD);
    plat_line(cx, chest_top + 4 * s, cx, chest_bot, PAL_RAMP(RAMP_BLOOD, 3));               /* spine */
    gg_strap(cx - 15 * s, chest_top + 2 * s, cx + 10 * s, chest_bot + 2 * s, s);
    gg_strap(cx + 15 * s, chest_top + 2 * s, cx - 10 * s, chest_bot + 2 * s, s);
    plat_rect(cx - 17 * s, by - 2 * s, 34 * s, 3 * s, GG_LEATH);                           /* waistband, sits above the cheeks */
    ui_frame(cx - 17 * s, by - 2 * s, 34 * s, 3 * s, GG_LINE);
    gg_studs(cx - 14 * s, by - s, cx + 14 * s, by - s, 7);

    gg_arms(cx, chest_top, s, 22 * s, m->chest);

    plat_rect(cx - 10 * s, ny, 20 * s, 3 * s, GG_LEATH);                                    /* collar from behind */
    ui_frame(cx - 10 * s, ny, 20 * s, 3 * s, GG_LINE);
    for (i = -3; i <= 3; i++) gg_tri(cx + i * 3 * s - 1, ny, cx + i * 3 * s + 1, ny, cx + i * 3 * s, ny - 3 * s, GG_CHAIN);

    gg_blob(cx, head_y, 14 * s, 13 * s, RAMP_BLOOD);
    gg_horns(cx, head_y, s);
    if (look) {                                                   /* a glance back over the shoulder */
        int fx = cx + 8 * s, ex = fx + 2 * s;
        ui_fill_ellipse(fx, head_y + s, 8 * s + 1, 11 * s + 1, GG_LINE);
        ui_fill_ellipse(fx, head_y + s, 8 * s, 11 * s, PAL_RAMP(RAMP_BLOOD, 5));
        if (expr == GG_WINK || expr == GG_LOVE) {
            plat_line(ex - 3 * s, head_y - 2 * s, ex + 3 * s, head_y - 2 * s + 1, GG_LINE);
            plat_line(ex - 3 * s, head_y - 2 * s + 1, ex + 3 * s, head_y - 2 * s + 2, GG_LINE);
        } else {
            ui_fill_ellipse(ex, head_y - 2 * s, 3 * s + 1, 2 * s + 1, GG_LINE);
            ui_fill_ellipse(ex, head_y - 2 * s, 3 * s, 2 * s, PAL_RAMP(RAMP_LAVA, 5));
            ui_fill_circle(ex - s, head_y - 2 * s, s + 1, PAL_RAMP(RAMP_STONE, 0));
        }
        plat_line(ex - 4 * s, head_y - 6 * s + 2, ex + 4 * s, head_y - 6 * s - 1, GG_LINE);     /* a cocked brow */
        plat_line(ex - 4 * s, head_y - 6 * s + 3, ex + 4 * s, head_y - 6 * s, GG_LINE);
        ui_fill_ellipse(fx + s, head_y + 6 * s, 6 * s + 1, 3 * s + 1, GG_LINE);               /* the smirk, fangs and all */
        ui_fill_ellipse(fx + s, head_y + 6 * s, 6 * s, 3 * s, PAL_RAMP(RAMP_FLESH, 1));
        ui_fill_ellipse(fx + s, head_y + 4 * s, 6 * s + 1, 2 * s, PAL_RAMP(RAMP_BLOOD, 5));
        for (i = -1; i <= 1; i += 2) gg_tri(fx + s + i * 3 * s - 2, head_y + 4 * s, fx + s + i * 3 * s + 2, head_y + 4 * s, fx + s + i * 3 * s, head_y + 7 * s, PAL_RAMP(RAMP_BONE, 7));
        ui_fill_ellipse(ex + 4 * s, head_y + 3 * s, 3 * s, 2 * s, PAL_RAMP(RAMP_CANDY, 6));      /* blush */
    }
}

/* Grumbleguts in PROFILE, facing right (dir = 1) or left (dir = -1): the belly out front
 * rides the beat (m->belly_x / belly_y), the rump behind it rings on its own (m->ly). The
 * geometry is mirrored by dir so the overlays sit on the same body parts either way. */
static void gg_profile(int cx, int yf, int s, int big, int expr, const GgMo *m, int dir)
{
    int belly_r = (big ? 27 : 18) * s, belly_ry = (big ? 22 : 14) * s, butt_r = (big ? 19 : 13) * s;
    int body_y = yf - 30 * s + m->bob, chest_top = body_y - 24 * s, chest_bot = body_y - 6 * s, wy = body_y - 26 * s;
    int belly_cy = chest_bot - 2 * s + belly_ry + m->belly_y / 2, head_y = yf - 60 * s + m->bob * 2 / 3;
    int bx = cx + dir * (6 * s + m->belly_x * 2), butx = cx - dir * (10 * s) - dir * m->lx;
    int buty = body_y + 4 * s + m->ly, hx = cx + dir * 4 * s, by, side, c, i, ex, ey, mx, my;

    gg_wing(cx - dir * 6 * s, wy, s, -dir, m->flap);
    gg_tail(cx - dir * 14 * s, yf - 22 * s, s, -dir, m->tail);

    for (side = -1; side <= 1; side += 2) {                       /* two legs, one a stride ahead */
        int lx = cx + side * 5 * s - 2 * s * dir + (side == dir ? 2 * s : 0);
        gg_blob(lx, (body_y + 8 * s + yf - 8 * s) / 2, 7 * s, (yf - 8 * s - body_y - 8 * s) / 2 + 4 * s, RAMP_BLOOD);
        gg_blob(lx + dir * s, yf - 7 * s, 6 * s, 7 * s, RAMP_BLOOD);
        for (c = -1; c <= 1; c++) gg_tri(lx + c * 3 * s - 2, yf - 2 * s, lx + c * 3 * s + 2, yf - 2 * s, lx + c * 3 * s, yf + 2, GG_HORN);
    }
    gg_blob(butx, buty + butt_r / 2 - s, butt_r + m->ly / 3, butt_r - 2 * s - m->ly / 4, RAMP_BLOOD);   /* the rump behind, wobbling on its own */
    plat_line(butx + dir * butt_r / 3, body_y + s, butx + dir * butt_r / 3, buty + butt_r - 2 * s, GG_LINE);
    gg_blob(cx + dir * 2 * s, (chest_top + chest_bot) / 2 + m->chest, 12 * s, 13 * s, RAMP_BLOOD);       /* chest, leaning in */
    gg_blob(cx + dir * 9 * s, chest_top + 8 * s + m->chest, 8 * s, 5 * s, RAMP_BLOOD);                  /* pec */
    gg_blob(bx, belly_cy, belly_r - 2 * s - m->belly_y / 3, belly_ry + m->belly_y / 3, RAMP_PEACH);     /* the belly, out front */
    ui_fill_ellipse(bx + dir * (belly_r - 5 * s), belly_cy + 2 * s, 2, 2, PAL_RAMP(RAMP_PEACH, 3));      /* navel in profile */
    gg_strap(cx - dir * 4 * s, chest_top + 2 * s, cx + dir * 10 * s, chest_bot + 2 * s, s);
    by = belly_cy + belly_ry - 5 * s;                             /* the belt runs belly to rump */
    {
        int ax = DOOMO_MIN(butx - dir * butt_r, bx + dir * (belly_r - 2 * s)), bx2 = DOOMO_MAX(butx - dir * butt_r, bx + dir * (belly_r - 2 * s));
        plat_rect(ax + 2, by, bx2 - ax - 4, 3 * s, GG_LEATH);
        ui_frame(ax + 2, by, bx2 - ax - 4, 3 * s, GG_LINE);
        gg_studs(ax + 6, by + s, bx2 - 6, by + s, 6);
    }

    {   /* the near arm, hanging forward */
        int ax = cx + dir * 8 * s, ay = chest_top + 10 * s + m->chest, fx = ax + dir * 3 * s, fy = ay + 12 * s;
        gg_blob(ax, ay, 6 * s, 9 * s, RAMP_BLOOD);
        gg_blob(fx, fy, 5 * s, 5 * s, RAMP_BLOOD);
        plat_rect(fx - 5 * s, fy - 7 * s, 10 * s, 2 * s, GG_LEATH);
        for (c = -1; c <= 1; c++) gg_tri(fx + c * 3 * s - 1, fy + 4 * s, fx + c * 3 * s + 1, fy + 4 * s, fx + c * 3 * s, fy + 6 * s + 1, GG_HORN);
    }

    plat_rect(hx - 6 * s, head_y + 14 * s, 12 * s, 3 * s, GG_LEATH);                       /* collar */
    for (i = -2; i <= 2; i++) gg_tri(hx + i * 3 * s - 1, head_y + 14 * s, hx + i * 3 * s + 1, head_y + 14 * s, hx + i * 3 * s, head_y + 11 * s, GG_CHAIN);
    gg_blob(hx, head_y, 13 * s, 12 * s, RAMP_BLOOD);                                       /* head */
    gg_blob(hx + dir * 9 * s, head_y + 4 * s, 7 * s, 6 * s, RAMP_BLOOD);                   /* muzzle */
    gg_tri(hx - dir * 4 * s, head_y - 8 * s, hx - dir * 14 * s, head_y - 20 * s, hx + dir * 2 * s, head_y - 10 * s, GG_LINE);   /* horn, swept back */
    gg_tri(hx - dir * 4 * s, head_y - 8 * s, hx - dir * 13 * s, head_y - 19 * s, hx + dir * 1 * s, head_y - 10 * s, GG_HORN);
    gg_tri(hx - dir * 12 * s, head_y - s, hx - dir * 19 * s, head_y - 6 * s, hx - dir * 9 * s, head_y - 7 * s, GG_LINE);       /* ear */
    ex = hx + dir * 7 * s; ey = head_y - 2 * s;
    if (expr == GG_WINK || expr == GG_LOVE) {
        plat_line(ex - 2 * s, ey, ex + 2 * s, ey + 1, GG_LINE);
        plat_line(ex - 2 * s, ey + 1, ex + 2 * s, ey + 2, GG_LINE);
    } else {
        ui_fill_ellipse(ex, ey, 3 * s + 1, 2 * s + 1, GG_LINE);
        ui_fill_ellipse(ex, ey, 3 * s, 2 * s, PAL_RAMP(RAMP_LAVA, 5));
        ui_fill_circle(ex + dir * s, ey, s + 1, PAL_RAMP(RAMP_STONE, 0));
    }
    plat_line(ex - 3 * s, ey - 4 * s + 2, ex + 3 * s, ey - 4 * s - 1, GG_LINE);              /* brow */
    plat_line(ex - 3 * s, ey - 4 * s + 3, ex + 3 * s, ey - 4 * s, GG_LINE);
    mx = hx + dir * 11 * s; my = head_y + 6 * s;
    ui_fill_ellipse(mx - dir * s, my, 6 * s + 1, 3 * s + 1, GG_LINE);                       /* the grin, fangs out */
    ui_fill_ellipse(mx - dir * s, my, 6 * s, 3 * s, PAL_RAMP(RAMP_FLESH, expr == GG_BLUSH ? 1 : 2));
    for (i = -1; i <= 1; i++) gg_tri(mx - dir * s + i * 3 * s - 2, my - 2 * s, mx - dir * s + i * 3 * s + 2, my - 2 * s, mx - dir * s + i * 3 * s, my + s, PAL_RAMP(RAMP_BONE, 7));
    ui_fill_ellipse(hx + dir * 3 * s, head_y + 4 * s, 3 * s, 2 * s, PAL_RAMP(RAMP_CANDY, 6));   /* blush */
    gg_tri(mx - 3 * s, my + 3 * s, mx + 3 * s, my + 3 * s, mx, my + 9 * s, PAL_RAMP(RAMP_VAPE, 1));   /* goatee */
}

static void shout(int x, int y, const char *s, uint8_t c)
{
    int w = ui_text_width(s), hi = plat_screen_w() - w - 2;
    ui_text_outline(DOOMO_CLAMP(x, 2, DOOMO_MAX(hi, 2)), y, s, c, PAL_BLACK);
}

/* a hand (an imp's, a guest's) coming in to slap or boop. fingers point toward dir */
static void gg_hand(int x, int y, int s, int dir, uint8_t col, uint8_t cuff)
{
    int i;
    plat_rect(DOOMO_MIN(x - dir * 14 * s, x - dir * 5 * s), y - 3 * s, 9 * s, 6 * s, cuff);
    ui_fill_ellipse(x, y, 5 * s + 1, 4 * s + 1, GG_LINE);
    ui_fill_ellipse(x, y, 5 * s, 4 * s, col);
    for (i = 0; i < 4; i++) {
        int fy = y - 3 * s + i * 2 * s;
        ui_fill_ellipse(x + dir * 5 * s, fy, 3 * s + 1, s + 1, GG_LINE);
        ui_fill_ellipse(x + dir * 5 * s, fy, 3 * s, s, col);
    }
}

/* comic impact star */
static void gg_burst(int x, int y, int r, uint8_t c)
{
    static const int8_t dx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 }, dy[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    int i;
    for (i = 0; i < 8; i++) {
        int d = (i & 1) ? r * 7 / 10 : r;
        plat_line(x + dx[i] * d / 2, y + dy[i] * d / 2, x + dx[i] * d, y + dy[i] * d, c);
        plat_line(x + dx[i] * d / 2 + 1, y + dy[i] * d / 2, x + dx[i] * d + 1, y + dy[i] * d, c);
    }
}

/* the full-screen cut-ins (see gg_cuts): a spotlight, a hard cut, and the camera goes
 * where the night wants it: onto his rump (BACK / LOOK) or square on the belly (SIDE).
 * Every slap or boop lands on the same tick the springs are kicked (gg_motion), so the
 * hand, the SMACK!, the flush on the skin and the ripple all agree. */
static void gg_cut_shot(int panel, uint32_t pt, uint32_t ct, const GgCut *cut)
{
    static const char *const smack[4] = { "SMACK!", "WHAP!", "CLAP!", "SPLAT~" };
    static const char *const back_say[6] = { "hnnnf~", "oooh~", "page one~", "next~", "don't stop~", "mmm~" };
    static const char *const look_say[6] = { "...see something?", "like the view?", "Shriekra~", "front of the line~", "your turn~", "brunch?" };
    int sw = plat_screen_w(), sh = plat_screen_h(), t = (int)pt, k = t - cut->a, i;
    int s0 = sh >= 200 ? 2 : 1, s = s0 + 1, dawn = panel == 19;
    int big = panel == 14 || panel == 15 || panel == 18, rear = cut->view == GG_V_BACK || cut->view == GG_V_LOOK;
    int every = gg_every(panel), h0 = cut->a + 4, nn, np, dtn, dtp, handed;
    int expr = (rear && cut->view == GG_V_LOOK) ? ((t / 24) & 1 ? GG_WINK : GG_SMUG)
             : panel == 14 ? GG_BLUSH : panel == 15 ? GG_LOVE : panel == 18 ? (((t / 20) & 1) ? GG_LOVE : GG_BLUSH) : (((t / 16) & 1) ? GG_WINK : GG_GRIN);
    int cx = sw / 2 + ui_wave((int)ct * 7, 14, rear ? (panel >= 17 ? s : s / 2 + 1) : 0), yf, cheeky = sh * 72 / 100;
    int r = (big ? 21 : 15) * s;
    GgMo m;

    if (rear) yf = cheeky + 26 * s - r / 2; else yf = sh - 30;
    gg_motion(&m, panel, t, ct, s, cut);

    /* hard cut: two white frames, then the lit stage */
    if (k < 2) { plat_rect(0, 0, sw, sh, PAL_WHITE); return; }
    plat_rect(0, 0, sw, sh, dawn ? PAL_RAMP(RAMP_PASTEL_PINK, 5) : PAL_RAMP(RAMP_VAPE, 0));
    for (i = 3; i >= 1; i--)
        ui_fill_ellipse(sw / 2, sh * 55 / 100, sw * (2 + i) / 8, sh * (2 + i) / 6, dawn ? PAL_RAMP(RAMP_PASTEL_PINK, 5 + (3 - i) / 2 + 1) : PAL_RAMP(RAMP_VAPE, 4 - i));
    plat_rect(0, yf - 8, sw, sh - yf + 8, PAL_RAMP(RAMP_GRASS, dawn ? 3 : 1));
    for (i = 0; i < 10; i++) ui_sparkle((i * 53 + 20) % sw, 8 + (i * 29) % (sh / 3), (int)((ct / 2 + (uint32_t)i * 3) & 15), dawn ? PAL_UI_GOLD : PAL_WHITE);

    /* which hit is coming up, and which one has just landed */
    nn = t <= h0 ? 0 : (t - h0 + every - 1) / every;
    dtn = h0 + nn * every - t;
    np = t >= h0 ? (t - h0) / every : -1;
    dtp = np >= 0 ? t - (h0 + np * every) : 99;
    handed = rear && panel != 14;                            /* the valve panel has no slapper: the pulses do it */

    if (rear) {
        int lcx = cx - r * 7 / 10 + m.lx, rcx = cx + r * 7 / 10 + m.rx, cyc = yf - 26 * s + r / 2;
        int hit, hs, hx, hy, vy;
        gg_rear(cx, yf, s, big, expr, &m, cut->view == GG_V_LOOK);
        vy = cyc - r / 2 + 9 * s;
        if (panel == 14) {                                   /* the valve in the crease, and the hiss that comes out of it */
            plat_rect(cx - 6, vy, 12, 9, PAL_BLACK);
            plat_rect(cx - 5, vy + 1, 10, 7, PAL_UI_GOLD);
            for (i = 0; i < 5; i++) {
                int u = (int)((ct * 3 + (uint32_t)i * 9) % 60u);
                gas(cx + u * (i & 1 ? 1 : -1) / 2, vy + 8 + u / 2 + ui_wave((int)ct * 5 + i * 7, 10, 3), 3 + u / 12, 0);
            }
            if ((t % 22) < 12) shout(cx - 34, vy + 24 * s, "hissssss~", PAL_WHITE);
        }
        /* the slap: a hand sweeps in, lands on a cheek, rings it */
        if (handed) {
            for (hit = 0; hit < 2; hit++) {
                int n = hit ? np : nn, d = hit ? dtp : dtn;
                if (n < 0 || d > 5) continue;
                hs = (n & 1) ? 1 : -1;
                hx = (hs < 0 ? lcx : rcx) + hs * (r / 4 + d * 11 * s / 2);
                hy = (hs < 0 ? cyc + m.ly : cyc + m.ry) - r / 4 - (hit ? d * 2 : 0);
                gg_hand(hx, hy, s, -hs, PAL_RAMP(RAMP_PEACH, 4 + (n % 3)), PAL_PAINT((n * 7 + 3) % PAL_PAINT_COUNT, 1));
            }
        }
        if (np >= 0 && handed) {
            hs = (np & 1) ? 1 : -1;
            hx = hs < 0 ? lcx : rcx;
            hy = (hs < 0 ? cyc + m.ly : cyc + m.ry) - r / 4;
            if (dtp < every - 2) {                           /* the flush: a pink print that cools off */
                uint8_t pc = dtp < 18 ? PAL_RAMP(RAMP_CANDY, 6) : PAL_RAMP(RAMP_CANDY, 4);
                ui_fill_ellipse(hx - hs * r / 5, hy, 4 * s, 3 * s, pc);
                for (i = 0; i < 3; i++) ui_fill_ellipse(hx - hs * (r / 5 + 5 * s), hy - 3 * s + i * 3 * s, 3 * s, s, pc);
            }
            if (dtp < 7) {
                gg_burst(hx + hs * r / 3, hy, 8 * s + (6 - dtp) * 2, dtp < 3 ? PAL_WHITE : PAL_UI_GOLD);
                if (sh >= 200) ui_text_big(DOOMO_CLAMP(hx - 30, 4, sw - 100), hy - r - 6 * s - dtp, smack[np & 3], 2, PAL_WHITE, PAL_UI_RED_D);
                else shout(hx - 20, hy - r - 6, smack[np & 3], PAL_WHITE);
            }
            for (i = 0; i < 3; i++)                          /* hearts pop off every hit */
                if (dtp < 22) ui_heart(hx + (i - 1) * 12 * s + hs * 6, hy - r / 2 - dtp * 3 - i * 5 * s, 2 + (i & 1), PAL_UI_PINK);
        }
        for (i = 0; i < 2; i++) {                            /* shimmy lines hugging a ringing cheek */
            int cyl = i ? m.ry : m.ly, cxx = i ? rcx + r + 2 : lcx - r - 2, sg = i ? 1 : -1, j;
            if (cyl > 2 || cyl < -2) for (j = 0; j < 3; j++)
                plat_line(cxx + sg * 2, cyc + (i ? m.ry : m.ly) - 8 * s + j * 6 * s, cxx + sg * (6 + j * 3), cyc + (i ? m.ry : m.ly) - 8 * s + j * 6 * s, PAL_WHITE);
        }
        ui_sparkle(lcx - r / 2, cyc - r / 2 + m.ly, (int)((ct / 2) & 15), PAL_WHITE);              /* glistening */
        ui_sparkle(rcx + r / 2, cyc - r / 2 + m.ry, (int)((ct / 2 + 7) & 15), PAL_WHITE);
        if (cut->view == GG_V_LOOK) {
            if (((t / 26) & 1)) shout(cx + 26 * s, yf - 56 * s, look_say[panel - 14], PAL_UI_PINK);
        } else if ((t % 30) < 18 && panel != 14) {
            shout(cx - 70, yf - 62 * s - 8, back_say[panel - 14], PAL_UI_PINK);
        }
        if (panel == 14 && cut->view == GG_V_LOOK && t > 190) shout(cx - 24, cheeky - r - 10 * s, "BLORP", PAL_UI_GOLD);
        if (panel == 18 && ((t / 30) & 1)) shout(4, 6, "Mavis~", PAL_UI_PINK);
        if (panel == 18 && !((t / 30) & 1)) shout(4, 6, "Gary~?", PAL_UI_PINK);
    } else {
        /* PROFILE: belly out front, a hand booping it, the whole body ringing */
        int dir = 1, belly_r = (big ? 27 : 18) * s, belly_ry = (big ? 22 : 14) * s;
        int body_y = yf - 30 * s + m.bob, bcy = body_y - 6 * s - 2 * s + belly_ry + m.belly_y / 2;
        int fx = cx + dir * (6 * s + m.belly_x * 2) + dir * (belly_r - 2 * s), hx = cx + 4 * s + 11 * s, hy = yf - 60 * s - 3 * s;
        gg_profile(cx, yf, s, big, expr, &m, dir);
        if (np >= 0 && dtp < 12)                             /* ripple rings run out from every hit */
            for (i = 1; i <= 3; i++)
                if (dtp > i) ui_fill_ellipse(fx - dir * (dtp + i * 3), bcy, 1 + i, 4 * s + i * 2, PAL_RAMP(RAMP_PEACH, 4 + (i & 1)));
        if (panel == 15 || panel == 17) {                    /* an imp / a ticket-holder boops the belly */
            for (i = 0; i < 2; i++) {
                int n = i ? np : nn, d = i ? dtp : dtn;
                if (n < 0 || d > 5) continue;
                gg_hand(fx + dir * (6 * s + d * 12 * s / 2), bcy + (n & 1 ? -s : s) * 3, s, -dir, PAL_RAMP(RAMP_PEACH, 4 + (n % 3)), PAL_PAINT((n * 7 + 3) % PAL_PAINT_COUNT, 1));
            }
            if (np >= 0 && dtp < 8) shout(fx - 30, bcy - belly_ry - 14 - dtp, (np & 1) ? "squelch~" : "boop!", PAL_UI_GOLD);
        }
        if (panel == 15) {                                   /* BRRRAAAP: it leaves his mouth and crosses the park */
            for (i = 0; i < 6; i++) {
                int u = (int)((ct * 4 + (uint32_t)i * 13) % 90u);
                gas(hx + 14 * s + u, hy + 10 * s + ui_wave((int)ct * 3 + i * 5, 16, 4), 5 + u / 12, 1);
            }
            if (k > 4) shout(hx + 10 * s, hy - 24, "BRRRAAAAP", PAL_UI_GOLD);
        }
        if (panel == 14) {                                   /* the valve whistles at his hip */
            for (i = 0; i < 6; i++) {
                int u = (int)((ct * 3 + (uint32_t)i * 11) % 70u);
                gas(fx - 10 * s + u, bcy - 6 * s - u / 4 + ui_wave((int)ct * 4 + i * 7, 12, 4), 4 + u / 14, 0);
            }
            plat_rect(fx - 6, bcy - 10, 8, 9, PAL_BLACK);
            plat_rect(fx - 5, bcy - 9, 6, 7, PAL_UI_GOLD);
            shout(cx - 30 * s, hy - 26, "gurrrgle~", PAL_UI_PINK);
        }
        if (panel == 18) {                                   /* the pump: a hose from the valve, the handle hammering */
            int stroke = ui_wave((int)ct * 6, 12, 5);
            plat_line(fx - 2, bcy, sw - 30, yf - 20, PAL_RAMP(RAMP_MINT, 6));
            plat_line(fx - 2, bcy + 1, sw - 30, yf - 19, PAL_RAMP(RAMP_MINT, 3));
            plat_rect(sw - 34, yf - 22, 6, 24, PAL_BLACK);
            plat_rect(sw - 40, yf - 28 + stroke, 14, 3, PAL_BLACK);
            if ((t % 10) > 4) shout(sw - 70, yf - 54, "pff!", PAL_WHITE);
        }
        for (i = 0; i < 3; i++) ui_heart(cx - 30 * s + i * 30 * s + ui_wave((int)ct * 2 + i * 9, 20, 4), yf - 100 * s / 2 - (int)((ct * 2 + (uint32_t)i * 17) % 40u), 3, PAL_UI_PINK);
        if ((t % 34) < 16) shout(hx - 26 * s, hy - 14 * s, panel == 17 ? "next~" : panel == 18 ? "harder~" : "mmh~", PAL_UI_PINK);
    }
}

static void night_scene(int panel, uint32_t pt, uint32_t ct)
{
    static const char *const names[5] = { "Shriekra~", "Spearmint~", "the waiter~", "the band~", "the gazebo~" };
    static const char *const wrong[4] = { "oh, Gary~", "yes, Mavis~", "...the Gate?", "Steve~~" };
    static const char *const nums[3] = { "No. 46", "No. 47", "No. 48" };
    int sw = plat_screen_w(), sh = plat_screen_h(), gy = sh * 3 / 4, i, k, t = (int)pt;
    int s = sh >= 200 ? 2 : 1, dawn = panel == 19, big = panel == 14 || panel == 15 || panel == 18;
    int shake = ui_wave((int)ct * 6, 8, (panel == 14 || panel == 18) ? 3 : 1) * (big ? 1 : 0);
    int cx = sw / 2 + shake + ui_wave((int)ct * 5, 10, panel >= 17 && panel <= 18 ? 2 : 1), yf = gy - 4;
    int r = (big ? 27 : 18) * s, ry = (big ? 22 : 14) * s;
    int cy = yf - 30 * s - 6 * s - 2 * s + ry;                   /* belly centre, as in gg_figure() */
    int hx = cx, hy = yf - 60 * s, ax = cx + r + 2 * s, ay = cy - 6 * s;
    int poke = panel == 15 ? ((t % 28) < 10 ? (t % 28) : 0) : 0;
    const GgCut *cut = gg_cut_at(panel, pt);
    GgMo mo;
    int expr = panel == 14 ? GG_BLUSH : panel == 15 ? GG_LOVE : panel == 16 ? GG_SMUG
             : panel == 17 ? (((t / 40) & 1) ? GG_WINK : GG_GRIN) : panel == 18 ? (((t / 50) & 1) ? GG_LOVE : GG_BLUSH) : GG_SMUG;

    if (cut) { gg_cut_shot(panel, pt, ct, cut); return; }       /* rump-cam / profile cut-in */
    gg_motion(&mo, panel, t, ct, s, 0);

    /* the sky (a night park, or the pink morning after), the table, the stragglers */
    plat_rect(0, 0, sw, gy, dawn ? PAL_RAMP(RAMP_PASTEL_PINK, 5) : PAL_RAMP(RAMP_VAPE, 0));
    plat_rect(0, gy, sw, sh - gy, PAL_RAMP(RAMP_GRASS, dawn ? 3 : 1));
    if (dawn) {
        ui_fill_circle(sw - 110, gy - 6, 22, PAL_UI_GOLD);
    } else {
        ui_fill_circle(sw - 46, 40, 16, PAL_RAMP(RAMP_BONE, 7));
        ui_fill_circle(sw - 40, 36, 14, PAL_RAMP(RAMP_VAPE, 0));
        for (i = 0; i < 9; i++) ui_sparkle((i * 47) % sw, 14 + (i * 23) % 50, (int)((ct / 3 + (uint32_t)i * 3) & 15), PAL_WHITE);
    }
    plat_rect(8, gy - 26, 70, 6, PAL_RAMP(RAMP_WOOD, 3));
    plat_rect(12, gy - 20, 4, 22, PAL_RAMP(RAMP_WOOD, 1));
    plat_rect(68, gy - 20, 4, 22, PAL_RAMP(RAMP_WOOD, 1));
    if (!dawn) candles(18, gy - 26, ct);

    gg_figure(cx, yf, s, big, expr, &mo);
    {   /* every beat throws sweat and a heart, and a speed arc shows which way the hips went */
        int kt = t % gg_every(panel);
        if (kt < 7) {
            ui_heart(cx + 22 * s + kt * 3, yf - 62 * s - kt * 3, 2 + (kt < 3), PAL_UI_PINK);
            ui_fill_circle(cx - 20 * s - kt * 2, yf - 52 * s + kt, 1, PAL_RAMP(RAMP_SKY, 7));
            if (kt < 3) { plat_line(cx - 30 * s, yf - 24 * s, cx - 36 * s, yf - 24 * s, PAL_WHITE); plat_line(cx + 30 * s, yf - 24 * s, cx + 36 * s, yf - 24 * s, PAL_WHITE); }
        }
    }

    if (panel == 14) {
        /* the valve whistles at his hip, a rolling train of gas puffs leaves it */
        for (i = 0; i < 7; i++) {
            int u = (int)((ct * 3 + (uint32_t)i * 11) % 80u);
            gas(ax + 8 + u, ay - 4 + ui_wave((int)ct * 4 + i * 7, 12, 5) - u / 8, 4 + u / 14, 0);
        }
        plat_rect(ax - 2, ay - 4, 8, 9, PAL_BLACK);
        plat_rect(ax - 1, ay - 3, 6, 7, PAL_UI_GOLD);                   /* the brass valve */
        for (i = 0; i < 3; i++) ui_fill_ellipse(cx - r / 3 + i * 14 * s / 2, cy + r / 4 + ui_wave((int)ct * 5 + i * 9, 10, 3), 5, 3, PAL_RAMP(RAMP_LAVA, 6));   /* gurgling */
        if ((t % 32) > 4 && (t % 32) < 26) shout(ax - 6, ay - 34 - (t % 32) / 3, "hissssss~", PAL_WHITE);
        if (t > 30 && (t % 36) > 18) shout(cx - r - 20, cy + r / 6, "gurrrgle", PAL_UI_PINK);
        if (t > 40 && (t % 40) < 20) shout(hx - 60, hy - 6, "mmmh~", PAL_UI_PINK);
        for (i = 0; i < 3; i++)                                         /* sweat, glistening */
            ui_fill_circle(hx + 12 * s + i * 4, hy - 4 * s + (int)((ct + (uint32_t)i * 7) % 24u), 1, PAL_RAMP(RAMP_SKY, 7));
        if (t > 110) {                                                  /* a bubble climbs his throat */
            int by = hy + 12 * s + (t - 110) * 18 / 80;
            ui_fill_circle(hx, DOOMO_MIN(by, hy + 18 * s), 3 + (t - 110) / 30, PAL_RAMP(RAMP_SKY, 6));
        }
        if (t > 170) shout(hx - 30, hy - 40, "BLORP", PAL_UI_GOLD);
    } else if (panel == 15) {
        /* the burp rolls out of his mouth and across the park */
        for (i = 0; i < 6; i++) {
            int u = (int)((ct * 4 + (uint32_t)i * 13) % 90u);
            if (t > 6) gas(hx - 18 * s - u, hy + 8 * s + ui_wave((int)ct * 3 + i * 5, 16, 4), 5 + u / 12, 1);
        }
        if (t > 8 && t < 70) shout(hx - 90, hy - 20, "BRRRAAAAP", PAL_UI_GOLD);
        /* an imp's finger pokes the belly; it ripples and answers */
        plat_rect(0, gy - 6, 18, 8, PAL_RAMP(RAMP_BLOOD, 2));            /* the brave imp */
        plat_line(10, cy + r / 4, cx - r + 6 + poke * 2, cy + r / 4, PAL_UI_RED);
        plat_line(10, cy + r / 4 + 1, cx - r + 6 + poke * 2, cy + r / 4 + 1, PAL_UI_RED);
        if (poke) {
            for (i = 1; i < 4; i++) ui_fill_ellipse(cx - r + 8, cy + r / 4, poke * i, 3 + poke, PAL_RAMP(RAMP_PEACH, 3 + i));
            shout(cx - r - 14, cy + r / 4 - 18, "squelch~", PAL_UI_PINK);
        }
        for (i = 0; i < 4; i++)                                          /* the crowd is not subtle */
            ui_heart(30 + i * 52 + ui_wave((int)ct + i * 11, 30, 4), gy - 70 - (int)((ct + (uint32_t)i * 17) % 40u), 3, PAL_UI_PINK);
        if (t > 80 && (t % 30) < 16) shout(hx + 40, hy - 10, "harder~", PAL_UI_PINK);
    } else if (panel == 16) {
        /* the little black book: pages flip, names pop out as hearts */
        int bx = cx - 20 * s, by = cy - 6 * s, flip = (t / 14) & 1;
        plat_rect(bx, by, 40, 28, PAL_BLACK);
        plat_rect(bx + 2, by + 2, 17 + flip, 24, PAL_RAMP(RAMP_BONE, 6));
        plat_rect(bx + 21 + flip, by + 2, 17 - flip, 24, PAL_RAMP(RAMP_BONE, 5));
        for (k = 0; k < 4; k++) {                                        /* scribbled names */
            plat_rect(bx + 5, by + 6 + k * 5, 10 + (k * 5 + t / 14) % 7, 1, PAL_BLACK);
            plat_rect(bx + 24, by + 6 + k * 5, 8 + (k * 3 + t / 14) % 9, 1, PAL_BLACK);
        }
        plat_rect(bx + 30, by - 6, 3, 10, PAL_RAMP(RAMP_BLOOD, 3));      /* the pencil */
        shout(hx - 90, hy - 4, names[(t / 40) % 5], PAL_UI_PINK);
        for (i = 0; i < 5; i++)
            ui_heart(bx + 6 + i * 9 + ui_wave((int)ct + i * 7, 12, 3), by - 10 - (int)((ct + (uint32_t)i * 13) % 50u), 2 + (i & 1), PAL_UI_PINK);
        if (t > 110 && (t % 18) < 9) shout(hx + 40, hy, "mmm~", PAL_UI_PINK);
    } else if (panel == 17) {
        /* the queue: figures shuffle forward along the grass, a ticket machine dings */
        for (i = 0; i < 8; i++) {
            int px = sw - 14 - i * 20 - (int)(ct / 40u % 2u) * 2, py = gy - 24 + ui_wave((int)ct * 2 + i * 6, 12, 2);
            ui_fill_circle(px, py - 12, 4, PAL_RAMP(RAMP_PEACH, 5));
            plat_rect(px - 4, py - 8, 9, 14, PAL_PAINT((i * 7 + 3) % PAL_PAINT_COUNT, 1));
            if (((i + t / 20) & 3) == 0) ui_heart(px, py - 24 - (int)((ct + (uint32_t)i * 9) % 12u), 2, PAL_UI_PINK);
        }
        plat_rect(sw - 62, gy - 58, 50, 22, PAL_BLACK);
        plat_rect(sw - 60, gy - 56, 46, 18, PAL_RAMP(RAMP_BONE, 6));
        shout(sw - 58, gy - 54, nums[DOOMO_MIN(t / 70, 2)], PAL_UI_RED);
        if ((t % 70) > 4 && (t % 70) < 24) shout(sw - 76, gy - 74, "DING!", PAL_UI_GOLD);
        if (t > 100 && (t % 50) < 25) shout(hx - 90, hy, "next~", PAL_UI_PINK);
    } else if (panel == 18) {
        /* the pump goes down the line: a new pair of hands every few strokes */
        int stroke = ui_wave((int)ct * 7, 20, 6), who = (t / 40) % 4;
        plat_line(ax + 4, ay, sw - 40, gy - 22, PAL_RAMP(RAMP_MINT, 6));
        plat_line(ax + 4, ay + 1, sw - 40, gy - 21, PAL_RAMP(RAMP_MINT, 3));
        plat_rect(sw - 42, gy - 24, 6, 26, PAL_BLACK);
        plat_rect(sw - 46, gy - 30 + stroke, 14, 3, PAL_BLACK);          /* the handle */
        ui_fill_circle(sw - 39, gy - 40, 4, PAL_RAMP(RAMP_PEACH, 5));    /* this pair of hands */
        plat_rect(sw - 44, gy - 36, 10, 8, PAL_PAINT((who * 9 + 5) % PAL_PAINT_COUNT, 1));
        if ((t % 20) > 10 && (t % 20) < 18) shout(sw - 62, gy - 60, "pff!", PAL_WHITE);
        if ((t % 40) > 8 && (t % 40) < 34) shout(hx - 100, hy - 6, wrong[who], PAL_UI_PINK);
        for (i = 0; i < 5; i++)
            ui_heart(20 + i * 50 + ui_wave((int)ct + i * 11, 26, 5), gy - 90 - (int)((ct + (uint32_t)i * 19) % 50u), 3, PAL_UI_PINK);
        if (t > 190) shout(hx + 30, hy - 20, "don't stop~", PAL_UI_PINK);
    } else {
        /* dawn: heaps of sleepers, bunting, shoes, a tally on the fence, a smug gazebo */
        plat_rect(4, gy - 46, 74, 5, PAL_RAMP(RAMP_WOOD, 3));
        plat_rect(4, gy - 46, 3, 46, PAL_RAMP(RAMP_WOOD, 2));
        plat_rect(75, gy - 46, 3, 46, PAL_RAMP(RAMP_WOOD, 2));
        for (k = 0; k < DOOMO_MIN(47, t / 4); k++)                       /* forty-seven */
            plat_rect(7 + (k % 16) * 4, gy - 42 + (k / 16) * 8, 1, 6, PAL_BLACK);
        for (i = 0; i < 6; i++) {                                        /* the sleepers */
            int px = sw - 20 - i * 34, py = gy - 22 + (i & 1) * 6;
            ui_fill_ellipse(px, py, 14, 6, PAL_PAINT((i * 7 + 3) % PAL_PAINT_COUNT, 1));
            ui_fill_circle(px - 12, py - 2, 4, PAL_RAMP(RAMP_PEACH, 5));
            plat_rect(px + 14, py - 6 - (i & 1) * 2, 5, 3, PAL_BLACK);   /* an abandoned shoe */
            if (((ct / 20u) + (uint32_t)i) % 4u == 0) shout(px - 8, py - 20, "zzz", PAL_WHITE);
        }
        plat_rect(sw - 60, gy - 36, 44, 4, PAL_RAMP(RAMP_WOOD, 4));      /* the smug gazebo */
        plat_rect(sw - 54, gy - 42, 32, 6, PAL_RAMP(RAMP_WOOD, 5));
        plat_rect(sw - 46, gy - 48, 16, 6, PAL_RAMP(RAMP_WOOD, 5));
        plat_rect(sw - 56, gy - 32, 3, 32, PAL_RAMP(RAMP_WOOD, 2));
        plat_rect(sw - 23, gy - 32, 3, 32, PAL_RAMP(RAMP_WOOD, 2));
        ui_heart(sw - 38, gy - 60 - ui_wave((int)ct * 2, 16, 3), 3, PAL_UI_PINK);
        if (t > 60) shout(hx - 60, hy, "mmm.", PAL_UI_PINK);
        if (t > 160 && (t % 40) < 20) shout(hx - 90, hy - 16, "...brunch?", PAL_UI_GOLD);
    }
}

void feast_draw(int panel, uint32_t pt, uint32_t ct)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), gy = sh * 3 / 4, i;
    int cx = sw / 2 - 20, cy = gy - 56, extra = inflate(panel, pt), shake = 0;
    int cute = F_CUTE;

    if (panel == 10 || panel == 11) { epilogue(panel, pt, ct); return; }
    if (panel == 12 || panel == 13) { pump_scene(panel, pt, ct); return; }
    if (panel >= 14 && panel <= 19) { night_scene(panel, pt, ct); return; }

    if (panel == 20) {                                   /* title card, then the memorial */
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
        int coil = 18 - (int)DOOMO_MIN(pt / 14, 15u);
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
    if (panel == 3 && pt > CU_REAR_B - 6u && pt < CU_REAR_B + 44u) {     /* SLAP! pop-in */
        int s = ui_pop_scale((int)pt - (int)CU_REAR_B + 6);
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
