/* tk.c - implementation of the trailer pixel-animation toolkit. */
#include "tk.h"
#include "pal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t tk_fb[TH][TW];

/* ------------------------------------------------------------------------ */
/* primitives                                                               */
/* ------------------------------------------------------------------------ */
void tk_clear(uint8_t c) { memset(tk_fb, c, sizeof tk_fb); }

void tk_px(int x, int y, uint8_t c) {
    if ((unsigned)x < TW && (unsigned)y < TH) tk_fb[y][x] = c;
}
uint8_t tk_get(int x, int y) {
    if ((unsigned)x < TW && (unsigned)y < TH) return tk_fb[y][x];
    return 0;
}
void tk_rect(int x, int y, int w, int h, uint8_t c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > TW ? TW : x + w, y1 = y + h > TH ? TH : y + h;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) tk_fb[j][i] = c;
}
void tk_frame(int x, int y, int w, int h, uint8_t c) {
    tk_rect(x, y, w, 1, c); tk_rect(x, y + h - 1, w, 1, c);
    tk_rect(x, y, 1, h, c); tk_rect(x + w - 1, y, 1, h, c);
}
void tk_line(int x0, int y0, int x1, int y1, uint8_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        tk_px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
void tk_ellipse(int cx, int cy, int rx, int ry, uint8_t c) {
    if (rx <= 0 || ry <= 0) { tk_px(cx, cy, c); return; }
    for (int y = -ry; y <= ry; y++)
        for (int x = -rx; x <= rx; x++)
            if ((float)(x * x) / ((rx + .5f) * (rx + .5f)) +
                (float)(y * y) / ((ry + .5f) * (ry + .5f)) <= 1.0f)
                tk_px(cx + x, cy + y, c);
}
void tk_circle(int cx, int cy, int r, uint8_t c) { tk_ellipse(cx, cy, r, r, c); }
void tk_ring(int cx, int cy, int r, uint8_t c) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        tk_px(cx + x, cy + y, c); tk_px(cx + y, cy + x, c);
        tk_px(cx - y, cy + x, c); tk_px(cx - x, cy + y, c);
        tk_px(cx - x, cy - y, c); tk_px(cx - y, cy - x, c);
        tk_px(cx + y, cy - x, c); tk_px(cx + x, cy - y, c);
        y++;
        if (err < 0) err += 2 * y + 1; else { x--; err += 2 * (y - x) + 1; }
    }
}
void tk_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t c) {
    int minx = x0, maxx = x0, miny = y0, maxy = y0;
    if (x1 < minx) { minx = x1; } if (x2 < minx) { minx = x2; }
    if (x1 > maxx) { maxx = x1; } if (x2 > maxx) { maxx = x2; }
    if (y1 < miny) { miny = y1; } if (y2 < miny) { miny = y2; }
    if (y1 > maxy) { maxy = y1; } if (y2 > maxy) { maxy = y2; }
    int area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (area == 0) { tk_line(x0, y0, x1, y1, c); tk_line(x1, y1, x2, y2, c); return; }
    for (int y = miny; y <= maxy; y++)
        for (int x = minx; x <= maxx; x++) {
            int w0 = (x1 - x) * (y2 - y) - (x2 - x) * (y1 - y);
            int w1 = (x2 - x) * (y0 - y) - (x0 - x) * (y2 - y);
            int w2 = (x0 - x) * (y1 - y) - (x1 - x) * (y0 - y);
            if ((area > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0))
                tk_px(x, y, c);
        }
}
static const uint8_t BAYER4[4][4] = {
    { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
void tk_dither_rect(int x, int y, int w, int h, uint8_t c, int level) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (BAYER4[j & 3][i & 3] < level) tk_px(i, j, c);
}
void tk_vgrad(int x, int y, int w, int h, const uint8_t *cols, int n) {
    if (n <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) {
        int pos = j * (n - 1) * 16 / (h > 1 ? h - 1 : 1);
        int a = pos >> 4, frac = pos & 15;
        if (a >= n - 1) { a = n - 1; frac = 0; }
        for (int i = 0; i < w; i++) {
            uint8_t c = cols[a];
            if (frac && BAYER4[(y + j) & 3][(x + i) & 3] < frac) c = cols[a + 1];
            tk_px(x + i, y + j, c);
        }
    }
}

/* ------------------------------------------------------------------------ */
/* font: 5x8, column-major, bit 0 = top row, ASCII 0x20..0x7E              */
/* ------------------------------------------------------------------------ */
static const uint8_t FONT[95][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x56,0x20,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},{0x2A,0x1C,0x7F,0x1C,0x2A},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x18,0xA4,0xA4,0xA4,0x7C},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x40,0x80,0x84,0x7D,0x00},
    {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0xFC,0x24,0x24,0x24,0x18},
    {0x18,0x24,0x24,0x18,0xFC},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x1C,0xA0,0xA0,0xA0,0x7C},
    {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00},{0x08,0x04,0x08,0x10,0x08},
};

static void glyph(int x, int y, unsigned ch, uint8_t c, int s) {
    if (ch == 0x7F) { /* bonus glyph: heart */
        static const uint8_t H[5] = {0x0C, 0x1E, 0x3C, 0x1E, 0x0C};
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 8; row++)
                if (H[col] >> row & 1) tk_rect(x + col * s, y + row * s, s, s, c);
        return;
    }
    if (ch < 0x20 || ch > 0x7E) ch = '?';
    const uint8_t *g = FONT[ch - 0x20];
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 8; row++)
            if (g[col] >> row & 1) tk_rect(x + col * s, y + row * s, s, s, c);
}
int tk_text_w(const char *s, int scale) {
    int best = 0, cur = 0;
    for (; *s; s++) {
        if (*s == '\n') { if (cur > best) best = cur; cur = 0; continue; }
        cur += 6 * scale;
    }
    if (cur > best) best = cur;
    return best ? best - scale : 0;
}
int tk_text(int x, int y, const char *s, uint8_t c, int scale) {
    int cx = x;
    for (; *s; s++) {
        if (*s == '\n') { cx = x; y += 9 * scale; continue; }
        glyph(cx, y, (unsigned char)*s, c, scale);
        cx += 6 * scale;
    }
    return cx - x;
}
void tk_text_outline(int x, int y, const char *s, uint8_t c, uint8_t oc, int scale) {
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx || dy) tk_text(x + dx * scale, y + dy * scale, s, oc, scale);
    tk_text(x + scale, y + 2 * scale, s, oc, scale); /* drop shadow */
    tk_text(x, y, s, c, scale);
}
void tk_caption(int cy, const char *s, uint8_t c, uint8_t oc, int scale) {
    /* multi-line aware centring */
    char line[256];
    int lines = 1;
    for (const char *p = s; *p; p++) if (*p == '\n') lines++;
    int y = cy - (lines * 9 * scale) / 2;
    const char *p = s;
    while (*p) {
        int n = 0;
        while (p[n] && p[n] != '\n' && n < 255) { line[n] = p[n]; n++; }
        line[n] = 0;
        tk_text_outline(TW / 2 - tk_text_w(line, scale) / 2, y, line, c, oc, scale);
        y += 9 * scale;
        p += n;
        if (*p == '\n') p++;
    }
}
void tk_text_wrap(int x, int y, int w, const char *s, uint8_t c, int chars) {
    int cx = x, cy = y, shown = 0;
    const char *p = s;
    while (*p && shown < chars) {
        if (*p == '\n') { cx = x; cy += 9; p++; shown++; continue; }
        if (*p == ' ') {
            /* look ahead: does the next word fit? */
            int wl = 0; const char *q = p + 1;
            while (*q && *q != ' ' && *q != '\n') { wl++; q++; }
            if (cx + 6 + wl * 6 > x + w) { cx = x; cy += 9; p++; shown++; continue; }
        }
        glyph(cx, cy, (unsigned char)*p, c, 1);
        cx += 6; p++; shown++;
    }
}

/* ------------------------------------------------------------------------ */
/* widgets                                                                  */
/* ------------------------------------------------------------------------ */
void tk_window(int x, int y, int w, int h, int style) {
    uint8_t fill = PAL_UI_PINK, border = PAL_UI_PINK_D, hi = PAL_WHITE;
    switch (style) {
    case TKW_MINT:  fill = PAL_UI_MINT;  border = PAL_UI_MINT_D; break;
    case TKW_CREAM: fill = PAL_UI_CREAM; border = PAL_UI_PINK_D; break;
    case TKW_HELL:  fill = PAL_RAMP(RAMP_BLOOD, 1); border = PAL_UI_RED; hi = PAL_RAMP(RAMP_LAVA, 5); break;
    case TKW_LCD:   fill = PAL_UI_LCD;   border = PAL_GRAY_D; hi = PAL_UI_LCD; break;
    case TKW_DARK:  fill = PAL_GRAY_D;   border = PAL_BLACK; hi = PAL_GRAY; break;
    default: break;
    }
    /* drop shadow */
    tk_dither_rect(x + 2, y + 2, w, h, PAL_BLACK, 8);
    /* rounded body */
    tk_rect(x + 2, y, w - 4, h, border);
    tk_rect(x, y + 2, w, h - 4, border);
    tk_rect(x + 1, y + 1, w - 2, h - 2, border);
    tk_rect(x + 2, y + 1, w - 4, h - 2, fill);
    tk_rect(x + 1, y + 2, w - 2, h - 4, fill);
    tk_rect(x + 3, y + 2, w - 6, 1, hi); /* glossy highlight */
}
void tk_dialog(const char *speaker, const char *text, int t, int style) {
    int x = 4, y = TH - 46, w = TW - 8, h = 42;
    tk_window(x, y, w, h, style);
    uint8_t tc = (style == TKW_HELL) ? PAL_WHITE : PAL_GRAY_D;
    if (style == TKW_LCD) tc = PAL_BLACK;
    tk_text_wrap(x + 8, y + 9, w - 16, text, tc, (int)(t * 1.5f));
    if (speaker && *speaker) {
        int nw = tk_text_w(speaker, 1) + 10;
        tk_window(x + 6, y - 9, nw, 13, style == TKW_HELL ? TKW_HELL : TKW_MINT);
        tk_text(x + 11, y - 6, speaker, style == TKW_HELL ? PAL_UI_GOLD : PAL_WHITE, 1);
    }
    /* bouncing "next" heart when fully typed */
    if ((int)(t * 1.5f) >= (int)strlen(text) && (t / 8) % 2 == 0)
        tk_heart(x + w - 12, y + h - 10, 1, style == TKW_HELL ? PAL_UI_GOLD : PAL_UI_PINK_D);
}

/* ------------------------------------------------------------------------ */
/* ASCII sprites                                                            */
/* ------------------------------------------------------------------------ */
const uint8_t TK_MAP_DEFAULT[128] = {
    ['k'] = PAL_BLACK, ['w'] = PAL_WHITE,
    ['p'] = PAL_PAINT(PAINT_BUBBLEGUM, 1), ['P'] = PAL_PAINT(PAINT_ROSE, 0),
    ['r'] = PAL_PAINT(PAINT_CHERRY, 1), ['R'] = PAL_PAINT(PAINT_BLOOD, 1),
    ['o'] = PAL_PAINT(PAINT_TANGERINE, 1), ['y'] = PAL_PAINT(PAINT_LEMON, 1),
    ['g'] = PAL_PAINT(PAINT_LIME, 1), ['G'] = PAL_PAINT(PAINT_FOREST, 1),
    ['b'] = PAL_PAINT(PAINT_SKY, 1), ['B'] = PAL_PAINT(PAINT_NAVY, 1),
    ['v'] = PAL_PAINT(PAINT_VAPE_PURPLE, 1), ['V'] = PAL_PAINT(PAINT_GRAPE, 0),
    ['s'] = PAL_SKIN(SKIN_PEACH, 1), ['S'] = PAL_SKIN(SKIN_PEACH, 0),
    ['n'] = PAL_PAINT(PAINT_CARAMEL, 1), ['N'] = PAL_PAINT(PAINT_CHOCOLATE, 1),
    ['e'] = PAL_GRAY, ['E'] = PAL_GRAY_D, ['c'] = PAL_UI_CREAM, ['m'] = PAL_UI_MINT,
    ['l'] = PAL_UI_LCD, ['L'] = PAL_RAMP(RAMP_GRASS, 1),
    ['f'] = PAL_RAMP(RAMP_FLESH, 5), ['F'] = PAL_RAMP(RAMP_FLESH, 2),
    ['h'] = PAL_RAMP(RAMP_LAVA, 6), ['x'] = PAL_RAMP(RAMP_BONE, 6),
};
void tk_sprite(const char *const *rows, int x, int y, const uint8_t *map, int s, int flip) {
    if (!map) map = TK_MAP_DEFAULT;
    int w = 0;
    for (int j = 0; rows[j]; j++) { int l = (int)strlen(rows[j]); if (l > w) w = l; }
    for (int j = 0; rows[j]; j++) {
        int l = (int)strlen(rows[j]);
        for (int i = 0; i < l; i++) {
            unsigned ch = (unsigned char)rows[j][i];
            if (ch >= 128 || ch == '.' || ch == ' ') continue;
            uint8_t c = map[ch];
            if (!c) continue;
            int px = flip ? (w - 1 - i) : i;
            tk_rect(x + px * s, y + j * s, s, s, c);
        }
    }
}

/* ------------------------------------------------------------------------ */
/* post effects                                                             */
/* ------------------------------------------------------------------------ */
static uint8_t hell_map[256], dark_map[16][256];
static int maps_ready;
static int nearest(int r, int g, int b) {
    int best = 1; long bd = 1L << 40;
    for (int i = 1; i < 256; i++) {
        long rm = (TK_PAL[i][0] + r) / 2;
        long dr = TK_PAL[i][0] - r, dg = TK_PAL[i][1] - g, db = TK_PAL[i][2] - b;
        long d = (((512 + rm) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rm) * db * db) >> 8);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
static void build_maps(void) {
    if (maps_ready) return;
    maps_ready = 1;
    for (int i = 0; i < 256; i++) {
        int r = TK_PAL[i][0], g = TK_PAL[i][1], b = TK_PAL[i][2];
        int l = (r * 3 + g * 6 + b) / 10;
        int hr = l * 5 / 4 + 20, hg = l / 3, hb = l / 4;
        if (l > 200) { hr = 255; hg = 160 + (l - 200); hb = 40; }  /* highlights -> lava */
        if (g > r + 30 && g > b) { hr = l / 2; hg = l * 3 / 4 + 20; hb = l / 5; } /* greens -> bile */
        if (hr > 255) hr = 255;
        hell_map[i] = i == 0 ? 0 : (uint8_t)nearest(hr, hg, hb);
        for (int lv = 0; lv < 16; lv++)
            dark_map[lv][i] = i == 0 ? 0 : (uint8_t)nearest(r * lv / 15, g * lv / 15, b * lv / 15);
    }
}
void tk_post_hellify(int amount) {
    build_maps();
    if (amount <= 0) return;
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++)
            if (BAYER4[y & 3][x & 3] < amount) tk_fb[y][x] = hell_map[tk_fb[y][x]];
}
void tk_post_darken(int level) {
    build_maps();
    if (level >= 15) return;
    if (level < 0) level = 0;
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) tk_fb[y][x] = dark_map[level][tk_fb[y][x]];
}
void tk_post_fade(int amount) {
    if (amount <= 0) return;
    tk_post_darken(15 - (amount * 15) / 16);
}
void tk_post_flash(uint8_t color, int amount) {
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++)
            if (BAYER4[y & 3][x & 3] < amount) tk_fb[y][x] = color;
}
void tk_post_shake(int dx, int dy) {
    static uint8_t tmp[TH][TW];
    memcpy(tmp, tk_fb, sizeof tmp);
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            /* clamp to the edge instead of exposing black bars */
            int sx = x - dx, sy = y - dy;
            sx = sx < 0 ? 0 : sx >= TW ? TW - 1 : sx;
            sy = sy < 0 ? 0 : sy >= TH ? TH - 1 : sy;
            tk_fb[y][x] = tmp[sy][sx];
        }
}
static int iris_in(float dx, float dy, float r, int heart) {
    if (r <= 0) return 0;
    float x = dx / r, y = dy / r;
    if (!heart) return x * x + y * y <= 1.0f;
    x *= 1.05f; y = -y * 1.05f + 0.15f;            /* heart: y up, nudged to centre it */
    float a = x * x + y * y - 1.0f;
    return a * a * a - x * x * y * y * y <= 0.0f;
}
void tk_post_iris(float cx, float cy, float r, int heart, uint8_t color, uint8_t rim) {
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (iris_in(dx, dy, r, heart)) continue;
            tk_fb[y][x] = (rim && r > 0 && iris_in(dx, dy, r + 2.0f, heart)) ? rim : color;
        }
}
void tk_post_glitch(int seed, int strength) {
    uint32_t s = tk_hash((uint32_t)seed * 7919u + 13u);
    for (int y = 0; y < TH; y++) {
        if (tk_rand(&s, 100) < strength) {
            int off = tk_rand(&s, 17) - 8;
            uint8_t row[TW];
            for (int x = 0; x < TW; x++) row[x] = tk_fb[y][(x - off + TW) % TW];
            memcpy(tk_fb[y], row, TW);
            if (tk_rand(&s, 4) == 0)
                for (int x = 0; x < TW; x += 1 + tk_rand(&s, 6))
                    tk_fb[y][x] = (uint8_t)(tk_rand(&s, 2) ? PAL_UI_PURPLE : PAL_UI_RED);
        }
    }
}
void tk_post_scanlines(void) {
    build_maps();
    for (int y = 1; y < TH; y += 2)
        for (int x = 0; x < TW; x++) tk_fb[y][x] = dark_map[13][tk_fb[y][x]];
}

/* ------------------------------------------------------------------------ */
/* particles                                                                */
/* ------------------------------------------------------------------------ */
static void burst(int x, int y, int age, uint32_t seed, int n, const uint8_t *cols, int ncols,
                  float speed, float grav, int sz, int life) {
    if (age < 0 || age > life) return;
    uint32_t s = tk_hash(seed);
    for (int i = 0; i < n; i++) {
        float a = tk_rand(&s, 6283) / 1000.0f;
        float v = speed * (0.4f + tk_rand(&s, 600) / 1000.0f);
        float px = x + cosf(a) * v * age;
        float py = y + sinf(a) * v * age - 0.6f * speed * age + 0.5f * grav * age * age;
        uint8_t c = cols[tk_rand(&s, ncols)];
        int w = sz + (tk_rand(&s, 3) == 0);
        if ((age + i) % 4 < 2 || sz > 1) tk_rect((int)px, (int)py, w, w > 1 ? w - (i & 1) : 1, c);
        else tk_px((int)px, (int)py, c);
    }
}
void tk_confetti(int x, int y, int age, uint32_t seed) {
    static const uint8_t c[] = { PAL_PAINT(PAINT_BUBBLEGUM,1), PAL_PAINT(PAINT_LEMON,1), PAL_PAINT(PAINT_MINT,1),
                                 PAL_PAINT(PAINT_SKY,1), PAL_PAINT(PAINT_LAVENDER,1), PAL_WHITE, PAL_UI_GOLD };
    burst(x, y, age, seed, 40, c, 7, 2.2f, 0.12f, 2, 60);
}
void tk_gibs(int x, int y, int age, uint32_t seed) {
    static const uint8_t c[] = { PAL_RAMP(RAMP_BLOOD,3), PAL_RAMP(RAMP_BLOOD,5), PAL_RAMP(RAMP_FLESH,4),
                                 PAL_RAMP(RAMP_FLESH,2), PAL_RAMP(RAMP_BONE,6), PAL_RAMP(RAMP_BLOOD,1) };
    burst(x, y, age, seed, 34, c, 6, 2.6f, 0.2f, 2, 60);
    burst(x, y, age, seed + 1, 20, c, 2, 1.5f, 0.25f, 1, 60);
}
void tk_sparkles(int x, int y, int age, uint32_t seed, int radius) {
    uint32_t s = tk_hash(seed);
    for (int i = 0; i < 10; i++) {
        int px = x + tk_rand(&s, 2 * radius + 1) - radius, py = y + tk_rand(&s, 2 * radius + 1) - radius;
        int ph = (age + tk_rand(&s, 30)) % 30;
        if (ph < 12) tk_star(px, py, ph < 6 ? ph / 2 : (12 - ph) / 2, ph % 4 < 2 ? PAL_WHITE : PAL_UI_GOLD);
    }
}
void tk_hearts(int x, int y, int age, uint32_t seed) {
    uint32_t s = tk_hash(seed);
    for (int i = 0; i < 8; i++) {
        int ph = (age + tk_rand(&s, 40)) % 40;
        int px = x + tk_rand(&s, 40) - 20 + (int)(3 * sinf(ph * 0.3f + i));
        int py = y - ph;
        tk_heart(px, py, 1, ph < 30 ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_PAINT(PAINT_BUBBLEGUM, 2));
    }
}
void tk_vape_cloud(int x, int y, int age, uint32_t seed, int size) {
    uint32_t s = tk_hash(seed);
    static const uint8_t c[] = { PAL_RAMP(RAMP_VAPE,7), PAL_RAMP(RAMP_VAPE,6), PAL_RAMP(RAMP_VAPE,5), PAL_RAMP(RAMP_VAPE,4) };
    for (int i = 0; i < 12; i++) {
        float a = tk_rand(&s, 6283) / 1000.0f;
        float d = tk_rand(&s, size * 10) / 10.0f;
        int r = size / 3 + tk_rand(&s, size / 2 + 1);
        int px = x + (int)(cosf(a + age * 0.02f) * d) + (int)(age * 0.3f * cosf(a));
        int py = y + (int)(sinf(a + age * 0.02f) * d * 0.6f) - age / 4;
        tk_circle(px, py, r, c[i & 3]);
    }
}
void tk_star(int x, int y, int r, uint8_t c) {
    tk_px(x, y, c);
    for (int i = 1; i <= r; i++) { tk_px(x + i, y, c); tk_px(x - i, y, c); tk_px(x, y + i, c); tk_px(x, y - i, c); }
}
void tk_heart(int x, int y, int size, uint8_t c) {
    static const char *H[] = { ".kk.kk.", "kppkppk", "kpppppk", ".kpppk.", "..kpk..", "...k...", NULL };
    uint8_t m[128] = {0};
    m['k'] = PAL_BLACK; m['p'] = c;
    tk_sprite(H, x - 3 * size, y - 3 * size, m, size, 0);
}

/* ------------------------------------------------------------------------ */
/* math                                                                     */
/* ------------------------------------------------------------------------ */
uint32_t tk_hash(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x ? x : 0x9E3779B9u;
}
int tk_rand(uint32_t *s, int n) {
    uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x;
    return n > 0 ? (int)(x % (uint32_t)n) : 0;
}
float tk_clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
float tk_ease(float t) { t = tk_clampf(t, 0, 1); return t * t * (3 - 2 * t); }
float tk_bounce(float t) {
    t = tk_clampf(t, 0, 1);
    return 1.0f - cosf(t * 6.0f) * expf(-4.0f * t) * (1.0f - t * 0.2f);
}
int tk_lerpi(int a, int b, float t) { return a + (int)((b - a) * t); }
float tk_tri_wave(float t) { t = t - floorf(t); return t < .5f ? t * 2 : 2 - t * 2; }

/* ------------------------------------------------------------------------ */
/* audio                                                                    */
/* ------------------------------------------------------------------------ */
#define SR 22050
typedef struct { double t, dur; float f0, f1, vol; int wave; } Note;
static Note *notes; static int nnotes, capnotes;
static void add(double t, double dur, float f0, float f1, int wave, float vol) {
    if (t < 0 || dur <= 0) return;
    if (nnotes == capnotes) { capnotes = capnotes ? capnotes * 2 : 4096; notes = realloc(notes, capnotes * sizeof *notes); }
    notes[nnotes++] = (Note){ t, dur, f0, f1, vol, wave };
}
static float mtof(float m) { return 440.0f * powf(2.0f, (m - 69.0f) / 12.0f); }
void tk_note(double t, double dur, int midi, int wave, float vol) { add(t, dur, mtof(midi), mtof(midi), wave, vol); }
void tk_slide(double t, double dur, int a, int b, int wave, float vol) { add(t, dur, mtof(a), mtof(b), wave, vol); }
#define KICK  100
#define SNARE 101
#define HAT   102
void tk_kick(double t, float v)  { add(t, 0.18, 150, 40, KICK, v); }
void tk_snare(double t, float v) { add(t, 0.16, 190, 160, SNARE, v); }
void tk_hat(double t, float v)   { add(t, 0.05, 8000, 8000, HAT, v); }

void tk_sfx(double t, int sfx, float v) {
    switch (sfx) {
    case SFX_BLIP:    tk_note(t, 0.05, 84, TKV_SQ25, v * .5f); break;
    case SFX_SELECT:  tk_note(t, 0.05, 79, TKV_SQ25, v * .5f); tk_note(t + .05, 0.08, 91, TKV_SQ25, v * .5f); break;
    case SFX_BOING:   tk_slide(t, 0.25, 55, 79, TKV_SQ50, v * .5f); tk_slide(t + .25, .15, 79, 67, TKV_SQ50, v * .3f); break;
    case SFX_POP:     tk_slide(t, 0.06, 70, 96, TKV_SINE, v * .7f); break;
    case SFX_SQUEAK:  tk_slide(t, 0.08, 88, 100, TKV_SQ12, v * .4f); tk_slide(t + .08, .12, 100, 90, TKV_SQ12, v * .4f); break;
    case SFX_SPLAT:   add(t, 0.25, 200, 60, TKV_NOISE, v * .8f); tk_slide(t, .2, 40, 28, TKV_SAW, v * .5f); break;
    case SFX_SHOTGUN: add(t, 0.35, 900, 120, TKV_NOISE, v); tk_slide(t, .25, 45, 25, TKV_SQ50, v * .8f); add(t + .45, .06, 2000, 2000, TKV_NOISE, v * .4f); add(t + .55, .06, 1500, 1500, TKV_NOISE, v * .4f); break;
    case SFX_EXPLOSION: add(t, 0.9, 600, 40, TKV_NOISE, v); tk_slide(t, .8, 40, 20, TKV_SAW, v * .7f); break;
    case SFX_SCRATCH: add(t, 0.12, 3000, 400, TKV_NOISE, v * .7f); add(t + .12, .18, 400, 3000, TKV_NOISE, v * .7f); break;
    case SFX_HEARTBEAT: tk_kick(t, v); tk_kick(t + .18, v * .7f); break;
    case SFX_BEEP:    tk_note(t, 0.07, 88, TKV_SQ50, v * .35f); tk_note(t + .1, .07, 88, TKV_SQ50, v * .35f); break;
    case SFX_CHOIR:   tk_note(t, 1.2, 64, TKV_TRI, v * .3f); tk_note(t, 1.2, 68, TKV_TRI, v * .3f); tk_note(t, 1.2, 71, TKV_TRI, v * .3f); tk_note(t, 1.2, 76, TKV_SINE, v * .25f); break;
    case SFX_FANFARE: { int m[] = { 72, 76, 79, 84 }; for (int i = 0; i < 4; i++) tk_note(t + i * .1, i == 3 ? .5 : .1, m[i], TKV_SQ25, v * .5f); break; }
    case SFX_WHOOSH:  add(t, 0.4, 300, 3000, TKV_NOISE, v * .4f); break;
    case SFX_ROAR:    tk_slide(t, 0.9, 40, 30, TKV_SAW, v * .7f); tk_slide(t, .9, 47, 35, TKV_SQ12, v * .5f); add(t, .9, 400, 150, TKV_NOISE, v * .5f); break;
    case SFX_SPARKLE: for (int i = 0; i < 5; i++) tk_note(t + i * .04, .05, 96 + i * 2, TKV_SINE, v * .3f); break;
    case SFX_SPLASH:  add(t, 0.4, 3000, 800, TKV_NOISE, v * .5f); break;
    case SFX_THUD:    tk_kick(t, v); add(t, .1, 200, 100, TKV_NOISE, v * .3f); break;
    case SFX_GLITCH:  for (int i = 0; i < 6; i++) tk_note(t + i * .03, .03, 40 + (i * 37) % 50, TKV_SQ12, v * .4f); break;
    case SFX_KAZOO:   tk_slide(t, .25, 67, 72, TKV_SAW, v * .35f); tk_slide(t + .25, .5, 72, 60, TKV_SAW, v * .35f); break;
    default: break;
    }
}

static float osc(int wave, double ph, uint32_t *lfsr) {
    double p = ph - floor(ph);
    switch (wave) {
    case TKV_SQ12: return p < .125 ? 1.f : -1.f;
    case TKV_SQ25: return p < .25 ? 1.f : -1.f;
    case TKV_SQ50: return p < .5 ? 1.f : -1.f;
    case TKV_TRI:  return (float)(p < .5 ? 4 * p - 1 : 3 - 4 * p);
    case TKV_SAW:  return (float)(2 * p - 1);
    case TKV_SINE: return (float)sin(p * 6.283185307);
    default: { uint32_t x = *lfsr; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *lfsr = x; return (x & 0xFFFF) / 32768.0f - 1.0f; }
    }
}
int tk_audio_render(double total, const char *path) {
    long n = (long)(total * SR);
    float *mix = calloc((size_t)n, sizeof(float));
    if (!mix) return 0;
    for (int k = 0; k < nnotes; k++) {
        Note *no = &notes[k];
        long s0 = (long)(no->t * SR), len = (long)(no->dur * SR);
        double ph = 0; uint32_t lfsr = 0xACE1u + (uint32_t)k * 31u;
        float hold = 0; int holdc = 0;
        for (long i = 0; i < len && s0 + i < n; i++) {
            double u = (double)i / (double)len;
            float f = no->f0 + (no->f1 - no->f0) * (float)u;
            float env;
            float a = (float)i / (0.004f * SR), r = (float)(len - i) / (0.03f * SR);
            env = a < 1 ? a : 1; if (r < env) env = r;
            float s;
            if (no->wave == KICK) { f = 150.0f * powf(40.0f / 150.0f, (float)u); s = sinf((float)(ph * 6.2831853)); env *= expf(-6.0f * (float)u); }
            else if (no->wave == SNARE) { s = 0.6f * osc(TKV_NOISE, ph, &lfsr) + 0.4f * osc(TKV_TRI, ph, &lfsr); env *= expf(-9.0f * (float)u); }
            else if (no->wave == HAT) { s = osc(TKV_NOISE, ph, &lfsr); env *= expf(-14.0f * (float)u); }
            else if (no->wave == TKV_NOISE) {
                /* pitched noise: sample-and-hold at f */
                if (holdc <= 0) { hold = osc(TKV_NOISE, ph, &lfsr); holdc = (int)(SR / (f > 50 ? f : 50)); }
                holdc--; s = hold; env *= (float)(1.0 - u * 0.7);
            } else s = osc(no->wave, ph, &lfsr);
            mix[s0 + i] += s * env * no->vol * 0.22f;
            ph += f / SR;
        }
    }
    FILE *fp = fopen(path, "wb");
    if (!fp) { free(mix); return 0; }
    uint32_t datalen = (uint32_t)n * 2;
    uint8_t h[44] = { 'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,
                      0,0,0,0,0,0,0,0,2,0,16,0,'d','a','t','a',0,0,0,0 };
    uint32_t v = 36 + datalen; memcpy(h + 4, &v, 4);
    v = SR; memcpy(h + 24, &v, 4); v = SR * 2; memcpy(h + 28, &v, 4);
    memcpy(h + 40, &datalen, 4);
    fwrite(h, 1, 44, fp);
    for (long i = 0; i < n; i++) {
        float s = mix[i];
        s = s / (1.0f + fabsf(s) * 0.6f) * 1.4f; /* soft clip */
        if (s > 1) { s = 1; } if (s < -1) { s = -1; }
        int16_t q = (int16_t)(s * 32000);
        fwrite(&q, 2, 1, fp);
    }
    fclose(fp);
    free(mix);
    return 1;
}

/* ------------------------------------------------------------------------ */
/* output                                                                   */
/* ------------------------------------------------------------------------ */
static uint32_t crc_table[256];
static uint32_t crc32b(uint32_t c, const uint8_t *p, size_t n) {
    if (!crc_table[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t k = i; for (int j = 0; j < 8; j++) k = k & 1 ? 0xEDB88320u ^ (k >> 1) : k >> 1; crc_table[i] = k; }
    c = ~c; while (n--) c = crc_table[(c ^ *p++) & 255] ^ (c >> 8); return ~c;
}
static void be32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len) {
    uint8_t b[4]; be32(b, len); fwrite(b, 1, 4, f); fwrite(type, 1, 4, f);
    if (len) fwrite(data, 1, len, f);
    uint32_t c = crc32b(0, (const uint8_t *)type, 4); c = crc32b(c, data, len);
    be32(b, c); fwrite(b, 1, 4, f);
}
int tk_write_png_rgb(const char *path, const uint8_t *rgb, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    static const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13]; be32(ihdr, w); be32(ihdr + 4, h); ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    size_t raw = (size_t)h * (1 + 3 * w);
    size_t nblocks = (raw + 65534) / 65535;
    size_t zlen = 2 + raw + nblocks * 5 + 4;
    uint8_t *z = malloc(zlen), *rows = malloc(raw);
    for (int y = 0; y < h; y++) { rows[y * (1 + 3 * w)] = 0; memcpy(rows + y * (1 + 3 * w) + 1, rgb + (size_t)y * 3 * w, 3 * w); }
    size_t o = 0; z[o++] = 0x78; z[o++] = 0x01;
    uint32_t a = 1, b2 = 0;
    for (size_t i = 0; i < raw; i++) { a = (a + rows[i]) % 65521; b2 = (b2 + a) % 65521; }
    for (size_t pos = 0; pos < raw; pos += 65535) {
        size_t n = raw - pos > 65535 ? 65535 : raw - pos;
        z[o++] = pos + n >= raw; z[o++] = n & 255; z[o++] = n >> 8; z[o++] = ~n & 255; z[o++] = (~n >> 8) & 255;
        memcpy(z + o, rows + pos, n); o += n;
    }
    be32(z + o, (b2 << 16) | a); o += 4;
    chunk(f, "IDAT", z, (uint32_t)o);
    chunk(f, "IEND", NULL, 0);
    fclose(f); free(z); free(rows);
    return 1;
}
void tk_fb_to_rgb(uint8_t *out) {
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            const unsigned char *c = TK_PAL[tk_fb[y][x]];
            out[(y * TW + x) * 3 + 0] = c[0];
            out[(y * TW + x) * 3 + 1] = c[1];
            out[(y * TW + x) * 3 + 2] = c[2];
        }
}
