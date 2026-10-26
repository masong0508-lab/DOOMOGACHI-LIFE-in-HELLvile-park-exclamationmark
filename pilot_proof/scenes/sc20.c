/* sc20.c - SC20 (1-07) MISS LULLA'S CRAFT TENT - DUSK. Hushed. Eleven
 * drawings, all the same friend: a purple cloud with a little crown, no
 * string. Through the flap: Cloud Nine Vapor, COMING SOON, flickering; the
 * crowd bobs past; Mascotty stands perfectly still. No button. */
#include "fair_common.h"
#define SHOTS(X) X(a_string, 8.0) X(b_flap, 5.0)
#include "scene_tl.h"
static const float T1 = 3.0f, T_F1 = 10.0f, T_F2 = 10.7f;
static void drawing(float x, float y, int i) {
    wd_rect(x - 7, y, 14, 12, PAL_WHITE, PAL_GRAY_L);
    wd_line(x - 1, y - 1, x + 1, y - 1, 0.6f, PAL_PAINT(PAINT_CARAMEL, 0));   /* the peg */
    uint32_t h = tk_hash(i * 7u);
    float wob = ((h % 5) - 2) * 0.3f;
    for (int k = 0; k < 4; k++) wd_blob(x - 3 + k * 2 + wob, y + 7, 2.2f, 1.8f, 0, PAL_PAINT(PAINT_GRAPE, (h >> k) & 1 ? 1 : 2), 0);   /* the purple cloud */
    wd_tri(x - 2 + wob, y + 4.5f, x + 2 + wob, y + 4.5f, x + wob, y + 2.5f, PAL_UI_GOLD);   /* little crown */
    wd_blob(x - 1 + wob, y + 7, 0.4f, 0.4f, 0, INK, 0); wd_blob(x + 1 + wob, y + 7, 0.4f, 0.4f, 0, INK, 0);
}
static void tent(float ta) {
    static const uint8_t c[] = { PAL_PAINT(PAINT_LAVENDER, 2), PAL_PAINT(PAINT_BONE, 2) };
    tk_vgrad(0, 0, TW, TH, c, 2);
    for (int k = -12; k < 14; k++) wd_rect(k * 16.0f, -80, 8, 90, PAL_PAINT(PAINT_LAVENDER, 1), 0);
    wd_rect(-200, 4, 500, 100, PAL_PAINT(PAINT_CARAMEL, 2), 0);
    wd_line(-80, -44, 200, -44, 0.4f, PAL_GRAY_D);
    for (int i = 0; i < 11; i++) drawing(-60 + i * 22, -44, i);
    /* sleepy children, background, softest pastels */
    wd_rect(-60, 4, 200, 6, PAL_PAINT(PAINT_CARAMEL, 1), 0);
    for (int i = 0; i < 6; i++) { Critter k = cast_pip(-50 + i * 30, 2, ta * 30 + i * 13); k.s = 0.45f; k.extras = 0; k.blink = 1; k.squash = 1 - 0.03f * sinf(ta * 2 + i); critter_draw(&k); }
}
SHOT(a_string, {
    wd_cam(lerp(-60, 150, seg(ta, 1.0f, 8.0f)), -34, 2.8f);   /* along the string at a constant speed */
    tent(ta);
    /* Miss Lulla: a young goat-kin daycare worker - toffee fur, little horns, a mint
     * smock, a big lemon hair bow and a pencil behind the ear (not Nana's sheep look) */
    Critter l = cast_pip(-76, 8, ta * 30);
    l.s = 1.0f; l.extras = 0; l.ears = CR_EARS_ROUND;
    l.fur = PAL_PAINT(PAINT_HONEY, 1); l.furd = PAL_PAINT(PAINT_CARAMEL, 1);
    l.outfit = PAL_PAINT(PAINT_MINT, 1); l.outfitd = PAL_PAINT(PAINT_TEAL, 1); l.outfitl = PAL_PAINT(PAINT_MINT, 2);
    l.armR = 2.4f; l.look_x = 1; l.blink = fmodf(ta * 30, 70) > 64;
    l.mouth = lip("a1_07_lulla_01", (int)((ta - T1) * 30), 165) * 0.6f;
    critter_draw(&l);
    float hy = critter_head_y;
    for (int k = -1; k <= 1; k += 2) wd_tri(-76 + k * 5.0f, hy - 5, -76 + k * 7.4f, hy - 4, -76 + k * 8.6f, hy - 10.5f, PAL_PAINT(PAINT_BONE, 2));   /* little horns */
    wd_blob_ol(-80, hy - 7.5f, 3.2f, 2.2f, 0.4f, PAL_PAINT(PAINT_LEMON, 2), INK, 0);    /* the hair bow */
    wd_blob_ol(-72.5f, hy - 7.5f, 3.2f, 2.2f, -0.4f, PAL_PAINT(PAINT_LEMON, 2), INK, 0);
    wd_blob_ol(-76.3f, hy - 7.5f, 1.4f, 1.4f, 0, PAL_PAINT(PAINT_LEMON, 1), INK, 0);
    wd_line(-84, hy - 2, -80, hy - 5, 0.7f, PAL_PAINT(PAINT_LEMON, 1));             /* pencil behind the ear */
    say_at_world("They all drew the same friend. Purple cloud, little crown. I've no idea who he is.", ta, T1, 5.5f, 60, -10, 2);
})
SHOT(b_flap, {
    /* through the tent flap, across the fair: the vapor sign flickers; Mascotty stands perfectly still */
    fair_backdrop(ta * 30, -80, 80);
    wd_cam(0, -24, 1.8f);
    wd_rect(-40, -62, 46, 20, PAL_PAINT(PAINT_GRAPE, 0), INK);
    int lit = !((ta >= T_F1 && ta < T_F1 + 0.15f) || (ta >= T_F2 && ta < T_F2 + 0.15f));
    int sx = (int)wd_sx(-39), sy = (int)wd_sy(-60);
    tk_text(sx + 2, sy + 2, "Cloud Nine Vapor", lit ? PAL_PAINT(PAINT_GRAPE, 2) : PAL_PAINT(PAINT_GRAPE, 0), 1);
    tk_text(sx + 2, sy + 14, "COMING SOON", lit ? PAL_PAINT(PAINT_LAVENDER, 2) : PAL_PAINT(PAINT_GRAPE, 0), 1);
    crowd_bg(ta, -100, 100, 51);
    wd_rect(-6, -14, 12, 14, PAL_PAINT(PAINT_SKY, 1), INK);                     /* Mascotty: perfectly still */
    wd_blob_ol(0, -22, 9, 9, 0, PAL_WHITE, INK, WB_SHADE);
    wd_blob_ol(-4, -38, 2.4f, 8, -0.1f, PAL_WHITE, INK, 0); wd_blob_ol(4, -38, 2.4f, 8, 0.1f, PAL_WHITE, INK, 0);
    wd_blob(-3, -23, 1.3f, 1.3f, 0, INK, 0); wd_blob(3, -23, 1.3f, 1.3f, 0, INK, 0);
    /* the tent flap frames it */
    wd_cam(TW * 0.5f, TH * 0.5f, 1);
    wd_quad(0, 0, 70, 0, 40, 160, 0, 160, PAL_PAINT(PAINT_LAVENDER, 1), 0);
    wd_quad(240, 0, 170, 0, 200, 160, 240, 160, PAL_PAINT(PAINT_LAVENDER, 1), 0);
})
void tk_music(void) {
    hahaland_bed(0, 13, 0.035f, 0, 0);   /* through the canvas, ducked */
    for (int k = 0; k < 8; k++) tk_note(0.5 + k * 1.4, 0.8, 52 + (k & 1) * 3, TKV_SINE, 0.03f);   /* drowsy hum */
    babble_line(T1, 2.0f, 64, 0.14f); babble_line(T1 + 2.6, 2.9f, 62, 0.12f);
    tk_note(T_F1, 0.05, 40, TKV_SQ12, 0.05f); tk_note(T_F2, 0.05, 40, TKV_SQ12, 0.05f);
}
