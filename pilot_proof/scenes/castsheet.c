/* castsheet.c - a model sheet of the cast in the plush rig, beside the game's chibis */
#include "act4_common.h"
#define SHOTS(X) X(main, 1.0)
#include "scene_tl.h"
void scene_main(int f, int n) {
    (void)n; tk_clear(PAL_UI_CREAM); wd_cam(60, -16, 1.15f); float t = f * 3;
    Mayor m; mayor_default(&m, -30, 0, 0.8f, t); m.expr = MX_HAPPY; m.armR = 1.4f; mayor_draw(&m);
    Sunny u; sunny_default(&u, -6, 0, 0.8f, t); u.suitcase = 0; sunny_draw(&u); pet_draw(-6, -23 * 0.8f - 4, 0.7f, t, 0, 0);
    Nana nn; nana_default(&nn, 18, 0, 0.8f, t); nana_draw(&nn);
    Critter c[6] = { cast_pip(40, 0, t), cast_fuzz(60, 0, t), cast_bloom(84, 0, t), cast_fuzz(108, 0, t), cast_fuzz(130, 0, t), cast_pip(150, 0, t) };
    c[3].fur = PAL_PAINT(PAINT_HONEY, 2); c[3].furd = PAL_PAINT(PAINT_HONEY, 1); c[3].extras = 0; c[3].outfit = PAL_PAINT(PAINT_TANGERINE, 1);
    c[4].fur = PAL_PAINT(PAINT_ROSE, 2); c[4].furd = PAL_PAINT(PAINT_ROSE, 1); c[4].extras = 0; c[4].outfit = PAL_PAINT(PAINT_GRAPE, 2);
    c[5].s = 0.8f; c[5].extras = 0; c[5].ears = CR_EARS_ROUND; c[5].fur = PAL_PAINT(PAINT_HONEY, 1); c[5].outfit = PAL_PAINT(PAINT_MINT, 1);
    for (int i = 0; i < 6; i++) { c[i].s *= 0.8f; critter_draw(&c[i]); }
    Bones b; bones_default(&b, 172, 0, 0.7f, t); bones_draw(&b);
    /* the game's own chibis for reference, bottom row */
    const TkChibi *R[5] = { &TK_MAYOR, &TK_NANA, &TK_PIP, &TK_FUZZ, &TK_HERO };
    for (int i = 0; i < 5; i++) tk_chibi(30 + i * 44, 156, R[i], 1, TKE_HAPPY, TKM_SMILE, f, 0);
    tk_text(4, 4, "plush rig (top)  vs  game chibis (bottom)", PAL_PAINT(PAINT_NAVY, 1), 1);
}
void scene_main_audio(double t0, double d) { (void)t0; (void)d; }
void tk_music(void) {}
