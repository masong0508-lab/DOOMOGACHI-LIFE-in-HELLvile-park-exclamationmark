/* mg_draw.c - the Fun Fair's procedural drawing kit (owner: minigames).
 * Everything is drawn at runtime from the UI primitives (spans), so the
 * fair costs no art ROM: candy-striped tents, bunting, a pastel sky with a
 * turning ferris wheel (or its hellish night version), marquee bulbs,
 * treats, plush imps, a vape wraith, Father Fuzz, the mascot Tix, booth
 * photos of the hosts, medals and the in-game HUD. Every shape uses a
 * coloured outline (a dark shade of its own colour), a base tone and a
 * light rim, and most of them breathe, blink or wiggle with `t`. */
#include "mg.h"

#define OUTLINE_DARK PAL_RAMP(RAMP_STONE, 1)

static const uint8_t flag_cols[6] = {
    PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_MINT, 1),
    PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_TANGERINE, 1)
};

/* ---- text ------------------------------------------------------------------ */
void mg_text(int x, int y, const char *s, uint8_t c)
{
    ui_text_outline(x, y, s, c, PAL_RAMP(RAMP_STONE, 0));
}

void mg_textc(int cx, int y, const char *s, uint8_t c)
{
    mg_text(cx - ui_text_width(s) / 2, y, s, c);
}

void mg_big_text(int cx, int y, const char *s, int scale, uint8_t c, uint8_t outline, int t)
{
    int n = (int)strlen(s), adv = 6 * scale, x = cx - (n * adv) / 2, i;
    char ch[2] = { 0, 0 };
    for (i = 0; i < n; i++, x += adv) {
        if (s[i] == ' ') continue;
        ch[0] = s[i];
        ui_text_big(x, y + ui_wave(t * 3 + i * 9, 60, scale), ch, scale, c, outline);
    }
}

/* ---- images ------------------------------------------------------------------ */
DoomoImage mg_crop(const DoomoImage *src, int top, int rows)
{
    DoomoImage d;
    top = DOOMO_CLAMP(top, 0, src->h - 1);
    rows = DOOMO_CLAMP(rows, 1, src->h - top);
    d.w = src->w;
    d.h = (uint16_t)rows;
    d.pix = src->pix + (size_t)top * src->w;
    /* its own uid (platforms cache uploads by uid): derived from the
     * source's, tagged so it never meets a doomo_image_new_uid() value */
    d.uid = (doomo_hash32(src->uid * 2654435761u + (uint32_t)top * 977u + (uint32_t)rows) |
             0x80000000u) & 0xFFFFFFFEu;
    return d;
}

void mg_blit2(int x, int y, const uint8_t *pix, int w, int h, const uint8_t pal[3], int sy256)
{
    int dh = h * sy256 / 256, dy;
    if (dh < 1) dh = 1;
    y += h - dh;
    for (dy = 0; dy < dh; dy++) {
        int sy = dy * h / dh, sx = 0;
        const uint8_t *row = pix + (sy * w) / 4;
        while (sx < w) {
            int v = (row[sx >> 2] >> (6 - 2 * (sx & 3))) & 3, run = 1;
            while (sx + run < w &&
                   (((row[(sx + run) >> 2] >> (6 - 2 * ((sx + run) & 3))) & 3) == v)) run++;
            if (v) plat_rect(x + sx, y + dy, run, 1, pal[v - 1]);
            sx += run;
        }
    }
}

/* ---- fair scenery ---------------------------------------------------------- */
void mg_tent(int y0, int y1, uint8_t a, uint8_t b, uint8_t trim, int t)
{
    int W = mg_w(), sw = mg_big() ? 20 : 16, x, i = 0, sway = ui_wave(t, 150, 2);
    int vh = mg_big() ? 10 : 8;
    for (x = -sw + sway; x < W; x += sw, i++) {
        uint8_t c = (i & 1) ? a : b;
        plat_rect(x, y0, sw, y1 - y0, c);
        plat_rect(x, y0, 1, y1 - y0, ui_lighter(c));
        plat_rect(x + sw - 1, y0, 1, y1 - y0, ui_darker(c));
    }
    /* valance: a band with hanging scallops and a gold rope */
    plat_rect(0, y0, W, vh, trim);
    plat_rect(0, y0 + vh - 1, W, 1, ui_darker(trim));
    for (x = sw / 2 + sway, i = 0; x < W + sw; x += sw, i++) {
        ui_fill_circle(x, y0 + vh, sw / 2 - 1, ui_darker(trim));
        ui_fill_circle(x, y0 + vh - 1, sw / 2 - 2, trim);
        plat_rect(x - 2, y0 + vh - 2, 3, 1, ui_lighter(trim));
    }
    plat_rect(0, y0 + 2, W, 1, PAL_UI_GOLD);
    plat_rect(0, y0 + 3, W, 1, PAL_RAMP(RAMP_CANDY, 2));
}

void mg_bunting(int x0, int x1, int y, int sag, int t)
{
    int span = x1 - x0, n = span / 14, i;
    if (n < 2) return;
    for (i = 0; i <= n * 2; i++) {               /* the string */
        int px = x0 + span * i / (n * 2);
        int u = i - n, py = y + sag - sag * u * u / (n * n);
        plat_rect(px, py, span / (n * 2) + 1, 1, PAL_RAMP(RAMP_WOOD, 2));
    }
    for (i = 0; i < n; i++) {
        int px = x0 + span * (2 * i + 1) / (n * 2), u = 2 * i + 1 - n;
        int py = y + sag - sag * u * u / (n * n) + 1;
        int sw = ui_wave(t + i * 13, 50, 1), r;
        uint8_t c = mg.hell ? (i & 1 ? PAL_RAMP(RAMP_BLOOD, 4) : PAL_RAMP(RAMP_STONE, 1)) : flag_cols[i % 6];
        for (r = 0; r < 7; r++) {
            int hw = 4 - r * 4 / 7;
            plat_rect(px - hw + (r * sw) / 3, py + r, hw * 2 + 1, 1, r == 0 ? ui_lighter(c) : c);
        }
    }
}

void mg_bulbs(int x, int y, int w, int h, int t)
{
    int per = 2 * (w + h) / 8, i, phase = t / 4;
    for (i = 0; i < per; i++) {
        int d = i * 8, bx, by, lit = (i + phase) % 3 == 0;
        if (d < w) { bx = x + d; by = y; }
        else if (d < w + h) { bx = x + w - 1; by = y + d - w; }
        else if (d < 2 * w + h) { bx = x + w - 1 - (d - w - h); by = y + h - 1; }
        else { bx = x; by = y + h - 1 - (d - 2 * w - h); }
        plat_rect(bx - 1, by, 3, 1, lit ? PAL_UI_GOLD : PAL_RAMP(RAMP_CANDY, 2));
        plat_rect(bx, by - 1, 1, 3, lit ? PAL_UI_GOLD : PAL_RAMP(RAMP_CANDY, 2));
        if (lit) plat_rect(bx, by, 1, 1, PAL_WHITE);
    }
}

void mg_shadow(int cx, int cy, int rx)
{
    int ry = rx / 3 + 1, y;
    for (y = -ry; y <= ry; y++) {
        int hw = rx * (ry * 2 - (y < 0 ? -y : y)) / (ry * 2);
        if (hw > 0) plat_checker(cx - hw, cy + y, hw * 2, 1, PAL_RAMP(RAMP_STONE, 1));
    }
}

static void ferris_wheel(int cx, int cy, int r, int t)
{
    int k, base = (t * 3) & DOOMO_ANG_MASK, gy = cy + r + r / 3;
    uint8_t leg = mg.hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_WHITE;
    uint8_t leg_d = mg.hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_RAMP(RAMP_PASTEL_PINK, 4);
    uint8_t spoke = mg.hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_RAMP(RAMP_PASTEL_PINK, 5);
    uint8_t rim = mg.hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_PASTEL_PINK, 3);
    /* A-frame legs (2 px) */
    plat_line(cx, cy, cx - r * 2 / 3, gy, leg_d);
    plat_line(cx + 1, cy, cx - r * 2 / 3 + 1, gy, leg);
    plat_line(cx, cy, cx + r * 2 / 3, gy, leg_d);
    plat_line(cx + 1, cy, cx + r * 2 / 3 + 1, gy, leg);
    for (k = 0; k < 8; k++) {
        int a = (base + k * 512) & DOOMO_ANG_MASK;
        plat_line(cx, cy, cx + FIX2INT(fx_cos(a) * r), cy + FIX2INT(fx_sin(a) * r), spoke);
    }
    for (k = 0; k < 40; k++) {
        int a = (base + k * 4096 / 40) & DOOMO_ANG_MASK;
        int x = cx + FIX2INT(fx_cos(a) * r), y = cy + FIX2INT(fx_sin(a) * r);
        int lit = (k & 3) == 0;
        uint8_t c = rim;
        if (lit) c = mg.hell ? (((k / 4 + t / 6) & 1) ? PAL_RAMP(RAMP_LAVA, 6) : PAL_RAMP(RAMP_BLOOD, 4))
                             : (((k / 4 + t / 8) & 1) ? PAL_WHITE : PAL_UI_GOLD);
        plat_rect(x, y, 2, 2, c);
    }
    for (k = 0; k < 8; k++) {
        int a = (base + k * 512 + 256) & DOOMO_ANG_MASK;
        int gx = cx + FIX2INT(fx_cos(a) * r), gy2 = cy + FIX2INT(fx_sin(a) * r);
        uint8_t c = mg.hell ? PAL_RAMP(RAMP_BLOOD, 2 + (k & 1)) : flag_cols[k % 6];
        int sw = ui_wave(t + k * 20, 70, 1);
        plat_rect(gx + sw, gy2 + 1, 1, 2, spoke);
        ui_round_rect(gx - 3 + sw, gy2 + 3, 7, 6, 2, ui_darker(c));
        plat_rect(gx - 2 + sw, gy2 + 4, 5, 2, c);
        plat_rect(gx - 2 + sw, gy2 + 4, 5, 1, ui_lighter(c));
    }
    ui_fill_circle(cx, cy, 3, mg.hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_RAMP(RAMP_PASTEL_PINK, 4));
    ui_fill_circle(cx, cy, 2, mg.hell ? PAL_RAMP(RAMP_LAVA, 4) : PAL_UI_GOLD);
}

static void cloud(int cx, int cy, int s)
{
    uint8_t c = mg.hell ? PAL_RAMP(RAMP_VAPE, 1) : PAL_WHITE;
    uint8_t d = mg.hell ? PAL_RAMP(RAMP_VAPE, 0) : PAL_RAMP(RAMP_SKY, 6);
    ui_fill_ellipse(cx, cy + 1, s * 2, s - 1, d);
    ui_fill_ellipse(cx - s, cy, s, s - 2, c);
    ui_fill_circle(cx, cy - 2, s - 1, c);
    ui_fill_ellipse(cx + s, cy, s, s - 3, c);
}

static void far_tent(int x, int ground, int w, uint8_t c, int t)
{
    int h = w * 3 / 4, r;
    uint8_t c2 = mg.hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_WHITE;
    for (r = 0; r < h / 2; r++) {
        int hw = 1 + r * (w / 2) / (h / 2);
        plat_rect(x - hw, ground - h + r, hw * 2, 1, ((r / 2) & 1) ? c : c2);
    }
    plat_rect(x - w / 2, ground - h / 2, w, h / 2, ui_darker(c));
    plat_rect(x - w / 2 + 2, ground - h / 2, w / 4, h / 2, c);
    plat_rect(x + w / 8, ground - h / 2, w / 4, h / 2, c);
    plat_rect(x - 2, ground - h / 3, 4, h / 3, PAL_RAMP(RAMP_STONE, mg.hell ? 0 : 2));
    plat_rect(x, ground - h - 4, 1, 4, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(x + 1, ground - h - 4 + (ui_wave(t, 30, 1) & 1), 3, 2, mg.hell ? PAL_RAMP(RAMP_LAVA, 4) : PAL_PAINT(PAINT_CHERRY, 1));
}

void mg_fair_sky(int y0, int y1, int t)
{
    int W = mg_w(), h = y1 - y0, b = h / 4, i;
    static const uint8_t day[4] = {
        PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7), PAL_RAMP(RAMP_PASTEL_PINK, 7)
    };
    static const uint8_t hell[4] = {
        PAL_RAMP(RAMP_BLOOD, 0), PAL_RAMP(RAMP_BLOOD, 1), PAL_RAMP(RAMP_BLOOD, 3), PAL_RAMP(RAMP_LAVA, 3)
    };
    const uint8_t *c = mg.hell ? hell : day;
    for (i = 0; i < 4; i++) {
        plat_rect(0, y0 + i * b, W, i == 3 ? h - 3 * b : b, c[i]);
        if (i) plat_checker(0, y0 + i * b - 1, W, 1, c[i]);
        if (i) plat_checker(1, y0 + i * b, W, 1, c[i - 1]);
    }
    if (mg.hell) {
        ui_fill_circle(W / 5, y0 + h / 4, h / 10 + 2, PAL_RAMP(RAMP_BLOOD, 5));
        ui_fill_circle(W / 5 - 2, y0 + h / 4 - 1, h / 10, PAL_RAMP(RAMP_BLOOD, 6));
    } else {
        ui_fill_circle(W / 6, y0 + h / 4, h / 13 + 3, PAL_RAMP(RAMP_CANDY, 6));
        ui_fill_circle(W / 6, y0 + h / 4, h / 13 + 1, PAL_RAMP(RAMP_CANDY, 7));
    }
    for (i = 0; i < 3; i++) {
        int s = 5 + i * 2 + (mg_big() ? 2 : 0);
        int span = W + 60, cx = (int)((uint32_t)(t / (3 + i) + i * 97) % (uint32_t)span) - 30;
        cloud(cx, y0 + 10 + i * (h / 6), s);
    }
    {
        int r = DOOMO_MIN(h * 3 / 10, mg_big() ? 52 : 40);
        ferris_wheel(W * 3 / 4, y1 - r - r / 3 - 1, r, t);
    }
    far_tent(W / 3, y1, mg_big() ? 40 : 30, mg.hell ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_PAINT(PAINT_CHERRY, 1), t);
    far_tent(W / 2 + 10, y1, mg_big() ? 30 : 22, mg.hell ? PAL_RAMP(RAMP_STONE, 2) : PAL_PAINT(PAINT_SKY, 1), t + 15);
    far_tent(W - 14, y1, mg_big() ? 34 : 26, mg.hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_PAINT(PAINT_LAVENDER, 1), t + 30);
    if (mg.hell)
        for (i = 0; i < 8; i++) {
            uint32_t s = doomo_hash32((uint32_t)i * 7177u + 3u);
            int ex = (int)(s % (uint32_t)W) + ui_wave(t + i * 17, 50, 4);
            int ey = y1 - (int)(((uint32_t)t * (1 + i % 3) + (s >> 9)) % (uint32_t)h);
            plat_rect(ex, ey, 1 + (i & 1), 1 + (i & 1), PAL_RAMP(RAMP_LAVA, 5 + (i & 1)));
        }
}

/* ---- treats ------------------------------------------------------------------ */
static void sprinkles(int cx, int cy, int r, int seed)
{
    int k;
    for (k = 0; k < 5; k++) {
        uint32_t h = doomo_hash32((uint32_t)(seed * 31 + k));
        int dx = (int)(h % (uint32_t)(r * 2 + 1)) - r, dy = (int)((h >> 8) % (uint32_t)(r + 1)) - r / 2;
        plat_rect(cx + dx * 2 / 3, cy + dy / 2, (k & 1) + 1, 2 - (k & 1), flag_cols[k]);
    }
}

void mg_treat(int kind, int cx, int cy, int r, int sx256, int sy256, int t)
{
    int rx = r * sx256 / 256, ry = r * sy256 / 256;
    if (rx < 2) rx = 2;
    if (ry < 2) ry = 2;
    switch (kind) {
    case TREAT_CUPCAKE: {
        int wy = cy, wh = ry, k;
        /* wrapper: a trapezoid of pleats */
        for (k = 0; k < wh; k++) {
            int hw = rx * 3 / 4 - k * rx / (4 * wh);
            plat_rect(cx - hw - 1, wy + k, hw * 2 + 2, 1, PAL_PAINT(PAINT_CHERRY, 0));
            plat_rect(cx - hw, wy + k, hw * 2, 1, PAL_PAINT(PAINT_SKY, 1));
            plat_rect(cx - hw / 2, wy + k, 1, 1, PAL_PAINT(PAINT_SKY, 2));
            plat_rect(cx + hw / 2, wy + k, 1, 1, PAL_PAINT(PAINT_SKY, 0));
        }
        plat_rect(cx - rx * 3 / 4 + 1, wy + wh, rx * 3 / 2 - 2, 1, PAL_PAINT(PAINT_COBALT, 0));
        /* frosting swirl */
        ui_fill_ellipse(cx, cy - 1, rx, ry * 2 / 3, PAL_PAINT(PAINT_ROSE, 0));
        ui_fill_ellipse(cx, cy - 2, rx - 1, ry * 2 / 3 - 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        ui_fill_ellipse(cx, cy - ry / 2 - 1, rx * 2 / 3, ry / 2, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        ui_fill_ellipse(cx - 1, cy - ry / 2 - 2, rx / 2, ry / 3, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        sprinkles(cx, cy - 2, rx - 2, kind + 3);
        ui_fill_circle(cx + 1, cy - ry - 2, 2, PAL_PAINT(PAINT_CHERRY, 1));
        plat_rect(cx + 1, cy - ry - 3, 1, 1, PAL_WHITE);
        plat_rect(cx + 2, cy - ry - 6, 1, 3, PAL_PAINT(PAINT_FOREST, 1));
        break;
    }
    case TREAT_DONUT:
        ui_fill_ellipse(cx, cy + 1, rx + 1, ry, PAL_PAINT(PAINT_CHOCOLATE, 1));
        ui_fill_ellipse(cx, cy, rx, ry - 1, PAL_PAINT(PAINT_CARAMEL, 1));
        ui_fill_ellipse(cx, cy - 1, rx - 1, ry - 2, PAL_PAINT(PAINT_MAGENTA, 2));
        ui_fill_ellipse(cx - 1, cy - 2, rx / 2, ry / 3, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        sprinkles(cx, cy, rx - 2, 7);
        ui_fill_ellipse(cx, cy, rx / 3 + 1, ry / 3 + 1, PAL_PAINT(PAINT_CHOCOLATE, 0));
        ui_fill_ellipse(cx, cy + 1, rx / 3, ry / 4, PAL_PAINT(PAINT_CHOCOLATE, 1));
        break;
    case TREAT_COOKIE:
        ui_fill_ellipse(cx, cy, rx + 1, ry + 1, PAL_PAINT(PAINT_CHOCOLATE, 1));
        ui_fill_ellipse(cx, cy, rx, ry, PAL_PAINT(PAINT_CARAMEL, 1));
        ui_fill_ellipse(cx - 1, cy - 1, rx - 2, ry - 2, PAL_PAINT(PAINT_CARAMEL, 2));
        plat_rect(cx - rx / 2, cy - ry / 3, 2, 2, PAL_PAINT(PAINT_CHOCOLATE, 0));
        plat_rect(cx + rx / 3, cy - ry / 2, 2, 2, PAL_PAINT(PAINT_CHOCOLATE, 0));
        plat_rect(cx, cy + ry / 3, 2, 2, PAL_PAINT(PAINT_CHOCOLATE, 0));
        plat_rect(cx - rx / 4, cy + 1, 2, 1, PAL_PAINT(PAINT_CHOCOLATE, 1));
        break;
    case TREAT_CANDY: {
        int k, sp = (t / 3) & 3;
        for (k = 0; k < 4; k++) {
            int hw = k + 1;
            plat_rect(cx - rx - 4 + k, cy - hw, 1, hw * 2 + 1, PAL_PAINT(PAINT_LEMON, k == 0 ? 0 : 1));
            plat_rect(cx + rx + 3 - k, cy - hw, 1, hw * 2 + 1, PAL_PAINT(PAINT_LEMON, k == 0 ? 0 : 1));
        }
        ui_fill_circle(cx, cy, rx, PAL_PAINT(PAINT_CHERRY, 0));
        ui_fill_circle(cx, cy, rx - 1, PAL_WHITE);
        for (k = 0; k < 4; k++) {
            int a = (k * 1024 + sp * 256) & DOOMO_ANG_MASK;
            int px = cx + FIX2INT(fx_cos(a) * (rx - 2)), py = cy + FIX2INT(fx_sin(a) * (rx - 2));
            plat_line(cx, cy, px, py, PAL_PAINT(PAINT_CHERRY, 1));
        }
        plat_rect(cx - rx / 2, cy - rx / 2, 2, 1, PAL_WHITE);
        break;
    }
    case TREAT_STAR:
        ui_star(cx, cy, rx + 1, PAL_UI_GOLD);
        ui_sparkle(cx + rx, cy - ry, (t / 2) & 15, PAL_WHITE);
        break;
    case TREAT_HEART:
        ui_heart_squash(cx, cy, rx > 3 ? rx : 3, sx256, sy256, PAL_PAINT(PAINT_ROSE, 1));
        break;
    case TREAT_BAGEL: {
        int k;
        /* charred brimstone bagel: glowing cracks, flickering flames */
        for (k = 0; k < 3; k++) {
            int fx2 = cx - rx / 2 + k * rx / 2, fh = 3 + ((t / 2 + k * 3) % 4);
            plat_rect(fx2 - 1, cy - ry - fh + 1, 3, fh, PAL_RAMP(RAMP_LAVA, 4));
            plat_rect(fx2, cy - ry - fh - 1, 1, fh, PAL_RAMP(RAMP_LAVA, 6));
        }
        ui_fill_ellipse(cx, cy + 1, rx + 1, ry, PAL_RAMP(RAMP_STONE, 0));
        ui_fill_ellipse(cx, cy, rx, ry - 1, PAL_RAMP(RAMP_WOOD, 1));
        ui_fill_ellipse(cx - 1, cy - 1, rx - 2, ry - 3, PAL_RAMP(RAMP_WOOD, 2));
        plat_rect(cx - rx + 2, cy, rx / 2, 1, PAL_RAMP(RAMP_LAVA, 5));
        plat_rect(cx + rx / 3, cy - ry / 2 + 1, rx / 2, 1, PAL_RAMP(RAMP_LAVA, (t / 3) & 1 ? 6 : 4));
        plat_rect(cx - 1, cy + ry / 2 - 1, 3, 1, PAL_RAMP(RAMP_LAVA, 5));
        ui_fill_ellipse(cx, cy, rx / 3 + 1, ry / 3 + 1, PAL_RAMP(RAMP_STONE, 0));
        ui_fill_ellipse(cx, cy, rx / 3, ry / 3, PAL_RAMP(RAMP_LAVA, 2));
        /* angry little eyes: it is a hell bagel after all */
        plat_rect(cx - rx / 2 - 1, cy - 2, 2, 2, PAL_RAMP(RAMP_LAVA, 7));
        plat_rect(cx + rx / 2, cy - 2, 2, 2, PAL_RAMP(RAMP_LAVA, 7));
        break;
    }
    default:
        break;
    }
}

/* ---- characters --------------------------------------------------------------- */
static void eye(int x, int y, int r, int look, int blink, uint8_t iris)
{
    if (blink) { plat_rect(x - r, y, r * 2 + 1, 1, OUTLINE_DARK); return; }
    ui_fill_ellipse(x, y, r, r + 1, OUTLINE_DARK);
    ui_fill_ellipse(x, y, r - 1, r, PAL_WHITE);
    ui_fill_circle(x + look, y + 1, r > 2 ? r - 1 : 1, iris);
    plat_rect(x + look - 1, y - 1, 1, 1, PAL_WHITE);
}

void mg_imp(int cx, int cy, int rx, int ry, int t, int mood)
{
    int gold = (mood & IMP_GOLD) != 0;
    uint8_t base = gold ? PAL_PAINT(PAINT_HONEY, 1) : PAL_PAINT(PAINT_CHERRY, 1);
    uint8_t dark = gold ? PAL_PAINT(PAINT_RUST, 1) : PAL_PAINT(PAINT_BLOOD, 1);
    uint8_t lite = gold ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_CHERRY, 2);
    int flap = ui_wave(t * 2, 16, 2), k, er = rx > 9 ? 3 : 2;
    int blink;
    mood &= 7;
    blink = mood != IMP_BONK && (t % 70) < 3;
    /* bat wings */
    for (k = 0; k < 4; k++) {
        int wy = cy - ry / 2 + k * 2 - flap * (3 - k) / 3, wl = rx / 2 + 3 - k;
        plat_rect(cx - rx - wl + 2, wy, wl, 2, k == 3 ? dark : PAL_PAINT(PAINT_GRAPE, 1));
        plat_rect(cx + rx - 2, wy, wl, 2, k == 3 ? dark : PAL_PAINT(PAINT_GRAPE, 1));
    }
    /* horns */
    for (k = 0; k < 4; k++) {
        int hy = cy - ry - 3 + k, hw = k / 2 + 1;
        plat_rect(cx - rx / 2 - hw / 2 - (3 - k) / 2, hy, hw, 1, PAL_RAMP(RAMP_BONE, 6 - k / 2));
        plat_rect(cx + rx / 2 - hw / 2 + (3 - k) / 2, hy, hw, 1, PAL_RAMP(RAMP_BONE, 6 - k / 2));
    }
    /* body: outline, base, rim light, belly fluff */
    ui_fill_ellipse(cx, cy, rx + 1, ry + 1, dark);
    ui_fill_ellipse(cx, cy, rx, ry, base);
    ui_fill_ellipse(cx - rx / 4, cy - ry / 3, rx / 2, ry / 3, lite);
    ui_fill_ellipse(cx, cy + ry / 2, rx / 2, ry / 3, gold ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_PEACH, 2));
    plat_rect(cx - rx + 2, cy - 1, 1, 3, lite);
    /* face */
    if (mood == IMP_BONK) {
        int ex;
        for (ex = -1; ex <= 1; ex += 2) {
            int x = cx + ex * rx / 3, y = cy - ry / 6;
            plat_line(x - 2, y - 2, x + 2, y + 2, OUTLINE_DARK);
            plat_line(x - 2, y + 2, x + 2, y - 2, OUTLINE_DARK);
        }
        ui_fill_ellipse(cx, cy + ry / 3, 2, 2, PAL_PAINT(PAINT_BLOOD, 0));
        for (k = 0; k < 3; k++) {          /* orbiting stars */
            int a = (t * 160 + k * 1365) & DOOMO_ANG_MASK;
            ui_sparkle(cx + FIX2INT(fx_cos(a) * (rx + 2)), cy - ry - 2 + FIX2INT(fx_sin(a) * 3), 6, PAL_UI_GOLD);
        }
    } else {
        int look = ui_wave(t, 90, 1);
        uint8_t iris = gold ? PAL_PAINT(PAINT_RUST, 0) : PAL_RAMP(RAMP_STONE, 0);
        eye(cx - rx / 3, cy - ry / 5, er, look, blink, iris);
        eye(cx + rx / 3, cy - ry / 5, er, look, blink, iris);
        if (mood == IMP_ANGRY) {
            plat_line(cx - rx / 3 - er, cy - ry / 5 - er - 2, cx - rx / 3 + er, cy - ry / 5 - er, OUTLINE_DARK);
            plat_line(cx + rx / 3 + er, cy - ry / 5 - er - 2, cx + rx / 3 - er, cy - ry / 5 - er, OUTLINE_DARK);
        } else if (mood == IMP_SNEAKY) {
            plat_rect(cx - rx / 3 - er, cy - ry / 5 - er - 1, er * 2 + 1, er + 1, base);
            plat_rect(cx + rx / 3 - er, cy - ry / 5 - er - 1, er * 2 + 1, er + 1, base);
        }
        plat_rect(cx - 2, cy + ry / 4, 5, 1, OUTLINE_DARK);
        plat_rect(cx - 3, cy + ry / 4 - 1, 1, 1, OUTLINE_DARK);
        plat_rect(cx + 3, cy + ry / 4 - 1, 1, 1, OUTLINE_DARK);
        plat_rect(cx + 1, cy + ry / 4 + 1, 1, 1, PAL_WHITE);       /* fang */
        plat_rect(cx - rx / 2 - 1, cy + 1, 2, 1, PAL_PAINT(PAINT_ROSE, 2));
        plat_rect(cx + rx / 2, cy + 1, 2, 1, PAL_PAINT(PAINT_ROSE, 2));
    }
    if (gold) ui_sparkle(cx + rx, cy - ry, (t / 2) & 15, PAL_WHITE);
}

void mg_wraith(int cx, int cy, int r, int t, int hurt)
{
    int bob = ui_wave(t, 40, 2), k;
    uint8_t d = PAL_RAMP(RAMP_VAPE, 3), b = PAL_RAMP(RAMP_VAPE, 5), l = PAL_RAMP(RAMP_VAPE, 6);
    cy += bob;
    if (hurt) { d = PAL_PAINT(PAINT_GRAPE, 0); b = PAL_PAINT(PAINT_VAPE_PURPLE, 1); l = PAL_PAINT(PAINT_VAPE_PURPLE, 2); }
    /* wispy tail */
    for (k = 3; k >= 1; k--) {
        int tx = cx + ui_wave(t + k * 10, 30, 2) - k, ty = cy + r + k * 3;
        ui_fill_circle(tx, ty, r / 2 - k + 2, d);
        ui_fill_circle(tx, ty - 1, r / 2 - k + 1, b);
    }
    ui_fill_circle(cx - r / 2, cy + 1, r * 2 / 3 + 1, d);
    ui_fill_circle(cx + r / 2, cy + 1, r * 2 / 3 + 1, d);
    ui_fill_circle(cx, cy - r / 3, r * 3 / 4 + 1, d);
    ui_fill_circle(cx - r / 2, cy + 1, r * 2 / 3, b);
    ui_fill_circle(cx + r / 2, cy + 1, r * 2 / 3, b);
    ui_fill_circle(cx, cy - r / 3, r * 3 / 4, b);
    ui_fill_ellipse(cx - r / 4, cy - r / 2, r / 3, r / 4, l);
    /* face: sleepy cool eyes, an "o" mouth blowing a ring */
    if (hurt) {
        plat_line(cx - r / 2 - 2, cy - 3, cx - r / 2 + 2, cy - 1, OUTLINE_DARK);
        plat_line(cx + r / 2 + 2, cy - 3, cx + r / 2 - 2, cy - 1, OUTLINE_DARK);
    } else {
        plat_rect(cx - r / 2 - 2, cy - 2, 5, 2, OUTLINE_DARK);
        plat_rect(cx + r / 2 - 2, cy - 2, 5, 2, OUTLINE_DARK);
        plat_rect(cx - r / 2 - 1, cy - 2, 1, 1, PAL_WHITE);
        plat_rect(cx + r / 2 - 1, cy - 2, 1, 1, PAL_WHITE);
    }
    ui_fill_circle(cx + 1, cy + r / 3, 2, OUTLINE_DARK);
    plat_rect(cx + 1, cy + r / 3, 1, 1, PAL_PAINT(PAINT_VAPE_PURPLE, 2));
    /* the vape pen with a glowing tip */
    plat_rect(cx + r / 2 + 2, cy + r / 3, 6, 2, PAL_RAMP(RAMP_STEEL, 4));
    plat_rect(cx + r / 2 + 8, cy + r / 3, 1, 2, (t / 4) & 1 ? PAL_PAINT(PAINT_MAGENTA, 2) : PAL_PAINT(PAINT_VAPE_PURPLE, 2));
    if ((t / 8) & 1) ui_fill_circle(cx + r / 2 + 11, cy + r / 3 - 3 - (t & 7) / 2, 2, PAL_RAMP(RAMP_VAPE, 6));
}

void mg_fuzz(int cx, int cy, int r, int t, int ouch)
{
    uint8_t fur = PAL_PAINT(PAINT_CARAMEL, 1), fur_d = PAL_PAINT(PAINT_CHOCOLATE, 1);
    uint8_t fur_l = PAL_PAINT(PAINT_CARAMEL, 2);
    int er = r / 3 + 1;
    /* robe + collar + tiny gold cross */
    ui_fill_ellipse(cx, cy + r + 2, r + 2, r / 2 + 2, PAL_RAMP(RAMP_STONE, 0));
    ui_fill_ellipse(cx, cy + r + 1, r + 1, r / 2 + 1, PAL_RAMP(RAMP_STONE, 2));
    plat_rect(cx - 3, cy + r - 2, 7, 3, PAL_WHITE);
    plat_rect(cx, cy + r + 2, 1, 5, PAL_UI_GOLD);
    plat_rect(cx - 1, cy + r + 3, 3, 1, PAL_UI_GOLD);
    /* ears */
    ui_fill_circle(cx - r + 2, cy - r + 2, er + 1, fur_d);
    ui_fill_circle(cx + r - 2, cy - r + 2, er + 1, fur_d);
    ui_fill_circle(cx - r + 2, cy - r + 2, er, fur);
    ui_fill_circle(cx + r - 2, cy - r + 2, er, fur);
    ui_fill_circle(cx - r + 2, cy - r + 3, er / 2, PAL_PAINT(PAINT_PEACH, 2));
    ui_fill_circle(cx + r - 2, cy - r + 3, er / 2, PAL_PAINT(PAINT_PEACH, 2));
    /* head */
    ui_fill_circle(cx, cy, r + 1, fur_d);
    ui_fill_circle(cx, cy, r, fur);
    ui_fill_ellipse(cx - r / 3, cy - r / 2, r / 2, r / 3, fur_l);
    plat_rect(cx - 1, cy - r - 1, 1, 2, fur);       /* fluff tuft */
    plat_rect(cx + 1, cy - r - 2, 1, 3, fur);
    ui_fill_ellipse(cx, cy + r / 3, r / 2, r / 3, PAL_PAINT(PAINT_BONE, 2));
    plat_rect(cx - 1, cy + r / 6, 3, 2, PAL_RAMP(RAMP_STONE, 0));
    if (ouch) {
        plat_line(cx - r / 2 - 2, cy - 4, cx - r / 2 + 1, cy - 2, OUTLINE_DARK);
        plat_line(cx - r / 2 - 2, cy, cx - r / 2 + 1, cy - 2, OUTLINE_DARK);
        plat_line(cx + r / 2 + 2, cy - 4, cx + r / 2 - 1, cy - 2, OUTLINE_DARK);
        plat_line(cx + r / 2 + 2, cy, cx + r / 2 - 1, cy - 2, OUTLINE_DARK);
        plat_rect(cx - 2, cy + r / 2, 5, 2, PAL_PAINT(PAINT_BLOOD, 1));
    } else {
        int blink = (t % 80) < 3;
        eye(cx - r / 2, cy - 2, 2, 0, blink, PAL_RAMP(RAMP_STONE, 0));
        eye(cx + r / 2, cy - 2, 2, 0, blink, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(cx - 2, cy + r / 2, 1, 1, OUTLINE_DARK);
        plat_rect(cx - 1, cy + r / 2 + 1, 3, 1, OUTLINE_DARK);
        plat_rect(cx + 2, cy + r / 2, 1, 1, OUTLINE_DARK);
        plat_rect(cx - r / 2 - 2, cy + 2, 3, 1, PAL_PAINT(PAINT_ROSE, 2));
        plat_rect(cx + r / 2, cy + 2, 3, 1, PAL_PAINT(PAINT_ROSE, 2));
    }
    ui_sparkle(cx + r + 2, cy - r, (t / 3) & 15, PAL_UI_GOLD);
}

void mg_mascot(int cx, int cy, int r, int t)
{
    int hop = ui_wave(t, 30, 2), sq = hop < 0 ? 1 : 0, k;
    uint8_t p = PAL_PAINT(PAINT_BUBBLEGUM, 1), pd = PAL_PAINT(PAINT_ROSE, 0);
    cy += hop;
    /* bunny ears with a twitch */
    for (k = -1; k <= 1; k += 2) {
        int tw = k > 0 && (t % 90) < 6 ? 2 : 0;
        ui_fill_ellipse(cx + k * r / 2 + tw, cy - r - r / 2, r / 4 + 1, r / 2 + 1, pd);
        ui_fill_ellipse(cx + k * r / 2 + tw, cy - r - r / 2, r / 4, r / 2, p);
        ui_fill_ellipse(cx + k * r / 2 + tw, cy - r - r / 2 + 1, r / 8 + 1, r / 3, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    }
    /* ticket body with notches */
    ui_round_rect(cx - r - 1, cy - r - 1 + sq, r * 2 + 2, r * 2 + 2 - sq, 4, pd);
    ui_round_rect(cx - r, cy - r + sq, r * 2, r * 2 - sq, 3, p);
    plat_rect(cx - r + 2, cy - r + 1 + sq, r * 2 - 4, 1, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    ui_fill_circle(cx - r - 1, cy + 2, 2, pd);
    ui_fill_circle(cx + r + 1, cy + 2, 2, pd);
    for (k = -r + 3; k < r - 2; k += 3) plat_rect(cx + k, cy + r / 2 + 3, 1, 1, pd);
    eye(cx - r / 3, cy - 1, 2, ui_wave(t, 100, 1), (t % 75) < 3, PAL_RAMP(RAMP_STONE, 0));
    eye(cx + r / 3, cy - 1, 2, ui_wave(t, 100, 1), (t % 75) < 3, PAL_RAMP(RAMP_STONE, 0));
    plat_rect(cx - 1, cy + 3, 3, 1, OUTLINE_DARK);
    plat_rect(cx - r / 2 - 2, cy + 2, 3, 1, PAL_PAINT(PAINT_CHERRY, 2));
    plat_rect(cx + r / 2, cy + 2, 3, 1, PAL_PAINT(PAINT_CHERRY, 2));
}

/* ---- hosts --------------------------------------------------------------------- */
int mg_host_alive(int id)
{
    return town_alive(id);
}

const char *mg_host_name(int id)
{
    const DoomoResident *r = town_res(id);
    if (mg_host_alive(id) && r && r->name[0]) return r->name;
    return "Tix";
}

void mg_host_frame(int x, int y, int id, int expr, int t)
{
    int rows = mg_big() ? 50 : 42, w = 56, h = rows + 16, bob = ui_wave(t, 64, 1);
    y += bob;
    ui_round_rect(x + 2, y + 3, w, h, 3, PAL_RAMP(RAMP_STONE, 1));
    ui_round_rect(x, y, w, h, 3, PAL_RAMP(RAMP_BONE, 5));
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 2, PAL_RAMP(RAMP_BONE, 7));
    plat_rect(x + 4, y + 4, 48, rows, mg.hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_PAINT(PAINT_SKY, 2));
    plat_rect(x + 4, y + 4 + rows * 2 / 3, 48, rows - rows * 2 / 3, mg.hell ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_PAINT(PAINT_MINT, 2));
    if (mg_host_alive(id)) {
        const DoomoImage *img = portrait_image(id, expr);
        if (img) {
            DoomoImage c = mg_crop(img, 0, rows);
            plat_image(x + 4, y + 4, &c, 1, 0, 0);
        }
    } else {
        mg_mascot(x + 28, y + 4 + rows / 2 + 4, 11, t);
    }
    /* tape strips */
    plat_checker(x - 3, y - 2, 12, 5, PAL_RAMP(RAMP_CANDY, 6));
    plat_checker(x + w - 9, y - 2, 12, 5, PAL_RAMP(RAMP_CANDY, 6));
    {
        const char *n = mg_host_name(id);
        int nw = ui_text_width(n) + 10, nx = x + w / 2 - nw / 2, ny = y + h - 11;
        ui_round_rect(nx, ny, nw, 12, 5, PAL_PAINT(PAINT_ROSE, 0));
        ui_round_rect(nx + 1, ny + 1, nw - 2, 10, 4, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        ui_text(nx + 5, ny + 2, n, PAL_WHITE);
    }
}

/* ---- HUD -------------------------------------------------------------------------- */
void mg_draw_hearts(int x, int y, int n, int max)
{
    int i;
    for (i = 0; i < max; i++)
        ui_heart(x + i * 11, y, 4, i < n ? PAL_PAINT(PAINT_ROSE, 1) : PAL_GRAY);
}

void mg_medal_icon(int cx, int cy, int medal, int t)
{
    static const uint8_t base[4] = { PAL_GRAY, PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_SILVER, 2), PAL_UI_GOLD };
    uint8_t c = base[medal & 3];
    plat_rect(cx - 3, cy + 2, 2, 6, medal ? PAL_PAINT(PAINT_SKY, 1) : PAL_GRAY_D);
    plat_rect(cx + 2, cy + 2, 2, 6, medal ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_GRAY_D);
    ui_fill_circle(cx, cy, 5, medal ? ui_darker(c) : PAL_GRAY_D);
    ui_fill_circle(cx, cy, 4, c);
    if (medal) {
        ui_star(cx, cy, 2, ui_lighter(c));
        if (medal == MG_MEDAL_GOLD) ui_sparkle(cx + 4, cy - 4, (t / 2) & 15, PAL_WHITE);
    } else {
        plat_rect(cx - 1, cy - 1, 3, 3, PAL_GRAY_L);
    }
}

void mg_hud(void)
{
    const MgGame *g = mg.g;
    int W = mg_w(), big = mg_big();
    char buf[32];
    int bump = mg.score_bump > 3 ? 1 : 0, x;
    if (mg.quiet_hud || plat_draw_skipped()) return;
    /* score pill */
    doomo_snprintf(buf, sizeof(buf), "%d", (int)mg.score);
    x = ui_text_width(buf) + 44;
    ui_round_rect(3, 3, x, 15, 7, PAL_RAMP(RAMP_STONE, 0));
    ui_round_rect(2, 2, x, 14, 7, mg.hell ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_PASTEL_PINK, 2));
    ui_round_rect(3, 3, x - 2, 12, 6, mg.hell ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_RAMP(RAMP_PASTEL_PINK, 5));
    ui_text(7, 5, "SCORE", PAL_WHITE);
    mg_text(40, 5 - bump, buf, bump ? PAL_UI_GOLD : PAL_WHITE);
    /* combo */
    if (mg.combo >= 2) {
        int cb = mg.combo_bump > 3 ? 1 : 0;
        doomo_snprintf(buf, sizeof(buf), "%d COMBO  x%d", mg.combo, mg_combo_mult());
        x = W / 2 - ui_text_width(buf) / 2 + (big ? 10 : 6);
        mg_text(x, 5 - cb + ui_wave(mg.play_t, 30, 1), buf,
                mg_combo_mult() >= 3 ? PAL_UI_GOLD : PAL_PAINT(PAINT_LEMON, 2));
    }
    /* time / lives */
    x = W - 4;
    if (g->time) {
        int secs = (mg.time_left + 29) / 30, bw = big ? 70 : 50;
        uint8_t c = secs <= 5 ? ((mg.play_t / 4) & 1 ? PAL_UI_RED : PAL_PAINT(PAINT_CHERRY, 2)) : PAL_PAINT(PAINT_MINT, 1);
        doomo_snprintf(buf, sizeof(buf), "%d", secs);
        x -= bw + 2;
        ui_round_rect(x - 1, 4, bw + 2, 8, 4, PAL_RAMP(RAMP_STONE, 0));
        ui_bar(x, 5, bw, 6, mg.time_left, g->time, c);
        mg_text(x - ui_text_width(buf) - 4, 4, buf, PAL_WHITE);
        x -= ui_text_width(buf) + 8;
    }
    if (g->lives) mg_draw_hearts(g->time ? W - 4 - g->lives * 11 + 4 : x - g->lives * 11 + 6,
                                 g->time ? 20 : 9, mg.lives, g->lives);
}
