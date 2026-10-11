/* s05_creator.c - the citizen creator.
 * A cute editor window: tabs, sliders, colour swatches and a big chibi
 * preview whose parts cycle FAST (hair, colours, horns, wings, halo, hats,
 * cameos from the cast). "Create ANYONE." ... "Literally anyone."
 * The cursor clicks EYES [+]. The preview grows a third eye. Its name is
 * still Kevin. "Yes, three eyes." */
#include "tk.h"
#include <math.h>
#include <string.h>

#define PX 53            /* preview feet x */
#define PY 119           /* preview feet y */
#define FINAL_F 100      /* cycling stops */
#define EYE_F 110        /* third eye appears */

static const char *STYLE_NAMES[9] = { "BALD", "SHORT", "SPIKY", "BOB", "LONG", "TWINTAIL", "AFRO", "MOHAWK", "BUN" };

static TkChibi final_look(void) {
    TkChibi c = { PAL_SKIN(SKIN_PEACH, 1), PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_MINT, 1),
                  PAL_PAINT(PAINT_SKY, 1), TKH_AFRO, 0, 0, 0, 0, 1, 0 };
    return c;
}

/* look number k in the cycle: gets more absurd as k grows */
static TkChibi look(int k, int *tab) {
    const TkChibi *cast[] = { &TK_RIVET, &TK_VEX, &TK_FUZZ, &TK_IMP, &TK_GOTH, &TK_MAYOR, &TK_NANA, &TK_PIP };
    TkChibi c;
    if (k % 7 == 3) c = *cast[(k / 7) % 8];
    else c = tk_random_citizen((uint32_t)(k * 13 + 5), k > 12 && (k & 1));
    *tab = k % 6;
    if (k > 12) {             /* "Literally anyone." escalation */
        c.horns = (uint8_t)(k % 3);
        c.wings = (uint8_t)((k / 2) % 3);
        c.halo = (k % 4) == 1;
        c.hat = (uint8_t)(k % 4);
        if (c.hat == 3 && (k & 1)) c.hat = 2;
        if (k % 5 == 0) { c.skin = PAL_PAINT(PAINT_SLIME, 1); c.hair = PAL_PAINT(PAINT_HELLFIRE, 1); }
        if (k % 9 == 4) { c.skin = PAL_PAINT(PAINT_SKY, 1); c.hair_style = TKH_MOHAWK; c.halo = 1; c.horns = 2; }
    } else if (k > 5) {
        c.horns = k % 4 == 2; c.wings = k % 5 == 1 ? 2 : 0; c.hat = k % 6 == 4 ? 1 : 0;
    }
    return c;
}

/* ---- widgets -------------------------------------------------------------------- */
static void label(int x, int y, const char *s) { tk_text(x, y, s, PAL_UI_PINK_D, 1); }

static void slider(int x, int y, int w, float v, int heart) {
    tk_rect(x, y + 3, w, 3, PAL_UI_PINK_D);
    tk_rect(x + 1, y + 4, w - 2, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
    int fill = (int)((w - 2) * v);
    tk_rect(x + 1, y + 4, fill, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    int kx = x + fill;
    if (heart) tk_heart(kx, y + 4, 1, PAL_PAINT(PAINT_CHERRY, 1));
    else {
        tk_rect(kx - 2, y, 5, 9, PAL_UI_PINK_D);
        tk_rect(kx - 1, y + 1, 3, 7, PAL_WHITE);
    }
}

static void cursor(int x, int y, int click) {
    static const char *AR[] = {
        "k.......",
        "kk......",
        "kwk.....",
        "kwwk....",
        "kwwwk...",
        "kwwwwk..",
        "kwwwwwk.",
        "kwwwkkkk",
        "kwkwk...",
        "kk.kwk..",
        "k...kwk.",
        "....kk..", NULL };
    tk_sprite(AR, x, y, NULL, 1, 0);
    if (click > 0 && click < 6) tk_ring(x, y, 2 + click, click < 3 ? PAL_WHITE : PAL_UI_PINK_D);
}

/* cursor path: list of (frame, x, y) keyframes, eased between */
static void cursor_pos(int f, int *x, int *y) {
    static const int K[][3] = {
        { 0, 250, 150 }, { 10, 168, 50 }, { 24, 196, 52 }, { 36, 150, 64 }, { 46, 182, 66 },
        { 58, 186, 78 }, { 70, 162, 92 }, { 82, 206, 66 }, { 94, 150, 102 }, { 102, 197, 115 },
        { 110, 197, 115 }, { 122, 222, 128 }, { 150, 224, 130 } };
    int n = (int)(sizeof K / sizeof K[0]);
    for (int i = 0; i < n - 1; i++)
        if (f >= K[i][0] && f < K[i + 1][0]) {
            float t = tk_ease((float)(f - K[i][0]) / (float)(K[i + 1][0] - K[i][0]));
            *x = tk_lerpi(K[i][1], K[i + 1][1], t);
            *y = tk_lerpi(K[i][2], K[i + 1][2], t);
            return;
        }
    *x = K[n - 1][1]; *y = K[n - 1][2];
}

/* draw the third eye on a size-s chibi standing at (x, yf) */
static void third_eye(int x, int yf, int s, int bob, uint8_t iris, uint8_t skin, int closed, int age) {
    int b = (bob / 4) & 1;
#define EX(lx) (x + ((lx) - 9) * s)
#define EY(ly) (yf + ((ly) - 27 + b) * s)
    /* parted forehead */
    for (int ly = 2; ly <= 6; ly++)
        for (int lx = 7; lx <= 11; lx++) {
            int corner = (ly == 2 && (lx == 7 || lx == 11));
            if (!corner) tk_rect(EX(lx), EY(ly), s, s, skin);
        }
    if (age < 2) { /* pop-in flash */
        tk_rect(EX(7), EY(3), 5 * s, 4 * s, PAL_WHITE);
        return;
    }
    if (closed) { tk_rect(EX(8), EY(5), 3 * s, s, PAL_BLACK); return; }
    tk_rect(EX(8), EY(3), 3 * s, 4 * s, PAL_BLACK);
    tk_rect(EX(8), EY(5), 3 * s, 2 * s, iris);
    tk_rect(EX(8), EY(3), s, 2 * s, PAL_WHITE);
    tk_rect(EX(9), EY(6), s, s, PAL_PAINT(PAINT_SKY, 2));
#undef EX
#undef EY
}

void scene_s05_creator(int f, int n) {
    (void)n;
    /* background: mint stripes scrolling */
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++)
            tk_fb[y][x] = (((x + y + f) / 10) & 1) ? PAL_RAMP(RAMP_MINT, 7) : PAL_RAMP(RAMP_MINT, 6);

    /* window slides up with a bounce */
    int oy = 0;
    if (f < 10) { static const int B[10] = { 150, 100, 55, 20, -6, -8, -3, 2, 1, 0 }; oy = B[f]; }
    int wx = 4, wy = 3 + oy, ww = 232, wh = 126;
    tk_window(wx, wy, ww, wh, TKW_CUTE);
    tk_heart(wx + 10, wy + 7, 1, PAL_WHITE);
    tk_text(wx + 16, wy + 4, "CITIZEN CREATOR", PAL_WHITE, 1);
    tk_text(wx + ww - 50, wy + 4, "#0042", PAL_UI_PINK_D, 1);

    /* which look are we on? */
    int k, tab = 0;
    if (f < 12) k = 0;
    else if (f < 58) k = (f - 12) / 4;
    else if (f < FINAL_F) k = 12 + (f - 58) / 3;
    else k = -1;
    TkChibi c = k < 0 ? final_look() : look(k, &tab);
    if (k < 0) tab = 2;

    /* tabs */
    static const char *TABS[6] = { "HAIR", "FACE", "EYES", "HORN", "WING", "HATS" };
    for (int i = 0; i < 6; i++) {
        int tx = wx + 6 + i * 37, ty = wy + 14;
        int act = i == tab;
        tk_rect(tx, ty + (act ? 0 : 2), 35, act ? 12 : 10, PAL_UI_PINK_D);
        tk_rect(tx + 1, ty + (act ? 1 : 3), 33, act ? 11 : 8, act ? PAL_UI_CREAM : PAL_RAMP(RAMP_PASTEL_PINK, 6));
        tk_text(tx + 6, ty + (act ? 3 : 4), TABS[i], act ? PAL_UI_PINK_D : PAL_WHITE, 1);
    }
    /* content panel */
    int cy0 = wy + 26;
    tk_rect(wx + 4, cy0, ww - 8, wh - 30, PAL_UI_CREAM);
    tk_rect(wx + 4, cy0, ww - 8, 1, PAL_UI_PINK_D);

    /* preview pane */
    int pvx = wx + 6, pvy = cy0 + 2, pvw = 86, pvh = wh - 34;
    static const uint8_t pg[] = { PAL_RAMP(RAMP_SKY, 7), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6) };
    tk_vgrad(pvx, pvy, pvw, pvh, pg, 3);
    tk_frame(pvx - 1, pvy - 1, pvw + 2, pvh + 2, PAL_UI_PINK_D);
    /* spinning spotlight sparkles */
    for (int i = 0; i < 6; i++) {
        float a = f * 0.09f + i * 1.047f;
        int sx = PX + (int)(cosf(a) * 34), sy = PY - 44 + (int)(sinf(a) * 8);
        tk_star(sx, sy + oy, ((f + i * 5) / 4) % 3, i & 1 ? PAL_WHITE : PAL_UI_GOLD);
    }
    tk_ellipse(PX, PY + oy, 30, 5, PAL_UI_PINK_D);
    tk_ellipse(PX, PY - 1 + oy, 29, 4, PAL_RAMP(RAMP_PASTEL_PINK, 5));
    tk_ellipse(PX - 4, PY - 2 + oy, 18, 2, PAL_RAMP(RAMP_PASTEL_PINK, 6));

    /* the citizen: a little "boing" squash on each part change */
    int changed = (f >= 12 && f < FINAL_F) ? ((f < 58) ? ((f - 12) % 4 == 0) : ((f - 58) % 3 == 0)) : 0;
    int pyo = changed ? 2 : 0;
    int expr = TKE_NORMAL, mouth = TKM_SMILE;
    if (k >= 0) {
        static const int E[] = { TKE_HAPPY, TKE_NORMAL, TKE_SPARKLE, TKE_SMUG, TKE_LOVE, TKE_NORMAL, TKE_ANGRY };
        static const int M[] = { TKM_OPEN, TKM_SMILE, TKM_CAT, TKM_SMILE, TKM_OPEN, TKM_O, TKM_FANGS };
        expr = E[k % 7]; mouth = M[k % 7];
    } else if (f >= EYE_F + 4 && f < EYE_F + 12) { expr = TKE_NORMAL; mouth = TKM_O; }
    int bob = f;
    /* clip region note: hats may poke above the pane - intentional */
    tk_chibi(PX, PY + pyo + oy, &c, 3, expr, mouth, bob, 0);
    if (k < 0 && f >= EYE_F) {
        int age = f - EYE_F;
        int closed = (f >= 128 && f < 132);                 /* only the third eye blinks */
        third_eye(PX, PY + oy, 3, bob, c.eyes, c.skin, closed, age);
        if (age < 16) tk_sparkles(PX, PY - 66 + oy, age, 5, 14);
    }

    /* right panel */
    int rx = wx + 98, ry = cy0 + 4;
    label(rx, ry, "NAME");
    tk_rect(rx + 30, ry - 2, 96, 11, PAL_UI_PINK_D);
    tk_rect(rx + 31, ry - 1, 94, 9, PAL_WHITE);
    tk_text(rx + 34, ry, "Kevin", PAL_GRAY_D, 1);
    if ((f / 8) & 1) tk_rect(rx + 34 + 30, ry - 1, 1, 8, PAL_GRAY_D);

    ry += 13;
    label(rx, ry, "STYLE");
    tk_text(rx + 36, ry, "<", PAL_UI_PINK_D, 1);
    tk_text(rx + 120, ry, ">", PAL_UI_PINK_D, 1);
    const char *sn = STYLE_NAMES[c.hair_style % 9];
    tk_text(rx + 81 - tk_text_w(sn, 1) / 2, ry, sn, PAL_GRAY_D, 1);

    ry += 13;
    label(rx, ry + 3, "COLOR");
    static const int SW[16] = { PAINT_WHITE, PAINT_JET, PAINT_CHERRY, PAINT_ROSE, PAINT_BUBBLEGUM, PAINT_TANGERINE,
                                PAINT_HONEY, PAINT_LEMON, PAINT_LIME, PAINT_MINT, PAINT_CYAN, PAINT_SKY, PAINT_COBALT,
                                PAINT_LAVENDER, PAINT_GRAPE, PAINT_CHOCOLATE };
    int sel = -1;
    for (int i = 0; i < 16; i++) if (PAL_PAINT(SW[i], 1) == c.hair) sel = i;
    if (sel < 0) sel = (k < 0 ? 13 : (k * 5) % 16);
    for (int i = 0; i < 16; i++) {
        int sx = rx + 36 + (i % 8) * 11, sy = ry - 2 + (i / 8) * 9;
        tk_rect(sx, sy, 10, 8, PAL_UI_PINK_D);
        tk_rect(sx + 1, sy + 1, 8, 6, PAL_PAINT(SW[i], 1));
        if (i == sel) { tk_frame(sx - 1, sy - 1, 12, 10, PAL_WHITE); tk_frame(sx - 2, sy - 2, 14, 12, PAL_UI_PINK_D); }
    }

    ry += 22;
    label(rx, ry, "HORNS");
    slider(rx + 36, ry - 1, 86, c.horns / 2.0f, 0);
    ry += 12;
    label(rx, ry, "CUTE");
    slider(rx + 36, ry - 1, 86, 1.0f, 1);
    ry += 12;
    label(rx, ry, "EVIL");
    float ev = k < 0 ? 0.08f : (c.horns ? 0.3f + 0.3f * c.horns : 0.1f) + (c.halo ? -0.1f : 0.0f);
    ev += 0.04f * sinf(f * 0.7f);
    slider(rx + 36, ry - 1, 86, tk_clampf(ev, 0.02f, 0.98f), 0);
    ry += 12;
    label(rx, ry, "EYES");
    int eyes = (f >= 106) ? 3 : 2;
    int pressed = (f >= 104 && f < 108);
    tk_rect(rx + 36, ry - 2, 11, 11, PAL_UI_PINK_D); tk_rect(rx + 37, ry - 1, 9, 9, PAL_RAMP(RAMP_PASTEL_PINK, 6));
    tk_text(rx + 39, ry, "-", PAL_WHITE, 1);
    char num[4] = { (char)('0' + eyes), 0, 0, 0 };
    int ny = (f >= 106 && f < 109) ? -2 : 0;
    tk_text_outline(rx + 58, ry + ny, num, eyes == 3 ? PAL_UI_GOLD : PAL_GRAY_D, eyes == 3 ? PAL_UI_PINK_D : PAL_UI_CREAM, 1);
    tk_rect(rx + 72, ry - 2 + pressed, 11, 11, PAL_UI_PINK_D);
    tk_rect(rx + 73, ry - 1 + pressed, 9, 9, pressed ? PAL_UI_PINK_D : PAL_PAINT(PAINT_BUBBLEGUM, 1));
    tk_text(rx + 75, ry + pressed, "+", PAL_WHITE, 1);
    if (f >= 106) tk_text(rx + 88, ry, "(?!)", PAL_GRAY, 1);

    /* cursor */
    int cx, cyy;
    cursor_pos(f, &cx, &cyy);
    int click = 0;
    static const int CLK[] = { 10, 24, 36, 46, 58, 70, 82, 94, 104 };
    for (unsigned i = 0; i < sizeof CLK / sizeof CLK[0]; i++) if (f >= CLK[i] && f < CLK[i] + 6) click = f - CLK[i];
    cursor(cx, cyy + oy, click);

    /* captions under the window */
    if (f >= 14 && f < 56) {
        int a = f - 14, dy = a == 0 ? -5 : a == 1 ? 1 : 0;
        tk_caption(145 + dy, "Create ANYONE.", PAL_WHITE, PAL_UI_PINK_D, 2);
    }
    if (f >= 60 && f < 102) {
        int a = f - 60, dy = a == 0 ? -5 : a == 1 ? 1 : 0;
        tk_caption(145 + dy, "Literally anyone.", PAL_WHITE, PAL_UI_PINK_D, 2);
    }
    if (f >= EYE_F + 6) {
        int a = f - EYE_F - 6, dy = a == 0 ? -5 : a == 1 ? 1 : 0;
        tk_caption(145 + dy, "Yes, three eyes.", PAL_WHITE, PAL_UI_PINK_D, 2);
    }
    if (f >= EYE_F && f < EYE_F + 2) tk_post_flash(PAL_WHITE, 6);
}

void scene_s05_creator_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    tk_sfx(t0 + 2 * fr, SFX_WHOOSH, 0.5f);
    tk_sfx(t0 + 6 * fr, SFX_POP, 0.5f);
    /* part changes: a rising scale of blips, faster in the second half */
    for (int i = 0; i < 12; i++) tk_note(t0 + (12 + i * 4) * fr, 0.035, 72 + (i * 5) % 12, TKV_SQ25, 0.2f);
    for (int i = 0; i < 14; i++) tk_note(t0 + (58 + i * 3) * fr, 0.03, 79 + (i * 7) % 12, TKV_SQ25, 0.2f);
    static const int CLK[] = { 10, 24, 36, 46, 58, 70, 82, 94 };
    for (int i = 0; i < 8; i++) tk_note(t0 + CLK[i] * fr, 0.015, 96, TKV_NOISE, 0.15f);
    tk_sfx(t0 + 104 * fr, SFX_SELECT, 0.6f);
    tk_sfx(t0 + EYE_F * fr, SFX_POP, 1.0f);
    tk_sfx(t0 + (EYE_F + 1) * fr, SFX_SPARKLE, 0.6f);
    tk_sfx(t0 + (EYE_F + 6) * fr, SFX_FANFARE, 0.45f);
    tk_note(t0 + 128 * fr, 0.05, 91, TKV_SINE, 0.25f);   /* the lone blink: plink */
}
