/* sc50.c - SC50 (4-02). A sniff. Pip, the only one still breathing, crying,
 * Mr. Flops in a headlock. "My eyes are just doing a rain." "...It's just a
 * NAME!" Up the steps; MUTE ALL; "CLINKER! The name is CLINKER! It says so
 * RIGHT HERE!" The purple drains; the bead swells gold; the town thaws. */
#include "act4_common.h"
#define SHOTS(X) X(main, 30.0)
#include "scene_tl.h"
static const float P1 = 2.6f, P2 = 7.0f, CLIMB = 9.0f, MUTE = 12.6f, P3 = 12.8f, DRAIN = 17.6f, THAW = 21.0f, TURN = 24.0f;
static const Cut C[] = { { 0, 40, 6, 4.0f, 4.4f, 0, 0 }, { 5.0f, 20, -4, 2.0f, 2.2f, 0, 0 }, { P2 - 0.3f, 40, 6, 4.2f, 4.6f, 0, 0 }, { CLIMB, 4, -14, 2.6f, 2.8f, -6, 0 },
    { P3 - 0.2f, 2, -16, 4.4f, 4.8f, 0, 0 }, { DRAIN, -40, -30, 3.4f, 3.8f, 0, 0 }, { 19.6f, 6, -20, 5.4f, 5.6f, 0, 0 }, { THAW, 14, -8, 1.6f, 1.8f, 0, 0 }, { TURN, 0, -14, 3.6f, 4.0f, 0, 0 } };
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    cut_at(C, 9, ta, 30);
    float poss = 1 - seg(ta, DRAIN, DRAIN + 0.7f);
    bead_flame = seg(ta, 19.6f, 20.6f);
    town(ta, 0, ta >= THAW ? THAW : -1, poss);
    float climb = seg(ta, CLIMB, MUTE - 0.4f);
    Critter p = cast_pip(lerp(40, 4, climb), lerp(20, -6, climb), ta * 30); p.flip = -1; p.armL = p.armR = 1.7f;   /* Mr. Flops in a two-armed headlock */
    p.blink = ta < 1.0f ? 1 : 0; p.squash = 1 - 0.04f * (ta < 1.0f);
    if (ta >= P3 && ta < P3 + 3) { p.armR = 2.4f; p.squash = 1.08f; p.mouth = 1; }   /* the paw jabs at the name */
    if (ta >= TURN) { p.armL = 1.2f; p.armR = 0.9f; }                               /* holds Mr. Flops properly */
    critter_draw(&p);
    for (int k = 0; k < 2; k++) if (ta < THAW) wd_blob(critter_mouth_x - 2 + k * 4, critter_head_y + fmodf(ta * 6 + k * 3, 6), 0.5f, 0.8f, 0, PAL_PAINT(PAINT_SKY, 2), WB_OUT);   /* tears */
    float mx = critter_mouth_x, my = critter_mouth_y;
    say_at_world("My eyes are just doing a rain.", ta, P1, 1.8f, mx, my, 1);
    say_at_world("...It's just a NAME!", ta, P2, 1.2f, mx, my, 1);
    say_at_world("CLINKER! The name is CLINKER! It says so RIGHT HERE!", ta, P3, 2.6f, mx, my, 2);
    if (ta >= 0 && ta < 0.8f) tk_text(6, 6, "(sniff)", PAL_WHITE, 1);
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    tk_note(0.1, 0.2, 70, TKV_NOISE, 0.08f);
    tk_slide(1.0, MUTE - 1.0, 33, 35, TKV_SAW, 0.05f);
    babble_line(P1, 1.8f, 80, 0.18f); babble_line(P2, 1.2f, 82, 0.2f); babble_line(P3, 2.6f, 84, 0.28f);
    static const int T[6] = { 72, 76, 79, 77, 76, 72 }; for (int i = 0; i < 6; i++) tk_note(17.0 + i * 0.5, 0.45, T[i], TKV_SINE, 0.12f);   /* TRUE NAME */
    for (int k = 0; k < 3; k++) tk_note(20.6, 4.0, 60 + k * 4 - (k == 2), TKV_TRI, 0.05f);   /* the choir, soft */
}
