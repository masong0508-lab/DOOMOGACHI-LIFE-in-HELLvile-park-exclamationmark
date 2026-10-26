/* sc30.c - SC30 (2-01) ACT CARD / BANDSTAND - NIGHT. The clipboard, 21:00
 * underlined twice in a shaking hand. Every lantern out; only the ribs glow.
 * Eyes go red, tik tik. The ribs play themselves and speak: the couplet. */
#include "act2_common.h"
#define SHOTS(X) X(main, 22.0)
#include "scene_tl.h"
static const float T_EYE = 6.4f, V1 = 10.2f, V2 = 15.2f, T_TILT = 20.0f;
static const Cut C[] = { { 0, 0, 0, 1, 1, 0, K_CARD }, { 3.0f, -16, -24, 1.3f, 1.55f, 8, 0 }, { 6.0f, -40, -50, 5.0f, 5.6f, 0, 0 },
    { 8.4f, 10, 16, 3.0f, 3.5f, 0, 0 }, { 10.0f, -40, -30, 3.2f, 3.7f, 0, 0 }, { 13.6f, -64, 14, 2.8f, 3.1f, 10, 0 },
    { 15.0f, -40, -34, 3.8f, 4.3f, 0, 0 }, { 19.2f, -40, -48, 4.6f, 5.4f, 0, 0 } };
static void card(float ta) { tk_clear(PAL_PAINT(PAINT_CARAMEL, 1)); tk_rect(40, 10, 160, 150, PAL_UI_CREAM); tk_frame(40, 10, 160, 150, PAL_PAINT(PAINT_CHOCOLATE, 1));
    static const char *L[5] = { "18:00 Welcome!", "19:00 Bake-Off!", "20:00 Bandstand!", "21:00 (we don't do that)", "22:00 Bedtime!" };
    for (int i = 0; i < 5; i++) tk_text(56, 32 + i * 22, L[i], PAL_PAINT(PAINT_NAVY, 1), 1);
    for (int u = 0; u < 2; u++) { float w = seg(ta, 0.3f + u * 0.6f, 0.8f + u * 0.6f) * (u ? 150 : 140); for (int x = 0; x < (int)w; x++) tk_px(56 + x, 108 + u * 3 + (int)(sinf(x * 0.7f + u) * 1.2f), PAL_PAINT(PAINT_CHERRY, 1)); } }
static float syll(float ta, float t0, float len, int n) { if (ta < t0 || ta >= t0 + len) return 0; float u = (ta - t0) / len * n; return 1 - (u - floorf(u)); }
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    if (cut_at(C, 8, ta, 22) == K_CARD) { card(ta); return; }
    glow_pulse = syll(ta, V1, 3.2f, 11) + syll(ta, V2, 3.6f, 12);
    dark_stage(ta, 0, 0, 0); possessed_bj(ta, 1, ta >= T_TILT ? 0.15f : 0);
    for (int k = 0; k < 2; k++) if (ta >= T_EYE + k * 0.07f) wd_blob(-42.4f + k * 4.8f, -50.5f, 1, 1, 0, PAL_UI_RED, 0);
    CrowdOpts o = { 0 }; o.n = 10; o.x0 = -100; o.x1 = 100; o.y0 = 18; o.y1 = 32; o.t = 3 * 30; o.freeze_at = 0; o.ripple = 0; o.seed = 91; o.avoid_x = 10; o.avoid_r = 14; crowd_draw(&o);
    /* frozen mid-party: a bubble wand halfway to a mouth, a balloon on a very still paw */
    wd_line(-80, 14, -76, 8, 0.4f, PAL_PAINT(PAINT_LEMON, 1)); wd_blob_ol(-75, 7, 1.4f, 1.4f, 0, 0, PAL_PAINT(PAINT_SKY, 2), 0); wd_blob(-73, 3, 1, 1, 0, PAL_PAINT(PAINT_SKY, 2), WB_OUT);
    wd_line(-54, 16, -52, -4, 0.3f, PAL_GRAY_D); wd_blob_ol(-52, -8, 3, 3.6f, 0, PAL_PAINT(PAINT_MINT, 2), INK, WB_SHADE);
    Sunny u; sunny_default(&u, 10, 30, 0.8f, ta * 30); u.no_page = 1; u.page = 0.3f; u.ears = ta >= T_EYE ? 1.2f : 0.9f; u.look_x = -0.8f; sunny_draw(&u);
    wd_blob_ol(sunny_hand_x - 2, sunny_hand_y - 3, 4.5f, 3.6f, 0, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0), WB_SHADE);
    if (ta >= T_EYE && ta < T_EYE + 0.5f) tk_text(150, 20, "tik. tik.", PAL_WHITE, 1);
    if (ta >= V1 && ta < V2 - 0.4f) { verdict("We came on the fog,", ta - V1, 106); if (ta >= V1 + 1.6f) verdict("and we came for the cake.", ta - V1 - 1.6f, 128); }
    if (ta >= V2 && ta < T_TILT - 0.6f) { verdict("You left us no slice,", ta - V2, 106); if (ta >= V2 + 1.8f) verdict("so the whole thing we'll take.", ta - V2 - 1.8f, 128); }
    if (ta >= T_TILT && ta < T_TILT + 0.6f) tk_text(150, 20, "(creak)", PAL_WHITE, 1);
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    tk_note(0, 0.3, 84, TKV_SINE, 0.3f); tk_note(0.12, 0.3, 79, TKV_SINE, 0.25f);   /* HEE_HEE bell */
    riff(3.0, 22, 0.14f, 0, 0);
    tk_slide(3.0, 19, 33, 35, TKV_SAW, 0.05f);
    tk_note(T_EYE, 0.04, 96, TKV_SINE, 0.2f); tk_note(T_EYE + 0.07, 0.04, 96, TKV_SINE, 0.2f);
    flat2(V1, 3.2f); flat2(V2, 3.6f);
    tk_slide(T_TILT, 0.3, 50, 47, TKV_NOISE, 0.1f);
}
