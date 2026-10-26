/* sc53.c - SC53 (4-05). The Mayor flaps up: "Right! Everybody home! Lovely
 * weather tonight!" "Nobody look at the boards!" (they all look). "Let's make
 * it OFFICIAL!" The grey sheet: You never asked where it went. Fold, fold.
 * "Old draft." 23:00 Home! The Giggle goes home; Pip sits on the crate. */
#include "act4_common.h"
#define SHOTS(X) X(main, 25.0)
#include "scene_tl.h"
static const float M1 = 1.4f, M3 = 6.0f, LOOK = 7.4f, M4 = 9.2f, FLIP = 11.0f, FOLD = 13.4f, M2 = 14.2f, WRITE = 17.0f, HOME = 20.0f;
static const Cut C[] = { { 0, -10, -26, 2.2f, 2.5f, 0, 0 }, { M3 - 0.2f, 20, -6, 1.5f, 1.6f, 0, 0 }, { M4 - 0.2f, -18, -36, 3.4f, 3.8f, 0, 0 }, { FLIP, 0, 0, 1, 1, 0, 1 },
    { FOLD, -18, -36, 3.8f, 4.0f, 0, 0 }, { WRITE, 0, 0, 1, 1, 0, 2 }, { HOME, 10, -14, 1.4f, 1.6f, 20, 0 } };
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    int k = cut_at(C, 7, ta, 25);
    if (k == 1) { tk_clear(PAL_PAINT(PAINT_NAVY, 0)); tk_rect(30, 10, 180, 140, PAL_GRAY_L); tk_frame(30, 10, 180, 140, PAL_GRAY); for (int y = 30; y < 140; y += 12) tk_line(40, y, 200, y, PAL_GRAY);
        tk_text(60, 62, "You never asked", PAL_PAINT(PAINT_NAVY, 1), 1); tk_text(60, 76, "where it went.", PAL_PAINT(PAINT_NAVY, 1), 1); return; }
    if (k == 2) { tk_clear(PAL_PAINT(PAINT_CARAMEL, 1)); tk_rect(40, 6, 160, 154, PAL_UI_CREAM);
        static const char *L[6] = { "18:00 Welcome!", "19:00 Bake-Off!", "20:00 Bandstand!", "21:00 (we don't do that)", "22:00 weather", "23:00 " };
        for (int i = 0; i < 6; i++) tk_text(56, 22 + i * 22, L[i], PAL_PAINT(PAINT_NAVY, 1), 1);
        int c = (int)((ta - WRITE) * 10); char s[8] = "Home!"; if (c < 5) s[c < 0 ? 0 : c] = 0; tk_text(98, 132, s, PAL_PAINT(PAINT_MAGENTA, 0), 1); return; }
    bead_flame = 1; glow_on = 0; dark_stage(ta, 0, 0, 0); candle_rail(ta);
    Bones bj; bones_default(&bj, -40, -10, 1.0f, ta * 30); bones_draw(&bj);
    float home = seg(ta, HOME, 25);
    CrowdOpts o = { 0 }; o.n = (int)(10 * (1 - home)) + 1; o.x0 = 20; o.x1 = 120; o.y0 = 18; o.y1 = 34; o.t = ta * 30; o.seed = 131; o.freeze_at = -1; o.turn = ta >= LOOK && ta < LOOK + 0.5f ? 1 : 0; crowd_draw(&o);
    Nana nn; nana_default(&nn, 20, 20, 0.85f, ta * 30); nn.buttons = 3; nn.look_x = ta >= M2 + 0.6f && ta < M2 + 1.0f ? -1 : 0; nana_draw(&nn);
    float up = seg(ta, 0, 0.8f);
    Mayor m; mayor_default(&m, -18, lerp(0, -10, up) - 6 * sinf(up * 3.14f), 1.0f, ta * 30);
    m.flutter = ta < 0.8f ? 1 : (ta >= FLIP && ta < M2) ? 0 : 0.4f; m.armL = m.armR = ta < M3 ? 2.6f : 1.2f; m.expr = MX_HAPPY;
    if (ta >= M3 && ta < LOOK) m.armR = 2.2f + 0.3f * sinf(ta * 14);   /* wagging a finger */
    if (ta >= M4 && ta < FLIP) m.armR = 2.4f;                          /* pen out, with a flourish */
    if (ta >= FLIP && ta < M2) { m.expr = MX_SHOCK; m.eye_pop = 0.5f; }
    m.mouth = lip("a4_05_mayor_01", (int)((ta - M1) * 30), 120) + lip("a4_05_mayor_03", (int)((ta - M3) * 30), 45) + lip("a4_05_mayor_04", (int)((ta - M4) * 30), 45) + lip("a4_05_mayor_02", (int)((ta - M2) * 30), 30);
    mayor_draw(&m);
    if (ta >= FOLD && ta < FOLD + 0.2f) wd_rect(mayor_hand_x[1] - 2, mayor_hand_y[1] - 4, 4 - (ta - FOLD) * 15, 5, PAL_GRAY_L, PAL_GRAY);
    float hx = mayor_head_x + 7, hy = mayor_head_y + 2;
    say_at_world("Right! Everybody home! Lovely weather tonight! Lovely, lovely weather!", ta, M1, 3.8f, hx, hy, 1);
    say_at_world("Nobody look at the boards!", ta, M3, 1.2f, hx, hy, 1);
    say_at_world("Let's make it OFFICIAL!", ta, M4, 1.4f, hx, hy, 1);
    say_at_world("Old draft!", ta, M2, 0.9f, hx, hy, 1);
    if (ta >= HOME + 1.5f) { Critter p = cast_pip(30, -14, ta * 30); p.kneel = 1; critter_draw(&p); wd_rect(22, -14, 18, 8, PAL_PAINT(PAINT_CARAMEL, 1), INK); }
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    static const int M[8] = { 72, 76, 79, 81, 79, 76, 77, 74 };
    for (int i = 0; ; i++) { double t = i * 0.25; if (t >= 25) break; if (t >= M2 - 0.5 && t < M2 + 0.5) continue; tk_note(t, 0.2, M[i % 8], TKV_SQ25, 0.07f); }   /* MAYOR motif, too bright */
    for (int k = 0; k < 6; k++) tk_slide(0.1 + k * 0.12, 0.06, 60, 64, TKV_NOISE, 0.1f);
    line_at("a4_05_mayor_01", M1); line_at("a4_05_mayor_03", M3); line_at("a4_05_mayor_04", M4); line_at("a4_05_mayor_02", M2);
    for (int v = 0; v < 6; v++) babble_line(5.4 + v * 0.03, 0.5f, 58 + v * 3, 0.04f);
    tk_note(FOLD, 0.05, 78, TKV_NOISE, 0.12f); tk_note(FOLD + 0.07, 0.05, 78, TKV_NOISE, 0.12f);
}
