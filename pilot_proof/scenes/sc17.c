/* sc17.c - SC17 (1-04) THE MIDSUMMER GIGGLE, HOOK-A-DUCK - DUSK. Steamy
 * introduces her ducks like a proud mother at a recital. One at the back
 * faces the Wall; nobody knows it. It squeaks by itself: three, then one.
 * Steamy very slowly sinks Duckworth. Blub. No recordings: bubbles + babble. */
#include "fair_common.h"
#define SHOTS(X) X(a_fair, 2.5) X(b_ducks, 4.7) X(c_wall, 2.4) X(d_squeak, 4.9)
#include "scene_tl.h"

static const float T1 = 3.0f, T2 = 4.4f, T3 = 6.4f, T_POP = 7.2f, T4 = 7.6f, T_SELF = 9.9f, T_SQZ = 10.4f, T_SINK = 13.4f;
static const float SQ[4] = { 10.9f, 11.4f, 11.9f, 12.8f };

static Critter steamy(float ta) {
    Critter c = cast_bloom(-14, 6, ta * 30);
    c.fur = PAL_PAINT(PAINT_MINT, 1); c.furd = PAL_PAINT(PAINT_TEAL, 1);
    c.outfit = PAL_PAINT(PAINT_SKY, 2); c.outfitd = PAL_PAINT(PAINT_SKY, 1); c.outfitl = PAL_WHITE;
    c.extras = 0; c.ears = CR_EARS_NONE; c.s = 1.0f;
    return c;
}
static void duck(float x, float y, int wall, float squash, float t) {
    float bob = 0.4f * sinf(t * 0.15f + x);
    float dir = wall ? 1 : -1;
    wd_blob_ol(x, y + bob, 3.4f, 2.4f * squash, 0, PAL_PAINT(PAINT_LEMON, 1), INK, WB_SHADE);
    wd_blob_ol(x + dir * 2.2f, y - 2.6f + bob, 1.9f, 1.8f, 0, PAL_PAINT(PAINT_LEMON, 1), INK, 0);
    wd_tri(x + dir * 3.8f, y - 2.8f + bob, x + dir * 5.6f, y - 2.4f + bob, x + dir * 3.8f, y - 2 + bob, PAL_PAINT(PAINT_TANGERINE, 1));
    if (!wall) wd_blob(x + dir * 2.6f, y - 3.2f + bob, 0.4f, 0.4f, 0, INK, 0);
}
static void tub(float ta, float sink) {
    wd_blob_ol(10, 6, 26, 6, 0, PAL_PAINT(PAINT_SKY, 1), INK, 0);
    wd_blob(10, 5, 23, 4, 0, PAL_PAINT(PAINT_SKY, 2), 0);
    float sq = ta >= T_SELF && ta < T_SELF + 0.2f ? 0.7f : 1;
    for (int k = 0; k < 4; k++) if (!(k == 0 && sink > 0)) duck(-4 + k * 8, 6 + (k & 1), 0, 1, ta * 30);
    if (sink > 0) duck(-4, 6 + sink * 4, 0, 1, ta * 30);
    float usq = 1; for (int i = 0; i < 4; i++) if (ta >= SQ[i] && ta < SQ[i] + 0.15f) usq = 0.7f;
    duck(26, 2, 1, ta >= T_SELF && ta < T_SELF + 0.2f ? sq : usq, ta * 30);   /* the one at the back, facing the Wall */
}
static void stage(float ta) {
    fair_backdrop(ta * 30, -80, 80);
    crowd_bg(ta, -120, 120, 17);
    fair_stall(10, 60, PAL_PAINT(PAINT_SKY, 1), "HOOK-A-DUCK", 1);
}
static void sunny_here(float ta) { Sunny u; sunny_default(&u, 46, 10, 0.8f, ta * 30); u.flip = -1; u.look_x = ta >= 7.2f ? 1 : -1; u.ears = ta >= 9.9f ? 1.2f : 1; sunny_draw(&u); }
static void steamy_draw(float ta, float pole_to_x) {
    sunny_here(ta);
    Critter c = steamy(ta);
    c.mouth = lip("a1_04_steamy_01", (int)((ta - T1) * 30), 27) + lip("a1_04_steamy_02", (int)((ta - T2) * 30), 45) + lip("a1_04_steamy_03", (int)((ta - T3) * 30), 24) + lip("a1_04_steamy_04", (int)((ta - T4) * 30), 54);
    c.armR = 1.3f; c.look_x = 1;
    if (ta >= T_POP) { c.look_x = 1; c.look_y = -0.3f; }
    if (ta >= T_SQZ) c.armL = 1.6f;
    critter_draw(&c);
    wd_line(critter_hand_x[1], critter_hand_y[1], pole_to_x, 0, 0.6f, PAL_PAINT(PAINT_CARAMEL, 0));
}
SHOT(a_fair, { wd_cam(lerp(-40, -10, seg(ta, 0, 2.5f)), -24, 1.4f); stage(ta); tub(ta, 0); steamy_draw(ta, -4); })
SHOT(b_ducks, {
    wd_cam(2, -12, 2.6f); stage(ta); tub(ta, 0);
    float px = ta < T2 ? -4 : ta < T3 ? 4 : 12;
    steamy_draw(ta, px);
    say_at_world("This is Duckworth!", ta, T1, 0.9f, critter_mouth_x, critter_mouth_y, 0);
    say_at_world("That's Quackers. He bites.", ta, T2, 1.5f, critter_mouth_x, critter_mouth_y, 0);
    say_at_world("And that's...", ta, T3, 0.8f, critter_mouth_x, critter_mouth_y, 0);
})
SHOT(c_wall, {
    wd_cam(24, -2, 4.2f); stage(ta); tub(ta, 0);
    if (ta >= T_POP && ta < T_POP + 0.3f) tk_text_outline(150, 30, "!", PAL_WHITE, PAL_PAINT(PAINT_NAVY, 1), 3);
    say_at_world("...Huh. I don't know that one.", ta, T4, 1.8f, -6, -24, 2);
})
SHOT(d_squeak, {
    wd_cam(8, -10, 2.6f); stage(ta);
    float sink = seg(ta, T_SINK, T_SINK + 1.0f);
    tub(ta, sink);
    steamy_draw(ta, ta < T_SINK ? -4 : -4);
    for (int i = 0; i < 4; i++) if (ta >= SQ[i] && ta < SQ[i] + 0.35f) tk_text_outline((int)wd_sx(26) - 10, (int)wd_sy(-8), "squeak", PAL_WHITE, PAL_PAINT(PAINT_LEMON, 0), 1);
    if (ta >= T_SELF && ta < T_SELF + 0.35f) tk_text_outline((int)wd_sx(26) - 10, (int)wd_sy(-8), "squeak", PAL_WHITE, PAL_PAINT(PAINT_LEMON, 0), 1);
    if (ta >= T_SQZ && ta < T_SQZ + 0.35f) tk_text_outline((int)wd_sx(-4) - 10, (int)wd_sy(-8), "squeak", PAL_WHITE, PAL_PAINT(PAINT_LEMON, 0), 1);
    if (ta >= T_SINK + 0.8f && ta < T_SINK + 1.2f) tk_text_outline((int)wd_sx(-4) - 6, (int)wd_sy(0), "blub", PAL_WHITE, PAL_PAINT(PAINT_SKY, 0), 1);
})
void tk_music(void) {
    hahaland_bed(0, 14.5, 0.08f, 0, 0);
    for (int k = 0; k < 6; k++) tk_slide(0.3 + k * 0.4, 0.3, 62 + k * 2, 66 + k * 2, TKV_TRI, 0.02f);   /* happy walla */
    babble_line(T1, 0.9f, 70, 0.2f); babble_line(T2, 1.5f, 70, 0.2f); babble_line(T3, 0.8f, 70, 0.2f);
    tk_note(T_POP, 0.05, 90, TKV_SQ12, 0.2f);
    babble_line(T4, 1.8f, 66, 0.2f);
    tk_slide(T_SELF, 0.12, 88, 92, TKV_SQ25, 0.2f);
    tk_slide(T_SQZ, 0.12, 86, 90, TKV_SQ25, 0.2f);
    for (int i = 0; i < 4; i++) tk_slide(SQ[i], 0.12, 88, 92, TKV_SQ25, 0.2f);
    tk_slide(T_SINK + 0.8, 0.3, 60, 48, TKV_SINE, 0.15f);   /* blub */
}
