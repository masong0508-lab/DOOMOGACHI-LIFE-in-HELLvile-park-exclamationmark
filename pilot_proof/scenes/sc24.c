/* sc24.c - SC24 (1-11) EDGE OF THE CROWD - NIGHT (19:55). Music out. Mascotty
 * and Sunny, two stopped clocks; the crowd flows round them. Page three. A
 * quarter-speed wave, one music-box note, and Sunny waves back with the page. */
#include "fair_common.h"
#define SHOTS(X) X(a_meet, 8.0) X(b_page, 10.0)
#include "scene_tl.h"
static const float T_STOP = 2.0f, T_OPEN = 8.4f, T_TAKE = 10.0f, T_WAVE = 12.5f, T_BACK = 15.0f;
static void mascotty(float ta) {
    float cool = 1;
    wd_rect(-26, -14, 12, 14, PAL_PAINT(PAINT_SKY, 1), INK);
    wd_blob_ol(-20, -24, 9, 9, 0, PAL_WHITE, INK, WB_SHADE);
    wd_blob_ol(-24, -40, 2.4f, 8, -0.1f, PAL_WHITE, INK, 0); wd_blob_ol(-16, -40, 2.4f, 8, 0.1f, PAL_WHITE, INK, 0);
    wd_blob(-23, -25, 1.3f, 1.3f, 0, INK, 0); wd_blob(-17, -25, 1.3f, 1.3f, 0, INK, 0);
    float open = seg(ta, T_OPEN, T_OPEN + 8 / 30.0f);
    float wave = ta >= T_WAVE ? sinf((ta - T_WAVE) * 1.6f) * 0.5f : 0;   /* quarter speed */
    float px = -12 + wave * 3, py = -10 - (ta >= T_WAVE ? 8 : 0);
    wd_blob_ol(px, py, 2.4f, 2.4f, 0, PAL_WHITE, INK, 0);
    if (ta < T_TAKE) wd_quad(px - 1, py - 1 - open * 3, px + 5, py - 1 - open * 3, px + 5, py + 4, px - 1, py + 4, PAL_PAINT(PAINT_SILVER, 2), PAL_GRAY_D);
    (void)cool;
}
SHOT(a_meet, { wd_cam(-6, -22, lerp(2.0f, 2.6f, seg(ta, 0, 8))); fair_backdrop(ta * 30, -100, 100); tk_post_darken(11);
    tk_dither_rect(0, 0, TW, TH, PAL_PAINT(PAINT_SKY, 2), 2);   /* cool light from nowhere */
    CrowdOpts o = { 0 }; o.n = 10; o.x0 = -100; o.x1 = 100; o.y0 = -4; o.y1 = 8; o.t = ta * 30; o.seed = 61; o.freeze_at = -1; o.scale = 0.8f; o.avoid_x = -6; o.avoid_r = 26;
    crowd_draw(&o); mascotty(ta);
    Sunny u; sunny_default(&u, 6, 2, 0.85f, ta < T_STOP ? ta * 30 : T_STOP * 30); u.flip = -1; u.look_x = -1; u.suitcase = 1; sunny_draw(&u); })
SHOT(b_page, { wd_cam(-6, -24, 3.0f); fair_backdrop(ta * 30, -100, 100); tk_post_darken(11); tk_dither_rect(0, 0, TW, TH, PAL_PAINT(PAINT_SKY, 2), 2);
    mascotty(ta);
    Sunny u; sunny_default(&u, 6, 2, 0.85f, ta < T_TAKE ? T_STOP * 30 : ta * 30); u.flip = -1; u.look_x = -1; u.suitcase = ta < T_TAKE;
    u.page = ta >= T_TAKE ? (ta >= T_BACK ? 0.8f + 0.2f * sinf((ta - T_BACK) * 1.6f) : 0.4f) : 0;
    sunny_draw(&u);
    if (ta >= T_TAKE) { float sx = 0; (void)sx; wd_rect(-2, 1, 10, 9, PAL_PAINT(PAINT_CARAMEL, 1), INK); } })
void tk_music(void) {
    for (int k = 0; k < 40; k++) babble_line(k * 0.45, 0.4f, 58 + (k * 7) % 14, 0.03f);   /* the walla, close and ordinary */
    tk_note(T_OPEN, 0.06, 80, TKV_NOISE, 0.06f);
    tk_note(T_WAVE, 1.2, 84, TKV_SINE, 0.25f);   /* one music-box note */
}
