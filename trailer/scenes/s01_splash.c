/* s01_splash.c - "a HaHaland production".
 * A chubby pastel heart drops in, boings, smiles. "100% demon-free!"
 * ...and for exactly three frames a little devil tail flicks out from
 * behind it. The heart side-eyes the audience. Nothing happened. */
#include "tk.h"
#include <math.h>
#include <string.h>

/* ---- the heart (implicit curve, squash & stretch, with a face) ----------- */
enum { HF_HAPPY, HF_OPEN, HF_SIDE, HF_SIDE_SWEAT };

static int heart_in(float u, float v) {
    float a = u * u + v * v - 1.0f;
    return a * a * a - u * u * v * v * v <= 0.0f;
}
/* bottom tip at (cx, tip_y); w/h are half sizes */
static void big_heart(int cx, int tip_y, float w, float h, int face, int f) {
    static uint8_t m[TH][TW];
    int cy = tip_y - (int)h;
    int x0 = cx - (int)w - 3, x1 = cx + (int)w + 3, y0 = cy - (int)h - 3, y1 = tip_y + 2;
    uint8_t fill = PAL_PAINT(PAINT_BUBBLEGUM, 1), dark = PAL_PAINT(PAINT_ROSE, 1),
            lite = PAL_PAINT(PAINT_BUBBLEGUM, 2), line = PAL_RAMP(RAMP_PASTEL_PINK, 1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if ((unsigned)x >= TW || (unsigned)y >= TH) continue;
            float u = (x - cx) / w * 1.14f, v = 0.12f - (y - cy) / h * 1.12f;
            m[y][x] = (uint8_t)heart_in(u, v);
        }
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if ((unsigned)x >= TW || (unsigned)y >= TH) continue;
            if (m[y][x]) {
                float u = (x - cx) / w * 1.14f, v = 0.12f - (y - cy) / h * 1.12f;
                uint8_t c = fill;
                if (u * 0.55f - v * 0.85f > 0.62f) c = dark;
                else if (u * 0.55f - v * 0.85f > 0.48f && ((x + y) & 1)) c = dark;
                float hu = u + 0.55f, hv = v - 0.62f;
                if (hu * hu * 3.0f + hv * hv * 7.0f < 0.10f) c = PAL_WHITE;
                else if (hu * hu * 3.0f + hv * hv * 7.0f < 0.22f) c = lite;
                tk_px(x, y, c);
            } else {
                int n = 0;
                if (y > 0 && m[y - 1][x]) n = 1;
                if (y < TH - 1 && m[y + 1][x]) n = 1;
                if (x > 0 && m[y][x - 1]) n = 1;
                if (x < TW - 1 && m[y][x + 1]) n = 1;
                if (n && y >= y0 && y <= y1) tk_px(x, y, line);
            }
        }
    /* clear the mask for next time */
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if ((unsigned)x < TW && (unsigned)y < TH) m[y][x] = 0;

    /* face */
    float sx = w / 34.0f, sy = h / 30.0f;
    int ey = cy + (int)(2 * sy), exl = cx - (int)(11 * sx), exr = cx + (int)(11 * sx);
    uint8_t k = PAL_RAMP(RAMP_PASTEL_PINK, 0);
    /* blush */
    tk_ellipse(exl - (int)(5 * sx), ey + (int)(7 * sy), 4, 2, PAL_PAINT(PAINT_ROSE, 2));
    tk_ellipse(exr + (int)(5 * sx), ey + (int)(7 * sy), 4, 2, PAL_PAINT(PAINT_ROSE, 2));
    for (int e = 0; e < 2; e++) {
        int ex = e ? exr : exl;
        switch (face) {
        case HF_HAPPY: /* ^ ^ */
            tk_px(ex - 3, ey + 1, k); tk_px(ex - 2, ey, k); tk_px(ex - 1, ey - 1, k);
            tk_px(ex, ey - 1, k); tk_px(ex + 1, ey, k); tk_px(ex + 2, ey + 1, k);
            tk_px(ex - 2, ey + 1, k); tk_px(ex + 1, ey + 1, k);
            break;
        case HF_OPEN:
            tk_rect(ex - 2, ey - 3, 4, 6, k);
            tk_rect(ex - 2, ey - 3, 2, 2, PAL_WHITE);
            break;
        default: { /* side-eye: white eyeball, pupil hard right */
            tk_rect(ex - 4, ey - 3, 9, 7, PAL_WHITE);
            tk_frame(ex - 5, ey - 4, 11, 9, k);
            tk_rect(ex + 1, ey - 1, 3, 4, k);
            tk_rect(ex - 4, ey - 3, 9, 2, fill); /* heavy, suspicious lid */
            tk_rect(ex - 5, ey - 2, 11, 1, k);
            break; }
        }
    }
    /* mouth */
    int my = ey + (int)(8 * sy);
    if (face == HF_HAPPY || face == HF_OPEN) {
        tk_rect(cx - 3, my, 7, 2, k);
        tk_rect(cx - 2, my + 2, 5, 2, k);
        tk_rect(cx - 1, my + 2, 3, 1, PAL_PAINT(PAINT_CHERRY, 1));
    } else {
        /* tight little "nothing happened" line */
        tk_rect(cx - 2, my + 1, 5, 1, k);
        tk_px(cx + 3, my, k);
    }
    if (face == HF_SIDE_SWEAT) {
        int sx0 = cx - (int)(26 * sx), sy0 = cy - (int)(12 * sy) + (f % 8 < 4 ? 0 : 1);
        uint8_t b = PAL_PAINT(PAINT_SKY, 2), bd = PAL_PAINT(PAINT_SKY, 1);
        tk_px(sx0, sy0, bd);
        tk_rect(sx0 - 1, sy0 + 1, 3, 2, b);
        tk_rect(sx0 - 1, sy0 + 3, 3, 1, bd);
        tk_px(sx0, sy0 + 1, PAL_WHITE);
    }
}

/* the forbidden tail: base hidden behind the heart, spade tip */
static void devil_tail(int cx, int cy, int stage) {
    uint8_t r = PAL_PAINT(PAINT_CHERRY, 1), rd = PAL_PAINT(PAINT_CHERRY, 0), k = PAL_BLACK;
    /* polyline control points (relative), stage 0 peek, 1 full flick, 2 retract */
    static const int P[3][5][2] = {
        { {10, 6}, {22, 4}, {28, -2}, {30, -8}, {30, -8} },
        { {10, 6}, {26, 6}, {38, 0}, {42, -10}, {38, -18} },
        { {10, 6}, {20, 6}, {26, 2}, {28, -2}, {28, -2} },
    };
    const int (*p)[2] = P[stage];
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < 4; i++) {
            int ax = cx + p[i][0], ay = cy + p[i][1], bx = cx + p[i + 1][0], by = cy + p[i + 1][1];
            for (int t = 0; t <= 8; t++) {
                int x = ax + (bx - ax) * t / 8, y = ay + (by - ay) * t / 8;
                if (pass == 0) tk_rect(x - 2, y - 2, 5, 5, k);
                else { tk_rect(x - 1, y - 1, 3, 3, r); tk_px(x + 1, y + 1, rd); }
            }
        }
    int tx = cx + p[4][0], ty = cy + p[4][1];
    /* spade tip */
    static const char *SP[] = { "...k...", "..krk..", ".krrrk.", "krrrrrk", "krrRrrk", ".kkRkk.", "...k...", NULL };
    uint8_t mp[128] = {0};
    mp['k'] = k; mp['r'] = r; mp['R'] = rd;
    tk_sprite(SP, tx - 3, ty - 6, mp, 1, 0);
}

static void bg(int f) {
    tk_clear(PAL_UI_CREAM);
    /* drifting polka dots */
    for (int j = 0; j < 7; j++)
        for (int i = 0; i < 10; i++) {
            int x = i * 28 + (j & 1) * 14 - (f / 3) % 28, y = j * 26 + 6 - (f / 6) % 26;
            tk_circle(x, y, 2, PAL_RAMP(RAMP_PASTEL_PINK, 7));
            tk_px(x, y, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        }
    /* soft vignette band top & bottom */
    tk_dither_rect(0, 0, TW, 6, PAL_UI_PINK, 6);
    tk_dither_rect(0, TH - 6, TW, 6, PAL_UI_PINK, 6);
}

/* heart vertical motion: returns tip offset above rest (px) and squash */
static void heart_motion(int f, float *off, float *squash) {
    *off = 0; *squash = 0;
    if (f < 14) { float t = f / 14.0f; *off = 100 * (1 - t * t); *squash = -0.22f * t; return; }
    if (f < 17) { static const float sq[3] = { 0.30f, 0.22f, 0.08f }; *squash = sq[f - 14]; return; }
    if (f < 27) { float t = (f - 17) / 10.0f; *off = 22 * 4 * t * (1 - t); *squash = (t < .5f ? -0.10f : 0.0f); return; }
    if (f < 29) { *squash = f == 27 ? 0.16f : 0.06f; return; }
    if (f < 35) { float t = (f - 29) / 6.0f; *off = 6 * 4 * t * (1 - t); return; }
    if (f < 37) { *squash = 0.05f; return; }
    /* idle breathing */
    *squash = 0.025f * sinf(f * 0.25f);
}

void scene_s01_splash(int f, int n) {
    bg(f);
    const int cx = 120, rest = 72;
    float off, sq;
    heart_motion(f, &off, &sq);
    float w = 30 * (1 + sq), h = 26 * (1 - sq);
    int tip = rest - (int)off;

    /* floor shadow */
    if (off < 160) {
        int sr = 20 - (int)(off / 10);
        if (sr < 4) sr = 4;
        tk_ellipse(cx, rest + 2, sr + (int)(sq * 20), 2, PAL_RAMP(RAMP_PASTEL_PINK, 6));
    }

    if (f >= 14 && f < 44) tk_confetti(cx, rest - 2, f - 14, 5);

    /* THE TAIL (frames 52,53,54 only) */
    if (f >= 52 && f <= 54) devil_tail(cx, tip - 16, f - 52);

    int face = HF_HAPPY;
    if (f >= 3 && f < 14) face = HF_OPEN;
    if (f >= 56 && f < 60) face = HF_SIDE;
    if (f >= 60 && f < 66) face = HF_SIDE_SWEAT;
    big_heart(cx, tip, w, h, face, f);

    /* a few sparkles around the heart once it lands */
    if (f >= 15) tk_sparkles(cx, tip - 26, f - 15, 77, 46);

    /* "HaHaland" letters pop in one by one */
    static const char *word = "HaHaland";
    static const uint8_t cols[8] = {
        PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_LEMON, 1),
        PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_PEACH, 1),
        PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_MINT, 1) };
    int ww = tk_text_w(word, 3), x0 = 120 - ww / 2;
    for (int i = 0; i < 8; i++) {
        int age = f - (18 + i * 2);
        if (age < 0) continue;
        int dy = age < 2 ? -6 : age < 4 ? 2 : age < 5 ? -1 : 0;
        dy += (int)(1.5f * sinf((f + i * 4) * 0.2f) * (f > 40));
        char s[2] = { word[i], 0 };
        tk_text_outline(x0 + i * 18, 94 + dy, s, cols[i], PAL_RAMP(RAMP_PASTEL_PINK, 1), 3);
    }
    if (f >= 30) {
        uint8_t c = PAL_RAMP(RAMP_PASTEL_PINK, 2);
        tk_text_outline(x0 - 2, 82, "a", c, PAL_UI_CREAM, 1);
        tk_text_outline(x0 + ww - tk_text_w("production", 1) + 2, 125, "production", c, PAL_UI_CREAM, 1);
    }
    if (f >= 38) {
        const char *df = "(100% demon-free!)";
        int dy = f < 40 ? -2 : 0;
        tk_text(120 - tk_text_w(df, 1) / 2, 143 + dy, df, PAL_GRAY, 1);
    }

    /* fade in from white, out to cream-white */
    if (f < 5) tk_post_flash(PAL_WHITE, 16 - f * 3);
    if (f > n - 7) tk_post_flash(PAL_WHITE, (f - (n - 7)) * 2 + 2);
}

void scene_s01_splash_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    tk_slide(t0 + 3 * fr, 11 * fr, 84, 60, TKV_SINE, 0.35f);     /* falling whistle */
    tk_sfx(t0 + 14 * fr, SFX_BOING, 1.0f);
    tk_sfx(t0 + 27 * fr, SFX_POP, 0.5f);
    for (int i = 0; i < 8; i++) tk_note(t0 + (18 + i * 2) * fr, 0.05, 76 + i * 2, TKV_SQ25, 0.25f);
    tk_sfx(t0 + 35 * fr, SFX_SPARKLE, 0.6f);
    tk_sfx(t0 + 52 * fr, SFX_GLITCH, 0.35f);                      /* ...what was that */
    tk_note(t0 + 56 * fr, 0.04, 91, TKV_SQ12, 0.2f);              /* eyes: tik */
    tk_note(t0 + 60 * fr, 0.04, 86, TKV_SQ12, 0.2f);
}
