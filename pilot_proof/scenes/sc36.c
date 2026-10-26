/* sc36.c - SC36 (2-07) BANDSTAND BOARDS - NIGHT. The chalk poem; a real clink
 * beyond the Wall; "...Three. Needs an ending." Pip hears it too; Nana knows
 * that voice; the sheet is in her recipe tin; "...And where's the tin?"
 * (the Mayor, from under the table: his recording); "Bake tent." Push in. */
#include "act2_common.h"
#define SHOTS(X) X(main, 40.0)
#include "scene_tl.h"
static const float T_CLINK = 4.6f, T_STOP = 8.2f, V = 10.4f, P1 = 14.0f, N1 = 17.0f, N2 = 20.2f, F1 = 22.4f, N3 = 25.4f, M1 = 28.6f, N5 = 30.2f, T_PUSH = 32.0f;
static const char *POEM1 = "Down by the Wall where the pumps", *POEM2 = "we work all night, and we-";
static void chalk(float ta) {
    int n1 = (int)(seg(ta, 0.6f, 4.4f) * 31), n2 = (int)(seg(ta, 5.4f, T_STOP) * 25);
    char a[48] = { 0 }, b[32] = { 0 }; memcpy(a, POEM1, n1); memcpy(b, POEM2, n2);
    tk_text(16, 88, a, PAL_WHITE, 1); if (ta >= 4.0f) tk_text(16, 100, "go clink,", PAL_WHITE, 1); tk_text(16, 112, b, PAL_WHITE, 1);
}
static void boards(float ta) { tk_clear(PAL_PAINT(PAINT_CHOCOLATE, 0)); for (int k = 0; k < 10; k++) tk_rect(0, k * 16, TW, 1, PAL_PAINT(PAINT_CHOCOLATE, 1)); tk_dither_rect(0, 0, TW, TH, PAL_PAINT(PAINT_GRAPE, 1), 2); chalk(ta);
    wd_cam(TW / 2, TH / 2, 1); float u = ta < T_STOP ? (ta < 4.4f ? seg(ta, 0.6f, 4.4f) * 0.95f : seg(ta, 5.4f, T_STOP) * 0.6f) : 0.6f;
    float fx = 16 + u * 230, fy = ta < 4.4f ? 104 : 116; wd_tube(fx + 40, fy - 60, fx + 3, fy - 2, 3, 2, INK, PAL_PAINT(PAINT_BONE, 0), PAL_PAINT(PAINT_BONE, 2), PAL_WHITE); }
static const Cut C[] = { { 0, 0, 0, 1, 1, 0, K_BOARDS }, { 13.6f, 4, 8, 4.4f, 4.8f, 0, 0 }, { 16.6f, 20, 4, 3.4f, 3.8f, 0, 0 },
    { 22.2f, 44, 2, 3.6f, 4.0f, 0, 0 }, { 25.2f, 24, 4, 2.6f, 2.9f, 0, 0 }, { 28.2f, -32, 14, 5.0f, 5.4f, 0, 0 }, { 30.0f, 10, 2, 2.2f, 2.4f, 0, 0 }, { T_PUSH, 0, 0, 1, 1, 0, 99 } };
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    int k = cut_at(C, 8, ta, 40);
    if (k == K_BOARDS) { boards(ta); if (ta >= T_CLINK && ta < T_CLINK + 1.5f) tk_text(6, 6, "[a faint clink, beyond the Wall]", PAL_WHITE, 1); if (ta >= V) verdict("...Three. Needs an ending.", ta - V, 40); return; }
    if (k == 99) { fair_backdrop(ta * 30, -200, 200); tk_post_darken(5); float u = seg(ta, T_PUSH, 40);
        wd_cam(lerp(0, 120, u), -20, lerp(0.9f, 2.6f, u * u)); fair_tent(120, 60, 40, PAL_PAINT(PAINT_MINT, 1));
        for (int j = 0; j < 14; j++) wd_blob(-100 + j * 18, -2 + (j & 1) * 3, 12, 4, 0, PAL_PAINT(PAINT_LAVENDER, 1), 0);
        for (int j = 0; j < 6; j++) { float x = -60 + j * 30; wd_blob(x, -10, 0.8f, 0.8f, 0, PAL_UI_RED, 0); wd_blob(x + 2.4f, -10, 0.8f, 0.8f, 0, PAL_UI_RED, 0); } return; }
    float cx = wd_cx, cy = wd_cy, cz = wd_z;
    dark_stage(ta, 2, -10, 0); wd_cam(cx, cy, cz);
    float turn = seg(ta, N5 + 0.4f, N5 + 0.9f);
    cake_table(-30, 24, ta, 0);
    Nana nn; nana_default(&nn, 14, 22, 0.9f, ta * 30); nn.look_x = -0.6f; nn.buttons = 3;
    if (ta >= N1 && ta < N2 + 1.6f) { nn.armL = 2.4f; nn.look_x = -0.2f; }                 /* paw to her chin, remembering */
    if (ta >= N2 + 0.6f && ta < N2 + 1.6f) { nn.armL = 0.6f; nn.armR = 1.4f; nn.squash = 1.06f; }   /* The CHEEK. */
    if (ta >= N3 && ta < N3 + 3) nn.armR = 1.2f;
    if (ta >= N5) { nn.armR = 1.9f; nn.look_x = 0.6f; }                                    /* pointing, without looking */
    nn.mouth = lip("x", (int)((ta - N1) * 30), 80) + lip("x", (int)((ta - N2) * 30), 45) + lip("x", (int)((ta - N3) * 30), 90) + lip("x", (int)((ta - N5) * 30), 40); nana_draw(&nn);
    float nmx = nana_mouth_x, nmy = nana_mouth_y;
    float lean = seg(ta, 13.6f, 14.0f);
    Critter p = cast_pip(lerp(10, 0, lean), 22, ta * 30); p.flip = 1; p.ear_turn = 0.8f; p.mouth = lip("y", (int)((ta - P1) * 30), 70); p.look_x = 1;
    if (turn > 0.5f) { p.ear_turn = -0.8f; p.look_x = -1; }
    critter_draw(&p); float pmx = critter_mouth_x, pmy = critter_mouth_y;
    Critter fz = fuzz_at(ta, 40, 20); fz.mouth = lip("z", (int)((ta - F1) * 30), 85);
    if (ta >= F1 - 0.3f && ta < F1 + 2.8f) { fz.armL = fz.armR = 1.6f; fz.squash = 1.05f; fz.look_y = -0.4f; }   /* hope rising */
    if (turn > 0.5f) fz.look_x = 1;
    critter_draw(&fz); float fmx = critter_mouth_x, fmy = critter_mouth_y;
    float lift = seg(ta, M1 - 0.6f, M1 - 0.2f) * (1 - seg(ta, M1 + 1.4f, M1 + 1.7f));
    if (lift > 0) { wd_rect(-56, 14 - lift * 3, 52, 2, PAL_PAINT(PAINT_CHOCOLATE, 0), 0); wd_blob(-36, 15 - lift * 2, 1.2f, 1.2f, 0, PAL_WHITE, WB_OUT); wd_blob(-32, 15 - lift * 2, 1.2f, 1.2f, 0, PAL_WHITE, WB_OUT);
        wd_blob(-35.6f, 15 - lift * 2, 0.5f, 0.5f, 0, INK, 0); wd_blob(-31.6f, 15 - lift * 2, 0.5f, 0.5f, 0, INK, 0); wd_line(-38, 17 - lift * 2, -30, 17 - lift * 2, 0.7f, PAL_PAINT(PAINT_CHOCOLATE, 1)); }
    say_at_world("Something over the Wall keeps going clink. Like in the poem.", ta, P1, 2.4f, pmx, pmy, -1);
    say_at_world("I know that voice. Last Mingle, somebody gave my pie a six.", ta, N1, 2.6f, nmx, nmy, 1);
    say_at_world("Said it needed salt. The CHEEK.", ta, N2, 1.6f, nmx, nmy, 1);
    say_at_world("So it was at that party! If we find out its name, we can send it home.", ta, F1, 2.8f, fmx, fmy, -1);
    say_at_world("Everybody who came that night signed my sheet. It's in my recipe tin.", ta, N3, 3.0f, nmx, nmy, 1);
    say_at_world("...And where's the tin?", ta, M1, 1.2f, -34, 12, 1);
    say_at_world("Bake tent. Other side of all that.", ta, N5, 1.4f, nmx, nmy, 1);
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    tk_slide(0.6, 3.8, 74, 78, TKV_NOISE, 0.06f); tk_slide(5.4, 2.8, 74, 78, TKV_NOISE, 0.06f);   /* chalk scrape */
    tk_note(T_CLINK, 0.06, 90, TKV_SINE, 0.06f);
    flat2(V, 2.0f); tk_slide(V + 1, 20, 33, 35, TKV_SAW, 0.04f);
    babble_line(P1, 2.4f, 80, 0.16f); babble_line(N1, 2.6f, 64, 0.2f); babble_line(N2, 1.6f, 62, 0.2f);
    babble_line(F1, 2.8f, 52, 0.2f); babble_line(N3, 3.0f, 64, 0.2f);
    line_at("a2_07_mayor_01", M1);
    babble_line(N5, 1.4f, 63, 0.2f);
    riff(N5 + 1.6, 40, 0.08f, 0, 0);
}
