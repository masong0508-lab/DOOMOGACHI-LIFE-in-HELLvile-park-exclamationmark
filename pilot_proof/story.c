/* story.c - the crayon storybook first half of the cold open (script pages
 * 1-2): calendar, the one park, the bridge, Fluffy, the pluck, the Wall, the
 * banner rolled from MINGLE to GIGGLE. Children's-book crayon on paper grain,
 * boiling lines, supers typed on in a child's hand. No narrator. */
#include "tk.h"
#include "wd.h"
#include "sets.h"
#include <math.h>

#define AT(fr) (t0 + (fr) / 30.0)
#define PAPER  PAL_UI_CREAM
#define INK    PAL_PAINT(PAINT_NAVY, 1)
static float seg(float f, float a, float b) { return wd_seg(f, a, b); }
static float lerp(float a, float b, float u) { return a + (b - a) * u; }

/* a super in a child's hand: typed on, each letter wobbling on its own */
static void super(const char *s, int f, int f0, int f1, int y) {
    if (f < f0 || f >= f1) return;
    int n = (f - f0) * 2, len = 0;
    while (s[len]) len++;
    if (n > len) n = len;
    int w = len * 6, x = (TW - w) / 2;
    if (x < 4) x = 4;
    for (int i = 0; i < n; i++) {
        char ch[2] = { s[i], 0 };
        uint32_t h = tk_hash((uint32_t)i * 131u + (uint32_t)(f / 4) * 7u);
        tk_text(x + i * 6, y + (int)(h % 3) - 1, ch, i % 7 == 0 ? PAL_PAINT(PAINT_GRAPE, 1) : INK, 1);
    }
}
/* two-line super, for the longer sentences */
static void super2(const char *a, const char *b, int f, int f0, int f1) {
    super(a, f, f0, f1, 134);
    int la = 0; while (a[la]) la++;
    super(b, f - la / 2, f0, f1 - la / 2, 145);
}

/* --- crayon kids: cute filter, side-on, never faces for the named two --- */
static void bunny(float x, float y, float s, int dir, float t) {
    float hop = fabsf(sinf(t * 0.25f)) * 3 * s;
    y -= hop;
    wd_blob_ol(x, y, 5 * s, 5.5f * s, 0, PAL_WHITE, INK, WB_SHADE);
    wd_blob_ol(x - 1.5f * s, y - 9 * s, 1.6f * s, 4.5f * s, -0.15f, PAL_WHITE, INK, 0);
    wd_blob_ol(x + 1.5f * s, y - 9 * s, 1.6f * s, 4.5f * s, 0.15f, PAL_WHITE, INK, 0);
    wd_blob(x + 1.5f * s, y - 9 * s, 0.6f * s, 3 * s, 0.15f, PAL_PAINT(PAINT_ROSE, 2), 0);
    wd_blob(x + dir * 2.2f * s, y - 1.2f * s, 0.8f * s, 0.9f * s, 0, INK, 0);
    wd_blob(x + dir * 3.2f * s, y + 0.8f * s, 0.9f * s, 0.6f * s, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
}
static void imp(float x, float y, float s, int dir, float t, int back) {
    float hop = fabsf(sinf(t * 0.25f + 1)) * 3 * s;
    y -= hop;
    uint8_t c = PAL_PAINT(PAINT_GRAPE, 2);
    wd_line(x - dir * 4 * s, y + 2 * s, x - dir * 8 * s, y - 2 * s, 0.9f * s, INK);   /* tail */
    wd_tri(x - dir * 8 * s, y - 4 * s, x - dir * 10 * s, y - 1 * s, x - dir * 7 * s, y - 1 * s, INK);
    wd_blob_ol(x, y, 5 * s, 5.5f * s, 0, c, INK, WB_SHADE);
    wd_tri(x - 3 * s, y - 4 * s, x - 1.5f * s, y - 5 * s, x - 3.5f * s, y - 8.5f * s, PAL_PAINT(PAINT_BONE, 2));
    wd_tri(x + 3 * s, y - 4 * s, x + 1.5f * s, y - 5 * s, x + 3.5f * s, y - 8.5f * s, PAL_PAINT(PAINT_BONE, 2));
    if (!back) {
        wd_blob(x + dir * 2.2f * s, y - 1.2f * s, 0.8f * s, 0.9f * s, 0, INK, 0);
        wd_blob(x + dir * 3.2f * s, y + 0.8f * s, 0.9f * s, 0.6f * s, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
    }
}
static void cloud(float x, float y, float s, int blink, int hearts) {
    for (int k = 0; k < 6; k++) {
        float a = k * 1.047f;
        wd_blob_ol(x + cosf(a) * 7 * s, y + sinf(a) * 3.5f * s, 6 * s, 5 * s, 0, PAL_WHITE, PAL_PAINT(PAINT_SKY, 0), 0);
    }
    wd_blob(x, y, 9 * s, 5.5f * s, 0, PAL_WHITE, 0);
    for (int e = -1; e <= 1; e += 2) {
        float ex = x + e * 3 * s, ey = y - 0.5f * s;
        if (hearts) tk_heart((int)wd_sx(ex) - 3, (int)wd_sy(ey) - 3, 1, PAL_PAINT(PAINT_CHERRY, 1));
        else if (blink) wd_line(ex - 1.2f * s, ey, ex + 1.2f * s, ey, 0.6f * s, INK);
        else wd_blob(ex, ey, 1.0f * s, 1.4f * s, 0, INK, 0);
    }
    wd_blob(x - 5.5f * s, y + 2 * s, 1.4f * s, 0.8f * s, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
    wd_blob(x + 5.5f * s, y + 2 * s, 1.4f * s, 0.8f * s, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
}
/* the young cherub, from behind: curls, halo, tiny wings, no face */
static void cherub_back(float x, float y, float s) {
    wd_blob_ol(x, y, 7 * s, 8 * s, 0, PAL_PAINT(PAINT_SKY, 2), INK, WB_SHADE);         /* nightie */
    wd_blob_ol(x - 6 * s, y - 3 * s, 3.5f * s, 2.2f * s, 0.6f, PAL_WHITE, INK, 0);      /* wings */
    wd_blob_ol(x + 6 * s, y - 3 * s, 3.5f * s, 2.2f * s, -0.6f, PAL_WHITE, INK, 0);
    wd_blob_ol(x, y - 11 * s, 5.5f * s, 5 * s, 0, PAL_PAINT(PAINT_LEMON, 2), INK, 0);  /* curls */
    for (int k = -2; k <= 2; k++) wd_blob(x + k * 2.2f * s, y - 13 * s, 1.4f * s, 1.4f * s, 0, PAL_PAINT(PAINT_HONEY, 2), 0);
    wd_blob_ol(x, y - 18.5f * s, 4 * s, 1.1f * s, 0, PAPER, PAL_PAINT(PAINT_HONEY, 1), 0);   /* halo */
}
static void bowl(float x, float y, float s) {
    wd_blob_ol(x, y, 5 * s, 2.6f * s, 0, PAL_PAINT(PAINT_TANGERINE, 2), INK, WB_SHADE);
    wd_blob(x, y - 1.6f * s, 4.2f * s, 0.9f * s, 0, PAL_PAINT(PAINT_TANGERINE, 1), 0);
    for (int k = 0; k < 2; k++) wd_line(x - 1 + k * 2.5f, y - 3 * s, x - 0.5f + k * 2.5f, y - 7 * s, 0.4f, PAL_PAINT(PAINT_SILVER, 1));
}
static void cupcake(float x, float y, float s) {
    wd_quad(x - 4 * s, y - 3 * s, x + 4 * s, y - 3 * s, x + 3 * s, y + 3 * s, x - 3 * s, y + 3 * s, PAL_PAINT(PAINT_CARAMEL, 2), INK);
    wd_blob_ol(x, y - 4.5f * s, 5 * s, 3.2f * s, 0, PAL_PAINT(PAINT_BUBBLEGUM, 2), INK, 0);
    wd_blob(x, y - 8.5f * s, 1.3f * s, 1.3f * s, 0, PAL_PAINT(PAINT_CHERRY, 1), 0);
}
static void firework(float x, float y, int age, int seed) {
    if (age < 0 || age > 26) return;
    static const uint8_t cs[4] = { PAL_PAINT(PAINT_LEMON, 2), PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_MINT, 2), PAL_PAINT(PAINT_SKY, 2) };
    float r = 4 + age * 0.9f;
    for (int k = 0; k < 10; k++) {
        float a = k * 0.628f + seed;
        wd_line(x + cosf(a) * r * 0.45f, y + sinf(a) * r * 0.45f, x + cosf(a) * r, y + sinf(a) * r, 1.6f, cs[(k + seed) & 3]);
    }
}
static void night(void) {
    static const uint8_t cols[] = { PAL_PAINT(PAINT_NAVY, 1), PAL_PAINT(PAINT_NAVY, 2), PAL_PAINT(PAINT_GRAPE, 1) };
    tk_vgrad(0, 0, TW, TH, cols, 3);
    for (int i = 0; i < 24; i++) { uint32_t h = tk_hash(i * 17u + 3u); tk_star(h % TW, (h >> 9) % 70, 1, PAL_PAINT(PAINT_LEMON, 2)); }
}
/* lantern-strung bandstand, storybook size */
static void bandstand(float x, float y, float s, float t) {
    for (int k = -1; k <= 1; k += 2) wd_tube(x + k * 14 * s, y, x + k * 14 * s, y - 18 * s, 1.2f * s, 1.2f * s, INK, PAL_WHITE, PAL_WHITE, PAL_WHITE);
    wd_rect(x - 18 * s, y - 2 * s, 36 * s, 4 * s, PAL_PAINT(PAINT_CARAMEL, 1), INK);
    wd_tri(x - 22 * s, y - 18 * s, x + 22 * s, y - 18 * s, x, y - 32 * s, PAL_PAINT(PAINT_ROSE, 2));
    for (int k = 0; k < 6; k++) {
        float lx = x - 30 * s + k * 12 * s, ly = y - 14 * s + fabsf(k - 2.5f) * -1.2f * s;
        wd_blob_ol(lx, ly, 2 * s, 2.4f * s, 0, k & 1 ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_ROSE, 2), INK, 0);
    }
    wd_line(x - 30 * s, y - 16 * s, x + 30 * s, y - 16 * s, 0.4f, INK);
}
static void park(float t, int wall) {
    night();
    wd_rect(-200, -10, 400, 120, PAL_RAMP(RAMP_GRASS, 4), 0);                 /* lawn */
    wd_rect(-200, 14, 400, 12, PAL_PAINT(PAINT_SKY, 1), INK);                  /* river */
    for (int k = 0; k < 12; k++) wd_line(-120 + k * 22 + fmodf(t * 0.3f, 22), 19, -114 + k * 22 + fmodf(t * 0.3f, 22), 19, 0.5f, PAL_WHITE);
    /* bridge */
    for (int k = 0; k < 12; k++) {          /* arched wooden deck */
        float x0 = -84 + k * 3.5f, x1 = x0 + 3.5f;
        float y0 = 14 - 8 * (1 - powf((x0 + 63) / 21, 2)), y1 = 14 - 8 * (1 - powf((x1 + 63) / 21, 2));
        wd_quad(x0, y0, x1, y1, x1, y1 + 4, x0, y0 + 4, PAL_PAINT(PAINT_CARAMEL, 2), INK);
        wd_line(x0, y0 - 4, x1, y1 - 4, 0.6f, INK);
        wd_line(x0, y0, x0, y0 - 4, 0.5f, INK);
    }
    bandstand(0, 0, 1, t);
    /* north: cupcake tower table; south: soup cauldron table */
    wd_rect(-56, -10, 36, 4, PAL_PAINT(PAINT_CARAMEL, 1), INK);
    for (int r = 0; r < 3; r++) for (int k = 0; k <= 2 - r; k++) cupcake(-46 + k * 8 + r * 4, -14 - r * 7, 0.7f);
    wd_rect(30, 40, 40, 4, PAL_PAINT(PAINT_CARAMEL, 1), INK);
    wd_blob_ol(50, 34, 9, 7, 0, PAL_PAINT(PAINT_CHARCOAL, 1), INK, WB_SHADE);
    wd_blob(50, 28, 7, 1.6f, 0, PAL_PAINT(PAINT_TANGERINE, 2), 0);
    for (int k = 0; k < 3; k++) wd_blob(46 + k * 4, 22 - fmodf(t * 0.2f + k * 3, 8), 1.6f, 1.6f, 0, PAL_WHITE, 0);
    if (wall) {
        wd_rect(-5, -120, 10, 240, PAL_PAINT(PAINT_SILVER, 1), INK);
        for (int k = 0; k < 24; k++) wd_line(-5, -120 + k * 10.0f, 5, -120 + k * 10.0f, 0.6f, PAL_PAINT(PAINT_CHARCOAL, 2));
    }
}

/* ============================================================ s01 calendar */
void scene_s01_calendar(int f, int n) {
    tk_clear(PAPER);
    wd_cam(0, 0, 1.0f + 0.15f * seg(f, 0, n));
    wd_rect(-80, -62, 160, 116, PAL_WHITE, INK);
    wd_rect(-80, -62, 160, 14, PAL_PAINT(PAINT_ROSE, 2), INK);
    wd_text(-24, -59, "SUMMER", 1.4f, INK, 0);
    for (int r = 0; r < 5; r++) for (int c = 0; c < 7; c++) {
        wd_rect(-78 + c * 22.4f, -46 + r * 20, 22.4f, 20, PAL_WHITE, PAL_PAINT(PAINT_SKY, 1));
        char d[3] = { 0 }; int day = r * 7 + c + 1; d[0] = '0' + day / 10; d[1] = '0' + day % 10;
        if (day < 10) { d[0] = d[1]; d[1] = 0; }
        if (day <= 30) wd_text(-76 + c * 22.4f, -44 + r * 20, d, 0.8f, PAL_PAINT(PAINT_SKY, 0), 0);
    }
    /* the square: MIDSUMMER, circled by a crayon in one happy loop */
    float cx = -78 + 3 * 22.4f + 11.2f, cy = -46 + 2 * 20 + 10;
    wd_rect(cx - 44, cy - 9, 88, 18, PAL_WHITE, 0);
    float circ = seg(f, 12, 40);
    for (int k = 0; k < (int)(circ * 40); k++) {
        float a0 = k * 0.17f, a1 = a0 + 0.17f, rr = 1.0f + 0.04f * sinf(k * 0.9f);
        wd_line(cx + cosf(a0) * 47 * rr, cy + sinf(a0) * 15 * rr, cx + cosf(a1) * 47 * rr, cy + sinf(a1) * 15 * rr, 1.8f, PAL_PAINT(PAINT_CHERRY, 1));
    }
    /* fireworks scribbled all round it, outside the lines */
    for (int k = 0; k < 6; k++) {
        float a = k * 1.05f + 0.3f;
        float fx = cx + cosf(a) * 62, fy = cy + sinf(a) * 34;
        int age = f - (46 + k * 8);
        if (age > 26) age = 26;
        firework(fx, fy, age, k);
    }
    wd_post_crayon(f, PAPER);
    /* the word in the square, in a child's careful capitals (drawn after the grain so it reads) */
    { int w = tk_text_w("MIDSUMMER", 2); tk_text((int)wd_sx(cx) - w / 2, (int)wd_sy(cy) - 7 + (int)((f / 4) % 2), "MIDSUMMER", PAL_PAINT(PAINT_MAGENTA, 0), 2); }
}
void scene_s01_calendar_audio(double t0, double dur) {
    tk_note(AT(12), 0.5, 60, TKV_NOISE, 0.05f);                 /* crayon on paper */
    for (int k = 0; k < 6; k++) tk_slide(AT(46 + k * 8), 0.12, 84, 96, TKV_SINE, 0.08f);
}

/* ============================================================== s02 park */
void scene_s02_park(int f, int n) {
    wd_cam(0, 8 - 6 * seg(f, 0, n), 1.25f + 0.15f * seg(f, 0, n));
    park(f, 0);
    /* little guests dotted about, all one park */
    for (int i = 0; i < 6; i++) {
        float x = -90 + i * 34 + 4 * sinf(f * 0.05f + i), y = i & 1 ? -2 : 46;
        if (i % 3 == 0) imp(x, y, 0.6f, 1, f + i * 9, 0); else bunny(x, y, 0.6f, -1, f + i * 9);
    }
    float r = 10 + 200 * seg(f, 0, 40);
    if (r < 200) tk_post_iris(120, 76, r, 0, PAPER, PAL_PAINT(PAINT_NAVY, 1));
    wd_post_crayon(f, PAPER);
    tk_rect(0, 128, TW, 32, PAPER);
    super2("Long ago, the whole park threw", "one big Midsummer party.", f, 44, n);
}
void scene_s02_park_audio(double t0, double dur) { tk_sfx(AT(2), SFX_SPARKLE, 0.25f); }

/* ============================================================ s03 bridge */
void scene_s03_bridge(int f, int n) {
    wd_cam(-62 + 10 * seg(f, 0, n), 18, 3.2f);
    park(f, 0);
    for (int i = 0; i < 4; i++) {
        float u = fmodf(f * 0.3f + i * 13, 52) - 26;
        float ub = u + 6, ui = -u - 6;
        float yb = 14 - 8 * (1 - fminf(1, powf(ub / 21, 2))) + 2, yi = 14 - 8 * (1 - fminf(1, powf(ui / 21, 2))) + 2;
        bunny(-63 + ub, yb, 0.5f, 1, f + i * 7);
        imp(-63 + ui, yi, 0.5f, -1, f + i * 5, 0);
    }
    wd_post_crayon(f, PAPER);
    tk_rect(0, 128, TW, 32, PAPER);
    super2("Everybody came.", "They called it the Mingle.", f, 10, n);
}
void scene_s03_bridge_audio(double t0, double dur) {
    for (int k = 0; k < 6; k++) tk_slide(AT(14 + k * 20), 0.06, 88, 92, TKV_SQ12, 0.06f);   /* giggles */
}

/* ============================================================ s04 fluffy */
void scene_s04_fluffy(int f, int n) {
    night();
    wd_cam(0, 0, 1.0f + 0.12f * seg(f, 0, n));
    wd_rect(-200, 30, 400, 100, PAL_RAMP(RAMP_GRASS, 4), INK);
    wd_rect(-70, 30, 140, 8, PAL_PAINT(PAINT_CARAMEL, 1), INK);     /* the table */
    for (int i = 0; i < 6; i++) {                                   /* fireworks on the beat */
        int age = f - (84 + i * 12);
        firework(-70 + i * 28, -50 + (i & 1) * 14, age, i);
    }
    float cy = -30 + 2 * sinf(f * 0.08f);
    float sx = -30, sy = 10;
    wd_line(sx + 7, sy - 4, -26 + sinf(f * 0.07f) * 2, cy + 6, 0.5f, INK);    /* the string */
    cloud(-26 + sinf(f * 0.07f) * 2, cy, 2.0f, (f % 70) > 64 || (f > 40 && f < 44), f >= 100);
    cherub_back(sx, sy, 1.4f);
    wd_blob_ol(sx + 7, sy - 4, 1.8f, 1.8f, 0, PAL_SKIN(SKIN_PEACH, 1), INK, 0);   /* hand on the string */
    /* the young imp, from behind; his horned hands set down two bowls */
    float ix = 34, iy = 12;
    imp(ix, iy, 1.6f, 1, 0, 1);
    for (int b = 0; b < 2; b++) {
        float land = seg(f, 30 + b * 14, 40 + b * 14);
        float bx = ix - 10 + b * 20, by = lerp(10, 27, land);
        bowl(bx, by, 1.3f);
        if (land < 1) wd_blob_ol(bx, by - 4, 3, 2.4f, 0, PAL_PAINT(PAINT_GRAPE, 2), INK, 0);
    }
    if (f >= 40 && f < 52) tk_text_outline((int)wd_sx(24), (int)wd_sy(16), "clink", PAL_WHITE, INK, 1);
    if (f >= 54 && f < 66) tk_text_outline((int)wd_sx(44), (int)wd_sy(16), "clink", PAL_WHITE, INK, 1);
    wd_post_crayon(f, PAPER);
    tk_rect(0, 128, TW, 32, PAPER);
    super2("Fluffy the cloud", "loved the fireworks most.", f, 70, n);
}
void scene_s04_fluffy_audio(double t0, double dur) {
    tk_note(AT(40), 0.25, 100, TKV_SINE, 0.25f); tk_note(AT(54), 0.25, 103, TKV_SINE, 0.25f);  /* clink clink */
    for (int i = 0; i < 6; i++) { tk_note(AT(84 + i * 12), 0.15, 50, TKV_NOISE, 0.18f); tk_slide(AT(80 + i * 12), 0.12, 70, 94, TKV_SINE, 0.05f); }
    tk_slide(AT(100), 0.3, 84, 91, TKV_SINE, 0.15f);    /* hearts */
}

/* ============================================================= s05 pluck */
void scene_s05_pluck(int f, int n) {
    if (f < 60) {
        /* close: the small hand, the loop slipping off the fingers */
        tk_clear(PAPER);
        wd_cam(0, 0, 4);
        uint8_t sk = PAL_SKIN(SKIN_PEACH, 1);
        wd_tube(8, 30, 2, 6, 5, 4.5f, INK, sk, sk, PAL_SKIN(SKIN_PEACH, 2));
        wd_blob_ol(0, 2, 7, 5.5f, 0, sk, INK, WB_SHADE);
        for (int k = 0; k < 4; k++) wd_tube(-4 + k * 2.8f, -1, -4.5f + k * 3.2f, -8, 1.3f, 1.1f, INK, sk, sk, PAL_SKIN(SKIN_PEACH, 2));
        float u = seg(f, 10, 30);
        float lx = -3 + u * 2, ly = -9 - u * u * 24;
        wd_blob_ol(lx, ly, 3.2f, 1.4f, 0, PAPER, INK, 0);
        wd_line(lx, ly - 1, lx + 6, ly - 40, 0.5f, INK);
        if (f >= 30 && f < 44) tk_text_outline(150, 40, "PLUCK", PAL_WHITE, INK, 2);
    } else {
        /* Fluffy drifts south, over the river, small and away */
        wd_cam(0, 10, 1.2f);
        park(f, 0);
        float u = seg(f, 60, n);
        cloud(lerp(-20, 60, u), lerp(-40, 60, u), lerp(1.4f, 0.6f, u), 0, 0);
        wd_line(lerp(-20, 60, u), lerp(-36, 63, u), lerp(-24, 56, u), lerp(-20, 74, u), 0.4f, PAL_WHITE);
    }
    wd_post_crayon(f, PAPER);
    tk_rect(0, 128, TW, 32, PAPER);
    super2("Then the string slipped...", "and nobody noticed.", f, 4, 66);
    super2("Fluffy drifted away.", "Not everybody got home.", f, 70, n);
}
void scene_s05_pluck_audio(double t0, double dur) {
    tk_slide(AT(30), 0.18, 79, 67, TKV_TRI, 0.5f);      /* PLUCK */
    tk_note(AT(30), 0.05, 60, TKV_NOISE, 0.2f);
}

/* ============================================================== s06 wall */
void scene_s06_wall(int f, int n) {
    wd_cam(0, -8, 2.2f + 0.1f * seg(f, 0, n));
    park(f, 1);
    /* two bowls south, one cupcake north */
    bowl(18, 4, 0.9f); bowl(30, 4, 0.9f);
    cupcake(-22, -4, 1.0f);
    if (f < 3) tk_post_shake(3, 2);
    wd_post_crayon(f, PAPER);
    tk_rect(0, 128, TW, 32, PAPER);
    super2("Then came the Wall.", "And nobody says why.", f, 8, n);
}
void scene_s06_wall_audio(double t0, double dur) { tk_sfx(AT(0), SFX_THUD, 0.7f); tk_kick(AT(0), 0.6f); }

/* ============================================================== s07 roll */
void scene_s07_roll(int f, int n) {
    tk_clear(PAPER);
    wd_cam(-36, -60, 2.6f);
    Banner b = { 0 };
    set_bandstand(f, &b, 1);
    /* the old storybook banner said MINGLE; a pink roller covers it all,
     * then GIGGLE is written fresh. The words never mix. */
    float rx = lerp(BAN_X0 - 6, BAN_X1 + 6, seg(f, 20, 80));
    wd_rect(BAN_X0 + 2, BAN_Y0 + 2, BAN_X1 - BAN_X0 - 4, BAN_Y1 - BAN_Y0 - 4, PAPER, 0);
    if (f < 82) wd_text(LET_X0, LET_Y0, "MINGLE", LET_PX, PAL_PAINT(PAINT_SKY, 1), INK);
    float cover = fminf(rx, BAN_X1 - 2) - (BAN_X0 + 2);
    if (cover > 0) wd_rect(BAN_X0 + 2, BAN_Y0 + 2, cover, BAN_Y1 - BAN_Y0 - 4, PAL_PAINT(PAINT_BUBBLEGUM, 2), 0);
    if (f >= 88) {
        float w = seg(f, 88, 100);
        if (w > 0.99f || (f & 1)) wd_text(LET_X0, LET_Y0, "GIGGLE", LET_PX, PAL_WHITE, PAL_PAINT(PAINT_MAGENTA, 1));
    }
    if (f >= 16 && f < 86) {
        wd_rect(rx - 2.4f, BAN_Y0 - 1, 4.8f, BAN_Y1 - BAN_Y0 + 2, PAL_PAINT(PAINT_BUBBLEGUM, 1), INK);
        wd_tube(rx, BAN_Y1 + 1, rx + 4, BAN_Y1 + 18, 1.0f, 1.0f, INK, PAL_PAINT(PAINT_HONEY, 0), PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_HONEY, 2));
    }
    wd_post_crayon(f, PAPER);
    tk_rect(0, 128, TW, 32, PAPER);
    super("Now each side throws its own party.", f, 6, 60, 138);
    super("Half the park each.", f, 64, 112, 138);
    super("And no more fireworks.", f, 116, n, 138);
}
void scene_s07_roll_audio(double t0, double dur) {
    for (int k = 0; k < 3; k++) tk_slide(AT(24 + k * 20), 0.25, 55, 60, TKV_NOISE, 0.08f);   /* roller */
}
