/* sc23.c - SC23 (1-10) FUN FAIR, LANTERN STRING - NIGHT (19:40). Pip, the
 * whole Junior Festival Patrol (and Mr. Flops), hangs Sunny's "1" on a
 * lantern: LOST PROPERTY. "When do the fireworks start?" The Mayor sweeps
 * past: "There's a lovely nine o'clock..." FREEZE 13 "...nothing!"
 * Mayor = recordings; Pip = bubbles + babble (no recording yet). */
#include "fair_common.h"
#include "mayor.h"
#define SHOTS(X) X(a_tie, 6.0) X(b_patrol, 10.4) X(c_mayor, 5.0) X(d_card, 1.0) X(e_baby, 5.6)
#include "scene_tl.h"
static const float P1 = 6.3f, P2 = 10.6f, SAL = 11.6f, P5 = 12.2f, P6 = 13.7f, P3 = 15.0f, M1 = 16.8f, FZ = 18.9f, M2 = 19.35f, P4 = 23.0f, FLOP = 26.4f;
static void night_fair(float ta) { fair_backdrop(ta * 30, -100, 100); tk_post_darken(11); }
static void lantern_1(float ta) {
    wd_line(-60, -64, 60, -60, 0.4f, PAL_PAINT(PAINT_CHOCOLATE, 1));
    for (int k = 0; k < 5; k++) wd_blob_ol(-48 + k * 24, -60 + k, 2.6f, 3.2f, 0, (k & 1) ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_CARAMEL, 0), WB_SHADE);
    float tied = seg(ta, 2.0f, 4.0f);
    float y = lerp(-46, -54, tied);
    wd_text(-3, y, "1", 1.2f, PAL_PAINT(PAINT_NAVY, 1), PAL_WHITE);
    if (tied > 0.5f) { wd_rect(4, y + 1, 12, 5, PAL_WHITE, PAL_GRAY_D); tk_text((int)wd_sx(4) + 1, (int)wd_sy(y + 1) + 1, "LOST PROPERTY", PAL_PAINT(PAINT_CHERRY, 1), 1); }
}
static void pip_at(float ta, float x, float y) {
    Critter p = cast_pip(x, y, ta * 30); p.s = 0.8f; p.flip = 1; p.look_x = 1;
    if (ta < 4.0f) p.armR = 2.8f;
    if (ta >= P2 && ta < P2 + 0.6f) p.armR = 1.4f;                       /* tapping the sash */
    if (ta >= SAL && ta < SAL + 1.0f) p.armR = 2.9f;                     /* salute */
    if (ta >= P3 && ta < P3 + 1.4f) { p.look_y = -0.6f; p.ears = CR_EARS_LONG; }
    p.mouth = lip("a1_10_pip_01", (int)((ta - P1) * 30), 120) + lip("a1_10_pip_02", (int)((ta - P2) * 30), 24) + lip("a1_10_pip_05", (int)((ta - P5) * 30), 36)
            + lip("a1_10_pip_06", (int)((ta - P6) * 30), 27) + lip("a1_10_pip_03", (int)((ta - P3) * 30), 39) + lip("a1_10_pip_04", (int)((ta - P4) * 30), 90);
    critter_draw(&p);
    if (ta >= SAL && ta < SAL + 1.0f) wd_line(x - 6, y - 15, x - 7, y - 21, 0.8f, PAL_PAINT(PAINT_ROSE, 1));   /* Mr. Flops salutes too */
    if (ta >= FLOP) wd_blob(x - 3.4f, y - 15.5f, 2.2f, 0.9f, 0.9f, PAL_PAINT(PAINT_ROSE, 2), WB_OUT);   /* his good ear flops down */
}
static void sunny_at(float ta, float x) { Sunny u; sunny_default(&u, x, 10, 0.85f, ta * 30); u.flip = -1; u.look_x = -1; u.look_y = 0.4f; sunny_draw(&u); }
SHOT(a_tie, { wd_cam(0, -34, 2.6f); night_fair(ta); lantern_1(ta);
    float hop = seg(ta, 4.4f, 4.8f);
    wd_rect(-12, -8, 14, 10, PAL_PAINT(PAINT_CARAMEL, 1), INK);
    pip_at(ta, -5, lerp(-8, 10, hop) - 4 * sinf(hop * 3.14f)); })
SHOT(b_patrol, { wd_cam(4, -14, 3.0f); night_fair(ta); lantern_1(ta); pip_at(ta, -10, 10); sunny_at(ta, 18);
    say_at_world("Found your number one! Lost stuff goes on a lantern, so the owner can find it.", ta, P1, 4.0f, critter_mouth_x, critter_mouth_y, 0);
    say_at_world("Patrol rules.", ta, P2, 0.8f, -10, -8, 0);
    say_at_world("I'm the whole patrol.", ta, P5, 1.2f, -10, -8, 0);
    say_at_world("And Mr. Flops!", ta, P6, 0.9f, -10, -8, 0);
    say_at_world("When do the fireworks start?", ta, P3, 1.3f, -10, -8, 0); })
SHOT(c_mayor, { wd_cam(4, -16, 2.2f); night_fair(ta); pip_at(ta, -10, 10); sunny_at(ta, 18);
    float run = ta < FZ ? (ta - 16.4f) * 50 : ta < M2 + 1.0f ? (FZ - 16.4f) * 50 : (FZ - 16.4f) * 50 + (ta - M2 - 1.0f) * 100;
    Mayor m; mayor_default(&m, -80 + run, 12, 0.85f, ta < FZ || ta >= FZ + 13 / 30.0f ? ta * 30 : FZ * 30);
    m.step = ta < FZ || ta >= M2 + 1.0f ? fabsf(sinf(ta * 12)) * 2 : 3; m.flutter = ta >= FZ && ta < M2 + 1.0f ? 0 : 0.6f;
    m.armR = 1.3f; m.expr = MX_HAPPY;
    m.mouth = lip("a1_10_mayor_01", (int)((ta - M1) * 30), 60) + lip("a1_10_mayor_02", (int)((ta - M2) * 30), 30);
    mayor_draw(&m);
    wd_rect(mayor_hand_x[1] - 1, mayor_hand_y[1] - 9, 8, 10, PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CHOCOLATE, 1));
    say_at_world("There's a lovely nine o'clock...", ta, M1, 2.0f, mayor_head_x + 6, mayor_head_y + 2, 0);
    say_at_world("...nothing!", ta, M2, 1.0f, mayor_head_x + 6, mayor_head_y + 2, 0); })
SHOT(d_card, { tk_clear(PAL_PAINT(PAINT_CARAMEL, 1)); tk_rect(40, 10, 160, 150, PAL_UI_CREAM); tk_frame(40, 10, 160, 150, PAL_PAINT(PAINT_CHOCOLATE, 1));
    static const char *L[5] = { "18:00 Welcome!", "19:00 Bake-Off!", "20:00 Bandstand!", "21:00 (we don't do that)", "22:00 Bedtime!" };
    for (int i = 0; i < 5; i++) tk_text(56, 32 + i * 22, L[i], i == 3 ? PAL_PAINT(PAINT_CHERRY, 0) : PAL_PAINT(PAINT_NAVY, 1), 1); })
SHOT(e_baby, { wd_cam(0, -14, 3.2f); night_fair(ta); lantern_1(ta); pip_at(ta, -8, 10); sunny_at(ta, 16);
    say_at_world("I saw fireworks once. But I was a baby, so it doesn't count.", ta, P4, 3.0f, -8, -8, 0); })
void tk_music(void) {
    hahaland_bed(0, 28, 0.03f, 0, 0);                                         /* ducked under the PIP motif */
    static const int PM[8] = { 79, 83, 86, 83, 81, 79, 76, 0 };              /* the PIP motif, pizzicato */
    for (int i = 0; i < 100; i++) { double t = i * 0.28; if (t > 28) break; if (t >= FZ && t < M2) continue; if (PM[i % 8]) tk_note(t, 0.06, PM[i % 8], TKV_TRI, 0.1f); }
    tk_note(4.6, 0.05, 50, TKV_NOISE, 0.15f);                                /* Pip hops down */
    babble_line(P1, 4.0f, 80, 0.18f); babble_line(P2, 0.8f, 80, 0.18f); babble_line(P5, 1.2f, 81, 0.18f); babble_line(P6, 0.9f, 82, 0.18f); babble_line(P3, 1.3f, 83, 0.18f); babble_line(P4, 3.0f, 79, 0.16f);
    line_at("a1_10_mayor_01", M1); line_at("a1_10_mayor_02", M2);
    tk_slide(FLOP, 0.3, 70, 64, TKV_SINE, 0.1f);
}
