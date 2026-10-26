/* sc27.c - SC27 (1-14) BANDSTAND / THE BOARDS - NIGHT. Act One out. Fog over
 * the boards; lanterns go out from the south, faster and faster; red eyes in
 * pairs; "come and PLAY with us" (+ Clinker R8 late, + a child's "...us").
 * The fog stops at Lulla's guy-ropes. Fuzz's 4-frame memory. The tune bends;
 * the fog pours into the ribs: FWOOMP. Everything dark but the glowing ribs.
 * {d}"...Clack." */
#include "fair_common.h"
#include "mayor.h"
#define SHOTS(X) X(a_fog, 7.0) X(b_eyes, 6.0) X(c_tent, 5.0) X(d_fuzz, 0.1334) X(e_fuzz2, 3.5) X(f_inhale, 5.0) X(g_clack, 3.37)
#include "scene_tl.h"
static const float T_FOG = 1.0f, T_EYES = 8.0f, T_VOICE = 10.3f, T_FUZZ = 17.9f, T_BEND = 21.5f, T_FWOOMP = 23.0f, T_OUT = 24.0f, T_CLACK = 27.0f;
static const float PFF[7] = { 2.0f, 2.7f, 3.13f, 3.4f, 3.57f, 3.67f, 3.74f };
static int lanterns_lit(float ta) { int n = 9; for (int i = 0; i < 7; i++) if (ta >= PFF[i]) n--; if (ta >= T_OUT) n = 0; return n; }
static void stage(float ta, float fog) {
    set_sky_dawn(ta * 30, 0.25f); set_wall(ta * 30);
    Banner b = { 0 }; set_bandstand(ta * 30, &b, 0);
    int lit = lanterns_lit(ta);
    tk_post_darken(ta >= T_OUT ? 3 : 10 - (9 - lit) / 2);
    for (int i = 0; i < 9; i++) { float x = 70 - i * 18; if (i >= 9 - lit) wd_blob(x, -74, 1.5f, 2.4f, 0, PAL_PAINT(PAINT_LEMON, 2), 0); }
    for (int k = 0; k < 14; k++) { float x = 80 - fog * 120 + k * 12 + 4 * sinf(ta + k); wd_blob(x, -6 + (k & 1) * 4, 10, 4, 0, PAL_PAINT(PAINT_LAVENDER, 2), 0); }
}
static void bones(float ta, float swell) {
    Bones bj; bones_default(&bj, -40, -10, 1.0f, ta * 30);
    bj.hitL = ta < T_FWOOMP ? fabsf(sinf(ta * 7)) : 0.2f; bj.hitR = ta < T_FWOOMP ? fabsf(cosf(ta * 7)) : 0.2f;
    bj.squash = 1 + swell * 0.25f; bj.possess = ta >= T_FWOOMP ? 1 : 0;
    if (ta >= T_CLACK - 0.8f) bj.look = -0.2f;
    bones_draw(&bj);
}
SHOT(a_fog, { wd_cam(10, -36, 1.6f); stage(ta, seg(ta, T_FOG, 7)); bones(ta, 0);
    for (int i = 0; i < 7; i++) if (ta >= PFF[i] && ta < PFF[i] + 0.15f) tk_text(180 - i * 22, 20, "pff", PAL_WHITE, 1); })
SHOT(b_eyes, { wd_cam(36, -30, 2.4f); stage(ta, 1);
    static const float E[4] = { 0, 0.27f, 0.44f, 0.54f };
    for (int i = 0; i < 4; i++) if (ta >= T_EYES + E[i]) { float x = 20 + i * 14, y = -16 - (i & 1) * 6; wd_blob(x, y, 1.2f, 1.2f, 0, PAL_PAINT(PAINT_CHERRY, 1), 0); wd_blob(x + 3, y, 1.2f, 1.2f, 0, PAL_PAINT(PAINT_CHERRY, 1), 0); }
    if (ta >= T_VOICE) tk_caption(130, "come and PLAY with us", PAL_PAINT(PAINT_LAVENDER, 2), PAL_PAINT(PAINT_GRAPE, 0), 1);
    if (ta >= T_VOICE + 1.5f) tk_caption(144, "...us", PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_GRAPE, 0), 1); })
SHOT(c_tent, { fair_backdrop(ta * 30, -80, 80); wd_cam(0, -24, 2.0f); tk_post_darken(6);
    fair_tent(0, 60, 50, PAL_PAINT(PAINT_LAVENDER, 1));
    wd_blob(0, -16, 6, 6, 0, PAL_PAINT(PAINT_LEMON, 2), 0);   /* its lantern still lit inside */
    float fx = lerp(-80, -36, seg(ta, 14, 16)) - (ta > 16.6f ? (ta - 16.6f) * 20 : 0);
    for (int k = 0; k < 8; k++) wd_blob(fx - k * 8, -4 + (k & 1) * 3, 8, 4, 0, PAL_PAINT(PAINT_LAVENDER, 2), 0);
    wd_line(-30, 0, -36, -18, 0.4f, PAL_GRAY_D); })
SHOT(d_fuzz, { /* INSERT 4: sepia, the kids' tent eleven years ago; a cloud-shaped lady, a crowned silhouette, no string */
    tk_clear(PAL_PAINT(PAINT_CARAMEL, 2)); tk_rect(80, 30, 80, 110, PAL_PAINT(PAINT_CARAMEL, 0));
    tk_ellipse(120, 70, 24, 18, PAL_PAINT(PAINT_BONE, 2)); tk_rect(110, 64, 4, 3, PAL_UI_RED); tk_rect(126, 64, 4, 3, PAL_UI_RED);
    tk_ellipse(150, 60, 10, 8, PAL_PAINT(PAINT_CHOCOLATE, 0)); tk_tri(144, 52, 156, 52, 150, 44, PAL_PAINT(PAINT_CHOCOLATE, 0)); })
SHOT(e_fuzz2, { fair_backdrop(ta * 30, -80, 80); wd_cam(0, -20, 3.0f); tk_post_darken(8);
    wd_rect(-24, -20, 48, 20, PAL_PAINT(PAINT_ROSE, 2), INK); tk_text((int)wd_sx(-20), (int)wd_sy(-18), "FREE HUGS", PAL_PAINT(PAINT_MAGENTA, 0), 1);
    Critter fz = cast_fuzz(0, 10, T_FUZZ * 30); fz.armL = fz.armR = 1.5f; critter_draw(&fz);
    if (ta >= 18.0f) for (int s = 0; s < 2; s++) wd_blob_ol(critter_hand_x[s] + ((int)(ta * 30) & 1 ? 0.5f : -0.5f), critter_hand_y[s], 2, 2, 0, fz.fur, INK, 0); })
SHOT(f_inhale, { wd_cam(-36, -32, lerp(2.0f, 2.6f, seg(ta, 21.5f, 23))); stage(ta, 1);
    float sw = seg(ta, T_FWOOMP - 0.5f, T_FWOOMP - 0.17f);
    for (int k = 0; k < 10; k++) wd_blob(lerp(30 - k * 6, -40, sw), -20 + (k & 1) * 4, 7 * (1 - sw * 0.7f), 4, 0, PAL_PAINT(PAINT_LAVENDER, 2), 0);
    bones(ta, sw);
    if (ta >= T_FWOOMP && ta < T_FWOOMP + 0.6f) tk_text_outline(60, 20, "FWOOMP", PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0), 3);
    Sunny u; sunny_default(&u, 20, 14, 0.85f, ta * 30); u.page = 0.3f; u.no_page = 1; sunny_draw(&u);
    wd_blob_ol(sunny_hand_x - 2, sunny_hand_y - 3, 5, 4, 0, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0), WB_SHADE); })
SHOT(g_clack, { wd_cam(-40, -30, 2.8f); tk_clear(PAL_PAINT(PAINT_NAVY, 0));
    for (int k = 0; k < 6; k++) wd_blob(-40, -32 + k * 2.6f, 10 - k, 2, 0, PAL_PAINT(PAINT_GRAPE, 1), 0);   /* the ribcage glows like a paper lantern */
    bones(ta, 0);
    if (ta >= T_CLACK) verdict("...Clack.", ta - T_CLACK, 130); })
void tk_music(void) {
    static const int H[8] = { 72, 76, 79, 76, 77, 81, 79, 0 };
    for (int i = 0; ; i++) { double t = i * 0.214; if (t >= T_FWOOMP) break; int m = H[i % 8]; if (t >= T_BEND && i % 8 == 3) m = 70; if (m) tk_note(t, 0.15, m, TKV_SINE, 0.14f); }   /* alone, bright */
    tk_slide(T_FOG, 8, 36, 38, TKV_SAW, 0.06f);                                   /* the first OMINOUS cue */
    for (int i = 0; i < 7; i++) tk_note(PFF[i], 0.08, 60, TKV_NOISE, 0.12f);
    for (int i = 0; i < 4; i++) tk_note(T_EYES + i * 0.2, 0.04, 96, TKV_SINE, 0.15f);
    babble_line(T_VOICE, 1.5f, 52, 0.1f); babble_line(T_VOICE + 0.27, 1.5f, 48, 0.08f); babble_line(T_VOICE + 1.5, 0.5f, 76, 0.08f);
    tk_slide(T_FWOOMP - 0.5, 0.33, 40, 52, TKV_NOISE, 0.2f);
    tk_note(T_FWOOMP, 0.5, 30, TKV_SAW, 0.4f);
    tk_note(T_CLACK, 0.05, 90, TKV_SQ50, 0.2f);
}
