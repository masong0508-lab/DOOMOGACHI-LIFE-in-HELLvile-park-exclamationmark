/* mg_gen.c - musgen's composer (owner: MU1T; the prototype of
 * design_generative.md sec. 3-5, moved verbatim - see mg.h).
 *
 * Levels, top down, all lazily evaluated one bar ahead of playback:
 *   1. arc      : chapters of 4-6 sections following an energy contour;
 *                 one session-pool element rotates per chapter
 *   2. section  : form grammar pick (no repeat, recency, anchor rule),
 *                 length, energy, variation level, key, progression
 *                 (anchor or functional grammar), phrase ideas, groove
 *   3. bar      : lead / counter / echo from motif development, harmony,
 *                 bass and drums realised against the section's chords
 *   4. guard    : 4-bar window hashes; a repeat re-rolls non-lead parts
 * Integer only, no division (ARM7TDMI), no malloc, no stdio.
 */
#include "mg.h"

/* ---- RNG: the repo's lowbias32 + xorshift32 (core/src/rng.c) ----------- */
static uint32_t h32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16;
    return x;
}
typedef struct { uint32_t s; } Rng;
static void rs(Rng *r, uint32_t seed) { uint32_t s = h32(seed + 0x9E3779B9u); r->s = s ? s : 0x6D2B79F5u; }
static uint32_t rn(Rng *r) { uint32_t x = r->s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; r->s = x; return x; }
static int rr(Rng *r, int n) { return n <= 1 ? (int)(rn(r) & 0) : (int)(((rn(r) >> 16) * (uint32_t)n) >> 16); }
static int rc(Rng *r, int pct) { return rr(r, 100) < pct; }
static uint32_t mix(uint32_t a, uint32_t b) { return h32(a ^ (b * 0x9E3779B1u)); }
static int rw(Rng *r, const uint8_t *w, int n)
{
    int i, sum = 0, x;
    for (i = 0; i < n; i++) sum += w[i];
    if (sum <= 0) return 0;
    x = rr(r, sum);
    for (i = 0; i < n; i++) { if (x < w[i]) return i; x -= w[i]; }
    return n - 1;
}

/* ---- pitch ---------------------------------------------------------------- */
static const uint8_t mode_steps[GMODE_COUNT][7] = {
    { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 8, 11 },
    { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 4, 5, 7, 9, 10 },
};
static int deg_semi(int mode, int deg)
{
    int oct = 0;
    while (deg < 0) { deg += 7; oct -= 12; }
    while (deg >= 7) { deg -= 7; oct += 12; }
    return oct + mode_steps[mode][deg];
}
static int pc12(int s) { while (s < 0) s += 12; while (s >= 12) s -= 12; return s; }
static int is_minorish(int mode) { return mode == GMODE_MINOR || mode == GMODE_HARM || mode == GMODE_PHRYG || mode == GMODE_DORIAN; }

static const int8_t cq_iv[GCQ_COUNT][4] = {
    { 0, 4, 7, -1 }, { 0, 3, 7, -1 }, { 0, 3, 6, -1 }, { 0, 4, 8, -1 }, { 0, 5, 7, -1 },
    { 0, 7, 12, -1 }, { 0, 4, 7, 10 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 3, 6, 10 },
    { 0, 3, 6, 9 }, { 0, 4, 7, 14 },
};
/* 12-bit pitch-class set of a chord (relative to the key tonic) */
static uint16_t chord_pcs(MgChord c)
{
    uint16_t m = 0;
    int i, q = GCH_Q(c), r = GCH_ROOT(c);
    for (i = 0; i < 4 && cq_iv[q][i] >= 0; i++) m |= (uint16_t)(1u << pc12(r + cq_iv[q][i]));
    if (q == GCQ_POW) m |= (uint16_t)(1u << pc12(r + 7));
    return m;
}
static int diatonic_q(int mode, int d)
{
    int r = deg_semi(mode, d), t = deg_semi(mode, d + 2) - r, f = deg_semi(mode, d + 4) - r;
    if (t == 4) return f == 8 ? GCQ_AUG : GCQ_MAJ;
    return f == 6 ? GCQ_DIM : GCQ_MIN;
}
static int fn_of(int mode, int d)
{
    if (!is_minorish(mode)) return (d == 0 || d == 2 || d == 5) ? GFN_T : (d == 1 || d == 3) ? GFN_S : GFN_D;
    return (d == 0 || d == 2) ? GFN_T : (d == 1 || d == 3 || d == 5) ? GFN_S : GFN_D;
}

/* ---- styles: the dial bands S0..S4 (design sec. 1.2 of direction.md) ------ */
static const MgDna dna_box = { {   /* S0 music box: a ticking clock */
    { 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    {  0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0 },
    {  0 }, { 0 }, { 0 } } };
static const MgDna dna_toy = { {   /* S1 toy pop */
    { 10, 0, 0, 0, 0, 0, 3, 0, 9, 0, 0, 2, 0, 0, 3, 0 },
    {  0, 0, 0, 0,10, 0, 0, 0, 0, 0, 0, 0,10, 0, 0, 2 },
    {  7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0, 7, 0, 9, 0 },
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    {  0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0 },
    {  0 } } };
static const MgDna dna_spook = { { /* S2 spooky swing: rims, claps */
    { 10, 0, 0, 0, 0, 0, 2, 0, 6, 0, 0, 3, 0, 0, 0, 0 },
    {  0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 3 },
    {  8, 0, 3, 0, 8, 0, 3, 0, 8, 0, 3, 0, 8, 0, 3, 0 },
    {  0 },
    {  0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 6, 0, 2, 0 },
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 } } };
static const MgDna dna_rock = { {  /* S3 chip rock */
    { 10, 0, 0, 0, 0, 0, 0, 0,10, 0, 4, 0, 0, 0, 2, 0 },
    {  0, 0, 0, 0,10, 0, 0, 0, 0, 0, 0, 0,10, 0, 0, 1 },
    {  9, 0, 9, 0, 9, 0, 9, 0, 9, 0, 9, 0, 9, 0, 9, 0 },
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0 },
    {  0 }, { 0 } } };
static const MgDna dna_metal = { { /* S4 chip metal: double kick */
    { 10, 9,10, 9,10, 9,10, 9,10, 9,10, 9,10, 9,10, 9 },
    {  0, 0, 0, 0,10, 0, 0, 0, 0, 0, 0, 0,10, 0, 0, 1 },
    {  8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0 },
    {  0 }, { 0 },
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 } } };

/* inst ids are abstract: 0x10 pulse25+vib, 0x11 pulse12, 0x12 pulse50, 0x13 tri,
 * 0x14 saw, 0x15 sync saw, 0x16 PWM, 0x20.. arp chords (low nibble = CQ_*) */
const MgStyle mg_styles[5] = {
    { GHS_ARP8,    GBS_LONG,  0, 5, 1, 0x11, 0x11, 0x13, 0x13, 0, &dna_box   },
    { GHS_OFFBEAT, GBS_ROOT5, 0, 7, 1, 0x10, 0x20, 0x13, 0x11, 0, &dna_toy   },
    { GHS_ARP8,    GBS_WALK,  4, 6, 1, 0x11, 0x20, 0x13, 0x12, 0, &dna_spook },
    { GHS_POWER,   GBS_OCT8,  0, 6, 1, 0x16, 0x20, 0x14, 0x12, 1, &dna_rock  },
    { GHS_POWER,   GBS_GALLOP,0, 7, 1, 0x15, 0x20, 0x14, 0x15, 1, &dna_metal },
};

/* ---- form grammar ------------------------------------------------------------ */
/* weights [previous][next] (0..100). Rules applied on top: no immediate repeat,
 * recency x1/2, anchor at least every 3 sections, REST only after 3+ minutes. */
static const uint8_t form_w[GSEC_COUNT][GSEC_COUNT] = {
    /*            INTRO A   A2  B   C   BRK BLD PEAK TAG REST */
    /* INTRO */ {  0, 100,  0,  0,  0,  0,  0,  0,  0,  0 },
    /* A     */ {  0,   0, 35, 35, 10, 10,  5,  0,  5,  0 },
    /* A2    */ {  0,   0,  0, 40, 25, 15,  5,  0, 15,  0 },
    /* B     */ {  0,  35, 25,  0, 15,  5, 15,  5,  0,  0 },
    /* C     */ {  0,  35,  0,  0,  0, 25, 15, 25,  0,  0 },
    /* BRK   */ {  0,  35,  0, 20,  0,  0, 45,  0,  0,  0 },
    /* BUILD */ {  0,  25,  0,  0,  0,  0,  0, 75,  0,  0 },
    /* PEAK  */ {  0,   0, 30, 25,  0, 20,  0,  0, 25,  0 },
    /* TAG   */ {  0,  55,  0, 25,  0,  0,  0,  0,  0, 20 },
    /* REST  */ { 50,   0,  0,  0,  0, 50,  0,  0,  0,  0 },
};
static const uint8_t sec_bars[GSEC_COUNT] = { 2, 8, 8, 8, 8, 4, 4, 8, 2, 8 };
static int is_anchor(int t) { return t == GSEC_A || t == GSEC_A2 || t == GSEC_PEAK; }
/* energy offset per section type (added to the latched intensity) */
static const int8_t sec_energy[GSEC_COUNT] = { -1, 0, 0, 0, 0, -2, 0, 1, 0, -4 };
/* arcs: energy offsets for the sections of a chapter */
static const int8_t arcs[4][6] = {
    { 0, 0, 1, 1, 0, -1 }, { -1, 0, 1, 0, 0, 0 }, { 0, 1, 0, 1, -1, 0 }, { 0, 0, 0, 1, 0, -1 } };
static const uint8_t arc_len[4] = { 6, 5, 6, 5 };

/* stems by energy: bit per role */
#define GBIT(r) (1u << (r))
static uint8_t stems_for(int e, int type)
{
    uint8_t m;
    if (type == GSEC_REST) return 0;   /* planned rest: the ambient bed only (mus_bed.c) */
    switch (e) {
    case 0:  m = GBIT(GR_HARM); break;
    case 1:  m = GBIT(GR_HARM) | GBIT(GR_BASS); break;
    case 2:  m = GBIT(GR_HARM) | GBIT(GR_BASS) | GBIT(GR_DRUMS); break;
    case 3:  m = GBIT(GR_LEAD) | GBIT(GR_HARM) | GBIT(GR_BASS) | GBIT(GR_DRUMS); break;
    case 4:  m = GBIT(GR_LEAD) | GBIT(GR_HARM) | GBIT(GR_BASS) | GBIT(GR_DRUMS) | GBIT(GR_COUNTER) | GBIT(GR_ECHO); break;
    default: m = GBIT(GR_LEAD) | GBIT(GR_HARM) | GBIT(GR_BASS) | GBIT(GR_DRUMS) | GBIT(GR_COUNTER) | GBIT(GR_ECHO) | GBIT(GR_DESCANT); break;
    }
    if (type == GSEC_INTRO) m &= (uint8_t)~(GBIT(GR_LEAD) | GBIT(GR_COUNTER) | GBIT(GR_ECHO));
    return m;
}

/* ---- harmony: a functional grammar -------------------------------------------- */
static const uint8_t trans_w[3][3] = { { 15, 55, 30 }, { 10, 25, 65 }, { 70, 10, 20 } };
static const uint8_t pool_maj[3][7] = {   /* weight per degree for T / S / D */
    { 60, 0, 12, 0, 0, 28, 0 }, { 0, 45, 0, 55, 0, 0, 0 }, { 0, 0, 0, 0, 85, 0, 15 } };
static const uint8_t pool_min[3][7] = {
    { 70, 0, 30, 0, 0, 0, 0 }, { 0, 20, 0, 45, 0, 35, 0 }, { 0, 0, 0, 0, 55, 0, 45 } };

typedef struct { int mode, dial, var, power; } HCtx;

static MgChord make_chord(const HCtx *h, int d, Rng *r)
{
    int q = diatonic_q(h->mode, d), root = deg_semi(h->mode, d), f = fn_of(h->mode, d);
    if (is_minorish(h->mode) && d == 4 && !h->power) q = GCQ_MAJ;  /* harmonic V */
    if (h->power) q = GCQ_POW;
    else if (h->var >= 2) {
        if (h->dial <= 1) {
            if ((d == 0 || d == 3) && !is_minorish(h->mode) && rc(r, 20)) q = GCQ_MAJ7;
            else if ((d == 1 || d == 5) && q == GCQ_MIN && rc(r, 30)) q = GCQ_MIN7;
            else if (d == 4 && rc(r, 40)) q = GCQ_DOM7;
        } else if (d == 4 && rc(r, 50)) q = GCQ_DOM7;
        else if (d == 6 && is_minorish(h->mode) && h->mode == GMODE_HARM) q = GCQ_DIM7;
    }
    return GCH_MAKE(pc12(root), q, f, d);
}

static int pick_deg(const HCtx *h, int f, int avoid, Rng *r)
{
    uint8_t w[7];
    int i;
    const uint8_t *pw = is_minorish(h->mode) ? pool_min[f] : pool_maj[f];
    for (i = 0; i < 7; i++) w[i] = (uint8_t)(i == avoid ? 0 : pw[i]);
    if (h->power && is_minorish(h->mode) && f == GFN_S) w[1] = 30;  /* bII (phrygian) */
    return rw(r, w, 7);
}

/* n slots ending in cadence `cad`; slot 0 is the tonic when start_t */
static void gen_phrase(const HCtx *h, Rng *r, MgChord *out, int n, int cad, int start_t)
{
    int d[32], f[32], fixed = n, i;
    int minor = is_minorish(h->mode);
    for (i = 0; i < n; i++) { d[i] = -1; f[i] = GFN_T; }
    switch (cad) {
    case GCAD_PAC:    d[n - 1] = 0; d[n - 2] = 4; fixed = n - 2; break;
    case GCAD_HC:     d[n - 1] = 4; fixed = n - 1; break;
    case GCAD_DC:     d[n - 1] = 5; d[n - 2] = 4; fixed = n - 2; break;
    case GCAD_PLAGAL: d[n - 1] = 0; d[n - 2] = 3; fixed = n - 2; break;
    default:         d[n - 1] = 0; d[n - 2] = 6; if (n >= 3) { d[n - 3] = 5; fixed = n - 3; } else fixed = n - 2; break;
    }
    if (start_t && fixed > 0) d[0] = 0;
    for (i = 0; i < n; i++) if (d[i] >= 0) f[i] = fn_of(h->mode, d[i]);
    if (cad == GCAD_AEOL) { if (n >= 3) f[n - 3] = GFN_S; f[n - 2] = GFN_D; }
    for (i = start_t ? 1 : 0; i < fixed; i++) {
        uint8_t w[3];
        int pf = i ? f[i - 1] : GFN_T, g;
        w[0] = trans_w[pf][0]; w[1] = trans_w[pf][1]; w[2] = trans_w[pf][2];
        if (i == fixed - 1 && fixed < n && f[fixed] == GFN_D) { w[1] = (uint8_t)(w[1] + 60); w[0] = (uint8_t)(w[0] >> 2); }
        if (i == fixed - 1 && fixed < n && f[fixed] == GFN_S) { w[2] = 0; }
        g = rw(r, w, 3);
        f[i] = g;
        d[i] = pick_deg(h, g, i ? d[i - 1] : -1, r);
        f[i] = fn_of(h->mode, d[i]);
    }
    for (i = 0; i < n; i++) out[i] = make_chord(h, d[i], r);
    /* applied dominants (cute / spooky only, var >= 1): V/x before x */
    if (!h->power && h->var >= 1)
        for (i = 1; i < fixed; i++) {
            int t = d[i], prev = i - 1;
            if (t == 1 || t == 3 || t == 4 || t == 5) {
                if (prev >= 1 && f[prev] == GFN_T && rc(r, h->var >= 2 ? 22 : 10)) {
                    int root = pc12(GCH_ROOT(out[i]) + 7);
                    out[prev] = GCH_MAKE(root, GCQ_DOM7, GFN_D, d[prev]);
                }
            }
        }
    (void)minor;
}

/* anchor progression from theme data, with colour at var >= 2 */
static void anchor_prog(const MgTheme *th, const HCtx *h, Rng *r, MgChord *out, int n, int rate)
{
    int i;
    for (i = 0; i < n; i++) {
        int k = rate == 2 ? i >> 1 : i, deg;
        while (k >= th->anchor_n) k -= th->anchor_n;
        deg = th->anchor[2 * k] & 7;
        int alt = ((th->anchor[2 * k] >> 3) & 3) - 1, q = th->anchor[2 * k + 1];
        int root = pc12(deg_semi(h->mode, deg) + alt);
        if (h->power) q = GCQ_POW;
        else if (h->var >= 2 && i > 0 && (i & (rate * 4 - 1)) != 0 && rc(r, 25)) {
            if (q == GCQ_MIN) q = GCQ_MIN7; else if (q == GCQ_MAJ && deg == 4) q = GCQ_DOM7;
            else if (q == GCQ_MAJ && deg == 3 && rc(r, 50)) q = GCQ_MAJ7;
        }
        out[i] = GCH_MAKE(root, q, fn_of(h->mode, deg), deg);
    }
}

/* ---- motifs -------------------------------------------------------------------- */
typedef struct { int16_t at, dur; int8_t deg, acc; uint8_t fl; } LNote;

static void derive_motif(MgPiece *p, int k, uint32_t seed)
{
    const MgMotif *m = &p->th->motif[0];
    Rng r;
    int i, n = 0, pos = 0, total = m->bars * p->th->steps;
    MgNote *o = p->pm[k];
    rs(&r, seed);
    if (k == 0) {
        /* the "answer": head rhythm kept, starts a third/fourth higher, tail inverted */
        int base = 2 + rr(&r, 2), pivot = m->n[m->head - 1].deg;
        for (i = 0; i < m->len && n < 16 && pos < total; i++) {
            MgNote x = m->n[i];
            if (i >= m->head) {
                x.deg = (int8_t)(2 * pivot - x.deg + base);
                if (x.dur == 2 && rc(&r, 40)) x.dur = 3;
                else if (x.dur == 3) x.dur = 2;
            } else x.deg = (int8_t)(x.deg + base);
            x.fl = 0; x.acc = 0;
            if (pos + x.dur > total) x.dur = (uint8_t)(total - pos);
            o[n++] = x; pos += x.dur;
        }
    } else {
        /* the "development": head augmented, then a falling sequence of it */
        int sh = rr(&r, 2) ? -1 : 1, rep = 0;
        while (n < 16 && pos < total) {
            for (i = 0; i < m->head && n < 16 && pos < total; i++) {
                MgNote x = m->n[i];
                x.deg = (int8_t)(x.deg + rep * sh * 2);
                x.dur = (uint8_t)(rep == 0 ? x.dur * 2 : x.dur);
                x.fl = 0; x.acc = 0;
                if (pos + x.dur > total) x.dur = (uint8_t)(total - pos);
                o[n++] = x; pos += x.dur;
            }
            rep++;
        }
    }
    if (pos < total && n > 0) o[n - 1].dur = (uint8_t)(o[n - 1].dur + total - pos);
    p->pm_len[k] = (uint8_t)n;
}

/* ---- section planning ----------------------------------------------------------- */
static void gen_pool_prog(MgPiece *p, int k, uint32_t seed)
{
    HCtx h;
    Rng r;
    rs(&r, seed);
    h.mode = p->th->mode; h.dial = p->dial; h.var = 1; h.power = mg_styles[p->dial].power;
    gen_phrase(&h, &r, p->pprog[k], 4, k == 2 ? GCAD_PLAGAL : GCAD_HC, k != 1);
    p->pprog_cad[k] = (uint8_t)(k == 2 ? GCAD_PLAGAL : GCAD_HC);
    p->work += 40;
}

static void gen_groove(MgPiece *p, uint16_t *lane, const MgDna *dna, uint32_t seed)
{
    Rng r;
    int l, s;
    rs(&r, seed);
    for (l = 0; l < 6; l++) {
        uint16_t m = 0;
        for (s = 0; s < 16; s++) {
            int pr = dna->p[l][s];
            if (pr >= 10 || (pr > 0 && rr(&r, 10) < pr)) m |= (uint16_t)(1u << s);
        }
        lane[l] = m;
    }
    p->work += 96;
}

static void rotate_pool(MgPiece *p)
{
    uint32_t sd = mix(p->seed, 0xC0000u + p->chapter);
    switch (p->pool_next) {
    case 0: derive_motif(p, 0, sd); break;
    case 1: gen_pool_prog(p, 0, sd); break;
    case 2: derive_motif(p, 1, sd); break;
    case 3: gen_pool_prog(p, 1, sd); break;
    default: gen_groove(p, p->pgroove[1], mg_styles[p->dial].dna, sd); break;
    }
    p->pool_next = (uint8_t)(p->pool_next >= 4 ? 0 : p->pool_next + 1);
}

static int choose_type(MgPiece *p, Rng *r)
{
    uint8_t w[GSEC_COUNT];
    int prev = p->hist[0], t, minutes_bars = 0;
    for (t = 0; t < GSEC_COUNT; t++) {
        int x = form_w[prev][t];
        if (t == prev) x = 0;
        if (t == p->hist[1] || t == p->hist[2]) x >>= 1;
        if (p->since_anchor >= 2 && !is_anchor(t)) x = 0;
        if (t == GSEC_PEAK && p->intensity < 2) x = 0;
        if (t == GSEC_REST) { (void)minutes_bars; if (p->bar_total < 96 || p->hist[3] == GSEC_REST) x = 0; }
        w[t] = (uint8_t)x;
    }
    t = rw(r, w, GSEC_COUNT);
    if (w[t] == 0) t = GSEC_A;          /* every weight zero: the theme */
    return t;
}

static void plan_section(MgPiece *p)
{
    const MgTheme *th = p->th;
    MgSection *s = &p->cur;
    const MgStyle *st;
    Rng r;
    HCtx h;
    int t, i, n, rate, e, nideas, sentence;
    uint32_t seed = mix(p->seed, 0x5EC00000u + p->sec_count);
    rs(&r, seed);
    /* chapter bookkeeping: a new arc and one pool rotation per chapter */
    if (p->sec_count > 0 && ++p->chap_pos >= arc_len[p->arc]) {
        p->chap_pos = 0;
        p->chapter++;
        p->arc = (uint8_t)((p->arc + 1 + rr(&r, 3)) & 3);
        rotate_pool(p);
        if (p->chapter >= 2 && rc(&r, 35)) p->key_lift = (uint8_t)(p->key_lift ? 0 : (rc(&r, 60) ? 2 : 1));
        else p->key_lift = 0;
    }
    t = p->sec_count == 0 ? GSEC_INTRO : choose_type(p, &r);
    memset(s, 0, sizeof(*s));
    s->type = (uint8_t)t;
    s->seed = seed;
    s->bars = sec_bars[t];
    if (t == GSEC_REST) s->bars = (uint8_t)(8 + (rr(&r, 2) << 3));
    e = p->intensity + sec_energy[t] + arcs[p->arc][p->chap_pos];
    if (t == GSEC_A && p->type_passes[GSEC_A] == 0) e = p->intensity;  /* first statement */
    if (is_anchor(t) && e < 1) e = 1;     /* the theme never plays at I0 */
    s->energy = (uint8_t)(e < 0 ? 0 : e > 5 ? 5 : e);
    if (t == GSEC_REST) s->energy = 0;
    {   /* variation level: V0 on the first statement, then rising */
        int v = p->type_passes[t];
        v = v == 0 ? (t == GSEC_A ? 0 : 1) : v < 2 ? 1 : v < 4 ? 2 : 3;
        s->var = (uint8_t)(v > th->var_max ? th->var_max : v);
    }
    s->dial = p->dial;
    st = &mg_styles[s->dial];
    s->mode = th->mode;
    if (s->dial == 2 && !is_minorish(th->mode)) s->mode = GMODE_HARM;    /* spooky-cute mapping */
    s->key = (uint8_t)pc12(th->key + ((t == GSEC_PEAK || t == GSEC_A2) ? p->key_lift : 0));
    /* progression */
    h.mode = s->mode; h.dial = s->dial; h.var = s->var; h.power = st->power;
    rate = st->chord_rate;
    n = s->bars * rate;
    if (is_anchor(t) || t == GSEC_INTRO || t == GSEC_TAG) {
        anchor_prog(th, &h, &r, s->chord, n, rate);
        if (t == GSEC_TAG) { /* the button: cadence on the last slots */
            MgChord tmp[4];
            gen_phrase(&h, &r, tmp, 2, th->final_cad == GCAD_AEOL ? GCAD_PAC : th->final_cad, 0);
            s->chord[n - 2] = tmp[0]; s->chord[n - 1] = tmp[1];
        }
        s->prog_src = 0;
    } else {
        int k = t == GSEC_B ? 0 : t == GSEC_BRK ? 2 : 1, j;
        /* a pool progression, repeated with a fresh cadence on its last phrase */
        for (i = 0; i < n; i++) s->chord[i] = p->pprog[k][(rate == 2 ? i >> 1 : i) & 3];
        if (s->var >= 2 || rc(&r, 30)) {
            MgChord tmp[8];
            int cad = t == GSEC_C ? GCAD_HC : rc(&r, 50) ? GCAD_DC : GCAD_HC;
            gen_phrase(&h, &r, tmp, 4, cad, 1);
            for (j = 0; j < 4 && n - 4 * rate + j * rate >= 0; j++) {
                int b = n - 4 * rate + j * rate;
                int q;
                for (q = 0; q < rate; q++) s->chord[b + q] = tmp[j];
            }
        }
        s->prog_src = (uint8_t)(k + 1);
    }
    /* make 2-per-bar rhythm move: the second half may be a neighbour chord */
    /* ideas: sentence or period */
    nideas = s->bars >> 1;
    if (nideas < 1) nideas = 1;
    sentence = rc(&r, 50);
    for (i = 0; i < nideas && i < MG_MAX_BARS / 2; i++) {
        int id = GID_FREE, xf = GXF_LIT, sh = 0;
        int last = i == nideas - 1;
        switch (t) {
        case GSEC_A: case GSEC_PEAK:
            if (i == 0) id = GID_STATE;
            else if (last) id = GID_CAD;
            else if (sentence) { id = i == 1 ? GID_SEQ : GID_FRAG; sh = i == 1 ? (rc(&r, 50) ? 1 : -1) : 0; }
            else id = i == 1 ? GID_FREE : GID_STATE;
            if (s->var >= 1 && id == GID_STATE && i > 0) { id = GID_VAR; xf = GXF_ORN + 0; }
            break;
        case GSEC_A2:
            if (last) id = GID_CAD;
            else if (i == 0) { id = GID_VAR; xf = s->var >= 2 ? GXF_CONT : GXF_RHY; }
            else if (i == 1) { id = GID_SEQ; sh = rc(&r, 50) ? 2 : -1; }
            else id = sentence ? GID_FRAG : GID_STATE;
            break;
        case GSEC_B:
            if (last) id = GID_CAD;
            else if (i == 0) id = GID_STATE;
            else { id = GID_VAR; xf = rc(&r, 50) ? GXF_RHY : GXF_RETRO; }
            break;
        case GSEC_C:
            if (last) id = GID_CAD;
            else { id = i == 0 ? GID_STATE : GID_SEQ; sh = -i; }
            break;
        case GSEC_BRK:  id = i == 0 ? GID_FRAG : GID_REST; break;
        case GSEC_BUILD: id = GID_SEQ; sh = i + 1; break;
        case GSEC_TAG:  id = GID_CAD; break;
        default:       id = GID_REST; break;
        }
        if (s->var >= 3 && id == GID_VAR && rc(&r, 40)) xf = GXF_INV;
        s->idea[i] = (uint8_t)id; s->xf[i] = (uint8_t)xf; s->shift[i] = (int8_t)sh;
    }
    s->motif = (uint8_t)(t == GSEC_B ? 1 : t == GSEC_C ? 2 : 0);
    /* melody-first: where a head is quoted over generated harmony, the chord
     * on its strong beats is re-chosen to contain the head note (reharm pool) */
    if (s->prog_src != 0) {
        const MgNote *mn = s->motif == 0 ? th->motif[0].n : p->pm[s->motif - 1];
        int mlen = s->motif == 0 ? th->motif[0].len : p->pm_len[s->motif - 1];
        int head = s->motif == 1 ? 2 : th->motif[0].head;
        for (i = 0; i < nideas && i < MG_MAX_BARS / 2; i++) {
            int id = s->idea[i], pos = 0, k;
            if (id != GID_STATE && id != GID_VAR && id != GID_SEQ && id != GID_FRAG) continue;
            for (k = 0; k < head && k < mlen; k++) {
                int bar = pos >= th->steps ? 1 : 0, step = pos - bar * th->steps;
                if (((th->steps == 12 ? 0x001 : 0x0101) >> (step & 15)) & 1) {
                    int slot = ((i << 1) + bar) * rate + (rate > 1 && step >= (th->steps >> 1) ? 1 : 0);
                    int d = mn[k].deg + s->shift[i], pcn = pc12(deg_semi(s->mode, d) + mn[k].acc);
                    if (slot < n && !(chord_pcs(s->chord[slot]) & (1u << pcn))) {
                        int cand[3], j, best = -1, f0 = GCH_FN(s->chord[slot]);
                        cand[0] = d; cand[1] = d - 2; cand[2] = d - 4;
                        for (j = 0; j < 3; j++) {
                            int dd = cand[j];
                            while (dd < 0) dd += 7;
                            while (dd >= 7) dd -= 7;
                            if (best < 0 || fn_of(s->mode, dd) == f0) { best = dd; if (fn_of(s->mode, dd) == f0) break; }
                        }
                        s->chord[slot] = make_chord(&h, best, &r);
                    }
                }
                pos += mn[k].dur;
            }
        }
    }
    s->lead_oct = (uint8_t)(t == GSEC_B && rc(&r, 40) ? 1 : 0);
    s->stems = stems_for(s->energy, t);
    s->cad_mid = (uint8_t)(s->bars >= 8 ? GCAD_HC : GCAD_PAC);
    for (i = 0; i < s->bars; i++)
        if (i == s->bars - 1 || ((i & 3) == 3 && rc(&r, 20 + s->var * 15))) s->fills |= (uint8_t)(1u << (i & 7));
    {   /* optional (probabilistic) steps of the style's drum DNA */
        int l, k2;
        for (l = 0; l < 6; l++) {
            uint16_t m = 0;
            for (k2 = 0; k2 < 16; k2++) { int pr = st->dna->p[l][k2]; if (pr > 0 && pr < 10) m |= (uint16_t)(1u << k2); }
            s->opt[l] = m;
        }
    }
    /* groove: section base pattern from the pool (A-family / others), mutated */
    {
        const uint16_t *g = is_anchor(t) ? p->pgroove[0] : p->pgroove[1];
        int l;
        for (l = 0; l < 6; l++) s->lane[l] = g[l];
        if (s->var >= 1) {
            int l2 = rr(&r, 6), st2 = rr(&r, 16);
            int pr = st->dna->p[l2][st2];
            if (pr > 0 && pr < 10) s->lane[l2] ^= (uint16_t)(1u << st2);
        }
    }
    /* history */
    p->hist[3] = p->hist[2]; p->hist[2] = p->hist[1]; p->hist[1] = p->hist[0]; p->hist[0] = (uint8_t)t;
    p->since_anchor = (uint8_t)(is_anchor(t) ? 0 : p->since_anchor + 1);
    if (p->type_passes[t] < 255) p->type_passes[t]++;
    p->sec_count++;
    p->bar_in_sec = 0;
    p->work += 300;
}

/* ---- bar realisation -------------------------------------------------------------- */
typedef struct {
    MgPiece *p;
    MgSection *s;
    const MgStyle *st;
    MgBar *out;
    int steps, bar, unit, base;      /* base: MIDI of the tonic in the lead octave */
    int strong;                      /* bitmask of strong steps                  */
    int salt, hint;                  /* guard re-roll; low-energy theme hint     */
} BCtx;

static void emit_u(BCtx *c, int role, int step, int dur_u, int note, int vel, int inst)
{
    MgBar *b = c->out;
    int at, dur;
    if (!(c->s->stems & (1u << role)) && role != GR_ECHO) return;
    if (b->n >= MG_BAR_EV || note <= 0 || note > 127 || dur_u <= 0) return;
    at = step * 12;
    if (c->st->swing && (step & 3) == 2) at += c->st->swing;          /* shuffle */
    dur = dur_u;
    if (at >= c->unit) return;
    b->ev[b->n].at = (uint16_t)at;
    b->ev[b->n].dur = (uint16_t)dur;
    b->ev[b->n].role = (uint8_t)role;
    b->ev[b->n].note = (uint8_t)note;
    b->ev[b->n].vel = (uint8_t)(vel < 1 ? 1 : vel > 63 ? 63 : vel);
    b->ev[b->n].inst = (uint8_t)inst;
    b->n++;
    {   /* incremental signature: rotate-xor, no multiply */
        uint32_t x = (uint32_t)at | (uint32_t)dur << 10 | (uint32_t)note << 20 | (uint32_t)role << 27;
        x ^= (uint32_t)inst << 4 ^ (uint32_t)(vel >> 2) << 14;
        b->sig = ((b->sig << 5) | (b->sig >> 27)) ^ x;
        if (role == GR_LEAD) b->sig_lead = ((b->sig_lead << 5) | (b->sig_lead >> 27)) ^ x;
    }
    c->p->work += 4;
}
#define emit(c, role, step, ds, note, vel, inst) emit_u(c, role, step, (ds) * 12, note, vel, inst)

static MgChord chord_at(const BCtx *c, int bar, int step)
{
    int rate = c->st->chord_rate, slot = bar * rate + (rate > 1 && step >= (c->steps >> 1) ? 1 : 0);
    if (slot >= c->s->bars * rate) slot = c->s->bars * rate - 1;
    return c->s->chord[slot];
}

/* nearest degree to d whose pitch class is in the chord; adjusts acc for chromatic chords */
static int fit_deg(const BCtx *c, int d, MgChord ch, int8_t *acc)
{
    uint16_t pcs = chord_pcs(ch);
    int k;
    for (k = 0; k < 4; k++) {
        int a = d + k, b2 = d - k;
        if (pcs & (1u << pc12(deg_semi(c->s->mode, a)))) { *acc = 0; return a; }
        if (pcs & (1u << pc12(deg_semi(c->s->mode, b2)))) { *acc = 0; return b2; }
    }
    if (pcs & (1u << pc12(deg_semi(c->s->mode, d) + 1))) { *acc = 1; return d; }
    if (pcs & (1u << pc12(deg_semi(c->s->mode, d) - 1))) { *acc = -1; return d; }
    *acc = 0;
    return d;
}

/* realise the lead notes of idea i (2 bars) into ln[] (steps from the idea start) */
static int build_idea(BCtx *c, int i, LNote *ln)
{
    MgSection *s = c->s;
    MgPiece *p = c->p;
    const MgTheme *th = p->th;
    const MgNote *mn;
    int mlen, head, n = 0, pos = 0, total = 2 * c->steps, id = s->idea[i], xf = s->xf[i];
    int sh = s->shift[i], k, anchor_h = (s->prog_src == 0);
    Rng r;
    rs(&r, mix(s->seed, 0x1D0000u + (uint32_t)i));
    if (s->motif == 0 || p->pm_len[s->motif - 1] == 0) { mn = th->motif[0].n; mlen = th->motif[0].len; head = th->motif[0].head; }
    else { mn = p->pm[s->motif - 1]; mlen = p->pm_len[s->motif - 1]; head = s->motif == 2 ? th->motif[0].head : 2; }
    if (th->motif[0].bars == 1) total = c->steps;   /* 1-bar themes: 1-bar ideas */
    if (id == GID_REST) return 0;
    if (id == GID_STATE || id == GID_VAR || id == GID_SEQ) {
        for (k = 0; k < mlen && pos < total; k++) {
            LNote x;
            x.at = (int16_t)pos; x.dur = mn[k].dur; x.deg = (int8_t)(mn[k].deg + sh); x.acc = mn[k].acc;
            x.fl = (uint8_t)(k < head ? GNF_HEAD : 0);
            if (mn[k].fl & GNF_REST) x.fl |= GNF_REST;
            ln[n++] = x; pos += mn[k].dur;
        }
    } else if (id == GID_FRAG) {
        int rep = 0;
        while (pos < total && n < 30) {
            for (k = 0; k < head && pos < total; k++) {
                LNote x;
                x.at = (int16_t)pos; x.dur = mn[k].dur; x.deg = (int8_t)(mn[k].deg + rep * (sh ? sh : (rc(&r, 50) ? 1 : -1)));
                x.acc = 0; x.fl = (uint8_t)(rep == 0 ? GNF_HEAD : 0);
                ln[n++] = x; pos += mn[k].dur;
            }
            if (rep == 1 && pos < total) {   /* answer: hold */
                LNote x; x.at = (int16_t)pos; x.dur = (int16_t)(total - pos); x.deg = ln[n - 1].deg; x.acc = 0; x.fl = GNF_REST;
                ln[n++] = x; pos = total;
            }
            rep++;
        }
    } else if (id == GID_FREE || id == GID_CAD) {
        int d = 2 + rr(&r, 3), dir = rc(&r, 50) ? 1 : -1, lim = id == GID_CAD ? (head < 4 ? head : 4) : mlen;
        static const int8_t moves[8] = { -2, -1, -1, 1, 1, 2, 0, 1 };
        for (k = 0; k < lim && pos < total; k++) {
            LNote x;
            int strong = (c->strong >> (pos & 15)) & 1;
            x.at = (int16_t)pos; x.dur = mn[k].dur; x.acc = 0; x.fl = 0;
            if (strong) d = d + rr(&r, 3) - 1;
            else { int mv = moves[rr(&r, 8)]; d += dir > 0 ? mv : -mv; }
            if (d > 9) { d = 8; dir = -1; }
            if (d < -1) { d = 0; dir = 1; }
            if (rc(&r, 18)) dir = -dir;
            x.deg = (int8_t)d;
            ln[n++] = x; pos += mn[k].dur;
        }
        if (id == GID_CAD && pos < total) {
            /* approach + the cadence note, held to the end */
            int cad_last = s->bars <= 2 ? th->final_cad : (i == (s->bars >> 1) - 1 ? th->final_cad : GCAD_HC);
            int target = (cad_last == GCAD_HC) ? (rc(&r, 50) ? 4 : 1) : (rc(&r, 30) ? 2 : 0);
            int appr = target + (d > target ? 1 : -1);
            LNote x;
            if (s->type == GSEC_B || s->type == GSEC_C) target = rc(&r, 50) ? 4 : 1;
            if (pos < c->steps) {
                x.at = (int16_t)pos; x.dur = (int16_t)(c->steps - pos); x.deg = (int8_t)appr; x.acc = 0; x.fl = 0;
                ln[n++] = x; pos = c->steps;
            }
            x.at = (int16_t)pos; x.dur = (int16_t)(total - pos); x.deg = (int8_t)target; x.acc = 0; x.fl = 0;
            ln[n++] = x; pos = total;
        }
    }
    /* tail transforms (head protected) */
    if (id == GID_VAR || (id == GID_SEQ && s->var >= 2)) {
        int pivot = head > 0 && head <= n ? ln[head - 1].deg : ln[0].deg;
        if (xf == GXF_INV) for (k = head; k < n; k++) ln[k].deg = (int8_t)(2 * pivot - ln[k].deg);
        else if (xf == GXF_RETRO) {
            int a = head, b2 = n - 1;
            while (a < b2) { int8_t t2 = ln[a].deg; ln[a].deg = ln[b2].deg; ln[b2].deg = t2; a++; b2--; }
        } else if (xf == GXF_CONT) {
            for (k = head; k < n; k++) {
                int iv = ln[k].deg - ln[k - 1 >= 0 ? k - 1 : 0].deg;
                int mag = 1 + rr(&r, 3);
                ln[k].deg = (int8_t)(ln[k - 1].deg + (iv > 0 ? mag : iv < 0 ? -mag : 0));
            }
        } else if (xf == GXF_RHY) {
            for (k = head; k + 1 < n; k++)
                if (ln[k].dur == 2 && ln[k + 1].dur == 2 && rc(&r, 50)) {
                    ln[k].dur = 3; ln[k + 1].at = (int16_t)(ln[k + 1].at + 1); ln[k + 1].dur = 1; k++;
                }
        } else if (xf == GXF_ORN) {
            /* grace notes are added at emit time (needs a free slot) */
        }
    }
    /* strong beats sit on chord tones (head notes too when the harmony is not the anchor) */
    for (k = 0; k < n; k++) {
        int bar = (ln[k].at >= c->steps) ? 1 : 0, step = ln[k].at - bar * c->steps;
        int strong = (c->strong >> (step & 15)) & 1;
        if (ln[k].fl & GNF_REST) continue;
        if (ln[k].fl & GNF_HEAD) continue;                 /* the head is never bent      */
        if (id == GID_STATE && anchor_h) continue;         /* authored against its chords */
        if (strong) {
            int8_t acc;
            MgChord ch = chord_at(c, (i << 1) + bar, step);
            ln[k].deg = (int8_t)fit_deg(c, ln[k].deg, ch, &acc);
            ln[k].acc = acc;
        }
    }
    c->p->work += (uint32_t)(n * 6);
    return n;
}

static int lead_base(const BCtx *c)
{
    const MgTheme *th = c->p->th;
    int b = 60 + c->s->key;
    while (b + 4 > th->lead_hi) b -= 12;
    while (b < th->lead_lo) b += 12;
    if (c->s->lead_oct == 1 && b + 16 <= th->lead_hi + 5) b += 12;
    return b;
}

static void do_lead(BCtx *c)
{
    LNote ln[32];
    int i = c->bar >> 1, half = c->bar & 1, n, k, base = c->base, gate = c->st->lead_gate, prev = 0;
    int ideasteps = c->p->th->motif[0].bars == 1 ? c->steps : 2 * c->steps;
    int lo = (ideasteps == c->steps) ? 0 : half * c->steps;
    MgSection *s = c->s;
    Rng r;
    if (c->p->th->motif[0].bars == 1) { i = c->bar; lo = 0; }
    if (i >= MG_MAX_BARS / 2) return;
    n = build_idea(c, i, ln);
    rs(&r, mix(s->seed, 0x0AA00000u + (uint32_t)c->bar));
    for (k = 0; k < n; k++) {
        int at = ln[k].at - lo, dur = ln[k].dur, note, vel;
        if (at < 0 || at >= c->steps) continue;
        if (ln[k].fl & GNF_REST) continue;
        if (at + dur > c->steps) dur = c->steps - at;          /* ties into the next bar: cut here */
        note = base + deg_semi(s->mode, ln[k].deg) + ln[k].acc;
        if (prev > 0 && !(ln[k].fl & GNF_HEAD)) {          /* no leap wider than a 6th */
            while (note - prev > 9) note -= 12;
            while (prev - note > 9) note += 12;
        }
        while (note > c->p->th->lead_hi + 4) note -= 12;
        while (note < c->p->th->lead_lo - 4) note += 12;
        prev = note;
        vel = ((c->strong >> (at & 15)) & 1) ? 48 : 40;
        vel += rr(&r, 5) - 2;
        if ((ln[k].fl & GNF_HEAD) && k == 0 && half == 0 && (s->motif != 1) &&
            (s->idea[i] == GID_STATE || s->idea[i] == GID_VAR || s->idea[i] == GID_FRAG || s->idea[i] == GID_SEQ)) c->out->head_stated = 1;
        if (c->hint) vel -= 12;
        /* ornament: a grace from the upper neighbour on long notes */
        if (s->var >= 1 && dur >= 4 && s->xf[i] == GXF_ORN && rc(&r, 35) && !(ln[k].fl & GNF_HEAD)) {
            int up = base + deg_semi(s->mode, ln[k].deg + 1);
            emit(c, GR_LEAD, at, 1, up, vel - 6, mg_styles[s->dial].lead_inst);
            at++; dur--;
        }
        {   /* gate in 1/8 of the note, in units */
            int gu = (dur * 12 * gate) >> 3;
            emit_u(c, GR_LEAD, at, gu < 6 ? 6 : gu, note, vel, c->hint ? 0x11 : c->st->lead_inst);
        }
        /* counter: a diatonic third (metal: twin lead) or sixth below, held */
        if ((s->stems & GBIT(GR_COUNTER)) && !c->hint) {
            int cd = c->st->power ? ln[k].deg - 2 : ln[k].deg - (rc(&r, 50) ? 2 : 5);
            if (c->st->power || ((c->strong >> (at & 15)) & 1)) {
                int8_t acc;
                MgChord ch = chord_at(c, c->bar, at);
                cd = fit_deg(c, cd, ch, &acc);
                emit(c, GR_COUNTER, at, c->st->power ? dur : (dur < 4 ? 4 : dur), base - 12 + deg_semi(s->mode, cd) + acc + (c->st->power ? 12 : 0),
                     vel - 10, c->st->ctr_inst);
            }
        }
        /* echo-by-repeat: a dotted 8th later, quieter */
        if ((s->stems & GBIT(GR_ECHO)) && !c->hint && at + 3 < c->steps)
            emit(c, GR_ECHO, at + 3, dur < 2 ? dur : 2, note, vel - 22, 0x11);
    }
}

static void do_harm(BCtx *c)
{
    MgSection *s = c->s;
    int rate = c->st->chord_rate, slot_len = rate == 2 ? c->steps >> 1 : c->steps, q, k;
    int pat_sec, inv_sec;
    Rng r;
    rs(&r, s->seed ^ 0x4A4A4A4Au);
    pat_sec = rr(&r, 3); inv_sec = rr(&r, 3);           /* per section: arp shape, inversion */
    rs(&r, mix(s->seed, 0x4A000000u + (uint32_t)c->bar * 4u + (uint32_t)c->salt));
    for (q = 0; q < rate; q++) {
        MgChord ch = chord_at(c, c->bar, q * slot_len);
        int qual = GCH_Q(ch), inv = (s->var >= 1) ? inv_sec + c->salt : 0;
        if (inv >= 3) inv -= 3;
        int root = 48 + pc12(s->key + GCH_ROOT(ch)) + (cq_iv[qual][inv] > 0 && inv ? cq_iv[qual][inv] - 12 : 0);
        int inst = 0x20 | qual;
        int st0 = q * slot_len;
        switch (c->st->harm_style) {
        case GHS_OFFBEAT:
            for (k = 2; k < slot_len; k += 4) emit(c, GR_HARM, st0 + k, 2, root, 34, inst);
            if (s->var >= 1 && rc(&r, 25 + c->salt * 35)) emit(c, GR_HARM, st0 + slot_len - 1, 1, root, 30, inst);  /* pickup */
            if (s->var >= 2 && rc(&r, 20)) emit(c, GR_HARM, st0, 1, root, 26, inst);                                 /* downbeat chirp */
            break;
        case GHS_ARP8: {
            int pat = c->salt ? rr(&r, 3) : pat_sec, tones[4], nt = 0, j, up = (s->var >= 2 ? rr(&r, 2) : 0), alt = 1;
            for (j = 0; j < 4 && cq_iv[qual][j] >= 0; j++) tones[nt++] = cq_iv[qual][j];
            for (k = 0, j = 0; k < slot_len; k += 2) {
                int idx;
                if (pat == 0) idx = up;                      /* up            */
                else if (pat == 1) idx = nt - 1 - up;        /* down          */
                else idx = (k & 2) ? alt : 0;                /* alberti-ish   */
                if (++up >= nt) up = 0;
                if (k & 2) { if (++alt >= nt) alt = 1; }
                emit(c, GR_HARM, st0 + k, 2, root + tones[idx] + (s->dial == 0 ? 12 : 0), 30, 0x11);
            }
            break; }
        case GHS_PAD:
            emit(c, GR_HARM, st0, slot_len, root, 28, inst);
            break;
        case GHS_POWER: {
            uint16_t m = (uint16_t)(s->lane[0] | 0x0101);
            if (s->var >= 1) {                /* per section: a chug rhythm of its own */
                Rng r2;
                int t2;
                rs(&r2, s->seed ^ 0x9C9C9C9Cu ^ (uint32_t)c->salt);
                for (t2 = 0; t2 < 2 + s->var; t2++) m ^= (uint16_t)(1u << (rr(&r2, 8) << 1));
                m |= 0x0001;
            }
            for (k = 0; k < slot_len; k += 2)
                if (m & (1u << ((st0 + k) & 15))) emit(c, GR_HARM, st0 + k, 2, root - 12 + 12, 40, 0x20 | GCQ_POW);
            break; }
        default:
            emit(c, GR_HARM, st0 + 4, 4, root, 32, inst);
            emit(c, GR_HARM, st0 + 8, 4, root, 30, inst);
            break;
        }
    }
    if (s->stems & GBIT(GR_PAD)) {
        MgChord ch = chord_at(c, c->bar, 0);
        emit(c, GR_PAD, 0, c->steps, 60 + pc12(s->key + GCH_ROOT(ch)), 22, 0x30 | GCH_Q(ch));
    }
}

static void do_bass(BCtx *c)
{
    MgSection *s = c->s;
    int k, rate = c->st->chord_rate, slot_len = rate == 2 ? c->steps >> 1 : c->steps;
    int style = c->p->th->riff ? GBS_RIFF : c->st->bass_style;
    Rng r;
    rs(&r, mix(s->seed, 0xBA550000u + (uint32_t)c->bar * 4u + (uint32_t)c->salt));
    if (c->steps == 12 && style != GBS_RIFF) style = GBS_WALTZ;
    for (k = 0; k < rate; k++) {
        MgChord ch = chord_at(c, c->bar, k * slot_len), nx;
        int root = 36 + pc12(s->key + GCH_ROOT(ch)), fifth = root + 7, st0 = k * slot_len, j;
        int last_slot = (k == rate - 1);
        nx = last_slot ? chord_at(c, c->bar + 1 < s->bars ? c->bar + 1 : c->bar, 0) : chord_at(c, c->bar, (k + 1) * slot_len);
        switch (style) {
        case GBS_ROOT5:
            for (j = 0; j < slot_len; j += 4) emit(c, GR_BASS, st0 + j, 4, (j & 4) ? fifth - 12 + 12 * ((j >> 3) & 1) : root + 12 * ((j >> 3) & 1), 46, c->st->bass_inst);
            break;
        case GBS_OCT8:
            for (j = 0; j < slot_len; j += 2) emit(c, GR_BASS, st0 + j, 2, root + ((j & 2) ? 12 : 0), 44, c->st->bass_inst);
            break;
        case GBS_WALK: {
            int tones[3] = { root, root + (cq_iv[GCH_Q(ch)][1] > 0 ? cq_iv[GCH_Q(ch)][1] : 4), fifth };
            int nroot = 36 + pc12(s->key + GCH_ROOT(nx));
            int ti = 0;
            for (j = 0; j < slot_len; j += 4) {
                int nt = (j + 4 >= slot_len) ? nroot + (rc(&r, 50) ? 1 : -1) : tones[ti];
                if (++ti >= 3) ti = 0;
                emit(c, GR_BASS, st0 + j, 4, nt, 44, c->st->bass_inst);
            }
            break; }
        case GBS_GALLOP:
            for (j = 0; j < slot_len; j += 4) {
                emit(c, GR_BASS, st0 + j, 2, root, 50, c->st->bass_inst);
                emit(c, GR_BASS, st0 + j + 2, 1, root, 42, c->st->bass_inst);
                emit(c, GR_BASS, st0 + j + 3, 1, root, 42, c->st->bass_inst);
            }
            break;
        case GBS_WALTZ:
            emit(c, GR_BASS, st0, 4, root, 46, c->st->bass_inst);
            if (rc(&r, 40 + s->var * 10)) emit(c, GR_BASS, st0 + 8, 4, fifth - 12, 36, c->st->bass_inst);
            break;
        case GBS_RIFF: {
            const MgMotif *m = &c->p->th->motif[1];
            int pos = 0, chroot = GCH_ROOT(ch);
            int keep_tonic = (GCH_DEG(ch) == 0);
            for (j = 0; j < m->len && pos < slot_len; j++) {
                int nt = 36 + s->key + chroot + deg_semi(GMODE_MINOR, m->n[j].deg) + m->n[j].acc;
                if (!keep_tonic && m->n[j].deg == 0 && rc(&r, 15)) nt += 12;
                emit(c, GR_BASS, st0 + pos, m->n[j].dur, nt, m->n[j].deg == 0 ? 44 : 50, c->st->bass_inst);
                pos += m->n[j].dur;
            }
            break; }
        default:
            emit(c, GR_BASS, st0, slot_len, root, 44, c->st->bass_inst);
            break;
        }
        /* variation: an approach into the next chord, or an octave pop */
        if (last_slot && style != GBS_RIFF && style != GBS_GALLOP && s->var >= 1 && rc(&r, 30 + c->salt * 35)) {
            int nroot = 36 + pc12(s->key + GCH_ROOT(nx)), t = st0 + slot_len - 2;
            emit(c, GR_BASS, t, 2, rc(&r, 50) ? nroot - 1 : nroot + 7 - 12, 40, c->st->bass_inst);
        } else if (s->var >= 2 && rc(&r, 18)) emit(c, GR_BASS, st0 + 6, 1, root + 12, 34, c->st->bass_inst);
    }
}

static void do_drums(BCtx *c, int salt)
{
    MgSection *s = c->s;
    int l, k, e = s->energy, fill = (s->fills >> (c->bar & 7)) & 1;
    uint16_t lane[6];
    static const uint8_t lane_dr[6] = { GDR_KICK, GDR_SNARE, GDR_HAT, GDR_OPEN, GDR_CLAP, GDR_TOM };
    Rng r;
    rs(&r, mix(s->seed, 0xD0000000u + (uint32_t)c->bar * 16u + (uint32_t)salt));
    for (l = 0; l < 6; l++) lane[l] = s->lane[l];
    /* per-bar mutation of the optional steps */
    if (s->var >= 1)                  /* random 16-bit masks: density 1/4 (var 3, re-roll) .. 1/8 */
        for (l = 0; l < 6; l++) {
            uint32_t m = rn(&r) & rn(&r);
            if (s->var < 3 && !salt) m &= rn(&r);
            lane[l] ^= (uint16_t)(m & s->opt[l]);
        }
    if (e <= 2) { lane[2] &= 0x1111; lane[3] = 0; lane[5] = 0; }      /* thin */
    if (e >= 4) lane[2] |= (uint16_t)(rc(&r, 50) ? 0x5555 : 0x4444);
    if (fill) {                                   /* the last beat (4 steps) is a fill */
        int type = rr(&r, 4), from = c->steps - 4;
        for (l = 0; l < 6; l++) lane[l] &= (uint16_t)((1u << from) - 1);
        for (k = from; k < c->steps; k++) {
            int v = 30 + (k - from) * 6;
            if (type == 0) emit(c, GR_DRUMS, k, 1, GDR_SNARE, v, 0);
            else if (type == 1) emit(c, GR_DRUMS, k, 1, GDR_TOM, v, 64 - (k - from) * 8);
            else if (type == 2) { if (!(k & 1)) emit(c, GR_DRUMS, k, 1, (k & 2) ? GDR_SNARE : GDR_KICK, v, 0); }
            else if (k == from) emit(c, GR_DRUMS, k, 2, GDR_CLAP, v, 0);
        }
    }
    if (c->bar == 0 && s->type != GSEC_INTRO && s->energy >= 3) emit(c, GR_DRUMS, 0, 4, GDR_CRASH, 50, 0);
    for (l = 0; l < 6; l++)
        for (k = 0; k < c->steps; k++)
            if (lane[l] & (1u << k)) {
                int acc = (k & 3) == 0 ? 50 : (k & 1) == 0 ? 42 : 34;
                emit(c, GR_DRUMS, k, 1, lane_dr[l], acc + rr(&r, 7) - 3, 0);
            }
    /* ghost snares at var >= 2 */
    if (s->var >= 2 && e >= 3)
        for (k = 1; k < c->steps; k += 2) if (rc(&r, 6)) emit(c, GR_DRUMS, k, 1, GDR_SNARE, 16, 0);
}

static void realise(MgPiece *p, MgBar *out, int salt)
{
    BCtx c;
    MgSection *s = &p->cur;
    c.p = p; c.s = s; c.st = &mg_styles[s->dial]; c.out = out;
    c.steps = p->th->steps; c.bar = p->bar_in_sec; c.unit = c.steps * 12;
    c.strong = c.steps == 12 ? 0x001 : 0x0101;
    memset(out, 0, sizeof(*out) - sizeof(out->ev));
    out->len = (uint16_t)c.unit; out->sec_type = s->type; out->bar_in_sec = (uint8_t)c.bar;
    out->energy = s->energy; out->key = s->key;
    c.base = lead_base(&c);
    c.salt = salt; c.hint = 0;
    if (s->stems & GBIT(GR_LEAD)) do_lead(&c);
    else if (is_anchor(s->type) && s->energy >= 1 && c.bar < 2) {
        /* identity floor: below I3 the theme head is still hinted on the music box */
        uint8_t keep = s->stems;
        c.hint = 1; s->stems |= GBIT(GR_LEAD);
        do_lead(&c);
        s->stems = keep; c.hint = 0;
    }
    if (s->stems & (GBIT(GR_HARM) | GBIT(GR_PAD))) do_harm(&c);
    if (s->stems & GBIT(GR_BASS)) do_bass(&c);
    if (s->stems & GBIT(GR_DRUMS)) do_drums(&c, salt);
    if ((c.bar & 3) == 3 || c.bar == s->bars - 1) out->cadence = 1;
}

/* ---- public ---------------------------------------------------------------------- */
void mg_start(MgPiece *p, const MgTheme *th, uint32_t seed)
{
    int k;
    memset(p, 0, sizeof(*p));
    p->th = th;
    p->seed = seed;
    p->intensity = 3;
    p->dial = th->dial;
    p->hist[0] = GSEC_TAG;
    p->arc = (uint8_t)(h32(seed) & 3);
    for (k = 0; k < MG_POOL_MOTIF; k++) derive_motif(p, k, mix(seed, 0xA0u + (uint32_t)k));
    for (k = 0; k < MG_POOL_PROG; k++) gen_pool_prog(p, k, mix(seed, 0xB0u + (uint32_t)k));
    gen_groove(p, p->pgroove[0], mg_styles[p->dial].dna, mix(seed, 0xC0u));
    gen_groove(p, p->pgroove[1], mg_styles[p->dial].dna, mix(seed, 0xC1u));
}

void mg_set(MgPiece *p, int intensity, int dial)
{
    p->intensity = (uint8_t)(intensity < 0 ? 0 : intensity > 5 ? 5 : intensity);
    if (dial != p->dial) {           /* a new band: regenerate the grooves (at the next section) */
        p->dial = (uint8_t)(dial < 0 ? 0 : dial > 4 ? 4 : dial);
        gen_groove(p, p->pgroove[0], mg_styles[p->dial].dna, mix(p->seed, 0xC0u + p->dial));
        gen_groove(p, p->pgroove[1], mg_styles[p->dial].dna, mix(p->seed, 0xC1u + p->dial));
    }
}

int mg_key_of(const MgPiece *p) { return p->cur.key; }

void mg_next_bar(MgPiece *p, MgBar *out)
{
    int tries;
    uint32_t w;
    if (p->sec_count == 0 || p->bar_in_sec >= p->cur.bars) plan_section(p);
    for (tries = 0; tries < 3; tries++) {
        int i, hit = 0;
        realise(p, out, tries);
        if (out->n == 0) out->sig = 0;                 /* silence is a rest, not a repeat */
        else { out->sig ^= (uint32_t)out->key << 24; if (!out->sig) out->sig = 1; }
        w = mix(mix(mix(out->sig, p->sig[0]), p->sig[1]), p->sig[2]);
        for (i = 0; i < MG_SIGRING; i++) if (p->win[i] == w) { hit = 1; break; }
        if (!hit || out->n == 0) break;
        p->guard_hits++;
    }
    p->sig[2] = p->sig[1]; p->sig[1] = p->sig[0]; p->sig[0] = out->sig;
    p->win[p->win_pos] = w;
    p->win_pos = (uint8_t)((p->win_pos + 1) & (MG_SIGRING - 1));
    p->bar_in_sec++;
    p->bar_total++;
}
