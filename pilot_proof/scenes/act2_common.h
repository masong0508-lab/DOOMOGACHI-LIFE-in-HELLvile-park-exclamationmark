/* act2_common.h - the dark bandstand of Act Two: only the possessed ribs glow */
#include "fair_common.h"
#include "mayor.h"
static void dark_stage(float ta, int candles, float c0, float flame_out) {
    set_sky_dawn(ta * 30, 0.25f); set_wall(ta * 30);
    Banner b = { 0 }; set_bandstand(ta * 30, &b, 0); tk_post_darken(4);
    for (int k = 0; k < 6; k++) wd_blob(-40, -32 + k * 2.6f, 11 - k, 2.4f, 0, PAL_PAINT(PAINT_GRAPE, 1), 0);   /* the glow */
    for (int i = 0; i < candles; i++) {
        float land = seg(ta, c0 + i * 0.5f, c0 + i * 0.5f + 0.1f); if (land <= 0) continue;
        float x = -12 + i * 6;
        wd_rect(x - 1, -16, 2, 6, PAL_PAINT(PAINT_BONE, 2), INK);
        int out = i == 0 && flame_out > 0 && ta >= flame_out;
        if (!out) wd_blob(x, -18 + 0.3f * sinf(ta * 20 + i), 1, 2, 0, PAL_PAINT(PAINT_LEMON, 2), WB_OUT);
        else if (ta < flame_out + 1.5f) wd_line(x, -18, x + 1, -26 - (ta - flame_out) * 4, 0.3f, PAL_GRAY_L);
    }
}
static void possessed_bj(float ta, int playing, float look) {
    Bones bj; bones_default(&bj, -40, -10, 1.0f, ta * 30); bj.possess = 1; bj.look = look;
    bj.hitL = playing ? fabsf(sinf(ta * 7)) * 0.8f : 0.1f; bj.hitR = playing ? fabsf(cosf(ta * 7)) * 0.8f : 0.1f;
    bj.squash = 1 + 0.02f * ((int)(ta * 30) & 1);   /* twitching on every frame */
    bones_draw(&bj);
}
static void riff(double t0, double t1, float vol, double s0, double s1) {
    static const int R[8] = { 76, 75, 72, 75, 76, 79, 75, 72 };
    for (int i = 0; ; i++) { double t = t0 + i * 0.22; if (t >= t1) break; if (t >= s0 && t < s1) continue; tk_note(t, 0.18, R[i % 8], TKV_SINE, vol); if (i % 4 == 0) tk_note(t, 0.3, 40, TKV_TRI, vol * 0.8f); }
}
static void flat2(double t, float secs) { for (int v = 0; v < 2; v++) { double tt = t + v * 8 / 30.0; int n = (int)(secs * 5) + 1; for (int k = 0; k < n; k++) tk_note(tt + k * secs / n, 0.1, v ? 38 : 41, TKV_TRI, 0.16f); } }
static Critter fuzz_at(float ta, float x, float y) { Critter c = cast_fuzz(x, y, ta * 30); c.flip = -1; return c; }
static Critter bloom_at(float ta, float x, float y) { Critter c = cast_bloom(x, y, ta * 30); c.flip = -1; return c; }
