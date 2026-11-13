/* fp_island_gfx.c - the illustrated free play island (owner: freeplay).
 *
 * HaHaland is a squircle island in a pastel sea. A river runs down its
 * middle with the Wall on the east bank: Hillvile (west) is a candy
 * meadow of pink apartment towers, cafes and flower beds; Hellvile (east)
 * is spooky-cute plum ground with horned towers, lava pools and a
 * graveyard with a friendly ghost. Hellvile Park sits in the middle with
 * a fountain on the bridge.
 *
 * Everything is drawn every frame from spans (no stored bitmap: the
 * island must fit 240x160 and 320x240 and costs no RAM): row spans that
 * repeat on the next row merge into tall rects and the river + Wall are
 * one set of rects per straight run, so the whole map is ~2,100 plat_rect
 * calls at 240x160 (land ~570, buildings ~790). Day / dusk / night
 * re-colour every span through the tint maps; lit windows and lamps are
 * drawn untinted so they glow at night. Nothing is ever still: waves, foam, a turning ferris
 * wheel, smoking chimneys, bubbling lava, a floating ghost, tiny citizens
 * strolling the roads.
 */
#include "fp_island.h"
#include "doomo/plat_ext.h"          /* SP3: plat_scene_scratch */

/* ====================================================================== */
/* geometry                                                                 */
static FpIslandGeo *G;                       /* in the island's scene scratch */
#define M       (G->map)
#define coast_l (G->coast_l)
#define coast_r (G->coast_r)
#define river_c (G->river_c)
static int g_tint, g_t;

static void build_geometry(void);
static void use_geo(void)
{
    G = fp_island_geo();
    if (G->geo_w != fp_sw() || G->geo_h != fp_sh() || M.h == 0) build_geometry();
}

const FpMap *fp_map(void) { use_geo(); return &M; }

int fp_mx(int u) { use_geo(); return M.x + u * M.w / 256; }
int fp_my(int u) { use_geo(); return M.y + u * M.h / 256; }

static int isqrt_i(int v) { return v <= 0 ? 0 : (int)fx_isqrt((uint32_t)v); }

static void build_geometry(void)
{
    int row, sw = fp_sw(), sh = fp_sh();
    M.x = 0;
    M.y = 16;
    M.w = sw;
    M.h = sh - 16 - 14;
    if (M.h > FP_GEO_ROWS - 2) M.h = FP_GEO_ROWS - 2;
    G->geo_w = (int16_t)sw;
    G->geo_h = (int16_t)sh;
    for (row = 0; row < M.h; row++) {
        /* superellipse |x/a|^4 + |y/b|^4 <= 1 with a little coastal wobble */
        int cy = M.h / 2, b = M.h / 2 - 3, a = M.w / 2 - 5;
        int dy = row - cy, v, hw, wob_l, wob_r;
        int q = dy * 256 / b;                       /* -256..256 */
        q = q < 0 ? -q : q;
        if (q > 256) { coast_l[row] = coast_r[row] = -1; river_c[row] = -1; continue; }
        v = (q * q / 256) * (q * q / 256) / 256;    /* (y/b)^4 in 1/256 */
        hw = a * isqrt_i(isqrt_i((256 - v) * 256) * 256) / 256;
        /* slow waves only: the flanks stay straight for a few rows at a
         * time, so their spans merge into tall rects (draw_land) */
        wob_l = ui_wave(row * 2 + 11, 140, 5) + ui_wave(row, 97, 2);
        wob_r = ui_wave(row * 2 + 70, 150, 5) + ui_wave(row + 40, 83, 2);
        coast_l[row] = (int16_t)(M.x + M.w / 2 - hw + wob_l);
        coast_r[row] = (int16_t)(M.x + M.w / 2 + hw + wob_r);
        if (coast_r[row] - coast_l[row] < 4) { coast_l[row] = coast_r[row] = -1; }
        river_c[row] = (int16_t)(M.x + M.w / 2 + ui_wave(row + 20, 140, 4));
    }
}

void fp_island_geometry(void) { use_geo(); }

/* Is the screen point on land, and on which side (0 hill, 1 hell)? */
int fp_island_side_at(int x, int y)
{
    int row;
    use_geo();
    row = y - M.y;
    if (row < 0 || row >= M.h || coast_l[row] < 0) return -1;
    if (x < coast_l[row] || x > coast_r[row]) return -1;
    return x > river_c[row] + 5 ? 1 : 0;
}

/* ====================================================================== */
/* colour + primitives                                                      */
static uint8_t tc(uint8_t c) { return g_tint ? doomo_tintmap[g_tint][c] : c; }

/* (integration, perf) The buildings' static parts are pre-rendered once
 * into sprites in the island's scene scratch (see "building sprites"
 * below) and drawn with one plat_image each; only their animated parts
 * are drawn live. g_pass selects which parts a b_*() function draws (both
 * when not cached, so the drawing order is exactly the original one); a
 * static part is never drawn after an animated part it overlaps. While a
 * sprite is rendered, put() writes into it instead of the screen (or only
 * measures its bounding box when g_off == G_MEASURE). */
enum { PASS_STATIC = 1, PASS_ANIM = 2, PASS_ALL = 3 };
static int g_pass = PASS_ALL;
#define ST (g_pass & PASS_STATIC)
#define DY (g_pass & PASS_ANIM)
static uint8_t g_measure_dummy;
#define G_MEASURE (&g_measure_dummy)
static uint8_t *g_off;                       /* NULL: draw to the screen */
static int g_ox, g_oy, g_ow, g_oh, g_bx0, g_by0, g_bx1, g_by1;

/* (perf) An opaque window drawn over the map later in the frame (the
 * island's place menus): screen draws that lie wholly inside it are
 * skipped, as the window paints over them anyway. Same pixels. */
static int g_occ, g_occ_x0, g_occ_y0, g_occ_x1, g_occ_y1;

void fp_island_occluder(int x, int y, int w, int h)
{
    g_occ = w > 0 && h > 0;
    g_occ_x0 = x; g_occ_y0 = y; g_occ_x1 = x + w; g_occ_y1 = y + h;
}

static int hidden(int x, int y, int w, int h)
{
    return g_occ && !g_off && x >= g_occ_x0 && y >= g_occ_y0 && x + w <= g_occ_x1 &&
           y + h <= g_occ_y1;
}

/* (perf, docs/ISLAND_SPEED.md I1) The land (cliff, meadow, plum ground, shores, river and Wall) depends only on
 * the map size and the tint: its rects and checkers are recorded once and replayed in the same order (same
 * pixels, minus the geometry maths and divisions of ~530 rects a frame). */
/* PS1: its 2 MB RAM is full to the byte (the disc build was 2,368 B over the stack reserve with these lists in BSS:
 * 8,960 B here and 3,200 B of deco variants below). (SP3, P-I1, docs/ISLAND_SPEED.md I12) So on the PS1 they have no
 * BSS: they live in the scene scratch the platform lends the island (plat_scene_scratch: the overlay slot's unused
 * tail while the island group is loaded), bound by lr_bind() before every map draw; without it (a platform that
 * lends none) every list has cap 0, each record overflows and the phase is drawn live, as before I1..I3. The native
 * runner keeps them in BSS (it can be told to lend scratch: the PS1 path under test).
 * (CR5a) The GBA's EWRAM is full too (the creator's LOOK pool needs the room): there the lists are in the arena
 * region the hub does not use (gba/src/bigbuf.c plat_scene_scratch, GBA_BIG_ISL), 13,720 B of .bss less, and the
 * caps stay the old BSS caps, so the recorded rects and the frames are the same. */
#define LR_MAX 640
#define LR_PATCH 320
#define LR_ROAD 160
#if defined(DOOMO_PLATFORM_PS1) || defined(DOOMO_PLATFORM_GBA)
#define LR_BSS(n) 0
#else
#define LR_BSS(n) (n)
#endif
typedef struct { int16_t x, y; uint8_t w, h, c, ck; } LRect;
typedef struct { LRect *r; int cap, n, over; int32_t key; } LList;
static LRect lr_land_r[LR_BSS(LR_MAX) + 1], lr_patch_r[LR_BSS(LR_PATCH) + 1], lr_road_r[LR_BSS(LR_ROAD) + 1];
static LList L_land = { lr_land_r, LR_BSS(LR_MAX), 0, 0, -1 };
static LList L_patch = { lr_patch_r, LR_BSS(LR_PATCH), 0, 0, -1 };   /* (I2) patches and their ember phase */
static LList L_road = { lr_road_r, LR_BSS(LR_ROAD), 0, 0, -1 };     /* (I2) roads */
/* (I3b, the governor) The animated phases that are plain rects: the sea's wave glints, the walkers, the weather. They
 * read g_q, the animation clock in steps of ANIM_Q ticks, so a list recorded for a step is replayed on every draw
 * inside it (the GBA draws every 3rd tick, so about every second draw replays). Not pixel-exact against the old
 * ROM: these phases now move in steps of ANIM_Q ticks. */
#define ANIM_Q 6
/* Sized to the worst case (glints 18 x 2 rects, walkers 14 x 4, weather 40 x 1 or 16 x 2); on the PS1 in the scene
 * scratch like the lists above (without it: cap 0, every record overflows and the phase is drawn live, still on the
 * 6-tick clock). */
#define AL_GLINT 40
#define AL_WALK  60
#define AL_WX    48
static LRect lr_glint_r[LR_BSS(AL_GLINT) + 1], lr_walk_r[LR_BSS(AL_WALK) + 1], lr_wx_r[LR_BSS(AL_WX) + 1];
static LList L_glint = { lr_glint_r, LR_BSS(AL_GLINT), 0, 0, -1 };
static LList L_walk = { lr_walk_r, LR_BSS(AL_WALK), 0, 0, -1 };
static LList L_wx = { lr_wx_r, LR_BSS(AL_WX), 0, 0, -1 };
static int g_q;                              /* g_t rounded down to a multiple of ANIM_Q */
static int g_wx;                             /* the weather being drawn (body of L_wx) */
static int g_walk_n;                         /* the walker count (body of L_walk) */
static LList *lr_cur;                        /* the list being recorded, or NULL */
static int lr_tapped;
static int lr_hits;                          /* replays so far (test_island_replay.c) */
static int lr_overs;                         /* (SP3) recordings that overflowed their list so far (test hook) */
static int lr_quiet;                         /* (I3) recording a deco variant: record only, draw nothing */
extern void (*ur_tap)(int, int, int, int, uint8_t);    /* ui_raster.c: every rect of a shape */
static void lr_add(int x, int y, int w, int h, uint8_t c, int ck)
{
    LList *L = lr_cur;
    if (L->n < L->cap && w > 0 && h > 0 && w < 256 && h < 256) {
        LRect *r = &L->r[L->n++];
        r->x = (int16_t)x; r->y = (int16_t)y; r->w = (uint8_t)w; r->h = (uint8_t)h; r->c = c; r->ck = (uint8_t)ck;
    } else L->over = 1;
}

static void lr_tap(int x, int y, int w, int h, uint8_t c)
{
    lr_tapped++;
    lr_add(x, y, w, h, c, 0);
    if (!lr_quiet && !hidden(x, y, w, h)) plat_rect(x, y, w, h, c);
}

/* (I3) the GBA draws a run of recorded rects in one ARM call (video_loops.s); weak, so other platforms and the
 * native runner link without it and replay one rect at a time as before. Returns 0 when it drew nothing. */
extern void plat_rect_run(const void *list, int n) __attribute__((weak));
extern int gba_nodraw __attribute__((weak));       /* 0: drawing to Mode 4 (the only case the run handles) */

/* run body() live, or replay its recorded rects when its key is unchanged (same calls, same order) */
static void lr_run(LList *L, int32_t key, void (*body)(void))
{
    if (g_off) { body(); return; }
    if (key == L->key && !L->over) {
        int i = 0;
        lr_hits++;
        while (i < L->n) {
            const LRect *r = &L->r[i];
            if (!g_occ && !r->ck && plat_rect_run && !gba_nodraw) {   /* a run of plain rects, nothing hiding them */
                int j = i + 1;
                while (j < L->n && !L->r[j].ck) j++;
                plat_rect_run(r, j - i);
                i = j;
                continue;
            }
            if (!hidden(r->x, r->y, r->w, r->h)) {
                if (r->ck) plat_checker(r->x, r->y, r->w, r->h, r->c);
                else plat_rect(r->x, r->y, r->w, r->h, r->c);
            }
            i++;
        }
    } else {
        int occ = g_occ;
        g_occ = 0;                           /* record everything: a menu window may be gone when this replays */
        L->n = 0; L->over = 0; lr_cur = L;
        body();
        lr_cur = 0;
        g_occ = occ;
        L->key = L->over ? -1 : key;
        lr_overs += L->over;
    }
}

static void put(int x, int y, int w, int h, uint8_t c)
{
    int j;
    if (!g_off) {
        if (lr_cur) lr_add(x, y, w, h, c, 0);
        if (!lr_quiet && !hidden(x, y, w, h)) plat_rect(x, y, w, h, c);
        return;
    }
    if (g_off == G_MEASURE) {
        g_bx0 = DOOMO_MIN(g_bx0, x); g_by0 = DOOMO_MIN(g_by0, y);
        g_bx1 = DOOMO_MAX(g_bx1, x + w); g_by1 = DOOMO_MAX(g_by1, y + h);
        return;
    }
    x -= g_ox; y -= g_oy;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > g_ow) w = g_ow - x;
    if (y + h > g_oh) h = g_oh - y;
    if (w <= 0 || h <= 0) return;
    if (!c) c = PAL_BLACK;                   /* 0 is transparent in images */
    for (j = 0; j < h; j++) memset(g_off + (y + j) * g_ow + x, c, (size_t)w);
}
static void R(int x, int y, int w, int h, uint8_t c) { if (w > 0 && h > 0) put(x, y, w, h, tc(c)); }
static void RG(int x, int y, int w, int h, uint8_t c) { if (w > 0 && h > 0) put(x, y, w, h, c); }
static void C(int cx, int cy, int r, uint8_t c)
{
    if (hidden(cx - r - 1, cy - r - 1, 2 * r + 3, 2 * r + 3)) return;
    if (lr_cur && !g_off) {                  /* (I3) recorded like E(): its rects go through lr_tap */
        int n0 = lr_tapped;
        ur_tap = lr_tap;
        ui_fill_circle(cx, cy, r, tc(c));
        ur_tap = 0;
        if (lr_tapped == n0) lr_cur->over = 1;
    } else ui_fill_circle(cx, cy, r, tc(c));
}
static void E(int cx, int cy, int rx, int ry, uint8_t c)
{
    if (hidden(cx - rx - 1, cy - ry - 1, 2 * rx + 3, 2 * ry + 3)) return;
    if (lr_cur && !g_off) {
        int n0 = lr_tapped;
        ur_tap = lr_tap;
        ui_fill_ellipse(cx, cy, rx, ry, tc(c));
        ur_tap = 0;
        if (lr_tapped == n0) lr_cur->over = 1;   /* drawn some other way (a cached shape): not recordable */
    } else ui_fill_ellipse(cx, cy, rx, ry, tc(c));
}
static void CK(int x, int y, int w, int h, uint8_t c)
{
    if (!g_off && lr_cur) lr_add(x, y, w, h, c, 1);
    if (!lr_quiet && !hidden(x, y, w, h)) plat_checker(x, y, w, h, c);
}

/* a shaded box with a coloured outline (dark shade of its own colour) */
static void box(int x, int y, int w, int h, uint8_t dark, uint8_t base, uint8_t light)
{
    R(x, y, w, h, ui_darker(dark));
    R(x + 1, y + 1, w - 2, h - 2, base);
    R(x + 1, y + 1, w - 2, 1, light);
    R(x + 1, y + 2, 1, h - 3, light);
    R(x + w - 3, y + 2, 2, h - 3, dark);
}

/* triangular roof: apex at (cx, y), h rows tall, w wide at the base */
static void roof(int cx, int y, int w, int h, uint8_t dark, uint8_t base, uint8_t light)
{
    int i;
    for (i = 0; i < h; i++) {
        int rw = 1 + (w - 1) * (i + 1) / h, x0 = cx - rw / 2;
        R(x0 - 1, y + i, rw + 2, 1, ui_darker(dark));
        R(x0, y + i, rw, 1, base);
        if (rw > 4) {
            R(x0 + 1, y + i, rw / 3, 1, light);
            R(x0 + rw - rw / 4, y + i, rw / 4, 1, dark);
        }
    }
}

/* window: lit ones glow untinted at night */
static void win(int x, int y, int w, int h, int lit, uint8_t frame)
{
    R(x - 1, y - 1, w + 2, h + 2, frame);
    if (lit) {
        RG(x, y, w, h, PAL_RAMP(RAMP_CANDY, 6));
        RG(x, y + h - 1, w, 1, PAL_RAMP(RAMP_CANDY, 4));
    } else {
        R(x, y, w, h, PAL_RAMP(RAMP_SKY, 6));
        R(x, y, 1, 1, PAL_WHITE);
        R(x + w - 1, y + h - 1, 1, 1, PAL_RAMP(RAMP_SKY, 4));
    }
}

static void shadow(int cx, int y, int rx)
{
    int i;
    for (i = 0; i < 3; i++) {
        int w = rx * 2 - (i == 1 ? 0 : 4) - (i == 2 ? 4 : 0);
        if (!hidden(cx - w / 2, y - 1 + i, w, 1))
            plat_shade_rect(cx - w / 2, y - 1 + i, w, 1, i == 1 ? 10 : 12);
    }
}

/* golden hour (dawn / dusk): the land keeps its candy colours, the sea
 * blushes peach and pink */
static int golden_hour(void)
{
    int h = town_hour();
    return !town_is_night() && (h >= 18 || h < 7);
}

static int night_lit(int salt)
{
    if (!g_tint) return 0;
    return (doomo_hash32((uint32_t)salt * 2654435761u + (uint32_t)(doomo_town.day)) & 3) != 0;
}

/* ====================================================================== */
/* terrain                                                                  */
#define SAND_HI   PAL_RAMP(RAMP_CANDY, 6)
#define SAND      PAL_RAMP(RAMP_CANDY, 5)
#define SAND_D    PAL_RAMP(RAMP_PEACH, 5)
#define ASH_HI    PAL_RAMP(RAMP_STONE, 4)
#define ASH       PAL_RAMP(RAMP_STONE, 3)
#define CLIFF     PAL_RAMP(RAMP_PEACH, 3)
#define CLIFF_H   PAL_RAMP(RAMP_VAPE, 1)

static uint8_t hill_grass(int row)
{
    int k = row * 6 / DOOMO_MAX(M.h, 1);
    static const uint8_t g[6] = {
        PAL_RAMP(RAMP_GRASS, 7), PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 6),
        PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 5)
    };
    return g[DOOMO_CLAMP(k, 0, 5)];
}

static uint8_t hell_ground(int row)
{
    int k = row * 6 / DOOMO_MAX(M.h, 1);
    static const uint8_t g[6] = {
        PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 4),
        PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 3)
    };
    return g[DOOMO_CLAMP(k, 0, 5)];
}

static void draw_glints_body(void)
{
    int i, t = g_q, night = g_tint == TINT_NIGHT;
    for (i = 0; i < 18; i++) {
        uint32_t h = doomo_hash32((uint32_t)i * 977u + 5u);
        int x = M.x + (int)((h & 0xFFF) % (uint32_t)M.w);
        int y = M.y + 3 + (int)((h >> 12) % (uint32_t)DOOMO_MAX(M.h - 6, 1));
        int ph = (t / 3 + (int)(h >> 20)) % 64;
        x = M.x + (x - M.x + t / 8) % M.w;
        if (fp_island_side_at(x, y) >= 0 || fp_island_side_at(x + 6, y) >= 0) continue;
        if (ph < 40) {
            int w = ph < 20 ? 2 + ph / 5 : 6 - (ph - 20) / 5;
            RG(x, y, w, 1, night ? PAL_RAMP(RAMP_SKY, 4) : golden_hour() ? PAL_RAMP(RAMP_CANDY, 7) : PAL_WHITE);
            if (w > 3) R(x + 1, y + 1, w - 2, 1, golden_hour() ? PAL_RAMP(RAMP_LAVA, 7) : PAL_RAMP(RAMP_SKY, 7));
        }
    }
}

static void draw_sea(void)
{
    int i, t = g_t, night = g_tint == TINT_NIGHT;
    int bands = 4, bh = (M.h + bands - 1) / bands;
    static const uint8_t day[4] = { PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_MINT, 6),
                                    PAL_RAMP(RAMP_MINT, 5), PAL_RAMP(RAMP_SKY, 5) };
    static const uint8_t dusk[4] = { PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_PASTEL_PINK, 6),
                                     PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_VAPE, 5) };
    const uint8_t *band = golden_hour() ? dusk : day;
    for (i = 0; i < bands; i++) {
        int y = M.y + i * bh;
        R(M.x, y, M.w, DOOMO_MIN(bh, M.y + M.h - y), band[i]);
        if (i) CK(M.x, y, M.w, 1, tc(band[i - 1]));
    }
    /* wave glints drifting along (recorded per animation step) */
    lr_run(&L_glint, (int32_t)((g_q / ANIM_Q) & 0x3FFF) | (night << 14) | (golden_hour() << 15), draw_glints_body);
    if (night) {
        for (i = 0; i < 14; i++) {
            uint32_t h = doomo_hash32((uint32_t)i * 7919u + 3u);
            int x = M.x + (int)(h % (uint32_t)M.w), y = M.y + (int)((h >> 10) % (uint32_t)M.h);
            if (fp_island_side_at(x, y) >= 0) continue;
            ui_sparkle(x, y, (t / 4 + i * 5) & 15, PAL_RAMP(RAMP_CANDY, 7));
        }
    }
}

/* The river and the Wall are vertical: they are drawn as tall rects, one
 * per run of rows with the same river centre and ground band, so the land
 * costs ~4 spans a row instead of ~14 (the GBA draws thousands of small
 * spans a frame slowly). */
static void draw_river_run(int row0, int row1)
{
    int y = M.y + row0, h = row1 - row0, rc = river_c[row0];
    int rl = rc - 5, wl = rc + 5, row;
    /* bank + water (rl .. rc + 4) */
    R(rl, y, 1, h, SAND_D);
    R(rl + 1, y, 9, h, PAL_RAMP(RAMP_SKY, 5));
    /* the Wall seen from above: dark outline, lit west face, stony top
     * with a checker texture */
    R(wl, y, 6, h, PAL_RAMP(RAMP_STONE, 1));
    R(wl + 1, y, 1, h, PAL_RAMP(RAMP_STONE, 6));
    R(wl + 2, y, 3, h, PAL_RAMP(RAMP_STONE, 5));
    CK(wl + 2 + (row0 & 1), y, 2, h, tc(PAL_RAMP(RAMP_STONE, 4)));
    /* its shadow on the Hellvile ground (darker shade of each band) */
    for (row = row0; row < row1;) {
        int r1 = row + 1;
        while (r1 < row1 && hell_ground(r1) == hell_ground(row)) r1++;
        R(wl + 6, M.y + row, 2, r1 - row, ui_darker(hell_ground(row)));
        row = r1;
    }
}

/* Row spans that repeat on the next row (same x, width and colour) grow
 * into one tall rect instead of one call a row: the coast is nearly
 * vertical along the island's flanks, so most rows merge. */
#define VM_SLOTS 6
typedef struct { int16_t x, y, w, h; uint8_t c; } VSpan;
static VSpan vm[VM_SLOTS];

static void vm_flush(int i)
{
    if (vm[i].h > 0) R(vm[i].x, vm[i].y, vm[i].w, vm[i].h, vm[i].c);
    vm[i].h = 0;
}

static void vm_span(int i, int x, int y, int w, uint8_t c)
{
    VSpan *v = &vm[i];
    if (w <= 0) return;
    if (v->h > 0 && v->x == x && v->w == w && v->c == c && v->y + v->h == y) { v->h++; return; }
    vm_flush(i);
    v->x = (int16_t)x;
    v->y = (int16_t)y;
    v->w = (int16_t)w;
    v->h = 1;
    v->c = c;
}

static void vm_flush_all(void)
{
    int i;
    for (i = 0; i < VM_SLOTS; i++) vm_flush(i);
}

static void draw_land_tail(void);
static void draw_land_body(void);
static void draw_land(void)
{
    lr_run(&L_land, (int32_t)(M.w | (M.h << 10) | (g_tint << 20)), draw_land_body);
    draw_land_tail();
}

static void draw_land_body(void)
{
    int row, run0 = -1;
    /* the cliff under the south rim: the land drawn 3 rows lower first;
     * only the slivers the land does not cover are drawn */
    for (row = 0; row < M.h; row++) {
        int src = row - 3, l, r, y = M.y + row;
        if (src < 0 || coast_l[src] < 0) continue;
        l = coast_l[src];
        r = coast_r[src];
        if (coast_l[row] < 0) {
            vm_span(0, l, y, river_c[src] - l, CLIFF);
            vm_span(1, river_c[src], y, r - river_c[src], CLIFF_H);
            continue;
        }
        if (coast_l[row] > l) vm_span(0, l, y, coast_l[row] - l, CLIFF);
        if (coast_r[row] < r) vm_span(1, coast_r[row], y, r - coast_r[row], CLIFF_H);
    }
    vm_flush_all();
    /* (integration, perf) the meadow and the plum ground run UNDER the
     * river + Wall, which every row covers from rc - 5 to rc + 13 and
     * which is drawn after them: their inner ends are then one fixed
     * column for the whole island, only the coast moves their outer ends,
     * and they merge into tall rects wherever the coast is straight
     * (before: a new span whenever the coast OR the river moved). Same
     * pixels. */
    {
        int gx = -0x7FFF, hx = 0x7FFF;
        for (row = 0; row < M.h; row++) {
            if (coast_l[row] < 0) continue;
            gx = DOOMO_MAX(gx, river_c[row] - 5);       /* >= every rl            */
            hx = DOOMO_MIN(hx, river_c[row] + 13);      /* <= every wl + 8        */
        }
        for (row = 0; row < M.h; row++) {
            int y = M.y + row, l = coast_l[row], r = coast_r[row], rc = river_c[row];
            if (l < 0) continue;
            /* only where the old spans [l + 3, rc - 5) / [rc + 13, r - 3)
             * were drawn; the extra part lies under the river + Wall */
            if (rc - 5 - (l + 3) > 0) vm_span(1, l + 3, y, DOOMO_MIN(gx, rc + 13) - (l + 3), hill_grass(row));
            else vm_flush(1);
            if (r - 3 - (rc + 13) > 0) vm_span(2, DOOMO_MAX(hx, rc - 5), y, r - 3 - DOOMO_MAX(hx, rc - 5), hell_ground(row));
            else vm_flush(2);
        }
        vm_flush_all();
    }
    for (row = 0; row < M.h; row++) {
        int y = M.y + row, l = coast_l[row], r = coast_r[row], rc = river_c[row];
        if (l < 0) {
            if (run0 >= 0) { draw_river_run(run0, row); run0 = -1; }
            continue;
        }
        /* west: sand rim (+ meadow, above); east: (plum ground +) ash shore */
        vm_span(0, l, y, 3, row < M.h / 2 ? SAND_HI : SAND);
        vm_span(3, r - 3, y, 3, ASH);
        /* runs of the river + Wall */
        if (run0 >= 0 && river_c[run0] != rc) {
            draw_river_run(run0, row);
            run0 = -1;
        }
        if (run0 < 0) run0 = row;
    }
    vm_flush_all();
    if (run0 >= 0) draw_river_run(run0, M.h);
}

/* the parts that move with time (foam, shimmer) and the cheap seams stay live */
static void draw_land_tail(void)
{
    int row, t = g_t;
    /* foam laps at the shore and a shimmer drifts down the river */
    for (row = (9 - (t / 6) % 9) % 9; row < M.h; row += 9) {
        int k;
        for (k = 0; k < 2 && row + k < M.h; k++) {
            if (coast_l[row + k] < 0) continue;
            RG(coast_l[row + k] - 1, M.y + row + k, 1, 1, tc(PAL_WHITE));
            RG(coast_r[row + k], M.y + row + k, 1, 1, tc(PAL_WHITE));
        }
    }
    for (row = (7 - (t / 4) % 7) % 7; row < M.h; row += 7) {
        if (coast_l[row] < 0) continue;
        R(river_c[row] - 3 + (row & 1), M.y + row, 2, 1, PAL_RAMP(RAMP_SKY, 7));
    }
    /* soft seams between the meadow bands */
    for (row = 1; row < 6; row++) {
        int rr0 = row * M.h / 6, l, r, rc;
        if (rr0 >= M.h || coast_l[rr0] < 0) continue;
        l = coast_l[rr0];
        r = coast_r[rr0];
        rc = river_c[rr0];
        CK(l + 3, M.y + rr0, rc - 4 - l - 3, 1, tc(hill_grass(rr0 - 1)));
        CK(rc + 13, M.y + rr0, r - 3 - rc - 13, 1, tc(hell_ground(rr0 - 1)));
    }
}

/* ---- ground patches: clover on the meadow, plum blotches and embers in
 * Hellvile (so neither side is ever a flat single-colour blob) --------- */
typedef struct { uint8_t x, y, rx, ry; } Patch;
static const Patch hill_patches[] = {
    { 40, 90, 14, 8 }, { 84, 140, 12, 7 }, { 20, 172, 10, 6 }, { 70, 40, 12, 6 },
    { 108, 104, 8, 10 }, { 54, 192, 12, 6 }, { 100, 222, 10, 5 }
};
static const Patch hell_patches[] = {
    { 176, 92, 16, 9 }, { 220, 100, 12, 8 }, { 196, 146, 18, 9 }, { 166, 184, 12, 7 },
    { 232, 168, 10, 10 }, { 200, 48, 14, 6 }, { 168, 226, 14, 5 }, { 234, 206, 8, 8 }
};

static void draw_patches_body(void)
{
    int i, t = g_t;
    for (i = 0; i < DOOMO_COUNTOF(hill_patches); i++) {
        const Patch *p = &hill_patches[i];
        int x = fp_mx(p->x), y = fp_my(p->y), rx = p->rx * M.w / 256 + 1, ry = p->ry * M.h / 256 + 1;
        E(x, y, rx, ry, PAL_RAMP(RAMP_GRASS, 7));
        E(x - rx / 4, y - ry / 4, rx / 2, ry / 2, PAL_RAMP(RAMP_MINT, 7));
        R(x - rx / 2, y, 1, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
        R(x + rx / 3, y - 1, 1, 1, PAL_WHITE);
    }
    for (i = 0; i < DOOMO_COUNTOF(hell_patches); i++) {
        const Patch *p = &hell_patches[i];
        int x = fp_mx(p->x), y = fp_my(p->y), rx = p->rx * M.w / 256 + 1, ry = p->ry * M.h / 256 + 1;
        int glow = ((t / 8) + i * 3) % 12;
        E(x, y, rx * 3 / 4, ry * 3 / 4, PAL_RAMP(RAMP_VAPE, 3));
        E(x + rx / 5, y + ry / 5, rx / 2, ry / 2, PAL_RAMP(RAMP_VAPE, 2));
        /* a little ember crack */
        R(x - rx / 2, y, rx / 3, 1, PAL_RAMP(RAMP_LAVA, 3));
        R(x - rx / 2 + rx / 3, y + 1, rx / 4, 1, PAL_RAMP(RAMP_LAVA, 3));
        if (glow < 6) RG(x - rx / 3, y, 2, 1, PAL_RAMP(RAMP_LAVA, 5 + glow / 3));
        /* purple grass tufts */
        R(x + rx - 1, y - ry, 1, 2, PAL_RAMP(RAMP_VAPE, 6));
        R(x + rx + 1, y - ry - 1, 1, 3, PAL_RAMP(RAMP_VAPE, 6));
    }
}

static void draw_patches(void)
{
    lr_run(&L_patch, (int32_t)(M.w | (M.h << 10) | (g_tint << 20) | (((g_t / 8) % 12) << 24)), draw_patches_body);
}

/* ---- roads --------------------------------------------------------------- */
typedef struct { uint8_t x0, y0, x1, y1; } Seg;     /* map units, axis aligned */
static const Seg roads[] = {
    /* Hillvile main street (east-west) and the avenues */
    { 24, 98, 112, 98 }, { 30, 160, 112, 160 }, { 40, 60, 104, 60 },
    { 56, 60, 56, 206 }, { 94, 60, 94, 200 }, { 24, 98, 24, 160 },
    /* to the park bridge */
    { 104, 128, 152, 128 },
    /* Hellvile lanes */
    { 150, 76, 226, 76 }, { 150, 76, 150, 200 }, { 150, 124, 210, 124 },
    { 150, 168, 196, 168 },
    /* the wall road */
    { 108, 40, 108, 128 }
};

static void draw_roads_body(void)
{
    int i, rw = 3;
    for (i = 0; i < DOOMO_COUNTOF(roads); i++) {
        const Seg *s = &roads[i];
        int x0 = fp_mx(s->x0), y0 = fp_my(s->y0), x1 = fp_mx(s->x1), y1 = fp_my(s->y1);
        int hell = s->x0 >= 140;
        uint8_t top = hell ? PAL_RAMP(RAMP_VAPE, 6) : PAL_RAMP(RAMP_PEACH, 7);
        uint8_t mid = hell ? PAL_RAMP(RAMP_VAPE, 5) : PAL_RAMP(RAMP_PEACH, 6);
        uint8_t edge = hell ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_PEACH, 4);
        if (y0 == y1) {
            R(x0, y0 - 1, x1 - x0 + rw, rw + 1, edge);
            R(x0, y0 - 1, x1 - x0 + rw, 1, top);
            R(x0, y0, x1 - x0 + rw, rw - 1, mid);
            if (!hell) {
                int x;
                for (x = x0 + 4; x < x1; x += 9) R(x, y0 + 1, 3, 1, PAL_RAMP(RAMP_PEACH, 7));
            } else {
                int x;
                for (x = x0 + 2; x < x1; x += 4) R(x, y0 + (x & 1), 2, 1, PAL_RAMP(RAMP_VAPE, 4));
            }
        } else {
            R(x0 - 1, y0, rw + 2, y1 - y0 + rw, edge);
            R(x0, y0, rw - 1, y1 - y0 + rw - 1, mid);
            R(x0, y0, 1, y1 - y0 + rw - 1, top);
            if (hell) {
                int y;
                for (y = y0 + 2; y < y1; y += 4) R(x0 + 1, y + (y & 1), 1, 2, PAL_RAMP(RAMP_VAPE, 4));
            }
        }
    }
}

static void draw_roads(void)
{
    lr_run(&L_road, (int32_t)(M.w | (M.h << 10) | (g_tint << 20)), draw_roads_body);
}

/* ---- decorations ----------------------------------------------------------- */
typedef struct { uint8_t x, y, kind; } Deco;
enum { D_TREE, D_TREE_PINK, D_FLOWERS, D_DEAD_TREE, D_MUSHROOM, D_LAMP, D_PUMPKIN,
       D_CANDY, D_SKULL_LAMP, D_BUSH };
static const Deco decos[] = {
    { 18, 84, D_TREE }, { 14, 130, D_TREE_PINK }, { 66, 80, D_FLOWERS }, { 46, 138, D_BUSH },
    { 82, 130, D_TREE }, { 30, 186, D_TREE_PINK }, { 72, 228, D_FLOWERS }, { 112, 184, D_TREE },
    { 46, 40, D_TREE_PINK }, { 88, 86, D_LAMP }, { 40, 108, D_CANDY }, { 116, 92, D_FLOWERS },
    { 68, 178, D_LAMP }, { 20, 108, D_FLOWERS },
    { 168, 96, D_DEAD_TREE }, { 238, 104, D_MUSHROOM }, { 176, 150, D_PUMPKIN }, { 230, 150, D_DEAD_TREE },
    { 214, 216, D_MUSHROOM }, { 140, 166, D_SKULL_LAMP }, { 178, 44, D_PUMPKIN }, { 240, 186, D_DEAD_TREE },
    { 200, 100, D_SKULL_LAMP }, { 134, 222, D_MUSHROOM }, { 236, 128, D_PUMPKIN }, { 166, 228, D_DEAD_TREE }
};

static void deco_trunk(int x, int y)
{
    R(x - 1, y - 5, 3, 5, PAL_RAMP(RAMP_WOOD, 3));
    R(x - 1, y - 5, 1, 5, PAL_RAMP(RAMP_WOOD, 5));
}

static void deco_tree(int x, int y, uint8_t leaf_d, uint8_t leaf, uint8_t leaf_l, int sway)
{
    if (ST) deco_trunk(x, y);                /* (in the park's sprite when cached) */
    C(x + sway, y - 9, 5, ui_darker(leaf_d));
    C(x + sway, y - 9, 4, leaf_d);
    C(x - 1 + sway, y - 10, 3, leaf);
    C(x - 2 + sway, y - 11, 1, leaf_l);
    R(x - 3 + sway, y - 12, 1, 1, PAL_WHITE);
}

static void draw_deco_xy(const Deco *d, int x, int y)
{
    int t = g_t;
    int sway = ui_wave(t + d->x * 9, 90, 1);
    switch (d->kind) {
    case D_TREE:
        deco_tree(x, y, PAL_RAMP(RAMP_GRASS, 3), PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 7), sway);
        break;
    case D_TREE_PINK:
        deco_tree(x, y, PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_PASTEL_PINK, 6),
                  PAL_RAMP(RAMP_PASTEL_PINK, 7), sway);
        break;
    case D_BUSH:
        C(x - 2, y - 2, 3, PAL_RAMP(RAMP_GRASS, 3));
        C(x + 2, y - 2, 3, PAL_RAMP(RAMP_GRASS, 4));
        R(x - 1, y - 4, 1, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
        R(x + 2, y - 3, 1, 1, PAL_RAMP(RAMP_CANDY, 7));
        break;
    case D_FLOWERS: {
        int i;
        for (i = 0; i < 5; i++) {
            int fx = x - 6 + i * 3, fy = y - 2 - (i & 1) * 2 + (ui_wave(t + i * 10, 40, 1) > 0);
            static const uint8_t fc[5] = { PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_CANDY, 7),
                                           PAL_WHITE, PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_PASTEL_PINK, 5) };
            R(fx, fy + 1, 1, 2, PAL_RAMP(RAMP_GRASS, 3));
            R(fx - 1, fy, 3, 1, fc[i]);
            R(fx, fy - 1, 1, 3, fc[i]);
            R(fx, fy, 1, 1, PAL_RAMP(RAMP_CANDY, 4));
        }
        break; }
    case D_DEAD_TREE:
        R(x, y - 10, 2, 10, PAL_RAMP(RAMP_STONE, 1));
        R(x - 3, y - 9, 3, 1, PAL_RAMP(RAMP_STONE, 1));
        R(x - 4, y - 11, 1, 2, PAL_RAMP(RAMP_STONE, 1));
        R(x + 2, y - 7, 3, 1, PAL_RAMP(RAMP_STONE, 1));
        R(x + 4, y - 9, 1, 2, PAL_RAMP(RAMP_STONE, 1));
        R(x, y - 12, 1, 2, PAL_RAMP(RAMP_STONE, 2));
        R(x + 1, y - 10, 1, 9, PAL_RAMP(RAMP_STONE, 3));
        /* a sleepy bat hanging upside down */
        if ((t / 120 + d->x) % 3 == 0) {
            R(x - 4, y - 8, 3, 2, PAL_RAMP(RAMP_VAPE, 1));
            R(x - 5, y - 8, 1, 1, PAL_RAMP(RAMP_VAPE, 1));
            R(x - 1, y - 8, 1, 1, PAL_RAMP(RAMP_VAPE, 1));
        }
        break;
    case D_MUSHROOM: {
        int glow = g_tint == TINT_NIGHT;
        R(x - 1, y - 3, 2, 3, PAL_RAMP(RAMP_BONE, 6));
        E(x, y - 4, 3, 2, PAL_RAMP(RAMP_VAPE, 5));
        if (glow) { RG(x - 1, y - 5, 1, 1, PAL_RAMP(RAMP_MINT, 7)); RG(x + 2, y - 4, 1, 1, PAL_RAMP(RAMP_MINT, 7)); }
        else { R(x - 1, y - 5, 1, 1, PAL_WHITE); R(x + 2, y - 4, 1, 1, PAL_WHITE); }
        break; }
    case D_PUMPKIN:
        E(x, y - 3, 4, 3, PAL_PAINT(PAINT_TANGERINE, 0));
        E(x, y - 3, 3, 3, PAL_PAINT(PAINT_TANGERINE, 1));
        R(x, y - 7, 1, 2, PAL_RAMP(RAMP_GRASS, 2));
        if (g_tint == TINT_NIGHT) {
            RG(x - 2, y - 4, 1, 1, PAL_RAMP(RAMP_CANDY, 7));
            RG(x + 1, y - 4, 1, 1, PAL_RAMP(RAMP_CANDY, 7));
            RG(x - 1, y - 2, 3, 1, PAL_RAMP(RAMP_CANDY, 6));
        } else {
            R(x - 2, y - 4, 1, 1, PAL_PAINT(PAINT_TANGERINE, 2));
        }
        break;
    case D_CANDY:
        R(x, y - 8, 1, 8, PAL_WHITE);
        C(x, y - 10, 3, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        R(x - 1, y - 11, 2, 1, PAL_WHITE);
        R(x + 1, y - 9, 1, 1, PAL_PAINT(PAINT_MINT, 1));
        break;
    case D_LAMP: case D_SKULL_LAMP: {
        int lit = g_tint != 0;
        R(x, y - 9, 1, 9, PAL_RAMP(RAMP_STONE, 1));
        if (d->kind == D_LAMP) {
            R(x - 1, y - 11, 3, 3, PAL_RAMP(RAMP_STONE, 2));
            if (lit) { RG(x, y - 10, 1, 1, PAL_WHITE); RG(x - 2, y - 12, 5, 5, PAL_RAMP(RAMP_CANDY, 6)); RG(x - 1, y - 11, 3, 3, PAL_WHITE); }
            else R(x, y - 10, 1, 1, PAL_RAMP(RAMP_CANDY, 6));
        } else {
            R(x - 1, y - 12, 3, 3, PAL_RAMP(RAMP_BONE, 6));
            R(x - 1, y - 11, 1, 1, lit ? PAL_RAMP(RAMP_MINT, 7) : PAL_RAMP(RAMP_STONE, 1));
            R(x + 1, y - 11, 1, 1, lit ? PAL_RAMP(RAMP_MINT, 7) : PAL_RAMP(RAMP_STONE, 1));
            if (lit) RG(x - 2, y - 13, 1, 1, PAL_RAMP(RAMP_MINT, 6));
        }
        break; }
    default: break;
    }
}

static void draw_deco(const Deco *d)
{
    int x = fp_mx(d->x), y = fp_my(d->y);
    if (hidden(x - 24, y - 40, 48, 48)) return;          /* (well inside any deco) */
    draw_deco_xy(d, x, y);
}

/* (I3) The decos barely animate: a tree's canopy moves one pixel every ~15 ticks, the flowers nod, a bat comes and
 * goes. A deco's picture depends only on its kind, the tint and that small state, never on where it stands, so each
 * (kind, state, tint) is recorded once, at a fixed spot, as a rect list and replayed shifted to every deco of that kind
 * (same rects, same order, so the same pixels). The ten-odd trees all sway on their own phase, which is why one list
 * for all of them (keyed on the whole state) was rebuilt nearly every frame. */
#define DV_POOL 400                         /* rects in the shared pool (3.2 KB; PS1: in the scene scratch, lr_bind) */
#define DV_TAGS 28
#define DV_REC_X 120                        /* where a variant is recorded: far enough from every screen edge */
#define DV_REC_Y 110
typedef struct { uint16_t tag, start; uint8_t n, bad; } DVar;
static LRect dv_pool_bss[LR_BSS(DV_POOL) + 1];
static LRect *dv_pool = dv_pool_bss;
static int dv_cap = LR_BSS(DV_POOL);        /* 0: no room, every variant is "bad" and the deco is drawn live */
static DVar dv_tab[DV_TAGS];
static int dv_ntab, dv_used;
#define DV_TMP 40
#if defined(DOOMO_PLATFORM_GBA)
/* (CR5a) after the lists in the arena region: a pointer and a cap, set by lr_bind (0 without the region) */
static LRect *dv_tmp;
static int dv_tmp_cap;
#else
static LRect dv_tmp[DV_TMP];                /* a variant's rects shifted to one deco's spot (no byte more than before CR5a) */
#define dv_tmp_cap DV_TMP
#endif

static unsigned deco_state(const Deco *d, unsigned flowers)
{
    switch (d->kind) {
    case D_TREE: case D_TREE_PINK: return (unsigned)(ui_wave(g_t + d->x * 9, 90, 1) + 1);
    case D_FLOWERS:                return flowers;
    case D_DEAD_TREE:              return ((g_t / 120 + d->x) % 3 == 0);
    default:                       return 0;
    }
}

static void draw_deco_xy(const Deco *d, int x, int y);

static DVar *dv_get(const Deco *d, unsigned st)
{
    uint16_t tag = (uint16_t)((d->kind << 11) | (g_tint << 8) | st);   /* st < 256, g_tint < 8 */
    DVar *v;
    int i;
    for (i = 0; i < dv_ntab; i++) if (dv_tab[i].tag == tag) return &dv_tab[i];
    if (dv_ntab >= DV_TAGS) { dv_ntab = 0; dv_used = 0; }           /* full: start over (rare) */
    v = &dv_tab[dv_ntab++];
    v->tag = tag; v->bad = 0; v->n = 0; v->start = (uint16_t)dv_used;
    {
        LList L;
        int occ = g_occ;
        L.r = dv_pool + dv_used; L.cap = DOOMO_MIN(dv_cap - dv_used, 255); L.n = 0; L.over = 0; L.key = 0;
        g_occ = 0;
        lr_cur = &L;
        lr_quiet = 1;
        draw_deco_xy(d, DV_REC_X, DV_REC_Y);
        lr_quiet = 0;
        lr_cur = 0;
        g_occ = occ;
        if (L.over) v->bad = 1;
        else { v->n = (uint8_t)L.n; dv_used += L.n; }
    }
    return v;
}

static void draw_decos(void)
{
    unsigned fl = 0;
    int i, k, t = g_t;
    if (g_off) { for (i = 0; i < DOOMO_COUNTOF(decos); i++) draw_deco(&decos[i]); return; }
    for (i = 0; i < 5; i++) fl = fl * 2 + (ui_wave(t + i * 10, 40, 1) > 0);
    for (i = 0; i < DOOMO_COUNTOF(decos); i++) {
        const Deco *d = &decos[i];
        int x = fp_mx(d->x), y = fp_my(d->y), dx = x - DV_REC_X, dy = y - DV_REC_Y;
        DVar *v;
        const LRect *r;
        if (hidden(x - 24, y - 40, 48, 48)) continue;         /* (well inside any deco) */
        v = dv_get(d, deco_state(d, fl));
        if (v->bad) { draw_deco_xy(d, x, y); continue; }
        r = dv_pool + v->start;
        for (k = 0; k < v->n; ) {
            int m = DOOMO_MIN(v->n - k, dv_tmp_cap), j;
            if (!g_occ && plat_rect_run && !gba_nodraw) {
                for (j = 0; j < m; j++) {
                    dv_tmp[j] = r[k + j];
                    dv_tmp[j].x = (int16_t)(dv_tmp[j].x + dx);
                    dv_tmp[j].y = (int16_t)(dv_tmp[j].y + dy);
                }
                plat_rect_run(dv_tmp, m);
            } else {
                for (j = 0; j < m; j++) {
                    int rx = r[k + j].x + dx, ry = r[k + j].y + dy;
                    if (!hidden(rx, ry, r[k + j].w, r[k + j].h)) plat_rect(rx, ry, r[k + j].w, r[k + j].h, r[k + j].c);
                }
            }
            k += m;
        }
    }
}

/* ====================================================================== */
/* buildings                                                                */
static void flag(int x, int y, uint8_t c)
{
    int w = ui_wave(g_t + x, 20, 1);
    R(x, y, 1, 7, PAL_RAMP(RAMP_STONE, 2));
    R(x + 1, y, 4, 2, c);
    R(x + 1, y + 2, 3 + w, 1, c);
    R(x + 1, y, 1, 1, ui_lighter(c));
}

static void chimney_smoke(int x, int y, uint8_t c)
{
    int i;
    for (i = 0; i < 3; i++) {
        int ph = (g_t / 2 + i * 16) % 48;
        int r = 1 + ph / 16;
        C(x + ph / 8 + ui_wave(ph * 4, 40, 1), y - ph / 3, r, c);
    }
}

static void b_towers(int x, int y, int hell)
{
    int i, j, w = 18, h = 26, bx = x - w / 2, by = y - h;
    uint8_t d = hell ? PAL_RAMP(RAMP_VAPE, 2) : PAL_PAINT(PAINT_BUBBLEGUM, 0);
    uint8_t b = hell ? PAL_RAMP(RAMP_VAPE, 3) : PAL_PAINT(PAINT_BUBBLEGUM, 1);
    uint8_t l = hell ? PAL_RAMP(RAMP_VAPE, 5) : PAL_PAINT(PAINT_BUBBLEGUM, 2);
    if (ST) {
        box(bx, by, w, h, d, b, l);
        /* roof trim */
        R(bx - 1, by - 2, w + 2, 3, hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_PAINT(PAINT_ROSE, 0));
        R(bx, by - 2, w, 1, hell ? PAL_RAMP(RAMP_STONE, 3) : PAL_PAINT(PAINT_ROSE, 2));
        for (j = 0; j < 4; j++)
            for (i = 0; i < 3; i++)
                win(bx + 3 + i * 5, by + 3 + j * 5, 3, 3, night_lit(x * 31 + i * 7 + j * 13 + hell),
                    hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_PAINT(PAINT_ROSE, 0));
        /* door */
        R(x - 2, y - 4, 4, 4, hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_WOOD, 3));
        R(x - 2, y - 4, 4, 1, hell ? PAL_RAMP(RAMP_BLOOD, 4) : PAL_RAMP(RAMP_WOOD, 5));
    }
    if (hell) {
        /* little horns and bat wings */
        if (ST) {
            R(bx + 1, by - 5, 2, 3, PAL_RAMP(RAMP_BONE, 6));
            R(bx, by - 6, 1, 2, PAL_RAMP(RAMP_BONE, 7));
            R(bx + w - 3, by - 5, 2, 3, PAL_RAMP(RAMP_BONE, 6));
            R(bx + w - 1, by - 6, 1, 2, PAL_RAMP(RAMP_BONE, 7));
        }
        if (DY) {
            int f = ui_wave(g_t, 24, 1);
            R(bx - 4, by + 4 + f, 4, 2, PAL_RAMP(RAMP_VAPE, 1));
            R(bx - 5, by + 6 + f, 2, 1, PAL_RAMP(RAMP_VAPE, 1));
            R(bx + w, by + 4 - f, 4, 2, PAL_RAMP(RAMP_VAPE, 1));
            R(bx + w + 3, by + 6 - f, 2, 1, PAL_RAMP(RAMP_VAPE, 1));
        }
    } else {
        /* a heart sign on the roof + flower boxes */
        if (DY) ui_heart(x, by - 5 + (ui_wave(g_t, 40, 1) > 0), 3, tc(PAL_PAINT(PAINT_ROSE, 1)));
        if (ST) {
            R(bx + 2, by + 7, 5, 1, PAL_RAMP(RAMP_GRASS, 4));
            R(bx + 12, by + 12, 5, 1, PAL_RAMP(RAMP_GRASS, 4));
            R(bx + 3, by + 6, 1, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
            R(bx + 13, by + 11, 1, 1, PAL_RAMP(RAMP_CANDY, 7));
        }
    }
}

static void b_town_hall(int x, int y)
{
    int w = 26, h = 14, bx = x - w / 2, by = y - h, tw = 10;
    int hr = town_hour() % 12, mn = doomo_town.minute % 60;
    if (ST) {
        box(bx, by, w, h, PAL_RAMP(RAMP_PEACH, 5), PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_PEACH, 7));
        /* columns */
        R(bx + 3, by + 4, 2, h - 5, PAL_WHITE);
        R(bx + w - 6, by + 4, 2, h - 5, PAL_WHITE);
        R(x - 2, y - 6, 4, 6, PAL_RAMP(RAMP_WOOD, 3));
        /* clock tower */
        box(x - tw / 2, by - 10, tw, 11, PAL_RAMP(RAMP_PEACH, 5), PAL_RAMP(RAMP_PEACH, 6), PAL_WHITE);
        roof(x, by - 17, tw + 4, 7, PAL_PAINT(PAINT_SKY, 0), PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_SKY, 2));
        C(x, by - 5, 3, PAL_RAMP(RAMP_STONE, 2));
        C(x, by - 5, 2, PAL_WHITE);
    }
    /* the hands follow the island clock */
    if (DY) {
        int ha = hr * 4096 / 12 + mn * 4096 / 720, ma = mn * 4096 / 60;
        int hx = FIX2INT(fx_sin(ha) * 2), hy = -FIX2INT(fx_cos(ha) * 2);
        int mx = FIX2INT(fx_sin(ma) * 2 + FIX_HALF), my = -FIX2INT(fx_cos(ma) * 2 + FIX_HALF);
        plat_line(x, by - 5, x + hx, by - 5 + hy, tc(PAL_RAMP(RAMP_STONE, 0)));
        plat_line(x, by - 5, x + mx, by - 5 + my, tc(PAL_PAINT(PAINT_CHERRY, 0)));
    }
    if (DY) flag(x, by - 24, PAL_PAINT(PAINT_ROSE, 1));
    if (ST) {
        win(bx + 7, by + 5, 2, 3, night_lit(77), PAL_RAMP(RAMP_PEACH, 4));
        win(bx + w - 9, by + 5, 2, 3, night_lit(78), PAL_RAMP(RAMP_PEACH, 4));
    }
}

static void awning(int x, int y, int w, uint8_t a, uint8_t b)
{
    int i;
    for (i = 0; i < w; i += 3) R(x + i, y, DOOMO_MIN(3, w - i), 3, (i / 3) & 1 ? b : a);
    for (i = 0; i < w; i += 3) R(x + i + 1, y + 3, 1, 1, (i / 3) & 1 ? b : a);
    R(x, y, w, 1, ui_lighter(a));
}

static void b_shop(int x, int y, int black)
{
    int w = 22, h = 14, bx = x - w / 2, by = y - h;
    if (black) {
        /* the black market: a patched purple tent with a skull lantern */
        int i;
        if (ST) {
            for (i = 0; i < 14; i++) {
                int rw = 2 + i * 22 / 14;
                R(x - rw / 2 - 1, by - 2 + i, rw + 2, 1, PAL_RAMP(RAMP_VAPE, 0));
                R(x - rw / 2, by - 2 + i, rw, 1, (i / 3) & 1 ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_VAPE, 3));
            }
            R(x - 4, by + 4, 8, 8, PAL_RAMP(RAMP_STONE, 0));
            R(x - 3, by + 5, 6, 7, PAL_RAMP(RAMP_BLOOD, 1));
            R(x - 7, by + 6, 3, 3, PAL_PAINT(PAINT_SLIME, 0));
        }
        {
            int fl = (g_t / 5) % 3;
            int lit = g_tint != 0;
            if (ST) {
                R(x + 6, by + 1, 1, 3, PAL_RAMP(RAMP_STONE, 1));
                R(x + 5, by + 4, 3, 3, PAL_RAMP(RAMP_BONE, 6));
            }
            if (DY) {
                RG(x + 5, by + 5, 1, 1, lit ? PAL_RAMP(RAMP_MINT, 7 - fl) : PAL_RAMP(RAMP_STONE, 0));
                RG(x + 7, by + 5, 1, 1, lit ? PAL_RAMP(RAMP_MINT, 7 - fl) : PAL_RAMP(RAMP_STONE, 0));
            }
        }
        if (ST) R(x - 1, by - 5, 2, 3, PAL_RAMP(RAMP_STONE, 1));
        return;
    }
    if (!ST) return;                         /* the shop has no animated parts */
    box(bx, by, w, h, PAL_RAMP(RAMP_PEACH, 5), PAL_UI_CREAM, PAL_WHITE);
    awning(bx - 1, by + 1, w + 2, PAL_PAINT(PAINT_ROSE, 1), PAL_WHITE);
    win(bx + 3, by + 7, 6, 4, night_lit(301), PAL_RAMP(RAMP_PEACH, 4));
    R(x + 3, y - 7, 5, 7, PAL_RAMP(RAMP_WOOD, 3));
    R(x + 3, y - 7, 5, 1, PAL_RAMP(RAMP_WOOD, 5));
    /* coin sign */
    C(x, by - 3, 3, PAL_RAMP(RAMP_CANDY, 3));
    C(x, by - 3, 2, PAL_UI_GOLD);
    R(x - 1, by - 4, 1, 1, PAL_WHITE);
}

static void b_cafe(int x, int y)
{
    int w = 20, h = 13, bx = x - w / 2, by = y - h;
    if (DY && !ST) { chimney_smoke(bx + w - 3, by - 1, PAL_RAMP(RAMP_STONE, 6)); return; }
    box(bx, by, w, h, PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6));
    awning(bx - 1, by + 1, w + 2, PAL_PAINT(PAINT_MINT, 1), PAL_WHITE);
    win(bx + 3, by + 6, 5, 4, night_lit(402), PAL_RAMP(RAMP_WOOD, 3));
    R(x + 2, y - 7, 5, 7, PAL_PAINT(PAINT_CHOCOLATE, 1));
    /* the giant cupcake on the roof */
    R(x - 5, by - 5, 10, 5, PAL_PAINT(PAINT_TANGERINE, 1));
    R(x - 5, by - 5, 10, 1, PAL_PAINT(PAINT_TANGERINE, 2));
    R(x - 4, by - 4, 1, 4, PAL_PAINT(PAINT_TANGERINE, 0));
    R(x, by - 4, 1, 4, PAL_PAINT(PAINT_TANGERINE, 0));
    C(x, by - 8, 5, PAL_PAINT(PAINT_BUBBLEGUM, 0));
    C(x, by - 8, 4, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    C(x - 1, by - 10, 2, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    C(x, by - 13, 1, PAL_PAINT(PAINT_CHERRY, 1));
    R(x - 3, by - 8, 1, 1, PAL_PAINT(PAINT_LEMON, 1));
    R(x + 2, by - 9, 1, 1, PAL_PAINT(PAINT_SKY, 1));
    R(x + 1, by - 6, 1, 1, PAL_PAINT(PAINT_MINT, 1));
    if (DY) chimney_smoke(bx + w - 3, by - 1, PAL_RAMP(RAMP_STONE, 6));
}

static void b_church(int x, int y)
{
    int w = 16, h = 14, bx = x - w / 2, by = y - h, sw = ui_wave(g_t, 50, 1);
    if (DY && !ST) { R(x - 1 + sw, by - 11, 3, 3, PAL_UI_GOLD); return; }
    box(bx, by, w, h, PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 6), PAL_WHITE);
    roof(x, by - 7, w + 4, 8, PAL_PAINT(PAINT_LAVENDER, 0), PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_LAVENDER, 2));
    /* steeple with a swinging bell and a plush cross */
    box(x - 3, by - 14, 7, 8, PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 6), PAL_WHITE);
    roof(x, by - 20, 9, 6, PAL_PAINT(PAINT_LAVENDER, 0), PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_LAVENDER, 2));
    if (DY) R(x - 1 + sw, by - 11, 3, 3, PAL_UI_GOLD);
    R(x, by - 26, 1, 6, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    R(x - 2, by - 24, 5, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    /* heart rose window + door */
    ui_heart(x, by + 4, 2, g_tint ? PAL_RAMP(RAMP_CANDY, 6) : tc(PAL_PAINT(PAINT_ROSE, 1)));
    R(x - 2, y - 6, 4, 6, PAL_RAMP(RAMP_WOOD, 3));
    R(x - 1, y - 7, 2, 1, PAL_RAMP(RAMP_WOOD, 3));
}

static void b_hospital(int x, int y)
{
    int w = 22, h = 15, bx = x - w / 2, by = y - h;
    int pulse = (g_t / 15) % 2;
    if (ST) {
        box(bx, by, w, h, PAL_RAMP(RAMP_MINT, 4), PAL_RAMP(RAMP_MINT, 6), PAL_RAMP(RAMP_MINT, 7));
        R(bx - 1, by - 2, w + 2, 3, PAL_RAMP(RAMP_MINT, 3));
        win(bx + 3, by + 4, 3, 3, night_lit(501), PAL_RAMP(RAMP_MINT, 3));
        win(bx + w - 7, by + 4, 3, 3, night_lit(502), PAL_RAMP(RAMP_MINT, 3));
        win(bx + 3, by + 9, 3, 3, night_lit(503), PAL_RAMP(RAMP_MINT, 3));
        R(x - 2, y - 6, 4, 6, PAL_RAMP(RAMP_SKY, 4));
        /* the red cross sign, pulsing softly */
        R(x - 4, by - 8, 9, 7, PAL_WHITE);
    }
    if (DY) {
        R(x - 1, by - 7, 3, 5, pulse ? PAL_UI_RED : PAL_PAINT(PAINT_CHERRY, 1));
        R(x - 3, by - 6, 7, 3, pulse ? PAL_UI_RED : PAL_PAINT(PAINT_CHERRY, 1));
    }
}

static void b_fair(int x, int y)
{
    int r = 9, cx = x - 3, cy = y - 13, i, a0 = g_t * 6;
    /* ferris wheel */
    R(cx - 1, cy, 2, 13, PAL_RAMP(RAMP_STONE, 4));
    R(cx - 4, y - 2, 8, 2, PAL_RAMP(RAMP_STONE, 3));
    for (i = 0; i < 8; i++) {
        int a = a0 + i * 512;
        int ex = cx + FIX2INT(fx_cos(a) * r), ey = cy + FIX2INT(fx_sin(a) * r);
        plat_line(cx, cy, ex, ey, tc(PAL_RAMP(RAMP_STONE, 6)));
    }
    for (i = 0; i < 16; i++) {
        int a = i * 256;
        R(cx + FIX2INT(fx_cos(a) * r), cy + FIX2INT(fx_sin(a) * r), 1, 1, PAL_PAINT(PAINT_ROSE, 1));
    }
    for (i = 0; i < 6; i++) {
        static const uint8_t gc[6] = { PAINT_LEMON, PAINT_MINT, PAINT_SKY, PAINT_BUBBLEGUM, PAINT_LAVENDER, PAINT_TANGERINE };
        int a = a0 + i * 683;
        int gx = cx + FIX2INT(fx_cos(a) * r), gy = cy + FIX2INT(fx_sin(a) * r);
        R(gx - 1, gy, 3, 3, PAL_PAINT(gc[i], 1));
        R(gx - 1, gy, 3, 1, PAL_PAINT(gc[i], 2));
    }
    C(cx, cy, 1, PAL_UI_GOLD);
    /* striped tent */
    for (i = 0; i < 8; i++) {
        int rw = 2 + i * 12 / 8;
        R(x + 8 - rw / 2, y - 8 + i, rw, 1, (i & 1) ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_WHITE);
    }
    R(x + 8, y - 11, 1, 3, PAL_RAMP(RAMP_STONE, 2));
    R(x + 9, y - 11, 2, 1, PAL_UI_GOLD);
}

static void b_library(int x, int y)
{
    int w = 22, h = 14, bx = x - w / 2, by = y - h;
    if (!ST) return;                         /* no animated parts */
    box(bx, by, w, h, PAL_RAMP(RAMP_WOOD, 2), PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5));
    roof(x, by - 7, w + 4, 8, PAL_PAINT(PAINT_FOREST, 0), PAL_PAINT(PAINT_FOREST, 1), PAL_PAINT(PAINT_FOREST, 2));
    win(bx + 3, by + 4, 4, 5, night_lit(601), PAL_RAMP(RAMP_WOOD, 1));
    win(bx + w - 7, by + 4, 4, 5, night_lit(602), PAL_RAMP(RAMP_WOOD, 1));
    R(x - 2, y - 7, 4, 7, PAL_RAMP(RAMP_WOOD, 1));
    /* an open book sign */
    R(x - 5, by - 3, 5, 4, PAL_WHITE);
    R(x + 1, by - 3, 5, 4, PAL_WHITE);
    R(x, by - 3, 1, 5, PAL_PAINT(PAINT_CHERRY, 0));
    R(x - 4, by - 2, 3, 1, PAL_RAMP(RAMP_STONE, 4));
    R(x + 2, by - 2, 3, 1, PAL_RAMP(RAMP_STONE, 4));
}

static void b_concert(int x, int y)
{
    int i, by = y - 10;
    if (ST) {
        R(x - 13, by, 26, 10, ui_darker(PAL_PAINT(PAINT_LAVENDER, 0)));
        R(x - 12, by + 1, 24, 9, PAL_PAINT(PAINT_LAVENDER, 1));
        for (i = 0; i < 4; i++) R(x - 10 + i * 6, by + 3, 3, 7, PAL_PAINT(PAINT_GRAPE, 0));
        /* dome */
        E(x, by, 13, 9, PAL_PAINT(PAINT_GRAPE, 0));
        E(x, by, 12, 8, PAL_PAINT(PAINT_GRAPE, 1));
        E(x - 3, by - 3, 6, 4, PAL_PAINT(PAINT_GRAPE, 2));
        R(x - 13, by, 26, 2, PAL_PAINT(PAINT_GRAPE, 0));
        R(x - 1, by - 12, 2, 4, PAL_UI_GOLD);
    }
    /* notes floating up */
    for (i = 0; DY && i < 2; i++) {
        int ph = (g_t + i * 30) % 60, nx = x + 6 + i * 4 + ui_wave(ph * 4, 60, 2);
        if (ph < 50) {
            char g[2] = { (char)ICON_NOTE, 0 };
            plat_text(nx, by - 12 - ph / 3, g, tc(i ? PAL_PAINT(PAINT_ROSE, 1) : PAL_PAINT(PAINT_SKY, 0)));
        }
    }
}

static void gear(int cx, int cy, int r, int a0, uint8_t c)
{
    int i;
    C(cx, cy, r, c);
    for (i = 0; i < 6; i++) {
        int a = a0 + i * 683;
        R(cx + FIX2INT(fx_cos(a) * (r + 1)) - 1, cy + FIX2INT(fx_sin(a) * (r + 1)) - 1, 2, 2, c);
    }
    C(cx, cy, r / 2 > 0 ? r / 2 : 1, ui_lighter(c));
}

static void b_workshop(int x, int y)
{
    int w = 22, h = 13, bx = x - w / 2, by = y - h;
    if (ST) {
        box(bx, by, w, h, PAL_RAMP(RAMP_STEEL, 3), PAL_RAMP(RAMP_STEEL, 4), PAL_RAMP(RAMP_STEEL, 6));
        R(bx - 1, by - 2, w + 2, 3, PAL_RAMP(RAMP_WOOD, 2));
        /* garage door stripes */
        R(x - 6, y - 8, 12, 8, PAL_RAMP(RAMP_STEEL, 2));
        R(x - 6, y - 6, 12, 1, PAL_RAMP(RAMP_STEEL, 5));
        R(x - 6, y - 3, 12, 1, PAL_RAMP(RAMP_STEEL, 5));
    }
    if (DY) {
        if ((g_t / 7) % 5 == 0) RG(x - 2 + (g_t % 5), y - 5, 2, 2, PAL_RAMP(RAMP_LAVA, 7));   /* weld sparks */
        gear(bx + 4, by - 5, 3, g_t * 12, PAL_UI_GOLD);
    }
    if (ST) R(bx + w - 5, by - 7, 3, 6, PAL_RAMP(RAMP_STONE, 2));
    if (DY) chimney_smoke(bx + w - 4, by - 8, PAL_RAMP(RAMP_STONE, 4));
}

static void b_beach(int x, int y)
{
    int i;
    if (ST) {
        /* sandcastle */
        R(x + 3, y - 5, 7, 5, PAL_RAMP(RAMP_CANDY, 4));
        R(x + 3, y - 7, 2, 2, PAL_RAMP(RAMP_CANDY, 4));
        R(x + 8, y - 7, 2, 2, PAL_RAMP(RAMP_CANDY, 4));
        R(x + 4, y - 4, 2, 1, PAL_RAMP(RAMP_CANDY, 6));
        R(x + 6, y - 2, 1, 2, PAL_RAMP(RAMP_PEACH, 4));
    }
    if (DY) flag(x + 9, y - 12, PAL_PAINT(PAINT_SKY, 1));
    if (ST) {
        /* umbrella */
        R(x - 5, y - 12, 1, 12, PAL_WHITE);
        for (i = 0; i < 5; i++) {
            int rw = 3 + i * 3;
            R(x - 5 - rw / 2, y - 16 + i, rw, 1, (i & 1) ? PAL_WHITE : PAL_PAINT(PAINT_CHERRY, 1));
        }
        /* towel */
        R(x - 11, y - 3, 7, 3, PAL_PAINT(PAINT_SKY, 1));
        R(x - 11, y - 2, 7, 1, PAL_WHITE);
    }
    /* a beach ball bouncing */
    if (DY) {
        int b = ui_wave(g_t, 30, 3);
        int by = y - 4 - (b > 0 ? b : -b);
        C(x - 13, by, 2, PAL_PAINT(PAINT_LEMON, 1));
        R(x - 14, by - 1, 1, 1, PAL_WHITE);
        R(x - 13, by, 2, 1, PAL_PAINT(PAINT_CHERRY, 1));
    }
}

static void b_park(int x, int y)
{
    int i, t = g_t;
    if (ST) {
        /* lawn circle */
        E(x, y - 4, 14, 6, PAL_RAMP(RAMP_GRASS, 4));
        E(x, y - 5, 13, 5, PAL_RAMP(RAMP_GRASS, 6));
        /* the bridge planks over the river */
        R(x - 6, y - 7, 12, 4, PAL_RAMP(RAMP_WOOD, 4));
        for (i = 0; i < 12; i += 2) R(x - 6 + i, y - 7, 1, 4, PAL_RAMP(RAMP_WOOD, 2));
        R(x - 6, y - 7, 12, 1, PAL_RAMP(RAMP_WOOD, 6));
        /* fountain */
        E(x, y - 8, 6, 3, PAL_RAMP(RAMP_STONE, 4));
        E(x, y - 9, 5, 2, PAL_RAMP(RAMP_SKY, 5));
        R(x - 1, y - 15, 2, 6, PAL_RAMP(RAMP_STONE, 5));
        if (!DY) {
            /* the sprite also gets the tree trunks, which deco_tree()
             * then leaves out (nothing animated overlaps them) */
            deco_trunk(x - 11, y - 3);
            deco_trunk(x + 11, y - 3);
        }
    }
    if (!DY) return;
    for (i = 0; i < 6; i++) {
        int ph = (t + i * 5) % 30;
        int dx = (i - 3) * ph / 10, dy = -15 - ph / 2 + ph * ph / 60;
        R(x + dx, y + dy, 1, 1, PAL_RAMP(RAMP_SKY, 7));
    }
    R(x - 1, y - 17, 2, 2, PAL_WHITE);
    /* two trees, one pastel one spooky: the two halves of the park */
    deco_tree(x - 11, y - 3, PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7),
              ui_wave(t, 90, 1));
    deco_tree(x + 11, y - 3, PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 6),
              ui_wave(t + 40, 90, 1));
}

static void b_wall_gate(int x, int y)
{
    int i, t = g_t;
    if (ST) {
        /* a tall chunk of wall with an arched gate */
        box(x - 12, y - 16, 24, 16, PAL_RAMP(RAMP_STONE, 2), PAL_RAMP(RAMP_STONE, 4), PAL_RAMP(RAMP_STONE, 5));
        for (i = 0; i < 6; i++) R(x - 12 + i * 4, y - 19, 3, 3, PAL_RAMP(RAMP_STONE, 4));
        for (i = 0; i < 6; i++) R(x - 12 + i * 4, y - 19, 3, 1, PAL_RAMP(RAMP_STONE, 6));
        R(x - 5, y - 11, 10, 11, PAL_RAMP(RAMP_STONE, 0));
        E(x, y - 11, 5, 3, PAL_RAMP(RAMP_STONE, 0));
        /* the bars, the chains and the big padlock */
        for (i = 0; i < 4; i++) R(x - 4 + i * 3, y - 12, 1, 12, PAL_RAMP(RAMP_STEEL, 4));
    }
    /* (the platform draws lines: they and what covers them stay live) */
    if (!DY) return;
    plat_line(x - 5, y - 9, x + 5, y - 3, tc(PAL_RAMP(RAMP_STEEL, 5)));
    plat_line(x + 5, y - 9, x - 5, y - 3, tc(PAL_RAMP(RAMP_STEEL, 5)));
    R(x - 2, y - 7, 5, 4, PAL_UI_GOLD);
    R(x - 1, y - 9, 3, 2, PAL_RAMP(RAMP_CANDY, 3));
    R(x, y - 6, 1, 2, PAL_RAMP(RAMP_CANDY, 2));
    /* purple fog leaking over the top */
    for (i = 0; i < 3; i++) {
        int ph = (t / 2 + i * 21) % 64;
        if (ph < 48) C(x - 8 + i * 8 + ph / 12, y - 20 - ph / 8, 1 + ph / 20, PAL_RAMP(RAMP_VAPE, 5));
    }
}

static void b_arena(int x, int y, int locked)
{
    int i;
    if (ST) {
        E(x, y - 6, 13, 7, PAL_RAMP(RAMP_BLOOD, 1));
        E(x, y - 7, 12, 6, PAL_RAMP(RAMP_BONE, 4));
        E(x, y - 8, 9, 4, PAL_RAMP(RAMP_PEACH, 4));
        for (i = 0; i < 7; i++) R(x - 11 + i * 4, y - 5, 2, 3, PAL_RAMP(RAMP_BLOOD, 1));
    }
    if (!DY) return;
    flag(x - 11, y - 18, PAL_PAINT(PAINT_HELLFIRE, 1));
    flag(x + 10, y - 18, PAL_PAINT(PAINT_HELLFIRE, 1));
    if (locked) {
        plat_line(x - 12, y - 12, x + 12, y - 2, tc(PAL_RAMP(RAMP_STEEL, 3)));
        plat_line(x + 12, y - 12, x - 12, y - 2, tc(PAL_RAMP(RAMP_STEEL, 3)));
        R(x - 3, y - 10, 6, 5, PAL_RAMP(RAMP_STEEL, 4));
        R(x - 2, y - 12, 4, 2, PAL_RAMP(RAMP_STEEL, 5));
        R(x - 1, y - 8, 1, 2, PAL_RAMP(RAMP_STEEL, 1));
    } else {
        int ph = (g_t / 4) % 8;
        RG(x - 4 + ph, y - 9, 2, 2, PAL_RAMP(RAMP_LAVA, 6));
    }
}

static void b_square(int x, int y)
{
    int i, t = g_t;
    if (ST) {
        /* goth fountain with purple glowing water and green flames */
        E(x, y - 4, 10, 4, PAL_RAMP(RAMP_STONE, 1));
        E(x, y - 5, 9, 3, PAL_RAMP(RAMP_STONE, 3));
        E(x, y - 5, 7, 2, PAL_RAMP(RAMP_VAPE, 5));
        R(x - 1, y - 13, 3, 8, PAL_RAMP(RAMP_STONE, 2));
        R(x - 2, y - 15, 5, 3, PAL_RAMP(RAMP_BONE, 6));
        R(x - 1, y - 14, 1, 1, PAL_RAMP(RAMP_STONE, 0));
        R(x + 1, y - 14, 1, 1, PAL_RAMP(RAMP_STONE, 0));
    }
    for (i = -1; i <= 1; i += 2) {
        int fx = x + i * 11, fl = ui_wave(t + i * 13, 12, 1);
        if (ST) R(fx, y - 9, 1, 9, PAL_RAMP(RAMP_STONE, 1));
        if (DY) {
            RG(fx - 1, y - 13 + fl, 3, 3, PAL_PAINT(PAINT_SLIME, 1));
            RG(fx, y - 15 + fl, 1, 2, PAL_PAINT(PAINT_SLIME, 2));
        }
    }
    for (i = 0; DY && i < 3; i++) {
        int ph = (t + i * 20) % 60;
        if (ph < 40) C(x - 4 + i * 4 + ph / 10, y - 16 - ph / 4, 1 + ph / 20, PAL_RAMP(RAMP_VAPE, 6));
    }
}

static void b_lake(int x, int y)
{
    int i, t = g_t;
    if (ST) {
        E(x, y - 5, 16, 6, PAL_RAMP(RAMP_STONE, 1));
        E(x, y - 5, 15, 5, PAL_RAMP(RAMP_LAVA, 3));
        E(x - 2, y - 6, 11, 3, PAL_RAMP(RAMP_LAVA, 5));
    }
    /* (the pier is drawn over the lava bubbles: it stays live too) */
    if (!DY) return;
    for (i = 0; i < 4; i++) {
        int ph = (t / 2 + i * 13) % 26;
        int bx = x - 10 + i * 6, by = y - 5 + (i & 1);
        if (ph < 8) RG(bx, by - ph / 3, 2, 2, PAL_RAMP(RAMP_LAVA, 7));
        else if (ph < 11) RG(bx - 1, by - 3, 4, 1, PAL_RAMP(RAMP_LAVA, 6));
    }
    /* the pier */
    R(x + 9, y - 9, 9, 3, PAL_RAMP(RAMP_WOOD, 2));
    R(x + 9, y - 9, 9, 1, PAL_RAMP(RAMP_WOOD, 4));
    R(x + 10, y - 6, 1, 3, PAL_RAMP(RAMP_WOOD, 1));
    R(x + 16, y - 6, 1, 3, PAL_RAMP(RAMP_WOOD, 1));
    /* a fishing line */
    plat_line(x + 15, y - 15, x + 6, y - 6, tc(PAL_RAMP(RAMP_STONE, 6)));
    R(x + 15, y - 15, 1, 7, PAL_RAMP(RAMP_WOOD, 3));
}

static void tomb(int x, int y, uint8_t c)
{
    R(x - 2, y - 6, 5, 6, ui_darker(c));
    R(x - 1, y - 7, 3, 1, ui_darker(c));
    R(x - 1, y - 6, 3, 6, c);
    R(x - 1, y - 6, 1, 6, ui_lighter(c));
    R(x, y - 5, 1, 3, ui_darker(c));
    R(x - 1, y - 4, 3, 1, ui_darker(c));
}

static void b_graveyard(int x, int y)
{
    int i, t = g_t;
    if (ST) {
        /* fence */
        for (i = 0; i < 8; i++) R(x - 13 + i * 4, y - 6, 1, 6, PAL_RAMP(RAMP_STONE, 1));
        R(x - 13, y - 4, 29, 1, PAL_RAMP(RAMP_STONE, 1));
        tomb(x - 8, y - 2, PAL_RAMP(RAMP_STONE, 5));
        tomb(x, y - 4, PAL_RAMP(RAMP_STONE, 6));
        tomb(x + 8, y - 1, PAL_RAMP(RAMP_STONE, 5));
        R(x - 5, y - 2, 2, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
        R(x + 4, y - 1, 2, 1, PAL_RAMP(RAMP_CANDY, 7));
    }
    /* the friendly ghost */
    if (DY) {
        int gx = x + 6 + ui_wave(t, 80, 4), gy = y - 15 + ui_wave(t + 20, 50, 2);
        C(gx, gy, 3, PAL_RAMP(RAMP_MINT, 6));
        R(gx - 3, gy, 7, 3, PAL_RAMP(RAMP_MINT, 6));
        R(gx - 3, gy + 3, 1, 1, PAL_RAMP(RAMP_MINT, 6));
        R(gx, gy + 3, 1, 1, PAL_RAMP(RAMP_MINT, 6));
        R(gx + 3, gy + 3, 1, 1, PAL_RAMP(RAMP_MINT, 6));
        R(gx - 1, gy - 1, 1, 2, PAL_RAMP(RAMP_STONE, 0));
        R(gx + 1, gy - 1, 1, 2, PAL_RAMP(RAMP_STONE, 0));
        R(gx - 2, gy - 2, 1, 1, PAL_WHITE);
    }
}

void fp_island_building(int kind, int x, int y, int locked)
{
    switch (kind) {
    case LOC_TOWERS: b_towers(x, y, 0); break;
    case LOC_PITS: b_towers(x, y, 1); break;
    case LOC_TOWN_HALL: b_town_hall(x, y); break;
    case LOC_SHOP: b_shop(x, y, 0); break;
    case LOC_BLACK_MARKET: b_shop(x, y, 1); break;
    case LOC_CAFE: b_cafe(x, y); break;
    case LOC_CHURCH: b_church(x, y); break;
    case LOC_HOSPITAL: b_hospital(x, y); break;
    case LOC_FUN_FAIR: b_fair(x, y); break;
    case LOC_LIBRARY: b_library(x, y); break;
    case LOC_CONCERT: b_concert(x, y); break;
    case LOC_WORKSHOP: b_workshop(x, y); break;
    case LOC_BEACH: b_beach(x, y); break;
    case LOC_PARK: b_park(x, y); break;
    case LOC_WALL: b_wall_gate(x, y); break;
    case LOC_ARENA: b_arena(x, y, locked); break;
    case LOC_SQUARE: b_square(x, y); break;
    case LOC_LAKE: b_lake(x, y); break;
    case LOC_GRAVEYARD: b_graveyard(x, y); break;
    default: break;
    }
}

/* ---- (integration, perf) building sprites ----------------------------------
 * The static part of every building (all but the flags, smoke, bells,
 * wheels, ghosts, lines ...) is rendered once into an 8bpp sprite in the
 * island's scene scratch and drawn with one plat_image: ~790 plat_rect
 * calls a frame become ~20 images + the animated bits, pixel for pixel
 * the same picture. The sprites are rebuilt when the screen size, the
 * tint, the day (lit windows) or a lock changes, and after another scene
 * took the scratch. Buildings that do not fit stay drawn live. */
static int cacheable(int kind)
{
    /* the fair's wheel, tent and posts overlap each other: all live */
    return kind < LOC_COUNT && kind != LOC_FUN_FAIR;
}

static uint32_t bc_key(const FpLocDraw *locs, int n)
{
    uint32_t h = 2166136261u;
    int i;
#define BC_MIX(v) (h = (h ^ (uint32_t)(v)) * 16777619u)
    BC_MIX(M.w); BC_MIX(M.h); BC_MIX(g_tint); BC_MIX(g_tint ? doomo_town.day : 0); BC_MIX(n);
    for (i = 0; i < n; i++) { BC_MIX(locs[i].kind); BC_MIX(locs[i].x); BC_MIX(locs[i].y); BC_MIX(locs[i].locked); }
#undef BC_MIX
    return h | 1u;
}

static void bc_build(FpBCache *bc, const FpLocDraw *locs, int n, uint32_t key)
{
    int i, used = 0;
    memset(bc->spr, 0, sizeof(bc->spr));
    for (i = 0; i < n && i < FP_LOC_MAX; i++) {
        const FpLocDraw *l = &locs[i];
        int x0, y0, x1, y1, ux0, uy0, ux1, uy1, w, h;
        if (!cacheable(l->kind)) continue;
        /* measure the static part */
        g_pass = PASS_STATIC;
        g_off = G_MEASURE;
        g_bx0 = g_by0 = 0x7FFF; g_bx1 = g_by1 = -0x7FFF;
        ui_raster_target(NULL, 0, 0, 0, 0);
        fp_island_building(l->kind, l->x, l->y, l->locked);
        x0 = g_bx0; y0 = g_by0; x1 = g_bx1; y1 = g_by1;
        if (ui_raster_bounds(&ux0, &uy0, &ux1, &uy1)) {
            x0 = DOOMO_MIN(x0, ux0); y0 = DOOMO_MIN(y0, uy0);
            x1 = DOOMO_MAX(x1, ux1); y1 = DOOMO_MAX(y1, uy1);
        }
        w = x1 - x0;
        h = y1 - y0;
        if (w > 0 && h > 0 && w <= 255 && h <= 255 && used + w * h <= FP_BCACHE_BYTES) {
            uint8_t *pix = bc->pix + used;
            memset(pix, 0, (size_t)(w * h));
            g_off = pix;
            g_ox = x0; g_oy = y0; g_ow = w; g_oh = h;
            ui_raster_target(pix, x0, y0, w, h);
            fp_island_building(l->kind, l->x, l->y, l->locked);
            bc->spr[i].x = (int16_t)x0;
            bc->spr[i].y = (int16_t)(y0 - l->y);          /* relative to the base */
            bc->spr[i].img.w = (uint16_t)w;
            bc->spr[i].img.h = (uint16_t)h;
            bc->spr[i].img.pix = pix;
            bc->spr[i].img.uid = doomo_image_new_uid();
            used += w * h;
        }
        ui_raster_target_off();
        g_off = NULL;
        g_pass = PASS_ALL;
    }
    bc->key = key;
}

static void draw_walkers_body(void)
{
    int i, t = g_q, n = g_walk_n;
    for (i = 0; i < n; i++) {
        const Seg *s = &roads[i % DOOMO_COUNTOF(roads)];
        int len = (s->x1 - s->x0) + (s->y1 - s->y0), p, x, y, hell, step;
        uint32_t h = doomo_hash32((uint32_t)i * 4099u + 17u);
        static const uint8_t shirts[6] = { PAINT_ROSE, PAINT_SKY, PAINT_LEMON, PAINT_MINT,
                                           PAINT_LAVENDER, PAINT_TANGERINE };
        if (len <= 0) continue;
        p = (int)((h + (uint32_t)(t * (1 + (h >> 28) % 2) / 4)) % (uint32_t)(len * 2));
        if (p > len) p = len * 2 - p;                /* walk back and forth */
        x = fp_mx(s->x0 + (s->y0 == s->y1 ? p : 0)) + 1;
        y = fp_my(s->y0 + (s->y0 == s->y1 ? 0 : p)) + 1;
        hell = s->x0 >= 140;
        step = (t / 6 + i) & 1;
        if (hell && town_is_night() == 0 && (i & 1)) continue;
        R(x, y - 5, 3, 3, hell ? PAL_RAMP(RAMP_VAPE, 6) : PAL_RAMP(RAMP_PEACH, 7));
        R(x, y - 2, 3, 2, PAL_PAINT(hell ? PAINT_GRAPE : shirts[h % 6], 1));
        R(x + step * 2, y, 1, 1, PAL_RAMP(RAMP_STONE, 1));
        R(x + 1, y - 4, 1, 1, PAL_RAMP(RAMP_STONE, 0));
    }
}

static void draw_walkers(void)
{
    int n = 14, pop = fp_island_pop();
    if (pop <= 0) return;
    if (pop < 40) n = 3 + pop / 6;
    g_walk_n = n;
    lr_run(&L_walk, (int32_t)((g_q / ANIM_Q) & 0x3FFF) | ((g_tint == TINT_NIGHT) << 14) | (n << 15), draw_walkers_body);
}

/* ---- weather --------------------------------------------------------------- */
int fp_island_weather(void)
{
    int w = opt_get(OPT_WEATHER), m, d;
    uint32_t h;
    if (!w) return WX_CLEAR;
    sim_date(&m, &d);
    h = doomo_hash32(doomo_town.seed ^ (doomo_town.day * 2246822519u));
    if ((h & 15) < (unsigned)(w == 2 ? 6 : 3)) {
        /* months 0..11 from April: Dec, Jan, Feb are 8, 9, 10 */
        if (m >= 8 && m <= 10) return WX_SNOW;
        return WX_RAIN;
    }
    if (((h >> 4) & 15) < 3) return WX_PETALS;
    return WX_CLEAR;
}

static void draw_weather_body(void)
{
    int i, t = g_q, wx = g_wx;
    for (i = 0; i < 40; i++) {
        uint32_t h = doomo_hash32((uint32_t)i * 2654435761u + 99u);
        int x0 = (int)(h % (uint32_t)M.w), sp = wx == WX_RAIN ? 5 : wx == WX_SNOW ? 1 : 1;
        int y = M.y + (int)(((h >> 8) % (uint32_t)M.h + (uint32_t)(t * sp)) % (uint32_t)M.h);
        int x = M.x + (x0 + (wx == WX_RAIN ? -(t * 2) : ui_wave(t + i * 7, 60, 3))) % M.w;
        if (x < M.x) x += M.w;
        if (wx == WX_RAIN) {
            RG(x, y, 1, 3, PAL_RAMP(RAMP_SKY, 7));
        } else if (wx == WX_SNOW) {
            RG(x, y, 2, 2, PAL_WHITE);
        } else if (i < 16) {
            RG(x, y, 2, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
            RG(x + 1, y + 1, 1, 1, PAL_RAMP(RAMP_PASTEL_PINK, 5));
        }
    }
}

static void draw_weather(int wx)
{
    if (wx == WX_CLEAR) return;
    g_wx = wx;
    lr_run(&L_wx, (int32_t)((g_q / ANIM_Q) & 0x3FFF) | (wx << 14), draw_weather_body);
}

/* ====================================================================== */
/* entry point                                                              */
int fp_island_tint(void)
{
    return town_is_night() ? TINT_NIGHT : TINT_NONE;
}


/* (SP3, P-I1) Where the recorded lists live: the scene scratch when the platform lends one (the PS1), else BSS (or,
 * on the PS1 without scratch, nowhere: cap 0). Asked before every map draw: the scratch is only good while its
 * generation stays the same (an overlay load overwrites it), and a new one means every list starts over. */
/* in the scratch the lists are sized for 320x240 too (the PS1; measured on the Classic island at 320x240: land 761,
 * patches 174, roads 157 rects; at 240x160 480, 136, 125) */
#if defined(DOOMO_PLATFORM_GBA)
/* (CR5a) the GBA's lists are the old BSS lists, byte for byte the same caps */
#define LRS_LAND  LR_MAX
#define LRS_PATCH LR_PATCH
#define LRS_ROAD  LR_ROAD
#define LRS_GLINT AL_GLINT
#define LRS_WALK  AL_WALK
#define LRS_WX    AL_WX
#define LRS_DV    DV_POOL
#define LRS_TMP   DV_TMP
#else
#define LRS_LAND  1024
#define LRS_PATCH 384
#define LRS_ROAD  256
#define LRS_GLINT 48
#define LRS_WALK  64
#define LRS_WX    64
#define LRS_DV    512
#define LRS_TMP   0
#endif
#define LR_SCRATCH_RECTS (LRS_LAND + 1 + LRS_PATCH + 1 + LRS_ROAD + 1 + LRS_GLINT + 1 + LRS_WALK + 1 + LRS_WX + 1 + \
                          LRS_DV + 1 + LRS_TMP)
#if defined(DOOMO_PLATFORM_GBA)
DOOMO_STATIC_ASSERT(LR_SCRATCH_RECTS * sizeof(LRect) == FP_ISLAND_SCRATCH_GBA, island_lists_are_the_gba_region);
#else
DOOMO_STATIC_ASSERT(LR_SCRATCH_RECTS * sizeof(LRect) <= FP_ISLAND_SCRATCH, island_lists_fit_the_scene_scratch);
#endif
static int lr_binds;                         /* times the lists were (re)bound to a scratch (test hook) */
int fp_island_lists_bound(void) { return lr_binds; }

static void lr_attach(LList *L, LRect **p, LRect *bss, int bss_cap, int cap)
{
    if (*p) { L->r = *p; L->cap = cap; *p += cap + 1; }
    else { L->r = bss; L->cap = bss_cap; }
    L->n = 0; L->over = 0; L->key = -1;
}

static void lr_bind(void)
{
    static LRect *seen;
    static uint32_t seen_gen;
    uint32_t gen = 0;
    LRect *p = (LRect *)plat_scene_scratch((uint32_t)(LR_SCRATCH_RECTS * sizeof(LRect)), &gen);
    if (p == seen && (!p || gen == seen_gen)) return;
    seen = p;
    seen_gen = gen;
    if (p) lr_binds++;
    lr_attach(&L_land, &p, lr_land_r, LR_BSS(LR_MAX), LRS_LAND);
    lr_attach(&L_patch, &p, lr_patch_r, LR_BSS(LR_PATCH), LRS_PATCH);
    lr_attach(&L_road, &p, lr_road_r, LR_BSS(LR_ROAD), LRS_ROAD);
    lr_attach(&L_glint, &p, lr_glint_r, LR_BSS(AL_GLINT), LRS_GLINT);
    lr_attach(&L_walk, &p, lr_walk_r, LR_BSS(AL_WALK), LRS_WALK);
    lr_attach(&L_wx, &p, lr_wx_r, LR_BSS(AL_WX), LRS_WX);
    if (p) { dv_pool = p; dv_cap = LRS_DV; }
    else { dv_pool = dv_pool_bss; dv_cap = LR_BSS(DV_POOL); }
#if defined(DOOMO_PLATFORM_GBA)
    dv_tmp = p ? p + LRS_DV + 1 : 0;
    dv_tmp_cap = p ? LRS_TMP : 0;
#endif
    dv_ntab = 0;
    dv_used = 0;
}

/* (perf, I10) The map in phases, so a refresh can be spread over several ticks: sea, land, patches, roads, decos,
 * walkers, buildings, weather, in the order they are drawn. A call with from == 0 sets up the clock, the tint and
 * the geometry; a call with from > 0 goes on with the same ones (the phases of one picture must see one clock,
 * even when the ticks differ). fp_island_draw_map is all of them at once. */
void fp_island_draw_map_part(const FpLocDraw *locs, int n, int sel, int from, int to)
{
    int i, order[FP_LOC_MAX];
    FpBCache *bc;
    lr_bind();
    if (from <= 0) {
        fp_island_geometry();
        g_t = fp_t();
        g_q = g_t - g_t % ANIM_Q;
        g_tint = fp_island_tint();
        {
            static int geo_seen = -1;            /* a new screen size: the lists of the animated phases are stale */
            int geo = M.w | (M.h << 10);
            if (geo != geo_seen) { geo_seen = geo; L_glint.key = L_walk.key = L_wx.key = -1; }
        }
    }
    if (from <= FP_MAP_SEA && to > FP_MAP_SEA) draw_sea();
    if (from <= FP_MAP_LAND && to > FP_MAP_LAND) draw_land();
    if (from <= FP_MAP_PATCHES && to > FP_MAP_PATCHES) draw_patches();
    if (from <= FP_MAP_ROADS && to > FP_MAP_ROADS) draw_roads();
    if (from <= FP_MAP_DECOS && to > FP_MAP_DECOS) draw_decos();
    if (from <= FP_MAP_WALKERS && to > FP_MAP_WALKERS) draw_walkers();
    if (from <= FP_MAP_BUILDINGS && to > FP_MAP_BUILDINGS) {
        bc = fp_island_bcache();
        /* buildings back to front */
        for (i = 0; i < n; i++) order[i] = i;
        for (i = 1; i < n; i++) {
            int j = i, v = order[i];
            while (j > 0 && locs[order[j - 1]].y > locs[v].y) { order[j] = order[j - 1]; j--; }
            order[j] = v;
        }
        /* (integration, perf) the static parts come from the sprite cache */
        {
            uint32_t key = bc_key(locs, n);
            if (bc->key != key && !plat_draw_skipped()) bc_build(bc, locs, n, key);
            if (bc->key != key) bc = NULL;       /* (a skipped frame: draw live) */
        }
        for (i = 0; i < n; i++) {
            const FpLocDraw *l = &locs[order[i]];
            int b = 0;
            if (l->kind >= LOC_COUNT) continue;
            if (order[i] == sel) {
                int ph = g_t % 24;
                b = ph < 6 ? ph / 2 : ph < 12 ? (12 - ph) / 2 : 0;
            }
            shadow(l->x, l->y, 11);
            if (bc && order[i] < FP_LOC_MAX && bc->spr[order[i]].img.pix) {
                const FpSprite *sp = &bc->spr[order[i]];
                /* hidden with a wide margin for its live bits (flags, smoke) */
                if (hidden(sp->x - 24, l->y - b + sp->y - 24, sp->img.w + 48, sp->img.h + 48))
                    continue;
                plat_image(sp->x, l->y - b + sp->y, &sp->img, 1, 0, 0);
                g_pass = PASS_ANIM;
                fp_island_building(l->kind, l->x, l->y - b, l->locked);
                g_pass = PASS_ALL;
            } else {
                fp_island_building(l->kind, l->x, l->y - b, l->locked);
            }
        }
    }
    if (from <= FP_MAP_WEATHER && to > FP_MAP_WEATHER) draw_weather(fp_island_weather());
}

void fp_island_draw_map(const FpLocDraw *locs, int n, int sel)
{
    fp_island_draw_map_part(locs, n, sel, 0, FP_MAP_PHASES);
}

/* (I3b) plat_clear's replacement: the sea paints the whole map box (full width, rows M.y .. M.y + M.h), so only what
 * lies outside it needs the sky colour. Same pixels as a full clear, about 80% of the fill. */
void fp_island_clear_outside(uint8_t c)
{
    int sw = fp_sw(), sh = fp_sh(), y1;
    fp_island_geometry();
    y1 = M.y + M.h;
    if (M.w < sw || M.x < 0 || M.x > 0 || M.h <= 0) { plat_clear(c); return; }
    plat_rect(0, 0, sw, M.y, c);
    plat_rect(0, y1, sw, sh - y1, c);
}

/* test hooks (core/tests/test_island_replay.c): forget every recorded list / count the replays */
void fp_island_replay_flush(void)
{
    L_land.key = L_patch.key = L_road.key = L_glint.key = L_walk.key = L_wx.key = -1;
}
int fp_island_replay_hits(void) { return lr_hits; }
int fp_island_replay_overs(void) { return lr_overs; }
