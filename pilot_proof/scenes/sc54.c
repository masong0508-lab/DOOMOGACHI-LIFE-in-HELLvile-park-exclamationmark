/* sc54.c - SC54 (4-06) THE MINGLE CRATE. "I didn't cry." "I did a little."
 * "...Not even one teeny firework?" The Mayor: crate, Wall, candle. "One."
 * "A small one." "For the patrol." "Patrol! Three steps back!" Step step step,
 * one more for Mr. Flops; Nana steps back too. DO NOT OPEN: he opens it. */
#include "act4_common.h"
#define SHOTS(X) X(main, 35.0)
#include "scene_tl.h"
static const float P1 = 1.4f, P2 = 3.2f, P3 = 5.4f, LOOKS = 8.0f, M1 = 11.6f, M2 = 13.2f, M3 = 15.0f, P4 = 17.0f, STEPS = 18.6f, NANA = 22.4f, OPEN = 24.0f, CANDLE = 29.0f, GO = 32.0f;
static const Cut C[] = { { 0, 30, -14, 3.6f, 4.0f, 0, 0 }, { LOOKS, 10, -20, 2.2f, 2.4f, 0, 0 }, { M1 - 0.3f, 14, -30, 3.8f, 4.2f, 0, 0 }, { P4 - 0.3f, 30, -10, 2.4f, 2.6f, 10, 0 },
    { OPEN, 34, -24, 3.6f, 4.2f, 0, 1 }, { CANDLE, 10, -20, 2.0f, 2.2f, 0, 0 }, { GO, 20, -20, 1.6f, 1.6f, -30, 0 } };
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    int k = cut_at(C, 7, ta, 35);
    bead_flame = 1; glow_on = 0; set_sky_dawn(ta * 30, 0.25f); set_wall(ta * 30); Banner b = { 0 }; set_bandstand(ta * 30, &b, 0); tk_post_darken(5);
    for (int i = 0; i < 3; i++) wd_rect(10 + i * 20, -14 + i * 3, 18, 3, PAL_PAINT(PAINT_CARAMEL, 1), INK);   /* thrown boards */
    float lid = seg(ta, OPEN + 0.4f, OPEN + 1.4f);
    wd_rect(20, -30, 34, 20, PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CHOCOLATE, 1));
    tk_text((int)wd_sx(22), (int)wd_sy(-28), "MINGLE", PAL_PAINT(PAINT_CHOCOLATE, 1), 1);
    if (lid > 0) { wd_quad(20, -30, 54, -30, 54 - lid * 4, -30 - lid * 12, 20 - lid * 4, -30 - lid * 12, PAL_PAINT(PAINT_CARAMEL, 2), INK);
        for (int r = 0; r < 4; r++) { wd_tube(26 + r * 7, -30, 27 + r * 7, -38, 1.2f, 1, INK, PAL_PAINT(PAINT_ROSE, 0), PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_ROSE, 2)); }   /* a nest of rockets in straw */
        for (int s2 = 0; s2 < 14; s2++) { uint32_t h = tk_hash(s2 * 17u); float sx2 = 22 + (h % 30), sy2 = -30 - (h >> 6) % 4; wd_line(sx2, sy2, sx2 + 3 - (h >> 9) % 6, sy2 - 2 - (h >> 12) % 3, 0.35f, PAL_PAINT(PAINT_HONEY, 2)); }   /* straw */
        float age = ta - OPEN - 0.4f;
        if (age < 4) for (int d = 0; d < 16; d++) { uint32_t h = tk_hash(d * 31u + 5u); float dx = 24 + (h % 28) + sinf(age * 1.5f + d) * 4 * age, dy = -32 - age * (4 + (h >> 8) % 6) ; float r = 1.0f + age * 0.5f;
            if (age < 3.5f) wd_blob(dx, dy, r, r * 0.8f, 0, (h & 1) ? PAL_PAINT(PAINT_BONE, 1) : PAL_PAINT(PAINT_BONE, 2), 0); }   /* eleven years of dust */ }
    float back = ta >= STEPS ? 0 : 0; int st = ta >= STEPS ? (int)((ta - STEPS) / 0.27f) : 0; if (st > 3) st = 3; if (ta >= STEPS + 1.6f) st = 4;
    Critter p = cast_pip(ta < P4 ? 37 : 60 + st * 6, ta < P4 ? -30 : 0, ta * 30); p.flip = -1; p.kneel = ta < P4; p.look_y = ta < P3 + 1 ? 0.5f : -0.3f;
    if (ta >= M3 + 0.4f) p.ear_turn = 0; if (ta >= P4) { p.armR = ta < P4 + 1 ? 2.9f : 0.5f; }
    p.mouth = lip("a", (int)((ta - P1) * 30), 30) + lip("b", (int)((ta - P2) * 30), 30) + lip("c", (int)((ta - P3) * 30), 50) + lip("d", (int)((ta - P4) * 30), 40);
    if (ta >= STEPS + 1.0f && ta < STEPS + 1.6f) p.look_y = 1;
    critter_draw(&p); float pmx = critter_mouth_x, pmy = critter_mouth_y; (void)back;
    Nana nn; nana_default(&nn, ta >= NANA ? 88 : 84, 2, 0.85f, ta * 30); nn.buttons = 3; nana_draw(&nn);
    Critter fz = fuzz_at(ta, 76, 0); fz.armR = ta >= CANDLE ? 1.6f : 1.2f; critter_draw(&fz);
    if (ta < CANDLE + 1) { wd_rect(critter_hand_x[1] - 0.8f, critter_hand_y[1] - 4, 1.6f, 4, PAL_PAINT(PAINT_BONE, 2), INK); wd_blob(critter_hand_x[1], critter_hand_y[1] - 5.4f, 0.8f, 1.4f, 0, PAL_PAINT(PAINT_LEMON, 2), WB_OUT); }
    Mayor m; mayor_default(&m, ta < OPEN - 1 ? 0 : lerp(0, 66, seg(ta, OPEN - 1, OPEN)), -10, 0.9f, ta * 30); if (ta >= OPEN - 1 && ta < GO) m.flip = -1; m.flutter = 0.1f;
    m.look_x = ta >= LOOKS && ta < LOOKS + 1.2f ? 1 : ta >= LOOKS + 1.2f && ta < LOOKS + 2.4f ? 1 : ta >= LOOKS + 2.4f && ta < M1 ? 1 : 0;
    m.look_y = ta >= LOOKS + 1.2f && ta < LOOKS + 2.4f ? -0.6f : 0;
    if (ta >= OPEN) m.armR = 1.4f; if (ta >= CANDLE + 0.5f) m.armL = 1.4f;
    m.mouth = lip("a4_06_mayor_01", (int)((ta - M1) * 30), 20) + lip("a4_06_mayor_02", (int)((ta - M2) * 30), 30) + lip("a4_06_mayor_03", (int)((ta - M3) * 30), 30);
    if (ta >= GO) m.x = lerp(30, -40, seg(ta, GO, 35));
    mayor_draw(&m); float mx = mayor_head_x + 7, my = mayor_head_y + 2;
    if (ta >= CANDLE + 1) { wd_rect(mayor_hand_x[0] - 0.8f, mayor_hand_y[0] - 4, 1.6f, 4, PAL_PAINT(PAINT_BONE, 2), INK); wd_blob(mayor_hand_x[0], mayor_hand_y[0] - 5.4f, 0.8f, 1.4f, 0, PAL_PAINT(PAINT_LEMON, 2), WB_OUT); }
    say_at_world("I didn't cry.", ta, P1, 0.9f, pmx, pmy, 2);
    say_at_world("I did a little.", ta, P2, 1.0f, pmx, pmy, 2);
    say_at_world("...Not even one teeny firework?", ta, P3, 1.8f, pmx, pmy, 2);
    say_at_world("One.", ta, M1, 0.7f, mx, my, -1);
    say_at_world("A small one.", ta, M2, 1.0f, mx, my, -1);
    say_at_world("For the patrol.", ta, M3, 1.0f, mx, my, -1);
    say_at_world("Patrol! Three steps back!", ta, P4, 1.2f, pmx, pmy, 1);
    if (k == 1 && ta < OPEN + 1.2f) tk_text(150, 20, "(creak)", PAL_WHITE, 1);
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    static const int PM[8] = { 79, 83, 86, 83, 81, 79, 76, 0 }; for (int i = 0; i < 24; i++) { double t = i * 0.36; if (t > LOOKS) break; if (PM[i % 8]) tk_note(t, 0.06, PM[i % 8], TKV_TRI, 0.08f); }
    babble_line(P1, 0.9f, 80, 0.16f); babble_line(P2, 1.0f, 79, 0.14f); babble_line(P3, 1.8f, 81, 0.14f);
    line_at("a4_06_mayor_01", M1); line_at("a4_06_mayor_02", M2); line_at("a4_06_mayor_03", M3);
    babble_line(P4, 1.2f, 83, 0.2f);
    for (int s = 0; s < 4; s++) tk_note(STEPS + s * 0.27 + (s == 3 ? 0.6 : 0), 0.04, 50, TKV_NOISE, 0.12f);
    tk_slide(OPEN + 0.4, 1.0, 46, 50, TKV_NOISE, 0.12f);
    static const int MM[8] = { 60, 64, 67, 65, 64, 62, 60, 0 }; for (int i = 0; i < 20; i++) if (MM[i % 8]) tk_note(P4 + i * 0.5, 0.48, MM[i % 8], TKV_TRI, 0.1f);
}
