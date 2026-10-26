/* sc32.c - SC32 (2-03) EXT. FUN FAIR - NIGHT. FWOOMP. Panic. The Mayor stays
 * calm, under the table. Puff Demons pour from the ribs. The teacups run
 * backwards; a demon goes in, confetti comes out (TRUTH 4: what really
 * happened; ONE CHANGE: the cup stays red). The not-scary house. The gust:
 * the boards fly, the MINGLE crate, the LOST PROPERTY lantern lifts.
 * FIREWORKS! / NOT NOW, PIP. / After you! ... and Sunny, the still point.
 * Standards: the truth insert has no child in it; Pip's shot has no gore. */
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
    X(a_fwoomp,  5.0) \
    X(b_demons,  5.0) \
    X(c_cups,    6.0) \
    X(d_house,   3.0) \
    X(e_gust,    4.0) \
    X(f_pip,     2.0) \
    X(g_notnow,  5.0)
#include "scene_tl.h"

#define INK PAL_PAINT(PAINT_JET, 1)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }

#define T_FUDGE  1.0f
#define T_CALM   2.0f
#define T_DIVE   3.9f
#define T_BEING  4.75f
#define T_SINGLE 8.0f
#define T_LEAVE  9.0f
#define T_NOTCUPS 10.4f
#define T_INCUP  11.6f
#define T_CONF   12.4f
#define T_TRUTH  13.2f
#define T_PLIP   13.4f
#define T_WEATHER 17.4f
#define T_GUST   19.0f
#define T_FIRE   23.5f
#define T_NOTNOW 25.6f
#define T_AFTER  27.0f
#define T_YOU    27.8f

static void night(float ta) {
    static const uint8_t cols[] = { PAL_PAINT(PAINT_NAVY, 1), PAL_PAINT(PAINT_GRAPE, 1), PAL_PAINT(PAINT_GRAPE, 2) };
    tk_vgrad(0, 0, TW, TH, cols, 3);
    for (int i = 0; i < 20; i++) { uint32_t h = tk_hash(i * 19u + 2u); tk_px(h % TW, (h >> 9) % 60, PAL_PAINT(PAINT_SKY, 2)); }
}
static void fairground(float ta) {
    wd_rect(-300, 0, 600, 120, PAL_RAMP(RAMP_GRASS, 3), 0);
    for (int i = 0; i < 12; i++) {          /* lantern strings across the fair */
        float x = -200 + i * 36;
        wd_line(x, -60, x + 36, -60, 0.4f, PAL_PAINT(PAINT_CHOCOLATE, 1));
        wd_blob_ol(x + 18, -57 + 1.5f * sinf(ta * 2 + i), 2.2f, 2.8f, 0, (i % 3 == 0) ? PAL_PAINT(PAINT_LEMON, 2) : (i % 3 == 1) ? PAL_PAINT(PAINT_MINT, 2) : PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_CARAMEL, 0), WB_SHADE);
    }
    for (int k = 0; k < 4; k++) {           /* hay bales */
        float x = -150 + k * 90;
        wd_rect(x - 10, -8, 20, 8, PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_CARAMEL, 0));
        for (int q = 0; q < 4; q++) wd_line(x - 8 + q * 5, -7, x - 6 + q * 5, -1, 0.4f, PAL_PAINT(PAINT_HONEY, 2));
    }
}
static void bandstand_bg(float ta, int boards_off) {
    set_sky_dawn(ta * 30, 0.25f);
    set_wall(ta * 30);
    Banner b = { 0 };
    set_bandstand(ta * 30, &b, 0);
    tk_post_darken(9);
    if (boards_off) {   /* the boards torn off: the MINGLE crate underneath */
        wd_rect(4, -73, 66, 63, PAL_PAINT(PAINT_NAVY, 0), 0);
        wd_rect(20, -30, 34, 20, PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CHOCOLATE, 1));
        wd_text(23, -27, "MINGLE", 0.8f, PAL_PAINT(PAINT_CHOCOLATE, 1), 0);
        wd_text(22, -21, "DO NOT OPEN", 0.45f, PAL_PAINT(PAINT_CHOCOLATE, 1), 0);
        wd_tri(36, -17, 38, -17, 37, -13, PAL_PAINT(PAINT_CHOCOLATE, 1));   /* stencilled rocket */
        wd_rect(36.4f, -15, 1.2f, 3, PAL_PAINT(PAINT_CHOCOLATE, 1), 0);
    }
}
static void possessed(float ta) {
    Bones bj; bones_default(&bj, -30, -10, 1.0f, ta * 30);
    bj.possess = 1; bj.look = sinf(ta * 0.4f);
    bj.hitL = fabsf(sinf(ta * 12)); bj.hitR = fabsf(cosf(ta * 12));
    bj.squash = 1.25f - 0.25f * seg(ta, 0, 0.3f);
    bones_draw(&bj);
}
static void demons(float ta, float t0, int n, float x0, float y0) {
    for (int i = 0; i < n; i++) {
        float age = ta - t0 - i * 0.12f;
        if (age < 0) continue;
        uint32_t h = tk_hash(i * 53u + 3u);
        float vx = ((float)(h % 100) / 100 - 0.3f) * 30, x = x0 + vx * age, y = y0 + fminf(age * 25, 40 + (h >> 8) % 20);
        puff_demon(x, y, 0.8f, ta * 30 + i * 5, vx > 0 ? 1 : -1);
    }
}
static void mayor_table(float ta) {
    cake_table(40, 10, ta, 1);
    if (ta < T_DIVE) {
        Mayor m; mayor_default(&m, 48, 6, 0.9f, ta * 30);
        m.armL = m.armR = 2.8f; m.expr = MX_HAPPY; m.fan = 0.8f;
        m.mouth = lip("a2_03_mayor_01", (int)((ta - T_CALM) * 30), 70);
        float d = seg(ta, T_DIVE - 0.2f, T_DIVE);
        m.y += d * 10; m.squash = 1 - d * 0.4f;
        mayor_draw(&m);
    } else {
        /* under the tablecloth: it trembles; the tail fan pokes out the back, quivering */
        for (int k = 0; k < 5; k++) wd_blob(54 + k * 2, -1 - k * 1.5f + 0.6f * sinf(ta * 40 + k), 2, 4, 0.3f * k, PAL_PAINT(PAINT_CARAMEL, 1), WB_OUT);
        float tr = 0.4f * sinf(ta * 50);
        wd_quad(15 + tr, 0, 65 + tr, 0, 67, 10, 13, 10, PAL_WHITE, PAL_PAINT(PAINT_ROSE, 1));
    }
}
static void say_world(const char *txt, float ta, float t0, float secs, float wx, float wy, int side) {
    if (ta < t0 || ta >= t0 + secs + 0.5f) return;
    bubble_side = side; bubble(txt, (int)((ta - t0) * 24) + 1, (int)wd_sx(wx), (int)wd_sy(wy)); bubble_side = 0;
}
static void panic(float ta, float x0, float x1, float turn) {
    CrowdOpts o = { 0 };
    o.n = 14; o.x0 = x0; o.x1 = x1; o.y0 = 14; o.y1 = 40; o.t = ta * 30; o.seed = 77;
    o.freeze_at = -1; o.panic = turn > 0.5f ? 0 : 1; o.turn = turn; o.avoid_x = 0; o.avoid_r = 0;
    crowd_draw(&o);
}

#define SHOT(name, ...) \
    void scene_##name(int f, int n) { float ta = (float)tk_scene_start(#name) + f / 30.0f; (void)n; __VA_ARGS__ } \
    void scene_##name##_audio(double t0, double dur) { (void)t0; (void)dur; }

SHOT(a_fwoomp, {
    wd_cam(lerp(10, 20, seg(ta, 0, 5)), -20, 1.6f);
    bandstand_bg(ta, 0);
    possessed(ta);
    mayor_table(ta);
    panic(ta, -90, 110, 0);
    if (ta < 0.4f) tk_post_shake(3, 2);
    if (ta < 0.6f) tk_text_outline(60, 20, "FWOOMP", PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0), 3);
    say_world("Fudge! FUDGE!", ta, T_FUDGE, 1.0f, -40, 10, 0);
    say_world("Everybody stay calm! I'll be under here.", ta, T_CALM, 2.45f, 52, -30, -1);
})
SHOT(b_demons, {
    wd_cam(lerp(-10, 0, seg(ta, 5, 10)), -18, 1.5f);
    bandstand_bg(ta, 0);
    possessed(ta);
    mayor_table(ta);
    demons(ta, 5.3f, 14, -28, -30);
    panic(ta, -110, 90, 0);
    /* a citizen trips flat and waves the others on, nobly */
    if (ta >= 8.6f) { Critter c = cast_pip(-60, 30, ta * 30); c.s = 0.7f; c.fur = PAL_PAINT(PAINT_CARAMEL, 1); c.furd = PAL_PAINT(PAINT_CARAMEL, 0); c.ears = CR_EARS_ROUND; c.extras = 0; c.squash = 0.5f; c.armR = 2.8f + 0.3f * sinf(ta * 12); critter_draw(&c); }
    say_world("Being calm!", ta, T_BEING, 1.4f, 58, 0, -1);
    say_world("Single file, please!", ta, T_SINGLE, 1.0f, 10, 16, 0);
    say_world("Leave me! Save the cupcakes!", ta, T_LEAVE, 1.1f, -60, 22, 1);
    if (ta >= 5.3f && ta < 7) tk_text_outline(40, 30, "squeak!", PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0), 1);
})
static void teacups(float ta, int red) {
    float rot = -ta * 2.2f;   /* turning backwards */
    wd_blob_ol(0, 6, 60, 10, 0, PAL_PAINT(PAINT_LAVENDER, 1), INK, WB_SHADE);
    wd_rect(-2, -40, 4, 40, PAL_PAINT(PAINT_BONE, 2), INK);
    wd_blob_ol(0, -42, 24, 6, 0, PAL_PAINT(PAINT_ROSE, 2), INK, WB_SHADE);
    for (int k = 0; k < 4; k++) {
        float a = rot + k * 1.5708f, x = cosf(a) * 38, depth = sinf(a);
        float y = 2 + depth * 5, s = 1 + depth * 0.15f;
        uint8_t c = k == 0 ? PAL_PAINT(PAINT_MINT, 2) : k == 1 ? PAL_PAINT(PAINT_LEMON, 2) : k == 2 ? PAL_PAINT(PAINT_SKY, 2) : PAL_PAINT(PAINT_BUBBLEGUM, 2);
        wd_quad(x - 9 * s, y - 8 * s, x + 9 * s, y - 8 * s, x + 7 * s, y + 2 * s, x - 7 * s, y + 2 * s, c, INK);
        wd_blob_ol(x, y - 8 * s, 9 * s, 2.4f * s, 0, (red && k == 0) ? PAL_PAINT(PAINT_CHERRY, 0) : PAL_PAINT(PAINT_NAVY, 0), INK, 0);
    }
}
SHOT(c_cups, {
    night(ta); wd_cam(0, -18, 2.0f); fairground(ta);
    int red = ta >= T_TRUTH + 4 / 30.0f;
    int truth = ta >= T_TRUTH && ta < T_TRUTH + 4 / 30.0f;
    teacups(ta, red);
    float cx = cosf(-ta * 2.2f) * 38, cy = 2 + sinf(-ta * 2.2f) * 5 - 8;
    if (ta < T_INCUP) puff_demon(lerp(-60, cx, seg(ta, 10, T_INCUP)), lerp(-10, cy - 8, seg(ta, 10, T_INCUP)) - 10 * sinf(seg(ta, 10, T_INCUP) * 3.14f), 1.0f, ta * 30, 1);
    if (ta >= T_INCUP && ta < T_CONF) puff_demon(cx, cy + seg(ta, T_INCUP, T_CONF) * 4, 1.0f * (1 - seg(ta, T_INCUP, T_CONF)), ta * 90, 1);
    if (ta >= T_CONF && !truth) tk_confetti((int)wd_sx(cx), (int)wd_sy(cy - 4), (int)((ta - T_CONF) * 30), 9);
    if (ta >= T_CONF && ta < T_CONF + 0.8f && !truth) tk_heart((int)wd_sx(cx) - 3, (int)wd_sy(cy - 10) - (int)((ta - T_CONF) * 30), 1, PAL_PAINT(PAINT_ROSE, 2));
    if (truth) {   /* TRUTH 4: the cup as a blender: a red spiral of fur, meat and teeth, one eyeball riding it */
        for (int k = 0; k < 18; k++) { float a = k * 0.7f + ta * 20, r = 3 + k * 0.6f; wd_blob(cx + cosf(a) * r, cy - k * 1.2f, 1.6f, 1.2f, 0, k % 3 ? PAL_PAINT(PAINT_CHERRY, 0) : PAL_PAINT(PAINT_GRAPE, 1), 0); }
        wd_blob_ol(cx + 6, cy - 12, 1.6f, 1.6f, 0, PAL_WHITE, INK, 0); wd_blob(cx + 6.4f, cy - 12, 0.7f, 0.7f, 0, PAL_PAINT(PAINT_CHERRY, 1), 0);
        for (int k = 0; k < 4; k++) wd_tri(cx - 4 + k * 3, cy - 18, cx - 3 + k * 3, cy - 18, cx - 3.5f + k * 3, cy - 16, PAL_WHITE);
    }
    if (ta >= T_PLIP && ta < T_PLIP + 0.5f) tk_text_outline((int)wd_sx(cx) + 10, (int)wd_sy(cy) - 14, "plip", PAL_WHITE, PAL_PAINT(PAINT_CHERRY, 0), 1);
    if (ta >= 14.2f) {   /* the nearest demon peers over the rim, and gulps */
        float px = 30;
        puff_demon(px, -14 + 2 * sinf(ta * 3), 0.9f, 0, -1);
        if (ta >= 14.9f && ta < 15.4f) tk_text_outline((int)wd_sx(px) - 6, (int)wd_sy(-30), "gulp", PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0), 1);
    }
    say_world("Not the teacups!", ta, T_NOTCUPS, 1.2f, -70, -10, 1);
})
SHOT(d_house, {
    night(ta); wd_cam(0, -30, 2.2f); fairground(ta);
    wd_rect(-40, -60, 80, 60, PAL_PAINT(PAINT_LAVENDER, 1), INK);
    wd_tri(-46, -60, 46, -60, 0, -86, PAL_PAINT(PAINT_GRAPE, 1));
    wd_rect(-8, -22, 16, 22, PAL_PAINT(PAINT_GRAPE, 0), INK);
    wd_rect(-38, -16, 28, 14, PAL_WHITE, INK);
    tk_text((int)wd_sx(-37), (int)wd_sy(-15), "NOTHING", PAL_PAINT(PAINT_CHERRY, 0), 1);
    tk_text((int)wd_sx(-37), (int)wd_sy(-15) + 9, "INSIDE!", PAL_PAINT(PAINT_CHERRY, 0), 1);
    tk_text((int)wd_sx(-37), (int)wd_sy(-15) + 18, "(we checked)", PAL_PAINT(PAINT_NAVY, 1), 1);
    for (int k = 0; k < 4; k++) {   /* every cardboard ghost turns to face the Wall, in a ripple */
        float wx = -30 + k * 18, turn = seg(ta, 16.4f + k * 0.15f, 16.6f + k * 0.15f);
        wd_rect(wx - 6, -52, 12, 14, PAL_PAINT(PAINT_LEMON, 2), INK);
        float w = lerp(4, 1.2f, turn);
        wd_blob(wx + turn * 2, -45, w, 4, 0, PAL_WHITE, WB_OUT);
        if (turn < 0.5f) { wd_blob(wx - 1.2f, -46, 0.6f, 0.8f, 0, INK, 0); wd_blob(wx + 1.2f, -46, 0.6f, 0.8f, 0, INK, 0); }
    }
    if (ta >= 16.4f && ta < 17.1f) tk_text_outline(150, 20, "fwip-fwip", PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0), 1);
    say_world("Is this the weather?!", ta, T_WEATHER, 1.0f, 30, -10, 1);
})
SHOT(e_gust, {
    wd_cam(lerp(30, 34, seg(ta, 19, 23)), -36, 1.7f);
    float g = seg(ta, T_GUST, T_GUST + 0.8f);
    bandstand_bg(ta, g >= 1);
    possessed(ta);
    if (g < 1) for (int k = 0; k < 3; k++) {   /* the boards cartwheel off into the dark */
        float u = seg(ta, T_GUST + k * 0.2f, T_GUST + 0.8f + k * 0.2f);
        if (u > 0 && u < 1) { float x = 36 + u * 120, y = -50 + k * 15 - u * 40; float a = u * 9; wd_quad(x + cosf(a) * -30, y + sinf(a) * -3, x + cosf(a) * 30, y + sinf(a) * 3, x + cosf(a) * 30 - sinf(a) * 3, y + sinf(a) * 3 + cosf(a) * 3, x - cosf(a) * 30 - sinf(a) * 3, y - sinf(a) * 3 + cosf(a) * 3, PAL_PAINT(PAINT_CARAMEL, 1), INK); }
    }
    if (ta >= T_GUST && ta < T_GUST + 1.2f) tk_text_outline(120, 14, "CLATTER", PAL_WHITE, PAL_PAINT(PAINT_CHOCOLATE, 1), 2);
    for (int k = 0; k < 8; k++) { float x = fmodf(ta * 120 + k * 40, 240) - 20; tk_line((int)x, 30 + k * 12, (int)x + 18, 28 + k * 12, PAL_PAINT(PAINT_SILVER, 2)); }   /* fog gust */
    /* the LOST PROPERTY lantern lifts free, Sunny's "1" hanging from it */
    float lu = seg(ta, T_GUST + 0.6f, 23.0f);
    float lx = lerp(-60, -40, lu), ly = lerp(-62, -100, lu);
    wd_blob_ol(lx, ly, 3, 3.6f, 0, PAL_PAINT(PAINT_BONE, 2), PAL_PAINT(PAINT_CARAMEL, 0), 0);
    wd_text(lx - 2.5f, ly + 5, "1", 0.6f, PAL_PAINT(PAINT_NAVY, 1), 0);
})
SHOT(f_pip, {
    night(ta); wd_cam(0, -10, 3.2f); fairground(ta);
    float up = seg(ta, 23.0f, 23.3f);
    Critter p = cast_pip(0, 2 + (1 - up) * 18, ta * 30); p.s = 0.8f; p.armL = p.armR = 2.8f; p.mouth = ta >= T_FIRE ? 1 : 0;
    p.ears = CR_EARS_LONG;
    critter_draw(&p);
    wd_rect(-14, -8, 28, 10, PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_CARAMEL, 0));   /* the hay bale in front */
    if (ta >= T_FIRE) { bubble_side = 1; bubble("FIREWORKS!", 10, (int)wd_sx(critter_mouth_x), (int)wd_sy(critter_mouth_y)); bubble_side = 0; }
})
SHOT(g_notnow, {
    float cu = seg(ta, 28.4f, 30.0f);
    wd_cam(lerp(0, 0, cu), lerp(-14, -4, cu), lerp(1.5f, 2.6f, cu));
    night(ta); fairground(ta);
    float stop = ta >= 25.3f && ta < 26.6f ? 1 : 0;
    panic(ta, -110, 110, stop);
    /* two citizens at a gap between two hay bales: bow, bow */
    for (int k = -1; k <= 1; k += 2) {
        Critter c = cast_fuzz(-40 + k * 8, 34, ta * 30); c.s = 0.65f; c.extras = 0; c.outfit = k < 0 ? PAL_PAINT(PAINT_MINT, 1) : PAL_PAINT(PAINT_LEMON, 1); c.outfitd = PAL_PAINT(PAINT_TEAL, 0); c.outfitl = PAL_WHITE;
        c.fur = k < 0 ? PAL_WHITE : PAL_PAINT(PAINT_HONEY, 2); c.furd = PAL_PAINT(PAINT_BONE, 1);
        c.flip = -k; c.squash = ta >= 26.8f ? 0.9f + 0.1f * cosf(ta * 4 + k) : 1; c.armL = c.armR = ta >= 26.8f ? 1.2f : 0.4f;
        critter_draw(&c);
    }
    /* Sunny: the one still point; the crowd splits round them */
    Sunny u; sunny_default(&u, 0, 30, 0.8f, ta * 30); u.suitcase = 0; u.no_page = 1; u.page = 0.25f; sunny_draw(&u);
    wd_blob_ol(sunny_hand_x - 2, sunny_hand_y - 3, 5, 4, 0, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0), WB_SHADE);
    /* the LOST PROPERTY lantern drifts off toward the Wall; nobody notices */
    wd_blob_ol(lerp(40, 90, seg(ta, 25, 30)), lerp(-60, -80, seg(ta, 25, 30)), 2.4f, 3, 0, PAL_PAINT(PAINT_BONE, 2), PAL_PAINT(PAINT_CARAMEL, 0), 0);
    if (ta >= T_NOTNOW && ta < T_NOTNOW + 1.1f) tk_caption(20, "NOT NOW, PIP.", PAL_WHITE, PAL_PAINT(PAINT_NAVY, 1), 2);
    say_world("After you!", ta, T_AFTER, 0.7f, -48, 18, -1);
    say_world("No, after YOU!", ta, T_YOU, 0.8f, -32, 18, 1);
})

static void babble(double t, float secs, float pitch, float vol) {
    int n = (int)(secs * 7.5f); if (n < 1) n = 1;
    for (int k = 0; k < n; k++) {
        uint32_t h = tk_hash((uint32_t)k * 31u + (uint32_t)(secs * 100) + (uint32_t)pitch);
        tk_note(t + k * secs / n, 0.07, (int)pitch + (int)(h % 5) - 2, TKV_TRI, vol);
        tk_note(t + k * secs / n, 0.05, (int)pitch + 12 + (int)(h % 5) - 2, TKV_SQ12, vol * 0.25f);
    }
}
static void walla(double t, float secs, float vol) { for (int v = 0; v < 4; v++) babble(t + v * 0.05, secs, 60 + v * 4, vol); }
void tk_music(void) {
    tk_note(0, 0.5, 30, TKV_SAW, 0.4f); tk_slide(0, 0.6, 50, 30, TKV_NOISE, 0.3f);    /* FWOOMP */
    /* the riff at swing tempo, under everything (out for the truth insert) */
    static const int R[8] = { 76, 75, 72, 75, 76, 79, 75, 72 };
    for (int i = 0; ; i++) {
        double t = 0.2 + i * 0.2 + ((i & 1) ? 0.05 : 0);
        if (t >= 30) break;
        if (t >= T_TRUTH && t < T_TRUTH + 0.9) continue;
        tk_note(t, 0.15, R[i % 8], TKV_SINE, 0.14f);
        if (i % 2 == 0) tk_kick(t, 0.14f);
        if (i % 4 == 2) tk_snare(t, 0.12f);
    }
    walla(T_FUDGE, 1.0f, 0.1f);
    line_at("a2_03_mayor_01", T_CALM);
    line_at("a2_03_mayor_02", T_BEING);
    for (int k = 0; k < 10; k++) tk_slide(5.3 + k * 0.12, 0.06, 90, 96, TKV_SQ12, 0.05f);   /* demon squeaks */
    walla(T_SINGLE, 1.0f, 0.08f); walla(T_LEAVE, 1.1f, 0.08f); walla(T_NOTCUPS, 1.2f, 0.09f);
    tk_slide(10.0, 4, 70, 60, TKV_SAW, 0.04f);                       /* teacups whirring in reverse */
    tk_sfx(T_CONF, SFX_SPARKLE, 0.3f);
    tk_slide(T_TRUTH, 4 / 30.0, 40, 70, TKV_NOISE, 0.4f);           /* the wet shred */
    tk_note(T_PLIP, 0.05, 88, TKV_SINE, 0.25f);
    tk_slide(14.95, 0.2, 55, 48, TKV_TRI, 0.2f);                     /* gulp */
    for (int k = 0; k < 3; k++) tk_note(16.4 + k * 0.15, 0.04, 80, TKV_NOISE, 0.12f);   /* fwip */
    walla(T_WEATHER, 1.0f, 0.09f);
    tk_slide(T_GUST, 1.0, 50, 70, TKV_NOISE, 0.3f);
    for (int k = 0; k < 3; k++) tk_note(T_GUST + k * 0.22, 0.08, 45, TKV_NOISE, 0.3f);  /* clatter */
    babble(T_FIRE, 0.8f, 79, 0.25f);                                  /* FIREWORKS! (Pip) */
    for (int v = 0; v < 6; v++) babble(T_NOTNOW + v * 0.01, 1.0f, 55 + v * 3, 0.07f);   /* NOT NOW, PIP (unison) */
    babble(T_AFTER, 0.7f, 64, 0.16f); babble(T_YOU, 0.8f, 68, 0.16f);
}
