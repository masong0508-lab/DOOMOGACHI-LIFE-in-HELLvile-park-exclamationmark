/* ec.c - the three eyecatch pairs (SC28-29, SC38-39, SC47-48), 10 s each:
 * an OUT card in pure HaHaland, a 4-f reduced hellify flash, then the IN card
 * with exactly one change. Build with -DEC=1/2/3. */
#include "act4_common.h"
#define SHOTS(X) X(main, 10.0)
#include "scene_tl.h"
static void card_bg(void) { tk_clear(PAL_UI_CREAM); tk_frame(4, 4, TW - 8, TH - 8, PAL_UI_PINK_D); tk_frame(6, 6, TW - 12, TH - 12, PAL_UI_PINK); }
static void clock_corner(const char *t) { tk_rect(196, 10, 36, 14, PAL_WHITE); tk_frame(196, 10, 36, 14, PAL_PAINT(PAINT_NAVY, 1)); tk_text(199, 13, t, PAL_PAINT(PAINT_NAVY, 1), 1); }
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    int in = ta >= 5.0f, flash = ta >= 5.0f && ta < 5.0f + 4 / 30.0f;
    float sway = sinf(ta * 1.4f) * 0.06f;
    card_bg(); wd_cam(0, -10, lerp(2.6f, 2.9f, seg(ta, 0, 10)));
#if EC == 1
    clock_corner("21:00");
    wd_rect(-60, -50, 120, 14, PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_PAINT(PAINT_MAGENTA, 1)); tk_caption((int)wd_sy(-48), "GIGGLE", PAL_WHITE, PAL_PAINT(PAINT_MAGENTA, 1), 1);
    wd_line(0, -36, sinf(sway) * 10, -20, 0.4f, PAL_PAINT(PAINT_CHOCOLATE, 1));
    wd_blob_ol(sinf(sway) * 10, -12, 9, 11, sway, in ? PAL_PAINT(PAINT_GRAPE, 1) : PAL_PAINT(PAINT_LEMON, 2), INK, WB_SHADE);
    for (int k = -2; k <= 2; k++) wd_line(sinf(sway) * 10 + k * 3, -22, sinf(sway) * 10 + k * 3.4f, -2, 0.4f, in ? PAL_PAINT(PAINT_GRAPE, 0) : PAL_PAINT(PAINT_HONEY, 1));
    tk_text((int)wd_sx(-6 + sinf(sway) * 10), (int)wd_sy(-14), "DL", PAL_PAINT(PAINT_MAGENTA, 0), 1);
#elif EC == 2
    clock_corner("21:40");
    wd_line(-60, -40, 60, -36, 0.4f, PAL_PAINT(PAINT_CHOCOLATE, 1));
    for (int s = 0; s < 2; s++) { float x = -16 + s * 32, y = -28 + s * 2 + sinf(ta * 2 + s) * 1.2f; wd_line(x, -39 + s * 0.6f, x, y - 6, 0.3f, INK); wd_blob_ol(x, y, 8, 6, 0, PAL_PAINT(PAINT_BONE, 2), INK, WB_SHADE); wd_blob(x, y - 3, 6.4f, 2, 0, PAL_PAINT(PAINT_BONE, 1), 0); }
    if (!in) { bubble_side = 2; bubble("beep.", 5, (int)wd_sx(0), (int)wd_sy(-30)); bubble_side = 0; }
    else { puff_demon(16, -32, 1.2f, ta * 30, -1); bubble_side = 2; bubble("beep.", 5, (int)wd_sx(16), (int)wd_sy(-24)); bubble_side = 0; }
#else
    clock_corner("22:59");
    wd_rect(-40, 0, 80, 4, PAL_PAINT(PAINT_CARAMEL, 1), INK);
    for (int c = 0; c < 3; c++) { float x = -20 + c * 20; wd_rect(x - 2.5f, -16, 5, 16, PAL_PAINT(PAINT_BONE, 2), INK);
        if (c < 2) { for (int k = 0; k < 6; k++) wd_blob(x + sinf(k * 0.8f + ta) * 2, -20 - k * 2.4f, 0.8f, 0.8f, 0, PAL_GRAY_L, 0); }
        else { float lean = in ? 0.5f : 0; wd_blob(x + lean * 3, -21, 2.2f, 4, lean, PAL_PAINT(PAINT_LEMON, 2), WB_OUT); } }
#endif
    if (flash) { tk_post_hellify(16); tk_post_darken(10); }
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    tk_note(0.0, 0.08, EC == 1 ? 90 : EC == 2 ? 70 : 60, TKV_NOISE, 0.2f);   /* the act-out's sound */
    tk_note(0.1, 0.3, 84, TKV_SINE, 0.25f); tk_note(0.22, 0.3, 79, TKV_SINE, 0.2f);   /* HEE_HEE bell */
    static const int H[8] = { 84, 88, 91, 88, 89, 93, 91, 0 }; for (int i = 0; i < 18; i++) if (H[i % 8]) tk_note(0.6 + i * 0.24, 0.2, H[i % 8], TKV_SINE, 0.08f);
    tk_slide(5.0, 0.13, 50, 40, TKV_NOISE, 0.2f);
    for (int i = 0; i < 18; i++) if (H[i % 8]) tk_note(5.4 + i * 0.24, 0.2, H[i % 8] - (EC == 2 ? 1 : 0), TKV_SINE, 0.08f);
    if (EC == 2) { tk_note(2.5, 0.08, 95, TKV_SQ50, 0.12f); tk_note(7.5, 0.1, 83, TKV_SQ50, 0.12f); }
}
