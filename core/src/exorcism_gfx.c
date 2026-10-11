/* exorcism_gfx.c - St. Cuddles church and the ritual's props (owner:
 * life). Everything is drawn with plat_* / ui_* primitives from a few
 * colour keys that slide from sugary pastel (dread 0) to a blood-red
 * nightmare (dread 15): the same church, rotting as the ritual goes on.
 */
#include "exorcism_int.h"

/* ---------------------------------------------------------------------- */
/* colours                                                                  */
/* three keys per colour: dread 0 (sugar pastel), 8 (vape purple), 15
 * (blood-red nightmare) */
static const uint8_t key_rgb[EC_COUNT][9] = {
    { 255, 214, 228,   196, 150, 226,    62,  16,  40 },   /* WALL_HI    */
    { 246, 178, 206,   150, 100, 196,    40,   8,  26 },   /* WALL_LO    */
    { 255, 236, 244,   214, 176, 240,    84,  22,  48 },   /* STRIPE     */
    { 236, 232, 246,   196, 180, 226,   112,  84, 104 },   /* STONE_HI   */
    { 198, 190, 220,   150, 128, 186,    70,  48,  70 },   /* STONE      */
    { 150, 140, 180,    96,  70, 130,    36,  20,  36 },   /* STONE_LO   */
    { 255, 230, 200,   186, 150, 210,    60,  24,  34 },   /* FLOOR_A    */
    { 250, 200, 170,   150, 112, 180,    34,  12,  22 },   /* FLOOR_B    */
    { 226,  92, 146,   176,  48, 150,   120,   8,  24 },   /* CARPET     */
    { 255, 160, 196,   224, 120, 200,   176,  30,  40 },   /* CARPET_HI  */
    { 255, 140, 196,   240,  60, 170,   170,  20,  30 },   /* GLASS_PINK */
    { 120, 190, 250,   110,  90, 230,    90,  20, 100 },   /* GLASS_SKY  */
    { 255, 226,  80,   255, 150,  60,   230,  70,  20 },   /* GLASS_LEMON*/
    { 120, 226, 186,    60, 170, 150,    40,  90,  40 },   /* GLASS_MINT */
    { 196, 170, 250,   160,  90, 230,    90,  30, 120 },   /* GLASS_LAV  */
    { 255, 252, 232,   236, 210, 255,   200,  40,  50 },   /* BEAM       */
    { 236, 182, 124,   180, 120, 130,    96,  48,  40 },   /* WOOD_HI    */
    { 196, 132,  82,   136,  80, 110,    66,  30,  28 },   /* WOOD       */
    { 140,  84,  56,    90,  44,  80,    36,  14,  16 },   /* WOOD_LO    */
    { 120,  72, 120,    70,  30, 100,    20,   4,  12 },   /* LEAD       */
};

static uint8_t col_cache[16][EC_COUNT];
static uint16_t col_ready;

uint8_t exo_col(int which, int dread)
{
    int d = DOOMO_CLAMP(dread, 0, 15), i;
    if (!(col_ready & (1u << d))) {
        for (i = 0; i < EC_COUNT; i++) {
            const uint8_t *k = key_rgb[i];
            const uint8_t *a = d <= 8 ? k : k + 3, *b = d <= 8 ? k + 3 : k + 6;
            int u = d <= 8 ? d : d - 8, n = d <= 8 ? 8 : 7;
            col_cache[d][i] = palette_nearest(a[0] + (b[0] - a[0]) * u / n,
                                              a[1] + (b[1] - a[1]) * u / n,
                                              a[2] + (b[2] - a[2]) * u / n);
        }
        col_ready |= (uint16_t)(1u << d);
    }
    return col_cache[d][DOOMO_CLAMP(which, 0, EC_COUNT - 1)];
}

/* ---------------------------------------------------------------------- */
/* the church                                                               */
/* the drape's width at row y: tied back in the middle, flaring below */
static int drape_w(int w, int h, int y)
{
    int k = y * 16 / (h > 0 ? h : 1);
    return w - (k > 4 && k < 11 ? (5 - (k - 8 < 0 ? 8 - k : k - 8)) * w / 12 : 0);
}

static void curtain(int x0, int w, int h, int left, int dread, uint32_t t)
{
    /* velvet drapes: vertical folds, drawn as a few tall strips per fold
     * (cheap on the GBA) with a slow sway on the lower half */
    uint8_t c0 = dread < 8 ? PAL_PAINT(PAINT_ROSE, 0) : PAL_PAINT(PAINT_BLOOD, 0);
    uint8_t c1 = dread < 8 ? PAL_PAINT(PAINT_ROSE, 1) : PAL_PAINT(PAINT_BLOOD, 1);
    uint8_t c2 = dread < 8 ? PAL_PAINT(PAINT_ROSE, 2) : PAL_PAINT(PAINT_CHERRY, 0);
    int fx, y0;
    int sway = ui_wave((int)t, 90, 1);
    for (fx = 0; fx < w; fx += 4) {
        /* rows where this fold is covered: everything except the tied band */
        int y, start = -1;
        for (y = 0; y <= h; y++) {
            int vis = y < h && drape_w(w, h, y) > fx;
            if (vis && start < 0) start = y;
            if ((!vis || y == h) && start >= 0) {
                int len = y - start, half = start < h / 2 ? DOOMO_MIN(len, h / 2 - start) : 0;
                int xs = left ? x0 + fx : x0 + w - fx - 4, f = ((fx >> 2) & 1);
                if (half > 0) plat_rect(xs, start, 4, half, f ? c1 : c0);
                if (len - half > 0) plat_rect(xs + sway, start + half, 4, len - half, f ? c1 : c0);
                start = -1;
            }
        }
    }
    /* a soft highlight down the inner edge */
    for (y0 = 0; y0 < h; y0 += 2) {
        int ww = drape_w(w, h, y0);
        plat_rect(left ? x0 + ww - 1 + (y0 > h / 2 ? sway : 0) : x0 + w - ww + (y0 > h / 2 ? sway : 0),
                  y0, 1, 2, c2);
    }
    /* golden tie-back rope */
    {
        int ty = h * 8 / 16, tx = x0 + w / 2;
        ui_fill_ellipse(tx, ty, 3, 2, PAL_UI_GOLD);
        plat_rect(tx - 1, ty - 1, 2, 1, PAL_RAMP(RAMP_CANDY, 7));
    }
}

static void rose_window(int cx, int cy, int r, int dread, uint32_t t)
{
    static const uint8_t glass[5] = { EC_GLASS_PINK, EC_GLASS_SKY, EC_GLASS_LEMON,
                                      EC_GLASS_MINT, EC_GLASS_LAV };
    int i, pr = r * 38 / 100 + 1, pd = r * 55 / 100;
    uint8_t lead = exo_col(EC_LEAD, dread);
    /* stone surround */
    ui_fill_circle(cx, cy, r + 4, exo_col(EC_STONE_LO, dread));
    ui_fill_circle(cx, cy, r + 3, exo_col(EC_STONE, dread));
    ui_fill_circle(cx, cy - 1, r + 2, exo_col(EC_STONE_HI, dread));
    ui_fill_circle(cx, cy, r + 1, lead);
    /* eight petals of stained glass */
    for (i = 0; i < 8; i++) {
        int a = i * (DOOMO_ANG_FULL / 8) + (DOOMO_ANG_FULL / 16);
        int px = cx + ((fx_cos(a) * pd) >> 16), py = cy + ((fx_sin(a) * pd) >> 16);
        uint8_t c = exo_col(glass[i % 5], dread);
        ui_fill_circle(px, py, pr, c);
        plat_rect(px - pr / 2, py - pr / 2, 2, 1, ui_lighter(c));          /* glint */
        plat_rect(px - pr / 2, py - pr / 2 + 1, 1, 1, ui_lighter(c));
    }
    /* centre: a heart... that becomes an eye when things go wrong */
    ui_fill_circle(cx, cy, r * 34 / 100 + 1, lead);
    ui_fill_circle(cx, cy, r * 30 / 100, exo_col(EC_GLASS_LEMON, dread));
    if (dread < 11) {
        int pulse = ui_wave((int)t, 60, 1);
        ui_heart(cx, cy, DOOMO_MAX(2, r * 16 / 100 + pulse), PAL_PAINT(PAINT_BUBBLEGUM, 1));
    } else {
        int look = ui_wave((int)t, 70, 2);
        ui_fill_ellipse(cx, cy, r * 24 / 100, r * 13 / 100 + 1, PAL_PAINT(PAINT_BONE, 2));
        ui_fill_circle(cx + look, cy, DOOMO_MAX(1, r * 10 / 100), PAL_UI_RED);
        plat_rect(cx + look, cy - 1, 1, 2, PAL_BLACK);
    }
}

static void light_beams(int cx, int top, int bottom, int dread, uint32_t t)
{
    uint8_t beam = exo_col(EC_BEAM, dread);
    int b, k;
    if (dread >= 13 && (t / 3) % 4 == 0) return;          /* failing light */
    for (b = -1; b <= 1; b++) {
        int x0 = cx + b * 8, x1 = cx + b * (bottom - top) * 3 / 4;
        for (k = -3; k <= 3; k += 2) {
            if (dread >= 8 && (k & 2)) continue;           /* thinner, dimmer */
            plat_line(x0 + k, top, x1 + k * 3, bottom, beam);
        }
    }
    /* dust motes twinkling in the light */
    for (k = 0; k < 7; k++) {
        int u = (int)((t / 2 + (uint32_t)k * 37) % (uint32_t)(bottom - top));
        int sx = cx + ui_wave((int)t + k * 40, 160, (bottom - top) / 3) + (k - 3) * 6;
        ui_sparkle(sx, top + u, (int)((t / 3 + (uint32_t)k * 5) & 15),
                   dread < 10 ? PAL_WHITE : PAL_UI_RED);
    }
}

static void pew(int x, int y, int w, int h, int dread)
{
    uint8_t hi = exo_col(EC_WOOD_HI, dread), md = exo_col(EC_WOOD, dread), lo = exo_col(EC_WOOD_LO, dread);
    /* backrest with a rounded top */
    ui_round_rect(x, y, w, h, 3, lo);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 2, md);
    plat_rect(x + 3, y + 2, w - 6, 1, hi);
    plat_rect(x + 3, y + h / 2, w - 6, 1, lo);
    /* little carved heart in the middle */
    ui_heart(x + w / 2, y + h / 2 - 2, 2, dread < 10 ? PAL_PAINT(PAINT_ROSE, 1) : PAL_UI_RED_D);
}

void exo_church(int dread, uint32_t t, int floor_y)
{
    int W = plat_screen_w(), H = plat_screen_h(), big = H >= 200;
    int x, y, cw = big ? 26 : 18, wr = big ? 30 : 21, wy = wr + (big ? 8 : 5);
    uint8_t whi = exo_col(EC_WALL_HI, dread), wlo = exo_col(EC_WALL_LO, dread);
    uint8_t stripe = exo_col(EC_STRIPE, dread);

    /* wallpaper: soft vertical stripes with a tiny heart / drip motif */
    plat_rect(0, 0, W, floor_y, wlo);
    for (x = 4; x < W; x += 14) {
        plat_rect(x, 0, 6, floor_y, whi);
        plat_rect(x + 2, 0, 2, floor_y, stripe);
    }
    for (y = 10; y < floor_y - 16; y += 22)
        for (x = 11 + ((y / 22) & 1) * 7; x < W; x += 28) {
            if (dread < 9) ui_heart_flat(x, y, 2, stripe);
            else { plat_rect(x, y, 2, 3 + (int)((x * 7 + t / 8) % 5), PAL_UI_RED_D); }
        }
    /* wainscot panel + gold trim */
    plat_rect(0, floor_y - 12, W, 12, exo_col(EC_WOOD, dread));
    plat_rect(0, floor_y - 12, W, 1, PAL_UI_GOLD);
    plat_rect(0, floor_y - 11, W, 1, exo_col(EC_WOOD_HI, dread));
    for (x = 6; x < W; x += 20) {
        plat_rect(x, floor_y - 9, 14, 7, exo_col(EC_WOOD_LO, dread));
        plat_rect(x + 1, floor_y - 8, 12, 5, exo_col(EC_WOOD, dread));
    }

    /* floor: tiles in perspective, a carpet up the aisle */
    {
        int vy = floor_y - (H - floor_y), k;
        uint8_t fa = exo_col(EC_FLOOR_A, dread), fb = exo_col(EC_FLOOR_B, dread);
        plat_rect(0, floor_y, W, H - floor_y, fa);
        for (k = 0, y = floor_y + 3; y < H; k++) {
            int step = 3 + k * 2;
            plat_rect(0, y, W, 1, fb);
            y += step;
        }
        for (k = -10; k <= 10; k++)
            plat_line(W / 2 + k * 6, floor_y, W / 2 + k * (W / 6), H + (H - floor_y), fb);
        (void)vy;
        /* carpet */
        for (y = floor_y; y < H; y++) {
            int half = 10 + (y - floor_y) * (big ? 30 : 20) / DOOMO_MAX(1, H - floor_y);
            plat_rect(W / 2 - half, y, half * 2, 1, exo_col(EC_CARPET, dread));
            plat_rect(W / 2 - half + 2, y, 1, 1, exo_col(EC_CARPET_HI, dread));
            plat_rect(W / 2 + half - 3, y, 1, 1, exo_col(EC_CARPET_HI, dread));
        }
        plat_rect(0, floor_y, W, 1, exo_col(EC_WOOD_LO, dread));
    }

    rose_window(W / 2, wy, wr, dread, t);
    light_beams(W / 2, wy + wr / 2, floor_y + (H - floor_y) / 2, dread, t);

    /* altar under the portrait */
    {
        int aw = big ? 92 : 64, ah = big ? 22 : 14, ax = W / 2 - aw / 2, ay = floor_y - ah + 4;
        ui_round_rect(ax - 1, ay - 1, aw + 2, ah + 2, 2, exo_col(EC_WOOD_LO, dread));
        plat_rect(ax, ay, aw, ah, PAL_PAINT(PAINT_WHITE, dread < 10 ? 1 : 0));
        plat_rect(ax, ay, aw, 2, PAL_PAINT(PAINT_WHITE, 2));
        plat_rect(ax, ay + ah - 3, aw, 1, PAL_UI_GOLD);
        for (x = ax + 3; x < ax + aw - 3; x += 6) plat_rect(x, ay + ah - 2, 3, 2, PAL_UI_GOLD);
        ui_heart(W / 2, ay + ah / 2, 2, dread < 10 ? PAL_PAINT(PAINT_ROSE, 1) : PAL_UI_RED);
    }

    /* foreground pews and the velvet curtains framing it all */
    {
        int pw = big ? 70 : 48, ph = big ? 24 : 16;
        pew(-pw / 4, H - ph + 4, pw, ph, dread);
        pew(W - pw + pw / 4, H - ph + 4, pw, ph, dread);
    }
    curtain(0, cw, floor_y + (big ? 30 : 20), 1, dread, t);
    curtain(W - cw, cw, floor_y + (big ? 30 : 20), 0, dread, t);
    /* scalloped valance */
    for (x = 0; x < W; x += 12) {
        ui_fill_circle(x + 6, 2, 7, dread < 8 ? PAL_PAINT(PAINT_ROSE, 0) : PAL_PAINT(PAINT_BLOOD, 0));
        ui_fill_circle(x + 6, 1, 6, dread < 8 ? PAL_PAINT(PAINT_ROSE, 1) : PAL_PAINT(PAINT_BLOOD, 1));
        plat_rect(x + 4, 1, 3, 1, dread < 8 ? PAL_PAINT(PAINT_ROSE, 2) : PAL_PAINT(PAINT_CHERRY, 0));
    }
    plat_rect(0, 0, W, 2, PAL_UI_GOLD);
    /* creeping vignette */
    if (dread > 4) {
        int d = (dread - 4) / 2, i;
        for (i = 0; i < d && i < 6; i++) {
            plat_shade_rect(0, 0, W, 2 + i * 2, 12);
            plat_shade_rect(0, H - 2 - i * 2, W, 2 + i * 2, 12);
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Father Fuzz                                                              */
void exo_fuzz(int cx, int foot_y, uint32_t t, int expr)
{
    const DoomoResident *f = town_res(ROSTER_ID_FATHER_FUZZ);
    const DoomoLook *look;
    const DoomoImage *img;
    int k;
    if (f && f->status != RES_EMPTY && f->status != RES_DEAD &&
        strncmp(f->name, roster_name(ROSTER_ID_FATHER_FUZZ), DOOMO_NAME_LEN) == 0)
        look = &f->look;
    else if (roster_premade(ROSTER_ID_FATHER_FUZZ))
        look = &roster_premade(ROSTER_ID_FATHER_FUZZ)->look;
    else
        return;
    img = figure_image(look, RES_ALIVE, expr);
    if (!img) return;
    /* soft contact shadow */
    for (k = -1; k <= 1; k++) {
        int half = 14 - (k < 0 ? -k : k) * 5;
        plat_shade_rect(cx - half, foot_y + k, half * 2, 1, 10);
    }
    plat_image(cx - img->w / 2, foot_y - img->h + ui_wave((int)t, 48, 1), img, 1, 0, 0);
}

/* ---------------------------------------------------------------------- */
/* the host's portrait                                                      */
void exo_host_frame(int cx, int cy, const DoomoImage *img, int scale, int shake,
                    int dread, uint32_t t, int *out_w, int *out_h)
{
    int iw = (img ? img->w : PORTRAIT_W) * scale, ih = (img ? img->h : PORTRAIT_H) * scale;
    int w = iw + 10, h = ih + 10, x, y, dx = 0, dy = 0;
    if (shake > 0) {
        uint32_t hsh = doomo_hash32(t * 2654435761u);
        dx = (int)(hsh % (uint32_t)(shake * 2 + 1)) - shake;
        dy = (int)((hsh >> 8) % (uint32_t)(shake + 1)) - shake / 2;
    }
    x = cx - w / 2 + dx;
    y = cy - h / 2 + dy;
    /* a breathing purple aura when the demon stirs */
    if (dread > 3) {
        int pulse = ui_wave((int)t, 24, 2);
        ui_fill_ellipse(cx + dx, cy + dy, w / 2 + 5 + pulse, h / 2 + 4 + pulse,
                        dread > 10 ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_VAPE, 2));
        ui_fill_ellipse(cx + dx, cy + dy, w / 2 + 2 + pulse / 2, h / 2 + 2 + pulse / 2,
                        dread > 10 ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_RAMP(RAMP_VAPE, 3));
    }
    plat_shade_rect(x + 3, y + 3, w, h, 10);
    ui_round_rect(x, y, w, h, 4, PAL_RAMP(RAMP_CANDY, 1));
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 3, PAL_RAMP(RAMP_CANDY, 4));
    plat_rect(x + 4, y + 2, w - 8, 1, PAL_RAMP(RAMP_CANDY, 6));
    ui_round_rect(x + 3, y + 3, w - 6, h - 6, 2, PAL_RAMP(RAMP_CANDY, 2));
    plat_rect(x + 4, y + 4, w - 8, h - 8,
              dread < 6 ? PAL_RAMP(RAMP_PASTEL_PINK, 6) : dread < 11 ? PAL_RAMP(RAMP_VAPE, 1)
                                                                     : PAL_RAMP(RAMP_BLOOD, 0));
    if (img) plat_image(x + 5, y + 5, img, scale, 0, 0);
    else ui_text_center(x + w / 2, y + h / 2 - 4, "?", PAL_WHITE);
    /* gilded corner hearts */
    ui_heart(x + 3, y + 3, 2, PAL_UI_GOLD);
    ui_heart(x + w - 4, y + 3, 2, PAL_UI_GOLD);
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

/* ---------------------------------------------------------------------- */
void exo_candles(int cx, int y, int lit, int total, uint32_t t, int big)
{
    int i, sp = big ? 14 : 10, x0 = cx - (total - 1) * sp / 2;
    for (i = 0; i < total; i++) {
        int x = x0 + i * sp, h = big ? 12 : 9;
        /* holder */
        ui_fill_ellipse(x, y + h + 1, 4, 1, PAL_RAMP(RAMP_CANDY, 2));
        plat_rect(x - 3, y + h, 7, 1, PAL_RAMP(RAMP_CANDY, 4));
        /* wax */
        plat_rect(x - 2, y, 5, h, PAL_PAINT(PAINT_BONE, 0));
        plat_rect(x - 1, y, 3, h, PAL_PAINT(PAINT_BONE, 2));
        plat_rect(x - 1, y + 2, 1, 3, PAL_WHITE);
        plat_rect(x, y - 1, 1, 1, PAL_GRAY_D);           /* wick */
        if (i < lit) {
            int f = (int)((t / 3 + (uint32_t)i * 2) % 4), fy = y - 6 - (f == 1);
            int sway = (f == 2) - (f == 3);
            ui_fill_ellipse(x + sway, fy + 2, 2, 3, PAL_RAMP(RAMP_LAVA, 4));
            ui_fill_ellipse(x + sway, fy + 3, 1, 2, PAL_RAMP(RAMP_LAVA, 6));
            plat_rect(x + sway, fy + 3, 1, 2, PAL_RAMP(RAMP_LAVA, 7));
            plat_rect(x + sway, fy, 1, 1, PAL_RAMP(RAMP_LAVA, 5));
        } else {
            /* a curl of smoke from a snuffed candle */
            int k;
            for (k = 0; k < 4; k++) {
                int sy = y - 3 - k * 3 - (int)((t / 4) % 3);
                plat_rect(x + ui_wave((int)t + k * 9, 30, 1), sy, 1, 1, k & 1 ? PAL_GRAY : PAL_GRAY_L);
            }
        }
    }
}

/* ---------------------------------------------------------------------- */
void exo_demon(int x, int y, uint32_t t, int flash, int scale)
{
    const DoomoImage *img = sprite_image(SPR_VAPE_WRAITH, flash > 0 ? DFRAME_PAIN :
                                         (int)((t / 8) & 1));
    if (!img) return;
    if (scale < 1) scale = 1;
    if (flash > 0 && (flash & 2))
        plat_image(x - img->w * scale / 2, y - img->h * scale / 2, img, scale, IMG_SILHOUETTE, PAL_WHITE);
    else
        plat_image(x - img->w * scale / 2, y - img->h * scale / 2 + ui_wave((int)t, 20, 1),
                   img, scale, IMG_SHADOW, 0);
}

/* ---------------------------------------------------------------------- */
void exo_bubble(int x, int y, int w, const char *text, int tx, int ty, int creepy)
{
    int lines = ui_text_wrap(0, 0, w - 8, text, 0, 0), h = lines * FONT_LINE + 6, i;
    uint8_t fill = creepy ? PAL_RAMP(RAMP_VAPE, 1) : PAL_WHITE;
    uint8_t edge = creepy ? PAL_UI_RED : PAL_UI_PINK_D;
    int bx = DOOMO_CLAMP(x, 2, plat_screen_w() - w - 2);
    /* tail */
    for (i = 0; i < 6; i++) {
        int px = tx + (bx + w / 2 - tx) * i / 6, py = ty + (y + h - ty) * i / 6;
        plat_rect(px - 1, py - 1, 3, 3, edge);
    }
    for (i = 1; i < 6; i++) {
        int px = tx + (bx + w / 2 - tx) * i / 6, py = ty + (y + h - ty) * i / 6;
        plat_rect(px, py, 1, 1, fill);
    }
    ui_round_rect(bx - 1, y - 1, w + 2, h + 2, 4, edge);
    ui_round_rect(bx, y, w, h, 3, fill);
    ui_text_wrap(bx + 4, y + 3, w - 8, text, creepy ? PAL_UI_RED : PAL_RAMP(RAMP_PASTEL_PINK, 0), 1);
}
