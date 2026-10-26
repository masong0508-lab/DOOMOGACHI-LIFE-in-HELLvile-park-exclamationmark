/* end.c - SC57-58 TAG "Morning" (35 s), SC59 END CREDITS (30 s), SC60 the
 * post-credits button at the sealed gate (10 s): "...needs salt." */
#include "act4_common.h"
#define SHOTS(X) X(tag, 35.0) X(credits, 30.0) X(button, 10.0)
#include "scene_tl.h"
static const float N4 = 18.0f, N5 = 21.0f, N1 = 24.0f, N2 = 28.6f, N3 = 30.4f;
static void cottage(float ta, float knock) {
    static const uint8_t c[] = { PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_UI_CREAM }; tk_vgrad(0, 0, TW, TH, c, 2);
    wd_rect(-200, 10, 400, 100, PAL_PAINT(PAINT_CARAMEL, 2), 0);
    for (int k = 0; k < 10; k++) wd_line(-200, 12 + k * 8.0f, 200, 12 + k * 8.0f, 0.4f, PAL_PAINT(PAINT_CARAMEL, 1));
    float w = knock > 0 ? sinf(knock * 30) * expf(-knock * 4) * 0.08f : 0;   /* the jelly door */
    wd_quad(-46, -40, -18, -40, -18 + w * 40, 10, -46 + w * 40, 10, PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_ROSE, 0));
    wd_blob(-22, -14, 1.4f, 1.4f, 0, PAL_UI_GOLD, WB_OUT);
    wd_rect(20, -40, 40, 30, PAL_PAINT(PAINT_SKY, 2), PAL_WHITE); wd_line(40, -40, 40, -10, 0.6f, PAL_WHITE); wd_rect(16, -10, 48, 3, PAL_WHITE, PAL_PAINT(PAINT_CARAMEL, 0));   /* the window sill */
    wd_rect(28, -36, 4, 26, PAL_PAINT(PAINT_SILVER, 1), 0);   /* the Wall, far off through the window */
}
void scene_tag(int f, int n) {
    float ta = f / 30.0f; (void)n;
    float knock = -1; for (int k = 0; k < 3; k++) if (ta >= 14.0f + k * 0.5f) knock = ta - 14.0f - k * 0.5f;
    int part2 = ta >= 13.0f;
    wd_cam(part2 ? -10 : lerp(-30, 20, seg(ta, 2, 10)), -16, part2 ? lerp(2.2f, 2.6f, seg(ta, 13, 35)) : 2.0f);
    cottage(ta, knock);
    float sx = part2 ? -10 : lerp(-36, 30, seg(ta, 3.0f, 8.0f));
    Sunny u; sunny_default(&u, sx, 8, 0.85f, ta * 30); u.suitcase = !part2 && ta < 6; u.look_x = part2 ? -1 : 1; sunny_draw(&u);
    float onsill = seg(ta, 9.0f, 10.0f);
    if (!part2 && ta < 9) pet_draw(sx, 8 - 23 * 0.85f - 4, 0.7f, ta * 30, 1, 0);
    for (int s = 0; s < 2; s++) if (ta >= 8.4f) { wd_blob_ol(32 + s * 10, -12, 3.6f, 2.4f, 0, PAL_PAINT(PAINT_BONE, 2), INK, 0); }
    if (ta >= 9.0f) { pet_draw(32, -12 - (1 - onsill) * 6, 0.7f, ta * 30, 1, 0); if (ta >= N2 - 0.4f) { wd_tri(30, -19, 31, -19, 30.5f, -21, PAL_PAINT(PAINT_MINT, 2)); wd_tri(33, -19, 34, -19, 33.5f, -21, PAL_PAINT(PAINT_MINT, 2)); } }   /* two tiny horn nubs */
    if (part2 && ta >= 16.0f) {
        Nana nn; nana_default(&nn, -34, 8, 0.85f, ta * 30); nn.flip = 1; nn.armR = ta >= N1 ? 1.4f : 0.9f; nn.buttons = 3; nn.look_x = ta >= N2 - 0.6f ? 1 : 0.3f;
        nn.mouth = 0.4f * ((ta >= N4 && ta < N4 + 1.2f) || (ta >= N5 && ta < N5 + 1) || (ta >= N1 && ta < N1 + 1.6f) || (ta >= N2 && ta < N2 + 0.5f) || (ta >= N3 && ta < N3 + 1.4f)) * fabsf(sinf(ta * 12));
        nana_draw(&nn);
        if (ta >= N1) { wd_blob_ol(nana_hand_x[1] + 4, nana_hand_y[1], 6, 2, 0, PAL_PAINT(PAINT_SILVER, 2), INK, 0); wd_blob(nana_hand_x[1] + 4, nana_hand_y[1] - 1, 1.6f, 1.2f, 0, PAL_PAINT(PAINT_CHOCOLATE, 0), 0); }   /* the dish, one burnt cookie */
        float mx = nana_mouth_x, my = nana_mouth_y;
        say_at_world("Morning, sweet pea.", ta, N4, 1.2f, mx, my, 2); say_at_world("Just muffin.", ta, N5, 1.0f, mx, my, 2);
        say_at_world("Somebody returned my dish.", ta, N1, 1.6f, mx, my, 2); say_at_world("Ah.", ta, N2, 0.5f, mx, my, 2); say_at_world("...He's teething.", ta, N3, 1.4f, mx, my, 2);
    }
    if (ta < 0.4f) tk_post_fade((int)(16 * (1 - ta / 0.4f)));
    if (knock >= 0 && knock < 0.3f) tk_text(150, 20, "bwomp", PAL_WHITE, 1);
}
void scene_tag_audio(double t0, double dur) {
    static const int H[8] = { 72, 74, 76, 79, 76, 74, 72, 0 }; for (int i = 0; i < 24; i++) if (H[i % 8]) tk_note(t0 + i * 0.5, 0.45, H[i % 8] + 12, TKV_SINE, 0.12f);
    tk_note(t0 + 0.5, 0.05, 80, TKV_NOISE, 0.15f);
    for (int k = 0; k < 3; k++) tk_slide(t0 + 14 + k * 0.5, 0.2, 48, 42, TKV_SINE, 0.25f);
    babble_line(t0 + N4, 1.2f, 64, 0.2f); babble_line(t0 + N5, 1.0f, 64, 0.2f); babble_line(t0 + N1, 1.6f, 63, 0.2f); babble_line(t0 + N2, 0.5f, 62, 0.2f); babble_line(t0 + N3, 1.4f, 60, 0.2f);
    for (int k = 0; k < 3; k++) tk_note(t0 + N3 + 2.0, 0.6, 72 + k * 4 - 1, TKV_SINE, 0.15f);   /* the ITS_FINE sting */
}
void scene_credits(int f, int n) {
    float ta = f / 30.0f; (void)n;
    tk_clear(PAL_UI_CREAM);
    if (ta < 4) { tk_caption(60, "MINGLE NIGHT", PAL_PAINT(PAINT_NAVY, 1), PAL_WHITE, 2); tk_rect(52, 58, 4, 10, PAL_PAINT(PAINT_BUBBLEGUM, 1)); return; }   /* one last curl of pink off the M */
    static const char *R[] = { "DOOMOGACHI LIFE in HELLvile Park!", "Episode 101  \"Mingle Night\"", "", "Written by [TBD]", "Created by [TBD]", "", "MAYOR GIGGS ....... the creator", "NANA PUDDIN", "PIP", "FATHER FUZZ", "BONEJANGLES", "CLINKER", "DR. BLOOM", "STEAMY", "KEVIN", "HATTIE HAHA", "MISS LULLA", "GURGLE", "", "SUNNY ....... (silent)", "THE PET ....... beep.", "MASCOTTY ....... (still)", "", "Music: Doomtastic Times", "", "Thank you for not screaming." };
    int nr = (int)(sizeof R / sizeof R[0]); float y0 = TH - (ta - 4) * 12;
    for (int i = 0; i < nr; i++) { int y = (int)(y0 + i * 14); if (y < -10 || y > TH) continue; int w = tk_text_w(R[i], 1); tk_text((TW - w) / 2, y, R[i], PAL_PAINT(PAINT_NAVY, 1), 1); }
}
void scene_credits_audio(double t0, double dur) {
    static const int H[16] = { 72, 76, 79, 72, 77, 81, 79, 0, 72, 76, 79, 84, 83, 79, 77, 76 };   /* the chorus hook, on the music box */
    for (int i = 0; i < 120; i++) { double t = t0 + i * 0.25; if (t > t0 + dur - 0.5) break; if (H[i % 16]) tk_note(t, 0.3, H[i % 16] + 12, TKV_SINE, 0.14f); }
}
void scene_button(int f, int n) {
    float ta = f / 30.0f; (void)n;
    static const uint8_t c[] = { PAL_PAINT(PAINT_NAVY, 0), PAL_PAINT(PAINT_NAVY, 1) }; tk_vgrad(0, 0, TW, TH, c, 2);
    float boing = ta >= 161 / 30.0f ? sinf((ta - 161 / 30.0f) * 20) * expf(-(ta - 161 / 30.0f) * 6) : 0;
    wd_cam(0, -30, 2.2f);
    wd_rect(-200, 0, 400, 60, PAL_RAMP(RAMP_GRASS, 1), 0);
    wd_rect(-60, -70, 120, 70, PAL_PAINT(PAINT_SILVER, 0), 0);
    wd_rect(-14 - boing * 2, -60, 28 + boing * 4, 60, PAL_PAINT(PAINT_CHARCOAL, 1), INK);
    for (int k = 0; k < 7; k++) { float j = 0; for (int g = 0; g < 3; g++) if (ta >= (118 + g * 15) / 30.0f && ta < (124 + g * 15) / 30.0f) j = (g + 1) * 0.6f * ((k & 1) ? 1 : -1); wd_blob_ol(-6 + (k % 2) * 12 + j, -54 + k * 7, 2, 2.4f, 0, PAL_UI_GOLD, INK, 0); }
    wd_rect(-5, -26, 10, 7, PAL_PAINT(PAINT_JET, 2), PAL_GRAY);
    float u = seg(ta, 0, 3.0f);   /* the LOST PROPERTY lantern drifts over the Wall and sinks out of sight */
    if (ta < 3.4f) { float lx = lerp(-80, -30, u), ly = lerp(-90, -66, u) + seg(ta, 2.6f, 3.4f) * 20; wd_blob_ol(lx, ly, 3, 3.6f, 0, PAL_PAINT(PAINT_BONE, 2), PAL_PAINT(PAINT_CARAMEL, 0), 0); wd_text(lx - 2.5f, ly + 5, "1", 0.6f, PAL_PAINT(PAINT_BUBBLEGUM, 1), 0); }
    if (ta >= 103 / 30.0f && ta < 110 / 30.0f) tk_text(6, TH - 10, "[wet crunch]", PAL_WHITE, 1);
    if (ta >= 110 / 30.0f && ta < 160 / 30.0f) tk_text(6, TH - 10, "[GURGLE: gurgle.]", PAL_WHITE, 1);
    if (ta >= 180 / 30.0f && ta < 222 / 30.0f) { tk_window(30, 112, 180, 36, TKW_MINT); tk_text(40, 106, "GURGLE", PAL_WHITE, 1); tk_text(44, 126, "...needs salt.", PAL_PAINT(PAINT_NAVY, 1), 1); }
}
void scene_button_audio(double t0, double dur) {
    for (int k = 0; k < 20; k++) tk_note(t0 + k * 0.5, 0.04, 100, TKV_SQ12, 0.03f);   /* crickets */
    tk_note(t0 + 103 / 30.0, 0.15, 45, TKV_NOISE, 0.3f);
    for (int g = 0; g < 3; g++) { tk_slide(t0 + (110 + g * 15) / 30.0, 0.4, 30 + g * 2, 26, TKV_SAW, 0.2f + g * 0.08f); tk_note(t0 + (118 + g * 15) / 30.0, 0.06, 80, TKV_NOISE, 0.1f + g * 0.05f); }
    tk_sfx(t0 + 161 / 30.0, SFX_BOING, 0.5f);
    tk_slide(t0 + 180 / 30.0, 0.3, 36, 30, TKV_SAW, 0.2f); tk_note(t0 + 6.4, 0.2, 34, TKV_TRI, 0.2f); tk_note(t0 + 6.7, 0.2, 31, TKV_TRI, 0.2f);
}
void tk_music(void) {}
