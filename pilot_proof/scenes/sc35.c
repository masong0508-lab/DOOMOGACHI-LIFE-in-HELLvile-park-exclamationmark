/* sc35.c - SC35 (2-06) BANDSTAND - NIGHT. TRY 1. THE POWER OF CUDDLES COMPELS
 * YOU! ...One. One's a start! The hymn in major vs the ribs in minor; kazoo;
 * the first candle dies; FAIL x1. A hand pulls the tablecloth down an inch. */
#include "act2_common.h"
#define SHOTS(X) X(main, 40.0)
#include "scene_tl.h"
static const float F1 = 1.0f, B1 = 4.2f, F3 = 5.6f, T_HYMN = 8.4f, T_KAZ = 26.3f, T_OUT = 28.0f, T_FAIL = 28.3f, T_CLOTH = 31.0f;
#define BEAT (60.0f / 140.0f)
static const Cut C[] = { { 0, 8, -22, 2.4f, 3.0f, 0, 0 }, { 3.6f, -14, -26, 1.6f, 1.75f, 0, 0 }, { 5.2f, 10, -30, 3.6f, 4.0f, 0, 0 },
    { 7.0f, -12, -24, 1.8f, 2.0f, 10, 0 }, { 12.0f, 10, -28, 3.2f, 3.6f, 0, 0 }, { 16.0f, -40, -30, 3.2f, 3.6f, 0, 0 },
    { 20.0f, -14, -26, 2.4f, 2.8f, 0, 0 }, { 24.0f, -6, -16, 5.0f, 5.5f, 0, 0 }, { 28.3f, -14, -10, 1.6f, 1.6f, 0, 0 },
    { T_CLOTH, 0, 0, 1, 1, 0, K_TABLE }, { 34.5f, 10, -30, 3.6f, 4.0f, 0, 0 }, { 37.5f, -36, -18, 2.4f, 3.0f, 0, 0 } };
static const char *BAR[4] = { "~ Come in close,", "~ come in close,", "~ hold the one who's", "~ frightened most. ~" };
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    if (cut_at(C, 12, ta, 40) == K_TABLE) { table_close(ta, T_CLOTH); return; }
    dark_stage(ta, 3, -10, T_OUT); possessed_bj(ta, ta >= T_HYMN, 0);
    Critter fz = fuzz_at(ta, 8, -10);
    int sing = ta >= T_HYMN && ta < T_HYMN + 16 * BEAT;
    float beat = sing ? (ta - T_HYMN) / BEAT : 0, ph = beat - floorf(beat);
    if (ta < T_HYMN) { fz.armL = fz.armR = 2.9f; fz.squash = 1.05f; }                  /* a bear about to hug a building */
    else if (sing || ta < T_KAZ) { int on = ((int)beat % 2) == 1 && ph < 0.3f; fz.armL = fz.armR = on ? 1.6f : 1.15f; fz.squash = on ? 0.95f : 1.0f; }
    if (ta >= T_FAIL) { fz.armL = 1.4f + 0.06f * sinf(ta * 70); fz.armR = 1.4f - 0.06f * cosf(ta * 63); fz.look_y = 0.7f; }   /* shaking paws */
    if (ta >= 36.0f) { fz.armL = fz.armR = 1.5f; }                                     /* pressed together, flat, until they stop */
    fz.mouth = lip("a", (int)((ta - F1) * 30), 80) + lip("b", (int)((ta - F3) * 30), 30) + (sing ? 0.6f * (1 - ph) : 0);
    critter_draw(&fz); float mx = critter_mouth_x, my = critter_mouth_y;
    Critter b = bloom_at(ta, 22, -6.6f); if (ta >= 36.5f) { b.armL = 1.4f; b.look_x = -1; } critter_draw(&b);
    if (ta >= F1 && ta < F1 + 2.6f) { bubble_side = 1; bubble("THE POWER OF CUDDLES COMPELS YOU!", (int)((ta - F1) * 24) + 1, (int)wd_sx(mx), (int)wd_sy(my)); bubble_side = 0; }
    if (ta >= B1 && ta < B1 + 1.0f) { int sx = (int)wd_sx(-40), sy = (int)wd_sy(-62); tk_text(sx - 14, sy, "...One.", PAL_PAINT(PAINT_GRAPE, 2), 1); }
    say_at_world("One's a start!", ta, F3, 0.8f, mx, my, 1);
    if (sing) { int bar = (int)(beat / 4); if (bar > 3) bar = 3; bubble_side = 1; bubble(BAR[bar], 30, (int)wd_sx(mx), (int)wd_sy(my)); bubble_side = 0; }
    if (ta >= T_KAZ && ta < T_KAZ + 1.5f) tk_text(150, 20, "(kazoo)", PAL_WHITE, 1);
    if (ta >= T_OUT && ta < T_OUT + 0.1f) tk_dither_rect(0, 0, TW, TH, PAL_PAINT(PAINT_CHERRY, 1), 3);
    if (ta >= T_FAIL && ta < T_CLOTH) {   /* FAIL x1 slams in from the top, clear of faces */
        int y = (int)lerp(-36, 6, seg(ta, T_FAIL, T_FAIL + 0.15f)) + (ta < T_FAIL + 0.25f && ta >= T_FAIL + 0.15f ? 2 : 0);
        tk_rect(50, y, 140, 30, PAL_UI_RED_D); tk_frame(50, y, 140, 30, PAL_WHITE);
        tk_text_outline(64, y + 4, "FAIL x1", PAL_WHITE, PAL_UI_RED_D, 3);
        if (ta < T_FAIL + 0.2f) tk_post_shake(2, 1);
    }
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
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
    tk_note(T_CLOTH + 1.5, 0.15, 60, TKV_NOISE, 0.06f);
    tk_sfx(T_FAIL, SFX_THUD, 0.5f); tk_note(T_FAIL + 0.1, 0.4, 47, TKV_SQ25, 0.2f);
}
