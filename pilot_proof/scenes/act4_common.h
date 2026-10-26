#include "act2_common.h"
#include <stdio.h>
/* the frozen town on the bandstand (Act Four): who still has a running clock is passed in */
static float bead_flame = 0;   /* 0 blue bead .. 1 warm gold */
static void candle_rail(float ta) {
    for (int i = 0; i < 3; i++) { float x = -12 + i * 6; wd_rect(x - 1, -16, 2, 6, PAL_PAINT(PAINT_BONE, 2), INK); }
    float s = lerp(0.6f, 1.6f, bead_flame);
    wd_blob(6, -17 - s, 0.8f * s, s, 0, bead_flame > 0.5f ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_SKY, 1), WB_OUT);
}
static void town(float ta, float freeze_t, float thaw_t, float poss) {
    float clock = ta < freeze_t ? ta : (thaw_t > 0 && ta >= thaw_t ? ta : freeze_t);
    glow_on = poss; dark_stage(ta, 0, 0, 0); candle_rail(ta);
    Bones bj; bones_default(&bj, -40, -10, 1.0f, ta * 30); bj.possess = poss; bj.hitL = bj.hitR = 0.1f;
    if (poss > 0.5f) bj.squash = 1 + 0.02f * ((int)(ta * 30) & 1); bones_draw(&bj);
    Critter fz = fuzz_at(clock, -2, -8); fz.kneel = 1; fz.armL = fz.armR = 1.3f; fz.mouth = ta < freeze_t + 0.1f || clock != freeze_t ? 0 : 0.8f; critter_draw(&fz);
    wd_rect(critter_hand_x[1] - 3, critter_hand_y[1] - 7, 7, 8, PAL_UI_CREAM, PAL_PAINT(PAINT_CARAMEL, 0));
    Critter bl = bloom_at(clock, 14, -4); critter_draw(&bl);
    Nana nn; nana_default(&nn, 34, 20, 0.85f, clock * 30); nn.buttons = 3; nana_draw(&nn);
    Mayor m; mayor_default(&m, 62, 20, 0.8f, clock * 30); m.look_y = -1; m.flutter = 0; m.expr = MX_NEUTRAL; mayor_draw(&m);
}
static void fade_in_from(float ta, float t0) { if (ta < t0 + 0.2f) tk_post_fade((int)(16 * (1 - seg(ta, t0, t0 + 0.2f)))); }
