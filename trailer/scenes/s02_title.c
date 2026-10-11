/* s02_title.c - the title card.
 * Pastel sky, rainbow, clouds. "DOOMOGACHI" drops in letter by letter with
 * squash & stretch, "LIFE" slams down, "in HELLvile Park!" pops in.
 * The two O's of DOOM are eyes. Then the "HELL" starts to drip...
 * the logo gets nervous, shakes it off, the drop splats on the camera lens,
 * and a cute sticker gets slapped over it. Nothing to see here! */
#include "tk.h"
#include <math.h>
#include <string.h>

/* ---- logo letter tiles ------------------------------------------------- */
#define TL_W 28
#define TL_H 40
#define TL_PAD 3
typedef struct { uint8_t px[TL_H][TL_W]; int w, h; } Tile;
static Tile tiles[14];
static int tiles_ready;
static const char LOGO1[] = "DOOMOGACHI";
static const char LOGO2[] = "LIFE";

static void make_tile(Tile *t, char ch, int s, int paint, uint8_t ol) {
    static uint8_t save[TH][TW];
    memcpy(save, tk_fb, sizeof save);
    tk_clear(0);
    char str[2] = { ch, 0 };
    tk_text(TL_PAD, TL_PAD, str, PAL_WHITE, s);
    int w = 5 * s + 2 * TL_PAD + 1, h = 7 * s + 2 * TL_PAD;
    static uint8_t m[TL_H][TL_W];
    for (int y = 0; y < TL_H; y++)
        for (int x = 0; x < TL_W; x++) m[y][x] = (x < w && y < h) ? tk_fb[y][x] != 0 : 0;
    memcpy(tk_fb, save, sizeof save);
    /* faux-bold: thicken vertical strokes by one pixel */
    for (int y = 0; y < h; y++)
        for (int x = w - 1; x > 0; x--) m[y][x] |= m[y][x - 1];
    t->w = w; t->h = h;
    memset(t->px, 0, sizeof t->px);
    /* outer white border (radius 3 diamond-ish), dark outline (radius 1), fill */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int d1 = 0, d3 = 0;
            for (int dy = -3; dy <= 3; dy++)
                for (int dx = -3; dx <= 3; dx++) {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= w || yy >= h || !m[yy][xx]) continue;
                    int ad = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
                    if (ad <= 2 && dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) d1 = 1;
                    if (ad <= 4) d3 = 1;
                }
            if (m[y][x]) {
                int gy = y - TL_PAD; /* glyph-local row */
                uint8_t c = PAL_PAINT(paint, 1);
                if (gy < 2 * s) c = PAL_PAINT(paint, 2);
                else if (gy >= 5 * s) c = PAL_PAINT(paint, 0);
                else if (gy >= 4 * s + s / 2 && ((x + y) & 1)) c = PAL_PAINT(paint, 0);
                if (y > 0 && !m[y - 1][x]) c = PAL_WHITE; /* glossy top edge */
                t->px[y][x] = c;
            } else if (d1) t->px[y][x] = ol;
            else if (d3) t->px[y][x] = PAL_WHITE;
        }
}

static void build_tiles(void) {
    if (tiles_ready) return;
    tiles_ready = 1;
    static const int pal1[10] = { PAINT_BUBBLEGUM, PAINT_SKY, PAINT_MINT, PAINT_LAVENDER, PAINT_LEMON,
                                  PAINT_PEACH, PAINT_BUBBLEGUM, PAINT_SKY, PAINT_MINT, PAINT_LAVENDER };
    uint8_t ol = PAL_RAMP(RAMP_VAPE, 1);
    for (int i = 0; i < 10; i++) make_tile(&tiles[i], LOGO1[i], 3, pal1[i], ol);
    for (int i = 0; i < 4; i++) make_tile(&tiles[10 + i], LOGO2[i], 4, PAINT_ROSE, ol);
}

/* blit a tile with its bottom-centre at (cx, by), scaled by sx, sy */
static void blit_tile(const Tile *t, int cx, int by, float sx, float sy, int shadow) {
    int W = (int)(t->w * sx + 0.5f), H = (int)(t->h * sy + 0.5f);
    if (W < 1 || H < 1) return;
    int x0 = cx - W / 2, y0 = by - H;
    for (int pass = shadow ? 0 : 1; pass < 2; pass++)
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                int sxp = (int)(x / sx), syp = (int)(y / sy);
                if (sxp >= t->w || syp >= t->h) continue;
                uint8_t c = t->px[syp][sxp];
                if (!c) continue;
                if (pass == 0) { if (((x + y) & 1) == 0) tk_px(x0 + x + 2, y0 + y + 3, PAL_RAMP(RAMP_VAPE, 4)); }
                else tk_px(x0 + x, y0 + y, c);
            }
}

/* ---- background --------------------------------------------------------- */
static void cloud(int x, int y, int s) {
    uint8_t sh = PAL_RAMP(RAMP_STONE, 7), w = PAL_WHITE;
    tk_ellipse(x, y + 2, 14 * s / 4, 5 * s / 4, sh);
    tk_circle(x - 6 * s / 4, y - 1, 6 * s / 4, w);
    tk_circle(x + 3 * s / 4, y - 4 * s / 4, 8 * s / 4, w);
    tk_circle(x + 10 * s / 4, y, 5 * s / 4, w);
    tk_ellipse(x, y + 1, 13 * s / 4, 4 * s / 4, w);
    tk_rect(x - 10 * s / 4, y + 4 * s / 4, 22 * s / 4, 1, sh);
}
static void background(int f) {
    static const uint8_t sky[] = { PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7),
                                   PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7) };
    tk_vgrad(0, 0, TW, TH, sky, 5);
    /* soft pastel rainbow */
    static const uint8_t rb[] = { PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_PEACH, 2), PAL_PAINT(PAINT_LEMON, 2),
                                  PAL_PAINT(PAINT_MINT, 2), PAL_PAINT(PAINT_SKY, 2), PAL_PAINT(PAINT_LAVENDER, 2) };
    for (int y = 10; y < 140; y++)
        for (int x = 0; x < TW; x++) {
            int dx = x - 120, dy = y - 150;
            int d = (int)sqrtf((float)(dx * dx + dy * dy));
            int band = (132 - d) / 5;
            if (band >= 0 && band < 6 && ((x + y) & 1)) tk_px(x, y, rb[band]);
        }
    /* drifting clouds */
    for (int i = 0; i < 6; i++) {
        int sp = 1 + (i % 3);
        int x = ((i * 67 + 20) + f * sp / 4) % 300 - 30;
        int y = 14 + (i * 37) % 70;
        cloud(x, y, 3 + (i & 1));
    }
    /* rolling hills with tiny cottages */
    for (int x = 0; x < TW; x++) {
        int h1 = 132 + (int)(6 * sinf(x * 0.035f + 1.0f));
        int h2 = 142 + (int)(5 * sinf(x * 0.05f + 3.0f));
        for (int y = h1; y < TH; y++) tk_px(x, y, PAL_RAMP(RAMP_GRASS, 6));
        tk_px(x, h1, PAL_RAMP(RAMP_GRASS, 7));
        for (int y = h2; y < TH; y++) tk_px(x, y, PAL_RAMP(RAMP_GRASS, 5));
        tk_px(x, h2, PAL_RAMP(RAMP_GRASS, 6));
    }
    static const int hx[] = { 14, 44, 196, 226 };
    static const int roof[] = { PAINT_ROSE, PAINT_SKY, PAINT_LAVENDER, PAINT_ROSE };
    for (int i = 0; i < 4; i++) {
        int x = hx[i], g = 132 + (int)(6 * sinf(x * 0.035f + 1.0f));
        tk_rect(x - 5, g - 7, 10, 8, PAL_UI_CREAM);
        tk_tri(x - 7, g - 7, x + 7, g - 7, x, g - 13, PAL_PAINT(roof[i], 1));
        tk_rect(x - 1, g - 4, 2, 4, PAL_PAINT(PAINT_CARAMEL, 1));
        tk_px(x - 3, g - 5, PAL_UI_GOLD); tk_px(x + 3, g - 5, PAL_UI_GOLD);
    }
}

/* ---- the logo -------------------------------------------------------------- */
#define L1_ADV 17
#define L1_BOTTOM 50
#define L2_ADV 23
#define L2_BOTTOM 90
#define SUB_Y 99

enum { EYE_NONE, EYE_OPEN, EYE_HAPPY, EYE_SHOCK, EYE_BLINK };

/* drop-in motion for an element that starts at local frame a = 0 */
static void drop_motion(int a, int fall, float height, float *dy, float *sx, float *sy) {
    *dy = 0; *sx = 1; *sy = 1;
    if (a < fall) { float t = (float)(a + 1) / fall; *dy = -height * (1 - t * t); *sx = 0.85f; *sy = 1.2f; return; }
    a -= fall;
    static const float SX[] = { 1.35f, 1.2f, 0.92f, 0.96f, 1.03f, 1.0f };
    static const float SY[] = { 0.65f, 0.8f, 1.1f, 1.06f, 0.97f, 1.0f };
    static const float DY[] = { 0, 0, -3, -4, -2, 0 };
    if (a < 6) { *sx = SX[a]; *sy = SY[a]; *dy = DY[a]; }
}

static void draw_eye(int x, int y, int state, int lx, int ly) {
    /* counter of a bold scale-3 'O': 8 x 15 at (x, y) */
    if (state == EYE_NONE) return;
    uint8_t k = PAL_RAMP(RAMP_VAPE, 0);
    tk_rect(x, y, 8, 15, PAL_WHITE);
    if (lx > 2) lx = 2;
    switch (state) {
    case EYE_OPEN:
        tk_rect(x + 2 + lx, y + 4 + ly, 4, 7, k);
        tk_rect(x + 2 + lx, y + 5 + ly, 2, 2, PAL_WHITE);
        tk_px(x + 4 + lx, y + 9 + ly, PAL_RAMP(RAMP_VAPE, 3));
        break;
    case EYE_SHOCK:
        tk_rect(x + 3 + lx, y + 6 + ly, 2, 3, k);
        break;
    case EYE_HAPPY:
        for (int i = 0; i < 4; i++) { tk_rect(x + i, y + 9 - i, 2, 2, k); tk_rect(x + 6 - i, y + 9 - i, 2, 2, k); }
        tk_rect(x + 3, y + 5, 2, 2, k);
        break;
    case EYE_BLINK:
        tk_rect(x + 1, y + 7, 6, 2, k);
        break;
    }
}

static void logo(int f, int jx, int jy, int eye, int lx, int ly, int show_sub_from) {
    build_tiles();
    int x0 = 120 - (9 * L1_ADV + tiles[0].w) / 2 + tiles[0].w / 2;
    /* shadows first so letters overlap neighbours' shadows */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < 10; i++) {
            int a = f - (i * 3) / 2;
            if (a < 0) continue;
            float dy, sx, sy;
            drop_motion(a, 7, 70, &dy, &sx, &sy);
            int wob = f > 30 ? (int)(1.4f * sinf(f * 0.18f + i * 0.7f)) : 0;
            int cx = x0 + i * L1_ADV + jx, by = L1_BOTTOM + (int)dy + wob + jy;
            if (pass == 0) blit_tile(&tiles[i], cx, by, sx, sy, 1);
            else {
                blit_tile(&tiles[i], cx, by, sx, sy, 0);
                if ((i == 1 || i == 2) && a >= 13) {
                    int ex = cx - tiles[i].w / 2 + TL_PAD + 4, ey = by - tiles[i].h + TL_PAD + 3;
                    draw_eye(ex, ey, eye, lx, ly);
                }
            }
        }
    }
    /* LIFE slams down as one */
    int x2 = 120 - (3 * L2_ADV + tiles[10].w) / 2 + tiles[10].w / 2;
    int a = f - 14;
    if (a >= 0) {
        float dy, sx, sy;
        drop_motion(a, 5, 90, &dy, &sx, &sy);
        for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i < 4; i++) {
                int cx = 120 + (int)((x2 + i * L2_ADV - 120) * sx) + jx;
                int wob = f > 30 ? (int)(1.4f * sinf(f * 0.18f + i * 0.9f + 2)) : 0;
                blit_tile(&tiles[10 + i], cx, L2_BOTTOM + (int)dy + wob + jy, sx, sy, pass == 0);
            }
        if (a >= 5 && a < 18) { /* dust puffs */
            int t = a - 5;
            for (int s = -1; s <= 1; s += 2) {
                int px = 120 + s * (52 + t * 3), py = L2_BOTTOM - 3 - t / 3;
                int r = 5 - t / 3;
                if (r > 0) { tk_circle(px, py, r, PAL_WHITE); tk_ring(px, py, r, PAL_RAMP(RAMP_STONE, 6)); }
            }
        }
    }
    /* subtitle */
    if (f >= show_sub_from) {
        static const char sub[] = "in HELLvile Park!";
        int w = tk_text_w(sub, 2), sx0 = 120 - w / 2 + jx;
        uint8_t ol = PAL_RAMP(RAMP_VAPE, 1);
        for (int i = 0; sub[i]; i++) {
            int age = f - show_sub_from - i;
            if (age < 0) break;
            int dy = age == 0 ? -5 : age == 1 ? 1 : 0;
            char s[2] = { sub[i], 0 };
            int hell = i >= 3 && i <= 6;
            uint8_t c = hell ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_WHITE;
            uint8_t o = hell ? PAL_RAMP(RAMP_BLOOD, 1) : ol;
            tk_text_outline(sx0 + i * 12, SUB_Y + dy + jy, s, c, o, 2);
        }
    }
}

/* ---- the drip, the splat, the stickers ---------------------------------- */
static void drop_blob(int x, int y, int r) {
    tk_circle(x, y, r + 1, PAL_RAMP(RAMP_BLOOD, 1));
    tk_circle(x, y, r, PAL_PAINT(PAINT_CHERRY, 1));
    if (r >= 2) tk_circle(x + r / 3, y + r / 3, r / 2, PAL_PAINT(PAINT_BLOOD, 1));
    tk_px(x - r / 2, y - r / 2, PAL_WHITE);
    if (r > 4) tk_rect(x - r / 2, y - r / 2, r / 3 + 1, r / 3 + 1, PAL_WHITE);
}

static void splat(int cx, int cy, int age) {
    uint32_t s = tk_hash(4242);
    uint8_t dk = PAL_RAMP(RAMP_BLOOD, 3), md = PAL_RAMP(RAMP_BLOOD, 5), lt = PAL_RAMP(RAMP_BLOOD, 6);
    int grow = age < 2 ? 2 : 0;
    for (int pass = 0; pass < 2; pass++) {
        uint32_t q = s;
        int o = pass == 0 ? 1 : 0;
        tk_circle(cx, cy, 15 - grow + o, pass ? md : dk);
        for (int i = 0; i < 11; i++) {
            float a = tk_rand(&q, 628) / 100.0f;
            int d = 12 + tk_rand(&q, 12), r = 2 + tk_rand(&q, 5);
            tk_circle(cx + (int)(cosf(a) * d), cy + (int)(sinf(a) * d * 0.8f), r + o, pass ? md : dk);
            int d2 = d + 6 + tk_rand(&q, 8);
            tk_circle(cx + (int)(cosf(a) * d2), cy + (int)(sinf(a) * d2 * 0.8f), 1 + o, pass ? md : dk);
        }
        /* running drips */
        for (int i = 0; i < 4; i++) {
            int dx = -10 + i * 7, len = 4 + (age * (3 + i * 2)) / 6;
            if (len > 34) len = 34;
            tk_rect(cx + dx - o, cy + 6, 3 + 2 * o, len + o, pass ? md : dk);
            tk_circle(cx + dx + 1, cy + 6 + len, 2 + o, pass ? md : dk);
        }
    }
    /* the one splotch that got away */
    tk_circle(cx - 35, cy + 11, 4, dk); tk_circle(cx - 35, cy + 11, 3, md);
    tk_rect(cx - 36, cy + 12, 2, 4 + (age > 20 ? 4 : age / 5), md);
    tk_rect(cx - 7, cy - 8, 4, 3, lt);
    tk_px(cx - 8, cy - 6, lt);
    tk_rect(cx - 6, cy - 9, 2, 1, PAL_WHITE);
}

static void star_poly(int cx, int cy, float ro, float ri, uint8_t c) {
    for (int i = 0; i < 5; i++) {
        float a0 = -1.5708f + i * 1.2566f, a1 = a0 + 0.6283f, a2 = a0 + 1.2566f;
        int ox = cx + (int)(cosf(a0) * ro), oy = cy + (int)(sinf(a0) * ro);
        int ix = cx + (int)(cosf(a1) * ri), iy = cy + (int)(sinf(a1) * ri);
        int px = cx + (int)(cosf(a0 - 0.6283f) * ri), py = cy + (int)(sinf(a0 - 0.6283f) * ri);
        int nx = cx + (int)(cosf(a2) * ro), ny = cy + (int)(sinf(a2) * ro);
        (void)nx; (void)ny;
        tk_tri(cx, cy, px, py, ox, oy, c);
        tk_tri(cx, cy, ox, oy, ix, iy, c);
    }
}
static void star_sticker(int cx, int cy, float sc, int f) {
    float ro = 23 * sc, ri = 12 * sc;
    star_poly(cx + 2, cy + 3, ro + 3, ri + 3, PAL_RAMP(RAMP_BLOOD, 2));     /* shadow */
    star_poly(cx, cy, ro + 4, ri + 4, PAL_RAMP(RAMP_VAPE, 1));
    star_poly(cx, cy, ro + 3, ri + 3, PAL_WHITE);
    star_poly(cx, cy, ro, ri, PAL_PAINT(PAINT_LEMON, 1));
    star_poly(cx - 1, cy - 1, ro * 0.7f, ri * 0.7f, PAL_PAINT(PAINT_LEMON, 2));
    if (sc > 1.05f) return;
    uint8_t k = PAL_RAMP(RAMP_VAPE, 0);
    int blink = (f % 40) > 36;
    for (int e = -1; e <= 1; e += 2) {
        int ex = cx + e * 6;
        if (blink) tk_rect(ex - 1, cy, 3, 1, k);
        else { tk_rect(ex - 1, cy - 2, 3, 4, k); tk_px(ex - 1, cy - 2, PAL_WHITE); }
        tk_rect(ex + e * 3 - 1, cy + 3, 3, 2, PAL_PAINT(PAINT_ROSE, 2));
    }
    tk_rect(cx - 2, cy + 4, 5, 1, k);
    tk_px(cx - 3, cy + 3, k); tk_px(cx + 3, cy + 3, k);
    /* peeling corner shine */
    tk_px(cx - 3, cy - 18, PAL_WHITE); tk_px(cx - 2, cy - 17, PAL_WHITE);
}
static void heart_sticker(int cx, int cy, float sc) {
    int s = sc > 1.2f ? 3 : 2;
    tk_heart(cx + 1, cy + 1, s, PAL_RAMP(RAMP_BLOOD, 2));
    tk_heart(cx, cy, s, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    tk_px(cx - 3, cy - 3, PAL_WHITE); tk_px(cx - 4, cy - 2, PAL_WHITE);
}

void scene_s02_title(int f, int n) {
    background(f);
    (void)n;
    /* nervous logic */
    int jx = 0, jy = 0;
    if (f >= 60 && f < 72) { jx = ((f * 7) % 5) - 2; jy = (f & 1) ? -1 : 1; }
    if (f >= 72 && f < 75) jy = (f & 1) ? -2 : 2;

    int eye = EYE_OPEN, lx = 0, ly = 0;
    if (f < 26) eye = EYE_BLINK;
    else if (f < 40) { lx = ((f / 6) & 1) ? 2 : -2; ly = -1; }
    else if (f < 50) { eye = EYE_HAPPY; }
    else if (f < 58) { lx = 2; ly = 4; }             /* ...looking down at the drip */
    else if (f < 72) { eye = EYE_SHOCK; lx = 1; ly = 4; }
    else if (f < 82) { eye = EYE_SHOCK; lx = 2; ly = 3; }
    else if (f < 90) { lx = -2; ly = 0; }            /* looks away. whistles. */
    else eye = EYE_HAPPY;
    if (f == 44 || f == 45 || f == 95) eye = EYE_BLINK;

    logo(f, jx, jy, eye, lx, ly, 24);

    /* sweat drop on the D */
    if (f >= 56 && f < 92) {
        int sx = 120 - (9 * L1_ADV + 21) / 2 - 2 + jx, sy = 22 + jy + ((f / 3) % 3);
        tk_px(sx, sy, PAL_PAINT(PAINT_SKY, 1));
        tk_rect(sx - 1, sy + 1, 3, 2, PAL_PAINT(PAINT_SKY, 2));
        tk_rect(sx - 1, sy + 3, 3, 1, PAL_PAINT(PAINT_SKY, 1));
        tk_px(sx, sy + 1, PAL_WHITE);
    }

    /* sparkles on the logo */
    if (f >= 22) tk_sparkles(120, 60, f - 22, 21, 90);

    /* THE DRIP from the second L of HELL */
    int dw = tk_text_w("in HELLvile Park!", 2);
    int dx = 120 - dw / 2 + 6 * 12 + 6 + jx, dtop = SUB_Y + 14 + jy;
    if (f >= 44 && f < 68) {
        int t = f - 44;
        int len = t < 12 ? t / 3 : 4 + (t - 12) * 2 / 3;
        int r = 1 + t / 6;
        if (r > 3) r = 3;
        tk_rect(dx - 1, dtop - 1, 3, len + 2, PAL_RAMP(RAMP_BLOOD, 1));
        tk_rect(dx, dtop - 1, 1, len + 1, PAL_PAINT(PAINT_CHERRY, 1));
        drop_blob(dx, dtop + len + r - 1, r);
    }
    /* flung at the camera */
    if (f >= 68 && f < 74) {
        float t = (f - 68) / 5.0f;
        int x = tk_lerpi(dx, 200, t * t), y = tk_lerpi(dtop + 12, 136, t * t) - (int)(18 * 4 * t * (1 - t));
        drop_blob(x, y, 3 + (int)(16 * t * t));
    }
    /* SPLAT on the lens */
    if (f >= 74) splat(200, 136, f - 74);
    /* sticker over it */
    if (f >= 84) {
        static const float SC[] = { 1.6f, 1.25f, 0.92f, 1.04f, 1.0f };
        int a = f - 84;
        star_sticker(201, 139, a < 5 ? SC[a] : 1.0f, f);
    }
    /* ...one drip escaped. another sticker. */
    if (f >= 104) {
        static const float SC[] = { 1.8f, 1.3f, 1.0f };
        int a = f - 104;
        heart_sticker(165, 147, a < 3 ? SC[a] : 1.0f);
    }
    if (f >= 87) {
        int dy = f < 89 ? -3 : 0;
        tk_text_outline(16, 136 + dy, "nothing to", PAL_WHITE, PAL_RAMP(RAMP_VAPE, 1), 1);
        tk_text_outline(16, 146 + dy, "see here!", PAL_WHITE, PAL_RAMP(RAMP_VAPE, 1), 1);
        if (f >= 92) tk_heart(16 + 9 * 6 + 5, 149 + dy, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    }

    if (f >= 74 && f < 77) tk_post_shake(f & 1 ? 3 : -3, f & 1 ? -2 : 2);
    if (f < 4) tk_post_flash(PAL_WHITE, 16 - f * 4);
}

void scene_s02_title_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    static const int notes[10] = { 72, 74, 76, 77, 79, 81, 83, 84, 86, 88 };
    for (int i = 0; i < 10; i++) tk_slide(t0 + ((i * 3) / 2 + 7) * fr, 0.06, notes[i] - 5, notes[i], TKV_SQ25, 0.22f);
    tk_sfx(t0 + 19 * fr, SFX_THUD, 0.9f);
    tk_sfx(t0 + 19 * fr, SFX_BOING, 0.6f);
    tk_sfx(t0 + 26 * fr, SFX_SPARKLE, 0.6f);
    tk_note(t0 + 26 * fr, 0.05, 81, TKV_SINE, 0.3f);                 /* eyes open: plink */
    tk_note(t0 + 28 * fr, 0.05, 88, TKV_SINE, 0.3f);
    tk_slide(t0 + 44 * fr, 0.7, 43, 40, TKV_SAW, 0.25f);             /* uh-oh */
    tk_note(t0 + 50 * fr, 0.08, 52, TKV_SQ12, 0.2f);                 /* drip... */
    for (int i = 0; i < 12; i++)                                       /* nervous jitter */
        tk_note(t0 + (60 + i) * fr, fr * 0.8, (i & 1) ? 74 : 75, TKV_SQ12, 0.18f);
    tk_sfx(t0 + 68 * fr, SFX_WHOOSH, 0.7f);
    tk_sfx(t0 + 74 * fr, SFX_SPLAT, 1.0f);
    tk_sfx(t0 + 84 * fr, SFX_POP, 0.8f);
    tk_sfx(t0 + 85 * fr, SFX_SQUEAK, 0.6f);
    tk_sfx(t0 + 89 * fr, SFX_SPARKLE, 0.4f);
    tk_sfx(t0 + 104 * fr, SFX_POP, 0.6f);
}
