/* sc15.c - SC15 (1-02) EXT. TOWN SIGN - SUNSET. The sign drops in; 500 rolls
 * to 501; TINK, the "1" falls off; a 1000 painted over underneath; pencil:
 * "and Sunny."; the ticket flutters onto Sunny's head; the "1" rolls away
 * downhill toward the lanterns, and Sunny follows. No dialogue. */
#include "tk.h"
#include "wd.h"
#include "sets.h"
#include "cast.h"
#include "show.h"
#include <math.h>

#define SHOTS(X) X(a_sign, 6.5) X(b_pencil, 4.0) X(c_ticket, 6.5)
#include "scene_tl.h"
#define INK PAL_PAINT(PAINT_JET, 1)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }
static float spring(float f, float f0, float amp) { return wd_spring(f, f0, amp, 0.55f, 0.11f); }
#define T_DROP 0.3f
#define T_ROLL 3.0f
#define T_TINK 3.3f
#define T_PENCIL 7.0f
#define T_TICKET 10.8f
#define T_AWAY 12.6f

static void sky(void) { static const uint8_t c[] = { PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PEACH, 6), PAL_PAINT(PAINT_LEMON, 2) }; tk_vgrad(0, 0, TW, TH, c, 3); }
static void ground(float t) {
    wd_rect(-300, 0, 600, 100, PAL_RAMP(RAMP_GRASS, 4), 0);
    wd_quad(-20, 0, 20, 0, 60, 100, -60, 100, PAL_PAINT(PAINT_CARAMEL, 2), 0);   /* the road */
    for (int i = 0; i < 6; i++) { float x = 140 + i * 16, y = -18 + i * 2; wd_line(x, y, x + 16, y + 2, 0.4f, PAL_PAINT(PAINT_CHOCOLATE, 1)); wd_blob(x + 8, y + 3, 1.6f, 2, 0, i & 1 ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_ROSE, 2), WB_OUT); }   /* lanterns, far off downhill */
    (void)t;
}
static void sign(float ta, int digits_text, float pencil) {
    float drop = seg(ta, T_DROP, T_DROP + 0.25f);
    float y = lerp(-140, -50, drop) + (ta >= T_DROP + 0.25f ? 0 : 0);
    float sq = 1 + spring(ta * 30, (T_DROP + 0.25f) * 30, -0.12f);
    wd_rect(-34, y + 22, 3, 30, PAL_PAINT(PAINT_CARAMEL, 0), INK); wd_rect(31, y + 22, 3, 30, PAL_PAINT(PAINT_CARAMEL, 0), INK);
    wd_rect(-40, y - 4 * sq, 80, 30 * sq, PAL_WHITE, PAL_PAINT(PAINT_ROSE, 0));
    wd_rect(-38, y - 2 * sq, 76, 26 * sq, PAL_RAMP(RAMP_PASTEL_PINK, 6), 0);
    int sx = (int)wd_sx(-36), sy = (int)wd_sy(y);
    tk_text(sx + 4, sy + 2, "HaHaland!", PAL_PAINT(PAINT_MAGENTA, 0), 2);
    tk_text(sx + 4, sy + 22, "Population:", PAL_PAINT(PAINT_NAVY, 1), 1);
    /* under the 500, faint, a 1000 painted over */
    tk_text(sx + 74, sy + 26, "1000", PAL_RAMP(RAMP_PASTEL_PINK, 5), 1);
    tk_text(sx + 72, sy + 22, digits_text ? "501" : "500", PAL_PAINT(PAINT_NAVY, 1), 1);
    tk_text(sx + 4, sy + 32, "lovely citizens", PAL_PAINT(PAINT_NAVY, 1), 1);
    if (pencil > 0) { int n = (int)(pencil * 10); char s[12] = "and Sunny."; s[n < 10 ? n : 10] = 0; tk_text(sx + 60, sy + 42, s, PAL_GRAY_D, 1); }
}
static void sunny(float ta, float x, float look, float squash) {
    Sunny u; sunny_default(&u, x, 30, 0.9f, ta * 30); u.look_x = look; u.look_y = ta < 6 ? -0.8f : 0; u.squash = squash; sunny_draw(&u);
}
void scene_a_sign(int f, int n) {
    float ta = f / 30.0f;
    sky(); wd_cam(0, -10, 1.6f); ground(ta);
    sign(ta, ta >= T_ROLL && ta < T_TINK + 0.1f, 0);
    if (ta >= T_TINK) { float u = seg(ta, T_TINK, T_TINK + 0.4f); wd_text(lerp(20, 26, u), lerp(-28, 28, u) - 6 * sinf(u * 3.14f), "1", 1.2f, PAL_PAINT(PAINT_NAVY, 1), PAL_WHITE); }
    sunny(ta, 10, 0, 1);
    (void)n;
}
void scene_a_sign_audio(double t0, double dur) {
    tk_sfx(t0 + T_DROP + 0.25, SFX_THUD, 0.4f);
    tk_note(t0 + T_ROLL, 0.3, 88, TKV_SINE, 0.25f); tk_note(t0 + T_ROLL + 0.1, 0.3, 93, TKV_SINE, 0.2f);   /* a cheerful chime */
    tk_note(t0 + T_TINK, 0.05, 100, TKV_SINE, 0.3f);                                                      /* TINK */
}
void scene_b_pencil(int f, int n) {
    float ta = 6.5f + f / 30.0f;
    sky(); wd_cam(4, -36, 3.0f); ground(ta);
    sign(ta, 0, seg(ta, T_PENCIL, T_PENCIL + 1.2f));
    (void)n;
}
void scene_b_pencil_audio(double t0, double dur) {
    tk_slide(t0 + 0.5, 1.2, 70, 74, TKV_NOISE, 0.08f);                 /* a pencil scratch */
    static const int H[8] = { 72, 74, 76, 79, 76, 74, 72, 0 };          /* two bars of the HOME motif, music box */
    for (int i = 0; i < 8; i++) if (H[i]) tk_note(t0 + 1.8 + i * 0.28, 0.4, H[i] + 12, TKV_SINE, 0.2f);
}
void scene_c_ticket(int f, int n) {
    float ta = 10.5f + f / 30.0f;
    sky(); wd_cam(lerp(10, 50, seg(ta, T_AWAY + 1.5f, 17)), -10, 2.2f); ground(ta);
    sign(ta, 0, 1);
    float look = ta < 11 ? sinf(ta * 4) : (ta < T_AWAY ? 0 : 1);
    float sq = 1 + spring(ta * 30, (T_TICKET + 0.5f) * 30, 0.1f);
    float sxp = ta < T_AWAY + 1.5f ? 10 : lerp(10, 60, seg(ta, T_AWAY + 1.5f, 17));
    sunny(ta, sxp, look, sq);
    /* the ticket: MIDSUMMER GIGGLE, ADMIT ONE (NEW) */
    float tu = seg(ta, T_TICKET - 0.3f, T_TICKET + 0.5f);
    float tx = lerp(-10, sxp, tu) + 6 * sinf(ta * 6) * (1 - tu), ty = lerp(-70, 30 - 30, tu);
    wd_rect(tx - 6, ty - 3, 12, 6, PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_ROSE, 0));
    if (ta >= T_TICKET + 0.6f && ta < T_TICKET + 2.0f) { tk_rect(150, 10, 84, 26, PAL_PAINT(PAINT_BUBBLEGUM, 2)); tk_frame(150, 10, 84, 26, PAL_PAINT(PAINT_ROSE, 0)); tk_text(154, 14, "MIDSUMMER GIGGLE", PAL_PAINT(PAINT_MAGENTA, 0), 1); tk_text(154, 24, "ADMIT ONE (NEW)", PAL_PAINT(PAINT_NAVY, 1), 1); }
    /* the fallen "1" tips over and rolls away downhill */
    float ru = seg(ta, T_AWAY, 17);
    wd_text(lerp(26, 120, ru), lerp(28, 18, ru), "1", 1.2f, PAL_PAINT(PAINT_NAVY, 1), PAL_WHITE);
    (void)n;
}
void scene_c_ticket_audio(double t0, double dur) {
    tk_slide(t0 + 0.3, 0.8, 84, 78, TKV_SINE, 0.08f);
    tk_note(t0 + 0.8, 0.06, 70, TKV_NOISE, 0.12f);      /* the ticket lands */
    for (int k = 0; k < 8; k++) tk_note(t0 + 2.1 + k * 0.3, 0.03, 82 - k, TKV_SINE, 0.08f);   /* the 1 rolling */
}
void tk_music(void) {}
