/* sc55.c - SC55 (4-07) THE JOKE GRAVEYARD, LAUNCH HILL PATH. Side-on, a
 * constant walking pace, low camera. Rest in pieces, Mr. Huggins the 1st.
 * FLUFFY THE CLOUD. DRIFTED. The Mayor's wings stop for exactly 8 f; only
 * Sunny sees. The wings start again. */
#include "act4_common.h"
#define SHOTS(X) X(main, 12.0)
#include "scene_tl.h"
static const float STOP = 6.0f;
static void stone(float x, const char *a, const char *b, float w) {
    wd_rect(x - w / 2, -14, w, 14, PAL_PAINT(PAINT_SILVER, 1), INK); wd_blob(x, -14, w / 2, 3, 0, PAL_PAINT(PAINT_SILVER, 1), 0);
    /* engraved in world units, sized to fit inside the stone */
    int la = (int)strlen(a), lb = b ? (int)strlen(b) : 0, lm = la > lb ? la : lb;
    float p = (w - 6) / (lm * 7.2f); if (p > 0.4f) p = 0.4f;
    wd_text(x - la * 3.6f * p, -12, a, p, PAL_PAINT(PAINT_CHARCOAL, 1), 0); if (b) wd_text(x - lb * 3.6f * p, -12 + 9 * p, b, p, PAL_PAINT(PAINT_CHARCOAL, 1), 0);
}
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    float walk = ta * 14;
    wd_cam(walk + 10, -4, 2.2f);
    static const uint8_t c[] = { PAL_PAINT(PAINT_NAVY, 1), PAL_PAINT(PAINT_GRAPE, 0) }; tk_vgrad(0, 0, TW, TH, c, 2);
    wd_rect(-200, 0, 600, 60, PAL_RAMP(RAMP_GRASS, 1), 0);
    stone(26, "Rest in pieces,", "Mr. Huggins the 1st", 56);
    stone(70, "HA", NULL, 14); stone(100, "lol", NULL, 14);
    stone(132, "FLUFFY THE CLOUD.", "DRIFTED.", 52); wd_blob(132, -19, 3, 2, 0, PAL_WHITE, WB_OUT);
    stone(170, "RIP", NULL, 14);
    float base = walk;
    Mayor m; mayor_default(&m, base + 8, 18, 0.75f, ta * 30); m.step = fabsf(sinf(ta * 6)) * 1.5f;
    int stop = ta >= STOP && ta < STOP + 8 / 30.0f; m.flutter = stop ? 0 : 0.5f; m.expr = stop ? MX_WORRY : MX_HAPPY; m.armL = 0.8f;
    mayor_draw(&m); wd_rect(mayor_hand_x[0] - 0.6f, mayor_hand_y[0] - 3, 1.2f, 3, PAL_PAINT(PAINT_BONE, 2), INK); wd_blob(mayor_hand_x[0], mayor_hand_y[0] - 4.2f, 0.6f, 1, 0, PAL_PAINT(PAINT_LEMON, 2), WB_OUT);
    Sunny u; sunny_default(&u, base - 6, 18, 0.7f, ta * 30); u.suitcase = 0; u.look_x = ta >= STOP && ta < STOP + 2 ? 1 : 0; sunny_draw(&u); pet_draw(base - 6, 18 - 23 * 0.7f - 4, 0.6f, ta * 30, 0, 0);
    Nana nn; nana_default(&nn, base - 20, 18, 0.7f, ta * 30); nana_draw(&nn);
    Critter fz = fuzz_at(ta, base - 32, 18); fz.s = 0.75f; fz.flip = 1; critter_draw(&fz);
    Critter p = cast_pip(base - 62, 18, ta * 30); p.flip = 1; critter_draw(&p);
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) { static const int MM[8] = { 60, 64, 67, 65, 64, 62, 60, 0 }; for (int i = 0; i < 24; i++) { double t = i * 0.5; if (t >= STOP && t < STOP + 13 / 30.0) continue; if (MM[i % 8]) tk_note(t, 0.48, MM[i % 8], TKV_TRI, 0.1f); } }
