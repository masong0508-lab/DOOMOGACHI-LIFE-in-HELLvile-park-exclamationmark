/* sc33.c - SC33 (2-04) EXT. FUN FAIR, THE STAMPEDE - NIGHT. Close on Sunny.
 * The towel slips: the egg, this morning's crack. Tock... tock... tocktock.
 * It hatches on the riff's downbeat. MUTE: the chaos goes on in silence.
 * The smallest bubble in the show: "beep." SUPER: ...it's fine. The skull
 * turns: "...Ten." The pet climbs up and fits between Sunny's ears exactly.
 * A brave demon creeps up; the pet just looks at it. Poff. */
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
    X(a_egg,    6.0) \
    X(b_hatch,  6.0) \
    X(c_ten,    4.0) \
    X(d_hood,   8.0) \
    X(e_look,   7.0) \
    X(f_turn,   4.0)
#include "scene_tl.h"

#define INK PAL_PAINT(PAINT_JET, 1)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }

#define T_SLIP   1.0f
#define T_TOCK   2.0f
#define T_FAST   4.5f
#define T_HATCH  6.4f
#define T_BEEP   (T_HATCH + 21 / 30.0f)
#define T_FINE   (T_HATCH + 29 / 30.0f)
#define T_SKULL  12.2f
#define T_TEN    14.0f
#define T_RIFF   15.0f
#define T_CLIMB  19.0f
#define T_ONHEAD 20.6f
#define T_CREEP  25.0f
#define T_STARE  27.0f
#define T_POFF   30.0f

static void night(void) {
    static const uint8_t cols[] = { PAL_PAINT(PAINT_NAVY, 1), PAL_PAINT(PAINT_GRAPE, 1), PAL_PAINT(PAINT_GRAPE, 2) };
    tk_vgrad(0, 0, TW, TH, cols, 3);
}
static void fair_bg(float ta) {
    wd_rect(-400, -24, 800, 230, PAL_RAMP(RAMP_GRASS, 3), 0);
    wd_rect(-400, -24, 800, 1.5f, PAL_RAMP(RAMP_GRASS, 4), 0);
    for (int i = 0; i < 16; i++) { float x = -260 + i * 36; wd_blob_ol(x, -57 + 1.5f * sinf(ta * 2 + i), 2.2f, 2.8f, 0, (i % 3) ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_CARAMEL, 0), WB_SHADE); }
    wd_rect(30, -8, 20, 8, PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_CARAMEL, 0));   /* the hay bale */
}
/* the stampede behind Sunny; it leaves a circle of room once the pet is out */
static void stampede(float ta, float room) {
    CrowdOpts o = { 0 };
    o.n = 12; o.x0 = -120; o.x1 = 120; o.y0 = -16; o.y1 = -8; o.t = ta * 30; o.seed = 133;
    o.freeze_at = -1; o.panic = 1; o.avoid_x = 0; o.avoid_r = room > 18 ? room : 18; o.scale = 0.75f;
    crowd_draw(&o);
    for (int i = 0; i < 6; i++) { float x = fmodf(ta * 40 + i * 50, 300) - 150; if (fabsf(x) > room) puff_demon(x, -2, 0.8f, ta * 30 + i * 7, 1); }
    /* a cupcake tumbling end over end */
    float cx = fmodf(ta * 60, 320) - 160; wd_blob_ol(cx, -20 + 6 * sinf(ta * 5), 2.4f, 2, ta * 9, PAL_PAINT(PAINT_BUBBLEGUM, 2), INK, 0);
}
static float egg_wobble(float ta) {
    if (ta < T_TOCK || ta >= T_HATCH) return 0;
    int per = ta < T_FAST ? 6 : 3;
    int f = (int)((ta - T_TOCK) * 30);
    return (f % per) < 2 ? ((f / per) & 1 ? 1 : -1) : 0;
}
static void sunny_with(float ta, float x, float y, float s, int pet_on_head, float hood) {
    Sunny u; sunny_default(&u, x, y, s, ta * 30); u.suitcase = 0; u.no_page = 1; u.page = pet_on_head ? 0 : 0.25f;
    u.ears = ta >= 3.0f ? 1.2f : 0.9f;
    if (hood > 0) u.ears = 1.2f - hood * 1.0f;
    u.look_y = ta >= T_HATCH && !pet_on_head ? 0.8f : 0;
    sunny_draw(&u);
}
static void egg_or_pet(float ta, float hx, float hy, float s, float pet_look) {
    if (ta < T_HATCH) {
        float slip = seg(ta, T_SLIP, T_SLIP + 0.4f);
        float w = egg_wobble(ta) * 0.25f;
        wd_blob_ol(hx - 2 * s, hy - 4 * s, 3.6f * s, 4.6f * s, w, PAL_PAINT(PAINT_BONE, 2), INK, WB_SHADE);
        wd_line(hx - 3 * s, hy - 5 * s, hx - 1 * s, hy - 6 * s, 0.3f * s, INK);          /* the hairline crack */
        wd_line(hx - 1 * s, hy - 6 * s, hx + 0.4f * s, hy - 4.6f * s, 0.3f * s, INK);
        wd_blob_ol(hx - 2 * s, hy - 1 * s + slip * 4 * s, 5 * s, 3 * s, 0, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0), 0);   /* the towel slips */
    } else {
        pet_draw(hx - 2 * s, hy - 1 * s, s * 0.9f, ta * 30, pet_look, 0);
        float fall = seg(ta, T_HATCH, T_HATCH + 0.5f);
        if (ta < T_CLIMB + 3) { wd_blob_ol(hx - 6 * s - fall * 2, hy + fall * 6 * s, 2 * s, 1.4f * s, 0.5f, PAL_PAINT(PAINT_BONE, 2), INK, 0); wd_blob_ol(hx + 2 * s + fall * 2, hy + fall * 6 * s, 2 * s, 1.4f * s, -0.5f, PAL_PAINT(PAINT_BONE, 2), INK, 0); }
    }
}

#define SHOT(name, ...) \
    void scene_##name(int f, int n) { float ta = (float)tk_scene_start(#name) + f / 30.0f; (void)n; __VA_ARGS__ } \
    void scene_##name##_audio(double t0, double dur) { (void)t0; (void)dur; }

SHOT(a_egg, {
    night(); wd_cam(0, -20, lerp(3.0f, 3.6f, seg(ta, 0, 6))); fair_bg(ta);
    stampede(ta, 0);
    sunny_with(ta, 0, 4, 0.9f, 0, 0);
    egg_or_pet(ta, sunny_hand_x, sunny_hand_y, 0.9f, 0);
    if (ta >= 4.0f && ta < 4.6f) { puff_demon(lerp(-20, 30, seg(ta, 4.0f, 4.6f)), -26 - 6 * sinf(seg(ta, 4, 4.6f) * 3.14f), 0.9f, ta * 30, 1); }
    if (egg_wobble(ta) != 0) tk_text((int)wd_sx(sunny_hand_x + 4), (int)wd_sy(sunny_hand_y - 10), "tock", PAL_WHITE, 1);
})
SHOT(b_hatch, {
    night(); wd_cam(0, -14, 4.0f); fair_bg(ta);
    stampede(ta, ta >= T_FINE ? 30 : 0);
    sunny_with(ta, 0, 4, 0.9f, 0, 0);
    egg_or_pet(ta, sunny_hand_x, sunny_hand_y, 0.9f, 0);
    if (ta >= T_HATCH && ta < T_HATCH + 0.1f) tk_post_flash(PAL_WHITE, 9);     /* reduced luminance, 3 f */
    if (ta >= T_BEEP) { bubble_side = 1; bubble("beep.", 5, (int)wd_sx(sunny_hand_x - 2), (int)wd_sy(sunny_hand_y - 3)); bubble_side = 0; tk_text(6, TH - 10, "[beep.]", PAL_WHITE, 1); }
    if (ta >= T_FINE) tk_caption(18, "...it's fine.", PAL_WHITE, PAL_PAINT(PAINT_NAVY, 1), 2);
})
SHOT(c_ten, {
    /* far off on the bandstand, the skull rotates toward Sunny, stiff, no squash */
    set_sky_dawn(ta * 30, 0.25f); wd_cam(-30, -36, 2.6f);
    Banner b = { 0 }; set_wall(ta * 30); set_bandstand(ta * 30, &b, 0); tk_post_darken(9);
    Bones bj; bones_default(&bj, -30, -10, 1.0f, ta * 30); bj.possess = 1;
    bj.look = lerp(-1, 1, seg(ta, T_SKULL, T_SKULL + 0.6f)); bj.hitL = bj.hitR = 0.2f;
    bones_draw(&bj);
    if (ta >= T_SKULL + 0.6f) tk_dither_rect((int)wd_sx(-24), (int)wd_sy(-36), 120, 30, PAL_PAINT(PAINT_GRAPE, 2), 3);   /* the ribs point like a lamp */
    if (ta >= T_TEN) verdict("...Ten.", ta - T_TEN, 120);
})
SHOT(d_hood, {
    night(); wd_cam(0, -18, 3.8f); fair_bg(ta);
    stampede(ta, 30);
    float hood = seg(ta, 16.6f, 17.4f) * (1 - seg(ta, T_CLIMB - 0.4f, T_CLIMB));
    int onhead = ta >= T_ONHEAD;
    sunny_with(ta, 0, 4, 0.9f, onhead, hood);
    float hx = sunny_hand_x, hy = sunny_hand_y;
    if (!onhead) {
        float c1 = seg(ta, T_CLIMB, T_CLIMB + 0.4f), c2 = seg(ta, T_CLIMB + 0.8f, T_CLIMB + 1.2f);
        float px = lerp(hx, 4, c1), py = lerp(hy, -10, c1) - 3 * sinf(c1 * 3.14f);
        px = lerp(px, 0, c2); py = lerp(py, 4 - 23 * 0.9f - 3, c2) - 3 * sinf(c2 * 3.14f);
        if (ta < T_CLIMB) egg_or_pet(ta, hx, hy, 0.9f, 0.6f); else pet_draw(px, py, 0.8f, ta * 30, 0, 0);
    } else {
        pet_draw(0, 4 - 23 * 0.9f - 4, 0.8f, ta * 30, 0, 0);    /* between the ears: it fits exactly */
        /* the two halves of shell, scooped into the towel, tucked under one arm */
        wd_blob_ol(hx - 1, hy - 2, 4, 3, 0, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0), WB_SHADE);
    }
})
SHOT(e_look, {
    night(); wd_cam(4, -18, lerp(2.4f, 2.8f, seg(ta, 24, 31))); fair_bg(ta);
    stampede(ta, 34);
    sunny_with(ta, 0, 4, 0.9f, 1, 0);
    float look = ta >= T_STARE ? -1 : 0;
    pet_draw(0, 4 - 23 * 0.9f - 4, 0.8f, ta * 30, look, 0);
    /* one brave puff creeps up behind; the pet just looks; it backs away like from a bee; poff */
    float dx = ta < T_STARE ? lerp(-60, -16, seg(ta, T_CREEP, T_STARE)) : lerp(-16, 30, seg(ta, T_STARE + 0.6f, T_POFF));
    if (ta >= T_STARE + 0.6f) dx = lerp(-16, -34, seg(ta, T_STARE + 0.6f, T_POFF));
    if (ta < T_POFF) puff_demon(dx, -2, 0.9f, ta < T_STARE ? ta * 30 : 0, 1);
    if (ta >= T_POFF && ta < T_POFF + 0.6f) tk_text_outline((int)wd_sx(-34), (int)wd_sy(-14), "poff", PAL_WHITE, PAL_PAINT(PAINT_HONEY, 0), 1);
    if (ta >= T_POFF) wd_blob(-34, -4, 6, 3, 0, PAL_PAINT(PAINT_HONEY, 2), 0);
})
SHOT(f_turn, {
    float u = seg(ta, 31, 35);
    night(); wd_cam(lerp(4, 20, u), -20, 2.2f); fair_bg(ta);
    stampede(ta, 34);
    Sunny s0; sunny_default(&s0, 0, 4, 0.9f, ta * 30); s0.suitcase = 0; s0.no_page = 1; s0.flip = 1; s0.look_x = 1; sunny_draw(&s0);
    pet_draw(0, 4 - 23 * 0.9f - 4, 0.8f, ta * 30, 1, 0);
    /* two figures pushing through the crowd toward the bandstand steps */
    Critter fz = cast_fuzz(lerp(30, 44, u), -6, ta * 30); fz.s = 0.7f; fz.flip = 1; critter_draw(&fz);
    Critter bl = cast_bloom(lerp(40, 54, u), -6, ta * 30); bl.s = 0.65f; bl.flip = 1; critter_draw(&bl);
})

static void flat(double t, float secs) { for (int v = 0; v < 2; v++) { double tt = t + v * 8 / 30.0; int n = (int)(secs * 5) + 1; for (int k = 0; k < n; k++) tk_note(tt + k * secs / n, 0.1, v ? 38 : 41, TKV_TRI, 0.16f); } }
void tk_music(void) {
    static const int R[8] = { 76, 75, 72, 75, 76, 79, 75, 72 };
    /* the riff and the stampede; MUTE from the hatch until the riff re-enters */
    for (int i = 0; ; i++) {
        double t = i * 0.2 + ((i & 1) ? 0.05 : 0);
        if (t >= 35) break;
        if (t >= T_HATCH && t < T_RIFF) continue;
        tk_note(t, 0.15, R[i % 8], TKV_SINE, 0.12f);
        if (i % 2 == 0) tk_kick(t, 0.12f);
        if (i % 4 == 2) tk_note(t, 0.05, 60, TKV_NOISE, 0.08f);
    }
    /* tock... tock... tocktocktock */
    for (double t = T_TOCK; t < T_HATCH; t += (t < T_FAST ? 0.2 : 0.1)) tk_note(t, 0.03, 84, TKV_SINE, 0.2f);
    tk_note(4.0, 0.05, 60, TKV_NOISE, 0.15f);                        /* the demon bounces off Sunny's shoulder */
    tk_note(T_HATCH, 0.05, 90, TKV_SINE, 0.2f);                      /* crack */
    line_at("a2_04_pet_01", T_BEEP);                                 /* beep. (raw piezo, library take) */
    static const int F[3] = { 72, 71, 67 };                           /* the ITS_FINE sting */
    for (int k = 0; k < 3; k++) tk_note(T_FINE + k * 0.25, 0.3, F[k], TKV_SINE, 0.16f);
    flat(T_TEN, 0.6f);
    tk_slide(T_CLIMB, 0.2, 80, 84, TKV_SINE, 0.08f); tk_slide(T_CLIMB + 0.8, 0.2, 82, 86, TKV_SINE, 0.08f);   /* two plush hops */
    tk_slide(T_STARE + 0.6, 1.5, 70, 64, TKV_TRI, 0.06f);
    tk_note(T_POFF, 0.12, 48, TKV_NOISE, 0.2f);                      /* poff */
}
