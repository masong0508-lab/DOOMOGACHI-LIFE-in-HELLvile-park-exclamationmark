/* sc22.c - SC22 (1-09) EXT. BAKE TENT - NIGHT (19:00). The bake-off: the
 * Mayor's A-plus and the blank card; the key in a muffin and the bite test;
 * the x-ray truth insert; page two under the pies; the egg; "...Mostly."
 * Mayor lines use his recordings (line_at). Nana has no recordings yet, so
 * her lines are bubbles with synth babble (no line_at). */
#include "tk.h"
#include "wd.h"
#include "sets.h"
#include "mayor.h"
#include "cast.h"
#include "show.h"
#include <math.h>
#include <string.h>

#define SHOTS(X) \
    X(a_card,     1.2) \
    X(b_aplus,    6.0) \
    X(c_second,   5.0) \
    X(d_muffins,  8.0) \
    X(e_bites,    6.5) \
    X(f_xray,     0.1334) \
    X(g_key,      4.0) \
    X(h_pagetwo,  7.0) \
    X(i_egg,      8.0) \
    X(j_mostly,   6.5)
#include "scene_tl.h"

#define AT(fr) (t0 + (fr) / 30.0)
#define INK PAL_PAINT(PAINT_JET, 1)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }
static float spring(float f, float f0, float amp) { return wd_spring(f, f0, amp, 0.55f, 0.11f); }

/* lines (lines.csv text; Mayor = recorded takes, Nana = bubbles + babble) */
typedef struct { const char *id, *text; float secs; } Line;
static const Line M02 = { "a1_09_mayor_02", "Mm! A-plus!", 1.9f };
static const Line M01 = { "a1_09_mayor_01", "Who's the second one for?", 2.0f };
static const Line N01 = { "a1_09_nana_01", "Habit, sweet pea.", 1.4f };
static const Line N02 = { "a1_09_nana_02", "The key's in a muffin. Don't ask which. I forgot. I always forget.", 5.9f };
static const Line N03 = { "a1_09_nana_03", "Raisin.", 0.5f };
static const Line N04 = { "a1_09_nana_04", "Raisin.", 0.5f };
static const Line N05 = { "a1_09_nana_05", "...That's a button.", 1.2f };
static const Line N10 = { "a1_09_nana_10", "There it is! Mind your teeth.", 2.5f };
static const Line N06 = { "a1_09_nana_06", "Is this yours? It was under the pies. It's mostly \"Hee. Hee.\"", 5.5f };
static const Line N11 = { "a1_09_nana_11", "And this. An egg.", 2.0f };
static const Line N07 = { "a1_09_nana_07", "It's been waiting for someone.", 2.5f };
static const Line N08 = { "a1_09_nana_08", "The last one hatched into a little demon! Ha! Kidding.", 3.8f };
static const Line N09 = { "a1_09_nana_09", "...Mostly.", 0.8f };

static int hold_of(const Line *l) { return (int)(l->secs * 30) + 14; }
static void say(const Line *l, int f, int cue, float hx, float hy) {
    if (f < cue || f >= cue + hold_of(l)) return;
    bubble(l->text, (int)((f - cue) * 0.75f) + 1, (int)wd_sx(hx), (int)wd_sy(hy));
}
static float mouth(const Line *l, int f, int cue) { return lip(l->id, f - cue, (int)(l->secs * 30)); }
/* Nana's babble: soft, warm, a little sing-song */
static void babble(double t, const Line *l, float pitch) {
    int n = (int)(l->secs * 7.5f); if (n < 2) n = 2;
    for (int k = 0; k < n; k++) {
        uint32_t h = tk_hash((uint32_t)k * 31u + (uint32_t)(l->secs * 100));
        int m = (int)pitch + (int)(h % 5) - 2 + (k == n - 1 ? -3 : 0);
        tk_note(t + k * l->secs / n, 0.07, m, TKV_TRI, 0.22f);
        tk_note(t + k * l->secs / n, 0.05, m + 12, TKV_SQ12, 0.05f);
    }
}

/* ---- the set: the bake tent at night ------------------------------------ */
static void tent(float t) {
    static const uint8_t cols[] = { PAL_PAINT(PAINT_NAVY, 1), PAL_PAINT(PAINT_GRAPE, 1), PAL_PAINT(PAINT_GRAPE, 2) };
    tk_vgrad(0, 0, TW, TH, cols, 3);
    /* striped canvas walls and a scalloped valance, lit warm from inside */
    for (int i = -12; i < 12; i++) wd_rect(i * 16.0f, -110, 16, 110, i & 1 ? PAL_PAINT(PAINT_MINT, 2) : PAL_PAINT(PAINT_BONE, 2), 0);
    for (int i = -12; i < 12; i++) wd_line(i * 16.0f, -110, i * 16.0f, 0, 0.5f, PAL_PAINT(PAINT_MINT, 1));
    for (int i = -24; i < 24; i++) wd_blob_ol(i * 8.0f + 4, -104, 4.4f, 3.4f, 0, i & 1 ? PAL_PAINT(PAINT_MINT, 1) : PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_MINT, 0), 0);
    for (int i = 0; i < 9; i++) {                          /* festoon bulbs */
        float x = -96 + i * 24, y = -92 + 4 * sinf(i * 1.3f);
        wd_line(x, y - 4, x + 24, -92 + 4 * sinf((i + 1) * 1.3f) - 4, 0.4f, PAL_PAINT(PAINT_CHOCOLATE, 1));
        wd_blob_ol(x, y, 1.8f, 2.2f, 0, PAL_PAINT(PAINT_LEMON, 2), PAL_PAINT(PAINT_HONEY, 1), WB_SHADE);
    }
    /* bunting sign */
    wd_rect(-30, -86, 60, 12, PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_ROSE, 0));
    wd_text(-25.5f, -83, "BAKE-OFF", 1.0f, PAL_PAINT(PAINT_MAGENTA, 0), 0);
    /* floor: grass mat */
    wd_rect(-200, 0, 400, 80, PAL_RAMP(RAMP_GRASS, 4), 0);
    /* long tables of entries, far */
    for (int k = -1; k <= 1; k += 2) {
        wd_rect(k * 70 - 26, -26, 52, 4, PAL_PAINT(PAINT_BONE, 2), PAL_PAINT(PAINT_CARAMEL, 0));
        for (int q = 0; q < 4; q++) wd_blob_ol(k * 70 - 18 + q * 12, -29, 4, 3, 0, q & 1 ? PAL_PAINT(PAINT_CARAMEL, 2) : PAL_PAINT(PAINT_BUBBLEGUM, 2), INK, 0);
    }
}
/* Nana's stall: two of everything; pies with name cards; a dozen muffins */
static void loaf(float x, float y) {
    wd_blob_ol(x, y, 6, 3.6f, 0, PAL_PAINT(PAINT_CARAMEL, 1), INK, WB_SHADE);
    for (int k = -1; k <= 1; k++) wd_line(x + k * 2.6f - 1, y - 2, x + k * 2.6f + 1, y - 3, 0.5f, PAL_PAINT(PAINT_CARAMEL, 2));
}
static void cake(float x, float y) {
    wd_rect(x - 5, y - 6, 10, 6, PAL_PAINT(PAINT_BUBBLEGUM, 2), INK);
    wd_rect(x - 5, y - 4, 10, 1, PAL_PAINT(PAINT_CHERRY, 1), 0);
    wd_blob(x, y - 7, 1.2f, 1.2f, 0, PAL_PAINT(PAINT_CHERRY, 1), WB_OUT);
}
void pie(float x, float y, float lift) {
    y -= lift;
    wd_blob_ol(x, y, 8, 2.8f, 0, PAL_PAINT(PAINT_CARAMEL, 1), INK, WB_SHADE);
    for (int k = -2; k <= 2; k++) wd_line(x + k * 2.8f, y - 2.2f, x + k * 2.8f + 0.6f, y + 0.6f, 0.5f, PAL_PAINT(PAINT_CARAMEL, 2));
    wd_blob(x, y - 0.6f, 6.4f, 0.8f, 0, PAL_PAINT(PAINT_CHERRY, 1), 0);
}
static void namecard(float x, float y, const char *name, int stamp) {
    wd_rect(x - 8, y - 5, 16, 5, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0));
    if (name) wd_text(x - 5.9f, y - 4, name, 0.5f, PAL_PAINT(PAINT_NAVY, 1), 0);
    if (stamp) { wd_blob_ol(x + 9, y - 4, 2.6f, 2.6f, -0.2f, PAL_PAINT(PAINT_CHERRY, 2), PAL_PAINT(PAINT_CHERRY, 0), 0); wd_text(x + 7.2f, y - 5.2f, "A+", 0.4f, PAL_WHITE, 0); }
}
static void muffin(float x, float y, int eyes, float t) {
    wd_quad(x - 2.4f, y - 2, x + 2.4f, y - 2, x + 1.9f, y + 1.5f, x - 1.9f, y + 1.5f, PAL_PAINT(PAINT_SKY, 2), INK);
    wd_blob_ol(x, y - 2.6f, 2.9f, 1.9f, 0, PAL_PAINT(PAINT_CARAMEL, 1), INK, WB_SHADE);
    wd_blob(x - 0.8f, y - 3, 0.4f, 0.4f, 0, PAL_PAINT(PAINT_CHOCOLATE, 1), 0);
    wd_blob(x + 1.0f, y - 2.4f, 0.4f, 0.4f, 0, PAL_PAINT(PAINT_CHOCOLATE, 1), 0);
    if (eyes) { float lx = 0.3f * sinf(t * 0.05f); wd_blob(x - 0.8f + lx, y - 0.2f, 0.45f, 0.55f, 0, INK, 0); wd_blob(x + 0.8f + lx, y - 0.2f, 0.45f, 0.55f, 0, INK, 0); }
}
static int eaten;   /* muffins gone from the tray, for the bite test */
static void stall(float t, int stamp, float pies_lift, int show_page, int muffin_eyes) {
    wd_rect(-60, -10, 120, 10, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0));           /* counter with gingham */
    for (int k = 0; k < 15; k++) wd_rect(-60 + k * 8, -10, 4, 10, PAL_PAINT(PAINT_ROSE, 2), 0);
    wd_rect(-62, -12, 124, 2.4f, PAL_PAINT(PAINT_CARAMEL, 1), INK);
    loaf(-50, -15); loaf(-37, -15);
    cake(-24, -12); cake(-12, -12);
    if (show_page) {      /* page two, flattened under where the pies sat */
        wd_quad(0, -13.4f, 30, -13.4f, 30, -12.4f, 0, -12.4f, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0));
    }
    pie(6, -14.5f, pies_lift); pie(24, -14.5f, pies_lift);
    namecard(6, -9.5f, "NANA", stamp); namecard(24, -9.5f, NULL, 0);
    /* the muffin tray: a dozen identical muffins */
    wd_rect(36, -14, 24, 3, PAL_GRAY_L, PAL_GRAY_D);
    for (int k = 0; k < 12; k++) if (k >= eaten) muffin(38.5f + (k % 6) * 3.9f, -16.4f - (k / 6) * 3.2f, muffin_eyes, t + k * 9);
}
static void nana_at(Nana *n, float t) {
    nana_default(n, 4, -12, 1.0f, t);    /* behind the counter */
    n->blink = fmodf(t, 101) > 97;
}
static void mayor_judge(Mayor *m, float t, float x) {
    mayor_default(m, x, 0, 1.0f, t);
    m->flutter = 0.2f;
    m->blink = fmodf(t, 83) > 79;
}
static void rosette(void) {           /* JUDGE rosette on the bow tie */
    float x = mayor_head_x + 2, y = mayor_head_y + 11;
    for (int k = 0; k < 8; k++) { float a = k * 0.785f; wd_blob(x + cosf(a) * 2, y + sinf(a) * 2, 1.3f, 1.3f, 0, PAL_PAINT(PAINT_SKY, 1), 0); }
    wd_blob_ol(x, y, 1.6f, 1.6f, 0, PAL_UI_GOLD, INK, 0);
    wd_quad(x - 1.4f, y + 1, x, y + 1, x - 1, y + 5, x - 2.4f, y + 4.4f, PAL_PAINT(PAINT_SKY, 0), 0);
    wd_quad(x, y + 1, x + 1.4f, y + 1, x + 2.4f, y + 4.4f, x + 1, y + 5, PAL_PAINT(PAINT_SKY, 0), 0);
}
static void fork_in_hand(float a) {
    float x = mayor_hand_x[1], y = mayor_hand_y[1];
    float ex = x + cosf(a) * 6, ey = y + sinf(a) * 6;
    wd_tube(x, y, ex, ey, 0.4f, 0.35f, PAL_GRAY_D, PAL_GRAY, PAL_GRAY_L, PAL_WHITE);
    for (int k = -1; k <= 1; k++) wd_line(ex + k * 0.5f, ey, ex + cosf(a) * 2 + k * 0.5f, ey + sinf(a) * 2, 0.3f, PAL_GRAY);
}

/* ================================================================== A */
/* CLIPBOARD INSERT, held 1 s: the pen ticks 19:00 Bake-Off! */
void scene_a_card(int f, int n) {
    tk_clear(PAL_PAINT(PAINT_CARAMEL, 1));
    tk_rect(40, 10, 160, 150, PAL_UI_CREAM); tk_frame(40, 10, 160, 150, PAL_PAINT(PAINT_CHOCOLATE, 1));
    tk_rect(100, 4, 40, 12, PAL_GRAY_L); tk_frame(100, 4, 40, 12, PAL_GRAY_D);
    static const char *L[5] = { "18:00 Welcome!", "19:00 Bake-Off!", "20:00 Bandstand!", "21:00 (we don't do that)", "22:00 Bedtime!" };
    for (int i = 0; i < 5; i++) tk_text(56, 32 + i * 22, L[i], PAL_PAINT(PAINT_NAVY, 1), 1);
    tk_text(46, 32, "v", PAL_PAINT(PAINT_CHERRY, 1), 2);
    if (f >= 10) { int w = f >= 16 ? 10 : (f - 10) * 2; tk_line(46, 58, 46 + w / 2, 62, PAL_PAINT(PAINT_CHERRY, 1)); tk_line(46 + w / 2, 62, 46 + w, 50, PAL_PAINT(PAINT_CHERRY, 1)); tk_line(47, 58, 47 + w / 2, 62, PAL_PAINT(PAINT_CHERRY, 1)); }
}
void scene_a_card_audio(double t0, double dur) { tk_note(AT(10), 0.08, 76, TKV_NOISE, 0.15f); }

/* ================================================================== B */
/* The tent; Nana's stall, two of everything. The Mayor, JUDGE rosette and a
 * tiny fork, tastes the first pie. Eyes close. "Mm! A-plus!" STAMP. */
#define B_CUE 92
void scene_b_aplus(int f, int n) {
    float push = seg(f, 0, n);
    wd_cam(lerp(0, -4, push), lerp(-30, -26, push), lerp(1.7f, 2.4f, push * push));
    tent(f); eaten = 0;
    Nana nn; nana_at(&nn, f); nn.armL = nn.armR = 0.4f; nn.look_x = -0.6f;
    nana_draw(&nn);
    stall(f, f >= 140, 0, 0, 0);
    Mayor m; mayor_judge(&m, f, lerp(-90, -8, seg(f, 0, 40)));
    m.step = f < 40 ? fabsf(sinf(f * 0.4f)) * 1.5f : 0;
    float taste = seg(f, 52, 66);
    m.armR = lerp(0.6f, 1.2f, taste) - 0.5f * seg(f, 70, 80);
    m.lean = 0.1f * taste;
    m.expr = f >= 70 ? MX_HAPPY : MX_NEUTRAL;
    if (f >= 70 && f < 130) { m.blink = 1; m.fan = 0.9f; m.flutter = 0.6f; }    /* transported */
    m.mouth = mouth(&M02, f, B_CUE) + (f >= 66 && f < 80 ? 0.3f * fabsf(sinf(f * 0.8f)) : 0);
    mayor_draw(&m);
    rosette();
    fork_in_hand(-0.3f);
    if (f >= 140) {
        float st = spring(f, 140, 0.4f);
        tk_text_outline((int)wd_sx(10), (int)wd_sy(-22) - (int)(st * 10), "STAMP!", PAL_WHITE, PAL_PAINT(PAINT_CHERRY, 0), 1);
        if (f < 144) tk_post_shake(1, 1);
    }
    say(&M02, f, B_CUE, mayor_head_x + 7, mayor_head_y + 2);
}
void scene_b_aplus_audio(double t0, double dur) {
    for (int k = 0; k < 5; k++) tk_note(AT(k * 8), 0.04, 45, TKV_NOISE, 0.1f);   /* footsteps */
    tk_note(AT(60), 0.06, 70, TKV_NOISE, 0.12f);   /* fork: crisp flake */
    tk_slide(AT(72), 0.6, 76, 84, TKV_SINE, 0.12f); /* transported */
    line_at(M02.id, AT(B_CUE));
    tk_sfx(AT(140), SFX_THUD, 0.5f);
}

/* ================================================================== C */
/* The stamp drifts to the second pie and stops over the blank card. */
#define C_CUE1 20
#define C_CUE2 86
void scene_c_second(int f, int n) {
    wd_cam(lerp(10, 14, seg(f, 0, n)), -28, 2.6f);
    tent(f); eaten = 0;
    Nana nn; nana_at(&nn, f); nn.armL = nn.armR = 0.4f; nn.look_x = 0.2f;
    nn.mouth = mouth(&N01, f, C_CUE2);
    if (f < C_CUE2) nn.blink = 0.6f;                 /* not looking up */
    nana_draw(&nn);
    stall(f, 1, 0, 0, 0);
    float go = seg(f, 136, 148);
    Mayor m; mayor_judge(&m, f, lerp(14, 120, go));
    m.armR = 1.1f; m.lean = go > 0 ? -0.2f : 0.05f;
    m.expr = f < 76 ? MX_HAPPY : MX_WORRY;
    m.look_x = -0.4f; m.look_y = 0.6f;
    m.step = go > 0 && go < 1 ? fabsf(sinf(f * 0.9f)) * 2 : 0;
    m.mouth = mouth(&M01, f, C_CUE1);
    mayor_draw(&m);
    rosette();
    /* the stamp hovers over the blank card and does not come down */
    if (go < 0.3f) {
        float sx = lerp(mayor_hand_x[1], 24, seg(f, 4, 20)), sy = lerp(mayor_hand_y[1], -17, seg(f, 4, 20)) + 0.4f * sinf(f * 0.4f);
        wd_rect(sx - 2.2f, sy - 4, 4.4f, 3, PAL_PAINT(PAINT_CHOCOLATE, 1), INK);
        wd_rect(sx - 3, sy - 1, 6, 1.4f, PAL_PAINT(PAINT_CHERRY, 1), INK);
    }
    say(&M01, f, C_CUE1, mayor_head_x + 7, mayor_head_y + 2);
    say(&N01, f, C_CUE2, nana_mouth_x + 4, nana_mouth_y);
}
void scene_c_second_audio(double t0, double dur) {
    line_at(M01.id, AT(C_CUE1));
    babble(AT(C_CUE2), &N01, 67);
    tk_sfx(AT(136), SFX_WHOOSH, 0.3f);
}

/* ================================================================== D */
/* Nana and the muffins. Twelve muffins look back. */
#define D_CUE 12
void scene_d_muffins(int f, int n) {
    wd_cam(lerp(26, 30, seg(f, 0, n)), -24, lerp(2.5f, 2.8f, seg(f, 150, n)));
    tent(f); eaten = 0;
    Nana nn; nana_at(&nn, f); nn.armR = 1.3f + 0.15f * sinf(f * 0.1f); nn.armL = 0.6f; nn.look_x = 1;
    nn.mouth = mouth(&N02, f, D_CUE);
    nn.squash = 1 + 0.03f * sinf(f * 0.2f) * (f < 190);
    nana_draw(&nn);
    stall(f, 1, 0, 0, f >= 196);
    Sunny u; sunny_default(&u, 56, 6, 0.9f, f); u.flip = -1; u.suitcase = 0;
    u.look_x = f >= 190 ? -0.6f : -1; u.look_y = f >= 190 ? -0.8f : 0;
    u.ears = f >= 196 ? 0.7f : 1;
    sunny_draw(&u);
    say(&N02, f, D_CUE, nana_mouth_x + 4, nana_mouth_y);
}
void scene_d_muffins_audio(double t0, double dur) {
    babble(AT(D_CUE), &N02, 66);
    tk_slide(AT(198), 0.3, 84, 79, TKV_SINE, 0.1f);   /* twelve muffins look back */
}

/* ================================================================== E */
/* [3x@15] The bite test. Crunch, crunch, CRUNCH, each bigger and lower.
 * Nana calls each by ear. Then a button. Then [R13] the biggest CRUNCH. */
static const int BITE[4] = { 20, 35, 50, 150 };
#define E_N03 24
#define E_N04 39
#define E_N05 56
void scene_e_bites(int f, int n) {
    wd_cam(32, -24, 3.0f);
    tent(f);
    eaten = 0; for (int k = 0; k < 4; k++) if (f >= BITE[k] - 6) eaten++;
    Nana nn; nana_at(&nn, f); nn.armL = 0.4f; nn.look_x = 0.8f;
    nn.armR = f >= 108 && f < 140 ? 2.6f : 0.6f;     /* paw to the cardigan, one button short */
    nn.buttons = 3;
    nn.mouth = mouth(&N03, f, E_N03) + mouth(&N04, f, E_N04) + mouth(&N05, f, E_N05);
    nana_draw(&nn);
    stall(f, 1, 0, 0, 0);
    Sunny u; sunny_default(&u, 54, 6, 0.9f, f); u.flip = -1; u.suitcase = 0; u.no_page = 1;
    float bob = 0;
    for (int k = 0; k < 4; k++) bob += spring(f, BITE[k], k == 3 ? 0.18f : 0.08f + k * 0.02f);
    u.squash = 1 - bob;
    u.page = f < 150 ? 0.55f : 0.55f;
    u.ears = f >= 152 ? 1.2f : 0.85f;
    if (f >= 152) u.look_y = -1;
    sunny_draw(&u);
    /* what's in the paw: a muffin, then the button */
    if (f >= 80 && f < 130) {
        wd_blob_ol(sunny_hand_x, sunny_hand_y - 2, 1.4f, 1.4f, 0, PAL_PAINT(PAINT_HONEY, 2), INK, 0);
        wd_blob(sunny_hand_x - 0.4f, sunny_hand_y - 2.3f, 0.3f, 0.3f, 0, INK, 0);
        wd_blob(sunny_hand_x + 0.4f, sunny_hand_y - 1.7f, 0.3f, 0.3f, 0, INK, 0);
    } else muffin(sunny_hand_x, sunny_hand_y - 3, 0, f);
    static const char *W[4] = { "crunch", "Crunch", "CRUNCH", "CRUNCH!" };
    for (int k = 0; k < 4; k++)
        if (f >= BITE[k] && f < BITE[k] + 12) tk_text_outline((int)wd_sx(60) - k * 3, (int)wd_sy(-30) - k * 2, W[k], PAL_WHITE, PAL_PAINT(PAINT_CHOCOLATE, 1), k >= 2 ? 2 : 1);
    if (f >= 150 && f < 154) tk_post_shake(2, 1);
    say(&N03, f, E_N03, nana_mouth_x + 4, nana_mouth_y);
    if (f >= E_N04) say(&N04, f, E_N04, nana_mouth_x + 4, nana_mouth_y);
    say(&N05, f, E_N05, nana_mouth_x + 4, nana_mouth_y);
}
void scene_e_bites_audio(double t0, double dur) {
    for (int k = 0; k < 4; k++) {   /* each bigger and lower */
        tk_note(AT(BITE[k]), 0.1 + k * 0.04, 62 - k * 6, TKV_NOISE, 0.25f + k * 0.08f);
        tk_kick(AT(BITE[k]), 0.1f + k * 0.08f);
    }
    babble(AT(E_N03), &N03, 67); babble(AT(E_N04), &N04, 65); babble(AT(E_N05), &N05, 64);
    tk_slide(AT(152), 0.2, 84, 96, TKV_SQ12, 0.12f);   /* ears up */
}

/* ================================================================== F */
/* [INSERT 4] an x-ray of the muffin: the key inside has tiny horns. */
void scene_f_xray(int f, int n) {
    tk_clear(PAL_PAINT(PAINT_NAVY, 0));
    wd_cam(0, 0, 3.0f);
    wd_blob_ol(0, -6, 18, 12, 0, PAL_PAINT(PAINT_NAVY, 1), PAL_PAINT(PAINT_SKY, 2), 0);
    wd_quad(-14, 2, 14, 2, 11, 20, -11, 20, PAL_PAINT(PAINT_NAVY, 1), PAL_PAINT(PAINT_SKY, 2));
    uint8_t k = PAL_PAINT(PAINT_SKY, 2);
    wd_blob_ol(-7, 0, 4, 4, 0, PAL_PAINT(PAINT_NAVY, 0), k, 0);
    wd_rect(-3, -1, 14, 2.4f, k, 0); wd_rect(7, 1, 2, 3, k, 0); wd_rect(10, 1, 1.5f, 2, k, 0);
    wd_tri(-10, -3, -8.5f, -4, -11, -8, PAL_PAINT(PAINT_CHERRY, 2));     /* tiny horns */
    wd_tri(-4, -3, -5.5f, -4, -3, -8, PAL_PAINT(PAINT_CHERRY, 2));
}
void scene_f_xray_audio(double t0, double dur) { tk_note(t0, dur, 30, TKV_SQ50, 0.4f); }

/* ================================================================== G */
/* [ONE CHANGE] Sunny is holding the key: small, pink, ordinary. Tik. */
#define G_CUE 22
void scene_g_key(int f, int n) {
    wd_cam(32, -24, 3.0f);
    tent(f); eaten = 4;
    Nana nn; nana_at(&nn, f); nn.armL = 0.4f; nn.armR = 1.0f + 0.4f * seg(f, G_CUE, G_CUE + 10); nn.look_x = 0.8f; nn.buttons = 3;
    nn.mouth = mouth(&N10, f, G_CUE);
    nana_draw(&nn);
    stall(f, 1, 0, 0, 0);
    Sunny u; sunny_default(&u, 54, 6, 0.9f, f); u.flip = -1; u.suitcase = 0; u.no_page = 1;
    u.page = 0.75f; u.ears = 1.2f; u.look_y = -0.6f; u.look_x = -0.2f;
    sunny_draw(&u);
    float kx = sunny_hand_x, ky = sunny_hand_y - 4;
    wd_blob_ol(kx - 2, ky, 1.6f, 1.6f, 0, PAL_PAINT(PAINT_BUBBLEGUM, 1), INK, 0);
    wd_blob(kx - 2, ky, 0.6f, 0.6f, 0, PAL_PAINT(PAINT_GRAPE, 2), 0);
    wd_rect(kx - 0.5f, ky - 0.5f, 5, 1.1f, PAL_PAINT(PAINT_BUBBLEGUM, 1), INK);
    wd_rect(kx + 3, ky + 0.5f, 0.9f, 1.4f, PAL_PAINT(PAINT_BUBBLEGUM, 1), 0);
    if (f >= 2 && f < 10) tk_star((int)wd_sx(kx + 3), (int)wd_sy(ky - 2), 3, PAL_WHITE);
    say(&N10, f, G_CUE, nana_mouth_x + 4, nana_mouth_y);
}
void scene_g_key_audio(double t0, double dur) {
    tk_note(AT(2), 0.04, 100, TKV_SINE, 0.3f);   /* tik */
    babble(AT(G_CUE), &N10, 70);
}

/* ================================================================== H */
/* Both pies lifted at once; underneath, flattened, is a page. Page two. */
#define H_CUE 30
void scene_h_pagetwo(int f, int n) {
    wd_cam(lerp(14, 18, seg(f, 0, n)), -26, lerp(2.5f, 2.8f, seg(f, 0, n)));
    tent(f); eaten = 4;
    float lift = seg(f, 4, 20) * 9;
    Nana nn; nana_at(&nn, f); nn.armL = nn.armR = 0.6f + 0.9f * seg(f, 0, 16); nn.look_x = 0.6f; nn.buttons = 3;
    nn.mouth = mouth(&N06, f, H_CUE);
    nana_draw(&nn);
    float take = seg(f, 196, 206);
    stall(f, 1, lift * (1 - take * 0), take < 1, 0);
    Sunny u; sunny_default(&u, 52, 6, 0.9f, f); u.flip = -1; u.suitcase = 0;
    u.page = take; u.look_x = -0.8f; u.look_y = -0.3f;
    sunny_draw(&u);
    if (take >= 1) tk_text((int)wd_sx(sunny_hand_x) - 10, (int)wd_sy(sunny_hand_y - 14), "Hee.", PAL_PAINT(PAINT_NAVY, 1), 1);
    say(&N06, f, H_CUE, nana_mouth_x + 4, nana_mouth_y);
}
void scene_h_pagetwo_audio(double t0, double dur) {
    tk_slide(AT(6), 0.2, 60, 67, TKV_SINE, 0.1f);
    babble(AT(H_CUE), &N06, 67);
    tk_note(AT(200), 0.08, 80, TKV_NOISE, 0.08f);   /* paper */
}

/* ================================================================== I */
/* The basket from the window: the egg in its tea towel, the lamp, the tag.
 * "And this. An egg." "It's been waiting for someone." She doesn't let go. */
#define I_CUE1 12
#define I_CUE2 84
static void basket(float x, float y, float t) {
    wd_blob_ol(x, y + 2, 11, 6, 0, PAL_PAINT(PAINT_CARAMEL, 1), INK, WB_SHADE);
    for (int k = -4; k <= 4; k++) wd_line(x + k * 2.4f, y - 1, x + k * 2.1f, y + 7, 0.45f, PAL_PAINT(PAINT_CARAMEL, 0));
    wd_blob_ol(x, y - 4, 4.6f, 6, 0, PAL_PAINT(PAINT_BONE, 2), INK, WB_SHADE);      /* the egg */
    wd_blob(x - 1.4f, y - 6, 0.8f, 0.8f, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
    wd_blob(x + 1.4f, y - 3, 0.7f, 0.7f, 0, PAL_PAINT(PAINT_MINT, 2), 0);
    wd_blob_ol(x, y - 0.5f, 9, 3, 0, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0), 0);        /* tea towel */
    for (int k = -3; k <= 3; k++) wd_line(x + k * 2.4f, y - 2.5f, x + k * 2.4f, y + 1.5f, 0.4f, PAL_PAINT(PAINT_ROSE, 2));
    wd_rect(x + 10, y - 7, 1, 7, PAL_PAINT(PAINT_HONEY, 1), 0);                         /* little lamp */
    wd_blob_ol(x + 10.5f, y - 9, 2.4f, 2.4f, 0, PAL_PAINT(PAINT_LEMON, 2), PAL_PAINT(PAINT_HONEY, 1), WB_SHADE);
    wd_rect(x - 12, y - 1, 5, 3, PAL_PAINT(PAINT_BONE, 2), PAL_PAINT(PAINT_CARAMEL, 0));   /* the tag */
}
void scene_i_egg(int f, int n) {
    wd_cam(lerp(20, 24, seg(f, 0, n)), lerp(-26, -25, seg(f, 0, n)), lerp(2.6f, 3.0f, seg(f, 0, n)));
    tent(f); eaten = 4;
    float out = seg(f, 0, 20), give = seg(f, 196, 206);
    float bx = lerp(4, 24, out) + 12 * give, by = lerp(-6, -22, out);
    Nana nn; nana_at(&nn, f); nn.armR = lerp(0.5f, 1.4f, out) - 0.6f * give; nn.armL = lerp(0.5f, 1.2f, out) - 0.6f * give; nn.look_x = 0.5f; nn.buttons = 3;
    nn.mouth = mouth(&N11, f, I_CUE1) + mouth(&N07, f, I_CUE2);
    nana_draw(&nn);
    stall(f, 1, 0, 0, 0);
    basket(bx, by, f);
    Sunny u; sunny_default(&u, 46, 6, 0.9f, f); u.flip = -1; u.suitcase = 0; u.no_page = 1;
    u.page = f >= 150 ? 0.4f : 0; u.look_x = -0.8f; u.look_y = -0.4f; u.ears = 0.9f;
    sunny_draw(&u);
    if (f >= 166 && f < 196) { tk_text((int)wd_sx(bx) - 6, (int)wd_sy(by - 18), "...", PAL_WHITE, 1); }   /* [R13] she doesn't let go */
    say(&N11, f, I_CUE1, nana_mouth_x + 4, nana_mouth_y);
    say(&N07, f, I_CUE2, nana_mouth_x + 4, nana_mouth_y);
}
void scene_i_egg_audio(double t0, double dur) {
    babble(AT(I_CUE1), &N11, 63);
    babble(AT(I_CUE2), &N07, 62);
}

/* ================================================================== J */
/* "The last one hatched into a little demon! Ha! Kidding." MUSIC OUT.
 * "...Mostly." [BUTTON +13] she tucks the tea towel tighter. */
#define J_CUE1 6
#define J_CUE2 150
void scene_j_mostly(int f, int n) {
    wd_cam(16, -36, lerp(3.4f, 3.9f, seg(f, 120, n)));
    tent(f); eaten = 4;
    Nana nn; nana_at(&nn, f); nn.buttons = 3;
    nn.armL = 0.5f; nn.armR = f < 120 ? 1.2f + 0.3f * sinf(f * 0.3f) : 0.5f;
    nn.squash = 1 + (f < 120 ? 0.04f * sinf(f * 0.5f) : 0);
    nn.look_x = f < 128 ? 0.8f : -0.3f;
    nn.mouth = mouth(&N08, f, J_CUE1) + mouth(&N09, f, J_CUE2);
    if (f >= 176) nn.armL = nn.armR = 1.1f;          /* the tuck */
    nana_draw(&nn);
    stall(f, 1, 0, 0, 0);
    say(&N08, f, J_CUE1, nana_mouth_x + 4, nana_mouth_y);
    say(&N09, f, J_CUE2, nana_mouth_x + 4, nana_mouth_y);
}
void scene_j_mostly_audio(double t0, double dur) {
    babble(AT(J_CUE1), &N08, 70);
    babble(AT(J_CUE2), &N09, 60);
    tk_note(AT(178), 0.12, 72, TKV_NOISE, 0.05f);   /* the tuck */
}

/* ---- MUSIC: the NANA motif, a warm little music-box waltz; MUSIC OUT at J 128 */
void tk_music(void) {
    static const int mel[12] = { 67, 72, 76, 74, 72, 69, 67, 69, 72, 71, 67, 0 };
    double t0 = tk_scene_start("b_aplus"), t1 = tk_scene_start("j_mostly") + 128 / 30.0;
    for (int i = 0; ; i++) {
        double t = t0 + i * 0.5;
        if (t >= t1) break;
        int m = mel[i % 12];
        if (m) { tk_note(t, 0.45, m + 12, TKV_SINE, 0.22f); tk_note(t, 0.1, m + 24, TKV_TRI, 0.06f); }
        if (i % 3 == 0) tk_note(t, 1.2, 48 + (i / 3 % 2) * 5, TKV_TRI, 0.15f);
    }
}
