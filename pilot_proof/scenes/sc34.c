/* sc34.c - SC34 (2-05) BANDSTAND STEPS - NIGHT. Clergy coming through. Doctor.
 * No pulse... he's a skeleton. Three candles, three tries. A scared thing in
 * a loud coat. The ribs yawn. Fuzz/Bloom: bubbles + babble (no recordings). */
#include "act2_common.h"
#define SHOTS(X) X(a_arrive, 7.0) X(b_pulse, 8.0) X(c_candles, 13.0)
#include "scene_tl.h"
static const float F3 = 0.8f, B3 = 3.6f, T_STOP = 8.2f, B1 = 11.4f, B2 = 12.8f, T_C = 15.4f, F1 = 17.4f, F2 = 22.0f, T_YAWN = 27.0f;
static void group(float ta, float fx, float bx) {
    Critter f = fuzz_at(ta, fx, ta < 7 ? 20 : -6); f.flip = ta < 7 ? -1 : -1;
    if (ta >= T_C && ta < F1) f.kneel = 1;
    f.mouth = lip("a2_05_fuzz_03", (int)((ta - F3) * 30), 50) + lip("a2_05_fuzz_01", (int)((ta - F1) * 30), 120) + lip("a2_05_fuzz_02", (int)((ta - F2) * 30), 130);
    critter_draw(&f); float fmx = critter_mouth_x, fmy = critter_mouth_y;
    Critter b = bloom_at(ta, bx, ta < 7 ? 22 : -10);
    if (ta >= T_STOP && ta < B2 + 1) { b.armR = 1.5f; b.armL = 2.8f; }
    b.mouth = lip("a2_05_bloom_03", (int)((ta - B3) * 30), 25) + lip("a2_05_bloom_01", (int)((ta - B1) * 30), 25) + lip("a2_05_bloom_02", (int)((ta - B2) * 30), 60);
    critter_draw(&b); float bmx = critter_mouth_x, bmy = critter_mouth_y;
    say_at_world("Pardon me! Clergy! Coming through!", ta, F3, 1.8f, fmx, fmy, 0);
    say_at_world("Doctor. Move.", ta, B3, 0.9f, bmx, bmy, 0);
    say_at_world("No pulse.", ta, B1, 0.8f, bmx, bmy, 2);
    say_at_world("Relax. He's a skeleton. He never has one.", ta, B2, 2.0f, bmx, bmy, 2);
    say_at_world("Three steps: a beat, holy water, and its real name. Three candles, three tries.", ta, F1, 4.0f, fmx, fmy, 1);
    say_at_world("A demon's just a scared thing in a loud coat. You help it take the coat off.", ta, F2, 4.4f, fmx, fmy, 1);
}
SHOT(a_arrive, { wd_cam(0, -16, 1.6f); dark_stage(ta, 0, 0, 0); possessed_bj(ta, 1, 0);
    CrowdOpts o = { 0 }; o.n = 10; o.x0 = -100; o.x1 = 100; o.y0 = 26; o.y1 = 40; o.t = ta * 30; o.seed = 93; o.freeze_at = -1; o.avoid_x = lerp(80, 0, seg(ta, 0, 6)); o.avoid_r = 14; crowd_draw(&o);
    group(ta, lerp(90, 10, seg(ta, 0, 6)), lerp(110, 24, seg(ta, 1.5f, 6.8f))); })
SHOT(b_pulse, { wd_cam(-24, -30, 2.6f); dark_stage(ta, 0, 0, 0); possessed_bj(ta, ta < T_STOP || ta >= B2 + 2.2f, ta >= 9 && ta < B2 ? 0.4f : 0);
    group(ta, 4, -26);
    if (ta >= 9.0f && ta < B1) { int k = (int)((ta - 9) * 2); for (int i = 0; i <= k && i < 4; i++) tk_text(10 + i * 26, 12, "tik", PAL_WHITE, 1); } })
SHOT(c_candles, { wd_cam(lerp(-10, -14, seg(ta, 15, 28)), -24, lerp(2.4f, 2.9f, seg(ta, 15, 28))); dark_stage(ta, 3, T_C, 0); possessed_bj(ta, 1, 0);
    group(ta, 4, -26);
    if (ta >= T_YAWN && ta < T_YAWN + 1.2f) tk_text(14, 14, "(a yawn, in minor)", PAL_WHITE, 1); })
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
