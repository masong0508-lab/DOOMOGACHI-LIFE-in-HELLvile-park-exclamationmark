/* sc16.c - SC16 (1-03) EXT. GIGGLING FOUNTAIN, PARK GATE - SUNSET. Through
 * the gate under the GIGGLE banner (the balloon still tied over the middle);
 * the fountain giggles, late, at nobody. A soggy page: WELCOME SPEECH, p.1,
 * every sentence run, "Hee. Hee." in the margin. Then it giggles on time.
 * Sunny takes one careful step back. No dialogue. */
#include "tk.h"
#include "wd.h"
#include "sets.h"
#include "cast.h"
#include "show.h"
#include <math.h>

#define SHOTS(X) X(a_gate, 4.0) X(b_page, 5.5) X(c_ontime, 3.5)
#include "scene_tl.h"
#define INK PAL_PAINT(PAINT_JET, 1)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }
#define T_LATE  4.3f
#define T_FISH  5.6f
#define T_READ  6.8f
#define T_ONTIME 10.2f
#define T_BACK  11.0f

static void gate(float t) {
    static const uint8_t c[] = { PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PEACH, 6) };
    tk_vgrad(0, 0, TW, TH, c, 2);
    wd_rect(-300, 40, 600, 100, PAL_RAMP(RAMP_PEACH, 6), 0);
    for (int k = -1; k <= 1; k += 2) { wd_rect(k * 70 - 5, -60, 10, 100, PAL_WHITE, PAL_PAINT(PAINT_ROSE, 1)); for (int i = 0; i < 10; i++) wd_rect(k * 70 - 5, -60 + i * 10, 10, 4, PAL_PAINT(PAINT_ROSE, 2), 0); }
    wd_rect(-66, -66, 132, 18, PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_PAINT(PAINT_MAGENTA, 1));
    wd_text(-31, -62, "GIGGLE", 1.7f, PAL_WHITE, PAL_PAINT(PAINT_MAGENTA, 1));
    /* the cupcake balloon, still tied over the middle, still facing front */
    wd_blob_ol(-2, -58, 10, 8, 0, PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_ROSE, 0), WB_SHADE);
    wd_quad(-8, -52, 4, -52, 2, -44, -6, -44, PAL_PAINT(PAINT_CARAMEL, 2), PAL_PAINT(PAINT_CARAMEL, 0));
    wd_blob(-2, -66, 2, 2, 0, PAL_PAINT(PAINT_CHERRY, 1), WB_OUT);
    wd_blob(-5, -57, 0.9f, 0.9f, 0, INK, 0); wd_blob(1, -57, 0.9f, 0.9f, 0, INK, 0);
    (void)t;
}
static void sunny_at(float ta, float x, float look) {
    Sunny u; sunny_default(&u, x, 48, 0.9f, ta * 30); u.look_x = look; u.squash = 1 + 0.03f * fabsf(sinf(ta * 8)) * (ta < 3.6f); sunny_draw(&u);
}
void scene_a_gate(int f, int n) {
    float ta = f / 30.0f;
    wd_cam(0, -20, 1.5f); gate(ta);
    sunny_at(ta, lerp(-100, -8, seg(ta, 0, 3.6f)), 1);
    (void)n;
}
void scene_a_gate_audio(double t0, double dur) {
    static const int M[8] = { 72, 76, 79, 76, 77, 81, 79, 0 };   /* HaHaland Day + walla, filtered by distance */
    for (int i = 0; i < 40; i++) if (M[i % 8]) tk_note(t0 + i * 0.214, 0.15, M[i % 8] - 12, TKV_TRI, 0.05f);
    for (int k = 0; k < 6; k++) tk_slide(t0 + k * 0.5, 0.3, 62 + k, 66 + k, TKV_TRI, 0.02f);
}
static void fountain_shot(float ta, int giggling, float g0) {
    static const uint8_t c[] = { PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PEACH, 6) };
    tk_vgrad(0, 0, TW, TH, c, 2);
    wd_cam(FOUNT_X + 10, FOUNT_Y - 26, 2.4f);
    wd_rect(-300, FOUNT_Y - 30, 600, 200, PAL_RAMP(RAMP_PEACH, 6), 0);
    for (int k = 0; k < 6; k++) wd_line(-300, FOUNT_Y - 26 + k * 10.0f, 300, FOUNT_Y - 26 + k * 10.0f, 0.4f, PAL_RAMP(RAMP_PEACH, 5));
    for (int k = 0; k < 5; k++) wd_blob(FOUNT_X - 80 + k * 40, FOUNT_Y - 40, 20, 14, 0, PAL_RAMP(RAMP_GRASS, 4), WB_SHADE);
    set_fountain(ta * 30, giggling, g0 * 30);
}
void scene_b_page(int f, int n) {
    float ta = 4.0f + f / 30.0f;
    if (ta < T_READ) {
        fountain_shot(ta, ta >= T_LATE && ta < T_LATE + 1.0f, T_LATE);
        Sunny u; sunny_default(&u, FOUNT_X + 36, FOUNT_Y + 14, 1.0f, ta * 30); u.look_x = -1; u.flip = -1;
        u.no_page = 1; u.page = ta >= T_FISH ? 0.5f : 0; u.suitcase = 1;
        sunny_draw(&u);
        if (ta >= T_LATE && ta < T_LATE + 1.0f) tk_text_outline((int)wd_sx(FOUNT_X - 20), (int)wd_sy(FOUNT_Y - 50), "hee hee", PAL_WHITE, PAL_PAINT(PAINT_SKY, 0), 1);
        if (ta >= T_FISH - 0.5f) wd_rect(lerp(FOUNT_X + 4, sunny_hand_x - 4, seg(ta, T_FISH, T_FISH + 0.4f)), lerp(FOUNT_Y - 4, sunny_hand_y - 8, seg(ta, T_FISH, T_FISH + 0.4f)), 9, 11, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0));
    } else {
        /* the page: WELCOME SPEECH, p.1, every sentence run; in the margin, Hee. Hee. */
        tk_clear(PAL_PAINT(PAINT_SKY, 1));
        tk_rect(40, 6, 160, 150, PAL_WHITE); tk_frame(40, 6, 160, 150, PAL_PAINT(PAINT_SKY, 0));
        tk_text(50, 12, "WELCOME SPEECH, p.1", PAL_PAINT(PAINT_NAVY, 1), 1);
        for (int i = 0; i < 8; i++) {
            int y = 32 + i * 14;
            for (int x = 52; x < 150; x += 3) { uint32_t h = tk_hash(x * 7u + i); tk_rect(x, y + (int)(h % 4), 2, 3 + (int)((h >> 3) % 5), PAL_PAINT(PAINT_SKY, (h >> 6) & 1)); }   /* run ink */
            tk_text(158, y, "Hee.", PAL_PAINT(PAINT_NAVY, 1), 1);
        }
        for (int k = 0; k < 6; k++) tk_circle(60 + k * 25, 140 + (int)(sinf(ta * 3 + k) * 2), 1, PAL_PAINT(PAINT_SKY, 2));   /* drips */
    }
    (void)n;
}
void scene_b_page_audio(double t0, double dur) {
    /* [R8] the fountain giggles, late, at nobody (library take stand-in) */
    for (int k = 0; k < 2; k++) { tk_slide(t0 + 0.3 + k * 0.25, 0.08, 84, 89, TKV_SQ12, 0.1f); tk_slide(t0 + 0.31 + k * 0.25, 0.08, 72, 77, TKV_TRI, 0.08f); }
    tk_note(t0 + 1.6, 0.2, 60, TKV_NOISE, 0.08f);   /* splash: fished out */
}
void scene_c_ontime(int f, int n) {
    float ta = 9.5f + f / 30.0f;
    fountain_shot(ta, ta >= T_ONTIME && ta < T_ONTIME + 0.8f, T_ONTIME);
    float back = seg(ta, T_BACK, T_BACK + 0.5f);
    Sunny u; sunny_default(&u, FOUNT_X + 36 + back * 8, FOUNT_Y + 14, 1.0f, ta * 30); u.look_x = -1; u.flip = -1; u.no_page = 0; u.page = 0.4f; u.ears = 1.2f;
    u.squash = 1 - 0.05f * sinf(back * 3.14f);
    sunny_draw(&u);
    if (ta >= T_ONTIME && ta < T_ONTIME + 0.8f) tk_text_outline((int)wd_sx(FOUNT_X - 20), (int)wd_sy(FOUNT_Y - 50), "...Hee.", PAL_WHITE, PAL_PAINT(PAINT_SKY, 0), 1);
    (void)n;
}
void scene_c_ontime_audio(double t0, double dur) {
    tk_slide(t0 + 0.7, 0.12, 82, 78, TKV_SQ12, 0.12f); tk_slide(t0 + 0.71, 0.12, 70, 66, TKV_TRI, 0.1f);   /* on time this time */
    tk_note(t0 + 1.5, 0.05, 45, TKV_NOISE, 0.1f);                                                         /* one careful step back */
}
void tk_music(void) {}
