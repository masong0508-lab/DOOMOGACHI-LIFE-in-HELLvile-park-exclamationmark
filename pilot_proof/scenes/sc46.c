/* sc46.c - SC46 (3-07) EXT. BANDSTAND - NIGHT. Act Three out: TRY 3.
 * Rhythm (the pet's beep, the hymn, the claps), water, the name... "Cl-".
 * FREEZE. "Father." "Nobody on this side says those names." Every adult
 * stops. The last flame gutters to a blue bead. {d}"We'll wait."
 * Everything is keyed to one absolute clock (seconds), so the freeze is one
 * frame for everyone. Mayor lines use his recordings; the others have none
 * yet and are bubbles with babble (assemble with NO_TTS for them). */
#include "tk.h"
#include "wd.h"
#include "sets.h"
#include "mayor.h"
#include "cast.h"
#include "show.h"
#include <math.h>
#include <string.h>

#define SHOTS(X) \
    X(a_top,      5.0) \
    X(b_rhythm,   4.5) \
    X(c_water,    6.0) \
    X(d_kneel,    2.5) \
    X(d_sheet,    4.5) \
    X(d_cl,       2.0) \
    X(e_father,   7.0) \
    X(f_still,    7.5) \
    X(g_wait,     6.0)
#include "scene_tl.h"

#define INK PAL_PAINT(PAINT_JET, 1)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }

/* key times (s) from the script's layout */
#define BEAT    (60.0 / 140.0)
#define T_HYMN  2.6
#define T_FREEZE 24.5
#define T_FATHER 25.4
#define T_NAMES  26.4
#define T_RIPPLE 29.9
#define T_BEAD   33.5
#define T_WALL   34.4
#define T_WAIT   40.0

typedef struct { const char *id, *text; double at; float secs; int rec; } Line;
static const Line LN[] = {
    { "a3_07_fuzz_01",  "All three. From the top.", 0.5, 1.9f, 0 },
    { "a3_07_fuzz_03",  "Come in close, come in close, hold the one who's frightened most.", T_HYMN, 6.8f, 0 },
    { "a3_07_bloom_01", "...Steady.", 7.4, 0.9f, 0 },
    { "a3_07_fuzz_04",  "Gently.", 10.2, 0.8f, 0 },
    { "a3_07_fuzz_02",  "Cl-", 24.3, 0.2f, 0 },
    { "a3_07_mayor_01", "Father.", T_FATHER, 1.07f, 1 },
    { "a3_07_mayor_02", "Nobody on this side says those names.", T_NAMES, 2.37f, 1 },
};
enum { L_TOP, L_HYMN, L_STEADY, L_GENTLY, L_CL, L_FATHER, L_NAMES };

/* the clock of each adult stops at the ripple (Fuzz already stopped at the freeze) */
static float clock_of(float ta, float stop) { return ta < stop ? ta : stop; }
static float mouth_of(int i, float ta) {
    const Line *l = &LN[i];
    int fr = (int)((ta - l->at) * 30);
    return lip(l->id, fr, (int)(l->secs * 30));
}
static void say_at(int i, float ta, float sx, float sy) {
    const Line *l = &LN[i];
    if (ta < l->at || ta >= l->at + l->secs + 0.6f) return;
    if (i == L_CL && ta >= T_FREEZE) { bubble(l->text, 3, (int)wd_sx(sx), (int)wd_sy(sy)); return; }
    bubble(l->text, (int)((ta - l->at) * 22) + 1, (int)wd_sx(sx), (int)wd_sy(sy));
}

/* ---- the stage ------------------------------------------------------------ */
static float flame_h(float ta) {               /* the last candle's flame height */
    float h = 4 + 2 * seg(ta, 8.0f, 9.5f) + 1.5f * seg(ta, 12.2f, 13.0f);   /* stands up straight; grows with the water */
    return ta < T_BEAD ? h : lerp(h, 1.0f, seg(ta, T_BEAD, T_BEAD + 26 / 30.0f));
}
static void candle(float ta, float x, float y) {
    wd_rect(x - 1.2f, y - 6, 2.4f, 6, PAL_PAINT(PAINT_BONE, 2), INK);
    float h = flame_h(ta), fl = ta < T_FREEZE ? 0.3f * sinf(ta * 23) : 0;
    int bead = ta >= T_BEAD + 0.8f;
    if (h > 1.2f) { wd_blob(x + fl * 0.3f, y - 7 - h * 0.5f, 3 + h * 0.4f, 2 + h * 0.7f, 0, PAL_PAINT(PAINT_LEMON, 2), 0); }
    wd_blob_ol(x + fl * 0.4f, y - 6.5f - h * 0.5f, 1.2f + h * 0.12f, 0.6f + h * 0.5f, 0,
               bead ? PAL_PAINT(PAINT_SKY, 1) : PAL_PAINT(PAINT_LEMON, 1), bead ? PAL_PAINT(PAINT_NAVY, 1) : PAL_PAINT(PAINT_TANGERINE, 1), 0);
}
static void steps(void) {
    for (int k = 0; k < 3; k++)
        wd_rect(20 + k * 7, -10 + k * 3.4f, 40 - k * 7, 3.4f, PAL_PAINT(PAINT_SILVER, 1 + (k & 1)), PAL_PAINT(PAINT_CHARCOAL, 1));
}
static void clipboard_knees(float x, float y) {
    wd_rect(x - 5, y - 2, 10, 7, PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CHOCOLATE, 1));
    wd_rect(x - 3.5f, y - 1, 7, 5.5f, PAL_WHITE, 0);
}
static void stage(float ta, int crowd_on) {
    int frozen = ta >= T_FREEZE;
    float tw = frozen ? T_FREEZE : ta;                /* world idle clock */
    set_sky_dawn(tw * 30, 0.25f);
    set_wall(tw * 30);
    Banner b = { 0 };
    set_bandstand(tw * 30, &b, 0);
    tk_post_darken(8);
    steps();
    /* possessed Bonejangles on the stage: never frozen, always twitching */
    Bones bj; bones_default(&bj, -28, -10, 1.0f, ta * 30);
    bj.possess = 1 - 0.3f * seg(ta, 11.6f, 12.4f);
    bj.hitL = ta < T_FREEZE ? fabsf(sinf(ta * 7.3f)) * 0.6f : 0.2f + 0.2f * (sinf(ta * 31) > 0.7f);
    bj.hitR = ta < T_FREEZE ? fabsf(cosf(ta * 7.3f)) * 0.6f : 0.2f;
    bj.squash = 1 + 0.03f * sinf(ta * (ta >= T_FREEZE ? 40.0f : 9.0f));
    bones_draw(&bj);
    if (ta >= 11.6f && ta < 13.0f) for (int k = 0; k < 4; k++) wd_blob(-28 + k * 2 - 3, -40 - (ta - 11.6f) * 12 - k * 3, 2.5f, 2, 0, PAL_GRAY_L, 0);  /* steam */
    candle(ta, -8, -10);
    /* Fuzz at the top of the steps */
    float fz = clock_of(ta, T_FREEZE);
    Critter fu = cast_fuzz(4, -10, fz * 30); fu.flip = -1;
    int sing = fz >= T_HYMN && fz < T_HYMN + 6.9f;
    fu.mouth = ta >= T_FREEZE ? 0.8f : mouth_of(L_TOP, fz) + mouth_of(L_HYMN, fz) + mouth_of(L_GENTLY, fz) + mouth_of(L_CL, fz);
    if (sing) { int bt = (int)((fz - T_HYMN) / BEAT); fu.armL = fu.armR = (bt % 2) ? 1.5f : 1.2f; }   /* claps on two and four */
    if (fz >= 9.6f && fz < 11.0f) fu.armR = 1.4f;                    /* the flask, to Sunny */
    fu.kneel = seg(fz, 15.6f, 16.4f);
    if (fz >= 16.4f) { fu.armL = fu.armR = 1.3f; fu.look_y = 1; }  /* the sheet up to the light */
    fu.blink = fmodf(fz * 30, 90) > 86 && !frozen;
    critter_draw(&fu);
    float fuzz_mx = critter_mouth_x, fuzz_my = critter_mouth_y;
    if (fz >= 16.4f) wd_rect(critter_hand_x[0] - 1, critter_hand_y[0] - 6, 7, 8, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0));
    /* Sunny at his side, the pet on their head */
    Sunny u; sunny_default(&u, 16, -10, 0.8f, ta * 30); u.suitcase = 0; u.no_page = 1; u.flip = -1;
    u.page = ta >= 10.6f && ta < 12.4f ? 0.5f : 0;
    u.look_x = ta >= 33 ? -1 : -0.5f;
    u.ears = 1;
    if (ta >= 34) u.look_y = 0.6f;          /* one ear, then the eyes, toward the sheet */
    sunny_draw(&u);
    if (ta >= 11.6f && ta < 12.4f) for (int k = 0; k < 5; k++) wd_blob(lerp(sunny_hand_x, -22, (ta - 11.6f) / 0.8f) - k, sunny_hand_y + k * 2, 0.6f, 0.9f, 0, PAL_PAINT(PAINT_SKY, 2), 0);   /* the water, gently */
    float pet_y = -10 - 23 * 0.8f - 7;
    int beat = ta >= T_HYMN && ta < T_FREEZE ? ((int)((ta - T_HYMN) / BEAT * 30) % 30) < 6 : 0;
    pet_draw(16, pet_y, 0.8f, ta * 30, ta >= 32 ? 1 : -0.4f, beat);
    /* Bloom, a step below; two fingers up for "...Steady." */
    float bl = clock_of(ta, T_RIPPLE + 0.03f);
    Critter bm = cast_bloom(36, -3.2f, bl * 30); bm.flip = -1;
    bm.mouth = mouth_of(L_STEADY, bl);
    if (bl >= 7.2f && bl < 9.2f) bm.armR = 2.6f;
    critter_draw(&bm);
    float bloom_mx = critter_mouth_x, bloom_my = critter_mouth_y;
    /* Pip and Nana at the foot of the steps */
    float pc = ta;                                   /* Pip's clock keeps running */
    Critter pp = cast_pip(54, 2, pc * 30); pp.flip = -1;
    if (pc >= T_HYMN + 4 * BEAT && pc < T_FREEZE) { int bt = (int)((pc - T_HYMN) / BEAT); pp.armL = pp.armR = (bt % 2) ? 1.6f : 1.1f; }
    if (ta >= T_RIPPLE) { pp.ear_turn = sinf((ta - T_RIPPLE) * 1.4f); pp.look_x = pp.ear_turn; }
    critter_draw(&pp);
    float nc = clock_of(ta, T_RIPPLE + 0.06f);
    Nana nn; nana_default(&nn, 68, 2, 0.85f, nc * 30); nn.flip = -1; nn.buttons = 3; nn.look_x = -0.6f;
    if (nc >= T_HYMN + 5 * BEAT && nc < T_FREEZE) { int bt = (int)((nc - T_HYMN) / BEAT); nn.armL = nn.armR = (bt % 2) ? 1.6f : 1.1f; }
    nana_draw(&nn);
    /* the Mayor off to one side, sat, clipboard on his knees; never looks up */
    float mc = clock_of(ta, T_RIPPLE + 0.1f);
    Mayor m; mayor_default(&m, 92, 4, 0.9f, mc * 30);
    m.squash = 0.82f; m.look_y = 1; m.look_x = -0.2f; m.armL = m.armR = 0.9f; m.flip = -1;
    m.expr = MX_NEUTRAL; m.flutter = 0;
    m.mouth = lip(LN[L_FATHER].id, (int)((ta - T_FATHER) * 30), 32) + lip(LN[L_NAMES].id, (int)((ta - T_NAMES) * 30), 71);
    mayor_draw(&m);
    float may_mx = mayor_head_x - 5, may_my = mayor_head_y + 2;
    clipboard_knees(92, -10);
    /* the crowd at the bottom, clapping in from bar 2, frozen at the ripple */
    if (crowd_on) {
        float cc = clock_of(ta, T_RIPPLE + 0.05f);
        for (int i = 0; i < 12; i++) {
            TkChibi c = tk_random_citizen((uint32_t)i + 90, 0);
            float x = -90 + i * 17.0f;
            int clap = cc >= T_HYMN + 6 * BEAT && cc < T_FREEZE && ((int)((cc - T_HYMN) / BEAT) % 2);
            tk_chibi((int)wd_sx(x), (int)wd_sy(40) - clap * 2, &c, wd_z > 2.2f ? 2 : 1, TKE_NORMAL, clap ? TKM_OPEN : TKM_SMILE, (int)(cc * 30) + i * 7, 0);
        }
    }
    /* bubbles */
    say_at(L_TOP, ta, fuzz_mx, fuzz_my);
    if (ta >= T_HYMN && ta < T_HYMN + 7.2f) say_at(L_HYMN, ta, fuzz_mx, fuzz_my);
    bubble_side = 2; say_at(L_STEADY, ta, bloom_mx, bloom_my); bubble_side = 0;
    say_at(L_GENTLY, ta, fuzz_mx, fuzz_my);
    say_at(L_CL, ta, fuzz_mx, fuzz_my);
    bubble_side = -1;
    say_at(L_FATHER, ta, may_mx, may_my);
    say_at(L_NAMES, ta, may_mx, may_my);
    bubble_side = 0;
    /* the caption under the rhythm */
    if (ta >= T_HYMN && ta < T_FREEZE) {
        int nb = (int)((ta - T_HYMN) / BEAT) % 4 + 1;
        char cap[32] = "["; for (int k = 0; k < nb; k++) strcat(cap, k ? " beep." : "beep."); strcat(cap, "]");
        tk_text(6, TH - 10, cap, PAL_WHITE, 1);
    }
}

#define SHOT(name, body) \
    void scene_##name(int f, int n) { float ta = (float)tk_scene_start(#name) + f / 30.0f; (void)n; body } \
    void scene_##name##_audio(double t0, double dur) { (void)t0; (void)dur; }

SHOT(a_top,    { float u = seg(f, 0, 150); wd_cam(lerp(18, 14, u), -26, lerp(1.45f, 1.65f, u)); stage(ta, 1); })
SHOT(b_rhythm, { float u = seg(f, 0, 135); wd_cam(lerp(14, 22, u), -24, lerp(1.9f, 2.1f, u)); stage(ta, 1); })
SHOT(c_water,  { float u = seg(f, 0, 180); wd_cam(lerp(-2, -6, u), -30, lerp(2.6f, 2.9f, u)); stage(ta, 0); })
SHOT(d_kneel,  { wd_cam(2, -26, 3.0f); stage(ta, 0); })
SHOT(e_father, { float u = seg(f, 0, 210); wd_cam(lerp(36, 46, u), -22, lerp(1.6f, 1.8f, u)); stage(ta, 1);
                 if (f >= 162) { tk_post_shake((f - 162) < 3 ? 1 : 0, 0); } })
SHOT(f_still,  { float u = seg(f, 0, 225); wd_cam(lerp(24, 16, u), -24, lerp(2.0f, 2.6f, u)); stage(ta, 1); })
SHOT(g_wait,   { float u = seg(f, 0, 180); wd_cam(lerp(-14, -18, u), -26, lerp(2.4f, 3.2f, u)); stage(ta, 0);
                 float w = (float)(ta - T_WAIT);
                 if (w >= 0) { tk_rect(0, 128, TW, 22, PAL_PAINT(PAINT_JET, 1)); tk_caption(132, "We'll wait.", PAL_PAINT(PAINT_GRAPE, 2), PAL_PAINT(PAINT_JET, 1), 1); }
                 if (w >= 8 / 30.0f) tk_caption(142, "We've got all night.", PAL_PAINT(PAINT_GRAPE, 1), PAL_PAINT(PAINT_JET, 1), 1); })

/* the sign-up sheet, over his shoulder; his finger runs down and stops */
void scene_d_sheet(int f, int n) {
    tk_clear(PAL_PAINT(PAINT_NAVY, 0));
    float u = seg(f, 10, 100);
    tk_rect(50, 6, 140, 154, PAL_UI_CREAM); tk_frame(50, 6, 140, 154, PAL_PAINT(PAINT_CARAMEL, 0));
    tk_text(62, 14, "SIGN-UP", PAL_PAINT(PAINT_NAVY, 1), 1);
    static const char *N[6] = { "Nox", "", "Charlotte", "Fuzz", "Bonejangles", "" };
    for (int i = 0; i < 6; i++) { tk_line(60, 42 + i * 18, 180, 42 + i * 18, PAL_PAINT(PAINT_SKY, 1)); if (N[i][0]) tk_text(64, 32 + i * 18, N[i], PAL_PAINT(PAINT_NAVY, 1), 1); }
    tk_tri(170, 52, 188, 52, 188, 70, PAL_PAINT(PAINT_BONE, 1));       /* the folded corner */
    /* the last line: his paw covers it; we never see the name */
    int py = 30 + (int)(u * 92);
    tk_ellipse(110, py + 10, 26, 13, PAL_PAINT(PAINT_CARAMEL, 1));
    tk_ellipse(132, py + 2, 9, 6, PAL_PAINT(PAINT_CARAMEL, 1));
    tk_ellipse(132, py + 2, 5, 3, PAL_PAINT(PAINT_CARAMEL, 2));
    /* candle light flicker across the paper */
    tk_post_darken(13 + (int)(sinf(f * 0.7f) > 0.6f));
    (void)n;
}
void scene_d_sheet_audio(double t0, double dur) { (void)t0; (void)dur; }
SHOT(d_cl,     { wd_cam(4, -32, 3.6f); stage(ta, 0); })

/* ---- sound: one score, on the absolute clock --------------------------- */
static void babble(double t, float secs, float pitch, float vol) {
    int n = (int)(secs * 7.5f); if (n < 1) n = 1;
    for (int k = 0; k < n; k++) {
        uint32_t h = tk_hash((uint32_t)k * 31u + (uint32_t)(secs * 100) + (uint32_t)pitch);
        tk_note(t + k * secs / n, 0.07, (int)pitch + (int)(h % 5) - 2, TKV_TRI, vol);
        tk_note(t + k * secs / n, 0.05, (int)pitch + 12 + (int)(h % 5) - 2, TKV_SQ12, vol * 0.25f);
    }
}
void tk_music(void) {
    /* lines without recordings: babble (Mayor lines go in by ID) */
    babble(LN[L_TOP].at, LN[L_TOP].secs, 52, 0.25f);
    babble(LN[L_STEADY].at, LN[L_STEADY].secs, 64, 0.22f);
    babble(LN[L_GENTLY].at, LN[L_GENTLY].secs, 50, 0.2f);
    tk_note(LN[L_CL].at, 0.2, 52, TKV_TRI, 0.25f);                   /* "Cl-", cut on frame 0 */
    line_at(LN[L_FATHER].id, LN[L_FATHER].at);
    line_at(LN[L_NAMES].id, LN[L_NAMES].at);
    /* the pet's beep on every beat, steady as a clock, until the freeze */
    for (double t = T_HYMN; t < T_FREEZE - 0.01; t += BEAT) tk_note(t, 0.06, 95, TKV_SQ50, 0.14f);
    /* the hymn, sung in major; the ribs answer in minor, a hair behind, dragging */
    static const int H[16] = { 64, 64, 67, 0, 64, 64, 67, 0, 69, 67, 65, 64, 62, 64, 60, 0 };
    static const int R[16] = { 63, 63, 67, 0, 63, 63, 67, 0, 68, 67, 65, 63, 62, 63, 60, 0 };
    for (int rep = 0; rep < 1; rep++)
        for (int i = 0; i < 16; i++) {
            double t = T_HYMN + i * BEAT;
            if (H[i]) { tk_note(t, BEAT * 0.9, H[i], TKV_TRI, 0.3f); tk_note(t, BEAT * 0.9, H[i] + 12, TKV_SQ12, 0.05f); }
            double drag = 0.06 + (i >= 4 && i < 12 ? 0.05 : 0.02);
            if (R[i] && !(i >= 12 && i % 2)) tk_note(t + drag, 0.25, R[i] + 12, TKV_SINE, 0.18f);   /* trails off in bar 4 */
            if (i % 2 == 1) tk_note(t, 0.03, 70, TKV_NOISE, i >= 4 ? 0.25f : 0.12f);              /* claps on 2 and 4 */
        }
    /* he hums on under the water and the name, beeps and claps keep time */
    for (double t = T_HYMN + 16 * BEAT; t < T_FREEZE - 0.01; t += BEAT) {
        int i = (int)((t - T_HYMN) / BEAT) % 16;
        if (H[i]) tk_note(t, BEAT * 0.8, H[i], TKV_TRI, 0.12f);
        if (i % 2 == 1) tk_note(t, 0.03, 70, TKV_NOISE, 0.15f);
    }
    tk_slide(11.6, 0.6, 96, 84, TKV_NOISE, 0.12f);   /* hiss */
    tk_note(12.0, 0.1, 58, TKV_SINE, 0.2f);          /* the ribs miss a note */
    /* the bead, then the WALL motif (E-F-E-Bb), low, once */
    tk_slide(T_BEAD, 0.8, 76, 64, TKV_SINE, 0.06f);
    static const int W[4] = { 40, 41, 40, 34 };
    for (int k = 0; k < 4; k++) tk_note(T_WALL + k * 0.6, 0.55, W[k], TKV_TRI, 0.35f);
    /* {d} We'll wait. Two voices, dead flat, 8 frames apart */
    babble(T_WAIT, 1.0f, 43, 0.14f);
    babble(T_WAIT + 8 / 30.0, 1.0f, 40, 0.12f);
}
