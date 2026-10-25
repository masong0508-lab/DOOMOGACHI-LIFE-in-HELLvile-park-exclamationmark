/* musgen.c - the build-time composer's front end (owner: MU1T;
 * docs/design/music/SPEC.md sec.8.3, 16.2, D2, D10, CF-38).
 *
 *   musgen fingerprint [--check]      the 4 reference hashes (MU1T acceptance:
 *                                     37d71b12 8d898ef0 c33cfa26 bdcb0bd2 on gcc
 *                                     and clang)
 *   musgen gen CUE                    realise CUE's GEN slots -> .dms on stdout
 *   musgen wav CUE SECONDS OUT.wav [steady|walk|morph]
 *                                     the composer's own stream (listening)
 *   musgen reel CUE OUT.wav           the accepted GEN variants in context:
 *                                     A (the theme, V0) | variant | A | ...
 *   musgen list                       the cue configurations
 *
 * CUE: title, hahaland, invasion, home (the four M1.5 hubs). `tools/mmlc.py
 * --gen MC_x` runs it and writes core/music/gen/<area>/<cue>.dms; the
 * committed .dms is the source (sec.8.3: curation by ear), so CI never runs
 * musgen. Host only: stdio is fine here; the composer (mg_gen.c) itself is
 * integer-only C99 without stdio, like the core.
 *
 * GEN realisation (sec.8.3): for each slot the composer plays its theme at
 * the cue's dial and intensity over a fixed list of seeds; every section of
 * the slot's form type (B = ANSWER, C = DEV, BUILD, TAG) is a candidate.
 * Kept are the pitched roles the variants own (LEAD and COUNTER; HARM, BASS
 * and KIT stay graph pools and DNA) and the section's chord chart.
 * Families are chord charts: a slot's first n_fam distinct charts; variants
 * of one family share the harmony and differ melodically.
 *
 * The offline guard (D10) rejects a candidate when
 *   - any of its 4-bar M2 windows (pitched roles: role, onset on the 16th
 *     grid, pitch, length class; no velocity) equals a window of the
 *     authored material (the theme's V0 anchor), of another accepted variant
 *     or of another family;
 *   - it is a near-copy of an accepted variant of its slot: mean fuzzy
 *     similarity F >= 0.85 (CF-38's F: per role Jaccard(onsets) x
 *     Jaccard(pitch classes), LEAD 3, COUNTER 1);
 *   - a strong beat of its lead is not a chord tone (STRONG_CT, sec.8.5);
 *   - it is too thin (energy below 3: no lead) or not in the home key (a
 *     key lift is a runtime op).
 * Rejections are counted per slot and printed in the .dms header (MU4's
 * "rejected-GEN counts"). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mg.h"

/* ---- the fingerprint (proto host_main.c, 10 minutes, steady, seed 1) ------ */
static uint32_t ref_hash(const MgTheme *th)
{
    MgPiece p;
    MgBar b;
    int bars_per_min = th->steps == 16 ? th->bpm / 4 : th->bpm / 3, n = bars_per_min * 10, i, k;
    uint32_t h = 2166136261u;
    mg_start(&p, th, 0x1234u + 7u);
    for (i = 0; i < n; i++) {
        mg_set(&p, 3, th->dial);
        mg_next_bar(&p, &b);
        for (k = 0; k < b.n; k++) {
            const MgEv *e = &b.ev[k];
            h = (h ^ (e->at | (uint32_t)e->dur << 16)) * 16777619u;
            h = (h ^ ((uint32_t)e->role | (uint32_t)e->note << 8 | (uint32_t)e->vel << 16 | (uint32_t)e->inst << 24)) * 16777619u;
        }
    }
    return h;
}

static const uint32_t ref_expect[4] = { 0x37d71b12u, 0x8d898ef0u, 0xc33cfa26u, 0xbdcb0bd2u };

static int cmd_fingerprint(int check)
{
    const MgTheme *th[4] = { &mg_theme_hook, &mg_theme_riff, &mg_theme_home, &mg_theme_title };
    int i, ok = 1;
    for (i = 0; i < 4; i++) {
        uint32_t h = ref_hash(th[i]);
        printf("%s%08x", i ? " " : "", (unsigned)h);
        if (h != ref_expect[i]) ok = 0;
    }
    printf("\n");
    if (check && !ok) { fprintf(stderr, "musgen: fingerprint MISMATCH (expected 37d71b12 8d898ef0 c33cfa26 bdcb0bd2)\n"); return 1; }
    return 0;
}

/* ---- cue configurations ------------------------------------------------------ */
enum { TM_SENTENCE, TM_PERIOD, TM_ANSWER, TM_DEV, TM_BUILD, TM_TAG };     /* = MGT_* (mus_data.h) */
static const char *tmpl_name[] = { "SENTENCE", "PERIOD", "ANSWER", "DEV", "BUILD", "TAG" };
static const char *cad_name[] = { "PAC", "HC", "DC", "PLAGAL", "AEOL" };

typedef struct Slot { const char *name; uint8_t sec, tmpl, cad, n_var, n_fam; } Slot;
typedef struct CueCfg {
    const char *id, *mc, *area, *motif;
    const MgTheme *base;
    int key;                    /* -1 = the theme's own                      */
    int oct;                    /* lane octave offset (engine key = MIDI + 12 * oct) */
    int intensity, dial;
    int n_slot;
    Slot slot[3];
} CueCfg;

/* Phase 1 (sec.8.3): 4 variants per slot in 2 families. Slots per sec.15:
 * TITLE "+ G BUILD/TAG"; HAHALAND "+ G x3" (B answer, C development, BUILD);
 * INVASION "+ G x2" = B and C (its BUILD sequences put the protected riff
 * head on clashing power chords, so STRONG_CT rejects every one); HOME
 * "+ G C" plus a B answer (its C development is the augmented head, so the
 * guard keeps few distinct C variants). */
static const CueCfg cues[] = {
    { "title", "MC_TITLE", "world", "TITLE", &mg_theme_title, -1, 0, 3, 1, 2,
      { { "GEN_BUILD", GSEC_BUILD, TM_BUILD, GCAD_HC, 4, 2 }, { "GEN_TAG", GSEC_TAG, TM_TAG, GCAD_PAC, 4, 2 } } },
    { "hahaland", "MC_HAHALAND", "world", "HOOK", &mg_theme_hook, 5, 0, 3, 1, 3,
      { { "GEN_B", GSEC_B, TM_ANSWER, GCAD_HC, 4, 2 }, { "GEN_C", GSEC_C, TM_DEV, GCAD_HC, 4, 2 },
        { "GEN_BUILD", GSEC_BUILD, TM_BUILD, GCAD_HC, 4, 2 } } },
    { "invasion", "MC_INVASION", "combat", "RIFF", &mg_theme_riff, -1, 0, 4, 4, 2,
      { { "GEN_B", GSEC_B, TM_ANSWER, GCAD_HC, 4, 2 }, { "GEN_C", GSEC_C, TM_DEV, GCAD_HC, 4, 2 } } },
    { "home", "MC_HOME", "world", "HOME", &mg_theme_home, -1, -1, 4, 0, 2,
      { { "GEN_B", GSEC_B, TM_ANSWER, GCAD_HC, 4, 2 }, { "GEN_C", GSEC_C, TM_DEV, GCAD_HC, 4, 2 } } },
};
#define N_CUES ((int)(sizeof(cues) / sizeof(cues[0])))

static MgTheme theme_of(const CueCfg *c)
{
    MgTheme t = *c->base;
    if (c->key >= 0) t.key = (uint8_t)c->key;
    return t;
}

static const CueCfg *find_cue(const char *id)
{
    int i;
    for (i = 0; i < N_CUES; i++)
        if (!strcmp(cues[i].id, id) || !strcmp(cues[i].mc, id)) return &cues[i];
    return NULL;
}

/* ---- candidates ---------------------------------------------------------------- */
#define MAXB 8
typedef struct Cand {
    int bars, steps;
    MgBar bar[MAXB];
    MgChord chord[MAXB];
    uint8_t key, mode, var, energy, type;
    uint32_t seed;
    int section;
    uint32_t chart;             /* FNV of the chord chart = the family key */
} Cand;

static uint32_t fnv(uint32_t h, uint32_t v)
{
    int i;
    for (i = 0; i < 4; i++) { h ^= (v >> (i * 8)) & 0xFFu; h *= 16777619u; }
    return h;
}

static int is_pitched(int role) { return role == GR_LEAD || role == GR_COUNTER; }

static int len_class(int dur)
{
    int c = 0;
    while (dur > 12 && c < 7) { dur >>= 1; c++; }
    return c;
}

/* M2 signature of one bar: the pitched roles, velocity-free */
static uint32_t bar_m2(const MgBar *b)
{
    uint32_t h = 2166136261u;
    int k, any = 0;
    for (k = 0; k < b->n; k++) {
        const MgEv *e = &b->ev[k];
        if (!is_pitched(e->role)) continue;
        any = 1;
        h = fnv(h, (uint32_t)e->role | (uint32_t)(e->at / 12) << 4 | (uint32_t)e->note << 12 | (uint32_t)len_class(e->dur) << 20);
    }
    return any ? (h | 1u) : 0;
}

/* fuzzy similarity of two bars (x1000): CF-38's F over LEAD (3) and COUNTER (1) */
static void bar_feat(const MgBar *b, int role, uint32_t *ons, uint32_t *pcs)
{
    int k;
    *ons = 0; *pcs = 0;
    for (k = 0; k < b->n; k++) {
        const MgEv *e = &b->ev[k];
        if (e->role != role) continue;
        *ons |= 1u << ((e->at / 12) & 31);
        *pcs |= 1u << (e->note % 12);
    }
}
static int popc(uint32_t x) { int n = 0; while (x) { x &= x - 1; n++; } return n; }
static int jac(uint32_t a, uint32_t b) { int u = popc(a | b); return u ? popc(a & b) * 1000 / u : 1000; }
static int bar_sim(const MgBar *a, const MgBar *b)
{
    static const int roles[2] = { GR_LEAD, GR_COUNTER }, w[2] = { 3, 1 };
    int i, num = 0, den = 0;
    for (i = 0; i < 2; i++) {
        uint32_t oa, pa, ob, pb;
        bar_feat(a, roles[i], &oa, &pa);
        bar_feat(b, roles[i], &ob, &pb);
        if (!oa && !ob) continue;
        num += w[i] * jac(oa, ob) * jac(pa, pb) / 1000;
        den += w[i];
    }
    return den ? num / den : 1000;
}

/* chord tones of a chord (mirror of mg_gen.c chord_pcs) */
static const int8_t cq_iv[GCQ_COUNT][4] = {
    { 0, 4, 7, -1 }, { 0, 3, 7, -1 }, { 0, 3, 6, -1 }, { 0, 4, 8, -1 }, { 0, 5, 7, -1 },
    { 0, 7, 12, -1 }, { 0, 4, 7, 10 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 3, 6, 10 },
    { 0, 3, 6, 9 }, { 0, 4, 7, 14 } };
static int in_chord(int note, int key, MgChord c)
{
    int i, pc = ((note - key - GCH_ROOT(c)) % 12 + 24) % 12;
    if (GCH_Q(c) == GCQ_POW) return pc == 0 || pc == 7 || pc == 3 || pc == 4;
    for (i = 0; i < 4 && cq_iv[GCH_Q(c)][i] >= 0; i++) if (pc == cq_iv[GCH_Q(c)][i] % 12) return 1;
    return 0;
}

/* ---- the guard's memory ------------------------------------------------------------ */
#define MAXW 4096
static uint32_t win_set[MAXW];
static int n_win;
static int win_has(uint32_t w) { int i; for (i = 0; i < n_win; i++) if (win_set[i] == w) return 1; return 0; }
static void win_add(uint32_t w) { if (n_win < MAXW && !win_has(w)) win_set[n_win++] = w; }

static int cand_windows(const Cand *c, uint32_t *out)
{
    int i, k, n = 0, wl = c->bars < 4 ? c->bars : 4;
    for (i = 0; i + wl <= c->bars; i++) {
        uint32_t h = 2166136261u;
        for (k = 0; k < wl; k++) h = fnv(h, bar_m2(&c->bar[i + k]));
        out[n++] = h;
    }
    return n;
}

/* ---- running the composer --------------------------------------------------------- */
typedef int (*CandFn)(void *ctx, const Cand *c);

/* play `th` for `minutes` at (intensity, dial) from `seed`; hand every
 * complete section of type `sec` (or every section if sec < 0) to fn */
static void play_sections(const MgTheme *th, uint32_t seed, int minutes, int inten, int dial, int sec,
                          CandFn fn, void *ctx)
{
    static MgPiece p;
    static Cand c;
    static MgBar tmp;
    int bars_per_min = th->steps == 16 ? th->bpm / 4 : th->bpm / 3, n = bars_per_min * minutes, i;
    int cap = -1;
    mg_start(&p, th, seed);
    for (i = 0; i < n; i++) {
        mg_set(&p, inten, dial);
        mg_next_bar(&p, &tmp);
        if (p.bar_in_sec == 1)                      /* a section just started */
            cap = ((sec < 0 || p.cur.type == sec) && p.cur.bars <= MAXB) ? 0 : -1;
        if (cap < 0) continue;
        memcpy(&c.bar[cap++], &tmp, sizeof(MgBar));
        if (cap == p.cur.bars) {
            int k;
            uint32_t h = 2166136261u;
            c.bars = p.cur.bars;
            c.steps = th->steps;
            c.key = p.cur.key; c.mode = p.cur.mode; c.var = p.cur.var; c.energy = p.cur.energy;
            c.type = p.cur.type; c.seed = seed; c.section = p.sec_count - 1;
            /* the family key: the chart's opening chord (its harmonic
             * function; each variant still carries its own chart) */
            for (k = 0; k < c.bars; k++) c.chord[k] = p.cur.chord[k];
            h = fnv(h, (uint32_t)(GCH_ROOT(c.chord[0]) | GCH_Q(c.chord[0]) << 4));
            c.chart = h ^ (uint32_t)c.bars;
            if (fn(ctx, &c)) return;
            cap = -1;
        }
    }
}

/* the authored material the guard protects: the theme's first statement (V0 A) */
static int take_anchor(void *ctx, const Cand *c)
{
    uint32_t w[MAXB];
    int i, n = cand_windows(c, w);
    (void)ctx;
    for (i = 0; i < n; i++) win_add(w[i]);
    return 1;
}

typedef struct Accepted { Cand c; int fam; } Accepted;
typedef struct SlotRun {
    const Slot *slot;
    Accepted *acc;
    int n_acc, n_fam, home_key;
    int seen, rej_thin, rej_key, rej_ct, rej_dup, rej_guard, rej_near;
} SlotRun;

static int strong_ok(const Cand *c)
{
    int i, k, n = 0, bad = 0;
    for (i = 0; i < c->bars; i++)
        for (k = 0; k < c->bar[i].n; k++) {
            const MgEv *e = &c->bar[i].ev[k];
            if (e->role != GR_LEAD) continue;
            if (e->at == 0 || (c->steps == 16 && e->at == 96)) {
                n++;
                if (!in_chord(e->note, c->key, c->chord[i])) bad++;
            }
        }
    return n > 0 && (n < 20 ? bad == 0 : bad * 20 <= n);
}

static int lead_every_bar(const Cand *c)
{
    int i, k;
    for (i = 0; i < c->bars; i++) {
        int any = 0;
        for (k = 0; k < c->bar[i].n; k++) if (c->bar[i].ev[k].role == GR_LEAD) any = 1;
        if (!any) return 0;
    }
    return 1;
}

/* phase 1: the candidate pool (every section that passes the musical gates,
 * literal duplicates dropped) */
#define MAXPOOL 600
static Cand pool[MAXPOOL];
static int n_pool;

static int cand_same(const Cand *a, const Cand *b)
{
    uint32_t wa[MAXB], wb[MAXB];
    int i, n = cand_windows(a, wa);
    if (a->bars != b->bars || n != cand_windows(b, wb)) return 0;
    for (i = 0; i < n; i++) if (wa[i] != wb[i]) return 0;
    return 1;
}

static int take_candidate(void *ctx, const Cand *c)
{
    SlotRun *r = (SlotRun *)ctx;
    int i;
    r->seen++;
    if (c->energy < 3 || !lead_every_bar(c)) { r->rej_thin++; return 0; }
    if (c->key != r->home_key) { r->rej_key++; return 0; }
    if (!strong_ok(c)) { r->rej_ct++; return 0; }
    for (i = 0; i < n_pool; i++) if (cand_same(&pool[i], c)) { r->rej_dup++; return 0; }
    if (n_pool < MAXPOOL) memcpy(&pool[n_pool++], c, sizeof(*c));
    return n_pool >= MAXPOOL;
}

/* near-copies: against a kept variant of the slot, an 8-bar slot needs a
 * mean F < 0.80 and >= 3 bars that really differ (F < 0.70); a short slot
 * (BUILD 4, TAG 2) a mean F < 0.85 and >= 1 such bar */
static int near_copy(const Cand *a, const Cand *c)
{
    int i, sum = 0, differ = 0, nb = a->bars < c->bars ? a->bars : c->bars, need = nb >= 8 ? 3 : 1;
    int lim = nb >= 8 ? 800 : 850;
    for (i = 0; i < nb; i++) {
        int sm = bar_sim(&a->bar[i], &c->bar[i]);
        sum += sm;
        differ += sm < 700;
    }
    return nb && (sum / nb >= lim || differ < need);
}

static int guard_ok(SlotRun *r, const Cand *c, int count)
{
    uint32_t w[MAXB];
    int i, n = cand_windows(c, w);
    for (i = 0; i < n; i++) if (win_has(w[i])) { if (count) r->rej_guard++; return 0; }
    for (i = 0; i < r->n_acc; i++) if (near_copy(&r->acc[i].c, c)) { if (count) r->rej_near++; return 0; }
    return 1;
}

static void accept(SlotRun *r, const Cand *c, int fam)
{
    uint32_t w[MAXB];
    int i, n = cand_windows(c, w);
    memcpy(&r->acc[r->n_acc].c, c, sizeof(*c));
    r->acc[r->n_acc].fam = fam;
    r->n_acc++;
    for (i = 0; i < n; i++) win_add(w[i]);
}

/* phase 2: families are chart groups. Take, in pool order, the n_fam groups
 * that can each give the most guard-clean, mutually distinct members; fill
 * each to n_var / n_fam, then top up from the chosen groups. */
static void select_variants(SlotRun *r)
{
    const Slot *s = r->slot;
    uint32_t keys[MAXPOOL];
    int score[MAXPOOL], n_keys = 0, i, j, f, per = s->n_var / s->n_fam;
    int chosen[4], n_ch = 0;
    for (i = 0; i < n_pool; i++) {
        for (j = 0; j < n_keys && keys[j] != pool[i].chart; j++) {}
        if (j == n_keys) keys[n_keys++] = pool[i].chart;
    }
    /* a group's score: how many members a greedy pass would keep on their own */
    for (j = 0; j < n_keys; j++) {
        int save_acc = r->n_acc, save_win = n_win;
        score[j] = 0;
        for (i = 0; i < n_pool && score[j] < per; i++)
            if (pool[i].chart == keys[j] && guard_ok(r, &pool[i], 0)) { accept(r, &pool[i], 0); score[j]++; }
        r->n_acc = save_acc;
        n_win = save_win;
    }
    while (n_ch < s->n_fam) {
        int best = -1;
        for (j = 0; j < n_keys; j++) {
            int used = 0;
            for (f = 0; f < n_ch; f++) used |= chosen[f] == j;
            if (!used && score[j] > 0 && (best < 0 || score[j] > score[best])) best = j;
        }
        if (best < 0) break;
        chosen[n_ch++] = best;
    }
    r->n_fam = n_ch;
    for (f = 0; f < n_ch; f++) {
        int got = 0;
        for (i = 0; i < n_pool && got < per; i++)
            if (pool[i].chart == keys[chosen[f]] && guard_ok(r, &pool[i], 1)) { accept(r, &pool[i], f); got++; }
    }
    for (f = 0; f < n_ch && r->n_acc < s->n_var; f++)
        for (i = 0; i < n_pool && r->n_acc < s->n_var; i++)
            if (pool[i].chart == keys[chosen[f]] && guard_ok(r, &pool[i], 1)) accept(r, &pool[i], f);
    /* keep the families together in the output */
    for (i = 1; i < r->n_acc; i++) {
        static Accepted tmpa;
        j = i;
        memcpy(&tmpa, &r->acc[i], sizeof(tmpa));
        while (j > 0 && r->acc[j - 1].fam > tmpa.fam) { memcpy(&r->acc[j], &r->acc[j - 1], sizeof(tmpa)); j--; }
        memcpy(&r->acc[j], &tmpa, sizeof(tmpa));
    }
}

static uint32_t cue_seed(const CueCfg *c, int k)
{
    uint32_t h = 2166136261u;
    const char *p;
    for (p = c->id; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
    return h ^ ((uint32_t)k * 0x9E3779B9u);
}

#define MAX_SEEDS 48
static Accepted accepted[3][8];
static SlotRun runs[3];

static void realise(const CueCfg *cc, const MgTheme *th)
{
    int si, k;
    n_win = 0;
    play_sections(th, cue_seed(cc, 0), 10, cc->intensity, cc->dial, GSEC_A, take_anchor, NULL);
    for (si = 0; si < cc->n_slot; si++) {
        SlotRun *r = &runs[si];
        memset(r, 0, sizeof(*r));
        r->slot = &cc->slot[si];
        r->acc = accepted[si];
        r->home_key = th->key;
        n_pool = 0;
        for (k = 1; k <= MAX_SEEDS && n_pool < MAXPOOL; k++)
            play_sections(th, cue_seed(cc, k * 16 + si), 20, cc->intensity, cc->dial, r->slot->sec, take_candidate, r);
        select_variants(r);
    }
}

/* ---- .dms output ------------------------------------------------------------------------ */
static const char *nn[12] = { "c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b" };
static const char *kn[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
static const char *qn[GCQ_COUNT] = { "", "m", "dim", "aug", "sus4", "5", "7", "maj7", "m7", "m7b5", "dim7", "add9" };
static const char *mode_name[GMODE_COUNT] = { "major", "minor", "harmonic", "dorian", "phrygian", "mixolydian" };

/* an MML length for `u` units (192 per whole): a tie chain of plain or dotted lengths */
static void mml_len(char *out, int u)
{
    static const int lens[] = { 288, 192, 144, 96, 72, 48, 36, 24, 18, 12, 6, 3 };
    static const char *txt[] = { "1.", "1", "2.", "2", "4.", "4", "8.", "8", "16.", "16", "32", "64" };
    int first = 1;
    *out = 0;
    while (u > 0) {
        int i;
        for (i = 0; i < 12 && lens[i] > u; i++) {}
        if (i == 12) i = 11;
        if (!first) strcat(out, "^");
        strcat(out, txt[i]);
        u -= lens[i];
        first = 0;
    }
}

/* the slot (in units) a gated lead note came from: the composer emits
 * max(6, slot * gate / 8) units for a slot of whole 16ths */
static int ungate(int dur, int gate)
{
    int d;
    if (gate >= 8) return dur;
    for (d = 1; d <= 64; d++) {
        int gu = (d * 12 * gate) >> 3;
        if ((gu < 6 ? 6 : gu) >= dur) return d * 12;
    }
    return dur;
}

/* one role of a variant as an MML lane: monophonic, rests in the gaps; a
 * note's length is its slot (cut at the next onset), the gate is the lane's q */
static void emit_lane(FILE *f, const Cand *c, int role, int gate, int oct_off)
{
    int i, k, oct = -1, unit = c->steps * 12;
    fprintf(f, "q%d", gate);
    for (i = 0; i < c->bars; i++) {
        int t = 0, n = 0, idx[MG_BAR_EV];
        const MgBar *b = &c->bar[i];
        for (k = 0; k < b->n; k++) if (b->ev[k].role == role) idx[n++] = k;
        for (k = 1; k < n; k++) {                         /* by onset (stable) */
            int j = k, v = idx[k];
            while (j > 0 && b->ev[idx[j - 1]].at > b->ev[v].at) { idx[j] = idx[j - 1]; j--; }
            idx[j] = v;
        }
        fprintf(f, " |");
        for (k = 0; k < n; k++) {
            const MgEv *e = &b->ev[idx[k]];
            int at = e->at, nxt = k + 1 < n ? b->ev[idx[k + 1]].at : unit, note = e->note + 12 * oct_off, o, end;
            char len[64];
            if (at < t) continue;                         /* a duplicate onset: the first wins */
            if (at > t) { mml_len(len, at - t); fprintf(f, " r%s", len); t = at; }
            end = at + ungate(e->dur, gate);
            if (end > nxt) end = nxt;
            if (end <= at) end = at + 3;
            o = note / 12;
            if (o != oct) { fprintf(f, " o%d", o); oct = o; }
            mml_len(len, end - at);
            fprintf(f, " %s%s", nn[note % 12], len);
            t = end;
        }
        if (t < unit) { char len[64]; mml_len(len, unit - t); fprintf(f, " r%s", len); }
    }
    fprintf(f, " |\n");
}

static void emit_chords(FILE *f, const Cand *c)
{
    int i;
    fprintf(f, "    chords  |");
    for (i = 0; i < c->bars; i++) {
        MgChord ch = c->chord[i];
        fprintf(f, " %s%s .%s", kn[(c->key + GCH_ROOT(ch)) % 12], qn[GCH_Q(ch)], c->steps == 12 ? "" : "");
        fprintf(f, " |");
    }
    fprintf(f, "\n");
}

static int role_used(const Cand *c, int role)
{
    int i, k;
    for (i = 0; i < c->bars; i++) for (k = 0; k < c->bar[i].n; k++) if (c->bar[i].ev[k].role == role) return 1;
    return 0;
}

static int cmd_gen(const CueCfg *cc)
{
    MgTheme th = theme_of(cc);
    int si, v;
    realise(cc, &th);
    printf("# core/music/gen/%s/%s.dms - musgen output for %s (D2, sec.8.3), curated source.\n", cc->area, cc->id, cc->mc);
    printf("# Written by `python3 tools/mmlc.py --gen %s` (tools/musgen: composer fingerprint\n", cc->mc);
    printf("# 37d71b12 8d898ef0 c33cfa26 bdcb0bd2). CI never re-runs musgen: edit by ear, delete\n");
    printf("# noodling, and mmlc lints what is left (lane sums, the guard, STRONG_CT). The\n");
    printf("# owning cue (%s) references the slots below with `gen <SLOT>`.\n", cc->mc);
    printf("gencue %s theme=%s key=%s:%s bpm=%d meter=%d dial=S%d i=I%d\n", cc->mc, cc->motif,
           kn[th.key], mode_name[th.mode], th.bpm, th.steps == 12 ? 3 : 4, cc->dial, cc->intensity);
    for (si = 0; si < cc->n_slot; si++) {
        const SlotRun *r = &runs[si];
        const Slot *s = r->slot;
        int bars = r->n_acc ? r->acc[0].c.bars : 0;
        printf("\n# %s: %d sections heard, %d kept in %d families; rejected: thin %d, key %d, strong-beat %d,\n"
               "#   duplicate %d, guard %d, near-copy %d (the rest: other chart families)\n",
               s->name, r->seen, r->n_acc, r->n_fam, r->rej_thin, r->rej_key, r->rej_ct, r->rej_dup, r->rej_guard, r->rej_near);
        printf("gen %s tmpl=%s motif=%s cad=%s n=%d fam=%d roles=LEAD,COUNTER bars=%d energy=%d var_cap=%d\n",
               s->name, tmpl_name[s->tmpl], cc->motif, cad_name[s->cad], r->n_acc, r->n_fam, bars, cc->intensity, th.var_max);
        for (v = 0; v < r->n_acc; v++) {
            const Cand *c = &r->acc[v].c;
            int gate = mg_styles[cc->dial].lead_gate;
            printf("  variant %d fam=%d from=seed:0x%08x,section:%d,var:%d,energy:%d\n", v, r->acc[v].fam,
                   (unsigned)c->seed, c->section, c->var, c->energy);
            emit_chords(stdout, c);
            printf("    LEAD    ");
            emit_lane(stdout, c, GR_LEAD, gate, cc->oct);
            if (role_used(c, GR_COUNTER)) { printf("    COUNTER "); emit_lane(stdout, c, GR_COUNTER, 8, cc->oct); }
        }
    }
    return 0;
}

/* ---- the reference chip renderer (the prototype's render_wav.c) ---------------------------- */
#define SR 22050
typedef struct { int32_t start, len, rel; uint32_t ph, inc, inc2; int kind, vol, role, note, q; uint32_t lfsr; int drum; } Voice;
#define MAXV 256
static Voice vv[MAXV];
static int nv;

static uint32_t note_inc(int note)
{
    static const uint32_t ratio[12] = { 65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193, 104032, 110218, 116772, 123715 };
    uint64_t hz_q16 = ((uint64_t)535809u * ratio[note % 12]) >> 16;
    hz_q16 <<= (note / 12);
    return (uint32_t)((hz_q16 << 16) / SR);
}

static void add_ev(const MgEv *e, int32_t t0, int32_t spu, int dial)
{
    Voice *x;
    if (nv >= MAXV) return;
    x = &vv[nv++];
    memset(x, 0, sizeof(*x));
    x->start = t0 + (int32_t)(e->at * spu >> 8);
    x->len = (int32_t)(e->dur * spu >> 8);
    x->rel = SR / 40;
    x->role = e->role; x->note = e->note; x->lfsr = 0x7FFF;
    x->vol = e->vel;
    if (e->role == GR_DRUMS) {
        x->drum = e->note;
        x->len = e->note == GDR_CRASH ? SR / 2 : e->note == GDR_OPEN ? SR / 6 : e->note == GDR_KICK ? SR / 9 : SR / 14;
        x->kind = 100;
        return;
    }
    x->q = e->inst & 15;
    x->kind = e->inst >> 4 == 2 ? 20 : e->inst >> 4 == 3 ? 30 : e->inst;
    if (dial >= 3 && e->role == GR_LEAD) x->rel = SR / 10;
    x->inc = note_inc(e->note);
}

static int16_t render_one(Voice *x, int32_t t)
{
    int32_t k = t - x->start, env, s = 0;
    if (k < 0) return 0;
    if (k >= x->len + x->rel) { x->kind = -1; return 0; }
    env = k < 44 ? k * 256 / 44 : 256 - (k > 2000 ? (k - 2000) / 200 : 0);
    if (env < 140) env = 140;
    if (k >= x->len) env = env * (x->len + x->rel - k) / x->rel;
    if (x->kind == 100) {
        int32_t dec = 256 - k * 256 / x->len;
        if (dec < 0) dec = 0;
        x->lfsr = (x->lfsr >> 1) ^ (uint32_t)(-(int32_t)(x->lfsr & 1) & 0xB400u);
        if (x->drum == GDR_KICK || x->drum == GDR_TOM) {
            uint32_t f0 = x->drum == GDR_KICK ? 160 : 260, f1 = x->drum == GDR_KICK ? 45 : 110;
            uint32_t fr = f1 + (f0 - f1) * (uint32_t)dec / 256u;
            x->ph += (fr << 16) / SR << 16;
            s = (x->ph >> 31) ? 9000 : -9000;
            if (x->drum == GDR_KICK) s += (k < 60) ? ((x->lfsr & 1) ? 4000 : -4000) : 0;
        } else {
            s = (x->lfsr & 1) ? 7000 : -7000;
            if (x->drum == GDR_HAT || x->drum == GDR_OPEN) s = s / 2;
        }
        return (int16_t)(s * dec / 256 * x->vol / 63);
    }
    if (x->kind == 20 || x->kind == 30) {
        int n = 0, i = (k / (SR / 60));
        while (n < 4 && cq_iv[x->q][n] >= 0) n++;
        x->ph += note_inc(x->note + cq_iv[x->q][i % n]);
        if (x->kind == 20) s = (x->ph >> 30) == 0 ? 6000 : -2000;
        else s = (int32_t)((x->ph >> 31) ? (x->ph >> 16) : (0xFFFF - (x->ph >> 16))) / 6 - 5400;
    } else {
        uint32_t inc = x->inc;
        if (x->kind == 0x10 && k > SR / 5) {
            int ph = (k / 64) & 63, tri = ph < 32 ? ph - 16 : 48 - ph;
            inc = (uint32_t)((int64_t)inc * (8192 + tri * 3) / 8192);
        }
        x->ph += inc;
        switch (x->kind) {
        case 0x11: s = (x->ph >> 29) == 0 ? 6500 : -1000; break;
        case 0x12: s = (x->ph >> 31) ? 5000 : -5000; break;
        case 0x13: s = (int32_t)((x->ph >> 31) ? (x->ph >> 16) : (0xFFFF - (x->ph >> 16))) / 5 - 6500; break;
        case 0x14: s = (int32_t)(x->ph >> 16) / 6 - 5400; break;
        case 0x15: x->inc2 += inc + inc / 2; if (x->ph < inc) x->inc2 = 0;
                   s = (int32_t)(x->inc2 >> 16) / 7 - 4600; break;
        case 0x16: { uint32_t duty = 0x30000000u + (uint32_t)((k / 32) & 255) * 0x00600000u;
                   s = x->ph < duty ? 5000 : -5000; break; }
        default:   s = (x->ph >> 30) == 0 ? 6000 : -2000; break;
        }
    }
    return (int16_t)(s * env / 256 * x->vol / 63);
}

static void le32(FILE *f, uint32_t x) { fputc((int)(x & 255), f); fputc((int)((x >> 8) & 255), f); fputc((int)((x >> 16) & 255), f); fputc((int)(x >> 24), f); }

typedef struct Wav { FILE *f; int32_t t, bar_start, spu; uint32_t samples; int32_t peak; uint64_t sumsq; } Wav;

static int wav_open(Wav *w, const char *path, int bpm)
{
    memset(w, 0, sizeof(*w));
    w->f = fopen(path, "wb");
    if (!w->f) return 0;
    fwrite("RIFF", 1, 4, w->f); le32(w->f, 36); fwrite("WAVEfmt ", 1, 8, w->f);
    le32(w->f, 16); fputc(1, w->f); fputc(0, w->f); fputc(1, w->f); fputc(0, w->f);
    le32(w->f, SR); le32(w->f, SR * 2); fputc(2, w->f); fputc(0, w->f); fputc(16, w->f); fputc(0, w->f);
    fwrite("data", 1, 4, w->f); le32(w->f, 0);
    w->spu = (int32_t)((int64_t)SR * 60 * 256 / (bpm * 48));
    nv = 0;
    return 1;
}

static void wav_bar(Wav *w, const MgBar *b, int dial)
{
    static const int role_gain[GR_COUNT] = { 256, 150, 150, 230, 200, 120, 110, 120 };
    int i, j;
    int32_t bar_len = (int32_t)(b->len * w->spu >> 8);
    for (i = 0; i < b->n; i++) add_ev(&b->ev[i], w->bar_start, w->spu, dial);
    for (i = 0; i < bar_len; i++, w->t++) {
        int32_t mixv = 0;
        for (j = 0; j < nv; j++) if (vv[j].kind >= 0) mixv += render_one(&vv[j], w->t) * role_gain[vv[j].role] / 256;
        mixv = mixv * 3 / 5;
        if (mixv > 32767) mixv = 32767;
        if (mixv < -32768) mixv = -32768;
        if ((mixv < 0 ? -mixv : mixv) > w->peak) w->peak = mixv < 0 ? -mixv : mixv;
        w->sumsq += (uint64_t)((int64_t)mixv * mixv);
        fputc(mixv & 255, w->f); fputc((mixv >> 8) & 255, w->f);
        w->samples++;
    }
    for (i = 0, j = 0; i < nv; i++) if (vv[i].kind >= 0 && w->t < vv[i].start + vv[i].len + vv[i].rel) vv[j++] = vv[i];
    nv = j;
    w->bar_start += bar_len;
}

static void wav_close(Wav *w, const char *label)
{
    uint32_t bytes = w->samples * 2;
    uint64_t ms = w->samples ? w->sumsq / w->samples : 0;
    uint32_t rms = 0;
    while ((uint64_t)(rms + 1) * (rms + 1) <= ms) rms++;
    fseek(w->f, 4, SEEK_SET); le32(w->f, 36 + bytes);
    fseek(w->f, 40, SEEK_SET); le32(w->f, bytes);
    fclose(w->f);
    printf("%s: %u samples (%u s), peak %d, rms %u\n", label, (unsigned)w->samples, (unsigned)(w->samples / SR), (int)w->peak, (unsigned)rms);
}

static int cmd_wav(const CueCfg *cc, int secs, const char *out, const char *script)
{
    MgTheme th = theme_of(cc);
    static MgPiece p;
    MgBar b;
    Wav w;
    uint32_t bar = 0;
    int sc = !strcmp(script, "steady") ? 0 : !strcmp(script, "walk") ? 1 : 2;
    if (!wav_open(&w, out, th.bpm)) return 1;
    mg_start(&p, &th, cue_seed(cc, 0));
    while (w.samples < (uint32_t)secs * SR) {
        if (sc == 0) mg_set(&p, cc->intensity, cc->dial);
        else if (sc == 1) mg_set(&p, 1 + (int)((bar >> 3) % 5), cc->dial);
        else { static const int8_t d[4] = { -1, 2, 4, 0 }; int dd = d[(bar >> 4) & 3]; mg_set(&p, (bar >> 4) & 1 ? 4 : 3, dd < 0 ? cc->dial : dd); }
        mg_next_bar(&p, &b);
        wav_bar(&w, &b, p.cur.dial);
        bar++;
    }
    wav_close(&w, out);
    return 0;
}

/* the reel: A (V0) between the kept variants, each played whole */
static Cand anchor_c;
static int keep_anchor(void *ctx, const Cand *c) { (void)ctx; memcpy(&anchor_c, c, sizeof(*c)); return 1; }

static int cmd_reel(const CueCfg *cc, const char *out)
{
    MgTheme th = theme_of(cc);
    Wav w;
    int si, v, i;
    realise(cc, &th);
    play_sections(&th, cue_seed(cc, 0), 10, cc->intensity, cc->dial, GSEC_A, keep_anchor, NULL);
    if (!wav_open(&w, out, th.bpm)) return 1;
    for (si = 0; si < cc->n_slot; si++)
        for (v = 0; v < runs[si].n_acc; v++) {
            for (i = 0; i < anchor_c.bars; i++) wav_bar(&w, &anchor_c.bar[i], cc->dial);
            for (i = 0; i < runs[si].acc[v].c.bars; i++) wav_bar(&w, &runs[si].acc[v].c.bar[i], cc->dial);
        }
    wav_close(&w, out);
    return 0;
}

int main(int argc, char **argv)
{
    const CueCfg *cc;
    if (argc >= 2 && !strcmp(argv[1], "fingerprint")) return cmd_fingerprint(argc >= 3 && !strcmp(argv[2], "--check"));
    if (argc >= 2 && !strcmp(argv[1], "list")) {
        int i;
        for (i = 0; i < N_CUES; i++) printf("%-9s %-12s %s\n", cues[i].id, cues[i].mc, cues[i].area);
        return 0;
    }
    if (argc >= 3 && (cc = find_cue(argv[2])) != NULL) {
        if (!strcmp(argv[1], "gen")) return cmd_gen(cc);
        if (!strcmp(argv[1], "wav") && argc >= 5) return cmd_wav(cc, atoi(argv[3]), argv[4], argc >= 6 ? argv[5] : "morph");
        if (!strcmp(argv[1], "reel") && argc >= 4) return cmd_reel(cc, argv[3]);
    }
    fprintf(stderr, "usage: musgen fingerprint [--check] | list | gen CUE | wav CUE SECONDS OUT.wav [steady|walk|morph] | reel CUE OUT.wav\n");
    return 2;
}
