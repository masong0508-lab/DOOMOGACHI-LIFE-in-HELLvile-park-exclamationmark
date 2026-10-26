/* sc56.c - SC56 (4-08) EXT. LAUNCH HILL - NIGHT. Protected hold 3 of 3, act
 * end. The rocket, the PINK HEART over the Wall, "...Oh.", the faces, the
 * Mayor at nine (crayon bleed at the edges), the half-split with Hellvile's
 * match, the purple answer, the hum one note further, the hold, the iris.
 * One absolute clock; the score is MUTE from 10.0 s. */
#include "tk.h"
#include "wd.h"
#include "sets.h"
#include "mayor.h"
#include "cast.h"
#include "show.h"
#include <math.h>
#include <string.h>

#define SHOTS(X) \
    X(a_arrive, 7.0) \
    X(b_fuse,   3.9) \
    X(c_bloom,  1.1) \
    X(d_faces,  6.0) \
    X(e_mayor,  3.9) \
    X(f_split,  2.6) \
    X(g_answer, 7.0) \
    X(h_hum,   10.1) \
    X(i_hold,   6.4)
#include "scene_tl.h"

#define INK PAL_PAINT(PAINT_JET, 1)
#define SIL PAL_PAINT(PAINT_NAVY, 0)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }

#define T_LIGHT 7.6f
#define T_MUTE  10.0f
#define T_BLOOM 10.9f
#define T_OH    11.5f
#define T_ANSWER 28.0f
#define T_HUM   32.0f   /* the creator's hum runs 9.6 s: start early so it ends at 41.6 s, before the 4 s hold */
#define T_IRIS  45.6f

/* ---- the panorama: the whole park, dark, the Wall on the centre line ---- */
static void sky(float ta) {
    static const uint8_t cols[] = { PAL_PAINT(PAINT_NAVY, 0), PAL_PAINT(PAINT_NAVY, 1), PAL_PAINT(PAINT_GRAPE, 0) };
    tk_vgrad(0, 0, TW, TH, cols, 3);
    for (int i = 0; i < 40; i++) { uint32_t h = tk_hash(i * 37u + 5u); tk_px(h % TW, (h >> 9) % 90, ((h >> 4) & 3) ? PAL_PAINT(PAINT_SKY, 2) : PAL_WHITE); }
}
static void park_below(float ta) {
    /* the park as a dark bowl under the horizon; pastel lights west, embers east */
    tk_ellipse(120, 140, 170, 46, PAL_PAINT(PAINT_TEAL, 0));
    for (int i = 0; i < 30; i++) {
        uint32_t h = tk_hash(i * 91u + 7u);
        int x = (int)(h % 150), y = 102 + (int)((h >> 8) % 24);
        int east = x > 75;
        int px = east ? 122 + (x - 75) : x + 40;
        tk_px(px, y, east ? (((h >> 3) & 1) ? PAL_PAINT(PAINT_CHERRY, 0) : PAL_PAINT(PAINT_TANGERINE, 0)) : (((h >> 3) & 1) ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_ROSE, 2)));
    }
    /* the river, the tiny bandstand, the Wall straight down the middle */
    tk_line(20, 118, 220, 112, PAL_PAINT(PAINT_SKY, 0));
    tk_tri(112, 108, 118, 102, 124, 108, PAL_PAINT(PAINT_ROSE, 0));
    tk_rect(118, 92, 4, 50, PAL_PAINT(PAINT_SILVER, 0));
    tk_rect(118, 92, 1, 50, PAL_PAINT(PAINT_SILVER, 1));
}
/* the hilltop in the foreground and the old launch stand, pointing south */
static void hilltop(void) {
    tk_ellipse(120, 214, 200, 66, PAL_PAINT(PAINT_TEAL, 0));
    tk_ellipse(120, 218, 196, 62, PAL_RAMP(RAMP_GRASS, 1));
    for (int i = 0; i < 26; i++) { uint32_t h = tk_hash(i * 13u); int x = h % TW; tk_line(x, 156 + (int)((h >> 8) % 4), x + 1, 152 + (int)((h >> 8) % 4), PAL_RAMP(RAMP_GRASS, 2)); }
}
static void stand(float x, float y, float s, int rocket, float fuse) {
    uint8_t ir = PAL_PAINT(PAINT_RUST, 0), irl = PAL_PAINT(PAINT_RUST, 1);
    wd_tube(x - 6 * s, y, x, y - 14 * s, 0.8f * s, 0.6f * s, INK, ir, irl, PAL_PAINT(PAINT_TANGERINE, 1));
    wd_tube(x + 6 * s, y, x, y - 14 * s, 0.8f * s, 0.6f * s, INK, ir, irl, PAL_PAINT(PAINT_TANGERINE, 1));
    wd_tube(x, y - 10 * s, x + 3 * s, y - 22 * s, 0.9f * s, 0.9f * s, INK, ir, irl, PAL_PAINT(PAINT_TANGERINE, 1));   /* the rail */
    if (rocket) {
        wd_tube(x + 1 * s, y - 14 * s, x + 3.6f * s, y - 24 * s, 1.2f * s, 1.0f * s, INK, PAL_PAINT(PAINT_ROSE, 0), PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_ROSE, 2));
        wd_tri(x + 2.6f * s, y - 24 * s, x + 4.8f * s, y - 23.4f * s, x + 4.2f * s, y - 27 * s, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        if (fuse > 0) for (int k = 0; k < 4; k++) { uint32_t h = tk_hash((uint32_t)(fuse * 100) + k); wd_blob(x + 0.6f * s + (h % 5 - 2) * 0.3f * s, y - 13 * s + ((h >> 4) % 4) * 0.3f * s, 0.4f * s, 0.4f * s, 0, (h & 1) ? PAL_PAINT(PAINT_LEMON, 2) : PAL_WHITE, 0); }
    }
}
/* fireworks in screen space: the heart, then the small purple answer */
static void heart_fw(int cx, int cy, float ta, float t0, float scale, uint8_t c, uint8_t c2, int small) {
    float age = ta - t0;
    if (age < 0) return;
    float grow = seg(age, 0, 0.5f), sink = small ? age * 2.0f : seg(ta, 24.5f, 28.0f) * 34;
    int n = small ? 24 : 64;
    for (int i = 0; i < n; i++) {
        float u = i * 6.2832f / n;
        float hx = 16 * powf(sinf(u), 3), hy = -(13 * cosf(u) - 5 * cosf(2 * u) - 2 * cosf(3 * u) - cosf(4 * u));
        if (small) { hx = cosf(u) * 9; hy = sinf(u) * 9; }
        float r = scale * grow;
        int x = cx + (int)(hx * r), y = cy + (int)(hy * r + sink + (small ? 0 : age * age * 0.4f));
        float fade = small ? 1 - seg(age, 3, 13) : 1 - seg(ta, 26, 28.6f);
        if (fade <= 0.02f) continue;
        if (tk_hash(i * 7u + (uint32_t)(ta * 12)) % 100 < fade * 100) { tk_rect(x, y, 2, 2, c); tk_px(x + 2, y + 1, c2); tk_px(x - 1, y + 2, c2); }
    }
}
/* the cast as silhouettes against the panorama (they face the Wall, away from us) */
static void sil_mayor(float x, float y, float s) {
    wd_blob(x, y - 12 * s, 9 * s, 10 * s, 0, SIL, 0);
    for (int k = 0; k < 5; k++) wd_blob(x - 8 * s + k * 4 * s, y - 20 * s, 3 * s, 5 * s, 0, SIL, 0);   /* tail fan */
    wd_blob(x, y - 26 * s, 6 * s, 6 * s, 0, SIL, 0);
    wd_rect(x - 4 * s, y - 42 * s, 8 * s, 12 * s, SIL, 0); wd_rect(x - 7 * s, y - 31 * s, 14 * s, 2 * s, SIL, 0);   /* the too-tall hat */
}
static void sil_bunny(float x, float y, float s) {
    wd_blob(x, y - 6 * s, 5 * s, 6 * s, 0, SIL, 0); wd_blob(x, y - 15 * s, 5 * s, 5 * s, 0, SIL, 0);
    wd_tube(x - 2 * s, y - 18 * s, x - 3 * s, y - 29 * s, 1.5f * s, 1.2f * s, SIL, SIL, SIL, SIL);
    wd_tube(x + 2 * s, y - 18 * s, x + 3 * s, y - 29 * s, 1.5f * s, 1.2f * s, SIL, SIL, SIL, SIL);
}
static void sil_round(float x, float y, float s, int ears) {     /* Fuzz (ears=1), Nana (ears=2) */
    wd_blob(x, y - 8 * s, 7 * s, 8 * s, 0, SIL, 0); wd_blob(x, y - 20 * s, 6 * s, 6 * s, 0, SIL, 0);
    if (ears == 1) { wd_blob(x - 5 * s, y - 25 * s, 2.2f * s, 2.2f * s, 0, SIL, 0); wd_blob(x + 5 * s, y - 25 * s, 2.2f * s, 2.2f * s, 0, SIL, 0); }
    if (ears == 2) for (int k = 0; k < 5; k++) wd_blob(x - 4 * s + k * 2 * s, y - 25 * s, 2 * s, 2 * s, 0, SIL, 0);
}
static void sil_bones(float x, float y, float s) {
    wd_tube(x - 2 * s, y, x - 1 * s, y - 14 * s, 0.7f * s, 0.7f * s, SIL, SIL, SIL, SIL); wd_tube(x + 2 * s, y, x + 1 * s, y - 14 * s, 0.7f * s, 0.7f * s, SIL, SIL, SIL, SIL);
    wd_blob(x, y - 22 * s, 5 * s, 8 * s, 0, SIL, 0); wd_blob(x, y - 34 * s, 5 * s, 5 * s, 0, SIL, 0); wd_rect(x - 4 * s, y - 41 * s, 8 * s, 3 * s, SIL, 0);
}
static void sil_mascotty(float x, float y, float s) {
    wd_rect(x - 3 * s, y - 12 * s, 6 * s, 12 * s, SIL, 0); wd_blob(x, y - 20 * s, 9 * s, 9 * s, 0, SIL, 0);
    wd_blob(x - 4 * s, y - 35 * s, 2.4f * s, 8 * s, 0, SIL, 0); wd_blob(x + 4 * s, y - 35 * s, 2.4f * s, 8 * s, 0, SIL, 0);
}
static void silhouettes(float ta, float s) {
    float mx = lerp(-40, -8, seg(ta, 0.5f, 4.5f));                 /* he carries the rocket to the stand */
    sil_mascotty(-64, -2, s * 0.9f);                                   /* at the very back; nobody saw her arrive */
    sil_bunny(-50, 0, s * 0.55f);                                      /* Pip, four steps back, Mr. Flops tight */
    sil_bunny(-30, 0, s * 0.8f); wd_blob(-30, -26 * s, 3 * s, 2.6f * s, 0, SIL, 0);   /* Sunny, the pet on their head */
    sil_round(-18, 0, s * 0.85f, 2);                                   /* Nana */
    sil_round(14, 0, s, 1);                                            /* Fuzz */
    sil_bones(26, 0, s);                                               /* Bonejangles */
    sil_mayor(mx, 0, s * 0.85f);
}

/* ---- shots ---------------------------------------------------------------- */
static void wide(float ta, float zoom) {
    sky(ta);
    heart_fw(120, 40, ta, T_BLOOM, 2.2f, PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_ROSE, 2), 0);
    heart_fw(124, 50, ta, T_ANSWER, 1.3f, PAL_PAINT(PAINT_GRAPE, 2), PAL_PAINT(PAINT_LAVENDER, 2), 1);
    park_below(ta);
    hilltop();
    wd_cam(0, -54, zoom);
    stand(-2, 0, 1.0f, ta < T_MUTE, ta >= T_LIGHT ? ta : 0);
    silhouettes(ta, 1.0f);
    /* the rocket's climb: thin and quiet */
    if (ta >= T_MUTE && ta < T_BLOOM) { float u = (ta - T_MUTE) / (T_BLOOM - T_MUTE); tk_line(120, 120 - (int)(u * 80), 120, 126 - (int)(u * 80), PAL_PAINT(PAINT_LEMON, 2)); }
}
#define SHOT(name, ...) \
    void scene_##name(int f, int n) { float ta = (float)tk_scene_start(#name) + f / 30.0f; (void)n; __VA_ARGS__ } \
    void scene_##name##_audio(double t0, double dur) { (void)t0; (void)dur; }

SHOT(a_arrive, { wide(ta, lerp(1.25f, 1.4f, seg(ta, 0, 7))); })
SHOT(b_fuse, {
    sky(ta); park_below(ta); hilltop();
    wd_cam(-4, -16, 3.0f);
    stand(-2, 0, 1.0f, 1, ta >= T_LIGHT ? ta : 0);
    Mayor m; mayor_default(&m, -18, 4, 0.9f, ta * 30);
    m.armR = lerp(0.6f, 1.4f, seg(ta, 7.0f, T_LIGHT)); m.look_x = 1; m.look_y = 0.5f; m.expr = MX_NEUTRAL; m.flutter = 0.1f;
    mayor_draw(&m);
    wd_rect(mayor_hand_x[1] - 0.6f, mayor_hand_y[1] - 4, 1.2f, 4, PAL_PAINT(PAINT_BONE, 2), INK);       /* the candle */
    wd_blob(mayor_hand_x[1], mayor_hand_y[1] - 5.4f, 0.7f, 1.3f, 0, PAL_PAINT(PAINT_LEMON, 2), 0);
    if (ta >= T_LIGHT && ta < T_MUTE) tk_text_outline((int)wd_sx(4), (int)wd_sy(-8), "fsss", PAL_WHITE, PAL_PAINT(PAINT_TANGERINE, 0), 1);
    if (ta >= T_MUTE) { float u = (ta - T_MUTE) / 0.9f; wd_line(1.5f, -20 - u * 60, 1.5f, -14 - u * 60, 0.6f, PAL_PAINT(PAINT_LEMON, 2)); }
})
SHOT(c_bloom, {
    sky(ta);
    heart_fw(120, 70, ta, T_BLOOM, 3.4f, PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_ROSE, 2), 0);
    park_below(ta);
    if (ta >= T_OH) bubble("...Oh.", (int)((ta - T_OH) * 20) + 1, 30, 150);
})
SHOT(d_faces, {
    /* one wide from below: every face goes pink in a ripple */
    sky(ta);
    heart_fw(120, 18, ta, T_BLOOM, 1.6f, PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_ROSE, 2), 0);
    wd_cam(-16, -24, 2.0f);
    float t30 = ta * 30;
    Nana nn; nana_default(&nn, -40, 4, 0.85f, t30); nn.look_x = 0; nn.buttons = 3; nana_draw(&nn);
    Critter fu = cast_fuzz(-22, 4, t30); fu.look_y = -1; critter_draw(&fu);
    Bones bj; bones_default(&bj, -6, 4, 0.95f, t30); bones_draw(&bj);
    Sunny u; sunny_default(&u, 10, 4, 0.8f, t30); u.suitcase = 0; u.look_y = -1; sunny_draw(&u);
    pet_draw(10, 4 - 23 * 0.8f - 7, 0.8f, t30, 0, 0);
    /* Mascotty's big bunny head at the back, lit pink too */
    wd_blob_ol(30, -40, 9, 9, 0, PAL_WHITE, INK, WB_SHADE);
    wd_blob_ol(26, -56, 2.6f, 8, -0.1f, PAL_WHITE, INK, 0); wd_blob_ol(34, -56, 2.6f, 8, 0.1f, PAL_WHITE, INK, 0);
    wd_blob(26.5f, -42, 1.4f, 1.4f, 0, INK, 0); wd_blob(33.5f, -42, 1.4f, 1.4f, 0, INK, 0);
    /* the ripple of pink light, left to right, across every face */
    float front = 240 * seg(ta, 12.2f, 13.4f);
    if (front > 0) tk_dither_rect(0, 0, (int)front, TH, PAL_PAINT(PAINT_BUBBLEGUM, 2), 3);
})
SHOT(e_mayor, {
    sky(ta);
    heart_fw(150, 26, ta, T_BLOOM, 1.4f, PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_ROSE, 2), 0);
    wd_cam(-8, -26, 3.6f);
    Mayor m; mayor_default(&m, -14, 4, 1.0f, ta * 30);
    m.look_x = 0.6f; m.look_y = -1; m.expr = MX_NEUTRAL; m.flutter = 0; m.eye_pop = 0.15f;
    m.blink = ta >= 20.4f && ta < 20.55f;
    mayor_draw(&m);
    tk_dither_rect(0, 0, TW, TH, PAL_PAINT(PAINT_BUBBLEGUM, 2), 1);
    /* the edges bleed into crayon: storybook paper and a boiling line; he is nine */
    float bleed = seg(ta, 19.0f, 19.0f + 8 / 30.0f) * (1 - seg(ta, 21.3f, 21.9f));
    if (bleed > 0) {
        static uint8_t keep[TH][TW];
        memcpy(keep, tk_fb, sizeof keep);
        wd_post_crayon((int)(ta * 30), PAL_UI_CREAM);
        int m0 = (int)(26 * bleed);
        for (int y = 0; y < TH; y++) for (int x = 0; x < TW; x++) {
            int d = x < TW - 1 - x ? x : TW - 1 - x; int dy = y < TH - 1 - y ? y : TH - 1 - y; if (dy < d) d = dy;
            if (d >= m0) tk_fb[y][x] = keep[y][x];
            else if (d > m0 - 6 && (tk_hash(x * 7u + y * 13u) & 1)) tk_fb[y][x] = keep[y][x];
        }
    }
})
SHOT(f_split, {
    /* brick wipe down the Wall line; the left keeps the hill, the right is HELLVILE, tight */
    float wipe_in = seg(ta, 21.9f, 22.2f), wipe_out = seg(ta, 24.2f, 24.5f);
    wide(ta, 1.4f);
    float half = 120 * (1 - wipe_out);
    if (wipe_in > 0) {
        int courses = (int)(wipe_in * 5 + 0.5f);
        static uint8_t right[TH][TW];
        /* draw the Hellvile half into the right side */
        memcpy(right, tk_fb, sizeof right);
        tk_rect(120, 0, 120, TH, PAL_PAINT(PAINT_CHERRY, 0));
        tk_dither_rect(120, 0, 120, TH, PAL_PAINT(PAINT_RUST, 0), 6);
        tk_rect(140, 116, 90, 6, PAL_PAINT(PAINT_CHOCOLATE, 0));             /* the table */
        tk_ellipse(165, 112, 12, 5, PAL_PAINT(PAINT_TANGERINE, 1)); tk_ellipse(205, 112, 12, 5, PAL_PAINT(PAINT_TANGERINE, 1));   /* two bowls */
        tk_line(176, 70, 196, 54, PAL_PAINT(PAINT_CHARCOAL, 1)); tk_ellipse(168, 76, 14, 9, PAL_PAINT(PAINT_CHARCOAL, 1));     /* a sooty hand */
        if (ta >= 22.6f) { tk_line(196, 54, 202, 48, PAL_PAINT(PAINT_BONE, 2)); tk_circle(203, 46, 2 + (int)(ta * 20) % 2, PAL_PAINT(PAINT_LEMON, 2)); }
        if (ta >= 23.2f) tk_text_outline(200, 30, "fsss", PAL_WHITE, PAL_PAINT(PAINT_CHERRY, 0), 1);
        if (ta >= 22.6f && ta < 23.2f) tk_text_outline(150, 30, "scratch", PAL_WHITE, PAL_PAINT(PAINT_CHERRY, 0), 1);
        for (int y = 0; y < TH; y++) for (int x = 0; x < (int)half && x < TW; x++) if (x < 120) right[y][x] = tk_fb[y][x];
        for (int y = 0; y < TH; y++) for (int x = 0; x < TW; x++) {
            int show_right = x >= 120 && x < 120 + (int)half && y < courses * 32;
            if (!show_right) tk_fb[y][x] = right[y][x];
        }
        /* the brick seam on the Wall line */
        for (int c = 0; c < courses; c++) { tk_rect(116, c * 32, 8, 31, PAL_PAINT(PAINT_SILVER, 1)); tk_frame(116, c * 32, 8, 31, PAL_PAINT(PAINT_CHARCOAL, 2)); }
    }
})
SHOT(g_answer, {
    wide(ta, 1.4f);
    if (ta >= T_ANSWER + 0.3f) for (int i = 0; i < 6; i++) {          /* every head turns to it, in a ripple */
        float u = seg(ta, T_ANSWER + 0.3f + i * 0.1f, T_ANSWER + 0.6f + i * 0.1f);
        (void)u;
    }
})
SHOT(h_hum, {
    sky(ta);
    heart_fw(80, 30, ta, T_BLOOM, 1.6f, PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_ROSE, 2), 0);
    heart_fw(160, 34, ta, T_ANSWER, 1.2f, PAL_PAINT(PAINT_GRAPE, 2), PAL_PAINT(PAINT_LAVENDER, 2), 1);
    park_below(ta);
    wd_cam(-10, -24, lerp(2.4f, 2.8f, seg(ta, 31.5f, 41.6f)));
    Mayor m; mayor_default(&m, -10, 4, 1.0f, ta * 30);
    m.look_y = -1; m.look_x = 0.3f; m.expr = MX_NEUTRAL; m.flutter = 0;
    m.mouth = lip("a4_08_mayor_01", (int)((ta - T_HUM) * 30), 288) * 0.5f;
    m.blink = fmodf(ta * 30, 120) > 116;
    mayor_draw(&m);
    if (ta >= T_HUM && ta < T_HUM + 9.6f) { tk_star((int)wd_sx(mayor_head_x + 9), (int)wd_sy(mayor_head_y - 8) - (int)((ta - T_HUM) * 4) % 10, 1, PAL_PAINT(PAINT_ROSE, 2)); }
})
SHOT(i_hold, {
    /* HOLD 4 s on the last pink speck and the last purple speck over the Wall, then the heart iris */
    sky(ta); park_below(ta);
    if (ta < 45.4f) { tk_px(117, 64, PAL_PAINT(PAINT_BUBBLEGUM, 2)); tk_px(123, 66, PAL_PAINT(PAINT_GRAPE, 2)); }
    float r = ta < T_IRIS ? 300 : lerp(160, 0, seg(ta, T_IRIS, T_IRIS + 5 / 30.0f));
    if (ta >= T_IRIS) tk_post_iris(120, 65, r, 1, PAL_PAINT(PAINT_JET, 2), PAL_PAINT(PAINT_JET, 2));
    if (ta >= 45.8f) tk_clear(PAL_PAINT(PAINT_JET, 2));
})

/* ---- sound: the slow MAYOR motif to the fuse, then MUTE; the sequencer keeps time */
void tk_music(void) {
    static const int mot[8] = { 60, 64, 67, 65, 64, 62, 60, 0 };
    for (int i = 0; i < 14; i++) { int m = mot[i % 8]; if (m) tk_note(i * 0.5, 0.48, m, TKV_TRI, 0.2f); }
    for (double t = 7.0; t < T_LIGHT; t += 0.2) tk_note(t, 0.05, 60, TKV_NOISE, 0.03f);
    tk_slide(T_LIGHT, T_MUTE - T_LIGHT, 80, 92, TKV_NOISE, 0.08f);      /* fsss */
    tk_slide(T_MUTE, 0.9, 70, 94, TKV_SINE, 0.05f);                     /* the climb, thin and quiet */
    tk_note(T_BLOOM, 0.25, 36, TKV_SINE, 0.4f);                          /* thump, soft, no bang */
    tk_note(T_OH, 0.4, 76, TKV_TRI, 0.12f);                              /* "...Oh." (Pip; bubble) */
    tk_note(22.6, 0.06, 80, TKV_NOISE, 0.15f);                           /* scratch */
    tk_slide(23.2, 1.0, 82, 90, TKV_NOISE, 0.06f);                       /* fsss (right half) */
    tk_note(24.45, 0.05, 96, TKV_SINE, 0.25f);                           /* trowel tink */
    tk_note(T_ANSWER, 0.2, 38, TKV_SINE, 0.3f);                          /* the answer: thump */
    line_at("a4_08_mayor_01", T_HUM);                                    /* the hum, one note further */
}
