/* concert_gfx.c - the Concert Hall stage (owner: concert).
 *
 * Painter's order: backdrop + theme props, LED wall light show (one of 16
 * shows, one per genre), speaker stacks, truss + lamps, the stage floor,
 * sweeping light beams (50% checker = translucent), then the performers
 * (lead singer figure with a mic stand, band mates with procedurally drawn
 * instruments, or up to 25 choir members on risers / in a semicircle / by
 * section / in a pyramid), stage fx (pyro, confetti, smoke), the crowd
 * seen from behind (front row chibis, back rows as silhouettes, glow
 * sticks), the curtain and the karaoke pill.
 *
 * Everything is integer, made of plat_rect / plat_checker spans and the
 * ui primitives, and animates from the beat (kick / snare / onset flashes)
 * and the scene clock. Chibis come from the portrait cache (one image per
 * citizen, fixed frame, so at most ~45 cached images are in use).
 */
#include "concert_int.h"

const char *const concert_stage_names[STAGE_COUNT] = {
    "Sugar Bowl", "Disco Inferno", "Graveyard Bash", "Hellfire Pit", "Dreamland"
};

/* ====================================================================== */
/* helpers                                                                  */
static uint32_t hash2(uint32_t a, uint32_t b) { return doomo_hash32(a * 2654435761u ^ (b + 0x9E37u)); }

/* a checker span with a consistent global parity (reads as 50% light) */
static void cspan(int x0, int x1, int y, int h, uint8_t c)
{
    if ((x0 + y) & 1) x0++;
    if (x1 > x0) plat_checker(x0, y, x1 - x0, h, c);
}

static void rect_clip(int x, int y, int w, int h, uint8_t c, int cx0, int cy0, int cx1, int cy1)
{
    if (x < cx0) { w -= cx0 - x; x = cx0; }
    if (y < cy0) { h -= cy0 - y; y = cy0; }
    if (x + w > cx1) w = cx1 - x;
    if (y + h > cy1) h = cy1 - y;
    if (w > 0 && h > 0) plat_rect(x, y, w, h, c);
}

void concert_note_glyph(int x, int y, uint8_t c)
{
    /* an eighth note: head, stem, flag */
    plat_rect(x, y + 4, 3, 2, c);
    plat_rect(x + 1, y + 3, 2, 1, c);
    plat_rect(x + 2, y, 1, 4, c);
    plat_rect(x + 3, y, 1, 1, c);
    plat_rect(x + 4, y + 1, 1, 2, c);
}

void concert_confetti(int x0, int y0, int w, int h, uint32_t t, int n, int fall)
{
    static const uint8_t cols[6] = { PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_SKY, PAINT_MINT,
                                     PAINT_LAVENDER, PAINT_TANGERINE };
    int i;
    if (w <= 0 || h <= 0) return;
    for (i = 0; i < n; i++) {
        uint32_t r = hash2((uint32_t)i, 77u);
        int x = x0 + (int)(r % (uint32_t)w) + ui_wave((int)t + i * 7, 40, 3);
        int y = y0 + (int)(((r >> 8) % (uint32_t)h + (uint32_t)(fall ? t * (1 + (r >> 20) % 3) : 0)) %
                           (uint32_t)h);
        int ph = (int)((t / 3 + r) & 3);
        uint8_t c = PAL_PAINT(cols[(r >> 4) % 6], 2 - (ph & 1));
        if (ph == 0) plat_rect(x, y, 2, 2, c);
        else if (ph == 1) plat_rect(x, y, 3, 1, c);
        else if (ph == 2) plat_rect(x, y, 1, 2, c);
        else plat_rect(x, y, 2, 1, c);
    }
}

/* ====================================================================== */
/* layout                                                                   */
void concert_layout(StageLayout *L)
{
    int w = plat_screen_w(), h = plat_screen_h();
    L->w = w;
    L->h = h;
    L->big = h >= 200;
    L->truss_h = L->big ? 12 : 9;
    L->floor_y = h * 62 / 100;
    L->stage_front = L->floor_y + (L->big ? 20 : 14);
    L->wall_w = w * 3 / 4;
    L->wall_x = (w - L->wall_w) / 2;
    L->wall_y = L->truss_h + (L->big ? 8 : 5);
    L->wall_h = L->floor_y - L->wall_y - (L->big ? 44 : 34);
    L->lyric_y = h - (L->big ? 24 : 19);
}

/* ====================================================================== */
/* palette per stage theme                                                  */
typedef struct StagePal { uint8_t sky[4], floor0, floor1, floor_line, lip, prop0, prop1; } StagePal;

static void stage_pal(int stage, StagePal *p)
{
    switch (stage) {
    case STAGE_DISCO_INFERNO:
        p->sky[0] = PAL_RAMP(RAMP_VAPE, 0); p->sky[1] = PAL_RAMP(RAMP_VAPE, 1);
        p->sky[2] = PAL_RAMP(RAMP_VAPE, 2); p->sky[3] = PAL_PAINT(PAINT_MAGENTA, 0);
        p->floor0 = PAL_RAMP(RAMP_VAPE, 1); p->floor1 = PAL_RAMP(RAMP_VAPE, 2);
        p->floor_line = PAL_RAMP(RAMP_VAPE, 0); p->lip = PAL_RAMP(RAMP_VAPE, 0);
        p->prop0 = PAL_PAINT(PAINT_SILVER, 1); p->prop1 = PAL_PAINT(PAINT_WHITE, 2);
        break;
    case STAGE_GRAVEYARD:
        p->sky[0] = PAL_RAMP(RAMP_SKY, 0); p->sky[1] = PAL_RAMP(RAMP_SKY, 1);
        p->sky[2] = PAL_RAMP(RAMP_MINT, 1); p->sky[3] = PAL_RAMP(RAMP_MINT, 2);
        p->floor0 = PAL_RAMP(RAMP_WOOD, 1); p->floor1 = PAL_RAMP(RAMP_WOOD, 2);
        p->floor_line = PAL_RAMP(RAMP_WOOD, 0); p->lip = PAL_RAMP(RAMP_STONE, 0);
        p->prop0 = PAL_RAMP(RAMP_STONE, 3); p->prop1 = PAL_RAMP(RAMP_STONE, 5);
        break;
    case STAGE_HELLFIRE:
        p->sky[0] = PAL_RAMP(RAMP_BLOOD, 0); p->sky[1] = PAL_RAMP(RAMP_BLOOD, 1);
        p->sky[2] = PAL_RAMP(RAMP_LAVA, 1); p->sky[3] = PAL_RAMP(RAMP_LAVA, 2);
        p->floor0 = PAL_RAMP(RAMP_STEEL, 1); p->floor1 = PAL_RAMP(RAMP_STEEL, 2);
        p->floor_line = PAL_RAMP(RAMP_STEEL, 0); p->lip = PAL_RAMP(RAMP_BLOOD, 0);
        p->prop0 = PAL_RAMP(RAMP_STEEL, 2); p->prop1 = PAL_RAMP(RAMP_LAVA, 5);
        break;
    case STAGE_DREAMLAND:
        p->sky[0] = PAL_RAMP(RAMP_SKY, 1); p->sky[1] = PAL_RAMP(RAMP_SKY, 2);
        p->sky[2] = PAL_RAMP(RAMP_VAPE, 3); p->sky[3] = PAL_RAMP(RAMP_VAPE, 4);
        p->floor0 = PAL_RAMP(RAMP_VAPE, 6); p->floor1 = PAL_RAMP(RAMP_VAPE, 7);
        p->floor_line = PAL_RAMP(RAMP_VAPE, 5); p->lip = PAL_RAMP(RAMP_VAPE, 3);
        p->prop0 = PAL_PAINT(PAINT_WHITE, 1); p->prop1 = PAL_PAINT(PAINT_WHITE, 2);
        break;
    default:   /* Sugar Bowl */
        p->sky[0] = PAL_RAMP(RAMP_VAPE, 1); p->sky[1] = PAL_RAMP(RAMP_VAPE, 2);
        p->sky[2] = PAL_RAMP(RAMP_PASTEL_PINK, 2); p->sky[3] = PAL_RAMP(RAMP_PASTEL_PINK, 3);
        p->floor0 = PAL_RAMP(RAMP_PASTEL_PINK, 4); p->floor1 = PAL_RAMP(RAMP_PASTEL_PINK, 5);
        p->floor_line = PAL_RAMP(RAMP_PASTEL_PINK, 3); p->lip = PAL_RAMP(RAMP_PASTEL_PINK, 1);
        p->prop0 = PAL_PAINT(PAINT_BUBBLEGUM, 1); p->prop1 = PAL_PAINT(PAINT_WHITE, 2);
        break;
    }
}

/* ====================================================================== */
/* the LED wall light shows                                                 */
typedef struct Wall { int x, y, w, h; } Wall;

static void wall_rect(const Wall *W, int x, int y, int w, int h, uint8_t c)
{
    rect_clip(x, y, w, h, c, W->x, W->y, W->x + W->w, W->y + W->h);
}

static void wall_circle(const Wall *W, int cx, int cy, int r, uint8_t c)
{
    int y;
    for (y = -r; y <= r; y++) {
        int dx = 0;
        while ((dx + 1) * (dx + 1) + y * y <= r * r + r) dx++;
        wall_rect(W, cx - dx, cy + y, 2 * dx + 1, 1, c);
    }
}

static void wall_heart(const Wall *W, int cx, int cy, int s, uint8_t c)
{
    /* two bumps and a point, from rows (clipped to the wall) */
    int y;
    for (y = 0; y < s * 2; y++) {
        int hw;
        if (y < s / 2 + 1) {
            int bump = s / 2 + 1, dy = bump - y, dx = 0;
            while ((dx + 1) * (dx + 1) + dy * dy <= bump * bump) dx++;
            wall_rect(W, cx - s / 2 - dx, cy - s + y, 2 * dx + 1, 1, c);
            wall_rect(W, cx + s / 2 - dx, cy - s + y, 2 * dx + 1, 1, c);
            continue;
        }
        hw = s + 1 - (y - s / 2) * (s + 1) / (s * 2 - s / 2);
        if (hw > 0) wall_rect(W, cx - hw, cy - s + y, 2 * hw + 1, 1, c);
    }
}

static void wall_skull(const Wall *W, int cx, int cy, int s, uint8_t c, uint8_t dark)
{
    wall_circle(W, cx, cy, s, c);
    wall_rect(W, cx - s / 2 - 1, cy + s - 1, s + 3, s / 2 + 1, c);
    wall_rect(W, cx - s / 2 - 1, cy - 1, s / 2 + 1, s / 2 + 1, dark);
    wall_rect(W, cx + 1, cy - 1, s / 2 + 1, s / 2 + 1, dark);
    wall_rect(W, cx, cy + s / 2, 1, 2, dark);
}

static void draw_show(const StageView *v, const Wall *W)
{
    const ConcertGenre *g = &concert_genres[v->genre];
    uint8_t c0 = PAL_PAINT(g->c0, 1), c1 = PAL_PAINT(g->c1, 2), c2 = PAL_PAINT(g->c2, 2);
    uint8_t c0d = PAL_PAINT(g->c0, 0), bg = PAL_RAMP(RAMP_STONE, 0);
    int t = (int)v->t, i, k, pulse = v->kick > 0 ? v->kick : 0;
    int lit = v->lights;
    int cx = W->x + W->w / 2, cy = W->y + W->h / 2;
    plat_rect(W->x, W->y, W->w, W->h, bg);
    if (lit <= 0) return;
    switch (g->show) {
    case SHOW_HEARTS: {
        int s = W->h / 6 + (pulse > 2 ? 2 : 0);
        for (i = 0; i < 7; i++) {
            int hx = W->x + W->w * (2 * i + 1) / 14, hy = cy + ui_wave(t + i * 9, 45, W->h / 6);
            uint8_t c = (i + t / 8) % 3 == 0 ? c0 : (i + t / 8) % 3 == 1 ? c1 : c2;
            wall_heart(W, hx, hy, s - (i & 1) * 2, c);
        }
        break; }
    case SHOW_EQ: case SHOW_PIXELS: {
        int bars = g->show == SHOW_EQ ? 16 : 12, bw = W->w / bars;
        for (i = 0; i < bars; i++) {
            int lvl = (int)(hash2((uint32_t)(t / 4), (uint32_t)i) % (uint32_t)(W->h - 4));
            int cells, ch = g->show == SHOW_PIXELS ? bw : 4;
            lvl = lvl / 2 + W->h / 4 + (pulse * W->h) / 12;
            cells = lvl / ch;
            for (k = 0; k < cells; k++) {
                uint8_t c = k * ch > W->h * 2 / 3 ? c1 : k * ch > W->h / 3 ? c2 : c0;
                if (g->show == SHOW_PIXELS && ((hash2((uint32_t)(t / 8 + k), (uint32_t)i) & 3) == 0))
                    c = PAL_PAINT(PAINT_WHITE, 2);
                wall_rect(W, W->x + i * bw + 1, W->y + W->h - (k + 1) * ch, bw - 2, ch - 1, c);
            }
        }
        break; }
    case SHOW_SKYLINE: {
        wall_circle(W, W->x + W->w * 3 / 4, W->y + W->h / 3, W->h / 6, c1);
        for (i = 0; i < 10; i++) {
            int bx = W->x + i * W->w / 10, bh = W->h / 3 + (int)(hash2((uint32_t)i, 3u) % (uint32_t)(W->h / 2));
            wall_rect(W, bx, W->y + W->h - bh, W->w / 10 - 1, bh, c0d);
            for (k = 0; k < bh / 5; k++) {
                if ((hash2((uint32_t)(i * 31 + k), (uint32_t)(t / 20)) & 3) == 0) continue;
                wall_rect(W, bx + 2, W->y + W->h - bh + 3 + k * 5, 2, 2, c1);
                wall_rect(W, bx + W->w / 10 - 6, W->y + W->h - bh + 3 + k * 5, 2, 2, c2);
            }
        }
        break; }
    case SHOW_STARS: case SHOW_NOTES: {
        for (i = 0; i < 18; i++) {
            uint32_t r = hash2((uint32_t)i, 9u);
            int sx = W->x + 3 + (int)(r % (uint32_t)(W->w - 6));
            int sy = W->y + 3 + (int)((r >> 10) % (uint32_t)(W->h - 6));
            if (g->show == SHOW_NOTES) {
                sy = W->y + W->h - 2 - (int)(((r >> 10) + (uint32_t)t / 2) % (uint32_t)(W->h + 6));
                concert_note_glyph(sx + ui_wave(t + i * 5, 30, 3), sy, (i & 1) ? c1 : c2);
            } else {
                ui_sparkle(sx, sy, (t / 3 + (int)r) & 15, (i & 1) ? c1 : PAL_PAINT(PAINT_WHITE, 2));
            }
        }
        if (g->show == SHOW_STARS) {
            int mx = W->x + W->w / 5, my = W->y + W->h / 3, mr = W->h / 5;
            wall_circle(W, mx, my, mr, PAL_PAINT(PAINT_LEMON, 2));
            wall_circle(W, mx + mr / 2, my - mr / 3, mr, bg);
        }
        break; }
    case SHOW_SPOTS: {
        for (i = 0; i < 6; i++) {
            int sx = cx + ui_wave(t + i * 40, 160 + i * 20, W->w / 2 - 8);
            int sy = cy + ui_wave(t + i * 25, 120, W->h / 3);
            wall_circle(W, sx, sy, W->h / 5 + (i & 1) * 2, i % 3 == 0 ? c0 : i % 3 == 1 ? c1 : c2);
        }
        break; }
    case SHOW_DISCO: {
        int cs = W->h / 4, nx = W->w / cs + 1, ny = 4;
        for (k = 0; k < ny; k++)
            for (i = 0; i < nx; i++) {
                uint32_t r = hash2((uint32_t)(i + k * 17), (uint32_t)(t / (opt_get(OPT_REDUCE_FLASH) ? 48 : 8)));
                uint8_t c = (r & 3) == 0 ? c0 : (r & 3) == 1 ? c1 : (r & 3) == 2 ? c2 : bg;
                wall_rect(W, W->x + i * cs + 1, W->y + k * cs + 1, cs - 2, cs - 2, c);
            }
        break; }
    case SHOW_WAVES: {
        for (k = 0; k < 4; k++) {
            uint8_t c = k == 0 ? c2 : k == 1 ? c1 : k == 2 ? c0 : c0d;
            for (i = 0; i < W->w; i += 4) {
                int top = W->y + W->h * (k + 1) / 5 + ui_wave(t * 2 + i * 3 + k * 50, 120, 3);
                wall_rect(W, W->x + i, top, 4, W->y + W->h - top, c);
            }
        }
        break; }
    case SHOW_GLASS: {
        int pw = W->w / 8, ph = W->h / 2;
        for (k = 0; k < 2; k++)
            for (i = 0; i < 8; i++) {
                int glow = ((i * 3 + k * 5 + t / 12) % 16) == 0;
                uint8_t c = (i + k) % 3 == 0 ? c0 : (i + k) % 3 == 1 ? c1 : c2;
                if (glow) c = PAL_PAINT(PAINT_WHITE, 2);
                ui_round_rect(W->x + i * pw + 2, W->y + k * ph + 2, pw - 4, ph - 4, k ? 1 : 4, c);
            }
        break; }
    case SHOW_GRID: {
        int hz = W->y + W->h / 2, sr = W->h / 3;
        for (k = 0; k <= sr; k++) {
            int dx = 0;
            while ((dx + 1) * (dx + 1) + k * k <= sr * sr) dx++;
            if ((k % 4) == 3) continue;                    /* sun stripes */
            wall_rect(W, cx - dx, hz - k, 2 * dx + 1, 1, k > sr / 2 ? c1 : c0);
        }
        wall_rect(W, W->x, hz, W->w, W->h - W->h / 2, PAL_RAMP(RAMP_VAPE, 0));
        for (k = 0; k < 5; k++) {
            int yy = hz + 1 + ((k * k * 3 + t / 2) % (W->h / 2));
            wall_rect(W, W->x, yy, W->w, 1, c2);
        }
        for (i = -6; i <= 6; i++) plat_line(cx + i * 6, hz, cx + i * W->w / 6, W->y + W->h - 1, c2);
        break; }
    case SHOW_BATS: {
        int mr = W->h / 3;
        wall_circle(W, cx, cy, mr, PAL_PAINT(PAINT_LAVENDER, 2));
        for (i = 0; i < 7; i++) {
            int bx = W->x + (int)((uint32_t)(t * (2 + i % 3) / 2 + i * 41) % (uint32_t)W->w);
            int by = W->y + 6 + (i * 13) % (W->h - 12) + ui_wave(t + i * 11, 24, 3);
            int flap = ((t / 3 + i) & 1) ? 2 : -1;
            wall_rect(W, bx - 1, by, 3, 2, PAL_RAMP(RAMP_VAPE, 0));
            plat_line(bx - 1, by, bx - 5, by - flap, PAL_RAMP(RAMP_VAPE, 0));
            plat_line(bx + 1, by, bx + 5, by - flap, PAL_RAMP(RAMP_VAPE, 0));
        }
        break; }
    case SHOW_FLAMES: {
        /* tongues of fire: each row narrower towards the tip, flickering */
        int fw = W->w / 9;
        for (i = 0; i < 9; i++) {
            int fx = W->x + fw * i + fw / 2;
            int fh = W->h * 2 / 3 + (int)(hash2((uint32_t)(t / 3), (uint32_t)i) % (uint32_t)(W->h / 3)) + pulse;
            int yy;
            for (yy = 0; yy < fh; yy += 2) {
                int hw = (fw / 2 + 1) * (fh - yy) / fh;
                int sway = ui_wave(t * 2 + i * 23 + yy * 3, 40, 1 + yy / 10);
                int f = yy * 8 / fh;
                if (hw <= 0) continue;
                wall_rect(W, fx - hw + sway, W->y + W->h - yy - 2, 2 * hw, 2,
                          PAL_RAMP(RAMP_LAVA, DOOMO_CLAMP(5 - f * 4 / 7, 1, 7)));
                if (hw > 2) wall_rect(W, fx - hw / 2 + sway, W->y + W->h - yy - 2, hw, 2,
                                      PAL_RAMP(RAMP_LAVA, DOOMO_CLAMP(7 - f * 4 / 7, 2, 7)));
            }
        }
        break; }
    case SHOW_SKULLS: {
        for (i = 0; i < 5; i++) {
            int sx = W->x + W->w * (2 * i + 1) / 10;
            int sy = cy + ((((t / 4) + i) & 1) ? -2 : 2) - (pulse > 2 && (i & 1) ? 3 : 0);
            wall_skull(W, sx, sy - 2, W->h / 6, (i & 1) ? c1 : PAL_RAMP(RAMP_BONE, 6), bg);
        }
        break; }
    case SHOW_GEARS: {
        for (i = 0; i < 3; i++) {
            int gx = W->x + W->w * (i + 1) / 4, gy = cy + (i == 1 ? 3 : -3), r = W->h / 4 + (i == 1 ? 3 : 0);
            int a = (i & 1 ? -t : t) * 16;
            wall_circle(W, gx, gy, r, i == 1 ? c0 : PAL_RAMP(RAMP_STEEL, 4));
            wall_circle(W, gx, gy, r / 3, bg);
            for (k = 0; k < 8; k++) {
                int ang = (a + k * 512) & 4095;
                int tx = gx + (int)((fx_cos(ang) * (r + 2)) >> 16), ty = gy + (int)((fx_sin(ang) * (r + 2)) >> 16);
                wall_rect(W, tx - 2, ty - 2, 4, 4, i == 1 ? c0 : PAL_RAMP(RAMP_STEEL, 4));
            }
        }
        if (pulse > 3) wall_rect(W, W->x, W->y, W->w, 2, c1);
        break; }
    case SHOW_FIELDS: {
        wall_circle(W, W->x + W->w / 5, W->y + W->h / 3, W->h / 6, PAL_PAINT(PAINT_LEMON, 2));
        for (k = 0; k < 3; k++)
            for (i = 0; i < W->w; i += 3) {
                int top = W->y + W->h * (k + 2) / 6 + ui_wave(i * 4 + k * 300, 400, 4);
                wall_rect(W, W->x + i, top, 3, W->y + W->h - top,
                          k == 0 ? PAL_RAMP(RAMP_GRASS, 5) : k == 1 ? PAL_RAMP(RAMP_GRASS, 4) : PAL_RAMP(RAMP_GRASS, 3));
            }
        for (i = 0; i < 9; i++) {
            int fx = W->x + 6 + i * (W->w - 12) / 8, fy = W->y + W->h - 5 - (i & 1) * 4;
            wall_rect(W, fx, fy - 1 + (((t / 10) + i) & 1), 3, 3, i & 1 ? c0 : c1);
        }
        break; }
    default: break;
    }
    /* dim the wall while the lights come up */
    if (lit < 16) plat_shade_rect(W->x, W->y, W->w, W->h, 15 - (16 - lit) * 3 / 4);
    (void)c0d;
}

/* ====================================================================== */
/* stage                                                                    */
static void draw_backdrop(const StageView *v, const StageLayout *L, const StagePal *p)
{
    int i, y0 = 0, band = L->floor_y / 4;
    for (i = 0; i < 4; i++) {
        int y1 = i == 3 ? L->floor_y : y0 + band;
        plat_rect(0, y0, L->w, y1 - y0, p->sky[i]);
        if (i < 3) cspan(0, L->w, y1 - 1, 2, p->sky[i + 1]);
        y0 = y1;
    }
    /* theme props behind the wall */
    switch (v->stage) {
    case STAGE_SUGAR_BOWL:
        for (i = 0; i < 2; i++) {
            int px = i ? L->w - L->wall_x / 2 - 4 : L->wall_x / 2 - 4, k;
            plat_rect(px, L->wall_y, 9, L->floor_y - L->wall_y, PAL_PAINT(PAINT_WHITE, 2));
            for (k = L->wall_y + ((int)(v->t / 4) % 8) - 8; k < L->floor_y; k += 8)
                rect_clip(px, k, 9, 3, PAL_PAINT(PAINT_BUBBLEGUM, 1), 0, L->wall_y, L->w, L->floor_y);
            ui_fill_circle(px + 4, L->wall_y - 2, 6, PAL_PAINT(PAINT_BUBBLEGUM, 2));
            ui_sparkle(px + 4, L->wall_y - 10, (int)(v->t / 2 + i * 7) & 15, PAL_PAINT(PAINT_LEMON, 2));
        }
        break;
    case STAGE_DISCO_INFERNO: break;            /* the ball hangs from the truss */
    case STAGE_GRAVEYARD:
        for (i = 0; i < 4; i++) {
            int gx = i < 2 ? 6 + i * 14 : L->w - 20 - (i - 2) * 14, gy = L->floor_y - 4;
            ui_round_rect(gx, gy - 16 - (i & 1) * 4, 11, 18 + (i & 1) * 4, 4, p->prop0);
            plat_rect(gx + 5, gy - 12 - (i & 1) * 4, 1, 6, p->prop1);
            plat_rect(gx + 3, gy - 10 - (i & 1) * 4, 5, 1, p->prop1);
        }
        cspan(0, L->w, L->floor_y - 6, 6, PAL_RAMP(RAMP_MINT, 5));
        break;
    case STAGE_HELLFIRE:
        for (i = 0; i < 2; i++) {
            int bx = i ? L->w - 14 : 14, by = L->floor_y - 4, k;
            plat_rect(bx - 4, by - 10, 9, 10, PAL_RAMP(RAMP_STEEL, 1));
            plat_rect(bx - 6, by - 12, 13, 3, PAL_RAMP(RAMP_STEEL, 3));
            for (k = 0; k < 4; k++)
                ui_fill_circle(bx + ui_wave((int)v->t + k * 9 + i * 20, 12, 1), by - 15 - k * 4,
                               4 - k, PAL_RAMP(RAMP_LAVA, 3 + k));
        }
        break;
    case STAGE_DREAMLAND:
        for (i = 0; i < 5; i++) {
            int cxx = (i * 67 + (int)v->t / 3) % (L->w + 40) - 20;
            int cyy = L->wall_y + 4 + (i * 23) % (L->floor_y - L->wall_y - 10);
            if (cxx > L->wall_x - 8 && cxx < L->wall_x + L->wall_w + 8) continue;
            ui_fill_ellipse(cxx, cyy, 12, 5, p->prop1);
            ui_fill_ellipse(cxx - 6, cyy + 2, 8, 4, p->prop0);
        }
        break;
    }
}

static void draw_speakers(const StageView *v, const StageLayout *L)
{
    int i, k, sw = L->big ? 26 : 18, sh = L->big ? 26 : 18;
    for (i = 0; i < 2; i++) {
        int x = i ? L->wall_x + L->wall_w + 2 : L->wall_x - sw - 2;
        if (x < 0) x = 0;
        if (x + sw > L->w) x = L->w - sw;
        for (k = 0; k < 2; k++) {
            int y = L->floor_y - (k + 1) * sh, r = sh / 3 + (v->kick > 2 ? 1 : 0);
            ui_round_rect(x, y, sw, sh - 1, 2, PAL_RAMP(RAMP_STONE, 1));
            ui_frame(x, y, sw, sh - 1, PAL_RAMP(RAMP_STONE, 0));
            ui_fill_circle(x + sw / 2, y + sh / 2, r, PAL_RAMP(RAMP_STONE, 0));
            ui_fill_circle(x + sw / 2, y + sh / 2, r / 2, PAL_RAMP(RAMP_STONE, 3));
            plat_rect(x + sw / 2 - 1, y + sh / 2 - 1, 1, 1, PAL_RAMP(RAMP_STONE, 5));
        }
    }
}

static void lamp_xy(const StageLayout *L, int i, int *x, int *y)
{
    *x = L->w * (2 * i + 1) / 12;
    *y = L->truss_h - 1;
}

static void draw_truss(const StageView *v, const StageLayout *L)
{
    const ConcertGenre *g = &concert_genres[v->genre];
    int i, th = L->truss_h, x;
    uint8_t s0 = PAL_RAMP(RAMP_STEEL, 2), s1 = PAL_RAMP(RAMP_STEEL, 4), s2 = PAL_RAMP(RAMP_STEEL, 6);
    plat_rect(0, 0, L->w, th - 3, PAL_RAMP(RAMP_STONE, 0));
    plat_rect(0, 1, L->w, 1, s1);
    plat_rect(0, th - 5, L->w, 1, s1);
    for (x = -4; x < L->w; x += 8) {
        plat_line(x, 1, x + 4, th - 5, s0);
        plat_line(x + 4, th - 5, x + 8, 1, s0);
    }
    plat_rect(0, 0, L->w, 1, s2);
    /* lamps: flash on the beat, colour of the genre */
    for (i = 0; i < 6; i++) {
        int lx, ly, on = v->lights > 0 && (((v->t / 8) + (uint32_t)i) % 3 != 0 || v->kick > 0);
        uint8_t c = i % 3 == 0 ? PAL_PAINT(g->c0, 2) : i % 3 == 1 ? PAL_PAINT(g->c1, 2) : PAL_PAINT(g->c2, 2);
        lamp_xy(L, i, &lx, &ly);
        plat_rect(lx - 3, ly - 5, 7, 5, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(lx - 2, ly - 1, 5, 2, on ? c : PAL_RAMP(RAMP_STONE, 2));
        if (on) plat_rect(lx - 1, ly - 1, 2, 1, PAL_WHITE);
    }
    /* disco ball for the disco stage */
    if (v->stage == STAGE_DISCO_INFERNO) {
        int bx = L->w / 2, by = th + 10, r = L->big ? 9 : 7, k;
        plat_rect(bx, th - 3, 1, 4, s1);
        ui_fill_circle(bx, by, r, PAL_PAINT(PAINT_SILVER, 1));
        for (k = -r; k <= r; k += 3)
            for (x = -r; x <= r; x += 3)
                if (x * x + k * k < r * r && (((x + k) / 3 + (int)(v->t / 4)) & 1))
                    plat_rect(bx + x, by + k, 2, 2, PAL_PAINT(PAINT_WHITE, 2));
        for (k = 0; k < 4; k++)
            ui_sparkle(bx + ui_wave((int)v->t * 3 + k * 50, 200, L->w / 2 - 10),
                       by + 10 + k * 9 + ui_wave((int)v->t + k * 30, 90, 6), ((int)v->t / 2 + k * 4) & 15,
                       PAL_PAINT(PAINT_WHITE, 2));
    }
}

static void draw_floor(const StageView *v, const StageLayout *L, const StagePal *p)
{
    int y, x, depth = L->stage_front - L->floor_y, i;
    plat_rect(0, L->floor_y, L->w, depth, p->floor0);
    switch (v->stage) {
    case STAGE_DISCO_INFERNO: {
        int cs = depth / 2;
        for (y = 0; y < 2; y++)
            for (x = 0; x * cs < L->w + cs; x++) {
                uint32_t r = hash2((uint32_t)(x + y * 40), v->t / (opt_get(OPT_REDUCE_FLASH) ? 48u : 8u));
                uint8_t c = (r & 3) == 0 ? PAL_PAINT(PAINT_MAGENTA, 2) : (r & 3) == 1 ? PAL_PAINT(PAINT_CYAN, 2) :
                            (r & 3) == 2 ? PAL_PAINT(PAINT_LEMON, 2) : p->floor1;
                plat_rect(x * cs + (y ? cs / 2 : 0) - cs / 2, L->floor_y + y * cs + 1, cs - 1, cs - 1, c);
            }
        break; }
    case STAGE_HELLFIRE:
        for (x = 0; x < L->w; x += 6) plat_rect(x, L->floor_y, 1, depth, p->floor_line);
        for (y = L->floor_y + 3; y < L->stage_front; y += 4) plat_rect(0, y, L->w, 1, p->floor_line);
        cspan(0, L->w, L->stage_front - 3, 3, PAL_RAMP(RAMP_LAVA, 4 + (v->kick > 0)));
        break;
    case STAGE_DREAMLAND:
        for (i = 0; i < 10; i++)
            ui_fill_ellipse(i * L->w / 9, L->floor_y + depth / 2 + (i & 1) * 2, L->w / 14, depth / 2, p->floor1);
        break;
    default:
        plat_rect(0, L->floor_y, L->w, 2, p->floor1);
        for (y = L->floor_y + 4; y < L->stage_front; y += 4) plat_rect(0, y, L->w, 1, p->floor_line);
        for (y = 0; y < depth / 4; y++)
            for (x = (y * 37) % 29; x < L->w; x += 29 + y * 3)
                plat_rect(x, L->floor_y + y * 4 + 1, 1, 3, p->floor_line);
        break;
    }
    /* the house: dark seats below the stage, lit a little by the show */
    {
        int top = L->stage_front + (L->big ? 7 : 5), hh = L->h - top;
        int hell = v->stage == STAGE_HELLFIRE;
        plat_rect(0, top, L->w, hh, hell ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_VAPE, 1));
        cspan(0, L->w, top, hh / 3, hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_VAPE, 2));
        cspan(0, L->w, top + hh / 3, hh / 3, hell ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_RAMP(RAMP_VAPE, 0));
    }
    /* the front lip with footlights */
    plat_rect(0, L->stage_front, L->w, L->big ? 7 : 5, p->lip);
    plat_rect(0, L->stage_front, L->w, 1, ui_lighter(p->lip));
    for (x = 8; x < L->w; x += L->w / 8) {
        int on = v->lights > 4 && (((uint32_t)x / 8 + v->t / 6) & 1);
        plat_rect(x, L->stage_front + 1, 4, 2, on ? PAL_PAINT(PAINT_LEMON, 2) : PAL_RAMP(RAMP_STONE, 2));
    }
}

/* beams from the truss lamps to targets on the stage */
static void beam(int ax, int ay, int tx, int ty, int hw0, int hw1, uint8_t c)
{
    int y, step = 3, dy = ty - ay;
    if (dy <= 0) return;
    for (y = ay; y < ty; y += step) {
        int f = (y - ay) * 256 / dy;
        int cx = ax + (tx - ax) * f / 256, hw = hw0 + (hw1 - hw0) * f / 256;
        cspan(cx - hw, cx + hw + 1, y, step, c);
    }
}

void concert_draw_stage(const StageView *v, const StageLayout *L)
{
    StagePal p;
    Wall W;
    const ConcertGenre *g = &concert_genres[v->genre];
    int i;
    stage_pal(v->stage, &p);
    draw_backdrop(v, L, &p);
    W.x = L->wall_x; W.y = L->wall_y; W.w = L->wall_w; W.h = L->wall_h;
    /* the wall frame */
    ui_round_rect(W.x - 3, W.y - 3, W.w + 6, W.h + 6, 3, PAL_RAMP(RAMP_STEEL, 1));
    plat_rect(W.x - 2, W.y - 3, W.w + 4, 1, PAL_RAMP(RAMP_STEEL, 4));
    draw_show(v, &W);
    /* scanline sheen */
    for (i = W.y + 1; i < W.y + W.h; i += 3) cspan(W.x, W.x + W.w, i, 1, PAL_RAMP(RAMP_STONE, 1));
    draw_speakers(v, L);
    draw_floor(v, L, &p);
    draw_truss(v, L);
    /* beams sweep with the music */
    if (v->lights > 6) {
        int n = v->sec == SEC_CHORUS1 || v->sec == SEC_CHORUS2 || v->encore ? 6 : 4;
        for (i = 0; i < 6; i++) {
            int lx, ly, tx, c;
            if (n == 4 && (i == 1 || i == 4)) continue;
            lamp_xy(L, i, &lx, &ly);
            tx = L->w / 2 + ui_wave((int)v->t * (i & 1 ? 3 : 2) + i * 70, 260, L->w / 2 - 10);
            c = i % 3 == 0 ? PAL_PAINT(g->c0, 2) : i % 3 == 1 ? PAL_PAINT(g->c1, 2) : PAL_PAINT(g->c2, 2);
            beam(lx, ly + 1, tx, L->stage_front, 1, L->big ? 14 : 10, (uint8_t)c);
        }
    }
}

/* ====================================================================== */
/* performers                                                               */
static const DoomoResident *res_of(int id) { return town_res(id); }

static void chibi(int id, int x, int feet, int scale, int dir, int flags, uint8_t c)
{
    const DoomoImage *img;
    if (id < 0 || !res_of(id)) return;
    img = bodyspr_image(id, dir, 0);
    if (!img) return;
    plat_image(x - img->w * scale / 2, feet - img->h * scale, img, scale, flags, c);
}

static void mic_stand(int x, int feet, int top, uint8_t c)
{
    plat_line(x, feet, x, top + 4, c);
    plat_rect(x - 4, feet, 9, 1, c);
    plat_rect(x - 1, top, 3, 4, PAL_RAMP(RAMP_STEEL, 5));
    plat_rect(x - 1, top, 3, 1, PAL_WHITE);
}

static void guitar(int x, int y, uint8_t body, int strum, int bass)
{
    uint8_t d = ui_darker(body), neck = PAL_RAMP(RAMP_WOOD, 3);
    int len = bass ? 18 : 14;
    plat_line(x + 3, y - 1, x + 3 + len, y - 6 - strum, neck);
    plat_line(x + 3, y, x + 3 + len, y - 5 - strum, neck);
    plat_rect(x + 2 + len, y - 8 - strum, 4, 3, PAL_RAMP(RAMP_WOOD, 1));
    ui_fill_ellipse(x - 3, y + 1, 6, 5, d);
    ui_fill_ellipse(x - 3, y, 5, 4, body);
    ui_fill_circle(x + 2, y - 1, 3, body);
    plat_rect(x - 4, y - 1, 3, 3, d);
    plat_rect(x - 6, y - 2, 2, 1, ui_lighter(body));
}

static void drum_kit(int x, int feet, int scale, const StageView *v, uint8_t c)
{
    int r = 7 * scale, by = feet - r;
    uint8_t d = ui_darker(c);
    int crash = v->kick > 2 ? 1 : 0;
    /* cymbals */
    plat_rect(x - 12 * scale, feet - 20 * scale - crash, 9 * scale, 2, PAL_PAINT(PAINT_HONEY, 2));
    plat_line(x - 8 * scale, feet - 20 * scale, x - 8 * scale, feet, PAL_RAMP(RAMP_STEEL, 4));
    plat_rect(x + 4 * scale, feet - 17 * scale + (v->snare > 2), 8 * scale, 2, PAL_PAINT(PAINT_HONEY, 2));
    plat_line(x + 8 * scale, feet - 17 * scale, x + 8 * scale, feet, PAL_RAMP(RAMP_STEEL, 4));
    /* toms */
    ui_fill_ellipse(x - 6 * scale, feet - 13 * scale, 4 * scale, 2 * scale, d);
    ui_fill_ellipse(x + 6 * scale, feet - 13 * scale, 4 * scale, 2 * scale, d);
    /* bass drum with a heart */
    ui_fill_circle(x, by, r, d);
    ui_fill_circle(x, by, r - 2, PAL_PAINT(PAINT_WHITE, 2));
    ui_heart_flat(x, by + 1, 2 + scale + (v->kick > 2), c);
}

static void keyboard(int x, int y, int w, uint8_t c, uint32_t t)
{
    int k;
    plat_rect(x - w / 2 - 1, y - 1, w + 2, 7, ui_darker(c));
    plat_rect(x - w / 2, y + 1, w, 4, PAL_PAINT(PAINT_WHITE, 2));
    for (k = 0; k < w; k += 3) {
        plat_rect(x - w / 2 + k, y + 1, 1, 4, PAL_RAMP(RAMP_STONE, 3));
        if ((k / 3) % 7 != 2 && (k / 3) % 7 != 6) plat_rect(x - w / 2 + k + 2, y + 1, 1, 2, PAL_BLACK);
    }
    plat_rect(x - w / 2 + (int)((t / 4) % (uint32_t)w), y + 1, 2, 2, PAL_PAINT(PAINT_LEMON, 2));
    plat_line(x - w / 2 + 2, y + 5, x - w / 2 - 2, y + 14, PAL_RAMP(RAMP_STEEL, 3));
    plat_line(x + w / 2 - 2, y + 5, x + w / 2 + 2, y + 14, PAL_RAMP(RAMP_STEEL, 3));
}

static int bob_of(const StageView *v, int i, int amp)
{
    int b = v->beat < 3 ? amp - v->beat : 0;
    return b > 0 ? -b * ((i & 1) ? 1 : 1) : 0;
}

/* choir formation: position of member i (feet y, centre x, scale) */
static void choir_pos(const StageView *v, const StageLayout *L, int i, int *x, int *feet, int *scale)
{
    const ConcertCast *c = v->cast;
    int n = c->n, sc = L->big ? 2 : 1, cw = 16 * sc + 2, rh = 12 * sc;
    int row = 0, col = 0, rown = n, k, center = L->w / 2;
    switch (c->layout) {
    case CHOIR_PYRAMID: {
        /* rows of 1, 3, 5, 7, 9 from the top */
        int r, used = 0, sizes[6] = { 1, 3, 5, 7, 9, 0 };
        int idx = n - 1 - i;                     /* the lead at the front */
        for (r = 0; r < 5; r++) {
            int s = DOOMO_MIN(sizes[r], n - used);
            if (idx < used + s || r == 4) { row = r; col = idx - used; rown = s; break; }
            used += s;
        }
        *scale = sc;
        *x = center + (col - (rown - 1) / 2) * cw - ((rown - 1) & 1) * cw / 2;
        if (row == 0) *x = center;
        *feet = L->floor_y + 4 - (4 - row) * rh * 2 / 3 - (n <= 9 ? -rh : 0);
        return; }
    case CHOIR_SECTIONS: {
        /* four columns: S A T B, side by side */
        int s = c->section[i], before = 0, cnt = 0, j;
        for (j = 0; j < n; j++) {
            if (c->section[j] == s) { if (j < i) before++; cnt++; }
        }
        k = (cnt + 2) / 3;                       /* columns inside a section */
        if (k < 1) k = 1;
        row = before / k;
        col = before % k;
        *scale = sc;
        *x = L->w * (2 * s + 1) / 8 + (col - (k - 1) / 2) * cw * 3 / 4 - ((k - 1) & 1) * cw * 3 / 8;
        *feet = L->floor_y + 4 - row * rh;
        return; }
    default: break;
    }
    /* risers / semicircle: front row 9, then 8, then 8 (the lead front centre) */
    {
        int fr = n > 16 ? 9 : n > 8 ? (n + 1) / 2 : n, idx = i;
        int rows_left = n;
        if (idx < fr) { row = 0; col = idx; rown = fr; }
        else {
            rows_left = n - fr;
            idx -= fr;
            {
                int r2 = rows_left > 8 ? 8 : rows_left;
                if (idx < r2) { row = 1; col = idx; rown = r2; }
                else { row = 2; col = idx - r2; rown = rows_left - r2; }
            }
        }
        /* the lead (i = 0) in the middle of the front row */
        {
            int mid = rown / 2, order = col == 0 ? mid : (col <= mid ? col - 1 : col);
            col = order;
        }
        *scale = sc;
        *x = center + (col * 2 - (rown - 1)) * cw / 2;
        *feet = L->floor_y + 6 - row * rh;
        if (c->layout == CHOIR_SEMICIRCLE) {
            int d = *x - center;
            *feet -= (d * d) / (L->big ? 1400 : 700) - 4;
        }
    }
}

static void draw_risers(const StageView *v, const StageLayout *L)
{
    int k, sc = L->big ? 2 : 1, rh = 12 * sc;
    uint8_t c = PAL_RAMP(RAMP_WOOD, 2), cl = PAL_RAMP(RAMP_WOOD, 4);
    if (v->cast->layout == CHOIR_SECTIONS || v->cast->layout == CHOIR_PYRAMID) {
        int rows = v->cast->layout == CHOIR_PYRAMID ? 5 : 3;
        for (k = rows - 1; k >= 1; k--) {
            int w = v->cast->layout == CHOIR_PYRAMID ? (11 - 2 * k) * (16 * sc + 2) : L->w * 7 / 8;
            int y = v->cast->layout == CHOIR_PYRAMID ? L->floor_y + 4 - k * rh * 2 / 3 : L->floor_y + 4 - k * rh;
            plat_rect(L->w / 2 - w / 2, y, w, L->floor_y + 6 - y, c);
            plat_rect(L->w / 2 - w / 2, y, w, 1, cl);
        }
        return;
    }
    for (k = 2; k >= 1; k--) {
        int w = (k == 2 ? 8 : 8) * (16 * sc + 2) + 8, y = L->floor_y + 6 - k * rh;
        plat_rect(L->w / 2 - w / 2, y, w, L->floor_y + 6 - y, c);
        plat_rect(L->w / 2 - w / 2, y, w, 1, cl);
    }
}

void concert_draw_performers(const StageView *v, const StageLayout *L)
{
    const ConcertCast *c = v->cast;
    const ConcertGenre *g = &concert_genres[v->genre];
    int i, lead = c->ids[0], cx = L->w / 2, feet = L->floor_y + (L->big ? 12 : 9);
    uint8_t steel = PAL_RAMP(RAMP_STEEL, 3);
    if (c->mode == CMODE_CHOIR) {
        draw_risers(v, L);
        /* back rows first: draw from the last member to the first */
        for (i = c->n - 1; i >= 0; i--) {
            int x, f, sc, sing = (v->singing >> i) & 1;
            choir_pos(v, L, i, &x, &f, &sc);
            if (sing && v->lights > 4)
                ui_fill_ellipse(x, f, 7 * sc, 2, PAL_PAINT(g->c1, 2));
            f += sing ? bob_of(v, i, 3) : ui_wave((int)v->t + i * 13, 90, 1);
            chibi(c->ids[i], x, f, sc, 0, 0, 0);
            /* a little songbook */
            plat_rect(x - 3 * sc, f - 9 * sc, 6 * sc, 4 * sc, (i & 1) ? PAL_PAINT(PAINT_CHERRY, 1)
                                                                       : PAL_PAINT(PAINT_NAVY, 1));
            plat_rect(x - 3 * sc, f - 9 * sc, 6 * sc, 1, PAL_PAINT(PAINT_WHITE, 2));
            if (sing && ((v->t / 6 + (uint32_t)i) % 5) == 0)
                concert_note_glyph(x + 4 * sc, f - 26 * sc - (int)(v->t % 6), PAL_PAINT(g->c1, 2));
        }
        if (c->layout == CHOIR_SECTIONS) {
            /* little signs on the stage lip: who sings which part */
            static const char *const sec_names[SECTION_COUNT] = { "Soprano", "Alto", "Tenor", "Bass" };
            for (i = 0; i < SECTION_COUNT; i++) {
                int sx = L->w * (2 * i + 1) / 8, tw = ui_text_width(sec_names[i]), on = 0, j;
                for (j = 0; j < c->n; j++) if (c->section[j] == i && ((v->singing >> j) & 1)) on = 1;
                ui_round_rect(sx - tw / 2 - 3, L->floor_y + (L->big ? 9 : 6), tw + 6, 10, 4,
                              on ? PAL_PAINT(g->c1, 1) : PAL_RAMP(RAMP_STONE, 1));
                ui_text(sx - tw / 2, L->floor_y + (L->big ? 10 : 7), sec_names[i],
                        on ? PAL_WHITE : PAL_RAMP(RAMP_STONE, 5));
            }
        }
        if (v->singing & 1) mic_stand(cx + 10, feet, feet - (L->big ? 30 : 20), steel);
        return;
    }
    if (c->mode == CMODE_BAND) {
        int sc = 2, strum = v->onset > 3 ? 1 : 0;
        /* drums and keys on risers at the back */
        int dx = L->w * 30 / 100, kx = L->w * 70 / 100, rf = L->floor_y + 1;
        plat_rect(dx - 22, rf, 44, 5, PAL_RAMP(RAMP_STONE, 1));
        plat_rect(kx - 22, rf, 44, 5, PAL_RAMP(RAMP_STONE, 1));
        if (c->band[BAND_DRUMS] != CONCERT_NOBODY) {
            int hit = v->kick > 2 || v->snare > 2;
            chibi(c->band[BAND_DRUMS], dx, rf + bob_of(v, 1, 2), sc, 0, 0, 0);
            plat_line(dx - 6, rf - 20, dx - 12, rf - 26 + (hit ? 6 : 0), PAL_RAMP(RAMP_WOOD, 5));
            plat_line(dx + 6, rf - 20, dx + 12, rf - 26 + (v->snare > 2 ? 6 : 0), PAL_RAMP(RAMP_WOOD, 5));
            drum_kit(dx, rf + 4, 1, v, PAL_PAINT(g->c0, 1));
        }
        if (c->band[BAND_KEYS] != CONCERT_NOBODY) {
            chibi(c->band[BAND_KEYS], kx, rf + bob_of(v, 2, 1), sc, 0, 0, 0);
            keyboard(kx, rf - 20, 26, PAL_PAINT(g->c2, 1), v->t);
        }
        if (c->band[BAND_GUITAR] != CONCERT_NOBODY) {
            int gx = L->w * 15 / 100, f = feet + bob_of(v, 3, 2);
            chibi(c->band[BAND_GUITAR], gx, f, sc, 0, 0, 0);
            guitar(gx - 2, f - 18, PAL_PAINT(g->c1, 1), strum, 0);
        }
        if (c->band[BAND_BASS] != CONCERT_NOBODY) {
            int bx = L->w * 85 / 100, f = feet + bob_of(v, 4, 2);
            chibi(c->band[BAND_BASS], bx, f, sc, 2, 0, 0);
            guitar(bx - 2, f - 18, PAL_PAINT(g->c0, 1), v->kick > 2, 1);
        }
    }
    /* the lead singer: the big figure behind a mic stand */
    {
        const DoomoResident *r = res_of(lead);
        if (r) {
            const DoomoImage *img = figure_image(&r->look, r->status,
                                                 v->sec == SEC_END ? EXPR_HAPPY : EXPR_TALK);
            int b = bob_of(v, 0, 2), sx = cx;
            if (v->lights > 4)
                ui_fill_ellipse(sx, feet, L->big ? 26 : 20, L->big ? 4 : 3, PAL_PAINT(g->c1, 2));
            if (img) {
                plat_image(sx - img->w / 2, feet - img->h + b, img, 1, IMG_SHADOW, 0);
                mic_stand(sx + 15, feet + 1, feet - img->h * 50 / 100 + b, steel);
            }
        }
    }
}

/* ====================================================================== */
/* crowd                                                                    */
void concert_draw_crowd(const StageView *v, const StageLayout *L)
{
    const ConcertGenre *g = &concert_genres[v->genre];
    int i, n = v->ncrowd, front = L->big ? 7 : 6, back = n - front, hype = v->hype;
    int amp = g->crowd == CROWD_JUMP || g->crowd == CROWD_MOSH ? 4 : g->crowd == CROWD_HEADBANG ? 3 : 2;
    uint8_t sil = v->stage == STAGE_HELLFIRE ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_RAMP(RAMP_VAPE, 0);
    if (back < 0) back = 0;
    if (amp > 1 && hype < 30) amp--;
    /* big screens: a middle row of silhouettes further back in the house */
    if (L->big && back > 2) {
        for (i = 0; i < back; i++) {
            int id = v->crowd[front + (i + 3) % back];
            int x = (i * 2 + 2) * L->w / (back * 2) - 6, f = L->stage_front + 52 + (i & 1) * 3;
            int b = g->crowd == CROWD_SWAY || g->crowd == CROWD_WAVE ? 0 : (((v->beat + i * 3) & 7) < 2 ? amp - 1 : 0);
            if (g->crowd == CROWD_SWAY || g->crowd == CROWD_WAVE) x += ui_wave((int)v->t + i * 11, 64, 3);
            if (id >= 0) chibi(id, x, f - b, 1, 3, IMG_SILHOUETTE, PAL_BLACK);
        }
    }
    /* back rows: silhouettes against the stage light */
    for (i = 0; i < back; i++) {
        int id = v->crowd[front + i];
        int x = (i * 2 + 1) * L->w / (back * 2) + (i & 1) * 3;
        int f = L->stage_front + (L->big ? 26 : 19) + (i % 3) * 2;
        int b = 0;
        switch (g->crowd) {
        case CROWD_SWAY: case CROWD_WAVE: x += ui_wave((int)v->t + i * 9, 60, 3); break;
        default: b = ((v->beat + i) & 7) < 2 ? amp - 1 : 0; break;
        }
        if (id >= 0) chibi(id, x, f - b, 1, 3, IMG_SILHOUETTE, sil);
        else {
            ui_fill_circle(x, f - 18 - b, 6, sil);
            plat_rect(x - 6, f - 12 - b, 13, 12, sil);
        }
        /* glow sticks */
        if (hype > 20 && (i % 2) == 0) {
            int sw = ui_wave((int)v->t * 2 + i * 15, 30, 4);
            uint8_t gc = PAL_PAINT((i & 2) ? g->c1 : g->c2, 2);
            plat_line(x + 4, f - 14 - b, x + 6 + sw, f - 24 - b, ui_darker(gc));
            plat_rect(x + 5 + sw, f - 26 - b, 2, 3, gc);
        }
    }
    /* front row: real chibis from behind, big, heads and shoulders */
    for (i = 0; i < front && i < n; i++) {
        int id = v->crowd[i];
        int x = (i * 2 + 1) * L->w / (front * 2) + ((i * 7) % 5) - 2;
        int f = L->h + (L->big ? 22 : 18), b = 0;
        int arms = v->cheer > 0 || (hype > 70 && ((v->t / 15 + (uint32_t)i) % 4 == 0));
        switch (g->crowd) {
        case CROWD_JUMP: b = (((int)v->t / 2 + i * 3) % 8) < 3 ? amp + 1 : 0; break;
        case CROWD_HEADBANG: b = v->beat < 3 ? amp : 0; break;
        case CROWD_MOSH: b = (int)(hash2(v->t / 3, (uint32_t)i) % (uint32_t)(amp + 2)); x += (int)(hash2(v->t / 5, (uint32_t)i) % 5) - 2; break;
        case CROWD_SWAY: case CROWD_WAVE: x += ui_wave((int)v->t + i * 20, 70, 4); b = 1; break;
        default: b = v->beat < 2 ? amp : 0; break;
        }
        if (arms) {
            /* paws in the air! */
            uint8_t pc = PAL_RAMP(RAMP_PEACH, 5);
            plat_line(x - 9, f - 30 - b, x - 13, f - 46 - b, pc);
            plat_line(x + 9, f - 30 - b, x + 13, f - 46 - b, pc);
            ui_fill_circle(x - 13, f - 47 - b, 2, pc);
            ui_fill_circle(x + 13, f - 47 - b, 2, pc);
        }
        if (id >= 0) chibi(id, x, f - b, 2, 3, 0, 0);
        else {
            ui_fill_circle(x, f - 34 - b, 12, PAL_RAMP(RAMP_STONE, 1));
            plat_rect(x - 14, f - 24 - b, 29, 24, PAL_RAMP(RAMP_STONE, 1));
        }
        /* fans of this genre hold up hearts */
        if (id >= 0 && res_of(id) && res_of(id)->like_music == v->genre && hype > 40 &&
            ((v->t / 20 + (uint32_t)i) & 3) == 0)
            ui_heart(x + 10, f - 58 - b - (int)(v->t % 20) / 4, 4, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    }
}

/* ====================================================================== */
/* fx                                                                       */
void concert_draw_fx(const StageView *v, const StageLayout *L)
{
    int i;
    /* pyro columns at the stage front */
    if (v->pyro > 0) {
        int h = (L->big ? 70 : 50) * DOOMO_MIN(v->pyro, 12) / 12;
        for (i = 0; i < 4; i++) {
            int x = L->w * (2 * i + 1) / 8, k;
            for (k = 0; k < 6; k++) {
                int r = 6 - k + (int)(hash2(v->t, (uint32_t)(i * 8 + k)) % 3u);
                ui_fill_circle(x + ui_wave((int)v->t * 3 + k * 20 + i * 9, 20, 2),
                               L->stage_front - k * h / 6, r, PAL_RAMP(RAMP_LAVA, 2 + k));
            }
            ui_sparkle(x, L->stage_front - h - 4, (int)(v->t + (uint32_t)i * 4) & 15, PAL_PAINT(PAINT_LEMON, 2));
        }
    }
    /* smoke on the floor */
    if (v->lights > 0) {
        for (i = 0; i < 6; i++) {
            int x = (i * L->w / 5 + (int)v->t / 2) % (L->w + 40) - 20;
            cspan(x - 18, x + 18, L->stage_front - 5 + (i & 1) * 2, 3, PAL_RAMP(RAMP_STONE, 6));
        }
    }
    if (v->confetti > 0)
        concert_confetti(0, 0, L->w, L->stage_front, v->t, L->big ? 60 : 40, 1);
}

void concert_draw_curtain(const StageView *v, const StageLayout *L)
{
    int half = L->w / 2, cw = half * v->curtain / 256, i, x;
    uint8_t c0 = PAL_PAINT(PAINT_CHERRY, 0), c1 = PAL_PAINT(PAINT_CHERRY, 1), c2 = PAL_PAINT(PAINT_CHERRY, 2);
    if (cw <= 0) return;
    for (i = 0; i < 2; i++) {
        int x0 = i ? L->w - cw : 0;
        plat_rect(x0, 0, cw, L->h, c1);
        for (x = 0; x < cw; x += 10) {
            int fx = i ? L->w - cw + x : cw - x - 10;
            plat_rect(fx + 2, 0, 3, L->h, c0);
            plat_rect(fx + 6, 0, 1, L->h, c2);
        }
        plat_rect(i ? L->w - cw : cw - 2, 0, 2, L->h, c0);
    }
    /* valance + gold fringe */
    for (x = 0; x < L->w; x += 20) ui_fill_ellipse(x + 10, 4, 11, 7, c1);
    for (x = 0; x < L->w; x += 3) plat_rect(x, 10 + ((x / 3) & 1), 2, 2, PAL_PAINT(PAINT_HONEY, 2));
}

/* ====================================================================== */
/* karaoke                                                                  */
void concert_draw_lyric(const StageLayout *L, const char *line, int lit, int ball, int edgy, uint32_t t)
{
    int len = (int)strlen(line), w = len * FONT_ADVANCE, pw = w + 16, x, y = L->lyric_y;
    uint8_t bg = edgy ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_RAMP(RAMP_VAPE, 0);
    uint8_t ol = edgy ? PAL_RAMP(RAMP_BLOOD, 4) : PAL_PAINT(PAINT_BUBBLEGUM, 1);
    char buf[CONCERT_LINE_LEN];
    if (len <= 0) return;
    if (pw > L->w - 4) pw = L->w - 4;
    x = (L->w - pw) / 2;
    ui_round_rect(x - 1, y - 1, pw + 2, 17, 7, ol);
    ui_round_rect(x, y, pw, 15, 6, bg);
    plat_rect(x + 6, y + 1, pw - 12, 1, ui_lighter(bg));
    x += 8;
    /* the unsung part in soft white, the sung part in lemon */
    plat_text(x, y + 4, line, PAL_GRAY_L);
    if (lit > 0) {
        if (lit > len) lit = len;
        memcpy(buf, line, (size_t)lit);
        buf[lit] = 0;
        plat_text(x, y + 4, buf, PAL_PAINT(PAINT_LEMON, 2));
    }
    if (ball >= 0 && ball < len) {
        int bx = x + ball * FONT_ADVANCE + 2, by = y - 3 - (int)((t % 8) < 4 ? (t % 4) : 3 - (t % 4));
        ui_heart_flat(bx, by, 3, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    }
}
