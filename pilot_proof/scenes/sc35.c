/* sc35.c - SC35 (2-06) BANDSTAND - NIGHT. TRY 1. THE POWER OF CUDDLES COMPELS
 * YOU! ...One. One's a start! The hymn in major vs the ribs in minor; kazoo;
 * the first candle dies; FAIL x1. A hand pulls the tablecloth down an inch. */
#include "act2_common.h"
#define SHOTS(X) X(a_compel, 8.0) X(b_hymn, 18.0) X(c_fail, 8.0) X(d_after, 6.0)
#include "scene_tl.h"
static const float F1 = 1.0f, B1 = 4.2f, F3 = 5.6f, T_HYMN = 8.4f, T_KAZ = 26.3f, T_OUT = 28.0f, T_FAIL = 28.3f, T_CLOTH = 31.0f;
#define BEAT (60.0f / 140.0f)
static void scene(float ta) {
    dark_stage(ta, 3, -10, T_OUT); possessed_bj(ta, ta >= T_HYMN, 0);
    Critter f = fuzz_at(ta, 8, -10);
    f.armL = f.armR = ta < T_HYMN ? 2.9f : (((int)((ta - T_HYMN) / BEAT) % 2) ? 1.5f : 1.2f);
    if (ta >= T_KAZ) f.armL = f.armR = 1.4f;
    f.mouth = lip("a2_06_fuzz_01", (int)((ta - F1) * 30), 80) + lip("a2_06_fuzz_03", (int)((ta - F3) * 30), 30) + (ta >= T_HYMN && ta < T_HYMN + 14 ? 0.5f * fabsf(sinf(ta * 6)) : 0);
    critter_draw(&f); float mx = critter_mouth_x, my = critter_mouth_y;
    Critter b = bloom_at(ta, 22, -6.6f); critter_draw(&b);
    if (ta >= F1 && ta < F1 + 3) { bubble_side = 1; bubble("THE POWER OF CUDDLES COMPELS YOU!", (int)((ta - F1) * 24) + 1, (int)wd_sx(mx), (int)wd_sy(my)); bubble_side = 0; }
    if (ta >= B1 && ta < B1 + 1.0f) tk_text(96, 60, "...One.", PAL_PAINT(PAINT_GRAPE, 2), 1);   /* the size of a crumb */
    say_at_world("One's a start!", ta, F3, 0.8f, mx, my, 1);
    if (ta >= T_HYMN && ta < T_HYMN + 14) tk_text(6, TH - 10, "Come in close, come in close, hold the one...", PAL_WHITE, 1);
    if (ta >= T_CLOTH && ta < T_CLOTH + 1.5f) tk_text(6, 6, "(under the cake table, the cloth goes down an inch)", PAL_WHITE, 1);
}
SHOT(a_compel, { wd_cam(-10, -26, lerp(2.2f, 2.6f, seg(ta, 0, 8))); scene(ta); })
SHOT(b_hymn, { wd_cam(lerp(-20, -10, seg(ta, 8, 26)), -26, 2.0f); scene(ta); })
SHOT(c_fail, { wd_cam(-12, -18, 3.2f); scene(ta);
    if (ta >= T_OUT && ta < T_OUT + 0.1f) tk_dither_rect(0, 0, TW, TH, PAL_PAINT(PAINT_CHERRY, 1), 3);
    if (ta >= T_FAIL) { float s = 1 + (1 - seg(ta, T_FAIL, T_FAIL + 0.2f)); tk_text_outline(60, 40, "FAIL x1", PAL_UI_RED, PAL_WHITE, s > 1.5f ? 4 : 3); } })
SHOT(d_after, { wd_cam(-30, -14, lerp(2.6f, 3.2f, seg(ta, 30, 36))); scene(ta); })
void tk_music(void) {
    babble_line(F1, 2.6f, 50, 0.26f);
    tk_note(B1, 0.12, 41, TKV_TRI, 0.1f);
    babble_line(F3, 0.8f, 52, 0.2f);
    static const int H[16] = { 64, 64, 67, 0, 64, 64, 67, 0, 69, 67, 65, 64, 62, 64, 60, 0 };
    static const int R[16] = { 63, 63, 67, 0, 63, 63, 67, 0, 68, 67, 65, 63, 62, 63, 60, 0 };
    for (int i = 0; i < 32; i++) { double t = T_HYMN + i * BEAT; int k = i % 16; if (i < 16 && H[k]) tk_note(t, BEAT * 0.9, H[k], TKV_TRI, 0.26f); if (R[k]) tk_note(t + 0.05 + i * 0.012, 0.25, R[k] + 12, TKV_SINE, 0.16f); if (i % 2 == 1 && i < 16) tk_note(t, 0.03, 70, TKV_NOISE, 0.15f); }
    riff(T_HYMN + 32 * BEAT, 36, 0.12f, T_KAZ, T_FAIL + 1.5);
    for (int k = 0; k < 3; k++) tk_note(T_KAZ + k * 0.5, 0.4, 62 - k * 3, TKV_SAW, 0.2f);   /* kazoo */
    tk_note(T_OUT, 0.1, 50, TKV_NOISE, 0.15f);
    tk_sfx(T_FAIL, SFX_THUD, 0.5f); tk_note(T_FAIL + 0.1, 0.4, 47, TKV_SQ25, 0.2f);
}
