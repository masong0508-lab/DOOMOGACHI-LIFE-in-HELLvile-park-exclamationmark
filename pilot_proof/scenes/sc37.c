/* sc37.c - SC37 (2-08) WHACK-AN-IMP BOOTH - NIGHT. Act Two out. Sunny looks
 * at the tent, at the eyes, at the tent; steps down into the dark. The
 * plastic imps pull their lids shut. Rivet's prize case under its caged lamp:
 * BUBBLE WAND, PARTY PUMP (over SLAUGHTER), PURRING SAW (over MEATCLEAVER).
 * TOP PRIZE. Please don't. -R. A paw on the glass. beep. */
#include "act2_common.h"
#define SHOTS(X) X(main, 27.0)
#include "scene_tl.h"
static const float T_STEP = 3.6f, T_IMPS = 7.0f, T_TILT = 11.0f, T_SIGN = 18.0f, T_PAW = 22.4f, T_BEEP = 25.0f;
static const Cut C[] = { { 0, 6, -20, 3.2f, 3.6f, 0, 0 }, { T_STEP, -10, -18, 1.6f, 1.8f, -30, 0 }, { T_IMPS, 0, -16, 3.0f, 3.4f, 6, 0 },
    { T_TILT, 0, 0, 1, 1, 0, 1 }, { T_SIGN, 0, -30, 4.0f, 4.4f, 0, 0 }, { 20.4f, -10, -22, 3.0f, 3.2f, 0, 0 }, { T_PAW, 0, 0, 1, 1, 0, 2 } };
static void night(float ta) { fair_backdrop(ta * 30, -200, 200); tk_post_darken(4); }
static void booth(float ta) {
    wd_rect(-30, -24, 60, 24, PAL_PAINT(PAINT_GRAPE, 1), INK);
    wd_rect(-30, -26, 60, 3, PAL_PAINT(PAINT_LEMON, 1), INK);
    for (int k = 0; k < 5; k++) {   /* imps pop up, see the fog, pull their lids shut, 2 f apart */
        float x = -24 + k * 12, t0 = T_IMPS + 0.4f + k * 0.067f;
        float up = seg(ta, T_IMPS + k * 0.067f, t0) * (1 - seg(ta, t0 + 0.8f, t0 + 0.9f));
        wd_blob(x, -26, 4, 1.4f, 0, PAL_PAINT(PAINT_JET, 2), 0);
        if (up > 0) { wd_blob_ol(x, -27 - up * 5, 3, 3, 0, PAL_PAINT(PAINT_CHERRY, 2), INK, 0); wd_tri(x - 2, -29 - up * 5, x - 1, -30 - up * 5, x - 2.5f, -32 - up * 5, PAL_WHITE); wd_tri(x + 2, -29 - up * 5, x + 1, -30 - up * 5, x + 2.5f, -32 - up * 5, PAL_WHITE);
            wd_blob(x - 1, -27.5f - up * 5, 0.5f, 0.6f, 0, INK, 0); wd_blob(x + 1, -27.5f - up * 5, 0.5f, 0.6f, 0, INK, 0); }
        if (ta >= t0 + 0.9f) wd_blob_ol(x, -26.5f, 4, 1.2f, 0, PAL_PAINT(PAINT_CHERRY, 1), INK, 0);
    }
    /* the prize case, under its caged work lamp */
    wd_rect(-14, -40, 28, 14, PAL_RAMP(RAMP_STEEL, 3), INK);
    for (int k = 0; k < 6; k++) wd_blob(-12 + k * 4.8f, -39, 0.7f, 0.7f, 0, PAL_RAMP(RAMP_STEEL, 6), WB_OUT);
    wd_rect(-12, -37, 24, 9, PAL_PAINT(PAINT_GRAPE, 0), PAL_GRAY_L);
    wd_line(0, -60, 0, -50, 0.4f, INK); wd_blob(0, -48, 3, 2, 0, PAL_PAINT(PAINT_LEMON, 2), WB_OUT);
    for (int k = -1; k <= 1; k++) wd_line(k * 2.4f, -50, k * 3, -46, 0.3f, INK);
    tk_dither_rect((int)wd_sx(-18), (int)wd_sy(-48), (int)(36 * wd_z), (int)(24 * wd_z), PAL_PAINT(PAINT_LEMON, 2), 2);
}
static void case_inside(float ta) {   /* tilt down into the case: three prizes on purple velvet, each tag held to read */
    float u = seg(ta, T_TILT, T_TILT + 13 / 30.0f);
    tk_clear(PAL_PAINT(PAINT_GRAPE, 0));
    int oy = (int)lerp(-60, 0, u * u * (3 - 2 * u));
    for (int y = 0; y < TH; y += 4) tk_rect(0, y + oy % 4, TW, 1, PAL_PAINT(PAINT_GRAPE, 1));
    wd_cam(TW / 2, TH / 2 - oy, 1);
    /* bubble wand */
    wd_tube(30, 90, 60, 60, 2, 1.6f, INK, PAL_PAINT(PAINT_LEMON, 0), PAL_PAINT(PAINT_LEMON, 1), PAL_WHITE); wd_blob_ol(66, 54, 7, 7, 0, 0, PAL_PAINT(PAINT_SKY, 2), 0);
    /* pump shotgun: bellows, drainpipe, bike bell */
    wd_rect(96, 70, 26, 14, PAL_PAINT(PAINT_CARAMEL, 1), INK); for (int k = 0; k < 4; k++) wd_line(98 + k * 6, 70, 98 + k * 6, 84, 0.6f, PAL_PAINT(PAINT_CARAMEL, 0));
    wd_tube(122, 76, 160, 62, 4, 4, INK, PAL_RAMP(RAMP_STEEL, 1), PAL_RAMP(RAMP_STEEL, 3), PAL_RAMP(RAMP_STEEL, 6)); wd_blob_ol(140, 64, 4, 3, 0, PAL_UI_GOLD, INK, WB_SHADE);
    /* welded saw */
    wd_blob_ol(196, 70, 16, 16, 0, PAL_RAMP(RAMP_STEEL, 5), INK, WB_SHADE); for (int k = 0; k < 12; k++) { float a = k * 0.5236f; wd_tri(196 + cosf(a) * 15, 70 + sinf(a) * 15, 196 + cosf(a + 0.2f) * 15, 70 + sinf(a + 0.2f) * 15, 196 + cosf(a + 0.1f) * 19, 70 + sinf(a + 0.1f) * 19, PAL_RAMP(RAMP_STEEL, 6)); }
    float r = ta - T_TILT;
    struct { int x; const char *t, *old; float at; } tags[3] = { { 6, "BUBBLE WAND", 0, 1.0f }, { 86, "PARTY PUMP", "SLAUGHTER", 2.6f }, { 164, "PURRING SAW", "MEATCLEAVER", 4.4f } };
    for (int i = 0; i < 3; i++) if (r >= tags[i].at) {
        int ty = 112 + oy;
        if (tags[i].old) { tk_rect(tags[i].x - 2, ty + 9, tk_text_w(tags[i].old, 1) + 4, 10, PAL_PAINT(PAINT_BONE, 1)); tk_text(tags[i].x, ty + 10, tags[i].old, PAL_PAINT(PAINT_CHERRY, 0), 1); }
        tk_rect(tags[i].x - 2, ty - 1, tk_text_w(tags[i].t, 1) + 4, 10, PAL_WHITE); tk_text(tags[i].x, ty, tags[i].t, PAL_PAINT(PAINT_NAVY, 1), 1);
    }
}
static void sunny_pet(float ta, float x, float y, float s, float look) {
    Sunny u; sunny_default(&u, x, y, s, ta * 30); u.look_x = look; u.suitcase = 0; u.ears = 1.1f; sunny_draw(&u);
    pet_draw(x, y - 23 * s - 4, s * 0.9f, ta * 30, ta >= T_BEEP - 0.6f ? 0 : look, ta >= T_BEEP && ta < T_BEEP + 0.3f);
}
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    int k = cut_at(C, 7, ta, 27);
    if (k == 1) { case_inside(ta); return; }
    if (k == 2) {   /* the paw on the glass: 6 f pull back, 3 f press, 8 f settle; pad squashes flat */
        float keep = T_TILT + 6; case_inside(keep);                          /* the prizes, through the glass */
        tk_dither_rect(0, 0, TW, TH, PAL_WHITE, 2);
        tk_line(0, 20, 60, 0, PAL_WHITE); tk_line(10, 40, 90, 0, PAL_WHITE);  /* glass glints */
        wd_cam(TW / 2, TH / 2, 1);
        float p = ta - T_PAW, back = seg(p, 0, 0.2f), press = seg(p, 0.2f, 0.3f), settle = seg(p, 0.3f, 0.57f);
        float py = 110 + back * 14 - press * 30 + settle * 3;
        float sq = press > 0.9f && p < 0.43f ? 0.75f : 1;
        wd_tube(170, 200, 140, py + 16, 18, 15, INK, PAL_PAINT(PAINT_SKY, 0), PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_SKY, 2));
        wd_blob_ol(130, py, 22 / sq, 20 * sq, 0, PAL_PAINT(PAINT_BONE, 2), INK, WB_SHADE);
        for (int j = 0; j < 4; j++) wd_blob(112 + j * 12, py - 12 * sq, 4, 3.4f, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
        wd_blob(130, py + 4, 9 / sq, 7 * sq, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
        wd_cam(0, -10, 3.4f);
        if (ta >= T_BEEP - 0.8f) { pet_draw(-18, -22, 2.4f, ta * 30, 1, ta >= T_BEEP && ta < T_BEEP + 0.3f); }
        if (ta >= T_BEEP) { bubble_side = 1; bubble("beep.", 5, (int)wd_sx(-14), (int)wd_sy(-26)); bubble_side = 0; tk_text(6, TH - 10, "[beep.]", PAL_WHITE, 1); }
        return;
    }
    night(ta);
    if (ta < T_STEP) {   /* the edge of the bandstand: tent, eyes, tent */
        wd_rect(-60, -2, 120, 6, PAL_PAINT(PAINT_SILVER, 1), INK);
        float look = ta < 1.2f ? 1 : ta < 2.2f ? -1 : 1;
        sunny_pet(ta, 6, -2, 1.0f, look);
        return;
    }
    booth(ta);
    float walk = seg(ta, T_STEP, T_IMPS + 2.5f);
    float sx = ta < T_SIGN ? lerp(-70, -22, walk) : -22;
    float sxl = ta >= T_SIGN + 2.4f ? ((int)((ta - T_SIGN - 2.4f) / 0.6f) % 2 ? 1 : -1) : 1;   /* the case, the tent, the case */
    sunny_pet(ta, sx, 4, 0.8f, sxl);
    if (ta >= T_IMPS + 0.4f && ta < T_IMPS + 1.4f) tk_text(150, 20, "clack clack clack", PAL_WHITE, 1);
    if (ta >= T_SIGN) { int x = (int)wd_sx(-14), y = (int)wd_sy(-37); if (x < 4) x = 4; tk_rect(x, y, 120, 22, PAL_WHITE); tk_frame(x, y, 120, 22, PAL_GRAY_D); tk_text(x + 4, y + 2, "TOP PRIZE.", PAL_PAINT(PAINT_NAVY, 1), 1); tk_text(x + 4, y + 12, "Please don't.  -R", PAL_PAINT(PAINT_NAVY, 1), 1); }
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    riff(0, T_TILT, 0.08f, 0, 0);
    for (int k = 0; k < 6; k++) tk_note(T_STEP + 0.4 + k * 0.45, 0.04, 45, TKV_NOISE, 0.1f);
    for (int k = 0; k < 5; k++) tk_note(T_IMPS + 0.9 + k * 0.067, 0.04, 70, TKV_NOISE, 0.2f);
    static const int RV[6] = { 67, 71, 74, 77, 74, 72 };
    for (int k = 0; k < 6; k++) tk_note(T_SIGN + k * 0.3, 0.28, RV[k], TKV_SQ25, 0.14f);
    tk_note(T_SIGN, 0.3, 84, TKV_SINE, 0.2f); tk_note(T_SIGN + 1.8, 0.2, 36, TKV_NOISE, 0.3f);   /* anvil */
    tk_note(T_PAW + 0.3, 0.05, 76, TKV_NOISE, 0.1f);
    line_at("a2_08_pet_01", T_BEEP);
}
