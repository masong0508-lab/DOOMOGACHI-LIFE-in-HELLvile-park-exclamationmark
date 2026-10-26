/* act2_common.h - the dark bandstand of Act Two: only the possessed ribs glow */
#include "fair_common.h"
#include "mayor.h"
static float glow_pulse = 0;
static void dark_stage(float ta, int candles, float c0, float flame_out) {
    set_sky_dawn(ta * 30, 0.25f); set_wall(ta * 30);
    Banner b = { 0 }; set_bandstand(ta * 30, &b, 0); tk_post_darken(4);
    for (int k = 0; k < 6; k++) wd_blob(-40, -32 + k * 2.6f, (11 - k) * (1 + 0.35f * glow_pulse), 2.4f * (1 + 0.35f * glow_pulse), 0, glow_pulse > 0.5f ? PAL_PAINT(PAINT_GRAPE, 2) : PAL_PAINT(PAINT_GRAPE, 1), 0);   /* the glow */
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

/* editing: a cut list; each cut pushes from z0 to z1 and drifts dx over its length */
typedef struct { float t, x, y, z0, z1, dx; int kind; } Cut;
enum { K_WORLD, K_CARD, K_WATCH, K_CANDLES, K_TABLE, K_BOARDS };
static int cut_at(const Cut *c, int n, float ta, float total) {
    int i = 0; while (i + 1 < n && ta >= c[i + 1].t) i++;
    float t1 = i + 1 < n ? c[i + 1].t : total, u = seg(ta, c[i].t, t1);
    wd_cam(c[i].x + c[i].dx * u, c[i].y, lerp(c[i].z0, c[i].z1, u));
    return c[i].kind;
}
#define ONE_SHOT(secs) X(main, secs)
static void fob_watch(float ta, float t0) {   /* insert: Bloom's pastel fob watch, ticking */
    tk_clear(PAL_PAINT(PAINT_GRAPE, 0));
    tk_circle(120, 82, 54, PAL_PAINT(PAINT_BUBBLEGUM, 1)); tk_circle(120, 82, 48, PAL_UI_CREAM); tk_ring(120, 82, 48, PAL_PAINT(PAINT_ROSE, 0));
    for (int k = 0; k < 12; k++) { float a = k * 0.5236f; tk_rect(120 + (int)(cosf(a) * 40) - 1, 82 + (int)(sinf(a) * 40) - 1, 3, 3, PAL_PAINT(PAINT_NAVY, 1)); }
    int ticks = (int)((ta - t0) * 2); float a = -1.5708f + ticks * 0.1047f;
    tk_line(120, 82, 120 + (int)(cosf(a) * 42), 82 + (int)(sinf(a) * 42), PAL_UI_RED); tk_circle(120, 82, 3, PAL_PAINT(PAINT_NAVY, 1));
    tk_rect(112, 20, 16, 10, PAL_PAINT(PAINT_BUBBLEGUM, 1)); tk_line(120, 0, 120, 20, PAL_UI_GOLD);
    /* her two fingers on the wrist bone, bottom left */
    wd_cam(TW / 2, TH / 2, 1); wd_tube(0, 150, 40, 128, 8, 6, INK, PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_MINT, 2), PAL_WHITE);
    wd_tube(36, 128, 70, 118, 4, 3, INK, PAL_PAINT(PAINT_BONE, 0), PAL_PAINT(PAINT_BONE, 2), PAL_WHITE);
    if (((int)((ta - t0) * 2)) != ((int)((ta - t0 - 0.1f) * 2))) tk_text(170, 20, "tik", PAL_WHITE, 1);
}
static void table_close(float ta, float t0) {   /* insert: under the cake table, a plump little hand pulls the cloth down an inch */
    set_sky_dawn(ta * 30, 0.25f); tk_post_darken(4);
    wd_cam(0, -6, 4.2f);
    wd_rect(-60, 4, 120, 20, PAL_PAINT(PAINT_CHOCOLATE, 0), 0);
    cake_table(0, 4, ta, 0);
    float reach = seg(ta, t0 + 0.6f, t0 + 1.2f), pull = seg(ta, t0 + 1.4f, t0 + 2.0f);
    wd_rect(-27, 4, 54, 1 + pull * 2.5f, PAL_WHITE, PAL_PAINT(PAINT_ROSE, 1));
    if (reach > 0 && pull < 1) { float hy = 6 - reach * 2 + pull * 3; wd_blob_ol(2, hy, 2.2f, 1.8f, 0, PAL_PAINT(PAINT_CARAMEL, 1), INK, WB_SHADE); for (int k = -1; k <= 1; k++) wd_blob(2 + k * 1.3f, hy - 1.4f, 0.8f, 1.1f, 0, PAL_PAINT(PAINT_CARAMEL, 2), WB_OUT); }
}
