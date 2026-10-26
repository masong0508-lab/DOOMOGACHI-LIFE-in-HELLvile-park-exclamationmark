/* sc40.c - SC40 (3-01) ACT CARD / FUN FAIR - NIGHT (CUTE MODE). Bedtime!
 * crossed out. A flat side view: Sunny marches right with the bubble wand,
 * the pet on their head, conducting bubbles that swallow demons; pops to
 * hearts. Two TRUTH 4 inserts (no child). Steamy hooks a bubbled demon:
 * "...Nope. Don't know that one either." Blub. On past FREE HUGS to the tent. */
#include "act2_common.h"
#define SHOTS(X) X(main, 40.0)
#include "scene_tl.h"
#define BEAT (60.0f / 140.0f)
static const float W1 = 3.0f, TR1 = 7.6f, W2 = 12.0f, TR2 = 16.4f, T_DUCK = 19.5f, ST1 = 23.6f, T_BLUB = 26.4f, W3 = 28.0f, T_END = 36.0f;
static const Cut C[] = { { 0, 0, 0, 1, 1, 0, 1 }, { W1, 0, -24, 2.2f, 2.3f, 0, 0 }, { 6.0f, 0, -20, 3.4f, 3.7f, 0, 0 }, { W2, 0, -24, 2.0f, 2.2f, 0, 0 },
    { T_DUCK, 0, -14, 3.0f, 3.4f, 0, 2 }, { ST1 - 0.2f, 0, -18, 4.2f, 4.6f, 0, 2 }, { W3, 0, -26, 1.6f, 1.8f, 0, 0 }, { 33.0f, 0, -30, 1.3f, 1.4f, 0, 0 } };
static float sunny_x(float ta) { return (ta - W1) * 16; }   /* marching right at a constant pace */
static void cute_bg(float ta, float cx) {
    static const uint8_t c[] = { PAL_PAINT(PAINT_LAVENDER, 2), PAL_PAINT(PAINT_LAVENDER, 1) };
    tk_vgrad(0, 0, TW, TH, c, 2);
    wd_rect(cx - 400, 0, 800, 100, PAL_RAMP(RAMP_GRASS, 5), 0);
    wd_rect(cx - 400, 0, 800, 2, PAL_RAMP(RAMP_GRASS, 6), 0);
    for (int k = -6; k < 30; k++) { float x = k * 70.0f; if (x < cx - 300 || x > cx + 300) continue;
        int kind = (k % 4 + 4) % 4; uint8_t col = kind == 0 ? PAL_PAINT(PAINT_MINT, 2) : kind == 1 ? PAL_PAINT(PAINT_ROSE, 2) : kind == 2 ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_SKY, 2);
        fair_stall(x, 40, col, NULL, 0); }
    if (cx > 260) { wd_rect(400, -20, 40, 20, PAL_PAINT(PAINT_ROSE, 2), INK); tk_text((int)wd_sx(402), (int)wd_sy(-18), "FREE HUGS", PAL_PAINT(PAINT_MAGENTA, 0), 1); }
    if (cx > 400) fair_tent(560, 70, 60, PAL_PAINT(PAINT_MINT, 1));
}
static void bubbles(float ta, float sx, float t0, float t1) {
    if (ta < t0 || ta > t1 + 2) return;
    for (int i = 0; i < 10; i++) {
        float born = t0 + i * BEAT * 2; if (ta < born) continue;
        float a = ta - born, x = sx + 12 + a * 28, y = -16 - 6 * sinf(a * 2 + i);
        float pop = born + 2 * BEAT;
        if (ta < pop) { soap(x, y, 6, PAL_PAINT(PAINT_SKY, 2)); wd_blob(x - 2, y - 2, 1.2f, 1.2f, 0, PAL_WHITE, 0); if (i % 2 == 0) puff_demon(x, y + 2, 0.8f, ta * 30, -1); }
        else if (ta < pop + 0.8f) { tk_heart((int)wd_sx(x) - 3, (int)wd_sy(y) + (int)((ta - pop) * 30), 1, PAL_PAINT(PAINT_ROSE, 2)); }
    }
}
static void march(float ta, float sx) {
    Sunny u; sunny_default(&u, sx, 0, 0.8f, ta * 30); u.suitcase = 0; u.no_page = 1;
    float bt = (ta - W1) / BEAT; u.page = 0.4f + 0.35f * fabsf(sinf(bt * 3.14159f));   /* the wand on the beat, like a conductor */
    u.squash = 1 - 0.04f * fabsf(sinf(bt * 3.14159f)); u.y -= fabsf(sinf(bt * 3.14159f)) * 1.2f;
    sunny_draw(&u);
    wd_tube(sunny_hand_x, sunny_hand_y, sunny_hand_x + 5, sunny_hand_y - 6, 0.5f, 0.4f, INK, PAL_PAINT(PAINT_LEMON, 0), PAL_PAINT(PAINT_LEMON, 1), PAL_WHITE);
    soap(sunny_hand_x + 6, sunny_hand_y - 7.5f, 2, PAL_PAINT(PAINT_SKY, 2));
    pet_draw(sx, -23 * 0.8f - 4, 0.75f, ta * 30, ta >= TR1 + 0.5f && ta < TR1 + 1.5f ? -1 : 0, 0);
    tk_dither_rect((int)wd_sx(sx + 10) - 30, (int)wd_sy(-16) - 20, 60, 40, PAL_WHITE, 1);   /* the bubbles glow: the only light */
}
static void truth_insert(int which) {
    tk_clear(PAL_PAINT(PAINT_NAVY, 0)); wd_cam(0, -10, 4);
    if (!which) { for (int k = 0; k < 12; k++) wd_blob(cosf(k) * 8, -10 + sinf(k) * 6, 3, 2, 0, PAL_PAINT(PAINT_CHERRY, k & 1), 0); wd_blob_ol(0, -2, 4, 4.6f, 0.3f, PAL_PAINT(PAINT_CHERRY, 0), INK, WB_SHADE); wd_tube(1, -6, 3, -10, 1, 0.8f, INK, PAL_PAINT(PAINT_CHERRY, 0), PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_CHERRY, 2)); }
    else { wd_blob_ol(0, -10, 9, 9, 0, PAL_PAINT(PAINT_CHERRY, 0), PAL_PAINT(PAINT_SKY, 2), 0); wd_line(-9, -10, 9, -10, 0.6f, PAL_PAINT(PAINT_NAVY, 0)); }
}
static void steamy_tub(float ta) {
    wd_cam(0, -14, ta < ST1 - 0.2f ? lerp(3.0f, 3.4f, seg(ta, T_DUCK, ST1)) : lerp(4.2f, 4.6f, seg(ta, ST1, W3)));
    cute_bg(ta, 0);
    wd_blob_ol(0, 0, 24, 6, 0, PAL_PAINT(PAINT_SKY, 1), INK, 0);
    float sink = seg(ta, T_BLUB, T_BLUB + 0.8f);
    float hx = -4, hy = -2 + sink * 5;
    for (int k = -1; k <= 1; k += 2) { wd_blob_ol(hx + k * 4, hy - 4, 2.8f, 2.6f, 0, PAL_PAINT(PAINT_MINT, 1), INK, 0); wd_blob(hx + k * 4, hy - 4.2f, 1.6f, 1.6f, 0, PAL_WHITE, 0); wd_blob(hx + k * 4 + 0.6f, hy - 4.2f, 0.8f, 0.9f, 0, INK, 0); }
    wd_blob_ol(hx, hy, 7, 4, 0, PAL_PAINT(PAINT_MINT, 1), INK, 0);
    wd_blob(hx, hy + 1.6f, 4, 0.4f + 0.8f * (ta >= ST1 && ta < ST1 + 2 ? fabsf(sinf(ta * 12)) : 0), 0, PAL_PAINT(PAINT_CHERRY, 0), 0);
    wd_blob_ol(hx, hy - 9, 2.6f, 1.8f, 0, PAL_PAINT(PAINT_LEMON, 1), INK, 0);   /* a duck balanced on her head */
    wd_rect(-4 - 26, 2, 52, 6, PAL_PAINT(PAINT_SKY, 2), 0);
    wd_blob_ol(18, -1, 3, 2, 0, PAL_PAINT(PAINT_LEMON, 1), INK, 0);             /* the duck nobody knows, facing the Wall */
    float lift = seg(ta, T_DUCK + 1.2f, T_DUCK + 2.2f);
    float bx = lerp(-40, 8, seg(ta, T_DUCK, T_DUCK + 1.4f)), by = lerp(-12, -18, lift);
    if (ta < T_BLUB) { wd_line(8, 4, bx, by + 3, 0.6f, PAL_PAINT(PAINT_CARAMEL, 0)); soap(bx, by, 6, PAL_PAINT(PAINT_SKY, 2)); puff_demon(bx, by + 2, 0.8f, 0, -1); }
    else if (ta < T_BLUB + 0.6f) { tk_heart((int)wd_sx(bx) - 3, (int)wd_sy(by), 1, PAL_PAINT(PAINT_ROSE, 2)); wd_blob(bx + 2, by, 2, 2, 0, PAL_WHITE, WB_OUT); }
    say_at_world("...Nope. Don't know that one either.", ta, ST1, 2.2f, hx + 6, hy - 2, 1);
    if (ta >= T_BLUB + 0.6f && ta < T_BLUB + 1.2f) tk_text((int)wd_sx(hx) - 6, (int)wd_sy(hy) - 30, "blub", PAL_WHITE, 1);
}
void scene_main(int f, int n) {
    float ta = f / 30.0f; (void)n;
    int k = cut_at(C, 8, ta, 40);
    if (k == 1) {
        tk_clear(PAL_PAINT(PAINT_CARAMEL, 1)); tk_rect(40, 10, 160, 150, PAL_UI_CREAM); tk_frame(40, 10, 160, 150, PAL_PAINT(PAINT_CHOCOLATE, 1));
        static const char *L[5] = { "18:00 Welcome!", "19:00 Bake-Off!", "20:00 Bandstand!", "21:00 (we don't do that)", "22:00 Bedtime!" };
        for (int i = 0; i < 5; i++) tk_text(56, 32 + i * 22, L[i], PAL_PAINT(PAINT_NAVY, 1), 1);
        for (int s = 0; s < 3; s++) { float w = seg(ta, 0.5f + s * 0.25f + (s ? 0.17f : 0), 0.65f + s * 0.25f + (s ? 0.17f : 0)) * 56; tk_line(98, 125 + s * 2, 98 + (int)w, 121 + s * 2 + s, PAL_PAINT(PAINT_CHERRY, 0)); }
        return;
    }
    if ((ta >= TR1 && ta < TR1 + 4 / 30.0f) || (ta >= TR2 && ta < TR2 + 4 / 30.0f)) { truth_insert(ta >= TR2); return; }
    if (k == 2) { steamy_tub(ta); return; }
    float sx = sunny_x(ta < T_DUCK ? ta : ta - (W3 - T_DUCK));
    wd_cam(sx + (ta >= 33 ? 60 : 30) + (wd_cx - 0), wd_cy, wd_z);
    cute_bg(ta, sx);
    if (ta >= W2 && ta < T_DUCK) {   /* the teacups, still turning backwards; the red one still red */
        float tx = sunny_x(W2) + 70; wd_blob_ol(tx, 0, 30, 6, 0, PAL_PAINT(PAINT_LAVENDER, 1), INK, 0);
        for (int c = 0; c < 3; c++) { float a = -ta * 2 + c * 2.1f; wd_quad(tx + cosf(a) * 20 - 6, -8, tx + cosf(a) * 20 + 6, -8, tx + cosf(a) * 20 + 5, 0, tx + cosf(a) * 20 - 5, 0, c == 0 ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_PAINT(PAINT_MINT, 2), INK); }
    }
    if (ta < 6.5f) {   /* behind Sunny at the start: Rivet's prize case, open, glass cracked, velvet empty */
        float cx = 24;
        wd_rect(cx - 12, -14, 24, 14, PAL_RAMP(RAMP_STEEL, 3), INK); for (int r = 0; r < 5; r++) wd_blob(cx - 10 + r * 5, -13, 0.6f, 0.6f, 0, PAL_RAMP(RAMP_STEEL, 6), WB_OUT);
        wd_rect(cx - 10, -12, 20, 8, PAL_PAINT(PAINT_GRAPE, 0), 0);
        wd_quad(cx - 12, -14, cx + 12, -14, cx + 10, -30, cx - 14, -30, PAL_RAMP(RAMP_STEEL, 2), INK);   /* the lid, up */
        wd_line(cx - 4, -12, cx + 2, -8, 0.3f, PAL_WHITE); wd_line(cx + 2, -8, cx - 1, -5, 0.3f, PAL_WHITE); wd_line(cx + 2, -8, cx + 7, -7, 0.3f, PAL_WHITE);   /* the crack */
    }
    if (ta >= W3 + 2) {   /* ahead, the demons pile onto the bake tent */
        { float tx0 = sx + 120; fair_tent(tx0, 70, 60, PAL_PAINT(PAINT_MINT, 1)); }
        for (int i = 0; i < 9; i++) { float t0 = W3 + 2 + i * 0.5f; if (ta < t0) continue; float u = seg(ta, t0, t0 + 1.2f);
            float tx = sx + 120; float x = lerp(tx - 60 + i * 6, tx + (i % 3 - 1) * 10, u), y = lerp(-2, -64 - (i / 3) * 10, u) - 14 * sinf(u * 3.14f); puff_demon(x, y, 1.0f, ta * 30 + i * 7, 1); }
    }
    march(ta, sx);
    float t0 = ta < W2 ? W1 + 0.5f : ta < T_DUCK ? W2 + 0.3f : W3 + 0.2f;
    bubbles(ta, sx, t0, t0 + 6);
    if (ta >= TR1 + 4 / 30.0f && ta < W2) { wd_blob(sunny_x(TR1) + 30, -2, 1.6f + 0.4f * fabsf(sinf(ta * 8)), 1.6f, 0, PAL_PAINT(PAINT_CHERRY, 0), 0); }   /* one heart is beating */
    if (ta >= TR2 + 4 / 30.0f && ta < T_DUCK) for (int i = 0; i < 3; i++) soap(sx + 16 + i * 9, -18 - i, 4, PAL_PAINT(PAINT_ROSE, 2));   /* the bubbles come out faintly pink */
    
}
void scene_main_audio(double t0, double dur) { (void)t0; (void)dur; }
void tk_music(void) {
    tk_note(0, 0.3, 84, TKV_SINE, 0.3f); tk_note(0.12, 0.3, 79, TKV_SINE, 0.25f); for (int s = 0; s < 3; s++) tk_slide(0.5 + s * 0.42, 0.15, 72, 76, TKV_NOISE, 0.1f);
    static const int H[8] = { 84, 88, 91, 88, 89, 93, 91, 0 };       /* the cute HOOK: glockenspiel + music box */
    for (int i = 0; ; i++) { double t = W1 + i * BEAT / 2; if (t >= 39) break; if ((t >= TR1 && t < TR1 + 0.14) || (t >= TR2 && t < TR2 + 0.14)) continue; if (H[i % 8]) { tk_note(t, 0.2, H[i % 8], TKV_SINE, 0.12f); tk_note(t, 0.08, H[i % 8] + 12, TKV_TRI, 0.04f); } }
    for (double t = W1 + 2 * BEAT; t < 34; t += 2 * BEAT) { if (t >= T_DUCK && t < W3) continue; tk_slide(t, 0.05, 90, 96, TKV_SINE, 0.08f); }   /* pops on the beat */
    tk_slide(TR1, 0.13, 40, 50, TKV_NOISE, 0.4f); tk_note(TR1 + 0.2, 0.15, 40, TKV_SINE, 0.3f); tk_note(TR1 + 0.45, 0.15, 40, TKV_SINE, 0.3f);   /* wet pop; ba-dum */
    tk_slide(TR2, 0.13, 40, 50, TKV_NOISE, 0.4f); tk_note(TR2 + 0.2, 0.05, 92, TKV_SINE, 0.15f);
    babble_line(ST1, 2.2f, 68, 0.2f); tk_slide(T_BLUB + 0.6, 0.3, 60, 48, TKV_SINE, 0.15f);
}
