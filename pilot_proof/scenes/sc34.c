/* sc34.c - SC34 (2-05) BANDSTAND STEPS - NIGHT. Clergy coming through. Doctor.
 * No pulse... he's a skeleton. Three candles, three tries. A scared thing in
 * a loud coat. The ribs yawn. Fuzz/Bloom: bubbles + babble (no recordings). */
#include "act2_common.h"
#define SHOTS(X) X(main, 28.0)
#include "scene_tl.h"
static const float F3 = 0.8f, B3 = 3.6f, T_STOP = 8.2f, B1 = 11.4f, B2 = 12.8f, T_C = 15.4f, F1 = 17.4f, F2 = 22.0f, T_YAWN = 26.8f;
static const Cut C[] = { { 0, 40, -12, 1.5f, 1.7f, -30, 0 }, { 3.3f, 40, -10, 2.8f, 3.2f, -10, 0 }, { 6.6f, -14, -22, 2.2f, 2.5f, 0, 0 },
    { 8.6f, 0, 0, 1, 1, 0, K_WATCH }, { 11.2f, -26, -26, 3.8f, 4.3f, 0, 0 }, { 14.8f, -6, -16, 5.0f, 5.4f, 0, 0 },
    { 17.2f, 4, -26, 3.4f, 3.9f, 0, 0 }, { 21.8f, -4, -22, 2.5f, 3.1f, 0, 0 }, { 26.6f, -40, -30, 3.0f, 3.5f, 0, 0 } };
static void group(float ta) {
    float in = seg(ta, 0, 6.6f), bin = seg(ta, 1.5f, 6.8f);
    float fx = lerp(90, 4, in), fy = lerp(20, -10, seg(ta, 5.4f, 6.6f)), bx = lerp(110, -26, bin), by = lerp(22, -10, seg(ta, 5.8f, 6.8f));
    Critter f = fuzz_at(ta, fx, fy);
    if (ta < 6.6f) { f.y -= fabsf(sinf(ta * 9)) * 1.5f; f.armL = 1.6f + 0.5f * sinf(ta * 3); f.armR = 1.6f - 0.5f * sinf(ta * 3); }   /* hugging people on the way past */
    if (ta >= T_C - 0.3f && ta < F1) { f.kneel = 1; f.armR = 1.2f; }
    if (ta >= F1 + 2.6f && ta < F1 + 4.0f) { int k = (int)((ta - F1 - 2.6f) / 0.45f); f.look_x = -1 + k * 0.6f; f.look_y = 0.8f; }   /* nods at each candle */
    if (ta >= F2 - 0.4f && ta < F2 + 1.4f) { f.armL = 1.3f + 0.3f * sinf(ta * 14); f.armR = 1.3f - 0.3f * sinf(ta * 14); }       /* rolls up his sleeves */
    if (ta >= F2 + 1.4f) { f.armL = f.armR = 0.9f; f.squash = 1.03f; }
    f.mouth = lip("a", (int)((ta - F3) * 30), 50) + lip("b", (int)((ta - F1) * 30), 120) + lip("c", (int)((ta - F2) * 30), 130);
    critter_draw(&f); float fmx = critter_mouth_x, fmy = critter_mouth_y;
    Critter b = bloom_at(ta, bx, by);
    if (ta >= B3 && ta < B3 + 0.8f) b.armR = 2.0f;                                       /* a flat wave: move */
    if (ta >= T_STOP - 0.6f && ta < B2 + 1) { b.armR = 1.5f; b.armL = 2.8f; b.look_x = -1; }   /* wrist bone in two fingers, one finger up */
    if (ta >= B2) b.mouth = 0;
    b.mouth = lip("d", (int)((ta - B3) * 30), 25) + lip("e", (int)((ta - B1) * 30), 25) + lip("f", (int)((ta - B2) * 30), 60);
    if (ta >= B2 + 1.2f && ta < B2 + 2.0f) b.look_x = 0.6f;
    critter_draw(&b); float bmx = critter_mouth_x, bmy = critter_mouth_y;
    say_at_world("Pardon me! Clergy! Coming through!", ta, F3, 1.8f, fmx, fmy, 0);
    say_at_world("Doctor. Move.", ta, B3, 0.9f, bmx, bmy, 0);
    say_at_world("No pulse.", ta, B1, 0.8f, bmx, bmy, 1);
    say_at_world("Relax. He's a skeleton. He never has one.", ta, B2, 2.0f, bmx, bmy, 1);
    say_at_world("Three steps: a beat, holy water, and its real name. Three candles, three tries.", ta, F1, 4.0f, fmx, fmy, 1);
    say_at_world("A demon's just a scared thing in a loud coat. You help it take the coat off.", ta, F2, 4.4f, fmx, fmy, 1);
}
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    if (cut_at(C, 9, ta, 28) == K_WATCH) { fob_watch(ta, 9.0f); return; }
    dark_stage(ta, 3, T_C, 0); possessed_bj(ta, ta < T_STOP || ta >= B2 + 2.2f, ta >= T_STOP && ta < B2 ? -0.6f : 0);
    if (ta < 6.6f) { CrowdOpts o = { 0 }; o.n = 10; o.x0 = -100; o.x1 = 110; o.y0 = 26; o.y1 = 40; o.t = ta * 30; o.seed = 93; o.freeze_at = -1; o.avoid_x = lerp(80, 0, seg(ta, 0, 6)); o.avoid_r = 14; crowd_draw(&o); }
    group(ta);
    for (int i = 0; i < 3; i++) if (ta >= T_C + i * 0.5f && ta < T_C + i * 0.5f + 0.3f) tk_text(30 + i * 60, 20, "TINK", PAL_WHITE, 1);
    if (ta >= T_YAWN && ta < T_YAWN + 1.2f) tk_text(140, 20, "(a yawn)", PAL_WHITE, 1);
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    riff(0, 28, 0.12f, T_STOP, B2 + 2.2f);
    babble_line(F3, 1.8f, 52, 0.2f); babble_line(B3, 0.9f, 62, 0.18f);
    for (int i = 0; i < 4; i++) tk_note(9.0 + i * 0.5, 0.03, 92, TKV_SINE, 0.15f);
    babble_line(B1, 0.8f, 62, 0.18f); babble_line(B2, 2.0f, 62, 0.18f);
    for (int i = 0; i < 3; i++) { tk_note(T_C + i * 0.5, 0.05, 98, TKV_SINE, 0.2f); tk_note(T_C + i * 0.5, 0.04, 60, TKV_NOISE, 0.1f); }
    static const int H[8] = { 64, 64, 67, 0, 64, 64, 67, 0 }; for (int i = 0; i < 24; i++) if (H[i % 8]) tk_note(16.5 + i * 0.43, 0.4, H[i % 8], TKV_TRI, 0.06f);   /* the FUZZ hymn, small */
    babble_line(F1, 4.0f, 52, 0.2f); babble_line(F2, 4.4f, 50, 0.2f);
    tk_note(T_YAWN, 0.3, 67, TKV_SINE, 0.15f); tk_note(T_YAWN + 0.4, 0.3, 63, TKV_SINE, 0.15f); tk_note(T_YAWN + 0.8, 0.5, 60, TKV_SINE, 0.15f);
}
