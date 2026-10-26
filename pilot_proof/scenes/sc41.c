/* sc41.c - SC41 (3-02) BAKE TENT - NIGHT. Squelch: the demons pile up into
 * ONE BIG PUFF on the tent roof. Bwip. Bwoop. BWOMP. It doesn't wobble. The
 * look chain; beep. The wand drops; the shotgun swings in; SQUEAK; SCRATCH;
 * 14 f of silence; CHK- */
#include "act2_common.h"
#define SHOTS(X) X(main, 15.0)
#include "scene_tl.h"
static const float T_PILE = 0.0f, B1 = 3.6f, L1 = 6.2f, L2 = 6.47f, L3 = 6.74f, BEEP = 7.3f, T_SWAP = 8.4f, T_SQ = 9.4f, T_SCR = 10.6f, T_CHK = 14.6f;
static const Cut C[] = { { 0, 0, -44, 1.6f, 1.8f, 0, 0 }, { B1, -4, -40, 1.25f, 1.35f, 0, 0 }, { L1, -20, -14, 3.6f, 3.8f, 0, 0 }, { L3, -20, -24, 4.4f, 4.6f, 0, 1 }, { T_SWAP, -20, -16, 2.8f, 3.2f, 0, 0 } };
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    int k = cut_at(C, 5, ta, 15);
    if (ta >= T_SCR && ta < T_SCR + 0.2f) { float cx = wd_cx, cy = wd_cy; wd_cam(cx + (ta - T_SCR) * 40, cy, wd_z); }
    fair_backdrop(ta * 30, -100, 100); tk_post_darken(10); tk_dither_rect(0, 0, TW, TH, PAL_PAINT(PAINT_LAVENDER, 2), 4);
    fair_tent(10, 70, 56, PAL_PAINT(PAINT_MINT, 1));
    tk_text((int)wd_sx(-6), (int)wd_sy(-30), "BAKE TENT", PAL_PAINT(PAINT_MAGENTA, 0), 1);
    float merge = seg(ta, 0.4f, 2.6f);
    for (int i = 0; i < 6; i++) if (merge < 1) puff_demon(10 + (i - 2.5f) * 6 * (1 - merge), -60 - i * 6 * (1 - merge), 1.2f, ta * 30, -1);
    if (merge > 0.5f) { float s = 1 + 2.4f * seg(ta, 1.6f, 2.6f); puff_demon(10, -62, 1.6f * s, 0, -1); }
    Sunny u; sunny_default(&u, -20, 6, 0.9f, ta * 30); u.suitcase = 0; u.no_page = 1;
    u.look_y = ta < L1 ? -0.8f : 0; u.look_x = ta >= L1 && ta < L3 ? -1 : 0;
    u.page = ta < T_SWAP ? (ta >= B1 && ta < 6 ? 0.8f : 0.4f) : 0.6f;
    sunny_draw(&u);
    if (ta < T_SWAP) { wd_tube(sunny_hand_x, sunny_hand_y, sunny_hand_x + 5, sunny_hand_y - 6, 0.5f, 0.4f, INK, PAL_PAINT(PAINT_LEMON, 0), PAL_PAINT(PAINT_LEMON, 1), PAL_WHITE); }
    else { float sw = seg(ta, T_SWAP, T_SWAP + 0.1f); wd_tube(sunny_hand_x - 6, sunny_hand_y + 2 - (1 - sw) * 8, sunny_hand_x + 12, sunny_hand_y - 6, 1.6f, 1.2f, INK, PAL_RAMP(RAMP_STEEL, 1), PAL_RAMP(RAMP_STEEL, 3), PAL_RAMP(RAMP_STEEL, 6)); wd_rect(sunny_hand_x - 4, sunny_hand_y - 1, 8, 4, PAL_PAINT(PAINT_CARAMEL, 1), INK);
        wd_line(-34, 8, -30, 2, 0.5f, PAL_PAINT(PAINT_LEMON, 1)); }   /* the dropped wand */
    float pl = ta >= L2 ? 1 : 0;
    pet_draw(-20 + pl * 3, -23 * 0.9f - 4 + pl * 2, 0.8f, ta * 30, pl ? -1 : 0, ta >= BEEP && ta < BEEP + 0.3f);
    static const char *BW[3] = { "bwip", "bwoop", "BWOMP" };
    for (int i = 0; i < 3; i++) { float t0 = B1 + i * 0.5f, a = ta - t0; if (a >= 0 && a < 2.0f) { float r = 3 + i * 2; soap(-8 + a * 30, -16 - a * 30 + (a > 0.5f ? (a - 0.5f) * 40 : 0), r, PAL_PAINT(PAINT_SKY, 2)); if (a > 0.45f && a < 0.8f) tk_text(130 + i * 20, 40, BW[i], PAL_WHITE, 1); } }
    if (k == 1) tk_text(4, 4, "(into the lens)", PAL_WHITE, 1);
    if (ta >= BEEP) { bubble_side = 1; if (ta < BEEP + 1) bubble("beep.", 5, (int)wd_sx(-16), (int)wd_sy(-28)); bubble_side = 0; if (ta < BEEP + 1) tk_text(6, TH - 10, "[beep.]", PAL_WHITE, 1); }
    if (ta >= T_SQ && ta < T_SQ + 0.8f) tk_text(6, TH - 10, "[squeak]", PAL_WHITE, 1);
    if (ta >= T_CHK) tk_text_outline(140, 30, "CHK-", PAL_WHITE, PAL_UI_RED_D, 2);
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    for (int i = 0; i < 6; i++) tk_slide(0.4 + i * 0.35, 0.15, 50, 44, TKV_NOISE, 0.15f);   /* squelch */
    static const int H[8] = { 84, 88, 91, 88, 89, 93, 91, 0 };
    for (int i = 0; ; i++) { double t = i * 0.214; if (t >= T_SCR) break; if (H[i % 8]) tk_note(t, 0.18, H[i % 8], TKV_SINE, 0.1f); }
    for (int i = 0; i < 3; i++) tk_slide(B1 + 0.45 + i * 0.5, 0.1, 70 - i * 6, 64 - i * 6, TKV_SINE, 0.2f);
    line_at("a3_02_pet_01", BEEP);
    tk_note(T_SWAP, 0.1, 60, TKV_NOISE, 0.2f);
    tk_slide(T_SQ, 0.15, 86, 92, TKV_SQ25, 0.25f);
    tk_sfx(T_SCR, SFX_SCRATCH, 0.4f); tk_slide(T_SCR, 0.17, 84, 60, TKV_SINE, 0.1f);
    tk_note(T_CHK, 0.06, 66, TKV_NOISE, 0.35f);
}
