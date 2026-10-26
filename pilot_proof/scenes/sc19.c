/* sc19.c - SC19 (1-06) HATTIE'S TENT - DUSK. Knock knock. Who's there?
 * Exactly! Nobody ever says! A laugh answers from beyond the Wall, late.
 * {d}...Tough crowd over there. */
#include "fair_common.h"
#define SHOTS(X) X(a_tent, 3.6) X(b_joke, 4.6) X(c_wall, 5.3)
#include "scene_tl.h"
static const float T1 = 1.5f, T_WHO = 2.5f, T2 = 3.6f, T_LAUGH = 7.2f, T_WALL = 8.5f, T3 = 10.2f;
static void hattie(float ta, float look) {
    Critter c = cast_fuzz(0, -4, ta * 30);
    c.fur = PAL_PAINT(PAINT_ROSE, 2); c.furd = PAL_PAINT(PAINT_ROSE, 1); c.extras = 0;
    c.outfit = PAL_PAINT(PAINT_GRAPE, 2); c.outfitd = PAL_PAINT(PAINT_GRAPE, 1); c.outfitl = PAL_WHITE;
    c.armR = 1.6f; c.look_x = look;
    c.mouth = lip("a1_06_hattie_01", (int)((ta - T1) * 30), 24) + lip("a1_06_hattie_02", (int)((ta - T2) * 30), 100) + lip("a1_06_hattie_03", (int)((ta - T3) * 30), 40);
    critter_draw(&c);
    wd_line(critter_hand_x[1], critter_hand_y[1], critter_hand_x[1] - 1, critter_hand_y[1] - 5, 0.8f, PAL_GRAY_D);   /* the mic */
    wd_blob_ol(critter_hand_x[1] - 1, critter_hand_y[1] - 6, 1.4f, 1.6f, 0, PAL_GRAY, INK, 0);
}
static void tent_in(float ta) {
    static const uint8_t c[] = { PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_BONE, 2) };
    tk_vgrad(0, 0, TW, TH, c, 2);
    for (int k = -8; k < 8; k++) wd_rect(k * 16.0f, -80, 8, 90, PAL_PAINT(PAINT_ROSE, 1), 0);
    wd_rect(-200, 4, 400, 100, PAL_PAINT(PAINT_CARAMEL, 2), 0);
    wd_rect(-14, -4, 28, 8, PAL_PAINT(PAINT_CARAMEL, 1), INK);   /* a crate for a stage */
    int sx = (int)wd_sx(0) - 60, sy = (int)wd_sy(-40);
    tk_rect(sx, sy, 120, 12, PAL_WHITE); tk_text(sx + 4, sy + 2, "HATTIE HAHA, LIVE! (mostly)", PAL_PAINT(PAINT_MAGENTA, 0), 1);
    (void)ta;
}
static void audience(float ta, float laugh) {
    for (int i = 0; i < 8; i++) {
        Critter c = cast_pip(-56 + i * 16, 34 + (i & 1) * 6, ta * 30 + i * 11);
        uint32_t h = tk_hash(i * 41u + 9u);
        c.s = 0.75f; c.extras = 0; c.ears = (h & 1) ? CR_EARS_ROUND : CR_EARS_LONG;
        c.fur = (h & 2) ? PAL_PAINT(PAINT_CARAMEL, 1) : PAL_PAINT(PAINT_BONE, 2); c.furd = PAL_PAINT(PAINT_CARAMEL, 0);
        c.outfit = (h & 4) ? PAL_PAINT(PAINT_MINT, 1) : PAL_PAINT(PAINT_LEMON, 1); c.outfitd = PAL_PAINT(PAINT_TEAL, 0); c.outfitl = PAL_WHITE;
        c.mouth = laugh * fabsf(sinf(ta * 18 + i)); c.squash = 1 - laugh * 0.05f * fabsf(sinf(ta * 18 + i));
        critter_draw(&c);
        wd_rect(-60 + i * 16, 36 + (i & 1) * 6, 8, 6, PAL_GRAY_L, PAL_GRAY_D);   /* folding chairs */
    }
}
SHOT(a_tent, { wd_cam(0, -10, 1.6f); tent_in(ta); hattie(ta, 0); audience(ta, 0);
    say_at_world("Knock knock.", ta, T1, 0.8f, critter_mouth_x, critter_mouth_y, 1);
    if (ta >= T_WHO) tk_caption(120, "Who's there?", PAL_WHITE, PAL_PAINT(PAINT_NAVY, 1), 1); })
SHOT(b_joke, { wd_cam(0, -18, 2.8f); tent_in(ta); hattie(ta, 0); audience(ta, seg(ta, T_LAUGH, T_LAUGH + 0.1f) * (1 - seg(ta, T_LAUGH + 1.0f, T_LAUGH + 1.2f)));
    if (ta < T_WHO + 0.8f) tk_caption(120, "Who's there?", PAL_WHITE, PAL_PAINT(PAINT_NAVY, 1), 1);
    say_at_world("Exactly! Nobody ever says! Thank you, I'm here all night.", ta, T2, 3.4f, critter_mouth_x, critter_mouth_y, 1); })
SHOT(c_wall, { wd_cam(4, -22, lerp(3.0f, 3.4f, seg(ta, 8.2f, 13.5f))); tent_in(ta); hattie(ta, ta >= T_WALL + 0.2f ? 1 : 0); audience(ta, (1 - seg(ta, T_LAUGH + 1.0f, T_LAUGH + 1.2f)));
    if (ta >= T_WALL && ta < T_WALL + 1.6f) tk_text(6, 6, "[a laugh from beyond the Wall, late]", PAL_WHITE, 1);
    say_at_world("...Tough crowd over there.", ta, T3, 1.4f, critter_mouth_x, critter_mouth_y, 1); })
void tk_music(void) {
    hahaland_bed(0, 13.5, 0.06f, T_WALL + 0.2, 13.5);   /* the music ducks to nothing */
    babble_line(T1, 0.8f, 66, 0.22f);
    for (int v = 0; v < 6; v++) babble_line(T_WHO + v * 0.03, 0.8f, 60 + v * 3, 0.06f);
    babble_line(T2, 3.4f, 66, 0.22f);
    for (int v = 0; v < 8; v++) for (int k = 0; k < 4; k++) tk_slide(T_LAUGH + v * 0.03 + k * 0.15, 0.1, 64 + v * 2, 68 + v * 2, TKV_TRI, 0.04f);
    for (int k = 0; k < 4; k++) tk_slide(T_WALL + k * 0.16, 0.1, 55, 58, TKV_TRI, 0.025f);   /* one voice, far, filtered */
    babble_line(T3, 1.4f, 63, 0.16f);
}
