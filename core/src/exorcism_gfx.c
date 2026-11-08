/* exorcism_gfx.c - St. Cuddles church and the ritual's props (owner:
 * life). Everything is drawn with plat_* / ui_* primitives from a few
 * colour keys that slide from sugary pastel (dread 0) to a blood-red
 * nightmare (dread 15): the same church, rotting as the ritual goes on.
 */
#include "exorcism_int.h"

extern void (*ur_tap)(int, int, int, int, uint8_t);   /* ui_raster.c: every rect of a shape */

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
/* The drape's width at row y: w less red[k] in band k = y * 16 / h (tied back in the middle,
 * flaring below): w - (5 - |k - 8|) * w / 12 for 4 < k < 11, else w. Band k is rows
 * [ceil(k h / 16), ceil((k + 1) h / 16)), so a curtain is walked band by band: no divide per
 * row (E1: ~2,400 software divides a frame on the GBA) and no per-row work for the folds. */
static void drape_bands(int w, int h, int red[17], int rs[18])
{
    int k;
    for (k = 0; k < 17; k++)
        red[k] = k > 4 && k < 11 ? (5 - (k - 8 < 0 ? 8 - k : k - 8)) * w / 12 : 0;
    for (k = 0; k <= 17; k++) rs[k] = k <= 16 ? (k * h + 15) / 16 : h + 1;
    if (rs[16] > h) rs[16] = h;                    /* band 16 starts at row h */
}

/* The curtain in two parts (SP7): CUR_UPPER is what never moves (the folds above h / 2 and the
 * highlights wholly above h / 2 - 1), CUR_LOWER the swaying rest (the folds from h / 2 down, the
 * highlights from h / 2 - 1, then the rope). Drawn upper then lower they are the old curtain,
 * pixel for pixel: the lower part's folds and highlights sit in rows the upper part never
 * touches or are drawn over it as before (highlights after folds, the rope last). */
enum { CUR_UPPER = 1, CUR_LOWER = 2 };
static void curtain(int x0, int w, int h, int left, int dread, int sway, int part)
{
    /* velvet drapes: vertical folds, drawn as a few tall strips per fold
     * (cheap on the GBA) with a slow sway on the lower half */
    uint8_t c0 = dread < 8 ? PAL_PAINT(PAINT_ROSE, 0) : PAL_PAINT(PAINT_BLOOD, 0);
    uint8_t c1 = dread < 8 ? PAL_PAINT(PAINT_ROSE, 1) : PAL_PAINT(PAINT_BLOOD, 1);
    uint8_t c2 = dread < 8 ? PAL_PAINT(PAINT_ROSE, 2) : PAL_PAINT(PAINT_CHERRY, 0);
    int fx, k, red[17], rs[18];
    if (h <= 0) return;
    drape_bands(w, h, red, rs);
    for (fx = 0; fx < w; fx += 4) {
        /* runs of rows where this fold is covered (everything except the tied band): a run
         * starts and ends only at band edges, since a band's rows share one width */
        int start = -1, y;
        for (k = 0; k <= 16; k++) {
            int y0 = rs[k], vis = k < 16 && w - red[k] > fx;
            if (rs[k + 1] <= y0 && k < 16) continue;          /* an empty band */
            if (vis && start < 0) start = y0;
            if ((!vis || k == 15) && start >= 0) {
                int len, half, xs = left ? x0 + fx : x0 + w - fx - 4, f = ((fx >> 2) & 1);
                y = vis ? h : y0;                             /* the run ends at h or here */
                len = y - start;
                half = start < h / 2 ? DOOMO_MIN(len, h / 2 - start) : 0;
                if (half > 0 && (part & CUR_UPPER)) plat_rect(xs, start, 4, half, f ? c1 : c0);
                if (len - half > 0 && (part & CUR_LOWER))
                    plat_rect(xs + sway, start + half, 4, len - half, f ? c1 : c0);
                start = -1;
            }
        }
    }
    /* a soft highlight down the inner edge, every second row */
    for (k = 0; k < 16; k++) {
        int y0 = (rs[k] + 1) & ~1, ww = w - red[k];
        for (; y0 < rs[k + 1] && y0 < h; y0 += 2) {
            if (!(part & (y0 + 2 > h / 2 ? CUR_LOWER : CUR_UPPER))) continue;
            plat_rect(left ? x0 + ww - 1 + (y0 > h / 2 ? sway : 0) : x0 + w - ww + (y0 > h / 2 ? sway : 0),
                      y0, 1, 2, c2);
        }
    }
    /* golden tie-back rope */
    if (part & CUR_LOWER) {
        int ty = h * 8 / 16, tx = x0 + w / 2;
        ui_fill_ellipse(tx, ty, 3, 2, PAL_UI_GOLD);
        plat_rect(tx - 1, ty - 1, 2, 1, PAL_RAMP(RAMP_CANDY, 7));
    }
}

static void rose_window(int cx, int cy, int r, int dread)
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
    /* centre: a heart... that becomes an eye when things go wrong (rose_heart) */
    ui_fill_circle(cx, cy, r * 34 / 100 + 1, lead);
    ui_fill_circle(cx, cy, r * 30 / 100, exo_col(EC_GLASS_LEMON, dread));
}

/* The heart (or eye) in the rose window's lemon centre: it beats with t, so it is drawn over
 * the backdrop (nothing the church draws later reaches the centre: the beams start below it,
 * the valance and the vignette end above it). */
static void rose_heart(int cx, int cy, int r, int dread, uint32_t t)
{
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

/* the beams fail (are not drawn, nor their motes) on some ticks of a nightmare */
static int beams_off(int dread, uint32_t t) { return dread >= 13 && (t / 3) % 4 == 0; }

static void light_beams(int cx, int top, int bottom, int dread)
{
    uint8_t beam = exo_col(EC_BEAM, dread);
    int b, k;
    for (b = -1; b <= 1; b++) {
        int x0 = cx + b * 8, x1 = cx + b * (bottom - top) * 3 / 4;
        for (k = -3; k <= 3; k += 2) {
            if (dread >= 8 && (k & 2)) continue;           /* thinner, dimmer */
            plat_line(x0 + k, top, x1 + k * 3, bottom, beam);
        }
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

/* The church's geometry for this screen (floor_y is the scene's, H * 62 / 100). */
typedef struct {
    int W, H, big, fy, cw, ch, wr, wy, btop, bbot, ax, ay, aw, ah;
} ChurchGeo;
static void church_geo(ChurchGeo *g)
{
    g->W = plat_screen_w();
    g->H = plat_screen_h();
    g->big = g->H >= 200;
    g->fy = g->H * 62 / 100;
    g->cw = g->big ? 26 : 18;
    g->ch = g->fy + (g->big ? 30 : 20);            /* curtain height */
    g->wr = g->big ? 30 : 21;
    g->wy = g->wr + (g->big ? 8 : 5);
    g->btop = g->wy + g->wr / 2;                     /* light beams */
    g->bbot = g->fy + (g->H - g->fy) / 2;
    g->aw = g->big ? 92 : 64;                        /* altar */
    g->ah = g->big ? 22 : 14;
    g->ax = g->W / 2 - g->aw / 2;
    g->ay = g->fy - g->ah + 4;
}

static void altar(const ChurchGeo *g, int dread)
{
    int x, ax = g->ax, ay = g->ay, aw = g->aw, ah = g->ah;
    ui_round_rect(ax - 1, ay - 1, aw + 2, ah + 2, 2, exo_col(EC_WOOD_LO, dread));
    plat_rect(ax, ay, aw, ah, PAL_PAINT(PAINT_WHITE, dread < 10 ? 1 : 0));
    plat_rect(ax, ay, aw, 2, PAL_PAINT(PAINT_WHITE, 2));
    plat_rect(ax, ay + ah - 3, aw, 1, PAL_UI_GOLD);
    for (x = ax + 3; x < ax + aw - 3; x += 6) plat_rect(x, ay + ah - 2, 3, 2, PAL_UI_GOLD);
    ui_heart(g->W / 2, ay + ah / 2, 2, dread < 10 ? PAL_PAINT(PAINT_ROSE, 1) : PAL_UI_RED);
}

/* ---- the backdrop key (SP7) ----------------------------------------------------------
 * Everything of the church that does not move every few ticks: the dread level (0..15), the
 * drips' step (dread >= 9: their length cycles every 8 ticks over 5 steps) and the failing
 * beams (dread >= 13: off one tick in four of every 12). The heart in the rose window, the
 * dust motes and the swaying lower curtains are drawn over it (exo_church_live). */
#define EXO_KEY_DRIP_SHIFT 4
#define EXO_KEY_BEAMS_OFF  0x80u
uint32_t exo_church_key(int dread, uint32_t t)
{
    uint32_t d = (uint32_t)DOOMO_CLAMP(dread, 0, 15), k = d;
    if (d >= 9) k |= ((t / 8) % 5) << EXO_KEY_DRIP_SHIFT;
    if (beams_off((int)d, t)) k |= EXO_KEY_BEAMS_OFF;
    return k;
}

int exo_church_keys(uint32_t *out, int cap)
{
    int n = 0, d, p, b;
    for (d = 0; d < 16; d++)
        for (p = 0; p < (d >= 9 ? 5 : 1); p++)
            for (b = 0; b < (d >= 13 ? 2 : 1); b++) {
                if (n < cap) out[n] = (uint32_t)d | (uint32_t)p << EXO_KEY_DRIP_SHIFT | (b ? EXO_KEY_BEAMS_OFF : 0u);
                n++;
            }
    return n;
}

void exo_church_backdrop(uint32_t key)
{
    ChurchGeo g;
    int dread = (int)(key & 15), drip = (int)((key >> EXO_KEY_DRIP_SHIFT) & 7);
    int x, y, W, H, floor_y, big;
    uint8_t whi = exo_col(EC_WALL_HI, dread), wlo = exo_col(EC_WALL_LO, dread);
    uint8_t stripe = exo_col(EC_STRIPE, dread);
    church_geo(&g);
    W = g.W; H = g.H; floor_y = g.fy; big = g.big;

    /* wallpaper: soft vertical stripes with a tiny heart / drip motif */
    plat_rect(0, 0, W, floor_y, wlo);
    for (x = 4; x < W; x += 14) {
        plat_rect(x, 0, 6, floor_y, whi);
        plat_rect(x + 2, 0, 2, floor_y, stripe);
    }
    for (y = 10; y < floor_y - 16; y += 22)
        for (x = 11 + ((y / 22) & 1) * 7; x < W; x += 28) {
            if (dread < 9) ui_heart_flat(x, y, 2, stripe);
            else { plat_rect(x, y, 2, 3 + (x * 7 + drip) % 5, PAL_UI_RED_D); }
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
        int k;
        uint8_t fa = exo_col(EC_FLOOR_A, dread), fb = exo_col(EC_FLOOR_B, dread);
        plat_rect(0, floor_y, W, H - floor_y, fa);
        for (k = 0, y = floor_y + 3; y < H; k++) {
            int step = 3 + k * 2;
            plat_rect(0, y, W, 1, fb);
            y += step;
        }
        for (k = -10; k <= 10; k++)
            plat_line(W / 2 + k * 6, floor_y, W / 2 + k * (W / 6), H + (H - floor_y), fb);
        /* carpet: half = 10 + (y - floor_y) * spread / depth, stepped per
         * row instead of divided (E1) */
        {
            int spread = big ? 30 : 20, depth = DOOMO_MAX(1, H - floor_y), q = 0, r = 0;
            uint8_t cc = exo_col(EC_CARPET, dread), ch = exo_col(EC_CARPET_HI, dread);
            for (y = floor_y; y < H; y++) {
                int half = 10 + q;
                plat_rect(W / 2 - half, y, half * 2, 1, cc);
                plat_rect(W / 2 - half + 2, y, 1, 1, ch);
                plat_rect(W / 2 + half - 3, y, 1, 1, ch);
                r += spread;
                while (r >= depth) { r -= depth; q++; }
            }
        }
        plat_rect(0, floor_y, W, 1, exo_col(EC_WOOD_LO, dread));
    }

    rose_window(W / 2, g.wy, g.wr, dread);
    if (!(key & EXO_KEY_BEAMS_OFF)) light_beams(W / 2, g.btop, g.bbot, dread);

    /* altar under the portrait */
    altar(&g, dread);

    /* foreground pews and the velvet curtains framing it all */
    {
        int pw = big ? 70 : 48, ph = big ? 24 : 16;
        pew(-pw / 4, H - ph + 4, pw, ph, dread);
        pew(W - pw + pw / 4, H - ph + 4, pw, ph, dread);
    }
    curtain(0, g.cw, g.ch, 1, dread, 0, CUR_UPPER);
    curtain(W - g.cw, g.cw, g.ch, 0, dread, 0, CUR_UPPER);
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

/* ---- the live parts, drawn over the backdrop -------------------------------------------
 * Each lies where nothing the church draws after it in the old order reaches (the rose's
 * centre: above the beams, below the valance and the vignette; the lower curtains: below the
 * valance, above the bottom vignette, beside the altar and the pews; the motes: in the beams,
 * where only the altar is drawn later), so drawing them last gives the old picture. The one
 * overlap, a mote on the altar, is resolved by not drawing the mote's pixels the altar covers. */

/* the altar's outline as rects (its outer round rect: everything the altar draws lies in it) */
static void (*ex_tap_prev)(int, int, int, int, uint8_t);
#define ALTAR_RECTS 12
static struct { int16_t w, h, n; int16_t r[ALTAR_RECTS][4]; } alt;
static void altar_tap(int x, int y, int w, int h, uint8_t c)
{
    (void)c;
    if (alt.n < ALTAR_RECTS) {
        alt.r[alt.n][0] = (int16_t)x; alt.r[alt.n][1] = (int16_t)y;
        alt.r[alt.n][2] = (int16_t)(x + w); alt.r[alt.n][3] = (int16_t)(y + h);
    }
    alt.n++;
}
static void altar_shape(const ChurchGeo *g)
{
    if (alt.w == g->W && alt.h == g->H) return;
    alt.n = 0;
    ex_tap_prev = ur_tap;
    ur_tap = altar_tap;
    ui_round_rect(g->ax - 1, g->ay - 1, g->aw + 2, g->ah + 2, 2, PAL_WHITE);
    ur_tap = ex_tap_prev;
    if (alt.n > ALTAR_RECTS) alt.n = -1;            /* (never: a round rect is a few bands) */
    alt.w = (int16_t)g->W;
    alt.h = (int16_t)g->H;
}
static int under_altar(int x, int y)
{
    int i;
    if (alt.n < 0) return 0;
    for (i = 0; i < alt.n; i++)
        if (x >= alt.r[i][0] && x < alt.r[i][2] && y >= alt.r[i][1] && y < alt.r[i][3]) return 1;
    return 0;
}
/* a mote's rect, less the pixels the altar covers */
static void mote_rect(const ChurchGeo *g, int x, int y, int w, int h, uint8_t c)
{
    int i, j;
    if (x + w <= g->ax - 1 || x >= g->ax + g->aw + 1 || y + h <= g->ay - 1 || y >= g->ay + g->ah + 1) {
        plat_rect(x, y, w, h, c);
        return;
    }
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (!under_altar(x + i, y + j)) plat_rect(x + i, y + j, 1, 1, c);
}

/* the dust mote k at tick t: its centre and sparkle size (0 = none); its box is
 * (x - s, y - s, 2 s + 1, 2 s + 1) */
static int mote_at(const ChurchGeo *g, int k, uint32_t t, int *x, int *y)
{
    int span = g->bbot - g->btop;
    int u = (int)((t / 2 + (uint32_t)k * 37) % (uint32_t)span);
    *x = g->W / 2 + ui_wave((int)t + k * 40, 160, span / 3) + (k - 3) * 6;
    *y = g->btop + u;
    return ui_sparkle_size((int)((t / 3 + (uint32_t)k * 5) & 15));
}

/* ui_sparkle(cx, cy, phase, c) through mote_rect */
static void mote(const ChurchGeo *g, int cx, int cy, int s, uint8_t c)
{
    if (s == 0 || plat_draw_skipped()) return;
    mote_rect(g, cx - s, cy, 2 * s + 1, 1, c);
    mote_rect(g, cx, cy - s, 1, 2 * s + 1, c);
    if (s >= 2) {
        mote_rect(g, cx - 1, cy - 1, 3, 3, c);
        mote_rect(g, cx, cy, 1, 1, PAL_WHITE);
    }
    if (s >= 3) {
        mote_rect(g, cx - 2, cy - 2, 1, 1, c);
        mote_rect(g, cx + 2, cy + 2, 1, 1, c);
        mote_rect(g, cx - 2, cy + 2, 1, 1, c);
        mote_rect(g, cx + 2, cy - 2, 1, 1, c);
    }
}

int exo_church_part(int part, int dread, uint32_t t, int16_t box[4], uint32_t *key)
{
    ChurchGeo g;
    int x = 0, y = 0, s;
    church_geo(&g);
    box[2] = box[3] = 0;
    *key = 0;
    switch (part) {
    case EXO_PART_HEART: {
        int r = g.wr * 30 / 100 + 3;          /* the lemon centre holds the heart / the eye */
        box[0] = (int16_t)(g.W / 2 - r); box[1] = (int16_t)(g.wy - r);
        box[2] = box[3] = (int16_t)(2 * r + 1);
        *key = dread < 11 ? 0x100u + (uint32_t)(ui_wave((int)t, 60, 1) + 1)
                          : 0x200u + (uint32_t)(ui_wave((int)t, 70, 2) + 2);
        *key ^= (uint32_t)dread << 12;
        return 1;
    }
    case EXO_PART_CURTAIN_L: case EXO_PART_CURTAIN_R:
        /* the lower folds sway a pixel either way; the rope starts 2 rows above h / 2; the last
         * highlight (2 rows tall) may end a row below h */
        box[0] = (int16_t)(part == EXO_PART_CURTAIN_L ? 0 : g.W - g.cw - 4);
        box[1] = (int16_t)(g.ch / 2 - 3);
        box[2] = (int16_t)(g.cw + 4);
        box[3] = (int16_t)(g.ch + 2 - (g.ch / 2 - 3));
        *key = (uint32_t)(ui_wave((int)t, 90, 1) + 1) | (uint32_t)(dread < 8) << 4 | 0x40u;
        return 1;
    default:
        if (part < EXO_PART_MOTE0 || part >= EXO_PART_MOTE0 + EXO_MOTES) return 0;
        if (beams_off(dread, t)) return 1;
        s = mote_at(&g, part - EXO_PART_MOTE0, t, &x, &y);
        if (!s) return 1;
        box[0] = (int16_t)(x - s); box[1] = (int16_t)(y - s);
        box[2] = box[3] = (int16_t)(2 * s + 1);
        *key = (uint32_t)(x & 0x3FF) | (uint32_t)(y & 0x3FF) << 10 | (uint32_t)s << 20 |
               (uint32_t)(dread < 10) << 24 | 0x80000000u;
        return 1;
    }
}

void exo_church_part_draw(int part, int dread, uint32_t t)
{
    ChurchGeo g;
    int x, y, s, sway;
    church_geo(&g);
    switch (part) {
    case EXO_PART_HEART: rose_heart(g.W / 2, g.wy, g.wr, dread, t); break;
    case EXO_PART_CURTAIN_L: case EXO_PART_CURTAIN_R:
        sway = ui_wave((int)t, 90, 1);
        if (part == EXO_PART_CURTAIN_L) curtain(0, g.cw, g.ch, 1, dread, sway, CUR_LOWER);
        else curtain(g.W - g.cw, g.cw, g.ch, 0, dread, sway, CUR_LOWER);
        break;
    default:
        if (part < EXO_PART_MOTE0 || part >= EXO_PART_MOTE0 + EXO_MOTES || beams_off(dread, t)) break;
        s = mote_at(&g, part - EXO_PART_MOTE0, t, &x, &y);
        altar_shape(&g);
        mote(&g, x, y, s, dread < 10 ? PAL_WHITE : PAL_UI_RED);
        break;
    }
}

void exo_church(int dread, uint32_t t, int floor_y)
{
    int p;
    (void)floor_y;                                  /* (always H * 62 / 100: church_geo) */
    exo_church_backdrop(exo_church_key(dread, t));
    for (p = 0; p < EXO_PARTS; p++) exo_church_part_draw(p, dread, t);
}

const DoomoBackdrop exo_church_family = { DOOMO_BACKDROP_EXO_CHURCH, exo_church_backdrop, exo_church_keys };

/* ---------------------------------------------------------------------- */
/* Father Fuzz                                                              */
const DoomoImage *exo_fuzz_image(int expr)
{
    const DoomoResident *f = town_res(ROSTER_ID_FATHER_FUZZ);
    DoomoLook look;
    if (f && f->status != RES_EMPTY && f->status != RES_DEAD &&
        strncmp(f->name, roster_name(ROSTER_ID_FATHER_FUZZ), DOOMO_NAME_LEN) == 0)
        res_get_look(f, &look);
    else if (roster_premade(ROSTER_ID_FATHER_FUZZ))
        res_get_look(roster_premade(ROSTER_ID_FATHER_FUZZ), &look);
    else
        return NULL;
    return figure_image(&look, RES_ALIVE, expr);
}

void exo_fuzz_draw(int cx, int foot_y, uint32_t t, const DoomoImage *img)
{
    int k;
    if (!img) return;
    /* soft contact shadow */
    for (k = -1; k <= 1; k++) {
        int half = 14 - (k < 0 ? -k : k) * 5;
        plat_shade_rect(cx - half, foot_y + k, half * 2, 1, 10);
    }
    plat_image(cx - img->w / 2, foot_y - img->h + ui_wave((int)t, 48, 1), img, 1, 0, 0);
}

void exo_fuzz(int cx, int foot_y, uint32_t t, int expr)
{
    exo_fuzz_draw(cx, foot_y, t, exo_fuzz_image(expr));
}

/* ---------------------------------------------------------------------- */
/* the host's portrait                                                      */
/* SP7: the frame's layers paint over one another (aura, drop shadow, three round rects, the
 * inner panel), and each later one covers the middle of the one before. On a fill-bound screen
 * (the GBA, and native so the tests run it) every layer leaves out the hole the next one is sure
 * to cover: the same pixels with a fifth of the writes (core/tests/test_stage.c compares). A
 * GPU platform (PS1, Dreamcast) pays per primitive, not per pixel, and draws them whole. */
#if defined(__mips__) || defined(__sh__)
#define HOST_HOLES 0
#else
#define HOST_HOLES 1
#endif
#if HOST_HOLES
static int16_t hole[4];                      /* [x0, x1) x [y0, y1) */
static void hole_tap(int x, int y, int w, int h, uint8_t c)
{
    int x1 = x + w, y1 = y + h, a, b;
    if (x >= hole[2] || x1 <= hole[0] || y >= hole[3] || y1 <= hole[1]) { plat_rect(x, y, w, h, c); return; }
    if (y < hole[1]) plat_rect(x, y, w, hole[1] - y, c);                     /* above */
    if (y1 > hole[3]) plat_rect(x, hole[3], w, y1 - hole[3], c);             /* below */
    a = DOOMO_MAX(y, hole[1]);
    b = DOOMO_MIN(y1, hole[3]);
    if (x < hole[0]) plat_rect(x, a, hole[0] - x, b - a, c);                 /* left  */
    if (x1 > hole[2]) plat_rect(hole[2], a, x1 - hole[2], b - a, c);         /* right */
}
static void with_hole(int x0, int y0, int x1, int y1)
{
    hole[0] = (int16_t)x0; hole[1] = (int16_t)y0; hole[2] = (int16_t)x1; hole[3] = (int16_t)y1;
    ur_tap = x0 < x1 && y0 < y1 ? hole_tap : 0;
}
#define HOLE(x0, y0, x1, y1) with_hole(x0, y0, x1, y1)
#define NO_HOLE() (ur_tap = 0)
#else
#define HOLE(x0, y0, x1, y1) ((void)0)
#define NO_HOLE() ((void)0)
#endif

/* exorcism_int.h: the same holes for the scene's own shapes (the prayer lane) */
void exo_hole(int x0, int y0, int x1, int y1) { HOLE(x0, y0, x1, y1); (void)x0; (void)y0; (void)x1; (void)y1; }
void exo_hole_off(void) { NO_HOLE(); }

void exo_shade_hole(int x, int y, int w, int h, int level, int hx0, int hy0, int hx1, int hy1)
{
#if HOST_HOLES
    int x1 = x + w, y1 = y + h;
    hx0 = DOOMO_MAX(hx0, x); hy0 = DOOMO_MAX(hy0, y);
    hx1 = DOOMO_MIN(hx1, x1); hy1 = DOOMO_MIN(hy1, y1);
    if (hx0 < hx1 && hy0 < hy1) {
        plat_shade_rect(x, y, w, hy0 - y, level);                    /* above */
        plat_shade_rect(x, hy1, w, y1 - hy1, level);                 /* below */
        plat_shade_rect(x, hy0, hx0 - x, hy1 - hy0, level);          /* left  */
        plat_shade_rect(hx1, hy0, x1 - hx1, hy1 - hy0, level);       /* right */
        return;
    }
#else
    (void)hx0; (void)hy0; (void)hx1; (void)hy1;
#endif
    plat_shade_rect(x, y, w, h, level);
}

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
    /* a breathing purple aura when the demon stirs (under the frame: its middle is the
     * frame's, whose outer round rect fills every row of [y + 4, y + h - 4) edge to edge) */
    if (dread > 3) {
        int pulse = ui_wave((int)t, 24, 2);
        HOLE(x, y + 4, x + w, y + h - 4);
        ui_fill_ellipse(cx + dx, cy + dy, w / 2 + 5 + pulse, h / 2 + 4 + pulse,
                        dread > 10 ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_VAPE, 2));
        ui_fill_ellipse(cx + dx, cy + dy, w / 2 + 2 + pulse / 2, h / 2 + 2 + pulse / 2,
                        dread > 10 ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_RAMP(RAMP_VAPE, 3));
        NO_HOLE();
    }
#if HOST_HOLES
    /* the drop shadow (x + 3, y + 3, w, h): only the band the frame does not paint over (the
     * right 7 columns and the bottom 7 rows of it hold every pixel its round corners leave) */
    plat_shade_rect(x + w - 4, y + 3, 7, h, 10);
    plat_shade_rect(x + 3, y + h - 4, w - 7, 7, 10);
#else
    plat_shade_rect(x + 3, y + 3, w, h, 10);
#endif
    HOLE(x + 1, y + 4, x + w - 1, y + h - 4);           /* the next round rect's full rows */
    ui_round_rect(x, y, w, h, 4, PAL_RAMP(RAMP_CANDY, 1));
    HOLE(x + 3, y + 5, x + w - 3, y + h - 5);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 3, PAL_RAMP(RAMP_CANDY, 4));
    NO_HOLE();
    plat_rect(x + 4, y + 2, w - 8, 1, PAL_RAMP(RAMP_CANDY, 6));
    HOLE(x + 4, y + 4, x + w - 4, y + h - 4);           /* the inner panel */
    ui_round_rect(x + 3, y + 3, w - 6, h - 6, 2, PAL_RAMP(RAMP_CANDY, 2));
    NO_HOLE();
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
