/* sc51.c - SC51 (4-03) THE SEALED GATE. Protected hold 2. A soft sigh; a
 * small tired puff drifts south over its own unfinished poem to the gate.
 * "...Clack?" "Something was in there with me..." The hook, redeemed: "...and
 * we dream in pink." The puff goes through the keyhole. "...Seven." Hold. */
#include "act4_common.h"
#define SHOTS(X) X(main, 45.0)
#include "scene_tl.h"
static const float SIGH = 0.4f, GATE = 9.0f, B1 = 13.0f, B2 = 15.0f, HOOK = 21.0f, SUNG = 24.0f, THRU = 31.0f, SEVEN = 36.0f, HOLD = 37.2f;
static const Cut C[] = { { 0, -40, -30, 3.0f, 3.3f, 0, 0 }, { 3.0f, -10, -24, 1.5f, 1.6f, 40, 0 }, { GATE, 74, -20, 3.4f, 3.8f, 0, 0 }, { 11.6f, -12, -40, 2.6f, 2.8f, 0, 0 },
    { HOOK, -30, -24, 2.2f, 2.5f, 20, 0 }, { THRU, 74, -20, 3.6f, 4.0f, 0, 0 }, { HOLD, 10, -20, 1.4f, 1.45f, 0, 0 } };
static void gate(void) {
    wd_rect(64, -60, 20, 60, PAL_PAINT(PAINT_CHARCOAL, 1), INK);
    for (int k = 0; k < 7; k++) wd_blob_ol(70 + (k % 2) * 8, -54 + k * 7, 1.6f, 2, 0, PAL_UI_GOLD, INK, 0);
    wd_rect(70, -24, 8, 6, PAL_PAINT(PAINT_JET, 2), PAL_GRAY);
    tk_text((int)wd_sx(62) - 6, (int)wd_sy(-70), "KEEP OUT. FOREVER.", PAL_WHITE, 1);
}
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    cut_at(C, 7, ta, 45);
    bead_flame = 1; town(ta, 99, 0, 0);
    gate();
    float drift = seg(ta, 2.0f, GATE), thru = seg(ta, THRU, THRU + 2.0f);
    float px = lerp(-40, 72, drift), py = lerp(-26, -21, drift);
    if (thru < 1) { float s = 1 - thru; py += 1.2f * sinf(ta * 2.2f) + (ta >= GATE ? 1.5f : 0); wd_blob(px + thru * 4, py, 4 * s, 3 * s, 0, PAL_PAINT(PAINT_LAVENDER, 1), 0); wd_blob(px + 2, py + 1, 2.4f * s, 1.8f * s, 0, PAL_GRAY_L, 0);
        if (ta < GATE) for (int k = 0; k < 4; k++) wd_blob(px - 3 - k * 3, py + 2 + k * 0.6f, 0.6f, 0.6f, 0, PAL_WHITE, 0); }   /* trailing chalk dust */
    Bones bj; bones_default(&bj, -40, -10, 1.0f, ta * 30); bj.possess = 0; bj.look = ta >= 16.5f && ta < HOOK ? 1 : 0;
    if (ta >= HOOK) { float b = (ta - HOOK) / 0.43f; bj.hitL = fabsf(sinf(b * 3.14f)); bj.hitR = fabsf(cosf(b * 3.14f)); }
    if (ta >= HOLD) bj.hitL = bj.hitR = 0.2f;
    bj.squash = ta < SIGH + 1.6f ? 1 + 0.08f * sinf(seg(ta, SIGH, SIGH + 1.6f) * 3.14f) * -1 : 1;   /* the long sigh */
    if (ta >= B1 && ta < B2) bj.look = 0.5f * sinf((ta - B1) * 5);   /* ...Clack? looking about */
    if (ta >= B2 && ta < B2 + 3.4f) { bj.hitL = 0.9f; bj.hitR = 0.1f; bj.look = 0.6f; }   /* a hand to his ribs, eyes to the puff */
    bj.jaw = lip("a", (int)((ta - B1) * 30), 20) + lip("b", (int)((ta - B2) * 30), 100) + (ta >= SUNG && ta < SUNG + 5 ? 0.4f * fabsf(sinf(ta * 8)) : 0);
    bones_draw(&bj);
    say_at_world("...Clack?", ta, B1, 0.8f, -34, -50, 1);
    say_at_world("Something was in there with me. It kept trying to finish a poem.", ta, B2, 3.4f, -34, -50, 1);
    say_at_world("~ Down by the Wall where the pumps go clink, we work all night, and we dream in pink.", ta, SUNG, 5.0f, -34, -50, 1);
    if (ta >= SEVEN && ta < SEVEN + 1.6f) tk_text(6, TH - 10, "[beyond the Wall, small:] ...Seven.", PAL_WHITE, 1);
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    tk_slide(SIGH, 1.6, 50, 44, TKV_NOISE, 0.08f);
    static const int G[8] = { 67, 64, 62, 60, 62, 64, 60, 0 }; for (int i = 0; i < 16; i++) if (G[i % 8]) tk_note(2 + i * 0.6, 0.55, G[i % 8], TKV_SINE, 0.08f);   /* GOODBYE motif */
    babble_line(B1, 0.8f, 55, 0.2f); babble_line(B2, 3.4f, 52, 0.18f);
    tk_note(HOOK - 0.8, 0.05, 84, TKV_SQ50, 0.15f); tk_note(HOOK - 0.4, 0.05, 84, TKV_SQ50, 0.15f);
    static const int H[16] = { 72, 76, 79, 72, 77, 81, 79, 0, 72, 76, 79, 84, 83, 79, 77, 76 };
    for (int i = 0; i < 40; i++) { double t = HOOK + i * 0.214; if (t > THRU - 0.2) break; if (H[i % 16]) tk_note(t, 0.18, H[i % 16], TKV_SINE, 0.15f); }
    babble_line(SUNG, 5.0f, 60, 0.1f);
    tk_slide(THRU, 2.0, 70, 60, TKV_NOISE, 0.05f);
    tk_note(SEVEN, 0.3, 64, TKV_TRI, 0.12f);
}
