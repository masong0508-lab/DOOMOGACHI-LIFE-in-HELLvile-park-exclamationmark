/* sc31.c - SC31 (2-02) EXT. BANDSTAND - CONTINUOUS. The possessed skull turns
 * across the crowd; breathing stops in a ripple. The tea towel twitches.
 * Nana's reflex: "...Pie, sweet pea?" The verdicts: Six. Needs salt. Four.
 * Too pink. Two. "...Six. Beat your speech." The riff winds back up; the
 * ribcage swells... SMASH CUT. No recordings exist for these lines yet:
 * bubbles + babble; the possessed verdicts are words that just appear. */
#include "tk.h"
#include "wd.h"
#include "sets.h"
#include "mayor.h"
#include "cast.h"
#include "show.h"
#include "fx.h"
#include <math.h>
#include <string.h>

#define SHOTS(X) \
    X(a_skull,   6.0) \
    X(b_towel,   3.0) \
    X(c_pie,     6.0) \
    X(d_verdict, 6.0) \
    X(e_speech,  6.0) \
    X(f_swell,   6.0)
#include "scene_tl.h"

#define INK PAL_PAINT(PAINT_JET, 1)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }

#define T_PIE     12.5f
#define T_SILENCE 15.0f
#define T_SIX     15.6f
#define T_SALT    16.6f
#define T_SHK     18.0f
#define T_PINK    19.6f
#define T_SPEECH  21.2f
#define T_TWO     22.3f
#define T_LOWER   23.0f
#define T_SIX2    25.8f
#define T_RIFF    27.2f
#define T_SWELL   32.3f

static void night_bandstand(float ta) {
    set_sky_dawn(ta * 30, 0.25f);
    set_wall(ta * 30);
    Banner b = { 0 };
    set_bandstand(ta * 30, &b, 0);
    set_balloon(BAL_X, N_CY - 2, -0.15f, 1, 0, BAL_X + 2.5f, BAN_Y1 - 1, ta * 30);
    tk_post_darken(9);
}
static float skull_look(float ta) { return lerp(-1, 1, seg(ta, 0.5f, 10.5f)); }   /* one pixel at a time */
static void possessed(float ta, float s) {
    Bones bj; bones_default(&bj, -30, -10, s, ta * 30);
    bj.possess = 1; bj.look = skull_look(ta);
    int playing = ta < T_SILENCE || ta >= T_RIFF;
    float rate = ta >= T_RIFF ? lerp(2.0f, 12.0f, seg(ta, T_RIFF, T_SWELL)) : 7.0f;
    bj.hitL = playing ? fabsf(sinf(ta * rate)) * 0.8f : 0.1f; bj.hitR = playing ? fabsf(cosf(ta * rate)) * 0.8f : 0.1f;
    float sw = seg(ta, T_SWELL, T_SWELL + 10 / 30.0f);
    bj.squash = 1 + 0.25f * sw + (ta > T_SWELL + 10 / 30.0f ? 0.01f * ((int)(ta * 30) % 2) : 0);
    bones_draw(&bj);
}
static void sunny_bundle(float x, float y, float ta) {
    Sunny u; sunny_default(&u, x, y, 0.85f, ta * 30); u.suitcase = 0; u.no_page = 1; u.page = 0.25f; u.flip = 1;
    sunny_draw(&u);
    float tw = ta >= 7.2f && ta < 7.5f ? 1.0f : 0;     /* the tea towel twitches. Once. */
    wd_blob_ol(sunny_hand_x - 2, sunny_hand_y - 3 - tw, 5, 4 + tw * 0.6f, 0, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0), WB_SHADE);
    for (int k = -1; k <= 1; k++) wd_line(sunny_hand_x - 2 + k * 2.4f, sunny_hand_y - 6, sunny_hand_x - 2 + k * 2.4f, sunny_hand_y, 0.4f, PAL_PAINT(PAINT_ROSE, 2));
}
static void table_pair(float ta) {
    float slice = seg(ta, 11.4f, 12.2f);
    Nana nn; nana_default(&nn, 30, 8, 0.9f, ta * 30); nn.look_x = -0.8f; nn.buttons = 4;
    nn.armR = slice > 0 ? lerp(0.6f, 1.9f, slice) : 0.6f;
    if (ta >= T_LOWER + 2.4f) { nn.look_x = 0.6f; nn.armR = 1.3f; }               /* the slice, drawn in closer */
    nn.mouth = lip("a2_02_nana_01", (int)((ta - T_PIE) * 30), 50) + lip("a2_02_nana_02", (int)((ta - T_SIX2) * 30), 50);
    nana_draw(&nn);
    float droop = ta >= 17.5f ? 1 : 0;
    if (slice > 0) wd_tri(nana_hand_x[1] - 3, nana_hand_y[1] - 2, nana_hand_x[1] + 3, nana_hand_y[1] - 3 + droop, nana_hand_x[1] + 3, nana_hand_y[1] - 0.5f + droop, PAL_PAINT(PAINT_CARAMEL, 1));
    /* the Mayor: clipboard to his bow tie, wings stopped dead; then the cupcake; then the speech */
    Mayor m; mayor_default(&m, 60, 8, 0.9f, ta * 30);
    m.flutter = 0; m.fan = 0.1f; m.expr = MX_WORRY; m.look_x = -1; m.eye_pop = 0.3f;
    m.armR = 1.2f; m.armL = 1.0f;
    float cup = seg(ta, T_SHK - 0.3f, T_SHK);
    if (ta >= T_SHK - 0.3f && ta < T_SPEECH) { m.armR = 1.5f + 0.3f * sinf(ta * 30) * (ta < T_SHK + 0.7f); m.armL = 1.5f; m.expr = MX_HAPPY; }
    float sp = seg(ta, T_SPEECH, T_SPEECH + 0.3f), low = seg(ta, T_LOWER + 0.4f, T_LOWER + 2.2f);
    if (ta >= T_SPEECH) { m.armR = m.armL = lerp(1.0f, 1.9f, sp) - 1.3f * low; m.expr = low > 0.3f ? MX_WORRY : MX_HAPPY; }
    if (ta >= T_PINK && ta < T_SPEECH) m.expr = MX_WORRY;
    mayor_draw(&m);
    if (ta < T_SHK - 0.3f || ta >= T_SPEECH) {
        if (ta < T_SPEECH) wd_rect(mayor_head_x - 2, mayor_head_y + 8, 7, 9, PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CHOCOLATE, 1));   /* clipboard to the bow tie */
        else {   /* WELCOME SPEECH, three pages, title first */
            float px = mayor_hand_x[1] - 2, py = mayor_hand_y[1] - 9;
            wd_rect(px - 6, py, 12, 14, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0));
            wd_text(px - 5.5f, py + 2, "WELCOME", 0.35f, PAL_PAINT(PAINT_NAVY, 1), 0);
            wd_text(px - 5.5f, py + 5, "SPEECH", 0.35f, PAL_PAINT(PAINT_NAVY, 1), 0);
        }
    } else {    /* the cupcake, and skull sprinkles: shk-shk-shk */
        float cx = mayor_hand_x[1] - 2, cy = mayor_hand_y[1] - 3 + (ta >= T_SPEECH - 0.3f ? 20 * seg(ta, T_SPEECH - 0.3f, T_SPEECH) : 0);
        wd_quad(cx - 2.4f, cy - 1, cx + 2.4f, cy - 1, cx + 2, cy + 2, cx - 2, cy + 2, PAL_PAINT(PAINT_SKY, 2), INK);
        wd_blob_ol(cx, cy - 2, 3, 2, 0, PAL_PAINT(PAINT_BUBBLEGUM, 2), INK, 0);
        if (ta < T_SHK + 0.7f) for (int k = 0; k < 4; k++) wd_blob(cx - 2 + k * 1.3f, cy - 4 - fmodf(ta * 30 + k * 3, 6), 0.4f, 0.4f, 0, PAL_WHITE, 0);
    }
    cake_table(44, 12, ta, slice > 0);   /* in front of them */
    (void)cup;
}
static void crowd_wide(float ta, float eyes) {
    CrowdOpts o = { 0 };
    o.n = 12; o.x0 = -110; o.x1 = 110; o.y0 = 22; o.y1 = 40; o.t = ta * 30; o.seed = 41;
    o.freeze_at = 0.5f * 30; o.ripple = 300.0f / 220.0f;   /* the skull's ripple, left to right over 10 s */
    o.eyes = eyes; o.avoid_x = 4; o.avoid_r = 12;
    crowd_draw(&o);
}
static float crowd_eyes(float ta) { return ta < T_RIFF ? 0 : seg(ta, T_RIFF, T_SWELL); }

#define SHOT(name, ...) \
    void scene_##name(int f, int n) { float ta = (float)tk_scene_start(#name) + f / 30.0f; (void)n; __VA_ARGS__ } \
    void scene_##name##_audio(double t0, double dur) { (void)t0; (void)dur; }

SHOT(a_skull, {
    wd_cam(lerp(-10, 10, seg(ta, 0, 6)), -24, 1.25f);
    night_bandstand(ta);
    possessed(ta, 1.0f);
    sunny_bundle(4, 34, ta);
    table_pair(ta);
    crowd_wide(ta, 0);
    /* the skull's gaze: a faint purple wedge sweeping across the crowd */
    float gx = wd_sx(lerp(-100, 100, seg(ta, 0.5f, 10.5f)));
    tk_dither_rect((int)gx - 10, 100, 20, 60, PAL_PAINT(PAINT_GRAPE, 1), 2);
})
SHOT(b_towel, {
    wd_cam(4, 18, 3.4f);
    night_bandstand(ta);
    sunny_bundle(4, 34, ta);
    if (ta >= 7.2f && ta < 7.6f) tk_text_outline((int)wd_sx(10), (int)wd_sy(14), "!", PAL_WHITE, PAL_PAINT(PAINT_NAVY, 1), 1);
})
SHOT(c_pie, {
    wd_cam(40, -10, 2.4f);
    night_bandstand(ta);
    possessed(ta, 1.0f);
    table_pair(ta);
    if (ta >= T_PIE && ta < T_PIE + 1.8f) { bubble_side = -1; bubble("...Pie, sweet pea?", (int)((ta - T_PIE) * 22) + 1, (int)wd_sx(nana_mouth_x), (int)wd_sy(nana_mouth_y)); bubble_side = 0; }
})
SHOT(d_verdict, {
    wd_cam(lerp(18, 26, seg(ta, 15, 21)), -16, 1.9f);
    night_bandstand(ta);
    possessed(ta, 1.0f);
    table_pair(ta);
    if (ta >= T_SIX && ta < T_SALT) verdict("Six.", ta - T_SIX, 20);
    if (ta >= T_SALT && ta < T_SHK) verdict("Needs salt.", ta - T_SALT, 20);
    if (ta >= T_PINK) verdict("Four. Too pink.", ta - T_PINK, 20);
    if (ta >= T_SHK && ta < T_SHK + 0.7f) tk_text_outline((int)wd_sx(mayor_hand_x[1]) - 6, (int)wd_sy(mayor_hand_y[1] - 14), "shk-shk-shk", PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0), 1);
})
SHOT(e_speech, {
    wd_cam(lerp(44, 48, seg(ta, 21, 27)), -12, lerp(2.4f, 2.7f, seg(ta, 21, 27)));
    night_bandstand(ta);
    possessed(ta, 1.0f);
    table_pair(ta);
    if (ta >= T_TWO && ta < T_TWO + 1.6f) verdict("Two.", ta - T_TWO, 20);
    if (ta >= T_SIX2 && ta < T_SIX2 + 1.4f) { bubble_side = -1; bubble("...Six. Beat your speech.", (int)((ta - T_SIX2) * 22) + 1, (int)wd_sx(nana_mouth_x), (int)wd_sy(nana_mouth_y)); bubble_side = 0; }
})
SHOT(f_swell, {
    float u = seg(ta, 27, 33);
    wd_cam(lerp(-6, -26, u), lerp(-20, -36, u), lerp(1.4f, 2.8f, u * u));
    night_bandstand(ta);
    possessed(ta, 1.0f);
    table_pair(ta);
    crowd_wide(ta, crowd_eyes(ta));
    if (ta >= T_SWELL + 10 / 30.0f) tk_post_shake(((int)(ta * 30) & 1) ? 1 : -1, 0);
})

/* ---- sound ---------------------------------------------------------------- */
static void babble(double t, float secs, float pitch, float vol) {
    int n = (int)(secs * 7.5f); if (n < 1) n = 1;
    for (int k = 0; k < n; k++) {
        uint32_t h = tk_hash((uint32_t)k * 31u + (uint32_t)(secs * 100) + (uint32_t)pitch);
        tk_note(t + k * secs / n, 0.07, (int)pitch + (int)(h % 5) - 2, TKV_TRI, vol);
        tk_note(t + k * secs / n, 0.05, (int)pitch + 12 + (int)(h % 5) - 2, TKV_SQ12, vol * 0.25f);
    }
}
static void flat(double t, float secs) {   /* the possessed verdict: two flat voices, 8 f apart */
    for (int v = 0; v < 2; v++) { double tt = t + v * 8 / 30.0; int n = (int)(secs * 5) + 1; for (int k = 0; k < n; k++) tk_note(tt + k * secs / n, 0.1, v ? 38 : 41, TKV_TRI, 0.16f); }
}
void tk_music(void) {
    /* the possessed riff: minor, on the ribs, until the silence */
    static const int R[8] = { 76, 75, 72, 75, 76, 79, 75, 72 };
    for (int i = 0; ; i++) { double t = i * 0.22; if (t >= T_SILENCE) break; tk_note(t, 0.18, R[i % 8], TKV_SINE, 0.16f); if (i % 4 == 0) tk_note(t, 0.3, 45, TKV_TRI, 0.12f); }
    babble(T_PIE, 1.4f, 66, 0.22f);
    flat(T_SIX, 0.5f); flat(T_SALT, 0.9f);
    for (int k = 0; k < 3; k++) tk_note(T_SHK + k * 0.2, 0.06, 92, TKV_NOISE, 0.15f);
    flat(T_PINK, 1.2f); flat(T_TWO, 0.5f);
    tk_slide(T_LOWER + 0.4, 1.8, 67, 55, TKV_SINE, 0.08f);       /* lowered, all the way down */
    babble(T_SIX2, 1.3f, 64, 0.2f);
    /* the riff comes back one note at a time, winding up like a music box, faster each note */
    double t = T_RIFF, gap = 0.9;
    for (int i = 0; t < 33.0; i++) { tk_note(t, 0.15, R[i % 8], TKV_SINE, 0.2f); t += gap; gap *= 0.84; if (gap < 0.08) gap = 0.08; }
    tk_slide(T_SWELL, 0.5, 40, 52, TKV_SAW, 0.12f);                /* the ribcage swells */
}
