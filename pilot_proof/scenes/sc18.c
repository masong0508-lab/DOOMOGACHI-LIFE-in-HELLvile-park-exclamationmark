/* sc18.c - SC18 (1-05) FACE PAINTING - DUSK. ANY FACE (within reason).
 * Kevin's three eyes; only the third one blinks; nobody asks about it. */
#include "fair_common.h"
#define SHOTS(X) X(a_easel, 4.0) X(b_third, 7.0)
#include "scene_tl.h"
static const float T1 = 1.4f, T_TIK = 4.1f, T2 = 4.8f, T3 = 8.4f, T_BACK = 9.9f;
static void kevin(float ta, float x) {
    Critter c = cast_fuzz(x, 8, ta * 30);
    c.fur = PAL_PAINT(PAINT_HONEY, 2); c.furd = PAL_PAINT(PAINT_HONEY, 1); c.extras = 0;
    c.outfit = PAL_PAINT(PAINT_TANGERINE, 1); c.outfitd = PAL_PAINT(PAINT_TANGERINE, 0); c.outfitl = PAL_PAINT(PAINT_TANGERINE, 2);
    c.armL = 1.4f; c.flip = 1; c.look_x = 0.6f;
    c.mouth = lip("a1_05_kevin_01", (int)((ta - T1) * 30), 72) + lip("a1_05_kevin_02", (int)((ta - T2) * 30), 99) + lip("a1_05_kevin_03", (int)((ta - T3) * 30), 36);
    critter_draw(&c);
    /* the third eye, in the middle; only it blinks */
    float ex = x + 0.6f, ey = critter_head_y - 3.0f;   /* in the middle, above the other two */
    int blink = (ta >= T_TIK && ta < T_TIK + 0.15f);
    if (blink) wd_line(ex - 1.2f, ey, ex + 1.2f, ey, 0.5f, INK); else { wd_blob_ol(ex, ey, 1.7f, 1.9f, 0, PAL_WHITE, INK, 0); wd_blob(ex + 0.3f, ey, 0.9f, 1.0f, 0, INK, 0); }
    /* the palette, held like a shield */
    wd_blob_ol(critter_hand_x[0] - 2, critter_hand_y[0], 5, 3.4f, 0.3f, PAL_PAINT(PAINT_CARAMEL, 2), INK, 0);
    for (int k = 0; k < 4; k++) wd_blob(critter_hand_x[0] - 4 + k * 1.8f, critter_hand_y[0] - 1 + (k & 1), 0.8f, 0.8f, 0, k == 0 ? PAL_PAINT(PAINT_CHERRY, 1) : k == 1 ? PAL_PAINT(PAINT_SKY, 1) : k == 2 ? PAL_PAINT(PAINT_LEMON, 1) : PAL_PAINT(PAINT_MINT, 1), 0);
}
static void easel(void) {
    wd_line(-36, 8, -30, -20, 0.8f, PAL_PAINT(PAINT_CARAMEL, 0)); wd_line(-24, 8, -30, -20, 0.8f, PAL_PAINT(PAINT_CARAMEL, 0));
    wd_rect(-42, -24, 24, 16, PAL_WHITE, PAL_PAINT(PAINT_CARAMEL, 0));
    int sx = (int)wd_sx(-41), sy = (int)wd_sy(-23);
    tk_text(sx + 2, sy + 2, "ANY FACE", PAL_PAINT(PAINT_MAGENTA, 0), 1);
    tk_text(sx + 2, sy + 12, "(within reason)", PAL_PAINT(PAINT_NAVY, 1), 1);
}
static void sunny_here(float ta, int both_blink) { Sunny u; sunny_default(&u, 30, 10, 0.85f, ta * 30); u.flip = -1; u.look_x = -1; u.blink = both_blink; sunny_draw(&u); }
SHOT(a_easel, { wd_cam(-6, -16, 2.0f); fair_backdrop(ta * 30, -80, 80); crowd_bg(ta, -140, 140, 23); fair_stall(-2, 50, PAL_PAINT(PAINT_TANGERINE, 1), "FACE PAINTING", 1); easel(); kevin(ta, -4); sunny_here(ta, 0);
    say_at_world("Yep. Three eyes. Go on, you can look.", ta, T1, 2.4f, critter_mouth_x, critter_mouth_y, 0); })
SHOT(b_third, { wd_cam(4, -22, 3.0f); fair_backdrop(ta * 30, -80, 80); fair_stall(-2, 50, PAL_PAINT(PAINT_TANGERINE, 1), NULL, 0); kevin(ta, -4); sunny_here(ta, ta >= T_BACK && ta < T_BACK + 0.25f);
    if (ta >= T_TIK && ta < T_TIK + 0.4f) tk_text(110, 30, "tik", PAL_WHITE, 1);
    say_at_world("Everyone asks about the first two. Nobody asks about the third one.", ta, T2, 3.3f, critter_mouth_x, critter_mouth_y, 0);
    say_at_world("He's very sensitive.", ta, T3, 1.2f, critter_mouth_x, critter_mouth_y, 0); })
void tk_music(void) {
    hahaland_bed(0, 11, 0.07f, 0, 0);
    babble_line(T1, 2.4f, 58, 0.2f); tk_note(T_TIK, 0.04, 98, TKV_SINE, 0.2f);
    babble_line(T2, 3.3f, 56, 0.2f); babble_line(T3, 1.2f, 55, 0.18f);
    tk_note(T_BACK, 0.04, 96, TKV_SINE, 0.15f); tk_note(T_BACK + 0.06, 0.04, 96, TKV_SINE, 0.15f);
}
