/* sc45.c - SC45 (3-06) BANDSTAND STEPS - NIGHT. Silent by design. Sunny brings
 * Nana's recipe tin up through drifting hearts. The crowd comes out; the Mayor
 * crawls out top hat first. Tunk. The keepsakes. WHO'S BRINGING WHAT? The
 * column: Nox, Giggs (+1, folded), Charlotte (her thumb), Fuzz, Bonejangles
 * (plink), Clinker: a poem (will rate yours). The looks. Fuzz stands. */
#include "act2_common.h"
#include <stdio.h>
#define SHOTS(X) X(main, 35.0)
#include "scene_tl.h"
static const float GIVE = 6.0f, LID = 9.0f, OPEN = 12.0f, N0 = 14.0f, LOOKS = 27.6f, STAND = 31.0f;
static const Cut C[] = { { 0, 10, -10, 1.5f, 1.7f, -10, 0 }, { 4.0f, -40, 18, 3.0f, 3.2f, 0, 0 }, { GIVE, 4, -4, 3.0f, 3.4f, 0, 0 }, { LID, 0, 0, 1, 1, 0, 1 },
    { N0, 0, 0, 1, 1, 0, 2 }, { LOOKS, 0, -10, 2.6f, 2.8f, 0, 0 }, { STAND, -6, -16, 2.0f, 2.3f, 0, 0 } };
static const char *NAMES[6] = { "Nox: bone soup (2 bowls)", "Giggs: cupcakes (+1)", "Charlotte: cookies (sorry)", "Fuzz: hugs", "Bonejangles: music", "Clinker: a poem (will rate yours)" };
static void sheet(float ta) {
    tk_clear(PAL_PAINT(PAINT_NAVY, 0)); tk_rect(8, 0, 224, 160, PAL_UI_CREAM); tk_frame(8, 0, 224, 160, PAL_PAINT(PAINT_CARAMEL, 0));
    float scroll = seg(ta, N0, N0 + 6 * 0.87f) * 40;
    int y0 = 8 - (int)scroll;
    tk_text(30, y0, "MIDSUMMER MINGLE", PAL_PAINT(PAINT_MAGENTA, 0), 1); tk_text(30, y0 + 10, "WHO'S BRINGING WHAT?", PAL_PAINT(PAINT_MAGENTA, 0), 1);
    for (int i = 0; i < 6; i++) {
        float at = N0 + 0.6f + i * 0.87f + (i == 5 ? 1.2f : 0); if (ta < at) continue;
        int y = y0 + 30 + i * 18;
        const char *s = NAMES[i]; char buf[40]; snprintf(buf, sizeof buf, "%s", s);
        if (i == 1 && ta >= at + 1.0f) buf[17] = 0;   /* the corner folded down over the (+1) */
        tk_text(16, y, buf, PAL_PAINT(PAINT_NAVY, 1), 1); tk_line(14, y + 10, 226, y + 10, PAL_PAINT(PAINT_SKY, 1));
        if (i == 1 && ta >= at + 0.6f && ta < at + 1.2f) { tk_ellipse(200, y + 2, 8, 6, PAL_PAINT(PAINT_CARAMEL, 1)); }   /* a plump little hand darts in: fwip */
        if (i == 1 && ta >= at + 1.0f) tk_tri(150, y - 2, 190, y - 2, 190, y + 10, PAL_PAINT(PAINT_BONE, 1));
        if (i == 2 && ta >= at + 0.3f) tk_ellipse(12, y + 4, 9, 7, PAL_PAINT(PAINT_BONE, 2));   /* Nana's thumb */
    }
}
static void keepsakes(float ta) {
    tk_clear(PAL_PAINT(PAINT_NAVY, 0)); tk_rect(40, 20, 160, 120, PAL_PAINT(PAINT_SKY, 1)); tk_rect(46, 26, 148, 108, PAL_PAINT(PAINT_SKY, 0));
    if (ta >= LID + 0.6f) {
        tk_circle(80, 60, 8, PAL_PAINT(PAINT_ROSE, 2)); tk_line(80, 68, 80, 90, PAL_RAMP(RAMP_GRASS, 3));        /* a dried lantern-flower */
        for (int k = 0; k < 20; k++) tk_px(110 + k, 60 + (int)(sinf(k * 0.6f) * 4), PAL_PAINT(PAINT_LEMON, 1));    /* a curl of ribbon */
        tk_tri(150, 70, 166, 70, 158, 54, PAL_GRAY_L);                                                            /* a horn-shaped cookie cutter */
        tk_rect(70, 100, 100, 14, PAL_WHITE); tk_text(74, 103, "Charlotte's turn: #9", PAL_PAINT(PAINT_NAVY, 1), 1);
    } else tk_rect(40, 20 - (int)(seg(ta, LID, LID + 0.4f) * 30), 160, 10, PAL_PAINT(PAINT_SKY, 2));
    if (ta >= OPEN) { float u = seg(ta, OPEN, OPEN + 1.0f); tk_rect(60, 140 - (int)(u * 100), 120, (int)(u * 100), PAL_UI_CREAM); }
}
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    int k = cut_at(C, 7, ta, 35);
    if (k == 1) { keepsakes(ta); return; }
    if (k == 2) { sheet(ta); return; }
    dark_stage(ta, 3, -10, 0.01f); wd_blob(0, -18, 1.2f, 2, 0, PAL_PAINT(PAINT_NAVY, 0), 0);
    possessed_bj(ta, 0, ta >= LOOKS + 0.6f ? -0.2f : 0);
    for (int i = 0; i < 12; i++) { uint32_t h = tk_hash(i * 7u); tk_heart((int)(h % TW), (int)((h >> 8) % TH + ta * 12) % TH, 1, PAL_PAINT(PAINT_ROSE, 2)); }
    cake_table(-50, 24, ta, 0);
    float crawl = seg(ta, 3.0f, 5.0f);
    Mayor m; mayor_default(&m, lerp(-50, -60, crawl), 24, 0.8f, ta * 30); m.squash = lerp(0.6f, 1.0f, crawl); m.flutter = 0; m.expr = MX_NEUTRAL;
    if (crawl > 0.2f) { if (ta >= LOOKS + 1.0f) m.look_x = 1; mayor_draw(&m); }
    CrowdOpts o = { 0 }; o.n = 8; o.x0 = 40; o.x1 = 110; o.y0 = 22; o.y1 = 30; o.t = ta * 30; o.seed = 121; o.freeze_at = -1; crowd_draw(&o);
    Critter fz = fuzz_at(ta, 4, -6); fz.kneel = ta < STAND ? 1 : 0; fz.look_y = ta < STAND ? 1 : 0; fz.armL = fz.armR = ta < STAND ? 1.2f : 1.0f;
    if (ta >= LOOKS && ta < LOOKS + 0.3f) fz.look_x = 1; else if (ta >= LOOKS + 0.3f && ta < STAND) { fz.look_x = -1; fz.look_y = -1; }
    critter_draw(&fz);
    Critter bl = bloom_at(ta, 16, -2); bl.kneel = 1; critter_draw(&bl);
    Nana nn; nana_default(&nn, ta < GIVE ? 30 : 22, ta < GIVE ? 20 : 0, 0.85f, ta * 30); nn.buttons = 3; if (ta >= GIVE) { nn.armL = nn.armR = 1.0f; } if (ta >= LOOKS + 0.6f) nn.look_x = -1; nana_draw(&nn);
    float sx = lerp(80, 30, seg(ta, 0, GIVE - 0.4f));
    Sunny u; sunny_default(&u, sx, 20, 0.8f, ta * 30); u.flip = -1; u.suitcase = 0; u.no_page = 1; u.page = ta < GIVE + 0.3f ? 0.35f : 0; sunny_draw(&u);
    pet_draw(sx, 20 - 23 * 0.8f - 4, 0.7f, ta * 30, -1, 0);
    float tx = ta < GIVE + 0.3f ? sunny_hand_x - 2 : nana_hand_x[1], ty = ta < GIVE + 0.3f ? sunny_hand_y - 3 : nana_hand_y[1] - 2;
    wd_rect(tx - 4, ty - 3, 8, 5, PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_NAVY, 1)); wd_blob(tx, ty - 3.5f, 1.4f, 0.9f, 0, PAL_WHITE, WB_OUT);
    Critter p = cast_pip(64, 22, ta * 30); p.flip = -1; if (ta >= LOOKS + 1.4f) p.ear_turn = 0; p.ears = CR_EARS_LONG; critter_draw(&p);
    if (ta >= STAND) { wd_rect(critter_hand_x[1] - 2, critter_hand_y[1] - 6, 6, 7, PAL_UI_CREAM, PAL_PAINT(PAINT_CARAMEL, 0)); }
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    tk_note(0, 0.08, 72, TKV_NOISE, 0.08f);                         /* pff: a heart lands on the lid */
    tk_note(LID + 0.4, 0.08, 40, TKV_NOISE, 0.2f);                  /* tunk */
    static const int NM[8] = { 67, 72, 76, 74, 72, 69, 67, 0 };      /* NANA motif, 2 bars, to "Bonejangles: music" */
    for (int i = 0; i < 8; i++) if (NM[i]) tk_note(N0 + i * 0.5, 0.45, NM[i] + 12, TKV_SINE, 0.14f);
    for (int i = 0; i < 6; i++) tk_note(N0 + 0.6 + i * 0.87 + (i == 5 ? 1.2 : 0), 0.04, 88, TKV_SINE, 0.1f);
    tk_note(N0 + 0.6 + 0.87 + 0.6, 0.06, 80, TKV_NOISE, 0.12f);    /* fwip */
    tk_note(N0 + 0.6 + 4 * 0.87, 0.3, 79, TKV_SINE, 0.15f);          /* plink */
    tk_slide(LOOKS - 1, 0.3, 50, 47, TKV_NOISE, 0.08f);              /* the skull creaks a pixel */
    static const int H[8] = { 64, 64, 67, 0, 64, 64, 67, 0 }; for (int i = 0; i < 12; i++) if (H[i % 8]) tk_note(STAND - 1 + i * 0.43, 0.4, H[i % 8], TKV_TRI, 0.06f);
}
