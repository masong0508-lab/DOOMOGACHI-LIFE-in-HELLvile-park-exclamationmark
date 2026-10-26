/* shots.c - the 13 shots of the pilot proof (cold open, live-action half).
 *
 * Direction: shots breathe (4-8 s) and the energy lives INSIDE them -
 * constant character motion (squash/stretch, anticipation, overshoot),
 * camera push-ins and drifts, sfx hits, pop-ins and small shakes on impacts.
 * Extreme close-ups are kept for the big beats only: the M, and the egg.
 * Each shot: scene_pNN(f, n) draws local frame f; scene_pNN_audio(t0, dur).
 */
#include "tk.h"
#include "wd.h"
#include "mayor.h"
#include "sets.h"
#include <math.h>
#include <string.h>

#define AT(fr) (t0 + (fr) / 30.0)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }
/* overshoot ease 0..1 (back-out) */
static float pop(float f, float a, float b) {
    float u = wd_clamp01((f - a) / (b - a));
    float s = 2.2f;
    u -= 1;
    return 1 + u * u * ((s + 1) * u + s);
}
static float spring(float f, float f0, float amp) { return wd_spring(f, f0, amp, 0.55f, 0.11f); }

/* dialogue box: show `chars` characters of text */
static void say(const char *text, int chars) {
    int n = (int)strlen(text);
    if (chars > n) chars = n;
    if (chars < 0) return;
    tk_dialog("MAYOR GIGGS", text, (int)ceilf(chars / 1.5f), TKW_CUTE);
}
/* a floating music note (hum) */
static void note_glyph(float x, float y, float age, uint8_t c) {
    if (age < 0 || age > 50) return;
    float dx = 3 * sinf(age * 0.15f), dy = -age * 0.45f;
    float s = 1.0f;
    if (age > 40) s = 1 - (age - 40) / 10.0f;
    wd_blob(x + dx, y + dy, 1.3f * s, 1.0f * s, -0.4f, c, WB_OUT);
    wd_line(x + dx + 1.1f * s, y + dy, x + dx + 1.1f * s, y + dy - 4.2f * s, 0.4f, wd_ol(c));
    wd_line(x + dx + 1.1f * s, y + dy - 4.2f * s, x + dx + 2.8f * s, y + dy - 2.8f * s, 0.4f, wd_ol(c));
}
/* screen-space "pop-in" sound word (the teaser's onomatopoeia) */
static void sfx_word(const char *w, float sx, float sy, float f, float f0, uint8_t c, uint8_t oc) {
    float d = f - f0;
    if (d < 0 || d > 22) return;
    int scale = d < 3 ? 1 : 2;
    int x = (int)sx - tk_text_w(w, scale) / 2, y = (int)sy - (int)(d * 0.4f);
    if (d > 18 && ((int)d & 1)) return;
    tk_text_outline(x, y, w, c, oc, scale);
}

/* the Mayor on top of the stepladder, with a per-shot clock */
static void mayor_up(Mayor *m, float f, float base) {
    mayor_default(m, LADDER_X + 1, -34.5f, 1.0f, base + f);
}
/* roller from the near hand to a target point on the banner */
static void roller_at(float rx, float ry, float wet) {
    float hx = mayor_hand_x[1], hy = mayor_hand_y[1];
    float ang = atan2f(ry - hy, rx - hx);
    float len = sqrtf((rx - hx) * (rx - hx) + (ry - hy) * (ry - hy));
    float ex = rx - cosf(ang) * 2.5f, ey = ry - sinf(ang) * 2.5f;
    float mx = hx + (ex - hx) * 0.7f, my = hy + (ey - hy) * 0.7f;
    wd_tube(mx, my, rx, ry, 0.55f, 0.55f, PAL_GRAY_D, PAL_GRAY, PAL_GRAY_L, PAL_WHITE);
    wd_tube(hx, hy, mx, my, 1.25f, 1.05f, PAL_PAINT(PAINT_CHOCOLATE, 1), PAL_PAINT(PAINT_HONEY, 0), PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_HONEY, 2));
    /* the roller: a fuzzy pink cylinder with grey end caps */
    wd_rect(rx - 6, ry - 2.2f, 12, 4.4f, PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_ROSE, 0));
    for (int k = 0; k < 6; k++) wd_line(rx - 5 + k * 2, ry - 1.6f, rx - 5.6f + k * 2, ry + 1.6f, 0.35f, PAL_PAINT(PAINT_ROSE, 1));
    wd_rect(rx - 7, ry - 1.4f, 1.2f, 2.8f, PAL_GRAY_L, PAL_GRAY_D);
    wd_rect(rx + 5.8f, ry - 1.4f, 1.2f, 2.8f, PAL_GRAY_L, PAL_GRAY_D);
    if (wet > 0) wd_rect(rx - 5, ry - 1.8f, 9, 0.8f, PAL_PAINT(PAINT_BUBBLEGUM, 2), 0);
    (void)len;
}
/* arm angle that points the near hand at a world target */
static float aim(const Mayor *m, float tx, float ty) {
    float shx = m->x + 6.5f * m->s * m->flip, shy = m->y - 23.5f * m->s;
    float a = atan2f(ty - shy, (tx - shx) * m->flip);
    return 1.5708f - a;
}
/* point the near arm at a target and stretch it (cartoon) if it is out of reach */
static void aim_reach(Mayor *m, float tx, float ty) {
    m->armR = aim(m, tx, ty);
    float shx = m->x + 6.5f * m->s * m->flip, shy = m->y - 23.5f * m->s;
    float d = sqrtf((tx - shx) * (tx - shx) + (ty - shy) * (ty - shy)) / m->s;
    m->reach = d > 11.5f ? d - 11.5f : 0;
}
static void bandstand_world(float t, const Banner *b) {
    set_sky_dawn(t, 0.25f);
    set_wall(t);
    set_bandstand(t, b, 0);
    set_fountain(t, 0, 0);
}

/* ===================================================================== 1 */
/* CRAYON STORYBOOK: the banner, close. The music box holds its last note. */
void scene_p01_storybook(int f, int n) {
    float u = seg(f, 0, n);
    wd_cam(-22, -60, lerp(3.0f, 3.2f, u));
    set_storybook(f);
    /* a crayon roller resting on the banner edge, still wet */
    wd_blob(-6, -52, 2.2f, 5.0f, 0.2f, PAL_PAINT(PAINT_BUBBLEGUM, 1), WB_OUT);
    wd_line(-6, -47, -2, -38, 1.2f, PAL_PAINT(PAINT_HONEY, 1));
    wd_post_crayon(f, PAL_UI_CREAM);
}
void scene_p01_storybook_audio(double t0, double dur) {
    /* music box: the melody's last phrase, landing on the note the hum will take */
    int m[4] = { 75, 73, 71, 66 };
    for (int i = 0; i < 4; i++) {
        double t = t0 + 0.35 + i * 0.4, d = i == 3 ? (dur - 0.35 - 1.2 + 0.25) : 0.36;
        tk_note(t, d, m[i], TKV_SINE, i == 3 ? 0.42f : 0.5f);
        tk_note(t, 0.12, m[i] + 12, TKV_TRI, 0.18f);
    }
}

/* ===================================================================== 2 */
/* MATCH CUT to the real banner at dawn. Wet pink paint, a drip. Pull back to
 * find the Mayor up a ladder, rolling paint and humming the melody. */
static void hum_pose(Mayor *m, float f) {
    float beat = f / 12.0f;                 /* 150 bpm = 12 frames a beat */
    float bar = beat / 3.0f;
    float sway = sinf(bar * 6.2832f);
    m->expr = MX_HAPPY;
    m->lean = 0.06f * sway;
    m->head_tilt = 0.10f * sway;
    m->head_dy = -0.6f * fabsf(sinf(beat * 3.1416f));
    m->squash = 1.0f + 0.035f * cosf(beat * 6.2832f);
    m->wattle = 0.25f * sway;
    m->hat_tilt = -0.08f * sway;
    m->fan = 0.35f + 0.12f * fabsf(sinf(bar * 3.1416f));
    m->flutter = 0.8f;
    m->mouth = 0.05f;
    m->hair_kick = 0.4f * sway;
    m->armL = 0.5f + 0.15f * sway;
}
void scene_p02_banner(int f, int n) {
    float pull = seg(f, 40, 130), push = seg(f, 140, n);
    float cx = lerp(-22, -60, pull), cy = lerp(-60, -54, pull), z = lerp(3.2f, 1.65f, pull);
    cx = lerp(cx, -50, push); cy = lerp(cy, -58, push); z = lerp(z, 1.95f, push);
    wd_cam(cx, cy, z);
    Banner b = { 0 };
    bandstand_world(f, &b);
    set_ladder();
    /* a fat pink drip forms on the hem and lets go */
    {
        float x = -21, g = seg(f, 0, 26), fall = f > 26 ? (f - 26) * (f - 26) * 0.05f : 0;
        if (fall < 60) wd_blob(x, BAN_Y1 + 1 + g * 2 + fall, 0.9f + g * 0.6f, 1.2f + g * 1.0f, 0, PAL_PAINT(PAINT_BUBBLEGUM, 1), WB_OUT | WB_SHADE);
    }
    Mayor m;
    mayor_up(&m, f, 0);
    hum_pose(&m, f);
    /* rolling: up and down on the downbeats, across the left of the band */
    float beat = f / 12.0f;
    float ry = -60 + 6.5f * sinf(beat * 3.1416f * 2.0f / 3.0f);
    float rx = -55 + 1.5f * sinf(beat * 0.5f);
    m.armR = aim(&m, rx, ry);
    mayor_draw(&m);
    roller_at(rx, ry, 1);
    /* hum notes drifting off the beak */
    for (int k = 0; k < 6; k++) {
        float a = f - (20 + k * 36);
        note_glyph(mayor_head_x + 5, mayor_head_y - 2, a, k & 1 ? PAL_PAINT(PAINT_MAGENTA, 1) : PAL_PAINT(PAINT_TEAL, 1));
    }
    /* wet-paint glints on the letters */
    for (int k = 0; k < 3; k++) {
        int ph = (f + k * 17) % 40;
        if (ph < 8) tk_star((int)wd_sx(LET_X0 + 6 + k * 21), (int)wd_sy(-63 + k * 2), ph < 4 ? ph / 2 : (8 - ph) / 2, PAL_WHITE);
    }
}
void scene_p02_banner_audio(double t0, double dur) {
    /* the hum itself is the creator's recording, mixed in by assemble.sh */
    tk_note(AT(30), 0.05, 91, TKV_SINE, 0.25f);     /* plip */
    for (int k = 0; k < 5; k++) {                   /* dawn birds */
        tk_slide(AT(18 + k * 41), 0.07, 96, 100, TKV_SINE, 0.12f);
        tk_slide(AT(22 + k * 41), 0.06, 98, 103, TKV_SINE, 0.10f);
    }
    for (int k = 0; k < 10; k++) tk_note(AT(6 + k * 24), 0.09, 50, TKV_NOISE, 0.05f);  /* roller squelch */
}

/* ===================================================================== 3 */
/* He stops humming. On the same note. "...Hee. Hee." The fountain giggles
 * back - a quarter-second late. Whip down: two stone faces, giggling. They
 * stop the instant we look. Whip back: the Mayor, eyes narrowed, down at it. */
void scene_p03_heehee(int f, int n) {
    float down = seg(f, 100, 110) * (1 - seg(f, 150, 160));
    float cx = lerp(-86, FOUNT_X, down), cy = lerp(-70, FOUNT_Y - 26, down), z = lerp(3.0f, 2.7f, down);
    cx += 0.02f * f; cy += f < 86 ? 0.01f * f : 0;
    wd_cam(cx, cy, z);
    Banner b = { 0 };
    set_sky_dawn(f + 240, 0.25f);
    set_wall(f);
    set_bandstand(f + 240, &b, 0);
    set_fountain(f, f >= 88 && f < 140 ? 1 : 0, 88);
    set_ladder();
    Mayor m;
    mayor_up(&m, f, 240);
    if (f < 6) hum_pose(&m, f + 240);
    else {
        m.expr = MX_NEUTRAL;
        m.flutter = 0.3f;
        m.eye_pop = 0.6f * expf(-(f - 6) * 0.15f);
        m.fan = 0.35f;
        m.wattle = spring(f, 6, 0.35f);
        m.hat_stretch = spring(f, 6, 1.2f);
    }
    if (f >= 20 && f < 24) m.blink = 1;
    if (f >= 29 && f < 32) m.blink = 1;
    /* the hees */
    float h1 = f - 50, h2 = f - 70;
    if (h1 >= 0 && h1 < 8) m.mouth = 0.7f * sinf(h1 / 8 * 3.14f);
    if (h2 >= 0 && h2 < 9) m.mouth = 0.9f * sinf(h2 / 9 * 3.14f);
    m.fan += 0.3f * (h1 >= 0 ? expf(-h1 * 0.08f) : 0) + 0.3f * (h2 >= 0 ? expf(-h2 * 0.08f) : 0);
    m.squash += spring(f, 50, 0.05f) + spring(f, 70, 0.06f);
    if (f >= 50) m.expr = f < 84 ? MX_HAPPY : MX_NEUTRAL;
    /* the fountain answers: eyes dart down */
    if (f >= 92) {
        m.look_y = 0.9f; m.look_x = 0.2f;
        m.wattle += spring(f, 92, 0.4f);
        m.eye_pop = 0.5f * expf(-(f - 92) * 0.12f);
    }
    if (f >= 160) {
        m.expr = MX_SQUINT;
        m.look_y = 0.9f;
        m.lean = -0.04f;
        m.wattle += 0.06f * sinf(f * 1.7f);
        m.head_dx = -0.5f;
    }
    if (f >= 178) { m.look_y = lerp(0.9f, -0.1f, seg(f, 178, 184)); m.look_x = lerp(0.2f, 1.0f, seg(f, 178, 184)); }
    float rx = -55, ry = -56;
    m.armR = aim(&m, rx, ry);
    mayor_draw(&m);
    roller_at(rx, ry, 1);
    if (f < 8) note_glyph(mayor_head_x + 5, mayor_head_y - 2, 30 + f * 2, PAL_PAINT(PAINT_TEAL, 1));
    /* fountain: "hee hee" sound words while we look at it */
    if (down > 0.9f && f < 140) {
        sfx_word("hee", wd_sx(FOUNT_X - 18), wd_sy(FOUNT_Y - 44), f, 112, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0));
        sfx_word("hee!", wd_sx(FOUNT_X + 18), wd_sy(FOUNT_Y - 46), f, 122, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0));
    }
    if (f >= 36 && f < 100) {
        int ch = f < 50 ? (f - 36) / 5 : f < 70 ? 3 + (int)fminf(4, (f - 50) * 1.0f) : 7 + (int)fminf(5, (f - 70) * 1.0f);
        say("...Hee. Hee.", ch);
    }
}
void scene_p03_heehee_audio(double t0, double dur) {
    /* "...Hee. Hee." is the Mayor's locked take (voice.lock co13_heehee),
     * mixed in by assemble.sh at 11.5 + 50/30 s. The fountain giggles back,
     * a quarter-second after he stops (stone echo). */
    for (int k = 0; k < 2; k++) {
        tk_slide(AT(88 + k * 8), 0.09, 84, 88, TKV_SQ12, 0.22f);
        tk_slide(AT(89 + k * 8), 0.09, 72, 76, TKV_TRI, 0.18f);
    }
    tk_sfx(AT(100), SFX_WHOOSH, 0.35f);
    for (int k = 0; k < 3; k++) {          /* giggling on camera */
        tk_slide(AT(112 + k * 9), 0.08, 84, 89, TKV_SQ12, 0.22f);
        tk_slide(AT(113 + k * 9), 0.08, 72, 77, TKV_TRI, 0.16f);
    }
    tk_note(AT(140), 0.03, 60, TKV_NOISE, 0.2f);   /* stone stop */
    tk_sfx(AT(150), SFX_WHOOSH, 0.35f);
    tk_slide(AT(178), 0.12, 70, 74, TKV_SQ12, 0.08f);  /* eye-slide */
}

/* ===================================================================== 4 */
/* The wet mystery M blooms back through the pink. He rolls over it.
 * Squeak. It blooms back. Squeak, lower. Bigger. Wind-up. SLAM. */
void scene_p04_roll(int f, int n) {
    float push = seg(f, 0, 150), push2 = seg(f, 150, 190);
    wd_cam(lerp(-72, -68, push), lerp(-60, -61, push), lerp(2.2f, 2.55f, push) + 0.25f * push2);
    Banner b = { 0 };
    /* stain timeline */
    float sc = 1.0f;
    if (f < 52) b.n_amt = f < 10 ? 0 : pop(f, 10, 30);
    else if (f < 66) { b.n_amt = 1; b.rolling = 1; b.roll_y = lerp(N_CY - 12, N_CY + 12, seg(f, 52, 64)); }
    else if (f < 92) { b.n_amt = 0; b.wet = 1; }
    else if (f < 118) b.n_amt = pop(f, 92, 108);
    else if (f < 128) { b.n_amt = 1; b.rolling = 1; b.roll_y = lerp(N_CY - 12, N_CY + 12, seg(f, 118, 127)); }
    else if (f < 134) { b.n_amt = 0; b.wet = 1; }
    else if (f < 180) { b.n_amt = pop(f, 134, 150); sc = 1.35f; }
    else if (f < 188) { b.n_amt = 1; sc = 1.35f; b.rolling = 1; b.roll_y = lerp(N_CY - 14, N_CY + 14, seg(f, 180, 187)); }
    else { b.n_amt = 0; b.wet = 1; }
    b.n_scale = sc;
    bandstand_world(f + 435, &b);
    set_ladder();

    Mayor m;
    mayor_up(&m, f, 435);
    m.flutter = 0.35f;
    m.look_x = 1.0f; m.look_y = 0.1f;
    /* double take at the first bloom */
    m.eye_pop = 0.9f * (f >= 20 ? expf(-(f - 20) * 0.08f) : 0);
    m.head_dx = -1.2f * (f >= 20 ? expf(-(f - 20) * 0.1f) : 0);
    m.wattle = spring(f, 20, 0.5f) + spring(f, 100, 0.4f) + spring(f, 140, 0.5f) + spring(f, 186, 0.6f);
    m.hat_stretch = spring(f, 20, 2.0f) + spring(f, 186, 1.8f);
    m.fan = 0.3f + 0.5f * (f >= 20 ? expf(-(f - 20) * 0.05f) : 0);
    /* roller target */
    float rx = N_CX, ry = N_CY - 10;
    if (f < 40) { rx = -56; ry = -57; }
    else if (f < 52) { rx = lerp(-56, N_CX, seg(f, 40, 50)); ry = lerp(-57, N_CY - 13, seg(f, 40, 50)); m.lean = -0.06f * seg(f, 40, 50); }
    else if (f < 66) { ry = lerp(N_CY - 12, N_CY + 12, seg(f, 52, 64)); m.lean = 0.08f; m.squash = 0.96f; }
    else if (f < 92) { ry = lerp(N_CY + 12, N_CY + 2, seg(f, 66, 80)); m.expr = MX_SMUG; m.head_dy = 0.8f * sinf((f - 66) * 0.5f) * (f < 80); }
    else if (f < 118) { ry = N_CY + 2; m.expr = MX_SHOCK; m.eye_pop = 0.5f; m.wattle += 0.05f * sinf(f * 2.1f); }
    else if (f < 128) { ry = lerp(N_CY - 12, N_CY + 12, seg(f, 118, 127)); m.lean = 0.1f; m.expr = MX_ANGRY; }
    else if (f < 160) {
        ry = N_CY + 6; m.expr = MX_SQUINT;
        float lean = seg(f, 140, 156);
        m.lean = 0.16f * lean; m.head_dx = 1.5f * lean;
        m.wattle += 0.09f * sinf(f * 1.9f);
    } else if (f < 180) {           /* wind-up */
        float w = seg(f, 160, 176);
        m.lean = lerp(0.16f, -0.16f, w); m.squash = lerp(1.0f, 0.9f, w);
        m.expr = MX_ANGRY; m.fan = lerp(0.4f, 1.0f, w);
        rx = lerp(N_CX, N_CX - 2, w); ry = lerp(N_CY + 6, N_CY - 18, w);
    } else if (f < 188) {           /* SLAM */
        ry = lerp(N_CY - 16, N_CY + 14, seg(f, 180, 186)); m.lean = 0.14f; m.squash = 1.08f; m.expr = MX_ANGRY; m.fan = 1.0f;
    } else {                        /* pant */
        ry = N_CY + 12; m.lean = 0.05f; m.expr = MX_NEUTRAL; m.squash = 1 + 0.04f * sinf(f * 0.6f);
        m.fan = lerp(1.0f, 0.4f, seg(f, 188, 206)); m.mouth = 0.3f + 0.2f * sinf(f * 0.6f);
    }
    if (f >= 68 && f < 84) m.mouth = 0.4f * fabsf(sinf((f - 68) * 0.7f));
    if (f >= 106 && f < 114) m.mouth = 0.8f * fabsf(sinf((f - 106) * 0.5f));
    m.armR = aim(&m, rx, ry);
    mayor_draw(&m);
    roller_at(rx, ry, 1);
    /* two short mutters, so we know what he is doing and how it is going */
    if (f >= 68 && f < 96) say("There. Giggle. Much better.", (f - 68) * 2);
    if (f >= 106 && f < 132) say("Oh, come ON.", (f - 106) * 2);
    sfx_word("squeak", wd_sx(N_CX + 14), wd_sy(N_CY - 14), f, 54, PAL_WHITE, PAL_PAINT(PAINT_BUBBLEGUM, 0));
    sfx_word("squeak", wd_sx(N_CX + 14), wd_sy(N_CY - 10), f, 119, PAL_WHITE, PAL_PAINT(PAINT_BUBBLEGUM, 0));
    if (f >= 186 && f < 192) tk_post_shake((f & 1) ? 2 : -2, (f & 1) ? -1 : 1);
}
void scene_p04_roll_audio(double t0, double dur) {
    tk_slide(AT(10), 0.3, 62, 50, TKV_SINE, 0.45f);   /* bloop: it blooms */
    tk_slide(AT(10), 0.3, 50, 38, TKV_TRI, 0.3f);
    tk_sfx(AT(20), SFX_BOING, 0.25f);                  /* double take */
    tk_slide(AT(53), 0.08, 86, 94, TKV_SQ12, 0.3f); tk_slide(AT(57), 0.1, 94, 86, TKV_SQ12, 0.3f);   /* squeak */
    { int m[6] = { 72, 74, 71, 76, 74, 72 }; for (int k = 0; k < 6; k++) tk_note(AT(68 + k * 3), 0.06, m[k], TKV_SQ25, 0.22f); }   /* "There. Giggle. Much better." */
    tk_slide(AT(106), 0.1, 74, 70, TKV_SQ25, 0.3f); tk_slide(AT(110), 0.25, 70, 79, TKV_SQ25, 0.34f);    /* "Oh, come ON." */
    tk_slide(AT(92), 0.3, 58, 46, TKV_SINE, 0.45f); tk_slide(AT(92), 0.3, 46, 34, TKV_TRI, 0.3f);
    tk_slide(AT(118), 0.07, 80, 88, TKV_SQ12, 0.3f); tk_slide(AT(121), 0.09, 88, 80, TKV_SQ12, 0.3f); /* lower */
    tk_slide(AT(134), 0.36, 55, 41, TKV_SINE, 0.5f); tk_slide(AT(134), 0.36, 43, 31, TKV_TRI, 0.35f);
    tk_slide(AT(160), 0.5, 55, 67, TKV_SQ12, 0.08f);   /* wind-up */
    tk_slide(AT(181), 0.07, 74, 82, TKV_SQ12, 0.32f); tk_slide(AT(184), 0.1, 82, 72, TKV_SQ12, 0.32f); /* lowest */
    tk_sfx(AT(186), SFX_THUD, 0.6f);
}

/* ===================================================================== 5 */
/* EXTREME CLOSE-UP on the M. Fresh paint, glinting. Then it blooms back
 * biggest - red this time (paint: CHERRY, glossy) - and a red drip runs. */
void scene_p05_red(int f, int n) {
    float dr = seg(f, 40, 150);
    wd_cam(-33, lerp(N_CY + 1, N_CY + 7, dr), lerp(2.05f, 2.25f, seg(f, 0, n)));
    Banner b = { 0 };
    b.n_scale = 1.75f; b.n_red = 1;
    b.n_amt = f < 24 ? 0 : pop(f, 24, 36);
    b.wet = f < 24 ? 1 : 0;
    b.drip = f < 44 ? 0 : 16 * powf(seg(f, 44, 150), 0.8f);
    set_sky_dawn(f, 0.25f);
    set_banner(f, &b, 0);
    if (f >= 28 && f < 36) tk_post_shake((f & 1) ? 2 : -2, 0);
}
void scene_p05_red_audio(double t0, double dur) {
    tk_sfx(AT(4), SFX_SPARKLE, 0.15f);
    /* dun dun DUNNN - on a toy organ */
    tk_note(AT(24), 0.18, 50, TKV_SAW, 0.3f); tk_note(AT(24), 0.18, 38, TKV_SQ50, 0.25f);
    tk_note(AT(31), 0.18, 49, TKV_SAW, 0.3f); tk_note(AT(31), 0.18, 37, TKV_SQ50, 0.25f);
    tk_note(AT(38), 1.2, 43, TKV_SAW, 0.32f); tk_note(AT(38), 1.2, 31, TKV_SQ50, 0.28f);
    tk_kick(AT(24), 0.5f); tk_kick(AT(38), 0.7f);
    for (int k = 0; k < 4; k++) tk_note(AT(70 + k * 20), 0.05, 40 - k, TKV_SINE, 0.25f);   /* drip creak */
    tk_note(AT(146), 0.05, 91, TKV_SINE, 0.3f);
}

/* ===================================================================== 6 */
/* FWOOF - tail fan. He whips out a cupcake balloon, ties it over the M -
 * tink - pats it twice. "Perfect." */
void scene_p06_perfect(int f, int n) {
    wd_cam(lerp(-72, -70, seg(f, 0, n)), -61, lerp(2.25f, 2.45f, seg(f, 60, n)));
    Banner b = { 0 };
    b.n_amt = 1; b.n_scale = 1.75f; b.n_red = 1; b.drip = 16 + f * 0.04f;
    bandstand_world(f + 790, &b);
    set_ladder();
    Mayor m;
    mayor_up(&m, f, 790);
    m.look_x = 1; m.flutter = 0.2f;
    m.eye_pop = 1.0f * expf(-f * 0.03f);
    m.expr = f < 20 ? MX_SHOCK : MX_NEUTRAL;
    m.fan = f < 6 ? 0.4f : lerp(1.0f, 0.55f, seg(f, 30, 60)) + spring(f, 6, 0.2f);
    m.hat_stretch = spring(f, 6, 2.5f);
    m.hair_kick = spring(f, 6, 1.5f);
    m.wattle = spring(f, 6, 0.6f) + spring(f, 34, 0.4f) + spring(f, 72, 0.25f) + spring(f, 86, 0.25f);
    /* balloon */
    float bxx = N_CX - 0.5f, byy = N_CY - 2;
    float tx = N_CX + 2, ty = BAN_Y1 - 1;
    float bq = 1;
    int show_b = f >= 34;
    float aimx, aimy;
    if (f < 20) { aimx = -54; aimy = -50; }
    else if (f < 34) { float w = seg(f, 20, 32); aimx = lerp(-54, -76, w); aimy = lerp(-50, -46, w); m.lean = -0.1f * w; m.squash = 1 - 0.06f * w; }
    else if (f < 62) {
        float w = seg(f, 36, 60);
        aimx = lerp(-60, bxx, w); aimy = lerp(-70, byy, w) - 8 * sinf(w * 3.14f);
        m.lean = lerp(0.0f, 0.1f, w);
    } else { aimx = bxx - 2; aimy = byy - 10; m.lean = 0.08f; }
    /* pats */
    float p1 = f - 72, p2 = f - 86;
    if (f >= 66 && f < 100) {
        float d = (p1 >= 0 && p1 < 8) ? sinf(p1 / 8 * 3.14f) : (p2 >= 0 && p2 < 8) ? sinf(p2 / 8 * 3.14f) : 0;
        aimx = bxx - 1; aimy = byy - 15 + d * 4;
    }
    bq = 1 + spring(f, 74, -0.18f) + spring(f, 88, -0.18f) + spring(f, 34, 0.25f);
    if (f >= 100) {
        m.expr = MX_SMUG; m.look_x = -0.3f; m.squash = 1.05f + 0.01f * sinf(f * 0.2f); m.fan = 0.8f + 0.1f * sinf(f * 0.15f);
        m.lean = -0.05f; m.flutter = 0.7f;
        aimx = -48; aimy = -46;
        float h = f - 104;
        if (h >= 0 && h < 18) m.mouth = 0.5f * fabsf(sinf(h * 0.55f));
    }
    if (f >= 62 && f < 92) m.mouth = 0.5f * fabsf(sinf((f - 62) * 0.55f));
    aim_reach(&m, aimx, aimy);
    mayor_draw(&m);
    if (show_b) {
        float x = f < 62 ? mayor_hand_x[1] + 2 : bxx, y = f < 62 ? mayor_hand_y[1] - 6 : byy;
        if (f >= 56 && f < 62) { float w = seg(f, 56, 62); x = lerp(x, bxx, w); y = lerp(y, byy, w); }
        float s = f < 40 ? pop(f, 34, 40) : 1;
        if (s > 0.05f) set_balloon(x, y, -0.15f, bq * s, 0.02f * sinf(f * 0.1f), f < 62 ? mayor_hand_x[1] : tx, f < 62 ? mayor_hand_y[1] : ty, f);
        if (f < 50) tk_sparkles((int)wd_sx(x), (int)wd_sy(y), f - 34, 5, 14);
    }
    if (f >= 62 && f < 76) tk_star((int)wd_sx(tx), (int)wd_sy(ty), (f - 62) < 7 ? (f - 62) / 2 : (14 - (f - 62)) / 2, PAL_WHITE);
    if (f >= 62 && f < 100) say("Ta-da! Nobody'll ever know.", (f - 62) * 2);
    if (f >= 100) {
        tk_sparkles((int)wd_sx(m.x + 2), (int)wd_sy(m.y - 40), f - 100, 9, 26);
        say("Perfect.", (f - 104) * 2);
    }
    sfx_word("FWOOF!", wd_sx(m.x - 18), wd_sy(m.y - 30), f, 6, PAL_WHITE, PAL_PAINT(PAINT_CARAMEL, 0));
    sfx_word("tink", wd_sx(tx + 8), wd_sy(ty + 8), f, 62, PAL_WHITE, PAL_PAINT(PAINT_TEAL, 0));
}
void scene_p06_perfect_audio(double t0, double dur) {
    tk_sfx(AT(6), SFX_WHOOSH, 0.5f);
    tk_note(AT(6), 0.12, 45, TKV_NOISE, 0.35f);          /* FWOOF */
    tk_sfx(AT(22), SFX_BLIP, 0.2f);                      /* rummage */
    tk_sfx(AT(34), SFX_POP, 0.7f);                       /* the balloon */
    tk_sfx(AT(35), SFX_SPARKLE, 0.3f);
    tk_note(AT(62), 0.35, 96, TKV_SINE, 0.45f);          /* tink */
    tk_note(AT(62), 0.2, 108, TKV_SINE, 0.15f);
    tk_slide(AT(74), 0.08, 60, 68, TKV_SINE, 0.4f); tk_note(AT(74), 0.04, 55, TKV_NOISE, 0.15f);   /* pat */
    tk_slide(AT(88), 0.08, 60, 68, TKV_SINE, 0.4f); tk_note(AT(88), 0.04, 55, TKV_NOISE, 0.15f);   /* pat */
    /* "Ta-da! Nobody'll ever know." - babble; "Perfect." is his locked take (co15_perfect, assemble.sh) */
    { int m[8] = { 79, 84, 72, 74, 76, 72, 74, 70 }; for (int k = 0; k < 8; k++) tk_note(AT(62 + k * 3 + (k > 1) * 3), 0.06, m[k], TKV_SQ25, 0.22f); }
    tk_sfx(AT(100), SFX_SPARKLE, 0.25f);
}

/* ===================================================================== 7 */
/* He glances south. We drift with his look: the boarded-up half, and the
 * Wall. The music box stops. Back on him: the wings stop. */
void scene_p07_glance(int f, int n) {
    float pan = seg(f, 26, 80), back = seg(f, 96, 104);
    float cx = lerp(-86, 160, pan), cy = lerp(-68, -64, pan), z = lerp(2.8f, 1.45f, pan);
    cx = lerp(cx, -86, back); cy = lerp(cy, -67, back); z = lerp(z, 3.1f, back);
    wd_cam(cx + (f > 104 ? 0.03f * (f - 104) : 0), cy, z);
    Banner b = { 0 };
    b.n_amt = 1; b.n_scale = 1.75f; b.n_red = 1; b.drip = 23;
    bandstand_world(f + 970, &b);
    set_ladder();
    set_balloon(N_CX - 0.5f, N_CY - 2, -0.15f, 1 + 0.02f * sinf(f * 0.2f), 0.02f * sinf(f * 0.1f), N_CX + 2, BAN_Y1 - 1, f);
    Mayor m;
    mayor_up(&m, f, 970);
    if (f < 20) { m.expr = MX_SMUG; m.fan = 0.8f; m.flutter = 0.7f; m.look_x = -0.3f; }
    else {
        m.expr = MX_NEUTRAL;
        m.look_x = 1; m.look_y = 0.1f;
        m.head_dx = 0.8f * seg(f, 20, 26);
        m.fan = f < 104 ? 0.6f : lerp(0.6f, 0.15f, seg(f, 104, 116));
        m.flutter = f < 104 ? 0.7f : 0.0f;          /* the wings stop */
        if (f >= 104) { m.brow_up = 0.4f; m.wattle = 0.02f * sinf(f * 0.3f); }
        if (f >= 128 && f < 136) m.blink = f < 132 ? 1 : 0.6f;
    }
    m.armR = aim(&m, -50, -46);
    mayor_draw(&m);
}
void scene_p07_glance_audio(double t0, double dur) {
    tk_slide(AT(20), 0.1, 70, 74, TKV_SQ12, 0.08f);     /* eye slide */
    tk_sfx(AT(96), SFX_WHOOSH, 0.3f);
}

/* ===================================================================== 8 */
/* INSERT (crayon): the small empty hand. Four frames. PLUCK. */
void scene_p08_pluck(int f, int n) { set_crayon_hand(f); }
void scene_p08_pluck_audio(double t0, double dur) {
    tk_note(t0, 0.7, 67, TKV_TRI, 0.7f);
    tk_note(t0, 0.06, 79, TKV_SQ25, 0.35f);
    tk_note(t0, 0.5, 79, TKV_SINE, 0.2f);
}

/* ===================================================================== 9 */
/* Back to scene: the balloon has turned to face the Wall. A small rubber
 * creak. His eyes slide to it. He turns it back. Squeak. A firm pat. "Stay." */
void scene_p09_stay(int f, int n) {
    wd_cam(lerp(-73, -70, seg(f, 0, n)), -61, lerp(2.3f, 2.6f, seg(f, 0, 90)));
    Banner b = { 0 };
    b.n_amt = 1; b.n_scale = 1.75f; b.n_red = 1; b.drip = 24;
    bandstand_world(f + 1124, &b);
    set_ladder();
    Mayor m;
    mayor_up(&m, f, 1124);
    m.flutter = 0; m.fan = 0.15f;
    m.look_x = 1.0f;
    float face = 1.0f, tilt = 0.08f + (f >= 12 ? 0.05f * expf(-(f - 12) * 0.08f) * sinf((f - 12) * 0.9f) : 0);
    float bq = 1;
    float ax = -52, ay = -46;
    if (f >= 24) { m.look_x = 1; m.look_y = 0.2f; m.head_dx = 0.6f; m.expr = MX_SQUINT; }
    if (f >= 46 && f < 50) m.blink = 1;
    if (f >= 28 && f < 50) m.mouth = 0.45f * fabsf(sinf((f - 28) * 0.6f));
    if (f >= 56 && f < 82) {
        float w = seg(f, 56, 64);
        ax = lerp(-52, N_CX + 6, w); ay = lerp(-46, N_CY - 2, w);
        m.lean = 0.1f * w;
        if (f >= 64) {
            face = 1.0f - 1.25f * pop(f, 64, 74) * (1.0f);
            if (face < -0.25f) face = -0.25f + spring(f, 72, 0.15f);
            tilt = lerp(0.08f, -0.03f, seg(f, 64, 70));
        }
    }
    if (f >= 74) { face = -0.15f + spring(f, 74, 0.2f); tilt = 0.0f; }
    if (f >= 82 && f < 100) {   /* the firm pat */
        float d = f - 86;
        ax = N_CX - 1; ay = N_CY - 15 + (d >= 0 && d < 6 ? sinf(d / 6 * 3.14f) * 5 : 0);
        m.expr = MX_ANGRY;
    }
    bq = 1 + spring(f, 88, -0.25f) + spring(f, 68, 0.12f);
    m.wattle = spring(f, 88, 0.5f) + spring(f, 68, 0.25f);
    if (f >= 100) {     /* "Stay." - the feather points */
        m.expr = MX_ANGRY; m.look_x = 0.8f; m.look_y = 0.2f;
        ax = N_CX - 2; ay = N_CY - 4;
        m.lean = 0.06f; m.fan = 0.45f + spring(f, 100, 0.2f);
        if (f >= 104 && f < 118) m.mouth = 0.7f * sinf((f - 104) / 14.0f * 3.14f);
        m.head_dy = spring(f, 150, 1.2f);     /* the nod */
        if (f >= 150) { m.expr = MX_SMUG; m.look_x = -0.2f; }
    }
    aim_reach(&m, ax, ay);
    m.grip = f >= 100 ? -1 : 0;
    mayor_draw(&m);
    set_balloon(N_CX - 0.5f, N_CY - 2, face, bq, tilt + (f >= 100 ? 0.01f * sinf(f * 1.3f) : 0), N_CX + 2, BAN_Y1 - 1, f);
    sfx_word("creak", wd_sx(N_CX + 16), wd_sy(N_CY - 18), f, 12, PAL_WHITE, PAL_PAINT(PAINT_CARAMEL, 0));
    sfx_word("squeak", wd_sx(N_CX + 16), wd_sy(N_CY - 18), f, 66, PAL_WHITE, PAL_PAINT(PAINT_BUBBLEGUM, 0));
    if (f >= 28 && f < 62) say("Hey. Eyes on the party.", (f - 28) * 2);
    if (f >= 100) say("Stay.", (f - 102) * 2);
}
void scene_p09_stay_audio(double t0, double dur) {
    tk_slide(AT(12), 0.35, 47, 50, TKV_SQ12, 0.22f);     /* rubber creak */
    tk_slide(AT(14), 0.3, 59, 61, TKV_SQ12, 0.1f);
    tk_slide(AT(26), 0.12, 70, 74, TKV_SQ12, 0.08f);     /* eyes slide */
    tk_slide(AT(66), 0.08, 86, 94, TKV_SQ12, 0.3f); tk_slide(AT(69), 0.1, 94, 84, TKV_SQ12, 0.3f);   /* squeak */
    tk_sfx(AT(88), SFX_THUD, 0.45f);                     /* firm pat */
    { int m[6] = { 72, 70, 74, 72, 76, 71 }; for (int k = 0; k < 6; k++) tk_note(AT(28 + k * 3 + (k > 0) * 3), 0.06, m[k], TKV_SQ25, 0.22f); }  /* "Hey. Eyes on the party." */
    /* "Stay." is his locked take (co18_stay), mixed in by assemble.sh */
    tk_note(AT(150), 0.05, 72, TKV_SINE, 0.15f);
}

/* ==================================================================== 10 */
/* He climbs down, briskly. Clack, clack, clack. Off he goes. The balloon
 * stays put. For now. */
void scene_p10_climb(int f, int n) {
    float pi = seg(f, 96, 140);
    wd_cam(lerp(-56, -56, pi), lerp(-46, -56, pi), lerp(1.3f, 2.2f, pi));
    Banner b = { 0 };
    b.n_amt = 1; b.n_scale = 1.75f; b.n_red = 1; b.drip = 24;
    bandstand_world(f + 1304, &b);
    set_ladder();
    float face = -0.1f;
    if (f >= 118) face = lerp(-0.1f, 0.55f, seg(f, 118, 140));
    set_balloon(N_CX - 0.5f, N_CY - 2, face, 1 + 0.02f * sinf(f * 0.2f), face * 0.05f, N_CX + 2, BAN_Y1 - 1, f);
    Mayor m;
    mayor_default(&m, LADDER_X + 1, -34.5f, 1.0f, 1304 + f);
    m.flip = f < 6 ? 1 : -1;
    m.fan = 0.4f; m.flutter = 0.5f; m.expr = MX_SMUG;
    static const float steps[5] = { -34.5f, -26, -19.5f, -13, 0 };
    static const int sf[5] = { 0, 12, 22, 32, 42 };
    float y = steps[0];
    for (int k = 1; k < 5; k++)
        if (f >= sf[k] - 6) {
            float w = seg(f, sf[k] - 6, sf[k]);
            y = lerp(steps[k - 1], steps[k], w) - (k == 4 ? 6 : 2.5f) * sinf(w * 3.14f);
            if (f < sf[k]) m.step = 2 * sinf(w * 3.14f);
        }
    m.y = y;
    float land = 0;
    for (int k = 1; k < 5; k++) land += spring(f, sf[k], k == 4 ? -0.16f : -0.08f);
    m.squash = 1 + land;
    m.hat_stretch = spring(f, 42, 2.0f);
    m.wattle = spring(f, 42, 0.5f) + spring(f, 12, 0.2f) + spring(f, 22, 0.2f) + spring(f, 32, 0.2f);
    if (f >= 48) {      /* jaunty walk off to the left, in waltz time */
        float d = f - 48;
        m.x = LADDER_X + 1 - d * 1.6f;
        float ph = d / 12.0f * 3.1416f;
        m.y = -fabsf(sinf(ph)) * 2.5f;
        m.step = 1.5f * sinf(ph);
        m.squash = 1 + 0.06f * cosf(ph * 2);
        m.lean = 0.08f;
        m.expr = MX_HAPPY; m.flutter = 0.9f; m.fan = 0.5f;
        m.armL = 0.6f + 0.4f * sinf(ph); m.armR = 0.6f - 0.4f * sinf(ph);
    } else m.armR = 0.3f;
    if (f < 120) mayor_draw(&m);
    sfx_word("clack", wd_sx(LADDER_X + 16), wd_sy(-26), f, 12, PAL_WHITE, PAL_PAINT(PAINT_CHOCOLATE, 1));
    sfx_word("clack", wd_sx(LADDER_X + 18), wd_sy(-18), f, 22, PAL_WHITE, PAL_PAINT(PAINT_CHOCOLATE, 1));
    sfx_word("clack", wd_sx(LADDER_X + 20), wd_sy(-10), f, 32, PAL_WHITE, PAL_PAINT(PAINT_CHOCOLATE, 1));
    sfx_word("creak", wd_sx(N_CX + 14), wd_sy(N_CY - 16), f, 120, PAL_WHITE, PAL_PAINT(PAINT_CARAMEL, 0));
}
void scene_p10_climb_audio(double t0, double dur) {
    for (int k = 0; k < 3; k++) {
        tk_note(AT(12 + k * 10), 0.04, 100, TKV_NOISE, 0.4f);
        tk_note(AT(12 + k * 10), 0.06, 81 - k * 2, TKV_SQ12, 0.25f);
    }
    tk_sfx(AT(42), SFX_THUD, 0.5f);
    tk_slide(AT(120), 0.5, 47, 49, TKV_SQ12, 0.22f);    /* balloon creaks toward the Wall */
}

/* ==================================================================== 11 */
/* High and wide: HaHaland waking up pastel. Lanterns go up. A poster.
 * Mascotty stands perfectly still in a crowd that never stops moving. */
void scene_p11_plaza(int f, int n) {
    float u = seg(f, 0, n);
    wd_cam(lerp(150, 230, u), -26, 1.0f);
    set_plaza(f);
    for (int i = 0; i < 16; i++) {
        uint32_t h = tk_hash((uint32_t)i * 977u + 5u);
        TkChibi c = tk_random_citizen((uint32_t)i + 30, 0);
        float x = 40 + (h % 520) + (int)((h >> 8) % 2 ? 1 : -1) * f * (0.3f + (h >> 12) % 4 * 0.12f);
        float y = 4 + (h >> 16) % 30;
        float hop = fabsf(sinf((f + (h % 40)) * 0.2f)) * 4;
        if (fabsf(x - 300) < 14) x += 28;
        tk_chibi((int)wd_sx(x), (int)(wd_sy(y) - hop), &c, 1, (h >> 4) % 3 ? TKE_HAPPY : TKE_NORMAL, TKM_SMILE, f + i * 7, (h >> 8) % 2);
        /* bit-actions: everyone has a job before the party */
        int sx = (int)wd_sx(x), sy = (int)(wd_sy(y) - hop);
        if (i == 2) {        /* carries a cupcake tray over the head */
            tk_rect(sx - 9, sy - 30, 18, 3, PAL_PAINT(PAINT_SILVER, 2));
            for (int q = 0; q < 3; q++) { tk_circle(sx - 6 + q * 6, sy - 33, 2, PAL_PAINT(PAINT_BUBBLEGUM, 2)); tk_px(sx - 6 + q * 6, sy - 36, PAL_PAINT(PAINT_CHERRY, 1)); }
        } else if (i == 4) { /* sweeping the plaza */
            int bw = (int)(4 * sinf(f * 0.4f));
            tk_line(sx + 8, sy - 16, sx + 12 + bw, sy, PAL_PAINT(PAINT_CARAMEL, 0));
            tk_rect(sx + 9 + bw, sy - 2, 7, 3, PAL_PAINT(PAINT_HONEY, 1));
        } else if (i == 7) { /* a balloon on a string */
            tk_line(sx + 6, sy - 14, sx + 8, sy - 34, PAL_GRAY_D);
            tk_ellipse(sx + 8, sy - 39, 4, 5, PAL_PAINT(PAINT_MINT, 1));
        } else if (i == 10) { /* waving hello, with a bigger hop */
            tk_text(sx - 4, sy - 34, "hi!", PAL_PAINT(PAINT_GRAPE, 0), 1);
        }
        if (i % 3 == 0) {   /* carrying a lantern */
            static const uint8_t lc[3] = { PAL_PAINT(PAINT_LEMON, 2), PAL_PAINT(PAINT_MINT, 2), PAL_PAINT(PAINT_ROSE, 2) };
            tk_circle((int)wd_sx(x) + 6, (int)(wd_sy(y) - hop) - 20, 3, lc[i % 3]);
        }
    }
    /* Mascotty is drawn in set_plaza at x 585 - bring it into frame here, still */
    {
        float x = 300;
        wd_rect(x - 4, 10, 8, 14, PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_SKY, 0));
        wd_blob(x, 0, 10, 10, 0, PAL_WHITE, WB_OUT | WB_SHADE);
        wd_blob(x - 4.5f, -18, 2.8f, 9, -0.1f, PAL_WHITE, WB_OUT | WB_SHADE);
        wd_blob(x + 4.5f, -18, 2.8f, 9, 0.1f, PAL_WHITE, WB_OUT | WB_SHADE);
        wd_blob(x - 3.5f, -2, 1.6f, 1.6f, 0, PAL_PAINT(PAINT_JET, 1), 0);
        wd_blob(x + 3.5f, -2, 1.6f, 1.6f, 0, PAL_PAINT(PAINT_JET, 1), 0);
        wd_line(x - 4, 4, x + 4, 4, 0.6f, PAL_PAINT(PAINT_JET, 1));
    }
}
void scene_p11_plaza_audio(double t0, double dur) {
    for (int k = 0; k < 4; k++) tk_slide(AT(10 + k * 25), 0.06, 96, 99, TKV_SINE, 0.08f);
}

/* ==================================================================== 12 */
/* Nana's bakery window. Two of everything. A woolly paw tucks a tea towel
 * round an egg in a basket, sets a small lamp beside it. Stays. Goes. */
void scene_p12_window(int f, int n) {
    float u = seg(f, 0, n);
    wd_cam(lerp(4, 18, u), lerp(-14, 6, u), lerp(1.25f, 3.0f, u * u * (3 - 2 * u)));
    float towel = seg(f, 40, 78);
    float lamp = f < 92 ? 0 : f < 102 ? (f - 92) / 10.0f : 1.0f + (f - 102) / 30.0f;
    if (f >= 100 && f < 108) lamp = fminf(lamp, 1.19f);
    if (f >= 108) lamp = 1.2f + (f - 108) / 30.0f;
    set_bakery(f, 0, towel, lamp, f);
    /* the paw */
    float px, py;
    float bx = 22, by = 14;
    if (f < 10) { px = 120; py = -110; }
    else if (f < 40) { float w = seg(f, 10, 36); px = lerp(120, bx - 4, w); py = lerp(-110, by - 2, w); }
    else if (f < 80) { px = bx - 4 + 6 * seg(f, 40, 78) + 1.2f * sinf(f * 0.7f); py = by - 2 + 1.0f * sinf(f * 0.9f); }
    else if (f < 100) { float w = seg(f, 80, 96); px = lerp(bx + 2, 52, w); py = lerp(by - 2, -8 - (1 - fminf(lamp, 1)) * 30, w) - 6 * sinf(w * 3.14f); }
    else if (f < 112) { px = 52; py = -6; }
    else if (f < 150) { float w = seg(f, 112, 124); px = lerp(52, bx + 10, w); py = lerp(-6, by - 6, w) + 0.6f * sinf(f * 0.15f); }
    else { float w = seg(f, 150, 176); px = lerp(bx + 10, 130, w); py = lerp(by - 6, -120, w); }
    if (f >= 92 && f < 104) { /* carrying the lamp down */
        float lx = 52, ly = 21 - (1 - fminf(lamp, 1)) * 30;
        px = lx + 2; py = ly - 22;
    }
    if (f < 180) set_paw(px, py, f);
    if (f >= 108 && f < 130) tk_sparkles((int)wd_sx(52), (int)wd_sy(8), f - 108, 3, 10);
}
void scene_p12_window_audio(double t0, double dur) {
    tk_note(AT(42), 0.15, 70, TKV_NOISE, 0.08f);        /* towel rustle */
    tk_note(AT(60), 0.15, 72, TKV_NOISE, 0.08f);
    tk_note(AT(102), 0.05, 84, TKV_SINE, 0.25f);        /* lamp set down */
    tk_note(AT(108), 0.4, 91, TKV_SINE, 0.2f);          /* lamp on */
    tk_sfx(AT(108), SFX_SPARKLE, 0.2f);
}

/* ==================================================================== 13 */
/* CLOSE ON THE EGG. A bus horn, far off: two notes. It wobbles. The horn
 * again, nearer: three notes. It wobbles harder. A heart iris closes on it
 * and holds, in silence. CRACK. Smash to black. */
void scene_p13_egg(int f, int n) {
    if (f >= 200) { tk_clear(PAL_BLACK); return; }
    /* warm lamp-lit bokeh behind */
    static const uint8_t cols[] = { PAL_RAMP(RAMP_PEACH, 4), PAL_RAMP(RAMP_PEACH, 5), PAL_RAMP(RAMP_CANDY, 6), PAL_RAMP(RAMP_PEACH, 6) };
    tk_vgrad(0, 0, TW, TH, cols, 4);
    wd_cam(0, 0, 1);
    for (int k = 0; k < 9; k++) {
        uint32_t h = tk_hash((uint32_t)k * 31u + 3u);
        float x = -120 + (h % 240) + 4 * sinf(f * 0.02f + k), y = -80 + ((h >> 8) % 90);
        tk_dither_rect((int)wd_sx(x) - 10, (int)wd_sy(y) - 10, 20, 20, k & 1 ? PAL_RAMP(RAMP_CANDY, 7) : PAL_RAMP(RAMP_PASTEL_PINK, 6), 6);
        wd_blob(x, y, 9, 9, 0, k & 1 ? PAL_RAMP(RAMP_CANDY, 7) : PAL_RAMP(RAMP_PASTEL_PINK, 6), 0);
    }
    float glow = 0.5f + 0.5f * sinf(f * 0.17f);
    tk_dither_rect(150, 0, 90, 160, PAL_RAMP(RAMP_CANDY, 7), 3 + (int)(glow * 3));
    /* wobbles */
    float rot = 0;
    rot += (f >= 30 ? 0.07f * expf(-(f - 30) * 0.05f) * sinf((f - 30) * 0.45f) : 0);
    rot += (f >= 96 ? 0.17f * expf(-(f - 96) * 0.035f) * sinf((f - 96) * 0.6f) : 0);
    rot += 0.008f * sinf(f * 0.2f);
    float sq = 1 + (f >= 96 ? 0.04f * expf(-(f - 96) * 0.05f) * sinf((f - 96) * 1.2f) : 0);
    /* basket + towel behind/around the egg */
    wd_cam(0, 0, 1);
    wd_blob(0, 70, 120, 40, 0, PAL_PAINT(PAINT_CARAMEL, 1), WB_SHADE);
    for (int k = -8; k <= 8; k++) wd_line(k * 14.0f, 40, k * 12.0f, 110, 2.0f, PAL_PAINT(PAINT_CARAMEL, 0));
    set_egg(0, 58, 40 * sq, rot, f >= 186 ? (f - 186) / 3.0f : 0, f);
    /* tea towel folds in front */
    wd_blob(-44, 52, 34, 16, -0.3f, PAL_WHITE, WB_ALL);
    wd_blob(46, 54, 32, 15, 0.35f + 0.02f * sinf(f * 0.1f), PAL_WHITE, WB_ALL);
    for (int k = 0; k < 5; k++) {
        wd_line(-70 + k * 12, 44, -66 + k * 12, 62, 1.5f, PAL_PAINT(PAINT_ROSE, 2));
        wd_line(26 + k * 11, 46, 22 + k * 11, 64, 1.5f, PAL_PAINT(PAINT_ROSE, 2));
    }
    if (f >= 96 && f < 106) tk_post_shake((f & 1) ? 1 : -1, 0);
    if (f >= 140) {
        float r = lerp(170, 46, seg(f, 140, 166));
        tk_post_iris(120, 70, r * 1.15f, 1, PAL_BLACK, PAL_PAINT(PAINT_ROSE, 1));
    }
    if (f >= 186 && f < 189) tk_post_flash(PAL_WHITE, 12);
    if (f >= 186 && f < 200) tk_post_shake((f & 1) ? 4 : -4, (f & 2) ? 2 : -2);
    if (f >= 188 && f < 200) {
        int d = f - 188;
        tk_text_outline(120 - tk_text_w("CRACK", 3) / 2 + ((f & 1) ? 2 : -2), 18 + d / 3, "CRACK", PAL_WHITE, PAL_BLACK, 3);
    }
}
void scene_p13_egg_audio(double t0, double dur) {
    /* far-off bus horn: two notes, quiet */
    tk_note(AT(18), 0.22, 64, TKV_SAW, 0.12f); tk_note(AT(18), 0.22, 52, TKV_SQ50, 0.08f);
    tk_note(AT(26), 0.3, 60, TKV_SAW, 0.12f); tk_note(AT(26), 0.3, 48, TKV_SQ50, 0.08f);
    tk_slide(AT(30), 0.1, 70, 73, TKV_SQ12, 0.1f);   /* wobble */
    /* nearer: three notes */
    tk_note(AT(84), 0.2, 64, TKV_SAW, 0.36f); tk_note(AT(84), 0.2, 52, TKV_SQ50, 0.22f);
    tk_note(AT(91), 0.2, 60, TKV_SAW, 0.36f); tk_note(AT(91), 0.2, 48, TKV_SQ50, 0.22f);
    tk_note(AT(98), 0.34, 64, TKV_SAW, 0.4f); tk_note(AT(98), 0.34, 52, TKV_SQ50, 0.25f);
    for (int k = 0; k < 4; k++) tk_note(AT(97 + k * 5), 0.04, 84, TKV_NOISE, 0.18f);   /* rattle */
    tk_sfx(AT(140), SFX_WHOOSH, 0.2f);
    /* CRACK */
    tk_note(AT(186), 0.08, 118, TKV_NOISE, 1.0f);
    tk_note(AT(187), 0.3, 100, TKV_NOISE, 0.6f);
    tk_kick(AT(186), 1.0f);
    tk_snare(AT(186), 0.8f);
}
