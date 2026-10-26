/* sc14.c - SC14 (1-01) ACT CARD / EXT. CANDY HILLS ROAD - SUNSET (18:00).
 * The clipboard card; the pink bus at a constant speed over the hills; the
 * brakes, the scratch, Sunny thrown off the step; BOING, the suitcase. No dialogue. */
#include "tk.h"
#include "wd.h"
#include "sets.h"
#include "cast.h"
#include "show.h"
#include <math.h>

#define SHOTS(X) X(a_card, 3.0) X(b_hills, 6.5) X(c_drop, 8.5)
#include "scene_tl.h"
#define INK PAL_PAINT(PAINT_JET, 1)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }
static float spring(float f, float f0, float amp) { return wd_spring(f, f0, amp, 0.55f, 0.11f); }
#define T_BRAKE 9.5f
#define T_THROW 10.0f
#define T_CASE  10.27f
#define T_DOOR  12.0f
#define T_LEAVE 12.6f

void scene_a_card(int f, int n) {
    tk_clear(PAL_PAINT(PAINT_CARAMEL, 1));
    tk_rect(40, 10, 160, 150, PAL_UI_CREAM); tk_frame(40, 10, 160, 150, PAL_PAINT(PAINT_CHOCOLATE, 1));
    tk_rect(100, 4, 40, 12, PAL_GRAY_L); tk_frame(100, 4, 40, 12, PAL_GRAY_D);
    static const char *L[5] = { "18:00 Welcome!", "19:00 Bake-Off!", "20:00 Bandstand!", "21:00 (we don't do that)", "22:00 Bedtime!" };
    for (int i = 0; i < 5 && f >= i * 8; i++) tk_text(56, 32 + i * 22, L[i], PAL_PAINT(PAINT_NAVY, 1), 1);
    if (f >= 50) { int w = f >= 56 ? 10 : (f - 50) * 2; tk_line(44, 36, 44 + w / 2, 40, PAL_PAINT(PAINT_CHERRY, 1)); tk_line(44 + w / 2, 40, 44 + w, 28, PAL_PAINT(PAINT_CHERRY, 1)); }
    (void)n;
}
void scene_a_card_audio(double t0, double dur) { tk_note(t0, 0.3, 84, TKV_SINE, 0.3f); tk_note(t0 + 0.12, 0.3, 79, TKV_SINE, 0.25f); for (int i = 0; i < 5; i++) tk_note(t0 + i * 8 / 30.0, 0.04, 70, TKV_NOISE, 0.08f); tk_note(t0 + 50 / 30.0, 0.06, 76, TKV_NOISE, 0.12f); }

static float hill_y(float x) { return -18 * sinf(x * 0.025f) - 10 * sinf(x * 0.061f + 1); }
static void hills(float t) {
    static const uint8_t cols[] = { PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PEACH, 6), PAL_PAINT(PAINT_LEMON, 2) };
    tk_vgrad(0, 0, TW, TH, cols, 3);
    wd_blob(80, -60, 30, 30, 0, PAL_PAINT(PAINT_LEMON, 2), 0);   /* the low sun */
    for (int k = 0; k < 6; k++) wd_blob(-200 + k * 90, 0, 70, 30, 0, PAL_PAINT(PAINT_MINT, 1), 0);   /* far candy hills */
    for (int x = -260; x < 260; x += 2) { float y = hill_y(x); wd_rect((float)x, y, 2.2f, 80, PAL_RAMP(RAMP_GRASS, 4), 0); wd_rect((float)x, y - 1.5f, 2.2f, 1.6f, PAL_RAMP(RAMP_GRASS, 5), 0); }
    for (int k = 0; k < 9; k++) { float x = -240 + k * 60; float y = hill_y(x); wd_line(x, y - 8, x, y, 0.8f, PAL_PAINT(PAINT_BUBBLEGUM, 1)); wd_blob_ol(x, y - 10, 3, 3, 0, k & 1 ? PAL_PAINT(PAINT_ROSE, 2) : PAL_PAINT(PAINT_MINT, 2), INK, 0); }   /* lollipops */
    (void)t;
}
static float bus_x(float ta) {
    if (ta < T_BRAKE) return -260 + (ta - 3.0f) * 40;   /* no easing: not up the hills, not down them */
    if (ta < T_LEAVE) return -260 + (T_BRAKE - 3.0f) * 40;
    return -260 + (T_BRAKE - 3.0f) * 40 + (ta - T_LEAVE) * 40;   /* leaves at exactly the speed it arrived */
}
static void bus(float x, float ta, int door) {
    float y = hill_y(x);
    wd_rect(x - 22, y - 20, 44, 18, PAL_PAINT(PAINT_BUBBLEGUM, 1), INK);
    wd_rect(x - 22, y - 8, 44, 3, PAL_PAINT(PAINT_ROSE, 1), 0);
    for (int k = 0; k < 4; k++) wd_rect(x - 18 + k * 9, y - 17, 7, 6, PAL_PAINT(PAINT_SKY, 2), INK);
    wd_rect(x + 14, y - 17, 6, 15, door ? PAL_PAINT(PAINT_NAVY, 0) : PAL_PAINT(PAINT_SKY, 2), INK);   /* the folding door */
    for (int k = -1; k <= 1; k += 2) { wd_blob_ol(x + k * 13, y - 2, 4, 4, ta * 6, PAL_PAINT(PAINT_CHARCOAL, 1), INK, 0); wd_blob(x + k * 13, y - 2, 1.5f, 1.5f, 0, PAL_GRAY_L, 0); }
    /* Sunny in the back window, ears up */
    if (!door && ta < T_THROW) { wd_blob(x - 16, y - 13, 2.4f, 2.2f, 0, PAL_PAINT(PAINT_BONE, 2), 0); wd_line(x - 17, y - 15, x - 17.5f, y - 19, 0.8f, PAL_PAINT(PAINT_BONE, 2)); wd_line(x - 15, y - 15, x - 14.5f, y - 19, 0.8f, PAL_PAINT(PAINT_BONE, 2)); }
}
void scene_b_hills(int f, int n) {
    float ta = 3.0f + f / 30.0f;
    wd_cam(lerp(-200, -60, seg(ta, 3.0f, 9.5f)), -20, 1.6f);
    hills(ta);
    bus(bus_x(ta), ta, 0);
    (void)n;
}
void scene_b_hills_audio(double t0, double dur) {
    static const int M[8] = { 72, 76, 79, 76, 77, 81, 79, 0 };   /* HaHaland Day, tinny, from the bus radio */
    for (int i = 0; i < 30; i++) { double t = t0 + i * 0.214; if (M[i % 8]) tk_note(t, 0.15, M[i % 8], TKV_SQ12, 0.12f); if (i % 2 == 0) tk_note(t, 0.2, 48 + (i / 8 % 2) * 5, TKV_TRI, 0.08f); }
    tk_slide(t0, 6.5, 40, 42, TKV_NOISE, 0.03f);   /* the engine */
}
void scene_c_drop(int f, int n) {
    float ta = 9.5f + f / 30.0f;
    float bx = bus_x(ta);
    wd_cam(lerp(bx + 10, bx + 30, seg(ta, T_LEAVE, 18)), hill_y(bx) - 14, 2.6f);
    hills(ta);
    bus(bx, ta, ta >= T_THROW - 0.2f && ta < T_DOOR);
    float sx = bx + 26, sy = hill_y(bx + 26) + 12;   /* in the road, in front of the bus */
    if (ta >= T_THROW) {
        Sunny u; sunny_default(&u, sx, sy, 0.75f, ta * 30);
        float fall = seg(ta, T_THROW, T_THROW + 0.25f);
        u.y = lerp(sy - 14, sy, fall);
        u.squash = 1 + spring(ta * 30, T_THROW * 30 + 7, -0.22f);   /* DROP, SQUASH 1.22/0.80 */
        u.suitcase = 0; u.look_x = ta > 13 ? 1 : 0; u.ears = ta > 11 ? 1 : 0.6f;
        sunny_draw(&u);
        if (ta >= T_CASE) {   /* BOING: it bounces twice and settles as if it had got there first */
            float a = ta - T_CASE, b = fabsf(sinf(a * 9)) * 10 * expf(-a * 3.5f);
            float cx = sx - 10, cy = sy + 1 - b;
            wd_rect(cx - 5, cy - 9, 10, 9, PAL_PAINT(PAINT_CARAMEL, 1), INK);
            wd_rect(cx - 2, cy - 10.5f, 4, 1.5f, PAL_PAINT(PAINT_CHOCOLATE, 1), 0);
            if (a < 0.4f) tk_text_outline((int)wd_sx(cx) - 12, (int)wd_sy(cy - 22), "BOING", PAL_WHITE, PAL_PAINT(PAINT_ROSE, 0), 1);
        }
    }
    if (ta >= T_LEAVE) for (int k = 0; k < 8; k++) wd_blob(bx - 26 - k * 5, hill_y(bx) - 2 - (k % 3), 3 + k * 0.5f, 2.4f, 0, PAL_PAINT(PAINT_BUBBLEGUM, 2), 0);   /* pink dust */
    (void)n;
}
void scene_c_drop_audio(double t0, double dur) {
    tk_slide(t0, 0.5, 76, 52, TKV_SQ12, 0.12f);                      /* the radio slides down and dies */
    tk_sfx(t0, SFX_SCRATCH, 0.4f);
    tk_note(t0 + 0.5, 0.08, 45, TKV_NOISE, 0.3f);                    /* Sunny lands sitting */
    tk_sfx(t0 + 0.77, SFX_BOING, 0.5f);
    tk_note(t0 + 2.5, 0.2, 60, TKV_NOISE, 0.12f);                    /* the door folds shut */
    tk_slide(t0 + 3.1, 5, 40, 40, TKV_NOISE, 0.03f);
}

void tk_music(void) {}
