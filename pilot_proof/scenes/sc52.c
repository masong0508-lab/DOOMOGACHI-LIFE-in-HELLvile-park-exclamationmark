/* sc52.c - SC52 (4-04). Fuzz with the last candle, gold again. "...I got
 * lucky." "You can't run a church on luck." Sunny hugs him anyway. pff.
 * His candle arm goes straight out, the flame clear of two plush people. */
#include "act4_common.h"
#define SHOTS(X) X(main, 20.0)
#include "scene_tl.h"
static const float F1 = 1.6f, F2 = 4.6f, HUG = 8.4f, ARM = 11.0f, OUT = 13.6f;
static const Cut C[] = { { 0, 14, -24, 3.6f, 3.9f, 0, 0 }, { 3.2f, 16, -24, 3.2f, 3.4f, 0, 0 }, { 7.6f, 4, -12, 2.6f, 2.9f, 0, 0 }, { OUT, 2, -14, 2.0f, 2.2f, 0, 0 } };
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    cut_at(C, 4, ta, 20);
    glow_on = 0; dark_stage(ta, 0, 0, 0);
    Critter fz = fuzz_at(ta, 0, -8); fz.kneel = 1; fz.look_y = 1;
    fz.armL = fz.armR = 1.3f + (ta < HUG ? 0.04f * sinf(ta * 60) : 0);   /* shaking paws */
    if (ta >= ARM) fz.armL = 2.0f;
    if (ta >= OUT) fz.armR = 1.57f + 0.0f;                                   /* the candle arm, straight out */
    if (ta >= F2 && ta < F2 + 1.6f) { fz.look_x = 0.6f * sinf((ta - F2) * 7); fz.look_y = 0.4f; fz.squash = 1.02f; }   /* a rueful head shake */
    fz.mouth = lip("a", (int)((ta - F1) * 30), 40) + lip("b", (int)((ta - F2) * 30), 50);
    fz.squash = ta >= HUG && ta < HUG + 0.2f ? 0.9f : 1;
    critter_draw(&fz); float mx = critter_mouth_x, my = critter_mouth_y;
    float cx = critter_hand_x[1] + (ta >= OUT ? 6 : 0), cy = critter_hand_y[1] - 2;
    wd_rect(cx - 0.8f, cy - 4, 1.6f, 4, PAL_PAINT(PAINT_BONE, 2), INK); wd_blob(cx, cy - 5.4f, 0.8f, 1.4f, 0, PAL_PAINT(PAINT_LEMON, 2), WB_OUT);
    if (ta >= HUG - 0.6f) { float in = seg(ta, HUG - 0.6f, HUG); Sunny u; sunny_default(&u, lerp(26, 8, in), -6, 0.8f, ta * 30); u.flip = -1; u.page = ta >= HUG ? 0.3f : 0; u.no_page = 1; u.squash = ta >= HUG && ta < HUG + 0.2f ? 0.86f : 1; u.blink = ta >= HUG; sunny_draw(&u); pet_draw(u.x, -6 - 23 * 0.8f - 4, 0.7f, ta * 30, -1, 0); }
    say_at_world("...I got lucky.", ta, F1, 1.2f, mx, my, 1);
    say_at_world("You can't run a church on luck.", ta, F2, 1.6f, mx, my, 1);
    if (ta >= HUG && ta < HUG + 0.6f) tk_text(150, 30, "pff", PAL_WHITE, 1);
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    babble_line(F1, 1.2f, 48, 0.18f); babble_line(F2, 1.6f, 50, 0.18f);
    tk_note(HUG, 0.1, 60, TKV_NOISE, 0.06f);
    static const int A[4] = { 65, 64, 62, 60 }; for (int i = 0; i < 4; i++) tk_note(HUG + 0.6 + i * 0.86, 0.8, A[i], TKV_TRI, 0.14f);   /* amen, half tempo */
}
