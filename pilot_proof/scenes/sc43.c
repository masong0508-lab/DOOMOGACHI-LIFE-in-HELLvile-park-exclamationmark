/* sc43.c - SC43 (3-04) BANDSTAND - NIGHT. Protected hold 1 of 3. "Father,
 * don't." "Nothing is unhuggable." He climbs; the crowd forgets to breathe;
 * Pip covers Mr. Flops' eyes. The hug: no squash; a terrible minor chord;
 * a kazoo; the second candle dies; FAIL x2. Eleven years ago tonight... */
#include "act2_common.h"
#define SHOTS(X) X(main, 40.0)
#include "scene_tl.h"
static const float B1 = 1.6f, PAT = 3.2f, F1 = 3.8f, CLIMB = 6.6f, TOP = 11.6f, HUG = 13.4f, KAZ = 18.0f, OUT = 19.2f, FAIL = 19.5f, LET = 22.0f, SIT = 24.4f, F2 = 26.0f, F3 = 30.6f, HOLD = 32.6f;
static const Cut C[] = { { 0, -14, -20, 1.5f, 1.6f, 0, 0 }, { 1.2f, 24, -6, 3.6f, 4.0f, 0, 0 }, { CLIMB, -14, -20, 1.5f, 1.55f, 0, 0 }, { 9.2f, 50, 20, 3.2f, 3.4f, 0, 0 },
    { TOP, -30, -32, 2.8f, 3.4f, 0, 0 }, { KAZ - 0.4f, -8, -16, 5.0f, 5.4f, 0, 0 }, { FAIL, -20, -26, 1.8f, 1.8f, 0, 0 }, { LET, -10, -14, 2.2f, 2.6f, 0, 0 }, { F2 - 0.4f, 6, -8, 3.6f, 4.0f, 0, 0 }, { HOLD, -14, -20, 1.6f, 1.7f, 0, 0 } };
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    cut_at(C, 10, ta, 40);
    dark_stage(ta, 3, -10, 0);
    if (ta >= OUT) { wd_blob(-6, -18, 1.2f, 2, 0, PAL_PAINT(PAINT_NAVY, 0), 0); }
    possessed_bj(ta, 0, 0.3f);
    /* the crowd behind the hay bales, forgetting to breathe one after another */
    CrowdOpts o = { 0 }; o.n = 9; o.x0 = 30; o.x1 = 110; o.y0 = 20; o.y1 = 30; o.t = ta * 30; o.seed = 111; o.freeze_at = CLIMB * 30; o.ripple = 4; crowd_draw(&o);
    for (int b = 0; b < 3; b++) wd_rect(32 + b * 28, 22, 24, 10, PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_CARAMEL, 0));
    Nana nn; nana_default(&nn, 56, 20, 0.85f, ta * 30); nn.buttons = 3; nana_draw(&nn);
    Critter p = cast_pip(48, 20, ta * 30); p.flip = -1; p.armL = ta >= 9.6f ? 2.0f : 0.5f; critter_draw(&p);   /* a paw over Mr. Flops' eyes, own eyes open */
    if (ta >= 9.6f) wd_blob(46, 9.5f, 2.2f, 1.2f, 0, p.fur, WB_OUT);
    /* Fuzz: up the steps, the hug, down, sat small */
    float climb = seg(ta, CLIMB, TOP);
    float fx = lerp(30, -26, climb), fy = lerp(-3, -10, climb);
    if (ta >= LET) { fx = lerp(-26, 6, seg(ta, LET, SIT)); fy = lerp(-10, -6, seg(ta, LET, SIT)); }
    Critter fz = fuzz_at(ta, fx, fy);
    if (ta < CLIMB) { fz.armL = fz.armR = 1.4f; if (ta >= PAT && ta < PAT + 0.4f) fz.armR = 1.0f; }
    else if (ta < TOP) { fz.y -= fabsf(sinf((ta - CLIMB) * 3.14f)) * 1.5f; fz.armL = fz.armR = 1.2f; }
    else if (ta < HUG) { fz.armL = fz.armR = lerp(1.2f, 2.8f, seg(ta, TOP + 0.4f, HUG - 0.4f)); }
    else if (ta < LET) { fz.armL = fz.armR = 1.6f; fz.blink = 1; fz.x = -32; }   /* all the way, eyes shut */
    else if (ta < SIT) fz.armL = fz.armR = 0.4f;
    else { fz.kneel = 1; fz.armL = fz.armR = 1.2f; fz.look_y = 1; fz.squash = 0.92f; }
    fz.mouth = lip("a", (int)((ta - F1) * 30), 40) + lip("b", (int)((ta - F2) * 30), 110) + lip("c", (int)((ta - F3) * 30), 50);
    critter_draw(&fz); float fmx = critter_mouth_x, fmy = critter_mouth_y;
    Critter bl = bloom_at(ta, ta < SIT ? 34 : 14, ta < SIT ? 0 : -2);
    if (ta >= B1 - 0.3f && ta < PAT + 0.6f) { bl.armR = 1.4f; bl.look_x = -1; }
    if (ta >= SIT) { bl.kneel = 1; bl.look_x = -1; }
    bl.mouth = lip("d", (int)((ta - B1) * 30), 30); critter_draw(&bl); float bmx = critter_mouth_x, bmy = critter_mouth_y;
    if (ta >= CLIMB && ta < TOP) for (int s = 0; s < 3; s++) if (ta >= CLIMB + s * 1.6f && ta < CLIMB + s * 1.6f + 0.4f) tk_text(30 + s * 60, 20, "pff", PAL_WHITE, 1);
    if (ta >= HUG && ta < KAZ) tk_text(6, 6, "(a chord: minor, detuned, terrible)", PAL_WHITE, 1);
    say_at_world("Father, don't.", ta, B1, 1.0f, bmx, bmy, 1);
    say_at_world("Nothing is unhuggable.", ta, F1, 1.4f, fmx, fmy, -1);
    say_at_world("Eleven years ago tonight, I hugged someone who really needed it.", ta, F2, 3.6f, fmx, fmy, -1);
    say_at_world("It didn't work then either.", ta, F3, 1.6f, fmx, fmy, -1);
    if (ta >= OUT && ta < OUT + 0.1f) tk_dither_rect(0, 0, TW, TH, PAL_PAINT(PAINT_CHERRY, 1), 3);
    if (ta >= FAIL && ta < LET) { int y = (int)lerp(-36, 6, seg(ta, FAIL, FAIL + 0.15f)); tk_rect(50, y, 140, 30, PAL_UI_RED_D); tk_frame(50, y, 140, 30, PAL_WHITE); tk_text_outline(64, y + 4, "FAIL x2", PAL_WHITE, PAL_UI_RED_D, 3); }
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    tk_note(0, 0.3, 40, TKV_NOISE, 0.15f);                                 /* a board creaks */
    babble_line(B1, 1.0f, 62, 0.16f); babble_line(F1, 1.4f, 50, 0.2f);
    for (int s = 0; s < 3; s++) tk_note(CLIMB + s * 1.6, 0.08, 45, TKV_NOISE, 0.12f);
    for (int k = 0; k < 3; k++) tk_note(HUG, KAZ - HUG, 52 + k * 3 + (k == 2 ? 1 : 0), TKV_SAW, 0.07f);   /* the chord won't stop */
    tk_slide(KAZ, 1.2, 50, 47, TKV_SAW, 0.2f);
    tk_note(OUT, 0.1, 50, TKV_NOISE, 0.15f);
    tk_sfx(FAIL, SFX_THUD, 0.5f); tk_note(FAIL + 0.1, 0.4, 47, TKV_SQ25, 0.2f);
    babble_line(F2, 3.6f, 48, 0.18f); babble_line(F3, 1.6f, 46, 0.16f);
}
