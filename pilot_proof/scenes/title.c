/* title.c - SC07-SC13 MAIN TITLE "Doomtastic Times" (50.0 s; picture cut to
 * the theme master, mixed in by the episode cut). Crane over HaHaland; the
 * logo drops; the cast pops in one per beat (Mascotty already there, still);
 * the brick wipe and split; the drop at the gate; hellify (no child in those
 * shots) and the puff demon in the corridor; three candles; BOING; the split
 * logo; the roller over the drip, which bleeds back; 13 f of silence; beep. */
#include "act4_common.h"
#include "fp.h"
#define SHOTS(X) X(main, 50.0)
#include "scene_tl.h"
#define BT (60.0f / 150.0f)
static const char *LOGO = "DOOMOGACHI LIFE";
static void pastel_sky(void) { static const uint8_t c[] = { PAL_PAINT(PAINT_SKY, 2), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PEACH, 6) }; tk_vgrad(0, 0, TW, TH, c, 3); }
static void logo(float ta, float t0, int y, int hell) {
    int n = (int)strlen(LOGO), w = n * 12;
    for (int i = 0; i < n; i++) {
        float at = t0 + i * 0.1f; if (ta < at) continue;
        float d = seg(ta, at, at + 0.15f), sq = ta < at + 0.25f && ta >= at + 0.15f ? 2 : 0;
        char ch[2] = { LOGO[i], 0 };
        int x = (TW - w) / 2 + i * 12 - sq / 2, yy = (int)lerp(-30, y, d);
        tk_text_outline(x, yy, ch, hell ? PAL_UI_RED : PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_MAGENTA, 0), 2);
    }
    if (ta >= t0 + 1.8f) tk_caption(y + 22, "in HELLvile Park!", hell ? PAL_PAINT(PAINT_CHERRY, 2) : PAL_PAINT(PAINT_TEAL, 0), PAL_WHITE, 1);
}
static void popin(float ta) {   /* MT-2: the cast pops in, one per beat, with CUTE name plates */
    static const char *N[11] = { "SUNNY", "THE PET", "MAYOR GIGGS", "NANA", "PIP & MR. FLOPS", "FATHER FUZZ", "DR. BLOOM", "STEAMY", "HATTIE", "BONEJANGLES", "RIVET" };
    int k = (int)((ta - 6.0f) / (2 * BT)); if (k > 10) k = 10;
    float a = (ta - 6.0f) - k * 2 * BT; float pop = 1 + 0.25f * expf(-a * 12) * sinf(a * 30);
    pastel_sky(); wd_cam(0, -14, 3.2f * pop);
    wd_rect(-200, 4, 400, 100, PAL_RAMP(RAMP_GRASS, 5), 0);
    /* Mascotty in the verse shot, perfectly still, never named */
    wd_rect(30, -8, 8, 12, PAL_PAINT(PAINT_SKY, 1), INK); wd_blob_ol(34, -14, 6, 6, 0, PAL_WHITE, INK, 0); wd_blob_ol(31.5f, -24, 1.6f, 5, 0, PAL_WHITE, INK, 0); wd_blob_ol(36.5f, -24, 1.6f, 5, 0, PAL_WHITE, INK, 0);
    float t30 = ta * 30;
    switch (k) {
    case 0: { Sunny u; sunny_default(&u, 0, 4, 0.9f, t30); sunny_draw(&u); break; }
    case 1: pet_draw(0, 0, 2.0f, t30, 0, 0); break;
    case 2: { Mayor m; mayor_default(&m, 0, 6, 0.9f, t30); m.expr = MX_HAPPY; m.armL = m.armR = 2.6f; m.fan = 1; mayor_draw(&m); break; }
    case 3: { Nana nn; nana_default(&nn, 0, 4, 1.0f, t30); nn.armR = 2.4f; nana_draw(&nn); break; }
    case 4: { Critter c = cast_pip(0, 4, t30); c.s = 1.0f; c.armR = 2.9f; critter_draw(&c); break; }
    case 5: { Critter c = cast_fuzz(0, 4, t30); c.armL = c.armR = 2.8f; critter_draw(&c); break; }
    case 6: { Critter c = cast_bloom(0, 4, t30); c.armR = 2.0f; critter_draw(&c); break; }
    case 7: { Critter c = cast_bloom(0, 4, t30); c.fur = c.furd = PAL_PAINT(PAINT_MINT, 1); c.extras = 0; c.outfit = PAL_PAINT(PAINT_SKY, 2); critter_draw(&c); break; }
    case 8: { Critter c = cast_fuzz(0, 4, t30); c.fur = PAL_PAINT(PAINT_ROSE, 2); c.furd = PAL_PAINT(PAINT_ROSE, 1); c.extras = 0; c.outfit = PAL_PAINT(PAINT_GRAPE, 2); c.armR = 1.6f; critter_draw(&c); break; }
    case 9: { Bones b; bones_default(&b, 0, 4, 0.7f, t30); b.hitL = fabsf(sinf(ta * 8)); b.hitR = 1 - b.hitL; bones_draw(&b); break; }
    default: wd_rect(-8, -24, 16, 18, PAL_RAMP(RAMP_STEEL, 2), INK); wd_rect(-6, -18, 12, 3, PAL_PAINT(PAINT_NAVY, 0), 0); for (int s = 0; s < 6; s++) { uint32_t h = tk_hash(s + (int)(ta * 20)); tk_px((int)wd_sx(8) + h % 12, (int)wd_sy(-8) + (h >> 8) % 10, PAL_PAINT(PAINT_LEMON, 2)); } break;   /* a welding mask and sparks */
    }
    const char *nm = N[k]; int w = tk_text_w(nm, 1); tk_rect((TW - w) / 2 - 4, 130, w + 8, 13, PAL_UI_PINK); tk_frame((TW - w) / 2 - 4, 130, w + 8, 13, PAL_UI_PINK_D); tk_text((TW - w) / 2, 132, nm, PAL_WHITE, 1);
}
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    if (ta < 6.0f) {   /* MT-1: the crane up over pastel HaHaland; the logo DROPs */
        pastel_sky(); wd_cam(0, lerp(10, -30, seg(ta, 0, 6)), 1.0f);
        wd_rect(-300, 0, 600, 100, PAL_RAMP(RAMP_GRASS, 5), 0);
        for (int i = 0; i < 7; i++) fair_stall(-150 + i * 50, 40, i & 1 ? PAL_PAINT(PAINT_ROSE, 2) : PAL_PAINT(PAINT_MINT, 2), NULL, 0);
        if (ta < 0.1f) tk_post_flash(PAL_WHITE, 6);
        logo(ta, 1.4f, 40, 0); return;
    }
    if (ta < 19.0f) { popin(ta); return; }
    if (ta < 26.0f) {   /* MT-2 end / MT-3: the Wall; the brick wipe and the mirrored split; two knocks */
        float u = seg(ta, 19, 21);
        wd_cam(0, -30, 1.6f); set_sky_dawn(ta * 30, 0.25f); set_wall(ta * 30);
        if (ta >= 21) { for (int y = 0; y < TH; y++) for (int x = TW / 2; x < TW; x++) tk_fb[y][x] = (x + y) & 1 ? PAL_PAINT(PAINT_JET, 2) : PAL_PAINT(PAINT_CHERRY, 0);
            tk_rect(TW / 2 - 3, 0, 6, TH, PAL_PAINT(PAINT_SILVER, 1));
            float run = seg(ta, 21.5f, 24.4f); int lx = (int)lerp(10, 100, run), rx = TW - 1 - lx;
            tk_circle(lx, 110, 8, PAL_PAINT(PAINT_BUBBLEGUM, 1)); tk_circle(rx, 110, 8, PAL_PAINT(PAINT_JET, 1));   /* silhouettes only on the Hellvile side */
            if ((ta >= 24.5f && ta < 24.7f) || (ta >= 25.3f && ta < 25.5f)) tk_post_shake(2, 0); }
        (void)u; return;
    }
    if (ta < 28.0f) {   /* MT-4: the drop: record scratch, near silence, the gate */
        tk_clear(PAL_PAINT(PAINT_NAVY, 0)); wd_cam(0, -30, 2.4f);
        wd_rect(-14, -60, 28, 60, PAL_PAINT(PAINT_CHARCOAL, 1), INK);
        for (int k = 0; k < 7; k++) wd_blob_ol(-6 + (k % 2) * 12, -54 + k * 7, 2, 2.4f, 0, PAL_UI_GOLD, INK, 0);
        wd_rect(-5, -26, 10, 7, PAL_PAINT(PAINT_JET, 2), PAL_GRAY);
        tk_caption(20, "KEEP OUT. FOREVER.", PAL_WHITE, PAL_PAINT(PAINT_CHERRY, 0), 1); return;
    }
    if (ta < 41.0f) {   /* MT-5: chorus; hellify rots in; first-person corridor, a puff demon; the burst on "side!" */
        float burst = 34.5f;
        tk_clear(PAL_PAINT(PAINT_GRAPE, 0)); for (int k = 0; k < 8; k++) { int w = 240 - k * 26; tk_frame((TW - w) / 2, (TH - w * 2 / 3) / 2, w, w * 2 / 3, PAL_PAINT(PAINT_GRAPE, 1 + (k & 1))); }
        wd_cam(0, 0, 1); float sz = lerp(1.0f, 7.0f, seg(ta, 28, burst));
        if (ta < burst) puff_demon(0, 0, sz, ta * 30, 1);
        else { if (ta < burst + 1.0f) tk_gibs(TW / 2, TH / 2, (int)((ta - burst) * 30), 7); if (ta < burst + 0.8f) fp_lens_blood((int)((ta - burst) * 30), 3); if (ta >= burst + 0.2f && ta < burst + 0.5f) tk_text(150, 30, "tink", PAL_WHITE, 1); }
        for (int c = 0; c < 3; c++) { float out = 37.0f + c * 2 * BT; int x = 40 + c * 80; tk_rect(x - 2, 140, 4, 12, PAL_PAINT(PAINT_BONE, 2)); if (ta < out) tk_circle(x, 136, 2, PAL_PAINT(PAINT_LEMON, 2)); }
        tk_post_hellify((int)(16 * seg(ta, 28.0f, 28.7f) * (ta < burst ? 1 : 0)));
        if (ta >= 39.6f) { int b = ta < 39.67f ? 4 : 0; tk_ellipse(TW / 2, 60 - b, 40, 30, PAL_PAINT(PAINT_JET, 2)); tk_text(150, 20, "BOING", PAL_WHITE, 1); }
        return;
    }
    if (ta < 47.0f) {   /* MT-6: split at the Wall; halves meet on the logo; the roller paints over the drip; it bleeds back */
        for (int y = 0; y < TH; y++) for (int x = 0; x < TW; x++) tk_fb[y][x] = x < TW / 2 ? PAL_RAMP(RAMP_PASTEL_PINK, 6) : PAL_PAINT(PAINT_CHERRY, 0);
        logo(41.0f + 3, 41.0f, 50, 0);
        for (int y = 46; y < 70; y++) for (int x = TW / 2; x < TW; x++) if (tk_fb[y][x] == PAL_PAINT(PAINT_BUBBLEGUM, 1)) tk_fb[y][x] = PAL_UI_RED;
        float drip = seg(ta, 41.5f, 42.5f) * 30, roll = seg(ta, 41.5f + 15 / 30.0f, 43.5f), back = seg(ta, 45.6f, 46.8f);
        int dx = 150; if (roll < 1 || back > 0) tk_rect(dx, 68, 3, (int)(roll < 1 ? drip * (1 - roll) : back * 26), PAL_UI_RED);
        if (roll > 0 && roll < 1) { int rx = (int)lerp(20, 220, roll); tk_rect(rx - 4, 50, 8, 26, PAL_PAINT(PAINT_BUBBLEGUM, 1)); tk_line(rx, 76, rx + 6, 100, PAL_PAINT(PAINT_HONEY, 1)); }
        return;
    }
    tk_clear(PAL_PAINT(PAINT_JET, 2));   /* MT-7: 13 f of silence, then the smallest bubble in the show */
    if (ta >= 47.5f) { bubble_side = 1; bubble("beep.", 5, 100, 80); bubble_side = 0; }
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) { line_at("mt7_pet_01", 47.5); }
