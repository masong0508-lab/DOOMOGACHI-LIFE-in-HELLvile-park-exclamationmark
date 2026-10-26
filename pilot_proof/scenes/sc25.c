/* sc25.c - SC25 (1-12) BANDSTAND - NIGHT (20:00). The speech: three pages,
 * one sentence. "Welcome to HaHaland, where..." fwip fwip fwip "hee. hee."
 * "It kills every time." It does. Ba-dum-CLACK. A fourth, grey sheet shows
 * for one second; he doesn't turn it over. HaHaland Day on the ribs. */
#include "fair_common.h"
#include "mayor.h"
#define SHOTS(X) X(a_hand, 4.0) X(b_speech, 7.0) X(c_kills, 4.5) X(d_grey, 4.5)
#include "scene_tl.h"
static const float T_TAP = 3.0f, M1 = 4.4f, M2 = 7.6f, M3 = 9.6f, T_LAUGH = 11.3f, T_RIM = 11.9f, T_GREY = 13.0f, T_SET = 15.2f;
static void stage(float ta) {
    set_sky_dawn(ta * 30, 0.25f); set_wall(ta * 30);
    Banner b = { 0 }; set_bandstand(ta * 30, &b, 0); tk_post_darken(10);
}
static void mayor_mic(float ta, float x) {
    Mayor m; mayor_default(&m, x, -10, 1.0f, ta * 30);
    m.armR = 1.3f; m.armL = 1.1f; m.expr = ta >= M2 && ta < M3 ? MX_NEUTRAL : MX_HAPPY;
    m.mouth = lip("a1_12_mayor_01", (int)((ta - M1) * 30), 56) + lip("a1_12_mayor_02", (int)((ta - M2) * 30), 36) + lip("a1_12_mayor_03", (int)((ta - M3) * 30), 48);
    m.fan = ta >= M3 ? 0.9f : 0.4f;
    mayor_draw(&m);
    float px = mayor_hand_x[1], py = mayor_hand_y[1] - 9;
    wd_rect(px - 5, py, 10, 12, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0));
    if (ta >= T_GREY && ta < T_GREY + 1.0f) wd_rect(px - 4, py - 2, 9, 3, PAL_GRAY_L, PAL_GRAY);   /* a fourth sheet, pale grey, not his paper */
    wd_line(x + 12, -10, x + 12, -30, 0.6f, PAL_GRAY_D); wd_blob_ol(x + 12, -32, 1.6f, 2, 0, PAL_GRAY, INK, 0);   /* the mic stand */
}
static void bones(float ta) {
    Bones bj; bones_default(&bj, -60, -10, 1.0f, ta * 30);
    bj.hitL = ta >= T_RIM && ta < T_RIM + 0.4f ? 1 : ta >= T_SET ? fabsf(sinf(ta * 7)) : 0.1f; bj.hitR = ta >= T_SET ? fabsf(cosf(ta * 7)) : 0.1f;
    bones_draw(&bj);
}
SHOT(a_hand, { wd_cam(0, -24, 1.8f); stage(ta); bones(ta); mayor_mic(ta, -20);
    Sunny u; sunny_default(&u, 4, 6, 0.8f, ta * 30); u.page = seg(ta, 0.4f, 1.4f) * (1 - seg(ta, 1.8f, 2.2f)); sunny_draw(&u);
    if (ta >= T_TAP && ta < T_TAP + 0.7f) tk_text_outline(150, 24, "THUMP THUMP", PAL_WHITE, PAL_PAINT(PAINT_NAVY, 1), 1); })
SHOT(b_speech, { wd_cam(-14, -38, 3.2f); stage(ta); mayor_mic(ta, -20);
    if (ta >= M1) { const char *t = "Welcome to HaHaland, where..."; int n = 27 + (ta >= M1 + 1.6f) + (ta >= M1 + 1.87f) + (ta >= M1 + 2.13f); char s[40] = { 0 }; for (int i = 0; i < n && t[i]; i++) s[i] = t[i]; bubble_side = 1; if (ta < M2) bubble(s, 40, (int)wd_sx(mayor_head_x + 6), (int)wd_sy(mayor_head_y + 2)); bubble_side = 0; }
    say_at_world("hee. hee.", ta, M2, 1.2f, mayor_head_x + 6, mayor_head_y + 2, 1); })
SHOT(c_kills, { wd_cam(-10, -28, 2.2f); stage(ta); bones(ta); mayor_mic(ta, -20);
    say_at_world("Short and sweet! It kills every time.", ta, M3, 1.7f, mayor_head_x + 6, mayor_head_y + 2, 1);
    if (ta >= T_RIM && ta < T_RIM + 0.6f) tk_text_outline(20, 20, "ba-dum-CLACK", PAL_WHITE, PAL_PAINT(PAINT_CHERRY, 0), 1);
    CrowdOpts o = { 0 }; o.n = 10; o.x0 = -90; o.x1 = 90; o.y0 = 16; o.y1 = 30; o.t = ta * 30; o.seed = 71; o.freeze_at = -1; o.panic = 0; crowd_draw(&o); })
SHOT(d_grey, { wd_cam(-16, -36, lerp(3.6f, 2.0f, seg(ta, 14.2f, 17))); stage(ta); bones(ta); mayor_mic(ta, -20);
    CrowdOpts o = { 0 }; o.n = 10; o.x0 = -90; o.x1 = 90; o.y0 = 16; o.y1 = 30; o.t = ta * 30; o.seed = 71; o.freeze_at = -1; crowd_draw(&o); })
void tk_music(void) {
    tk_note(T_TAP, 0.08, 36, TKV_NOISE, 0.3f); tk_note(T_TAP + 0.3, 0.08, 36, TKV_NOISE, 0.3f);
    line_at("a1_12_mayor_01", M1);
    for (int k = 0; k < 3; k++) tk_note(M1 + 1.6 + k * 0.27, 0.06, 78, TKV_NOISE, 0.12f);   /* fwip fwip fwip */
    line_at("a1_12_mayor_02", M2); line_at("a1_12_mayor_03", M3);
    for (int v = 0; v < 8; v++) for (int k = 0; k < 5; k++) tk_slide(T_LAUGH + v * 0.03 + k * 0.14, 0.1, 64 + v * 2, 68 + v * 2, TKV_TRI, 0.04f);
    tk_snare(T_RIM, 0.2f); tk_snare(T_RIM + 0.15, 0.2f); tk_note(T_RIM + 0.35, 0.12, 84, TKV_SQ50, 0.3f);
    hahaland_bed(T_SET, 20, 0.12f, 0, 0);
}
