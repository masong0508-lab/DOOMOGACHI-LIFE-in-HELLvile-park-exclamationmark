/* art_palette.c - palette families, shade steps, light colormaps and tint
 * maps (artgen only). The results are written to core/gen/art_data.c.
 *
 * Every palette index belongs to a "family" (a ramp, a paint's 3 shades,
 * a skin tone's 3 shades, or a small UI group). Shade steps and light
 * levels prefer to stay inside the family so a pink stays pink as it
 * darkens instead of turning into a muddy grey; when the family runs out
 * we fall back to the perceptually nearest colour that keeps the hue.
 */
#include "art.h"

uint8_t art_colormap[DOOMO_LIGHT_LEVELS][256];
uint8_t art_tintmap[TINT_COUNT][256];
uint8_t art_darker_tab[256];
uint8_t art_lighter_tab[256];

static int R[256], G[256], B[256], L[256];
static int fam_id[256];          /* family number                         */
static int fam_pos[256];         /* position inside the family (dark=0)   */
static int fam_len[256];
static uint8_t fam_first[256];   /* first (darkest) index of the family   */
static int setup_done;

void art_rgb(uint8_t idx, int *r, int *g, int *b)
{
    *r = R[idx]; *g = G[idx]; *b = B[idx];
}

int art_luma(uint8_t idx) { return L[idx]; }

static uint32_t dist(int r, int g, int b, int i)
{
    return palette_distance(r, g, b, R[i], G[i], B[i]);
}

uint8_t art_nearest(int r, int g, int b)
{
    uint32_t best = 0xFFFFFFFFu;
    int i, bi = 1;
    for (i = 1; i < 256; i++) {
        uint32_t d = dist(r, g, b, i);
        if (d < best) { best = d; bi = i; }
    }
    return (uint8_t)bi;
}

/* hue (0..1535) and saturation (0..255) of an RGB colour */
static void hue_sat(int r, int g, int b, int *h, int *s)
{
    int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    int mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    int d = mx - mn;
    *s = mx ? d * 255 / mx : 0;
    if (d == 0) { *h = 0; return; }
    if (mx == r) *h = art_wrap(((g - b) * 256) / d, 1536);
    else if (mx == g) *h = ((b - r) * 256) / d + 512;
    else *h = ((r - g) * 256) / d + 1024;
    *h = art_wrap(*h, 1536);
}

static int hue_diff(int a, int b)
{
    int d = art_abs(a - b) % 1536;
    return d > 768 ? 1536 - d : d;
}

/* nearest colour to (r,g,b) that is darker (or lighter) than `ref` and
 * keeps its hue when the reference is saturated. */
static uint8_t nearest_dir(int r, int g, int b, int ref, int darker)
{
    uint32_t best = 0xFFFFFFFFu;
    int i, bi = ref;
    int hr, sr;
    hue_sat(R[ref], G[ref], B[ref], &hr, &sr);
    for (i = 1; i < 256; i++) {
        uint32_t d;
        int hi, si;
        if (darker ? (L[i] >= L[ref] - 4) : (L[i] <= L[ref] + 4)) continue;
        d = dist(r, g, b, i);
        hue_sat(R[i], G[i], B[i], &hi, &si);
        if (sr > 60 && si > 40) d += (uint32_t)(hue_diff(hr, hi) * 40);
        else if (sr > 60 && si <= 40) d += 6000;
        if (d < best) { best = d; bi = i; }
    }
    return (uint8_t)bi;
}

/* Each paint / skin / UI colour continues into a related environment ramp
 * when its own 2-3 shades run out (shadows, outlines, light levels). */
static int rel_ramp[256];
static const int8_t paint_rel[PAL_PAINT_COUNT] = {
    RAMP_STONE, RAMP_STONE, RAMP_STONE, RAMP_STONE,            /* white silver charcoal jet */
    RAMP_BLOOD, RAMP_BLOOD, RAMP_PASTEL_PINK, RAMP_PASTEL_PINK,/* cherry blood rose bubblegum */
    RAMP_PEACH, RAMP_LAVA, RAMP_WOOD, RAMP_CANDY,              /* peach tangerine rust honey */
    RAMP_CANDY, RAMP_SICK, RAMP_GRASS, RAMP_MINT,              /* lemon chartreuse lime mint */
    RAMP_GRASS, RAMP_MINT, RAMP_SKY, RAMP_SKY,                 /* forest teal cyan sky */
    RAMP_SKY, RAMP_SKY, RAMP_VAPE, RAMP_VAPE,                  /* cobalt navy lavender grape */
    RAMP_VAPE, RAMP_PASTEL_PINK, RAMP_WOOD, RAMP_WOOD,         /* vape magenta chocolate caramel */
    RAMP_BONE, RAMP_SICK, RAMP_LAVA, RAMP_MINT};               /* bone slime hellfire ecto */
static const int8_t skin_rel[PAL_SKIN_COUNT] = {
    RAMP_PEACH, RAMP_PEACH, RAMP_PEACH, RAMP_PEACH, RAMP_WOOD, RAMP_WOOD, RAMP_WOOD, RAMP_WOOD};

static void build_related(void)
{
    int i, k;
    for (i = 0; i < 256; i++) rel_ramp[i] = -1;
    for (k = 0; k < PAL_PAINT_COUNT; k++)
        for (i = 0; i < 3; i++) rel_ramp[PAL_PAINT(k, i)] = paint_rel[k];
    for (k = 0; k < PAL_SKIN_COUNT; k++)
        for (i = 0; i < 3; i++) rel_ramp[PAL_SKIN(k, i)] = skin_rel[k];
    rel_ramp[PAL_WHITE] = rel_ramp[PAL_GRAY_L] = rel_ramp[PAL_GRAY] = RAMP_STONE;
    rel_ramp[PAL_GRAY_D] = rel_ramp[PAL_BLACK] = RAMP_STONE;
    rel_ramp[PAL_UI_PINK] = rel_ramp[PAL_UI_PINK_D] = RAMP_PASTEL_PINK;
    rel_ramp[PAL_UI_MINT] = rel_ramp[PAL_UI_MINT_D] = RAMP_MINT;
    rel_ramp[PAL_UI_CREAM] = RAMP_PEACH;
    rel_ramp[PAL_UI_RED] = rel_ramp[PAL_UI_RED_D] = RAMP_BLOOD;
    rel_ramp[PAL_UI_GOLD] = RAMP_CANDY;
    rel_ramp[PAL_UI_PURPLE] = RAMP_VAPE;
    rel_ramp[PAL_UI_LCD] = RAMP_GRASS;
}

/* k is a shading candidate for i: same family or i's related ramp */
static int kin(int i, int k)
{
    if (fam_id[k] == fam_id[i]) return 1;
    if (rel_ramp[i] >= 0 && k >= PAL_RAMP(rel_ramp[i], 0) && k <= PAL_RAMP(rel_ramp[i], 7)) return 1;
    return 0;
}

/* kin colour one clear step darker / lighter than i, closest to the
 * requested rgb; -1 if none */
static int kin_step(int i, int r, int g, int b, int darker)
{
    int k, best = -1;
    uint32_t bestd = 0xFFFFFFFFu;
    for (k = 1; k < 256; k++) {
        uint32_t d;
        if (k == i || !kin(i, k)) continue;
        if (darker ? (L[k] >= L[i] - 8) : (L[k] <= L[i] + 8)) continue;
        d = dist(r, g, b, k);
        if (d < bestd) { bestd = d; best = k; }
    }
    return best;
}

static void set_family(int first, int n, int id)
{
    int k;
    for (k = 0; k < n; k++) {
        fam_id[first + k] = id;
        fam_pos[first + k] = k;
        fam_len[first + k] = n;
        fam_first[first + k] = (uint8_t)first;
    }
}

static int fam_count;
static void set_list(const uint8_t *list, int n)
{
    int k;
    for (k = 0; k < n; k++) {
        fam_id[list[k]] = fam_count;
        fam_pos[list[k]] = k;
        fam_len[list[k]] = n;
        fam_first[list[k]] = list[0];
    }
    fam_count++;
}

/* family member at position pos (only for contiguous families) */
static int fam_member(int idx, int pos)
{
    return fam_first[idx] + pos;
}

static int contiguous(int idx) { return idx >= PAL_PAINT_BASE; }

static void build_steps(void)
{
    int i;
    art_darker_tab[0] = 0;
    art_lighter_tab[0] = 0;
    for (i = 1; i < 256; i++) {
        int r = R[i], g = G[i], b = B[i];
        int dr = r * 66 / 100, dg = g * 62 / 100, db = b * 74 / 100 + 6;
        int lr = r + (255 - r) * 30 / 100 + 6, lg = g + (255 - g) * 30 / 100 + 4,
            lb = b + (255 - b) * 22 / 100;
        /* darker */
        if (contiguous(i) && fam_pos[i] > 0) art_darker_tab[i] = (uint8_t)fam_member(i, fam_pos[i] - 1);
        else if (i == PAL_BLACK) art_darker_tab[i] = PAL_BLACK;
        else if (i == PAL_WHITE) art_darker_tab[i] = PAL_GRAY_L;
        else if (i == PAL_GRAY_L) art_darker_tab[i] = PAL_GRAY;
        else if (i == PAL_GRAY) art_darker_tab[i] = PAL_GRAY_D;
        else if (i == PAL_GRAY_D) art_darker_tab[i] = PAL_PAINT(PAINT_JET, 2);
        else if (i == PAL_UI_PINK) art_darker_tab[i] = PAL_UI_PINK_D;
        else if (i == PAL_UI_MINT) art_darker_tab[i] = PAL_UI_MINT_D;
        else if (i == PAL_UI_RED) art_darker_tab[i] = PAL_UI_RED_D;
        else if (L[i] < 14) art_darker_tab[i] = PAL_BLACK;
        else {
            int k = kin_step(i, dr, dg, db, 1);
            art_darker_tab[i] = (uint8_t)(k >= 0 ? k : nearest_dir(dr, dg, db, i, 1));
        }
        /* lighter */
        if (contiguous(i) && fam_pos[i] < fam_len[i] - 1) art_lighter_tab[i] = (uint8_t)fam_member(i, fam_pos[i] + 1);
        else if (i == PAL_WHITE) art_lighter_tab[i] = PAL_WHITE;
        else if (i == PAL_BLACK) art_lighter_tab[i] = PAL_GRAY_D;
        else if (i == PAL_GRAY_D) art_lighter_tab[i] = PAL_GRAY;
        else if (i == PAL_GRAY) art_lighter_tab[i] = PAL_GRAY_L;
        else if (i == PAL_GRAY_L) art_lighter_tab[i] = PAL_WHITE;
        else if (i == PAL_UI_PINK_D) art_lighter_tab[i] = PAL_UI_PINK;
        else if (i == PAL_UI_MINT_D) art_lighter_tab[i] = PAL_UI_MINT;
        else if (i == PAL_UI_RED_D) art_lighter_tab[i] = PAL_UI_RED;
        else if (L[i] > 244) art_lighter_tab[i] = PAL_WHITE;
        else {
            int k = kin_step(i, lr, lg, lb, 0);
            art_lighter_tab[i] = (uint8_t)(k >= 0 ? k : nearest_dir(lr, lg, lb, i, 0));
        }
    }
}

/* nearest to (r,g,b) preferring the family of `ref` */
static uint8_t nearest_fam(int r, int g, int b, int ref, int slack_pct)
{
    uint32_t gbest = 0xFFFFFFFFu, fbest = 0xFFFFFFFFu;
    int i, gi = 1, fi = -1;
    for (i = 1; i < 256; i++) {
        uint32_t d = dist(r, g, b, i);
        if (d < gbest) { gbest = d; gi = i; }
        if (fam_id[i] == fam_id[ref] && d < fbest) { fbest = d; fi = i; }
    }
    /* a black target should become real black */
    if (r + g + b < 12) return PAL_BLACK;
    if (fi >= 0 && fbest <= gbest + gbest * (uint32_t)slack_pct / 100u + 120u) return (uint8_t)fi;
    return (uint8_t)gi;
}

/* Light level -> brightness factor (0..256): slightly faster than linear
 * so corridors get moody, but level 8+ still reads clearly on a GBA. */
static int light_factor(int lv)
{
    int quad = lv * lv * 256 / 225, lin = lv * 256 / 15;
    return (quad + lin * 2) / 3;
}

/* Shade `i` down to brightness f/256. Pixel-art rule: a colour in shadow
 * walks down its OWN ramp (rich, hue-shifted shadows instead of grey), so
 * we first look for the family member whose brightness matches; only
 * when the family has no shade that dark do we fall back to the nearest
 * palette colour (with a small cool tint, so deep shadow leans blue). */
static uint8_t shade_to(int i, int f)
{
    int r = R[i] * f / 256, g = G[i] * f / 256, b = B[i] * f / 256;
    int tl = L[i] * f / 256, k, best = -1, famlo = 999;
    uint32_t bestd = 0xFFFFFFFFu;
    if (tl < 6) return PAL_BLACK;
    for (k = 1; k < 256; k++)
        if (kin(i, k) && L[k] < famlo) famlo = L[k];
    if (tl >= famlo - 10) {
        for (k = 1; k < 256; k++) {
            int dl;
            uint32_t d;
            if (!kin(i, k)) continue;
            if (L[k] > L[i]) continue;                 /* never brighter */
            dl = L[k] - tl;
            d = (uint32_t)(dl * dl) * 40u + dist(r, g, b, k) / 8u;
            if (d < bestd) { bestd = d; best = k; }
        }
        if (best >= 0) return (uint8_t)best;
    }
    /* fallback: nearest overall, gently cooled, keeping the hue when the
     * source is saturated */
    b += (B[i] + 32) * (256 - f) / 1536;
    r -= r * (256 - f) / 1536;
    return nearest_dir(art_clamp(r, 0, 255), g, art_clamp(b, 0, 255), i, 1);
}

static void build_colormap(void)
{
    int lv, i;
    for (lv = 0; lv < DOOMO_LIGHT_LEVELS; lv++) {
        art_colormap[lv][0] = 0;
        for (i = 1; i < 256; i++) {
            if (lv == DOOMO_LIGHT_LEVELS - 1) { art_colormap[lv][i] = (uint8_t)i; continue; }
            if (lv == 0) { art_colormap[lv][i] = PAL_BLACK; continue; }
            art_colormap[lv][i] = shade_to(i, light_factor(lv));
        }
    }
    /* monotonic: never brighter at a lower light level */
    for (i = 1; i < 256; i++) {
        for (lv = DOOMO_LIGHT_LEVELS - 2; lv >= 0; lv--) {
            uint8_t a = art_colormap[lv][i], up = art_colormap[lv + 1][i];
            if (L[a] > L[up]) art_colormap[lv][i] = up;
        }
    }
}

static int emissive(int i)
{
    if (i == PAL_UI_GOLD || i == PAL_UI_LCD || i == PAL_WHITE) return 1;
    if (i >= PAL_RAMP(RAMP_LAVA, 4) && i <= PAL_RAMP(RAMP_LAVA, 7)) return 1;
    if (i >= PAL_PAINT(PAINT_HELLFIRE, 0) && i <= PAL_PAINT(PAINT_HELLFIRE, 2)) return 1;
    if (i >= PAL_PAINT(PAINT_ECTOPLASM, 1) && i <= PAL_PAINT(PAINT_ECTOPLASM, 2)) return 1;
    if (i == PAL_RAMP(RAMP_CANDY, 7)) return 1;
    return 0;
}

static void build_tints(void)
{
    int t, i;
    for (i = 0; i < 256; i++) art_tintmap[TINT_NONE][i] = (uint8_t)i;
    for (t = 1; t < TINT_COUNT; t++) {
        art_tintmap[t][0] = 0;
        for (i = 1; i < 256; i++) {
            int r = R[i], g = G[i], b = B[i];
            switch (t) {
            case TINT_RED:     /* damage flash */
                r = r * 50 / 100 + 255 * 50 / 100; g = g * 45 / 100; b = b * 45 / 100;
                break;
            case TINT_GOLD:    /* holy flash / pickup */
                r = r * 60 / 100 + 255 * 40 / 100; g = g * 60 / 100 + 224 * 40 / 100;
                b = b * 60 / 100 + 96 * 40 / 100;
                break;
            case TINT_GREEN:   /* sickness / slime */
                r = r * 55 / 100 + 40 * 45 / 100; g = g * 55 / 100 + 230 * 45 / 100;
                b = b * 55 / 100 + 60 * 45 / 100;
                break;
            case TINT_PURPLE:  /* vape haze: hazy and lifted */
                r = r * 55 / 100 + 176 * 45 / 100; g = g * 50 / 100 + 104 * 50 / 100;
                b = b * 55 / 100 + 224 * 45 / 100;
                break;
            default:           /* TINT_NIGHT */
                if (emissive(i)) { art_tintmap[t][i] = (uint8_t)i; continue; }
                {
                    int l = (r * 77 + g * 150 + b * 29) >> 8;
                    /* desaturate a bit, darken, push to moonlit blue */
                    r = (r * 2 + l) / 3; g = (g * 2 + l) / 3; b = (b * 2 + l) / 3;
                    r = r * 34 / 100 + 6; g = g * 42 / 100 + 10; b = b * 60 / 100 + 34;
                }
                break;
            }
            art_tintmap[t][i] = nearest_fam(art_clamp(r, 0, 255), art_clamp(g, 0, 255),
                                            art_clamp(b, 0, 255), i, t == TINT_NIGHT ? 10 : 0);
        }
    }
}

void art_palette_setup(void)
{
    int i, k;
    static const uint8_t grays[] = {PAL_BLACK, PAL_GRAY_D, PAL_GRAY, PAL_GRAY_L, PAL_WHITE};
    static const uint8_t pinks[] = {PAL_UI_PINK_D, PAL_UI_PINK};
    static const uint8_t mints[] = {PAL_UI_MINT_D, PAL_UI_MINT};
    static const uint8_t reds[] = {PAL_UI_RED_D, PAL_UI_RED};
    static const uint8_t cream[] = {PAL_UI_CREAM};
    static const uint8_t gold[] = {PAL_UI_GOLD};
    static const uint8_t purple[] = {PAL_UI_PURPLE};
    static const uint8_t lcd[] = {PAL_UI_LCD};
    if (setup_done) return;
    for (i = 0; i < 256; i++) {
        uint8_t r, g, b;
        palette_rgb((uint8_t)i, &r, &g, &b);
        R[i] = r; G[i] = g; B[i] = b;
        L[i] = (r * 77 + g * 150 + b * 29) >> 8;
    }
    fam_count = 0;
    set_list(grays, 5); set_list(pinks, 2); set_list(mints, 2); set_list(reds, 2);
    set_list(cream, 1); set_list(gold, 1); set_list(purple, 1); set_list(lcd, 1);
    fam_id[0] = -1;
    build_related();
    for (k = 0; k < PAL_PAINT_COUNT; k++) set_family(PAL_PAINT(k, 0), 3, fam_count++);
    for (k = 0; k < PAL_SKIN_COUNT; k++) set_family(PAL_SKIN(k, 0), 3, fam_count++);
    for (k = 0; k < PAL_RAMP_COUNT; k++) set_family(PAL_RAMP(k, 0), 8, fam_count++);
    build_steps();
    build_colormap();
    build_tints();
    setup_done = 1;
}

uint8_t art_darker(uint8_t idx, int steps)
{
    while (steps-- > 0) idx = art_darker_tab[idx];
    return idx;
}

uint8_t art_lighter(uint8_t idx, int steps)
{
    while (steps-- > 0) idx = art_lighter_tab[idx];
    return idx;
}

uint8_t art_outline_of(uint8_t idx)
{
    /* dark, hue-matched: walk down the family until it is properly dark */
    int target, n = 0;
    uint8_t c = idx;
    if (idx == 0) return 0;
    target = L[idx] * 30 / 100;
    if (target < 26) target = 26;
    if (target > 70) target = 70;
    while (L[c] > target && n < 6) {
        uint8_t d = art_darker_tab[c];
        if (d == c) break;
        c = d;
        n++;
    }
    if (n < 2) c = art_darker(c, 2 - n);
    return c;
}

uint8_t art_ramp_v(int ramp, int lo, int hi, int v, int x, int y, int dither)
{
    int span = hi - lo + 1;
    int s = v * span;                 /* 0 .. 256*span */
    int k = s >> 8, frac = s & 255;
    if (dither) {
        int th = art_bayer(x, y) * 16 + 8;
        if (frac > th) k++;
    } else if (frac >= 128) {
        /* round to nearest band */
    }
    k = art_clamp(k, 0, span - 1);
    return (uint8_t)PAL_RAMP(ramp, lo + k);
}

ArtMat art_mat_ramp(int ramp, int lo, int hi)
{
    ArtMat m;
    int k;
    memset(&m, 0, sizeof m);
    for (k = lo; k <= hi && m.n < 12; k++) m.c[m.n++] = PAL_RAMP(ramp, k);
    return m;
}

ArtMat art_mat_paint(int paint)
{
    ArtMat m;
    memset(&m, 0, sizeof m);
    m.n = 5;
    m.c[1] = PAL_PAINT(paint, 0);
    m.c[2] = PAL_PAINT(paint, 1);
    m.c[3] = PAL_PAINT(paint, 2);
    m.c[0] = art_darker_tab[m.c[1]];
    m.c[4] = art_lighter_tab[m.c[3]];
    return m;
}

ArtMat art_mat_skin(int skin)
{
    ArtMat m;
    memset(&m, 0, sizeof m);
    m.n = 5;
    m.c[1] = PAL_SKIN(skin, 0);
    m.c[2] = PAL_SKIN(skin, 1);
    m.c[3] = PAL_SKIN(skin, 2);
    m.c[0] = art_darker_tab[m.c[1]];
    m.c[4] = art_lighter_tab[m.c[3]];
    return m;
}

ArtMat art_mat_of(int n, const uint8_t *c)
{
    ArtMat m;
    memset(&m, 0, sizeof m);
    if (n > 12) n = 12;
    m.n = (uint8_t)n;
    memcpy(m.c, c, (size_t)n);
    return m;
}

ArtMat art_mat2(uint8_t a, uint8_t b) { uint8_t c[2]; c[0] = a; c[1] = b; return art_mat_of(2, c); }
ArtMat art_mat3(uint8_t a, uint8_t b, uint8_t c3)
{
    uint8_t c[3]; c[0] = a; c[1] = b; c[2] = c3; return art_mat_of(3, c);
}
ArtMat art_mat4(uint8_t a, uint8_t b, uint8_t c3, uint8_t d)
{
    uint8_t c[4]; c[0] = a; c[1] = b; c[2] = c3; c[3] = d; return art_mat_of(4, c);
}
ArtMat art_mat5(uint8_t a, uint8_t b, uint8_t c3, uint8_t d, uint8_t e)
{
    uint8_t c[5]; c[0] = a; c[1] = b; c[2] = c3; c[3] = d; c[4] = e; return art_mat_of(5, c);
}

uint8_t art_mat_pick(const ArtMat *m, int v, int x, int y, int dither)
{
    int s, k, frac;
    if (m->n == 0) return 0;
    v = art_clamp(v, 0, 255);
    s = v * m->n;
    k = s >> 8;
    frac = s & 255;
    if (dither) {
        /* only a soft 50% checker right at the band edge (reads as fluff,
         * never as noise) */
        if (frac > 160 && ((x + y) & 1)) k++;
    }
    k = art_clamp(k, 0, m->n - 1);
    return m->c[k];
}
