/* sc21.c - SC21 (1-08) EXT. BANDSTAND (NORTH HALF) - DUSK. The Mayor's
 * welcome: spotlight on Sunny, the patter, the missing speech, page one, the
 * creep toward the boards, "...the weather side.", the deadpan, Hee. Hee.,
 * and Bonejangles' fanfare. Lines placed by ID (show.h); takes go through
 * the dialogue chain in assemble_scene.sh. Shots breathe; energy inside. */
#include "tk.h"
#include "wd.h"
#include "sets.h"
#include "mayor.h"
#include "cast.h"
#include "show.h"
#include <math.h>
#include <string.h>

#define SHOTS(X) \
    X(a_spot,     4.5) \
    X(b_welcome,  7.5) \
    X(c_newface,  4.0) \
    X(d_speech,   8.5) \
    X(e_pageone,  7.0) \
    X(f_twosides, 7.0) \
    X(g_weather,  2.5) \
    X(h_deadpan,  4.6) \
    X(i_clack,    2.6)
#include "scene_tl.h"

#define AT(fr) (t0 + (fr) / 30.0)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }
static float spring(float f, float f0, float amp) { return wd_spring(f, f0, amp, 0.55f, 0.11f); }

/* the lines, from lines.csv (the text the takes were recorded from) */
#define L01 "a1_08_mayor_01"
#define L10 "a1_08_mayor_10"
#define L02 "a1_08_mayor_02"
#define L03 "a1_08_mayor_03"
#define L04 "a1_08_mayor_04"
#define L05 "a1_08_mayor_05"
#define L06 "a1_08_mayor_06"
#define L07 "a1_08_mayor_07"
#define L08 "a1_08_mayor_08"
#define L09 "a1_08_mayor_09"
#define LB  "a1_08_bone_01"
static const char *T01 = "Welcome, welcome! Mayor Gigglesworth. Friends call me Giggs. Enemies don't exist!";
static const char *T10 = "Ooh, a new face! We get so few.";
static const char *T02 = "My speech. I've given it four hundred times.";
static const char *T03 = "I can't remember a WORD.";
static const char *T04 = "Page one! Manners! We'll keep you.";
static const char *T05 = "The SPEECH is at eight!";
static const char *T06 = "Hellvile Park has two sides. This one, and...";
static const char *T07 = "...the weather side.";
static const char *T08 = "We don't talk about the other side.";
static const char *T09 = "Hee. Hee.";
static const char *TB  = "Clack clack!";

/* a line on screen: bubble from the speaker, typed while he talks */
static void say(const char *id, const char *text, int f, int cue, int hold, float hx, float hy) {
    if (f < cue || f >= cue + hold) return;
    bubble(text, (int)((f - cue) * 0.75f) + 1, (int)wd_sx(hx), (int)wd_sy(hy));
    (void)id;
}

/* ---- the world: north half at dusk, lanterns lit, the balloon on the banner */
static void world(float t) {
    set_sky_dawn(t, 0.25f);
    set_wall(t);
    Banner b = { 0 };
    set_bandstand(t, &b, 0);
    set_balloon(BAL_X, N_CY - 2, -0.15f, 1 + 0.02f * sinf(t * 0.2f), 0.02f * sinf(t * 0.1f), BAL_X + 2.5f, BAN_Y1 - 1, t);
    for (int i = 0; i < 8; i++) {                 /* lantern string across the north half */
        float x = -88 + i * 12, y = -73 + 2.0f * sinf(i * 0.9f);
        static const uint8_t lc[3] = { PAL_PAINT(PAINT_LEMON, 2), PAL_PAINT(PAINT_MINT, 2), PAL_PAINT(PAINT_ROSE, 2) };
        wd_blob_ol(x, y + 3, 2.2f, 2.8f, 0, lc[i % 3], PAL_PAINT(PAINT_CARAMEL, 0), WB_SHADE);
    }
}
/* the Giggle crowd, in front of the stage; `turn` makes them look at Sunny */
static void crowd(float t, float turn, float cheer) {
    for (int i = 0; i < 14; i++) {
        uint32_t h = tk_hash((uint32_t)i * 977u + 11u);
        TkChibi c = tk_random_citizen((uint32_t)i + 50, 0);
        float x = -100 + i * 15.5f + (h % 7), y = 14 + (h >> 8) % 22;
        if (fabsf(x - 38) < 9 && y > 24) x += 18;
        float hop = cheer * fabsf(sinf((t + (h % 30)) * 0.35f)) * 5 + fabsf(sinf((t + (h % 40)) * 0.08f)) * 1.0f;
        int flip = turn > 0.5f ? (x > 38) : (h >> 3) % 2;
        int sz = wd_z > 2.2f ? 2 : 1;
        tk_chibi((int)wd_sx(x), (int)(wd_sy(y) - hop * wd_z), &c, sz, cheer > 0.3f ? TKE_HAPPY : TKE_NORMAL, cheer > 0.3f ? TKM_OPEN : TKM_SMILE, (int)t + i * 7, flip);
    }
}
static void sunny_at(float t, float page, float look_x, float ears) {
    Sunny u; sunny_default(&u, 38, 30, 0.9f, t);
    u.page = page; u.look_x = look_x; u.ears = ears;
    u.blink = fmodf(t, 97) > 93;
    sunny_draw(&u);
}
static void bones_at(float t, float hit) {
    Bones b; bones_default(&b, -82, -10, 0.9f, t);
    b.hitL = hit; b.hitR = hit > 0 ? 1 - hit : 0;
    bones_draw(&b);
}
/* a lightweight spotlight: everything outside the circle dims */
static void spotlight(float cx, float cy, float r) {
    static uint8_t keep[TH][TW];
    memcpy(keep, tk_fb, sizeof keep);
    tk_post_darken(10);
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float dx = x - cx, dy = (y - cy) * 1.3f;
            if (dx * dx + dy * dy < r * r) tk_fb[y][x] = keep[y][x];
        }
}
static void mayor_base(Mayor *m, float f, float x, float y) {
    mayor_default(m, x, y, 1.0f, f);
    m->flutter = 0.25f;
    m->blink = fmodf(f, 83) > 79;
}
static void clipboard(float hx, float hy, float flipup, int pages) {
    float w = 9, h = 12;
    float y0 = hy - h + 2 - flipup * 3;
    wd_rect(hx - w / 2, y0, w, h, PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CHOCOLATE, 1));
    if (pages) { wd_rect(hx - 3.5f, y0 + 2, 7, 9, PAL_WHITE, PAL_PAINT(PAINT_SKY, 1)); for (int k = 0; k < 3; k++) wd_line(hx - 2.5f, y0 + 4 + k * 2, hx + 2.5f, y0 + 4 + k * 2, 0.4f, PAL_PAINT(PAINT_SKY, 1)); }
    wd_rect(hx - 2.5f, y0 - 1, 5, 2.2f, PAL_GRAY_L, PAL_GRAY_D);
}

/* ================================================================== A */
/* A spotlight sweeps the crowd, overshoots, swings back, lands on Sunny. */
void scene_a_spot(int f, int n) {
    wd_cam(lerp(-6, 4, seg(f, 0, n)), -30, 1.12f);
    world(f);
    bones_at(f, 0);
    float turn = seg(f, 96, 110);
    crowd(f, turn, 0);
    sunny_at(f, 0, turn * 0.8f, 1 - 0.3f * seg(f, 100, 120));
    /* path: left to right, overshoot past Sunny, swing back and settle */
    float u = seg(f, 6, 70), back = seg(f, 70, 92);
    float sx = lerp(20, 210, u), tx = wd_sx(38), ty = wd_sy(16);
    sx = lerp(sx, tx, back) + (f > 92 ? 3 * expf(-(f - 92) * 0.15f) * sinf((f - 92) * 0.9f) : 0);
    float sy = lerp(wd_sy(26), ty, back);
    spotlight(sx, sy, 22 + 4 * seg(f, 92, 100));
    if (f >= 100 && f < 124) tk_text_outline((int)tx - 6, (int)ty - 30, "!", PAL_WHITE, PAL_PAINT(PAINT_NAVY, 1), 2);
}
void scene_a_spot_audio(double t0, double dur) {
    tk_sfx(AT(4), SFX_WHOOSH, 0.35f);
    tk_sfx(AT(70), SFX_WHOOSH, 0.3f);
    tk_note(AT(92), 0.05, 50, TKV_NOISE, 0.3f);            /* clunk: the light lands */
    for (int k = 0; k < 8; k++) tk_slide(AT(100 + k * 2), 0.08, 60 + k, 63 + k, TKV_SQ12, 0.05f);   /* the crowd turns */
}

/* ================================================================== B */
/* MAYOR GIGGS flaps up onto the rail, wings wide: the curtain call. */
#define B_CUE 30
void scene_b_welcome(int f, int n) {
    float up = seg(f, 0, 22);
    wd_cam(lerp(-24, -20, seg(f, 0, n)), lerp(-32, -36, seg(f, 0, n)), lerp(1.9f, 2.15f, seg(f, 0, n)));
    world(f);
    bones_at(f, 0);
    Mayor m; mayor_base(&m, f, -32, lerp(14, -10, up) - 8 * sinf(up * 3.14f));
    m.flutter = f < 26 ? 1.0f : 0.3f;
    m.fan = f < 26 ? 1.0f : 0.5f + 0.2f * sinf(f * 0.1f);
    m.armL = m.armR = f < 26 ? 2.2f + 0.8f * sinf(f * 1.3f) : 2.4f + 0.15f * sinf(f * 0.2f);   /* arms wide */
    m.squash = 1 + spring(f, 22, -0.25f);
    m.expr = MX_HAPPY;
    m.mouth = lip(L01, f - B_CUE, 170);
    m.head_dy = 0.6f * sinf(f * 0.3f) * (f > B_CUE);
    mayor_draw(&m);
    crowd(f, 0, seg(f, 200, 206));
    say(L01, T01, f, B_CUE, 190, mayor_head_x + 7, mayor_head_y + 2);
}
void scene_b_welcome_audio(double t0, double dur) {
    for (int k = 0; k < 6; k++) tk_slide(AT(k * 4), 0.06, 60, 64, TKV_NOISE, 0.12f);   /* flaps */
    tk_sfx(AT(22), SFX_BOING, 0.4f);
    line_at(L01, AT(B_CUE));
    tk_sfx(AT(200), SFX_FANFARE, 0.15f);            /* the crowd cheers */
    for (int k = 0; k < 10; k++) tk_slide(AT(200 + k * 1.5), 0.12, 72 + (k * 5) % 12, 79 + (k * 3) % 9, TKV_SQ25, 0.05f);
}

/* ================================================================== C */
/* He leans in toward Sunny, delighted. */
#define C_CUE 8
void scene_c_newface(int f, int n) {
    float push = seg(f, 0, n);
    wd_cam(lerp(-16, -14, push), lerp(-38, -40, push), lerp(2.8f, 3.2f, push));
    world(f);
    Mayor m; mayor_base(&m, f, -32, -10);
    m.lean = 0.12f + spring(f, 2, 0.08f);
    m.flip = 1;
    m.expr = MX_HAPPY; m.eye_pop = 0.3f + 0.2f * seg(f, 0, 8);
    m.look_x = 1; m.look_y = 0.6f;
    m.fan = 0.7f;
    m.armL = 0.6f; m.armR = 1.2f + 0.2f * sinf(f * 0.2f);
    m.mouth = lip(L10, f - C_CUE, 90);
    mayor_draw(&m);
    say(L10, T10, f, C_CUE, 110, mayor_head_x + 7, mayor_head_y + 2);
}
void scene_c_newface_audio(double t0, double dur) { line_at(L10, AT(C_CUE)); tk_sfx(AT(2), SFX_SPARKLE, 0.15f); }

/* ================================================================== D */
/* The speech: a flourish, the clipboard flips up... the clip, and air. */
#define D_CUE1 10
#define D_CUE2 162
void scene_d_speech(int f, int n) {
    wd_cam(-18, lerp(-34, -37, seg(f, 0, n)), lerp(2.4f, 2.7f, seg(f, 100, n)));
    world(f);
    Mayor m; mayor_base(&m, f, -32, -10);
    float flourish = seg(f, 0, 14), flip = seg(f, 110, 118);
    m.armR = lerp(0.6f, 2.6f, flourish) - 1.1f * seg(f, 16, 26);        /* sweep up, then hold it out */
    m.armL = 0.5f;
    m.fan = 0.6f;
    m.expr = f < 120 ? MX_HAPPY : MX_WORRY;          /* the smile stays; the eyes don't */
    if (f >= 120) { m.eye_pop = 0.4f; m.look_x = 0.9f; m.look_y = 0.3f; m.wattle += 0.03f * sinf(f * 1.7f); }
    if (f >= 126 && f < 130) m.blink = 1;
    m.mouth = lip(L02, f - D_CUE1, 140) + lip(L03, f - D_CUE2, 60);
    mayor_draw(&m);
    clipboard(mayor_hand_x[1] + 3, mayor_hand_y[1] - 1, flip, 0);
    if (f >= 118 && f < 150) {                        /* air */
        float a = seg(f, 118, 150);
        tk_text_outline((int)wd_sx(mayor_hand_x[1] + 8), (int)wd_sy(mayor_hand_y[1] - 14) - (int)(a * 6), "...", PAL_WHITE, PAL_PAINT(PAINT_NAVY, 1), 1);
    }
    say(L02, T02, f, D_CUE1, 148, mayor_head_x + 7, mayor_head_y + 2);
    say(L03, T03, f, D_CUE2, 92, mayor_head_x + 7, mayor_head_y + 2);
}
void scene_d_speech_audio(double t0, double dur) {
    tk_sfx(AT(2), SFX_WHOOSH, 0.3f);
    line_at(L02, AT(D_CUE1));
    tk_note(AT(112), 0.04, 70, TKV_NOISE, 0.3f);      /* the clipboard flips: flap */
    tk_slide(AT(120), 0.5, 72, 66, TKV_SINE, 0.12f);  /* a sad little whistle of air */
    line_at(L03, AT(D_CUE2));
}

/* ================================================================== E */
/* Sunny holds up page one. It drips. "Page one! Manners! We'll keep you."
 * He takes it in two fingers, clips it on, spins to the crowd. */
#define E_CUE1 24
#define E_CUE2 128
void scene_e_pageone(int f, int n) {
    float push = seg(f, 0, 90);
    wd_cam(lerp(6, 2, push), lerp(-16, -18, push), lerp(2.0f, 2.2f, push));
    world(f);
    float raise = seg(f, 0, 14);
    float taken = seg(f, 100, 108);
    Mayor m; mayor_base(&m, f, -26, -10);
    float spin = seg(f, 110, 124);
    m.flip = spin > 0 && spin < 1 ? (spin < 0.5f ? -1 : 1) : 1;
    m.expr = MX_HAPPY; m.eye_pop = f < 60 ? 0.6f : 0.2f; m.fan = f >= E_CUE1 && f < 70 ? 1.0f : 0.5f;
    m.squash = 1 + spring(f, E_CUE1, 0.12f) + spring(f, 124, -0.1f);
    m.look_x = f < 110 ? 1 : -0.4f; m.look_y = f < 110 ? 0.8f : 0;
    m.armR = f < 96 ? 1.0f : f < 110 ? lerp(1.0f, 1.6f, seg(f, 96, 104)) : 1.2f;
    m.armL = f >= E_CUE2 ? 2.5f + 0.2f * sinf(f * 0.3f) : 0.5f;
    m.mouth = lip(L04, f - E_CUE1, 80) + lip(L05, f - E_CUE2, 66);
    mayor_draw(&m);
    clipboard(mayor_hand_x[1] + 3 * m.flip, mayor_hand_y[1] - 1, 0, taken >= 1);
    crowd(f, 0.6f, seg(f, 196, 202));
    sunny_at(f, taken < 1 ? fmaxf(0.01f, raise * (1 - taken)) : 0, -0.6f, 0.8f + 0.2f * raise);
    say(L04, T04, f, E_CUE1, 86, mayor_head_x + 7, mayor_head_y + 2);
    say(L05, T05, f, E_CUE2, 70, mayor_head_x + 7, mayor_head_y + 2);
}
void scene_e_pageone_audio(double t0, double dur) {
    for (int k = 0; k < 3; k++) tk_note(AT(10 + k * 9), 0.05, 88, TKV_SINE, 0.1f);   /* drip */
    line_at(L04, AT(E_CUE1));
    tk_note(AT(104), 0.05, 80, TKV_NOISE, 0.15f);     /* two fingers: pinch */
    tk_note(AT(108), 0.03, 90, TKV_SQ12, 0.2f);       /* clip */
    tk_sfx(AT(112), SFX_WHOOSH, 0.3f);                /* spin */
    line_at(L05, AT(E_CUE2));
    tk_sfx(AT(196), SFX_FANFARE, 0.12f);
}

/* ================================================================== F */
/* Sunny's eyes drift to the boarded half; the camera creeps after them, one
 * pixel at a time. The Mayor steps into the lens, smile filling the frame. */
#define F_CUE 104
void scene_f_twosides(int f, int n) {
    float creep = seg(f, 0, 90), step = seg(f, 90, 102);
    float cx = lerp(lerp(-14, 26, creep), -17, step), cy = lerp(lerp(-36, -40, creep), -41, step);
    float z = lerp(1.7f, 4.0f, step);
    wd_cam(cx, cy, z);
    world(f);
    bones_at(f, 0);
    Mayor m; mayor_base(&m, f, lerp(-60, -32, step), -10);
    m.expr = MX_HAPPY; m.look_x = 0; m.look_y = 0.2f;   /* straight into the lens */
    m.squash = 1 + spring(f, 102, 0.08f);
    m.armR = 0.4f; m.armL = 0.4f; m.fan = 0.5f;
    m.mouth = lip(L06, f - F_CUE, 100);
    mayor_draw(&m);
    if (step < 0.5f) crowd(f, 0, 0);
    say(L06, T06, f, F_CUE, 106, mayor_head_x + 7, mayor_head_y + 2);
}
void scene_f_twosides_audio(double t0, double dur) {
    for (int k = 0; k < 3; k++) tk_note(AT(15 + k * 25), 0.05, 40, TKV_TRI, 0.1f);   /* step, step, step */
    tk_sfx(AT(90), SFX_WHOOSH, 0.35f);
    line_at(L06, AT(F_CUE));
}

/* ================================================================== G */
/* [FREEZE 13] Tapestop. Wings stop. Then, too smoothly: "...the weather side." */
#define G_CUE 16
void scene_g_weather(int f, int n) {
    float fz = f < 13 ? 0 : f - 13;               /* the clock stops for 13 frames */
    wd_cam(-17, -41, 4.0f);
    world(209);
    Mayor m; mayor_base(&m, 209 + fz, -32, -10);
    m.flutter = 0; m.fan = 0.5f; m.armR = m.armL = 0.4f;
    m.expr = MX_HAPPY; m.look_y = 0.2f;
    if (f >= 13) { m.head_tilt = 0.06f * sinf(fz * 0.3f) * expf(-fz * 0.05f); m.look_x = 0.4f * sinf(fz * 0.2f); }
    m.mouth = f < 13 ? 0.2f : lip(L07, f - G_CUE, 40);
    mayor_draw(&m);
    say(L07, T07, f, G_CUE, 59, mayor_head_x + 7, mayor_head_y + 2);
}
void scene_g_weather_audio(double t0, double dur) { line_at(L07, AT(G_CUE)); }

/* ================================================================== H */
/* {d} The deadpan. [HOLD 13]. Then, late, the fountain giggles. "Hee. Hee." */
#define H_CUE1 6
#define H_CUE2 98
void scene_h_deadpan(int f, int n) {
    wd_cam(-17, -41, lerp(4.0f, 4.3f, seg(f, 0, 60)));
    world(f);
    Mayor m; mayor_base(&m, 300 + f, -32, -10);
    m.flutter = 0; m.fan = 0.2f; m.armR = m.armL = 0.3f;
    m.expr = f < H_CUE2 ? MX_NEUTRAL : MX_HAPPY;      /* flat, no wink: he means it */
    m.blink = f >= 66 && f < 70;
    if (f >= 84 && f < H_CUE2) { m.look_x = -0.9f; m.eye_pop = 0.2f; }   /* the fountain */
    m.mouth = lip(L08, f - H_CUE1, 50) + lip(L09, f - H_CUE2, 36);
    m.head_dy = f >= H_CUE2 ? 0.6f * seg(f, H_CUE2, H_CUE2 + 30) : 0;
    mayor_draw(&m);
    if (f >= 84 && f < 96) tk_text_outline(6, 70, "hee", PAL_WHITE, PAL_PAINT(PAINT_SKY, 0), 1);
    say(L08, T08, f, H_CUE1, 64, mayor_head_x + 7, mayor_head_y + 2);
    say(L09, T09, f, H_CUE2, 40, mayor_head_x + 7, mayor_head_y + 2);
}
void scene_h_deadpan_audio(double t0, double dur) {
    line_at(L08, AT(H_CUE1));
    for (int k = 0; k < 2; k++) {                 /* far off at the gate, late */
        tk_slide(AT(84 + k * 7), 0.08, 84, 89, TKV_SQ12, 0.06f);
        tk_slide(AT(85 + k * 7), 0.08, 72, 77, TKV_TRI, 0.05f);
    }
    line_at(L09, AT(H_CUE2));
}

/* ================================================================== I */
/* Bonejangles slams a fanfare down his ribs: "Clack clack!" Music crashes back. */
#define I_CUE 6
void scene_i_clack(int f, int n) {
    wd_cam(-64, -34, lerp(2.6f, 2.4f, seg(f, 0, n)));
    world(f);
    Bones b; bones_default(&b, -82, -10, 1.0f, f);
    b.hitL = f >= 4 && f < 9 ? 1 : 0; b.hitR = f >= 10 && f < 15 ? 1 : 0;
    if (f >= 16) { b.hitL = fabsf(sinf(f * 0.5f)); b.hitR = 1 - b.hitL; }
    b.jaw = lip(LB, f - I_CUE, 20);
    b.squash = 1 + spring(f, 5, 0.1f);
    bones_draw(&b);
    Mayor m; mayor_base(&m, f, -32, -10);
    m.expr = MX_HAPPY; m.fan = 0.9f; m.armL = m.armR = 2.4f + 0.2f * sinf(f * 0.4f);
    mayor_draw(&m);
    crowd(f, 0, f >= 20 ? 1.0f : 0);
    if (f >= 20) tk_confetti(120, 20, f - 20, 7);
    if (f >= 4 && f < 18) tk_text_outline((int)wd_sx(-82) - 22, (int)wd_sy(-48), "CLACK", PAL_WHITE, PAL_PAINT(PAINT_CHERRY, 0), 1);
    say(LB, TB, f, I_CUE, 26, -82, -50);
}
void scene_i_clack_audio(double t0, double dur) {
    tk_note(AT(4), 0.12, 84, TKV_SQ50, 0.3f); tk_note(AT(10), 0.12, 91, TKV_SQ50, 0.3f);   /* two crisp rib hits */
    line_at(LB, AT(I_CUE));
    tk_sfx(AT(20), SFX_FANFARE, 0.3f);
}

/* ---- MUSIC: "HaHaland Day", live from the bandstand. Tapestop at the
 * freeze; silence through the deadpan; crashes back on Bonejangles' downbeat. */
static void hahaland(double t0, double t1, float vol) {
    static const int mel[16] = { 72, 76, 79, 76, 77, 81, 79, 0, 72, 76, 79, 84, 83, 79, 77, 76 };
    static const int bass[4] = { 48, 53, 55, 48 };
    const double beat = 0.4;
    for (int i = 0; ; i++) {
        double t = t0 + i * beat / 2;
        if (t >= t1) break;
        int m = mel[i % 16];
        if (m) tk_note(t, beat * 0.4, m, TKV_SQ25, vol * 0.5f);
        if (i % 2 == 0) { tk_note(t, beat * 0.8, bass[(i / 8) % 4], TKV_TRI, vol * 0.7f); }
        if (i % 4 == 0) tk_kick(t, vol * 0.4f);
        if (i % 4 == 2) tk_snare(t, vol * 0.3f);
        tk_hat(t, vol * 0.15f);
    }
}
void tk_music(void) {
    double g = tk_scene_start("g_weather");
    hahaland(0.0, g, 0.32f);
    /* TAPESTOP: the band slides down and dies */
    tk_slide(g, 0.6, 72, 48, TKV_SQ25, 0.2f);
    tk_slide(g, 0.6, 48, 30, TKV_TRI, 0.25f);
    double i0 = tk_scene_start("i_clack") + 20 / 30.0;
    hahaland(i0, tk_total_seconds() + 0.6, 0.42f);
}
