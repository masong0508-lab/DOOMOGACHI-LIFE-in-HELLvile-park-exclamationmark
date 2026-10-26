/* sc26.c - SC26 (1-13) THE MIDSUMMER GIGGLE - NIGHT (21:00). BONG: nine. The
 * whole Giggle looks up. No fireworks, only stars. MUTE. One cricket. A
 * balloon pops far off. "...Hee. Hee." INSERT 4: beyond the Wall, a pipe
 * heaves a purple sigh. Every lantern leans south and gutters. Thumps from
 * the Wall; Mascotty thumps back. Only Sunny sees. */
#include "fair_common.h"
#include "mayor.h"
#define SHOTS(X) X(a_bong, 6.0) X(b_sky, 5.0) X(c_hee, 2.6) X(d_pipe, 0.1334) X(e_lean, 2.0) X(f_thump, 8.27)
#include "scene_tl.h"
static const float T_BONG = 0.3f, T_MUTE = 3.6f, T_CRICK = 6.5f, T_POP = 9.5f, T_HEE = 11.6f, T_PIPE = 13.6f, TH1 = 16.0f;
static const float THW[4] = { 16.0f, 16.5f, 17.0f, 17.87f }, THM[4] = { 18.8f, 19.3f, 19.8f, 20.67f };
static void giggle_wide(float ta, float lean) {
    fair_backdrop(ta * 30, -120, 120); tk_post_darken(11);
    for (int i = 0; i < 10; i++) {   /* lantern flames */
        float x = -100 + i * 22;
        wd_blob(x, -62, 1.5f, 2.6f, lean * 0.6f, lean > 0.9f && ((int)(ta * 30) % 4 < 2) ? PAL_PAINT(PAINT_TANGERINE, 0) : PAL_PAINT(PAINT_LEMON, 2), 0);
    }
    CrowdOpts o = { 0 }; o.n = 12; o.x0 = -110; o.x1 = 110; o.y0 = -4; o.y1 = 10; o.t = ta >= T_BONG ? T_BONG * 30 : ta * 30; o.seed = 81; o.freeze_at = -1; o.eyes = ta >= T_BONG ? 0.5f : 0;
    crowd_draw(&o);
}
SHOT(a_bong, { wd_cam(0, -24, 1.4f); giggle_wide(ta, 0);
    Bones bj; bones_default(&bj, 70, -4, 0.8f, ta * 30); bj.hitL = bj.hitR = 0.9f; bones_draw(&bj);
    if (ta >= T_BONG && ta < T_BONG + 0.8f) tk_text_outline(90, 14, "BONG", PAL_WHITE, PAL_PAINT(PAINT_NAVY, 1), 2); })
SHOT(b_sky, { static const uint8_t c[] = { PAL_PAINT(PAINT_NAVY, 0), PAL_PAINT(PAINT_NAVY, 1) }; tk_vgrad(0, 0, TW, TH, c, 2);
    for (int i = 0; i < 50; i++) { uint32_t h = tk_hash(i * 29u); tk_px(h % TW, (h >> 9) % TH, ((h >> 4) & 3) ? PAL_PAINT(PAINT_SKY, 2) : PAL_WHITE); }
    if (ta >= T_CRICK && ta < T_CRICK + 1) tk_text(10, 140, "[a cricket]", PAL_WHITE, 1);
    if (ta >= T_POP && ta < T_POP + 0.6f) tk_text(150, 140, "[pop]", PAL_WHITE, 1); })
SHOT(c_hee, { wd_cam(-10, -34, 3.4f); fair_backdrop(ta * 30, -80, 80); tk_post_darken(11);
    Mayor m; mayor_default(&m, -10, 4, 1.0f, ta * 30); m.look_y = -1; m.expr = MX_NEUTRAL; m.flutter = 0; m.fan = 0.1f;
    m.mouth = lip("a1_13_mayor_01", (int)((ta - T_HEE) * 30), 30); mayor_draw(&m);
    say_at_world("...Hee. Hee.", ta, T_HEE, 0.9f, mayor_head_x + 6, mayor_head_y + 2, 1); })
SHOT(d_pipe, { tk_clear(PAL_PAINT(PAINT_RUST, 0)); tk_rect(60, 70, 140, 24, PAL_PAINT(PAINT_CHARCOAL, 1)); tk_circle(200, 82, 14, PAL_PAINT(PAINT_CHARCOAL, 1)); tk_vape_cloud(200, 60, (int)((ta - T_PIPE) * 30) + 6, 3, 2); })
SHOT(e_lean, { wd_cam(0, -24, 1.4f); giggle_wide(ta, 1); })
SHOT(f_thump, { wd_cam(10, -20, 1.7f); giggle_wide(ta, 1);
    set_wall(ta * 30);
    float k = 0; for (int i = 0; i < 4; i++) if (ta >= THM[i] && ta < THM[i] + 0.2f) k = 1;
    wd_rect(-70, -14 - k * 2, 12, 14, PAL_PAINT(PAINT_SKY, 1), INK); wd_blob_ol(-64, -24, 9, 9, 0, PAL_WHITE, INK, WB_SHADE);
    wd_blob_ol(-68, -40, 2.4f, 8, -0.1f, PAL_WHITE, INK, 0); wd_blob_ol(-60, -40, 2.4f, 8, 0.1f, PAL_WHITE, INK, 0);
    wd_blob(-67, -25, 1.3f, 1.3f, 0, INK, 0); wd_blob(-61, -25, 1.3f, 1.3f, 0, INK, 0);
    Sunny u; sunny_default(&u, 30, 14, 0.85f, ta * 30); u.look_x = -1; u.ears = 1.2f; sunny_draw(&u);
    for (int i = 0; i < 4; i++) { if (ta >= THW[i] && ta < THW[i] + 0.3f) tk_text(200, 40, "thump", PAL_WHITE, 1); if (ta >= THM[i] && ta < THM[i] + 0.3f) tk_text(20, 40, "thump", PAL_WHITE, 1); } })
void tk_music(void) {
    hahaland_bed(0, T_MUTE, 0.12f, 0, 0);
    for (int k = 0; k < 9; k++) tk_note(T_BONG + k * 0.36, 0.34, 55, TKV_SINE, 0.3f - k * 0.02f);
    for (int k = 0; k < 6; k++) tk_note(T_CRICK + k * 0.12, 0.04, 100, TKV_SQ12, 0.05f);
    tk_note(T_POP, 0.05, 70, TKV_NOISE, 0.08f);
    line_at("a1_13_mayor_01", T_HEE);
    tk_slide(T_PIPE, 0.2, 40, 34, TKV_NOISE, 0.4f);
    for (int i = 0; i < 4; i++) { tk_note(THW[i], 0.15, 33, TKV_SINE, 0.3f); tk_note(THM[i], 0.15, 36, TKV_SINE, 0.3f); }
    (void)TH1;
}
