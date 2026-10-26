#include "fp.h"
#include "wd.h"
#include "tk.h"
#include <math.h>

static float cx_, cy_, dx_, dy_, px_, py_, horiz_, projY_;
static float zb[FP_VH][TW];
static const uint8_t BY[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

void fp_camera(float x, float y, float yaw, float bob) {
    cx_ = x; cy_ = y; dx_ = cosf(yaw); dy_ = sinf(yaw);
    const float plane = 0.68f;
    px_ = -dy_ * plane; py_ = dx_ * plane;
    projY_ = (TW / 2) / plane;
    horiz_ = FP_VH * 0.5f + bob;
}
static uint8_t fogged(uint8_t c, float t, const FpMap *m, int x, int y) {
    int lvl = (int)(t * m->fog_k * 16);
    if (lvl > 15) return m->fog;
    return BY[y & 3][x & 3] < lvl ? m->fog : c;
}
void fp_render(const FpMap *m) {
    for (int x = 0; x < TW; x++) {
        float camX = 2.0f * (x + 0.5f) / TW - 1.0f;
        float rx = dx_ + px_ * camX, ry = dy_ + py_ * camX;
        int mx = (int)floorf(cx_), my = (int)floorf(cy_);
        float ddx = rx == 0 ? 1e30f : fabsf(1 / rx), ddy = ry == 0 ? 1e30f : fabsf(1 / ry);
        int sx, sy; float sdx, sdy;
        if (rx < 0) { sx = -1; sdx = (cx_ - mx) * ddx; } else { sx = 1; sdx = (mx + 1.0f - cx_) * ddx; }
        if (ry < 0) { sy = -1; sdy = (cy_ - my) * ddy; } else { sy = 1; sdy = (my + 1.0f - cy_) * ddy; }
        int side = 0; char c = '.';
        for (int it = 0; it < 96; it++) {
            if (sdx < sdy) { sdx += ddx; mx += sx; side = 0; } else { sdy += ddy; my += sy; side = 1; }
            c = m->cell(mx, my);
            if (c != '.') break;
        }
        float t = side == 0 ? sdx - ddx : sdy - ddy;
        if (t < 0.05f) t = 0.05f;
        float wpos = side == 0 ? cy_ + t * ry : cx_ + t * rx;
        float u = wpos - floorf(wpos);
        if ((side == 0 && sx < 0) || (side == 1 && sy > 0)) u = 1 - u;
        float h = projY_ / t;
        int top = (int)(horiz_ - h * 0.5f), bot = (int)(horiz_ + h * 0.5f);
        for (int y = 0; y < FP_VH; y++) {
            uint8_t col; float d;
            if (y < top) {          /* ceiling */
                d = 0.5f * projY_ / (horiz_ - y - 0.5f);
                col = m->ceil(cx_ + rx * d, cy_ + ry * d);
            } else if (y >= bot) {  /* floor */
                d = 0.5f * projY_ / (y + 0.5f - horiz_);
                col = m->floor(cx_ + rx * d, cy_ + ry * d);
            } else {
                d = t;
                float v = 1 - (y - top + 0.5f) / (float)(bot - top);
                col = m->wall(c, mx, my, u, v);
                if (side == 1 && BY[y & 3][x & 3] < 4) col = m->fog;   /* side shading */
            }
            tk_fb[y][x] = fogged(col, d, m, x, y);
            zb[y][x] = d;
        }
    }
}
int fp_project(float wx, float wy, float z, float *sx, float *sy, float *scale) {
    float rx = wx - cx_, ry = wy - cy_;
    float inv = 1.0f / (px_ * dy_ - dx_ * py_);
    float tx = inv * (dy_ * rx - dx_ * ry), ty = inv * (-py_ * rx + px_ * ry);
    if (ty < 0.2f) return 0;
    *sx = (TW / 2.0f) * (1 + tx / ty);
    *sy = horiz_ + (0.5f - z) * projY_ / ty;
    *scale = projY_ / ty;
    int ix = (int)*sx, iy = (int)*sy;
    if (ix >= 0 && ix < TW && iy >= 0 && iy < FP_VH && zb[iy][ix] < ty - 0.3f) return 0;
    return 1;
}
int fp_sprite(float wx, float wy, float z, float unit) {
    float sx, sy, sc;
    if (!fp_project(wx, wy, z, &sx, &sy, &sc)) return 0;
    float zz = sc * unit;
    wd_cam(-(sx - TW * 0.5f) / zz, -(sy - TH * 0.5f) / zz, zz);
    return 1;
}

/* ---- the gun: twin barrels, wooden forend, Sunny's paws in blue sleeves --- */
void fp_shotgun(int fire_age, int pump_age, float raise) {
    float rec = fire_age >= 0 && fire_age < 9 ? (9 - fire_age) * 2.6f : 0;
    float pump = pump_age >= 0 && pump_age < 12 ? sinf(pump_age / 12.0f * 3.14159f) * 9 : 0;
    float oy = rec + (1 - raise) * 90;
    wd_cam(TW * 0.5f, TH * 0.5f, 1);
    uint8_t ol = PAL_PAINT(PAINT_JET, 1);
    /* sleeves and paws */
    wd_tube(60, 175 + oy, 92, 146 + oy + pump, 12, 10, ol, PAL_PAINT(PAINT_SKY, 0), PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_SKY, 2));
    wd_tube(206, 178 + oy, 166, 158 + oy, 12, 10, ol, PAL_PAINT(PAINT_SKY, 0), PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_SKY, 2));
    for (int s = -1; s <= 1; s += 2) {
        float tx = 120 + s * 8, ty = 92 + oy, bx = 120 + s * 18, by = 175 + oy;
        wd_tube(bx, by, tx, ty, 13, 7, ol, PAL_RAMP(RAMP_STEEL, 1), PAL_RAMP(RAMP_STEEL, 3), PAL_RAMP(RAMP_STEEL, 6));
        wd_blob(tx, ty, 4.5f, 2.2f, 0, PAL_BLACK, 0);
    }
    wd_tube(120, 145 + oy + pump, 120, 172 + oy + pump, 26, 30, ol, PAL_RAMP(RAMP_WOOD, 2), PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 6));   /* forend */
    wd_blob_ol(96, 146 + oy + pump, 9, 8, 0, PAL_WHITE, ol, WB_SHADE);
    wd_blob_ol(162, 160 + oy, 10, 9, 0, PAL_WHITE, ol, WB_SHADE);
    tk_circle(120, (int)(90 + oy), 1, PAL_UI_RED);
    if (fire_age >= 0 && fire_age < 4) {
        float r = fire_age < 2 ? 22 : 12;
        for (int s = -1; s <= 1; s += 2) { wd_blob(120 + s * 8, 84 + oy, r * 0.6f, r * 0.5f, 0, PAL_RAMP(RAMP_LAVA, 6), 0); tk_star(120 + s * 8, (int)(84 + oy), (int)r, PAL_RAMP(RAMP_CANDY, 7)); }
        wd_blob(120, 82 + oy, r * 0.3f, r * 0.3f, 0, PAL_WHITE, 0);
    }
}

/* ---- the HUD: hearts, shells, the pet's LCD face, plates, mode label ----- */
void fp_hud(const FpHud *h, int f) {
    int y0 = FP_VH;
    tk_rect(0, y0, TW, TH - y0, PAL_PAINT(PAINT_CHARCOAL, 0));
    tk_rect(0, y0, TW, 1, PAL_PAINT(PAINT_SILVER, 1));
    for (int i = 0; i < 3; i++) tk_heart(6 + i * 14, y0 + 6, 1, i < h->hearts ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_PAINT(PAINT_CHARCOAL, 2));
    for (int i = 0; i < 8; i++) tk_rect(190 + (i % 4) * 12, y0 + 4 + (i / 4) * 12, 9, 9, i < h->shells ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_PAINT(PAINT_CHARCOAL, 2));
    /* the pet's face in LCD green */
    tk_rect(114, y0 + 3, 32, 26, PAL_PAINT(PAINT_TEAL, 0));
    tk_ellipse(130, y0 + 18, 10, 8, PAL_PAINT(PAINT_MINT, 2));
    int lx = h->pet_look;
    tk_rect(125 + lx * 2, y0 + 15, 2, 3, PAL_PAINT(PAINT_TEAL, 0)); tk_rect(134 + lx * 2, y0 + 15, 2, 3, PAL_PAINT(PAINT_TEAL, 0));
    if (h->pet_mood > 0) tk_line(126, y0 + 21, 134, y0 + 21, PAL_PAINT(PAINT_TEAL, 0)), tk_px(125, y0 + 20, PAL_PAINT(PAINT_TEAL, 0)), tk_px(135, y0 + 20, PAL_PAINT(PAINT_TEAL, 0));
    if (h->pet_mood < 0) { tk_line(127, y0 + 22, 133, y0 + 20, PAL_PAINT(PAINT_TEAL, 0)); }
    tk_text(48, y0 + 4, "PARTY PUMP", PAL_PAINT(PAINT_LEMON, 2), 1);
    tk_text(48, y0 + 14, "RIP+TEAR", PAL_PAINT(PAINT_SILVER, 2), 1);
    tk_text(48, y0 + 22, "..GENTLY", PAL_PAINT(PAINT_SILVER, 1), 1);
    tk_text(150, y0 + 4, "GORE:", PAL_PAINT(PAINT_SILVER, 2), 1);
    if (h->brutal) tk_text(150, y0 + 14, "BRUTAL", PAL_UI_RED, 1);
    /* the Board of Decency sticker, slapped over it; then torn off in the blast */
    if (h->sticker_age >= 0 && h->sticker_torn_age < 0) {
        int r = h->sticker_age < 4 ? 20 - h->sticker_age * 2 : 13;
        tk_circle(168, y0 + 16, r, PAL_WHITE); tk_ring(168, y0 + 16, r, PAL_PAINT(PAINT_ROSE, 1));
        tk_text(159, y0 + 9, "OK!", PAL_PAINT(PAINT_ROSE, 0), 1);
        tk_text(157, y0 + 17, "(ok)", PAL_PAINT(PAINT_NAVY, 1), 1);
    }
    if (h->sticker_torn_age >= 0 && h->sticker_torn_age < 40) {
        int a = h->sticker_torn_age;
        int sx = 168 + (int)(sinf(a * 0.4f) * 18), sy = y0 + 16 - a * 3;
        tk_ellipse(sx, sy, 11 - (a / 6) % 3 * 3, 8, PAL_WHITE);
        tk_px(sx + 3, sy, PAL_PAINT(PAINT_CHERRY, 1)); tk_px(sx - 2, sy + 2, PAL_PAINT(PAINT_CHERRY, 1));
    }
    (void)f;
}
void fp_lens_blood(int age, uint32_t seed) {
    if (age < 0 || age > 30) return;
    int slide = (age / 2) * 3;     /* slides off on twos */
    for (int k = 0; k < 9; k++) {
        uint32_t h = tk_hash(seed * 31u + k);
        int x = (int)(h % TW), y = (int)((h >> 8) % 80) + slide;
        int r = 6 + (int)((h >> 4) % 10);
        if (y - r > FP_VH) continue;
        tk_ellipse(x, y, r, r - 2, PAL_PAINT(PAINT_CHERRY, 0));
        tk_rect(x - 1, y, 3, r + 8, PAL_PAINT(PAINT_CHERRY, 0));
        tk_px(x - r / 3, y - r / 3, PAL_PAINT(PAINT_CHERRY, 2));
    }
}
